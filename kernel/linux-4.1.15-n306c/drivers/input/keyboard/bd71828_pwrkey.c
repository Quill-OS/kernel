/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 * Copyright 2010, 2011 David Jander <david@protonic.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <linux/mfd/rohm-bd71815.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/mfd/rohm-generic.h>

#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"
extern volatile NTX_HWCONFIG *gptHWCFG;

extern int gSleep_Mode_Suspend;


struct pwrkey_button_data {
	const struct gpio_keys_button *button;
	struct input_dev *input;

	spinlock_t lock;
	bool disabled;
	
	unsigned int irq_push;
	unsigned int irq_short_push;
	unsigned int irq_mid_push;
	unsigned int irq_long_push;
};

struct bd71828_pwrkey_drvdata {
	const struct bd71828_pwrkey_platform_data *pdata;
	struct input_dev *input;
	struct mutex disable_lock;
	struct pwrkey_button_data data[0];
};

/*
 * SYSFS interface for enabling/disabling keys and switches:
 *
 * There are 4 attributes under /sys/devices/platform/gpio-keys/
 *	keys [ro]              - bitmap of keys (EV_KEY) which can be
 *	                         disabled
 *	switches [ro]          - bitmap of switches (EV_SW) which can be
 *	                         disabled
 *	disabled_keys [rw]     - bitmap of keys currently disabled
 *	disabled_switches [rw] - bitmap of switches currently disabled
 *
 * Userland can change these values and hence disable event generation
 * for each key (or switch). Disabling a key means its interrupt line
 * is disabled.
 *
 * For example, if we have following switches set up as gpio-keys:
 *	SW_DOCK = 5
 *	SW_CAMERA_LENS_COVER = 9
 *	SW_KEYPAD_SLIDE = 10
 *	SW_FRONT_PROXIMITY = 11
 * This is read from switches:
 *	11-9,5
 * Next we want to disable proximity (11) and dock (5), we write:
 *	11,5
 * to file disabled_switches. Now proximity and dock IRQs are disabled.
 * This can be verified by reading the file disabled_switches:
 *	11,5
 * If we now want to enable proximity (11) switch we write:
 *	5
 * to disabled_switches.
 *
 * We can disable only those keys which don't allow sharing the irq.
 */

/**
 * get_n_events_by_type() - returns maximum number of events per @type
 * @type: type of button (%EV_KEY, %EV_SW)
 *
 * Return value of this function can be used to allocate bitmap
 * large enough to hold all bits for given type.
 */
static inline int get_n_events_by_type(int type)
{
	BUG_ON(type != EV_SW && type != EV_KEY);

	return (type == EV_KEY) ? KEY_CNT : SW_CNT;
}

/**
 * bd71828_pwrkey_disable_button() - disables given GPIO button
 * @bdata: button data for button to be disabled
 *
 * Disables button pointed by @bdata. This is done by masking
 * IRQ line. After this function is called, button won't generate
 * input events anymore. Note that one can only disable buttons
 * that don't share IRQs.
 *
 * Make sure that @bdata->disable_lock is locked when entering
 * this function to avoid races when concurrent threads are
 * disabling buttons at the same time.
 */
static void bd71828_pwrkey_disable_button(struct pwrkey_button_data *bdata)
{
	if (!bdata->disabled) {
		/*
		 * Disable IRQ and associated timer/work structure.
		 */
		disable_irq(bdata->irq_push);
		disable_irq(bdata->irq_short_push);
		disable_irq(bdata->irq_mid_push);
		disable_irq(bdata->irq_long_push);

		bdata->disabled = true;
	}
}

/**
 * bd71828_pwrkey_enable_button() - enables given GPIO button
 * @bdata: button data for button to be disabled
 *
 * Enables given button pointed by @bdata.
 *
 * Make sure that @bdata->disable_lock is locked when entering
 * this function to avoid races with concurrent threads trying
 * to enable the same button at the same time.
 */
static void bd71828_pwrkey_enable_button(struct pwrkey_button_data *bdata)
{
	if (bdata->disabled) {
		enable_irq(bdata->irq_push);
		enable_irq(bdata->irq_short_push);
		enable_irq(bdata->irq_mid_push);
		enable_irq(bdata->irq_long_push);

		bdata->disabled = false;
	}
}

/**
 * bd71828_pwrkey_attr_show_helper() - fill in stringified bitmap of buttons
 * @ddata: pointer to drvdata
 * @buf: buffer where stringified bitmap is written
 * @type: button type (%EV_KEY, %EV_SW)
 * @only_disabled: does caller want only those buttons that are
 *                 currently disabled or all buttons that can be
 *                 disabled
 *
 * This function writes buttons that can be disabled to @buf. If
 * @only_disabled is true, then @buf contains only those buttons
 * that are currently disabled. Returns 0 on success or negative
 * errno on failure.
 */
static ssize_t bd71828_pwrkey_attr_show_helper(struct bd71828_pwrkey_drvdata *ddata,
					  char *buf, unsigned int type,
					  bool only_disabled)
{
	int n_events = get_n_events_by_type(type);
	unsigned long *bits;
	ssize_t ret;
	int i;

	bits = kcalloc(BITS_TO_LONGS(n_events), sizeof(*bits), GFP_KERNEL);
	if (!bits)
		return -ENOMEM;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pwrkey_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (only_disabled && !bdata->disabled)
			continue;

		__set_bit(bdata->button->code, bits);
	}

	ret = scnprintf(buf, PAGE_SIZE - 1, "%*pbl", n_events, bits);
	buf[ret++] = '\n';
	buf[ret] = '\0';

	kfree(bits);

	return ret;
}

/**
 * bd71828_pwrkey_attr_store_helper() - enable/disable buttons based on given bitmap
 * @ddata: pointer to drvdata
 * @buf: buffer from userspace that contains stringified bitmap
 * @type: button type (%EV_KEY, %EV_SW)
 *
 * This function parses stringified bitmap from @buf and disables/enables
 * GPIO buttons accordingly. Returns 0 on success and negative error
 * on failure.
 */
static ssize_t bd71828_pwrkey_attr_store_helper(struct bd71828_pwrkey_drvdata *ddata,
					   const char *buf, unsigned int type)
{
	int n_events = get_n_events_by_type(type);
	unsigned long *bits;
	ssize_t error;
	int i;

	bits = kcalloc(BITS_TO_LONGS(n_events), sizeof(*bits), GFP_KERNEL);
	if (!bits)
		return -ENOMEM;

	error = bitmap_parselist(buf, bits, n_events);
	if (error)
		goto out;

	/* First validate */
	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pwrkey_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (test_bit(bdata->button->code, bits) &&
		    !bdata->button->can_disable) {
			error = -EINVAL;
			goto out;
		}
	}

	mutex_lock(&ddata->disable_lock);

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pwrkey_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (test_bit(bdata->button->code, bits))
			bd71828_pwrkey_disable_button(bdata);
		else
			bd71828_pwrkey_enable_button(bdata);
	}

	mutex_unlock(&ddata->disable_lock);

out:
	kfree(bits);
	return error;
}

#define ATTR_SHOW_FN(name, type, only_disabled)				\
static ssize_t bd71828_pwrkey_show_##name(struct device *dev,		\
				     struct device_attribute *attr,	\
				     char *buf)				\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct bd71828_pwrkey_drvdata *ddata = platform_get_drvdata(pdev);	\
									\
	return bd71828_pwrkey_attr_show_helper(ddata, buf,			\
					  type, only_disabled);		\
}

ATTR_SHOW_FN(keys, EV_KEY, false);
ATTR_SHOW_FN(switches, EV_SW, false);
ATTR_SHOW_FN(disabled_keys, EV_KEY, true);
ATTR_SHOW_FN(disabled_switches, EV_SW, true);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/gpio-keys/keys [ro]
 * /sys/devices/platform/gpio-keys/switches [ro]
 */
static DEVICE_ATTR(keys, S_IRUGO, bd71828_pwrkey_show_keys, NULL);
static DEVICE_ATTR(switches, S_IRUGO, bd71828_pwrkey_show_switches, NULL);

#define ATTR_STORE_FN(name, type)					\
static ssize_t bd71828_pwrkey_store_##name(struct device *dev,		\
				      struct device_attribute *attr,	\
				      const char *buf,			\
				      size_t count)			\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct bd71828_pwrkey_drvdata *ddata = platform_get_drvdata(pdev);	\
	ssize_t error;							\
									\
	error = bd71828_pwrkey_attr_store_helper(ddata, buf, type);		\
	if (error)							\
		return error;						\
									\
	return count;							\
}

ATTR_STORE_FN(disabled_keys, EV_KEY);
ATTR_STORE_FN(disabled_switches, EV_SW);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/gpio-keys/disabled_keys [rw]
 * /sys/devices/platform/gpio-keys/disables_switches [rw]
 */
static DEVICE_ATTR(disabled_keys, S_IWUSR | S_IRUGO,
		   bd71828_pwrkey_show_disabled_keys,
		   bd71828_pwrkey_store_disabled_keys);
static DEVICE_ATTR(disabled_switches, S_IWUSR | S_IRUGO,
		   bd71828_pwrkey_show_disabled_switches,
		   bd71828_pwrkey_store_disabled_switches);

static struct attribute *bd71828_pwrkey_attrs[] = {
	&dev_attr_keys.attr,
	&dev_attr_switches.attr,
	&dev_attr_disabled_keys.attr,
	&dev_attr_disabled_switches.attr,
	NULL,
};

static struct attribute_group bd71828_pwrkey_attr_group = {
	.attrs = bd71828_pwrkey_attrs,
};

static irqreturn_t bd71828_pwrkey_irq_isr(int irq, void *dev_id)
{
	struct pwrkey_button_data *bdata = dev_id;
	const struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned long flags;

	BUG_ON(irq != bdata->irq_push);

	spin_lock_irqsave(&bdata->lock, flags);

	if (bdata->button->wakeup)
		pm_wakeup_event(bdata->input->dev.parent, 0);

	input_event(input, EV_KEY, button->code, 1);
	input_sync(input);

out:
	spin_unlock_irqrestore(&bdata->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t bd71828_pwrkey_up_irq_isr(int irq, void *dev_id)
{
	struct pwrkey_button_data *bdata = dev_id;
	const struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned long flags;

	spin_lock_irqsave(&bdata->lock, flags);

	input_event(input, EV_KEY, button->code, 0);
	input_sync(input);

	out:
	spin_unlock_irqrestore(&bdata->lock, flags);
	return IRQ_HANDLED;
}


static int bd71828_pwrkey_setup_key(struct platform_device *pdev,
				struct input_dev *input,
				struct pwrkey_button_data *bdata,
				const struct gpio_keys_button *button)
{
	const char *desc = button->desc ? button->desc : "bd71828_pwrkey";
	struct device *dev = &pdev->dev;
	irq_handler_t isr;
	unsigned long irqflags;
	int irq;
	int error;

	bdata->input = input;
	bdata->button = button;
	spin_lock_init(&bdata->lock);

		if (!bdata->irq_push) {
			dev_err(dev, "No IRQ specified\n");
			return -EINVAL;
		}

		if (button->type && button->type != EV_KEY) {
			dev_err(dev, "Only EV_KEY allowed for IRQ buttons.\n");
			return -EINVAL;
		}
		irqflags = 0;

	
	input_set_capability(input, button->type ?: EV_KEY, button->code);

	/*
	 * If platform has specified that the button can be disabled,
	 * we don't want it to share the interrupt line.
	 */
	if (!button->can_disable)
		irqflags |= IRQF_SHARED;

	error = devm_request_any_context_irq(&pdev->dev, bdata->irq_push,
					     bd71828_pwrkey_irq_isr, irqflags, desc, bdata);
	if (error < 0) {
		dev_err(dev, "Unable to claim irq_push %d; error %d\n",
			bdata->irq_push, error);
		return error;
	}
	
	error = devm_request_any_context_irq(&pdev->dev, bdata->irq_short_push,
					     bd71828_pwrkey_up_irq_isr, irqflags, desc, bdata);
	if (error < 0) {
		dev_err(dev, "Unable to claim irq_short_push %d; error %d\n",
			bdata->irq_short_push, error);
		return error;
	}

	error = devm_request_any_context_irq(&pdev->dev, bdata->irq_mid_push,
					     bd71828_pwrkey_up_irq_isr, irqflags, desc, bdata);
	if (error < 0) {
		dev_err(dev, "Unable to claim irq_mid_push %d; error %d\n",
			bdata->irq_mid_push, error);
		return error;
	}

	error = devm_request_any_context_irq(&pdev->dev, bdata->irq_long_push,
					     bd71828_pwrkey_up_irq_isr, irqflags, desc, bdata);
	if (error < 0) {
		dev_err(dev, "Unable to claim irq_long_push %d; error %d\n",
			bdata->irq_long_push, error);
		return error;
	}

	return 0;
}

static int bd71828_pwrkey_open(struct input_dev *input)
{
	struct bd71828_pwrkey_drvdata *ddata = input_get_drvdata(input);
	const struct bd71828_pwrkey_platform_data *pdata = ddata->pdata;
	int error;

	if (pdata->enable) {
		error = pdata->enable(input->dev.parent);
		if (error)
			return error;
	}
	
	// report up event
	input_event(input, EV_KEY, ddata->data[0].button->code, 0);
	input_sync(input);

	return 0;
}

static void bd71828_pwrkey_close(struct input_dev *input)
{
	struct bd71828_pwrkey_drvdata *ddata = input_get_drvdata(input);
	const struct bd71828_pwrkey_platform_data *pdata = ddata->pdata;

	if (pdata->disable)
		pdata->disable(input->dev.parent);
}

/*
 * Handlers for alternative sources of platform_data
 */

#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static struct bd71828_pwrkey_platform_data *
bd71828_pwrkey_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *pp;
	struct bd71828_pwrkey_platform_data *pdata;
	struct gpio_keys_button *button;
	int error;
	int nbuttons;
	int i;

	node = dev->of_node;
	if (!node)
		return ERR_PTR(-ENODEV);

	nbuttons = of_get_child_count(node);
	if (nbuttons == 0)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(dev,
			     sizeof(*pdata) + nbuttons * sizeof(*button),
			     GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->buttons = (struct gpio_keys_button *)(pdata + 1);
	pdata->nbuttons = nbuttons;

	pdata->rep = !!of_get_property(node, "autorepeat", NULL);

	i = 0;
	for_each_child_of_node(node, pp) {
		enum of_gpio_flags flags;

		button = &pdata->buttons[i++];

		button->gpio = of_get_gpio_flags(pp, 0, &flags);
		if (button->gpio < 0) {
			error = button->gpio;
			if (error != -ENOENT) {
				if (error != -EPROBE_DEFER)
					dev_err(dev,
						"Failed to get gpio flags, error: %d\n",
						error);
				return ERR_PTR(error);
			}
		} else {
			button->active_low = flags & OF_GPIO_ACTIVE_LOW;
		}

		button->irq = irq_of_parse_and_map(pp, 0);

		if (of_property_read_u32(pp, "linux,code", &button->code)) {
			dev_err(dev, "Button without keycode: 0x%x\n",
				button->gpio);
			return ERR_PTR(-EINVAL);
		}

		button->desc = of_get_property(pp, "label", NULL);

		if(0==strcmp(button->desc,"Power")) {
			extern gpio_keys_callback ntx_get_power_key_hookfn(void);
			gpio_keys_callback pfn;

			pfn = ntx_get_power_key_hookfn();
			if(pfn) {
				button->hook = pfn;
			}
		}

		if (of_property_read_u32(pp, "linux,input-type", &button->type))
			button->type = EV_KEY;

		button->can_disable = !!of_get_property(pp, "linux,can-disable", NULL);

		if (of_property_read_u32(pp, "debounce-interval",
					 &button->debounce_interval))
			button->debounce_interval = 5;
	}

	if (pdata->nbuttons == 0)
		return ERR_PTR(-EINVAL);

	return pdata;
}

static const struct of_device_id bd71828_pwrkey_of_match[] = {
	{ .compatible = "bd71828-pwrkey", },
	{ },
};
MODULE_DEVICE_TABLE(of, bd71828_pwrkey_of_match);

#else

static inline struct bd71828_pwrkey_platform_data *
bd71828_pwrkey_get_devtree_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}

#endif

static int bd71828_pwrkey_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct bd71828_pwrkey_platform_data *pdata = dev_get_platdata(dev);
	struct bd71828_pwrkey_drvdata *ddata;
	struct input_dev *input;
	size_t size;
	int i, error;
	int wakeup = 0;



	if (!pdata) {
		pdata = bd71828_pwrkey_get_devtree_pdata(dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	size = sizeof(struct bd71828_pwrkey_drvdata) +
			pdata->nbuttons * sizeof(struct pwrkey_button_data);
	ddata = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!ddata) {
		dev_err(dev, "failed to allocate state\n");
		return -ENOMEM;
	}

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(dev, "failed to allocate input device\n");
		return -ENOMEM;
	}


	ddata->pdata = pdata;
	ddata->input = input;
	mutex_init(&ddata->disable_lock);

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	input->name = pdata->name ? : pdev->name;
//	input->phys = "gpio-keys/input0";
	input->dev.parent = &pdev->dev;
	input->open = bd71828_pwrkey_open;
	input->close = bd71828_pwrkey_close;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);

	for (i = 0; i < pdata->nbuttons; i++) {
		const struct gpio_keys_button *button = &pdata->buttons[i];
		struct pwrkey_button_data *bdata = &ddata->data[i];

		bdata->irq_push = pdata->irq_push;
		bdata->irq_short_push = pdata->irq_short_push;
		bdata->irq_mid_push = pdata->irq_mid_push;
		bdata->irq_long_push = pdata->irq_long_push;
		
		error = bd71828_pwrkey_setup_key(pdev, input, bdata, button);
		if (error)
			return error;

		if (button->wakeup)
			wakeup = 1;
	}


	input_set_capability(input, EV_MSC, MSC_RAW);	// for si114x report event

	error = sysfs_create_group(&pdev->dev.kobj, &bd71828_pwrkey_attr_group);
	if (error) {
		dev_err(dev, "Unable to export keys/switches, error: %d\n",
			error);
		return error;
	}

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			error);
		goto err_remove_group;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	return 0;

err_remove_group:
	sysfs_remove_group(&pdev->dev.kobj, &bd71828_pwrkey_attr_group);
	return error;
}

static int bd71828_pwrkey_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &bd71828_pwrkey_attr_group);

	device_init_wakeup(&pdev->dev, 0);

	return 0;
}

static struct platform_driver bd71828_pwrkey_device_driver = {
	.probe		= bd71828_pwrkey_probe,
	.remove		= bd71828_pwrkey_remove,
	.driver		= {
		.name	= "bd71828-pwrkey",
		.of_match_table = of_match_ptr(bd71828_pwrkey_of_match),
	}
};

static int __init bd71828_pwrkey_init(void)
{
	return platform_driver_register(&bd71828_pwrkey_device_driver);
}

static void __exit bd71828_pwrkey_exit(void)
{
	platform_driver_unregister(&bd71828_pwrkey_device_driver);
}

late_initcall(bd71828_pwrkey_init);
module_exit(bd71828_pwrkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joseph Lai");
MODULE_DESCRIPTION("Rohm BD71828 power key");
MODULE_ALIAS("platform:bd71828-pwrkey");

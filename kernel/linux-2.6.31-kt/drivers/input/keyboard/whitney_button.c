/*
 * HOME button driver for MX50 Whitney board
 *
 * Copyright (C) 2010-2011 Amazon Technologies, Inc. All Rights Reserved.
 * Manish Lachwani (lachwani@lab126.com).
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
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/whitney_button.h>
#include <linux/workqueue.h>

#include <asm/gpio.h>

/* 1.9 seconds after IRQ press */
#define WHITNEY_BUTTON_WAKE_PRESSED	900

struct whitney_button_data {
	struct whitney_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
	struct work_struct work;
	struct delayed_work irq_work;
};

struct whitney_keys_drvdata {
	struct input_dev *input;
	struct whitney_button_data data[0];
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
};

static struct platform_device* whitney_keys_dev;

// Proc entry structures and global variables.
#define PROC_WHITNEY_BUTTON "whitney_button"
#define PROC_CMD_LEN 50
static struct proc_dir_entry *proc_entry;
static int whitney_button_lock = 0;  // 0=unlocked; non-zero=locked
static int whitney_wakeup_capable = 0;
static int whitney_wakeup_enable = 0;

static int whitney_keys_suspend(struct platform_device *pdev, pm_message_t state);
static int whitney_keys_resume(struct platform_device *pdev);

/*!
 * This function returns the current state of the whitney button
 */
int whitney_keys_proc_read( char *page, char **start, off_t off,
					int count, int *eof, void *data )
{
	int len;

	if (off > 0) {
		*eof = 1;
		return 0;
	}

	len = sprintf(page, whitney_button_lock ? "whitney button is locked\n":"whitney button is unlocked\n");

	return len;
}

/*!
 * This function executes keypad control commands based on the input string.
 *    unlock = unlock the keypad
 *    lock   = lock the keypad
 */
ssize_t whitney_keys_proc_write( struct file *filp, const char __user *buff,
					unsigned long len, void *data )

{
	char command[PROC_CMD_LEN];

	if (len > PROC_CMD_LEN) {
		dev_err(&whitney_keys_dev->dev, "keypad_proc_write:proc_len::keypad command is too long!\n");
		return -ENOSPC;
	}

	if (copy_from_user(command, buff, len)) {
		dev_err(&whitney_keys_dev->dev,"keypad_proc_write:copy2user::cannot copy from user!\n");
		return -EFAULT;
	}

	if ( !strncmp(command, "unlock", 6) ) {
		// Requested to unlock button
		if (whitney_button_lock) {
			// Unlock button
			whitney_keys_resume(whitney_keys_dev);
			whitney_button_lock = 0;
		}
	} else if ( !strncmp(command, "lock", 4) ) {
		// Requested to lock button
		if (!whitney_button_lock) {
			// Lock button
			struct pm_message pm_msg;
			whitney_keys_suspend(whitney_keys_dev,pm_msg);
			whitney_button_lock = 1;
		}
	} else {
		dev_err(&whitney_keys_dev->dev,"whitney_button_proc_write:proc_cmd:command=%s:Unrecognized whitney button command\n", command);
	}
	return len;
}


static ssize_t
show_whitney_wakeup_enable(struct device *_dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", whitney_wakeup_enable);
}

static ssize_t
store_whitney_wakeup_enable(struct device *_dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value;
	
	if ((sscanf(buf, "%d", &value) > 0) &&
		((value == 0) || (value == 1))) {
			whitney_wakeup_enable = value;
			return strlen(buf);
	}

	return -EINVAL;
}

static DEVICE_ATTR(whitney_wakeup_enable, S_IRUGO|S_IWUSR, show_whitney_wakeup_enable, store_whitney_wakeup_enable);


static void whitney_keys_report_dtcp(struct work_struct *work)
{
	struct whitney_button_data *bdata =
		container_of(work, struct whitney_button_data, irq_work.work);
	struct whitney_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned int type = button->type ?: EV_KEY;
	int state = (gpio_get_value(button->gpio) ? 1 : 0);

	if (!state) {
		input_event(input, type, button->code, !state);
		input_sync(input);
		kobject_uevent_atomic(&input->dev.kobj, KOBJ_ADD);
	}
}

static void whitney_keys_report_event(struct work_struct *work)
{
	struct whitney_button_data *bdata =
		container_of(work, struct whitney_button_data, work);
	struct whitney_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned int type = button->type ?: EV_KEY;
	int state = (gpio_get_value(button->gpio) ? 1 : 0) ^ button->active_low;

	input_event(input, type, button->code, !!state);
	input_sync(input);
	if (state == 0)
		kobject_uevent_atomic(&input->dev.kobj, KOBJ_CHANGE);
}

static void whitney_keys_timer(unsigned long _data)
{
	struct whitney_button_data *data = (struct whitney_button_data *)_data;
	struct whitney_keys_button *button = data->button;
	int state = gpio_get_value(button->gpio);

	if (state == 1)
		cancel_delayed_work(&data->irq_work);

	schedule_work(&data->work);
}

static irqreturn_t whitney_keys_isr(int irq, void *dev_id)
{
	struct whitney_button_data *bdata = dev_id;
	struct whitney_keys_button *button = bdata->button;

	BUG_ON(irq != gpio_to_irq(button->gpio));

	if (button->debounce_interval)
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(button->debounce_interval));
	else
		schedule_work(&bdata->work);

	if (whitney_wakeup_enable) {
		schedule_delayed_work(&bdata->irq_work,
			msecs_to_jiffies(WHITNEY_BUTTON_WAKE_PRESSED));
	}

	return IRQ_HANDLED;
}

extern int mx50_yoshi_set_home_button(void);
extern int gpio_whitney_button_int(void);

static int whitney_button_open(struct input_dev *input)
{
	return 0;
}

static void whitney_button_close(struct input_dev *input)
{
	struct whitney_keys_drvdata *ddata = input_get_drvdata(input);

	if (ddata->disable)
		ddata->disable(input->dev.parent);
}

static int __devinit whitney_keys_probe(struct platform_device *pdev)
{
	struct whitney_keys_platform_data *pdata = pdev->dev.platform_data;
	struct whitney_keys_drvdata *ddata;
	struct input_dev *input;
	int i, error;

	ddata = kzalloc(sizeof(struct whitney_keys_drvdata) +
			pdata->nbuttons * sizeof(struct whitney_button_data),
			GFP_KERNEL);
	input = input_allocate_device();
	whitney_keys_dev = pdev;
	if (!ddata || !input) {
		error = -ENOMEM;
		goto fail1;
	}

	platform_set_drvdata(pdev, ddata);

	input->name = pdev->name;
	input->phys = "whitney-button/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);

	ddata->input = input;
	ddata->enable = pdata->enable;
	ddata->disable = pdata->disable;

	input_set_drvdata(input, ddata);

	whitney_wakeup_capable = 0;
	whitney_wakeup_enable = 0;

	for (i = 0; i < pdata->nbuttons; i++) {
		struct whitney_keys_button *button = &pdata->buttons[i];
		struct whitney_button_data *bdata = &ddata->data[i];
		int irq;
		unsigned int type = button->type ?: EV_KEY;

		bdata->input = input;
		bdata->button = button;
		setup_timer(&bdata->timer,
			    whitney_keys_timer, (unsigned long)bdata);
		INIT_WORK(&bdata->work, whitney_keys_report_event);
		INIT_DELAYED_WORK(&bdata->irq_work, whitney_keys_report_dtcp);

		irq = gpio_whitney_button_int();
		if (irq < 0) {
			error = irq;
			pr_err("whitney-button: Unable to get irq number"
				" for GPIO %d, error %d\n",
				button->gpio, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		error = request_irq(irq, whitney_keys_isr,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
					button->desc ? button->desc : "whitney_keys",
					bdata);
		if (error) {
			pr_err("whitney-button: Unable to claim irq %d; error %d\n",
				irq, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		if (button->wakeup)
			whitney_wakeup_capable = 1;

		input_set_capability(input, type, button->code);
	}

	error = input_register_device(input);
	if (error) {
		pr_err("whitney-button: Unable to register input device, "
			"error: %d\n", error);
		goto fail2;
	}

	if (whitney_wakeup_capable) {
		device_init_wakeup(&pdev->dev, 1);
		if (device_create_file(&pdev->dev, &dev_attr_whitney_wakeup_enable) < 0)
		printk(KERN_ERR "whitney_button: Error creating whitney_wakeup_enable file\n");
	}
	
	/* Create proc entry */
	proc_entry = create_proc_entry( "whitney_button", 0644, NULL );
		if (proc_entry == NULL) {
			dev_err(&pdev->dev, "create_proc: whitney_button could not create proc entry\n");
			return -ENOMEM;
		} else {
			proc_entry->read_proc = whitney_keys_proc_read;
			proc_entry->write_proc = whitney_keys_proc_write;
	}
	/*end proc entry*/
	return 0;

 fail2:
	while (--i >= 0) {
		free_irq(gpio_to_irq(pdata->buttons[i].gpio), &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		cancel_work_sync(&ddata->data[i].work);
		gpio_free(pdata->buttons[i].gpio);
	}

	platform_set_drvdata(pdev, NULL);
 fail1:
	input_free_device(input);
	kfree(ddata);

	return error;
}

static int __devexit whitney_keys_remove(struct platform_device *pdev)
{
	struct whitney_keys_platform_data *pdata = pdev->dev.platform_data;
	struct whitney_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

	if (whitney_wakeup_capable) {
		device_init_wakeup(&pdev->dev, 0);
		device_remove_file(&pdev->dev, &dev_attr_whitney_wakeup_enable);
	}

	for (i = 0; i < pdata->nbuttons; i++) {
		int irq = gpio_to_irq(pdata->buttons[i].gpio);
		free_irq(irq, &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		cancel_work_sync(&ddata->data[i].work);
		gpio_free(pdata->buttons[i].gpio);
	}

	input_unregister_device(input);

	remove_proc_entry("whitney_button", NULL);

	return 0;
}

static int whitney_keys_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct whitney_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	for (i = 0; i < pdata->nbuttons; i++) {
		struct whitney_keys_button *button = &pdata->buttons[i];
		int irq = gpio_to_irq(button->gpio);
		if (button->wakeup && device_may_wakeup(&pdev->dev)) {
			enable_irq_wake(irq);
		} else {
			disable_irq(irq);
		}
	}

	return 0;
}

static int whitney_keys_resume(struct platform_device *pdev)
{
	struct whitney_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	for (i = 0; i < pdata->nbuttons; i++) {
		struct whitney_keys_button *button = &pdata->buttons[i];
		int irq = gpio_to_irq(button->gpio);
		if (button->wakeup && device_may_wakeup(&pdev->dev)) {
			disable_irq_wake(irq);
		} else {
			enable_irq(irq);
		}
	}
	return 0;
}


static struct platform_driver whitney_keys_device_driver = {
	.probe		= whitney_keys_probe,
	.remove		= __devexit_p(whitney_keys_remove),
	.suspend	= whitney_keys_suspend,
	.resume		= whitney_keys_resume,
	.driver		= {
		.name	= "whitney-button",
		.owner	= THIS_MODULE,
	}
};

static int __init whitney_keys_init(void)
{
	return platform_driver_register(&whitney_keys_device_driver);
}

static void __exit whitney_keys_exit(void)
{
	platform_driver_unregister(&whitney_keys_device_driver);
}

module_init(whitney_keys_init);
module_exit(whitney_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Manish Lachwani (lachwani@lab126.com)");
MODULE_DESCRIPTION("HOME Buttom Driver for whitney Boards");

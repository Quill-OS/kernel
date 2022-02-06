/*
 * Copyright 2004-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file mc13892/pmic_light.c
 * @brief This is the main file of PMIC(mc13783) Light and Backlight driver.
 *
 * @ingroup PMIC_LIGHT
 */

/*
 * Includes
 */
#define DEBUG
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/fs.h>

typedef enum {
	PMIC_SUCCESS = 0,	/*!< The requested operation was successfully
				   completed.                                     */
	PMIC_ERROR = -1,	/*!< The requested operation could not be completed
				   due to an error.                               */
	PMIC_PARAMETER_ERROR = -2,	/*!< The requested operation failed because
					   one or more of the parameters was
					   invalid.                             */
	PMIC_NOT_SUPPORTED = -3,	/*!< The requested operation could not be
					   completed because the PMIC hardware
					   does not support it. */
	PMIC_SYSTEM_ERROR_EINTR = -EINTR,

	PMIC_MALLOC_ERROR = -5,	/*!< Error in malloc function             */
	PMIC_UNSUBSCRIBE_ERROR = -6,	/*!< Error in un-subscribe event          */
	PMIC_EVENT_NOT_SUBSCRIBED = -7,	/*!< Event occur and not subscribed       */
	PMIC_EVENT_CALL_BACK = -8,	/*!< Error - bad call back                */
	PMIC_CLIENT_NBOVERFLOW = -9,	/*!< The requested operation could not be
					   completed because there are too many
					   PMIC client requests */
} PMIC_STATUS;

enum lit_channel {
	LIT_MAIN = 0,
	LIT_AUX,
	LIT_KEY,
	LIT_RED,
	LIT_GREEN,
	LIT_BLUE,
};

extern void ntx_led_blink (unsigned int channel, int period);
extern void ntx_led_dc (unsigned int channel, unsigned char dc);
extern void ntx_led_current (unsigned int channel, unsigned char value);

static int pmic_light_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
};

static int pmic_light_resume(struct platform_device *pdev)
{
	return 0;
};


PMIC_STATUS mc13892_bklit_set_current(enum lit_channel channel,
				      unsigned char level)
{
	ntx_led_current (channel, level);
	return PMIC_SUCCESS;
}

PMIC_STATUS mc13892_bklit_get_current(enum lit_channel channel,
				      unsigned char *level)
{
	*level = 0;

	return PMIC_SUCCESS;
}

PMIC_STATUS mc13892_bklit_set_dutycycle(enum lit_channel channel,
					unsigned char dc)
{
	ntx_led_dc (channel, dc);
	return PMIC_SUCCESS;
}

PMIC_STATUS mc13892_bklit_get_dutycycle(enum lit_channel channel,
					unsigned char *dc)
{
	return PMIC_SUCCESS;
}

PMIC_STATUS mc13892_bklit_set_ramp(enum lit_channel channel, int flag)
{
	return PMIC_SUCCESS;
}

PMIC_STATUS mc13892_bklit_get_ramp(enum lit_channel channel, int *flag)
{
	return PMIC_SUCCESS;
}

PMIC_STATUS mc13892_bklit_set_blink_p(enum lit_channel channel, int period)
{
	ntx_led_blink (channel, period);
	return PMIC_SUCCESS;
}

PMIC_STATUS mc13892_bklit_get_blink_p(enum lit_channel channel, int *period)
{
	return PMIC_SUCCESS;
}

EXPORT_SYMBOL(mc13892_bklit_set_current);
EXPORT_SYMBOL(mc13892_bklit_get_current);
EXPORT_SYMBOL(mc13892_bklit_set_dutycycle);
EXPORT_SYMBOL(mc13892_bklit_get_dutycycle);
EXPORT_SYMBOL(mc13892_bklit_set_ramp);
EXPORT_SYMBOL(mc13892_bklit_get_ramp);
EXPORT_SYMBOL(mc13892_bklit_set_blink_p);
EXPORT_SYMBOL(mc13892_bklit_get_blink_p);

static int pmic_light_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef DEBUG
static ssize_t lit_info(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return 0;
}

enum {
	SET_CURR = 0,
	SET_DC,
	SET_RAMP,
	SET_BP,
	SET_CH,
	LIT_CMD_MAX
};

static const char *const lit_cmd[LIT_CMD_MAX] = {
	[SET_CURR] = "cur",
	[SET_DC] = "dc",
	[SET_RAMP] = "ra",
	[SET_BP] = "bp",
	[SET_CH] = "ch"
};

static int cmd(unsigned int index, int value)
{
	static int ch = LIT_MAIN;
	int ret = 0;

	switch (index) {
	case SET_CH:
		ch = value;
		break;
	case SET_CURR:
//		printk("set %d cur %d\n", ch, value);
		ret = mc13892_bklit_set_current(ch, value);
		break;
	case SET_DC:
//		printk("set %d dc %d\n", ch, value);
		ret = mc13892_bklit_set_dutycycle(ch, value);
		break;
	case SET_RAMP:
//		printk("set %d ramp %d\n", ch, value);
		ret = mc13892_bklit_set_ramp(ch, value);
		break;
	case SET_BP:
//		printk("set %d bp %d\n", ch, value);
		ret = mc13892_bklit_set_blink_p(ch, value);
		break;
	default:
		pr_debug("error command\n");
		break;
	}

	if (ret == PMIC_SUCCESS)
		pr_debug("command exec successfully!\n");

	return 0;
}

static ssize_t lit_ctl(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int state = 0;
	const char *const *s;
	char *p, *q;
	int error;
	int len, value = 0;

	pr_debug("lit_ctl\n");

	q = NULL;
	q = memchr(buf, ' ', count);

	if (q != NULL) {
		len = q - buf;
		q += 1;
		value = simple_strtoul(q, NULL, 10);
	} else {
		p = memchr(buf, '\n', count);
		len = p ? p - buf : count;
	}

	for (s = &lit_cmd[state]; state < LIT_CMD_MAX; s++, state++) {
		if (*s && !strncmp(buf, *s, len))
			break;
	}
	if (state < LIT_CMD_MAX && *s)
		error = cmd(state, value);
	else
		error = -EINVAL;

	return count;
}

#else
static ssize_t lit_info(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return 0;
}

static ssize_t lit_ctl(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	return count;
}

#endif

static DEVICE_ATTR(lit, 0644, lit_info, lit_ctl);

static int pmic_light_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("PMIC ADC start probe\n");
	ret = device_create_file(&(pdev->dev), &dev_attr_lit);
	if (ret) {
		pr_debug("Can't create device file!\n");
		return -ENODEV;
	}

//	pmic_light_init_reg();

	pr_debug("PMIC Light successfully loaded\n");
	return 0;
}

static struct platform_driver pmic_light_driver_ldm = {
	.driver = {
		   .name = "pmic_light",
		   },
	.suspend = pmic_light_suspend,
	.resume = pmic_light_resume,
	.probe = pmic_light_probe,
	.remove = pmic_light_remove,
};

/*
 * Initialization and Exit
 */

static int __init pmic_light_init(void)
{
	pr_debug("PMIC Light driver loading...\n");
	return platform_driver_register(&pmic_light_driver_ldm);
}
static void __exit pmic_light_exit(void)
{
	platform_driver_unregister(&pmic_light_driver_ldm);
	pr_debug("PMIC Light driver successfully unloaded\n");
}

/*
 * Module entry points
 */

subsys_initcall(pmic_light_init);
module_exit(pmic_light_exit);

MODULE_DESCRIPTION("PMIC_LIGHT");
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_LICENSE("GPL");

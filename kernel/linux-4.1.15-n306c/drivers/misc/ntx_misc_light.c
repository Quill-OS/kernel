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
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/leds.h>

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

struct led_classdev *gptLEDcdev_green , *gptLEDcdev_red , *gptLEDcdev_blue ;
struct led_classdev *gptLEDcdev_cur;

int ntx_register_ledcdev_green(struct led_classdev *led_cdev)
{
	//printk("%s():led_cdev@%p\n",__FUNCTION__,led_cdev);
	if(gptLEDcdev_green) {
		printk(KERN_ERR"%s():green ledcdev has been registed !\n",__FUNCTION__);
		return -1;
	}
	gptLEDcdev_green = led_cdev;
	return 0;
}
int ntx_register_ledcdev_red(struct led_classdev *led_cdev)
{
	//printk("%s():led_cdev@%p\n",__FUNCTION__,led_cdev);
	if(gptLEDcdev_red) {
		printk(KERN_ERR"%s():red ledcdev has been registed !\n",__FUNCTION__);
		return -1;
	}
	gptLEDcdev_red = led_cdev;
	return 0;
}
int ntx_register_ledcdev_blue(struct led_classdev *led_cdev)
{
	//printk("%s():led_cdev@%p\n",__FUNCTION__,led_cdev);
	if(gptLEDcdev_blue) {
		printk(KERN_ERR"%s():blue ledcdev has been registed !\n",__FUNCTION__);
		return -1;
	}
	gptLEDcdev_blue = led_cdev;
	return 0;
}

void led_green (int isOn)
{
	if(gptLEDcdev_green) {
		if(isOn) {
			led_set_brightness(gptLEDcdev_green,LED_HALF);
		}
		else {
			led_set_brightness(gptLEDcdev_green,LED_OFF);
		}
	}
}

void led_blue (int isOn)
{
	if(gptLEDcdev_blue) {
		if(isOn) {
			led_set_brightness(gptLEDcdev_blue,LED_HALF);
		}
		else {
			led_set_brightness(gptLEDcdev_blue,LED_OFF);
		}
	}
}

void led_red (int isOn) 
{
	if(gptLEDcdev_red) {
		if(isOn) {
			led_set_brightness(gptLEDcdev_red,LED_HALF);
		}
		else {
			led_set_brightness(gptLEDcdev_red,LED_OFF);
		}
	}
}


void ntx_led_set_auto_blink(int iAutoBlinkMode)
{
}
void ntx_led_blink (unsigned int channel, int period)
{
	switch (channel) {
	case LIT_RED:// red .
		gptLEDcdev_cur = gptLEDcdev_red;
		break;
	case LIT_GREEN:// green .
		gptLEDcdev_cur = gptLEDcdev_green;
		break;
	case LIT_BLUE:// blue .
		gptLEDcdev_cur = gptLEDcdev_blue;
		break;
	}

	if(gptLEDcdev_cur) {
		if(0==period) {
			led_set_brightness(gptLEDcdev_cur,LED_OFF);
		}
		else {
			unsigned long delay_on_ms,delay_off_ms;

			switch(period) {
			case 1: // 1/8 s
				delay_on_ms = 1000/8;
				delay_off_ms = 1000/8;
				break;
			case 2: // 1 s
				delay_on_ms = 1000;
				delay_off_ms = 1000;
				break;
			case 3: // 2 s
				delay_on_ms = 2000;
				delay_off_ms = 2000;
				break;
			default:
				if(period>10) {
					delay_on_ms = period;
					delay_off_ms = period;
				}

			}
			led_blink_set(gptLEDcdev_cur,&delay_on_ms,&delay_off_ms);
		}
	}
}
void ntx_led_dc (unsigned int channel, unsigned char dc)
{
	switch (channel) {
	case LIT_RED:// red .
		gptLEDcdev_cur = gptLEDcdev_red;
		break;
	case LIT_GREEN:// green .
		gptLEDcdev_cur = gptLEDcdev_green;
		break;
	case LIT_BLUE:// blue .
		gptLEDcdev_cur = gptLEDcdev_blue;
		break;
	}

	if(gptLEDcdev_cur) {
		if(0==dc) {
			led_set_brightness(gptLEDcdev_cur,LED_OFF);
		}
		else {
			if(gptLEDcdev_cur->blink_delay_on && gptLEDcdev_cur->blink_delay_off ) 
			{
				led_blink_set(gptLEDcdev_cur,
					&gptLEDcdev_cur->blink_delay_on,&gptLEDcdev_cur->blink_delay_off);
			}
			else {
				led_set_brightness(gptLEDcdev_cur,LED_HALF);
			}
		}
	}

}

void ntx_led_current (unsigned int channel, unsigned char value)
{
	if(!value) {
		ntx_led_dc(channel,0);
	}
}


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

	//printk(KERN_INFO"PMIC light start probe\n");
	ret = device_create_file(&(pdev->dev), &dev_attr_lit);
	if (ret) {
		pr_debug("Can't create device file!\n");
		return -ENODEV;
	}

//	pmic_light_init_reg();
	if(sysfs_create_link(&pdev->dev.parent->kobj, &pdev->dev.kobj, "pmic_light.1")) {
		printk("[%s-%d] create pmic_light.1 link fail !\n", __func__, __LINE__);
	}

	//printk(KERN_INFO"PMIC Light successfully loaded\n");
	return 0;
}

static const struct of_device_id ntx_misc_light_dt_ids[] = {
	{ .compatible = "ntx,ntx_led", },
	{ /* sentinel */ }
};

static struct platform_driver pmic_light_driver_ldm = {
	.driver = {
		   .name = "ntx_led",
			 .owner = THIS_MODULE,
			 .of_match_table = of_match_ptr(ntx_misc_light_dt_ids),
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
	//printk(KERN_INFO"PMIC Light driver loading...\n");
	return platform_driver_register(&pmic_light_driver_ldm);
}
static void __exit pmic_light_exit(void)
{
	platform_driver_unregister(&pmic_light_driver_ldm);
	//printk(KERN_INFO"PMIC Light driver successfully unloaded\n");
}

/*
 * Module entry points
 */

subsys_initcall(pmic_light_init);
module_exit(pmic_light_exit);


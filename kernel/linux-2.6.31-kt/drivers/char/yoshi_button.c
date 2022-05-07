/*
 * Copyright 2011-2012 Amazon.com, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pmic_external.h>
#include <linux/pmic_adc.h>
#include <linux/pmic_light.h>
#include <linux/sysctl.h>
#include <linux/sysfs.h>
#include <linux/sysdev.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <mach/clock.h>
#include <mach/boardid.h>

/* There are 3 buttons on the Atlas PMIC and we use ONOFD1 */
#define YOSHI_BUTTON_MINOR	158 /* Major 10, Minor 158, /dev/yoshibutton */

/*
 * Uses Atlas PMIC
 */
#include <linux/pmic_external.h>
#include <mach/pmic_power.h>

static int mb_evt[2] = {KOBJ_OFFLINE, KOBJ_ONLINE};

static struct miscdevice button_misc_device = {
	YOSHI_BUTTON_MINOR,
	"yoshibutton",
	NULL,
};

#define LED_BLINK_THRESHOLD	2000	/* Blink for 2 seconds only */
#define CHGDETS			0x40
#define GREEN_LED_MASK		0x3f
#define GREEN_LED_LSH		15

static void yoshi_button_handler(void *);
void yoshi_button_reset_ctrl(int enable) 
{
	if (!(mx50_board_is_celeste() || mx50_board_is_celeste_wfo())) {
		return;
	}

	if (enable) {
		/* Note: ignore & clear power-button interrupts while inactive */
		pmic_write_reg(REG_INT_STATUS1, (1 << 3), (1 << 3));
		/* subscribe power button event */
		pmic_power_event_sub(PWR_IT_ONOFD1I, yoshi_button_handler);
	} else {
		/* unsubscribe power button event */
		pmic_power_event_unsub(PWR_IT_ONOFD1I, yoshi_button_handler);
	}
}

static ssize_t pbutton_ctrl_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;
	if (sscanf(buf, "%d", &value) <= 0) {
		printk(KERN_ERR "Could not update pbutton_ctrl \n");
		return -EINVAL;
	}
	if (value > 0) {
		yoshi_button_reset_ctrl(1);
	} else {
		yoshi_button_reset_ctrl(0);
	}
	return size;
}
static SYSDEV_ATTR(pbutton_ctrl, 0644, NULL, pbutton_ctrl_store);

static ssize_t pbutton_sense_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	unsigned int sense = 0;
	pmic_read_reg(REG_INT_SENSE1, &sense, (1 << 3));
	if (sense & (1 << 3))
		return sprintf(buf,"1\n");
	else 
		return sprintf(buf,"0\n");
}
static SYSDEV_ATTR(pbutton_sense, 0444, pbutton_sense_show, NULL);

static struct sysdev_class yoshi_button_sysclass = {
	.name   = "yoshi_button",
};

static struct sys_device yoshi_button_device = {
	.id = 0,
	.cls    = &yoshi_button_sysclass,
};

/*
 * Is a charger connected?
 */
static int charger_connected(void)
{
	int sense_0 = 0;
	int ret = 0; /* Default: no charger */

	pmic_read_reg(REG_INT_SENSE0, &sense_0, 0xffffff);
	if (sense_0 & CHGDETS)
		ret = 1;

	return ret;
}

extern int yoshi_button_green_led;

#ifdef CONFIG_MXC_PMIC_MC13892

static void pmic_enable_green_led(int enable)
{
	if (enable) {
		mc13892_bklit_set_current(LIT_KEY, 0x7);
		mc13892_bklit_set_ramp(LIT_KEY, 0);
		mc13892_bklit_set_dutycycle(LIT_KEY, 0x3f);
	}
	else {
		mc13892_bklit_set_current(LIT_KEY, 0);
		mc13892_bklit_set_dutycycle(LIT_KEY, 0);
	}
}
#endif

#ifdef CONFIG_MXC_PMIC_MC34708

static void pmic_enable_green_led(int enable)
{
	if (enable) {
		mc34708_bklit_set_current(LIT_KEY, 0x7);
		mc34708_bklit_set_ramp(LIT_KEY, 0);
		mc34708_bklit_set_dutycycle(LIT_KEY, 0x3f);
	}
	else {
		mc34708_bklit_set_current(LIT_KEY, 0);
		mc34708_bklit_set_dutycycle(LIT_KEY, 0);
	}
}

#endif

static int bp_pressed = 0;

static void led_timer_handle_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(led_timer_work, led_timer_handle_work);

static void led_timer_handle_work(struct work_struct *work)
{
	unsigned int sense;

	pmic_read_reg(REG_INT_SENSE1, &sense, (1 << 3));

	/* Check if button still pressed after 4 seconds */
	if (bp_pressed) {
		/* If button still pressed, blink LED twice */
		if (!(sense & (1 << 3)) ) {
			pmic_enable_green_led(0);
			mdelay(200);
			pmic_enable_green_led(1);
			mdelay(200);
			pmic_enable_green_led(0);
			mdelay(200);
			pmic_enable_green_led(1);
			mdelay(200);
			pmic_enable_green_led(0);
		}
		else
			pmic_enable_green_led(0);

		/* Clear the state */
		bp_pressed = 0;
		yoshi_button_green_led = 0;

		return;
	}

	if (!(sense & (1 << 3))) {
		/* Still pressed after 2 seconds, reschedule workqueue after 2 seconds */
		bp_pressed = 1;
		schedule_delayed_work(&led_timer_work, msecs_to_jiffies(LED_BLINK_THRESHOLD));
	}
	else {
		pmic_enable_green_led(0);
		yoshi_button_green_led = 0;
	}
}

extern void mxcuart_enable_clock(void);

/*
 * Interrupt triggered when button pressed
 */
static void yoshi_button_handler(void *param)
{
	unsigned int sense;
	unsigned int press_event = 0;
	int i, ret;
	int pb_counter = 120;	/* Monitor 15 second battery cut */

	if (!charger_connected()) {
		/* Set the green LED only if charger not connected */
		pmic_enable_green_led(1);	
		yoshi_button_green_led = 1;
		schedule_delayed_work(&led_timer_work, msecs_to_jiffies(LED_BLINK_THRESHOLD));
	}

	/*
	 * reset button - restart
	 */
	pmic_power_set_auto_reset_en(0);
	pmic_power_set_conf_button(BT_ON1B, 0, 2);	
	
	pmic_read_reg(REG_INT_SENSE1, &sense, (1 << 3));

	if (!(sense & (1 << 3))) {
		/* Delay of about 2sec */
		for (i = 0; i < 100; i++) {
			pmic_read_reg(REG_INT_SENSE1, &sense, (1 << 3));

			if (sense & (1 << 3)) {
				press_event = 1;
				break;
			}

			msleep(25);
		}
	} else {
		pmic_power_set_auto_reset_en(1);
		pmic_power_set_conf_button(BT_ON1B, 1, 2);
		press_event = 1;
	}

	if ((ret = kobject_uevent(&button_misc_device.this_device->kobj,
			   mb_evt[press_event])))
		printk(KERN_WARNING "yoshibutton: can't send uevent, returned with error:%d\n", ret);

	if (!press_event)
		do {
			msleep(50);
			if (--pb_counter == 0)
				printk(KERN_EMERG "bcut: C def:pcut:pending 15 second battery cut:\n");
			pmic_read_reg(REG_INT_SENSE1, &sense, (1 << 3));
		} while (!(sense & (1 << 3)));

	/* Atlas N1B interrupt line debouce is 30 ms */
	msleep(40);

	/* ignore release interrupts */
	pmic_write_reg(REG_INT_STATUS1, (1 << 3), (1 << 3));

	pmic_write_reg(REG_INT_MASK1, 0, (1 << 3));

	mxcuart_enable_clock();
}

static int __init yoshi_power_button_init(void)
{
	int error = 0;
	printk (KERN_INFO "Amazon MX35 Yoshi Power Button Driver\n");

	if (misc_register (&button_misc_device)) {
		printk (KERN_WARNING "yoshibutton: Couldn't register device 10, "
				"%d.\n", YOSHI_BUTTON_MINOR);
		return -EBUSY;
	}

	if (pmic_power_set_conf_button(BT_ON1B, 0, 2)) {
		printk(KERN_WARNING "yoshibutton: can't configure debounce "
		       "time\n");
		misc_deregister(&button_misc_device);
		return -EIO;
	}

	if (pmic_power_event_sub(PWR_IT_ONOFD1I, yoshi_button_handler)) {
		printk(KERN_WARNING "yoshibutton: can't subscribe to IRQ\n");
		misc_deregister(&button_misc_device);
		return -EIO;
	}

	error = sysdev_class_register(&yoshi_button_sysclass);
	if (!error)
		error = sysdev_register(&yoshi_button_device);
	if (!error) {
		error = sysdev_create_file(&yoshi_button_device, &attr_pbutton_sense);
		error = sysdev_create_file(&yoshi_button_device, &attr_pbutton_ctrl);
	}

	/* Success */
	return 0;
}

static void __exit yoshi_power_button_exit(void)
{
	sysdev_remove_file(&yoshi_button_device, &attr_pbutton_sense);
	sysdev_remove_file(&yoshi_button_device, &attr_pbutton_ctrl);
	sysdev_unregister(&yoshi_button_device);
	sysdev_class_unregister(&yoshi_button_sysclass);

	pmic_power_event_unsub(PWR_IT_ONOFD1I, yoshi_button_handler);
	misc_deregister (&button_misc_device);
}

MODULE_AUTHOR("Manish Lachwani");
MODULE_DESCRIPTION("MX50 Yoshi Power Button");
MODULE_LICENSE("GPL");

module_init(yoshi_power_button_init);
module_exit(yoshi_power_button_exit);


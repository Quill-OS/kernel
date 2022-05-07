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
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/sysdev.h>
#include <linux/workqueue.h>
#include <linux/yoshime_hall.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <mach/clock.h>
#include <mach/boardid.h>

#define HALL_DEV_MINOR		163
#define HALL_DRIVER_NAME	"yoshime_hall"

#define HALL_CLOSE_DEB		50
#define HALL_EVENT_DELAY	3000
/* Hall Sensor States */
#define STATE_OPENED		0
#define STATE_CLOSED 		1

struct hall_drvdata {
	struct yoshime_hall_platform_data* pdata;
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
};

extern int gpio_hallsensor_detect(void);
extern int gpio_hallsensor_irq(void);
/* Control yoshi power button dynamically - when hall is active & inactive */
extern void yoshi_button_reset_ctrl(int);	
extern void gpio_hallsensor_pullup(int);

static int hall_dbg = 0;
static int hall_wkup_enabled = 0;	/* hall wakeup source enabled */
static int is_hall_wkup = 0;		/* set when last wakeup due to hall event (cover open) */
static int hall_event = 0;			/* track hall events (open / close) */
static int hall_pullup_enabled = 0;	/* default=0 to be enabled only for VNI tests */
static int hall_close_delay = 0;	/* delay to avoid multiple transitions within short duration */
static int current_state = 0;		/* OPEN=0, CLOSE=1 */

static void hall_close_event_work_handler(struct work_struct *);
static DECLARE_DELAYED_WORK(hall_close_event_work, hall_close_event_work_handler);

static void hall_open_event_work_handler(struct work_struct *);
static DECLARE_DELAYED_WORK(hall_open_event_work, hall_open_event_work_handler);

static long hall_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int __user *argp = (int __user *)arg;
	int ret = -EINVAL;
	int state = 0;

	switch (cmd) {
		case HALL_IOCTL_GET_STATE:
			state = gpio_hallsensor_detect();
			if (put_user(state, argp))
				return -EFAULT;
			else
				ret = 0;
			break;
		default:
			break;
	}
	return ret;
}

static ssize_t hall_misc_write(struct file *file, const char __user *buf,
								size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t hall_misc_read(struct file *file, char __user *buf,
								size_t count, loff_t *pos)
{
	return 0;
}

static const struct file_operations hall_misc_fops =
{
	.owner = THIS_MODULE,
	.read  = hall_misc_read,
	.write = hall_misc_write,
	.unlocked_ioctl = hall_ioctl,
};

static struct miscdevice hall_misc_device =
{
	.minor = HALL_DEV_MINOR,
	.name  = HALL_DRIVER_NAME,
	.fops  = &hall_misc_fops,
};

static ssize_t hall_gpio_pullup_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;
	if (sscanf(buf, "%d", &value) <= 0) {
		printk(KERN_ERR "Could not update hall debug ctrl \n");
		return -EINVAL;
	}
	hall_pullup_enabled = (value > 0) ? 1 : 0;
	gpio_hallsensor_pullup(hall_pullup_enabled);
	return size;
}

static ssize_t hall_gpio_pullup_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hall_pullup_enabled);
}
static SYSDEV_ATTR(hall_gpio_pullup, 0644, hall_gpio_pullup_show, hall_gpio_pullup_store);

static ssize_t hall_trig_wkup_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_hall_wkup);
}
static SYSDEV_ATTR(hall_trig_wkup, 0444, hall_trig_wkup_show, NULL);

static ssize_t hall_debug_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;
	if (sscanf(buf, "%d", &value) <= 0) {
		printk(KERN_ERR "Could not update hall debug ctrl \n");
		return -EINVAL;
	}
	hall_dbg = (value > 0) ? 1 : 0;
	return size;
}

static ssize_t hall_debug_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hall_dbg);
}
static SYSDEV_ATTR(hall_debug, 0644, hall_debug_show, hall_debug_store);

static ssize_t hall_detect_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gpio_hallsensor_detect());
}
static SYSDEV_ATTR(hall_detect, 0444, hall_detect_show, NULL);

static struct sysdev_class yoshime_hall_sysclass = {
	.name   = "yoshime_hall",
};

static struct sys_device yoshime_hall_device = {
	.id = 0,
	.cls    = &yoshime_hall_sysclass,
};

static void hall_wakeup_ctrl(int enable)
{
	int irq = gpio_hallsensor_irq();
	int ret = 0;
	if (!(mx50_board_is_celeste() || mx50_board_is_celeste_wfo())) {
		return;
	}

	if(enable) {
		if (!hall_wkup_enabled) {
			ret = enable_irq_wake(irq);	
			if (ret)
				printk(KERN_ERR "HallSensor - Enable wakeup failed %d \n", ret);
			hall_wkup_enabled = 1;
			pr_debug("%s - hall enable wakeup flag=%d \n", __func__,hall_wkup_enabled);
		}
	} else {
		if (hall_wkup_enabled) {
			ret = disable_irq_wake(irq);
			if (ret)
				printk(KERN_ERR "HallSensor - Disable wakeup failed %d \n", ret);
			hall_wkup_enabled = 0;
			pr_debug("%s - hall disable wakeup flag=%d \n", __func__,hall_wkup_enabled);
		}
	}
}

static void hall_send_close_event(void) 
{
	char *envp[] = {"HALLSENSOR=closed", NULL};
	kobject_uevent_env(&hall_misc_device.this_device->kobj, KOBJ_ONLINE, envp);	
	current_state = STATE_CLOSED;
}

static void hall_close_event_work_handler(struct work_struct *work)
{
	if(gpio_hallsensor_detect() && current_state == STATE_OPENED) {
		hall_send_close_event();
		pr_debug("%s - hall close event - state=%d \n", __func__,current_state);
	} 
}

static void hall_open_event_work_handler(struct work_struct *work)
{
	if(!gpio_hallsensor_detect()) {
		/* hall in open state beyond the timeout so no need for delay */
		hall_close_delay = 0;
	}
}

static void hall_detwq_handler(struct work_struct *dummy)
{
	int irq = gpio_hallsensor_irq();

	if (!(mx50_board_is_celeste() || mx50_board_is_celeste_wfo())) {
		goto out;
	}

	if(gpio_hallsensor_detect()) {
		/* Hall - Close */
		yoshi_button_reset_ctrl(0);		
		set_irq_type(irq, IRQF_TRIGGER_RISING);
		hall_wakeup_ctrl(1);
		cancel_delayed_work_sync(&hall_open_event_work);
		if (hall_close_delay) {
			/* 	Note:
			 *	To avoid framework overload during back-to-back cover open-close events
			 *	defer current close event when previous open event occured immiediately after close  
			 */
			schedule_delayed_work(&hall_close_event_work, msecs_to_jiffies(HALL_EVENT_DELAY));
		} else { 
			schedule_delayed_work(&hall_close_event_work, msecs_to_jiffies(HALL_CLOSE_DEB));
		}
		if (hall_dbg) 
			printk(KERN_INFO "%s: I hall:closed::\n",HALL_DRIVER_NAME);
	} else {
		/* Hall - Open */
		char *envp[] = {"HALLSENSOR=opened", NULL};
		cancel_delayed_work_sync(&hall_close_event_work);
		if (current_state != STATE_OPENED) {
			kobject_uevent_env(&hall_misc_device.this_device->kobj, KOBJ_OFFLINE, envp);
			current_state = STATE_OPENED;
			pr_debug("%s - hall open event - state=%d \n", __func__,current_state);
		}
		hall_close_delay = 1;
		schedule_delayed_work(&hall_open_event_work, msecs_to_jiffies(HALL_EVENT_DELAY));
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
		hall_wakeup_ctrl(0);
		yoshi_button_reset_ctrl(1);		
		if (hall_dbg) 
			printk(KERN_INFO "%s: I hall:opened::\n",HALL_DRIVER_NAME);
	}
	hall_event = 1;	

out:
	enable_irq(irq);
}

static DECLARE_WORK(hall_detwq, hall_detwq_handler); 

static irqreturn_t hall_isr(int irq, void *dev_id)
{
	disable_irq_nosync(irq);
	schedule_work(&hall_detwq);

	return IRQ_HANDLED;
}

static int __devinit yoshime_hall_probe(struct platform_device *pdev)
{
	int error;
    int irq;
    struct hall_drvdata* ddata;
	struct yoshime_hall_platform_data *pdata = pdev->dev.platform_data;

	ddata = kzalloc(sizeof(struct hall_drvdata) + sizeof(struct hall_drvdata),
			GFP_KERNEL);
	if (!ddata) {
		error = -ENOMEM;
		goto fail1;
	}
	platform_set_drvdata(pdev, ddata);

	if (misc_register(&hall_misc_device)) {
		error = -EBUSY;
		printk(KERN_ERR "%s Couldn't register device %d \n",__FUNCTION__, HALL_DEV_MINOR);
		goto fail2;
	}

	irq = pdata->hall_irq;
	error = request_irq(irq, hall_isr, IRQF_TRIGGER_NONE, 
							"yoshime_hall", pdata);
	if (error) {
		printk(KERN_ERR "%s Failed to claim irq %d, error %d \n",__FUNCTION__, irq, error);
		goto fail3;
	} 

	/* Init Hall state during boot 
	 * to avoid out of sync issue may due to bcut / wdog / HR
	 */
	if(gpio_hallsensor_detect()) {
		/* Cover closed */
		yoshi_button_reset_ctrl(0);		
		set_irq_type(irq, IRQF_TRIGGER_RISING);
		hall_wakeup_ctrl(1);
		current_state = STATE_CLOSED;
	} else {
		/* Cover open */
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
		hall_wakeup_ctrl(0);
		current_state = STATE_OPENED;
	}

	error = sysdev_class_register(&yoshime_hall_sysclass);
	if (!error)
		error = sysdev_register(&yoshime_hall_device);
	if (!error) {
		error = sysdev_create_file(&yoshime_hall_device, &attr_hall_detect);
		error = sysdev_create_file(&yoshime_hall_device, &attr_hall_debug);
		error = sysdev_create_file(&yoshime_hall_device, &attr_hall_trig_wkup);
		error = sysdev_create_file(&yoshime_hall_device, &attr_hall_gpio_pullup);
	}

	return 0;

fail3:
	misc_deregister(&hall_misc_device);
fail2:
	platform_set_drvdata(pdev, NULL);
	kfree(ddata);
fail1:
	return error;
}

static int __devexit yoshime_hall_remove(struct platform_device *pdev)
{
	struct yoshime_hall_platform_data *pdata = pdev->dev.platform_data;

	sysdev_remove_file(&yoshime_hall_device, &attr_hall_detect);
	sysdev_remove_file(&yoshime_hall_device, &attr_hall_debug);
	sysdev_remove_file(&yoshime_hall_device, &attr_hall_trig_wkup);
	sysdev_remove_file(&yoshime_hall_device, &attr_hall_gpio_pullup);
	sysdev_unregister(&yoshime_hall_device);
	sysdev_class_unregister(&yoshime_hall_sysclass);

	cancel_delayed_work_sync(&hall_open_event_work);		
	cancel_delayed_work_sync(&hall_close_event_work);		

	misc_deregister(&hall_misc_device);
	disable_irq_wake(pdata->hall_irq);
	free_irq(pdata->hall_irq, pdev->dev.platform_data);	
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static int yoshime_hall_suspend(struct platform_device *pdev, pm_message_t state)
{
	int irq = gpio_hallsensor_irq();

	/* Enable hall wkup when cover open */
	if(!gpio_hallsensor_detect()) {		
		set_irq_type(irq, IRQF_TRIGGER_FALLING);
		hall_wakeup_ctrl(1);		
		pr_debug("%s - cover open - state=%d \n", __func__, gpio_hallsensor_detect());
	}

	is_hall_wkup = 0;
	hall_event = 0;
	return 0;
}

static int yoshime_hall_resume(struct platform_device *pdev)
{
	int irq = gpio_hallsensor_irq();

	if (hall_event) { 
		is_hall_wkup = 1;
		hall_event = 0;
		pr_debug("%s - hall event - state=%d \n", __func__, gpio_hallsensor_detect());
	} else {
		/* no hall event - restore hall state if cover is still open */
		if(!gpio_hallsensor_detect()) { 
			set_irq_type(irq, IRQF_TRIGGER_FALLING);
			hall_wakeup_ctrl(0);	
			pr_debug("%s - cover open - state=%d \n", __func__, gpio_hallsensor_detect());
		}
	}
	return 0;
}

static struct platform_driver yoshime_hall_device_driver = {
	.driver		= {
		.name	= "yoshime_hall",
		.owner	= THIS_MODULE,
	},
	.probe		= yoshime_hall_probe,
	.remove		= __devexit_p(yoshime_hall_remove),
	.suspend 	= yoshime_hall_suspend,
	.resume 	= yoshime_hall_resume,
};

static int __init yoshime_hall_init(void)
{
	return platform_driver_register(&yoshime_hall_device_driver);
}

static void __exit yoshime_hall_exit(void)
{
	platform_driver_unregister(&yoshime_hall_device_driver);
}

module_init(yoshime_hall_init);
module_exit(yoshime_hall_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vidhyananth Venkatasamy");
MODULE_DESCRIPTION("hall sensor Driver for yoshime platform");

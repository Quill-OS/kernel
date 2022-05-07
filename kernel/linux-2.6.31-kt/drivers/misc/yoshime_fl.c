/*
 * Copyright (C) 2011-2012 Amazon Technologies Inc. All rights reserved.
 * Vidhyananth Venkatasamy (venkatas@lab126.com)
 *
 * TPS6116x - Front Light driver based on PWM / 1-Wire interface
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <mach/yoshime_fl_int.h>
#include <linux/sysfs.h>
#include <linux/sysdev.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

struct fl_data {
#ifdef CONFIG_YOSHIME_FL_PWM
	struct pwm_device *pwm;
	int active_low;
	int period;
#endif
	int max_intensity;
	int def_intensity;
};

struct fl_data *fl_gdata = NULL;
static int intensity = 0;
static int intensity_saved = 0;
static int is_intensity_zero = 0;
static void fl_set_intensity(int);
extern void gpio_fl_ctrl(unsigned int);

#ifdef CONFIG_YOSHIME_FL_OWIRE
extern void gpio_fl_owire_enable(int);
static void fl_owire_init(void);
static void owire_write_byte(u8);
/* TPS6116x Easyscale timing w/ margin (usecs) */
#define T_POWER_SETTLE		2000
#define T_ES_DELAY			120
#define T_ES_DETECT			280
#define T_ES_WINDOW			(1000 - T_ES_DELAY - T_ES_DETECT)
#define T_START				3
#define T_EOS				3
#define T_INACTIVE			3
#define T_ACTIVE			(3 * T_INACTIVE)
#define CMD_SET				0x72
#endif

#define FL_DRIVER_NAME 		"yoshime_fl"
#define FL_DEV_MINOR		162
#define FL_INTENSITY_MIN	0

/*
 * sysfs entries for Front-Light
 */
static ssize_t fl_intensity_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
    int value = 0;

    if (sscanf(buf, "%d", &value) <= 0) {
        printk(KERN_ERR "Could not store the codec register value\n");
        return -EINVAL;
    }

    fl_set_intensity(value);

    return size;
}

static ssize_t fl_intensity_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
    char *curr = buf;

    curr += sprintf(curr, "FrontLight(Intensity) = %d\n", intensity);
    curr += sprintf(curr, "\n");

    return curr - buf;
}
static SYSDEV_ATTR(fl_intensity, 0644, fl_intensity_show, fl_intensity_store);

static struct sysdev_class fl_tps6116x_sysclass = {
    .name   = "fl_tps6116x",
};

static struct sys_device fl_tps6116x_device = {
    .id = 0,
    .cls    = &fl_tps6116x_sysclass,
};

#ifdef CONFIG_YOSHIME_FL_OWIRE
static void owire_write_byte(u8 data)
{
	int bit;
	gpio_fl_ctrl(1);
	udelay(T_START);
	for (bit = 0; bit < 8; bit++, data <<= 1) {
		int val = data & 0x80;
		int t_lo = val ? T_INACTIVE : T_ACTIVE;
		int t_hi = val ? T_ACTIVE : T_INACTIVE;
		gpio_fl_ctrl(0);
		udelay(t_lo);
		gpio_fl_ctrl(1);
		udelay(t_hi);
	}
	gpio_fl_ctrl(0);
	udelay(T_EOS);
	gpio_fl_ctrl(1);
}

static void fl_owire_init()
{
	unsigned long flags;
	local_irq_save(flags);
	gpio_fl_ctrl(1);
	udelay(T_ES_DELAY);
	gpio_fl_ctrl(0);
	udelay(T_ES_DETECT);
	gpio_fl_ctrl(1);
	local_irq_restore(flags);
}
#endif

static void fl_set_intensity(int value)
{
	int max = fl_gdata->max_intensity;
	intensity = clamp(value, 0, max);
#ifdef CONFIG_YOSHIME_FL_PWM	
	{
		int period = fl_gdata->period;
		if (intensity == 0) {
			if (!is_intensity_zero) {
				pwm_config(fl_gdata->pwm, 0, period);
				pwm_disable(fl_gdata->pwm);
				is_intensity_zero = 1;
			}
		} else {
			pwm_config(fl_gdata->pwm, intensity * period / max, period);
			pwm_enable(fl_gdata->pwm);
			is_intensity_zero = 0;
		}
	}
#elif defined(CONFIG_YOSHIME_FL_OWIRE)
	owire_write_byte(CMD_SET);
	owire_write_byte(intensity);
#endif
}

static long fl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int __user *argp = (int __user *)arg;
	int ret = -EINVAL;
	int val = 0;

	switch (cmd) {
		case FL_IOCTL_SET_INTENSITY:
			if (get_user(val, argp))
				return -EFAULT;
			else {
				fl_set_intensity(val);
				ret = 0;
			}
			break;
		case FL_IOCTL_GET_INTENSITY:
			if (put_user(intensity, argp))
				return -EFAULT;
			else
				ret = 0;
			break;
		case FL_IOCTL_GET_RANGE_MAX:
			if (put_user(fl_gdata->max_intensity, argp))
				return -EFAULT;
			else
				ret = 0;
			break;
		default:
			break;
	}
	return ret;
}

static ssize_t fl_misc_write(struct file *file, const char __user *buf,
								size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t fl_misc_read(struct file *file, char __user *buf,
								size_t count, loff_t *pos)
{
	return 0;
}

static const struct file_operations fl_misc_fops =
{
	.owner = THIS_MODULE,
	.read  = fl_misc_read,
	.write = fl_misc_write,
	.unlocked_ioctl = fl_ioctl,
};

static struct miscdevice fl_misc_device =
{
	.minor = FL_DEV_MINOR,
	.name  = FL_DRIVER_NAME,
	.fops  = &fl_misc_fops,
};

static int yoshime_fl_probe(struct platform_device *pdev)
{
	struct yoshime_fl_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
	int error = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "%s No platform data \n",__FUNCTION__);
		return -EBUSY;
	}

	fl_gdata = kzalloc(sizeof(struct fl_data), GFP_KERNEL);
	if (!fl_gdata) {
		dev_err(&pdev->dev, "%s Failed to allocate fl data \n",__FUNCTION__);
		return -ENOMEM;
	}

	fl_gdata->max_intensity = pdata->max_intensity;
	fl_gdata->def_intensity = pdata->def_intensity;
#ifdef CONFIG_YOSHIME_FL_PWM	
	fl_gdata->pwm = pwm_request(pdata->pwm_id, pdev->name);
	if (IS_ERR(fl_gdata->pwm)) {
		dev_err(&pdev->dev, "%s unable to request PWM %d\n",__FUNCTION__, pdata->pwm_id);
		ret = -EBUSY;
		goto err;
	}
	fl_gdata->period = pdata->pwm_period_ns;

#elif defined(CONFIG_YOSHIME_FL_OWIRE)
	fl_owire_init();
#endif

	error = sysdev_class_register(&fl_tps6116x_sysclass);
	if (!error)
		error = sysdev_register(&fl_tps6116x_device);
	if (!error) 
		error = sysdev_create_file(&fl_tps6116x_device, &attr_fl_intensity);

	if (misc_register(&fl_misc_device)) {
		dev_err(&pdev->dev, "%s Couldn't register device %d.\n",__FUNCTION__, FL_DEV_MINOR);
		ret = -EBUSY;
		goto err;
	}
	
	fl_set_intensity(fl_gdata->def_intensity);
	platform_set_drvdata(pdev, fl_gdata);
	return 0;

#ifdef CONFIG_YOSHIME_FL_PWM	
err:
#endif
	kfree(fl_gdata);
	return ret;
}

static int __devexit yoshime_fl_remove(struct platform_device *pdev)
{
	struct fl_data *fl_gdata;

	fl_set_intensity(FL_INTENSITY_MIN);
	misc_deregister(&fl_misc_device);

	sysdev_remove_file(&fl_tps6116x_device, &attr_fl_intensity);
	sysdev_unregister(&fl_tps6116x_device);
	sysdev_class_unregister(&fl_tps6116x_sysclass);

	fl_gdata = platform_get_drvdata(pdev);

#ifdef CONFIG_YOSHIME_FL_PWM	
	pwm_free(fl_gdata->pwm);
#endif

	kfree(fl_gdata);

	return 0;
}

static int yoshime_fl_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* save the original intensity */
	intensity_saved = intensity;	
	fl_set_intensity(FL_INTENSITY_MIN);
#ifdef CONFIG_YOSHIME_FL_OWIRE
	gpio_fl_owire_enable(0);
#endif
	return 0;
}

static int yoshime_fl_resume(struct platform_device *pdev)
{
#ifdef CONFIG_YOSHIME_FL_OWIRE
	gpio_fl_owire_enable(1);
	fl_owire_init();
#endif
	/* set it to original intensity */
	fl_set_intensity(intensity_saved);
	return 0;
}

static struct platform_driver yoshime_fl_driver = {
	.driver		= {
		.name	= "yoshime_fl",
		.owner	= THIS_MODULE,
	},
	.probe		= yoshime_fl_probe,
	.remove		= __devexit_p(yoshime_fl_remove),
	.suspend 	= yoshime_fl_suspend,
	.resume 	= yoshime_fl_resume,
};

static int __init yoshime_fl_init(void)
{
	return platform_driver_register(&yoshime_fl_driver);
}

static void __exit yoshime_fl_exit(void)
{
	platform_driver_unregister(&yoshime_fl_driver);
}

module_init(yoshime_fl_init);
module_exit(yoshime_fl_exit);

MODULE_AUTHOR("Vidhyananth Venkatasamy <venkatas@lab126.com>");
MODULE_DESCRIPTION("TPS6116x FL driver");
MODULE_LICENSE("GPL");

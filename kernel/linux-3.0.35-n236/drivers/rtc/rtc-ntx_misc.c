/*
 * Copyright (C) 2011 Netronix, Inc.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
 * Implementation based on rtc-ntx_misc.c
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/sched.h>

struct rtc_drv_data {
	struct rtc_device *rtc;
	int irq;
	bool irq_enable;
};

static unsigned long rtc_status;

static DEFINE_SPINLOCK(rtc_lock);
DECLARE_COMPLETION(ntx_misc_completion);
static int64_t time_diff;

extern int up_get_time(struct rtc_time *tm);
extern int up_set_time(struct rtc_time *tm);
extern int up_get_alarm(struct rtc_time *tm);
extern int up_set_alarm(struct rtc_time *tm);

static unsigned long rtc_read_second (void)
{
	struct rtc_time tm;
	unsigned long time;
	up_get_time (&tm);
	rtc_tm_to_time(&tm, &time);
	return time;
}

/*!
 * This function updates the RTC alarm registers and then clears all the
 * interrupt status bits.
 *
 * @param  alrm         the new alarm value to be updated in the RTC
 *
 * @return  0 if successful; non-zero otherwise.
 */
static int rtc_update_alarm(struct device *dev, struct rtc_time *alrm, int isEnable)
{
	struct rtc_drv_data *pdata = dev_get_drvdata(dev);
	struct rtc_time alarm_tm, now_tm;
	unsigned long now, time;
	int ret;
	unsigned long flags;
	
	if (!isEnable) {
		return up_set_alarm (0);
	}

	up_get_time (&now_tm);

	alarm_tm.tm_year = now_tm.tm_year;
	alarm_tm.tm_mon = now_tm.tm_mon;
	alarm_tm.tm_mday = now_tm.tm_mday;

	alarm_tm.tm_hour = alrm->tm_hour;
	alarm_tm.tm_min = alrm->tm_min;
	alarm_tm.tm_sec = alrm->tm_sec;

	rtc_tm_to_time(&now_tm, &now);
	rtc_tm_to_time(&alarm_tm, &time);

	if (time < now) {
		time += 60 * 60 * 24;
		rtc_time_to_tm(time, &alarm_tm);
	}
	ret = rtc_tm_to_time(&alarm_tm, &time);

	up_set_alarm (&alarm_tm);

	return ret;
}

/*!
 * This function is the RTC interrupt service routine.
 *
 * @param  irq          RTC IRQ number
 * @param  dev_id       device ID which is not used
 *
 * @return IRQ_HANDLED as defined in the include/linux/interrupt.h file.
 */
static irqreturn_t ntx_misc_rtc_interrupt(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct rtc_drv_data *pdata = platform_get_drvdata(pdev);
	u32 events = 0;

	printk ("[%s-%d] ...",__func__,__LINE__);
	if (pdata->irq_enable) {
		disable_irq_nosync(pdata->irq);
		pdata->irq_enable = false;
	}
	rtc_update_irq(pdata->rtc, 1, (RTC_AF | RTC_IRQF));
	return IRQ_HANDLED;
}

/*!
 * This function is used to open the RTC driver.
 *
 * @return  0 if successful; non-zero otherwise.
 */
static int ntx_misc_rtc_open(struct device *dev)
{
	if (test_and_set_bit(1, &rtc_status))
		return -EBUSY;
	return 0;
}

/*!
 * clear all interrupts and release the IRQ
 */
static void ntx_misc_rtc_release(struct device *dev)
{
	rtc_status = 0;
}

/*!
 * This function reads the current RTC time into tm in Gregorian date.
 *
 * @param  tm           contains the RTC time value upon return
 *
 * @return  0 if successful; non-zero otherwise.
 */
static int ntx_misc_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	up_get_time (tm);
	printk ("[%s-%d] %d/%d/%d %d:%d:%d\n",__func__,__LINE__,
		tm->tm_year ,
		tm->tm_mon 	,
		tm->tm_mday ,
		tm->tm_hour ,
		tm->tm_min 	,
		tm->tm_sec 	
	); 
	return 0;
}

/*!
 * This function sets the internal RTC time based on tm in Gregorian date.
 *
 * @param  tm           the time value to be set in the RTC
 *
 * @return  0 if successful; non-zero otherwise.
 */
static int ntx_misc_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time;
	int ret;

	printk ("[%s-%d] %d/%d/%d %d:%d:%d\n",__func__,__LINE__,
		tm->tm_year ,
		tm->tm_mon 	,
		tm->tm_mday ,
		tm->tm_hour ,
		tm->tm_min 	,
		tm->tm_sec 	
	);
	
	ret = rtc_tm_to_time(tm, &time);
	if (ret != 0)
		return ret;
	
	up_set_time (tm);

	return 0;
}

/*!
 * This function reads the current alarm value into the passed in \b alrm
 * argument. It updates the \b alrm's pending field value based on the whether
 * an alarm interrupt occurs or not.
 *
 * @param  alrm         contains the RTC alarm value upon return
 *
 * @return  0 if successful; non-zero otherwise.
 */
static int ntx_misc_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_drv_data *pdata = dev_get_drvdata(dev);

	up_get_alarm (&alrm->time);
	alrm->pending = 0;

	return 0;
}

/*!
 * This function sets the RTC alarm based on passed in alrm.
 *
 * @param  alrm         the alarm value to be set in the RTC
 *
 * @return  0 if successful; non-zero otherwise.
 */
static int ntx_misc_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_drv_data *pdata = dev_get_drvdata(dev);
	unsigned long lock_flags = 0;
	int ret=0;

	if (rtc_valid_tm(&alrm->time)) {
		if (alrm->time.tm_sec > 59 ||
		    alrm->time.tm_hour > 23 || alrm->time.tm_min > 59) {
			return -EINVAL;
		}
	}

	if (alrm->enabled) {
		ret = rtc_update_alarm(dev, &alrm->time, 1);
		if (ret)
			goto out;
			
		if (!pdata->irq_enable) {
			if (pdata->irq >= 0) 
				enable_irq(pdata->irq);
			pdata->irq_enable = true;
		}
	}
	else {
		rtc_update_alarm(dev, &alrm->time, 0);
		if (pdata->irq >= 0) 
			disable_irq(pdata->irq);
		pdata->irq_enable = false;
	}

out:
	return ret;
}

/*!
 * This function is used to provide the content for the /proc/driver/rtc
 * file.
 *
 * @param  seq  buffer to hold the information that the driver wants to write
 *
 * @return  The number of bytes written into the rtc file.
 */
static int ntx_misc_rtc_proc(struct device *dev, struct seq_file *seq)
{
	struct rtc_drv_data *pdata = dev_get_drvdata(dev);

	seq_printf(seq, "alarm_IRQ\t: %s\n",
		   (pdata->irq_enable ? "yes" : "no"));

	return 0;
}

static int ntx_misc_rtc_alarm_irq_enable(struct device *dev, unsigned int enable)
{
	struct rtc_drv_data *pdata = dev_get_drvdata(dev);
	if (enable) {
		if (!pdata->irq_enable) {
			if (pdata->irq >= 0) 
				enable_irq(pdata->irq);
			pdata->irq_enable = true;
		}
	} else {
		if (pdata->irq_enable) {
			if (pdata->irq >= 0) 
				disable_irq(pdata->irq);
			pdata->irq_enable = false;
			up_set_alarm (0);
		}
	}
	return 0;
}

/*!
 * This function is used to support some ioctl calls directly.
 * Other ioctl calls are supported indirectly through the
 * arm/common/rtctime.c file.
 *
 * @param  cmd          ioctl command as defined in include/linux/rtc.h
 * @param  arg          value for the ioctl command
 *
 * @return  0 if successful or negative value otherwise.
 */
static int ntx_misc_rtc_ioctl(struct device *dev, unsigned int cmd,
			 unsigned long arg)
{
	return -ENOIOCTLCMD;
}
/*!
 * The RTC driver structure
 */
static struct rtc_class_ops ntx_misc_rtc_ops = {
	.open = ntx_misc_rtc_open,
	.release = ntx_misc_rtc_release,
	.read_time = ntx_misc_rtc_read_time,
	.set_time = ntx_misc_rtc_set_time,
	.read_alarm = ntx_misc_rtc_read_alarm,
	.set_alarm = ntx_misc_rtc_set_alarm,
	.proc = ntx_misc_rtc_proc,
	.ioctl = ntx_misc_rtc_ioctl,
	.alarm_irq_enable = ntx_misc_rtc_alarm_irq_enable,
};

/*! NTX MISC RTC Power management control */
static int ntx_misc_rtc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct rtc_device *rtc;
	struct rtc_drv_data *pdata = NULL;
	u32 lp_cr;
	int ret = 0;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	platform_set_drvdata(pdev, pdata);

	pdata->irq = pdev->dev.platform_data;
	if (pdata->irq >= 0) {
		printk ("[%s-%d] request_irq %d...\n",__func__,__LINE__,pdata->irq);
		if (request_irq(pdata->irq, ntx_misc_rtc_interrupt, IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
				pdev->name, pdev) < 0) {
			dev_warn(&pdev->dev, "interrupt %d not available.\n", pdata->irq);
			pdata->irq = -1;
		} else {
			enable_irq_wake(pdata->irq);
			disable_irq(pdata->irq);
			pdata->irq_enable = false;
		}
	}

	rtc = rtc_device_register(pdev->name, &pdev->dev,
				  &ntx_misc_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		ret = PTR_ERR(rtc);
		goto err_out;
	}

	pdata->rtc = rtc;

	/* Remove can_wakeup flag to add common power wakeup interface */
	pdev->dev.power.can_wakeup = 0;

	/* By default, devices should wakeup if they can */
	/* So snvs is set as "should wakeup" as it can */
	device_init_wakeup(&pdev->dev, 1);

	return ret;

err_out:
	kfree(pdata);
	return ret;
}

static int __exit ntx_misc_rtc_remove(struct platform_device *pdev)
{
	struct rtc_drv_data *pdata = platform_get_drvdata(pdev);
	rtc_device_unregister(pdata->rtc);
	kfree(pdata);
	return 0;
}

/*!
 * This function is called to save the system time delta relative to
 * the NTX MISC RTC when enterring a low power state. This time delta is
 * then used on resume to adjust the system time to account for time
 * loss while suspended.
 *
 * @param   pdev  not used
 * @param   state Power state to enter.
 *
 * @return  The function always returns 0.
 */
static int ntx_misc_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
#if 0
	struct rtc_drv_data *pdata = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev)) {
		printk ("[%s-%d] enable_irq_wake %d ...",__func__,__LINE__,pdata->irq);
		enable_irq_wake(pdata->irq);
	} else {
		if (pdata->irq_enable) {
			printk ("[%s-%d] disable_irq %d ...",__func__,__LINE__,pdata->irq);
			disable_irq(pdata->irq);
		}
	}
#endif
	return 0;
}

/*!
 * This function is called to correct the system time based on the
 * current NTX MISC RTC time relative to the time delta saved during
 * suspend.
 *
 * @param   pdev  not used
 *
 * @return  The function always returns 0.
 */
static int ntx_misc_rtc_resume(struct platform_device *pdev)
{
#if 0
	struct rtc_drv_data *pdata = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev)) {
		disable_irq_wake(pdata->irq);
	} else {
		if (pdata->irq_enable)
			enable_irq(pdata->irq);
	}
#endif
	return 0;
}

/*!
 * Contains pointers to the power management callback functions.
 */
static struct platform_driver ntx_misc_rtc_driver = {
	.driver = {
		   .name = "ntx_misc_rtc",
		   },
	.probe = ntx_misc_rtc_probe,
	.remove = __exit_p(ntx_misc_rtc_remove),
	.suspend = ntx_misc_rtc_suspend,
	.resume = ntx_misc_rtc_resume,
};

/*!
 * This function creates the /proc/driver/rtc file and registers the device RTC
 * in the /dev/misc directory. It also reads the RTC value from external source
 * and setup the internal RTC properly.
 *
 * @return  -1 if RTC is failed to initialize; 0 is successful.
 */
static int __init ntx_misc_rtc_init(void)
{
	return platform_driver_register(&ntx_misc_rtc_driver);
}

/*!
 * This function removes the /proc/driver/rtc file and un-registers the
 * device RTC from the /dev/misc directory.
 */
static void __exit ntx_misc_rtc_exit(void)
{
	platform_driver_unregister(&ntx_misc_rtc_driver);

}

module_init(ntx_misc_rtc_init);
module_exit(ntx_misc_rtc_exit);

MODULE_AUTHOR("Netronix, Inc.");
MODULE_DESCRIPTION("NTX-MISC Realtime Clock Driver (RTC)");
MODULE_LICENSE("GPL");

/*
 * twl6030_vbus - Simple driver to handle the VBUS detection
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/i2c/twl.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>

#define STS_HW_CONDITIONS               0x21
#define STS_USB_ID                      BIT(2)

#define CONTROLLER_STAT1		0x03
#define	VBUS_DET			BIT(2)
#define SINGLE_SHOT_TIME (10*1000) //in ms

struct twl6030_vbus {
	struct device		*dev;
	spinlock_t		lock;
	int			(*callback)(struct device *dev, int on);
	int			irq;
	int			present;
};

#define vbus_to_twl(x)		container_of((x), struct twl6030_vbus, vbus);

/*-------------------------------------------------------------------------*/

static struct wake_lock vbus_detect_wake_lock;

static inline int twl6030_writeb(struct twl6030_vbus *twl, u8 module,
						u8 data, u8 address)
{
	int ret = 0;

	ret = twl_i2c_write_u8(module, data, address);
	if (ret < 0)
		dev_err(twl->dev,
			"Write[0x%x] Error %d\n", address, ret);
	return ret;
}

static inline u8 twl6030_readb(struct twl6030_vbus *twl, u8 module, u8 address)
{
	u8 data, ret = 0;

	ret = twl_i2c_read_u8(module, &data, address);
	if (ret >= 0)
		ret = data;
	else
		dev_err(twl->dev,
			"readb[0x%x,0x%x] Error %d\n",
					module, address, ret);
	return ret;
}

static ssize_t twl6030_vbus_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct twl6030_vbus *twl = dev_get_drvdata(dev);
	unsigned long flags;
	int ret = -EINVAL;

	spin_lock_irqsave(&twl->lock, flags);

	switch (twl->present) {
	case 1:
	       ret = snprintf(buf, PAGE_SIZE, "vbus\n");
	       break;
	case 0:
	       ret = snprintf(buf, PAGE_SIZE, "none\n");
	       break;
	default:
	       ret = snprintf(buf, PAGE_SIZE, "UNKNOWN\n");
	}
	spin_unlock_irqrestore(&twl->lock, flags);

	return ret;
}
static DEVICE_ATTR(vbus, 0444, twl6030_vbus_show, NULL);

static irqreturn_t twl6030_vbus_irq(int irq, void *_twl)
{
	struct twl6030_vbus *twl = _twl;
	struct device *dev;
	struct twl4030_vbus_data *pdata;
	u8 vbus_state, hw_state;
	
	wake_lock_timeout(&vbus_detect_wake_lock, 10*HZ);

	dev  = twl->dev;
	pdata = dev->platform_data;

	hw_state = twl6030_readb(twl, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);

	vbus_state = twl6030_readb(twl, TWL_MODULE_MAIN_CHARGE,
						CONTROLLER_STAT1);
	if (!(hw_state & STS_USB_ID)) {
		if ( (vbus_state & VBUS_DET) && !twl->present ) {
			
			twl6030_set_power_source(POWER_SUPPLY_TYPE_MAINS);
			
			wake_lock_timeout(&vbus_detect_wake_lock, 10*HZ);
			
			twl->present = 1;
			pdata->board_control_vbus_power(twl->dev, 1);
		} else if( !(vbus_state & VBUS_DET) && twl->present) {
			
			if ( wake_lock_active(&vbus_detect_wake_lock) ){
				wake_unlock(&vbus_detect_wake_lock);
			}
			
			twl->present = 0;
			pdata->board_control_vbus_power(twl->dev, 0);
		}
	}
	sysfs_notify(&twl->dev->kobj, NULL, "vbus");

	return IRQ_HANDLED;
}

static int twl6030_enable_irq(struct twl6030_vbus *twl)
{
	twl6030_interrupt_unmask(0x08, REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(0x08, REG_INT_MSK_STS_C);

	return 0;
}

static struct twl6030_vbus *private_twl;

static void vbus_single_shot_worker(struct work_struct *work)
{
	twl6030_vbus_irq(private_twl->irq, (void *)private_twl);
}
static DECLARE_DELAYED_WORK(vbus_single_shot_work, vbus_single_shot_worker);

static int __devinit twl6030_vbus_probe(struct platform_device *pdev)
{
	struct twl6030_vbus	*twl;
	int			status;
	struct twl4030_vbus_data *pdata;
	struct device *dev = &pdev->dev;
	pdata = dev->platform_data;

	wake_lock_init(&vbus_detect_wake_lock, WAKE_LOCK_SUSPEND, "vbus_detect_wake_lock");
	
	twl = kzalloc(sizeof *twl, GFP_KERNEL);
	if (!twl)
		return -ENOMEM;

	twl->dev		= &pdev->dev;
	twl->irq		= platform_get_irq(pdev, 0);
	twl->callback		= pdata->board_control_vbus_power;

	spin_lock_init(&twl->lock);

	platform_set_drvdata(pdev, twl);
	if (device_create_file(&pdev->dev, &dev_attr_vbus))
		dev_warn(&pdev->dev, "could not create sysfs file\n");

	status = request_threaded_irq(twl->irq, NULL, twl6030_vbus_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"twl6030_vbus", twl);
	if (status < 0) {
		dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
			twl->irq, status);
		kfree(twl);
		return status;
	}

	private_twl = twl;
	schedule_delayed_work(&vbus_single_shot_work, msecs_to_jiffies(SINGLE_SHOT_TIME));

	twl6030_enable_irq(twl);

	dev_info(&pdev->dev, "Initialized TWL6030 VUSB module\n");

	return 0;
}

static int __exit twl6030_vbus_remove(struct platform_device *pdev)
{
	struct twl6030_vbus *twl = platform_get_drvdata(pdev);

	struct twl4030_vbus_data *pdata;
	struct device *dev = &pdev->dev;
	pdata = dev->platform_data;
	
	wake_lock_destroy(&vbus_detect_wake_lock);

	cancel_delayed_work_sync(&vbus_single_shot_work);
	twl6030_interrupt_mask(0x08,
		REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(0x08,
			REG_INT_MSK_STS_C);
	free_irq(twl->irq, twl);
	device_remove_file(twl->dev, &dev_attr_vbus);
	kfree(twl);

	return 0;
}

static struct platform_driver twl6030_vbus_driver = {
	.probe		= twl6030_vbus_probe,
	.remove		= __exit_p(twl6030_vbus_remove),
	.driver		= {
		.name	= "twl6030_vbus",
		.owner	= THIS_MODULE,
	},
};

static int __init twl6030_vbus_init(void)
{
	return platform_driver_register(&twl6030_vbus_driver);
}
subsys_initcall(twl6030_vbus_init);

static void __exit twl6030_vbus_exit(void)
{
	platform_driver_unregister(&twl6030_vbus_driver);
}
module_exit(twl6030_vbus_exit);

MODULE_ALIAS("platform:twl6030_vbus");
MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("TWL6030 VBUS irq handler");
MODULE_LICENSE("GPL");

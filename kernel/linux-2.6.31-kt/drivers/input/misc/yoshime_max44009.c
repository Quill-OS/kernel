/*
 * Copyright (C) 2011 Maxim Integrated Products
 * Copyright (C) 2011-2012  Amazon.com, Inc. or its affiliates All Rights Reserved.
 * ALS(MAX44009) driver for Yoshime platform
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/sysdev.h>
#include <linux/hwmon.h>
#include <linux/yoshime_als.h>

#define MAX44009_ALS				"yoshime_max44009"
#define MAX44009_ALS_DEV_MINOR 		166
#define DRIVER_VERSION				"1.0"

#define MAX44009_I2C_ADDRESS0       0x94
#define MAX44009_I2C_ADDRESS1       0x96
#define MAX44009_INT_STATUS_ADDR    0x00
#define MAX44009_INT_ENABLE_ADDR    0x01
#define MAX44009_CONFIG_ADDR        0x02
#define MAX44009_LUX_HIGH_ADDR      0x03
#define MAX44009_LUX_LOW_ADDR       0x04
#define MAX44009_THRESH_HIGH_ADDR   0x05
#define MAX44009_THRESH_LOW_ADDR    0x06
#define MAX44009_THRESH_TIM_ADDR    0x07
#define MAX44009_RETRY_DELAY        1
#define MAX44009_MAX_RETRIES        1
#define MAX44009_HYSTERESIS			10	

extern int gpio_max44009_als_int(void);
static struct i2c_client *max44009_i2c_client = NULL;
static struct device *max44009_hwmon = NULL;
static u8 max44009_reg_number = 0;
#ifdef MAX44009_INPUT_DEV	
static struct input_dev *max44009_idev = NULL;
#endif

static void max44009_als_wqthread(struct work_struct *);
static DECLARE_WORK(max44009_als_wq, max44009_als_wqthread);
static int max44009_read_lux(int *);

struct max44009_thresh_zone {
	u8 upper_thresh;
	u8 lower_thresh;
};

static long als_max44009_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int __user *argp = (int __user *)arg;
	int ret = -EINVAL;
	int lux = 0;

	switch (cmd) {
		case ALS_IOCTL_GET_LUX:
			max44009_read_lux(&lux);
			if (put_user(lux, argp))
				return -EFAULT;
			else
				ret = 0;
			break;
		default:
			break;
	}
	return ret;
}

static ssize_t als_max44009_misc_write(struct file *file, const char __user *buf,
								size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t als_max44009_misc_read(struct file *file, char __user *buf,
								size_t count, loff_t *pos)
{
	return 0;
}

static const struct file_operations als_max44009_misc_fops =
{
	.owner = THIS_MODULE,
	.read  = als_max44009_misc_read,
	.write = als_max44009_misc_write,
	.unlocked_ioctl = als_max44009_ioctl,
};

static struct miscdevice als_max44009_misc_device =
{
	.minor = MAX44009_ALS_DEV_MINOR,
	.name  = MAX44009_ALS,
	.fops  = &als_max44009_misc_fops,
};

static int max44009_write_reg(u8 reg_num, u8 value)
{
	s32 err;
	
	err = i2c_smbus_write_byte_data(max44009_i2c_client, reg_num, value);
	if (err < 0) {
		printk(KERN_ERR "max44009: i2c write error\n");
	}
	
	return err;
}

static int max44009_read_reg(u8 reg_num, u8 *value)
{
	s32 retval;
	
	retval = i2c_smbus_read_byte_data(max44009_i2c_client, reg_num);
	if (retval < 0) {
		printk(KERN_ERR "max44009: i2c read error\n");
		return retval;
	}
	
	*value = (u8) (retval & 0xFF);
	return 0;
}

/*
 *  max44009_read_spl - Performs a special read of two bytes to a MAX44009 device.
 *  This follows the recommended method of reading the high and low lux values 
 *  per the application note AN5033.pdf 
 *
 *  This special format for two byte read avoids the stop between bytes.
 *  Otherwise the stop would allow the MAX44009 to update the low lux byte,
 *  and the two bytes could be out of sync:
 *    [S] [Slave Addr + 0(W)] [ACK] [Register 1] [ACK] [Sr] [Slave Addr + 1(R)]
 *    [ACK] [Data 1] [NACK] [Sr] [S] [Slave Addr + 0(W)] [ACK] [Register 2] [ACK]
 *    [Sr] [Slave Addr + 1(R)] [ACK] [Data 2] [NACK] [P]
 *
 */
static int max44009_read_spl(u8 *buffer)
{
	int err;        
	int retry = 0;  
	
	struct i2c_msg msgs[] =
	{
		{	/* first message is the register address */
			.addr = max44009_i2c_client->addr,
			.flags = max44009_i2c_client->flags,
			.len = 1,
			.buf = buffer,
		}, 
		{	/* second message is the read length */
			.addr = max44009_i2c_client->addr,
			.flags = max44009_i2c_client->flags | I2C_M_RD,
			.len = 1,
			.buf = buffer,
		}, 
		{	/* third message is the next register address */
			.addr = max44009_i2c_client->addr,
			.flags = max44009_i2c_client->flags,
			.len = 1,
			.buf = &(buffer[1]),
		}, 
		{	/* fourth message is the next read length */
			.addr = max44009_i2c_client->addr,
			.flags = max44009_i2c_client->flags | I2C_M_RD,
			.len = 1,
			.buf = &(buffer[1]),
		}, 
	};

	do
	{
		err = i2c_transfer(max44009_i2c_client->adapter, msgs, 4);
		if (err != 4) {
		    msleep_interruptible(MAX44009_RETRY_DELAY);
			printk(KERN_ERR "%s: I2C SPL read transfer error err=%d\n", __FUNCTION__, err);
		}
	} while ((err != 4) && (++retry < MAX44009_MAX_RETRIES));
	
	if (err != 4) {
		printk(KERN_ERR "%s: read transfer error err=%d\n", __FUNCTION__, err);
		err = -EIO;
	} else {
		err = 0;
		printk(KERN_INFO "%s LUX Hi=0x%02x Lo=0x%02x\n", __FUNCTION__,
					buffer[0], buffer[1]);
	}
	return err;
}

static void max44009_reg_dump(void)
{
	u8 i = 0;
	u8 value = 0xff;
	int ret = 0;
	
	for (i = 0; i <= 0x7; i++) {
		ret = max44009_read_reg(i, &value);
		if (ret) {
			printk(KERN_ERR "%s: ERROR reading MAX44009 registers \n",__FUNCTION__);
			break;
		}
		printk(KERN_DEBUG "MAX44009: reg=0x%x, value=0x%x\n", i, value);		
	}
}

/*
 * /sys access to the max44009 registers
 */
static ssize_t max44009_regaddr_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;
	
	if (sscanf(buf, "%x", &value) <= 0) {
	    printk(KERN_ERR "Could not store the max44009 register address\n");
	    return -EINVAL;
	}
	
	max44009_reg_number = value;
	return size;
}

static ssize_t max44009_regaddr_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	char *curr = buf;
	
	curr += sprintf(curr, "max44009 register address = 0x%x\n", max44009_reg_number);
	curr += sprintf(curr, "\n");
	
	return curr - buf;
}
static SYSDEV_ATTR(max44009_regaddr, 0644, max44009_regaddr_show, max44009_regaddr_store);

static ssize_t max44009_regval_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;
	
	if (sscanf(buf, "%x", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}
	
	if (max44009_reg_number == 0x03 || max44009_reg_number == 0x04) {
		printk(KERN_ERR "Error trying to update readonly register\n");
		return -EINVAL;
	}
	
	max44009_write_reg(max44009_reg_number, value);
	return size;
}

static ssize_t max44009_regval_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	unsigned char value = 0xff;
	char *curr = buf;
	
	if (max44009_reg_number >= 0x08) {
		/* dump all the regs */
		curr += sprintf(curr, "MAX44009 Register Dump\n");
		curr += sprintf(curr, "\n");
		max44009_reg_dump();
	}
	else {
		max44009_read_reg(max44009_reg_number, &value);
		curr += sprintf(curr, "0x%x\n", value);
	}
	
	return curr - buf;
};
static SYSDEV_ATTR(max44009_regval, 0644, max44009_regval_show, max44009_regval_store);

/*
 * /sys access to get the lux value
 */
static ssize_t max44009_lux_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	char *curr = buf;
	int lux = 0;
	max44009_read_lux(&lux);
	curr += sprintf(curr, "%d\n", lux);
	return curr - buf;
};
static SYSDEV_ATTR(max44009_lux, 0644, max44009_lux_show, NULL);

static struct sysdev_class max44009_ctrl_sysclass = {
    .name   = "max44009_ctrl",
};

static struct sys_device device_max44009_ctrl = {
    .id     = 0,
    .cls    = &max44009_ctrl_sysclass,
};

/*
 *  Updates the integration time settings in the configuration register
 *
 */
int max44009_set_integration_time(u8 time)
{
	int err = 0;
	u8 config = 0;
	printk(KERN_DEBUG "max44009_set_integration_time\n");
	
	err = max44009_read_reg(MAX44009_CONFIG_ADDR, &config);
	if (err)
		return err;
	
	if (!(config & 0x40))
		return 0;
	
	config &= 0xF8;          
	config |= (time & 0x7);  
	                        
	err = max44009_write_reg(MAX44009_CONFIG_ADDR, config);
	if (err) {
		printk("%s: couldn't update config: integ_time\n", __FUNCTION__);
	}
	return err;
}

/*
 *  Enables or disables manual mode on the MAX44009
 */
int max44009_set_manual_mode(bool enable)
{
	int err = 0;
	u8 config = 0;
	bool currently_manual; 
	
	err = max44009_read_reg(MAX44009_CONFIG_ADDR, &config);
	if (err) {
		printk("%s: couldn't read config \n", __FUNCTION__);
		return err;
	}
	
	currently_manual = (config & 0x40) ? true : false;
	
	if (currently_manual != enable)
	{
		if (enable)
			config |= 0x40;
		else
			config &= 0xBF;          
	
		err = max44009_write_reg(MAX44009_CONFIG_ADDR, config);
		if (err) {
			printk("%s: couldn't update config: auto/manual mode status \n", __FUNCTION__);
		}
	}
	return err;
}

/*
 *  Enables or disables continuous mode on the MAX44009 
 */
int max44009_set_continuous_mode(bool enable)
{
	int err = 0;
	u8 config = 0;
	bool currently_cont; 
	
	err = max44009_read_reg(MAX44009_CONFIG_ADDR, &config);
	if (err) {
		printk("%s: couldn't read config \n", __FUNCTION__);
		return err;
	}
	
	currently_cont = (config & 0x80) ? true : false;
	
	if (currently_cont != enable) {
		if (enable)
			config |= 0x80;
		else
			config &= 0x7F;          
	
		err = max44009_write_reg(MAX44009_CONFIG_ADDR, config);
		if (err) {
			printk("%s: couldn't update config: cont mode status \n", __FUNCTION__);
		}
	}
	return err;
}

/*
 *  Sets or clears the current division ratio bit, if necessary
 *
 */
int max44009_set_current_div_ratio(bool enable)
{
	int err = 0;
	u8 config = 0;
	bool currently_cdr; 
	
	err = max44009_read_reg(MAX44009_CONFIG_ADDR, &config);
	if (err) {
		printk("%s: couldn't read config \n", __FUNCTION__);
		return err;
	}
	
	currently_cdr = (config & 0x08) ? true : false;
	
	if (currently_cdr != enable)
	{
		if (enable)
			config |= 0x08;
		else
			config &= 0xF7;          
		
		err = max44009_write_reg(MAX44009_CONFIG_ADDR, config);
		if (err) {
			printk("%s: couldn't update config: cdr mode \n", __FUNCTION__);
		}
	}
	return err;
}

/*
 *  Updates threshold timer register to a new value
 */ 
int max44009_set_thresh_tim(u8 new_tim_val)
{
	int err = 0;
	
	err = max44009_write_reg(MAX44009_THRESH_TIM_ADDR, new_tim_val);
	if (err) {
		printk("%s: couldn't update config: cdr mode \n", __FUNCTION__);
	}
	return err;
}

/*
 *  Changes the threshold zone based on the current lux reading
 */
int max44009_set_threshold_zone(struct max44009_thresh_zone *new_zone)
{
	int err = 0;
	printk(KERN_DEBUG "max44009 set_threshold_zone %d %d\n",
					((new_zone->upper_thresh & 0xf0) + 0x0f) <<
					(new_zone->upper_thresh & 0xf),
					(new_zone->lower_thresh & 0xf0) <<
					(new_zone->lower_thresh & 0xf));
	
	err = max44009_write_reg(MAX44009_THRESH_HIGH_ADDR, new_zone->upper_thresh) ;
	if (err) {
		printk("%s: couldn't update upper threshold \n", __FUNCTION__);
		goto set_err;
	}
	
	err = max44009_write_reg(MAX44009_THRESH_LOW_ADDR, new_zone->lower_thresh);
	if (err) {
		printk("%s: couldn't update lower threshold \n", __FUNCTION__);
	}

set_err:
	return err;
}

/*
 *  Gets the lux reading from the light sensor
 */
static int max44009_read_lux(int *adjusted_lux)
{
	u8 exponent = 0, mantissa = 0;
	u8 lux_high = 0;
	u8 lux_low = 0;
	
	/* do a read of the ADC - LUX[HI] register (0x03) */
	if (max44009_read_reg(MAX44009_LUX_HIGH_ADDR, &lux_high) != 0) {
		printk("%s: couldn't read lux(HI) data\n", __FUNCTION__);
		return -EIO;
	} 
	
	/* calculate the lux value */
	exponent = (lux_high >> 4) & 0x0F;
	if (exponent == 0x0F) {
		printk("%s: overload on light sensor!\n", __FUNCTION__);
		/* maximum reading (per datasheet) */
		*adjusted_lux = 188006; 
		return -EINVAL;
	}

	/* do a read of the ADC - LUX[LO] register (0x04) */
	if (max44009_read_reg(MAX44009_LUX_LOW_ADDR, &lux_low) != 0) {
		printk("%s: couldn't read lux(LOW) data\n", __FUNCTION__);
		return -EIO;
	} 

	mantissa = ((lux_high & 0x0F) << 4) | (lux_low & 0x0F);
	/* LUX = [2pow(exp)] * mantissa * 0.72 */
	*adjusted_lux = ((int)(1 << exponent)) * mantissa * 18 / 25;
	
	/* TODO: add any other adjustments 
	 * 		 (e.g. correct for a lens,glass, other filtering, etc)
	 */
	return 0;
}

/*
 *  Converts a lux value into a threshold mantissa and exponent.
 *
 */
int max44009_lux_to_thresh(int lux)
{
	long result = 0;
	int mantissa = 0;
	int exponent = 0;
	
	if (lux == 0) {
		/* 0 Has no Mantissa */
		printk(KERN_INFO "max44009 lux_to_thresh lux=0x%04x result=0x%02x\n", lux, 0);
		return 0;
	}
	
	/* Lux (max) = 0x2DE66 
	 * Add some for rounding
	 * Range of the Mantissa is 8 to 15 so rounding 
	 * is half the average LSB.
	 */
	result = (lux + lux / 24);
	result = result * 40;
	
	while (result >= 0x100) {
		result = result >> 1;
		exponent++;
	}
	
	mantissa = (result >> 4) & 0xf;
	result = mantissa + (exponent << 4);
	
	printk(KERN_INFO "max44009 lux_to_thresh lux=0x%x result=0x%x\n", lux, (int)result);
	
	return (int)result;
}

#ifdef MAX44009_AUTO_MODE 	
/*
 * Read the ambient light level from MAX44009 device & sets the reading
 * as an input event on the if the reading is valid
 */
int max44009_report_light_level(void)
{
	int err;
	int lux; 
	struct max44009_thresh_zone zone;
	
	/* lux goes into the input device as an input event */
	err = max44009_read_lux(&lux);
	if (err == 0) {
		/* Reset lux interrupt thresholds */
		zone.upper_thresh = max44009_lux_to_thresh(lux +
					((lux * MAX44009_HYSTERESIS) >> 8));
		
		if ((lux - ((lux * MAX44009_HYSTERESIS) >> 8)) < 0)
		{
			zone.lower_thresh = max44009_lux_to_thresh(0);
		}
		else
		{
			zone.lower_thresh = max44009_lux_to_thresh(lux -
					((lux * MAX44009_HYSTERESIS) >> 8));
		}
		
		err = max44009_set_threshold_zone(&zone);
		
		printk(KERN_INFO "%s to upper layer lux=%d\n",__FUNCTION__,lux);
		printk(KERN_INFO "%s delta=%d raw thresh=%d %d\n",__FUNCTION__,
						(lux * MAX44009_HYSTERESIS) >> 8,
						lux + ((lux * MAX44009_HYSTERESIS) >> 8),
						lux - ((lux * MAX44009_HYSTERESIS) >> 8));
	
#ifdef MAX44009_INPUT_DEV	
		/*  Report event to upper layer */
		input_report_abs(max44009_idev, ABS_MISC, lux);
		input_sync(max44009_idev);
#endif
	} else {
	    printk(KERN_ERR "%s: problem getting lux reading from MAX44009\n", __FUNCTION__);
	}
	
	return err;
} 
#endif

static void max44009_als_wqthread(struct work_struct *dummy)
{
	int irq = gpio_max44009_als_int();
	u8 intsts = 0;
	int err = 0;

#ifdef MAX44009_AUTO_MODE 	
	max44009_report_light_level();
#endif
	
	err = max44009_read_reg(MAX44009_INT_STATUS_ADDR, &intsts);
	if (err) {
		printk(KERN_ERR "%s: couldn't read interrupt sts : %d  \n", __FUNCTION__, err);
	}
	enable_irq(irq);
}

static irqreturn_t max44009_als_irq(int irq, void *device)
{
	disable_irq_nosync(irq);
	schedule_work(&max44009_als_wq);
	return IRQ_HANDLED;
}

/*  
 * Initialize max44009 to low power manual mode  
 */
static int max44009_init_lpm(void)
{
	int err = 0;

    /* initialize configuration register - cont mode disabled */
	err = max44009_write_reg(MAX44009_CONFIG_ADDR, 0x40);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__, MAX44009_CONFIG_ADDR);
		goto failed;
	}

    /* initialize upper threshold */
	err = max44009_write_reg(MAX44009_THRESH_HIGH_ADDR, 0xFF);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__, MAX44009_THRESH_HIGH_ADDR);
		goto failed;
	}

    /* initialize lower threshold */
	err = max44009_write_reg(MAX44009_THRESH_LOW_ADDR, 0x0);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__, MAX44009_THRESH_LOW_ADDR);
		goto failed;
	}

	/* initialize threshold timer */
	err = max44009_write_reg(MAX44009_THRESH_TIM_ADDR, 0xFF);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__, MAX44009_THRESH_TIM_ADDR);
		goto failed;
	}
	return 0;

failed:
	printk("%s: couldn't initialize lpm mode \n", __FUNCTION__);
	return err;
}

/*  
 * Initialize max44009 to continuous mode with 
 * threshold (lower & upper) & interrupt enabled
 */
static int max44009_init_normal(void)
{
	int err = 0;
	
	/* initialize receiver configuration register */
	err = max44009_write_reg(MAX44009_CONFIG_ADDR, 0x80);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__, MAX44009_CONFIG_ADDR);
		goto failed;
	}
	
	/* initialize interrupt configuration register */
	err = max44009_write_reg(MAX44009_INT_ENABLE_ADDR, 0x01);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__,MAX44009_INT_ENABLE_ADDR); 
		goto failed;
	}
	
	/* initialize upper threshold */
	/* Sunlight=0xFD; Roomlight=0x88; Hysteresis=0x0 */
	err = max44009_write_reg(MAX44009_THRESH_HIGH_ADDR, 0x0);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__, MAX44009_THRESH_HIGH_ADDR);
		goto failed;
	}
	
	/* initialize lower threshold */
	/* Shadow=0x58 ; Cover=0x16 ; Hysteresis=0xFF */
	err = max44009_write_reg(MAX44009_THRESH_LOW_ADDR, 0xFF);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__, MAX44009_THRESH_LOW_ADDR);
		goto failed;
	}
	
	/* initialize threshold timer */
	err = max44009_write_reg(MAX44009_THRESH_TIM_ADDR, 0x01);
	if (err) {
		printk(KERN_ERR "%s reg %d write failed \n", __FUNCTION__, MAX44009_THRESH_TIM_ADDR);
		goto failed;
	}
	return 0;

failed:
	printk("%s: couldn't initialize %d\n", __FUNCTION__,err);
	return err;
} 

static int __devinit max44009_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int err = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int irq = gpio_max44009_als_int();
#ifdef MAX44009_INPUT_DEV	
	struct input_dev *idev;
#endif
	
	if(!i2c_check_functionality(adapter, I2C_FUNC_I2C |  
		I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA)) {	
		printk(KERN_ERR "%s: I2C_FUNC not supported\n", __FUNCTION__);
		return -ENODEV;
	}
	
#ifdef MAX44009_INPUT_DEV	
	max44009_idev = input_allocate_device();
	if (!max44009_idev) {
		dev_err(&client->dev, "alloc input device failed!\n");
		return -ENOMEM;
	}
	idev = max44009_idev;
	idev->name = MAX44009_ALS;
	idev->id.bustype = BUS_I2C;
	idev->dev.parent = &client->dev;
	input_set_capability(idev, EV_MSC, MSC_RAW);
	
	err = input_register_device(idev);
	if (err) {
		dev_err(&client->dev, "register input device failed!\n");
		goto err_reg_device;
	}
#endif
	
	max44009_hwmon = hwmon_device_register(&client->dev);
	if (IS_ERR(max44009_hwmon)) {
		printk(KERN_ERR "%s: Failed to initialize hw monitor\n", __FUNCTION__);
		err = -EINVAL;
		goto err_hwmon;
	}
	
	max44009_i2c_client = client;
	
	err = max44009_init_lpm();
	if (err) {
		printk(KERN_ERR "%s: Register initialization LPM failed: %d\n", __FUNCTION__, err);
		err = -ENODEV;
		goto err_init;
	}
	
	err = request_irq(irq, max44009_als_irq,
					IRQF_DISABLED | IRQF_TRIGGER_FALLING, "max44009_int", NULL);
	if (err != 0) {
		printk(KERN_ERR "IRQF_DISABLED: Could not get IRQ %d\n", irq);
		goto err_init;
	}

	if (misc_register(&als_max44009_misc_device)) {
		err = -EBUSY;
		printk(KERN_ERR "%s Couldn't register device %d \n",__FUNCTION__, MAX44009_ALS_DEV_MINOR);
		goto err_irq;
	}
	
	err = sysdev_class_register(&max44009_ctrl_sysclass);
	if (!err)
		err = sysdev_register(&device_max44009_ctrl);
	if (!err) {
		err = sysdev_create_file(&device_max44009_ctrl, &attr_max44009_regaddr);
		err = sysdev_create_file(&device_max44009_ctrl, &attr_max44009_regval);
		err = sysdev_create_file(&device_max44009_ctrl, &attr_max44009_lux);
	}
	
	return 0;

err_irq:
	free_irq(gpio_max44009_als_int(), NULL);
err_init:
	max44009_i2c_client = NULL;
	hwmon_device_unregister(max44009_hwmon);	
	max44009_hwmon = NULL;
err_hwmon:
#ifdef MAX44009_INPUT_DEV	
	input_unregister_device(max44009_idev);
err_reg_device:
	input_free_device(max44009_idev);
	max44009_idev = NULL;
#endif
	return err;
}

static int __devexit max44009_remove(struct i2c_client *client)
{
	sysdev_remove_file(&device_max44009_ctrl, &attr_max44009_regaddr);
	sysdev_remove_file(&device_max44009_ctrl, &attr_max44009_regval);
	sysdev_remove_file(&device_max44009_ctrl, &attr_max44009_lux);
	
	sysdev_unregister(&device_max44009_ctrl);
	sysdev_class_unregister(&max44009_ctrl_sysclass);

	misc_deregister(&als_max44009_misc_device);
	free_irq(gpio_max44009_als_int(), NULL);

	hwmon_device_unregister(max44009_hwmon);	
	max44009_hwmon = NULL;
#ifdef MAX44009_INPUT_DEV	
	if (max44009_idev) {
		input_unregister_device(max44009_idev);
		input_free_device(max44009_idev);
		max44009_idev = NULL;
	}
#endif
	return 0;
}

static int max44009_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int max44009_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id max44009_id[] =  {
	{MAX44009_ALS, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max44009_id);

static struct i2c_driver max44009_i2c_driver = {
	.driver = {
		.name = MAX44009_ALS,
		},
	.probe = max44009_probe,
	.remove = max44009_remove,
	.suspend = max44009_suspend,
	.resume = max44009_resume,
	.id_table = max44009_id,
};

static int __init yoshime_max44009_init(void)
{
	int err = 0;

	err = i2c_add_driver(&max44009_i2c_driver);
	if (err) {
		printk(KERN_ERR "Yoshime max44009: Could not add i2c driver\n");
		return err;
	}
	return 0;
}
	

static void __exit yoshime_max44009_exit(void)
{
	i2c_del_driver(&max44009_i2c_driver);
}

module_init(yoshime_max44009_init);
module_exit(yoshime_max44009_exit);

MODULE_DESCRIPTION("MX50 Yoshime MAXIM ALS (MAX44009) Driver");
MODULE_AUTHOR("Vidhyananth Venkatasamy");
MODULE_LICENSE("GPL");

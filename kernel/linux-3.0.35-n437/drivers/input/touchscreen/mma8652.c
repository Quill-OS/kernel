/*
 *  mma8652.c - Linux kernel modules for 3-Axis Orientation/Motion
 *  Detection Sensor MMA8451/MMA8452/MMA8453/MMA8652/MMA8653
 *
 *  Copyright (C) 2012-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/input.h>
#include <linux/gpio.h>

#define MMA8X5X_I2C_ADDR	0x1D
#define MMA8451_ID			0x1A
#define MMA8452_ID			0x2A
#define MMA8453_ID			0x3A
#define MMA8652_ID			0x4A
#define MMA8653_ID			0x5A

#define MMA8X5X_STATUS_ZYXDR	0x08
#define MMA8X5X_BUF_SIZE	6
/* register enum for mma8652 registers */
enum {
	MMA8X5X_STATUS = 0x00,
	MMA8X5X_OUT_X_MSB,
	MMA8X5X_OUT_X_LSB,
	MMA8X5X_OUT_Y_MSB,
	MMA8X5X_OUT_Y_LSB,
	MMA8X5X_OUT_Z_MSB,
	MMA8X5X_OUT_Z_LSB,

	MMA8X5X_F_SETUP = 0x09,
	MMA8X5X_TRIG_CFG,
	MMA8X5X_SYSMOD,
	MMA8X5X_INT_SOURCE,
	MMA8X5X_WHO_AM_I,
	MMA8X5X_XYZ_DATA_CFG,
	MMA8X5X_HP_FILTER_CUTOFF,

	MMA8X5X_PL_STATUS,
	MMA8X5X_PL_CFG,
	MMA8X5X_PL_COUNT,
	MMA8X5X_PL_BF_ZCOMP,
	MMA8X5X_P_L_THS_REG,

	MMA8X5X_FF_MT_CFG,
	MMA8X5X_FF_MT_SRC,
	MMA8X5X_FF_MT_THS,
	MMA8X5X_FF_MT_COUNT,

	MMA8X5X_TRANSIENT_CFG = 0x1D,
	MMA8X5X_TRANSIENT_SRC,
	MMA8X5X_TRANSIENT_THS,
	MMA8X5X_TRANSIENT_COUNT,

	MMA8X5X_PULSE_CFG,
	MMA8X5X_PULSE_SRC,
	MMA8X5X_PULSE_THSX,
	MMA8X5X_PULSE_THSY,
	MMA8X5X_PULSE_THSZ,
	MMA8X5X_PULSE_TMLT,
	MMA8X5X_PULSE_LTCY,
	MMA8X5X_PULSE_WIND,

	MMA8X5X_ASLP_COUNT,
	MMA8X5X_CTRL_REG1,
	MMA8X5X_CTRL_REG2,
	MMA8X5X_CTRL_REG3,
	MMA8X5X_CTRL_REG4,
	MMA8X5X_CTRL_REG5,

	MMA8X5X_OFF_X,
	MMA8X5X_OFF_Y,
	MMA8X5X_OFF_Z,

	MMA8X5X_REG_END,
};

/* The sensitivity is represented in counts/g. In 2g mode the
sensitivity is 1024 counts/g. In 4g mode the sensitivity is 512
counts/g and in 8g mode the sensitivity is 256 counts/g.
 */
enum {
	MODE_2G = 0,
	MODE_4G,
	MODE_8G,
};

enum {
	MMA_STANDBY = 0,
	MMA_ACTIVED,
};
struct mma8652_data_axis{
	short x;
	short y;
	short z;
};
struct mma8652_data{
	struct i2c_client * client;
//	struct input_polled_dev *poll_dev;
	struct mutex data_lock;
	int active;
	int position;
	u8 chip_id;
	int mode;
};
/* Addresses scanned */
static const unsigned short normal_i2c[] = {0x1c, 0x1d, I2C_CLIENT_END};

static int mma8652_chip_id[] ={
 	MMA8451_ID,
 	MMA8452_ID,
 	MMA8453_ID,
 	MMA8652_ID,
	MMA8653_ID,  
};
static char * mma8652_names[] ={
   "mma8451",
   "mma8452",
   "mma8453",
   "mma8652",
   "mma8653",
};

struct i2c_client *g_mma8652_client;
static struct delayed_work mma8652_delay_work;
static struct timer_list tap_timer;
static const char MMA8652_NAME[]	= "mma8652";
static struct input_dev *idev;
static int pulse_triggered, trans_triggered, orient_triggered, fifo_outofrange;

extern int gSleep_Mode_Suspend;
static int g_fifo_ZSumMax = 120, g_fifo_ZSumMin = 60;

static irqreturn_t mma8652_interrupt(int irq, void *dev_id)
{
//	printk("[%s-%d] mma8652 interrupt(%d) received\n",__func__,__LINE__,irq);
	schedule_delayed_work(&mma8652_delay_work, 0);
	return IRQ_HANDLED;
}

static void tap_timer_func (unsigned long v)
{
//	printk("sum==> %d %d %d\n", SumX, SumY, SumZ);    
	if (trans_triggered || orient_triggered || fifo_outofrange) {
//		printk("[%s-%d] No Tap to report \n",__func__,__LINE__);
	}  
	else {
		if (0xCC == pulse_triggered) {
			printk("[%s-%d] Double Tap triggered\n",__func__,__LINE__);
	               	input_report_key(idev, 0x40, 1);
        	       	input_report_key(idev, 0x40, 0);
               		input_sync(idev);
		}
		else if (0xC4 == pulse_triggered) {		
			printk("[%s-%d] Tap triggered\n",__func__,__LINE__);
	                input_report_key(idev, 0x80, 1);
        	        input_report_key(idev, 0x80, 0);
              		input_sync(idev);
		}
		else {
			printk("undefined pulse triggered 0x%02X, ignore pulse.\n", pulse_triggered);
		}
	}		
	trans_triggered = orient_triggered = pulse_triggered = fifo_outofrange = 0;
}

static void mma8652_work_func(struct work_struct *work)
{
	u8 tmp_data[1], int_src;
	int SumX=0, SumY=0, SumZ=0;
		
	int_src = i2c_smbus_read_byte_data(g_mma8652_client, MMA8X5X_INT_SOURCE);
//	printk("int source 0x%02X\n", int_src);
	if (int_src & 0x08) {	// Pulse triggered
		pulse_triggered |= i2c_smbus_read_byte_data(g_mma8652_client, MMA8X5X_PULSE_SRC);
//		printk("Pulse int triggered. %02X\n", pulse_triggered); 	
		mod_timer(&tap_timer, jiffies + 40);	// trigger timer 400ms latter
	}
	else if (int_src & 0x20) {	// Transient triggered
//		printk("Transient int triggered. Ignore pulse!!\n"); 	
		tmp_data[0] = i2c_smbus_read_byte_data(g_mma8652_client, MMA8X5X_TRANSIENT_SRC);
		trans_triggered = 1;
		mod_timer(&tap_timer, jiffies + 40);	// trigger timer 400ms latter
	}
	else if (int_src & 0x10) {	// Orientation triggered
//		printk("Orientation int triggered!!\n"); 	
		tmp_data[0] = i2c_smbus_read_byte_data(g_mma8652_client, MMA8X5X_PL_STATUS);
		orient_triggered = 1;
		mod_timer(&tap_timer, jiffies + 40);	// trigger timer 400ms latter
	}
	else if (int_src & 0x40) {   // FIFO triggered
		signed char DataFIFO[6*32];
		int fifoCnt;
		uint16_t i, isample=1;
		bool bOffset=0;
		for(i=0;i<3*32;i+=3){
			 i2c_smbus_read_i2c_block_data(g_mma8652_client, MMA8X5X_OUT_X_MSB, 3 , (u8*)&DataFIFO[i]);
//			printk("%d,%d,%d\n", DataFIFO[i], DataFIFO[i+1], DataFIFO[i+2]);	
			SumX=SumX+abs(DataFIFO[i]);
			SumY=SumY+abs(DataFIFO[i+1]);
			SumZ=SumZ+abs(DataFIFO[i+2]);
		}
		printk("SumX=%d SumY=%d SumZ=%d\n", SumX, SumY, SumZ); 
		if((SumX>SumZ)||(SumY>SumZ)||(SumZ>g_fifo_ZSumMax)||(SumZ<g_fifo_ZSumMin)||((SumZ>110)&&((SumX>70) ||(SumY>70)))){
			printk("!!!!Discard!!!!\n\n");
			fifo_outofrange = 1;			
		}
		else {
//			printk("TAP\n\n");
		}
	}
	if (!gpio_get_value(irq_to_gpio(g_mma8652_client->irq))) 
		schedule_delayed_work(&mma8652_delay_work, 1);	
}

static int mma8652_check_id(int id){
	int i = 0;
	for(i = 0 ; i < sizeof(mma8652_chip_id)/sizeof(mma8652_chip_id[0]);i++)
		if(id == mma8652_chip_id[i])
			return 1;
	return 0;
}
static char * mma8652_id2name(u8 id){
	return mma8652_names[(id >> 4)-1];
}
static int mma8652_device_init(struct i2c_client *client)
{
	int result,val;
    	struct mma8652_data *pdata = i2c_get_clientdata(client);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, 0);
	if (result < 0)
		goto out;

	// FIFO setting
	result = i2c_smbus_write_byte_data(client, MMA8X5X_XYZ_DATA_CFG, 0x11);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, 0x0a); //400HZ
//	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, 0x02); //data rate 800hz
	// Orient initial
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PL_CFG, 0xC0);
	 //Set the Back/Front Angle trip points
	 //Set the Z-Lockout angle trip point
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PL_BF_ZCOMP, 0xC7);
	 //Set the Trip Threshold Angle
	 //Set the Hysteresis Angle
	result = i2c_smbus_write_byte_data(client, MMA8X5X_P_L_THS_REG, 0x84);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PL_COUNT, 0xC8);
	
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PULSE_CFG, 0x70);  	// enable Z pulse events
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PULSE_THSX, 0x20); 	// set x,y threshold 
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PULSE_THSY, 0x20);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PULSE_THSZ, 0x08); 	// set z threshold 
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PULSE_TMLT, 0x60);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PULSE_LTCY, 0x50);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_PULSE_WIND, 0xF0);

	// shake initial
	// Enable Y and Z Axes and enable the latch
	result = i2c_smbus_write_byte_data(client, MMA8X5X_TRANSIENT_CFG, 0x1C);
	// Set the Threshold
	// Note: Step count is 0.063g per count
	// 0.25g / 0.063g = 3.97. Therefore set the threshold to 4 counts
	result = i2c_smbus_write_byte_data(client, MMA8X5X_TRANSIENT_THS, 0x03);
	// Set the Debounce Counter for 50 ms
	// Note: 400 Hz ODR, therefore 2.5 ms step sizes
	result = i2c_smbus_write_byte_data(client, MMA8X5X_TRANSIENT_COUNT, 0x14);
	
	 //Set the FIFO to	Trigger mode, and set Watermark to 20 in Register 0x09 F_Setup
	result = i2c_smbus_write_byte_data(client, MMA8X5X_F_SETUP, 0xD4);
	 //Set the FIFO Trigger for Tap Detection
	result = i2c_smbus_write_byte_data(client, MMA8X5X_TRIG_CFG, 0x08);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG4, 0x68);
	
/* END */
	val = i2c_smbus_read_byte_data(client,MMA8X5X_CTRL_REG1);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val|0x01);  
	if(!result){
		pdata->active = MMA_ACTIVED;
	}
	return 0;
out:
	dev_err(&client->dev, "error when init mma8652:(%d)", result);
	return result;
}
static int mma8652_device_stop(struct i2c_client *client)
{
#if 0
	u8 val;
	val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
	i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1,val & 0xfe);
#endif
	return 0;
}

static ssize_t mma8652_tap_threshold_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
    char threshold = i2c_smbus_read_byte_data(g_mma8652_client, MMA8X5X_PULSE_THSZ);
	return sprintf(buf, "%d\n", threshold);
}

static ssize_t mma8652_fifo_ZSumMax_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_fifo_ZSumMax);
}

static ssize_t mma8652_fifo_ZSumMin_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_fifo_ZSumMin);
}

static ssize_t mma8652_tap_threshold_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int  threshold;
	threshold = simple_strtoul(buf, NULL, 10);    
	if (0 > threshold || 127 < threshold) {
		printk ("Threshold out of randge(1-127).");
		return -1;
	}
	printk ("Set tap threshold to %d\n", threshold);
	i2c_smbus_write_byte_data(g_mma8652_client, MMA8X5X_PULSE_THSZ, threshold);
	return count;
}

static ssize_t mma8652_fifo_ZSumMax_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int  zmax;
	zmax = simple_strtoul(buf, NULL, 10);   
 	if (zmax <= g_fifo_ZSumMin) {
		printk ("Please choose a number higher than %d\n", g_fifo_ZSumMin);
		return -1;
	}
	printk ("Set tap fifo Z-sum maxium to %d\n", zmax);
	g_fifo_ZSumMax = zmax;
	return count;
}

static ssize_t mma8652_fifo_ZSumMin_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int  zmin;
	zmin = simple_strtoul(buf, NULL, 10);
     	if (zmin >= g_fifo_ZSumMax) {
		printk ("Please choose a number lower than %d\n", g_fifo_ZSumMax);
		return -1;
	}
	printk ("Set tap fifo Z-sum minium to %d\n", zmin);
	g_fifo_ZSumMin = zmin;
	return count;
}


static DEVICE_ATTR(tap_threshold, S_IWUSR | S_IRUGO,
		   mma8652_tap_threshold_show, mma8652_tap_threshold_store);
static DEVICE_ATTR(tap_zmax, S_IWUSR | S_IRUGO,
		   mma8652_fifo_ZSumMax_show, mma8652_fifo_ZSumMax_store);
static DEVICE_ATTR(tap_zmin, S_IWUSR | S_IRUGO,
		   mma8652_fifo_ZSumMin_show, mma8652_fifo_ZSumMin_store);

static struct attribute *mma8652_attributes[] = {
	&dev_attr_tap_threshold.attr,
	&dev_attr_tap_zmax.attr,
	&dev_attr_tap_zmin.attr,
	NULL
};

static const struct attribute_group mma8652_attr_group = {
	.attrs = mma8652_attributes,
};
static int mma8652_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int chip_id;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -ENODEV;
	chip_id = i2c_smbus_read_byte_data(client, MMA8X5X_WHO_AM_I);
	if(!mma8652_check_id(chip_id))
		return -ENODEV;
	printk(KERN_INFO "check %s i2c address 0x%x \n",
						  mma8652_id2name(chip_id),client->addr);
	strlcpy(info->type, "mma8652", I2C_NAME_SIZE);
	return 0;
}
static int __devinit mma8652_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result, chip_id, err;
	struct mma8652_data *pdata;
	struct i2c_adapter *adapter;
	adapter = to_i2c_adapter(client->dev.parent);
	result = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if(!result)
		goto err_out;

	chip_id = i2c_smbus_read_byte_data(client, MMA8X5X_WHO_AM_I);
	printk("[%s-%d] MMA8652 Chip ID = %d\n",__func__,__LINE__,chip_id);
	if(!mma8652_check_id(chip_id))
	{
	   dev_err(&client->dev,
			"read chip ID 0x%x is not equal to 0x%x,0x%x,0x%x,0x%x,0x%x!\n",
			 chip_id, MMA8451_ID, MMA8452_ID, MMA8453_ID,MMA8652_ID,MMA8653_ID);
		result = -EINVAL;
		goto err_out;
	}
    pdata = kzalloc(sizeof(struct mma8652_data), GFP_KERNEL);
	if(!pdata){
		result = -ENOMEM;
		dev_err(&client->dev, "alloc data memory error!\n");
		goto err_out;
    }
	/* Initialize the MMA8X5X chip */

	g_mma8652_client = client;

	pdata->client = client;
	pdata->chip_id = chip_id;
	pdata->mode = MODE_2G;
//	pdata->position = CONFIG_SENSORS_MMA_POSITION;
	pdata->position = 0;
	i2c_set_clientdata(client,pdata);

	idev = input_allocate_device();

	idev->name = "FreescaleAccelerometer";
	idev->uniq = mma8652_id2name(pdata->chip_id);
	idev->id.bustype = BUS_I2C;
	idev->evbit[0] = BIT_MASK(EV_KEY) ;
	
	input_set_capability(idev, EV_KEY, 0x40);
	input_set_capability(idev, EV_KEY, 0x80);

	result = input_register_device(idev);
    	result = sysfs_create_group(&idev->dev.kobj, &mma8652_attr_group);
	if (result) {
		dev_err(&client->dev, "create device file failed!\n");
		result = -EINVAL;
		input_unregister_device(idev);
	}

	INIT_DELAYED_WORK(&mma8652_delay_work, mma8652_work_func);

	err = request_irq(g_mma8652_client->irq, mma8652_interrupt, IRQF_TRIGGER_FALLING, MMA8652_NAME,MMA8652_NAME);
	if (err < 0) {
		printk(KERN_ERR "%s(%s): Can't allocate irq %d\n", __FILE__, __func__, g_mma8652_client->irq);
		cancel_delayed_work_sync (&mma8652_delay_work);
		return err;
	}

	tap_timer.function = tap_timer_func;
	init_timer(&tap_timer);

	mma8652_device_init(client);
	
	printk("mma8652 device driver probe successfully\n");
	return 0;
err_out:
	return result;
}
static int __devexit mma8652_remove(struct i2c_client *client)
{
#if 0
	struct mma8652_data *pdata = i2c_get_clientdata(client);
	struct input_polled_dev *poll_dev = pdata->poll_dev;
	mma8652_device_stop(client);
	if(pdata){
		input_unregister_polled_device(poll_dev);
		input_free_polled_device(poll_dev);
		kfree(pdata);
	}
#endif
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mma8652_suspend(struct device *dev)
{
#if 0
	struct i2c_client *client = to_i2c_client(dev);
    struct mma8652_data *pdata = i2c_get_clientdata(client);
    if(pdata->active == MMA_ACTIVED)
		mma8652_device_stop(client);
#endif
	if (gSleep_Mode_Suspend) {
		free_irq(g_mma8652_client->irq, MMA8652_NAME);
	}
	else {
		printk("mma8652_suspend,enable irq wakeup source %d\n",g_mma8652_client->irq);
		enable_irq_wake(g_mma8652_client->irq);
	}
	return 0;
}

static int mma8652_resume(struct device *dev)
{
#if 0
    int val = 0;
	struct i2c_client *client = to_i2c_client(dev);
    struct mma8652_data *pdata = i2c_get_clientdata(client);
    if(pdata->active == MMA_ACTIVED){
	   val = i2c_smbus_read_byte_data(client,MMA8X5X_CTRL_REG1);
	   i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val|0x01);  
    }
#endif
	if (gSleep_Mode_Suspend) {
		int ret; 
		mma8652_device_init(g_mma8652_client);
		ret = request_irq(g_mma8652_client->irq, mma8652_interrupt, IRQF_TRIGGER_FALLING, MMA8652_NAME, MMA8652_NAME);
		printk("request_irq returned value = %d\n", ret);
	}
	else {
		schedule_delayed_work(&mma8652_delay_work, 0);
		printk("mma8652_resume,disable irq wakeup source %d\n",g_mma8652_client->irq);
		disable_irq_wake(g_mma8652_client->irq);
	}

	return 0;
	  
}
#endif

static const struct i2c_device_id mma8652_id[] = {
	{"mma8652", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, mma8652_id);
static SIMPLE_DEV_PM_OPS(mma8652_pm_ops, mma8652_suspend, mma8652_resume);
static struct i2c_driver mma8652_driver = {
	.class  = I2C_CLASS_HWMON,
	.driver = {
		   .name = "mma8652",
		   .owner = THIS_MODULE,
		   .pm = &mma8652_pm_ops,
		   },
	.probe = mma8652_probe,
	.remove = __devexit_p(mma8652_remove),
	.id_table = mma8652_id,
	.detect = mma8652_detect,
	.address_list = normal_i2c,
};

static int __init mma8652_init(void)
{
	/* register driver */
	int res;

	res = i2c_add_driver(&mma8652_driver);
	if (res < 0) {
		printk(KERN_INFO "add mma8652 i2c driver failed\n");
		return -ENODEV;
	}
	return res;
}

static void __exit mma8652_exit(void)
{
	i2c_del_driver(&mma8652_driver);
}

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MMA8X5X 3-Axis Orientation/Motion Detection Sensor driver");
MODULE_LICENSE("GPL");

module_init(mma8652_init);
module_exit(mma8652_exit);

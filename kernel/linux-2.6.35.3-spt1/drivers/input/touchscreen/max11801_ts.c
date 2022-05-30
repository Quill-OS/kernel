/*
 * max11801_ts.c - Driver for MAXI MAX11801 - A Resistive touch screen
 * controller with i2c interface
 *
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

/* This driver is aim for support the serial of MAXI touch chips
 * max11801-max11803, the main different of this 4 chips can get from
 * this table
 * ----------------------------------------------
 * | CHIP     |	 AUTO MODE SUPPORT | INTERFACE	|
 * |--------------------------------------------|
 * | max11800 |	 YES		   |   SPI	|
 * | max11801 |	 YES		   |   I2C	|
 * | max11802 |	 NO		   |   SPI	|
 * | max11803 |	 NO		   |   I2C	|
 * ----------------------------------------------
 *
 * Currently, this driver only support max11801.
 * */

#define DEBUG 1
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <asm/bitops.h>

/* Register Address define */
#define GENERNAL_STATUS_REG		0x00
#define GENERNAL_CONF_REG		0x01
#define MESURE_RES_CONF_REG		0x02
#define MESURE_AVER_CONF_REG		0x03
#define ADC_SAMPLE_TIME_CONF_REG	0x04
#define PANEL_SETUP_TIME_CONF_REG	0x05
#define DELAY_CONVERSION_CONF_REG	0x06
#define TOUCH_DETECT_PULLUP_CONF_REG	0x07
#define AUTO_MODE_TIME_CONF_REG		0x08 /* only for max11800/max11801 */
#define APERTURE_CONF_REG		0x09 /* only for max11800/max11801 */
#define AUX_MESURE_CONF_REG		0x0a
#define OP_MODE_CONF_REG		0x0b

/* Three is a auto mode in max11800/max11801 chips, the result is put
 * in the FIFO, every read on FIFO will get on complete event, the
 * length depends on your AMODE config in OP_MODE register */
#define FIFO_RD_CMD			0x50 /* only for max11800/max11801 */

/* For max11802/max11803 only can read event from this mode. */
#define DATA_RD_CMD_START		0x52
#define DATA_RD_CMD_LEN			0x0a

#define MAX11801_POWER_OFFSET           7
#define POWER_OFF			(1 << MAX11801_POWER_OFFSET)
#define POWER_ON			(0 << MAX11801_POWER_OFFSET)
#define MAX11801_AUTOMODE_OFFSET	5
#define AUTOMODE_OFF			(0 << MAX11801_AUTOMODE_OFFSET)
#define AUTOMODE_ON_XYZ1Z2		(3 << MAX11801_AUTOMODE_OFFSET)
#define MAX11801_APERTURE_OFFSET	4
#define APERTURE_ON			(1 << MAX11801_APERTURE_OFFSET)
#define APERTURE_OFF			(0 << MAX11801_APERTURE_OFFSET)

#define MAX11801_FIFO_INT		(1 << 2)
#define MAX11801_FIFO_OVERFLOW		(1 << 3)
#define MAX11801_SCAN_STATUS		(1 << 4)
#define MAX11801_TDM_STATUS		(1 << 5)
#define MAX11801_LPM_STATUS		(1 << 6)

#define READ_BUFFER_XYZ1Z2_SIZE		8

#define MAX11801_MAX_XC			0xfff
#define MAX11801_MAX_YC			0xfff


enum {
	READ_X_POS_MSB,
	READ_X_POS_LSB,
	READ_Y_POS_MSB,
	READ_Y_POS_LSB,
	READ_Z1_POS_MSB,
	READ_Z1_POS_LSB,
	READ_Z2_POS_MSB,
	READ_Z2_POS_LSB,
};

#define FIFO_12BIT_LSB_MASK	   0xf0


#define FIFO_DATA_TAG_OFFSET		2
#define FIFO_DATA_TAG_MASK		(3 << FIFO_DATA_TAG_OFFSET)
#define FIFO_EVENT_TAG_OFFSET		0
#define FIFO_EVENT_TAG_MASK		(3 << FIFO_EVENT_TAG_OFFSET)

struct maxi_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct max11801_ts_platform_data *platform_data;
	bool auto_mode_enable;
};

struct max11801_ts_platform_data {
	bool auto_mode_enable;
	/* Enable Autonomous converion mode, only availible in max11801 and
	 * max11800 */
	bool aperture_criteria_enable;
	/* Enable aperture criteria or not if in Autonomous mode, better
	 * enable this if you can enable autonomouse mode */

	/* X & Y size of aperture criteria, the set to
	 * 2^(x/y)_scale */
	unsigned char aperture_x_scale;
	unsigned char aperture_y_scale;
};

static irqreturn_t max11801_ts_interrupt(int irq, void *dev_id)
{
	struct maxi_ts_data *data = dev_id;
	struct i2c_client *client = data->client;

	int x, y, z1, z2;
	int status;
	int err;
	int i;

	/* TODO: filo read should in a seperated function */
	u8 fifo_buffer[READ_BUFFER_XYZ1Z2_SIZE];

	printk("max11801 irq\n");

	status = i2c_smbus_read_byte_data(client, GENERNAL_STATUS_REG);
	if (status < 0) {
		dev_err(&client->dev, "Error reading status reg\n");
		return -EIO;
	}
	dev_dbg(&client->dev, "status reg: %X \n", status);
	if (status & MAX11801_FIFO_INT) {
		err = i2c_smbus_read_i2c_block_data(client, FIFO_RD_CMD,
						    READ_BUFFER_XYZ1Z2_SIZE,
						    fifo_buffer);

		for (i = 0; i < READ_BUFFER_XYZ1Z2_SIZE; i++) {
			dev_dbg(&client->dev, "FIFO %d: %X | ", i, fifo_buffer[i]);
		}

		x = (fifo_buffer[READ_X_POS_MSB] << 8) | (fifo_buffer[READ_X_POS_LSB] & FIFO_12BIT_LSB_MASK);
		y = (fifo_buffer[READ_Y_POS_MSB] << 8) | (fifo_buffer[READ_Y_POS_LSB] & FIFO_12BIT_LSB_MASK);
		z1 = (fifo_buffer[READ_Z1_POS_MSB] << 8) | (fifo_buffer[READ_Z1_POS_LSB] & FIFO_12BIT_LSB_MASK);
		z2 = (fifo_buffer[READ_Z2_POS_MSB] << 8) | (fifo_buffer[READ_Z2_POS_LSB] & FIFO_12BIT_LSB_MASK);

		dev_dbg(&client->dev, "X:%d Y:%d Z1:%d Z2:%d",
			x, y, z1, z2);
	}

	/* report Event: */

	return IRQ_HANDLED;
}

static void max11801_ts_phy_init(struct maxi_ts_data *data)
{
	const struct max11801_ts_platform_data *pdata = data->platform_data;
	struct i2c_client *client = data->client;
	int ret;
	u8 opreg, areg;

	/* Set auto mode */
	/* if (pdata->auto_mode_enable) { */
/* 		ret = i2c_smbus_write_byte_data(client, OP_MODE_CONF_REG, 0x70); */
/* 		opreg = POWER_OFF | AUTOMODE_ON_XYZ1Z2; */
/* 		if (pdata->aperture_criteria_enable) { */
/* 			opreg |= APERTURE_ON; */
/* 			areg = ((pdata->aperture_x_scale & 0xf) << 4) | (pdata->aperture_y_scale & 0xf); */
/* 			ret = i2c_smbus_write_byte_data(client, APERTURE_CONF_REG, areg); */
/* 			printk("aperture :%d rd:%X\n", ret, i2c_smbus_read_byte_data(client, APERTURE_CONF_REG)); */


/* 		} */

/* 		//ret = i2c_smbus_write_byte_data(client, OP_MODE_CONF_REG, opreg); */
/* 		//printk("operg:%d, RD:%X\n", ret, i2c_smbus_read_byte_data(client, OP_MODE_CONF_REG)); */
/* 	} */

/* 	/\* set rate *\/ */
/* 	printk("RD gen conf:%dRD:%X\n", ret, i2c_smbus_read_byte_data(client, GENERNAL_CONF_REG)); */
/* 	ret = i2c_smbus_write_byte_data(client, GENERNAL_CONF_REG, 0xF3); */
/* 	printk("gen conf:%dRD:%X\n", ret, i2c_smbus_read_byte_data(client, GENERNAL_CONF_REG)); */

/* 	/\* Power up *\/ */
/* //	ret = i2c_smbus_write_byte_data(client, OP_MODE_CONF_REG, opreg &= ~POWER_OFF); */
/* 	printk("opreg:%d RD:%X\n", ret, i2c_smbus_read_byte_data(client, OP_MODE_CONF_REG)); */
/* 	printk("status:%X\n", i2c_smbus_read_byte_data(client, GENERNAL_STATUS_REG));
 */

	i2c_smbus_write_byte_data(client, GENERNAL_CONF_REG, 0xf0);
	i2c_smbus_write_byte_data(client, 0x05, 0x11);
	i2c_smbus_write_byte_data(client, 0x07, 0x10);
	i2c_smbus_write_byte_data(client, 0x08, 0xAA);
	i2c_smbus_write_byte_data(client, 0x0b, 0x06);
	printk("rev:%d\n", i2c_smbus_read_byte_data(client, 0x0d));

}

static irqreturn_t max11801_ts_hr_irq(int irq, void *dev_id)
{
	printk("irq \n");
	return IRQ_HANDLED;
}


static int __devinit max11801_ts_probe(struct i2c_client *client,
				       const struct i2c_device_id *id)
{
	struct maxi_ts_data *data;
	struct input_dev *input_dev;
	int ret;

	printk("max11801 ts probe\n");

	if (!client->dev.platform_data)
		return -EINVAL;

	data = kzalloc(sizeof(struct maxi_ts_data), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_free_mem;
	}

	data->client = client;
	data->input_dev = input_dev;
	data->platform_data = client->dev.platform_data;

	input_dev->name = "MAXI MAX11801 Touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	input_set_abs_params(input_dev, ABS_X, 0, MAX11801_MAX_XC, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, MAX11801_MAX_YC, 0, 0);

	input_set_drvdata(input_dev, data);

	ret = request_threaded_irq(client->irq, max11801_ts_hr_irq, max11801_ts_interrupt,
				   IRQF_TRIGGER_FALLING, "max11801_ts", data);

	if (ret < 0) {
		dev_err(&client->dev, "Failed to register interrupt \n");
		goto err_free_mem;
	}

	ret = input_register_device(data->input_dev);
	if (ret < 0)
		goto err_free_irq;

	max11801_ts_phy_init(data);
	i2c_set_clientdata(client, data);

	printk("max11801 init fnished\n");

	return 0;

err_free_irq:
	free_irq(client->irq, data);
err_free_mem:
	input_free_device(input_dev);
	kfree(data);
	return ret;
}

static __devexit int max11801_ts_remove(struct i2c_client *client)
{
	struct maxi_ts_data *data = i2c_get_clientdata(client);

	free_irq(client->irq, data);
	input_unregister_device(data->input_dev);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
#define max11801_ts_suspend NULL
#define max11801_ts_resume NULL
#else
#define max11801_ts_suspend NULL
#define max11801_ts_resume NULL
#endif





static const struct i2c_device_id max11801_ts_id[] = {
	{"max11801", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, max11801_ts_id);

static struct i2c_driver max11801_ts_driver = {
	.probe		= max11801_ts_probe,
	.remove		= __devexit_p(max11801_ts_remove),
	.suspend	= max11801_ts_suspend,
	.resume		= max11801_ts_resume,
	.driver = {
		.name = "max11801_ts",
	},
	.id_table	= max11801_ts_id,
};

static int __init max11801_ts_init(void)
{
	return i2c_add_driver(&max11801_ts_driver);
}

static void __exit max11801_ts_exit(void)
{
	i2c_del_driver(&max11801_ts_driver);
}

module_init(max11801_ts_init);
module_exit(max11801_ts_exit);

/* Module information */
MODULE_AUTHOR("Jiejing Zhang <jiejing.zhang@freescale.com>");
MODULE_DESCRIPTION("Touchscreen driver for MAXI MAX11801 controller");
MODULE_LICENSE("GPL");

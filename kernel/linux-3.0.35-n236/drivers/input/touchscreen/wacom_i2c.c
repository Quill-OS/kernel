/*
 * Wacom Penabled Driver for I2C
 *
 * Copyright (c) 2011 - 2013 Tatsunosuke Tobita, Wacom.
 * <tobita.tatsunosuke@wacom.co.jp>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version of 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <asm/unaligned.h>
#include <linux/delay.h>

#include "../../../arch/arm/mach-mx6/ntx_hwconfig.h"
#include "../../../arch/arm/mach-mx6/board-mx6sl_ntx.h"

extern volatile NTX_HWCONFIG *gptHWCFG;

#define WACOM_CMD_QUERY0	0x04
#define WACOM_CMD_QUERY1	0x00
#define WACOM_CMD_QUERY2	0x33
#define WACOM_CMD_QUERY3	0x02
#define WACOM_CMD_THROW0	0x05
#define WACOM_CMD_THROW1	0x00
#define WACOM_QUERY_SIZE	19

extern int gSleep_Mode_Suspend;
static struct delayed_work wacom_delay_work;

#define FORCE_DISPLAY_RES_MAPPING		1
#define WACOM_PERFORMANCE_IMPROVE		1

struct wacom_features {
	int x_max;
	int y_max;
	int pressure_max;
	char fw_version;
};
struct wacom_features g_features;

struct wacom_i2c {
	struct i2c_client *client;
	struct input_dev *input;
	u8 data[WACOM_QUERY_SIZE];
	bool prox;
	int tool;

#ifdef FORCE_DISPLAY_RES_MAPPING//[
	unsigned long dwXRes,dwYRes;
#endif //]FORCE_DISPLAY_RES_MAPPING

#ifdef WACOM_PERFORMANCE_IMPROVE//[
	int iGotReady;
	int iIs_IRQ_disabled;
#endif //]WACOM_PERFORMANCE_IMPROVE
};

static struct wacom_i2c *g_wac_i2c;

extern void ntx_wacom_reset(bool on) ;

static int wacom_int_level(void)
{
	unsigned v;
	v = gpio_get_value(irq_to_gpio(g_wac_i2c->client->irq));
	return v?0:1;
}



static int wacom_query_device(struct i2c_client *client,
			      struct wacom_features *features)
{
	int ret;
	u8 cmd1[] = { WACOM_CMD_QUERY0, WACOM_CMD_QUERY1,
			WACOM_CMD_QUERY2, WACOM_CMD_QUERY3 };
	u8 cmd2[] = { WACOM_CMD_THROW0, WACOM_CMD_THROW1 };
	u8 data[WACOM_QUERY_SIZE];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd1),
			.buf = cmd1,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd2),
			.buf = cmd2,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(data),
			.buf = data,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)  {
		printk ("[%s-%d] return %d\n",__func__,__LINE__,ret);
		return ret;
	}
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;


#if 0
	features->x_max = get_unaligned_le16(&data[3]);
	features->y_max = get_unaligned_le16(&data[5]);
#else
	features->y_max = get_unaligned_le16(&data[3]);
	features->x_max = get_unaligned_le16(&data[5]);
#endif
	features->pressure_max = get_unaligned_le16(&data[11]);
	features->fw_version = get_unaligned_le16(&data[13]);
	printk ("[%s-%d] max_x=%d ,max_y=%d\n", __func__, __LINE__, features->x_max, features->y_max);

	dev_dbg(&client->dev,
		"x_max:%d, y_max:%d, pressure:%d, fw:%d\n",
		features->x_max, features->y_max,
		features->pressure_max, features->fw_version);

	return 0;
}


static void wacom_work_func(struct work_struct *work)
{
	struct input_dev *input = g_wac_i2c->input;
	u8 *data = g_wac_i2c->data;
	unsigned int x, y, pressure;
	unsigned int wac_x, wac_y;
	unsigned char tsw, f1, f2, ers;
	int error;
	unsigned char inv,rdy;

	

	error = i2c_master_recv(g_wac_i2c->client,
				g_wac_i2c->data, sizeof(g_wac_i2c->data));
	if (error < 0)
		goto out;

	tsw = data[3] & 0x01;
	ers = data[3] & 0x04;
	f1 = data[3] & 0x02;
	f2 = data[3] & 0x10;
	inv = data[3] & 0x03;
	rdy = data[3] & 0x20;

#ifdef WACOM_PERFORMANCE_IMPROVE//[
	if(rdy) {
		g_wac_i2c->iGotReady = 1;
		if(!g_wac_i2c->iIs_IRQ_disabled) {
			disable_irq_nosync(g_wac_i2c->client->irq);g_wac_i2c->iIs_IRQ_disabled=1;
		}
	}
	else {
		g_wac_i2c->iGotReady = 0;
		if(g_wac_i2c->iIs_IRQ_disabled) {
			enable_irq(g_wac_i2c->client->irq);g_wac_i2c->iIs_IRQ_disabled=0;
		}
	}
#endif //]WACOM_PERFORMANCE_IMPROVE

	wac_y = le16_to_cpup((__le16 *)&data[4]);
	wac_x = le16_to_cpup((__le16 *)&data[6]);

#ifdef FORCE_DISPLAY_RES_MAPPING//[
	#if 1
	{
		// ebrmain .
		y = (g_features.y_max-wac_y) * g_wac_i2c->dwYRes/g_features.y_max;
		x = (wac_x) * g_wac_i2c->dwXRes/g_features.x_max;
	}
	#else
	{
		// android
		y = (wac_y) * g_wac_i2c->dwXRes/g_features.x_max;
		x = (g_features.x_max-wac_x) * g_wac_i2c->dwXRes/g_features.x_max;
	}
	#endif

#else //][!FORCE_DISPLAY_RES_MAPPING
	{
		// 
		#if 0
		y = wac_y;
		x = wac_x;
		#else
		y = g_features.y_max-wac_y;
		x = wac_x;
		#endif
	}
#endif //] FORCE_DISPLAY_RES_MAPPING

	pressure = le16_to_cpup((__le16 *)&data[8]);

	if (!g_wac_i2c->prox)
		g_wac_i2c->tool = (data[3] & 0x0c) ?
			BTN_TOOL_RUBBER : BTN_TOOL_PEN;

	g_wac_i2c->prox = data[3] & 0x20;

//	printk ("[%s-%d] stylus %02X, (%d, %d) p=%d\n", __func__, __LINE__, data[3], x,y,pressure);
	input_report_key(input, BTN_TOUCH, tsw || ers);
	input_report_key(input, g_wac_i2c->tool, g_wac_i2c->prox);
	input_report_key(input, BTN_STYLUS, f1);
	input_report_key(input, BTN_STYLUS2, f2);
	input_report_abs(input, ABS_X, x);
	input_report_abs(input, ABS_Y, y);
	input_report_abs(input, ABS_PRESSURE, pressure);

#if 0
	printk("%s :(%d,%d,%d),tsw=%d,ssw1=%d,ssw2=%d,esr=%d,inv=%d,rdy=%d,wac_x=%d,wac_y=%d\n",
			__func__,(int)x,(int)y,(int)pressure,
			tsw?1:0,f1?1:0,f2?1:0,ers?1:0,inv?1:0,rdy?1:0,
			(int)wac_x,(int)wac_y);
#endif

	input_sync(input);

out:
#ifdef WACOM_PERFORMANCE_IMPROVE//[
	if ( wacom_int_level() || g_wac_i2c->iGotReady ) {
		schedule_delayed_work(&wacom_delay_work, 0);
	}
#else//][!WACOM_PERFORMANCE_IMPROVE
	if ( wacom_int_level() ) 
		schedule_delayed_work(&wacom_delay_work, 1);
#endif //]WACOM_PERFORMANCE_IMPROVE
}

static irqreturn_t wacom_i2c_irq(int irq, void *dev_id)
{
	schedule_delayed_work(&wacom_delay_work, 0);
	return IRQ_HANDLED;
}

static int wacom_i2c_open(struct input_dev *dev)
{
	printk("%s()\n",__func__);
	enable_irq(g_wac_i2c->client->irq);
	ntx_wacom_reset(1);
	return 0;
}

static void wacom_i2c_close(struct input_dev *dev)
{
	struct i2c_client *client = g_wac_i2c->client;
	printk("%s()\n",__func__);

	disable_irq(client->irq);
	ntx_wacom_reset(0);
}

static int wacom_i2c_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct input_dev *input;
	int error;

	msleep(10);
	ntx_wacom_reset(1);
	msleep(10);


	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}

	error = wacom_query_device(client, &g_features);
	if (error)
		return error;

	g_wac_i2c = kzalloc(sizeof(*g_wac_i2c), GFP_KERNEL);
	input = input_allocate_device();
	if (!g_wac_i2c || !input) {
		error = -ENOMEM;
		goto err_free_mem;
	}



	g_wac_i2c->client = client;
	g_wac_i2c->input = input;
	if(1==gptHWCFG->m_val.bDisplayResolution) {
		// 1024x758 .
		g_wac_i2c->dwXRes=1024;
		g_wac_i2c->dwYRes=758;
	}
	else if(2==gptHWCFG->m_val.bDisplayResolution) {
		// 1024x768
		g_wac_i2c->dwXRes=1024;
		g_wac_i2c->dwYRes=768;
	}
	else if(3==gptHWCFG->m_val.bDisplayResolution) {
		// 1440x1080
		g_wac_i2c->dwXRes=1440;
		g_wac_i2c->dwYRes=1080;
	}
	else if(4==gptHWCFG->m_val.bDisplayResolution) {
		// 1366x768
		g_wac_i2c->dwXRes=1366;
		g_wac_i2c->dwYRes=768;
	}
	else if(5==gptHWCFG->m_val.bDisplayResolution) {
		// 1448x1072
		g_wac_i2c->dwXRes=1448;
		g_wac_i2c->dwYRes=1072;
	}
	else if(6==gptHWCFG->m_val.bDisplayResolution) {
		// 1600x1200
		g_wac_i2c->dwXRes=1600;
		g_wac_i2c->dwYRes=1200;
	}
	else if(8==gptHWCFG->m_val.bDisplayResolution) {
		// 1872x1404
		g_wac_i2c->dwXRes=1872;
		g_wac_i2c->dwYRes=1404;
	}
	else {
		// 800x600 
		g_wac_i2c->dwXRes=800;
		g_wac_i2c->dwYRes=600;
	}

	input->name = "Wacom I2C Digitizer";
	input->id.bustype = BUS_I2C;
	input->id.vendor = 0x56a;
	input->id.version = g_features.fw_version;
	input->dev.parent = &client->dev;
	input->open = wacom_i2c_open;
	input->close = wacom_i2c_close;

	input->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

	__set_bit(BTN_TOOL_PEN, input->keybit);
	__set_bit(BTN_TOOL_RUBBER, input->keybit);
	__set_bit(BTN_STYLUS, input->keybit);
	__set_bit(BTN_STYLUS2, input->keybit);
	__set_bit(BTN_TOUCH, input->keybit);
#ifdef FORCE_DISPLAY_RES_MAPPING//[
	input_set_abs_params(input, ABS_X, 0, g_wac_i2c->dwXRes, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, g_wac_i2c->dwYRes, 0, 0);
#else //][FORCE_DISPLAY_RES_MAPPING
	input_set_abs_params(input, ABS_X, 0, g_features.x_max, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, g_features.y_max, 0, 0);
#endif //]FORCE_DISPLAY_RES_MAPPING
	input_set_abs_params(input, ABS_PRESSURE,
			     0, g_features.pressure_max, 0, 0);

	input_set_drvdata(input, g_wac_i2c);

	INIT_DELAYED_WORK(&wacom_delay_work, wacom_work_func);

	error = request_irq(client->irq, wacom_i2c_irq, IRQF_TRIGGER_FALLING, "wacom_i2c", g_wac_i2c);
	if (error) {
		dev_err(&client->dev,
			"Failed to enable IRQ, error: %d\n", error);
		cancel_delayed_work_sync (&wacom_delay_work);
		goto err_free_mem;
	}

	/* Disable the IRQ, we'll enable it in wac_i2c_open() */
	disable_irq(client->irq);

	error = input_register_device(g_wac_i2c->input);
	if (error) {
		dev_err(&client->dev,
			"Failed to register input device, error: %d\n", error);
		goto err_free_irq;
	}

	i2c_set_clientdata(client, g_wac_i2c);
	return 0;

err_free_irq:
	free_irq(client->irq, g_wac_i2c);
err_free_mem:
	input_free_device(input);
	kfree(g_wac_i2c);

	return error;
}

static int wacom_i2c_remove(struct i2c_client *client)
{
	free_irq(client->irq, g_wac_i2c);
	input_unregister_device(g_wac_i2c->input);
	kfree(g_wac_i2c);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wacom_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	/* wacom wants to send data, trigger a read */
	if ( wacom_int_level() ) 
	{
		printk ("[%s-%d] KL25 event not processed.\n",__func__,__LINE__);
		schedule_delayed_work(&wacom_delay_work, 0);
		return -1;
	}

	if (gSleep_Mode_Suspend) {
		free_irq(client->irq, 0);
	}
	else {
		enable_irq_wake(client->irq);
	}

	return 0;
}

static int wacom_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (gSleep_Mode_Suspend) {
		request_irq(client->irq, wacom_i2c_irq, IRQF_TRIGGER_FALLING, "wacom_i2c", g_wac_i2c);
	}
	else {
		disable_irq_wake(client->irq);
	}
	
	if ( wacom_int_level() ) 
	{
		schedule_delayed_work(&wacom_delay_work, 0);
	}
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(wacom_i2c_pm, wacom_i2c_suspend, wacom_i2c_resume);

static const struct i2c_device_id wacom_i2c_id[] = {
	{ "wacom_i2c", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, wacom_i2c_id);

static struct i2c_driver wacom_i2c_driver = {
	.driver	= {
		.name	= "wacom_i2c",
		.owner	= THIS_MODULE,
		.pm	= &wacom_i2c_pm,
	},

	.probe		= wacom_i2c_probe,
	.remove		= wacom_i2c_remove,
	.id_table	= wacom_i2c_id,
};

static int __init wacom_init(void)
{
	return i2c_add_driver(&wacom_i2c_driver);
}

static void __exit wacom_exit(void)
{
	i2c_del_driver(&wacom_i2c_driver);
}

module_init(wacom_init);
module_exit(wacom_exit);

MODULE_AUTHOR("Tatsunosuke Tobita <tobita.tatsunosuke@wacom.co.jp>");
MODULE_DESCRIPTION("WACOM EMR I2C Driver");
MODULE_LICENSE("GPL");

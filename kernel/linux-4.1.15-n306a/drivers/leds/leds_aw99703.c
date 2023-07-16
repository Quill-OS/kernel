/*
* aw99703.c   aw99703 backlight module
*
* Version: v1.0.3
*
* Copyright (c) 2019 AWINIC Technology CO., LTD
*
*  Author: Joseph <zhangzetao@awinic.com.cn>
*
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
*/

#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/leds_aw99703.h>

#include "../../arch/arm/mach-imx/ntx_firmware.h"
#include "../../arch/arm/mach-imx/ntx_hwconfig.h"

#define AW99703_LED_DEV "aw99703-bl"
#define AW99703_NAME "aw99703-bl"

#define AW99703_VERSION "v1.0.3"

struct aw99703_data *g_aw99703_data;

static struct aw99703_data *gpled[2];
static int g_aw99703_leds;

extern NTX_FW_AW99703FL_DUALFL_percent_tab *gptAw99703fl_dualcolor_percent_tab; 
extern NTX_FW_AW99703FL_dualcolor_hdr *gptAw99703fl_dualcolor_tab_hdr; 
extern volatile NTX_HWCONFIG *gptHWCFG;

extern int gSleep_Mode_Suspend;
static volatile int giFL_PWR_state;
static volatile int giFL_PWR_state_max;
static int fl_en_status = 0;
static int giSuspend = 0;

int fl_aw99703_x2_percentage (int iFL_Percentage);
int fl_aw99703_current(struct aw99703_data *drvdata, unsigned char led_current);

static void aw99703_ramp_setting(struct aw99703_data *drvdata);

static int platform_read_i2c_block(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}

static int aw99703_i2c_read(struct i2c_client *client, u8 addr, u8 *val)
{
	return platform_read_i2c_block(client, &addr, 1, val, 1);
}

static int platform_write_i2c_block(struct i2c_client *client,
		char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);

	return ret;
}

static int aw99703_i2c_write(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return platform_write_i2c_block(client, buf, sizeof(buf));
}

static ssize_t led_max_color_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int iColors;
	if (gptAw99703fl_dualcolor_percent_tab) {
		iColors = (int)gptAw99703fl_dualcolor_tab_hdr->dwTotalColors ;
	}
	else {
		iColors = 1 ;
	}
	sprintf (buf, "%d", (iColors-1));
	return strlen(buf);
}

static ssize_t led_max_color_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	return count;
}

static ssize_t led_color_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int val = 0 , i ;
	struct led_classdev *pLedclass = dev_get_drvdata(dev);

	for(i=0 ; i< g_aw99703_leds ; i++)
	{
		if(strcmp(gpled[i]->label, pLedclass->name) == 0){
			sprintf (buf, "%d", gpled[i]->frontlight_table);
		}
	}

	return strlen(buf);
}

static ssize_t led_color_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int val = simple_strtoul(buf, NULL, 10);
	int i=0,select_i;
	struct led_classdev *pLedclass = dev_get_drvdata(dev);

	for(i=0 ; i< g_aw99703_leds ; i++)
	{
		if(strcmp(gpled[i]->label,pLedclass->name) == 0){
			select_i = i;
		}
		gpled[i]->frontlight_table = val;
	}
	fl_aw99703_x2_percentage(gpled[select_i]->percentage);

	return count;
}

static ssize_t led_bl_power_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	struct led_classdev *pLedclass = dev_get_drvdata(dev);
	
	for(i=0 ; i< g_aw99703_leds ; i++)
	{
		if(strcmp(gpled[i]->label, pLedclass->name) == 0){
			sprintf (buf, "%d", gpled[i]->full_scale_led);
		}
	}
	
	return strlen(buf);
}

static ssize_t led_bl_power_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned char val = simple_strtoul(buf, NULL, 10);
	int i, select_i = 0;
	struct led_classdev *pLedclass = dev_get_drvdata(dev);
	
	for(i=0 ; i< g_aw99703_leds ; i++)
	{
		if(strcmp(gpled[i]->label, pLedclass->name) == 0){
			select_i = i;
		}
	}
	
	fl_aw99703_current(gpled[select_i], val);
	gpled[select_i]->full_scale_led = val;
	
	return count;
}

static ssize_t led_ramp_on_get(struct device *dev, struct device_attribute *attr,char *buf)
{
	int i;
	struct led_classdev *pLedclass = dev_get_drvdata(dev);
	
	for(i=0 ; i< g_aw99703_leds ; i++)
	{
		if(strcmp(gpled[i]->label, pLedclass->name) == 0){
			sprintf (buf, "%d", gpled[i]->ramp_on_time);
		}
	}
	
	return strlen(buf);
}
static ssize_t led_ramp_on_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	unsigned char val = simple_strtoul(buf, NULL, 10);
	int i, select_i = 0;
	struct led_classdev *pLedclass = dev_get_drvdata(dev);
	
	for(i=0 ; i< g_aw99703_leds ; i++)
	{
		if(strcmp(gpled[i]->label, pLedclass->name) == 0){
			select_i = i;
		}
	}
	
	gpled[select_i]->ramp_on_time = val;
	aw99703_ramp_setting(gpled[select_i]);
	return count;
}
static ssize_t led_ramp_off_get(struct device *dev, struct device_attribute *attr,char *buf)
{
	int i;
	struct led_classdev *pLedclass = dev_get_drvdata(dev);
	
	for(i=0 ; i< g_aw99703_leds ; i++)
	{
		if(strcmp(gpled[i]->label, pLedclass->name) == 0){
			sprintf (buf, "%d", gpled[i]->ramp_off_time);
		}
	}
	
	return strlen(buf);
}
static ssize_t led_ramp_off_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	unsigned char val = simple_strtoul(buf, NULL, 10);
	int i, select_i = 0;
	struct led_classdev *pLedclass = dev_get_drvdata(dev);
	
	for(i=0 ; i< g_aw99703_leds ; i++)
	{
		if(strcmp(gpled[i]->label, pLedclass->name) == 0){
			select_i = i;
		}
	}

	gpled[select_i]->ramp_off_time = val;
	aw99703_ramp_setting(gpled[select_i]);
	return count;
}

static void aw99703_hwen_pin_ctrl(struct aw99703_data *drvdata, int en)
{
	if (gpio_is_valid(drvdata->hwen_gpio)) {
		if (en) {
			pr_info("hwen pin is going to be high!---<%d>\n", en);
			gpio_set_value(drvdata->hwen_gpio, true);
			usleep_range(3500, 4000);
			fl_en_status = 1;
		} else {
			pr_info("hwen pin is going to be low!---<%d>\n", en);
			gpio_set_value(drvdata->hwen_gpio, false);
			usleep_range(1000, 2000);
		}
	}
}

static int aw99703_gpio_init(struct aw99703_data *drvdata)
{

	int ret;
	
	if (g_aw99703_leds == 0) {
		if (drvdata->hwen_gpio > 0) {
			if (gpio_is_valid(drvdata->hwen_gpio)) {
				ret = gpio_request(drvdata->hwen_gpio, "hwen_gpio");
				if (ret < 0) {
					pr_err("failed to request gpio\n");
					return -1;
				}

				ret = gpio_direction_output(drvdata->hwen_gpio, 0);
				pr_info("request gpio init\n");
				if (ret < 0) {
					pr_err("failed to set output\n");
					gpio_free(drvdata->hwen_gpio);
					return ret;
				}
				aw99703_hwen_pin_ctrl(drvdata, 1);
			}
		}
	} else {
		drvdata->hwen_gpio = gpled[0]->hwen_gpio;
	}
	
	return 0;
}

static int aw99703_i2c_write_bit(struct i2c_client *client,
	unsigned int reg_addr, unsigned int  mask, unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw99703_i2c_read(client, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw99703_i2c_write(client, reg_addr, reg_val);

	return 0;
}

static int aw99703_brightness_map(unsigned int bl_map, unsigned int level)
{
	/*MAX_LEVEL_256*/
	if (bl_map == 1) {
		if (level == 255)
			return 2047;
		return level * 8;
	}
	/*MAX_LEVEL_1024*/
	if (bl_map == 2)
		return level * 2;
	/*MAX_LEVEL_2048*/
	if (bl_map == 3)
		return level;

	return  level;
}

static int aw99703_bl_enable_channel(struct aw99703_data *drvdata)
{
	int ret = 0;

	if (drvdata->channel == 3) {
		pr_info("%s turn all channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH3_ENABLE |
						AW99703_LEDCUR_CH2_ENABLE |
						AW99703_LEDCUR_CH1_ENABLE);
	} else if (drvdata->channel == 2) {
		pr_info("%s turn two channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH2_ENABLE |
						AW99703_LEDCUR_CH1_ENABLE);
	} else if (drvdata->channel == 1) {
		pr_info("%s turn one channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH1_ENABLE);
	} else {
		pr_info("%s all channels are going to be disabled\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						0x98);
	}

	return ret;
}

static void aw99703_pwm_mode_enable(struct aw99703_data *drvdata)
{
	if (drvdata->pwm_mode) {
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_PDIS_MASK,
					AW99703_MODE_PDIS_ENABLE);
		pr_info("%s pwm_mode is enable\n", __func__);
	} else {
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_PDIS_MASK,
					AW99703_MODE_PDIS_DISABLE);
		pr_info("%s pwm_mode is disable\n", __func__);
	}
}

static void aw99703_ramp_setting(struct aw99703_data *drvdata)
{
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TURNCFG,
				AW99703_TURNCFG_ON_TIM_MASK,
				drvdata->ramp_on_time << 4);
	pr_info("%s drvdata->ramp_on_time is 0x%x\n",
		__func__, drvdata->ramp_on_time);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TURNCFG,
				AW99703_TURNCFG_OFF_TIM_MASK,
				drvdata->ramp_off_time);
	pr_info("%s drvdata->ramp_off_time is 0x%x\n",
		__func__, drvdata->ramp_off_time);

}
static void aw99703_transition_ramp(struct aw99703_data *drvdata)
{

	pr_info("%s enter\n", __func__);
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TRANCFG,
				AW99703_TRANCFG_PWM_TIM_MASK,
				drvdata->pwm_trans_dim);
	pr_info("%s drvdata->pwm_trans_dim is 0x%x\n", __func__,
		drvdata->pwm_trans_dim);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TRANCFG,
				AW99703_TRANCFG_I2C_TIM_MASK,
				drvdata->i2c_trans_dim);
	pr_info("%s drvdata->i2c_trans_dim is 0x%x\n",
		__func__, drvdata->i2c_trans_dim);

}

static int aw99703_backlight_init(struct aw99703_data *drvdata)
{
	pr_info("%s enter.\n", __func__);

	aw99703_pwm_mode_enable(drvdata);

	/*mode:map type*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_MODE,
				AW99703_MODE_MAP_MASK,
				AW99703_MODE_MAP_LINEAR);

	/*default OVPSEL 38V*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_OVPSEL_MASK,
				AW99703_BSTCTR1_OVPSEL_38V);

	/*switch frequency 1000kHz*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_SF_MASK,
				AW99703_BSTCTR1_SF_1000KHZ);

	/*OCP SELECT*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_OCPSEL_MASK,
				AW99703_BSTCTR1_OCPSEL_3P3A);

	/*BSTCRT2 IDCTSEL*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR2,
				AW99703_BSTCTR2_IDCTSEL_MASK,
				AW99703_BSTCTR2_IDCTSEL_10UH);

	/*Backlight current full scale*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_LEDCUR,
				AW99703_LEDCUR_BLFS_MASK,
				drvdata->full_scale_led << 3);

	aw99703_bl_enable_channel(drvdata);

	aw99703_ramp_setting(drvdata);
	aw99703_transition_ramp(drvdata);

	return 0;
}

static int aw99703_backlight_enable(struct aw99703_data *drvdata)
{
	pr_info("%s enter.\n", __func__);

	/*aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_MODE,
				AW99703_MODE_WORKMODE_MASK,
				AW99703_MODE_WORKMODE_BACKLIGHT);*/

	drvdata->enable = true;

	return 0;
}

int  aw99703_set_cur_brightness(struct aw99703_data *drvdata, unsigned char led_current, int brt_val)
{
	pr_info("%s brt_val is %d\n", __func__, brt_val);

	if (drvdata->enable == false) {
		aw99703_backlight_init(drvdata);
		aw99703_backlight_enable(drvdata);
	}

	brt_val = aw99703_brightness_map(drvdata->bl_map, brt_val);
	
	/* set bacjlight current */
	aw99703_i2c_write_bit(drvdata->client,
			AW99703_REG_LEDCUR,
			AW99703_LEDCUR_BLFS_MASK,
			led_current << 3);
		
	/* set backlight brt_val */
	aw99703_i2c_write(drvdata->client,
			AW99703_REG_LEDLSB,
			brt_val&0x0007);
	aw99703_i2c_write(drvdata->client,
			AW99703_REG_LEDMSB,
			(brt_val >> 3)&0xff);

	if (brt_val > 0) {
		/*enalbe bl mode*/
		/* backlight enable */
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_BACKLIGHT);
	} else {
		/* standby mode*/
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_STANDBY);
	}

	drvdata->brightness = brt_val;

	if (drvdata->brightness == 0)
		drvdata->enable = false;

	return 0;
}

int  aw99703_set_cur_brightness_ex(struct aw99703_data *drvdata, unsigned char led_current, int brt_val)
{
	pr_info("%s brt_val is %d\n", __func__, brt_val);

	if (drvdata->enable == false) {
		aw99703_backlight_init(drvdata);
		aw99703_backlight_enable(drvdata);
	}

	brt_val = aw99703_brightness_map(drvdata->bl_map, brt_val);
	

	// brightness decline
#if 0
	if(drvdata->brightness > brt_val && abs(drvdata->brightness - brt_val)>500)
	{
		/* set backlight brt_val */
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDLSB,
				brt_val&0x0007);
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDMSB,
				(brt_val >> 3)&0xff);

		msleep(500);
		
		/* set backlight current */
		aw99703_i2c_write_bit(drvdata->client,
			AW99703_REG_LEDCUR,
			AW99703_LEDCUR_BLFS_MASK,
			led_current << 3);

		printk(KERN_ERR"[Decline] sleep 500 ms , (%d_%d)\n",drvdata->brightness,brt_val);
	}
	else if(brt_val > drvdata->brightness  && abs(drvdata->brightness - brt_val)>500)
	{

		/* set backlight current */
		aw99703_i2c_write_bit(drvdata->client,
			AW99703_REG_LEDCUR,
			AW99703_LEDCUR_BLFS_MASK,
			led_current << 3);
		msleep(500);
		
		/* set backlight brt_val */
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDLSB,
				brt_val&0x0007);
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDMSB,
				(brt_val >> 3)&0xff);

		printk(KERN_ERR"[ascent] sleep 500 ms , (%d_%d)\n",drvdata->brightness,brt_val);
	}
	else
#endif
	{
		/* set backlight current */
		aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_LEDCUR,
				AW99703_LEDCUR_BLFS_MASK,
				led_current << 3);
			
		/* set backlight brt_val */
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDLSB,
				brt_val&0x0007);
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDMSB,
				(brt_val >> 3)&0xff);
	}

	if (brt_val > 0) {
		/*enalbe bl mode*/
		/* backlight enable */
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_BACKLIGHT);
	} else {
		/* standby mode*/
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_STANDBY);
	}

	drvdata->brightness = brt_val;

	if (drvdata->brightness == 0)
		drvdata->enable = false;
	
	return 0;
}

int  aw99703_set_brightness(struct aw99703_data *drvdata, int brt_val)
{
	pr_info("%s brt_val is %d\n", __func__, brt_val);
 
	if (drvdata->enable == false) {
		aw99703_backlight_init(drvdata);
		aw99703_backlight_enable(drvdata);
	}

	brt_val = aw99703_brightness_map(drvdata->bl_map, brt_val);
	
	/* set backlight brt_val */
	aw99703_i2c_write(drvdata->client,
			AW99703_REG_LEDLSB,
			brt_val&0x0007);
	aw99703_i2c_write(drvdata->client,
			AW99703_REG_LEDMSB,
			(brt_val >> 3)&0xff);
	
	if (brt_val > 0) {
		/*enalbe bl mode*/
		/* backlight enable */
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_BACKLIGHT);
	} else {
		/* standby mode*/
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_STANDBY);
	}

	drvdata->brightness = brt_val;

	if (drvdata->brightness == 0)
		drvdata->enable = false;
	return 0;
}

int aw99703_get_FL_current(void)
{
	int iRet = -1;
	unsigned long dwCurrent=0;

	unsigned long *pdwCurrTab;
	unsigned long dwCurrTabSize;
	int iTabIdx;

	if(0x0c==gptHWCFG->m_val.bFL_PWM)
	{
		int iColor = gpled[0]->frontlight_table;
		int iPercentIdx=gpled[0]->percentage-1;

		if(gptAw99703fl_dualcolor_percent_tab){
			if(iPercentIdx>=0) {
				dwCurrent = gptAw99703fl_dualcolor_percent_tab[iColor].dwCurrentA[iPercentIdx];
				iRet = dwCurrent;
			}
			else{
				iRet = 0;
			}
		}
	}

	if(iRet<0) {
		printk(KERN_ERR"%s():curr table not avalible(%d) !!\n",__FUNCTION__,iRet);
	}

	return iRet;
}

int fl_aw99703_current(struct aw99703_data *drvdata, unsigned char led_current)
{
	if (led_current > 0x1f || led_current < 0) {
		printk(KERN_ERR"[%s_%d] led_current is out of range !!");
		return -1 ;
	}
	
	if (drvdata->enable == false) {
		aw99703_backlight_init(drvdata);
		aw99703_backlight_enable(drvdata);
	}
	
	aw99703_i2c_write_bit(drvdata->client,
			AW99703_REG_LEDCUR,
			AW99703_LEDCUR_BLFS_MASK,
			led_current << 3);
			
	return 0;
}

int fl_aw99703_x2_percentage (int iFL_Percentage)
{
	if( iFL_Percentage > 100 || iFL_Percentage < 0)
	{
		printk(KERN_ERR"[%s_%d] iFL_Percentage is out of range !!");
		return -1 ;
	}
	
	if(0x0c == gptHWCFG->m_val.bFL_PWM) /* FL PWM is AW99703 */
	{
		int iColor = gpled[0]->frontlight_table;
		unsigned char led_A_current = 0, led_B_current = 0;
		if(gptAw99703fl_dualcolor_percent_tab){
			if(gptAw99703fl_dualcolor_percent_tab[iColor].bC1_CurrentA[iFL_Percentage - 1] != 0) {
				led_A_current = gptAw99703fl_dualcolor_percent_tab[iColor].bC1_CurrentA[iFL_Percentage - 1];
			}
			else if(gptAw99703fl_dualcolor_percent_tab[iColor].bDefaultC1_Current != 0) {
				led_A_current = gptAw99703fl_dualcolor_percent_tab[iColor].bDefaultC1_Current;
			}
			else if(gptAw99703fl_dualcolor_tab_hdr->bDefaultC1_Current != 0) {
				led_A_current = gptAw99703fl_dualcolor_tab_hdr->bDefaultC1_Current;
			}
			
			if(gptAw99703fl_dualcolor_percent_tab[iColor].bC2_CurrentA[iFL_Percentage - 1] != 0) {
				led_B_current = gptAw99703fl_dualcolor_percent_tab[iColor].bC2_CurrentA[iFL_Percentage - 1];
			}
			else if(gptAw99703fl_dualcolor_percent_tab[iColor].bDefaultC2_Current != 0) {
				led_B_current = gptAw99703fl_dualcolor_percent_tab[iColor].bDefaultC2_Current;
			}
			else if(gptAw99703fl_dualcolor_tab_hdr->bDefaultC2_Current != 0) {
				led_B_current = gptAw99703fl_dualcolor_tab_hdr->bDefaultC2_Current;
			}
			
			if( 0 == iFL_Percentage){
				aw99703_set_cur_brightness(gpled[0], 0, 0);
				aw99703_set_cur_brightness(gpled[1], 0, 0);
			}
			else{
				if( gptHWCFG->m_val.bPCB == 0x62 || gptHWCFG->m_val.bPCB == 0x65 ) // E80P04 / E70K14
				{
					aw99703_set_cur_brightness_ex(gpled[0], led_A_current, gptAw99703fl_dualcolor_percent_tab[iColor].bC1_BrightnessA[iFL_Percentage-1]);
					aw99703_set_cur_brightness_ex(gpled[1], led_B_current, gptAw99703fl_dualcolor_percent_tab[iColor].bC2_BrightnessA[iFL_Percentage-1]);
				}
				else{
					aw99703_set_cur_brightness(gpled[0], led_A_current, gptAw99703fl_dualcolor_percent_tab[iColor].bC1_BrightnessA[iFL_Percentage-1]);
					aw99703_set_cur_brightness(gpled[1], led_B_current, gptAw99703fl_dualcolor_percent_tab[iColor].bC2_BrightnessA[iFL_Percentage-1]);
				}
				printk(KERN_INFO"[AW99703] iColor:%d Percentage:%d , A:(%d)%d , B:(%d)%d \n",iColor,iFL_Percentage,
					led_A_current, gptAw99703fl_dualcolor_percent_tab[iColor].bC1_BrightnessA[iFL_Percentage-1],
					led_B_current, gptAw99703fl_dualcolor_percent_tab[iColor].bC2_BrightnessA[iFL_Percentage-1]);
			}
			gpled[0]->percentage = iFL_Percentage;
			gpled[1]->percentage = iFL_Percentage;
		}
	}
	
	return 0;
}

#ifdef KERNEL_ABOVE_4_14
static int aw99703_bl_get_brightness(struct backlight_device *bl_dev)
{
		return bl_dev->props.brightness;
}

static int aw99703_bl_update_status(struct backlight_device *bl_dev)
{
		struct aw99703_data *drvdata = bl_get_data(bl_dev);
		int brt;

		if (bl_dev->props.state & BL_CORE_SUSPENDED)
				bl_dev->props.brightness = 0;

		brt = bl_dev->props.brightness;
		/*
		 * Brightness register should always be written
		 * not only register based mode but also in PWM mode.
		 */
		return aw99703_set_brightness(drvdata, brt);
}

static const struct backlight_ops aw99703_bl_ops = {
		.update_status = aw99703_bl_update_status,
		.get_brightness = aw99703_bl_get_brightness,
};
#endif

static int aw99703_read_chipid(struct aw99703_data *drvdata)
{
	int ret = -1;
	u8 value = 0;
	unsigned char cnt = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw99703_i2c_read(drvdata->client, 0x00, &value);
		if (ret < 0) {
			pr_err("%s: failed to read reg AW99703_REG_ID: %d\n",
				__func__, ret);
		}
		switch (value) {
		case 0x03:
			pr_info("%s aw99703 detected\n", __func__);
			return 0;
		default:
			pr_info("%s unsupported device revision (0x%x)\n",
				__func__, value);
			break;
		}
		cnt++;

		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

int chargepump_set_backlight_level(unsigned int level)
{

	pr_info("%s aw99703_brt_level is %d \n",__func__, level);

	if (g_aw99703_data->enable == false) {
		aw99703_backlight_init(g_aw99703_data);
		aw99703_backlight_enable(g_aw99703_data);
	}

	level = aw99703_brightness_map(g_aw99703_data->bl_map, level);
    
	/* set backlight level */
	aw99703_i2c_write(g_aw99703_data->client,
			AW99703_REG_LEDLSB,
			level&0x0007);
	aw99703_i2c_write(g_aw99703_data->client,
			AW99703_REG_LEDMSB,
			(level >> 3)&0xff);

	if (level > 0) {
		/*enalbe bl mode*/
		/* backlight enable */
		aw99703_i2c_write_bit(g_aw99703_data->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_BACKLIGHT);
	} else {
		/* standby mode*/
		aw99703_i2c_write_bit(g_aw99703_data->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_STANDBY);
	}

	g_aw99703_data->brightness = level;

	if (g_aw99703_data->brightness == 0)
		g_aw99703_data->enable = false;

	return 0;
}

static void __aw99703_work(struct aw99703_data *led,
				enum led_brightness value)
{
	mutex_lock(&led->lock);
	aw99703_set_brightness(led, value);
	mutex_unlock(&led->lock);
}

static void aw99703_work(struct work_struct *work)
{
	struct aw99703_data *drvdata = container_of(work,
					struct aw99703_data, work);

	__aw99703_work(drvdata, drvdata->led_dev.brightness);
}


static void aw99703_brightness_set(struct led_classdev *led_cdev,
			enum led_brightness brt_val)
{
	struct aw99703_data *drvdata;

	drvdata = container_of(led_cdev, struct aw99703_data, led_dev);
	schedule_work(&drvdata->work);
}

static int aw99703_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw99703_data *drvdata = i2c_get_clientdata(client);

	printk(KERN_INFO"[%s_%d] gSleep_Mode_Suspend:%d \n", __FUNCTION__, __LINE__, gSleep_Mode_Suspend);

	mutex_lock(&drvdata->lock);
	if (gSleep_Mode_Suspend) {
		if(--giFL_PWR_state <= 0) {
			if(giFL_PWR_state < 0) {
				printk(KERN_WARNING"%s():[WARNING] FL PWR state <0 !!\n",__FUNCTION__);
				giFL_PWR_state = 0;
			}
					
			if (0 != fl_en_status) {
				if(gpio_is_valid(drvdata->hwen_gpio)) {
					printk("%s():FL PWR[OFF] ,drvdata->hwen_gpio:%d \n", __FUNCTION__, drvdata->hwen_gpio);
					gpio_direction_output(drvdata->hwen_gpio, 0);
				}
				if (gpio_is_valid(drvdata->pwron)) {
					gpio_direction_output(drvdata->pwron,0);
				}
				fl_en_status = 0;
			}
		}
		
		drvdata->enable = false;
	}
	
	mutex_unlock(&drvdata->lock);

	return 0;
}

static int aw99703_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw99703_data *drvdata = i2c_get_clientdata(client);

	printk(KERN_INFO"[%s_%d] gSleep_Mode_Suspend:%d \n",__FUNCTION__,__LINE__,gSleep_Mode_Suspend);
	
	mutex_lock(&drvdata->lock);
	if (gSleep_Mode_Suspend) {
		if(++giFL_PWR_state > giFL_PWR_state_max) {
			printk(KERN_WARNING"%s():[WARNING] FL PWR state > max !!\n", __FUNCTION__);
			giFL_PWR_state = giFL_PWR_state_max;
		}

		if (giFL_PWR_state == 1) {
			printk("%s():FL PWR[ON] \n",__FUNCTION__);
			if (gpio_is_valid(drvdata->pwron)) {
				gpio_direction_output(drvdata->pwron,1);
			}
			if(gpio_is_valid(drvdata->hwen_gpio)) {
				printk(KERN_INFO"=== hwen_gpio:%d \n", drvdata->hwen_gpio);
				gpio_direction_output(drvdata->hwen_gpio, 1);
			}
			fl_en_status = 1;
		}
		
		aw99703_backlight_init(drvdata);
		aw99703_backlight_enable(drvdata);
		aw99703_set_cur_brightness(drvdata, 0, 0);
	}
	
	mutex_unlock(&drvdata->lock);

	return 0;
}

static void aw99703_get_dt_data(struct device *dev, struct aw99703_data *drvdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	const char *name;
	u32 bl_channel, temp;

	drvdata->hwen_gpio = of_get_named_gpio(np, "aw99703,hwen-gpio", 0);
	pr_info("%s drvdata->hwen_gpio --<%d>\n", __func__, drvdata->hwen_gpio);

	// Enable FL_PWR_ON 
	drvdata->pwron = of_get_named_gpio(np, "aw99703,pwron", 0);
	pr_info("%s drvdata->pwron --<%d>\n", __func__, drvdata->pwron);

	if (gpio_is_valid(drvdata->pwron)) {
			gpio_direction_output(drvdata->pwron,1);
	}


	rc = of_property_read_u32(np, "aw99703,pwm-mode", &drvdata->pwm_mode);
	if (rc != 0)
		pr_err("%s pwm-mode not found\n", __func__);
	else
		pr_info("%s pwm_mode=%d\n", __func__, drvdata->pwm_mode);

	drvdata->using_lsb = of_property_read_bool(np, "aw99703,using-lsb");
	pr_info("%s using_lsb --<%d>\n", __func__, drvdata->using_lsb);

	if (drvdata->using_lsb) {
		drvdata->default_brightness = 0;
		drvdata->max_brightness = 2047;
	} else {
		drvdata->default_brightness = 0;
		drvdata->max_brightness = 255;
	}

	rc = of_property_read_u32(np, "aw99703,bl-fscal-led", &temp);
	if (rc) {
		pr_err("Invalid backlight full-scale led current!\n");
	} else {
		drvdata->full_scale_led = temp;
		pr_info("%s full-scale led current --<%d mA>\n",
			__func__, drvdata->full_scale_led);
	}

	rc = of_property_read_u32(np, "aw99703,turn-on-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,turnon!\n");
	} else {
		drvdata->ramp_on_time = temp;
		pr_info("%s ramp on time --<%d ms>\n",
			__func__, drvdata->ramp_on_time);
	}

	rc = of_property_read_u32(np, "aw99703,turn-off-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,,turnoff!\n");
	} else {
		drvdata->ramp_off_time = temp;
		pr_info("%s ramp off time --<%d ms>\n",
			__func__, drvdata->ramp_off_time);
	}

	rc = of_property_read_u32(np, "aw99703,pwm-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid pwm-tarns-dim value!\n");
	} else {
		drvdata->pwm_trans_dim = temp;
		pr_info("%s pwm trnasition dimming	--<%d ms>\n",
			__func__, drvdata->pwm_trans_dim);
	}

	rc = of_property_read_u32(np, "aw99703,i2c-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid i2c-trans-dim value !\n");
	} else {
		drvdata->i2c_trans_dim = temp;
		pr_info("%s i2c transition dimming --<%d ms>\n",
			__func__, drvdata->i2c_trans_dim);
	}

	rc = of_property_read_u32(np, "aw99703,bl-channel", &bl_channel);
	if (rc) {
		pr_err("Invalid channel setup\n");
	} else {
		drvdata->channel = bl_channel;
		pr_info("%s bl-channel --<%x>\n", __func__, drvdata->channel);
	}

	rc = of_property_read_u32(np, "aw99703,bl-map", &drvdata->bl_map);
	if (rc != 0)
		pr_err("%s bl_map not found\n", __func__);
	else
		pr_info("%s bl_map=%d\n", __func__, drvdata->bl_map);
		
	rc = of_property_read_string(np, "aw99703,label", &name);
	if (rc)
		snprintf(drvdata->label, sizeof(drvdata->label),
			"%s::", drvdata->client->name);
	else
		snprintf(drvdata->label, sizeof(drvdata->label),
			 "%s_%s", drvdata->client->name, name);
	
	drvdata->led_dev.name = drvdata->label;
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw99703_i2c_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw99703_data *aw99703 = dev_get_drvdata(dev);

	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw99703_i2c_write(aw99703->client,
				(unsigned char)databuf[0],
				(unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw99703_i2c_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct aw99703_data *aw99703 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW99703_REG_MAX; i++) {
		if (!(aw99703_reg_access[i]&REG_RD_ACCESS))
			continue;
		aw99703_i2c_read(aw99703->client, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n",
				i, reg_val);
	}

	return len;
}

static DEVICE_ATTR(reg, 0664, aw99703_i2c_reg_show, aw99703_i2c_reg_store);
static DEVICE_ATTR(max_color, 0644, led_max_color_get, led_max_color_set);
static DEVICE_ATTR(color, 0644, led_color_get, led_color_set);
static DEVICE_ATTR(bl_power, 0644, led_bl_power_get, led_bl_power_set);
static DEVICE_ATTR(ramp_on_time, 0644, led_ramp_on_get, led_ramp_on_set);
static DEVICE_ATTR(ramp_off_time, 0644, led_ramp_off_get, led_ramp_off_set);

static struct attribute *aw99703_attributes[] = {
	&dev_attr_reg.attr,
	NULL
};

static struct attribute_group aw99703_attribute_group = {
	.attrs = aw99703_attributes
};

static int aw99703_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct aw99703_data *drvdata;
#ifdef KERNEL_ABOVE_4_14
	struct backlight_device *bl_dev;
	struct backlight_properties props;
#endif
	int err = 0;

	pr_info("%s enter! driver version %s\n", __func__, AW99703_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : I2C_FUNC_I2C not supported\n", __func__);
		err = -EIO;
		goto err_out;
	}

	if (!client->dev.of_node) {
		pr_err("%s : no device node\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}

	drvdata = kzalloc(sizeof(struct aw99703_data), GFP_KERNEL);
	if (drvdata == NULL) {
		pr_err("%s : kzalloc failed\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}

	drvdata->client = client;
	drvdata->adapter = client->adapter;
	drvdata->addr = client->addr;
	drvdata->brightness = 0;
	drvdata->enable = true;
	drvdata->led_dev.default_trigger = "bkl-trigger";
	drvdata->led_dev.name = AW99703_LED_DEV;
	drvdata->led_dev.brightness_set = aw99703_brightness_set;
	drvdata->led_dev.max_brightness = MAX_BRIGHTNESS;
	mutex_init(&drvdata->lock);
	INIT_WORK(&drvdata->work, aw99703_work);
	aw99703_get_dt_data(&client->dev, drvdata);
	i2c_set_clientdata(client, drvdata);
	aw99703_gpio_init(drvdata);

	err = aw99703_read_chipid(drvdata);
	if (err < 0) {
		pr_err("%s : ID idenfy failed\n", __func__);
		goto err_init;
	}

	err = led_classdev_register(&client->dev, &drvdata->led_dev);
	if (err < 0) {
		pr_err("%s : Register led class failed\n", __func__);
		err = -ENODEV;
		goto err_init;
	} else {
	pr_debug("%s: Register led class successful\n", __func__);
	}

#ifdef KERNEL_ABOVE_4_14
	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.brightness = MAX_BRIGHTNESS;
	props.max_brightness = MAX_BRIGHTNESS;
	bl_dev = backlight_device_register(AW99703_NAME, &client->dev,
					drvdata, &aw99703_bl_ops, &props);
#endif

	g_aw99703_data = drvdata;
	aw99703_backlight_init(drvdata);
	aw99703_backlight_enable(drvdata);

	/* clear current and brightness default value */
	aw99703_set_cur_brightness(drvdata, drvdata->full_scale_led, drvdata->default_brightness);
	
	giFL_PWR_state_max += 1;
	giFL_PWR_state += 1;
	gpled[g_aw99703_leds++] = drvdata;
	
	//schedule_work(&drvdata->work);
	err = sysfs_create_group(&client->dev.kobj, &aw99703_attribute_group);
	if (err < 0) {
		dev_info(&client->dev, "%s error creating sysfs attr files\n",
			__func__);
		goto err_sysfs;
	}
	
	err = device_create_file(drvdata->led_dev.dev, &dev_attr_max_color);
	if (err < 0) {
		dev_err(drvdata->led_dev.dev, "fail : max_color create.\n");
		goto err_sysfs;
	}

	err = device_create_file(drvdata->led_dev.dev, &dev_attr_color);
	if (err < 0) {
		dev_err(drvdata->led_dev.dev, "fail : color create.\n");
		goto err_sysfs;
	}
	
	err = device_create_file(drvdata->led_dev.dev, &dev_attr_bl_power);
	if (err < 0) {
		dev_err(drvdata->led_dev.dev, "fail : bl_power create.\n");
		goto err_sysfs;
	}
	
	err = device_create_file(drvdata->led_dev.dev, &dev_attr_ramp_on_time);
	if (err < 0) {
		dev_err(drvdata->led_dev.dev, "fail : ramp_on_time create.\n");
		goto err_sysfs;
	}
	
	err = device_create_file(drvdata->led_dev.dev, &dev_attr_ramp_off_time);
	if (err < 0) {
		dev_err(drvdata->led_dev.dev, "fail : ramp_off_time create.\n");
		goto err_sysfs;
	}
	
	pr_info("%s exit\n", __func__);
	return 0;

err_sysfs:
err_init:
	kfree(drvdata);
err_out:
	return err;
}

static int aw99703_remove(struct i2c_client *client)
{
	struct aw99703_data *drvdata = i2c_get_clientdata(client);

	led_classdev_unregister(&drvdata->led_dev);

	kfree(drvdata);
	return 0;
}

static const struct i2c_device_id aw99703_id[] = {
	{AW99703_NAME, 0},
	{}
};
static struct of_device_id match_table[] = {
		{.compatible = "awinic,aw99703-bl",}
};

static const struct dev_pm_ops aw99703_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(aw99703_suspend, aw99703_resume)
};

MODULE_DEVICE_TABLE(i2c, aw99703_id);

static struct i2c_driver aw99703_i2c_driver = {
	.probe = aw99703_probe,
	.remove = aw99703_remove,
	.id_table = aw99703_id,
	.driver = {
		.name = AW99703_NAME,
		.owner = THIS_MODULE,
		.pm = &aw99703_pm_ops,
		.of_match_table = match_table,
	},
};

module_i2c_driver(aw99703_i2c_driver);
MODULE_DESCRIPTION("Back Light driver for aw99703");
MODULE_LICENSE("GPL v2");

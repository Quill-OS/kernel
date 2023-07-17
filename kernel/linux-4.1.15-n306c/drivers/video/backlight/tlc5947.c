/*
* Simple driver for Texas Instruments TLC5947 Backlight driver chip
* Copyright (C) 2012 Texas Instruments
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/platform_data/tlc5947.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>


#include "../../../arch/arm/mach-imx/ntx_firmware.h"

#define DEFAULT_ON_RAMP_LVL	0
#define DEFAULT_OFF_RAMP_LVL	0
#define DEFAULT_UP_RAMP_LVL	0
#define DEFAULT_DN_RAMP_LVL	0

struct tlc5947_chip {
	struct device *dev;
#if 0
	struct delayed_work work;
	int irq;
	struct workqueue_struct *irqthread;
#endif
	struct spi_device *spi;
	struct backlight_device *bled;

	int gpio_power_on;
	int gpio_fl_r_en;
	int gpio_xlat;
	int gpio_blank;

	unsigned short out[24];
	// ntx used
	int frontlight_table; 	// 2 color mix table index .
	int percentage;			
};

extern int gSleep_Mode_Suspend;

static struct tlc5947_chip *gpchip[1];
static int tlc5947_chips = 0;


extern NTX_FW_TLC5947FL_dualcolor_hdr *gptTlc5947fl_dualcolor_tab_hdr; 
extern NTX_FW_TLC5947FL_dualcolor_percent_tab *gptTlc5947fl_dualcolor_percent_tab; 


int tlc5947_get_FL_current(void)
{
	int iRet = -1;
	unsigned long dwCurrent=0;

	int iColor=gpchip[0]->frontlight_table;
	int iPercentIdx=gpchip[0]->percentage-1;

	if(gptTlc5947fl_dualcolor_percent_tab) {
		if(gpchip[0]->percentage>0) {
					iRet = (int)gptTlc5947fl_dualcolor_percent_tab[iColor].dwCurrentA[iPercentIdx];
					printk("dualcolor[%d][%d] curr=%d\n",iColor,iPercentIdx+1,iRet);
		}
		else {
			iRet = 0;
		}
	}
	else
		return -1 ;

	return iRet;
}



static int tlc5947_send (struct spi_device *spi, unsigned short *pBuf)
{
	struct spi_message msg;
	struct spi_transfer xfer = {
		.len		= 24*sizeof(unsigned short),
		.bits_per_word = 12,
		.tx_buf		= pBuf,
	};
	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	return spi_sync(spi, &msg);
}

static int tlc5947_update (struct tlc5947_chip *pchip) 
{
	int isChipOn = 0;
	int i;
	unsigned short buf[24];
	static int isPowerOn;

	memset (buf, 0, 24*sizeof(unsigned short));
	for (i=0; i<8; i++) {
		if (pchip->out[i]) {
			isChipOn = 1;
		}
		buf[23-i] = pchip->out[i];
	}
	if (isChipOn) {
		gpio_direction_output (pchip->gpio_power_on, 1);
		gpio_direction_output (pchip->gpio_xlat, 0);
		if (!isPowerOn) {
			gpio_direction_output (pchip->gpio_blank, 1);
			msleep(20);
		}
		tlc5947_send (pchip->spi, buf);
		gpio_direction_output (pchip->gpio_xlat, 1);
		msleep(20);
		gpio_direction_output (pchip->gpio_xlat, 0);
		gpio_direction_output (pchip->gpio_blank, 0);
	}
	else {
		gpio_direction_output (pchip->gpio_blank, 0);
		gpio_direction_output (pchip->gpio_power_on, 0);
		gpio_direction_output (pchip->gpio_xlat, 0);
		gpio_direction_output (pchip->gpio_blank, 0);
	}
	isPowerOn = isChipOn;
	return 0;
}


// dual color fl percentage control .
int fl_tlc5947_percentage (struct tlc5947_chip *pchip,int iFL_Percentage,int bTableChanged)
{
	int iFL_temp = pchip->frontlight_table;
	if(bTableChanged==0 && pchip->percentage==iFL_Percentage)
		return ;
	pchip->percentage = iFL_Percentage;

	if (0 == iFL_Percentage) {
			pchip->out[0] = pchip->out[1] = pchip->out[2] = pchip->out[3] = 0;
			tlc5947_update (pchip);
	}
	else if(gptTlc5947fl_dualcolor_percent_tab) {
		unsigned short ch1_val=0,ch2_val=0,ch3_val=0,ch4_val=0;
		unsigned char boostLv=0;
		unsigned char ch0_Num0 = gptTlc5947fl_dualcolor_tab_hdr->bC0_ChNumA[0];
		unsigned char ch0_Num1 = gptTlc5947fl_dualcolor_tab_hdr->bC0_ChNumA[1];
		unsigned char ch1_Num0 = gptTlc5947fl_dualcolor_tab_hdr->bC1_ChNumA[0];
		unsigned char ch1_Num1 = gptTlc5947fl_dualcolor_tab_hdr->bC1_ChNumA[1];

		if(gptTlc5947fl_dualcolor_percent_tab[iFL_temp].wC0_ChValA[0][iFL_Percentage-1]!=0) {
			ch1_val = gptTlc5947fl_dualcolor_percent_tab[iFL_temp].wC0_ChValA[0][iFL_Percentage-1];
		}
		if(gptTlc5947fl_dualcolor_percent_tab[iFL_temp].wC0_ChValA[1][iFL_Percentage-1]!=0) {
			ch2_val = gptTlc5947fl_dualcolor_percent_tab[iFL_temp].wC0_ChValA[1][iFL_Percentage-1];
		}
		if(gptTlc5947fl_dualcolor_percent_tab[iFL_temp].wC1_ChValA[0][iFL_Percentage-1]!=0) {
			ch3_val = gptTlc5947fl_dualcolor_percent_tab[iFL_temp].wC1_ChValA[0][iFL_Percentage-1];
		}
		if(gptTlc5947fl_dualcolor_percent_tab[iFL_temp].wC1_ChValA[1][iFL_Percentage-1]!=0) {
			ch4_val = gptTlc5947fl_dualcolor_percent_tab[iFL_temp].wC1_ChValA[1][iFL_Percentage-1];
		}		
		boostLv =  gptTlc5947fl_dualcolor_percent_tab[iFL_temp].bBoostLvlA[iFL_Percentage-1];
		
		if(boostLv < 2 && boostLv >= 0)
			gpio_set_value(pchip->gpio_fl_r_en,boostLv);

		pchip->out[ch0_Num0] = ch1_val ;
		pchip->out[ch0_Num1] = ch2_val ;
		pchip->out[ch1_Num0] = ch3_val ;
		pchip->out[ch1_Num1] = ch4_val ;

		//printk(KERN_ERR"------CH%d:%d , CH%d:%d , CH%d:%d , CH%d:%d , BootLv:%d -----\n",\
			ch0_Num0,ch1_val,ch0_Num1,ch2_val,ch1_Num0,ch3_val,ch1_Num1,ch4_val,boostLv);
		tlc5947_update (pchip);			
	}

	return 0;
}


int fl_tlc5947_percentageEx (int iFL_Percentage)
{
	// using default value
	return  fl_tlc5947_percentage (gpchip[0],iFL_Percentage,0);
}

/* initialize chip */
static int tlc5947_chip_init(struct tlc5947_chip *pchip)
{
	/* set boost control */
	memset (pchip->out, 0, 24*sizeof(unsigned short));
	tlc5947_update (pchip);

	return 0;
}

/* update and get brightness */
static int tlc5947_update_status(struct backlight_device *bl)
{
	int i;
	struct tlc5947_chip *pchip = bl_get_data(bl);

	printk("%s(%d) %s brgitness %d\n",__FUNCTION__,__LINE__,__FUNCTION__, bl->props.brightness);

	for (i=0; i<8; i++)
		pchip->out[i] = bl->props.brightness;
	tlc5947_update (pchip);

	return bl->props.brightness;
}

static int tlc5947_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops tlc5947_ops = {
	.update_status = tlc5947_update_status,
	.get_brightness = tlc5947_get_brightness,
};

static ssize_t out_gs_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct tlc5947_chip *pchip = dev_get_drvdata(dev);
	sprintf (buf, "%d %d %d %d %d %d %d %d", \
		pchip->out[0], pchip->out[1], pchip->out[2], pchip->out[3], \
		pchip->out[4], pchip->out[5], pchip->out[6], pchip->out[7] );
	return strlen (buf);
}

static ssize_t out_gs_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct tlc5947_chip *pchip = dev_get_drvdata(dev);

	if (count) {
		char channel = buf[0]-'0';
		if (8 > channel) {
			int duty = simple_strtol(&buf[2], NULL, 10);
			printk ("Set channel %d to %d\n", channel, duty);
			pchip->out[channel] = duty;
			tlc5947_update (pchip);
		}
		else {
			printk ("Wrong channel No.\n");
		}
	}
	else {
		printk ("echo \"channel duty\" to set duty of channel.\n");
	}
	return count;
}

static ssize_t fl_r_en_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct tlc5947_chip *pchip = dev_get_drvdata(dev);
	int val = gpio_get_value(pchip->gpio_fl_r_en) ;
	sprintf (buf, "gpio_fl_r_en: %d \n",  val );
	return strlen (buf);
}

static ssize_t fl_r_en_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct tlc5947_chip *pchip = dev_get_drvdata(dev);

	if (count) {
		int en = simple_strtol(&buf[0], NULL, 10);
		gpio_set_value(pchip->gpio_fl_r_en,en);
		printk ("Set fl_r_en to %d\n", en);
	}
	else {
		printk ("echo \"fl_r_en\" to set failed.\n");
	}
	return count;
}

static ssize_t led_color_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct tlc5947_chip *pchip = dev_get_drvdata(dev);

	sprintf (buf, "%d", pchip->frontlight_table);
	return strlen(buf);
}

static ssize_t led_color_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int val = simple_strtoul (buf, NULL, 10);
	struct tlc5947_chip *pchip = dev_get_drvdata(dev);
	if(val >= (int)gptTlc5947fl_dualcolor_tab_hdr->dwTotalColors)
	{
		printk(KERN_ERR "[TLC5947] set color failed. val:%d > max_color:%d \n",val,(int)gptTlc5947fl_dualcolor_tab_hdr->dwTotalColors);
		return count;
	}
	pchip->frontlight_table = val;
	fl_tlc5947_percentage (pchip, pchip->percentage,1);

	return count;
}


static ssize_t led_max_color_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int iColors;
	if (gptTlc5947fl_dualcolor_percent_tab) {
		iColors = (int)gptTlc5947fl_dualcolor_tab_hdr->dwTotalColors ;
	}
	else {
		iColors = 1 ;
	}
	sprintf (buf, "%d\n", (iColors-1));
	return strlen(buf);
}

static ssize_t led_max_color_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	return count;
}


static DEVICE_ATTR (out, 0644, out_gs_get, out_gs_set);
static DEVICE_ATTR (fl_r_en, 0644, fl_r_en_get,fl_r_en_set);
static DEVICE_ATTR (color, 0644, led_color_get, led_color_set);
static DEVICE_ATTR (max_color, 0644, led_max_color_get, led_max_color_set);

static int tlc5947_backlight_register(struct tlc5947_chip *pchip)
{
	struct backlight_properties props;
	int rval;
	
	props.type = BACKLIGHT_RAW;
	props.brightness = 0;
	props.max_brightness = 4096;

	pchip->bled =
		backlight_device_register(TLC5947_NAME, pchip->dev, pchip,
							       &tlc5947_ops, &props);
	if (IS_ERR(pchip->bled))
		return PTR_ERR(pchip->bled);

	rval = device_create_file(&pchip->bled->dev, &dev_attr_out);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : tlc5947 out create.\n");
		return rval;
	}

	rval = device_create_file(&pchip->bled->dev, &dev_attr_color);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : tlc5947 color create.\n");
		return rval;
	}

	rval = device_create_file(&pchip->bled->dev, &dev_attr_max_color);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : tlc5947 max_color create.\n");
		return rval;
	}	


	if(pchip->gpio_fl_r_en > 0)
	{
		rval = device_create_file(&pchip->bled->dev, &dev_attr_fl_r_en);
		if (rval < 0) {
			dev_err(&pchip->bled->dev, "fail : tlc5947 fl_r_en create.\n");
			return rval;
		}
	}

	return 0;
}

static int tlc5947_probe(struct spi_device *spi)
{
	struct tlc5947_chip *pchip;
	int rval;

	pchip = devm_kzalloc(&spi->dev, sizeof(struct tlc5947_chip),
			     GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;
	pchip->dev = &spi->dev;
	spi->bits_per_word = 12;
	spi->mode = SPI_MODE_0; 	// POL=0, PHA=0

	rval = spi_setup(spi);
	if (rval < 0) {
		dev_err (pchip->dev, "spi setup error!");
		return -ENOMEM;
	}
	pchip->spi = spi;
#ifdef CONFIG_OF
	{
		struct device_node *tlc5947_bl_np = of_node_get(pchip->dev->of_node);

			pchip->gpio_power_on = of_get_named_gpio(tlc5947_bl_np, "gpio_fl_pwr_on", 0);
		if (gpio_is_valid(pchip->gpio_power_on)) {
			if (0 > devm_gpio_request_one(&spi->dev, pchip->gpio_power_on, GPIOF_OUT_INIT_LOW, "fl_pwr_on")) 
				dev_err(&spi->dev, "request FL_EN gpio failed !\n");
		}
		else
			dev_err(&spi->dev, "no epdc pmic pwrgood pin available\n");

		pchip->gpio_xlat = of_get_named_gpio(tlc5947_bl_np, "gpio_xlat", 0);
		if (gpio_is_valid(pchip->gpio_xlat)) {
			if (0 > devm_gpio_request_one(&spi->dev, pchip->gpio_xlat, GPIOF_OUT_INIT_LOW, "fl_xlat")) 
				dev_err(&spi->dev, "request FL_XLT gpio failed !\n");
		}
		else
			dev_err(&spi->dev, "no FL_XLT pin available\n");

		pchip->gpio_blank = of_get_named_gpio(tlc5947_bl_np, "gpio_blank", 0);
		if (gpio_is_valid(pchip->gpio_blank)) {
			if (0 > devm_gpio_request_one(&spi->dev, pchip->gpio_blank, GPIOF_OUT_INIT_LOW, "fl_blank")) 
				dev_err(&spi->dev, "request FL_blank gpio failed !\n");
		}
		else
			dev_err(&spi->dev, "no FL_blank pin available\n");

		pchip->gpio_fl_r_en = of_get_named_gpio(tlc5947_bl_np, "gpio_fl_r_en", 0);
		//printk(KERN_ERR "----- FL_R_EN:%d , pwr:%d ,LINE:%d  \n",pchip->gpio_fl_r_en,pchip->gpio_power_on,__LINE__);
		if (gpio_is_valid(pchip->gpio_fl_r_en)) {
			if (0 > devm_gpio_request_one(&spi->dev, pchip->gpio_fl_r_en, GPIOF_OUT_INIT_LOW, "fl_r_en")) 
				dev_err(&spi->dev, "request FL_R_EN gpio failed !\n");
			if(pchip->gpio_fl_r_en > 0)
			{
				gpio_direction_output(pchip->gpio_fl_r_en,0);
				//printk(KERN_ERR "----- GPIO %d  set output  \n",pchip->gpio_fl_r_en);
			}				
		}
		else
			dev_err(&spi->dev, "no gpio_fl_r_en pin available\n");

	}
#else
	{
		struct tlc5947_platform_data *pdata = spi->dev.platform_data;
		pchip->gpio_power_on = pdata->gpio_power_on;
		pchip->gpio_xlat = pdata->gpio_xlat;
		pchip->gpio_blank = pdata->gpio_blank;
		pchip->gpio_fl_r_en = pdata->gpio_fl_r_en;
		pchip->percentage = 0;
		gpio_request (pchip->gpio_power_on, "TLC5947_EN");
		gpio_request (pchip->gpio_xlat, "TLC5947_XLAT");
		gpio_request (pchip->gpio_blank, "TLC5947_BLANK");
		gpio_request (pchip->gpio_fl_r_en,"TLC5947_FL_R_EN");
	}
#endif
	/* backlight register */
	rval = tlc5947_backlight_register(pchip);
	if (rval < 0) {
		dev_err(&spi->dev, "fail : backlight register.\n");
		return rval;
	}

	rval = tlc5947_chip_init(pchip);
	if (rval < 0) {
		dev_err(&spi->dev, "fail : init chip\n");
		return rval;
	}

	dev_set_drvdata(&spi->dev, pchip);

	gpchip[tlc5947_chips++] = pchip;
	dev_info(&spi->dev, "TLC5947 backlight register OK.\n");
	return 0;
}

static int tlc5947_remove(struct spi_device *spi)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tlc5947_bl_dt_match[] = {
	{ .compatible = "ti,tlc5947_bl", },
	{},
};
MODULE_DEVICE_TABLE(of, tlc5947_bl_dt_match);
#endif

static struct spi_driver tlc5947_spi_driver = {
	.driver = {
		   .name = TLC5947_NAME,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(tlc5947_bl_dt_match),
#endif
		   },
	.probe = tlc5947_probe,
	.remove = tlc5947_remove,
};

static int __init tlc5947_init(void)
{
	return spi_register_driver(&tlc5947_spi_driver);
}

static void __exit tlc5947_exit(void)
{
	spi_unregister_driver(&tlc5947_spi_driver);
}
module_init(tlc5947_init);
module_exit(tlc5947_exit);

MODULE_DESCRIPTION("Texas Instruments Backlight driver for TLC5947");
MODULE_LICENSE("GPL v2");

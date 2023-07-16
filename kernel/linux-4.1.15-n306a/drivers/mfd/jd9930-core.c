/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
 *
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*!
 * @file pmic/core/jd9930.c
 * @brief This file contains JD9930 specific PMIC code. This implementaion
 * may differ for each PMIC chip.
 *
 * @ingroup PMIC_CORE
 */

/*
 * Includes
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/pmic_status.h>
#include <linux/mfd/core.h>
#include <linux/mfd/jd9930.h>
#include <asm/mach-types.h>


#define GDEBUG 1
#include <linux/gallen_dbg.h>

//#define NTX_HWCONFIG_ENABLED 1

#ifdef NTX_HWCONFIG_ENABLED	//[
#include "../../arch/arm/mach-imx/ntx_hwconfig.h"

extern volatile NTX_HWCONFIG *gptHWCFG;
#endif //] NTX_HWCONFIG_ENABLED
/*
 * EPDC PMIC I2C address
 */
#define EPDC_PMIC_I2C_ADDR 0x18


#define JD9930_PWRON_READYUS	1000

static struct jd9930 *gptJD9930;

static int jd9930_detect(struct i2c_client *client, struct i2c_board_info *info);
//static struct regulator *gpio_regulator;

static struct mfd_cell jd9930_devs[] = {
	{ .name = "jd9930-pmic", },
	{ .name = "jd9930-sns", },
};

static const unsigned short normal_i2c[] = {EPDC_PMIC_I2C_ADDR, I2C_CLIENT_END};


int jd9930_reg_read(struct jd9930 *jd9930,int reg_num, unsigned int *reg_val)
{
	int result,iRet;
	struct i2c_client *jd9930_client = jd9930->i2c_client;
	struct gpio_desc *gpiod_pwrall;
	int iOldPwrallState;

	if (jd9930_client == NULL) {
		dev_err(&jd9930_client->dev,
			"jd9930 I2C adaptor not ready !\n");
		return PMIC_ERROR;
	}

	gpiod_pwrall = gpio_to_desc(jd9930->gpio_pmic_pwrall);
	if(!gpiod_pwrall) {
		dev_err(&jd9930_client->dev,
			"get pmic_powerup gpio desc failed !\n");
	}
	else {
		iOldPwrallState = gpiod_get_value(gpiod_pwrall);
		if(0==iOldPwrallState) {
			dev_warn(&jd9930_client->dev,
				"in chip off state !! turn it on !\n");

			//gpiod_set_value(gpiod_pwrall,1);
			jd9930_chip_power(jd9930,1);
		}
	}

	iRet = PMIC_SUCCESS;
	do {
		result = i2c_smbus_read_byte_data(jd9930_client, reg_num);
		if (result < 0) {
			if(!time_is_before_jiffies(jd9930->jiffies_chip_wake)) {
				dev_err(&jd9930_client->dev,
					"Unable to read jd9930 reg%d via I2C after waked , retry...\n",reg_num);
				continue;
			}
			dev_err(&jd9930_client->dev,
				"Unable to read jd9930 register%d via I2C\n",reg_num);
			iRet = PMIC_ERROR;
			goto exit;
		}
		else {
			pr_debug("%s():reg%d=0x%x\n",__FUNCTION__,reg_num,result);
		}
		break;
		mdelay(1);
	}while(0);

	*reg_val = result;

exit:
#if 0
	if(gpiod_pwrall) {
		if(0==iOldPwrallState) {
			gpiod_set_value(gpiod_pwrall,0);
		}
	}
#endif

	return iRet;
}

int jd9930_reg_write(struct jd9930 *jd9930,int reg_num, const unsigned int reg_val)
{
	int result,iRet;
	struct i2c_client *jd9930_client=jd9930->i2c_client;
	struct gpio_desc *gpiod_pwrall;
	int iOldPwrallState;


	if (jd9930_client == NULL) {
		dev_err(&jd9930_client->dev,
			"jd9930 I2C adaptor not ready !\n");
		return PMIC_ERROR;
	}

	gpiod_pwrall = gpio_to_desc(jd9930->gpio_pmic_pwrall);
	if(!gpiod_pwrall) {
		dev_err(&jd9930_client->dev,
			"get pmic_powerup gpio desc failed !\n");
	}
	else {
		iOldPwrallState = gpiod_get_value(gpiod_pwrall);
		if(0==iOldPwrallState) {
			dev_warn(&jd9930_client->dev,
				"in chip off state !! turn it on\n");

			//gpiod_set_value(gpiod_pwrall,1);
			jd9930_chip_power(jd9930,1);
		}
	}


	iRet = PMIC_SUCCESS;
	do {
		result = i2c_smbus_write_byte_data(jd9930_client, reg_num, reg_val);
		if (result < 0) {
			if(!time_is_before_jiffies(jd9930->jiffies_chip_wake)) {
				dev_err(&jd9930_client->dev,
					"Unable to write JD9930 reg%d via I2C after waked! retry ...\n",reg_num);
				continue;
			}
			dev_err(&jd9930_client->dev,
				"Unable to write JD9930 register%d via I2C\n",reg_num);
			iRet = PMIC_ERROR;
			goto exit;
		}
		else {
			pr_debug("%s():reg%d<=0x%x\n",__FUNCTION__,reg_num,reg_val);
		}
		break;
		mdelay(1);
	}while(1);

exit:
#if 0
	if(gpiod_pwrall) {
		if(0==iOldPwrallState) {
			gpiod_set_value(gpiod_pwrall,0);
		}
	}
#endif

	return iRet;
}


int jd9930_chip_power(struct jd9930 *jd9930,int iIsON)
{
	int iPwrallCurrentStat=-1;
	int iRet = 0;
	
	unsigned long waitDelayTime;
	struct gpio_desc *gpiod_pwrall,*gpiod_xon,*gpiod_rail;

	if(!jd9930) {
		printk(KERN_ERR"%s(): object error !! \n",__FUNCTION__);
		return -1;
	}

	if (!gpio_is_valid(jd9930->gpio_pmic_pwrall)) {
		printk(KERN_ERR"%s(): no epdc pmic pwrall pin available\n",__FUNCTION__);
		return -2;
	}

	pr_debug("%s(%d)\n",__FUNCTION__,iIsON);

	gpiod_pwrall = gpio_to_desc(jd9930->gpio_pmic_pwrall);
	if(!gpiod_pwrall) {
		printk(KERN_ERR"%s(%d) get pmic_powerup gpio desc failed !\n",__func__,__LINE__);
	}
	if (gpio_is_valid(jd9930->gpio_pmic_xon_ctrl)) {
		gpiod_xon = gpio_to_desc(jd9930->gpio_pmic_xon_ctrl);
		if(!gpiod_xon) {
			printk("%s(%d) get pmic_xon gpio desc failed !\n",__func__,__LINE__);
		}
	}
	gpiod_rail = gpio_to_desc(jd9930->gpio_pmic_powerup);
	if(!gpiod_rail) {
		printk(KERN_ERR"%s(%d) get pmic_rail gpio desc failed !\n",__func__,__LINE__);
	}

	//iPwrallCurrentStat = jd9930->gpio_pmic_pwrall_stat;
	iPwrallCurrentStat = gpiod_get_value(gpiod_pwrall);

	if(iIsON) {
		if(gpiod_xon) {
			gpiod_direction_input(gpiod_xon);
		}

		gpiod_set_value(gpiod_pwrall,1);
		jd9930->jiffies_chip_on = jiffies;
		jd9930->jiffies_chip_wake = jiffies+msecs_to_jiffies(JD9930_WAKEUP_MS);
		if(0==iPwrallCurrentStat) {
			pr_debug("%s():chip power 0->1 .\n",__FUNCTION__);
			udelay(JD9930_PWRON_READYUS);
			jd9930_regs_init(jd9930);	
			jd9930->need_reinit = 0;
		}
	}
	else {
		jd9930->need_reinit = 1;//need setup vcom/regs again .

		if(1==gpiod_get_value(gpiod_rail)) {
			ERR_MSG("%s(): trun off chip when rail power on !! turn it off.\n",__FUNCTION__);
			gpiod_set_value(gpiod_rail,0);
		}

		gpiod_set_value(gpiod_pwrall,0);
		if(gpiod_xon) {
			gpiod_set_value(gpiod_xon,0);
		}
		
		waitDelayTime = jiffies + jd9930->turnoff_delay_ep3v3;
		
		while(time_before(jiffies, waitDelayTime)) {
			//printk("wait delay time for turn off EP_3V3 ...\n");
		}
	}


	if(iPwrallCurrentStat!=iIsON) {
		if(iIsON) {
			// state change and turn on .
			iRet = 1;
		}
		else {
			// state change and turn off .
			iRet = 2;
		}
	}

	return iRet;
}



#ifdef CONFIG_OF
static struct jd9930_platform_data *jd9930_i2c_parse_dt_pdata(
					struct device *dev)
{
	struct jd9930_platform_data *pdata;
	struct device_node *pmic_np;
	struct jd9930 *jd9930 = dev_get_drvdata(dev);
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for pdata\n");
		return ERR_PTR(-ENOMEM);
	}

	pmic_np = of_node_get(dev->of_node);
	if (!pmic_np) {
		dev_err(dev, "could not find pmic sub-node\n");
		return ERR_PTR(-ENODEV);
	}

	jd9930->gpio_pmic_pwrall = of_get_named_gpio(pmic_np,
					"gpio_pmic_pwrall", 0);
	if (!gpio_is_valid(jd9930->gpio_pmic_pwrall)) {
		dev_err(dev, "no epdc pmic pwrall pin available\n");
		return ERR_PTR(-ENODEV);
	}
	else {
		printk("%s():gpio_pmic_pwrall=%d\n",__FUNCTION__,jd9930->gpio_pmic_pwrall);
	}
	ret = devm_gpio_request_one(dev, jd9930->gpio_pmic_pwrall,
				GPIOF_OUT_INIT_HIGH, "epdc-pmic-pwrall");
	if (ret < 0) {
		dev_err(dev, "request pwrall gpio failed (%d)!\n",ret);
		//goto err;
	}
	else {
		printk("%s():gpio_pmic_pwrall init set 1\n",__FUNCTION__);
		//jd9930->gpio_pmic_pwrall_stat = 1;
		jd9930->jiffies_chip_on = jiffies;
		udelay(JD9930_PWRON_READYUS);
	}



	jd9930->gpio_pmic_powerup = of_get_named_gpio(pmic_np,
					"gpio_pmic_powerup", 0);
	if (!gpio_is_valid(jd9930->gpio_pmic_powerup)) {
		dev_err(dev, "no epdc pmic powerup pin available\n");
	}
	else {
		printk("%s():gpio_pmic_powerup=%d\n",__FUNCTION__,jd9930->gpio_pmic_powerup);
	}
	ret = devm_gpio_request_one(dev, jd9930->gpio_pmic_powerup,
				GPIOF_OUT_INIT_LOW, "epdc-powerup");
	//jd9930->gpio_pmic_powerup_stat = -1;
	if (ret < 0) {
		dev_err(dev, "request powerup gpio failed (%d)!\n",ret);
	}
	else {
		//jd9930->gpio_pmic_powerup_stat = 0;
	}

	jd9930->gpio_pmic_xon_ctrl = of_get_named_gpio(pmic_np,
					"gpio_pmic_xon_ctrl", 0);
	if (!gpio_is_valid(jd9930->gpio_pmic_xon_ctrl)) {
		dev_err(dev, "no epdc pmic xon ctrl pin available\n");
		//return ERR_PTR(-ENODEV);
	}
	else {
		printk("%s():gpio_pmic_xon_ctrl=%d\n",__FUNCTION__,jd9930->gpio_pmic_xon_ctrl);
		ret = devm_gpio_request_one(dev, jd9930->gpio_pmic_xon_ctrl,
				GPIOF_IN, "epdc-pmic-xon-ctrl");
		if (ret < 0) {
			dev_err(dev, "request pwrall gpio failed (%d)!\n",ret);
			//goto err;
		}
		else {
			printk("%s():gpio_pmic_xon_ctrl init input\n",__FUNCTION__);
		}
	}

	jd9930->gpio_pmic_ts_en = of_get_named_gpio(pmic_np,
					"gpio_pmic_ts_en", 0);
	if (!gpio_is_valid(jd9930->gpio_pmic_ts_en)) {
		dev_err(dev, "no epdc pmic ts_en pin available\n");
		return ERR_PTR(-ENODEV);
	}
	else {
		printk("%s():gpio_pmic_ts_en=%d\n",__FUNCTION__,jd9930->gpio_pmic_ts_en);
	}
	ret = devm_gpio_request_one(dev, jd9930->gpio_pmic_ts_en,
				GPIOF_OUT_INIT_HIGH, "epdc-pmic-ts_en");
	if (ret < 0) {
		dev_err(dev, "request pwrall gpio failed (%d)!\n",ret);
		//goto err;
	}
	else {
		printk("%s():gpio_pmic_ts_en init set 1\n",__FUNCTION__);
	}


	return pdata;
}
#else
static struct jd9930_platform_data *jd9930_i2c_parse_dt_pdata(
					struct device *dev)
{
	return NULL;
}
#endif	/* !CONFIG_OF */

static int jd9930_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct jd9930 *jd9930;
	struct jd9930_platform_data *pdata = client->dev.platform_data;
	struct device_node *np = client->dev.of_node;
	int ret = 0;
	struct gpio_desc *gpiod_pwrall;

#ifdef NTX_HWCONFIG_ENABLED //[
	switch(gptHWCFG->m_val.bDisplayCtrl) {
	case 26: // mx6sll+JD9930
	case 27: // mx6ull+JD9930
	case 28: // b300+JD9930
	case 29: // b810+JD9930
		break;
	default :
		printk("%s skipped because of wrong DisplayCtrl %d\n",
				__func__,gptHWCFG->m_val.bDisplayCtrl);
		return -ENODEV;
	}
#endif //]NTX_HWCONFIG_ENABLED

	printk("jd9930_probe calling\n");

	if (!np)
		return -ENODEV;


/*	
	gpio_regulator = devm_regulator_get(&client->dev, "SENSOR");
	if (!IS_ERR(gpio_regulator)) {
		ret = regulator_enable(gpio_regulator);
		if (ret) {
			dev_err(&client->dev, "PMIC power on failed !\n");
			return ret;
		}
	}
*/


	/* Create the PMIC data structure */
	jd9930 = kzalloc(sizeof(struct jd9930), GFP_KERNEL);
	if (jd9930 == NULL) {
		kfree(client);
		return -ENOMEM;
	}

	//jd9930->gpio_pmic_powerup_stat = -1;
	//jd9930->gpio_pmic_pwrall_stat = -1;
	jd9930->need_reinit = 1;// need setup registers again .
	jd9930->jiffies_chip_on = jiffies;// .
	jd9930->jiffies_chip_wake = jiffies;// .

	jd9930->on_delay1 = 0xff;
	jd9930->on_delay2 = 0xff;
	jd9930->on_delay3 = 0xff;
	jd9930->on_delay4 = 0xff;

	jd9930->vgh_ext = 0;
	jd9930->vgl_ext = 0;
	jd9930->vghnm_ext = 0;
	jd9930->vghnm = 0;
	jd9930->disa_delay = 0;
	jd9930->xon_delay = 0;
	jd9930->xon_len = 0;


	/* Initialize the PMIC data structure */
	i2c_set_clientdata(client, jd9930);
	jd9930->dev = &client->dev;
	jd9930->i2c_client = client;


	if (jd9930->dev->of_node) {
		pdata = jd9930_i2c_parse_dt_pdata(jd9930->dev);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			goto err1;
		}

	}


	if (!gpio_is_valid(jd9930->gpio_pmic_pwrall)) {
		dev_err(&client->dev, "pwrall gpio not available !!\n");
		goto err1;
	}
	gpiod_pwrall = gpio_to_desc(jd9930->gpio_pmic_pwrall);
	if(!gpiod_pwrall) {
		dev_err(&client->dev,"get pmic_powerup gpio desc failed !\n");
	}

	//if(1!=jd9930->gpio_pmic_pwrall_stat) 
	if(1!=gpiod_get_value(gpiod_pwrall)) 
	{
		dev_info(&client->dev, "PMIC chip off, turning on the PMIC chip power ...\n");
		gpiod_set_value(gpiod_pwrall,1);
		jd9930->jiffies_chip_on = jiffies+1;
		//jd9930->gpio_pmic_pwrall_stat = 1;
		udelay(JD9930_PWRON_READYUS);
	}

	ret = jd9930_detect(client, NULL);
	if (ret)
		goto err1;

	jd9930->pdata = pdata;

	mfd_add_devices(jd9930->dev, -1, jd9930_devs,
			ARRAY_SIZE(jd9930_devs),
			NULL, 0, NULL);

	gptJD9930 = jd9930;

	dev_info(&client->dev, "PMIC JD9930 for eInk display\n");

	printk("jd9930_probe success\n");

	return ret;

//err2:
	mfd_remove_devices(jd9930->dev);
err1:
	kfree(jd9930);

	return ret;
}


static int jd9930_remove(struct i2c_client *i2c)
{
	struct jd9930 *jd9930 = i2c_get_clientdata(i2c);

	mfd_remove_devices(jd9930->dev);

	return 0;
}

extern int gSleep_Mode_Suspend;
static int jd9930_suspend_late(struct device *dev)
{
	struct i2c_client *client = i2c_verify_client(dev);    
	struct jd9930 *jd9930 = i2c_get_clientdata(client);
	if (gSleep_Mode_Suspend) {
		jd9930_chip_power(jd9930,0);
	}
	return 0;
}


static int jd9930_resume_early(struct device *dev)
{
	//struct i2c_client *client = i2c_verify_client(dev);    
	//struct jd9930 *jd9930 = i2c_get_clientdata(client);


	return 0;
}

static int jd9930_suspend(struct device *dev)
{
	struct i2c_client *client = i2c_verify_client(dev);    
	struct jd9930 *jd9930 = i2c_get_clientdata(client);

	struct gpio_desc *gpiod_ts_en;

	gpiod_ts_en = gpio_to_desc(jd9930->gpio_pmic_ts_en);
	if(!gpiod_ts_en) {
		ERR_MSG("%s(%d) get pmic_ts_en gpio desc failed !\n",__func__,__LINE__);
	}
	else {
		gpiod_set_value(gpiod_ts_en,0);
	}


	

	
	return 0;
}

static int jd9930_resume(struct device *dev)
{
	struct i2c_client *client = i2c_verify_client(dev);    
	struct jd9930 *jd9930 = i2c_get_clientdata(client);
	int iChk;
	unsigned int dwReg;
	struct gpio_desc *gpiod_ts_en;
	

	if (gSleep_Mode_Suspend) {
		jd9930_chip_power(jd9930,1);
	}


	gpiod_ts_en = gpio_to_desc(jd9930->gpio_pmic_ts_en);
	if(!gpiod_ts_en) {
		ERR_MSG("%s(%d) get pmic_ts_en gpio desc failed !\n",__func__,__LINE__);
	}
	else {
		gpiod_set_value(gpiod_ts_en,1);
	}

	if(jd9930->need_reinit) 
	{
		jd9930_regs_init(jd9930);	
		jd9930->need_reinit = 0;
	}

	return 0;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int jd9930_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	//struct jd9930_platform_data *pdata = client->dev.platform_data;
	struct i2c_adapter *adapter = client->adapter;
	struct jd9930 *jd9930 = i2c_get_clientdata(client);

	const int iMaxRetryCnt = 100;
	int iRetryN;
	int iIsDeviceReady;

	int iStatus ;

	printk("jd9930_detect calling\n");


	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&adapter->dev,"I2C adapter error! \n");
		return -ENODEV;
	}

	for (iIsDeviceReady=0,iRetryN=1;iRetryN<=iMaxRetryCnt;iRetryN++)
	{
		/* identification */

		iStatus = i2c_smbus_read_byte_data(client,0);
		if(iStatus>=0) {
			iIsDeviceReady = 1;
			break;
		}
		else {
			msleep(2);
			dev_err(&adapter->dev,
					"Device probe no ACK , retry %d/%d ... \n",iRetryN,iMaxRetryCnt);
		}
	}

	if(!iIsDeviceReady) {
		dev_err(&adapter->dev,
		    "Device no ACK and retry %d times failed \n",iMaxRetryCnt);
		return -ENODEV;
	}

	printk("%s():reg0=0x%x\n",__FUNCTION__,(unsigned char)iStatus);


	if (info) {
		strlcpy(info->type, "jd9930_sensor", I2C_NAME_SIZE);
	}

	printk("jd9930_detect success\n");
	return 0;
}

static const struct i2c_device_id jd9930_id[] = {
	{ "jd9930", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jd9930_id);

static const struct of_device_id jd9930_dt_ids[] = {
	{
		.compatible = "Fiti,jd9930",
		.data = (void *) &jd9930_id[0],
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, jd9930_dt_ids);

static const struct dev_pm_ops jd9930_dev_pm= {
	.suspend_late = jd9930_suspend_late,
	.resume_early = jd9930_resume_early,
	.suspend = jd9930_suspend,
	.resume = jd9930_resume,
};


static struct i2c_driver jd9930_driver = {
	.driver = {
		   .name = "jd9930",
		   .owner = THIS_MODULE,
		   .of_match_table = jd9930_dt_ids,
		   .pm = (&jd9930_dev_pm),
	},
	.probe = jd9930_probe,
	.remove = jd9930_remove,
	.id_table = jd9930_id,
	.detect = jd9930_detect,
	.address_list = &normal_i2c[0],
};

static int __init jd9930_init(void)
{
	return i2c_add_driver(&jd9930_driver);
}

static void __exit jd9930_exit(void)
{
	i2c_del_driver(&jd9930_driver);
}

struct jd9930 *jd9930_get(void)
{
	return gptJD9930;
}


/*
 * Module entry points
 */
subsys_initcall(jd9930_init);
module_exit(jd9930_exit);

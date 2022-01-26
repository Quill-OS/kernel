/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * Includes
 */
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/iomux-mx6q.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/regulator/consumer.h>

#include "../../../arch/arm/mach-mx6/ntx_hwconfig.h"

#include "../../../arch/arm/mach-mx6/board-mx6sl_ntx.h"

#define GDEBUG 0
#include <linux/gallen_dbg.h>

extern volatile NTX_HWCONFIG *gptHWCFG;

struct i2c_client *g_ht68f20_i2c_client;
struct regulator *g_fl_regulator;

int ht68f20_read_reg(unsigned char reg)
{
	int ret;
	unsigned char buffer[10];
	if(g_ht68f20_i2c_client) {
		struct i2c_msg msg[] = 
		{
			{.addr = g_ht68f20_i2c_client->addr, .flags = 0, .len = 1, .buf = &reg,}, 
			{.addr = g_ht68f20_i2c_client->addr, .flags = I2C_M_RD, .len = 1, .buf = buffer,},
		};
		if(0 > i2c_transfer(g_ht68f20_i2c_client->adapter, msg, 2))
			printk ("[%s-%d] i2c_transfer failed...\n", __func__, __LINE__);
	}
	else {
		printk("%s():ht68f20 not ready !\n",__FUNCTION__);
		buffer[0]=0;
	}
	return buffer[0];
}

int ht68f20_write_reg(unsigned char reg, int value)
{
	int iRet;
	unsigned char buffer[10];
	if(g_ht68f20_i2c_client) {
		struct i2c_msg msg[] = 
		{
			{.addr = g_ht68f20_i2c_client->addr, .flags = 0, .len = 2, .buf = buffer,}, 
		};
		buffer[0] = reg;
		buffer[1] = value & 0xFF;
		iRet = i2c_transfer(g_ht68f20_i2c_client->adapter, msg, 1);
	}
	else {
		printk("%s():ht68f20 not ready !\n",__FUNCTION__);
		iRet = -ENODEV;
	}
	
	return iRet;
}

unsigned short ht68f20_fw_version(void)
{
	return ht68f20_read_reg(0);
}

void ht68f20_enable(int isEnable)
{
	static int s_is_enabled;
	if (g_fl_regulator && (s_is_enabled != isEnable)) {
		s_is_enabled = isEnable;
		printk ("[%s-%d] regulator %s\n",__func__,__LINE__,(isEnable)?"on":"off");
		if (isEnable) {
  			regulator_enable (g_fl_regulator);
  			msleep (200);
  		}
		else
  			regulator_disable (g_fl_regulator);
	}
}

static __devinit int ht68f20_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int err = 0;

	g_fl_regulator = regulator_get(&client->dev, "vdd_pwm");
	if (IS_ERR(g_fl_regulator)) {
  		printk("%s, regulator \"vdd_pwm\" not registered.(%d)\n", __func__, g_fl_regulator);
	}
	else {
  		printk("%s, regulator found\n", __func__);
	}
  		
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
  		printk("%s, functionality check failed\n", __func__);
    		return -1;
  	}
	
	g_ht68f20_i2c_client = client;
	
	return 0;
}

static __devexit int ht68f20_i2c_remove(struct i2c_client *client)
{
	return 0;
}


static const struct i2c_device_id ht68f20_id[] = {
	{"ht68f20", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ht68f20_id);

static struct i2c_driver ht68f20_i2c_driver = {
	.driver = {
		   .name = "ht68f20",
		   .owner = THIS_MODULE,
		   },
	.probe = ht68f20_i2c_probe,
	.remove = __devexit_p(ht68f20_i2c_remove),
	.id_table = ht68f20_id,
};

static int __init ht68f20_init(void)
{
	return i2c_add_driver(&ht68f20_i2c_driver);
}

static void __exit ht68f20_exit(void)
{
	i2c_del_driver(&ht68f20_i2c_driver);
}

module_init(ht68f20_init);
module_exit(ht68f20_exit);

MODULE_DESCRIPTION("HT68F20 driver");
MODULE_AUTHOR("Netronix Inc.");
MODULE_LICENSE("GPL");

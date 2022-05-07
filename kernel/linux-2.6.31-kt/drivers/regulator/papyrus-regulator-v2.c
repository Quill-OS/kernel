/*
 * Copyright (C) 2010-2011 Amazon.com, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Based on Papyrus V1 driver */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/papyrus.h>
#include <linux/gpio.h>
#include <asm/div64.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#define PAPYRUS_V2_DEBUG	1

/*
 * PMIC Register Addresses
 */
enum {
    REG_PAPYRUS_EXT_TEMP = 0x0,
    REG_PAPYRUS_ENABLE,
    REG_PAPYRUS_VADJ,
    REG_PAPYRUS_VCOM1 = 0x3,
    REG_PAPYRUS_VCOM2,
    REG_PAPYRUS_INT_ENABLE1 = 0x5,
    REG_PAPYRUS_INT_ENABLE2,
    REG_PAPYRUS_INT1,
    REG_PAPYRUS_INT2 = 0x8,
    REG_PAPYRUS_UPSEQ0,
    REG_PAPYRUS_UPSEQ1,  /*0x0A*/
    REG_PAPYRUS_DWNSEQ0,
    REG_PAPYRUS_DWNSEQ1,
    REG_PAPYRUS_TMST_CONFIG = 0x0D,
    REG_PAPYRUS_TMST2_CONFIG,
    REG_PAPYRUS_PG_GOOD = 0x0f,
    REG_PAPYRUS_PRODUCT_REV = 0x10,
};

#define PAPYRUS_REG_NUM			21
#define PAPYRUS_V2_MAX_REGISTERS	0x10 
#define PAPYRUS_POWER_GOOD_STATUS	0xFA

#define PAPYRUS_ENABLE_REGS		0xbf
#define PAPYRUS_STANDBY_REGS		0x7f	

#define PAPYRUS_TEMP_THRESHOLD		60000	/* 60 seconds */
#define PAPYRUS_VCOM_FACTOR		10.78

#define PAPYRUS_DISPLAY_SETTLE_TIME 15 /* 15 ms */
#define PAPYRUS_SUSPEND_DISPLAY_SETTLE_TIME	30 /* 30ms */
#define PAPYRUS_MAX_ENABLE_RETRY 2 /* Number of retries to enable */

/*
 * Bitfield macros that use rely on bitfield width/shift information.
 */
#define BITFMASK(field) (((1U << (field ## _WID)) - 1) << (field ## _LSH))
#define BITFVAL(field, val) ((val) << (field ## _LSH))
#define BITFEXT(var, bit) ((var & BITFMASK(bit)) >> (bit ## _LSH))

/*
 * Shift and width values for each register bitfield
 */
#define EXT_TEMP_LSH    7
#define EXT_TEMP_WID    9

#define THERMAL_SHUTDOWN_LSH    0
#define THERMAL_SHUTDOWN_WID    1

#define INT_TEMP_LSH    7
#define INT_TEMP_WID    9

#define STAT_BUSY_LSH   0
#define STAT_BUSY_WID   1
#define STAT_OPEN_LSH   1
#define STAT_OPEN_WID   1
#define STAT_SHRT_LSH   2
#define STAT_SHRT_WID   1

#define PROD_REV_LSH    0
#define PROD_REV_WID    8

#define PROD_ID_LSH     0
#define PROD_ID_WID     8

#define VCOM1_LSH         0
#define VCOM1_WID         8

#define ENABLE_LSH      7
#define ENABLE_WID      1

#define STANDBY_LSH	6
#define STANDBY_WID	1

#define VCOM_ENABLE_LSH 4
#define VCOM_ENABLE_WID 1

/*
 * Regulator definitions
 *   *_MIN_uV  - minimum microvolt for regulator
 *   *_MAX_uV  - maximum microvolt for regulator
 *   *_STEP_uV - microvolts between regulator output levels
 *   *_MIN_VAL - minimum register field value for regulator
 *   *_MAX_VAL - maximum register field value for regulator
 */
#define PAPYRUS_GVDD_MIN_uV    5000000
#define PAPYRUS_GVDD_MAX_uV   20000000
#define PAPYRUS_GVDD_STEP_uV   1000000
#define PAPYRUS_GVDD_MIN_VAL         0
#define PAPYRUS_GVDD_MAX_VAL         1

#define PAPYRUS_GVEE_MIN_uV    5000000
#define PAPYRUS_GVEE_MAX_uV   20000000
#define PAPYRUS_GVEE_STEP_uV   1000000
#define PAPYRUS_GVEE_MIN_VAL         0
#define PAPYRUS_GVEE_MAX_VAL         1

#define PAPYRUS_VCOM_MIN_uV             0
#define PAPYRUS_VCOM_MAX_uV             511000 
#define PAPYRUS_VCOM_STEP_uV            10000
#define PAPYRUS_VCOM_MIN_VAL            0
#define PAPYRUS_VCOM_MAX_VAL            511
#define PAPYRUS_VCOM_FUDGE_FACTOR       0
#define PAPYRUS_VCOM_VOLTAGE_DEFAULT    205000	

#define PAPYRUS_VNEG_MIN_uV    5000000
#define PAPYRUS_VNEG_MAX_uV   20000000
#define PAPYRUS_VNEG_STEP_uV   1000000
#define PAPYRUS_VNEG_MIN_VAL         0
#define PAPYRUS_VNEG_MAX_VAL         1

#define PAPYRUS_VPOS_MIN_uV    5000000
#define PAPYRUS_VPOS_MAX_uV   20000000
#define PAPYRUS_VPOS_STEP_uV   1000000
#define PAPYRUS_VPOS_MIN_VAL         0
#define PAPYRUS_VPOS_MAX_VAL         1

#define PAPYRUS_TMST_MIN_uV	5000000
#define PAPYRUS_TMST_MAX_uV	20000000
#define PAPYRUS_TSMT_STEP_uV	1000000
#define PAPYRUS_TMST_MIN_VAL	0
#define PAPYRUS_TMST_MAX_VAL	1
#define PAPYRUS_TEMP_LOGGING	32


int papyrus_vcom_cur_voltage = PAPYRUS_VCOM_VOLTAGE_DEFAULT;

static int papyrus_gpio_wakeup = 1;

static void papyrus_gettemp_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(papyrus_temp_work, papyrus_gettemp_work);

struct papyrus {
	/* chip revision */
	int rev;

	struct device *dev;

	/* Platform connection */
	struct i2c_client *i2c_client;

	/* Client devices */
	struct platform_device *pdev[PAPYRUS_REG_NUM];

	/* GPIOs */
	int gpio_pmic_pwrgood;
	int pwrgood_irq;
	int gpio_pmic_vcom_ctrl;
	int gpio_pmic_wakeup;
	int gpio_pmic_intr;

	bool vcom_setup;
	bool is_suspended;
	bool is_shutdown;
	
	int max_wait;
	int num_disp_enabled;
	int num_vcom_enabled;
};

/*this is saved from the probe and used in gpio_wakeup sys entry only*/
static int gpio_pmic_wakeup_saved; 

DEFINE_MUTEX(papyrus_display_mutex);
DEFINE_MUTEX(papyrus_vcom_mutex);

struct papyrus *g_papyrus = NULL;

/*
 * helper function that do i2c communications, does not do any conversions
 * */
static int papyrus_i2c_read_vcom(struct i2c_client *client, int* return_vcom)
{
        int vcom1 = 0;
        int vcom2 = 0;
        vcom1 = i2c_smbus_read_byte_data(client, REG_PAPYRUS_VCOM1);
        vcom2 = i2c_smbus_read_byte_data(client, REG_PAPYRUS_VCOM2);
        
        if(vcom1 < 0 || vcom2 < 0)
        {
                printk(KERN_ERR "KERNEL: E papyrus: %s_failed:vcom1=%d,vcom2=%d:", __FUNCTION__, vcom1, vcom2);
                return ((vcom1 < 0)?vcom1:vcom2);
        }
        
        /* vcom = vcom2[0]vcom1[7:0] */
        vcom2 &= 0x1; 
        *return_vcom = vcom2 << 8 | vcom1;
        return 0;
}

/*
 * helper function that do i2c communications, does not do any conversions
 * */
static int papyrus_i2c_write_vcom(struct i2c_client *client, int vcom_to_set)
{
        int vcom1 = 0;
        int vcom2 = 0;
        int ret = 0;

        vcom2 = i2c_smbus_read_byte_data(client, REG_PAPYRUS_VCOM2);
        
        if(vcom2 < 0)
        {
                printk(KERN_ERR "KERNEL: E papyrus: %s_failed:vcom2=%d:", __FUNCTION__, vcom2);
                return vcom2;
        }
        
        vcom2 &= 0xFE; /*clear last bit*/
        vcom1 = 0xFF & vcom_to_set; /*lower 8 bits*/
        vcom2 |= ((0x0100 & vcom_to_set) >> 8); /*get 9th bit, shift to bit 0*/
        
        if((ret = i2c_smbus_write_byte_data(client, REG_PAPYRUS_VCOM1, vcom1)) < 0
                || (ret = i2c_smbus_write_byte_data(client, REG_PAPYRUS_VCOM2, vcom2)) < 0)
        {       
                return ret; 
        }
        
        return 0;                
}

static int papyrus_vcom_set_voltage(struct regulator_dev *reg,
					int minuV, int uV)
{
	struct papyrus *papyrus = rdev_get_drvdata(reg);
	struct i2c_client *client = papyrus->i2c_client;
	int ret;
	int reg_val;
	int vcom_read;

	if ((uV < PAPYRUS_VCOM_MIN_uV) || (uV > PAPYRUS_VCOM_MAX_uV))
		return -EINVAL;
	
	if ((ret=papyrus_i2c_read_vcom(client, &reg_val)) < 0) // VCOM2[0]VCOM1[0:7]
		return ret;
	/*
	 * Only program VCOM if it is not set to the desired value.
	 * Programming VCOM excessively degrades ability to keep
	 * DVR register value persistent.
	 */
    
	vcom_read = (reg_val) * 1000;
	if (vcom_read != uV) {
		reg_val = (uV/1000);
		if ((ret = papyrus_i2c_write_vcom(client, reg_val)) < 0) // VCOM2[0]VCOM1[0:7]
			return ret;
                        
		papyrus_vcom_cur_voltage = uV;
	}

	return 0;
}

static int papyrus_vcom_get_voltage(struct regulator_dev *reg)
{
	struct papyrus *papyrus = rdev_get_drvdata(reg);
	struct i2c_client *client = papyrus->i2c_client;
	int reg_val;
	int ret;

	if ((ret=papyrus_i2c_read_vcom(client, &reg_val)) < 0) // VCOM2[0]VCOM1[0:7]
		return ret;
	return ((reg_val) * 10);
}

static int papyrus_vcom_enable(struct regulator_dev *reg)
{
	struct papyrus *papyrus = rdev_get_drvdata(reg);
	int ret = 0;

	mutex_lock(&papyrus_display_mutex);
	mutex_lock(&papyrus_vcom_mutex);        
	if (!papyrus_gpio_wakeup || papyrus->is_shutdown)
	{
		ret = -1;
		goto out;
	}
	
	if (papyrus->is_suspended || !papyrus->num_disp_enabled)
	{
		printk(KERN_ERR "Attempting to enable VCOM while Papyrus disabled\n");
		ret = -1;
		goto out;
	}

        /*
	 * Check to see if we need to set the VCOM voltage.
	 * We can only change vcom voltage if we have been enabled.
	 * TEQ-1762 - Jack says we need to set VCOM on every power up
	 */
	if (papyrus_vcom_set_voltage(reg,
				papyrus_vcom_cur_voltage,
				papyrus_vcom_cur_voltage) < 0)
	{
		ret = -1;
		goto out;
	}
        
	/* Turn on the GPIO */
	gpio_set_value(papyrus->gpio_pmic_vcom_ctrl, 1);
    papyrus->num_vcom_enabled++;
out:	
	mutex_unlock(&papyrus_vcom_mutex);
	mutex_unlock(&papyrus_display_mutex);
	return ret;
}

static int papyrus_vcom_disable(struct regulator_dev *reg)
{
	struct papyrus *papyrus   = rdev_get_drvdata(reg);

	mutex_lock(&papyrus_display_mutex);
	mutex_lock(&papyrus_vcom_mutex);
	if (!papyrus_gpio_wakeup || !papyrus->num_vcom_enabled)
		goto out;

	papyrus->num_vcom_enabled--;
	if (papyrus->num_vcom_enabled)
		goto out;

	/* Turn off the GPIO */
	gpio_set_value(papyrus->gpio_pmic_vcom_ctrl, 0);
 
out:
	mutex_unlock(&papyrus_vcom_mutex);
	mutex_unlock(&papyrus_display_mutex);
	return 0;
}

static int papyrus_tmst_enable(struct regulator_dev *reg)
{
	/* Papyrus already in standby */
	return 0;
}

static int papyrus_tmst_disable(struct regulator_dev *reg)
{
	/* Papyrus already in standby */
	return 0;
}

static int papyrus_tmst_get_temp(struct regulator_dev *reg)
{
	struct papyrus *papyrus = rdev_get_drvdata(reg);
	struct i2c_client *client = papyrus->i2c_client;
	int reg_val;
	int i = 0;

	i2c_smbus_write_byte_data(client, REG_PAPYRUS_TMST_CONFIG, 0x80);
	reg_val = i2c_smbus_read_byte_data(client, REG_PAPYRUS_TMST_CONFIG);

	for (i=0; i < papyrus->max_wait; i++) {
		if (reg_val & 0x20)
			break;
		msleep(20);
		reg_val = i2c_smbus_read_byte_data(client, REG_PAPYRUS_TMST_CONFIG);
	}	

	if (i == papyrus->max_wait)
		return -ETIMEDOUT;

	reg_val = i2c_smbus_read_byte_data(client, REG_PAPYRUS_EXT_TEMP);
	return reg_val;
}	

static int papyrus_wait_power_good(struct papyrus *papyrus)
{
	int i;

	for (i = 0; i < papyrus->max_wait; i++) {
		if (gpio_get_value(papyrus->gpio_pmic_pwrgood))
			return 0;
		msleep(1);
	}

	printk(KERN_ERR "kernel: E papyrus:Power_good_timed_out::\n");
	return -ETIMEDOUT;
}

static int papyrus_wait_power_not_good(struct papyrus *papyrus)
{
	struct i2c_client *client = papyrus->i2c_client;
	unsigned int reg_val;
	int i;

	reg_val = i2c_smbus_read_byte_data(client, REG_PAPYRUS_PG_GOOD);
	for (i = 0; i < papyrus->max_wait; i++)
	{
		if (reg_val == 0)
			return reg_val;
		msleep(1);
		reg_val = i2c_smbus_read_byte_data(client, REG_PAPYRUS_PG_GOOD);
	}
	printk(KERN_ERR "kernel: E papyrus:Power_not_good_timed_out::\n");
	return -ETIMEDOUT;
}

static int papyrus_display_is_enabled(struct regulator_dev *reg)
{
	struct papyrus *papyrus = rdev_get_drvdata(reg);
	int reg_val = 0;
	if (papyrus->is_shutdown)
		return -1;
	reg_val = i2c_smbus_read_byte_data(papyrus->i2c_client, REG_PAPYRUS_PG_GOOD);
	if (reg_val == PAPYRUS_POWER_GOOD_STATUS && gpio_get_value(papyrus->gpio_pmic_pwrgood))
		return 1;
	
	return 0;
}

static void papyrus_sleep(void)
{
	struct papyrus *papyrus = g_papyrus;

	if (!papyrus || papyrus->is_suspended)
		return; 
	/* Pull down the GPIO pin for sleep mode */
	/* VCOM will be cleared and will need to be reset */
	papyrus->vcom_setup = false;
	papyrus->is_suspended = true;
	papyrus->num_disp_enabled = 0;
	papyrus->num_vcom_enabled = 0;
	gpio_set_value(papyrus->gpio_pmic_vcom_ctrl, 0);
	gpio_set_value(papyrus->gpio_pmic_wakeup, 0);
	/* Technically we should be waiting 3 seconds here */
}

static int papyrus_enable(struct papyrus *papyrus)
{
	struct i2c_client *client = papyrus->i2c_client;
	int retry = PAPYRUS_MAX_ENABLE_RETRY;
	int ret = 0;

	if (!papyrus->num_disp_enabled)
	{
		while(retry)
		{
			if ((ret = i2c_smbus_write_byte_data(client, REG_PAPYRUS_ENABLE, PAPYRUS_ENABLE_REGS)) < 0)
			{
				retry--;
				msleep(5);
				continue;
			}
			break;
		}
	}
	if (!retry) {
		printk(KERN_ERR "Papyrus: I2C Send ENABLE failed! : %d\n", ret);
		return -1;
	}

	papyrus->num_disp_enabled++;
	return ret;
}

static irqreturn_t papyrus_irq_handler(int irq, void *devid)
{
	struct papyrus *papyrus = g_papyrus;
	
	// Papyrus is in the shutdown state. Don't get out of suspend.
	if (!papyrus->is_shutdown && gpio_get_value(papyrus->gpio_pmic_pwrgood))
		papyrus->is_suspended = false;
	
	return IRQ_HANDLED;
}

static int papyrus_wake(void)
{
	struct papyrus *papyrus = g_papyrus;
	int ret = 0;
	int retry = PAPYRUS_MAX_ENABLE_RETRY;
	struct i2c_client *client = papyrus->i2c_client;
	
	if (!papyrus)
		return -1;

	while(retry)
	{
		/* Turn on Wakeup GPIO . Will be on till next suspend */
		gpio_set_value(papyrus->gpio_pmic_wakeup, 1);
		papyrus->is_shutdown = false;
		
		/* Wait for I2C to be ready */
		if (papyrus->is_suspended)
			msleep(PAPYRUS_SUSPEND_DISPLAY_SETTLE_TIME);

		/* Papyrus enable success ? */
		if(!(ret = papyrus_enable(papyrus)))
			break;
		
		/* Failed. Reset and Retry */
		printk(KERN_ERR "Retrying Papyrus enable\n");
		retry--;
	}

	if (!retry)
		return ret;

	return 0;
}

static int papyrus_standby(void)
{
	struct papyrus *papyrus = g_papyrus;
	struct i2c_client *client = papyrus->i2c_client;
	int ret = 0;

	if (papyrus->is_shutdown || papyrus->is_suspended || !papyrus->num_disp_enabled)
		goto out;
	
	papyrus->num_disp_enabled--;
	if (papyrus->num_disp_enabled > 0)
		goto out;

    /*display mutex is locked in display_disable()*/
	mutex_lock(&papyrus_vcom_mutex);
	ret = i2c_smbus_write_byte_data(client, REG_PAPYRUS_ENABLE, PAPYRUS_STANDBY_REGS);
	mutex_unlock(&papyrus_vcom_mutex);
out:
	return ret;
}

static int papyrus_display_enable(struct regulator_dev *reg)
{
	int ret = 0;

	mutex_lock(&papyrus_display_mutex);
	
	if (!papyrus_gpio_wakeup || g_papyrus->is_shutdown)
	{
		ret = -1;
		goto out;
	}

	ret = papyrus_wake();

out:
	mutex_unlock(&papyrus_display_mutex);
	return ret;
}

static int papyrus_display_disable(struct regulator_dev *reg)
{
	int ret = 0;

	mutex_lock(&papyrus_display_mutex);
	if (!papyrus_gpio_wakeup)
		goto out;
	// Set papyrus to standby
	ret = papyrus_standby();

out:
	mutex_unlock(&papyrus_display_mutex);
	return ret;
}

static int papyrus_suspend(struct i2c_client *client, pm_message_t state)
{
	struct papyrus *papyrus = i2c_get_clientdata(client);
	
	cancel_delayed_work_sync(&papyrus_temp_work);
	disable_irq(papyrus->pwrgood_irq);
	mutex_lock(&papyrus_display_mutex);
	mutex_lock(&papyrus_vcom_mutex);
	papyrus->is_shutdown = true;
	papyrus_sleep();
	mutex_unlock(&papyrus_vcom_mutex);
	mutex_unlock(&papyrus_display_mutex);
	return 0;
}
	
static int papyrus_resume(struct i2c_client *client)
{
	struct papyrus *papyrus = i2c_get_clientdata(client);
	
	enable_irq(papyrus->pwrgood_irq);
	/* Restart the temp collection worker now */
	schedule_delayed_work(&papyrus_temp_work, msecs_to_jiffies(0));	
	return 0;
}

/*
 * Regulator operations
 */
static struct regulator_ops papyrus_display_ops = {
	.enable = papyrus_display_enable,
	.disable = papyrus_display_disable,
	.is_enabled = papyrus_display_is_enabled,
};

static struct regulator_ops papyrus_gvdd_ops = {
};

static struct regulator_ops papyrus_gvee_ops = {
};

static struct regulator_ops papyrus_vcom_ops = {
	.enable = papyrus_vcom_enable,
	.disable = papyrus_vcom_disable,
	.get_voltage = papyrus_vcom_get_voltage,
	.set_voltage = papyrus_vcom_set_voltage,
};

static struct regulator_ops papyrus_vneg_ops = {
};

static struct regulator_ops papyrus_vpos_ops = {
};

static struct regulator_ops papyrus_tmst_ops = {
	.get_voltage = papyrus_tmst_get_temp,
	.enable = papyrus_tmst_enable,
	.disable = papyrus_tmst_disable,
};

/*
 * Regulator descriptors
 */
static struct regulator_desc papyrus_reg[PAPYRUS_NUM_REGULATORS] = {
{
	.name = "DISPLAY",
	.id = PAPYRUS_DISPLAY,
	.ops = &papyrus_display_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "GVDD",
	.id = PAPYRUS_GVDD,
	.ops = &papyrus_gvdd_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "GVEE",
	.id = PAPYRUS_GVEE,
	.ops = &papyrus_gvee_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "VCOM",
	.id = PAPYRUS_VCOM,
	.ops = &papyrus_vcom_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "VNEG",
	.id = PAPYRUS_VNEG,
	.ops = &papyrus_vneg_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "VPOS",
	.id = PAPYRUS_VPOS,
	.ops = &papyrus_vpos_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "TMST",
	.id = PAPYRUS_TMST,
	.ops = &papyrus_tmst_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
};

/*
 * Regulator init/probing/exit functions
 */
static int papyrus_regulator_probe(struct platform_device *pdev)
{
	struct regulator_dev *rdev;

	rdev = regulator_register(&papyrus_reg[pdev->id], &pdev->dev,
				  pdev->dev.platform_data,
				  dev_get_drvdata(&pdev->dev));

	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "failed to register %s\n",
			papyrus_reg[pdev->id].name);
		return PTR_ERR(rdev);
	}

	return 0;
}

static int papyrus_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	regulator_unregister(rdev);
	return 0;
}

static struct platform_driver papyrus_regulator_driver = {
	.probe = papyrus_regulator_probe,
	.remove = papyrus_regulator_remove,
	.driver = {
		.name = "papyrus-reg",
	},
};

static int papyrus_register_regulator(struct papyrus *papyrus, int reg,
				     struct regulator_init_data *initdata)
{
	struct platform_device *pdev;
	int ret;

	struct i2c_client *client = papyrus->i2c_client;

	ret = i2c_smbus_read_byte_data(client, 0x10);

	/* If we can't find PMIC via I2C, we should not register regulators */
	if (ret < 0) {
		dev_err(papyrus->dev,
			"Papyrus PMIC not found!\n");
		return -ENXIO;
	}

	if (papyrus->pdev[reg])
		return -EBUSY;

	pdev = platform_device_alloc("papyrus-reg", reg);
	if (!pdev)
		return -ENOMEM;

	papyrus->pdev[reg] = pdev;

	initdata->driver_data = papyrus;

	pdev->dev.platform_data = initdata;
	pdev->dev.parent = papyrus->dev;
	platform_set_drvdata(pdev, papyrus);

	ret = platform_device_add(pdev);

	if (ret != 0) {
		dev_err(papyrus->dev,
		       "Failed to register regulator %d: %d\n",
			reg, ret);
		platform_device_del(pdev);
		papyrus->pdev[reg] = NULL;
	}

	return ret;
}

struct regulator *temp_regulator, *display_regulator, *vcom_regulator;
signed int papyrus_temp = 25;
EXPORT_SYMBOL(papyrus_temp);

static ssize_t show_papyrus_gpio_wakeup(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", papyrus_gpio_wakeup);
}

static ssize_t
store_papyrus_gpio_wakeup(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	volatile int wakeup = 0;
	
	if (sscanf(buf, "%d", &wakeup) > 0) {
		if(0 == wakeup)
		{
			mutex_lock(&papyrus_display_mutex);
			mutex_lock(&papyrus_vcom_mutex);
			papyrus_gpio_wakeup = wakeup;
			papyrus_sleep();
			mutex_unlock(&papyrus_vcom_mutex);
			mutex_unlock(&papyrus_display_mutex);
		}else if(1 == wakeup)
		{
			mutex_lock(&papyrus_display_mutex);
			mutex_lock(&papyrus_vcom_mutex);
			papyrus_gpio_wakeup = wakeup;
			papyrus_wake();
			papyrus_wait_power_good(g_papyrus);
			mutex_unlock(&papyrus_vcom_mutex);
			mutex_unlock(&papyrus_display_mutex);
		}else
		{
			printk(KERN_ERR "invalid gpio_wakeup echo\n");
		}
		return strlen(buf);
	}else
	{
		printk(KERN_ERR "invalid gpio_wakeup echo\n");
	}

	return -EINVAL;
}
static DEVICE_ATTR(papyrus_gpio_wakeup, 0666, show_papyrus_gpio_wakeup, store_papyrus_gpio_wakeup);

static ssize_t show_papyrus_vcom_voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = -EINVAL;
	int volt = 0;

	ret = regulator_enable(display_regulator);
	if (ret < 0)
	{
		return sprintf(buf, "%d\n", ret);
	}
	if (papyrus_wait_power_good(g_papyrus) < 0)
	{
		regulator_disable(display_regulator);
		return sprintf(buf, "%d\n", ret);
	}
  
	ret = regulator_enable(vcom_regulator);
	if (ret < 0)
	{
		regulator_disable(display_regulator);
		return sprintf(buf, "%d\n", ret);
	}

	volt = regulator_get_voltage(vcom_regulator) ;

	regulator_disable(vcom_regulator);
	regulator_disable(display_regulator);
	return sprintf(buf, "%d\n", volt);
}

static ssize_t
store_papyrus_vcom_voltage(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int volt = 0;
	
	if (sscanf(buf, "%d", &volt) > 0) {
		if (regulator_enable(display_regulator) < 0)
			return -EINVAL;
		if (papyrus_wait_power_good(g_papyrus) < 0)
		{
			regulator_disable(display_regulator);
			return -EINVAL;
		}

		if (regulator_enable(vcom_regulator) < 0)
		{
			regulator_disable(display_regulator);
			return -EINVAL;
		}
                
        volt *= 100;
		if (regulator_set_voltage(vcom_regulator, volt, volt) < 0) {
			printk(KERN_ERR "Error setting Papyrus VCOM to %d\n", volt);
			regulator_disable(vcom_regulator);		
			regulator_disable(display_regulator);
			return -EINVAL;
		}

		regulator_disable(vcom_regulator);		
		regulator_disable(display_regulator);

		return strlen(buf);
	}

	return -EINVAL;
}
static DEVICE_ATTR(papyrus_vcom_voltage, 0666, show_papyrus_vcom_voltage, store_papyrus_vcom_voltage);

static ssize_t
show_papyrus_temperature(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", papyrus_temp);
}
static DEVICE_ATTR(papyrus_temperature, 0444, show_papyrus_temperature, NULL);

union TEMP{
        signed char temp[4];
        int ret_val; 
};

static void papyrus_gettemp_work(struct work_struct *work)
{
	union TEMP temp;

	if (!g_papyrus || !papyrus_gpio_wakeup)
		return;
	
	mutex_lock(&papyrus_display_mutex);

	if (g_papyrus->is_suspended == true)
	{
		/* Wakeup papyrus, read temp and go to standby */
		papyrus_wake();
		if (papyrus_wait_power_good(g_papyrus) < 0)
		{
			mutex_unlock(&papyrus_display_mutex);
			goto out;
		}
		temp.ret_val = regulator_get_voltage(temp_regulator);
		papyrus_standby();
	}
	else
	{
		/* Papyrus is already awake. Just get the temp */
		temp.ret_val = regulator_get_voltage(temp_regulator);
	}
	mutex_unlock(&papyrus_display_mutex);	
	
	if(temp.ret_val >= 0) {
		papyrus_temp = temp.temp[0];
	}else {
		printk(KERN_ERR "kernel: E papyrus:TempReadFailed:temp=%d:\n", temp.ret_val);
	}
        
	if(papyrus_temp >= 85 || papyrus_temp <= -10) {
		printk(KERN_DEBUG "kernel: I papyrus:TempExceeded:temp=%d:out of hardware range\n", papyrus_temp);        
	}
	       
	if (papyrus_temp >= PAPYRUS_TEMP_LOGGING) {	
		printk(KERN_DEBUG "kernel: I papyrus:TempExceeded:temp=%d:\n", papyrus_temp);
	}
out:
	schedule_delayed_work(&papyrus_temp_work, msecs_to_jiffies(PAPYRUS_TEMP_THRESHOLD));
}

static int papyrus_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int i;
	struct papyrus *papyrus;
	struct papyrus_platform_data *pdata = client->dev.platform_data;
	int ret = 0;

	if (!pdata || !pdata->regulator_init)
		return -ENODEV;

	/* Create the PMIC data structure */
	papyrus = kzalloc(sizeof(struct papyrus), GFP_KERNEL);
	if (papyrus == NULL) {
		kfree(client);
		return -ENOMEM;
	}
  
	/* Initialize the PMIC data structure */
	i2c_set_clientdata(client, papyrus);
	papyrus->dev = &client->dev;
	papyrus->i2c_client = client;

	papyrus->gpio_pmic_pwrgood = pdata->gpio_pmic_pwrgood;
	papyrus->pwrgood_irq = pdata->pwrgood_irq;
	papyrus->gpio_pmic_vcom_ctrl = pdata->gpio_pmic_vcom_ctrl;
	papyrus->gpio_pmic_wakeup = pdata->gpio_pmic_wakeup;
	papyrus->gpio_pmic_intr = pdata->gpio_pmic_intr;

	papyrus->is_shutdown  = true;
	papyrus->is_suspended = true;
	papyrus->vcom_setup		= false; 
	papyrus->num_disp_enabled = 0;
	papyrus->num_vcom_enabled = 0;
	papyrus->max_wait = 100;

	gpio_pmic_wakeup_saved = papyrus->gpio_pmic_wakeup;
	g_papyrus = papyrus;
	mutex_init(&papyrus_vcom_mutex);
	mutex_init(&papyrus_display_mutex);
	
	/* Put Papyrus in standby */
	mutex_lock(&papyrus_display_mutex);
	papyrus_wake();
	if ((ret = papyrus_wait_power_good(g_papyrus)) < 0)
	{
		dev_err(papyrus->dev, "Papyrus: Probe fail wait power good\n");
		ret = -EBUSY;
		goto out;
	}
	papyrus->is_suspended = false;
	papyrus->is_shutdown = false;
	
	ret = platform_driver_register(&papyrus_regulator_driver);
	if (ret < 0)
		goto out;

	for (i = 0; i <= PAPYRUS_TMST; i++) {
		ret = papyrus_register_regulator(papyrus, i, &pdata->regulator_init[i]);
		if (ret != 0) {
			dev_err(papyrus->dev, "Platform init() failed: %d\n",
			ret);
			goto out;
		}
	}
	
	papyrus_standby();
	mutex_unlock(&papyrus_display_mutex);
 
	/* Initialize the PMIC device */
	dev_info(&client->dev, "PMIC PAPYRUS for eInk display\n");

	/* Get the temperature reg */
	temp_regulator = regulator_get(NULL, "TMST");
	if (IS_ERR(temp_regulator)) {
		printk(KERN_ERR "Could not get the TMST regulator\n");
	}

	display_regulator = regulator_get(NULL, "DISPLAY");
	if (IS_ERR(display_regulator)) {
		printk(KERN_ERR "Could not get the DISPLAY regulator\n");
	}

	vcom_regulator = regulator_get(NULL, "VCOM");
	if (IS_ERR(vcom_regulator)) {
		printk(KERN_ERR "Could not get the VCOM regulator\n");
	}

	if (device_create_file(&client->dev, &dev_attr_papyrus_temperature) < 0)
		printk(KERN_ERR "(%s): Could not create papyrus_temperature file\n", __FUNCTION__);

	if (device_create_file(&client->dev, &dev_attr_papyrus_vcom_voltage) < 0)
		printk(KERN_ERR "(%s): Could not create papyrus_vcom_voltage file\n", __FUNCTION__);

	if (device_create_file(&client->dev, &dev_attr_papyrus_gpio_wakeup) < 0)
		printk(KERN_ERR "(%s): Could not create papyrus_gpio_wakeup file\n", __FUNCTION__);

	set_irq_type(papyrus->pwrgood_irq, IRQF_TRIGGER_RISING);
	if (request_irq(papyrus->pwrgood_irq, papyrus_irq_handler, IRQF_DISABLED|IRQF_NODELAY, "pap_pwrgood", NULL))
	{
		printk(KERN_ERR "Could not get Papyrus IRQ\n");
		ret = -EBUSY;
		goto err;
	}
	schedule_delayed_work(&papyrus_temp_work, msecs_to_jiffies(0));	

	return ret;
out:
	mutex_unlock(&papyrus_display_mutex);
err:
	kfree(papyrus);

	return ret;
}


static int papyrus_i2c_remove(struct i2c_client *i2c)
{
	struct papyrus *papyrus = i2c_get_clientdata(i2c);
	int i;

	cancel_delayed_work_sync(&papyrus_temp_work);

	for (i = 0; i < ARRAY_SIZE(papyrus->pdev); i++)
		platform_device_unregister(papyrus->pdev[i]);

	device_remove_file(&i2c->dev, &dev_attr_papyrus_temperature);
	device_remove_file(&i2c->dev, &dev_attr_papyrus_vcom_voltage);
	device_remove_file(&i2c->dev, &dev_attr_papyrus_gpio_wakeup);

	platform_driver_unregister(&papyrus_regulator_driver);
	regulator_put(temp_regulator);
	regulator_put(display_regulator);
	regulator_put(vcom_regulator);

	kfree(papyrus);

	return 0;
}

static const struct i2c_device_id papyrus_i2c_id[] = {
       { "papyrus", 0 },
       { }
};
MODULE_DEVICE_TABLE(i2c, papyrus_i2c_id);


static struct i2c_driver papyrus_i2c_driver = {
	.driver = {
		   .name = "papyrus",
		   .owner = THIS_MODULE,
	},
	.probe = papyrus_i2c_probe,
	.remove = papyrus_i2c_remove,
	.suspend = papyrus_suspend,
	.resume = papyrus_resume,
	.id_table = papyrus_i2c_id,
};

static int __init papyrus_init(void)
{
	return i2c_add_driver(&papyrus_i2c_driver);
}
module_init(papyrus_init);

static void __exit papyrus_exit(void)
{
	i2c_del_driver(&papyrus_i2c_driver);
}
module_exit(papyrus_exit);

/* Module information */
MODULE_DESCRIPTION("PAPYRUS regulator driver");
MODULE_LICENSE("GPL");

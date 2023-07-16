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
 * @file pmic/core/fp9928.c
 * @brief This file contains FP9928 specific PMIC code. This implementaion
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

#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/pmic_status.h>
#include <linux/mfd/core.h>
#include <linux/mfd/fp9928.h>
#include <asm/mach-types.h>

/*
 * EPDC PMIC I2C address
 * PAPYRUS II 1p1 and later uses 0x68, others 0x48
 */
#define EPDC_PMIC_I2C_ADDR 0x68

static struct mfd_cell fp9928_devs[] = {
	{ .name = "fp9928-pmic", },
	{ .name = "fp9928-sns", },
};

static const unsigned short normal_i2c[] = {EPDC_PMIC_I2C_ADDR, I2C_CLIENT_END};

int fp9928_reg_read(struct fp9928 *fp9928,int reg_num, unsigned int *reg_val)
{
	int result;
	struct i2c_client *fp9928_client = fp9928->i2c_client;


	if (fp9928_client == NULL) {
		dev_err(&fp9928_client->dev,
			"fp9928 I2C adaptor not ready !\n");
		return PMIC_ERROR;
	}

	result = i2c_smbus_read_byte_data(fp9928_client, reg_num);
	if (result < 0) {
		dev_err(&fp9928_client->dev,
			"Unable to read fp9928 register%d via I2C\n",reg_num);
		return PMIC_ERROR;
	}

	*reg_val = result;
	return PMIC_SUCCESS;
}

int fp9928_reg_write(struct fp9928 *fp9928,int reg_num, const unsigned int reg_val)
{
	int result;
	struct i2c_client *fp9928_client=fp9928->i2c_client;

	if (fp9928_client == NULL) {
		dev_err(&fp9928_client->dev,
			"fp9928 I2C adaptor not ready !\n");
		return PMIC_ERROR;
	}

	result = i2c_smbus_write_byte_data(fp9928_client, reg_num, reg_val);
	if (result < 0) {
		dev_err(&fp9928_client->dev,
			"Unable to write FP9928 register%d via I2C\n",reg_num);
		return PMIC_ERROR;
	}

	return PMIC_SUCCESS;
}

#ifdef CONFIG_OF
static struct fp9928_platform_data *fp9928_i2c_parse_dt_pdata(
					struct device *dev)
{
	struct fp9928_platform_data *pdata;
	struct device_node *pmic_np;
	struct fp9928 *fp9928 = dev_get_drvdata(dev);
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

	fp9928->gpio_pmic_pwrall = of_get_named_gpio(pmic_np,
					"gpio_pmic_pwrall", 0);
	if (!gpio_is_valid(fp9928->gpio_pmic_pwrall)) {
		dev_err(dev, "no epdc pmic pwrall pin available\n");
		return ERR_PTR(-ENODEV);
	}
	else {
		printk("%s():gpio_pmic_pwrall=%d\n",__FUNCTION__,fp9928->gpio_pmic_pwrall);
	}
	ret = devm_gpio_request_one(dev, fp9928->gpio_pmic_pwrall,
				GPIOF_OUT_INIT_HIGH, "epdc-pmic-pwrall");
	if (ret < 0) {
		dev_err(dev, "request pwrall gpio failed (%d)!\n",ret);
		//goto err;
	}

	return pdata;
}
#else
static struct fp9928_platform_data *fp9928_i2c_parse_dt_pdata(
					struct device *dev)
{
	return NULL;
}
#endif	/* !CONFIG_OF */

static int fp9928_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct fp9928 *fp9928;
	struct fp9928_platform_data *pdata = client->dev.platform_data;
	struct device_node *np = client->dev.of_node;
	int ret = 0;

	if (!np)
		return -ENODEV;

	/* Create the PMIC data structure */
	fp9928 = kzalloc(sizeof(struct fp9928), GFP_KERNEL);
	if (fp9928 == NULL) {
		kfree(client);
		return -ENOMEM;
	}

	/* Initialize the PMIC data structure */
	i2c_set_clientdata(client, fp9928);
	fp9928->dev = &client->dev;
	fp9928->i2c_client = client;

	if (fp9928->dev->of_node) {
		pdata = fp9928_i2c_parse_dt_pdata(fp9928->dev);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			goto err1;
		}
	}

	if (!gpio_is_valid(fp9928->gpio_pmic_pwrall)) {
		dev_err(&client->dev, "pwrall gpio not available !!\n");
		goto err1;
	}
	if (!gpio_get_value(fp9928->gpio_pmic_pwrall)) {
		dev_info(&client->dev, "turning on the PMIC chip power ...\n");
		gpio_set_value(fp9928->gpio_pmic_pwrall,1);
		mdelay(1);
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "fp9928 cannot probe from i2c !!\n");
		return -ENODEV;
	}

	mfd_add_devices(fp9928->dev, -1, fp9928_devs,
			ARRAY_SIZE(fp9928_devs),
			NULL, 0, NULL);

	fp9928->pdata = pdata;

	dev_info(&client->dev, "PMIC FP9928 for eInk display\n");

	return ret;

err1:
	kfree(fp9928);

	return ret;
}


static int fp9928_remove(struct i2c_client *i2c)
{
	struct fp9928 *fp9928 = i2c_get_clientdata(i2c);

	mfd_remove_devices(fp9928->dev);

	return 0;
}

static int fp9928_suspend(struct device *dev)
{
	struct i2c_client *client = i2c_verify_client(dev);    
	struct fp9928 *fp9928 = i2c_get_clientdata(client);
	gpio_set_value(fp9928->gpio_pmic_vcom_ctrl,0);
	gpio_set_value(fp9928->gpio_pmic_powerup,0);
	gpio_set_value(fp9928->gpio_pmic_pwrall,0);
	return 0;
}

static int fp9928_resume(struct device *dev)
{
	struct i2c_client *client = i2c_verify_client(dev);    
	struct fp9928 *fp9928 = i2c_get_clientdata(client);
	gpio_set_value(fp9928->gpio_pmic_pwrall,1);
	return 0;
}

static const struct i2c_device_id fp9928_id[] = {
	{ "fp9928", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fp9928_id);

static const struct of_device_id fp9928_dt_ids[] = {
	{
		.compatible = "fiti,fp9928",
		.data = (void *) &fp9928_id[0],
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, fp9928_dt_ids);

static const struct dev_pm_ops fp9928_dev_pm= {
	.suspend_late = fp9928_suspend,
	.resume_early = fp9928_resume,
};

static struct i2c_driver fp9928_driver = {
	.driver = {
		   .name = "fp9928",
		   .owner = THIS_MODULE,
		   .of_match_table = fp9928_dt_ids,
		   .pm = (&fp9928_dev_pm),
	},
	.probe = fp9928_probe,
	.remove = fp9928_remove,
	.id_table = fp9928_id,
	.address_list = &normal_i2c[0],
};

static int __init fp9928_init(void)
{
	return i2c_add_driver(&fp9928_driver);
}

static void __exit fp9928_exit(void)
{
	i2c_del_driver(&fp9928_driver);
}



/*
 * Module entry points
 */
subsys_initcall(fp9928_init);
module_exit(fp9928_exit);

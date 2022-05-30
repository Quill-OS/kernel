/*
 * Voltage regulation for the TWL6025 Smart Reflex
 *
 * Copyright (C) 2011 Texas Instruments Inc
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/i2c/twl.h>

#define DRIVER_NAME			"twl-smartreflex"

#define TWL_SMARTREFLEX_SMPS1 0
#define TWL_SMARTREFLEX_SMPS2 1

/* flags for SMPS voltages */
#define DCDC_OFFSET_EN		BIT(0)
#define DCDC_EXTENDED_EN	BIT(1)

#define SMPS_MULTOFFSET_SMPS1	BIT(3)
#define SMPS_MULTOFFSET_SMPS2	BIT(4)

#define SMPS1_CFG_VOLTAGE	0x55
#define SMPS2_CFG_VOLTAGE	0x5B

#define DCDC_VSEL_MASK		0x3F

struct twl_smartreflex_info {
	struct i2c_client *client;
	struct regulator_dev *rdev;
	struct regulator_desc desc;

	/* Register that holds VSEL */
	u8 cfgreg;

	/* flags to indicate mode */
	int flags;
};

static int twl_smartreflex_read_reg(struct i2c_client *client,
		u8 addr, u8 *data)
{
	int ret;

	i2c_master_send(client, &addr, 1);
	ret = i2c_master_recv(client, data, 1);

	if (ret < 0) {
		dev_err(&client->dev, "I2C read error\n");
		return ret;
	}

	return ret;
}

static int twl_smartreflex_write_reg(struct i2c_client *client,
		u8 addr, u8 data)
{
	int ret;
	u8 txbuf[2];

	txbuf[0] = addr;
	txbuf[1] = data;

	ret = i2c_master_send(client, txbuf, 2);
	if (ret < 0)
		dev_err(&client->dev, "I2C write error\n");

	return ret;
}

static int
twl_smartreflex_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct twl_smartreflex_info	*info = rdev_get_drvdata(rdev);

	int voltage = 0;

	switch(info->flags) {
	case 0:
		if (index == 0)
			voltage = 0;
		else if (index < 58)
			voltage = (600000 + (12500 * (index - 1)));
		else if (index == 58)
			voltage = 1350 * 1000;
		else if (index == 59)
			voltage = 1500 * 1000;
		else if (index == 60)
			voltage = 1800 * 1000;
		else if (index == 61)
			voltage = 1900 * 1000;
		else if (index == 62)
			voltage = 2100 * 1000;
		break;
	case DCDC_OFFSET_EN:
		if (index == 0)
			voltage = 0;
		else if (index < 58)
			voltage = (700000 + (12500 * (index - 1)));
		else if (index == 58)
			voltage = 1350 * 1000;
		else if (index == 59)
			voltage = 1500 * 1000;
		else if (index == 60)
			voltage = 1800 * 1000;
		else if (index == 61)
			voltage = 1900 * 1000;
		else if (index == 62)
			voltage = 2100 * 1000;
		break;
	case DCDC_EXTENDED_EN:
		if (index == 0)
			voltage = 0;
		else if (index < 58)
			voltage = (1852000 + (38600 * (index - 1)));
		else if (index == 58)
			voltage = 2084 * 1000;
		else if (index == 59)
			voltage = 2315 * 1000;
		else if (index == 60)
			voltage = 2778 * 1000;
		else if (index == 61)
			voltage = 2932 * 1000;
		else if (index == 62)
			voltage = 3241 * 1000;
		break;
	case DCDC_OFFSET_EN|DCDC_EXTENDED_EN:
		if (index == 0)
			voltage = 0;
		else if (index < 58)
			voltage = (2161000 + (38600 * (index - 1)));
		else if (index == 58)
			voltage = 4167 * 1000;
		else if (index == 59)
			voltage = 2315 * 1000;
		else if (index == 60)
			voltage = 2778 * 1000;
		else if (index == 61)
			voltage = 2932 * 1000;
		else if (index == 62)
			voltage = 3241 * 1000;
		break;
	}

	return voltage;
}

static int
twl_smartreflex_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV)
{
	struct twl_smartreflex_info	*info = rdev_get_drvdata(rdev);
	struct i2c_client *client = info->client;
	int	vsel = 0;

	switch(info->flags) {
	case 0:
		if(min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 600000) && (max_uV <= 1300000)) {
			vsel = (min_uV - 600000) / 125;
			if (vsel % 100)
				vsel += 100;
			vsel /= 100;
			vsel ++;
		}
		/* This logic is a little difficult to read but it is taking
		 * care of the case where min/max do not fit within the slot
		 * size of the TWL DCDC regulators.
		 */
		else if ((min_uV >1900000) && (max_uV >= 2100000))
			vsel = 62;
		else if ((min_uV >1800000) && (max_uV >= 1900000))
			vsel = 61;
		else if ((min_uV >1500000) && (max_uV >= 1800000))
			vsel = 60;
		else if ((min_uV >1350000) && (max_uV >= 1500000))
			vsel = 59;
		else if ((min_uV >1300000) && (max_uV >= 1350000))
			vsel = 58;
		else
			return -EINVAL;
		break;
	case DCDC_OFFSET_EN:
		if(min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 700000) && (max_uV <= 1420000)) {
			vsel = (min_uV - 600000) / 125;
			if (vsel % 100)
				vsel += 100;
			vsel /= 100;
			vsel ++;
		}
		/* This logic is a little difficult to read but it is taking
		 * care of the case where min/max do not fit within the slot
		 * size of the TWL DCDC regulators.
		 */
		else if ((min_uV >1900000) && (max_uV >= 2100000))
			vsel = 62;
		else if ((min_uV >1800000) && (max_uV >= 1900000))
			vsel = 61;
		else if ((min_uV >1350000) && (max_uV >= 1800000))
			vsel = 60;
		else if ((min_uV >1350000) && (max_uV >= 1500000))
			vsel = 59;
		else if ((min_uV >1300000) && (max_uV >= 1350000))
			vsel = 58;
		else
			return -EINVAL;
		break;
	case DCDC_EXTENDED_EN:
		if(min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 1852000) && (max_uV <= 4013600)) {
			vsel = (min_uV - 1852000) / 386;
			if (vsel % 100)
				vsel += 100;
			vsel /= 100;
			vsel ++;
		}
		break;
	case DCDC_OFFSET_EN|DCDC_EXTENDED_EN:
		if(min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 2161000) && (max_uV <= 4321000)) {
			vsel = (min_uV - 1852000) / 386;
			if (vsel % 100)
				vsel += 100;
			vsel /= 100;
			vsel ++;
		}
		break;
	}

	return twl_smartreflex_write_reg(client, info->cfgreg, vsel);
}

static int twl_smartreflex_get_voltage(struct regulator_dev *rdev)
{
	struct twl_smartreflex_info	*info = rdev_get_drvdata(rdev);
	u8 vsel;
	int voltage = 0;

	twl_smartreflex_read_reg(info->client, info->cfgreg, &vsel);

	vsel &= DCDC_VSEL_MASK;

	switch(info->flags) {
	case 0:
		if (vsel == 0)
			voltage = 0;
		else if (vsel < 58)
			voltage = (600000 + (12500 * (vsel - 1)));
		else if (vsel == 58)
			voltage = 1350 * 1000;
		else if (vsel == 59)
			voltage = 1500 * 1000;
		else if (vsel == 60)
			voltage = 1800 * 1000;
		else if (vsel == 61)
			voltage = 1900 * 1000;
		else if (vsel == 62)
			voltage = 2100 * 1000;
		break;
	case DCDC_OFFSET_EN:
		if (vsel == 0)
			voltage = 0;
		else if (vsel < 58)
			voltage = (700000 + (12500 * (vsel - 1)));
		else if (vsel == 58)
			voltage = 1350 * 1000;
		else if (vsel == 59)
			voltage = 1500 * 1000;
		else if (vsel == 60)
			voltage = 1800 * 1000;
		else if (vsel == 61)
			voltage = 1900 * 1000;
		else if (vsel == 62)
			voltage = 2100 * 1000;
		break;
	case DCDC_EXTENDED_EN:
		if (vsel == 0)
			voltage = 0;
		else if (vsel < 58)
			voltage = (1852000 + (38600 * (vsel - 1)));
		else if (vsel == 58)
			voltage = 2084 * 1000;
		else if (vsel == 59)
			voltage = 2315 * 1000;
		else if (vsel == 60)
			voltage = 2778 * 1000;
		else if (vsel == 61)
			voltage = 2932 * 1000;
		else if (vsel == 62)
			voltage = 3241 * 1000;
		break;
	case DCDC_EXTENDED_EN|DCDC_OFFSET_EN:
		if (vsel == 0)
			voltage = 0;
		else if (vsel < 58)
			voltage = (2161000 + (38600 * (vsel - 1)));
		else if (vsel == 58)
			voltage = 4167 * 1000;
		else if (vsel == 59)
			voltage = 2315 * 1000;
		else if (vsel == 60)
			voltage = 2778 * 1000;
		else if (vsel == 61)
			voltage = 2932 * 1000;
		else if (vsel == 62)
			voltage = 3241 * 1000;
		break;
	}
	return voltage;
}

static struct regulator_ops twl_smartreflex_ops = {
	.list_voltage = twl_smartreflex_list_voltage,
	.get_voltage = twl_smartreflex_get_voltage,
	.set_voltage = twl_smartreflex_set_voltage,
};

static struct twl_smartreflex_info twl_smartreflex_reg[] = {
	{
		.desc = {
			.name = "SMPS1",
			.id = TWL_SMARTREFLEX_SMPS1,
			.ops = &twl_smartreflex_ops,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = 63,
			.owner = THIS_MODULE,
		},
		.cfgreg = SMPS1_CFG_VOLTAGE,
	},
	{
		.desc = {
			.name = "SMPS2",
			.id = TWL_SMARTREFLEX_SMPS2,
			.ops = &twl_smartreflex_ops,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = 63,
			.owner = THIS_MODULE,
		},
		.cfgreg = SMPS2_CFG_VOLTAGE,
	},
};

static const struct i2c_device_id twl_smartreflex_id[] = {
	{ "twl-smartreflex", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, twl_smartreflex_id);

static int twl_smartreflex_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct regulator_init_data *init_data = client->dev.platform_data;
	int ret, i;

	if (!init_data)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(twl_smartreflex_reg); i++ ) {
		twl_smartreflex_reg[i].client = client;
		twl_smartreflex_reg[i].rdev =
				regulator_register(&twl_smartreflex_reg[i].desc,
				&client->dev, &init_data[i],
				&twl_smartreflex_reg[i]);

		if (IS_ERR(twl_smartreflex_reg[i].rdev)) {
			ret = PTR_ERR(twl_smartreflex_reg[i].rdev);
			dev_err(&client->dev, "failed to register %s %s\n",
					id->name, twl_smartreflex_reg[i].desc.name);
			goto err;
		}
	}

	if(twl_get_smps_offset() & SMPS_MULTOFFSET_SMPS1)
		twl_smartreflex_reg[0].flags |= DCDC_OFFSET_EN;
	if(twl_get_smps_offset() & SMPS_MULTOFFSET_SMPS2)
		twl_smartreflex_reg[1].flags |= DCDC_OFFSET_EN;
	if(twl_get_smps_mult() & SMPS_MULTOFFSET_SMPS1)
		twl_smartreflex_reg[0].flags |= DCDC_EXTENDED_EN;
	if(twl_get_smps_mult() & SMPS_MULTOFFSET_SMPS2)
		twl_smartreflex_reg[1].flags |= DCDC_EXTENDED_EN;

	i2c_set_clientdata(client, twl_smartreflex_reg);
	dev_dbg(&client->dev, "%s regulator driver is registered.\n", id->name);

	return 0;

err:
	return ret;
}

static int twl_smartreflex_remove(struct i2c_client *client)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(twl_smartreflex_reg); i++)
		regulator_unregister(twl_smartreflex_reg[i].rdev);

	return 0;
}

static struct i2c_driver twl_smartreflex_driver = {
	.driver.name	= DRIVER_NAME,
	.probe		= twl_smartreflex_probe,
	.remove		= twl_smartreflex_remove,
	.id_table	= twl_smartreflex_id,
};

static int __init twl_smartreflex_init(void)
{
	return i2c_add_driver(&twl_smartreflex_driver);
}
subsys_initcall(twl_smartreflex_init);

static void __exit twl_smartreflex_exit(void)
{
	i2c_del_driver(&twl_smartreflex_driver);
}
module_exit(twl_smartreflex_exit);

MODULE_DESCRIPTION("TWL Smart Reflex Driver");
MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:twl-smartreflex");

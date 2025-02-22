/*
 * drivers/regulator/ricoh619-regulator.c
 *
 * Regulator driver for RICOH R5T619 power management chip.
 *
 * Copyright (C) 2012-2014 RICOH COMPANY,LTD
 *
 * Based on code
 *	Copyright (C) 2011 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*#define DEBUG			1*/
/*#define VERBOSE_DEBUG		1*/
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/ricoh619.h>
#include <linux/regulator/ricoh619-regulator.h>

#include <linux/of.h>
#include <linux/regulator/of_regulator.h>

#define RICOH_TYPE_1145     1145
#define RICOH_TYPE_1137     1137

extern int giBusFreqDebugLvl;

static int gRicoh_type = RICOH_TYPE_1137;

#include "../../arch/arm/mach-imx/hardware.h"

#include "../../arch/arm/mach-imx/ntx_hwconfig.h"
struct ricoh61x_regulator {
	int		id;
	int		sleep_id;
	/* Regulator register address.*/
	u8		reg_en_reg;
	u8		en_bit;
	u8		reg_disc_reg;
	u8		disc_bit;
	u8		vout_reg;
	u8		vout_mask;
	u8		vout_reg_cache;
	u8		sleep_reg;
	u8		eco_reg;
	u8		eco_bit;
	u8		eco_slp_reg;
	u8		eco_slp_bit;

	/* chip constraints on regulator behavior */
	int			min_uV;
	int			max_uV;
	int			step_uV;
	int			nsteps;

	/* regulator specific turn-on delay */
	u16			delay;

	/* used by regulator core */
	struct regulator_desc	desc;

	/* Device */
	struct device		*dev;
};
extern volatile NTX_HWCONFIG *gptHWCFG;

static unsigned int ricoh61x_suspend_status;

static inline struct device *to_ricoh61x_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static int ricoh61x_regulator_enable_time(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);

	return ri->delay;
}

static int ricoh61x_reg_is_enabled(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	uint8_t control;
	int ret;

	ret = ricoh61x_read(parent, ri->reg_en_reg, &control);
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in reading the control register\n");
		return ret;
	}
	return (((control >> ri->en_bit) & 1) == 1);
}

static int ricoh61x_reg_enable(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;

	ret = ricoh61x_set_bits(parent, ri->reg_en_reg, (1 << ri->en_bit));
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in updating the STATE register\n");
		return ret;
	}
	udelay(ri->delay);
	return ret;
}

static int ricoh61x_reg_disable(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;

	ret = ricoh61x_clr_bits(parent, ri->reg_en_reg, (1 << ri->en_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error in updating the STATE register\n");

	return ret;
}

static int ricoh61x_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);

	return ri->min_uV + (ri->step_uV * index);
}

static int __ricoh61x_set_s_voltage(struct device *parent,
		struct ricoh61x_regulator *ri, int min_uV, int max_uV)
{
	int vsel;
	int ret;

	if ((min_uV < ri->min_uV) || (max_uV > ri->max_uV))
		return -EDOM;

	vsel = (min_uV - ri->min_uV + ri->step_uV - 1)/ri->step_uV;
	if (vsel > ri->nsteps)
		return -EDOM;

	printk ("%s at %duv suspend\n", ri->desc.name, min_uV);
	ret = ricoh61x_update(parent, ri->sleep_reg, vsel, ri->vout_mask);
	if (ret < 0)
		dev_err(ri->dev, "Error in writing the sleep register\n");
	return ret;
}

static int __ricoh61x_set_voltage(struct device *parent,
		struct ricoh61x_regulator *ri, int min_uV, int max_uV,
		unsigned *selector)
{
	int vsel;
	int ret;
	uint8_t vout_val;

	if ((min_uV < ri->min_uV) || (max_uV > ri->max_uV))
		return -EDOM;

	vsel = (min_uV - ri->min_uV + ri->step_uV - 1)/ri->step_uV;
	if (vsel > ri->nsteps)
		return -EDOM;

	if (selector)
		*selector = vsel;

	vout_val = (ri->vout_reg_cache & ~ri->vout_mask) |
				(vsel & ri->vout_mask);
	ret = ricoh61x_write(parent, ri->vout_reg, vout_val);
	if (ret < 0)
		dev_err(ri->dev, "Error in writing the Voltage register\n");
	else
		ri->vout_reg_cache = vout_val;

	return ret;
}

static int ricoh61x_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);

	if (ricoh61x_suspend_status)
		return -EBUSY;

	if( giBusFreqDebugLvl >= 2 ) {
		printk("%s()-%s (%d~%d)uV \n",__FUNCTION__,rdev->desc->name,min_uV,max_uV);

		if( 9 == giBusFreqDebugLvl ) {
			dump_stack();
		}
		
	}
	return __ricoh61x_set_voltage(parent, ri, min_uV, max_uV, selector);
}

static int ricoh619_set_suspend_voltage(struct regulator_dev *rdev,
		int uV)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);

	return __ricoh61x_set_s_voltage(parent, ri, uV, uV);
}

static int ricoh61x_get_voltage(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t vsel;

	vsel = ri->vout_reg_cache & ri->vout_mask;
	return ri->min_uV + vsel * ri->step_uV;
}

int ricoh61x_regulator_enable_eco_mode(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;

	ret = ricoh61x_set_bits(parent, ri->eco_reg, (1 << ri->eco_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error Enable LDO eco mode\n");

	return ret;
}
EXPORT_SYMBOL_GPL(ricoh61x_regulator_enable_eco_mode);

int ricoh61x_regulator_disable_eco_mode(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;

	ret = ricoh61x_clr_bits(parent, ri->eco_reg, (1 << ri->eco_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error Disable LDO eco mode\n");

	return ret;
}
EXPORT_SYMBOL_GPL(ricoh61x_regulator_disable_eco_mode);

int ricoh61x_regulator_enable_eco_slp_mode(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;

	ret = ricoh61x_set_bits(parent, ri->eco_slp_reg,
						(1 << ri->eco_slp_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error Enable LDO eco mode in d during sleep\n");

	return ret;
}
EXPORT_SYMBOL_GPL(ricoh61x_regulator_enable_eco_slp_mode);

int ricoh61x_regulator_disable_eco_slp_mode(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;

	ret = ricoh61x_clr_bits(parent, ri->eco_slp_reg,
						(1 << ri->eco_slp_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error Enable LDO eco mode in d during sleep\n");

	return ret;
}
EXPORT_SYMBOL_GPL(ricoh61x_regulator_disable_eco_slp_mode);

static unsigned int ricoh619_dcdc_get_mode(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;
	uint8_t control;
	u8 mask = 0x30;
	
	ret = ricoh61x_read(parent, ri->reg_en_reg,&control);
        if (ret < 0) {
                return ret;
        }
	control=(control & mask) >> 4;
	switch (control) {
	case 1:
		return REGULATOR_MODE_FAST;
	case 0:
		return REGULATOR_MODE_NORMAL;
	case 2:
		return REGULATOR_MODE_STANDBY;
	case 3:
		return REGULATOR_MODE_NORMAL;
	default:
		return -1;
	}

}
static int ricoh619_dcdc_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;
	uint8_t control;
	
	ret = ricoh61x_read(parent, ri->reg_en_reg,&control);
	switch(mode)
	{
	case REGULATOR_MODE_FAST:
		return ricoh61x_write(parent, ri->reg_en_reg, ((control & 0xcf) | 0x10));
	case REGULATOR_MODE_NORMAL:
		return ricoh61x_write(parent, ri->reg_en_reg, (control & 0xcf));
	case REGULATOR_MODE_STANDBY:
		return ricoh61x_write(parent, ri->reg_en_reg, ((control & 0xcf) | 0x20));	
	default:
		printk("error:pmu_619 only powersave pwm psm mode\n");
		return -EINVAL;
	}
	

}

static int ricoh619_dcdc_set_voltage_time_sel(struct regulator_dev *rdev,   unsigned int old_selector,
				     unsigned int new_selector)
{
	int old_volt, new_volt;
	
	old_volt = ricoh61x_list_voltage(rdev, old_selector);
	if (old_volt < 0)
		return old_volt;
	
	new_volt = ricoh61x_list_voltage(rdev, new_selector);
	if (new_volt < 0)
		return new_volt;

	return DIV_ROUND_UP(abs(old_volt - new_volt)*2, 14000);
}
static int ricoh619_dcdc_set_suspend_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;
	uint8_t control;
	
	ret = ricoh61x_read(parent, ri->reg_en_reg,&control);
	switch(mode)
	{
	case REGULATOR_MODE_FAST:
		return ricoh61x_write(parent, ri->reg_en_reg, ((control & 0x3f) | 0x40));
	case REGULATOR_MODE_NORMAL:
		return ricoh61x_write(parent, ri->reg_en_reg, (control & 0x3f));
	case REGULATOR_MODE_STANDBY:
		return ricoh61x_write(parent, ri->reg_en_reg, ((control & 0x3f) | 0x80));	
	default:
		printk("error:pmu_619 only powersave pwm psm mode\n");
		return -EINVAL;
	}
	

}
static int ricoh619_reg_suspend_enable(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;

	ret = ricoh61x_set_bits(parent, (0x16 + ri->id), (0xf << 0));
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in updating the STATE register\n");
		return ret;
	}
	udelay(ri->delay);
	return ret;
}

static int ricoh619_reg_suspend_disable(struct regulator_dev *rdev)
{
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh61x_dev(rdev);
	int ret;

	ret = ricoh61x_clr_bits(parent, (0x16 + ri->id), (0xf <<0));
	if (ret < 0)
		dev_err(&rdev->dev, "Error in updating the STATE register\n");

	return ret;
}

static struct regulator_ops ricoh61x_ops = {
	.list_voltage	= ricoh61x_list_voltage,
	.set_voltage	= ricoh61x_set_voltage,
	.get_voltage	= ricoh61x_get_voltage,
	.set_suspend_voltage = ricoh619_set_suspend_voltage,
	.set_voltage_time_sel = ricoh619_dcdc_set_voltage_time_sel,
	.get_mode = ricoh619_dcdc_get_mode,
	.set_mode = ricoh619_dcdc_set_mode,
	.enable		= ricoh61x_reg_enable,
	.disable	= ricoh61x_reg_disable,
	.set_suspend_mode = ricoh619_dcdc_set_suspend_mode,
	.set_suspend_enable				= ricoh619_reg_suspend_enable,
	.set_suspend_disable				= ricoh619_reg_suspend_disable,
	.is_enabled	= ricoh61x_reg_is_enabled,
	.enable_time	= ricoh61x_regulator_enable_time,
};

#define RICOH61x_REG(_id, _en_reg, _en_bit, _disc_reg, _disc_bit, _vout_reg, \
		_vout_mask, _ds_reg, _min_mv, _max_mv, _step_uV, _nsteps,    \
		_ops, _delay, _eco_reg, _eco_bit, _eco_slp_reg, _eco_slp_bit) \
{								\
	.reg_en_reg	= _en_reg,				\
	.en_bit		= _en_bit,				\
	.reg_disc_reg	= _disc_reg,				\
	.disc_bit	= _disc_bit,				\
	.vout_reg	= _vout_reg,				\
	.vout_mask	= _vout_mask,				\
	.sleep_reg	= _ds_reg,				\
	.min_uV		= _min_mv * 1000,			\
	.max_uV		= _max_mv * 1000,			\
	.step_uV	= _step_uV,				\
	.nsteps		= _nsteps,				\
	.delay		= _delay,				\
	.id		= RICOH619_ID_##_id,			\
	.sleep_id	= RICOH619_DS_##_id,			\
	.eco_reg	=  _eco_reg,				\
	.eco_bit	=  _eco_bit,				\
	.eco_slp_reg	=  _eco_slp_reg,			\
	.eco_slp_bit	=  _eco_slp_bit,			\
	.desc = {						\
		.name = ricoh619_rails(_id),			\
		.id = RICOH619_ID_##_id,			\
		.n_voltages = _nsteps,				\
		.ops = &_ops,					\
		.type = REGULATOR_VOLTAGE,			\
		.owner = THIS_MODULE,				\
	},							\
}

static struct ricoh61x_regulator ricoh61x_regulator[] = {
	RICOH61x_REG(DC1, 0x2C, 0, 0x2C, 1, 0x36, 0xFF, 0x3B,
			600, 3500, 12500, 0xE8, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(DC2, 0x2E, 0, 0x2E, 1, 0x37, 0xFF, 0x3C,
			600, 3500, 12500, 0xE8, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(DC3, 0x30, 0, 0x30, 1, 0x38, 0xFF, 0x3D,
			600, 3500, 12500, 0xE8, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(DC4, 0x32, 0, 0x32, 1, 0x39, 0xFF, 0x3E,
			600, 3500, 12500, 0xE8, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(DC5, 0x34, 0, 0x34, 1, 0x3A, 0xFF, 0x3F,
			600, 3500, 12500, 0xE8, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(LDO1, 0x44, 0, 0x46, 0, 0x4C, 0x7F, 0x58,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x48, 0, 0x4A, 0),

	RICOH61x_REG(LDO2, 0x44, 1, 0x46, 1, 0x4D, 0x7F, 0x59,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x48, 1, 0x4A, 1),

	RICOH61x_REG(LDO3, 0x44, 2, 0x46, 2, 0x4E, 0x7F, 0x5A,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x48, 2, 0x4A, 2),

	RICOH61x_REG(LDO4, 0x44, 3, 0x46, 3, 0x4F, 0x7F, 0x5B,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x48, 3, 0x4A, 3),

	RICOH61x_REG(LDO5, 0x44, 4, 0x46, 4, 0x50, 0x7F, 0x5C,
			600, 3500, 25000, 0x74, ricoh61x_ops, 500,
			0x48, 4, 0x4A, 4),

	RICOH61x_REG(LDO6, 0x44, 5, 0x46, 5, 0x51, 0x7F, 0x5D,
			600, 3500, 25000, 0x74, ricoh61x_ops, 500,
			0x48, 5, 0x4A, 5),

	RICOH61x_REG(LDO7, 0x44, 6, 0x46, 6, 0x52, 0x7F, 0x5E,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(LDO8, 0x44, 7, 0x46, 7, 0x53, 0x7F, 0x5F,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(LDO9, 0x45, 0, 0x47, 0, 0x54, 0x7F, 0x60,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(LDO10, 0x45, 1, 0x47, 1, 0x55, 0x7F, 0x61,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(LDORTC1, 0x45, 4, 0x00, 0, 0x56, 0x7F, 0x00,
			1700, 3500, 25000, 0x48, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),

	RICOH61x_REG(LDORTC2, 0x45, 5, 0x00, 0, 0x57, 0x7F, 0x00,
			900, 3500, 25000, 0x68, ricoh61x_ops, 500,
			0x00, 0, 0x00, 0),
};
static inline struct ricoh61x_regulator *find_regulator_info(int id)
{
	struct ricoh61x_regulator *ri;
	int i;

	for (i = 0; i < ARRAY_SIZE(ricoh61x_regulator); i++) {
		ri = &ricoh61x_regulator[i];
		if (ri->desc.id == id)
			return ri;
	}
	return NULL;
}

static int ricoh61x_regulator_preinit(struct device *parent,
		struct ricoh61x_regulator *ri,
		struct ricoh619_regulator_platform_data *ricoh61x_pdata)
{
	int ret = 0;

	if (!ricoh61x_pdata->init_apply)
		return 0;

	if (ricoh61x_pdata->init_uV >= 0) {
		ret = __ricoh61x_set_voltage(parent, ri,
				ricoh61x_pdata->init_uV,
				ricoh61x_pdata->init_uV, 0);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to initialize voltage %d "
				"for rail %d err %d\n", ricoh61x_pdata->init_uV,
				ri->desc.id, ret);
			return ret;
		}
		if (ricoh61x_pdata->sleep_uV <= 0) 
			ricoh61x_pdata->sleep_uV = ricoh61x_pdata->init_uV;
	}

	if (ricoh61x_pdata->sleep_uV > 0) {
		int vReg = ri->min_uV + (ri->vout_reg_cache & ri->vout_mask) * ri->step_uV;

		if (3300*1000 == vReg)
			ret = __ricoh61x_set_s_voltage(parent, ri, vReg,vReg);
		else
			ret = __ricoh61x_set_s_voltage(parent, ri,
				ricoh61x_pdata->sleep_uV,
				ricoh61x_pdata->sleep_uV);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to initialize sleep voltage %d "
				"for rail %d err %d\n", ricoh61x_pdata->sleep_uV,
				ri->desc.id, ret);
			return ret;
		}
	}

	if (ricoh61x_pdata->init_enable) {
		if (RICOH619_ID_DC1 <= ri->id && RICOH619_ID_DC5 >= ri->id) {
			ret = ricoh61x_set_bits(parent, ri->reg_en_reg,
										((1 << ri->en_bit)|0x80));
		}
		else
			ret = ricoh61x_set_bits(parent, ri->reg_en_reg,
								(1 << ri->en_bit));
	}
	else
		ret = ricoh61x_clr_bits(parent, ri->reg_en_reg,
							(1 << ri->en_bit));
	if (ret < 0)
		dev_err(ri->dev, "Not able to %s rail %d err %d\n",
			(ricoh61x_pdata->init_enable) ? "enable" : "disable",
			ri->desc.id, ret);

	return ret;
}

static inline int ricoh61x_cache_regulator_register(struct device *parent,
	struct ricoh61x_regulator *ri)
{
	ri->vout_reg_cache = 0;
	return ricoh61x_read(parent, ri->vout_reg, &ri->vout_reg_cache);
}

#ifdef CONFIG_OF
static struct of_regulator_match ricoh619_regulator_matches[] = {
	{ .name	= "ricoh619_dc1",},
	{ .name = "ricoh619_dc2",},
	{ .name = "ricoh619_dc3",},
	{ .name = "ricoh619_dc4",},
	{ .name = "ricoh619_dc5",},
	{ .name = "ricoh619_ldo1",},
	{ .name = "ricoh619_ldo2",},
	{ .name = "ricoh619_ldo3",},
	{ .name = "ricoh619_ldo4",},
	{ .name = "ricoh619_ldo5",},
	{ .name = "ricoh619_ldo6",},
	{ .name = "ricoh619_ldo7",},
	{ .name = "ricoh619_ldo8",},
	{ .name = "ricoh619_ldo9",},
	{ .name = "ricoh619_ldo10",},
	{ .name = "ricoh619_ldortc1",},
	{ .name = "ricoh619_ldortc2",},
};
#endif

#ifdef CONFIG_OF
static int ricoh619_regulator_dt_init(struct platform_device *pdev,
				    struct regulator_config *config,
				    int regidx)
{
	struct device_node *nproot, *np;
	int rcount;
	nproot = of_node_get(pdev->dev.parent->of_node);
	if (!nproot)
		return -ENODEV;
	np = of_find_node_by_name(nproot, "regulators");
	if (!np) {
		dev_err(&pdev->dev, "failed to find regulators node\n");
		return -ENODEV;
	}
	
	of_property_read_u32(np, "ricoh-type", &gRicoh_type);

	rcount = of_regulator_match(&pdev->dev, np,
				&ricoh619_regulator_matches[regidx], 1);
	of_node_put(np);
	if (rcount < 0)
		return -ENODEV;
	config->init_data = ricoh619_regulator_matches[regidx].init_data;
	config->of_node = ricoh619_regulator_matches[regidx].of_node;

	return 0;
}
#else
#define ricoh619_regulator_dt_init(x, y, z)	(-1)
#endif

static int ricoh61x_regulator_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct ricoh61x_regulator *ri = NULL;
	struct regulator_dev *rdev;
	struct regulator_config config = { };
	int err,id=0;
	
	printk(KERN_INFO "PMU: %s\n", __func__);
	rdev = devm_kzalloc(&pdev->dev, RICOH619_NUM_REGULATOR *
				sizeof(*rdev), GFP_KERNEL);
	if (!rdev) {
		dev_err(&pdev->dev, "Mmemory alloc failed\n");
		return -ENOMEM;
	}

	for (id = 0; id < RICOH619_NUM_REGULATOR; ++id) {
		ri = find_regulator_info(id);
		if (!ri) {
			dev_err(&pdev->dev, "invalid regulator ID %d specified\n", id);
			err = -EINVAL;
		}
		else
			ri->dev = &pdev->dev;
		config.dev = &pdev->dev;
		config.driver_data = ri;

		config.of_node = ricoh619_regulator_matches[id].of_node;

		err = ricoh619_regulator_dt_init(pdev, &config, id);
		if (err < 0) {
			dev_err(&pdev->dev, "failed to regulator dt init\n");
		}

		rdev = regulator_register(&ri->desc, &config);
		if (IS_ERR_OR_NULL(rdev)) {
			dev_err(&pdev->dev, "failed to register regulator %s\n",
					ri->desc.name);
			return PTR_ERR(rdev);
		}
	}

	platform_set_drvdata(pdev, rdev);
#else
	struct ricoh61x_regulator *ri = NULL;
	struct regulator_dev *rdev;
	struct ricoh619_regulator_platform_data *tps_pdata;
	int id = pdev->id;
	int err;

//	printk(KERN_INFO "PMU: %s\n", __func__);
	ri = find_regulator_info(id);
	if (ri == NULL) {
		dev_err(&pdev->dev, "invalid regulator ID specified\n");
		return -EINVAL;
	}
	tps_pdata = pdev->dev.platform_data;
	ri->dev = &pdev->dev;
	ricoh61x_suspend_status = 0;

	err = ricoh61x_cache_regulator_register(pdev->dev.parent, ri);
	if (err) {
		dev_err(&pdev->dev, "Fail in caching register\n");
		return err;
	}

	err = ricoh61x_regulator_preinit(pdev->dev.parent, ri, tps_pdata);
	if (err) {
		dev_err(&pdev->dev, "Fail in pre-initialisation\n");
		return err;
	}
	rdev = regulator_register(&ri->desc, &pdev->dev,
				&tps_pdata->regulator, ri);
	if (IS_ERR_OR_NULL(rdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
				ri->desc.name);
		return PTR_ERR(rdev);
	}

	platform_set_drvdata(pdev, rdev);
#endif
	if (3 == gptHWCFG->m_val.bTouch2Ctrl) { //Digitizer
		if (RICOH619_ID_LDO1 == id) {
			printk("[%s-%d] ID=%d: eco_slp_reg=%2X, setting to 0\n", __func__, __LINE__, id, ri-> eco_slp_reg);
			ri->eco_slp_reg=0;
		}
	}
	
	if (ri->eco_slp_reg) 
		ricoh61x_regulator_enable_eco_slp_mode (rdev);

	return 0;
}

static int ricoh61x_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);
	return 0;
}

#ifdef CONFIG_PM
extern int gSleep_Mode_Suspend;
static uint8_t regulator_slot[RICOH619_ID_LDORTC2+1]; 
static int ricoh61x_regulator_suspend(struct device *dev)
{
	struct regulator_dev *rdev = platform_get_drvdata(to_platform_device(dev));
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct ricoh619_regulator_platform_data *ricoh61x_pdata = ((struct device*)(ri->dev))->platform_data;
	uint8_t temp;
	int reg_id;
		
	for (reg_id=RICOH619_ID_DC1; reg_id<=RICOH619_ID_LDORTC2; reg_id++) {
		int offset = 0x16 + (reg_id - RICOH619_ID_DC1);
		ricoh61x_read(to_ricoh61x_dev(rdev), offset, &regulator_slot[reg_id]);
		if (gSleep_Mode_Suspend ) {
			temp = (regulator_slot[reg_id] & 0xF0) | 0x0E;

			switch (reg_id) {
			case RICOH619_ID_DC1:
				if (gRicoh_type == RICOH_TYPE_1145) { // Core4_1V2
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x2C, 0x83); // set psm mode in sleep mode
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x2D, 0x03);
				} else { // Core2_1V3_SOC
					if (cpu_is_imx6ull()) {
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x2C, 0x83); // set psm mode in sleep mode
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x2D, 0x03);
					}
				}
				break;
			case RICOH619_ID_DC2:
				if (gRicoh_type == RICOH_TYPE_1145) { // Core2_1V3_SOC
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x2E, 0x83);	// set psm mode in sleep mode
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x2F, 0x03);
				} else {	// Core3_3V3
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x3C, 0xC0);	// set 3V in sleep mode
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x2E, 0x83);	// set psm mode in sleep mode
					if (cpu_is_imx6ull())
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x2F, 0x03);
				}
				break;
			case RICOH619_ID_DC3:
				if (gRicoh_type == RICOH_TYPE_1145) { // Core3_3V3
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x3D, 0xC0);	// set 3V in sleep mode
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x30, 0x83);	// set psm mode in sleep mode
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x31, 0x03);
				} else {	// Core2_1V3_ARM
					if(63 != gptHWCFG->m_val.bPCB && 81 != gptHWCFG->m_val.bPCB && 85 != gptHWCFG->m_val.bPCB) {	// T60Q02,E60U22,E60U24,T60U02
						ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
					}
					
					if (cpu_is_imx6ull()) {
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x30, 0x83);	// set psm mode in sleep mode
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x31, 0x03);
					}
				}
				break;
			case RICOH619_ID_DC4:
				if (gRicoh_type == RICOH_TYPE_1145) { // Core4_1V8
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x32, 0x83);	// set psm mode in sleep mode
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x33, 0x03);
				} else { // Core4_1V2
					if (cpu_is_imx6ull()) {
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x32, 0x83);	// set psm mode in sleep mode
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x33, 0x03);
					}
				}
				break;
			case RICOH619_ID_DC5:
				if (gRicoh_type == RICOH_TYPE_1145) { // Core2_1V3_ARM
					if(63 != gptHWCFG->m_val.bPCB && 81 != gptHWCFG->m_val.bPCB && 85 != gptHWCFG->m_val.bPCB) {	// T60Q02,E60U22,E60U24,T60U02
						ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
					}
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x34, 0x83);	// set psm mode in sleep mode
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x35, 0x03);
				} else { // Core4_1V8
					if (cpu_is_imx6ull()) {
						if (81 == gptHWCFG->m_val.bPCB) {
							ricoh61x_write(to_ricoh61x_dev(rdev), 0x34, 0x83);	// set psm mode in sleep mode
							ricoh61x_write(to_ricoh61x_dev(rdev), 0x35, 0x03);
						}
						else
							ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
					}
				}
				break;
			case RICOH619_ID_LDO2:
				if (gRicoh_type == RICOH_TYPE_1145) { // Core1_3V3
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x59, 0x4E);	// set 2.85V in sleep mode
				} else {
					if (cpu_is_imx6ull())
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x59, 0x4E);	// set 2.85V in sleep mode
				}
				break;
			case RICOH619_ID_LDO8:	// VDD_EP_1V8
			case RICOH619_ID_LDO3:	// CORE5_1V2
			case RICOH619_ID_LDO5:	// SPD_3V3
			case RICOH619_ID_LDO7:	// VDD_PWM (FL_3V3)
				ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
				break;
			case RICOH619_ID_LDO4:
			case RICOH619_ID_LDO9:
			case RICOH619_ID_LDO10:
				if (50==gptHWCFG->m_val.bPCB || 47==gptHWCFG->m_val.bPCB || 54==gptHWCFG->m_val.bPCB || 
				    58==gptHWCFG->m_val.bPCB || 61==gptHWCFG->m_val.bPCB || 63==gptHWCFG->m_val.bPCB || 
				    66==gptHWCFG->m_val.bPCB || 68==gptHWCFG->m_val.bPCB || 81==gptHWCFG->m_val.bPCB ||
				    85==gptHWCFG->m_val.bPCB) {
					// E60QFX/ED0Q0X/ED0Q1X/E60QJX/E60QKX/T60Q02/E60U02/E60QP2/E60U22/E60U24
					ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
				}
				break;
			case RICOH619_ID_LDO6:	// DDR_0V6
				if( 4==gptHWCFG->m_val.bRamType || 10==gptHWCFG->m_val.bRamType ) {
					// DDR3 || LPDDR3.
					ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
				}
				break;
			case RICOH619_ID_LDO1:	// TP_3V3
				if( 3==gptHWCFG->m_val.bTouchType ||
				   (4==gptHWCFG->m_val.bTouchType && 0x03!=gptHWCFG->m_val.bUIConfig) )
				{
					ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
				}
				break;
			}

		}
		else {
			switch (reg_id) {
			case RICOH619_ID_DC2:
				if (gRicoh_type == RICOH_TYPE_1145) { // Core2_1V3_SOC
					
				} else { // Core3_3V3
					if (cpu_is_imx6ull()) // E60U22, E60U24
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x3C, 0xC0);	// set 3 V
					else
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x3C, 0xD8);	// set 3.3V
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x2E, 0x03);	// set auto mode
				}
				break;
			case RICOH619_ID_DC3:
				if (gRicoh_type == RICOH_TYPE_1145) { // Core3_3V3
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x3D, 0xD8);	// set 3.3V
					ricoh61x_write(to_ricoh61x_dev(rdev), 0x30, 0x03);	// set auto mode
				} else {	// Core2_1V3_ARM
					if (cpu_is_imx6ull()) {
						ricoh61x_write(to_ricoh61x_dev(rdev), 0x30, 0x03);	// set auto mode
					}
				}
				break;
			case RICOH619_ID_LDO8:	// VDD_EP_1V8
				//ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
				break;
			default:
				if (0x0F != (regulator_slot[reg_id] & 0x0F)) {
					temp = regulator_slot[reg_id] | 0x0F;
					ricoh61x_write(to_ricoh61x_dev(rdev), offset, temp);
				}
				break;
			}
		}
	}

	{
		if (5==gptHWCFG->m_val.bRamType)  	// LPDDR2
			ricoh61x_write(to_ricoh61x_dev(rdev), 0x4A, 0x22);	// set core3_3v3 & DDR_0V6 eco mode.
		else
			ricoh61x_write(to_ricoh61x_dev(rdev), 0x4A, 0x02);	// set core3_3v3 eco mode
	}

	ricoh61x_suspend_status = 1;

	return 0;
}

static int ricoh61x_regulator_resume(struct device *dev)
{
//	printk(KERN_INFO "PMU: %s %s\n", __func__, ri->desc.name);
	struct regulator_dev *rdev = platform_get_drvdata(to_platform_device(dev));
	struct ricoh61x_regulator *ri = rdev_get_drvdata(rdev);
	struct ricoh619_regulator_platform_data *ricoh61x_pdata = ((struct device*)(ri->dev))->platform_data;
	int reg_id;
	
	ricoh61x_write(to_ricoh61x_dev(rdev), 0x4A, 0x0);

	for (reg_id=RICOH619_ID_DC1; reg_id<=RICOH619_ID_LDORTC2; reg_id++) {
		int offset = 0x16 + (reg_id - RICOH619_ID_DC1);
		if (gSleep_Mode_Suspend) {
			ricoh61x_write(to_ricoh61x_dev(rdev), offset, regulator_slot[reg_id]);
		}
		else {
			switch (reg_id) {
			case RICOH619_ID_LDO8:	// VDD_EP_1V8
				//ricoh61x_write(to_ricoh61x_dev(rdev), offset, regulator_slot[reg_id]);
				break;
			}
		}
	}

	ricoh61x_suspend_status = 0;

	return 0;
}

static const struct dev_pm_ops ricoh61x_regulator_pm_ops = {
	.suspend	= ricoh61x_regulator_suspend,
	.resume		= ricoh61x_regulator_resume,
};
#endif

static struct platform_driver ricoh61x_regulator_driver = {
	.driver	= {
		.name	= "ricoh61x-regulator",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &ricoh61x_regulator_pm_ops,
#endif
	},
	.probe		= ricoh61x_regulator_probe,
	.remove		= ricoh61x_regulator_remove,
};

static int __init ricoh61x_regulator_init(void)
{
	printk(KERN_INFO "PMU: %s\n", __func__);
	return platform_driver_register(&ricoh61x_regulator_driver);
}
subsys_initcall_sync(ricoh61x_regulator_init);

static void __exit ricoh61x_regulator_exit(void)
{
	platform_driver_unregister(&ricoh61x_regulator_driver);
}
module_exit(ricoh61x_regulator_exit);

MODULE_DESCRIPTION("RICOH R5T619 regulator driver");
MODULE_ALIAS("platform:ricoh619-regulator");
MODULE_LICENSE("GPL");

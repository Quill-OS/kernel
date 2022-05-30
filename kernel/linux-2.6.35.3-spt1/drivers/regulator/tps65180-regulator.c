/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
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
#include <linux/regulator/tps65180.h>
#include <linux/gpio.h>
/* 2011/05/30 FY11 : Supported to read vcom from eMMC. */
#include <linux/mmc/rawdatatable.h>

/*
 * PMIC build options
 */
#define CUSTOM_RAILS 0
#define MANUAL_VCOM_ADJ 0
#define SWITCH_3V3 0


/*
 * PMIC Register Addresses
 */
enum {
    REG_TPS65180_TMST_VALUE = 0x0,
    REG_TPS65180_ENABLE,
    REG_TPS65180_VP_ADJUST,
    REG_TPS65180_VN_ADJUST,
    REG_TPS65180_VCOM_ADJUST,
    REG_TPS65180_INT_ENABLE1, /* 5 */
    REG_TPS65180_INT_ENABLE2,
    REG_TPS65180_INT_STATUS1,
    REG_TPS65180_INT_STATUS2,
    REG_TPS65180_PWR_SEQ0,
    REG_TPS65180_PWR_SEQ1, /* 10 */
    REG_TPS65180_PWR_SEQ2,
    REG_TPS65180_TMST_CONFIG,
    REG_TPS65180_TMST_OS,
    REG_TPS65180_TMST_HYST,
    REG_TPS65180_PG_STATUS, /* 15 */
    REG_TPS65180_REVID,
    REG_TPS65180_FIX_READ_POINTER,
};
#define TPS65180_REG_NUM        18
#define TPS65180_MAX_REGISTER   0xFF

/*
 * TPS65185 registers that are redefined differently
 */
#define REG_TPS65185_VADJ	REG_TPS65180_VP_ADJUST
#define REG_TPS65185_VCOM1	REG_TPS65180_VN_ADJUST
#define REG_TPS65185_VCOM2	REG_TPS65180_VCOM_ADJUST
#define REG_TPS65185_UPSEQ0	REG_TPS65180_PWR_SEQ0
#define REG_TPS65185_UPSEQ1	REG_TPS65180_PWR_SEQ1
#define REG_TPS65185_DWNSEQ0	REG_TPS65180_PWR_SEQ2
#define REG_TPS65185_DWNSEQ1	REG_TPS65180_TMST_CONFIG
#define REG_TPS65185_TMST1	REG_TPS65180_TMST_OS
#define REG_TPS65185_TMST2	REG_TPS65180_TMST_HYST
/* 2011/2/3 FY11 : Supported the function to save VCOM. */
#define REG_TPS65185_INT_STATUS1	REG_TPS65180_INT_STATUS1
/* 2011/05/25 FY11 : Changed the flag to confirm completion of reading temperature. */
#define REG_TPS6518x_INT_STATUS2	REG_TPS65180_INT_STATUS2

#define TPS6518x_REVID_VERSION(x)	(x & 0xf)
#define TPS6518x_REVID_MJREV(x)	((x >> 6) & 3)
#define TPS6518x_REVID_MNREV(x)	((x) >> 4) & 3)

/*
 * Bitfield macros that use rely on bitfield width/shift information.
 */
#define BITFMASK(field) (((1U << (field ## _WID)) - 1) << (field ## _LSH))
#define BITFVAL(field, val) ((val) << (field ## _LSH))
#define BITFEXT(var, bit) ((var & BITFMASK(bit)) >> (bit ## _LSH))

/*
 * Shift and width values for each register bitfield
 */
/* TMST_VALUE */
#define TMST_VALUE_LSH  0
#define TMST_VALUE_WID  8
/* ENABLE */
#define ACTIVE_LSH      7
#define ACTIVE_WID      1
#define STANDBY_LSH     6
#define STANDBY_WID     1
#define V3P3_SW_EN_LSH  5
#define V3P3_SW_EN_WID  1
#define VCOM_EN_LSH     4
#define VCOM_EN_WID     1
#define VDDH_EN_LSH     3
#define VDDH_EN_WID     1
#define VPOS_EN_LSH     2
#define VPOS_EN_WID     1
#define VEE_EN_LSH      1
#define VEE_EN_WID      1
#define VNEG_EN_LSH     0
#define VNEG_EN_WID     1
/* VP_ADJUST */
#define VDDH_SET_LSH    4
#define VDDH_SET_WID    3
#define VPOS_SET_LSH    0
#define VPOS_SET_WID    3
/* VN_ADJUST */
#define VCOM_ADJ_LSH    7
#define VCOM_ADJ_WID    1
#define VEE_SET_LSH     4
#define VEE_SET_WID     3
#define VNEG_SET_LSH    0
#define VNEG_SET_WID    3
/* VCOM_ADJUST */
#define VCOM_SET_LSH    0
#define VCOM_SET_WID    8
#define VCOM1_SET_LSH    0
#define VCOM1_SET_WID    8
#define VCOM2_SET_LSH    0
#define VCOM2_SET_WID    1
/* 2011/2/3 FY11 : Supported the function to save VCOM. */
#define VCOM_PROG_LSH    6
#define VCOM_PROG_WID    1
/* INT_ENABLE1 */
#define TSD_EN_LSH        6
#define TSD_EN_WID        1
#define HOT_EN_LSH        5
#define HOT_EN_WID        1
#define TMST_HOT_EN_LSH   4
#define TMST_HOT_EN_WID   1
#define TMST_COOL_EN_LSH  3
#define TMST_COOL_EN_WID  1
#define UVLO_EN_LSH       2
#define UVLO_EN_WID       1
/* 2011/2/3 FY11 : Supported the function to save VCOM. */
#define VCOM_PROGC_LSH    0
#define VCOM_PROGC_WID    1
/* INT_ENABLE2 */
#define VB_UV_EN_LSH      7
#define VB_UV_EN_WID      1
#define VDDH_UV_EN_LSH    6
#define VDDH_UV_EN_WID    1
#define VN_UV_EN_LSH      5
#define VN_UV_EN_WID      1
#define VPOS_UV_EN_LSH    4
#define VPOS_UV_EN_WID    1
#define VEE_UV_EN_LSH     3
#define VEE_UV_EN_WID     1
#define VNEG_UV_EN_LSH    1
#define VNEG_UV_EN_WID    1
#define EOC_EN_LSH        0
#define EOC_EN_WID        1
/* INT_STATUS1 */
#define TSDN_LSH        6
#define TSDN_WID        1
#define HOT_LSH         5
#define HOT_WID         1
#define TMST_HOT_LSH    4
#define TMST_HOT_WID    1
#define TMST_COOL_LSH   3
#define TMST_COOL_WID   1
#define UVLO_LSH        2
#define UVLO_WID        1
/* INT_STATUS2 */
#define VB_UV_LSH       7
#define VB_UV_WID       1
#define VDDH_UV_LSH     6
#define VDDH_UV_WID     1
#define VN_UV_LSH       5
#define VN_UV_WID       1
#define VPOS_UV_LSH     4
#define VPOS_UV_WID     1
#define VEE_UV_LSH      3
#define VEE_UV_WID      1
#define VNEG_UV_LSH     1
#define VNEG_UV_WID     1
#define EOC_LSH         0
#define EOC_WID         1
/* PWR_SEQ0 */
#define VDDH_SEQ_LSH    6
#define VDDH_SEQ_WID    2
#define VPOS_SEQ_LSH    4
#define VPOS_SEQ_WID    2
#define VEE_SEQ_LSH     2
#define VEE_SEQ_WID     2
#define VNEG_SEQ_LSH    0
#define VNEG_SEQ_WID    2
/* PWR_SEQ1 */
#define DLY1_LSH    4
#define DLY1_WID    4
#define DLY0_LSH    0
#define DLY0_WID    4
/* PWR_SEQ2 */
#define DLY3_LSH    4
#define DLY3_WID    4
#define DLY2_LSH    0
#define DLY2_WID    4
/* TMST_CONFIG */
#define READ_THERM_LSH      7
#define READ_THERM_WID      1
#define CONV_END_LSH        5
#define CONV_END_WID        1
#define FAULT_QUE_LSH       3
#define FAULT_QUE_WID       2
#define FAULT_QUE_CLR_LSH   2
#define FAULT_QUE_CLR_WID   1
/* TMST_OS */
#define TMST_HOT_SET_LSH    0
#define TMST_HOT_SET_WID    8
/* TMST_HYST */
#define TMST_COOL_SET_LSH   0
#define TMST_COOL_SET_WID   8
/* PG_STATUS */
#define VB_PG_LSH       7
#define VB_PG_WID       1
#define VDDH_PG_LSH     6
#define VDDH_PG_WID     1
#define VN_PG_LSH       5
#define VN_PG_WID       1
#define VPOS_PG_LSH     4
#define VPOS_PG_WID     1
#define VEE_PG_LSH      3
#define VEE_PG_WID      1
#define VNEG_PG_LSH     1
#define VNEG_PG_WID     1
/* REVID */
#define MJREV_LSH       6
#define MJREV_WID       2
#define MNREV_LSH       4
#define MNREV_WID       2
#define VERSION_LSH     0
#define VERSION_WID     4
/* FIX_READ_POINTER */
#define FIX_RD_PTR_LSH  0
#define FIX_RD_PTR_WID  1

/*
 * Regulator definitions
 *   *_MIN_uV  - minimum microvolt for regulator
 *   *_MAX_uV  - maximum microvolt for regulator
 *   *_STEP_uV - microvolts between regulator output levels
 *   *_MIN_VAL - minimum register field value for regulator
 *   *_MAX_VAL - maximum register field value for regulator
 */
/*
 * VCOM Definitions
 *
 * The register fields accept voltages in the range 0V to -2.75V, but the
 * VCOM parametric performance is only guaranteed from -0.3V to -2.5V.
 */
/* 2011/5/26 FY11 : Modified to use absolute value for VCOM (TPS65180/1). */
#define TPS65180_VCOM_MIN_uV	      0
#define TPS65180_VCOM_MAX_uV	2750000 
#define TPS65180_VCOM_MIN_SET         0
#define TPS65180_VCOM_MAX_SET       255
/* 2011/05/31 FY11 : Fixed invalid step uV. */
#define TPS65180_VCOM_BASE_uV     10784
#define TPS65180_VCOM_STEP_uV     10784
/* 2011/1/19 FY11 : Modified to use absolute value for VCOM. */
#define TPS65185_VCOM_MIN_uV  	      0
#define TPS65185_VCOM_MAX_uV    5110000
#define TPS65185_VCOM_MIN_SET         0
#define TPS65185_VCOM_MAX_SET       511
#define TPS65185_VCOM_BASE_uV     10000
#define TPS65185_VCOM_STEP_uV     10000

/* 2011/05/30 FY11 : Supported write vcom value to eMMC. */
#define VCOM_INFO_ADDR_OFFSET	8	// Attention!! This value must be consistent with EPD driver.

/*
 * UPSEQx and DWNSEQx
 */
#define UD_DLY1_LSH    2
#define UD_DLY1_WID    2
#define UD_DLY0_LSH    0
#define UD_DLY0_WID    2
#define UD_DLY3_LSH    6
#define UD_DLY3_WID    2
#define UD_DLY2_LSH    4
#define UD_DLY2_WID    2


/*
 * Timing Parameters
 */
enum {
	/* as specific event in TPS65180 */
	TPS65180_TP_VEE_PWRUP,
	TPS65180_TP_VNEG_PWRUP,
	TPS65180_TP_VPOS_PWRUP,
	TPS65180_TP_VDDH_PWRUP,
	TPS65180_TP_VDDH_PWRDN,
	TPS65180_TP_VPOS_PWRDN,
	TPS65180_TP_VNEG_PWRDN,
	TPS65180_TP_VEE_PWRDN,
};
#define TPS65180_TP_COUNT 8

struct tps65180_timings {
	unsigned int    vee_seq;
	unsigned int    vneg_seq;
	unsigned int    vpos_seq;
	unsigned int    vddh_seq;
	unsigned int    delay0;
	unsigned int    delay1;
	unsigned int    delay2;
	unsigned int    delay3;
};

enum {
	TPS65180_STROBE1,
	TPS65180_STROBE2,
	TPS65180_STROBE3,
	TPS65180_STROBE4,
};
/* Default value for power sequence register from datasheet */
#define TPS65180_DEFAULT_POWSEQ 0xE4


/* 2011/03/22 FY11 : Added retry for display regulator. */
#define TPS65185_DISP_ENABLE_RETRY	3
/*2011/05/31 FY11 : Added retry for first i2c communication with TPS65181. */
#define TPS6518x_1st_I2C_RETRY	3

/* Power sequencing order is Vee, Vneg, Vpos, Vddh, VCOM, updating epd, VCOM off, Vddh off, Vpos off, Vneg off, and then Vee off last. */
/* 2011/05/26 FY11 : Supported TPS65181. */
/* This timing is only for TPS65181. */
static struct tps65180_timings power_sequence_timing = {
	.vddh_seq = 3,
	.vpos_seq = 2,
	.vee_seq = 1,
	.vneg_seq = 0,
	.delay0 = 15,
	.delay3 = 15,
	.delay2 = 5,
	.delay1 = 2,
};


struct tps65180 {
	/* chip revision */
	int revId;

	struct device *dev;

	/* Platform connection */
	struct i2c_client *i2c_client;

	/* Client devices */
	struct platform_device *pdev[TPS65180_REG_NUM];

	/* GPIOs */
	int gpio_pmic_pwrgood;
	int gpio_pmic_vcom_ctrl;
	int gpio_pmic_wakeup;
	int gpio_pmic_intr;
	int gpio_pmic_pwr2_ctrl;
	int gpio_pmic_pwr0_ctrl;
/* 2011/05/26 FY11 : Supported TPS65181. */
	int gpio_pmic_v3p3_ctrl;
/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
	int gpio_pmic_vsys;

	int pass_num;
	int vcom_uV;

	bool pmic_setup;

	int max_wait;
};

/*
 * Internal Function Prototypes
 */
static int tps65180_register_regulator(struct tps65180 *tps65180, int reg,
			      struct regulator_init_data *initdata);

/*
 * TPS65180 device initialization and exit.
 */
int tps65180_device_init(struct tps65180 *tps65180, int irq,
		       struct tps65180_platform_data *pdata);
void tps65180_device_exit(struct tps65180 *tps65180);

/*
 * TPS65180 device IO
 */
static unsigned int to_reg_val(unsigned int cur_reg, unsigned int fld_mask,
                        unsigned int fld_val);

/*
 * TPS65180 Timing Parameters
 */
int tps65180_set_timings(struct regulator_dev *reg, const struct tps65180_timings *ts);

/*
 * to_reg_val(): Creates a register value with new data
 *
 * Creates a new register value for a particular field.  The data
 * outside of the new field is not modified.
 *
 * @cur_reg: current value in register
 * @reg_mask: mask of field bits to be modified
 * @fld_val: new value for register field.
 */
static unsigned int to_reg_val(unsigned int cur_reg, unsigned int fld_mask,
                        unsigned int fld_val)
{
	return (cur_reg & (~fld_mask)) | fld_val;
}

/*
 * Regulator operations
 */
/* 2011/01/19 FY11 : Fixed the bug cannot enable V3P3. */
static int iV3P3Enabled = 0;
static int tps65180_v3p3_is_enabled(struct regulator_dev *reg)
{
	return iV3P3Enabled;
}


static int tps65180_v3p3_enable(struct regulator_dev *reg)
{
	int ret=0;
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int new_reg_val; /* new register value to write */

/* 2011/05/26 FY11 : Supported TPS65181. */
	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
		case 0 : /* TPS65180 */
		case 1 : /* TPS65181 */
			gpio_set_value(tps65180->gpio_pmic_v3p3_ctrl, 0);
			break;
		case 5 : /* TPS65185 */
		case 6 : /* TPS65186 */
			ret = i2c_smbus_read_byte_data(client, REG_TPS65180_ENABLE);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s fail to read i2c. %d\n", __func__, ret );
				break;
			}
			cur_reg_val = (unsigned int)ret;
			new_reg_val = to_reg_val(cur_reg_val,
						 BITFMASK(V3P3_SW_EN),
						 BITFVAL(V3P3_SW_EN, true));

			ret = i2c_smbus_write_byte_data(client, REG_TPS65180_ENABLE, new_reg_val);
			break;
		default :
			ret = -EINVAL;
			break;
	}


/* 2011/01/19 FY11 : Fixed the bug cannot enable V3P3. */
	if ( ret == 0 )
	{
		iV3P3Enabled = 1;
	}

	return ret;
}

static int tps65180_v3p3_disable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int new_reg_val; /* new register value to write */
	int ret = 0;

/* 2011/05/26 FY11 : Supported TPS65181. */
	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
		case 0 : /* TPS65180 */
		case 1 : /* TPS65181 */
			gpio_set_value(tps65180->gpio_pmic_v3p3_ctrl, 1);
			break;
		case 5 : /* TPS65185 */
		case 6 : /* TPS65186 */
			ret = i2c_smbus_read_byte_data(client, REG_TPS65180_ENABLE);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s fail to read i2c. %d\n", __func__, ret );
				break;
			}
			cur_reg_val = (unsigned int)ret;
			new_reg_val = to_reg_val(cur_reg_val,
						 BITFMASK(V3P3_SW_EN),
						 BITFVAL(V3P3_SW_EN, false));

			ret = i2c_smbus_write_byte_data(client, REG_TPS65180_ENABLE, new_reg_val);
			break;
		default :
			ret = -EINVAL;
			break;
	}

/* 2011/01/19 FY11 : Fixed the bug cannot enable V3P3. */
	if ( ret == 0 )
	{
		iV3P3Enabled = 0;
	}

	return ret;
}

static int set_power_sequence(struct tps65180 *tps65180,
                              const struct tps65180_timings *ts)
{
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int	reg_val;
	int retval;

	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
	   case 0 : /* TPS65180 */
	   case 1 : /* TPS65181 */
		reg_val = BITFVAL(VNEG_SEQ, ts->vneg_seq) |
		  BITFVAL(VPOS_SEQ, ts->vpos_seq) |
		  BITFVAL(VEE_SEQ,  ts->vee_seq)  |
		  BITFVAL(VDDH_SEQ, ts->vddh_seq) ;

		retval = i2c_smbus_write_byte_data(client, REG_TPS65180_PWR_SEQ0, reg_val);
		break;
	   case 5 : /* TPS65185 */
		/*
		 * set the power up sequence
		 */
		reg_val = 0xe4; /* Vneg -> Vnee -> Vpos -> Vddh power sequence */
		retval = i2c_smbus_write_byte_data(client, REG_TPS65185_UPSEQ0, reg_val);
		if (retval)
		   break;
		/*
		 * set the power down sequence
		 */
		reg_val = 0x1e; /* Vddh -> Vpos -> Vneg -> Vee power sequence */
		retval = i2c_smbus_write_byte_data(client, REG_TPS65185_DWNSEQ0, reg_val);
		break;
	   default :
		retval = -1;
	}
	return retval;
}

static int set_power_delays(struct tps65180 *tps65180,
                            const struct tps65180_timings *ts)
{
/* 2011/03/22 FY11 : Modified to handle error. */
	int ret = 0;
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int	reg_val;

	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
	   case 0 : /* TPS65180 */
	   case 1 : /* TPS65181 */
/* 2011/05/26 FY11 : Supported TPS65181 to handle error. */
		reg_val = BITFVAL(DLY0, ts->delay0) | BITFVAL(DLY1, ts->delay1) ;
		ret = i2c_smbus_write_byte_data(client, REG_TPS65180_PWR_SEQ1, reg_val);
		if ( ret < 0 )
		{
			return ret;
		}

		reg_val = BITFVAL(DLY2, ts->delay2) | BITFVAL(DLY3, ts->delay3) ;
		ret = i2c_smbus_write_byte_data(client, REG_TPS65180_PWR_SEQ2, reg_val);
		if ( ret < 0 )
		{
			return ret;
		}
		break;
	   case 5 : /* TPS65185 */
	   case 6 : /* TPS65186 */
		/*
		 * UPSEQ1
		 */
		reg_val = 0x55 ; /* all interval are 6 mSec */
		ret = i2c_smbus_write_byte_data(client, REG_TPS65185_UPSEQ1, reg_val);
		if ( ret < 0 )
		{
			return ret;
		}
		/*
		 * DWNSEQ1
		 */
		reg_val = 0xE0 ; /* 3 mSec - strobe1, 6 mSec - strobe2, 24 mSec - strobe3, 48 mSec - strobe4 */
		ret = i2c_smbus_write_byte_data(client, REG_TPS65185_DWNSEQ1, reg_val);
		if ( ret < 0 )
		{
			return ret;
		}
		break;
	}

	return ret;
}

int tps65180_set_timings(struct regulator_dev *reg, const struct tps65180_timings *ts)
{
/* 2011/03/22 FY11 : Modified to handle error. */
	int ret = 0;
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);

	ret = set_power_sequence(tps65180, ts);
	if ( ret <  0 )
	{
		printk (KERN_ERR "%s fail to set power sequence. %d\n", __func__, ret );
		return ret;
	}

	ret = set_power_delays(tps65180, ts);
	if ( ret <  0 )
	{
		printk (KERN_ERR "%s fail to set power delays. %d\n", __func__, ret );
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(tps65180_set_timings);

static int vcom_rs_to_uV(unsigned int reg_setting)
{
	if (reg_setting <= TPS65180_VCOM_MIN_SET)
		return TPS65180_VCOM_MIN_uV;
	if (reg_setting >= TPS65180_VCOM_MAX_SET)
		return TPS65180_VCOM_MAX_uV;
/* 2011/5/26 FY11 : Modified to use absolute value for VCOM (TPS65180/1). */
	return (reg_setting * TPS65180_VCOM_STEP_uV);
}
static int vcom2_rs_to_uV(unsigned int reg_setting)
{
	if (reg_setting <= TPS65185_VCOM_MIN_SET)
		return TPS65185_VCOM_MIN_uV;
	if (reg_setting >= TPS65185_VCOM_MAX_SET)
		return TPS65185_VCOM_MAX_uV;
/* 2011/1/19 FY11 : Fixed the bug returns minus value. */
	return (reg_setting * TPS65185_VCOM_STEP_uV);
}


static int vcom_uV_to_rs(int uV)
{
	if (uV <= TPS65180_VCOM_MIN_uV)
		return TPS65180_VCOM_MIN_SET;
	if (uV >= TPS65180_VCOM_MAX_uV)
		return TPS65180_VCOM_MAX_SET;
/* 2011/5/26 FY11 : Modified to use absolute value for VCOM (TPS65180/1). */
/* 2011/05/31 FY11 : Modified to calcurate more accurate val. */
	return (uV + TPS65180_VCOM_STEP_uV/2) / TPS65180_VCOM_STEP_uV;
}

static int vcom2_uV_to_rs(int uV)
{
	if (uV <= TPS65185_VCOM_MIN_uV)
		return TPS65185_VCOM_MIN_SET;
	if (uV >= TPS65185_VCOM_MAX_uV)
		return TPS65185_VCOM_MAX_SET;
/* 2011/1/19 FY11 : Changed argument spec (Not use minus value). */
/* 2011/05/31 FY11 : Modified to calcurate more accurate val. */
	return (uV + TPS65185_VCOM_STEP_uV/2) / TPS65185_VCOM_STEP_uV;
}

/* 2011/05/26 FY11 : Supported TPS65181. */
static int tps65180_vcom_set_voltage_to_pmic(struct regulator_dev *reg,
                                     int minuV, int uV)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int new_reg_val; /* new register value to write */
	int retval;
	int max_retry = 100;

	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
	   case 0 : /* TPS65180 */
	   case 1 : /* TPS65181 */
		retval = i2c_smbus_read_byte_data(client, REG_TPS65180_VCOM_ADJUST);
		if ( retval < 0 )
		{
			printk( KERN_ERR "%s fail to read i2c. %d\n", __func__, retval );
			break;
		}
		cur_reg_val = (unsigned int)retval;
		new_reg_val = to_reg_val(cur_reg_val,
				 BITFMASK(VCOM_SET),
				 BITFVAL(VCOM_SET, vcom_uV_to_rs(uV)));

		retval = i2c_smbus_write_byte_data(client, REG_TPS65180_VCOM_ADJUST,
	                          new_reg_val);
		break;
	   case 5 : /* TPS65185 */
	   case 6 : /* TPS65186 */
		retval = i2c_smbus_write_byte_data(client, REG_TPS65185_VCOM1,
	                          vcom2_uV_to_rs(uV) & 255);
		if (retval)
		   break;
		retval = i2c_smbus_read_byte_data(client, REG_TPS65185_VCOM2);
		if ( retval < 0 )
		{
			printk( KERN_ERR "%s fail to read i2c. %d\n", __func__, retval );
			break;
		}
		cur_reg_val = (unsigned int)retval;
		new_reg_val = to_reg_val(cur_reg_val,
				 BITFMASK(VCOM2_SET),
				 BITFVAL(VCOM2_SET, vcom2_uV_to_rs(uV)/256));

		retval = i2c_smbus_write_byte_data(client, REG_TPS65185_VCOM2,
	                          new_reg_val);
		if ( retval )
			break;


/* 2011/2/3 FY11 : Supported the function to save VCOM. */
		// set program bit
		new_reg_val = to_reg_val(new_reg_val,
				 BITFMASK(VCOM_PROG),
				 BITFVAL(VCOM_PROG, 1));
		retval = i2c_smbus_write_byte_data(client, REG_TPS65185_VCOM2,
	                          new_reg_val);
		if ( retval )
		{
			break;
		}

		// wait completion
		while (max_retry)
		{
			retval = i2c_smbus_read_byte_data(client, REG_TPS65185_INT_STATUS1);
			if ( retval < 0 )
			{
				printk( KERN_ERR "%s fail to read i2c. %d\n", __func__, retval );
				break;
			}
			cur_reg_val = (unsigned int)retval;
			if ( cur_reg_val & BITFVAL(VCOM_PROGC, 1) )
			{
				break;
			}
			max_retry--;
			msleep(10);
		}

		// check timeout
		if ( max_retry == 0 )
		{
			printk( KERN_ERR "%s retry out!\n", __func__ );
			retval = -ETIMEDOUT;
		}

		break;
	   default :
		retval = -1;
	}
	return retval;
}

/* 2011/05/26 FY11 : Supported TPS65181. */
static int tps65180_vcom_set_voltage(struct regulator_dev *reg,
                                     int minuV, int uV)
{
	int ret = 0;

/* 2011/05/26 FY11 : Supported write vcom value to eMMC. */
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);

	/* save vcom voltage to storage. */
	unsigned long ulWfIndex, ulWfAreaSize;
	unsigned long ulVcomRegVal;

	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
	   case 0 : /* TPS65180 */
	   case 1 : /* TPS65181 */
		ulVcomRegVal = vcom_uV_to_rs(uV);
		uV = vcom_rs_to_uV(ulVcomRegVal);
		break;
	   case 5 : /* TPS65185 */
	   case 6 : /* TPS65186 */
		ulVcomRegVal = vcom2_uV_to_rs(uV);
		uV = vcom2_rs_to_uV(ulVcomRegVal);
		break;
	}

	ulWfIndex = rawdata_index("Waveform", &ulWfAreaSize);
	ret = rawdata_write( 	ulWfIndex, 
				ulWfAreaSize -VCOM_INFO_ADDR_OFFSET, 
				(char*)&ulVcomRegVal, 
				sizeof(ulVcomRegVal));
	if ( ret < 0 )
	{
		printk ( KERN_ERR "%s fail to write to eMMC.\n", __func__ );
		return ret;
	}


	ret = tps65180_vcom_set_voltage_to_pmic( reg, minuV, uV );
	if ( ret < 0 )
	{
		printk ( KERN_ERR "%s fail to set vcom to pmic.\n", __func__ );
		return ret;
	}

	tps65180->vcom_uV = uV;
	return ret;
}

static int tps65180_vcom_get_voltage(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value */
	unsigned int cur_fld_val; /* current bitfield value*/
	int vcomValue;
	int ret = 0;

	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
	   case 0 : /* TPS65180 */
	   case 1 : /* TPS65181 */
		ret = i2c_smbus_read_byte_data(client, REG_TPS65180_VCOM_ADJUST);
		if ( ret < 0 )
		{
			printk( KERN_ERR "%s fail to read i2c. %d\n", __func__, ret );
			break;
		}
		cur_reg_val = (unsigned int)ret;
		cur_fld_val = BITFEXT(cur_reg_val, VCOM_SET);
		vcomValue = vcom_rs_to_uV(cur_fld_val);
		break;
	   case 5 : /* TPS65185 */
	   case 6 : /* TPS65186 */
		ret = i2c_smbus_read_byte_data(client, REG_TPS65185_VCOM1);
		if ( ret < 0 )
		{
			printk( KERN_ERR "%s fail to read i2c. %d\n", __func__, ret );
			break;
		}
		cur_reg_val = (unsigned int)ret;
		cur_reg_val |= 256 * (1 & i2c_smbus_read_byte_data(client, REG_TPS65185_VCOM2));
		vcomValue = vcom2_rs_to_uV(cur_reg_val);
		break;
	   default:
		vcomValue = 0;
	}
	
	return vcomValue;

}

/* 2011/01/21 FY11 : Fixed the bug cannot enable VCOM. */
static int tps65180_vcom_is_enabled(struct regulator_dev *reg )
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	int gpio = 0;
	gpio = gpio_get_value(tps65180->gpio_pmic_vcom_ctrl);
	if ( gpio == 0 )
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

static int tps65180_vcom_enable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
#if CUSTOM_RAILS
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int new_reg_val; /* new register value to write */
	int ret;

	ret = i2c_smbus_read_byte_data(client, REG_TPS65180_ENABLE);
	if ( ret < 0 )
		return ret;
	cur_reg_val = (unsigned int)ret;
	new_reg_val = to_reg_val(cur_reg_val,
				 BITFMASK(VCOM_EN),
				 BITFVAL(VCOM_EN, true));

	return i2c_smbus_write_byte_data(client, REG_TPS65180_ENABLE, new_reg_val);
#else
	gpio_set_value(tps65180->gpio_pmic_vcom_ctrl,1);
	return 0;
#endif
}

static int tps65180_vcom_disable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
#if CUSTOM_RAILS
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int new_reg_val; /* new register value to write */
	int ret;

	ret = i2c_smbus_read_byte_data(client, REG_TPS65180_ENABLE);
	if ( ret < 0 )
		return ret;
	cur_reg_val = (unsigned int)ret;
	new_reg_val = to_reg_val(cur_reg_val,
				 BITFMASK(VCOM_EN),
				 BITFVAL(VCOM_EN, false));

	return i2c_smbus_write_byte_data(client, REG_TPS65180_ENABLE, new_reg_val);
#else
	gpio_set_value(tps65180->gpio_pmic_vcom_ctrl,0);
	return 0;
#endif
}

static int tps65180_wait_power_good(struct tps65180 *tps65180)
{
	int i;

	for (i = 0; i < tps65180->max_wait * 3; i++) {
/* 2011/1/26 FY11 : Fixed the bug cannot wait power_good. */
		if (gpio_get_value(tps65180->gpio_pmic_pwrgood))
			return 0;
		else
			msleep(10);

	}
	return -ETIMEDOUT;
}

/* 2011/1/24 FY11 : Added wakeup port control. */
static void tps65180_wakeup(struct tps65180 *tps65180)
{
	if(0==gpio_get_value(tps65180->gpio_pmic_wakeup))
	{
		//It was in SLEEP mode
		//which is the lowest power mode of operation.
		//All internal circuitry is turned off, registers
		//are reset to default values and the device does not
		//respond to I2C communications.
		gpio_set_value(tps65180->gpio_pmic_wakeup, 1);
		msleep(15);
		tps65180->pmic_setup = false;
	}
}


static void tps65180_sleep(struct tps65180 *tps65180)
{
	gpio_set_value(tps65180->gpio_pmic_wakeup, 0);
	tps65180->pmic_setup = false;
}


/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */

static int tps65180_vsys_is_enabled(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	int gpio = gpio_get_value(tps65180->gpio_pmic_vsys);

	if (gpio == 0)
		return 0;
	else
		return 1;
}

static int tps65180_vsys_enable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	gpio_set_value(tps65180->gpio_pmic_vsys, 1);
	printk(KERN_INFO "%s : wait VSYS_EPD.\n", __func__ );
	msleep(20);
	return 0;
}

static int tps65180_vsys_disable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	gpio_set_value(tps65180->gpio_pmic_vsys, 0);
	printk(KERN_INFO "%s : VSYS_EPD OFF.\n", __func__ );
	return 0;
}


/* 2011/05/30 FY11 : Supported to read vcom from eMMC. */
static int tps65180_display_first_enable = 1;

static int tps65180_display_enable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int fld_mask;	  /* register mask for bitfield to modify */
	unsigned int fld_val;	  /* new bitfield value to write */
	unsigned int new_reg_val; /* new register value to write */
	int ret = 0;
/* 2011/03/22 FY11 : Added retry for display regulator. */
	int retry;

/* 2011/05/30 FY11 : Supported to read vcom from eMMC. */
	if(tps65180_display_first_enable)
	{
		unsigned long ulWfIndex, ulWfAreaSize;
		unsigned long ulVcomRegVal;
		ulWfIndex = rawdata_index("Waveform", &ulWfAreaSize);
		ret = rawdata_read( 	ulWfIndex, 
					ulWfAreaSize -VCOM_INFO_ADDR_OFFSET, 
					(char*)&ulVcomRegVal, 
					sizeof(ulVcomRegVal));
		if ( ret < 0 )
		{
			printk( KERN_ERR "%s fail to read vcom from eMMC. %d\n", __func__, ret );
			return ret;
		}

		switch (TPS6518x_REVID_VERSION(tps65180->revId))
		{
			case 1 : /* TPS65181 */
				tps65180->vcom_uV = vcom_rs_to_uV(ulVcomRegVal);
				break;

			case 5 : /* TPS65185 */
				tps65180->vcom_uV = vcom2_rs_to_uV(ulVcomRegVal);
				break;
		}
		tps65180_display_first_enable = 0;
	}


/* 2011/1/24 FY11 : Added wakeup port control. */
	tps65180_wakeup(tps65180);

	/* check if pmic_setup is required */
	if (tps65180->pmic_setup == false) {
	   switch (TPS6518x_REVID_VERSION(tps65180->revId))
	   {
	      case 0 : /* TPS65180 */
	      case 1 : /* TPS65181 */
#if MANUAL_VCOM_ADJ
		/* TPS65180 has VCOM_ADJ bit set by default.  If this bit is set VCOM is adjusted by I2C only, */
		cur_reg_val = i2c_smbus_read_byte_data(client, REG_TPS65180_VN_ADJUST);
		fld_mask = BITFMASK(VCOM_ADJ);
		fld_val = BITFVAL(VCOM_ADJ, false); /* set VCOM by potentiometer only */
		new_reg_val = to_reg_val(cur_reg_val, fld_mask, fld_val);
		i2c_smbus_write_byte_data(client, REG_TPS65180_VN_ADJUST, new_reg_val);

#else
		/* TPS65180 has VCOM_ADJ bit set by default.  If this bit is set VCOM is adjusted by I2C only, */
/*2011/05/31 FY11 : Added retry for first i2c communication with TPS65181. */
		retry = TPS6518x_1st_I2C_RETRY;
		while ( retry >= 0 )
		{
			ret = i2c_smbus_read_byte_data(client, REG_TPS65180_VN_ADJUST);
			if ( ret < 0 )
			{
				retry--;
				if ( retry < 0 )
				{
					printk( KERN_ERR "%s i2c access retry out. %d\n",
						__func__,  ret );
					tps65180_sleep(tps65180); 
					return ret;
				}
//				printk( KERN_ERR "%s i2c access retry. %d\n",
//					__func__,  TPS65185_DISP_ENABLE_RETRY - retry );
				msleep(50);	
			}
			else
			{
/* 2011/06/13 FY11 : Modified wait time for retry after first i2c success. (1.8msec -> 10msec) */
				msleep(10);
				ret = i2c_smbus_read_byte_data(client, REG_TPS65180_VN_ADJUST);
				if ( ret < 0 )
				{
					printk( KERN_ERR "%s final i2c access retry out. %d\n",
						__func__,  ret );
					tps65180_sleep(tps65180); 
					return ret;
				}
				cur_reg_val = (unsigned int)ret;
				break;
			}
		}
		fld_mask = BITFMASK(VCOM_ADJ);
		fld_val = BITFVAL(VCOM_ADJ, true); /* set VCOM by i2c */
		new_reg_val = to_reg_val(cur_reg_val, fld_mask, fld_val);
		ret = i2c_smbus_write_byte_data(client, REG_TPS65180_VN_ADJUST, new_reg_val);
		if ( ret < 0 )
		{
			printk( KERN_ERR "%s fail to write i2c. %d\n", __func__,  ret );
			tps65180_sleep(tps65180); 
			return ret;
		}

		/* set the vcom to default value */
/* 2011/05/26 FY11 : Supported TPS65181. */
		ret = tps65180_vcom_set_voltage_to_pmic(reg,tps65180->vcom_uV,tps65180->vcom_uV);
		if ( ret < 0 )
		{
			tps65180_sleep(tps65180); 
			return ret;
		}

#endif
		/* set the power sequencing */
		ret = tps65180_set_timings(reg, &power_sequence_timing);
		if ( ret < 0 )
		{
			tps65180_sleep(tps65180); 
			return ret;
		}

/* 2011/05/26 FY11 : Supported TPS65181. */
		/* set read therm. */
		ret = i2c_smbus_read_byte_data(client, REG_TPS65180_TMST_CONFIG);
		if ( ret < 0 )
		{
			printk( KERN_ERR "%s fail to read TMST_CONFIG reg. %d\n", __func__, ret );
			tps65180_sleep(tps65180);
			return ret;
		}
		cur_reg_val = (unsigned int)ret;
		new_reg_val = to_reg_val(cur_reg_val,
					 BITFMASK(READ_THERM),
					 BITFVAL(READ_THERM, true));
		ret = i2c_smbus_write_byte_data(client, REG_TPS65180_TMST_CONFIG, new_reg_val);
		if ( ret < 0 )
		{
			printk( KERN_ERR "%s fail to write TMST_CONFIG reg. %d\n", __func__, ret );
			tps65180_sleep(tps65180);
			return ret;
		}
/* 2011/06/07 FY11 : Modified to wait completion of AD convert. */
		udelay(400);
		
		break;
	      case 5: /* TPS65185 */
	      case 6: /* TPS65186 */
		retry = TPS65185_DISP_ENABLE_RETRY;
/* 2011/1/19 FY11 : Remove default value setting. (Use default value stored in non-volatile memory.) */
		/* set the vcom to default value */
//		tps65180_vcom_set_voltage(reg,tps65180->vcom_uV,tps65180->vcom_uV);
		/* set the power sequencing */
/* 2011/03/22 FY11 : Modified to handle error. */
		while ( retry > 0 )
		{
			ret = tps65180_set_timings(reg, &power_sequence_timing);
			if ( ret == 0 )
			{
				break;
			}
			printk( KERN_ERR "%s fails to set timing.(%d) retry(%d)\n", __func__, ret, retry );
			retry--;
			if ( retry == 0 )
			{
				msleep(1000);
			}
			else
			{
				msleep(100);
			}
			continue;
		}
		if ( ret < 0 )
		{
			printk( KERN_ERR "%s set timing retry out!\n", __func__ );
		}
		break;

		default:
			ret = -EINVAL;
			return ret;
	   }

	   /* setup is complete */
	   tps65180->pmic_setup = true;
	}
#if SWITCH_3V3
	/* turn on 3p3 rail */
	cur_reg_val = i2c_smbus_read_byte_data(client, REG_TPS65180_ENABLE);
	fld_mask = BITFMASK(V3P3_SW_EN);
	fld_val = BITFVAL(V3P3_SW_EN, true);
	new_reg_val = to_reg_val(cur_reg_val, fld_mask, fld_val);
	i2c_smbus_write_byte_data(client, REG_TPS65180_ENABLE, new_reg_val);

	msleep(1);
#endif

	return ret;
}


static int tps65180_display_disable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);

/* 2011/1/24 FY11 : Added wakeup port control. */
	tps65180_sleep(tps65180);

	return 0;
}

static int tps65180_display_is_enabled(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	int gpio = gpio_get_value(tps65180->gpio_pmic_wakeup);

	if (gpio == 0)
		return 0;
	else
		return 1;
}

static int tps65180_Temp_enable(struct regulator_dev *reg)
{

	return 0;
}

static int tps65180_Temp_disable(struct regulator_dev *reg)
{

	return 0;
}

static int tps65180_Temp_get_temperature(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);

	struct i2c_client *client = tps65180->i2c_client;
	unsigned int reg_val, new_reg_val;
	int ret = 0, retry = 100;

/*2011/05/26 FY11 : Supported TPS65181. */
	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
		case 0 : /* TPS65180 */
		case 1 : /* TPS65181 */
			break;
		case 5 : /* TPS65185 */
		case 6 : /* TPS65186 */
/* 2011/1/31 FY11: Supported manual read temperature. */
			// read TMST1
			ret = i2c_smbus_read_byte_data(client,
					REG_TPS65185_TMST1);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s fail to read TMST1 reg. %d\n", __func__, ret );
				return ret;
			}
			reg_val = (unsigned int)ret;

			// set READ_THERM bit
			new_reg_val = to_reg_val( reg_val, BITFMASK(READ_THERM), BITFVAL(READ_THERM, 1) );
			ret = i2c_smbus_write_byte_data( client, REG_TPS65185_TMST1, new_reg_val);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s fail to write TMST1 reg. %d\n", __func__, ret );
				return ret;
			}

/* 2011/05/25 FY11 : Changed the flag to confirm completion of reading temperature. */
			// wait EOC of INT2
			while( retry > 0 )
			{
				ret = i2c_smbus_read_byte_data( client, REG_TPS6518x_INT_STATUS2  );
				if ( ret < 0 )
				{
					printk( KERN_ERR "%s fail to read INT_STATUS2 reg. %d\n", __func__, ret );
					return ret;
				}
				reg_val = (unsigned int)ret;
				if ( reg_val & BITFMASK(EOC))
				{
					break;
				}
				udelay(10);
				retry--;
			}

			if ( retry <= 0 )
			{
				printk (KERN_ERR "%s: retry out.\n", __func__ );
				return -ETIMEDOUT;
			}

			break;
		default:
			ret = -EINVAL;
			return ret;
	}

	// read temperature
	reg_val = i2c_smbus_read_byte_data(client,
					REG_TPS65180_TMST_VALUE);
#if 1 /* E_BOOK */
/* 2010/12/10 FY11 : Fixed the bug that temperature is twice of correct value. */
	return reg_val; // Temperature in 1 degrees increments
#endif /* E_BOOK */
}

/* 2011/05/26 FY11 : Supported TPS65181. */
static int iPowerUpEnabled = 0;

/* 2011/01/21 FY11 : Fixed the bug cannot enable PWRUP. */
static int epdc_pwr0_is_enabled(struct regulator_dev *reg )
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	int ret = 0, i;

/* 2011/05/26 FY11 : Supported TPS65181. */
// modify to support tps65181
// use enabled flag
/* 2011/07/05 FY11 : Added workaround for EPD PMIC heat up. */
	if ( iPowerUpEnabled )
	{
		// if power is alredy ON, check PWRSTAT
		ret = 0;
		for ( i = 2; i > 0; i-- )
		{
			if(gpio_get_value(tps65180->gpio_pmic_pwrgood))
			{
				ret = iPowerUpEnabled;
				break;
			}
			msleep(100);
		}
	}
	else
	{
		// if power is not ON yet, return current status.
		ret = iPowerUpEnabled;
	}
	return ret;
}

static int epdc_pwr0_enable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);
	int ret = 0;

/* 2011/05/26 FY11 : Supported TPS65181. */
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int new_reg_val; /* new register value to write */
	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
		case 0 : /* TPS65180 */
		case 1 : /* TPS65181 */
			ret = i2c_smbus_read_byte_data(client, REG_TPS65180_ENABLE);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s i2c failed %d\n", __func__, ret );
				return ret;
			}
			cur_reg_val = (unsigned int)ret;
			new_reg_val = to_reg_val(cur_reg_val,
						 BITFMASK(ACTIVE),
						 BITFVAL(ACTIVE, true));
			ret = i2c_smbus_write_byte_data(client, REG_TPS65180_ENABLE, new_reg_val);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s i2c failed %d\n", __func__, ret );
				return ret;
			}

			ret = tps65180_wait_power_good(tps65180);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s wait power good failed %d\n", __func__, ret );
				new_reg_val = to_reg_val(cur_reg_val,
							 BITFMASK(STANDBY),
							 BITFVAL(STANDBY, true));
				i2c_smbus_write_byte_data(client, REG_TPS65180_ENABLE, new_reg_val);
			}
			break;
		case 5 : /* TPS65185 */
		case 6 : /* TPS65186 */
			gpio_set_value(tps65180->gpio_pmic_pwr0_ctrl, 1);
/* 2011/1/24 FY11 : Added wait for power good. */
			ret = tps65180_wait_power_good(tps65180);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s wait power good failed %d\n", __func__, ret );
				gpio_set_value(tps65180->gpio_pmic_pwr0_ctrl, 0);
			}
			break;
		default :
			ret = -EINVAL;
			return ret;
	}

/* 2011/05/26 FY11 : Supported TPS65181. */
	if ( ret == 0 )
	{
		iPowerUpEnabled = 1;
	}

	return ret;
}

static int epdc_pwr0_disable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);

/* 2011/05/26 FY11 : Supported TPS65181. */
	int ret = 0;
	struct i2c_client *client = tps65180->i2c_client;
	unsigned int cur_reg_val; /* current register value to modify */
	unsigned int new_reg_val; /* new register value to write */
	switch (TPS6518x_REVID_VERSION(tps65180->revId))
	{
		case 0 : /* TPS65180 */
		case 1 : /* TPS65181 */
			ret = i2c_smbus_read_byte_data(client, REG_TPS65180_ENABLE);
			if ( ret < 0 )
			{
				printk( KERN_ERR "%s i2c failed %d\n", __func__, ret );
				return ret;
			}
			cur_reg_val = (unsigned int)ret;
			new_reg_val = to_reg_val(cur_reg_val,
						 BITFMASK(STANDBY),
						 BITFVAL(STANDBY, true));
			ret = i2c_smbus_write_byte_data(client, REG_TPS65180_ENABLE, new_reg_val);
			break;
		case 5 : /* TPS65185 */
		case 6 : /* TPS65186 */
			gpio_set_value(tps65180->gpio_pmic_pwr0_ctrl, 0);
			break;
		default :
			return -EINVAL;
	}

/* 2011/03/30 FY11 : Fixed power down sequence. */
/* 2011/06/08 FY11 : Modified wait time. (80msec -> 200msec) */
	msleep(200);

/* 2011/05/26 FY11 : Supported TPS65181. */
	if ( ret == 0 )
	{
		iPowerUpEnabled = 0;
	}
	return 0;

}

static int epdc_pwr2_enable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);

	gpio_set_value(tps65180->gpio_pmic_pwr2_ctrl, 0);

	return 0;

}

static int epdc_pwr2_disable(struct regulator_dev *reg)
{
	struct tps65180 *tps65180 = rdev_get_drvdata(reg);

	gpio_set_value(tps65180->gpio_pmic_pwr2_ctrl, 1);

	return 0;

}

/*
 * Regulator operations
 */

static struct regulator_ops tps65180_display_ops = {
	.enable = tps65180_display_enable,
	.disable = tps65180_display_disable,
	.is_enabled = tps65180_display_is_enabled,
};

static struct regulator_ops tps65180_vcom_ops = {
	.enable = tps65180_vcom_enable,
	.disable = tps65180_vcom_disable,
/* 2011/01/21 FY11 : Fixed the bug cannot enable PWRCOM(VCOM). */
	.is_enabled = tps65180_vcom_is_enabled,
	.get_voltage = tps65180_vcom_get_voltage,
	.set_voltage = tps65180_vcom_set_voltage,
};

static struct regulator_ops tps65180_v3p3_ops = {
	.enable = tps65180_v3p3_enable,
	.disable = tps65180_v3p3_disable,
/* 2011/01/19 FY11 : Fixed the bug cannot enable V3P3. */
	.is_enabled = tps65180_v3p3_is_enabled,
};

static struct regulator_ops tps65180_Temp_ops = {
	.enable = tps65180_Temp_enable,
	.disable = tps65180_Temp_disable,
	.get_voltage = tps65180_Temp_get_temperature,
};

static struct regulator_ops epdc_pwr0_ops = {
	.enable = epdc_pwr0_enable,
	.disable = epdc_pwr0_disable,
/* 2011/01/21 FY11 : Fixed the bug cannot enable PWRUP. */
	.is_enabled = epdc_pwr0_is_enabled,
};

static struct regulator_ops epdc_pwr2_ops = {
	.enable = epdc_pwr2_enable,
	.disable = epdc_pwr2_disable,
};

/* 2011/07/05 FY11 : Added vsys regulator. */
static struct regulator_ops tps65180_vsys_ops = {
	.enable = tps65180_vsys_enable,
	.disable = tps65180_vsys_disable,
	.is_enabled = tps65180_vsys_is_enabled,
};


/*
 * Regulator descriptors
 */
static struct regulator_desc tps65180_reg[TPS65180_NUM_REGULATORS] = {
{
	.name = "DISPLAY",
	.id = TPS65180_DISPLAY,
	.ops = &tps65180_display_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "VCOM",
	.id = TPS65180_VCOM,
	.ops = &tps65180_vcom_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "V3P3_CTRL",
	.id = TPS65180_V3P3,
	.ops = &tps65180_v3p3_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "PMIC_TEMP",
	.id = TPS65180_TEMP,
	.ops = &tps65180_Temp_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "PWR0_CTRL",
	.id = EPD_PWR0_CTRL,
	.ops = &epdc_pwr0_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "PWR2_CTRL",
	.id = EPD_PWR2_CTRL,
	.ops = &epdc_pwr2_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
{
	.name = "VSYS_EPD",
	.id = TPS65180_VSYS,
	.ops = &tps65180_vsys_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
};

/*
 * Regulator init/probing/exit functions
 */
static int tps65180_regulator_probe(struct platform_device *pdev)
{
	struct regulator_dev *rdev;

	rdev = regulator_register(&tps65180_reg[pdev->id], &pdev->dev,
				  pdev->dev.platform_data,
				  dev_get_drvdata(&pdev->dev));

	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "failed to register %s\n",
			tps65180_reg[pdev->id].name);
		return PTR_ERR(rdev);
	}

	return 0;
}

static int tps65180_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	regulator_unregister(rdev);
	return 0;
}

static struct platform_driver tps65180_regulator_driver = {
	.probe = tps65180_regulator_probe,
	.remove = tps65180_regulator_remove,
	.driver = {
		.name = "tps65180-reg",
	},
};

static int tps65180_register_regulator(struct tps65180 *tps65180, int reg,
				     struct regulator_init_data *initdata)
{
	struct platform_device *pdev;
	int ret;
/*2011/05/31 FY11 : Added retry for first i2c communication with TPS65181. */
	int retry = TPS6518x_1st_I2C_RETRY;

	struct i2c_client *client = tps65180->i2c_client;

i2c_retry:
	/* If we can't find PMIC via I2C, we should not register regulators */
	tps65180->revId = i2c_smbus_read_byte_data(client,REG_TPS65180_REVID);
/* 2011/2/7 FY11 : Supported TPS65185 1.1 . */
/* 2011/03/24 FY11 : Supported TPS65185 1.2 . */
/* 2011/05/26 FY11 : Modified not to check full revision. */
#if 0
	/*
	 * support TPS65180/181 1.2, TPS65185/186 1.0, TPS65185 1.1 and TPS65185 1.2
	 */
	if (!((tps65180->revId == 0x60) || (tps65180->revId == 0x61) || (tps65180->revId == 0x45) || (tps65180->revId == 0x46) || (tps65180->revId == 0x55) || (tps65180->revId == 0x65)))
	{
		dev_err(tps65180->dev,
			"TPS6518x PMIC not found!\n");
		return -ENXIO;
	}
#else
/*2011/05/31 FY11 : Added retry for first i2c communication with TPS65181. */
	if ( tps65180->revId < 0 )
	{
		retry--;
		if ( retry < 0 )
		{
			dev_err( tps65180->dev, "read rev id failed. retryout\n" );
			return tps65180->revId;
		}
		//dev_err( tps65180->dev, "read rev id failed. retry=%d\n", TPS6518x_1st_I2C_RETRY - retry );
		msleep(50);
		goto i2c_retry;
	}
	else
	{
		udelay(1800);
		tps65180->revId = i2c_smbus_read_byte_data(client,REG_TPS65180_REVID);
		if ( tps65180->revId < 0 )
		{
			dev_err( tps65180->dev, "final read rev id failed.\n" );
			return tps65180->revId;
		}
	}
#endif


	if (tps65180->pdev[reg])
		return -EBUSY;

	pdev = platform_device_alloc("tps65180-reg", reg);
	if (!pdev)
		return -ENOMEM;

	tps65180->pdev[reg] = pdev;

	initdata->driver_data = tps65180;

	pdev->dev.platform_data = initdata;
	pdev->dev.parent = tps65180->dev;
	platform_set_drvdata(pdev, tps65180);

	ret = platform_device_add(pdev);

	if (ret != 0) {
		dev_err(tps65180->dev,
		       "Failed to register regulator %d: %d\n",
			reg, ret);
		platform_device_del(pdev);
		tps65180->pdev[reg] = NULL;
	}

	return ret;
}

static int tps65180_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int i;
	struct tps65180 *tps65180;
	struct tps65180_platform_data *pdata = client->dev.platform_data;
	int ret = 0;

	if (!pdata || !pdata->regulator_init)
		return -ENODEV;

	/* Create the PMIC data structure */
	tps65180 = kzalloc(sizeof(struct tps65180), GFP_KERNEL);
	if (tps65180 == NULL) {
		kfree(client);
		return -ENOMEM;
	}


	/* Initialize the PMIC data structure */
	i2c_set_clientdata(client, tps65180);
	tps65180->dev = &client->dev;
	tps65180->i2c_client = client;

	tps65180->gpio_pmic_pwrgood = pdata->gpio_pmic_pwrgood;
	tps65180->gpio_pmic_vcom_ctrl = pdata->gpio_pmic_vcom_ctrl;
	tps65180->gpio_pmic_wakeup = pdata->gpio_pmic_wakeup;
	tps65180->gpio_pmic_intr = pdata->gpio_pmic_intr;
	tps65180->gpio_pmic_pwr2_ctrl = pdata->gpio_pmic_pwr2_ctrl;
	tps65180->gpio_pmic_pwr0_ctrl = pdata->gpio_pmic_pwr0_ctrl;
/* 2011/05/26 FY11 : Supported TPS65181. */
	tps65180->gpio_pmic_v3p3_ctrl = pdata->gpio_pmic_v3p3_ctrl;
/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
	tps65180->gpio_pmic_vsys = pdata->gpio_pmic_vsys;

	tps65180->pass_num = pdata->pass_num;
	tps65180->vcom_uV = pdata->vcom_uV;

	tps65180->pmic_setup = false;

	ret = platform_driver_register(&tps65180_regulator_driver);
	if (ret < 0)
		goto err;

/* 2011/1/24 FY11 : Added wakeup port control. */
	tps65180_wakeup(tps65180);
	for (i = 0; i < TPS65180_NUM_REGULATORS; i++) {
		ret = tps65180_register_regulator(tps65180, i, &pdata->regulator_init[i]);
		if (ret != 0) {
			dev_err(tps65180->dev, "Platform init() failed: %d\n",
			ret);
		goto err;
		}
	}
/* 2011/1/24 FY11 : Added wakeup port control. */
	tps65180_sleep(tps65180);

	tps65180->max_wait = pdata->vpos_pwrup + pdata->vneg_pwrup +
		pdata->gvdd_pwrup + pdata->gvee_pwrup;

	/* Initialize the PMIC device */
	dev_info(&client->dev, "PMIC TPS6518x for eInk display\n");

	return ret;
err:
	kfree(tps65180);

	return ret;
}


static int tps65180_i2c_remove(struct i2c_client *i2c)
{
	struct tps65180 *tps65180 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < ARRAY_SIZE(tps65180->pdev); i++)
		platform_device_unregister(tps65180->pdev[i]);

	platform_driver_unregister(&tps65180_regulator_driver);

	kfree(tps65180);

	return 0;
}

static const struct i2c_device_id tps65180_i2c_id[] = {
       { "tps65180", 0 },
       { }
};
MODULE_DEVICE_TABLE(i2c, tps65180_i2c_id);


static struct i2c_driver tps65180_i2c_driver = {
	.driver = {
		   .name = "tps65180",
		   .owner = THIS_MODULE,
	},
	.probe = tps65180_i2c_probe,
	.remove = tps65180_i2c_remove,
	.id_table = tps65180_i2c_id,
};

static int __init tps65180_init(void)
{
	return i2c_add_driver(&tps65180_i2c_driver);
}
module_init(tps65180_init);

static void __exit tps65180_exit(void)
{
	i2c_del_driver(&tps65180_i2c_driver);
}
module_exit(tps65180_exit);

/* Module information */
MODULE_DESCRIPTION("TPS6518x regulator driver");
MODULE_LICENSE("GPL");

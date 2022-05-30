/*
 * linux/drivers/power/twl6030_bci_battery.c
 *
 * OMAP4:TWL6030 battery driver for Linux
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#include <linux/power_supply.h>
#include <linux/i2c/twl6030-gpadc.h>
#include <linux/i2c/bq2415x.h>
#include <mach/gpio.h>

#define CONTROLLER_INT_MASK	0x00
#define CONTROLLER_CTRL1	0x01
#define CONTROLLER_WDG		0x02
#define CONTROLLER_STAT1	0x03
#define CHARGERUSB_INT_STATUS	0x04
#define CHARGERUSB_INT_MASK	0x05
#define CHARGERUSB_STATUS_INT1	0x06
#define CHARGERUSB_STATUS_INT2	0x07
#define CHARGERUSB_CTRL1	0x08
#define CHARGERUSB_CTRL2	0x09
#define CHARGERUSB_CTRL3	0x0A
#define CHARGERUSB_STAT1	0x0B
#define CHARGERUSB_VOREG	0x0C
#define CHARGERUSB_VICHRG	0x0D
#define CHARGERUSB_CINLIMIT	0x0E
#define CHARGERUSB_CTRLLIMIT1	0x0F
#define CHARGERUSB_CTRLLIMIT2	0x10
#define ANTICOLLAPSE_CTRL1	0x11
#define ANTICOLLAPSE_CTRL2	0x12

/* TWL6025 registers 0xDA to 0xDE - TWL6025_MODULE_CHARGER */
#define CONTROLLER_CTRL2		0x00
#define CONTROLLER_VSEL_COMP	0x01
#define CHARGERUSB_VSYSREG		0x02
#define CHARGERUSB_VICHRG_PC	0x03
#define LINEAR_CHRG_STS			0x04

#define LINEAR_CHRG_STS_CRYSTL_OSC_OK		0x40
#define LINEAR_CHRG_STS_END_OF_CHARGE		0x20
#define LINEAR_CHRG_STS_VBATOV		0x10
#define LINEAR_CHRG_STS_VSYSOV		0x08
#define LINEAR_CHRG_STS_DPPM_STS		0x04
#define LINEAR_CHRG_STS_CV_STS		0x02
#define LINEAR_CHRG_STS_CC_STS		0x01

#define FG_REG_00	0x00
#define FG_REG_01	0x01
#define FG_REG_02	0x02
#define FG_REG_03	0x03
#define FG_REG_04	0x04
#define FG_REG_05	0x05
#define FG_REG_06	0x06
#define FG_REG_07	0x07
#define FG_REG_08	0x08
#define FG_REG_09	0x09
#define FG_REG_10	0x0A
#define FG_REG_11	0x0B

/* CONTROLLER_INT_MASK */
#define MVAC_FAULT		(1 << 7)
#define MAC_EOC			(1 << 6)
#define LINCH_GATED		(1 << 5)
#define MBAT_REMOVED	(1 << 4)
#define MFAULT_WDG		(1 << 3)
#define MBAT_TEMP		(1 << 2)
#define MVBUS_DET		(1 << 1)
#define MVAC_DET		(1 << 0)

/* CONTROLLER_CTRL1 */
#define CONTROLLER_CTRL1_EN_LINCH	(1 << 5)
#define CONTROLLER_CTRL1_EN_CHARGER	(1 << 4)
#define CONTROLLER_CTRL1_SEL_CHARGER	(1 << 3)

/* CONTROLLER_STAT1 */
#define CONTROLLER_STAT1_EXTCHRG_STATZ	(1 << 7)
#define CONTROLLER_STAT1_LINCH_GATED	(1 << 6)
#define CONTROLLER_STAT1_CHRG_DET_N	(1 << 5)
#define CONTROLLER_STAT1_FAULT_WDG	(1 << 4)
#define CONTROLLER_STAT1_VAC_DET	(1 << 3)
#define VAC_DET	(1 << 3)
#define CONTROLLER_STAT1_VBUS_DET	(1 << 2)
#define VBUS_DET	(1 << 2)
#define CONTROLLER_STAT1_BAT_REMOVED	(1 << 1)
#define CONTROLLER_STAT1_BAT_TEMP_OVRANGE (1 << 0)

/* CHARGERUSB_INT_STATUS */
#define CURRENT_TERM_INT	(1 << 3)
#define CHARGERUSB_STAT		(1 << 2)
#define CHARGERUSB_THMREG	(1 << 1)
#define CHARGERUSB_FAULT	(1 << 0)

/* CHARGERUSB_INT_MASK */
#define MASK_MCURRENT_TERM		(1 << 3)
#define MASK_MCHARGERUSB_STAT		(1 << 2)
#define MASK_MCHARGERUSB_THMREG		(1 << 1)
#define MASK_MCHARGERUSB_FAULT		(1 << 0)

/* CHARGERUSB_STATUS_INT1 */
#define CHARGERUSB_STATUS_INT1_TMREG	(1 << 7)
#define CHARGERUSB_STATUS_INT1_NO_BAT	(1 << 6)
#define CHARGERUSB_STATUS_INT1_BST_OCP	(1 << 5)
#define CHARGERUSB_STATUS_INT1_TH_SHUTD	(1 << 4)
#define CHARGERUSB_STATUS_INT1_BAT_OVP	(1 << 3)
#define CHARGERUSB_STATUS_INT1_POOR_SRC	(1 << 2)
#define CHARGERUSB_STATUS_INT1_SLP_MODE	(1 << 1)
#define CHARGERUSB_STATUS_INT1_VBUS_OVP	(1 << 0)

/* CHARGERUSB_STATUS_INT2 */
#define ICCLOOP		(1 << 3)
#define CURRENT_TERM	(1 << 2)
#define CHARGE_DONE	(1 << 1)
#define ANTICOLLAPSE	(1 << 0)

/* CHARGERUSB_CTRL1 */
#define SUSPEND_BOOT	(1 << 7)
#define OPA_MODE	(1 << 6)
#define HZ_MODE		(1 << 5)
#define TERM		(1 << 4)

/* CHARGERUSB_CTRL2 */
#define CHARGERUSB_CTRL2_VITERM_50	(0 << 5)
#define CHARGERUSB_CTRL2_VITERM_100	(1 << 5)
#define CHARGERUSB_CTRL2_VITERM_150	(2 << 5)
#define CHARGERUSB_CTRL2_VITERM_400	(7 << 5)

/* CHARGERUSB_CTRL3 */
#define VBUSCHRG_LDO_OVRD	(1 << 7)
#define CHARGE_ONCE		(1 << 6)
#define BST_HW_PR_DIS		(1 << 5)
#define AUTOSUPPLY		(1 << 3)
#define BUCK_HSILIM		(1 << 0)

/* CHARGERUSB_VOREG */
#define CHARGERUSB_VOREG_3P52		0x01
#define CHARGERUSB_VOREG_4P0		0x19
#define CHARGERUSB_VOREG_4P2		0x23
#define CHARGERUSB_VOREG_4P76		0x3F

/* CHARGERUSB_VICHRG */
#define CHARGERUSB_VICHRG_300		0x0
#define CHARGERUSB_VICHRG_500		0x4
#define CHARGERUSB_VICHRG_1500		0xE

/* CHARGERUSB_CINLIMIT */
#define CHARGERUSB_CIN_LIMIT_100	0x1
#define CHARGERUSB_CIN_LIMIT_300	0x5
#define CHARGERUSB_CIN_LIMIT_500	0x9
#define CHARGERUSB_CIN_LIMIT_NONE	0xF

/* CHARGERUSB_CTRLLIMIT1 */
#define VOREGL_4P16			0x21
#define VOREGL_4P56			0x35

/* CHARGERUSB_CTRLLIMIT2 */
#define CHARGERUSB_CTRLLIMIT2_1500	0x0E
#define		LOCK_LIMIT		(1 << 4)

/* ANTICOLLAPSE_CTRL2 */
#define BUCK_VTH_SHIFT			5

/* FG_REG_00 */
#define CC_ACTIVE_MODE_SHIFT	6
#define CC_AUTOCLEAR		(1 << 2)
#define CC_CAL_EN		(1 << 1)
#define CC_PAUSE		(1 << 0)

#define REG_TOGGLE1		0x90
#define FGDITHS			(1 << 7)
#define FGDITHR			(1 << 6)
#define FGS			(1 << 5)
#define FGR			(1 << 4)

#define PWDNSTATUS2		0x94

/* TWL6030_GPADC_CTRL */
#define GPADC_CTRL_TEMP1_EN	(1 << 0)    /* input ch 1 */
#define GPADC_CTRL_TEMP2_EN	(1 << 1)    /* input ch 4 */
#define GPADC_CTRL_SCALER_EN	(1 << 2)    /* input ch 2 */
#define GPADC_CTRL_SCALER_DIV4	(1 << 3)
#define GPADC_CTRL_SCALER_EN_CH11	(1 << 4)    /* input ch 11 */
#define GPADC_CTRL_TEMP1_EN_MONITOR	(1 << 5)
#define GPADC_CTRL_TEMP2_EN_MONITOR	(1 << 6)
#define GPADC_CTRL_ISOURCE_EN		(1 << 7)
#define ENABLE_ISOURCE		0x80

#define REG_MISC1		0xE4
#define VAC_MEAS		0x04
#define VBAT_MEAS		0x02
#define BB_MEAS			0x01

#define REG_USB_VBUS_CTRL_SET	0x04
#define VBUS_MEAS		0x01
#define REG_USB_ID_CTRL_SET	0x06
#define ID_MEAS			0x01

#define	BBSPOR_CFG			0xE6
#define	BB_CHG_EN		(1 << 3)

#define VBAT_CHANNEL		7
#define VBKP_CHANNEL		8

#define BOARD_ID	(5*32+14)	/* GPIO_6_14 */


/* Ptr to thermistor table */
static const unsigned int fuelgauge_rate[4] = {4, 16, 64, 256};
static const unsigned int fuelgauge_time[4] = {250000, 62500, 15600, 3900};

struct twl6030_bci_device_info {
	struct device		*dev;
	int			*battery_tmp_tbl;
	unsigned int		battery_tmp_tblsize;
	int			*battery_volt_tbl;
	unsigned int		battery_volt_tblsize;

	int			voltage_uV;
	int			bk_voltage_uV;
	int			current_uA;
#if 0 /* FY11 EBOOK */
	int			current_avg_uA;
#endif /* FY11 EBOOK */
	int			temp_C;
	int			charge_status;
	int			vac_priority;
	int			bat_health;
	int			charger_source;

	int			fuelgauge_mode;
	int			timer_n2;
	int			timer_n1;
	s32			charge_n1;
	s32			charge_n2;
	s16			cc_offset;
	u8			usb_online;
	u8			ac_online;
	u8			stat1;
	u8			linear_stat;
	u8			status_int1;
	u8			status_int2;

	u8			watchdog_duration;
	u16			current_avg_interval;
	u16			monitoring_interval;
	unsigned int		min_vbus;
	unsigned int		max_charger_voltagemV;
	unsigned int		max_charger_currentmA;
	unsigned int		charger_incurrentmA;
	unsigned int		charger_outcurrentmA;
	unsigned int		regulation_voltagemV;
	unsigned int		low_bat_voltagemV;
	unsigned int		termination_currentmA;
	unsigned int		use_hw_charger;
	unsigned int		use_eeprom_config;
	unsigned int		power_path;

	/* values for the voltage averaging calculation */
	unsigned int		v_no_samples;
	unsigned int		timer_usecs;
	unsigned int		voltage_uV_avg;
	unsigned int		voltage_uV_max;
	unsigned int		voltage_uV_min;
	
	/* values for capacity detection */
	unsigned int		vprime;
	unsigned int		capacity;
	unsigned int		old_capacity;
	unsigned int (*vprime_callback)(int voltage_uV, int current_uA);
	unsigned int (*capacity_callback)(int voltage_uV);
	int			bat_registered;
	int			disable_current_avg;
	int			fuelgauge_recal;
	int			fuelgauge_recal_cur;

	/* channel used for vbat, due to features this may not be 7 */
	int			vbat_channel;
	
	struct power_supply	bat;
	struct power_supply	usb;
	struct power_supply	ac;
	struct power_supply	bk_bat;
#if 1	/* FY11: EBOOK */
	struct delayed_work	twl6030_check_battery_state_work;
#else
	struct delayed_work	twl6030_bci_temperature_work;
	struct delayed_work	twl6030_bci_monitor_work;
	struct delayed_work	twl6030_current_avg_work;
#endif	/* FY11: EBOOK */
};
static struct blocking_notifier_head notifier_list;
struct twl6030_bci_device_info *the_di = NULL;

static int twl6030battery_current_setup(void);
static int twl6030battery_current_shutdown(void);
static int twl6030battery_current_calibrate(void);

static struct wake_lock update_battery_capacity;

static void twl6030_config_vsys_reg(struct twl6030_bci_device_info *di,
	int mV)
{
	u8 reg;

	if (mV > 4760 || mV < 3500) {
		dev_dbg(di->dev, "invalid min vsysbus\n");
		return;
	}

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &reg, CHARGERUSB_VSYSREG);
	reg &= ~0x3F;

	/* voltage is defined in 0.02 steps*/
	reg |= (mV - 3500) / 20;
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, reg, CHARGERUSB_VSYSREG);
}

static void twl6030_config_min_vbus_reg(struct twl6030_bci_device_info *di,
						unsigned int value)
{
	u8 rd_reg = 0;
	if (value > 4760 || value < 4200) {
		dev_dbg(di->dev, "invalid min vbus\n");
		return;
	}
	
	/* not required on TWL6025 */
	if (twl_features() & TWL6025_SUBCLASS)
		return;

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &rd_reg, ANTICOLLAPSE_CTRL2);
	rd_reg = rd_reg & 0x1F;
	rd_reg = rd_reg | (((value - 4200)/80) << BUCK_VTH_SHIFT);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, rd_reg, ANTICOLLAPSE_CTRL2);

	return;
}

static void twl6030_config_iterm_reg(struct twl6030_bci_device_info *di,
						unsigned int term_currentmA)
{
	if ((term_currentmA > 400) || (term_currentmA < 50)) {
		dev_dbg(di->dev, "invalid termination current\n");
		return;
	}

	term_currentmA = ((term_currentmA - 50)/50) << 5;
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, term_currentmA,
						CHARGERUSB_CTRL2);
	return;
}

static unsigned int twl6030_get_iterm_reg(struct twl6030_bci_device_info *di)
{
	unsigned int currentmA;
	u8 val;

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &val, CHARGERUSB_CTRL2);
	currentmA = 50 + (val >> 5) * 50;
	
	return currentmA;
}

static void twl6030_config_voreg_reg(struct twl6030_bci_device_info *di,
							unsigned int voltagemV)
{
	if ((voltagemV < 3500) || (voltagemV > 4760)) {
		dev_dbg(di->dev, "invalid charger_voltagemV\n");
		return;
	}

	voltagemV = (voltagemV - 3500) / 20;
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, voltagemV,
						CHARGERUSB_VOREG);
	return;
}

static unsigned int twl6030_get_voreg_reg(struct twl6030_bci_device_info *di)
{
	unsigned int voltagemV;
	u8 val;

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &val, CHARGERUSB_VOREG);
	voltagemV = 3500 + (val * 20);
	
	return voltagemV;
}

static void twl6030_config_vichrg_reg(struct twl6030_bci_device_info *di,
							unsigned int currentmA)
{
	if ((currentmA >= 300) && (currentmA <= 450))
		currentmA = (currentmA - 300) / 50;
	else if ((currentmA >= 500) && (currentmA <= 1500))
		currentmA = (currentmA - 500) / 100 + 4;
	else {
		dev_dbg(di->dev, "invalid charger_currentmA\n");
		return;
	}

	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, currentmA,
						CHARGERUSB_VICHRG);
	return;
}

static const int vichrg[] = {
	300, 350, 400, 450, 500, 600, 700, 800,
	900, 1000, 1100, 1200, 1300, 1400, 1500, 300
};

static unsigned int twl6030_get_vichrg_reg(struct twl6030_bci_device_info *di)
{
	unsigned int currentmA;
	u8 val;

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &val, CHARGERUSB_VICHRG);
	currentmA = vichrg[val & 0xF];
	
	return currentmA;
}

static void twl6030_config_cinlimit_reg(struct twl6030_bci_device_info *di,
							unsigned int currentmA)
{
	if ((currentmA >= 50) && (currentmA <= 750))
		currentmA = (currentmA - 50) / 50;
	else if (currentmA >= 750) {
		if (twl_features() & TWL6025_SUBCLASS)
			currentmA = (currentmA % 100) ? 0x30 : 0x20 + (currentmA - 100) / 100;
		else
			currentmA = (800 - 50) / 50;
	} else {
		dev_dbg(di->dev, "invalid input current limit\n");
		return;
	}

	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, currentmA,
					CHARGERUSB_CINLIMIT);
	return;
}

static void twl6030_config_limit1_reg(struct twl6030_bci_device_info *di,
							unsigned int voltagemV)
{
	if ((voltagemV < 3500) || (voltagemV > 4760)) {
		dev_dbg(di->dev, "invalid max_charger_voltagemV\n");
		return;
	}

	voltagemV = (voltagemV - 3500) / 20;
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, voltagemV,
						CHARGERUSB_CTRLLIMIT1);
	return;
}

static unsigned int twl6030_get_limit1_reg(struct twl6030_bci_device_info *di)
{
	unsigned int voltagemV;
	u8 val;

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &val, CHARGERUSB_CTRLLIMIT1);
	voltagemV = 3500 + (val * 20);
	
	return voltagemV;
}

static void twl6030_config_limit2_reg(struct twl6030_bci_device_info *di,
							unsigned int currentmA)
{
	if ((currentmA >= 300) && (currentmA <= 450))
		currentmA = (currentmA - 300) / 50;
	else if ((currentmA >= 500) && (currentmA <= 1500))
		currentmA = (currentmA - 500) / 100 + 4;
	else {
		dev_dbg(di->dev, "invalid max_charger_currentmA\n");
		return;
	}

	// FIXME: currentmA |= LOCK_LIMIT;
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, currentmA,
						CHARGERUSB_CTRLLIMIT2);
	return;
}

static unsigned int twl6030_get_limit2_reg(struct twl6030_bci_device_info *di)
{
	unsigned int currentmA;
	u8 val;

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &val, CHARGERUSB_CTRLLIMIT2);
	currentmA = vichrg[val & 0xF];
	
	return currentmA;
}

int twl6030_set_usb_charge_enable(int enable)
{
	if (!the_di)
		return -ENODEV;

	// TODO - enable or disable the USB hw charger
	return 0;
}
EXPORT_SYMBOL(twl6030_set_usb_charge_enable);

int twl6030_set_usb_in_current(int currentmA)
{
	u8 reg;

	if (!the_di)
		return -ENODEV;

	switch (currentmA) {
	case 0:
		twl6030_config_cinlimit_reg(the_di, currentmA);
		twl6030_config_limit2_reg(the_di, 900);
		twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &reg,
			CHARGERUSB_CTRL1);
		reg |= HZ_MODE;
		twl_i2c_write_u8(TWL6030_MODULE_CHARGER, reg, CHARGERUSB_CTRL1);
		the_di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 100:
		twl6030_config_cinlimit_reg(the_di, currentmA);
		twl6030_config_limit2_reg(the_di, 900);
		twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &reg,
			CHARGERUSB_CTRL1);
		reg &= ~HZ_MODE;
		twl_i2c_write_u8(TWL6030_MODULE_CHARGER, reg, CHARGERUSB_CTRL1);
		if(the_di->charge_status == POWER_SUPPLY_STATUS_DISCHARGING){
			the_di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
		break;
	case 500:
		twl6030_config_cinlimit_reg(the_di, currentmA);
		twl6030_config_limit2_reg(the_di, 900);
		twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &reg,
			CHARGERUSB_CTRL1);
		reg &= ~HZ_MODE;
		twl_i2c_write_u8(TWL6030_MODULE_CHARGER, reg, CHARGERUSB_CTRL1);
		if(the_di->charge_status == POWER_SUPPLY_STATUS_DISCHARGING){
			the_di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
		break;
	case 1000:
		twl6030_config_cinlimit_reg(the_di, currentmA);
		twl6030_config_limit2_reg(the_di, 1500);
		twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &reg,
			CHARGERUSB_CTRL1);
		reg &= ~HZ_MODE;
		twl_i2c_write_u8(TWL6030_MODULE_CHARGER, reg, CHARGERUSB_CTRL1);
		if(the_di->charge_status == POWER_SUPPLY_STATUS_DISCHARGING){
			the_di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(twl6030_set_usb_in_current);

int twl6030_set_power_source(int source)
{
	if (!the_di)
		return -ENODEV;
	
	switch (source) {
	case POWER_SUPPLY_TYPE_MAINS:
		the_di->charger_source = source;
		the_di->ac_online = 1;
		the_di->usb_online = 0;
		break;
	case POWER_SUPPLY_TYPE_USB:
		the_di->charger_source = source;
		the_di->usb_online = 1;
		the_di->ac_online = 0;
		break;
	case 0:
		the_di->charger_source = 0;
		the_di->ac_online = 0;
		the_di->usb_online = 0;
		break;
	default:
		return -EINVAL;
	}
	
	power_supply_changed(&the_di->bat);

	return 0;
}
EXPORT_SYMBOL(twl6030_set_power_source);

static void twl6030_stop_usb_charger(struct twl6030_bci_device_info *di)
{
	di->charger_source = 0;
	di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	
	if ((twl_features() & TWL6025_SUBCLASS) && di->use_hw_charger)
		return;
	
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, 0, CONTROLLER_CTRL1);
}

static void twl6030_start_usb_charger(struct twl6030_bci_device_info *di)
{
	di->charger_source = POWER_SUPPLY_TYPE_USB;
	di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	dev_dbg(di->dev, "USB charger detected\n");
	
	if ((twl_features() & TWL6025_SUBCLASS) && di->use_hw_charger)
		return;

	if (di->use_eeprom_config)
		goto enable;
	
	twl6030_config_vichrg_reg(di, di->charger_outcurrentmA);
	twl6030_config_voreg_reg(di, di->regulation_voltagemV);
	twl6030_config_iterm_reg(di, di->termination_currentmA);
	twl6030_config_cinlimit_reg(di, di->charger_incurrentmA);
	
enable:
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER,
			CONTROLLER_CTRL1_EN_CHARGER,
			CONTROLLER_CTRL1);
}

static void twl6030_stop_ac_charger(struct twl6030_bci_device_info *di)
{
	long int events;
	di->charger_source = 0;
	di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	events = BQ2415x_STOP_CHARGING;
	blocking_notifier_call_chain(&notifier_list, events, NULL);
	
	if ((twl_features() & TWL6025_SUBCLASS) && di->use_hw_charger)
		return;
	
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, 0, CONTROLLER_CTRL1);
}

static void twl6030_start_ac_charger(struct twl6030_bci_device_info *di)
{
	long int events;

	dev_dbg(di->dev, "AC charger detected\n");
	di->charger_source = POWER_SUPPLY_TYPE_MAINS;
	di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	events = BQ2415x_START_CHARGING;
	blocking_notifier_call_chain(&notifier_list, events, NULL);
	if ((twl_features() & TWL6025_SUBCLASS) && di->use_hw_charger)
		return;

	twl_i2c_write_u8(TWL6030_MODULE_CHARGER,
			CONTROLLER_CTRL1_EN_CHARGER |CONTROLLER_CTRL1_SEL_CHARGER,
			CONTROLLER_CTRL1);
}

static void twl6030_stop_charger(struct twl6030_bci_device_info *di)
{
	if (di->charger_source == POWER_SUPPLY_TYPE_MAINS)
		twl6030_stop_ac_charger(di);
	else if (di->charger_source == POWER_SUPPLY_TYPE_USB)
		twl6030_stop_usb_charger(di);
}

static void twl6030_check_battery_state_work(struct work_struct *work)
{
	//u8 linear_state_diff, ctrl_stat1_diff, usb_charge_sts1_diff;
	u8 present_linear_chg_state, present_ctrl_stat1_state, present_usb_charge_sts1;
	int err;
	
	if (!the_di || !the_di->bat_registered)
		return;
	
	/* Read LINEAR_CHRG_STS Register */
	err = twl_i2c_read_u8(TWL6025_MODULE_CHARGER, &present_linear_chg_state, LINEAR_CHRG_STS);
	
	if (err < 0)
		return;
	
	/* Read CONTROLLER_STAT1 Register  */
	err = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &present_ctrl_stat1_state, CONTROLLER_STAT1);
	
	if (err < 0)
		return;
	
	/* Read CHARGERUSB_STATUS_INT1 Register  */
	err = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &present_usb_charge_sts1, CHARGERUSB_STATUS_INT1);
	
	if (err < 0)
		return;

	//linear_state_diff = di->linear_stat ^ present_linear_chg_state;
	the_di->linear_stat = present_linear_chg_state;
	
	//ctrl_stat1_diff = di->stat1 ^ present_ctrl_stat1_state
	the_di->stat1 = present_ctrl_stat1_state;
	
	//usb_charge_sts1_diff = di->status_int1 ^ present_usb_charge_sts1;
	the_di->status_int1 = present_usb_charge_sts1;

	//Check charge_status
	if( present_linear_chg_state & (LINEAR_CHRG_STS_CC_STS | LINEAR_CHRG_STS_CV_STS))
	{
		dev_dbg(the_di->dev, "Linear status: START OF CHARGE\n");
		the_di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	}
	else if( !(present_ctrl_stat1_state & (VBUS_DET | VAC_DET)))
	{
		the_di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	else if( present_linear_chg_state & LINEAR_CHRG_STS_END_OF_CHARGE)
	{
		dev_dbg(the_di->dev, "Linear status: END OF CHARGE\n");
		printk("Linear status: END OF CHARGE\n");
		the_di->charge_status = POWER_SUPPLY_STATUS_FULL;
	}
	else
	{
		u8 reg;
		twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &reg, CHARGERUSB_CTRL1);
		if((reg & HZ_MODE) && (the_di->charger_source == POWER_SUPPLY_TYPE_USB)){
			the_di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
		}else{
			the_di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
	}

	//Check bat_health
	if ( !(present_linear_chg_state & LINEAR_CHRG_STS_CRYSTL_OSC_OK) )
	{
		//Cause??
		dev_dbg(the_di->dev, "Linear status: CRYSTAL OSC NG\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	else if (present_linear_chg_state & LINEAR_CHRG_STS_VBATOV)
	{
		//VBAT Over Voltage
		dev_dbg(the_di->dev, "Linear Status: VBATOV\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	else if (present_linear_chg_state & LINEAR_CHRG_STS_VSYSOV )
	{
		//VSYS Over Voltage
		dev_dbg(the_di->dev, "Linear Status: VSYSOV\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	else if( present_ctrl_stat1_state & CONTROLLER_STAT1_BAT_TEMP_OVRANGE )
	{
		//Temperature Over Range
		dev_dbg(the_di->dev, "CONTROLLER_STAT1: Over temperatureV\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	}
	else if( present_ctrl_stat1_state & CONTROLLER_STAT1_BAT_REMOVED )
	{
		//Battery Removed
		dev_dbg(the_di->dev, "CONTROLLER_STAT1: Battery Removed\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	else if( present_ctrl_stat1_state & CONTROLLER_STAT1_FAULT_WDG )
	{
		//Watchdog
		dev_dbg(the_di->dev, "CONTROLLER_STAT1: FAULT WDG\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	else if( present_usb_charge_sts1 & CHARGERUSB_STATUS_INT1_TH_SHUTD )
	{
		//Charger thermal shutdown
		dev_dbg(the_di->dev, "CHARGERUSB_STATUS_INT1: Charger thermal shutdown\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	else if( present_usb_charge_sts1 & CHARGERUSB_STATUS_INT1_BAT_OVP )
	{
		//VBAT Over Voltage
		dev_dbg(the_di->dev, "CHARGERUSB_STATUS_INT1: VBATOV\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	else if( present_usb_charge_sts1 & CHARGERUSB_STATUS_INT1_POOR_SRC ) 
	{
		//VBAT Over Voltage
		dev_dbg(the_di->dev, "CHARGERUSB_STATUS_INT1: Poor input source\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	else if( present_usb_charge_sts1 & CHARGERUSB_STATUS_INT1_SLP_MODE ) 
	{
		//VBAT >= VBUS
		dev_dbg(the_di->dev, "CHARGERUSB_STATUS_INT1: VBAT >= VBUS\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	else if( present_usb_charge_sts1 & CHARGERUSB_STATUS_INT1_VBUS_OVP ) 
	{
		//VBUS Over voltage
		dev_dbg(the_di->dev, "CHARGERUSB_STATUS_INT1: VBUS Over voltag\n");
		the_di->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	else
	{
		the_di->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	
	power_supply_changed(&the_di->bat);
}

static void twl6025_charger_ctrl_interrupt(struct twl6030_bci_device_info *di)
{
	u8 stat_toggle, stat_reset, stat_set = 0;
	u8 present_state, linear_state;
	int err;
	
	// FIXME: this is a kludge as status bit lag the VUSB detection by
	// some milliseconds
	msleep(1000);

	err = twl_i2c_read_u8(TWL6025_MODULE_CHARGER, &present_state,
			LINEAR_CHRG_STS);
	
	if (err < 0)
		return;

	linear_state = di->linear_stat;

	stat_toggle = linear_state ^ present_state;
	stat_set = stat_toggle & present_state;
	stat_reset = stat_toggle & linear_state;
	di->linear_stat = present_state;

	if (stat_set & LINEAR_CHRG_STS_CRYSTL_OSC_OK)
		dev_dbg(di->dev, "Linear status: CRYSTAL OSC OK\n");
	
	if (stat_set & LINEAR_CHRG_STS_END_OF_CHARGE) {
		dev_dbg(di->dev, "Linear status: END OF CHARGE\n");
		printk("Linear status: END OF CHARGE\n");
		di->charge_status = POWER_SUPPLY_STATUS_FULL;
	}
	
	if(present_state &
		(LINEAR_CHRG_STS_CC_STS | LINEAR_CHRG_STS_CV_STS)) {
		dev_dbg(di->dev, "Linear status: START OF CHARGE\n");
		di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	}
	
	if (stat_set & LINEAR_CHRG_STS_VBATOV) {
		dev_dbg(di->dev, "Linear Status: VBATOV\n");
		di->bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	
	if (stat_reset & LINEAR_CHRG_STS_VBATOV) {
		dev_dbg(di->dev, "Linear Status: VBATOV\n");
		di->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	
	if (stat_set & LINEAR_CHRG_STS_VSYSOV)
		dev_dbg(di->dev, "Linear Status: VSYSOV\n");
	
	if (stat_set & LINEAR_CHRG_STS_DPPM_STS)
		dev_dbg(di->dev, "Linear Status: DPPM STS\n");
	
	if (stat_set & LINEAR_CHRG_STS_CV_STS)
		dev_dbg(di->dev, "Linear Status: CV STS\n");
	
	if (stat_set & LINEAR_CHRG_STS_CC_STS)
		dev_dbg(di->dev, "Linear Status: CC STS\n");

	power_supply_changed(&di->bat);
}


/*
 * Interrupt service routine
 *
 * Attends to TWL 6030 power module interruptions events, specifically
 * USB_PRES (USB charger presence) CHG_PRES (AC charger presence) events
 *
 */
static irqreturn_t twl6030charger_ctrl_interrupt(int irq, void *_di)
{
#if 1 /* E_BOOK */
	struct twl6030_bci_device_info *di = _di;
	cancel_delayed_work(&di->twl6030_check_battery_state_work);
	schedule_delayed_work(&di->twl6030_check_battery_state_work, msecs_to_jiffies(500));
	return IRQ_HANDLED;
	
#else
	
	struct twl6030_bci_device_info *di = _di;
	int ret;
	int charger_fault = 0;
	long int events;
	u8 stat_toggle, stat_reset, stat_set = 0;
	u8 charge_state;
	u8 present_charge_state;
	u8 ac_or_vbus, no_ac_and_vbus;

	/* read charger controller_stat1 */
	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &present_charge_state,
		CONTROLLER_STAT1);
	if (ret)
		return IRQ_NONE;
	charge_state = di->stat1;

	stat_toggle = charge_state ^ present_charge_state;
	stat_set = stat_toggle & present_charge_state;
	stat_reset = stat_toggle & charge_state;

	no_ac_and_vbus = !((present_charge_state) & (VBUS_DET | VAC_DET));
	ac_or_vbus = charge_state & (VBUS_DET | VAC_DET);
	if (no_ac_and_vbus && ac_or_vbus) {
		di->charger_source = 0;
		dev_dbg(di->dev, "No Charging source\n");
		/* disable charging when no source present */
	}

	charge_state = present_charge_state;
	di->stat1 = present_charge_state;
	if ((charge_state & VAC_DET) &&
		(charge_state & CONTROLLER_STAT1_EXTCHRG_STATZ)) {
		events = BQ2415x_CHARGER_FAULT;
		blocking_notifier_call_chain(&notifier_list, events, NULL);
	}

	if (!((twl_features() & TWL6025_SUBCLASS) && di->use_hw_charger)) {
		if (stat_reset & VBUS_DET) {
			di->usb_online = 0;
			dev_dbg(di->dev, "usb removed\n");
			twl6030_stop_usb_charger(di);
			if (present_charge_state & VAC_DET)
				twl6030_start_ac_charger(di);

		}
		if (stat_set & VBUS_DET) {
			di->usb_online = POWER_SUPPLY_TYPE_USB;
			if ((present_charge_state & VAC_DET) &&
				(di->vac_priority == 2))
				dev_dbg(di->dev,
				"USB charger detected, continue with VAC\n");
			else
				twl6030_start_usb_charger(di);
		}

		if (stat_reset & VAC_DET) {
			di->ac_online = 0;
			dev_dbg(di->dev, "vac removed\n");
			twl6030_stop_ac_charger(di);
			if (present_charge_state & VBUS_DET)
				twl6030_start_usb_charger(di);
		}
		if (stat_set & VAC_DET) {
			di->ac_online = POWER_SUPPLY_TYPE_MAINS;
			if ((present_charge_state & VBUS_DET) &&
						(di->vac_priority == 3))
				dev_dbg(di->dev,
				"AC charger detected, continue with VBUS\n");
			else
				twl6030_start_ac_charger(di);
		}
	} else {
		if (!(charge_state & (VBUS_DET | VAC_DET)))
			di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			//di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	}

	if (stat_set & CONTROLLER_STAT1_FAULT_WDG) {
		charger_fault = 1;
		dev_dbg(di->dev, "Fault watchdog fired\n");
	}
	if (stat_reset & CONTROLLER_STAT1_FAULT_WDG)
		dev_dbg(di->dev, "Fault watchdog recovered\n");
	if (stat_set & CONTROLLER_STAT1_BAT_REMOVED)
		dev_dbg(di->dev, "Battery removed\n");
	if (stat_reset & CONTROLLER_STAT1_BAT_REMOVED)
		dev_dbg(di->dev, "Battery inserted\n");
	if (stat_set & CONTROLLER_STAT1_BAT_TEMP_OVRANGE) {
		dev_dbg(di->dev, "Battery temperature overrange\n");
		di->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	}
	if (stat_reset & CONTROLLER_STAT1_BAT_TEMP_OVRANGE) {
		dev_dbg(di->dev, "Battery temperature within range\n");
		di->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	// FIXME: this bit does not seem to be reliable on all chip versions
	//if (charge_state & CONTROLLER_STAT1_LINCH_GATED) {
	//	dev_dbg(di->dev, "Linear Charge Gated\n");
		twl6025_charger_ctrl_interrupt(di);
	//}

	if (charger_fault) {
		twl6030_stop_usb_charger(di);
		di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		dev_err(di->dev, "Charger Fault stop charging\n");
	}
	
	//printk("[twl6030charger_ctrl_interrupt] charge_status = %d\n", di->charge_status);
	power_supply_changed(&di->bat);

	return IRQ_HANDLED;
#endif /* E_BOOK */
}

static irqreturn_t twl6030charger_fault_interrupt(int irq, void *_di)
{
	struct twl6030_bci_device_info *di = _di;
	int charger_fault = 0;
	int ret;

	u8 usb_charge_sts, usb_charge_sts1, usb_charge_sts2;

	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &usb_charge_sts,
						CHARGERUSB_INT_STATUS);
	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &usb_charge_sts1,
						CHARGERUSB_STATUS_INT1);
	ret = twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &usb_charge_sts2,
						CHARGERUSB_STATUS_INT2);
	
	//printk("twl6030charger_fault_interrupt\n");
	//printk("usb_charge_sts1 = %02X\n",usb_charge_sts1);
	//printk("usb_charge_sts2 = %02X\n",usb_charge_sts2);
	
	di->status_int1 = usb_charge_sts1;
	di->status_int2 = usb_charge_sts2;
	if (usb_charge_sts & CURRENT_TERM_INT)
		dev_dbg(di->dev, "USB CURRENT_TERM_INT\n");
	if (usb_charge_sts & CHARGERUSB_THMREG)
		dev_dbg(di->dev, "USB CHARGERUSB_THMREG\n");
	if (usb_charge_sts & CHARGERUSB_FAULT)
		dev_dbg(di->dev, "USB CHARGERUSB_FAULT\n");

	if (usb_charge_sts1 & CHARGERUSB_STATUS_INT1_TMREG)
		dev_dbg(di->dev, "USB CHARGER Thermal regulation activated\n");
	if (usb_charge_sts1 & CHARGERUSB_STATUS_INT1_NO_BAT)
		dev_dbg(di->dev, "No Battery Present\n");
	if (usb_charge_sts1 & CHARGERUSB_STATUS_INT1_BST_OCP)
		dev_dbg(di->dev, "USB CHARGER Boost Over current protection\n");
	if (usb_charge_sts1 & CHARGERUSB_STATUS_INT1_TH_SHUTD) {
		charger_fault = 1;
		dev_dbg(di->dev, "USB CHARGER Thermal Shutdown\n");
	}
	if (usb_charge_sts1 & CHARGERUSB_STATUS_INT1_BAT_OVP)
		dev_dbg(di->dev, "USB CHARGER Bat Over Voltage Protection\n");
	if (usb_charge_sts1 & CHARGERUSB_STATUS_INT1_POOR_SRC)
		dev_dbg(di->dev, "USB CHARGER Poor input source\n");
	if (usb_charge_sts1 & CHARGERUSB_STATUS_INT1_SLP_MODE)
		dev_dbg(di->dev, "USB CHARGER Sleep mode\n");
	if (usb_charge_sts1 & CHARGERUSB_STATUS_INT1_VBUS_OVP)
		dev_dbg(di->dev, "USB CHARGER VBUS over voltage\n");

	if ((usb_charge_sts2 & CHARGE_DONE) && !di->power_path) {
		di->charge_status = POWER_SUPPLY_STATUS_FULL;
		dev_dbg(di->dev, "USB charge done\n");
	}
	if (usb_charge_sts2 & CURRENT_TERM)
		dev_dbg(di->dev, "USB CURRENT_TERM\n");
	if (usb_charge_sts2 & ICCLOOP)
		dev_dbg(di->dev, "USB ICCLOOP\n");
	if (usb_charge_sts2 & ANTICOLLAPSE)
		dev_dbg(di->dev, "USB ANTICOLLAPSE\n");	
	
	if (charger_fault) {
#if 1 /* E_BOOK */
		twl6030_set_usb_in_current(0);
		di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
#else 
		twl6030_stop_usb_charger(di);
		di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
#endif /* E_BOOK */
		dev_err(di->dev, "Charger Fault stop charging\n");
	}
	dev_dbg(di->dev, "Charger fault detected STS, INT1, INT2 %x %x %x\n",
	    usb_charge_sts, usb_charge_sts1, usb_charge_sts2);

	cancel_delayed_work(&di->twl6030_check_battery_state_work);
	schedule_delayed_work(&di->twl6030_check_battery_state_work, msecs_to_jiffies(500));

	return IRQ_HANDLED;
}

static void twl6030battery_current(struct twl6030_bci_device_info *di)
{
	int ret;
	u16 read_value;
	s16 temp;
	int current_now;

	/* FG_REG_10, 11 is 14 bit signed instantaneous current sample value */
	ret = twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *)&read_value,
								FG_REG_10, 2);
	if (ret < 0) {
		dev_dbg(di->dev, "failed to read FG_REG_10: current_now\n");
		return;
	}

	temp = ((s16)(read_value << 2) >> 2);
	current_now = temp - di->cc_offset;

	/* current drawn per sec */
	current_now = current_now * fuelgauge_rate[di->fuelgauge_mode];
	/* current in mAmperes */
	current_now = (current_now * 3000) >> 14;
	/* current in uAmperes */
	current_now = current_now * 1000;
	di->current_uA = current_now;

	return;
}

/*
 * Return channel value
 * Or < 0 on failure.
 */
static int twl6030_get_gpadc_conversion(int channel_no)
{
	struct twl6030_gpadc_request req;
	int temp = 0;
	int ret;

	req.channels = (1 << channel_no);
	if (twl_features() & TWL6025_SUBCLASS)
		req.method	= TWL6025_GPADC_SW2;
	else
		req.method	= TWL6030_GPADC_SW2;
	req.active = 0;
	req.func_cb = NULL;
	ret = twl6030_gpadc_conversion(&req);
	if (ret < 0)
		return ret;

	if (req.rbuf[channel_no] > 0)
		temp = req.rbuf[channel_no];

	return temp;
}

static void twl6030_battery_sleep(unsigned int usecs)
{
	struct hrtimer_sleeper *sleeper, _sleeper;
	enum hrtimer_mode mode = HRTIMER_MODE_REL;
	int state = TASK_INTERRUPTIBLE;

	sleeper = &_sleeper;
	/*
	 * This is really just a reworked and simplified version
	 * of do_nanosleep().
	 */
	hrtimer_init(&sleeper->timer, CLOCK_MONOTONIC, mode);
	hrtimer_set_expires(&sleeper->timer, ktime_set(0, usecs*NSEC_PER_USEC));
	hrtimer_init_sleeper(sleeper, current);

	do {
		set_current_state(state);
		hrtimer_start_expires(&sleeper->timer, mode);
		if (sleeper->task)
			schedule();
		hrtimer_cancel(&sleeper->timer);
		mode = HRTIMER_MODE_ABS;
	} while (sleeper->task && !signal_pending(current));
}

static void twl6030battery_voltage(struct twl6030_bci_device_info *di)
{
	unsigned int vnow, sum;
	int n;
	u8 rd_reg;

	dev_dbg(di->dev, "Entered %s\n", __func__);
	
	/* Enable the resister divider depending on channel */
	twl_i2c_read_u8(TWL6030_MODULE_ID0, &rd_reg, REG_MISC1);
	if (di->vbat_channel == VBAT_CHANNEL)
		rd_reg |= VBAT_MEAS;
	else if (di->vbat_channel == VBKP_CHANNEL)
		rd_reg |= BB_MEAS;
	twl_i2c_write_u8(TWL6030_MODULE_ID0, rd_reg, REG_MISC1);

	/* Settling time for divider 10ms (too small for msleep) */
	twl6030_battery_sleep(10000);

	/* setup the delay between samples */
	di->timer_usecs =  fuelgauge_time[di->fuelgauge_mode] /
						di->v_no_samples;
	
	/* Get the initial voltage to setup readings */
	vnow = twl6030_get_gpadc_conversion(di->vbat_channel) * 1000;
	
	sum = vnow;

	di->voltage_uV_max = di->voltage_uV_min = di->voltage_uV_avg = vnow;

	for (n = 1; n < di->v_no_samples; n++) {

		if( di->timer_usecs > 3000 ){
			twl6030_battery_sleep(di->timer_usecs - 3000); //I2C comunication time is 3ms
		}else{
			twl6030_battery_sleep(di->timer_usecs);

		}

		vnow = twl6030_get_gpadc_conversion(di->vbat_channel) * 1000;
		
		sum += vnow;

		if (vnow > di->voltage_uV_max)
			di->voltage_uV_max = vnow;
		else if (vnow < di->voltage_uV_min)
			di->voltage_uV_min = vnow;

	}
	
	/* Do the average of the voltage */
	di->voltage_uV_avg = sum / (di->v_no_samples);

	dev_dbg(di->dev, "voltage avg: %d\n", di->voltage_uV_avg);
	dev_dbg(di->dev, "voltage max: %d\n", di->voltage_uV_max);
	dev_dbg(di->dev, "voltage min: %d\n", di->voltage_uV_min);

	/* set the voltage to our calculated average */
	di->voltage_uV = di->voltage_uV_avg;
	
	/* Disable the resister divider depending on channel */
	twl_i2c_read_u8(TWL6030_MODULE_ID0, &rd_reg, REG_MISC1);
	if (di->vbat_channel == VBAT_CHANNEL)
		rd_reg &= ~VBAT_MEAS;
	else if (di->vbat_channel == VBKP_CHANNEL)
		rd_reg &= ~BB_MEAS;
	twl_i2c_write_u8(TWL6030_MODULE_ID0, rd_reg, REG_MISC1);
}

static void twl6030_bci_temperature(struct twl6030_bci_device_info *di)
{
	struct twl6030_gpadc_request req;
	int adc_code;
	int temp;
	int ret;
	
	req.channels = (1 << 1);
	if (twl_features() & TWL6025_SUBCLASS)
		req.method	= TWL6025_GPADC_SW2;
	else
		req.method	= TWL6030_GPADC_SW2;
	req.active = 0;
	req.func_cb = NULL;
	ret = twl6030_gpadc_conversion(&req);

	if (ret < 0) {
		dev_dbg(di->dev, "gpadc conversion failed: %d\n", ret);
		return;
	}

	if (di->battery_tmp_tbl == NULL)
		return;

	if (req.rbuf[1] > 100 && req.rbuf[1] < 950) {
		adc_code = req.rbuf[1];
		for (temp = 0; temp < di->battery_tmp_tblsize; temp++) {
			if (adc_code >= di->battery_tmp_tbl[temp])
				break;
		}

		/* first 2 values are for negative temperature */
		di->temp_C = (temp - 2) * 10; /* in tenths of degree Celsius */
	}

}

/*
 * Setup the twl6030 BCI module to enable backup
 * battery charging.
 */
static int twl6030backupbatt_setup(void)
{
	int ret;
	u8 rd_reg;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0, &rd_reg, BBSPOR_CFG);
	rd_reg |= BB_CHG_EN;
	ret |= twl_i2c_write_u8(TWL6030_MODULE_ID0, rd_reg, BBSPOR_CFG);

	return ret;
}

/*
 * Setup the twl6030 BCI module to measure battery
 * temperature
 */
static int twl6030battery_temp_setup(void)
{
	int ret;
	u8 rd_reg;

	ret = twl_i2c_read_u8(TWL_MODULE_MADC, &rd_reg, TWL6030_GPADC_CTRL);
	rd_reg |= GPADC_CTRL_TEMP1_EN | GPADC_CTRL_TEMP2_EN |
		GPADC_CTRL_TEMP1_EN_MONITOR | GPADC_CTRL_TEMP2_EN_MONITOR |
		GPADC_CTRL_SCALER_DIV4;
	ret |= twl_i2c_write_u8(TWL_MODULE_MADC, rd_reg, TWL6030_GPADC_CTRL);

	return ret;
}

static int twl6030battery_voltage_setup(void)
{
	int ret;
	u8 rd_reg;

	ret |= twl_i2c_read_u8(TWL_MODULE_USB, &rd_reg, REG_USB_VBUS_CTRL_SET);
	rd_reg = rd_reg | VBUS_MEAS;
	ret |= twl_i2c_write_u8(TWL_MODULE_USB, rd_reg, REG_USB_VBUS_CTRL_SET);

	ret |= twl_i2c_read_u8(TWL_MODULE_USB, &rd_reg, REG_USB_ID_CTRL_SET);
	rd_reg = rd_reg | ID_MEAS;
	ret |= twl_i2c_write_u8(TWL_MODULE_USB, rd_reg, REG_USB_ID_CTRL_SET);

	return ret;
}

static int twl6030battery_current_setup(void)
{
	int ret;
	u8 rd_reg;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID1, &rd_reg, PWDNSTATUS2);
	rd_reg = (rd_reg & 0x30) >> 2;
	rd_reg = rd_reg | FGDITHS | FGS;
	ret |= twl_i2c_write_u8(TWL6030_MODULE_ID1, rd_reg, REG_TOGGLE1);

	return ret;
}

static int twl6030battery_current_calibrate(void)
{
	int ret;
	u8 rd_reg;

	ret = twl_i2c_write_u8(TWL6030_MODULE_GASGAUGE, CC_CAL_EN, FG_REG_00);

	if (ret)
		return ret;

	/* Wait for calibration to finish */
	do {
		//printk(KERN_WARNING "CC: AUTOCAL\n");
		msleep(100);
		twl_i2c_read_u8(TWL6030_MODULE_GASGAUGE, &rd_reg, FG_REG_00);
	} while (rd_reg & CC_CAL_EN);

	return 0;
}

static int twl6030battery_current_shutdown(void)
{
	int ret;
	u8 rd_reg;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID1, &rd_reg, PWDNSTATUS2);
	rd_reg = (rd_reg & 0x30) >> 2;
	rd_reg = rd_reg | FGDITHR | FGR;
	ret |= twl_i2c_write_u8(TWL6030_MODULE_ID1, rd_reg, REG_TOGGLE1);
	ret |= twl_i2c_write_u8(TWL6030_MODULE_GASGAUGE, CC_CAL_EN, FG_REG_00);

	return ret;
}

static enum power_supply_property twl6030_bci_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
#if 0 /* FY11 EBOOK */
	POWER_SUPPLY_PROP_CURRENT_AVG,
#endif /* FY11 EBOOK */
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property twl6030_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property twl6030_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property twl6030_bk_bci_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};


#if 0	/* FY11: EBOOK */

static void twl6030_bci_temperature_work(struct work_struct *work)
{
	struct twl6030_bci_device_info *di = container_of(work,
	struct twl6030_bci_device_info, twl6030_bci_temperature_work.work);
	struct twl6030_gpadc_request req;
	int adc_code;
	int temp;
	int ret;
	
	req.channels = (1 << 1);
	if (twl_features() & TWL6025_SUBCLASS)
		req.method	= TWL6025_GPADC_SW2;
	else
		req.method	= TWL6030_GPADC_SW2;
	req.active = 0;
	req.func_cb = NULL;
	ret = twl6030_gpadc_conversion(&req);

	schedule_delayed_work(&di->twl6030_bci_temperature_work,
			msecs_to_jiffies(1000 * di->monitoring_interval));
	if (ret < 0) {
		dev_dbg(di->dev, "gpadc conversion failed: %d\n", ret);
		return;
	}

	if (di->battery_tmp_tbl == NULL)
		return;

	if (req.rbuf[1] > 100 && req.rbuf[1] < 950) {
		adc_code = req.rbuf[1];
		for (temp = 0; temp < di->battery_tmp_tblsize; temp++) {
			if (adc_code >= di->battery_tmp_tbl[temp])
				break;
		}

		/* first 2 values are for negative temperature */
		di->temp_C = (temp - 2) * 10; /* in tenths of degree Celsius */
	}

}

static void twl6030_bci_battery_work(struct work_struct *work)
{
	struct twl6030_bci_device_info *di = container_of(work,
	struct twl6030_bci_device_info, twl6030_bci_monitor_work.work);
	struct twl6030_gpadc_request req;
	int adc_code;
	int temp;
	int ret;
	
	req.channels = (1 << 1) | (1 << 7) | (1 << 8);
	if (twl_features() & TWL6025_SUBCLASS)
		req.method	= TWL6025_GPADC_SW2;
	else
		req.method	= TWL6030_GPADC_SW2;
	req.active = 0;
	req.func_cb = NULL;
	ret = twl6030_gpadc_conversion(&req);

	schedule_delayed_work(&di->twl6030_bci_monitor_work,
			msecs_to_jiffies(1000 * di->monitoring_interval));
	if (ret < 0) {
		dev_dbg(di->dev, "gpadc conversion failed: %d\n", ret);
		return;
	}

	if (req.rbuf[7] > 0)
		di->voltage_uV = req.rbuf[7];
	if (req.rbuf[8] > 0)
		di->bk_voltage_uV = req.rbuf[8];

	if (di->battery_tmp_tbl == NULL)
		return;

	if (req.rbuf[1] > 100 && req.rbuf[1] < 950) {
		adc_code = req.rbuf[1];
		for (temp = 0; temp < di->battery_tmp_tblsize; temp++) {
			if (adc_code >= di->battery_tmp_tbl[temp])
				break;
		}

		/* first 2 values are for negative temperature */
		di->temp_C = (temp - 2) * 10; /* in tenths of degree Celsius */
	}

}

static void twl6030_current_avg(struct work_struct *work)
{
	s32 samples;
	s16 cc_offset;
	int current_avg_uA;
	struct twl6030_bci_device_info *di = container_of(work,
		struct twl6030_bci_device_info,
		twl6030_current_avg_work.work);

	di->charge_n2 = di->charge_n1;
	di->timer_n2 = di->timer_n1;

	/* FG_REG_01, 02, 03 is 24 bit unsigned sample counter value */
	twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *) &di->timer_n1,
							FG_REG_01, 3);
	/*
	 * FG_REG_04, 5, 6, 7 is 32 bit signed accumulator value
	 * accumulates instantaneous current value
	 */
	twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *) &di->charge_n1,
							FG_REG_04, 4);
	/* FG_REG_08, 09 is 10 bit signed calibration offset value */
	twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *) &cc_offset,
							FG_REG_08, 2);
	cc_offset = ((s16)(cc_offset << 6) >> 6);
	di->cc_offset = cc_offset;

	samples = di->timer_n1 - di->timer_n2;
	/* check for timer overflow */
	if (di->timer_n1 < di->timer_n2)
		samples = samples + (1 << 24);

	cc_offset = cc_offset * samples;
	current_avg_uA = ((di->charge_n1 - di->charge_n2 - cc_offset)
							* 3000) >> 12;
	if (samples)
		current_avg_uA = current_avg_uA / samples;
	di->current_avg_uA = current_avg_uA * 1000;

	schedule_delayed_work(&di->twl6030_current_avg_work,
		msecs_to_jiffies(1000 * di->current_avg_interval));
}

#endif	/* FY11: EBOOK */

static void twl6030_current_mode_changed(struct twl6030_bci_device_info *di)
{

	/* FG_REG_01, 02, 03 is 24 bit unsigned sample counter value */
	twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *) &di->timer_n1,
							FG_REG_01, 3);
	/*
	 * FG_REG_04, 5, 6, 7 is 32 bit signed accumulator value
	 * accumulates instantaneous current value
	 */
	twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *) &di->charge_n1,
							FG_REG_04, 4);

#if 0 /* FY11 EBOOK */
	cancel_delayed_work(&di->twl6030_current_avg_work);
	schedule_delayed_work(&di->twl6030_current_avg_work,
		msecs_to_jiffies(1000 * di->current_avg_interval));
#endif /* FY11 EBOOK */
}


static void twl6030_work_interval_changed(struct twl6030_bci_device_info *di)
{
#if 0	/* FY11: EBOOK */
	cancel_delayed_work(&di->twl6030_bci_temperature_work);
	schedule_delayed_work(&di->twl6030_bci_temperature_work,
		msecs_to_jiffies(1000 * di->monitoring_interval));

	cancel_delayed_work(&di->twl6030_bci_monitor_work);
	schedule_delayed_work(&di->twl6030_bci_monitor_work,
		msecs_to_jiffies(1000 * di->monitoring_interval));
#endif
}

#define to_twl6030_bci_device_info(x) container_of((x), \
			struct twl6030_bci_device_info, bat);

static void twl6030_bci_battery_external_power_changed(struct power_supply *psy)
{
	//struct twl6030_bci_device_info *di = to_twl6030_bci_device_info(psy);

	//twl6025_battery_capacity();
	
#if 0	/* FY11: EBOOK */	
	cancel_delayed_work(&di->twl6030_bci_monitor_work);
	schedule_delayed_work(&di->twl6030_bci_monitor_work, 0);
#endif	/* FY11: EBOOK */
}

#define to_twl6030_ac_device_info(x) container_of((x), \
		struct twl6030_bci_device_info, ac);

static int twl6030_ac_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct twl6030_bci_device_info *di = to_twl6030_ac_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->ac_online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		//val->intval = twl6030_get_gpadc_conversion(9);	//External charger input
		val->intval = twl6030_get_gpadc_conversion(10);		//VBUS Voltage
		val->intval = di->voltage_uV;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define to_twl6030_usb_device_info(x) container_of((x), \
		struct twl6030_bci_device_info, usb);

static int twl6030_usb_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct twl6030_bci_device_info *di = to_twl6030_usb_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->usb_online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = twl6030_get_gpadc_conversion(10);		//VBUS Voltage
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define to_twl6030_bk_bci_device_info(x) container_of((x), \
		struct twl6030_bci_device_info, bk_bat);

static int twl6030_bk_bci_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct twl6030_bci_device_info *di = to_twl6030_bk_bci_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = di->bk_voltage_uV * 1000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int twl6030_bci_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct twl6030_bci_device_info *di;

	di = to_twl6030_bci_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = di->charge_status;
		//u8 res;
		//twl_i2c_read_u8(TWL6025_MODULE_CHARGER, &res, LINEAR_CHRG_STS);
		//printk("Read POWER_SUPPLY_PROP_STATUS =%02X\n",res);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		//twl6030battery_voltage(di);
		val->intval = di->voltage_uV;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		//if(di->disable_current_avg) {
		//	twl6030battery_current_setup();
		//	msleep((fuelgauge_time[di->fuelgauge_mode]/1000) + 10);
		//}
		//twl6030battery_current(di);
		//if(di->disable_current_avg)
		//	twl6030battery_current_shutdown();
		val->intval = di->current_uA;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = di->temp_C;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->charger_source;
		break;
#if 0 /* FY11 EBOOK */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = di->current_avg_uA;
		break;
#endif /* FY11 EBOOK */
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = di->bat_health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		//twl6025_battery_capacity();
		val->intval = di->capacity;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int twl6030_register_notifier(struct notifier_block *nb,
				unsigned int events)
{
	return blocking_notifier_chain_register(&notifier_list, nb);
}
EXPORT_SYMBOL_GPL(twl6030_register_notifier);

int twl6030_unregister_notifier(struct notifier_block *nb,
				unsigned int events)
{
	return blocking_notifier_chain_unregister(&notifier_list, nb);
}
EXPORT_SYMBOL_GPL(twl6030_unregister_notifier);

#if 1

static ssize_t set_battery_capacity(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val > 101))
		return -EINVAL;
	
	di->capacity = val;
	di->old_capacity = di->capacity;
	power_supply_changed(&di->bat);
	
	return status;
}

static ssize_t show_battery_capacity(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->capacity;
	return sprintf(buf, "%d\n", val);
}

#endif

static ssize_t set_fg_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val > 3))
		return -EINVAL;
		
	di->fuelgauge_mode = val;
	twl_i2c_write_u8(TWL6030_MODULE_GASGAUGE, (val << 6) | CC_CAL_EN,
							FG_REG_00);
	twl6030_current_mode_changed(di);
	return status;
}

static ssize_t show_fg_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->fuelgauge_mode;
	return sprintf(buf, "%d\n", val);
}

static ssize_t set_v_no_samples(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val > 1000))
		return -EINVAL;
	
	di->v_no_samples = val;

	return status;
}

static ssize_t show_v_no_samples(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->v_no_samples;
	return sprintf(buf, "%d\n", val);
}

static ssize_t set_charge_src(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 2) || (val > 3))
		return -EINVAL;
	di->vac_priority = val;
	return status;
}

static ssize_t show_charge_src(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->vac_priority;
	return sprintf(buf, "%d\n", val);
}

static ssize_t show_vbus_voltage(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;

	val = twl6030_get_gpadc_conversion(10);

	return sprintf(buf, "%d\n", val);
}

static ssize_t show_id_level(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	int val;

	val = twl6030_get_gpadc_conversion(14);

	return sprintf(buf, "%d\n", val);
}

static ssize_t set_watchdog(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 1) || (val > 127))
		return -EINVAL;
	di->watchdog_duration = val;
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, val, CONTROLLER_WDG);

	return status;
}

static ssize_t show_watchdog(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->watchdog_duration;
	return sprintf(buf, "%d\n", val);
}

static ssize_t show_fg_counter(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int fg_counter = 0;

	twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *) &fg_counter,
							FG_REG_01, 3);
	return sprintf(buf, "%d\n", fg_counter);
}

static ssize_t show_fg_accumulator(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	long fg_accum = 0;

	twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *) &fg_accum, FG_REG_04, 4);

	return sprintf(buf, "%ld\n", fg_accum);
}

static ssize_t show_fg_offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 fg_offset = 0;

	twl_i2c_read(TWL6030_MODULE_GASGAUGE, (u8 *) &fg_offset, FG_REG_08, 2);
	fg_offset = ((s16)(fg_offset << 6) >> 6);

	return sprintf(buf, "%d\n", fg_offset);
}

static ssize_t set_fg_clear(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	long val;
	int status = count;

	if ((strict_strtol(buf, 10, &val) < 0) || (val != 1))
		return -EINVAL;
	twl_i2c_write_u8(TWL6030_MODULE_GASGAUGE, CC_AUTOCLEAR, FG_REG_00);

	return status;
}

static ssize_t set_fg_cal(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	long val;
	int status = count;

	if ((strict_strtol(buf, 10, &val) < 0) || (val != 1))
		return -EINVAL;
	twl_i2c_write_u8(TWL6030_MODULE_GASGAUGE, CC_CAL_EN, FG_REG_00);

	return status;
}

static ssize_t set_charging(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if (strncmp(buf, "startac", 7) == 0) {
		if (di->charger_source == POWER_SUPPLY_TYPE_USB)
			twl6030_stop_usb_charger(di);
		twl6030_start_ac_charger(di);
	} else if (strncmp(buf, "startusb", 8) == 0) {
		if (di->charger_source == POWER_SUPPLY_TYPE_MAINS)
			twl6030_stop_ac_charger(di);
		twl6030_start_usb_charger(di);
	} else if (strncmp(buf, "stop" , 4) == 0)
		twl6030_stop_charger(di);
	else
		return -EINVAL;

	return status;
}

static ssize_t set_regulation_voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 3500)
				|| (val > di->max_charger_voltagemV))
		return -EINVAL;
	di->regulation_voltagemV = val;
	twl6030_config_voreg_reg(di, val);

	return status;
}

static ssize_t show_regulation_voltage(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->regulation_voltagemV;
	return sprintf(buf, "%u\n", val);
}

static ssize_t set_termination_current(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 50) || (val > 400))
		return -EINVAL;
	di->termination_currentmA = val;
	twl6030_config_iterm_reg(di, val);

	return status;
}

static ssize_t show_termination_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	unsigned int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->termination_currentmA;
	return sprintf(buf, "%u\n", val);
}

static ssize_t set_cin_limit(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 50) || (val > 1500))
		return -EINVAL;
	di->charger_incurrentmA = val;
	twl6030_config_cinlimit_reg(di, val);

	return status;
}

static ssize_t show_cin_limit(struct device *dev, struct device_attribute *attr,
								  char *buf)
{
	unsigned int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->charger_incurrentmA;
	return sprintf(buf, "%u\n", val);
}

static ssize_t set_charge_current(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 300)
				|| (val > di->max_charger_currentmA))
		return -EINVAL;
	di->charger_outcurrentmA = val;
	twl6030_config_vichrg_reg(di, val);

	return status;
}

static ssize_t show_charge_current(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->charger_outcurrentmA;
	return sprintf(buf, "%u\n", val);
}

static ssize_t set_min_vbus(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 4200) || (val > 4760))
		return -EINVAL;
	di->min_vbus = val;
	twl6030_config_min_vbus_reg(di, val);

	return status;
}

static ssize_t show_min_vbus(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	unsigned int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->min_vbus;
	return sprintf(buf, "%u\n", val);
}

static ssize_t set_current_avg_interval(struct device *dev,
	  struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 10) || (val > 3600))
		return -EINVAL;
	di->current_avg_interval = val;
	twl6030_current_mode_changed(di);

	return status;
}

static ssize_t show_current_avg_interval(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	unsigned int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->current_avg_interval;
	return sprintf(buf, "%u\n", val);
}

static ssize_t set_monitoring_interval(struct device *dev,
	  struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val < 10) || (val > 3600))
		return -EINVAL;
	di->monitoring_interval = val;
	twl6030_work_interval_changed(di);

	return status;
}

static ssize_t show_monitoring_interval(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	unsigned int val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->monitoring_interval;
	return sprintf(buf, "%u\n", val);
}

static ssize_t show_bsi(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	int val;

	val = twl6030_get_gpadc_conversion(0);
	return sprintf(buf, "%d\n", val);
}

static ssize_t show_stat1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->stat1;
	return sprintf(buf, "%u\n", val);
}

static ssize_t show_status_int1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->status_int1;
	return sprintf(buf, "%u\n", val);
}

static ssize_t show_status_int2(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned val;
	struct twl6030_bci_device_info *di = dev_get_drvdata(dev);

	val = di->status_int2;
	return sprintf(buf, "%u\n", val);
}

static DEVICE_ATTR(fg_mode, S_IWUSR | S_IRUGO, show_fg_mode, set_fg_mode);
#if 1
static DEVICE_ATTR(battery_capacity, S_IWUSR | S_IRUGO, show_battery_capacity, set_battery_capacity);
#endif 
static DEVICE_ATTR(v_no_samples, S_IWUSR | S_IRUGO, show_v_no_samples, set_v_no_samples);
static DEVICE_ATTR(charge_src, S_IWUSR | S_IRUGO, show_charge_src,
		set_charge_src);
static DEVICE_ATTR(vbus_voltage, S_IRUGO, show_vbus_voltage, NULL);
static DEVICE_ATTR(id_level, S_IRUGO, show_id_level, NULL);
static DEVICE_ATTR(watchdog, S_IWUSR | S_IRUGO, show_watchdog, set_watchdog);
static DEVICE_ATTR(fg_counter, S_IRUGO, show_fg_counter, NULL);
static DEVICE_ATTR(fg_accumulator, S_IRUGO, show_fg_accumulator, NULL);
static DEVICE_ATTR(fg_offset, S_IRUGO, show_fg_offset, NULL);
static DEVICE_ATTR(fg_clear, S_IWUSR, NULL, set_fg_clear);
static DEVICE_ATTR(fg_cal, S_IWUSR, NULL, set_fg_cal);
static DEVICE_ATTR(charging, S_IWUSR | S_IRUGO, NULL, set_charging);
static DEVICE_ATTR(regulation_voltage, S_IWUSR | S_IRUGO,
		show_regulation_voltage, set_regulation_voltage);
static DEVICE_ATTR(termination_current, S_IWUSR | S_IRUGO,
		show_termination_current, set_termination_current);
static DEVICE_ATTR(cin_limit, S_IWUSR | S_IRUGO, show_cin_limit,
		set_cin_limit);
static DEVICE_ATTR(charge_current, S_IWUSR | S_IRUGO, show_charge_current,
		set_charge_current);
static DEVICE_ATTR(min_vbus, S_IWUSR | S_IRUGO, show_min_vbus, set_min_vbus);
static DEVICE_ATTR(monitoring_interval, S_IWUSR | S_IRUGO,
		show_monitoring_interval, set_monitoring_interval);
static DEVICE_ATTR(current_avg_interval, S_IWUSR | S_IRUGO,
		show_current_avg_interval, set_current_avg_interval);
static DEVICE_ATTR(bsi, S_IRUGO, show_bsi, NULL);
static DEVICE_ATTR(stat1, S_IRUGO, show_stat1, NULL);
static DEVICE_ATTR(status_int1, S_IRUGO, show_status_int1, NULL);
static DEVICE_ATTR(status_int2, S_IRUGO, show_status_int2, NULL);

static struct attribute *twl6030_bci_attributes[] = {
	&dev_attr_fg_mode.attr,
#if 1
	&dev_attr_battery_capacity.attr,
#endif
	&dev_attr_v_no_samples.attr,
	&dev_attr_charge_src.attr,
	&dev_attr_vbus_voltage.attr,
	&dev_attr_id_level.attr,
	&dev_attr_watchdog.attr,
	&dev_attr_fg_counter.attr,
	&dev_attr_fg_accumulator.attr,
	&dev_attr_fg_offset.attr,
	&dev_attr_fg_clear.attr,
	&dev_attr_fg_cal.attr,
	&dev_attr_charging.attr,
	&dev_attr_regulation_voltage.attr,
	&dev_attr_termination_current.attr,
	&dev_attr_cin_limit.attr,
	&dev_attr_charge_current.attr,
	&dev_attr_min_vbus.attr,
	&dev_attr_monitoring_interval.attr,
	&dev_attr_current_avg_interval.attr,
	&dev_attr_bsi.attr,
	&dev_attr_stat1.attr,
	&dev_attr_status_int1.attr,
	&dev_attr_status_int2.attr,
	NULL,
};

static const struct attribute_group twl6030_bci_attr_group = {
	.attrs = twl6030_bci_attributes,
};

static char *twl6030_bci_supplied_to[] = {
	"twl6030_battery",
};

/* this function gets the battery capacity */
int twl6025_battery_capacity(void)
{
	struct twl6030_bci_device_info *di;
	int diff, changed = 0;
	unsigned int prev_capacity;
	
	if (!the_di || !the_di->bat_registered)
		return -ENODEV;

	di = the_di;

	if (!di->vprime_callback) {
		dev_dbg(di->dev, "missing vprime_callback\n");
		return -ENODEV;
	}

	if(!di->capacity_callback) {
		dev_dbg(di->dev, "missing capacity_callback\n");
		return -ENODEV;
	}
	
	wake_lock(&update_battery_capacity);

	/* Read the voltage which is an average over the same time period
	 * as the fuelgauge then read the fuelgauge measurement for this
	 * same period */
	
	if(di->disable_current_avg)
		twl6030battery_current_setup();
	
	twl6030battery_voltage(di);
	twl6030battery_current(di);
	twl6030_bci_temperature(di);
	
	if(di->fuelgauge_recal == di->fuelgauge_recal_cur) {
		twl6030battery_current_calibrate();
		di->fuelgauge_recal_cur = 0;
	} else
		di->fuelgauge_recal_cur ++;

	if(di->disable_current_avg)
		twl6030battery_current_shutdown();

	/* Calculate the V' from V compensating for the IR drop in battery */
	di->vprime = di->vprime_callback(di->voltage_uV, di->current_uA);

	/* Use V' to calculate the capacity of the battery capacity is
         * returned in % */
	prev_capacity = di->capacity;
	di->capacity = di->capacity_callback(di->vprime);
	
	/* If we are not charging then an upwards change in capacity
         * is environmental and we dont want to report it
         */
	if ((di->capacity > prev_capacity) &&
		(di->charge_status != POWER_SUPPLY_STATUS_CHARGING))
		di->capacity = prev_capacity;

	diff = abs(di->capacity - di->old_capacity);

	/* If there is a difference of 5% in capacity or we just reached
         * 100 or 0 and this is a change from last value then indicate
         * to userspace that there has been a change in capacity */
	if (diff > 5) {
		changed = 1;
	} else if (di->capacity != di->old_capacity) {
		if (di->capacity == 100)
			changed = 1;
		else if (di->capacity == 0)
			changed = 1;
		else
			changed = 0;
	}

	if (changed) {
		di->old_capacity = di->capacity;
		power_supply_changed(&di->bat);
	}
	
	if (wake_lock_active(&update_battery_capacity)){
		wake_unlock(&update_battery_capacity);
	}

	return 0;
}
EXPORT_SYMBOL(twl6025_battery_capacity);


static int __devinit twl6030_bci_battery_probe(struct platform_device *pdev)
{
	struct twl4030_bci_platform_data *pdata = pdev->dev.platform_data;
	struct twl6030_bci_device_info *di;
	int irq;
	int ret;
	u8 controller_stat = 0, reg;
	
	wake_lock_init(&update_battery_capacity, WAKE_LOCK_SUSPEND, "twl_update_battery_capacity");

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		ret = -EINVAL;
		goto err_pdata;
	}

	if (pdata->monitoring_interval == 0) {
		di->monitoring_interval = 10;
		di->current_avg_interval = 10;
	} else {
		di->monitoring_interval = pdata->monitoring_interval;
		di->current_avg_interval = pdata->monitoring_interval;
	}

	di->dev = &pdev->dev;

	if (pdata->use_eeprom_config) {
		di->max_charger_currentmA = twl6030_get_limit2_reg(di);
		di->max_charger_voltagemV = twl6030_get_limit1_reg(di);
		di->termination_currentmA = twl6030_get_iterm_reg(di);
		di->regulation_voltagemV = twl6030_get_voreg_reg(di);
		di->low_bat_voltagemV = pdata->low_bat_voltagemV;
		dev_dbg(di->dev, "EEPROM max charge %d mA\n",
			di->max_charger_currentmA);
		dev_dbg(di->dev, "EEPROM max voltage %d mV\n",
			di->max_charger_voltagemV);
		dev_dbg(di->dev, "EEPROM termination %d mA\n",
			di->termination_currentmA);
		dev_dbg(di->dev, "EEPROM regulation %d mV\n",
			di->regulation_voltagemV);
	} else {
		di->max_charger_currentmA = pdata->max_charger_currentmA;
		di->max_charger_voltagemV = pdata->max_bat_voltagemV;
		di->termination_currentmA = pdata->termination_currentmA;
		di->regulation_voltagemV = pdata->max_bat_voltagemV;
		di->low_bat_voltagemV = pdata->low_bat_voltagemV;
	}
	di->battery_tmp_tbl = pdata->battery_tmp_tbl;
	di->battery_tmp_tblsize = pdata->battery_tmp_tblsize;
	di->battery_volt_tbl = pdata->battery_volt_tbl;
	di->battery_volt_tblsize = pdata->battery_volt_tblsize;
	di->use_eeprom_config = pdata->use_eeprom_config;
	di->use_hw_charger = pdata->use_hw_charger;
	di->power_path = pdata->power_path;

	di->bat.name = "twl6030_battery";
	di->bat.supplied_to = twl6030_bci_supplied_to;
	di->bat.num_supplicants = ARRAY_SIZE(twl6030_bci_supplied_to);
	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties = twl6030_bci_battery_props;
	di->bat.num_properties = ARRAY_SIZE(twl6030_bci_battery_props);
	di->bat.get_property = twl6030_bci_battery_get_property;
	di->bat.external_power_changed =
			twl6030_bci_battery_external_power_changed;
	di->bat_health = POWER_SUPPLY_HEALTH_GOOD;

	di->usb.name = "twl6030_usb";
	di->usb.type = POWER_SUPPLY_TYPE_USB;
	di->usb.properties = twl6030_usb_props;
	di->usb.num_properties = ARRAY_SIZE(twl6030_usb_props);
	di->usb.get_property = twl6030_usb_get_property;

	di->ac.name = "twl6030_ac";
	di->ac.type = POWER_SUPPLY_TYPE_MAINS;
	di->ac.properties = twl6030_ac_props;
	di->ac.num_properties = ARRAY_SIZE(twl6030_ac_props);
	di->ac.get_property = twl6030_ac_get_property;

	di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;

	di->bk_bat.name = "twl6030_bk_battery";
	di->bk_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bk_bat.properties = twl6030_bk_bci_battery_props;
	di->bk_bat.num_properties = ARRAY_SIZE(twl6030_bk_bci_battery_props);
	di->bk_bat.get_property = twl6030_bk_bci_battery_get_property;

	di->vac_priority = 2;
	
	di->capacity = 100;
	
	u8 vbus_state;
	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &vbus_state, CONTROLLER_STAT1);
	if(vbus_state & VBUS_DET){
		di->charger_source = POWER_SUPPLY_TYPE_MAINS;
		di->usb_online = 0;
		di->ac_online = 1;
	}
		
	
	/* Averaging calculations */
	di->v_no_samples = pdata->initial_v_no_samples;
	
	/* Capacity Calculations */
	di->vprime_callback = pdata->vprime_callback;
	di->capacity_callback = pdata->capacity_callback;
	di->disable_current_avg = pdata->disable_current_avg;
	di->fuelgauge_recal = di->fuelgauge_recal_cur = pdata->fuelgauge_recal;

	/* Use channel if defined in pdata or default to 7 */
#if 1 /* FY11 E_BOOK */
	if ( !gpio_get_value(BOARD_ID) )  {	//In the case of BB.
		di->vbat_channel = 7;
	}else{								//In the case of Set.
		di->vbat_channel = 8;
	}
#else
	if (pdata->vbat_channel)
		di->vbat_channel = pdata->vbat_channel;
	else
		di->vbat_channel = 7;
#endif /* FY11 E_BOOK */
	
	platform_set_drvdata(pdev, di);
	the_di = di;

	/* settings for temperature sensing */
	ret = twl6030battery_temp_setup();
	if (ret)
		goto temp_setup_fail;

	/* request charger fault interruption */
	irq = platform_get_irq(pdev, 1);
	ret = request_threaded_irq(irq, NULL, twl6030charger_fault_interrupt,
		0, "twl_bci_fault", di);
	if (ret) {
		dev_dbg(&pdev->dev, "could not request irq %d, status %d\n",
			irq, ret);
		goto temp_setup_fail;
	}

	/* request charger ctrl interruption */
	irq = platform_get_irq(pdev, 0);
	ret = request_threaded_irq(irq, NULL, twl6030charger_ctrl_interrupt,
		0, "twl_bci_ctrl", di);

	if (ret) {
		dev_dbg(&pdev->dev, "could not request irq %d, status %d\n",
			irq, ret);
		goto chg_irq_fail;
	}

	ret = power_supply_register(&pdev->dev, &di->bat);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to register main battery\n");
		goto batt_failed;
	}

	di->bat_registered = 1;
	BLOCKING_INIT_NOTIFIER_HEAD(&notifier_list);
#if 1	/* FY11: EBOOK */
	INIT_DELAYED_WORK_DEFERRABLE(&di->twl6030_check_battery_state_work,
								twl6030_check_battery_state_work);
#else
	INIT_DELAYED_WORK_DEFERRABLE(&di->twl6030_bci_temperature_work, 
								twl6030_bci_temperature_work);
	schedule_delayed_work(&di->twl6030_bci_temperature_work, 0);
	
	INIT_DELAYED_WORK_DEFERRABLE(&di->twl6030_bci_monitor_work,
				twl6030_bci_battery_work);
	schedule_delayed_work(&di->twl6030_bci_monitor_work, 0);
#endif	/* FY11: EBOOK */

	ret = power_supply_register(&pdev->dev, &di->usb);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to register usb power supply\n");
		goto usb_failed;
	}

	ret = power_supply_register(&pdev->dev, &di->ac);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to register ac power supply\n");
		goto ac_failed;
	}

	ret = power_supply_register(&pdev->dev, &di->bk_bat);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to register backup battery\n");
		goto bk_batt_failed;
	}
	di->charge_n1 = 0;
	di->timer_n1 = 0;

#if 0 /* FY11 EBOOK */
	if (!di->disable_current_avg) {
		INIT_DELAYED_WORK_DEFERRABLE(&di->twl6030_current_avg_work,
						twl6030_current_avg);
		schedule_delayed_work(&di->twl6030_current_avg_work, 500);
		twl6030battery_current_setup();
		twl6030battery_current_calibrate();
	}
#endif /* FY11 EBOOK */

	ret = twl6030battery_voltage_setup();
	if (ret)
		dev_dbg(&pdev->dev, "voltage measurement setup failed\n");

	/* initialize for USB charging */
	if (!pdata->use_eeprom_config) {
		/* initialize for USB charging */
		twl6030_config_limit1_reg(di, pdata->max_charger_voltagemV);
		twl6030_config_limit2_reg(di, di->max_charger_currentmA);
	}
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, MBAT_TEMP,
						CONTROLLER_INT_MASK);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, MASK_MCHARGERUSB_THMREG,
						CHARGERUSB_INT_MASK);

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &controller_stat,
		CONTROLLER_STAT1);

	di->charger_incurrentmA = di->max_charger_currentmA;
	di->charger_outcurrentmA = di->max_charger_currentmA;
	di->watchdog_duration = 32;
#if 0 /* FY11 EBOOK */
	di->voltage_uV = twl6030_get_gpadc_conversion(7);
	dev_info(&pdev->dev, "Battery Voltage at Bootup is %d mV\n",
							di->voltage_uV);
#endif /* FY11 EBOOK */

	if (!((twl_features() & TWL6025_SUBCLASS) && di->use_hw_charger)) {
		if (controller_stat & VBUS_DET) {
			di->usb_online = POWER_SUPPLY_TYPE_USB;
			twl6030_start_usb_charger(di);
		}

		if (controller_stat & VAC_DET) {
			di->ac_online = POWER_SUPPLY_TYPE_MAINS;
			twl6030_start_ac_charger(di);
		}
	} else {		
		
		twl_i2c_read_u8(TWL6025_MODULE_CHARGER, &reg, LINEAR_CHRG_STS);

		if (reg & 0x3)
			di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
		else if (reg & LINEAR_CHRG_STS_END_OF_CHARGE)
			di->charge_status = POWER_SUPPLY_STATUS_FULL;
		else {
			if(controller_stat & (VBUS_DET | VAC_DET)){
				di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			else
				di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	}

	if (!pdata->disable_backup_charge)
	{
		ret = twl6030backupbatt_setup();
		if (ret)
			dev_dbg(&pdev->dev,
				"Backup Bat charging setup failed\n");
	}

	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
						REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
						REG_INT_MSK_STS_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_FAULT_INT_MASK,
						REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_FAULT_INT_MASK,
						REG_INT_MSK_STS_C);

	ret = sysfs_create_group(&pdev->dev.kobj, &twl6030_bci_attr_group);
	if (ret)
		dev_dbg(&pdev->dev, "could not create sysfs files\n");

	return 0;

bk_batt_failed:
	power_supply_unregister(&di->ac);
ac_failed:
	power_supply_unregister(&di->usb);
usb_failed:
#if 1	/* FY11: EBOOK */
	cancel_delayed_work(&di->twl6030_check_battery_state_work);
#else
	cancel_delayed_work(&di->twl6030_bci_temperature_work);
	cancel_delayed_work(&di->twl6030_bci_monitor_work);
#endif	/* FY11: EBOOK */
	power_supply_unregister(&di->bat);
batt_failed:
	free_irq(irq, di);
chg_irq_fail:
	irq = platform_get_irq(pdev, 1);
	free_irq(irq, di);
temp_setup_fail:
	platform_set_drvdata(pdev, NULL);
err_pdata:
	kfree(di);

	return ret;
}

int confirm_the_di(void)
{
	return the_di;
}
EXPORT_SYMBOL(confirm_the_di);

static int __devexit twl6030_bci_battery_remove(struct platform_device *pdev)
{
	struct twl6030_bci_device_info *di = platform_get_drvdata(pdev);
	int irq;

	twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
						REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
						REG_INT_MSK_STS_C);
	twl6030_interrupt_mask(TWL6030_CHARGER_FAULT_INT_MASK,
						REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_CHARGER_FAULT_INT_MASK,
						REG_INT_MSK_STS_C);

	irq = platform_get_irq(pdev, 0);
	free_irq(irq, di);

	irq = platform_get_irq(pdev, 1);
	free_irq(irq, di);

	sysfs_remove_group(&pdev->dev.kobj, &twl6030_bci_attr_group);
#if 1	/* FY11: EBOOK */
	cancel_delayed_work(&di->twl6030_check_battery_state_work);
#else
	cancel_delayed_work(&di->twl6030_bci_temperature_work);
	cancel_delayed_work(&di->twl6030_bci_monitor_work);
	cancel_delayed_work(&di->twl6030_current_avg_work);
#endif	/* FY11: EBOOK */
	flush_scheduled_work();
	power_supply_unregister(&di->bat);
	power_supply_unregister(&di->usb);
	power_supply_unregister(&di->ac);
	power_supply_unregister(&di->bk_bat);
	platform_set_drvdata(pdev, NULL);
	kfree(di);

	wake_lock_destroy(&update_battery_capacity);
	
	return 0;
}

#ifdef CONFIG_PM
static int twl6030_bci_battery_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct twl6030_bci_device_info *di = platform_get_drvdata(pdev);
	u8 rd_reg;

	/* mask to prevent wakeup due to 32s timeout from External charger */
	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &rd_reg, CONTROLLER_INT_MASK);
	rd_reg |= MVAC_FAULT;
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, MBAT_TEMP,
						CONTROLLER_INT_MASK);

#if 1	/* FY11: EBOOK */
	cancel_delayed_work(&di->twl6030_check_battery_state_work);
#else
	cancel_delayed_work(&di->twl6030_bci_temperature_work);
	cancel_delayed_work(&di->twl6030_bci_monitor_work);
	cancel_delayed_work(&di->twl6030_current_avg_work);
#endif	/* FY11: EBOOK */
	return 0;
}

static int twl6030_bci_battery_resume(struct platform_device *pdev)
{
	struct twl6030_bci_device_info *di = platform_get_drvdata(pdev);
	u8 rd_reg;

	twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &rd_reg, CONTROLLER_INT_MASK);
	rd_reg &= ~(0xFF & MVAC_FAULT);
	twl_i2c_write_u8(TWL6030_MODULE_CHARGER, MBAT_TEMP,
						CONTROLLER_INT_MASK);

#if 1	/* FY11: EBOOK */
	schedule_delayed_work(&di->twl6030_check_battery_state_work, 0);
#else
	schedule_delayed_work(&di->twl6030_bci_temperature_work, 0);
	schedule_delayed_work(&di->twl6030_bci_monitor_work, 0);
	schedule_delayed_work(&di->twl6030_current_avg_work, 50);
#endif	/* FY11: EBOOK */
	return 0;
}
#else
#define twl6030_bci_battery_suspend	NULL
#define twl6030_bci_battery_resume	NULL
#endif /* CONFIG_PM */

static struct platform_driver twl6030_bci_battery_driver = {
	.probe		= twl6030_bci_battery_probe,
	.remove		= __devexit_p(twl6030_bci_battery_remove),
	.suspend	= twl6030_bci_battery_suspend,
	.resume		= twl6030_bci_battery_resume,
	.driver		= {
		.name	= "twl6030_bci",
	},
};

static int __init twl6030_battery_init(void)
{
	return platform_driver_register(&twl6030_bci_battery_driver);
}
module_init(twl6030_battery_init);

static void __exit twl6030_battery_exit(void)
{
	platform_driver_unregister(&twl6030_bci_battery_driver);
}
module_exit(twl6030_battery_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:twl6030_bci");
MODULE_AUTHOR("Texas Instruments Inc");

/*
 * drivers/power/ricoh619-battery.c
 *
 * Charger driver for RICOH R5T619 power management chip.
 *
 * Copyright (C) 2012-2014 RICOH COMPANY,LTD
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
#define RICOH61x_BATTERY_VERSION "RICOH61x_BATTERY_VERSION: 2014.02.21 V3.1.0.0-Solution1 2015/02/09"


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/mfd/ricoh619.h>
#include <linux/power/ricoh619_battery.h>
#include <linux/power/ricoh61x_battery_init.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/rtc.h>
#include <asm-generic/rtc.h>

#include <linux/power/ricoh619_standby.h>

#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"
extern volatile NTX_HWCONFIG *gptHWCFG;

#define ALL_LPTFT_PANEL		1
#if 0
#define SUSPEND_PRINT(fmt,args...)
#else
#define SUSPEND_PRINT(fmt,args...)		printk(fmt,##args)
#endif

#define BATTERY_ADC_UNUSED_TEMP -999
static int gBatt_ADC_isr_temp = BATTERY_ADC_UNUSED_TEMP;
static unsigned long gBatt_ADC_isr_tick;

/* define for function */
#define ENABLE_FUEL_GAUGE_FUNCTION
#define ENABLE_LOW_BATTERY_DETECTION
#define ENABLE_FACTORY_MODE
#define DISABLE_CHARGER_TIMER
/* #define ENABLE_FG_KEEP_ON_MODE */
#define ENABLE_OCV_TABLE_CALIB
/* #define ENABLE_MASKING_INTERRUPT_IN_SLEEP */

#define	ENABLE_BATTERY_TEMP_DETECTION
#define BATTERY_TEMP_0_VOL		1824	// 0 degrees
#define BATTERY_TEMP_5_VOL		1764	// 5 degrees
#define BATTERY_TEMP_8_VOL		1710	// 8 degrees
#define BATTERY_TEMP_40_VOL	870		// 40 degrees
#define BATTERY_TEMP_48_VOL	622		// 48 degrees
#define BATTERY_TEMP_50_VOL	606		// 50 degrees

#define _RICOH619_DEBUG_
#define LTS_DEBUG
//#define STANDBY_MODE_DEBUG
//#define CHANGE_FL_MODE_DEBUG

/* FG setting */
#define RICOH61x_REL1_SEL_VALUE		64
#define RICOH61x_REL2_SEL_VALUE		0

enum int_type {
	SYS_INT  = 0x01,
	DCDC_INT = 0x02,
	ADC_INT  = 0x08,
	GPIO_INT = 0x10,
	CHG_INT	 = 0x40,
};

#ifdef ENABLE_FUEL_GAUGE_FUNCTION
/* define for FG delayed time */
#define RICOH61x_MONITOR_START_TIME		15
#define RICOH61x_FG_RESET_TIME			6
#define RICOH61x_MAIN_START_TIME		2
#define RICOH61x_FG_STABLE_TIME			120
#define RICOH61x_DISP_CHG_UPDATE_TIME		10
#define RICOH61x_DISPLAY_UPDATE_TIME		29
#define RICOH61x_LOW_VOL_DOWN_TIME		60	//10
#define RICOH61x_CHARGE_MONITOR_TIME		19
#define RICOH61x_CHARGE_RESUME_TIME		1
#define RICOH61x_CHARGE_CALC_TIME		1
#define RICOH61x_JEITA_UPDATE_TIME		60
#define RICOH61x_DELAY_TIME			40	/* 120 */
/* define for FG parameter */
#define RICOH61x_MAX_RESET_SOC_DIFF		5
#define RICOH61x_GET_CHARGE_NUM			10
#define RICOH61x_UPDATE_COUNT_DISP		4
#define RICOH61x_UPDATE_COUNT_FULL		4
#define RICOH61x_UPDATE_COUNT_FULL_RESET 	4
#define RICOH61x_CHARGE_UPDATE_TIME		3
#define RE_CAP_GO_DOWN				10	/* 40 */
#define RICOH61x_ENTER_LOW_VOL			70
#define RICOH61x_TAH_SEL2			5
#define RICOH61x_TAL_SEL2			6

#define RICOH61x_OCV_OFFSET_BOUND	3
#define RICOH61x_OCV_OFFSET_RATIO	2

#define RICOH61x_ENTER_FULL_STATE_OCV	9
#define RICOH61x_ENTER_FULL_STATE_DSOC	85	/* 90 */

#define RICOH61x_FL_LEVEL_DEF			70	// 70%
#define RICOH61x_FL_CURRENT_DEF			29593	// 29.593mA(70%)
#define RICOH61x_IDLE_CURRENT_DEF		20000	// 20mA
#define RICOH61x_SUS_CURRENT_DEF		3000	// 3mA
#define RICOH61x_SUS_CURRENT_THRESH		20000	// 20mA
#define RICOH61x_HIBER_CURRENT_DEF		800	// 0.8mA
#define RICOH61x_FL_CURRENT_LIMIT		150000	// 150mA
#define RICOH61x_SLEEP_CURRENT_LIMIT		50000	// 50mA

#define ORIGINAL	0
#define USING		1


/* define for FG status */
enum {
	RICOH61x_SOCA_START,
	RICOH61x_SOCA_UNSTABLE,
	RICOH61x_SOCA_FG_RESET,
	RICOH61x_SOCA_DISP,
	RICOH61x_SOCA_STABLE,
	RICOH61x_SOCA_ZERO,
	RICOH61x_SOCA_FULL,
	RICOH61x_SOCA_LOW_VOL,
};

/* table of dividing charge current */
#define RICOH61x_IBAT_TABLE_NUM			16
static int ibat_table[RICOH61x_IBAT_TABLE_NUM]		/* 85% - 100% */
		 = {370, 348, 326, 304, 282, 260, 238, 216, 194, 172, 150, 128, 107, 87, 68, 50};

#endif

#ifdef ENABLE_LOW_BATTERY_DETECTION
#define LOW_BATTERY_DETECTION_TIME		0	//10
#endif

struct ricoh61x_soca_info {
	int Rbat;
	int n_cap;
	int ocv_table_def[11];
	int ocv_table[11];
	int ocv_table_low[11];
	uint8_t battery_init_para_original[32];
	int soc;		/* Latest FG SOC value */
	int displayed_soc;
	int suspend_soc;
	int suspend_cc;
	int suspend_rsoc;
	bool suspend_full_flg;
	int status;		/* SOCA status 0: Not initial; 5: Finished */
	int stable_count;
	int chg_status;		/* chg_status */
	int soc_delta;		/* soc delta for status3(DISP) */
	int cc_delta;
	int cc_cap_offset;
	long sus_cc_cap_offset;
	int last_soc;
	int last_displayed_soc;
	int last_cc_rrf0;
	int last_cc_delta_cap;
	long last_cc_delta_cap_mas;
	long temp_cc_delta_cap_mas;
	int temp_cc_delta_cap;
	int ready_fg;
	int reset_count;
	int reset_soc[3];
	int dischg_state;
	int Vbat[RICOH61x_GET_CHARGE_NUM];
	int Vsys[RICOH61x_GET_CHARGE_NUM];
	int Ibat[RICOH61x_GET_CHARGE_NUM];
	int Vbat_ave;
	int Vbat_old;
	int Vsys_ave;
	int Ibat_ave;
	int chg_count;
	int full_reset_count;
	int soc_full;
	int fc_cap;
	/* for LOW VOL state */
	int hurry_up_flg;
	int zero_flg;
	int re_cap_old;

	int cutoff_ocv;
	int Rsys;
	int target_vsys;
	int target_ibat;
	int jt_limit;
	int OCV100_min;
	int OCV100_max;
	int R_low;
	int rsoc_ready_flag;
	int init_pswr;
	int last_soc_full;
	int rsoc_limit;
	int critical_low_flag;

	int store_fl_current;
	int store_slp_state;
	int store_sus_current;
	int store_hiber_current;
};

static int critical_low_flag = 0;

struct ricoh61x_battery_info {
	struct device      *dev;
	struct power_supply	*battery;
	struct delayed_work	monitor_work;
	struct delayed_work	displayed_work;
	struct delayed_work	charge_stable_work;
	struct delayed_work	changed_work;
#ifdef ENABLE_LOW_BATTERY_DETECTION
	struct delayed_work	low_battery_work;
#endif
#ifdef ENABLE_BATTERY_TEMP_DETECTION
	int iEnableTempDetect;
	struct delayed_work	battery_temp_work;
#endif
	struct delayed_work	charge_monitor_work;
	struct delayed_work	get_charge_work;
	struct delayed_work	jeita_work;

	struct work_struct	irq_work;	/* for Charging & VADP/VUSB */

	struct workqueue_struct *monitor_wqueue;
	struct workqueue_struct *workqueue;	/* for Charging & VUSB/VADP */
	struct workqueue_struct *usb_workqueue;	/* for ADC_VUSB */

#ifdef ENABLE_FACTORY_MODE
	struct delayed_work	factory_mode_work;
	struct workqueue_struct *factory_mode_wqueue;
#endif

	struct mutex		lock;
	unsigned long		monitor_time;
	int		adc_vdd_mv;
	int		multiple;
	int		alarm_vol_mv;
	int		status;
	int		min_voltage;
	int		max_voltage;
	int		cur_voltage;
	int		capacity;
	int		battery_temp;
	int		time_to_empty;
	int		time_to_full;
	int		chg_ctr;
	int		chg_stat1;
	unsigned	present:1;
	u16		delay;
	struct		ricoh61x_soca_info *soca;
	int		first_pwon;
	bool		entry_factory_mode;
	int		ch_vfchg;
	int		ch_vrchg;
	int		ch_vbatovset;
	int		ch_ichg;
	int		ch_ilim_adp;
	int		ch_ilim_usb;
	int		ch_icchg;
	int		fg_target_vsys;
	int		fg_target_ibat;
	int		fg_poff_vbat;
	int		fg_sus_curr;
	int		fg_hiber_curr;
	int		jt_en;
	int		jt_hw_sw;
	int		jt_temp_h;
	int		jt_temp_l;
	int		jt_vfchg_h;
	int		jt_vfchg_l;
	int		jt_ichg_h;
	int		jt_ichg_l;
	bool		suspend_state;
	bool		stop_disp;
	unsigned long	sleepEntryTime;
	unsigned long	sleepExitTime;

	int 		num;
	};

struct power_supply *powerac;
struct power_supply *powerusb;
struct power_supply *powerntx;

int g_full_flag;
int charger_irq;
int g_soc;
int g_fg_on_mode;
int g_fake_soc = -1;
int type_n;

static void _ricoh_suspend_state_sync(struct ricoh61x_battery_info *info);

int giCapacityMethod = 0; // 0:default ; 1:voltage

static ssize_t show_fake_soc(struct device *device,
			struct device_attribute *attr, char *buf) {
	return sprintf(buf, "fake soc = %d\r\n", g_fake_soc);
}

static ssize_t store_fake_soc(struct device *device,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
        int desired_soc;
        int ret;

        desired_soc = simple_strtol(buf, NULL, 10);
        if (desired_soc < -1 || desired_soc > 100) {
                return -EINVAL;
        }

	g_fake_soc = desired_soc;
	printk("store fake soc [%d]\r\n", g_fake_soc);
	return count;
}
static DEVICE_ATTR(fake_soc, 0644, show_fake_soc, store_fake_soc);



static int add_fake_soc_sysfs(struct device *dev) {
	int err;
	err = device_create_file(dev, &dev_attr_fake_soc);
	if (err< 0)
		printk("**FAILED adding fake_soc sysfs\r\n");
	return err;
}

static void remove_fake_soc_sysfs(struct device *dev) {
	device_remove_file(dev, &dev_attr_fake_soc);
}


extern int ricoh619_charger_detect(void);
typedef void (*usb_insert_handler) (char inserted);
#if 0
extern usb_insert_handler mxc_misc_report_usb;
#endif
static int giRICOH619_DCIN;
static int _config_ricoh619_charger_params(struct device *bat_dev,int iChargerType)
{
	uint8_t val = 0, ilim_adp = 0, ilim_usb = 0, ichg = 0;
	
	ricoh61x_read(bat_dev->parent, CHGISET_REG, &val);
	val &= 0xe0;
	//if (status&0x30)
	if(iChargerType==CDP_CHARGER||iChargerType==DCP_CHARGER)
	{	// DCP(10) or CDP(01) .
		printk(KERN_INFO "PMU:%s set DCP/CDP charging.\n", __func__);
		switch (gptHWCFG->m_val.bPCB) {
		case 49:  //E60QDX
			ilim_adp = 0x09;	//1000mA
			ilim_usb = 0x29;	//1000mA
			ichg = 0x07; 		//800mA
			break;
		default:
			ilim_adp = 0x0D;  	//1400mA
			ilim_usb = 0x2D;  	//1400mA
			ichg = 0x09;		//1000mA
			break;
		}
		ricoh61x_write(bat_dev->parent, REGISET1_REG, ilim_adp);
		ricoh61x_write(bat_dev->parent, REGISET2_REG, ilim_usb);
		ricoh61x_write(bat_dev->parent, CHGISET_REG, val|ichg);
	}
	else if(iChargerType==SDP_ADPT_CHARGER)
	{	// SDP Adptor charger .
		printk(KERN_INFO "PMU:%s set SDP 800mA charging.\n", __func__);
		ricoh61x_write(bat_dev->parent, REGISET1_REG, 0x09);
		ricoh61x_write(bat_dev->parent, REGISET2_REG, 0x29);
		ricoh61x_write(bat_dev->parent, CHGISET_REG, val|0x07);
	}
	else 
	{	// SDP PC
		if (iChargerType!=NO_CHARGER_PLUGGED)
			printk(KERN_INFO "PMU:%s set SDP 500mA charging. (type %d)\n", __func__, iChargerType);
		ricoh61x_write(bat_dev->parent, REGISET1_REG, 0x06);
		ricoh61x_write(bat_dev->parent, REGISET2_REG, 0xE6);
		ricoh61x_write(bat_dev->parent, CHGISET_REG, val|0x04);
	}

	return 0;
}


static ssize_t charger_type_read(struct device *dev, struct device_attribute *attr,char *buf)
{
	switch (giRICOH619_DCIN) {
	case CDP_CHARGER:
		sprintf (buf,"CDP\n");
		break;
	case DCP_CHARGER:
		sprintf (buf,"DCP\n");
		break;
	case SDP_PC_CHARGER:
		sprintf (buf,"SDP_PC\n");
		break;
	case SDP_ADPT_CHARGER:
		sprintf (buf,"SDP_ADPT\n");
		break;
	case NO_CHARGER_PLUGGED:
		sprintf (buf,"NO\n");
		break;
	case -1:
		sprintf (buf,"DISABLE\n");
		break;
	default:
		buf[0] = '\0';
		break;
	}
	return strlen(buf);
}

static ssize_t charger_type_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	uint8_t val = 0, ilim_adp = 0, ilim_usb = 0, ichg = 0;
	int iLastChargerType=giRICOH619_DCIN;

	if(0==strcmp(buf,"CDP")) {
		if(0x03==gptHWCFG->m_val.bUIConfig) {
			giRICOH619_DCIN=CDP_CHARGER;
		}
	}
	else if(0==strcmp(buf,"DCP")) {
		if(0x03==gptHWCFG->m_val.bUIConfig) {
			giRICOH619_DCIN=DCP_CHARGER;
		}
	}
	else if(0==strcmp(buf,"SDP_PC")) {
		giRICOH619_DCIN=SDP_PC_CHARGER;
	}
	else if(0==strcmp(buf,"SDP_ADPT")) {
		giRICOH619_DCIN=SDP_ADPT_CHARGER;
	}
	else if(0==strcmp(buf,"NO")) {
		giRICOH619_DCIN=NO_CHARGER_PLUGGED;
	}
	else {
		printk("Wrong type ! only allow [CDP|DCP|SDP_PC|SDP_ADPT|NO|DISABLE] \n");
	}

	if(iLastChargerType!=giRICOH619_DCIN) {
		printk("%s():report DCIN=%d\n",__FUNCTION__,giRICOH619_DCIN?1:0);
		_config_ricoh619_charger_params(dev,giRICOH619_DCIN);
#if 0
		if(mxc_misc_report_usb) {
			mxc_misc_report_usb(giRICOH619_DCIN?1:0);
		}
#endif
	}

	return count;
}

static DEVICE_ATTR(charger_type, S_IRUGO, charger_type_read, charger_type_write);
static int _create_sys_attrs(struct device *dev)
{

	int err;
	err = device_create_file(dev,&dev_attr_charger_type);
	if (err) {
		pr_err("Can't create ricoh ntx attr sysfs !\n");
		return -1;
	}
	return 0;
}

static void _remove_sys_attrs(struct device *dev)
{
	device_remove_file(dev,&dev_attr_charger_type);
}



#ifdef STANDBY_MODE_DEBUG
int multiple_sleep_mode;
#endif

/*This is for full state*/
static int BatteryTableFlageDef=0;
static int BatteryTypeDef=0;
static int Battery_Type(void)
{
	return BatteryTypeDef;
}

static int Battery_Table(void)
{
	switch (gptHWCFG->m_val.bBattery) {
	case 8:  //3000mAh
		BatteryTableFlageDef=1;
		break;
	case 10: //1200mAh
#ifndef ALL_LPTFT_PANEL//[
		if(!NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bEPD_Flags,1)) {
			BatteryTableFlageDef=2;
		}
		else 
#endif //]!ALL_LPTFT_PANEL
		{
			BatteryTableFlageDef=4;
		}
		break;
	case 12: //SP284657-1000mA
		BatteryTableFlageDef=3;
		break;
	case 14: //PR-284983N-1500mA
		BatteryTableFlageDef=5;
		break;
	case 15: //PR-284983N-3000mA
		BatteryTableFlageDef=6;
		break;
	case 16: //EVE254385N-1000mA
		BatteryTableFlageDef=7;
		break;
	default:
		BatteryTableFlageDef=0;
		break;
	}

	return BatteryTableFlageDef;
}

static void ricoh61x_battery_work(struct work_struct *work)
{
	struct ricoh61x_battery_info *info = container_of(work,
		struct ricoh61x_battery_info, monitor_work.work);

//	printk(KERN_INFO "PMU: %s\n", __func__);
	power_supply_changed(info->battery);
	queue_delayed_work(info->monitor_wqueue, &info->monitor_work,
			   info->monitor_time);
}

#define RTC_SEC_REG	0xA0
static void get_current_time(struct ricoh61x_battery_info *info,
				unsigned long *seconds)
{
	struct rtc_time tm;
	u8 buff[7];
	int err;
	int cent_flag;

	/* get_rtc_time(&tm); */
	err = ricoh61x_bulk_reads(info->dev->parent, RTC_SEC_REG, sizeof(buff), buff);
	if (err < 0) {
		dev_err(info->dev, "PMU: %s *** failed to read time *****\n", __func__);
		return;
	}
	if (buff[5] & 0x80)
		cent_flag = 1;
	else
		cent_flag = 0;

	tm.tm_sec  = bcd2bin(buff[0] & 0x7f);
	tm.tm_min  = bcd2bin(buff[1] & 0x7f);
	tm.tm_hour = bcd2bin(buff[2] & 0x3f);	/* 24h */
	tm.tm_wday = bcd2bin(buff[3] & 0x07);
	tm.tm_mday = bcd2bin(buff[4] & 0x3f);
	tm.tm_mon  = bcd2bin(buff[5] & 0x1f) - 1;/* back to system 0-11 */
	tm.tm_year = bcd2bin(buff[6]) + 100 * cent_flag;
	
	dev_dbg(info->dev, "rtc-time : Mon/ Day/ Year H:M:S\n");
	dev_dbg(info->dev, "         : %d/%d/%d %d:%d:%d\n",
		(tm.tm_mon+1), tm.tm_mday, (tm.tm_year + 1900),
		tm.tm_hour, tm.tm_min, tm.tm_sec);
		
	rtc_tm_to_time(&tm, seconds);
}

/**
* Enable test register of Bank1
*
* info : battery info
*
* return value : 
*     true : Removing Protect correctly
*     false : not Removing protect
*/
static bool Enable_Test_Register(struct ricoh61x_battery_info *info){
	int ret;
	uint8_t val = 0x01;
	uint8_t val_backUp;
	uint8_t val2;

	//Remove protect of test register
	ret = ricoh61x_write_bank1(info->dev->parent, BAT_TEST_EN_REG, 0xaa);
	ret += ricoh61x_write_bank1(info->dev->parent, BAT_TEST_EN_REG, 0x55);
	ret += ricoh61x_write_bank1(info->dev->parent, BAT_TEST_EN_REG, 0xaa);
	ret += ricoh61x_write_bank1(info->dev->parent, BAT_TEST_EN_REG, 0x55);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing BAT_TEST_EN_REG\n");
		return false;
	}
	
	//Check protect is removed or not
	ret = ricoh61x_read_bank1(info->dev->parent, BAT_ADD1B2_REG, &val_backUp);
	ret += ricoh61x_write_bank1(info->dev->parent, BAT_ADD1B2_REG, val);
	ret += ricoh61x_read_bank1(info->dev->parent, BAT_ADD1B2_REG, &val2);
	ret += ricoh61x_write_bank1(info->dev->parent, BAT_ADD1B2_REG, val_backUp);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing BAT_ADD1B2_REG\n");
		return false;
	}

	if(val == val2){
		return true;
	} else {
		return false;
	}
	
	return false;
}

/**
* check can write correctly or not
*
* regAddr		: register address
* targetValue	: target value for write
* bank_num		: bank number
*
* return : ture or false
*/
static bool write_and_check_read_back(struct ricoh61x_battery_info *info, u8 regAddr, uint8_t targetValue, int bank_num)
{
	int ret;
	uint8_t val;

	//Check protect is removed or not
	if(bank_num == 0){	
		ret = ricoh61x_write(info->dev->parent, regAddr, targetValue);
		ret += ricoh61x_read(info->dev->parent, regAddr, &val);
		if (ret < 0) {
			dev_err(info->dev, "Error in writing in 0x%d\n",regAddr);
			return false;
		}
	} else {
		ret = ricoh61x_write_bank1(info->dev->parent, regAddr, targetValue);
		ret += ricoh61x_read_bank1(info->dev->parent, regAddr, &val);
		if (ret < 0) {
			dev_err(info->dev, "Error in writing in 0x%d\n",regAddr);
			return false;
		}
	}

	if(targetValue == val){
		return true;
	} else {
		return false;
	}
}
/**
* get stored time from register
* 0xB2 : bit 0 ~ 7
* 0xB3 : bit 8 ~ 15
* 0xDD : bit 16 ~ 23
*
* info : battery info
*
* return sored time unit is hour
*/
static unsigned long get_storedTime_from_register(struct ricoh61x_battery_info *info)
{
	unsigned long hour = 0;
	uint8_t val;
	int ret;
	
	ret = ricoh61x_read_bank1(info->dev->parent, BAT_ADD1B2_REG, &val);
	hour += val;
	ret = ricoh61x_read_bank1(info->dev->parent, BAT_ADD1B3_REG, &val);
	hour += val << 8;
	ret = ricoh61x_read_bank1(info->dev->parent, BAT_ADD1DD_REG, &val);
	hour += val << 16;

	
	return hour;
}


/**
* Set current RTC time to Register. unit is hour
* 0xB2 : bit 0 ~ 7
* 0xB3 : bit 8 ~ 15
* 0xDD : bit 16 ~ 23
*
* info : battery info
*
* return
*/
static void set_current_time2register(struct ricoh61x_battery_info *info)
{
	unsigned long hour;
	unsigned long seconds;
	int loop_counter = 0;
	bool canWriteFlag = true;
	uint8_t val;
	
	//
	get_current_time(info, &seconds);
	hour  = seconds / 3600;
	SUSPEND_PRINT(KERN_DEBUG"PMU : %s : second is %lu, hour is %lu\n",__func__, seconds, hour);

	do{
		val = hour & 0xff;
		canWriteFlag &= write_and_check_read_back(info, BAT_ADD1B2_REG, val, 1);
		val = (hour >> 8) & 0xff;
		canWriteFlag &= write_and_check_read_back(info, BAT_ADD1B3_REG, val, 1);
		val = (hour >> 16) & 0xff;
		canWriteFlag &= write_and_check_read_back(info, BAT_ADD1DD_REG, val, 1);

		if(canWriteFlag != true){
			Enable_Test_Register(info);
			loop_counter++;
		}

		//read back
		if(loop_counter > 5){
			canWriteFlag = true;
		}
	}while(canWriteFlag == false);

	return;
}

static int measure_vbatt_ADC(struct ricoh61x_battery_info *info, int *data);
#ifdef ENABLE_FUEL_GAUGE_FUNCTION
static int measure_vbatt_FG(struct ricoh61x_battery_info *info, int *data);
static int measure_Ibatt_FG(struct ricoh61x_battery_info *info, int *data);
static int calc_capacity(struct ricoh61x_battery_info *info);
static int calc_capacity_2(struct ricoh61x_battery_info *info);
static int get_OCV_init_Data(struct ricoh61x_battery_info *info, int index, int table_num);
static int get_OCV_voltage(struct ricoh61x_battery_info *info, int index, int table_num);
static int get_check_fuel_gauge_reg(struct ricoh61x_battery_info *info,
					 int Reg_h, int Reg_l, int enable_bit);
static int calc_capacity_in_period(struct ricoh61x_battery_info *info,
				 int *cc_cap, long *cc_cap_mas, bool *is_charging, int cc_rst);
static int get_charge_priority(struct ricoh61x_battery_info *info, bool *data);
static int set_charge_priority(struct ricoh61x_battery_info *info, bool *data);
static int get_power_supply_status(struct ricoh61x_battery_info *info);
static int get_power_supply_Android_status(struct ricoh61x_battery_info *info);
static int measure_vsys_ADC(struct ricoh61x_battery_info *info, int *data);
static int Calc_Linear_Interpolation(int x0, int y0, int x1, int y1, int y);
static int get_battery_temp(struct ricoh61x_battery_info *info);
static int get_battery_temp_2(struct ricoh61x_battery_info *info);
static int check_jeita_status(struct ricoh61x_battery_info *info, bool *is_jeita_updated);
static void ricoh61x_scaling_OCV_table(struct ricoh61x_battery_info *info, int cutoff_vol, int full_vol, int *start_per, int *end_per);
static int ricoh61x_Check_OCV_Offset(struct ricoh61x_battery_info *info);
static void mainFlowOfLowVoltage(struct ricoh61x_battery_info *info);
static void initSettingOfLowVoltage(struct ricoh61x_battery_info *info);
static int getCapFromOriTable(struct ricoh61x_battery_info *info, int voltage, int currentvalue, int resvalue);
static int getCapFromOriTable_U10per(struct ricoh61x_battery_info *info, int voltage, int currentvalue, int resvalue);

static int calc_ocv(struct ricoh61x_battery_info *info)
{
	int Vbat = 0;
	int Ibat = 0;
	int ret;
	int ocv;

	ret = measure_vbatt_FG(info, &Vbat);
	ret = measure_Ibatt_FG(info, &Ibat);

	ocv = Vbat - Ibat * info->soca->Rbat;

	return ocv;
}

static int calc_soc_on_ocv(struct ricoh61x_battery_info *info, int Ocv)
{
	int i;
	int capacity=0;

	/* capacity is 0.01% unit */
	if (info->soca->ocv_table[0] >= Ocv) {
		capacity = 1 * 100;
	} else if (info->soca->ocv_table[10] <= Ocv) {
		capacity = 100 * 100;
	} else {
		for (i = 1; i < 11; i++) {
			if (info->soca->ocv_table[i] >= Ocv) {
				/* unit is 0.01% */
				capacity = Calc_Linear_Interpolation(
					(i-1)*10 * 100, info->soca->ocv_table[i-1], i*10 * 100,
					 info->soca->ocv_table[i], Ocv);
				if(capacity < 100){
					capacity = 100;
				}
				break;
			}
		}
	}

	printk(KERN_INFO "PMU: %s capacity(%d)\n",
			 __func__, capacity);

	return capacity;
}

static int set_Rlow(struct ricoh61x_battery_info *info)
{
	int err;
	int Rbat_low_max;
	uint8_t val;
	int Vocv;
	int temp;

	if (info->soca->Rbat == 0)
			info->soca->Rbat = get_OCV_init_Data(info, 12, USING) * 1000 / 512
							 * 5000 / 4095;
	
	Vocv = calc_ocv(info);
	Rbat_low_max = info->soca->Rbat * 1.5;

	if (Vocv < get_OCV_voltage(info,3, USING))
	{
		info->soca->R_low = Calc_Linear_Interpolation(info->soca->Rbat,get_OCV_voltage(info,3,USING),
			Rbat_low_max, get_OCV_voltage(info,0,USING), Vocv);
#ifdef	_RICOH619_DEBUG_
		printk(KERN_DEBUG"PMU: Modify RBAT from %d to %d ", info->soca->Rbat, info->soca->R_low);
#endif
		temp = info->soca->R_low *4095/5000*512/1000;
		
		val = temp >> 8;
		err = ricoh61x_write_bank1(info->dev->parent, 0xD4, val);
		if (err < 0) {
			dev_err(info->dev, "batterry initialize error\n");
			return err;
		}

		val = info->soca->R_low & 0xff;
		err = ricoh61x_write_bank1(info->dev->parent, 0xD5, val);
		if (err < 0) {
			dev_err(info->dev, "batterry initialize error\n");
			return err;
		}
	}
	else  info->soca->R_low = 0;
		

	return err;
}

static int Set_back_ocv_table(struct ricoh61x_battery_info *info)
{
	int err;
	uint8_t val;
	int temp;
	int i;
	uint8_t debug_disp[22];

	/* Modify back ocv table */

	if (0 != info->soca->ocv_table_low[0])
	{
		for (i = 0 ; i < 11; i++){
			battery_init_para[info->num][i*2 + 1] = info->soca->ocv_table_low[i];
			battery_init_para[info->num][i*2] = info->soca->ocv_table_low[i] >> 8;
		}
		err = ricoh61x_clr_bits(info->dev->parent, FG_CTRL_REG, 0x01);

		err = ricoh61x_bulk_writes_bank1(info->dev->parent,
			BAT_INIT_TOP_REG, 22, battery_init_para[info->num]);

		err = ricoh61x_set_bits(info->dev->parent, FG_CTRL_REG, 0x01);

		/* debug comment start*/
		err = ricoh61x_bulk_reads_bank1(info->dev->parent,
			BAT_INIT_TOP_REG, 22, debug_disp);
		for (i = 0; i < 11; i++){
			printk("PMU : %s : after OCV table %d 0x%x\n",__func__, i * 10, (debug_disp[i*2] << 8 | debug_disp[i*2+1]));
		}
		/* end */
		/* clear table*/
		for(i = 0; i < 11; i++)
		{
			info->soca->ocv_table_low[i] = 0;
		}
	}
	
	/* Modify back Rbat */
	if (0!=info->soca->R_low)
	{		
		printk("PMU: Modify back RBAT from %d to %d ",  info->soca->R_low,info->soca->Rbat);
		temp = info->soca->Rbat*4095/5000*512/1000;
		
		val = temp >> 8;
		err = ricoh61x_write_bank1(info->dev->parent, 0xD4, val);
		if (err < 0) {
			dev_err(info->dev, "batterry initialize error\n");
			return err;
		}

		val = info->soca->R_low & 0xff;
		err = ricoh61x_write_bank1(info->dev->parent, 0xD5, val);
		if (err < 0) {
			dev_err(info->dev, "batterry initialize error\n");
			return err;
		}

		info->soca->R_low = 0;
	}
	return 0;
}

/**
**/
static int ricoh61x_Check_OCV_Offset(struct ricoh61x_battery_info *info)
{
	int ocv_table[11]; // HEX value
	int i;
	int temp;
	int ret;
	uint8_t debug_disp[22];
	uint8_t val = 0;

	printk("PMU : %s : calc ocv %d get OCV %d\n",__func__,calc_ocv(info),get_OCV_voltage(info, RICOH61x_OCV_OFFSET_BOUND,USING));

	/* check adp/usb status */
	ret = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &val);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		return 0;
	}

	val = (val & 0xC0) >> 6;

	if (val != 0){ /* connect adp or usb */
		if (calc_ocv(info) < get_OCV_voltage(info, RICOH61x_OCV_OFFSET_BOUND,USING) )
		{
			if(0 == info->soca->ocv_table_low[0]){
				for (i = 0 ; i < 11; i++){
				ocv_table[i] = (battery_init_para[info->num][i*2]<<8) | (battery_init_para[info->num][i*2+1]);
				printk("PMU : %s : OCV table %d 0x%x\n",__func__,i * 10, ocv_table[i]);
				info->soca->ocv_table_low[i] = ocv_table[i];
				}

				for (i = 0 ; i < 11; i++){
					temp = ocv_table[i] * (100 + RICOH61x_OCV_OFFSET_RATIO) / 100;

					battery_init_para[info->num][i*2 + 1] = temp;
					battery_init_para[info->num][i*2] = temp >> 8;
				}
				ret = ricoh61x_clr_bits(info->dev->parent, FG_CTRL_REG, 0x01);

				ret = ricoh61x_bulk_writes_bank1(info->dev->parent,
					BAT_INIT_TOP_REG, 22, battery_init_para[info->num]);

				ret = ricoh61x_set_bits(info->dev->parent, FG_CTRL_REG, 0x01);

				/* debug comment start*/
				ret = ricoh61x_bulk_reads_bank1(info->dev->parent,
					BAT_INIT_TOP_REG, 22, debug_disp);
				for (i = 0; i < 11; i++){
					printk("PMU : %s : after OCV table %d 0x%x\n",__func__, i * 10, (debug_disp[i*2] << 8 | debug_disp[i*2+1]));
				}
				/* end */
			}
		}
	}
	
	return 0;
}

static int reset_FG_process(struct ricoh61x_battery_info *info)
{
	int err;

	//err = set_Rlow(info);
	//err = ricoh61x_Check_OCV_Offset(info);
	err = ricoh61x_write(info->dev->parent,
					 FG_CTRL_REG, 0x51);
	info->soca->ready_fg = 0;
	info->soca->rsoc_ready_flag = 1;

	return err;
}


static int check_charge_status_2(struct ricoh61x_battery_info *info, int displayed_soc_temp)
{
	if (displayed_soc_temp < 0)
			displayed_soc_temp = 0;
	
	get_power_supply_status(info);
	info->soca->soc = calc_capacity(info) * 100;

	if (POWER_SUPPLY_STATUS_FULL == info->soca->chg_status) {
		if ((info->first_pwon == 1)
			&& (RICOH61x_SOCA_START == info->soca->status)) {
				g_full_flag = 1;
				info->soca->soc_full = info->soca->soc;
				info->soca->displayed_soc = 100*100;
				info->soca->full_reset_count = 0;
		} else {
			if (calc_ocv(info) > get_OCV_voltage(info, 9,USING)){
				g_full_flag = 1;
				info->soca->soc_full = info->soca->soc;
				info->soca->displayed_soc = 100*100;
				info->soca->full_reset_count = 0;
			} else {
				g_full_flag = 0;
				info->soca->displayed_soc = displayed_soc_temp;
				SUSPEND_PRINT(KERN_INFO "PMU: %s Charge Complete but OCV is low\n", __func__);
			}

		}
	}
	if (info->soca->Ibat_ave >= 0) {
		if (g_full_flag == 1) {
			info->soca->displayed_soc = 100*100;
		} else {
			info->soca->displayed_soc = min(9949, displayed_soc_temp);
		}
	}
	if (info->soca->Ibat_ave < 0) {
		if (g_full_flag == 1) {
			if (calc_ocv(info) < get_OCV_voltage(info, 9, USING) + (get_OCV_voltage(info, 10, USING) - get_OCV_voltage(info, 9, USING))*7/10) {
				g_full_flag = 0;
				//info->soca->displayed_soc = 100*100;
				info->soca->displayed_soc = displayed_soc_temp;
				SUSPEND_PRINT(KERN_INFO "PMU: %s g_full_flag=1 but OCV is low\n", __func__);
			} else {
				info->soca->displayed_soc = 100*100;
			}
		} else {
			info->soca->displayed_soc = min(9949, displayed_soc_temp);
			g_full_flag = 0;
		}
	}
	if (RICOH61x_SOCA_START == info->soca->status) {
		if ((g_full_flag == 1) && (calc_ocv(info) > get_OCV_voltage(info, 9,USING))){
			info->soca->soc_full = info->soca->soc;
			info->soca->displayed_soc = 100*100;
			info->soca->full_reset_count = 0;
			SUSPEND_PRINT(KERN_INFO "PMU:%s Charge Complete in PowerOff\n", __func__);
		} else if ((info->first_pwon == 0)
			&& !g_fg_on_mode) {
			SUSPEND_PRINT(KERN_INFO "PMU:%s 2nd P-On init_pswr(%d), cc(%d)\n",
				__func__, info->soca->init_pswr, info->soca->cc_delta);
			if ((info->soca->init_pswr == 100)
				&& (info->soca->cc_delta > -100)) {
				SUSPEND_PRINT(KERN_INFO "PMU:%s Set 100%%\n", __func__);
				g_full_flag = 1;
				info->soca->soc_full = info->soca->soc;
				info->soca->displayed_soc = 100*100;
				info->soca->full_reset_count = 0;
			}
		}
	} else {
		SUSPEND_PRINT(KERN_DEBUG "PMU:%s Resume Sus_soc(%d), cc(%d)\n",
			__func__, info->soca->suspend_soc, info->soca->cc_delta);
		if ((info->soca->suspend_soc == 10000)
			&& (info->soca->cc_delta > -100)) {
			SUSPEND_PRINT(KERN_INFO "PMU:%s Set 100%%\n", __func__);
			info->soca->displayed_soc = 100*100;
		}
	}

	return info->soca->displayed_soc;
}

/**
* Calculate Capacity in a period
* - read CC_SUM & FA_CAP from Coulom Counter
* -  and calculate Capacity.
* @cc_cap: capacity in a period, unit 0.01%
* @cc_cap_mas : capacity in a period, unit 1mAs
* @is_charging: Flag of charging current direction
*               TRUE : charging (plus)
*               FALSE: discharging (minus)
* @cc_rst: reset CC_SUM or not
*               0 : not reset
*               1 : reset
*               2 : half reset (Leave under 1% of FACAP)
**/
static int calc_capacity_in_period(struct ricoh61x_battery_info *info,
				 int *cc_cap, long *cc_cap_mas, bool *is_charging, int cc_rst)
{
	int err;
	uint8_t 	cc_sum_reg[4];
	uint8_t 	cc_clr[4] = {0, 0, 0, 0};
	uint8_t 	fa_cap_reg[2];
	uint16_t 	fa_cap;
	uint32_t 	cc_sum;
	int		cc_stop_flag;
	uint8_t 	status;
	uint8_t 	charge_state;
	int 		Ocv;
	uint32_t 	cc_cap_temp;
	uint32_t 	cc_cap_min;
	int		cc_cap_res;
	int		fa_cap_int;
	long		cc_sum_int;
	long		cc_sum_dec;

	*is_charging = true;	/* currrent state initialize -> charging */

	if (info->entry_factory_mode)
		return 0;

	/* Read FA_CAP */
	err = ricoh61x_bulk_reads(info->dev->parent,
				 FA_CAP_H_REG, 2, fa_cap_reg);
	if (err < 0)
		goto out;

	/* fa_cap = *(uint16_t*)fa_cap_reg & 0x7fff; */
	fa_cap = (fa_cap_reg[0] << 8 | fa_cap_reg[1]) & 0x7fff;


	/* get  power supply status */
	err = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &status);
	if (err < 0)
		goto out;
	charge_state = (status & 0x1F);
	Ocv = calc_ocv(info);
	if (charge_state == CHG_STATE_CHG_COMPLETE) {
		/* Check CHG status is complete or not */
		cc_stop_flag = 0;
//	} else if (calc_capacity(info) == 100) {
//		/* Check HW soc is 100 or not */
//		cc_stop_flag = 0;
	} else if (Ocv < get_OCV_voltage(info, 9, USING)) {
		/* Check VBAT is high level or not */
		cc_stop_flag = 0;
	} else {
		cc_stop_flag = 1;
	}

	if (cc_stop_flag == 1)
	{
		/* Disable Charging/Completion Interrupt */
		err = ricoh61x_set_bits(info->dev->parent,
						RICOH61x_INT_MSK_CHGSTS1, 0x01);
		if (err < 0)
			goto out;

		/* disable charging */
		err = ricoh61x_clr_bits(info->dev->parent, RICOH61x_CHG_CTL1, 0x03);
		if (err < 0)
			goto out;
	}

	/* Read CC_SUM */
	err = ricoh61x_bulk_reads(info->dev->parent,
					CC_SUMREG3_REG, 4, cc_sum_reg);
	if (err < 0)
		goto out;

	/* cc_sum = *(uint32_t*)cc_sum_reg; */
	cc_sum = cc_sum_reg[0] << 24 | cc_sum_reg[1] << 16 |
				cc_sum_reg[2] << 8 | cc_sum_reg[3];

	/* calculation  two's complement of CC_SUM */
	if (cc_sum & 0x80000000) {
		cc_sum = (cc_sum^0xffffffff)+0x01;
		*is_charging = false;		/* discharge */
	}

	if (cc_rst == 1) {
		/* CC_pause enter */
		err = ricoh61x_write(info->dev->parent, CC_CTRL_REG, 0x01);
		if (err < 0)
			goto out;

		/* CC_SUM <- 0 */
		err = ricoh61x_bulk_writes(info->dev->parent,
						CC_SUMREG3_REG, 4, cc_clr);
		if (err < 0)
			goto out;
	} else if (cc_rst == 2) {
		/* Check 1%[mAs] of FA_CAP (FA_CAP * 3600 /100) */
		fa_cap_int = fa_cap * 36;
		cc_sum_int = cc_sum / fa_cap_int;
		cc_sum_dec = cc_sum % fa_cap_int;

		if (*is_charging == false) {
			cc_sum_dec = (cc_sum_dec^0xffffffff) + 1;
		}
		SUSPEND_PRINT(KERN_DEBUG "PMU %s 1%%FACAP(%d)[mAs], cc_sum(%d)[mAs], cc_sum_dec(%d)\n",
			 __func__, fa_cap_int, cc_sum, cc_sum_dec);

		if (cc_sum_int != 0) {
			cc_clr[0] = (uint8_t)(cc_sum_dec >> 24) & 0xff;
			cc_clr[1] = (uint8_t)(cc_sum_dec >> 16) & 0xff;
			cc_clr[2] = (uint8_t)(cc_sum_dec >> 8) & 0xff;
			cc_clr[3] = (uint8_t)cc_sum_dec & 0xff;

			/* CC_pause enter */
			err = ricoh61x_write(info->dev->parent, CC_CTRL_REG, 0x01);
			if (err < 0)
				goto out;

			/* CC_SUM <- 0 */
			err = ricoh61x_bulk_writes(info->dev->parent,
							CC_SUMREG3_REG, 4, cc_clr);
			if (err < 0)
				goto out;
			SUSPEND_PRINT(KERN_INFO "PMU %s Half-Clear CC, cc_sum is over 1%%\n",
				 __func__);
		}
	}

	/* CC_pause exist */
	err = ricoh61x_write(info->dev->parent, CC_CTRL_REG, 0);
	if (err < 0)
		goto out;
	if (cc_stop_flag == 1)
	{
	
		/* Enable charging */
		err = ricoh61x_set_bits(info->dev->parent, RICOH61x_CHG_CTL1, 0x03);
		if (err < 0)
			goto out;

		udelay(1000);

		/* Clear Charging Interrupt status */
		err = ricoh61x_clr_bits(info->dev->parent,
					RICOH61x_INT_IR_CHGSTS1, 0x01);
		if (err < 0)
			goto out;

		/* ricoh61x_read(info->dev->parent, RICOH61x_INT_IR_CHGSTS1, &val);
//		SUSPEND_PRINT("INT_IR_CHGSTS1 = 0x%x\n",val); */

		/* Enable Charging Interrupt */
		err = ricoh61x_clr_bits(info->dev->parent,
						RICOH61x_INT_MSK_CHGSTS1, 0x01);
		if (err < 0)
			goto out;
	}

	/* (CC_SUM x 10000)/3600/FA_CAP */

	if(fa_cap == 0)
		goto out;
	else
		*cc_cap = cc_sum*25/9/fa_cap;		/* unit is 0.01% */

	*cc_cap_mas = cc_sum;

	//SUSPEND_PRINT("PMU: cc_sum = %d: cc_cap= %d: cc_cap_mas = %d\n", cc_sum, *cc_cap, *cc_cap_mas);
	
	if (cc_rst == 1) {
		cc_cap_min = fa_cap*3600/100/100/100;	/* Unit is 0.0001% */

		if(cc_cap_min == 0)
			goto out;
		else
			cc_cap_temp = cc_sum / cc_cap_min;

		cc_cap_res = cc_cap_temp % 100;

#ifdef	_RICOH619_DEBUG_
		SUSPEND_PRINT(KERN_DEBUG"PMU: cc_sum = %d: cc_cap_res= %d: cc_cap_mas = %d\n", cc_sum, cc_cap_res, cc_cap_mas);
#endif

		if(*is_charging) {
			info->soca->cc_cap_offset += cc_cap_res;
			if (info->soca->cc_cap_offset >= 100) {
				*cc_cap += 1;
				info->soca->cc_cap_offset %= 100;
			}
		} else {
			info->soca->cc_cap_offset -= cc_cap_res;
			if (info->soca->cc_cap_offset <= -100) {
				*cc_cap += 1;
				info->soca->cc_cap_offset %= 100;
			}
		}
#ifdef	_RICOH619_DEBUG_
		SUSPEND_PRINT(KERN_DEBUG"PMU: cc_cap_offset= %d: \n", info->soca->cc_cap_offset);
#endif
	} else {
		info->soca->cc_cap_offset = 0;
	}
	
	//////////////////////////////////////////////////////////////////
	return 0;
out:
	/* CC_pause exist */
	err = ricoh61x_write(info->dev->parent, CC_CTRL_REG, 0);

	dev_err(info->dev, "Error !!-----\n");
	return err;
}

/**
* Initial setting of Low voltage.
**/
static void initSettingOfLowVoltage(struct ricoh61x_battery_info *info)
{
	int err;
	int cc_cap;
	long cc_cap_mas;
	bool is_charging = true;


	if(info->soca->rsoc_ready_flag ==1) {
		err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 1);
		info->soca->last_cc_delta_cap = 0;
	} else {
		err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 0);
		info->soca->last_cc_delta_cap = (is_charging == true) ? cc_cap : -cc_cap;
	}
	if (err < 0)
		dev_err(info->dev, "Read cc_sum Error !!-----\n");

	return;
}

/**
* Low voltage main flow.
**/
static void mainFlowOfLowVoltage(struct ricoh61x_battery_info *info)
{
	int ret = 0;
	int cc_cap = 0;
	long cc_cap_mas = 0;
	bool is_charging = true;
	int cc_delta_cap;
	int cc_delta_cap_temp;
	int cc_delta_cap_mas_temp;
	int cc_delta_cap_now;
	int cc_delta_cap_debug; //for debug value
	int capacity_now;		//Unit is 0.01 %
	int capacity_zero;		//Unit is 0.01 %
	int capacity_remain;	//Unit is 0.01 %
	int low_rate;			//Unit is 0.01 times
	int target_equal_soc;	//unit is 0.01 %
	int temp_cc_delta_cap;	//unit is 0.01 %
	int fa_cap;				//unit is mAh

	if(info->soca->rsoc_ready_flag ==1) {
		ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 1);
	} else {
		ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 0);
	}
	if (ret < 0)
		dev_err(info->dev, "Read cc_sum Error !!-----\n");

	if(is_charging == true) {
		cc_delta_cap_now = cc_cap;
		//cc_cap_mas;
	} else {
		cc_delta_cap_now = -cc_cap;
		cc_cap_mas = -cc_cap_mas;
	}

	fa_cap = (battery_init_para[info->num][22]<<8)
					 | (battery_init_para[info->num][23]);
	
	if(fa_cap != 0) {
		//( cc(mas) * 10000 ) / 3600 / fa_cap
		temp_cc_delta_cap = info->soca->temp_cc_delta_cap_mas * 25 / 9 / fa_cap;
	} else {
		temp_cc_delta_cap = 0;
	}
	
	cc_delta_cap = (cc_delta_cap_now - info->soca->last_cc_delta_cap) + temp_cc_delta_cap;
	
//	info->soca->temp_cc_delta_cap_mas = info->soca->temp_cc_delta_cap_mas - ( (fa_cap * 3600) * (temp_cc_delta_cap / 10000)) ;
	info->soca->temp_cc_delta_cap_mas = info->soca->temp_cc_delta_cap_mas - ( ((fa_cap * 9) / 25) * temp_cc_delta_cap);

	printk(KERN_DEBUG "PMU: %s :	Noxx : cc_delta_cap is %d, cc_delta_cap_now is %d, last_cc_delta_cap is %d\n"
		, __func__, cc_delta_cap, cc_delta_cap_now, info->soca->last_cc_delta_cap);
	printk(KERN_DEBUG "PMU: %s :	Noxx : temp_cc_delta_cap is %d, after temp_cc_delta_cap_mas is %ld, cc_cap_mas %ld\n"
		, __func__,  temp_cc_delta_cap ,info->soca->temp_cc_delta_cap_mas, cc_cap_mas);

	if(info->soca->rsoc_ready_flag ==1) {
		info->soca->last_cc_delta_cap = 0;
		info->soca->last_cc_delta_cap_mas = 0;
	} else {
		info->soca->last_cc_delta_cap = cc_delta_cap_now;
		info->soca->last_cc_delta_cap_mas = cc_cap_mas;
	}

	cc_delta_cap_debug = cc_delta_cap;

	// check charging or not, if charging -> move to Disp state
	if ((cc_delta_cap > 0) ||
		(info->soca->Ibat_ave >= 0)){//chekc discharging or not
		info->soca->soc = calc_capacity(info) * 100;
		info->soca->status = RICOH61x_SOCA_DISP;
		info->soca->last_soc = info->soca->soc;
		info->soca->soc_delta = 0;
		info->soca->hurry_up_flg = 0;
		info->soca->temp_cc_delta_cap_mas = 0;
		return;
	}

	//check Vbat and POff_vbat
	if(info->soca->Vbat_ave <= (info->fg_poff_vbat * 1000)) {
		info->soca->displayed_soc = info->soca->displayed_soc - 100;
		info->soca->displayed_soc = max(0, info->soca->displayed_soc);
		info->soca->hurry_up_flg = 1;
		return;
	}

	//calc current recap value
	capacity_now = getCapFromOriTable(info,info->soca->Vbat_ave,info->soca->Ibat_ave,info->soca->Rbat);

	//calc recap value when soc is 0%
	if(info->fg_poff_vbat != 0){
		//enable poff vbat
		capacity_zero = getCapFromOriTable(info,(info->fg_poff_vbat * 1000),info->soca->Ibat_ave,info->soca->Rbat);
	} else if(info->fg_target_vsys != 0){
		//enable target vsys
		capacity_zero = getCapFromOriTable(info,(info->fg_target_vsys * 1000),info->soca->Ibat_ave,info->soca->Rsys);
	} else {
		//disable poff vbat and target vsys
		capacity_zero = 0;
	}

	capacity_remain = (capacity_now - capacity_zero) + 50;

	if (capacity_remain <= 50){
		printk(KERN_INFO "PMU: %s : No6 :Hurry up!!! \n", __func__);
		info->soca->displayed_soc = info->soca->displayed_soc - 100;
		info->soca->displayed_soc = max(0, info->soca->displayed_soc);
		info->soca->hurry_up_flg = 1;
		return;
	}
	else {
		info->soca->hurry_up_flg = 0;

		if (info->soca->displayed_soc < 1000) { //low DSOC case
			if(capacity_remain > info->soca->displayed_soc){
				target_equal_soc = info->soca->displayed_soc * 95 / 100;
			} else {
				target_equal_soc = 50;
			}
		} else {// normal case
			if(capacity_remain > info->soca->displayed_soc){
				target_equal_soc = info->soca->displayed_soc - 1000;
			} else {
				target_equal_soc = capacity_remain - 1000;
			}
		}

		target_equal_soc = max(50, target_equal_soc);

		low_rate = (info->soca->displayed_soc - target_equal_soc) * 100 / (capacity_remain - target_equal_soc);

		low_rate = max(1, low_rate);
		low_rate = min(300, low_rate);

		cc_delta_cap_temp = cc_delta_cap * 100 * low_rate / 100;					//unit is 0.0001%

		if(cc_delta_cap_temp < 0){
			//Unit 0.0001 -> 0.01
			cc_delta_cap = cc_delta_cap_temp / 100;

			cc_delta_cap_temp = cc_delta_cap_temp - cc_delta_cap * 100;
			//transform 0.0001% -> mAs
			//mAs = 0.0001 % * (fa_cap(mAh)*60*60)
			//mAs = cc_delta_cap_temp * (fa_cap * 60 * 60) / (100 * 100 * 100)
			cc_delta_cap_mas_temp = cc_delta_cap_temp * fa_cap * 9 / 2500;
			info->soca->temp_cc_delta_cap_mas += cc_delta_cap_mas_temp;
		}else{
			cc_delta_cap = 0;
		}

		info->soca->displayed_soc = info->soca->displayed_soc + cc_delta_cap;
		info->soca->displayed_soc = max(100, info->soca->displayed_soc); //Set Under limit DSOC is 1%
		printk(KERN_DEBUG "PMU: %s :	No9 :Cap is %d , low_rate is %d, dsoc is %d, capnow is %d, capzero is %d, delta cc is %d, delta cc ori is %d\n"
			, __func__, capacity_remain, low_rate, info->soca->displayed_soc, capacity_now, capacity_zero, cc_delta_cap, cc_delta_cap_debug);
		printk(KERN_DEBUG "PMU: %s :	No10 :temp_mas is %d, offset_mas is %d, value is %d, final value is %d\n"
			, __func__, info->soca->temp_cc_delta_cap_mas, cc_delta_cap_mas_temp,(cc_delta_cap_temp + cc_delta_cap * 100), cc_delta_cap);
	}
	return;
}

/**
* get capacity from Original OCV Table. this value is calculted by ocv
* info : battery info
* voltage : unit is 1mV
* current : unit is 1mA
* resvalue: unit is 1mohm
*
* return value : capcaity, unit is 0.01%
*/
static int getCapFromOriTable(struct ricoh61x_battery_info *info, int voltage, int currentvalue, int resvalue)
{
	int ocv = 0;
	int i =0;
	int capacity=0;

	int ocv_table[11];

	ocv = voltage - (currentvalue * resvalue);

	//get ocv table from header file
	for(i = 0; i < 11; i++){
		ocv_table[i] = get_OCV_voltage(info, i, ORIGINAL);
	}
	
	/* capacity is 0.01% unit */
	if (ocv_table[10] <= ocv) {
		capacity = 100 * 100;
	} else {
		for (i = 1; i < 11; i++) {
			if (ocv_table[i] >= ocv) {
				if(i == 1){//Under 10 %
					capacity = getCapFromOriTable_U10per(info, voltage, currentvalue, resvalue);
				}else{
					/* unit is 0.01% */
					capacity = Calc_Linear_Interpolation(
						(i-1)*10 * 100, ocv_table[i-1], i*10 * 100,
						 ocv_table[i], ocv);
					if(capacity < 100){
						capacity = 100;
					}
				}
				break;
			}
		}
	}
	return capacity;
}

/**
* get capacity from special OCV Table(10%-0%). this value is calculted by ocv
* info : battery info
* voltage : unit is 1mV
* current : unit is 1mA
* resvalue: unit is 1mohm
*
* return value : capcaity, unit is 0.01%
*/
static int getCapFromOriTable_U10per(struct ricoh61x_battery_info *info, int voltage, int currentvalue, int resvalue)
{
	int ocv = 0;
	int i =0;
	int capacity=0;

	int ocv_table_for_1200mAh[11] = {	3760971,
							3762013,
							3763979,
							3765679,
							3767413,
							3770971,
							3773546,
							3775474,
							3777529,
							3778813,
							3779815};
	int ocv_table_regular[11] = {   3518300,
							3572600,
							3615300,
							3647400,
							3663300,
							3670000,
							3673900,
							3674525,
							3674550,
							3677300,
							3678800};
	int ocv_table_for_1200mAh_coff36[11] = {	3602154,
 							3640621,
 							3670068,
 							3680034,
 							3683734,
 							3685768,
 							3689288,
 							3692088,
 							3695388,
 							3698221,
 							3701000};
	int ocv_table_for_1500mAh[11] = {	
							3535700,
							3617900,
							3657600,
							3677200,
							3678000,
							3679900,
							3684400,
							3686900,
							3688100,
							3689300,
 							3688100};
	int ocv_table_for_3000mAh_coff36[11] = {	3602154,
 							3640621,
 							3670068,
 							3680034,
 							3683734,
 							3685768,
 							3689288,
 							3692088,
 							3695388,
 							3698221,
 							3701000};
	int ocv_table_for_1000mAh[11] = {
							3375294,
                                                        3425013,
                                                        3454313,
                                                        3475113,
                                                        3488513,
                                                        3499513,
                                                        3509313,
                                                        3519113,
                                                        3531313,
                                                        3543513,
                                                        3556913
	};


	int *ocv_table;
	if(10 == gptHWCFG->m_val.bBattery) {// 1200mAh battery
#ifndef ALL_LPTFT_PANEL
		/*
		 * 新機型不再會有非LPTFT .
		 */
		if(!NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bEPD_Flags,1)) 
		{
			ocv_table = ocv_table_for_1200mAh;
		}
		else 
#endif //] !ALL_LPTFT_PANEL
		{
			// cut off @ 3.6V for LPTFT .
			ocv_table = ocv_table_for_1200mAh_coff36;
		}
	}
	else if (14 == gptHWCFG->m_val.bBattery)		// PR-284983N 1500mAH
			ocv_table = ocv_table_for_1500mAh;
	else if (15 == gptHWCFG->m_val.bBattery)		// PR-248899G-3000mA
			ocv_table = ocv_table_for_3000mAh_coff36;
	else if (16 == gptHWCFG->m_val.bBattery)
			ocv_table = ocv_table_for_1000mAh;
	else {
		ocv_table = ocv_table_regular;
	}

	ocv = voltage - (currentvalue * resvalue);

	/* capacity is 0.01% unit */
	if (ocv_table[0] >= ocv) {
		capacity = 0;
	} else if (ocv_table[10] <= ocv) {
		capacity = 10 * 100;
	} else {
		for (i = 1; i < 11; i++) {
			if (ocv_table[i] >= ocv) {
				/* unit is 0.01% */
				capacity = Calc_Linear_Interpolation(
					(i-1) * 100, ocv_table[i-1], i * 100,
					 ocv_table[i], ocv);
				if(capacity < 0){
					capacity = 0;
				}
				break;
			}
		}
	}
	return capacity;

}

/**
* ReWrite extra CC Value to CC_SUM(register)
* info : battery info
* extraValue : Under 1% value. unit is 0.01%
*
* return value : delta soc, unit is "minus" 0.01%
*/

static void write_extra_value_to_ccsum(struct ricoh61x_battery_info *info, int extraValue)
{
	int err;
	uint8_t 	cc_clr[4] = {0, 0, 0, 0}; //temporary box
	uint8_t 	fa_cap_reg[2]; //reg value
	int fa_cap;		//Unit is mAh
	int cc_sum_dec; //unit is mAs
	bool is_charging = 0;

	//check dicharging or not
	if(extraValue < 0){
		extraValue = extraValue * -1;
		is_charging = false;
	} else {
		is_charging = true;
	}

	/* Read FA_CAP */
	err = ricoh61x_bulk_reads(info->dev->parent,
				 FA_CAP_H_REG, 2, fa_cap_reg);
	if (err < 0)
		dev_err(info->dev, "Read fa_cap Error !!-----\n");

	/* fa_cap = *(uint16_t*)fa_cap_reg & 0x7fff; */
	fa_cap = (fa_cap_reg[0] << 8 | fa_cap_reg[1]) & 0x7fff;

	//convertion extraValue(0.01%) -> mAs
	//cc_sum_dec = (extraValue * fa_cap * 3600) / (100 * 100)
	cc_sum_dec = (extraValue * fa_cap * 9) / 25;

	// Add 0.005%
	if (extraValue < 100) {
		cc_sum_dec += (1 * fa_cap * 9) / 25;
	}

	if (is_charging == false) {
		cc_sum_dec = (cc_sum_dec^0xffffffff) + 1;
	}

	cc_clr[0] = (uint8_t)(cc_sum_dec >> 24) & 0xff;
	cc_clr[1] = (uint8_t)(cc_sum_dec >> 16) & 0xff;
	cc_clr[2] = (uint8_t)(cc_sum_dec >> 8) & 0xff;
	cc_clr[3] = (uint8_t)cc_sum_dec & 0xff;

	/* CC_pause enter */
	err = ricoh61x_write(info->dev->parent, CC_CTRL_REG, 0x01);
	if (err < 0)
		dev_err(info->dev, "Write cc_CTRL Error !!-----\n");

	/* CC_SUM <- 0 */
	err = ricoh61x_bulk_writes(info->dev->parent,
					CC_SUMREG3_REG, 4, cc_clr);
	if (err < 0)
		dev_err(info->dev, "Write cc_Sum Error !!-----\n");

	/* CC_pause exit */
	err = ricoh61x_write(info->dev->parent, CC_CTRL_REG, 0);
	if (err < 0)
		dev_err(info->dev, "Write cc_CTRL Error !!-----\n");

	return;
}


#ifdef ENABLE_OCV_TABLE_CALIB
/**
* Calibration OCV Table
* - Update the value of VBAT on 100% in OCV table 
*    if battery is Full charged.
* - int vbat_ocv <- unit is uV
**/
static int calib_ocvTable(struct ricoh61x_battery_info *info, int vbat_ocv)
{
	int ret;
	int cutoff_ocv;
	int i;
	int ocv100_new;
	int start_per = 0;
	int end_per = 0;
	
	if (info->soca->Ibat_ave > RICOH61x_REL1_SEL_VALUE) {
		printk("PMU: %s IBAT > 64mA -- Not Calibration --\n", __func__);
		return 0;
	}
	
	if (vbat_ocv < info->soca->OCV100_max) {
		if (vbat_ocv < info->soca->OCV100_min)
			ocv100_new = info->soca->OCV100_min;
		else
			ocv100_new = vbat_ocv;
	} else {
		ocv100_new = info->soca->OCV100_max;
	}
	printk("PMU : %s :max %d min %d current %d\n",__func__,info->soca->OCV100_max,info->soca->OCV100_min,vbat_ocv);
	printk("PMU : %s : New OCV 100 = 0x%x\n",__func__,ocv100_new);
	
	/* FG_En Off */
	ret = ricoh61x_clr_bits(info->dev->parent, FG_CTRL_REG, 0x01);
	if (ret < 0) {
		dev_err(info->dev,"Error in FG_En OFF\n");
		goto err;
	}


	//cutoff_ocv = (battery_init_para[info->num][0]<<8) | (battery_init_para[info->num][1]);
	cutoff_ocv = get_OCV_voltage(info, 0, USING);

	info->soca->ocv_table_def[10] = info->soca->OCV100_max;

	ricoh61x_scaling_OCV_table(info, cutoff_ocv/1000, ocv100_new/1000, &start_per, &end_per);

	ret = ricoh61x_bulk_writes_bank1(info->dev->parent,
				BAT_INIT_TOP_REG, 22, battery_init_para[info->num]);
	if (ret < 0) {
		dev_err(info->dev, "batterry initialize error\n");
		goto err;
	}

	for (i = 0; i <= 10; i = i+1) {
		info->soca->ocv_table[i] = get_OCV_voltage(info, i, USING);
		printk("PMU: %s : * %d0%% voltage = %d uV\n",
				 __func__, i, info->soca->ocv_table[i]);
	}
	
	/* FG_En on & Reset*/
	ret = reset_FG_process(info);
	if (ret < 0) {
		dev_err(info->dev, "Error in FG_En On & Reset %d\n", ret);
		goto err;
	}

	printk("PMU: %s Exit \n", __func__);
	return 0;
err:
	return ret;

}

#endif

/**
* get SOC value during period of Suspend/Hibernate with voltage method
* info : battery info
*
* return value : soc, unit is 0.01%
*/

static int calc_soc_by_voltageMethod(struct ricoh61x_battery_info *info)
{
	int soc;
	int ret;
	if (info->soca->ready_fg) {
		ret = measure_vbatt_FG(info, &info->soca->Vbat_ave);
	}
	else {
		printk("%s(): FG not ready , getting vbatt from ADC .\n",__FUNCTION__);
		ret = measure_vbatt_ADC(info, &info->soca->Vbat_ave);
	}


	if(10 == gptHWCFG->m_val.bBattery) {// 1200mAh battery
#ifndef ALL_LPTFT_PANEL //[
		if (!NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bEPD_Flags,1)) {	
			if(info->soca->Vbat_ave > 4100000) {
				soc = 10000;
			} else if(info->soca->Vbat_ave < 3750000) {
				soc = 0;
			} else {
				soc = 10000 - ((4100000 - info->soca->Vbat_ave) / 35);
			}
		}
		else 
#endif //]!ALL_LPTFT_PANEL
		{
			// 1200mA for lp-tft
			if(info->soca->Vbat_ave > 4100000) {
				soc = 10000;
			} else if(info->soca->Vbat_ave < 3650000) {
				soc = 0;
			} else {
				int i;
				for (i = 0; i < 10; i++) {
					if (info->soca->ocv_table[i] <= info->soca->Vbat_ave && info->soca->ocv_table[i+1] > info->soca->Vbat_ave) {
						soc = 1000*i + ((info->soca->Vbat_ave - info->soca->ocv_table[i])*1000) / 
								(info->soca->ocv_table[i+1] - info->soca->ocv_table[i]);
						break;
					}
				}
			}		
			
		}
	}
	else if (81 == gptHWCFG->m_val.bPCB) {
		printk(KERN_ERR"==== %s E60U22 ====\n",__FUNCTION__);
		if(info->soca->Vbat_ave > 4100000) {
			soc = 10000;
		} else if(info->soca->Vbat_ave < 3472500) {
			soc = 0;
		} else {
			int i;
			for (i = 0; i < 10; i++) {
				if (info->soca->ocv_table[i] <= info->soca->Vbat_ave && info->soca->ocv_table[i+1] > info->soca->Vbat_ave) {
					soc = 1000*i + ((info->soca->Vbat_ave - info->soca->ocv_table[i])*1000) / 
							(info->soca->ocv_table[i+1] - info->soca->ocv_table[i]);
					break;
				}
			}
		}
	}
	else {
		if(info->soca->Vbat_ave > 4100000) {
			soc = 10000;
		} else if(info->soca->Vbat_ave < 3500000) {
			soc = 0;
		} else {
			soc = 10000 - ((4100000 - info->soca->Vbat_ave) / 60);
		}
	}

	get_power_supply_status(info);
	if (0==giCapacityMethod && POWER_SUPPLY_STATUS_FULL == info->soca->chg_status) {
		soc = 10000;
	} else {
		soc = min(soc, 9900);
	}

	// Cutoff under 1% on Voltage Method
	soc = (soc / 100) * 100;

	SUSPEND_PRINT(KERN_DEBUG"PMU : %s : VBAT is %d [uV], soc is %d [0.01%%] ----------\n"
		,__func__, info->soca->Vbat_ave, soc);

	// soc range is 0~10000
	return soc;
}

/**
* update RSOC and related parameters after using voltage method
* info : battery info
* soc_voltage : soc by using voltage method
*
*/

static void update_rsoc_on_voltageMethod(struct ricoh61x_battery_info *info, int soc_voltage)
{
	info->soca->init_pswr = soc_voltage / 100;
	write_extra_value_to_ccsum(info, (soc_voltage % 100));
	info->soca->status = RICOH61x_SOCA_STABLE;
	info->soca->last_soc = soc_voltage;
	info->soca->rsoc_ready_flag = 0;

	SUSPEND_PRINT(KERN_INFO "PMU: %s : Voltage Method. state(%d), dsoc(%d), rsoc(%d), init_pswr(%d), cc_delta(%d) ----------\n",
		 __func__, info->soca->status, soc_voltage, soc_voltage, info->soca->init_pswr, soc_voltage%100);

	return;
}

/**
* update RSOC and related parameters after using current method
* Only resume function can call this one.
* info : battery info
* soc_current : soc by using current method
*
*/

static void update_rsoc_on_currentMethod(struct ricoh61x_battery_info *info, int soc_current)
{
	int resume_rsoc;

	if (RICOH61x_SOCA_START == info->soca->status
		|| RICOH61x_SOCA_UNSTABLE == info->soca->status
		|| RICOH61x_SOCA_STABLE == info->soca->status) {
		resume_rsoc = soc_current;
	} else {
		resume_rsoc = info->soca->suspend_rsoc + info->soca->cc_delta;
	}
	resume_rsoc = max(0, min(10000, resume_rsoc));	// Apply upper&lower limit
	info->soca->init_pswr = resume_rsoc / 100;
	write_extra_value_to_ccsum(info, (resume_rsoc % 100));
	info->soca->rsoc_ready_flag = 0;
	SUSPEND_PRINT(KERN_DEBUG "PMU: %s : Current Method. state(%d), dsoc(%d), rsoc(%d), init_pswr(%d), cc_delta(%d) ----------\n",
		 __func__, info->soca->status, soc_current, resume_rsoc, info->soca->init_pswr, resume_rsoc%100);

	return;
}


static void ricoh61x_displayed_work(struct work_struct *work)
{
	int err;
	uint8_t val;
	uint8_t val_pswr;
	uint8_t val2;
	int soc_round;
	int last_soc_round;
	int last_disp_round;
	int displayed_soc_temp;
	int disp_dec;
	int cc_cap = 0;
	long cc_cap_mas = 0;
	bool is_charging = true;
	int re_cap,fa_cap,use_cap;
	bool is_jeita_updated;
	uint8_t reg_val;
	int delay_flag = 0;
	int Vbat = 0;
	int Ibat = 0;
	int Vsys = 0;
	int temp_ocv;
	int current_soc_full;
	int fc_delta = 0;
	int temp_soc;
	int current_cc_sum;
	int calculated_ocv;
	long full_rate = 0;
	long full_rate_org;
	long full_rate_max;
	long full_rate_min;
	int temp_cc_delta_cap;
	int ibat_soc = 0;
	int ibat_soc_base;
	int dsoc_var;
	int dsoc_var_org;
	int cc_delta;
	int i;
	int last_dsoc;

	struct ricoh61x_battery_info *info = container_of(work,
	struct ricoh61x_battery_info, displayed_work.work);

	if (info->entry_factory_mode) {
		info->soca->status = RICOH61x_SOCA_STABLE;
		info->soca->displayed_soc = -EINVAL;
		info->soca->ready_fg = 0;
		return;
	}

	if (info->stop_disp) {
		printk(KERN_INFO "PMU: Finish displayed_work func\n",
			__func__);
		return;
	}

	mutex_lock(&info->lock);
	
	is_jeita_updated = false;

	if ((RICOH61x_SOCA_START == info->soca->status)
		 || (RICOH61x_SOCA_STABLE == info->soca->status)
		 || (RICOH61x_SOCA_FULL == info->soca->status))
		{
			info->soca->ready_fg = 1;
		}
	//if (RICOH61x_SOCA_FG_RESET != info->soca->status)
	//	Set_back_ocv_table(info);

	if (bat_alert_req_flg == 1) {
		// Use Voltage method if difference is large
		info->soca->displayed_soc = calc_soc_by_voltageMethod(info);
		update_rsoc_on_voltageMethod(info, info->soca->displayed_soc);
		bat_alert_req_flg = 0;

		goto end_flow;
	}

	/* judge Full state or Moni Vsys state */
	calculated_ocv = calc_ocv(info);
	if ((RICOH61x_SOCA_DISP == info->soca->status)
		 || (RICOH61x_SOCA_STABLE == info->soca->status)) {	
		/* caluc 95% ocv */
		temp_ocv = get_OCV_voltage(info, 10, USING) -
					(get_OCV_voltage(info, 10, USING) - get_OCV_voltage(info, 9, USING))/2;
		
		if(g_full_flag == 1){	/* for issue 1 solution start*/
			info->soca->status = RICOH61x_SOCA_FULL;
			info->soca->last_soc_full = 0;
		} else if ((POWER_SUPPLY_STATUS_FULL == info->soca->chg_status)
			&& (calculated_ocv > temp_ocv)) {
			info->soca->status = RICOH61x_SOCA_FULL;
			g_full_flag = 0;
			info->soca->last_soc_full = 0;
		} else if ( ((10 == gptHWCFG->m_val.bBattery)&&info->soca->Ibat_ave >= -6) ||
			((10 != gptHWCFG->m_val.bBattery)&&info->soca->Ibat_ave >= -12) ) 
		{
			/* for issue1 solution end */
			/* check Full state or not*/
			if ((calculated_ocv > get_OCV_voltage(info, RICOH61x_ENTER_FULL_STATE_OCV, USING))
 				|| (POWER_SUPPLY_STATUS_FULL == info->soca->chg_status)
				|| (info->soca->displayed_soc > RICOH61x_ENTER_FULL_STATE_DSOC * 100)) {
				info->soca->status = RICOH61x_SOCA_FULL;
				g_full_flag = 0;
				info->soca->last_soc_full = 0;
			} else if ((calculated_ocv > get_OCV_voltage(info, 9, USING))
				&& (info->soca->Ibat_ave < 300)) {
				info->soca->status = RICOH61x_SOCA_FULL;
				g_full_flag = 0;
				info->soca->last_soc_full = 0;
			}
		} else { /* dis-charging */
//			if (info->soca->displayed_soc/100 < RICOH61x_ENTER_LOW_VOL) {
				initSettingOfLowVoltage(info);
				info->soca->status = RICOH61x_SOCA_LOW_VOL;
//			}
		}
	}

	if (RICOH61x_SOCA_STABLE == info->soca->status) {
		info->soca->soc = calc_capacity_2(info);
		info->soca->soc_delta = info->soca->soc - info->soca->last_soc;

		if (info->soca->soc_delta >= -100 && info->soca->soc_delta <= 100) {
			info->soca->displayed_soc = info->soca->soc;
		} else {
			info->soca->status = RICOH61x_SOCA_DISP;
		}
		info->soca->last_soc = info->soca->soc;
		info->soca->soc_delta = 0;
	} else if (RICOH61x_SOCA_FULL == info->soca->status) {
		err = check_jeita_status(info, &is_jeita_updated);
		if (err < 0) {
			dev_err(info->dev, "Error in updating JEITA %d\n", err);
			goto end_flow;
		}
		info->soca->soc = calc_capacity(info) * 100;
		info->soca->last_soc = calc_capacity_2(info);	/* for DISP */
		last_dsoc = info->soca->displayed_soc;
		
		if ( ((10 == gptHWCFG->m_val.bBattery)&&info->soca->Ibat_ave >= -6) ||
			((10 != gptHWCFG->m_val.bBattery)&&info->soca->Ibat_ave >= -12) ) 
		{ /* charging */
			if (0 == info->soca->jt_limit) {
				if (g_full_flag == 1) {
					
					if (POWER_SUPPLY_STATUS_FULL == info->soca->chg_status) {
						if(info->soca->full_reset_count < RICOH61x_UPDATE_COUNT_FULL_RESET) {
							info->soca->full_reset_count++;
						} else if (info->soca->full_reset_count < (RICOH61x_UPDATE_COUNT_FULL_RESET + 1)) {
							err = reset_FG_process(info);
							if (err < 0)
								dev_err(info->dev, "Error in writing the control register\n");
							info->soca->full_reset_count++;
							info->soca->rsoc_ready_flag =1;
							goto end_flow;
						} else if(info->soca->full_reset_count < (RICOH61x_UPDATE_COUNT_FULL_RESET + 2)) {
							info->soca->full_reset_count++;
							info->soca->fc_cap = 0;
							info->soca->soc_full = info->soca->soc;
						}
					} else {
						if(info->soca->fc_cap < -1 * 200) {
							g_full_flag = 0;
							info->soca->displayed_soc = 99 * 100;
						}
						info->soca->full_reset_count = 0;
					}
					

					if(info->soca->rsoc_ready_flag ==1) {
						err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 1);
						if (err < 0)
							dev_err(info->dev, "Read cc_sum Error !!-----\n");

						fc_delta = (is_charging == true) ? cc_cap : -cc_cap;

						info->soca->fc_cap = info->soca->fc_cap + fc_delta;
					}

					if (g_full_flag == 1){
						info->soca->displayed_soc = 100*100;
					}
				} else {
					if ((calculated_ocv < get_OCV_voltage(info, (RICOH61x_ENTER_FULL_STATE_OCV - 1), USING))
						&& (info->soca->displayed_soc < (RICOH61x_ENTER_FULL_STATE_DSOC - 10) * 100)) { /* fail safe*/
						g_full_flag = 0;
						info->soca->status = RICOH61x_SOCA_DISP;
						info->soca->soc_delta = 0;
						info->soca->full_reset_count = 0;
						info->soca->last_soc = info->soca->soc;
						info->soca->temp_cc_delta_cap = 0;
					} else if ((POWER_SUPPLY_STATUS_FULL == info->soca->chg_status) 
						&& (info->soca->displayed_soc >= 9890)){
						info->soca->displayed_soc = 100*100;
						g_full_flag = 1;
						info->soca->full_reset_count = 0;
						info->soca->soc_full = info->soca->soc;
						info->soca->fc_cap = 0;
						info->soca->last_soc_full = 0;
#ifdef ENABLE_OCV_TABLE_CALIB
						err = calib_ocvTable(info,calculated_ocv);
						if (err < 0)
							dev_err(info->dev, "Calibration OCV Error !!\n");
#endif
					} else {
						fa_cap = get_check_fuel_gauge_reg(info, FA_CAP_H_REG, FA_CAP_L_REG,
							0x7fff);
						
						if (info->soca->displayed_soc >= 9950) {
							if((info->soca->soc_full - info->soca->soc) < 200) {
								goto end_flow;
							}
						}

						/* Calculate CC Delta */
						if(info->soca->rsoc_ready_flag ==1) {
							err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 1);
							if (err < 0)
								dev_err(info->dev, "Read cc_sum Error !!-----\n");
							info->soca->cc_delta
								 = (is_charging == true) ? cc_cap : -cc_cap;
						} else {
							err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 0);
							if (err < 0)
								dev_err(info->dev, "Read cc_sum Error !!-----\n");
							cc_delta = (is_charging == true) ? cc_cap : -cc_cap;
							current_soc_full = info->soca->init_pswr * 100 + cc_delta;

							if (info->soca->last_soc_full == 0) { /* initial setting of last cc sum */
								info->soca->cc_delta = 0;
								info->soca->rsoc_limit = 0;
								printk(KERN_INFO "PMU: %s 1st last_soc_full(%d), cc_delta=0\n",
									 __func__, info->soca->last_soc_full);
							} else if (info->soca->rsoc_limit == 1) {
								info->soca->cc_delta = 100 + current_soc_full - info->soca->last_soc_full;
							} else {
								info->soca->cc_delta = current_soc_full - info->soca->last_soc_full;
							}
							info->soca->last_soc_full = current_soc_full;

							if ((info->soca->init_pswr == 100) && (cc_delta >= 100)) {
								info->soca->rsoc_limit = 1;
							} else {
								info->soca->rsoc_limit = 0;
							}
						}
						
						printk(KERN_INFO "PMU: %s rrf= %d: cc_delta= %d: current_soc= %d: rsoc_limit= %d: cc_delta_temp = %d:\n",
							 __func__, info->soca->rsoc_ready_flag, info->soca->cc_delta, current_soc_full, info->soca->rsoc_limit,info->soca->temp_cc_delta_cap);

						info->soca->temp_cc_delta_cap = min(800, info->soca->temp_cc_delta_cap);

						info->soca->cc_delta += info->soca->temp_cc_delta_cap;
						info->soca->temp_cc_delta_cap = 0;


#ifdef LTS_DEBUG
						printk(KERN_INFO "PMU: %s rrf= %d: cc_delta= %d: current_soc= %d: rsoc_limit= %d:\n",
							 __func__, info->soca->rsoc_ready_flag, info->soca->cc_delta, current_soc_full, info->soca->rsoc_limit);
#endif

						if(POWER_SUPPLY_STATUS_FULL == info->soca->chg_status)
						{
							info->soca->displayed_soc += 13 * 3000 / fa_cap;
						} else {
						  
							if(81==gptHWCFG->m_val.bPCB)
                                                        {       // Disable ibat_table
                                                                printk(KERN_ERR"==== DISABLE ibat ====\n");
                                                                ibat_soc_base = 10000 - (RICOH61x_IBAT_TABLE_NUM - 1) * 100 - 50;

                                                                if (ibat_soc_base < info->soca->displayed_soc){
                                                                        ibat_soc = ibat_soc_base;
                                                                } else {
                                                                        ibat_soc = info->soca->displayed_soc;
                                                                }
                                                                printk(KERN_INFO "PMU: %s IBAT= %d: ibat_table[%d%%]= %d: ibat_soc= %d ************\n",
                                                                        __func__, info->soca->Ibat_ave, (100 - RICOH61x_IBAT_TABLE_NUM + 1), ibat_table[0], ibat_soc);
                                                        }
                                                        else {
								ibat_soc_base = 10000 - (RICOH61x_IBAT_TABLE_NUM - 1) * 100 - 50;
								if (ibat_table[0] < info->soca->Ibat_ave) {
									if (ibat_soc_base < info->soca->displayed_soc){
										ibat_soc = ibat_soc_base;
									} else {
										ibat_soc = info->soca->displayed_soc;
									}
									printk(KERN_INFO "PMU: %s IBAT= %d: ibat_table[%d%%]= %d: ibat_soc= %d ************\n",
										__func__, info->soca->Ibat_ave, (100 - RICOH61x_IBAT_TABLE_NUM + 1), ibat_table[0], ibat_soc);
								} else if (ibat_table[RICOH61x_IBAT_TABLE_NUM-1] >= info->soca->Ibat_ave) {
									ibat_soc = 9950;
									printk(KERN_INFO "PMU: %s IBAT= %d: ibat_table[100%%]= %d: ibat_soc= %d ************\n",
										__func__, info->soca->Ibat_ave, ibat_table[RICOH61x_IBAT_TABLE_NUM-1], ibat_soc);
								} else {
									for (i = 1; i <= (RICOH61x_IBAT_TABLE_NUM-1); i++) {
										if(ibat_table[i] <= info->soca->Ibat_ave) {
											ibat_soc = Calc_Linear_Interpolation(
												(i-1) * 100, ibat_table[i-1], i * 100,
												ibat_table[i], info->soca->Ibat_ave);
											ibat_soc += ibat_soc_base;

#ifdef LTS_DEBUG
											printk(KERN_INFO "PMU: %s IBAT= %d: ibat_table[%d%%]= %d, ibat_table[%d%%]= %d: ibat_soc= %d: ************\n",
												__func__, info->soca->Ibat_ave, (100 - RICOH61x_IBAT_TABLE_NUM + i), ibat_table[i-1],
												  (100 - RICOH61x_IBAT_TABLE_NUM + 1 + i), ibat_table[i], ibat_soc);
#endif
											break;
										}
									}
								}
							}

							// full_rate = 100 * (100 - DSOC) * (100 - DSOC) / ((100 - IBAT_SOC) * (100 - IBAT_SOC))
							full_rate = (long)(100 * (10000 - info->soca->displayed_soc)) / (10000 - ibat_soc);
							full_rate = full_rate * (10000 - info->soca->displayed_soc) / (10000 - ibat_soc);

							/* Adjust parameters */
							full_rate_org = full_rate;
							
							if(10 == gptHWCFG->m_val.bBattery) {// 1200mAh battery
								full_rate_max = 180;
								full_rate_min = 30;

								if (ibat_soc >= 8950) {
									full_rate_max = 180 + (ibat_soc - 8950) / 3;
								}
							}
							else {
								full_rate_max = 140;
								full_rate_min = 30;

								if (ibat_soc >= 9450) {
									full_rate_max = 140 + (ibat_soc - 9450) / 3;
								}
							}

							full_rate = min(full_rate_max, max(full_rate_min, full_rate));

							dsoc_var = info->soca->cc_delta * (int)full_rate / 100;
							dsoc_var_org = dsoc_var;
							if (info->soca->cc_delta <= 0) {
								dsoc_var = 0;
							} else {
								dsoc_var = max(3, dsoc_var);
							}

#ifdef LTS_DEBUG
							printk(KERN_INFO "PMU: cc_delta= %d: ibat_soc= %d: full_rate= %ld: %ld: dsoc_var= %d: %d: IBAT= %d: DSOC= %d: RSOC= %d:\n",
								info->soca->cc_delta, ibat_soc, full_rate_org, full_rate, dsoc_var_org, dsoc_var,
								info->soca->Ibat_ave, (info->soca->displayed_soc + dsoc_var), info->soca->last_soc);
#endif

							info->soca->displayed_soc
								= info->soca->displayed_soc + dsoc_var;
						}
						info->soca->displayed_soc
							 = min(10000, info->soca->displayed_soc);
						info->soca->displayed_soc = max(0, info->soca->displayed_soc);

						if (info->soca->displayed_soc >= 9890) {
							info->soca->displayed_soc = 99 * 100;
						}
					}
				}
			} else {
				info->soca->full_reset_count = 0;
			}
		} else { /* discharging */
			if (info->soca->displayed_soc >= 9950) {
				if (info->soca->Ibat_ave <= -1 * RICOH61x_REL1_SEL_VALUE) {
					if ((calculated_ocv < (get_OCV_voltage(info, 9, USING) + (get_OCV_voltage(info, 10, USING) - get_OCV_voltage(info, 9, USING))*3/10))
						|| ((info->soca->soc_full - info->soca->soc) > 200)) {

						g_full_flag = 0;
						info->soca->full_reset_count = 0;
						info->soca->displayed_soc = 100 * 100;
						info->soca->status = RICOH61x_SOCA_DISP;
						info->soca->last_soc = info->soca->soc;
						info->soca->soc_delta = 0;
						info->soca->temp_cc_delta_cap = 0;
					} else {
						info->soca->displayed_soc = 100 * 100;
					}
				} else { /* into relaxation state */
					ricoh61x_read(info->dev->parent, CHGSTATE_REG, &reg_val);
					if (reg_val & 0xc0) {
						info->soca->displayed_soc = 100 * 100;
					} else {
						g_full_flag = 0;
						info->soca->full_reset_count = 0;
						info->soca->displayed_soc = 100 * 100;
						info->soca->status = RICOH61x_SOCA_DISP;
						info->soca->last_soc = info->soca->soc;
						info->soca->soc_delta = 0;
						info->soca->temp_cc_delta_cap = 0;
					}
				}
			} else {
				g_full_flag = 0;
				info->soca->status = RICOH61x_SOCA_DISP;
				info->soca->soc_delta = 0;
				info->soca->full_reset_count = 0;
				info->soca->last_soc = info->soca->soc;
				info->soca->temp_cc_delta_cap = 0;
			}
		}
	} else if (RICOH61x_SOCA_LOW_VOL == info->soca->status) {

			mainFlowOfLowVoltage(info);
	}

	if (RICOH61x_SOCA_DISP == info->soca->status) {

		info->soca->soc = calc_capacity_2(info);

		soc_round = (info->soca->soc + 50) / 100;
		last_soc_round = (info->soca->last_soc + 50) / 100;
		last_disp_round = (info->soca->displayed_soc + 50) / 100;

		info->soca->soc_delta =
			info->soca->soc_delta + (info->soca->soc - info->soca->last_soc);

		info->soca->last_soc = info->soca->soc;
		/* six case */
		if (last_disp_round == soc_round) {
			/* if SOC == DISPLAY move to stable */
			info->soca->displayed_soc = info->soca->soc ;
			info->soca->status = RICOH61x_SOCA_STABLE;
			delay_flag = 1;
		} else if (info->soca->Ibat_ave > 0) {
			if ((0 == info->soca->jt_limit) || 
			(POWER_SUPPLY_STATUS_FULL != info->soca->chg_status)) {
				/* Charge */
				if (last_disp_round < soc_round) {
					/* Case 1 : Charge, Display < SOC */
					if (info->soca->soc_delta >= 100) {
						info->soca->displayed_soc
							= last_disp_round * 100 + 50;
	 					info->soca->soc_delta -= 100;
						if (info->soca->soc_delta >= 100)
		 					delay_flag = 1;
					} else {
						info->soca->displayed_soc += 25;
						disp_dec = info->soca->displayed_soc % 100;
						if ((50 <= disp_dec) && (disp_dec <= 74))
							info->soca->soc_delta = 0;
					}
					if ((info->soca->displayed_soc + 50)/100
								 >= soc_round) {
						info->soca->displayed_soc
							= info->soca->soc ;
						info->soca->status
							= RICOH61x_SOCA_STABLE;
						delay_flag = 1;
					}
				} else if (last_disp_round > soc_round) {
					/* Case 2 : Charge, Display > SOC */
					if (info->soca->soc_delta >= 300) {
						info->soca->displayed_soc += 100;
						info->soca->soc_delta -= 300;
					}
					if ((info->soca->displayed_soc + 50)/100
								 <= soc_round) {
						info->soca->displayed_soc
							= info->soca->soc ;
						info->soca->status
						= RICOH61x_SOCA_STABLE;
						delay_flag = 1;
					}
				}
			} else {
				info->soca->soc_delta = 0;
			}
		} else {
			/* Dis-Charge */
			if (last_disp_round > soc_round) {
				/* Case 3 : Dis-Charge, Display > SOC */
				if (info->soca->soc_delta <= -100) {
					info->soca->displayed_soc
						= last_disp_round * 100 - 75;
					info->soca->soc_delta += 100;
					if (info->soca->soc_delta <= -100)
						delay_flag = 1;
				} else {
					info->soca->displayed_soc -= 25;
					disp_dec = info->soca->displayed_soc % 100;
					if ((25 <= disp_dec) && (disp_dec <= 49))
						info->soca->soc_delta = 0;
				}
				if ((info->soca->displayed_soc + 50)/100
							 <= soc_round) {
					info->soca->displayed_soc
						= info->soca->soc ;
					info->soca->status
						= RICOH61x_SOCA_STABLE;
					delay_flag = 1;
				}
			} else if (last_disp_round < soc_round) {
				/* Case 4 : Dis-Charge, Display < SOC */
				if (info->soca->soc_delta <= -300) {
					info->soca->displayed_soc -= 100;
					info->soca->soc_delta += 300;
				}
				if ((info->soca->displayed_soc + 50)/100
							 >= soc_round) {
					info->soca->displayed_soc
						= info->soca->soc ;
					info->soca->status
						= RICOH61x_SOCA_STABLE;
					delay_flag = 1;
				}
			}
		}
	} else if (RICOH61x_SOCA_UNSTABLE == info->soca->status) {
		/* caluc 95% ocv */
		temp_ocv = get_OCV_voltage(info, 10, USING) -
					(get_OCV_voltage(info, 10, USING) - get_OCV_voltage(info, 9, USING))/2;
		
		if(g_full_flag == 1){	/* for issue 1 solution start*/
			info->soca->status = RICOH61x_SOCA_FULL;
			info->soca->last_soc_full = 0;
			err = reset_FG_process(info);
			if (err < 0)
				dev_err(info->dev, "Error in writing the control register\n");
			
			goto end_flow;
		}else if ((POWER_SUPPLY_STATUS_FULL == info->soca->chg_status)
			&& (calculated_ocv > temp_ocv)) {
			info->soca->status = RICOH61x_SOCA_FULL;
			g_full_flag = 0;
			info->soca->last_soc_full = 0;
			err = reset_FG_process(info);
			if (err < 0)
				dev_err(info->dev, "Error in writing the control register\n");
			goto end_flow;
		} else if ( ((10 == gptHWCFG->m_val.bBattery)&&info->soca->Ibat_ave >= -6) ||
			((10 != gptHWCFG->m_val.bBattery)&&info->soca->Ibat_ave >= -12) ) 
		{
			/* for issue1 solution end */
			/* check Full state or not*/
			if ((calculated_ocv > (get_OCV_voltage(info, 9, USING) + (get_OCV_voltage(info, 10, USING) - get_OCV_voltage(info, 9, USING))*7/10))
				|| (POWER_SUPPLY_STATUS_FULL == info->soca->chg_status)
				|| (info->soca->displayed_soc > 9850))
			{
				info->soca->status = RICOH61x_SOCA_FULL;
				g_full_flag = 0;
				info->soca->last_soc_full = 0;
				err = reset_FG_process(info);
				if (err < 0)
					dev_err(info->dev, "Error in writing the control register\n");
				goto end_flow;
			} else if ((calculated_ocv > (get_OCV_voltage(info, 9, USING)))
				&& (info->soca->Ibat_ave < 300))
			{
				info->soca->status = RICOH61x_SOCA_FULL;
				g_full_flag = 0;
				info->soca->last_soc_full = 0;
				err = reset_FG_process(info);
				if (err < 0)
					dev_err(info->dev, "Error in writing the control register\n");				
				goto end_flow;
			}
		}

		info->soca->soc = info->soca->init_pswr * 100;

		err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
						 &is_charging, 0);
		if (err < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");

		info->soca->cc_delta
			 = (is_charging == true) ? cc_cap : -cc_cap;

		displayed_soc_temp
		       = info->soca->soc + info->soca->cc_delta;
		if (displayed_soc_temp < 0)
			displayed_soc_temp = 0;
		displayed_soc_temp
			 = min(9850, displayed_soc_temp);
		displayed_soc_temp = max(0, displayed_soc_temp);

		info->soca->displayed_soc = displayed_soc_temp;

	} else if (RICOH61x_SOCA_FG_RESET == info->soca->status) {
		/* No update */
	} else if (RICOH61x_SOCA_START == info->soca->status) {

		err = measure_Ibatt_FG(info, &Ibat);
		err = measure_vbatt_FG(info, &Vbat);
		err = measure_vsys_ADC(info, &Vsys);

		info->soca->Ibat_ave = Ibat;
		info->soca->Vbat_ave = Vbat;
		info->soca->Vsys_ave = Vsys;

		err = check_jeita_status(info, &is_jeita_updated);
		is_jeita_updated = false;
		if (err < 0) {
			dev_err(info->dev, "Error in updating JEITA %d\n", err);
		}
		err = ricoh61x_read(info->dev->parent, PSWR_REG, &val_pswr);
		val_pswr &= 0x7f;

		if (info->first_pwon) {
			displayed_soc_temp = val_pswr * 100;

			info->soca->soc = calc_capacity(info) * 100;

			err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 1);
			if (err < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");

			info->soca->cc_delta
				 = (is_charging == true) ? cc_cap : -cc_cap;

			//get DSOC temp value
			if (displayed_soc_temp == 0) {	//initial power on or some error case

				displayed_soc_temp = info->soca->soc;
				printk(KERN_INFO "PMU: %s : initial power on\n",__func__);

			} else if ((Ibat > 0)
				&& (displayed_soc_temp < info->soca->soc)){ //charge and poff_DSOC < RSOC
				displayed_soc_temp = info->soca->soc;
				printk(KERN_INFO "PMU: %s : normal case Ibat is %dmA, poffDSOC is %d, RSOC is %dn\n"
					,__func__, Ibat, val_pswr*100, info->soca->soc);
			} else if ((info->soca->cc_delta <= 0)
				&& (displayed_soc_temp > info->soca->soc)){ //discharge and poff_DSOC > RSOC

				displayed_soc_temp = info->soca->soc;
				printk(KERN_INFO "PMU: %s : normal case cc delta is %dmA, poffDSOC is %d, RSOC is %dn\n"
					,__func__, info->soca->cc_delta, val_pswr*100, info->soca->soc);

			} else if ((info->soca->cc_delta > 0)
				&& (displayed_soc_temp < info->soca->soc)){ //charge and poff_DSOC < RSOC
				displayed_soc_temp = info->soca->soc;
				printk(KERN_INFO "PMU: %s : normal case cc delta is %dmA, poffDSOC is %d, RSOC is %dn\n"
					,__func__, info->soca->cc_delta, val_pswr*100, info->soca->soc);
			} else {
				//displayed_soc_temp = displayed_soc_temp;
				printk(KERN_INFO "PMU: %s : error case cc delta is %dmA, poffDSOC is %d, RSOC is %dn\n"
					,__func__, info->soca->cc_delta, val_pswr*100, info->soca->soc);
			}

			//val = (info->soca->soc + 50)/100;
			val = (displayed_soc_temp + 50)/100;
			val &= 0x7f;
			err = ricoh61x_write(info->dev->parent, PSWR_REG, val);
			if (err < 0)
				dev_err(info->dev, "Error in writing PSWR_REG\n");
			info->soca->init_pswr = val;
			g_soc = val;
			set_current_time2register(info);

			if (0 == info->soca->jt_limit) {
				check_charge_status_2(info, displayed_soc_temp);
			} else {
				info->soca->displayed_soc = displayed_soc_temp;
			}
			if (Ibat < 0) {
				initSettingOfLowVoltage(info);
				info->soca->status = RICOH61x_SOCA_LOW_VOL;
			} else {
				info->soca->status = RICOH61x_SOCA_DISP;
				info->soca->soc_delta = 0;
				info->soca->last_soc = displayed_soc_temp;
			}

		} else if (g_fg_on_mode && (val_pswr == 0x7f)) {
			info->soca->soc = calc_capacity(info) * 100;
			if (0 == info->soca->jt_limit) {
				check_charge_status_2(info, info->soca->soc);
			} else {
				info->soca->displayed_soc = info->soca->soc;
			}
			info->soca->last_soc = info->soca->soc;
			info->soca->status = RICOH61x_SOCA_STABLE;
		} else {
			info->soca->soc = val_pswr * 100;
			if (err < 0) {
				dev_err(info->dev,
					 "Error in reading PSWR_REG %d\n", err);
				info->soca->soc
					 = calc_capacity(info) * 100;
			}

			err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 2);
			if (err < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");

			info->soca->cc_delta
				 = (is_charging == true) ? cc_cap : -cc_cap;
			displayed_soc_temp
			       = info->soca->soc + (info->soca->cc_delta / 100) * 100;

			displayed_soc_temp
				 = min(10000, displayed_soc_temp);
			if (displayed_soc_temp <= 100) {
				displayed_soc_temp = 100;
				err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 1);
			}

			printk(KERN_INFO "PMU: %s : dsoc_temp(%d), soc(%d), cc_delta(%d)\n",
				 __func__, displayed_soc_temp, info->soca->soc, info->soca->cc_delta);
			printk(KERN_INFO "PMU: %s : status(%d), rsoc_ready_flag(%d)\n",
				 __func__, info->soca->status, info->soca->rsoc_ready_flag);

			if (0 == info->soca->jt_limit) {
				check_charge_status_2(info, displayed_soc_temp);
			} else {
				info->soca->displayed_soc = displayed_soc_temp;
			}

			val = (displayed_soc_temp + 50)/100;
			val &= 0x7f;
			err = ricoh61x_write(info->dev->parent, PSWR_REG, val);
			if (err < 0)
				dev_err(info->dev, "Error in writing PSWR_REG\n");
			info->soca->init_pswr = val;
			g_soc = val;
			set_current_time2register(info);

			info->soca->last_soc = calc_capacity_2(info);

			if(info->soca->rsoc_ready_flag == 0) {

				info->soca->status = RICOH61x_SOCA_DISP;
				info->soca->soc_delta = 0;
#ifdef	_RICOH619_DEBUG_
				printk(KERN_DEBUG"PMU FG_RESET : %s : initial  dsoc  is  %d\n",__func__,info->soca->displayed_soc);
#endif
			} else if  (Ibat < 0) {
				initSettingOfLowVoltage(info);
				info->soca->status = RICOH61x_SOCA_LOW_VOL;
			} else {
				info->soca->status = RICOH61x_SOCA_DISP;
				info->soca->soc_delta = 0;
			}
		}
	}
end_flow:
	/* keep DSOC = 1 when Vbat is over 3.4V*/
	if( info->fg_poff_vbat != 0) {
		if (info->soca->zero_flg == 1) {
			if ((info->soca->Ibat_ave >= 0)
			|| (info->soca->Vbat_ave >= (info->fg_poff_vbat+100)*1000)) {
				info->soca->zero_flg = 0;
			} else {
				info->soca->displayed_soc = 0;
			}
		} else if (info->soca->displayed_soc < 50) {
			if (info->soca->Vbat_ave < 2000*1000) { /* error value */
				info->soca->displayed_soc = 100;
			} else if (info->soca->Vbat_ave < info->fg_poff_vbat*1000) {
				info->soca->displayed_soc = 0;
				info->soca->zero_flg = 1;
			} else {
				info->soca->displayed_soc = 100;
			}
		}
	}

	if (g_fg_on_mode
		 && (info->soca->status == RICOH61x_SOCA_STABLE)) {
		err = ricoh61x_write(info->dev->parent, PSWR_REG, 0x7f);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");
		g_soc = 0x7F;
		set_current_time2register(info);
		err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							&is_charging, 1);
		if (err < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");

	} else if (((RICOH61x_SOCA_UNSTABLE != info->soca->status)
			&& (info->soca->rsoc_ready_flag != 0))
			|| (RICOH61x_SOCA_LOW_VOL == info->soca->status)){
		if ((info->soca->displayed_soc + 50)/100 <= 1) {
			val = 1;
		} else {
			val = (info->soca->displayed_soc + 50)/100;
			val &= 0x7f;
		}
		err = ricoh61x_write(info->dev->parent, PSWR_REG, val);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");

		g_soc = val;
		set_current_time2register(info);

		info->soca->init_pswr = val;

		if(RICOH61x_SOCA_LOW_VOL != info->soca->status)
		{
		err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 1);
			if (err < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");
			printk(KERN_INFO "PMU: %s Full-Clear CC, PSWR(%d)\n",
				 __func__, val);
		}
	} else {	/* Case of UNSTABLE STATE */
		if ((info->soca->displayed_soc + 50)/100 <= 1) {
			val = 1;
		} else {
			val = (info->soca->displayed_soc + 50)/100;
			val &= 0x7f;
		}
		err = ricoh61x_write(info->dev->parent, PSWR_REG, val);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");

		g_soc = val;
		set_current_time2register(info);

		err = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 2);
		if (err < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");
		info->soca->cc_delta
			 = (is_charging == true) ? cc_cap : -cc_cap;

		val = info->soca->init_pswr + (info->soca->cc_delta/100);
		val = min(100, val);
		val = max(1, val);

		info->soca->init_pswr = val;

		info->soca->last_cc_rrf0 = info->soca->cc_delta%100;

		printk(KERN_INFO "PMU: %s Half-Clear CC, init_pswr(%d), cc_delta(%d)\n",
			 __func__, info->soca->init_pswr, info->soca->cc_delta);

	}
	
#ifdef	_RICOH619_DEBUG_
	printk(KERN_DEBUG"PMU:STATUS= %d: IBAT= %d: VSYS= %d: VBAT= %d: DSOC= %d: RSOC= %d: cc_delta=%d: rrf= %d\n",
	       info->soca->status, info->soca->Ibat_ave, info->soca->Vsys_ave, info->soca->Vbat_ave,
		info->soca->displayed_soc, info->soca->soc, info->soca->cc_delta, info->soca->rsoc_ready_flag);
#endif

//	printk("PMU AGE*STATUS * %d*IBAT*%d*VSYS*%d*VBAT*%d*DSOC*%d*RSOC*%d*-------\n",
//	      info->soca->status, info->soca->Ibat_ave, info->soca->Vsys_ave, info->soca->Vbat_ave,
//		info->soca->displayed_soc, info->soca->soc);

#ifdef DISABLE_CHARGER_TIMER
	/* clear charger timer */
	if ( info->soca->chg_status == POWER_SUPPLY_STATUS_CHARGING ) {
		err = ricoh61x_read(info->dev->parent, TIMSET_REG, &val);
		if (err < 0)
			dev_err(info->dev,
			"Error in read TIMSET_REG%d\n", err);
		/* to check bit 0-1 */
		val2 = val & 0x03;

		if (val2 == 0x02){
			/* set rapid timer 240 -> 300 */
			err = ricoh61x_set_bits(info->dev->parent, TIMSET_REG, 0x03);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
			}
		} else {
			/* set rapid timer 300 -> 240 */
			err = ricoh61x_clr_bits(info->dev->parent, TIMSET_REG, 0x01);
			err = ricoh61x_set_bits(info->dev->parent, TIMSET_REG, 0x02);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
			}
		}
	}
#endif

	if (0 == info->soca->ready_fg)
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH61x_FG_RESET_TIME * HZ);
	else if (delay_flag == 1)
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH61x_DELAY_TIME * HZ);
	else if ((RICOH61x_SOCA_DISP == info->soca->status)
		 && (info->soca->Ibat_ave > 0))
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH61x_DISP_CHG_UPDATE_TIME * HZ);
	else if ((info->soca->hurry_up_flg == 1) && (RICOH61x_SOCA_LOW_VOL == info->soca->status))
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH61x_LOW_VOL_DOWN_TIME * HZ);
	else
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH61x_DISPLAY_UPDATE_TIME * HZ);

	mutex_unlock(&info->lock);

	if((true == is_jeita_updated)
	|| (info->soca->last_displayed_soc/100 != (info->soca->displayed_soc+50)/100))
			power_supply_changed(info->battery);

	info->soca->last_displayed_soc = info->soca->displayed_soc+50;

	return;
}

static void ricoh61x_stable_charge_countdown_work(struct work_struct *work)
{
	int ret;
	int max = 0;
	int min = 100;
	int i;
	struct ricoh61x_battery_info *info = container_of(work,
		struct ricoh61x_battery_info, charge_stable_work.work);

	if (info->entry_factory_mode)
		return;

	mutex_lock(&info->lock);
	if (RICOH61x_SOCA_FG_RESET == info->soca->status)
		info->soca->ready_fg = 1;

	if (2 <= info->soca->stable_count) {
		if (3 == info->soca->stable_count
			&& RICOH61x_SOCA_FG_RESET == info->soca->status) {
			ret = reset_FG_process(info);
			if (ret < 0)
				dev_err(info->dev, "Error in writing the control register\n");
		}
		info->soca->stable_count = info->soca->stable_count - 1;
		queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH61x_FG_STABLE_TIME * HZ / 10);
	} else if (0 >= info->soca->stable_count) {
		/* Finished queue, ignore */
	} else if (1 == info->soca->stable_count) {
		if (RICOH61x_SOCA_UNSTABLE == info->soca->status) {
			/* Judge if FG need reset or Not */
			info->soca->soc = calc_capacity(info) * 100;
			if (info->chg_ctr != 0) {
				queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH61x_FG_STABLE_TIME * HZ / 10);
				mutex_unlock(&info->lock);
				return;
			}
			/* Do reset setting */
			ret = reset_FG_process(info);
			if (ret < 0)
				dev_err(info->dev, "Error in writing the control register\n");

			info->soca->status = RICOH61x_SOCA_FG_RESET;

			/* Delay for addition Reset Time (6s) */
			queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH61x_FG_RESET_TIME*HZ);
		} else if (RICOH61x_SOCA_FG_RESET == info->soca->status) {
			info->soca->reset_soc[2] = info->soca->reset_soc[1];
			info->soca->reset_soc[1] = info->soca->reset_soc[0];
			info->soca->reset_soc[0] = calc_capacity(info) * 100;
			info->soca->reset_count++;

			if (info->soca->reset_count > 10) {
				/* Reset finished; */
				info->soca->soc = info->soca->reset_soc[0];
				info->soca->stable_count = 0;
				goto adjust;
			}

			for (i = 0; i < 3; i++) {
				if (max < info->soca->reset_soc[i]/100)
					max = info->soca->reset_soc[i]/100;
				if (min > info->soca->reset_soc[i]/100)
					min = info->soca->reset_soc[i]/100;
			}

			if ((info->soca->reset_count > 3) && ((max - min)
					< RICOH61x_MAX_RESET_SOC_DIFF)) {
				/* Reset finished; */
				info->soca->soc = info->soca->reset_soc[0];
				info->soca->stable_count = 0;
				goto adjust;
			} else {
				/* Do reset setting */
				ret = reset_FG_process(info);
				if (ret < 0)
					dev_err(info->dev, "Error in writing the control register\n");

				/* Delay for addition Reset Time (6s) */
				queue_delayed_work(info->monitor_wqueue,
						 &info->charge_stable_work,
						 RICOH61x_FG_RESET_TIME*HZ);
			}
		/* Finished queue From now, select FG as result; */
		} else if (RICOH61x_SOCA_START == info->soca->status) {
			/* Normal condition */
		} else { /* other state ZERO/DISP/STABLE */
			info->soca->stable_count = 0;
		}

		mutex_unlock(&info->lock);
		return;

adjust:
		info->soca->last_soc = info->soca->soc;
		info->soca->status = RICOH61x_SOCA_DISP;
		info->soca->soc_delta = 0;

	}
	mutex_unlock(&info->lock);
	return;
}

static void ricoh61x_charge_monitor_work(struct work_struct *work)
{
	struct ricoh61x_battery_info *info = container_of(work,
		struct ricoh61x_battery_info, charge_monitor_work.work);

	get_power_supply_status(info);

	if (POWER_SUPPLY_STATUS_DISCHARGING == info->soca->chg_status
		|| POWER_SUPPLY_STATUS_NOT_CHARGING == info->soca->chg_status) {
		switch (info->soca->dischg_state) {
		case	0:
			info->soca->dischg_state = 1;
			break;
		case	1:
			info->soca->dischg_state = 2;
			break;
	
		case	2:
		default:
			break;
		}
	} else {
		info->soca->dischg_state = 0;
	}

	queue_delayed_work(info->monitor_wqueue, &info->charge_monitor_work,
					 RICOH61x_CHARGE_MONITOR_TIME * HZ);

	return;
}

static void ricoh61x_get_charge_work(struct work_struct *work)
{
	struct ricoh61x_battery_info *info = container_of(work,
		struct ricoh61x_battery_info, get_charge_work.work);

	int Vbat_temp, Vsys_temp, Ibat_temp;
	int Vbat_sort[RICOH61x_GET_CHARGE_NUM];
	int Vsys_sort[RICOH61x_GET_CHARGE_NUM];
	int Ibat_sort[RICOH61x_GET_CHARGE_NUM];
	int i, j;
	int ret;

	mutex_lock(&info->lock);

	for (i = RICOH61x_GET_CHARGE_NUM-1; i > 0; i--) {
		if (0 == info->soca->chg_count) {
			info->soca->Vbat[i] = 0;
			info->soca->Vsys[i] = 0;
			info->soca->Ibat[i] = 0;
		} else {
			info->soca->Vbat[i] = info->soca->Vbat[i-1];
			info->soca->Vsys[i] = info->soca->Vsys[i-1];
			info->soca->Ibat[i] = info->soca->Ibat[i-1];
		}
	}

	ret = measure_vbatt_FG(info, &info->soca->Vbat[0]);
	ret = measure_vsys_ADC(info, &info->soca->Vsys[0]);
	ret = measure_Ibatt_FG(info, &info->soca->Ibat[0]);

	info->soca->chg_count++;

	if (RICOH61x_GET_CHARGE_NUM != info->soca->chg_count) {
		queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
					 RICOH61x_CHARGE_CALC_TIME * HZ);
		mutex_unlock(&info->lock);
		return ;
	}

	for (i = 0; i < RICOH61x_GET_CHARGE_NUM; i++) {
		Vbat_sort[i] = info->soca->Vbat[i];
		Vsys_sort[i] = info->soca->Vsys[i];
		Ibat_sort[i] = info->soca->Ibat[i];
	}

	Vbat_temp = 0;
	Vsys_temp = 0;
	Ibat_temp = 0;
	for (i = 0; i < RICOH61x_GET_CHARGE_NUM - 1; i++) {
		for (j = RICOH61x_GET_CHARGE_NUM - 1; j > i; j--) {
			if (Vbat_sort[j - 1] > Vbat_sort[j]) {
				Vbat_temp = Vbat_sort[j];
				Vbat_sort[j] = Vbat_sort[j - 1];
				Vbat_sort[j - 1] = Vbat_temp;
			}
			if (Vsys_sort[j - 1] > Vsys_sort[j]) {
				Vsys_temp = Vsys_sort[j];
				Vsys_sort[j] = Vsys_sort[j - 1];
				Vsys_sort[j - 1] = Vsys_temp;
			}
			if (Ibat_sort[j - 1] > Ibat_sort[j]) {
				Ibat_temp = Ibat_sort[j];
				Ibat_sort[j] = Ibat_sort[j - 1];
				Ibat_sort[j - 1] = Ibat_temp;
			}
		}
	}

	Vbat_temp = 0;
	Vsys_temp = 0;
	Ibat_temp = 0;
	for (i = 3; i < RICOH61x_GET_CHARGE_NUM-3; i++) {
		Vbat_temp = Vbat_temp + Vbat_sort[i];
		Vsys_temp = Vsys_temp + Vsys_sort[i];
		Ibat_temp = Ibat_temp + Ibat_sort[i];
	}
	Vbat_temp = Vbat_temp / (RICOH61x_GET_CHARGE_NUM - 6);
	Vsys_temp = Vsys_temp / (RICOH61x_GET_CHARGE_NUM - 6);
	Ibat_temp = Ibat_temp / (RICOH61x_GET_CHARGE_NUM - 6);

	if (0 == info->soca->chg_count) {
		queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
				 RICOH61x_CHARGE_UPDATE_TIME * HZ);
		mutex_unlock(&info->lock);
		return;
	} else {
		info->soca->Vbat_ave = Vbat_temp;
		info->soca->Vsys_ave = Vsys_temp;
		info->soca->Ibat_ave = Ibat_temp;
	}

	info->soca->chg_count = 0;
	queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
				 RICOH61x_CHARGE_UPDATE_TIME * HZ);
	mutex_unlock(&info->lock);
	return;
}

/* Initial setting of FuelGauge SOCA function */
static int ricoh61x_init_fgsoca(struct ricoh61x_battery_info *info)
{
	int i;
	int err;
	uint8_t val;

	for (i = 0; i <= 10; i = i+1) {
		info->soca->ocv_table[i] = get_OCV_voltage(info, i, USING);
		printk(KERN_INFO "PMU: %s : * %d0%% voltage = %d uV\n",
				 __func__, i, info->soca->ocv_table[i]);
	}

	for (i = 0; i < 3; i = i+1)
		info->soca->reset_soc[i] = 0;
	info->soca->reset_count = 0;

	if (info->first_pwon) {

		err = ricoh61x_read(info->dev->parent, CHGISET_REG, &val);
		if (err < 0)
			dev_err(info->dev,
			"Error in read CHGISET_REG%d\n", err);

		err = ricoh61x_write(info->dev->parent, CHGISET_REG, 0);
		if (err < 0)
			dev_err(info->dev,
			"Error in writing CHGISET_REG%d\n", err);
		/* msleep(1000); */

		if (!info->entry_factory_mode) {
			err = ricoh61x_write(info->dev->parent,
							FG_CTRL_REG, 0x51);
			if (err < 0)
				dev_err(info->dev, "Error in writing the control register\n");
		}

		info->soca->rsoc_ready_flag = 1;

		/* msleep(6000); */

		err = ricoh61x_write(info->dev->parent, CHGISET_REG, val);
		if (err < 0)
			dev_err(info->dev,
			"Error in writing CHGISET_REG%d\n", err);
	}
	
	/* Rbat : Transfer */
	info->soca->Rbat = get_OCV_init_Data(info, 12, USING) * 1000 / 512
							 * 5000 / 4095;
	info->soca->n_cap = get_OCV_init_Data(info, 11, USING);


	//info->soca->displayed_soc = 0;
	//info->soca->last_displayed_soc = 0;
	info->soca->displayed_soc = calc_soc_by_voltageMethod(info);
	info->soca->last_displayed_soc = info->soca->displayed_soc;
	info->soca->suspend_soc = 0;
	info->soca->suspend_full_flg = false;
	info->soca->ready_fg = 0;
	info->soca->soc_delta = 0;
	info->soca->full_reset_count = 0;
	info->soca->soc_full = 0;
	info->soca->fc_cap = 0;
	info->soca->status = RICOH61x_SOCA_START;
	/* stable count down 11->2, 1: reset; 0: Finished; */
	info->soca->stable_count = 11;
	info->soca->dischg_state = 0;
	info->soca->Vbat_ave = 0;
	info->soca->Vbat_old = 0;
	info->soca->Vsys_ave = 0;
	info->soca->Ibat_ave = 0;
	info->soca->chg_count = 0;
	info->soca->hurry_up_flg = 0;
	info->soca->re_cap_old = 0;
	info->soca->jt_limit = 0;
	info->soca->zero_flg = 0;
	info->soca->cc_cap_offset = 0;
	info->soca->sus_cc_cap_offset = 0;
	info->soca->last_soc_full = 0;
	info->soca->rsoc_limit = 0;
	info->soca->last_cc_rrf0 = 0;
	info->soca->last_cc_delta_cap = 0;
	info->soca->last_cc_delta_cap_mas = 0;
	info->soca->temp_cc_delta_cap_mas = 0;
	info->soca->temp_cc_delta_cap     = 0;

	info->soca->store_fl_current = RICOH61x_FL_CURRENT_DEF;
	info->soca->store_slp_state = 0;
	info->soca->store_sus_current = RICOH61x_SUS_CURRENT_DEF;
	info->soca->store_hiber_current = RICOH61x_HIBER_CURRENT_DEF;

	for (i = 0; i < 11; i++) {
		info->soca->ocv_table_low[i] = 0;
	}

	for (i = 0; i < RICOH61x_GET_CHARGE_NUM; i++) {
		info->soca->Vbat[i] = 0;
		info->soca->Vsys[i] = 0;
		info->soca->Ibat[i] = 0;
	}

	/*********************************/
	//fl_level = RICOH61x_FL_LEVEL_DEF;
	//fl_current = RICOH61x_FL_CURRENT_DEF;
	//slp_state = 0;
	//idle_current = RICOH61x_IDLE_CURRENT_DEF;
	//sus_current = RICOH61x_SUS_CURRENT_DEF;
	//hiber_current = RICOH61x_HIBER_CURRENT_DEF;
	//bat_alert_req_flg = 0;
#ifdef STANDBY_MODE_DEBUG
	multiple_sleep_mode = 0;
#endif
	/*********************************/

#ifdef ENABLE_FG_KEEP_ON_MODE
	g_fg_on_mode = 1;
	info->soca->rsoc_ready_flag = 1;
#else
	g_fg_on_mode = 0;
#endif


	/* Start first Display job */
	if(info->first_pwon) {
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
						   RICOH61x_FG_RESET_TIME*HZ);
	}else {
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
						   RICOH61x_MAIN_START_TIME*HZ);
	}

	/* Start first Waiting stable job */
	queue_delayed_work(info->monitor_wqueue, &info->charge_stable_work,
		   RICOH61x_FG_STABLE_TIME*HZ/10);

	queue_delayed_work(info->monitor_wqueue, &info->charge_monitor_work,
					 RICOH61x_CHARGE_MONITOR_TIME * HZ);

	queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
					 RICOH61x_CHARGE_MONITOR_TIME * HZ);
	if (info->jt_en) {
		if (info->jt_hw_sw) {
			/* Enable JEITA function supported by H/W */
			err = ricoh61x_set_bits(info->dev->parent, CHGCTL1_REG, 0x04);
			if (err < 0)
				dev_err(info->dev, "Error in writing the control register\n");
		} else {
		 	/* Disable JEITA function supported by H/W */
			err = ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x04);
			if (err < 0)
				dev_err(info->dev, "Error in writing the control register\n");
			queue_delayed_work(info->monitor_wqueue, &info->jeita_work,
						 	 RICOH61x_FG_RESET_TIME * HZ);
		}
	} else {
		/* Disable JEITA function supported by H/W */
		err = ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x04);
		if (err < 0)
			dev_err(info->dev, "Error in writing the control register\n");
		if (0xff != info->ch_ilim_adp && (info->ch_ilim_adp <= 0x1D)) {
			/* REGISET1:(0xB6) setting */
			err = ricoh61x_write(info->dev->parent, REGISET1_REG, info->ch_ilim_adp);
			if (err < 0)
				dev_err(info->dev, "Error in writing REGISET1_REG %d\n",err);
			if (0xff != info->jt_ichg_h && (info->jt_ichg_h <= 0x1D)) {
				/* CHGISET:(0xB8) setting */
				err = ricoh61x_write(info->dev->parent, CHGISET_REG, info->jt_ichg_h);
				if (err < 0)
					dev_err(info->dev, "Error in writing CHGISET_REG %d\n",err);
			}
		}
	}

	printk(KERN_INFO "PMU: %s : * Rbat = %d mOhm   n_cap = %d mAH\n",
			 __func__, info->soca->Rbat, info->soca->n_cap);
	return 1;
}
#endif

static void ricoh61x_changed_work(struct work_struct *work)
{
	struct ricoh61x_battery_info *info = container_of(work,
		struct ricoh61x_battery_info, changed_work.work);

	printk(KERN_INFO "PMU: %s\n", __func__);
	power_supply_changed(info->battery);

	return;
}

static int check_jeita_status(struct ricoh61x_battery_info *info, bool *is_jeita_updated)
/*  JEITA Parameter settings
*
*          VCHG  
*            |     
* jt_vfchg_h~+~~~~~~~~~~~~~~~~~~~+
*            |                   |
* jt_vfchg_l-| - - - - - - - - - +~~~~~~~~~~+
*            |    Charge area    +          |               
*  -------0--+-------------------+----------+--- Temp
*            !                   +
*          ICHG     
*            |                   +
*  jt_ichg_h-+ - -+~~~~~~~~~~~~~~+~~~~~~~~~~+
*            +    |              +          |
*  jt_ichg_l-+~~~~+   Charge area           |
*            |    +              +          |
*         0--+----+--------------+----------+--- Temp
*            0   jt_temp_l      jt_temp_h   55
*/
{
	int temp;
	int err = 0;
	int vfchg;
	uint8_t chgiset_org;
	uint8_t batset2_org;
	uint8_t set_vchg_h, set_vchg_l;
	uint8_t set_ichg_h, set_ichg_l;

	*is_jeita_updated = false;
	/* No execute if JEITA disabled */
	if (!info->jt_en || info->jt_hw_sw)
		return 0;

	/* Check FG Reset */
	if (info->soca->ready_fg) {
		temp = get_battery_temp_2(info) / 10;
	} else {
		printk(KERN_INFO "JEITA: %s *** cannot update by resetting FG ******\n", __func__);
		goto out;
	}

	/* Read BATSET2 */
	err = ricoh61x_read(info->dev->parent, BATSET2_REG, &batset2_org);
	if (err < 0) {
		dev_err(info->dev, "Error in readng the battery setting register\n");
		goto out;
	}
	vfchg = (batset2_org & 0x70) >> 4;
	batset2_org &= 0x8F;
	
	/* Read CHGISET */
	err = ricoh61x_read(info->dev->parent, CHGISET_REG, &chgiset_org);
	if (err < 0) {
		dev_err(info->dev, "Error in readng the chrage setting register\n");
		goto out;
	}
	chgiset_org &= 0xC0;

	set_ichg_h = (uint8_t)(chgiset_org | info->jt_ichg_h);
	set_ichg_l = (uint8_t)(chgiset_org | info->jt_ichg_l);
		
	set_vchg_h = (uint8_t)((info->jt_vfchg_h << 4) | batset2_org);
	set_vchg_l = (uint8_t)((info->jt_vfchg_l << 4) | batset2_org);

	printk(KERN_INFO "PMU: %s *** Temperature: %d, vfchg: %d, SW status: %d, chg_status: %d ******\n",
		 __func__, temp, vfchg, info->soca->status, info->soca->chg_status);

	if (temp <= 0 || 55 <= temp) {
		/* 1st and 5th temperature ranges (~0, 55~) */
		printk(KERN_INFO "PMU: %s *** Temp(%d) is out of 0-55 ******\n", __func__, temp);
		err = ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto out;
		}
		info->soca->jt_limit = 0;
		*is_jeita_updated = true;
	} else if (temp < info->jt_temp_l) {
		/* 2nd temperature range (0~12) */
		if (vfchg != info->jt_vfchg_h) {
			printk(KERN_INFO "PMU: %s *** 0<Temp<12, update to vfchg=%d ******\n", 
									__func__, info->jt_vfchg_h);
			err = ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
				goto out;
			}

			/* set VFCHG/VRCHG */
			err = ricoh61x_write(info->dev->parent,
							 BATSET2_REG, set_vchg_h);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the battery setting register\n");
				goto out;
			}
			info->soca->jt_limit = 0;
			*is_jeita_updated = true;
		} else
			printk(KERN_INFO "PMU: %s *** 0<Temp<50, already set vfchg=%d, so no need to update ******\n",
					__func__, info->jt_vfchg_h);

		/* set ICHG */
		err = ricoh61x_write(info->dev->parent, CHGISET_REG, set_ichg_l);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the battery setting register\n");
			goto out;
		}
		err = ricoh61x_set_bits(info->dev->parent, CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto out;
		}
	} else if (temp < info->jt_temp_h) {
		/* 3rd temperature range (12~50) */
		if (vfchg != info->jt_vfchg_h) {
			printk(KERN_INFO "PMU: %s *** 12<Temp<50, update to vfchg==%d ******\n", __func__, info->jt_vfchg_h);

			err = ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
				goto out;
			}
			/* set VFCHG/VRCHG */
			err = ricoh61x_write(info->dev->parent,
							 BATSET2_REG, set_vchg_h);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the battery setting register\n");
				goto out;
			}
			info->soca->jt_limit = 0;
			*is_jeita_updated = true;
		} else
			printk(KERN_INFO "PMU: %s *** 12<Temp<50, already set vfchg==%d, so no need to update ******\n", 
					__func__, info->jt_vfchg_h);
		
		/* set ICHG */
		err = ricoh61x_write(info->dev->parent, CHGISET_REG, set_ichg_h);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the battery setting register\n");
			goto out;
		}
		err = ricoh61x_set_bits(info->dev->parent, CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto out;
		}
	} else if (temp < 55) {
		/* 4th temperature range (50~55) */
		if (vfchg != info->jt_vfchg_l) {
			printk(KERN_INFO "PMU: %s *** 50<Temp<55, update to vfchg==%d ******\n", __func__, info->jt_vfchg_l);
			
			err = ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
				goto out;
			}
			/* set VFCHG/VRCHG */
			err = ricoh61x_write(info->dev->parent,
							 BATSET2_REG, set_vchg_l);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the battery setting register\n");
				goto out;
			}
			info->soca->jt_limit = 1;
			*is_jeita_updated = true;
		} else
			printk(KERN_INFO "JEITA: %s *** 50<Temp<55, already set vfchg==%d, so no need to update ******\n", 
					__func__, info->jt_vfchg_l);

		/* set ICHG */
		err = ricoh61x_write(info->dev->parent, CHGISET_REG, set_ichg_h);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the battery setting register\n");
			goto out;
		}
		err = ricoh61x_set_bits(info->dev->parent, CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto out;
		}
	}

	get_power_supply_status(info);
	printk(KERN_INFO "PMU: %s *** Hope updating value in this timing after checking jeita, chg_status: %d, is_jeita_updated: %d ******\n",
		 __func__, info->soca->chg_status, *is_jeita_updated);

	return 0;
	
out:
	printk(KERN_INFO "PMU: %s ERROR ******\n", __func__);
	return err;
}

static void ricoh61x_jeita_work(struct work_struct *work)
{
	int ret;
	bool is_jeita_updated = false;
	struct ricoh61x_battery_info *info = container_of(work,
		struct ricoh61x_battery_info, jeita_work.work);

	mutex_lock(&info->lock);

	ret = check_jeita_status(info, &is_jeita_updated);
	if (0 == ret) {
		queue_delayed_work(info->monitor_wqueue, &info->jeita_work,
					 RICOH61x_JEITA_UPDATE_TIME * HZ);
	} else {
		printk(KERN_INFO "PMU: %s *** Call check_jeita_status() in jeita_work, err:%d ******\n", 
							__func__, ret);
		queue_delayed_work(info->monitor_wqueue, &info->jeita_work,
					 RICOH61x_FG_RESET_TIME * HZ);
	}

	mutex_unlock(&info->lock);

	if(true == is_jeita_updated)
		power_supply_changed(info->battery);

	return;
}

#ifdef ENABLE_FACTORY_MODE
/*------------------------------------------------------*/
/* Factory Mode						*/
/*    Check Battery exist or not			*/
/*    If not, disabled Rapid to Complete State change	*/
/*------------------------------------------------------*/
static int ricoh61x_factory_mode(struct ricoh61x_battery_info *info)
{
	int ret = 0;
	uint8_t val = 0;

	ret = ricoh61x_read(info->dev->parent, RICOH61x_INT_MON_CHGCTR, &val);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		return ret;
	}
	if (!(val & 0x01)) /* No Adapter connected */
		return ret;

	/* Rapid to Complete State change disable */
	ret = ricoh61x_set_bits(info->dev->parent, CHGCTL1_REG, 0x40);

	if (ret < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
		return ret;
	}

	/* Wait 1s for checking Charging State */
	queue_delayed_work(info->factory_mode_wqueue, &info->factory_mode_work,
			 1*HZ);

	return ret;
}

static void check_charging_state_work(struct work_struct *work)
{
	struct ricoh61x_battery_info *info = container_of(work,
		struct ricoh61x_battery_info, factory_mode_work.work);

	int ret = 0;
	uint8_t val = 0;
	int chargeCurrent = 0;

	ret = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &val);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		return;
	}


	chargeCurrent = get_check_fuel_gauge_reg(info, CC_AVERAGE1_REG,
						 CC_AVERAGE0_REG, 0x3fff);
	if (chargeCurrent < 0) {
		dev_err(info->dev, "Error in reading the FG register\n");
		return;
	}

	/* Repid State && Charge Current about 0mA */
	if (((chargeCurrent >= 0x3ffc && chargeCurrent <= 0x3fff)
		|| chargeCurrent < 0x05) && val == 0x43) {
		printk(KERN_INFO "PMU:%s --- No battery !! Enter Factory mode ---\n"
				, __func__);
		info->entry_factory_mode = true;
		/* clear FG_ACC bit */
		ret = ricoh61x_clr_bits(info->dev->parent, RICOH61x_FG_CTRL, 0x10);
		if (ret < 0)
			dev_err(info->dev, "Error in writing FG_CTRL\n");
		
		return;	/* Factory Mode */
	}

	/* Return Normal Mode --> Rapid to Complete State change enable */
	/* disable the status change from Rapid Charge to Charge Complete */

	ret = ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x40);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
		return;
	}
	printk(KERN_INFO "PMU:%s --- Battery exist !! Return Normal mode ---0x%2x\n"
			, __func__, val);

	return;
}
#endif /* ENABLE_FACTORY_MODE */

static int Calc_Linear_Interpolation(int x0, int y0, int x1, int y1, int y)
{
	int	alpha;
	int x;

	alpha = (y - y0)*100 / (y1 - y0);

	x = ((100 - alpha) * x0 + alpha * x1) / 100;

	return x;
}

static void ricoh61x_scaling_OCV_table(struct ricoh61x_battery_info *info, int cutoff_vol, int full_vol, int *start_per, int *end_per)
{
	int		i, j;
	int		temp;
	int		percent_step;
	int		OCV_percent_new[11];

	/* get ocv table. this table is calculated by Apprication */
	//printk("PMU : %s : original table\n",__func__);
	//for (i = 0; i <= 10; i = i+1) {
	//	printk(KERN_INFO "PMU: %s : %d0%% voltage = %d uV\n",
	//			 __func__, i, info->soca->ocv_table_def[i]);
	//}
	//printk("PMU: %s : cutoff_vol %d full_vol %d\n",
	//			 __func__, cutoff_vol,full_vol);

	/* Check Start % */
	if (info->soca->ocv_table_def[0] > cutoff_vol * 1000) {
		*start_per = 0;
		printk("PMU : %s : setting value of cuttoff_vol(%d) is out of range(%d) \n",__func__, cutoff_vol, info->soca->ocv_table_def[0]);
	} else {
		for (i = 1; i < 11; i++) {
			if (info->soca->ocv_table_def[i] >= cutoff_vol * 1000) {
				/* unit is 0.001% */
				*start_per = Calc_Linear_Interpolation(
					(i-1)*1000, info->soca->ocv_table_def[i-1], i*1000,
					info->soca->ocv_table_def[i], (cutoff_vol * 1000));
				break;
			}
		}
	}

	/* Check End % */
	for (i = 1; i < 11; i++) {
		if (info->soca->ocv_table_def[i] >= full_vol * 1000) {
			/* unit is 0.001% */
			*end_per = Calc_Linear_Interpolation(
				(i-1)*1000, info->soca->ocv_table_def[i-1], i*1000,
				 info->soca->ocv_table_def[i], (full_vol * 1000));
			break;
		}
	}

	/* calc new ocv percent */
	percent_step = ( *end_per - *start_per) / 10;
	//printk("PMU : %s : percent_step is %d end per is %d start per is %d\n",__func__, percent_step, *end_per, *start_per);

	for (i = 0; i < 11; i++) {
		OCV_percent_new[i]
			 = *start_per + percent_step*(i - 0);
	}

	/* calc new ocv voltage */
	for (i = 0; i < 11; i++) {
		for (j = 1; j < 11; j++) {
			if (1000*j >= OCV_percent_new[i]) {
				temp = Calc_Linear_Interpolation(
					info->soca->ocv_table_def[j-1], (j-1)*1000,
					info->soca->ocv_table_def[j] , j*1000,
					 OCV_percent_new[i]);

				temp = ( (temp/1000) * 4095 ) / 5000;

				battery_init_para[info->num][i*2 + 1] = temp;
				battery_init_para[info->num][i*2] = temp >> 8;

				break;
			}
		}
	}
	printk("PMU : %s : new table\n",__func__);
	for (i = 0; i <= 10; i = i+1) {
		temp = (battery_init_para[info->num][i*2]<<8)
			 | (battery_init_para[info->num][i*2+1]);
		/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
		temp = ((temp * 50000 * 10 / 4095) + 5) / 10;
		printk("PMU : %s : ocv_table %d is %d v\n",__func__, i, temp);
	}

}

static int ricoh61x_set_OCV_table(struct ricoh61x_battery_info *info)
{
	int		ret = 0;
	int		i;
	int		full_ocv;
	int		available_cap;
	int		available_cap_ori;
	int		temp;
	int		temp1;
	int		start_per = 0;
	int		end_per = 0;
	int		Rbat;
	int		Ibat_min;
	uint8_t val;
	uint8_t val2;
	uint8_t val_temp;


	//get ocv table 
	for (i = 0; i <= 10; i = i+1) {
		info->soca->ocv_table_def[i] = get_OCV_voltage(info, i, USING);
		printk(KERN_INFO "PMU: %s : %d0%% voltage = %d uV\n",
			 __func__, i, info->soca->ocv_table_def[i]);
	}

	//save original header file data 
	for (i = 0; i < 32; i++){
		info->soca->battery_init_para_original[i] = battery_init_para[info->num][i];
	}

	temp =  (battery_init_para[info->num][24]<<8) | (battery_init_para[info->num][25]);
	Rbat = temp * 1000 / 512 * 5000 / 4095;
	info->soca->Rsys = Rbat + 55;

	if ((info->fg_target_ibat == 0) || (info->fg_target_vsys == 0)) {	/* normal version */

		temp =  (battery_init_para[info->num][22]<<8) | (battery_init_para[info->num][23]);
		//fa_cap = get_check_fuel_gauge_reg(info, FA_CAP_H_REG, FA_CAP_L_REG,
		//				0x7fff);

		info->soca->target_ibat = temp*2/10; /* calc 0.2C*/
		temp1 =  (battery_init_para[info->num][0]<<8) | (battery_init_para[info->num][1]);
//		temp = get_OCV_voltage(info, 0) / 1000; /* unit is 1mv*/
//		info->soca->cutoff_ocv = info->soca->target_vsys - Ibat_min * info->soca->Rsys / 1000;

		info->soca->target_vsys = temp1 + ( info->soca->target_ibat * info->soca->Rsys ) / 1000;
		

	} else {
		info->soca->target_ibat = info->fg_target_ibat;
		/* calc min vsys value */
		temp1 =  (battery_init_para[info->num][0]<<8) | (battery_init_para[info->num][1]);
		temp = temp1 + ( info->soca->target_ibat * info->soca->Rsys ) / 1000;
		if( temp < info->fg_target_vsys) {
			info->soca->target_vsys = info->fg_target_vsys;
		} else {
			info->soca->target_vsys = temp;
			printk("PMU : %s : setting value of target vsys(%d) is out of range(%d)\n",__func__, info->fg_target_vsys, temp);
		}
	}

	//for debug
	printk("PMU : %s : target_vsys is %d target_ibat is %d\n",__func__,info->soca->target_vsys,info->soca->target_ibat);
	
	if ((info->soca->target_ibat == 0) || (info->soca->target_vsys == 0)) {	/* normal version */
	} else {	/*Slice cutoff voltage version. */

		Ibat_min = -1 * info->soca->target_ibat;
		info->soca->cutoff_ocv = info->soca->target_vsys - Ibat_min * info->soca->Rsys / 1000;
		
		full_ocv = (battery_init_para[info->num][20]<<8) | (battery_init_para[info->num][21]);
		full_ocv = full_ocv * 5000 / 4095;

		ricoh61x_scaling_OCV_table(info, info->soca->cutoff_ocv, full_ocv, &start_per, &end_per);

		/* calc available capacity */
		/* get avilable capacity */
		/* battery_init_para23-24 is designe capacity */
		available_cap = (battery_init_para[info->num][22]<<8)
					 | (battery_init_para[info->num][23]);

		available_cap = available_cap
			 * ((10000 - start_per) / 100) / 100 ;


		battery_init_para[info->num][23] =  available_cap;
		battery_init_para[info->num][22] =  available_cap >> 8;

	}
	ret = ricoh61x_clr_bits(info->dev->parent, FG_CTRL_REG, 0x01);
	if (ret < 0) {
		dev_err(info->dev, "error in FG_En off\n");
		goto err;
	}
	/////////////////////////////////
	ret = ricoh61x_read_bank1(info->dev->parent, 0xDC, &val);
	if (ret < 0) {
		dev_err(info->dev, "batterry initialize error\n");
		goto err;
	}

	val_temp = val;
	val	&= 0x0F; //clear bit 4-7
	val	|= 0x10; //set bit 4
	
	ret = ricoh61x_write_bank1(info->dev->parent, 0xDC, val);
	if (ret < 0) {
		dev_err(info->dev, "batterry initialize error\n");
		goto err;
	}
	
	ret = ricoh61x_read_bank1(info->dev->parent, 0xDC, &val2);
	if (ret < 0) {
		dev_err(info->dev, "batterry initialize error\n");
		goto err;
	}

	ret = ricoh61x_write_bank1(info->dev->parent, 0xDC, val_temp);
	if (ret < 0) {
		dev_err(info->dev, "batterry initialize error\n");
		goto err;
	}

	//printk("PMU : %s : original 0x%x, before 0x%x, after 0x%x\n",__func__, val_temp, val, val2);
	
	if (val != val2) {
		ret = ricoh61x_bulk_writes_bank1(info->dev->parent,
				BAT_INIT_TOP_REG, 30, battery_init_para[info->num]);
		if (ret < 0) {
			dev_err(info->dev, "batterry initialize error\n");
			goto err;
		}
	} else {
		ret = ricoh61x_read_bank1(info->dev->parent, 0xD2, &val);
		if (ret < 0) {
		dev_err(info->dev, "batterry initialize error\n");
		goto err;
		}
	
		ret = ricoh61x_read_bank1(info->dev->parent, 0xD3, &val2);
		if (ret < 0) {
			dev_err(info->dev, "batterry initialize error\n");
			goto err;
		}
		
		available_cap_ori = val2 + (val << 8);
		available_cap = battery_init_para[info->num][23]
						+ (battery_init_para[info->num][22] << 8);

		if (available_cap_ori == available_cap) {
			ret = ricoh61x_bulk_writes_bank1(info->dev->parent,
				BAT_INIT_TOP_REG, 22, battery_init_para[info->num]);
			if (ret < 0) {
				dev_err(info->dev, "batterry initialize error\n");
				return ret;
			}
			
			for (i = 0; i < 6; i++) {
				ret = ricoh61x_write_bank1(info->dev->parent, 0xD4+i, battery_init_para[info->num][24+i]);
				if (ret < 0) {
					dev_err(info->dev, "batterry initialize error\n");
					return ret;
				}
			}
		} else {
			ret = ricoh61x_bulk_writes_bank1(info->dev->parent,
				BAT_INIT_TOP_REG, 30, battery_init_para[info->num]);
			if (ret < 0) {
				dev_err(info->dev, "batterry initialize error\n");
				goto err;
			}
		}
	}

	////////////////////////////////

	return 0;
err:
	return ret;
}

/* Initial setting of battery */
static int ricoh61x_init_battery(struct ricoh61x_battery_info *info)
{
	int ret = 0;
	uint8_t val;
	uint8_t val2;
	unsigned long hour_power_off;
	unsigned long hour_power_on;
	long power_off_period;
	unsigned long seconds;
	int cc_cap = 0;
	long cc_cap_mas = 0;
	bool is_charging = true;
	
	/* Need to implement initial setting of batery and error */
	/* -------------------------- */
#ifdef ENABLE_FUEL_GAUGE_FUNCTION

	/* set relaxation state */
	if (RICOH61x_REL1_SEL_VALUE > 240)
		val = 0x0F;
	else
		val = RICOH61x_REL1_SEL_VALUE / 16 ;

	/* set relaxation state */
	if (RICOH61x_REL2_SEL_VALUE > 120)
		val2 = 0x0F;
	else
		val2 = RICOH61x_REL2_SEL_VALUE / 8 ;

	val =  val + (val2 << 4);

	//ret = ricoh61x_write_bank1(info->dev->parent, BAT_REL_SEL_REG, val);
	ret = ricoh61x_write_bank1(info->dev->parent, BAT_REL_SEL_REG, 0);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing BAT_REL_SEL_REG\n");
		return ret;
	}

	ret = ricoh61x_read_bank1(info->dev->parent, BAT_REL_SEL_REG, &val);
	//printk("PMU: -------  BAT_REL_SEL= %xh: =======\n",
	//	val);

	ret = ricoh61x_write_bank1(info->dev->parent, BAT_TA_SEL_REG, 0x00);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing BAT_TA_SEL_REG\n");
		return ret;
	}

	//check first power on condition
	//initial value
	info->first_pwon = 0;
	ret = ricoh61x_read(info->dev->parent, PSWR_REG, &val);
	if (ret < 0) {
		dev_err(info->dev,"Error in reading PSWR_REG %d\n", ret);
		return ret;
	}

	g_soc = val & 0x7f;	
	info->soca->init_pswr = val & 0x7f;
	printk("PMU FG_RESET : %s : initial pswr = %d\n",__func__,info->soca->init_pswr);

	if(val == 0){
		printk("PMU : %s : first attached battery\n", __func__);
		ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 1);
		info->first_pwon = 1;
	}

	//info->first_pwon = (val == 0) ? 1 : 0;

	ret = ricoh61x_read(info->dev->parent, RICOH61x_PWR_OFF_HIS, &val);
	if (ret < 0) {
		dev_err(info->dev,"Error in reading PWER OFF HIS %d\n", ret);
		return ret;
	}
	printk("PMU : %s : POWER off history 0x%02x is 0x%02x \n", __func__,RICOH61x_PWR_OFF_HIS ,val);
	//check bit 0 status
	if(val & 0x01){
		printk("PMU : %s : Long power on key press\n", __func__);
		info->first_pwon = 1;
	}

	ret = ricoh61x_read(info->dev->parent, RICOH61x_PWR_FUNC, &val);
	if (ret < 0) {
		dev_err(info->dev,"Error in reading PWER FUNC %d\n", ret);
		return ret;
	}
	printk("PMU : %s : POWER control function 0x%02x is 0x%02x \n", __func__,RICOH61x_PWR_FUNC ,val);
#if 0
	//check all bit is clear or not
	if((val & 0xFF) == 0){
		printk("PMU : %s : cold boot\n", __func__);
		info->first_pwon = 1;
	}
#endif
	//end first power on condition
	
	if(info->first_pwon == 0){
		//check Power off period
		//if upper 1day, this power on sequence become first power on
		hour_power_off = get_storedTime_from_register(info);
		get_current_time(info, &seconds);
		hour_power_on  = seconds / 3600;

		hour_power_on &= 0xFFFFFF;
		
		power_off_period = hour_power_on - hour_power_off;
		if(power_off_period >= 24) {
			bat_alert_req_flg = 1;
		} else if(power_off_period < 0){
			//error case
			bat_alert_req_flg = 1;
		} else {
			bat_alert_req_flg = 0;
		}
		printk("PMU : %s : off is %lu, on is %lu, period is %lu, fpon_flag is %d\n", __func__, hour_power_off, hour_power_on, power_off_period, info->first_pwon);
	}

	if(info->first_pwon) {
		info->soca->rsoc_ready_flag = 1;
	}else {
		info->soca->rsoc_ready_flag = 0;
	}

	ret = ricoh61x_set_OCV_table(info);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the OCV Tabler\n");
		return ret;
	}

	ret = ricoh61x_write(info->dev->parent, FG_CTRL_REG, 0x11);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
		return ret;
	}

	
	Enable_Test_Register(info);

#endif

#if 1
	ret = ricoh61x_write(info->dev->parent, VINDAC_REG, 0x0);
	//ret = ricoh61x_write(info->dev->parent, VINDAC_REG, 0x01);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
		return ret;
	}
#endif

#if 0
	if (info->alarm_vol_mv < 2700 || info->alarm_vol_mv > 3400) {
		dev_err(info->dev, "alarm_vol_mv is out of range!\n");
		return -1;
	}
#endif

	return ret;
}

/* Initial setting of charger */
static int ricoh61x_init_charger(struct ricoh61x_battery_info *info)
{
	int err;
	uint8_t val;
	uint8_t val2;
	uint8_t val3;
	int charge_status;
	int	vfchg_val;
	int	icchg_val;
	int	rbat;
	int	temp;

	info->chg_ctr = 0;
	info->chg_stat1 = 0;

	err = ricoh61x_set_bits(info->dev->parent, RICOH61x_PWR_FUNC, 0x20);
	if (err < 0) {
		dev_err(info->dev, "Error in writing the PWR FUNC register\n");
		goto free_device;
	}

	charge_status = get_power_supply_status(info);

	if (charge_status != POWER_SUPPLY_STATUS_FULL)
	{
		/* Disable charging */
		err = ricoh61x_clr_bits(info->dev->parent,CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto free_device;
		}
	}

	
	err = ricoh61x_read(info->dev->parent, 0xDA, &val);
	printk("PMU : %s : GCHGDET (0xDA) is 0x%x\n",__func__,val);
	if (val & 0x30) {
		/* REGISET1:(0xB6) setting */
		if ((info->ch_ilim_adp != 0xFF) || (info->ch_ilim_adp <= 0x1D)) {
			val = info->ch_ilim_adp;

		}
		else
			val = 0x0D;
		err = ricoh61x_write(info->dev->parent, REGISET1_REG,val);
		if (err < 0) {
			dev_err(info->dev, "Error in writing REGISET1_REG %d\n",
										 err);
			goto free_device;
		}
	}

		/* REGISET2:(0xB7) setting */
	err = ricoh61x_read(info->dev->parent, REGISET2_REG, &val);
	if (err < 0) {
		dev_err(info->dev,
	 	"Error in read REGISET2_REG %d\n", err);
		goto free_device;
	}
	
	if ((info->ch_ilim_usb != 0xFF) || (info->ch_ilim_usb <= 0x1D)) {
		val2 = info->ch_ilim_usb;
	} else {/* Keep OTP value */
		val2 = (val & 0x1F);
	}

		/* keep bit 5-7 */
	val &= 0xE0;
	
	val = val + val2;
	
	val |= 0xA0;		// Set  SDPOVRLIM to allow charge limit 500mA 
	
	err = ricoh61x_write(info->dev->parent, REGISET2_REG,val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing REGISET2_REG %d\n",
									 err);
		goto free_device;
	}

	err = ricoh61x_read(info->dev->parent, CHGISET_REG, &val);
	if (err < 0) {
		dev_err(info->dev,
	 	"Error in read CHGISET_REG %d\n", err);
		goto free_device;
	}

		/* Define Current settings value for charging (bit 4~0)*/
	if ((info->ch_ichg != 0xFF) || (info->ch_ichg <= 0x1D)) {
		val2 = info->ch_ichg;
	} else { /* Keep OTP value */
		val2 = (val & 0x1F);
	}

		/* Define Current settings at the charge completion (bit 7~6)*/
	if ((info->ch_icchg != 0xFF) || (info->ch_icchg <= 0x03)) {
		val3 = info->ch_icchg << 6;
	} else { /* Keep OTP value */
		val3 = (val & 0xC0);
	}

	val = val2 + val3;

	err = ricoh61x_write(info->dev->parent, CHGISET_REG, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing CHGISET_REG %d\n",
									 err);
		goto free_device;
	}

		//debug messeage
	err = ricoh61x_read(info->dev->parent, CHGISET_REG,&val);
	printk("PMU : %s : after CHGISET_REG (0x%x) is 0x%x info->ch_ichg is 0x%x info->ch_icchg is 0x%x\n",__func__,CHGISET_REG,val,info->ch_ichg,info->ch_icchg);

		//debug messeage
	err = ricoh61x_read(info->dev->parent, BATSET1_REG,&val);
	printk("PMU : %s : before BATSET1_REG (0x%x) is 0x%x info->ch_vbatovset is 0x%x\n",__func__,BATSET1_REG,val,info->ch_vbatovset);
	
	/* BATSET1_REG(0xBA) setting */
	err = ricoh61x_read(info->dev->parent, BATSET1_REG, &val);
	if (err < 0) {
		dev_err(info->dev,
	 	"Error in read BATSET1 register %d\n", err);
		goto free_device;
	}

		/* Define Battery overvoltage  (bit 4)*/
	if ((info->ch_vbatovset != 0xFF) || (info->ch_vbatovset <= 0x1)) {
		val2 = info->ch_vbatovset;
		val2 = val2 << 4;
	} else { /* Keep OTP value */
		val2 = (val & 0x10);
	}
	
		/* keep bit 0-3 and bit 5-7 */
	val = (val & 0xEF);
	
	val = val + val2;

	val |= 0x08;	// set vweak to 3.3

	if (49 == gptHWCFG->m_val.bPCB)  //E60QDX
		val |= 0x80;	// set CHGPON to 3.3V

	err = ricoh61x_write(info->dev->parent, BATSET1_REG, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing BAT1_REG %d\n",
									 err);
		goto free_device;
	}

	if (49 == gptHWCFG->m_val.bPCB)  //E60QDX
		err = ricoh61x_write(info->dev->parent, 0xB4, 0x20);	// set USB_VCONTMASK to enable CHGPON

		//debug messeage
	err = ricoh61x_read(info->dev->parent, BATSET1_REG,&val);
	printk("PMU : %s : after BATSET1_REG (0x%x) is 0x%x info->ch_vbatovset is 0x%x\n",__func__,BATSET1_REG,val,info->ch_vbatovset);
	
		//debug messeage
	err = ricoh61x_read(info->dev->parent, BATSET2_REG,&val);
	printk("PMU : %s : before BATSET2_REG (0x%x) is 0x%x info->ch_vrchg is 0x%x info->ch_vfchg is 0x%x \n",__func__,BATSET2_REG,val,info->ch_vrchg,info->ch_vfchg);

	
	/* BATSET2_REG(0xBB) setting */
	err = ricoh61x_read(info->dev->parent, BATSET2_REG, &val);
	if (err < 0) {
		dev_err(info->dev,
	 	"Error in read BATSET2 register %d\n", err);
		goto free_device;
	}

		/* Define Re-charging voltage (bit 2~0)*/
	if ((info->ch_vrchg != 0xFF) || (info->ch_vrchg <= 0x04)) {
		val2 = info->ch_vrchg;
	} else { /* Keep OTP value */
		val2 = (val & 0x07);
	}

		/* Define FULL charging voltage (bit 6~4)*/
	if ((info->ch_vfchg != 0xFF) || (info->ch_vfchg <= 0x04)) {
		val3 = info->ch_vfchg;
		val3 = val3 << 4;
	} else {	/* Keep OTP value */
		val3 = (val & 0x70);
	}

		/* keep bit 3 and bit 7 */
	val = (val & 0x88);
	
	val = val + val2 + val3;

	err = ricoh61x_write(info->dev->parent, BATSET2_REG, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing RICOH61x_RE_CHARGE_VOLTAGE %d\n",
									 err);
		goto free_device;
	}

	/* Set rising edge setting ([1:0]=01b)for INT in charging */
	/*  and rising edge setting ([3:2]=01b)for charge completion */
	err = ricoh61x_read(info->dev->parent, RICOH61x_CHG_STAT_DETMOD1, &val);
	if (err < 0) {
		dev_err(info->dev, "Error in reading CHG_STAT_DETMOD1 %d\n",
								 err);
		goto free_device;
	}
	val &= 0xf0;
	val |= 0x05;
	err = ricoh61x_write(info->dev->parent, RICOH61x_CHG_STAT_DETMOD1, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing CHG_STAT_DETMOD1 %d\n",
								 err);
		goto free_device;
	}

	/* Unmask In charging/charge completion */
	err = ricoh61x_write(info->dev->parent, RICOH61x_INT_MSK_CHGSTS1, 0xfc);
	if (err < 0) {
		dev_err(info->dev, "Error in writing INT_MSK_CHGSTS1 %d\n",
								 err);
		goto free_device;
	}

	/* Set both edge for VUSB([3:2]=11b)/VADP([1:0]=11b) detect */
	err = ricoh61x_read(info->dev->parent, RICOH61x_CHG_CTRL_DETMOD1, &val);
	if (err < 0) {
		dev_err(info->dev, "Error in reading CHG_CTRL_DETMOD1 %d\n",
								 err);
		goto free_device;
	}
	val &= 0xf0;
	val |= 0x0f;
	err = ricoh61x_write(info->dev->parent, RICOH61x_CHG_CTRL_DETMOD1, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing CHG_CTRL_DETMOD1 %d\n",
								 err);
		goto free_device;
	}

	/* Unmask In VUSB/VADP completion */
	err = ricoh61x_write(info->dev->parent, RICOH61x_INT_MSK_CHGCTR, 0xfc);
	if (err < 0) {
		dev_err(info->dev, "Error in writing INT_MSK_CHGSTS1 %d\n",
								 err);
		goto free_device;
	}
	
	if (charge_status != POWER_SUPPLY_STATUS_FULL)
	{
		/* Enable charging */
		err = ricoh61x_set_bits(info->dev->parent,CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto free_device;
		}
	}
	/* get OCV100_min, OCV100_min*/
	temp = (battery_init_para[info->num][24]<<8) | (battery_init_para[info->num][25]);
	rbat = temp * 1000 / 512 * 5000 / 4095;
	
	/* get vfchg value */
	err = ricoh61x_read(info->dev->parent, BATSET2_REG, &val);
	if (err < 0) {
		dev_err(info->dev, "Error in reading the batset2reg\n");
		goto free_device;
	}
	val &= 0x70;
	val2 = val >> 4;
	if (val2 <= 3) {
		vfchg_val = 4050 + val2 * 50;
	} else {
		vfchg_val = 4350;
	}
	printk("PMU : %s : test test val %d, val2 %d vfchg %d\n", __func__, val, val2, vfchg_val);

	/* get  value */
	err = ricoh61x_read(info->dev->parent, CHGISET_REG, &val);
	if (err < 0) {
		dev_err(info->dev, "Error in reading the chgisetreg\n");
		goto free_device;
	}
	val &= 0xC0;
	val2 = val >> 6;
	icchg_val = 50 + val2 * 50;
	printk("PMU : %s : test test val %d, val2 %d icchg %d\n", __func__, val, val2, icchg_val);

	info->soca->OCV100_min = ( vfchg_val * 99 / 100 - (icchg_val * (rbat +20))/1000 - 20 ) * 1000;
	info->soca->OCV100_max = ( vfchg_val * 101 / 100 - (icchg_val * (rbat +20))/1000 + 20 ) * 1000;
	
	printk("PMU : %s : 100 min %d, 100 max %d vfchg %d icchg %d rbat %d\n",__func__,
	info->soca->OCV100_min,info->soca->OCV100_max,vfchg_val,icchg_val,rbat);

#ifdef ENABLE_LOW_BATTERY_DETECTION
	/* Set ADRQ=00 to stop ADC */
	ricoh61x_write(info->dev->parent, RICOH61x_ADC_CNT3, 0x0);
#if 0
	/* Enable VSYS threshold Low interrupt */
	ricoh61x_write(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x10);
	/* Set ADC auto conversion interval 250ms */
	ricoh61x_write(info->dev->parent, RICOH61x_ADC_CNT2, 0x0);
	/* Enable VSYS pin conversion in auto-ADC */
	ricoh61x_write(info->dev->parent, RICOH61x_ADC_CNT1, 0x10);
	/* Set VSYS threshold low voltage value = (voltage(V)*255)/(3*2.5) */
	val = info->alarm_vol_mv * 255 / 7500;
	ricoh61x_write(info->dev->parent, RICOH61x_ADC_VSYS_THL, val);
#else
	/* Enable VBAT threshold Low interrupt */
	ricoh61x_write(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x02);
	/* Set ADC auto conversion interval 250ms */
	ricoh61x_write(info->dev->parent, RICOH61x_ADC_CNT2, 0x0);
	/* Enable VBAT pin conversion in auto-ADC */
	ricoh61x_write(info->dev->parent, RICOH61x_ADC_CNT1, 0x12);
	/* Set VBAT threshold low voltage value = (voltage(V)*255)/(2*2.5) */
	val = (info->alarm_vol_mv) * 255 / 5000;
	ricoh61x_write(info->dev->parent, RICOH61x_ADC_VBAT_THL, val);
#endif
#ifdef ENABLE_BATTERY_TEMP_DETECTION
	if(info->iEnableTempDetect) {
		/* Enable VTHM threshold Low interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_ADC_CNT1, 0x20);
		/* Enable VBAT threshold Low interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x20);
		/* Set VTHM threshold low voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_40_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THL, val);
		/* Enable VTHM threshold high interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC2, 0x20);
		/* Set VTHM threshold high voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_8_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THH, val);
	}
#endif

	/* Start auto-mode & average 4-time conversion mode for ADC */
	ricoh61x_write(info->dev->parent, RICOH61x_ADC_CNT3, 0x28);

#endif

free_device:
	return err;
}


static int get_power_supply_status(struct ricoh61x_battery_info *info)
{
	uint8_t status;
	uint8_t supply_state;
	uint8_t charge_state;
	int ret = 0;

	/* get  power supply status */
	ret = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &status);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		return ret;
	}
	charge_state = (status & 0x1F);
	supply_state = ((status & 0xC0) >> 6);

	if (info->entry_factory_mode)
			return POWER_SUPPLY_STATUS_NOT_CHARGING;

	if (supply_state == SUPPLY_STATE_BAT) {
		info->soca->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else {
		switch (charge_state) {
		case	CHG_STATE_CHG_OFF:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_DISCHARGING;
				break;
		case	CHG_STATE_CHG_READY_VADP:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_CHG_TRICKLE:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_CHARGING;
				break;
		case	CHG_STATE_CHG_RAPID:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_CHARGING;
				break;
		case	CHG_STATE_CHG_COMPLETE:
				if (info->chg_stat1 & 0x04) {
					printk ("[%s-%d] report charging because charge overtime.\n", __func__, __LINE__);
					info->soca->chg_status
						= POWER_SUPPLY_STATUS_CHARGING;
				}
				else
					info->soca->chg_status
						= POWER_SUPPLY_STATUS_FULL;
				break;
		case	CHG_STATE_SUSPEND:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_DISCHARGING;
				break;
		case	CHG_STATE_VCHG_OVER_VOL:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_DISCHARGING;
				break;
		case	CHG_STATE_BAT_ERROR:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_NO_BAT:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_BAT_OVER_VOL:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_BAT_TEMP_ERR:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_DIE_ERR:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_DIE_SHUTDOWN:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_DISCHARGING;
				break;
		case	CHG_STATE_NO_BAT2:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_CHG_READY_VUSB:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		default:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_UNKNOWN;
				break;
		}
	}

	return info->soca->chg_status;
}

static int get_power_supply_Android_status(struct ricoh61x_battery_info *info)
{

	get_power_supply_status(info);

	/* get  power supply status */
	if (info->entry_factory_mode)
			return POWER_SUPPLY_STATUS_NOT_CHARGING;

	switch (info->soca->chg_status) {
		case	POWER_SUPPLY_STATUS_UNKNOWN:
				return POWER_SUPPLY_STATUS_UNKNOWN;
				break;

		case	POWER_SUPPLY_STATUS_NOT_CHARGING:
				return POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;

		case	POWER_SUPPLY_STATUS_DISCHARGING:
				return POWER_SUPPLY_STATUS_DISCHARGING;
				break;

		case	POWER_SUPPLY_STATUS_CHARGING:
				return POWER_SUPPLY_STATUS_CHARGING;
				break;

		case	POWER_SUPPLY_STATUS_FULL:
				if(info->soca->displayed_soc == 100 * 100) {
					return POWER_SUPPLY_STATUS_FULL;
				} else {
					return POWER_SUPPLY_STATUS_CHARGING;
				}
				break;
		default:
				return POWER_SUPPLY_STATUS_UNKNOWN;
				break;
	}

	return POWER_SUPPLY_STATUS_UNKNOWN;
}

int ricoh619_dcin_status(void)
{
	return giRICOH619_DCIN;
}

int ricoh619_is_sdp(void)
{
	return ((DCP_CHARGER == giRICOH619_DCIN) || (CDP_CHARGER == giRICOH619_DCIN))?0:1;
}

static void charger_irq_work(struct work_struct *work)
{
	struct ricoh61x_battery_info *info
		 = container_of(work, struct ricoh61x_battery_info, irq_work);
	//uint8_t status;
	int ret = 0;
	uint8_t val = 0 ;
	//uint8_t adp_current_val = 0x0E;
	//uint8_t usb_current_val = 0x04;
	extern void led_red(int isOn);
	
//	printk(KERN_INFO "PMU:%s In\n", __func__);


	power_supply_changed(info->battery);

#if defined (STANDBY_MODE_DEBUG)
	ret = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &val);
	if (val & 0xc0) {
		if(multiple_sleep_mode == 0) {
			multiple_sleep_mode = 1;
			printk("PMU: %s sleep time ratio = x60 *****************\n", __func__);
		} else if(multiple_sleep_mode == 1) {
			multiple_sleep_mode = 2;
			printk("PMU: %s sleep time ratio = x3600 *****************\n", __func__);
		} else if(multiple_sleep_mode == 2) {
			multiple_sleep_mode = 0;
			printk("PMU: %s sleep time ratio = x1 *****************\n", __func__);
		}
	}
#elif defined(CHANGE_FL_MODE_DEBUG)
	ret = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &val);
	if (val & 0xc0) {
		if (fl_current < 10000) {
			fl_current = 10000;	// If FL<10mA, Set FL=10mA
		} else if (fl_current < 20000) {
			fl_current = 20000;	// If FL<20mA, Set FL=20mA
		} else if (fl_current < 30000) {
			fl_current = 30000;	// If FL<30mA, Set FL=30mA
		} else if (fl_current < 40000) {
			fl_current = 40000;	// If FL<40mA, Set FL=40mA
		} else {
			fl_current = 5000;	// If FL>40mA, Set FL=5mA
		}
		printk("PMU: %s FL(%d) mA *****************\n", __func__, fl_current/1000);
	}
#endif
//	mutex_lock(&info->lock);

#if 0	
	if(info->chg_ctr & 0x02) {
		uint8_t sts;
		ret = ricoh61x_read(info->dev->parent, RICOH61x_INT_MON_CHGCTR, &sts);
		if (ret < 0)
			dev_err(info->dev, "Error in reading the control register\n");

		sts &= 0x02;

		/* If "sts" is true, USB is plugged. If not, unplugged. */
	}
#endif		
	if (0x04 & info->chg_stat1) {
		int err;
		printk(KERN_INFO "PMU:charge overtime int triggered...\n", __func__);

		/* Disable charging */
		err = ricoh61x_clr_bits(info->dev->parent,CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
		}

		err = ricoh61x_read(info->dev->parent, TIMSET_REG, &val);
		if (err < 0)
			dev_err(info->dev,
			"Error in read TIMSET_REG%d\n", err);
		/* set rapid timer 240 -> 300 */
		err = ricoh61x_set_bits(info->dev->parent, TIMSET_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
		}
		/* to check bit 0-1 */
		if (0x03 == (val & 0x03)) {
			/* set rapid timer 300 -> 240 */
			err = ricoh61x_clr_bits(info->dev->parent, TIMSET_REG, 0x01);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
			}
		}

		/* Enable charging */
		err = ricoh61x_set_bits(info->dev->parent,CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
		}
	}
	info->chg_ctr = 0;
	info->chg_stat1 = 0;

	/* Enable Interrupt for VADP/VUSB */
	ret = ricoh61x_write(info->dev->parent, RICOH61x_INT_MSK_CHGCTR, 0xfc);
	if (ret < 0)
		dev_err(info->dev,
			 "%s(): Error in enable charger mask INT %d\n",
			 __func__, ret);

	/* Enable Interrupt for Charging & complete */
	ret = ricoh61x_write(info->dev->parent, RICOH61x_INT_MSK_CHGSTS1, 0xfc);
	if (ret < 0)
		dev_err(info->dev,
			 "%s(): Error in enable charger mask INT %d\n",
			 __func__, ret);

	/* set USB/ADP ILIM */
#if 0
	ret = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &val);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		return;
	}

	val = (val & 0xC0) >> 6;
	switch (val) {
	case	0: // plug out USB/ADP
			printk("%s : val = %d plug out\n",__func__, val);
			break;
	case	1: // plug in ADP
			printk("%s : val = %d plug in ADPt\n",__func__, val);
			//Add the code of AC adapter Charge and Limit current settings
			//ret = ricoh61x_write(info->dev->parent, REGISET1_REG, adp_current_val);
			break;
	case	2:// plug in USB
			printk("%s : val = %d plug in USB\n",__func__, val);
			//Add the code of USB Charge and Limit current settings
			//ret = ricoh61x_write(info->dev->parent, REGISET2_REG, usb_current_val)
			break;
	case	3:// plug in USB/ADP
			printk("%s : val = %d plug in ADP USB\n",__func__, val);
			break;
	default:
			printk("%s : val = %d unknown\n",__func__, val);
			break;
	}
#endif


	giRICOH619_DCIN = ricoh619_charger_detect();
	if(9!=gptHWCFG->m_val.bCustomer) {
		if(giRICOH619_DCIN) {
			led_red(1);
		}
		else {
			led_red(0);
		}
	}

	_config_ricoh619_charger_params(info->dev,giRICOH619_DCIN);
#if 0
	if(mxc_misc_report_usb) {
		mxc_misc_report_usb(giRICOH619_DCIN?1:0);
	}
#endif
	{
		extern void usb_plug_handler(void *dummy);
		usb_plug_handler (NULL);
	}

//	mutex_unlock(&info->lock);
//	printk(KERN_INFO "PMU:%s Out\n", __func__);
}

#ifdef ENABLE_LOW_BATTERY_DETECTION
static void low_battery_irq_work(struct work_struct *work)
{
	struct ricoh61x_battery_info *info = container_of(work,
		 struct ricoh61x_battery_info, low_battery_work.work);

	int ret = 0;

	printk(KERN_INFO "PMU:%s In\n", __func__);
	
	critical_low_flag = 1;
	power_supply_changed(info->battery);
	info->suspend_state = false;
	printk(KERN_INFO "PMU:%s Set ciritical_low_flag = 1 **********\n", __func__);
	
#if 0
	/* Enable VSYS threshold Low interrupt */
	ricoh61x_write(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x10);
	if (ret < 0)
		dev_err(info->dev,
			 "%s(): Error in enable adc mask INT %d\n",
			 __func__, ret);
#endif
}
#endif

#ifdef ENABLE_BATTERY_TEMP_DETECTION
static void battery_temp_irq_work(struct work_struct *work)
{
	struct ricoh61x_battery_info *info = container_of(work,
		 struct ricoh61x_battery_info, battery_temp_work.work);

	int ret = 0;
	uint8_t val;
	int bat_temp_vol;

	printk(KERN_INFO "PMU:%s In\n", __func__);

	power_supply_changed(info->battery);

	ricoh61x_read(info->dev->parent, RICOH61x_ADC_VTHMDATAH, &val);
	bat_temp_vol = val*2500/255;
	printk(KERN_INFO "PMU:%s Battery temperature triggered %dmV (VTHMDATA 0x%02X)**********\n", __func__, bat_temp_vol, val);

	if (bat_temp_vol < BATTERY_TEMP_50_VOL) {
		printk (KERN_INFO "PMU:%s Battery temp > 50 degrees !! \n", __func__);
		/* Set VTHM threshold high voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_50_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THH, val);
		/* Enable VTHM threshold high interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC2, 0x20);
		gBatt_ADC_isr_temp = 505;
	}
	else if (bat_temp_vol < BATTERY_TEMP_48_VOL) {
		printk (KERN_INFO "PMU:%s 48 degrees < Battery temp < 50 degrees\n", __func__);
		/* Set VTHM threshold low voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_50_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THL, val);
		/* Enable VBAT threshold Low interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x20);
		/* Set VTHM threshold high voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_48_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THH, val);
		/* Enable VTHM threshold high interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC2, 0x20);
		gBatt_ADC_isr_temp = 455;

		ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
	}
	else if (bat_temp_vol < BATTERY_TEMP_40_VOL) {
		printk (KERN_INFO "PMU:%s 40 degrees < Battery temp < 48 degrees\n", __func__);
		/* Set VTHM threshold low voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_48_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THL, val);
		/* Enable VBAT threshold Low interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x20);
		/* Set VTHM threshold high voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_40_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THH, val);
		/* Enable VTHM threshold high interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC2, 0x20);
		gBatt_ADC_isr_temp = 405;

		ricoh61x_set_bits(info->dev->parent, CHGCTL1_REG, 0x03);
	}
	else if (bat_temp_vol > BATTERY_TEMP_0_VOL){
		printk (KERN_INFO "PMU:%s Battery temp < 0 degree !! \n", __func__);
		/* Set VTHM threshold low voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_0_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THL, val);
		/* Enable VBAT threshold Low interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x20);
		gBatt_ADC_isr_temp = -5;
	}
	else if (bat_temp_vol > BATTERY_TEMP_5_VOL){
		printk (KERN_INFO "PMU:%s 0 degrees < Battery temp < 5 degrees\n", __func__);
		/* Set VTHM threshold low voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_5_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THL, val);
		/* Enable VBAT threshold Low interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x20);
		/* Set VTHM threshold high voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_0_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THH, val);
		/* Enable VTHM threshold high interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC2, 0x20);
		gBatt_ADC_isr_temp = 45;

		ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
	}
	else if (bat_temp_vol > BATTERY_TEMP_8_VOL){
		printk (KERN_INFO "PMU:%s 5 degrees < Battery temp < 8 degrees\n", __func__);
		/* Set VTHM threshold low voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_8_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THL, val);
		/* Enable VBAT threshold Low interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x20);
		/* Set VTHM threshold high voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_5_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THH, val);
		/* Enable VTHM threshold high interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC2, 0x20);
		gBatt_ADC_isr_temp = 75;

		ricoh61x_set_bits(info->dev->parent, CHGCTL1_REG, 0x03);
	}
	else {
		printk (KERN_INFO "PMU:%s 8 degrees < Battery temp < 40 degrees\n", __func__);
		/* Set VTHM threshold low voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_40_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THL, val);
		/* Enable VBAT threshold Low interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x20);
		/* Set VTHM threshold high voltage value = (voltage(V)*255)/(2.5) */
		val = BATTERY_TEMP_8_VOL * 255 / 2500;
		ricoh61x_write(info->dev->parent, RICOH61x_ADC_VTHM_THH, val);
		/* Enable VTHM threshold high interrupt */
		ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC2, 0x20);
		if (bat_temp_vol > ((BATTERY_TEMP_8_VOL+BATTERY_TEMP_40_VOL)/2))
			gBatt_ADC_isr_temp = 85;
		else
			gBatt_ADC_isr_temp = 395;
	}
	gBatt_ADC_isr_tick = jiffies;
}
#endif

static irqreturn_t charger_in_isr(int irq, void *battery_info)
{
	struct ricoh61x_battery_info *info = battery_info;
//	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_stat1 |= 0x01;
	queue_work(info->workqueue, &info->irq_work);
	return IRQ_HANDLED;
}

static irqreturn_t charger_complete_isr(int irq, void *battery_info)
{
	struct ricoh61x_battery_info *info = battery_info;
//	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_stat1 |= 0x02;
	queue_work(info->workqueue, &info->irq_work);

	return IRQ_HANDLED;
}

static irqreturn_t charger_overtime_isr(int irq, void *battery_info)
{
	struct ricoh61x_battery_info *info = battery_info;
	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_stat1 |= 0x04;
	queue_work(info->workqueue, &info->irq_work);

	return IRQ_HANDLED;
}

static irqreturn_t charger_usb_isr(int irq, void *battery_info)
{
	struct ricoh61x_battery_info *info = battery_info;
	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_ctr |= 0x02;
	
	queue_work(info->workqueue, &info->irq_work);

	info->soca->dischg_state = 0;
	info->soca->chg_count = 0;
	if (RICOH61x_SOCA_UNSTABLE == info->soca->status
		|| RICOH61x_SOCA_FG_RESET == info->soca->status)
		info->soca->stable_count = 11;

	return IRQ_HANDLED;
}

static irqreturn_t charger_adp_isr(int irq, void *battery_info)
{
	struct ricoh61x_battery_info *info = battery_info;
	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_ctr |= 0x01;
	queue_work(info->workqueue, &info->irq_work);

	info->soca->dischg_state = 0;
	info->soca->chg_count = 0;
	if (RICOH61x_SOCA_UNSTABLE == info->soca->status
		|| RICOH61x_SOCA_FG_RESET == info->soca->status)
		info->soca->stable_count = 11;

	return IRQ_HANDLED;
}


#ifdef ENABLE_LOW_BATTERY_DETECTION
/*************************************************************/
/* for Detecting Low Battery                                 */
/*************************************************************/

static irqreturn_t adc_vsysl_isr(int irq, void *battery_info)
{

	struct ricoh61x_battery_info *info = battery_info;

	printk(KERN_INFO "PMU:%s\n", __func__);
	printk(KERN_INFO "PMU:%s Detect Low Battery Interrupt **********\n", __func__);
	// critical_low_flag = 1;
	queue_delayed_work(info->monitor_wqueue, &info->low_battery_work,
					LOW_BATTERY_DETECTION_TIME*HZ);

	return IRQ_HANDLED;
}
#endif

#ifdef ENABLE_BATTERY_TEMP_DETECTION
/*************************************************************/
/* for Detecting Battery Temperature                         */
/*************************************************************/

static irqreturn_t adc_vtherm_isr(int irq, void *battery_info)
{

	struct ricoh61x_battery_info *info = battery_info;

	printk(KERN_INFO "PMU:%s\n", __func__);
	printk(KERN_INFO "PMU:%s Detect Battery Temperature Interrupt **********\n", __func__);
	queue_delayed_work(info->monitor_wqueue, &info->battery_temp_work, 0);

	return IRQ_HANDLED;
}
#endif

/*
 * Get Charger Priority
 * - get higher-priority between VADP and VUSB
 * @ data: higher-priority is stored
 *         true : VUSB
 *         false: VADP
 */
static int get_charge_priority(struct ricoh61x_battery_info *info, bool *data)
{
	int ret = 0;
	uint8_t val = 0;

	ret = ricoh61x_read(info->dev->parent, CHGCTL1_REG, &val);
	val = val >> 7;
	*data = (bool)val;

	return ret;
}

/*
 * Set Charger Priority
 * - set higher-priority between VADP and VUSB
 * - data: higher-priority is stored
 *         true : VUSB
 *         false: VADP
 */
static int set_charge_priority(struct ricoh61x_battery_info *info, bool *data)
{
	int ret = 0;
	uint8_t val = 0x80;

	if (*data == 1)
		ret = ricoh61x_set_bits(info->dev->parent, CHGCTL1_REG, val);
	else
		ret = ricoh61x_clr_bits(info->dev->parent, CHGCTL1_REG, val);

	return ret;
}

#ifdef	ENABLE_FUEL_GAUGE_FUNCTION
static int get_check_fuel_gauge_reg(struct ricoh61x_battery_info *info,
					 int Reg_h, int Reg_l, int enable_bit)
{
	uint8_t get_data_h, get_data_l;
	int old_data, current_data;
	int i;
	int ret = 0;

	old_data = 0;

	for (i = 0; i < 5 ; i++) {
		ret = ricoh61x_read(info->dev->parent, Reg_h, &get_data_h);
		if (ret < 0) {
			dev_err(info->dev, "Error in reading the control register\n");
			return ret;
		}

		ret = ricoh61x_read(info->dev->parent, Reg_l, &get_data_l);
		if (ret < 0) {
			dev_err(info->dev, "Error in reading the control register\n");
			return ret;
		}

		current_data = ((get_data_h & 0xff) << 8) | (get_data_l & 0xff);
		current_data = (current_data & enable_bit);

		if (current_data == old_data)
			return current_data;
		else
			old_data = current_data;
	}

	return current_data;
}

static int calc_capacity(struct ricoh61x_battery_info *info)
{
	uint8_t capacity;
	long	capacity_l;
	int temp;
	int ret = 0;
	int nt;
	int temperature;
	int cc_cap = 0;
	long cc_cap_mas =0;
	int cc_delta;
	bool is_charging = true;

	if (info->soca->rsoc_ready_flag  != 0) {
		/* get remaining battery capacity from fuel gauge */
		ret = ricoh61x_read(info->dev->parent, SOC_REG, &capacity);
		if (ret < 0) {
			dev_err(info->dev, "Error in reading the control register\n");
			return ret;
		}
		capacity_l = (long)capacity;
	} else {
		ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 0);
		cc_delta = (is_charging == true) ? cc_cap : -cc_cap;
		capacity_l = (info->soca->init_pswr * 100 + cc_delta) / 100;
#ifdef	_RICOH619_DEBUG_
		SUSPEND_PRINT(KERN_DEBUG"PMU FG_RESET : %s : capacity %d init_pswr %d cc_delta %d\n",__func__,	capacity_l, info->soca->init_pswr, cc_delta);
#endif
	}

	temperature = get_battery_temp_2(info) / 10; /* unit 0.1 degree -> 1 degree */

	if (temperature >= 25) {
		nt = 0;
	} else if (temperature >= 5) {
		nt = (25 - temperature) * RICOH61x_TAH_SEL2 * 625 / 100;
	} else {
		nt = (625  + (5 - temperature) * RICOH61x_TAL_SEL2 * 625 / 100);
	}

	temp = capacity_l * 100 * 100 / (10000 - nt);

	temp = min(100, temp);
	temp = max(0, temp);
	
	return temp;		/* Unit is 1% */
}

static int calc_capacity_2(struct ricoh61x_battery_info *info)
{
	uint8_t val;
	long capacity;
	int re_cap, fa_cap;
	int temp;
	int ret = 0;
	int nt;
	int temperature;
	int cc_cap = 0;
	long cc_cap_mas =0;
	int cc_delta;
	bool is_charging = true;


	if (info->soca->rsoc_ready_flag  != 0) {
		re_cap = get_check_fuel_gauge_reg(info, RE_CAP_H_REG, RE_CAP_L_REG,
							0x7fff);
		fa_cap = get_check_fuel_gauge_reg(info, FA_CAP_H_REG, FA_CAP_L_REG,
							0x7fff);

		if (fa_cap != 0) {
			capacity = ((long)re_cap * 100 * 100 / fa_cap);
			capacity = (long)(min(10000, (int)capacity));
			capacity = (long)(max(0, (int)capacity));
		} else {
			ret = ricoh61x_read(info->dev->parent, SOC_REG, &val);
			if (ret < 0) {
				dev_err(info->dev, "Error in reading the control register\n");
				return ret;
			}
			capacity = (long)val * 100;
		}
	} else {
		ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas, &is_charging, 0);
		cc_delta = (is_charging == true) ? cc_cap : -cc_cap;
		capacity = info->soca->init_pswr * 100 + cc_delta;
#ifdef	_RICOH619_DEBUG_
		SUSPEND_PRINT(KERN_DEBUG"PMU FG_RESET : %s : capacity %d init_pswr %d cc_delta %d\n",__func__,	(int)capacity, info->soca->init_pswr, cc_delta);
#endif
	}

	temperature = get_battery_temp_2(info) / 10; /* unit 0.1 degree -> 1 degree */

	if (temperature >= 25) {
		nt = 0;
	} else if (temperature >= 5) {
		nt = (25 - temperature) * RICOH61x_TAH_SEL2 * 625 / 100;
	} else {
		nt = (625  + (5 - temperature) * RICOH61x_TAL_SEL2 * 625 / 100);
	}

	temp = (int)(capacity * 100 * 100 / (10000 - nt));

	temp = min(10000, temp);
	temp = max(0, temp);

	return temp;		/* Unit is 0.01% */
}

static int get_battery_temp(struct ricoh61x_battery_info *info)
{
	int ret = 0;
	int sign_bit;

	ret = get_check_fuel_gauge_reg(info, TEMP_1_REG, TEMP_2_REG, 0x0fff);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	/* bit3 of 0xED(TEMP_1) is sign_bit */
	sign_bit = ((ret & 0x0800) >> 11);

	ret = (ret & 0x07ff);

	if (sign_bit == 0)	/* positive value part */
		/* conversion unit */
		/* 1 unit is 0.0625 degree and retun unit
		 * should be 0.1 degree,
		 */
		ret = ret * 625  / 1000;
	else {	/*negative value part */
		ret = (~ret + 1) & 0x7ff;
		ret = -1 * ret * 625 / 1000;
	}

	return ret;
}

static int get_battery_temp_2(struct ricoh61x_battery_info *info)
{
	uint8_t reg_buff[2];
	long temp, temp_off, temp_gain;
	bool temp_sign, temp_off_sign, temp_gain_sign;
	int Vsns = 0;
	int Iout = 0;
	int Vthm, Rthm;
	int reg_val = 0;
	int new_temp;
	long R_ln1, R_ln2;
	int ret = 0;

	/* Calculate TEMP */
	ret = get_check_fuel_gauge_reg(info, TEMP_1_REG, TEMP_2_REG, 0x0fff);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the fuel gauge register\n");
		goto out;
	}

	reg_val = ret;
	temp_sign = (reg_val & 0x0800) >> 11;
	reg_val = (reg_val & 0x07ff);

	if (temp_sign == 0)	/* positive value part */
		/* the unit is 0.0001 degree */
		temp = (long)reg_val * 625;
	else {	/*negative value part */
		reg_val = (~reg_val + 1) & 0x7ff;
		temp = -1 * (long)reg_val * 625;
	}

	/* Calculate TEMP_OFF */
	ret = ricoh61x_bulk_reads_bank1(info->dev->parent,
					TEMP_OFF_H_REG, 2, reg_buff);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the fuel gauge register\n");
		goto out;
	}

	reg_val = reg_buff[0] << 8 | reg_buff[1];
	temp_off_sign = (reg_val & 0x0800) >> 11;
	reg_val = (reg_val & 0x07ff);

	if (temp_off_sign == 0)	/* positive value part */
		/* the unit is 0.0001 degree */
		temp_off = (long)reg_val * 625;
	else {	/*negative value part */
		reg_val = (~reg_val + 1) & 0x7ff;
		temp_off = -1 * (long)reg_val * 625;
	}

	/* Calculate TEMP_GAIN */
	ret = ricoh61x_bulk_reads_bank1(info->dev->parent,
					TEMP_GAIN_H_REG, 2, reg_buff);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the fuel gauge register\n");
		goto out;
	}

	reg_val = reg_buff[0] << 8 | reg_buff[1];
	temp_gain_sign = (reg_val & 0x0800) >> 11;
	reg_val = (reg_val & 0x07ff);

	if (temp_gain_sign == 0)	/* positive value part */
		/* 1 unit is 0.000488281. the result is 0.01 */
		temp_gain = (long)reg_val * 488281 / 100000;
	else {	/*negative value part */
		reg_val = (~reg_val + 1) & 0x7ff;
		temp_gain = -1 * (long)reg_val * 488281 / 100000;
	}

	/* Calculate VTHM */
	if (0 != temp_gain)
		Vthm = (int)((temp - temp_off) / 4095 * 2500 / temp_gain);
	else {
#ifdef	_RICOH619_DEBUG_
		printk(KERN_DEBUG"PMU %s Skip to compensate temperature\n", __func__);
#endif
		goto out;
	}

	ret = measure_Ibatt_FG(info, &Iout);
	Vsns = Iout * 2 / 100;

	if (temp < -120000) {
		/* Low Temperature */
		if (0 != (2500 - Vthm)) {
			Rthm = 10 * 10 * (Vthm - Vsns) / (2500 - Vthm);
		} else {
#ifdef	_RICOH619_DEBUG_
			printk(KERN_DEBUG"PMU %s Skip to compensate temperature\n", __func__);
#endif
			goto out;
		}

		R_ln1 = Rthm / 10;
		R_ln2 =  (R_ln1 * R_ln1 * R_ln1 * R_ln1 * R_ln1 / 100000
			- R_ln1 * R_ln1 * R_ln1 * R_ln1 * 2 / 100
			+ R_ln1 * R_ln1 * R_ln1 * 11
			- R_ln1 * R_ln1 * 2980
			+ R_ln1 * 449800
			- 784000) / 10000;

		/* the unit of new_temp is 0.1 degree */
		new_temp = (int)((100 * 1000 * B_VALUE / (R_ln2 + B_VALUE * 100 * 1000 / 29815) - 27315) / 10);
#ifdef	_RICOH619_DEBUG_
		printk(KERN_DEBUG"PMU %s low temperature %d\n", __func__, new_temp/10);  
#endif
	} else if (temp > 520000) {
		/* High Temperature */
		if (0 != (2500 - Vthm)) {
			Rthm = 100 * 10 * (Vthm - Vsns) / (2500 - Vthm);
		} else {
#ifdef	_RICOH619_DEBUG_
			printk(KERN_DEBUG"PMU %s Skip to compensate temperature\n", __func__);
#endif
			goto out;
		}
#ifdef	_RICOH619_DEBUG_
		printk(KERN_DEBUG"PMU %s [Rthm] Rthm %d[ohm]\n", __func__, Rthm);
#endif

		R_ln1 = Rthm / 10;
		R_ln2 =  (R_ln1 * R_ln1 * R_ln1 * R_ln1 * R_ln1 / 100000 * 15652 / 100
			- R_ln1 * R_ln1 * R_ln1 * R_ln1 / 1000 * 23103 / 100
			+ R_ln1 * R_ln1 * R_ln1 * 1298 / 100
			- R_ln1 * R_ln1 * 35089 / 100
			+ R_ln1 * 50334 / 10
			- 48569) / 100;
		/* the unit of new_temp is 0.1 degree */
		new_temp = (int)((100 * 100 * B_VALUE / (R_ln2 + B_VALUE * 100 * 100 / 29815) - 27315) / 10);
#ifdef	_RICOH619_DEBUG_
		printk(KERN_DEBUG"PMU %s high temperature %d\n", __func__, new_temp/10);  
#endif
	} else {
		/* the unit of new_temp is 0.1 degree */
		new_temp = temp / 1000;
	}

	return new_temp;

out:
	new_temp = get_battery_temp(info);
	return new_temp;
}

static int get_time_to_empty(struct ricoh61x_battery_info *info)
{
	int ret = 0;

	ret = get_check_fuel_gauge_reg(info, TT_EMPTY_H_REG, TT_EMPTY_L_REG,
								0xffff);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	/* conversion unit */
	/* 1unit is 1miniute and return nnit should be 1 second */
	ret = ret * 60;

	return ret;
}

static int get_time_to_full(struct ricoh61x_battery_info *info)
{
	int ret = 0;

	ret = get_check_fuel_gauge_reg(info, TT_FULL_H_REG, TT_FULL_L_REG,
								0xffff);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	ret = ret * 60;

	return  ret;
}

/* battery voltage is get from Fuel gauge */
static int measure_vbatt_FG(struct ricoh61x_battery_info *info, int *data)
{
	int ret = 0;

	if(info->soca->ready_fg == 1) {
		ret = get_check_fuel_gauge_reg(info, VOLTAGE_1_REG, VOLTAGE_2_REG,
									0x0fff);
		if (ret < 0) {
			dev_err(info->dev, "Error in reading the fuel gauge control register\n");
			return ret;
		}

		*data = ret;
		/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
		*data = *data * 50000 / 4095;
		/* return unit should be 1uV */
		*data = *data * 100;
		info->soca->Vbat_old = *data;
	} else {
		*data = info->soca->Vbat_old;
	}

	return ret;
}

static int measure_Ibatt_FG(struct ricoh61x_battery_info *info, int *data)
{
	int ret = 0;

	ret =  get_check_fuel_gauge_reg(info, CC_AVERAGE1_REG,
						 CC_AVERAGE0_REG, 0x3fff);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	*data = (ret > 0x1fff) ? (ret - 0x4000) : ret;
	return ret;
}

/**
* index : index No.(2 -> 20%)
* table_num : ocv table selection
*	0 : Original Table(defined by header file)
*	1 : Using Table
*/
static int get_OCV_init_Data(struct ricoh61x_battery_info *info, int index, int table_num)
{
	int ret = 0;
	if (table_num == USING) {
	ret = (battery_init_para[info->num][index*2]<<8) | (battery_init_para[info->num][index*2+1]);
	} else if (table_num == ORIGINAL) {
		ret = (info->soca->battery_init_para_original[index*2]<<8)
				| (info->soca->battery_init_para_original[index*2+1]);
	}
	return ret;
}

/**
* index : index No.(2 -> 20%)
* table_num : ocv table selection
*	0 : Original Table
*	1 : Using Table
*/
static int get_OCV_voltage(struct ricoh61x_battery_info *info, int index, int table_num)
{
	int ret = 0;
	ret =  get_OCV_init_Data(info, index, table_num);
	/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
	ret = ret * 50000 / 4095;
	/* return unit should be 1uV */
	ret = ret * 100;
	return ret;
}

static int measure_vbatt_ADC(struct ricoh61x_battery_info *info, int *data)
{
	uint8_t data_l = 0, data_h = 0;
	int ret;

	ret = ricoh61x_read(info->dev->parent, VBATDATAH_REG, &data_h);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		goto err;
	}

	ret = ricoh61x_read(info->dev->parent, VBATDATAL_REG, &data_l);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		goto err;
	}

	*data = ((data_h & 0xff) << 4) | (data_l & 0x0f);
	/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
	*data = *data * 5000 / 4095;
	/* return unit should be 1uV */
	*data = *data * 1000;

	printk("%s():%d uV\n",__FUNCTION__,*data);

	return 0;

err:
	return -1;
}
#else //][!ENABLE_FUEL_GAUGE_FUNCTION

/* battery voltage is get from ADC */
static int measure_vbatt_ADC(struct ricoh61x_battery_info *info, int *data)
{
	int	i;
	uint8_t data_l = 0, data_h = 0;
	int ret;

	/* ADC interrupt enable */
	ret = ricoh61x_set_bits(info->dev->parent, INTEN_REG, 0x08);
	if (ret < 0) {
		dev_err(info->dev, "Error in setting the control register bit\n");
		goto err;
	}

	/* enable interrupt request of single mode */
	ret = ricoh61x_set_bits(info->dev->parent, EN_ADCIR3_REG, 0x01);
	if (ret < 0) {
		dev_err(info->dev, "Error in setting the control register bit\n");
		goto err;
	}

	/* single request */
	ret = ricoh61x_write(info->dev->parent, ADCCNT3_REG, 0x10);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
		goto err;
	}

	for (i = 0; i < 5; i++) {
		usleep(1000);
		dev_info(info->dev, "ADC conversion times: %d\n", i);
		/* read completed flag of ADC */
		ret = ricoh61x_read(info->dev->parent, EN_ADCIR3_REG, &data_h);
		if (ret < 0) {
			dev_err(info->dev, "Error in reading the control register\n");
			goto err;
		}

		if (data_h & 0x01)
			goto	done;
	}

	dev_err(info->dev, "ADC conversion too long!\n");
	goto err;

done:
	ret = ricoh61x_read(info->dev->parent, VBATDATAH_REG, &data_h);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		goto err;
	}

	ret = ricoh61x_read(info->dev->parent, VBATDATAL_REG, &data_l);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		goto err;
	}

	*data = ((data_h & 0xff) << 4) | (data_l & 0x0f);
	/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
	*data = *data * 5000 / 4095;
	/* return unit should be 1uV */
	*data = *data * 1000;

	return 0;

err:
	return -1;
}
#endif //]ENABLE_FUEL_GAUGE_FUNCTION

static int measure_vsys_ADC(struct ricoh61x_battery_info *info, int *data)
{
	uint8_t data_l = 0, data_h = 0;
	int ret;

	ret = ricoh61x_read(info->dev->parent, VSYSDATAH_REG, &data_h);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
	}

	ret = ricoh61x_read(info->dev->parent, VSYSDATAL_REG, &data_l);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
	}

	*data = ((data_h & 0xff) << 4) | (data_l & 0x0f);
	*data = *data * 1000 * 3 * 5 / 2 / 4095;
	/* return unit should be 1uV */
	*data = *data * 1000;

	return 0;
}

static void ricoh61x_external_power_changed(struct power_supply *psy)
{
	struct ricoh61x_battery_info *info;

	info = container_of(psy, struct ricoh61x_battery_info, battery);
	queue_delayed_work(info->monitor_wqueue,
			   &info->changed_work, HZ / 2);
	return;
}

static int gRicoh619_cur_voltage;
static struct platform_device *gBattery_dev;
int ricoh619_battery_2_msp430_adc(void)
{
	int result;
#if 0
	// report battery status by voltage
	int i, battValue;
	const unsigned short battGasgauge[] = {
	//	3.0V, 3.1V, 3.2V, 3.3V, 3.4V, 3.5V, 3.6V, 3.7V, 3.8V, 3.9V, 4.0V, 4.1V, 4.2V,
		 767,  791,  812,  833,  852,  877,  903,  928,  950,  979,  993, 1019, 1023,
	};

	if (critical_low_flag) return 0x8000;
	if ((!gRicoh619_cur_voltage) || (3000 > gRicoh619_cur_voltage) || (4200 < gRicoh619_cur_voltage))
		return 1023;
		
	i = (gRicoh619_cur_voltage - 3000)/100;
	if (gRicoh619_cur_voltage % 100) {
		result =  (gRicoh619_cur_voltage % 100)/ (100 / (battGasgauge[i+1]-battGasgauge[i]));
		result += battGasgauge[i];
	}
	else
		result = battGasgauge[i];
#else
	// report battery status by percentage
	struct ricoh61x_battery_info *info = platform_get_drvdata(gBattery_dev);
	int percentage = (info->soca->displayed_soc+50)/100;
	if (0 >= percentage)
		return 0x8000;
	result = 885 + (1023-885)*percentage/100;
#endif
	return result;
}

static ssize_t show_power_on_reason(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ricoh61x_battery_info *info = platform_get_drvdata(gBattery_dev);
	uint8_t val;
	ricoh61x_read(info->dev->parent, 0x09, &val);	// read power on history register.
	switch (val) {
	case 1:
		return sprintf(buf, "POWER KEY");
	case 2:
		return sprintf(buf, "REBOOT");
	case 4:
		return sprintf(buf, "USB");
	case 8:
		return sprintf(buf, "EXTIN");
	case 0x10:
		return sprintf(buf, "RTC");
	default:
		return sprintf(buf, "UNDEFINED %X",val);
	}
}

static DEVICE_ATTR(power_on_reason, 0444, show_power_on_reason, NULL);
static int add_power_on_reason_sysfs(struct device *dev) {
	int err;
	err = device_create_file(dev, &dev_attr_power_on_reason);
	if (err< 0)
		printk("**FAILED adding power_on_reason sysfs\r\n");
	return err;
}


static ssize_t show_eval_curr_vals(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ricoh61x_battery_info *info = dev_get_drvdata(dev);
	_ricoh_suspend_state_sync(info);
	return sprintf(buf,"fl_current=%d\nsus_current=%d\nhiber_current=%d\n",
			fl_current,sus_current,hiber_current);
}
static DEVICE_ATTR(eval_curr_vals, 0444, show_eval_curr_vals, NULL);
static int add_eval_curr_vals_sysfs(struct device *dev) {
	int err;
	err = device_create_file(dev, &dev_attr_eval_curr_vals);
	if (err< 0)
		printk("**FAILED adding eval_curr_vals sysfs\r\n");
	return err;
}

static ssize_t show_batt_params(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ricoh61x_battery_info *info = platform_get_drvdata(gBattery_dev);
	int len = 0;
	int tot_len = 0;
	int i;

	len = sprintf(buf+tot_len,"battery_type=%d\n",(int)gptHWCFG->m_val.bBattery);tot_len += len;
	len = sprintf(buf+tot_len,"0p_mV=%d\n",(int)info->fg_poff_vbat);tot_len += len;
	len = sprintf(buf+tot_len,"Critical_mV=%d\n",(int)info->alarm_vol_mv);tot_len += len;
	len = sprintf(buf+tot_len,"OCV_TabIdx=%d\n",(int)info->num);tot_len += len;

	for (i = 0; i <= 10; i = i+1) {
		len = sprintf(buf+tot_len,"%d0p_voltage=%duV\n",
				  i, get_OCV_voltage(info, i, USING)); tot_len += len;
	}

	return tot_len;
}
static DEVICE_ATTR(batt_params, 0444, show_batt_params, NULL);
static int add_batt_params_sysfs(struct device *dev) {
	int err;
	err = device_create_file(dev, &dev_attr_batt_params);
	if (err< 0)
		printk("**FAILED adding batt_params sysfs\r\n");
	return err;
}


static const char gszTestStates_prop_capacity_method[] = "capacity_method";
#define prop_capacity_method_voltage "voltage"
#define prop_capacity_method_default  "default"
static char *prop_capacity_methodA[] = {
prop_capacity_method_default,
prop_capacity_method_voltage,
};
static int prop_capacity_method_str2idx(const char *szMethod)
{
	int i;
	for(i=0;i<sizeof(prop_capacity_methodA)/sizeof(prop_capacity_methodA[0]);i++)
	{
		if(0==strcmp(prop_capacity_methodA[i],szMethod)) {
			return i;
		}
	}
	return -1;
}
static ssize_t store_test_states(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int i;
	char cCmdA[256],cValA[256];
	sscanf(buf,"%s : %s",cCmdA,cValA);
	printk("%s: cmd=%s,val=%s\n",__FUNCTION__,cCmdA,cValA);

	if(0==strcmp(cCmdA,gszTestStates_prop_capacity_method)) {
		i = prop_capacity_method_str2idx(cValA);
		if(i>=0) {
			giCapacityMethod = i;
		}
		else {
			printk("%s - \"%s\" not found\n",gszTestStates_prop_capacity_method,cValA);
		}
	}

	return strlen(buf);
}

static ssize_t show_test_states(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char *p=buf;
	int iLen;

	iLen = sprintf(p,"%s : %s\n",
			gszTestStates_prop_capacity_method,prop_capacity_methodA[giCapacityMethod]);
	p += iLen;
	// next prop . 

	return strlen(buf);
}

static DEVICE_ATTR(test_states, 0644, show_test_states,store_test_states);
static int add_test_states_sysfs(struct device *dev) {
	int err;
	err = device_create_file(dev, &dev_attr_test_states);
	if (err< 0)
		printk("**FAILED adding test_states sysfs\r\n");
	return err;
}


#if 0
extern int ntx_is_bat_critical (void);
#endif
static int ricoh61x_batt_get_prop(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct ricoh61x_battery_info *info = dev_get_drvdata(psy->dev.parent);
	int data = 0;
	int ret = 0;
	uint8_t status;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &status);
		if (ret < 0) {
			dev_err(info->dev, "Error in reading the control register\n");
			mutex_unlock(&info->lock);
			return ret;
		}
		if (!(0xC0 & status)) {
			val->intval = 0;
			break;
		}
#if 1
		if(CDP_CHARGER==giRICOH619_DCIN) {
			val->intval = 4;
		}
		else if(DCP_CHARGER==giRICOH619_DCIN) {
			val->intval = 3;
		}
		else if(SDP_PC_CHARGER==giRICOH619_DCIN) {
			val->intval = 2;
		}
		else {
			val->intval = 1;
		}
#else
		val->intval = (0x04 == (status & 0x1F))?1:3;

		ret = ricoh61x_read(info->dev->parent, 0xDA, &status);
		printk("type %X, status : 0x%02x \n", psy->type, status);
		if (((psy->desc->type == POWER_SUPPLY_TYPE_MAINS) && (0x20 != (status & 0x30))) ||
			((psy->desc->type == POWER_SUPPLY_TYPE_USB) && (0x0 != (status & 0x30))))
			val->intval = 0;
#endif
		break;
	/* this setting is same as battery driver of 584 */
	case POWER_SUPPLY_PROP_STATUS:
		ret = get_power_supply_Android_status(info);
		if (POWER_SUPPLY_STATUS_FULL == ret)
			if (49 == gptHWCFG->m_val.bPCB)  //E60QDX
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			val->intval = ret;
		info->status = ret;
		dev_dbg(info->dev, "Power Supply Status is %d\n",
							info->status);
		break;

	/* this setting is same as battery driver of 584 */
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->present;
		break;

	/* current voltage is got from fuel gauge */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* return real vbatt Voltage */
#ifdef	ENABLE_FUEL_GAUGE_FUNCTION
		if (info->soca->ready_fg)
			ret = measure_vbatt_FG(info, &data);
		else {
			//val->intval = -EINVAL;
			data = info->cur_voltage * 1000;
		}
#else
		ret = measure_vbatt_ADC(info, &data);
#endif
		val->intval = data;
		/* convert unit uV -> mV */
		info->cur_voltage = data / 1000;
		
		gRicoh619_cur_voltage = info->cur_voltage;
#ifdef	_RICOH619_DEBUG_
		dev_dbg(info->dev, "battery voltage is %d mV\n",
						info->cur_voltage);
#endif
		break;

#ifdef	ENABLE_FUEL_GAUGE_FUNCTION
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_check_fuel_gauge_reg(info, RE_CAP_H_REG, RE_CAP_L_REG,
							0x7fff);
		ret = 0;
		break;

	/* current battery capacity is get from fuel gauge */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (info->entry_factory_mode){
			val->intval = 100;
			info->capacity = 100;
		} else if (info->soca->displayed_soc <= 0) {
			val->intval = 0;
			info->capacity = 0;
		} else if (1==giCapacityMethod) {	
			int soc_voltage_percent;
			
			soc_voltage_percent = calc_soc_by_voltageMethod(info)/100;
			printk("report capacity by voltage method\n");
			val->intval = soc_voltage_percent;
			info->capacity = soc_voltage_percent;
		
		} else {
			val->intval = (info->soca->displayed_soc + 50)/100;
			info->capacity = (info->soca->displayed_soc + 50)/100;
			if (g_fake_soc >= 0)
				val->intval = g_fake_soc;
		}

		//printk("%s():critical_low_flag=%d,soca->displayed_soc=%d,%d%%\n",
		//	__FUNCTION__,critical_low_flag,info->soca->displayed_soc,info->capacity);

#if 0
		if (critical_low_flag || ntx_is_bat_critical()) 
#else
		if (critical_low_flag)
#endif 
		{
			uint8_t chg_sts = 0;
			ret = ricoh61x_read(info->dev->parent, CHGSTATE_REG, &chg_sts);
			if (ret < 0) {
				dev_err(info->dev, "Error in reading the status register\n");
				chg_sts = 0xC0;
			}
			if (chg_sts & 0xC0) {
				critical_low_flag = 0;
			} else {
				if (10 < info->capacity) {
					critical_low_flag = 0;	// clear battery critical flag.
					printk ("Battery criticl with high battery capcaty.\n");
					break;
				}
				val->intval = 0;
				info->capacity = 0;
			}
		}

#ifdef	_RICOH619_DEBUG_
		dev_dbg(info->dev, "battery capacity is %d%%\n",
							info->capacity);
#endif
		break;

	/* current temperature of battery */
	case POWER_SUPPLY_PROP_TEMP:
		if (BATTERY_ADC_UNUSED_TEMP != gBatt_ADC_isr_temp) {
			val->intval= gBatt_ADC_isr_temp;
			info->battery_temp = get_battery_temp_2(info)/10;
			if(time_after(jiffies, gBatt_ADC_isr_tick + 2*HZ))
				gBatt_ADC_isr_temp = BATTERY_ADC_UNUSED_TEMP;
			break;
		}
		if (info->soca->ready_fg) {
			if(49==gptHWCFG->m_val.bPCB) {
				// E60QDx
				int bat_temp_val;
				int bat_temp_vol;
				unsigned char adc_val;
				ricoh61x_read(info->dev->parent, RICOH61x_ADC_VTHMDATAH, &adc_val);
				bat_temp_val = adc_val << 4;
				ricoh61x_read(info->dev->parent, RICOH61x_ADC_VTHMDATAL, &adc_val);
				bat_temp_val |= (adc_val & 0x0F);

				bat_temp_vol = bat_temp_val*2500/4095;
	//			printk(KERN_INFO "PMU:%s Battery temperature %dmV (VTHMDATA 0x%02X)**********\n", __func__, bat_temp_vol);
				if (BATTERY_TEMP_0_VOL < bat_temp_vol)
					val->intval = -10;
				else if ((BATTERY_TEMP_5_VOL < bat_temp_vol))
					val->intval = 50-50*(bat_temp_vol-BATTERY_TEMP_5_VOL)/(BATTERY_TEMP_0_VOL-BATTERY_TEMP_5_VOL);
				else if ((BATTERY_TEMP_8_VOL < bat_temp_vol))
					val->intval = 80-30*(bat_temp_vol-BATTERY_TEMP_8_VOL)/(BATTERY_TEMP_5_VOL-BATTERY_TEMP_8_VOL);
				else if ((BATTERY_TEMP_40_VOL < bat_temp_vol))
					val->intval = 400-320*(bat_temp_vol-BATTERY_TEMP_40_VOL)/(BATTERY_TEMP_8_VOL-BATTERY_TEMP_40_VOL);
				else if ((BATTERY_TEMP_48_VOL < bat_temp_vol))
					val->intval = 480-80*(bat_temp_vol-BATTERY_TEMP_48_VOL)/(BATTERY_TEMP_40_VOL-BATTERY_TEMP_48_VOL);
				else if ((BATTERY_TEMP_50_VOL < bat_temp_vol))
					val->intval = 500-20*(bat_temp_vol-BATTERY_TEMP_50_VOL)/(BATTERY_TEMP_48_VOL-BATTERY_TEMP_50_VOL);
				else
					val->intval = 510;
			}
			else
				val->intval = get_battery_temp_2(info);

			info->battery_temp = val->intval/10;
#ifdef	_RICOH619_DEBUG_
			dev_dbg(info->dev,
					 "battery temperature is %d degree\n",
							 info->battery_temp);
#endif
			ret = 0;
		} else {
			val->intval = info->battery_temp * 10;
#ifdef	_RICOH619_DEBUG_
			dev_dbg(info->dev, "battery temperature is %d degree\n", info->battery_temp);
#endif
		}
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		if (info->soca->ready_fg) {
			ret = get_time_to_empty(info);
			val->intval = ret;
			info->time_to_empty = ret/60;
#ifdef	_RICOH619_DEBUG_
			dev_dbg(info->dev,
				 "time of empty battery is %d minutes\n",
							 info->time_to_empty);
#endif
		} else {
			//val->intval = -EINVAL;
			val->intval = info->time_to_empty * 60;
#ifdef	_RICOH619_DEBUG_
			dev_dbg(info->dev, "time of empty battery is %d minutes\n", info->time_to_empty); 
#endif
		}
		break;

	 case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		if (info->soca->ready_fg) {
			ret = get_time_to_full(info);
			val->intval = ret;
			info->time_to_full = ret/60;
#ifdef	_RICOH619_DEBUG_
			dev_dbg(info->dev,
				 "time of full battery is %d minutes\n",
							 info->time_to_full);
#endif
		} else {
			//val->intval = -EINVAL;
			val->intval = info->time_to_full * 60;
#ifdef	_RICOH619_DEBUG_
			dev_dbg(info->dev, "time of full battery is %d minutes\n", info->time_to_full);
#endif
		}
		break;
#endif
	 case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = info->fg_poff_vbat;
		ret = 0;
		break;

	default:
		mutex_unlock(&info->lock);
		return -ENODEV;
	}

	mutex_unlock(&info->lock);

	return ret;
}

static int ricoh61x_batt_set_prop(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct ricoh61x_battery_info *info = dev_get_drvdata(psy->dev.parent);
	int data = 0;
	int ret = -ENODEV;
	uint8_t status;

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		if (3400000 < val->intval && 3800000 > val->intval) {
			info->fg_poff_vbat = val->intval;
			info->alarm_vol_mv = val->intval-500000;
			val = (info->alarm_vol_mv) * 255 / 5000;
			ricoh61x_write(info->dev->parent, RICOH61x_ADC_VBAT_THL, val);
			ret = 0;
		}
		break;

	default:
		break;
	}
	mutex_unlock(&info->lock);
	return ret;
}

static int ricoh61x_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property ricoh61x_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,

#ifdef	ENABLE_FUEL_GAUGE_FUNCTION
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
#endif
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
};

static enum power_supply_property ricoh61x_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

struct power_supply_desc	power_ntx_desc = {
		.name = "mc13892_charger",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = ricoh61x_power_props,
		.num_properties = ARRAY_SIZE(ricoh61x_power_props),
		.get_property = ricoh61x_batt_get_prop,
};

struct power_supply_desc	powerac_desc = {
		.name = "acpwr",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ricoh61x_power_props,
		.num_properties = ARRAY_SIZE(ricoh61x_power_props),
		.get_property = ricoh61x_batt_get_prop,
};

struct power_supply_desc	powerusb_desc = {
		.name = "usbpwr",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = ricoh61x_power_props,
		.num_properties = ARRAY_SIZE(ricoh61x_power_props),
		.get_property = ricoh61x_batt_get_prop,
};

struct power_supply_desc powerbattery = {
		.name = "mc13892_bat",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = ricoh61x_batt_props,
		.num_properties = ARRAY_SIZE(ricoh61x_batt_props),
		.get_property = ricoh61x_batt_get_prop,
		.set_property = ricoh61x_batt_set_prop,
		.external_power_changed = ricoh61x_external_power_changed,
		.property_is_writeable	= ricoh61x_batt_prop_is_writeable,
};

#ifdef CONFIG_OF
static struct ricoh619_battery_platform_data *
ricoh619_battery_dt_init(struct platform_device *pdev)
{
	struct device_node *nproot = pdev->dev.parent->of_node;
	struct device_node *np;
	struct ricoh619_battery_platform_data *pdata;

	if (!nproot)
		return pdev->dev.platform_data;

	np = of_find_node_by_name(nproot, "battery");
	if (!np) {
		dev_err(&pdev->dev, "failed to find battery node\n");
		return NULL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct ricoh619_battery_platform_data),
			GFP_KERNEL);

	pdata->type[0].fg_hiber_curr=-1;
	pdata->type[0].fg_sus_curr=-1;
	pdata->alarm_vol_mv=-1;
	pdata->type[0].fg_poff_vbat=-1;

	of_property_read_u32(np, "ricoh,monitor-time", &pdata->monitor_time);
	of_property_read_u32(np, "ricoh,alarm-vol-mv", &pdata->alarm_vol_mv);

	/* check rage of b,.attery type */
	type_n = Battery_Type();
	printk("%s type_n=%d\n", __func__, type_n);

	switch (type_n) {
	case (0):
		of_property_read_u32(np, "ricoh,ch-vfchg", &pdata->type[0].ch_vfchg);
		of_property_read_u32(np, "ricoh,ch-vrchg", &pdata->type[0].ch_vrchg);
		of_property_read_u32(np, "ricoh,ch-vbatovset", &pdata->type[0].ch_vbatovset);
		of_property_read_u32(np, "ricoh,ch-ichg", &pdata->type[0].ch_ichg);
		of_property_read_u32(np, "ricoh,ch-ilim-adp", &pdata->type[0].ch_ilim_adp);
		of_property_read_u32(np, "ricoh,ch-ilim-usb", &pdata->type[0].ch_ilim_usb);
		of_property_read_u32(np, "ricoh,ch-icchg", &pdata->type[0].ch_icchg);
		of_property_read_u32(np, "ricoh,fg-target-vsys", &pdata->type[0].fg_target_vsys);
		of_property_read_u32(np, "ricoh,fg-target-ibat", &pdata->type[0].fg_target_ibat);
		of_property_read_u32(np, "ricoh,fg-poff-vbat", &pdata->type[0].fg_poff_vbat);
		of_property_read_u32(np, "ricoh,fg-hiber-curr", &pdata->type[0].fg_hiber_curr);
		of_property_read_u32(np, "ricoh,fg-sus-curr", &pdata->type[0].fg_sus_curr);
		of_property_read_u32(np, "ricoh,jt-en", &pdata->type[0].jt_en);
		of_property_read_u32(np, "ricoh,jt-hw-sw", &pdata->type[0].jt_hw_sw);
		of_property_read_u32(np, "ricoh,jt-temp-h", &pdata->type[0].jt_temp_h);
		of_property_read_u32(np, "ricoh,jt-temp-l", &pdata->type[0].jt_temp_l);
		of_property_read_u32(np, "ricoh,jt-vfchg-h", &pdata->type[0].jt_vfchg_h);
		of_property_read_u32(np, "ricoh,jt-vfchg-l", &pdata->type[0].jt_vfchg_l);
		of_property_read_u32(np, "ricoh,jt-ichg-h", &pdata->type[0].jt_ichg_h);
		of_property_read_u32(np, "ricoh,jt-ichg-l", &pdata->type[0].jt_ichg_l);
		break;
#if 0
	case (1):
		of_property_read_u32(np, "ricoh,ch-vfchg-1", &pdata->type[1].ch_vfchg);
		of_property_read_u32(np, "ricoh,ch-vrchg-1", &pdata->type[1].ch_vrchg);
		of_property_read_u32(np, "ricoh,ch-vbatovset-1", &pdata->type[1].ch_vbatovset);
		of_property_read_u32(np, "ricoh,ch-ichg-1", &pdata->type[1].ch_ichg);
		of_property_read_u32(np, "ricoh,ch-ilim-adp-1", &pdata->type[1].ch_ilim_adp);
		of_property_read_u32(np, "ricoh,ch-ilim-usb-1", &pdata->type[1].ch_ilim_usb);
		of_property_read_u32(np, "ricoh,ch-icchg-1", &pdata->type[1].ch_icchg);
		of_property_read_u32(np, "ricoh,fg-target-vsys-1", &pdata->type[1].fg_target_vsys);
		of_property_read_u32(np, "ricoh,fg-target-ibat-1", &pdata->type[1].fg_target_ibat);
		of_property_read_u32(np, "ricoh,fg-poff-vbat-1", &pdata->type[1].fg_poff_vbat);
		of_property_read_u32(np, "ricoh,jt-en-1", &pdata->type[1].jt_en);
		of_property_read_u32(np, "ricoh,jt-hw-sw-1", &pdata->type[1].jt_hw_sw);
		of_property_read_u32(np, "ricoh,jt-temp-h-1", &pdata->type[1].jt_temp_h);
		of_property_read_u32(np, "ricoh,jt-temp-l-1", &pdata->type[1].jt_temp_l);
		of_property_read_u32(np, "ricoh,jt-vfchg-h-1", &pdata->type[1].jt_vfchg_h);
		of_property_read_u32(np, "ricoh,jt-vfchg-l-1", &pdata->type[1].jt_vfchg_l);
		of_property_read_u32(np, "ricoh,jt-ichg-h-1", &pdata->type[1].jt_ichg_h);
		of_property_read_u32(np, "ricoh,jt-ichg-l-1", &pdata->type[1].jt_ichg_l);
		break;
#endif
	default:
		of_node_put(np);
		return 0;
	}

	of_node_put(np);


	if(-1==pdata->alarm_vol_mv) {
		if(10==gptHWCFG->m_val.bBattery) {
			// 1200mA battery .
			pdata->alarm_vol_mv = 3550;		// set battery critical to 3.55V
		}
		else {
			pdata->alarm_vol_mv = 3400;		// set battery critical to 3.55V
		}
	}

	if(-1==pdata->type[0].fg_poff_vbat) {
		if(10==gptHWCFG->m_val.bBattery) {
			// 1200mA battery .
			pdata->type[0].fg_poff_vbat = 3600;		// set battery 0% to 3.6V
		}
		else 
		{
			pdata->type[0].fg_poff_vbat = 3520;		// set battery 0% to 3.6V
		}
	}

	dev_info(&pdev->dev, "alarm_vol_mv=%d\n",pdata->alarm_vol_mv);
	dev_info(&pdev->dev, "fg_poff_vbat=%d\n",pdata->type[0].fg_poff_vbat);

	return pdata;
}
#else
static struct ricoh619_battery_platform_data *
ricoh619_battery_dt_init(struct platform_device *pdev)
{
	return pdev->dev.platform_data;
}
#endif

static int ricoh61x_battery_probe(struct platform_device *pdev)
{
	struct ricoh61x_battery_info *info;
	struct ricoh619_battery_platform_data *pdata;
	struct ricoh61x *ricoh61x = dev_get_drvdata(pdev->dev.parent);
	int type_n;
	int ret, temp;
	struct power_supply_config psy_cfg = {};

	printk(KERN_EMERG "PMU: %s : version is %s\n", __func__,RICOH61x_BATTERY_VERSION);

	pdata = ricoh619_battery_dt_init(pdev);
	if (!pdata) {
		dev_err(&pdev->dev, "platform data isn't assigned to "
			"power supply\n");
		return -EINVAL;
	}



	info = kzalloc(sizeof(struct ricoh61x_battery_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->soca = kzalloc(sizeof(struct ricoh61x_soca_info), GFP_KERNEL);
		if (!info->soca)
			return -ENOMEM;

	info->dev = &pdev->dev;
	info->status = POWER_SUPPLY_STATUS_CHARGING;
	info->monitor_time = pdata->monitor_time * HZ;
	info->alarm_vol_mv = pdata->alarm_vol_mv;
	info->present = 1;

	/* check rage of b,.attery type */
	type_n = Battery_Type();
	temp = sizeof(pdata->type)/(sizeof(struct ricoh619_battery_type_data));
	if(type_n  >= temp)
	{
		printk("%s : Battery type num is out of range\n", __func__);
		type_n = 0;
	}
	printk("%s type_n=%d,temp is %d\n", __func__, type_n,temp);

	/* set ibat_table[] for charge 85%-100%*/
	switch(gptHWCFG->m_val.bPCB) {
		default:
			info->iEnableTempDetect = 0;
			break;
	}
	printk("%s() TempDetect=%d\n", __func__, info->iEnableTempDetect);

	/* check rage of battery num */
	info->num = Battery_Table();
	temp = sizeof(battery_init_para)/(sizeof(uint8_t)*32);
	if(info->num >= (sizeof(battery_init_para)/(sizeof(uint8_t)*32)))
	{
		printk("%s : Battery num is out of range\n", __func__);
		info->num = 0;
	}
	printk("%s info->num=%d,temp is %d\n", __func__, info->num,temp);

	/* these valuse are set in platform */
	info->ch_vfchg = pdata->type[type_n].ch_vfchg;
	info->ch_vrchg = pdata->type[type_n].ch_vrchg;
	info->ch_vbatovset = pdata->type[type_n].ch_vbatovset;
	info->ch_ichg = pdata->type[type_n].ch_ichg;
	info->ch_ilim_adp = pdata->type[type_n].ch_ilim_adp;
	info->ch_ilim_usb = pdata->type[type_n].ch_ilim_usb;
	info->ch_icchg = pdata->type[type_n].ch_icchg;
	info->fg_target_vsys = pdata->type[type_n].fg_target_vsys;
	info->fg_target_ibat = pdata->type[type_n].fg_target_ibat;
	info->fg_poff_vbat = pdata->type[type_n].fg_poff_vbat;
	info->fg_sus_curr = pdata->type[type_n].fg_sus_curr;
	info->fg_hiber_curr = pdata->type[type_n].fg_hiber_curr;
	info->jt_en = pdata->type[type_n].jt_en;
	info->jt_hw_sw = pdata->type[type_n].jt_hw_sw;
	info->jt_temp_h = pdata->type[type_n].jt_temp_h;
	info->jt_temp_l = pdata->type[type_n].jt_temp_l;
	info->jt_vfchg_h = pdata->type[type_n].jt_vfchg_h;
	info->jt_vfchg_l = pdata->type[type_n].jt_vfchg_l;
	info->jt_ichg_h = pdata->type[type_n].jt_ichg_h;
	info->jt_ichg_l = pdata->type[type_n].jt_ichg_l;

	/*
	printk("%s setting value\n", __func__);
	printk("%s info->ch_vfchg = 0x%x\n", __func__, info->ch_vfchg);
	printk("%s info->ch_vrchg = 0x%x\n", __func__, info->ch_vrchg);
	printk("%s info->ch_vbatovset =0x%x\n", __func__, info->ch_vbatovset);
	printk("%s info->ch_ichg = 0x%x\n", __func__, info->ch_ichg);
	printk("%s info->ch_ilim_adp =0x%x \n", __func__, info->ch_ilim_adp);
	printk("%s info->ch_ilim_usb = 0x%x\n", __func__, info->ch_ilim_usb);
	printk("%s info->ch_icchg = 0x%x\n", __func__, info->ch_icchg);
	printk("%s info->fg_target_vsys = 0x%x\n", __func__, info->fg_target_vsys);
	printk("%s info->fg_target_ibat = 0x%x\n", __func__, info->fg_target_ibat);
	printk("%s info->jt_en = 0x%x\n", __func__, info->jt_en);
	printk("%s info->jt_hw_sw = 0x%x\n", __func__, info->jt_hw_sw);
	printk("%s info->jt_temp_h = 0x%x\n", __func__, info->jt_temp_h);
	printk("%s info->jt_temp_l = 0x%x\n", __func__, info->jt_temp_l);
	printk("%s info->jt_vfchg_h = 0x%x\n", __func__, info->jt_vfchg_h);
	printk("%s info->jt_vfchg_l = 0x%x\n", __func__, info->jt_vfchg_l);
	printk("%s info->jt_ichg_h = 0x%x\n", __func__, info->jt_ichg_h);
	printk("%s info->jt_ichg_l = 0x%x\n", __func__, info->jt_ichg_l);	
	*/

	info->adc_vdd_mv = ADC_VDD_MV;		/* 2800; */
	info->min_voltage = MIN_VOLTAGE;	/* 3100; */
	info->max_voltage = MAX_VOLTAGE;	/* 4200; */
	info->delay = 500;
	info->entry_factory_mode = false;
	info->suspend_state = false;
	info->stop_disp = false;

	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);

	gBattery_dev = pdev;

	/* Disable Charger/ADC interrupt */
	ret = ricoh61x_clr_bits(info->dev->parent, RICOH61x_INTC_INTEN,
							 CHG_INT | ADC_INT);
	if (ret)
		goto out;

	ret = ricoh61x_init_battery(info);
	if (ret)
		goto out;

#ifdef ENABLE_FACTORY_MODE
	info->factory_mode_wqueue
		= create_singlethread_workqueue("ricoh61x_factory_mode");
	INIT_DEFERRABLE_WORK(&info->factory_mode_work,
					 check_charging_state_work);

	ret = ricoh61x_factory_mode(info);
	if (ret)
		goto out;

#endif

	info->battery = power_supply_register(&pdev->dev, &powerbattery, &psy_cfg);

	if (info->battery) {
		info->battery->dev.parent = &pdev->dev;
	}

	psy_cfg.drv_data = info;
	powerac = power_supply_register(&pdev->dev, &powerac_desc, &psy_cfg);
	powerusb = power_supply_register(&pdev->dev, &powerusb_desc, &psy_cfg);
 	powerntx = power_supply_register(&pdev->dev, &power_ntx_desc, &psy_cfg);

	info->monitor_wqueue
		= create_singlethread_workqueue("ricoh61x_battery_monitor");
	info->workqueue = create_singlethread_workqueue("r5t61x_charger_in");
	INIT_WORK(&info->irq_work, charger_irq_work);
	INIT_DEFERRABLE_WORK(&info->monitor_work,
					 ricoh61x_battery_work);
	INIT_DEFERRABLE_WORK(&info->displayed_work,
					 ricoh61x_displayed_work);
	INIT_DEFERRABLE_WORK(&info->charge_stable_work,
					 ricoh61x_stable_charge_countdown_work);
	INIT_DEFERRABLE_WORK(&info->charge_monitor_work,
					 ricoh61x_charge_monitor_work);
	INIT_DEFERRABLE_WORK(&info->get_charge_work,
					 ricoh61x_get_charge_work);
	INIT_DEFERRABLE_WORK(&info->jeita_work, ricoh61x_jeita_work);
	INIT_DELAYED_WORK(&info->changed_work, ricoh61x_changed_work);

	/* Charger IRQ workqueue settings */
	ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_FONCHGINT) ,
					NULL, charger_in_isr, IRQF_ONESHOT,
						"r5t61x_charger_in", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't get CHG_INT IRQ for chrager: %d\n",
									ret);
		goto out;
	}

	ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_FCHGCMPINT),
						NULL, charger_complete_isr,
					IRQF_ONESHOT, "r5t61x_charger_comp",
								info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't get CHG_COMP IRQ for chrager: %d\n",
									 ret);
		goto out;
	}

	ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_FRTIMOVINT),
					NULL, charger_overtime_isr, IRQF_ONESHOT,
						"r5t61x_charger_timov", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't get CHG_TIMOV IRQ for chrager: %d\n",
									 ret);
		goto out;
	}

	ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_FVUSBDETSINT),
					NULL, charger_usb_isr, IRQF_ONESHOT,
						"r5t61x_usb_det", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't get USB_DET IRQ for chrager: %d\n",
									 ret);
		goto out;
	}

	ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_FVADPDETSINT),
					NULL, charger_adp_isr, IRQF_ONESHOT,
						"r5t61x_adp_det", info);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Can't get ADP_DET IRQ for chrager: %d\n", ret);
		goto out;
	}


#ifdef ENABLE_LOW_BATTERY_DETECTION
//	ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_VSYSLIR),
//					NULL, adc_vsysl_isr, IRQF_ONESHOT,
//						"r5t61x_adc_vsysl", info);
	ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_VBATLIR),
					NULL, adc_vsysl_isr, IRQF_ONESHOT,
						"r5t61x_adc_vsysl", info);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Can't get ADC_VSYSL IRQ for chrager: %d\n", ret);
		goto out;
	}
	INIT_DEFERRABLE_WORK(&info->low_battery_work,
					 low_battery_irq_work);
#endif

#ifdef ENABLE_BATTERY_TEMP_DETECTION
	if(info->iEnableTempDetect) {

		ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_VTHMLIR),
					NULL, adc_vtherm_isr, IRQF_ONESHOT,
						"r5t61x_adc_vtherm", info);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Can't get ADC_VTHML IRQ for chrager: %d\n", ret);
			goto out;
		}

		ret = request_threaded_irq(irq_create_mapping(ricoh61x->irq_domain, RICOH61x_IRQ_VTHMHIR),
					NULL, adc_vtherm_isr, IRQF_ONESHOT,
						"r5t61x_adc_vtherm", info);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Can't get ADC_VTHMH IRQ for chrager: %d\n", ret);
			goto out;
		}

		INIT_DEFERRABLE_WORK(&info->battery_temp_work,
					 battery_temp_irq_work);
	}
#endif

	/* Charger init and IRQ setting */
	ret = ricoh61x_init_charger(info);
	if (ret)
		goto out;

#ifdef	ENABLE_FUEL_GAUGE_FUNCTION
	ret = ricoh61x_init_fgsoca(info);
#endif
	queue_delayed_work(info->monitor_wqueue, &info->monitor_work,
					RICOH61x_MONITOR_START_TIME*HZ);


	/* Enable Charger/ADC interrupt */
	ricoh61x_set_bits(info->dev->parent, RICOH61x_INTC_INTEN, CHG_INT | ADC_INT);

//   	if(sysfs_create_link(info->battery.dev->kobj, info->battery.dev->kobj, "mc13892_bat")) {
//		printk("[%s-%d] create mc13892_bat link fail !\n", __func__, __LINE__);
//	}

#if 0
   	if(sysfs_create_link(&info->dev->parent->parent->parent->parent->parent->parent->kobj, &info->dev->kobj, "pmic_battery.1")) {
		printk("[%s-%d] create pmic_battery.1 link fail !\n", __func__, __LINE__);
	}
#else
	{
		struct device *pDev = info->dev;
		while (pDev->parent)
			pDev = pDev->parent;
		printk ("Create pmic_battery.1 in %s\n",dev_name(pDev));
	   	if(sysfs_create_link(&pDev->kobj, &info->dev->kobj, "pmic_battery.1")) 
			printk("[%s-%d] create pmic_battery.1 link fail !\n", __func__, __LINE__);
	}
#endif

	add_fake_soc_sysfs(&pdev->dev);
	_create_sys_attrs(&pdev->dev);

	add_power_on_reason_sysfs(&pdev->dev);
	add_eval_curr_vals_sysfs(&pdev->dev);
	add_test_states_sysfs(&pdev->dev);
	add_batt_params_sysfs(&pdev->dev);
	return 0;

out:
	kfree(info);
	return ret;
}

static int ricoh61x_battery_remove(struct platform_device *pdev)
{
	struct ricoh61x_battery_info *info = platform_get_drvdata(pdev);
	struct ricoh61x *ricoh619 = dev_get_drvdata(pdev->dev.parent);
	uint8_t val;
	int ret;
	int err;
	int cc_cap = 0;
	long cc_cap_mas = 0;
	bool is_charging = true;

	if (g_fg_on_mode
		 && (info->soca->status == RICOH61x_SOCA_STABLE)) {
		err = ricoh61x_write(info->dev->parent, PSWR_REG, 0x7f);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");
		g_soc = 0x7f;
	 	set_current_time2register(info);

	} else if (info->soca->status != RICOH61x_SOCA_START
		&& info->soca->status != RICOH61x_SOCA_UNSTABLE
		&& info->soca->rsoc_ready_flag != 0) {
		if (info->soca->displayed_soc < 50) {
			val = 1;
		} else {
			val = (info->soca->displayed_soc + 50)/100;
			val &= 0x7f;
		}
		ret = ricoh61x_write(info->dev->parent, PSWR_REG, val);
		if (ret < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");

		g_soc = val;
		set_current_time2register(info);

		ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 1);
		if (ret < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");
	}

	if (g_fg_on_mode == 0) {
		ret = ricoh61x_clr_bits(info->dev->parent,
					 FG_CTRL_REG, 0x01);
		if (ret < 0)
			dev_err(info->dev, "Error in clr FG EN\n");
	}
	
	/* set rapid timer 300 min */
	err = ricoh61x_set_bits(info->dev->parent, TIMSET_REG, 0x03);
	if (err < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
	}
	
	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_FONCHGINT), &info);
	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_FRTIMOVINT), &info);
	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_FCHGCMPINT), &info);
	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_FVUSBDETSINT), &info);
	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_FVADPDETSINT), &info);
#ifdef ENABLE_LOW_BATTERY_DETECTION
//	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_VSYSLIR), &info);
	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_VADPLIR), &info);
#endif
#ifdef ENABLE_BATTERY_TEMP_DETECTION
	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_VTHMLIR), &info);
	free_irq(irq_create_mapping(ricoh619->irq_domain, RICOH61x_IRQ_VTHMHIR), &info);
#endif


	cancel_delayed_work(&info->monitor_work);
	cancel_delayed_work(&info->charge_stable_work);
	cancel_delayed_work(&info->charge_monitor_work);
	cancel_delayed_work(&info->get_charge_work);
	cancel_delayed_work(&info->displayed_work);
	cancel_delayed_work(&info->changed_work);
#ifdef ENABLE_LOW_BATTERY_DETECTION
	cancel_delayed_work(&info->low_battery_work);
#endif
#ifdef ENABLE_BATTERY_TEMP_DETECTION
	if(info->iEnableTempDetect) {
		cancel_delayed_work(&info->battery_temp_work);
	}
#endif
#ifdef ENABLE_FACTORY_MODE
	cancel_delayed_work(&info->factory_mode_work);
#endif
	cancel_delayed_work(&info->jeita_work);
	cancel_work_sync(&info->irq_work);

	flush_workqueue(info->monitor_wqueue);
	flush_workqueue(info->workqueue);
#ifdef ENABLE_FACTORY_MODE
	flush_workqueue(info->factory_mode_wqueue);
#endif

	destroy_workqueue(info->monitor_wqueue);
	destroy_workqueue(info->workqueue);
#ifdef ENABLE_FACTORY_MODE
	destroy_workqueue(info->factory_mode_wqueue);
#endif

	remove_fake_soc_sysfs(&pdev->dev);
	_remove_sys_attrs(&pdev->dev);

	power_supply_unregister(info->battery);
	kfree(info);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM

extern void ricoh_suspend_state_sync(void);
static void _ricoh_suspend_state_sync(struct ricoh61x_battery_info *info)
{
	// If get value from dts , don't enter this function (ricoh_suspend_state_sync)

	if(info->fg_sus_curr!=-1) {
		sus_current = info->fg_sus_curr;
	}

	if(info->fg_hiber_curr!=-1) {
		hiber_current = info->fg_hiber_curr;
	}
	ricoh_suspend_state_sync();
}

static int ricoh61x_battery_suspend(struct device *dev)
{
	struct ricoh61x_battery_info *info = dev_get_drvdata(dev);
	uint8_t val;
	uint8_t val2;
	int ret;
	int err;
	int cc_display2suspend = 0;
	int cc_cap = 0;
	long cc_cap_mas =0;
	bool is_charging = true;
	int displayed_soc_temp;

	SUSPEND_PRINT(KERN_DEBUG"PMU: %s START ================================================================================\n", __func__);

	_ricoh_suspend_state_sync(info);

	get_current_time(info, &info->sleepEntryTime);
//	dev_info(info->dev, "sleep entry time : %lu secs\n",
//				info->sleepEntryTime);

#ifdef ENABLE_MASKING_INTERRUPT_IN_SLEEP
	ricoh61x_clr_bits(dev->parent, RICOH61x_INTC_INTEN, CHG_INT);
#endif
	info->stop_disp = true;
	info->soca->suspend_full_flg = false;

	if (g_fg_on_mode
		 && (info->soca->status == RICOH61x_SOCA_STABLE)) {
		err = ricoh61x_write(info->dev->parent, PSWR_REG, 0x7f);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");
		 g_soc = 0x7F;
		set_current_time2register(info);

		info->soca->suspend_soc = info->soca->displayed_soc;
		ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 1);
		if (ret < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");

		info->soca->suspend_cc = 0;

	} else {
		if(info->soca->status == RICOH61x_SOCA_LOW_VOL){
			ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
					 &is_charging, 1);
			if (ret < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");
				
			if(is_charging == true){
				info->soca->cc_delta = cc_cap;
				//cc_cap_mas;
			}else {
				info->soca->cc_delta = -cc_cap;
				cc_cap_mas = -cc_cap_mas;
			}

			info->soca->temp_cc_delta_cap_mas += cc_cap_mas - info->soca->last_cc_delta_cap_mas;

			SUSPEND_PRINT(KERN_DEBUG"PMU : %s : Suspend : temp_cc_delta_cap_mas is %ld, cc_delta is %ld, last_cc_delta_cap_mas is %ld\n"
					,__func__, info->soca->temp_cc_delta_cap_mas, cc_cap_mas, info->soca->last_cc_delta_cap_mas);
			displayed_soc_temp = info->soca->displayed_soc;

			if ((info->soca->displayed_soc + 50)/100 >= 100) {
				displayed_soc_temp = min(10000, displayed_soc_temp);
			} else {
				displayed_soc_temp = min(9949, displayed_soc_temp);
			}
			displayed_soc_temp = max(0, displayed_soc_temp);
			info->soca->displayed_soc = displayed_soc_temp;

			info->soca->suspend_soc = info->soca->displayed_soc;
			info->soca->suspend_cc = 0;
			info->soca->suspend_rsoc = calc_capacity_2(info);

		}else if (info->soca->rsoc_ready_flag == 0
			|| info->soca->status == RICOH61x_SOCA_START
			|| info->soca->status == RICOH61x_SOCA_UNSTABLE) {
			ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
								 &is_charging, 2);
			if (ret < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");
				
			if(is_charging == true){
				info->soca->cc_delta = cc_cap;
				//cc_cap_mas;
			}else {
				info->soca->cc_delta = -cc_cap;
				cc_cap_mas = -cc_cap_mas;
			}

			//info->soca->temp_cc_delta_cap_mas += cc_cap_mas - info->soca->last_cc_delta_cap_mas;

			//get charge/discharge value from displayed work to suspend start
			cc_display2suspend = info->soca->cc_delta - info->soca->last_cc_rrf0;
			cc_display2suspend = min(400, cc_display2suspend);	//fail-safe
			cc_display2suspend = max(-400, cc_display2suspend);
			info->soca->last_cc_rrf0 = 0;

			SUSPEND_PRINT(KERN_DEBUG"PMU : %s : Suspend : temp_cc_delta_cap_mas is %ld, cc_delta is %ld, last_cc_delta_cap_mas is %ld, cc_display2suspend is %d\n"
					,__func__, info->soca->temp_cc_delta_cap_mas, cc_cap_mas, info->soca->last_cc_delta_cap_mas, cc_display2suspend);

			if (info->soca->status == RICOH61x_SOCA_START
				|| info->soca->status == RICOH61x_SOCA_UNSTABLE
				|| info->soca->status == RICOH61x_SOCA_STABLE) {
				displayed_soc_temp
					 = info->soca->init_pswr * 100 + info->soca->cc_delta;
			} else {
				if(info->soca->status == RICOH61x_SOCA_FULL){
					info->soca->temp_cc_delta_cap += cc_display2suspend;
					displayed_soc_temp
					= info->soca->displayed_soc;// + (info->soca->cc_delta/100) *100;
				} else {
					displayed_soc_temp
					= info->soca->displayed_soc + cc_display2suspend;
				}
			}

			if ((info->soca->displayed_soc + 50)/100 >= 100) {
				displayed_soc_temp = min(10000, displayed_soc_temp);
			} else {
				displayed_soc_temp = min(9949, displayed_soc_temp);
			}
			displayed_soc_temp = max(0, displayed_soc_temp);
			info->soca->displayed_soc = displayed_soc_temp;

			info->soca->suspend_soc = info->soca->displayed_soc;
			info->soca->suspend_cc = info->soca->cc_delta % 100;

			val = info->soca->init_pswr + (info->soca->cc_delta/100);
			val = min(100, val);
			val = max(1, val);

			info->soca->init_pswr = val;
			info->soca->suspend_rsoc = (info->soca->init_pswr * 100) + (info->soca->cc_delta % 100);

		} else {
			ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
								 &is_charging, 1);
			if (ret < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");

			if(is_charging == true){
				info->soca->cc_delta = cc_cap;
				//cc_cap_mas;
			}else {
				info->soca->cc_delta = -cc_cap;
				cc_cap_mas = -cc_cap_mas;
			}

			//info->soca->temp_cc_delta_cap_mas += cc_cap_mas - info->soca->last_cc_delta_cap_mas;
				SUSPEND_PRINT(KERN_DEBUG"PMU : %s : Suspend : temp_cc_delta_cap_mas is %ld, cc_delta is %ld, last_cc_delta_cap_mas is %ld\n"
					,__func__, info->soca->temp_cc_delta_cap_mas, cc_cap_mas, info->soca->last_cc_delta_cap_mas);

			if (info->soca->status == RICOH61x_SOCA_FULL){
				info->soca->temp_cc_delta_cap += info->soca->cc_delta;
				displayed_soc_temp = info->soca->displayed_soc;
			} else {
				displayed_soc_temp
					 = info->soca->displayed_soc + info->soca->cc_delta;
			}
			
			if ((info->soca->displayed_soc + 50)/100 >= 100) {
				displayed_soc_temp = min(10000, displayed_soc_temp);
			} else {
				displayed_soc_temp = min(9949, displayed_soc_temp);
			}
			displayed_soc_temp = max(0, displayed_soc_temp);
			info->soca->displayed_soc = displayed_soc_temp;

			info->soca->suspend_soc = info->soca->displayed_soc;
			info->soca->suspend_cc = 0;
			info->soca->suspend_rsoc = calc_capacity_2(info);

		}

		SUSPEND_PRINT(KERN_DEBUG "PMU: %s status(%d), rrf(%d), suspend_soc(%d), suspend_cc(%d)\n",
		 __func__, info->soca->status, info->soca->rsoc_ready_flag, info->soca->suspend_soc, info->soca->suspend_cc);
		SUSPEND_PRINT(KERN_DEBUG "PMU: %s DSOC(%d), init_pswr(%d), cc_delta(%d)\n",
		__func__, info->soca->displayed_soc, info->soca->init_pswr, info->soca->cc_delta);

		if (info->soca->displayed_soc < 50) {
			val = 1;
		} else {
			val = (info->soca->displayed_soc + 50)/100;
			val &= 0x7f;
		}
		ret = ricoh61x_write(info->dev->parent, PSWR_REG, val);
		if (ret < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");

		g_soc = val;
		set_current_time2register(info);

	}

	ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
					 &is_charging, 0);
	info->soca->cc_delta
		 = (is_charging == true) ? cc_cap : -cc_cap;

	SUSPEND_PRINT(KERN_DEBUG "PMU: %s : STATUS(%d), DSOC(%d), RSOC(%d), init_pswr*100(%d), cc_delta(%d) ====================\n",
		 __func__, info->soca->status, displayed_soc_temp,
		 calc_capacity_2(info), info->soca->init_pswr*100, info->soca->cc_delta);

	if (info->soca->status == RICOH61x_SOCA_DISP
		|| info->soca->status == RICOH61x_SOCA_STABLE
		|| info->soca->status == RICOH61x_SOCA_FULL) {
		info->soca->soc = calc_capacity_2(info);
		info->soca->soc_delta =
			info->soca->soc_delta + (info->soca->soc - info->soca->last_soc);

	} else {
		info->soca->soc_delta = 0;
	}

	if (info->soca->status == RICOH61x_SOCA_FULL)
	{
		info->soca->suspend_full_flg = true;
		info->soca->status = RICOH61x_SOCA_DISP;
	}

	if (info->soca->status == RICOH61x_SOCA_LOW_VOL)
	{
		//reset current information
		info->soca->hurry_up_flg = 0;
	}

	info->soca->store_fl_current = fl_current;
	info->soca->store_slp_state = slp_state;
	info->soca->store_sus_current = sus_current;
	info->soca->store_hiber_current = hiber_current;

	SUSPEND_PRINT(KERN_DEBUG "PMU: %s : fl_current(%d), slp_state(%d), sus_current(%d), hiber_current(%d)\n",
		 __func__, 	info->soca->store_fl_current, info->soca->store_slp_state,
		info->soca->store_sus_current, info->soca->store_hiber_current);

	/* set rapid timer 300 min */
	err = ricoh61x_set_bits(info->dev->parent, TIMSET_REG, 0x03);
	if (err < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
	}
	
	/* Enable VBAT threshold Low interrupt */
	err = ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x02);
	if (err < 0) {
		dev_err(info->dev, "Error in settind VBAT ThrLow\n");
	}
	info->suspend_state = true;

	//Enable relaxtion state
	/* set relaxation state */
	if (RICOH61x_REL1_SEL_VALUE > 240)
		val = 0x0F;
	else
		val = RICOH61x_REL1_SEL_VALUE / 16 ;

	/* set relaxation state */
	if (RICOH61x_REL2_SEL_VALUE > 120)
		val2 = 0x0F;
	else
		val2 = RICOH61x_REL2_SEL_VALUE / 8 ;

	val =  val + (val2 << 4);

	err = ricoh61x_write_bank1(info->dev->parent, BAT_REL_SEL_REG, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing BAT_REL_SEL_REG\n");
	}

	
	cancel_delayed_work(&info->monitor_work);
	cancel_delayed_work(&info->displayed_work);
	cancel_delayed_work(&info->charge_stable_work);
	cancel_delayed_work(&info->charge_monitor_work);
	cancel_delayed_work(&info->get_charge_work);
	cancel_delayed_work(&info->changed_work);
#ifdef ENABLE_LOW_BATTERY_DETECTION
	cancel_delayed_work(&info->low_battery_work);
#endif
#ifdef ENABLE_BATTERY_TEMP_DETECTION
	if(info->iEnableTempDetect) {
		cancel_delayed_work(&info->battery_temp_work);
	}
#endif
#ifdef ENABLE_FACTORY_MODE
	cancel_delayed_work(&info->factory_mode_work);
#endif
	cancel_delayed_work(&info->jeita_work);
/*	flush_work(&info->irq_work); */

	return 0;
}

/**
* get SOC value during period of Suspend/Hibernation
* this function is only run discharge state
* info : battery info
* Period : sleep period
* sleepCurrent : sleep current
*
* return value : delta soc, unit is "minus" 0.01%
*/

static int calc_cc_value_by_sleepPeriod(struct ricoh61x_battery_info *info, unsigned long Period, int sleepCurrent)
{
	int fa_cap;	//unit is mAh
	unsigned long delta_cap;		//unit is uAs
	unsigned long delta_soc;		//unit is 0.01%
	
	fa_cap = (battery_init_para[info->num][22]<<8)
					 | (battery_init_para[info->num][23]);
	
	if(fa_cap == 0){
		// avoiding 0 divied
		return 0;
	} else {
		// Check Suspend current is normal

		// delta_cap[uAs] = Period[s] * (Suspend current + FL current)[uA]
		delta_cap = (Period * sleepCurrent) + info->soca->sus_cc_cap_offset;
		//delta_soc[0.01%] = (delta_cap/1000)[mAs] * 10000/ (fa_cap * 60 * 60)[mAs];
		delta_soc = (delta_cap / (fa_cap * 36)) / 10;

		//info->soca->sus_cc_cap_offset[uAs] = delta_cap[uAs] - (delta_soc[0.01%] * (fa_cap[mAs] * 60 * 60/ (100 * 100)))*1000;
		info->soca->sus_cc_cap_offset = delta_cap - (delta_soc * (fa_cap * 360));
		//0.01% uAs => fa_cap*360
		info->soca->sus_cc_cap_offset = min((360*fa_cap), info->soca->sus_cc_cap_offset);
		info->soca->sus_cc_cap_offset = max(0, info->soca->sus_cc_cap_offset);

		delta_soc = min(10000, delta_soc);
		delta_soc = max(0, delta_soc);

		SUSPEND_PRINT(KERN_DEBUG"PMU : %s : delta_cap is %ld [uAs], Period is %ld [s], fa_cap is %d [mAh], delta_soc is %ld [0.01%%], offset is %d [uAs]\n"
			,__func__, delta_cap, Period, fa_cap, delta_soc, info->soca->sus_cc_cap_offset);
	}

	return (-1 * delta_soc);
}

/**
* get SOC value during period of Suspend/Hibernate with current method
* info : battery info
* Period : sleep period
* is_low_current : flag of calculation method for sleep current
*
* return value : soc, unit is 0.01%
*/

static int calc_soc_by_currentMethod(struct ricoh61x_battery_info *info, unsigned long Period, bool *is_low_current)
{
	int soc;
	int sleepCurrent;	// unit is uA

	if(info->soca->store_slp_state == 0) {
		// Check Suspend current is normal
		if ((info->soca->store_sus_current <= 0)
			 || (info->soca->store_sus_current > RICOH61x_SLEEP_CURRENT_LIMIT)) {
			if ((sus_current <= 0) || (sus_current > RICOH61x_SLEEP_CURRENT_LIMIT)) {
				info->soca->store_sus_current = RICOH61x_SUS_CURRENT_DEF;
			} else {
				info->soca->store_sus_current = sus_current;
			}
		}

		if ((info->soca->store_fl_current < 0)
			 || (info->soca->store_fl_current > RICOH61x_FL_CURRENT_LIMIT)) {
			if ((fl_current < 0) || (fl_current > RICOH61x_FL_CURRENT_LIMIT)) {
				info->soca->store_fl_current = RICOH61x_FL_CURRENT_DEF;
			} else {
				info->soca->store_fl_current = fl_current;
			}
		}

		sleepCurrent = info->soca->store_sus_current + info->soca->store_fl_current;

		if (sleepCurrent < RICOH61x_SUS_CURRENT_THRESH) {
			// Calculate cc_delta from [Suspend current * Sleep period]
			info->soca->cc_delta = calc_cc_value_by_sleepPeriod(info, Period, sleepCurrent);
			*is_low_current = true;
			SUSPEND_PRINT(KERN_DEBUG "PMU: %s Suspend(S/W) slp_current(%d), sus_current(%d), fl_current(%d), cc_delta(%d) ----------\n",
				 __func__, sleepCurrent, info->soca->store_sus_current, info->soca->store_fl_current, info->soca->cc_delta);
		} else {
			// Calculate cc_delta between Sleep-In and Sleep-Out
			info->soca->cc_delta -= info->soca->suspend_cc;
			*is_low_current = false;
			SUSPEND_PRINT(KERN_DEBUG "PMU: %s Suspend(H/W) slp_current(%d), sus_current(%d), fl_current(%d), cc_delta(%d) ----------\n",
				 __func__, sleepCurrent, info->soca->store_sus_current, info->soca->store_fl_current, info->soca->cc_delta);
		}
	} else {
		// Check Hibernate current is normal
		if ((info->soca->store_hiber_current <= 0)
			 || (info->soca->store_hiber_current > RICOH61x_SLEEP_CURRENT_LIMIT)) {
			if ((hiber_current <= 0) || (hiber_current > RICOH61x_SLEEP_CURRENT_LIMIT)) {
				sleepCurrent = RICOH61x_HIBER_CURRENT_DEF;
			} else {
				sleepCurrent = hiber_current;
			}
		} else {
			sleepCurrent = info->soca->store_hiber_current;
		}
		// Calculate cc_delta from [Hibernate current * Sleep period]
		info->soca->cc_delta = calc_cc_value_by_sleepPeriod(info, Period, sleepCurrent);
		*is_low_current = true;
		SUSPEND_PRINT(KERN_INFO "PMU: %s Hibernate(S/W) hiber_current(%d), cc_delta(%d) ----------\n",
			 __func__, sleepCurrent, info->soca->cc_delta);
	}

	soc = info->soca->suspend_soc + info->soca->cc_delta;

	SUSPEND_PRINT(KERN_DEBUG"PMU : %s : slp_state is %d, soc is %d [0.01%%]  ----------\n"
		, __func__, info->soca->store_slp_state, soc);

	// soc range is 0~10000
	return soc;
}

static int ricoh61x_battery_resume(struct device *dev)
{
	struct ricoh61x_battery_info *info = dev_get_drvdata(dev);
	uint8_t val;
	int ret;
	int displayed_soc_temp;
	int cc_cap = 0;
	long cc_cap_mas = 0;
	bool is_charging = true;
	bool is_jeita_updated;
	int i;
	unsigned long suspend_period_time;		//unit is sec
	int soc_voltage, soc_current;
//	int resume_rsoc;
	int fa_cap;	//unit is mAh
	bool is_low_current = false;
	static int Cnt_ApplyVolMethod;

	fa_cap = (battery_init_para[info->num][22]<<8)
					 | (battery_init_para[info->num][23]);

	SUSPEND_PRINT(KERN_DEBUG"PMU: %s START ================================================================================\n", __func__);

	get_current_time(info, &info->sleepExitTime);
//	dev_info(info->dev, "sleep exit time : %lu secs\n",
//				info->sleepExitTime);

	suspend_period_time = info->sleepExitTime - info->sleepEntryTime;


#ifdef STANDBY_MODE_DEBUG
	if(multiple_sleep_mode == 0) {
//		suspend_period_time *= 1;
	} else if(multiple_sleep_mode == 1) {
		suspend_period_time *= 60;
	} else if(multiple_sleep_mode == 2) {
		suspend_period_time *= 3600;
	}
#endif

	SUSPEND_PRINT(KERN_DEBUG"PMU : %s : 	suspend_period_time is %lu, sleepExitTime is %lu sleepEntryTime is %lu ==========\n",
		 __func__, suspend_period_time,info->sleepExitTime,info->sleepEntryTime);
	
	/* Clear VBAT threshold Low interrupt */
	ret = ricoh61x_clr_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x02);
	if (ret < 0) {
		dev_err(info->dev, "Error in clearing VBAT ThrLow\n");
	}
	
	ricoh61x_read (info->dev->parent, RICOH61x_INT_IR_ADCL, &val);
	SUSPEND_PRINT(KERN_DEBUG"PMU : %s : 	RICOH61x_INT_IR_ADCL 0x%02X\n",	 __func__, val);
	if (val & 0x02) {
		printk("PMU : %s : 	Wakup by VBATL !!!!!\n", __func__);
		queue_delayed_work(info->monitor_wqueue, &info->low_battery_work,
						LOW_BATTERY_DETECTION_TIME*HZ);
	}


#ifdef ENABLE_MASKING_INTERRUPT_IN_SLEEP
	ricoh61x_set_bits(dev->parent, RICOH61x_INTC_INTEN, CHG_INT);
#endif
	ret = check_jeita_status(info, &is_jeita_updated);
	if (ret < 0) {
		dev_err(info->dev, "Error in updating JEITA %d\n", ret);
	}

	if (info->entry_factory_mode) {
		info->soca->displayed_soc = -EINVAL;
	} else {
		info->soca->soc = info->soca->suspend_soc + info->soca->suspend_cc;

		if (RICOH61x_SOCA_START == info->soca->status
			|| RICOH61x_SOCA_UNSTABLE == info->soca->status
			|| info->soca->rsoc_ready_flag == 0) {
			ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 2);
			if (ret < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");
			info->soca->cc_delta
				 = (is_charging == true) ? cc_cap : -cc_cap;

			//this is for CC delta issue for Hibernate
			if((info->soca->cc_delta - info->soca->suspend_cc) <= 0){
				// Discharge Processing
				SUSPEND_PRINT(KERN_DEBUG "PMU: %s : Discharge Processing (rrf=0)\n", __func__);

				// Calculate SOC by Current Method
				soc_current = calc_soc_by_currentMethod(info, suspend_period_time, &is_low_current);

				// Calculate SOC by Voltage Method
				soc_voltage = calc_soc_by_voltageMethod(info);
				
				SUSPEND_PRINT(KERN_DEBUG "PMU: %s : soc_current(%d), soc_voltage(%d), Diff(%d), is_low_current(%d), Cnt_ApplyVolMethod (%d) ==========\n",
					 __func__, soc_current, soc_voltage, (soc_current - soc_voltage), is_low_current, Cnt_ApplyVolMethod);

				// If difference is small, use current method. If not, use voltage method.
				if ((false == is_low_current) || (30 > suspend_period_time) || ((soc_current - soc_voltage) < 1000) || (5 > Cnt_ApplyVolMethod++)) {
					if ((soc_current - soc_voltage) < 1000)
						Cnt_ApplyVolMethod = 0;
					// Use Current method if difference is small
					if (gptHWCFG->m_val.bBattery == 16) { // EVE254385N-1000mA
						if (200 < (soc_current - soc_voltage)) {
							//increase consumption when difference > 2%
							info->soca->cc_delta += ((info->soca->cc_delta-2)/2);
							if (400 < (soc_current - soc_voltage))
								  info->soca->cc_delta += info->soca->cc_delta;
							SUSPEND_PRINT(KERN_DEBUG "PMU: %s: Update cc_delta %d\n", __func__, info->soca->cc_delta);
						}
						else if (200 < (soc_voltage - soc_current)) {
							//helf consumption when difference > 2%
							info->soca->cc_delta -= ((info->soca->cc_delta-2)/2);
							if (400 < (soc_voltage - soc_current))
								info->soca->cc_delta -= info->soca->cc_delta;
							SUSPEND_PRINT(KERN_DEBUG "PMU: %s: Update cc_delta %d\n", __func__, info->soca->cc_delta);
						}
					}
					
					soc_current = info->soca->suspend_soc + info->soca->cc_delta;
					displayed_soc_temp = soc_current;
					update_rsoc_on_currentMethod(info, soc_current);
					if((info->soca->status == RICOH61x_SOCA_LOW_VOL) && (is_low_current == false)){

						cc_cap_mas = -cc_cap_mas;
						//info->soca->temp_cc_delta_cap_mas += cc_cap_mas - ((fa_cap * 3600) * (info->soca->cc_delta / 10000);
						info->soca->temp_cc_delta_cap_mas += cc_cap_mas - (((fa_cap * 9) / 25) * info->soca->cc_delta);

						SUSPEND_PRINT(KERN_DEBUG "PMU: %s : temp_cc_delta_cap_mas(%d), extra value(%d) ==========\n",
							__func__,info->soca->temp_cc_delta_cap_mas, (cc_cap_mas -(((fa_cap * 9) / 25) * info->soca->cc_delta)));
					}
				} else {
					Cnt_ApplyVolMethod = 0;
					// Use Voltage method if difference is large
					displayed_soc_temp = soc_voltage;
					update_rsoc_on_voltageMethod(info, soc_voltage);
				}
			} else {
				// Charge Processing
				val = info->soca->init_pswr + (info->soca->cc_delta/100);
				val = min(100, val);
				val = max(1, val);

				info->soca->init_pswr = val;

				if (RICOH61x_SOCA_START == info->soca->status
					|| RICOH61x_SOCA_UNSTABLE == info->soca->status
					|| RICOH61x_SOCA_STABLE == info->soca->status) {
//					displayed_soc_temp = info->soca->suspend_soc + info->soca->cc_delta;
					displayed_soc_temp = (info->soca->init_pswr*100) + info->soca->cc_delta%100;
				} else {
					info->soca->cc_delta = info->soca->cc_delta - info->soca->suspend_cc;
					if ((info->soca->cc_delta < 400) && (info->soca->suspend_full_flg == true)){
						displayed_soc_temp = info->soca->suspend_soc;
						info->soca->temp_cc_delta_cap += info->soca->cc_delta;
						info->soca->cc_delta = info->soca->suspend_cc;
						SUSPEND_PRINT(KERN_DEBUG"PMU: %s : under 400 cc_delta is %d, temp_cc_delta is %d\n",
							__func__, info->soca->cc_delta, info->soca->temp_cc_delta_cap);
					}else {
						displayed_soc_temp = info->soca->suspend_soc + info->soca->cc_delta;
						info->soca->temp_cc_delta_cap = 0;
					}
				}
			}

		} else { 
			ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
							 &is_charging, 1);
			if (ret < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");
			info->soca->cc_delta
				 = (is_charging == true) ? cc_cap : -cc_cap;
			
			SUSPEND_PRINT(KERN_DEBUG"PMU: %s cc_delta(%d), suspend_cc(%d), Diff(%d)\n",
				 __func__, info->soca->cc_delta, info->soca->suspend_cc, (info->soca->cc_delta - info->soca->suspend_cc));
			//this is for CC delta issue for Hibernate
			if((info->soca->cc_delta - info->soca->suspend_cc) <= 0){
				// Discharge Processing
				SUSPEND_PRINT(KERN_INFO "PMU: %s : Discharge Processing (rrf=1)\n", __func__);

				// Calculate SOC by Current Method
				soc_current = calc_soc_by_currentMethod(info, suspend_period_time, &is_low_current);

				// Calculate SOC by Voltage Method
				soc_voltage = calc_soc_by_voltageMethod(info);

				SUSPEND_PRINT(KERN_DEBUG "PMU: %s : soc_current(%d), soc_voltage(%d), Diff(%d), is_low_current(%d), Cnt_ApplyVolMethod (%d) ==========\n",
					 __func__, soc_current, soc_voltage, (soc_current - soc_voltage), is_low_current, Cnt_ApplyVolMethod);

				// If difference is small, use current method. If not, use voltage method.
				if ((false == is_low_current) || (30 > suspend_period_time) || ((soc_current - soc_voltage) < 1000) || (5 > Cnt_ApplyVolMethod++)) {
					if ((soc_current - soc_voltage) < 1000)
						Cnt_ApplyVolMethod = 0;
					// Use Current method if difference is small
					displayed_soc_temp = soc_current;
					update_rsoc_on_currentMethod(info, soc_current);
					
					if((info->soca->status == RICOH61x_SOCA_LOW_VOL) && (is_low_current == false)){

						cc_cap_mas = -cc_cap_mas;
						//info->soca->temp_cc_delta_cap_mas += cc_cap_mas - ((fa_cap * 3600) * (info->soca->cc_delta / 10000);
						info->soca->temp_cc_delta_cap_mas += cc_cap_mas - (((fa_cap * 9) / 25) * info->soca->cc_delta);

						SUSPEND_PRINT( "PMU: %s : temp_cc_delta_cap_mas(%d), extra value(%d) ==========\n",
							__func__,info->soca->temp_cc_delta_cap_mas, (cc_cap_mas -(((fa_cap * 9) / 25) * info->soca->cc_delta)));
					}
				} else {
					Cnt_ApplyVolMethod = 0;
					// Use Voltage method if difference is large
					displayed_soc_temp = soc_voltage;
					update_rsoc_on_voltageMethod(info, soc_voltage);
				}
			} else {
				// Charge Processing
				info->soca->cc_delta = info->soca->cc_delta - info->soca->suspend_cc;
				if ((info->soca->cc_delta < 400) && (info->soca->suspend_full_flg == true)){
					displayed_soc_temp = info->soca->suspend_soc;
					info->soca->temp_cc_delta_cap += info->soca->cc_delta;
					info->soca->cc_delta = info->soca->suspend_cc;
					SUSPEND_PRINT("PMU: %s : under 400 cc_delta is %d, temp_cc_delta is %d\n",
						__func__, info->soca->cc_delta, info->soca->temp_cc_delta_cap);
				}else {
					displayed_soc_temp = info->soca->suspend_soc + info->soca->cc_delta;
					info->soca->temp_cc_delta_cap = 0;
				}
			}
		}

		/* Check "zero_flg" in all states */
		if (info->soca->zero_flg == 1) {
			if((info->soca->Ibat_ave >= 0) 
			|| (displayed_soc_temp >= 100)){
				info->soca->zero_flg = 0;
			} else {
				displayed_soc_temp = 0;
			}
		} else if (displayed_soc_temp < 100) {
			/* keep DSOC = 1 when Vbat is over 3.4V*/
			if( info->fg_poff_vbat != 0) {
				if (info->soca->Vbat_ave < 2000*1000) { /* error value */
					displayed_soc_temp = 100;
				} else if (info->soca->Vbat_ave < info->fg_poff_vbat*1000) {
					displayed_soc_temp = 0;
					info->soca->zero_flg = 1;
				} else {
					displayed_soc_temp = 100;
				}
			}
		}
		displayed_soc_temp = min(10000, displayed_soc_temp);
		displayed_soc_temp = max(0, displayed_soc_temp);

		if (0 == info->soca->jt_limit) {
			check_charge_status_2(info, displayed_soc_temp);
		} else {
			info->soca->displayed_soc = displayed_soc_temp;
		}

		val = (info->soca->displayed_soc + 50)/100;
		val &= 0x7f;
		ret = ricoh61x_write(info->dev->parent, PSWR_REG, val);
		if (ret < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");

		g_soc = val;
		set_current_time2register(info);
		

		if ((RICOH61x_SOCA_DISP == info->soca->status)
			|| (RICOH61x_SOCA_STABLE == info->soca->status)){
			info->soca->last_soc = calc_capacity_2(info);
		}
	}

	/* set rapid timer 240 min to restart rapid charge */
	ricoh61x_set_bits(info->dev->parent, TIMSET_REG, 0x02);
	ricoh61x_clr_bits(info->dev->parent, TIMSET_REG, 0x01);

	/* Enable VBAT threshold Low interrupt */
	ret = ricoh61x_set_bits(info->dev->parent, RICOH61x_INT_EN_ADC1, 0x02);
	if (ret < 0) {
		dev_err(info->dev, "Error in settind VBAT ThrLow\n");
	}

	ret = calc_capacity_in_period(info, &cc_cap, &cc_cap_mas,
					 &is_charging, 0);
	if(is_charging == true) {
		info->soca->cc_delta = cc_cap;
		//cc_cap_mas;
	} else {
		info->soca->cc_delta = -cc_cap;
		cc_cap_mas = -cc_cap_mas;
	}

	//if (info->soca->status == RICOH61x_SOCA_LOW_VOL)
	//{
		info->soca->last_cc_delta_cap = info->soca->cc_delta;
		info->soca->last_cc_delta_cap_mas = cc_cap_mas;
	//}
		


	SUSPEND_PRINT(KERN_DEBUG "PMU: %s : STATUS(%d), DSOC(%d), RSOC(%d), init_pswr*100(%d), cc_delta(%d) ====================\n",
		 __func__, info->soca->status, displayed_soc_temp, calc_capacity_2(info), info->soca->init_pswr*100, info->soca->cc_delta);

	ret = measure_vbatt_FG(info, &info->soca->Vbat_ave);
	ret = measure_vsys_ADC(info, &info->soca->Vsys_ave);
	ret = measure_Ibatt_FG(info, &info->soca->Ibat_ave);

	//Disable relaxtion state
	ret = ricoh61x_write_bank1(info->dev->parent, BAT_REL_SEL_REG, 0);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing BAT_REL_SEL_REG\n");
	}

	info->stop_disp = false;

	power_supply_changed(info->battery);
	queue_delayed_work(info->monitor_wqueue, &info->displayed_work, HZ);

	if (RICOH61x_SOCA_UNSTABLE == info->soca->status) {
		info->soca->stable_count = 10;
		queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH61x_FG_STABLE_TIME*HZ/10);
	} else if (RICOH61x_SOCA_FG_RESET == info->soca->status) {
		info->soca->stable_count = 1;

		for (i = 0; i < 3; i = i+1)
			info->soca->reset_soc[i] = 0;
		info->soca->reset_count = 0;

		queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH61x_FG_RESET_TIME*HZ);
	}

	queue_delayed_work(info->monitor_wqueue, &info->monitor_work,
						 info->monitor_time);

	queue_delayed_work(info->monitor_wqueue, &info->charge_monitor_work,
					 RICOH61x_CHARGE_RESUME_TIME * HZ);

	info->soca->chg_count = 0;
	queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
					 RICOH61x_CHARGE_RESUME_TIME * HZ);
	if (info->jt_en) {
		if (!info->jt_hw_sw) {
			queue_delayed_work(info->monitor_wqueue, &info->jeita_work,
					 RICOH61x_JEITA_UPDATE_TIME * HZ);
		}
	}

	return 0;
}

static const struct dev_pm_ops ricoh61x_battery_pm_ops = {
	.suspend	= ricoh61x_battery_suspend,
	.resume		= ricoh61x_battery_resume,
};
#endif

static struct platform_driver ricoh61x_battery_driver = {
	.driver	= {
				.name	= "ricoh619-battery",
				.owner	= THIS_MODULE,
#ifdef CONFIG_PM
				.pm	= &ricoh61x_battery_pm_ops,
#endif
	},
	.probe	= ricoh61x_battery_probe,
	.remove	= ricoh61x_battery_remove,
};

static int __init ricoh61x_battery_init(void)
{
	printk(KERN_INFO "PMU: %s\n", __func__);
	return platform_driver_register(&ricoh61x_battery_driver);
}
module_init(ricoh61x_battery_init);

static void __exit ricoh61x_battery_exit(void)
{
	platform_driver_unregister(&ricoh61x_battery_driver);
}
module_exit(ricoh61x_battery_exit);

MODULE_DESCRIPTION("RICOH R5T619 Battery driver");
MODULE_ALIAS("platform:ricoh619-battery");
MODULE_LICENSE("GPL");

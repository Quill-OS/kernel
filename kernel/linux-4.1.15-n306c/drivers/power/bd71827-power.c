/*
 * bd71827-power.c
 * @file ROHM BD71827 Charger driver
 *
 * Copyright 2016.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

//#define DEBUG
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/mfd/rohm-bd718x7.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/mfd/rohm-bd71879.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/rtc.h>
#include <linux/kthread.h>
#include <linux/fs.h>

#include <linux/usb/composite.h>
#include "../usb/chipidea/udc.h"

#include "../../arch/arm/mach-imx/hardware.h"
#include "../../arch/arm/mach-imx/ntx_hwconfig.h"

extern volatile NTX_HWCONFIG *gptHWCFG;

int bd71828_charger_detect(void);

#define MAX(X, Y) ((X) >= (Y) ? (X) : (Y))
#define uAMP_TO_mAMP(ma) ((ma) / 1000)
#define mAMP_TO_uAMP(ma) ((ma) * 1000)

/* BD71828 and BD71827 common defines */
#define BD7182x_MASK_VBAT_U						0x1f
#define BD7182x_MASK_VDCIN_U					0x0f
#define BD7182x_MASK_IBAT_U						0x3f
#define BD7182x_MASK_CURDIR_DISCHG				0x80
#define BD7182x_MASK_CC_CCNTD_HI				0x0FFF
#define BD7182x_MASK_CC_CCNTD					0x0FFFFFFF
#define BD7182x_MASK_CHG_STATE					0x7f
#define BD7182x_MASK_CC_FULL_CLR				0x10
#define BD7182x_MASK_BAT_TEMP					0x07
#define BD7182x_MASK_DCIN_DET					0x01
#define BD7182x_MASK_CONF_PON					0x01
#define BD7182x_MASK_BAT_STAT					0x3f
#define BD7182x_MASK_DCIN_STAT					0x07

#define BD7182x_MASK_CCNTRST					0x80
#define BD7182x_MASK_CCNTENB					0x40
#define BD7182x_MASK_CCCALIB					0x20
#define BD7182x_MASK_WDT_AUTO					0x40
#define BD7182x_MASK_VBAT_ALM_LIMIT_U			0x01
#define BD7182x_MASK_CHG_EN						0x01

#define BD7182x_DCIN_COLLAPSE_DEFAULT			0x36

/* Measured min and max value clear bits */
#define BD7182x_MASK_VSYS_MIN_AVG_CLR			0x10
#define BD7182x_MASK_VBAT_MIN_AVG_CLR			0x01


#define JITTER_DEFAULT							3000		/* hope 3s is enough */
#define JITTER_REPORT_CAP						10000		/* 10 seconds */
#define BATTERY_CAP_MAH_DEFAULT_28				910
#define BATTERY_CAP_MAH_DEFAULT_78				910

#define BATTERY_CAP_MAH_DEFAULT_28_PR284983N	1431//1464
#define BATTERY_CAP_MAH_DEFAULT_28_EVE254385N	962//972

static int BATTERY_CAP_MAH_DEFAULT = BATTERY_CAP_MAH_DEFAULT_28;

#define MAX_VOLTAGE_DEFAULT_28					4200000
#define MAX_VOLTAGE_DEFAULT_78					4200000

#define MIN_VOLTAGE_DEFAULT_28					3400000
#define MIN_VOLTAGE_DEFAULT_78					3200000
#define THR_VOLTAGE_DEFAULT						4150000

#define MIN_VOLTAGE_DEFAULT_28_PR284983N		3520000
#define MIN_VOLTAGE_DEFAULT_28_EVE254385N		3350000

static int MAX_VOLTAGE_DEFAULT = MAX_VOLTAGE_DEFAULT_28;
static int MIN_VOLTAGE_DEFAULT = MIN_VOLTAGE_DEFAULT_28;

#define MAX_CURRENT_DEFAULT						890000		/* uA */
#define AC_NAME									"ac"
#define BAT_NAME								"battery"
#define BATTERY_FULL_DEFAULT					100

#define BY_BAT_VOLT								0
#define BY_VBATLOAD_REG							1
#define INIT_COULOMB							BY_VBATLOAD_REG

#define CALIB_CURRENT_A2A3						0xCE9E

/*
 * VBAT Low voltage detection Threshold 
 * 0x00D4*16mV = 212*0.016 = 3.392v
 */
#define VBAT_LOW_TH								0x00D4		// 0.016 * 212 (0xd4) = 3.392
//#define RS_30mOHM
//#ifdef RS_30mOHM
//#define A10s_mAh(s)		((s) * 1000 / (360 * 3))
//#define mAh_A10s(m)		((m) * (360 * 3) / 1000)
//#else
//#define A10s_mAh(s)		((s) * 1000 / 360)
//#define mAh_A10s(m)		((m) * 360 / 1000)
//#endif
#define A10s_mAh(s,factor)		((s) * 10000 / (36 * factor))
#define mAh_A10s(m,factor)		((m) * (36 * factor) / 10000)

#define THR_RELAX_CURRENT_DEFAULT				5		/* mA */
#define THR_RELAX_TIME_DEFAULT					(60 * 60)	/* sec. */

#define DGRD_CYC_CAP_DEFAULT_28					26	/* 1 micro Ah unit */
#define DGRD_CYC_CAP_DEFAULT_78					15	/* 1 micro Ah unit */
#define DGRD_CYC_CAP_DEFAULT_28_PR284983N		815	/* 1 micro Ah unit */
#define DGRD_CYC_CAP_DEFAULT_28_EVE254385N		81	/* 1 micro Ah unit */
static int DGRD_CYC_CAP_DEFAULT = DGRD_CYC_CAP_DEFAULT_28;

#define DGRD_TEMP_H_28							45	/* 1 degrees C unit */
#define DGRD_TEMP_M_28							25	/* 1 degrees C unit */
#define DGRD_TEMP_L_28							5	/* 1 degrees C unit */

#define DGRD_TEMP_H_78							0	/* 1 degrees C unit */
#define DGRD_TEMP_M_78							0	/* 1 degrees C unit */
#define DGRD_TEMP_L_78							0	/* 1 degrees C unit */
#define DGRD_TEMP_VL_DEFAULT					0	/* 1 degrees C unit */

static int DGRD_TEMP_H_DEFAULT = DGRD_TEMP_H_28;
static int DGRD_TEMP_M_DEFAULT = DGRD_TEMP_M_28;
static int DGRD_TEMP_L_DEFAULT = DGRD_TEMP_L_28;

#define DGRD_TEMP_VL_DEFAULT					0	/* 1 degrees C unit */

#define SOC_EST_MAX_NUM_DEFAULT_28				1
#define SOC_EST_MAX_NUM_DEFAULT_78				5
#define SOC_EST_MAX_NUM_DEFAULT_28_PR284983N	5
#define SOC_EST_MAX_NUM_DEFAULT_28_EVE254385N	5
static int  SOC_EST_MAX_NUM_DEFAULT = SOC_EST_MAX_NUM_DEFAULT_28;

#define DGRD_TEMP_CAP_H_DEFAULT_28				(0)		/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_M_DEFAULT_28				(1187)	/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_L_DEFAULT_28				(5141)	/* 1 micro Ah unit */

#define DGRD_TEMP_CAP_H_DEFAULT_78				(0)		/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_M_DEFAULT_78				(1187)	/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_L_DEFAULT_78				(5141)	/* 1 micro Ah unit */

#define DGRD_TEMP_CAP_H_DEFAULT_28_PR284983N	(0)		/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_M_DEFAULT_28_PR284983N	(0)		/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_L_DEFAULT_28_PR284983N	(0)		/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_H_DEFAULT_28_EVE254385N	(0)		/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_M_DEFAULT_28_EVE254385N	(0)		/* 1 micro Ah unit */
#define DGRD_TEMP_CAP_L_DEFAULT_28_EVE254385N	(0)		/* 1 micro Ah unit */
static int  DGRD_TEMP_CAP_H_DEFAULT = DGRD_TEMP_CAP_H_DEFAULT_28;
static int  DGRD_TEMP_CAP_M_DEFAULT = DGRD_TEMP_CAP_M_DEFAULT_28;
static int  DGRD_TEMP_CAP_L_DEFAULT = DGRD_TEMP_CAP_L_DEFAULT_28;

#define PWRCTRL_NORMAL							0x22
#define PWRCTRL_RESET							0x23

int through_voltage;
extern struct usb_composite_driver chargedetect_driver;
/* TODO: Evaluate which members of "pwr" are really updated/read from separate
 * 	 threads and actually do require memory barriers. Furthermore, evaluate
 * 	 if the smp_rmb() is only required at start of update cycle / start of
 * 	 request callbacks. This current 'call barrier for every access to "pwr"
 * 	 is probably terrible for cache usage on the system...
 */

struct pwr_regs {
	u8 vbat_init;
	u8 vbat_init2;
	u8 vbat_init3;
	u8 vbat_avg;
	u8 ibat;
	u8 ibat_avg;
	u8 vsys_avg;
	u8 vbat_min_avg;
	u8 meas_clear;
	u8 vsys_min_avg;
	u8 btemp_vth;
	u8 chg_state;
	u8 coulomb3;
	u8 coulomb2;
	u8 coulomb1;
	u8 coulomb0;
	u8 coulomb_ctrl;
	u8 vbat_rex_avg;
	u8 rex_clear_reg;
	u8 rex_clear_mask;
	u8 coulomb_full3;
	u8 cc_full_clr;
	u8 coulomb_chg3;
	u8 bat_temp;
	u8 dcin_stat;
	u8 dcin_collapse_limit;
	u8 chg_set1;
	u8 chg_en;
	u8 vbat_alm_limit_u;
	u8 batcap_mon_limit_u;
	u8 conf;
	u8 bat_stat;
	u8 vdcin;
#ifdef PWRCTRL_HACK
	u8 pwrctrl;

#endif // PWRCTRL_HACK
};

struct pwr_regs pwr_regs_bd71827 = {
	.vbat_init = BD71827_REG_VM_OCV_PRE_U,
	.vbat_init2 = BD71827_REG_VM_OCV_PST_U,
	.vbat_init3 = BD71827_REG_VM_OCV_PWRON_U,
	.vbat_avg = BD71827_REG_VM_SA_VBAT_U,
	.ibat = BD71827_REG_CC_CURCD_U,
	.ibat_avg = BD71827_REG_CC_SA_CURCD_U,
	.vsys_avg = BD71827_REG_VM_SA_VSYS_U,
	.vbat_min_avg = BD71827_REG_VM_SA_VBAT_MIN_U,
	.meas_clear = BD71827_REG_VM_SA_MINMAX_CLR,
	.vsys_min_avg = BD71827_REG_VM_SA_VSYS_MIN_U,
	.btemp_vth = BD71827_REG_VM_BTMP,
	.chg_state = BD71827_REG_CHG_STATE,
	.coulomb3 = BD71827_REG_CC_CCNTD_3,
	.coulomb2 = BD71827_REG_CC_CCNTD_2,
	.coulomb1 = BD71827_REG_CC_CCNTD_1,
	.coulomb0 = BD71827_REG_CC_CCNTD_0,
	.coulomb_ctrl = BD71827_REG_CC_CTRL,
	.vbat_rex_avg = BD71827_REG_REX_SA_VBAT_U,
	.rex_clear_reg = BD71827_REG_REX_CTRL_1,
	.rex_clear_mask = BD71827_REX_CLR_MASK,
	.coulomb_full3 = BD71827_REG_FULL_CCNTD_3,
	.cc_full_clr = BD71827_REG_FULL_CTRL,
	.coulomb_chg3 = BD71827_REG_CCNTD_CHG_3,
	.bat_temp = BD71827_REG_BAT_TEMP,
	.dcin_stat = BD71827_REG_DCIN_STAT,
	.dcin_collapse_limit = BD71827_REG_DCIN_CLPS,
	.chg_set1 = BD71827_REG_CHG_SET1,
	.chg_en = BD71827_REG_CHG_SET1,
	.vbat_alm_limit_u = BD71827_REG_ALM_VBAT_TH_U,
	.batcap_mon_limit_u = BD71827_REG_CC_BATCAP1_TH_U,
	.conf = BD71827_REG_CONF,
	.bat_stat = BD71827_REG_BAT_STAT,
	.vdcin = BD71827_REG_VM_DCIN_U,
#ifdef PWRCTRL_HACK
	.pwrctrl = BD71827_REG_PWRCTRL,
	.hibernate_mask = 0x1,
#endif
};

struct pwr_regs pwr_regs_bd71828 = {
	.vbat_init = BD71828_REG_VBAT_INITIAL1_U,
	.vbat_init2 = BD71828_REG_VBAT_INITIAL2_U,
	.vbat_init3 = BD71828_REG_OCV_PWRON_U,
	.vbat_avg = BD71828_REG_VBAT_U,
	.ibat = BD71828_REG_IBAT_U,
	.ibat_avg = BD71828_REG_IBAT_AVG_U,
	.vsys_avg = BD71828_REG_VSYS_AVG_U,
	.vbat_min_avg = BD71828_REG_VBAT_MIN_AVG_U,
	.meas_clear = BD71828_REG_MEAS_CLEAR,
	.vsys_min_avg = BD71828_REG_VSYS_MIN_AVG_U,
	.btemp_vth = BD71828_REG_VM_BTMP_U,
	.chg_state = BD71828_REG_CHG_STATE,
	.coulomb3 = BD71828_REG_CC_CNT3,
	.coulomb2 = BD71828_REG_CC_CNT2,
	.coulomb1 = BD71828_REG_CC_CNT1,
	.coulomb0 = BD71828_REG_CC_CNT0,
	.coulomb_ctrl = BD71828_REG_COULOMB_CTRL,
	.vbat_rex_avg = BD71828_REG_VBAT_REX_AVG_U,
	.rex_clear_reg = BD71828_REG_COULOMB_CTRL2,
	.rex_clear_mask = BD71828_MASK_REX_CC_CLR,
	.coulomb_full3 = BD71828_REG_CC_CNT_FULL3,
	.cc_full_clr = BD71828_REG_COULOMB_CTRL2,
	.coulomb_chg3 = BD71828_REG_CC_CNT_CHG3,
	.bat_temp = BD71828_REG_BAT_TEMP,
	.dcin_stat = BD71828_REG_DCIN_STAT,
	.dcin_collapse_limit = BD71828_REG_DCIN_CLPS,
	.chg_set1 = BD71828_REG_CHG_SET1,
	.chg_en   = BD71828_REG_CHG_EN,
	.vbat_alm_limit_u = BD71828_REG_ALM_VBAT_LIMIT_U,
	.batcap_mon_limit_u = BD71828_REG_BATCAP_MON_LIMIT_U,
	.conf = BD71828_REG_CONF,
	.bat_stat = BD71828_REG_BAT_STAT,
	.vdcin = BD71828_REG_VDCIN_U,
#ifdef PWRCTRL_HACK
	.pwrctrl = BD71828_REG_PS_CTRL_1,
	.hibernate_mask = 0x2,
#endif
};

//static struct task_struct *etx_thread = NULL;
//static long ciritical_low_flag = 0;
static long ciritical_low_count = 0;

static int soc_table_default[23] = {
	1000, 1000, 950, 900, 850, 800, 750, 700, 650, 600, 550, 500, 450, 400, 350, 300, 250, 200, 150, 100, 50, 0, -50
};

static int ocv_table_78[23] = {
	4200000, 4183673, 4133087, 4088990, 4050001, 3999386, 3969737, 3941923, 3914141, 3876458, 3840151, 3818242, 3803144, 3791427, 3782452, 3774388, 3759613, 3739858, 3713895, 3691682, 3625561, 3278893, 1625099
};

static int vdr_table_h_78[23] = {
	100, 100, 101, 101, 102, 102, 103, 103, 104, 104, 105, 105, 106, 106, 107, 107, 108, 108, 108, 112, 136, 215, 834
};

static int vdr_table_m_78[23] = {
	100, 100, 101, 102, 104, 105, 106, 107, 109, 110, 111, 112, 114, 115, 116, 117, 118, 111, 111, 118, 141, 202, 526
};

static int vdr_table_l_78[23] = {
	100, 100, 102, 104, 105, 107, 109, 111, 113, 114, 116, 118, 120, 121, 123, 125, 127, 132, 141, 168, 249, 276, 427
};

static int vdr_table_vl_78[23] = {
	100, 100, 102, 104, 107, 109, 111, 113, 115, 117, 120, 122, 124, 126, 128, 131, 134, 144, 161, 201, 284, 382, 479
};

static int ocv_table_28[23] = {
	4200000, 4167456, 4109781, 4065242, 4025618, 3989877, 3958031, 3929302, 3900935, 3869637, 3838475, 3815196, 3799778, 3788385, 3779627, 3770675, 3755368, 3736049, 3713545, 3685118, 3645278, 3465599, 2830610
};

static int vdr_table_h_28[23] = {
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100
};

static int vdr_table_m_28[23] = {
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100
};

static int vdr_table_l_28[23] = {
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100
};

static int vdr_table_vl_28[23] = {
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100
};

static int ocv_table_28_PR284983N[23] = {
	//4200000, 4162288, 4110762, 4066502, 4025265, 3988454, 3955695, 3926323, 3900244, 3876035, 3834038, 3809386, 3794093, 3782718, 3774483, 3768044, 3748158, 3728750, 3704388, 3675577, 3650676, 3463852, 2768530
	4200000, 4166349, 4114949, 4072016, 4031575, 3995353, 3963956, 3935650, 3910161, 3883395, 3845310, 3817535, 3801354, 3789708, 3781393, 3774994, 3765230, 3749035, 3726707, 3699147, 3671953, 3607301, 3148394
};

static int vdr_table_h_28_PR284983N[23] = {
	//100, 100, 101, 101, 102, 102, 103, 103, 104, 104, 105, 106, 106, 107, 107, 108, 108, 109, 110, 112, 124, 157, 786
	100, 100, 101, 102, 102, 105, 106, 107, 112, 108, 108, 105, 105, 108, 110, 110, 110, 111, 112, 114, 120, 131, 620
};

static int vdr_table_m_28_PR284983N[23] = {
	//100, 100, 101, 101, 102, 102, 103, 103, 104, 104, 105, 102, 100, 100, 102, 103, 103, 105, 108, 112, 124, 157, 586
	100, 100, 103, 106, 110, 114, 115, 119, 122, 122, 115, 113, 112, 114, 117, 124, 126, 123, 122, 126, 140, 156, 558
};

static int vdr_table_l_28_PR284983N[23] = {
	//100, 100, 103, 105, 110, 110, 113, 112, 112, 112, 105, 110, 110, 111, 122, 131, 138, 143, 150, 166, 242, 354, 357
	100, 100, 105, 110, 114, 117, 121, 125, 126, 122, 116, 114, 115, 118, 124, 132, 140, 148, 156, 170, 210, 355, 579
};

static int vdr_table_vl_28_PR284983N[23] = {
	//100, 100, 103, 106, 108, 111, 114, 117, 118, 115, 108, 106, 108, 113, 115, 114, 118, 125, 144, 159, 204, 361, 874
	100, 100, 109, 115, 118, 123, 127, 130, 140, 139, 134, 130, 128, 138, 140, 150, 154, 164, 178, 204, 271, 362, 352
};

static int ocv_table_28_EVE254385N[23] = {
	//4200000, 4168448, 4118382, 4081056, 4022653, 3980665, 3953661, 3922665, 3885431, 3844195, 3817680, 3799564, 3787086, 3781909, 3779384, 3774286, 3761315, 3743249, 3694915, 3612078, 3547464, 3473038, 3176971
	4200000, 4171210, 4117125, 4074718, 4015549, 3976113, 3944032, 3910847, 3867808, 3830473, 3805610, 3789527, 3782943, 3780541, 3777092, 3763087, 3723143, 3663123, 3603970, 3555157, 3508877, 3408363, 3072054
};

static int ocv_table_28_PR284983N_3p60[23] = {
	4200000, 4166666, 4116873, 4074990, 4035777, 4000425, 3969471, 3941807, 3916760, 3892890, 3863633, 3827848, 3806835, 3794639, 3785042, 3778677, 3773142, 3762234, 3745469, 3722882, 3695570, 3673969, 3598702
};

static int vdr_table_h_28_PR284983N_3p60[23] = {
	100, 100, 82, 84, 86, 88, 95, 100, 100, 105, 99, 93, 92, 92, 90, 95, 105, 114, 110, 111, 113, 120, 161
};

static int vdr_table_m_28_PR284983N_3p60[23] = {
	100, 100, 108, 116, 124, 128, 133, 141, 142, 147, 142, 116, 109, 111, 113, 119, 127, 128, 125, 126, 131, 136, 181
};

static int vdr_table_l_28_PR284983N_3p60[23] = {
	100, 85, 105, 111, 119, 122, 130, 128, 131, 132, 128, 117, 117, 119, 124, 133, 140, 151, 161, 173, 192, 207, 234
};

static int vdr_table_vl_28_PR284983N_3p60[23] = {
	100, 100, 105, 106, 108, 108, 110, 110, 112, 116, 115, 108, 105, 106, 108, 116, 121, 126, 133, 143, 153, 164, 177
};

static int vdr_table_h_28_EVE254385N[23] = {
	//100, 100, 104, 108, 109, 112, 117, 126, 134, 115, 111, 111, 112, 116, 122, 120, 115, 115, 121, 123, 125, 125, 202
	100, 100, 109, 118, 124, 129, 138, 155, 146, 129, 124, 124, 126, 131, 143, 145, 148, 147, 141, 141, 151, 174, 441
};

static int vdr_table_m_28_EVE254385N[23] = {
	//100, 100, 106, 112, 114, 119, 129, 142, 139, 117, 111, 111, 114, 119, 127, 132, 127, 127, 136, 133, 138, 149, 255
	100, 100, 109, 118, 123, 131, 144, 164, 147, 127, 120, 120, 123, 131, 144, 161, 171, 158, 143, 141, 155, 172, 450
};

static int vdr_table_l_28_EVE254385N[23] = {
	//100, 100, 100, 100, 100, 100, 101, 103, 95, 90, 88, 90, 93, 95, 98, 102, 106, 127, 132, 136, 139, 195, 342 
	100, 100, 109, 118, 122, 127, 124, 125, 120, 116, 113, 113, 118, 123, 131, 153, 159, 154, 147, 146, 172, 268, 214
};

static int vdr_table_vl_28_EVE254385N[23] = {
	//100, 100, 100, 102, 102, 101, 98, 95, 85, 80, 75, 78, 80, 82, 85, 90, 95, 100, 111, 123, 125, 125, 202 
	100, 100, 108, 117, 111, 112, 114, 106, 103, 100, 97, 98, 103, 111, 118, 148, 151, 147, 144, 149, 164, 273, 362
};

static int *ocv_table_default = ocv_table_28;
static int *vdr_table_h_default = vdr_table_h_28;
static int *vdr_table_m_default = vdr_table_m_28;
static int *vdr_table_l_default = vdr_table_l_28;
static int *vdr_table_vl_default = vdr_table_vl_28;

int use_load_bat_params;

static int battery_cap_mah;
static int battery_cap;

int dgrd_cyc_cap;

int soc_est_max_num;

int dgrd_temp_cap_h;
int dgrd_temp_cap_m;
int dgrd_temp_cap_l;

static unsigned int battery_cycle;

int ocv_table[23];
int soc_table[23];
int vdr_table_h[23];
int vdr_table_m[23];
int vdr_table_l[23];
int vdr_table_vl[23];

struct bd7182x_soc_data {
	int    vbus_status;		/**< last vbus status */
	int    charge_status;		/**< last charge status */
	int    bat_status;		/**< last bat status */

	int	bat_online;		/**< battery connect */
	int	charger_online;		/**< charger connect */
	int	vcell;			/**< battery voltage */
	int	vsys;			/**< system voltage */
	int	vcell_min;		/**< minimum battery voltage */
	int	vsys_min;		/**< minimum system voltage */
	int	rpt_status;		/**< battery status report */
	int	prev_rpt_status;	/**< previous battery status report */
	int	bat_health;		/**< battery health */
	int	designed_cap;		/**< battery designed capacity */
	int	full_cap;		/**< battery capacity */
	int	curr;			/**< battery current from ADC */
	int	curr_avg;		/**< average battery current */
	int	temp;			/**< battery tempature */
	u32	coulomb_cnt;		/**< Coulomb Counter */
	int	state_machine;		/**< initial-procedure state machine */

	u32	soc_norm;		/**< State Of Charge using full
					     capacity without by load */
	u32	soc;			/**< State Of Charge using full
					     capacity with by load */
	u32	clamp_soc;		/**< Clamped State Of Charge using
					     full capacity with by load */

	int	relax_time;		/**< Relax Time */

	u32	cycle;			/**< Charging and Discharging cycle
					     number */
};

/** @brief power deivce */
struct bd71827_power {
	struct rohm_regmap_dev *mfd;	/**< parent for access register */
	struct power_supply *ac;	/**< alternating current power */
	struct power_supply *usb;	/**< alternating current power */
	struct power_supply *bat;	/**< battery power */
	int gauge_delay;		/**< Schedule to call gauge algorithm */
	struct bd7182x_soc_data d_r;	/**< SOC algoritm data for reporting */
	struct bd7182x_soc_data d_w;	/**< internal SOC algoritm data */
	spinlock_t dlock;
	struct delayed_work bd_work;	/**< delayed work for timed work */
	struct delayed_work charger_detect_work;	/**< charger detection work */

	struct pwr_regs *regs;
	/* Reg val to uA */
	int curr_factor;
	int (*get_temp) (struct bd71827_power *pwr, int *temp);
	enum rohm_chip_type chip_type;
	u32 dc_in_jiffies;
	u32 dc_out_jiffies;
	u32 resume_jiffies;
	u32 suspend_resume;
	u32 boot_jiffies;

    int charger_type;
};

static void update_soc_data(struct bd71827_power *pwr);

static ssize_t charger_type_read(struct device *dev, struct device_attribute *attr,char *buf)
{
    struct platform_device *pdev = container_of(dev, struct platform_device, dev);
    struct bd71827_power *pwr = platform_get_drvdata(pdev);

	switch (pwr->charger_type) {
		case BD_71879_CHARGER_TYPE_DCP:
			sprintf (buf,"DCP\n");
			break;
		case BD_71879_CHARGER_TYPE_SDP:
			sprintf (buf,"SDP_PC\n");
			break;
		case BD_71879_CHARGER_TYPE_NONE:
		default:
			sprintf (buf,"NO\n");
			break;
	}
	return strlen(buf);
}

static ssize_t batt_params_read(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct bd71827_power *pwr = platform_get_drvdata(pdev);
	int len = 0,total_len=0;
	char *pcBuf = buf;
	int i;

	len = sprintf(pcBuf,"Battery=%d (0x%02x)\n",gptHWCFG->m_val.bBattery,gptHWCFG->m_val.bBattery);
	pcBuf += len;
	total_len += len;

	len = sprintf(pcBuf,"ocv_table[] = {\n");pcBuf += len;total_len += len;
	for(i=0;i<23;i++) {
			len = sprintf(pcBuf,"[%d] %d uV\n",i,ocv_table_default[i]);pcBuf += len;total_len += len;
	}
	len = sprintf(pcBuf,"}\n");pcBuf += len;total_len += len;

	len = sprintf(pcBuf,"0%% = %d uV\n",MIN_VOLTAGE_DEFAULT);pcBuf += len;total_len += len;
	len = sprintf(pcBuf,"100%% = %d uV\n",MAX_VOLTAGE_DEFAULT);pcBuf += len;total_len += len;
	len = sprintf(pcBuf,"Battery_Full_Cap=%d mAh\n",BATTERY_CAP_MAH_DEFAULT);pcBuf += len;total_len += len;
	

	return total_len;
}

static DEVICE_ATTR(charger_type, S_IRUGO, charger_type_read, NULL);
static DEVICE_ATTR(batt_params, S_IRUGO, batt_params_read, NULL);
static int _create_sys_attrs(struct device *dev)
{
	int err;
	err = device_create_file(dev,&dev_attr_charger_type);
	if (err) {
		pr_err("Can't create rohm charger type attr sysfs !\n");
		return -1;
	}
	err = device_create_file(dev,&dev_attr_batt_params);
	if (err) {
		pr_err("Can't create rohm batt_params attr sysfs !\n");
		return -1;
	}
	return 0;
}

static void _remove_sys_attrs(struct device *dev)
{
	device_remove_file(dev,&dev_attr_charger_type);
	device_remove_file(dev,&dev_attr_batt_params);
}

#define CALIB_NORM			0
#define CALIB_START			1
#define CALIB_GO			2

enum {
	STAT_POWER_ON,
	STAT_INITIALIZED,
};

u32 bd71827_calc_soc_org(u32 cc, int designed_cap);

static struct platform_device *gPwr_dev;

bool bd71828_is_ready(void)
{
	if (gPwr_dev)
		return true;
	else
		return false;
}

int bd71828_get_battery_capacity(void)
{
	struct power_supply	*psy;
	union power_supply_propval capacity;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL){
		printk("%s():error when get power_supply structure\n",__func__);
	} else {
		ret = power_supply_get_property(psy,POWER_SUPPLY_PROP_CAPACITY,&capacity);
		if (!ret) {
			//printk("%s():battery capacity=%d\n",__func__,capacity.intval);
			return capacity.intval;
		} else {
			printk("%s():error reading capacity\n",__func__);
		}
	}
	return -EPERM;
}

static int bd7182x_write16(struct bd71827_power *pwr, int reg, uint16_t val)
{

	val = cpu_to_be16(val);

	return regmap_bulk_write(pwr->mfd->regmap, reg, &val, sizeof(val));
}

static int bd7182x_read16_himask(struct bd71827_power *pwr, int reg, int himask,
				 uint16_t *val)
{
	struct regmap *regmap = pwr->mfd->regmap;
	int ret;
	u8 *tmp = (u8 *) val;

	ret = regmap_bulk_read(regmap, reg, val, sizeof(*val));
	if (!ret) {
		*tmp &= himask;
		*val = be16_to_cpu(*val);
	}
	return ret;
}


#if INIT_COULOMB == BY_VBATLOAD_REG
#define INITIAL_OCV_REGS 4
/** @brief get initial battery voltage and current
 * @param pwr power device
 * @return 0
 */
static int bd71827_get_init_bat_stat(struct bd71827_power *pwr,
				     int *ocv, int flag)
{
	int ret;
	int i;
	u8 regs[INITIAL_OCV_REGS] = {
		pwr->regs->vbat_init,
		pwr->regs->vbat_init2,
		pwr->regs->vbat_init3,
		0x84
	 };
	uint16_t vals[INITIAL_OCV_REGS];

	*ocv = 0;

	for (i = (flag?0:3); i < INITIAL_OCV_REGS; i++) {

		ret = bd7182x_read16_himask(pwr, regs[i], BD7182x_MASK_VBAT_U,
					    &vals[i]);
		if (ret) {
			dev_err(pwr->mfd->dev,
				"Failed to read initial battery voltage\n");
			return ret;
		}
		*ocv = MAX(vals[i], *ocv);

		dev_err(pwr->mfd->dev, "VM_OCV_%d = %d\n", i,
			((int)vals[i]) * 1000);
	}

	*ocv *= 1000;

	return ret;
}
#endif

/** @brief get battery average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd71827_get_vbat(struct bd71827_power *pwr, int *vcell)
{
	uint16_t tmp_vcell;
	int ret,count,tempValue=0,totalValue=0;
	static int vbat_record[10] = {0}, index=0;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vbat_avg,
				    BD7182x_MASK_VBAT_U, &tmp_vcell);
	if(ret)
		dev_err(pwr->mfd->dev,
			"Failed to read battery average voltage\n");
	else
		/**vcell*/tempValue = ((int)tmp_vcell);// * 1000;

	vbat_record[index++] = tempValue;
	if(index>=10)
		index = 0;
	for(count=0;count<10;count++) {
		if(vbat_record[count] == 0)
			break;
		totalValue += vbat_record[count];
	}

	*vcell =  (totalValue * 1000)/count;

	return ret;
}

#if INIT_COULOMB == BY_BAT_VOLT
/** @brief get battery average voltage and current
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @param curr  pointer to return back current in unit uA.
 * @return 0
 */
static int bd71827_get_vbat_curr(struct bd71827_power *pwr, int *vcell, int *curr)
{
	int ret;

	ret = bd71827_get_vbat(pwr, vcell);
	*curr = 0;
	
	return ret;
}
#endif


/** @brief get battery current and battery average current from DS-ADC
 * @param pwr power device
 * @param current in unit uA
 * @param average current in unit uA
 * @return 0
 */
static int bd71827_get_current_ds_adc(struct bd71827_power *pwr, int *curr, int *curr_avg)
{
	uint16_t tmp_curr;
	char *tmp = (char *)&tmp_curr;
	int dir = 1;
	int regs[] = { pwr->regs->ibat, pwr->regs->ibat_avg };
	int *vals[] = { curr, curr_avg };
	int ret, i;

	for (dir = 1, i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = regmap_bulk_read(pwr->mfd->regmap, regs[i], &tmp_curr,
				       sizeof(tmp_curr));
		if (ret)
			break;

		if (*tmp & BD7182x_MASK_CURDIR_DISCHG)
			dir = -1;

		*tmp &= BD7182x_MASK_IBAT_U;
		tmp_curr = be16_to_cpu(tmp_curr);

		*vals[i] = dir * ((int)tmp_curr) * pwr->curr_factor;
	}

	return ret;
}

/** @brief get system average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd71827_get_vsys(struct bd71827_power *pwr, int *vsys)
{
	uint16_t tmp_vsys;
	int ret,count,tempValue=0,totalValue=0;
	static int vsys_record[10] = {0}, index=0;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vsys_avg,
				    BD7182x_MASK_VBAT_U, &tmp_vsys);
	if(ret)
		dev_err(pwr->mfd->dev,
			"Failed to read system average voltage\n");
	else
		/**vsys*/tempValue = ((int)tmp_vsys);// * 1000;

	if(pwr->suspend_resume) {
		for(count=0;count<10;count++)
			vsys_record[count] = 0;
		index = 0;
	}

	vsys_record[index++] = tempValue;
	if(index>=10)
		index = 0;
	for(count=0;count<10;count++) {
		if(vsys_record[count] == 0)
			break;
		totalValue += vsys_record[count];
	}

	*vsys =  (totalValue * 1000)/count;

	return ret;
}

/** @brief get battery minimum average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd71827_get_vbat_min(struct bd71827_power *pwr, int *vcell)
{
	uint16_t tmp_vcell;
	int ret,count,tempValue=0,totalValue=0;
	static int vbat_min_record[10] = {0}, index=0;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vbat_min_avg,
				    BD7182x_MASK_VBAT_U, &tmp_vcell);
	if(ret)
		dev_err(pwr->mfd->dev,
			"Failed to read battery min average voltage\n");
	else
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->meas_clear,
					 BD7182x_MASK_VBAT_MIN_AVG_CLR, 
					 BD7182x_MASK_VBAT_MIN_AVG_CLR);

	/**vcell*/tempValue = ((int)tmp_vcell);// * 1000;

	if(pwr->suspend_resume) {
		for(count=0;count<10;count++)
			vbat_min_record[count] = 0;
		index = 0;
	}

	vbat_min_record[index++] = tempValue;
	if(index>=10)
		index = 0;
	for(count=0;count<10;count++) {
		if(vbat_min_record[count] == 0)
			break;
		totalValue += vbat_min_record[count];
	}

	*vcell =  (totalValue * 1000)/count;

	return ret;
}

/** @brief get system minimum average voltage
 * @param pwr power device
 * @param vcell pointer to return back voltage in unit uV.
 * @return 0
 */
static int bd71827_get_vsys_min(struct bd71827_power *pwr, int *vcell)
{
	uint16_t tmp_vcell;
	int ret,count,tempValue=0,totalValue=0;
	static int vsys_min_record[10] = {0}, index=0;

	ret = bd7182x_read16_himask(pwr, pwr->regs->vsys_min_avg,
				    BD7182x_MASK_VBAT_U, &tmp_vcell);
	if(ret)
		dev_err(pwr->mfd->dev,
			"Failed to read system min average voltage\n");
	else
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->meas_clear,
					 BD7182x_MASK_VSYS_MIN_AVG_CLR, 
					 BD7182x_MASK_VSYS_MIN_AVG_CLR);

	/**vcell*/tempValue = ((int)tmp_vcell);// * 1000;

	if(pwr->suspend_resume) {
		for(count=0;count<10;count++)
			vsys_min_record[count] = 0;
		index = 0;
	}

	vsys_min_record[index++] = tempValue;
	if(index>=10)
		index = 0;
	for(count=0;count<10;count++) {
		if(vsys_min_record[count] == 0)
			break;
		totalValue += vsys_min_record[count];
	}

	*vcell =  (totalValue * 1000)/count;

	return ret;
}

/** @brief get battery capacity
 * @param ocv open circuit voltage
 * @return capcity in unit 0.1 percent
 */
static int bd71827_voltage_to_capacity(int ocv)
{
	int i = 0;
	int soc;

	if (ocv > ocv_table[0]) {
		soc = soc_table[0];
	} else {
		for (i = 0; soc_table[i] != -50; i++) {
			if ((ocv <= ocv_table[i]) && (ocv > ocv_table[i+1])) {
				soc = (soc_table[i] - soc_table[i+1]) *
				      (ocv - ocv_table[i+1]) /
				      (ocv_table[i] - ocv_table[i+1]);
				soc += soc_table[i+1];
				break;
			}
		}
		if (soc_table[i] == -50)
			soc = soc_table[i];
	}
	return soc;
}

/** @brief get battery temperature
 * @param pwr power device
 * @return temperature in unit deg.Celsius
 */
static int bd71827_get_temp(struct bd71827_power *pwr, int *temp)
{
	struct regmap *regmap = pwr->mfd->regmap;
	int ret;
	int t;

	ret = regmap_read(regmap, pwr->regs->btemp_vth, &t);
	t = 200 - t;

	if (ret || t > 200) {
		dev_err(pwr->mfd->dev, "Failed to read battery temperature\n");
		*temp = 200;
	} else {
		*temp = t;
	}

	return ret;
}

static int bd71828_get_temp(struct bd71827_power *pwr, int *temp)
{
	uint16_t t;
	int ret;
	int tmp = 200 * 10000;

	ret = bd7182x_read16_himask(pwr, pwr->regs->btemp_vth,
				    BD71828_MASK_VM_BTMP_U, &t);
	if(ret || t > 3200)
		dev_err(pwr->mfd->dev,
			"Failed to read system min average voltage\n");

	tmp -= 625ULL * (unsigned int)t;
	*temp = tmp / 10000;

	return ret;
}

static int bd71827_reset_coulomb_count(struct bd71827_power* pwr,
				       struct bd7182x_soc_data *wd);

/** @brief get battery charge status
 * @param pwr power device
 * @return temperature in unit deg.Celsius
 */
static int bd71827_charge_status(struct bd71827_power *pwr,
				 struct bd7182x_soc_data *wd, int mode)
{
	unsigned int state;
	int ret = 1;

	wd->prev_rpt_status = wd->rpt_status;

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_state, &state);
	if (ret)
		dev_err(pwr->mfd->dev, "charger status reading failed (%d)\n", ret);

	state &= BD7182x_MASK_CHG_STATE;

	dev_dbg(pwr->mfd->dev, "%s(): CHG_STATE %d\n", __func__, state);

	switch (state) {
	case 0x00:
		ret = 0;
		wd->rpt_status = POWER_SUPPLY_STATUS_DISCHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x0E:
		wd->rpt_status = POWER_SUPPLY_STATUS_CHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x0F:
		ret = 0;
		wd->rpt_status = POWER_SUPPLY_STATUS_FULL;
		wd->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
		ret = 0;
		wd->rpt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	case 0x30:
	case 0x31:
	case 0x32:
	case 0x40:
		ret = 0;
		wd->rpt_status = POWER_SUPPLY_STATUS_DISCHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x7f:
	default:
		ret = 0;
		wd->rpt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		wd->bat_health = POWER_SUPPLY_HEALTH_DEAD;
		break;	
	}

	if(mode)
		bd71827_reset_coulomb_count(pwr, wd);

	return ret;
}

#if INIT_COULOMB == BY_BAT_VOLT
static int bd71827_calib_voltage(struct bd71827_power* pwr, int* ocv)
{
	int r, curr, volt, ret;

	bd71827_get_vbat_curr(pwr, &volt, &curr);
	
	ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_state, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Charger state reading failed (%d)\n",
			ret);
	} else if (curr > 0) {
		// voltage increment caused by battery inner resistor
		if (r == 3) volt -= 100 * 1000;
		else if (r == 2) volt -= 50 * 1000;
	}
	*ocv = volt;

	return 0;
}
#endif
static int __write_cc(struct bd71827_power* pwr, uint16_t bcap,
		      unsigned int reg, uint32_t *new)
{
	int ret;
	uint32_t tmp;
	uint16_t *swap_hi = (uint16_t *)&tmp;
	uint16_t *swap_lo = swap_hi + 1;

	*swap_hi = cpu_to_be16(bcap & BD7182x_MASK_CC_CCNTD_HI);
	*swap_lo = 0;

	ret = regmap_bulk_write(pwr->mfd->regmap, reg, &tmp, sizeof(tmp));
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to write coulomb counter\n");
		return ret;
	}
	if (new)
		*new = cpu_to_be32(tmp);

	return ret;
}

static int write_cc(struct bd71827_power* pwr, uint16_t bcap)
{
	int ret;
	uint32_t new;

	ret = __write_cc(pwr, bcap, pwr->regs->coulomb3, &new);
	if (!ret)
		pwr->d_w.coulomb_cnt = new;

	return ret;
}

static int stop_cc(struct bd71827_power* pwr)
{
	struct regmap *r = pwr->mfd->regmap;

	return regmap_update_bits(r, pwr->regs->coulomb_ctrl,
				  BD7182x_MASK_CCNTENB, 0);
}

static int start_cc(struct bd71827_power* pwr)
{
	struct regmap *r = pwr->mfd->regmap;

	return regmap_update_bits(r, pwr->regs->coulomb_ctrl,
				  BD7182x_MASK_CCNTENB, BD7182x_MASK_CCNTENB);
}

static int update_cc(struct bd71827_power* pwr, uint16_t bcap)
{
	int ret;

	ret = stop_cc(pwr);
	if (ret)
		goto err_out;

	ret = write_cc(pwr, bcap);
	if (ret)
		goto enable_out;

	ret = start_cc(pwr);
	if (ret)
		goto enable_out;

	return 0;

enable_out:
	start_cc(pwr);
err_out:
	dev_err(pwr->mfd->dev, "Coulomb counter write failed  (%d)\n", ret);
	return ret;
}

static int __read_cc(struct bd71827_power* pwr, u32 *cc, unsigned int reg)
{
	int ret;
	u32 tmp_cc;

	ret = regmap_bulk_read(pwr->mfd->regmap, reg, &tmp_cc, sizeof(tmp_cc));
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read coulomb counter\n");
		return ret;
	}
	*cc = be32_to_cpu(tmp_cc) & BD7182x_MASK_CC_CCNTD;

	return 0;
}

static int read_cc_full(struct bd71827_power* pwr, u32 *cc)
{
	return __read_cc(pwr, cc, pwr->regs->coulomb_full3);
}

static int read_cc(struct bd71827_power* pwr, u32 *cc)
{
	return __read_cc(pwr, cc, pwr->regs->coulomb3);
}

static int limit_cc(struct bd71827_power* pwr, struct bd7182x_soc_data *wd,
		    u32 *soc_org)
{
	uint16_t bcap;
	int ret;

	*soc_org = 100;
	bcap = wd->designed_cap + wd->designed_cap / 200;
	ret = update_cc(pwr, bcap);
	
	dev_dbg(pwr->mfd->dev,  "Limit Coulomb Counter\n");
	dev_dbg(pwr->mfd->dev,  "CC_CCNTD = %d\n", wd->coulomb_cnt);

	return ret;
}

static int sync_cc(struct bd71827_power* pwr, struct bd7182x_soc_data *wd,
		    u32 soc_org)
{
	uint16_t bcap;
	int ret;

	bcap = ((wd->designed_cap + wd->designed_cap / 200) * soc_org) / 100;
	ret = update_cc(pwr, bcap);

	dev_err(pwr->mfd->dev,  "Sync Coulomb Counter\n");
	dev_err(pwr->mfd->dev,  "CC_CCNTD = %d\n", wd->coulomb_cnt);

	return ret;
}

/** @brief set initial coulomb counter value from battery voltage
 * @param pwr power device
 * @return 0
 */
static int calibration_coulomb_counter(struct bd71827_power* pwr,
				       struct bd7182x_soc_data *wd, int flag)
{
	struct regmap *regmap = pwr->mfd->regmap;
	u32 bcap;
	int soc, ocv, ret = 0, tmpret = 0;

#if INIT_COULOMB == BY_VBATLOAD_REG
	/* Get init OCV by HW */
	bd71827_get_init_bat_stat(pwr, &ocv, flag);
#elif INIT_COULOMB == BY_BAT_VOLT
	bd71827_calib_voltage(pwr, &ocv);
#endif
	dev_err(pwr->mfd->dev, "ocv %d\n", ocv);
	//store ocv for debug
	if(flag)
		regmap_bulk_write(pwr->mfd->regmap, 0xf0, &ocv, sizeof(ocv));

	/* Get init soc from ocv/soc table */
	soc = bd71827_voltage_to_capacity(ocv);
	dev_err(pwr->mfd->dev, "soc %d[0.1%%]\n", soc);
	if (soc < 0)
		soc = 0;
	bcap = wd->designed_cap * soc / 1000;

	tmpret = write_cc(pwr, bcap + wd->designed_cap / 200);
	if (tmpret)
		goto enable_cc_out;

	dev_err(pwr->mfd->dev, "%s() CC_CCNTD = %d\n", __func__,
		wd->coulomb_cnt);

enable_cc_out:
	/* Start canceling offset of the DS ADC. This needs 1 second at least */
	ret = regmap_update_bits(regmap, pwr->regs->coulomb_ctrl,
				 BD7182x_MASK_CCCALIB, BD7182x_MASK_CCCALIB);

	return (tmpret) ? tmpret : ret;
}

struct rohm_rtc {
	u8 sec;
	u8 min;
	u8 hour;
	u8 week;
	u8 day;
	u8 month;
	u8 year;
} __packed;

static inline void rtc2time(struct rohm_rtc *r, struct rtc_time *t)
{
	t->tm_sec = bcd2bin(r->sec & BD70528_MASK_RTC_SEC);
	t->tm_min = bcd2bin(r->min & BD70528_MASK_RTC_MINUTE);
	t->tm_hour = bcd2bin(r->hour & BD70528_MASK_RTC_HOUR);
	/*
	 * If RTC is in 12H mode, then bit BD70528_MASK_RTC_HOUR_PM
	 * is not BCD value but tells whether it is AM or PM
	 */
	if (!(r->hour & BD70528_MASK_RTC_HOUR_24H)) {
		t->tm_hour %= 12;
		if (r->hour & BD70528_MASK_RTC_HOUR_PM)
			t->tm_hour += 12;
	}
	t->tm_mday = bcd2bin(r->day & BD70528_MASK_RTC_DAY);
	t->tm_mon = bcd2bin(r->month & BD70528_MASK_RTC_MONTH) - 1;
	t->tm_year = 100 + bcd2bin(r->year & BD70528_MASK_RTC_YEAR);
	t->tm_wday = bcd2bin(r->week & BD70528_MASK_RTC_WEEK);
}

/** @brief adjust coulomb counter values at relaxed state
 * @param pwr power device
 * @return 0
 */
static int bd71827_adjust_coulomb_count(struct bd71827_power* pwr,
					struct bd7182x_soc_data *wd, int mode)
{
	int relax_ocv=0;
	uint16_t tmp;
	struct regmap *regmap = pwr->mfd->regmap;
	int ret;
	struct rohm_rtc current_rtc, last_poweroff_rtc;
	struct rtc_time current_time, last_poweroff_time;
	time64_t current_seconds, last_poweroff_seconds;

	if(mode) {
		ret = regmap_bulk_read(regmap, BD71879_REG_RTC_SEC, &current_rtc, sizeof(current_rtc));
		ret |= regmap_bulk_read(regmap, 0xf4, &last_poweroff_rtc, sizeof(last_poweroff_rtc));
		if (ret) {
			dev_err(pwr->mfd->dev, "Failed to read RTC time (err %d)\n", ret);
		}
		else {
			rtc2time(&current_rtc, &current_time);
			rtc2time(&last_poweroff_rtc, &last_poweroff_time);
			current_seconds = rtc_tm_to_time64(&current_time);
			last_poweroff_seconds = rtc_tm_to_time64(&last_poweroff_time);
			dev_err(pwr->mfd->dev, "Current second %ld\n", current_seconds);
			dev_err(pwr->mfd->dev, "Last power off second %ld\n", last_poweroff_seconds);
			if(current_seconds < (last_poweroff_seconds+1296000)) {
				dev_err(pwr->mfd->dev, "Till HBNT over 15 dsys to relax check\n");
				return 0;
			}
			else
				dev_err(pwr->mfd->dev, "Check Relax voltage\n");
		}
	}

	ret = bd7182x_read16_himask(pwr, pwr->regs->vbat_rex_avg,
				    BD7182x_MASK_VBAT_U, &tmp);
	if (ret)
		return ret;

	relax_ocv = ((int)tmp) * 1000;

	if (relax_ocv != 0) {
		u32 bcap;
		int soc;

		dev_err(pwr->mfd->dev,  "%s(): relax_ocv = 0x%x\n", __func__,
			relax_ocv);
		/* Clear Relaxed Coulomb Counter */
		ret = regmap_update_bits(regmap, pwr->regs->rex_clear_reg,
					 pwr->regs->rex_clear_mask,
					 pwr->regs->rex_clear_mask);
		if (ret)
			return ret;

		/* Get soc at relaxed state from ocv/soc table */
		soc = bd71827_voltage_to_capacity(relax_ocv);
		dev_err(pwr->mfd->dev,  "soc %d[0.1%%]\n", soc);
		if (soc < 0)
			soc = 0;

		bcap = wd->designed_cap * soc / 1000;
		bcap = (bcap + wd->designed_cap / 200);

		ret = update_cc(pwr, bcap);
		if (ret)
			return ret;

		dev_err(pwr->mfd->dev,
			"Adjust Coulomb Counter at Relaxed State\n");
		dev_err(pwr->mfd->dev, "CC_CCNTD = %d\n",
			wd->coulomb_cnt);
		dev_err(pwr->mfd->dev,
			"relaxed_ocv:%d, bcap:%d, soc:%d, coulomb_cnt:%d\n",
			relax_ocv, bcap, soc, wd->coulomb_cnt);


		/* If the following commented out code is enabled, the SOC is not clamped at the relax time. */
		/* Reset SOCs */
		/* bd71827_calc_soc_org(pwr, wd); */
		/* wd->soc_norm = wd->soc_org; */
		/* wd->soc = wd->soc_norm; */
		/* wd->clamp_soc = wd->soc; */
	}

	return ret;
}

/** @brief reset coulomb counter values at full charged state
 * @param pwr power device
 * @return 0
 */
static int bd71827_reset_coulomb_count(struct bd71827_power* pwr,
				       struct bd7182x_soc_data *wd)
{
	u32 full_charged_coulomb_cnt;
	struct regmap *regmap = pwr->mfd->regmap;
	int ret;

	ret = read_cc_full(pwr, &full_charged_coulomb_cnt);
	if (ret) {
		dev_err(pwr->mfd->dev, "failed to read full coulomb counter\n");
		return ret;
	}

	if (full_charged_coulomb_cnt != 0) {
		int diff_coulomb_cnt;
		u32 cc;
		uint16_t bcap;

		dev_err(pwr->mfd->dev, "%s(): full_charged_coulomb_cnt=0x%x\n", __func__, full_charged_coulomb_cnt);
		/* Clear Full Charged Coulomb Counter */
		ret = regmap_update_bits(regmap, pwr->regs->cc_full_clr,
					 BD7182x_MASK_CC_FULL_CLR,
					 BD7182x_MASK_CC_FULL_CLR);

		bd71827_charge_status(pwr, wd, 0);
		dev_err(pwr->mfd->dev, "battery status %d\n", wd->rpt_status);
		if(wd->rpt_status != POWER_SUPPLY_STATUS_FULL) {
			dev_err(pwr->mfd->dev, "Battery charge status isn't POWER_SUPPLY_STATUS_FULL !!!!!! Ignored!!!!!\n");
			return 0;
		}

		ret = read_cc(pwr, &cc);
		if (ret)
			return ret;

		diff_coulomb_cnt = full_charged_coulomb_cnt - cc;

		diff_coulomb_cnt = diff_coulomb_cnt >> 16;
		if (diff_coulomb_cnt > 0)
			diff_coulomb_cnt = 0;

		dev_err(pwr->mfd->dev,  "diff_coulomb_cnt = %d\n", diff_coulomb_cnt);
		if(diff_coulomb_cnt!=0)
			dev_err(pwr->mfd->dev,  "diff_coulomb_cnt %d[for designed_cap]\n", (abs(diff_coulomb_cnt)*100)/wd->designed_cap);

		bcap = wd->designed_cap + wd->designed_cap / 200 +
		       diff_coulomb_cnt;
		ret = update_cc(pwr, bcap);
		if (ret)
			return ret;
		dev_err(pwr->mfd->dev,
			"Reset Coulomb Counter at POWER_SUPPLY_STATUS_FULL\n");
		dev_err(pwr->mfd->dev,"CC_CCNTD = %d\n", wd->coulomb_cnt);
	}

	return 0;
}

/** @brief get battery parameters, such as voltages, currents, temperatures.
 * @param pwr power device
 * @return 0
 */
static int bd71827_get_voltage_current(struct bd71827_power* pwr,
				       struct bd7182x_soc_data *wd)
{
	int ret;
	int temp, temp2;

	if (pwr->chip_type != ROHM_CHIP_TYPE_BD71828 &&
	    pwr->chip_type != ROHM_CHIP_TYPE_BD71827) {
		return -EINVAL;
	}

	ret = bd71827_get_vbat(pwr, &temp);
	if (ret)
		return ret;

	wd->vcell = temp;
	ret = bd71827_get_current_ds_adc(pwr, &temp, &temp2);
	if (ret)
		return ret;
	wd->curr_avg = temp2;
	wd->curr = temp;

	/* Read detailed vsys */
	ret = bd71827_get_vsys(pwr, &temp);
	if (ret)
		return ret;

	wd->vsys = temp;
	dev_dbg(pwr->mfd->dev,  "VM_VSYS = %d\n", temp);

	/* Read detailed vbat_min */
	ret = bd71827_get_vbat_min(pwr, &temp);
	if (ret)
		return ret;
	wd->vcell_min = temp;
	dev_dbg(pwr->mfd->dev,  "VM_VBAT_MIN = %d\n", temp);

	/* Read detailed vsys_min */
	ret = bd71827_get_vsys_min(pwr, &temp);
	if (ret)
		return ret;

	wd->vsys_min = temp;
	dev_dbg(pwr->mfd->dev,  "VM_VSYS_MIN = %d\n", temp);

	/* Get tempature */
	ret = pwr->get_temp(pwr, &temp);

	if (ret)
		return ret;

	wd->temp = temp;

	return 0;
}

/** @brief adjust coulomb counter values at relaxed state by SW
 * @param pwr power device
 * @return 0
 */


static int bd71827_adjust_coulomb_count_sw(struct bd71827_power* pwr,
					   struct bd7182x_soc_data *wd)
{
	int tmp_curr_mA, ret;

	tmp_curr_mA = uAMP_TO_mAMP(wd->curr);
	if ((tmp_curr_mA * tmp_curr_mA) <=
	    (THR_RELAX_CURRENT_DEFAULT * THR_RELAX_CURRENT_DEFAULT))
		 /* No load */
		wd->relax_time = wd->relax_time + (JITTER_DEFAULT / 1000);
	else
		wd->relax_time = 0;

	dev_dbg(pwr->mfd->dev,  "%s(): pwr->relax_time = 0x%x\n", __func__,
		wd->relax_time);
	if (wd->relax_time >= THR_RELAX_TIME_DEFAULT) { /* Battery is relaxed. */
		u32 bcap;
		int soc, ocv;

		wd->relax_time = 0;

		/* Get OCV */
		ocv = wd->vcell;

		/* Get soc at relaxed state from ocv/soc table */
		soc = bd71827_voltage_to_capacity(ocv);
		dev_err(pwr->mfd->dev,  "soc %d[0.1%%]\n", soc);
		if (soc < 0)
			soc = 0;

		bcap = wd->designed_cap * soc / 1000;

		ret = update_cc(pwr, bcap + wd->designed_cap / 200);
		if (ret)
			return ret;

		dev_err(pwr->mfd->dev,
			"Adjust Coulomb Counter by SW at Relaxed State\n");
		dev_err(pwr->mfd->dev, "CC_CCNTD = %d\n", wd->coulomb_cnt);

		/* If the following commented out code is enabled, the SOC is not clamped at the relax time. */
		/* Reset SOCs */
		/* bd71827_calc_soc_org(pwr, wd); */
		/* wd->soc_norm = wd->soc_org; */
		/* wd->soc = wd->soc_norm; */
		/* wd->clamp_soc = wd->soc; */
	}

	return 0;
}

/** @brief get coulomb counter values
 * @param pwr power device
 * @return 0
 */
static int bd71827_coulomb_count(struct bd71827_power* pwr,
				 struct bd7182x_soc_data *wd)
{
	int ret = 0;

	if (wd->state_machine == STAT_POWER_ON) {
		wd->state_machine = STAT_INITIALIZED;
		/* Start Coulomb Counter */
		ret = start_cc(pwr);
	} else if (wd->state_machine == STAT_INITIALIZED) {
		u32 cc;
		ret = read_cc(pwr, &cc);
		wd->coulomb_cnt = cc;
	}
	dev_dbg(pwr->mfd->dev, "%s(): state_machine = 0x%x,cc=%d\n", __func__,
		wd->state_machine,wd->coulomb_cnt);
	return ret;
}

/** @brief calc cycle
 * @param pwr power device
 * @return 0
 */
static int bd71827_update_cycle(struct bd71827_power* pwr,
				struct bd7182x_soc_data *wd)
{
	int tmpret, ret;
	uint16_t charged_coulomb_cnt;

	ret = bd7182x_read16_himask(pwr, pwr->regs->coulomb_chg3, 0xff,
				    &charged_coulomb_cnt);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read charging CC (%d)\n",
			ret);
		return ret;
	}

	dev_dbg(pwr->mfd->dev, "%s(): charged_coulomb_cnt = 0x%x\n", __func__,
		(int)charged_coulomb_cnt);
	if (charged_coulomb_cnt >= wd->designed_cap) {
		wd->cycle++;
		dev_dbg(pwr->mfd->dev,  "Update cycle = %d\n", wd->cycle);
		battery_cycle = wd->cycle;
		charged_coulomb_cnt -= wd->designed_cap;
		
		ret = stop_cc(pwr);
		if (ret)
			return ret;

		ret = bd7182x_write16(pwr, pwr->regs->coulomb_chg3,
				      charged_coulomb_cnt);
		if (ret) {
			dev_err(pwr->mfd->dev,
				"Failed to update charging CC (%d)\n", ret);
		}

		tmpret = start_cc(pwr);
		if (tmpret)
			return tmpret;
	}
	return ret;
}

/** @brief calc full capacity value by Cycle and Temperature
 * @param pwr power device
 * @return 0
 */
static int bd71827_calc_full_cap(struct bd71827_power* pwr,
				 struct bd7182x_soc_data *wd)
{
	u32 designed_cap_uAh;
	u32 full_cap_uAh;

	/* Calculate full capacity by cycle */
	designed_cap_uAh = A10s_mAh(wd->designed_cap, pwr->curr_factor) * 1000;

	if (dgrd_cyc_cap * wd->cycle >= designed_cap_uAh) {
		/* Battry end of life? */
		wd->full_cap = 1;
		return 0;
	}

	full_cap_uAh = designed_cap_uAh - dgrd_cyc_cap * wd->cycle;
	wd->full_cap = mAh_A10s( uAMP_TO_mAMP(full_cap_uAh), pwr->curr_factor);
	dev_dbg(pwr->mfd->dev,  "Calculate full capacity by cycle\n");
	dev_dbg(pwr->mfd->dev,  "%s() pwr->full_cap = %d\n", __func__,
		wd->full_cap);

	/* Calculate full capacity by temperature */
	dev_dbg(pwr->mfd->dev,  "Temperature = %d\n", wd->temp);
	if (wd->temp >= DGRD_TEMP_M_DEFAULT) {
		full_cap_uAh += (wd->temp - DGRD_TEMP_M_DEFAULT) *
				dgrd_temp_cap_h;
		wd->full_cap = mAh_A10s(uAMP_TO_mAMP(full_cap_uAh), pwr->curr_factor);
	}
	else if (wd->temp >= DGRD_TEMP_L_DEFAULT) {
		full_cap_uAh += (wd->temp - DGRD_TEMP_M_DEFAULT) *
				dgrd_temp_cap_m;
		wd->full_cap = mAh_A10s(uAMP_TO_mAMP(full_cap_uAh), pwr->curr_factor);
	}
	else {
		full_cap_uAh += (DGRD_TEMP_L_DEFAULT - DGRD_TEMP_M_DEFAULT) *
				dgrd_temp_cap_m;
		full_cap_uAh += (wd->temp - DGRD_TEMP_L_DEFAULT) *
				dgrd_temp_cap_l;
		wd->full_cap = mAh_A10s(uAMP_TO_mAMP(full_cap_uAh), pwr->curr_factor);
	}
	
	if (wd->full_cap < 1)
		wd->full_cap = 1;
	
	dev_dbg(pwr->mfd->dev,  "Calculate full capacity by cycle and temperature\n");
	dev_dbg(pwr->mfd->dev,  "%s() pwr->full_cap = %d\n", __func__,
		wd->full_cap);

	return 0;
}

/** @brief calculate SOC values by designed capacity
 * @param pwr power device
 * @return 0
 */

u32 bd71827_calc_soc_org(u32 cc, int designed_cap)
{
	return ( cc >> 16) * 100 / designed_cap;
}

/** @brief calculate SOC values by full capacity
 * @param pwr power device
 * @return 0
 */
static int bd71827_calc_soc_norm(struct bd71827_power* pwr,
				 struct bd7182x_soc_data *wd)
{
	int lost_cap;
	int mod_coulomb_cnt;

	lost_cap = wd->designed_cap - wd->full_cap;
	dev_dbg(pwr->mfd->dev,  "%s() lost_cap = %d\n", __func__, lost_cap);

	mod_coulomb_cnt = (wd->coulomb_cnt >> 16) - lost_cap;
	if ((mod_coulomb_cnt > 0) && (wd->full_cap > 0))
		wd->soc_norm = mod_coulomb_cnt * 100 / wd->full_cap;
	else
		wd->soc_norm = 0;

	if (wd->soc_norm > 100)
		wd->soc_norm = 100;

	dev_dbg(pwr->mfd->dev,  "%s() pwr->soc_norm = %d\n", __func__,
		wd->soc_norm);

	return 0;
}

/** @brief get OCV value by SOC
 * @param pwr power device
 * @return 0
 */
int bd71827_get_ocv(struct bd71827_power* pwr, int dsoc)
{
	int i = 0;
	int ocv = 0;

	if (dsoc > soc_table[0]) {
		ocv = MAX_VOLTAGE_DEFAULT;
	}
	else if (dsoc == 0) {
			ocv = ocv_table[21];
	}
	else {
		i = 0;
		while (i < 22) {
			if ((dsoc <= soc_table[i]) && (dsoc > soc_table[i+1])) {
				ocv = (ocv_table[i] - ocv_table[i+1]) *
				      (dsoc - soc_table[i+1]) / (soc_table[i] -
				      soc_table[i+1]) + ocv_table[i+1];
				break;
			}
			i++;
		}
		if (i == 22)
			ocv = ocv_table[22];
	}
	dev_dbg(pwr->mfd->dev,  "%s() ocv = %d\n", __func__, ocv);
	return ocv;
}

static void calc_vdr(int *res, int *vdr, int temp, int dgrd_temp,
		     int *vdr_hi, int dgrd_temp_hi, int items)
{
	int i;

	for (i = 0; i < items; i++)
		res[i] = vdr[i] + (temp - dgrd_temp) * (vdr_hi[i] - vdr[i]) /
			 (dgrd_temp_hi - dgrd_temp);
}

/** @brief get VDR(Voltage Drop Rate) value by SOC
 * @param pwr power device
 * @return 0
 */
static int bd71827_get_vdr(struct bd71827_power* pwr, int dsoc,
			   struct bd7182x_soc_data *wd)
{
	int i = 0;
	int vdr = 100;
	int vdr_table[23];
	
	/* Calculate VDR by temperature */
	if (wd->temp >= DGRD_TEMP_H_DEFAULT)
		for (i = 0; i < 23; i++)
			vdr_table[i] = vdr_table_h[i];
	else if (wd->temp >= DGRD_TEMP_M_DEFAULT)
		calc_vdr(vdr_table, vdr_table_m, wd->temp, DGRD_TEMP_M_DEFAULT,
			 vdr_table_h, DGRD_TEMP_H_DEFAULT, 23);
	else if (wd->temp >= DGRD_TEMP_L_DEFAULT)
		calc_vdr(vdr_table, vdr_table_l, wd->temp, DGRD_TEMP_L_DEFAULT,
			 vdr_table_m, DGRD_TEMP_M_DEFAULT, 23);
	else if (wd->temp >= DGRD_TEMP_VL_DEFAULT)
		calc_vdr(vdr_table, vdr_table_vl, wd->temp,
			 DGRD_TEMP_VL_DEFAULT, vdr_table_l, DGRD_TEMP_L_DEFAULT,
			 23);
	else
		for (i = 0; i < 23; i++)
			vdr_table[i] = vdr_table_vl[i];

	if (dsoc > soc_table[0]) {
		vdr = 100;
	}
	else if (dsoc == 0) {
		vdr = vdr_table[21];
	}
	else {
		for (i = 0; i < 22; i++)
			if ((dsoc <= soc_table[i]) && (dsoc > soc_table[i+1])) {
				vdr = (vdr_table[i] - vdr_table[i+1]) *
				      (dsoc - soc_table[i+1]) /
				      (soc_table[i] - soc_table[i+1]) +
				      vdr_table[i+1];
				break;
			}
		if (i == 22)
			vdr = vdr_table[22];
	}
	dev_dbg(pwr->mfd->dev, "%s() vdr = %d\n", __func__, vdr);
	return vdr;
}

/** @brief calculate SOC value by full_capacity and load
 * @param pwr power device
 * @return OCV
 */

static void soc_not_charging(struct bd71827_power* pwr,
			    struct bd7182x_soc_data *wd)
{
	int ocv_table_load[23];
	int i;
	int ocv;
	int lost_cap;
	int mod_coulomb_cnt;
	int dsoc;

	lost_cap = wd->designed_cap - wd->full_cap;
	mod_coulomb_cnt = (wd->coulomb_cnt >> 16) - lost_cap;
	dsoc = mod_coulomb_cnt * 1000 /  wd->full_cap;
	dev_dbg(pwr->mfd->dev,  "%s() dsoc = %d\n", __func__,
		dsoc);

	ocv = bd71827_get_ocv(pwr, dsoc);
	for (i = 1; i < 23; i++) {
		ocv_table_load[i] = ocv_table[i] - (ocv - wd->vcell);
		if (ocv_table_load[i] <= MIN_VOLTAGE_DEFAULT) {
			dev_dbg(pwr->mfd->dev,
				"%s() ocv_table_load[%d] = %d\n", __func__,
				i, ocv_table_load[i]);
			break;
		}
	}
	if (i < 23) {
		int j, k, m;
		int dv;
		int lost_cap2, new_lost_cap2;
		int mod_coulomb_cnt2, mod_full_cap;
		int dsoc0;
		int vdr, vdr0;
		dv = (ocv_table_load[i-1] - ocv_table_load[i]) / 5;
		for (j = 1; j < 5; j++){
			if ((ocv_table_load[i] + dv * j) >
			    MIN_VOLTAGE_DEFAULT) {
				break;
			}
		}
		lost_cap2 = ((21 - i) * 5 + (j - 1)) * wd->full_cap / 100;
		dev_dbg(pwr->mfd->dev, "%s() lost_cap2-1 = %d\n", __func__,
			lost_cap2) ;
		for (m = 0; m < soc_est_max_num; m++) {
			new_lost_cap2 = lost_cap2;
			dsoc0 = lost_cap2 * 1000 / wd->full_cap;
			if ((dsoc >= 0 && dsoc0 > dsoc) ||
			    (dsoc < 0 && dsoc0 < dsoc))
				dsoc0 = dsoc;

			dev_dbg(pwr->mfd->dev, "%s() dsoc0(%d) = %d\n",
				__func__, m, dsoc0);

			vdr = bd71827_get_vdr(pwr, dsoc, wd);
			vdr0 = bd71827_get_vdr(pwr, dsoc0, wd);

			for (k = 1; k < 23; k++) {
				ocv_table_load[k] = ocv_table[k] -
						    (ocv - wd->vcell) * vdr0
						    / vdr;
				if (ocv_table_load[k] <= MIN_VOLTAGE_DEFAULT) {
					dev_dbg(pwr->mfd->dev,
						"%s() ocv_table_load[%d] = %d\n",
						__func__, k, ocv_table_load[k]);
					break;
				}
			}
			if (k < 23) {
				dv = (ocv_table_load[k-1] -
				     ocv_table_load[k]) / 5;
				for (j = 1; j < 5; j++)
					if ((ocv_table_load[k] + dv * j) >
					     MIN_VOLTAGE_DEFAULT)
						break;

				new_lost_cap2 = ((21 - k) * 5 + (j - 1)) *
						wd->full_cap / 100;
				if (soc_est_max_num == 1)
					lost_cap2 = new_lost_cap2;
				else 
					lost_cap2 += (new_lost_cap2 - lost_cap2) /
						     (2 * (soc_est_max_num - m));
				dev_dbg(pwr->mfd->dev,
					"%s() lost_cap2-2(%d) = %d\n", __func__,
					m, lost_cap2);
			}
			if (new_lost_cap2 == lost_cap2)
				break;
		}
		mod_coulomb_cnt2 = mod_coulomb_cnt - lost_cap2;
		mod_full_cap = wd->full_cap - lost_cap2;
		if ((mod_coulomb_cnt2 > 0) && (mod_full_cap > 0))
			wd->soc = mod_coulomb_cnt2 * 100 / mod_full_cap;
		else
			wd->soc = 0;

		dev_dbg(pwr->mfd->dev,  "%s() pwr->soc(by load) = %d\n",
			__func__, wd->soc);
	}
}


static int bd71827_calc_soc(struct bd71827_power* pwr,
			    struct bd7182x_soc_data *wd)
{
	wd->soc = wd->soc_norm;

	 /* Adjust for 0% between thr_voltage and min_voltage */
	switch (wd->rpt_status) {
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (wd->vcell <= THR_VOLTAGE_DEFAULT)
			soc_not_charging(pwr, wd);
		else
			wd->soc = 100;
		break;
	default:
		break;
	}

	switch (wd->rpt_status) {/* Adjust for 0% and 100% */
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (wd->vcell <= MIN_VOLTAGE_DEFAULT)
			wd->soc = 0;
		else if (wd->soc == 0)
			wd->soc = 1;
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if (wd->soc == 100)
			wd->soc = 99;
		break;
	default:
		break;
	}
	dev_dbg(pwr->mfd->dev,  "%s() pwr->soc = %d\n", __func__, wd->soc);
	return 0;
}

/** @brief calculate Clamped SOC value by full_capacity and load
 * @param pwr power device
 * @return OCV
 */
static int bd71827_calc_soc_clamp(struct bd71827_power* pwr,
				  struct bd7182x_soc_data *wd)
{
	switch (wd->rpt_status) {/* Adjust for 0% and 100% */
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if((wd->soc <= wd->clamp_soc && !ciritical_low_count) || ((wd->vcell < through_voltage) && ciritical_low_count))
			wd->clamp_soc = wd->soc;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		wd->clamp_soc = 100;
		break;
	default:
		wd->clamp_soc = wd->soc;
		break;
	}
	dev_dbg(pwr->mfd->dev,  "%s() pwr->clamp_soc = %d\n", __func__,
		wd->clamp_soc);
	return 0;
}

/** @brief get battery and DC online status
 * @param pwr power device
 * @return 0
 */
static int bd71827_get_online(struct bd71827_power* pwr,
			      struct bd7182x_soc_data *wd)
{
	int r, ret;

#if 0
#define TS_THRESHOLD_VOLT	0xD9
	r = bd71827_reg_read(pwr->mfd, BD71827_REG_VM_VTH);
	pwr->bat_online = (r > TS_THRESHOLD_VOLT);
#endif
#if 0
	r = bd71827_reg_read(pwr->mfd, BD71827_REG_BAT_STAT);
	if (r >= 0 && (r & BAT_DET_DONE)) {
		pwr->bat_online = (r & BAT_DET) != 0;
	}
#endif
#if 1
#define BAT_OPEN	0x7
	ret = regmap_read(pwr->mfd->regmap, pwr->regs->bat_temp, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read battery temperature\n");
		return ret;
	}
	wd->bat_online = ((r & BD7182x_MASK_BAT_TEMP) != BAT_OPEN);
#endif	
	ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read DCIN status\n");
		return ret;
	}
	wd->charger_online = ((r & BD7182x_MASK_DCIN_DET) != 0);

	dev_dbg(pwr->mfd->dev,
		"%s(): pwr->bat_online = %d, pwr->charger_online = %d\n",
		__func__, wd->bat_online, wd->charger_online);

	return 0;
}

static int EnableDCINCharger(struct bd71827_power *pwr)
{
	int val, ret;
	uint16_t tmp_vcell;

	ret = regmap_read(pwr->mfd->regmap, BD71879_REG_BAT_STAT, &val);
	if(ret)
		return ret;
	if((val & (BAT_DET_DONE|BAT_DET))==0x30) {
		ret = bd7182x_read16_himask(pwr, pwr->regs->vbat_min_avg, BD7182x_MASK_VBAT_U, &tmp_vcell);
		if((tmp_vcell*1000) < MIN_VOLTAGE_DEFAULT) {
			pr_info("skip EnableDCINCharger\n");
			return 0;
		}
		ret = regmap_read(pwr->mfd->regmap, BD71879_REG_DCIN_SET, &val);
		if(ret)
			return ret;
		val |= 0x80;
		ret = regmap_write(pwr->mfd->regmap, BD71879_REG_DCIN_SET, val);

		return ret;
	}
	else {
		pr_info("without battery\n");
		return 0;
	}
}

static int DisableDCINCharger(struct bd71827_power *pwr)
{
	int val, ret;
	uint16_t tmp_vcell;

	ret = regmap_read(pwr->mfd->regmap, BD71879_REG_BAT_STAT, &val);
	if(ret)
		return ret;
	if((val & (BAT_DET_DONE|BAT_DET))==0x30) {
		ret = bd7182x_read16_himask(pwr, pwr->regs->vbat_min_avg, BD7182x_MASK_VBAT_U, &tmp_vcell);
		if((tmp_vcell*1000) < MIN_VOLTAGE_DEFAULT) {
			pr_info("skip DisableDCINCharger\n");
			return 0;
		}
		ret = regmap_read(pwr->mfd->regmap, BD71879_REG_DCIN_SET, &val);
		if(ret)
			return ret;
		val &= 0x7F;
		ret = regmap_write(pwr->mfd->regmap, BD71879_REG_DCIN_SET, val);

		return ret;
	}
	else {
		pr_info("without battery\n");
		return 0;
	}
}

int reset_for_manual_Calibration(struct bd71827_power *pwr,
				 struct bd7182x_soc_data *wd)
{
	int ret = 0;

	/* Stop Coulomb Counter */
	ret = stop_cc(pwr);
	if (ret)
		return ret;

	/* Set Coulomb Counter Reset bit*/
	ret = regmap_update_bits(pwr->mfd->regmap,
			pwr->regs->coulomb_ctrl,
			BD7182x_MASK_CCNTRST,
			BD7182x_MASK_CCNTRST);
	if (ret)
		return ret;

	/* Clear Coulomb Counter Reset bit*/
	ret = regmap_update_bits(pwr->mfd->regmap,
			pwr->regs->coulomb_ctrl,
			BD7182x_MASK_CCNTRST, 0);
	if (ret)
		return ret;

	/* Clear Relaxed Coulomb Counter */
	ret = regmap_update_bits(pwr->mfd->regmap,
			pwr->regs->rex_clear_reg,
			pwr->regs->rex_clear_mask,
			pwr->regs->rex_clear_mask);

	/* Set initial Coulomb Counter by HW OCV */
	calibration_coulomb_counter(pwr, wd, 0);

	return 0;
}

/** @brief init bd71827 sub module charger
 * @param pwr power device
 * @return 0
 */
static int bd71827_init_hardware(struct bd71827_power *pwr,
				 struct bd7182x_soc_data *wd)
{
	int r, temp, ret;
	uint16_t tmp;
	u32 cc;
	int sorg, soc_cal, status;

	ret = regmap_write(pwr->mfd->regmap, pwr->regs->dcin_collapse_limit,
			   BD7182x_DCIN_COLLAPSE_DEFAULT);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to write DCIN collapse limit\n");
		return ret;
	}

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->conf, &r);
	if (ret) {
		dev_err(pwr->mfd->dev, "Failed to read CONF register\n");
		return ret;
	}

	/* Always set default Battery Capacity ? */
	wd->designed_cap = battery_cap;
	wd->full_cap = battery_cap;
	/* Why BD71827_REG_CC_BATCAP_U is not used? */

	if (r & BD7182x_MASK_CONF_PON) {
		/* Init HW, when the battery is inserted. */
		dev_err(pwr->mfd->dev, "Power-on-reset on\n");

		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->conf,
					 BD7182x_MASK_CONF_PON, 0);
		if (ret) {
			dev_err(pwr->mfd->dev, "Failed to clear CONF register\n");
			return ret;
		}

		/* Stop Coulomb Counter */
		ret = stop_cc(pwr);
		if (ret)
			return ret;

		/* Set Coulomb Counter Reset bit*/
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->coulomb_ctrl,
					 BD7182x_MASK_CCNTRST,
					 BD7182x_MASK_CCNTRST);
		if (ret)
			return ret;

		/* Clear Coulomb Counter Reset bit*/
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->coulomb_ctrl,
					 BD7182x_MASK_CCNTRST, 0);
		if (ret)
			return ret;

		/* Clear Relaxed Coulomb Counter */
		ret = regmap_update_bits(pwr->mfd->regmap,
					 pwr->regs->rex_clear_reg,
					 pwr->regs->rex_clear_mask,
					 pwr->regs->rex_clear_mask);


		/* Set initial Coulomb Counter by HW OCV */
		calibration_coulomb_counter(pwr, wd, 1);

		/* WDT_FST auto set */
		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_set1,
					 BD7182x_MASK_WDT_AUTO,
					 BD7182x_MASK_WDT_AUTO);
		if (ret)
			return ret;

		// Critical Low
		/*if( 0x0a==gptHWCFG->m_val.bBattery) {
			// E60U20
			ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,	0x00CC);		// 0.016 * 204(0xcc) = 3.264
		}
		else if(0x0e==gptHWCFG->m_val.bBattery) {
			if( 101 == gptHWCFG->m_val.bPCB ) // E70K10
				ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,	0x00DC);	// 0.016 * 220(0xdc) = 3.52
			else if( 103 == gptHWCFG->m_val.bPCB ) // E60K20 
				ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,	0x00de);	// 0.016 * 222(0xde) = 3.552
		}
		else {
			ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,	VBAT_LOW_TH);
		}

		if (ret)
			return ret;*/

		ret = bd7182x_write16(pwr, pwr->regs->batcap_mon_limit_u,
				      battery_cap * 9 / 10);
		if (ret)
			return ret;

		/* Set Battery Capacity Monitor threshold1 as 90% */
		dev_dbg(pwr->mfd->dev, "BD71827_REG_CC_BATCAP1_TH = %d\n",
			(battery_cap * 9 / 10));

		/* Enable LED ON when charging 
 		   Should we do this decision here? Should the enabling be
		   in LED driver and come from DT?
		bd71827_set_bits(pwr->mfd, BD71827_REG_LED_CTRL, CHGDONE_LED_EN);
		*/
		wd->state_machine = STAT_POWER_ON;
	} else {
		dev_err(pwr->mfd->dev, "Power-on-reset off\n");
		wd->state_machine = STAT_INITIALIZED;	// STAT_INITIALIZED
	}

	// Critical Low
	if(0x10==gptHWCFG->m_val.bBattery) {
		// E60U20
		ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,	0x00CC);		// 0.016 * 204(0xcc) = 3.264
	}
	else if(0x0e==gptHWCFG->m_val.bBattery) {
		if( 101 == gptHWCFG->m_val.bPCB ) // E70K10
			ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,	0x00D4);	// 0.016 * 212(0xd4) = 3.392
		else if( 103 == gptHWCFG->m_val.bPCB ) // E60K20 
			ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,	0x00de);	// 0.016 * 222(0xde) = 3.552
	}
	else {
		ret = bd7182x_write16(pwr, pwr->regs->vbat_alm_limit_u,	VBAT_LOW_TH);
	}

	if (ret)
		return ret;

	bd7182x_read16_himask(pwr, pwr->regs->vbat_alm_limit_u , 0x01, &tmp);
	through_voltage = tmp*16000;
	dev_err(pwr->mfd->dev,  "%s() battery through_voltage = %d\n", __func__, through_voltage);

	ret = pwr->get_temp(pwr, &temp);
	if (ret)
		return ret;

	wd->temp = temp;
	dev_err(pwr->mfd->dev,  "Temperature = %d\n", wd->temp);
	bd71827_adjust_coulomb_count(pwr, wd, 1);
	bd71827_reset_coulomb_count(pwr, wd);
	ret = read_cc(pwr, &cc);
	if (ret)
		return ret;

	wd->coulomb_cnt = cc;
	dev_err(pwr->mfd->dev,  "%s() coulomb_cnt = %d\n", __func__, cc);
	/* If we boot up with CC stopped and both REX and FULL CC being 0
	 * - then the bd71827_adjust_coulomb_count and
	 * bd71827_reset_coulomb_count wont start CC. Just start CC here for
	 * now to mimic old operation where bd71827_calc_soc_org did
	 * always stop and start cc.
	 */
	start_cc(pwr);
	sorg = bd71827_calc_soc_org(wd->coulomb_cnt, wd->designed_cap);
	if (sorg > 100)
		limit_cc(pwr, wd, &sorg);
	if(wd->state_machine == STAT_INITIALIZED) {
		DisableDCINCharger(pwr);
		mdelay(500);
		bd71827_get_init_bat_stat(pwr, &temp, 0);
		soc_cal = bd71827_voltage_to_capacity(temp)/10;
		wd->vcell = temp;
		wd->soc_norm = sorg;
		bd71827_charge_status(pwr, wd, 0);
		bd71827_calc_soc(pwr, wd);
		regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &status);
		dev_err(pwr->mfd->dev,  "%s() sorg = %d, soc_cal = %d, voltage %d, soc %d, charge %d, dc %d\n", __func__, sorg, soc_cal, temp, wd->soc, wd->rpt_status, status&BD7182x_MASK_DCIN_STAT);
		if(((sorg+5) < soc_cal) && (sorg > 0)) {
			dev_err(pwr->mfd->dev,  "%s() reset_for_manual_Calibration\n", __func__);
			ret = reset_for_manual_Calibration(pwr, wd);
			if (ret)
				return ret;
			ret = read_cc(pwr, &cc);
			if (ret)
				return ret;

			wd->coulomb_cnt = cc;
			start_cc(pwr);
			sorg = bd71827_calc_soc_org(wd->coulomb_cnt, wd->designed_cap);
			if (sorg > 100)
				limit_cc(pwr, wd, &sorg);
		}
		else {
			if(((soc_cal<=0) && (sorg > wd->soc)) || (sorg < 10)) {
				// low battery, let capacity used the cal by soc_not_charging, not used the from coulomb counter.
				dev_err(pwr->mfd->dev,  "%s() low battery reset_for_manual_Calibration\n", __func__);
				ret = reset_for_manual_Calibration(pwr, wd);
				if (ret)
					return ret;
				ret = read_cc(pwr, &cc);
				if (ret)
					return ret;

				wd->coulomb_cnt = cc;
				start_cc(pwr);
				sorg = bd71827_calc_soc_org(wd->coulomb_cnt, wd->designed_cap);
				if (sorg > 100)
					limit_cc(pwr, wd, &sorg);
			}
		}
		EnableDCINCharger(pwr);
	}

	wd->soc_norm = sorg;
	wd->soc = wd->soc_norm;
	wd->clamp_soc = wd->soc;
	dev_err(pwr->mfd->dev,  "%s() CC_CCNTD = %d\n", __func__, wd->coulomb_cnt);
	dev_err(pwr->mfd->dev,  "%s() pwr->soc = %d\n", __func__, wd->soc);
	dev_err(pwr->mfd->dev,  "%s() pwr->clamp_soc = %d\n", __func__, wd->clamp_soc);

	wd->cycle = battery_cycle;
	wd->curr = 0;
	wd->relax_time = 0;

	update_soc_data(pwr);
	return 0;
}

/** @brief set bd71827 battery parameters
 * @param pwr power device
 * @return 0
 */
static int bd71827_set_battery_parameters(struct bd71827_power *pwr)
{
	//struct bd71827 *mfd = pwr->mfd;
	int i;

	if (use_load_bat_params == 0) {
		printk(KERN_INFO "bd71827_set_battery_parameters: Battery %d \n", gptHWCFG->m_val.bBattery);
		switch(gptHWCFG->m_val.bBattery) {
			case 14:	//PR-284983N-1500mA
				MAX_VOLTAGE_DEFAULT = MAX_VOLTAGE_DEFAULT_28;
				if(0x67 == gptHWCFG->m_val.bPCB ) // E60K24
					MIN_VOLTAGE_DEFAULT = 3600000;	// 0 % , 3.6 V 
				else
					MIN_VOLTAGE_DEFAULT = MIN_VOLTAGE_DEFAULT_28_PR284983N;
				BATTERY_CAP_MAH_DEFAULT = BATTERY_CAP_MAH_DEFAULT_28_PR284983N;
				DGRD_CYC_CAP_DEFAULT = DGRD_CYC_CAP_DEFAULT_28_PR284983N;
				SOC_EST_MAX_NUM_DEFAULT = SOC_EST_MAX_NUM_DEFAULT_28_PR284983N;
				DGRD_TEMP_CAP_H_DEFAULT = DGRD_TEMP_CAP_H_DEFAULT_28_PR284983N;
				DGRD_TEMP_CAP_M_DEFAULT = DGRD_TEMP_CAP_M_DEFAULT_28_PR284983N;
				DGRD_TEMP_CAP_L_DEFAULT = DGRD_TEMP_CAP_L_DEFAULT_28_PR284983N;
				ocv_table_default = ocv_table_28_PR284983N;
				vdr_table_h_default = vdr_table_h_28_PR284983N;
				vdr_table_m_default = vdr_table_m_28_PR284983N;
				vdr_table_l_default = vdr_table_l_28_PR284983N;
				vdr_table_vl_default = vdr_table_vl_28_PR284983N;
				if(0x67 == gptHWCFG->m_val.bPCB ) {
					BATTERY_CAP_MAH_DEFAULT = 1407;
					ocv_table_default = ocv_table_28_PR284983N_3p60;
					vdr_table_h_default = vdr_table_h_28_PR284983N_3p60;
					vdr_table_m_default = vdr_table_m_28_PR284983N_3p60;
					vdr_table_l_default = vdr_table_l_28_PR284983N_3p60;
					vdr_table_vl_default = vdr_table_vl_28_PR284983N_3p60;
				}
				break;
			case 16:	//EVE254385N-1000mA
				MAX_VOLTAGE_DEFAULT = MAX_VOLTAGE_DEFAULT_28;
				MIN_VOLTAGE_DEFAULT = MIN_VOLTAGE_DEFAULT_28_EVE254385N;
				BATTERY_CAP_MAH_DEFAULT = BATTERY_CAP_MAH_DEFAULT_28_EVE254385N;
				DGRD_CYC_CAP_DEFAULT = DGRD_CYC_CAP_DEFAULT_28_EVE254385N;
				SOC_EST_MAX_NUM_DEFAULT = SOC_EST_MAX_NUM_DEFAULT_28_EVE254385N;
				DGRD_TEMP_CAP_H_DEFAULT = DGRD_TEMP_CAP_H_DEFAULT_28_EVE254385N;
				DGRD_TEMP_CAP_M_DEFAULT = DGRD_TEMP_CAP_M_DEFAULT_28_EVE254385N;
				DGRD_TEMP_CAP_L_DEFAULT = DGRD_TEMP_CAP_L_DEFAULT_28_EVE254385N;
				ocv_table_default = ocv_table_28_EVE254385N;
				vdr_table_h_default = vdr_table_h_28_EVE254385N;
				vdr_table_m_default = vdr_table_m_28_EVE254385N;
				vdr_table_l_default = vdr_table_l_28_EVE254385N;
				vdr_table_vl_default = vdr_table_vl_28_EVE254385N;
				break;
			default:
				printk(KERN_INFO "bd71827_set_battery_parameters: Battery parameter not match \n");
				break;
		}
		battery_cap_mah = BATTERY_CAP_MAH_DEFAULT;
		dgrd_cyc_cap = DGRD_CYC_CAP_DEFAULT;
		soc_est_max_num = SOC_EST_MAX_NUM_DEFAULT;
		dgrd_temp_cap_h = DGRD_TEMP_CAP_H_DEFAULT;
		dgrd_temp_cap_m = DGRD_TEMP_CAP_M_DEFAULT;
		dgrd_temp_cap_l = DGRD_TEMP_CAP_L_DEFAULT;
		for (i = 0; i < 23; i++) {
			ocv_table[i] = ocv_table_default[i];
			soc_table[i] = soc_table_default[i];
			vdr_table_h[i] = vdr_table_h_default[i];
			vdr_table_m[i] = vdr_table_m_default[i];
			vdr_table_l[i] = vdr_table_l_default[i];
			vdr_table_vl[i] = vdr_table_vl_default[i];
		}

		printk("PMU : %s : new table\n",__func__);
		for (i = 0; i <= 10; i = i+1) {
			printk("PMU : %s : ocv_table %d is %d mv\n",__func__, i, ocv_table_default[i]/1000);
		}

	}
	for (i = 0; i < 23; i++) {
		soc_table[i] = soc_table_default[i];
	}
	battery_cap = mAh_A10s(battery_cap_mah, pwr->curr_factor);
	smp_wmb();

	return 0;
}

static void update_soc_data(struct bd71827_power *pwr)
{
	spin_lock(&pwr->dlock);
	pwr->d_r = pwr->d_w;
	spin_unlock(&pwr->dlock);
}

static int setCurrentLimit(struct bd71827_power *pwr, unsigned int currentLimit)
{
    int val, ret;

    if(currentLimit < 50)
    {
        currentLimit = 50;
    }
	dev_info(pwr->mfd->dev, "%s() set current limit %d mA\n", __func__, currentLimit);
 
    val = currentLimit/50 - 1;

    /* set current limit*/
	val |= 0xC0;
	ret = regmap_write(pwr->mfd->regmap, BD71879_REG_DCIN_SET, val);

    return ret;
}

static int setChargeCurrent(struct bd71827_power *pwr, unsigned int chargeCurrent)
{
    int val, ret;
	
    if(chargeCurrent < 100)
    {
        chargeCurrent = 100;
    }
    dev_info(pwr->mfd->dev, "%s() set charge current %d mA\n", __func__, chargeCurrent);

    val = chargeCurrent/25;

    /* set charge current*/
	val |= 0x40;
	ret = regmap_write(pwr->mfd->regmap, BD71879_REG_CHG_IFST, val);
    
    return ret;
}

static int resetBatteryCharger(struct bd71827_power *pwr)
{
    int ret, val;
	
    val = 0x02;
	ret = regmap_write(pwr->mfd->regmap, BD71879_REG_CHG_INIT, val);
	mdelay(100);
	val = 0x00;
	ret += regmap_write(pwr->mfd->regmap, BD71879_REG_CHG_INIT, val);

    return ret;
}

static int setChargerParam(struct bd71827_power *pwr, unsigned currentLimit, unsigned chargeCurrent)
{
    /* set current limit*/
    setCurrentLimit(pwr, currentLimit);

    /* set charge current*/
    setChargeCurrent(pwr, chargeCurrent);
    
    /* Battery charger reset */
    resetBatteryCharger(pwr);
	
    return 0;
}

int changeToNoneMode(void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;
	struct regmap *regmap = pwr->mfd->regmap;
	int result, value=0;

	dev_info(pwr->mfd->dev, "changeToNoneMode\n");
	
    setChargerParam(pwr, 0, 0);
	
    power_supply_changed(pwr->ac);
	power_supply_changed(pwr->bat);
    
    return 0;
}

int changeToSDPMode(void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;
	struct regmap *regmap = pwr->mfd->regmap;
	int result, value=0;

	dev_info(pwr->mfd->dev, "changeToSDPMode\n");
    
    setChargerParam(pwr, 500, 500);
	
	power_supply_changed(pwr->ac);
	power_supply_changed(pwr->bat);

    return 0;
}

int changeToDCPMode(void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;
	struct regmap *regmap = pwr->mfd->regmap;
	int result, value=0;

	dev_info(pwr->mfd->dev, "changeToDCPMode\n");
	
    if(gptHWCFG->m_val.bPCB==101)
    {
        setChargerParam(pwr, 2000, 1300);
    }
	else if(gptHWCFG->m_val.bPCB==103)
	{
		setChargerParam(pwr, 2000, 1300);
	}
	else
    {
        setChargerParam(pwr, 2000, 1000);
    }
	
    power_supply_changed(pwr->ac);
	power_supply_changed(pwr->bat);

	return 0;
}

extern int check_usb_state_by_ep(unsigned long dwTimeout_ms);
/**@brief timed work function called by system
 *  read battery capacity,
 *  sense change of charge status, etc.
 * @param work work struct
 * @return  void
 */

extern void usb_plug_handler(void *dummy);

static void charger_detect_work_func(struct work_struct *work)
{
	struct bd71827_power *pwr;
	struct delayed_work *delayed_work;
	int ret;
	int i_IsDCIN;
	int status;

	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd71827_power, charger_detect_work);

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &status);
	if (ret) {
		dev_err(pwr->mfd->dev,"%s(): error (%d) on reading dcin status \n",__func__,ret);
		goto err_out;
	}

	i_IsDCIN = (status&BD7182x_MASK_DCIN_STAT)?1:0;
	dev_info(pwr->mfd->dev, "%s(): charger_type=%d,dcin=%d\n",
			__func__,pwr->charger_type,i_IsDCIN);

	if(i_IsDCIN) {
		int iEP_state = check_usb_state_by_ep(1000);
		if(iEP_state>0) {
			pwr->charger_type = BD_71879_CHARGER_TYPE_SDP; // SDP .
			changeToSDPMode(pwr);
		}
		else if(0==iEP_state){
			pwr->charger_type = BD_71879_CHARGER_TYPE_DCP; // DCP.
			changeToDCPMode(pwr);
		}
		else {
			// unkown .
			pwr->charger_type = BD_71879_CHARGER_TYPE_SDP; // SDP .
			changeToSDPMode(pwr);
		}

		ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &status);
		if (ret) {
			dev_err(pwr->mfd->dev,"%s(): error (%d) on reading dcin status \n",__func__,ret);
			goto err_out;
		}

		if(status&BD7182x_MASK_DCIN_STAT) {
            //do notihng
		}
		else {
			pwr->charger_type = BD_71879_CHARGER_TYPE_NONE;
			changeToNoneMode(pwr);
			dev_err(pwr->mfd->dev,"%s(): charger removed while detecting !\n",__func__);
		}

	}
	else {
		pwr->charger_type = BD_71879_CHARGER_TYPE_NONE;
		changeToNoneMode(pwr);
	}

	dev_info(pwr->mfd->dev, "%s(): charger_type=%d\n",
			__func__,pwr->charger_type);

	usb_plug_handler(NULL);	

	return;

err_out:
	return ;
}

static void bd_work_callback(struct work_struct *work)
{
	struct bd71827_power *pwr;
	struct delayed_work *delayed_work;
	int status, changed = 0, ret;
	u32 sorg;
	static int cap_counter = 0;
	const char *errstr = "DCIN status reading failed";
	struct bd7182x_soc_data *wd;

	delayed_work = container_of(work, struct delayed_work, work);
	pwr = container_of(delayed_work, struct bd71827_power, bd_work);
	wd = &pwr->d_w;

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->dcin_stat, &status);
	if (ret)
		goto err_out;

	status &= BD7182x_MASK_DCIN_STAT;
	if (status != wd->vbus_status) {
			dev_err(pwr->mfd->dev, "DCIN_STAT CHANGED from 0x%X to 0x%X\n",
			wd->vbus_status, status);
		if((wd->clamp_soc < wd->soc_norm) && (wd->vbus_status==0) && (status==1))
			sync_cc(pwr, wd, wd->clamp_soc);
		wd->vbus_status = status;
		changed = 1;
	}

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->bat_stat, &status);

	errstr = "battery status reading failed";
	if (ret) 
		goto err_out;

	status &= BD7182x_MASK_BAT_STAT;
	status &= ~BAT_DET_DONE;
	if (status != wd->bat_status) {
		dev_dbg(pwr->mfd->dev, "BAT_STAT CHANGED from 0x%X to 0x%X\n",
			wd->bat_status, status);
		wd->bat_status = status;
		changed = 1;
	}

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_state, &status);
	errstr = "Charger state reading failed";
	if (ret)
		goto err_out;

	status &= BD7182x_MASK_CHG_STATE;

	if (status != wd->charge_status) {
		dev_dbg(pwr->mfd->dev, "CHG_STATE CHANGED from 0x%X to 0x%X\n",
			wd->charge_status, status);
		wd->charge_status = status;
	}
	ret = bd71827_get_voltage_current(pwr, wd);
	errstr = "Failed to get current voltage";
	if (ret) 
		goto err_out;

	errstr = "Failed to adjust coulomb count";
	ret = bd71827_adjust_coulomb_count(pwr, wd, 0);
	if (ret) 
		goto err_out;
	
	errstr = "Failed to reset coulomb count";
	ret = bd71827_reset_coulomb_count(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to adjust coulomb count (sw)";
	ret = bd71827_adjust_coulomb_count_sw(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to get coulomb count";
	ret = bd71827_coulomb_count(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to perform update cycle";
	ret = bd71827_update_cycle(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to calculate full capacity";
	ret = bd71827_calc_full_cap(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to calculate org state of charge";
	sorg = bd71827_calc_soc_org(wd->coulomb_cnt, wd->designed_cap);
	if (sorg > 100)
		ret = limit_cc(pwr, wd, &sorg);
	if (ret)
		goto err_out;

	errstr = "Failed to calculate norm state of charge";
	ret = bd71827_calc_soc_norm(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to calculate state of charge";
	ret = bd71827_calc_soc(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to calculate clamped state of charge";
	ret = bd71827_calc_soc_clamp(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to get charger online status";
	ret = bd71827_get_online(pwr, wd);
	if (ret) 
		goto err_out;

	errstr = "Failed to get charger state";
	ret = bd71827_charge_status(pwr, wd, 1);
	if (ret) 
		goto err_out;

	if(pwr->suspend_resume) {
		pwr->suspend_resume = 0;
		changed = 1;
	}

	if (changed || cap_counter++ > JITTER_REPORT_CAP / JITTER_DEFAULT) {
		power_supply_changed(pwr->ac);
		power_supply_changed(pwr->bat);
		cap_counter = 0;
	}

printk(KERN_DEBUG"bd71827-power %s():soc(c=%d,v=%d,clamp_soc=%d),vbat(stat=(%d,%d),v=%d,vmin=%d),vsys(v=%d,vmin=%d),curr=%d(avg=%d),cc=%d,dcap=%d,fcap=%d\n",
 __func__,
 wd->soc_norm,wd->soc,wd->clamp_soc,
 wd->bat_status,wd->rpt_status,wd->vcell,wd->vcell_min,
 wd->vsys,wd->vsys_min,
 wd->curr,wd->curr_avg,
 wd->coulomb_cnt,wd->designed_cap,wd->full_cap);


	pwr->gauge_delay = JITTER_DEFAULT;
	schedule_delayed_work(&pwr->bd_work,
			      msecs_to_jiffies(JITTER_DEFAULT));

	update_soc_data(pwr);
	ciritical_low_count = 0;
	return;
err_out:	
    	dev_err(pwr->mfd->dev, "fuel-gauge cycle error %d - %s\n", ret,
		(errstr) ? errstr : "Unknown error");
}

/** @brief get property of power supply ac
 *  @param psy power supply deivce
 *  @param psp property to get
 *  @param val property value to return
 *  @retval 0  success
 *  @retval negative fail
 */
static int bd71827_charger_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct bd71827_power *pwr = dev_get_drvdata(psy->dev.parent);
	u32 vot;
	uint16_t tmp;
	int ret;
	//struct bd7182x_soc_data *wr = &pwr->d_r;
	smp_rmb();
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		spin_lock(&pwr->dlock);
		val->intval = pwr->charger_type;//pwr->charger_online;
		spin_unlock(&pwr->dlock);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bd7182x_read16_himask(pwr, pwr->regs->vdcin ,
					    BD7182x_MASK_VDCIN_U, &tmp);
		if (ret)
			return ret;

		vot = tmp;
		val->intval = 5000 * vot;		// 5 milli volt steps
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/** @brief get property of power supply ac
 *  @param psy power supply deivce
 *  @param psp property to get
 *  @param val property value to return
 *  @retval 0  success
 *  @retval negative fail
 */
static int bd71827_usb_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct bd71827_power *pwr = dev_get_drvdata(psy->dev.parent);
	u32 vot;
	uint16_t tmp;
	int ret;
	struct bd7182x_soc_data *wr = &pwr->d_r;

	smp_rmb();
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		spin_lock(&pwr->dlock);
		val->intval = pwr->charger_type;//pwr->charger_online;
		spin_unlock(&pwr->dlock);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/** @brief get property of power supply bat
 *  @param psy power supply deivce
 *  @param psp property to get
 *  @param val property value to return
 *  @retval 0  success
 *  @retval negative fail
 */

static int bd71827_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct bd71827_power *pwr = dev_get_drvdata(psy->dev.parent);
	struct bd7182x_soc_data *wr = &pwr->d_r;
	struct bd7182x_soc_data *wd = &pwr->d_w;
	int ret = 0;

	spin_lock(&pwr->dlock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = wr->rpt_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = wr->bat_health;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (wr->rpt_status == POWER_SUPPLY_STATUS_CHARGING)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = wr->bat_online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = wr->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = wr->clamp_soc;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	{
		u32 t;

		t = wr->coulomb_cnt >> 16;
		t = A10s_mAh(t, pwr->curr_factor);
		if (t > A10s_mAh(wr->designed_cap, pwr->curr_factor))
			 t = A10s_mAh(wr->designed_cap, pwr->curr_factor);
		val->intval = t * 1000;		/* uA to report */
		break;
	}
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = wr->bat_online;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = BATTERY_FULL_DEFAULT *
			      A10s_mAh(wr->designed_cap, pwr->curr_factor)* 10;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = BATTERY_FULL_DEFAULT *
			      A10s_mAh(wr->full_cap, pwr->curr_factor) * 10;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = wr->curr_avg;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = wr->curr;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = wr->temp * 10; /* 0.1 degrees C unit */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = MAX_VOLTAGE_DEFAULT;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		val->intval = MIN_VOLTAGE_DEFAULT;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = MAX_CURRENT_DEFAULT;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	spin_unlock(&pwr->dlock);

	return ret;
}

/** @brief ac properties */
static enum power_supply_property bd71827_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property bd71827_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

/** @brief bat properies */
static enum power_supply_property bd71827_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	//POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static ssize_t bd71827_sysfs_set_charging(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	ssize_t ret = 0;
	unsigned int val;

	ret = sscanf(buf, "%x", &val);
	if (ret < 1) {
		return ret;
	}

	if (ret == 1 && val >1) {
		dev_warn(dev, "use 0/1 to disable/enable charging\n");
		return -EINVAL;
	}

	if(val == 1)
		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
					 BD7182x_MASK_CHG_EN,
					 BD7182x_MASK_CHG_EN );
	else
		ret = regmap_update_bits(pwr->mfd->regmap, pwr->regs->chg_en,
					 BD7182x_MASK_CHG_EN,
					 0);
	if (ret)
		return ret;

	return count;
}

static ssize_t bd71827_sysfs_show_charging(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int chg_en, ret;

	ret = regmap_read(pwr->mfd->regmap, pwr->regs->chg_en, &chg_en);
	if (ret)
		return ret;

	chg_en &= BD7182x_MASK_CHG_EN;
	smp_rmb();
	return sprintf(buf, "%x\n", pwr->d_w.charger_online && chg_en);
}

static DEVICE_ATTR(charging, S_IWUSR | S_IRUGO,
		bd71827_sysfs_show_charging, bd71827_sysfs_set_charging);

static ssize_t bd71827_sysfs_set_gauge(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	ssize_t ret;
	int delay;

	ret = sscanf(buf, "%d", &delay);
	if (ret < 1) {
		dev_err(pwr->mfd->dev, "error: write a integer string");
		return -EINVAL;
	}

	if (delay == -1) {
		dev_info(pwr->mfd->dev, "Gauge schedule cancelled\n");
		cancel_delayed_work(&pwr->bd_work);
		return count;
	}

	dev_info(pwr->mfd->dev, "Gauge schedule in %d\n", delay);
	pwr->gauge_delay = delay;
	smp_wmb();
	schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(delay));

	return count;
}

static ssize_t bd71827_sysfs_show_gauge(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	ssize_t ret;

	smp_rmb();
	ret = sprintf(buf, "Gauge schedule in %d\n",
		      pwr->gauge_delay);
	return ret;
}

static DEVICE_ATTR(gauge, S_IWUSR | S_IRUGO,
		bd71827_sysfs_show_gauge, bd71827_sysfs_set_gauge);

static ssize_t bd71827_sysfs_show_temperature(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int temp;
	ssize_t ret;

	smp_rmb();

	/* Get tempature */
	ret = pwr->get_temp(pwr, &temp);

	if (ret)
		return ret;

	ret = sprintf(buf, "%d\n", temp);
	return ret;
}

static DEVICE_ATTR(temperature, S_IWUSR | S_IRUGO,
		bd71827_sysfs_show_temperature, NULL);

static ssize_t bd71827_sysfs_show_register(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct bd71827_power *pwr = power_supply_get_drvdata(psy);
	int temp,count,offset=0;
	ssize_t ret=0;

	smp_rmb();

	sprintf(&buf[0], "00: ");
	offset = 4;
	for (count=0;count<0x100;count++) {
		regmap_read(pwr->mfd->regmap, count, &temp);
		if((count+1)%16==0 && count && ((count+1)/16)<16)
			ret = sprintf(&buf[offset], "%02x \n%02d: ", temp, (count+1)/16);
		else if((count+1)%16==0 && count && ((count+1)/16)==16)
			ret = sprintf(&buf[offset], "%02x \n", temp);
		else
			ret = sprintf(&buf[offset], "%02x ", temp);
		offset += ret;
	}

	return offset;
}

static DEVICE_ATTR(register, S_IWUSR | S_IRUGO,
		bd71827_sysfs_show_register, NULL);

static struct attribute *bd71827_sysfs_attributes[] = {
	&dev_attr_charging.attr,
	&dev_attr_gauge.attr,
	&dev_attr_temperature.attr,
	&dev_attr_register.attr,
	NULL,
};

static const struct attribute_group bd71827_sysfs_attr_group = {
	.attrs = bd71827_sysfs_attributes,
};

/** @brief powers supplied by bd71827_ac */
static char *bd71827_ac_supplied_to[] = {
	BAT_NAME,
};

static const struct power_supply_desc bd71827_ac_desc = {
	.name		= AC_NAME,
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.properties	= bd71827_charger_props,
	.num_properties	= ARRAY_SIZE(bd71827_charger_props),
	.get_property	= bd71827_charger_get_property,
};

static const struct power_supply_desc bd71827_usb_desc = {
	.name 		= "usb",
	.type 		= POWER_SUPPLY_TYPE_USB,
	.properties = bd71827_usb_props,
	.num_properties = ARRAY_SIZE(bd71827_usb_props),
	.get_property = bd71827_usb_get_property,
};

static const struct power_supply_desc bd71827_battery_desc = {
	.name		= BAT_NAME,
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties	= bd71827_battery_props,
	.num_properties	= ARRAY_SIZE(bd71827_battery_props),
	.get_property	= bd71827_battery_get_property,
};

#ifdef PWRCTRL_HACK
/* This is not-so-pretty hack for allowing external code to call
 * bd71827_chip_hibernate() without this power device-data
 */
static struct bd71827_power *hack = NULL;
static DEFINE_SPINLOCK(pwrlock);

static struct bd71827_power * get_power()
{
	mutex_lock(&pwrlock);
	if (!hack) {
		mutex_unlock(&pwrlock);
		return -ENOENT;
	}
	return hack;
}

static void put_power()
{
	mutex_unlock(&pwrlock);
}

static int set_power(struct bd71827_power *pwr)
{
	mutex_lock(&pwrlock);
	hack = pwr;
	mutex_unlock(&pwrlock);
}

static void free_power()
{
	mutex_lock(&pwrlock);
	hack = NULL;
	mutex_unlock(&pwrlock);
}

/* called from pm inside machine_halt */
void bd71827_chip_hibernate(void)
{
	struct bd71827_power *pwr = get_power();

	if (IS_ERR(pwr)) {
		pr_err("bd71827_chip_hibernate called before probe finished\n");
		return PTR_ERR(pwr);
	}

	/* programming sequence in EANAB-151 */
	regmap_update_bits(pwr->mfd->regmap, pwr->regs->pwrctrl,
			   pwr->regs->hibernate_mask, 0);
	regmap_update_bits(pwr->mfd->regmap, pwr->regs->pwrctrl,
			   pwr->regs->hibernate_mask,
			   pwr->regs->hibernate_mask);
	put_power();
}
#endif // PWRCTRL_HACK

#define RSENS_CURR 10000000000LLU

static int bd7182x_set_chip_specifics(struct bd71827_power *pwr, int rsens_ohm)
{
	u64 tmp = RSENS_CURR;
	switch (pwr->chip_type) {
		case ROHM_CHIP_TYPE_BD71828:
			pwr->regs = &pwr_regs_bd71828;
			pwr->get_temp = bd71828_get_temp;
			break;
		case ROHM_CHIP_TYPE_BD71827:
			pwr->regs = &pwr_regs_bd71827;
			pwr->get_temp = bd71827_get_temp;
			dev_warn(pwr->mfd->dev, "BD71817 not tested\n");
		case ROHM_CHIP_TYPE_BD71878:
			MIN_VOLTAGE_DEFAULT = MIN_VOLTAGE_DEFAULT_78;
			ocv_table_default = ocv_table_78;
			vdr_table_h_default = vdr_table_h_78;
			vdr_table_m_default = vdr_table_m_78;
			vdr_table_l_default = vdr_table_l_78;
			vdr_table_vl_default = vdr_table_vl_78;
			BATTERY_CAP_MAH_DEFAULT = BATTERY_CAP_MAH_DEFAULT_78;
			DGRD_CYC_CAP_DEFAULT = DGRD_CYC_CAP_DEFAULT_78;
			SOC_EST_MAX_NUM_DEFAULT = SOC_EST_MAX_NUM_DEFAULT_78;
			DGRD_TEMP_H_DEFAULT = DGRD_TEMP_H_78;
			DGRD_TEMP_M_DEFAULT = DGRD_TEMP_M_78;
			DGRD_TEMP_L_DEFAULT = DGRD_TEMP_L_78;
			DGRD_TEMP_CAP_H_DEFAULT = DGRD_TEMP_CAP_H_DEFAULT_78;
			DGRD_TEMP_CAP_M_DEFAULT = DGRD_TEMP_CAP_M_DEFAULT_78;
			DGRD_TEMP_CAP_L_DEFAULT = DGRD_TEMP_CAP_L_DEFAULT_78;
			break;
		default:
			dev_err(pwr->mfd->dev, "Unknown PMIC\n");
			return -EINVAL;
	}
	/* Reg val to uA */
	do_div(tmp, rsens_ohm);

	pwr->curr_factor = tmp;
	pr_info("Setting curr-factor to %u\n", pwr->curr_factor);
	return 0;
}

/* returns standard charger type defined in rohm-generic.h
 * NO charger: 0x0000, SDP Charger: 0x0001 , DCP Charger: 0x0021
 */
int bd71828_charger_detect(void)
{
    struct bd71827_power *pwr = platform_get_drvdata(gPwr_dev);

    switch(pwr->charger_type)
    {
        case BD_71879_CHARGER_TYPE_NONE:
            return NO_CHARGER_PLUGGED;
        case BD_71879_CHARGER_TYPE_DCP:
            return DCP_CHARGER;
        case BD_71879_CHARGER_TYPE_SDP:
            return SDP_CHARGER;
        default:
            dev_info(pwr->mfd->dev, "Undefined charger type\n");
            return SDP_CHARGER;
    }
}

#if 0
static irqreturn_t bd7182x_short_push(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	kobject_uevent(&(pwr->mfd->dev->kobj), KOBJ_OFFLINE);
	dev_info(pwr->mfd->dev, "POWERON_SHORT\n");

	return IRQ_HANDLED;
}
static irqreturn_t bd7182x_long_push(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	kobject_uevent(&(pwr->mfd->dev->kobj), KOBJ_OFFLINE);
	dev_info(pwr->mfd->dev, "POWERON_LONG\n");

	return IRQ_HANDLED;
}
static irqreturn_t bd7182x_mid_push(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	kobject_uevent(&(pwr->mfd->dev->kobj), KOBJ_OFFLINE);
	dev_info(pwr->mfd->dev, "POWERON_MID\n");

	return IRQ_HANDLED;
}
static irqreturn_t bd7182x_push(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	kobject_uevent(&(pwr->mfd->dev->kobj), KOBJ_ONLINE);
	dev_info(pwr->mfd->dev, "POWERON_PRESS\n");

	return IRQ_HANDLED;
}
#endif

extern int fts_switch_charger(u8 value);
static irqreturn_t bd7182x_dcin_removed(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;
	
	cancel_delayed_work_sync(&pwr->charger_detect_work);
	
	pwr->dc_out_jiffies = jiffies;
	dev_info(pwr->mfd->dev, "\n~~~DCIN removed\n");
	pwr->charger_type = BD_71879_CHARGER_TYPE_NONE;
	changeToNoneMode(pwr);
	usb_plug_handler(NULL);	
	
#if 0
	if(0x14 == gptHWCFG->m_val.bTouchCtrl){ // E60K24 , FT5426
		printk(KERN_ERR"[%s_%d] fts_switch_charger : 0\n",__FUNCTION__,__LINE__);
		fts_switch_charger(0);
	}
#endif

	return IRQ_HANDLED;
}

static irqreturn_t bd7182x_dcin_detected(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;
	
	cancel_delayed_work_sync(&pwr->charger_detect_work);
	
	pwr->dc_in_jiffies = jiffies;
	dev_info(pwr->mfd->dev, "\n~~~DCIN inserted\n");
	pwr->charger_type = BD_71879_CHARGER_TYPE_SDP;
	changeToSDPMode(pwr);
	schedule_delayed_work(&pwr->charger_detect_work, msecs_to_jiffies(100));
#if 0
	if(0x14 == gptHWCFG->m_val.bTouchCtrl){ // E60K24 , FT5426
		printk(KERN_ERR"[%s_%d] fts_switch_charger : 1\n",__FUNCTION__,__LINE__);
		fts_switch_charger(1);
	}
#endif

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_vbat_low_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	if (time_is_after_jiffies(pwr->boot_jiffies + msecs_to_jiffies(10000)))
		return IRQ_HANDLED;
	dev_info(pwr->mfd->dev, "\n~~~ VBAT MON Resumed ... \n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_vbat_low_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	if (time_is_after_jiffies(pwr->boot_jiffies + msecs_to_jiffies(10000)))
		return IRQ_HANDLED;
	dev_info(pwr->mfd->dev, "\n~~~ VBAT MON Detected ... \n");
	if(pwr->d_r.clamp_soc == 0) {
		//ciritical_low_flag = 1;
		power_supply_changed(pwr->bat);
		printk(KERN_INFO "PMU:%s Set ciritical_low_flag = 1 **********\n", __func__);
	}
	ciritical_low_count++;
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_bat_hi_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ Overtemp Detected ... \n");
	
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_bat_hi_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ Overtemp Resumed ... \n");
	
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_bat_low_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ Lowtemp Detected ... \n");
	
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_bat_low_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ Lowtemp Resumed ... \n");
	
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_vf_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VF Detected ... \n");
	
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_vf_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VF Resumed ... \n");
	
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_vf125_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VF125 Detected ... \n");
	
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_temp_vf125_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ VF125 Resumed ... \n");
	
	return IRQ_HANDLED;
}

static irqreturn_t bd71827_charger_done(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	cancel_delayed_work_sync(&pwr->bd_work);
	schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(0));
	dev_info(pwr->mfd->dev, "\n~~~ CHARGER DONE ... \n");
	// finetune the VBAT_MNT for charged
	//regmap_update_bits(pwr->mfd->regmap, 0x81, 0x07, 0x04);

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_recharger_res(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ RECHARGER Resumed ... \n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_recharger_det(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ RECHARGER Detected ... \n");

	return IRQ_HANDLED;
}

static irqreturn_t bd71827_charger_wdg_expired(int irq, void *data)
{
	struct bd71827_power *pwr = (struct bd71827_power *)data;

	dev_info(pwr->mfd->dev, "\n~~~ CHARGER WHDOG EXPIRED ... \n");
	resetBatteryCharger(pwr);
	// finetune the VBAT_MNT for re-charged
	//regmap_update_bits(pwr->mfd->regmap, 0x81, 0x07, 0x06);

	return IRQ_HANDLED;
}

struct bd7182x_irq_res {
	const char *name;
	irq_handler_t handler;
};

#define BDIRQ(na, hn) { .name = (na), .handler = (hn) }

int bd7182x_get_irqs(struct platform_device *pdev, struct bd71827_power *pwr)
{
	int i, irq, ret;
	static const struct bd7182x_irq_res irqs[] = {
/*		BDIRQ("bd71828-pwr-longpush", bd7182x_long_push),
		BDIRQ("bd71828-pwr-midpush", bd7182x_mid_push),
		BDIRQ("bd71828-pwr-shortpush", bd7182x_short_push),
		BDIRQ("bd71828-pwr-push", bd7182x_push),*/
		BDIRQ("bd71828-pwr-dcin-in", bd7182x_dcin_detected),
		BDIRQ("bd71828-pwr-dcin-out", bd7182x_dcin_removed),
		BDIRQ("bd71828-vbat-mon-res", bd71827_vbat_low_res),
		BDIRQ("bd71828-vbat-mon-det", bd71827_vbat_low_det),
		BDIRQ("bd71828-btemp-hi", bd71827_temp_bat_hi_det),
		BDIRQ("bd71828-btemp-cool", bd71827_temp_bat_hi_res),
		BDIRQ("bd71828-btemp-lo", bd71827_temp_bat_low_det),
		BDIRQ("bd71828-btemp-warm", bd71827_temp_bat_low_res),
		BDIRQ("bd71828-temp-hi", bd71827_temp_vf_det),
		BDIRQ("bd71828-temp-norm", bd71827_temp_vf_res),
		BDIRQ("bd71828-temp-125-over", bd71827_temp_vf125_det),
		BDIRQ("bd71828-temp-125-under", bd71827_temp_vf125_res),
		BDIRQ("bd71828-charger-done", bd71827_charger_done),
		//BDIRQ("bd71828-rechg-res", bd71827_recharger_res),
		//BDIRQ("bd71828-rechg-det", bd71827_recharger_det),
		BDIRQ("bd71828-charger-wdg-expired", bd71827_charger_wdg_expired),
	};

	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		irq = platform_get_irq_byname(pdev, irqs[i].name);

		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
						irqs[i].handler, 0 /* IRQF_ONESHOT */,
						irqs[i].name, pwr);
		if (ret)
			break;
	}

	return ret;
}

#define RSENS_DEFAULT_30MOHM 30000000

int dt_get_rsens(struct device *dev, int *rsens_ohm)
{
	if (dev->of_node) {
		int ret;
		uint32_t rs;

		ret = of_property_read_u32(dev->of_node,
					   "rohm,charger-sense-resistor-ohms",
					   &rs);
		if (ret) {
			if (ret == -EINVAL)
				return 0;

			dev_err(dev, "Bad RSENS dt property\n");
			return ret;
		}
		printk(KERN_ERR"%s() RSENS dts property %d\n",__func__, (int)rs);
		*rsens_ohm = (int)rs;
	}
	return 0;
}


static int bd71827_power_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct bd71827_power *pwr = platform_get_drvdata(pdev);

	if(pwr->suspend_resume) {
		printk(KERN_ERR"%s() wait one time call for schedule work!!!\n",__func__);
		return -1;
	}

	cancel_delayed_work_sync(&pwr->bd_work);

	if(delayed_work_pending(&pwr->charger_detect_work))	{
		printk(KERN_ERR"%s() charger detect work is pending !\n",__func__);
		return -1;
	}

	if(work_busy(&pwr->charger_detect_work.work)) {
		printk(KERN_ERR"%s() charger detect work is busy !\n",__func__);
		return -1;
	}

	if (time_is_after_jiffies(pwr->resume_jiffies + msecs_to_jiffies(1000))) {
		printk(KERN_ERR"%s() wait resume_jiffies !!!\n",__func__);
		return -1;
	}
	pwr->suspend_resume = 0;
	return 0;
};

static int bd71827_power_resume(struct platform_device *pdev)
{
	struct bd71827_power *pwr = platform_get_drvdata(pdev);

	pwr->resume_jiffies = jiffies;
	pwr->suspend_resume = 1;
	cancel_delayed_work_sync(&pwr->bd_work);
	schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(500));
	return 0;
};

/** @brief probe pwr device 
 * @param pdev platform deivce of bd71827_power
 * @retval 0 success
 * @retval negative fail
 */
static int bd71827_power_probe(struct platform_device *pdev)
{
//	struct bd71827 *bd71827 = dev_get_drvdata(pdev->dev.parent);
	struct rohm_regmap_dev *mfd;
	struct bd71827_power *pwr;
	struct power_supply_config ac_cfg = {};
	struct power_supply_config usb_cfg = {};
	struct power_supply_config bat_cfg = {};
	int ret;
	int rsens_ohm = RSENS_DEFAULT_30MOHM;

	printk(KERN_INFO "PMU: %s begin \n", __func__);

	mfd = dev_get_drvdata(pdev->dev.parent);

	pwr = devm_kzalloc(&pdev->dev, sizeof(*pwr), GFP_KERNEL);
	if (pwr == NULL)
		return -ENOMEM;

    pwr->charger_type = BD_71879_CHARGER_TYPE_NONE;
	pwr->dc_in_jiffies = jiffies;
	pwr->dc_out_jiffies = jiffies;
	pwr->resume_jiffies = jiffies;
	pwr->suspend_resume = 0;
	pwr->boot_jiffies = jiffies;
	pwr->chip_type = mfd->chip_type;//platform_get_device_id(pdev)->driver_data;
	pwr->mfd = mfd;
	pwr->mfd->dev = &pdev->dev;
	spin_lock_init(&pwr->dlock);

	ret = dt_get_rsens(pdev->dev.parent, &rsens_ohm);
	if (ret)
		return ret;
	else
		pr_info("RSENS prop found %u\n", rsens_ohm);

	ret = bd7182x_set_chip_specifics(pwr, rsens_ohm);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pwr);

	gPwr_dev = pdev;

	if (battery_cycle <= 0) {
		battery_cycle = 0;
	}
	dev_info(pwr->mfd->dev, "battery_cycle = %d\n", battery_cycle);

	/* If the product often power up/down and the power down time is long, the Coulomb Counter may have a drift. */
	/* If so, it may be better accuracy to enable Coulomb Counter using following commented out code */
	/* for counting Coulomb when the product is power up(including sleep). */
	/* The condition  */
	/* (1) Product often power up and down, the power down time is long and there is no power consumed in power down time. */
	/* (2) Kernel must call this routin at power up time. */
	/* (3) Kernel must call this routin at charging time. */
	/* (4) Must use this code with "Stop Coulomb Counter" code in bd71827_power_remove() function */
	/* Start Coulomb Counter */
	/* bd71827_set_bits(pwr->mfd, pwr->regs->coulomb_ctrl, BD7182x_MASK_CCNTENB); */

	bd71827_set_battery_parameters(pwr);

	bd71827_init_hardware(pwr, &pwr->d_w);

	bat_cfg.drv_data 			= pwr;
	pwr->bat = devm_power_supply_register(&pdev->dev, &bd71827_battery_desc,
					 &bat_cfg);
	if (IS_ERR(pwr->bat)) {
		ret = PTR_ERR(pwr->bat);
		dev_err(&pdev->dev, "failed to register bat: %d\n", ret);
		return ret;
	}

	usb_cfg.supplied_to			= bd71827_ac_supplied_to;
	usb_cfg.num_supplicants		= ARRAY_SIZE(bd71827_ac_supplied_to);
	usb_cfg.drv_data 			= pwr;
	pwr->ac = devm_power_supply_register(&pdev->dev, &bd71827_usb_desc, &usb_cfg);
	if (IS_ERR(pwr->ac)) {
		ret = PTR_ERR(pwr->ac);
		dev_err(&pdev->dev, "failed to register usb: %d\n", ret);
		return ret;
	}

	ac_cfg.supplied_to			= bd71827_ac_supplied_to;
	ac_cfg.num_supplicants		= ARRAY_SIZE(bd71827_ac_supplied_to);
	ac_cfg.drv_data 			= pwr;
	pwr->ac = devm_power_supply_register(&pdev->dev, &bd71827_ac_desc, &ac_cfg);
	if (IS_ERR(pwr->ac)) {
		ret = PTR_ERR(pwr->ac);
		dev_err(&pdev->dev, "failed to register ac: %d\n", ret);
		return ret;
	}

	/*ret = bd7182x_get_irqs(pdev, pwr);
	if (ret) {
		dev_err(&pdev->dev, "failed to request IRQs: %d\n", ret);
		return ret;
	};*/

	/* Configure wakeup capable */
	device_set_wakeup_capable(pwr->mfd->dev, 1);
	device_set_wakeup_enable(pwr->mfd->dev , 1);

	ret = sysfs_create_group(&pwr->bat->dev.kobj,
				 &bd71827_sysfs_attr_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register sysfs interface\n");
		return ret;
	}

	INIT_DELAYED_WORK(&pwr->bd_work, bd_work_callback);
	INIT_DELAYED_WORK(&pwr->charger_detect_work, charger_detect_work_func);

	/* Schedule timer to check current status */
	pwr->gauge_delay = 0;
	smp_wmb();
	schedule_delayed_work(&pwr->bd_work, msecs_to_jiffies(3000));
	schedule_delayed_work(&pwr->charger_detect_work, msecs_to_jiffies(3000));

	_create_sys_attrs(&pdev->dev);
	// enable charge, disable by uboot
	regmap_write(pwr->mfd->regmap, BD71828_REG_CHG_EN, 0x01);
	// disbale current limitation when charge current less then system current
	ret = regmap_update_bits(pwr->mfd->regmap, 0x76, 0x08, 0x00);
	if (ret)
		return ret;

	{
		printk(KERN_INFO "%s modify the OTP config for battery temperature\n", __func__);
		regmap_write(pwr->mfd->regmap, 0xfe, 0x8c);
		regmap_write(pwr->mfd->regmap, 0xff, 0x01);
		regmap_write(pwr->mfd->regmap_otp, 0x80, 0xa0);		//T3 fall
		regmap_write(pwr->mfd->regmap_otp, 0x81, 0xa0);		//T3 rise
		regmap_write(pwr->mfd->regmap_otp, 0x82, 0x9f);		//T4 fall
		regmap_write(pwr->mfd->regmap_otp, 0x83, 0x9d);		//T4 rise
		regmap_write(pwr->mfd->regmap_otp, 0x84, 0x9f);		//T5 fall
		regmap_write(pwr->mfd->regmap_otp, 0x85, 0x9d);		//T5 rise
		regmap_write(pwr->mfd->regmap, 0xfe, 0x8c);
		regmap_write(pwr->mfd->regmap, 0xff, 0x00);
	}

	ret = bd7182x_get_irqs(pdev, pwr);
	if (ret) {
		dev_err(&pdev->dev, "failed to request IRQs: %d\n", ret);
		return ret;
	}

	printk(KERN_INFO "PMU: %s end \n", __func__);

	return 0;
}

/** @brief remove pwr device
 * @param pdev platform deivce of bd71827_power
 * @return 0
 */

static int bd71827_power_remove(struct platform_device *pdev)
{
	struct bd71827_power *pwr = platform_get_drvdata(pdev);

	/* If the product often power up/down and the power down time is long, the Coulomb Counter may have a drift. */
	/* If so, it may be better accuracy to disable Coulomb Counter using following commented out code */
	/* for stopping counting Coulomb when the product is power down(without sleep). */
	/* The condition  */
	/* (1) Product often power up and down, the power down time is long and there is no power consumed in power down time. */
	/* (2) Kernel must call this routin at power down time. */
	/* (3) Must use this code with "Start Coulomb Counter" code in bd71827_power_probe() function */
	/* Stop Coulomb Counter */
	/* bd71827_clear_bits(pwr->mfd, pwr->regs->coulomb_ctrl, BD7182x_MASK_CCNTENB); */

	_remove_sys_attrs(&pdev->dev);
	sysfs_remove_group(&pwr->bat->dev.kobj, &bd71827_sysfs_attr_group);
	cancel_delayed_work(&pwr->bd_work);
	cancel_delayed_work_sync(&pwr->charger_detect_work);

	return 0;
}

static const struct platform_device_id bd718x7_id[] = {
	{ "bd71827-power", ROHM_CHIP_TYPE_BD71827 },
	{ "bd71828-power", ROHM_CHIP_TYPE_BD71828 },
	{ "bd71878-power", ROHM_CHIP_TYPE_BD71878 },
	{ },
};
MODULE_DEVICE_TABLE(platform, bd718x7_id);

static struct platform_driver bd71827_power_driver = {
	.driver = {
		.name = "bd71827-power",
		.owner = THIS_MODULE,
	},
	.probe = bd71827_power_probe,
	.remove = bd71827_power_remove,
	.suspend = bd71827_power_suspend,
	.resume = bd71827_power_resume,
};

//module_platform_driver(bd71827_power_driver);

static int __init bd71827_battery_init(void)
{
	printk(KERN_INFO "PMU: %s\n", __func__);
	return platform_driver_register(&bd71827_power_driver);
}
subsys_initcall(bd71827_battery_init);

static void __exit bd71827_battery_exit(void)
{
	platform_driver_unregister(&bd71827_power_driver);
}
module_exit(bd71827_battery_exit);

module_param(use_load_bat_params, int, S_IRUGO);
MODULE_PARM_DESC(use_load_bat_params, "use_load_bat_params:Use loading battery parameters");

module_param(battery_cap_mah, int, S_IRUGO);
MODULE_PARM_DESC(battery_cap_mah, "battery_cap_mah:Battery capacity (mAh)");

module_param(dgrd_cyc_cap, int, S_IRUGO);
MODULE_PARM_DESC(dgrd_cyc_cap, "dgrd_cyc_cap:Degraded capacity per cycle (uAh)");

module_param(soc_est_max_num, int, S_IRUGO);
MODULE_PARM_DESC(soc_est_max_num, "soc_est_max_num:SOC estimation max repeat number");

module_param(dgrd_temp_cap_h, int, S_IRUGO);
MODULE_PARM_DESC(dgrd_temp_cap_h, "dgrd_temp_cap_h:Degraded capacity at high temperature (uAh)");

module_param(dgrd_temp_cap_m, int, S_IRUGO);
MODULE_PARM_DESC(dgrd_temp_cap_m, "dgrd_temp_cap_m:Degraded capacity at middle temperature (uAh)");

module_param(dgrd_temp_cap_l, int, S_IRUGO);
MODULE_PARM_DESC(dgrd_temp_cap_l, "dgrd_temp_cap_l:Degraded capacity at low temperature (uAh)");

module_param(battery_cycle, uint, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(battery_parameters, "battery_cycle:battery charge/discharge cycles");

module_param_array(ocv_table, int, NULL, S_IRUGO);
MODULE_PARM_DESC(ocv_table, "ocv_table:Open Circuit Voltage table (uV)");

module_param_array(vdr_table_h, int, NULL, S_IRUGO);
MODULE_PARM_DESC(vdr_table_h, "vdr_table_h:Voltage Drop Ratio temperatyre high area table");

module_param_array(vdr_table_m, int, NULL, S_IRUGO);
MODULE_PARM_DESC(vdr_table_m, "vdr_table_m:Voltage Drop Ratio temperatyre middle area table");

module_param_array(vdr_table_l, int, NULL, S_IRUGO);
MODULE_PARM_DESC(vdr_table_l, "vdr_table_l:Voltage Drop Ratio temperatyre low area table");

module_param_array(vdr_table_vl, int, NULL, S_IRUGO);
MODULE_PARM_DESC(vdr_table_vl, "vdr_table_vl:Voltage Drop Ratio temperatyre very low area table");

MODULE_AUTHOR("Cong Pham <cpham2403@gmail.com>");
MODULE_DESCRIPTION("ROHM BD71827/BD71828 PMIC Battery Charger driver");
MODULE_LICENSE("GPL");

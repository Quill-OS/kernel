/*
 * include/linux/mfd/wm831x/core.h -- Core interface for WM831x
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */



#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/types.h>

#define SUB_NORMAL_MODE 1
#define SUB_UPDATE_MODE 2

#define SEND_BUFFER_LENGTH 255
#define READ_BUFFER_LENGTH 255
#define CMD_LENGTH 0x01
#define CHECK_SUM_LENGTH 0x01

#define REG_WRITE_CMD		0x01
#define REG_READ_CMD		0x02
#define ALL_CONFIG_READ_CMD	0x03
#define ALL_STATUS_READ_CMD	0x04
#define UPDATE_REG_CMD		0x05
#define STANDBY_REQ_CMD		0x06
#define RESUME_REQ_CMD		0x07
#define PMIC_INIT_COMPLETE_CMD		0x08

#define REQ_MCU_FW_VER_CMD	0x80
#define REQ_UPD_PGM_VER_CMD	0x81
#define SUB_VER_LENGTH		0x08

#define CURRENT_LIMIT_REG		0x01
#define CURRENT_SETTING_BITS	0xF0
#define CURRENT_LIMIT_1A		0x40

#define PIN_CONTROL_REG		0x05
#define AUTO_CHG_ENABLE_BIT		(1 << 7)

#define CMD_A_REG			0x31
#define HC_MODE_BIT				(1 << 2)
#define USB_500_MODE_BIT		(1 << 3)
#define BATT_CHG_DISABLE		(1 << 4)
#define ALLOW_VOLATILE_WRITES_BIT	(1 << 7)

/* Status Register B */
#define STATUS_REG_B		0x33
#define USB_UV_BIT			(1 << 0)
#define USBIN_OV_BIT			(1 << 1)
#define INTERNAL_TEMP_LIMIT_BIT	(1 << 4)

/* Status Register D */
#define TEMP_TOO_COLD_BIT	(1 << 4)
#define TEMP_TOO_HOT_BIT		(1 << 5)

/* Status Register E */
#define CHG_ERROR_BIT		(1 << 3)
#define CHG_TERMINATION_CURRENT_BIT	(1 << 6)

/* Status Register F */
#define BATT_LOW_BIT		(1 << 0)
#define SAFETY_TIMER_BITS	(0x03 << 4)
#define HOLDOFF_TIMER_BIT	(0x03 << 4)

/* Extended Register A (Address 0x3D) */
#define EXT_REG_A			0x3D
#define WDT_CLEAR_BIT		(1 << 0)
#define WDT_START_BIT		(1 << 1)
#define SHUTDOWN_BIT		(1 << 2)
#define REBOOT_BIT			(1 << 3)
#define NO_BATT_BIT			(1 << 4)
#define CHG_SETTING_BITS	(0x07 << 5)
#define CHG_STOP			(1 << 5)
#define CHG_100				(1 << 6)
#define CHG_500				(0x03 << 5)
#define CHG_AC				(1 << 7)

/* Extended Register C (Address 0x3F) */
#define OVP_OK_BIT			(1 << 0)
#define CHG_LED_BIT			(1 << 1)
#define PWR_KEY_BIT			(1 << 2)
#define USB_DET_BIT			(1 << 3)
#define USB_SUSP_BIT		(1 << 4)
#define LOW_BATT_BIT		(1 << 5)
#define WDT_EXPIRE_BIT		(1 << 6)

/* Extended Register D (Address 0x40) */
#define EXT_D_REG_ADDRS		0x40
#define WDT2_ENABLE_BIT		(1 << 7)

typedef struct {
	u8	a;
	u8	b;
	u8	c;
	u8	d;
	u8	e;
	u8	f;
	u8	g;
	u8	h;
	u8	ext_a;
	u8	ext_b;
	u8	ext_c;
}CHG_IC_STATUS_REG;

typedef struct {
	u8	charge_current;
	u8	input_current_limit;
	u8	float_voltage;
	u8	control_reg_a;
	u8	control_reg_b;
	u8	pin_control;
	u8	otg_lbr_control;
	u8	fault_interrupt_reg;
	u8	cell_temperature_monitor;
	u8	safety_timers;
	u8	vsys;
	u8	i2c_address;
	u8	irq_reset;
	u8	command_reg_a;
}CHG_IC_CONFIG_REG;

struct sub_main {
	
	struct mutex io_lock;

	struct device *dev;

	void *control_data;
	
	int control_mode;
	
	int standby_flag;

	struct completion irq_event;
	
	struct wake_lock sub_irq_wake_lock;
	
	struct wake_lock wake_up_wake_lock;

	CHG_IC_STATUS_REG chg_ic_status_reg;
	
	CHG_IC_STATUS_REG pre_chg_ic_status_reg;
	
	CHG_IC_CONFIG_REG chg_ic_config_reg;
};

//struct sub_cpu_vbus_data {
	/* Called when the state of VBUS changes */
//	int		(*board_control_vbus_power)(struct device *dev, int on);
//};

struct sub_cpu_bci_platform_data {
	int (*board_control_vbus_power)(struct device *dev, int on);
};

struct sub_main_platform_data {
	
	int (*init)(void);
	
	struct sub_cpu_bci_platform_data	*bci;
	
	//unsigned				irq_base, irq_end;

};

/* Device I/O API */
int write_sub_cpu_reg(u8 reg_address, u8 write_data);
int read_sub_cpu_reg(u8 reg_address, u8 *read_data);
int set_bit_sub_cpu_reg(u8 reg_address, u8 set_bit);
int clear_bit_sub_cpu_reg(u8 reg_address, u8 clear_bit);
int read_all_config_reg(void);
int read_all_status_reg(void);
int update_sub_reg(void);
int sub_wdt_start(void);
int sub_wdt_stop(void);
int sub_wdt_clear(void);
void wm8321_init_complete_cmd(void);


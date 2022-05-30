/*
 * linux/drivers/power/sub_cpu_battery.c
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
#include <linux/power_supply.h>
#include <linux/mfd/sub-main/core.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <mach/gpio.h>

#define SPI_SUB_INT    (2*32+26) /* GPIO_3_26 */
#define SET_ID	(5*32+15)	/* GPIO_6_15 */
#define WIFI_MODEL	1
#define WAN_MODEL	0

extern struct sub_main *confirm_the_sub(void);
extern int read_all_status_reg(void);
extern void disable_spi_sub_int_irq(void);

static struct sub_main *the_sub;

static int set_model;

struct regulator* disp_regu;
struct regulator* temp_regu;

static void set_charge_current_0mA(struct work_struct *ignored);
static void set_charge_current_100mA(struct work_struct *ignored);
static void set_charge_current_500mA(struct work_struct *ignored);
static void set_charge_current_1000mA(struct work_struct *ignored);

static DECLARE_WORK(set_charge_current_0mA_work, set_charge_current_0mA);
static DECLARE_WORK(set_charge_current_100mA_work, set_charge_current_100mA);
static DECLARE_WORK(set_charge_current_500mA_work, set_charge_current_500mA);
static DECLARE_WORK(set_charge_current_1000mA_work, set_charge_current_1000mA);
struct workqueue_struct *charge_work_queue;

static int battery_voltage_table_wifi[] = {
	/* voltage for capacity in steps of 5%
	 * (or steps of 100% / table size) */
	3610,	/* 0% (Level 0->No Batt) */
	3680,	/* 5% */
	3710,	/* 10%  (Level 1->0) */
	3730,	/* 15% */
	3750,	/* 20% */
	3770,	/* 25% (Level 2>1) */
	3780,	/* 30% */
	3790,	/* 35% */
	3800,	/* 40% (Level 3->2) */
	3810,	/* 45% */
	3830,	/* 50% */
	3850,	/* 55% (Level 4->3) */
	3870,	/* 60% */
	3900,	/* 65% */
	3930,	/* 70% */
	3960,	/* 75% */
	3990,	/* 80% */
	4030,	/* 85% */
	4070,	/* 90% */
	4110,	/* 95% */
	4160,	/* 100% */
};

static int battery_voltage_table_cold_wifi[] = {
	/* voltage for capacity in steps of 5%
	 * (or steps of 100% / table size) */
	3610,	/* 0% (Level 0->No Batt) */
	3660,	/* 5% */
	3690,	/* 10% (Level 1->0) */
	3720,	/* 15% */
	3730,	/* 20% */
	3740,	/* 25% (Level 2>1) */
	3750,	/* 30% */
	3770,	/* 35% */
	3780,	/* 40% (Level 3->2) */
	3800,	/* 45% */
	3820,	/* 50% */
	3840,	/* 55% (Level 4->3) */
	3870,	/* 60% */
	3890,	/* 65% */
	3920,	/* 70% */
	3950,	/* 75% */
	3980,	/* 80% */
	4020,	/* 85% */
	4060,	/* 90% */
	4110,	/* 95% */
	4160,	/* 100% */
};

static int battery_voltage_table_wan[] = {
	/* voltage for capacity in steps of 5%
	 * (or steps of 100% / table size) */
	3550,	/* 0% (Level 0->No Batt) */
	3670,	/* 5% */
	3700,	/* 10%  (Level 1->0) */
	3730,	/* 15% */
	3740,	/* 20% */
	3760,	/* 25% (Level 2>1) */
	3770,	/* 30% */
	3780,	/* 35% */
	3790,	/* 40% (Level 3->2) */
	3800,	/* 45% */
	3820,	/* 50% */
	3840,	/* 55% (Level 4->3) */
	3860,	/* 60% */
	3890,	/* 65% */
	3920,	/* 70% */
	3950,	/* 75% */
	3980,	/* 80% */
	4020,	/* 85% */
	4060,	/* 90% */
	4100,	/* 95% */
	4160,	/* 100% */
};

static int battery_voltage_table_cold_wan[] = {
	/* voltage for capacity in steps of 5%
	 * (or steps of 100% / table size) */
	3610,	/* 0% (Level 0->No Batt) */
	3680,	/* 5% */
	3720,	/* 10% (Level 1->0) */
	3740,	/* 15% */
	3750,	/* 20% */
	3760,	/* 25% (Level 2>1) */
	3770,	/* 30% */
	3780,	/* 35% */
	3790,	/* 40% (Level 3->2) */
	3810,	/* 45% */
	3820,	/* 50% */
	3840,	/* 55% (Level 4->3) */
	3870,	/* 60% */
	3890,	/* 65% */
	3920,	/* 70% */
	3950,	/* 75% */
	3990,	/* 80% */
	4030,	/* 85% */
	4070,	/* 90% */
	4110,	/* 95% */
	4160,	/* 100% */
};

struct sub_cpu_bci_device_info {
	struct device		*dev;
#if 0 /* FY11 EBOOK */
	int			*battery_tmp_tbl;
	unsigned int		battery_tmp_tblsize;
	int			*battery_volt_tbl;
	unsigned int		battery_volt_tblsize;
#endif /* FY11 EBOOK */

	int			voltage_uV;
	int			current_uA;
	int			temp_C;
	int			charge_status;
	int			android_charge_status;
	int			bat_health;
	int			charger_source;

	u8			battery_online;
	u8			usb_online;
	u8			ac_online;

	unsigned int		capacity;
	//unsigned int		old_capacity;
	
	struct power_supply	bat;
	struct power_supply	usb;
	struct power_supply	ac;

	//struct delayed_work	sub_cpu_check_battery_state_work;
	
	struct delayed_work	send_chraging_state_work;
	
	int detect_charger_complete_flg;
	
	int (*board_control_vbus_power)(struct device *dev, int on);

};

struct sub_cpu_bci_device_info *the_di = NULL;

static struct wake_lock update_battery_capacity;
static struct wake_lock no_battery;

 /*****************************************************************************
	sub_power_state_change_detect
*****************************************************************************/
void sub_power_state_change_detect(void)
{
	if(the_di){
		//schedule_delayed_work(&the_di->sub_cpu_check_battery_state_work, msecs_to_jiffies(0));
	}
}
EXPORT_SYMBOL(sub_power_state_change_detect);



 /*****************************************************************************
	sub_cpu_set_usb_in_current
*****************************************************************************/
static int sub_cpu_set_usb_in_current(int currentmA)
{
	u8 temp;
	
	if (!the_di)
		return -ENODEV;

	switch (currentmA) {
	case 0:
		read_sub_cpu_reg(EXT_REG_A, &temp);
		temp &= (u8)~CHG_SETTING_BITS;
		write_sub_cpu_reg(EXT_REG_A, temp);
		break;
	case 100:
		read_sub_cpu_reg(EXT_REG_A, &temp);
		temp &= (u8)~CHG_SETTING_BITS;
		temp |= CHG_100;
		write_sub_cpu_reg(EXT_REG_A, temp);
		break;
	case 500:
		read_sub_cpu_reg(EXT_REG_A, &temp);
		temp &= (u8)~CHG_SETTING_BITS;
		temp |= CHG_500;
		write_sub_cpu_reg(EXT_REG_A, temp);
		break;
	case 1000:
		read_sub_cpu_reg(EXT_REG_A, &temp);
		temp &= (u8)~CHG_SETTING_BITS;
		temp |= CHG_AC;
		write_sub_cpu_reg(EXT_REG_A, temp);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void set_charge_current_0mA(struct work_struct *ignored)
{
	sub_cpu_set_usb_in_current(0);
}

static void set_charge_current_100mA(struct work_struct *ignored)
{
	sub_cpu_set_usb_in_current(100);
}

static void set_charge_current_500mA(struct work_struct *ignored)
{
	sub_cpu_set_usb_in_current(500);
}

static void set_charge_current_1000mA(struct work_struct *ignored)
{
	sub_cpu_set_usb_in_current(1000);
}

int set_usb_charge_current(int current_mA)
{
	switch (current_mA) {
	case 0:
		queue_work(charge_work_queue, &set_charge_current_0mA_work);
		break;
	case 100:
		queue_work(charge_work_queue, &set_charge_current_100mA_work);
		break;
	case 500:
		queue_work(charge_work_queue, &set_charge_current_500mA_work);
		break;
	case 1000:
		queue_work(charge_work_queue, &set_charge_current_1000mA_work);
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}
EXPORT_SYMBOL(set_usb_charge_current);

 /*****************************************************************************
	send_shutdown_request
*****************************************************************************/
void send_shutdown_request(void)
{
	disable_spi_sub_int_irq();
	if( the_di->capacity == 0){
		if( !(the_sub->chg_ic_status_reg.ext_c & USB_DET_BIT) 				//USB_DET : Low = VBUS exist
			&& ((the_sub->chg_ic_status_reg.ext_a & CHG_500) == CHG_500)) 	//USB Charging 500mA mode
		{
			set_bit_sub_cpu_reg(EXT_REG_A, REBOOT_BIT);
		}else{
			set_bit_sub_cpu_reg(EXT_REG_A, NO_BATT_BIT);
		}
	}else{
		if( !(the_sub->chg_ic_status_reg.ext_c & USB_DET_BIT) ){ 	//USB_DET : Low = VBUS exist
			sub_cpu_set_usb_in_current(0); 	// Set USB charging 0mA
		}
		set_bit_sub_cpu_reg(EXT_REG_A, SHUTDOWN_BIT);
	}
}
EXPORT_SYMBOL(send_shutdown_request);

 /*****************************************************************************
	send_reboot_request
*****************************************************************************/
void send_reboot_request(void)
{
	printk("send send_reboot_request\n");
	set_bit_sub_cpu_reg(EXT_REG_A, REBOOT_BIT);
}
EXPORT_SYMBOL(send_reboot_request);

 /*****************************************************************************
	sub_cpu_set_power_source
*****************************************************************************/
int sub_cpu_set_power_source(int source)
{
	int chg_flag = 0;
	
	if (!the_di)
		return -ENODEV;
	
	if(the_di->charger_source != source){
		chg_flag = 1;
	}
	
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
	case POWER_SUPPLY_TYPE_BATTERY:
		the_di->charger_source = source;
		the_di->ac_online = 0;
		the_di->usb_online = 0;
		break;
	default:
		return -EINVAL;
	}
	
	if(chg_flag){
		wake_lock_timeout(&update_battery_capacity, 1 * HZ);
		power_supply_changed(&the_di->bat);
	}

	return 0;
}
EXPORT_SYMBOL(sub_cpu_set_power_source);

extern void enable_otg_vbus_wake_up(bool enable);

#ifdef CONFIG_REGULATOR_TWL4030 
static struct regulator* ldo5;
static struct regulator* ldousb;
#endif

#ifdef CONFIG_REGULATOR_WM831X
static struct regulator* ldo8;	//USBPHY 2.5V
static struct regulator* ldo9;	//USBPHY 3.1V
#endif

int comfirm_usb_phy_power_state(void)
{
#ifdef CONFIG_REGULATOR_WM831X
	return (regulator_is_enabled(ldo8) && regulator_is_enabled(ldo9));
#endif
	
#ifdef CONFIG_REGULATOR_TWL4030
	return (regulator_is_enabled(ldo5) && regulator_is_enabled(ldousb));
#endif
}


 /*****************************************************************************
	send_vbus_state
*****************************************************************************/
void send_vbus_state(void)
{	
	//USB_DET : Low = VBUS exist 
	if( the_sub->chg_ic_status_reg.ext_c & USB_DET_BIT ){
		
		if(comfirm_usb_phy_power_state()){
			enable_otg_vbus_wake_up(false);
		}
		
		sub_cpu_set_power_source(POWER_SUPPLY_TYPE_BATTERY);

		//Send VBUS OFF Key Code
		the_di->board_control_vbus_power(NULL, 0);

	}else{
		
		if(comfirm_usb_phy_power_state()){
			enable_otg_vbus_wake_up(true);
		}
		
		sub_cpu_set_power_source(POWER_SUPPLY_TYPE_MAINS);
		
		//Send VBUS OFF Key Code
		the_di->board_control_vbus_power(NULL, 1);

	}
}
EXPORT_SYMBOL(send_vbus_state);

 /*****************************************************************************
	send_vbus_state_change_info
*****************************************************************************/
static void send_vbus_state_change_info(void)
{	
	if( (the_sub->pre_chg_ic_status_reg.ext_c & USB_DET_BIT)
		!= (the_sub->chg_ic_status_reg.ext_c & USB_DET_BIT)){
			
		send_vbus_state();
	}
}

 /*****************************************************************************
	set_ac_usb_detect_flag
*****************************************************************************/
void set_ac_usb_detect_flag(int flag_info)
{
	if(!the_di)
		return;
		
	the_di->detect_charger_complete_flg = flag_info;
}
EXPORT_SYMBOL(set_ac_usb_detect_flag);

 /*****************************************************************************
	get_ac_online_info
*****************************************************************************/
int get_ac_online_info(void)
{
	if (!the_di)
		return -ENODEV;
	
	return the_di->ac_online;
}
EXPORT_SYMBOL(get_ac_online_info);

 /*****************************************************************************
	get_usb_online_info
*****************************************************************************/
int get_usb_online_info(void)
{
	if (!the_di)
		return -ENODEV;
	
	return the_di->usb_online;
}
EXPORT_SYMBOL(get_usb_online_info);

 /*****************************************************************************
	power_sorce_detection_complete
*****************************************************************************/
int power_sorce_detection_complete(void)
{
	if (!the_di)
		return -ENODEV;
	
	return the_di->detect_charger_complete_flg;
}
EXPORT_SYMBOL(power_sorce_detection_complete);

 /*****************************************************************************
	get_charging_info
*****************************************************************************/
int get_charging_info(void)
{
	if (!the_di)
		return -ENODEV;
	
	return the_di->charge_status;
}
EXPORT_SYMBOL(get_charging_info);

 /*****************************************************************************
	get_battery_capacity_info
*****************************************************************************/
int get_battery_capacity_info(void)
{
	if (!the_di)
		return -ENODEV;
	
	return  the_di->capacity;
}
EXPORT_SYMBOL(get_battery_capacity_info);

 /*****************************************************************************
	print_sub_state
*****************************************************************************/
static void print_sub_state(void)
{
	printk("Status Register A = %02X\n", the_sub->chg_ic_status_reg.a);
	printk("Status Register B = %02X\n", the_sub->chg_ic_status_reg.b);
	printk("Status Register C = %02X\n", the_sub->chg_ic_status_reg.c);
	printk("Status Register D = %02X\n", the_sub->chg_ic_status_reg.d);
	printk("Status Register E = %02X\n", the_sub->chg_ic_status_reg.e);
	printk("Status Register F = %02X\n", the_sub->chg_ic_status_reg.f);
	printk("Status Register G = %02X\n", the_sub->chg_ic_status_reg.g);
	printk("Status Register H = %02X\n", the_sub->chg_ic_status_reg.h);
	printk("Status Register Ext A = %02X\n", the_sub->chg_ic_status_reg.ext_a);
	printk("Status Register Ext B = %02X\n", the_sub->chg_ic_status_reg.ext_b);
	printk("Status Register Ext C = %02X\n", the_sub->chg_ic_status_reg.ext_c);
}

 /*****************************************************************************
	batt_voltage_diff
*****************************************************************************/
static int batt_voltage_diff(void)
{
	if( the_sub->pre_chg_ic_status_reg.ext_b
		!= the_sub->chg_ic_status_reg.ext_b )
	{
		return 1;
	}else{
		return 0;
	}	
}


 /*****************************************************************************
	set_charge_state
*****************************************************************************/
static int set_charge_state(void)
{
	int pre_state = the_di->charge_status;
	
	if(the_sub->chg_ic_status_reg.ext_c & CHG_LED_BIT)
	{
		the_di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
	}
	else if(the_sub->chg_ic_status_reg.ext_c & USB_SUSP_BIT)
	{
		the_di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	else if( (the_sub->chg_ic_status_reg.ext_a & CHG_SETTING_BITS)
			&& (the_sub->chg_ic_status_reg.e & CHG_TERMINATION_CURRENT_BIT) )
	{
		the_di->charge_status = POWER_SUPPLY_STATUS_FULL;
	}
	else if( !(the_sub->chg_ic_status_reg.ext_c & USB_DET_BIT) )
	{
		//USB_DET_BIT = Low : VBUS exist.
		the_di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	else
	{
		the_di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	
	//if( pre_state == the_di->charge_status ){
	//	return 0;
	//}
	if(the_di && (pre_state != the_di->charge_status)){
		wake_lock_timeout(&update_battery_capacity, 2 * HZ);
		cancel_delayed_work(&the_di->send_chraging_state_work);
		schedule_delayed_work(&the_di->send_chraging_state_work, msecs_to_jiffies(300));
	}
	
	return 0;
}

 /*****************************************************************************
	set_health
*****************************************************************************/
static int set_health(void)
{
	int pre_health = the_di->bat_health;
	int ret = 0;
	
	if((the_sub->chg_ic_status_reg.b & USBIN_OV_BIT)
		|| (the_sub->chg_ic_status_reg.ext_c & OVP_OK_BIT))
	{
		the_di->bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	else if((the_sub->chg_ic_status_reg.d & TEMP_TOO_COLD_BIT)
			|| (the_sub->chg_ic_status_reg.d & TEMP_TOO_HOT_BIT))
	{
		the_di->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	}
	else if( the_sub->chg_ic_status_reg.e & CHG_ERROR_BIT )
	{
		the_di->bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	else if( the_sub->chg_ic_status_reg.b & INTERNAL_TEMP_LIMIT_BIT )
	{
		the_di->bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	else if((the_sub->chg_ic_status_reg.f & BATT_LOW_BIT)
	        && !(the_sub->chg_ic_status_reg.b & USBIN_OV_BIT)
			&& (the_sub->chg_ic_status_reg.e & SAFETY_TIMER_BITS)
			&& !((the_sub->chg_ic_status_reg.e & HOLDOFF_TIMER_BIT) == HOLDOFF_TIMER_BIT))
	{
		the_di->bat_health = POWER_SUPPLY_HEALTH_DEAD;
	}
	else
	{
		the_di->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	
	if( pre_health != the_di->bat_health){
		ret = 1;
	}
	
	return ret;
	
}

 /*****************************************************************************
	get_temperature
*****************************************************************************/
int get_temperature(void)
{

	regulator_enable(disp_regu);
	
	the_di->temp_C = regulator_get_voltage(temp_regu);
	if ( the_di->temp_C > 128 )
		the_di->temp_C -= 256;
	
	regulator_disable(disp_regu);

	return the_di->temp_C;
}

 /*****************************************************************************
	calculate_battery_capacity
*****************************************************************************/
static int calculate_battery_capacity(int voltage_uV)
{
	int cap;
	int *battery_voltage_table;
	int size = ARRAY_SIZE(battery_voltage_table_wifi);
	int temp;
	
	temp = get_temperature();
	
	if( (temp > 5) && (temp <= 128 ) ){
		if(set_model == WIFI_MODEL){
			battery_voltage_table = &battery_voltage_table_wifi[0];
		}else{
			battery_voltage_table = &battery_voltage_table_wan[0];
		}
	}else{
		if(set_model == WIFI_MODEL){
			battery_voltage_table = &battery_voltage_table_cold_wifi[0];
		}else{
			battery_voltage_table = &battery_voltage_table_cold_wan[0];
		}
	}
	
	for (cap = 0; cap < size; cap++) {
		if ((voltage_uV/1000) <= *(battery_voltage_table + cap)){
			return (cap * 5);
		}
	}
	
	return 100;
}

 /*****************************************************************************
	set_capacity
*****************************************************************************/
static int set_capacity(void)
{
	int ret = 0;
	int new_batt_capacity;
	
	if( the_sub->chg_ic_status_reg.ext_c & LOW_BATT_BIT ){
		printk("Detect HW No Battery!!!\n");
		if (!wake_lock_active(&no_battery)){
			wake_lock(&no_battery);
		}
		the_di->capacity = 0;
		return 1;
	}
		
	if( batt_voltage_diff() )
	{
		the_di->voltage_uV = ((the_sub->chg_ic_status_reg.ext_b * 10) + 2000 ) * 1000;
		
		new_batt_capacity = calculate_battery_capacity(the_di->voltage_uV);
		
		//printk("Voltage  : %duV\n", the_di->voltage_uV);
		//printk("Capacity : %d%\n", new_batt_capacity);
		
		if( the_di->capacity != new_batt_capacity)
		{
			if( the_di->capacity > new_batt_capacity)
			{
				the_di->capacity = new_batt_capacity;
				ret = 1;
			}
			else if( the_sub->chg_ic_status_reg.ext_c & CHG_LED_BIT )
			{
				the_di->capacity = new_batt_capacity;
				ret = 1;
			}
			else if ( (the_sub->chg_ic_status_reg.ext_c & CHG_LED_BIT)
						!= (the_sub->pre_chg_ic_status_reg.ext_c & CHG_LED_BIT) )
			{
				the_di->capacity = new_batt_capacity;
				ret = 1;
			}
			
			if( the_di->capacity == 0){
				if (!wake_lock_active(&no_battery)){
					wake_lock(&no_battery);
				}
			}else{
				if (wake_lock_active(&no_battery)){
					wake_unlock(&no_battery);
				}
			}			
		}
	}
	
	return ret;
}

 /*****************************************************************************
	sub_cpu_check_battery_state_work
*****************************************************************************/
//static void sub_cpu_check_battery_state_work(struct work_struct *work)
void sub_cpu_check_battery_state_work(void)
{

	int ret = 0;
	
#if 0
	print_sub_state();
#endif
	
	if(!the_di){
		return;
	}
	
	send_vbus_state_change_info();
	
	if( set_capacity() ){
		ret = 1;
	}

	if(set_charge_state()){
		ret = 1;
	}
	
	if( set_health() ){
		ret = 1;
	}
	
	if(ret){
		wake_lock_timeout(&update_battery_capacity, 1 * HZ);
		power_supply_changed(&the_di->bat);
	}
	
}

EXPORT_SYMBOL(sub_cpu_check_battery_state_work);

 /*****************************************************************************
	send_chraging_state_work
*****************************************************************************/
static void send_chraging_state_work(struct work_struct *work)
{

	the_di->android_charge_status = the_di->charge_status;
	power_supply_changed(&the_di->bat);
	
}

 /*****************************************************************************
	sub_cpu_bci_battery_props
*****************************************************************************/
static enum power_supply_property sub_cpu_bci_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

 /*****************************************************************************
	sub_cpu_usb_props
*****************************************************************************/
static enum power_supply_property sub_cpu_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

 /*****************************************************************************
	sub_cpu_ac_props
*****************************************************************************/
static enum power_supply_property sub_cpu_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

#define to_sub_cpu_bci_device_info(x) container_of((x), \
			struct sub_cpu_bci_device_info, bat);

#define to_sub_cpu_ac_device_info(x) container_of((x), \
		struct sub_cpu_bci_device_info, ac);
		
#define to_sub_cpu_usb_device_info(x) container_of((x), \
		struct sub_cpu_bci_device_info, usb);

 /*****************************************************************************
	sub_cpu_ac_get_property
*****************************************************************************/
static int sub_cpu_ac_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sub_cpu_bci_device_info *di = to_sub_cpu_ac_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->ac_online;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

 /*****************************************************************************
	sub_cpu_usb_get_property
*****************************************************************************/
static int sub_cpu_usb_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sub_cpu_bci_device_info *di = to_sub_cpu_usb_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->usb_online;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

 /*****************************************************************************
	sub_cpu_bci_battery_get_property
*****************************************************************************/
static int sub_cpu_bci_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sub_cpu_bci_device_info *di;

	di = to_sub_cpu_bci_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		//val->intval = di->charge_status;
		val->intval = di->android_charge_status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = di->voltage_uV;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = di->temp_C;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->battery_online;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = di->bat_health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = di->capacity;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

 /*****************************************************************************
	set_battery_capacity
*****************************************************************************/
static ssize_t set_battery_capacity(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long val;
	int status = count;
	struct sub_cpu_bci_device_info *di = dev_get_drvdata(dev);

	if ((strict_strtol(buf, 10, &val) < 0) || (val > 101))
		return -EINVAL;
	
	di->capacity = val;
	//di->old_capacity = di->capacity;
	wake_lock_timeout(&update_battery_capacity, 1 * HZ);
	if(the_di)
		power_supply_changed(&di->bat);
	
	return status;
}

 /*****************************************************************************
	show_battery_capacity
*****************************************************************************/
static ssize_t show_battery_capacity(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;
	struct sub_cpu_bci_device_info *di = dev_get_drvdata(dev);

	val = di->capacity;
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}



 /*****************************************************************************
	set_charging
*****************************************************************************/
static ssize_t set_charging(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int status = count;
	//struct sub_cpu_bci_device_info *di = dev_get_drvdata(dev);

	if (strncmp(buf, "startac", 7) == 0) {
		sub_cpu_set_usb_in_current(1000);
	} else if (strncmp(buf, "startusb", 8) == 0) {
		sub_cpu_set_usb_in_current(500);
	} else if (strncmp(buf, "stop" , 4) == 0){
		sub_cpu_set_usb_in_current(0);
	}
	else
		return -EINVAL;

	return status;
}



static DEVICE_ATTR(battery_capacity, S_IWUSR | S_IRUGO, show_battery_capacity, set_battery_capacity);
static DEVICE_ATTR(charging, S_IWUSR | S_IRUGO, NULL, set_charging);

static struct attribute *sub_cpu_bci_attributes[] = {
	&dev_attr_battery_capacity.attr,
	&dev_attr_charging.attr,
	NULL,
};

static const struct attribute_group sub_cpu_bci_attr_group = {
	.attrs = sub_cpu_bci_attributes,
};

static char *sub_cpu_bci_supplied_to[] = {
	"sub_cpu_battery",
};


 /*****************************************************************************
	sub_cpu_bci_probe
*****************************************************************************/
static int __devinit sub_cpu_bci_probe(struct platform_device *pdev)
{
	struct sub_cpu_bci_platform_data *pdata = pdev->dev.platform_data;
	struct sub_cpu_bci_device_info *di;
	int ret;
	//u8 controller_stat = 0, reg;
		
	the_sub = confirm_the_sub();
	
	if( gpio_get_value(SET_ID) ){
		//WiFi Model
		set_model = WIFI_MODEL;
	}else{
		//WAN (3G) Model
		set_model = WAN_MODEL;
	}
	
	charge_work_queue = create_workqueue("usb_charge_workqueue");
	if (charge_work_queue == NULL) {
		printk(KERN_ERR "Coulndn't create usb charge work queue\n");
		return -ENOMEM;
	}
	
	wake_lock_init(&update_battery_capacity, WAKE_LOCK_SUSPEND, "update_battery_capacity");
	wake_lock_init(&no_battery, WAKE_LOCK_SUSPEND, "no_battery");

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		ret = -EINVAL;
		goto err_pdata;
	}

	
	di->dev = &pdev->dev;
	
	di->board_control_vbus_power = pdata->board_control_vbus_power;

	di->bat.name = "sub_cpu_battery";
	di->bat.supplied_to = sub_cpu_bci_supplied_to;
	di->bat.num_supplicants = ARRAY_SIZE(sub_cpu_bci_supplied_to);
	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties = sub_cpu_bci_battery_props;
	di->bat.num_properties = ARRAY_SIZE(sub_cpu_bci_battery_props);
	di->bat.get_property = sub_cpu_bci_battery_get_property;
	
	//di->bat_health = POWER_SUPPLY_HEALTH_GOOD;

	di->usb.name = "sub_cpu_usb";
	di->usb.type = POWER_SUPPLY_TYPE_USB;
	di->usb.properties = sub_cpu_usb_props;
	di->usb.num_properties = ARRAY_SIZE(sub_cpu_usb_props);
	di->usb.get_property = sub_cpu_usb_get_property;

	di->ac.name = "sub_cpu_ac";
	di->ac.type = POWER_SUPPLY_TYPE_MAINS;
	di->ac.properties = sub_cpu_ac_props;
	di->ac.num_properties = ARRAY_SIZE(sub_cpu_ac_props);
	di->ac.get_property = sub_cpu_ac_get_property;
	
	di->detect_charger_complete_flg = 0;
	
	//INIT_DELAYED_WORK_DEFERRABLE(&di->sub_cpu_check_battery_state_work,
	//							sub_cpu_check_battery_state_work);
	
	INIT_DELAYED_WORK_DEFERRABLE(&di->send_chraging_state_work,
								send_chraging_state_work);
	
	platform_set_drvdata(pdev, di);
	
	ret = power_supply_register(&pdev->dev, &di->bat);
	if (ret) {
		printk("failed to register main battery\n");
		goto batt_failed;
	}

	ret = power_supply_register(&pdev->dev, &di->usb);
	if (ret) {
		printk("failed to register main battery\n");
		goto usb_failed;
	}

	ret = power_supply_register(&pdev->dev, &di->ac);
	if (ret) {
		printk("failed to register main battery\n");
		goto ac_failed;
	}	
	
	the_di = di;	

	the_di->battery_online = 1;
	
	//USB_DET : Low = VBUS exist 
	if( the_sub->chg_ic_status_reg.ext_c & USB_DET_BIT ){
		the_di->charger_source = 0;
		the_di->ac_online = 0;
		the_di->usb_online = 0;
	}else{
		the_di->charger_source = POWER_SUPPLY_TYPE_MAINS;
		the_di->ac_online = 1;
		the_di->usb_online = 0;
	}
	
	disp_regu = regulator_get( NULL, "DISPLAY" );
	temp_regu = regulator_get(NULL, "PMIC_TEMP");
	
	set_health();
	set_charge_state();
	send_vbus_state();
	the_di->voltage_uV = ((the_sub->chg_ic_status_reg.ext_b * 10) + 2000 ) * 1000;
	the_di->capacity = calculate_battery_capacity(the_di->voltage_uV);
	
	ret = sysfs_create_group(&pdev->dev.kobj, &sub_cpu_bci_attr_group);
	if (ret)
		dev_dbg(&pdev->dev, "could not create sysfs files\n");

	return 0;
	
ac_failed:
	power_supply_unregister(&di->ac);
usb_failed:
	power_supply_unregister(&di->usb);	
batt_failed:
	power_supply_unregister(&di->bat);
err_pdata:
	kfree(di);

	return ret;
}


 /*****************************************************************************
	confirm_the_di
*****************************************************************************/
struct sub_cpu_bci_device_info *confirm_the_di(void)
{
	return the_di;
}
EXPORT_SYMBOL(confirm_the_di);


 /*****************************************************************************
	sub_cpu_bci_remove
*****************************************************************************/
static int __devexit sub_cpu_bci_remove(struct platform_device *pdev)
{
	struct sub_cpu_bci_device_info *di = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &sub_cpu_bci_attr_group);

	//cancel_delayed_work(&di->sub_cpu_check_battery_state_work);
	
	cancel_delayed_work(&di->send_chraging_state_work);

	flush_scheduled_work();
	
	flush_workqueue(charge_work_queue);
	destroy_workqueue(charge_work_queue);
	
	regulator_put(disp_regu);
	regulator_put(temp_regu);
	
	power_supply_unregister(&di->bat);
	power_supply_unregister(&di->usb);
	power_supply_unregister(&di->ac);
	platform_set_drvdata(pdev, NULL);
	kfree(di);

	wake_lock_destroy(&update_battery_capacity);
	wake_lock_destroy(&no_battery);
	
	return 0;
}

#ifdef CONFIG_PM
 /*****************************************************************************
	sub_cpu_bci_suspend
*****************************************************************************/
static int sub_cpu_bci_suspend(struct platform_device *pdev,
	pm_message_t state)
{

	flush_scheduled_work();

	return 0;
}

#if 0
static int sub_cpu_bci_resume(struct platform_device *pdev)
{
	return 0;
}
#endif 
#else
#define sub_cpu_bci_suspend	NULL
#define sub_cpu_bci_resume	NULL
#endif /* CONFIG_PM */

static struct platform_driver sub_cpu_bci_driver = {
	.probe		= sub_cpu_bci_probe,
	.remove		= __devexit_p(sub_cpu_bci_remove),
	.suspend	= sub_cpu_bci_suspend,
	//.resume		= sub_cpu_bci_resume,
	.driver		= {
		.name	= "sub_cpu_bci",
	},
};

static int __init sub_cpu_init(void)
{
#ifdef CONFIG_REGULATOR_TWL4030 
	ldo5 = regulator_get(NULL, "LDO5");
	if(IS_ERR(ldo5)){
		printk(KERN_ERR"error: regulator_get LDO5\n"); 
		ldo5 = NULL;  
	}
	
	ldousb = regulator_get(NULL, "LDOUSB");
	if(IS_ERR(ldousb))
	{
		printk(KERN_ERR"error: regulator_get LDOUSB\n"); 
		ldousb = NULL; 
	}
	#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	ldo8 = regulator_get(NULL, "LDO8");
	if(IS_ERR(ldo8)){
		printk(KERN_ERR"error: regulator_get LDO8\n"); 
		ldo8 = NULL;
	}
	
	ldo9 = regulator_get(NULL, "LDO9");
	if(IS_ERR(ldo9))
	{
		printk(KERN_ERR"error: regulator_get LDO9\n");
		ldo9 = NULL; 
	}
#endif
	
	return platform_driver_register(&sub_cpu_bci_driver);
}
//module_init(sub_cpu_init);
late_initcall(sub_cpu_init);

static void __exit sub_cpu_exit(void)
{
#ifdef CONFIG_REGULATOR_TWL4030 
	if(ldo5){
		regulator_put(ldo5);	
	}
	
	if(ldousb) {
		regulator_put(ldousb); 
	}
#endif
	
#ifdef CONFIG_REGULATOR_WM831X 
	if(ldo8){
		regulator_put(ldo8);	
	}
	
	if(ldo9) {
		regulator_put(ldo9); 
	}
#endif
	
	platform_driver_unregister(&sub_cpu_bci_driver);
}
module_exit(sub_cpu_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sub_cpu_bci");
MODULE_AUTHOR("xxx");

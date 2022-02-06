/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * Includes
 */
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/mach-types.h>
#include <linux/pmic_battery.h>
#include <linux/pmic_adc.h>
#include <linux/pmic_status.h>
#include <linux/gpio.h>
#include <mach/iomux-mx6q.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/regulator/consumer.h>

#include "../../arch/arm/mach-mx6/ntx_hwconfig.h"
#include "ntx-misc.h"

#include "../../arch/arm/mach-mx6/board-mx6sl_ntx.h"

#define GDEBUG 1
#include <linux/gallen_dbg.h>

extern volatile NTX_HWCONFIG *gptHWCFG;
extern int gSleep_Mode_Suspend;

struct ntx_misc_platform_data *ntx_misc;

#define NTX_IS_CHARGING		(gpio_get_value (ntx_misc->chg_gpio)?0:1)
#define NTX_ACIN_PG 		(gpio_get_value (ntx_misc->acin_gpio)?0:1)

struct i2c_client *g_up_i2c_client;
static unsigned short gw_uP_version;
extern int g_wakeup_by_alarm;

static unsigned long gdwLastRTCReadTick;
static volatile int giIsSuspending=0;
static struct rtc_time gtLastRTCtm;
static struct regulator *g_fl_regulator;

void msp430_fl_enable (int isEnable)
{
	static int s_is_enabled;
	if (g_fl_regulator && (s_is_enabled != isEnable)) {
		s_is_enabled = isEnable;
		printk ("[%s-%d] regulator %s\n",__func__,__LINE__,(isEnable)?"on":"off");
		if (isEnable) {
  			regulator_enable (g_fl_regulator);
  			msleep (200);
  		}
		else
  			regulator_disable (g_fl_regulator);
	}
}

int up_read_reg(unsigned char reg)
{
	int iChk;
	unsigned char buffer[10];

	int iSkipI2CTransfer = 0;

	if(!g_up_i2c_client) {
		printk("%s():skipped because of up not ready !\n",__FUNCTION__);
		iSkipI2CTransfer = 1;
	}

	if(giIsSuspending) {
		printk("%s():skipped read reg0x%02X because of suspending !\n",
				__FUNCTION__,reg);
		iSkipI2CTransfer = 1;
	}

	if(iSkipI2CTransfer) {
		return -1;
	}

	{
		struct i2c_msg msg[] = 
		{
			{.addr = g_up_i2c_client->addr, .flags = 0, .len = 1, .buf = &reg,}, 
			{.addr = g_up_i2c_client->addr, .flags = I2C_M_RD, .len = 2, .buf = buffer,},
		};
		iChk = i2c_transfer(g_up_i2c_client->adapter, msg, 2);
		if(0 > iChk) {
			printk ("[%s-%d] i2c_transfer failed (%d)...\n", __func__, __LINE__,iChk);
			return -1;
		}
	}

	return ((buffer[0]<<8) | buffer[1]);
}

int up_write_reg(unsigned char reg, int value)
{
	int iRet;
	unsigned char buffer[10];

	int iSkipI2CTransfer = 0;

	if(!g_up_i2c_client) {
		printk("%s():skipped because of up not ready !\n",__FUNCTION__);
		iSkipI2CTransfer = 1;
	}

	if(giIsSuspending) {
		printk("%s():skipped write reg0x%02X<==%x because of suspending !\n",
				__FUNCTION__,reg,value);
		iSkipI2CTransfer = 1;
	}

	if(iSkipI2CTransfer) {
		return -EAGAIN;
	}

	{
		struct i2c_msg msg[] = 
		{
			{.addr = g_up_i2c_client->addr, .flags = 0, .len = 3, .buf = buffer,}, 
		};
		buffer[0] = reg;
		buffer[1] = value >> 8;
		buffer[2] = value & 0xFF;
		iRet = i2c_transfer(g_up_i2c_client->adapter, msg, 1);

#if 0
		if(0xC0==buffer[0]) 
		{
			printk("%s():send cmd to msp430 0x%02X,0x%02X,0x%02X\n",
				__FUNCTION__,buffer[0],buffer[1],buffer[2]);
		}
#endif
	}
	return iRet;
}


int up_get_time(struct rtc_time *tm)
{
  unsigned int tmp;
	int iSkip_Read_RTC_from_I2C = 0;

#if 0
	if(time_before(jiffies,gdwLastRTCReadTick)) {
		iSkip_Read_RTC_from_I2C = 1;
	}
#endif

	if(giIsSuspending) {
		iSkip_Read_RTC_from_I2C = 1;
	}

	if(iSkip_Read_RTC_from_I2C) {
		printk("%s : skipped frequently RTC read from I2C !\n",__FUNCTION__);
		tm->tm_year = gtLastRTCtm.tm_year;
		tm->tm_mon = gtLastRTCtm.tm_mon;
		tm->tm_mday = gtLastRTCtm.tm_mday;
		tm->tm_hour = gtLastRTCtm.tm_hour;
		tm->tm_min = gtLastRTCtm.tm_min;
		tm->tm_sec = gtLastRTCtm.tm_sec;
		return 0;
	}

	gdwLastRTCReadTick=jiffies+(HZ/2);
   	tmp = up_read_reg (0x20);
	tm->tm_year = ((tmp >> 8) & 0x0FF)+100;
	tm->tm_mon = (tmp & 0x0FF)-1;
    tmp = up_read_reg (0x21);
	tm->tm_mday = (tmp >> 8) & 0x0FF;
	tm->tm_hour = tmp & 0x0FF;
	tmp = up_read_reg (0x23);
	tm->tm_min = (tmp >> 8) & 0x0FF;
	tm->tm_sec = tmp & 0x0FF;

	gtLastRTCtm.tm_year = tm->tm_year;
	gtLastRTCtm.tm_mon = tm->tm_mon;
	gtLastRTCtm.tm_mday = tm->tm_mday;
	gtLastRTCtm.tm_hour = tm->tm_hour;
	gtLastRTCtm.tm_min = tm->tm_min;
	gtLastRTCtm.tm_sec = tm->tm_sec;

	return 0;
}
EXPORT_SYMBOL_GPL(up_get_time);

int up_set_time(struct rtc_time *tm)
{
	if(giIsSuspending) {
		return -1;
	}

	up_write_reg (0x10, ((tm->tm_year-100)<<8));
	up_write_reg (0x11, ((tm->tm_mon+1)<<8));
	up_write_reg (0x12, (tm->tm_mday<<8));
	up_write_reg (0x13, (tm->tm_hour<<8));
	up_write_reg (0x14, (tm->tm_min<<8));
	up_write_reg (0x15, (tm->tm_sec<<8));
	
	return 0;
}
EXPORT_SYMBOL_GPL(up_set_time);

static unsigned long gAlarmTime;
static unsigned long g_alarm_enabled;

int up_get_alarm(struct rtc_time *tm)
{
	rtc_time_to_tm(gAlarmTime,tm);
	return 0;
}
EXPORT_SYMBOL_GPL(up_get_alarm);

int up_set_alarm(struct rtc_time *tm)
{
	struct rtc_time now_tm;
	unsigned long now, time;

	if (!tm) {
		printk ("[%s-%d] Disable alarm.\n", __func__, __LINE__);
		up_write_reg (0x1B, 0);
		up_write_reg (0x1C, 0);
		return 0;
	}
	
	up_get_time (&now_tm);
	rtc_tm_to_time(&now_tm, &now);
	rtc_tm_to_time(tm, &time);
	gAlarmTime=time;

	if(time > now) {
		int interval = time-now;
		printk ("[%s-%d] alarm %d\n",__func__,__LINE__,interval);
		up_write_reg (0x1B, (interval&0xFF00));
		up_write_reg (0x1C, ((interval<<8)& 0xFF00));
	}
	else {
		int tmp = up_read_reg (0x60);
		if (tmp & 0x8000) {
			printk ("[%s-%d] =================> Micro P MSP430 alarm triggered <===================\n", __func__, __LINE__);
			g_wakeup_by_alarm = 1;
		}
		up_write_reg (0x1B, 0);
		up_write_reg (0x1C, 0);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(up_set_alarm);

unsigned short msp430_deviceid(void)
{
	static unsigned short gw_uP_version;

	if(0==gw_uP_version) {
		gw_uP_version = up_read_reg(0);
	}

	return gw_uP_version;
}

void msp430_auto_power(int minutes)
{
}

void msp430_powerkeep(int n)
{
}

void msp430_power_off(void)
{
   	while (1) {
		printk("Kernel--- Power Down ---\n");
		up_write_reg (0x50, 0x0100);
      	msleep(1400);
	}
}

void msp430_pm_restart(void)
{
   	while (1) {
		printk("Kernel--- restart ---\n");
		up_write_reg (0x90, 0xff00);

      	msleep(1400);
	}
}

struct ntx_up_dev_info {
	struct device *dev;
	int battery_status;
	int charger_online;
	int full_counter;
	int voltage_uV;
	unsigned short voltage_raw;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
	struct power_supply bat;
	struct power_supply charger;
};

static enum power_supply_property ntx_up_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE, /* External power source */
};

static enum power_supply_property ntx_up_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
};

enum chg_state {
	CHG_POWER_OFF,
	CHG_RESTART,
	CHG_CHARGING,
	CHG_DISCHARGING_WITH_CHARGER,
	CHG_DISCHARGING,
};

int gIsMSP430IntTriggered;
unsigned long gLastBatTick, gUSB_Change_Tick;
int gLastBatValue;

static int g_dc_charger_connect=0;
void set_pmic_dc_charger_state(int dccharger)
{
	g_dc_charger_connect=dccharger;
}
EXPORT_SYMBOL(set_pmic_dc_charger_state);

#define TICS_TO_CHK_ACIN_AFTER_BOOT		500
static unsigned long gdwTheTickToChkACINPlug;
static int g_acin_pg_debounce;
static int g_battery_full_flag=0;
struct ntx_up_dev_info *g_ntx_bat_di;
struct workqueue_struct *chg_wq;
struct delayed_work chg_work;
static int state=CHG_RESTART;

typedef void (*usb_insert_handler) (char inserted);

static void acin_pg_work_func(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_acin_pg,acin_pg_work_func);
static int ntx_up_battery_vol (void);

extern usb_insert_handler mxc_misc_report_usb;
extern void ntx_charger_online_event_callback(void);

extern int gIsCustomerUi;
extern int ntx_charge_status (void);

int ntx_up_charge_status (void)
{
#if 0
	if (gpio_get_value (ntx_misc->acin_gpio))
		return 0;	
	else 
	   	return ((up_read_reg (0x60)&0x08)?1:3);
#else
	return ntx_charge_status();
#endif
}

static void chg_thread(struct work_struct *work)
{
	int charger;

	charger = ntx_up_charge_status ();

	if (charger != g_ntx_bat_di->charger_online) {
		g_ntx_bat_di->charger_online = charger;
		dev_info(g_ntx_bat_di->charger.dev, "charger status: %s\n",
			charger ? "online" : "offline");
		power_supply_changed(&g_ntx_bat_di->charger);

		cancel_delayed_work(&g_ntx_bat_di->monitor_work);
		queue_delayed_work(g_ntx_bat_di->monitor_wqueue,
			&g_ntx_bat_di->monitor_work, HZ / 10);
	}
	
	switch(state)
	{
	case CHG_RESTART:
		if(charger){
			state = CHG_CHARGING;
		}
		else
			state = CHG_DISCHARGING;
		queue_delayed_work(chg_wq, &chg_work, HZ*1);
		break;

	case CHG_POWER_OFF:
		pr_notice("Battery level < 3.5V!\n");
		pr_notice("After power off, PMIC will charge up battery.\n");
//		orderly_poweroff(1);
		break;

	case CHG_CHARGING:
		if (charger & 1) {
			if (charger & 2) {
				g_battery_full_flag=0;
			}
			else {
				g_battery_full_flag=1;
				state = CHG_DISCHARGING_WITH_CHARGER;
			}
		}
		else
		{
			g_battery_full_flag=0;
			state = CHG_DISCHARGING;
		}
		queue_delayed_work(chg_wq, &chg_work, HZ*5);
		break;

	case CHG_DISCHARGING:
		if(charger) {
			state = CHG_RESTART;
			queue_delayed_work(chg_wq, &chg_work, HZ/2);
		}
		else
			queue_delayed_work(chg_wq, &chg_work, HZ*10);
		break;

	case CHG_DISCHARGING_WITH_CHARGER:
		if(ntx_up_battery_vol()<4000000)
			state = CHG_RESTART;
		if(!charger)
			state = CHG_DISCHARGING;
		queue_delayed_work(chg_wq, &chg_work, HZ*2);
		break;
	}
}

int msp430_battery(void)
{
	int battValue, temp;
	gIsMSP430IntTriggered = 0;
	if (gUSB_Change_Tick) {
		if (time_after (jiffies, (gUSB_Change_Tick+500))) {
			gUSB_Change_Tick = 0;
			gLastBatValue = 0;
		}
	}
	
	if (gIsMSP430IntTriggered || !gLastBatValue || ((0 == gUSB_Change_Tick) && (time_after (jiffies, gLastBatTick)))) {
		battValue = up_read_reg (0x41);
		if (battValue>0) {
			gLastBatTick = jiffies+200;
			if (gpio_get_value (ntx_misc->acin_gpio)) {// not charging
				temp = up_read_reg (0x60);
				if (-1 != temp ) {
					if (0x8000 & temp) {
						printk ("[%s-%d] =================> Micro P MSP430 alarm triggered <===================\n", __func__, __LINE__);
						g_wakeup_by_alarm = 1;
						up_set_alarm (0);
					}
					if (0x01 & temp) {
						printk ("[%s-%d] =================> Micro P MSP430 Critical_Battery_Low <===================\n", __func__, __LINE__);
						return 0;
					}
					else if (!gLastBatValue) 
						gLastBatValue = battValue;
					else if (gLastBatValue > battValue)
						gLastBatValue = battValue;
					else
						battValue = gLastBatValue;
				}
			}
			else {
				if (gLastBatValue < battValue)
					gLastBatValue = battValue;
				else
					battValue = gLastBatValue;
			}
		}
		else {
			printk ("[%s-%d] MSP430 read failed\n", __func__, __LINE__);
			battValue = 0;
		}
	}
	else 
		battValue = gLastBatValue;
	
	return battValue;
}

static int ntx_up_battery_vol (void)
{
	int i, battValue, result;
	const unsigned short battGasgauge[] = {
	//	3.0V, 3.1V, 3.2V, 3.3V, 3.4V, 3.5V, 3.6V, 3.7V, 3.8V, 3.9V, 4.0V, 4.1V, 4.2V,
//		 743,  767,  791,  812,  835,  860,  885,  909,  935,  960,  985, 1010, 1023,
		 767,  791,  812,  833,  852,  877,  903,  928,  950,  979,  993, 1019, 1023,
	};
	
	if (NTX_ACIN_PG && !(NTX_IS_CHARGING))
		return 4200000;
	
	battValue = msp430_battery ();
	// transfer to uV to pmic interface.
	for (i=0; i< sizeof (battGasgauge); i++) {                 
		if (battValue <= battGasgauge[i]) {
			if (i && (battValue != battGasgauge[i])) {
				result = 3000000+ (i-1)*100000;
				result += ((battValue-battGasgauge[i-1]) * 100000 / (battGasgauge[i]-battGasgauge[i-1]));
			}
			else
				result = 3000000+ i*100000;
			break;
		}
	}
	return result;
}

static int ntx_up_charger_update_status(struct ntx_up_dev_info *di)
{
	int ret = 0;
	int online;

	online = ntx_up_charge_status ();
	if (online != di->charger_online) {
		di->charger_online = online;

		dev_info(di->charger.dev, "charger status: %s\n",
			online ? "online" : "offline");
		power_supply_changed(&di->charger);

		cancel_delayed_work(&di->monitor_work);
		queue_delayed_work(di->monitor_wqueue,
			&di->monitor_work, HZ / 10);
	}

	return ret;
}

static void ntx_up_battery_update_status(struct ntx_up_dev_info *di)
{
	int old_battery_status = di->battery_status;

	if (di->battery_status == POWER_SUPPLY_STATUS_UNKNOWN)
		di->full_counter = 0;

	if (di->charger_online & 0x01) {
		if (di->charger_online & 2)
			di->battery_status =
				POWER_SUPPLY_STATUS_CHARGING;
		else
			di->battery_status =
				POWER_SUPPLY_STATUS_NOT_CHARGING;

		if (di->battery_status == POWER_SUPPLY_STATUS_NOT_CHARGING)
			di->full_counter++;
		else
			di->full_counter = 0;
	} else {
		di->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->full_counter = 0;
	}

	dev_dbg(di->bat.dev, "bat status: %d\n",
		di->battery_status);

	if (old_battery_status != POWER_SUPPLY_STATUS_UNKNOWN) {
		int curVoltage = ntx_up_battery_vol();
		if((di->battery_status != old_battery_status) || (di->voltage_uV != curVoltage)) {
			di->voltage_uV = curVoltage;
			power_supply_changed(&di->bat);
		}
	}
}

static int ntx_up_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct ntx_up_dev_info *di =
		container_of((psy), struct ntx_up_dev_info, charger);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->charger_online;
		return 0;
		break;
	default:
		break;
	}
}

static int ntx_up_battery_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct ntx_up_dev_info *di =
		container_of((psy), struct ntx_up_dev_info, bat);
	int value;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->charger_online;
		return 0;
		
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if(g_battery_full_flag)
		{
			/* Hardcode a current value to cheat upper layer charge is full */
			val->intval = 50000;
		}
		else
		val->intval = 500000;
		return 0;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = 500000;
		return 0;

	case POWER_SUPPLY_PROP_STATUS: /* Charger status output */
		if (di->battery_status == POWER_SUPPLY_STATUS_UNKNOWN) {
			ntx_up_charger_update_status(di);
			ntx_up_battery_update_status(di);
		}
		val->intval = di->battery_status;
		return 0;
		
	case POWER_SUPPLY_PROP_HEALTH: /* Fault or OK */
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		return 0;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (POWER_SUPPLY_STATUS_NOT_CHARGING == g_ntx_bat_di->battery_status) {
			val->intval = 100;
			return 0;
		}
		value = ntx_up_battery_vol();
		if (4100000 <= value) {
			val->intval =  100;
		}
		else if (3400000 > value) {
			printk("%s : empty !! %d\n",__FUNCTION__,value);
			val->intval = 0;
		}
		else
			val->intval  = 100 - ((4100000 - value)/7000);
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = ntx_up_battery_vol();
		return 0;

	default:
		break;
	}
	return -EINVAL;
}

static ssize_t chg_wa_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (NTX_IS_CHARGING)
		return sprintf(buf, "Charger LED workaround timer is on\n");
	else
		return sprintf(buf, "Charger LED workaround timer is off\n");
}

static ssize_t chg_wa_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	if (strstr(buf, "1") != NULL) {
		printk(KERN_INFO "Turned on the timer\n");
	} else if (strstr(buf, "0") != NULL) {
		printk(KERN_INFO "The Charger workaround timer is off\n");
	}

	return size;
}

static DEVICE_ATTR(enable, 0644, chg_wa_enable_show, chg_wa_enable_store);

static void acin_pg_chk( void )
{
	extern void led_red(int isOn);
	extern int mxc_usb_plug_getstatus (void);


	if((36==gptHWCFG->m_val.bPCB||40==gptHWCFG->m_val.bPCB) && 0x03!=gptHWCFG->m_val.bUIConfig) {
		// E60Q32/E60Q5X control charging led if not MP/RD mode . 
		if(mxc_usb_plug_getstatus()) {
			led_red(1);
		}
		else {
			led_red(0);
		}
	}

	cancel_delayed_work(&work_acin_pg);
	if (!gpio_get_value (ntx_misc->acin_gpio)) {
		++g_acin_pg_debounce;
		if (10 == g_acin_pg_debounce) {
			ntx_up_charger_update_status(g_ntx_bat_di);
			gUSB_Change_Tick = jiffies;
		}
		else
			schedule_delayed_work(&work_acin_pg,1);
	}
	else {
		ntx_up_charger_update_status(g_ntx_bat_di);
		gUSB_Change_Tick = jiffies;
	}
}

static void acin_pg_work_func(struct work_struct *work)
{
	acin_pg_chk();
}

static irqreturn_t ntx_misc_dcin(int irq, void *_data)
{
	cancel_delayed_work(&work_acin_pg);
	g_acin_pg_debounce = 0;
	if(time_after(jiffies,gdwTheTickToChkACINPlug)) {
		schedule_delayed_work(&work_acin_pg,1);
	}
	else {
		schedule_delayed_work(&work_acin_pg,TICS_TO_CHK_ACIN_AFTER_BOOT);
	}
	return IRQ_HANDLED;
}

static irqreturn_t ntx_misc_chg(int irq, void *_data)
{
	return IRQ_HANDLED;
}

static int pmic_battery_remove(struct platform_device *pdev)
{
	struct ntx_up_dev_info *di = platform_get_drvdata(pdev);

	cancel_rearming_delayed_workqueue(di->monitor_wqueue, &di->monitor_work);
	cancel_rearming_delayed_workqueue(chg_wq, &chg_work);
	destroy_workqueue(di->monitor_wqueue);
	destroy_workqueue(chg_wq);
	power_supply_unregister(&di->charger);

	kfree(di);

	return 0;
}

static void pmic_battery_work(struct work_struct *work)
{
	struct ntx_up_dev_info *di = container_of(work,
						     struct ntx_up_dev_info,
						     monitor_work.work);
	const int interval = HZ * 20;

	dev_dbg(di->dev, "%s\n", __func__);

	ntx_up_battery_update_status(di);
	queue_delayed_work(di->monitor_wqueue, &di->monitor_work, interval);
}

static int g_acin_status;
static int pmic_battery_suspend(struct platform_device *pdev, pm_message_t state)
{
	g_acin_status = gpio_get_value (ntx_misc->acin_gpio);
	
	if (g_acin_status != !g_ntx_bat_di->charger_online) {
		printk ("[%s-%d] charger status not matched.\n",__func__,__LINE__);
		g_ntx_bat_di->charger_online = ntx_up_charge_status ();
	}
	return 0;
}

static int pmic_battery_resume(struct platform_device *pdev)
{
	if (g_acin_status != gpio_get_value (ntx_misc->acin_gpio)) {
		printk ("[%s-%d] charger status changed.\n",__func__,__LINE__);
		power_supply_changed(&g_ntx_bat_di->charger);
		cancel_delayed_work(&work_acin_pg);
		g_acin_pg_debounce = 0;
		schedule_delayed_work(&work_acin_pg,1);		
	}
	return 0;
}

static int pmic_battery_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct ntx_up_dev_info *di;
	int irq;

	if (!pdev->dev.platform_data)
		return -EBUSY;
		
	ntx_misc = pdev->dev.platform_data;
	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		retval = -ENOMEM;
		goto di_alloc_failed;
	}
	g_ntx_bat_di = di;

	platform_set_drvdata(pdev, di);

	INIT_DELAYED_WORK(&di->monitor_work, pmic_battery_work);
	di->monitor_wqueue = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!di->monitor_wqueue) {
		retval = -ESRCH;
		goto workqueue_failed;
	}
	queue_delayed_work(di->monitor_wqueue, &di->monitor_work, HZ * 10);

	INIT_DELAYED_WORK(&chg_work, chg_thread);
	chg_wq = create_singlethread_workqueue("mxc_chg");
	if (!chg_wq) {
		retval = -ESRCH;
		goto workqueue_failed;
	}
	queue_delayed_work(chg_wq, &chg_work, HZ);

	di->charger.name = "mc13892_charger";	// "ntx_up_charger"
	di->charger.type = POWER_SUPPLY_TYPE_MAINS;
	di->charger.properties = ntx_up_charger_props;
	di->charger.num_properties = ARRAY_SIZE(ntx_up_charger_props);
	di->charger.get_property = ntx_up_charger_get_property;
	retval = power_supply_register(&pdev->dev, &di->charger);
	if (retval) {
		dev_err(di->dev, "failed to register charger\n");
		goto charger_failed;
	}
	
	di->dev	= &pdev->dev;
	di->bat.name = "mc13892_bat";	// "ntx_up_battery"
	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties = ntx_up_battery_props;
	di->bat.num_properties = ARRAY_SIZE(ntx_up_battery_props);
	di->bat.get_property = ntx_up_battery_get_property;
	di->bat.use_for_apm = 1;
	retval = power_supply_register(&pdev->dev, &di->bat);
	if (retval) {
		dev_err(di->dev, "failed to register charger\n");
		goto charger_failed;
	}

	irq=gpio_to_irq(ntx_misc->acin_gpio);
	retval = request_threaded_irq(irq,
			ntx_misc_dcin, NULL, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"NTX_MISC DC IN", di);
	if (retval) {
		dev_err(di->dev, "Cannot request irq %d for DC (%d)\n",
				irq, retval);
		goto charger_failed;
	}
	//irq_set_irq_type(irq, IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING);
	enable_irq_wake(irq);

#if 0	// ignore chg# signal.
	irq=gpio_to_irq(ntx_misc->chg_gpio);
	retval = request_threaded_irq(irq,
			ntx_misc_chg, NULL, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"NTX_MISC CHG", di);
	if (retval) {
		dev_err(di->dev, "Cannot request irq %d for CHG (%d)\n",
				irq, retval);
		goto charger_failed;
	}
	enable_irq_wake(irq);
#endif

	goto success;

workqueue_failed:
	power_supply_unregister(&di->charger);
charger_failed:
	kfree(di);
di_alloc_failed:
success:
	dev_dbg(di->dev, "%s battery probed!\n", __func__);
	return retval;
}

static irqreturn_t msp430_int(int irq, void *dev_id)
{
	gIsMSP430IntTriggered = 1;
	printk ("[%s-%d] \n",__func__,__LINE__);
	return IRQ_HANDLED;
}

void msp430_homepad_enable(int iEnable)
{
	if (2==iEnable) {
		printk("%s(),re-calibrate home pad\n",__FUNCTION__);
		up_write_reg(0xC0,0x0300); // enabled touch pad and auto calibration .
	}
	else if(1==iEnable) {
		up_write_reg(0xC0,0x0100); // enabled touch pad and auto calibration .
	}
	else if(0==iEnable) {
		up_write_reg(0xC0,0x0000); // disabled touch pad .
	}
	else {
		// nothing to do .
	}
}

void msp430_homepad_sensitivity_set(unsigned char bVal)
{
	unsigned short wTemp = (unsigned short)bVal;

	wTemp = wTemp<<8 ;
	printk("%s(0x%x)\n",__FUNCTION__,bVal);
	up_write_reg(0xAC,wTemp); // disabled touch pad .
}

#define HOMELED_PWM_DELAY_LEVEL_MAX		0x05
#define HOMELED_PWM_DELAY_LEVEL_MIN		0x0
static const unsigned short gwHomeLed_pwm_delay_level_max = HOMELED_PWM_DELAY_LEVEL_MAX<<8;
static unsigned short gwHomeLed_pwm_delay_level = 0x0200;
#define HOMELED_GPIO_DELAY_LEVEL_MAX		0x20
#define HOMELED_GPIO_DELAY_LEVEL_MIN		0x0
static unsigned short gwHomeLed_gpio_delay_level_max = HOMELED_GPIO_DELAY_LEVEL_MAX<<8;
static unsigned short gwHomeLed_gpio_delay_level = 0x0900;

static unsigned short gwHomeLedType = (unsigned short)(-1);

static const char * gszHomeLedTypesA[] = {
	"gpio",
	"pwm",
};

int msp430_homeled_type_set_by_name(const char *I_pszHomeLedTypeName)
{
	int i;
	int iChk;

	for(i=0;i<sizeof(gszHomeLedTypesA)/sizeof(gszHomeLedTypesA[0]);i++)
	{
		iChk = strcmp(I_pszHomeLedTypeName,gszHomeLedTypesA[i]);
		if(0==iChk) {
			msp430_homeled_type_set(i);
			return i;
		}
	}
	return (-1);
}

int msp430_homeled_type_set(int iHomeLedType)
{
	unsigned short wHomeLedType;
	switch (iHomeLedType) {
	case MSP430_HOMELED_TYPE_NORMAL:
		wHomeLedType = 0x0100;
		break;
	case MSP430_HOMELED_TYPE_PWM:
		wHomeLedType = 0x0200;
		break;
	default :
		return -1;
	}

	if(iHomeLedType!=(int)gwHomeLedType) {
		up_write_reg(0xA9,wHomeLedType); // .
		gwHomeLedType = (unsigned short)iHomeLedType ;
		printk("%s() : [%d] -> 0x%x\n",__FUNCTION__,iHomeLedType,wHomeLedType);
	}

	return (int)(iHomeLedType);
}

int msp430_homeled_type_get(char **O_ppszHomeLedStr)
{
	if(O_ppszHomeLedStr) {
		*O_ppszHomeLedStr = gszHomeLedTypesA[gwHomeLedType];
	}
	return (int)(gwHomeLedType);
}

void msp430_homeled_enable(int iEnable)
{
	if(iEnable) {
		up_write_reg(0xA8,0x0100); // enabled the home led .
	}
	else {
		up_write_reg(0xA8,0x0000); // disabled the home led .
	}
}

int msp430_set_homeled_delayms(int iDelayms)
{
#if 1
	int iChk;

	if( MSP430_HOMELED_TYPE_PWM==gwHomeLedType ) {
		int iPWM_Param = iDelayms/1000/2;

		if(iPWM_Param<HOMELED_PWM_DELAY_LEVEL_MIN) {
			iPWM_Param = HOMELED_PWM_DELAY_LEVEL_MIN;
		}
		else if(iPWM_Param>HOMELED_PWM_DELAY_LEVEL_MAX) {
			iPWM_Param = HOMELED_PWM_DELAY_LEVEL_MAX;
		}
		iChk = msp430_set_homeled_pwm_delaylevel(iPWM_Param);
		if(iChk>=0) {
			return (iPWM_Param*1000*2);
		}
		else {
			return (-1);
		}
	}
	else if(MSP430_HOMELED_TYPE_NORMAL==gwHomeLedType) {
		int iGPIO_Param = iDelayms/100;

		if(iGPIO_Param<HOMELED_GPIO_DELAY_LEVEL_MIN) {
			iGPIO_Param = HOMELED_GPIO_DELAY_LEVEL_MIN;
		}
		else if(iGPIO_Param>HOMELED_GPIO_DELAY_LEVEL_MAX) {
			iGPIO_Param = HOMELED_GPIO_DELAY_LEVEL_MAX;
		}
		iChk = msp430_set_homeled_gpio_delaylevel(iGPIO_Param);
		if(iChk>=0) {
			return (iGPIO_Param*100);
		}
		else {
			return (-2);
		}
	}
	else {
		return (-3);
	}

#else
	printk("%s(), unsupported !!\n",__FUNCTION__);
	return (-4);
#endif
}


int msp430_set_homeled_gpio_delaylevel(int iDelayLevel)
{
	int iRet;
	unsigned short wDelayLevel,wCurDelayLevel;
	if(iDelayLevel<HOMELED_GPIO_DELAY_LEVEL_MIN) {
		printk(KERN_WARNING"%s():invalid delay level (<0x%x) \n",
				__FUNCTION__,HOMELED_GPIO_DELAY_LEVEL_MIN);
		return (-1);
	}
	if(iDelayLevel>HOMELED_GPIO_DELAY_LEVEL_MAX) {
		printk(KERN_WARNING"%s():invalid delay level (>0x%x) \n",
				__FUNCTION__,HOMELED_GPIO_DELAY_LEVEL_MAX);
		return (-2);
	}
	wCurDelayLevel = gwHomeLed_gpio_delay_level;
	wDelayLevel = ((unsigned short)iDelayLevel)<<8;
	if(wCurDelayLevel!=wDelayLevel) {
		printk("%s():0x%x->0x%x\n",__FUNCTION__,wCurDelayLevel,wDelayLevel);
		up_write_reg(0xAA,wDelayLevel); // set Home LED normal delay 
		gwHomeLed_gpio_delay_level = wDelayLevel;
	}
	return 0;
}
int msp430_get_homeled_gpio_delaylevel(void)
{
	int iRet;
	iRet = (int)(gwHomeLed_gpio_delay_level>>8);
	return iRet;
}


int msp430_set_homeled_pwm_delaylevel(int iDelayLevel)
{
	int iRet;
	unsigned short wDelayLevel,wCurDelayLevel;
	if(iDelayLevel<HOMELED_PWM_DELAY_LEVEL_MIN) {
		printk(KERN_WARNING"%s():invalid delay level (<0x%x) \n",
				__FUNCTION__,HOMELED_PWM_DELAY_LEVEL_MIN);
		return (-1);
	}
	if(iDelayLevel>HOMELED_PWM_DELAY_LEVEL_MAX) {
		printk(KERN_WARNING"%s():invalid delay level (>0x%x) \n",
				__FUNCTION__,HOMELED_PWM_DELAY_LEVEL_MAX);
		return (-2);
	}
	wCurDelayLevel = gwHomeLed_pwm_delay_level;
	wDelayLevel = ((unsigned short)iDelayLevel)<<8;
	if(wCurDelayLevel!=wDelayLevel) {
		printk("%s():0x%x->0x%x\n",__FUNCTION__,wCurDelayLevel,wDelayLevel);
		up_write_reg(0xAB,wDelayLevel); // set Home LED normal delay 
		gwHomeLed_pwm_delay_level = wDelayLevel;
	}
	return 0;
}

int msp430_get_homeled_pwm_delaylevel(void)
{
	int iRet;
	iRet = (int)(gwHomeLed_pwm_delay_level>>8);
	return iRet;
}


static void msp430_create_sys_attrs(void)
{
	extern void ntx_create_homepad_sys_attrs(struct kobject *kobj);

	if(!g_up_i2c_client) {
		printk(KERN_ERR"msp430 not ready !!\n");
		return ;
	}

	ntx_create_homepad_sys_attrs(&g_up_i2c_client->dev.kobj);

}

static __devinit int msp430_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int err = 0;

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
  		printk("%s, functionality check failed\n", __func__);
    		return -1;
  	}
	gdwLastRTCReadTick = jiffies;
	g_up_i2c_client = client;
	printk ("[%s-%d] firmware version %X\n",__func__,__LINE__,msp430_deviceid());

	if( 1==gptHWCFG->m_val.bPMIC && 0!=gptHWCFG->m_val.bFrontLight) {
		// FL_3V3 for Ricoh PMIC & FL is ON .
		g_fl_regulator = regulator_get(&client->dev, "vdd_fl_pwm");
		if (IS_ERR(g_fl_regulator)) {
			g_fl_regulator = regulator_get(&client->dev, "vdd_fl_0_pwm");
			if (IS_ERR(g_fl_regulator)) {
				printk("%s, regulator \"vdd_fl_pwm\" not registered.(%d)\n", __func__, g_fl_regulator);
				return -1;
			}
			else
  				printk("%s, regulator found on channel 0\n", __func__);
		}
		else {
  			printk("%s, regulator found\n", __func__);
		}
 	}

	err = request_irq(client->irq, msp430_int, IRQF_TRIGGER_FALLING, "msp430_int", "msp430_int");
	if (err < 0) {
		printk(KERN_ERR "%s(%s): Can't allocate irq %d\n", __FILE__, __func__, client->irq);
	}
	enable_irq_wake(client->irq);

	
	if( 36==gptHWCFG->m_val.bPCB || 0!=gptHWCFG->m_val.bHOME_LED_PWM ) {
		// E60Q3X || HOME LED enabled .
		msp430_homepad_enable(2);
	}

	if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
		extern int ntx_get_homeled_delay_ms(void);
		// HOME LED is controlled by MSP430 .
		msp430_homeled_type_set(MSP430_HOMELED_TYPE_NORMAL);
		//msp430_homeled_enable(1);
		msp430_set_homeled_delayms(ntx_get_homeled_delay_ms());
	}

	msp430_create_sys_attrs();
	return 0;
}

static __devexit int msp430_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static int msp430_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int iRet=0;

	giIsSuspending = 1;

	return iRet;
}

extern int ntx_get_homepad_enabled_status(void);

static int msp430_resume(struct i2c_client *client)
{
	int iRet=0;

	giIsSuspending = 0;

	gdwLastRTCReadTick = jiffies;

	//if(gSleep_Mode_Suspend) {
	//}
	return iRet;
}

static const struct i2c_device_id msp430_id[] = {
	{"msp430", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, msp430_id);

static struct i2c_driver up_i2c_driver = {
	.driver = {
		   .name = "msp430",
		   .owner = THIS_MODULE,
		   },
	.probe = msp430_i2c_probe,
	.remove = __devexit_p(msp430_i2c_remove),
	.id_table = msp430_id,
	.suspend = msp430_suspend,
	.resume = msp430_resume,
};

static struct platform_driver pmic_battery_driver_ldm = {
	.driver = {
		   .name = "pmic_battery",
		   .bus = &platform_bus_type,
		   .owner	= THIS_MODULE,
		   },
	.probe = pmic_battery_probe,
	.remove = pmic_battery_remove,
	.suspend = pmic_battery_suspend,
	.resume = pmic_battery_resume,
};

static int __init pmic_battery_init(void)
{
	pr_debug("PMIC Battery driver loading...\n");
	gdwTheTickToChkACINPlug = (unsigned long)(jiffies+TICS_TO_CHK_ACIN_AFTER_BOOT);
	i2c_add_driver(&up_i2c_driver);
	return platform_driver_register(&pmic_battery_driver_ldm);
}

static void __exit pmic_battery_exit(void)
{
	platform_driver_unregister(&pmic_battery_driver_ldm);
	i2c_del_driver(&up_i2c_driver);
	pr_debug("PMIC Battery driver successfully unloaded\n");
}

module_init(pmic_battery_init);
module_exit(pmic_battery_exit);

MODULE_DESCRIPTION("pmic_battery driver");
MODULE_AUTHOR("Netronix Inc.");
MODULE_LICENSE("GPL");

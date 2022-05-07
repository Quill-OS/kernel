/*
 * Copyright 2011-2012 Amazon.com, Inc. All rights reserved.
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

#if defined(CONFIG_MACH_MX50_YOSHIME) && defined(CONFIG_MXC_PMIC_MC13892)

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pmic_external.h>
#include <linux/pmic_adc.h>
#include <linux/pmic_light.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <mach/boardid.h>

#define DRIVER_NAME 	"aplite_charger"
#define VBUSVALIDS		0x08	/* Sense bit for valid VBUS */
#define CHGDETS  		(1U<<6)	/* Sense bit for CHGDET */
#define CHRGSE1BS		(1U<<10) /* SE1B sense bit */
#define BVALIDS			0x10000	/* Sense bit for BVALID */
#define CHGCURRS		0x800	/* Bit #11 */
#define CHGCURRS_THRESHOLD	4100	/* CHGCURRS Threshold */
#define PC_WORK_THRESHOLD	30000	/* Port change threshold */
#define ARCOTG_IRQ		37
#define CHARGER_STARTUP 	4000	/* Delayed workqueue that runs on startup */
#define CHARGER_MISC		10000	/* Charger misc timer */
#define CHARGER_TYPE	        0	/* Charger type detection */
#define CHARGER_TYPE_SUSPEND	5000	/* Charger detection after a system resume */
#define THIRD_PARTY_DETECT	10000	/* Third Party charger detect */
#define HOST_ENU_DETECT 	300	/* Host enumeration check */
#define BATT_VOLTAGE_SCALE	4687	/* Scale the values from the ADC based on manual */
#define BATT_CURRENT_SCALE	5865	/* Scale the current read from the ADC */
#define CHARGE_VOLTAGE_SCALE	11700	/* Scale the charger voltage read from A/D */
#define CHARGER_FULL		0x0	/* off - 0mA */
#define CHARGING_HOST		0x1	/* Host charging with PHY active */
#define CHARGING_WALL		0x5	/* Wall charging - */
#define CHARGING_THIRD_PARTY	0x5	/* Third party chargers */
#define BPSNS_VALUE		0x2	/* For LOBATHI and LOBATLI */
#define BPSNS_VALUE_SUSPEND	0x3	/* Suspend - LOBATHI and LOBATLI */
#define BPSNS_MASK		0x3	/* BPSNS Mask in PMIC reg 13 */
#define BPSNS_VOLTAGE_THRESH	3600	/* 3.6V BPSNS threshold */
#define PMIC_REG_PC_0		13	/* PMIC Register 13 */
#define BPSNS_OFFSET		16	/* BPSNS OFFSET in Register 13 */
#define GREEN_LED_THRESHOLD	95	/* Turn Charge LED to Green */
#define BATTCYCL_THRESHOLD	4105	/* Restart charging if not done automatically */
#define LOW_TEMP_CHARGING	0x2	/* 240mA charging if temp < 10C */
#define CHRGLEDEN		18 	/* offset 18 in REG_CHARGE */
#define CHRGCYCLB		22	/* BATTCYCL to restart charging at 95% full */
#define CHRGRESTART		20	/* Restart charging */
#define BPON_THRESHOLD		3800
#define BPON_THRESHOLD_HI	3810
#define CHARGING_BPON		0x3	/* 320mA charging */
#define CHARGER_TURN_ON		3400
#define CHRGRAW_THRESHOLD	6000
#define CRAW_MON_THREHOLD	6 	/* Every 60 seconds or so, monitor the chrgraw */
#define IDGND_SENSE		(1 << 20)
#define CHRG_SUS_LOW_VOLTAGE	3450	/* 3.45V */
#define CHRG_TIMER_FAULT_MAX	4

#define CHARGER_TYPE_NONE 	(0)
#define CHARGER_TYPE_HOST 	(1)
#define CHARGER_TYPE_WALL 	(2)

#define BIT_CHG_CURR_LSH	3
#define BIT_CHG_CURR_WID	4

#define BIT_CHG_CURRS_LSH 	11
#define BIT_CHG_CURRS_WID	1

#define BAT_TH_CHECK_DIS_LSH	9
#define BAT_TH_CHECK_DIS_WID	1

#define RESTART_CHG_STAT_LSH	20
#define RESTART_CHG_STAT_WID	1

#define AUTO_CHG_DIS_LSH	21
#define AUTO_CHG_DIS_WID	1

#define VI_PROGRAM_EN_LSH	23
#define VI_PROGRAM_EN_WID	1

#define EOC_BATTERY_CURRENT		(55)
#define EOC_BATTERY_CAPACITY   		(100)
#define RESTART_BATTERY_CAPACITY	(95)
#define CHARGER_WORK_INIT			(10*1000)	
#define CHARGER_WORK_INTERVAL 		(30*1000)
#define CHARGER_NODELAY				0			/* Charger detection without delay */
#define CHARGER_DELAY				(5*1000)	/* Charger detection after system resume */
#define VBUS_POWER_INIT				(100)

#define SYS_LOW_VOLTAGE_THRESHOLD  		3400    /* 3.4V */ 
#define SYS_CRIT_VOLTAGE_THRESHOLD 		3100    /* 3.1V */      
#define CHARGER_LOBAT_WORK_INTERVAL		(30*1000)

/*
 *	The spec sheet is not clear about these two bits. The name and description shifted for a row. 
 *	See P143 mc13892 user guild.
 * */
#define CHGFAULTM 0x000300

#define CHARGER_NAME	"aplite_charger"

extern int yoshi_battery_capacity;
extern int yoshi_reduce_charging;
extern int yoshi_battery_error_flags;
extern int yoshi_battery_current;
extern int yoshi_voltage;
extern int yoshi_battery_id;
extern int yoshi_battery_valid(void);
extern int lobat_condition;
extern int lobat_crit_condition;
extern void yoshi_battery_lobat_event(int);
extern int pmic_check_factory_mode(void);

static void charger_lobat_work_handler(struct work_struct *);
static DECLARE_DELAYED_WORK(charger_lobat_work, charger_lobat_work_handler);

static ssize_t
show_connected(struct device *dev, struct device_attribute *attr, char *buf);
static DEVICE_ATTR(connected, 0444, show_connected, NULL);

static ssize_t
show_charging(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t
store_charging(struct device *_dev, struct device_attribute *attr,
	       const char *buf, size_t count);
static DEVICE_ATTR(charging, 0644, show_charging, store_charging);

static ssize_t
store_green_led(struct device *_dev, struct device_attribute *attr,
               const char *buf, size_t count);
static DEVICE_ATTR(green_led, 0200, NULL, store_green_led);

static ssize_t
store_yellow_led(struct device *_dev, struct device_attribute *attr,
               const char *buf, size_t count);
static DEVICE_ATTR(yellow_led, 0200, NULL, store_yellow_led);

static void charger_work_handler(struct work_struct *);
static int detect_charger_type(void);
static DECLARE_DELAYED_WORK(charger_work, charger_work_handler);

/* 
 * We only set three charging current
 *
 * 1. 100mA for USB Host before enumeration
 * 2. 500mA for Wall charger
 * 3. 100 ~ 500mA for USB Host after enumeration
 * 
 * CHGDETI - indicates some charger attached. Set charging current to 100mA.
 * CHGSE1B - indicates wall charger attached. Set charging current to 500mA.
 *  
 * A hook is provided to usb to set charging current after enumeration.
 */
static pmic_event_callback_t charger_detect;
static pmic_event_callback_t lobatli_event;
static pmic_event_callback_t lobathi_event;
static pmic_event_callback_t chgfaulti_event;
static pmic_event_callback_t chrgse1b_event;
static int s_chrgfault_count;
static int battery_is_full;
static unsigned int s_vbus_current = VBUS_POWER_INIT;
static unsigned int chrg_disabled_by_user;
static int charger_suspended = 0;
static int chrg_conn = 0;
static int green_led_set = 0;
atomic_t charger_atomic_detection = ATOMIC_INIT(0);

static int pmic_set_chg_current(unsigned int curr);
static int is_charging_enabled(void);	/* check if charging enabled */
static int is_charger_connected(void);	/* check if charger connected */
static void charger_enable(int);		/* enabled / disable charging */

extern void gpio_chrgled_active(int enable);

#define VUSB2_VOTE_SOURCE_CHARGER  (1U << 3)
extern void pmic_vusb2_enable_vote(int enable,int source);

int battery_is_charging(void)
{
	if (is_charger_connected()) {
		if(is_charging_enabled() || battery_is_full )
			return 1;
	}
	return 0;
}
EXPORT_SYMBOL(battery_is_charging);

void battery_charging_ctrl(int enable)
{
	if (enable > 0)
		charger_enable(1);
	else 
		charger_enable(0);	
}
EXPORT_SYMBOL(battery_charging_ctrl);

static void set_chrg_led(int enable)
{
	pmic_write_reg(REG_CHARGE, (enable << CHRGLEDEN), (1 << CHRGLEDEN));
}

static void set_green_led(int enable)
{
	if (enable) {
		mc13892_bklit_set_current(LIT_KEY, 0x7);
		mc13892_bklit_set_ramp(LIT_KEY, 0);
		mc13892_bklit_set_dutycycle(LIT_KEY, 0x3f);
	} else {
		mc13892_bklit_set_current(LIT_KEY, 0);
		mc13892_bklit_set_dutycycle(LIT_KEY, 0);
	}
}

static int detect_charger_type(void)
{
	int chgdets = 0;
	int chrgse1bs = 0;
	int ichrg = 0; 

	pmic_read_reg(REG_INT_SENSE0, &chgdets, 0xffffff);
	pmic_read_reg(REG_PU_MODE_S, &chrgse1bs, 0xffffff);
	
	if (!(chgdets & CHGDETS)) {
		goto NO_CHARGER;
	}

	if (!(chrgse1bs & CHRGSE1BS)) {
		pr_debug("Wall charger detected\n");
		chrg_conn = CHARGER_TYPE_WALL;
		ichrg = CHARGING_WALL;
	} else {
		pr_debug("Charger detected\n");
		chrg_conn = CHARGER_TYPE_HOST;
		ichrg = CHARGING_WALL;
	}
NO_CHARGER:
	return ichrg;
}

/*
 * Is a USB charger connected?
 */
static int is_charger_connected(void)
{
	int sense_0 = 0;
	int ret = 0; /* Default: no charger */
	
	pmic_read_reg(REG_INT_SENSE0, &sense_0, 0xffffff);
	if (sense_0 & CHGDETS)
		ret = 1;

	pr_debug("%s: %d\n", __func__, ret);

	return ret;
}

static int is_charging_enabled(void)
{
	int value = 0;
	int ret = 0;
	pmic_read_reg(REG_CHARGE, &value, BITFMASK(BIT_CHG_CURR));
	if (value)
		ret = 1;
	return ret;
}

/*
 * Dynamically enable/disable the charger even if the charge
 * cable is connected.
 */
static void charger_enable(int enable)
{
	pr_debug("%s: begin\n", __func__);    

	if (enable) {
		/* reenable charging */
		pmic_set_chg_current(detect_charger_type());
		chrg_disabled_by_user = 0;
		/* Yellow(Chrg) LED - PMIC ctrl */
		set_chrg_led(1);
		gpio_chrgled_active(1);	
	}
	else {
		/* Set the ichrg to 0 */
		pmic_set_chg_current(0);

		/* Clear the charger timer counter */
		s_chrgfault_count  = 0;
		chrg_disabled_by_user = 1;
		battery_is_full = 0;
		/* Turn OFF both LEDs */
		set_green_led(0);
		set_chrg_led(0);
		gpio_chrgled_active(0);
		green_led_set = 0;
	}
}

static ssize_t
show_connected(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (is_charger_connected())
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t
show_charging(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", battery_is_charging());	
}

static ssize_t
store_charging(struct device *_dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	switch (buf[0]) {
		case '0':
			battery_charging_ctrl(0);
			break;
		case '1':
			battery_charging_ctrl(1);
			break;
		default:
			return -EINVAL;
	}
	return count;
}

static ssize_t
store_green_led(struct device *_dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
	switch (buf[0]) {
		case '0':
			set_green_led(0);
			break;
		case '1':
			set_green_led(1);
			break;
		default:
			return -EINVAL;
	}
	return count;
}

static ssize_t
store_yellow_led(struct device *_dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
	switch (buf[0]) {
		case '0':
			gpio_chrgled_active(0);
			break;
		case '1':
			gpio_chrgled_active(1);
			break;
		default:
			return -EINVAL;
	}
	return count;
}

static int pmic_set_chg_current(unsigned int curr)
{
	unsigned int mask;
	unsigned int value;

	/* If we have no battery, charging must be shut off unless in factory mode */
	if (!yoshi_battery_valid() && !pmic_check_factory_mode()) {
		printk(KERN_ERR "KERNEL: E pmic: Invalid battery disable charging::\n");
		curr = 0;
	}		

	if( curr > CHARGING_WALL) {
		/* invalid charging current */
		return -EINVAL;
	}
	value = BITFVAL(BIT_CHG_CURR, curr);
	mask = BITFMASK(BIT_CHG_CURR);
	CHECK_ERROR(pmic_write_reg(REG_CHARGE, value, mask));

	return 0;
}

static int pmic_read_adc_voltage(void)
{
	unsigned short bp = 0;
	int err = 0;

	err = pmic_adc_convert(APPLICATION_SUPPLY, &bp);

	if (err) {
		printk(KERN_ERR "KERNEL: E pmic:could not read APPLICATION_SUPPLY from adc: %d:\n",  err);
		return err;
	}

	/*
	 * MC13892: Multiply the adc value by BATT_VOLTAGE_SCALE to get
	 * the battery value. This is from the Atlas Lite manual
	 */
	return bp*BATT_VOLTAGE_SCALE/1000; /* Return in mV */
}

/*
 * The battery driver has already computed the voltage. 
 * Reuse the value unless battery error flags are set.
 */
static int read_supply_voltage(void)
{
	if ((!yoshi_battery_error_flags) && (yoshi_voltage > 0))
		return yoshi_voltage;
	
	/* use adc to read the voltage if it is not initialized */
	return pmic_read_adc_voltage();
}

static void charger_lobat_work_handler(struct work_struct *work)
{
	int batt_voltage = read_supply_voltage();
	int charger_conn = is_charger_connected();
	
	if (!yoshi_battery_valid())
		return;

	if (!charger_conn && (batt_voltage <= SYS_CRIT_VOLTAGE_THRESHOLD)) {
		yoshi_battery_lobat_event(1);
	} else if (!charger_conn && (batt_voltage <= SYS_LOW_VOLTAGE_THRESHOLD)) {
		yoshi_battery_lobat_event(0);
	} else {
		schedule_delayed_work(&charger_lobat_work, msecs_to_jiffies(CHARGER_LOBAT_WORK_INTERVAL));
	}

	return;
}

static void lobat_work(void)
{
	if (!yoshi_battery_valid())
		return;

	schedule_delayed_work(&charger_lobat_work, msecs_to_jiffies(CHARGER_LOBAT_WORK_INTERVAL));
	return;
}
		
/*
 * LOBATHI event callback from Atlas
 */
static void callback_lobathi_event(void *param)
{
	lobat_work();
}

/*
 * LOBATLI event callback from Atlas
 */
static void callback_lobatli_event(void *param)
{
	lobat_work();
}

/*
 * Charger callback from Atlas
 */
static void callback_charger_detect(void *param)
{
	struct platform_device *pdev = (struct platform_device *)param;
	int chrgdetected = 0;

	if (pdev == NULL) {
		printk(KERN_ERR "%s: invalid pdev\n",__func__);
		return;
	}

    pr_debug("%s: begin\n", __func__);
    /*
     * Exit the low power mode before the detection is done
     */	
    if (is_charger_connected()) {
		if (yoshi_battery_error_flags) {
			pr_debug("%s: battery error, not acking\n", __func__);
		    return;
		}

		if (atomic_read(&charger_atomic_detection) == 1) {
			pr_debug("%s: detection in progress\n", __func__);
		    return;
		}

		atomic_set(&charger_atomic_detection, 1);
		
		pmic_vusb2_enable_vote(1, VUSB2_VOTE_SOURCE_CHARGER);
		pr_debug("%s: exiting low power\n", __func__);

		if (charger_suspended) {
			charger_suspended = 0; 
			msleep(CHARGER_DELAY);
			pr_debug("%s: charger event - wakeup from suspend\n", __func__);
		} else {
			pr_debug("%s: charger event - idle charger plug\n", __func__);
		}
		if (detect_charger_type()) {
			chrgdetected = 1;
		}

		if (chrgdetected) {
			kobject_uevent(&((&pdev->dev)->kobj), KOBJ_ADD);
			if (device_create_file(&pdev->dev, &dev_attr_connected) < 0)
				printk(KERN_ERR "Charger: Error creating device file\n");

			/* cancel any pending lobat work */
			cancel_delayed_work_sync(&charger_lobat_work);
			/* schedule charger work */
			schedule_delayed_work(&charger_work, msecs_to_jiffies(CHARGER_WORK_INIT));
			if (lobat_condition) {
				lobat_crit_condition = 0;
				lobat_condition = 0;
			}
			if (yoshi_battery_valid()) {
				pmic_set_chg_current(CHARGING_WALL);
			}
		}
		/* Restart charging */
		pmic_write_reg(REG_CHARGE, (1 << CHRGRESTART), (1 << CHRGRESTART));
	} else {	/* charger disconnected */
		if (atomic_read(&charger_atomic_detection) == 0) {
			pr_debug("%s: already removed\n", __func__);

			pmic_vusb2_enable_vote(0, VUSB2_VOTE_SOURCE_CHARGER);
			return; 
		}

		atomic_set(&charger_atomic_detection, 0);
		pr_debug("%s: disconnecting\n", __func__);

		if (chrg_conn) {
			pr_debug("Charger disconnected\n");
			pmic_set_chg_current(0);
			chrg_conn = CHARGER_TYPE_NONE;
			s_chrgfault_count = 0;
			battery_is_full = 0;
			s_vbus_current = VBUS_POWER_INIT;
			device_remove_file(&pdev->dev, &dev_attr_connected);
			kobject_uevent_atomic(&((&pdev->dev)->kobj), KOBJ_REMOVE);
			cancel_delayed_work_sync(&charger_work);
			pmic_vusb2_enable_vote(0, VUSB2_VOTE_SOURCE_CHARGER);
			green_led_set = 0;
			/* Turn OFF green LED */
			set_green_led(0);
			/* Yellow(Chrg) LED - PMIC ctrl */
			set_chrg_led(1);
			gpio_chrgled_active(1);
		}
	}
}

static void pmic_restart_charging(void)
{
    if (yoshi_battery_valid()) {
	/* Restart the charging process */
	pmic_write_reg(REG_CHARGE, (1 << CHRGRESTART), (1 << CHRGRESTART));
    }
}

/*
 *! The charger timer is configured for 120 minutes in APLite.
 *  The charger is automatically turned off after 120 minutes of
 *  charging. Once this occurs, the APLite generates a CHGFAULT
 *  with the sense bits correctly set to "10"
 */
static void callback_faulti(void *param)
{
	int sense_0 = 0;
	/* Bits 9 and 8 should be "10" to detect charger timer expiry */
	int charger_timer_mask = 0x200;

	pmic_read_reg(REG_INT_SENSE0, &sense_0, 0xffffff);

	sense_0 &= CHGFAULTM;

	pr_debug("%s: sense=0x%x\n", __func__, sense_0);

	switch(sense_0) /*see mc13892 data sheet*/
	{	case 0x100:
			printk(KERN_INFO "%s: I pmic:faulti:sense_0=0x%x:charger path over voltage/dissipation in pass device.\n", CHARGER_NAME, sense_0);
			break;
		case 0x200:
			printk(KERN_INFO "%s: I pmic:faulti:sense_0=0x%x:battery dies/charger times out.\n", CHARGER_NAME, sense_0);	
			break;
		case 0x300:
			printk(KERN_INFO "%s: I pmic:faulti:sense_0=0x%x:battery out of temperature.\n", CHARGER_NAME, sense_0);
			break;
		default:
				;
	}
	/* Restart charging if the charger timer has expired */
	if (sense_0 & charger_timer_mask) {
		/*
		* According to the IEEE 1725 spec, the max charger timer
		* should not exceed 10 hours or 360 minutes
		* Note: Based on CEL-2457 changing from 6 hours to 10 hours
		*/
		if( ++s_chrgfault_count <=  CHRG_TIMER_FAULT_MAX ) {		
			if (is_charger_connected()) {
				printk(KERN_INFO "KERNEL: I %s:restart charging::\n",DRIVER_NAME);
			    pmic_restart_charging();
			}
		} else {
			printk(KERN_ERR "KERNEL: I %s:charging over 10hrs::\n",DRIVER_NAME);
		}
	} 
}

static void charger_loop(int resched_work)
{
	unsigned int ichrg = ~0U;

	/* Do not proceed if charger is not connected */
	if ( is_charger_connected() == 0 )
		return;

	if( chrg_disabled_by_user )
		goto retry;

	if ( yoshi_battery_error_flags || yoshi_battery_valid() == 0 ) {
		printk(KERN_ERR "KERNEL: E %s: Battery error: err_flag=0x%x, voltage=%d, battery_id=%d:\n",DRIVER_NAME,
			yoshi_battery_error_flags,yoshi_voltage,yoshi_battery_id);
		ichrg = 0;
		s_chrgfault_count = 0;
		battery_is_full = 0;
		green_led_set = 0;		
		/* Turn OFF both LEDs */
		set_green_led(0);
		set_chrg_led(0);
		gpio_chrgled_active(0);
		goto set_charging;
	}

	if (!green_led_set) {
		if (yoshi_battery_capacity > GREEN_LED_THRESHOLD) {
			green_led_set = 1;
			/* SW takes ctrl from PMIC to forcefully 
			 * turn OFF Yellow(Chrg) & turn ON Green LED 
			 */
			set_chrg_led(0);
			gpio_chrgled_active(0);	
			/* Turn ON green led */
			set_green_led(1);
		} else {
			green_led_set = 0;
			/* Turn OFF green led */
			set_green_led(0);
			/* Turn ON chrg yellow led */
			set_chrg_led(1);
			gpio_chrgled_active(1);	
		}
	}

	if( yoshi_battery_capacity == EOC_BATTERY_CAPACITY
		&& yoshi_battery_current > 0 && yoshi_battery_current < EOC_BATTERY_CURRENT ) {
		/* Disable charging */ 
		if (!battery_is_full) {
			ichrg = 0;
			s_chrgfault_count = 0;
			battery_is_full = 1;
			printk(KERN_INFO "KERNEL: I %s: Battery full Stop charging:\n",DRIVER_NAME);
		}
	} else if (yoshi_battery_capacity <= RESTART_BATTERY_CAPACITY) {
		if (battery_is_full) {
			printk(KERN_INFO "KERNEL: I %s: Restart charging:\n",DRIVER_NAME);
			battery_is_full = 0;
		}
		ichrg = detect_charger_type();
	} else if ((yoshi_battery_capacity > RESTART_BATTERY_CAPACITY) && !battery_is_full) {
		ichrg = detect_charger_type();
	}

	if(yoshi_reduce_charging) {
		if (ichrg > LOW_TEMP_CHARGING)
			ichrg = LOW_TEMP_CHARGING;
		printk(KERN_INFO "KERNEL: I %s: Reduce charging current:\n",DRIVER_NAME);
	}

set_charging:
	if( ichrg < ~0U )
		pmic_set_chg_current(ichrg);
retry:
	if(resched_work)
		schedule_delayed_work(&charger_work, msecs_to_jiffies(CHARGER_WORK_INTERVAL));
}

static void charger_work_handler(struct work_struct *work)
{
	charger_loop(1);
}

static int aplite_charger_probe(struct platform_device *pdev)
{
	set_green_led(0);		 
	/* Yellow(Chrg) LED - PMIC ctrl by default */
	set_chrg_led(1);
	gpio_chrgled_active(1);

	callback_charger_detect(pdev);

	charger_detect.param = pdev;
	charger_detect.func = callback_charger_detect;
	CHECK_ERROR(pmic_event_subscribe(EVENT_CHGDETI, charger_detect));

	chrgse1b_event.param = pdev;
	chrgse1b_event.func = callback_charger_detect;
	CHECK_ERROR(pmic_event_subscribe(EVENT_SE1I, chrgse1b_event));

	lobatli_event.param = pdev;
	lobatli_event.func = callback_lobatli_event;    
	CHECK_ERROR(pmic_event_subscribe(EVENT_LOBATLI, lobatli_event));

	lobathi_event.param = pdev;
	lobathi_event.func = callback_lobathi_event;
	CHECK_ERROR(pmic_event_subscribe(EVENT_LOBATHI, lobathi_event));

/*
	bpon_event.func = callback_bpon_event;
	CHECK_ERROR(pmic_event_subscribe(EVENT_BPONI, bpon_event));
*/
	chgfaulti_event.param = pdev;
	chgfaulti_event.func = callback_faulti;
	CHECK_ERROR(pmic_event_subscribe(EVENT_CHGFAULTI, chgfaulti_event));

	/* Set the BPSNS */
	CHECK_ERROR(pmic_write_reg(PMIC_REG_PC_0, BPSNS_VALUE << BPSNS_OFFSET,
					BPSNS_MASK << BPSNS_OFFSET));
	
	/* Enable cycling */
	pmic_write_reg(REG_CHARGE, (0 << CHRGCYCLB), (1 << CHRGCYCLB));
	
	/* */
	pmic_write_reg(REG_CHARGE, BITFVAL(VI_PROGRAM_EN,1),BITFMASK(VI_PROGRAM_EN));

	if (device_create_file(&pdev->dev, &dev_attr_charging) < 0)
		printk(KERN_ERR "Charger: Error creating device file for charging\n");

	if (device_create_file(&pdev->dev, &dev_attr_green_led) < 0)
		printk(KERN_ERR "Charger: Error creating device file for green led\n");

	if (device_create_file(&pdev->dev, &dev_attr_yellow_led) < 0)
		printk(KERN_ERR "Charger: Error creating device file for yellow led\n");

	return 0;
}

static int aplite_charger_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_connected);	
	device_remove_file(&pdev->dev, &dev_attr_charging);
	device_remove_file(&pdev->dev, &dev_attr_green_led);
	device_remove_file(&pdev->dev, &dev_attr_yellow_led);

	cancel_delayed_work_sync(&charger_lobat_work);
	cancel_delayed_work_sync(&charger_work);

	pmic_event_unsubscribe(EVENT_CHGDETI, charger_detect);
	pmic_event_unsubscribe(EVENT_LOBATLI, lobatli_event);
	pmic_event_unsubscribe(EVENT_LOBATHI, lobathi_event);
	pmic_event_unsubscribe(EVENT_CHGFAULTI, chgfaulti_event);
	pmic_event_unsubscribe(EVENT_SE1I, chrgse1b_event);
	return 0;
}

static int aplite_charger_suspend(struct platform_device *pdev, pm_message_t state)
{
	int batt_voltage = 0;
	/* Set the BPSNS suspend value */
	CHECK_ERROR(pmic_write_reg(PMIC_REG_PC_0, BPSNS_VALUE_SUSPEND << BPSNS_OFFSET,
					BPSNS_MASK << BPSNS_OFFSET));

	/* Don't suspend on low voltage 
	 * since the PMIC's LOBATH will shut us down soon 
	 */
	batt_voltage = read_supply_voltage();
	if (yoshi_battery_valid() && (batt_voltage > 0) && (batt_voltage <= CHRG_SUS_LOW_VOLTAGE))
		return -EBUSY;

	cancel_delayed_work_sync(&charger_work); 	
	cancel_delayed_work_sync(&charger_lobat_work);

	gpio_chrgled_active(1);
	charger_suspended = 1;
	return 0;
}


static int aplite_charger_resume(struct platform_device *pdev)
{
	/* Restore BPSNS value */
	CHECK_ERROR(pmic_write_reg(PMIC_REG_PC_0, BPSNS_VALUE << BPSNS_OFFSET,
					BPSNS_MASK << BPSNS_OFFSET));

	if (!is_charger_connected()) {
	    pmic_vusb2_enable_vote(0, VUSB2_VOTE_SOURCE_CHARGER);
	}
	return 0;
}

static struct platform_driver aplite_charger_driver = {
        .driver = {
                   .name = DRIVER_NAME, 
                   .bus = &platform_bus_type,
                   },
        .probe = aplite_charger_probe,
        .remove = aplite_charger_remove,
        .suspend = aplite_charger_suspend,
        .resume = aplite_charger_resume,
};

static int __init apl_charger_init(void)
{
	return platform_driver_register(&aplite_charger_driver);
}

module_init(apl_charger_init);

static void __exit apl_charger_exit(void)
{
	platform_driver_unregister(&aplite_charger_driver);
}

module_exit(apl_charger_exit);

MODULE_DESCRIPTION("MX50 Yoshime3 Charger Driver");
MODULE_AUTHOR("Vidhyananth Venkatasamy");
MODULE_LICENSE("GPL");

#endif

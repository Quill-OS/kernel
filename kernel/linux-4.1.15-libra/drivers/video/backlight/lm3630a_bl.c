/*
* Simple driver for Texas Instruments LM3630A Backlight driver chip
* Copyright (C) 2012 Texas Instruments
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/pwm.h>
#include <linux/platform_data/lm3630a_bl.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "../../../arch/arm/mach-imx/hardware.h"

#include "../../../arch/arm/mach-imx/ntx_firmware.h"
#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"
extern volatile NTX_HWCONFIG *gptHWCFG;


#include "lm3630a_bl_tables.h"

#define REG_CTRL	0x00
#define REG_BOOST	0x02
#define REG_CONFIG	0x01
#define REG_BRT_A	0x03
#define REG_BRT_B	0x04
#define REG_I_A		0x05
#define REG_I_B		0x06
#define REG_ONOFF_RAMP		0x07
#define REG_RUN_RAMP		0x08
#define REG_INT_STATUS	0x09
#define REG_INT_EN	0x0A
#define REG_FAULT	0x0B
#define REG_PWM_OUTLOW	0x12
#define REG_PWM_OUTHIGH	0x13
#define REG_REV		0x1F
#define REG_FILTER_STRENGTH		0x50
#define REG_MAX		0x51

#define INT_DEBOUNCE_MSEC	10

#define DEFAULT_ON_RAMP_LVL	0
#define DEFAULT_OFF_RAMP_LVL	0
#define DEFAULT_UP_RAMP_LVL	0
#define DEFAULT_DN_RAMP_LVL	0

struct lm3630a_chip {
	struct device *dev;
	struct delayed_work work;

	int irq;
	struct workqueue_struct *irqthread;
	struct lm3630a_platform_data *pdata;
	struct backlight_device *bleda;
	struct backlight_device *bledb;
	struct backlight_device *bled;

	struct regmap *regmap;
	struct pwm_device *pwmd;
	int hwen_gpio;

	int frontlight_table; // 2 color mix table index .
//	unsigned char bCurrentA; // LED currently current on A channel .
//	unsigned char bCurrentB; // LED currently current on B channel .
	unsigned char bBrightnessA; // LED currently brightness(duty) on A channel .
	unsigned char bBrightnessB; // LED currently brightness(duty) on B channel .
	unsigned char *pbPercentDutyTableA; // Percent duty mapping table on A channel .
	int iPercentDutyTableASize; // Percent duty mapping table size on A channel .
	unsigned char *pbPercentDutyTableB; // Percent duty ampping table on B channel .
	int iPercentDutyTableBSize; // Percent duty mapping table size on B channel .
	unsigned char bDefaultCurrentA;
	unsigned char bDefaultCurrentB;
	unsigned char bOnOffRamp;
	unsigned char bRunRamp;
	unsigned long ulFLRampWaitTick;
};

extern int volatile giSuspendingState;
extern int gSleep_Mode_Suspend;

static volatile int giFL_PWR_state;
static volatile int giFL_PWR_state_max;

static struct lm3630a_chip *gpchip[2];
static int gilm3630a_chips;

static int fl_en_status = 0;

extern NTX_FW_LM3630FL_dualcolor_hdr *gptLm3630fl_dualcolor_tab_hdr; 
extern NTX_FW_LM3630FL_dualcolor_percent_tab *gptLm3630fl_dualcolor_percent_tab; 
extern int lm3630a_get_FL_current(void);

const static unsigned long gdwRamp_lvl_wait_ticksA[8] = {
	1,//0
	27,//1
	53,//2
	105,//3
	210,//4
	419,//5
	837,//6
	1673,//7
};


/* i2c access */
static int lm3630a_read(struct lm3630a_chip *pchip, unsigned int reg)
{
	int rval;
	unsigned int reg_val;

	rval = regmap_read(pchip->regmap, reg, &reg_val);
	if (rval < 0)
		return rval;
	return reg_val & 0xFF;
}

static int lm3630a_write(struct lm3630a_chip *pchip,
			 unsigned int reg, unsigned int data)
{
	return regmap_write(pchip->regmap, reg, data);
}

static int lm3630a_update(struct lm3630a_chip *pchip,
			  unsigned int reg, unsigned int mask,
			  unsigned int data)
{
	return regmap_update_bits(pchip->regmap, reg, mask, data);
}

/* initialize chip */
static int lm3630a_chip_init(struct lm3630a_chip *pchip)
{
	int rval;
	struct lm3630a_platform_data *pdata = pchip->pdata;
	int iRampLvl;

	usleep_range(1000, 2000);

	/* set Filter Strength Register */
	rval = lm3630a_write(pchip, REG_FILTER_STRENGTH, 0x03);
	/* set Cofig. register */
	rval |= lm3630a_update(pchip, REG_CONFIG, 0x07, pdata->pwm_ctrl);
	/* set ramp registers */
	rval |= lm3630a_write(pchip, REG_ONOFF_RAMP, pchip->bOnOffRamp);
	rval |= lm3630a_write(pchip, REG_RUN_RAMP, pchip->bRunRamp);
	/* set boost control */
	//rval |= lm3630a_write(pchip, REG_BOOST, 0x38);
	rval |= lm3630a_write(pchip, REG_BOOST, 0x63);
	/* set current A */
	pchip->bleda->props.power = 0;
	rval |= lm3630a_update(pchip, REG_I_A, 0x1F, pchip->bleda->props.power);
	/* set current B */
	pchip->bledb->props.power = 0;
	rval |= lm3630a_write(pchip, REG_I_B, pchip->bledb->props.power);
	/* set control */
	rval |= lm3630a_update(pchip, REG_CTRL, 0x14, pdata->leda_ctrl);// LED_A_EN,LINEAR_A control .
	rval |= lm3630a_update(pchip, REG_CTRL, 0x0B, pdata->ledb_ctrl);// LED_B_EN,LINEAR_B control .
	usleep_range(1000, 2000);
	/* set brightness A and B */
	rval |= lm3630a_write(pchip, REG_BRT_A, pdata->leda_init_brt);
	pchip->bleda->props.brightness = pdata->leda_init_brt;
	rval |= lm3630a_write(pchip, REG_BRT_B, pdata->ledb_init_brt);
	pchip->bledb->props.brightness = pdata->ledb_init_brt;

	

	iRampLvl=pchip->bOnOffRamp&0x7;
	if((int)((pchip->bOnOffRamp>>3)&0x7)>iRampLvl) {
		iRampLvl = (int)((pchip->bOnOffRamp>>3)&0x7);
	}

	pchip->ulFLRampWaitTick = jiffies+gdwRamp_lvl_wait_ticksA[iRampLvl];


	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");
	return rval;
}

/* interrupt handling */
static void lm3630a_delayed_func(struct work_struct *work)
{
	int rval;
	struct lm3630a_chip *pchip;

	pchip = container_of(work, struct lm3630a_chip, work.work);

	rval = lm3630a_read(pchip, REG_INT_STATUS);
	if (rval < 0) {
		dev_err(pchip->dev,
			"i2c failed to access REG_INT_STATUS Register\n");
		return;
	}

	dev_info(pchip->dev, "REG_INT_STATUS Register is 0x%x\n", rval);
}

static irqreturn_t lm3630a_isr_func(int irq, void *chip)
{
	int rval;
	struct lm3630a_chip *pchip = chip;
	unsigned long delay = msecs_to_jiffies(INT_DEBOUNCE_MSEC);

	queue_delayed_work(pchip->irqthread, &pchip->work, delay);

	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0) {
		dev_err(pchip->dev, "i2c failed to access register\n");
		return IRQ_NONE;
	}
	return IRQ_HANDLED;
}

static int lm3630a_intr_config(struct lm3630a_chip *pchip)
{
	int rval;

	rval = lm3630a_write(pchip, REG_INT_EN, 0x87);
	if (rval < 0)
		return rval;

	INIT_DELAYED_WORK(&pchip->work, lm3630a_delayed_func);
	pchip->irqthread = create_singlethread_workqueue("lm3630a-irqthd");
	if (!pchip->irqthread) {
		dev_err(pchip->dev, "create irq thread fail\n");
		return -ENOMEM;
	}
	if (request_threaded_irq
	    (pchip->irq, NULL, lm3630a_isr_func,
	     IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lm3630a_irq", pchip)) {
		dev_err(pchip->dev, "request threaded irq fail\n");
		destroy_workqueue(pchip->irqthread);
		return -ENOMEM;
	}
	return rval;
}

static void lm3630a_pwm_ctrl(struct lm3630a_chip *pchip, int br, int br_max)
{
	unsigned int period = pwm_get_period(pchip->pwmd);
	unsigned int duty = br * period / br_max;

	pwm_config(pchip->pwmd, duty, period);
	if (duty)
		pwm_enable(pchip->pwmd);
	else
		pwm_disable(pchip->pwmd);
}

/* update and get brightness */
static int lm3630a_bank_a_update_status(struct backlight_device *bl)
{
	int ret;
	int fl_current_now;
	struct lm3630a_chip *pchip = bl_get_data(bl);
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

#if 0
	printk("%s():brightness %d,FL_gpio(%d)=%d,pwr=%d,%s\n",__FUNCTION__,
		bl->props.brightness,pchip->hwen_gpio,
		gpio_get_value(pchip->hwen_gpio),giFL_PWR_state,
		(bl->props.state & BL_CORE_SUSPENDED)?"suspending":"normal");
	printk("%s():suspending state=%d,SleepMode=%d\n",
		__FUNCTION__,giSuspendingState,gSleep_Mode_Suspend);
#endif


	if (bl->props.state & BL_CORE_SUSPENDED) {
		
		// single FL LED from CH A
		if (gSleep_Mode_Suspend) {
			//printk("%s():hibernation\n",__FUNCTION__);
			
			if(pchip&&time_before(jiffies,pchip->ulFLRampWaitTick)) {
				printk(KERN_INFO"%s():ramp waiting ...\n",__FUNCTION__);
				return -1;
			}

			if(--giFL_PWR_state<=0) {
				if(giFL_PWR_state<0) {
					printk(KERN_WARNING"%s():[WARNING] FL PWR state <0 !!\n",__FUNCTION__);
					giFL_PWR_state=0;
				}
				
				if (fl_en_status) {
					printk("%s():FL PWR[OFF] \n",__FUNCTION__);
					gpio_direction_output(pchip->hwen_gpio, 0);
					fl_en_status = 0;
				}
			}

			return 0;
		}
		else {
			//printk("%s():suspend\n",__FUNCTION__);
			if (cpu_is_imx6ull() || 87==gptHWCFG->m_val.bPCB) {
				fl_current_now = lm3630a_get_FL_current();
				if (fl_current_now == -1 && fl_en_status) {
					printk("%s():FL PWR[OFF] \n",__FUNCTION__);
					gpio_direction_output(pchip->hwen_gpio, 0);
					fl_en_status = 0;
				}
				return 0;
			}
		}
	}

	if (giSuspendingState && gSleep_Mode_Suspend) {
		//printk("%s():hibernation resume\n",__FUNCTION__);
		if(++giFL_PWR_state>giFL_PWR_state_max) {
			printk(KERN_WARNING"%s():[WARNING] FL PWR state > max !!\n",__FUNCTION__);
			giFL_PWR_state=giFL_PWR_state_max;
		}

		if (giFL_PWR_state==1) {
			printk("%s():FL PWR[ON] \n",__FUNCTION__);
			gpio_direction_output(pchip->hwen_gpio, 1);
			fl_en_status = 1;
			msleep (50);
			lm3630a_chip_init(pchip);
		}
	}
	else if ((cpu_is_imx6ull() || 87==gptHWCFG->m_val.bPCB) && giSuspendingState && !fl_en_status) {
		gpio_direction_output(pchip->hwen_gpio, 1);
		fl_en_status = 1;
		msleep (50);
		lm3630a_chip_init(pchip);
	}

	if (pchip->bBrightnessA == bl->props.brightness)
		return bl->props.brightness;

	pchip->bBrightnessA = bl->props.brightness;

	/* pwm control */
	if ((pwm_ctrl & LM3630A_PWM_BANK_A) != 0) {
		lm3630a_pwm_ctrl(pchip, bl->props.brightness,
				 bl->props.max_brightness);
		return bl->props.brightness;
	}

	/* disable sleep */
	ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (ret < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	/* minimum brightness is 0x04 */
	ret = lm3630a_write(pchip, REG_BRT_A, bl->props.brightness);
	if( 0x20 > bl->props.power ) {
		ret |= lm3630a_update(pchip,REG_I_A,0x1F,bl->props.power);
	}

	if (bl->props.brightness < 0x1) {
		ret |= lm3630a_update(pchip, REG_CTRL, LM3630A_LEDA_ENABLE, 0);
		if (!(lm3630a_read(pchip, REG_CTRL) & LM3630A_LEDB_ENABLE)) {
			// Enter sleep mode if both led are disabled.
			ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x80);
		}
	}
	else
		ret |= lm3630a_update(pchip, REG_CTRL,
				      LM3630A_LEDA_ENABLE, LM3630A_LEDA_ENABLE);
	if (ret < 0)
		goto out_i2c_err;
	return bl->props.brightness;

out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access\n");
	return bl->props.brightness;
}

static int lm3630a_bank_a_get_brightness(struct backlight_device *bl)
{
	int brightness, rval;
	struct lm3630a_chip *pchip = bl_get_data(bl);
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	if ((pwm_ctrl & LM3630A_PWM_BANK_A) != 0) {
		rval = lm3630a_read(pchip, REG_PWM_OUTHIGH);
		if (rval < 0)
			goto out_i2c_err;
		brightness = (rval & 0x01) << 8;
		rval = lm3630a_read(pchip, REG_PWM_OUTLOW);
		if (rval < 0)
			goto out_i2c_err;
		brightness |= rval;
		goto out;
	}

	/* disable sleep */
	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	rval = lm3630a_read(pchip, REG_BRT_A);
	if (rval < 0)
		goto out_i2c_err;
	brightness = rval;

out:
	bl->props.brightness = brightness;
	//printk("%s():brightness %d\n",__FUNCTION__,brightness);
	return bl->props.brightness;
out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access register\n");
	return 0;
}


static const struct backlight_ops lm3630a_bank_a_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3630a_bank_a_update_status,
	.get_brightness = lm3630a_bank_a_get_brightness,
};

/* update and get brightness */
static int lm3630a_bank_b_update_status(struct backlight_device *bl)
{
	int ret;
	int fl_current_now;
	struct lm3630a_chip *pchip = bl_get_data(bl);
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

#if 0
	printk("%s():brightness %d,FL_gpio(%d)=%d,pwr=%d,%s\n",__FUNCTION__,
		bl->props.brightness,pchip->hwen_gpio,
		gpio_get_value(pchip->hwen_gpio),giFL_PWR_state,
		(bl->props.state & BL_CORE_SUSPENDED)?"suspending":"normal");
#endif

	if (bl->props.state & BL_CORE_SUSPENDED) {


		// single FL LED from CH B 
		if (gSleep_Mode_Suspend) {
			//printk("%s():hibernation\n",__FUNCTION__);

			if(pchip&&time_before(jiffies,pchip->ulFLRampWaitTick)) {
				printk(KERN_INFO"%s():ramp waiting ...\n",__FUNCTION__);
				return -1;
			}
			
			if(--giFL_PWR_state<=0) {
				if(giFL_PWR_state<0) {
					printk(KERN_WARNING"%s():[WARNING] FL PWR state <0 !!\n",__FUNCTION__);
					giFL_PWR_state=0;
				}
				
				if (fl_en_status) {
					printk("%s():FL PWR[OFF] \n",__FUNCTION__);
					gpio_direction_output(pchip->hwen_gpio, 0);
					fl_en_status = 0;
				}
			}
			
			return 0;
		}
		else {
			//printk("%s():suspend\n",__FUNCTION__);
			if (cpu_is_imx6ull() || 87==gptHWCFG->m_val.bPCB) {
				fl_current_now = lm3630a_get_FL_current();
				if (fl_current_now == -1 && fl_en_status) {
					printk("%s():FL PWR[OFF] \n",__FUNCTION__);
					gpio_direction_output(pchip->hwen_gpio, 0);
					fl_en_status = 0;
				}
				return 0;
			}
		}
	}

	if (giSuspendingState && gSleep_Mode_Suspend) {
		//printk("%s():hibernation resume\n",__FUNCTION__);
		if(++giFL_PWR_state>giFL_PWR_state_max) {
			printk(KERN_WARNING"%s():[WARNING] FL PWR state > max !!\n",__FUNCTION__);
			giFL_PWR_state=giFL_PWR_state_max;
		}

		if (giFL_PWR_state==1) {
			printk("%s():FL PWR[ON] \n",__FUNCTION__);
			gpio_direction_output(pchip->hwen_gpio, 1);
			fl_en_status = 1;
			msleep (50);
			lm3630a_chip_init(pchip);
		}
	}
	else if ((cpu_is_imx6ull() || 87==gptHWCFG->m_val.bPCB) && giSuspendingState && !fl_en_status) {
		gpio_direction_output(pchip->hwen_gpio, 1);
		fl_en_status = 1;
		msleep (50);
		lm3630a_chip_init(pchip);
	}
	
	if (pchip->bBrightnessB == bl->props.brightness)
		return bl->props.brightness;

	pchip->bBrightnessB = bl->props.brightness;

	/* pwm control */
	if ((pwm_ctrl & LM3630A_PWM_BANK_B) != 0) {
		lm3630a_pwm_ctrl(pchip, bl->props.brightness,
				 bl->props.max_brightness);
		return bl->props.brightness;
	}

	/* disable sleep */
	ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (ret < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	/* minimum brightness is 0x04 */
	ret = lm3630a_write(pchip, REG_BRT_B, bl->props.brightness);
	if( 0x20 > bl->props.power ) {
		ret |= lm3630a_write(pchip,REG_I_B,bl->props.power);
	}

	if (bl->props.brightness < 0x1) {
		ret |= lm3630a_update(pchip, REG_CTRL, LM3630A_LEDB_ENABLE, 0);
		if (!(lm3630a_read(pchip, REG_CTRL) & LM3630A_LEDA_ENABLE)) {
			// Enter sleep mode if both led are disabled.
			ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x80);
		}
	}
	else
		ret |= lm3630a_update(pchip, REG_CTRL,
				      LM3630A_LEDB_ENABLE, LM3630A_LEDB_ENABLE);
	if (ret < 0)
		goto out_i2c_err;
	return bl->props.brightness;

out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access REG_CTRL\n");
	return bl->props.brightness;
}

static int lm3630a_bank_b_get_brightness(struct backlight_device *bl)
{
	int brightness, rval;
	struct lm3630a_chip *pchip = bl_get_data(bl);
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	if ((pwm_ctrl & LM3630A_PWM_BANK_B) != 0) {
		rval = lm3630a_read(pchip, REG_PWM_OUTHIGH);
		if (rval < 0)
			goto out_i2c_err;
		brightness = (rval & 0x01) << 8;
		rval = lm3630a_read(pchip, REG_PWM_OUTLOW);
		if (rval < 0)
			goto out_i2c_err;
		brightness |= rval;
		goto out;
	}

	/* disable sleep */
	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	rval = lm3630a_read(pchip, REG_BRT_B);
	if (rval < 0)
		goto out_i2c_err;
	brightness = rval;

out:
	bl->props.brightness = brightness;
	//printk("%s():brightness %d\n",__FUNCTION__,brightness);
	return bl->props.brightness;
out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access register\n");
	return 0;
}

static const struct backlight_ops lm3630a_bank_b_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3630a_bank_b_update_status,
	.get_brightness = lm3630a_bank_b_get_brightness,
};

static ssize_t led_a_per_info(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = pchip->bleda->props.brightness;
	if (val) {
		for (val=0;val <pchip->iPercentDutyTableASize ;val++) {
			if (pchip->bleda->props.brightness == pchip->pbPercentDutyTableA[val])
				break;
		}
		val++;
	}
	sprintf (buf, "%d", val);
	return strlen(buf);
}

static ssize_t led_a_per_ctrl(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = simple_strtoul (buf, NULL, 10);
	if (val == 0)
		pchip->bleda->props.brightness = 0;
	else {
		if (val > pchip->iPercentDutyTableASize)
			val = pchip->iPercentDutyTableASize;
		pchip->bleda->props.brightness = pchip->pbPercentDutyTableA[val-1];
	}
//	printk ("[%s-%d] set %d\%, brightness %d\n", __func__, __LINE__, val, pchip->bleda->props.brightness);
	lm3630a_bank_a_update_status (pchip->bleda);
	return count;
}

static DEVICE_ATTR (percent, 0644, led_a_per_info, led_a_per_ctrl);

static ssize_t led_b_per_info(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = pchip->bledb->props.brightness;
	if (val) {
		for (val=0;val < pchip->iPercentDutyTableBSize;val++) {
			if (pchip->bledb->props.brightness == pchip->pbPercentDutyTableB[val])
				break;
		}
		val++;
	}
	sprintf (buf, "%d", val);
	return strlen(buf);
}

static ssize_t led_b_per_ctrl(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = simple_strtoul (buf, NULL, 10);

	if (val == 0)
		pchip->bledb->props.brightness = 0;
	else {
		if (val > pchip->iPercentDutyTableBSize)
			val = pchip->iPercentDutyTableBSize;
		pchip->bledb->props.brightness = pchip->pbPercentDutyTableB[val-1];
	}
//	printk ("[%s-%d] set %d\%, brightness %d\n", __func__, __LINE__, val, pchip->bledb->props.brightness);
	lm3630a_bank_b_update_status (pchip->bledb);
	return count;
}

static struct device_attribute dev_attr_b_percent = __ATTR(percent, 0644,led_b_per_info, led_b_per_ctrl);



int lm3630a_get_FL_current(void)
{
	int iRet = -1;
	unsigned long dwCurrent=0;

	unsigned long *pdwCurrTab;
	unsigned long dwCurrTabSize;
	int iTabIdx;

	switch (gptHWCFG->m_val.bFL_PWM)
	{
	case 5:// 4 color FL .
	{
		unsigned long dwRGBW_CurrTabItems;
		NTX_FW_LM3630FL_RGBW_current_item *ptRGBW_CurrTab;


		dwCurrent=0;

		lm3630a_get_FL_RGBW_RicohCurrTab(&dwRGBW_CurrTabItems,&ptRGBW_CurrTab);

		if(ptRGBW_CurrTab) {
			// binary search the RGBW current .
			int iFirst,iLast,iMiddle,iEnd,iTmp;
			unsigned char bCurVal;
			unsigned char bCurR,bCurG,bCurB,bCurW;

			iFirst=0;
			iLast=iEnd=dwRGBW_CurrTabItems-1;
		
//#define RGBW_BSEARCH_DBG	1

#if 0
			// for test .

			bCurR = 165;
			bCurG = 81;
			bCurB = 1;
			bCurW = 1;
#else 	
			bCurR = gpchip[1]->bleda->props.brightness;
			bCurG = gpchip[0]->bledb->props.brightness;
			bCurB = gpchip[0]->bleda->props.brightness;
			bCurW = gpchip[1]->bledb->props.brightness;
#endif


			while(iFirst<=iLast) 
			{
				iMiddle = (iFirst==iLast)?iFirst:(iFirst+iLast)/2;
				
				bCurVal = ptRGBW_CurrTab[iMiddle].bRed;

				if(bCurR>bCurVal) {
					iFirst = iMiddle+1;
				}
				else
				if(bCurR==bCurVal) {

					for(iTmp=iMiddle;iTmp>iFirst;) {
						if(ptRGBW_CurrTab[iTmp-1].bRed==bCurR) {
							iTmp--;continue;
						}
						else {
							break;
						}
					}
					iFirst=iTmp;

					for(iTmp=iMiddle;iTmp<iLast;) {
						if(ptRGBW_CurrTab[iTmp+1].bRed==bCurR) {
							iTmp++;continue;
						}
						else {
							break;
						}
					}
					iLast=iTmp;
#ifdef RGBW_BSEARCH_DBG//[
					printk("Got R=%d in idx %d of table(%d~%d)\n",
							(int)bCurR,iMiddle,iFirst,iLast);
#endif //]RGBW_BSEARCH_DBG

					break;
				}
				else {
					iLast = iMiddle-1;
				}
			}

			while(iFirst<=iLast) 
			{
				iMiddle = (iFirst==iLast)?iFirst:(iFirst+iLast)/2;
				
				bCurVal = ptRGBW_CurrTab[iMiddle].bGreen;

				if(bCurG>bCurVal) {
					iFirst = iMiddle+1;
				}
				else
				if(bCurG==bCurVal) {

					for(iTmp=iMiddle;iTmp>iFirst;) {
						if(ptRGBW_CurrTab[iTmp-1].bGreen==bCurG) {
							iTmp--;continue;
						}
						else {
							break;
						}
					}
					iFirst=iTmp;

					for(iTmp=iMiddle;iTmp<iLast;) {
						if(ptRGBW_CurrTab[iTmp+1].bGreen==bCurG) {
							iTmp++;continue;
						}
						else {
							break;
						}
					}
					iLast=iTmp;

#ifdef RGBW_BSEARCH_DBG//[
					printk("Got G=%d in idx %d of table(%d~%d)\n",
							(int)bCurG,iMiddle,iFirst,iLast);
#endif //]RGBW_BSEARCH_DBG


					break;
				}
				else {
					iLast = iMiddle-1;
				}
			}

			while(iFirst<=iLast) 
			{
				iMiddle = (iFirst==iLast)?iFirst:(iFirst+iLast)/2;
				
				bCurVal = ptRGBW_CurrTab[iMiddle].bBlue;

				if(bCurB>bCurVal) {
					iFirst = iMiddle+1;
				}
				else
				if(bCurB==bCurVal) {
					for(iTmp=iMiddle;iTmp>iFirst;) {
						if(ptRGBW_CurrTab[iTmp-1].bBlue==bCurB) {
							iTmp--;continue;
						}
						else {
							break;
						}
					}
					iFirst=iTmp;

					for(iTmp=iMiddle;iTmp<iLast;) {
						if(ptRGBW_CurrTab[iTmp+1].bBlue==bCurB) {
							iTmp++;continue;
						}
						else {
							break;
						}
					}
					iLast=iTmp;


#ifdef RGBW_BSEARCH_DBG//[
					printk("Got B=%d in idx %d of table(%d~%d)\n",
							(int)bCurB,iMiddle,iFirst,iLast);
#endif //]RGBW_BSEARCH_DBG


					break;
				}
				else {
					iLast = iMiddle-1;
				}
			}

			

			while(iFirst<=iLast) 
			{
				iMiddle = (iFirst==iLast)?iFirst:(iFirst+iLast)/2;
				
				bCurVal = ptRGBW_CurrTab[iMiddle].bWhite;

				if(bCurW>bCurVal) {
					iFirst = iMiddle+1;
				}
				else
				if(bCurW==bCurVal) {
					for(iTmp=iMiddle;iTmp>iFirst;) {
						if(ptRGBW_CurrTab[iTmp-1].bWhite==bCurW) {
							iTmp--;continue;
						}
						else {
							break;
						}
					}
					iFirst=iTmp;

					for(iTmp=iMiddle;iTmp<iLast;) {
						if(ptRGBW_CurrTab[iTmp+1].bWhite==bCurW) {
							iTmp++;continue;
						}
						else {
							break;
						}
					}
					iLast=iTmp;

#ifdef RGBW_BSEARCH_DBG//[
					printk("Got W=%d in idx %d of table(%d~%d)\n",
							(int)bCurW,iMiddle,iFirst,iLast);
#endif //]RGBW_BSEARCH_DBG
					

					break;
				}
				else {
					iLast = iMiddle-1;
				}
			}

			if( (bCurW==ptRGBW_CurrTab[iMiddle].bWhite) && 
					(bCurR==ptRGBW_CurrTab[iMiddle].bRed) &&
					(bCurB==ptRGBW_CurrTab[iMiddle].bBlue) &&
					(bCurG==ptRGBW_CurrTab[iMiddle].bGreen) ) 
			{
				iRet = (int)ptRGBW_CurrTab[iMiddle].dwCurrent;
//#ifdef RGBW_BSEARCH_DBG//[
				printk("R=%d,G=%d,B=%d,W=%d,curr=%duA\n",
						(int)bCurR,(int)bCurG,(int)bCurB,(int)bCurW,iRet);
//#endif //]RGBW_BSEARCH_DBG
			}
			else {
				iRet = (int)ptRGBW_CurrTab[iMiddle].dwCurrent;
				printk(KERN_ERR"curr@ R=%d,G=%d,B=%d,W=%d not found !!,assume curr=%duA\n",
						(int)bCurR,(int)bCurG,(int)bCurB,(int)bCurW,iRet);
			}

		}
		else {
			if(gpchip[1]->bleda->props.brightness) {
				iTabIdx = gpchip[1]->bleda->props.brightness-1;
				if(lm3630a_get_FL_RicohCurrTab(1,0,&pdwCurrTab,&dwCurrTabSize)<0) {
					iRet = -2;break;
				}
				if(iTabIdx>=(int)dwCurrTabSize) {
					printk(KERN_ERR"%s():RED tab idx(%d) >tab size(%d) \n",__FUNCTION__,
						iTabIdx,(int)dwCurrTabSize);
					iRet = -3;break;
				}
				dwCurrent += pdwCurrTab[iTabIdx];
				printk("RED curr[%d]=%d\n",iTabIdx+1,(int)(pdwCurrTab[iTabIdx]));
			}


			if(gpchip[1]->bledb->props.brightness) {
				iTabIdx = gpchip[1]->bledb->props.brightness-1;
				if(lm3630a_get_FL_RicohCurrTab(1,1,&pdwCurrTab,&dwCurrTabSize)<0) {
					iRet = -4;break;
				}
				if(iTabIdx>=(int)dwCurrTabSize) {
					printk(KERN_ERR"%s():WHITE tab idx(%d) >tab size(%d) \n",__FUNCTION__,
						iTabIdx,(int)dwCurrTabSize);
					iRet = -5;break;
				}
				dwCurrent += pdwCurrTab[iTabIdx];
				printk("WHITE curr[%d]=%d\n",iTabIdx+1,(int)(pdwCurrTab[iTabIdx]));
			}


			if(gpchip[0]->bleda->props.brightness) {
				iTabIdx = gpchip[0]->bleda->props.brightness-1;
				if(lm3630a_get_FL_RicohCurrTab(0,0,&pdwCurrTab,&dwCurrTabSize)<0) {
					iRet = -6;break;
				}
				if(iTabIdx>=(int)dwCurrTabSize) {
					printk(KERN_ERR"%s():BLUE tab idx(%d) >tab size(%d) \n",__FUNCTION__,
						iTabIdx,(int)dwCurrTabSize);
					iRet = -7;break;
				}
				dwCurrent += pdwCurrTab[iTabIdx];
				printk("BLUE curr[%d]=%d\n",iTabIdx+1,(int)(pdwCurrTab[iTabIdx]));
			}

			if(gpchip[0]->bledb->props.brightness) {
				iTabIdx = gpchip[0]->bledb->props.brightness-1;
				if(lm3630a_get_FL_RicohCurrTab(0,1,&pdwCurrTab,&dwCurrTabSize)<0) {
					iRet = -8;break;
				}
				if(iTabIdx>=(int)dwCurrTabSize) {
					printk(KERN_ERR"%s():GREEN tab idx(%d) >tab size(%d) \n",__FUNCTION__,
						iTabIdx,(int)dwCurrTabSize);
					iRet = -9;break;
				}
				dwCurrent += pdwCurrTab[iTabIdx];
				printk("GREEN curr[%d]=%d\n",iTabIdx+1,(int)(pdwCurrTab[iTabIdx]));
			}

			iRet = (int)(dwCurrent);
			printk("fl_current=%d\n",iRet);
		}

	}
	break;

	case 6:// 1 color FL @ channel 0(A).
		if(gpchip[0]->bleda->props.brightness) {
			iTabIdx = gpchip[0]->bleda->props.brightness-1;
			if(lm3630a_get_FL_RicohCurrTab(0,0,&pdwCurrTab,&dwCurrTabSize)<0) {
				printk(KERN_ERR"%s():get ricoh current table failed ! \n",__FUNCTION__);
				iRet = -4;break;
			}
			if(iTabIdx>=(int)dwCurrTabSize) {
				printk(KERN_ERR"%s():WHITE tab idx(%d) >tab size(%d) \n",__FUNCTION__,
					iTabIdx,(int)dwCurrTabSize);
				iRet = -5;break;
			}
			dwCurrent = pdwCurrTab[iTabIdx];
			iRet = (int)(dwCurrent);
			printk("WHITE curr[%d]=%d\n",iTabIdx+1,iRet);
		}
		else {
			iRet;
		}
		break;

	case 7:// 1 color FL @ channel 1(B).
		if(gpchip[0]->bledb->props.brightness) {
			iTabIdx = gpchip[0]->bledb->props.brightness-1;
			if(lm3630a_get_FL_RicohCurrTab(0,1,&pdwCurrTab,&dwCurrTabSize)<0) {
				printk(KERN_ERR"%s():get ricoh current table failed ! \n",__FUNCTION__);
				iRet = -4;break;
			}
			if(iTabIdx>=(int)dwCurrTabSize) {
				printk(KERN_ERR"%s():WHITE tab idx(%d) >tab size(%d) \n",__FUNCTION__,
					iTabIdx,(int)dwCurrTabSize);
				iRet = -5;break;
			}
			dwCurrent = pdwCurrTab[iTabIdx];
			iRet = (int)(dwCurrent);
			printk("WHITE curr[%d]=%d\n",iTabIdx+1,iRet);
		}
		else {
			iRet = 0;
		}
		break;

	case 2:// dual color FL .
		{
			int iColor=gpchip[0]->frontlight_table;
			int iPercentIdx=gpchip[0]->bled->props.brightness-1;

			if(gptLm3630fl_dualcolor_percent_tab) {
				if(gpchip[0]->bled->props.brightness>0) {
					iRet = (int)gptLm3630fl_dualcolor_percent_tab[iColor].dwCurrentA[iPercentIdx];
					printk("dualcolor[%d][%d] curr=%d\n",iColor,iPercentIdx+1,iRet);
				}
				else {
					iRet = 0;
				}
			}
			else
			{
				NTX_FW_LM3630FL_MIX2COLOR11_current_tab *ptLm3630fl_Mix2Color11_RicohCurrTab;
				ptLm3630fl_Mix2Color11_RicohCurrTab = lm3630a_get_FL_Mix2color11_RicohCurrTab();

				if(ptLm3630fl_Mix2Color11_RicohCurrTab) {
					if(gpchip[0]->bled->props.brightness>0) {
						iRet = (int) ptLm3630fl_Mix2Color11_RicohCurrTab->dwCurrentA[iColor][iPercentIdx];
						printk("Mix2Color[%d][%d] curr=%d\n",iColor,iPercentIdx+1,iRet);
					}
					else {
						//printk("Mix2Color 0%%\n");
						iRet = 0;
					}
				}
				else {
					printk(KERN_ERR"%s(): Mix2Color11 table not exist !\n",__FUNCTION__);
				}
			}
		}
		break;
	case 4:// 4 color FL on msp430 & lm3630 .
	default :
		break;
	}

	if(iRet<0) {
		printk(KERN_ERR"%s():curr table not avalible(%d) !!\n",__FUNCTION__,iRet);
	}

	return iRet;
}


static void _lm3630a_set_FL (struct lm3630a_chip *pchip,
		unsigned char led_A_current, unsigned char led_A_brightness,
		unsigned char led_B_current, unsigned char led_B_brightness)
{
	int ret;
	int iRampLvl;
	int iZeroA=0;
	int iZeroB=0;

	unsigned char bLedA_Enable,bLedB_Enable;
	printk ("[%s-%d] led A c=%d,b=%d, led B c=%d,b=%d\n", __func__,__LINE__,
		 led_A_current,led_A_brightness,led_B_current,led_B_brightness);
	
	if (gpio_is_valid(pchip->hwen_gpio)) {
		if ((led_A_current | led_B_current) && (!fl_en_status)) {
			printk("%s():FL PWR[ON] \n",__FUNCTION__);
			gpio_direction_output(pchip->hwen_gpio,1);
			fl_en_status = 1;
			msleep(50);
			lm3630a_chip_init(pchip);
		}
		else if (!led_A_current && !led_B_current && fl_en_status) {
			printk("%s():FL PWR[OFF] \n",__FUNCTION__);
			gpio_direction_output(pchip->hwen_gpio,0);
			fl_en_status = 0;
			msleep(50);
		}
	}
	
	iRampLvl=pchip->bOnOffRamp&0x7;
	if((int)((pchip->bOnOffRamp>>3)&0x7)>iRampLvl) {
		iRampLvl = (int)((pchip->bOnOffRamp>>3)&0x7);
	}
	if((int)(pchip->bRunRamp&0x7)>iRampLvl) {
		iRampLvl = (int)(pchip->bRunRamp&0x7);
	}
	if((int)((pchip->bRunRamp>>3)&0x7)>iRampLvl) {
		iRampLvl = (int)((pchip->bRunRamp>>3)&0x7);
	}

	/* disable sleep */
	ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	usleep_range(1000, 2000);

	if(255==led_A_current) {
		led_A_current = 0 ;
		iZeroA = 1;
	}
	if(255==led_B_current) {
		led_B_current = 0 ;
		iZeroB = 1;
	}

	/* minimum brightness is 0x04 */
	ret = lm3630a_write(pchip, REG_BRT_A, led_A_brightness);
	pchip->bleda->props.brightness = led_A_brightness;
	if (0x20 > led_A_current) {
		ret |= lm3630a_update(pchip, REG_I_A, 0x1F, led_A_current);
		pchip->bleda->props.power = led_A_current;
	}

	/* minimum brightness is 0x04 */
	ret = lm3630a_write(pchip, REG_BRT_B, led_B_brightness);
	pchip->bledb->props.brightness = led_B_brightness;
	if (0x20 > led_B_current) {
		ret |= lm3630a_write(pchip, REG_I_B, led_B_current);
		pchip->bledb->props.power = led_B_current;
	}
	if(0==led_A_current && iZeroA==0) {
		bLedA_Enable=LM3630A_LEDA_DISABLE;
	}
	else {
		bLedA_Enable=LM3630A_LEDA_ENABLE;
	}
	if(0==led_B_current && iZeroB==0) {
		bLedB_Enable=LM3630A_LEDB_DISABLE;
	}
	else {
		bLedB_Enable=LM3630A_LEDB_ENABLE;
	}

	if (bLedA_Enable|bLedB_Enable) {
		lm3630a_update(pchip, REG_CTRL,
			LM3630A_LEDA_ENABLE|LM3630A_LEDB_ENABLE, bLedA_Enable|bLedB_Enable);
		
	}
	else {
		// Enter sleep mode if both led are disabled.
		ret = lm3630a_update(pchip, REG_CTRL, 0x80|LM3630A_LEDA_ENABLE|LM3630A_LEDB_ENABLE, 0x80);
	}


	pchip->ulFLRampWaitTick = jiffies+gdwRamp_lvl_wait_ticksA[iRampLvl];
}

int lm3630a_get_FL_EX (int iChipIdx,
		unsigned char *O_pbLed_A_current, unsigned char *O_pbLed_A_brightness,
		unsigned char *O_pbLed_B_current, unsigned char *O_pbLed_B_brightness)
{
	if(gilm3630a_chips>iChipIdx) {
		if(O_pbLed_A_current) {
			*O_pbLed_A_current = gpchip[iChipIdx]->bleda->props.power;
		}
		if(O_pbLed_B_current) {
			*O_pbLed_B_current = gpchip[iChipIdx]->bledb->props.power;
		}
		if(O_pbLed_A_brightness) {
			*O_pbLed_A_brightness = gpchip[iChipIdx]->bleda->props.brightness;
		}
		if(O_pbLed_B_brightness) {
			*O_pbLed_B_brightness = gpchip[iChipIdx]->bledb->props.brightness;
		}
		return 0;
	}
	else {
		printk("%s():index(%d) error !!\n",__FUNCTION__,iChipIdx);
		return -1;
	}

}
void lm3630a_set_FL_EX (int iChipIdx,
		unsigned char led_A_current, unsigned char led_A_brightness,
		unsigned char led_B_current, unsigned char led_B_brightness)
{
	if(gilm3630a_chips>iChipIdx) {
		_lm3630a_set_FL (gpchip[iChipIdx],led_A_current,led_A_brightness,
				led_B_current,led_B_brightness);
	}
	else {
		printk("%s():index(%d) error !!\n",__FUNCTION__,iChipIdx);
	}
}
void lm3630a_set_FL (
		unsigned char led_A_current, unsigned char led_A_brightness,
		unsigned char led_B_current, unsigned char led_B_brightness)
{
	lm3630a_set_FL_EX (0,led_A_current,led_A_brightness,
			led_B_current,led_B_brightness);
}


// dual color fl percentage control .
static int _fl_lm3630a_percentage (struct lm3630a_chip *pchip,int iFL_Percentage)
{
	int iFL_temp = pchip->frontlight_table;

	printk("%s(%d) %s(%d)\n",__FUNCTION__,__LINE__,__FUNCTION__,iFL_Percentage);
	if (0 == iFL_Percentage) {
		_lm3630a_set_FL (pchip ,0, 0, 0, 0);
	}
	else if(gptLm3630fl_dualcolor_percent_tab) {
		unsigned char led_A_current=4,led_B_current=7;
		
		if ((int)gptLm3630fl_dualcolor_tab_hdr->dwTotalColors <= iFL_temp) {
			printk ("[%s-%d] Front light color index %d >= %d\n", __func__,__LINE__, 
					(int)gptLm3630fl_dualcolor_tab_hdr->dwTotalColors,iFL_temp);
			return -1;
		}
		
		if(gptLm3630fl_dualcolor_percent_tab[iFL_temp].bC1_CurrentA[iFL_Percentage-1]!=0) {
			led_A_current = gptLm3630fl_dualcolor_percent_tab[iFL_temp].bC1_CurrentA[iFL_Percentage-1];
		}
		else if(gptLm3630fl_dualcolor_percent_tab[iFL_temp].bDefaultC1_Current!=0) {
			led_A_current = gptLm3630fl_dualcolor_percent_tab[iFL_temp].bDefaultC1_Current;
		}
		else if(gptLm3630fl_dualcolor_tab_hdr->bDefaultC1_Current!=0) {
			led_A_current = gptLm3630fl_dualcolor_tab_hdr->bDefaultC1_Current;
		}

		if(gptLm3630fl_dualcolor_percent_tab[iFL_temp].bC2_CurrentA[iFL_Percentage-1]!=0) {
			led_B_current = gptLm3630fl_dualcolor_percent_tab[iFL_temp].bC2_CurrentA[iFL_Percentage-1];
		}
		else if(gptLm3630fl_dualcolor_percent_tab[iFL_temp].bDefaultC2_Current!=0) {
			led_B_current = gptLm3630fl_dualcolor_percent_tab[iFL_temp].bDefaultC2_Current;
		}
		else if(gptLm3630fl_dualcolor_tab_hdr->bDefaultC2_Current!=0) {
			led_B_current = gptLm3630fl_dualcolor_tab_hdr->bDefaultC2_Current;
		}

		_lm3630a_set_FL (pchip ,
			led_A_current, 
			gptLm3630fl_dualcolor_percent_tab[iFL_temp].bC1_BrightnessA[iFL_Percentage-1],
			led_B_current,
			gptLm3630fl_dualcolor_percent_tab[iFL_temp].bC2_BrightnessA[iFL_Percentage-1]);
	}
	else {
		printk (KERN_ERR"[%s-%d] Front light table not exist !!\n", __func__,__LINE__);
		return -1;
	}
	pchip->bled->props.brightness = iFL_Percentage;
	return 0;
}

int fl_lm3630a_percentageEx (int iChipIdx,int iFL_Percentage)
{
	if(gilm3630a_chips>iChipIdx) {

		if(5==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) {

			if(gpchip[iChipIdx]->pbPercentDutyTableB && gpchip[iChipIdx]->pbPercentDutyTableA) {
				unsigned char bCurrA,bBrigA,bCurrB,bBrigB;

				lm3630a_get_FL_EX (iChipIdx,&bCurrA, &bBrigA, &bCurrB, &bBrigB);

				do {
					if(iFL_Percentage<0) {
						printk(KERN_WARNING"\n\n[WARNING]%s() percent cannot <0\n\n",__FUNCTION__);
						break;
					}
					if(iFL_Percentage>100) {
						printk(KERN_WARNING"\n\n[WARNING]%s() percent cannot >100\n\n",__FUNCTION__);
						break;
					}
					if(6==gptHWCFG->m_val.bFL_PWM) {
						// 1 color ,white FL @ channel A .
						if(0==iFL_Percentage) {
							lm3630a_set_FL_EX (0,0,0,0,0);
						}
						else {
							lm3630a_set_FL_EX (0,gpchip[iChipIdx]->bDefaultCurrentA,gpchip[iChipIdx]->pbPercentDutyTableA[iFL_Percentage-1],0,0);
						}
					}
					else if(7==gptHWCFG->m_val.bFL_PWM){
						// 1 color ,white FL @ channel B .
						if(0==iFL_Percentage) {
							lm3630a_set_FL_EX (0,0,0,0,0);
						}
						else {
							lm3630a_set_FL_EX (0,0,0,gpchip[iChipIdx]->bDefaultCurrentB,gpchip[iChipIdx]->pbPercentDutyTableB[iFL_Percentage-1]);
						}
					}
					else if(5==gptHWCFG->m_val.bFL_PWM){
						// 4 color ,white FL @ channel B .
						if(0==iFL_Percentage) {
							lm3630a_set_FL_EX (iChipIdx,bCurrA,bBrigA,0,0);
						}
						else {
							lm3630a_set_FL_EX (iChipIdx,bCurrA,bBrigA,
								gpchip[iChipIdx]->bDefaultCurrentB,gpchip[iChipIdx]->pbPercentDutyTableB[iFL_Percentage-1]);
						}
					}
				}while(0);
			}
			else {
				printk(KERN_WARNING"\n\n[WARNING] %s percent/duty table not avaliable !!!\n\n",__FUNCTION__);
			}
		}
		else {
			return _fl_lm3630a_percentage(gpchip[iChipIdx],iFL_Percentage);
		}
	}
	else {
		printk("%s():index(%d) error !!\n",__FUNCTION__,iChipIdx);
		return 0;
	}
}
int fl_lm3630a_percentage (int iFL_Percentage)
{
	if(5==gptHWCFG->m_val.bFL_PWM) {
		// white FL controlled by Chip2 .
		return fl_lm3630a_percentageEx (1,iFL_Percentage);
	}
	else {
		// 
		return fl_lm3630a_percentageEx (0,iFL_Percentage);
	}
}



static int _fl_lm3630a_set_color (struct lm3630a_chip *pchip,int iFL_color)
{
	if (gptLm3630fl_dualcolor_percent_tab) {
		if ((int)gptLm3630fl_dualcolor_tab_hdr->dwTotalColors <= iFL_color) {
			printk ("[%s-%d] Front light color index %d >= %d\n", 
					__func__,__LINE__, iFL_color,(int)gptLm3630fl_dualcolor_tab_hdr->dwTotalColors);
			return -1;
		}
	}
	else {
		printk (KERN_ERR"[%s-%d] Front light table not exist !!\n", __func__,__LINE__);
		return -1;
	}
	pchip->frontlight_table = iFL_color;
	_fl_lm3630a_percentage (pchip,pchip->bled->props.brightness);
	return 0;
}

int fl_lm3630a_set_colorEx (int iChipIdx,int iFL_color)
{
	if(gilm3630a_chips>iChipIdx) {
		return _fl_lm3630a_set_color(gpchip[iChipIdx],iFL_color);
	}
	else {
		printk(KERN_ERR"%s():index(%d) error !!\n",__FUNCTION__,iChipIdx);
		return 0;
	}
}

int fl_lm3630a_set_color (int iFL_color)
{
	return fl_lm3630a_set_colorEx(0,iFL_color);
}

/* update and get brightness */
static int lm3630a_update_status(struct backlight_device *bl)
{
	struct lm3630a_chip *pchip=bl_get_data(bl);

	printk("%s(%d) %s(%d)\n",__FILE__,__LINE__,__FUNCTION__,(int)bl->props.brightness);
	_fl_lm3630a_percentage (pchip,bl->props.brightness);
	return bl->props.brightness;
}

static int lm3630a_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops lm3630a_ops = {
	.update_status = lm3630a_update_status,
	.get_brightness = lm3630a_get_brightness,
};

static ssize_t led_color_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);

	sprintf (buf, "%d", pchip->frontlight_table);
	return strlen(buf);
}

static ssize_t led_color_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int val = simple_strtoul (buf, NULL, 10);
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);


	pchip->frontlight_table = val;
	_fl_lm3630a_percentage (pchip, pchip->bled->props.brightness);

	return count;
}

static ssize_t led_max_color_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int iColors;
	if (gptLm3630fl_dualcolor_percent_tab) {
		iColors = (int)gptLm3630fl_dualcolor_tab_hdr->dwTotalColors ;
	}
	else {
		iColors = 1 ;
	}
	sprintf (buf, "%d", (iColors-1));
	return strlen(buf);
}

static ssize_t led_max_color_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	return count;
}

static ssize_t lm3630_ramp_on_get(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	sprintf(buf,"%d\n",(int)(pchip->bOnOffRamp>>3));
	return strlen(buf);
}
static ssize_t lm3630_ramp_on_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = simple_strtoul(buf,NULL,10);
	unsigned char bTemp;

	if(val<0) {
		return count;
	}
	if(val>7) {
		return count;
	}
	
	bTemp = ~(unsigned char)(0x7<<3);
	pchip->bOnOffRamp &= 0x7<<3;
	pchip->bOnOffRamp |= val<<3;
	lm3630a_write(pchip,REG_ONOFF_RAMP,pchip->bOnOffRamp);
	return count;
}
static ssize_t lm3630_ramp_off_get(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	sprintf(buf,"%d\n",(int)(pchip->bOnOffRamp&0x7));
	return strlen(buf);
}
static ssize_t lm3630_ramp_off_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = simple_strtoul(buf,NULL,10);
	unsigned char bTemp;

	if(val<0) {
		return count;
	}
	if(val>7) {
		return count;
	}
	
	bTemp = ~0x7;
	pchip->bOnOffRamp &= bTemp;
	pchip->bOnOffRamp |= val;
	lm3630a_write(pchip,REG_ONOFF_RAMP,pchip->bOnOffRamp);
	return count;
}
static ssize_t lm3630_ramp_up_get(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	sprintf(buf,"%d\n",(int)(pchip->bRunRamp>>3));
	return strlen(buf);
}
static ssize_t lm3630_ramp_up_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = simple_strtoul(buf,NULL,10);
	unsigned char bTemp;

	if(val<0) {
		return count;
	}
	if(val>7) {
		return count;
	}
	
	bTemp = ~(unsigned char)(0x7<<3);
	pchip->bRunRamp &= 0x7<<3;
	pchip->bRunRamp |= val<<3;
	lm3630a_write(pchip,REG_RUN_RAMP,pchip->bRunRamp);
	return count;
}
static ssize_t lm3630_ramp_down_get(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	sprintf(buf,"%d\n",(int)(pchip->bRunRamp&0xf));
	return strlen(buf);
}
static ssize_t lm3630_ramp_down_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = simple_strtoul(buf,NULL,10);
	unsigned char bTemp;

	if(val<0) {
		return count;
	}
	if(val>7) {
		return count;
	}
	
	bTemp = ~0x7;
	pchip->bRunRamp &= bTemp;
	pchip->bRunRamp |= val;
	lm3630a_write(pchip,REG_RUN_RAMP,pchip->bRunRamp);
	return count;
}

static ssize_t lm3630_ramp_get(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	sprintf(buf,"%d",pchip->bOnOffRamp>>3);
	return strlen(buf);
}
static ssize_t lm3630_ramp_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int val = simple_strtoul(buf,NULL,10);

	if(val<0) {
		return count;
	}
	if(val>7) {
		return count;
	}
	
	pchip->bOnOffRamp = val<<3|val;
	pchip->bRunRamp = val<<3|val;
	lm3630a_write(pchip,REG_ONOFF_RAMP,pchip->bOnOffRamp);
	lm3630a_write(pchip,REG_RUN_RAMP,pchip->bRunRamp);
	return count;
}

static ssize_t lm3630_regs_read(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct lm3630a_chip *pchip = dev_get_drvdata(dev);
	int iReg_BRT_A ;
	int iReg_BRT_B ;
	int iReg_Control ;
	int iReg_Configuration ;
	int iReg_BoostCtrl ;
	int iReg_CurrentA ;
	int iReg_CurrentB ;
	int iReg_OnOffRamp ;
	int iReg_RunRamp ;
	int iReg_PWMOutLow ;
	int iReg_PWMOutHigh ;
	int iReg_Rev ;
	int iReg_FilterStrength ;
	int iReg_IntStatus ;
	int iReg_Fault ;

	iReg_BRT_A = lm3630a_read(pchip, REG_BRT_A);
	iReg_BRT_B = lm3630a_read(pchip, REG_BRT_B);
	iReg_Control = lm3630a_read(pchip, REG_CTRL);
	iReg_Configuration = lm3630a_read(pchip, REG_CONFIG);
	iReg_BoostCtrl = lm3630a_read(pchip, REG_BOOST);
	iReg_CurrentA = lm3630a_read(pchip, REG_I_A);
	iReg_CurrentB = lm3630a_read(pchip, REG_I_B);
	iReg_OnOffRamp = lm3630a_read(pchip, REG_ONOFF_RAMP);
	iReg_RunRamp = lm3630a_read(pchip, REG_RUN_RAMP);
	iReg_PWMOutLow = lm3630a_read(pchip, REG_PWM_OUTLOW);
	iReg_PWMOutHigh = lm3630a_read(pchip, REG_PWM_OUTHIGH);
	iReg_Rev = lm3630a_read(pchip, REG_REV);
	iReg_FilterStrength = lm3630a_read(pchip, REG_FILTER_STRENGTH);
	iReg_IntStatus = lm3630a_read(pchip, REG_INT_STATUS);
	iReg_Fault = lm3630a_read(pchip, REG_FAULT);

	sprintf(buf,"\
Revision=0x%x\n\
Control=0x%x\n\
Configuration=0x%x\n\
Boost Control=0x%x\n\
CurrentA=0x%x\n\
CurrentB=0x%x\n\
Brightness A=0x%x\n\
Brightness B=0x%x\n\
On/Off Ramp=0x%x\n\
Run Ramp=0x%x\n\
PWM Out Low=0x%x\n\
PWM Out High=0x%x\n\
Filter Strength=0x%x\n\
Int Status=0x%x\n\
Fault=0x%x\n\
",\
iReg_Rev,\
iReg_Control,\
iReg_Configuration,\
iReg_BoostCtrl,\
iReg_CurrentA,\
iReg_CurrentB,\
iReg_BRT_A,\
iReg_BRT_B,\
iReg_OnOffRamp,\
iReg_RunRamp,\
iReg_PWMOutLow,\
iReg_PWMOutHigh,\
iReg_FilterStrength,\
iReg_IntStatus,\
iReg_Fault);

	return strlen(buf);
}

static ssize_t lm3630_regs_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR (color, 0644, led_color_get, led_color_set);
static DEVICE_ATTR (max_color, 0644, led_max_color_get, led_max_color_set);
static DEVICE_ATTR (regs, 0644, lm3630_regs_read, lm3630_regs_write);
static DEVICE_ATTR (ramp, 0644, lm3630_ramp_get, lm3630_ramp_set);
static DEVICE_ATTR (ramp_on, 0644, lm3630_ramp_on_get, lm3630_ramp_on_set);
static DEVICE_ATTR (ramp_off, 0644, lm3630_ramp_off_get, lm3630_ramp_off_set);
static DEVICE_ATTR (ramp_up, 0644, lm3630_ramp_up_get, lm3630_ramp_up_set);
static DEVICE_ATTR (ramp_down, 0644, lm3630_ramp_down_get, lm3630_ramp_down_set);

static int lm3630a_backlight_register(struct lm3630a_chip *pchip,int iChipIdx)
{
	struct backlight_properties props;
	struct lm3630a_platform_data *pdata = pchip->pdata;
	const char szDriverBaseName[]="lm3630a_led";
	char cDriverNameA[64]="";
	extern int ntx_set_main_bldev(struct backlight_device *main_bldev);
	int rval;

	props.type = BACKLIGHT_RAW;
	if (pdata->leda_ctrl != LM3630A_LEDA_DISABLE) {
		props.brightness = pdata->leda_init_brt;
		props.max_brightness = pdata->leda_max_brt;
		props.power = pdata->leda_full_scale;
		if(0==iChipIdx) {
			sprintf(cDriverNameA,"%sa",szDriverBaseName);
		}
		else {
			sprintf(cDriverNameA,"%s%da",szDriverBaseName,iChipIdx);
		}
		pchip->bleda =
		    devm_backlight_device_register(pchip->dev, cDriverNameA,
						   pchip->dev, pchip,
						   &lm3630a_bank_a_ops, &props);
		if (IS_ERR(pchip->bleda))
			return PTR_ERR(pchip->bleda);

		if(gptHWCFG && 6==gptHWCFG->m_val.bFL_PWM) {
			ntx_set_main_bldev(pchip->bleda);
		}

#if 1 // disabled experimenting function .
		rval = device_create_file(&pchip->bleda->dev, &dev_attr_percent);
		if (rval < 0) {
			dev_err(&pchip->bleda->dev, "fail : backlight percent create.\n");
			return rval;
		}
#endif

	}

	if ((pdata->ledb_ctrl != LM3630A_LEDB_DISABLE) &&
	    (pdata->ledb_ctrl != LM3630A_LEDB_ON_A)) {
		props.brightness = pdata->ledb_init_brt;
		props.max_brightness = pdata->ledb_max_brt;
		props.power = pdata->ledb_full_scale;
		if(0==iChipIdx) {
			sprintf(cDriverNameA,"%sb",szDriverBaseName);
		}
		else {
			sprintf(cDriverNameA,"%s%db",szDriverBaseName,iChipIdx);
		}
		pchip->bledb =
		    devm_backlight_device_register(pchip->dev, cDriverNameA,
						   pchip->dev, pchip,
						   &lm3630a_bank_b_ops, &props);
		if (IS_ERR(pchip->bledb))
			return PTR_ERR(pchip->bledb);

		if(gptHWCFG && 7==gptHWCFG->m_val.bFL_PWM) {
			ntx_set_main_bldev(pchip->bledb);
		}

#if 1 // disabled experimenting function .
		rval = device_create_file(&pchip->bledb->dev, &dev_attr_b_percent);
		if (rval < 0) {
			dev_err(&pchip->bledb->dev, "fail : backlight percent create.\n");
			return rval;
		}
#endif
	}

	props.brightness = 0;
	props.max_brightness = 100;
	if(0==iChipIdx) {
		sprintf(cDriverNameA,"%s",szDriverBaseName);
	}
	else {
		sprintf(cDriverNameA,"%s%d",szDriverBaseName,iChipIdx);
	}
	pchip->bled =
#if 0
			devm_backlight_device_register(pchip->dev,cDriverNameA, pchip->dev, pchip,
		       0, &props);
#else
			devm_backlight_device_register(pchip->dev,cDriverNameA, pchip->dev, pchip,
		       &lm3630a_ops, &props);
#endif

	if (IS_ERR(pchip->bled)) {
		dev_err(&pchip->bled->dev, "fail : \"lm3639a_led\" register .\n");
		return PTR_ERR(pchip->bled);
	}

	rval = device_create_file(&pchip->bled->dev, &dev_attr_color);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : backlight color create.\n");
		return rval;
	}
	rval = device_create_file(&pchip->bled->dev, &dev_attr_max_color);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : backlight max_color create.\n");
		return rval;
	}
	rval = device_create_file(&pchip->bled->dev, &dev_attr_regs);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : lm3630 regs create.\n");
		return rval;
	}
	rval = device_create_file(&pchip->bled->dev, &dev_attr_ramp);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : lm3630 ramp create.\n");
		return rval;
	}
	rval = device_create_file(&pchip->bled->dev, &dev_attr_ramp_on);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : lm3630 ramp_on create.\n");
		return rval;
	}
	rval = device_create_file(&pchip->bled->dev, &dev_attr_ramp_off);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : lm3630 ramp_off create.\n");
		return rval;
	}
	rval = device_create_file(&pchip->bled->dev, &dev_attr_ramp_up);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : lm3630 ramp_up create.\n");
		return rval;
	}
	rval = device_create_file(&pchip->bled->dev, &dev_attr_ramp_down);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : lm3630 ramp_down create.\n");
		return rval;
	}


	return 0;
}

static const struct regmap_config lm3630a_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

#ifdef CONFIG_OF
static void lm3630a_bl_parse_dt(struct lm3630a_chip *pchip)
{
	struct device_node *lm3630a_bl_pmic_np;

	lm3630a_bl_pmic_np = of_node_get(pchip->dev->of_node);
	if (!lm3630a_bl_pmic_np) {
		printk("could not find lm3630a_bl sub-node\n");
		return ;
	}

	pchip->hwen_gpio = of_get_named_gpio(lm3630a_bl_pmic_np,"gpios",0);
		if (!gpio_is_valid(pchip->hwen_gpio)) {
			printk("invalid gpio: %d\n",  pchip->hwen_gpio);
		}
	printk ("%s pchip=%p,hwen_gpio %d\n",__func__,pchip,pchip->hwen_gpio);
}

#else
void lm3630a_bl_parse_dt(struct lm3630a_chip *pchip)
{
	return NULL;
}
#endif

static int lm3630a_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct lm3630a_platform_data *pdata = dev_get_platdata(&client->dev);
	struct lm3630a_chip *pchip;
	int rval;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "fail : i2c functionality check\n");
		return -EOPNOTSUPP;
	}


	pchip = devm_kzalloc(&client->dev, sizeof(struct lm3630a_chip),
			     GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	pchip->dev = &client->dev;

	pchip->regmap = devm_regmap_init_i2c(client, &lm3630a_regmap);
	if (IS_ERR(pchip->regmap)) {
		rval = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "fail : allocate reg. map: %d\n", rval);
		return rval;
	}

	if (pchip->dev->of_node) {
		lm3630a_bl_parse_dt (pchip);
	}
	if (gpio_request(pchip->hwen_gpio, "lm3630a_bl_hwen") < 0) {
		dev_err(pchip->dev,"Failed to request gpio %d .\n", pchip->hwen_gpio);
	}
	else {
		gpio_direction_output(pchip->hwen_gpio,1);
		fl_en_status = 1;
	}
	giFL_PWR_state_max+=2;
	giFL_PWR_state+=2;



	i2c_set_clientdata(client, pchip);
	if (pdata == NULL) {
		pdata = devm_kzalloc(pchip->dev,
				     sizeof(struct lm3630a_platform_data),
				     GFP_KERNEL);
		if (pdata == NULL) {
			dev_err(&client->dev, "%s : fail : memory not enought \n",__FUNCTION__);
			return -ENOMEM;
		}
		/* default values */
		pdata->leda_ctrl = LM3630A_LEDA_ENABLE;
		pdata->ledb_ctrl = LM3630A_LEDB_ENABLE;
		pdata->leda_max_brt = LM3630A_MAX_BRIGHTNESS;
		pdata->ledb_max_brt = LM3630A_MAX_BRIGHTNESS;
		pdata->leda_init_brt = 0;
		pdata->ledb_init_brt = 0;
		pdata->leda_full_scale = LM3630A_DEF_FULLSCALE;
		pdata->ledb_full_scale = LM3630A_DEF_FULLSCALE;
		pdata->pwm_ctrl = LM3630A_PWM_DISABLE;
	}
	pchip->pdata = pdata;

	if(5==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) {
		// 4 color FL controlled by 2 lm3630a .
		pchip->pbPercentDutyTableA = lm3630a_get_FL_W_duty_table(gptHWCFG->m_val.bFrontLight,&pchip->iPercentDutyTableASize);
		pchip->pbPercentDutyTableB = lm3630a_get_FL_W_duty_table(gptHWCFG->m_val.bFrontLight,&pchip->iPercentDutyTableBSize);
		lm3630a_get_default_power_by_table(gptHWCFG->m_val.bFrontLight,&pchip->bDefaultCurrentA);
		lm3630a_get_default_power_by_table(gptHWCFG->m_val.bFrontLight,&pchip->bDefaultCurrentB);
	}

	/* backlight register */
	rval = lm3630a_backlight_register(pchip,gilm3630a_chips);
	if (rval < 0) {
		dev_err(&client->dev, "fail : backlight register.\n");
		return rval;
	}
	/* pwm */
	if (pdata->pwm_ctrl != LM3630A_PWM_DISABLE) {
		pchip->pwmd = devm_pwm_get(pchip->dev, "lm3630a-pwm");
		if (IS_ERR(pchip->pwmd)) {
			dev_err(&client->dev, "fail : get pwm device\n");
			return PTR_ERR(pchip->pwmd);
		}
		pchip->pwmd->period = pdata->pwm_period;
	}
	/* chip initialize */
	if(40==gptHWCFG->m_val.bPCB||50==gptHWCFG->m_val.bPCB) {
		pchip->bOnOffRamp = 3<<3 | 3;
		pchip->bRunRamp = 3<<3 | 3;
	}
	else {
		pchip->bOnOffRamp = DEFAULT_ON_RAMP_LVL<<3 | DEFAULT_OFF_RAMP_LVL;
		pchip->bRunRamp = DEFAULT_UP_RAMP_LVL<<3 | DEFAULT_DN_RAMP_LVL;
	}
	rval = lm3630a_chip_init(pchip);
	if (rval < 0) {
		dev_err(&client->dev, "fail : init chip\n");
		return rval;
	}

	gpchip[gilm3630a_chips++] = pchip;

	/* interrupt enable  : irq 0 is not allowed */
	pchip->irq = client->irq;
	if (pchip->irq) {
		rval = lm3630a_intr_config(pchip);
		if (rval < 0) {
			dev_err(&client->dev, "fail : initr config failed ! \n");
			return rval;
		}
	}

	dev_info(&client->dev, "LM3630A[%d] backlight register OK.\n",gilm3630a_chips);
	return 0;
}

static int lm3630a_remove(struct i2c_client *client)
{
	int rval;
	struct lm3630a_chip *pchip = i2c_get_clientdata(client);

	gpchip[--gilm3630a_chips] = 0;

	rval = lm3630a_write(pchip, REG_BRT_A, 0);
	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");

	rval = lm3630a_write(pchip, REG_BRT_B, 0);
	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");

	if (pchip->irq) {
		free_irq(pchip->irq, pchip);
		flush_workqueue(pchip->irqthread);
		destroy_workqueue(pchip->irqthread);
	}
	return 0;
}

static const struct i2c_device_id lm3630a_id[] = {
	{LM3630A_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3630a_id);

#ifdef CONFIG_OF
static const struct of_device_id lm3630a_bl_dt_match[] = {
	{ .compatible = "ti,lm3630a_bl", },
	{},
};
MODULE_DEVICE_TABLE(of, lm3630a_bl_dt_match);
#endif


static struct i2c_driver lm3630a_i2c_driver = {
	.driver = {
		   .name = LM3630A_NAME,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(lm3630a_bl_dt_match),
#endif
		   },
	.probe = lm3630a_probe,
	.remove = lm3630a_remove,
	.id_table = lm3630a_id,
};

module_i2c_driver(lm3630a_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments Backlight driver for LM3630A");
MODULE_AUTHOR("Daniel Jeong <gshark.jeong@gmail.com>");
MODULE_AUTHOR("LDD MLP <ldd-mlp@list.ti.com>");
MODULE_LICENSE("GPL v2");

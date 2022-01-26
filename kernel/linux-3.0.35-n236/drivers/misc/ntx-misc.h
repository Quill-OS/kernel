/* ntx-misc.h
 *
 * Copyright (C) 2012 Netronix, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#ifndef _NTX_MISC_H //[
#define _NTX_MISC_H

struct ntx_misc_platform_data {
	unsigned	acin_gpio;
	unsigned 	chg_gpio;
};


void msp430_homepad_enable(int iEnable);

#define MSP430_HOMELED_TYPE_NORMAL			0
#define MSP430_HOMELED_TYPE_PWM					1
int msp430_homeled_type_set(int iHomeLedType);
int msp430_homeled_type_set_by_name(const char *I_pszHomeLedTypeName);
int msp430_homeled_type_get(char **O_ppszHomeLedStr);

void msp430_homeled_enable(int iEnable);

int msp430_set_homeled_delayms(int iDelayms);

int msp430_set_homeled_pwm_delaylevel(int iDelayLevel);
int msp430_get_homeled_pwm_delaylevel();

int msp430_set_homeled_gpio_delaylevel(int iDelayLevel);
int msp430_get_homeled_gpio_delaylevel(void);

int msp430_homepad_sensitivity_set(unsigned char bVal);
int msp430_homepad_sensitivity_get(void);

#define MSP430_FL_IDX_ALL		(-1)
#define MSP430_FL_IDX_W			(0)
#define MSP430_FL_IDX_R			(1)
int msp430_fl_set_duty(int iColorIDX,int iDuty);
int msp430_fl_get_duty(int iColorIDX);
int msp430_fl_get_duty_max(int iColorIDX);
int msp430_fl_get_duty_min(int iColorIDX);
int msp430_fl_set_freq(int iColorIDX,int iFreq);
int msp430_fl_get_freq(int iColorIDX);
int msp430_fl_get_freq_max(int iColorIDX);
int msp430_fl_get_freq_min(int iColorIDX);
int msp430_fl_enable(int iColorIDX,int iIsEnable);
int msp430_fl_is_enable(int iColorIDX);
#endif //] _NTX_MISC_H


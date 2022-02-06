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

void msp430_homepad_sensitivity_set(unsigned char bVal);

#endif //] _NTX_MISC_H


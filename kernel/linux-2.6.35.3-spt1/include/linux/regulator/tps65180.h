/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#ifndef __LINUX_REGULATOR_TPS65180_H_
#define __LINUX_REGULATOR_TPS65180_H_

/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
enum {
    /* In alphabetical order */
    TPS65180_DISPLAY, /* virtual master enable */
    TPS65180_VCOM,
    TPS65180_V3P3,
    TPS65180_TEMP,
    EPD_PWR0_CTRL,
    EPD_PWR2_CTRL,
    TPS65180_VSYS,
    TPS65180_NUM_REGULATORS,
};

/*
 * Declarations
 */
struct regulator_init_data;

struct tps65180_platform_data {
	unsigned int gvee_pwrup;
	unsigned int vneg_pwrup;
	unsigned int vpos_pwrup;
	unsigned int gvdd_pwrup;
	unsigned int gvdd_pwrdn;
	unsigned int vpos_pwrdn;
	unsigned int vneg_pwrdn;
	unsigned int gvee_pwrdn;
	int gpio_pmic_pwrgood;
	int gpio_pmic_vcom_ctrl;
	int gpio_pmic_wakeup;
	int gpio_pmic_intr;
/* 2011/05/26 FY11 : Supported TPS65181. */
	int gpio_pmic_v3p3_ctrl;
	int gpio_pmic_pwr2_ctrl;
	int gpio_pmic_pwr0_ctrl;
/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
	int gpio_pmic_vsys;
	int pass_num;
	int vcom_uV;
	struct regulator_init_data *regulator_init;
};

#endif

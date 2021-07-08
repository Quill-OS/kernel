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
#ifndef __LINUX_REGULATOR_FP9928_H_
#define __LINUX_REGULATOR_FP9928_H_

/*
 * PMIC Register Addresses
 */
enum {
    REG_FP9928_TMST_VAL = 0x0,
    REG_FP9928_VADJ,
    REG_FP9928_VCOM,
    FP9928_REG_NUM,
};

#define FP9928_MAX_REGISTER   0xFF

/*
 * Bitfield macros that use rely on bitfield width/shift information.
 */
#define BITFMASK(field) (((1U << (field ## _WID)) - 1) << (field ## _LSH))
#define BITFVAL(field, val) ((val) << (field ## _LSH))
#define BITFEXT(var, bit) ((var & BITFMASK(bit)) >> (bit ## _LSH))

/*
 * Shift and width values for each register bitfield
 */
/* TMST_VALUE */
#define TMST_VALUE_LSH  0
#define TMST_VALUE_WID  8

/*
 * VCOM Definitions
 *
 * The register fields accept voltages in the range 0V to -2.75V, but the
 * VCOM parametric performance is only guaranteed from -0.3V to -2.5V.
 */
#define FP9928_VCOM_MIN_uV   -5002000
#define FP9928_VCOM_MAX_uV    -604000
#define FP9928_VCOM_MIN_SET        28
#define FP9928_VCOM_MAX_SET       232
#define FP9928_VCOM_STEP_uV     21570

#define FP9928_VCOM_MIN_VAL      0x1B
#define FP9928_VCOM_MAX_VAL      0xE8

struct regulator_init_data;

struct fp9928 {
	struct device *dev;
	struct fp9928_platform_data *pdata;

	/* Platform connection */
	struct i2c_client *i2c_client;

	/* GPIOs */
	int gpio_pmic_pwrgood;
	int gpio_pmic_vcom_ctrl;
	int gpio_pmic_powerup;
	int gpio_pmic_pwrall;

	/* FP9928 part variables */
	int vcom_uV;

	/* One-time VCOM setup marker */
	bool vcom_setup;

	/* powerup/powerdown wait time */
	int max_wait;

	/* Dynamically determined polarity for PWRGOOD */
	int pwrgood_polarity;
};

enum {
    /* In alphabetical order */
    FP9928_DISPLAY, /* virtual master enable */
    FP9928_VCOM,
    FP9928_V3P3,
    FP9928_TMST,
    FP9928_NUM_REGULATORS,
};

/*
 * Declarations
 */
struct regulator_init_data;
struct fp9928_regulator_data;

struct fp9928_platform_data {
	int gpio_pmic_pwrgood;
	int gpio_pmic_vcom_ctrl;
	int gpio_pmic_powerup;
	int gpio_pmic_pwrall;
	int vcom_uV;

	/* PMIC */
	struct fp9928_regulator_data *regulators;
	int num_regulators;
};

struct fp9928_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

int fp9928_reg_write(struct fp9928 *fp9928,int reg_num, const unsigned int reg_val);
int fp9928_reg_read(struct fp9928 *fp9928,int reg_num, unsigned int *reg_val);
int fp9928_get_temperature(struct fp9928 *fp9928,int *O_piTemperature);
int fp9928_get_vcom(struct fp9928 *fp9928,int *O_piVCOMmV);
int fp9928_set_vcom(struct fp9928 *fp9928,int iVCOMmV,int iIsWriteToFlash);

#endif

// SPDX-License-Identifier: GPL-2.0-only
//
// Copyright (C) 2019 ROHM Semiconductors
//
// ROHM BD71828/BD71815 PMIC driver

#include <linux/fs.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/rohm-bd71815.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/mfd/rohm-bd71879.h>
#include <linux/mfd/rohm-generic.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <asm/mach-types.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>

static	struct rohm_regmap_dev *gptBd71828;


static struct gpio_keys_button button = {
	.code = KEY_POWER,
	.gpio = -1,
	.type = EV_KEY,
};

static struct bd71828_pwrkey_platform_data bd71828_powerkey_data = {
	.buttons = &button,
	.nbuttons = 1,
	.name = "bd71828-pwrkey",
};

static const struct resource bd71815_rtc_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD71815_INT_RTC0, "bd71815-rtc-alm-0"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_RTC1, "bd71815-rtc-alm-1"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_RTC2, "bd71815-rtc-alm-2"),
};

static const struct resource bd71828_rtc_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD71828_INT_RTC0, "bd71828-rtc-alm-0"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_RTC1, "bd71828-rtc-alm-1"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_RTC2, "bd71828-rtc-alm-2"),
};

static struct resource bd71815_power_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_RMV, "bd71815-dcin-rmv"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CLPS_OUT, "bd71815-clps-out"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CLPS_IN, "bd71815-clps-in"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_OVP_RES, "bd71815-dcin-ovp-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_OVP_DET, "bd71815-dcin-ovp-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_MON_RES, "bd71815-dcin-mon-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_MON_DET, "bd71815-dcin-mon-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_UV_RES, "bd71815-vsys-uv-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_UV_DET, "bd71815-vsys-uv-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_LOW_RES, "bd71815-vsys-low-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_LOW_DET, "bd71815-vsys-low-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_MON_RES, "bd71815-vsys-mon-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_MON_DET, "bd71815-vsys-mon-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_WDG_TEMP, "bd71815-chg-wdg-temp"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_WDG_TIME, "bd71815-chg-wdg"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_RECHARGE_RES, "bd71815-rechg-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_RECHARGE_DET, "bd71815-rechg-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_RANGED_TEMP_TRANSITION, "bd71815-ranged-temp-transit"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_STATE_TRANSITION, "bd71815-chg-state-change"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_TEMP_NORMAL, "bd71815-bat-temp-normal"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_TEMP_ERANGE, "bd71815-bat-temp-erange"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_REMOVED, "bd71815-bat-rmv"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_DETECTED, "bd71815-bat-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_THERM_REMOVED, "bd71815-therm-rmv"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_THERM_DETECTED, "bd71815-therm-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_DEAD, "bd71815-bat-dead"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_SHORTC_RES, "bd71815-bat-short-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_SHORTC_DET, "bd71815-bat-short-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_LOW_VOLT_RES, "bd71815-bat-low-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_LOW_VOLT_DET, "bd71815-bat-low-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_VOLT_RES, "bd71815-bat-over-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_VOLT_DET, "bd71815-bat-over-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_MON_RES, "bd71815-bat-mon-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_MON_DET, "bd71815-bat-mon-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_CC_MON1, "bd71815-bat-cc-mon1"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_CC_MON2, "bd71815-bat-cc-mon2"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_CC_MON3, "bd71815-bat-cc-mon3"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_1_RES, "bd71815-bat-oc1-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_1_DET, "bd71815-bat-oc1-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_2_RES, "bd71815-bat-oc2-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_2_DET, "bd71815-bat-oc2-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_3_RES, "bd71815-bat-oc3-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_3_DET, "bd71815-bat-oc3-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_TEMP_BAT_LOW_RES, "bd71815-bat-low-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_TEMP_BAT_LOW_DET, "bd71815-bat-low-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_TEMP_BAT_HI_RES, "bd71815-bat-hi-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_TEMP_BAT_HI_DET, "bd71815-bat-hi-det"),
};

static struct resource bd71828_power_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD71828_INT_DCIN_DET, "bd71828-pwr-dcin-in"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_DCIN_RMV, "bd71828-pwr-dcin-out"),
	//DEFINE_RES_IRQ_NAMED(BD71828_INT_BAT_LOW_VOLT_RES, "bd71828-vbat-normal"),
	//DEFINE_RES_IRQ_NAMED(BD71828_INT_BAT_LOW_VOLT_DET, "bd71828-vbat-low"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_BAT_MON_RES, "bd71828-vbat-mon-res"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_BAT_MON_DET, "bd71828-vbat-mon-det"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_BAT_HI_DET, "bd71828-btemp-hi"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_BAT_HI_RES, "bd71828-btemp-cool"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_BAT_LOW_DET, "bd71828-btemp-lo"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_BAT_LOW_RES, "bd71828-btemp-warm"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_CHIP_OVER_VF_DET, "bd71828-temp-hi"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_CHIP_OVER_VF_RES, "bd71828-temp-norm"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_CHIP_OVER_125_DET, "bd71828-temp-125-over"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_TEMP_CHIP_OVER_125_RES, "bd71828-temp-125-under"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_CHG_TOPOFF_TO_DONE, "bd71828-charger-done"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_CHG_RECHARGE_RES, "bd71828-rechg-res"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_CHG_RECHARGE_DET, "bd71828-rechg-det"),
	DEFINE_RES_IRQ_NAMED(BD71828_INT_CHG_WDG_TIME, "bd71828-charger-wdg-expired"),
};

static struct mfd_cell bd71815_mfd_cells[] = {
	{ .name = "bd71815-pmic", },
	{ .name = "bd71815-clk", },
	{ .name = "bd71815-gpo", },
	{
		.name = "bd71815-power",
		.num_resources = ARRAY_SIZE(bd71815_power_irqs),
		.resources = &bd71815_power_irqs[0],
	},
	{
		.name = "bd71815-rtc",
		.num_resources = ARRAY_SIZE(bd71815_rtc_irqs),
		.resources = &bd71815_rtc_irqs[0],
	},
};

static struct mfd_cell bd71828_mfd_cells[] = {
	{
		.name = "bd71828-pmic",
		.resources = NULL,
		.num_resources = 0,
	},
	//{ .name = "bd71828-gpio", },
	//{ .name = "bd71828-led", .of_compatible = "rohm,bd71828-leds" },
	/*
	 * We use BD71837 driver to drive the clock block. Only differences to
	 * BD70528 clock gate are the register address and mask.
	 */
	//{ .name = "bd71828-clk", },
	{
		.name = "bd71827-power",
		.resources = &bd71828_power_irqs,
		.num_resources = ARRAY_SIZE(bd71828_power_irqs),
	},
	{
		.name = "bd71828-rtc",
		.resources = bd71828_rtc_irqs,
		.num_resources = ARRAY_SIZE(bd71828_rtc_irqs),
	},
	{
		.name = "bd71828-pwrkey",
		.platform_data = &bd71828_powerkey_data,
		.pdata_size = sizeof(bd71828_powerkey_data),
	},
};

static const struct regmap_range bd71815_volatile_ranges[] = {
	{
		.range_min = BD71815_REG_SEC,
		.range_max = BD71815_REG_YEAR,
	}, {
		.range_min = BD71815_REG_CONF,
		.range_max = BD71815_REG_BAT_TEMP,
	}, {
		.range_min = BD71815_REG_VM_IBAT_U,
		.range_max = BD71815_REG_CC_CTRL,
	}, {
		.range_min = BD71815_REG_CC_STAT,
		.range_max = BD71815_REG_CC_CURCD_L,
	}, {
		.range_min = BD71815_REG_VM_BTMP_MON,
		.range_max = BD71815_REG_VM_BTMP_MON,
	}, {
		.range_min = BD71815_REG_INT_STAT,
		.range_max = BD71815_REG_INT_UPDATE,
	}, {
		.range_min = BD71815_REG_VM_VSYS_U,
		.range_max = BD71815_REG_REX_CTRL_1,
	}, {
		.range_min = BD71815_REG_FULL_CCNTD_3,
		.range_max = BD71815_REG_CCNTD_CHG_2,
	},
};

static const struct regmap_range bd71828_volatile_ranges[] = {
	{
		.range_min = BD71828_REG_PS_CTRL_1,
		.range_max = BD71828_REG_PS_CTRL_1,
	}, {
		.range_min = BD71828_REG_PS_CTRL_3,
		.range_max = BD71828_REG_PS_CTRL_3,
	}, {
		.range_min = BD71828_REG_RTC_SEC,
		.range_max = BD71828_REG_RTC_YEAR,
	}, {
		/*
		 * For now make all charger registers volatile because many
		 * needs to be and because the charger block is not that
		 * performance critical.
		 */
		.range_min = BD71828_REG_CHG_STATE,
		.range_max = BD71828_REG_CHG_FULL,
	}, {
		.range_min = BD71828_REG_INT_MAIN,
		.range_max = BD71828_REG_IO_STAT,
	},
};

static const struct regmap_access_table bd71815_volatile_regs = {
	.yes_ranges = &bd71815_volatile_ranges[0],
	.n_yes_ranges = ARRAY_SIZE(bd71815_volatile_ranges),
};

static const struct regmap_access_table bd71828_volatile_regs = {
	.yes_ranges = &bd71828_volatile_ranges[0],
	.n_yes_ranges = ARRAY_SIZE(bd71828_volatile_ranges),
};

static const struct regmap_config bd71815_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &bd71815_volatile_regs,
	.max_register = BD71815_MAX_REGISTER - 1,
	.cache_type = REGCACHE_RBTREE,
};

static const struct regmap_config bd71828_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &bd71828_volatile_regs,
	.max_register = BD71828_MAX_REGISTER,
	.cache_type = REGCACHE_NONE,//REGCACHE_RBTREE,
};

/*
 * Mapping of main IRQ register bits to sub-IRQ register offsets so that we can
 * access corect sub-IRQ registers based on bits that are set in main IRQ
 * register. BD71815 and BD71828 have same sub-register-block offests.
 */

static unsigned int bit0_offsets[] = {11};		/* RTC IRQ */
static unsigned int bit1_offsets[] = {10};		/* TEMP IRQ */
static unsigned int bit2_offsets[] = {6, 7, 8, 9};	/* BAT MON IRQ */
static unsigned int bit3_offsets[] = {5};		/* BAT IRQ */
static unsigned int bit4_offsets[] = {4};		/* CHG IRQ */
static unsigned int bit5_offsets[] = {3};		/* VSYS IRQ */
static unsigned int bit6_offsets[] = {1, 2};		/* DCIN IRQ */
static unsigned int bit7_offsets[] = {0};		/* BUCK IRQ */

static struct regmap_irq_sub_irq_map bd718xx_sub_irq_offsets[] = {
	REGMAP_IRQ_MAIN_REG_OFFSET(bit0_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit1_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit2_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit3_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit4_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit5_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit6_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit7_offsets),
};

static const struct regmap_irq bd71815_irqs[] = {
	REGMAP_IRQ_REG(BD71815_INT_BUCK1_OCP, 0, BD71815_INT_BUCK1_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BUCK2_OCP, 0, BD71815_INT_BUCK2_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BUCK3_OCP, 0, BD71815_INT_BUCK3_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BUCK4_OCP, 0, BD71815_INT_BUCK4_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BUCK5_OCP, 0, BD71815_INT_BUCK5_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_LED_OVP, 0, BD71815_INT_LED_OVP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_LED_OCP, 0, BD71815_INT_LED_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_LED_SCP, 0, BD71815_INT_LED_SCP_MASK),
	/* DCIN1 interrupts */
	REGMAP_IRQ_REG(BD71815_INT_DCIN_RMV, 1, BD71815_INT_DCIN_RMV_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CLPS_OUT, 1, BD71815_INT_CLPS_OUT_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CLPS_IN, 1, BD71815_INT_CLPS_IN_MASK),
	REGMAP_IRQ_REG(BD71815_INT_DCIN_OVP_RES, 1, BD71815_INT_DCIN_OVP_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_DCIN_OVP_DET, 1, BD71815_INT_DCIN_OVP_DET_MASK),
	/* DCIN2 interrupts */
	REGMAP_IRQ_REG(BD71815_INT_DCIN_MON_RES, 2, BD71815_INT_DCIN_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_DCIN_MON_DET, 2, BD71815_INT_DCIN_MON_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_WDOG, 2, BD71815_INT_WDOG_MASK),
	/* Vsys */
	REGMAP_IRQ_REG(BD71815_INT_VSYS_UV_RES, 3, BD71815_INT_VSYS_UV_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_UV_DET, 3, BD71815_INT_VSYS_UV_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_LOW_RES, 3, BD71815_INT_VSYS_LOW_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_LOW_DET, 3, BD71815_INT_VSYS_LOW_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_MON_RES, 3, BD71815_INT_VSYS_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_MON_DET, 3, BD71815_INT_VSYS_MON_DET_MASK),
	/* Charger */
	REGMAP_IRQ_REG(BD71815_INT_CHG_WDG_TEMP, 4, BD71815_INT_CHG_WDG_TEMP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_WDG_TIME, 4, BD71815_INT_CHG_WDG_TIME_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_RECHARGE_RES, 4, BD71815_INT_CHG_RECHARGE_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_RECHARGE_DET, 4, BD71815_INT_CHG_RECHARGE_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_RANGED_TEMP_TRANSITION, 4,
		       BD71815_INT_CHG_RANGED_TEMP_TRANSITION_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_STATE_TRANSITION, 4, BD71815_INT_CHG_STATE_TRANSITION_MASK),
	/* Battery */
	REGMAP_IRQ_REG(BD71815_INT_BAT_TEMP_NORMAL, 5, BD71815_INT_BAT_TEMP_NORMAL_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_TEMP_ERANGE, 5, BD71815_INT_BAT_TEMP_ERANGE_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_REMOVED, 5, BD71815_INT_BAT_REMOVED_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_DETECTED, 5, BD71815_INT_BAT_DETECTED_MASK),
	REGMAP_IRQ_REG(BD71815_INT_THERM_REMOVED, 5, BD71815_INT_THERM_REMOVED_MASK),
	REGMAP_IRQ_REG(BD71815_INT_THERM_DETECTED, 5, BD71815_INT_THERM_DETECTED_MASK),
	/* Battery Mon 1 */
	REGMAP_IRQ_REG(BD71815_INT_BAT_DEAD, 6, BD71815_INT_BAT_DEAD_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_SHORTC_RES, 6, BD71815_INT_BAT_SHORTC_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_SHORTC_DET, 6, BD71815_INT_BAT_SHORTC_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_LOW_VOLT_RES, 6, BD71815_INT_BAT_LOW_VOLT_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_LOW_VOLT_DET, 6, BD71815_INT_BAT_LOW_VOLT_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_VOLT_RES, 6, BD71815_INT_BAT_OVER_VOLT_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_VOLT_DET, 6, BD71815_INT_BAT_OVER_VOLT_DET_MASK),
	/* Battery Mon 2 */
	REGMAP_IRQ_REG(BD71815_INT_BAT_MON_RES, 7, BD71815_INT_BAT_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_MON_DET, 7, BD71815_INT_BAT_MON_DET_MASK),
	/* Battery Mon 3 (Coulomb counter) */
	REGMAP_IRQ_REG(BD71815_INT_BAT_CC_MON1, 8, BD71815_INT_BAT_CC_MON1_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_CC_MON2, 8, BD71815_INT_BAT_CC_MON2_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_CC_MON3, 8, BD71815_INT_BAT_CC_MON3_MASK),
	/* Battery Mon 4 */
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_1_RES, 9, BD71815_INT_BAT_OVER_CURR_1_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_1_DET, 9, BD71815_INT_BAT_OVER_CURR_1_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_2_RES, 9, BD71815_INT_BAT_OVER_CURR_2_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_2_DET, 9, BD71815_INT_BAT_OVER_CURR_2_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_3_RES, 9, BD71815_INT_BAT_OVER_CURR_3_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_3_DET, 9, BD71815_INT_BAT_OVER_CURR_3_DET_MASK),
	/* Temperature */
	REGMAP_IRQ_REG(BD71815_INT_TEMP_BAT_LOW_RES, 10, BD71815_INT_TEMP_BAT_LOW_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_BAT_LOW_DET, 10, BD71815_INT_TEMP_BAT_LOW_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_BAT_HI_RES, 10, BD71815_INT_TEMP_BAT_HI_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_BAT_HI_DET, 10, BD71815_INT_TEMP_BAT_HI_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_CHIP_OVER_125_RES, 10,
		       BD71815_INT_TEMP_CHIP_OVER_125_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_CHIP_OVER_125_DET, 10,
		       BD71815_INT_TEMP_CHIP_OVER_125_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_CHIP_OVER_VF_RES, 10,
		       BD71815_INT_TEMP_CHIP_OVER_VF_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_CHIP_OVER_VF_DET, 10,
		       BD71815_INT_TEMP_CHIP_OVER_VF_DET_MASK),
	/* RTC Alarm */
	REGMAP_IRQ_REG(BD71815_INT_RTC0, 11, BD71815_INT_RTC0_MASK),
	REGMAP_IRQ_REG(BD71815_INT_RTC1, 11, BD71815_INT_RTC1_MASK),
	REGMAP_IRQ_REG(BD71815_INT_RTC2, 11, BD71815_INT_RTC2_MASK),
};

static struct regmap_irq bd71828_irqs[] = {
	REGMAP_IRQ_REG(BD71828_INT_BUCK1_OCP, 0, BD71828_INT_BUCK1_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK2_OCP, 0, BD71828_INT_BUCK2_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK3_OCP, 0, BD71828_INT_BUCK3_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK4_OCP, 0, BD71828_INT_BUCK4_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK5_OCP, 0, BD71828_INT_BUCK5_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK6_OCP, 0, BD71828_INT_BUCK6_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BUCK7_OCP, 0, BD71828_INT_BUCK7_OCP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_PGFAULT, 0, BD71828_INT_PGFAULT_MASK),
	/* DCIN1 interrupts */
	REGMAP_IRQ_REG(BD71828_INT_DCIN_DET, 1, BD71828_INT_DCIN_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_DCIN_RMV, 1, BD71828_INT_DCIN_RMV_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CLPS_OUT, 1, BD71828_INT_CLPS_OUT_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CLPS_IN, 1, BD71828_INT_CLPS_IN_MASK),
	/* DCIN2 interrupts */
	REGMAP_IRQ_REG(BD71828_INT_DCIN_MON_RES, 2, BD71828_INT_DCIN_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_DCIN_MON_DET, 2, BD71828_INT_DCIN_MON_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_LONGPUSH, 2, BD71828_INT_LONGPUSH_MASK),
	REGMAP_IRQ_REG(BD71828_INT_MIDPUSH, 2, BD71828_INT_MIDPUSH_MASK),
	REGMAP_IRQ_REG(BD71828_INT_SHORTPUSH, 2, BD71828_INT_SHORTPUSH_MASK),
	REGMAP_IRQ_REG(BD71828_INT_PUSH, 2, BD71828_INT_PUSH_MASK),
	REGMAP_IRQ_REG(BD71828_INT_WDOG, 2, BD71828_INT_WDOG_MASK),
	REGMAP_IRQ_REG(BD71828_INT_SWRESET, 2, BD71828_INT_SWRESET_MASK),
	/* Vsys */
	REGMAP_IRQ_REG(BD71828_INT_VSYS_UV_RES, 3, BD71828_INT_VSYS_UV_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_UV_DET, 3, BD71828_INT_VSYS_UV_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_LOW_RES, 3, BD71828_INT_VSYS_LOW_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_LOW_DET, 3, BD71828_INT_VSYS_LOW_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_HALL_IN, 3, BD71828_INT_VSYS_HALL_IN_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_HALL_TOGGLE, 3, BD71828_INT_VSYS_HALL_TOGGLE_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_MON_RES, 3, BD71828_INT_VSYS_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_VSYS_MON_DET, 3, BD71828_INT_VSYS_MON_DET_MASK),
	/* Charger */
	REGMAP_IRQ_REG(BD71828_INT_CHG_DCIN_ILIM, 4, BD71828_INT_CHG_DCIN_ILIM_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_TOPOFF_TO_DONE, 4, BD71828_INT_CHG_TOPOFF_TO_DONE_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_WDG_TEMP, 4, BD71828_INT_CHG_WDG_TEMP_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_WDG_TIME, 4, BD71828_INT_CHG_WDG_TIME_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_RECHARGE_RES, 4, BD71828_INT_CHG_RECHARGE_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_RECHARGE_DET, 4, BD71828_INT_CHG_RECHARGE_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_RANGED_TEMP_TRANSITION, 4,
		       BD71828_INT_CHG_RANGED_TEMP_TRANSITION_MASK),
	REGMAP_IRQ_REG(BD71828_INT_CHG_STATE_TRANSITION, 4, BD71828_INT_CHG_STATE_TRANSITION_MASK),
	/* Battery */
	REGMAP_IRQ_REG(BD71828_INT_BAT_TEMP_NORMAL, 5, BD71828_INT_BAT_TEMP_NORMAL_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_TEMP_ERANGE, 5, BD71828_INT_BAT_TEMP_ERANGE_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_TEMP_WARN, 5, BD71828_INT_BAT_TEMP_WARN_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_REMOVED, 5, BD71828_INT_BAT_REMOVED_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_DETECTED, 5, BD71828_INT_BAT_DETECTED_MASK),
	REGMAP_IRQ_REG(BD71828_INT_THERM_REMOVED, 5, BD71828_INT_THERM_REMOVED_MASK),
	REGMAP_IRQ_REG(BD71828_INT_THERM_DETECTED, 5, BD71828_INT_THERM_DETECTED_MASK),
	/* Battery Mon 1 */
	REGMAP_IRQ_REG(BD71828_INT_BAT_DEAD, 6, BD71828_INT_BAT_DEAD_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_SHORTC_RES, 6, BD71828_INT_BAT_SHORTC_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_SHORTC_DET, 6, BD71828_INT_BAT_SHORTC_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_LOW_VOLT_RES, 6, BD71828_INT_BAT_LOW_VOLT_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_LOW_VOLT_DET, 6, BD71828_INT_BAT_LOW_VOLT_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_VOLT_RES, 6, BD71828_INT_BAT_OVER_VOLT_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_VOLT_DET, 6, BD71828_INT_BAT_OVER_VOLT_DET_MASK),
	/* Battery Mon 2 */
	REGMAP_IRQ_REG(BD71828_INT_BAT_MON_RES, 7, BD71828_INT_BAT_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_MON_DET, 7, BD71828_INT_BAT_MON_DET_MASK),
	/* Battery Mon 3 (Coulomb counter) */
	REGMAP_IRQ_REG(BD71828_INT_BAT_CC_MON1, 8, BD71828_INT_BAT_CC_MON1_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_CC_MON2, 8, BD71828_INT_BAT_CC_MON2_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_CC_MON3, 8, BD71828_INT_BAT_CC_MON3_MASK),
	/* Battery Mon 4 */
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_1_RES, 9, BD71828_INT_BAT_OVER_CURR_1_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_1_DET, 9, BD71828_INT_BAT_OVER_CURR_1_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_2_RES, 9, BD71828_INT_BAT_OVER_CURR_2_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_2_DET, 9, BD71828_INT_BAT_OVER_CURR_2_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_3_RES, 9, BD71828_INT_BAT_OVER_CURR_3_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_BAT_OVER_CURR_3_DET, 9, BD71828_INT_BAT_OVER_CURR_3_DET_MASK),
	/* Temperature */
	REGMAP_IRQ_REG(BD71828_INT_TEMP_BAT_LOW_RES, 10, BD71828_INT_TEMP_BAT_LOW_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_BAT_LOW_DET, 10, BD71828_INT_TEMP_BAT_LOW_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_BAT_HI_RES, 10, BD71828_INT_TEMP_BAT_HI_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_BAT_HI_DET, 10, BD71828_INT_TEMP_BAT_HI_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_CHIP_OVER_125_RES, 10,
		       BD71828_INT_TEMP_CHIP_OVER_125_RES_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_CHIP_OVER_125_DET, 10,
		       BD71828_INT_TEMP_CHIP_OVER_125_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_CHIP_OVER_VF_DET, 10,
		       BD71828_INT_TEMP_CHIP_OVER_VF_DET_MASK),
	REGMAP_IRQ_REG(BD71828_INT_TEMP_CHIP_OVER_VF_RES, 10,
		       BD71828_INT_TEMP_CHIP_OVER_VF_RES_MASK),
	/* RTC Alarm */
	REGMAP_IRQ_REG(BD71828_INT_RTC0, 11, BD71828_INT_RTC0_MASK),
	REGMAP_IRQ_REG(BD71828_INT_RTC1, 11, BD71828_INT_RTC1_MASK),
	REGMAP_IRQ_REG(BD71828_INT_RTC2, 11, BD71828_INT_RTC2_MASK),
};

static struct regmap_irq_chip bd71828_irq_chip = {
	.name = "bd71828_irq",
	.main_status = BD71828_REG_INT_MAIN,
	.irqs = &bd71828_irqs[0],
	.num_irqs = ARRAY_SIZE(bd71828_irqs),
	.status_base = BD71828_REG_INT_BUCK,
	.mask_base = BD71828_REG_INT_MASK_BUCK,
	.ack_base = BD71828_REG_INT_BUCK,
	.mask_invert = true,
	.init_ack_masked = true,
	.num_regs = 12,
	.num_main_regs = 1,
	.sub_reg_offsets = &bd718xx_sub_irq_offsets[0],
	.num_main_status_bits = 8,
	.irq_reg_stride = 1,
};

static struct regmap_irq_chip bd71815_irq_chip = {
	.name = "bd71815_irq",
	.main_status = BD71815_REG_INT_STAT,
	.irqs = &bd71815_irqs[0],
	.num_irqs = ARRAY_SIZE(bd71815_irqs),
	.status_base = BD71815_REG_INT_STAT_01,
	.mask_base = BD71815_REG_INT_EN_01,
	.ack_base = BD71815_REG_INT_STAT_01,
	.mask_invert = true,
	.init_ack_masked = true,
	.num_regs = 12,
	.num_main_regs = 1,
	.sub_reg_offsets = &bd718xx_sub_irq_offsets[0],
	.num_main_status_bits = 8,
	.irq_reg_stride = 1,
};

static int set_clk_mode(struct device *dev, struct regmap *regmap,
			int clkmode_reg)
{
	/*int ret;
	unsigned int open_drain;

	ret = of_property_read_u32(dev->of_node, "rohm,clkout-open-drain", &open_drain);
	if (ret) {
		if (ret == -EINVAL)
			return 0;
		return ret;
	}
	if (open_drain > 1) {
		dev_err(dev, "bad clk32kout mode configuration");
		return -EINVAL;
	}

	if (open_drain)
		return regmap_update_bits(regmap, clkmode_reg, OUT32K_MODE,
					  OUT32K_MODE_OPEN_DRAIN);

	return regmap_update_bits(regmap, clkmode_reg, OUT32K_MODE,
				  OUT32K_MODE_CMOS);*/
	return regmap_write(gptBd71828->regmap, clkmode_reg, 0x00);
}

int bd71828_restart(void)
{
	printk ("[%s-%d] ...\n", __func__, __LINE__);

	while (true) {
		regmap_write(gptBd71828->regmap, 6, 0x01);
		msleep (500);
	}
}

int bd71828_power_off(void)
{
	printk ("[%s-%d] ...\n", __func__, __LINE__);

	{
		//keep power off date-time
		unsigned int value;

		regmap_read(gptBd71828->regmap, BD71879_REG_RTC_SEC, &value);
		regmap_write(gptBd71828->regmap, 0xf4, value);
		regmap_read(gptBd71828->regmap, BD71879_REG_RTC_MIN, &value);
		regmap_write(gptBd71828->regmap, 0xf5, value);
		regmap_read(gptBd71828->regmap, BD71879_REG_RTC_HOUR, &value);
		regmap_write(gptBd71828->regmap, 0xf6, value);
		regmap_read(gptBd71828->regmap, BD71879_REG_RTC_WEEK, &value);
		regmap_write(gptBd71828->regmap, 0xf7, value);
		regmap_read(gptBd71828->regmap, BD71879_REG_RTC_DAY, &value);
		regmap_write(gptBd71828->regmap, 0xf8, value);
		regmap_read(gptBd71828->regmap, BD71879_REG_RTC_MONTH, &value);
		regmap_write(gptBd71828->regmap, 0xf9, value);
		regmap_read(gptBd71828->regmap, BD71879_REG_RTC_YEAR, &value);
		regmap_write(gptBd71828->regmap, 0xfa, value);
	}

	regmap_write(gptBd71828->regmap, 0x03, 0xff);
	mdelay(500);
	regmap_write(gptBd71828->regmap, 0xe2, 0xff);
	mdelay (500);
	while (true) {
		regmap_write(gptBd71828->regmap, 4, 0x02);
		mdelay (500);
	}
}

static const struct of_device_id bd71828_of_match[] = {
	{
		.compatible = "rohm,bd71828",
		.data = (void *)ROHM_CHIP_TYPE_BD71828,
	}, {
		.compatible = "rohm,bd71815",
		.data = (void *)ROHM_CHIP_TYPE_BD71815,
	 },
	{ },
};
MODULE_DEVICE_TABLE(of, bd71828_of_match);

static int bd71828_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	//struct regmap_irq_chip_data *irq_data;
	//int ret;
	//struct regmap *regmap;
	//const struct regmap_config *regmap_config;
	//struct regmap_irq_chip *irqchip;
	//unsigned int chip_type;
	//struct mfd_cell *mfd;
	//int cells;
	//int button_irq;
	//int clkmode_reg;
	//unsigned int data;

	//printk(KERN_INFO "PMU: %s begin \n", __func__);

	//i2c->irq = 129;
	//if (!i2c->irq) {
	//	dev_err(&i2c->dev, "No IRQ configured\n");
	//	return -EINVAL;
	//}
	//i2c->irq = gpio_to_irq(i2c->irq);

	//chip_type = (unsigned int)(uintptr_t)
	//	    of_device_get_match_data(&i2c->dev);
	//chip_type = (unsigned int)(uintptr_t)of_driver_match_device(&i2c->dev,i2c->dev.driver);
	//printk(KERN_INFO "bd71828_i2c_probe chip_type %d \n", chip_type);
	//switch (chip_type) {
	//case ROHM_CHIP_TYPE_BD71828:
	//	printk(KERN_INFO "PMIC ROHM_CHIP_TYPE_BD71828\n");
	//	mfd = bd71828_mfd_cells;
	//	cells = ARRAY_SIZE(bd71828_mfd_cells);
	//	regmap_config = &bd71828_regmap;
	//	irqchip = &bd71828_irq_chip;
		//clkmode_reg = BD71828_REG_OUT32K;
		//button_irq = BD71828_INT_SHORTPUSH;
	//	break;
	//case ROHM_CHIP_TYPE_BD71815:
	//	printk(KERN_INFO "PMIC ROHM_CHIP_TYPE_BD71815\n");
	//	mfd = bd71815_mfd_cells;
	//	cells = ARRAY_SIZE(bd71815_mfd_cells);
	//	regmap_config = &bd71815_regmap;
	//	irqchip = &bd71815_irq_chip;
	//	clkmode_reg = BD71815_REG_OUT32K;
		/*
		 * If BD71817 support is needed we should be able to handle it
		 * with proper DT configs + BD71815 drivers + power-button.
		 * BD71815 data-sheet does not list the power-button IRQ so we
		 * don't use it.
		 */
	//	button_irq = 0;
	//	break;
	//default:
	//	dev_err(&i2c->dev, "Unknown device type");
	//	return -EINVAL;
	//}

	//regmap = devm_regmap_init_i2c(i2c, regmap_config);
	//if (IS_ERR(regmap)) {
	//	dev_err(&i2c->dev, "Failed to initialize Regmap\n");
	//	return PTR_ERR(regmap);
	//}

    //ret = regmap_read(regmap, 0x00, &data);
    //if (ret < 0) {
    //    dev_err(&i2c->dev, "device access error\n");
    //    return -ENODEV;
    //}
	//printk(KERN_INFO "PMIC ROHM_CHIP_TYPE_BD71828 regmap_read 0x00 :%d\n",data);

	//printk(KERN_INFO "PMIC ROHM_CHIP_TYPE_BD71828 IRQ :%d\n",i2c->irq);
	//ret = devm_regmap_add_irq_chip(&i2c->dev, regmap, i2c->irq,
	//			       IRQF_ONESHOT, 0, irqchip, &irq_data);
	//ret = regmap_add_irq_chip(regmap, i2c->irq,
	//			       IRQF_ONESHOT, 0, irqchip, &irq_data);
	//if (ret) {
	//	dev_err(&i2c->dev, "Failed to add IRQ chip\n");
	//	return ret;
	//}

	//dev_dbg(&i2c->dev, "Registered %d IRQs for chip\n",
	//	irqchip->num_irqs);

	//if (button_irq) {
	//	ret = regmap_irq_get_virq(irq_data, button_irq);
	//	if (ret < 0) {
	//		dev_err(&i2c->dev, "Failed to get the power-key IRQ\n");
	//		return ret;
	//	}
//
	//	button.irq = ret;
	//}

	/*ret = set_clk_mode(&i2c->dev, regmap, clkmode_reg);
	if (ret)
		return ret;*/

	//ret = devm_mfd_add_devices(&i2c->dev, PLATFORM_DEVID_NONE, mfd, cells,
	//			   NULL, 0, /*regmap_irq_get_domain(irq_data)*/NULL);

	//if (ret)
	//	dev_err(&i2c->dev, "Failed to create subdevices\n");

	//printk(KERN_INFO "PMU: %s end \n", __func__);

	unsigned int data;
	const struct of_device_id *match;
	struct regmap_irq_chip_data *irq_data;
	int irq_gpio;
	int ret;
	int button_irq = BD71828_INT_PUSH;
	int clkmode_reg = BD71828_REG_OUT32K;

	printk(KERN_INFO "PMU: %s begin \n", __func__);

	gptBd71828 = devm_kzalloc(&i2c->dev, sizeof(struct rohm_regmap_dev), GFP_KERNEL);
	if (!gptBd71828)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		match = of_match_node(bd71828_of_match, i2c->dev.of_node);
		if (!match)
			return -EINVAL;

		gptBd71828->chip_type = (unsigned long)match->data;
	}
	else
		gptBd71828->chip_type = id->driver_data;
	dev_info(&i2c->dev, "bd71828_i2c_probe chip_type %ld \n", gptBd71828->chip_type);

	i2c_set_clientdata(i2c, gptBd71828);
	gptBd71828->dev = &i2c->dev;
	gptBd71828->i2c = i2c;
	gptBd71828->irq = NULL;

	if(i2c->irq) {
		gptBd71828->irq = i2c->irq;
	}
	else {
		irq_gpio = of_get_named_gpio(i2c->dev.of_node,"gpio_int",0);
		if (!gpio_is_valid(irq_gpio)) {
			printk("invalid irq_gpio: %d\n",  irq_gpio);
			return -EINVAL;
		}
		else
			printk("bd71828 irq gpio %d\n",irq_gpio);
		ret = gpio_request(irq_gpio, "rohm_pmic_irq");
		gpio_direction_input(irq_gpio);
		gptBd71828->irq = gpio_to_irq(irq_gpio);
	}
	dev_info(gptBd71828->dev, "PMIC ROHM_CHIP_TYPE_BD71828 IRQ ID :%02x, trigger type %d\n",gptBd71828->irq,irqd_get_trigger_type(irq_get_irq_data(i2c->irq)));

	gptBd71828->regmap = devm_regmap_init_i2c(gptBd71828->i2c, &bd71828_regmap);
	if(IS_ERR(gptBd71828->regmap)) {
		dev_err(gptBd71828->dev, "Failed to initialize Regmap\n");
		return PTR_ERR(gptBd71828->regmap);
	}

	{
		struct i2c_board_info info;

		memset(&info, 0, sizeof(struct i2c_board_info));
		info.addr = i2c->addr+1;
		info.platform_data = gptBd71828;
		strlcpy(info.type, "bd71828-otp", I2C_NAME_SIZE);
		gptBd71828->i2c_otp = i2c_new_device(i2c->adapter,&info);
		if(NULL == gptBd71828->i2c_otp) {
			dev_err(gptBd71828->dev,"Failed to create bd71828 otp\n");
		}

		gptBd71828->regmap_otp = devm_regmap_init_i2c(gptBd71828->i2c_otp, &bd71828_regmap);
		if (IS_ERR(gptBd71828->regmap_otp)) {
			dev_err(gptBd71828->dev, "Failed to initialize Regmap otp\n");
			return PTR_ERR(gptBd71828->regmap_otp);
		}
	}

	ret = regmap_read(gptBd71828->regmap, 0x00, &data);
	if (ret < 0) {
		dev_err(gptBd71828->dev, "device access error\n");
		return -ENODEV;
	}
	dev_info(gptBd71828->dev, "PMIC ROHM_CHIP_TYPE_BD71828 PMIC ID 0x00 :%02x\n",data);

	// turn on OTP access
	regmap_write(gptBd71828->regmap, 0xfe, 0x8c);
	regmap_write(gptBd71828->regmap, 0xff, 0x01);
	ret = regmap_read(gptBd71828->regmap_otp, 0x00, &data);
	if (ret < 0) {
		dev_err(gptBd71828->dev, "device access error\n");
		return -ENODEV;
	}
	dev_info(gptBd71828->dev, "PMIC ROHM_CHIP_TYPE_BD71828 PMIC OTP ID 0x00 :%02x\n",data);
	// Modify F_PWRON_DBNC 120ms
	regmap_write(gptBd71828->regmap_otp, 0x62, 0x1f);
	// turn on OTP access
	regmap_write(gptBd71828->regmap, 0xfe, 0x8c);
	regmap_write(gptBd71828->regmap, 0xff, 0x00);

	if(gptBd71828->irq) {
		irq_set_status_flags(gptBd71828->irq, IRQ_NOAUTOEN);
		ret = devm_regmap_add_irq_chip(gptBd71828->dev,gptBd71828->regmap, gptBd71828->irq,
					  IRQF_TRIGGER_LOW|IRQF_ONESHOT ,  0, &bd71828_irq_chip,
					  &irq_data);
		if (ret) {
			dev_err(gptBd71828->dev, "Failed to add IRQ chip\n");
			return ret;
		}
		irq_set_irq_type(gptBd71828->irq, IRQ_TYPE_LEVEL_LOW);

		enable_irq(gptBd71828->irq);
		enable_irq_wake(gptBd71828->irq);
	}

	/*if (button_irq) {
		ret = regmap_irq_get_virq(irq_data, button_irq);
		if (ret < 0) {
			dev_err(gptBd71828->dev, "Failed to get the power-key IRQ\n");
			return ret;
		}

		button.irq = ret;
	}*/
	if (button_irq) {
		ret = regmap_irq_get_virq(irq_data, button_irq);
		if (ret < 0) {
			dev_err(gptBd71828->dev, "Failed to get the power-key IRQ\n");
			return ret;
		}
		bd71828_powerkey_data.irq_push = button.irq = ret;

		ret = regmap_irq_get_virq(irq_data,BD71828_INT_SHORTPUSH);
		if (ret < 0) {
			dev_err(gptBd71828->dev, "Failed to get the power-key IRQ\n");
			return ret;
		}
		bd71828_powerkey_data.irq_short_push = ret;

		ret = regmap_irq_get_virq(irq_data, BD71828_INT_MIDPUSH);
		if (ret < 0) {
			dev_err(gptBd71828->dev, "Failed to get the power-key IRQ\n");
			return ret;
		}
		bd71828_powerkey_data.irq_mid_push = ret;

		ret = regmap_irq_get_virq(irq_data,BD71828_INT_LONGPUSH);
		if (ret < 0) {
			dev_err(gptBd71828->dev, "Failed to get the power-key IRQ\n");
			return ret;
		}
		bd71828_powerkey_data.irq_long_push = ret;
	}

	ret = set_clk_mode(&i2c->dev, gptBd71828->regmap, clkmode_reg);
	if (ret)
		return ret;

	ret = devm_mfd_add_devices(gptBd71828->dev, PLATFORM_DEVID_NONE, bd71828_mfd_cells, ARRAY_SIZE(bd71828_mfd_cells),
					NULL, 0, regmap_irq_get_domain(irq_data));
	if (ret)
		dev_err(gptBd71828->dev, "Failed to create subdevices\n");

	pm_power_off = bd71828_power_off;

	printk(KERN_INFO "PMU: %s end \n", __func__);

	return ret;
}

static const struct i2c_device_id bd71828_i2c_id[] = {
	{"bd71828", ROHM_CHIP_TYPE_BD71828},
	{}
};

MODULE_DEVICE_TABLE(i2c, bd71828_i2c_id);

static struct i2c_driver bd71828_drv = {
	.driver = {
		.name = "bd71828",
		.owner = THIS_MODULE,
		.of_match_table = bd71828_of_match,
	},
	.probe = &bd71828_i2c_probe,
	.id_table = bd71828_i2c_id,
};

//module_i2c_driver(bd71828_drv);

static int __init rohmbd71828_i2c_init(void)
{
	int ret = -ENODEV;

	printk(KERN_INFO "PMU: %s:\n", __func__);

	ret = i2c_add_driver(&bd71828_drv);
	if (ret != 0)
		pr_err("Failed to register I2C driver: %d\n", ret);

	return ret;
}

subsys_initcall(rohmbd71828_i2c_init);

static void __exit rohmbd71828_i2c_exit(void)
{
	i2c_del_driver(&bd71828_drv);
}

module_exit(rohmbd71828_i2c_exit);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("ROHM BD71828 Power Management IC driver");
MODULE_LICENSE("GPL");


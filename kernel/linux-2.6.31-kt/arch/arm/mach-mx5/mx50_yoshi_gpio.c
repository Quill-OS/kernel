/*
 * Copyright 2007-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2010-2011 Amazon.com, Inc. All rights reserved.
 * Manish Lachwani (lachwani@lab126.com)
 */
/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/pmic_external.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/boardid.h>

#include "mx50_pins.h"
#include "iomux.h"

#define TEQUILA_KEYB_PAD_CONFIG PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |		\
				PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH |	\
				PAD_CTL_100K_PD

#define RESTARTEN_LSH			0
#define WDIRESET_LSH			12
#define I2C_SCL_RECOVERY_CNT	10

extern void mx50_audio_clock_enable(int enable);

/*!
 * @file mach-mx50/mx50_yoshi_gpio.c
 *
 * @brief This file contains all the GPIO setup functions for the board.
 *
 * @ingroup GPIO
 */

static struct mxc_iomux_pin_cfg __initdata mxc_iomux_pins[] = {
	{
        MX50_PIN_SSI_RXC, IOMUX_CONFIG_ALT1,
        (PAD_CTL_PKE_NONE | PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_MAX),
        },
	/* Accessory UART*/
	/* RXD*/
	{
	MX50_PIN_UART4_RXD, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PU | PAD_CTL_HYS_ENABLE),
	MUX_IN_UART4_IPP_UART_RXD_MUX_SELECT_INPUT, INPUT_CTL_PATH1,
	},

	/* TXD*/
	{
	MX50_PIN_UART4_TXD, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD | PAD_CTL_HYS_ENABLE),
	},
	 
	/* Audio */
	{
	MX50_PIN_SSI_RXD, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE | 
	 PAD_CTL_HYS_ENABLE),
	},
	{
	MX50_PIN_SSI_TXD, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_SSI_TXFS, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	/* WM8962 IRQ */
	{
	MX50_PIN_EIM_DA4, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU | PAD_CTL_HYS_ENABLE),
	},
	{
	MX50_PIN_OWIRE, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH |
	 PAD_CTL_100K_PU | PAD_CTL_HYS_ENABLE),
	},
#ifdef CONFIG_SND_MXC_SOC
	{
	MX50_PIN_SD1_CMD, IOMUX_CONFIG_ALT7,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_SRE_FAST),
	},
#endif
	/* Debug UART */
	{
	MX50_PIN_UART1_RXD, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PU | PAD_CTL_HYS_ENABLE),
	},

	{
	MX50_PIN_UART1_TXD, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD | PAD_CTL_HYS_ENABLE),
	},

	/* Display Panel */
	{
	MX50_PIN_ECSPI2_MISO, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD | PAD_CTL_HYS_ENABLE),
	},

	{
	MX50_PIN_ECSPI2_MOSI, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},

	{
	MX50_PIN_ECSPI2_SCLK, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},

	{
	MX50_PIN_ECSPI2_SS0, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PU),
	},
	{
	MX50_PIN_EIM_DA0, IOMUX_CONFIG_ALT1, 
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL | PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU),
	},
	/* EMMC RESET*/
	{
	MX50_PIN_DISP_WR, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU),
	},

	/* EPDC */
	{
	MX50_PIN_EPDC_BDR0, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_BDR1, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D0, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D1, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D2, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D3, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D4, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D5, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D6, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D7, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D8, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D9, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D10, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D11, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D12, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D13, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D14, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_D15, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_GDCLK, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_GDOE, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_GDRL, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_GDSP, IOMUX_CONFIG_ALT1,
	
	},

	{
	MX50_PIN_EPDC_PWRCOM, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_PWRCTRL2, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_PWRCTRL3, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_PWRSTAT, IOMUX_CONFIG_ALT1,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL | PAD_CTL_100K_PU |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE),
	},

	{
	MX50_PIN_EPDC_SDCE0, IOMUX_CONFIG_ALT1,
	},

	{
	MX50_PIN_EPDC_SDCE1, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	{
	MX50_PIN_EPDC_SDCE2, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDCE3, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDCE4, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDCE5, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDCLK, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDCLKN, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDLE, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDOE, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDOED, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDOEZ, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_SDSHR, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_VCOM0, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_VCOM1, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	MX50_PIN_EPDC_PWRCTRL0, IOMUX_CONFIG_GPIO
	},
	{
	MX50_PIN_EPDC_PWRCTRL1, IOMUX_CONFIG_GPIO
	},
	{
	 MX50_PIN_I2C3_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
	  PAD_CTL_100K_PU),
	},
	{
	 MX50_PIN_I2C3_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE |
		PAD_CTL_100K_PU), 
	},
	{
	 MX50_PIN_DISP_D0, IOMUX_CONFIG_ALT1,
	 (PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |  PAD_CTL_100K_PU | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	{
	 MX50_PIN_DISP_D7, IOMUX_CONFIG_ALT1,
	 (PAD_CTL_PKE_NONE |  PAD_CTL_22K_PU | PAD_CTL_ODE_OPENDRAIN_NONE),
	},

	/* Accessory GPIOs */
	{
	 MX50_PIN_EIM_DA13, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL | PAD_CTL_100K_PD |
	  PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE),
	},
	{
	 MX50_PIN_EIM_DA14, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL | PAD_CTL_100K_PU |
	  PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE),
	},

	/* Neonode BSL RST */
	{
	MX50_PIN_DISP_BUSY, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL | PAD_CTL_22K_PU),
	},
	/* Neonode BSL TEST */ 
	{
	MX50_PIN_DISP_RESET, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL | PAD_CTL_100K_PD),
	},
	{ 
	MX50_PIN_DISP_D5, IOMUX_CONFIG_ALT2, (PAD_CTL_DRV_HIGH | PAD_CTL_100K_PU),
	},
	/* TOUCH */
	/* INTERACTION_RESET */
	{
	MX50_PIN_EIM_LBA, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_ENABLE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU),
	},
	/* TIRQ */
	{
	MX50_PIN_SD1_D0, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU),
	},
	/* TSRQ */
	{
	MX50_PIN_SD1_D1, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU),
	},
	/* AIRQ */
	{
	MX50_PIN_SD1_D3, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU),
	},
	/* INTERACTION UART */
	{
	MX50_PIN_UART3_RXD, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PU | PAD_CTL_HYS_ENABLE),
	MUX_IN_UART3_IPP_UART_RXD_MUX_SELECT_INPUT, INPUT_CTL_PATH1,
	},
	{
	MX50_PIN_UART3_TXD, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD | PAD_CTL_HYS_ENABLE),
	},
	/* INTERACTION I2C */
	{
	 MX50_PIN_I2C1_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE |
		PAD_CTL_100K_PU),
	},
	{
	 MX50_PIN_I2C1_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE |
		PAD_CTL_100K_PU),
	},
	 /* INTERACTION I2S */
	 /* INT_I2S_BCLK */
	{
	MX50_PIN_SD2_CD, IOMUX_CONFIG_ALT2,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_HYS_ENABLE | PAD_CTL_100K_PU),
	},
#if !defined(CONFIG_KEYBOARD_MXC_MODULE) && !defined(CONFIG_KEYBOARD_MXC)
	/* INT_I2S_RXD */
	{
	MX50_PIN_SD2_D6, IOMUX_CONFIG_ALT2,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_HYS_ENABLE| PAD_CTL_100K_PU),
	 },
	/* INT_I2S_FS */
	{
	MX50_PIN_SD2_D7, IOMUX_CONFIG_ALT2,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_HYS_ENABLE| PAD_CTL_100K_PU),
	},
#endif
	/* INT_I2S_TXD */
	{
	MX50_PIN_SD2_WP, IOMUX_CONFIG_ALT2,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE | 
	 PAD_CTL_100K_PU),
	},
	/* INTERACTION SPI */
	{
	MX50_PIN_ECSPI1_MISO, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},
	{
	MX50_PIN_ECSPI1_MOSI, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},
	{
	MX50_PIN_ECSPI1_SCLK, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},
	{
	MX50_PIN_ECSPI1_SS0, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PU),
	},
	/* Accelerometer */
	{
	MX50_PIN_SD1_D2, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH),
	},
	{
	MX50_PIN_SD1_CLK, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE),
	},
	/* PMIC */
	/* CHRGLED_DIS */
	{
	MX50_PIN_EIM_DA2, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH| 
	 PAD_CTL_100K_PD),
	},
	/* VBUSEN */
	{
	MX50_PIN_PWM2, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH| 
	 PAD_CTL_100K_PD),
	},
	/* PMIC_INT */
	{
	MX50_PIN_UART1_CTS, IOMUX_CONFIG_GPIO,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH| 
	 PAD_CTL_100K_PD),
	},
	{
	MX50_PIN_CSPI_MISO, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},
	{
	MX50_PIN_CSPI_MOSI, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},
	{
	MX50_PIN_CSPI_SCLK, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},
	{
	MX50_PIN_CSPI_SS0, IOMUX_CONFIG_ALT0,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PU),
	},
#ifdef CONFIG_MX50_YOSHI_SDCARD
	/* SD card slot */
	/* SD_CARD_CMD */
	{
	 MX50_PIN_DISP_D8, IOMUX_CONFIG_ALT4 | IOMUX_CONFIG_SION,
	 0x01e4, MUX_IN_ESDHC4_IPP_CMD_IN_SELECT_INPUT, INPUT_CTL_PATH2,
	},
	/* SD_CARD_CLK */
	{
	MX50_PIN_DISP_D9, IOMUX_CONFIG_ALT4 | IOMUX_CONFIG_SION,
	 0x00d4, MUX_IN_ESDHC4_IPP_CARD_CLK_IN_SELECT_INPUT, INPUT_CTL_PATH2,
	},
	/* SD_CARD_D0 */
	{
	MX50_PIN_DISP_D10, IOMUX_CONFIG_ALT4,
	 0x01d4, MUX_IN_ESDHC4_IPP_DAT0_IN_SELECT_INPUT, INPUT_CTL_PATH1,
	},
	/* SD_CARD_D1 */
	{
	MX50_PIN_DISP_D11, IOMUX_CONFIG_ALT4,
	 0x01d4, MUX_IN_ESDHC4_IPP_DAT1_IN_SELECT_INPUT, INPUT_CTL_PATH1,
	},
	/* SD_CARD_D2 */
	{
	MX50_PIN_DISP_D12, IOMUX_CONFIG_ALT4,
	 0x01d4, MUX_IN_ESDHC4_IPP_DAT2_IN_SELECT_INPUT, INPUT_CTL_PATH1,
	},
	/* SD_CARD_D3 */
	{
	MX50_PIN_DISP_D13, IOMUX_CONFIG_ALT4,
	 0x01d4, MUX_IN_ESDHC4_IPP_DAT3_IN_SELECT_INPUT, INPUT_CTL_PATH1,
	},
	/* SD_CARD_WP */
	{
	MX50_PIN_DISP_D14, IOMUX_CONFIG_ALT4,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_100K_PU),
	},
	/* SD_CARD_DET */
	{
	MX50_PIN_DISP_D15, IOMUX_CONFIG_GPIO,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_100K_PU),
	},
#else
	/* SD card slot */
	/* SD_CARD_CMD */
	{
	 MX50_PIN_DISP_D8, IOMUX_CONFIG_GPIO,
	 (0x01e4 | PAD_CTL_100K_PD),
	},
	/* SD_CARD_CLK */
	{
	MX50_PIN_DISP_D9, IOMUX_CONFIG_GPIO,
	 (0x00d4 | PAD_CTL_100K_PD),
	},
	/* SD_CARD_D0 */
	{
	MX50_PIN_DISP_D10, IOMUX_CONFIG_GPIO,
	 (0x01d4 | PAD_CTL_100K_PD),
	},
	/* SD_CARD_D1 */
	{
	MX50_PIN_DISP_D11, IOMUX_CONFIG_GPIO,
	 (0x01d4 | PAD_CTL_100K_PD),
	},
	/* SD_CARD_D2 */
	{
	MX50_PIN_DISP_D12, IOMUX_CONFIG_GPIO,
	 (0x01d4 | PAD_CTL_100K_PD),
	},
	/* SD_CARD_D3 */
	{
	MX50_PIN_DISP_D13, IOMUX_CONFIG_GPIO,
	 (0x01d4 | PAD_CTL_100K_PD),
	},
	/* SD_CARD_WP */
	{
	MX50_PIN_DISP_D14, IOMUX_CONFIG_GPIO,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_100K_PD),
	},
	/* SD_CARD_DET */
	{
	MX50_PIN_DISP_D15, IOMUX_CONFIG_GPIO,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_100K_PD),
	},
#endif
	/* Battery */
	{
	 MX50_PIN_I2C2_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE |
		PAD_CTL_100K_PU),
	},
	{
	 MX50_PIN_I2C2_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	 (PAD_CTL_HYS_ENABLE | PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE |
		PAD_CTL_100K_PU), 
	},

	/* USB_HUB_RESET*/
	{
	MX50_PIN_DISP_CS, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU),
	},
	/* WAN */
	/* WAN_USB_EN */
	{
	MX50_PIN_DISP_RD, IOMUX_CONFIG_GPIO,
	},
	/* WAN_GPIO2 (FW ready) */
	{
	MX50_PIN_EIM_DA6, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | 
	 PAD_CTL_100K_PD),
	},
	/* WAN_GPIO1 */
	{
	MX50_PIN_EIM_DA7, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | 
	 PAD_CTL_100K_PD),
	},
	/* WAN_MHI (USB wake) */
	{
	MX50_PIN_EIM_DA8, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},
	/* WAN_HMI */
	{
	MX50_PIN_EIM_DA9, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | 
	 PAD_CTL_100K_PD),
	},
	/* WAN_RF_ENABLE */
	{
	MX50_PIN_EIM_DA10, IOMUX_CONFIG_GPIO,
	},
	/* WAN_SHUTDOWN_B */
	{
	MX50_PIN_EIM_RW, IOMUX_CONFIG_GPIO,
	},
	/* SIM_PRESENT_B */
	{
	MX50_PIN_UART1_RTS, IOMUX_CONFIG_GPIO,
	},
	{
	 MX50_PIN_EIM_DA11, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD), 
	},
	{
	 MX50_PIN_EIM_DA12, IOMUX_CONFIG_GPIO,
	 (PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PD),
	},
	/* Watchdog */
	{
	MX50_PIN_WDOG, IOMUX_CONFIG_ALT2,
	(PAD_CTL_HYS_NONE | PAD_CTL_PKE_NONE |
	 PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_ENABLE),
	},
	/* Wifi */
	/* WIFI_PWR */	
	{
	MX50_PIN_SD3_WP, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_ENABLE | PAD_CTL_DRV_LOW | 
	 PAD_CTL_100K_PU),
	},
	/* WIFI_WAKE_ON_LAN */
	{
	MX50_PIN_EIM_DA5, IOMUX_CONFIG_GPIO,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU),
	},
	/* WIFI_SDIO */
	{
	MX50_PIN_SD2_CLK, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_47K_PU),
	},
	{
	 MX50_PIN_DISP_RS, IOMUX_CONFIG_ALT1,
	(PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH | 
	 PAD_CTL_100K_PU),
	}, 
	{
	MX50_PIN_SD2_CMD, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PU),
	},
	{
	MX50_PIN_SD2_D0, IOMUX_CONFIG_ALT0,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_47K_PU),
	},
	{
	MX50_PIN_SD2_D1, IOMUX_CONFIG_ALT0,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_47K_PU),
	},
	{
	MX50_PIN_SD2_D2, IOMUX_CONFIG_ALT0,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_47K_PU),
	},
	{
	MX50_PIN_SD2_D3, IOMUX_CONFIG_ALT0,
	(PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
	 PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE |
	 PAD_CTL_100K_PU),
	},
	{               /* SD3 CMD */
	 MX50_PIN_SD3_CMD, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,0x1e4,
	},
	{               /* SD3 CLK */
	 MX50_PIN_SD3_CLK, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION,0xd4,
	},
	{               /* SD3 D0 */
	 MX50_PIN_SD3_D0, IOMUX_CONFIG_ALT0,0x1d4,
	},
	{               /* SD3 D1 */
	 MX50_PIN_SD3_D1, IOMUX_CONFIG_ALT0,0x1d4,
	},
	{               /* SD3 D2 */
	 MX50_PIN_SD3_D2, IOMUX_CONFIG_ALT0,0x1d4,
	},
	{               /* SD3 D3 */
	 MX50_PIN_SD3_D3, IOMUX_CONFIG_ALT0,0x1d4,
	},
	{               /* SD3 D4 */
	 MX50_PIN_SD3_D4, IOMUX_CONFIG_ALT0,0x1d4,
	},
	{               /* SD3 D5 */
	 MX50_PIN_SD3_D5, IOMUX_CONFIG_ALT0,0x1d4,
	},
	{               /* SD3 D6 */
	 MX50_PIN_SD3_D6, IOMUX_CONFIG_ALT0,0x1d4,
	},
	{               /* SD3 D7 */
	 MX50_PIN_SD3_D7, IOMUX_CONFIG_ALT0,0x1d4,
	},
};

void __init mx50_yoshi_io_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mxc_iomux_pins); i++) {
		mxc_request_iomux(mxc_iomux_pins[i].pin,
		                  mxc_iomux_pins[i].mux_mode);
		if (mxc_iomux_pins[i].pad_cfg)
		        mxc_iomux_set_pad(mxc_iomux_pins[i].pin,
		                          mxc_iomux_pins[i].pad_cfg);
		if (mxc_iomux_pins[i].in_select)
		        mxc_iomux_set_input(mxc_iomux_pins[i].in_select,
		                            mxc_iomux_pins[i].in_mode);
	}

	/* SD4 CD */
	gpio_request(IOMUX_TO_GPIO(MX50_PIN_DISP_D15), "sd4_cd");
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D15));

	/* SD4 WP */
	gpio_request(IOMUX_TO_GPIO(MX50_PIN_DISP_D14), "sd4_wp");
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D14));

	/* PMIC Interrupt */
	gpio_request(IOMUX_TO_GPIO(MX50_PIN_UART1_CTS), "pmic_int");
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_UART1_CTS));

	/* WiFi Power */
	gpio_request(IOMUX_TO_GPIO(MX50_PIN_SD3_WP), "sd3_wp");
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_SD3_WP), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_SD3_WP), 1);	

	/* WiFi spec, add delay */
	mdelay(40);

	gpio_request(IOMUX_TO_GPIO(MX50_PIN_SSI_RXC), "gp5_6");
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_SSI_RXC), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_SSI_RXC), 1);
	
	/* Wait 30ms before powering on Papyrus */
	mdelay(30);

#ifdef CONFIG_SND_MXC_SOC

	mxc_request_iomux(MX50_PIN_SSI_TXC, IOMUX_CONFIG_ALT0);
	mxc_iomux_set_pad(MX50_PIN_SSI_TXC, PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
					PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE);

	mx50_audio_clock_enable(1);
#endif
	gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_VCOM0), "epdc_vcom");
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EPDC_VCOM0), 0);

	gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRSTAT), "epdc_pwrstat");
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRSTAT));

	gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRCOM), "epdc_pwrcom");
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRCOM), 0);

	gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRCTRL0), "epdc_pwrctrl0");
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRCTRL0), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRCTRL0), 1);

	gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRCTRL1), "epdc_pwrctrl1");
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EPDC_PWRCTRL1));

	gpio_request(IOMUX_TO_GPIO(MX50_PIN_EIM_DA4), "eim_da4");
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EIM_DA4));

	gpio_request(IOMUX_TO_GPIO(MX50_PIN_OWIRE), "owire");
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_OWIRE));

	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_DISP_CS), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_DISP_CS), 0);
	mdelay(10);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_DISP_CS), 1);

	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_BUSY));
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_RESET));
	
	/* WAN Power */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EIM_RW),0);

	/* WAN USB */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_DISP_RD), 0);

	/* WAN RF */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EIM_DA10),0);

	/* WAN HMI Host->Modem IRQ */
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EIM_DA9));

	/* WAN MHI : Modem->Host IRQ */
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EIM_DA8));	

	/* WAN FW Ready */
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EIM_DA6));

	/* EIM_DA10 and EIM_DA11 */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EIM_DA11), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA11), 0);

	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EIM_DA12), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA12), 0);

	/* Proximity */
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D0));

	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EIM_DA0));

	/* Proximity Reset */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_DISP_D7), 0);

	/* Accelerometer IRQ2 moved after Whitney protos */
	if (mx50_board_is(BOARD_ID_WHITNEY_PROTO) || 
	    mx50_board_is(BOARD_ID_WHITNEY_WFO_PROTO)) {
		mxc_free_iomux(MX50_PIN_I2C3_SCL, IOMUX_CONFIG_ALT0);
		mxc_request_iomux(MX50_PIN_I2C3_SCL, IOMUX_CONFIG_ALT1);

		gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C3_SCL), "i2c3_scl");
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_I2C3_SCL));

		gpio_request(IOMUX_TO_GPIO(MX50_PIN_SD1_D3), "sd1_d3");
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_SD1_D3));
		
	} else {
		gpio_request(IOMUX_TO_GPIO(MX50_PIN_SD1_D2), "sd1_d2");
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_SD1_D2));

		gpio_request(IOMUX_TO_GPIO(MX50_PIN_SD1_CLK), "sd1_clk");
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_SD1_CLK));
	}

	if (mx50_board_is(BOARD_ID_WHITNEY) || 
		mx50_board_is(BOARD_ID_WHITNEY_WFO) )  {
		printk("Configure EMI_CS1 for whitney \n");

		mxc_request_iomux(MX50_PIN_EIM_CS1, IOMUX_CONFIG_GPIO);
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EIM_CS1));
		mxc_iomux_set_pad(MX50_PIN_EIM_CS1, PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
					PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH |
					PAD_CTL_100K_PD);
	}

	/* EPDC */
	gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_SDCE0), "epdc_sdce0");	
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EPDC_SDCE0));

	gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_GDSP), "epdc_gdsp");
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EPDC_GDSP));

	/* Accessory power good */
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EIM_DA13));

	/* Accessory charging */
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_EIM_DA14));

	/* UART4 is not needed on !FINKLE */
	if (!mx50_board_is(BOARD_ID_FINKLE_EVT1)) {
		mxc_free_iomux(MX50_PIN_UART4_TXD, IOMUX_CONFIG_ALT0);
		mxc_request_iomux(MX50_PIN_UART4_TXD, IOMUX_CONFIG_ALT1);
		gpio_request(IOMUX_TO_GPIO(MX50_PIN_UART4_TXD), "uart4_txd");
		gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_UART4_TXD), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_UART4_TXD), 0);
	}

#ifndef CONFIG_MX50_YOSHI_SDCARD
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D8));
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D9));
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D10));
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D11));
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D12));
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D13));
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D14));
	gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_D15));
#endif

	/* NAND family - 1.8V */
	__raw_writel(0x2000, IO_ADDRESS(IOMUXC_BASE_ADDR) + 0x06c0);

	__raw_writel(0x2000, IO_ADDRESS(IOMUXC_BASE_ADDR) + 0x069c);
}

const int tequila_row_gpios[] = {
				 IOMUX_TO_GPIO(MX50_PIN_KEY_ROW0),
				 IOMUX_TO_GPIO(MX50_PIN_KEY_ROW1),
				 IOMUX_TO_GPIO(MX50_PIN_KEY_ROW2),
				 IOMUX_TO_GPIO(MX50_PIN_KEY_ROW3),
				 IOMUX_TO_GPIO(MX50_PIN_KEY_COL0),
				 IOMUX_TO_GPIO(MX50_PIN_KEY_COL1),
				 IOMUX_TO_GPIO(MX50_PIN_KEY_COL2),
				 IOMUX_TO_GPIO(MX50_PIN_KEY_COL3),
				};

const int tequila_col_gpios[] = { IOMUX_TO_GPIO(MX50_PIN_SD2_D4) };

EXPORT_SYMBOL(tequila_col_gpios);
EXPORT_SYMBOL(tequila_row_gpios);

void mx50_tequila_set_pad_keys(void)
{
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW0, TEQUILA_KEYB_PAD_CONFIG);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW1, TEQUILA_KEYB_PAD_CONFIG);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW2, TEQUILA_KEYB_PAD_CONFIG);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW3, TEQUILA_KEYB_PAD_CONFIG);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL0, TEQUILA_KEYB_PAD_CONFIG);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL1, TEQUILA_KEYB_PAD_CONFIG);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL2, TEQUILA_KEYB_PAD_CONFIG);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL3, TEQUILA_KEYB_PAD_CONFIG);
}
EXPORT_SYMBOL(mx50_tequila_set_pad_keys);

int gpio_accelerometer_int1(void)
{
	if ((mx50_board_is(BOARD_ID_WHITNEY_PROTO)) ||
		(mx50_board_is(BOARD_ID_WHITNEY_PROTO)))
			return IOMUX_TO_IRQ(MX50_PIN_I2C3_SCL);
	
	return IOMUX_TO_IRQ(MX50_PIN_SD1_D2);
}
EXPORT_SYMBOL(gpio_accelerometer_int1);

int gpio_accelerometer_int2(void)
{
	if ((mx50_board_is(BOARD_ID_WHITNEY_PROTO)) ||
		(mx50_board_is(BOARD_ID_WHITNEY_PROTO)))
			return IOMUX_TO_IRQ(MX50_PIN_SD1_D3);
	
	return IOMUX_TO_IRQ(MX50_PIN_SD1_CLK);
}
EXPORT_SYMBOL(gpio_accelerometer_int2);

int gpio_proximity_int(void)
{
	return IOMUX_TO_IRQ(MX50_PIN_DISP_D0);
}
EXPORT_SYMBOL(gpio_proximity_int);

int gpio_proximity_detected(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_DISP_D0));
}
EXPORT_SYMBOL(gpio_proximity_detected);

/* Home Button */
int gpio_whitney_button_int(void)
{
	return IOMUX_TO_IRQ(MX50_PIN_EIM_DA0);
}
EXPORT_SYMBOL(gpio_whitney_button_int);

int gpio_whitney_button_detected(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA0));
}
EXPORT_SYMBOL(gpio_whitney_button_detected);

void mx50_yoshi_set_home_button(void)
{
	mxc_iomux_set_pad(MX50_PIN_EIM_DA0, PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
				PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH |
				PAD_CTL_100K_PU);
}
EXPORT_SYMBOL(mx50_yoshi_set_home_button);

void mx50_yoshi_gpio_spi_chipselect_active(int cspi_mode, int status,
		                             int chipselect)
{
	switch (cspi_mode) {
	case 1:
		break;
	case 2:
		break;
	case 3:
		switch (chipselect) {
		case 0x1:
		        /* enable ss0 */
		        mxc_request_iomux(MX50_PIN_CSPI_SS0, IOMUX_CONFIG_ALT0);
		        /*disable other ss */
		        mxc_request_iomux(MX50_PIN_ECSPI1_MOSI, IOMUX_CONFIG_GPIO);
		        /* pull up/down deassert it */
		        gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_ECSPI1_MOSI));
		        break;
		case 0x2:
		        /* enable ss1 */
		        mxc_request_iomux(MX50_PIN_ECSPI1_MOSI, IOMUX_CONFIG_ALT2);
		        /*disable other ss */
		        mxc_request_iomux(MX50_PIN_CSPI_SS0, IOMUX_CONFIG_GPIO);
		        /* pull up/down deassert it */
		        gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_CSPI_SS0));
		        break;
		default:
		        break;
		}
		break;

	default:
		break;
	}
}

void mx50_yoshi_gpio_spi_chipselect_inactive(int cspi_mode, int status,
		                               int chipselect)
{
	switch (cspi_mode) {
	case 1:
		break;
	case 2:
		break;
	case 3:
		switch (chipselect) {
		case 0x1:
		        mxc_free_iomux(MX50_PIN_ECSPI1_MOSI, IOMUX_CONFIG_GPIO);
		        break;
		case 0x2:
		        mxc_free_iomux(MX50_PIN_CSPI_SS0, IOMUX_CONFIG_GPIO);
		        break;
		default:
		        break;
		}
		break;
	default:
		break;
	}

}

static int charger_led_state = 0;

void gpio_chrgled_active(int enable)
{
	if (charger_led_state != enable) {
		gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EIM_DA2), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA2), !enable);

		charger_led_state = enable;
	}
}
EXPORT_SYMBOL(gpio_chrgled_active);

/* Fiveway */
#define FIVEWAY_up_gpio		MX50_PIN_EIM_EB1
#define FIVEWAY_down_gpio	MX50_PIN_EIM_EB0
#define FIVEWAY_left_gpio	MX50_PIN_EIM_CS2
#define FIVEWAY_right_gpio	MX50_PIN_EIM_CS1
#define FIVEWAY_select_gpio	MX50_PIN_EIM_CS0

#define FIVEWAY_pad_disable	PAD_CTL_PKE_ENABLE

void gpio_fiveway_inactive(void)
{
	mxc_iomux_set_pad(FIVEWAY_up_gpio, FIVEWAY_pad_disable);
	mxc_iomux_set_pad(FIVEWAY_down_gpio, FIVEWAY_pad_disable);
	mxc_iomux_set_pad(FIVEWAY_left_gpio, FIVEWAY_pad_disable);
	mxc_iomux_set_pad(FIVEWAY_right_gpio, FIVEWAY_pad_disable);
	mxc_iomux_set_pad(FIVEWAY_select_gpio, FIVEWAY_pad_disable);
	mxc_free_iomux(FIVEWAY_up_gpio, IOMUX_CONFIG_GPIO);
	mxc_free_iomux(FIVEWAY_down_gpio, IOMUX_CONFIG_GPIO);
	mxc_free_iomux(FIVEWAY_left_gpio, IOMUX_CONFIG_GPIO);
	mxc_free_iomux(FIVEWAY_right_gpio, IOMUX_CONFIG_GPIO);
	mxc_free_iomux(FIVEWAY_select_gpio, IOMUX_CONFIG_GPIO);
}
EXPORT_SYMBOL(gpio_fiveway_inactive);

static void fiveway_set_gpio_pads(iomux_pin_name_t gpio)
{
	unsigned int FIVEWAY_pad_enable = PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
					PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_DRV_HIGH;

	if (mx50_board_is(BOARD_ID_TEQUILA))
		FIVEWAY_pad_enable |= PAD_CTL_100K_PD;
	else
		FIVEWAY_pad_enable |= PAD_CTL_100K_PU;

	mxc_iomux_set_pad(gpio, FIVEWAY_pad_enable);
}

#define SET_FIVEWAY_GPIO(gpio)					\
	err = mxc_request_iomux(gpio, IOMUX_CONFIG_GPIO);	\
	if (err) {						\
		goto disable_fiveway_gpios;			\
	}							\
	gpio_direction_input(IOMUX_TO_GPIO(gpio));		\
	fiveway_set_gpio_pads(gpio)

int gpio_fiveway_active(void)
{
	int err;

	SET_FIVEWAY_GPIO(FIVEWAY_up_gpio);
	SET_FIVEWAY_GPIO(FIVEWAY_down_gpio);
	SET_FIVEWAY_GPIO(FIVEWAY_left_gpio);
	SET_FIVEWAY_GPIO(FIVEWAY_right_gpio);
	SET_FIVEWAY_GPIO(FIVEWAY_select_gpio);

	return err;

disable_fiveway_gpios:
	gpio_fiveway_inactive();
	return err;
}
EXPORT_SYMBOL(gpio_fiveway_active);

bool fiveway_datain(int line_direction)
{
	iomux_pin_name_t gpio;
	int err = -1;

	switch (line_direction) {
	case 0: gpio = FIVEWAY_up_gpio;
		break;
	case 1: gpio = FIVEWAY_down_gpio;
		break;
	case 2: gpio = FIVEWAY_left_gpio;
		break;
	case 3: gpio = FIVEWAY_right_gpio;
		break;
	case 4: gpio = FIVEWAY_select_gpio;
		break;
	default: return err;
	}

	if (mx50_board_is(BOARD_ID_TEQUILA))
		return !gpio_get_value(IOMUX_TO_GPIO(gpio));
	else
		return gpio_get_value(IOMUX_TO_GPIO(gpio));
}
EXPORT_SYMBOL(fiveway_datain);

int fiveway_get_irq(int line_direction)
{
	iomux_pin_name_t gpio;
	int err = -1;

	switch (line_direction) {
	case 0: gpio = FIVEWAY_up_gpio;
		break;
	case 1: gpio = FIVEWAY_down_gpio;
		break;
	case 2: gpio = FIVEWAY_left_gpio;
		break;
	case 3: gpio = FIVEWAY_right_gpio;
		break;
	case 4: gpio = FIVEWAY_select_gpio;
		break;
	default: return err;
	}
	return IOMUX_TO_IRQ(gpio);
}
EXPORT_SYMBOL(fiveway_get_irq);

/* Volume Button */

#define VOLUME_UP_gpio		MX50_PIN_EIM_BCLK
#define VOLUME_DOWN_gpio	MX50_PIN_EIM_RDY
#define VOLUME_pad_enable	PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE |	\
				PAD_CTL_PUE_PULL | PAD_CTL_ODE_OPENDRAIN_NONE |	\
				PAD_CTL_DRV_HIGH | PAD_CTL_100K_PD

void gpio_volume_inactive(void)
{
	mxc_iomux_set_pad(VOLUME_UP_gpio, PAD_CTL_PKE_ENABLE);
	mxc_iomux_set_pad(VOLUME_DOWN_gpio, PAD_CTL_PKE_ENABLE);
	mxc_free_iomux(VOLUME_UP_gpio, IOMUX_CONFIG_GPIO);
	mxc_free_iomux(VOLUME_DOWN_gpio, IOMUX_CONFIG_GPIO);
}
EXPORT_SYMBOL(gpio_volume_inactive);

#define SET_VOLUME_GPIO(gpio) \
	err = mxc_request_iomux(gpio, IOMUX_CONFIG_GPIO); \
	if (err) { \
		goto disable_volume_gpios; \
	} \
	gpio_direction_input(IOMUX_TO_GPIO(gpio)); \
	mxc_iomux_set_pad(gpio, VOLUME_pad_enable);

int gpio_volume_active(void)
{
	int err;

	SET_VOLUME_GPIO(VOLUME_UP_gpio);
	SET_VOLUME_GPIO(VOLUME_DOWN_gpio);
	
	return err;

disable_volume_gpios:
	gpio_volume_inactive();
	return err;	
	
}
EXPORT_SYMBOL(gpio_volume_active);

int volume_datain(int line_direction)
{
	iomux_pin_name_t gpio;
	int err = -1;

	switch (line_direction) {
	case 0: gpio = VOLUME_UP_gpio;
		break;
	case 1: gpio = VOLUME_DOWN_gpio;
		break;
	default: return err;
	}
	return (!gpio_get_value(IOMUX_TO_GPIO(gpio)));
}
EXPORT_SYMBOL(volume_datain);

int volume_get_irq(int line_direction)
{
	iomux_pin_name_t gpio;
	int err = -1;

	switch (line_direction) {
	case 0: gpio = VOLUME_UP_gpio;
		break;
	case 1: gpio = VOLUME_DOWN_gpio;
		break;
	default: return err;
	}
	return IOMUX_TO_IRQ(gpio);
}
EXPORT_SYMBOL(volume_get_irq);

void gpio_wan_power(int enable)
{
	/* Set the direction to output */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EIM_RW), enable);
}
EXPORT_SYMBOL(gpio_wan_power);

void gpio_wan_rf_enable(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA10), enable);
}
EXPORT_SYMBOL(gpio_wan_rf_enable);

void gpio_wan_usb_enable(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_DISP_RD), enable);
}
EXPORT_SYMBOL(gpio_wan_usb_enable);

int gpio_wan_mhi_irq(void)
{
	return IOMUX_TO_IRQ(MX50_PIN_EIM_DA8);
}
EXPORT_SYMBOL(gpio_wan_mhi_irq);

int gpio_wan_fw_ready_irq(void)
{
	return IOMUX_TO_IRQ(MX50_PIN_EIM_DA6);
}
EXPORT_SYMBOL(gpio_wan_fw_ready_irq);

int gpio_wan_fw_ready(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA6));
}
EXPORT_SYMBOL(gpio_wan_fw_ready);

void set_fw_ready_gpio_state(int enable)
{
	if( enable) {
		mxc_iomux_set_pad(MX50_PIN_EIM_DA6,
			PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
			PAD_CTL_ODE_OPENDRAIN_NONE | 
			PAD_CTL_100K_PU
		);
	} else {
		mxc_iomux_set_pad(MX50_PIN_EIM_DA6,
			PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
			PAD_CTL_ODE_OPENDRAIN_NONE | 
			PAD_CTL_100K_PD
		);
	}
}
EXPORT_SYMBOL(set_fw_ready_gpio_state);

int gpio_wan_usb_wake(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA8));
}
EXPORT_SYMBOL(gpio_wan_usb_wake);

/* Accessory */
int gpio_accessory_get_irq(void)
{
	return IOMUX_TO_IRQ(MX50_PIN_EIM_DA13);
}
EXPORT_SYMBOL(gpio_accessory_get_irq);

int gpio_accessory_charger_detected(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA13));
}
EXPORT_SYMBOL(gpio_accessory_charger_detected);

int gpio_accessory_charging(void)
{
	return !gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA14));
}
EXPORT_SYMBOL(gpio_accessory_charging);

/* Touch Controller - IRQ GPIO */
void gpio_touchcntrl_request_irq(int enable)
{
	if (enable) {
		mxc_request_iomux(MX50_PIN_DISP_RS, IOMUX_CONFIG_GPIO);
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_RS));
	}
	else {
		mxc_free_iomux(MX50_PIN_DISP_RS, IOMUX_CONFIG_GPIO);
	}
}
EXPORT_SYMBOL(gpio_touchcntrl_request_irq);

int gpio_touchcntrl_irq(void)
{
	return IOMUX_TO_IRQ(MX50_PIN_DISP_RS);
}
EXPORT_SYMBOL(gpio_touchcntrl_irq);

bool gpio_touchcntrl_datain(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_DISP_RS));
}
EXPORT_SYMBOL(gpio_touchcntrl_datain);

void gpio_neonode_bsl_init(int enable)
{
	if (enable)
	{
		gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_DISP_BUSY), 0);
		gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_DISP_RESET), 0);
	}
	else
	{
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_BUSY));
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_DISP_RESET));
	}
}
EXPORT_SYMBOL(gpio_neonode_bsl_init);

void gpio_neonode_set_bsl_reset(int val)
{
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_DISP_BUSY), val);
}
EXPORT_SYMBOL(gpio_neonode_set_bsl_reset);

void gpio_neonode_set_bsl_test(int val)
{
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_DISP_RESET), val);
}
EXPORT_SYMBOL(gpio_neonode_set_bsl_test);

void gpio_configure_otg(void)
{
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_PWM2), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_PWM2), 1);
}
EXPORT_SYMBOL(gpio_configure_otg);

void gpio_unconfigure_otg(void)
{
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_PWM2), 0);
}
EXPORT_SYMBOL(gpio_unconfigure_otg);

/* WiFi Power */
int gpio_wifi_power_pin(void)
{
	return MX50_PIN_SD3_WP;
}
EXPORT_SYMBOL(gpio_wifi_power_pin);

/* WiFi Power Enable/Disable */
void gpio_wifi_power_enable(int enable)
{
	/* WiFi Power */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_SD3_WP), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_SD3_WP), enable);
}
EXPORT_SYMBOL(gpio_wifi_power_enable);

/* Audio */
void gpio_audio_output(int enable)
{
#ifdef CONFIG_SND_MXC_SOC
       if (!enable) {
		/* Configure as GPIO, output and pull it low */
		mxc_free_iomux(MX50_PIN_SSI_TXC, IOMUX_CONFIG_ALT0);

		/* clko2 */
		mxc_free_iomux(MX50_PIN_SD1_CMD, IOMUX_CONFIG_ALT7);
		mxc_request_iomux(MX50_PIN_SD1_CMD, IOMUX_CONFIG_ALT1);
		gpio_request(IOMUX_TO_GPIO(MX50_PIN_SD1_CMD), "sd1_cmd");
		gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_SD1_CMD));
       }
       else {
		gpio_free(IOMUX_TO_GPIO(MX50_PIN_SD1_CMD));
		mxc_free_iomux(MX50_PIN_SD1_CMD, IOMUX_CONFIG_ALT1);

		mxc_request_iomux(MX50_PIN_SD1_CMD, IOMUX_CONFIG_ALT7);
		mxc_iomux_set_pad(MX50_PIN_SD1_CMD, PAD_CTL_PKE_ENABLE |
					PAD_CTL_PUE_KEEPER |
					PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE |
					PAD_CTL_SRE_FAST);

		mxc_request_iomux(MX50_PIN_SSI_TXC, IOMUX_CONFIG_ALT0);
		mxc_iomux_set_pad(MX50_PIN_SSI_TXC, PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
					PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE);
       }
#endif
}
EXPORT_SYMBOL(gpio_audio_output);

void gpio_sdhc_active(int module)
{
	int pad_cfg = PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
			PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_NONE;

	if (module == 2) {
		mxc_iomux_set_pad(MX50_PIN_SD3_CMD, 0x1e4);
		mxc_iomux_set_pad(MX50_PIN_SD3_CLK, 0xd4);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, 0x1d4);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, 0x1d4);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, 0x1d4);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, 0x1d4);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, 0x1d4);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, 0x1d4);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, 0x1d4);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, 0x1d4);
	}
	else {
		mxc_iomux_set_pad(MX50_PIN_SD3_CMD, pad_cfg | PAD_CTL_100K_PU);
		mxc_iomux_set_pad(MX50_PIN_SD3_CLK, pad_cfg | PAD_CTL_47K_PU);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, pad_cfg | PAD_CTL_47K_PU);
		mxc_iomux_set_pad(MX50_PIN_SD3_D1, pad_cfg | PAD_CTL_47K_PU);
		mxc_iomux_set_pad(MX50_PIN_SD3_D2, pad_cfg | PAD_CTL_47K_PU);
		mxc_iomux_set_pad(MX50_PIN_SD3_D3, pad_cfg | PAD_CTL_47K_PU);
	}
}
EXPORT_SYMBOL(gpio_sdhc_active);

void gpio_sdhc_inactive(int module)
{
	if (module == 2) {
		mxc_iomux_set_pad(MX50_PIN_SD3_CMD, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_CLK, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_D0, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_D1, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_D2, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_D3, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_D4, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_D5, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_D6, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD3_D7, PAD_CTL_100K_PD);
	}
	else {
		mxc_iomux_set_pad(MX50_PIN_SD2_CMD, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD2_CLK, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD2_D0, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD2_D1, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD2_D2, PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX50_PIN_SD2_D3, PAD_CTL_100K_PD);
	}	
}
EXPORT_SYMBOL(gpio_sdhc_inactive);


int gpio_keypad_active(void)
{
	int pad_val = PAD_CTL_HYS_ENABLE | PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
			PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE | PAD_CTL_100K_PU;

	if (mx50_board_is(BOARD_ID_TEQUILA))
		return 0;

	mxc_request_iomux(MX50_PIN_KEY_COL0, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_COL1, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_COL2, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_COL3, IOMUX_CONFIG_ALT0);

	mxc_iomux_set_pad(MX50_PIN_KEY_COL0, pad_val);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL1, pad_val);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL2, pad_val);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL3, pad_val);

	mxc_request_iomux(MX50_PIN_SD2_D4, IOMUX_CONFIG_ALT3);
	mxc_request_iomux(MX50_PIN_SD2_D6, IOMUX_CONFIG_ALT3);
	mxc_request_iomux(MX50_PIN_SD2_D5, IOMUX_CONFIG_ALT3);
	mxc_request_iomux(MX50_PIN_SD2_D7, IOMUX_CONFIG_ALT3);

	mxc_iomux_set_pad(MX50_PIN_SD2_D4, pad_val);
	mxc_iomux_set_input(MUX_IN_KPP_IPP_IND_COL_6_SELECT_INPUT, INPUT_CTL_PATH0);

	mxc_iomux_set_pad(MX50_PIN_SD2_D6, pad_val);
	mxc_iomux_set_input(MUX_IN_KPP_IPP_IND_COL_7_SELECT_INPUT, INPUT_CTL_PATH0);

	mxc_iomux_set_pad(MX50_PIN_SD2_D5, pad_val);
	mxc_iomux_set_input(MUX_IN_KPP_IPP_IND_ROW_6_SELECT_INPUT, INPUT_CTL_PATH0);

	mxc_iomux_set_pad(MX50_PIN_SD2_D7, pad_val);
	mxc_iomux_set_input(MUX_IN_KPP_IPP_IND_ROW_7_SELECT_INPUT, INPUT_CTL_PATH0);
	
	mxc_request_iomux(MX50_PIN_EIM_DA1, IOMUX_CONFIG_ALT3);
	mxc_request_iomux(MX50_PIN_EIM_DA3, IOMUX_CONFIG_ALT3);

	mxc_iomux_set_pad(MX50_PIN_EIM_DA1, pad_val);
	mxc_iomux_set_input(MUX_IN_KPP_IPP_IND_ROW_4_SELECT_INPUT, INPUT_CTL_PATH2);

	mxc_iomux_set_pad(MX50_PIN_EIM_DA3, pad_val);
	mxc_iomux_set_input(MUX_IN_KPP_IPP_IND_ROW_5_SELECT_INPUT, INPUT_CTL_PATH2);

	mxc_request_iomux(MX50_PIN_KEY_ROW0, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_ROW1, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_ROW2, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_ROW3, IOMUX_CONFIG_ALT0);

	mxc_iomux_set_pad(MX50_PIN_KEY_ROW0, pad_val);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW1, pad_val);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW2, pad_val);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW3, pad_val);

	return 0;
}
EXPORT_SYMBOL(gpio_keypad_active);

int gpio_keypad_inactive(void)
{	
	if (mx50_board_is(BOARD_ID_TEQUILA))
		return 0;

	mxc_request_iomux(MX50_PIN_KEY_COL0, IOMUX_CONFIG_ALT0);
        mxc_request_iomux(MX50_PIN_KEY_COL1, IOMUX_CONFIG_ALT0);
        mxc_request_iomux(MX50_PIN_KEY_COL2, IOMUX_CONFIG_ALT0);
        mxc_request_iomux(MX50_PIN_KEY_COL3, IOMUX_CONFIG_ALT0);
        mxc_request_iomux(MX50_PIN_SD2_D4, IOMUX_CONFIG_ALT3);
        mxc_request_iomux(MX50_PIN_SD2_D6, IOMUX_CONFIG_ALT3);
        mxc_request_iomux(MX50_PIN_SD2_D5, IOMUX_CONFIG_ALT3);
        mxc_request_iomux(MX50_PIN_SD2_D7, IOMUX_CONFIG_ALT3);
	mxc_request_iomux(MX50_PIN_KEY_ROW0, IOMUX_CONFIG_ALT0);
        mxc_request_iomux(MX50_PIN_KEY_ROW1, IOMUX_CONFIG_ALT0);
        mxc_request_iomux(MX50_PIN_KEY_ROW2, IOMUX_CONFIG_ALT0);
        mxc_request_iomux(MX50_PIN_KEY_ROW3, IOMUX_CONFIG_ALT0);

	return 0;       
}
EXPORT_SYMBOL(gpio_keypad_inactive);

void gpio_epdc_pins_enable(int enable)
{
	if (enable == 0) {
		mxc_free_iomux(MX50_PIN_EPDC_SDCE0, IOMUX_CONFIG_ALT0);
		mxc_free_iomux(MX50_PIN_EPDC_GDSP, IOMUX_CONFIG_ALT0);

		mxc_request_iomux(MX50_PIN_EPDC_SDCE0, IOMUX_CONFIG_ALT1);
		mxc_request_iomux(MX50_PIN_EPDC_GDSP, IOMUX_CONFIG_ALT1);

		gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_SDCE0), "epdc_sdce0");
		gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EPDC_SDCE0), 0);

		gpio_request(IOMUX_TO_GPIO(MX50_PIN_EPDC_GDSP), "epdc_gdsp");
		gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_EPDC_GDSP), 0);

		gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_EPDC_SDCE0), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_EPDC_GDSP), 0);
	}
	else {
		mxc_free_iomux(MX50_PIN_EPDC_SDCE0, IOMUX_CONFIG_ALT1);
		mxc_free_iomux(MX50_PIN_EPDC_GDSP, IOMUX_CONFIG_ALT1);

		mxc_request_iomux(MX50_PIN_EPDC_SDCE0, IOMUX_CONFIG_ALT0);
		mxc_request_iomux(MX50_PIN_EPDC_GDSP, IOMUX_CONFIG_ALT0);

		mxc_iomux_set_pad(MX50_PIN_EPDC_SDCE0, PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
						PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE);

		mxc_iomux_set_pad(MX50_PIN_EPDC_GDSP, PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_KEEPER |
						PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE);
	}		
}
EXPORT_SYMBOL(gpio_epdc_pins_enable);

void gpio_ssi_rxc_enable(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_SSI_RXC), enable);

	/* Delay to make sure the I2C line is ready else Papyrus accesses will fail */
	if (enable) 
		mdelay(30);
}
EXPORT_SYMBOL(gpio_ssi_rxc_enable);

void gpio_proximity_reset(void)
{
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_DISP_D7), 0);
	mdelay(10);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_DISP_D7), 1);
}
EXPORT_SYMBOL(gpio_proximity_reset);

/* Called during ship mode */
void gpio_watchdog_low(void)
{
	printk(KERN_CRIT "Halting ...\n");

	/* configure the PMIC for WDIRESET */
	pmic_write_reg(REG_POWER_CTL2, (0 << WDIRESET_LSH), (1 << WDIRESET_LSH));

	/* Turn on RESTARTEN */
	pmic_write_reg(REG_POWER_CTL2, (1 << RESTARTEN_LSH), (1 << RESTARTEN_LSH));

	/* Free WDOG as ALT2 */
	mxc_free_iomux(MX50_PIN_WDOG, IOMUX_CONFIG_ALT2);

	/* Re-request as GPIO */
	mxc_request_iomux(MX50_PIN_WDOG, IOMUX_CONFIG_ALT1);

	/* Pad settings */
	mxc_iomux_set_pad(MX50_PIN_WDOG, PAD_CTL_HYS_NONE | PAD_CTL_PKE_NONE |
				PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE);

	/* Enumerate */
	gpio_request(IOMUX_TO_GPIO(MX50_PIN_WDOG), "wdog");

	/* Direction output */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_WDOG), 0);

	/* Set low */
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_WDOG), 0);
}
EXPORT_SYMBOL(gpio_watchdog_low);

int gpio_headset_value(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_EIM_DA4));
}
EXPORT_SYMBOL(gpio_headset_value);

int gpio_maxim_al32_irq(void)
{
	return gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_OWIRE));
}
EXPORT_SYMBOL(gpio_maxim_al32_irq);

void gpio_watchdog_reboot(void)
{
	/* configure the PMIC for WDIRESET */
        pmic_write_reg(REG_POWER_CTL2, (0 << WDIRESET_LSH), (1 << WDIRESET_LSH));

	/* Free WDOG as ALT2 */
	mxc_free_iomux(MX50_PIN_WDOG, IOMUX_CONFIG_ALT2);

	/* Re-request as GPIO */
	mxc_request_iomux(MX50_PIN_WDOG, IOMUX_CONFIG_ALT1);

	/* Pad settings */
	mxc_iomux_set_pad(MX50_PIN_WDOG, PAD_CTL_HYS_NONE | PAD_CTL_PKE_NONE |
				PAD_CTL_DRV_HIGH | PAD_CTL_ODE_OPENDRAIN_NONE);

	/* Enumerate */
	gpio_request(IOMUX_TO_GPIO(MX50_PIN_WDOG), "wdog");

	/* Direction output */
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_WDOG), 0);

	/* Set low */
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_WDOG), 0);
}
EXPORT_SYMBOL(gpio_watchdog_reboot);

/* gpio_i2c2_scl_toggle: 
 *		Generate I2C2 SCL pulse to recover from slave error
 */
void gpio_i2c2_scl_toggle(void)
{
	int i = 0;
	gpio_direction_output(IOMUX_TO_GPIO(MX50_PIN_I2C2_SCL), 1);
	gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_I2C2_SCL), 1);

	for (i = 0; i < I2C_SCL_RECOVERY_CNT; i++) {
		gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_I2C2_SCL), 0);
		udelay(20);
		gpio_set_value(IOMUX_TO_GPIO(MX50_PIN_I2C2_SCL), 1);
		udelay(20);
	}
}
EXPORT_SYMBOL(gpio_i2c2_scl_toggle);

/* gpio_i2c2_read: Read I2C2 (gpio) line status
 * 		return 1 when SCL is high & SDA line is pulled low
 *		retrun 0 when SCL & SDA is high (normal behavior)
 */
int gpio_i2c2_read(void) 
{
	int ret = 0;
	
	ret = (gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_I2C2_SCL)) && !gpio_get_value(IOMUX_TO_GPIO(MX50_PIN_I2C2_SDA)));
	return ret;
}
EXPORT_SYMBOL(gpio_i2c2_read);

void config_i2c2_gpio_input(int enable)
{
	if (enable) {
	/* Configure I2C2 (SCL & SDA) lines as GPIO/input */
		mxc_request_iomux(MX50_PIN_I2C2_SCL, IOMUX_CONFIG_ALT1);
		gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C2_SCL), "i2c2_scl");
		mxc_iomux_set_pad(MX50_PIN_I2C2_SCL, PAD_CTL_ODE_OPENDRAIN_ENABLE | PAD_CTL_DRV_MAX);
		
		mxc_request_iomux(MX50_PIN_I2C2_SDA, IOMUX_CONFIG_ALT1);
		gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C2_SDA), "i2c2_sda");
		mxc_iomux_set_pad(MX50_PIN_I2C2_SDA, PAD_CTL_ODE_OPENDRAIN_ENABLE | PAD_CTL_DRV_MAX);
	} else {
	/* Configure I2C2 (SCL & SDA) lines to default state (I2C/Output) */
		mxc_free_iomux(MX50_PIN_I2C2_SCL, IOMUX_CONFIG_ALT1);
		mxc_request_iomux(MX50_PIN_I2C2_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
		mxc_iomux_set_pad(MX50_PIN_I2C2_SCL, PAD_CTL_HYS_ENABLE |
								PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
								PAD_CTL_100K_PU);
		mxc_free_iomux(MX50_PIN_I2C2_SDA, IOMUX_CONFIG_ALT1);
		mxc_request_iomux(MX50_PIN_I2C2_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
		mxc_iomux_set_pad(MX50_PIN_I2C2_SDA, PAD_CTL_HYS_ENABLE |
								PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
								PAD_CTL_100K_PU);
	}
}
EXPORT_SYMBOL(config_i2c2_gpio_input);

void gpio_i2c_active(int i2c_num)
{
        if (i2c_num == 0) {     
                mxc_free_iomux(MX50_PIN_I2C1_SCL, IOMUX_CONFIG_ALT1);

                mxc_free_iomux(MX50_PIN_I2C1_SDA, IOMUX_CONFIG_ALT1);

                mxc_request_iomux(MX50_PIN_I2C1_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_iomux_set_pad(MX50_PIN_I2C1_SCL, PAD_CTL_HYS_ENABLE |
                                        PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
                                        PAD_CTL_100K_PU);
                mxc_request_iomux(MX50_PIN_I2C1_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_iomux_set_pad(MX50_PIN_I2C1_SDA, PAD_CTL_HYS_ENABLE |
                                        PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
                                        PAD_CTL_100K_PU);
        }

        if (i2c_num == 1) {              
                mxc_free_iomux(MX50_PIN_I2C2_SCL, IOMUX_CONFIG_ALT1);
                mxc_free_iomux(MX50_PIN_I2C2_SDA, IOMUX_CONFIG_ALT1);        

                mxc_request_iomux(MX50_PIN_I2C2_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_iomux_set_pad(MX50_PIN_I2C2_SCL, PAD_CTL_HYS_ENABLE |
                                        PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
                                        PAD_CTL_100K_PU);
                mxc_request_iomux(MX50_PIN_I2C2_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_iomux_set_pad(MX50_PIN_I2C2_SDA, PAD_CTL_HYS_ENABLE |
                                        PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
                                        PAD_CTL_100K_PU);
        }
        if (i2c_num == 2) {              
                mxc_free_iomux(MX50_PIN_I2C3_SCL, IOMUX_CONFIG_ALT1);
                mxc_free_iomux(MX50_PIN_I2C3_SDA, IOMUX_CONFIG_ALT1);        

                mxc_request_iomux(MX50_PIN_I2C3_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_iomux_set_pad(MX50_PIN_I2C3_SCL, PAD_CTL_HYS_ENABLE |
                                        PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
                                        PAD_CTL_100K_PU);
                mxc_request_iomux(MX50_PIN_I2C3_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_iomux_set_pad(MX50_PIN_I2C3_SDA, PAD_CTL_HYS_ENABLE |
                                        PAD_CTL_DRV_MAX | PAD_CTL_ODE_OPENDRAIN_ENABLE |
                                        PAD_CTL_100K_PU);
        }
}

EXPORT_SYMBOL(gpio_i2c_active);

void gpio_i2c_inactive(int i2c_num)
{
        if (i2c_num == 0) {
                mxc_free_iomux(MX50_PIN_I2C1_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_request_iomux(MX50_PIN_I2C1_SCL, IOMUX_CONFIG_ALT1);
                gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C1_SCL), "i2c1_scl");
                gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_I2C1_SCL));
                
                mxc_free_iomux(MX50_PIN_I2C1_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_request_iomux(MX50_PIN_I2C1_SDA, IOMUX_CONFIG_ALT1);
                gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C1_SDA), "i2c1_sda");
                gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_I2C1_SDA));
        }

        if (i2c_num == 1) {
                mxc_free_iomux(MX50_PIN_I2C2_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_request_iomux(MX50_PIN_I2C2_SCL, IOMUX_CONFIG_ALT1);
                gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C2_SCL), "i2c2_scl");
                gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_I2C2_SCL));

                mxc_free_iomux(MX50_PIN_I2C2_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_request_iomux(MX50_PIN_I2C2_SDA, IOMUX_CONFIG_ALT1);
                gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C2_SDA), "i2c2_sda");
                gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_I2C2_SDA));
        }

        if (i2c_num == 2) {
                mxc_free_iomux(MX50_PIN_I2C3_SCL, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_request_iomux(MX50_PIN_I2C3_SCL, IOMUX_CONFIG_ALT1);
                gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C3_SCL), "i2c3_scl");
                gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_I2C3_SCL));

                mxc_free_iomux(MX50_PIN_I2C3_SDA, IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
                mxc_request_iomux(MX50_PIN_I2C3_SDA, IOMUX_CONFIG_ALT1);
                gpio_request(IOMUX_TO_GPIO(MX50_PIN_I2C3_SDA), "i2c3_sda");
                gpio_direction_input(IOMUX_TO_GPIO(MX50_PIN_I2C3_SDA));
        }
}
EXPORT_SYMBOL(gpio_i2c_inactive);


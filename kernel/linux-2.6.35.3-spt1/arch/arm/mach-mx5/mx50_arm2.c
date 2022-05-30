/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
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

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/nodemask.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/ata.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tps65180.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/videodev2.h>
#include <linux/mxcfb.h>
#include <linux/fec.h>
#include <linux/gpmi-nfc.h>
#include <linux/usb/android_composite.h>
#include <linux/power_supply.h>
#ifdef CONFIG_REGULATOR_WM831X
#include <linux/mfd/wm831x/pdata.h>
#include <linux/mfd/wm831x/core.h>
#include <linux/mfd/sub-main/core.h>
#endif
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/flash.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/arc_otg.h>
#include <mach/memory.h>
#include <mach/gpio.h>
#include <mach/mmc.h>
#include <mach/mxc_dvfs.h>
#include <mach/iomux-mx50.h>
#include <mach/i2c.h>
#ifdef CONFIG_REGULATOR_TWL4030 
#include <linux/i2c/twl.h>
#include <linux/i2c/twl-6025.h>
#endif

#if 1 /* E_BOOK *//* for LED 2011/2/21 */
#include <linux/leds_pwm.h>
#endif
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif

#ifdef CONFIG_KEYBOARD_GPIO /* E_BOOK */ 
/* Support FY11 */
#include <linux/input.h>
#include <linux/gpio_keys.h>
#endif /* E_BOOK */

#include "devices.h"
#include "crm_regs.h"
#include "usb.h"
#include "dma-apbh.h"
#include "mx50_gpio.h"

#define AFTER_PMIC_INTEGRATED
#define MAX_REG_TRANS_TIME 220

#define SD1_WP	(4*32 + 17)	/*GPIO_5_17 */
#define SD1_CD	(1*32 + 19)	/*GPIO_2_19 */
#define SD2_WP	(4*32 + 16)	/*GPIO_5_16 */
#define SD2_CD	(5*32 + 24) /*GPIO_6_24 */
#define SD3_WP	(4*32 + 28) /*GPIO_5_28 */
#define SD3_CD	(3*32 + 4) /*GPIO_4_4 */
#define PWR_INT		(3*32 + 1)	/*GPIO_4_1 */

#define EPDC_D0		(2*32 + 0)	/*GPIO_3_0 */
#define EPDC_D1		(2*32 + 1)	/*GPIO_3_1 */
#define EPDC_D2		(2*32 + 2)	/*GPIO_3_2 */
#define EPDC_D3		(2*32 + 3)	/*GPIO_3_3 */
#define EPDC_D4		(2*32 + 4)	/*GPIO_3_4 */
#define EPDC_D5		(2*32 + 5)	/*GPIO_3_5 */
#define EPDC_D6		(2*32 + 6)	/*GPIO_3_6 */
#define EPDC_D7		(2*32 + 7)	/*GPIO_3_7 */
#define EPDC_GDCLK	(2*32 + 16)	/*GPIO_3_16 */
#define EPDC_GDSP	(2*32 + 17)	/*GPIO_3_17 */
#define EPDC_GDOE	(2*32 + 18)	/*GPIO_3_18 */
#define EPDC_GDRL	(2*32 + 19)	/*GPIO_3_19 */
#define EPDC_SDCLK	(2*32 + 20)	/*GPIO_3_20 */
#define EPDC_SDOE	(2*32 + 23)	/*GPIO_3_23 */
#define EPDC_SDLE	(2*32 + 24)	/*GPIO_3_24 */
#define EPDC_SDSHR	(2*32 + 26)	/*GPIO_3_26 */
#define EPDC_BDR0	(3*32 + 23)	/*GPIO_4_23 */
#define EPDC_BDR1	(3*32 + 24)	/*GPIO_4_24 */
#define EPDC_SDCE0	(3*32 + 25)	/*GPIO_4_25 */
#define EPDC_SDCE1	(3*32 + 26)	/*GPIO_4_26 */
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
#define EPDC_SDCE2	(3*32 + 27)	/*GPIO_4_27 */
#endif /* E_BOOK */

#if 1 /* E_BOOK *//* Disable FEC Switch 2011/1/28 */
#define BOARD_ID	(5*32+14)	/* GPIO_6_14 */
#endif

/* 2011/05/26 FY11 : Supported TPS65181. */
#define EPDC_SDOEZ	(2*32 + 21) /*GPIO_3_21 */

/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
#define EPD_PMIC_VSYS	(2*32 + 11) /* GPIO_3_11 */

#define USB_OTG_PWR	(5*32 + 25) /*GPIO_6_25*/
#define EPDC_PMIC_WAKE     (2*32 + 8)  /*GPIO_3_8 */
#define EPDC_PMIC_INT      (2*32 + 10) /*GPIO_3_10 */
#define EPDC_VCOM  (2*32 + 27) /*GPIO_3_27 */
#define EPDC_PWRSTAT	(2*32 + 28)	/*GPIO_3_28 */
#define EPDC_ELCDIF_BACKLIGHT	(1*32 + 18)	/*GPIO_2_18 */
#define CSPI_CS1	(3*32 + 13)	/*GPIO_4_13 */
#define CSPI_CS2	(3*32 + 11) /*GPIO_4_11*/
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
#define EPDC_PWR2_CTRL (2*32 + 31) /*GPIO_3_31 */
#endif /* E_BOOK */
#define EPDC_PWR0_CTRL (2*32 + 9) /*GPIO_3_9 */

#define DISP_D14 (1*32 + 14)
#define WAKE_ON_WLAN (4*32 + 26) /*GPIO_5_26 */
#define CHIP_PWD_L (4*32 + 27) /*GPIO_5_27 */
#define WLAN_POWER_EN (4*32 + 25) /*GPIO_5_25 */
#define WWAN_EN   (2*32 + 22) /*GPIO_3_22 */

#ifdef CONFIG_KEYBOARD_MXC /* E_BOOK */
/* Suppoert config change */
/* Not support FY11 */
#define UIKEY_0   (2*32 + 12) /*GPIO_3_12 */
#endif /* E_BOOK */

#define REBOOT_REQ     (2*32+19) /* GPIO_3_19 */
#define SPI_WAKE_INT   (2*32+25) /* GPIO_3_25 */
#define SPI_SUB_INT    (2*32+26) /* GPIO_3_26 */
#define SPI_RCV_RDY    (3*32+23) /* GPIO_4_23 */
#define YOBI_MS1       (3*32+24) /* GPIO_4_24 */
#ifdef CONFIG_REGULATOR_WM831X
/* for POWER3 2011/05/06 */
#define YOBI_MW0       (3*32+0) /* GPIO_4_0 */
#define PMIC_IRQ       (3*32+1) /* GPIO_4_1 */
#define AU_POWER_EN    (3*32+2) /* GPIO_4_2 */
#define YOBI_MS0       (3*32+14) /* GPIO_4_14 */
#define NVCC_SD2_EN    (3*32+17) /* GPIO_4_17 */
#define YOBI_MW1       (5*32+27) /* GPIO_6_27 */
#define USB_H1_VDDA25_EN (1*32+14) /* GPIO_2_14 */
#endif
#define POWER3_DET     (5*32+17) /* GPIO_6_17 */

#define LCD_VSYNC      (1*32+17)  /* GPIO_2_17 */
#define SET_ID_YOBI    (1*32+20)  /* GPIO_2_20 */

/* for FEC disable GPIO */
#define GPIO2_0		(1*32+0) /* GPIO_2_0 */
#define GPIO2_1		(1*32+1) /* GPIO_2_1 */
#define GPIO2_2		(1*32+2) /* GPIO_2_2 */
#define GPIO2_3		(1*32+3) /* GPIO_2_3 */
#define GPIO2_4		(1*32+4) /* GPIO_2_4 */
#define GPIO2_5		(1*32+5) /* GPIO_2_5 */
#define GPIO2_6		(1*32+6) /* GPIO_2_6 */
#define GPIO2_7		(1*32+7) /* GPIO_2_7 */
#define GPIO6_4		(5*32+4) /* GPIO_6_4 */
#define GPIO6_5		(5*32+5) /* GPIO_6_5 */
#define FEC_RESET	(5*32+28) /* GPIO_6_28 */

/* SD2 disable 2011/06/15 */
#define GPIO5_6		(4*32+6) /* GPIO_5_6 */
#define GPIO5_7		(4*32+7) /* GPIO_5_7 */
#define GPIO5_8		(4*32+8) /* GPIO_5_8 */
#define GPIO5_9		(4*32+9) /* GPIO_5_9 */
#define GPIO5_10	(4*32+10) /* GPIO_5_10 */
#define GPIO5_11	(4*32+11) /* GPIO_5_11*/

#define VDDGP_UV_MAX 1200000
#define VDDGP_UV_MIN 1000000

extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
extern void phxlit_vbus_callback(int on);
extern void charger_detection_disable(void);

#ifdef CONFIG_REGULATOR_TWL4030 
extern int confirm_the_di(void);
extern int twl6030_set_power_source(int source);
#endif

#ifdef CONFIG_REGULATOR_WM831X
extern struct sub_cpu_bci_device_info *confirm_the_di(void);
extern int sub_cpu_set_power_source(int source);
extern void send_shutdown_request(void);
#endif

#if 1 /*E_BOOK forWiFi*/
extern void mxc_mmc_force_detect(int id);
static int wlan_card_status = 0;
#endif

static struct pad_desc  mx50_armadillo2[] = {
#if 1 /* E_BOOK *//* Change for Reader 2010/12/24 */
	/* SD1 - SD/MMC */
	MX50_PAD_SD2_CD__GPIO_5_17, //Write Protect
	MX50_PAD_DISP_RD__GPIO_2_19, //Card Detect
	MX50_PAD_SD1_CMD__SD1_CMD,
	MX50_PAD_SD1_CLK__SD1_CLK,
	MX50_PAD_SD1_D0__SD1_D0,
	MX50_PAD_SD1_D1__SD1_D1,
	MX50_PAD_SD1_D2__SD1_D2,
	MX50_PAD_SD1_D3__SD1_D3,

	/* SD2 - don't use. close port  */
	MX50_PAD_PWM1__GPIO_6_24, //Card Detct
	MX50_PAD_SD2_CMD__GPIO_5_7,
	MX50_PAD_SD2_CLK__GPIO_5_6,
	MX50_PAD_SD2_D0__GPIO_5_8,
	MX50_PAD_SD2_D1__GPIO_5_9,
	MX50_PAD_SD2_D2__GPIO_5_10,
	MX50_PAD_SD2_D3__GPIO_5_11,
	
	/* SD3 - eMMC */
	MX50_PAD_SD3_CMD__SD3_CMD,
	MX50_PAD_SD3_CLK__SD3_CLK,
	MX50_PAD_SD3_D0__SD3_D0,
	MX50_PAD_SD3_D1__SD3_D1,
	MX50_PAD_SD3_D2__SD3_D2,
	MX50_PAD_SD3_D3__SD3_D3,
	MX50_PAD_DISP_CS__GPIO_2_21, /* eSD Detect */
	
	/* SD4 - WLAN (disable setting) */
	MX50_PAD_DISP_D8__GPIO_2_8,
	MX50_PAD_DISP_D9__GPIO_2_9,
	MX50_PAD_DISP_D10__GPIO_2_10,
	MX50_PAD_DISP_D11__GPIO_2_11,
	MX50_PAD_DISP_D12__GPIO_2_12,
	MX50_PAD_DISP_D13__GPIO_2_13,

	/* Audio */
	MX50_PAD_AUD_MUTE,
	MX50_PAD_AUD_RESET,
	MX50_PAD_HP_DETECT,
	MX50_PAD_SSI_TXD__SSI_TXD,
	MX50_PAD_SSI_TXC__SSI_TXC,
	MX50_PAD_SSI_TXFS__SSI_TXFS,
	MX50_PAD_OWIRE__SSI_EXT1_CLK, /* AUDIO Master clock */
	
	/* UART pad setting */
	MX50_PAD_UART1_TXD__UART1_TXD,
	MX50_PAD_UART1_RXD__UART1_RXD,
	MX50_PAD_ECSPI2_MISO_UART5_TXD,
	MX50_PAD_ECSPI2_SS0_UART5_RXD,
#if 0 /* E_BOOK *//* delete 2011/2/2 */
	MX50_PAD_UART2_TXD__UART2_TXD,
	MX50_PAD_UART2_RXD__UART2_RXD,
	MX50_PAD_UART2_CTS__UART2_CTS,
	MX50_PAD_UART2_RTS__UART2_RTS,
#endif
	
	/* Power */
#ifdef CONFIG_REGULATOR_TWL4030 
	MX50_PAD_KEY_COL0__GPIO_4_0, /* /GAUGE_ALRT */
#else 
#ifdef CONFIG_REGULATOR_WM831X 
	/* for POWER3 2011/05/06 */
	MX50_PAD_KEY_COL0__GPIO_4_0, /* YOBI_MW0 */
	MX50_PAD_ECSPI1_MOSI__GPIO_4_13, /* NOR CS */
#endif
#endif
	MX50_PAD_KEY_ROW0__GPIO_4_1, /* PMIC_IRQ */
	MX50_PAD_KEY_COL1__GPIO_4_2, /* AU_POWER_EN */
	MX50_PAD_EPDC_GDRL__GPIO_3_19,    /* REBOOT_REQ */
	MX50_PAD_EPDC_SDCLKN__GPIO_3_25,  /* SPI_WAKE_INT */
	MX50_PAD_EPDC_SDSHR__GPIO_3_26,   /* SPI_SUB_INT */
	MX50_PAD_EPDC_BDR0__GPIO_4_23, /* SPI_RCV_RDY */
	MX50_PAD_EPDC_BDR1__GPIO_4_24, /* YOBI_MS1 */
	MX50_PAD_EPDC_SDCE1__GPIO_4_26,
#ifdef CONFIG_REGULATOR_WM831X 
	MX50_PAD_CSPI_SCLK__CSPI_SCLK,
	MX50_PAD_CSPI_MOSI__CSPI_MOSI,
	MX50_PAD_CSPI_MISO__CSPI_MISO,
	MX50_PAD_ECSPI1_MOSI__CSPI_SS1, /* CSPI_SS1_NOR */
	MX50_PAD_ECSPI1_MISO__GPIO_4_14, /* YOBI_MS0 */
	MX50_PAD_ECSPI2_MOSI__GPIO_4_17, /* NVCC_SD2_EN */
#endif
	MX50_PAD_SD2_D4__GPIO_5_12,  /* NVCC_SD1_EN */
	MX50_PAD_SD2_WP__GPIO_5_16,  /* EMMC_VCC_EN */
	MX50_PAD_SD3_D4__GPIO_5_24,  /* LOG_INT */
#ifdef CONFIG_REGULATOR_TWL4030 
	MX50_PAD_EPITO__GPIO_6_27,   /* MSECURE */
#else
#ifdef CONFIG_REGULATOR_WM831X 
	MX50_PAD_DISP_D14__GPIO_2_14, /* USB_H1_VDDA25_EN */
	MX50_PAD_UART4_RXD__GPIO_6_17, /* POWER3_DET */
	MX50_PAD_I2C1_SCL__I2C1_SCL, /* I2C1_SCL (POWER/DVFS) */
	MX50_PAD_I2C1_SDA__I2C1_SDA, /* I2C1_SDA (POWER/DVFS) */
	MX50_PAD_EPITO__GPIO_6_27,   /* YOBI_MW1 */
#endif
#endif
	MX50_PAD_UART4_RXD__GPIO_6_17, /* POWER3_DET */

	/* I2C */
#ifdef CONFIG_REGULATOR_TWL4030 
	MX50_PAD_I2C1_SCL__I2C1_SCL,
	MX50_PAD_I2C1_SDA__I2C1_SDA,
#endif
	MX50_PAD_I2C2_SCL__I2C2_SCL,
	MX50_PAD_I2C2_SDA__I2C2_SDA,
	MX50_PAD_I2C3_SCL__I2C3_SCL,
	MX50_PAD_I2C3_SDA__I2C3_SDA,
	
	/* IR TouchPanel */
	MX50_PAD_ECSPI1_SCLK__GPIO_4_12,  /* TP_INT */
	MX50_PAD_ECSPI1_SS0__GPIO_4_15,   /* TP_SBWTDIO */
	MX50_PAD_ECSPI2_SCLK__GPIO_4_16,  /* TP_SBWTCK */
#if 1  /* E_BOOK *//* Serial Switcher  2011/2/2 */
	MX50_PAD_UART2_CTS__GPIO_6_12,
	MX50_PAD_UART2_RTS__GPIO_6_13,
#else	
	MX50_PAD_UART2_CTS__UART2_CTS,
	MX50_PAD_UART2_RTS__UART2_RTS,
#endif
#if 0 /* E_BOOK *//* delete for leak signal at cold boot 2011/08/23 */
	MX50_PAD_UART2_RXD__UART2_RXD,
	MX50_PAD_UART2_TXD__UART2_TXD,
#endif
	
	/* SubCPU */
#if 1  /* E_BOOK *//* Serial Switcher  2011/2/2 */
	MX50_PAD_UART1_CTS__GPIO_6_8,
	MX50_PAD_UART1_RTS__GPIO_6_9,
#else
	MX50_PAD_UART1_CTS__UART1_CTS,
	MX50_PAD_UART1_RTS__UART1_RTS,
#endif
	MX50_PAD_UART1_RXD__UART1_RXD,
	MX50_PAD_UART1_TXD__UART1_TXD,
	
	/* EPDC */
	MX50_PAD_DISP_BUSY__GPIO_2_18,    /* EPD_VENDOR */
	MX50_PAD_EPDC_D10__GPIO_3_10,     /* /EPDC_INT */
	MX50_PAD_EPDC_D11__GPIO_3_11,     /* EPDC_WAKEUP_DRY */
	MX50_PAD_EPDC_D8__GPIO_3_8,       /* EPDC_WAKEUP */
	MX50_PAD_EPDC_D9__GPIO_3_9,       /* EPDC_PWRUP */
/* 2011/1/22 FY11 : Fixed the bug cannot controll PWRCOM */
	MX50_PAD_EPDC_PWRCOM__GPIO_3_27,	/* EPDC_PWRUP */
/* 2011/1/26 FY11 : Fixed the bug cannot read PWRSTAT status. */
	MX50_PAD_EPDC_PWRSTAT__GPIO_3_28,

	/* FEC */
	MX50_PAD_SSI_RXC__FEC_MDIO,
	MX50_PAD_DISP_D0__FEC_TXCLK,
	MX50_PAD_DISP_D1__FEC_RX_ER,
	MX50_PAD_DISP_D2__FEC_RX_DV,
	MX50_PAD_DISP_D3__FEC_RXD1,
	MX50_PAD_DISP_D4__FEC_RXD0,
	MX50_PAD_DISP_D5__FEC_TX_EN,
	MX50_PAD_DISP_D6__FEC_TXD1,
	MX50_PAD_DISP_D7__FEC_TXD0,
	MX50_PAD_SSI_RXFS__FEC_MDC,
	
	/* */
	MX50_PAD_CSPI_SS0__CSPI_SS0,
	MX50_PAD_ECSPI1_MOSI__CSPI_SS1,
#ifdef CONFIG_REGULATOR_TWL4030 
	MX50_PAD_CSPI_MOSI__CSPI_MOSI,
	MX50_PAD_CSPI_MISO__CSPI_MISO,
#endif
	
	/* GPIO for DEBUG */
	MX50_PAD_EIM_OE__GPIO_1_24,
	MX50_PAD_EIM_RW__GPIO_1_25,
	MX50_PAD_EIM_LBA__GPIO_1_26,
	MX50_PAD_EIM_CRE__GPIO_1_27,
	MX50_PAD_UART3_TXD__GPIO_6_14, /* BOARD ID */
	MX50_PAD_UART3_RXD__GPIO_6_15, /* SET_ID0 */
	MX50_PAD_UART4_TXD__GPIO_6_16, /* SET_ID1 */
	
	/* LED */
#if 0 /* E_BOOK *//* delete for LED 2010/12/27 */
	MX50_PAD_PWM2__GPIO_6_25, /* Orange LED */
#else
	MX50_PAD_PWM2__PWM2, /* Orange LED */
#endif
	MX50_PAD_DISP_WR__GPIO_2_16, /* slot LED */

#if 0	/* E_BOOK */
	/* WLAN USER SWITCH */
	MX50_PAD_EPDC_SDOEZ__GPIO_3_21,
#endif	/* E_BOOK */
	MX50_PAD_SD3_D6__GPIO_5_26,	//WAKE_ON_WLAN
	MX50_PAD_SD3_D7__GPIO_5_27,	//CHIP_PWD_L
	MX50_PAD_KEY_ROW1__GPIO_4_3,//WAN_POWER_EN
	MX50_PAD_SD3_D5__GPIO_5_25,	//WLAN_POWER_EN
	MX50_PAD_EPDC_SDOED__GPIO_3_22, //WWAN_EN

#ifdef CONFIG_KEYBOARD_MXC /* E_BOOK */
	/* Suppoert config change */
	/* Not support FY11 */
	MX50_PAD_EPDC_D12__GPIO_3_12, //UIKEY_0
#endif /* E_BOOK */

	/* misc */
	MX50_PAD_EPDC_PWRCTRL0__GPIO_3_29,
	MX50_PAD_EPDC_PWRCTRL1__GPIO_3_30,
#else
	/* SD1 - SD/MMC */
	MX50_PAD_SD2_CD__GPIO_5_17, //Write Protect
	MX50_PAD_DISP_RD__GPIO_2_19, //Card Detect
	MX50_PAD_SD1_CMD__SD1_CMD,

	MX50_PAD_SD1_CLK__SD1_CLK,
	MX50_PAD_SD1_D0__SD1_D0,
	MX50_PAD_SD1_D1__SD1_D1,
	MX50_PAD_SD1_D2__SD1_D2,
	MX50_PAD_SD1_D3__SD1_D3,

	/* SD2 - Memory Stick */
	MX50_PAD_PWM1__GPIO_6_24, //Card Detct
	MX50_PAD_SD2_CMD__SD2_CMD,
	MX50_PAD_SD2_CLK__SD2_CLK,
	MX50_PAD_SD2_D0__SD2_D0,
	MX50_PAD_SD2_D1__SD2_D1,
	MX50_PAD_SD2_D2__SD2_D2,
	MX50_PAD_SD2_D3__SD2_D3,
	MX50_PAD_SD2_D4__SD2_D4,
	MX50_PAD_SD2_D5__SD2_D5,

	/* SD3 */
	MX50_PAD_SD3_WP__GPIO_5_28,
	MX50_PAD_KEY_COL2__GPIO_4_4,
	MX50_PAD_SD3_CMD__SD3_CMD,
	MX50_PAD_SD3_CLK__SD3_CLK,
	MX50_PAD_SD3_D0__SD3_D0,
	MX50_PAD_SD3_D1__SD3_D1,
	MX50_PAD_SD3_D2__SD3_D2,
	MX50_PAD_SD3_D3__SD3_D3,

/* 2011/05/26 FY11 : Supported TPS65181. */
	/* port to identify EPD PMIC type. */
	MX50_PAD_EPDC_SDOEZ__GPIO_3_21,

	/* PWR_INT */
	MX50_PAD_KEY_ROW0__GPIO_4_1,

	/* log int */
	MX50_PAD_LOG_INT,

	/* Audio */
	MX50_PAD_AUD_MUTE,
	MX50_PAD_AUD_RESET,
	MX50_PAD_HP_DETECT,
	MX50_PAD_SSI_TXD__SSI_TXD,
	MX50_PAD_SSI_TXC__SSI_TXC,
	MX50_PAD_SSI_TXFS__SSI_TXFS,
	MX50_PAD_OWIRE__SSI_EXT1_CLK,	/* AUDIO Master clock */

	/* UART pad setting */
	MX50_PAD_UART1_TXD__UART1_TXD,
	MX50_PAD_UART1_RXD__UART1_RXD,
	MX50_PAD_ECSPI2_MISO_UART5_TXD,
	MX50_PAD_ECSPI2_SS0_UART5_RXD,
	MX50_PAD_UART2_TXD__UART2_TXD,
	MX50_PAD_UART2_RXD__UART2_RXD,
	MX50_PAD_UART2_CTS__UART2_CTS,
	MX50_PAD_UART2_RTS__UART2_RTS,
	MX50_PAD_UART1_CTS__UART1_CTS,
	MX50_PAD_UART1_RTS__UART1_RTS,

	MX50_PAD_I2C1_SCL__I2C1_SCL,
	MX50_PAD_I2C1_SDA__I2C1_SDA,
	MX50_PAD_I2C2_SCL__I2C2_SCL,
	MX50_PAD_I2C2_SDA__I2C2_SDA,
	MX50_PAD_I2C3_SCL__I2C3_SCL,
	MX50_PAD_I2C3_SDA__I2C3_SDA,

	/* EPDC pins */
	MX50_PAD_EPDC_D0__EPDC_D0,
	MX50_PAD_EPDC_D1__EPDC_D1,
	MX50_PAD_EPDC_D2__EPDC_D2,
	MX50_PAD_EPDC_D3__EPDC_D3,
	MX50_PAD_EPDC_D4__EPDC_D4,
	MX50_PAD_EPDC_D5__EPDC_D5,
	MX50_PAD_EPDC_D6__EPDC_D6,
	MX50_PAD_EPDC_D7__EPDC_D7,
	MX50_PAD_EPDC_GDCLK__EPDC_GDCLK,
	MX50_PAD_EPDC_GDSP__EPDC_GDSP,
	MX50_PAD_EPDC_GDOE__EPDC_GDOE	,
	MX50_PAD_EPDC_GDRL__EPDC_GDRL,
	MX50_PAD_EPDC_SDCLK__EPDC_SDCLK,
	MX50_PAD_EPDC_SDOE__EPDC_SDOE,
	MX50_PAD_EPDC_SDLE__EPDC_SDLE,
	MX50_PAD_EPDC_SDSHR__EPDC_SDSHR,
	MX50_PAD_EPDC_BDR0__EPDC_BDR0,
	MX50_PAD_EPDC_BDR1__EPDC_BDR1,
	MX50_PAD_EPDC_SDCE0__EPDC_SDCE0,
	MX50_PAD_EPDC_SDCE1__EPDC_SDCE1,
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	MX50_PAD_EPDC_SDCE2__EPDC_SDCE2,
#endif /* E_BOOK */

	MX50_PAD_EPDC_PWRSTAT__GPIO_3_28,
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	MX50_PAD_EPDC_VCOM0__GPIO_4_21,
#endif /* E_BOOK */

	MX50_PAD_DISP_D8__DISP_D8,
	MX50_PAD_DISP_D9__DISP_D9,
	MX50_PAD_DISP_D10__DISP_D10,
	MX50_PAD_DISP_D11__DISP_D11,
	MX50_PAD_DISP_D12__DISP_D12,
	MX50_PAD_DISP_D13__DISP_D13,
//	MX50_PAD_DISP_D14__DISP_D14, the LDO control for USB host test
	MX50_PAD_DISP_D15__DISP_D15,
	MX50_PAD_DISP_RS__ELCDIF_VSYNC,

	/* ELCDIF contrast */
	MX50_PAD_DISP_BUSY__GPIO_2_18,

	MX50_PAD_DISP_CS__ELCDIF_HSYNC,
	MX50_PAD_DISP_RD__ELCDIF_EN,
	MX50_PAD_DISP_WR__ELCDIF_PIXCLK,

	/* EPD PMIC WAKEUP */
	MX50_PAD_UART4_TXD__GPIO_6_16,

	/* EPD PMIC intr */
	MX50_PAD_UART4_RXD__GPIO_6_17,

	MX50_PAD_EPITO__USBH1_PWR,
	/* Need to comment below line if
	 * one needs to debug owire.
	 */
	MX50_PAD_OWIRE__SSI_EXT1_CLK,

	/* using gpio to control otg pwr */
	MX50_PAD_PWM2__GPIO_6_25,
	MX50_PAD_PWM1__USBOTG_OC,

	MX50_PAD_SSI_RXC__FEC_MDIO,
	MX50_PAD_SSI_RXC__FEC_MDIO,
	MX50_PAD_DISP_D0__FEC_TXCLK,
	MX50_PAD_DISP_D1__FEC_RX_ER,
	MX50_PAD_DISP_D2__FEC_RX_DV,
	MX50_PAD_DISP_D3__FEC_RXD1,
	MX50_PAD_DISP_D4__FEC_RXD0,
	MX50_PAD_DISP_D5__FEC_TX_EN,
	MX50_PAD_DISP_D6__FEC_TXD1,
	MX50_PAD_DISP_D7__FEC_TXD0,
	MX50_PAD_SSI_RXFS__FEC_MDC,

	MX50_PAD_CSPI_SS0__CSPI_SS0,
	MX50_PAD_ECSPI1_MOSI__CSPI_SS1,
	MX50_PAD_CSPI_MOSI__CSPI_MOSI,
	MX50_PAD_CSPI_MISO__CSPI_MISO,
	
	MX50_PAD_SD3_D6__GPIO_5_26,	//WAKE_ON_WLAN
	MX50_PAD_SD3_D7__GPIO_5_27,	//CHIP_PWD_L
	MX50_PAD_KEY_ROW1__GPIO_4_3,//WAN_POWER_EN
    MX50_PAD_SD3_D5__GPIO_5_25,
#ifdef CONFIG_KEYBOARD_MXC /* E_BOOK */
/* Suppoert config change */
/* Not support FY11 */
    MX50_PAD_EPDC_D12__GPIO_3_12, //UIKEY_0
#endif /* E_BOOK */
#endif
};

static struct pad_desc mxc_sd4_wifi_active[] = {
	MX50_PAD_DISP_D8__SD4_CMD,
	MX50_PAD_DISP_D9__SD4_CLK,
	MX50_PAD_DISP_D10__SD4_D0,
	MX50_PAD_DISP_D11__SD4_D1,
	MX50_PAD_DISP_D12__SD4_D2,
	MX50_PAD_DISP_D13__SD4_D3,
};
static struct pad_desc mxc_sd4_wifi_active_bb[] = {
	MX50_PAD_DISP_D8__SD4_CMD_BB,
	MX50_PAD_DISP_D9__SD4_CLK_BB,
	MX50_PAD_DISP_D10__SD4_D0_BB,
	MX50_PAD_DISP_D11__SD4_D1_BB,
	MX50_PAD_DISP_D12__SD4_D2_BB,
	MX50_PAD_DISP_D13__SD4_D3_BB,
};

static struct pad_desc mxc_sd4_wifi_inactive[] = {
	MX50_PAD_DISP_D8__GPIO_2_8,
	MX50_PAD_DISP_D9__GPIO_2_9,
	MX50_PAD_DISP_D10__GPIO_2_10,
	MX50_PAD_DISP_D11__GPIO_2_11,
	MX50_PAD_DISP_D12__GPIO_2_12,
	MX50_PAD_DISP_D13__GPIO_2_13,
};

static int mxc_sd4_gpio[] = {
	(1*32 + 8),
	(1*32 + 9),
	(1*32 + 10),
	(1*32 + 11),
	(1*32 + 12),
	(1*32 + 13),
};

static char* mxc_sd4_gpio_name[] = {
    "sdhc4_cmd",
    "sdhc4_clk",
    "sdhc4_d0",
    "sdhc4_d1",
    "sdhc4_d2",
    "sdhc4_d3"
};   

static struct pad_desc mxc_fec_inactive[] = {
  MX50_PAD_DISP_D0__GPIO_2_0,
  MX50_PAD_DISP_D1__GPIO_2_1,
  MX50_PAD_DISP_D2__GPIO_2_2,
  MX50_PAD_DISP_D3__GPIO_2_3,
  MX50_PAD_DISP_D4__GPIO_2_4,
  MX50_PAD_DISP_D5__GPIO_2_5,
  MX50_PAD_DISP_D6__GPIO_2_6,
  MX50_PAD_DISP_D7__GPIO_2_7,
  MX50_PAD_SSI_RXFS__GPIO_6_4,
  MX50_PAD_SSI_RXC__GPIO_6_5
};

static struct mxc_dvfs_platform_data dvfs_core_data = {
	.reg_id = "SMPS1",
	.clk1_id = "cpu_clk",
	.clk2_id = "gpc_dvfs_clk",
	.gpc_cntr_offset = MXC_GPC_CNTR_OFFSET,
	.gpc_vcr_offset = MXC_GPC_VCR_OFFSET,
	.ccm_cdcr_offset = MXC_CCM_CDCR_OFFSET,
	.ccm_cacrr_offset = MXC_CCM_CACRR_OFFSET,
	.ccm_cdhipr_offset = MXC_CCM_CDHIPR_OFFSET,
	.prediv_mask = 0x1F800,
	.prediv_offset = 11,
	.prediv_val = 3,
	.div3ck_mask = 0xE0000000,
	.div3ck_offset = 29,
	.div3ck_val = 2,
	.emac_val = 0x08,
	.upthr_val = 25,
	.dnthr_val = 9,
	.pncthr_val = 33,
	.upcnt_val = 10,
	.dncnt_val = 10,
	.delay_time = MAX_REG_TRANS_TIME,
	.num_wp = 2,
};

/* working point(wp): 0 - 800MHz; 1 - 166.25MHz; */
static struct cpu_wp cpu_wp_auto[] = {
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 800000000,
	 .pdf = 0,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 0,
	 .cpu_voltage = 1025000,},
	{
	.pll_rate = 800000000,
	.cpu_rate = 400000000,
	.pdf = 1,
	.mfi = 8,
	.mfd = 2,
	.mfn = 1,
	.cpu_podf = 1,
	.cpu_voltage = 900000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 160000000,
	 .pdf = 4,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 4,
	 .cpu_voltage = 850000,},
};

static int num_cpu_wp = ARRAY_SIZE(cpu_wp_auto);

static struct cpu_wp *mx50_arm2_get_cpu_wp(int *wp)
{
	*wp = num_cpu_wp;
	return cpu_wp_auto;
}

static void mx50_arm2_set_num_cpu_wp(int num)
{
	num_cpu_wp = num;
	return;
}

static struct mxc_w1_config mxc_w1_data = {
	.search_rom_accelerator = 1,
};

static struct fec_platform_data fec_data = {
	.phy = PHY_INTERFACE_MODE_RMII,
};

/* workaround for cspi chipselect pin may not keep correct level when idle */
static void mx50_arm2_gpio_spi_chipselect_active(int cspi_mode, int status,
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
			{
			struct pad_desc cspi_ss0 = MX50_PAD_CSPI_SS0__CSPI_SS0;
			struct pad_desc cspi_cs1 = MX50_PAD_ECSPI1_MOSI__GPIO_4_13;

			/* pull up/down deassert it */
			mxc_iomux_v3_setup_pad(&cspi_ss0);
			mxc_iomux_v3_setup_pad(&cspi_cs1);

			gpio_request(CSPI_CS1, "cspi-cs1");
			gpio_direction_input(CSPI_CS1);
			}
			break;
		case 0x2:
			{
			struct pad_desc cspi_ss1 = MX50_PAD_ECSPI1_MOSI__CSPI_SS1;
			struct pad_desc cspi_ss0 = MX50_PAD_CSPI_SS0__GPIO_4_11;

			/*disable other ss */
			mxc_iomux_v3_setup_pad(&cspi_ss1);
			mxc_iomux_v3_setup_pad(&cspi_ss0);

			/* pull up/down deassert it */
			gpio_request(CSPI_CS2, "cspi-cs2");
			gpio_direction_input(CSPI_CS2);
			}
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

static void mx50_arm2_gpio_spi_chipselect_inactive(int cspi_mode, int status,
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
			gpio_free(CSPI_CS1);
			break;
		case 0x2:
			gpio_free(CSPI_CS2);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

}

static struct mxc_spi_master mxcspi1_data = {
	.maxchipselect = 4,
	.spi_version = 23,
	.chipselect_active = mx50_arm2_gpio_spi_chipselect_active,
	.chipselect_inactive = mx50_arm2_gpio_spi_chipselect_inactive,
};

static struct mxc_spi_master mxcspi3_data = {
	.maxchipselect = 4,
	.spi_version = 7,
	.chipselect_active = mx50_arm2_gpio_spi_chipselect_active,
	.chipselect_inactive = mx50_arm2_gpio_spi_chipselect_inactive,
};

/* 2011/05/26 FY11 : Modified i2c clock. (100k -> 400k) */
static struct imxi2c_platform_data mxci2c_data = {
       .bitrate = 400000,
};

#define mV_to_uV(mV) (mV * 1000)
#define uV_to_mV(uV) (uV / 1000)
#define V_to_uV(V) (mV_to_uV(V * 1000))
#define uV_to_V(uV) (uV_to_mV(uV) / 1000)

/* 2011/03/22 FY11 : Fixed the bug that dropped memory area is accessed. */
static struct regulator_init_data tps65180_init_data[] = {
	{
		.constraints = {
			.name = "DISPLAY",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
	}, {
		.constraints = {
			.name = "VCOM",
/* 2011/1/19 FY11 : Changed to use absolute value. */
			.min_uV = mV_to_uV(0),
			.max_uV = mV_to_uV(5110),
/* 2011/1/21 FY11 : Fixed the bug cannot change PWRCOM status. */
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
		},
	},{
		.constraints = {
			.name = "V3P3_CTRL",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
	},{
		.constraints = {
			.name = "PMIC_TEMP",
		},
	},{
		.constraints = {
			.name = "PWR0_CTRL",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
	},{
		.constraints = {
			.name = "PWR2_CTRL",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
	},
/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
	{
		.constraints = {
			.name = "VSYS_EPD",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
	},
};

static void epdc_get_pins(void)
{
	/* Claim GPIOs for EPDC pins - used during power up/down */
	gpio_request(EPDC_D0, "epdc_d0");
	gpio_request(EPDC_D1, "epdc_d1");
	gpio_request(EPDC_D2, "epdc_d2");
	gpio_request(EPDC_D3, "epdc_d3");
	gpio_request(EPDC_D4, "epdc_d4");
	gpio_request(EPDC_D5, "epdc_d5");
	gpio_request(EPDC_D6, "epdc_d6");
	gpio_request(EPDC_D7, "epdc_d7");
	gpio_request(EPDC_GDCLK, "epdc_gdclk");
	gpio_request(EPDC_GDSP, "epdc_gdsp");
	gpio_request(EPDC_GDOE, "epdc_gdoe");
	gpio_request(EPDC_SDCLK, "epdc_sdclk");
	gpio_request(EPDC_SDOE, "epdc_sdoe");
	gpio_request(EPDC_SDLE, "epdc_sdle");
	gpio_request(EPDC_SDCE0, "epdc_sdce0");
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	gpio_request(EPDC_SDCE2, "epdc_sdce2");
#endif /* E_BOOK */
}

static void epdc_put_pins(void)
{
	gpio_free(EPDC_D0);
	gpio_free(EPDC_D1);
	gpio_free(EPDC_D2);
	gpio_free(EPDC_D3);
	gpio_free(EPDC_D4);
	gpio_free(EPDC_D5);
	gpio_free(EPDC_D6);
	gpio_free(EPDC_D7);
	gpio_free(EPDC_GDCLK);
	gpio_free(EPDC_GDSP);
	gpio_free(EPDC_GDOE);
	gpio_free(EPDC_SDCLK);
	gpio_free(EPDC_SDOE);
	gpio_free(EPDC_SDLE);
	gpio_free(EPDC_SDCE0);
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	gpio_free(EPDC_SDCE2);
#endif /* E_BOOK */
}

static struct pad_desc  mx50_epdc_pads_enabled[] = {
	MX50_PAD_EPDC_D0__EPDC_D0,
	MX50_PAD_EPDC_D1__EPDC_D1,
	MX50_PAD_EPDC_D2__EPDC_D2,
	MX50_PAD_EPDC_D3__EPDC_D3,
	MX50_PAD_EPDC_D4__EPDC_D4,
	MX50_PAD_EPDC_D5__EPDC_D5,
	MX50_PAD_EPDC_D6__EPDC_D6,
	MX50_PAD_EPDC_D7__EPDC_D7,
	MX50_PAD_EPDC_GDCLK__EPDC_GDCLK,
	MX50_PAD_EPDC_GDSP__EPDC_GDSP,
	MX50_PAD_EPDC_GDOE__EPDC_GDOE,
	MX50_PAD_EPDC_SDCLK__EPDC_SDCLK,
	MX50_PAD_EPDC_SDOE__EPDC_SDOE,
	MX50_PAD_EPDC_SDLE__EPDC_SDLE,
	MX50_PAD_EPDC_SDCE0__EPDC_SDCE0,
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	MX50_PAD_EPDC_SDCE2__EPDC_SDCE2,
#endif /* E_BOOK */
};

static struct pad_desc  mx50_epdc_pads_disabled[] = {
	MX50_PAD_EPDC_D0__GPIO_3_0,
	MX50_PAD_EPDC_D1__GPIO_3_1,
	MX50_PAD_EPDC_D2__GPIO_3_2,
	MX50_PAD_EPDC_D3__GPIO_3_3,
	MX50_PAD_EPDC_D4__GPIO_3_4,
	MX50_PAD_EPDC_D5__GPIO_3_5,
	MX50_PAD_EPDC_D6__GPIO_3_6,
	MX50_PAD_EPDC_D7__GPIO_3_7,
	MX50_PAD_EPDC_GDCLK__GPIO_3_16,
	MX50_PAD_EPDC_GDSP__GPIO_3_17,
	MX50_PAD_EPDC_GDOE__GPIO_3_18,
	MX50_PAD_EPDC_SDCLK__GPIO_3_20,
	MX50_PAD_EPDC_SDOE__GPIO_3_23,
	MX50_PAD_EPDC_SDLE__GPIO_3_24,
	MX50_PAD_EPDC_SDCE0__GPIO_4_25,
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	MX50_PAD_EPDC_SDCE2__GPIO_4_27,
#endif /* E_BOOK */
};

static void epdc_enable_pins(void)
{
	/* Configure MUX settings to enable EPDC use */
	mxc_iomux_v3_setup_multiple_pads(mx50_epdc_pads_enabled, \
				ARRAY_SIZE(mx50_epdc_pads_enabled));

	gpio_direction_input(EPDC_D0);
	gpio_direction_input(EPDC_D1);
	gpio_direction_input(EPDC_D2);
	gpio_direction_input(EPDC_D3);
	gpio_direction_input(EPDC_D4);
	gpio_direction_input(EPDC_D5);
	gpio_direction_input(EPDC_D6);
	gpio_direction_input(EPDC_D7);
	gpio_direction_input(EPDC_GDCLK);
	gpio_direction_input(EPDC_GDSP);
	gpio_direction_input(EPDC_GDOE);
	gpio_direction_input(EPDC_SDCLK);
	gpio_direction_input(EPDC_SDOE);
	gpio_direction_input(EPDC_SDLE);
	gpio_direction_input(EPDC_SDCE0);
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	gpio_direction_input(EPDC_SDCE2);
#endif /* E_BOOK */
}

static void epdc_disable_pins(void)
{
	/* Configure MUX settings for EPDC pins to
	 * GPIO and drive to 0. */
	mxc_iomux_v3_setup_multiple_pads(mx50_epdc_pads_disabled, \
				ARRAY_SIZE(mx50_epdc_pads_disabled));

	gpio_direction_output(EPDC_D0, 0);
	gpio_direction_output(EPDC_D1, 0);
	gpio_direction_output(EPDC_D2, 0);
	gpio_direction_output(EPDC_D3, 0);
	gpio_direction_output(EPDC_D4, 0);
	gpio_direction_output(EPDC_D5, 0);
	gpio_direction_output(EPDC_D6, 0);
	gpio_direction_output(EPDC_D7, 0);
	gpio_direction_output(EPDC_GDCLK, 0);
	gpio_direction_output(EPDC_GDSP, 0);
	gpio_direction_output(EPDC_GDOE, 0);
	gpio_direction_output(EPDC_SDCLK, 0);
	gpio_direction_output(EPDC_SDOE, 0);
	gpio_direction_output(EPDC_SDLE, 0);
	gpio_direction_output(EPDC_SDCE0, 0);
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	gpio_direction_output(EPDC_SDCE2, 0);
#endif /* E_BOOK */
}

#if 0 /*E_BOOK*/
/*
 * 
 *  Testing code section begin
 * 
 * 
 *  
 */
#ifdef AFTER_PMIC_INTEGRATED

static struct regulator* ldo5;
static struct regulator* ldousb;


void sd4_wifi_init(void)
{
	//int result;

    printk("request WAKE_ON_WLAN gpio\n");
	gpio_request(WAKE_ON_WLAN, "wake-on-lan");	
    printk("request WAN_POWER_EN gpio\n");
	gpio_request(WAN_POWER_EN, "wan-power-en");
    printk("request WLAN_POWER_EN gpio\n");
    gpio_request(WLAN_POWER_EN, "wlan-power-en");
    printk("request CHIP_PWD_L gpio\n");
	gpio_request(CHIP_PWD_L, "chip-pwd-l");
	
#ifdef CONFIG_REGULATOR_TWL4030
	printk("get LDO5 regulator\n");
	ldo5 = regulator_get(NULL, "LDO5");
	if(IS_ERR(ldo5)){
		printk(KERN_ERR"error: regulator_get LDO5\n");
		ldo5 = NULL;
	}

	printk("get LDOUSB regulator\n");
	ldousb = regulator_get(NULL, "LDOUSB");
	if(IS_ERR(ldousb))
	{
		printk(KERN_ERR"error: regulator_get LDOUSB\n");
		ldousb = NULL;
	}
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	//TODO: get 2.5V regulator
	//TODO: get 3.3V regulator
#endif

}


void sd4_wifi_exit(void)
{
    printk("free WAKE_ON_WLAN gpio\n");
	gpio_free(WAKE_ON_WLAN);	
    printk("free WAN_POWER_EN gpio\n");
	gpio_free(WAN_POWER_EN);
    printk("free CHIP_PWD_L gpio\n");
	gpio_free(CHIP_PWD_L);
	
#ifdef CONFIG_REGULATOR_TWL4030
	printk("put LDO5 regulator\n");
	if(ldo5)
		regulator_put(ldo5);

	printk("put LDOUSB regulator\n");
		if(ldo5)
			regulator_put(ldousb);
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	//TODO: put 2.5V regulator
	//TODO: put 3.3V regulator
#endif
	
}

void enable_sd4_wifi(void)
{
	//Change WAKE_ON_WLAN (SD3_D6, GP5_26) to INPUT
    printk("set WAKE_ON_WLAN gpio input\n");
	gpio_direction_input(WAKE_ON_WLAN);

	//Change SDHC interface from GPIO to SD function
    printk("setup sd4 pads\n");
    mxc_iomux_v3_setup_multiple_pads(mxc_sd4_wifi_active, \
                    ARRAY_SIZE(mxc_sd4_wifi_active));

	//Output HIGH from WAN_POWER_EN (KEY_ROW1, GP4_3)
    printk("output WAN_POWER_EN high\n");
	gpio_direction_output(WAN_POWER_EN, 1);
    
    //Output HIGH from WLAN_POWER_EN
    printk("output WLAN_POWER_EN high\n");
    gpio_direction_output(WLAN_POWER_EN, 1);

	//wait 2 msecond
        printk("delay 2ms\n");
	mdelay(2);
	
#ifdef CONFIG_REGULATOR_TWL4030
	//Turn TI 6025 LDO5 on
        printk("enable LDO5 regulator\n");
	if(ldo5)
		regulator_enable(ldo5);

	{	
	        u8 value;
/*
	        twl_i2c_read_u8(TWL_MODULE_RTC, &value, 
	                TWL6025_LDO5_CFG_TRANS);
	        printk("Phoenix Lite I2C LDO5_TRANS Value read: %x\n", value);        
	        twl_i2c_write_u8(TWL_MODULE_RTC, 0x6,
	                TWL6025_LDO5_CFG_TRANS);
	        printk("Phoenix Lite I2C LDO5_TRANS Value written: %x\n", 0x6);
	        twl_i2c_read_u8(TWL_MODULE_RTC, &value, 
	                TWL6025_LDO5_CFG_TRANS);
	        printk("Phoenix Lite I2C LDO5_TRANS Value read: %x\n", value);
	        twl_i2c_read_u8(TWL_MODULE_RTC,&value, 
	                TWL6025_LDO5_CFG_STATE);
	        printk("Phoenix Lite I2C LDO5_STATE Value read: %x\n", value);
	        twl_i2c_write_u8(TWL_MODULE_RTC,0x1,
	                TWL6025_LDO5_CFG_STATE);
	        printk("Phoenix Lite I2C LDO5_STATE Value written: %x\n", 0x1);
	        twl_i2c_read_u8(TWL_MODULE_RTC,&value, 
	                TWL6025_LDO5_CFG_STATE);
	        printk("Phoenix Lite I2C LDO5_STATE Value read: %x\n", value);
*/
        	printk("output 32khz from Phoenix Lite\n");
	                
	        twl_i2c_write_u8(TWL_MODULE_RTC, 0x5,
	                TWL6025_CLK32KG_CFG_TRANS);
	        printk("Phoenix Lite I2C CLK32KG_TRANS Value written: %x\n", 0x5);
	        twl_i2c_read_u8(TWL_MODULE_RTC, &value, 
	                TWL6025_CLK32KG_CFG_TRANS);
	        printk("Phoenix Lite I2C CLK32KG_TRANS Value read: %x\n", value);
	        twl_i2c_write_u8(TWL_MODULE_RTC,0x1,
	                TWL6025_CLK32KG_CFG_STATE);
	        printk("Phoenix Lite I2C CLK32KG_STATE Value written: %x\n", 0x1);
	         twl_i2c_read_u8(TWL_MODULE_RTC, &value, 
	                TWL6025_CLK32KG_CFG_STATE);
	        printk("Phoenix Lite I2C CLK32KG_STATE Value read: %x\n", value);
	}
/*
    printk("enable LDOUSB regulator\n");
    if(ldousb)
    {	
        regulator_enable(ldousb);
        twl_i2c_write_u8(TWL_MODULE_RTC,0x1,
	                TWL6025_LDO5_CFG_STATE);
    }
*/    	
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	//TODO: Enable 2.5V regulator
	//TODO: Enable 32khz
	//TODO: Enable 3.3V regulator
#endif
	
	//wait 10 msecond
	printk("delay 10ms\n");
	mdelay(10);

	//Set CHIP_PWD_L (SD3_D7, GP5_27) to High
    printk("output CHIP_PWD_L gpio 1\n");
	gpio_direction_output(CHIP_PWD_L, 1);
}


void disable_sd4_wifi(void)
{
	int i;

	//Set CHIP_PWD_L to LOW
    printk("output CHIP_PWD_L gpio 0\n");
	gpio_direction_output(CHIP_PWD_L, 0);

	//Output LOW from WAN_POWER_EN
    printk("output WAN_POWER_EN low\n");
	gpio_direction_output(WAN_POWER_EN, 0); 

	//Output LOW from WLAN_POWER_EN
    printk("output WLAN_POWER_EN low\n");
	gpio_direction_output(WLAN_POWER_EN, 0); 
	
#ifdef CONFIG_REGULATOR_TWL4030
	//LDO5 disable
	printk("disable LDO5 regulator\n");
	if(ldo5)
		regulator_disable(ldo5);
        {
                u8 value;
/*
                twl_i2c_read_u8(TWL_MODULE_RTC,&value, 
                        TWL6025_LDO5_CFG_STATE);
                printk("Phoenix Lite I2C LDO5_STATE Value read: %x\n", value);
                twl_i2c_write_u8(TWL_MODULE_RTC,0x0,
                        TWL6025_LDO5_CFG_STATE);
                printk("Phoenix Lite I2C LDO5_STATE Value written: %x\n", 0x0);
                twl_i2c_read_u8(TWL_MODULE_RTC,&value, 
                        TWL6025_LDO5_CFG_STATE);
                printk("Phoenix Lite I2C LDO5_STATE Value read: %x\n", value);
*/

	        twl_i2c_write_u8(TWL_MODULE_RTC,0,
	                TWL6025_CLK32KG_CFG_STATE);
	        printk("Phoenix Lite I2C CLK32KG_STATE Value written: %x\n", 0);
	        twl_i2c_read_u8(TWL_MODULE_RTC, &value, 
	                TWL6025_CLK32KG_CFG_STATE);
	        printk("Phoenix Lite I2C CLK32KG STATE Value read: %x\n", value); 
        }
/*        
	printk("disable LDOUSB regulator\n");
	if(ldousb)
	{
		regulator_disable(ldousb);
		twl_i2c_write_u8(TWL_MODULE_RTC,0x1,
	                TWL6025_LDO5_CFG_STATE);
	}
*/
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	//TODO: Disable 2.5V regulator
	//TODO: Disable 32khz
	//TODO: Disable 3.3V regulator
#endif
	
	//Change SDHC interface from SD function to GPIO
    printk("setup sd4 pads\n");
    mxc_iomux_v3_setup_multiple_pads(mxc_sd4_wifi_inactive, \
                    ARRAY_SIZE(mxc_sd4_wifi_inactive));
    printk("output sd4 gpio 0\n");
	for(i=0; i<ARRAY_SIZE(mxc_sd4_gpio); i++)
    {
        gpio_request(mxc_sd4_gpio[i], mxc_sd4_gpio_name[i]);
		gpio_direction_output(mxc_sd4_gpio[i], 0);
    }
						   
   	//Set WAKE_ON_WLAN to LOW
    printk("output WAKE_ON_WLAN gpio 0\n");
	gpio_direction_output(WAKE_ON_WLAN, 0);
}

static struct completion wlan_user_event;
static int wlan_switch_status = 0;

static int wlan_user_thread(void *arg)
{
    printk("wlan user thread\n");
    
    while(true)
    {
        wait_for_completion_interruptible(&wlan_user_event);

        if (!gpio_get_value(WLAN_USER) && wlan_switch_status == 0)
        {
            sd4_wifi_init();
            enable_sd4_wifi();
            wlan_switch_status = 1;
        }
        else if (gpio_get_value(WLAN_USER) && wlan_switch_status == 1)
        {
            disable_sd4_wifi();
            sd4_wifi_exit();
            wlan_switch_status = 0;
        }
    }
    return 0;
}
#endif
#endif /*E_BOOK*/

#ifdef CONFIG_KEYBOARD_MXC /* E_BOOK */
/* Suppoert config change */
/* Not support FY11 */
static irqreturn_t uikey_0_irq_handle(int irq, void *dev_id)
{
    pr_info(KERN_INFO "uikey_0 pressed\n");
    return IRQ_HANDLED;
}
#endif /* E_BOOK */

/*
 * 
 *  Testing code section end
 * 
 * 
 *  
 */

/* Include EPD panel info. */
#include "mx50_epd.h"



static struct mxc_epdc_fb_platform_data epdc_data = {
	.epdc_mode = panel_modes,
	.num_modes = ARRAY_SIZE(panel_modes),
	.get_pins = epdc_get_pins,
	.put_pins = epdc_put_pins,
	.enable_pins = epdc_enable_pins,
	.disable_pins = epdc_disable_pins,
};

/* 2011/03/22 FY11 : Fixed the bug that dropped memory area is accessed. */
static struct tps65180_platform_data tps65180_pdata = {
	.vneg_pwrup = 1,
	.gvee_pwrup = 1,
	.vpos_pwrup = 2,
	.gvdd_pwrup = 1,
	.gvdd_pwrdn = 1,
	.vpos_pwrdn = 2,
	.gvee_pwrdn = 1,
	.vneg_pwrdn = 1,
	.gpio_pmic_pwrgood = EPDC_PWRSTAT,
	.gpio_pmic_vcom_ctrl = EPDC_VCOM,
	.gpio_pmic_wakeup = EPDC_PMIC_WAKE,
	.gpio_pmic_intr = EPDC_PMIC_INT,
#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	.gpio_pmic_pwr2_ctrl = EPDC_PWR2_CTRL,
#endif /* E_BOOK */
	.gpio_pmic_pwr0_ctrl = EPDC_PWR0_CTRL,
/* 2011/05/26 FY11 : Supported TPS65181. */
	.gpio_pmic_v3p3_ctrl = EPDC_PWR0_CTRL,
/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
	.gpio_pmic_vsys = EPD_PMIC_VSYS,
	.regulator_init = tps65180_init_data,
};

static int __initdata tps65180_pass_num = { 2 };
/* 2011/1/19 FY11 : Changed to use absolute value. */
static int __initdata tps65180_vcom = { 1250000 };
/*
 * Parse user specified options (`tps65180:')
 * example:
 * 	tps65180:pass=2,vcom=-1250000
 */
static int __init tps65180_setup(char *options)
{
	char *opt;
	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;
		if (!strncmp(opt, "pass=", 5))
			tps65180_pass_num =
				simple_strtoul(opt + 5, NULL, 0);
		if (!strncmp(opt, "vcom=", 5)) {
			int offs = 5;
			if (opt[5] == '-')
				offs = 6;
			tps65180_vcom =
				simple_strtoul(opt + offs, NULL, 0);
			tps65180_vcom = -tps65180_vcom;
		}
	}
	return 1;
}

__setup("tps65180:", tps65180_setup);

#ifdef CONFIG_REGULATOR_TWL4030
static struct regulator_init_data mxc6025_ldo2 = {
	.constraints = {
		.min_uV			= 2380000,
		.max_uV			= 2630000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_ldo4 = {
	.constraints = {
		.min_uV			= 1150000,
		.max_uV			= 1350000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_ldo3 = {
	.constraints = {
		.min_uV			= 1750000,
		.max_uV			= 1950000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_ldo5 = {
	.constraints = {
		.min_uV			= 2500000,
		.max_uV			= 2500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	/*.num_consumer_supplies  = 1,
	.consumer_supplies      = mxc6025_vmmc_supply,*/
};

static struct regulator_init_data mxc6025_ldo1 = {
	.constraints = {
		.min_uV			= 1100000,
		.max_uV			= 1300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_ldo7 = {
	.constraints = {
		.min_uV			= 2000000,
		.max_uV			= 3200000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_ldo6 = {
	.constraints = {
		.min_uV			= 2380000,
		.max_uV			= 2620000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_ldoln = {
	.constraints = {
		.min_uV			= 3000000,
		.max_uV			= 3600000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_ldousb = {
	.constraints = {
		.min_uV			= 3100000,
		.max_uV			= 3100000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 =	REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_smps3 = {
	.constraints = {
		.min_uV			= 1150000,
		.max_uV			= 1270000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 =	REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_smps4 = {
	.constraints = {
		.min_uV			= 2700000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 =	REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_vio = {
	.constraints = {
		.min_uV			= 1710000,
		.max_uV			= 1890000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 =	REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data mxc6025_smartreflex[] = {
	{
		.constraints = {
			.min_uV			= 800000,
			.max_uV			= 1050000,
			.apply_uV		= true,
			.valid_modes_mask	= REGULATOR_MODE_NORMAL
						| REGULATOR_MODE_STANDBY,
			.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
						| REGULATOR_CHANGE_MODE
						| REGULATOR_CHANGE_STATUS,
		},
	},
	{
		.constraints = {
			.min_uV			= 1000000,
			.max_uV			= 1225000,
			.apply_uV		= true,
			.valid_modes_mask	= REGULATOR_MODE_NORMAL
						| REGULATOR_MODE_STANDBY,
			.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
						| REGULATOR_CHANGE_MODE
						| REGULATOR_CHANGE_STATUS,
		},
	}
};

static struct i2c_board_info smartreflex_board_info = {
	I2C_BOARD_INFO("twl-smartreflex", 0x12),
	.platform_data = mxc6025_smartreflex,
};

static int twl6025_regulator_init(void)
{
	u8 value;
	struct i2c_adapter *i2c_adapt;

	/* Using TWL_MODULE_RTC as we want to access by absolute register
	 * numbers.
	 */

	/*
	WA:- Remove SMPS3 from the Sleep Sequence
	.Remove the Resource assignment for PREQ1 due to sequencing issue.

	.PREQ1_RES_ASS_A register is defining which resource are part of PREQ1 sleep sequence.

	.Keeping field to 0 will remove assignment from Sleep sequence.Still the VSEL level changes for SMPS1,2 and 3

	PREQ1_RES_ASS_A :
	
	[5] LDOUSB : 1--control wrt PREQ1, 0--PREQ1 has no effect on USB LDO
	[4] VIO    : 1--control wrt PREQ1, 0--PREQ1 has no effect on VIO
	[3] SMPS4  : 1--control wrt PREQ1, 0--PREQ1 has no effect on SMPS4
	[2] SMPS3  : 1--control wrt PREQ1, 0--PREQ1 has no effect on SMPS3
	[1] SMPS2  : 1--control wrt PREQ1, 0--PREQ1 has no effect on SMPS2
	[0] SMPS1  : 1--control wrt PREQ1, 0--PREQ1 has no effect on SMPS1
	
	*/
	//twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_PREQ1_RES_ASS_A);
	//value &= 0xFB;
	twl_i2c_write_u8(TWL_MODULE_RTC, 0x07, TWL6025_PREQ1_RES_ASS_A);

	/*
	.Enable the SLEEP sequence wrt PREQ1.

	.PHOENIX_MSK_TRANSITION registers is defining whether PREQ1/2/3 to use to go to SLEEP state

	.Keeping field to 0 will unmask the PREQ sleep events.

	.Example to set the Sleep wrt PREQ1.

	PHOENIX_MSK_TRANSITION :
	*/
	twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_PHOENIX_MSK_TRANSITION);
	value &= 0xDF;
	twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_PHOENIX_MSK_TRANSITION);

	/*
	.Add the Resource assignment for PREQ1.

	.PREQ1_RES_ASS_A/B/C  registers is defining which resource are part of PREQ1 sleep sequence.

	.Keeping field to 1 will assign the resource to PREQ1.

	.To assign the LDO 3,4 and 6 for Sleep wrt PREQ1.

	PREQ1_RES_ASS_B :

	[7] LDOLN : 1--control wrt PREQ1, 0--PREQ1 has no effect on LDOLN
	[6] LDO7  : 1--control wrt PREQ1, 0--PREQ1 has no effect on LDO7
	[5] LDO6  : 1--control wrt PREQ1, 0--PREQ1 has no effect on LDO6
	[4] LDO5  : 1--control wrt PREQ1, 0--PREQ1 has no effect on LDO5
	[3] LDO4  : 1--control wrt PREQ1, 0--PREQ1 has no effect on LDO4
	[2] LDO3  : 1--control wrt PREQ1, 0--PREQ1 has no effect on LDO3
	[1] LDO2  : 1--control wrt PREQ1, 0--PREQ1 has no effect on LDO2
	[0] LDO1  : 1--control wrt PREQ1, 0--PREQ1 has no effect on LDO1
	*/
	//twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_PREQ1_RES_ASS_B);
	//value &= 0xC3;
	twl_i2c_write_u8(TWL_MODULE_RTC, 0x2C, TWL6025_PREQ1_RES_ASS_B);
	
	/* TWL6025_LDO5_CFG_TRANS 
	   LDO5 is turned off when CPU state is standby. */
	//twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_LDO5_CFG_TRANS);
	//value &= ~0x0C;
	//twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_LDO5_CFG_TRANS);

	/*
	.To set the resource in OFF state when SLEEP state is entered

	.LDO3_CFG_TRANS register is defining the state of the resource LDO3.

	.To set the LDO 3 in OFF when in Sleep.Write "00" to bits 2 and 3 of above register

	LDO3_CFG_TRANS :

	[3] SLEEP_1   [2] SLEEP_0
	0		0		:Resource will be in OFF state when in Sleep state
	0		1		:AMS/SLEEP
	1	 	0		:Reserved
	1		1		:Active mode
	*/
	twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_LDO3_CFG_TRANS);
	value &= 0xF3;
	twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_LDO3_CFG_TRANS);

	/*
	.To set the resource in OFF state when SLEEP state is entered

	.LDO4_CFG_TRANS register is defining the state of the resource LDO4.
	
	.To set the LDO 3 in OFF when in Sleep.Write "00" to bits 2 and 3 of above register

	LDO4_CFG_TRANS :

	[3] SLEEP_1   [2] SLEEP_0
	0		0		:Resource will in OFF state when in Sleep state
	0		1		:AMS/SLEEP
	1	 	0		:Reserved
	1		1		:Active mode
*/
	twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_LDO4_CFG_TRANS);
	value &= 0xF3;
	twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_LDO4_CFG_TRANS);

	/*
	.To set the resource in OFF state when SLEEP state is entered
	
	.LDO6_CFG_TRANS register is defining the state of the resource LDO6.

	.To set the LDO 3 in OFF when in Sleep.Write "00" to bits 2 and 3 of above register

	LDO6_CFG_TRANS :

	[3] SLEEP_1   [2] SLEEP_0
	0		0		:Resource will in OFF state when in Sleep state
	0		1		:AMS/SLEEP
	1	 	0		:Reserved
	1		1		:Active mode
	*/
	twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_LDO6_CFG_TRANS);
	value &= 0xF3;
	twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_LDO6_CFG_TRANS);
	
	/*
	.Add the Resource assignment for PREQ1.
	
	.PREQ1_RES_ASS_A/B/C  registers is defining which resource are part of PREQ1 sleep sequence.
	
	.To set the REGEN 1&SYSEN in Sleep wrt PREQ1.
	
	PREQ1_RES_ASS_C :
	
	
	[5] VBATMIN_HI  : 1--control wrt PREQ1, 0--PREQ1 has no effect on VBATMIN_HI
	[4] CLK32KG     : 1--control wrt PREQ1, 0--PREQ1 has no effect on CLK32KG
	[3] CLK32KAUDIO : 1--control wrt PREQ1, 0--PREQ1 has no effect on CLK32KAUDIO
	[2] SYSEN       : 1--control wrt PREQ1, 0--PREQ1 has no effect on SYSEN
	[1] REGEN2      : 1--control wrt PREQ1, 0--PREQ1 has no effect on REGEN2
	[0] REGEN1      : 1--control wrt PREQ1, 0--PREQ1 has no effect on REGEN1
	*/

	twl_i2c_write_u8(TWL_MODULE_RTC, 0x05, TWL6025_PREQ1_RES_ASS_C);
	
	/*
	.To set the resource in OFF state when SLEEP state is entered
	
	.REGEN1_CFG_TRANS register is defining the state of the resource REGEN1.
	
	.To set the REGEN1 in OFF when in Sleep.Write "00" to bits 2 and 3 of above register
	
	REGEN1_CFG_TRANS :
	[3] SLEEP_1   [2] SLEEP_0
	0		0		: Resource will in OFF state when in Sleep state
	0		1		:AMS/SLEEP
	1	 	0		:Reserved
	1		1		:Active mode
	*/
	twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_REGEN1_CFG_TRANS);
	value &= 0xF3;
	twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_REGEN1_CFG_TRANS);

	/*
	.To set the resource in OFF state when SLEEP state is entered
	
	.SYSEN_CFG_TRANS register is defining the state of the resource SYSEN.
	
	.To set the LDO 3 in OFF when in Sleep.Write "00" to bits 2 and 3 of above register
	
	SYSEN_CFG_TRANS :
	[3] SLEEP_1   [2] SLEEP_0
	0		0		: Resource will in OFF state when in Sleep state
	0		1		:AMS/SLEEP
	1	 	0		:Reserved
	1		1		:Active mode
	*/
	twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_SYSEN_CFG_TRANS);
	value &= 0xF3;
	twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_SYSEN_CFG_TRANS);

	/*
	.Disable  internal PU/PD on PREQ1, PREQ2B/C and enable internal PD for PREQ2A
	
	.CFG_INPUT_PUPD1  register is defining PU/PD on PREQ signals
	
	CFG_INPUT_PUPD1 :
	
	[7] PREQ2C_PU   : 1--enable the Pull-Up   on PREQ2C, 0--disable the Pull-Up   on PREQ2C
	[6] PREQ2C_PD   : 1--enable the Pull-Down on PREQ2C, 0--disable the Pull-Down on PREQ2C
	[5] PREQ2B_PU   : 1--enable the Pull-Up   on PREQ2B, 0--disable the Pull-Up   on PREQ2B
	[4] PREQ2B_PD   : 1--enable the Pull-Down on PREQ2B, 0--disable the Pull-Down on PREQ2B
	[3] PREQ2A_PU   : 1--enable the Pull-Up   on PREQ2A, 0--disable the Pull-Up   on PREQ2A
	[2] PREQ2A_PD   : 1--enable the Pull-Down on PREQ2A, 0--disable the Pull-Down on PREQ2A
	[1] PREQ1_PU    : 1--enable the Pull-Up   on PREQ1,  0--disable the Pull-Up   on PREQ1
	[0] PREQ1_PD    : 1--enable the Pull-Down on PREQ1,  0--disable the Pull-Down on PREQ1
	*/
	twl_i2c_write_u8(TWL_MODULE_RTC, 0x04, TWL6025_CFG_INPUT_PUPD1);

	/*
	.Disable  internal PU/PD on PREQ3
	
	.CFG_INPUT_PUPD1  register is defining PU/PD on PREQ signals
	
	
	CFG_INPUT_PUPD2 :
	
	[5] PWMFORCE_PD : 1--enable the Pull-Up   on SPEAR6,  0--disable the Pull-Up   on SPEAR6
	[1] PREQ3_PU    : 1--enable the Pull-Up   on PREQ3,  0--disable the Pull-Up   on PREQ3
	[0] PREQ3_PD    : 1--enable the Pull-Down on PREQ3,  0--disable the Pull-Down on PREQ3
	*/
	//twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_CFG_INPUT_PUPD2);
	//value &= 0xFC;
	value = 0x45;
	twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_CFG_INPUT_PUPD2);


	/* Enable soft control of LDOUSB possibly not needed in final product */
	twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_MISC2);
	twl_i2c_write_u8(TWL_MODULE_RTC, value | 0x10, TWL6025_MISC2);

	if ( !gpio_get_value(BOARD_ID) )  {	//In the case of BB.
		/*
		   Bit[3] Command to apply to the resource in case of SLEEP state
		   0: OFF
		   1: ON
		*/
		//printk("Phenix Lite Setting for BB.\n");
		twl_i2c_read_u8(TWL_MODULE_RTC, &value, TWL6025_RC6MHZ_CFG_TRANS);
		value |= 0x04;
		twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_RC6MHZ_CFG_TRANS);
	}else{
		//printk("Phenix Lite Setting for Set.\n");
	}
	
	/*
	LDO5_CFG_BOLTAGE	Bit[5:0]	10000: 2.5V
	*/
	value = 0x10;
	twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_LDO5_CFG_VOLTAGE);
	
	/*
	LDO5_CFG_STATE	Bit[1:0]	00: OFF
								01: ON
								10: OFF
								11: SLEEP
	*/
	//value = 0x01;	//LDO5 ON
	//twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_LDO5_CFG_STATE);
	
	/*
	LDOUSB_CFG_STATE	Bit[1:0]	00: OFF
									01: ON
									10: OFF
									11: SLEEP
	*/
	//value = 0x01;	//LDOUSB ON
	//twl_i2c_write_u8(TWL_MODULE_RTC, value, TWL6025_LDOUSB_CFG_STATE);
	
	i2c_adapt = i2c_get_adapter(0);
	i2c_new_device(i2c_adapt, &smartreflex_board_info);

	return 0;
}

/* FREESCALE TODO: table to be updated with battery temperature data */
static int battery_temp_table[] = {
	/* twl6025 ADC value for temperature in degree C */
	929, 925, /* -2 ,-1 */
	920, 917, 912, 908, 904, 899, 895, 890, 885, 880, /* 00 - 09 */
	875, 869, 864, 858, 853, 847, 841, 835, 829, 823, /* 10 - 19 */
	816, 810, 804, 797, 790, 783, 776, 769, 762, 755, /* 20 - 29 */
	748, 740, 732, 725, 718, 710, 703, 695, 687, 679, /* 30 - 39 */
	671, 663, 655, 647, 639, 631, 623, 615, 607, 599, /* 40 - 49 */
	591, 583, 575, 567, 559, 551, 543, 535, 527, 519, /* 50 - 59 */
	511, 504, 496 /* 60 - 62 */
};

/* FREESCALE TODO: table to be updated with battery voltage data */
/* NOTE: This can be used as alternative to Gas Gauge */
static int battery_voltage_table[] = {
	/* voltage for capacity in steps of 10%
	 * (or steps of 100% / table size) */
	3300,	/* 0% */
	3400,	/* 10% */
	3500,	/* 20% */
	3600,	/* 30% */
	3700,	/* 40% */
	3800,	/* 50% */
	3850,	/* 60% */
	3900,	/* 70% */
	3950,	/* 80% */
	4000,	/* 90% */
	4050,	/* 100% */
};

static struct twl4030_madc_platform_data twl6025_gpadc_data = {
	.irq_line	= 1,
};

unsigned int twl6025_vprime_callback(int voltage_uV, int current_uA)
{
	// TODO: to have IR drop calculation filled in by Sony
	return voltage_uV;
}

unsigned int twl6025_capacity_callback(int voltage_uV)
{
	// TODO: to have battery capacity calculation filled in by Sony
	int cap;
	int size = ARRAY_SIZE(battery_voltage_table);
	
	for (cap = 0; cap < size; cap++) {
		if ((voltage_uV/1000) <= battery_voltage_table[cap]){
			return (cap * 10);
		}
	}
	
	return 100;
}

static struct twl4030_bci_platform_data twl6025_bci_data = {
	.monitoring_interval		= 10,
	.max_charger_currentmA		= 1500,
	.max_charger_voltagemV		= 4560,
	.max_bat_voltagemV		= 4200,
	.low_bat_voltagemV		= 3300,
	.termination_currentmA		= 100,
	.battery_tmp_tbl		= battery_temp_table,
	.battery_tmp_tblsize		= ARRAY_SIZE(battery_temp_table),
	.battery_volt_tbl		= battery_voltage_table,
	.battery_volt_tblsize		= ARRAY_SIZE(battery_voltage_table),
	.use_hw_charger			= 1,
	.use_eeprom_config		= 1,
	.power_path			= 1,
	.initial_v_no_samples		= 25,
	.initial_fuelgauge_mode		= 0,
#if 0 /* FY11 E_BOOK */
	.vbat_channel			= 8,
#endif  /* FY11 E_BOOK */
	.vprime_callback		= twl6025_vprime_callback,
	.capacity_callback		= twl6025_capacity_callback,
	.disable_current_avg		= 1,
	.disable_backup_charge		= 1,
	.fuelgauge_recal		= 200,
};

static struct delayed_work bat_voltage_ws;
static struct timer_list bat_voltage_timer;

static void twl6025battery_voltage_work(struct work_struct *ws)
{
	twl6025_battery_capacity();
}

static void twl6025battery_voltage_timer(unsigned long data)
{
	schedule_delayed_work(&bat_voltage_ws, 0);
	mod_timer(&bat_voltage_timer, jiffies + msecs_to_jiffies(60000));
}
#endif

static int mx50_arm2_control_phy_power(struct device *dev, int on)
{
#ifdef CONFIG_REGULATOR_TWL4030
	//printk(KERN_ERR "%s %d\n", __func__, on);
	//twl6025_battery_capacity();
#endif
	return 0;
}

static int mx50_arm2_control_vbus_power(struct device *dev, int on)
{
#ifdef CONFIG_REGULATOR_TWL4030
	//printk(KERN_ERR "%s %d\n", __func__, on);
	if(confirm_the_di()){
		phxlit_vbus_callback(on);
		cancel_delayed_work(&bat_voltage_ws);
		schedule_delayed_work(&bat_voltage_ws, 0);
		mod_timer(&bat_voltage_timer, jiffies + msecs_to_jiffies(60000));
		twl6030_set_power_source(POWER_SUPPLY_TYPE_MAINS);
	}
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	//printk(KERN_ERR "%s %d\n", __func__, on);
	if(confirm_the_di()){
		phxlit_vbus_callback(on);
	}
#endif
	
	return 0;
}

static struct input_event phxlit_vbus_event = {
	.type = EV_SW,
	.code = SW_USB_INSERT,
};

static struct platform_device phxlit_vbus_device = {
	.name = "phxlit_vbus",
	.id = 0,
	.num_resources = 0,
};

#ifdef CONFIG_REGULATOR_TWL4030
static struct twl4030_usb_data twl6025_mxc_usb = {
	.board_control_phy_power = mx50_arm2_control_phy_power,
	.board_control_vbus_power = mx50_arm2_control_vbus_power,
};

static struct twl4030_vbus_data twl6025_mxc_vbus = {
	.board_control_vbus_power = mx50_arm2_control_vbus_power,
};

static struct twl4030_platform_data mxc6025_twldata = {

	/* Regulator Init callback */
	.init		= twl6025_regulator_init,
	
/* 2010/01/17 FY11 :Add irw setting. */
	/* irq */
	.irq_base       = PMIC_IRQ_START,
	.irq_end        = PMIC_IRQ_START + PMIC_IRQS,
	
	/* Regulators */
	.ldo1		= &mxc6025_ldo1,
	.ldo2		= &mxc6025_ldo2,
	.ldo3		= &mxc6025_ldo3,
	.ldo4		= &mxc6025_ldo4,
	.ldo5		= &mxc6025_ldo5,
	.ldo6		= &mxc6025_ldo6,
	.ldo7		= &mxc6025_ldo7,
	.vana		= &mxc6025_vana,
	.ldoln		= &mxc6025_ldoln,
	.ldousb		= &mxc6025_ldousb,
	.smps3		= &mxc6025_smps3,
	.smps4		= &mxc6025_smps4,
	.vio6025	= &mxc6025_vio,

	/* ADC and battery */
	.madc           = &twl6025_gpadc_data,
	.bci            = &twl6025_bci_data,
	
	/* USB */
	.usb		= &twl6025_mxc_usb,
	.vbus		= &twl6025_mxc_vbus,
};
#endif

#ifdef CONFIG_REGULATOR_WM831X
static struct regulator_consumer_supply vcore_consumers[] = {
	REGULATOR_SUPPLY("SMPS1", NULL),
};
 
static struct regulator_init_data vcore_init = {
	.constraints = {
		.name = "VCORE",
		.always_on = 1,

		.min_uV = 850000,
		.max_uV = 1100000,

		.uV_offset = 25000,
		.uV_offset_apply = 0,

		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
	},
	.consumer_supplies = vcore_consumers,
	.num_consumer_supplies = ARRAY_SIZE(vcore_consumers),
};

static struct regulator_consumer_supply vperi_consumers[] = {
	REGULATOR_SUPPLY("SMPS2", NULL),
};

static struct regulator_init_data vperi_init = {
	.constraints = {
		.name = "VPERI",
		.always_on = 1,
		
		.min_uV = 950000,
		.max_uV = 1250000,

		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
	},
	.consumer_supplies = vperi_consumers,
	.num_consumer_supplies = ARRAY_SIZE(vperi_consumers),
};

static struct regulator_init_data dcdc3_init = {
	.constraints = {
		.always_on = 1,
	},
};

static struct regulator_init_data dcdc4_init = {
	.constraints = {
		.always_on = 1,
	},
};

#define LDO_OPS_MASK \
	(REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE)

static struct regulator_init_data ldo_init[] = {
	[0] = {
		.constraints = {
			.name = "LDO1",
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[1] = {
		.constraints = {
			.name = "LDO2",
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[2] = {
		.constraints = {
			.name = "LDO3",
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[3] = {
		.constraints = {
			.name = "LDO4",
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[4] = {
		.constraints = {
			.name = "LDO5",
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[5] = {
		.constraints = {
			.name = "LDO6",
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[6] = {
		.constraints = {
			.name = "LDO7",
			.min_uV = 1000000,
			.max_uV = 3500000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[7] = {
		.constraints = {
			.name = "LDO8",
			.min_uV = 1000000,
			.max_uV = 3500000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[8] = {
		.constraints = {
			.name = "LDO9",
			.min_uV = 1000000,
			.max_uV = 3500000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[9] = {
		.constraints = {
			.name = "LDO10",
			.min_uV = 1000000,
			.max_uV = 3500000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},

	[10] = {
		.constraints = {
			.name = "LDO11",
			.min_uV = 800000,
			.max_uV = 1550000,
			.valid_ops_mask = LDO_OPS_MASK,
		},
	},
};

static struct wm831x *wm8321;

int WM8321_post_init(struct wm831x *wm831x)
{
	wm8321_init_complete_cmd();
	return 0;
}

int WM8321_pre_init(struct wm831x *wm831x)
{
	wm8321 = wm831x;
	
	if( wm831x_reg_write(wm831x,0x4008,0x9716))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);		

	if( wm831x_reg_write(wm831x,0x4003,0x9800))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4005,0x0219))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4006,0x8001))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4017,0x0000))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if(wm831x_reg_write(wm831x,0x4039,0xA400))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4041,0x808C))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4058,0x602A))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4059,0x0218))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x405D,0x413C))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x405E,0x0324))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4060,0x101E))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4062,0x8126))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4063,0x0326))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4064,0x001E))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4066,0x6152))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4067,0x0352))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x406A,0x0101))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x406C,0xA106))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x406D,0x0106))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4070,0x0117))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4071,0x0201))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4072,0x001E))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4073,0x001E))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4075,0x611B))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4076,0x011B))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4078,0x8117))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4079,0x0117))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x407B,0xA10E))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x407C,0x010E))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x407E,0x0115))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x407F,0x0115))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4081,0x011B))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4082,0x011B))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x4084,0x001C))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4085,0x001C))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x4088,0x0008))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x408E,0x000F))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);

	if( wm831x_reg_write(wm831x,0x408F,0x0077))
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x4057,0x8300)) //DCDC1 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x405C,0x8300)) //DCDC2 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x4061,0x8380)) //DCDC3 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x4065,0x8180)) //DCDC4 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x4068,0x8201)) //LDO1 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x406B,0x8901)) //LDO2 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x406E,0x8201)) //LDO3 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x4074,0x8901)) //LDO5 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x4077,0x8901)) //LDO6 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x407A,0x8900)) //LDO7 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x407D,0x8200)) //LDO8 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	if( wm831x_reg_write(wm831x,0x4080,0x8200)) //LDO9 Control
		printk("ERROR!!!!!!!!! %s:%u\n", __func__, __LINE__);
	
	return 0;
}
 
static struct wm831x_pdata wm831x_pdata = {
	
	.rtc_alarm_off_on_shutdown = true,
	
	.irq_base = PMIC_IRQ_START,
	
	.gpio_defaults = {
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0xa400, 0xa400, 0x8481, 0x808c, 0xa400, 0xa400
	},
	
	.pre_init = WM8321_pre_init,
	.post_init = WM8321_post_init,

	.dcdc = {
		&vcore_init,    /* DCDC1 */
		&vperi_init,    /* DCDC2 */
		&dcdc3_init,
		&dcdc4_init,
	},
	
	.ldo = {
		&ldo_init[0],
		&ldo_init[1],
		&ldo_init[2],
		&ldo_init[3],
		&ldo_init[4],
		&ldo_init[5],
		&ldo_init[6],
		&ldo_init[7],
		&ldo_init[8],
		&ldo_init[9],
		&ldo_init[10],
	},
};

#endif

static struct i2c_board_info mxc_i2c0_board_info[] __initdata = {
#ifdef CONFIG_REGULATOR_WM831X
	{
	 I2C_BOARD_INFO("wm8321", 0x34),
	 .platform_data = &wm831x_pdata,
	 .irq = IOMUX_TO_IRQ_V3((3*32 + 1)),  /* GPIO_4_1 */
	},
#endif
};

/* 2011/05/26 FY11 : Supported TPS65181. */
bool use_tps65185 = false;
#define TPS65181_I2C_ADDR	0x48
#define TPS65185_I2C_ADDR	0x68

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{ /*  This must be first entry 
	      Because to modify i2c address in mxc_board_init()  */
/* 2011/05/26 FY11 : Supported TPS65181. */
	 I2C_BOARD_INFO("tps65180", TPS65181_I2C_ADDR),
	 .platform_data = &tps65180_pdata,
	},
#ifdef CONFIG_REGULATOR_TWL4030
	{
	I2C_BOARD_INFO("twl6025", 0x48),
	.flags = I2C_CLIENT_WAKE,
	.irq = IOMUX_TO_IRQ_V3(PWR_INT),
	.platform_data = &mxc6025_twldata,
	},
#endif	
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("cs42l52-i2c", 0x4a)
	},
};

static struct mtd_partition mxc_dataflash_partitions[] = {
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 0x000100000,},
	{
	 .name = "kernel",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL,},
};

static struct flash_platform_data mxc_spi_flash_data[] = {
	{
	 .name = "mxc_dataflash",
	 .parts = mxc_dataflash_partitions,
	 .nr_parts = ARRAY_SIZE(mxc_dataflash_partitions),
	 .type = "at45db321d",}
};

#ifdef CONFIG_REGULATOR_WM831X
static int sub_main_platform_init(void)
{
	return 0;
}

struct sub_cpu_bci_platform_data sub_cpu_bci_data = {
	.board_control_vbus_power = mx50_arm2_control_vbus_power,
};

static struct sub_main_platform_data sub_main_pdata = {
		
	 /* Init callback */
	.init		= sub_main_platform_init,
	
	/* ADC and battery */
	.bci            = &sub_cpu_bci_data,
	
	/* irq */
	//.irq_base       = PMIC_IRQ_START,
	//.irq_end        = PMIC_IRQ_START + PMIC_IRQS,
	
};
#endif

static struct spi_board_info mxc_dataflash_device[] __initdata = {
	{
	 .modalias = "mxc_dataflash",
	 .max_speed_hz = 25000000,	/* max spi clock (SCK) speed in HZ */
	 .bus_num = 3,
	 .chip_select = 1,
	 .platform_data = &mxc_spi_flash_data[0],
	},
#ifdef CONFIG_REGULATOR_WM831X
	{
	 .modalias = "sub_main",
	 .max_speed_hz = 2000000,	/* max spi clock (SCK) speed in HZ */
	 .bus_num = 3,
	 .chip_select = 2,
	 .platform_data = &sub_main_pdata,
	 .irq = IOMUX_TO_IRQ_V3(SPI_SUB_INT),
	},
#endif
};

#if 1 /* E_BOOK *//* for POWER3 2011/05/17 */
int power3_detect_flag;
static void check_power3_det()
{
  int det;
  struct pad_desc pwr3_det_pwr2 = MX50_PAD_UART4_RXD__GPIO_6_17_PWR2;

#ifdef CONFIG_REGULATOR_WM831X
  power3_detect_flag=1;
  gpio_request(POWER3_DET,"power3");
  gpio_direction_input(POWER3_DET);
  gpio_free(POWER3_DET);
  printk("PWR3DET:power 3 detect\n");
#endif
#ifdef CONFIG_REGULATOR_TWL4030 
  power3_detect_flag=0;
  printk("PWR3DET:not power 3\n");
  /* close port */
  mxc_iomux_v3_setup_pad(&pwr3_det_pwr2);
#endif
#if 0
  gpio_request(POWER3_DET,"power3");
  gpio_direction_input(POWER3_DET);
  gpio_free(POWER3_DET);
  udelay(10);
  det = gpio_get_value(POWER3_DET);
  if ( det ) {
    power3_detect_flag=1;
    printk("PWR3DET:power 3 detect\n");
  } else {
    power3_detect_flag=0;
    printk("PWR3DET:not power 3\n");
    /* close port */
    mxc_iomux_v3_setup_pad(&pwr3_det_pwr2);
  }
#endif
}
EXPORT_SYMBOL(power3_detect_flag);
#include <linux/proc_fs.h>
#ifdef CONFIG_PROC_FS
static ssize_t power3_det_read(struct file *file, char __user *buffer,
                                   size_t count, loff_t * offset)
{
  if ( *offset != 0 ) {
    return 0;
  }
  if (power3_detect_flag) {
    buffer[0] = '1';
  } else {
    buffer[0] = '0';
  }
  buffer[1] = 0;
  *offset += 2;
  return 2;
}

static const struct file_operations power3_det_info_ops = {
  .read = power3_det_read,
};
static int __init power3_det_info_init(void)
{
	struct proc_dir_entry *res;
	res = create_proc_entry("pwr3", 0, NULL);
	if (!res) {
		printk(KERN_ERR "Failed to create dma info file \n");
		return -ENOMEM;
	}
	res->proc_fops = &power3_det_info_ops;
	return 0;
}

late_initcall(power3_det_info_init);
#endif
#endif
#if 1 /* E_BOOK *//* for LED 2011/2/21 */
static struct led_pwm pwm_orange_led={
  .name = "mxc_pwm",
  .pwm_id = 1,
  .max_brightness = 255,
  .pwm_period_ns = 8000000,
};
static struct led_pwm_platform_data led_pwm_platform_data= {
  .num_leds = 1,
  .leds = &pwm_orange_led,
};
static struct platform_device pwm_led = {
  .name = "leds_pwm",
  .dev = {
    .platform_data = &led_pwm_platform_data,
  },
};
#endif

static void mx50_arm2_usb_set_vbus(bool enable)
{
	gpio_set_value(USB_OTG_PWR, enable);
}


static int sdhc_write_protect(struct device *dev)
{
	unsigned short rc = 0;

	if (to_platform_device(dev)->id == 0)
		rc = gpio_get_value(SD1_WP);
	else if (to_platform_device(dev)->id == 1)
		rc = 0; //Memory Stick does not have WP
	else if (to_platform_device(dev)->id == 2)
		rc = 0;
	else if (to_platform_device(dev)->id == 3)
		rc = 0;

	return rc;
}

static unsigned int sdhc_get_card_det_status(struct device *dev)
{
	int ret = 0;
	if (to_platform_device(dev)->id == 0)
		ret = gpio_get_value(SD1_CD);
	else if (to_platform_device(dev)->id == 1)
		ret = gpio_get_value(SD2_CD);
	else if (to_platform_device(dev)->id == 2)
		ret = 0; // eMMC is solder on Sony bread board
	else if (to_platform_device(dev)->id == 3)
    {
#if 1 /*E_BOOK*/
		ret = !wlan_card_status;
#else
#ifdef AFTER_PMIC_INTEGRATED
        complete(&wlan_user_event);
		ret = gpio_get_value(WLAN_USER);
#endif        
#endif	/*E_BOOK*/
    }

	return ret;
}

static struct mxc_mmc_platform_data mmc1_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
	.power_mmc = NULL,
};

static struct mxc_mmc_platform_data mmc2_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
};

static struct mxc_mmc_platform_data mmc3_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 400000,
#if 1 /* E_BOOK *//* up to 50MHz for eMMC */
	.max_clk = 50000000,	// for Sony bread board
#else
	.max_clk = 12500000,	// for Sony bread board
#endif
#if 1 /* E_BOOK *//* 2011/1/20 */
	.clk_always_on = 0,
#else
	.clk_always_on = 1,
#endif
	.dll_override_en = 1,
	.dll_delay_cells = 0xc,
	.card_inserted_state = 1,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
#if 1 /* E_BOOK *//* 2011/1/20 */
	.clk_always_on = 0,
#else
	.clk_always_on = 1,
#endif
};

static struct mxc_mmc_platform_data mmc4_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 25000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
	.power_mmc = NULL,
};

#ifdef CONFIG_KEYBOARD_GPIO /* E_BOOK */
/* GPIO-keys input device */
/* Support FY11 */
static struct gpio_keys_button ebk_gpio_keys[] = {
	{
		.type		= EV_KEY,
		.code		= KEY_ENTER,		/* 28 */
		.gpio		= (2 * 32 + 12),	/* MX50_PAD_EPDC_D12__GPIO_3_12 */
		.desc		= "Key0",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_UP,			/* 103 */
		.gpio		= (2 * 32 + 13),	/* MX50_PAD_EPDC_D13__GPIO_3_13 */
		.desc		= "Key1",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_RIGHTMETA,	/* 126 */
		.gpio		= (2 * 32 + 14),	/* MX50_PAD_EPDC_D14__GPIO_3_14 */
		.desc		= "Key2",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_DOWN,			/* 108 */
		.gpio		= (2 * 32 + 15),	/* MX50_PAD_EPDC_D15__GPIO_3_15 */
		.desc		= "Key3",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_LEFTMETA,		/* 125 */
		.gpio		= (3 * 32 + 27),	/* MX50_PAD_EPDC_SDCE2__GPIO_4_27 */
		.desc		= "Key4",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_HOME,			/* 102 */
		.gpio		= (3 * 32 + 28),	/* MX50_PAD_EPDC_SDCE3__GPIO_4_28 */
		.desc		= "Key5",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_VOLUMEDOWN,	/* 114 */
		.gpio		= (3 * 32 + 29),	/* MX50_PAD_EPDC_SDCE4__GPIO_4_29 */
		.desc		= "Key6",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_BACK,			/* 158 */
		.gpio		= (3 * 32 + 30),	/* MX50_PAD_EPDC_SDCE5__GPIO_4_30 */
		.desc		= "Key7",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_LEFT,			/* 105 */
		.gpio		= (3 * 32 + 20),	/* MX50_PAD_EPDC_PWRCTRL3__GPIO_4_20 */
		.desc		= "Key8(SLSW A)",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_VOLUMEUP,		/* 115 */
		.gpio		= (3 * 32 + 22),	/* MX50_PAD_EPDC_VCOM1__GPIO_4_22 */
		.desc		= "Key9(SLSW Push)",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_RIGHT,		/* 106 */
		.gpio		= (3 * 32 + 21),	/* MX50_PAD_EPDC_VCOM0__GPIO_4_21 */
		.desc		= "Key10(SLSW B)",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_ZOOM,			/* 372 */
		.gpio		= (2 * 32 + 29),	/* MX50_PAD_EPDC_PWRCTRL0__GPIO_3_29 */
		.desc		= "RENC A",
		.wakeup		= 0,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_OPTION,		/* 357 */
		.gpio		= (2 * 32 + 31),	/* MX50_PAD_EPDC_PWRCTRL2__GPIO_3_31 */
		.desc		= "Key11(RENC Push)",
		.wakeup		= 1,
		.active_low	= 1,
	},
	{
		.type		= EV_KEY,
		.code		= KEY_MUTE,			/* 113 */
		.gpio		= (2 * 32 + 30),	/* MX50_PAD_EPDC_PWRCTRL1__GPIO_3_30 */
		.desc		= "RENC B",
		.wakeup		= 0,
		.active_low	= 1,
	},
};

static struct gpio_keys_platform_data ebk_button_data = {
	.buttons	= ebk_gpio_keys,
	.nbuttons	= ARRAY_SIZE(ebk_gpio_keys),
};
#endif /* E_BOOK */

static int mxc_cs42l52_amp_enable(int enable)
{
	return 0;
}

static int headphone_det_status(void)
{
	return (gpio_get_value(HP_DETECT) != 0);
}

static struct mxc_audio_platform_data cs42l52_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.hp_irq = IOMUX_TO_IRQ_V3(HP_DETECT),
	.hp_status = headphone_det_status,
	.amp_enable = mxc_cs42l52_amp_enable,
	.sysclk = 12000000,
};

static struct platform_device mxc_cs42l52_device = {
	.name = "imx-3stack-cs42l52",
};

static struct pad_desc armadillo2_wvga_pads[] = {
	MX50_PAD_DISP_D0__DISP_D0,
	MX50_PAD_DISP_D1__DISP_D1,
	MX50_PAD_DISP_D2__DISP_D2,
	MX50_PAD_DISP_D3__DISP_D3,
	MX50_PAD_DISP_D4__DISP_D4,
	MX50_PAD_DISP_D5__DISP_D5,
	MX50_PAD_DISP_D6__DISP_D6,
	MX50_PAD_DISP_D7__DISP_D7,
};

static void wvga_reset(void)
{
	mxc_iomux_v3_setup_multiple_pads(armadillo2_wvga_pads, \
				ARRAY_SIZE(armadillo2_wvga_pads));
	return;
}

static struct mxc_lcd_platform_data lcd_wvga_data = {
	.reset = wvga_reset,
};

static struct platform_device lcd_wvga_device = {
	.name = "lcd_claa",
	.dev = {
		.platform_data = &lcd_wvga_data,
		},
};

static struct fb_videomode video_modes[] = {
	{
	 /* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	 "CLAA-WVGA", 57, 800, 480, 37037, 40, 60, 10, 10, 20, 10,
	 FB_SYNC_CLK_LAT_FALL,
	 FB_VMODE_NONINTERLACED,
	 0,},
};

static struct mxc_fb_platform_data fb_data[] = {
	{
	 .interface_pix_fmt = V4L2_PIX_FMT_RGB565,
	 .mode_str = "CLAA-WVGA",
	 .mode = video_modes,
	 .num_modes = ARRAY_SIZE(video_modes),
	 },
};

static int __initdata enable_w1 = { 0 };
static int __init w1_setup(char *__unused)
{
	enable_w1 = 1;
	return cpu_is_mx50();
}

__setup("w1", w1_setup);

static struct mxs_dma_plat_data dma_apbh_data = {
	.chan_base = MXS_DMA_CHANNEL_AHB_APBH,
	.chan_num = MXS_MAX_DMA_CHANNELS,
};



/* Android USB gadget platform_data */

#define	ANDROID_USB_VENDOR_ID		(0x054c)
#define	ANDROID_USB_PRODUCT_ID		(0x05c2)

static char* usb_functions[] = {
	"usb_mass_storage",
	"adb",
};

static struct android_usb_product usb_products[] = {
	{
		.product_id		= ANDROID_USB_PRODUCT_ID,

		.num_functions	= ARRAY_SIZE(usb_functions),
		.functions		= usb_functions,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id			= ANDROID_USB_VENDOR_ID,
	.product_id			= ANDROID_USB_PRODUCT_ID,
	.version			= 0x1000,

	.num_products	= ARRAY_SIZE(usb_products),
	.products		= usb_products,

	.num_functions	= ARRAY_SIZE(usb_functions),
	.functions		= usb_functions,
};

static	struct	usb_mass_storage_platform_data	android_mass_storage_pdata = {
	.vendor		= "Sony",
	.release	= 0x2000,
	.nluns		= 3,
};



/* OTP data */
/* Building up eight registers's names of a bank */
#define BANK(a, b, c, d, e, f, g, h)	\
	{\
	("HW_OCOTP_"#a), ("HW_OCOTP_"#b), ("HW_OCOTP_"#c), ("HW_OCOTP_"#d), \
	("HW_OCOTP_"#e), ("HW_OCOTP_"#f), ("HW_OCOTP_"#g), ("HW_OCOTP_"#h) \
	}

#define BANKS		(5)
#define BANK_ITEMS	(8)
static const char *bank_reg_desc[BANKS][BANK_ITEMS] = {
	BANK(LOCK, CFG0, CFG1, CFG2, CFG3, CFG4, CFG5, CFG6),
	BANK(MEM0, MEM1, MEM2, MEM3, MEM4, MEM5, GP0, GP1),
	BANK(SCC0, SCC1, SCC2, SCC3, SCC4, SCC5, SCC6, SCC7),
	BANK(SRK0, SRK1, SRK2, SRK3, SRK4, SRK5, SRK6, SRK7),
	BANK(SJC0, SJC1, MAC0, MAC1, HWCAP0, HWCAP1, HWCAP2, SWCAP),
};

static struct fsl_otp_data otp_data = {
	.fuse_name	= (char **)bank_reg_desc,
	.fuse_num	= BANKS * BANK_ITEMS,
};
#undef BANK
#undef BANKS
#undef BANK_ITEMS

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data android_pmem_gpu_data = {
	.name = "pmem_gpu",
	.size = SZ_32M,
};
#endif

/*!
 * Board specific fixup function. It is called by \b setup_arch() in
 * setup.c file very early on during kernel starts. It allows the user to
 * statically fill in the proper values for the passed-in parameters. None of
 * the parameters is used currently.
 *
 * @param  desc         pointer to \b struct \b machine_desc
 * @param  tags         pointer to \b struct \b tag
 * @param  cmdline      pointer to the command line
 * @param  mi           pointer to \b struct \b meminfo
 */
static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_ANDROID_PMEM
	char *str;
	struct tag *t;
	struct tag *mem_tag = 0;
	int total_mem = SZ_512M;
	int left_mem = 0;
#endif

	mxc_set_cpu_type(MXC_CPU_MX50);

	get_cpu_wp = mx50_arm2_get_cpu_wp;
	set_num_cpu_wp = mx50_arm2_set_num_cpu_wp;

#ifdef CONFIG_ANDROID_PMEM
	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size - android_pmem_gpu_data.size;
			left_mem = total_mem;
			break;
		}
	}

	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			str = t->u.cmdline.cmdline;
			str = strstr(str, "mem=");
			if (str != NULL) {
				str += 4;
				left_mem = memparse(str, &str);
				if (left_mem == 0 || left_mem > total_mem)
				left_mem = total_mem;
			}
			break;
		}
	}

	if (mem_tag) {
		android_pmem_gpu_data.start = mem_tag->u.mem.start + left_mem;
		mem_tag->u.mem.size = left_mem;
	}
#endif
}

static void __init mx50_arm2_io_init(void)
{
	int i;
#if 1 /* E_BOOK *//* Add for change IR Power to LDO10 2011/07/13 */
	struct pad_desc setid1 = MX50_PAD_EPDC_SDOED__GPIO_3_22_LDO10;
#endif
	mxc_iomux_v3_setup_multiple_pads(mx50_armadillo2, \
			ARRAY_SIZE(mx50_armadillo2));

	gpio_request(REBOOT_REQ,"power3");
	gpio_direction_output(REBOOT_REQ,0);
	gpio_request(SPI_WAKE_INT,"power3");
	gpio_direction_output(SPI_WAKE_INT,0);
	gpio_request(SPI_SUB_INT,"power3");
	gpio_direction_input(SPI_SUB_INT);
	gpio_request(SPI_RCV_RDY,"power3");
	gpio_direction_input(SPI_RCV_RDY);
	gpio_request(YOBI_MS1,"power3");
	gpio_direction_input(YOBI_MS1);
#ifdef CONFIG_REGULATOR_WM831X
	/* for POWER3 2011/05/06 */
	gpio_request(YOBI_MW0,"power3");
	gpio_direction_input(YOBI_MW0);
	gpio_request(PMIC_IRQ,"power3");
	gpio_direction_input(PMIC_IRQ);
	gpio_request(YOBI_MS0,"power3");
	gpio_direction_input(YOBI_MS0);
	gpio_request(YOBI_MW1,"power3");
	gpio_direction_input(YOBI_MW1);
	gpio_request(CSPI_CS1, "cspi-cs1");
	gpio_direction_output(CSPI_CS1, 1);
	gpio_request(NVCC_SD2_EN, "power3");
	gpio_direction_output(NVCC_SD2_EN,1);
	gpio_request(USB_H1_VDDA25_EN, "power3");
	gpio_direction_output(USB_H1_VDDA25_EN,0);
	gpio_request(AU_POWER_EN,"power3");
	gpio_direction_output(AU_POWER_EN,0);
#endif

	gpio_request(EPDC_SDCE1,"key");
	gpio_direction_output(EPDC_SDCE1,0);

	gpio_request(SD1_WP, "sdhc1-wp");
	gpio_direction_input(SD1_WP);

	gpio_request(SD1_CD, "sdhc1-cd");
	gpio_direction_input(SD1_CD);

#if 0 /* E_BOOK *//* delete 2011/06/14 */
	gpio_request(SD2_WP, "sdhc2-wp");
	gpio_direction_input(SD2_WP);
#endif

	gpio_request(SD2_CD, "sdhc2-cd");
	gpio_direction_input(SD2_CD);
#if 0	/*E_BOOK*/
	gpio_request(SD3_WP, "sdhc3-wp");
	gpio_direction_input(SD3_WP);
#endif
	gpio_request(SD3_CD, "sdhc3-cd");
	gpio_direction_input(SD3_CD);

	gpio_request(HP_DETECT, "hp-det");
	gpio_direction_input(HP_DETECT);

	gpio_request(PWR_INT, "pwr-int");
	gpio_direction_input(PWR_INT);

	gpio_request(SD2_WP,"iomux"); /* EMMC_VCC_EN */
	gpio_direction_output(SD2_WP, 1);
	
	gpio_request(WWAN_EN, "wwan_en");
	gpio_direction_input(WWAN_EN);
#if 1 /* E_BOOK *//* Add for change IR Power to LDO10 2011/07/13 */
	mdelay(5);
	if ( gpio_get_value(WWAN_EN) == 0) {
	  printk("IR Power is LDO10\n");
	  mxc_iomux_v3_setup_pad(&setid1);
	} else {
	  printk("IR Power is LDO4\n");
	}
#endif

	/* SD2 disable setting */
	gpio_request(GPIO5_6,"iomux");
	gpio_direction_output(GPIO5_6, 0);
	gpio_free(GPIO5_6);
	gpio_request(GPIO5_7,"iomux");
	gpio_direction_output(GPIO5_7, 0);
	gpio_free(GPIO5_7);
	gpio_request(GPIO5_8,"iomux");
	gpio_direction_output(GPIO5_8, 0);
	gpio_free(GPIO5_8);
	gpio_request(GPIO5_9,"iomux");
	gpio_direction_output(GPIO5_9, 0);
	gpio_free(GPIO5_9);
	gpio_request(GPIO5_10,"iomux");
	gpio_direction_output(GPIO5_10, 0);
	gpio_free(GPIO5_10);
	gpio_request(GPIO5_11,"iomux");
	gpio_direction_output(GPIO5_11, 0);
	gpio_free(GPIO5_11);
	

	/* --- start gpio ports for EPD --- */
/* 2011/07/05 FY11 : Added VSYS_EPD_ON. */
	gpio_request(EPD_PMIC_VSYS, "epd-pmic-vsys");
	gpio_direction_output(EPD_PMIC_VSYS, 1);


/* 2011/05/26 FY11 : Supported TPS65181. */
	/* identify EPD PMIC type. */
	{
		struct pad_desc tmp_desc = MX50_PAD_EPDC_SDOEZ__GPIO_3_21;
/* 2011/05/27 FY11 : Fixed the false recogition of EPD PMIC type. */
		tmp_desc.pad_ctrl = PAD_CTL_PKE | PAD_CTL_PUE |PAD_CTL_PUS_22K_UP | PAD_CTL_DSE_LOW;
		mxc_iomux_v3_setup_pad( &tmp_desc );
		udelay(10);

		gpio_request(EPDC_SDOEZ, "tps65185");
		gpio_direction_input(EPDC_SDOEZ);
		if ( gpio_get_value(EPDC_SDOEZ) )
		{
			use_tps65185 = true;
		}
		tmp_desc.pad_ctrl = PAD_CTL_PKE | PAD_CTL_PUE |PAD_CTL_PUS_100K_DOWN | PAD_CTL_DSE_LOW;
		mxc_iomux_v3_setup_pad( &tmp_desc );
	}

	gpio_request(EPDC_PMIC_WAKE, "epdc-pmic-wake");
/* 2011/1/24 FY11 : Modified default value.(default disable) */
	gpio_direction_output(EPDC_PMIC_WAKE, 0);

#if 0 /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	gpio_request(EPDC_PWR2_CTRL, "epdc-pwr2_ctrl");
	gpio_direction_output(EPDC_PWR2_CTRL, 1);
#endif /* E_BOOK */

	gpio_request(EPDC_PWR0_CTRL, "epdc-pwr0-ctrl");
	/*
	 * PWRUP pin of TPS6518x connects to this PWR0 gpio pin
	 */
/* 2011/05/26 FY11 : Supported TPS65181. */
	if ( use_tps65185 )
	{
		printk( KERN_ERR " This is TPS65185.\n" );
		gpio_direction_output(EPDC_PWR0_CTRL, 0);
	}
	else
	{
		printk( KERN_ERR " This is TPS65181.\n" );
		gpio_direction_output(EPDC_PWR0_CTRL, 1);
	}

	gpio_request(EPDC_PMIC_INT, "epdc-pmic-int");
	gpio_direction_input(EPDC_PMIC_INT);

	gpio_request(EPDC_VCOM, "epdc-vcom");
	gpio_direction_output(EPDC_VCOM, 0);

	gpio_request(EPDC_PWRSTAT, "epdc-pwrstat");
	gpio_direction_input(EPDC_PWRSTAT);


	/* --- end gpio ports for EPD --- */


#if 0 /* E_BOOK *//* delete 2011/07/04 */
	gpio_request(DISP_D14, "usb-power-ctrl.0");
	gpio_direction_output(DISP_D14, 1);
#endif

#if 1 /* E_BOOK *//* change setting 2011/07/04 */
	gpio_request(EPDC_ELCDIF_BACKLIGHT, "iomux");
	gpio_direction_input(EPDC_ELCDIF_BACKLIGHT);
#else 
	/* ELCDIF backlight */
	gpio_request(EPDC_ELCDIF_BACKLIGHT, "elcdif-backlight");
	gpio_direction_output(EPDC_ELCDIF_BACKLIGHT, 1);
#endif

	if (enable_w1) {
		struct pad_desc one_wire = MX50_PAD_OWIRE__OWIRE;
		mxc_iomux_v3_setup_pad(&one_wire);
	}
    
#if 0	/* E_BOOK */
    gpio_request(WLAN_USER, "wlan-user");
    gpio_direction_input(WLAN_USER);
#endif	/* E_BOOK */

#ifdef CONFIG_KEYBOARD_MXC /* E_BOOK */
/* Suppoert config change */
/* Not support FY11 */
    gpio_request(UIKEY_0, "uikey-0");
    gpio_direction_input(UIKEY_0);
    
    request_irq(IOMUX_TO_IRQ_V3(UIKEY_0), uikey_0_irq_handle,
        IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "key 0", NULL );
#endif /* E_BOOK */
    
#if 0	/* E_BOOK */
    //set WAN_POWER_EN to high. Otherwise, USB host driver cannot initialize successful.
    gpio_request(WAN_POWER_EN, "wan-power-en");
    gpio_direction_output(WAN_POWER_EN, 1);
    gpio_free(WAN_POWER_EN);
#ifdef AFTER_PMIC_INTEGRATED
    init_completion(&wlan_user_event);
    
	kthread_run(wlan_user_thread, NULL, "wlan thread");
#endif
#endif	/* E_BOOK */
	/* USB OTG PWR */
	gpio_request(USB_OTG_PWR, "usb otg power");
	gpio_direction_output(USB_OTG_PWR, 1);
	gpio_set_value(USB_OTG_PWR, 0);

	/* WWAN */
	gpio_wwan_active();

	/* initialize WIFI port (wifi off setting)*/
	for(i=0; i<ARRAY_SIZE(mxc_sd4_gpio); i++) {
	  gpio_request(mxc_sd4_gpio[i], "iomux");
	  gpio_direction_output(mxc_sd4_gpio[i], 0);
	  gpio_free(mxc_sd4_gpio[i]);
	}
	
	/* Audio */
	gpio_audio_activate();

	/* log int(reset button) */
	gpio_logint_activate();

	/* EPD : disable port */
	epdc_get_pins();
	epdc_disable_pins();
	epdc_put_pins();

	/* misc */
	gpio_request(LCD_VSYNC, "iomux");
	gpio_direction_output(LCD_VSYNC,0);
	gpio_free(LCD_VSYNC);
	gpio_request(SET_ID_YOBI, "iomux");
	gpio_direction_input(SET_ID_YOBI);
	gpio_free(SET_ID_YOBI);
	
	/* POWER3 detect */
	check_power3_det();
}

#ifdef AFTER_PMIC_INTEGRATED
void mxc_power_off(void)
{
#ifdef CONFIG_REGULATOR_TWL4030 
	twl_i2c_write_u8(TWL_MODULE_RTC, 0x1, TWL6025_PHOENIX_DEV_ON);
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	send_shutdown_request();
#endif
      while(1);  
}
#endif


/*
 * Board specific initialization.
 */
static void __init mxc_board_init(void)
{
	/* SD card detect irqs */
	mxcsdhc1_device.resource[2].start = IOMUX_TO_IRQ_V3(SD1_CD);
	mxcsdhc1_device.resource[2].end = IOMUX_TO_IRQ_V3(SD1_CD);
#if 1 /* E_BOOK *//* delete detect irq SDHC2 2011/06/28 */
	mxcsdhc2_device.resource[2].start = 0;
	mxcsdhc2_device.resource[2].end = 0;
#else
	mxcsdhc2_device.resource[2].start = IOMUX_TO_IRQ_V3(SD2_CD);
	mxcsdhc2_device.resource[2].end = IOMUX_TO_IRQ_V3(SD2_CD);
#endif
	mxcsdhc3_device.resource[2].start = IOMUX_TO_IRQ_V3(SD3_CD);
	mxcsdhc3_device.resource[2].end = IOMUX_TO_IRQ_V3(SD3_CD);
#if 0	/* E_BOOK */
#ifdef AFTER_PMIC_INTEGRATED
    mxcsdhc4_device.resource[2].start = IOMUX_TO_IRQ_V3(WLAN_USER);
    mxcsdhc4_device.resource[2].end = IOMUX_TO_IRQ_V3(WLAN_USER);
#endif
#endif	/* E_BOOK */

	mxc_cpu_common_init();
	mx50_arm2_io_init();

	mxc_register_device(&mxc_dma_device, NULL);
	mxc_register_device(&mxs_dma_apbh_device, &dma_apbh_data);
	mxc_register_device(&mxc_wdt_device, NULL);
	mxc_register_device(&mxcspi1_device, &mxcspi1_data);
	mxc_register_device(&mxcspi3_device, &mxcspi3_data);
	mxc_register_device(&mxci2c_devices[0], &mxci2c_data);
	mxc_register_device(&mxci2c_devices[1], &mxci2c_data);
	mxc_register_device(&mxci2c_devices[2], &mxci2c_data);

	mxc_register_device(&mxc_rtc_device, NULL);
	mxc_register_device(&mxc_w1_master_device, &mxc_w1_data);
	mxc_register_device(&gpu_device, &z160_revision);
	mxc_register_device(&mxc_pxp_device, NULL);
	mxc_register_device(&mxc_pxp_client_device, NULL);
	mxc_register_device(&mxc_pxp_v4l2, NULL);
	mxc_register_device(&mxc_dvfs_core_device, &dvfs_core_data);
	mxc_register_device(&busfreq_device, NULL);

	/*
	mxc_register_device(&mx53_lpmode_device, NULL);
	mxc_register_device(&mxc_dvfs_per_device, &dvfs_per_data);
	*/

/*	mxc_register_device(&mxc_keypad_device, &keypad_plat_data); */

	mxc_register_device(&mxcsdhc1_device, &mmc1_data);
	mxc_register_device(&mxcsdhc2_device, &mmc2_data);
	mxc_register_device(&mxcsdhc3_device, &mmc3_data);
	mxc_register_device(&mxcsdhc4_device, &mmc4_data);
	mxc_register_device(&mxc_ssi1_device, NULL);
	mxc_register_device(&mxc_ssi2_device, NULL);
#if 1 /* E_BOOK *//* Disable FEC 2011/1/28 */
	if ( gpio_get_value(BOARD_ID) )  {
	  printk("FEC disable\n");
	  mxc_iomux_v3_setup_multiple_pads(mxc_fec_inactive,ARRAY_SIZE(mxc_fec_inactive));
	  gpio_request(GPIO2_0,"fec");
	  gpio_direction_input(GPIO2_0);
	  gpio_free(GPIO2_0);
	  gpio_request(GPIO2_1,"fec");
	  gpio_direction_input(GPIO2_1);
	  gpio_free(GPIO2_1);
	  gpio_request(GPIO2_2,"fec");
	  gpio_direction_input(GPIO2_2);
	  gpio_free(GPIO2_2);
	  gpio_request(GPIO2_3,"fec");
	  gpio_direction_input(GPIO2_3);
	  gpio_free(GPIO2_3);
	  gpio_request(GPIO2_4,"fec");
	  gpio_direction_input(GPIO2_4);
	  gpio_free(GPIO2_4);
	  gpio_request(GPIO2_5,"fec");
	  gpio_direction_output(GPIO2_5,0);
	  gpio_free(GPIO2_5);
	  gpio_request(GPIO2_6,"fec");
	  gpio_direction_output(GPIO2_6,0);
	  gpio_free(GPIO2_6);
	  gpio_request(GPIO2_7,"fec");
	  gpio_direction_output(GPIO2_7,0);
	  gpio_free(GPIO2_7);
	  gpio_request(GPIO6_4,"fec");
	  gpio_direction_input(GPIO6_4);
	  gpio_free(GPIO6_4);
	  gpio_request(GPIO6_5,"fec");
	  gpio_direction_input(GPIO6_5);
	  gpio_free(GPIO6_5);
	  gpio_request(FEC_RESET,"fec");
	  gpio_direction_output(FEC_RESET,0);
	  gpio_free(FEC_RESET);
	} else {
		printk("FEC enable\n");
		mxc_register_device(&mxc_fec_device, &fec_data);
	}
#else
	mxc_register_device(&mxc_fec_device, &fec_data);
#endif
	spi_register_board_info(mxc_dataflash_device,
				ARRAY_SIZE(mxc_dataflash_device));
    i2c_register_board_info(0, mxc_i2c0_board_info,
                ARRAY_SIZE(mxc_i2c0_board_info));

/* 2011/05/26 FY11 : Supported TPS65181. */
	if (use_tps65185)
	{
		mxc_i2c1_board_info[0].addr = TPS65185_I2C_ADDR;
	}

	i2c_register_board_info(1, mxc_i2c1_board_info,
				ARRAY_SIZE(mxc_i2c1_board_info));
	i2c_register_board_info(2, mxc_i2c2_board_info,
				ARRAY_SIZE(mxc_i2c2_board_info));
	tps65180_pdata.pass_num = tps65180_pass_num;

// must set vcom read from eMMC.
	tps65180_pdata.vcom_uV = tps65180_vcom;

	mxc_register_device(&epdc_device, &epdc_data);
	mxc_register_device(&lcd_wvga_device, &lcd_wvga_data);
	mxc_register_device(&elcdif_device, &fb_data[0]);
	mxc_register_device(&mxs_viim, NULL);

#if 1 /* E_BOOK *//* for LED 2011/2/21 */
	mxc_register_device(&mxc_pwm2_device, NULL);
	platform_device_register(&pwm_led );
#endif
	mxc_register_device(&android_usb_device, &android_usb_pdata);
	mxc_register_device(&usb_mass_storage_device, &android_mass_storage_pdata);

#ifdef AFTER_PMIC_INTEGRATED
	pm_power_off = mxc_power_off;
#endif

	mxc_register_device(&mxc_cs42l52_device, &cs42l52_data);

	mx5_set_otghost_vbus_func(mx50_arm2_usb_set_vbus);
	mx5_usb_dr_init();
	mx5_usbh1_init();
	charger_detection_disable();

	mxc_register_device(&mxc_rngb_device, NULL);
	mxc_register_device(&dcp_device, NULL);
	mxc_register_device(&fsl_otp_device, &otp_data);

#ifdef CONFIG_ANDROID_PMEM
	mxc_register_device(&mxc_android_pmem_gpu_device, &android_pmem_gpu_data);
#endif
	
#ifdef CONFIG_REGULATOR_TWL4030
	/* Initialise the 5s timer for testing battery calculation */
	INIT_DELAYED_WORK_DEFERRABLE(&bat_voltage_ws, twl6025battery_voltage_work);
	init_timer(&bat_voltage_timer);
	bat_voltage_timer.expires = jiffies + msecs_to_jiffies(5000);
	bat_voltage_timer.function = twl6025battery_voltage_timer;
	bat_voltage_timer.data = NULL;
	add_timer(&bat_voltage_timer);
#endif

#ifdef CONFIG_KEYBOARD_GPIO /* E_BOOK */
/* Use it in GPIO for keys of FY11. */
	mxc_register_device(&ebk_button_device, &ebk_button_data);
#endif /* E_BOOK */
	
	mxc_register_device(&phxlit_vbus_device, &phxlit_vbus_event);

#if 1	/* E_BOOK */
mxc_register_device( &ar6000_pm_device, NULL );		//for WiFi
mxc_register_device( &wwan_ctrl_device, NULL );		//for WWAN
#endif	/* E_BOOK */
#ifdef CONFIG_TOUCHSCREEN_SONYIR /* E_BOOK *//* 2011/1/12 */
mxc_register_device(&sonyirtp_device[0],NULL);
#ifndef CONFIG_TOUCHSCREEN_SONYIR_NOFILM
mxc_register_device(&sonyirtp_device[1],NULL);
#endif /* CONFIG_TOUCHSCREEN_SONYIR_NOFILM */
#endif
}

static void __init mx50_arm2_timer_init(void)
{
	struct clk *uart_clk;

	mx50_clocks_init(32768, 24000000, 22579200);

	uart_clk = clk_get_sys("mxcintuart.4", NULL);
	early_console_setup(MX53_BASE_ADDR(UART5_BASE_ADDR), uart_clk);
}

static struct sys_timer mxc_timer = {
	.init	= mx50_arm2_timer_init,
};


#if 1 /*E_BOOK forWiFi*/
static void sdhc4_power_init( void )
{
	int val;
	int ret;
	
	gpio_request( WAKE_ON_WLAN,"wake-on-lan" );
	gpio_request( WLAN_POWER_EN,"wlan-power-en" );
	gpio_request( CHIP_PWD_L,"chip-pwd-l" );
	
	//1
	gpio_direction_input(WAKE_ON_WLAN);
	//2
	gpio_direction_output(WLAN_POWER_EN, 1);
	udelay(800);									//800us
	//3
	if( gpio_get_value(BOARD_ID) )		//SET
	{
		mxc_iomux_v3_setup_multiple_pads(mxc_sd4_wifi_active,ARRAY_SIZE(mxc_sd4_wifi_active));
	}
	else		//BB
	{
		mxc_iomux_v3_setup_multiple_pads(mxc_sd4_wifi_active_bb,ARRAY_SIZE(mxc_sd4_wifi_active_bb));
	}
	//4
#ifdef CONFIG_REGULATOR_TWL4030 
	twl_i2c_write_u8(TWL_MODULE_RTC,0x5,TWL6025_CLK32KG_CFG_TRANS);
	twl_i2c_write_u8(TWL_MODULE_RTC,0x1,TWL6025_CLK32KG_CFG_STATE);
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	BUG_ON(!wm8321);
	/* enable 32.768kHz clock on GPIO8 */
	val = wm831x_reg_read(wm8321, WM831X_GPIO8_CONTROL);
	if (val < 0) {
		pr_err("Can't read GPIO8 control register\n");
		goto pmic_fail;
	}
	/* configure GPIO8 as output */
	val &= ~(1 << 15);				//bit15:0=Out
	/* set 32.768kHz oscillator function on GPIO8 */
	val = (val & ~0x000f) | 0x81;	//1=32k Tri-state
	ret = wm831x_reg_write(wm8321, WM831X_GPIO8_CONTROL, val);
	if (val < 0)
		pr_err("Can't write to GPIO8 control register\n");

pmic_fail:
#endif
	//5
	mdelay(5);
	//6
	gpio_direction_output(CHIP_PWD_L, 1);
}
static void sdhc4_power_exit( void )
{
	int i;
	int val;
	int ret;
	
	//1
	gpio_direction_output(CHIP_PWD_L, 0);
	//2
#ifdef CONFIG_REGULATOR_TWL4030
	twl_i2c_write_u8(TWL_MODULE_RTC,0,TWL6025_CLK32KG_CFG_STATE);
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
	BUG_ON(!wm8321);
	/* disable 32.768kHz clock on GPIO8 */
	val = wm831x_reg_read(wm8321, WM831X_GPIO8_CONTROL);
	if (val < 0) {
		pr_err("Can't read GPIO8 control register\n");
		goto pmic_fail;
	}
	/* configure GPIO8 as input */
	val |= (1 << 15);		//bit15:1=In
	/* set GPIO function on GPIO8 */
	val &= ~0x008f;			//0=GPIO Normal
	ret = wm831x_reg_write(wm8321, WM831X_GPIO8_CONTROL, val);
	if (val < 0)
		pr_err("Can't write to GPIO8 control register\n");
pmic_fail:
#endif
	//3
	mxc_iomux_v3_setup_multiple_pads(mxc_sd4_wifi_inactive,ARRAY_SIZE(mxc_sd4_wifi_inactive));
	for(i=0; i<ARRAY_SIZE(mxc_sd4_gpio); i++)
    {
        gpio_request(mxc_sd4_gpio[i], mxc_sd4_gpio_name[i]);
		gpio_direction_output(mxc_sd4_gpio[i], 0);
    }
	//4
	gpio_direction_output(WLAN_POWER_EN, 0);
	//5
	gpio_direction_output(WAKE_ON_WLAN, 0);
	gpio_free(WAKE_ON_WLAN);
	gpio_free(CHIP_PWD_L);
}

/* WiFi Power control. call from ar6000 driver */
void plat_wifi_power_ctrl( int on )
{
	if( on )
	{
//		printk("WiFi Power ON\n");
		sdhc4_power_init();			//PowerON
		wlan_card_status = 1;		//CardState change
		mxc_mmc_force_detect(3);	//force_detect sdhc3ch
	}
	else
	{
//		printk("WiFi Power OFF\n");
		wlan_card_status = 0;		//CardState change
		mxc_mmc_force_detect(3);	//force_detect sdhc3ch
		sdhc4_power_exit();			//PowerOFF
	}
}
EXPORT_SYMBOL(plat_wifi_power_ctrl);
#endif	/*E_BOOK*/

/*
 * The following uses standard kernel macros define in arch.h in order to
 * initialize __mach_desc_MX50_ARM2 data structure.
 */
MACHINE_START(MX50_ARM2, "Freescale MX50 ARM2 Board")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mxc_board_init,
	.timer = &mxc_timer,
MACHINE_END

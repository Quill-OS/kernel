/*
 * Copyright (C) 2007, Guennadi Liakhovetski <lg@denx.de>
 *
 * (C) Copyright 2009-2011 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/mx50.h>
#include <asm/arch/mx50_pins.h>
#include <asm/arch/iomux.h>
#include <asm/errno.h>
#include <asm/sizes.h>

#ifdef CONFIG_IMX_CSPI
#include <imx_spi.h>
#include <asm/arch/imx_spi_pmic.h>
#endif

#if CONFIG_I2C_MXC
#include <i2c.h>
#endif

#ifdef CONFIG_CMD_MMC
#include <mmc.h>
#include <fsl_esdhc.h>
#endif

#ifdef CONFIG_ARCH_MMU
#include <asm/mmu.h>
#include <asm/arch/mmu.h>
#endif

#ifdef CONFIG_CMD_CLOCK
#include <asm/clock.h>
#endif

#ifdef CONFIG_MXC_EPDC
#include <lcd.h>
#endif

#ifdef CONFIG_ANDROID_RECOVERY
#include "../common/recovery.h"
#include <part.h>
#include <ext2fs.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <ubi_uboot.h>
#include <jffs2/load_kernel.h>
#endif

#include "ntx_comm.h"
#include "ntx_comm.c"

#include "ntx_hwconfig.h"
extern NTX_HWCONFIG *gptNtxHwCfg ;

#ifdef CONFIG_I2C_MULTI_BUS//[


I2C_PLATFORM_DATA gtI2C_platform_dataA[] = {
	{I2C1_BASE_ADDR,50000,0xfe,0},
	{I2C2_BASE_ADDR,200000,0xfe,0},
	{I2C3_BASE_ADDR,100000,0xfe,0},
};

unsigned int gdwBusNum=2; //default BUS is I2C3 .
#endif //CONFIG_I2C_MULTI_BUS

DECLARE_GLOBAL_DATA_PTR;

static u32 system_rev;
static enum boot_device boot_dev;



static GPIO_OUT gt_ntx_gpio_spd_en= {
	MX50_PIN_ECSPI1_MOSI,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	0, // pad config .
	4, // gpio group .
	13, // gpio number .
	1, // output value .
	0, // not inited .
	"SPD_EN",
};

static GPIO_OUT gt_ntx_gpio_tp_pwen= {
	MX50_PIN_CSPI_MOSI,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	0, // pad config .
	4, // gpio group .
	9, // gpio number .
	1, // output value .
	0, // not inited .
	"TP_PWEN",
};

static GPIO_OUT gt_ntx_gpio_tp_rst= {
	MX50_PIN_SD2_D4,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	0, // pad config .
	5, // gpio group .
	12, // gpio number .
	0, // output value .
	0, // not inited .
	"TP_RST",
};

static GPIO_KEY_BTN gt_ntx_gpio_home_key= {
	MX50_PIN_KEY_ROW0,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	//PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE|PAD_CTL_DRV_HIGH, // pad config .
	0, // pad config .
	4, // gpio group .
	1, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HOME]",
};

static GPIO_KEY_BTN gt_ntx_gpio_fl_key= {
	MX50_PIN_EPDC_BDR0,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE|PAD_CTL_DRV_HIGH, // pad config .
	4, // gpio group .
	23, // gpio number .
	0, // key down value .
	0, // not inited .
	"[FL]",
};

static GPIO_KEY_BTN gt_ntx_gpio_hallsensor_key= {
	MX50_PIN_SD3_D5,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	0, // pad config .
	5, // gpio group .
	25, // gpio number .
	0, // not inited .
	0, // key down value .
	"[HALL]",
};

static GPIO_KEY_BTN gt_ntx_gpio_5_13_hallsensor_key= {
	MX50_PIN_SD2_D5,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	0, // pad config .
	5, // gpio group .
	13, // gpio number .
	0, // not inited .
	0, // key down value .
	"[HALL]",
};

static GPIO_KEY_BTN gt_ntx_gpio_5_15_hallsensor_key= {
	MX50_PIN_SD2_D7,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	0, // pad config .
	5, // gpio group .
	15, // gpio number .
	0, // not inited .
	0, // key down value .
	"[HALL]",
};

GPIO_KEY_BTN * ntx_gpio_keysA[] = {
	&gt_ntx_gpio_home_key,
	&gt_ntx_gpio_fl_key,
//	&gt_ntx_gpio_hallsensor_key,
};

int gi_ntx_gpio_keys=sizeof(ntx_gpio_keysA)/sizeof(ntx_gpio_keysA[0]);



static inline void setup_boot_device(void)
{
	uint soc_sbmr = readl(SRC_BASE_ADDR + 0x4);
	uint bt_mem_ctl = (soc_sbmr & 0x000000FF) >> 4 ;
	uint bt_mem_type = (soc_sbmr & 0x00000008) >> 3;

	switch (bt_mem_ctl) {
	case 0x0:
		if (bt_mem_type)
			boot_dev = ONE_NAND_BOOT;
		else
			boot_dev = WEIM_NOR_BOOT;
		break;
	case 0x2:
		if (bt_mem_type)
			boot_dev = SATA_BOOT;
		else
			boot_dev = PATA_BOOT;
		break;
	case 0x3:
		if (bt_mem_type)
			boot_dev = SPI_NOR_BOOT;
		else
			boot_dev = I2C_BOOT;
		break;
	case 0x4:
	case 0x5:
		boot_dev = SD_BOOT;
		break;
	case 0x6:
	case 0x7:
		boot_dev = MMC_BOOT;
		break;
	case 0x8 ... 0xf:
		boot_dev = NAND_BOOT;
		break;
	default:
		boot_dev = UNKNOWN_BOOT;
		break;
	}
}

enum boot_device get_boot_device(void)
{
	return boot_dev;
}

u32 get_board_rev(void)
{
	return system_rev;
}

static inline void setup_soc_rev(void)
{
	int reg = __REG(ROM_SI_REV);

	switch (reg) {
	case 0x10:
		system_rev = 0x50000 | CHIP_REV_1_0;
		break;
	case 0x11:
		system_rev = 0x50000 | CHIP_REV_1_1_1;
		break;
	default:
		system_rev = 0x50000 | CHIP_REV_1_1_1;
	}
}

static inline void set_board_rev(int rev)
{
	system_rev |= (rev & 0xF) << 8;
}

static inline void setup_board_rev(void)
{
#if defined(CONFIG_MX50_RD3)
	unsigned int reg;

	set_board_rev(0x3);

	/* IO init for RD3 */
	// disable HDMI_PWR
	reg = readl(GPIO1_BASE_ADDR + GPIO_DR);
	reg &= (~(1<<25));
	writel(reg, GPIO1_BASE_ADDR + GPIO_DR);
	reg = readl(GPIO1_BASE_ADDR + GPIO_GDIR);
	reg |= (1<<25);
	writel(reg, GPIO1_BASE_ADDR + GPIO_GDIR);
	mxc_request_iomux(MX50_PIN_EIM_RW, IOMUX_CONFIG_ALT1);
	mxc_iomux_set_pad(MX50_PIN_EIM_RW, 0x84);
#endif
}

static inline void setup_arch_id(void)
{
#if defined(CONFIG_MX50_RDP) || defined(CONFIG_MX50_RD3)
	gd->bd->bi_arch_number = MACH_TYPE_MX50_RDP;
#elif defined(CONFIG_MX50_ARM2)
	gd->bd->bi_arch_number = MACH_TYPE_MX50_ARM2;
#else
#	error "Unsupported board!"
#endif
}

inline int is_soc_rev(int rev)
{
	return (system_rev & 0xFF) - rev;
}

#ifdef CONFIG_ARCH_MMU
void board_mmu_init(void)
{
	unsigned long ttb_base = PHYS_SDRAM_1 + 0x4000;
	unsigned long i;

	/*
	* Set the TTB register
	*/
	asm volatile ("mcr  p15,0,%0,c2,c0,0" : : "r"(ttb_base) /*:*/);

	/*
	* Set the Domain Access Control Register
	*/
	i = ARM_ACCESS_DACR_DEFAULT;
	asm volatile ("mcr  p15,0,%0,c3,c0,0" : : "r"(i) /*:*/);

	/*
	* First clear all TT entries - ie Set them to Faulting
	*/
	memset((void *)ttb_base, 0, ARM_FIRST_LEVEL_PAGE_TABLE_SIZE);
	/* Actual   Virtual  Size   Attributes          Function */
	/* Base     Base     MB     cached? buffered?  access permissions */
	/* xxx00000 xxx00000 */
	X_ARM_MMU_SECTION(0x000, 0x000, 0x10,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* ROM, 16M */
	X_ARM_MMU_SECTION(0x070, 0x070, 0x010,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* IRAM */
	X_ARM_MMU_SECTION(0x100, 0x100, 0x040,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* SATA */
	X_ARM_MMU_SECTION(0x180, 0x180, 0x100,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* IPUv3M */
	X_ARM_MMU_SECTION(0x200, 0x200, 0x200,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* GPU */
	X_ARM_MMU_SECTION(0x400, 0x400, 0x300,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* periperals */
	X_ARM_MMU_SECTION(0x700, 0x700, 0x400,
			ARM_CACHEABLE, ARM_BUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* CSD0 1G */
	X_ARM_MMU_SECTION(0x700, 0xB00, 0x400,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* CSD0 1G */
	X_ARM_MMU_SECTION(0xF00, 0xF00, 0x100,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* CS1 EIM control*/
	X_ARM_MMU_SECTION(0xF80, 0xF80, 0x001,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* iRam */

	/* Workaround for arm errata #709718 */
	/* Setup PRRR so device is always mapped to non-shared */
	asm volatile ("mrc p15, 0, %0, c10, c2, 0" : "=r"(i) : /*:*/);
	i &= (~(3 << 0x10));
	asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(i) /*:*/);

	/* Enable MMU */
	MMU_ON();
}
#endif

int dram_init(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
	return 0;
}

static void setup_uart(void)
{
	unsigned int reg;

#if defined(CONFIG_MX50_RD3)
	/* UART3 TXD */
	mxc_request_iomux(MX50_PIN_UART3_TXD, IOMUX_CONFIG_ALT1);
	mxc_iomux_set_pad(MX50_PIN_UART3_TXD, 0x1E4);
	/* Enable UART1 */
	reg = readl(GPIO6_BASE_ADDR + 0x0);
	reg |= (1 << 14);
	writel(reg, GPIO6_BASE_ADDR + 0x0);

	reg = readl(GPIO6_BASE_ADDR + 0x4);
	reg |= (1 << 14);
	writel(reg, GPIO6_BASE_ADDR + 0x4);
#endif
#ifdef CONFIG_MX50_E606XX
	/* UART2 RXD */
	mxc_request_iomux(MX50_PIN_UART2_RXD, IOMUX_CONFIG_ALT0);
	mxc_iomux_set_pad(MX50_PIN_UART2_RXD, 0x1E4);
	mxc_iomux_set_input(MUX_IN_UART2_IPP_UART_RXD_MUX_SELECT_INPUT, 0x3);

	/* UART2 TXD */
	mxc_request_iomux(MX50_PIN_UART2_TXD, IOMUX_CONFIG_ALT0);
	mxc_iomux_set_pad(MX50_PIN_UART2_TXD, 0x1E4);
#else
	/* UART1 RXD */
	mxc_request_iomux(MX50_PIN_UART1_RXD, IOMUX_CONFIG_ALT0);
	mxc_iomux_set_pad(MX50_PIN_UART1_RXD, 0x1E4);
	mxc_iomux_set_input(MUX_IN_UART1_IPP_UART_RXD_MUX_SELECT_INPUT, 0x1);

	/* UART1 TXD */
	mxc_request_iomux(MX50_PIN_UART1_TXD, IOMUX_CONFIG_ALT0);
	mxc_iomux_set_pad(MX50_PIN_UART1_TXD, 0x1E4);
#endif
}


static void _init_tps65185_power(int iIsWakeup,int iIsActivePwr)
{
	char cCmdA[256];
	int iCur_I2C_Chn;

	//ASSERT(O_pbRegVal);
	//

	if(!gptNtxHwCfg) {
		printf("%s(): cannot init without hwconfig !\n",__FUNCTION__);
		return ;
	}

	if(6==gptNtxHwCfg->m_val.bDisplayCtrl) {
		printf("init TPS65185 power ...\n");
		iCur_I2C_Chn=(int)(gdwBusNum+1);

		// Display controller is MX50+TPS65185 .
		
		// EPD PMIC power ...
		
		// power on the PMIC ...
		mxc_request_iomux(MX50_PIN_EIM_CRE,
				IOMUX_CONFIG_ALT1);
		//mxc_iomux_set_pad(MX50_PIN_EIM_CRE, 0);
		{
			unsigned int reg;
			// set output .
			reg = readl(GPIO1_BASE_ADDR + 0x4);
			reg |= (0x1 << 27);
			writel(reg, GPIO1_BASE_ADDR + 0x4);

#if 0
			// output low .
			reg = readl(GPIO1_BASE_ADDR + 0x0);
			reg &= ~(0x1 << 27);
			writel(reg, GPIO1_BASE_ADDR + 0x0);
			udelay(50*1000);
#endif

			// output high .
			reg = readl(GPIO1_BASE_ADDR + 0x0);
			reg |= (0x1 << 27);
			writel(reg, GPIO1_BASE_ADDR + 0x0);
			udelay(50*1000);
		}

		{
			// set PMIC into STANBY mode ...
			// EPD_PWRCTRL0 (WAKEUP)
			mxc_request_iomux(MX50_PIN_EPDC_PWRCTRL0,
				IOMUX_CONFIG_ALT1);
			//mxc_iomux_set_pad(MX50_PIN_EPDC_PWRCTRL0, 0);
			{
				unsigned int reg;
				// set output .
				reg = readl(GPIO3_BASE_ADDR + 0x4);
				reg |= (0x1 << 29);
				writel(reg, GPIO3_BASE_ADDR + 0x4);				
				
				reg = readl(GPIO3_BASE_ADDR + 0x0);
				if(iIsWakeup) {
					reg |= (0x1 << 29);
				}
				else {
					reg &= ~(0x1 << 29);
				}
				writel(reg, GPIO3_BASE_ADDR + 0x0);
			}
			udelay(50*1000);
		}

		{
			// EPD_PWRCTRL1 (PWRUP)
			mxc_request_iomux(MX50_PIN_EPDC_PWRCTRL1,
					IOMUX_CONFIG_ALT1);
			//mxc_iomux_set_pad(MX50_PIN_EPDC_PWRCTRL1, 0);
			{
				unsigned int reg;
				// set output .
				reg = readl(GPIO3_BASE_ADDR + 0x4);
				reg |= (0x1 << 30);
				writel(reg, GPIO3_BASE_ADDR + 0x4);	
				
				reg = readl(GPIO3_BASE_ADDR + 0x0);
				if(iIsActivePwr) {
					reg |= (0x1 << 30);
				}
				else {
					reg &= ~(0x1 << 30);
				}
				writel(reg, GPIO3_BASE_ADDR + 0x0);		
			}
			udelay(5*1000);
		}

	}

}


#ifdef CONFIG_I2C_MXC
static void setup_i2c(unsigned int module_base)
{
	int iI2C_Chn=-1;

	switch (module_base) {
	case I2C1_BASE_ADDR:
		iI2C_Chn=1;
		/* i2c1 SDA */
		mxc_request_iomux(MX50_PIN_I2C1_SDA,
				IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
		mxc_iomux_set_pad(MX50_PIN_I2C1_SDA, PAD_CTL_SRE_FAST |
				PAD_CTL_ODE_OPENDRAIN_ENABLE |
				PAD_CTL_DRV_HIGH | PAD_CTL_22K_PU |
				PAD_CTL_HYS_ENABLE|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
		/* i2c1 SCL */
		mxc_request_iomux(MX50_PIN_I2C1_SCL,
				IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
		mxc_iomux_set_pad(MX50_PIN_I2C1_SCL, PAD_CTL_SRE_FAST |
				PAD_CTL_ODE_OPENDRAIN_ENABLE |
				PAD_CTL_DRV_HIGH | PAD_CTL_22K_PU |
				PAD_CTL_HYS_ENABLE|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
		break;

	case I2C2_BASE_ADDR:
		iI2C_Chn=2;
		/* i2c2 SDA */
		mxc_request_iomux(MX50_PIN_I2C2_SDA,
				IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
		mxc_iomux_set_pad(MX50_PIN_I2C2_SDA,
				PAD_CTL_SRE_FAST |
				PAD_CTL_ODE_OPENDRAIN_ENABLE |
				PAD_CTL_DRV_HIGH | PAD_CTL_22K_PU |
				PAD_CTL_HYS_ENABLE|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);

		/* i2c2 SCL */
		mxc_request_iomux(MX50_PIN_I2C2_SCL,
				IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
		mxc_iomux_set_pad(MX50_PIN_I2C2_SCL,
				PAD_CTL_SRE_FAST |
				PAD_CTL_ODE_OPENDRAIN_ENABLE |
				PAD_CTL_DRV_HIGH | PAD_CTL_22K_PU |
				PAD_CTL_HYS_ENABLE|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
	
		break;

	case I2C3_BASE_ADDR:
		iI2C_Chn=3;
		/* i2c3 SDA */
		mxc_request_iomux(MX50_PIN_I2C3_SDA,
				IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
		mxc_iomux_set_pad(MX50_PIN_I2C3_SDA,
				PAD_CTL_SRE_FAST |
				PAD_CTL_ODE_OPENDRAIN_ENABLE |
				PAD_CTL_DRV_HIGH | PAD_CTL_22K_PU |
				PAD_CTL_HYS_ENABLE|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);

		/* i2c3 SCL */
		mxc_request_iomux(MX50_PIN_I2C3_SCL,
				IOMUX_CONFIG_ALT0 | IOMUX_CONFIG_SION);
		mxc_iomux_set_pad(MX50_PIN_I2C3_SCL,
				PAD_CTL_SRE_FAST |
				PAD_CTL_ODE_OPENDRAIN_ENABLE |
				PAD_CTL_DRV_HIGH | PAD_CTL_22K_PU |
				PAD_CTL_HYS_ENABLE|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
		break;
	default:
		printf("Invalid I2C base: 0x%x\n", module_base);
		break;
	}

	//
	// devices power initialize ...
	//
	if(IS_I2C_CHN_TPS65185(iI2C_Chn)) {
		_init_tps65185_power(1,0);
	}
	
	if(34==gptNtxHwCfg->m_val.bPCB || 35==gptNtxHwCfg->m_val.bPCB){
		gpio_out_init(&gt_ntx_gpio_tp_rst);
		gpio_out_init(&gt_ntx_gpio_tp_pwen);
		udelay(50*1000);
		gpio_output(&gt_ntx_gpio_tp_rst,1);
	}
	if(41==gptNtxHwCfg->m_val.bPCB){
		gpio_out_init(&gt_ntx_gpio_tp_pwen);
	}
}

#endif

#ifdef CONFIG_IMX_CSPI
s32 spi_get_cfg(struct imx_spi_dev_t *dev)
{
	switch (dev->slave.cs) {
	case 0:
		/* PMIC */
		dev->base = CSPI3_BASE_ADDR;
		dev->freq = 25000000;
		dev->ss_pol = IMX_SPI_ACTIVE_HIGH;
		dev->ss = 0;
		dev->fifo_sz = 32;
		dev->us_delay = 0;
		break;
	case 1:
		/* SPI-NOR */
		dev->base = CSPI3_BASE_ADDR;
		dev->freq = 25000000;
		dev->ss_pol = IMX_SPI_ACTIVE_LOW;
		dev->ss = 1;
		dev->fifo_sz = 32;
		dev->us_delay = 0;
		break;
	default:
		printf("Invalid Bus ID!\n");
	}

	return 0;
}

void spi_io_init(struct imx_spi_dev_t *dev)
{
	switch (dev->base) {
	case CSPI3_BASE_ADDR:
		mxc_request_iomux(MX50_PIN_CSPI_MOSI, IOMUX_CONFIG_ALT0);
		mxc_iomux_set_pad(MX50_PIN_CSPI_MOSI, 0x4);

		mxc_request_iomux(MX50_PIN_CSPI_MISO, IOMUX_CONFIG_ALT0);
		mxc_iomux_set_pad(MX50_PIN_CSPI_MISO, 0x4);

		if (dev->ss == 0) {
			/* de-select SS1 of instance: cspi */
			mxc_request_iomux(MX50_PIN_ECSPI1_MOSI,
						IOMUX_CONFIG_ALT1);

			mxc_request_iomux(MX50_PIN_CSPI_SS0, IOMUX_CONFIG_ALT0);
			mxc_iomux_set_pad(MX50_PIN_CSPI_SS0, 0xE4);
		} else if (dev->ss == 1) {
			/* de-select SS0 of instance: cspi */
			mxc_request_iomux(MX50_PIN_CSPI_SS0, IOMUX_CONFIG_ALT1);

			mxc_request_iomux(MX50_PIN_ECSPI1_MOSI,
						IOMUX_CONFIG_ALT2);
			mxc_iomux_set_pad(MX50_PIN_ECSPI1_MOSI, 0xE4);
			mxc_iomux_set_input(
			MUX_IN_CSPI_IPP_IND_SS1_B_SELECT_INPUT, 0x1);
		}

		mxc_request_iomux(MX50_PIN_CSPI_SCLK, IOMUX_CONFIG_ALT0);
		mxc_iomux_set_pad(MX50_PIN_CSPI_SCLK, 0x4);
		break;
	case CSPI2_BASE_ADDR:
	case CSPI1_BASE_ADDR:
		/* ecspi1-2 fall through */
		break;
	default:
		break;
	}
}
#endif

#ifdef CONFIG_NAND_GPMI
void setup_gpmi_nand(void)
{
	u32 src_sbmr = readl(SRC_BASE_ADDR + 0x4);

	/* Fix for gpmi gatelevel issue */
	mxc_iomux_set_pad(MX50_PIN_SD3_CLK, 0x00e4);

	/* RESETN,WRN,RDN,DATA0~7 Signals iomux*/
	/* Check if 1.8v NAND is to be supported */
	if ((src_sbmr & 0x00000004) >> 2)
		*(u32 *)(IOMUXC_BASE_ADDR + PAD_GRP_START + 0x58) = (0x1 << 13);

	/* RESETN */
	mxc_request_iomux(MX50_PIN_SD3_WP, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_WP, PAD_CTL_DRV_HIGH);

	/* WRN */
	mxc_request_iomux(MX50_PIN_SD3_CMD, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_CMD, PAD_CTL_DRV_HIGH);

	/* RDN */
	mxc_request_iomux(MX50_PIN_SD3_CLK, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_CLK, PAD_CTL_DRV_HIGH);

	/* D0 */
	mxc_request_iomux(MX50_PIN_SD3_D4, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_D4, PAD_CTL_DRV_HIGH);

	/* D1 */
	mxc_request_iomux(MX50_PIN_SD3_D5, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_D5, PAD_CTL_DRV_HIGH);

	/* D2 */
	mxc_request_iomux(MX50_PIN_SD3_D6, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_D6, PAD_CTL_DRV_HIGH);

	/* D3 */
	mxc_request_iomux(MX50_PIN_SD3_D7, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_D7, PAD_CTL_DRV_HIGH);

	/* D4 */
	mxc_request_iomux(MX50_PIN_SD3_D0, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_D0, PAD_CTL_DRV_HIGH);

	/* D5 */
	mxc_request_iomux(MX50_PIN_SD3_D1, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_D1, PAD_CTL_DRV_HIGH);

	/* D6 */
	mxc_request_iomux(MX50_PIN_SD3_D2, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_D2, PAD_CTL_DRV_HIGH);

	/* D7 */
	mxc_request_iomux(MX50_PIN_SD3_D3, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_SD3_D3, PAD_CTL_DRV_HIGH);

	/*CE0~3,and other four controls signals muxed on KPP*/
	switch ((src_sbmr & 0x00000018) >> 3) {
	case  0:
		/* Muxed on key */
		if ((src_sbmr & 0x00000004) >> 2)
			*(u32 *)(IOMUXC_BASE_ADDR + PAD_GRP_START + 0x20) =
								(0x1 << 13);

		/* CLE */
		mxc_request_iomux(MX50_PIN_KEY_COL0, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_KEY_COL0, PAD_CTL_DRV_HIGH);

		/* ALE */
		mxc_request_iomux(MX50_PIN_KEY_ROW0, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_KEY_ROW0, PAD_CTL_DRV_HIGH);

		/* READY0 */
		mxc_request_iomux(MX50_PIN_KEY_COL3, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_KEY_COL3,
				PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
				PAD_CTL_100K_PU);
		mxc_iomux_set_input(
			MUX_IN_RAWNAND_U_GPMI_INPUT_GPMI_RDY0_IN_SELECT_INPUT,
			INPUT_CTL_PATH0);

		/* DQS */
		mxc_request_iomux(MX50_PIN_KEY_ROW3, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_KEY_ROW3, PAD_CTL_DRV_HIGH);
		mxc_iomux_set_input(
			MUX_IN_RAWNAND_U_GPMI_INPUT_GPMI_DQS_IN_SELECT_INPUT,
			INPUT_CTL_PATH0);

		/* CE0 */
		mxc_request_iomux(MX50_PIN_KEY_COL1, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_KEY_COL1, PAD_CTL_DRV_HIGH);

		/* CE1 */
		mxc_request_iomux(MX50_PIN_KEY_ROW1, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_KEY_ROW1, PAD_CTL_DRV_HIGH);

		/* CE2 */
		mxc_request_iomux(MX50_PIN_KEY_COL2, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_KEY_COL2, PAD_CTL_DRV_HIGH);

		/* CE3 */
		mxc_request_iomux(MX50_PIN_KEY_ROW2, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_KEY_ROW2, PAD_CTL_DRV_HIGH);

		break;
	case 1:
		if ((src_sbmr & 0x00000004) >> 2)
			*(u32 *)(IOMUXC_BASE_ADDR + PAD_GRP_START + 0xc) =
								(0x1 << 13);

		/* CLE */
		mxc_request_iomux(MX50_PIN_EIM_DA8, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA8, PAD_CTL_DRV_HIGH);

		/* ALE */
		mxc_request_iomux(MX50_PIN_EIM_DA9, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA9, PAD_CTL_DRV_HIGH);

		/* READY0 */
		mxc_request_iomux(MX50_PIN_EIM_DA14, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA14,
				PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
				PAD_CTL_100K_PU);
		mxc_iomux_set_input(
			MUX_IN_RAWNAND_U_GPMI_INPUT_GPMI_RDY0_IN_SELECT_INPUT,
			INPUT_CTL_PATH2);

		/* DQS */
		mxc_request_iomux(MX50_PIN_EIM_DA15, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA15, PAD_CTL_DRV_HIGH);
		mxc_iomux_set_input(
			MUX_IN_RAWNAND_U_GPMI_INPUT_GPMI_DQS_IN_SELECT_INPUT,
			INPUT_CTL_PATH2);

		/* CE0 */
		mxc_request_iomux(MX50_PIN_EIM_DA10, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA10, PAD_CTL_DRV_HIGH);

		/* CE1 */
		mxc_request_iomux(MX50_PIN_EIM_DA11, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA11, PAD_CTL_DRV_HIGH);

		/* CE2 */
		mxc_request_iomux(MX50_PIN_EIM_DA12, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA12, PAD_CTL_DRV_HIGH);

		/* CE3 */
		mxc_request_iomux(MX50_PIN_EIM_DA13, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA13, PAD_CTL_DRV_HIGH);

		break;
	case 2:
		if ((src_sbmr & 0x00000004) >> 2)
			*(u32 *)(IOMUXC_BASE_ADDR + PAD_GRP_START + 0x48) =
								(0x1 << 13);

		/* CLE */
		mxc_request_iomux(MX50_PIN_DISP_D8, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_DISP_D8, PAD_CTL_DRV_HIGH);

		/* ALE */
		mxc_request_iomux(MX50_PIN_DISP_D9, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_DISP_D9, PAD_CTL_DRV_HIGH);

		/* READY0 */
		mxc_request_iomux(MX50_PIN_DISP_D14, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_DISP_D14,
				PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
				PAD_CTL_100K_PU);
		mxc_iomux_set_input(
			MUX_IN_RAWNAND_U_GPMI_INPUT_GPMI_RDY0_IN_SELECT_INPUT,
			INPUT_CTL_PATH1);

		/* DQS */
		mxc_request_iomux(MX50_PIN_DISP_D15, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_DISP_D15, PAD_CTL_DRV_HIGH);
		mxc_iomux_set_input(
			MUX_IN_RAWNAND_U_GPMI_INPUT_GPMI_DQS_IN_SELECT_INPUT,
			INPUT_CTL_PATH1);

		/* CE0 */
		mxc_request_iomux(MX50_PIN_DISP_D10, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_DISP_D10, PAD_CTL_DRV_HIGH);

		/* CE1 */
		mxc_request_iomux(MX50_PIN_EIM_DA11, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_EIM_DA11, PAD_CTL_DRV_HIGH);

		/* CE2 */
		mxc_request_iomux(MX50_PIN_DISP_D12, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_DISP_D12, PAD_CTL_DRV_HIGH);

		/* CE3 */
		mxc_request_iomux(MX50_PIN_DISP_D13, IOMUX_CONFIG_ALT2);
		mxc_iomux_set_pad(MX50_PIN_DISP_D13, PAD_CTL_DRV_HIGH);

		break;
	default:
		break;
	}
}
#endif

#ifdef CONFIG_MXC_FEC

#ifdef CONFIG_GET_FEC_MAC_ADDR_FROM_IIM

#define HW_OCOTP_MACn(n)	(0x00000250 + (n) * 0x10)

int fec_get_mac_addr(unsigned char *mac)
{
	u32 *ocotp_mac_base =
		(u32 *)(OCOTP_CTRL_BASE_ADDR + HW_OCOTP_MACn(0));
	int i;

	for (i = 0; i < 6; ++i, ++ocotp_mac_base)
		mac[6 - 1 - i] = readl(++ocotp_mac_base);

	return 0;
}
#endif

static void setup_fec(void)
{
	volatile unsigned int reg;

#if defined(CONFIG_MX50_RDP)
	/* FEC_EN: gpio6-23 set to 0 to enable FEC */
	mxc_request_iomux(MX50_PIN_I2C3_SDA, IOMUX_CONFIG_ALT1);

	reg = readl(GPIO6_BASE_ADDR + 0x0);
	reg &= ~(1 << 23);
	writel(reg, GPIO6_BASE_ADDR + 0x0);

	reg = readl(GPIO6_BASE_ADDR + 0x4);
	reg |= (1 << 23);
	writel(reg, GPIO6_BASE_ADDR + 0x4);

#elif defined(CONFIG_MX50_RD3)
	/* FEC_EN: gpio4-15 set to 0 to enable FEC */
	mxc_request_iomux(MX50_PIN_I2C3_SDA, IOMUX_CONFIG_ALT1);

	reg = readl(GPIO4_BASE_ADDR + 0x0);
	reg |= (1 << 15);
	writel(reg, GPIO4_BASE_ADDR + 0x0);

	reg = readl(GPIO4_BASE_ADDR + 0x4);
	reg |= (1 << 15);
	writel(reg, GPIO4_BASE_ADDR + 0x4);
#endif

	/*FEC_MDIO*/
	mxc_request_iomux(MX50_PIN_SSI_RXC, IOMUX_CONFIG_ALT6);
	mxc_iomux_set_pad(MX50_PIN_SSI_RXC, 0xC);
	mxc_iomux_set_input(MUX_IN_FEC_FEC_MDI_SELECT_INPUT, 0x1);

	/*FEC_MDC*/
	mxc_request_iomux(MX50_PIN_SSI_RXFS, IOMUX_CONFIG_ALT6);
	mxc_iomux_set_pad(MX50_PIN_SSI_RXFS, 0x004);

	/* FEC RXD1 */
	mxc_request_iomux(MX50_PIN_DISP_D3, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_DISP_D3, 0x0);
	mxc_iomux_set_input(MUX_IN_FEC_FEC_RDATA_1_SELECT_INPUT, 0x0);

	/* FEC RXD0 */
	mxc_request_iomux(MX50_PIN_DISP_D4, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_DISP_D4, 0x0);
	mxc_iomux_set_input(MUX_IN_FEC_FEC_RDATA_0_SELECT_INPUT, 0x0);

	 /* FEC TXD1 */
	mxc_request_iomux(MX50_PIN_DISP_D6, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_DISP_D6, 0x004);

	/* FEC TXD0 */
	mxc_request_iomux(MX50_PIN_DISP_D7, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_DISP_D7, 0x004);

	/* FEC TX_EN */
	mxc_request_iomux(MX50_PIN_DISP_D5, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_DISP_D5, 0x004);

	/* FEC TX_CLK */
	mxc_request_iomux(MX50_PIN_DISP_D0, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_DISP_D0, 0x0);
	mxc_iomux_set_input(MUX_IN_FEC_FEC_TX_CLK_SELECT_INPUT, 0x0);

	/* FEC RX_ER */
	mxc_request_iomux(MX50_PIN_DISP_D1, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_DISP_D1, 0x0);
	mxc_iomux_set_input(MUX_IN_FEC_FEC_RX_ER_SELECT_INPUT, 0);

	/* FEC CRS */
	mxc_request_iomux(MX50_PIN_DISP_D2, IOMUX_CONFIG_ALT2);
	mxc_iomux_set_pad(MX50_PIN_DISP_D2, 0x0);
	mxc_iomux_set_input(MUX_IN_FEC_FEC_RX_DV_SELECT_INPUT, 0);

#if defined(CONFIG_MX50_RDP) || defined(CONFIG_MX50_RD3)
	/* FEC_RESET_B: gpio4-12 */
	mxc_request_iomux(MX50_PIN_ECSPI1_SCLK, IOMUX_CONFIG_ALT1);

	reg = readl(GPIO4_BASE_ADDR + 0x0);
	reg &= ~(1 << 12);
	writel(reg, GPIO4_BASE_ADDR + 0x0);

	reg = readl(GPIO4_BASE_ADDR + 0x4);
	reg |= (1 << 12);
	writel(reg, GPIO4_BASE_ADDR + 0x4);

	udelay(500);

	reg = readl(GPIO4_BASE_ADDR + 0x0);
	reg |= (1 << 12);
	writel(reg, GPIO4_BASE_ADDR + 0x0);
#elif defined(CONFIG_MX50_ARM2)
	/* phy reset: gpio4-6 */
	mxc_request_iomux(MX50_PIN_KEY_COL3, IOMUX_CONFIG_ALT1);

	reg = readl(GPIO4_BASE_ADDR + 0x0);
	reg &= ~0x40;
	writel(reg, GPIO4_BASE_ADDR + 0x0);

	reg = readl(GPIO4_BASE_ADDR + 0x4);
	reg |= 0x40;
	writel(reg, GPIO4_BASE_ADDR + 0x4);

	udelay(500);

	reg = readl(GPIO4_BASE_ADDR + 0x0);
	reg |= 0x40;
	writel(reg, GPIO4_BASE_ADDR + 0x0);
#else
#	error "Unsupported board!"
#endif
}
#endif

#ifdef CONFIG_CMD_MMC
struct fsl_esdhc_cfg esdhc_cfg[3] = {
	{MMC_SDHC1_BASE_ADDR, 1, 1},
	{MMC_SDHC2_BASE_ADDR, 1, 1},
	{MMC_SDHC3_BASE_ADDR, 1, 1},
};


#ifdef CONFIG_DYNAMIC_MMC_DEVNO
int get_mmc_env_devno(void)
{
	uint soc_sbmr = readl(SRC_BASE_ADDR + 0x4);
	int mmc_devno = 0;

	switch (soc_sbmr & 0x00300000) {
	default:
	case 0x0:
		mmc_devno = 0;
		break;
	case 0x00100000:
		mmc_devno = 1;
		break;
	case 0x00200000:
		mmc_devno = 2;
		break;
	}

	return mmc_devno;
}
#endif


int esdhc_gpio_init(bd_t *bis)
{
	s32 status = 0;
	u32 index = 0;

	for (index = 0; index < CONFIG_SYS_FSL_ESDHC_NUM;
		++index) {
		switch (index) {
		case 0:
			mxc_request_iomux(MX50_PIN_SD1_CMD, IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD1_CLK, IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD1_D0,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD1_D1,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD1_D2,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD1_D3,  IOMUX_CONFIG_ALT0);

			mxc_iomux_set_pad(MX50_PIN_SD1_CMD, 0x1E4);
			mxc_iomux_set_pad(MX50_PIN_SD1_CLK, 0xD4);
			mxc_iomux_set_pad(MX50_PIN_SD1_D0,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD1_D1,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD1_D2,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD1_D3,  0x1D4);

			break;
		case 1:
			mxc_request_iomux(MX50_PIN_SD2_CMD, IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_CLK, IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_D0,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_D1,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_D2,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_D3,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_D4,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_D5,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_D6,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD2_D7,  IOMUX_CONFIG_ALT0);

			mxc_iomux_set_pad(MX50_PIN_SD2_CMD, 0x1E4);
			mxc_iomux_set_pad(MX50_PIN_SD2_CLK, 0xD4);
			mxc_iomux_set_pad(MX50_PIN_SD2_D0,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD2_D1,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD2_D2,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD2_D3,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD2_D4,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD2_D5,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD2_D6,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD2_D7,  0x1D4);

			break;
		case 2:
#ifndef CONFIG_NAND_GPMI
			mxc_request_iomux(MX50_PIN_SD3_CMD, IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_CLK, IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_D0,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_D1,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_D2,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_D3,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_D4,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_D5,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_D6,  IOMUX_CONFIG_ALT0);
			mxc_request_iomux(MX50_PIN_SD3_D7,  IOMUX_CONFIG_ALT0);

			mxc_iomux_set_pad(MX50_PIN_SD3_CMD, 0x1E4);
			mxc_iomux_set_pad(MX50_PIN_SD3_CLK, 0xD4);
			mxc_iomux_set_pad(MX50_PIN_SD3_D0,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD3_D1,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD3_D2,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD3_D3,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD3_D4,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD3_D5,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD3_D6,  0x1D4);
			mxc_iomux_set_pad(MX50_PIN_SD3_D7,  0x1D4);
#endif
			break;
		default:
			printf("Warning: you configured more ESDHC controller"
				"(%d) as supported by the board(2)\n",
				CONFIG_SYS_FSL_ESDHC_NUM);
			return status;
			break;
		}
		status |= fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
	}

	return status;
}

int board_mmc_init(bd_t *bis)
{
	if (!esdhc_gpio_init(bis))
		return 0;
	else
		return -1;
}

#endif

#ifdef CONFIG_MXC_EPDC
#ifdef CONFIG_SPLASH_SCREEN
int setup_splash_img()
{
#ifdef CONFIG_SPLASH_IS_IN_MMC
	int mmc_dev = get_mmc_env_devno();
	ulong offset = CONFIG_SPLASH_IMG_OFFSET;
	ulong size = CONFIG_SPLASH_IMG_SIZE;
	ulong addr = 0;
	char *s = NULL;
	struct mmc *mmc = find_mmc_device(mmc_dev);
	uint blk_start, blk_cnt, n;

	s = getenv("splashimage");

	if (NULL == s) {
		puts("env splashimage not found!\n");
		return -1;
	}
	addr = simple_strtoul(s, NULL, 16);

	if (!mmc) {
		printf("MMC Device %d not found\n",
			mmc_dev);
		return -1;
	}

	if (mmc_init(mmc)) {
		puts("MMC init failed\n");
		return  -1;
	}

	blk_start = ALIGN(offset, mmc->read_bl_len) / mmc->read_bl_len;
	blk_cnt   = ALIGN(size, mmc->read_bl_len) / mmc->read_bl_len;
	n = mmc->block_dev.block_read(mmc_dev, blk_start,
					blk_cnt, (u_char *)addr);
	flush_cache((ulong)addr, blk_cnt * mmc->read_bl_len);

	return (n == blk_cnt) ? 0 : -1;
#endif
}
#endif

vidinfo_t panel_info = {
	.vl_refresh = 60,
	.vl_col = 800,
	.vl_row = 600,
	.vl_pixclock = 17700000,
	.vl_left_margin = 8,
	.vl_right_margin = 142,
	.vl_upper_margin = 4,
	.vl_lower_margin = 10,
	.vl_hsync = 20,
	.vl_vsync = 4,
	.vl_sync = 0,
	.vl_mode = 0,
	.vl_flag = 0,
	.vl_bpix = 3,
	cmap:0,
};

static void setup_epdc_power()
{
	unsigned int reg;

	/* Setup epdc voltage */

	/* EPDC PWRSTAT - GPIO3[28] for PWR_GOOD status */
	mxc_request_iomux(MX50_PIN_EPDC_PWRSTAT, IOMUX_CONFIG_ALT1);

	/* EPDC VCOM0 - GPIO4[21] for VCOM control */
	mxc_request_iomux(MX50_PIN_EPDC_VCOM0, IOMUX_CONFIG_ALT1);
	/* Set as output */
	reg = readl(GPIO4_BASE_ADDR + 0x4);
	reg |= (1 << 21);
	writel(reg, GPIO4_BASE_ADDR + 0x4);

	/* UART4 TXD - GPIO6[16] for EPD PMIC WAKEUP */
	mxc_request_iomux(MX50_PIN_UART4_TXD, IOMUX_CONFIG_ALT1);
	/* Set as output */
	reg = readl(GPIO6_BASE_ADDR + 0x4);
	reg |= (1 << 16);
	writel(reg, GPIO6_BASE_ADDR + 0x4);
}

void epdc_power_on()
{
	unsigned int reg;

	/* Set PMIC Wakeup to high - enable Display power */
	reg = readl(GPIO6_BASE_ADDR + 0x0);
	reg |= (1 << 16);
	writel(reg, GPIO6_BASE_ADDR + 0x0);

	/* Wait for PWRGOOD == 1 */
	while (1) {
		reg = readl(GPIO3_BASE_ADDR + 0x0);
		if (!(reg & (1 << 28)))
			break;

		udelay(100);
	}

	/* Enable VCOM */
	reg = readl(GPIO4_BASE_ADDR + 0x0);
	reg |= (1 << 21);
	writel(reg, GPIO4_BASE_ADDR + 0x0);

	reg = readl(GPIO4_BASE_ADDR + 0x0);

	udelay(500);
}

void  epdc_power_off()
{
	unsigned int reg;
	/* Set PMIC Wakeup to low - disable Display power */
	reg = readl(GPIO6_BASE_ADDR + 0x0);
	reg |= 0 << 16;
	writel(reg, GPIO6_BASE_ADDR + 0x0);

	/* Disable VCOM */
	reg = readl(GPIO4_BASE_ADDR + 0x0);
	reg |= 0 << 21;
	writel(reg, GPIO4_BASE_ADDR + 0x0);
}

int setup_waveform_file()
{
#ifdef CONFIG_WAVEFORM_FILE_IN_MMC
	int mmc_dev = get_mmc_env_devno();
	ulong offset = CONFIG_WAVEFORM_FILE_OFFSET;
	ulong size = CONFIG_WAVEFORM_FILE_SIZE;
	ulong addr = CONFIG_WAVEFORM_BUF_ADDR;
	char *s = NULL;
	struct mmc *mmc = find_mmc_device(mmc_dev);
	uint blk_start, blk_cnt, n;

	if (!mmc) {
		printf("MMC Device %d not found\n",
			mmc_dev);
		return -1;
	}

	if (mmc_init(mmc)) {
		puts("MMC init failed\n");
		return -1;
	}

	blk_start = ALIGN(offset, mmc->read_bl_len) / mmc->read_bl_len;
	blk_cnt   = ALIGN(size, mmc->read_bl_len) / mmc->read_bl_len;
	n = mmc->block_dev.block_read(mmc_dev, blk_start,
		blk_cnt, (u_char *)addr);
	flush_cache((ulong)addr, blk_cnt * mmc->read_bl_len);

	return (n == blk_cnt) ? 0 : -1;
#else
	return -1;
#endif
}

static void setup_epdc()
{
	unsigned int reg;

	/* epdc iomux settings */
	mxc_request_iomux(MX50_PIN_EPDC_D0, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_D1, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_D2, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_D3, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_D4, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_D5, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_D6, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_D7, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_GDCLK, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_GDSP, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_GDOE, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_GDRL, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_SDCLK, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_SDOE, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_SDLE, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_SDSHR, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_BDR0, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_SDCE0, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_SDCE1, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_EPDC_SDCE2, IOMUX_CONFIG_ALT0);


	/*** epdc Maxim PMIC settings ***/

	/* EPDC PWRSTAT - GPIO3[28] for PWR_GOOD status */
	mxc_request_iomux(MX50_PIN_EPDC_PWRSTAT, IOMUX_CONFIG_ALT1);

	/* EPDC VCOM0 - GPIO4[21] for VCOM control */
	mxc_request_iomux(MX50_PIN_EPDC_VCOM0, IOMUX_CONFIG_ALT1);

	/* UART4 TXD - GPIO6[16] for EPD PMIC WAKEUP */
	mxc_request_iomux(MX50_PIN_UART4_TXD, IOMUX_CONFIG_ALT1);


	/*** Set pixel clock rates for EPDC ***/

	/* EPDC AXI clk and EPDC PIX clk from PLL1 */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CLKSEQ_BYPASS);
	reg &= ~(0x3 << 4);
	reg |= (0x2 << 4) | (0x2 << 12);
	writel(reg, CCM_BASE_ADDR + CLKCTL_CLKSEQ_BYPASS);

	/* EPDC AXI clk enable and set to 200MHz (800/4) */
	reg = readl(CCM_BASE_ADDR + 0xA8);
	reg &= ~((0x3 << 30) | 0x3F);
	reg |= (0x2 << 30) | 0x4;
	writel(reg, CCM_BASE_ADDR + 0xA8);

	/* EPDC PIX clk enable and set to 20MHz (800/40) */
	reg = readl(CCM_BASE_ADDR + 0xA0);
	reg &= ~((0x3 << 30) | (0x3 << 12) | 0x3F);
	reg |= (0x2 << 30) | (0x1 << 12) | 0x2D;
	writel(reg, CCM_BASE_ADDR + 0xA0);

	panel_info.epdc_data.working_buf_addr = CONFIG_WORKING_BUF_ADDR;
	panel_info.epdc_data.waveform_buf_addr = CONFIG_WAVEFORM_BUF_ADDR;

	panel_info.epdc_data.wv_modes.mode_init = 0;
	panel_info.epdc_data.wv_modes.mode_du = 1;
	panel_info.epdc_data.wv_modes.mode_gc4 = 3;
	panel_info.epdc_data.wv_modes.mode_gc8 = 2;
	panel_info.epdc_data.wv_modes.mode_gc16 = 2;
	panel_info.epdc_data.wv_modes.mode_gc32 = 2;

	setup_epdc_power();

	/* Assign fb_base */
	gd->fb_base = CONFIG_FB_BASE;
}
#endif


static int IsMaxCpuFreq1GHz(void)
{
#if 1 //[
	if(gptNtxHwCfg) {
		if(2==gptNtxHwCfg->m_val.bCPUFreq) {
			return 1;
		}
		else {
			return 0;
		}
	}
	else {
		printf("[WARNING] %s no hwconfig !\n",__FUNCTION__);
		return 0;
	}

#else //][
	char *bootargs;

	bootargs = getenv("bootargs_base");

	if( bootargs != NULL &&
		strstr(bootargs, "mx50_1GHz") != NULL)
	{
		return 1;
	}
	return 0;
#endif //]
}


#ifdef CONFIG_IMX_CSPI
static void setup_power(void)
{
	struct spi_slave *slave;
	unsigned int val;

	puts("PMIC Mode: SPI\n");

	/* Enable VGEN1 to enable ethernet */
	slave = spi_pmic_probe();

#if defined(CONFIG_MX50_RD3)
	// MC34708 (Ripley)
	// set SW1 operating mode to PWM/PFS
	val = pmic_reg(slave, 28, 0, 0);
	val &= ~0x00000F;
	val |= 0x00000D;
	pmic_reg(slave, 28, val, 1);

	if(IsMaxCpuFreq1GHz()) {
		// setup SW1A/B power @1.275V
		pmic_reg(slave, 24, 0x432, 1);
		// setup SW2/3 power, SW2=1.225V, SW3=1.30V
		pmic_reg(slave, 25, 0x31A62E, 1);
	}
	else {
		// setup SW2/3 power, SW2=1.225V, SW3=1.20V
		pmic_reg(slave, 25, 0x31662E, 1);
	}
	udelay(10000);
	
	// set VGEN1 to 1.25V
	val = pmic_reg(slave, 30, 0, 0);
	val &= 0xFFFFF7;
	val |= 0x01;
	pmic_reg(slave, 30, val, 1);

	// reset PMIC IRQ mask
	pmic_reg(slave, 1, 0xffffff, 1);
	pmic_reg(slave, 4, 0xffffff, 1);

	/* Set global reset time to 0s*/
	val = pmic_reg(slave, 15, 0, 0);
	val &= ~(0x300);
	pmic_reg(slave, 15, val, 1);
#else
	// MC13892
	// set SW1 operating mode to PWM/AUTO
	val = pmic_reg(slave, 28, 0, 0);
	val &= ~0x00000F;
	val |= 0x000006;
	pmic_reg(slave, 28, val, 1);
	
	if(IsMaxCpuFreq1GHz()) {
		// setup SW1 power @1.275V
		val = pmic_reg(slave, MC13892_REG_24, 0, 0);
		val &= ~0x1F;
		val |= 0x1B;
		pmic_reg(slave, MC13892_REG_24, val, 1);
		// setup SW3 power SW3=1.30V
		val = pmic_reg(slave, MC13892_REG_26, 0x00381C, 1);
	}
	udelay(10000);

	val = pmic_reg(slave, 30, 0, 0);
	val |= 0x3;
	pmic_reg(slave, 30, val, 1);

	val = pmic_reg(slave, 32, 0, 0);
	val |= 0x1;
	pmic_reg(slave, 32, val, 1);

	/* Enable VCAM   */
	val = pmic_reg(slave, 33, 0, 0);
	val |= 0x40;
	pmic_reg(slave, 33, val, 1);
#endif

	spi_pmic_free(slave);
}

void setup_voltage_cpu(void)
{
	/* Currently VDDGP 1.05v
	 * no one tell me we need increase the core
	 * voltage to let CPU run at 800Mhz, not do it
	 */

	/* Raise the core frequency to 800MHz */
	writel(0x0, CCM_BASE_ADDR + CLKCTL_CACRR);

}
#endif

#define ON_LED(_IsTurnON)	\
	switch(gptNtxHwCfg->m_val.bPCB) {\
	case 34:/* E606FXA */\
	case 35:/* E606FXB */\
	case 28:/* E606CX */\
	case 41:/* E606GX */\
		E60612_led_B(_IsTurnON);\
		break;\
	default:\
		E60612_led_G(_IsTurnON);\
		break;\
	}

void E60612_led_R(int iIsTrunOn)
{
	unsigned int reg;
	reg = readl(GPIO6_BASE_ADDR + 0x0);
	if(iIsTrunOn) {
		reg &= ~(0x1 << 25);
	}
	else {
		reg |= (0x1 << 25);
	}
	writel(reg, GPIO6_BASE_ADDR + 0x0);
	
	
	reg = readl(GPIO1_BASE_ADDR + 0x0);
	if(iIsTrunOn) {
		reg &= ~(0x1 << 25);
	}
	else {
		reg |= (0x1 << 25);
	}
	writel(reg, GPIO1_BASE_ADDR + 0x0);
}
void E60612_led_G(int iIsTrunOn)
{
	unsigned int reg;
	reg = readl(GPIO6_BASE_ADDR + 0x0);
	if(iIsTrunOn) {
		reg &= ~(0x1 << 26);
	}
	else {
		reg |= (0x1 << 26);
	}
	writel(reg, GPIO6_BASE_ADDR + 0x0);
	
	
	reg = readl(GPIO1_BASE_ADDR + 0x0);
	if(iIsTrunOn) {
		reg &= ~(0x1 << 24);
	}
	else {
		reg |= (0x1 << 24);
	}
	writel(reg, GPIO1_BASE_ADDR + 0x0);
	
}
void E60612_led_B(int iIsTrunOn)
{
	unsigned int reg;
	reg = readl(GPIO6_BASE_ADDR + 0x0);
	if(iIsTrunOn) {
		reg &= ~(0x1 << 24);
	}
	else {
		reg |= (0x1 << 24);
	}
	writel(reg, GPIO6_BASE_ADDR + 0x0);
}
#define BOOT_DIRTYLINE_FIX 1
int board_init(void)
{
	unsigned int reg;
	
	// initial MSP_INT
	mxc_request_iomux(MX50_PIN_CSPI_SS0, IOMUX_CONFIG_ALT1);
	mxc_iomux_set_pad(MX50_PIN_CSPI_SS0, PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);

	mxc_request_iomux(MX50_PIN_OWIRE, IOMUX_CONFIG_ALT1);
	mxc_request_iomux(MX50_PIN_PWM1, IOMUX_CONFIG_ALT1);
	mxc_request_iomux(MX50_PIN_PWM2, IOMUX_CONFIG_ALT1);
	mxc_request_iomux(MX50_PIN_EIM_OE, IOMUX_CONFIG_ALT1);
	mxc_request_iomux(MX50_PIN_EIM_RW, IOMUX_CONFIG_ALT1);
	/* Set as output */
	reg = readl(GPIO6_BASE_ADDR + 0x4);
	reg |= (7 << 24);
	writel(reg, GPIO6_BASE_ADDR + 0x4);
	
	reg = readl(GPIO1_BASE_ADDR + 0x4);
	reg |= (3 << 24);
	writel(reg, GPIO1_BASE_ADDR + 0x4);

	/* Set LED status */
	reg = readl(GPIO6_BASE_ADDR + 0x0);
	reg |= (3 << 24);
	writel(reg, GPIO6_BASE_ADDR + 0x0);

#ifdef BOOT_DIRTYLINE_FIX //[ try to fix dirty line when booting .
	// PWRALL pull down .
	mxc_request_iomux(MX50_PIN_EIM_CRE,
			IOMUX_CONFIG_ALT1);
	// set output .
	reg = readl(GPIO1_BASE_ADDR + 0x4);
	reg |= (0x1 << 27);
	writel(reg, GPIO1_BASE_ADDR + 0x4);
	// output low .
	reg = readl(GPIO1_BASE_ADDR + 0x0);
	reg &= ~(0x1 << 27);
	writel(reg, GPIO1_BASE_ADDR + 0x0);

	// EPD_PWRCTRL1
	mxc_request_iomux(MX50_PIN_EPDC_PWRCTRL1,
			IOMUX_CONFIG_ALT1);
	// set output .
	reg = readl(GPIO3_BASE_ADDR + 0x4);
	reg |= (0x1 << 30);
	writel(reg, GPIO3_BASE_ADDR + 0x4);
	// output low
	reg = readl(GPIO3_BASE_ADDR + 0x0);
	reg &= ~(0x1 << 30);
	writel(reg, GPIO3_BASE_ADDR + 0x0);

#endif //]BOOT_DIRTYLINE_FIX

#if 0
	reg = readl(GPIO1_BASE_ADDR + 0x0);
	reg |= (3 << 24);
	reg &= (~(1 << 24));
	writel(reg, GPIO1_BASE_ADDR + 0x0);
#else
	ON_LED(1);
#endif
	
	

#ifdef CONFIG_MFG
/* MFG firmware need reset usb to avoid host crash firstly */
#define USBCMD 0x140
	int val = readl(OTG_BASE_ADDR + USBCMD);
	val &= ~0x1; /*RS bit*/
	writel(val, OTG_BASE_ADDR + USBCMD);
#endif
	/* boot device */
	setup_boot_device();

	/* soc rev */
	setup_soc_rev();

	/* board rev */
	setup_board_rev();

	/* arch id for linux */
	setup_arch_id();

	/* boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	/* iomux for uart */
	setup_uart();

#ifdef CONFIG_MXC_FEC
	/* iomux for fec */
	setup_fec();
#endif

#ifdef CONFIG_NAND_GPMI
	setup_gpmi_nand();
#endif

#ifdef CONFIG_MXC_EPDC
	setup_epdc();
#endif


#ifdef CONFIG_I2C_MXC
	#if 0//def CONFIG_I2C_MULTI_BUS//[
	{
		int i;
		for(i=0;i<sizeof(gtI2C_platform_dataA)/sizeof(gtI2C_platform_dataA[0]);i++)
		{
			setup_i2c (gtI2C_platform_dataA[i].dwPort);
		}
	}
	#else //][! CONFIG_I2C_MULTI_BUS
	setup_i2c (CONFIG_SYS_I2C_PORT);
	#endif //]  CONFIG_I2C_MULTI_BUS
#endif

	 
	return 0;
}

#ifdef CONFIG_ANDROID_RECOVERY
struct reco_envs supported_reco_envs[BOOT_DEV_NUM] = {
	{
	 .cmd = NULL,
	 .args = NULL,
	 },
	{
	 .cmd = NULL,
	 .args = NULL,
	 },
	{
	 .cmd = NULL,
	 .args = NULL,
	 },
	{
	 .cmd = NULL,
	 .args = NULL,
	 },
	{
	 .cmd = NULL,
	 .args = NULL,
	 },
	{
	 .cmd = NULL,
	 .args = NULL,
	 },
	{
	 .cmd = CONFIG_ANDROID_RECOVERY_BOOTCMD_MMC,
	 .args = CONFIG_ANDROID_RECOVERY_BOOTARGS_MMC,
	 },
	{
	 .cmd = CONFIG_ANDROID_RECOVERY_BOOTCMD_MMC,
	 .args = CONFIG_ANDROID_RECOVERY_BOOTARGS_MMC,
	 },
	{
	 .cmd = NULL,
	 .args = NULL,
	 },
};

int check_recovery_cmd_file(void)
{
	disk_partition_t info;
	ulong part_length;
	int filelen;

	switch (get_boot_device()) {
	case MMC_BOOT:
	case SD_BOOT:
		{
			block_dev_desc_t *dev_desc = NULL;
			struct mmc *mmc = find_mmc_device(0);

			dev_desc = get_dev("mmc", 0);

			if (NULL == dev_desc) {
				puts("** Block device MMC 0 not supported\n");
				return 0;
			}

			mmc_init(mmc);

			if (get_partition_info(dev_desc,
					CONFIG_ANDROID_CACHE_PARTITION_MMC,
					&info)) {
				printf("** Bad partition %d **\n",
					CONFIG_ANDROID_CACHE_PARTITION_MMC);
				return 0;
			}

			part_length = ext2fs_set_blk_dev(dev_desc,
							CONFIG_ANDROID_CACHE_PARTITION_MMC);
			if (part_length == 0) {
				printf("** Bad partition - mmc 0:%d **\n",
					CONFIG_ANDROID_CACHE_PARTITION_MMC);
				ext2fs_close();
				return 0;
			}

			if (!ext2fs_mount(part_length)) {
				printf("** Bad ext2 partition or disk - mmc 0:%d **\n",
					CONFIG_ANDROID_CACHE_PARTITION_MMC);
				ext2fs_close();
				return 0;
			}

			filelen = ext2fs_open(CONFIG_ANDROID_RECOVERY_CMD_FILE);

			ext2fs_close();
		}
		break;
	case NAND_BOOT:
		return 0;
		break;
	case SPI_NOR_BOOT:
		return 0;
		break;
	case UNKNOWN_BOOT:
	default:
		return 0;
		break;
	}

	return (filelen > 0) ? 1 : 0;

}
#endif

extern void mx50_overclock(u32 databahn_base, u32 ccm_base, u32 pll1_base, u32 ddr_div_pll);
extern void mxc_dump_clocks(void);

#define ENSETx  0x100
#define IRAM_BASE_FREE_ADDR	(IRAM_BASE_ADDR + 0x6000)
void start_overclock(void)
{
	void (*pfn_overclock)(u32 databahn_base, u32 ccm_base, u32 pll1_base, u32 ddr_div_pll);
	unsigned int enset[4];
	int i;

	printf("Relock PLL1 to 1GHz ...\n");

	pfn_overclock = (void *)IRAM_BASE_FREE_ADDR;
	memcpy(pfn_overclock, mx50_overclock, SZ_4K);

	// disable all interrupts in TZIC
	for (i = 0; i < 4; i++) {
		enset[i] = readl(TZIC_BASE_ADDR + ENSETx + i*4);
		writel(0, TZIC_BASE_ADDR + ENSETx + i*4);
	}

#if defined(CONFIG_LPDDR2) || defined(CONFIG_DDR2)
	pfn_overclock(DATABAHN_BASE_ADDR, CCM_BASE_ADDR, PLL1_BASE_ADDR, 4);
#else // CONFIG_MDDR
	pfn_overclock(DATABAHN_BASE_ADDR, CCM_BASE_ADDR, PLL1_BASE_ADDR, 5);
#endif

	// restore interrupts in TZIC
	for (i = 0; i < 4; i++) {
		writel(enset[i], TZIC_BASE_ADDR + ENSETx + i*4);
	}

	mxc_dump_clocks();
}

int board_late_init(void)
{
#ifdef CONFIG_IMX_CSPI
	setup_power();
#endif
	run_command("load_ntxbins", 0);//

	_init_tps65185_power(0,0);

	if(IsMaxCpuFreq1GHz())
		start_overclock();
	
	return 0;
}

int checkboard(void)
{
#if defined(CONFIG_MX50_RDP)
	printf("Board: MX50 RDP board\n");
#elif defined(CONFIG_MX50_RD3)
	printf("Board: MX50 RD3 board\n");
#elif defined(CONFIG_MX50_ARM2)
	printf("Board: MX50 ARM2 board\n");
#else
#	error "Unsupported board!"
#endif

	printf("Boot Reason: [");

	switch (__REG(SRC_BASE_ADDR + 0x8)) {
	case 0x0001:
		printf("POR");
		break;
	case 0x0009:
		printf("RST");
		break;
	case 0x0010:
	case 0x0011:
		printf("WDOG");
		break;
	default:
		printf("unknown");
	}
	printf("]\n");

	printf("Boot Device: ");
	switch (get_boot_device()) {
	case WEIM_NOR_BOOT:
		printf("NOR\n");
		break;
	case ONE_NAND_BOOT:
		printf("ONE NAND\n");
		break;
	case PATA_BOOT:
		printf("PATA\n");
		break;
	case SATA_BOOT:
		printf("SATA\n");
		break;
	case I2C_BOOT:
		printf("I2C\n");
		break;
	case SPI_NOR_BOOT:
		printf("SPI NOR\n");
		break;
	case SD_BOOT:
		printf("SD\n");
		break;
	case MMC_BOOT:
		printf("MMC\n");
		break;
	case NAND_BOOT:
		printf("NAND\n");
		break;
	case UNKNOWN_BOOT:
	default:
		printf("UNKNOWN\n");
		break;
	}

	return 0;
}

int init_pwr_i2c_function(int iSetAsFunc)
{
	return 0;
}

int esd_wp_read(void)
{
	return 1;
}

int _sd_cd_status (void)
{
	unsigned int reg;
	mxc_request_iomux(MX50_PIN_SD2_CD, IOMUX_CONFIG_ALT1);
	mxc_iomux_set_pad(MX50_PIN_SD2_CD, PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
	/* Set as input */
	reg = readl(GPIO5_BASE_ADDR + 0x4);
	reg &= ~(1 << 17);
	writel(reg, GPIO5_BASE_ADDR + 0x4);
	
	return (readl(GPIO5_BASE_ADDR + 0x0)&(1<<17))?0:1;
}

int gpio_out_init(GPIO_OUT *I_pt_gpio_out)
{
	int iRet = 0 ;
	unsigned int reg;
	u32 dwGPIO_dir_addr=0;
	u32 dwGPIO_data_addr=0;
	
	if(!I_pt_gpio_out) {
		printf("%s(%d) : error parameter ! null ptr !\n",__FUNCTION__,__LINE__);
		return -1;
	}
	
	
	mxc_request_iomux(I_pt_gpio_out->PIN, I_pt_gpio_out->PIN_CFG);
	mxc_iomux_set_pad(I_pt_gpio_out->PIN, I_pt_gpio_out->PIN_PAD_CFG);
	
	switch(I_pt_gpio_out->GPIO_Grp) {
	case 1:
		dwGPIO_dir_addr = GPIO1_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO1_BASE_ADDR + 0x0;
		break;
	case 2:
		dwGPIO_dir_addr = GPIO2_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO2_BASE_ADDR + 0x0;
		break;
	case 3:
		dwGPIO_dir_addr = GPIO3_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO3_BASE_ADDR + 0x0;
		break;
	case 4:
		dwGPIO_dir_addr = GPIO4_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO4_BASE_ADDR + 0x0;
		break;
	case 5:
		dwGPIO_dir_addr = GPIO5_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO5_BASE_ADDR + 0x0;
		break;
	case 6:
		dwGPIO_dir_addr = GPIO6_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		break;
	case 7:
		dwGPIO_dir_addr = GPIO7_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		break;
	default :
		printf("%s():%s [ERROR] GPIO group number error (%hd)!!\n",
			__FUNCTION__,I_pt_gpio_out->pszName,
			I_pt_gpio_out->GPIO_Grp);
		return -2;
	}
	
	/* Set as output */
	reg = readl(dwGPIO_dir_addr);
	reg |= (u32)(1 << I_pt_gpio_out->GPIO_Num);
	writel(reg, dwGPIO_dir_addr);

	// set output value .
	reg = readl(dwGPIO_data_addr);
	if(1==I_pt_gpio_out->iOutputValue) {
		reg |= (u32)(1 << I_pt_gpio_out->GPIO_Num);
	}
	else if(0==I_pt_gpio_out->iOutputValue){
		reg &= ~((u32)(1 << I_pt_gpio_out->GPIO_Num));
	}
	writel(reg, dwGPIO_data_addr);
	
	++I_pt_gpio_out->iIsInited;
	
	return iRet;
}

int gpio_output(GPIO_OUT *I_pt_gpio_out,int iOutVal) 
{
	unsigned int reg;
	int iRet;
	int iChk;
	u32 dwGPIO_data_addr=0;
	
	if(!I_pt_gpio_out) {
		printf("%s(%d) : error parameter ! null ptr !\n",__FUNCTION__,__LINE__);
		return -1;
	}
	
	if(0==I_pt_gpio_out->iIsInited) {
		iChk = gpio_out_init(I_pt_gpio_out);
		if(iChk<0) {
			return iChk;
		}
		udelay(100);
	}
	
	switch(I_pt_gpio_out->GPIO_Grp) {
	case 1:
		dwGPIO_data_addr = GPIO1_BASE_ADDR + 0x0;
		break;
	case 2:
		dwGPIO_data_addr = GPIO2_BASE_ADDR + 0x0;
		break;
	case 3:
		dwGPIO_data_addr = GPIO3_BASE_ADDR + 0x0;
		break;
	case 4:
		dwGPIO_data_addr = GPIO4_BASE_ADDR + 0x0;
		break;
	case 5:
		dwGPIO_data_addr = GPIO5_BASE_ADDR + 0x0;
		break;
	case 6:
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		break;
	case 7:
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		break;
	default :
		printf("%s():%s [ERROR] GPIO group number error (%hd)!!\n",
			__FUNCTION__,I_pt_gpio_out->pszName,
			I_pt_gpio_out->GPIO_Grp);
		return -2;
	}
	
	reg = readl(dwGPIO_data_addr);
	if(1==iOutVal) {
		reg |= (u32)(1 << I_pt_gpio_out->GPIO_Num);
	}
	else if(0==iOutVal){
		reg &= ~((u32)(1 << I_pt_gpio_out->GPIO_Num));
	}
	writel(reg, dwGPIO_data_addr);
	
	
	return iRet;
}



int gpio_key_btn_init(GPIO_KEY_BTN *I_pt_gpio_key_btn)
{
	int iRet = 0 ;
	unsigned int reg;
	u32 dwGPIO_dir_addr=0;
	
	if(!I_pt_gpio_key_btn) {
		printf("%s(%d) : error parameter ! null ptr !\n",__FUNCTION__,__LINE__);
		return -1;
	}
	
	
	mxc_request_iomux(I_pt_gpio_key_btn->PIN, I_pt_gpio_key_btn->PIN_CFG);
	mxc_iomux_set_pad(I_pt_gpio_key_btn->PIN, I_pt_gpio_key_btn->PIN_PAD_CFG);
	
	switch(I_pt_gpio_key_btn->GPIO_Grp) {
	case 1:
		dwGPIO_dir_addr = GPIO1_BASE_ADDR + 0x4;
		break;
	case 2:
		dwGPIO_dir_addr = GPIO2_BASE_ADDR + 0x4;
		break;
	case 3:
		dwGPIO_dir_addr = GPIO3_BASE_ADDR + 0x4;
		break;
	case 4:
		dwGPIO_dir_addr = GPIO4_BASE_ADDR + 0x4;
		break;
	case 5:
		dwGPIO_dir_addr = GPIO5_BASE_ADDR + 0x4;
		break;
	case 6:
		dwGPIO_dir_addr = GPIO6_BASE_ADDR + 0x4;
		break;
	case 7:
		dwGPIO_dir_addr = GPIO7_BASE_ADDR + 0x4;
		break;
	default :
		printf("%s():%s [ERROR] GPIO group number error (%hd)!!\n",
			__FUNCTION__,I_pt_gpio_key_btn->pszReportName,
			I_pt_gpio_key_btn->GPIO_Grp);
		return -2;
	}
	
	/* Set as input */
	reg = readl(dwGPIO_dir_addr);
	reg &= ~(1 << I_pt_gpio_key_btn->GPIO_Num);
	writel(reg, dwGPIO_dir_addr);
	
	++I_pt_gpio_key_btn->iIsInited;
	
	return iRet;
}


int gpio_key_btn_is_down(GPIO_KEY_BTN *I_pt_gpio_key_btn) 
{
	int iRet;
	int iChk;
	u32 dwGPIO_data_addr=0;
	
	if(!I_pt_gpio_key_btn) {
		printf("%s(%d) : error parameter ! null ptr !\n",__FUNCTION__,__LINE__);
		return -1;
	}
	
	if(0==I_pt_gpio_key_btn->iIsInited) {
		iChk = gpio_key_btn_init(I_pt_gpio_key_btn);
		if(iChk<0) {
			return iChk;
		}
		udelay(100);
	}
	
	switch(I_pt_gpio_key_btn->GPIO_Grp) {
	case 1:
		dwGPIO_data_addr = GPIO1_BASE_ADDR + 0x0;
		break;
	case 2:
		dwGPIO_data_addr = GPIO2_BASE_ADDR + 0x0;
		break;
	case 3:
		dwGPIO_data_addr = GPIO3_BASE_ADDR + 0x0;
		break;
	case 4:
		dwGPIO_data_addr = GPIO4_BASE_ADDR + 0x0;
		break;
	case 5:
		dwGPIO_data_addr = GPIO5_BASE_ADDR + 0x0;
		break;
	case 6:
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		break;
	case 7:
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		break;
	default :
		printf("%s():%s [ERROR] GPIO group number error (%hd)!!\n",
			__FUNCTION__,I_pt_gpio_key_btn->pszReportName,
			I_pt_gpio_key_btn->GPIO_Grp);
		return -2;
	}
	
	iChk = (readl(dwGPIO_data_addr)&(1<<I_pt_gpio_key_btn->GPIO_Num))?1:0;
	if(iChk==I_pt_gpio_key_btn->DownValue) {
		iRet = 1;
	}
	else {
		iRet = 0;
	}
	
	return iRet;
}


int _hallsensor_status (void)
{
	int iChk;
	if(gptNtxHwCfg&&0!=gptNtxHwCfg->m_val.bHallSensor) {
		if(gptNtxHwCfg->m_val.bPCB==35)
			iChk = gpio_key_btn_init(&gt_ntx_gpio_5_15_hallsensor_key);
		else if(gptNtxHwCfg->m_val.bPCB==41)
			iChk = gpio_key_btn_init(&gt_ntx_gpio_5_13_hallsensor_key);
		else
			iChk = gpio_key_btn_init(&gt_ntx_gpio_hallsensor_key);
		
		if(iChk<0) {
			return iChk;
		}
		
		if(gptNtxHwCfg->m_val.bPCB==35)
			return gpio_key_btn_is_down(&gt_ntx_gpio_5_15_hallsensor_key);
		else if(gptNtxHwCfg->m_val.bPCB==41)
			return gpio_key_btn_is_down(&gt_ntx_gpio_5_13_hallsensor_key);
		else
			return gpio_key_btn_is_down(&gt_ntx_gpio_hallsensor_key);
	}
	else {
		return -1;
	}
}

int ntx_gpio_key_is_home_down(void)
{
	int iChk;
	
	//iChk = gpio_key_btn_init(&gt_ntx_gpio_hallsensor_key);
	//if(iChk<0) {
	//	return iChk;
	//}
		
	return gpio_key_btn_is_down(&gt_ntx_gpio_home_key);
}



int _get_pcba_id (void)
{
	static int g_pcba_id;
	unsigned int reg;

	//return 0x4;	
	if (g_pcba_id)
		return g_pcba_id;
	
	mxc_request_iomux(MX50_PIN_UART3_TXD, IOMUX_CONFIG_ALT1);
	mxc_request_iomux(MX50_PIN_UART3_RXD, IOMUX_CONFIG_ALT1);
	mxc_request_iomux(MX50_PIN_UART4_RXD, IOMUX_CONFIG_ALT1);
	mxc_request_iomux(MX50_PIN_SD2_WP, IOMUX_CONFIG_ALT1);
	mxc_iomux_set_pad(MX50_PIN_UART3_TXD, 0);
	mxc_iomux_set_pad(MX50_PIN_UART3_RXD, 0);
	mxc_iomux_set_pad(MX50_PIN_UART4_RXD, 0);
	mxc_iomux_set_pad(MX50_PIN_SD2_WP, 0);
	/* Set as input */
	reg = readl(GPIO6_BASE_ADDR + 0x4);
	reg &= ~(0x0B << 14);
	writel(reg, GPIO6_BASE_ADDR + 0x4);

	reg = readl(GPIO5_BASE_ADDR + 0x4);
	reg &= ~(0x01 << 16);
	writel(reg, GPIO5_BASE_ADDR + 0x4);
#if 0 //Read hardware id	

	readl(GPIO6_BASE_ADDR + 0x0);// dummy .
	readl(GPIO5_BASE_ADDR + 0x0);// dummy .
	udelay(10000);

	if(readl(GPIO6_BASE_ADDR + 0x0)&(1<<14)) {
		g_pcba_id |= 1;
	}
	if(readl(GPIO6_BASE_ADDR + 0x0)&(1<<15)) {
		g_pcba_id |= 2;
	}
	if(readl(GPIO6_BASE_ADDR + 0x0)&(1<<17)) {
		g_pcba_id |= 4;
	}
	if( !(readl(GPIO5_BASE_ADDR + 0x0)& (1<<16)) ) {
		g_pcba_id |= 8;
	}
#else   //Read hwconfig
	switch(gptNtxHwCfg->m_val.bPCB)
	{
		case 12: //E60610
		case 20: //E60610C
		case 21: //E60610D
			g_pcba_id = 1;
			break;
		case 15: //E60620
			g_pcba_id = 4;
			break;
		case 16: //E60630
			g_pcba_id = 6;
			break;
		case 18: //E50600
			g_pcba_id = 2;
			break;
		case 19: //E60680
			g_pcba_id = 3;
			break;
		case 22: //E606A0
			g_pcba_id = 10;
			break;
		case 23: //E60670
			g_pcba_id = 5;
			break;
		case 24: //E606B0
			g_pcba_id = 14;
			break;
		case 27: //E50610
			g_pcba_id = 9;
			break;
		case 28: //E606C0
			g_pcba_id = 11;
			break;
		default:
			g_pcba_id = gptNtxHwCfg->m_val.bPCB;
//			printf ("[%s-%d] undefined PCBA ID\n",__func__,__LINE__);
			break;	
	}
#endif
	printf ("PCB ID is %d\n",g_pcba_id);

	return g_pcba_id;
}

int _get_sd_number (void)
{
	static int g_sd_number=-1;
	unsigned int reg;

	if (g_sd_number >= 0)
		return g_sd_number;

	mxc_request_iomux(MX50_PIN_EIM_WAIT, IOMUX_CONFIG_ALT1);
	mxc_request_iomux(MX50_PIN_EIM_EB1, IOMUX_CONFIG_ALT1);

	mxc_iomux_set_pad(MX50_PIN_EIM_WAIT, 0);
	mxc_iomux_set_pad(MX50_PIN_EIM_EB1, 0);
	
	/* Set as input */
	reg = readl(GPIO1_BASE_ADDR + 0x4);
	reg &= ~(0x03 << 20);
	writel(reg, GPIO1_BASE_ADDR + 0x4);
	
	readl(GPIO1_BASE_ADDR + 0x0);// dummy .
	udelay(10000);

	g_sd_number = (readl(GPIO1_BASE_ADDR + 0x0)>>20) & (0x3);
	printf("[%s] g_sd_number:%d\n",__FUNCTION__,g_sd_number);
	return g_sd_number;
}

int _power_key_status (void)
{
	unsigned int reg;
	mxc_request_iomux(MX50_PIN_CSPI_MISO, IOMUX_CONFIG_ALT1);
	mxc_iomux_set_pad(MX50_PIN_CSPI_MISO, PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
	
	/* Set as input */
	reg = readl(GPIO4_BASE_ADDR + 0x4);
	reg &= ~(1 << 10);
	writel(reg, GPIO4_BASE_ADDR + 0x4);
	
	readl(GPIO4_BASE_ADDR + 0x0);// dummy .
	udelay(10000);
	if ((0x06 == _get_pcba_id()) || (0x02 == _get_pcba_id()))
		return (readl(GPIO4_BASE_ADDR + 0x0)&(1<<10))?1:0;
	else
		return (readl(GPIO4_BASE_ADDR + 0x0)&(1<<10))?0:1;
}

#if defined(CONFIG_MXC_KPD)
int setup_mxc_kpd(void)
{
	mxc_request_iomux(MX50_PIN_KEY_COL0, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_ROW0, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_COL1, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_ROW1, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_COL2, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_ROW2, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_COL3, IOMUX_CONFIG_ALT0);
	mxc_request_iomux(MX50_PIN_KEY_ROW3, IOMUX_CONFIG_ALT0);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW0, PAD_CTL_HYS_ENABLE|PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW1, PAD_CTL_HYS_ENABLE|PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW2, PAD_CTL_HYS_ENABLE|PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
	mxc_iomux_set_pad(MX50_PIN_KEY_ROW3, PAD_CTL_HYS_ENABLE|PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL0, 0);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL1, 0);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL2, 0);
	mxc_iomux_set_pad(MX50_PIN_KEY_COL3, 0);

	return 0;
}
#endif


#ifdef CONFIG_I2C_MULTI_BUS//[

unsigned int i2c_get_bus_num(void) 
{
	return gdwBusNum;
}

int i2c_set_bus_num(unsigned int dwBusNum)
{
	if(dwBusNum<sizeof(gtI2C_platform_dataA)/sizeof(gtI2C_platform_dataA[0])) {
		
		gdwBusNum=dwBusNum;

		if(0==gtI2C_platform_dataA[dwBusNum].iIsInited) {
			setup_i2c (gtI2C_platform_dataA[dwBusNum].dwPort);
			i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
			gtI2C_platform_dataA[dwBusNum].iIsInited = 1;
		}

		return 0;
	}
	else {
		printf("%s(%d) : parameter error !!\n",__FUNCTION__,(int)dwBusNum);
		return -1;
	}
}

#endif //CONFIG_I2C_MULTI_BUS

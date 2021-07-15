/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
 *
 * Configuration settings for the MX50-RDP Freescale board.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <asm/arch/mx50.h>

 /* High Level Configuration Options */
#define CONFIG_MXC
#define CONFIG_MX50
#define CONFIG_MX50_RDP
#define CONFIG_DDR2
#define CONFIG_MX50_E606XX
#define CONFIG_FLASH_HEADER
#define CONFIG_FLASH_HEADER_OFFSET 0x400

#define CONFIG_SKIP_RELOCATE_UBOOT

/*
#define CONFIG_ARCH_CPU_INIT
#define CONFIG_ARCH_MMU
*/

#define CONFIG_MX50_HCLK_FREQ	24000000
#define CONFIG_SYS_PLL2_FREQ    400
#define CONFIG_SYS_AHB_PODF     2
#define CONFIG_SYS_AXIA_PODF    0
#define CONFIG_SYS_AXIB_PODF    1

#define CONFIG_DISPLAY_CPUINFO
#define CONFIG_DISPLAY_BOARDINFO

#define CONFIG_SYS_64BIT_VSPRINTF

#define BOARD_LATE_INIT
/*
 * Disabled for now due to build problems under Debian and a significant
 * increase in the final file size: 144260 vs. 109536 Bytes.
 */

#define CONFIG_CMDLINE_TAG		1	/* enable passing of ATAGs */
#define CONFIG_REVISION_TAG		1
#define CONFIG_SETUP_MEMORY_TAGS	1
#define CONFIG_INITRD_TAG		1

/*
 * Size of malloc() pool
 */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE + 2 * 1024 * 1024)
/* size in bytes reserved for initial data */
#define CONFIG_SYS_GBL_DATA_SIZE	128

/*
 * Hardware drivers
 */
#define CONFIG_MXC_UART
#ifdef CONFIG_MX50_E606XX	// Joseph 20110502
#define CONFIG_UART_BASE_ADDR	UART2_BASE_ADDR
#else
#define CONFIG_UART_BASE_ADDR	UART1_BASE_ADDR
#endif

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_CONS_INDEX		1
#define CONFIG_BAUDRATE			115200
#define CONFIG_SYS_BAUDRATE_TABLE	{9600, 19200, 38400, 57600, 115200}

/***********************************************************
 * Command definition
 ***********************************************************/

//Joseph 20110531	#include <config_cmd_default.h>

#define CONFIG_CMD_BDI		/* bdinfo			*/
#define CONFIG_CMD_BOOTD	/* bootd			*/
#define CONFIG_CMD_CONSOLE	/* coninfo			*/
#define CONFIG_CMD_ECHO		/* echo arguments		*/
#define CONFIG_CMD_FPGA		/* FPGA configuration Support	*/
#define CONFIG_CMD_IMI		/* iminfo			*/
#define CONFIG_CMD_ITEST	/* Integer (and string) test	*/
#define CONFIG_CMD_LOADB	/* loadb			*/
#define CONFIG_CMD_LOADS	/* loads			*/
#define CONFIG_CMD_MEMORY	/* md mm nm mw cp cmp crc base loop mtest */
#define CONFIG_CMD_MISC		/* Misc functions like sleep etc*/
#define CONFIG_CMD_RUN		/* run command in env variable	*/
#define CONFIG_CMD_SAVEENV	/* saveenv			*/
#define CONFIG_CMD_SETGETDCR	/* DCR support on 4xx		*/
#define CONFIG_CMD_SOURCE	/* "source" command support	*/
#define CONFIG_CMD_XIMG		/* Load part of Multi Image	*/


// Joseph 20110531	#define CONFIG_CMD_PING
// Joseph 20110531	#define CONFIG_CMD_DHCP
#define CONFIG_CMD_MII
// Joseph 20110531	#define CONFIG_CMD_NET
// Joseph 20110531	#define CONFIG_NET_RETRY_COUNT  100
// Joseph 20110531	#define CONFIG_NET_MULTI 1
// Joseph 20110531	#define CONFIG_BOOTP_SUBNETMASK
// Joseph 20110531	#define CONFIG_BOOTP_GATEWAY
// Joseph 20110531	#define CONFIG_BOOTP_DNS

#define CONFIG_CMD_MMC
//#define CONFIG_CMD_ENV

/*#define CONFIG_CMD */
#define CONFIG_REF_CLK_FREQ CONFIG_MX50_HCLK_FREQ

#undef CONFIG_CMD_IMLS

#define CONFIG_BOOTDELAY	1

#define CONFIG_PRIME	"FEC0"

#define CONFIG_LOADADDR		0x70800000	/* loadaddr env var */
#define CONFIG_RD_LOADADDR	(CONFIG_LOADADDR + 0x300000)
//#define CONFIG_ENV_IS_EMBEDDED

#ifdef CONFIG_MX50_E606XX
#if 0
#define	CONFIG_EXTRA_ENV_SETTINGS					\
		"uboot=u-boot.bin\0"			\
		"kernel=uImage\0"				\
		"bootargs_base=setenv bootargs noinitrd console=ttymxc0 rootwait rw no_console_suspend ${bootargs}\0"\
		"rootdevESD1=/dev/mmcblk1p1\0" \
		"rootdevESD2=/dev/mmcblk1p2\0" \
		"rootdevRecovery=/dev/mmcblk0p2\0" \
		"rootdevNormal=/dev/mmcblk0p1\0" \
		"rootdev=${rootdevNormal}\0" \
		"rootfstype=ext3\0" \
		"bootargs_mmc=setenv bootargs ${bootargs} root=${rootdev} rootfstype=${rootfstype}\0" \
		"bootcmd_mmc=run bootargs_base bootargs_mmc;load_ntxkernel; bootm\0"   \
		"bootcmd=run bootcmd_mmc\0" 

#else	
#define	CONFIG_EXTRA_ENV_SETTINGS					\
		"uboot=u-boot.bin\0"			\
		"kernel=uImage\0"				\
		"bootargs_base=setenv bootargs console=ttymxc0,115200 rootwait rw no_console_suspend lpj=3997696\0"\
		"bootargs_mmc=setenv bootargs ${bootargs}\0" \
		"bootcmd_mmc=run bootargs_base bootargs_mmc;load_ntxkernel; bootm\0"   \
		"bootargs_SD=setenv bootargs ${bootargs}\0" \
		"bootcmd_SD=run bootargs_base bootargs_SD;load_ntxkernel; bootm\0"   \
		"bootargs_recovery=setenv bootargs ${bootargs}\0" \
		"bootcmd_recovery=run bootargs_base bootargs_recovery;load_ntxkernel; bootm\0"   \
		"bootcmd=run bootcmd_mmc\0" \
		"KRN_SDNUM_SD=1\0" \
		"KRN_SDNUM_Recovery=0\0" \
		"verify=no"
#endif

#else
#define	CONFIG_EXTRA_ENV_SETTINGS					\
		"netdev=eth0\0"						\
		"ethprime=FEC0\0"					\
		"uboot=u-boot.bin\0"			\
		"kernel=uImage\0"				\
		"nfsroot=/opt/eldk/arm\0"				\
		"bootargs_base=setenv bootargs console=ttymxc0,115200\0"\
		"bootargs_nfs=setenv bootargs ${bootargs} root=/dev/nfs "\
			"ip=dhcp nfsroot=${serverip}:${nfsroot},v3,tcp\0"\
		"bootcmd_net=run bootargs_base bootargs_nfs; "		\
			"tftpboot ${loadaddr} ${kernel}; bootm\0"	\
		"bootargs_mmc=setenv bootargs ${bootargs} ip=dhcp "     \
			"root=/dev/mmcblk0p2 rootwait\0"                \
		"bootcmd_mmc=run bootargs_base bootargs_mmc; bootm\0"   \
		"bootcmd=run bootcmd_mmc\0"                             \
		
#endif


#define CONFIG_ARP_TIMEOUT	200UL

/*
 * Miscellaneous configurable options
 */
#define CONFIG_SYS_LONGHELP		/* undef to save memory */
#define CONFIG_SYS_PROMPT		"eBR-1A # "
#define CONFIG_AUTO_COMPLETE
#define CONFIG_SYS_CBSIZE		256	/* Console I/O Buffer Size */
/* Print Buffer Size */
#define CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_MAXARGS	16	/* max number of command args */
#define CONFIG_SYS_BARGSIZE CONFIG_SYS_CBSIZE /* Boot Argument Buffer Size */

#define CONFIG_SYS_MEMTEST_START	0	/* memtest works on */
#define CONFIG_SYS_MEMTEST_END		0x10000

#undef	CONFIG_SYS_CLKS_IN_HZ		/* everything, incl board info, in Hz */

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR

#define CONFIG_SYS_HZ				1000

#define CONFIG_CMDLINE_EDITING	1

#define CONFIG_FEC0_IOBASE	FEC_BASE_ADDR
#define CONFIG_FEC0_PINMUX	-1
#define CONFIG_FEC0_PHY_ADDR	-1
#define CONFIG_FEC0_MIIBASE	-1

//#define CONFIG_GET_FEC_MAC_ADDR_FROM_IIM

//#define CONFIG_MXC_FEC
#define CONFIG_MII
#define CONFIG_MII_GASKET
#define CONFIG_DISCOVER_PHY

/*
 * DDR ZQ calibration
 */
#define CONFIG_ZQ_CALIB

/*
 * I2C Configs
 */
#define CONFIG_CMD_I2C          1

#ifdef CONFIG_CMD_I2C
	#define CONFIG_HARD_I2C         1
	#define CONFIG_I2C_MXC          1
	#define CONFIG_SYS_I2C_PORT             I2C3_BASE_ADDR
	#define CONFIG_SYS_I2C_SPEED            100000
	#define CONFIG_SYS_I2C_SLAVE            0xfe
#endif


/*
 * SPI Configs
 */
#if 0
#define CONFIG_FSL_SF		1
#define CONFIG_CMD_SPI
#define CONFIG_CMD_SF
#define CONFIG_SPI_FLASH_IMX_ATMEL	1
#define CONFIG_SPI_FLASH_CS	1
#define CONFIG_IMX_CSPI
#define IMX_CSPI_VER_0_7        1
#define MAX_SPI_BYTES		(8 * 4)
#define CONFIG_IMX_SPI_PMIC
#define CONFIG_IMX_SPI_PMIC_CS 0
#endif

/*
 * MMC Configs
 */
#ifdef CONFIG_CMD_MMC
	#define CONFIG_MMC				1
	#define CONFIG_GENERIC_MMC
	#define CONFIG_IMX_MMC
	#define CONFIG_SYS_FSL_ESDHC_NUM        2
	#define CONFIG_SYS_FSL_ESDHC_ADDR       0
	#define CONFIG_SYS_MMC_ENV_DEV  0
	#define CONFIG_DOS_PARTITION	1
	#define CONFIG_CMD_FAT		1
	#define CONFIG_CMD_EXT2		1

	/* detect whether ESDHC1, ESDHC2, or ESDHC3 is boot device */
	#define CONFIG_DYNAMIC_MMC_DEVNO

	#define CONFIG_BOOT_PARTITION_ACCESS
	#define CONFIG_EMMC_DDR_MODE

	/* Indicate to esdhc driver which ports support 8-bit data */
	#define CONFIG_MMC_8BIT_PORTS		0x6   /* ports 1 and 2 */
#endif

/*
 * GPMI Nand Configs
 */
//#define CONFIG_CMD_NAND

#ifdef CONFIG_CMD_NAND
	#define CONFIG_NAND_GPMI
	#define CONFIG_GPMI_NFC_SWAP_BLOCK_MARK
	#define CONFIG_GPMI_NFC_V2

	#define CONFIG_GPMI_REG_BASE	GPMI_BASE_ADDR
	#define CONFIG_BCH_REG_BASE	BCH_BASE_ADDR

	#define NAND_MAX_CHIPS		8
	#define CONFIG_SYS_NAND_BASE		0x40000000
	#define CONFIG_SYS_MAX_NAND_DEVICE	1
#endif

/*
 * APBH DMA Configs
 */
#define CONFIG_APBH_DMA

#ifdef CONFIG_APBH_DMA
	#define CONFIG_APBH_DMA_V2
	#define CONFIG_MXS_DMA_REG_BASE	ABPHDMA_BASE_ADDR
#endif

/*-----------------------------------------------------------------------
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128 * 1024)	/* regular stack */

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1
#define PHYS_SDRAM_1		CSD0_BASE_ADDR
#ifdef CONFIG_MX50_E606XX	// Joseph 20110502
#define PHYS_SDRAM_1_SIZE	(128 * 1024 * 1024)
#else
#define PHYS_SDRAM_1_SIZE	(512 * 1024 * 1024)
#endif
#define iomem_valid_addr(addr, size) \
	(addr >= PHYS_SDRAM_1 && addr <= (PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE))

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */
#define CONFIG_SYS_NO_FLASH

/* Monitor at beginning of flash */
#define CONFIG_FSL_ENV_IN_MMC

#define CONFIG_ENV_SECT_SIZE    (128 * 1024)
#define CONFIG_ENV_SIZE         CONFIG_ENV_SECT_SIZE

#if defined(CONFIG_FSL_ENV_IN_NAND)
	#define CONFIG_ENV_IS_IN_NAND 1
	#define CONFIG_ENV_OFFSET	0x100000
#elif defined(CONFIG_FSL_ENV_IN_MMC)
	#define CONFIG_ENV_IS_IN_MMC	1
	#define CONFIG_ENV_OFFSET	(768 * 1024)
#elif defined(CONFIG_FSL_ENV_IN_SF)
	#define CONFIG_ENV_IS_IN_SPI_FLASH	1
	#define CONFIG_ENV_SPI_CS		1
	#define CONFIG_ENV_OFFSET       (768 * 1024)
#else
	#define CONFIG_ENV_IS_NOWHERE	1
#endif
#endif				/* __CONFIG_H */

#define CONFIG_MXC_KPD
#define CONFIG_MXC_KEYMAPPING \
	{	\
		0, 1, 2, 3, \
		4, 5, 6, 7, \
		8, 9, 10, 11, \
		12, 13, 14, 15,\
	}
#define CONFIG_MXC_KPD_COLMAX 4
#define CONFIG_MXC_KPD_ROWMAX 4


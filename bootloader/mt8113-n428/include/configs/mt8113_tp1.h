/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Configuration for MediaTek MT8113 P1 SoC
 *
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Wenmin Tu <wenmin.tu@mediatek.com>
 */

#ifndef __MT8113_TP1_H
#define __MT8113_TP1_H

#include <linux/sizes.h>

#define CONFIG_ENV_SIZE				SZ_4K

/* Machine ID */
#define CONFIG_SYS_NONCACHED_MEMORY		SZ_1M

#define CONFIG_CPU_ARMV8

#define COUNTER_FREQUENCY			13000000

/* DRAM definition */
#define CONFIG_SYS_SDRAM_BASE			0x40000000
#define CONFIG_SYS_SDRAM_SIZE			0x20000000

#define CONFIG_SYS_LOAD_ADDR			0x41000000
#define CONFIG_LOADADDR				CONFIG_SYS_LOAD_ADDR

#define CONFIG_SYS_MALLOC_LEN			SZ_32M
#define CONFIG_SYS_BOOTM_LEN			SZ_64M

/* Uboot definition */
#define CONFIG_SYS_UBOOT_START			CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_INIT_SP_ADDR			(CONFIG_SYS_TEXT_BASE + \
						SZ_2M - \
						GENERATED_GBL_DATA_SIZE)

/* ENV Setting */
#if defined(CONFIG_MMC_MTK)
#define CONFIG_SYS_MMC_ENV_DEV			0
#define CONFIG_ENV_OFFSET			0x1e00000
#define CONFIG_ENV_OVERWRITE

/* MMC offset in block unit,and block size is 0x200 */
#define ENV_GET_IMG_SIZE \
	"get_bootimg_size=fdt addr ${loadaddr}" \
	";fdt header get bootimg_size totalsize" \
	";setexpr bootimg_size ${bootimg_size} + FFF" \
	";setexpr bootimg_size ${bootimg_size} / 1000" \
	";setexpr bootimg_size ${bootimg_size} * 1000\0"

#define ENV_INIT_BOOT_INFO \
	ENV_GET_IMG_SIZE \
	"init_boot_info=part start mmc 0 boot_a boot_start_block" \
	";mmc read ${loadaddr} ${boot_start_block} 1" \
	";run get_bootimg_size" \
	";setexpr bootimg_block ${bootimg_size} / 200\0"

#define ENV_BOOT_READ_IMAGE \
	ENV_INIT_BOOT_INFO \
	"boot_rd_img=mmc dev 0" \
	";run init_boot_info" \
	";mmc read ${loadaddr} ${boot_start_block} ${bootimg_block}" \
	";iminfo ${loadaddr}\0"
#endif

/* Console configuration */
#define ENV_DEVICE_SETTINGS \
	"stdin=serial\0" \
	"stdout=serial\0" \
	"stderr=serial\0"

#define ENV_BOOT_CMD \
	"mtk_boot=run boot_rd_img;bootm;\0"

#define ENV_FASTBOOT \
	"serial#=1234567890ABCDEF\0" \
	"board=mt8110\0"

#if 1
#define ENV_BOOT_DEV \
	"rootdev=PARTUUID=936e1662-2b63-435d-859d-1b9e5a838335\0" 
#else 
#define ENV_BOOT_DEV \
	"rootdev=/dev/mmcblk0p10\0" 
#endif

#define ENV_BOOT_ARGS \
	"bootargs=console=ttyS0,921600n1 rootwait skip_initramfs  earlycon=uart8250,mmio32,0x11002000 initcall_debug androidboot.hardware=mt8512 firmware_class.path=/vendor/firmware no_console_suspend\0" \

#define CONFIG_EXTRA_ENV_SETTINGS \
	"fdt_high=0x6c000000\0" \
	ENV_DEVICE_SETTINGS \
	ENV_FASTBOOT \
	ENV_BOOT_READ_IMAGE \
	ENV_BOOT_DEV \
	ENV_BOOT_ARGS \
	ENV_BOOT_CMD \
	"bootcmd=ext4load mmc 0:0xA ${loadaddr} image.itb; bootm;\0" \
	"verify=no\0"

#define CONFIG_FS_EXT4
#define CONFIG_CMD_EXT4

#endif

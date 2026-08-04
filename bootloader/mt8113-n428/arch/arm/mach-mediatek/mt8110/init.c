// SPDX-License-Identifier: GPL-2.0
/*
 * Configuration for MediaTek MT8110 SoC
 *
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Mingming Lee <mingming.lee@mediatek.com>
 */
#include <clk.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <ram.h>
#include <asm/arch/misc.h>
#include <asm/armv8/mmu.h>
#include <asm/sections.h>
#include <dm/uclass.h>
#include <dt-bindings/clock/mt8512-clk.h>
#include "../../../board/mediatek/mt8113_tp1/mt8113_tp1_data.h"

DECLARE_GLOBAL_DATA_PTR;
void hex_dump(const char *prefix, unsigned char *buf, int len)
{
   int i;

   if (!buf || !len)
		return;

	debug("%s:\n", prefix);
	for (i = 0; i < len; i++) {
		if (i != 0 && !(i % 16))
			debug("\n");
		debug("%02x", *(buf + i));
	}
	debug("\n");
}

unsigned int get_dramsize_from_boot_args(void)
{
        unsigned int dram_size = 0;
        debug("BOOT_ARGUMENT_LOCATION = 0x%x\n", BOOT_ARGUMENT_LOCATION);
        hex_dump("BOOT_ARGUMENT_LOCATION hex:", (unsigned char *)BOOT_ARGUMENT_LOCATION, sizeof(BOOT_ARGUMENT_T));
        BOOT_ARGUMENT_T *g_boot_args = (BOOT_ARGUMENT_T *)BOOT_ARGUMENT_LOCATION;
        if (g_boot_args->magic_number_begin != BOOT_ARGUMENT_MAGIC
           || g_boot_args->magic_number_end != BOOT_ARGUMENT_MAGIC) {
                printf("%s:boot arg magic error, please check the flow.\n", __func__);
                return 0;
        }

        dram_size = g_boot_args->dram_size;
        debug("g_boot_args->dram_size is 0x%x\n", g_boot_args->dram_size);
        return dram_size;
}

int dram_init(void)
{
	int iRet;
	unsigned int lk_dram_size = get_dramsize_from_boot_args();

	debug("lk ram size=%u\n",lk_dram_size);
	iRet = fdtdec_setup_mem_size_base();
	if( (unsigned int)gd->ram_size > lk_dram_size) {
		debug("gd dram size(%u) > lk ram size(%u),use lk dram size\n",
			(unsigned int)gd->ram_size , lk_dram_size);
		gd->ram_size = lk_dram_size;
	}
	return iRet;
}

phys_size_t  get_effective_memsize(void)
{
	/* limit stack below tee reserve memory */
	return gd->ram_size - 6 * SZ_1M;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = get_effective_memsize();

	return 0;
}

void reset_cpu(ulong addr)
{
	struct udevice *watchdog_dev = NULL;

	if (uclass_get_device_by_seq(UCLASS_WDT, 0, &watchdog_dev))
		if (uclass_get_device(UCLASS_WDT, 0, &watchdog_dev))
			printf("%s: pls implement wdt first!\n", __func__);

	wdt_expire_now(watchdog_dev, 0);
}

int print_cpuinfo(void)
{
	debug("CPU:   MediaTek MT8110\n");
	return 0;
}

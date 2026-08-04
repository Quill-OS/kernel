// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

//#define DEBUG
#include <common.h>
#include <dm.h>
#include <mapmem.h>

#include "mt8113_tp1_data.h"

DECLARE_GLOBAL_DATA_PTR;

BOOT_ARGUMENT_T *gpt_boot_args;

int board_init(void)
{
	unsigned int dram_size;

	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;
	debug("%s : gd->fdt_blob is %p\n",__FILE__,gd->fdt_blob);

	//gpt_boot_args=(BOOT_ARGUMENT_T *)BOOT_ARGUMENT_LOCATION;
	gpt_boot_args=(BOOT_ARGUMENT_T *)gd->bd->bi_boot_params;
	debug("bootarg @ %p,0x%x\n",gpt_boot_args,BOOT_ARGUMENT_LOCATION);
	
	debug("bootarg magic begin=0x%x\n",gpt_boot_args->magic_number_begin);
	debug("bootarg magic end=0x%x\n",gpt_boot_args->magic_number_end);

	debug("powerkey_status=0x%x\n",gpt_boot_args->powerkey_status);
	debug("usb_status=0x%x\n",gpt_boot_args->usb_status);
	return 0;
}


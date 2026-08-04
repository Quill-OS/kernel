/*****************************************************************************
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * Accelerometer Sensor Driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/

#include "hwtcon_hal.h"
#include "../hwtcon_def.h"
#include "../hwtcon_driver.h"

#include <linux/dma-mapping.h>

int hwtcon_hal_get_time_in_ms(HWTCON_TIME start, HWTCON_TIME end)
{
	HWTCON_TIME duration = end - start;

	return duration;
}

HWTCON_TIME timeofday_ms(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);
	return ((HWTCON_TIME) tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

HWTCON_TIME timeofday_us(void)
{
	struct timeval tv;

	do_gettimeofday(&tv);
	return ((HWTCON_TIME) tv.tv_sec * 1000000 + tv.tv_usec);

}

int hwtcon_hal_get_gpt_time_in_unit(HWTCON_TIME start, HWTCON_TIME end)
{
	/* 13 unit = 1us */
	HWTCON_TIME duration = 0;

	if (start > end)
		end += 0x100000000;
	duration = end - start;
	return duration;
}

u32 *hwtcon_hal_convert_pa_2_va(u32 pa)
{
	if (pa >= hwtcon_device_info()->mmsys_reg_pa &&
		pa < hwtcon_device_info()->mmsys_reg_pa + 0x10000)
		return (u32 *)((pa - hwtcon_device_info()->mmsys_reg_pa) +
			hwtcon_device_info()->mmsys_reg_va);

	if (pa >= hwtcon_device_info()->imgsys_reg_pa &&
		pa < hwtcon_device_info()->imgsys_reg_pa + 0x10000)
		return (u32 *)((pa - hwtcon_device_info()->imgsys_reg_pa) +
			hwtcon_device_info()->imgsys_reg_va);

	return NULL;
}

int hwtcon_hal_ffs(u64 data)
{
	int i = 0;

	for (i = 0; i < 64; i++)
		if (data & (1LL << i))
			return i;

	/* not found */
	return -1;
}

int hwtcon_hal_bit_set_cnt(u64 data)
{
	int i = 0;
	int count = 0;

	for (i = 0; i < 64; i++)
		if (data & (1LL << i))
			count++;
	return count;
}

u32 pp_read(void *va)
{
	return readl(va);
}

u32 pp_read_pa(u32 pa)
{
	u32 *va = ioremap(pa, sizeof(u32));
	u32 value = 0;

	value = readl(va);

	iounmap(va);
	return value;
}

void pp_write(struct cmdqRecStruct *pkt, u32 pa, u32 value)
{
	if (!pkt) {
		u32 *va = hwtcon_hal_convert_pa_2_va(pa);
		bool register_remap = false;

		if (va == NULL) {
			va = ioremap(pa, sizeof(u32));
			register_remap = true;
		}

		writel(value, va);

		if (register_remap)
			iounmap(va);
	} else {
		/* use gce */
		#if 0
		cmdq_pkt_assign_command(pkt, GCE_SPR0, pa);
		cmdq_pkt_store_value(pkt, GCE_SPR0, value, 0xFFFFFFFF);
		#else
		cmdqRecWrite(pkt, pa, value, 0xFFFFFFFF);
		#endif
	}
}

void pp_write_mask(struct cmdqRecStruct *pkt, u32 pa, u32 value, u32 mask)
{

	if (!pkt) {
		/* only update mask bit = 1, for 0 case, do not update. */
		u32 *va = hwtcon_hal_convert_pa_2_va(pa);
		bool register_remap = false;
		u32 read_back = 0;

		if (va == NULL) {
			va = ioremap(pa, sizeof(u32));
			register_remap = true;
		}
		read_back = pp_read(va);
		if (register_remap)
			iounmap(va);
		pp_write(pkt, pa, (read_back & ~mask) | (value & mask));
	} else {
		/* use gce */
		#if 0
		cmdq_pkt_assign_command(pkt, GCE_SPR0, pa);
		cmdq_pkt_store_value(pkt, GCE_SPR0, value, mask);
		#else
		cmdqRecWrite(pkt, pa, value, mask);
		#endif
	}
}

void pp_poll(struct cmdqRecStruct *pkt, u32 pa, u32 value, u32 mask)
{
	/* TODO: */
}

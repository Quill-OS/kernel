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

#ifndef __HWTCON_HAL_H__
#define __HWTCON_HAL_H__
#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "cmdq_record.h"
#include <linux/soc/mediatek/mtk-cmdq.h>
#include "hwtcon_reg.h"
#include "../hwtcon_def.h"

u32 *hwtcon_hal_convert_pa_2_va(u32 pa);
u32 pp_read(void *va);
u32 pp_read_pa(u32 pa);
void pp_write(struct cmdqRecStruct *pkt, u32 pa, u32 value);
void pp_write_mask(struct cmdqRecStruct *pkt, u32 pa, u32 value, u32 mask);
void pp_poll(struct cmdqRecStruct *pkt, u32 pa, u32 value, u32 mask);
HWTCON_TIME timeofday_ms(void);
int hwtcon_hal_get_time_in_ms(HWTCON_TIME start, HWTCON_TIME end);
int hwtcon_hal_get_gpt_time_in_unit(HWTCON_TIME start, HWTCON_TIME end);
int hwtcon_hal_ffs(u64 data);
int hwtcon_hal_bit_set_cnt(u64 data);

#endif /* __HWTCON_HAL_H__ */

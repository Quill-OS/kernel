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

#include "hwtcon_tcon_config.h"
#include "../hwtcon_fb.h"
#include "hwtcon_hal.h"
#include "../hwtcon_epd.h"

void tcon_config_swtcon_pin(struct cmdqRecStruct *pkt)
{

	/* T0 */
	pp_write_mask(pkt, PAPER_TCTOP_PIN_SWAP3,
		28 << 6,
		GENMASK(11, 6));

	/* T2 */
	pp_write_mask(pkt, PAPER_TCTOP_PIN_SWAP3,
		30 << 12,
		GENMASK(17, 12));

	/* T1 */
	pp_write_mask(pkt, PAPER_TCTOP_PIN_SWAP3,
		29 << 18,
		GENMASK(23, 18));
	/* T5 */
	pp_write_mask(pkt, PAPER_TCTOP_PIN_SWAP3,
		33 << 24,
		GENMASK(29, 24));

	/* T3 */
	pp_write_mask(pkt, PAPER_TCTOP_PIN_SWAP4,
		31,
		GENMASK(5, 0));

	/* T4 */
	pp_write_mask(pkt, PAPER_TCTOP_PIN_SWAP4,
		32 << 6,
		GENMASK(11, 6));

}


void tcon_config_global_register(struct cmdqRecStruct *pkt)
{
	pp_write(pkt, TCON_GR1, 0x0C000000);
	pp_write(pkt, TCON_GR0, 0x8000007F);
}

void tcon_config_timing0(struct cmdqRecStruct *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM0R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM0R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM0R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing1(struct cmdqRecStruct *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM1R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM1R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM1R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing2(struct cmdqRecStruct *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM2R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM2R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM2R1,
		((timing_para->HSPLCNT >> 8) & 0x00FF) |
		((timing_para->HSPLCNT & 0x00FF) << 8),
		GENMASK(15, 0));
	pp_write_mask(pkt, TCON_TIM2R4,
		timing_para->VACTSEL << 31,
		BIT_MASK(31));
	pp_write_mask(pkt, TCON_TIM2R6,
		timing_para->TCOPR,
		GENMASK(2, 0));
	pp_write_mask(pkt, TCON_TIM2R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing3(struct cmdqRecStruct *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM3R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM3R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM3R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

#if 1
void tcon_config_timing4(struct cmdqRecStruct *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM4R0,
		(timing_para->HE<<16) | timing_para->HS);
	pp_write(pkt, TCON_TIM4R3,
		(timing_para->VE<<16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM4R1,
		((timing_para->HSPLCNT >> 8) & 0x00FF) |
		((timing_para->HSPLCNT & 0x00FF) << 8),
		GENMASK(15, 0));
	pp_write_mask(pkt, TCON_TIM4R4,
		timing_para->VACTSEL << 31,
		BIT_MASK(31));
	pp_write_mask(pkt, TCON_TIM4R6,
		timing_para->TCOPR,
		GENMASK(2, 0));
	pp_write_mask(pkt, TCON_TIM4R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}
#endif

void tcon_config_timing5(struct cmdqRecStruct *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM5R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM5R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM5R1,
		((timing_para->HSPLCNT >> 8) & 0x00FF) |
		((timing_para->HSPLCNT & 0x00FF) << 8),
		GENMASK(15, 0));
	pp_write_mask(pkt, TCON_TIM5R4,
		timing_para->VACTSEL << 31,
		BIT_MASK(31));
	pp_write_mask(pkt, TCON_TIM5R6,
		timing_para->TCOPR,
		GENMASK(2, 0));
	pp_write_mask(pkt, TCON_TIM5R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_setting(struct cmdqRecStruct *pkt)
{
	tcon_config_timing0(pkt, hw_tcon_get_edp_tcon0_para());
	tcon_config_timing1(pkt, hw_tcon_get_edp_tcon1_para());
	tcon_config_timing2(pkt, hw_tcon_get_edp_tcon2_para());
	tcon_config_timing3(pkt, hw_tcon_get_edp_tcon3_para());
	tcon_config_timing4(pkt, hw_tcon_get_edp_tcon4_para());
	tcon_config_timing5(pkt, hw_tcon_get_edp_tcon5_para());
}


void tcon_disable(struct cmdqRecStruct *pkt)
{
	pp_write(pkt, TCON_GR1, 0x00000000);
	pp_write(pkt, TCON_GR0, 0x00000000);
}


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

#include "hwtcon_dpi_config.h"
#include "hwtcon_hal.h"
#include "../hwtcon_epd.h"

void wf_lut_config_dpi(struct cmdqRecStruct *pkt,
	struct dpi_timing_para *dpi_para,
	unsigned int width, unsigned int height)
{
	unsigned int dpi_size = (height << 16) | width;

	pp_write(pkt, WF_LUT_DPI_INTEN, 0x01);	/* IRQ Enable */

	pp_write_mask(pkt, WF_LUT_DPI_OUTPUT_SETTING,
		dpi_para->CK_POL<<15,
		BIT_MASK(15));

	/* OUTPUT_FRAME_WIDTH */
	pp_write(pkt, WF_LUT_DPI_SIZE, dpi_size);
	pp_write_mask(pkt, WF_LUT_DPI_TGEN_HWIDTH,
		dpi_para->HSA, GENMASK(11, 0));
	pp_write(pkt, WF_LUT_DPI_TGEN_HPORCH,
		(dpi_para->HFP<<16) | dpi_para->HBP);
	pp_write_mask(pkt, WF_LUT_DPI_TGEN_VWIDTH,
		dpi_para->VSA, GENMASK(11, 0));
	pp_write(pkt, WF_LUT_DPI_TGEN_VPORCH,
		(dpi_para->VFP<<16)|dpi_para->VBP);
	pp_write(pkt, WF_LUT_DPI_MUTEX_VSYNC_SETTING, 0x00000101);
}

void wf_lut_dpi_enable(struct cmdqRecStruct *pkt)
{
	wf_lut_dpi_enable_force_output(pkt, false);
	#if 1
	pp_write_mask(pkt, PAPER_TCTOP_DPI_CFG, 0x1<<1, BIT_MASK(1));
	#endif
	pp_write(pkt, WF_LUT_DPI_EN, 0x07);
}

void wf_lut_dpi_disable(struct cmdqRecStruct *pkt)
{
	wf_lut_dpi_enable_force_output(pkt, true);

	pp_write(pkt, WF_LUT_DPI_EN, 0x00);
}


void wf_lut_dpi_checksum_config(struct cmdqRecStruct *pkt,
	unsigned int enable)
{
	pp_write_mask(pkt, WF_LUT_DPI_CHECKSUM, enable<<31, BIT_MASK(31));
}

unsigned int wf_lut_dpi_get_checksum(struct cmdqRecStruct *pkt)
{
	if ((pp_read(WF_LUT_DPI_CHECKSUM_VA) & BIT_MASK(30)) == BIT_MASK(30)) {
		pp_write_mask(pkt, WF_LUT_DPI_CHECKSUM, 0 << 31, BIT_MASK(31));
		pp_write_mask(pkt, WF_LUT_DPI_CHECKSUM, 1 << 31, BIT_MASK(31));
		return (pp_read(WF_LUT_DPI_CHECKSUM_VA) & GENMASK(23, 0));
	}
	return 0x00;
}

u32 wf_lut_dpi_get_irq_status(void)
{
	return pp_read(WF_LUT_DPI_INTSTA_VA);
}


void wf_lut_dpi_clear_irq_status(struct cmdqRecStruct *pkt)
{
	pp_write(pkt, WF_LUT_DPI_INTSTA, 0x0);
}

/* No need to use in HWTCON V2 */
void wf_lut_dpi_enable_force_output(struct cmdqRecStruct *pkt, bool enable)
{
#if 0
	if (enable) {
		/* force dpi output 0 */
		pp_write(pkt, WF_LUT_DPI_TMODE, 0x01);
		pp_write_mask(pkt,
			WF_LUT_DPI_OUTPUT_SETTING, 1 << 16, BIT_MASK(16));
	} else {
		/* default value: disable force dpi output
		 * must disable before WF_LUT start to work
		 */
		pp_write(pkt, WF_LUT_DPI_TMODE, 0x01);
		pp_write_mask(pkt,
			WF_LUT_DPI_OUTPUT_SETTING, 0 << 16, BIT_MASK(16));
	}
#endif
}

void wf_lut_config_dpi_context(struct cmdqRecStruct *pkt,
	unsigned int width, unsigned int height)
{
	wf_lut_dpi_checksum_config(pkt, 0x01);
	wf_lut_dpi_enable_force_output(pkt, true);
	wf_lut_config_dpi(pkt, hw_tcon_get_edp_dpi_para(), width, height);
}


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

#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include "hwtcon_pipeline_config.h"
#include "hwtcon_reg.h"
#include "hwtcon_hal.h"
#include "../hwtcon_def.h"
#include "../hwtcon_driver.h"
#include "hwtcon_regal_config.h"
#include "hwtcon_reg.h"
#include "../hwtcon_epd.h"
#include "../hwtcon_core.h"
#include "../hwtcon_mdp.h"
#include "../hwtcon_debug.h"
#include "hwtcon_wf_lut_config.h"
#include "cmdq_helper_ext.h"
#include <asm/cacheflush.h>
/* change this var to switch gce / cpu config pipeline */
static bool pipeline_use_gce = true;

void pipeline_config_enable_sw_wb_wdma(struct cmdqRecStruct *pkt, bool enable)
{
	/* sw config working buffer wdma hardwware reg(size/addr/pitch),
	 *not pipeline hardware auto config.
	 */
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, enable << 2, BIT_MASK(2));
}

void pipeline_config_enable_sw_wb_rdma(struct cmdqRecStruct *pkt, bool enable)
{
	/* sw config working buffer rdma hardwware reg(size/addr/pitch),
	 *not pipeline hardware auto config.
	 */
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, enable << 1, BIT_MASK(1));
}

void pipeline_config_enable_sw_img_rdma(struct cmdqRecStruct *pkt, bool enable)
{
	/* sw config image buffer rdma hardwware reg(size/addr/pitch),
	 * not pipeline hardware auto config.
	 */
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, enable << 0, BIT_MASK(0));
}

void pipeline_config_img_buffer_pitch(struct cmdqRecStruct *pkt,
	enum PITCH_SELECT pitch_select_type, u32 pitch)
{
	if (pitch_select_type == PITCH_SELECT_HW_AUTO) {
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 0 << 6, BIT_MASK(6));
	} else {
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 1 << 6, BIT_MASK(6));
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG1, pitch, GENMASK(15, 0));
	}
}

void pipeline_config_wb_buffer_pitch(struct cmdqRecStruct *pkt,
	enum PITCH_SELECT pitch_select_type, u32 pitch)
{
	/* This is working buffer RDMA pitch
	 * Hardware will auto calculate working buffer WDMA pitch.
	 * for Non regal case: WB WDMA pitch = WB RDMA pitch
	 * for regal case: WB WDMA pitch = WB RDMA pitch / 2
	 */
	if (pitch_select_type == PITCH_SELECT_HW_AUTO) {
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 0 << 3, BIT_MASK(3));
	} else {
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 1 << 3, BIT_MASK(3));
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG1,
					pitch << 16, GENMASK(31, 16));
	}
}

void pipeline_config_enable_irq(struct cmdqRecStruct *pkt,
					enum HWTCON_IRQ_TYPE irq)
{
	switch (irq) {
	case IRQ_PIPELINE_COLLISION:
		/* collision irq must enable. if disable,
		 * lut_col event will not come.
		 * if we clear collsion irq in irq handler,
		 * the COLLISION_LUT status will reset to 0.
		 * Normal driver flow, we need to enable collision irq,
		 * but don't register this irq to kernel.
		 */
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 0, BIT_MASK(0));
		break;
	case IRQ_PIPELINE_LUT_ASSIGN_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 1, BIT_MASK(1));
		break;
	case IRQ_PIPELINE_DPI_UPDATE_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 2, BIT_MASK(2));
		break;
	case IRQ_PIPELINE_LUT_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 3, BIT_MASK(3));
		break;
	case IRQ_PIPELINE_REGION_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 4, BIT_MASK(4));
		break;
	case IRQ_WF_LUT_TCON_END:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 5, BIT_MASK(5));
		break;
	case IRQ_PIPELINE_PIXEL_LUT_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 6, BIT_MASK(6));
		break;
	case IRQ_PIPELINE_WB_FRAME_DONE:
		pp_write(pkt, WB_WDMA_INTEN, 3);
		break;
	default:
		TCON_ERR("invalid irq type:%d", irq);
	}
}

void pipeline_config_disable_irq(struct cmdqRecStruct *pkt,
						enum HWTCON_IRQ_TYPE irq)
{
	switch (irq) {
	case IRQ_PIPELINE_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 0, BIT_MASK(0));
		break;
	case IRQ_PIPELINE_LUT_ASSIGN_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 1, BIT_MASK(1));
		break;
	case IRQ_PIPELINE_DPI_UPDATE_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 2, BIT_MASK(2));
		break;
	case IRQ_PIPELINE_LUT_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 3, BIT_MASK(3));
		break;
	case IRQ_PIPELINE_REGION_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 4, BIT_MASK(4));
		break;
	case IRQ_WF_LUT_TCON_END:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 5, BIT_MASK(5));
		break;
	case IRQ_PIPELINE_PIXEL_LUT_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 6, BIT_MASK(6));
		break;
	case IRQ_PIPELINE_WB_FRAME_DONE:
		pp_write(pkt, WB_WDMA_INTEN, 0);
		break;
	default:
		TCON_ERR("invalid irq type:%d", irq);
	}
}


void pipeline_config_clear_irq(struct cmdqRecStruct *pkt,
					enum HWTCON_IRQ_TYPE irq)
{
	switch (irq) {
	case IRQ_PIPELINE_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 16, BIT_MASK(16));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 16, BIT_MASK(16));
		break;
	case IRQ_PIPELINE_LUT_ASSIGN_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 17, BIT_MASK(17));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 17, BIT_MASK(17));
		break;
	case IRQ_PIPELINE_DPI_UPDATE_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 18, BIT_MASK(18));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 18, BIT_MASK(18));
		break;
	case IRQ_PIPELINE_LUT_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 19, BIT_MASK(19));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 19, BIT_MASK(19));
		break;
	case IRQ_PIPELINE_REGION_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 20, BIT_MASK(20));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 20, BIT_MASK(20));
		break;
	case IRQ_WF_LUT_TCON_END:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 21, BIT_MASK(21));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 21, BIT_MASK(21));
		break;
	case IRQ_PIPELINE_PIXEL_LUT_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 22, BIT_MASK(22));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 22, BIT_MASK(22));
		break;
	case IRQ_PIPELINE_WB_FRAME_DONE:
		pp_write(pkt, WB_WDMA_INTSTA, 0);
		break;
	default:
		TCON_ERR("invalid irq type:%d", irq);
	}
}

void pipeline_config_reset_wb_rdma(struct cmdqRecStruct *pkt)
{
	pp_write_mask(pkt, WB_RDMA_GLOBAL_CON, 1 << 4, BIT_MASK(4));
	pp_write_mask(pkt, WB_RDMA_GLOBAL_CON, 0 << 4, BIT_MASK(4));
}

void pipeline_config_enable_wb_rdma(struct cmdqRecStruct *pkt, bool enable)
{
	u32 ultra_threshold_low = 0;
	u32 ultra_threshold_high = 0;
	u32 pre_ultra_threshold_low = 0x80;
	u32 pre_ultra_threshold_high = 0x80;

	if (enable)
		pp_write(pkt, WB_RDMA_GLOBAL_CON, 0x103);
	else
		pp_write(pkt, WB_RDMA_GLOBAL_CON, 0x0);
	/* config wb rdma fifo */
	pp_write_mask(pkt, WB_RDMA_FIFO_CON, 1, GENMASK(9, 0));

	/* config rdma ultra & pre-ultra */
	pp_write_mask(pkt, WB_RDMA_GMC_SETTING_0,
		ultra_threshold_low << 0 |
		ultra_threshold_high << 16 |
		pre_ultra_threshold_low << 8 |
		pre_ultra_threshold_high << 24,
		GENMASK(7, 0) |
		GENMASK(23, 16) |
		GENMASK(15, 8) |
		GENMASK(31, 24));
}

void pipeline_config_reset_img_rdma(struct cmdqRecStruct *pkt)
{
	pp_write_mask(pkt, IMG_RDMA_GLOBAL_CON, 1 << 4, BIT_MASK(4));
	pp_write_mask(pkt, IMG_RDMA_GLOBAL_CON, 0 << 4, BIT_MASK(4));
}

void pipeline_config_enable_img_rdma(struct cmdqRecStruct *pkt, bool enable)
{
	u32 ultra_threshold_low = 0;
	u32 ultra_threshold_high = 0;
	u32 pre_ultra_threshold_low = 0x80;
	u32 pre_ultra_threshold_high = 0x80;

	if (enable)
		pp_write(pkt, IMG_RDMA_GLOBAL_CON, 0x103);
	else
		pp_write(pkt, IMG_RDMA_GLOBAL_CON, 0x0);
	/* config img rdma fifo */
	pp_write_mask(pkt, IMG_RDMA_FIFO_CON, 1, GENMASK(9, 0));

	/* config rdma ultra & pre-ultra */
	pp_write_mask(pkt, IMG_RDMA_GMC_SETTING_0,
		ultra_threshold_low << 0 |
		ultra_threshold_high << 16 |
		pre_ultra_threshold_low << 8 |
		pre_ultra_threshold_high << 24,
		GENMASK(7, 0) |
		GENMASK(23, 16) |
		GENMASK(15, 8) |
		GENMASK(31, 24));
}

void pipeline_config_reset_wb_wdma(struct cmdqRecStruct *pkt)
{
	pp_write(pkt, WB_WDMA_RST, 1);
	pp_write(pkt, WB_WDMA_RST, 0);
}

void pipeline_config_enable_wb_wdma(struct cmdqRecStruct *pkt, bool enable)
{
	int ultra_en = 0;
	int pre_ultra_en = 1;
	int frame_end_ultra_en = 0;
	int issue_req_threshold = 1;

	if (enable) {
		/* reset WDMA */
		pp_write(pkt, WB_WDMA_RST, 1);
		/* set output format */
		pp_write(pkt, WB_WDMA_CFG, 0x50);
		/* enable WB WDMA */
		pp_write(pkt, WB_WDMA_EN, 1);
	} else {
		pp_write(pkt, WB_WDMA_EN, 0);
	}

	/* config wb wdma fifo */
	pp_write_mask(pkt, WB_WDMA_BUF_CON1,
		ultra_en << 31 | pre_ultra_en << 30 |
		frame_end_ultra_en << 28 | issue_req_threshold << 16,
		BIT_MASK(31) | BIT_MASK(30) | BIT_MASK(28) | GENMASK(25, 16));
}

void pipeline_config_enable_illegal_setting_buffer_write(
				struct cmdqRecStruct *pkt, bool enable)
{
	/* 0x1400D004[29]: 1: disable working buffer write when illegal setting
	 * 0x1400D004[29]: 0: enable working buffer write when illegal setting
	 */
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, (!enable) << 29, BIT_MASK(29));
}

void paper_config_reset_dpi_on_frame_done(
					struct cmdqRecStruct *pkt, bool enable)
{
	/* 0x1400D004[1]: 1: enable reset dpi on
	 * every DPI frame done. reset when WF_LUT hang to recover.
	 * 0x1400D004[0]: 0: disable this feature.
	 * when WF_LUT hang, the WF_LUT will not recover.
	 */
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, enable << 1, BIT_MASK(1));
}

void pipeline_config_fifo(struct cmdqRecStruct *pkt,
				bool enable, u32 fifo_size, u32 read_threshold)
{

	pp_write_mask(pkt, PAPET_TCTOP_FIFO_CFG, enable << 16 |
		fifo_size << 8 | read_threshold << 0,
		BIT_MASK(16) | GENMASK(15, 8) | GENMASK(7, 0));
}



void pipeline_config_enable_histogram(struct cmdqRecStruct *pkt, bool enable)
{
	if (enable)
		pp_write_mask(pkt, PAPER_TCTOP_HIST_CFG0, 1 << 0, BIT_MASK(0));
	else
		pp_write_mask(pkt, PAPER_TCTOP_HIST_CFG0, 0 << 0, BIT_MASK(0));
}

void pipeline_config_gray_value(struct cmdqRecStruct *pkt,
	u32 y2_gray_val, u32 y4_gray_val,
	u32 y8_gray_val, u32 y16_gray_val)
{
	pp_write(pkt, PAPER_TCTOP_HIST_CFG1, y2_gray_val);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG2, y4_gray_val);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG3, y8_gray_val);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG4, y16_gray_val);
}

void pipeline_config_wb_merge_region(struct cmdqRecStruct *pkt,
	bool enable, struct rect *merge_region)
{
	pp_write_mask(pkt, PAPER_TCTOP_WB_BUF_XY_INFO,
		enable << 31, BIT_MASK(31));
	if (enable && merge_region) {
		pp_write_mask(pkt, PAPER_TCTOP_WB_BUF_XY_INFO,
			merge_region->x << 0 | merge_region->y << 16,
			GENMASK(13, 0) | GENMASK(29, 16));
		pp_write_mask(pkt, PAPER_TCTOP_WB_BUF_WH_INFO,
			merge_region->width << 0 | merge_region->height << 16,
			GENMASK(13, 0) | GENMASK(29, 16));
	}
}

void pipeline_config_img_rdma_addr(struct cmdqRecStruct *pkt, u32 img_rdma_pa)
{
	pp_write(pkt, PAPER_TCTOP_IMG_ST_ADDR, img_rdma_pa);
}

void pipeline_config_wb_rdma_addr(struct cmdqRecStruct *pkt, u32 wb_rdma_pa)
{
	pp_write(pkt, PAPER_TCTOP_WB_ST_ADDR1, wb_rdma_pa);
}

void pipeline_config_wb_wdma_addr(struct cmdqRecStruct *pkt, u32 wb_wdma_pa)
{
	pp_write(pkt, PAPER_TCTOP_WB_ST_ADDR0, wb_wdma_pa);
}

void pipeline_config_panel_resolution(struct cmdqRecStruct *pkt,
					int panel_width, int panel_height)
{
	panel_width &= 0x3FFF;
	panel_height &= 0x3FFF;
	pp_write(pkt, PAPER_TCTOP_PANEL_SIZE, panel_height << 16 | panel_width);
}

/* enum PIPELINE_FLAG_ENUM */
void pipeline_config_update_flag(struct cmdqRecStruct *pkt, u32 flag)
{
	pp_write(pkt, PIPELINE_FLAG, flag);
}

void pipeline_config_update_lut(struct cmdqRecStruct *pkt, int lut_id)
{
	lut_id &= GENMASK(5, 0);
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2, lut_id << 4, GENMASK(9, 4));
}

void pipeline_config_trigger_hw(struct cmdqRecStruct *pkt)
{
	/* trigger pipeline start to work */
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2, 1 << 0, BIT_MASK(0));
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2, 0 << 0, BIT_MASK(0));
}

void pipeline_config_update_region(struct cmdqRecStruct *pkt,
							struct rect region)
{
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG0,
		(region.x & 0x3FFF) << 0 |
		(region.y & 0x3FFF) << 16,
		GENMASK(13, 0) | GENMASK(29, 16));
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG1,
		(region.width & 0x3FFF) << 0 |
		(region.height & 0x3FFF) << 16,
		GENMASK(13, 0) | GENMASK(29, 16));
}

void pipeline_config_active_lut(struct cmdqRecStruct *pkt,
					u32 active_lut1, u32 active_lut0)
{
	pp_write(pkt, PAPER_TCTOP_LUT_ACTIVE0, active_lut0);
	pp_write(pkt, PAPER_TCTOP_LUT_ACTIVE1, active_lut1);
}


void pipeline_get_collision_region(struct rect *region)
{
	u32 val = 0;

	val = pp_read(PAPER_TCTOP_PXL_COL_RGN0_VA);
	region->x = val & GENMASK(13, 0);
	region->y = (val & GENMASK(29, 16)) >> 16;

	val = pp_read(PAPER_TCTOP_PXL_COL_RGN1_VA);
	region->width = val & GENMASK(13, 0);
	region->height = (val & GENMASK(29, 16)) >> 16;
}

void pipeline_get_collision_lut_mask(u32 *col_lut1, u32 *col_lut0)
{
	*col_lut1 = pp_read(PAPER_TCTOP_COL_STATUS1_VA);
	*col_lut0 = pp_read(PAPER_TCTOP_COL_STATUS0_VA);
}

void pipeline_get_pixel_update_region(struct rect *region)
{
	u32 val = 0;

	val = pp_read(PAPER_TCTOP_PXL_UPD_RGN0_VA);
	region->x = val & GENMASK(13, 0);
	region->y = (val & GENMASK(29, 16)) >> 16;

	val = pp_read(PAPER_TCTOP_PXL_UPD_RGN1_VA);
	region->width = val & GENMASK(13, 0);
	region->height = (val & GENMASK(29, 16)) >> 16;
}

bool pipeline_get_update_void_status(void)
{
	u32 val = 0;

	val = pp_read(PAPER_TCTOP_VOID_LUT_VA);
	return val & BIT_MASK(0);
}

bool pipeline_get_do_clear_status(void)
{
	u32 val = 0;

	val = pp_read(PAPER_TCTOP_VOID_LUT_VA);
	return (val & BIT_MASK(1)) >> 1;
}

void pipeline_get_histogram(u32 *next_histogram, u32 *current_histogram)
{
	if (next_histogram)
		*next_histogram = pp_read(PAPER_TCTOP_HIST_STA1_VA);
	if (current_histogram)
		*current_histogram = pp_read(PAPER_TCTOP_HIST_STA0_VA);
}

void pipeline_get_gray_value(u32 *next_gray_value, u32 *current_gray_value)
{
	u32 gray_value = pp_read(PAPER_TCTOP_HIST_STA2_VA);

	if (next_gray_value)
		*next_gray_value = (gray_value & GENMASK(6, 4)) >> 4;
	if (current_gray_value)
		*current_gray_value = gray_value & GENMASK(2, 0);
}

int pipeline_poll_task_done(struct cmdqRecStruct *pkt)
{
	if (pkt) {
		cmdqRecWait(pkt, CMDQ_EVENT_WB_WDMA_DONE);
		return cmdqRecFlush(pkt);
	}
	if (wait_for_completion_timeout(
		&hwtcon_fb_info()->wb_frame_done_completion,
		msecs_to_jiffies(1000)) == 0) {
		TCON_ERR("wait pipeline wb frame done timeout");
		return -1;
	}
	return 0;
}

int pipeline_config_trigger(u32 img_addr_pa,
	u32 input_wb_addr_pa,
	u32 output_wb_addr_pa,
	struct pipeline_config_info config,
	struct pipeline_info *info)
{
	int status = 0;
	struct cmdqRecStruct *pkt = NULL;

	if (pipeline_use_gce) {
		cmdqRecCreate(CMDQ_SCENARIO_PIPELINE, &pkt);
		cmdqRecReset(pkt);
	}
	TCON_LOG("trigger pipeline lut:%d regoin[%d %d %d %d]",
		config.update_lut,
		config.update_region.x,
		config.update_region.y,
		config.update_region.width,
		config.update_region.height
		);
	TCON_LOG("flag:0x%08x active lut[0x%08x 0x%08x] regal_mode:%d",
		config.pipeline_ctl_flag,
		config.active_lut1,
		config.active_lut0,
		config.regal_mode);

	pipeline_config_enable_img_rdma(pkt, true);
	pipeline_config_enable_wb_rdma(pkt, true);
	pipeline_config_enable_wb_wdma(pkt, true);
	pipeline_config_enable_histogram(pkt, true);
	pipeline_config_enable_illegal_setting_buffer_write(pkt, false);
	paper_config_reset_dpi_on_frame_done(pkt, true);

	/* enable irq */
	pipeline_config_enable_irq(pkt, IRQ_PIPELINE_WB_FRAME_DONE);
	pipeline_config_enable_irq(pkt, IRQ_PIPELINE_COLLISION);
	pipeline_config_enable_irq(pkt, IRQ_PIPELINE_LUT_ASSIGN_DONE);
	pipeline_config_enable_irq(pkt, IRQ_PIPELINE_LUT_ILLEGAL);
	pipeline_config_enable_irq(pkt, IRQ_PIPELINE_REGION_ILLEGAL);

	if (config.pipeline_ctl_flag | PIPELINE_FLAG_REGAL)
		pipeline_config_disable_irq(pkt,
			IRQ_PIPELINE_PIXEL_LUT_COLLISION);
	else
		pipeline_config_enable_irq(pkt,
			IRQ_PIPELINE_PIXEL_LUT_COLLISION);

	pipeline_config_fifo(pkt, true, 0xFF, 0x10);

	pipeline_config_img_rdma_addr(pkt, img_addr_pa);
	pipeline_config_wb_rdma_addr(pkt, input_wb_addr_pa);
	pipeline_config_wb_wdma_addr(pkt, output_wb_addr_pa);

	pipeline_config_panel_resolution(pkt,
		hw_tcon_get_edp_width(),
		hw_tcon_get_edp_height());

	pipeline_config_update_flag(pkt, config.pipeline_ctl_flag);
	if (config.pipeline_ctl_flag & PIPELINE_FLAG_REGAL)
		hwtcon_regal_config_regal_mode(pkt,
			config.regal_mode,
			hw_tcon_get_edp_width(),
			hw_tcon_get_edp_height());

	if (config.use_wb_merge_region) {
		/* sw config working merge region */
		pipeline_config_wb_merge_region(pkt,
			true, &config.wb_merge_region);
	} else
		pipeline_config_wb_merge_region(pkt, false, NULL);

	if (config.use_sw_config_img_pitch)
		pipeline_config_img_buffer_pitch(pkt,
			PITCH_SELECT_SW_CONFIG, config.img_pitch);
	else
		pipeline_config_img_buffer_pitch(pkt,
					PITCH_SELECT_HW_AUTO, 0);

	if (config.use_sw_config_wb_pitch)
		pipeline_config_wb_buffer_pitch(pkt,
			PITCH_SELECT_SW_CONFIG, config.wb_pitch);
	else
		pipeline_config_wb_buffer_pitch(pkt,
					PITCH_SELECT_HW_AUTO, 0);

	pipeline_config_update_lut(pkt, config.update_lut);
	pipeline_config_update_region(pkt, config.update_region);

	pipeline_config_active_lut(pkt, config.active_lut1, config.active_lut0);

	if (pkt)
		cmdqCoreClearEvent(CMDQ_EVENT_WB_WDMA_DONE);
	else
		reinit_completion(&hwtcon_fb_info()->wb_frame_done_completion);
	pipeline_config_trigger_hw(pkt);

	status = pipeline_poll_task_done(pkt);

	pipeline_get_collision_lut_mask(&info->collision_lut_1,
						&info->collision_lut_0);
	pipeline_get_collision_region(&info->collision_region);
	pipeline_get_histogram(&info->next_histogram, &info->current_histogram);
	info->do_clear = pipeline_get_do_clear_status();
	info->update_void = pipeline_get_update_void_status();
	info->panel_width = pp_read(PAPER_TCTOP_PANEL_SIZE_VA) & GENMASK(12, 0);
	info->panel_height =
		(pp_read(PAPER_TCTOP_PANEL_SIZE_VA) & GENMASK(28, 16)) >> 16;

	TCON_LOG("pipeline info void:%d clear:%d histo[0x%08x 0x%08x]",
		info->update_void,
		info->do_clear,
		info->next_histogram,
		info->current_histogram);
	TCON_LOG("col_lut[0x%08x 0x%08x] col_region[%d %d %d %d]",
		info->collision_lut_1,
		info->collision_lut_0,
		info->collision_region.x,
		info->collision_region.y,
		info->collision_region.width,
		info->collision_region.height);

	if (pipeline_use_gce)
		cmdqRecDestroy(pkt);

	return status;
}

int pipeline_init_working_buffer(void)
{
	struct pipeline_config_info config;
	struct pipeline_info info;
	u64 active_lut = 0LL;
	unsigned long flags;
	int status = 0;

	memset(&config, 0, sizeof(config));
	memset(&info, 0, sizeof(info));

	spin_lock_irqsave(&hwtcon_fb_info()->lut_active_lock, flags);
	active_lut = hwtcon_fb_info()->lut_active;
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_active_lock, flags);

	config.active_lut0 = (u32)(active_lut & 0xFFFFFFFF);
	config.active_lut1 = (u32)(active_lut >> 32);
	config.update_lut = 0x3f;
	config.update_region.x = 0;
	config.update_region.y = 0;
	config.update_region.width = 0;
	config.update_region.height = 0;

	config.pipeline_ctl_flag = PIPELINE_FLAG_FULL_UPDATE |
		PIPELINE_FLAG_Y5_INPUT |
		PIPELINE_FLAG_CLEAR;

	status = pipeline_config_trigger(hwtcon_fb_info()->img_buffer_pa,
		hwtcon_fb_info()->wb_buffer_pa,
		hwtcon_fb_info()->wb_buffer_pa,
		config, &info);
	if (status != 0)
		return status;


	/* update released lut to free lut */
	spin_lock_irqsave(&hwtcon_fb_info()->lut_free_lock, flags);
	hwtcon_fb_info()->lut_free = ~active_lut & LUT_BIT_ALL_SET;
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_free_lock, flags);

	return 0;
}

int pipeline_trigger_regal(struct hwtcon_task *task)
{
	struct pipeline_config_info config;
	struct pipeline_info info;
	int status = 0;

	memset(&config, 0, sizeof(config));
	memset(&info, 0, sizeof(info));

	if (task->regal_status != REGAL_STATUS_REGAL) {
		TCON_ERR("invalid regal status:%d",
			task->regal_status);
		return 0;
	}

	config.regal_mode = task->regal_mode;

	/* update region: panel resolution */
	config.update_region.x = 0;
	config.update_region.y = 0;
	config.update_region.width = hw_tcon_get_edp_width();
	config.update_region.height = hw_tcon_get_edp_height();

	config.active_lut0 = 0;
	config.active_lut1 = 0;
	config.update_lut = 0;
	config.pipeline_ctl_flag = PIPELINE_FLAG_FULL_UPDATE |
		PIPELINE_FLAG_REGAL;

	status = pipeline_config_trigger(hwtcon_fb_info()->img_buffer_pa,
		hwtcon_fb_info()->wb_buffer_pa,
		hwtcon_fb_info()->temp_img_buffer_pa,
		config, &info);
	if (status != 0)
		return status;

	task->regal_status = REGAL_STATUS_REGAL_HANDLED;
	return 0;
}

int pipeline_handle_normal_update(struct hwtcon_task *task,
	struct pipeline_info *info)
{
	int status = 0;
	int acquired_id = 0;
	bool need_do_clear = false;
	u32 pipeline_flag = PIPELINE_FLAG_Y5_INPUT;
	struct pipeline_config_info config;
	u64 active_lut = 0LL;
	unsigned long flags;
	HWTCON_TIME T1, T2;
	int ret;

	if ( hwtcon_core_use_cfa_color_and_regal(task) && 
			(REGAL_MODE_REGAL_CFA==task->regal_mode) ) 
	{
		/* invalid CFA regal input buffer before CFA regal start to read */
		hwtcon_fb_invalid_cache(hwtcon_fb_info()->img_cache_buf);
		hwtcon_fb_invalid_cache(hwtcon_fb_info()->wb_cache_buf);
		T1 = timeofday_ms();
		ret = hwtcon_mdp_regal_handle(task);
		if (ret)
			TCON_ERR("trigger cfa color regal fail");
		else
			TCON_LOG("trigger cfa color regal successfully");
		T2 = timeofday_ms();
		/* clean CFA regal output buffer after CFA regal work done */
		hwtcon_fb_clean_cache(hwtcon_fb_info()->temp_img_cache_buf);
		task->regal_status = REGAL_STATUS_REGAL_HANDLED;

		hwtcon_debug_record_printf("cfa color regal takes:%d ms\n",
			hwtcon_hal_get_time_in_ms(T1, T2));
	} else if (task->regal_status == REGAL_STATUS_REGAL) {

		status = pipeline_trigger_regal(task);
		if (status != 0)
			return status;
	}

	/* full / partial update */
	if (task->update_data.update_mode == UPDATE_MODE_FULL)
		pipeline_flag |= PIPELINE_FLAG_FULL_UPDATE;

	/* allocate a free lut */
	status = hwtcon_core_get_free_lut(&need_do_clear, &acquired_id);
	if (status == GET_LUT_ERR || status == GET_LUT_TIMEOUT)
		return status;
	if (status == GET_LUT_BUSY) {
		status = hwtcon_core_get_free_lut(&need_do_clear, &acquired_id);
		if (status != GET_LUT_OK) {
			TCON_ERR("realloc lut id fail:%d", status);
			return status;
		}
	}

	spin_lock_irqsave(&hwtcon_fb_info()->lut_active_lock, flags);
	active_lut = hwtcon_fb_info()->lut_active & ~(1LL << acquired_id);
	spin_unlock_irqrestore(&hwtcon_fb_info()->lut_active_lock, flags);


	/* allocate lut success. */
	if (need_do_clear)
		pipeline_flag |= PIPELINE_FLAG_CLEAR;

	memset(&config, 0, sizeof(config));
	memset(info, 0, sizeof(*info));

	config.update_lut = acquired_id;
	config.active_lut0 = (u32)(active_lut & 0xFFFFFFFF);
	config.active_lut1 = (u32)(active_lut >> 32);
	config.pipeline_ctl_flag = pipeline_flag;
	config.update_region.x = hwtcon_core_get_task_region(task).x;
	config.update_region.y = hwtcon_core_get_task_region(task).y;
	config.update_region.width = hwtcon_core_get_task_region(task).width;
	config.update_region.height = hwtcon_core_get_task_region(task).height;

	hwtcon_edp_pinmux_active();

	flush_cache_all();
	outer_flush_all();

	//outer_flush_range(hwtcon_fb_info()->wb_buffer_pa, hwtcon_fb_info()->wb_buffer_size);

	if (task->regal_status == REGAL_STATUS_REGAL_HANDLED)
		status = pipeline_config_trigger(
				hwtcon_fb_info()->temp_img_buffer_pa,
				hwtcon_fb_info()->wb_buffer_pa,
				hwtcon_fb_info()->wb_buffer_pa,
				config, info);
	else
		status = pipeline_config_trigger(
			hwtcon_fb_info()->img_buffer_pa,
			hwtcon_fb_info()->wb_buffer_pa,
			hwtcon_fb_info()->wb_buffer_pa,
			config, info);
	if (status != 0) {
		/* pipeline update working buffer fail,
		 * put back acquired lut id
		 */
		spin_lock_irqsave(&hwtcon_fb_info()->lut_free_lock, flags);
		hwtcon_fb_info()->lut_free |= (1LL << acquired_id);
		spin_unlock_irqrestore(&hwtcon_fb_info()->lut_free_lock, flags);

		spin_lock_irqsave(&hwtcon_fb_info()->lut_active_lock, flags);
		hwtcon_fb_info()->lut_active &= ~(1LL << acquired_id);
		spin_unlock_irqrestore(
			&hwtcon_fb_info()->lut_active_lock, flags);
		return status;
	}

	/* update free lut */
	if (info->update_void) {
		/* pipeline does not update working buffer,
		 * put back acquired lut id
		 */
		spin_lock_irqsave(&hwtcon_fb_info()->lut_free_lock, flags);
		hwtcon_fb_info()->lut_free |= (1LL << acquired_id);
		spin_unlock_irqrestore(&hwtcon_fb_info()->lut_free_lock, flags);

		spin_lock_irqsave(&hwtcon_fb_info()->lut_active_lock, flags);
		hwtcon_fb_info()->lut_active &= ~(1LL << acquired_id);
		spin_unlock_irqrestore(
			&hwtcon_fb_info()->lut_active_lock, flags);
	} else {
		task->assign_lut = acquired_id;
		if (pipeline_flag & PIPELINE_FLAG_CLEAR) {
			spin_lock_irqsave(
				&hwtcon_fb_info()->lut_free_lock, flags);
			hwtcon_fb_info()->lut_free =
				~(active_lut | 1LL << acquired_id) &
				LUT_BIT_ALL_SET;
			spin_unlock_irqrestore(
				&hwtcon_fb_info()->lut_free_lock, flags);
		}
	}
	return 0;
}


void rdma_config_smi_setting(struct cmdqRecStruct *pkt)
{
#ifndef HWTCON_M4U_SUPPORT
	u32 i = 0;

	/* smi common */
	pp_write(pkt, 0x140021a0, 0x1);
	pp_write(pkt, 0x140021c0, 0x0);
	pp_write(pkt, 0x140021a0, 0x0);
	pp_write(pkt, 0x140021a4, 0x1);
	pp_write(pkt, 0x14002220, 0x4444);

	/* SMI LARB0 */
	pp_write(pkt, 0x14003400, 0x1);
	pp_write(pkt, 0x14003400, 0x0);
	pp_write(pkt, 0x14003404, 0x1);

	for (i = 0; i < 10; i++) {
		pp_write(pkt, 0x14003380 + i * 4, 0);
		pp_write(pkt, 0x14003f80 + i * 4, 0);
	}

	/* SMI LARB1 */
	pp_write(pkt, 0x15002400, 0x1);
	pp_write(pkt, 0x15002400, 0x0);
	pp_write(pkt, 0x15002404, 0x1);

	/* memset 0x15002f80 ~ 0x15002fe0 to 0 */
	for (i = 0; i < 18; i++) {
		pp_write(pkt, 0x15002380 + i * 4, 0);
		pp_write(pkt, 0x15002f80 + i * 4, 0);
	}
#endif
}


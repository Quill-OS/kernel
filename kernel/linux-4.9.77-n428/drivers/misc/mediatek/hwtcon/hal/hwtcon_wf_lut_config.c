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

#include "hwtcon_wf_lut_config.h"
#include "hwtcon_hal.h"
#include "hwtcon_dpi_config.h"
#include "hwtcon_wf_lut_rdma_config.h"
#include "hwtcon_tcon_config.h"
#include "../hwtcon_def.h"
#include "../hwtcon_epd.h"
#include "../hwtcon_core.h"
#include "hwtcon_pipeline_config.h"
#include "hwtcon_wf_lut_config.h"
#include <linux/of_reserved_mem.h>
#include <asm/memory.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/pstore.h>
#include <linux/io.h>
#include <mt-plat/aee.h>
#include "../hwtcon_debug.h"

#define WF_WDMA_SUPPORT

struct pinctrl *g_pctrl;
struct pinctrl_state *g_pin_state_active;
struct pinctrl_state *g_pin_state_inactive;
static struct pinctrl_state *g_pin_state_current=(struct pinctrl_state *)-1;

struct wf_lut_waveform *g_current_waveform_mode_in_HW[
				WAVEFORM_MODE_TOTAL_NUM] = {NULL};
static int g_current_temperature;
static int g_ts_threshold[TEMPERATURE_NUM];

static struct wf_mode_relation *gptWF_tab = 0;
static int giWF_tab_items = 0;


static struct wf_mode_relation * _get_wf_mode_tab(u8 bModeVer, int *O_piTabSize);

static struct wf_mode_map gtUserWfmMapTabA[] = 
{
	{0,WAVEFORM_MODE_INIT,WAVEFORM_MODE_INIT,-1},
	{0,WAVEFORM_MODE_DU,WAVEFORM_MODE_DU,-1},
	{0,WAVEFORM_MODE_GC16,WAVEFORM_MODE_GC16,-1},
	{0,WAVEFORM_MODE_A2,WAVEFORM_MODE_A2,-1},
	{0,WAVEFORM_MODE_GL16,WAVEFORM_MODE_GL16,-1},
	{0,WAVEFORM_MODE_GLR16,WAVEFORM_MODE_GLR16,-1},
	{0,WAVEFORM_MODE_GCK16,WAVEFORM_MODE_GCK16,-1},
	{0,WAVEFORM_MODE_GLKW16,WAVEFORM_MODE_GLKW16,-1},
	{0,WAVEFORM_MODE_GCC16,WAVEFORM_MODE_GCC16,-1},
	{0,WAVEFORM_MODE_GLRC16,WAVEFORM_MODE_GLRC16,-1},
};
#define gtUserWfmMapTab_total sizeof(gtUserWfmMapTabA)/sizeof(gtUserWfmMapTabA[0])

static struct wf_file_info g_wf_file_info;

int giWFM_gl16,giWFM_gc16,giWFM_du,giWFM_glrc16;
int giWFM_gl16_eink,giWFM_gc16_eink,giWFM_du_eink,giWFM_glrc16_eink;


void wf_lut_parse_wf_file(char *waveform_va)
{
	snprintf(g_wf_file_info.wf_file_name,
		sizeof(g_wf_file_info.wf_file_name),
		"%s",
		waveform_va);
	g_wf_file_info.mode_version = *(waveform_va + WF_OFFSET_MODE_VERSION);
	g_wf_file_info.wf_type = *(waveform_va + WF_OFFSET_WF_TYPE);
	g_wf_file_info.wfm_rev = *(waveform_va + WF_OFFSET_WFM_REV);

	g_wf_file_info.wf_file_ready = true;

	gptWF_tab = _get_wf_mode_tab(wf_lut_get_wf_info()->mode_version,&giWF_tab_items);

	{
		int i;
		int iSlot;
		int iWFM_eink;

		giWFM_gc16 = WAVEFORM_MODE_GC16;
		if(WF_SLOT_0==wf_lut_get_waveform_mode_slot(giWFM_gc16,-1,&giWFM_gc16_eink)) {
			giWFM_gc16 = WAVEFORM_MODE_INIT;
			giWFM_gc16_eink = 0;
		}
	
		giWFM_gl16 = WAVEFORM_MODE_GL16;
		if(WF_SLOT_0==wf_lut_get_waveform_mode_slot(giWFM_gl16,-1,&giWFM_gl16_eink)) {
			giWFM_gl16 = giWFM_gc16;
			giWFM_gl16_eink = giWFM_gc16_eink;
		}
	
		giWFM_du = WAVEFORM_MODE_DU;
		if(WF_SLOT_0==wf_lut_get_waveform_mode_slot(giWFM_du,-1,&giWFM_du_eink)) {
			giWFM_du = giWFM_gl16;
			giWFM_du_eink = giWFM_gl16_eink;
		}

		giWFM_glrc16 = WAVEFORM_MODE_GLRC16;
		if(WF_SLOT_0==wf_lut_get_waveform_mode_slot(giWFM_glrc16,-1,&giWFM_glrc16_eink)) {
			giWFM_glrc16 = giWFM_gl16;
			giWFM_glrc16_eink = giWFM_gl16_eink;
		}

		for(i=0;i<gtUserWfmMapTab_total;i++)
		{

			gtUserWfmMapTabA[i].szUser_wfm_name = hwtcon_core_get_wf_mode_name(gtUserWfmMapTabA[i].iUser_wfm);

			iSlot = wf_lut_get_waveform_mode_slot(gtUserWfmMapTabA[i].iUser_wfm,-1,&iWFM_eink);
			if(WF_SLOT_0==iSlot) {
				// waveform mode not supported by this waveform file . 
				switch (gtUserWfmMapTabA[i].iUser_wfm) {
				case WAVEFORM_MODE_GL16:
				case WAVEFORM_MODE_GLR16:
				case WAVEFORM_MODE_GLKW16:
				case WAVEFORM_MODE_GLRC16:
					gtUserWfmMapTabA[i].iMtk_wfm = giWFM_gl16;
					gtUserWfmMapTabA[i].iEink_wfm = giWFM_gl16_eink;
					break;
				case WAVEFORM_MODE_GCK16:
				case WAVEFORM_MODE_GCC16:
					gtUserWfmMapTabA[i].iMtk_wfm = giWFM_gc16;
					gtUserWfmMapTabA[i].iEink_wfm = giWFM_gc16_eink;
					break;
				case WAVEFORM_MODE_A2:
					gtUserWfmMapTabA[i].iMtk_wfm = giWFM_du;
					gtUserWfmMapTabA[i].iEink_wfm = giWFM_du_eink;
					break;
				}

				TCON_ERR("mode \"%s\" not supported in mode_ver=0x%x waveform file ",
						gtUserWfmMapTabA[i].szUser_wfm_name,wf_lut_get_wf_info()->mode_version);
			}
			else {
#if 0
				if( hw_tcon_get_epd_type() && 
					(WAVEFORM_MODE_GLR16==gtUserWfmMapTabA[i].iUser_wfm||
					(WAVEFORM_MODE_GLRC16==gtUserWfmMapTabA[i].iUser_wfm||
					WAVEFORM_MODE_GLKW16==gtUserWfmMapTabA[i].iUser_wfm) )
				{
					gtUserWfmMapTabA[i].iMtk_wfm = giWFM_gl16;
					gtUserWfmMapTabA[i].iEink_wfm = giWFM_gl16_eink;
				}
				else 
#endif
				{
					gtUserWfmMapTabA[i].iEink_wfm = iWFM_eink;
				}
			}
		}
	}
}

enum WAVEFORM_MODE_ENUM wf_lut_wfmode_set_driver_wfm(
		enum WAVEFORM_MODE_ENUM user_wfm,
		enum WAVEFORM_MODE_ENUM driver_wfm)
{
	int i;
	enum WAVEFORM_MODE_ENUM wfm_ret = WAVEFORM_MODE_INIT;

	if(WAVEFORM_MODE_AUTO == user_wfm) {
		return WAVEFORM_MODE_AUTO;
	}
	else if(WAVEFORM_MODE_INIT == user_wfm) {
		return WAVEFORM_MODE_INIT;
	}

	for(i=0;i<gtUserWfmMapTab_total;i++)
	{
		if(gtUserWfmMapTabA[i].iUser_wfm==user_wfm) {
			wfm_ret = gtUserWfmMapTabA[i].iMtk_wfm ;
			gtUserWfmMapTabA[i].iMtk_wfm = driver_wfm;
			wf_lut_get_waveform_mode_slot(driver_wfm,-1,&gtUserWfmMapTabA[i].iEink_wfm);
			break;
		}
	}

	if(WAVEFORM_MODE_INIT==wfm_ret) {
		TCON_ERR("use a unsupported waveform mode %d ! please check !",user_wfm);
	}

	return wfm_ret;
}

enum WAVEFORM_MODE_ENUM wf_lut_wfmode_user2driver(
		enum WAVEFORM_MODE_ENUM user_waveform_mode)
{
	int i;
	enum WAVEFORM_MODE_ENUM wfm_ret = WAVEFORM_MODE_INIT;

	if(WAVEFORM_MODE_AUTO == user_waveform_mode) {
		return WAVEFORM_MODE_AUTO;
	}
	else if(WAVEFORM_MODE_INIT == user_waveform_mode) {
		return WAVEFORM_MODE_INIT;
	}

	for(i=0;i<gtUserWfmMapTab_total;i++)
	{
		if(gtUserWfmMapTabA[i].iUser_wfm==user_waveform_mode) {
			wfm_ret = gtUserWfmMapTabA[i].iMtk_wfm ;
			break;
		}
	}

	if(WAVEFORM_MODE_INIT==wfm_ret) {
		TCON_ERR("use a unsupported waveform mode %d ! please check !",user_waveform_mode);
	}

	return wfm_ret;
}

static int giWaveform_modes_item_idx;
static char gcWaveform_modes_info_bufA[256];
char * wf_lut_get_waveform_modes_info_header(void)
{
	return "name, user, driver, eink";
}
char * wf_lut_get_waveform_modes_info(int iIsFirst)
{
	if(iIsFirst) {
		giWaveform_modes_item_idx=0;
	}

	if(giWaveform_modes_item_idx>=gtUserWfmMapTab_total) {
		return 0;
	}

	sprintf(gcWaveform_modes_info_bufA,"%s, %d(%s), %d(%s), %d",
			gtUserWfmMapTabA[giWaveform_modes_item_idx].szUser_wfm_name,
			gtUserWfmMapTabA[giWaveform_modes_item_idx].iUser_wfm,
			hwtcon_core_get_wf_mode_name(gtUserWfmMapTabA[giWaveform_modes_item_idx].iUser_wfm),
			gtUserWfmMapTabA[giWaveform_modes_item_idx].iMtk_wfm,
			hwtcon_core_get_wf_mode_name(gtUserWfmMapTabA[giWaveform_modes_item_idx].iMtk_wfm),
			gtUserWfmMapTabA[giWaveform_modes_item_idx].iEink_wfm);

	giWaveform_modes_item_idx++;
	return gcWaveform_modes_info_bufA;
}

struct wf_file_info *wf_lut_get_wf_info(void)
{
	if (!g_wf_file_info.wf_file_ready)
		hwtcon_core_load_init_setting_from_file();

	return &g_wf_file_info;
}

#ifdef WF_WDMA_SUPPORT
void Wf_Lut_Wdma_addr(struct cmdqRecStruct *pkt, unsigned int addr)
{
	pp_write(pkt, WDMA_DST_ADDR0, addr);/* WDMA_DST_ADDR0 */
}

void Wf_Lut_Wdma_Config(struct cmdqRecStruct *pkt,
			unsigned int width, unsigned int height)
{
	pp_write_mask(NULL, MMSYS_CG_CON0, 0x0<<29, BIT_MASK(29));
	pp_write_mask(pkt, DISP_WDMA0_SEL_IN, 0x1, BIT_MASK(0));
	pp_write(pkt, WDMA_INTEN, 0x00000001);
	pp_write(pkt, WDMA_CFG, 0x02020030);
	pp_write(pkt, WDMA_DST_W_IN_BYTE, 4 * width);
	pp_write(pkt, WDMA_DST_UV_PITCH, width);
	pp_write(pkt, WDMA_SRC_SIZE, height << 16 | width);
	pp_write(pkt, WDMA_CLIP_SIZE, height << 16 | width);
	pp_write(pkt, WDMA_CLIP_COORD, 0x00000000);
	Wf_Lut_Wdma_addr(pkt, hwtcon_fb_info()->wb_buffer_pa);
	pp_write(pkt, WDMA_EN, 0x00000001);
}

void wf_lut_wdma_clear_irq(void)
{
	pp_write(NULL, WDMA_INTSTA, 0x00000000); /* bit clear*/
}
#endif

void wf_lut_all_end_clear_irq(void)
{
	pp_write_mask(NULL, WF_LUT_DATAPATH_CON, 0x1<<24, BIT_MASK(24));
	pp_write_mask(NULL, WF_LUT_DATAPATH_CON, 0x0<<24, BIT_MASK(24));
}

static struct wf_lut_waveform
g_waveform_table[TEMPERATURE_NUM][WAVEFORM_MODE_TOTAL_NUM];

void wf_lut_config_auto_off_tcon_dpi_signal(struct cmdqRecStruct *pkt,
								bool enable)
{
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, enable << 30, BIT_MASK(30));
	pp_write_mask(pkt, PAPER_TCTOP_DPI_CFG,
		((hw_tcon_get_edp_dpi_cnt_off() * 7) >> 3) << 4,
		GENMASK(23, 4));
}

void wf_lut_config_mmsys(struct cmdqRecStruct *pkt,
			 struct wf_lut_con_config *wf_lut_config)
{
	#ifdef FPGA_EARLY_PORTING
	/* enable fiti power */
	pp_write(NULL, MMSYS_DUMMY1, 1 << 1 | 1 << 3 | 1 << 5);
	mdelay(100);
	#endif

	if (wf_lut_config->rg_8b_out) {
		pp_write(pkt, MMSYS_MDP_DL_CFG_WD,
			 (wf_lut_config->height << 16) |
			 (wf_lut_config->width / 4));
	} else {
		pp_write(pkt, MMSYS_MDP_DL_CFG_WD,
			 (wf_lut_config->height << 16) |
			 (wf_lut_config->width / 8));
	}

}

void wf_lut_config_lut_enable(struct cmdqRecStruct *pkt,
		unsigned int lut0_value,
		unsigned int lut1_value)
{
	pp_write(pkt, WF_LUT_EN_0, lut0_value);/*WF_LUT_EN_0-31 enable lut64*/
	pp_write(pkt, WF_LUT_EN_1, lut1_value);/*WF_LUT_EN_32-63 enable lut64*/
}


void wf_lut_config_waveform(struct cmdqRecStruct *pkt,
				struct wf_lut_waveform *waveform)
{
	/* config current temp_zone's all waveform mode */
	int i = 0;
	struct wf_lut_waveform *current_waveform = waveform;
	const u32 wf_lut_addr_reg[WAVEFORM_MODE_TOTAL_NUM] = {
		WF_LUT_ADDR_0, WF_LUT_ADDR_1, WF_LUT_ADDR_2, WF_LUT_ADDR_3,
		WF_LUT_ADDR_4, WF_LUT_ADDR_5, WF_LUT_ADDR_6, WF_LUT_ADDR_7,
		WF_LUT_ADDR_8, WF_LUT_ADDR_9, WF_LUT_ADDR_10, WF_LUT_ADDR_11,
		WF_LUT_ADDR_12, WF_LUT_ADDR_13, WF_LUT_ADDR_14, WF_LUT_ADDR_15,
	};
	const u32 wf_lut_len_reg[WAVEFORM_MODE_TOTAL_NUM] = {
		WF_LUT_LEN_0, WF_LUT_LEN_1, WF_LUT_LEN_2, WF_LUT_LEN_3,
		WF_LUT_LEN_4, WF_LUT_LEN_5, WF_LUT_LEN_6, WF_LUT_LEN_7,
		WF_LUT_LEN_8, WF_LUT_LEN_9, WF_LUT_LEN_10, WF_LUT_LEN_11,
		WF_LUT_LEN_12, WF_LUT_LEN_13, WF_LUT_LEN_14, WF_LUT_LEN_15,
	};

	for (i = 0; i < WAVEFORM_MODE_TOTAL_NUM; i++) {
		pp_write(pkt, wf_lut_addr_reg[i],
			current_waveform[i].start_addr);
		pp_write(pkt, wf_lut_len_reg[i], current_waveform[i].len);
		TCON_LVL_LOG(TCON_LVL_PIPELINE,"id:%d wf_mode:%d, start_addr:0x%08x",
			i, current_waveform[i].waveform_mode,
			current_waveform[i].start_addr
			);
		TCON_LVL_LOG(TCON_LVL_PIPELINE,"wf_len:%d wf_temp_zone:%d",
			current_waveform[i].len,
			current_waveform[i].temperature_zone);
	}
}

void wf_lut_config_common(struct cmdqRecStruct *pkt,
			  struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_ROI_SIZE,
		 (wf_lut_config->height << 16) | wf_lut_config->width);
	pp_write(pkt, WF_LUT_SRC_CON, wf_lut_config->rdma_enable_mask & 0xf);

	pp_write(pkt, WF_LUT_DATAPATH_CON, 0x40000091);
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
				wf_lut_config->layer_greq_num << 26,
							GENMASK(31, 26));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
				wf_lut_config->checksum_sel << 8,
							GENMASK(10, 8));
	/* 0 use dpi crc, 1 use wf_lut crc */
	if (wf_lut_config->checksum_mode)
		pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
				0x0 << 11, BIT_MASK(11));
	else
		pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
				0x1 << 11, BIT_MASK(11));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
				wf_lut_config->rg_lut_end_sel << 7,
				BIT_MASK(7));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
				wf_lut_config->rg_de_sel << 6, BIT_MASK(6));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
				wf_lut_config->checksum_en << 4, BIT_MASK(4));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
				wf_lut_config->layer_smi_id_en, BIT_MASK(0));
	/* pp_write(pkt, WF_LUT_DATAPATH_CON, 0x40000891); */

}

void wf_lut_config_inter_rdma(struct cmdqRecStruct *pkt,
				struct wf_lut_con_config *wf_lut_config)
{
	unsigned int rdma_control_value = 0x0;

	/* bit8 need set to 1 */
	rdma_control_value = (wf_lut_config->byte_swap << 24) |
		(wf_lut_config->DECFMT << 12) |
		(wf_lut_config->H_FLIP_EN << 10) |
		(wf_lut_config->V_FLIP_EN << 9) | 0x00000100;

	pp_write(pkt, WF_LUT_L0_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L1_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L2_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L3_CON, rdma_control_value);

	if (!wf_lut_config->direct_link) {
		pp_write(pkt, WF_LUT_L0_SRC_SIZE,
			 wf_lut_config->wb_rdma[0].
			 height << 16 | wf_lut_config->wb_rdma[0].width);
		pp_write(pkt, WF_LUT_L0_OFFSET,
			 wf_lut_config->wb_rdma[0].y << 16 | wf_lut_config->
			 wb_rdma[0].x);
		pp_write(pkt, WF_LUT_L0_ADDR,
			 wf_lut_config->wb_rdma[0].start_addr);

		pp_write(pkt, WF_LUT_L1_SRC_SIZE,
			 wf_lut_config->wb_rdma[1].
			 height << 16 | wf_lut_config->wb_rdma[1].width);
		pp_write(pkt, WF_LUT_L1_OFFSET,
			 wf_lut_config->wb_rdma[1].y << 16 | wf_lut_config->
			 wb_rdma[1].x);
		pp_write(pkt, WF_LUT_L1_ADDR,
			 wf_lut_config->wb_rdma[1].start_addr);

		pp_write(pkt, WF_LUT_L2_SRC_SIZE,
			 wf_lut_config->wb_rdma[2].
			 height << 16 | wf_lut_config->wb_rdma[2].width);
		pp_write(pkt, WF_LUT_L2_OFFSET,
			 wf_lut_config->wb_rdma[2].y << 16 | wf_lut_config->
			 wb_rdma[2].x);
		pp_write(pkt, WF_LUT_L2_ADDR,
			 wf_lut_config->wb_rdma[2].start_addr);

		pp_write(pkt, WF_LUT_L3_SRC_SIZE,
			 wf_lut_config->wb_rdma[3].
			 height << 16 | wf_lut_config->wb_rdma[3].width);
		pp_write(pkt, WF_LUT_L3_OFFSET,
			 wf_lut_config->wb_rdma[3].y << 16 | wf_lut_config->
			 wb_rdma[3].x);
		pp_write(pkt, WF_LUT_L3_ADDR,
			 wf_lut_config->wb_rdma[3].start_addr);
	}

	pp_write(pkt, WF_LUT_L0_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA0_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA0_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA0_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L1_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA1_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA1_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA1_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L2_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA2_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA2_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA2_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L3_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA3_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA3_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA3_FIFO_CTRL, 0x00800000);

}

void wf_lut_enable(struct cmdqRecStruct *pkt,
		   struct wf_lut_con_config *wf_lut_config)
{
	pp_write_mask(pkt, 0x10005760, 1<<12, GENMASK(14, 12));
	pp_write(pkt, WF_LUT_INTEN, wf_lut_config->wf_lut_inten);
}

void wf_lut_config_link_mode(struct cmdqRecStruct *pkt,
				struct wf_lut_con_config *wf_lut_config)
{
	/* not setting,direct link alway using this setting */
	if (wf_lut_config->direct_link)
		pp_write(pkt, WF_LUT_LINK_MODE, 0xE4380ff2);
	else
		pp_write(pkt, WF_LUT_LINK_MODE, 0x00000002);

}

void wf_lut_config_base_addr(struct cmdqRecStruct *pkt,
				struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_LINK_BASE_ADDR, wf_lut_config->base_addr);
	pp_write(pkt, WF_LUT_LINK_BASE_ADDR_1, wf_lut_config->base_addr1);
}

u8 wf_lut_get_8bit(void)
{
	return (pp_read(WF_LUT_CON_VA) >> 12) & 0x1;
}

u8 wf_lut_get_hflip(void)
{
	return ((pp_read(WF_LUT_L0_CON_VA) >> 10) & 0x1)
		|| ((pp_read(WF_LUT_L1_CON_VA) >> 10) & 0x1)
		|| ((pp_read(WF_LUT_L2_CON_VA) >> 10) & 0x1)
		|| ((pp_read(WF_LUT_L3_CON_VA) >> 10) & 0x1);
}

u8 wf_lut_get_vflip(void)
{
	return ((pp_read(WF_LUT_L0_CON_VA) >> 9) & 0x1)
		|| ((pp_read(WF_LUT_L1_CON_VA) >> 9) & 0x1)
		|| ((pp_read(WF_LUT_L2_CON_VA) >> 9) & 0x1)
		|| ((pp_read(WF_LUT_L3_CON_VA) >> 9) & 0x1);
}


void wf_lut_config_lut_con(struct cmdqRecStruct *pkt,
				struct wf_lut_con_config *wf_lut_config)
{
	pp_write_mask(pkt, WF_LUT_CON, wf_lut_config->gray_mode, GENMASK(2, 0));
	pp_write_mask(pkt, WF_LUT_CON,
				wf_lut_config->rg_8b_out << 12, BIT_MASK(12));
	pp_write_mask(pkt, WF_LUT_CON,
				wf_lut_config->rg_partial_up_en << 19,
				BIT_MASK(19));
	pp_write_mask(pkt, WF_LUT_CON,
				wf_lut_config->rg_partial_up_val << 20,
				GENMASK(23, 20));
	pp_write_mask(pkt, WF_LUT_CON,
				wf_lut_config->rg_default_val << 8,
				GENMASK(11, 8));
	/* ultra close for test 16bit */
	#if 1
	pp_write_mask(pkt, WF_LUT_CON,
			0x3 << 25, GENMASK(26, 25));
	pp_write_mask(pkt, WF_LUT_CON,
			0x1 << 24, BIT_MASK(24));

#if 0
	pp_write_mask(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING2,
		  0x80 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING2,
		  0x80 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING2,
		  0x80 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING2,
		  0x80 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING2,
		  0x40 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING2,
		  0x40 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING2,
		  0x40 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING2,
		  0x40 << 24, GENMASK(31, 24));
#else

	pp_write_mask(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING2,
		  0x0 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING2,
		  0x0 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING2,
		  0x0 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING2,
		  0x0 << 16, GENMASK(23, 16));

	pp_write_mask(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING2,
		  0x0 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING2,
		  0x0 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING2,
		  0x0 << 24, GENMASK(31, 24));

	pp_write_mask(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING2,
		  0x0 << 24, GENMASK(31, 24));
#endif
	#endif
}

void wf_lut_config_mout(struct cmdqRecStruct *pkt,
			struct wf_lut_con_config *wf_lut_config)
{
	if (wf_lut_config->rg_8b_out) {
		pp_write(pkt, WF_LUT_MOUT, 0x00107001);
		pp_write_mask(pkt, WF_LUT_MOUT,
				wf_lut_config->wf_lut_mout, GENMASK(1, 0));
	} else {
		pp_write(pkt, WF_LUT_MOUT, 0x00107241);
		pp_write_mask(pkt, WF_LUT_MOUT,
				wf_lut_config->wf_lut_mout, GENMASK(1, 0));
	}
}

void wf_lut_config_wf_lut_checksum(struct cmdqRecStruct *pkt,
				unsigned int enable, unsigned int sel)
{
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, sel << 8, GENMASK(11, 8));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, enable << 4, BIT_MASK(4));
}

unsigned int wf_lut_convert_order(unsigned char *addr)
{
	unsigned int value = 0x00;

	value = ((*addr)<<24) + (*(addr+1)<<16) + (*(addr+2)<<8) + *(addr+3);
	return value;
}

int wf_lut_waveform_get_temperature_index(int temperature)
{
	int i = 0;
	int ts_index = 9;
	int ts_total_number = *((char *)hwtcon_fb_info()->waveform_va +
		WAVEFORM_TS_NUM_TO_BEGIN);

	if (ts_total_number <= 0
		|| ts_total_number > ARRAY_SIZE(g_ts_threshold)) {
		TCON_ERR("ts_total_number error: %d", ts_total_number);
		WARN_ON(1);
		return ts_index;
	}

	for (i = 0; i < ts_total_number; i++)
		g_ts_threshold[i] = *((char *)hwtcon_fb_info()->waveform_va +
		WAVEFORM_TS_TO_BEGIN + i);

	for (i = 0; i < ts_total_number; i++) {
		if (temperature < g_ts_threshold[i]) {
			ts_index = i;
			break;
		}
	}

	TCON_LVL_LOG(TCON_LVL_PIPELINE,"ts_index:%d", ts_index);
	return ts_index;
}

void wf_lut_waveform_table_init(void)
{
	#if 1
	int i = 0;
	int j = 0;

	memset((unsigned char *)&g_waveform_table, 0x00,
		TEMPERATURE_NUM * WAVEFORM_MODE_TOTAL_NUM *
		sizeof(struct wf_lut_waveform));
	for (i = 0; i < TEMPERATURE_NUM; i++) {
		for (j = 0; j < WAVEFORM_MODE_TOTAL_NUM; j++) {
			g_waveform_table[i][j].temperature_zone = i;
			g_waveform_table[i][j].waveform_mode = j;
			g_waveform_table[i][j].start_addr =
			hwtcon_fb_info()->waveform_pa +
			wf_lut_convert_order((char *)hwtcon_fb_info()->
				waveform_va
			+ WAVEFORM_ADDR_OFFSET_TO_BEGIN +
			i * WAVEFORM_ADDR_OFFSET_PER_TEMP + 4 * j);
			/* be carefore for point ++ */
			g_waveform_table[i][j].start_addr_va =
			hwtcon_fb_info()->waveform_va +
			wf_lut_convert_order(hwtcon_fb_info()->waveform_va
				+ WAVEFORM_ADDR_OFFSET_TO_BEGIN +
				i * WAVEFORM_ADDR_OFFSET_PER_TEMP + 4 * j);
			#if 1
			g_waveform_table[i][j].len =
			wf_lut_convert_order((char *)hwtcon_fb_info()->
				waveform_va
			+ WAVEFORM_LEN_OFFSET_TO_BEGIN +
			i * WAVEFORM_LEN_OFFSET_PER_TEMP + 4 * j) / 0x100;
			#else
			g_waveform_table[i][j].len = 10 - j;
			#endif
		}
	}
	#else
	#if 1
	int i = 0;
	int j = 0;

	memset((unsigned char *)&g_waveform_table, 0x00,
			TEMPERATURE_NUM * WAVEFORM_MODE_TOTAL_NUM *
			sizeof(struct wf_lut_waveform));
	for (i = 0; i < TEMPERATURE_NUM; i++) {
		for (j = 0; j < WAVEFORM_MODE_TOTAL_NUM; j++) {
			g_waveform_table[i][j].waveform_mode = j;
			if (j == 0) {
				g_waveform_table[i][j].start_addr =
				hwtcon_fb_info()->waveform_pa;
				g_waveform_table[i][j].start_addr_va =
					hwtcon_fb_info()->waveform_va;
			} else {
				g_waveform_table[i][j].start_addr =
				g_waveform_table[i][j-1].start_addr +
					(1 + 2 * (j - 1)) * 0x100;
				g_waveform_table[i][j].start_addr_va =
					(u32 *)((u8 *)g_waveform_table
					[i][j-1].start_addr_va +
					(1 + 2 * (j - 1)) * 0x100);
			}
			/* test for waveform len */
			g_waveform_table[i][j].len = (1 + 2 * j);
		}
	}
	#else
	int i = 0;
	int j = 0;

	memset((unsigned char *)&g_waveform_table, 0x00,
			TEMPERATURE_NUM * WAVEFORM_MODE_TOTAL_NUM *
			sizeof(struct wf_lut_waveform));
	for (i = 0; i < TEMPERATURE_NUM; i++) {
		for (j = 0; j < WAVEFORM_MODE_TOTAL_NUM; j++) {
			g_waveform_table[i][j].waveform_mode = j;
			g_waveform_table[i][j].start_addr =
			hwtcon_fb_info()->waveform_pa;
			g_waveform_table[i][j].start_addr_va =
				hwtcon_fb_info()->waveform_va;
			g_waveform_table[i][j].len = 39;
		}
	}
	#endif
	#endif
	TCON_LVL_LOG(TCON_LVL_PIPELINE,"wf_lut_waveform_table_init!");
}

static void wf_lut_waveform_replace(struct cmdqRecStruct *pkt,
		enum WF_SLOT_ENUM hw_slot, enum WF_FILE_MODE_ENUM wf_file_mode)
{
	struct wf_lut_waveform *current_waveform = NULL;
	const u32 wf_lut_addr_reg[16] = {
		WF_LUT_ADDR_0, WF_LUT_ADDR_1, WF_LUT_ADDR_2, WF_LUT_ADDR_3,
		WF_LUT_ADDR_4, WF_LUT_ADDR_5, WF_LUT_ADDR_6, WF_LUT_ADDR_7,
		WF_LUT_ADDR_8, WF_LUT_ADDR_9, WF_LUT_ADDR_10, WF_LUT_ADDR_11,
		WF_LUT_ADDR_12, WF_LUT_ADDR_13, WF_LUT_ADDR_14, WF_LUT_ADDR_15,
	};
	const u32 wf_lut_len_reg[16] = {
		WF_LUT_LEN_0, WF_LUT_LEN_1, WF_LUT_LEN_2, WF_LUT_LEN_3,
		WF_LUT_LEN_4, WF_LUT_LEN_5, WF_LUT_LEN_6, WF_LUT_LEN_7,
		WF_LUT_LEN_8, WF_LUT_LEN_9, WF_LUT_LEN_10, WF_LUT_LEN_11,
		WF_LUT_LEN_12, WF_LUT_LEN_13, WF_LUT_LEN_14, WF_LUT_LEN_15,
	};

	current_waveform =
			&g_waveform_table[g_current_temperature][wf_file_mode];

	if (hw_slot < WF_SLOT_0 || hw_slot > WF_SLOT_15) {
		TCON_ERR("invalid slot[%d]", hw_slot);
		return;
	}

	pp_write(pkt, wf_lut_addr_reg[hw_slot],
		current_waveform->start_addr);
	pp_write(pkt, wf_lut_len_reg[hw_slot], current_waveform->len);
}


#define INVALID_MODE 0x1000

const static struct wf_mode_relation wf_lut_mode_association[] = {
		{WAVEFORM_MODE_INIT, WF_SLOT_0, WF_FILE_MODE_INIT}, /* 0 */
		{WAVEFORM_MODE_DU, WF_SLOT_1, WF_FILE_MODE_DU},	/* 1 */
		{WAVEFORM_MODE_GC16, WF_SLOT_2, WF_FILE_MODE_GC16},	/* 2 */
		{WAVEFORM_MODE_GL16, WF_SLOT_3, WF_FILE_MODE_GL16},	/* 3 */
		{WAVEFORM_MODE_GLR16, WF_SLOT_4, WF_FILE_MODE_GLR16},	/* 4 */
		{WAVEFORM_MODE_A2, WF_SLOT_6, WF_FILE_MODE_A2},	/* 6 */
		{WAVEFORM_MODE_GCK16, WF_SLOT_8, WF_FILE_MODE_GCK16},	/* 8 */
		{WAVEFORM_MODE_GLKW16,WF_SLOT_9, WF_FILE_MODE_GCKW16},	/* 9 */
};

const static struct wf_mode_relation wf_lut_mode_association_0x54[] = {
		{WAVEFORM_MODE_INIT, WF_SLOT_0, 0},
		{WAVEFORM_MODE_DU, WF_SLOT_1, 1},
		{WAVEFORM_MODE_GC16, WF_SLOT_2, 2},
		{WAVEFORM_MODE_GL16, WF_SLOT_2, 3},	
		{WAVEFORM_MODE_GLR16,WF_SLOT_3 , 3},
		{WAVEFORM_MODE_A2, WF_SLOT_3, 5},
};

// Generic CFA waveform list :
const static struct wf_mode_relation wf_lut_mode_association_0x16[] = {
		{WAVEFORM_MODE_INIT, WF_SLOT_0, 0},
		{WAVEFORM_MODE_DU, WF_SLOT_1, 1},
		{WAVEFORM_MODE_GC16, WF_SLOT_2, 2},
		{WAVEFORM_MODE_GL16, WF_SLOT_3, 3},	
		{WAVEFORM_MODE_GLR16,WF_SLOT_4 , 4},
		{WAVEFORM_MODE_A2, WF_SLOT_6, 6},
		{WAVEFORM_MODE_GCK16, WF_SLOT_2, 2},
		{WAVEFORM_MODE_GLKW16,WF_SLOT_2, 2},
};

// Formal CFA waveform list for EC070KH2 & EC060KH5 :
const static struct wf_mode_relation wf_lut_mode_association_0x72[] = {
		{WAVEFORM_MODE_INIT, WF_SLOT_0, 0},
		{WAVEFORM_MODE_DU, WF_SLOT_1, 1},
		{WAVEFORM_MODE_GC16, WF_SLOT_2, 2},
		{WAVEFORM_MODE_GL16, WF_SLOT_3, 3},	
		{WAVEFORM_MODE_GLR16,WF_SLOT_4 , 4},
		{WAVEFORM_MODE_A2, WF_SLOT_6, 6},
		{WAVEFORM_MODE_GCK16, WF_SLOT_8, 8},
		{WAVEFORM_MODE_GLKW16,WF_SLOT_9, 9},
		{WAVEFORM_MODE_GCC16, WF_SLOT_10, 10},
		{WAVEFORM_MODE_GLRC16, WF_SLOT_11, 11},
};



const static struct wf_mode_table gwf_mode_tableA[] ={
	{0x58,sizeof(wf_lut_mode_association)/sizeof(wf_lut_mode_association[0]),
		(struct wf_mode_relation *)wf_lut_mode_association},
	{0x54,sizeof(wf_lut_mode_association_0x54)/sizeof(wf_lut_mode_association_0x54[0]),
		(struct wf_mode_relation *)wf_lut_mode_association_0x54},
	{0x16,sizeof(wf_lut_mode_association_0x16)/sizeof(wf_lut_mode_association_0x16[0]),
		(struct wf_mode_relation *)wf_lut_mode_association_0x16},
	{0x72,sizeof(wf_lut_mode_association_0x72)/sizeof(wf_lut_mode_association_0x72[0]),
		(struct wf_mode_relation *)wf_lut_mode_association_0x72},
};

static struct wf_mode_relation * _get_wf_mode_tab(u8 bModeVer, int *O_piTabSize)
{
	int i;
	struct wf_mode_relation *ptwf_tab=0;
	for(i=0;i<sizeof(gwf_mode_tableA)/sizeof(gwf_mode_tableA[0]);i++) {
		if(bModeVer==gwf_mode_tableA[i].wf_mode_ver) {
			ptwf_tab = gwf_mode_tableA[i].wf_mode_tab;
			if(O_piTabSize) {
				*O_piTabSize = gwf_mode_tableA[i].wf_tab_size;
			}
		}
	}
	if(ptwf_tab) {
		return ptwf_tab;
	}
	else {
		TCON_ERR("cannot find waveform mode table by mode version 0x%x",bModeVer);
		return (struct wf_mode_relation *)wf_lut_mode_association;
	}
}


enum WF_SLOT_ENUM wf_lut_get_waveform_mode_slot(
	enum WAVEFORM_MODE_ENUM wf_mode,
	int night_mode,int * eink_wfm)
{
	int i = 0;
	struct wf_mode_relation *ptWF_tab = 0;
	int iWF_tab_size;
	
	//night_mode = night_mode ;
	ptWF_tab = gptWF_tab;
	iWF_tab_size = giWF_tab_items;
	if(!ptWF_tab) {
		TCON_ERR("waveform mode table cannot found ! wf_mode=%d,night_mode=%d",
			wf_mode, night_mode);
		return WF_SLOT_0;
	}
	for (i = 0; i < iWF_tab_size; i++) {
		if (ptWF_tab[i].wf_mode == wf_mode) {
			if (ptWF_tab[i].slot != INVALID_MODE) {
				if(eink_wfm) {
					*eink_wfm = ptWF_tab[i].wf_file_mode;
				}
				return ptWF_tab[i].slot;
			}
			TCON_ERR("waveform mode[%d] not loaded night_mode[%d]",
				wf_mode, night_mode);
			return WF_SLOT_0;
		}
	}
	TCON_ERR("invalid waveform mode[%d]",wf_mode);
	return WF_SLOT_0;
}

#if 0
void wf_lut_waveform_day_mode_slot(struct cmdqRecStruct *pkt)
{
	TCON_ERR("enter");
	wf_lut_waveform_replace(pkt, WF_SLOT_0, WF_FILE_MODE_INIT);
	wf_lut_waveform_replace(pkt, WF_SLOT_1, WF_FILE_MODE_DU);
	wf_lut_waveform_replace(pkt, WF_SLOT_2, WF_FILE_MODE_GC16);
	wf_lut_waveform_replace(pkt, WF_SLOT_3, WF_FILE_MODE_GL16);
	wf_lut_waveform_replace(pkt, WF_SLOT_4, WF_FILE_MODE_GLR16);
	wf_lut_waveform_replace(pkt, WF_SLOT_5, WF_FILE_MODE_A2);

}

void wf_lut_waveform_night_mode_slot(struct cmdqRecStruct *pkt)
{
	TCON_ERR("enter");
	wf_lut_waveform_replace(pkt, WF_SLOT_0, WF_FILE_MODE_INIT);
	wf_lut_waveform_replace(pkt, WF_SLOT_1, WF_FILE_MODE_DU);
	wf_lut_waveform_replace(pkt, WF_SLOT_2, WF_FILE_MODE_GL16);
	wf_lut_waveform_replace(pkt, WF_SLOT_3, WF_FILE_MODE_A2);
	wf_lut_waveform_replace(pkt, WF_SLOT_4, WF_FILE_MODE_GCK16);
	wf_lut_waveform_replace(pkt, WF_SLOT_5, WF_FILE_MODE_GCKW16);
}
#endif 
void wf_lut_waveform_slot_association(struct cmdqRecStruct *pkt,
	unsigned int mode, unsigned int temp)
{
	int i = 0;
	struct wf_mode_relation *ptWF_tab = 0;
	int iWF_tab_size;
	
	ptWF_tab = gptWF_tab;
	iWF_tab_size = giWF_tab_items;
	if(!ptWF_tab) {
		TCON_ERR("WF table not avalible !");
		return ;
	}

	/* temperature assigned should insert here */
	g_current_temperature = temp;

	for (i = 0; i < iWF_tab_size; i++) {
		if (ptWF_tab[i].slot != INVALID_MODE)
			wf_lut_waveform_replace(pkt,ptWF_tab[i].slot,ptWF_tab[i].wf_file_mode);
	}
}


struct wf_lut_waveform *wf_lut_read_waveform_by_slot(int slot_index)
{
	int i, j;
	char *lut_addr[WAVEFORM_MODE_TOTAL_NUM] = {
		WF_LUT_ADDR_0_VA, WF_LUT_ADDR_1_VA,
		WF_LUT_ADDR_2_VA, WF_LUT_ADDR_3_VA,
		WF_LUT_ADDR_4_VA, WF_LUT_ADDR_5_VA,
		WF_LUT_ADDR_6_VA, WF_LUT_ADDR_7_VA,
		WF_LUT_ADDR_8_VA, WF_LUT_ADDR_9_VA,
		WF_LUT_ADDR_10_VA, WF_LUT_ADDR_11_VA,
		WF_LUT_ADDR_12_VA, WF_LUT_ADDR_13_VA,
		WF_LUT_ADDR_14_VA, WF_LUT_ADDR_15_VA};

	if (slot_index < 0 || slot_index >= WAVEFORM_MODE_TOTAL_NUM) {
		TCON_ERR("invalid slot index:%d", slot_index);
		return NULL;
	}

	for (i = 0; i < TEMPERATURE_NUM; i++)
		for (j = 0; j < WAVEFORM_MODE_TOTAL_NUM; j++)
			if (pp_read(lut_addr[slot_index]) ==
					g_waveform_table[i][j].start_addr)
				return &g_waveform_table[i][j];

	/* not found */
	TCON_ERR("%s slot:%d waveform addr:0x%08x found error!",
		__func__,
		slot_index,
		pp_read(lut_addr[slot_index]));

	return NULL;
}

void wf_lut_get_waveform_mode_in_hardware(void)
{
	int i = 0;
	/* update g_current_waveform_mode_in_HW:
	 * g_current_waveform_mode_in_HW store current loaded waveform
	 */
	for (i = 0; i < WAVEFORM_MODE_TOTAL_NUM; i++)
		g_current_waveform_mode_in_HW[i] =
				wf_lut_read_waveform_by_slot(i);
}

void wf_lut_print_current_loaded_wavefrom_info(void)
{
	struct wf_lut_waveform *waveform_info = NULL;
	int i = 0;

	wf_lut_get_waveform_mode_in_hardware();
	for (i = 0; i < WAVEFORM_MODE_TOTAL_NUM; i++) {
		waveform_info = g_current_waveform_mode_in_HW[i];
		if (waveform_info != NULL) {
			TCON_ERR("WF_LUT slot:%02d wf_mode:%02d",
				i, waveform_info->waveform_mode);
			TCON_ERR("wf_addr:0x%08x wf_len:%d wf_addr_va:%p",
				waveform_info->start_addr,
				waveform_info->len,
				waveform_info->start_addr_va);
			TCON_ERR("temp_zone:%d",
				waveform_info->temperature_zone);
			}
		else
			TCON_ERR("WF_LUT slot:%d waveform not loaded", i);
	}
}
#if 0
void wf_lut_waveform_select_by_temp(struct cmdqRecStruct *pkt, int temp)
{
	int index = 0;

	if (temp == g_current_temperature)
		return;

	TCON_LVL_LOG(TCON_LVL_PIPELINE,"temp change before temp:%d,after temp:%d\n",
		g_current_temperature, temp);
	/* get  before temperature waveform mode */
	wf_lut_get_waveform_mode_in_hardware();

	/* temperature assigned should insert here */
	g_current_temperature = temp;

	/* set  now temperature waveform mode */
	for (index = 0; index < WAVEFORM_MODE_TOTAL_NUM; index++) {
		if (g_current_waveform_mode_in_HW[index] != NULL)
			wf_lut_waveform_replace(pkt,
			index,
			g_current_waveform_mode_in_HW[index]->waveform_mode);
		else
		    TCON_ERR("slot index %d addr NULL bypass replace", index);
	}

}
#endif 
unsigned int wf_lut_get_waveform_len(int temp, int mode)
{
	struct wf_lut_waveform *current_waveform = NULL;

	if ((temp < 0) || (temp >= TEMPERATURE_NUM)) {
		TCON_ERR("invalid temp zone:%d", temp);
		return 0;
	}

	current_waveform =
		((struct wf_lut_waveform *)
		&g_waveform_table[temp][0])
			+ mode;

	return current_waveform->len;
}

#if 0
void wf_lut_config_waveform_v2(struct cmdqRecStruct *pkt)
{
	int index = 0;

	for (index = 0; index < WAVEFORM_MODE_TOTAL_NUM; index++) {
		if (g_current_waveform_mode_in_HW[index] != NULL)
			wf_lut_waveform_replace(pkt,
			index,
			g_current_waveform_mode_in_HW[index]->waveform_mode);
		else
		    TCON_ERR("slot index %d addr NULL bypass replace", index);
	}
}
#endif 

unsigned int wf_lut_get_rdma0_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_0_VA);
}

unsigned int wf_lut_get_rdma1_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_1_VA);
}

unsigned int wf_lut_get_rdma2_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_2_VA);
}

unsigned int wf_lut_get_rdma3_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_3_VA);
}

unsigned int wf_lut_get_wf_lut_output_checksum(void)
{
	return pp_read(WF_LUT_CHKSUM_4_VA);
}


u32 wf_lut_get_irq_status(void)
{

	return pp_read(WF_LUT_INTSTA_VA);
}

void wf_lut_clear_lut_end_irq_status(struct cmdqRecStruct *pkt)
{
	pp_write_mask(pkt, WF_LUT_CON, 0x01 << 15, BIT_MASK(15));
	pp_write_mask(pkt, WF_LUT_CON, 0x00 << 15, BIT_MASK(15));
}

void wf_lut_set_lut_info(struct cmdqRecStruct *pkt,
				int lut_id, int x, int y, int w, int h)
{
	pp_write(pkt, WF_LUT_INFO_XY_CFG, y|(x<<16));
	/*bit0-12:y bit13-15:id_2_0 bit16-28:x bit29-31:id_5_3*/

	pp_write(pkt, WF_LUT_INFO_WH_CFG, h | (w<<16));
	/*bit0-12:h bit13-15:id_2_0 bit16-28:w bit29-31:id_5_3*/

}

void wf_lut_set_lut_id_info(struct cmdqRecStruct *pkt,
				int lut_id, enum WF_SLOT_ENUM lut_mode)
{
	pp_write(pkt, WF_LUT_INFO_ID_CFG, lut_id << 4 | lut_mode);
}

void wf_lut_clear_irq_status(struct cmdqRecStruct *pkt)
{
	//pp_write(pkt, WF_LUT_INTSTA, 0x0);
	pp_write_mask(pkt, WF_LUT_INTSTA, 0x0, GENMASK(1, 0));
}

void wf_lut_common_8bit_setting(struct wf_lut_con_config *wf_lut_config)
{
	memset((char *)wf_lut_config, 0x00, sizeof(struct wf_lut_con_config));
	wf_lut_config->base_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->base_addr1 = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->gray_mode = GRAY_MODE_32_GRAY_LEVEL;/*y4 or y5*/
	wf_lut_config->width = hw_tcon_get_edp_width();
	wf_lut_config->height = hw_tcon_get_edp_height();
	wf_lut_config->rdma_enable_mask = 0xf;
	wf_lut_config->DECFMT = 0x0;
	wf_lut_config->checksum_en = 0x01;
	wf_lut_config->checksum_sel = 0x00;
	wf_lut_config->checksum_mode = 0x00;
	wf_lut_config->H_FLIP_EN = 0x00;
	wf_lut_config->V_FLIP_EN = 0x00;
	wf_lut_config->wf_lut_en = 0x01;
	wf_lut_config->wf_lut_inten = 0x02;
	wf_lut_config->rg_default_val = 0x00;
	wf_lut_config->rg_partial_up_en = 0x00;
	wf_lut_config->rg_partial_up_val = 0x00;
	wf_lut_config->layer_greq_num = 0x10;
	wf_lut_config->layer_smi_id_en = 0x01;
	wf_lut_config->rg_8b_out = 0x01; /*8bit or 16bit*/
	wf_lut_config->byte_swap = 0x02;
	wf_lut_config->rg_lut_end_sel = 0x01;
	wf_lut_config->direct_link = 0x01;

	/*bit 0 output to dpi,bit1 output to wdma*/
	wf_lut_config->wf_lut_mout = WF_LUT_MOUT_WDMA;
	wf_lut_config->temperature_index = 0x6;
	wf_lut_config->waveform_table_current =
		(struct wf_lut_waveform *)&g_waveform_table[
			wf_lut_config->temperature_index][0];

	#ifdef WAVEFORM_M4U_TEST
	wf_lut_config->wb_rdma[0].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[0].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[0].start_addr
		= hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;

	wf_lut_config->wb_rdma[1].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[1].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[1].start_addr
		= hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;

	wf_lut_config->wb_rdma[2].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[2].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[2].start_addr
		= hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;

	wf_lut_config->wb_rdma[3].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[3].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[3].start_addr
		= hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;

	#else
	wf_lut_config->wb_rdma[0].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[0].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[0].start_addr
		= hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;

	wf_lut_config->wb_rdma[1].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[1].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[1].start_addr
		= hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;

	wf_lut_config->wb_rdma[2].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[2].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[2].start_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;

	wf_lut_config->wb_rdma[3].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[3].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[3].start_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;
	#endif
}

void wf_lut_common_16bit_setting(struct wf_lut_con_config *wf_lut_config)
{
	memset((char *)wf_lut_config, 0x00, sizeof(struct wf_lut_con_config));
	wf_lut_config->base_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->base_addr1 = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->gray_mode = GRAY_MODE_32_GRAY_LEVEL;/*y4 or y5*/
	wf_lut_config->width = hw_tcon_get_edp_width();
	wf_lut_config->height = hw_tcon_get_edp_height();
	/* this one be careful */
	wf_lut_config->rdma_enable_mask = 0xF;
	wf_lut_config->DECFMT = 0x0;
	wf_lut_config->checksum_en = 0x01;
	wf_lut_config->checksum_sel = 0x00;
	wf_lut_config->checksum_mode = 0x00;
	wf_lut_config->H_FLIP_EN = 0x00;
	wf_lut_config->V_FLIP_EN = 0x00;
	wf_lut_config->wf_lut_en = 0x01;
	wf_lut_config->wf_lut_inten = 0x02;
	wf_lut_config->rg_default_val = 0x00;
	wf_lut_config->rg_partial_up_en = 0x00;
	wf_lut_config->rg_partial_up_val = 0x00;
	wf_lut_config->layer_greq_num = 0x10;
	wf_lut_config->layer_smi_id_en = 0x01;
	wf_lut_config->rg_8b_out = 0x00; /*8bit or 16bit*/
	wf_lut_config->byte_swap = 0x02;
	wf_lut_config->rg_lut_end_sel = 0x01;
	wf_lut_config->direct_link = 0x01;
	/*bit 0 output to dpi,bit1 output to wdma*/
	wf_lut_config->wf_lut_mout = WF_LUT_MOUT_WDMA;
	wf_lut_config->temperature_index = 0x6;
	wf_lut_config->waveform_table_current =
		(struct wf_lut_waveform *)&g_waveform_table[
				wf_lut_config->temperature_index][0];

	#ifdef WAVEFORM_M4U_TEST
	wf_lut_config->wb_rdma[0].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[0].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[0].start_addr =
		hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;

	wf_lut_config->wb_rdma[1].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[1].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[1].start_addr =
		hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;

	wf_lut_config->wb_rdma[2].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[2].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[2].start_addr =
		hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;

	wf_lut_config->wb_rdma[3].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[3].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[3].start_addr =
		hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;

	#else
	wf_lut_config->wb_rdma[0].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[0].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[0].start_addr =
			hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;

	wf_lut_config->wb_rdma[1].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[1].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[1].start_addr =
			hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;

	wf_lut_config->wb_rdma[2].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[2].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[2].start_addr =
			hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;

	wf_lut_config->wb_rdma[3].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[3].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[3].start_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;
	#endif

}

void wf_lut_common_16bit_setting_old(struct wf_lut_con_config *wf_lut_config)
{
	memset((char *)wf_lut_config, 0x00, sizeof(struct wf_lut_con_config));

	wf_lut_config->base_addr = 0x00000000;
	wf_lut_config->base_addr1 = 0x00000000;
	wf_lut_config->gray_mode = GRAY_MODE_32_GRAY_LEVEL;/*y4 or y5*/
	wf_lut_config->width = hw_tcon_get_edp_width();
	wf_lut_config->height = hw_tcon_get_edp_height();
	wf_lut_config->rdma_enable_mask = 0x1;
	wf_lut_config->DECFMT = 0x0;
	wf_lut_config->checksum_en = 0x01;
	wf_lut_config->checksum_sel = 0x00;
	wf_lut_config->checksum_mode = 0x00;
	wf_lut_config->rg_de_sel = 0x01;
	wf_lut_config->H_FLIP_EN = 0x00;
	wf_lut_config->V_FLIP_EN = 0x00;
	wf_lut_config->wf_lut_en = 0x01;
	wf_lut_config->wf_lut_inten = 0x02;
	wf_lut_config->rg_default_val = 0x00;
	wf_lut_config->rg_partial_up_en = 0x00;
	wf_lut_config->rg_partial_up_val = 0x00;
	wf_lut_config->layer_greq_num = 0x10;
	wf_lut_config->layer_smi_id_en = 0x01;
	wf_lut_config->rg_8b_out = 0x00; /*8bit or 16bit*/
	wf_lut_config->byte_swap = 0x02;
	wf_lut_config->rg_lut_end_sel = 0x01;

	/*bit 0 output to dpi,bit1 output to wdma*/
	wf_lut_config->wf_lut_mout = WF_LUT_MOUT_WDMA;
	wf_lut_config->temperature_index = 0x8;
	wf_lut_config->waveform_table_current =
		(struct wf_lut_waveform *)&g_waveform_table[
				wf_lut_config->temperature_index][0];

	#ifdef WAVEFORM_M4U_TEST
	wf_lut_config->wb_rdma[0].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[0].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[0].start_addr =
		hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;

	wf_lut_config->wb_rdma[1].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[1].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[1].start_addr =
		hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;

	wf_lut_config->wb_rdma[2].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[2].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[2].start_addr =
		hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;

	wf_lut_config->wb_rdma[3].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[3].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[3].start_addr =
		hwtcon_fb_info()->wb_buffer_pa_mva;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;

	#else
	wf_lut_config->wb_rdma[0].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[0].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[0].start_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;

	wf_lut_config->wb_rdma[1].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[1].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[1].start_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;

	wf_lut_config->wb_rdma[2].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[2].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[2].start_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;

	wf_lut_config->wb_rdma[3].width = hw_tcon_get_edp_width();
	wf_lut_config->wb_rdma[3].height = hw_tcon_get_edp_height();
	wf_lut_config->wb_rdma[3].start_addr = hwtcon_fb_info()->wb_buffer_pa;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;
	#endif

}


void wf_lut_config_lut_info(struct cmdqRecStruct *pkt,
						struct lut_info_para *lut_info)
{
	int i = 0;

	for (i = 0; i < 64; i++) {
		if (lut_info[i].valid) {
			wf_lut_set_lut_info(NULL, i, lut_info[i].x,
				lut_info[i].y, lut_info[i].w, lut_info[i].h);
			wf_lut_set_lut_id_info(NULL, i,
				lut_info[i].waveform_mode);
		}
	}
	pp_write_mask(NULL, WF_LUT_SHADOW_UP, 0x1<<0, BIT_MASK(0));
}

void wf_lut_config_tcon_end_counter(struct cmdqRecStruct *pkt,
	struct wf_lut_con_config *wf_lut_config)
{
	/* delay dpi_time = 6 dpi counter * 3 */
	u32 delay_dpi_time = hw_tcon_get_edp_dpi_cnt_off() * 3;

	#if 0
	pp_write_mask(pkt, PAPER_TCTOP_TCON_POS_CFG, 0 << 31, BIT_MASK(31));
	if (wf_lut_config->rg_8b_out) {
		/* config TCON end counter */
		pp_write(pkt, PAPER_TCTOP_TCON_POS_CFG,
			(wf_lut_config->height +
				hw_tcon_get_edp_blank_vtotal() - 1) << 16 |
			(wf_lut_config->width / 4));
	} else {
		/* config TCON end counter */
		pp_write(pkt, PAPER_TCTOP_TCON_POS_CFG,
			(wf_lut_config->height +
				hw_tcon_get_edp_blank_vtotal() - 1) << 16 |
			(wf_lut_config->width / 8));
		TCON_ERR("config vtotal: %d value:0x%08x",
			(wf_lut_config->height +
				hw_tcon_get_edp_blank_vtotal() - 1),
			(wf_lut_config->height +
				hw_tcon_get_edp_blank_vtotal() - 1) << 16 |
			(wf_lut_config->width / 8));
	}
	#else
	pp_write_mask(pkt, PAPER_TCTOP_TCON_POS_CFG, 1 << 31, BIT_MASK(31));

	pp_write_mask(pkt, PAPER_TCTOP_TCON_POS_CFG,
		(delay_dpi_time & GENMASK(11, 0)), GENMASK(11, 0));
	pp_write_mask(pkt, PAPER_TCTOP_TCON_POS_CFG,
		(delay_dpi_time >> 12) << 16, GENMASK(23, 16));
	#endif
}

void wf_lut_config_context_test_without_trigger(struct cmdqRecStruct *pkt,
	struct wf_lut_con_config *wf_lut_config, struct lut_info_para *lut_info)
{
	pp_write(NULL, 0x14000100, 0x0);

	wf_lut_waveform_table_init();

	wf_lut_config_auto_off_tcon_dpi_signal(pkt, true);

	wf_lut_config_tcon_end_counter(pkt, wf_lut_config);

	wf_lut_config_mmsys(pkt, wf_lut_config);

	/*smi config in rdma already config*/
	wf_lut_rdma_config_smi_setting(pkt);

	wf_lut_config_waveform(pkt, wf_lut_config->waveform_table_current);

	wf_lut_config_link_mode(pkt, wf_lut_config);

	wf_lut_config_base_addr(pkt, wf_lut_config);

	/*lut enable info from pipeline in direct link mode, not need config*/

	wf_lut_config_mout(pkt, wf_lut_config);

	wf_lut_config_common(pkt, wf_lut_config);

	wf_lut_config_inter_rdma(pkt, wf_lut_config);

	wf_lut_config_lut_con(pkt, wf_lut_config);

	wf_lut_enable(pkt, wf_lut_config);

	/*wdma*/
	if (wf_lut_config->rg_8b_out) {
		if (wf_lut_config->wf_lut_mout == WF_LUT_MOUT_WDMA) {
			Wf_Lut_Wdma_Config(pkt,
				wf_lut_config->width/4, wf_lut_config->height);
		}

		/*rdma*/
		wf_lut_rdma_config_rdma(pkt,
			wf_lut_config->width/4, wf_lut_config->height, 2, 0);

		/*dpi config*/
		wf_lut_config_dpi_context(pkt,
			wf_lut_config->width/4, wf_lut_config->height);

	} else {
		if (wf_lut_config->wf_lut_mout == WF_LUT_MOUT_WDMA) {
			Wf_Lut_Wdma_Config(pkt,
				wf_lut_config->width/8, wf_lut_config->height);
		}

		/*rdma*/
		wf_lut_rdma_config_rdma(pkt, wf_lut_config->width/8,
			wf_lut_config->height, 2, 0);

		/*dpi config*/
		wf_lut_config_dpi_context(pkt, wf_lut_config->width/8,
			wf_lut_config->height);
	}

	pp_write_mask(NULL, WF_LUT_DATAPATH_CON, 0x1<<25, BIT_MASK(25));

	pp_write_mask(NULL, PAPER_TCTOP_IRQ_CTL, 0x1<<2, BIT_MASK(2));

	pipeline_config_enable_irq(NULL, IRQ_WF_LUT_TCON_END);

}

void wf_lut_config_context_test_trigger(struct cmdqRecStruct *pkt,
					struct lut_info_para *lut_info)
{
	wf_lut_config_lut_info(pkt, lut_info);

	pp_write_mask(NULL, WF_LUT_DATAPATH_CON, 0x1<<25, BIT_MASK(25));

	wf_lut_dpi_enable(pkt);
}

void swtcon_config_context(struct cmdqRecStruct *pkt)
{

	int value = 0;
	int frame_cnt = 0;
	/* pmic control */
	//pp_write(pkt, MMSYS_DUMMY1, 0x0000007F);
	TCON_LVL_LOG(TCON_LVL_PIPELINE,"enter swtcon_config_context!\n");
	pp_write_mask(pkt, PAPER_TCTOP_PIN_INV, 0x001b0000, GENMASK(20, 16));
	tcon_config_swtcon_pin(pkt);
	/*rdma */
	wf_lut_rdma_config_rdma(pkt, 192, 405, 2, 1);
	wf_lut_rdma_config_smi_setting(pkt);
	/*600*400=192,405 */
	/*1448*1072=0x1A0,0x435 */
	/*dpi config */
	wf_lut_config_dpi_context(pkt, 192, 405);
	while (1) {
		value = pp_read(WF_LUT_RDMA_INT_STATUS_VA);
		if ((value & 0x04) == 0x04) {
			TCON_ERR("DPI_TEST_1: frame %d end!!\n", frame_cnt);
			if (frame_cnt >= 39) {
				TCON_ERR("DPI_TEST_1: frame %d FINISH!!\n",
					 frame_cnt);
				#if 0
				pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR,
					 0x56000000);
				#else
				pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR,
					 hwtcon_fb_info()->wb_buffer_pa);
				#endif
				break;
			}
			pp_write(NULL, WF_LUT_RDMA_INT_STATUS, 0x0);
			TCON_ERR("==>WAIT:rdma INT STATUS:0x%x\n",
				 pp_read(WF_LUT_RDMA_INT_STATUS_VA));
			pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR, 0x56000000);
			frame_cnt++;
		}
	}

}

void swdata_hwtcon_config_context(struct cmdqRecStruct *pkt)
{
	int value = 0;
	int frame_cnt = 0;
	/* pmic control */
	//pp_write(pkt, MMSYS_DUMMY1, 0x0000007F);
	TCON_LVL_LOG(TCON_LVL_PIPELINE,"enter swdata_hwtcon_config_context!\n");
	tcon_setting(pkt);
	/*rdma */
	wf_lut_rdma_config_rdma(pkt, 150, 400, 2, 1);
	wf_lut_rdma_config_smi_setting(pkt);
	/*600*400=150,400 */
	/*1448*1072=362,1072 */
	/*dpi config */
	wf_lut_config_dpi_context(pkt, 150, 400);
	while (1) {
		value = pp_read(WF_LUT_RDMA_INT_STATUS_VA);
		if ((value & 0x04) == 0x04) {
			frame_cnt++;
			TCON_ERR("DPI_TEST_1: frame %d end!!\n", frame_cnt);
			if (frame_cnt >= 39) {
				TCON_ERR("DPI_TEST_1: frame %d FINISH!!\n",
					 frame_cnt);
				pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR,
					 hwtcon_fb_info()->wb_buffer_pa+
					 frame_cnt * hw_tcon_get_edp_height() *
					 hw_tcon_get_edp_width());
				break;
			}
			pp_write(NULL, WF_LUT_RDMA_INT_STATUS, 0x0);
			TCON_ERR("==>WAIT:rdma INT STATUS:0x%x\n",
				 pp_read(WF_LUT_RDMA_INT_STATUS_VA));
			pp_write(NULL, WF_LUT_RDMA_MEM_START_ADDR,
				 hwtcon_fb_info()->wb_buffer_pa +
				 frame_cnt * hw_tcon_get_edp_height() *
				 hw_tcon_get_edp_width());
		}
	}

}


void hwtcon_edp_pinmux_control(struct platform_device *pdev)
{
	#ifndef FPGA_EARLY_PORTING
	g_pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(g_pctrl)) {
		TCON_ERR("devm_pinctrl_get error!\n");
		return;
	}

	g_pin_state_active = pinctrl_lookup_state(g_pctrl, "active");
	if (IS_ERR(g_pin_state_active)) {
		TCON_ERR("pinctrl_lookup_state active error!\n");
		return;
	}

	g_pin_state_inactive = pinctrl_lookup_state(g_pctrl, "inactive");
	if (IS_ERR(g_pin_state_inactive))
		TCON_ERR("pinctrl_lookup_state inactive error!\n");
	#endif
}

void hwtcon_edp_pinmux_release(void)
{
	#ifndef FPGA_EARLY_PORTING
	devm_pinctrl_put(g_pctrl);
	#endif
}

void hwtcon_edp_pinmux_active(void)
{
	#ifndef FPGA_EARLY_PORTING
	if (IS_ERR(g_pin_state_active)) {
		TCON_ERR("active pin state not ready !\n");
		return ;
	}

	if( g_pin_state_current != g_pin_state_active) {
		TCON_LVL_LOG(TCON_LVL_PIPELINE,"hwtcon_edp_pinmux_active!\n");
		if(0==pinctrl_select_state(g_pctrl, g_pin_state_active)) {
			g_pin_state_current = g_pin_state_active;
		}
	}
	else {
		TCON_LVL_LOG(TCON_LVL_PIPELINE,"pinmux_active already !\n");
	}
	#endif
}

void hwtcon_edp_pinmux_inactive(void)
{
	#ifndef FPGA_EARLY_PORTING
	if (IS_ERR(g_pin_state_inactive)) {
		TCON_ERR("inactive pin state not ready !\n");
		return ;
	}

	if( g_pin_state_current != g_pin_state_inactive) {
		TCON_LVL_LOG(TCON_LVL_PIPELINE,"hwtcon_edp_pinmux_inactive!\n");
		if(0==pinctrl_select_state(g_pctrl, g_pin_state_inactive)) {
			g_pin_state_current = g_pin_state_inactive;
		}
	}
	else {
		TCON_LVL_LOG(TCON_LVL_PIPELINE,"pinmux_inactive already !\n");
	}

	#endif
}

void wf_lut_config_context_init_for_pipeline(void)
{
	struct wf_lut_con_config wf_lut_config = {0x00};

	if (hw_tcon_get_edp_out_8bit())
		wf_lut_common_8bit_setting(&wf_lut_config);
	else
		wf_lut_common_16bit_setting(&wf_lut_config);

	wf_lut_config.wf_lut_mout = WF_LUT_MOUT_DPI;

	#ifdef FPGA_EARLY_PORTING
	tcon_config_swtcon_pin(NULL);
	#endif

	tcon_setting(NULL);

	/*special config should below common setting,
	 * because will memset in common setting
	 */
	wf_lut_config_context_test_without_trigger(NULL, &wf_lut_config, NULL);

}

void TS_WF_LUT_set_lut_info(struct cmdqRecStruct *pkt,
	int x, int y, int w, int h,
	int lut_id, enum WF_SLOT_ENUM slot)
{
	if (lut_id < 0 || lut_id > MAX_LUT_REGION_COUNT)
		return;

	mod_timer(&hwtcon_fb_info()->timer_lut_release[lut_id],
		jiffies + msecs_to_jiffies(HWTCON_TASK_TIMEOUT_MS));

	pp_write_mask(pkt, WF_LUT_EN, 0x1<<0, BIT_MASK(0));
	wf_lut_set_lut_info(pkt, lut_id, x, y, w, h);
	wf_lut_set_lut_id_info(pkt, lut_id, slot);
	pp_write_mask(pkt, WF_LUT_SHADOW_UP, 0x1<<0, BIT_MASK(0));
	tcon_config_global_register(pkt);
	wf_lut_dpi_enable(pkt);
}

void TS_WF_LUT_disable_wf_lut(void)
{
	wf_lut_dpi_disable(NULL);
	tcon_disable(NULL);
	pp_write_mask(NULL, WF_LUT_EN, 0<<0, BIT_MASK(0));
}

void hwtcon_verify_show_picture_by_frame(int counter)
{
	struct cmdqRecStruct *pkt = NULL;
	int lut_id = 0;

	for (lut_id = 0; lut_id < counter; lut_id++) {
		cmdqRecCreate(CMDQ_SCENARIO_WF_LUT, &pkt);
		cmdqRecReset(pkt);

		mod_timer(&hwtcon_fb_info()->timer_lut_release[lut_id],
			jiffies + msecs_to_jiffies(HWTCON_TASK_TIMEOUT_MS));

		if (lut_id != 0) {
			cmdqRecClearEventToken(pkt,
				CMDQ_EVENT_WF_LUT_FRAME_DONE);
			cmdqRecWait(pkt, CMDQ_EVENT_WF_LUT_FRAME_DONE);
		}

		pp_write_mask(pkt, WF_LUT_EN, 0x1<<0, BIT_MASK(0));
		wf_lut_set_lut_info(pkt, lut_id, 0, 0, hw_tcon_get_edp_width(),
					hw_tcon_get_edp_height());
		wf_lut_set_lut_id_info(pkt, lut_id, WF_SLOT_4);
		pp_write_mask(pkt, WF_LUT_SHADOW_UP, 0x1<<0, BIT_MASK(0));
		tcon_config_global_register(pkt);
		wf_lut_dpi_enable(pkt);
		cmdqRecFlushAsync(pkt);
		cmdqRecDestroy(pkt);
	}
}

void wf_lut_config_show_picture_by_frame(int *lut_id_array,
			int counter, enum WF_SLOT_ENUM waveform_slot)
{
	struct cmdqRecStruct *pkt = NULL;
	int lut_id = 0;

	for (lut_id = 0; lut_id < counter; lut_id++) {
		cmdqRecCreate(CMDQ_SCENARIO_WF_LUT, &pkt);
		cmdqRecReset(pkt);

		if (lut_id != 0) {
			cmdqRecClearEventToken(pkt,
				CMDQ_EVENT_WF_LUT_FRAME_DONE);
			cmdqRecWait(pkt, CMDQ_EVENT_WF_LUT_FRAME_DONE);
		}
		pp_write_mask(pkt, WF_LUT_EN, 0x1<<0, BIT_MASK(0));
		wf_lut_set_lut_info(pkt, lut_id_array[lut_id], 0, 0,
			hw_tcon_get_edp_width(), hw_tcon_get_edp_height());
		wf_lut_set_lut_id_info(pkt,
			lut_id_array[lut_id], waveform_slot);
		pp_write_mask(pkt, WF_LUT_SHADOW_UP, 0x1<<0, BIT_MASK(0));

		tcon_config_global_register(pkt);
		wf_lut_dpi_enable(pkt);

		cmdqRecFlushAsync(pkt);
		cmdqRecDestroy(pkt);
	}
}

irqreturn_t hwtcon_core_disp_rdma_irq_handle(int irq, void *dev)
{
	/* clear irq status. */
	wf_lut_clear_disp_rdma_irq_status(NULL);

	return IRQ_HANDLED;
}

irqreturn_t hwtcon_core_wf_lut_wdma_irq_handle(int irq, void *dev)
{
	//pr_err("enter wdma irq!\n");

	/* clear irq status. */
	wf_lut_wdma_clear_irq();
	#if 0
	if (wf_lut_get_8bit())
		Wf_Lut_Wdma_addr(NULL, pp_read(WDMA_DST_ADDR0_VA) +
			hw_tcon_get_edp_width()*hw_tcon_get_edp_height());
	else
		Wf_Lut_Wdma_addr(NULL, pp_read(WDMA_DST_ADDR0_VA) +
			hw_tcon_get_edp_width()*hw_tcon_get_edp_height()/2);
	#else
	Wf_Lut_Wdma_addr(NULL, pp_read(WDMA_DST_ADDR0_VA) +
		pp_read(WDMA_DST_W_IN_BYTE_VA)*hw_tcon_get_edp_height());
	#endif

	return IRQ_HANDLED;
}

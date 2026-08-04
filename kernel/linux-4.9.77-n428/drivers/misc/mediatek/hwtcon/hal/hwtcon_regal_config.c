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

#include "hwtcon_regal_config.h"
#include "../hwtcon_driver.h"
#include "hwtcon_hal.h"
#include "../hwtcon_fb.h"

#define		DUMP_FSDB_FILE	/* if define, dump waveform as fsdb file */
//`define		LOAD_SDF_FILE
//=====================================================
/* define the bit length of parameters received from command */
#define	PARAMETER_BIT	12
#define	DATA_BIT	16
#define	ROW_BIT	11
#define	COL_BIT	8
#define	SEQ_BIT	9
#define	MODE_BIT	4
#define	REG_RES_BIT	3

/* define data length */
#define	DATA_LENGTH	16

/* define data length stored in ROM (extra 3 bits are for en, W/R and DC/X) */
#define	ROM_LENGTH	19
/* define the max number of different patterns as 2^(`ROM_SIZE)-1 */
#define	ROM_SIZE	10
/* define the max number counted as 2^(`CNT_LENGTH)-1 */
#define	CNT_SIZE	25
#define	ADDR_BIT	9


#define	DWORD_BIT	32
#define	WORD_BIT	16
#define	IMG_BIT	13
#define	SMIBUS_BIT	128


#define REAGL_BASE_VA	(hwtcon_device_info()->mmsys_reg_va + 0xC000)
#define REAGL_BASE		(hwtcon_device_info()->mmsys_reg_pa + 0xC000)


#define	REAGLADDR_OFFSET	0x00000000
//Registers address need 0x0000_0000 ~ 0x0000_00A0
//Please also define the offset in the rt0_REAGL_ECLIPSE_CTRL.v file

#define	IP_ENABLE 0x00000000
#define REAGL_SFT_UPDATE  0x00000004
#define	ECLIPSE_SIT_UPDATE	0x00000008
#define	IMAGE_WIDTH	0x0000000C
#define	IMAGE_HEIGHT	0x00000010
#define	IP_STATUS  0x00000014
#define	IP_VERSION 0x00000018
#define	IP_CLR_INT 0x0000001C    //  2017_1201   v0_2

#define	REAGL_MASK_000	0x00000020
#define	REAGL_MASK_001	0x00000024
#define	REAGL_MASK_002	0x00000028
#define	REAGL_MASK_003	0x0000002C
#define	REAGL_MASK_004	0x00000030
#define	REAGL_MASK_005	0x00000034
#define	REAGL_MASK_006	0x00000038
#define	REAGL_MASK_007	0x0000003C
#define	REAGL_MASK_008	0x00000040
#define	REAGL_MASK_009	0x00000044
#define	REAGL_MASK_010	0x00000048
#define	REAGL_MASK_011	0x0000004C
#define	REAGL_MASK_012	0x00000050
#define	REAGL_MASK_013	0x00000054
#define	REAGL_MASK_014	0x00000058
#define	REAGL_MASK_015	0x0000005C
#define	REAGL_MASK_016	0x00000060
#define	REAGL_MASK_017	0x00000064
#define	REAGL_MASK_018	0x00000068
#define	REAGL_MASK_019	0x0000006C
#define	REAGL_MASK_020	0x00000070
#define	REAGL_MASK_021	0x00000074
#define	REAGL_MASK_022	0x00000078
#define	REAGL_MASK_023	0x0000007C
#define	REAGL_MASK_024	0x00000080
#define	REAGL_MASK_025	0x00000084
#define	REAGL_MASK_026	0x00000088
#define	REAGL_MASK_027	0x0000008C
#define	REAGL_MASK_028	0x00000090
#define	REAGL_MASK_029	0x00000094
#define	REAGL_MASK_030	0x00000098
#define	REAGL_MASK_031	0x0000009C

#define	REAGLD_FRAME_CNT	0x000000A0

void regal_write_mask(struct cmdqRecStruct *cmdq,
	u32 offset, u32 value, u32 mask)
{
	return pp_write_mask(cmdq, REAGL_BASE + offset, value, mask);
}
void regal_write(struct cmdqRecStruct *cmdq, u32 offset, u32 value)
{
	return pp_write(cmdq, REAGL_BASE + offset, value);
}
u32 regal_read(u32 offset)
{
	return pp_read(REAGL_BASE_VA + offset);
}

static void hwtcon_regal_config_regal(struct cmdqRecStruct *pkt)
{
	regal_write(pkt, REAGL_MASK_000, 0x76543210);
	regal_write(pkt, REAGL_MASK_001, 0x01234567);
	regal_write(pkt, REAGL_MASK_002, 0x01234567);
	regal_write(pkt, REAGL_MASK_003, 0x76543210);
	regal_write(pkt, REAGL_MASK_004, 0x76543210);
	regal_write(pkt, REAGL_MASK_005, 0x01234567);
	regal_write(pkt, REAGL_MASK_006, 0x01234567);
	regal_write(pkt, REAGL_MASK_007, 0x76543210);
	regal_write(pkt, REAGL_MASK_008, 0x76543210);
	regal_write(pkt, REAGL_MASK_009, 0x01234567);
	regal_write(pkt, REAGL_MASK_010, 0x01234567);
	regal_write(pkt, REAGL_MASK_011, 0x76543210);
	regal_write(pkt, REAGL_MASK_012, 0x76543210);
	regal_write(pkt, REAGL_MASK_013, 0x01234567);
	regal_write(pkt, REAGL_MASK_014, 0x01234567);
	regal_write(pkt, REAGL_MASK_015, 0x76543210);
	regal_write(pkt, REAGL_MASK_016, 0x76543210);
	regal_write(pkt, REAGL_MASK_017, 0x01234567);
	regal_write(pkt, REAGL_MASK_018, 0x01234567);
	regal_write(pkt, REAGL_MASK_019, 0x76543210);
	regal_write(pkt, REAGL_MASK_020, 0x76543210);
	regal_write(pkt, REAGL_MASK_021, 0x01234567);
	regal_write(pkt, REAGL_MASK_022, 0x01234567);
	regal_write(pkt, REAGL_MASK_023, 0x76543210);
	regal_write(pkt, REAGL_MASK_024, 0x76543210);
	regal_write(pkt, REAGL_MASK_025, 0x01234567);
	regal_write(pkt, REAGL_MASK_026, 0x01234567);
	regal_write(pkt, REAGL_MASK_027, 0x76543210);
	regal_write(pkt, REAGL_MASK_028, 0x76543210);
	regal_write(pkt, REAGL_MASK_029, 0x01234567);
	regal_write(pkt, REAGL_MASK_030, 0x01234567);
	regal_write(pkt, REAGL_MASK_031, 0x76543210);
	regal_write(pkt, REAGLD_FRAME_CNT, 0x00000000);
}


void hwtcon_regal_config_regal_mode(struct cmdqRecStruct *pkt,
	u32 regal_mode, int img_width, int img_height)
{
	/* NOTE: img_width & img_height should dec 1 by resolution */
	img_width -= 1;
	img_height -= 1;
	switch (regal_mode) {

	case REGAL_MODE_REGAL:
		regal_write(pkt, REAGL_SFT_UPDATE, 0x1D1D1F31);
		regal_write(pkt, ECLIPSE_SIT_UPDATE, 0x33153000);
		break;
	case REGAL_MODE_DRAK:
		regal_write(pkt, REAGL_SFT_UPDATE, 0x1D1D1F31);
		regal_write(pkt, ECLIPSE_SIT_UPDATE, 0x33153004);
		break;
	default:
		TCON_ERR("invalid regal mode:%d", regal_mode);
		return;
	}

	hwtcon_regal_config_regal(pkt);
	regal_write(pkt, IMAGE_WIDTH + REAGLADDR_OFFSET, img_width);
	regal_write(pkt, IMAGE_HEIGHT + REAGLADDR_OFFSET, img_height);
	regal_write(pkt, IP_ENABLE + REAGLADDR_OFFSET, 0x00000001);
}

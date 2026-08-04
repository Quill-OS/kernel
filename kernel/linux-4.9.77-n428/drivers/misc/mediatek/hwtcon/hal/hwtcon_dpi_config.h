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

#ifndef __HWTCON_DPI_CONFIG_H__
#define __HWTCON_DPI_CONFIG_H__
#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "cmdq_record.h"
#include "../hwtcon_epd.h"


void wf_lut_config_dpi(struct cmdqRecStruct *pkt,
	struct dpi_timing_para *dpi_para,
	unsigned int width, unsigned int height);

void wf_lut_dpi_enable(struct cmdqRecStruct *pkt);
void wf_lut_dpi_disable(struct cmdqRecStruct *pkt);
void wf_lut_config_dpi_context(struct cmdqRecStruct *pkt,
	unsigned int width, unsigned int height);
unsigned int wf_lut_dpi_get_checksum(struct cmdqRecStruct *pkt);
u32 wf_lut_dpi_get_irq_status(void);
void wf_lut_dpi_clear_irq_status(struct cmdqRecStruct *pkt);
void wf_lut_dpi_enable_force_output(struct cmdqRecStruct *pkt, bool enable);


#endif /* __HWTCON_DPI_CONFIG_H__ */

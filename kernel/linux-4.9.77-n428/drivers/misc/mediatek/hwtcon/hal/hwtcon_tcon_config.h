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

#ifndef __HWTCON_TCON_CONFIG_H__
#define __HWTCON_TCON_CONFIG_H__
#include <linux/types.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include "../hwtcon_epd.h"
#include "cmdq_record.h"
#include "hwtcon_hal.h"

struct tcon_pin_swap_para {
	unsigned int sdec;
	unsigned int sdoe;
	unsigned int gdclk;
	unsigned int sdle;
	unsigned int gdoe;
};

void tcon_config_global_register(struct cmdqRecStruct *pkt);
void tcon_config_pin(struct cmdqRecStruct *pkt);
void tcon_setting(struct cmdqRecStruct *pkt);
void tcon_config_swtcon_pin(struct cmdqRecStruct *pkt);
void tcon_disable(struct cmdqRecStruct *pkt);

#endif /* __HWTCON_TCON_CONFIG_H__ */

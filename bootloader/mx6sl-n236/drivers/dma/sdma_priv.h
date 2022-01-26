/*
 * Copyright (C) 2011, Freescale Semiconductor, Inc. All Rights Reserved
 * THIS SOURCE CODE IS CONFIDENTIAL AND PROPRIETARY AND MAY NOT
 * BE USED OR DISTRIBUTED WITHOUT THE WRITTEN PERMISSION OF
 * Freescale Semiconductor, Inc.
*/

/*!
 * @file sdma_priv.h
 * @brief Private header for SDMA library
 */

#ifndef SDMA_PRIV_H
#define SDMA_PRIV_H

#include "sdma_script_info.h"

#include <sdma.h>

/*--------------------------------- macros --------------------------------------*/

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define SDMA_SCRATCH_ENABLE

#define SDMA_NUM_CHANNELS 32
#define SDMA_NUM_REQUESTS 48
#define SDMA_NUM_BUF_DESC 64

#define SDMA_REG_RESET_BIT_RESET	(1<<0)
#define SDMA_REG_RESET_BIT_RESCHED	(1<<1)

#define SDMA_REG_CONFIG_VAL_ACR_TWICE	(0<<4)
#define SDMA_REG_CONFIG_VAL_ACR_ONCE	(1<<4)

#define SDMA_REG_CHN0ADDR_BIT_SMSZ	(1<<14)

#define SDMA_CHANNEL_PRIORITY_FREE	0
#define SDMA_CHANNEL_PRIORITY_LOW	1
#define SDMA_CHANNEL_PRIORITY_HIGH	7

#define SDMA_CMD_C0_SET_DM              0x01
#define SDMA_CMD_C0_SET_PM              0x04
#define SDMA_CMD_C0_GET_DM              0x02
#define SDMA_CMD_C0_GET_PM              0x06
#define SDMA_CMD_C0_SETCTX              0x07
#define SDMA_CMD_C0_GETCTX              0x03

#define SDMA_BD_MODE_CMD_SHIFT		24
#define SDMA_BD_MODE_CNT_MASK		0xFFFF

#define SDMA_RAMSCRIPT_CODE_START	0x1800

#define CHANNEL_STOP_STAT(channel) (sdma_base->stop_stat & (1<<channel))

/*-------------------------------- structures -------------------------------------*/

typedef struct {
    unsigned int pc_rpc;
    unsigned int spc_epc;
    unsigned int gr[8];
    unsigned int mda;
    unsigned int msa;
    unsigned int ms;
    unsigned int md;
    unsigned int pda;
    unsigned int psa;
    unsigned int ps;
    unsigned int pd;
    unsigned int ca;
    unsigned int cs;
    unsigned int dda;
    unsigned int dsa;
    unsigned int ds;
    unsigned int dd;
#ifdef SDMA_SCRATCH_ENABLE
    unsigned int scratch[8];
#endif
} sdma_channel_context_t, *sdma_channel_context_p;

typedef struct {
    unsigned int currentBDptr;
    unsigned int baseBDptr;
    unsigned int chanDesc;
    unsigned int status;
} sdma_ccb_t, *sdma_ccb_p;

typedef struct {
    sdma_ccb_t sdma_ccb[SDMA_NUM_CHANNELS]; //channel control block
    sdma_bd_p sdma_bdp[SDMA_NUM_CHANNELS];
    unsigned int sdma_bd_num[SDMA_NUM_CHANNELS];
    sdma_channel_context_t chan_cnxt;   //as the buf to load channel's context to sdma core
    sdma_bd_t chan0BD;          //channel 0 buffer descriptor reserved buffer
} sdma_env_t, *sdma_env_p;

typedef struct {
    volatile unsigned int mc0ptr;
    volatile unsigned int intr;
    volatile unsigned int stop_stat;
    volatile unsigned int hstart;
    volatile unsigned int evtovr;
    volatile unsigned int dspovr;
    volatile unsigned int hostovr;
    volatile unsigned int evtpend;
    volatile unsigned int reserved0;
    volatile unsigned int reset;
    volatile unsigned int evterr;
    volatile unsigned int intrmask;
    volatile unsigned int psw;
    volatile unsigned int evterrdbg;
    volatile unsigned int config;
    volatile unsigned int sdma_lock;
    volatile unsigned int once_enb;
    volatile unsigned int once_data;
    volatile unsigned int once_instr;
    volatile unsigned int once_stat;
    volatile unsigned int once_cmd;
    volatile unsigned int reserved1;
    volatile unsigned int illinstaddr;
    volatile unsigned int chn0addr;
    volatile unsigned int evt_mirror;
    volatile unsigned int evt_mirror2;
    volatile unsigned int reserved2[2];
    volatile unsigned int xtrig_conf1;
    volatile unsigned int xtrig_conf2;
    volatile unsigned int otb;
    volatile unsigned int prf_cntx[6];
    volatile unsigned int prf_cfg;
    volatile unsigned int reserved3[26];
    volatile unsigned int chnpri[SDMA_NUM_CHANNELS];
    volatile unsigned int reserved4[32];
    volatile unsigned int chnenbl[SDMA_NUM_REQUESTS];
    volatile unsigned int reserved5[16];
} sdma_reg_t, *sdma_reg_p;

#endif

/*
 * Copyright (C) 2011, Freescale Semiconductor, Inc. All Rights Reserved
 * THIS SOURCE CODE IS CONFIDENTIAL AND PROPRIETARY AND MAY NOT
 * BE USED OR DISTRIBUTED WITHOUT THE WRITTEN PERMISSION OF
 * Freescale Semiconductor, Inc.
*/

/*!
 * @file sdma.h
 * @brief SDMA library macros, structures and functions 
 */

#ifndef SDMA_H
#define SDMA_H

//#include "io.h"

/*--------------------------------- macros --------------------------------------*/
#define SDMA_ENV_BUF_SIZE		0x800

#define SDMA_CHANNEL_PRIORITY_FREE	0
#define SDMA_CHANNEL_PRIORITY_LOW	1
#define SDMA_CHANNEL_PRIORITY_HIGH	7

typedef enum {
    SDMA_AP_2_AP = 0,
    SDMA_APP_2_MCU,
    SDMA_MCU_2_APP,
    SDMA_UART_2_MCU,
    SDMA_SHP_2_MCU,
    SDMA_MCU_2_SHP,
    SDMA_SPDIF_2_MCU,
    SDMA_MCU_2_SPDIF,
    SDMA_FIRI_2_MCU,
    SDMA_MCU_2_FIRI,
    SDMA_MCU_2_SSIAPP,
    SDMA_MCU_2_SSISH,
    SDMA_P_2_P,
    SDMA_SSIAPP_2_MCU,
    SDMA_SSISH_2_MCU,
    SDMA_AP_2_AP_SIMP,

    SDMA_NUM_SCRIPTS
} script_name_e;

typedef enum {
    SDMA_FLAGS_BUSY = (1 << 16),
    SDMA_FLAGS_WRAP = (1 << 17),
    SDMA_FLAGS_CONT = (1 << 18),
    SDMA_FLAGS_INTR = (1 << 19),
    SDMA_FLAGS_ERROR = (1 << 20),
    SDMA_FLAGS_NFC_RD = (1 << 28),
    SDMA_FLAGS_BW8 = (1 << 24),
    SDMA_FLAGS_BW16 = (2 << 24),
    SDMA_FLAGS_BW24 = (3 << 24),
    SDMA_FLAGS_BW32 = (0 << 24)
} sdma_flag_e;

typedef enum {
    SDMA_CHANNEL_STATUS_ERROR,
    SDMA_CHANNEL_STATUS_BUSY,
    SDMA_CHANNEL_STATUS_DONE
} sdma_channel_status_e;

typedef enum {
    SDMA_RETV_SUCCESS = 0,
    SDMA_RETV_FAIL = -1,

    SDMA_RETV_NULLP = -2,
    SDMA_RETV_PRIORITY = -3,
    SDMA_RETV_NO_FREE_CHANNEL = -4,
    SDMA_RETV_SET_CONTEXT = -5,
    SDMA_RETV_BD_VALIDATE = -6,
    SDMA_RETV_NOT_INITED = -7
} sdma_error_e;

typedef struct {
    unsigned int mode;          //mode word, including count, command, flag...
    unsigned int buf_addr;      //buffer address, while peripheral address in channel context
    unsigned int ext_buf_addr;  //extended buffer address, not mandatory for scripts
} sdma_bd_t, *sdma_bd_p;

typedef struct {
    unsigned int script_addr;   //script to use
    unsigned int gpr[8];        //r0-r7, including DMA mask, watermark...
    unsigned int dma_mask[2];   //dma_mask[0]: DMA request 0-31, 1: 32-47
    unsigned int priority;     //priority of channel(0-7)
    unsigned int nbd;           //number of buffer descriptors
} sdma_chan_desc_t, *sdma_chan_desc_p;

typedef void (*sdma_channel_isr) (unsigned int);

/*--------------------------------- functions -------------------------------------*/
int sdma_init(unsigned int *, unsigned int);
void sdma_deinit(void);
int sdma_channel_start(unsigned int);
int sdma_channel_stop(unsigned int);
int sdma_channel_request(sdma_chan_desc_p, sdma_bd_p);
int sdma_channel_release(unsigned int);
unsigned int sdma_channel_status(unsigned int, unsigned int *);
int sdma_lookup_script(script_name_e, unsigned int *);
void sdma_setup_interrupt(void);
int sdma_channel_isr_attach(unsigned int, sdma_channel_isr isr);

#endif

/*
 * Copyright (C) 2011, Freescale Semiconductor, Inc. All Rights Reserved
 * THIS SOURCE CODE IS CONFIDENTIAL AND PROPRIETARY AND MAY NOT
 * BE USED OR DISTRIBUTED WITHOUT THE WRITTEN PERMISSION OF
 * Freescale Semiconductor, Inc.
*/

/*!
 * @file sdma_script_info.h
 * @brief header files to describe the script code.
 */

#ifndef SDMA_SCRIPT_INFO_H
#define SDMA_SCRIPT_INFO_H

#include <sdma.h>

#define SDMA_SCRIPT_MAX_NUM	64

typedef struct {
    script_name_e script_index;
    unsigned int script_addr;
} sdma_script_map_t, *sdma_script_map_p;

typedef struct {
    unsigned int version;
    unsigned int target_platform;
    sdma_script_map_t script_maps[SDMA_SCRIPT_MAX_NUM];
    unsigned int ram_code_size;
    const short *ram_code;
} sdma_script_info_t, *sdma_script_info_p;

#endif

/*
 * Copyright (C) 2011, Freescale Semiconductor, Inc. All Rights Reserved
 * THIS SOURCE CODE IS CONFIDENTIAL AND PROPRIETARY AND MAY NOT
 * BE USED OR DISTRIBUTED WITHOUT THE WRITTEN PERMISSION OF
 * Freescale Semiconductor, Inc.
*/

/*!
 * @file sdma_script_info.c
 * @brief a global varialbe to hold the information of the script code.
 */

#include "sdma_script_info.h"
#include "sdma_script_code.h"

const sdma_script_info_t script_info = {
    0x00000001,
    0x61,
    {
     {SDMA_AP_2_AP, ap_2_ap_ADDR},
     {SDMA_APP_2_MCU, app_2_mcu_ADDR},
     {SDMA_MCU_2_APP, mcu_2_app_ADDR},
     {SDMA_UART_2_MCU, uart_2_mcu_ADDR},
     {SDMA_SHP_2_MCU, shp_2_mcu_ADDR},
     {SDMA_MCU_2_SHP, mcu_2_shp_ADDR},
     {SDMA_SPDIF_2_MCU, spdif_2_mcu_ADDR},
     {SDMA_MCU_2_SPDIF, mcu_2_spdif_ADDR},
     {SDMA_MCU_2_SSIAPP, mcu_2_ssiapp_ADDR},
     {SDMA_MCU_2_SSISH, mcu_2_ssish_ADDR},
     {SDMA_P_2_P, p_2_p_ADDR},
     {SDMA_SSIAPP_2_MCU, ssiapp_2_mcu_ADDR},
     {SDMA_SSISH_2_MCU, ssish_2_mcu_ADDR},
     {SDMA_AP_2_AP_SIMP, ap_2_ap_simp_ADDR},
     {SDMA_NUM_SCRIPTS, 0},
     },
    RAM_CODE_SIZE,
    sdma_code
};

/**
 *  * @file stk83xx_ver.h
 *   *
 *    * Copyright (c) 2020, Sensortek.
 *     * All rights reserved.
 *      *
 *       ******************************************************************************/

/*==============================================================================
 *
 *     Change Log:
 *
 *         EDIT HISTORY FOR FILE
 *             
 *                 Apr 6 2022 STK - 1.0.0
 *                       - first SSU architecture
 *                       - include spi/i2c 1.0 and 2.0
 * 
  ============================================================================*/

#ifndef _STK83XX_VER_H
#define _STK83XX_VER_H

// 32-bit version number represented as major[31:16].minor[15:8].rev[7:0]
 #define STK83XX_MAJOR        1
 #define STK83XX_MINOR        0
 #define STK83XX_REV          0
 #define VERSION_STK83XX  ((STK83XX_MAJOR<<16) | (STK83XX_MINOR<<8) | STK83XX_REV)

 #endif //_STK83XX_VER_H

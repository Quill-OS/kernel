/*
 * Copyright (c) 2010 Amazon Technologies, Inc. All Rights Reserved.
 * Nadim Awad (nawad@lab126.com)
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __ZFORCE_H__
#define __ZFORCE_H__

#define ZF_MAGIC_NUMBER             'Z'
#define ZF_GET_LED_LVL              _IO(ZF_MAGIC_NUMBER, 0x01) 
#define ZF_GET_LOW_SIG_INFO         _IO(ZF_MAGIC_NUMBER, 0x02)
#define ZF_SET_STATE_UPDATE         _IO(ZF_MAGIC_NUMBER, 0x03)
#define ZF_SET_STATE_RUN            _IO(ZF_MAGIC_NUMBER, 0x04)
#define ZF_GET_RAW_DIODE_LVLS       _IO(ZF_MAGIC_NUMBER, 0x05)
#define ZF_SET_FIXED_PULSE          _IO(ZF_MAGIC_NUMBER, 0x06)
#define ZF_FORCE_CALIBRATION        _IO(ZF_MAGIC_NUMBER, 0x07)

#define X_RESOLUTION                4095 // X output resolution
#define Y_RESOLUTION                4095 // Y output resolution

#define MAX_X_LED_COUNT             25   // Maximum number of LEDS in X
#define MAX_Y_LED_COUNT             25   // Maximum number of LEDS in Y

typedef struct LedSignalInfo_s
{
  u8 LedStrength1:4;
  u8 LedStrength2:4;
  u8 PDSignal1;
  u8 PDSignal2;
} LedSignalInfo;

typedef struct LedLevelResponse_s
{
  u8 xCount;
  u8 yCount;
  LedSignalInfo xLEDS[MAX_X_LED_COUNT];
  LedSignalInfo yLEDS[MAX_Y_LED_COUNT];
} LedLevelResponse;

typedef struct RawDiodeData_s
{ 
   u8 NumberOfPulses;        // Will always be 2
   u8 xCount;                // Should be 11
   u8 yCount;                // Should be 15
   u8 xValues[MAX_X_LED_COUNT*2];
   u8 yValues[MAX_Y_LED_COUNT*2];
} RawDiodeData;

typedef struct FixedPulseStrengthResponse_s
{
  u8 xCount;
  u8 yCount;
  u8 xValues[MAX_X_LED_COUNT*2];
  u8 yValues[MAX_Y_LED_COUNT*2];
} FixedPulseStrengthResponse;

typedef struct PulseSignalInfo_s
{
  u8 strength:4;
  u8 time:3;
  u8 reserved:1;
} PulseSignalInfo;

#endif // __ZFORCE_H__

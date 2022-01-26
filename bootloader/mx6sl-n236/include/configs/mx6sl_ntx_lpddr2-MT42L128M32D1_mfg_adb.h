#ifndef __mx6sl_ntx_lpddr2_h//[
#define __mx6sl_ntx_lpddr2_h

#define CONFIG_MX6SL_NTX_LPDDR2
#define CONFIG_MT42L128M32D1

#undef PHYS_SDRAM_1_SIZE 
#define PHYS_SDRAM_1_SIZE	(512 * 1024 * 1024)

#define CONFIG_MFG
#define CONFIG_MFG_ADB

#include "mx6sl_ntx_android.h"
#include "mx6sl_ntx.h"

#endif //]__mx6sl_ntx_lpddr2_h


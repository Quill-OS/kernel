#ifndef __mx6sl_ntx_lpddr2_h//[
#define __mx6sl_ntx_lpddr2_h


#undef PHYS_SDRAM_1_SIZE 
#define PHYS_SDRAM_1_SIZE	(256 * 1024 * 1024)

#include "mx6sl_ntx_android.h"

#ifndef CONFIG_MX6SL_NTX_LPDDR2//[
	#define CONFIG_MX6SL_NTX_LPDDR2
#endif //]CONFIG_MX6SL_NTX_LPDDR2

#endif //]__mx6sl_ntx_lpddr2_h


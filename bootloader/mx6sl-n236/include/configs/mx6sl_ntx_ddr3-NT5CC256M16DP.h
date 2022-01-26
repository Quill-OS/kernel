#ifndef __mx6sl_ntx_ddr3_h//[
#define __mx6sl_ntx_ddr3_h


#undef PHYS_SDRAM_1_SIZE 
#define PHYS_SDRAM_1_SIZE	(512 * 1024 * 1024)

#include "mx6sl_ntx_android.h"

#ifndef CONFIG_MX6SL_NTX_DDR3//[
	#define CONFIG_MX6SL_NTX_DDR3
#endif //]CONFIG_MX6SL_NTX_DDR3

#endif //]__mx6sl_ntx_ddr3_h


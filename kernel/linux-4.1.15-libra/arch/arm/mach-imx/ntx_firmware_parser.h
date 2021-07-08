#ifndef __ntx_firmware_parser_h//[
#define __ntx_firmware_parser_h


#include "ntx_firmware.h"

typedef int (NTX_FIRMWARE_PARSE_PROC)(NTX_FIRMWARE_HDR *I_ptFWHDR,NTX_FIRMWARE_ITEM_HDR *IO_ptFWItemHdr,void *pvFWItemBin,int iItemIdx);

int ntx_firmware_parse_fw_buf(NTX_FIRMWARE_HDR *I_ptFWHDR,unsigned long I_dwFWSize,
		NTX_FIRMWARE_PARSE_PROC *I_pfnFWParseProc);


#endif //]__ntx_firmware_parser_h 


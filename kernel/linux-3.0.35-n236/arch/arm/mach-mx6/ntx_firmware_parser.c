
#ifdef __KERNEL__//[
	#include <linux/kernel.h>
	#include <linux/string.h>
#else //][!__KERNEL__
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <stdlib.h>
	#include <string.h>
	#include <stdio.h>
#endif //]__KERNEL__

#include "ntx_firmware_parser.h"


const static char gszNTXFW_parser_SignatureS[] = NTX_FIRMWARE_SIGNATURE_S;
const static char gszNTXFW_parser_SignatureE[] = NTX_FIRMWARE_SIGNATURE_E;
const static unsigned char gbNTXFW_parser_item_magicA[NTX_FW_ITEM_MAGIC_SIZE] = NTX_FW_ITEM_MAGIC;

int ntx_firmware_parse_fw_buf(NTX_FIRMWARE_HDR *I_ptFWHDR,unsigned long I_dwFWSize,
		NTX_FIRMWARE_PARSE_PROC *I_pfnFWParseProc)
{
	int iRet = 0;
	unsigned short w;
	
	if(0!=strcmp(I_ptFWHDR->szSignatureS,gszNTXFW_parser_SignatureS)) {
		return -1;
	}


	if(I_pfnFWParseProc) {
		NTX_FIRMWARE_ITEM_HDR *L_ptFWItemHdr;
		void *L_pvFWItemBin;
		unsigned long dwFWSize;

		L_ptFWItemHdr = (NTX_FIRMWARE_ITEM_HDR *)(((unsigned char *)I_ptFWHDR)+sizeof(NTX_FIRMWARE_HDR));
		

		// parse ntx firmware items ...
		//
		for (w=0;w<I_ptFWHDR->wFirmwareItems;w++)
		{

			L_pvFWItemBin = (void *)(((unsigned char*)L_ptFWItemHdr) + sizeof(NTX_FIRMWARE_ITEM_HDR));

			if(0==memcmp(L_ptFWItemHdr->bMagicA,gbNTXFW_parser_item_magicA,sizeof(gbNTXFW_parser_item_magicA))) {
				if(I_pfnFWParseProc(I_ptFWHDR,L_ptFWItemHdr,L_pvFWItemBin,(int)w)>=0) {
					dwFWSize = L_ptFWItemHdr->dwFirmwareSize;
					L_ptFWItemHdr = (NTX_FIRMWARE_ITEM_HDR *) (((unsigned char *)L_pvFWItemBin)+dwFWSize);
				}
				else {
					// firmware item parsing failed ! 
					iRet = -2;break;
				}
			}
			else {
				// firmware magic error ! 
				iRet = -3;break;
			}

		}
		
	}

	return iRet;
}



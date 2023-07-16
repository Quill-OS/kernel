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

#ifndef __KERNEL__ //[
unsigned long ntx_firmware_header_load(int ifdFW, NTX_FIRMWARE_HDR *O_ptFWHDR)
{
	unsigned long dwFWSize = 0;
	unsigned short w;
	unsigned long dw;
	NTX_FIRMWARE_ITEM_HDR tFWItem;
	off_t offset_cur;
	size_t szChk;

	if(ifdFW<0) {
		fprintf(stderr,"file descriptor (%d) error !!",ifdFW);
		return 0;
	}

	offset_cur = lseek(ifdFW,0,SEEK_CUR);

	szChk = read(ifdFW,O_ptFWHDR,sizeof(NTX_FIRMWARE_HDR));
	if(szChk!=sizeof(NTX_FIRMWARE_HDR)) {
		fprintf(stderr,"read firmware header %d bytes failed!!\n",(int)sizeof(NTX_FIRMWARE_HDR));
		dwFWSize = 0;	goto exit;
	}
	O_ptFWHDR->wFirmwareItems = le16toh(O_ptFWHDR->wFirmwareItems);	
	if(!NTX_FW_CHECK_FWHDR(O_ptFWHDR)) {
		fprintf(stderr,"firmware header format error !\n");
		dwFWSize = 0;	goto exit;
	}

	dwFWSize += sizeof(NTX_FIRMWARE_HDR);
	printf("ntx firmware header size =%d,items=%d\n",sizeof(NTX_FIRMWARE_HDR),
			O_ptFWHDR->wFirmwareItems);


	
	for(w=0;w<O_ptFWHDR->wFirmwareItems;w++) {
		szChk = read(ifdFW,&tFWItem,sizeof(tFWItem));
		if(szChk!=sizeof(tFWItem)) {
			fprintf(stderr,"read firmware item header %d bytes failed!!\n",(int)sizeof(tFWItem));
			dwFWSize = 0;	goto exit;
		}
		dwFWSize += sizeof(tFWItem);
		dw = le32toh(tFWItem.dwFirmwareSize);
		dwFWSize += dw ;
		lseek(ifdFW,dw,SEEK_CUR);
		printf("ntx firmware item size=%u\n",(unsigned int)dw);
	}

exit:

	lseek(ifdFW,offset_cur,SEEK_SET);

	return dwFWSize ;
}
#endif //] 


static int _fwParseV1Proc(NTX_FIRMWARE_HDR *I_ptFWHDR,NTX_FIRMWARE_ITEM_HDR *IO_ptFWItemHdr,void *pvFWItemBin,int iItemIdx,void *pvParam)
{
	NTX_FIRMWARE_PARSE_PROC *pfnFWParseProc = pvParam;
	return pfnFWParseProc(I_ptFWHDR,IO_ptFWItemHdr,pvFWItemBin,iItemIdx);
}
int ntx_firmware_parse_fw_buf_ex(NTX_FIRMWARE_HDR *I_ptFWHDR,unsigned long I_dwFWSize,
		NTX_FIRMWARE_PARSE_PROC_EX *I_pfnFWParseProc,void *IO_pvParam)
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
				if(I_pfnFWParseProc(I_ptFWHDR,L_ptFWItemHdr,L_pvFWItemBin,(int)w,IO_pvParam)>=0) {
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


int ntx_firmware_parse_fw_buf(NTX_FIRMWARE_HDR *I_ptFWHDR,unsigned long I_dwFWSize,
		NTX_FIRMWARE_PARSE_PROC *I_pfnFWParseProc)
{
	return ntx_firmware_parse_fw_buf_ex(I_ptFWHDR,I_dwFWSize,_fwParseV1Proc,I_pfnFWParseProc);
}



/*
 * 這個檔案可被U-BOOT,開發板.c 板引入 .
 *
 *
 */

#include <mmc.h>

#include "ntx_hwconfig.h"


// binaries sector number of sd card .
int SD_OFFSET_SECS_KERNEL = 81920;
#define SD_OFFSET_SECS_INITRD		12288
#define SD_OFFSET_SECS_INITRD2		8192
#define SD_OFFSET_SECS_HWCFG		1024
#define SD_OFFSET_SECS_LOGO			18432
#define SD_OFFSET_SECS_WAVEFORM		14336

#define SD_OFFSET_SECS_SN		1

#define DEFAULT_LOAD_KERNEL_SZ	16384
#define DEFAULT_LOAD_RD_SZ	8192
#define KERNEL_RAM_ADDR		CONFIG_LOADADDR



const static unsigned char gszNtxBinMagicA[4]={0xff,0xf5,0xaf,0xff};


NTX_HWCONFIG gtNtxHwCfg,*gptNtxHwCfg=0,gtEsdNtxHwCfg,*gptEsdNtxHwCfg=0;
unsigned long gdwNtxHwCfgSize = 0,gdwEsdNtxHwCfgSize = 0;



#define BOOTARGS_BUF_SIZE	768
//char gszBootArgs[BOOTARGS_BUF_SIZE]="root=/dev/mmcblk0p1 noinitrd rootdelay=1 rw rootfstype=ext3 console=ttyS0,115200 uart_dma";
//char gszBootArgs[BOOTARGS_BUF_SIZE]="noinitrd rw rootfstype=ext3 console=ttyS0,115200 uart_dma";
char gszBootArgs[BOOTARGS_BUF_SIZE]="";
char gszMMC_Boot_Cmd[128]="";

static unsigned char gbSectorBufA[512],*gpbSectorBuffer=gbSectorBufA;

static int giIsNtxBinsLoaded = 0;

#define NTX_BOOTMODE_NA		(-1)
#define NTX_BOOTMODE_ISD		0 // normal boot mode .
#define NTX_BOOTMODE_ESD_UPG	1 // external upgrade mode .
#define NTX_BOOTMODE_RECOVERY	2 // internal recovery mode .

static int giNtxBootMode = NTX_BOOTMODE_NA ;


//#define MMC_DEV_ISD		0
//#define MMC_DEV_ESD		1

#define MMC_NUM_NUKOWN	(-1)
#define MMC_NUM_NONE		(-2)
static int gi_mmc_num_kernel=-1;
//static int gi_mmc_isd_num=-1;
static int gi_mmc_esd_num=-1;

#define AUTO_DETECT_KIMGSIZE		1



// external function ...
extern int ntxup_is_ext_card_inserted(void);
extern int ntxup_wait_key_esdupg(void);
extern int _get_sd_number(void);
extern int _get_pcba_id (void);
extern int _power_key_status (void);

// internal help functions ...
static int _detect_bootmode_and_append_boot_args(char *O_cBufA,unsigned long I_ulBufSize);


void E60612_led_R(int iIsTrunOn);
void E60612_led_G(int iIsTrunOn);
void E60612_led_B(int iIsTrunOn);


#define GET_SD_NUM(_i)	({\
	if(MMC_NUM_NUKOWN==gi_mmc_##_i##sd_num) {\
		if( 9==_get_pcba_id() ) {\
			/* E5061X */ \
			gi_mmc_esd_num = MMC_NUM_NONE;\
		}\
		else {\
			gi_mmc_esd_num = 1;\
		}\
	}\
	gi_mmc_##_i##sd_num; \
})

#define GET_ISD_NUM()	_get_sd_number()
#define GET_ESD_NUM()	GET_SD_NUM(e)


static unsigned long _get_ramsize(unsigned char **O_ppbRamStart)
{
	DECLARE_GLOBAL_DATA_PTR;
	if(O_ppbRamStart) {
		*O_ppbRamStart = (unsigned char*)gd->bd->bi_dram[0].start;
	}

	return (unsigned long)(gd->bd->bi_dram[0].size);
}


static unsigned long _load_ntx_bin_header(int I_iSDDevNum,unsigned long I_dwBinSectorNum,unsigned char *O_pbBuf,unsigned long I_dwBufSize)
{
	char cCmdA[128];
	unsigned long dwBinSize = 0;
	unsigned char *pbMagic;

	//ASSERT(I_dwBufSize>=512);


	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x1",I_iSDDevNum,(unsigned)O_pbBuf,(unsigned)(I_dwBinSectorNum-1));
	run_command(cCmdA, 0);//

	pbMagic = O_pbBuf + 512 -16 ;
	if( gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]&&
		gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3])
	{
		dwBinSize = *((unsigned long *)(pbMagic+8));
	}
	else {
		printf("binary magic @ sector no. %lu not found !\n",I_dwBinSectorNum);
	}

	return dwBinSize;
}

static void _load_ntx_bin(int I_iSDDevNum,unsigned long I_dwBinSectorNum,unsigned long I_dwBinSectorsToLoad,unsigned char *O_pbBuf,unsigned long I_dwBufSize)
{
	char cCmdA[128];

	//ASSERT(I_dwBufSize>=(I_dwBinSectorsToLoad*512));


	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",I_iSDDevNum,(unsigned)O_pbBuf,(unsigned)I_dwBinSectorNum,(unsigned)I_dwBinSectorsToLoad);
	run_command(cCmdA, 0);//
}

void _load_isd_hwconfig(void)
{
	unsigned long dwChk;

	if(gptNtxHwCfg) {
		return ;
	}

	dwChk = _load_ntx_bin_header(GET_ISD_NUM(),SD_OFFSET_SECS_HWCFG,gbSectorBufA,sizeof(gbSectorBufA));
	if(dwChk>0) {
		_load_ntx_bin(GET_ISD_NUM(),SD_OFFSET_SECS_HWCFG,1,gbSectorBufA,sizeof(gbSectorBufA));
		memcpy((unsigned char *)&gtNtxHwCfg,gbSectorBufA,sizeof(gtNtxHwCfg));
		gptNtxHwCfg = &gtNtxHwCfg;
		gdwNtxHwCfgSize = dwChk;
	}
	else {
		printf("ISD hwconfig missing !\n");
	}
}

void _load_esd_hwconfig(void)
{
	unsigned long dwChk;

	if(gptEsdNtxHwCfg) {
		return ;
	}

	dwChk = _load_ntx_bin_header(GET_ESD_NUM(),SD_OFFSET_SECS_HWCFG,gbSectorBufA,sizeof(gbSectorBufA));
	if(dwChk>0) {
		_load_ntx_bin(GET_ESD_NUM(),SD_OFFSET_SECS_HWCFG,1,gbSectorBufA,sizeof(gbSectorBufA));
		memcpy((unsigned char *)&gtEsdNtxHwCfg,gbSectorBufA,sizeof(gtEsdNtxHwCfg));
		gptEsdNtxHwCfg = &gtEsdNtxHwCfg;
		gdwEsdNtxHwCfgSize = dwChk;
	}
	else {
		printf("ESD hwconfig missing !\n");
	}
}



const static char gszKParamName_HwCfg[]="hwcfg";
const static char gszKParamName_logo[]="logo";
const static char gszKParamName_waveform[]="waveform";

static void _load_ntxbins_and_append_boot_args(char *O_cBufA,unsigned long I_ulBufSize)
{
	char cCmdA[128];
	char cAppendStr[128];

	volatile unsigned char *pbMagic = gpbSectorBuffer+(512-16) ;
	unsigned long dwImgSize ;
	unsigned long dwReadSectors, dwReadBytes;
	unsigned long dwTotalReservedSize,dwRamSize;

	unsigned char *pbLoadAddr;
	int iLoadDeviceNum = GET_ISD_NUM();


	// [ WARNING : be pair .
	int iLoadSecNO[] = {SD_OFFSET_SECS_HWCFG,SD_OFFSET_SECS_LOGO,SD_OFFSET_SECS_WAVEFORM};
	char *szLoadNameA[] = {(char *)gszKParamName_HwCfg,(char *)gszKParamName_logo,(char *)gszKParamName_waveform};
	// ] 
	
	int i;


	if(giIsNtxBinsLoaded) {
		printf("%s : ntx bins load skip (has been loaded already) !\n",__FUNCTION__);
		return ;
	}

	cAppendStr[0] = '\0';


	dwRamSize = _get_ramsize(&pbLoadAddr);
	printf("ram p=%p,size=%u\n",pbLoadAddr,(unsigned)dwRamSize);
	pbLoadAddr += dwRamSize;
	dwTotalReservedSize = 0;
	//
	//
	iLoadDeviceNum = GET_ISD_NUM();
	for(i=0;i<sizeof(iLoadSecNO)/sizeof(iLoadSecNO[0]);i++) 
	{
		if(0==strcmp(szLoadNameA[i],gszKParamName_HwCfg) && gptNtxHwCfg && gdwNtxHwCfgSize>0 ) {
			dwImgSize = gdwNtxHwCfgSize;
			dwReadSectors = (dwImgSize>>9)+1;
			dwReadBytes = dwReadSectors<<9;

			pbLoadAddr -= dwReadBytes;
			memcpy(pbLoadAddr,gptNtxHwCfg,gdwNtxHwCfgSize);
			gptNtxHwCfg = (NTX_HWCONFIG *)pbLoadAddr;


			//printf("hwcfg p=%p,size=%lu\n",gptNtxHwCfg,gdwNtxHwCfgSize);
			sprintf(cAppendStr," %s_p=0x%x %s_sz=%d",
					szLoadNameA[i],(unsigned)(pbLoadAddr),
					szLoadNameA[i],(int)dwReadBytes);
			strcat(O_cBufA,cAppendStr);

			dwTotalReservedSize+=dwReadBytes;
		}
		else {
			sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x1",iLoadDeviceNum,(unsigned)gpbSectorBuffer,(unsigned)(iLoadSecNO[i]-1));
			run_command(cCmdA, 0);// 
			if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
				&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
			{
				dwImgSize = *((unsigned long *)(pbMagic+8));
				dwReadSectors = (dwImgSize>>9)+1;
				dwReadBytes = dwReadSectors<<9;

				pbLoadAddr -= dwReadBytes;
				//printf("%s size=%d,sectors=%d,readbytes=%d,loadto=0x%p\n",
				//		szLoadNameA[i],dwImgSize,dwReadSectors,dwReadBytes,pbLoadAddr);
				sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",\
						iLoadDeviceNum,(unsigned)(pbLoadAddr),(unsigned)iLoadSecNO[i],(unsigned)dwReadSectors);
				//printf("cmd=%s\n",cCmdA);
				run_command(cCmdA, 0);// 


				if(0==strcmp(szLoadNameA[i],gszKParamName_HwCfg)) {
					gptNtxHwCfg = (NTX_HWCONFIG *)pbLoadAddr;
					gdwNtxHwCfgSize = dwImgSize;
				}
#if 1
				sprintf(cAppendStr," %s_p=0x%x %s_sz=%d",
						szLoadNameA[i],(unsigned)(pbLoadAddr),
						szLoadNameA[i],(int)dwReadBytes);
#endif

				dwTotalReservedSize+=dwReadBytes;
				strcat(O_cBufA,cAppendStr);
			}
			else {
				printf("no \"%s\" bin header\n",szLoadNameA[i]);
			}
		}
		
	}

	if(gptEsdNtxHwCfg) {
		// if boot from esd we should pass the ESD software configs to kernel .
		gptNtxHwCfg->m_val.bCustomer = gptEsdNtxHwCfg->m_val.bCustomer;
		gptNtxHwCfg->m_val.bRootFsType = gptEsdNtxHwCfg->m_val.bRootFsType;
		gptNtxHwCfg->m_val.bSysPartType = gptEsdNtxHwCfg->m_val.bSysPartType;
		gptNtxHwCfg->m_val.bProgressCnts = gptEsdNtxHwCfg->m_val.bProgressCnts;
		gptNtxHwCfg->m_val.bProgressXHiByte = gptEsdNtxHwCfg->m_val.bProgressXHiByte;
		gptNtxHwCfg->m_val.bProgressXLoByte = gptEsdNtxHwCfg->m_val.bProgressXLoByte;
		gptNtxHwCfg->m_val.bProgressYHiByte = gptEsdNtxHwCfg->m_val.bProgressYHiByte;
		gptNtxHwCfg->m_val.bProgressYLoByte = gptEsdNtxHwCfg->m_val.bProgressYLoByte;
		gptNtxHwCfg->m_val.bContentType = gptEsdNtxHwCfg->m_val.bContentType;
		gptNtxHwCfg->m_val.bUIStyle = gptEsdNtxHwCfg->m_val.bUIStyle;
		gptNtxHwCfg->m_val.bUIConfig = gptEsdNtxHwCfg->m_val.bUIConfig;
	}
	
	if(0!=dwTotalReservedSize) 
	{
		unsigned long dwTemp,dwTemp2;

		dwTemp = (dwRamSize-dwTotalReservedSize)>>20;
		dwTemp2 = dwTemp&0xffffffff;


		printf("Kernel RAM visiable size=%dM->%dM\n",(int)dwTemp,(int)dwTemp2);

		sprintf(cAppendStr," mem=%dM",(int)dwTemp2);
		strcat(O_cBufA,cAppendStr);
	}


	//run_command("mmc sw_dev 1", 0);// switch to external sdcard .

	giIsNtxBinsLoaded = 1;

}


unsigned char *gpbKernelAddr=0; // kernel address .
unsigned long gdwKernelSize=0; // kernel size in byte .

static int _load_ntxkernel(unsigned char **O_ppbKernelAddr,unsigned long *O_pdwKernelSize)
{
	char cCmdA[128];
	volatile unsigned char *pbMagic = gpbSectorBuffer+(512-16) ;
	unsigned long dwReadSectors;
	unsigned long dwImgSize=0 ;
	char *s;
	ulong offset = 0;
	int iRet=0;

	/* pre-set offset from CONFIG_SYS_LOAD_ADDR */
	offset = CONFIG_SYS_LOAD_ADDR;

	/* pre-set offset from $loadaddr */
	if ((s = getenv("loadaddr")) != NULL) {
		offset = simple_strtoul(s, NULL, 16);
	}


#ifdef AUTO_DETECT_KIMGSIZE//[
	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x1",gi_mmc_num_kernel,\
		(unsigned)gpbSectorBuffer,(unsigned)(SD_OFFSET_SECS_KERNEL-1));
	run_command(cCmdA, 0);// read kernel image information .
	//printf("cmd=%s\n",cCmdA);

	if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
		&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
	{

		dwImgSize = *((unsigned long *)(pbMagic+8));
		printf("kernel size = %lu\n",dwImgSize);
		dwReadSectors = dwImgSize>>9|1;
		dwReadSectors += 6;
		//dwReadSectors = 0xfff;
		sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",gi_mmc_num_kernel,(unsigned)offset,\
			SD_OFFSET_SECS_KERNEL,(unsigned)dwReadSectors);
		run_command(cCmdA, 0);// 
	}
	else
#endif //] AUTO_DETECT_KIMGSIZE
	{
#ifdef AUTO_DETECT_KIMGSIZE//[

		printf("no kernel image signature !\n");
#endif //]AUTO_DETECT_KIMGSIZE
		dwImgSize = DEFAULT_LOAD_KERNEL_SZ;
		sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",gi_mmc_num_kernel,(unsigned)offset,\
			SD_OFFSET_SECS_KERNEL,DEFAULT_LOAD_KERNEL_SZ);
		//printf("cmd=%s\n",cCmdA);
		run_command(cCmdA, 0);//
	}

	if(O_ppbKernelAddr) {
		*O_ppbKernelAddr = (unsigned char *)offset;
	}
	if(O_pdwKernelSize) {
		*O_pdwKernelSize = dwImgSize;
	}

	return iRet;
}
static int do_load_ntxkernel(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	iRet = _load_ntxkernel(&gpbKernelAddr,&gdwKernelSize);
	return iRet;
}
U_BOOT_CMD(load_ntxkernel, 2, 0, do_load_ntxkernel,
	"load_kernel - netronix kernel image load \n",
	"load_kernel "
		" - load netronix kernel from sd card .\n"
);




unsigned char *gpbRDaddr=0; // initrd address . 
unsigned long gdwRDsize=0; // initrd size in byte .

static int _load_ntxrd(unsigned char **O_ppbRDaddr,unsigned long *O_pdwRDsize)
{
	char cCmdA[128];
	volatile unsigned char *pbMagic = gpbSectorBuffer+(512-16) ;
	unsigned long dwReadSectors;
	unsigned long dwImgSize ;
	char *s;
	ulong offset = 0;
	int iRet = 0;
	int i;

	unsigned long dwOffsetSecsINITRDA[] = {
		SD_OFFSET_SECS_INITRD2,SD_OFFSET_SECS_INITRD
	};


	/* pre-set offset from CONFIG_RD_LOADADDR */
	offset = CONFIG_RD_LOADADDR;

	/* pre-set offset from $rd_loadaddr */
	if ((s = getenv("rd_loadaddr")) != NULL) {
		offset = simple_strtoul(s, NULL, 16);
	}
	else {
		printf("no ramdisk env ,default rd->%p\n",(void *)offset);
	}

	

#ifdef AUTO_DETECT_KIMGSIZE//[

	for (i=0;i<sizeof(dwOffsetSecsINITRDA)/sizeof(dwOffsetSecsINITRDA[0]);i++) {

		sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x1",gi_mmc_num_kernel,\
			(unsigned)gpbSectorBuffer,(unsigned)(dwOffsetSecsINITRDA[i]-1));
		run_command(cCmdA, 0);// read kernel image information .
		//printf("cmd=%s\n",cCmdA);

		if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
			&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
		{

			dwImgSize = *((unsigned long *)(pbMagic+8));
			printf("rd size = %lu\n",dwImgSize);
			dwReadSectors = dwImgSize>>9|1;
			dwReadSectors += 6;
			//dwReadSectors = 0xfff;
			sprintf(cCmdA,"mmc read %d 0x%lx 0x%lx 0x%lx",gi_mmc_num_kernel,offset,\
				(unsigned long)dwOffsetSecsINITRDA[i],dwReadSectors);
			run_command(cCmdA, 0);// 

			if(O_ppbRDaddr) {
				*O_ppbRDaddr = (unsigned char *)offset;
			}

			if(O_pdwRDsize) {
				*O_pdwRDsize = dwImgSize;
			}

			break;
		}
		else 
		{
			printf("no ramdisk image signature ! skip load initrd !\n");
		}

	} // initrd offset loop .

#else //][!

		{
			sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",gi_mmc_num_kernel,offset,\
				SD_OFFSET_SECS_INITRD,DEFAULT_LOAD_RD_SZ);
			//printf("cmd=%s\n",cCmdA);
			run_command(cCmdA, 0);// 
		
		}

#endif //] AUTO_DETECT_KIMGSIZE


	return iRet;
}
static int do_load_ntxrd(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	iRet = _load_ntxrd(&gpbRDaddr,&gdwRDsize);
	return iRet;
}
U_BOOT_CMD(load_ntxrd, 2, 0, do_load_ntxrd,
	"load_ntxrd - netronix ramdisk image load \n",
	"load_ntxrd "
		" - load netronix ramdisk from sd card .\n"
);




/*********************************************************
 * purpose : 
 * 	detect ntx bootmode and modify the kernel command line .
 * 
 * return :
 * 	return boot mode . 
 * 	
 * 
 * args : 
 * 	O_cBufA (output) : boot args string (kernel command line).
 * 	I_ulBufSize (input) : boot args string buffer size .	
 * 
 * author :
 * 	Gallen Lin
 * 
 * Date :
 * 	2011/04/14
 * 
 * *******************************************************/
static int _detect_bootmode(void)
{
	int iRet=NTX_BOOTMODE_NA;
	int iPwr_Key=-1,iUPG_Key=-1,iESD_in=-1;



	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}

	//printf("\n hwcfgp=%p,customer=%d\n\n",gptNtxHwCfg,gptNtxHwCfg->m_val.bCustomer);
	switch(gptNtxHwCfg->m_val.bPCB) {
	case 16://E60630.
	case 18://E50600.
		if(ntxup_is_ext_card_inserted() && 1==ntxup_wait_key_esdupg()) {
			printf("\n**************************\n\n");
			printf("\n**  1. Boot from ESD    **\n\n");
			printf("\n**************************\n\n");
			iRet = NTX_BOOTMODE_ESD_UPG;
		}
		else {
			iRet = NTX_BOOTMODE_ISD;
		}
		break;
	case 27:
		if(ntxup_wait_touch_recovery()) {
			printf("\n**************************************\n\n");
			printf("\n**  Boot to basic diagnostics mode  **\n\n");
			printf("\n**************************************\n\n");
			iRet = NTX_BOOTMODE_RECOVERY;
			SD_OFFSET_SECS_KERNEL = 19456;
		}
		else
			iRet = NTX_BOOTMODE_ISD;
		break;
	default :

#if 0
	{
		// test code .
		int iTempKey=-1;
		while(1) {
			iTempKey=_power_key_status();
			if(iTempKey!=iPwr_Key) {
				printf("PwrKey=%d\n",iTempKey);
			}
			iPwr_Key=iTempKey;
		}
	}
#endif 

		if((iESD_in=ntxup_is_ext_card_inserted()) && 1==(iUPG_Key=ntxup_wait_key_esdupg()) && (iPwr_Key=_power_key_status()) ) 
		{
			printf("\n**************************\n\n");
			printf("\n**  0. Boot from ESD    **\n\n");
			printf("\n**************************\n\n");
			iRet = NTX_BOOTMODE_ESD_UPG;
			_load_esd_hwconfig();
		}
		else if(1==ntxup_wait_key_esdupg()) {
			printf("\n**************************************\n\n");
			printf("\n**  Boot to basic diagnostics mode  **\n\n");
			printf("\n**************************************\n\n");
			iRet = NTX_BOOTMODE_RECOVERY;
			SD_OFFSET_SECS_KERNEL = 19456;
		}
		else {
			iRet = NTX_BOOTMODE_ISD;
			//sprintf(cCmdA,"setenv bootcmd run bootcmd_mmc");
			//run_command(cCmdA,0);
			_load_ntxrd(&gpbRDaddr,&gdwRDsize);
		}
		//printf("ESDin=%d,UPGKey=%d,PWRKey=%d\n",iESD_in,iUPG_Key,iPwr_Key);
		break;
	}

	return iRet;
}



void ntx_prebootm(void)
{
	char cCmdA[500];
	char *pcEnv_bootargs=0;
	char *pcLPJ;
	unsigned char bSysPartType,bRootFsType,bUIStyle;

	pcEnv_bootargs=getenv("bootargs");
	if(0==pcEnv_bootargs) {
		return ;
	}


	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}


	if(!gpbKernelAddr) {
		_load_ntxkernel(&gpbKernelAddr,&gdwKernelSize);
	}

	if(gptNtxHwCfg) { //[

		//if(NTX_BOOTMODE_ESD_UPG==giNtxBootMode) {
		//	_load_esd_hwconfig();
		//}

		//if(gptEsdNtxHwCfg) {
		//	bSysPartType = gptEsdNtxHwCfg->m_val.bSysPartType ;
		//	bRootFsType = gptEsdNtxHwCfg->m_val.bRootFsType;
		//	bUIStyle = gptNtxHwCfg->m_val.bUIStyle;
		//}
		//else
		{
			bSysPartType = gptNtxHwCfg->m_val.bSysPartType ;
			bRootFsType = gptNtxHwCfg->m_val.bRootFsType;
			bUIStyle = gptNtxHwCfg->m_val.bUIStyle;
		}

		if(2==bUIStyle)
		{
			// Android UI .

			_load_ntx_bin(GET_ISD_NUM(),SD_OFFSET_SECS_SN,1,gbSectorBufA,512);

			strcat(gszBootArgs," androidboot.serialno=");
			strcat(gszBootArgs,(char *)(&gbSectorBufA[3]));
			//printf("bootargs=\"%s\"\n",gszBootArgs);
		}
		else {
			// Linux UI .
			//if(!gpbRDaddr) {
			//	_load_ntxrd(&gpbRDaddr,&gdwRDsize);
			//}
		}

		if(!gpbRDaddr)
		{

			if(0==strstr(pcEnv_bootargs,"rootfstype=")) {
				printf("hwcfg rootfstype : %d\n",bRootFsType);
				switch(bRootFsType)
				{
				case 0:
					strcat(gszBootArgs," rootfstype=ext2");
					break;
				default:
				case 1:
					strcat(gszBootArgs," rootfstype=ext3");
					break;
				case 2:
					strcat(gszBootArgs," rootfstype=ext4");
					break;
				case 3:
					strcat(gszBootArgs," rootfstype=vfat");
					break;
				}
			}

			if(0==strstr(pcEnv_bootargs,"root=")) {
				char cTempA[5] = "0";

				printf("hwcfg partition type : %d\n",(int)bSysPartType);
				strcat(gszBootArgs," root=/dev/mmcblk");
				if(NTX_BOOTMODE_ISD==giNtxBootMode) {
					cTempA[0]='0';
					strcat(gszBootArgs,cTempA);

					switch (gptNtxHwCfg->m_val.bSysPartType)
					{
					default:
					case 0://TYPE1
					case 2://TYPE3
					case 4://TYPE5
					case 5://TYPE6
						strcat(gszBootArgs,"p1");
						break;
					case 1://TYPE2
					case 3://TYPE4
					case 6://TYPE7
						strcat(gszBootArgs,"p2");
						strcat(gszBootArgs," rootdelay=3");
						break;
					}
				}
				else if(NTX_BOOTMODE_ESD_UPG==giNtxBootMode){
					cTempA[0]='1';
					strcat(gszBootArgs,cTempA);
					switch (bSysPartType)
					{
					default:
					case 0://TYPE1
					case 2://TYPE3
					case 4://TYPE5
					case 5://TYPE6
						strcat(gszBootArgs,"p1");
						break;
					case 1://TYPE2
					case 3://TYPE4
					case 6://TYPE7
						strcat(gszBootArgs,"p2");
						break;
					}
				}
				else if(NTX_BOOTMODE_RECOVERY==giNtxBootMode) {
					cTempA[0]='0';
					strcat(gszBootArgs,cTempA);

					switch (gptNtxHwCfg->m_val.bSysPartType)
					{
					default:
						strcat(gszBootArgs,"p1");
						break;
					case 2://TYPE3
						strcat(gszBootArgs,"p2");
						break;
					case 1://TYPE2
						strcat(gszBootArgs,"p1");
						break;
					case 5://TYPE6
					case 6://TYPE7
						strcat(gszBootArgs,"p3");
						break;
					case 7://TYPE8
						strcat(gszBootArgs,"p4");
						break;
					}

					
				}
			}
		}
		if(2==gptNtxHwCfg->m_val.bCPUFreq) {
			// 1G Hz .
			strcat(gszBootArgs," mx50_1GHz");
			pcLPJ = strstr(pcEnv_bootargs,"lpj=");
			if(pcLPJ) {
				strncpy(pcLPJ+4,"4997120",7);
			}
			else {
				strcat(gszBootArgs," lpj=4997120");
			}
			
		}
		else if(1==gptNtxHwCfg->m_val.bCPUFreq) {
			// 800M Hz .
			/*
			  
			pcLPJ = strstr(pcEnv_bootargs,"lpj=");
			if(pcLPJ) {
				strncpy(pcLPJ+4,"3997696",7);
			}
			else {
				strcat(gszBootArgs," lpj=3997696");
			}
			 
			*/
		}




	} //] in hwconfig field ...


	sprintf(cCmdA,"setenv bootargs ${bootargs} %s",gszBootArgs);
	run_command(cCmdA, 0);// 
	//printf("%s : cmd=%s\n",__FUNCTION__,cCmdA);	

	//run_command("load_ntxrd", 0);// 
}



static int do_ntx_prepare_load_kernel(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	//printf("%s(%d)\n",__FUNCTION__,__LINE__);
	//init_VCOM_function(1,0);
	return iRet;
}

U_BOOT_CMD(ntx_prepare_load_kernel, 2, 0, do_ntx_prepare_load_kernel,
	"ntx_prepare_load_kernel - prepare to load kernel \n",
	"ntx_prepare_load_kernel "
		" - ntx_prepare_load_kernel.\n"
);
//]


// gallen add 2011/03/02 [
static int do_load_ntxbins(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	char *pstr;
	char cCmdA[128];
	//char cAppendStr[128]="";
	
	giNtxBootMode = _detect_bootmode();
	_load_ntxbins_and_append_boot_args(gszBootArgs,BOOTARGS_BUF_SIZE);


	gi_mmc_num_kernel = GET_ISD_NUM() ;

	switch(giNtxBootMode)	{
	case NTX_BOOTMODE_ISD:
		if(gptNtxHwCfg) {
			if(9==gptNtxHwCfg->m_val.bCustomer) {
				E60612_led_R(0);
				E60612_led_G(0);
				E60612_led_B(1);
			}
		}
		else {
			printf("boot normal : no hwconfig !\n");
		}
		break;

	case NTX_BOOTMODE_RECOVERY:
		pstr = getenv ("KRN_SDNUM_Recovery");
		if(pstr) {
			printf("KRN_SDNUM_Recovery=\"%s\"\n",pstr);
			gi_mmc_num_kernel = *pstr - '0';
		}

		if(gptNtxHwCfg) {
			if(9==gptNtxHwCfg->m_val.bCustomer) {
				E60612_led_R(1);
				E60612_led_G(0);
				E60612_led_B(0);
			}
		}
		else {
			printf("boot recovery : no hwconfig !\n");
		}
		sprintf(cCmdA,"setenv bootcmd run bootcmd_recovery");
		run_command(cCmdA,0);
		break;

	case NTX_BOOTMODE_ESD_UPG:
		pstr = getenv ("KRN_SDNUM_SD");
		if(pstr) {
			printf("KRN_SDNUM_SD=\"%s\"\n",pstr);
			gi_mmc_num_kernel = *pstr - '0';
		}
		sprintf(cCmdA,"setenv bootcmd run bootcmd_SD");
		run_command(cCmdA,0);
		break;

	}
	//setenv("bootargs",gszBootArgs);
	return iRet;

}

U_BOOT_CMD(load_ntxbins, 2, 0, do_load_ntxbins,
	"load_ntxbins - netronix binaries load \n",
	"load_ntxbins "
		" - load netronix binaries from sd card (hwcfg,logo,waveform).\n"
);
//] gallen add 2011/03/02



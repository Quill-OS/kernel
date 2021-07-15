
/* 
 * 這個檔案可被U-BOOT,開發板.c 板引入 .
 * 
 * 
 */

#include <mmc.h>

#include "ntx_hwconfig.h"


// binaries sector number of sd card .
int SD_OFFSET_SECS_KERNEL = 81920;
#define SD_OFFSET_SECS_HWCFG		1024
#define SD_OFFSET_SECS_LOGO			18432
#define SD_OFFSET_SECS_WAVEFORM		14336

#define DEFAULT_LOAD_KERNEL_SZ	16384
#define KERNEL_RAM_ADDR		CONFIG_LOADADDR

const static unsigned char gszNtxBinMagicA[4]={0xff,0xf5,0xaf,0xff};


NTX_HWCONFIG gtNtxHwCfg,*gptNtxHwCfg=0;



char gszMMC_Boot_Cmd[128]="";
#define BOOTARGS_BUG_SIZE		256
//char gszBootArgs[BOOTARGS_BUG_SIZE]="root=/dev/mmcblk0p1 noinitrd rootdelay=1 rw rootfstype=ext3 console=ttyS0,115200 uart_dma";
//char gszBootArgs[BOOTARGS_BUG_SIZE]="noinitrd rw rootfstype=ext3 console=ttyS0,115200 uart_dma";
char gszBootArgs[BOOTARGS_BUG_SIZE]="";

static unsigned char gbSectorBufA[512],*gpbSectorBuffer=gbSectorBufA;

static int giIsNtxBinsLoaded = 0;

#define NTX_BOOTMODE_NA		(-1)
#define NTX_BOOTMODE_ISD		0 // normal boot mode .
#define NTX_BOOTMODE_ESD_UPG	1 // external upgrade mode .
#define NTX_BOOTMODE_RECOVERY	2 // internal recovery mode .

static int giNtxBootMode = NTX_BOOTMODE_NA ;


#define MMC_DEV_ISD		0
#define MMC_DEV_ESD		1
static int gi_mmc_dev_num=MMC_DEV_ISD;

#define AUTO_DETECT_KIMGSIZE		1



// external function ...
extern int ntxup_is_ext_card_inserted(void);
extern int ntxup_wait_key_esdupg(void);

// internal help functions ...
static int _detect_bootmode_and_append_boot_args(char *O_cBufA,unsigned long I_ulBufSize);


void E60612_led_R(int iIsTrunOn);
void E60612_led_G(int iIsTrunOn);
void E60612_led_B(int iIsTrunOn);




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
	
	
	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x1",I_iSDDevNum,O_pbBuf,I_dwBinSectorNum-1);
	run_command(cCmdA, 0);//
	
	pbMagic = O_pbBuf + 512 -16 ;
	if( gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]&&
		gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
	{
		dwBinSize = *((unsigned long *)(pbMagic+8));
	}
	else {
		printf("binary magic @ sector no. %u not found !\n",I_dwBinSectorNum);
	}
	
	return dwBinSize; 
}

static void _load_ntx_bin(int I_iSDDevNum,unsigned long I_dwBinSectorNum,unsigned long I_dwBinSectorsToLoad,unsigned char *O_pbBuf,unsigned long I_dwBufSize)
{
	char cCmdA[128];
	
	//ASSERT(I_dwBufSize>=(I_dwBinSectorsToLoad*512));
	
	
	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",I_iSDDevNum,O_pbBuf,I_dwBinSectorNum,I_dwBinSectorsToLoad);
	run_command(cCmdA, 0);//
		 
}

void _load_isd_hwconfig(void)
{
	unsigned long dwChk;
	dwChk = _load_ntx_bin_header(0,SD_OFFSET_SECS_HWCFG,gbSectorBufA,sizeof(gbSectorBufA));
	if(dwChk>0) {
		_load_ntx_bin(0,SD_OFFSET_SECS_HWCFG,1,gbSectorBufA,sizeof(gbSectorBufA));
		memcpy((unsigned char *)&gtNtxHwCfg,gbSectorBufA,sizeof(gtNtxHwCfg));
		gptNtxHwCfg = &gtNtxHwCfg;
	}
	else {
		printf("ISD hwconfig missing !\n");
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
	int iLoadDeviceNum = MMC_DEV_ISD;


	// [ WARNING : be pair .
	int iLoadSecNO[] = {SD_OFFSET_SECS_HWCFG,SD_OFFSET_SECS_LOGO,SD_OFFSET_SECS_WAVEFORM};
	char *szLoadNameA[] = {gszKParamName_HwCfg,gszKParamName_logo,gszKParamName_waveform};
	// ] 
	
	int i;


	if(giIsNtxBinsLoaded) {
		printf("%s : ntx bins load skip (has been loaded already) !\n",__FUNCTION__);
		return ;
	}

	cAppendStr[0] = '\0';


	dwRamSize = _get_ramsize(&pbLoadAddr);
	printf("ram p=%p,size=%u\n",pbLoadAddr,dwRamSize);
	pbLoadAddr += dwRamSize;
	dwTotalReservedSize = 0;
	//
	//
	for(i=0;i<sizeof(iLoadSecNO)/sizeof(iLoadSecNO[0]);i++) 
	{

		if( (0==strcmp(szLoadNameA[i],gszKParamName_HwCfg)) &&  (NTX_BOOTMODE_ESD_UPG==giNtxBootMode) ) 
		{
			iLoadDeviceNum = MMC_DEV_ESD;
		}
		else 
		{
			iLoadDeviceNum = MMC_DEV_ISD;
		}

		sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x1",iLoadDeviceNum,(unsigned long)gpbSectorBuffer,iLoadSecNO[i]-1);
		run_command(cCmdA, 0);// 
		if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
			&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
		{
			dwImgSize = *((unsigned long *)(pbMagic+8));
			dwReadSectors = dwImgSize>>9|1;
			dwReadBytes = dwReadSectors<<9;

			pbLoadAddr -= dwReadBytes;
			//printf("%s size=%d,sectors=%d,readbytes=%d,loadto=0x%p\n",
			//		szLoadNameA[i],dwImgSize,dwReadSectors,dwReadBytes,pbLoadAddr);
			sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",\
					iLoadDeviceNum,(unsigned long)(pbLoadAddr),iLoadSecNO[i],dwReadSectors);
			//printf("cmd=%s\n",cCmdA);
			run_command(cCmdA, 0);// 


			if(0==strcmp(szLoadNameA[i],gszKParamName_HwCfg)) {
				gptNtxHwCfg = pbLoadAddr;
			}
#if 1
			sprintf(cAppendStr," %s_p=0x%x %s_sz=%d",
					szLoadNameA[i],(unsigned long)(pbLoadAddr),
					szLoadNameA[i],dwReadBytes);
#endif

			dwTotalReservedSize+=dwReadBytes;
			strcat(O_cBufA,cAppendStr);
		}
	}

	
	if(0!=dwTotalReservedSize) 
	{
		unsigned long dwTemp,dwTemp2;

		dwTemp = (dwRamSize-dwTotalReservedSize)>>20;
		dwTemp2 = dwTemp&0xffffffff;


		printf("Kernel RAM visiable size=%dM->%dM\n",dwTemp,dwTemp2);

		sprintf(cAppendStr," mem=%dM",dwTemp2);
		strcat(O_cBufA,cAppendStr);
	}
	

	//run_command("mmc sw_dev 1", 0);// switch to external sdcard .

	giIsNtxBinsLoaded = 1;

}


static void _load_ntxkernel(void)
{
	char cCmdA[128];
	volatile unsigned char *pbMagic = gpbSectorBuffer+(512-16) ;
	unsigned long dwReadSectors;
	unsigned long dwImgSize ;
	
#ifdef AUTO_DETECT_KIMGSIZE//[

	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x1",gi_mmc_dev_num,\
		(unsigned long)gpbSectorBuffer,SD_OFFSET_SECS_KERNEL-1);
	run_command(cCmdA, 0);// read kernel image information .
	printf("cmd=%s\n",cCmdA);

	if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
		&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
	{

		dwImgSize = *((unsigned long *)(pbMagic+8));
		printf("kernel size = %d\n",dwImgSize);
		dwReadSectors = dwImgSize>>9|1;
		dwReadSectors += 6;
		//dwReadSectors = 0xfff;
		sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",gi_mmc_dev_num,KERNEL_RAM_ADDR,\
			SD_OFFSET_SECS_KERNEL,dwReadSectors);
		run_command(cCmdA, 0);// 
	}
	else 
#endif //] AUTO_DETECT_KIMGSIZE
	{
#ifdef AUTO_DETECT_KIMGSIZE//[

		printf("no kernel image signature !\n");
#endif //]AUTO_DETECT_KIMGSIZE
		sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",gi_mmc_dev_num,KERNEL_RAM_ADDR,\
			SD_OFFSET_SECS_KERNEL,DEFAULT_LOAD_KERNEL_SZ);
		printf("cmd=%s\n",cCmdA);
		run_command(cCmdA, 0);// 
	
	}
	
}
static int do_load_ntxkernel(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	_load_ntxkernel();
	return iRet;
}
U_BOOT_CMD(load_ntxkernel, 2, 0, do_load_ntxkernel,
	"load_kernel - netronix kernel image load \n",
	"load_kernel "
		" - load netronix kernel from sd card .\n"
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

	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}
	
	//printf("\n hwcfgp=%p,customer=%d\n\n",gptNtxHwCfg,gptNtxHwCfg->m_val.bCustomer);
	if(ntxup_is_ext_card_inserted() && 1==ntxup_wait_key_esdupg() && _power_key_status() ) 
	{
		printf("\n**************************\n\n");
		printf("\n**    Boot from SD      **\n\n");
		printf("\n**************************\n\n");		
		iRet = NTX_BOOTMODE_ESD_UPG;
	}
	else if(1==ntxup_wait_key_esdupg()) {
		printf("\n**************************************\n\n");
		printf("\n**  Boot to basic diagnostics mode  **\n\n");
		printf("\n**************************************\n\n");
		iRet = NTX_BOOTMODE_RECOVERY;
		SD_OFFSET_SECS_KERNEL=19456;
	}
	else {
		iRet = NTX_BOOTMODE_ISD;
		//sprintf(cCmdA,"setenv bootcmd run bootcmd_mmc");
		//run_command(cCmdA,0);
	}

	
	return iRet;
}



void ntx_prebootm(void)
{
	char cCmdA[256];
	char *pstr;
	
	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}
	
	pstr=getenv("bootargs");
	if(pstr && gptNtxHwCfg) {
		if(0==strstr(pstr,"rootfstype=")) {
			printf("hwcfg rootfstype : %d\n",gptNtxHwCfg->m_val.bRootFsType);
			switch(gptNtxHwCfg->m_val.bRootFsType)
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
		
		if(0==strstr(pstr,"root=")) {
			char cTempA[5] = "0";

			printf("hwcfg partition type : %d\n",gptNtxHwCfg->m_val.bSysPartType);
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
					strcat(gszBootArgs,"p1");
					break;
				case 1://TYPE2
				case 3://TYPE4
					strcat(gszBootArgs,"p2");
					break;
				}			
			}
			else if(NTX_BOOTMODE_ESD_UPG==giNtxBootMode){
				cTempA[0]='1';
				strcat(gszBootArgs,cTempA);
				switch (gptNtxHwCfg->m_val.bSysPartType)
				{
				default:
				case 0://TYPE1
				case 2://TYPE3
				case 4://TYPE5
					strcat(gszBootArgs,"p1");
					break;
				case 1://TYPE2
				case 3://TYPE4
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
				}			
			}



		}
	}
	
	sprintf(cCmdA,"setenv bootargs ${bootargs} %s",gszBootArgs);
	run_command(cCmdA, 0);// 
	//printf("%s : cmd=%s\n",__FUNCTION__,cCmdA);	
	
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
	char cAppendStr[128]="";
	
	giNtxBootMode = _detect_bootmode();
	_load_ntxbins_and_append_boot_args(gszBootArgs,BOOTARGS_BUG_SIZE);

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
			gi_mmc_dev_num = *pstr - '0';
		}
		else {
			gi_mmc_dev_num = MMC_DEV_ISD ;
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
			gi_mmc_dev_num = *pstr - '0';
		}
		else {
			gi_mmc_dev_num = MMC_DEV_ESD ;
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

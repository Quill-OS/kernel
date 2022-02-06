
/* 
 * 這個檔案可被U-BOOT,開發板.c 板引入 .
 * 
 * 
 */

#include <mmc.h>

#ifdef CONFIG_FASTBOOT //[
	#include <fastboot.h>
#endif //]CONFIG_FASTBOOT
#include "ntx_hwconfig.h"
#include "ntx_hw.h"

#define MMC_CMD_SEPERATED_SDNUM		1// run mmc command with seperated sd number .

// binaries sector number of sd card .
#define SD_OFFSET_SECS_KERNEL		81920
#define SD_OFFSET_SECS_INITRD		12288
#define SD_OFFSET_SECS_INITRD2		8192
#ifdef _MX6Q_//[
	#define SD_OFFSET_SECS_HWCFG		1524
#else 
	#define SD_OFFSET_SECS_HWCFG		1024
#endif //]_MX6Q_
#define SD_OFFSET_SECS_WAVEFORM		14336
#define SD_OFFSET_SECS_LOGO			18432
#define SD_OFFSET_SECS_LOGO2			34816

#define SD_OFFSET_SECS_SNMAC		1
//#define SD_OFFSET_SECS_SN		1

#define DEFAULT_LOAD_KERNEL_SZ	18432
#define DEFAULT_LOAD_RD_SZ	8192
//#define KERNEL_RAM_ADDR		CONFIG_LOADADDR


//#define MAC_BOFFSET_IN_SNMAC_SECTOR		128


const static unsigned char gszNtxBinMagicA[4]={0xff,0xf5,0xaf,0xff};


volatile NTX_HWCONFIG gtNtxHwCfg,*gptNtxHwCfg=0,gtEsdNtxHwCfg,*gptEsdNtxHwCfg=0;
unsigned long gdwNtxHwCfgSize = 0,gdwEsdNtxHwCfgSize = 0;



#define BOOTARGS_BUF_SIZE	1024	
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
#define NTX_BOOTMODE_FASTBOOT	3 // fastboot mode .

static int giNtxBootMode = NTX_BOOTMODE_NA ;


//#define MMC_DEV_ISD		0
//#define MMC_DEV_ESD		1

#define MMC_NUM_NUKOWN	(-1)
#define MMC_NUM_NONE		(-2)
static int gi_mmc_num_kernel=-1;
static int gi_mmc_isd_num=0;
static int gi_mmc_esd_num=1;

#define AUTO_DETECT_KIMGSIZE		1


// external function ...
extern int ntxup_is_ext_card_inserted(void);
extern int ntxup_wait_key_esdupg(void);
extern unsigned long fastboot_connection_timeout_us_set(unsigned long dwTimeoutSetUS);

// internal help functions ...
static int _detect_bootmode_and_append_boot_args(char *O_cBufA,unsigned long I_ulBufSize);



#define GET_SD_NUM(_i)	({\
	gi_mmc_##_i##sd_num; \
})

#define GET_ISD_NUM()	GET_SD_NUM(i)
#define GET_ESD_NUM()	GET_SD_NUM(e)



int ntx_gpio_init(NTX_GPIO *I_pt_gpio)
{
	int iRet = 0 ;
	unsigned int reg;
	u32 dwGPIO_dir_addr=0;
	u32 dwGPIO_data_addr=0;
	int iIsSetVal = 0;
	
	if(!I_pt_gpio) {
		printf("%s(%d) : error parameter ! null ptr !\n",__FUNCTION__,__LINE__);
		return -1;
	}
	
#ifdef _MX50_ //[
	mxc_request_iomux(I_pt_gpio->PIN, I_pt_gpio->PIN_CFG);
	mxc_iomux_set_pad(I_pt_gpio->PIN, I_pt_gpio->PIN_PAD_CFG);
#elif defined(_MX6SL_)||defined(_MX6Q_) //][! _MX50_
	mxc_iomux_v3_setup_pad(I_pt_gpio->tIOMUXv3_PAD_CFG);
#endif //] _MX50_
	
	switch(I_pt_gpio->GPIO_Grp) {
	case 1:
		dwGPIO_dir_addr = GPIO1_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO1_BASE_ADDR + 0x0;
		break;
	case 2:
		dwGPIO_dir_addr = GPIO2_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO2_BASE_ADDR + 0x0;
		break;
	case 3:
		dwGPIO_dir_addr = GPIO3_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO3_BASE_ADDR + 0x0;
		break;
	case 4:
		dwGPIO_dir_addr = GPIO4_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO4_BASE_ADDR + 0x0;
		break;
	case 5:
		dwGPIO_dir_addr = GPIO5_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO5_BASE_ADDR + 0x0;
		break;
	case 6:
		dwGPIO_dir_addr = GPIO6_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		break;
	case 7:
		dwGPIO_dir_addr = GPIO7_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		break;
	default :
		printf("%s():%s [ERROR] GPIO group number error (%hd)!!\n",
			__FUNCTION__,I_pt_gpio->pszName,
			I_pt_gpio->GPIO_Grp);
		return -2;
	}
	
	if(0==I_pt_gpio->iDirection) {
		/* Set as output */
		reg = readl(dwGPIO_dir_addr);
		reg |= (u32)(1 << I_pt_gpio->GPIO_Num);
		writel(reg, dwGPIO_dir_addr);


		// set output value .
		if(1==I_pt_gpio->iDefaultValue) {
			reg = readl(dwGPIO_data_addr);
			reg |= (u32)(1 << I_pt_gpio->GPIO_Num);
			iIsSetVal = 1;
		}
		else if(0==I_pt_gpio->iDefaultValue){
			reg = readl(dwGPIO_data_addr);
			reg &= ~((u32)(1 << I_pt_gpio->GPIO_Num));
			iIsSetVal = 1;
		}

		if(iIsSetVal) {
			writel(reg, dwGPIO_data_addr);
		}

	}
	else if(1==I_pt_gpio->iDirection||2==I_pt_gpio->iDirection) {
		/* Set as input */
		reg = readl(dwGPIO_dir_addr);
		reg &= ~(1 << I_pt_gpio->GPIO_Num);
		writel(reg, dwGPIO_dir_addr);
	}
	else {
		// error direction .
		printf("[WARNING] %s : error direction type (%d)!\n",\
				__FUNCTION__,I_pt_gpio->iDirection);
	}
	
	++I_pt_gpio->iIsInited;
	
	return iRet;
}

int ntx_gpio_set_value(NTX_GPIO *I_pt_gpio,int iOutVal) 
{
	unsigned int reg,regDir;
	int iRet;
	int iChk;
	u32 dwGPIO_data_addr=0;
	u32 dwGPIO_dir_addr=0;
	
	if(!I_pt_gpio) {
		printf("%s(%d) : error parameter ! null ptr !\n",__FUNCTION__,__LINE__);
		return -1;
	}

	if(0!=I_pt_gpio->iDirection) {
		printf("%s(%d):gpio%d-%d,cannot set value (the direction must be output) !\n",
				__FUNCTION__,__LINE__,I_pt_gpio->GPIO_Grp,I_pt_gpio->GPIO_Num);
	}
	
	if(0==I_pt_gpio->iIsInited) {
		iChk = ntx_gpio_init(I_pt_gpio);
		if(iChk<0) {
			return iChk;
		}
		udelay(100);
	}
	
	switch(I_pt_gpio->GPIO_Grp) {
	case 1:
		dwGPIO_data_addr = GPIO1_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO1_BASE_ADDR + 0x4;
		break;
	case 2:
		dwGPIO_data_addr = GPIO2_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO2_BASE_ADDR + 0x4;
		break;
	case 3:
		dwGPIO_data_addr = GPIO3_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO3_BASE_ADDR + 0x4;
		break;
	case 4:
		dwGPIO_data_addr = GPIO4_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO4_BASE_ADDR + 0x4;
		break;
	case 5:
		dwGPIO_data_addr = GPIO5_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO5_BASE_ADDR + 0x4;
		break;
	case 6:
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO6_BASE_ADDR + 0x4;
		break;
	case 7:
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO7_BASE_ADDR + 0x4;
		break;
	default :
		printf("%s():%s [ERROR] GPIO group number error (%hd)!!\n",
			__FUNCTION__,I_pt_gpio->pszName,
			I_pt_gpio->GPIO_Grp);
		return -2;
	}
	regDir = readl(dwGPIO_dir_addr);
	if(!(regDir&(u32)(1 << I_pt_gpio->GPIO_Num))) {
		// direction is input 
		printf("%s():[WARNING] \"%s\" set output value on input GPIO pin \n",
				__FUNCTION__,I_pt_gpio->pszName);
	}
	reg = readl(dwGPIO_data_addr);
	if(1==iOutVal) {
		reg |= (u32)(1 << I_pt_gpio->GPIO_Num);
	}
	else if(0==iOutVal){
		reg &= ~((u32)(1 << I_pt_gpio->GPIO_Num));
	}
	writel(reg, dwGPIO_data_addr);
	
	
	return iRet;
}

int ntx_gpio_get_value(NTX_GPIO *I_pt_gpio) 
{
	unsigned int regDir;
	int iRet;
	int iChk;
	u32 dwGPIO_data_addr=0;
	u32 dwGPIO_dir_addr=0;
	
	if(!I_pt_gpio) {
		printf("%s(%d) : error parameter ! null ptr !\n",__FUNCTION__,__LINE__);
		return -1;
	}
	
	if(0==I_pt_gpio->iIsInited) {
		iChk = ntx_gpio_init(I_pt_gpio);
		if(iChk<0) {
			return iChk;
		}
		udelay(100);
	}

	if(1!=I_pt_gpio->iDirection&&2!=I_pt_gpio->iDirection) {
		printf("[WARNING] %s() :read gpio%d-%d \"%s\" with wrong direction (%d) \n",\
				__FUNCTION__,I_pt_gpio->GPIO_Grp,I_pt_gpio->GPIO_Num,\
				I_pt_gpio->pszName,I_pt_gpio->iDirection);
	}
	
	switch(I_pt_gpio->GPIO_Grp) {
	case 1:
		dwGPIO_data_addr = GPIO1_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO1_BASE_ADDR + 0x4;
		break;
	case 2:
		dwGPIO_data_addr = GPIO2_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO2_BASE_ADDR + 0x4;
		break;
	case 3:
		dwGPIO_data_addr = GPIO3_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO3_BASE_ADDR + 0x4;
		break;
	case 4:
		dwGPIO_data_addr = GPIO4_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO4_BASE_ADDR + 0x4;
		break;
	case 5:
		dwGPIO_data_addr = GPIO5_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO5_BASE_ADDR + 0x4;
		break;
	case 6:
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO6_BASE_ADDR + 0x4;
		break;
	case 7:
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO7_BASE_ADDR + 0x4;
		break;
	default :
		printf("%s():%s [ERROR] GPIO group number error (%hd)!!\n",
			__FUNCTION__,I_pt_gpio->pszName,
			I_pt_gpio->GPIO_Grp);
		return -2;
	}
	regDir = readl(dwGPIO_dir_addr);
	if((regDir&(u32)(1 << I_pt_gpio->GPIO_Num))) {
		// direction is input 
		printf("%s():[WARNING] \"%s\" read value on output GPIO pin\n",
				__FUNCTION__,I_pt_gpio->pszName);
	}
	
	iRet = (readl(dwGPIO_data_addr)&(1<<I_pt_gpio->GPIO_Num))?1:0;
	
	return iRet;
}


int ntx_gpio_key_is_down(NTX_GPIO *I_pt_gpio)
{
	int iRet,iChk;

	iChk = ntx_gpio_get_value(I_pt_gpio);

	if(iChk<0) {
		iRet = iChk;
	}
	else if(iChk == I_pt_gpio->iDefaultValue) {
		iRet = 1;
	}
	else {
		iRet = 0;
	}

	return iRet;
}




static unsigned long _get_ramsize(unsigned char **O_ppbRamStart)
{
	DECLARE_GLOBAL_DATA_PTR;
	if(O_ppbRamStart) {
		*O_ppbRamStart = (unsigned char*)gd->bd->bi_dram[0].start;
	}

	return (unsigned long)(gd->bd->bi_dram[0].size);
}


static int _read_mmc(int I_iSDDevNum,unsigned char *O_pbBuf,
		unsigned long I_dwBinSectorNum,unsigned long I_dwBinReadSectors)
{
	int iRet;
	char cCmdA[128+1];
#ifdef MMC_CMD_SEPERATED_SDNUM//[

	static int giCurSDDevNum=-1;

	if(giCurSDDevNum!=I_iSDDevNum) {
		sprintf(cCmdA,"mmc dev %d",I_iSDDevNum);
		run_command(cCmdA,0);
		giCurSDDevNum = I_iSDDevNum;
	}
	
	sprintf(cCmdA,"mmc read 0x%x 0x%x 0x%x",(unsigned)O_pbBuf,
			(unsigned)(I_dwBinSectorNum),(unsigned)I_dwBinReadSectors);
	run_command(cCmdA, 0);//

#else //][!MMC_CMD_SEPERATED_SDNUM	

	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",I_iSDDevNum,(unsigned)O_pbBuf,
			(unsigned)(I_dwBinSectorNum),(unsigned)I_dwBinReadSectors);
	run_command(cCmdA, 0);//

#endif //]MMC_CMD_SEPERATED_SDNUM

	return iRet;
}

static unsigned long _load_ntx_bin_header(int I_iSDDevNum,unsigned long I_dwBinSectorNum,
		unsigned char *O_pbBuf,unsigned long I_dwBufSize)
{
	unsigned long dwBinSize = 0;
	unsigned char *pbMagic;
	
	//ASSERT(I_dwBufSize>=512);
	_read_mmc(I_iSDDevNum,O_pbBuf,(unsigned long)(I_dwBinSectorNum-1),1) ;
	
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

static void _load_ntx_bin(int I_iSDDevNum,unsigned long I_dwBinSectorNum,
		unsigned long I_dwBinSectorsToLoad,unsigned char *O_pbBuf,unsigned long I_dwBufSize)
{
	//char cCmdA[128];
	
	if(I_dwBufSize<(I_dwBinSectorsToLoad*512)) {
		printf("%s() : buffer size not enough (buffer size %hu should >= %hu)!\n",
				__FUNCTION__,I_dwBufSize,(I_dwBinSectorsToLoad*512));
		return ;
	}
	
	_read_mmc(I_iSDDevNum,O_pbBuf,I_dwBinSectorNum,I_dwBinSectorsToLoad);
		 
}

void _load_isd_hwconfig(void)
{
	unsigned long dwChk;
	int iSD_IDX;

#ifdef CONFIG_MFG
	gptNtxHwCfg = 0x9FFFFE00;
	gdwNtxHwCfgSize = 110;
	return;
#endif
	
	if(gptNtxHwCfg) {
		return ;
	}

	iSD_IDX = GET_ISD_NUM();
	dwChk = _load_ntx_bin_header(iSD_IDX,SD_OFFSET_SECS_HWCFG,gbSectorBufA,sizeof(gbSectorBufA));
	if(dwChk>0) {
		_load_ntx_bin(iSD_IDX,SD_OFFSET_SECS_HWCFG,1,gbSectorBufA,sizeof(gbSectorBufA));
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
	int iSD_IDX;

	if(gptEsdNtxHwCfg) {
		return ;
	}

	iSD_IDX = GET_ESD_NUM();
	dwChk = _load_ntx_bin_header(iSD_IDX,SD_OFFSET_SECS_HWCFG,gbSectorBufA,sizeof(gbSectorBufA));
	if(dwChk>0) {
		_load_ntx_bin(iSD_IDX,SD_OFFSET_SECS_HWCFG,1,gbSectorBufA,sizeof(gbSectorBufA));
		memcpy((unsigned char *)&gtEsdNtxHwCfg,gbSectorBufA,sizeof(gtEsdNtxHwCfg));
		gptEsdNtxHwCfg = &gtEsdNtxHwCfg;
		gdwEsdNtxHwCfgSize = dwChk;
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
	else {
		printf("ESD hwconfig missing !\n");
	}
}



const static char gszKParamName_HwCfg[]="hwcfg";
const static char gszKParamName_logo[]="logo";
const static char gszKParamName_logo2[]="logo";
const static char gszKParamName_waveform[]="waveform";

static void _load_ntxbins_and_append_boot_args(char *O_cBufA,unsigned long I_ulBufSize,char *pszCurKernCmdLine)
{
	//char cCmdA[128];
	char cAppendStr[128];

	volatile unsigned char *pbMagic = gpbSectorBuffer+(512-16) ;
	unsigned long dwImgSize ;
	unsigned long dwReadSectors, dwReadBytes;
	unsigned long dwTotalReservedSize,dwRamSize;

	unsigned char *pbLoadAddr;
	char *pcTemp,*pcTemp2,cStore;
	unsigned long dwTemp,dwTemp2;
	int iLoadDeviceNum = GET_ISD_NUM();



#ifdef _MX6Q_//[
	int iLoadSecNO[] = {SD_OFFSET_SECS_HWCFG};
	char *szLoadNameA[] = {(char *)gszKParamName_HwCfg};
	// ] 
#else //][!_MX6Q_
	// [ WARNING : be pair .
	int iLoadSecNO[] = {SD_OFFSET_SECS_HWCFG,SD_OFFSET_SECS_WAVEFORM,SD_OFFSET_SECS_LOGO,SD_OFFSET_SECS_LOGO2};
	char *szLoadNameA[] = {(char *)gszKParamName_HwCfg,(char *)gszKParamName_waveform,(char *)gszKParamName_logo,(char *)gszKParamName_logo2};
	// ] 
#endif //] _MX6Q_	
	unsigned long dwLastLoadSecNo = 0;
	int i;


	if(giIsNtxBinsLoaded) {
		printf("%s : ntx bins load skip (has been loaded already) !\n",__FUNCTION__);
		return ;
	}

	if(!gptNtxHwCfg) {
		printf("[ERROR] %s() : NTX hwconfig not ready !\n",__FUNCTION__);
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
		if(i>0) {
			if(iLoadSecNO[i]<=dwLastLoadSecNo) {
				printf("[WARNING] Binaries load sequence should Lo->Hi !\n");
			}

			if(((dwLastLoadSecNo*512)+dwImgSize)>=(iLoadSecNO[i]*512)) {
				printf("skip load \"%s\" because of overwrited \n",szLoadNameA[i]);
				continue ;
			}
		}

		

		if(0==strcmp(szLoadNameA[i],gszKParamName_HwCfg) && gptNtxHwCfg && gdwNtxHwCfgSize>0 ) {
			dwImgSize = gdwNtxHwCfgSize;
			dwReadSectors = (dwImgSize>>9)+1;
			dwReadBytes = dwReadSectors<<9;

			pbLoadAddr -= dwReadBytes;
			memcpy(pbLoadAddr,(unsigned char *)gptNtxHwCfg,gdwNtxHwCfgSize);
			gptNtxHwCfg = (NTX_HWCONFIG *)pbLoadAddr;


			//printf("hwcfg p=%p,size=%lu\n",gptNtxHwCfg,gdwNtxHwCfgSize);
			sprintf(cAppendStr," %s_p=0x%x %s_sz=%d",
					szLoadNameA[i],(unsigned)(pbLoadAddr),
					szLoadNameA[i],(int)dwImgSize);
			strcat(O_cBufA,cAppendStr);

			dwTotalReservedSize+=dwReadBytes;
		}
		else {

			_read_mmc(iLoadDeviceNum,gpbSectorBuffer,(unsigned long)(iLoadSecNO[i]-1),1);
			
			if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
				&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
			{
				dwImgSize = *((unsigned long *)(pbMagic+8));
				dwReadSectors = (dwImgSize>>9)+1;
				dwReadBytes = dwReadSectors<<9;

				pbLoadAddr -= dwReadBytes;
				//printf("%s size=%d,sectors=%d,readbytes=%d,loadto=0x%p\n",
				//		szLoadNameA[i],dwImgSize,dwReadSectors,dwReadBytes,pbLoadAddr);
				_read_mmc(iLoadDeviceNum,pbLoadAddr,(unsigned long)(iLoadSecNO[i]),dwReadSectors);


				if(0==strcmp(szLoadNameA[i],gszKParamName_HwCfg)) {
					gptNtxHwCfg = (NTX_HWCONFIG *)pbLoadAddr;
					gdwNtxHwCfgSize = dwImgSize;
				}
				pcTemp = strstr(O_cBufA,szLoadNameA[i]) ;
				if(pcTemp) {
					dwTemp = strlen(szLoadNameA[i]);
					pcTemp2 = pcTemp+dwTemp+5+8+1+dwTemp+4+8;
					cStore = *pcTemp2;
					*pcTemp2 = '\0';
					//printf("%s():update %s from \"%s\" ",__FUNCTION__,szLoadNameA[i],pcTemp);
					sprintf(pcTemp,"%s_p=0x%-8x %s_sz=%-8d",
						szLoadNameA[i],(unsigned)(pbLoadAddr),
						szLoadNameA[i],(int)dwImgSize);
					//printf("to \"%s\" \n",pcTemp);
					*pcTemp2 = cStore;
				}
				else {
					sprintf(cAppendStr," %s_p=0x%-8x %s_sz=%-8d",
						szLoadNameA[i],(unsigned)(pbLoadAddr),
						szLoadNameA[i],(int)dwImgSize);

					dwTotalReservedSize+=dwReadBytes;
					strcat(O_cBufA,cAppendStr);
				}
			}
			else {
				printf("no \"%s\" bin header\n",szLoadNameA[i]);
			}
		}
		dwLastLoadSecNo = iLoadSecNO[i];
	}


#if !defined(_MX6Q_) //[
	pcTemp = strstr(pszCurKernCmdLine,"fbmem=");
	if(pcTemp) {
		pcTemp2 = strstr(pcTemp,"M");
		*pcTemp2 = 0;

		dwTemp2 = simple_strtoul(&pcTemp[6], NULL, 10);
		*pcTemp2 = 'M';
		dwTotalReservedSize += (unsigned long)(dwTemp2<<20);
		printf("fbmem=%dM ,reserved size=%u\n",(int)dwTemp2,(unsigned)dwTotalReservedSize);
	}

#endif //](_MX6Q_)
	
	if(0!=dwTotalReservedSize) 
	{

		dwTemp = (unsigned long)((dwRamSize-dwTotalReservedSize)>>20);
		dwTemp2 = dwTemp&0xffffffff;


		printf("Kernel RAM visiable size=%dM->%dM\n",(int)dwTemp,(int)dwTemp2);

		sprintf(cAppendStr," mem=%dM",(int)dwTemp2);
		strcat(O_cBufA,cAppendStr);
	}
	

	//run_command("mmc sw_dev 1", 0);// switch to external sdcard .

	giIsNtxBinsLoaded = 1;

}


volatile unsigned char *gpbKernelAddr=0; // kernel address . 
volatile unsigned long gdwKernelSize=0; // kernel size in byte .

static int _load_ntxkernel(unsigned char **O_ppbKernelAddr,unsigned long *O_pdwKernelSize)
{
	//char cCmdA[128];
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

	if(0==offset) {
		// load to unkown address .
		return 0;
	}

	if (gi_mmc_num_kernel<0) {
		gi_mmc_num_kernel = GET_ISD_NUM() ;
	}
	
#ifdef AUTO_DETECT_KIMGSIZE//[

	_read_mmc(gi_mmc_num_kernel,gpbSectorBuffer,\
			(unsigned long)(SD_OFFSET_SECS_KERNEL-1),1);

	if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
		&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
	{

		dwImgSize = *((unsigned long *)(pbMagic+8));
		printf("kernel size = %lu\n",dwImgSize);
		dwReadSectors = dwImgSize>>9|1;
		dwReadSectors += 6;
		//dwReadSectors = 0xfff;
		
		_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
			(unsigned long)(SD_OFFSET_SECS_KERNEL),dwReadSectors);
	}
	else 
#endif //] AUTO_DETECT_KIMGSIZE
	{
#ifdef AUTO_DETECT_KIMGSIZE//[

		printf("no kernel image signature !\n");
#endif //]AUTO_DETECT_KIMGSIZE
		dwImgSize = DEFAULT_LOAD_KERNEL_SZ;
		_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
				SD_OFFSET_SECS_KERNEL,DEFAULT_LOAD_KERNEL_SZ);
	
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
	//iRet = _load_ntxkernel(&gpbKernelAddr,&gdwKernelSize);
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
	//char cCmdA[128];
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

	if(0==offset) {
		// load to unkown address .
		return 0;
	}

	if (gi_mmc_num_kernel<0) {
		gi_mmc_num_kernel = GET_ISD_NUM() ;
	}
	

#ifdef AUTO_DETECT_KIMGSIZE//[

	for (i=0;i<sizeof(dwOffsetSecsINITRDA)/sizeof(dwOffsetSecsINITRDA[0]);i++) {

		_read_mmc(gi_mmc_num_kernel,gpbSectorBuffer,\
				(unsigned long)(dwOffsetSecsINITRDA[i]-1),1);

		if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
			&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
		{

			dwImgSize = *((unsigned long *)(pbMagic+8));
			printf("rd size = %lu\n",dwImgSize);
			dwReadSectors = dwImgSize>>9|1;
			dwReadSectors += 6;
			//dwReadSectors = 0xfff;

			_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
					(unsigned long)dwOffsetSecsINITRDA[i],dwReadSectors);

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

			_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
					SD_OFFSET_SECS_INITRD,DEFAULT_LOAD_RD_SZ);
		
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
	int iPwr_Key=-1,iUPG_Key=-1,iESD_in=-1,iUSB_in=-1;



	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}
	
#if 1 //[ debug code .
	printf("\n hwcfgp=%p,pcb=%d,customer=%d\n\n",gptNtxHwCfg,\
			gptNtxHwCfg->m_val.bPCB,gptNtxHwCfg->m_val.bCustomer);
#endif //]

	switch(gptNtxHwCfg->m_val.bPCB) {
#ifdef _MX50_ //[
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
	case 27://E50610.
		if(ntxup_wait_touch_recovery()) {
			printf("\n**********************************\n\n");
			printf("\n**  Boot from SD quick recovery **\n\n");
			printf("\n**********************************\n\n");		
			iRet = NTX_BOOTMODE_RECOVERY;
		}
		else
			iRet = NTX_BOOTMODE_ISD;
		break;
#endif //] _MX50_

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

		iUSB_in=ntx_detect_usb_plugin();
		iESD_in=ntxup_is_ext_card_inserted();
		iPwr_Key=_power_key_status();


		if(2!=gptNtxHwCfg->m_val.bUIStyle) 
		{
			if (12==gptNtxHwCfg->m_val.bKeyPad) {
				int iChkRecovery = 0;

				iRet = NTX_BOOTMODE_ISD;
				// No Key in this model .
				//iESD_in=ntxup_is_ext_card_inserted();
				//iPwr_Key=_power_key_status();

				if( iPwr_Key && 1==iUSB_in ) {

					printf("\n**********************************\n\n");
					printf("\n** 0. fastboot mode (NO Key Model) **\n\n");
					printf("\n**********************************\n\n");		
					iRet = NTX_BOOTMODE_FASTBOOT;
					fastboot_connection_timeout_us_set(2*1000*1000);
				}
				else {
					if( iPwr_Key ) {
						iUPG_Key=ntxup_wait_touch_recovery();

						if(iESD_in) {
							_load_esd_hwconfig();

							if ( (gptEsdNtxHwCfg && NTXHWCFG_TST_FLAG(gptEsdNtxHwCfg->m_val.bBootOpt,0)) || 
									(2==iUPG_Key) ) 
							{
								printf("\n**********************************\n\n");
								printf("\n* Boot from ESD (NO Key Model)  **\n\n");
								printf("\n**********************************\n\n");		
								iRet = NTX_BOOTMODE_ESD_UPG;
							}
							else {
								iChkRecovery = 1;
							}
						}
						else {
							iChkRecovery = 1;
						}
					}

					if( iChkRecovery && iPwr_Key && (1==iUPG_Key) ) {
						printf("\n**********************************\n\n");
						printf("\n** Boot recovery (NO Key Model) **\n\n");
						printf("\n**********************************\n\n");		
						iRet = NTX_BOOTMODE_RECOVERY;
					}
					else if( iPwr_Key && (3==iUPG_Key) ) {
						printf("\n**********************************\n\n");
						printf("\n** fastboot mode (NO Key Model) **\n\n");
						printf("\n**********************************\n\n");		
						iRet = NTX_BOOTMODE_FASTBOOT;
						fastboot_connection_timeout_us_set(30*1000*1000);
					}
				}
			}
			else  {
				iUPG_Key=ntxup_wait_key_esdupg();
				if( 1==iUPG_Key && 1==iUSB_in) {
					printf("\n**********************************\n\n");
					printf("\n** fastboot mode                **\n\n");
					printf("\n**********************************\n\n");
					iRet = NTX_BOOTMODE_FASTBOOT;
					fastboot_connection_timeout_us_set(30*1000*1000);
				}
				else 
				if(iESD_in && 1==iUPG_Key && iPwr_Key ) 
				{
					printf("\n**************************\n\n");
					printf("\n** Boot from ESD        **\n\n");
					printf("\n**************************\n\n");		
					iRet = NTX_BOOTMODE_ESD_UPG;
					_load_esd_hwconfig();
				}
				else 
				if(1==iUPG_Key) {
					printf("\n**********************************\n\n");
					printf("\n**  Boot from SD quick recovery **\n\n");
					printf("\n**********************************\n\n");		
					iRet = NTX_BOOTMODE_RECOVERY;
				}
				else {
					iRet = NTX_BOOTMODE_ISD;
				}
			}
		}
		else {
			iRet = NTX_BOOTMODE_ISD;
			//sprintf(cCmdA,"setenv bootcmd run bootcmd_mmc");
			//run_command(cCmdA,0);
			//_load_ntxrd(&gpbRDaddr,&gdwRDsize);
		}

#if 1//[ debug code .
		printf("ESDin=%d,UPGKey=%d,PWRKey=%d,USBin=%d\n",
				iESD_in,iUPG_Key,iPwr_Key,iUSB_in);
#endif //]

		break;
	}

#ifdef _MX6SL_ //[
	if( (NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bFrontLight_Flags,0)) && (iRet == NTX_BOOTMODE_ISD) )
	{
		frontLightCtrl();	
	}
#endif //]_MX6SL_
	return iRet;
}
#ifdef CONFIG_FASTBOOT //[

#define SECTOR_SIZE				512

//unit: sector = 512B
#define MBR_OFFSET		0
#define MBR_SIZE			1
#define SN_OFFSET                            1
#define SN_SIZE                                  1

#define BOOTLOADER_OFFSET	2

#ifdef _MX6SL_//[
	#define BOOTLOADER_SIZE	1022
	#define HWCFG_OFFSET	1024
#elif defined(_MX6Q_) //][
	#define BOOTLOADER_SIZE	1522
	#define HWCFG_OFFSET	1524
#else //][
	#error "unkown platform !!!"
#endif //]_MX6SL_

#define HWCFG_SIZE		2
#define WAVEFORM_OFFSET			14336//7MB
#define WAVEFORM_SIZE				20480//10MB
#define LOGO_OFFSET					34816 //17MB
#define LOGO_SIZE						4096 //2MB
#define KERNEL_OFFSET 			2048
#define KERNEL_SIZE					12284
#define BOOTENV_OFFSET 			1536
#define BOOTENV_SIZE				510

#define NTX_FASTBOOT_MAX_PPTN	16
#define NTX_FASTBOOT_MAX_BPTN	12
// fastboot binaries ...
static struct fastboot_ptentry ntx_fb_binsA[NTX_FASTBOOT_MAX_BPTN]= {
	{
		.name="mbr",
		.start=MBR_OFFSET,
		.length=MBR_SIZE,
		.flags=0,
		.partition_id=0,
	},
	{
		.name="sn",
		.start=SN_OFFSET,
		.length=SN_SIZE,
		.flags=0,
		.partition_id=0,
	},
	{
		.name="bootloader",
		.start=BOOTLOADER_OFFSET,
		.length=BOOTLOADER_SIZE,
		.flags=0,
		.partition_id=0,
	},
	{
		.name="hwcfg",
		.start=HWCFG_OFFSET,
		.length=HWCFG_SIZE,
		.flags=0,
		.partition_id=0,
	},
	{
		.name="waveform",
		.start=WAVEFORM_OFFSET,
		.length=WAVEFORM_SIZE,
		.flags=0,
		.partition_id=0,
	},
	{
		.name="logo",
		.start=LOGO_OFFSET,
		.length=LOGO_SIZE,
		.flags=0,
		.partition_id=0,
	},
	{
		.name="bootenv",
		.start=BOOTENV_OFFSET,
		.length=BOOTENV_SIZE,
		.flags=0,
		.partition_id=0,
	},


	// binary end tag .
	{
		.name=0
	}

};

// fastboot partitions ...
static struct fastboot_ptentry ntx_fb_partsA[NTX_FASTBOOT_MAX_PPTN]= {
#if 0//[
#ifdef CONFIG_ANDROID_BOOT_PARTITION_MMC//[
	{
		.name="boot",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_BOOT_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_BOOT_PARTITION_MMC
#ifdef CONFIG_ANDROID_RECOVERY_PARTITION_MMC//[
	{
		.name="recovery",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_RECOVERY_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_RECOVERY_PARTITION_MMC
#ifdef CONFIG_ANDROID_DATA_PARTITION_MMC//[
	{
		.name="data",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_DATA_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_DATA_PARTITION_MMC
#ifdef CONFIG_ANDROID_SYSTEM_PARTITION_MMC//[
	{
		.name="system",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_SYSTEM_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_SYSTEM_PARTITION_MMC
#ifdef CONFIG_ANDROID_CACHE_PARTITION_MMC//[
	{
		.name="cache",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_CACHE_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_CACHE_PARTITION_MMC
#ifdef CONFIG_ANDROID_VENDOR_PARTITION_MMC//[
	{
		.name="vendor",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_VENDOR_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_VENDOR_PARTITION_MMC
#ifdef CONFIG_ANDROID_DEVICE_PARTITION_MMC//[
	{
		.name="device",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_DEVICE_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_DEVICE_PARTITION_MMC
#ifdef CONFIG_ANDROID_MISC_PARTITION_MMC//[
	{
		.name="misc",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_MISC_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_MISC_PARTITION_MMC
#ifdef CONFIG_ANDROID_EMERGENCY_PARTITION_MMC//[
	{
		.name="emergency",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_EMERGENCY_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_EMERGENCY_PARTITION_MMC
#ifdef CONFIG_ANDROID_MEDIA_PARTITION_MMC//[
	{
		.name="media",
		.start=0,
		.length=0,
		.flags=0,
		.partition_id=CONFIG_ANDROID_MEDIA_PARTITION_MMC,
	},
#endif //]CONFIG_ANDROID_MEDIA_PARTITION_MMC
#endif//]
};


static const char gszLinuxKernel_Name[]="kernel";
static const char gszLinuxRootFS_Name[]="rootfs";
static const char gszLinuxRecoveryFS_Name[]="recoveryfs";
static const char gszLinuxVfatFS_Name[]="vfat";

static const char gszDroidSystem_Name[]="system";
static const char gszDroidData_Name[]="data";
static const char gszDroidCache_Name[]="cache";
static const char gszDroidRecovery_Name[]="recovery";
static const char gszDroidBoot_Name[]="boot";
static const char gszDroidVendor_Name[]="vendor";
static const char gszDroidDevice_Name[]="device";
static const char gszDroidMisc_Name[]="misc";
static const char gszDroidEmergency_Name[]="emergency";
static const char gszDroidMedia_Name[]="media";

void hotfix_avoid_mmc_partition_cannot_be_recognized(void)
{
	// workarround to avoid mmc partition cannot be recognized on Q92
	//if(46==gptNtxHwCfg->m_val.bPCB) 
	if(NTXHW_BOOTDEV_SD==ntxhw_get_bootdevice_type())
	{
		// E60Q9X 
		run_command("mmcinfo", 0);//
		run_command("mmc rescan", 0);//
	}
}

void ntx_config_fastboot_layout(void)
{
	int i;
	disk_partition_t info;

	int mmcc=0;
	struct mmc *mmc;
	block_dev_desc_t *dev_desc = NULL;
	unsigned sector, partno = -1;

	hotfix_avoid_mmc_partition_cannot_be_recognized();

	memset((void *)&info, 0 , sizeof(disk_partition_t));
	/* i.MX use MBR as partition table, so this will have
	   to find the start block and length for the
	   partition name and register the fastboot pte we
	   define the partition number of each partition in
	   config file
	 */
	mmc = find_mmc_device(mmcc);
	if (!mmc) {
		printf("%s: cannot find '%d' mmc device\n",__FUNCTION__, mmcc);
		return;
	}
	dev_desc = get_dev("mmc", mmcc);
	if (NULL == dev_desc) {
		printf("** Block device MMC %d not supported\n", mmcc);
		return;
	}

	/* below was i.MX mmc operation code */
	if (mmc_init(mmc)) {
		printf("mmc%d init failed\n", mmcc);
		return;
	}

	if(2!=gptNtxHwCfg->m_val.bUIStyle) {
		for (i=0;i<sizeof(ntx_fb_binsA)/sizeof(ntx_fb_binsA[0]);i++) {
			if(0==ntx_fb_binsA[i].name[0]) {
				break;
			}
		}

		strcpy(ntx_fb_binsA[i].name,gszLinuxKernel_Name);
		ntx_fb_binsA[i].partition_id=0;
		ntx_fb_binsA[i].start=KERNEL_OFFSET;
		ntx_fb_binsA[i].length=KERNEL_SIZE;
		ntx_fb_binsA[i].flags=0;
		i++;

	}

	switch(gptNtxHwCfg->m_val.bSysPartType) {
	case 0://TYPE1
		strcpy(ntx_fb_partsA[0].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszLinuxVfatFS_Name);
		ntx_fb_partsA[1].partition_id=2;
		break;

	case 1://TYPE2
		strcpy(ntx_fb_partsA[0].name,gszLinuxRecoveryFS_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszLinuxVfatFS_Name);
		ntx_fb_partsA[2].partition_id=3;
		break;

	case 2://TYPE3
		strcpy(ntx_fb_partsA[0].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszLinuxRecoveryFS_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszLinuxVfatFS_Name);
		ntx_fb_partsA[2].partition_id=3;
		break;

	case 3://TYPE4
		strcpy(ntx_fb_partsA[0].name,gszLinuxVfatFS_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[1].partition_id=2;
		break;

	case 4://TYPE5
		strcpy(ntx_fb_partsA[0].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[0].partition_id=1;
		break;

	case 5://TYPE6
		strcpy(ntx_fb_partsA[0].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszLinuxVfatFS_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszLinuxRecoveryFS_Name);
		ntx_fb_partsA[2].partition_id=3;
		break;

	case 6://TYPE7
		strcpy(ntx_fb_partsA[0].name,gszLinuxVfatFS_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszLinuxRecoveryFS_Name);
		ntx_fb_partsA[2].partition_id=3;
		break;

	case 7://TYPE8
		strcpy(ntx_fb_partsA[0].name,gszDroidMedia_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszDroidSystem_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszDroidRecovery_Name);
		ntx_fb_partsA[2].partition_id=4;

		strcpy(ntx_fb_partsA[3].name,gszDroidData_Name);
		ntx_fb_partsA[3].partition_id=5;

		strcpy(ntx_fb_partsA[4].name,gszDroidCache_Name);
		ntx_fb_partsA[4].partition_id=6;
		break;

	case 8://TYPE9
		strcpy(ntx_fb_partsA[0].name,gszDroidBoot_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszDroidRecovery_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszDroidMedia_Name);
		ntx_fb_partsA[2].partition_id=4;

		strcpy(ntx_fb_partsA[3].name,gszDroidSystem_Name);
		ntx_fb_partsA[3].partition_id=5;

		strcpy(ntx_fb_partsA[4].name,gszDroidCache_Name);
		ntx_fb_partsA[4].partition_id=6;

		strcpy(ntx_fb_partsA[5].name,gszDroidData_Name);
		ntx_fb_partsA[5].partition_id=7;

		strcpy(ntx_fb_partsA[6].name,gszDroidVendor_Name);
		ntx_fb_partsA[6].partition_id=8;

		strcpy(ntx_fb_partsA[7].name,gszDroidMisc_Name);
		ntx_fb_partsA[7].partition_id=9;

		break;

	case 9://TYPE10
		strcpy(ntx_fb_partsA[0].name,gszDroidMedia_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszDroidSystem_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszDroidRecovery_Name);
		ntx_fb_partsA[2].partition_id=4;

		strcpy(ntx_fb_partsA[3].name,gszDroidData_Name);
		ntx_fb_partsA[3].partition_id=5;

		strcpy(ntx_fb_partsA[4].name,gszDroidCache_Name);
		ntx_fb_partsA[4].partition_id=6;

		strcpy(ntx_fb_partsA[5].name,gszDroidEmergency_Name);
		ntx_fb_partsA[5].partition_id=7;

		break;

	case 10://TYPE11
		strcpy(ntx_fb_partsA[0].name,gszDroidBoot_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszDroidRecovery_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszDroidData_Name);
		ntx_fb_partsA[2].partition_id=4;

		strcpy(ntx_fb_partsA[3].name,gszDroidSystem_Name);
		ntx_fb_partsA[3].partition_id=5;

		strcpy(ntx_fb_partsA[4].name,gszDroidCache_Name);
		ntx_fb_partsA[4].partition_id=6;

		strcpy(ntx_fb_partsA[5].name,gszDroidVendor_Name);
		ntx_fb_partsA[5].partition_id=7;

		strcpy(ntx_fb_partsA[6].name,gszDroidMisc_Name);
		ntx_fb_partsA[6].partition_id=8;

		break;

	case 11://TYPE12
		strcpy(ntx_fb_partsA[0].name,gszDroidBoot_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszDroidRecovery_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszDroidMedia_Name);
		ntx_fb_partsA[2].partition_id=4;

		strcpy(ntx_fb_partsA[3].name,gszDroidSystem_Name);
		ntx_fb_partsA[3].partition_id=5;

		strcpy(ntx_fb_partsA[4].name,gszDroidCache_Name);
		ntx_fb_partsA[4].partition_id=6;

		strcpy(ntx_fb_partsA[5].name,gszDroidData_Name);
		ntx_fb_partsA[5].partition_id=7;

		strcpy(ntx_fb_partsA[6].name,gszDroidVendor_Name);
		ntx_fb_partsA[6].partition_id=8;

		strcpy(ntx_fb_partsA[7].name,gszDroidMisc_Name);
		ntx_fb_partsA[7].partition_id=9;

		strcpy(ntx_fb_partsA[8].name,gszDroidEmergency_Name);
		ntx_fb_partsA[8].partition_id=10;

		break;

	case 12://TYPE13
		strcpy(ntx_fb_partsA[0].name,gszLinuxVfatFS_Name);
		ntx_fb_partsA[0].partition_id=1;

		strcpy(ntx_fb_partsA[1].name,gszLinuxRecoveryFS_Name);
		ntx_fb_partsA[1].partition_id=2;

		strcpy(ntx_fb_partsA[2].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[2].partition_id=3;
		break;

	default:
		printf("%s():invalid partition type %d\n",__FUNCTION__,
				(int)gptNtxHwCfg->m_val.bSysPartType);
		break;
	}

	for (i=0;i<sizeof(ntx_fb_binsA)/sizeof(ntx_fb_binsA[0]);i++)
	{
		if(0==ntx_fb_binsA[i].name[0]) {
			printf("%s():%d binaries partition added\n",__FUNCTION__,i);
			break;
		}
#if 0
		printf("%s(%d),adding fastboot binary parts \"%s\",start=%d,len=%d\n",
				__FUNCTION__,__LINE__,ntx_fb_binsA[i].name,
				ntx_fb_binsA[i].start,ntx_fb_binsA[i].length);
#endif

		fastboot_flash_add_ptn(&ntx_fb_binsA[i]);
	}

	for (i=0;i<sizeof(ntx_fb_partsA)/sizeof(ntx_fb_partsA[0]);i++)
	{
		if(0==ntx_fb_partsA[i].name[0]) {
			printf("%s():%d mbr partition added\n",__FUNCTION__,i);
			break;
		}
#if 0
		printf("%s(%d),adding fastboot part \"%s\",start=%d,len=%d\n",
				__FUNCTION__,__LINE__,ntx_fb_partsA[i].name,
				ntx_fb_partsA[i].start,ntx_fb_partsA[i].length);
#endif

		if(0==ntx_fb_partsA[i].length&&ntx_fb_partsA[i].partition_id!=0) {
			if (get_partition_info(dev_desc,ntx_fb_partsA[i].partition_id , &info)) {
				printf("%s: get partition:%s fail !!\n",__FUNCTION__, ntx_fb_partsA[i].name);
				continue ;
			}
			ntx_fb_partsA[i].start = info.start;
			ntx_fb_partsA[i].length = info.size;
		}

		fastboot_flash_add_ptn(&ntx_fb_partsA[i]);
	}
	//fastboot_flash_dump_ptn();

}

#else //][!CONFIG_FASTBOOT

void ntx_config_fastboot_layout(void){}

#endif //]CONFIG_FASTBOOT

#ifdef _MX6SL_//[
static const NTX_GPIO gt_ntx_gpio_bat_low= {
	MX6SL_PAD_KEY_COL2__GPIO_3_28,  // pin pad/mux control .
	3, // gpio group .
	28, // gpio number .
	0, // NC .
	0, // not inited .
	"Bat_low", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
#endif//]_MX6SL_

void isBatCritical (void)
{
#ifdef _MX6SL_//[
	unsigned char bBufA[2];

	if (49!=gptNtxHwCfg->m_val.bPCB)	// only block in boot with BN models.
		return;

	if (1 == gptNtxHwCfg->m_val.bPMIC) {
		if (0==ntx_gpio_get_value(&gt_ntx_gpio_bat_low)) {
			/* bypass battery critical if usb / adaptor detected. */
			i2c_read(0x32, 0xBD, 1, bBufA, 1);
			//printf("Raw %x\n", bBufA[0]);
			if (bBufA[0] & 0xC0) {
				/* USB IN */
				unsigned short waitTime = 0;
				unsigned char  interval = 2; //sec
				unsigned short timeout = 480; //sec
				printf("Battery critical !! USB_IN, charge a while\n");
				while(1) {
					if (waitTime*interval > timeout) {
						printf("\nCharge Timeout !! (%d) secs\n", timeout);
						return;
					}
					if (1==ntx_gpio_get_value(&gt_ntx_gpio_bat_low)) {
						printf("\nBattery critical !! charge completed, take (%d) secs\n", waitTime*interval);
						return;
					}
					i2c_read(0x32, 0xBD, 1, bBufA, 1);
					//printf("Raw %x\n", bBufA[0]);
					if ((bBufA[0] & 0xC0) == 0) {
						printf("\nBattery critical !! USB_OUT\n");
						break;
					}
					printf(".");
					waitTime++;
					udelay(interval*1000000);
				}
			}
			printf("Battery critical !! force power off.\n");
			/* Not repeat power ON after power off(Power Off/N_OE) */
			/*__ricoh61x_write(ricoh61x_i2c_client, RICOH61x_PWR_REP_CNT, 0x0);*/
			bBufA[0] = 0x0;
			i2c_write(0x32, 0x0F, 1, bBufA, 1);

			/* Power OFF */
			/*__ricoh61x_write(ricoh61x_i2c_client, RICOH61x_PWR_SLP_CNT, 0x1);*/
			bBufA[0] = 0x1;
			i2c_write(0x32, 0x0E, 1, bBufA, 1);
		}
	}
#endif//]_MX6SL_
}

void ntx_preboot(int iBootType,char *I_pszKernCmdLine,
		char *O_pszKernCmdLine,unsigned long I_dwKernCmdLineSize)
{
	char *pcEnv_bootargs=I_pszKernCmdLine;
	char *pcLPJ;
	unsigned char bSysPartType,bRootFsType,bUIStyle;
	unsigned long dwBootArgsLen;
	char cCmdA[128];
	
#ifdef CONFIG_MFG
	return;
#endif

	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}
	
	isBatCritical();
	
	giNtxBootMode = _detect_bootmode();
	_load_ntxbins_and_append_boot_args(O_pszKernCmdLine,I_dwKernCmdLineSize,I_pszKernCmdLine);


	gi_mmc_num_kernel = GET_ISD_NUM() ;
	sprintf(cCmdA," boot_port=%d",_get_boot_sd_number());
	strcat(O_pszKernCmdLine,cCmdA);

	switch(giNtxBootMode)	{
	case NTX_BOOTMODE_ISD:
		if(gptNtxHwCfg) {
			if(9==gptNtxHwCfg->m_val.bCustomer) {
				_led_R(0);
				_led_G(0);
				_led_B(1);
			}
		}
		else {
			printf("boot normal : no hwconfig !\n");
		}
		break;
	case NTX_BOOTMODE_FASTBOOT:
#ifdef CONFIG_FASTBOOT //[
		if(gptNtxHwCfg) {
			if(9==gptNtxHwCfg->m_val.bCustomer) {
				_led_R(1);
				_led_G(1);
				_led_B(1);
			}
		}
		else {
			printf("fastboot : no hwconfig !\n");
		}
		run_command("fastboot q0",0);
		giNtxBootMode=NTX_BOOTMODE_ISD;
		break;
#endif//] CONFIG_FASTBOOT
	case NTX_BOOTMODE_RECOVERY:
		if(gptNtxHwCfg) {
			if(9==gptNtxHwCfg->m_val.bCustomer) {
				_led_R(1);
				_led_G(0);
				_led_B(0);
			}
		}
		else {
			printf("boot recovery : no hwconfig !\n");
		}
		sprintf(cCmdA,"setenv bootcmd run bootcmd_recovery");
		run_command(cCmdA,0);
		break;

	case NTX_BOOTMODE_ESD_UPG:
		gi_mmc_num_kernel = GET_ESD_NUM() ;

		sprintf(cCmdA,"setenv bootcmd run bootcmd_SD");
		run_command(cCmdA,0);
		break;

	}


	if(!gpbKernelAddr && 0==iBootType) {
		// bootm to load nt kernel .
		_load_ntxkernel(&gpbKernelAddr,&gdwKernelSize);
	}

	dwBootArgsLen=strlen(O_pszKernCmdLine);
	if(dwBootArgsLen>=I_dwKernCmdLineSize) {
		printf("[ERROR] kernel command buffer size not enough !\n");
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
		
		_load_ntx_bin(GET_ISD_NUM(),SD_OFFSET_SECS_SNMAC,1,gbSectorBufA,512);
		if(2==bUIStyle)
		{
			// Android UI .

			strcat(O_pszKernCmdLine," androidboot.serialno=");
			strcat(O_pszKernCmdLine,(char *)(&gbSectorBufA[3]));
			//printf("bootargs=\"%s\"\n",O_pszKernCmdLine);

#if 0
			if(pcEnv_bootargs&&0==strstr(pcEnv_bootargs,"androidboot.hardware=")) {
				char *pszPCB_name;

				pszPCB_name=NtxHwCfg_GetCfgFldStrVal(gptNtxHwCfg,HWCFG_FLDIDX_PCB);
				if(pszPCB_name) {
					strcat(O_pszKernCmdLine," androidboot.hardware=");
					strcat(O_pszKernCmdLine,pszPCB_name);
				}
				else {
					strcat(O_pszKernCmdLine," androidboot.hardware=freescale");
				}
			}
#endif

		}
		else {
			// Linux UI .
			//if(!gpbRDaddr) {
			//	_load_ntxrd(&gpbRDaddr,&gdwRDsize);
			//}
		}

		//if(!gpbRDaddr)
		{
			

			if(pcEnv_bootargs&&0==strstr(pcEnv_bootargs,"rootfstype=")) {
				printf("hwcfg rootfstype : %d\n",bRootFsType);
				switch(bRootFsType)
				{
				case 0:
					strcat(O_pszKernCmdLine," rootfstype=ext2");
					break;
				default:
				case 1:
					strcat(O_pszKernCmdLine," rootfstype=ext3");
					break;
				case 2:
					strcat(O_pszKernCmdLine," rootfstype=ext4");
					break;
				case 3:
					strcat(O_pszKernCmdLine," rootfstype=vfat");
					break;
				}
			}
			
			
		
			if(pcEnv_bootargs&&0==strstr(pcEnv_bootargs,"root=")) {
				char cTempA[5] = "0";

				printf("hwcfg partition type : %d\n",(int)bSysPartType);
				strcat(O_pszKernCmdLine," root=/dev/mmcblk");
				if(NTX_BOOTMODE_ISD==giNtxBootMode) {
					cTempA[0]='0';
					strcat(O_pszKernCmdLine,cTempA);
					
					switch (gptNtxHwCfg->m_val.bSysPartType)
					{
					default:
					case 0://TYPE1
					case 2://TYPE3
					case 4://TYPE5
					case 5://TYPE6
						strcat(O_pszKernCmdLine,"p1");
						break;
					case 1://TYPE2
					case 3://TYPE4
					case 6://TYPE7
						strcat(O_pszKernCmdLine,"p2");
						//strcat(O_pszKernCmdLine," rootdelay=3");
						break;
					case 12://TYPE13
						strcat(O_pszKernCmdLine,"p3");
						break;
					}
				}
				else if(NTX_BOOTMODE_ESD_UPG==giNtxBootMode){
					cTempA[0]='1';
					strcat(O_pszKernCmdLine,cTempA);
									
					switch (bSysPartType)
					{
					default:
					case 0://TYPE1
					case 2://TYPE3
					case 4://TYPE5
					case 5://TYPE6
						strcat(O_pszKernCmdLine,"p1");
						break;
					case 1://TYPE2
					case 3://TYPE4
					case 6://TYPE7
						strcat(O_pszKernCmdLine,"p2");
						break;
					case 12://TYPE13
						strcat(O_pszKernCmdLine,"p3");
						break;
					}			
				}
				else if(NTX_BOOTMODE_RECOVERY==giNtxBootMode) {
					cTempA[0]='0';
					strcat(O_pszKernCmdLine,cTempA);
					
					switch (gptNtxHwCfg->m_val.bSysPartType)
					{
					default:
						strcat(O_pszKernCmdLine,"p1");
						break;
					case 12://TYPE13
					case 2://TYPE3
						strcat(O_pszKernCmdLine,"p2");
						break;
					case 1://TYPE2
						strcat(O_pszKernCmdLine,"p1");
						break;
					case 5://TYPE6
					case 6://TYPE7
						strcat(O_pszKernCmdLine,"p3");
						break;
					case 7://TYPE8
						strcat(O_pszKernCmdLine,"p4");
						break;
					}			
				}
			}



		}
		
		
#ifdef _MX50_ //[
		if(pcEnv_bootargs) {
			if(2==gptNtxHwCfg->m_val.bCPUFreq) {
				// 1G Hz .
				strcat(O_pszKernCmdLine," mx50_1GHz");
				pcLPJ = strstr(pcEnv_bootargs,"lpj=");
				if(pcLPJ) {
					strncpy(pcLPJ+4,"4997120",7);
				}
				else {
					//strcat(O_pszKernCmdLine," lpj=4997120");
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
					strcat(O_pszKernCmdLine," lpj=3997696");
				}
				 
				*/
			}
		}
#endif //]_MX50_

#ifdef MAC_BOFFSET_IN_SNMAC_SECTOR//[
		if(pcEnv_bootargs&&0==strstr(pcEnv_bootargs,"fec_mac=")) {
			if( 'M'==(char)gbSectorBufA[MAC_BOFFSET_IN_SNMAC_SECTOR+0] && \
					'A'==(char)gbSectorBufA[MAC_BOFFSET_IN_SNMAC_SECTOR+1] && \
					'C'==(char)gbSectorBufA[MAC_BOFFSET_IN_SNMAC_SECTOR+2] && \
					'='==(char)gbSectorBufA[MAC_BOFFSET_IN_SNMAC_SECTOR+3] && \	
					'\0'==(char)gbSectorBufA[MAC_BOFFSET_IN_SNMAC_SECTOR+4+17] )
			{
				strcat(O_pszKernCmdLine," fec_mac=");
				strcat(O_pszKernCmdLine,&gbSectorBufA[MAC_BOFFSET_IN_SNMAC_SECTOR+4]);
			}
		}
#endif//]MAC_BOFFSET_IN_SNMAC_SECTOR

		if(9==gptNtxHwCfg->m_val.bCustomer) {
			if(pcEnv_bootargs&&0==strstr(pcEnv_bootargs,"quiet")) {
				strcat(O_pszKernCmdLine," quiet");
			}
		}

	} //] in hwconfig field ...

	
	//run_command("load_ntxrd", 0);// 
}


void ntx_prebootm(void)
{
	char cCmdA[500];
	char *pcEnv_bootargs=0;

	pcEnv_bootargs=getenv("bootargs");
	if(0==pcEnv_bootargs) {
		printf("%s() [WARNING] no bootargs !!! \n",__FUNCTION__);
		//return ;
	}

	ntx_preboot(0,pcEnv_bootargs,gszBootArgs,BOOTARGS_BUF_SIZE);

	if(pcEnv_bootargs) {
		sprintf(cCmdA,"setenv bootargs ${bootargs} %s",gszBootArgs);
		run_command(cCmdA, 0);// 
		//printf("%s : cmd=%s\n",__FUNCTION__,cCmdA);	
	}
	
}
void ntx_prebooti(char *I_pszKernCmdLine)
{
	char cCmdA[500];
	char *cmdline = getenv ("bootargs");


	if(cmdline) {
		printf ("bootargs %s\n",cmdline);
		ntx_preboot(1,cmdline,gszBootArgs,BOOTARGS_BUF_SIZE);
		sprintf(cCmdA,"setenv bootargs %s %s",cmdline,gszBootArgs);
	}
	else 
	{
		ntx_preboot(1,I_pszKernCmdLine,gszBootArgs,BOOTARGS_BUF_SIZE);
		sprintf(cCmdA,"setenv bootargs %s %s",I_pszKernCmdLine,gszBootArgs);
	}
	run_command(cCmdA, 0);// 
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
	return iRet;
}

U_BOOT_CMD(load_ntxbins, 2, 0, do_load_ntxbins,
	"load_ntxbins - netronix binaries load \n",
	"load_ntxbins "
		" - load netronix binaries from sd card (hwcfg,logo,waveform).\n"
);
//] gallen add 2011/03/02



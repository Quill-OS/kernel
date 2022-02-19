
/* 
 * 這個檔案可被U-BOOT,開發板.c 板引入 .
 * 
 * 
 */

#include <mmc.h>


#include <ext_common.h>
#include <ext4fs.h>
#include <common.h>
#include <timer.h>

#ifdef CONFIG_FASTBOOT //[
	#include <fastboot.h>
#endif //]CONFIG_FASTBOOT
#ifdef CONFIG_FSL_FASTBOOT //[
	#include <fsl_fastboot.h>
#endif //]CONFIG_FSL_FASTBOOT
#include "ntx_hwconfig.h"
#include "ntx_hw.h"

#define ASSERT(x)	if(!(x))printf("\n\n[WARNING]\nassertion (%s) failed !!!\n\n\n",#x)
//#define DEBUG
//#include <common.h>

#define USB_OTG_FASTBOOT_MODE		1// enter fastboot mode by USB OTG .
#define FASTBOOT_TIMEOUT_SETTING	1

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
#define SD_OFFSET_SECS_DTB		1286
#define SD_OFFSET_SECS_RAMFS		20480

#ifdef ADVANCE_WAVEFORM_FILE//[
	#define SD_OFFSET_SECS_BOOTWAVEFORM		SD_OFFSET_SECS_WAVEFORM
#else //][!ADVANCE_WAVEFORM_FILE
	#define SD_OFFSET_SECS_BOOTWAVEFORM		55296
#endif //] ADVANCE_WAVEFORM_FILE

#define SD_OFFSET_SECS_NTXFW		1030

#define SD_OFFSET_SECS_SNMAC		1
//#define SD_OFFSET_SECS_SN		1

#define DEFAULT_LOAD_KERNEL_SZ	18432
#define DEFAULT_LOAD_RD_SZ	8192
//#define KERNEL_RAM_ADDR		CONFIG_LOADADDR


//#define MAC_BOFFSET_IN_SNMAC_SECTOR		128

//#define USE_HWCONFIG

#ifdef USE_HWCONFIG//[
#else //][!USE_HWCONFIG
const char gszNtxHwCfgMagic[]="HW CONFIG ";// hw config tool magic .

const char * gszPCBA[]={ 
	"E60800","E60810","E60820","E90800","E90810", //  0~4 
	"E60830","E60850","E50800","E50810","E60860", //  5~9
	"E60MT2","E60M10","E60610","E60M00","E60M30", // 10~14
	"E60620","E60630","E60640","E50600","E60680", // 15~19
	"E60610C","E60610D","E606A0","E60670","E606B0", // 20~24
	"E50620","Q70Q00","E50610","E606C0","E606D0", // 25~29
	"E606E0","E60Q00","E60Q10","E60Q20","E606F0",	// 30~34
	"E606F0B","E60Q30","E60QB0","E60QC0","A13120",// 35~39
	"E60Q50","E606G0","E60Q60","E60Q80","A13130",// 40~44
	"E606H2","E60Q90","ED0Q00","E60QA0","E60QD0",// 45~49
	"E60QF0","E60QH0","E60QG0","H70000","ED0Q10",// 50~54 
	"E70Q00","H40000","NC","E60QJ0","E60QL0",// 55~59
	"E60QM0", // 60~65
};
#endif //]USE_HWCONFIG

#define NTX_HWCFG_PRELOAD_ADDR	0x9FFFFE00
#define NTX_SN_PRELOAD_ADDR			0x9FFFFC00

const static unsigned char gszNtxBinMagicA[4]={0xff,0xf5,0xaf,0xff};


volatile NTX_HWCONFIG *gptNtxHwCfg=0,gtEsdNtxHwCfg,*gptEsdNtxHwCfg=0;
unsigned long gdwNtxHwCfgSize = 0,gdwEsdNtxHwCfgSize = 0;
#define NTX_HWCFG_SRC_SDMMC		0
#define NTX_HWCFG_SRC_RAM			1
volatile int giNtxHwCfgSrc = NTX_HWCFG_SRC_SDMMC;

volatile unsigned char *gpbWaveform,*gpbLogo;
volatile unsigned long gdwWaveformSize,gdwLogoSize;



#define BOOTARGS_BUF_SIZE	1024	
//char gszBootArgs[BOOTARGS_BUF_SIZE]="root=/dev/mmcblk0p1 noinitrd rootdelay=1 rw rootfstype=ext3 console=ttyS0,115200 uart_dma";
//char gszBootArgs[BOOTARGS_BUF_SIZE]="noinitrd rw rootfstype=ext3 console=ttyS0,115200 uart_dma";
char gszBootArgs[BOOTARGS_BUF_SIZE]="";
char gszMMC_Boot_Cmd[128]="";

//static unsigned char gbSectorBufA[512],*gpbSectorBuffer=gbSectorBufA;

char gcNTX_SN[512],*gpszNTX_SN;


#define NTX_BOOTMODE_NA		(-1)
#define NTX_BOOTMODE_ISD		0 // normal boot mode .
#define NTX_BOOTMODE_ESD_UPG	1 // external upgrade mode .
#define NTX_BOOTMODE_RECOVERY	2 // internal recovery mode .
#define NTX_BOOTMODE_FASTBOOT	3 // fastboot mode .
#define NTX_BOOTMODE_SDOWNLOAD	4 // USB serial download mode .
#define NTX_BOOTMODE_SPI		5 // USB spi mode .

static volatile int giNtxBootMode = NTX_BOOTMODE_NA ;


//#define MMC_DEV_ISD		0
//#define MMC_DEV_ESD		1

#define MMC_NUM_NUKOWN	(-1)
#define MMC_NUM_NONE		(-2)
static int gi_mmc_num_kernel=-1;
static int gi_mmc_isd_num=0;
static int gi_mmc_esd_num=1;

#define AUTO_DETECT_KIMGSIZE		1


// external function ...
extern int ntxup_wait_key_downloadmode(void);
extern void frontLightCtrl(void);
extern int ntxup_wait_touch_recovery(void);
extern int ntxup_is_ext_card_inserted(void);
extern int ntxup_wait_key_esdupg(void);

// internal help functions ...
static int _detect_bootmode_and_append_boot_args(char *O_cBufA,unsigned long I_ulBufSize);

int ntx_is_fastboot_abort_inusbremove(void);


#define GET_SD_NUM(_i)	({\
	gi_mmc_##_i##sd_num; \
})

#define GET_ISD_NUM()	GET_SD_NUM(i)
#define GET_ESD_NUM()	GET_SD_NUM(e)


#ifdef FASTBOOT_TIMEOUT_SETTING//[
extern unsigned long fastboot_connection_timeout_us_set(unsigned long dwTimeoutSetUS);
extern int fastboot_connection_abortchk_setup(int (*fastboot_abort_check_fn)(void));
#else //][!FASTBOOT_TIMEOUT_SETTING
#warning "[WARNING] FASTBOOT_TIMEOUT_SETTING not defined !!"

unsigned long fastboot_connection_timeout_us_set(unsigned long dwTimeOutUS)
{
	return 0;
}
int fastboot_connection_abort_at_usb_remove_chk_setup(int (*func)(void))
{
	return 0;
}

#endif //] FASTBOOT_TIMEOUT_SETTING


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
	imx_request_iomux(I_pt_gpio->PIN, I_pt_gpio->PIN_CFG);
	imx_iomux_set_pad(I_pt_gpio->PIN, I_pt_gpio->PIN_PAD_CFG);
#else //][ !_MX50_
	imx_iomux_v3_setup_pad(I_pt_gpio->tIOMUXv3_PAD_CFG);
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

#ifndef _MX6ULL_ //[
	case 6:
		dwGPIO_dir_addr = GPIO6_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		break;
	case 7:
		dwGPIO_dir_addr = GPIO7_BASE_ADDR + 0x4;
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		break;
#endif //] _MX6ULL_

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
			I_pt_gpio->iCurrentVal = 1;
			iIsSetVal = 1;
		}
		else if(0==I_pt_gpio->iDefaultValue){
			reg = readl(dwGPIO_data_addr);
			reg &= ~((u32)(1 << I_pt_gpio->GPIO_Num));
			I_pt_gpio->iCurrentVal = 0;
			iIsSetVal = 1;
		}
		else {
			I_pt_gpio->iCurrentVal = -1;
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

int ntx_gpio_set_valueEx(NTX_GPIO *I_pt_gpio,int iOutVal,int iSetDir) 
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
#ifndef _MX6ULL_ //[
	case 6:
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO6_BASE_ADDR + 0x4;
		break;
	case 7:
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO7_BASE_ADDR + 0x4;
		break;
#endif //] _MX6ULL_
	default :
		printf("%s():%s [ERROR] GPIO group number error (%hd)!!\n",
			__FUNCTION__,I_pt_gpio->pszName,
			I_pt_gpio->GPIO_Grp);
		return -2;
	}

	regDir = readl(dwGPIO_dir_addr);
	if(-1==iSetDir) {
		// do not set direction of gpio 
		if(0!=I_pt_gpio->iDirection) {
			printf("%s(%d):gpio%d-%d,cannot set value (the direction must be output) !\n",
				__FUNCTION__,__LINE__,I_pt_gpio->GPIO_Grp,I_pt_gpio->GPIO_Num);
		}

		if(!(regDir&(u32)(1 << I_pt_gpio->GPIO_Num))) {
			// direction is input 
			printf("%s():[WARNING] \"%s\" set output value on input GPIO pin \n",
				__FUNCTION__,I_pt_gpio->pszName);
		}
	}
	else {
		if(1==iSetDir || 2==iSetDir) {
			// input .
			I_pt_gpio->iDirection = iSetDir;
			regDir &= ~(1 << I_pt_gpio->GPIO_Num);
		}
		else if(0==iSetDir) {
			// output .
			I_pt_gpio->iDirection = 0;
			regDir |= (u32)(1 << I_pt_gpio->GPIO_Num);
		}
		writel(regDir, dwGPIO_dir_addr);
	}

	reg = readl(dwGPIO_data_addr);
	if(1==iOutVal) {
		reg |= (u32)(1 << I_pt_gpio->GPIO_Num);
		I_pt_gpio->iCurrentVal = 1;
	}
	else if(0==iOutVal){
		reg &= ~((u32)(1 << I_pt_gpio->GPIO_Num));
		I_pt_gpio->iCurrentVal = 0;
	}
	writel(reg, dwGPIO_data_addr);
	
	
	return iRet;
}

int ntx_gpio_set_value(NTX_GPIO *I_pt_gpio,int iOutVal) 
{
	return ntx_gpio_set_valueEx(I_pt_gpio,iOutVal,-1);
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
#ifndef _MX6ULL_ //[
	case 6:
		dwGPIO_data_addr = GPIO6_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO6_BASE_ADDR + 0x4;
		break;
	case 7:
		dwGPIO_data_addr = GPIO7_BASE_ADDR + 0x0;
		dwGPIO_dir_addr = GPIO7_BASE_ADDR + 0x4;
		break;
#endif //] _MX6ULL_
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


int ntx_gpio_get_current_value(NTX_GPIO *I_pt_gpio) 
{
	if(!I_pt_gpio) {
		printf("%s(%d) : error parameter ! null ptr !\n",__FUNCTION__,__LINE__);
		return -2;
	}
	
	if(0==I_pt_gpio->iIsInited) {
		printf("%s(%d) : error parameter ! \"%s\" uninit !\n",
				__FUNCTION__,__LINE__,I_pt_gpio->pszName);
		return -3;
	}
	
	return I_pt_gpio->iCurrentVal;
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




static int upg_key_chk_cus9(void)
{
	int iPgUp,iPgDn;

	iPgDn = ntx_gpio_key_is_pgdn_down();
	iPgUp = ntx_gpio_key_is_pgup_down();
	//printf("PgUp=%d,PgDn=%d\n",iPgDn,iPgUp);
	return ( iPgUp|| iPgDn) ;
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
	int iRet = 0;
	char cCmdA[128+1];
#ifdef CONFIG_SYS_BOOT_SPINOR
	int offset= I_dwBinSectorNum*512;
	int	len = I_dwBinReadSectors*512;
	static int is_sf_initialized;
	if (!is_sf_initialized) {
		is_sf_initialized = 1;
		run_command("sf probe", 0);//
	}
	sprintf(cCmdA,"sf read 0x%x 0x%x 0x%x",(unsigned)O_pbBuf,
			(unsigned)offset,(unsigned)len);
	run_command(cCmdA, 0);//
#else
#ifdef MMC_CMD_SEPERATED_SDNUM//[

	static int giCurSDDevNum=-1;

	if(giCurSDDevNum!=I_iSDDevNum) {
		sprintf(cCmdA,"mmc dev %d",I_iSDDevNum);
		run_command(cCmdA,0);
		giCurSDDevNum = I_iSDDevNum;
	}

	//printf("%s(): read to %p\n",__FUNCTION__,O_pbBuf);

	sprintf(cCmdA,"mmc read 0x%x 0x%x 0x%x",(unsigned)O_pbBuf,
			(unsigned)(I_dwBinSectorNum),(unsigned)I_dwBinReadSectors);
	printf("%s\n",cCmdA);
	run_command(cCmdA, 0);//

#else //][!MMC_CMD_SEPERATED_SDNUM	

	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x%x",I_iSDDevNum,(unsigned)O_pbBuf,
			(unsigned)(I_dwBinSectorNum),(unsigned)I_dwBinReadSectors);
	run_command(cCmdA, 0);//

#endif //]MMC_CMD_SEPERATED_SDNUM
#endif
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
		printf("%s() : buffer size not enough (buffer size %d should >= %d)!\n",
				__FUNCTION__,(int)I_dwBufSize,(int)(I_dwBinSectorsToLoad*512));
		return ;
	}
	
	_read_mmc(I_iSDDevNum,O_pbBuf,I_dwBinSectorNum,I_dwBinSectorsToLoad);
		 
}

const static char gszKParamName_HwCfg[]="hwcfg";
const static char gszKParamName_logo[]="logo";
const static char gszKParamName_logo2[]="logo";
const static char gszKParamName_waveform[]="waveform";
const static char gszKParamName_ntxfw[]="ntxfw";

volatile unsigned char *gpbRAM_TopAddr;
volatile unsigned long gdwRAM_TotalSize,gdwRAM_ReservedSize;

typedef struct tagNtxHiddenMem {
	// private :
	const char *pszName; // binary data name .
	const unsigned long dwLoadSectNo;// the sector numbers loaded from emmc .
	// public :
	unsigned long dwLoadSects; // total sectors loaded from emmc .

	unsigned char *pbMemStart; // binary data ptr in ram for program using .
	unsigned long dwMemSize; // binary data size in ram for program using .
	unsigned long dwIdx; // loaded index .
	int iIsEnabled; // active flag .
} NtxHiddenMem;

static unsigned long gdwNtxHiddenMemIdx;
static NtxHiddenMem *gptNtxHiddenMemInfoA[4];

static NtxHiddenMem gtNtxHiddenMem_HwCfg = {
	.pszName = gszKParamName_HwCfg ,
	.dwLoadSectNo = SD_OFFSET_SECS_HWCFG,
};
static NtxHiddenMem gtNtxHiddenMem_waveform = {
	.pszName = gszKParamName_waveform ,
	.dwLoadSectNo = SD_OFFSET_SECS_WAVEFORM,
};
static NtxHiddenMem gtNtxHiddenMem_logo = {
	.pszName = gszKParamName_logo ,
	.dwLoadSectNo = SD_OFFSET_SECS_LOGO,
};
static NtxHiddenMem gtNtxHiddenMem_logo2 = {
	.pszName = gszKParamName_logo2 ,
	.dwLoadSectNo = SD_OFFSET_SECS_LOGO2,
};
static NtxHiddenMem gtNtxHiddenMem_ntxfw = {
	.pszName = gszKParamName_ntxfw ,
	.dwLoadSectNo = SD_OFFSET_SECS_NTXFW,
};

void NtxHiddenMem_append_kcmdline(char *I_pcCmdlineBufA,unsigned long I_ulCmdlineBufSize,char *O_pcCmdlineBufA)
{
	unsigned long dwTemp,dwTemp2;
	char *pcTemp,*pcTemp2;

	char cAppendStr[128];
	int i;
	int iTotalItems;

	iTotalItems = sizeof(gptNtxHiddenMemInfoA)/sizeof(gptNtxHiddenMemInfoA[0]);

	//printf("%s(),TotalItems=%d cmdline in=\"%s\" , out=\"%s\",outsize=%d\n",
	//		__FUNCTION__,iTotalItems,I_pcCmdlineBufA,O_pcCmdlineBufA,(int)I_ulCmdlineBufSize);

	if(O_pcCmdlineBufA) 
	{
		for (i=0;i<iTotalItems;i++)
		{
			if(!gptNtxHiddenMemInfoA[i]) {
				break;
			}

			//printf("[%d]append kcmdline for \"%s\" \n",i,gptNtxHiddenMemInfoA[i]->pszName);

			if(gptNtxHiddenMemInfoA[i]->iIsEnabled) {
				sprintf(cAppendStr," %s_p=0x%x %s_sz=%d",
					gptNtxHiddenMemInfoA[i]->pszName,
					(unsigned)(gptNtxHiddenMemInfoA[i]->pbMemStart),
					gptNtxHiddenMemInfoA[i]->pszName,
					(int)gptNtxHiddenMemInfoA[i]->dwMemSize);

				//printf("%s\n",cAppendStr);

				if(strlen(cAppendStr)+strlen(O_pcCmdlineBufA)<I_ulCmdlineBufSize) 
				{
					strcat(O_pcCmdlineBufA,cAppendStr);
					//printf("out=\"%s\"\n",O_pcCmdlineBufA);
				}
				else {
					printf("%s(%d):cmdline buffer not enought !!\n",__FILE__,__LINE__);
				}
			}
			else {
				printf("%s mem disabled or not avalible !!\n",gptNtxHiddenMemInfoA[i]->pszName);
				sprintf(cAppendStr," %s=bypass",
					gptNtxHiddenMemInfoA[i]->pszName);


				if(strlen(cAppendStr)+strlen(O_pcCmdlineBufA)<I_ulCmdlineBufSize) 
				{
					strcat(O_pcCmdlineBufA,cAppendStr);
					//printf("out=\"%s\"\n",O_pcCmdlineBufA);
				}
				else {
					printf("%s(%d):cmdline buffer not enought !!\n",__FILE__,__LINE__);
				}
			}
		}
	}
	else {
		printf("cmdline buffer not avalible !!\n");
	}

	//printf("%s(%d)\n",__FUNCTION__,__LINE__);

#if !defined(_MX6Q_) //[
	pcTemp = strstr(I_pcCmdlineBufA,"fbmem=");
	if(pcTemp) {
		pcTemp2 = strstr(pcTemp,"M");
		*pcTemp2 = 0;

		dwTemp2 = simple_strtoul(&pcTemp[6], NULL, 10);
		*pcTemp2 = 'M';
		gdwRAM_ReservedSize += (unsigned long)(dwTemp2<<20);
		printf("fbmem=%dM ,reserved size=%u\n",(int)dwTemp2,(unsigned)gdwRAM_ReservedSize);
	}
#endif //](_MX6Q_)

	//printf("%s(%d)\n",__FUNCTION__,__LINE__);
	if(49==gptNtxHwCfg->m_val.bPCB) {	// only register ram console for E60QDx
		printf("Reserved 1M for ram console\n");
		gdwRAM_ReservedSize += 0x100000;	//reserve 1M for ram_console
		dwTemp = (unsigned long)(gdwRAM_TotalSize-gdwRAM_ReservedSize)+0x80000000;
		sprintf(cAppendStr," ram_console_p=0x%x",(int)dwTemp);
		strcat(O_pcCmdlineBufA,cAppendStr);
	}
	
	if(0!=gdwRAM_ReservedSize) 
	{

		dwTemp = (unsigned long)((gdwRAM_TotalSize-gdwRAM_ReservedSize)>>20);
		dwTemp2 = dwTemp&0xffffffff;


		printf("Kernel RAM visiable size=%dM->%dM\n",(int)dwTemp,(int)dwTemp2);

		sprintf(cAppendStr," mem=%dM",(int)dwTemp2);
		if(strlen(cAppendStr)+strlen(O_pcCmdlineBufA)<I_ulCmdlineBufSize) {
			strcat(O_pcCmdlineBufA,cAppendStr);
		}
		else {
			printf("%s(%d):cmdline buffer not enought !!\n",__FILE__,__LINE__);
		}
	}
	//printf("%s() exit\n",__FUNCTION__);
	
}

static unsigned char * NtxHiddenMem_get_topaddr(void)
{
	if(0==gpbRAM_TopAddr) {
		gdwRAM_TotalSize = _get_ramsize((unsigned char **)&gpbRAM_TopAddr);
		printf("ram p=%p,size=%u\n",gpbRAM_TopAddr,(unsigned)gdwRAM_TotalSize);
		gpbRAM_TopAddr += gdwRAM_TotalSize;
		gdwRAM_ReservedSize = 0;
		gdwNtxHiddenMemIdx = 0;
	}
	return (unsigned char *)gpbRAM_TopAddr;
}

static unsigned char * NtxHiddenMem_load_ntxbin(NtxHiddenMem *IO_ptNtxHiddenMem,unsigned long *O_pdwBinSize)
{
	int iLoadDeviceNum ;
	unsigned long dwChk;
	unsigned long dwBinSectsToLoad;
	unsigned long dwBinBytesToLoad;
	unsigned char *pbRetAddr=0;
	//char cAppendStr[128];


	if(IO_ptNtxHiddenMem->pbMemStart && IO_ptNtxHiddenMem->dwMemSize) {
		printf("\"%s\" loaded already !!\n",IO_ptNtxHiddenMem->pszName);
		if(O_pdwBinSize) {
			*O_pdwBinSize = IO_ptNtxHiddenMem->dwMemSize;
		}
		return IO_ptNtxHiddenMem->pbMemStart;
	}

	//cAppendStr[0] = '\0';

	NtxHiddenMem_get_topaddr();

	iLoadDeviceNum = GET_ISD_NUM();
	
	do {
		dwChk = _load_ntx_bin_header(iLoadDeviceNum,\
				IO_ptNtxHiddenMem->dwLoadSectNo,(unsigned char *)gpbRAM_TopAddr-512,512);

		if(dwChk>0) {

			if(dwChk>=(gdwRAM_TotalSize-gdwRAM_ReservedSize)) {
				printf("ERROR : bin size (%d) bigger than RAM size(%d-%d) !!!\n",
						(int)dwChk,(int)gdwRAM_TotalSize,(int)gdwRAM_ReservedSize);
				break;
			}

			if(gdwNtxHiddenMemIdx!=0) {
				if(IO_ptNtxHiddenMem->dwLoadSectNo<=gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx-1]->dwLoadSectNo) {
					printf("[WARNING] Binaries load sequence should Lo->Hi !\n");
				}
				else 
				if( (gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx-1]->dwLoadSectNo+\
						gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx-1]->dwLoadSects) >= 
						IO_ptNtxHiddenMem->dwLoadSectNo ) 
				{
					printf("skip load \"%s\" because it will overwrite \"%s\" \n",
							IO_ptNtxHiddenMem->pszName,gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx-1]->pszName);
					break ;
				}
			}

			//dwBinSectsToLoad = (dwChk&0x1ff)?(dwChk>>9)+1:dwChk>>9;
			dwBinSectsToLoad = (dwChk>>9)+1;
			dwBinBytesToLoad = dwBinSectsToLoad<<9;
			gpbRAM_TopAddr -= dwBinBytesToLoad;
			_load_ntx_bin(iLoadDeviceNum,IO_ptNtxHiddenMem->dwLoadSectNo,
					dwBinSectsToLoad,(unsigned char *)gpbRAM_TopAddr,dwBinBytesToLoad);

			pbRetAddr = (unsigned char *)gpbRAM_TopAddr;

			gdwRAM_ReservedSize += dwBinBytesToLoad;

			
			if(gdwNtxHiddenMemIdx<sizeof(gptNtxHiddenMemInfoA)/sizeof(gptNtxHiddenMemInfoA[0])) {
				gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx] = IO_ptNtxHiddenMem;

				gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx]->pbMemStart = (unsigned char *)gpbRAM_TopAddr;
				gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx]->dwMemSize = dwChk;
				gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx]->dwIdx = gdwNtxHiddenMemIdx;
				gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx]->iIsEnabled = 1;

				gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx]->dwLoadSects = dwBinSectsToLoad;

#if 0 //[ debug informations .
				printf("[%d]%s added\n",gdwNtxHiddenMemIdx,gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx]->pszName);
				printf(" mem start=%p\n",gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx]->pbMemStart);
				printf(" mem size=%d\n",gptNtxHiddenMemInfoA[gdwNtxHiddenMemIdx]->dwMemSize);
#endif //] debug informations .

				gdwNtxHiddenMemIdx++;
			}
			else {
				printf("Hidden memory out of range (must < %d)\n",
						sizeof(gptNtxHiddenMemInfoA)/sizeof(gptNtxHiddenMemInfoA[0]));
			}


			if(O_pdwBinSize) {
				*O_pdwBinSize = dwChk;
			}

		}
		else {
			printf("\"%s\" not exist !!\n",IO_ptNtxHiddenMem->pszName);
		}
	} while(0);

	return pbRetAddr;
	
}

unsigned long ntx_get_file_size_in_ext4(int dev,int part,const char *filename)
{
	unsigned long dwRet=0;
	char cCmdA[384];
	char *s ;
	//unsigned long filelen;


	sprintf(cCmdA,"ext4size mmc %d:%d %s",dev,part,filename);
	
	printf("run cmd : \"%s\"\n",cCmdA);
	run_command(cCmdA,0);

	if ((s = getenv("filesize")) != NULL) {
		dwRet = simple_strtoul(s, NULL, 16);
		printf("filelen=%d\n",(int)dwRet);
	}

	return dwRet;
}

int ntx_read_file_in_ext4(int dev,int part,const char *filename,unsigned long *O_filelen,void *pvLoadAddr,unsigned long dwLoadMaxSize)
{
	int iRet=0;
	char cCmdA[384];
	char *s ;
	//unsigned long filelen;

	if((unsigned long)(-1)==dwLoadMaxSize) {
		sprintf(cCmdA,"ext4load mmc %d:%d %p %s",dev,part,pvLoadAddr,filename);
	}
	else {
		sprintf(cCmdA,"ext4load mmc %d:%d %p %s 0x%x",dev,part,pvLoadAddr,filename,(unsigned int)dwLoadMaxSize);
	}
	
	printf("run cmd : \"%s\"\n",cCmdA);
	run_command(cCmdA,0);
        
	if(O_filelen) {
		if ((s = getenv("filesize")) != NULL) {
			*O_filelen = simple_strtoul(s, NULL, 16);
                        printf("filelen=%d\n",(int)*O_filelen);
		}
                else
                {
                        loff_t file_len;
                        if(ext4fs_open(filename,&file_len)>0)
                        {
                                printf("==== Can't read file from ext4load() ====\n");
                                int ret=0 , fatfilelen = 0 ,  dev;
                                unsigned long addr = 0 ;
                                block_dev_desc_t *ext4_dev_desc ;
                                addr = pvLoadAddr ;
                                dev = gi_mmc_num_kernel;
                                ext4_dev_desc = get_dev("mmc",dev);

                                if (ext4_dev_desc==NULL) {
                                        printf ("** Block device mmc %d not supported\n",dev);
                                        iRet = 0;
                                }
                                else{
                                        printf("[DBG]------ Try to read uinitrmafs ------\n");
                                        fat_register_device(ext4_dev_desc,1);
                                        file_fat_detectfs();
                                        ret = file_fat_ls("/");
                                        printf("[DBG]------ file_fat_ls ret:%d ------\n",ret);
                                        fatfilelen = file_fat_read("/U2-uinitramfs",addr,10*1024*1024); // 10*1024*1024 , MAX_SIZE
                                        printf("[DBG] Read fat file :%s , ext4fs_read(%p,%d)\n","/U2-uinitramfs",addr,fatfilelen);				
                                        if(fatfilelen>0)
                                        {
                                                printf("[DBG] Set filelen:%d \n",fatfilelen);
                                                *O_filelen = fatfilelen ;
                                        }
                                        else{
                                                printf("** Unable to read \"%s\" from %d:%d **\n",
                                                        filename, dev, part);
                                                ext4fs_close();
                                                //deinit_fs(ext4_dev_desc);
                                                iRet = 0;
                                        }
                                }
                        }
                }
                
	}

	return iRet;
}


void _load_boot_waveform(void)
{
#ifdef CONFIG_SPLASH_SCREEN//[
	unsigned long dwChk;
	unsigned long dwBinSectsToLoad ;
	unsigned long dwBinBytesToLoad ;
	int iLoadDeviceNum;


	if(gpbWaveform) {
		return ;
	}

#ifdef ADVANCE_WAVEFORM_FILE//[
	gpbWaveform = NtxHiddenMem_load_ntxbin(&gtNtxHiddenMem_waveform,&gdwWaveformSize);
#else //][!ADVANCE_WAVEFORM_FILE
	iLoadDeviceNum = GET_ISD_NUM();

	dwChk = _load_ntx_bin_header(iLoadDeviceNum,SD_OFFSET_SECS_BOOTWAVEFORM,
			gbSectorBufA,sizeof(gbSectorBufA));
	if(dwChk>0) {
		dwBinSectsToLoad = (dwChk>>9)+1;
		dwBinBytesToLoad = dwBinSectsToLoad<<9;

		gpbWaveform = CONFIG_WAVEFORM_BUF_ADDR;
		_load_ntx_bin(iLoadDeviceNum,SD_OFFSET_SECS_BOOTWAVEFORM,
				dwBinSectsToLoad,gpbWaveform,
				CONFIG_FB_BASE-CONFIG_WAVEFORM_BUF_ADDR);

		gdwWaveformSize = dwChk;
	}
#endif//]ADVANCE_WAVEFORM_FILE

#endif //] CONFIG_SPLASH_SCREEN
}

void _load_ntx_sn(void)
{
	unsigned char *pbSectTempBuf = NtxHiddenMem_get_topaddr()-512;

	_load_ntx_bin(GET_ISD_NUM(),SD_OFFSET_SECS_SNMAC,1,pbSectTempBuf,512);
	memcpy(gcNTX_SN,pbSectTempBuf,512);

#ifdef CONFIG_MFG //[
	if( !( 'S'==gcNTX_SN[0] && 'N'==gcNTX_SN[1] && '-'==gcNTX_SN[2] ) )
	{
		// serail no from MFGTool .
		int i=0;

		gcNTX_SN[i++]='S';
		gcNTX_SN[i++]='N';
		gcNTX_SN[i++]='-';

#if 0 //[
		{
			int j;
			unsigned long long u64_cur_tick = get_ticks();
			char cTemp;

			printf("curtick=%016x\n",u64_cur_tick);
			gcNTX_SN[i++]='M';
			gcNTX_SN[i++]='F';
			gcNTX_SN[i++]='G';
			for(j=0;j<16;j++,i++) {
				cTemp = (char)((u64_cur_tick>>(j*4))&0xf);
				if( cTemp>=0x0 && cTemp<=9) {
					gcNTX_SN[i]=cTemp+'0';
				}
				else {
					gcNTX_SN[i]=cTemp+'a';
				}
			}
		}
#else //][
		{
			char *pc;
			int j;

			for(pc=NTX_SN_PRELOAD_ADDR,j=0;i<511;i++,j++)
			{
				if(pc[j]=='\0'||pc[j]=='\x0d'||pc[j]=='\x0a') {
					break;
				}
				gcNTX_SN[i]=pc[j];
			}
		}
#endif //]

		gcNTX_SN[i]='\0';

	}

#endif //] CONFIG_MFG
	gpszNTX_SN = &gcNTX_SN[3];
	if('S'==gpszNTX_SN[0] && 'N'==gpszNTX_SN[1] && '-'==gpszNTX_SN[2] ) {
		printf("NTXSN:\"%s\"\n",gpszNTX_SN);
	}
	else {
		printf("NTXSN not avalible !\n");
	}
}

void _load_isd_hwconfig(void)
{
	//NTX_HWCONFIG *ptNtxHwCfg;

#ifdef CONFIG_MFG
	gptNtxHwCfg = NTX_HWCFG_PRELOAD_ADDR;
	gdwNtxHwCfgSize = 110;
#ifdef CONFIG_MFG_FASTBOOT //[
	giNtxHwCfgSrc = NTX_HWCFG_SRC_RAM;
#endif //]CONFIG_MFG_FASTBOOT
	return;
#endif
	
	if(gptNtxHwCfg) {
		return ;
	}

#if 0//[
	ptNtxHwCfg = NTX_HWCFG_PRELOAD_ADDR;
	if(gszNtxHwCfgMagic[0]==ptNtxHwCfg->m_hdr.cMagicNameA[0] &&\
		gszNtxHwCfgMagic[1]==ptNtxHwCfg->m_hdr.cMagicNameA[1] &&\
		gszNtxHwCfgMagic[2]==ptNtxHwCfg->m_hdr.cMagicNameA[2] &&\
		gszNtxHwCfgMagic[3]==ptNtxHwCfg->m_hdr.cMagicNameA[3] &&\
		gszNtxHwCfgMagic[4]==ptNtxHwCfg->m_hdr.cMagicNameA[4] &&\
		gszNtxHwCfgMagic[5]==ptNtxHwCfg->m_hdr.cMagicNameA[5] &&\
		gszNtxHwCfgMagic[6]==ptNtxHwCfg->m_hdr.cMagicNameA[6] &&\
		gszNtxHwCfgMagic[7]==ptNtxHwCfg->m_hdr.cMagicNameA[7] &&\
		gszNtxHwCfgMagic[8]==ptNtxHwCfg->m_hdr.cMagicNameA[8] &&\
		gszNtxHwCfgMagic[9]==ptNtxHwCfg->m_hdr.cMagicNameA[9] )
	{
		gptNtxHwCfg = ptNtxHwCfg;
		gdwNtxHwCfgSize = 110;
		giNtxHwCfgSrc = NTX_HWCFG_SRC_RAM;
		printf("ISD hwconfig loaded from RAM !\n");
	}
	else 
#endif //]
	{
		gptNtxHwCfg = (NTX_HWCONFIG *)NtxHiddenMem_load_ntxbin(&gtNtxHiddenMem_HwCfg,&gdwNtxHwCfgSize);
	}
}

void _load_esd_hwconfig(void)
{
	unsigned long dwChk;
	int iSD_IDX;	
	unsigned char *pbSectTempBuf = NtxHiddenMem_get_topaddr()-512;



	if(gptEsdNtxHwCfg) {
		return ;
	}

	iSD_IDX = GET_ESD_NUM();
	dwChk = _load_ntx_bin_header(iSD_IDX,SD_OFFSET_SECS_HWCFG,pbSectTempBuf,512);
	if(dwChk>0) {
		_load_ntx_bin(iSD_IDX,SD_OFFSET_SECS_HWCFG,1,pbSectTempBuf,512);
		memcpy((unsigned char *)&gtEsdNtxHwCfg,pbSectTempBuf,sizeof(gtEsdNtxHwCfg));
		gptEsdNtxHwCfg = &gtEsdNtxHwCfg;
		gdwEsdNtxHwCfgSize = dwChk;
	}
	else {
		printf("ESD hwconfig missing !\n");
	}
}



volatile unsigned char *gpbKernelAddr=0; // kernel address . 
volatile unsigned long gdwKernelSize=0; // kernel size in byte .
volatile unsigned char *gpbRamfsAddr=0; // Ramfs address . 
volatile unsigned long gdwRamfsSize=0; // Ramfs size in byte .

static int _load_ntxRamfs(unsigned char **O_ppbRamfsAddr,unsigned long *O_pdwRamfsSize)
{
#ifdef CONFIG_MFG //[
	return 0;
#else //][!CONFIG_MFG
	//char cCmdA[128];
	volatile unsigned char *pbMagic ;
	unsigned long dwReadSectors;
	unsigned long dwImgSize=0 ;
	char *s;
	ulong offset = 0;

	if (!gpbKernelAddr) {
		printf ("Error: please load kernel image before loading ramfs.\n");
		return -1;
	}

	/* pre-set offset from CONFIG_SYS_LOAD_ADDR */
//	offset = gpbKernelAddr+gdwKernelSize;
	if ((s = getenv("initrd_addr")) != NULL) {
		offset = simple_strtoul(s, NULL, 16);
	}
	printf ("load ramfs to 0x%08X\n", offset);
	if(0==offset) {
		// load to unkown address .
		return -1;
	}

	pbMagic = (unsigned char *)(offset+(512-16)) ;


	if (gi_mmc_num_kernel<0) {
		gi_mmc_num_kernel = GET_ISD_NUM() ;
	}
	
#ifdef AUTO_DETECT_KIMGSIZE//[

	_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
			(unsigned long)(SD_OFFSET_SECS_RAMFS-1),1);

	if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
		&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
	{

		dwImgSize = *((unsigned long *)(pbMagic+8));
		printf("Ramfs size = %lu@%p\n",dwImgSize,(unsigned char *)offset);
		dwReadSectors = dwImgSize>>9|1;
		dwReadSectors += 6;
		//dwReadSectors = 0xfff;
		
		_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
			(unsigned long)(SD_OFFSET_SECS_RAMFS),dwReadSectors);
	}
	else 
#endif //] AUTO_DETECT_KIMGSIZE
	{

		printf("Error: no Ramfs image signature !\n");
		return -1;	
	}

	if(O_ppbRamfsAddr) {
		*O_ppbRamfsAddr = (unsigned char *)offset;
	}
	if(O_pdwRamfsSize) {
		*O_pdwRamfsSize = dwImgSize;
	}

	return 0;
#endif //]CONFIG_MFG
}
static int do_load_ntxRamfs(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	iRet =  _load_ntxRamfs((unsigned char **)&gpbRamfsAddr,(unsigned long *)&gdwRamfsSize)>=0?
		CMD_RET_SUCCESS:CMD_RET_FAILURE;
	return iRet;
}
U_BOOT_CMD(load_ntxRamfs, 2, 0, do_load_ntxRamfs,
	"load_ramfs - netronix ramfs image load \n",
	"load_ramfs "
		" - load netronix ramfs from sd card .\n"
);


static int _load_ntxkernel(unsigned char **O_ppbKernelAddr,unsigned long *O_pdwKernelSize)
{
#ifdef CONFIG_MFG //[
	return 0;
#else //][!CONFIG_MFG
	//char cCmdA[128];
	volatile unsigned char *pbMagic ;
	unsigned long dwReadSectors;
	unsigned long dwImgSize=0 ;
	char *s;
	ulong offset = 0;

	/* pre-set offset from CONFIG_SYS_LOAD_ADDR */
	offset = CONFIG_SYS_LOAD_ADDR;

	/* pre-set offset from $loadaddr */
	if ((s = getenv("loadaddr")) != NULL) {
		offset = simple_strtoul(s, NULL, 16);
	}

	if(0==offset) {
		// load to unkown address .
		return -1;
	}

	//pbMagic = gpbSectorBuffer+(512-16) ;
	pbMagic = (unsigned char *)(offset+(512-16)) ;


	if (gi_mmc_num_kernel<0) {
		gi_mmc_num_kernel = GET_ISD_NUM() ;
	}


	if(9==gptNtxHwCfg->m_val.bCustomer)	{
		dwImgSize = DEFAULT_LOAD_KERNEL_SZ;
		_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
			SD_OFFSET_SECS_KERNEL,DEFAULT_LOAD_KERNEL_SZ);
	}
	else {
#ifdef AUTO_DETECT_KIMGSIZE//[

		_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
			(unsigned long)(SD_OFFSET_SECS_KERNEL-1),1);

		if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
			&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
		{

			dwImgSize = *((unsigned long *)(pbMagic+8));
			printf("kernel size = %lu@%p\n",dwImgSize,(unsigned char *)offset);
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
	}

	if(O_ppbKernelAddr) {
		*O_ppbKernelAddr = (unsigned char *)offset;
	}
	if(O_pdwKernelSize) {
		*O_pdwKernelSize = dwImgSize;
	}

	return 0;
#endif //]CONFIG_MFG
}
static int do_load_ntxkernel(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	iRet =  _load_ntxkernel((unsigned char **)&gpbKernelAddr,(unsigned long *)&gdwKernelSize)>=0?
		CMD_RET_SUCCESS:CMD_RET_FAILURE;
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
#ifdef CONFIG_MFG //[
	return 0;
#else //][!CONFIG_MFG
	int iRet = 0;
	int i;
	ulong offset = 0;
	char *s;
	unsigned char *L_pbRDAddr=0;
	unsigned long L_dwRDSize=0;

#ifdef CONFIG_RD_LOADADDR //[
	/* pre-set offset from CONFIG_RD_LOADADDR */
	offset = CONFIG_RD_LOADADDR;
#endif //]CONFIG_RD_LOADADDR

	/* pre-set offset from $rd_loadaddr */
	if ((s = getenv("rd_loadaddr")) != NULL) {
		offset = simple_strtoul(s, NULL, 16);
	}
	else {
		printf("no ramdisk env ,default rd->%p\n",(void *)offset);
	}

	if(0==offset) {
		// load to unkown address .
		printf("RD loadaddr not assigned !!\n");
		iRet = -1;goto loadrd_end;
	}

	if (gi_mmc_num_kernel<0) {
		gi_mmc_num_kernel = GET_ISD_NUM() ;
	}

#ifdef CONFIG_CMD_EXT4//[ load INITRD from EMMC "rootfs/boot/uinitramfs"
	if(0==L_pbRDAddr) {

		char *filename = NULL;
		int dev, part = 1;
		ulong addr = 0;
		int filelen;
		int fatfilelen = 0 ,ret=0;
		
		do {

			filename = "/boot/uinitramfs" ;
			addr = offset;

			// locate rootfs partition no. 
			switch(gptNtxHwCfg->m_val.bSysPartType) {
			case 0://TYPE1
			case 2://TYPE3
			case 4://TYPE5
			case 5://TYPE6
				part = 1;
				break;
			case 1://TYPE2
			case 3://TYPE4
			case 6://TYPE7
				part = 2;
				break;
			case 12://TYPE13
				part = 3;
				break;
			default:
				part = 0;
				break;
			}

			dev = gi_mmc_num_kernel;

			
			ntx_read_file_in_ext4(dev,part,filename,
					(unsigned long *)&filelen,(void *)addr,(unsigned long)(-1));

			/* Loading ok, update default load address */

			L_pbRDAddr = (unsigned char *)addr; 
			L_dwRDSize = (unsigned long)filelen;

			printf ("\"%s\" %d bytes read @ %p\n",filename, filelen,(void *)addr);

			if(filelen < 16 )
			{
				printf("## ramdisk is not exist , set rd_loadaddr -");
				setenv("rd_loadaddr","-");		
			}

		} while(0);
	}
#endif//]CONFIG_CMD_EXT4


#if 1//[ load INITRD from EMMC offset .

	if(0==L_pbRDAddr) 
	{
		//char cCmdA[128];
		volatile unsigned char *pbMagic ;
		unsigned long dwReadSectors;
		unsigned long dwImgSize ;

		unsigned long dwOffsetSecsINITRDA[2] = {
			SD_OFFSET_SECS_INITRD2,SD_OFFSET_SECS_INITRD,
		};

		unsigned long dwBENV_NTXRD_offset;


		if ((s = getenv("ntx_rd_offset")) != NULL) {
			dwBENV_NTXRD_offset = simple_strtoul(s, NULL, 16);
			dwOffsetSecsINITRDA[0]=dwBENV_NTXRD_offset;
			dwOffsetSecsINITRDA[1]=0xffffffff;
		}

		//pbMagic = gpbSectorBuffer+(512-16) ;
		pbMagic = (unsigned char *)(offset+(512-16)) ;
		
#ifdef AUTO_DETECT_KIMGSIZE//[

		for (i=0;i<sizeof(dwOffsetSecsINITRDA)/sizeof(dwOffsetSecsINITRDA[0]);i++) {

			if(0xffffffff==dwOffsetSecsINITRDA[i]) {
				break;
			}

			_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
					(unsigned long)(dwOffsetSecsINITRDA[i]-1),1);

			if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
				&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
			{

				dwImgSize = *((unsigned long *)(pbMagic+8));
				printf("rd size = %lu@%p\n",dwImgSize,(unsigned char *)offset);
				dwReadSectors = dwImgSize>>9|1;
				dwReadSectors += 6;
				//dwReadSectors = 0xfff;

				_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
						(unsigned long)dwOffsetSecsINITRDA[i],dwReadSectors);

				L_pbRDAddr = (unsigned char *)offset;
				L_dwRDSize = (unsigned long)dwImgSize;

				break;
			}
			else 
			{
				printf("no ramdisk image signature @sec no. %d ! \n",
						(int)(dwOffsetSecsINITRDA[i]-1));
			}

		} // initrd offset loop .

#else //][!

		{

			_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
					SD_OFFSET_SECS_INITRD,DEFAULT_LOAD_RD_SZ);
		
		}

#endif //] AUTO_DETECT_KIMGSIZE
	}// 
#endif //]


loadrd_end:
	if(O_ppbRDaddr) {
		*O_ppbRDaddr = L_pbRDAddr;
	}

	if(O_pdwRDsize) {
		*O_pdwRDsize = L_dwRDSize;
	}

	return iRet;
#endif //]CONFIG_MFG
}

static int do_load_ntxrd(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	iRet = _load_ntxrd(&gpbRDaddr,&gdwRDsize)>=0?
		CMD_RET_SUCCESS:CMD_RET_FAILURE;
	return iRet;
}
U_BOOT_CMD(load_ntxrd, 2, 0, do_load_ntxrd,
	"load_ntxrd - netronix ramdisk image load \n",
	"load_ntxrd "
		" - load netronix ramdisk from sd card .\n"
);



unsigned char *gpbDTBaddr=0; // DTB address . 
unsigned long gdwDTBsize=0; // DTB size in byte .

static int _load_ntxdtb(unsigned char **O_ppbDTBAddr,unsigned long *O_pdwDTBSize)
{
#ifdef CONFIG_MFG //[
	return 0;
#else //][!CONFIG_MFG

	//char cCmdA[128];
	volatile unsigned char *pbMagic ;
	unsigned long dwReadSectors;
	unsigned long dwImgSize=0 ;
	char *s;
	ulong offset = 0;

	/* pre-set offset from $loadaddr */
	if ((s = getenv("fdt_addr")) != NULL) {
		offset = simple_strtoul(s, NULL, 16);
	}

	if(0==offset) {
		// load to unkown address .
		return -1;
	}

	if (gi_mmc_num_kernel<0) {
		gi_mmc_num_kernel = GET_ISD_NUM() ;
	}
	
	//pbMagic = gpbSectorBuffer+(512-16) ;
	pbMagic = (unsigned char *)(offset+(512-16)) ;
#ifdef AUTO_DETECT_KIMGSIZE//[

	_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
			(unsigned long)(SD_OFFSET_SECS_DTB-1),1);

	if(gszNtxBinMagicA[0]==pbMagic[0]&&gszNtxBinMagicA[1]==pbMagic[1]\
		&&gszNtxBinMagicA[2]==pbMagic[2]&&gszNtxBinMagicA[3]==pbMagic[3]) 
	{

		dwImgSize = *((unsigned long *)(pbMagic+8));
		printf("dtb size = %lu@%p\n",dwImgSize,(unsigned char *)offset);
		dwReadSectors = dwImgSize>>9|1;
		dwReadSectors += 6;
		//dwReadSectors = 0xfff;
		
		_read_mmc(gi_mmc_num_kernel,(unsigned char *)offset,\
			(unsigned long)(SD_OFFSET_SECS_DTB),dwReadSectors);
	}
	else 
#endif //] AUTO_DETECT_KIMGSIZE
	{
#ifdef AUTO_DETECT_KIMGSIZE//[

		printf("no dtb signature !\n");
#endif //]AUTO_DETECT_KIMGSIZE
		return -2;
	}

	if(O_ppbDTBAddr) {
		*O_ppbDTBAddr = (unsigned char *)offset;
	}
	if(O_pdwDTBSize) {
		*O_pdwDTBSize = dwImgSize;
	}

	return 0;
#endif //]CONFIG_MFG
}

static int do_load_ntxdtb(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	iRet = _load_ntxdtb(&gpbDTBaddr,&gdwDTBsize)>=0?
		CMD_RET_SUCCESS:CMD_RET_FAILURE;
	return iRet;
}
U_BOOT_CMD(load_ntxdtb, 2, 0, do_load_ntxdtb,
	"load_ntxdtb - netronix dtb load \n",
	"load_ntxdtb "
		" - load netronix dtb from sd card .\n"
);


static int do_load_ntxwf(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	iRet = 	NtxHiddenMem_load_ntxbin(&gtNtxHiddenMem_waveform,0)?
		CMD_RET_SUCCESS:CMD_RET_FAILURE;
	return iRet;
}
U_BOOT_CMD(load_ntxwf, 2, 0, do_load_ntxwf,
	"load_ntxwf - netronix waveform load \n",
	"load_ntxwf "
		" - load netronix waveform from sd card .\n"
);



static void _cp_swcfg_esd_to_isd(void)
{
	if(gptNtxHwCfg&&gptEsdNtxHwCfg) {
		printf("copy soft configs from ESD to ISD ...\n");
		// if boot from esd we should pass the ESD software configs to kernel .
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
}

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
	int iPwr_Key=-1,iUPG_Key=-1,iESD_in=-1,iUSB_in=-1,iBootESD=0;
	int iMenuKey=-1;

	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}

	if(gptNtxHwCfg && NTX_HWCFG_SRC_RAM == giNtxHwCfgSrc) {
		//fastboot_connection_timeout_us_set(30*1000*1000);
		return NTX_BOOTMODE_FASTBOOT;
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
			_cp_swcfg_esd_to_isd();
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

		iUSB_in=ntx_detect_usb_plugin(0);
		iESD_in=ntxup_is_ext_card_inserted();
		iPwr_Key=_power_key_status();
		iMenuKey = ntxup_wait_key_downloadmode();

		if(1==iPwr_Key && 1==iESD_in) {
			_load_esd_hwconfig();
		}

		iBootESD=(gptEsdNtxHwCfg&&NTXHWCFG_TST_FLAG(gptEsdNtxHwCfg->m_val.bBootOpt,0))?1:0;

#ifdef USB_OTG_FASTBOOT_MODE//[
		//printf("## fastboot mode OTG USB test&debug ##\n");	iUSB_in|=USB_CHARGER_OTG;//test&debug only .
		if(iUSB_in&USB_CHARGER_OTG) {
			printf("\n**********************************\n\n");
			printf("\n** fastboot mode OTG USB        **\n\n");
			printf("\n**********************************\n\n");
			iRet = NTX_BOOTMODE_FASTBOOT;
			//fastboot_connection_timeout_us_set(5*1000*1000);
			fastboot_connection_timeout_us_set(0);
			fastboot_connection_abort_at_usb_remove_chk_setup(&ntx_is_fastboot_abort_inusbremove);
		}
		else
#endif //]USB_OTG_FASTBOOT_MODE
		if(2!=gptNtxHwCfg->m_val.bUIStyle) 
		{
			if (12==gptNtxHwCfg->m_val.bKeyPad) {
				int iChkRecovery = 0;

				iRet = NTX_BOOTMODE_ISD;
				// No Key in this model .
				//iESD_in=ntxup_is_ext_card_inserted();
				//iPwr_Key=_power_key_status();

				if( 1==iPwr_Key && 1==iUSB_in ) {

					printf("\n**********************************\n\n");
					printf("\n** 0. fastboot mode (NO Key Model) **\n\n");
					printf("\n**********************************\n\n");		
					iRet = NTX_BOOTMODE_FASTBOOT;
					fastboot_connection_timeout_us_set(5*1000*1000);
					fastboot_connection_abort_at_usb_remove_chk_setup(&ntx_is_fastboot_abort_inusbremove);
				}
				else {
					if( 1==iPwr_Key ) {
						iUPG_Key=ntxup_wait_touch_recovery();

						if(1==iESD_in) {

							if ( 1==iBootESD || (2==iUPG_Key) ) 
							{
								printf("\n**********************************\n\n");
								printf("\n* Boot from ESD (NO Key Model)  **\n\n");
								printf("\n**********************************\n\n");		
								iRet = NTX_BOOTMODE_ESD_UPG;
								_cp_swcfg_esd_to_isd();
							}
							else {
								iChkRecovery = 1;
							}
						}
						else {
							iChkRecovery = 1;
						}
					}

					if( iChkRecovery && (1==iPwr_Key) && (1==iUPG_Key) ) {
						printf("\n**********************************\n\n");
						printf("\n** Boot recovery (NO Key Model) **\n\n");
						printf("\n**********************************\n\n");		
						iRet = NTX_BOOTMODE_RECOVERY;
					}
					else if( (1==iPwr_Key) && (3==iUPG_Key) ) {
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
				if( (1==iMenuKey) && (1==iUSB_in) && (1==iPwr_Key)) {
					printf("\n**********************************\n\n");
					printf("\n** USB SDownload mode           **\n\n");
					printf("\n**********************************\n\n");
					iRet = NTX_BOOTMODE_SDOWNLOAD;
				}
				else
				if( (1==iUPG_Key) && (1==iUSB_in)) {
					iRet = NTX_BOOTMODE_FASTBOOT;
					printf("\n**********************************\n\n");
					if (1==iPwr_Key) {
						printf("\n** fastboot mode (cus)          **\n\n");
						fastboot_connection_timeout_us_set(1000*1000);
					}
					else {
						printf("\n** fastboot mode (normal)       **\n\n");
						fastboot_connection_timeout_us_set(30*1000*1000);
					}
					printf("\n**********************************\n\n");
				}
				else
				if (18==gptNtxHwCfg->m_val.bKeyPad && (1==iPwr_Key) && (1==iUSB_in)) {
					// HOMEPAD only type 
					printf("\n**********************************\n\n");
					printf("\n** fastboot mode (HOMEPAD) **\n\n");
					printf("\n**********************************\n\n");		
					iRet = NTX_BOOTMODE_FASTBOOT;
					fastboot_connection_timeout_us_set(5*1000*1000);
				}
				else 
				if((1==iESD_in) && (1==iPwr_Key) &&	((1==iBootESD) || (1==iUPG_Key)) ) 
				{
					printf("\n**************************\n\n");
					printf("\n** Boot from ESD        **\n\n");
					printf("\n**************************\n\n");		
					iRet = NTX_BOOTMODE_ESD_UPG;
					_cp_swcfg_esd_to_isd();
				}
				else 
				if(9==gptNtxHwCfg->m_val.bCustomer) {
					// . 
					if(0==ntx_wait_powerkey_ex(10,1,1,upg_key_chk_cus9)) {// if user pressing and holding power key over 10 secs will return 0 or break and return non zero . 
						printf("[0]enter recovery cus %d ...\n",gptNtxHwCfg->m_val.bCustomer);
						iRet=NTX_BOOTMODE_RECOVERY;
					}
					else {
						iRet=NTX_BOOTMODE_ISD;
					}
				}
				else {
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
		}
		else {
			iRet = NTX_BOOTMODE_ISD;
			//sprintf(cCmdA,"setenv bootcmd run bootcmd_mmc");
			//run_command(cCmdA,0);
			//_load_ntxrd(&gpbRDaddr,&gdwRDsize);
		}

#if 1//[ debug code .
		printf("ESDin=%d,UPGKey=%d,PWRKey=%d,USBin=0x%x,BootESD=%d,MenuKey=%d\n",
				iESD_in,iUPG_Key,iPwr_Key,iUSB_in,iBootESD,iMenuKey);
#endif //]

		break;
	}

	if( (NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bFrontLight_Flags,0)) && (iRet == NTX_BOOTMODE_ISD) )
	{
		frontLightCtrl();	
	}
	return iRet;
}

int giNTX_VFAT_partNO;
int giNTX_RootFS_partNO;
int giNTX_RecoveryFS_partNO;
int giNTX_BootImg_partNO;
int giNTX_RecoveryImg_partNO;
int giNTX_System_partNO;
int giNTX_Data_partNO;
int giNTX_Cache_partNO;
int giNTX_Vendor_partNO;
int giNTX_Misc_partNO;
int giNTX_Emergency_partNO;


int ntx_parse_syspart_type(void)
{
	int iRet = 0;

	switch(gptNtxHwCfg->m_val.bSysPartType) {
	case 0://TYPE1
		giNTX_RootFS_partNO=1;
		giNTX_VFAT_partNO=2;
		break;

	case 1://TYPE2
		giNTX_RootFS_partNO=2;
		giNTX_RecoveryFS_partNO=1;
		giNTX_VFAT_partNO=3;
		break;

	case 2://TYPE3
		giNTX_RootFS_partNO=1;
		giNTX_RecoveryFS_partNO=2;
		giNTX_VFAT_partNO=3;
		break;

	case 3://TYPE4
		giNTX_RootFS_partNO=2;
		giNTX_VFAT_partNO=1;
		break;

	case 4://TYPE5
		giNTX_RootFS_partNO=1;
		break;

	case 5://TYPE6
		giNTX_RootFS_partNO=1;
		giNTX_RecoveryFS_partNO=3;
		giNTX_VFAT_partNO=2;
		break;

	case 6://TYPE7
		giNTX_RootFS_partNO=2;
		giNTX_RecoveryFS_partNO=3;
		giNTX_VFAT_partNO=1;
		break;

	case 7://TYPE8
		giNTX_System_partNO=2;
		giNTX_VFAT_partNO=1;
		giNTX_RecoveryFS_partNO=4;
		giNTX_Data_partNO=5;
		giNTX_Cache_partNO=6;
		break;

	case 8://TYPE9
		giNTX_BootImg_partNO=1;
		giNTX_RecoveryImg_partNO=2;
		giNTX_System_partNO=5;
		giNTX_VFAT_partNO=4;
		giNTX_Data_partNO=7;
		giNTX_Cache_partNO=6;
		giNTX_Vendor_partNO=8;
		giNTX_Misc_partNO=9;
		break;

	case 9://TYPE10
		giNTX_System_partNO=2;
		giNTX_VFAT_partNO=1;
		giNTX_RecoveryFS_partNO=4;
		giNTX_Data_partNO=5;
		giNTX_Cache_partNO=6;
		giNTX_Emergency_partNO=7;
		break;

	case 10://TYPE11
		giNTX_BootImg_partNO=1;
		giNTX_RecoveryImg_partNO=2;
		giNTX_System_partNO=5;
		giNTX_Data_partNO=4;
		giNTX_Cache_partNO=6;
		giNTX_Vendor_partNO=7;
		giNTX_Misc_partNO=8;
		break;

	case 11://TYPE12
		giNTX_BootImg_partNO=1;
		giNTX_RecoveryImg_partNO=2;
		giNTX_System_partNO=5;
		giNTX_VFAT_partNO=4;
		giNTX_Data_partNO=7;
		giNTX_Cache_partNO=6;
		giNTX_Vendor_partNO=8;
		giNTX_Misc_partNO=9;
		giNTX_Emergency_partNO=10;
		break;

	case 12://TYPE13
		giNTX_RootFS_partNO=3;
		giNTX_RecoveryFS_partNO=2;
		giNTX_VFAT_partNO=1;
		break;

	case 13://TYPE14
		giNTX_BootImg_partNO=1;
		giNTX_RecoveryImg_partNO=2;
		giNTX_System_partNO=5;
		giNTX_Data_partNO=4;
		giNTX_Cache_partNO=6;
		giNTX_Vendor_partNO=7;
		giNTX_Misc_partNO=8;
		break;

	case 14://TYPE15
		giNTX_RootFS_partNO=2;
		giNTX_VFAT_partNO=3;
		break;

	default:
		iRet = -1;
		printf("%s():invalid partition type %d\n",__FUNCTION__,
				(int)gptNtxHwCfg->m_val.bSysPartType);
		break;
	}
	return iRet;
}

#if defined(CONFIG_FASTBOOT) || defined(CONFIG_FSL_FASTBOOT) //[

//#define SECTOR_SIZE				512

//unit: sector = 512B
#define MBR_OFFSET		0
#define MBR_SIZE			1
#define SN_OFFSET                            1
#define SN_SIZE                                  1

#define BOOTLOADER_OFFSET	2

#if defined(_MX6SL_) || defined(_MX7D_) || defined(_MX6ULL_)||defined(_MX6SLL_)//[
	#define BOOTLOADER_SIZE	1022
	#define HWCFG_OFFSET	1024
#elif defined(_MX6Q_) //][
	#define BOOTLOADER_SIZE	1522
	#define HWCFG_OFFSET	1524
#else //][
	#error "unkown platform !!!"
#endif //]_MX6SL_ || _MX7D_ || _MX6ULL_

#define HWCFG_SIZE		2
#define WAVEFORM_OFFSET			14336//7MB
#define WAVEFORM_SIZE				20480//10MB
#define LOGO_OFFSET					34816 //17MB
#define LOGO_SIZE						4096 //2MB
#define KERNEL_OFFSET 			2048
#define KERNEL_SIZE					12284
#define DTB_OFFSET				1286
#define DTB_SIZE					250
#define BOOTENV_OFFSET 			1536
#define BOOTENV_SIZE				510
#define NTXFW_OFFSET				1030
#define NTXFW_SIZE					255

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
		.name="ntxfw",
		.start=NTXFW_OFFSET,
		.length=NTXFW_SIZE,
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
		.name=0,
	}

};

// fastboot partitions ...
static struct fastboot_ptentry ntx_fb_partsA[NTX_FASTBOOT_MAX_PPTN]= {
};


static const char gszLinuxKernel_Name[]="kernel";
static const char gszLinuxKernelDTB_Name[]="dtb";
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
	//unsigned sector, partno = -1;

	int iPIdx;

	//printf("%s()\n",__FUNCTION__);
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

		strcpy(ntx_fb_binsA[i].name,gszLinuxKernelDTB_Name);
		ntx_fb_binsA[i].partition_id=0;
		ntx_fb_binsA[i].start=DTB_OFFSET;
		ntx_fb_binsA[i].length=DTB_SIZE;
		ntx_fb_binsA[i].flags=0;
		i++;

	}


	iPIdx=0;

	if(giNTX_RootFS_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszLinuxRootFS_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_RootFS_partNO;
		iPIdx++;
	}

	if(giNTX_VFAT_partNO>0) {
		if(2==gptNtxHwCfg->m_val.bUIStyle) {
			strcpy(ntx_fb_partsA[iPIdx].name,gszDroidMedia_Name);
		}
		else {
			strcpy(ntx_fb_partsA[iPIdx].name,gszLinuxVfatFS_Name);
		}
		ntx_fb_partsA[iPIdx].partition_id=giNTX_VFAT_partNO;
		iPIdx++;
	}
	
	if(giNTX_RecoveryFS_partNO>0) {
		if(2==gptNtxHwCfg->m_val.bUIStyle) {
			strcpy(ntx_fb_partsA[iPIdx].name,gszDroidRecovery_Name);
		}
		else {
			strcpy(ntx_fb_partsA[iPIdx].name,gszLinuxRecoveryFS_Name);
		}
		ntx_fb_partsA[iPIdx].partition_id=giNTX_RecoveryFS_partNO;
		iPIdx++;
	}

	if(giNTX_System_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszDroidSystem_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_System_partNO;
		iPIdx++;
	}

	if(giNTX_Data_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszDroidData_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_Data_partNO;
		iPIdx++;
	}

	if(giNTX_Cache_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszDroidCache_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_Cache_partNO;
		iPIdx++;
	}
	
	if(giNTX_BootImg_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszDroidBoot_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_BootImg_partNO;
		iPIdx++;
	}

	if(giNTX_RecoveryImg_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszDroidRecovery_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_RecoveryImg_partNO;
		iPIdx++;
	}

	if(giNTX_Vendor_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszDroidVendor_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_Vendor_partNO;
		iPIdx++;
	}

	if(giNTX_Misc_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszDroidMisc_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_Misc_partNO;
		iPIdx++;
	}

	if(giNTX_Emergency_partNO>0) {
		strcpy(ntx_fb_partsA[iPIdx].name,gszDroidEmergency_Name);
		ntx_fb_partsA[iPIdx].partition_id=giNTX_Emergency_partNO;
		iPIdx++;
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

int ntx_is_fastboot_abort_inusbremove(void) 
{
#if 0
	if(49==gptNtxHwCfg->m_val.bPCB && gptNtxHwCfg->m_val.bPCB_REV>=0x20)
	{
		return 1;
	}
	else if(2!=gptNtxHwCfg->m_val.bUIStyle && 12==gptNtxHwCfg->m_val.bKeyPad)
	{
		// linux platform & models without keys .
		if(_power_key_status()) {
			return 1;
		}
	}
#endif
	return 1;
}

#else //][! defined(CONFIG_FASTBOOT) || defined(CONFIG_FSL_FASTBOOT)

void ntx_config_fastboot_layout(void){}
int ntx_is_fastboot_abort_inusbremove(void) {return 0;}

#endif //]CONFIG_FASTBOOT

#ifdef _MX7D_ //[
static const NTX_GPIO gt_ntx_gpio_bat_low= {
	MX7D_PAD_GPIO1_IO00__GPIO1_IO0,  // pin pad/mux control .
	1, // gpio group .
	0, // gpio number .
	0, // NC .
	0, // not inited .
	"Bat_low", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

#elif defined(_MX6ULL_)//][
static const NTX_GPIO gt_ntx_gpio_bat_low= {
	MX6_PAD_SNVS_TAMPER3__GPIO5_IO03|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	1, // gpio group .
	0, // gpio number .
	0, // NC .
	0, // not inited .
	"Bat_low", // name .
	1, // 1:input ; 0:output ; 2:btn .
};


#elif defined(_MX6SL_) || defined(_MX6SLL_)//][

static const NTX_GPIO gt_ntx_gpio_bat_low= {
	MX6_PAD_KEY_COL2__GPIO3_IO28,  // pin pad/mux control .
	3, // gpio group .
	28, // gpio number .
	0, // NC .
	0, // not inited .
	"Bat_low", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
#endif//]_MX7D_

#define IS_BATT_CRITIAL_LOW()	\
		(0==ntx_gpio_get_value(&gt_ntx_gpio_bat_low))?1:0

extern int RC5T619_read_reg(unsigned char bRegAddr,unsigned char *O_pbRegVal);
extern int RC5T619_write_buffer(unsigned char bRegAddr,unsigned char *I_pbRegWrBuf,unsigned short I_wRegWrBufBytes);

int ricoh_read_adc (unsigned char adsel)
{
	int i, iChk;
	unsigned char bValue;

	/* ADC interrupt enable */
	RC5T619_write_reg (0x9D, 0x08);	// INTEN_REG

	/* enable interrupt request of single mode */
	RC5T619_write_reg (0x8A, 0x01);	// EN_ADCIR3_REG

	/* single request */
	RC5T619_write_reg (0x66, (0x10|adsel));	// ADCCNT3_REG
	for (i = 0; i < 5; i++) {
		udelay(2000);
		/* read completed flag of ADC */
		iChk = RC5T619_read_reg(0x8A,&bValue);	// EN_ADCIR3_REG
		if (iChk < 0) 
			printf("Error in reading EN_ADCIR3_REG register\n");
		if (bValue & 0x01) {
			/* disable interrupt request of single mode */
			RC5T619_write_reg (0x8A, 0x00);	// EN_ADCIR3_REG
			break;
		}
	}
	iChk = RC5T619_read_reg((adsel<<1) + 0x68,&bValue);	// VBATDATAH_REG
	if (iChk < 0) 
		printf("Error in reading H register\n");
	i = bValue<<4;

	iChk = RC5T619_read_reg((adsel<<1) + 0x69,&bValue);	// VBATDATAL_REG
	if (iChk < 0) 
		printf("Error in reading L register\n");
	i |= bValue&0x0F;

	return i;
}

#define THEM_BAT_HEAT	(606*4095/2500)		// 50 degrees
#define THEM_BAT_COLD	(1824*4095/2500)	// 0 degrees

// ADC*2*2.5/4095=V
#define BATTERY_THRESHOLD 		2948	// 3.6V
//#define BATTERY_USB_THRESHOLD 	3112	//3.8V
//#define BATTERY_USB_THRESHOLD 	3071	//3.75V
#define BATTERY_USB_THRESHOLD 	2989	// 3.65V
void _power_off (void)
{
	if (1 == gptNtxHwCfg->m_val.bPMIC) {
		unsigned char bVal;

		/* Disable all interrupt */
		RC5T619_write_reg(0x9D, 0);		// RICOH61x_INTC_INTEN
		/* Disable RTC alarm */
		RC5T619_read_reg(0xAE, &bVal);	// read rtc_ctrl1
		bVal &= 0xBF;	// clear DALE
		RC5T619_write_reg(0xAE, bVal);		// rtc_ctrl1
		/* Not repeat power ON after power off(Power Off/N_OE) */
		RC5T619_write_reg(0x0F, 0);		// RICOH61x_PWR_REP_CNT
		/* Power OFF */
		RC5T619_write_reg(0x0E, 1);		// RICOH61x_PWR_SLP_CNT
	}
}

void isBatCritical (void)
{
	if (49!=gptNtxHwCfg->m_val.bPCB && 61!=gptNtxHwCfg->m_val.bPCB)	// only block in boot with BN models.
		return;

	if (1 == gptNtxHwCfg->m_val.bPMIC) {
		int i = ricoh_read_adc (5);	// read battery temperature
		unsigned char regVal;

		RC5T619_read_reg(0x09, &regVal);	// read power on history
		if (regVal & 0xFA)
			printf ("RC5T619 power on history 0x%02X.\n", regVal);

		printf ("RC5T619 read battery temperature %dmV (0x%03X).\n", i*2500/4095, i);

		if ((THEM_BAT_HEAT > i) || (THEM_BAT_COLD < i)) {
			printf ("RC5T619 battery temperature too high / low. Force power off.\n");
			_power_off ();
		}

		if (IS_BATT_CRITIAL_LOW()) {
			/* bypass battery critical if usb / adaptor detected. */
			if (ntx_detect_usb_plugin(1)) {
				/* USB IN */
				unsigned short waitTime = 0;
				unsigned char  interval = 2; //sec
				unsigned short timeout = 480; //sec
				printf("Battery critical !! USB_IN, charge a while\n");
				if (49 == gptNtxHwCfg->m_val.bPCB)	//E60QDx
					timeout = 1200;		// set 20 minutes
				// set VWEAK to 3.3V
				RC5T619_read_reg (0xBA, &regVal);
				regVal &= 0xF3;
				regVal |= 0x08;
				RC5T619_write_reg (0xBA, regVal);
				i = ricoh_read_adc (1);
				while(1) {
					if (waitTime*interval > timeout) {
						printf("\nCharge Timeout !! (%d) secs\n", timeout);
						break;
					}
					i = ricoh_read_adc (1);
					if (!IS_BATT_CRITIAL_LOW() || (BATTERY_USB_THRESHOLD < i)) {
						printf("\nBattery critical !! charge completed, take (%d) secs\n", waitTime*interval, i);
						printf ("RC5T619 battery %dmv. (0x%X , battery low %d)\n", i*5000/4095, i, IS_BATT_CRITIAL_LOW());
						return;
					}
					if (!ntx_detect_usb_plugin(0)) {
						printf("\nBattery critical !! USB_OUT\n");
						break;
					}
					if (!(waitTime%10))
						printf ("RC5T619 battery too low %dmv. (0x%X , battery low %d)\n", i*5000/4095, i, IS_BATT_CRITIAL_LOW());
					printf(".");
					waitTime++;
					udelay(interval*1000000);
				}
			}
			else
				i = ricoh_read_adc (1);
			printf ("RC5T619 battery too low %dmv. (0x%X) Force power off.\n", i*5000/4095, i);

			_power_off ();
		}
		else {
			RC5T619_charger_redetect();
		}
	}
}




void ntx_preboot(int iBootType,char *I_pszKernCmdLine,
		char *O_pszKernCmdLine,unsigned long I_dwKernCmdLineSize)
{
	char *pcEnv_bootargs=I_pszKernCmdLine;
	char *pcLPJ;
	//unsigned char bSysPartType,bRootFsType,bUIStyle,bCustomer;
	unsigned long dwBootArgsLen;
	char cCmdA[128];
	static int giCallCnt = 0;

#if 0	//[
	if (is_boot_from_usb()) {
		printf("%s() boot from usb. Skip mmc read.\n",__func__);
		return ;
	}
#endif //]


	if(1==giCallCnt) {
		printf("%s():calling %d\n",__FUNCTION__,giCallCnt);	
		return ;
	}

	giCallCnt++;

#ifdef CONFIG_MFG


	_load_isd_hwconfig();

	strcat(O_pszKernCmdLine," boot_port=0");



#if (PHYS_SDRAM_SIZE==SZ_256M) || (PHYS_SDRAM_1_SIZE==(256*1024*1024)) //[
	strcat(O_pszKernCmdLine," mem=242M");
#elif (PHYS_SDRAM_SIZE==SZ_512M) || (PHYS_SDRAM_1_SIZE==(512*1024*1024)) //][
	strcat(O_pszKernCmdLine," mem=500M");
#elif (PHYS_SDRAM_SIZE==SZ_64M) || (PHYS_SDRAM_1_SIZE==(64*1024*1024)) //][
//	strcat(O_pszKernCmdLine," mem=52M");
#else //][ 
	#error "PHYS_SDRAM_1_SIZE || PHYS_SDRAM_SIZE value not supported ! please check !!"
#endif //]


	strcat(O_pszKernCmdLine," hwcfg_p=0x9ffffe00 hwcfg_sz=110");
	strcat(O_pszKernCmdLine," rdinit=/linuxrc g_mass_storage.stall=0 g_mass_storage.removable=1 g_mass_storage.file=/fat g_mass_storage.ro=1 g_mass_storage.idVendor=0x066F g_mass_storage.idProduct=0x37FF g_mass_storage.iSerialNumber=");

	//sprintf(cCmdA,"setenv bootcmd run bootcmd_mfg");
	//printf("%s():run \"%s\"\n",__FUNCTION__,cCmdA);	
	//run_command(cCmdA,0);

	return;
#endif

	ASSERT(gptNtxHwCfg);

	isBatCritical();
	
	giNtxBootMode = _detect_bootmode();
	
	

	NtxHiddenMem_load_ntxbin(&gtNtxHiddenMem_waveform,0);
	NtxHiddenMem_load_ntxbin(&gtNtxHiddenMem_ntxfw,0);

	NtxHiddenMem_append_kcmdline(I_pszKernCmdLine,I_dwKernCmdLineSize,O_pszKernCmdLine);

	gi_mmc_num_kernel = GET_ISD_NUM() ;
	sprintf(cCmdA," boot_port=%d",_get_boot_sd_number());
	strcat(O_pszKernCmdLine,cCmdA);

BOOTMODE_ENTRY:

	//printf("%s(%d):bootmode=%d\n",__FUNCTION__,__LINE__,giNtxBootMode);
	switch(giNtxBootMode)	{
	case NTX_BOOTMODE_ISD:
		if(gptNtxHwCfg) {
			if(9==gptNtxHwCfg->m_val.bCustomer) {
				_led_R(0);
				_led_G(1);
				_led_B(1);
			}
		}
		else {
			printf("boot normal : no hwconfig !\n");
		}
		break;
	case NTX_BOOTMODE_FASTBOOT:
#if defined(CONFIG_FASTBOOT) || defined(CONFIG_FSL_FASTBOOT) //[
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
		if(12==gptNtxHwCfg->m_val.bKeyPad) {
			// models without keys
#if 0 //[
			if(2==ntx_wait_powerkey(2,-1,0)) 
#else //][
			if(0==ntx_wait_powerkey(10,1,1)) // if user pressing and holding power key over 10 secs will return 0 or break and return non zero . 
#endif//]
			{
				// power key clicked .
				printf("enter recovery ...\n");
				giNtxBootMode=NTX_BOOTMODE_RECOVERY;
				goto BOOTMODE_ENTRY ;
			}
			else {
				giNtxBootMode=NTX_BOOTMODE_ISD;
			}
		}
		else {
			if(9==gptNtxHwCfg->m_val.bCustomer) {
				// . 
				if(0==ntx_wait_powerkey_ex(10,1,1,upg_key_chk_cus9)) {// if user pressing and holding power key over 10 secs will return 0 or break and return non zero . 
					printf("enter recovery cus %d ...\n",gptNtxHwCfg->m_val.bCustomer);
					giNtxBootMode=NTX_BOOTMODE_RECOVERY;
					goto BOOTMODE_ENTRY ;
				}
				else {
					giNtxBootMode=NTX_BOOTMODE_ISD;
				}

			}
		}
		break;
#endif//] CONFIG_FASTBOOT || CONFIG_FSL_FASTBOOT

	case NTX_BOOTMODE_SDOWNLOAD:
		run_command("download_mode",0);
		break;

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
		// bootm to load ntx kernel .
		_load_ntxkernel(&gpbKernelAddr,&gdwKernelSize);
	}

	dwBootArgsLen=strlen(O_pszKernCmdLine);
	if(dwBootArgsLen>=I_dwKernCmdLineSize) {
		printf("[ERROR] kernel command buffer size not enough !\n");
	}

#ifdef CONFIG_SYS_BOOT_SPINOR
	_load_ntxRamfs(&gpbRamfsAddr,&gdwRamfsSize);
//	sprintf(cCmdA,"setenv initrd_addr 0x%08X", gpbRamfsAddr);
	run_command(cCmdA,0);
	run_command("printenv",0);
	return;
#endif

	{ //[ 

		//if(NTX_BOOTMODE_ESD_UPG==giNtxBootMode) {
		//	_load_esd_hwconfig();
		//}


		
#ifndef CONFIG_MFG //[
		if(2==gptNtxHwCfg->m_val.bUIStyle)
#endif //]!CONFIG_MFG
		{
			// Android UI .

			strcat(O_pszKernCmdLine," androidboot.serialno=");
			strcat(O_pszKernCmdLine,(char *)(&gcNTX_SN[3]));
			//printf("bootargs=\"%s\"\n",O_pszKernCmdLine);

#if 0
			// only for KK android .
			 
			if(pcEnv_bootargs&&0==strstr(pcEnv_bootargs,"androidboot.hardware=")) {
				char *pszPCB_name;

#ifdef USE_HWCONFIG//[
				pszPCB_name=NtxHwCfg_GetCfgFldStrVal(gptNtxHwCfg,HWCFG_FLDIDX_PCB);
#else//][!USE_HWCONFIG
				if ( gptNtxHwCfg->m_val.bPCB<(sizeof(gszPCBA)/sizeof(gszPCBA[0])) ) {
					pszPCB_name = gszPCBA[gptNtxHwCfg->m_val.bPCB];
				}
				else {
					pszPCB_name = "unkown";
				}
#endif//]USE_HWCONFIG
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

		//if(!gpbRDaddr)
		{
			

			if(pcEnv_bootargs&&0==strstr(pcEnv_bootargs,"rootfstype=")) {
				printf("hwcfg rootfstype : %d\n",gptNtxHwCfg->m_val.bRootFsType);
				switch(gptNtxHwCfg->m_val.bRootFsType)
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

				printf("hwcfg partition type : %d,bootmode=%d\n",(int)gptNtxHwCfg->m_val.bSysPartType,giNtxBootMode);
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
					case 14://TYPE15
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
			if( 'M'==(char)gcNTX_SN[MAC_BOFFSET_IN_SNMAC_SECTOR+0] && \
					'A'==(char)gcNTX_SN[MAC_BOFFSET_IN_SNMAC_SECTOR+1] && \
					'C'==(char)gcNTX_SN[MAC_BOFFSET_IN_SNMAC_SECTOR+2] && \
					'='==(char)gcNTX_SN[MAC_BOFFSET_IN_SNMAC_SECTOR+3] && \
					'\0'==(char)gcNTX_SN[MAC_BOFFSET_IN_SNMAC_SECTOR+4+17] )
			{
				strcat(O_pszKernCmdLine," fec_mac=");
				strcat(O_pszKernCmdLine,&gcNTX_SN[MAC_BOFFSET_IN_SNMAC_SECTOR+4]);
			}
		}
#endif//]MAC_BOFFSET_IN_SNMAC_SECTOR

		if(9==gptNtxHwCfg->m_val.bCustomer) {
			if(pcEnv_bootargs&&0==strstr(pcEnv_bootargs,"quiet")) {
				strcat(O_pszKernCmdLine," quiet");
			}
		}

	} //] in hwconfig field ...

		
	//RC5T619_watchdog(1,32);

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

	//printf("%s : cmdline before change=\"%s\"\n",__FUNCTION__,pcEnv_bootargs);	

	ntx_preboot(0,pcEnv_bootargs,gszBootArgs,BOOTARGS_BUF_SIZE);

	//printf("%s : cmdline after change=\"%s\"\n",__FUNCTION__,gszBootArgs);

	if(pcEnv_bootargs) {
		sprintf(cCmdA,"setenv bootargs ${bootargs} %s",gszBootArgs);
		run_command(cCmdA, 0);// 
		printf("%s : cmd=%s\n",__FUNCTION__,cCmdA);	
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

static unsigned char *boot_recovery="boot-recovery";
static unsigned char *wipe_data="recovery\x0A--wipe_data\x0A--locale=en_US\x0A";
int g_ntx_misc_offset;
extern void setup_recovery_env(void);

static int read_recovery_BCB (unsigned char *pBuf)
{
	char cCmdA[128+1];
	sprintf(cCmdA,"mmc dev %d",GET_ISD_NUM());
	run_command(cCmdA,0);

	if (!g_ntx_misc_offset) {
		sprintf(cCmdA,"mmc part");
		run_command(cCmdA,0);
		if (!g_ntx_misc_offset) {
			printf ("misc partition not found!!\n");
			return -1;
		}
	}

#ifdef MMC_CMD_SEPERATED_SDNUM
	sprintf(cCmdA,"mmc read 0x%x 0x%x 0x1",(unsigned)pBuf, g_ntx_misc_offset);
#else //][!MMC_CMD_SEPERATED_SDNUM	
	sprintf(cCmdA,"mmc read %d 0x%x 0x%x 0x1", GET_ISD_NUM(), (unsigned)pBuf, g_ntx_misc_offset);
#endif //]MMC_CMD_SEPERATED_SDNUM
	printf ("Writing recovery BCB to misc partition...\n%s\n",cCmdA);
	run_command(cCmdA, 0);//
	return 0;
}

int write_recovery_BCB (char *pBuf)
{
	char cCmdA[128+1];
	unsigned char cBCB[512], *pBCB;

	sprintf(cCmdA,"mmc dev %d",GET_ISD_NUM());
	run_command(cCmdA,0);

	if (!g_ntx_misc_offset) {
		sprintf(cCmdA,"mmc part");
		run_command(cCmdA,0);
		if (!g_ntx_misc_offset) {
			printf ("misc partition not found!!\n");
			return 0;
		}
	}

	if (pBuf) {
		pBCB = pBuf;
		printf ("Writing boot count %d to misc partition (offset %d)...\n", pBuf[64], g_ntx_misc_offset);
	}
	else {
		memset (cBCB, 0, 512);
		strcpy (cBCB, boot_recovery);
		strcpy (&cBCB[64], wipe_data);
		pBCB = cBCB;
		printf ("Writing recovery BCB to misc partition (offset %d)...\n", g_ntx_misc_offset);
	}

#ifdef MMC_CMD_SEPERATED_SDNUM
	sprintf(cCmdA,"mmc write 0x%x 0x%x 0x1",(unsigned)pBCB, g_ntx_misc_offset);
#else //][!MMC_CMD_SEPERATED_SDNUM	
	sprintf(cCmdA,"mmc write %d 0x%x 0x%x 0x1", GET_ISD_NUM(), (unsigned)pBCB, g_ntx_misc_offset);
#endif //]MMC_CMD_SEPERATED_SDNUM
	run_command(cCmdA, 0);//

//	if (!pBuf)
//		setup_recovery_env();
	return 0;
}

#define NTX_BOOT_CNT_SIG	"NTX_BOOT_COUNT"
int ntx_check_and_increase_boot_count (void)
{
	unsigned char cMiscA[512];
	if (0 == read_recovery_BCB (cMiscA)) {	// enter recovery if recovery flag found.
		if (!strcmp (cMiscA, boot_recovery)) {
			printf ("Recovery BCB found, skip count!\n");
			return 1;
		}
		
#ifdef NTX_ANDROID_BOOT_FAIL_COUNT
		// this will enable boot failed conter for Android. (E60QDx customer request function.)
		if (!strcmp (cMiscA, NTX_BOOT_CNT_SIG)) {
			if (8 < ++cMiscA[64]) {
				write_recovery_BCB (0);
				return 1;
			}
			else 
				write_recovery_BCB (cMiscA);
		}
		else {
			memset (cMiscA, 0, 512);
			strcpy (cMiscA, NTX_BOOT_CNT_SIG);
			++cMiscA[64];
			write_recovery_BCB (cMiscA);
		}
#endif
	}
	else 
		printf ("misc partition not found!!\n");

	return 0;
}


unsigned long long ntx_get_timeout_ustick(unsigned long dwUS)
{
	unsigned long long u64_timeout_tick;
	uint64_t _Ticks;
	extern uint64_t usec_to_tick(unsigned long usec);
	_Ticks = usec_to_tick(dwUS);
	u64_timeout_tick = get_ticks() + _Ticks;
	return u64_timeout_tick;
}

int ntx_is_tick_timeout(unsigned long long u64TimeoutTick)
{
	unsigned long long u64_current_tick = get_ticks();
	if(u64_current_tick>=u64TimeoutTick) {
		return 1;
	}
	else {
		return 0;
	}
}


/*
 * This is a power key checking loop . 
 * iWaitSecs : the waiting seconds in power key state checking loop . 
 * iWaitPwrKeyStatChgTimes : the condition to break this checking loop . if the power key state changging conuter over this value will break the loop . 
 * iChkEnterPwrKeyState : check the power key intial state when enter this funcion . -1 means not check this condition .  
 * ext_wait_condition_fn : this is a function callback ptr . the checking loop will be break when this callback exist and return 0 . 
 */ 

int ntx_wait_powerkey_ex(int iWaitSecs,int iWaitPwrKeyStatChgTimes,int iChkEnterPwrKeyState,int *(ext_wait_condition_fn)(void))
{
	unsigned long long u64_timeout_tick = ntx_get_timeout_ustick(iWaitSecs*1000*1000);
	unsigned long dwLoopCnt =	0;
	int iLedOnOff=0;
	int iPwrkeyLastState,iPwrkeyCurState;
	unsigned long dwPwrkeyStatChgCnt =	0;
#define PWRKEY_DEBOUNCE_TOTAL		80000
	unsigned long dwPwrkeyDebounceCnt=0;

	if(iChkEnterPwrKeyState!=-1) {
		iPwrkeyLastState=_power_key_status();
		if(iPwrkeyLastState!=iChkEnterPwrKeyState) {
			return -1;
		}
	}

	printf("%s() : checking power key ...\n",__FUNCTION__);
	do {

		if(0==(dwLoopCnt&0xfff)) {
			_led_R(iLedOnOff);
			_led_G(iLedOnOff);
			_led_B(iLedOnOff);
			iLedOnOff = (iLedOnOff)?0:1;
		}


		if(0==dwPwrkeyDebounceCnt) {
			iPwrkeyCurState = _power_key_status();
			if( iPwrkeyCurState != iPwrkeyLastState ) {
				dwPwrkeyStatChgCnt++;

				if((unsigned long)iWaitPwrKeyStatChgTimes==dwPwrkeyStatChgCnt) {
					break; 
				}

				dwPwrkeyDebounceCnt=PWRKEY_DEBOUNCE_TOTAL;
				iPwrkeyLastState = iPwrkeyCurState;
			}
		}
		else {
			dwPwrkeyDebounceCnt--;
		}

		
		if(ext_wait_condition_fn && !ext_wait_condition_fn()) {
			printf("%s():wait condition check failed !!!\n",__FUNCTION__);
			return -2;
		}

		if(ntx_is_tick_timeout(u64_timeout_tick)) {
			printf("%s():times out!!!\n",__FUNCTION__);
			break;
		}

		dwLoopCnt++;
	}while (1);


	if(dwPwrkeyStatChgCnt) {
		printf("%s():power key stat changed %d!!!\n",__FUNCTION__,dwPwrkeyStatChgCnt);
		return (int)dwPwrkeyStatChgCnt;
	}

	return 0;
}
int ntx_wait_powerkey(int iWaitSecs,int iWaitPwrKeyStatChgTimes,int iChkEnterPwrKeyState)
{
	return ntx_wait_powerkey_ex(iWaitSecs, iWaitPwrKeyStatChgTimes, iChkEnterPwrKeyState, 0);
}




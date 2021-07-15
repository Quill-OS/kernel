

#include <common.h>
#include <command.h>
#include <exports.h>
#include <i2c.h>
#include <mxc_keyb.h>

#include "ntx_hwconfig.h"

typedef struct tagRTC_HumanReadable {
	unsigned short wYear; // 2010~XXXX .
	unsigned char bMonth; // 1~12 .
	unsigned char bDay; // 1~31 .
	
	unsigned char bHour; // 0~23 .
	unsigned char bMin; // 0~59 .
	unsigned char bSecs; // 0~59 .
}RTC_HR;


static unsigned char gbEsdUPGKeyVal_E60612[2]={0x00,0x01};

static unsigned char gbMicroP_VersionA[2] ;
static int giIsMicroP_inited=0;

static const unsigned char gbMicroPI2C_ChipAddr = 0x43;
static const unsigned short gwDefaultStartYear = 2000; // microp default start year .


extern NTX_HWCONFIG *gptNtxHwCfg ;

#define EVENT_REG_BIT_POWERON		0x80 // 1: power on .
#define EVENT_REG_BIT_KEYIN			0x40 // 1: keypad input .
#define EVENT_REG_BIT_ESDIN			0x20 // 1: External SD card inserted .
#define EVENT_REG_BIT_I2CEVT		0x10 // 1: i2c event for touch .
#define EVENT_REG_BIT_UARTEVT		0x08 // 1: uart event for touch .
#define EVENT_REG_BIT_DCIN			0x04 // 1: DC in .
#define EVENT_REG_BIT_BATLOW		0x02 // 1: battery is low .
#define EVENT_REG_BIT_BATCRTLOW		0x01 // 1: battery very low .


int ntxup_init(void)
{
	int iRet = 0 ;
	int iChk;
	
	init_pwr_i2c_function(0);
	iChk = i2c_read(gbMicroPI2C_ChipAddr, 0, 1, gbMicroP_VersionA, 2);
	if(0==iChk) {
		// success .
		printf("microp version=0x%02x%02x\n",gbMicroP_VersionA[0],gbMicroP_VersionA[1]);
	}
	else {
		printf("microp version read fail !\n");
		iRet = -1;
	}
	giIsMicroP_inited = 1;
	
	return iRet ;
}

int ntxup_get_adcvalue(unsigned short *O_pwAdcValue)
{
	int iRet = 0;
	int iChk ;
	unsigned char bBufA[2];
	
	if(!giIsMicroP_inited) {
		ntxup_init();
	}
	
	iChk = i2c_read(gbMicroPI2C_ChipAddr, 0x41, 1, bBufA, 2);
	if(0==iChk) {
		if(O_pwAdcValue) {
			*O_pwAdcValue = (bBufA[0] << 8) | bBufA[1];
		}
	}
	else {
		printf("%s : ctrl&event read fail (%d)!\n",__FUNCTION__,iChk);
		iRet = -1;
	}
	return iRet;
}


extern int _sd_cd_status (void);

int ntxup_get_CtrlEvent(unsigned char *O_pbEvent,unsigned char *O_pbCtrl)
{
	int iRet = 0;
	int iChk ;
	unsigned char bBufA[2];
	
	if(!giIsMicroP_inited) {
		ntxup_init();
	}
	//printf("%s : ctrl&event=0x%02x%02x\n",__FUNCTION__,bBufA[0],bBufA[1]);
	iChk = i2c_read(gbMicroPI2C_ChipAddr, 0x60, 1, bBufA, 2);
	if(0==iChk) {
		if(O_pbEvent) {
			*O_pbEvent = bBufA[1];
		}
		if(O_pbCtrl) {
			*O_pbCtrl = bBufA[0];
		}
	}
	else {
		printf("%s : ctrl&event read fail (%d)!\n",__FUNCTION__,iChk);
		iRet = -1;
	}
	
	return iRet;
}

int ntxup_is_ext_card_inserted(void)
{
#if 0
	// card insert detected by micro-p .
	int iRet = 0;
	unsigned char bEvent;
	
	if(ntxup_get_CtrlEvent(&bEvent,0)<0) {
		// read ctrl & event error .
	}
	else {
		if(bEvent&EVENT_REG_BIT_ESDIN) {
			iRet = 1;
		}
	}
	
	return iRet;
#else 
	return (_sd_cd_status()) ? 1 : 0;
#endif
}


int ntxup_rtc_cmd(int iIsSet,
	unsigned char *IO_pbY,unsigned char *IO_pbM,unsigned char *IO_pbD,
	unsigned char *IO_pbH,unsigned char *IO_pbm,unsigned char *IO_pbS)
{
	int iRet = 0;
	int iChk;
	
	if(!giIsMicroP_inited) {
		ntxup_init();
	}
	
	if(iIsSet) {
		if(IO_pbY) {
			iChk = i2c_write(gbMicroPI2C_ChipAddr, 0x10, 1, IO_pbY, 1);
			if(iChk!=0) {
				iRet = -1;
			}
		}
		if(IO_pbM) {
			iChk = i2c_write(gbMicroPI2C_ChipAddr, 0x11, 1, IO_pbM, 1);
			if(iChk!=0) {
				iRet = -2;
			}
		}
		if(IO_pbD) {
			iChk = i2c_write(gbMicroPI2C_ChipAddr, 0x12, 1, IO_pbD, 1);
			if(iChk!=0) {
				iRet = -3;
			}
		}
		if(IO_pbH) {
			iChk = i2c_write(gbMicroPI2C_ChipAddr, 0x13, 1, IO_pbH, 1);
			if(iChk!=0) {
				iRet = -4;
			}
		}
		if(IO_pbm) {
			iChk = i2c_write(gbMicroPI2C_ChipAddr, 0x14, 1, IO_pbm, 1);
			if(iChk!=0) {
				iRet = -5;
			}
		}
		if(IO_pbS) {
			iChk = i2c_write(gbMicroPI2C_ChipAddr, 0x15, 1, IO_pbS, 1);
			if(iChk!=0) {
				iRet = -6;
			}
		}
	}
	else {
		unsigned char bBufA[2];
		if(IO_pbY||IO_pbM) {
			iChk = i2c_read(gbMicroPI2C_ChipAddr, 0x20, 1, bBufA, 2);
			if(iChk!=0) {
				iRet = -7;
			}
			//printf("YM=0x%02x,0x%02x\n",bBufA[0],bBufA[1]);
			if(IO_pbY) {
				*IO_pbY = bBufA[0];
			}
			if(IO_pbM) {
				*IO_pbM = bBufA[1];
			}
		}
		if(IO_pbD||IO_pbH) {
			iChk = i2c_read(gbMicroPI2C_ChipAddr, 0x21, 1, bBufA, 2);
			if(iChk!=0) {
				iRet = -8;
			}
			//printf("DH=0x%02x,0x%02x\n",bBufA[0],bBufA[1]);
			if(IO_pbD) {
				*IO_pbD = bBufA[0];
			}
			if(IO_pbH) {
				*IO_pbH = bBufA[1];
			}
		}
		if(IO_pbm||IO_pbS) {
			iChk = i2c_read(gbMicroPI2C_ChipAddr, 0x23, 1, bBufA, 2);
			if(iChk!=0) {
				iRet = -9;
			}
			//printf("mS=0x%02x,0x%02x\n",bBufA[0],bBufA[1]);
			if(IO_pbm) {
				*IO_pbm = bBufA[0];
			}
			if(IO_pbS) {
				*IO_pbS = bBufA[1];
			}
		}
	}
	
	return iRet;
}


int ntxup_rtc_set(RTC_HR *ptRTCHR) 
{
	int iRet = 0;
	unsigned char bYear;
	
	// convert humanable format to microp format ...
	if(ptRTCHR->wYear<gwDefaultStartYear) {
		return -1;
	}
	bYear = (unsigned char)(ptRTCHR->wYear - gwDefaultStartYear);
	
	if( ntxup_rtc_cmd(1,&bYear,&ptRTCHR->bMonth,&ptRTCHR->bDay,
		&ptRTCHR->bHour,&ptRTCHR->bMin,&ptRTCHR->bSecs) <0) 
	{
		iRet = -2;
	}
	
	return iRet;
}

int ntxup_rtc_get(RTC_HR *ptRTCHR) 
{
	int iRet = 0;
	unsigned char bYear;
	
	
	if( ntxup_rtc_cmd(0,&bYear,&ptRTCHR->bMonth,&ptRTCHR->bDay,
		&ptRTCHR->bHour,&ptRTCHR->bMin,&ptRTCHR->bSecs) <0) 
	{
		iRet = -2;
	}
	
	ptRTCHR->wYear = (unsigned short)bYear ;
	ptRTCHR->wYear += gwDefaultStartYear;
	
	return iRet;
}

int ntxup_wait_key(unsigned char *pbWaitKeysMaskA,int iIntervalms,int iTimeoutms)
{
	int iRet = -1;
	int iTotalms = 0;
	unsigned char bReadKeyValA[2];
	
	
	if( 4== gptNtxHwCfg->m_val.bKeyPad ) {
		
		struct kpp_key_info *key_info;
		int keys, i;
		int iKeyPressedCnt = 0;
		
		unsigned short wKeyVal = 0;
		
		printf("[only 1 key]\n");
		
		mxc_kpp_init();
		do {
			udelay(iIntervalms*1000);
				
			key_info = 0;
			keys = mxc_kpp_getc(&key_info);
			for (i = 0; i < keys; i++) {
				if (test_key(i, &key_info[i])) {
					wKeyVal |= 0x1 << i;
					iKeyPressedCnt++;
				}
			}
			bReadKeyValA[0] = (wKeyVal>>8)&0xff;
			bReadKeyValA[1] = (wKeyVal)&0xff;
			printf("%d keys,0x%02x,0x%02x\n",keys,bReadKeyValA[0],bReadKeyValA[1]);
			
			if( (0==(bReadKeyValA[0]^pbWaitKeysMaskA[0])) && \
				(0==(bReadKeyValA[1]^pbWaitKeysMaskA[1])) ) 
			{
				iRet = 0;
				break;
			}

			if(key_info) {
				free(key_info);
			}
			
			iTotalms += iIntervalms;
		}while(iTotalms<iTimeoutms) ;
		
	}
	else {
		unsigned short wWaitKeyBits ;
		int iChk ;
		//int i;
		
		
		if(!giIsMicroP_inited) {
			ntxup_init();
		}
		
		i2c_read(gbMicroPI2C_ChipAddr, 0x42, 1, bReadKeyValA, 2);// clear key value .
		do {
			
			iChk = i2c_read(gbMicroPI2C_ChipAddr, 0x42, 1, bReadKeyValA, 2);
			if(0==iChk) {
				// success .
				//printf("%s : keyval=0x%02x%02x,waitkeysmask=0x%02x%02x\n",__FUNCTION__,
				//	bReadKeyValA[0],bReadKeyValA[1],pbWaitKeysMaskA[0],pbWaitKeysMaskA[1]);
					
				bReadKeyValA[0] &= pbWaitKeysMaskA[0];
				bReadKeyValA[1] &= pbWaitKeysMaskA[1];
				
				
				if( (0==(bReadKeyValA[0]^pbWaitKeysMaskA[0])) && \
					(0==(bReadKeyValA[1]^pbWaitKeysMaskA[1])) ) 
				{
					iRet = 0;
					break;
				}
			}
			else {
				iRet = -2;
				printf("%s : keyval read fail !\n",__FUNCTION__);
				//break;
			}
			
			udelay(iIntervalms*1000);
			iTotalms += iIntervalms;
		}while(iTotalms<iTimeoutms) ;
		
	}
	return iRet;
	
}

int ntxup_wait_key_esdupg(void) 
{
	int iRet;

	//printf("%s: gptNtxHwCfg=%p ,pcb=%d\n",__FUNCTION__,gptNtxHwCfg,gptNtxHwCfg->m_val.bPCB);
	if( 12 == gptNtxHwCfg->m_val.bPCB) {// E60612 .
		iRet = (ntxup_wait_key(gbEsdUPGKeyVal_E60612,5,20)==0)?1:0;
	}
	else {
		printf("%s: fail ,pcb=%d\n",__FUNCTION__,gptNtxHwCfg->m_val.bPCB);
		iRet = -1;
	}

	return iRet;
}

static int do_ntxup(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	int iChk ;
	unsigned char bBufA[2];
	
	if(argc<2) {
		printf("%s:argc<2\n\t%s\n",__FUNCTION__,cmdtp->usage);
		return -1;
	}

	if(0==strcmp(argv[1],"init")) {
		iRet = ntxup_init();
	}
	else 
	if(0==strcmp(argv[1],"release")) {
		iRet = init_pwr_i2c_function(1);
	}
	else
	if(0==strcmp(argv[1],"getkey")) {
		
		if(!giIsMicroP_inited) {
			ntxup_init();
		}


		iChk = i2c_read(0x43, 0x42, 1, bBufA, 2);
		if(0==iChk) {
			// success .
			printf("keyval=0x%02x%02x\n",bBufA[0],bBufA[1]);
		}
		else {
			printf("microp key read fail !\n");
		}
	}
	else
	if(0==strcmp(argv[1],"extcard_st")) {
		if(ntxup_is_ext_card_inserted()) {
			printf("inserted\n");
		}
		else {
			printf("NoCard\n");
		}
	}
	else
	if(0==strcmp(argv[1],"waitkey")) {
	}
	else {
	}
	return iRet;
}

U_BOOT_CMD(ntxup, 6, 0, do_ntxup,
	"ntxup - netronix microp commands\n",
	"ntxup init"
		" - ntxup initial : initial bus & protocols .\n"
	"ntxup release"
		" - ntxup release : release bus & protocols .\n"
	"ntxup getkey"
		" - ntxup get key value without wait .\n"
	"ntxup extcard_st"
		" - ntxup get external card status \"inserted\" or \"NoCard\" .\n"
	"ntxup waitkey [keyvalue] [interval (ms)] [timeout (ms)]"
		" - polling key each interval and wait until key pressed .\n"
);

// gallen add 2011/03/31 [
#define TOTAL_KEY	16
static char *gszKeyStringA[TOTAL_KEY] = {
	"[S01]","[S02]","[S03]","[S04]",
	"[S05]","[S06]","[S07]","[S08]",
	"[S09]","[S10]","[S11]","[S12]",
	"[S13]","[S14]","[S15]","[S16]"
};

inline int test_key(int value, struct kpp_key_info *ki)
{
	return (ki->val == value) && (ki->evt == KDepress);
}


static int do_mf_key(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	struct kpp_key_info *key_info;
	int keys, i;
	int iKeyPressedCnt = 0;

	mxc_kpp_init();
	udelay (1000);
	keys = mxc_kpp_getc(&key_info);
	for (i = 0; i < keys; i++) {
		if (test_key(i, &key_info[i])) {
			printf("fail: %s\n\r",gszKeyStringA[i]);
			iKeyPressedCnt++;
		}
	}
	free(key_info);

	if(0==iKeyPressedCnt) {
		printf("pass\n\r");
	}
	return 0;
}

U_BOOT_CMD(mf_key, 2, 0, do_mf_key,
	"mf_key - ntx key test \n",
	"mf_key "
		" - ntx key test .\n"
);
//] gallen add 2011/03/31


static int do_mf_rtc(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	RTC_HR tRTC_HR;
	
	if(argc==7) {
		tRTC_HR.wYear = simple_strtoul(argv[1], NULL, 10);
		tRTC_HR.bMonth = simple_strtoul(argv[2], NULL, 10);
		tRTC_HR.bDay = simple_strtoul(argv[3], NULL, 10);
		tRTC_HR.bHour = simple_strtoul(argv[4], NULL, 10);
		tRTC_HR.bMin = simple_strtoul(argv[5], NULL, 10);
		tRTC_HR.bSecs = simple_strtoul(argv[6], NULL, 10);
		
		if(ntxup_rtc_set(&tRTC_HR)<0) {
			printf("RTC set Error 1\n\n");
			iRet = -1;
		}
		else {
			printf("\rSet time %04d/%02d/%02d %02d:%02d:%02d\n\r",
				tRTC_HR.wYear,tRTC_HR.bMonth,tRTC_HR.bDay,
				tRTC_HR.bHour,tRTC_HR.bMin,tRTC_HR.bSecs);
		}
		
	}
	else if(argc==1) {

		if(ntxup_rtc_get(&tRTC_HR)<0) {
			printf("RTC get Error 1\n\n");
			iRet = -2;
		}
		else {
			printf("\r%04d/%02d/%02d %02d:%02d:%02d\n\r",
				tRTC_HR.wYear,tRTC_HR.bMonth,tRTC_HR.bDay,
				tRTC_HR.bHour,tRTC_HR.bMin,tRTC_HR.bSecs);
		}
	}
	else {
		printf("parameter cnt error argc=%d !\n",argc);
	}
	
	

	return iRet;
}

U_BOOT_CMD(mf_rtc, 7, 0, do_mf_rtc,
	"mf_rtc - rtc date/time set/get \n",
	"mf_rtc "
		" - rtc date/time get .\n"
	"mf_rtc <year> <month> <day> <hour> <min> <sec> "
		" - rtc date/time set .\n"
);

static int do_mf_sd_wp(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	unsigned char bEvent;
	int iIsWriteProtected = 0;
	
	if(ntxup_get_CtrlEvent(&bEvent,0)>=0) {
		if(gptNtxHwCfg&&1==gptNtxHwCfg->m_val.bExternalMem) {
			// external memory is SD card .
			extern int esd_wp_read(void);
			
			iIsWriteProtected = esd_wp_read();
			if(bEvent&EVENT_REG_BIT_ESDIN) {
				if(iIsWriteProtected) {
					printf("\rSD in :enabled\n\r");
				}
				else {
					printf("\rSD in :disabled\n\r");
				}
			}
			else {
				if(iIsWriteProtected) {
					printf("\rSD out :enabled\n\r");
				}
				else {
					printf("\rSD out :disabled\n\r");
				}
			}
		}
		else {
			// No external , micro sd or Nand flash .
			if(bEvent&EVENT_REG_BIT_ESDIN) {
				printf("\rSD in :disabled\n\r");
			}
			else {
				printf("\rSD out :enabled\n\r");
			}
		}
	}
	else {
		// get value from microp fail ! .
		iRet = -1;
	}

	return iRet;
}

U_BOOT_CMD(mf_sd_wp, 1, 0, do_mf_sd_wp,
	"mf_sd_wp - external sd card info \n",
	"mf_sd_wp "
		" - extern sd card info .\n"
);


static int do_mf_adc(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	unsigned short wADC;
	
	if(ntxup_get_adcvalue(&wADC)>=0) {
		iRet = (int)wADC;
		printf("%d\n\r", iRet);
	}
	else {
		iRet = -1;
	}

	return iRet;
}

U_BOOT_CMD(mf_adc, 1, 0, do_mf_adc,
	"mf_adc - get adc value \n",
	"mf_adc "
		" - get adc value .\n"
);

#if 0
// gallen add 2011/03/02 [
#define NTX_BOOTMODE_NA		(-1)
#define NTX_BOOTMODE_ISD	0 // internal sd boot ,internal kernel,root @ internal sdcard partition 1.
#define NTX_BOOTMODE_ESD	1 // external sd boot ,detect external kernel or use internal kernel , root @ external sdcard partition 2.

static int giNtxBootMode = NTX_BOOTMODE_NA ;

static int do_load_ntxbins(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	char cAppendStr[128];
	
	if(NTX_BOOTMODE_NA == giNtxBootMode) {
		giNtxBootMode = _detect_bootmode_and_append_boot_args(gszBootArgs,BOOTARGS_BUG_SIZE);
	}
	
	_load_ntxbins_and_append_boot_args(gszBootArgs,BOOTARGS_BUG_SIZE);
	
	setenv("bootargs",gszBootArgs);	
	return iRet;
}

U_BOOT_CMD(load_ntxbins, 2, 0, do_load_ntxbins,
	"load_ntxbins - netronix binaries load \n",
	"load_ntxbins "
		" - load netronix binaries from sd card (hwcfg,logo,waveform).\n"
);
//] gallen add 2011/03/02
#endif

// gallen add 2011/03/31 [
static int do_get_PCBA_id(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	
	if(!gptNtxHwCfg) {
		run_command("load_ntxbins",0);
	}
	
	if(gptNtxHwCfg) {
		printf("PCBA_ID:%d\n",gptNtxHwCfg->m_val.bPCB);
	}
	else {
		printf("PCBA_ID:-1\n");
	}
	return iRet;
}

U_BOOT_CMD(get_PCBA_id, 2, 0, do_get_PCBA_id,
	"get_PCBA_id - get PCBA id \n",
	"get_PCBA_id "
		" - get netronix PCBA id .\n"
);
//] gallen add 2011/03/31

static int do_get_up_ver(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	iRet = ntxup_init();
	return iRet;
}


U_BOOT_CMD(get_up_ver, 2, 0, do_get_up_ver,
	"get_up_ver - get microp version \n",
	"get_up_ver "
		" - get microp version .\n"
);

#include <mmc.h>

static int card_get_capacity_size(void)
{
	int iRet = 0;
	struct mmc *mmc;
	
	mmc = find_mmc_device(0);
	
	if (mmc) {
		if (mmc_init(mmc))
			puts("MMC card init failed!\n");
		else
			iRet = mmc->capacity>>10;
	}
	
	return iRet;
}

extern void E60612_led_R(int iIsTrunOn);
extern void E60612_led_G(int iIsTrunOn);
extern void E60612_led_B(int iIsTrunOn);

static int do_nandinfo(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	int cardsize = card_get_capacity_size();
	
	E60612_led_R (1);
	E60612_led_G (1);
	E60612_led_B (1);
	
	if (7000000 < cardsize)
		cardsize = 8;
	else if (3000000 < cardsize)
		cardsize = 4;
	else if (1500000 < cardsize)
		cardsize = 2;
	else 
		cardsize = 1;

    printf("\r[SD card] %dGB\n\r\n\r", cardsize);	

	return iRet;
}

U_BOOT_CMD(nandinfo, 2, 0, do_nandinfo,
	"nandinfo - get nand flash information .\n",
	"nandinfo "
		" - get get nand flash information .\n"
);


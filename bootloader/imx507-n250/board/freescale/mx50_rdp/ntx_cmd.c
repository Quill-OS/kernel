

#include <common.h>
#include <command.h>
#include <exports.h>
#include <i2c.h>
#include <mxc_keyb.h>
#include <rtc.h>
#include <asm/io.h>
#include <asm/arch/mx50.h>
#include <asm/arch/mx50_pins.h>
#include <asm/arch/iomux.h>

#include "ntx_hwconfig.h"

#include "ntx_comm.h"

typedef struct tagRTC_HumanReadable {
	unsigned short wYear; // 2010~XXXX .
	unsigned char bMonth; // 1~12 .
	unsigned char bDay; // 1~31 .
	
	unsigned char bHour; // 0~23 .
	unsigned char bMin; // 0~59 .
	unsigned char bSecs; // 0~59 .
}RTC_HR;


static unsigned char gbEsdUPGKeyVal_default[2]={0x00,0x01}; // E60612,E60612D,E60612C,E606A2,E606B2 ,....
static unsigned char gbEsdUPGKeyVal_E60630[2]={0x00,0x02};
static unsigned char gbEsdUPGKeyVal_E50600[2]={0x00,0x08};

static unsigned char gbMicroP_VersionA[2] ;
static int giIsMicroP_inited=0;

static const unsigned char gbMicroPI2C_ChipAddr = 0x43;
static const unsigned short gwDefaultStartYear = 2000; // microp default start year .

static const unsigned char gbZforceI2C_ChipAddr = 0x50;

static int isUpgMSP430=0;

extern NTX_HWCONFIG *gptNtxHwCfg ;

#define EVENT_REG_BIT_POWERON		0x80 // 1: power on .
#define EVENT_REG_BIT_KEYIN			0x40 // 1: keypad input .
#define EVENT_REG_BIT_ESDIN			0x20 // 1: External SD card inserted .
#define EVENT_REG_BIT_I2CEVT		0x10 // 1: i2c event for touch .
#define EVENT_REG_BIT_UARTEVT		0x08 // 1: uart event for touch .
#define EVENT_REG_BIT_DCIN			0x04 // 1: DC in .
#define EVENT_REG_BIT_BATLOW		0x02 // 1: battery is low .
#define EVENT_REG_BIT_BATCRTLOW		0x01 // 1: battery very low .

static int giCurrentHallSensorState = -1;
extern int _hallsensor_status (void);
extern int _get_sd_number(void);


static unsigned short calc_crc16(unsigned char *data, unsigned short len) {

	unsigned short crc = 0xffff;
	unsigned char i;
 
	while (len--) {
		crc ^= *data++ << 8;
		for (i = 0; i < 8; i++) {
			crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
		}
	}
	return crc;

}

static void add_crc16(unsigned char *data, int len) {

	unsigned short crc = calc_crc16(data, len);
	data[len] = crc & 0xff;
	data[len+1] = (crc >> 8) & 0xff;
}

int ntxup_init(void)
{
	int iRet = 0 ;
	int iChk;
	
	if(1!=giIsMicroP_inited) {
		giCurrentHallSensorState = _hallsensor_status();
	}
	
	init_pwr_i2c_function(0);
	iChk = i2c_read(gbMicroPI2C_ChipAddr, 0, 1, gbMicroP_VersionA, 2);
	if(0==iChk) {
		// success .
		printf("microp version=0x%02x%02x\n",gbMicroP_VersionA[0],gbMicroP_VersionA[1]);
		if (0xe9==gbMicroP_VersionA[0] && 0x16==gbMicroP_VersionA[1]) {
			char buffer[10];
			isUpgMSP430 = 1;
			buffer[0]=22;
			buffer[1]=0xFF;
			add_crc16(buffer, 2);
			i2c_write(gbMicroPI2C_ChipAddr, 22, 1, &buffer[1], 3);
		}
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
	
	if (isUpgMSP430) {
		iChk = i2c_read(gbMicroPI2C_ChipAddr, 0x08, 1, bBufA, 2);
		if(0==iChk) {
			if(O_pwAdcValue) {
				*O_pwAdcValue = ((bBufA[1] << 8) | bBufA[0])>>2;
			}
		}
		else {
			printf("%s : ctrl&event read fail (%d)!\n",__FUNCTION__,iChk);
			iRet = -1;
		}
	}
	else {
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
	
	if (isUpgMSP430) {
		struct rtc_time tm;
		unsigned long time;
		char buffer[10];
		
		if(iIsSet) {
			time = mktime ((*IO_pbY+gwDefaultStartYear), *IO_pbM, *IO_pbD, *IO_pbH, *IO_pbm, *IO_pbS);
			buffer[0] = 5;
			buffer[1] = 0;
			buffer[2] = time&0xFF;
			buffer[3] = (time>>8)&0xFF;
			buffer[4] = (time>>16)&0xFF;
			buffer[5] = (time>>24)&0xFF;
			add_crc16(buffer, 6);
			iChk = i2c_write(gbMicroPI2C_ChipAddr, 5, 1, &buffer[1], 7);
			if(iChk!=0) {
				iRet = -1;
			}
		}               
		else {
			iChk = i2c_read(gbMicroPI2C_ChipAddr, 4, 1, buffer, 4);
			time = buffer[3]<<24 | buffer[2]<<16 | buffer[1]<<8 | buffer[0];
			to_tm(time, &tm);
			if (gwDefaultStartYear > tm.tm_year)
				*IO_pbY = 0; 
			else
				*IO_pbY = tm.tm_year-gwDefaultStartYear; 
			*IO_pbM = tm.tm_mon; 
			*IO_pbD = tm.tm_mday;
			*IO_pbH = tm.tm_hour; 
			*IO_pbm = tm.tm_min;
			*IO_pbS = tm.tm_sec;
		}
	}
	else {
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

int rtc_get (struct rtc_time *tm)
{
	return 0;
}

int rtc_set (struct rtc_time *tm)
{
	return 0;
}

void rtc_reset (void) {}

int ntxup_wait_key(unsigned char *pbWaitKeysMaskA,int iIntervalms,int iTimeoutms)
{
	int iRet = -1;
	int iTotalms = 0;
	unsigned char bReadKeyValA[2];
	
	struct kpp_key_info *key_info;
	int keys, i;
	int iKeyPressedCnt = 0;
	
	unsigned short wKeyVal = 0;
	
	mxc_kpp_init();
	do {
		udelay(iIntervalms*1000);
			
		key_info = 0;
		keys = mxc_kpp_getc(&key_info);
		for (i = 0; i < keys; i++) {
			if (key_info[i].evt == KDepress) {
				wKeyVal |= 0x1 << key_info[i].val;
				iKeyPressedCnt++;
			}
		}
		bReadKeyValA[0] = (wKeyVal>>8)&0xff;
		bReadKeyValA[1] = (wKeyVal)&0xff;
		if (keys)
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
		
	return iRet;
	
}

extern int _get_pcba_id (void);

int ntxup_wait_key_esdupg(void) 
{
	int iRet=0;

	//printf("%s: gptNtxHwCfg=%p ,pcb=%d,hw pcb id=%d,pcb flags=0x%x\n",__FUNCTION__,gptNtxHwCfg,gptNtxHwCfg->m_val.bPCB,_get_pcba_id(),gptNtxHwCfg->m_val.bPCB_Flags);
	if(NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bPCB_Flags,0)) {
		// no keymatrix .
		iRet = ntx_gpio_key_is_home_down();
		//printf("%d=ntx_gpio_key_is_home_down()\n",iRet);
	}
	else
	if (16 == gptNtxHwCfg->m_val.bPCB) 
	{
		// E6063X .
		iRet = (ntxup_wait_key(gbEsdUPGKeyVal_E60630,5,20)==0)?1:0;
	}
	else 
	if (18 == gptNtxHwCfg->m_val.bPCB) 
	{
		// E5060X .
		iRet = (ntxup_wait_key(gbEsdUPGKeyVal_E50600,5,20)==0)?1:0;
	}
	else  
	{
		//printf("%s: fail ,pcb=%d, hw_pcb_id =%d\n",__FUNCTION__,gptNtxHwCfg->m_val.bPCB,_get_pcba_id());
		iRet = (ntxup_wait_key(gbEsdUPGKeyVal_default,5,20)==0)?1:0;
	}

	return iRet;
}

int zforce_read (unsigned char *data)
{
	int iChk=0;
	int retry = 0;
	int readCnt;
	do {
		iChk = i2c_read(gbZforceI2C_ChipAddr, 0, 1, data, 2);
		if(0==iChk) {
			readCnt = data[1];
			if (0xEE == data[0]) {
				do {
					iChk = i2c_read(gbZforceI2C_ChipAddr, 0, 1, data, readCnt);
				} while ((0 > iChk) && (100 > retry++));
				printf("zforce read %X %X (%d bytes)\n", data[0], data[1], readCnt);
				return readCnt;
			}
			else
				printf("zforce frame start not found !\n");
		}
		else
			printf("zforce read failed !\n");
	} while ((0 > iChk) && (100 > retry++));
	return -1;
}
#if 1
#define DEFAULT_PANEL_W		800
#define DEFAULT_PANEL_H		600
#else
#define DEFAULT_PANEL_W		1440
#define DEFAULT_PANEL_H		1080
#endif
static uint8_t cmd_Resolution_v2[] = {0x05, 0x02, (DEFAULT_PANEL_H&0xFF), (DEFAULT_PANEL_H>>8), (DEFAULT_PANEL_W&0xFF), (DEFAULT_PANEL_W>>8)};
static const uint8_t cmd_TouchData_v2[] = { 0x01, 0x04};
static const uint8_t cmd_Frequency_v2[] = { 0x07, 0x08, 10, 00, 100, 00, 100, 00};
static const uint8_t cmd_Active_v2[] = { 0x01, 0x01};
static const uint8_t cmd_Dual_touch_v2[] = { 0x05,0x03,1,0,0,0};
extern int _power_key_status (void);

int ntxup_wait_touch_recovery(void) 
{
	int iRet=0;
	unsigned char data[128];
	int retry;
	unsigned int reg;

#if 0
	// touch_rst high
	mxc_request_iomux(MX50_PIN_SD3_D6,
				IOMUX_CONFIG_ALT1);
	{
			// set output .
			reg = readl(GPIO5_BASE_ADDR + 0x4);
			reg |= (0x1 << 26);
			writel(reg, GPIO5_BASE_ADDR + 0x4);
			
			// output high .
			reg = readl(GPIO5_BASE_ADDR + 0x0);
			reg |= (0x1 << 26);
			writel(reg, GPIO5_BASE_ADDR + 0x0);		
	}
#endif
	if (41 == gptNtxHwCfg->m_val.bPCB){
		// IR_PWEN
		mxc_request_iomux(MX50_PIN_CSPI_MOSI,
					IOMUX_CONFIG_ALT1);
			// set output .
			reg = readl(GPIO4_BASE_ADDR + 0x4);
			reg |= (0x1 << 9);
			writel(reg, GPIO4_BASE_ADDR + 0x4);
			
			// output high .
			reg = readl(GPIO4_BASE_ADDR + 0x0);
			reg |= (0x1 << 9);
			writel(reg, GPIO4_BASE_ADDR + 0x0);		

		// IR_RST
		mxc_request_iomux(MX50_PIN_SD2_D4,
					IOMUX_CONFIG_ALT1);
			// set output .
			reg = readl(GPIO5_BASE_ADDR + 0x4);
			reg |= (0x1 << 12);
			writel(reg, GPIO5_BASE_ADDR + 0x4);
			
			// output high .
			reg = readl(GPIO5_BASE_ADDR + 0x0);
			reg |= (0x1 << 12);
			writel(reg, GPIO5_BASE_ADDR + 0x0);		
	}
	i2c_set_bus_num (0);
	zforce_read (data);
	if (7 == data[0]) {
		i2c_write (gbZforceI2C_ChipAddr,0xEE,1,cmd_Active_v2, sizeof(cmd_Active_v2));
		zforce_read (data);
		i2c_write (gbZforceI2C_ChipAddr,0xEE,1,cmd_Resolution_v2,sizeof(cmd_Resolution_v2));
		zforce_read (data);
		i2c_write (gbZforceI2C_ChipAddr,0xEE,1,cmd_Frequency_v2,sizeof(cmd_Frequency_v2));
		zforce_read (data);
		i2c_write (gbZforceI2C_ChipAddr,0xEE,1,cmd_Dual_touch_v2,sizeof(cmd_Dual_touch_v2));
		zforce_read (data);
		i2c_write (gbZforceI2C_ChipAddr,0xEE,1,cmd_TouchData_v2,sizeof(cmd_TouchData_v2));
		zforce_read (data);
		for (retry=0;retry<100;retry++) {
			int dataCnt;
			dataCnt=zforce_read (data);
			if (0 <= iRet && 4 == data[0]) {
				if (2 == data[1]) {
					int x,y;
					int flag=0;
					printf ("Got dual finger touched!!\n");
					x = (data[3]<<8) | data[2];
					y = (data[5]<<8) | data[4];
					if (60 > x && (DEFAULT_PANEL_W-60) < y)
						flag |= 1;
					if ((DEFAULT_PANEL_H-60) < x && (DEFAULT_PANEL_W-60) < y)
						flag |= 2;
					if (60 > x && 60 > y)
						flag |= 0x10;
					if ((DEFAULT_PANEL_H-60) < x && 60 > y)
						flag |= 0x20;
					x = (data[12]<<8) | data[11];
					y = (data[14]<<8) | data[13];
					if (60 > x && (DEFAULT_PANEL_W-60) < y)
						flag |= 1;
					if ((DEFAULT_PANEL_H-60) < x && (DEFAULT_PANEL_W-60) < y)
						flag |= 2;
					if (60 > x && 60 > y)
						flag |= 0x10;
					if ((DEFAULT_PANEL_H-60) < x && 60 > y)
						flag |= 0x20;
					if (3 == flag) {
						E60612_led_R (1);
						printf ("Entering recovery mode !!\n");
						iRet = 1;
						break;
					}
					else if (0x30 == flag) {
						E60612_led_R (1);
						printf ("Entering external sd boot mode !!\n");
						iRet = 2;
						break;
					}
					else {
						printf ("Wrong points!\n");
					}
				}
				else {
//					printf ("[%02X %02X %02X %02X %02X %02X %02X %02X %02X]\n",data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9]);
					printf ("Wrong point!\n");
				}
			}
			if (!_power_key_status()) {
				printf ("Power key released!!\n");
				break;
			}
		}
	}
	else {
		printf("Boot complete not received !!\n");
	}
	i2c_set_bus_num (2);
	return iRet;
}

static const uint8_t FL100[] = {0x01, 0x8F, 0xFF, 0xFF, 0x01, 0x90, 0x01};

void frontLightCtrl(void){
	if(NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bFrontLight_Flags,0)){
		unsigned int reg;

		printf("turning on FL\n");
	
		i2c_write(gbMicroPI2C_ChipAddr, 0xA7, 1, &FL100[0], 1);
		i2c_write(gbMicroPI2C_ChipAddr, 0xA6, 1, &FL100[1], 1);
     
		i2c_write(gbMicroPI2C_ChipAddr, 0xA1, 1, &FL100[2], 1);
		i2c_write(gbMicroPI2C_ChipAddr, 0xA2, 1, &FL100[3], 1);
		i2c_write(gbMicroPI2C_ChipAddr, 0xA5, 1, &FL100[4], 1);
		i2c_write(gbMicroPI2C_ChipAddr, 0xA4, 1, &FL100[5], 1);
		i2c_write(gbMicroPI2C_ChipAddr, 0xA3, 1, &FL100[6], 1);

		mxc_request_iomux(MX50_PIN_EPDC_VCOM1,IOMUX_CONFIG_ALT1);
		/* FL_R_EN set as output */
		reg = readl(GPIO4_BASE_ADDR + 0x4);
		reg |= (1 << 22);
		writel(reg, GPIO4_BASE_ADDR + 0x4);

		/* FL_R_EN set high */
		reg = readl(GPIO4_BASE_ADDR + 0x0);
		reg |= ( 1 << 22);
		writel(reg, GPIO4_BASE_ADDR + 0x0);

		mxc_request_iomux(MX50_PIN_ECSPI1_MISO,IOMUX_CONFIG_ALT1);
		/* FL_EN set as output */
		reg = readl(GPIO4_BASE_ADDR + 0x4);
		reg |= (1 << 14);
		writel(reg, GPIO4_BASE_ADDR + 0x4);

		/* FL_EN set low */
		reg = readl(GPIO4_BASE_ADDR + 0x0);
		reg &= (~(1 << 14));
		writel(reg, GPIO4_BASE_ADDR + 0x0);
	
	}
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
		ntxup_wait_key_esdupg();
	}
	else
	if(0==strcmp(argv[1],"waittouch")) {
		ntxup_wait_touch_recovery();
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
	"ntxup waittouch [interval (ms)] [timeout (ms)]"
		" - polling touch each interval and wait until touch pressed .\n"
);


#define PMIC_TPS65185	1
#ifdef PMIC_TPS65185 //[

#define TPS65185_RET_SUCCESS				(0)
#define TPS65185_RET_I2CTRANS_ERR 	(-1)

static const unsigned char gbI2C_ADDR_TPS65185 = 0x68; 
static unsigned char gbTPS65185_version;

typedef struct tagTPS65185_VERSIONS{
	unsigned char bMajor;
	unsigned char bMinor;
	unsigned char bVersion;
	unsigned char bRevID;
} TPS65185_VERSIONS;


static int tps65185_get_reg(unsigned char bRegAddr,unsigned char  *O_pbRegVal)
{
	int iRet=TPS65185_RET_SUCCESS;
	int iChk;
	unsigned char bA[1] ;

	char cCmdA[256];
	int iCur_I2C_Chn=(int)(gdwBusNum+1);
	int iTPS65185_I2C_Chn = GET_I2C_CHN_TPS65185();

	//ASSERT(O_pbRegVal);
	//
	if( iCur_I2C_Chn != iTPS65185_I2C_Chn ) {
		sprintf(cCmdA,"i2c dev %d",(iTPS65185_I2C_Chn-1));
		run_command(cCmdA,0);
		//printf(cCmdA);printf("\n");
		udelay(10*1000);
	}


	bA[0]=bRegAddr;
	
	iChk = i2c_read(gbI2C_ADDR_TPS65185, (int)bRegAddr, 1,bA , 1);
	if (iChk != 0) {
		//ERR_MSG("%s(%d):%s i2c_master_send fail !\n",__FILE__,__LINE__,__FUNCTION__);
		iRet = TPS65185_RET_I2CTRANS_ERR;
		goto exit;
	}

	if(O_pbRegVal) {
		*O_pbRegVal = bA[0];
	}

exit:

	if( iCur_I2C_Chn != iTPS65185_I2C_Chn ) {
		sprintf(cCmdA,"i2c dev %d",(iCur_I2C_Chn-1));
		run_command(cCmdA,0);
		//printf(cCmdA);printf("\n");
		udelay(10*1000);
	}

	return iRet;
}

static int tps65185_get_versions(TPS65185_VERSIONS *O_pt65185ver)
{
	int iRet=TPS65185_RET_SUCCESS;
	int iChk;
	unsigned short wReg;
	unsigned char bReg;

	//ASSERT(O_pt65185ver);

	iChk = tps65185_get_reg(0x10,&bReg);
	if(iChk<0) {
		//printf("%d=tps65185_get_reg() fail !\n",iChk);
	}
	else {
		gbTPS65185_version = bReg;
	}


	iRet = iChk;
	if(O_pt65185ver&&iChk>=0) {
		O_pt65185ver->bMajor = (bReg>>6)&0x3;
		O_pt65185ver->bMinor  = (bReg>>4)&0x3;
		O_pt65185ver->bVersion = (bReg)&0xf;
		O_pt65185ver->bRevID = bReg;
	}

	return iRet;
}

static int do_get_epdpmic_ver(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	TPS65185_VERSIONS tTPS65185_ver;

	iRet = tps65185_get_versions(&tTPS65185_ver);
	if(iRet<0) {
		printf("cannnot get EPD PMIC version (%d)!!\n",iRet);
	}
	else {
		printf("EPD PMIC version=0x%x\n",tTPS65185_ver.bRevID);
	}

	return iRet;
}


U_BOOT_CMD(get_epdpmic_ver, 2, 0, do_get_epdpmic_ver,
	"get_epdpmic_ver - get EPD PMIC version \n",
	"get_epdpmic_ver "
		" - get EPD PMIC version .\n"
);



#endif //] PMIC_TPS65185




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
	int iCurrentHallSensorState;
	
	int iChk;

	iCurrentHallSensorState = _hallsensor_status();
	if(iCurrentHallSensorState!=giCurrentHallSensorState) {
		printf("fail: [S17]\n\r");
		iKeyPressedCnt++;
		giCurrentHallSensorState = iCurrentHallSensorState;
	}
	
	
	if(NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bPCB_Flags,0)) 
	{
		extern GPIO_KEY_BTN * ntx_gpio_keysA[] ;
		extern int gi_ntx_gpio_keys;
		extern int gpio_key_btn_is_down(GPIO_KEY_BTN *I_pt_gpio_key_btn);
		
		for(i=0;i<gi_ntx_gpio_keys;i++) {
			iChk = gpio_key_btn_is_down(ntx_gpio_keysA[i]);
			if(1==iChk) {
				// key down .
				printf("fail: %s\n\r",ntx_gpio_keysA[i]->pszReportName);
				iKeyPressedCnt++;
			}
			else if(0==iChk) {
				// key up .
			}
			else {
				// error !! .
			}
		}
		
	}
	else {
		mxc_kpp_init();
		udelay (1000);
		keys = mxc_kpp_getc(&key_info);
		for (i = 0; i < keys; i++) {
			if (key_info[i].evt == KDepress) {
				printf("fail: %s\n\r",gszKeyStringA[key_info[i].val]);
				iKeyPressedCnt++;
			}
		}
		free(key_info);
	}

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
	
#if 0
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
#else
	if(_sd_cd_status()) {
		printf("\rSD in :disabled\n\r");
	}
	else {
		printf("\rSD out :enabled\n\r");
	}
#endif
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
	
	mmc = find_mmc_device(_get_sd_number());
	
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

static int do_hallsensor(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int iRet = 0;
	int iHallSensorState,iWaitState;

	if(argc>1&&argc<=3) {
		if(0==strcmp(argv[1],"wait")) {
			if(3==argc) {
				iWaitState = (argv[2][0]=='0')?0:1;
			}
			else {
				iHallSensorState = _hallsensor_status();
				iWaitState = iHallSensorState?0:1;
			}
			
			do {
				iHallSensorState = _hallsensor_status();
			} while(iHallSensorState!=iWaitState);
			printf("%d\n",iHallSensorState);
		}
	}
	else {
		printf("%d\n",_hallsensor_status());
	}
	return iRet;
}

U_BOOT_CMD(hallsensor, 3, 0, do_hallsensor,
	"hallsensor - get hallsensor state .\n",
	"hallsensor wait [waitstate] .\n"
		" - get the hallsensor state until hallsensor state = [waitstate] (block mode) .\n"
	"hallsensor wait .\n"
		" - get the hallsensor state until hallsensor state changed (block mode) .\n"
	"hallsensor"
		" - get the hallsensor state (nonblock mode) .\n"
);

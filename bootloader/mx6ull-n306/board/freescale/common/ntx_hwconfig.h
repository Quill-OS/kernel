#ifndef __ntx_hwconfig_h//[
#define __ntx_hwconfig_h


#ifdef __cplusplus //[
extern "C" {
#endif //] __cplusplus


#define NTX_HWCFG_MAGIC				"HW CONFIG "
#define NTX_HWCFG_VER					"v2.8"

#define SYSHWCONFIG_SEEKSIZE	(1024*512)


typedef struct tagNTXHWCFG_HDR {
	char cMagicNameA[10];// should be "HW CONFIG "
	char cVersionNameA[5];// should be "vx.x"
	unsigned char bHWConfigSize;// v0.1=19 .
} NTXHWCFG_HDR ;

typedef struct tagNTXHWCFG_VAL { 
	unsigned char bPCB;// 
	unsigned char bKeyPad;// key pad type .
	unsigned char bAudioCodec;//
	unsigned char bAudioAmp;//
	unsigned char bWifi;//
	unsigned char bBT;//
	unsigned char bMobile;//
	unsigned char bTouchCtrl;//touch controller .
	unsigned char bTouchType;//touch type .
	unsigned char bDisplayCtrl;//
	unsigned char bDisplayPanel;//
	unsigned char bRSensor;//
	unsigned char bMicroP;//
	unsigned char bCustomer;//
	unsigned char bBattery;//
	unsigned char bLed;//
	unsigned char bRamSize;// ram size 
	unsigned char bIFlash; // internal flash type .
	unsigned char bExternalMem;// external sd type .
	unsigned char bRootFsType;// root fs type .
	unsigned char bSysPartType;// system partition type .
	unsigned char bProgressXHiByte; // Progress bar position while boot ,X
	unsigned char bProgressXLoByte; //                                  ,X
	unsigned char bProgressYHiByte; // 							,Y
	unsigned char bProgressYLoByte; // 							,Y
	unsigned char bProgressCnts;		// 							Cnt .
	unsigned char bContentType; // Book content type .
	unsigned char bCPU; //main cpu type .
	unsigned char bUIStyle; // UI style .
	unsigned char bRamType; // Ram Type .
	unsigned char bUIConfig; // UI config .
	unsigned char bDisplayResolution;// Display resolution .
	unsigned char bFrontLight;// Front Light .
	unsigned char bCPUFreq;// CPU frequency  .
	unsigned char bHallSensor;// Hall Sensor Controller  .
	unsigned char bDisplayBusWidth;// Display BUS width/bits .
	unsigned char bFrontLight_Flags;// Front Light Flags .
	unsigned char bPCB_Flags;// PCB Flags .
	unsigned char bFrontLight_LED_Driver;// Front Light LED driver .
	unsigned char bVCOM_10mV_HiByte;// VCOM mV High byte .
	unsigned char bVCOM_10mV_LoByte;// VCOM mV Low byte .
	unsigned char bPCB_REV;// PCB revision .
	unsigned char bPCB_LVL;// PCB develop level .
	unsigned char bHOME_LED_PWM;// HOME LED PWM source .
	unsigned char bPMIC;// System PMIC .
	unsigned char bFL_PWM;// FrontLight PWM source
	unsigned char bRTC;// System RTC source
	unsigned char bBootOpt;// Boot option
	unsigned char bTouch2Ctrl;//touch controller .
	unsigned char bTouch2Type;//touch type .
	unsigned char bGPS;//GPS module .
	unsigned char bFM;//FM module .
	unsigned char bRSensor2;//
	unsigned char bLightSensor;//
	unsigned char bTPFWIDByte0;// TP firmware ID byte0 .
	unsigned char bTPFWIDByte1;// TP firmware ID byte1 .
	unsigned char bTPFWIDByte2;// TP firmware ID byte2 .
	unsigned char bTPFWIDByte3;// TP firmware ID byte3 .
	unsigned char bTPFWIDByte4;// TP firmware ID byte4 .
	unsigned char bTPFWIDByte5;// TP firmware ID byte5 .
	unsigned char bTPFWIDByte6;// TP firmware ID byte6 .
	unsigned char bTPFWIDByte7;// TP firmware ID byte7 .
	unsigned char bGPU; //GPU .
	unsigned char bPCB_Flags2;// PCB Flags2 .
	unsigned char bEPD_Flags;// EPD Flags .
} NTXHWCFG_VAL ;

typedef struct tagNTX_HWCONFG{
	NTXHWCFG_HDR m_hdr;
	NTXHWCFG_VAL m_val;
	unsigned char m_bReserveA[110-sizeof(NTXHWCFG_HDR)-sizeof(NTXHWCFG_VAL)];
} NTX_HWCONFIG;

// Filed types ...
#define FIELD_TYPE_IDXSTR	0
#define FIELD_TYPE_BYTE		1
#define FIELD_TYPE_FLAGS		2

// Filed Flags ...
#define FIELD_FLAGS_HW		0x0000
#define FIELD_FLAGS_SW		0x0001


typedef struct tagHwConfigField{
	char *szVersion;
	char *szFieldName ;
	int iFieldValueCnt;
	char **szFieldValueA ;
	unsigned short wFieldType;
	unsigned short wFieldFlags;
} HwConfigField ;


extern const char * gszPCBA[];
extern const char * gszKeyPadA[];
extern const char * gszAudioCodecA[];//
extern const char * gszAudioAmpA[];//
extern const char * gszWifiA[];//
extern const char * gszBTA[];//
extern const char * gszMobileA[];//
extern const char * gszTouchCtrlA[];//touch controller .
extern const char * gszTouchTypeA[];//
extern const char * gszDisplayCtrlA[];
extern const char * gszDisplayPanelA[];
extern const char * gszRSensorA[];//
extern const char * gszMicroPA[];//
extern const char * gszCustomerA[];//
extern const char * gszBatteryA[];//
extern const char * gszLedA[];//
extern const char * gszRamSizeA[];// ram size 
extern const char * gszIFlashA[]; // internal flash type .
extern const char * gszExternalMemA[];// external sd type .
extern const char * gszRootFsTypeA[];// root fs type .
extern const char * gszSysPartTypeA[];// system partition type .
extern const char * gszCPUA[];// cpu type .
extern const char * gszUIStyleA[];// UI Style (netronix UI/customer UI) .
extern const char * gszRAMTypeA[];// Ram Type .
extern const char * gszUIConfigA[];// UI Config .
extern const char * gszDisplayResolutionA[];// Display Resolution .
extern const char * gszFrontLightA[];// Front Light .
extern const char * gszCPUFreqA[];// CPU frequency .
extern const char * gszHallSensorA[];// Hall Sensor .
extern const char * gszDisplayBusWidthA[];// Display BUS width .
extern const char * gszFrontLightLEDrvA[];//Front Light LED driver IC .
extern const char * gszPCB_LVLA[];//PCB develop level .
extern const char * gszHOME_LED_PWMA[];// HOME LED PWM source .
extern const char * gszPMICA[];// System PMIC .
extern const char * gszFL_PWMA[];// FrontLight PWM source
extern const char * gszRTCA[];// System RTC source .
extern const char * gszFMA[];// FM controller .
extern const char * gszGPSA[];// GPS controller .
extern const char * gszLightSensorA[];// Light sensor .
extern const char * gszGPUA[];// GPU .


// the return value of hw config apis . >=0 is success ,others is fail .
#define HWCFG_RET_SUCCESS  (0)
#define HWCFG_RET_PTRERR (-1) // parameter include error pointer
#define HWCFG_RET_HDRNOTMATCH (-2) // hwconfig header magic not match .
#define HWCFG_RET_CFGVERTOOOLD (-3) // hwconfig tool version newer than config struct value (you should update the config file)...
#define HWCFG_RET_CFGVERTOONEW (-4) // hwconfig tool version older than config struct value (you should update the config tool)...
#define HWCFG_RET_CFGVERNOTMATCH (-5) // hwconfig tool version and config struct version not match ...
#define HWCFG_RET_CFGVERFMTERR (-6) // config version format error (must be vX.X) ...
#define HWCFG_RET_NOTHISFIELDIDX (-7) // field index error .
#define HWCFG_RET_NOTHISFIELDNAME (-8) // field name error .
#define HWCFG_RET_FILEOPENFAIL (-9) // file open fail .
#define HWCFG_RET_FILEWRITEFAIL (-10) // file open fail .
#define HWCFG_RET_FIELDTYPEERROR (-11) // field type error .
#define HWCFG_RET_FIELDTRDONLY (-12) // field read only .
#define HWCFG_RET_CFGVALFLAGIDXERROR (-13) // index of config's flags not avalible  .
#define HWCFG_RET_CFGVALFLAGNAMEERROR (-14) // flag name of config not avalible  .
#define HWCFG_RET_FIELDIDXOUTRANGE (-15) // field index out of range  .


// ntx hardward config field index list ...
#define HWCFG_FLDIDX_PCB								0
#define HWCFG_FLDIDX_KeyPad							1
#define HWCFG_FLDIDX_AudioCodec					2
#define HWCFG_FLDIDX_AudioAmp						3
#define HWCFG_FLDIDX_Wifi								4
#define HWCFG_FLDIDX_BT									5
#define HWCFG_FLDIDX_Mobile							6
#define HWCFG_FLDIDX_TouchCtrl					7
#define HWCFG_FLDIDX_TouchType					8
#define HWCFG_FLDIDX_DisplayCtrl				9
#define HWCFG_FLDIDX_DisplayPanel				10
#define HWCFG_FLDIDX_RSensor						11
#define HWCFG_FLDIDX_MicroP							12
#define HWCFG_FLDIDX_Customer						13
#define HWCFG_FLDIDX_Battery						14
#define HWCFG_FLDIDX_Led								15
#define HWCFG_FLDIDX_RamSize						16
#define HWCFG_FLDIDX_IFlash							17
#define HWCFG_FLDIDX_ExternalMem				18
#define HWCFG_FLDIDX_RootFsType					19 // v0.2
#define HWCFG_FLDIDX_SysPartType				20 // v0.3

#define HWCFG_FLDIDX_ProgressXHiByte			21 // v0.4
#define HWCFG_FLDIDX_ProgressXLoByte			22 // v0.4
#define HWCFG_FLDIDX_ProgressYHiByte			23 // v0.4
#define HWCFG_FLDIDX_ProgressYLoByte			24 // v0.4
#define HWCFG_FLDIDX_ProgressCnts				25 // v0.4

#define HWCFG_FLDIDX_ContentType				26 // v0.5
#define HWCFG_FLDIDX_CPU						27 // v0.6
#define HWCFG_FLDIDX_UISTYLE					28 // v0.7

#define HWCFG_FLDIDX_RAMTYPE					29 // v0.8
#define HWCFG_FLDIDX_UICONFIG					30 // v0.9
#define HWCFG_FLDIDX_DISPLAYRESOLUTION			31 // v1.0
#define HWCFG_FLDIDX_FRONTLIGHT			32 // v1.1
#define HWCFG_FLDIDX_CPUFREQ			33 // v1.2
#define HWCFG_FLDIDX_HALLSENSOR			34 // v1.3
#define HWCFG_FLDIDX_DisplayBusWidth			35 // v1.4
#define HWCFG_FLDIDX_FrontLight_Flags			36 // v1.5
#define HWCFG_FLDIDX_PCB_Flags			37 // v1.6
#define HWCFG_FLDIDX_FrontLight_LEDrv			38 // v1.7
#define HWCFG_FLDIDX_VCOM_10mV_HiByte			39 // v1.8
#define HWCFG_FLDIDX_VCOM_10mV_LoByte			40 // v1.8
#define HWCFG_FLDIDX_PCB_REV			41 // v1.9
#define HWCFG_FLDIDX_PCB_LVL			42 // v2.0
#define HWCFG_FLDIDX_HOME_PWM			43 // v2.1
#define HWCFG_FLDIDX_PMIC					44 // v2.1
#define HWCFG_FLDIDX_FL_PWM				45// v2.1
#define HWCFG_FLDIDX_RTC				46// v2.1
#define HWCFG_FLDIDX_BOOTOPT				47// v2.2
#define HWCFG_FLDIDX_Touch2Ctrl					48 // v2.3
#define HWCFG_FLDIDX_Touch2Type					49 // v2.3
#define HWCFG_FLDIDX_GPS					50 // v2.4
#define HWCFG_FLDIDX_FM					51 // v2.4
#define HWCFG_FLDIDX_RSensor2					52 // v2.5
#define HWCFG_FLDIDX_LightSensor					53 // v2.5
#define HWCFG_FLDIDX_TPFWIDByte0					54 // v2.6
#define HWCFG_FLDIDX_TPFWIDByte1					55 // v2.6
#define HWCFG_FLDIDX_TPFWIDByte2					56 // v2.6
#define HWCFG_FLDIDX_TPFWIDByte3					57 // v2.6
#define HWCFG_FLDIDX_TPFWIDByte4					58 // v2.6
#define HWCFG_FLDIDX_TPFWIDByte5					59 // v2.6
#define HWCFG_FLDIDX_TPFWIDByte6					60 // v2.6
#define HWCFG_FLDIDX_TPFWIDByte7					61 // v2.6
#define HWCFG_FLDIDX_GPU									62 // v2.7
#define HWCFG_FLDIDX_PCB_Flags2						63 // v2.8
#define HWCFG_FLDIDX_EPD_Flags						64 // v2.8




NTX_HWCONFIG *NtxHwCfg_Load(const char *szFileName,int iIsSeek);
NTX_HWCONFIG *NtxHwCfg_LoadEx(const char *szFileName,unsigned long dwIsSeek);
int NtxHwCfg_Save(const char *szFileName,int iIsSeek);
NTX_HWCONFIG *NtxHwCfg_Get(void);


int NtxHwCfg_GetTotalFlds(void);//取得共有幾個欄位.
int NtxHwCfg_GetFldVal(int iFldIdx,HwConfigField *O_ptHwCfgFld);//取得己定義的欄位值.
unsigned char NtxHwCfg_FldStrVal2Val(int iFldIdx,char *szFldStrVal);//欄位的字串值轉換為欄位.
const char *NtxHwCfg_FldVal2StrVal(int iFldIdx,unsigned char bFldVal);//欄位的值轉換為字串值.
int NtxHwCfg_FldName2Idx(const char *szFldName);
int NtxHwCfg_is_HW_Fld(int iFldIdx);
int NtxHwCfg_is_SW_Fld(int iFldIdx);

int NtxHwCfg_ChkCfgHeaderEx2(NTX_HWCONFIG *pHdr,int iIsIgnoreVersion,const char *pcCompareHdrVersionA);
int NtxHwCfg_ChkCfgHeaderEx(NTX_HWCONFIG *pHdr,int iIsIgnoreVersion);
int NtxHwCfg_ChkCfgHeader(NTX_HWCONFIG *pHdr);//檢查組態表頭,版本.
int NtxHwCfg_CfgUpgrade(NTX_HWCONFIG *pHdr);//格式升級至工具一致的版本.

int NtxHwCfg_GetCfgTotalFlds(NTX_HWCONFIG *pHdr);//取得組態總共有幾個欄位.

int NtxHwCfg_GetCfgFldVal(NTX_HWCONFIG *pHdr,int iFieldIdx);
const char *NtxHwCfg_GetCfgFldStrVal(NTX_HWCONFIG *pHdr,int iFieldIdx);
int NtxHwCfg_GetCfgFldFlagVal(NTX_HWCONFIG *pHdr,int iFieldIdx,int iFlagsIdx);
int NtxHwCfg_GetCfgFldFlagValByName(NTX_HWCONFIG *pHdr,int iFieldIdx,const char *pszFlagName);

int NtxHwCfg_SetCfgFldVal(NTX_HWCONFIG *pHdr,int iFieldIdx,int iFieldVal);
int NtxHwCfg_SetCfgFldValEx(NTX_HWCONFIG *pHdr,int iFieldIdx,int iFieldVal,int iHW_WR_Protect);
int NtxHwCfg_SetCfgFldStrVal(NTX_HWCONFIG *pHdr,int iFieldIdx,const char *pszFieldStrVal);
int NtxHwCfg_SetCfgFldStrValEx(NTX_HWCONFIG *pHdr,int iFieldIdx,const char *pszFieldStrVal,int iHW_WR_Protect);
int NtxHwCfg_SetCfgFldFlagValByName(NTX_HWCONFIG *pHdr,int iFieldIdx,const char *pszFlagName,int iIsTurnON,int iHW_WR_Protect);

int NtxHwCfg_SetCfgFldValDefs(NTX_HWCONFIG *I_pHdr,int I_iFieldIdx,
		const char **I_pszFieldValDefs,int I_iTotalVals);

int NtxHwCfg_CompareHdrFldVersion(NTX_HWCONFIG *pHdr,int iFieldIdx);

// get field value name in hardware config struct ...
#define NTXHWCFG_GET_FLDCFGNAME(pHdr,_fld)		gsz##_fld##A[pHdr->m_val.b##_fld]
// get print format by field in hardware config struct ...
#define NTXHWCFG_GET_FLDCFGPRTFMT(pHdr,_fld)	"%s : %s",#_fld,NTXHWCFG_GET_FLDCFGNAME(pHdr,_fld)

#define NTXHWCFG_TST_FLAG(_flags,_bit_n)	((_flags)&(0x01<<(_bit_n)))?1:0


#define NTXHWCFG_CHK_HDR(_pHdr,_iIsIgnoreVersion,_pcCompareHdrVersionA) \
({\
	int _iRet ;\
	const char szNtxHwCfgMagic[]=NTX_HWCFG_MAGIC;\
	char *_pcHdrVer=(_pcCompareHdrVersionA);\
	\
	if(!(_pHdr)) {\
		_iRet = HWCFG_RET_PTRERR;\
	}\
	else {\
		if(szNtxHwCfgMagic[0]==(_pHdr)->m_hdr.cMagicNameA[0] &&\
			szNtxHwCfgMagic[1]==(_pHdr)->m_hdr.cMagicNameA[1] &&\
			szNtxHwCfgMagic[2]==(_pHdr)->m_hdr.cMagicNameA[2] &&\
			szNtxHwCfgMagic[3]==(_pHdr)->m_hdr.cMagicNameA[3] &&\
			szNtxHwCfgMagic[4]==(_pHdr)->m_hdr.cMagicNameA[4] &&\
			szNtxHwCfgMagic[5]==(_pHdr)->m_hdr.cMagicNameA[5] &&\
			szNtxHwCfgMagic[6]==(_pHdr)->m_hdr.cMagicNameA[6] &&\
			szNtxHwCfgMagic[7]==(_pHdr)->m_hdr.cMagicNameA[7] &&\
			szNtxHwCfgMagic[8]==(_pHdr)->m_hdr.cMagicNameA[8] &&\
			szNtxHwCfgMagic[9]==(_pHdr)->m_hdr.cMagicNameA[9] )\
		{\
			if((_iIsIgnoreVersion)) {\
				_iRet = HWCFG_RET_SUCCESS;\
			}\
			else if((_pcHdrVer)) {\
				if(_pcHdrVer[0]==(_pHdr)->m_hdr.cVersionNameA[0] &&\
					_pcHdrVer[2]==(_pHdr)->m_hdr.cVersionNameA[2] &&\
					_pcHdrVer[4]==(_pHdr)->m_hdr.cVersionNameA[4] ) \
				{\
					if((_pHdr)->m_hdr.cVersionNameA[1]>_pcHdrVer[1]) {\
						_iRet = HWCFG_RET_CFGVERTOONEW;\
					}\
					else if((_pHdr)->m_hdr.cVersionNameA[1]<_pcHdrVer[1]) {\
						_iRet = HWCFG_RET_CFGVERTOOOLD;\
					}\
					else {\
						if((_pHdr)->m_hdr.cVersionNameA[3]>_pcHdrVer[3]) {\
							_iRet = HWCFG_RET_CFGVERTOONEW;\
						}\
						else if((_pHdr)->m_hdr.cVersionNameA[3]<_pcHdrVer[3]) {\
							_iRet = HWCFG_RET_CFGVERTOOOLD;\
						}\
						else {\
							_iRet = HWCFG_RET_SUCCESS;\
						}\
					}\
				}\
				else {\
					_iRet = HWCFG_RET_CFGVERFMTERR;\
				}\
			}\
			else {\
				_iRet = HWCFG_RET_PTRERR;\
			}\
		}\
		else {\
			_iRet = HWCFG_RET_HDRNOTMATCH;\
		}\
	}\
	_iRet;\
})



#ifdef __cplusplus //[
}
#endif //] __cplusplus

#endif //] __ntx_hwconfig_h


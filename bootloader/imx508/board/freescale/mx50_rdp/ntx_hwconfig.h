#ifndef __ntx_hwconfig_h//[
#define __ntx_hwconfig_h


#ifdef __cplusplus //[
extern "C" {
#endif //] __cplusplus


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
	unsigned char bTouchType;//touch controller .
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
	unsigned char bProgressYHiByte; // 																	,Y
	unsigned char bProgressYLoByte; // 																	,Y
	unsigned char bProgressCnts;		//                                  ,Cnt .
	unsigned char bContentType; // Book content type .
	unsigned char bCPU; //main cpu type .
	unsigned char bUIStyle; //main cpu type .
} NTXHWCFG_VAL ;

typedef struct tagNTX_HWCONFG{
	NTXHWCFG_HDR m_hdr;
	NTXHWCFG_VAL m_val;
} NTX_HWCONFIG;


#define FIELD_TYPE_IDXSTR	0
#define FIELD_TYPE_BYTE		1

typedef struct tagHwConfigField{
	char *szVersion;
	char *szFieldName ;
	int iFieldValueCnt;
	char **szFieldValueA ;
	unsigned short wFieldType;
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
extern const char * gszUIStyleA[];// cpu type .


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


NTX_HWCONFIG *NtxHwCfg_Load(const char *szFileName,int iIsSeek);
int NtxHwCfg_Save(const char *szFileName,int iIsSeek);
NTX_HWCONFIG *NtxHwCfg_Get(void);


int NtxHwCfg_GetTotalFlds(void);//取得共有幾個欄位.
int NtxHwCfg_GetFldVal(int iFldIdx,HwConfigField *O_ptHwCfgFld);//取得己定義的欄位值.
unsigned char NtxHwCfg_FldStrVal2Val(int iFldIdx,char *szFldStrVal);//欄位的字串值轉換為欄位.
const char *NtxHwCfg_FldVal2StrVal(int iFldIdx,unsigned char bFldVal);//欄位的值轉換為字串值.
int NtxHwCfg_FldName2Idx(const char *szFldName);


int NtxHwCfg_ChkCfgHeaderEx(NTX_HWCONFIG *pHdr,int iIsIgnoreVersion);
int NtxHwCfg_ChkCfgHeader(NTX_HWCONFIG *pHdr);//檢查組態表頭,版本.
int NtxHwCfg_CfgUpgrade(NTX_HWCONFIG *pHdr);//格式升級至工具一致的版本.

int NtxHwCfg_GetCfgTotalFlds(NTX_HWCONFIG *pHdr);//取得組態總共有幾個欄位.

int NtxHwCfg_GetCfgFldVal(NTX_HWCONFIG *pHdr,int iFieldIdx);
const char *NtxHwCfg_GetCfgFldStrVal(NTX_HWCONFIG *pHdr,int iFieldIdx);

int NtxHwCfg_SetCfgFldVal(NTX_HWCONFIG *pHdr,int iFieldIdx,int iFieldVal);
int NtxHwCfg_SetCfgFldStrVal(NTX_HWCONFIG *pHdr,int iFieldIdx,const char *pszFieldStrVal);

int NtxHwCfg_CompareHdrFldVersion(NTX_HWCONFIG *pHdr,int iFieldIdx);

// get field value name in hardware config struct ...
#define NTXHWCFG_GET_FLDCFGNAME(pHdr,_fld)		gsz##_fld##A[pHdr->m_val.b##_fld]
// get print format by field in hardware config struct ...
#define NTXHWCFG_GET_FLDCFGPRTFMT(pHdr,_fld)	"%s : %s",#_fld,NTXHWCFG_GET_FLDCFGNAME(pHdr,_fld)


#ifdef __cplusplus //[
}
#endif //] __cplusplus

#endif //] __ntx_hwconfig_h

#ifndef __ntx_firmware_h//[
#define __ntx_firmware_h



#define NTX_FIRMWARE_SIGNATURE_S "{NTXFW-->"
#define NTX_FIRMWARE_SIGNATURE_E "<--NTXFW}"

#define NTX_FIRMWARE_SIGNATURE_SIZE		10
#define NTX_FIRMWARE_VERSION_MAJOR		0
#define NTX_FIRMWARE_VERSION_SUB			0
#define NTX_FIRMWARE_VERSION_MINOR		0
#define NTX_FIRMWARE_VERSION_MINI			0

#define NTX_FIRMWARE_NAME_LEN				32

typedef struct tagNTX_FIRMWARE_HDR{
	char szSignatureS[NTX_FIRMWARE_SIGNATURE_SIZE]; // 
	char szFirmwareName[NTX_FIRMWARE_NAME_LEN+4];//

	unsigned short wFirmwareItems; //

	unsigned char bVersionMajor; // 
	unsigned char bVersionSub; // 
	unsigned char bVersionMinor; // 
	unsigned char bVersionMini; // 

} NTX_FIRMWARE_HDR;




//
//
// Firmware Items ....
//
//
#define NTX_FW_ITEM_MAGIC_SIZE 8
#define NTX_FW_ITEM_MAGIC	{0xff,0xfc,0x5c,0xc5,0x5c,0xc5,0xcf,0xff}

//
// FW_TYPE definitions list .
//
#define NTX_FW_TYPE_LM3630_FLPERCTTAB						0// frontlight percentage table for lm3630 .
#define NTX_FW_TYPE_LM3630_FLCURTABLE						1// frontlight current table for lm3630/ricoh .
#define NTX_FW_TYPE_LM3630_RGBW_CURTAB_HDR					2// frontlight current table header for lm3630/ricoh for each combinations on 4colors(R/G/B/W) models .
#define NTX_FW_TYPE_LM3630_RGBW_CURTAB					3// frontlight current table for lm3630/ricoh for each combinations on 4colors(R/G/B/W) models .
#define NTX_FW_TYPE_LM3630_MIX2COLOR11_CURTAB		4// frontlight current table for lm3630/ricoh for each combinations on 2colors mix to 11 colors 100 percent current tables .
#define NTX_FW_TYPE_FLPERCETCURTABLE						5// frontlight 100% current table for ricoh .
#define NTX_FW_TYPE_LM3630_DUALFL_HDR					6// frontlight table header for lm3630 for each 100% table which combine 2colors .
#define NTX_FW_TYPE_LM3630_DUALFL_PERCENTTAB		7// frontlight percent table for lm3630 for each combinations on 2colors models .

typedef struct tagNTX_FIRMWARE_ITEM_HDR{
	unsigned char bMagicA[NTX_FW_ITEM_MAGIC_SIZE];
	unsigned long dw12345678;
	unsigned short wFirmwareType,wReserved;// 
	unsigned long dwFirmwareSize;// 
	char szFirmwareName[NTX_FIRMWARE_NAME_LEN+4];//
} NTX_FIRMWARE_ITEM_HDR;


typedef struct tagNTX_FIRMWARE {
	NTX_FIRMWARE_HDR tHdr;
	NTX_FIRMWARE_ITEM_HDR tFirstFirmware;
} NTX_FIRMWARE;



/////////////////////////////////////////////////////////////////
//
// firmware descriptions for type NTX_FW_TYPE_LM3630_FLPERCTTAB
//

#define NTX_FW_FL_COLOR_WHITE			0
#define NTX_FW_FL_COLOR_RED				1
#define NTX_FW_FL_COLOR_BLUE			2
#define NTX_FW_FL_COLOR_GREEN			3


typedef struct tagNTX_FW_LM3630FL_percent_tab {
	unsigned char bColor;// which color for this percentage table .
	unsigned char bDefaultCurrent; // default current when led turning on for lm3630a .
	unsigned char bDefaultFreq; // default frequency for lm3630a .
	unsigned char bDefaultFlags; // default flags to control the behavior of lm3630a .
	unsigned char bPercentBrightnessA[100];// percent-brightness (1-100) table .
	unsigned char bPercentCurrentA[100];// percent-current (1-100) table .
} NTX_FW_LM3630FL_percent_tab; // percentage setting table .


/////////////////////////////////////////////////////////////////
//
// firmware descriptions for type NTX_FW_TYPE_LM3630_FLCURTABLE
//
typedef struct tagNTX_FW_LM3630FL_current_tab {
	unsigned char bColor,bReservedA[3];
	unsigned long dwCurrentA[255]; // current from 1-255 .
} NTX_FW_LM3630FL_current_tab; // current usage table for each brightness.
/////////////////////////////////////////////////////////////////
//
// firmware descriptions for type NTX_FW_TYPE_LM3630_RGBW_CURTAB
//
typedef struct tagNTX_FW_LM3630FL_RGBW_current_item {
	unsigned long dwCurrent;
	unsigned char bPercent;
	unsigned char bRed,bGreen,bBlue,bWhite ;
	unsigned char bReservedA[3];
}NTX_FW_LM3630FL_RGBW_current_item;

// firmware descriptions for type NTX_FW_TYPE_LM3630_RGBW_CURTAB_HDR
typedef struct tagNTX_FW_LM3630FL_RGBW_current_tab {
	unsigned long dwTotalItems;
	//NTX_FW_LM3630FL_RGBW_current_item *ptRGBW_current_tab;
}NTX_FW_LM3630FL_RGBW_current_tab_hdr;

/////////////////////////////////////////////////////////////////
//
// firmware descriptions for type NTX_FW_TYPE_LM3630_MIX2COLOR11_CURTAB
//
typedef struct tagNTX_FW_LM3630FL_MIX2COLOR11_current_tab {
	unsigned long dwCurrentA[11][100];
}NTX_FW_LM3630FL_MIX2COLOR11_current_tab;

/////////////////////////////////////////////////////////////////
//
// firmware descriptions for type NTX_FW_TYPE_FLPERCETCURTABLE
//
typedef struct tagNTX_FW_percent_current_tab {
	unsigned char bColor,bReservedA[3];
	unsigned long dwCurrentA[100]; // current from 1-100 .
} NTX_FW_percent_current_tab; // current usage table for each brightness.

/////////////////////////////////////////////////////////////////
//
// firmware descriptions for type NTX_FW_TYPE_LM3630_DUALFL_PERCENTTAB
//

typedef struct tagNTX_FW_LM3630FL_dualcolor_percent_tab {
	char szColorName[32]; // 
	unsigned char bDefaultC1_Current; // default power/current on ch1 FL of lm3630a for this color .
	unsigned char bDefaultC2_Current; // default power/current on ch2 FL of lm3630a for this color .
	unsigned char bDefaultFreq; // default frequency of FL for this color .
	unsigned char bDefaultFlags; // default flags about lm3630a for this color .
	unsigned char bC1_BrightnessA[100]; // brightness percentage table of Ch1 .
	unsigned char bC1_CurrentA[100]; // power/current percentage table of Ch1 .
	unsigned char bC2_BrightnessA[100]; // brightness percentage table of Ch2 .
	unsigned char bC2_CurrentA[100]; // power/current percentage table of Ch2 .
	unsigned long dwCurrentA[100]; // current table in each combination .
	unsigned char bReservedA[8];
}NTX_FW_LM3630FL_dualcolor_percent_tab;

// firmware descriptions for type NTX_FW_TYPE_LM3630_DUALFL_HDR
typedef struct tagNTX_FW_LM3630FL_dualcolor_hdr {
	unsigned long dwTotalColors; // total color temperatures .
	unsigned char bDefaultC1_Current; // default power/current on ch1 FL of lm3630a .
	unsigned char bDefaultC2_Current; // default power/current on ch2 FL of lm3630a .
	unsigned char bDefaultFreq; // default frequency of FL .
	unsigned char bDefaultFlags; // default flags about lm3630a .
	unsigned char bReservedA[8]; // reserved .
}NTX_FW_LM3630FL_dualcolor_hdr;


#endif //]__ntx_firmware_h


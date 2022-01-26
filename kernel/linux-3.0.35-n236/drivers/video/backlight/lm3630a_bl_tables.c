#include <linux/kernel.h>
#include "lm3630a_bl_tables.h"
#include "../../../arch/arm/mach-mx6/ntx_firmware.h"

typedef struct tagLedPercentItem {
	int iTableIdx;// you can use FrontLight field in hwconfig .
	unsigned char *pbPercentDutyTable; // percentage table for duty controll .
	int iTableSize;
} LedPercent; //

typedef struct tagLedPowerType {
	int iTableIdx;// you can use FrontLight field in hwconfig .
	unsigned char bPower;
} LedPowerType ;


#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

static const unsigned char gbLm3630_FL_W_percent_dutyA[] = {
  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, //  1- 10
 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, // 11- 20
 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, // 21- 30
 62, 65, 67, 69, 71, 73, 75, 77, 79, 81, // 31- 40
 83, 85, 87, 89, 91, 93, 95, 97, 99,101, // 41- 50
103,105,107,109,111,113,115,117,119,121, // 51- 60
123,125,127,129,131,133,135,137,139,141, // 61- 70
143,145,147,149,151,153,155,157,159,161, // 71- 80
163,165,167,169,171,173,175,177,179,181, // 81- 90
183,185,187,190,195,200,205,210,215,221, // 91-100
}; // FL TABLE0+ : E70Q02 .

static const unsigned char gbLm3630_FL_percent_table1[] = {
 31, 42, 50, 56, 58, 60, 62, 64, 66, 68, //  1- 10
 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, // 11- 20
 90, 92, 94, 96, 98,100,102,104,106,108, // 21- 30
110,112,114,116,118,120,122,124,126,128, // 31- 40
130,132,134,136,138,140,142,144,146,148, // 41- 50
150,152,154,156,158,160,162,164,166,168, // 51- 60
170,172,174,176,178,180,182,184,186,188, // 61- 70
190,192,194,196,198,200,202,204,206,208, // 71- 80
210,212,214,216,218,220,222,224,226,228, // 81- 90
230,232,234,236,238,240,242,244,246,248, // 91-100
}; // FL TABLE1+ : E60QL2 .


static LedPercent lm3630a_FL_W_duty_table[] = {
	{1 /* TABLE0 */,0,0},
	{2 /* TABLE0+ */,gbLm3630_FL_W_percent_dutyA,ARRAY_SIZE(gbLm3630_FL_W_percent_dutyA)},
	{4 /* TABLE1+ */,gbLm3630_FL_percent_table1,ARRAY_SIZE(gbLm3630_FL_percent_table1)},
};

static LedPowerType lm3630a_FL_power_table[] = {
	{1/* TABLE0 */,31,},
	{2/* TABLE0+ */,31,},
	{4/* TABLE1+ */,5,},
};


unsigned char *lm3630a_get_FL_W_duty_table(int iFL_table_idx,int *O_piTableSize) 
{
	unsigned char *pbFL_duty_table=0;
	int i;

	for(i=0;i<ARRAY_SIZE(lm3630a_FL_W_duty_table);i++)
	{
		if(iFL_table_idx==lm3630a_FL_W_duty_table[i].iTableIdx) {
			pbFL_duty_table = lm3630a_FL_W_duty_table[i].pbPercentDutyTable;
			if(O_piTableSize) {
				*O_piTableSize = lm3630a_FL_W_duty_table[i].iTableSize;
			}
			break;
		}
	}
	return pbFL_duty_table;
}


int lm3630a_get_default_power_by_table(int iFL_table_idx,unsigned char *O_pbPower)
{
	int iRet = 0;
	int i;

	for(i=0;i<ARRAY_SIZE(lm3630a_FL_power_table);i++)
	{
		if(iFL_table_idx==lm3630a_FL_power_table[i].iTableIdx) {
			if(O_pbPower) {
				*O_pbPower = lm3630a_FL_power_table[i].bPower;
			}
			break;
		}
	}

	if(i>=ARRAY_SIZE(lm3630a_FL_power_table)) {
		// cannot find such table .
		iRet = -1;
	}

	return iRet;
}

int lm3630a_set_FL_W_duty_table(int iFL_table_idx,int I_iTableSize,unsigned char *I_pbFL_DutyTab)
{
	int iRet=0;
	int i;

	for(i=0;i<ARRAY_SIZE(lm3630a_FL_W_duty_table);i++)
	{
		if(iFL_table_idx==lm3630a_FL_W_duty_table[i].iTableIdx) {
			lm3630a_FL_W_duty_table[i].pbPercentDutyTable = I_pbFL_DutyTab;
			lm3630a_FL_W_duty_table[i].iTableSize = I_iTableSize;
			break;
		}
	}

	if(i>=ARRAY_SIZE(lm3630a_FL_W_duty_table)) {
		iRet=-1;
	}

	return iRet;
}

int lm3630a_set_default_power_by_table(int iFL_table_idx,unsigned char I_bPower)
{
	int iRet = 0;
	int i;

	for(i=0;i<ARRAY_SIZE(lm3630a_FL_power_table);i++)
	{
		if(iFL_table_idx==lm3630a_FL_power_table[i].iTableIdx) {
			lm3630a_FL_power_table[i].bPower = I_bPower;
			break;
		}
	}

	if(i>=ARRAY_SIZE(lm3630a_FL_power_table)) {
		// cannot find such table .
		iRet = -1;
	}

	return iRet;
}



#define LM3630_CHIPS_MAX	2
#define LM3630_CHANELS_MAX	2

static unsigned long *gpdwRicohCurrTabA[LM3630_CHIPS_MAX][LM3630_CHANELS_MAX];
static unsigned long gdwRicohCurrTabSizeA[LM3630_CHIPS_MAX][LM3630_CHANELS_MAX];

int lm3630a_set_FL_RicohCurrTab(int I_iChipIdx,int I_iChnIdx,
		unsigned long *I_pdwRicohCurrTabA,unsigned long I_dwRicohCurrTabSize)
{
	int iRet=0;

	if(I_iChipIdx<0) {
		return -1;
	}
	if(I_iChipIdx>=LM3630_CHIPS_MAX) {
		return -2;
	}
	if(I_iChnIdx<0) {
		return -3;
	}
	if(I_iChnIdx>=LM3630_CHANELS_MAX) {
		return -4;
	}

	gpdwRicohCurrTabA[I_iChipIdx][I_iChnIdx] = I_pdwRicohCurrTabA;
	gdwRicohCurrTabSizeA[I_iChipIdx][I_iChnIdx] = I_dwRicohCurrTabSize;

	return iRet;
}

int lm3630a_get_FL_RicohCurrTab(int I_iChipIdx,int I_iChnIdx,
		unsigned long **O_ppdwRicohCurrTabA,unsigned long *O_pdwRicohCurrTabSize)
{
	int iRet=0;
	if(I_iChipIdx<0) {
		return -1;
	}
	if(I_iChipIdx>=LM3630_CHIPS_MAX) {
		return -2;
	}
	if(I_iChnIdx<0) {
		return -3;
	}
	if(I_iChnIdx>=LM3630_CHANELS_MAX) {
		return -4;
	}

	if(O_ppdwRicohCurrTabA) {
		*O_ppdwRicohCurrTabA = gpdwRicohCurrTabA[I_iChipIdx][I_iChnIdx];
	}

	if(O_pdwRicohCurrTabSize) {
		*O_pdwRicohCurrTabSize = gdwRicohCurrTabSizeA[I_iChipIdx][I_iChnIdx];
	}

	return iRet;
}


static NTX_FW_LM3630FL_MIX2COLOR11_current_tab *gptLm3630fl_Mix2Color11_curr_tab;

int lm3630a_set_FL_Mix2color11_RicohCurrTab(void *I_pvLm3630fl_Mix2Color11_curr_tab)
{
	gptLm3630fl_Mix2Color11_curr_tab = (NTX_FW_LM3630FL_MIX2COLOR11_current_tab *)I_pvLm3630fl_Mix2Color11_curr_tab;
	return 0;
}

void *lm3630a_get_FL_Mix2color11_RicohCurrTab(void)
{
	return (void *)gptLm3630fl_Mix2Color11_curr_tab;
}



static NTX_FW_LM3630FL_RGBW_current_item *gptLm3630fl_RGBW_RicohCurrTab;
static unsigned long gdwLm3630fl_RGBW_RicohCurrTabItems;

int lm3630a_set_FL_RGBW_RicohCurrTab(unsigned long I_dwRGBW_curr_items,void *I_pvLm3630fl_RGBW_curr_tab)
{
	gdwLm3630fl_RGBW_RicohCurrTabItems = I_dwRGBW_curr_items;
	gptLm3630fl_RGBW_RicohCurrTab = (NTX_FW_LM3630FL_RGBW_current_item *)I_pvLm3630fl_RGBW_curr_tab;

#if 0
	printk("%s() : RGBW %d items table @ %p\n",__FUNCTION__,
			(int)gdwLm3630fl_RGBW_RicohCurrTabItems,gptLm3630fl_RGBW_RicohCurrTab);
#endif

	return 0;
}


int lm3630a_get_FL_RGBW_RicohCurrTab(unsigned long *O_pdwRGBW_curr_items,void **O_pvLm3630fl_RGBW_curr_tab)
{
	if(O_pdwRGBW_curr_items) {
		*O_pdwRGBW_curr_items = gdwLm3630fl_RGBW_RicohCurrTabItems;
	}
	if(O_pvLm3630fl_RGBW_curr_tab) {
		*O_pvLm3630fl_RGBW_curr_tab = (void *)gptLm3630fl_RGBW_RicohCurrTab;
	}
	return 0;
}



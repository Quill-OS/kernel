#ifndef _lm3630a_bl_tables_h //[
#define _lm3630a_bl_tables_h



unsigned char *lm3630a_get_FL_W_duty_table(int iFL_table_idx,int *O_piTableSize);
int lm3630a_set_FL_W_duty_table(int iFL_table_idx,int I_iTableSize,unsigned char *I_pbFL_DutyTab);
int lm3630a_get_default_power_by_table(int iFL_table_idx,unsigned char *O_pbPower);
int lm3630a_set_default_power_by_table(int iFL_table_idx,unsigned char I_bPower);

int lm3630a_set_FL_RicohCurrTab(int I_iChipIdx,int I_iChnIdx,
		unsigned long *I_pdwRicohCurrTabA,unsigned long I_dwRicohCurrTabSize);
int lm3630a_get_FL_RicohCurrTab(int I_iChipIdx,int I_iChnIdx,
		unsigned long **O_ppdwRicohCurrTabA,unsigned long *O_pdwRicohCurrTabSize);

int lm3630a_set_FL_Mix2color11_RicohCurrTab(void *I_pvLm3630fl_Mix2Color11_curr_tab);
void *lm3630a_get_FL_Mix2color11_RicohCurrTab(void);

int lm3630a_set_FL_RGBW_RicohCurrTab(unsigned long I_dwRGBW_curr_items,void *I_pvLm3630fl_RGBW_curr_tab);
int lm3630a_get_FL_RGBW_RicohCurrTab(unsigned long *O_pdwRGBW_curr_items,void **O_pvLm3630fl_RGBW_curr_tab);


#endif //]_lm3630a_bl_tables_h



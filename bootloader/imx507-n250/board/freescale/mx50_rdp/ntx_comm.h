#ifndef _ntx_comm_h//[
#define _ntx_comm_h

#ifdef __cplusplus //[
extern C {
#endif //] __cplusplus



#ifndef _GPIO_KEY_BTN//[
#define _GPIO_KEY_BTN

typedef struct tagGPIO_KEY_BTN{
	iomux_pin_name_t PIN; // pin name .
	iomux_pin_cfg_t PIN_CFG; // pin config .
	u32	PIN_PAD_CFG; // pad config .
	u32 GPIO_Grp; //  gpio group .
	u32	GPIO_Num; // gpio number .
	int DownValue; // key down value .
	int iIsInited;
	const char *pszReportName;
}GPIO_KEY_BTN;

#endif //]_GPIO_KEY_BTN

#ifndef _GPIO_OUT//[
#define _GPIO_OUT

typedef struct tagGPIO_OUT{
	iomux_pin_name_t PIN; // pin name .
	iomux_pin_cfg_t PIN_CFG; // pin config .
	u32	PIN_PAD_CFG; // pad config .
	u32 GPIO_Grp; //  gpio group .
	u32	GPIO_Num; // gpio number .
	int iOutputValue; // key down value .
	int iIsInited;
	const char *pszName;
}GPIO_OUT;

#endif //]_GPIO_OUT





#ifdef __cplusplus //[
}
#endif //] __cplusplus

#endif//]_ntx_comm_h

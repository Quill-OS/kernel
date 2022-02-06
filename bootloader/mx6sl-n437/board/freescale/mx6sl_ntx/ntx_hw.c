/*
 * ntx_hw.c : 將所有硬體相關的控制放置於此,由board.c引入.
 *
 *
 *
 */ 

//#define DEBUG_I2C_CHN	1
//
#ifdef _MX6SL_ //[

#define PMIC_RC5T619 1
static const NTX_GPIO gt_ntx_gpio_home_key_gp3_24= {
	MX6SL_PAD_KEY_COL0__GPIO_3_24_KEYPAD,  // pin pad/mux control .
	3, // gpio group .
	24, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HOME]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};


static const NTX_GPIO gt_ntx_gpio_home_key= {
	MX6SL_PAD_KEY_ROW0__GPIO_3_25,  // pin pad/mux control .
	3, // gpio group .
	25, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HOME]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_fl_key= {
	MX6SL_PAD_KEY_ROW0__GPIO_3_25,  // pin pad/mux control  .
	3, // gpio group .
	25, // gpio number .
	0, // key down value .
	0, // not inited .
	"[FL]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_return_key= {
	MX6SL_PAD_KEY_ROW1__GPIO_3_27,  // pin pad/mux control  .
	3, // gpio group .
	27, // gpio number .
	0, // key down value .
	0, // not inited .
	"[RETURN]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_menu_key= {
	MX6SL_PAD_KEY_ROW2__GPIO_3_29,  // pin pad/mux control  .
	3, // gpio group .
	29, // gpio number .
	0, // key down value .
	0, // not inited .
	"[MENU]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};


static const NTX_GPIO gt_ntx_gpio_fl_key_gp3_26= {
	MX6SL_PAD_KEY_COL1__GPIO_3_26,  // pin pad/mux control  .
	3, // gpio group .
	26, // gpio number .
	0, // key down value .
	0, // not inited .
	"[FL]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_hallsensor_key_gp5_12= {
	MX6SL_PAD_SD1_DAT4__GPIO_5_12,  // pin pad/mux control .
	5, // gpio group .
	12, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HALL]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_hallsensor_key= {
	MX6SL_PAD_FEC_MDC__GPIO_4_23,  // pin pad/mux control .
	4, // gpio group .
	23, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HALL]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_power_key_gp5_8= {
	MX6SL_PAD_SD1_DAT1__GPIO_5_8,  // pin pad/mux control .
	5, // gpio group .
	8, // gpio number .
	0, // key down value .
	0, // not inited .
	"[POWER]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};


static const NTX_GPIO gt_ntx_gpio_power_key= {
	MX6SL_PAD_FEC_CRS_DV__GPIO_4_25,  // pin pad/mux control .
	4, // gpio group .
	25, // gpio number .
	0, // key down value .
	0, // not inited .
	"[POWER]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_ACIN= {
	MX6SL_PAD_SD1_CMD__GPIO_5_14,  // pin pad/mux control .
	5, // gpio group .
	14, // gpio number .
	0, // NC .
	0, // not inited .
	"ACIN", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_esdin= {
	MX6SL_PAD_SD2_DAT4__GPIO_5_2,  // pin pad/mux control .
	5, // gpio group .
	2, // gpio number .
	0, // NC .
	0, // not inited .
	"ESDIN", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_bootcfg_23= {
	MX6SL_PAD_LCD_DAT11__GPIO_2_31,  // pin pad/mux control .
	2, // gpio group .
	31, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG23", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_bootcfg_24= {
	MX6SL_PAD_LCD_DAT12__GPIO_3_0,  // pin pad/mux control .
	3, // gpio group .
	0, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG24", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_bootcfg_16= {
	MX6SL_PAD_LCD_DAT6__GPIO_2_26,  // pin pad/mux control .
	2, // gpio group .
	26, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG16", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_bootcfg_15= {
	MX6SL_PAD_LCD_DAT5__GPIO_2_25,  // pin pad/mux control .
	2, // gpio group .
	25, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG15", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_EPDPMIC_VCOM= {
	MX6SL_PAD_EPDC_VCOM0__GPIO_2_3,  // pin pad/mux control  .
	2, // gpio group .
	3, // gpio number .
	0, // default output value .
	0, // not inited .
	"EPDPMIC_VCOM", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EPDPMIC_VIN= {
	MX6SL_PAD_EPDC_PWRWAKEUP__GPIO_2_14,  // pin pad/mux control  .
	2, // gpio group .
	14, // gpio number .
	1, // default output value .
	0, // not inited .
	"EPDPMIC_VIN", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_65185_WAKEUP= {
	MX6SL_PAD_EPDC_PWRCTRL0__GPIO_2_7,  // pin pad/mux control  .
	2, // gpio group .
	7, // gpio number .
	0, // default output value .
	0, // not inited .
	"65185_WAKEUP", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_65185_PWRUP= {
	MX6SL_PAD_EPDC_PWRCTRL1__GPIO_2_8,  // pin pad/mux control  .
	2, // gpio group .
	8, // gpio number .
	0, // default output value .
	0, // not inited .
	"65185_PWRUP", // name .
	0, // 1:input ; 0:output ; 2:btn .
};



static const NTX_GPIO gt_ntx_gpio_ON_LED= {
	MX6SL_PAD_FEC_REF_CLK__GPIO_4_26,  // pin pad/mux control .
	4, // gpio group .
	26, // gpio number .
	0, // default output value .
	0, // not inited .
	"ON_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_ON_LED_gp5_13= {
	MX6SL_PAD_SD1_DAT2__GPIO_5_13,  // pin pad/mux control .
	5, // gpio group .
	13, // gpio number .
	0, // default output value .
	0, // not inited .
	"ON_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_ACTION_LED= {
	MX6SL_PAD_FEC_TX_EN__GPIO_4_22,  // pin pad/mux control .
	4, // gpio group .
	22, // gpio number .
	0, // default output value .
	0, // not inited .
	"ACTION_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_ACTION_LED_gp5_7= {
	MX6SL_PAD_SD1_DAT6__GPIO_5_7,  // pin pad/mux control .
	5, // gpio group .
	7, // gpio number .
	0, // default output value .
	0, // not inited .
	"ACTION_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_Charge_LED= {
	MX6SL_PAD_FEC_TXD1__GPIO_4_16,  // pin pad/mux control .
	4, // gpio group .
	16, // gpio number .
	0, // default output value .
	0, // not inited .
	"Charge_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_Charge_LED_gp5_15= {
	MX6SL_PAD_SD1_CLK__GPIO_5_15,  // pin pad/mux control .
	5, // gpio group .
	15, // gpio number .
	0, // default output value .
	0, // not inited .
	"Charge_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_Charge_LED_gp5_10= {
	MX6SL_PAD_SD1_DAT7__GPIO_5_10,  // pin pad/mux control .
	5, // gpio group .
	10, // gpio number .
	0, // default output value .
	0, // not inited .
	"Charge_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_ISD_3V3_ON= {
	MX6SL_PAD_KEY_ROW3__GPIO_3_31,  // pin pad/mux control .
	3, // gpio group .
	31, // gpio number .
	0, // default output value .
	0, // not inited .
	"ISD_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_ESD_3V3_ON= {
	MX6SL_PAD_KEY_ROW2__GPIO_3_29,  // pin pad/mux control .
	3, // gpio group .
	29, // gpio number .
	0, // default output value .
	0, // not inited .
	"ESD_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_TP_3V3_ON= {
	MX6SL_PAD_KEY_ROW4__GPIO_4_1,  // pin pad/mux control .
	4, // gpio group .
	1, // gpio number .
	0, // default output value .
	0, // not inited .
	"TP_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_IR_RST= {
	MX6SL_PAD_SD1_DAT5__GPIO_5_9,  // pin pad/mux control .
	5, // gpio group .
	9, // gpio number .
	0, // default output value .
	0, // not inited .
	"IR_RST", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_CT_RST= {
	MX6SL_PAD_SD1_DAT2__GPIO_5_13,  // pin pad/mux control .
	5, // gpio group .
	13, // gpio number .
	0, // default output value .
	0, // not inited .
	"CT_RST", // name .
	0, // 1:input ; 0:output ; 2:btn .
};


static const NTX_GPIO gt_ntx_gpio_WIFI_3V3_ON= {
	MX6SL_PAD_SD2_DAT6__GPIO_4_29,  // pin pad/mux control .
	4, // gpio group .
	29, // gpio number .
	0, // default output value .
	0, // not inited .
	"WIFI_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EP_3V3_ON= {
	MX6SL_PAD_KEY_ROW5__GPIO_4_3,  // pin pad/mux control .
	4, // gpio group .
	3, // gpio number .
	0, // default output value .
	0, // not inited .
	"EP_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EP_PWRCTRL1= {
	MX6SL_PAD_EPDC_PWRCTRL1__GPIO_2_8,  // pin pad/mux control .
	2, // gpio group .
	8, // gpio number .
	0, // default output value .
	0, // not inited .
	"EPDC_PWRCTRL1", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_Wifi3V3= {
	MX6SL_PAD_SD2_DAT6__GPIO_4_29,  // pin pad/mux control .
	4, // gpio group .
	29, // gpio number .
	0, // default output value .
	0, // not inited .
	"WIFI_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_HOME_LED= {
	MX6SL_PAD_SD1_DAT7__GPIO_5_10,  // pin pad/mux control .
	5, // gpio group .
	10, // gpio number .
	0, // default output value .
	1, // inited .
	"HOME_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};



#elif defined (_MX50_) //][

static const NTX_GPIO gt_ntx_gpio_home_key= {
	MX50_PIN_KEY_ROW0,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	//PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE|PAD_CTL_DRV_HIGH, // pad config .
	0, // pad config .
	4, // gpio group .
	1, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HOME]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_fl_key= {
	MX50_PIN_EPDC_BDR0,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	PAD_CTL_100K_PU|PAD_CTL_PUE_PULL|PAD_CTL_PKE_ENABLE|PAD_CTL_DRV_HIGH, // pad config .
	4, // gpio group .
	23, // gpio number .
	0, // key down value .
	0, // not inited .
	"[FL]",// name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_hallsensor_key= {
	MX50_PIN_SD3_D5,  // pin name .
	IOMUX_CONFIG_ALT1, // pin config .
	0, // pad config .
	5, // gpio group .
	25, // gpio number .
	0, // not inited .
	0, // key down value .
	"[HALL]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
#elif defined ( _MX6Q_ )
static const NTX_GPIO gt_ntx_gpio_WIFI_3V3_ON= {
	//MX6Q_PAD_GPIO_6__GPIO_1_6,  // pin pad/mux control .
	_MX6Q_PAD_GPIO_6__GPIO_1_6,  // pin pad/mux control .
	1, // gpio group .
	6, // gpio number .
	0, // default output value .
	0, // not inited .
	"WIFI_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_bootcfg_0= {
	//MX6Q_PAD_EIM_DA12__GPIO_3_12,  // pin pad/mux control .
	_MX6Q_PAD_EIM_DA12__GPIO_3_12,  // pin pad/mux control .
	3, // gpio group .
	12, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG_SRC0", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_bootcfg_1= {
	//MX6Q_PAD_EIM_DA13__GPIO_3_13,  // pin pad/mux control .
	_MX6Q_PAD_EIM_DA13__GPIO_3_13,  // pin pad/mux control .
	3, // gpio group .
	13, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG_SRC1", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_power_key= {
	MX6Q_PAD_GPIO_2__GPIO_1_2_BTN,  // pin pad/mux control .
	//_MX6Q_PAD_GPIO_2__GPIO_1_2,  // pin pad/mux control .
	1, // gpio group .
	2, // gpio number .
	0, // key down value .
	0, // not inited .
	"[POWER]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_OTG_FB_key= {
	MX6Q_PAD_EIM_D26__GPIO_3_26_PDBTN,  // pin pad/mux control .
	//MX6Q_PAD_EIM_D26__GPIO_3_26,  // pin pad/mux control .
	3, // gpio group .
	26, // gpio number .
	1, // key down value .
	0, // not inited .
	"[OTG_FB]", // name . OTG and FastBoot mode 
	2, // 1:input ; 0:output ; 2:btn .
};

#endif //]CONFIG_MX6SL
NTX_GPIO * ntx_gpio_keysA[] = {
#ifdef _MX6SL_//[
	&gt_ntx_gpio_home_key,
	&gt_ntx_gpio_fl_key,
//	&gt_ntx_gpio_hallsensor_key,
	0,
#elif defined (_MX6Q_)
	&gt_ntx_gpio_power_key,
	&gt_ntx_gpio_OTG_FB_key,
#endif //]_MX6SL_
};

int gi_ntx_gpio_keys=sizeof(ntx_gpio_keysA)/sizeof(ntx_gpio_keysA[0]);


void wifi_3v3(int iON) 
{
#ifdef _MX6SL_//[
	ntx_gpio_set_value(&gt_ntx_gpio_Wifi3V3,iON);
#endif //]_MX6SL_
}

void _led_R(int iIsTurnON)
{
	if(!gptNtxHwCfg) {
		printf("%s(%d) : cannot work without ntx hwconfig !\n",__FUNCTION__,iIsTurnON);
		return ;
	}

#if _MX6SL_//[
	if(31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) {
		// E60Q0X/E60Q1X .
		ntx_gpio_set_value(&gt_ntx_gpio_Charge_LED,iIsTurnON);
	}
	else if(47==gptNtxHwCfg->m_val.bPCB) {
		// ED0Q02 .
	}
	else if(40==gptNtxHwCfg->m_val.bPCB) {
		// E60Q52 .
		ntx_gpio_set_value(&gt_ntx_gpio_Charge_LED_gp5_15,iIsTurnON?0:1);
	}
	else {
		if(5==gptNtxHwCfg->m_val.bLed||6==gptNtxHwCfg->m_val.bLed||7==gptNtxHwCfg->m_val.bLed||8==gptNtxHwCfg->m_val.bLed) {
			// Green only/White only/No Lights/White and Home LED .
		}
		else
		if(1==gptNtxHwCfg->m_val.bLed) {
			// RGB type LED .
			ntx_gpio_set_value(&gt_ntx_gpio_Charge_LED_gp5_10,iIsTurnON?0:1);
		}
		else {
			ntx_gpio_set_value(&gt_ntx_gpio_Charge_LED_gp5_15,iIsTurnON?0:1);
		}
	}
#endif //]_MX6SL_
}
void _led_G(int iIsTurnON)
{
#ifdef _MX6SL_//[
	//ntx_gpio_set_value(&gt_ntx_gpio_ISD_3V3_ON,0);
	//ntx_gpio_set_value(&gt_ntx_gpio_ESD_3V3_ON,1);
	//ntx_gpio_set_value(&gt_ntx_gpio_TP_3V3_ON,1);
	//ntx_gpio_set_value(&gt_ntx_gpio_WIFI_3V3_ON,0);
	//ntx_gpio_set_value(&gt_ntx_gpio_EP_3V3_ON,1);
	if(!gptNtxHwCfg) {
		printf("%s(%d) : cannot work without ntx hwconfig !\n",__FUNCTION__,iIsTurnON);
		return ;
	}

	if(31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) {
		// E60Q0X/E60Q1X .
		ntx_gpio_set_value(&gt_ntx_gpio_ACTION_LED,iIsTurnON);
	}
	else {
		ntx_gpio_set_value(&gt_ntx_gpio_ACTION_LED_gp5_7,iIsTurnON?0:1);
	}
#endif //]_MX6SL_
}
void _led_B(int iIsTurnON)
{
#ifdef _MX6SL_//[
	if(31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) {
		ntx_gpio_set_value(&gt_ntx_gpio_ON_LED,iIsTurnON);
	}
	else if(47==gptNtxHwCfg->m_val.bPCB) {
		// ED0Q02 .
	}
	else {
		if(1==gptNtxHwCfg->m_val.bLed) {
			// RGB type LED .
			ntx_gpio_set_value(&gt_ntx_gpio_ON_LED_gp5_13,iIsTurnON?0:1);
		}
	}
#endif //]_MX6SL_
}

void _set_ISD_3V3_ON(int value)
{
#ifdef _MX6SL_//[
	ntx_gpio_set_value(&gt_ntx_gpio_ISD_3V3_ON, value);
#endif //]_MX6SL_
}

void _set_ESD_3V3_ON(int value)
{
#ifdef _MX6SL_//[
	ntx_gpio_set_value(&gt_ntx_gpio_ESD_3V3_ON, value);
#endif//]_MX6SL_
}

void _set_TP_3V3_ON(int value)
{
#ifdef _MX6SL_//[
	ntx_gpio_set_value(&gt_ntx_gpio_TP_3V3_ON, value);
#endif //]_MX6SL_
}
void _set_TP_RST(int value)
{
#ifdef _MX6SL_//[
	if(4==gptNtxHwCfg->m_val.bTouchType) {
		// IR touch type ...
		ntx_gpio_set_value(&gt_ntx_gpio_IR_RST, value);
	}
	else if (3==gptNtxHwCfg->m_val.bTouchType) {
		// C touch type ...
		ntx_gpio_set_value(&gt_ntx_gpio_CT_RST, value);
	}
#endif //]_MX6SL_

}


void _set_WIFI_3V3_ON(int value)
{
	ntx_gpio_set_value((struct NTX_GPIO *)&gt_ntx_gpio_WIFI_3V3_ON, value);
}

void _set_EP_3V3_ON(int value)
{
#ifdef _MX6SL_//[
	ntx_gpio_set_value(&gt_ntx_gpio_EP_3V3_ON, value);
#endif //]_MX6SL_
}


int __get_sd_number(void)
{
	int iBT_PortNum=-1;
#ifdef _MX6Q_ //[
	int iBT_CFG_PORT0,iBT_CFG_PORT1;

	iBT_CFG_PORT0=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_0);
	iBT_CFG_PORT1=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_1);
	iBT_PortNum=iBT_CFG_PORT1<<1|iBT_CFG_PORT0;
#else //][!_MX6Q_
	int iBT_CFG23,iBT_CFG24;

	iBT_CFG23=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_23);
	iBT_CFG24=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_24);
	iBT_PortNum=iBT_CFG24<<1|iBT_CFG23;
#endif //] _MX6Q_

	return iBT_PortNum;
}

unsigned char ntxhw_get_bootdevice_type(void) 
{
	unsigned char bBootDevType = NTXHW_BOOTDEV_UNKOWN;
#ifdef _MX6SL_//[
	unsigned char bBootCfg15;
	unsigned char bBootCfg16;

	bBootCfg15=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_15)?0x1:0x0;
	bBootCfg16=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_16)?0x2:0x0;

	bBootDevType=bBootCfg15|bBootCfg16;

	#if 0
	printf("%s(),BT_CFG15=%x,BT_CFG16=%x,BT_DEV=%x\n",__FUNCTION__,
			bBootCfg15,bBootCfg16,bBootDevType);
	#endif
#else //][!_MX6SL_

#endif //]_MX6SL_

	return bBootDevType;
}

int _get_boot_sd_number(void)
{
	static int giBT_PortNum=-1;

	if(-1==giBT_PortNum) {
		//char cCmdA[256];

		giBT_PortNum=__get_sd_number();

		//sprintf(cCmdA,"setenv fastboot_dev mmc%d",giBT_PortNum);
		//run_command(cCmdA, 0);// 

	}
	else {
	}

	return giBT_PortNum;
}

int _get_pcba_id (void)
{
	static int g_pcba_id;

	if(g_pcba_id) {
		return g_pcba_id;
	}

	if(gptNtxHwCfg) {
		switch(gptNtxHwCfg->m_val.bPCB)
		{

#ifdef _MX6SL_ //[
			
#else //][ !_MX6SL_
			case 12: //E60610
			case 20: //E60610C
			case 21: //E60610D
				g_pcba_id = 1;
				break;
			case 15: //E60620
				g_pcba_id = 4;
				break;
			case 16: //E60630
				g_pcba_id = 6;
				break;
			case 18: //E50600
				g_pcba_id = 2;
				break;
			case 19: //E60680
				g_pcba_id = 3;
				break;
			case 22: //E606A0
				g_pcba_id = 10;
				break;
			case 23: //E60670
				g_pcba_id = 5;
				break;
			case 24: //E606B0
				g_pcba_id = 14;
				break;
			case 27: //E50610
				g_pcba_id = 9;
				break;
			case 28: //E606C0
				g_pcba_id = 11;
				break;
#endif //]_MX6SL

			default:
				g_pcba_id = gptNtxHwCfg->m_val.bPCB;
				printf ("[%s-%d] PCBA ID=%d\n",__func__,__LINE__,g_pcba_id);
				break;	
		}
	}
	else {
		printf("%s(): [Warning] No hwconfig !\n",__FUNCTION__);
	}
	
	return g_pcba_id;
}

int _power_key_status (void)
{
	int iRet;

#ifdef _MX6SL_
	if (31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) {
		// E60Q1X/E60Q0X .
		iRet=ntx_gpio_key_is_down(&gt_ntx_gpio_power_key);
	}
	else {
		iRet=ntx_gpio_key_is_down(&gt_ntx_gpio_power_key_gp5_8);
	}
#elif defined (_MX6Q_) 
	iRet=ntx_gpio_key_is_down(&gt_ntx_gpio_power_key);
#else 
#error "Platform error !"
#endif
	return iRet;
}

int _sd_cd_status (void)
{
#ifdef _MX6Q_ //[
	return 0;
#else//][!_MX6Q_
	int iRet=ntx_gpio_get_value(&gt_ntx_gpio_esdin);
	return iRet?0:1;
#endif //] _MX6Q_
}

int _hallsensor_status (void)
{
#if defined (_MX6Q_) //[
#else //][!
	if(gptNtxHwCfg&&0!=gptNtxHwCfg->m_val.bHallSensor) {
		
		//return 0;
		if (31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) {
			// E60Q1X/E60Q0X .
			return ntx_gpio_key_is_down(&gt_ntx_gpio_hallsensor_key);
		}
		else {
			return ntx_gpio_key_is_down(&gt_ntx_gpio_hallsensor_key_gp5_12);
		}
	}
	else {
		return -1;
	}
#endif //]
	return 0;
}

int ntx_gpio_key_is_fastboot_down(void)
{
#ifdef _MX6Q_ //[
	return ntx_gpio_key_is_down(&gt_ntx_gpio_OTG_FB_key);
#elif defined(_MX6SL_)
	if(36==gptNtxHwCfg->m_val.bPCB || 40==gptNtxHwCfg->m_val.bPCB) {
		return ntx_gpio_key_is_fl_down();
	}
	else {
		return ntx_gpio_key_is_home_down();
	}
#else //][!_MX6Q_
	return 0;
#endif//] _MX6Q_
}
int ntx_gpio_key_is_home_down(void)
{
#ifdef _MX6SL_//[
	int iChk;
	if(12==gptNtxHwCfg->m_val.bKeyPad) {
		// model without keypad .
		return 0;
	}
	if (31==gptNtxHwCfg->m_val.bPCB||
			32==gptNtxHwCfg->m_val.bPCB||
			47==gptNtxHwCfg->m_val.bPCB) 
	{
		// E60Q1X/E60Q0X/ED0Q0X .
		return ntx_gpio_key_is_down(&gt_ntx_gpio_home_key);
	}
	else {
		return ntx_gpio_key_is_down(&gt_ntx_gpio_home_key_gp3_24);
	}
#endif //]_MX6SL_

	return 0;
}

int ntx_gpio_key_is_fl_down(void)
{
#ifdef _MX6SL_//[
	int iChk;
	if(12==gptNtxHwCfg->m_val.bKeyPad||17==gptNtxHwCfg->m_val.bKeyPad) {
		// Keypad is NO_Key || RETURN+HOME+MENU .
		return 0;
	}
	if (31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) {
		// E60Q1X/E60Q0X .
		return ntx_gpio_key_is_down(&gt_ntx_gpio_fl_key);
	}
	else {
		return ntx_gpio_key_is_down(&gt_ntx_gpio_fl_key_gp3_26);
	}
#endif //]
	return 0;
}


static const unsigned char gbMicroPI2C_ChipAddr = 0x43;
static unsigned int guiMicroPI2C_I2C_bus = 2;// I2C3
int msp430_I2C_Chn_set(unsigned int uiI2C_Chn)
{
	switch(uiI2C_Chn) {
	case 0:
	case 1:
	case 2:
		guiMicroPI2C_I2C_bus = uiI2C_Chn;
		break;
	default:
		printf("%s invalid I2C channel #%u!!\n",__FUNCTION__,uiI2C_Chn);
		return -1;
	}
	return 0;
}

int msp430_read_buf(unsigned char bRegAddr,unsigned char *O_pbBuf,int I_iBufSize)
{
	int iRet;
	unsigned int uiCurrI2CBus;

	uiCurrI2CBus = i2c_get_bus_num();
	if(uiCurrI2CBus!=guiMicroPI2C_I2C_bus) {
#ifdef DEBUG_I2C_CHN//[
		printf("%s(): change I2C bus to %d for MSP430\n",
				__FUNCTION__,(int)guiMicroPI2C_I2C_bus);
#endif //]DEBUG_I2C_CHN
		i2c_set_bus_num(guiMicroPI2C_I2C_bus);
	}

	iRet = i2c_read(gbMicroPI2C_ChipAddr, bRegAddr, 1, O_pbBuf, I_iBufSize);

	if(uiCurrI2CBus!=guiMicroPI2C_I2C_bus) {
#ifdef DEBUG_I2C_CHN//[
		printf("%s(): restore I2C bus to %d \n",
				__FUNCTION__,(int)uiCurrI2CBus);
#endif//]DEBUG_I2C_CHN
		i2c_set_bus_num(uiCurrI2CBus);
	}

	return iRet;
}
int msp430_read_reg(unsigned char bRegAddr,unsigned char *O_pbRegVal)
{
	return msp430_read_buf(bRegAddr,O_pbRegVal,1);
}

int msp430_write_buf(unsigned char bRegAddr,unsigned char *I_pbBuf,int I_iBufSize)
{
	int iRet;
	int iChk;
	unsigned int uiCurrI2CBus;

	uiCurrI2CBus = i2c_get_bus_num();
	if(uiCurrI2CBus!=guiMicroPI2C_I2C_bus) {
#ifdef DEBUG_I2C_CHN//[
		printf("%s(): change I2C bus to %d for MSP430\n",
				__FUNCTION__,(int)guiMicroPI2C_I2C_bus);
#endif//]DEBUG_I2C_CHN
		i2c_set_bus_num(guiMicroPI2C_I2C_bus);
	}

	iRet = i2c_write(gbMicroPI2C_ChipAddr,bRegAddr,1,I_pbBuf,I_iBufSize);

	if(uiCurrI2CBus!=guiMicroPI2C_I2C_bus) {
#ifdef DEBUG_I2C_CHN//[
		printf("%s(): restore I2C bus to %d \n",
				__FUNCTION__,(int)uiCurrI2CBus);
#endif //]DEBUG_I2C_CHN
		i2c_set_bus_num(uiCurrI2CBus);
	}

	return iRet;
}
int msp430_write_reg(unsigned char bRegAddr,unsigned char bRegVal)
{
	return msp430_write_buf(bRegAddr,&bRegVal,1);
}


#ifdef PMIC_RC5T619 //[

static const unsigned char gbRicohRC5T619_ChipAddr=0x32;
static const unsigned int guiRicohRC5T619_I2C_bus=2;// I2C3 .

int RC5T619_read_reg(unsigned char bRegAddr,unsigned char *O_pbRegVal)
{
	int iRet = -1;
	int iChk;
	unsigned char bRegVal;
	unsigned int uiCurrI2CBus;

	uiCurrI2CBus = i2c_get_bus_num();
	if(uiCurrI2CBus!=guiRicohRC5T619_I2C_bus) {
#ifdef DEBUG_I2C_CHN//[
		printf("%s(): change I2C bus to %d for RC5T619\n",
				__FUNCTION__,(int)guiRicohRC5T619_I2C_bus);
#endif//] DEBUG_I2C_CHN
		i2c_set_bus_num(guiRicohRC5T619_I2C_bus);
	}

	iChk = i2c_read(gbRicohRC5T619_ChipAddr, bRegAddr, 1, &bRegVal, 1);
	if(0==iChk) {
		printf("RC5T619 [0x%x]=0x%x\n",bRegAddr,bRegVal);
		if(O_pbRegVal) {
			*O_pbRegVal = bRegVal;
			iRet = 0;
		}
		else {
			iRet = 1;
		}
	}
	else {
		printf("RC5T619 read [0x%x] failed !!\n",bRegAddr);
	}

	if(uiCurrI2CBus!=guiRicohRC5T619_I2C_bus) {
#ifdef DEBUG_I2C_CHN//[
		printf("%s(): restore I2C bus to %d \n",
				__FUNCTION__,(int)uiCurrI2CBus);
#endif //]DEBUG_I2C_CHN
		i2c_set_bus_num(uiCurrI2CBus);
	}

	return iRet;
}
int RC5T619_write_buffer(unsigned char bRegAddr,unsigned char *I_pbRegWrBuf,unsigned short I_wRegWrBufBytes)
{
	int iRet = -1;
	unsigned int uiCurrI2CBus;
	int iChk;

	uiCurrI2CBus = i2c_get_bus_num();
	if(uiCurrI2CBus!=guiRicohRC5T619_I2C_bus) {
#ifdef DEBUG_I2C_CHN//[
		printf("%s(): change I2C bus to %d for RC5T619\n",
				__FUNCTION__,(int)guiRicohRC5T619_I2C_bus);
#endif //]DEBUG_I2C_CHN
		i2c_set_bus_num(guiRicohRC5T619_I2C_bus);
	}

	iChk = i2c_write(gbRicohRC5T619_ChipAddr, bRegAddr, 1, I_pbRegWrBuf, I_wRegWrBufBytes);
	if(0==iChk) {
	}
	else {
		printf("RC5T619 write to [0x%x] %d bytes failed !!\n",bRegAddr,I_wRegWrBufBytes);
	}

	if(uiCurrI2CBus!=guiRicohRC5T619_I2C_bus) {
#ifdef DEBUG_I2C_CHN//[
		printf("%s(): restore I2C bus to %d \n",
				__FUNCTION__,(int)uiCurrI2CBus);
#endif //]DEBUG_I2C_CHN
		i2c_set_bus_num(uiCurrI2CBus);
	}

	return iRet;
}

int RC5T619_write_reg(unsigned char bRegAddr,unsigned char bRegWrVal)
{
	return RC5T619_write_buffer(bRegAddr,&bRegWrVal,1);
}
#else //][!PMIC_RC5T619
int RC5T619_read_reg(unsigned char bRegAddr,unsigned char *O_pbRegVal){return -1;}
int RC5T619_write_reg(unsigned char bRegAddr,unsigned char bRegWrVal){return -1;}
int RC5T619_write_buffer(unsigned char bRegAddr,unsigned char *I_pbRegWrBuf,unsigned short I_wRegWrBufBytes){return -1;}
#endif //] PMIC_RC5T619


int ntx_detect_usb_plugin(void) 
{
	int iRet=0;
	if(1==gptNtxHwCfg->m_val.bPMIC) {
#ifdef PMIC_RC5T619 //[
		unsigned char bReg,bRegAddr;
		int iChk;
		// Ricoh RC5T619 .
		bRegAddr=0xbd;
		iChk = RC5T619_read_reg(bRegAddr,&bReg);
		if(iChk>=0) {
			if(bReg&0xc0) {
				iRet = 1;
			}
		}
#endif //]PMIC_RC5T619
	}
	else {
		iRet = ntx_gpio_get_value(&gt_ntx_gpio_ACIN)?0:1;
	}
	return iRet;
}


int init_pwr_i2c_function(int iSetAsFunc)
{
	return 0;
}

void EPDPMIC_power_on(int iIsPowerON)
{
#ifdef _MX6Q_//[
#else //][!_MX6Q_
	if(iIsPowerON) {
		ntx_gpio_set_value(&gt_ntx_gpio_EPDPMIC_VIN,1);
	}
	else {
		ntx_gpio_set_value(&gt_ntx_gpio_EPDPMIC_VIN,0);
	}
#endif ///]_MX6Q_
}
void EPDPMIC_vcom_onoff(int iIsON)
{
#ifdef _MX6Q_//[
#else //][!_MX6Q_
	if(iIsON) {
		ntx_gpio_set_value(&gt_ntx_gpio_EPDPMIC_VCOM,1);
	}
	else {
		ntx_gpio_set_value(&gt_ntx_gpio_EPDPMIC_VCOM,0);
	}
#endif//] _MX6Q_
}
 
void FP9928_rail_power_onoff(int iIsON)
{
#ifdef _MX6Q_//[
#else //][!_MX6Q_
	if(iIsON) {
		ntx_gpio_set_value(&gt_ntx_gpio_EP_PWRCTRL1,1);
	}
	else {
		ntx_gpio_set_value(&gt_ntx_gpio_EP_PWRCTRL1,0);
	}
#endif//] _MX6Q_
}

void _init_tps65185_power(int iIsWakeup,int iIsActivePwr)
{

#ifdef _MX6Q_//[
#else //][!_MX6Q_
	udelay(200);

	ntx_gpio_set_value(&gt_ntx_gpio_65185_WAKEUP,iIsWakeup);
	ntx_gpio_set_value(&gt_ntx_gpio_65185_PWRUP,iIsActivePwr);
	if(iIsActivePwr) {
		udelay(2*1000);
	}

#endif //]_MX6Q_
}

/**********************************************
 *
 * ntx_hw_early_init() 
 *
 * the initial actions in early time .
 * 
 **********************************************/
void ntx_hw_early_init(void)
{
	EPDPMIC_power_on(0);
}


void ntx_hw_late_init(void) 
{
	NTX_GPIO *pt_gpio;
	int i;

	printf("%s()\n",__FUNCTION__);
	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}

#if defined(_MX6Q_) //[
	if(NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bPCB_Flags,0)) 
	{
		
		for(i=0;i<gi_ntx_gpio_keys;i++) {
			ntx_gpio_init(ntx_gpio_keysA[i]);
		}
		
	}
#else //][!
	if(9==gptNtxHwCfg->m_val.bCustomer) {
		_led_R(0);
		_led_G(0);
		_led_B(1);
	}
	else {
		_led_R(0);
		_led_G(1);
		_led_B(0);
	}

	if (31==gptNtxHwCfg->m_val.bPCB||
			32==gptNtxHwCfg->m_val.bPCB||
			47==gptNtxHwCfg->m_val.bPCB) 
	{
		// E60Q1X/E60Q0X/ED0Q0X .
	}
	else 
	{
		//  
		ntx_gpio_keysA[0] = &gt_ntx_gpio_home_key_gp3_24;
	}
  
  // Assign front light key GPIO
  if (31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) { // E60Q1X/E60Q0X
    ntx_gpio_keysA[1] = &gt_ntx_gpio_fl_key;
  }
  else if (47==gptNtxHwCfg->m_val.bPCB) { //ED0Q0X
    ntx_gpio_keysA[1] = &gt_ntx_gpio_return_key;
	}
  else if (36==gptNtxHwCfg->m_val.bPCB||40==gptNtxHwCfg->m_val.bPCB) { //E60Q3X/E60Q5X
    ntx_gpio_keysA[1] = &gt_ntx_gpio_fl_key_gp3_26;
  }
  else {
    ntx_gpio_keysA[1] = 0;
  }
    
  if (47==gptNtxHwCfg->m_val.bPCB) { //ED0Q0X
    ntx_gpio_keysA[2] = &gt_ntx_gpio_menu_key;
	}

	pt_gpio = &gt_ntx_gpio_esdin;
	ntx_gpio_init(pt_gpio);

	if (36==gptNtxHwCfg->m_val.bPCB||40==gptNtxHwCfg->m_val.bPCB) {
		// E60Q32/E60Q52
		ntx_gpio_init(&gt_ntx_gpio_HOME_LED);
	}


	// setup I2C chanel .
	if( (46==gptNtxHwCfg->m_val.bPCB && gptNtxHwCfg->m_val.bPCB_REV>=0x10) ||
			48==gptNtxHwCfg->m_val.bPCB	)
	{
		// >=E60Q9XA1 or =E60QAX
		msp430_I2C_Chn_set(0); // MSP430 @ I2C1 .
	}


#endif //]
}


/*
 * ntx_hw.c : 將所有硬體相關的控制放置於此,由board.c引入.
 *
 *
 *
 */ 

//#define DEBUG_I2C_CHN	1
//
//#include <usb/imx_udc.h>

#include "ntx_pads.h"

#ifdef _MX7D_ //[


static const NTX_GPIO gt_ntx_gpio_WIFI_3V3_ON= {
	MX7D_PAD_ENET1_RX_CLK__GPIO7_IO13,  // pin pad/mux control .
	7, // gpio group .
	13, // gpio number .
	0, // default output value .
	0, // not inited .
	"WIFI_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_bootcfg_11= {
	MX7D_PAD_LCD_DATA11__GPIO3_IO16,  // pin pad/mux control .
	3, // gpio group .
	16, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG11", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_bootcfg_10= {
	MX7D_PAD_LCD_DATA10__GPIO3_IO15,  // pin pad/mux control .
	3, // gpio group .
	15, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG10", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_esdin= {
	MX7D_PAD_SD2_CD_B__GPIO5_IO9,  // pin pad/mux control .
	5, // gpio group .
	9, // gpio number .
	0, // NC .
	0, // not inited .
	"ESDIN", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_ACIN= {
	MX7D_PAD_GPIO1_IO01__GPIO1_IO1,  // pin pad/mux control .
	1, // gpio group .
	1, // gpio number .
	0, // NC .
	0, // not inited .
	"ACIN", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_EPDPMIC_VIN= {
	MX7D_PAD_ENET1_CRS__GPIO7_IO14 | MUX_PAD_CTRL(EPDC_PAD_CTRL),  // pin pad/mux control  .
	7, // gpio group .
	14, // gpio number .
	1, // default output value .
	0, // not inited .
	"EPDPMIC_VIN", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EPDPMIC_VCOM= {
	MX7D_PAD_ENET1_RGMII_RD0__GPIO7_IO0 | MUX_PAD_CTRL(EPDC_PAD_CTRL),  // pin pad/mux control  .
	7, // gpio group .
	0, // gpio number .
	0, // default output value .
	0, // not inited .
	"EPDPMIC_VCOM", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_65185_WAKEUP= {
	MX7D_PAD_EPDC_SDCE3__GPIO2_IO23,  // pin pad/mux control  .
	2, // gpio group .
	23, // gpio number .
	0, // default output value .
	0, // not inited .
	"65185_WAKEUP", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_65185_PWRUP= {
	MX7D_PAD_EPDC_PWR_COM__GPIO2_IO30,  // pin pad/mux control  .
	2, // gpio group .
	30, // gpio number .
	0, // default output value .
	0, // not inited .
	"65185_PWRUP", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_EPDPMIC_PWRGOOD= {
	MX7D_PAD_EPDC_PWR_STAT__GPIO2_IO31,  // pin pad/mux control .
	2, // gpio group .
	31, // gpio number .
	0, // NC .
	0, // not inited .
	"EPDPMIC_PWRGOOD", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_LED_R= {
	MX7D_PAD_ENET1_TX_CLK__GPIO7_IO12,  // pin pad/mux control .
	7, // gpio group .
	12, // gpio number .
	0, // default output value .
	0, // not inited .
	"R", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_LED_G= {
	MX7D_PAD_ENET1_RGMII_TX_CTL__GPIO7_IO10,  // pin pad/mux control .
	7, // gpio group .
	10, // gpio number .
	0, // default output value .
	0, // not inited .
	"G", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_LED_B= {
	MX7D_PAD_ENET1_RGMII_TXC__GPIO7_IO11,  // pin pad/mux control .
	7, // gpio group .
	11, // gpio number .
	0, // default output value .
	0, // not inited .
	"B", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_FLPWR= {
	MX7D_PAD_ENET1_RGMII_RD1__GPIO7_IO1,  // pin pad/mux control  .
	7, // gpio group .
	1, // gpio number .
	1, // default output value .
	0, // not inited .
	"FL_PWR", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_TPRST= {
	MX7D_PAD_ENET1_RGMII_TD0__GPIO7_IO6,  // pin pad/mux control  .
	7, // gpio group .
	6, // gpio number .
	1, // default output value .
	0, // not inited .
	"TP_RST", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

#elif defined(_MX6ULL_) //][

#define PMIC_RC5T619 1

static const NTX_GPIO gt_ntx_gpio_TPRST= {
	MX6_PAD_GPIO1_IO06__GPIO1_IO06,  // pin pad/mux control  .
	1, // gpio group .
	6, // gpio number .
	1, // default output value .
	0, // not inited .
	"TP_RST", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static NTX_GPIO gt_ntx_gpio_LED_G= {
	MX6_PAD_SNVS_TAMPER5__GPIO5_IO05 | MUX_PAD_CTRL(NO_PAD_CTRL),  // pin pad/mux control .
	5, // gpio group .
	5, // gpio number .
	0, // default output value .
	0, // not inited .
	"G", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_WIFI_3V3_ON= {
	MX6_PAD_CSI_DATA02__GPIO4_IO23 | MUX_PAD_CTRL(NO_PAD_CTRL),  // pin pad/mux control .
	4, // gpio group .
	23, // gpio number .
	0, // default output value .
	0, // not inited .
	"WIFI_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_power_key= {
	MX6_PAD_SNVS_TAMPER4__GPIO5_IO04 | MUX_PAD_CTRL(NO_PAD_CTRL),  // pin pad/mux control .
	5, // gpio group .
	4, // gpio number .
	0, // key down value .
	0, // not inited .
	"[POWER]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EPDPMIC_VIN= {
	MX6_PAD_LCD_DATA09__GPIO3_IO14 | MUX_PAD_CTRL(EPDC_PAD_CTRL),  // pin pad/mux control  .
	3, // gpio group .
	14, // gpio number .
	1, // default output value .
	0, // not inited .
	"EPDPMIC_VIN", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EPDPMIC_VCOM= {
	MX6_PAD_LCD_DATA19__GPIO3_IO24 | MUX_PAD_CTRL(EPDC_PAD_CTRL),  // pin pad/mux control  .
	3, // gpio group .
	24, // gpio number .
	0, // default output value .
	0, // not inited .
	"EPDPMIC_VCOM", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EPDPMIC_PWRGOOD= {
	MX6_PAD_LCD_DATA11__GPIO3_IO16|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	3, // gpio group .
	16, // gpio number .
	0, // NC .
	0, // not inited .
	"EPDPMIC_PWRGOOD", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_bootcfg2_3= {
	MX6_PAD_LCD_DATA11__GPIO3_IO16|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	3, // gpio group .
	16, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG2_3", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_bootcfg2_4= {
	MX6_PAD_LCD_DATA12__GPIO3_IO17|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	3, // gpio group .
	17, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG2_4", // name .
	1, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_hallsensor_key= {
	MX6_PAD_SNVS_TAMPER2__GPIO5_IO02|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	5, // gpio group .
	2, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HALL]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_home_key= {
	0,  // pin pad/mux control .
	0, // gpio group .
	0, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HOME]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_LED_R= {
	0,  // pin pad/mux control .
	0, // gpio group .
	0, // gpio number .
	0, // default output value .
	0, // not inited .
	"R", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_LED_B= {
	0,  // pin pad/mux control .
	0, // gpio group .
	0, // gpio number .
	0, // default output value .
	0, // not inited .
	"B", // name .
	0, // 1:input ; 0:output ; 2:btn .
};


#elif defined(_MX6SLL_) //][
#define PMIC_RC5T619 1
static const NTX_GPIO gt_ntx_gpio_TPRST= {
	MX6_PAD_SD1_DATA2__GPIO5_IO13|MUX_PAD_CTRL(LED_PAD_CTRL),  // pin pad/mux control  .
	5, // gpio group .
	13, // gpio number .
	1, // default output value .
	0, // not inited .
	"TP_RST", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_TPRST2= {
	MX6_PAD_GPIO4_IO18__GPIO4_IO18|MUX_PAD_CTRL(LED_PAD_CTRL),  // pin pad/mux control  .
	4, // gpio group .
	18, // gpio number .
	1, // default output value .
	0, // not inited .
	"TP_RST", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_LED_G= {
	MX6_PAD_SD1_DATA6__GPIO5_IO07|MUX_PAD_CTRL(LED_PAD_CTRL),  // pin pad/mux control .
	5, // gpio group .
	7, // gpio number .
	0, // default output value .
	0, // not inited .
	"G", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_LED_G2= {
	MX6_PAD_GPIO4_IO22__GPIO4_IO22|MUX_PAD_CTRL(LED_PAD_CTRL),  // pin pad/mux control .
	4, // gpio group .
	22, // gpio number .
	0, // default output value .
	0, // not inited .
	"G", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_LED_R= {
	MX6_PAD_SD1_DATA5__GPIO5_IO09|MUX_PAD_CTRL(LED_PAD_CTRL),  // pin pad/mux control .
	5, // gpio group .
	9, // gpio number .
	0, // default output value .
	0, // not inited .
	"R", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_LED_R2= {
	MX6_PAD_GPIO4_IO17__GPIO4_IO17|MUX_PAD_CTRL(LED_PAD_CTRL),  // pin pad/mux control .
	4, // gpio group .
	17, // gpio number .
	0, // default output value .
	0, // not inited .
	"R", // name .
	0, // 1:input ; 0:output ; 2:btn .
};

static NTX_GPIO gt_ntx_gpio_LED_B= {
	MX6_PAD_SD1_DATA7__GPIO5_IO10|MUX_PAD_CTRL(LED_PAD_CTRL),  // pin pad/mux control .
	5, // gpio group .
	10, // gpio number .
	0, // default output value .
	0, // not inited .
	"B", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_LED_B2= {
	MX6_PAD_GPIO4_IO16__GPIO4_IO16|MUX_PAD_CTRL(LED_PAD_CTRL),  // pin pad/mux control .
	4, // gpio group .
	16, // gpio number .
	0, // default output value .
	0, // not inited .
	"B", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_WIFI_3V3_ON= {
	MX6_PAD_SD2_DATA6__GPIO4_IO29,  // pin pad/mux control .
	4, // gpio group .
	29, // gpio number .
	0, // default output value .
	0, // not inited .
	"WIFI_3V3", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_power_key= {
	MX6_PAD_SD1_DATA1__GPIO5_IO08|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	5, // gpio group .
	8, // gpio number .
	0, // key down value .
	0, // not inited .
	"[POWER]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_power_key_2= {
	MX6_PAD_GPIO4_IO25__GPIO4_IO25|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	4, // gpio group .
	25, // gpio number .
	0, // key down value .
	0, // not inited .
	"[POWER]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_home_key= {
	MX6_PAD_KEY_COL0__GPIO3_IO24|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	3, // gpio group .
	24, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HOME]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_pgup_key= {
	MX6_PAD_KEY_COL4__GPIO4_IO00|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	4, // gpio group .
	0, // gpio number .
	0, // key down value .
	0, // not inited .
	"[PGUP]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_pgdn_key= {
	MX6_PAD_KEY_COL5__GPIO4_IO02|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	4, // gpio group .
	2, // gpio number .
	0, // key down value .
	0, // not inited .
	"[PGDN]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

static const NTX_GPIO gt_ntx_gpio_EPDPMIC_VIN= {
	MX6_PAD_EPDC_PWR_WAKE__GPIO2_IO14 | MUX_PAD_CTRL(EPDC_PAD_CTRL),  // pin pad/mux control  .
	2, // gpio group .
	14, // gpio number .
	1, // default output value .
	0, // not inited .
	"EPDPMIC_VIN", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EPDPMIC_VCOM= {
	MX6_PAD_EPDC_VCOM0__GPIO2_IO03 | MUX_PAD_CTRL(EPDC_PAD_CTRL),  // pin pad/mux control  .
	2, // gpio group .
	3, // gpio number .
	0, // default output value .
	0, // not inited .
	"EPDPMIC_VCOM", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_EPDPMIC_PWRGOOD= {
	MX6_PAD_EPDC_PWR_STAT__GPIO2_IO13|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	2, // gpio group .
	13, // gpio number .
	0, // NC .
	0, // not inited .
	"EPDPMIC_PWRGOOD", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_bootcfg_23= {
	MX6_PAD_LCD_DATA11__GPIO2_IO31|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	2, // gpio group .
	31, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG23", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_bootcfg_24= {
	MX6_PAD_LCD_DATA12__GPIO3_IO00|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	3, // gpio group .
	0, // gpio number .
	0, // NC .
	0, // not inited .
	"BT_CFG24", // name .
	1, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_hallsensor_key= {
	MX6_PAD_SD1_DATA4__GPIO5_IO12|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	5, // gpio group .
	12, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HALL]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_hallsensor_key2= {
	MX6_PAD_GPIO4_IO23__GPIO4_IO23|MUX_PAD_CTRL(INPUT_PAD_CTRL),  // pin pad/mux control .
	4, // gpio group .
	23, // gpio number .
	0, // key down value .
	0, // not inited .
	"[HALL]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};

#elif defined(_MX6SL_) //][

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

static const NTX_GPIO gt_ntx_gpio_home_key_gp3_27= {
	MX6SL_PAD_KEY_ROW1__GPIO_3_27_KEYPAD,  // pin pad/mux control  .
	3, // gpio group .
	27, // gpio number .
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
static const NTX_GPIO gt_ntx_gpio_menu_key_gp3_25= {
	MX6SL_PAD_KEY_ROW0__GPIO_3_25,  // pin pad/mux control  .
	3, // gpio group .
	25, // gpio number .
	0, // key down value .
	0, // not inited .
	"[MENU]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_left_key= {
	MX6SL_PAD_KEY_ROW2__GPIO_3_29,  // pin pad/mux control  .
	3, // gpio group .
	29, // gpio number .
	0, // key down value .
	0, // not inited .
	"[LEFT]", // name .
	2, // 1:input ; 0:output ; 2:btn .
};
static const NTX_GPIO gt_ntx_gpio_right_key= {
	MX6SL_PAD_KEY_ROW3__GPIO_3_31_KEYPAD,  // pin pad/mux control .
	3, // gpio group .
	31, // gpio number .
	0, // default output value .
	0, // not inited .
	"[RIGHT]", // name .
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
static NTX_GPIO *gpt_ntx_gpio_ACIN=&gt_ntx_gpio_ACIN;

static const NTX_GPIO gt_ntx_gpio_USBID= {
	MX6SL_PAD_SD1_DAT5__GPIO_5_9_PULLHIGH,  // pin pad/mux control .
	5, // gpio group .
	9, // gpio number .
	0, // NC .
	0, // not inited .
	"USBID", // name .
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

static const NTX_GPIO gt_ntx_gpio_EPDPMIC_PWRGOOD= {
	MX6SL_PAD_EPDC_PWRSTAT__GPIO_2_13_PUINT,  // pin pad/mux control .
	2, // gpio group .
	13, // gpio number .
	0, // NC .
	0, // not inited .
	"EPDPMIC_PWRGOOD", // name .
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
static NTX_GPIO gt_ntx_gpio_ON_LED_gp5_13= {
	MX6SL_PAD_SD1_DAT2__GPIO_5_13,  // pin pad/mux control .
	5, // gpio group .
	13, // gpio number .
	0, // default output value .
	0, // not inited .
	"ON_LED", // name .
	0, // 1:input ; 0:output ; 2:btn .
};
static NTX_GPIO gt_ntx_gpio_ACTION_LED_gp5_10= {
	MX6SL_PAD_SD1_DAT7__GPIO_5_10,  // pin pad/mux control .
	5, // gpio group .
	10, // gpio number .
	0, // default output value .
	0, // not inited .
	"ACTION_LED", // name .
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
static const NTX_GPIO gt_ntx_gpio_Charge_LED_gp5_9= {
	MX6SL_PAD_SD1_DAT5__GPIO_5_9,  // pin pad/mux control .
	5, // gpio group .
	9, // gpio number .
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
#elif defined ( _MX6Q_ )
#endif //]CONFIG_MX6SL


// LED gpios ...
static NTX_GPIO *gpt_ntx_gpio_LED_G;
static NTX_GPIO *gpt_ntx_gpio_LED_B;
static NTX_GPIO *gpt_ntx_gpio_LED_R;
static NTX_GPIO *gpt_ntx_gpio_LED_HOME;

// Touch reset gpios ...
static NTX_GPIO *gpt_ntx_gpio_TP_RST; // Touch reset output .
static NTX_GPIO *gpt_ntx_gpio_IR_RST; // IR touch reset output .
static NTX_GPIO *gpt_ntx_gpio_CT_RST; // C touch reset output .

// EPD PMIC gpios ...
static NTX_GPIO *gpt_ntx_gpio_65185_WAKEUP;
static NTX_GPIO *gpt_ntx_gpio_65185_PWRUP;
static NTX_GPIO *gpt_ntx_gpio_FP9928_RAIL_ON;

// MISC detction gpios ...
static NTX_GPIO *gpt_ntx_gpio_esdin; // External SD detction pin .
static NTX_GPIO *gpt_ntx_gpio_ACIN; // 
static const NTX_GPIO *gpt_ntx_gpio_USBID; // 

// power switch gpios ...
static NTX_GPIO *gpt_ntx_gpio_ISD_3V3_ON; // internal SD power .
static NTX_GPIO *gpt_ntx_gpio_ESD_3V3_ON; // external SD power . 
static NTX_GPIO *gpt_ntx_gpio_EP_3V3_ON; // EPD power .
static NTX_GPIO *gpt_ntx_gpio_EP_VCOM_ON; // EPD power .
static NTX_GPIO *gpt_ntx_gpio_TP_3V3_ON; // Touch power .
static NTX_GPIO *gpt_ntx_gpio_WIFI_3V3_ON; // Wifi power .
static NTX_GPIO *gpt_ntx_gpio_FLPWR; // Front light power .


int ntxhw_USBID_detected(void)
{
	if(gpt_ntx_gpio_USBID && !ntx_gpio_get_value(gpt_ntx_gpio_USBID)) {
		return 1;
	}
	return 0;
}

NTX_GPIO * ntx_gpio_keysA[NTX_GPIO_KEYS] = {
#if defined(_MX6Q_) //[
	&gt_ntx_gpio_power_key,
	&gt_ntx_gpio_OTG_FB_key,
#elif defined(_MX7D_) //][
#elif defined(_MX6SL_) //][
#elif defined(_MX6ULL_) //][
//	&gt_ntx_gpio_home_key,
//	&gt_ntx_gpio_fl_key,
//	&gt_ntx_gpio_hallsensor_key,
#endif //]_MX6SL_
	0,
};

//int gi_ntx_gpio_keys=sizeof(ntx_gpio_keysA)/sizeof(ntx_gpio_keysA[0]);
int gi_ntx_gpio_keys=0;

NTX_GPIO *gptNtxGpioKey_Home,*gptNtxGpioKey_FL,*gptNtxGpioKey_Power;
NTX_GPIO *gptNtxGpioKey_Menu,*gptNtxGpioKey_Return,*gptNtxGpioSW_HallSensor;
NTX_GPIO *gptNtxGpioKey_Left,*gptNtxGpioKey_Right;
NTX_GPIO *gptNtxGpioKey_PGUP,*gptNtxGpioKey_PGDN;

void wifi_3v3(int iON) 
{
	void _set_WIFI_3V3_ON(int value);

	_set_WIFI_3V3_ON(iON);
}

void _led_R(int iIsTurnON)
{
	if(!gptNtxHwCfg) {
		printf("%s(%d) : cannot work without ntx hwconfig !\n",__FUNCTION__,iIsTurnON);
		return ;
	}

#if defined(_MX7D_) || defined(_MX6SLL_) || defined(_MX6ULL_)//[
	
	if(gpt_ntx_gpio_LED_R) {
		ntx_gpio_set_value(gpt_ntx_gpio_LED_R,iIsTurnON?0:1);
	}

#elif defined(_MX6SL_) //][
	if(31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) {
		// E60Q0X/E60Q1X .
		ntx_gpio_set_value(&gt_ntx_gpio_Charge_LED,iIsTurnON);
	}
	else if(47==gptNtxHwCfg->m_val.bPCB || 54==gptNtxHwCfg->m_val.bPCB) {
		// ED0Q02 . ED0Q12
	}
	else if(40==gptNtxHwCfg->m_val.bPCB) {
		// E60Q52 .
		ntx_gpio_set_value(&gt_ntx_gpio_Charge_LED_gp5_15,iIsTurnON?0:1);
	}
	else if(55==gptNtxHwCfg->m_val.bPCB) {
		// E70Q02 .
		ntx_gpio_set_value(&gt_ntx_gpio_Charge_LED_gp5_9,iIsTurnON?0:1);
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
#endif //]_MX6SL_ || _MX6SLL_
}
/*
 * For models without RGB LED, default goes _led_G()
 */
void _led_G(int iIsTurnON)
{

#if defined(_MX7D_) || defined(_MX6ULL_) || defined(_MX6SLL_) //[
	if(6==gptNtxHwCfg->m_val.bLed) {
		// no lights .
	}
	else {
		//printf("%s(%d):%p\n",__FUNCTION__,iIsTurnON,gpt_ntx_gpio_LED_G);
		if(gpt_ntx_gpio_LED_G) {
			if(iIsTurnON) {
				ntx_gpio_set_valueEx(gpt_ntx_gpio_LED_G,0,0);
			}
			else {
				ntx_gpio_set_valueEx(gpt_ntx_gpio_LED_G,1,0);
			}
		}
	}
#elif defined(_MX6SL_) //][
	int iChk;

	//printf("%s(%d)\n",__FUNCTION__,iIsTurnON);
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
	else if(50==gptNtxHwCfg->m_val.bPCB||46==gptNtxHwCfg->m_val.bPCB||
		58==gptNtxHwCfg->m_val.bPCB||42==gptNtxHwCfg->m_val.bPCB||
		61==gptNtxHwCfg->m_val.bPCB)
	{
		// E60QFX/E60Q9X/E60QJX/E60Q6X/E60QKX .
		if(iIsTurnON) {

			gt_ntx_gpio_ON_LED_gp5_13.iIsInited = 0;
#if 1
			gt_ntx_gpio_ON_LED_gp5_13.iDirection = 0;
			ntx_gpio_set_value(&gt_ntx_gpio_ON_LED_gp5_13,0);
#else
			gt_ntx_gpio_ON_LED_gp5_13.iDirection = 1;
			iChk = ntx_gpio_get_value(&gt_ntx_gpio_ON_LED_gp5_13);
			if(iChk!=0) {
				printf("The LED should be set low ,Please check !!!\n");
			}
#endif
		}
		else {
			gt_ntx_gpio_ON_LED_gp5_13.iIsInited = 0;
			gt_ntx_gpio_ON_LED_gp5_13.iDirection = 0;
			ntx_gpio_set_value(&gt_ntx_gpio_ON_LED_gp5_13,1);
		}
	}
	else {
		if(6==gptNtxHwCfg->m_val.bLed) {
			// no lights .
		}
		else {
			ntx_gpio_set_value(&gt_ntx_gpio_ACTION_LED_gp5_7,iIsTurnON?0:1);
		}
	}
#endif //]_MX6SL_
}
void _led_B(int iIsTurnON)
{
#if defined(_MX7D_) || defined(_MX6ULL_) || defined(_MX6SLL_) //[
	if(gpt_ntx_gpio_LED_B) {
		ntx_gpio_set_value(gpt_ntx_gpio_LED_B,iIsTurnON?0:1);
	}
#elif defined(_MX6SL_) //][
	if(31==gptNtxHwCfg->m_val.bPCB||32==gptNtxHwCfg->m_val.bPCB) {
		ntx_gpio_set_value(&gt_ntx_gpio_ON_LED,iIsTurnON);
	}
	else if(47==gptNtxHwCfg->m_val.bPCB || 54==gptNtxHwCfg->m_val.bPCB) {
		// ED0Q02 . ED0Q12
	}
	else if(55==gptNtxHwCfg->m_val.bPCB) {
		// E70Q02 .
		ntx_gpio_set_value(&gt_ntx_gpio_ACTION_LED_gp5_10,iIsTurnON?0:1);
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
	if(gpt_ntx_gpio_ISD_3V3_ON) {
		ntx_gpio_set_value(gpt_ntx_gpio_ISD_3V3_ON, value);
	}
}

void _set_ESD_3V3_ON(int value)
{
	if(gpt_ntx_gpio_ESD_3V3_ON) {
		ntx_gpio_set_value(gpt_ntx_gpio_ESD_3V3_ON, value);
	}
}

void _set_TP_3V3_ON(int value)
{
	if(gpt_ntx_gpio_TP_3V3_ON) {
		ntx_gpio_set_value(gpt_ntx_gpio_TP_3V3_ON, value);
	}
}
void _set_TP_RST(int value)
{
	if(gpt_ntx_gpio_TP_RST) {
		ntx_gpio_set_value(gpt_ntx_gpio_TP_RST, value);
	}

}


void _set_WIFI_3V3_ON(int value)
{
	if(gpt_ntx_gpio_WIFI_3V3_ON) {
		ntx_gpio_set_value((struct NTX_GPIO *)gpt_ntx_gpio_WIFI_3V3_ON, value);
	}
}

void _set_EP_3V3_ON(int value)
{
	if(gpt_ntx_gpio_EP_3V3_ON) {
		ntx_gpio_set_value(gpt_ntx_gpio_EP_3V3_ON, value);
	}
}


int __get_sd_number(void)
{
	int iBT_PortNum=-1;
#ifdef _MX6Q_ //[
	int iBT_CFG_PORT0,iBT_CFG_PORT1;

	iBT_CFG_PORT0=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_0);
	iBT_CFG_PORT1=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_1);
	iBT_PortNum=iBT_CFG_PORT1<<1|iBT_CFG_PORT0;
#elif defined(_MX6SL_) || defined(_MX6SLL_) //][!_MX6Q_
	int iBT_CFG23,iBT_CFG24;

	iBT_CFG23=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_23);
	iBT_CFG24=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_24);
	printf("%s(),cfg23=%d,cfg24=%d \n",__FUNCTION__,iBT_CFG23,iBT_CFG24);
	iBT_PortNum=iBT_CFG24<<1|iBT_CFG23;
#elif defined(_MX6ULL_) //][!_MX6ULL_
#if 1
	iBT_PortNum=1;
#else
	int iBT_CFG2_3,iBT_CFG2_4;

	iBT_CFG2_4=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg2_4);
	iBT_CFG2_3=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg2_3);
	printf("%s(),cfg2_3=%d,cfg2_4=%d \n",__FUNCTION__,iBT_CFG2_3,iBT_CFG2_4);
	iBT_PortNum=iBT_CFG2_4<<1|iBT_CFG2_3;
#endif
#elif defined(_MX7D_) //][!_MX7D_
	int iBT_CFG11,iBT_CFG10;

	iBT_CFG11=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_11);
	iBT_CFG10=ntx_gpio_get_value(&gt_ntx_gpio_bootcfg_10);
	printf("%s(),cfg11=%d,cfg10=%d \n",__FUNCTION__,iBT_CFG11,iBT_CFG10);
	iBT_PortNum=iBT_CFG11<<1|iBT_CFG10;
#endif //] _MX6Q_

	return iBT_PortNum;
}

unsigned char ntxhw_get_bootdevice_type(void) 
{
	unsigned char bBootDevType = NTXHW_BOOTDEV_UNKOWN;
#ifdef _MX7D_ //[
#elif defined(_MX6ULL_) //][
#elif defined(_MX6SL_) //][

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

#ifdef _MX7D_ //[
#elif defined(_MX6SL_)||defined(_MX6SLL_) //][
#elif defined(_MX6ULL_) //][
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
	iRet=ntx_gpio_key_is_down(gptNtxGpioKey_Power);
	return iRet;
}

int _sd_cd_status (void)
{
#ifdef _MX6Q_ //[
	return 0;
#else//][!_MX6Q_
	if(gpt_ntx_gpio_esdin) {
		printf ("no extSD defined.\n");
		return 0;
	}
	else {
		return ntx_gpio_get_value(gpt_ntx_gpio_esdin)?0:1;
	}
#endif //] _MX6Q_
}

int _hallsensor_status (void)
{
#if defined (_MX6Q_) //[
#else //][!
	if(gptNtxGpioSW_HallSensor) {
		// E60Q1X/E60Q0X .
		return ntx_gpio_key_is_down(gptNtxGpioSW_HallSensor);
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
#elif defined(_MX6SL_) || defined(_MX6SLL_) || defined(_MX6ULL_)
	if( 36==gptNtxHwCfg->m_val.bPCB || 40==gptNtxHwCfg->m_val.bPCB|| 
			50==gptNtxHwCfg->m_val.bPCB || 
			16==gptNtxHwCfg->m_val.bKeyPad || 18==gptNtxHwCfg->m_val.bKeyPad ||
			11==gptNtxHwCfg->m_val.bKeyPad )
	{
		// Q30/Q50/QF0/QG0/Keypad type: HOMEPAD/FL_Key .
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
#if defined(_MX6SL_)||defined(_MX6SLL_)||defined(_MX6ULL_)//[
	if(gptNtxGpioKey_Home) {
		return ntx_gpio_key_is_down(gptNtxGpioKey_Home);
	}
#endif //]_MX6SL_

	return 0;
}

int ntx_gpio_key_is_menu_down(void)
{

#if defined(_MX6SL_) || defined(_MX6SLL_) || defined(_MX6ULL_)//[
	if(gptNtxGpioKey_Menu) {
		return ntx_gpio_key_is_down(gptNtxGpioKey_Menu);
	}
#endif //]_MX6SL_
	return 0;
}

int ntx_gpio_key_is_fl_down(void)
{
#if defined(_MX6SL_) || defined(_MX6SLL_) || defined(_MX6ULL_)//[
	if(gptNtxGpioKey_FL) {
		return ntx_gpio_key_is_down(gptNtxGpioKey_FL);
	}
#endif //]
	return 0;
}

int ntx_gpio_key_is_pgdn_down(void)
{
	if(gptNtxGpioKey_PGDN) {
		return ntx_gpio_key_is_down(gptNtxGpioKey_PGDN);
	}
	return 0;
}

int ntx_gpio_key_is_pgup_down(void)
{
	if(gptNtxGpioKey_PGUP) {
		return ntx_gpio_key_is_down(gptNtxGpioKey_PGUP);
	}
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
	case 3:
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
static unsigned int guiRicohRC5T619_I2C_bus=2;// I2C3 .

int RC5T619_I2C_Chn_set(unsigned int uiI2C_Chn)
{
	switch(uiI2C_Chn) {
	case 0:// I2C1 .
	case 1:// I2C2 .
	case 2:// I2C3 .
	case 3:// I2C4 .
		guiRicohRC5T619_I2C_bus = uiI2C_Chn;
		break;
	default:
		printf("%s invalid I2C channel #%u!!\n",__FUNCTION__,uiI2C_Chn);
		return -1;
	}
	return 0;
}

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
//		printf("RC5T619 [0x%x]=0x%x\n",bRegAddr,bRegVal);
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

#if 0 // debug .
	{
		unsigned short w;
		printf("%s():reg=0x%x,data=\n ",__FUNCTION__,bRegAddr);
		for(w=0;w<I_wRegWrBufBytes;w++) {
			printf("0x%x,",I_pbRegWrBuf[w]);
		}
		printf("\n");
	}
#endif 

	iChk = i2c_write(gbRicohRC5T619_ChipAddr, bRegAddr, 1, I_pbRegWrBuf, I_wRegWrBufBytes);
	if(0==iChk) {
		iRet=(int)I_wRegWrBufBytes;
	}
	else {
		printf("RC5T619 write to [0x%x] %d bytes failed !!\n",bRegAddr,I_wRegWrBufBytes);
		iRet=iChk ;
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

int RC5T619_set_charger_params(int iChargerType)
{
	unsigned char val;
	if(1!=gptNtxHwCfg->m_val.bPMIC) {
		//printf("%s():skipped ,RC5T619 disabled by hwconfig",__FUNCTION__);
		return -3;
	}

	RC5T619_read_reg (0xBD, &val);
	if (0 == (val & 0xC0))
		return 0;

	switch (iChargerType) {
	default:
	case USB_CHARGER_SDP:
		{
			int retry_cnt = 10;
			unsigned char ilim=8;		// set ILIM 900mA

			if (49 == gptNtxHwCfg->m_val.bPCB)
				ilim = 5;	// set ILIM 600mA for E60QDx
			RC5T619_write_reg (0xB6, ilim);	// REGISET1 , set ILIM_ADP
			RC5T619_write_reg (0xB8, 0x04);	// CHGISET , set ICHG 500mA
			RC5T619_write_reg (0xB7, (0xE0|ilim));	// REGISET2 , set ILIM_USB
			do {
				RC5T619_write_reg (0xB7, (0xE0|ilim));	// REGISET2 , set ILIM_USB
				RC5T619_read_reg (0xB7, &val);
				if (ilim != (val&0x0F))
					printf ("REGISET2 val %02X\n",val);
			} while ((val != (val&0x0F)) && (--retry_cnt));
		}
		break;
	case USB_CHARGER_CDP:
	case USB_CHARGER_DCP:
		//printf ("%s : set 900mA for DCP/CDP \n",__func__);
		RC5T619_write_reg (0xB6, 0x09);	// REGISET1 , set ILIM_ADP 1000mA
		RC5T619_write_reg (0xB7, 0x29);	// REGISET2 , set ILIM_USB 1000mA
		RC5T619_write_reg (0xB8, 0x07);	// CHGISET , set ICHG 800mA
		break;
	}
	return 0;
}

int RC5T619_charger_redetect(void)
{
	int iRet=0;
	int iChk;
	unsigned char bRegAddr,bVal;

	if(1!=gptNtxHwCfg->m_val.bPMIC) {
		//printf("%s():skipped ,RC5T619 disabled by hwconfig",__FUNCTION__);
		return -3;
	}
 
	bRegAddr = 0xDA;
	bVal = 0x01;
	iChk = RC5T619_write_reg(bRegAddr,bVal);
	if(iChk<0) {
		printf("%s():write reg0x%x->0x%x error %d",
				__FUNCTION__,bRegAddr,bVal,iChk);
		return -1;
	}

	return iRet;
}

int RC5T619_restart(void)
{
	int iRet=0;
	int iChk;
	unsigned char bRegAddr,bVal;

	if(1!=gptNtxHwCfg->m_val.bPMIC) {
		//printf("%s():skipped ,RC5T619 disabled by hwconfig",__FUNCTION__);
		return -3;
	}

	printf("%s():restarting system ...\n",__FUNCTION__);

	bRegAddr = 0x0f; // RICOH61x_PWR_REP_CNT .
	bVal = 0x35;// 500ms off time .
	iChk = RC5T619_write_reg(bRegAddr,bVal);
	if(iChk<0) {
		printf("%s():write reg0x%x->0x%x error %d",
				__FUNCTION__,bRegAddr,bVal,iChk);
		return -1;
	}

	bRegAddr = 0x0e; // RICOH61x_PWR_SLP_CNT .
	bVal = 0x1; 
	iChk = RC5T619_write_reg(bRegAddr,bVal);
	if(iChk<0) {
		printf("%s():write reg0x%x->0x%x error %d",
				__FUNCTION__,bRegAddr,bVal,iChk);
		return -1;
	}

	return iRet;
}


int RC5T619_watchdog(int I_iCmd,int I_iTimeout)
{
	int iRet=0;
	int iChk;
	unsigned char bWDG_settings,bPWRIREN,bPWRIRQ,bREPCNT ;
	static unsigned char gbRptON_Setup=0x05 ;//RICOH61x_PWR_REP_CNT , 500ms off time .
	const int iRetryMax=10;
	int iRetryCnt;



	if(1!=gptNtxHwCfg->m_val.bPMIC) {
		//printf("%s():skipped ,RC5T619 disabled by hwconfig",__FUNCTION__);
		return -3;
	}

	printf("%s(%d,%d)\n",__FUNCTION__,I_iCmd,I_iTimeout);
	

	iChk = RC5T619_read_reg(0x0b,&bWDG_settings); // WATCHDOG setting register .
	if(iChk<0) {
		printf("%s():read reg0x0b error %d",
				__FUNCTION__,iChk);
		return -1;
	}
	//printf("WATCHDOG=0x%x\n",bWDG_settings);

	iChk = RC5T619_read_reg(0x12,&bPWRIREN); // PWRIREN
	if(iChk<0) {
		printf("%s():read reg0x12 error %d",
				__FUNCTION__,iChk);
		return -1;
	}
	//printf("PWRIREN=0x%x\n",bPWRIREN);
	
	iChk = RC5T619_read_reg(0x13,&bPWRIRQ); // PWRIRQ
	if(iChk<0) {
		printf("%s():read reg0x13 error %d",
				__FUNCTION__,iChk);
		return -1;
	}
	//printf("PWRIRQ=0x%x\n",bPWRIRQ);

	iChk = RC5T619_read_reg(0x0f,&bREPCNT); // REPCNT
	if(iChk<0) {
		printf("%s():read reg0x0f error %d",
				__FUNCTION__,iChk);
		return -1;
	}
	//printf("REPCNT=0x%x\n",bREPCNT);
	
	// RICOH61x_PWR_WD
	if(1==I_iCmd) {
		// enable watchdog .
		bWDG_settings &= ~0x3;
		if(I_iTimeout>=128) {
			bWDG_settings |= 0x11; // 32secs count down watchdog .
		}
		else if(I_iTimeout>=32) {
			bWDG_settings |= 0x2; // 32secs count down watchdog .
		}
		else if(I_iTimeout>=8) {
			bWDG_settings |= 0x1; // 8secs count down watchdog .
		}
		else {
			//bWDG_settings |= 0x0; // 1secs count down watchdog .
		}

		bWDG_settings |= 0xC; // watchdog enable .
		iChk = RC5T619_write_reg(0x0b,bWDG_settings);
		if(iChk<0) {
			printf("%s():write reg0x0b -> 0x%x, error %d",
				__FUNCTION__,bWDG_settings,iChk);
		}

		bPWRIREN |= 0x40;
		iChk = RC5T619_write_reg(0x12,bPWRIREN);
		if(iChk<0) {
			printf("%s():write reg0x12 error(%d)",
				__FUNCTION__,iChk);
		}

		
		for(iRetryCnt=0;iRetryCnt<iRetryMax;iRetryCnt++) 
		{

			iChk = RC5T619_write_reg(0x0f,gbRptON_Setup); // 
			if(iChk<0) {
				printf("%s():write reg0xf error(%d)",
					__FUNCTION__,iChk);
				continue;
			}
			iChk = RC5T619_read_reg(0x0f,&bREPCNT);
			if(iChk<0) {
				printf("%s():read reg0xf error %d",
					__FUNCTION__,iChk);
				continue;
			}

			if (bREPCNT==gbRptON_Setup) {
				break;
			}

			printf("RICOH61x_PWR_REP_CNT set to 0x%x failed , retry %d/%d... \n",gbRptON_Setup,iRetryCnt,iRetryMax);
			udelay(10*1000);
		} 

	}
	else if(0==I_iCmd) {
		// disable watchdog .
		bWDG_settings &= ~0x0C;
		if(RC5T619_write_reg(0x0b,bWDG_settings)<0) {
			printf("%s():write reg0xb error",
				__FUNCTION__);
		}
		bPWRIREN &= ~0x40;
		if(RC5T619_write_reg(0x12,bPWRIREN)<0) {
			printf("%s():write reg0x12 error",
				__FUNCTION__);
		}
	}
	else if(2==I_iCmd) {
		// touch/clear watchdog . 
		bPWRIRQ &= ~0x40;
		if(RC5T619_write_reg(0x13,bPWRIRQ)<0) {
			printf("%s():write reg0x13 error",
				__FUNCTION__);
		}
	}
	else if(3==I_iCmd) {
		// power off action . 
		bREPCNT &= ~0x7; // REPWRTIM , RE PWRON field clear . 
		if(I_iTimeout>=1000) {
			bREPCNT |= 0x6; // 1S repeat on . 
		}
		else if(I_iTimeout>=500) {
			bREPCNT |= 0x4; // 500ms repeat on . 
		}
		else if(I_iTimeout>=100) {
			bREPCNT |= 0x2; // 100ms repeat on . 
		}
		else {
			//bREPCNT |= 0; // 10ms repeat on . 
		}

		if(I_iTimeout>0) {
			bREPCNT |= 1;
		}
		
		iChk = RC5T619_write_reg(0x0f,bREPCNT);
		if(iChk<0) {
			printf("%s():write reg0xf error %d",
				__FUNCTION__,iChk);
			iRet = -2;
		}
		else {
			gbRptON_Setup = bREPCNT;
		}
	}
	else if(4==I_iCmd) {
		// watchdog status .
		printf("WATCHDOG=0x%x\n",bWDG_settings);
		printf("PWRIREN=0x%x\n",bPWRIREN);
		printf("PWRIRQ=0x%x\n",bPWRIRQ);
		printf("REPCNT=0x%x\n",bREPCNT);
	}

	return iRet;
}
#define RC5T619_DCDC1		1
#define RC5T619_DCDC3		3
#define RC5T619_MIN_UV  600000	// 0.6V
#define RC5T619_MAX_UV  3500000 // 3.5V
#define RC5T619_UV_STEP  12500 // 12.5mV
static int RC5T619_uV_to_reg(int uV,unsigned char *O_bReg)
{
	int iRet = 0;

	if(uV<RC5T619_MIN_UV) {
		printf("invalid ricoh voltage %duV < %duV\n",uV,RC5T619_MIN_UV);
		iRet = -1;goto err;
	}
	if(uV>RC5T619_MAX_UV) {
		printf("invalid ricoh voltage %duV > %duV\n",uV,RC5T619_MAX_UV);
		iRet = -1;goto err;
	}

	if(O_bReg) {
		*O_bReg = (unsigned char)((uV-RC5T619_MIN_UV)/RC5T619_UV_STEP);
	}

err:
	return iRet;
}

int RC5T619_regulator_voltage_set(int iRegulatorID,int uV)
{
	int iRet = 0;
	unsigned char bVReg;

	if(RC5T619_uV_to_reg(uV,&bVReg)<0) {
		iRet = -1;goto err;
	}

	switch(iRegulatorID) {
	case RC5T619_DCDC1: // VDD_SOC_IN
		iRet = RC5T619_write_reg(0x36,bVReg);
		break;
	case RC5T619_DCDC3: // VDD_ARM_IN
		iRet = RC5T619_write_reg(0x38,bVReg);
		break;
	default:
		iRet = -2;
		break;
	}
err:
	return iRet;
}

void RC5T619_regulator_init(void)
{

#if defined(_MX6SLL_)
	int VDD_SOC_IN_uV = 1275000;
	int VDD_ARM_IN_uV = 1275000;
	printf("%s : VDD_SOC_IN : %duV, VDD_ARM_IN : %duV\n",
			__FUNCTION__,VDD_SOC_IN_uV,VDD_ARM_IN_uV);
	RC5T619_regulator_voltage_set(RC5T619_DCDC1,VDD_SOC_IN_uV);// VDD_SOC_IN;
	RC5T619_regulator_voltage_set(RC5T619_DCDC3,VDD_ARM_IN_uV);// VDD_ARM_IN;
#endif

}
#else //][!PMIC_RC5T619
int RC5T619_read_reg(unsigned char bRegAddr,unsigned char *O_pbRegVal){return -1;}
int RC5T619_write_reg(unsigned char bRegAddr,unsigned char bRegWrVal){return -1;}
int RC5T619_write_buffer(unsigned char bRegAddr,unsigned char *I_pbRegWrBuf,unsigned short I_wRegWrBufBytes){return -1;}
int RC5T619_watchdog(int cmd,int secs){return -1;}
int RC5T619_charger_redetect(void){return -1;}
int RC5T619_I2C_Chn_set(unsigned int uiI2C_Chn) {return -1;}
void RC5T619_regulator_init(void){}
int RC5T619_regulator_voltage_set(int iRegulatorID,int uV){return -1;}
#endif //] PMIC_RC5T619


int ntx_detect_usb_plugin(int iIsDetectChargerType) 
{
	int iRet=0;
	if(1==gptNtxHwCfg->m_val.bPMIC) {
#ifdef PMIC_RC5T619 //[
		unsigned char bReg,bRegAddr;
		int iChk;
		int iTryCnt=0;
		const int iTryMax=50;


		// Ricoh RC5T619 .
		bRegAddr=0xbd;
		iChk = RC5T619_read_reg(bRegAddr,&bReg);
		if(iChk>=0) {
			if(bReg&0xc0) 
			{
				iRet = USB_CHARGER_UNKOWN;

				if(iIsDetectChargerType) 
				{
					RC5T619_charger_redetect();
					printf("%s():Charge detecting ...",__FUNCTION__);
					do {
						if(++iTryCnt>=iTryMax) {
							printf("retry(%d) timed out\n",iTryCnt);
							break;
						}

						iChk = RC5T619_read_reg(0xDA,&bReg);	// GCHGDET_REG

						if(0x8!=(bReg&0xc)) {
							// Detecting not completely .
							udelay(10*1000);
							printf(".");
							continue ;
						}

						if (bReg & 0x30) {
							if((bReg & 0x30)==0x01) {
								printf ("set 900mA for CDP (%d)\n",iTryCnt);
								RC5T619_set_charger_params(USB_CHARGER_CDP);
								iRet = USB_CHARGER_CDP;
							}
							else {
								printf ("set 900mA for DCP (%d)\n",iTryCnt);
								RC5T619_set_charger_params(USB_CHARGER_DCP);
								iRet = USB_CHARGER_DCP;
							}
						}
						else {
							printf ("set 500mA for SDP (0x%02x)(%d)\n",bReg,iTryCnt);
							RC5T619_set_charger_params(USB_CHARGER_SDP);
							iRet = USB_CHARGER_SDP;
						}
						break;
					}while(1);

				}// Detecting Charger type ...
			}
		}
#endif //]PMIC_RC5T619
	}
	else {
		if(gpt_ntx_gpio_ACIN) {
			iRet = ntx_gpio_get_value(gpt_ntx_gpio_ACIN)?0:1;
		}
		else {
			iRet = 0;
		}
	}

	if(49==gptNtxHwCfg->m_val.bPCB && gptNtxHwCfg->m_val.bPCB_REV>=0x20) {
		// E60QDXA2
		// detect the usb id pin .
		if(ntxhw_USBID_detected()) {
			iRet |= USB_CHARGER_OTG;
		}
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

int EPDPMIC_isPowerGood(void)
{
	int iRet;
	iRet = ntx_gpio_get_value(&gt_ntx_gpio_EPDPMIC_PWRGOOD)?1:0;
	//printf("%s() = %d\n",__FUNCTION__,iRet);
	return iRet;
}
 
void FP9928_rail_power_onoff(int iIsON)
{
	if(gpt_ntx_gpio_FP9928_RAIL_ON) {
		ntx_gpio_set_value(gpt_ntx_gpio_FP9928_RAIL_ON,iIsON?1:0);
	}
}

void _init_tps65185_power(int iIsWakeup,int iIsActivePwr)
{

#ifdef _MX6Q_//[
#else //][!_MX6Q_
	udelay(200);
	printf("@@@ init_tps_65185_WAKEUP @@@\r\n ");
	if(gpt_ntx_gpio_65185_WAKEUP) {
		ntx_gpio_set_value(gpt_ntx_gpio_65185_WAKEUP,iIsWakeup);
	}

	printf("@@@ init_tps_65185_PWRUP @@@\r\n ");
	if(gpt_ntx_gpio_65185_PWRUP) {
		ntx_gpio_set_value(gpt_ntx_gpio_65185_PWRUP,iIsActivePwr);
		if(iIsActivePwr) {
			udelay(2*1000);
		}
	}

#endif //]_MX6Q_
}

void tps65185_rail_power_onoff(int iIsON)
{
	if(iIsON) {
		if(gpt_ntx_gpio_65185_WAKEUP) {
			if(1!=ntx_gpio_get_current_value(gpt_ntx_gpio_65185_WAKEUP)) {
				printf("%s() TPS65185 wakeup\n",__FUNCTION__);
				ntx_gpio_set_value(gpt_ntx_gpio_65185_WAKEUP,1);
				udelay(2000);
			}
		}
		if(gpt_ntx_gpio_65185_PWRUP) {
			if(1!=ntx_gpio_get_current_value(gpt_ntx_gpio_65185_PWRUP)) {
				printf("%s() TPS65185 Enable RAIL POWER\n",__FUNCTION__);
				ntx_gpio_set_value(gpt_ntx_gpio_65185_PWRUP,1);
				udelay(2000);
			}
		}
	}
	else {

		if(gpt_ntx_gpio_65185_PWRUP) {
			if(0!=ntx_gpio_get_current_value(gpt_ntx_gpio_65185_PWRUP)) {
				printf("%s() TPS65185 Disable RAIL POWER\n",__FUNCTION__);
				ntx_gpio_set_value(gpt_ntx_gpio_65185_PWRUP,0);
				udelay(2000);
			}
		}

		if(gpt_ntx_gpio_65185_WAKEUP) {
			if(0!=ntx_gpio_get_current_value(gpt_ntx_gpio_65185_WAKEUP)) {
				printf("%s() TPS65185 sleep\n",__FUNCTION__);
				ntx_gpio_set_value(gpt_ntx_gpio_65185_WAKEUP,0);
				udelay(2000);
			}
		}
	}
}

static void ntx_io_assign(void)
{
	// assigns power key .
	if(!gptNtxHwCfg) {
		printf("warning : ntxhwcfg not exist !\n");
	}
	gptNtxGpioKey_Power = &gt_ntx_gpio_power_key;
	#if defined(_MX6SLL_)
	if (NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bPCB_Flags2,0)) 
		gptNtxGpioKey_Power = &gt_ntx_gpio_power_key_2;
	#endif

	// assigns home key .
	if(11==gptNtxHwCfg->m_val.bKeyPad || 12==gptNtxHwCfg->m_val.bKeyPad ) {
		// KeyPad is FL_Key/NO_Key .
		gptNtxGpioKey_Home = 0;
	}
	else {
		gptNtxGpioKey_Home = &gt_ntx_gpio_home_key;
	}

	// assigns frontlight key .
	if(12==gptNtxHwCfg->m_val.bKeyPad||14==gptNtxHwCfg->m_val.bKeyPad||
		17==gptNtxHwCfg->m_val.bKeyPad||18==gptNtxHwCfg->m_val.bKeyPad||
		22==gptNtxHwCfg->m_val.bKeyPad) 
	{
		// Keypad is NO_Key || RETURN+HOME+MENU || HOMEPAD || HOME_Key || LEFT+RIGHT+HOME+MENU
		gptNtxGpioKey_FL = 0;
	}

	// assigns page up/down key . 
	if(26==gptNtxHwCfg->m_val.bKeyPad) {
#if defined(_MX6SLL_) //[
		gptNtxGpioKey_PGUP = &gt_ntx_gpio_pgup_key;
		gptNtxGpioKey_PGDN = &gt_ntx_gpio_pgdn_key;
#endif //] _MX6SLL_
	}

	if(0!=gptNtxHwCfg->m_val.bHallSensor) {

		gptNtxGpioSW_HallSensor= &gt_ntx_gpio_hallsensor_key;
#if defined(_MX6SLL_) //[
		if(NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bPCB_Flags2,0)) {
			// EMMC@SD1(I/O@SD4).
			gptNtxGpioSW_HallSensor= &gt_ntx_gpio_hallsensor_key2;
		}
#endif //](_MX6SLL_)
	}
	else {
		//printf("no hallsensor !\n");
		gptNtxGpioSW_HallSensor=0;
	}

#if 0
	if(17==gptNtxHwCfg->m_val.bKeyPad) 
	{
		// keypad is 'RETURN+HOME+MENU'
		gptNtxGpioKey_Menu = &gt_ntx_gpio_menu_key;
		gptNtxGpioKey_Return = &gt_ntx_gpio_return_key ;
	}

	if(22==gptNtxHwCfg->m_val.bKeyPad) 
	{
		// keypad is 'LEFT+RIGHT+HOME+MENU'
		gptNtxGpioKey_Menu = &gt_ntx_gpio_menu_key_gp3_25;
		gptNtxGpioKey_Left = &gt_ntx_gpio_left_key;
		gptNtxGpioKey_Right = &gt_ntx_gpio_right_key;
	}
#endif


	gpt_ntx_gpio_LED_G=&gt_ntx_gpio_LED_G;
	gpt_ntx_gpio_LED_R=&gt_ntx_gpio_LED_R;
	gpt_ntx_gpio_LED_B=&gt_ntx_gpio_LED_B;
	#if defined(_MX6SLL_)
	// assigns Green LED .
	if (NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bPCB_Flags2,0)) {
		if(75==gptNtxHwCfg->m_val.bPCB) {
			//E80K00 .
			gpt_ntx_gpio_LED_G=&gt_ntx_gpio_LED_R2;
		}
		else {
			gpt_ntx_gpio_LED_R=&gt_ntx_gpio_LED_R2;
			gpt_ntx_gpio_LED_G=&gt_ntx_gpio_LED_G2;
			gpt_ntx_gpio_LED_B=&gt_ntx_gpio_LED_B2;
		}
	}
	#endif

	// assigns WIFI 3V3 .
	gpt_ntx_gpio_WIFI_3V3_ON = &gt_ntx_gpio_WIFI_3V3_ON;


	gpt_ntx_gpio_CT_RST = &gt_ntx_gpio_TPRST;
	#if defined(_MX6SLL_)//[
	if (NTXHWCFG_TST_FLAG(gptNtxHwCfg->m_val.bPCB_Flags2,0)) {
		gpt_ntx_gpio_CT_RST = &gt_ntx_gpio_TPRST2;
	}
	#endif //]

	// assigns Touch RST .
	if(4==gptNtxHwCfg->m_val.bTouchType) {
		// IR touch type ...
		gpt_ntx_gpio_TP_RST = gpt_ntx_gpio_IR_RST;
	}
	else if (3==gptNtxHwCfg->m_val.bTouchType) {
		// C touch type ...
		gpt_ntx_gpio_TP_RST = gpt_ntx_gpio_CT_RST;
	}


	gpt_ntx_gpio_EP_3V3_ON = &gt_ntx_gpio_EPDPMIC_VIN ;
	gpt_ntx_gpio_EP_VCOM_ON = &gt_ntx_gpio_EPDPMIC_VCOM ;

	//gpt_ntx_gpio_FLPWR = ;
	//gpt_ntx_gpio_FP9928_RAIL_ON	= ;
	
#ifdef PMIC_RC5T619 //[
	// Ricoh PMIC IO assign ...
#if defined(_MX6ULL_) //[
	RC5T619_I2C_Chn_set(3);
#endif //] _MX6ULL_

#endif //]PMIC_RC5T619

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
	//EPDPMIC_power_on(0);
	
	printf("%s() %d\n",__FUNCTION__, is_boot_from_usb());
	//udc_init();
	if(!gptNtxHwCfg) {
		if (is_boot_from_usb()) {
			gptNtxHwCfg = 0x83808000;
			return;
		}
		_load_isd_hwconfig();
	}

	ntx_io_assign();

	RC5T619_write_reg(0x11, 8);	// disable N_OE
	RC5T619_regulator_init();
	RC5T619_watchdog(1,32);
	if(gpt_ntx_gpio_FLPWR) {
		ntx_gpio_init(gpt_ntx_gpio_FLPWR);
	}


	if(gpt_ntx_gpio_TP_RST) {
		ntx_gpio_init(gpt_ntx_gpio_TP_RST);
	}

	EPDPMIC_power_on(0);

	if(11==gptNtxHwCfg->m_val.bDisplayCtrl) {
		_init_tps65185_power(0,0);
	}

}

static void ntx_keys_setup(void)
{

	ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_Power;
	gi_ntx_gpio_keys++;
	if(12==gptNtxHwCfg->m_val.bKeyPad) {
		// NO_Key .
	}
	else 
	{


		if(gptNtxGpioKey_Home && 
				(13==gptNtxHwCfg->m_val.bKeyPad||
				 14==gptNtxHwCfg->m_val.bKeyPad||
				 16==gptNtxHwCfg->m_val.bKeyPad||
				 17==gptNtxHwCfg->m_val.bKeyPad||
				 18==gptNtxHwCfg->m_val.bKeyPad)) 
		{
			// FL+HOME/HOME_Key/FL+HOMEPAD/RETURN+HOME+MENU/HOMEPAD
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_Home;
			gi_ntx_gpio_keys++;
		}

		if(gptNtxGpioKey_FL && 
			(11==gptNtxHwCfg->m_val.bKeyPad||
			13==gptNtxHwCfg->m_val.bKeyPad||
			16==gptNtxHwCfg->m_val.bKeyPad)) 
		{
			// FL_Key/FL+HOME/FL+HOMEPAD
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_FL;
			gi_ntx_gpio_keys++;
		}

		if(gptNtxGpioSW_HallSensor && 0!=gptNtxHwCfg->m_val.bHallSensor)
		{
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioSW_HallSensor;
			gi_ntx_gpio_keys++;
		}

		if(gptNtxGpioKey_Menu) 
		{
			// RETURN+HOME+MENU
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_Menu;
			gi_ntx_gpio_keys++;
		}

		if(gptNtxGpioKey_PGUP) 
		{
			// PGUP
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_PGUP;
			gi_ntx_gpio_keys++;
		}

		if(gptNtxGpioKey_PGDN) 
		{
			// PGDN
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_PGDN;
			gi_ntx_gpio_keys++;
		}

		if(gptNtxGpioKey_Return) 
		{
			// RETURN+HOME+MENU
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_Return;
			gi_ntx_gpio_keys++;
		}

		if(gptNtxGpioKey_Left) 
		{
			// 
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_Left;
			gi_ntx_gpio_keys++;
		}

		if(gptNtxGpioKey_Right) 
		{
			// 
			ntx_gpio_keysA[gi_ntx_gpio_keys] = gptNtxGpioKey_Right;
			gi_ntx_gpio_keys++;
		}
		
	}


	if(gi_ntx_gpio_keys>=NTX_GPIO_KEYS) {
		printf("\n\n\n%s(%d) memory overwrite !!!\n\n\n",__FILE__,__LINE__);
		udelay(1000*1000);
	}
}

void ntx_hw_late_init(void) 
{
	//NTX_GPIO *pt_gpio;
	int i;

	printf("%s()\n",__FUNCTION__);
	if(!gptNtxHwCfg) {
		_load_isd_hwconfig();
	}

	_load_ntx_sn();


	ntx_parse_syspart_type();

	EPDPMIC_power_on(1);
	if(11==gptNtxHwCfg->m_val.bDisplayCtrl) {
		_init_tps65185_power(1,0);
	}

	ntx_keys_setup();

#if defined(_MX6Q_) //[
#else //][!


	if(9==gptNtxHwCfg->m_val.bCustomer) {
		_led_R(1);
		_led_G(1);
		_led_B(1);
	}
	else {
		_led_R(0);
		_led_G(1);
		_led_B(0);
	}

  

	_sd_cd_status();


	gpt_ntx_gpio_USBID = 0;

#if defined(_MX6SL_) || defined(_MX6SLL_) || defined(_MX6ULL_)//[
	if(gpt_ntx_gpio_LED_HOME) {
		ntx_gpio_init(gpt_ntx_gpio_LED_HOME);
	}

	// setup I2C chanel .
	if( (46==gptNtxHwCfg->m_val.bPCB && gptNtxHwCfg->m_val.bPCB_REV>=0x10) ||
			48==gptNtxHwCfg->m_val.bPCB ||
			50==gptNtxHwCfg->m_val.bPCB ||
	 		51==gptNtxHwCfg->m_val.bPCB	||
			55==gptNtxHwCfg->m_val.bPCB)
	{
		// >=E60Q9XA1 or =E60QAX | E60QFX | E60QHX |E70Q02 .
		msp430_I2C_Chn_set(0); // MSP430 @ I2C1 .
	}

	if(3==gptNtxHwCfg->m_val.bUIConfig) {
		// MFG mode .
		RC5T619_set_charger_params(USB_CHARGER_CDP);
	}
	else {
		if (49 == gptNtxHwCfg->m_val.bPCB) {	// run charger detect for E60QDx
			RC5T619_write_reg(0xBA, 0x88);	// set CHGPON to 3.2V
			RC5T619_write_reg(0xB4, 0x20);	// set USB_VCONTMASK

			RC5T619_charger_redetect();
			ntx_detect_usb_plugin (1);
		}
		else
			RC5T619_set_charger_params(USB_CHARGER_SDP);
	}

#endif//] _MX6SL_ || _MX6SLL_ || _MX6ULL_

#endif //]
}


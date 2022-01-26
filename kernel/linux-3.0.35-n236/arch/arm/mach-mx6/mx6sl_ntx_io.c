#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/input.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/iomux-mx6sl.h>
#include <asm/uaccess.h>
#include <asm/system.h>


#include <generated/autoconf.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/irq.h>
#include <linux/freezer.h>

#include <mach/common.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <mach/iomux-v3.h>


#include <linux/mfd/ricoh619.h>
#include <linux/rtc/rtc-ricoh619.h>
#include <linux/power/ricoh619_battery.h>
#include <linux/regulator/ricoh619-regulator.h>

#define GDEBUG 0
#include <linux/gallen_dbg.h>

//#define GPIOFN_PWRKEY	1

//#ifdef GPIOFN_PWRKEY//[
	#include "../../../drivers/input/keyboard/gpiofn.h"
//#endif //]GPIOFN_PWRKEY

#include "../../../drivers/video/mxc/lk_tps65185.h"
#include "../../../drivers/video/mxc/lk_fp9928.h"
#include "../../../drivers/misc/ntx-misc.h"


//#define _WIFI_ALWAYS_ON_	// wifi always on for startic

#include "board-mx6sl_ntx.h"
#include "ntx_hwconfig.h"
#include "ntx_firmware.h"

#define		DEVICE_NAME 		"ntx_io"		// "pvi_io"
#define		DEVICE_MINJOR		190

#define  CM_PLATFORM		164
#define  CM_HWCONFIG		165
#define  CM_SET_HWCONFIG	166

#define	CM_SD_IN				117
#define	AC_IN					118
#define CM_PWR_ON2				112
#define CM_AUDIO_PWR			113
#define	CM_POWER_BTN			110
#define CM_USB_Plug_IN			108
#define CM_AC_CK				109
#define CM_CHARGE_STATUS		204
#define	CM_nLED					101
#define	CM_nLED_CPU				102
#define	POWER_OFF_COMMAND		0xC0	// 192
#define	SYS_RESET_COMMAND 		193		// Joseph 091223
#define	GET_LnBATT_CPU			0XC2		// 194
#define	GET_VBATT_TH			0XC3	// 195
#define	CM_SIGUSR1				104
//kay 20081110 for detecting SD write protect
#define	CM_SD_PROTECT			120
#define	SYS_AUTO_POWER_ON 		0xC4	// 196 Joseph 120620
                            	
//20090216 for detecting controller
#define	CM_CONTROLLER			121

//20090416 for detecting controller
#define	CM_USB_AC_STATUS		122
#define	CM_RTC_WAKEUP_FLAG		123
#define	CM_SYSTEM_RESET			124
#define	CM_USB_HOST_PWR			125
#define	CM_BLUETOOTH_PWR		126
#define	CM_TELLPID				99	
#define CM_LED_BLINK 			127
#define CM_TOUCH_LOCK 			128
#define CM_DEVICE_MODULE 		129
#define CM_BLUETOOTH_RESET 		130
#define CM_DEVICE_INFO 			131

//Joseph 091211 for 3G
#define CM_3G_POWER 			150
#define CM_3G_RF_OFF 			151
#define CM_3G_RESET 			152
#define CM_3G_GET_WAKE_STATUS	153

//Joseph 091209
#define	CM_ROTARY_STATUS 		200	
#define	CM_GET_KEY_STATUS 		201	
#define	CM_GET_WHEEL_KEY_STATUS 202
#define CM_GET_KL25_STATUS		203	
#define CM_GET_KL25_ACTION		199	
#define	POWER_KEEP_COMMAND 		205	
#define	CM_GET_BATTERY_STATUS 	206	
#define	CM_SET_ALARM_WAKEUP	 	207	
#define	CM_WIFI_CTRL	 		208	
#define	CM_ROTARY_ENABLE 		209	
#define	CM_USB_ID_STATUS 		210

#define CM_GET_UP_VERSION 		215

// gallen 100621
// Audio functions ...
#define CM_AUDIO_GET_VOLUME		230
#define CM_AUDIO_SET_VOLUME		240
#define CM_FL_LM3630_SET		239
#define CM_FRONT_LIGHT_SET		241
#define CM_FRONT_LIGHT_AVAILABLE	242
#define CM_FL_MSP430_R_DUTY		243
#define CM_FL_MSP430_FREQUENCY		244
#define CM_FRONT_LIGHT_R_EN		245
#define CM_FL_HT68F20_SETDUTY		246
#define CM_FL_HT68F20_GETDUTY		247
#define CM_FL_LM3630_TABLE		248
#define CM_FL_MSP430_W_DUTY		249
#define CM_FL_MSP430_W_H_EN		250
#define CM_FL_MSP430_PWM_EN		251

#define CM_GET_KEYS				107


#ifdef GPIOFN_PWRKEY//[
static void power_key_chk(unsigned long v);

static int PWR_SW_func(int iGPIOVal)
{
	printk("[%s]\n",__FUNCTION__);
	power_key_chk(0);
}

static GPIODATA gtNTX_PWR_GPIO_data = {
	.pfnGPIO = PWR_SW_func,
	.uGPIO = gMX6SL_PWR_SW,
	.szName = "PWR_SW",
	.tPADCtrl = MX50_PAD_CSPI_MISO__GPIO_4_10,
	.uiIRQType = IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
	.iWakeup = 1,
};

#endif //]GPIOFN_PWRKEY


unsigned short  __EBRMAIN_PID__ = 0;

unsigned char __USB_ADAPTOR__=0;
EXPORT_SYMBOL(__USB_ADAPTOR__);

static int Driver_Count = -1;
unsigned char __TOUCH_LOCK__= 0;
int gSleep_Mode_Suspend;
int gWakeUpbyKL25;
static bool touch_under_resetting = 0;

extern volatile NTX_HWCONFIG *gptHWCFG;

extern volatile int giISD_3V3_ON_Ctrl ;

typedef enum __DEV_MODULE_NAME{
    EB500=0,
    EB600=1,
    EB600E=2,
    EB600EM=3,
    COOKIE=4,    
}__dev_module_name;

typedef enum __DEV_MODULE_CPU{
    CPU_S3C2410=0,
    CPU_S3C2440=1,
    CPU_S3C2416=2,
    CPU_CORETEX_A8=3,
    CPU_COOKIE=4,    
}__dev_module_cpu;

typedef enum __DEV_MODULE_CONTROLLER{
    CONTROLLER_PVI=0,
    CONTROLLER_EPSON=1,
    CONTROLLER_SW=2,
}__dev_module_controller;

typedef enum __DEV_MODULE_WIFI{
    WIFI_NONE=0,
    WIFI_MARVELL=1,
    WIFI_OTHER=2,
}__dev_module_wifi;

typedef enum __DEV_MODULE_BLUETOOTH{
    BLUETOOTH_NONE=0,
    BLUETOOTH_TI=1,
    BLUETOOTH_CSR=2,
}__devi_module_bluetooth;

struct ebook_device_info {
    char device_name;
    char cpu;
    char controller;
    char wifi;
    char bluetooth;
};


static volatile int giFL_ON=0; // low or high to turn on the FL .

static unsigned short FL_table0[100]={
0x0001,0x0006,0x0007,0x0009,0x000C,0x000D,0x000E,0x000F,0x0011,0x0012,
0x0014,0x0015,0x0017,0x0018,0x001A,0x001B,0x001C,0x001D,0x001F,0x0020,
0x0022,0x0023,0x0025,0x0027,0x0028,0x002A,0x002B,0x002D,0x002E,0x0030,
0x0031,0x0033,0x0035,0x0036,0x0038,0x0039,0x003B,0x003C,0x003E,0x0040,
0x0041,0x0043,0x0044,0x0046,0x0047,0x0049,0x004A,0x0051,0x0057,0x005D,
0x0063,0x006A,0x0070,0x0076,0x007C,0x0083,0x0089,0x008F,0x0095,0x009C,
0x00A2,0x00A8,0x00AE,0x00B5,0x00B9,0x00BB,0x00C1,0x00C7,0x00CE,0x00D4,
0x00DA,0x00E0,0x00E7,0x00ED,0x00F3,0x00F9,0x0100,0x0106,0x010C,0x0112,
0x0118,0x011F,0x0125,0x012B,0x0131,0x0138,0x013E,0x0144,0x014A,0x0151,
0x0157,0x015D,0x0163,0x016A,0x0170,0x0176,0x017C,0x0183,0x0189,0x018F
};

struct front_light_setting {
    unsigned short fl_r_en;
    unsigned short freq;
    unsigned short duty;
};

static struct front_light_setting FL_table[][100]={
{// TABLE1
{0,20000,3}, {0,20000,5}, {0,20000,7}, {0,20000,9}, {0,20000,11},
{0,20000,13}, {0,20000,15}, {0,20000,17}, {0,20000,19}, {0,20000,21}, 
{0,20000,23}, {0,20000,25}, {0,20000,27}, {0,20000,30}, {0,20000,33},
{0,20000,36}, {0,20000,39}, {0,20000,42}, {0,20000,46}, {0,20000,51},
{0,20000,54}, {0,20000,59}, {0,20000,63}, {0,20000,68}, {0,20000,71},
{0,20000,74}, {0,20000,87}, {0,20000,99}, {0,20000,112}, {0,20000,124},
{0,20000,137}, {0,20000,149}, {0,20000,162}, {0,20000,174}, {0,20000,185},
{0,20000,193}, {0,20000,206}, {0,20000,218}, {0,20000,231}, {0,20000,243}, 
{0,20000,256}, {1,20000,45}, {1,20000,46}, {1,20000,47}, {1,20000,48}, 
{1,20000,49}, {1,20000,50}, {1,20000,51}, {1,20000,53}, {1,20000,55},
{1,20000,56}, {1,20000,58}, {1,20000,60}, {1,20000,63}, {1,20000,65}, 
{1,20000,68}, {1,20000,70}, {1,20000,75}, {1,20000,78}, {1,20000,80}, 
{1,20000,85}, {1,20000,88}, {1,20000,90}, {1,20000,95}, {1,20000,100}, 
{1,20000,105}, {1,20000,110}, {1,20000,115}, {1,20000,120}, {1,20000,125}, 
{1,20000,130}, {1,20000,135}, {1,20000,140}, {1,20000,145}, {1,20000,150}, 
{1,20000,155}, {1,20000,160}, {1,20000,170}, {1,20000,180}, {1,20000,190}, 
{1,20000,200}, {1,20000,210}, {1,20000,220}, {1,20000,230}, {1,20000,240}, 
{1,20000,250}, {1,20000,260}, {1,20000,270}, {1,20000,280}, {1,20000,290}, 
{1,20000,300}, {1,20000,310}, {1,20000,320}, {1,20000,330}, {1,20000,340}, 
{1,20000,350}, {1,20000,360}, {1,20000,370}, {1,20000,380}, {1,20000,400}
},
{// TABLE2
{0,20000,  7 }, {0,20000,  7 }, {0,20000,  7 }, {0,20000,  7 }, {0,20000,  7 }, //  1%
{0,20000,  8 }, {0,20000,  8 }, {0,20000,  9 }, {0,20000,  9 }, {0,20000, 10 }, //
{0,20000, 15 }, {0,20000, 18 }, {0,20000, 21 }, {0,20000, 23 }, {0,20000, 25 }, //  11%
{0,20000, 35 }, {0,20000, 42 }, {0,20000, 50 }, {0,20000, 55 }, {0,20000, 60 }, //
{0,20000, 80 }, {0,20000,100 }, {0,20000,120 }, {0,20000,140 }, {0,20000,160 }, //  21%
{0,20000,200 }, {0,20000,240 }, {0,20000,280 }, {0,20000,320 }, {0,20000,350 }, //
{0,20000,360 }, {0,20000,370 }, {0,20000,385 }, {0,20000,390 }, {1,20000,  5 }, //  31%
{1,20000, 10 }, {1,20000, 15 }, {1,20000, 20 }, {1,20000, 25 }, {1,20000, 30 }, //
{1,20000, 35 }, {1,20000, 40 }, {1,20000, 43 }, {1,20000, 46 }, {1,20000, 50 }, //  41%
{1,20000, 52 }, {1,20000, 54 }, {1,20000, 56 }, {1,20000, 58 }, {1,20000, 60 }, //  
{1,20000, 62 }, {1,20000, 65 }, {1,20000, 69 }, {1,20000, 74 }, {1,20000, 80 }, //  51%
{1,20000, 84 }, {1,20000, 88 }, {1,20000, 92 }, {1,20000, 96 }, {1,20000,100 }, //  
{1,20000,106 }, {1,20000,112 }, {1,20000,118 }, {1,20000,124 }, {1,20000,130 }, //  61%
{1,20000,136 }, {1,20000,142 }, {1,20000,148 }, {1,20000,154 }, {1,20000,160 }, //  
{1,20000,168 }, {1,20000,176 }, {1,20000,184 }, {1,20000,192 }, {1,20000,200 }, //  71%
{1,20000,206 }, {1,20000,212 }, {1,20000,218 }, {1,20000,224 }, {1,20000,230 }, //  
{1,20000,238 }, {1,20000,246 }, {1,20000,254 }, {1,20000,262 }, {1,20000,270 }, //  81%
{1,20000,278 }, {1,20000,286 }, {1,20000,294 }, {1,20000,302 }, {1,20000,310 }, //  
{1,20000,318 }, {1,20000,326 }, {1,20000,334 }, {1,20000,342 }, {1,20000,350 }, //  91%
{1,20000,360 }, {1,20000,370 }, {1,20000,380 }, {1,20000,330 }, {1,20000,400 }
}, 
{// TABLE3
{0,10000,6},{0,10000,28},{0,10000,44},{0,10000,62},{0,10000,87},
{0,10000,100},{0,10000,112},{0,10000,128},{0,10000,147},{0,10000,153},
{0,10000,165},{0,10000,172},{0,10000,181},{0,10000,194},{0,10000,215},
{0,10000,225},{0,10000,247},{0,10000,265},{0,10000,287},{0,10000,490},
{0,10000,500},{0,10000,515},{0,10000,537},{0,10000,553},{0,10000,587},
{0,10000,618},{0,10000,681},{0,10000,737},{0,10000,799},{1,10000,6},
{1,10000,12},{1,10000,19},{1,10000,25},{1,10000,28},{1,10000,37},
{1,10000,44},{1,10000,53},{1,10000,56},{1,10000,62},{1,10000,69},
{1,10000,75},{1,10000,81},{1,10000,87},{1,10000,94},{1,10000,100},
{1,10000,103},{1,10000,109},{1,10000,116},{1,10000,122},{1,10000,128},
{1,10000,134},{1,10000,137},{1,10000,144},{1,10000,150},{1,10000,165},
{1,10000,178},{1,10000,187},{1,10000,197},{1,10000,206},{1,10000,215},
{1,10000,225},{1,10000,237},{1,10000,250},{1,10000,262},{1,10000,275},
{1,10000,287},{1,10000,300},{1,10000,312},{1,10000,325},{1,10000,337},
{1,10000,350},{1,10000,365},{1,10000,381},{1,10000,393},{1,10000,409},
{1,10000,425},{1,10000,440},{1,10000,456},{1,10000,471},{1,10000,484},
{1,10000,496},{1,10000,509},{1,10000,524},{1,10000,540},{1,10000,553},
{1,10000,568},{1,10000,584},{1,10000,599},{1,10000,615},{1,10000,631},
{1,10000,646},{1,10000,659},{1,10000,677},{1,10000,696},{1,10000,712},
{1,10000,731},{1,10000,746},{1,10000,765},{1,10000,784},{1,10000,799}
},
{// TABLE4
{0,20000,2}, {0,20000,5}, {0,20000,8}, {0,20000,12}, {0,20000,16},
{0,20000,22}, {0,20000,28}, {0,20000,36}, {0,20000,39}, {0,20000,45},
{0,20000,52}, {0,20000,56}, {0,20000,61}, {0,20000,67}, {0,20000,75},
{0,20000,81}, {0,20000,87}, {0,20000,94}, {0,20000,100}, {0,20000,106},
{0,20000,112}, {0,20000,119}, {0,20000,131}, {0,20000,137}, {0,20000,144},
{0,20000,156}, {0,20000,175}, {0,20000,188}, {0,20000,212}, {0,20000,231},
{0,20000,237}, {0,20000,250}, {0,20000,256}, {0,20000,262}, {0,20000,268},
{0,20000,275}, {0,20000,287}, {0,20000,293}, {0,20000,306}, {0,20000,331},
{0,20000,337}, {0,20000,350}, {0,20000,368}, {1,20000,67}, {1,20000,69},
{1,20000,72}, {1,20000,75}, {1,20000,81}, {1,20000,87}, {1,20000,94},
{1,20000,100}, {1,20000,106}, {1,20000,112}, {1,20000,119}, {1,20000,125},
{1,20000,131}, {1,20000,137}, {1,20000,144}, {1,20000,150}, {1,20000,156},
{1,20000,162}, {1,20000,169}, {1,20000,175}, {1,20000,181}, {1,20000,185},
{1,20000,188}, {1,20000,194}, {1,20000,200}, {1,20000,206}, {1,20000,212},
{1,20000,219}, {1,20000,225}, {1,20000,231}, {1,20000,237}, {1,20000,244},
{1,20000,250}, {1,20000,256}, {1,20000,262}, {1,20000,268}, {1,20000,275},
{1,20000,281}, {1,20000,287}, {1,20000,293}, {1,20000,300}, {1,20000,306},
{1,20000,312}, {1,20000,318}, {1,20000,325}, {1,20000,331}, {1,20000,337},
{1,20000,343}, {1,20000,350}, {1,20000,356}, {1,20000,362}, {1,20000,368},
{1,20000,375}, {1,20000,381}, {1,20000,387}, {1,20000,393}, {1,20000,400}
},
{// TABLE5
{0,20000,39},{0,20000,41},{0,20000,42},{0,20000,45},{0,20000,47},
{0,20000,48},{0,20000,50},{0,20000,53},{0,20000,56},{0,20000,59},
{0,20000,62},{0,20000,64},{0,20000,67},{0,20000,70},{0,20000,73},
{0,20000,75},{0,20000,81},{0,20000,87},{0,20000,94},{0,20000,100},
{0,20000,106},{0,20000,112},{0,20000,119},{0,20000,125},{0,20000,131},
{0,20000,137},{0,20000,144},{0,20000,150},{0,20000,156},{0,20000,162},
{0,20000,168},{0,20000,175},{0,20000,181},{0,20000,185},{0,20000,194},
{0,20000,200},{0,20000,206},{0,20000,212},{0,20000,219},{0,20000,225},
{0,20000,231},{0,20000,237},{0,20000,244},{0,20000,250},{1,20000,67},
{1,20000,70},{1,20000,73},{1,20000,75},{1,20000,81},{1,20000,87},
{1,20000,94},{1,20000,100},{1,20000,106},{1,20000,112},{1,20000,119},
{1,20000,125},{1,20000,131},{1,20000,137},{1,20000,144},{1,20000,150},
{1,20000,156},{1,20000,162},{1,20000,169},{1,20000,175},{1,20000,181},
{1,20000,188},{1,20000,194},{1,20000,200},{1,20000,206},{1,20000,212},
{1,20000,219},{1,20000,225},{1,20000,231},{1,20000,237},{1,20000,244},
{1,20000,250},{1,20000,256},{1,20000,262},{1,20000,268},{1,20000,275},
{1,20000,281},{1,20000,287},{1,20000,293},{1,20000,300},{1,20000,306},
{1,20000,312},{1,20000,318},{1,20000,325},{1,20000,331},{1,20000,337},
{1,20000,343},{1,20000,350},{1,20000,356},{1,20000,362},{1,20000,368},
{1,20000,375},{1,20000,381},{1,20000,387},{1,20000,393},{1,20000,400}
},
{// TABLE6
{0,20000,39},{0,20000,42},{0,20000,45},{0,20000,47},{0,20000,50},
{0,20000,53},{0,20000,59},{0,20000,62},{0,20000,64},{0,20000,67},
{0,20000,70},{0,20000,73},{0,20000,75},{0,20000,81},{0,20000,87},
{0,20000,94},{0,20000,100},{0,20000,106},{0,20000,112},{0,20000,119},
{0,20000,125},{0,20000,131},{0,20000,137},{0,20000,144},{0,20000,150},
{0,20000,156},{0,20000,162},{0,20000,168},{0,20000,175},{0,20000,181},
{0,20000,185},{0,20000,194},{0,20000,200},{0,20000,206},{0,20000,212},
{0,20000,219},{0,20000,225},{0,20000,231},{0,20000,237},{0,20000,244},
{0,20000,250},{0,20000,260},{0,20000,270},{0,20000,280},{1,20000,55},
{1,20000,58},{1,20000,63},{1,20000,68},{1,20000,75},{1,20000,81},
{1,20000,87},{1,20000,94},{1,20000,100},{1,20000,106},{1,20000,112},
{1,20000,119},{1,20000,125},{1,20000,131},{1,20000,137},{1,20000,144},
{1,20000,150},{1,20000,156},{1,20000,162},{1,20000,169},{1,20000,175},
{1,20000,181},{1,20000,188},{1,20000,194},{1,20000,200},{1,20000,206},
{1,20000,212},{1,20000,219},{1,20000,225},{1,20000,231},{1,20000,237},
{1,20000,244},{1,20000,250},{1,20000,256},{1,20000,262},{1,20000,268},
{1,20000,275},{1,20000,281},{1,20000,287},{1,20000,293},{1,20000,300},
{1,20000,306},{1,20000,312},{1,20000,318},{1,20000,325},{1,20000,331},
{1,20000,337},{1,20000,343},{1,20000,350},{1,20000,356},{1,20000,362},
{1,20000,368},{1,20000,375},{1,20000,381},{1,20000,387},{1,20000,400}
},
{ // TABLE7
{0,20000,39}, {0,20000,41}, {0,20000,42}, {0,20000,45}, {0,20000,47}, 
{0,20000,48}, {0,20000,50}, {0,20000,53}, {0,20000,56}, {0,20000,59}, 
{0,20000,62}, {0,20000,64}, {0,20000,67}, {0,20000,70}, {0,20000,73}, 
{0,20000,75}, {0,20000,81}, {0,20000,87}, {0,20000,94}, {0,20000,106}, 
{0,20000,119}, {0,20000,131}, {0,20000,144}, {0,20000,156}, {0,20000,168}, 
{0,20000,181}, {0,20000,194}, {0,20000,206}, {0,20000,219}, {0,20000,231}, 
{0,20000,244}, {0,20000,255}, {0,20000,265}, {0,20000,275}, {0,20000,285}, 
{0,20000,295}, {0,20000,305}, {0,20000,315}, {0,20000,325}, {0,20000,335}, 
{0,20000,345}, {0,20000,355}, {0,20000,365}, {0,20000,375}, {1,20000,67}, 
{1,20000,70}, {1,20000,73}, {1,20000,75}, {1,20000,81}, {1,20000,87}, 
{1,20000,94}, {1,20000,100}, {1,20000,106}, {1,20000,112}, {1,20000,119}, 
{1,20000,125}, {1,20000,131}, {1,20000,137}, {1,20000,144}, {1,20000,150}, 
{1,20000,156}, {1,20000,162}, {1,20000,169}, {1,20000,175}, {1,20000,181}, 
{1,20000,188}, {1,20000,194}, {1,20000,200}, {1,20000,206}, {1,20000,212}, 
{1,20000,219}, {1,20000,225}, {1,20000,231}, {1,20000,237}, {1,20000,244}, 
{1,20000,250}, {1,20000,256}, {1,20000,262}, {1,20000,268}, {1,20000,275}, 
{1,20000,281}, {1,20000,287}, {1,20000,293}, {1,20000,300}, {1,20000,306}, 
{1,20000,312}, {1,20000,318}, {1,20000,325}, {1,20000,331}, {1,20000,337}, 
{1,20000,343}, {1,20000,350}, {1,20000,356}, {1,20000,362}, {1,20000,368}, 
{1,20000,375}, {1,20000,381}, {1,20000,387}, {1,20000,393}, {1,20000,400} 
},
{// TABLE8
{0,20000,18},{0,20000,24},{0,20000,30},{0,20000,41},{0,20000,49},
{0,20000,62},{0,20000,71},{0,20000,82},{0,20000,93},{0,20000,110},
{0,20000,132},{0,20000,150},{0,20000,170},{0,20000,195},{0,20000,216},
{0,20000,238},{0,20000,252},{0,20000,266},{0,20000,280},{1,20000,30},
{1,20000,32},{1,20000,33},{1,20000,35},{1,20000,37},{1,20000,39},
{1,20000,41},{1,20000,43},{1,20000,45},{1,20000,47},{1,20000,49},
{1,20000,51},{1,20000,53},{1,20000,55},{1,20000,57},{1,20000,59},
{1,20000,61},{1,20000,63},{1,20000,65},{1,20000,67},{1,20000,69},
{1,20000,71},{1,20000,73},{1,20000,75},{1,20000,77},{1,20000,80},
{1,20000,84},{1,20000,87},{1,20000,89},{1,20000,91},{1,20000,93},
{1,20000,96},{1,20000,99},{1,20000,102},{1,20000,104},{1,20000,106},
{1,20000,109},{1,20000,112},{1,20000,114},{1,20000,116},{1,20000,118},
{1,20000,120},{1,20000,123},{1,20000,126},{1,20000,128},{1,20000,131},
{1,20000,133},{1,20000,136},{1,20000,138},{1,20000,140},{1,20000,143},
{1,20000,146},{1,20000,149},{1,20000,152},{1,20000,155},{1,20000,158},
{1,20000,160},{1,20000,162},{1,20000,165},{1,20000,167},{1,20000,172},
{1,20000,175},{1,20000,178},{1,20000,185},{1,20000,192},{1,20000,196},
{1,20000,200},{1,20000,204},{1,20000,208},{1,20000,216},{1,20000,223},
{1,20000,230},{1,20000,237},{1,20000,245},{1,20000,248},{1,20000,252},
{1,20000,257},{1,20000,262},{1,20000,268},{1,20000,274},{1,20000,280},
},
{//TABLE9
{0,20000,18}, {0,20000,21}, {0,20000,26}, {0,20000,38}, {0,20000,46},
{0,20000,58}, {0,20000,67}, {0,20000,77}, {0,20000,90}, {0,20000,101},
{0,20000,123}, {0,20000,140}, {0,20000,160}, {0,20000,190}, {0,20000,210},
{0,20000,230}, {0,20000,245}, {0,20000,258}, {0,20000,270},{1,20000,24},
{1,20000,26}, {1,20000,28}, {1,20000,30}, {1,20000,32}, {1,20000,34},
{1,20000,36}, {1,20000,38}, {1,20000,40}, {1,20000,42}, {1,20000,44},
{1,20000,46}, {1,20000,48}, {1,20000,50}, {1,20000,52}, {1,20000,54},
{1,20000,56}, {1,20000,58}, {1,20000,60}, {1,20000,62}, {1,20000,64},
{1,20000,66}, {1,20000,68}, {1,20000,70}, {1,20000,72}, {1,20000,74},
{1,20000,76}, {1,20000,78}, {1,20000,80}, {1,20000,82}, {1,20000,84},
{1,20000,86}, {1,20000,88}, {1,20000,90}, {1,20000,92}, {1,20000,94},
{1,20000,96}, {1,20000,98}, {1,20000,100}, {1,20000,102}, {1,20000,104},
{1,20000,106}, {1,20000,108}, {1,20000,110}, {1,20000,112}, {1,20000,114},
{1,20000,116}, {1,20000,118}, {1,20000,120}, {1,20000,122}, {1,20000,124},
{1,20000,126}, {1,20000,128}, {1,20000,130}, {1,20000,132}, {1,20000,134},
{1,20000,136}, {1,20000,138}, {1,20000,142}, {1,20000,144}, {1,20000,148},
{1,20000,152}, {1,20000,158}, {1,20000,161}, {1,20000,163}, {1,20000,166},
{1,20000,169}, {1,20000,175}, {1,20000,180}, {1,20000,184}, {1,20000,187},
{1,20000,192}, {1,20000,196}, {1,20000,199}, {1,20000,203}, {1,20000,207},
{1,20000,211}, {1,20000,215}, {1,20000,219}, {1,20000,226}, {1,20000,230}
},
{//TABLE10
{0,20000,17}, {0,20000,19}, {0,20000,22}, {0,20000,24}, {0,20000,26},
{0,20000,28}, {0,20000,31}, {0,20000,33}, {0,20000,36}, {0,20000,41},
{0,20000,44}, {0,20000,47}, {0,20000,50}, {0,20000,53}, {0,20000,56},
{0,20000,59}, {0,20000,62}, {0,20000,65}, {0,20000,68}, {0,20000,71},
{0,20000,80}, {0,20000,88}, {0,20000,98}, {0,20000,113}, {0,20000,128},
{0,20000,144}, {0,20000,160}, {0,20000,178}, {0,20000,197}, {0,20000,214},
{0,20000,228}, {0,20000,248}, {0,20000,263}, {0,20000,283},{1,20000,22},
{1,20000,25}, {1,20000,28}, {1,20000,31}, {1,20000,33}, {1,20000,36},
{1,20000,39}, {1,20000,41}, {1,20000,43}, {1,20000,44}, {1,20000,46},
{1,20000,47}, {1,20000,48}, {1,20000,49}, {1,20000,50}, {1,20000,51},
{1,20000,52}, {1,20000,53}, {1,20000,55}, {1,20000,58}, {1,20000,60},
{1,20000,62}, {1,20000,64}, {1,20000,66}, {1,20000,67}, {1,20000,69},
{1,20000,72}, {1,20000,75}, {1,20000,78}, {1,20000,80}, {1,20000,83},
{1,20000,86}, {1,20000,89}, {1,20000,92}, {1,20000,95}, {1,20000,98},
{1,20000,101}, {1,20000,104}, {1,20000,108}, {1,20000,112}, {1,20000,115},
{1,20000,118}, {1,20000,121}, {1,20000,123}, {1,20000,126}, {1,20000,129},
{1,20000,132}, {1,20000,136}, {1,20000,140}, {1,20000,143}, {1,20000,147},
{1,20000,151}, {1,20000,154}, {1,20000,158}, {1,20000,162}, {1,20000,165},
{1,20000,169}, {1,20000,172}, {1,20000,176}, {1,20000,179}, {1,20000,183},
{1,20000,187}, {1,20000,192}, {1,20000,197}, {1,20000,202}, {1,20000,208}
},
{//TABLE11
{0,20000,89}, {0,20000,94}, {0,20000,99}, {0,20000,104}, {0,20000,109},
{0,20000,114}, {0,20000,119}, {0,20000,128}, {0,20000,138}, {0,20000,145},
{0,20000,155}, {0,20000,163}, {0,20000,176}, {0,20000,185}, {0,20000,197},
{0,20000,206}, {0,20000,216}, {0,20000,227}, {0,20000,238}, {0,20000,250},
{0,20000,266}, {0,20000,284}, {0,20000,301}, {0,20000,319}, {0,20000,336}, 
{0,20000,353}, {1,20000,24}, {1,20000,26}, {1,20000,27}, {1,20000,28},
{1,20000,29}, {1,20000,31}, {1,20000,32}, {1,20000,33}, {1,20000,34},
{1,20000,35}, {1,20000,37}, {1,20000,38}, {1,20000,39}, {1,20000,40},
{1,20000,42}, {1,20000,43}, {1,20000,46}, {1,20000,48}, {1,20000,51},
{1,20000,53}, {1,20000,55}, {1,20000,58}, {1,20000,61}, {1,20000,64},
{1,20000,68}, {1,20000,72}, {1,20000,76}, {1,20000,81}, {1,20000,85},
{1,20000,89}, {1,20000,93}, {1,20000,98}, {1,20000,102}, {1,20000,106},
{1,20000,110}, {1,20000,114}, {1,20000,118}, {1,20000,122}, {1,20000,126},
{1,20000,130}, {1,20000,135}, {1,20000,139}, {1,20000,143}, {1,20000,147},
{1,20000,151}, {1,20000,155}, {1,20000,160}, {1,20000,164}, {1,20000,169},
{1,20000,174}, {1,20000,178}, {1,20000,182}, {1,20000,188}, {1,20000,192},
{1,20000,196}, {1,20000,200}, {1,20000,205}, {1,20000,210}, {1,20000,214},
{1,20000,219}, {1,20000,223}, {1,20000,228}, {1,20000,232}, {1,20000,237},
{1,20000,241}, {1,20000,247}, {1,20000,252}, {1,20000,257}, {1,20000,262},
{1,20000,267}, {1,20000,272}, {1,20000,277}, {1,20000,283}, {1,20000,288}
},
{//TABLE12
{0,20000,16}, {0,20000,21}, {0,20000,25}, {0,20000,30}, {0,20000,34},//1~5
{0,20000,43}, {0,20000,54}, {0,20000,69}, {0,20000,85}, {0,20000,104},//6~10
{0,20000,122}, {0,20000,141}, {0,20000,159}, {0,20000,178}, {0,20000,197},//11~15
{0,20000,217}, {0,20000,236}, {0,20000,275}, {0,20000,316}, {0,20000,356},//16~20
{1,20000,33}, {1,20000,36}, {1,20000,39}, {1,20000,42}, {1,20000,45},//21~25 
{1,20000,48}, {1,20000,51}, {1,20000,54}, {1,20000,57}, {1,20000,60},//26~30
{1,20000,62}, {1,20000,64}, {1,20000,66}, {1,20000,68}, {1,20000,70},//31~35
{1,20000,72}, {1,20000,74}, {1,20000,76}, {1,20000,78}, {1,20000,80},//36~40
{1,20000,82}, {1,20000,84}, {1,20000,86}, {1,20000,88}, {1,20000,91},//41~45
{1,20000,93}, {1,20000,95}, {1,20000,97}, {1,20000,99}, {1,20000,102},//46~50
{1,20000,104}, {1,20000,106}, {1,20000,108}, {1,20000,110}, {1,20000,112},//51~55
{1,20000,114}, {1,20000,116}, {1,20000,118}, {1,20000,120}, {1,20000,122},//56~60
{1,20000,124}, {1,20000,126}, {1,20000,128}, {1,20000,130}, {1,20000,132},//61~65
{1,20000,136}, {1,20000,139}, {1,20000,142}, {1,20000,145}, {1,20000,148},//66~70
{1,20000,151}, {1,20000,154}, {1,20000,156}, {1,20000,158}, {1,20000,160},//71~75
{1,20000,162}, {1,20000,164}, {1,20000,166}, {1,20000,169}, {1,20000,172},//76~80
{1,20000,175}, {1,20000,179}, {1,20000,183}, {1,20000,188}, {1,20000,193},//81~85
{1,20000,198}, {1,20000,204}, {1,20000,208}, {1,20000,213}, {1,20000,221},//86~90
{1,20000,227}, {1,20000,235}, {1,20000,242}, {1,20000,249}, {1,20000,257},//91~95
{1,20000,268}, {1,20000,274}, {1,20000,280}, {1,20000,286}, {1,20000,292} //96~100
},
{//TABLE13
{0,20000,19},{0,20000,22},{0,20000,28},{0,20000,40},{0,20000,51},
{0,20000,65},{0,20000,76},{0,20000,88},{0,20000,105},{0,20000,121},
{0,20000,150},{0,20000,167},{0,20000,195},{0,20000,225},{0,20000,250},
{0,20000,277},{0,20000,293},{0,20000,312},{0,20000,325},{1,20000,27},
{1,20000,30},{1,20000,32},{1,20000,35},{1,20000,38},{1,20000,41},
{1,20000,44},{1,20000,46},{1,20000,48},{1,20000,51},{1,20000,54},
{1,20000,57},{1,20000,59},{1,20000,61},{1,20000,64},{1,20000,67},
{1,20000,70},{1,20000,72},{1,20000,75},{1,20000,77},{1,20000,80},
{1,20000,83},{1,20000,85},{1,20000,88},{1,20000,91},{1,20000,94},
{1,20000,97},{1,20000,99},{1,20000,102},{1,20000,105},{1,20000,107},
{1,20000,110},{1,20000,113},{1,20000,116},{1,20000,118},{1,20000,121},
{1,20000,124},{1,20000,127},{1,20000,130},{1,20000,133},{1,20000,135},
{1,20000,138},{1,20000,141},{1,20000,143},{1,20000,146},{1,20000,149},
{1,20000,152},{1,20000,155},{1,20000,158},{1,20000,161},{1,20000,164},
{1,20000,167},{1,20000,170},{1,20000,173},{1,20000,176},{1,20000,178},
{1,20000,182},{1,20000,185},{1,20000,190},{1,20000,194},{1,20000,200},
{1,20000,205},{1,20000,214},{1,20000,219},{1,20000,222},{1,20000,226},
{1,20000,231},{1,20000,239},{1,20000,244},{1,20000,253},{1,20000,257},
{1,20000,265},{1,20000,271},{1,20000,274},{1,20000,280},{1,20000,286},
{1,20000,291},{1,20000,298},{1,20000,304},{1,20000,309},{1,20000,319}
}
#if 0
// TABLE pattern
{
{0,20000,}, {0,20000,}, {0,20000,}, {0,20000,}, {0,20000,},//1~5
{0,20000,}, {0,20000,}, {0,20000,}, {0,20000,}, {0,20000,},//6~10
{0,20000,}, {0,20000,}, {0,20000,}, {0,20000,}, {0,20000,},//11~15
{0,20000,}, {0,20000,}, {0,20000,}, {0,20000,}, {0,20000,},//16~20
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//21~25 
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//26~30
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//31~35
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//36~40
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//41~45
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//46~50
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//51~55
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//56~60
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//61~65
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//66~70
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//71~75
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//76~80
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//81~85
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//86~90
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,},//91~95
{1,20000,}, {1,20000,}, {1,20000,}, {1,20000,}, {1,20000,} //96~100
}
#endif
};

struct ht68f20_FL_setting {
    unsigned short fl_r_en;
    unsigned short duty;
};

static struct ht68f20_FL_setting ht68f20_table[][100]={
{// TABLE0
{0,49}, {0,52}, {0,56}, {0,61}, {0,67}, {0,75}, {0,80}, {0,87}, {0,93}, {0,104},
{0,117}, {0,128}, {0,140}, {0,156}, {0,169}, {0,182}, {0,204}, {0,216}, {0,260}, {1,50},
{1,53}, {1,55}, {1,59}, {1,62}, {1,66}, {1,69}, {1,73}, {1,77}, {1,81},{1,85},
{1,88}, {1,92}, {1,96}, {1,99}, {1,103}, {1,107}, {1,111}, {1,116}, {1,120},{1,124},
{1,128}, {1,132}, {1,136}, {1,140}, {1,145}, {1,153}, {1,159}, {1,163}, {1,167},{1,171},
{1,176}, {1,182}, {1,188}, {1,192}, {1,196}, {1,202}, {1,207}, {1,211}, {1,215},{1,219},
{1,223}, {1,229}, {1,235}, {1,239}, {1,244}, {1,249}, {1,254}, {1,258}, {1,262},{1,268},
{1,274}, {1,280}, {1,286}, {1,292}, {1,298}, {1,302}, {1,306}, {1,312}, {1,316},{1,326},
{1,332}, {1,339}, {1,353}, {1,367}, {1,375}, {1,383}, {1,392}, {1,400}, {1,417}, {1,431},
{1,446}, {1,460}, {1,477}, {1,484}, {1,492}, {1,503}, {1,509}, {1,510}, {1,511}, {1,512}
}
};

struct delayed_work FL_W_off_work;
struct delayed_work FL_R_off_work;

static void FL_module_off(unsigned short wColorFlags);

void FL_W_off_func(struct work_struct *work);
void FL_R_off_func(struct work_struct *work);
int FL_suspend(void);

//kay for LED thread
//static unsigned char LED_conitnuous=0;
static unsigned char LED_conitnuous=1;
static int LED_Flash_Count;
static int gKeepPowerAlive;
static int gMxcPowerKeyIrqTriggered, g_power_key_pressed;
volatile int g_mxc_touch_triggered = 1;	//gallen 100420
int g_wakeup_by_alarm;
int gWifiEnabled=0;
static unsigned long g_usb_in_tick;	// Joseph 101001
static int g_ioctl_SD_status, g_ioctl_USB_status, g_ioctl_rotary_status,g_Cus_Ctrl_Led;
int g_mmc_card_detect_changed;	// Joseph 20110125
static DEFINE_SPINLOCK(led_flash_lock);
static DECLARE_WAIT_QUEUE_HEAD(LED_blink_WaitQueue);
static DECLARE_WAIT_QUEUE_HEAD(LED_freeze_WaitQueue);
static DECLARE_WAIT_QUEUE_HEAD(WheelKey_WaitQueue);
////////////////////

unsigned int  last_FL_duty = 0;
static unsigned int  last_FL_R_percent = 0;
static unsigned int  last_FL_G_percent = 0;
static unsigned int  last_FL_B_percent = 0;
static unsigned int  current_FL_freq = 0xFFFF;


static DECLARE_WAIT_QUEUE_HEAD(Reset_WaitQueue);

extern int gIsCustomerUi;

int ntx_charge_status (void);
int mxc_usb_plug_getstatus (void);

void led_green (int isOn)
{

	if(0x03!=gptHWCFG->m_val.bUIConfig) {
		// do not check charging status in MP/RD mode 
		if(9!=gptHWCFG->m_val.bCustomer) {
			if(gMX6SL_ON_LED==gMX6SL_CHG_LED&&mxc_usb_plug_getstatus()) {
				// skip control charge led while charging .
				return ;
			}
		}
	}


	if (isOn)
		gpio_direction_output (gMX6SL_ON_LED,0);
	else {
		switch(gptHWCFG->m_val.bPCB) {
		case 33: 
			// E60Q2X .
			gpio_direction_output (gMX6SL_ON_LED,1);
		default :
			gpio_direction_input (gMX6SL_ON_LED);
		}
	}
}

void led_blue (int isOn)
{

	switch(gptHWCFG->m_val.bLed) {
		case 1:
			if (isOn)
				gpio_direction_output (gMX6SL_ACT_LED,0);
			else
				gpio_direction_input (gMX6SL_ACT_LED);
			break;
	}
}

void led_red (int isOn) {
	switch(gptHWCFG->m_val.bLed) {
	//case 0:// Type1 .
	case 1:// RGB
	case 2:// RG
	case 3:// RGH
	case 4:// W
	case 5:// G
	case 7:// WH
		if (isOn) {
			gpio_direction_output (gMX6SL_CHG_LED,0);
		}
		else {
			if(0x03!=gptHWCFG->m_val.bUIConfig) {
				// do not check charging status in MP/RD mode 
				if(9!=gptHWCFG->m_val.bCustomer) {
					if(mxc_usb_plug_getstatus()) {
						// skip control charge led while charging .
						break ;
					}
				}
			}
			gpio_direction_input (gMX6SL_CHG_LED);
		}
		break;
	}
}


//kay 20090925
//check WiFi ID
static int check_hardware_wifi(void)
{
    return  WIFI_NONE;           
}

//check Bluetooth ID
static int check_hardware_bt(void)
{
    return  BLUETOOTH_NONE;           
}

static int check_hardware_cpu(void)
{
    return CPU_S3C2440;
}

//static int check_hardeare_name(void)
int check_hardware_name(void)
{
	static int pcb_id = -1;

	if (0 >= pcb_id) {
		switch(gptHWCFG->m_val.bPCB)
		{
			default:
				pcb_id = gptHWCFG->m_val.bPCB;
				break;	
		}
		printk ("[%s-%d] PCBA ID is %d\n",__func__,__LINE__,pcb_id);
	}

    return pcb_id;      
}
EXPORT_SYMBOL(check_hardware_name);

static int check_hardware_controller(void)
{
    return CONTROLLER_EPSON;
}

static void collect_hardware_info(struct ebook_device_info *info)
{
    info->cpu = check_hardware_cpu();
    info->device_name = check_hardware_name();
    info->controller = check_hardware_controller();
    info->wifi = check_hardware_wifi();
    info->bluetooth = check_hardware_bt();
}

static int openDriver(struct inode *inode,struct file *filp)
{
	if(!Driver_Count)
		Driver_Count++;
	return 0;
}
static int releaseDriver(struct inode *inode,struct file *filp)
{
	if(Driver_Count)
		Driver_Count--;
	return 0;
}
static void bluetooth_reset(int i)
{
}

static void bluetooth_pwr(int i)
{
}

extern unsigned long g_kl25_result;
extern unsigned long g_kl25_action;
extern void ntx_wifi_power_ctrl (int isWifiEnable);

extern u16 msp430_deviceid(void);
extern void msp430_auto_power(int minutes);
extern void msp430_power_off(void);
extern void msp430_pm_restart(void);
extern void msp430_powerkeep(int n);
extern int msp430_battery(void);
extern int up_cmd_lock(int iLockID);
extern int up_cmd_unlock(void);

int ricoh619_restart(void);
int ricoh619_battery_2_msp430_adc(void);
int ricoh619_charger_detect(void);
int ricoh619_dcin_status(void);

extern int ht68f20_write_reg(unsigned int reg, unsigned int value);
extern unsigned int ht68f20_read_reg(unsigned int reg);
extern void ht68f20_enable(int isEnable);

extern int up_write_reg(unsigned int reg, unsigned int value);
extern unsigned int up_safe_read_reg(unsigned int reg);
extern int msp430_fl_endtime(unsigned short dwFL_EndtimeScale);

extern void fl_regulator_enable(int isEnable);
int g_power_key_debounce;		// Joseph 20100921 for ESD

volatile static unsigned short gwFL_ColorEnableState=0;
static DEFINE_MUTEX(fl_pwr_mutex);

static unsigned short _fl_set_ColorFlags(unsigned short wColorFlags,int isEnable)
{
	unsigned short wColorFlagsOld;

	if(wColorFlags&FL_COLOR_FLAGS_W) {
		if(isEnable) 
			gwFL_ColorEnableState |= FL_COLOR_FLAGS_W;
		else 
			gwFL_ColorEnableState &= ~FL_COLOR_FLAGS_W;
	}
	
	if(wColorFlags&FL_COLOR_FLAGS_R) {
		if(isEnable) 
			gwFL_ColorEnableState |= FL_COLOR_FLAGS_R;
		else 
			gwFL_ColorEnableState &= ~FL_COLOR_FLAGS_R;
	}
	
	if(wColorFlags&FL_COLOR_FLAGS_G) {
		if(isEnable) 
			gwFL_ColorEnableState |= FL_COLOR_FLAGS_G;
		else 
			gwFL_ColorEnableState &= ~FL_COLOR_FLAGS_G;
	}

	if(wColorFlags&FL_COLOR_FLAGS_B) {
		if(isEnable) 
			gwFL_ColorEnableState |= FL_COLOR_FLAGS_B;
		else 
			gwFL_ColorEnableState &= ~FL_COLOR_FLAGS_B;
	}

	return wColorFlagsOld;
}
static unsigned short _fl_get_ColorFlags(void)
{
	return gwFL_ColorEnableState;
}

void fl_pwr_force_enable (int isEnable)
{

	printk("%s(%d)\n",__FUNCTION__,isEnable);
	if(1==isEnable||2==isEnable) {
		gpio_direction_output(gMX6SL_FL_PWR_EN,giFL_ON);
		if(1==isEnable && !in_interrupt()) {
			// critical section .
			fl_regulator_enable(2);
		}
	}
	else if(0==isEnable||-1==isEnable){
		if(0==isEnable && !in_interrupt()) {
			// critical section .
			fl_regulator_enable(0);
		}
		gpio_direction_input(gMX6SL_FL_PWR_EN);
	}
}

int fl_pwr_enable (unsigned short wColorFlags,int isEnable,int iDelayMS)
{

	unsigned short wFLOldStatus = _fl_get_ColorFlags();
	unsigned short wFLNewStatus ;
	int iRet = -1;
	int iIsInInterrupt=in_interrupt();

	if(!iIsInInterrupt) {
		mutex_lock(&fl_pwr_mutex);
	}

	_fl_set_ColorFlags(wColorFlags,isEnable);

	wFLNewStatus = _fl_get_ColorFlags();

	printk("%s(0x%x,new=0x%x,old=0x%x)\n",__FUNCTION__,wColorFlags,wFLNewStatus,wFLOldStatus);
	if(isEnable) {
		if(0==wFLOldStatus) {
			fl_pwr_force_enable(1);
			iRet = 1;
			if(iDelayMS>0) {
				if(iIsInInterrupt) {
					mdelay(iDelayMS);
				}
				else {
					msleep(iDelayMS);
				}
			}
		}
	}
	else {
		if(4==gptHWCFG->m_val.bFL_PWM||2==gptHWCFG->m_val.bFL_PWM||
			 6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) 
		{
			// single lm3630a .
		}
		else if (wFLOldStatus && 0==wFLNewStatus){
			if(iDelayMS>0) {
				if(iIsInInterrupt) {
					mdelay(iDelayMS);
				}
				else {
					msleep(iDelayMS);
				}
			}
			fl_pwr_force_enable(0);
			iRet = 0;
		}
	}

	if(!iIsInInterrupt) {
		mutex_unlock(&fl_pwr_mutex);
	}

	return iRet;
}



static void fl_pwm_enable (unsigned short wColorFlags,int isEnable) 
{

	if(1==gptHWCFG->m_val.bFL_PWM)
	{
		ht68f20_enable (isEnable);
		if (isEnable) {
			mxc_iomux_v3_setup_pad (MX6SL_PAD_I2C1_SCL__I2C1_SCL);
			mxc_iomux_v3_setup_pad (MX6SL_PAD_I2C1_SDA__I2C1_SDA);
		}
		else {
			mxc_iomux_v3_setup_pad (MX6SL_PAD_I2C1_SCL__GPIO_3_12);
			mxc_iomux_v3_setup_pad (MX6SL_PAD_I2C1_SDA__GPIO_3_13);
		}
	}
	else {
		if(wColorFlags & FL_COLOR_FLAGS_W) {
			if(0==gptHWCFG->m_val.bFL_PWM || 4==gptHWCFG->m_val.bFL_PWM) {
				msp430_fl_enable(MSP430_FL_IDX_W,isEnable);
			}
		}

		if(wColorFlags & FL_COLOR_FLAGS_R) {
			if(4==gptHWCFG->m_val.bFL_PWM) {	
				msp430_fl_enable(MSP430_FL_IDX_R,isEnable);
			}
		}
	}
}


unsigned long long hwconfig = 0x0000000011000001LL;
EXPORT_SYMBOL(hwconfig);
unsigned char platform_type[32];
EXPORT_SYMBOL(platform_type);

static int __init early_hw(char *p)
{
	hwconfig = simple_strtoull(p, NULL, 16);
	printk("hwconfig: %16llX\n", hwconfig);
	return 0;
}
early_param("hwconfig", early_hw);

//to parse hardware configuration bits
static int __init early_board(char *p)
{
	strncpy(platform_type, p, sizeof(platform_type));
	printk("board: %s\n", platform_type);
	return 0;
}
early_param("board", early_board);

int power_key_status (void)
{
	return gpio_get_value (gMX6SL_PWR_SW)?0:1;
}

int kl25_int_status (void)
{
	return gpio_get_value (MX6SL_KL25_INT2)?1:0;
}

#define LM3630_BRIGTNESS_MIN	0x04
//static unsigned char gbGCur=0,gbGBrig=LM3630_BRIGTNESS_MIN;
//static unsigned char gbBCur=0,gbBBrig=LM3630_BRIGTNESS_MIN;
extern int fl_lm3630a_percentageEx (int iChipIdx,int iFL_Percentage);
extern int fl_lm3630a_set_color (int iFL_color);
extern int fl_lm3630a_set_colorEx (int iChipIdx,int iFL_color);
extern void lm3630a_set_FL (unsigned char led_A_current, unsigned char led_A_brightness,
		unsigned char led_B_current, unsigned char led_B_brightness);
void lm3630a_set_FL_EX (int iChipIdx,
		unsigned char led_A_current, unsigned char led_A_brightness,
		unsigned char led_B_current, unsigned char led_B_brightness);

extern int lm3630a_get_FL_EX (int iChipIdx,
		unsigned char *O_pbLed_A_current, unsigned char *O_pbLed_A_brightness,
		unsigned char *O_pbLed_B_current, unsigned char *O_pbLed_B_brightness);
int lm3630a_get_default_power_by_table(int iFL_table_idx,unsigned char *O_pbPower);

int fl_get_color_percentage(unsigned short wColorFlags)
{
	int iRet=-1;
	if(FL_COLOR_FLAGS_W==wColorFlags) {
		iRet = last_FL_duty;
	}
	else if(FL_COLOR_FLAGS_R==wColorFlags) {
		iRet = last_FL_R_percent;
	}
	else if(FL_COLOR_FLAGS_G==wColorFlags) {
		iRet = last_FL_G_percent;
	}
	else if(FL_COLOR_FLAGS_B==wColorFlags) {
		iRet = last_FL_B_percent;
	}
	return iRet;
}
int fl_set_color_percentage(unsigned short wColorFlags,int iFL_Percentage)
{
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE //[
	int iRet = 0;
	int p=iFL_Percentage;

	unsigned short wFLOldStatus = _fl_get_ColorFlags();

	printk("%s(0x%x,%d),FL old status=0x%x\n",__FUNCTION__,
			wColorFlags,iFL_Percentage,wFLOldStatus);
	
	if(0==gptHWCFG->m_val.bFrontLight)
		return -1;

	if(1==gptHWCFG->m_val.bFL_PWM)
	{
		 // HT68F20

		if (p) {
			fl_pwm_enable (FL_COLOR_FLAGS_W,1);
			if(delayed_work_pending(&FL_W_off_work)){
				cancel_delayed_work_sync(&FL_W_off_work);
				printk("FL_W_off delayed work canceled");
			}
			printk ("\nset front light level : %d\n",p);
			if(p<=100)
			{
				int fl_r_en;
				int duty;
				fl_r_en = ht68f20_table[0][p-1].fl_r_en;
				duty = ht68f20_table[0][p-1].duty;
				if (last_FL_duty >= p)
					gpio_direction_output (gMX6SL_FL_W_H_EN, fl_r_en);
				
				ht68f20_write_reg (0xA6, duty&0xFF);	// Set PWM duty
				if ((duty>>8) == (ht68f20_table[0][last_FL_duty-1].duty)>>8)
					msleep (20);
				ht68f20_write_reg (0xA7, duty>>8);
				printk ("Set front light duty : %d\n",duty);

				if (last_FL_duty < p)
					gpio_direction_output (gMX6SL_FL_W_H_EN, fl_r_en);
			}
			else{
				printk("Wrong number! level range from 0 to 100\n");
			}
			if (0 == last_FL_duty){
				ht68f20_write_reg (0xA3, 0x01);	// enable front light pwm

				fl_pwr_enable(FL_COLOR_FLAGS_W,1,10);
			}
		}
		else if(last_FL_duty != 0){
			printk ("FL PWM off command\n");
			ht68f20_write_reg(0xA3, 0); 
			schedule_delayed_work(&FL_W_off_work, 120);
		}
		last_FL_duty = p;
		
	}
	else if(0==gptHWCFG->m_val.bFL_PWM||
			4==gptHWCFG->m_val.bFL_PWM||
			5==gptHWCFG->m_val.bFL_PWM||
			6==gptHWCFG->m_val.bFL_PWM||
			7==gptHWCFG->m_val.bFL_PWM)
	{
		int iMSP430_color_idx;




		if(4==gptHWCFG->m_val.bFL_PWM || 5==gptHWCFG->m_val.bFL_PWM||
			6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) 
		{
			unsigned char bBCurr,bBBrig;
			unsigned char bGCurr,bGBrig;
			unsigned char bWCurr,bWBrig;
			unsigned char bRCurr,bRBrig;

			// msp430+lm3630 | lm3636 x 2 .
			lm3630a_get_FL_EX(0,&bBCurr,&bBBrig,&bGCurr,&bGBrig);
			if(FL_COLOR_FLAGS_G==wColorFlags) {
				if(0==p) {
					bGCurr=0;
					bGBrig=LM3630_BRIGTNESS_MIN;
				}
				else if(p<=100){
					if(0==last_FL_G_percent) {
						fl_pwr_enable(wColorFlags,1,10);
					}
					else {
						fl_pwr_enable(wColorFlags,1,0);
					}
					lm3630a_get_default_power_by_table(gptHWCFG->m_val.bFrontLight,&bGCurr);
					//gbGBrig=(p*255)/100;
					bGBrig=p+151;
				}

				lm3630a_set_FL(bBCurr,bBBrig,bGCurr,bGBrig);

				if(0==p) {
					if(0==last_FL_G_percent) {
						fl_pwr_enable(wColorFlags,0,0);
					}
					else {
						fl_pwr_enable(wColorFlags,0,10);
					}
				}
				last_FL_G_percent=p;
				return 0;
			}
			else if(FL_COLOR_FLAGS_B==wColorFlags) {
				if(0==p) {
					bBCurr=0;
					bBBrig=LM3630_BRIGTNESS_MIN;
				}
				else if(p<=100){
					if(0==last_FL_B_percent) {
						fl_pwr_enable(wColorFlags,1,10);
					}
					else {
						fl_pwr_enable(wColorFlags,1,0);
					}
					lm3630a_get_default_power_by_table(gptHWCFG->m_val.bFrontLight,&bBCurr);
					//gbBBrig=(p*255)/100;
					bBBrig=p+151;
				}
				lm3630a_set_FL(bBCurr,bBBrig,bGCurr,bGBrig);

				if(0==p) {
					if(0==last_FL_B_percent) {
						fl_pwr_enable(wColorFlags,0,0);
					}
					else {
						fl_pwr_enable(wColorFlags,0,10);
					}
				}
				last_FL_B_percent=p;
				return 0;
			}

			if(5==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) 
			{
				lm3630a_get_FL_EX(1,&bWCurr,&bWBrig,&bRCurr,&bRBrig);

				if(FL_COLOR_FLAGS_W==wColorFlags) {
					if(0==p) {
						bWCurr=0;
						bWBrig=LM3630_BRIGTNESS_MIN;
					}
					else if(p<=100){
						if(0==last_FL_duty) {
							fl_pwr_enable(wColorFlags,1,10);
						}
						else {
							fl_pwr_enable(wColorFlags,1,0);
						}
						lm3630a_get_default_power_by_table(gptHWCFG->m_val.bFrontLight,&bWCurr);
						//gbBBrig=(p*255)/100;
						bWBrig=p+151;
					}

					if(6==gptHWCFG->m_val.bFL_PWM) {
						lm3630a_set_FL_EX(0,bWCurr,bWBrig,0,0);
					}
					else if(7==gptHWCFG->m_val.bFL_PWM) {
						lm3630a_set_FL_EX(0,0,0,bWCurr,bWBrig);
					}
					else {
						lm3630a_set_FL_EX(1,bWCurr,bWBrig,bRCurr,bRBrig);
					}

					if(0==p) {
						if(0==last_FL_duty) {
							fl_pwr_enable(wColorFlags,0,0);
						}
						else {
							fl_pwr_enable(wColorFlags,0,10);
						}
					}
					last_FL_duty=p;
					return 0;
				}
			}

			if(5==gptHWCFG->m_val.bFL_PWM) {
				if(FL_COLOR_FLAGS_R==wColorFlags) {
					if(0==p) {
						bRCurr=0;
						bRBrig=LM3630_BRIGTNESS_MIN;
					}
					else if(p<=100){
						if(0==last_FL_R_percent) {
							fl_pwr_enable(wColorFlags,1,10);
						}
						else {
							fl_pwr_enable(wColorFlags,1,0);
						}
						lm3630a_get_default_power_by_table(gptHWCFG->m_val.bFrontLight,&bRCurr);
						//gbBBrig=(p*255)/100;
						bRBrig=p+151;
					}
					lm3630a_set_FL_EX(1,bWCurr,bWBrig,bRCurr,bRBrig);

					if(0==p) {
						if(0==last_FL_R_percent) {
							fl_pwr_enable(wColorFlags,0,0);
						}
						else {
							fl_pwr_enable(wColorFlags,0,10);
						}
					}
					last_FL_R_percent=p;
					return 0;
				}
			}
		}
		else {
			//return -1;
		}
		if(FL_COLOR_FLAGS_W==wColorFlags) {
			iMSP430_color_idx = MSP430_FL_IDX_W;
		}
		else if(FL_COLOR_FLAGS_R==wColorFlags) {
			iMSP430_color_idx = MSP430_FL_IDX_R;
		}
		
		// FL PWM source from MSP430 .
		if (p) {

			if(FL_COLOR_FLAGS_W==wColorFlags) {
				if(0==last_FL_duty) {
					fl_pwr_enable (wColorFlags,1,100);
				}
				else {
					fl_pwr_enable (wColorFlags,1,0);
				}
				if(delayed_work_pending(&FL_W_off_work)){
					cancel_delayed_work_sync(&FL_W_off_work);
					printk("FL_W_off_work delayed work canceled");
				}
			}
			else if(FL_COLOR_FLAGS_R==wColorFlags) {
				if(0==last_FL_R_percent) {
					fl_pwr_enable (wColorFlags,1,100);
				}
				else {
					fl_pwr_enable (wColorFlags,1,0);
				}
				if(delayed_work_pending(&FL_R_off_work)){
					cancel_delayed_work_sync(&FL_R_off_work);
					printk("FL_R_off_work delayed work canceled");
				}
			}
			else {
				// wrong color .
				printk(KERN_ERR "%s():Wrong color flag(%x)!!!\n",__FUNCTION__,wColorFlags);
			}

			printk ("\nset front light level : %d\n",p);
			if(p<=100)
			{
				
				if( gptHWCFG->m_val.bFrontLight == 3){  //TABLE0a
					msp430_fl_set_freq(iMSP430_color_idx,0x0190);
					msp430_fl_set_duty(iMSP430_color_idx,FL_table0[p-1]);
					printk("PWMCNT : 0x%04x\n", FL_table0[p-1]);
				}
				else if( gptHWCFG->m_val.bFrontLight == 1 || gptHWCFG->m_val.bFrontLight == 2 ){  //TABLE0, TABLE0+
					if (!wFLOldStatus){
						msp430_fl_set_freq(iMSP430_color_idx,0x0190);
					}
					if(p<=50){
						if(FL_COLOR_FLAGS_W==wColorFlags) {
							gpio_direction_output(gMX6SL_FL_W_H_EN,0);
						}
						msp430_fl_set_duty(iMSP430_color_idx,FL_table0[2*(p-1)]);
						printk("PWMCNT : 0x%04x\n", FL_table0[2*(p-1)]);
					}else{
						if(FL_COLOR_FLAGS_W==wColorFlags) {
							gpio_direction_output(gMX6SL_FL_W_H_EN,1);
						}
						msp430_fl_set_duty(iMSP430_color_idx,FL_table0[p-1]);
						printk("PWMCNT : 0x%04x\n", FL_table0[p-1]);
					}
				}
				else {  
					int t_no = gptHWCFG->m_val.bFrontLight-4; // mapping hwconfig to FL_table
					int freq ;
					int iDuty ,iFreq;

#if 0
					if (30 == gptHWCFG->m_val.bPCB && p==5) { //E606E2
						p=1;
					}
#endif 

					if(FL_COLOR_FLAGS_W==wColorFlags) {
						if (last_FL_duty >= p)
							gpio_direction_output (gMX6SL_FL_W_H_EN, FL_table[t_no][p-1].fl_r_en);
						iFreq = FL_table[t_no][p-1].freq;
						iDuty = FL_table[t_no][p-1].duty;
					}
					else {
						iFreq = 20000; // fix at 20KHZ .
						iDuty = p<<2;
					}

					if( freq != current_FL_freq){
						printk ("Set front light %d Frequency : %d\n",
								iMSP430_color_idx,iFreq);
						freq = 8000000/iFreq;
						msp430_fl_set_freq(iMSP430_color_idx,freq);
						current_FL_freq = freq;
					}

					if(FL_COLOR_FLAGS_W==wColorFlags) {
						msp430_fl_set_duty(iMSP430_color_idx,iDuty);
						if (last_FL_duty < p)
							gpio_direction_output (gMX6SL_FL_W_H_EN, FL_table[t_no][p-1].fl_r_en);
					}
					else {
						msp430_fl_set_duty(iMSP430_color_idx,iDuty);
					}

					printk ("Set front light %d duty : %d\n",
						iMSP430_color_idx,iDuty);
				}
			}
			else{
				printk("Wrong number! level range from 0 to 100\n");
			}
			msp430_fl_endtime(0xffff); // Disable front light auto off timer .
			fl_pwm_enable(wColorFlags,1);
			//fl_pwr_enable (wColorFlags,1,0);
		}
		else if(p==0) {
			if (wFLOldStatus&wColorFlags){
				printk ("FL 0x%x tunning off \n",wColorFlags);
				fl_pwm_enable(wColorFlags,0);
				if(FL_COLOR_FLAGS_W==wColorFlags) {
					schedule_delayed_work(&FL_W_off_work, 120);
				}
				else if(FL_COLOR_FLAGS_R==wColorFlags) {
					schedule_delayed_work(&FL_R_off_work, 120);
				}
			}
		}
		if(FL_COLOR_FLAGS_W==wColorFlags) {
			last_FL_duty = p;
		}
		else if(FL_COLOR_FLAGS_R==wColorFlags) {
			last_FL_R_percent = p;
		}

	}

	return iRet;
#else //][CONFIG_BACKLIGHT_CLASS_DEVICE
	return 0;
#endif //]CONFIG_BACKLIGHT_CLASS_DEVICE
}

int fl_set_percentage(int iFL_Percentage)
{
	return fl_set_color_percentage(FL_COLOR_FLAGS_W,iFL_Percentage);
}



static void ntx_system_reset(const char *pszDomain)
{
	if(pszDomain) {
		printk("%s() ---%s reset ---\n",__FUNCTION__,pszDomain);
	}

	if(0!=gptHWCFG->m_val.bFrontLight){
		fl_pwm_enable(FL_COLOR_FLAGS_ALL,0);
		msleep (1200);

		FL_module_off(FL_COLOR_FLAGS_ALL);
	}

	while (1) {
		gKeepPowerAlive = 0;
		if (1==gptHWCFG->m_val.bPMIC) {
			// RC5T19 .
			printk("%s() --- RC5T19 restarting system ---\n",__FUNCTION__);
			ricoh619_restart ();
		}
		else {
			printk("%s() --- MSP430 restarting system ---\n",__FUNCTION__);
			msp430_pm_restart();
		}
		sleep_on_timeout(&Reset_WaitQueue, 14*HZ/10);
	}
}

static void ntx_system_poweroff(const char *pszDomain)
{
	if(pszDomain) {
		printk("%s() ---%s poweroff ---\n",__FUNCTION__,pszDomain);
	}
	if (!gKeepPowerAlive) {

		if(0!=gptHWCFG->m_val.bFrontLight){
			
			if(0==gptHWCFG->m_val.bFL_PWM || 4==gptHWCFG->m_val.bFL_PWM) {
				// FL is controlled by MSP430 .
				fl_pwm_enable(FL_COLOR_FLAGS_ALL,0);
			}

			msleep (1200);

			FL_module_off(FL_COLOR_FLAGS_ALL);
		}

		LED_conitnuous = 0;
		led_green(0);
		while (1) {
			printk("Kernel---Power Down ---\n");
			if(1==gptHWCFG->m_val.bPMIC)
				ricoh619_power_off();
			else
				msp430_power_off();
   		sleep_on_timeout(&Reset_WaitQueue, 14*HZ/10);
		}
	}
	else {
		printk("Kernel---in keep alive mode ---\n");
	}
}

static int  ioctlDriver(struct file *filp, unsigned int command, unsigned long arg)
{
	unsigned long i = 0, temp;
	unsigned int p = arg;//*(unsigned int *)arg;
  struct ebook_device_info info; 
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
	extern int mxc_epdc_fb_shutdown(struct platform_device *pdev);
#endif //]CONFIG_FB_MXC_EINK_PANEL
  	
	if(!Driver_Count){
		printk("pvi_io : do not open\n");
		return -1;	
	}

	switch(command)
	{
		case POWER_OFF_COMMAND:
			{
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
				mxc_epdc_fb_shutdown(0);
#endif //]CONFIG_FB_MXC_EINK_PANEL
				ntx_system_poweroff("POWER_OFF_COMMAND");
			}
			break;

		case SYS_RESET_COMMAND:
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
			mxc_epdc_fb_shutdown(0);
#endif //]CONFIG_FB_MXC_EINK_PANEL
			ntx_system_reset("SYS_RESET_COMMAND");
			break;
			
		case SYS_AUTO_POWER_ON:
			msp430_auto_power(p);
			ntx_system_poweroff("SYS_AUTO_POWER_ON");
			break;
			
		case POWER_KEEP_COMMAND:
			printk("Kernel---System Keep Alive --- %d\n",p);
			gKeepPowerAlive=p;
			if(1==gptHWCFG->m_val.bPMIC)
				break;
			if (gKeepPowerAlive) {
				msp430_powerkeep(1);
		   		wake_up_interruptible(&LED_freeze_WaitQueue);
			}
			else
				msp430_powerkeep(0);
			break;
			
		case CM_GET_BATTERY_STATUS:
			if(1==gptHWCFG->m_val.bPMIC) {
				i = ricoh619_battery_2_msp430_adc();
			}
			else {
				i = msp430_battery ();
			}
			if (0 == i)
				i = 0x8000;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));

			break;
			
		case AC_IN:
			if(1==gptHWCFG->m_val.bPMIC) {
				i = ricoh619_charger_detect()?1:0;
			}
			else 
				i = gpio_get_value (gMX6SL_NTX_ACIN_PG)?0:1;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;	

		case CM_SD_IN:
			g_ioctl_SD_status = gpio_get_value (MX6SL_EXT_SD_CD);
			i = (g_ioctl_SD_status)?0:1;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_USB_Plug_IN:
			if(1==gptHWCFG->m_val.bPMIC) {
				g_ioctl_USB_status = ricoh619_charger_detect()?0:1;
			}
			else {
				if (!g_ioctl_USB_status && gpio_get_value (gMX6SL_NTX_ACIN_PG)) {
					msleep(200);	// sleep 200ms to avoid system halt when USB plug out.
				}
				g_ioctl_USB_status = gpio_get_value (gMX6SL_NTX_ACIN_PG);
			}
			i = (g_ioctl_USB_status)?0:1;
			if (!g_Cus_Ctrl_Led) {
				led_red(g_ioctl_USB_status?0:1);
			}
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case GET_LnBATT_CPU:
			break;
		case GET_VBATT_TH:
			break;
		case CM_AC_CK:
			break;
		case CM_CHARGE_STATUS:
			i = ntx_charge_status();
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
		case CM_PWR_ON2:
			break;
		case CM_AUDIO_PWR:
			break;
		case CM_nLED:
			//printk("CM_nLED %d\n",p);
			led_green(p?1:0);
			break;			
			
		case CM_nLED_CPU:
			break;			
			
		case CM_SD_PROTECT:
			i = 0;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_CONTROLLER:
            i = 2;	// 2: Epson controller. for micro window
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_USB_AC_STATUS:
			i = 0;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_RTC_WAKEUP_FLAG:
			if (!g_wakeup_by_alarm) {
				if(1!=gptHWCFG->m_val.bPMIC) {
					int tmp = up_safe_read_reg (0x60);
					if (0x8000 & tmp) {
						printk ("[%s-%d] =================> Micro P MSP430 alarm triggered <===================\n", __func__, __LINE__);
						g_wakeup_by_alarm = 1;
					}
				}
            }
	        i = g_wakeup_by_alarm;		// Joseph 091221 for slide show test.
	        g_wakeup_by_alarm = 0;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_SYSTEM_RESET:
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
			mxc_epdc_fb_shutdown(0);
#endif //]CONFIG_FB_MXC_EINK_PANEL
			ntx_system_reset("CM_SYSTEM_RESET");
			break;
			
		case CM_USB_HOST_PWR:
			break;
			
		case CM_BLUETOOTH_PWR:
			ntx_wifi_power_ctrl (p);
			break;
			
        case CM_TELLPID:
			if(p!=0){
			    printk("PID %d\n",p);
			    __EBRMAIN_PID__= p;
			}
			break;
			
		case CM_LED_BLINK:
		    if (2==p) {
				spin_lock(&led_flash_lock);
		    	LED_Flash_Count++;
				spin_unlock(&led_flash_lock);
		   	}
		   	if (!LED_conitnuous)
		   		wake_up_interruptible(&LED_freeze_WaitQueue);
			LED_conitnuous = p;         
	      break;
	      
		case CM_TOUCH_LOCK:
			if(p==0)
			{
		         __TOUCH_LOCK__ = 0;         
		      }else{
		         __TOUCH_LOCK__ = 1;        
		      }
	      break;  
      
		case CM_DEVICE_MODULE:
      		i = check_hardware_name();
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
              	          	
		case CM_BLUETOOTH_RESET:
			break;
			
		case CM_DEVICE_INFO:		  
		  	collect_hardware_info(&info);
		  	copy_to_user((void __user *)arg, &info, sizeof(info));
      		break;	
      		
		case CM_ROTARY_STATUS:	
      		break;	
      		
		case CM_ROTARY_ENABLE:	
	      	break;

		case CM_USB_ID_STATUS:
			if (49==gptHWCFG->m_val.bPCB)
				i = gpio_get_value (MX6SL_USB_ID)?1:0;
			else
				i = 1;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
	      	break;

		case CM_GET_KEYS:
			i = 0;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
		case CM_POWER_BTN:
		case CM_GET_KEY_STATUS:	
			if (g_power_key_pressed) {
				g_power_key_pressed = 0;
				i = 1;
			}
			else {
				i = power_key_status ();
				
				if (i) {
					if (2 >= g_power_key_debounce) { 	// Joseph 20100921 for ESD
						printk ("[%s-%d] power key bounce detected %d\n",__func__,__LINE__, g_power_key_debounce);
						i=0;
					}
					else {
						gMxcPowerKeyIrqTriggered = 0;
					}
				}
				else if (gMxcPowerKeyIrqTriggered) {	// POWER key interrupt triggered.
					if (2 < g_power_key_debounce) {
						i = 1;
					}
					else
						printk ("[%s-%d] power key bounce detected %d\n",__func__,__LINE__,g_power_key_debounce);
					gMxcPowerKeyIrqTriggered = 0;
				}
			}
			
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			g_mxc_touch_triggered = 0;
      		break;	
      		
		case CM_GET_WHEEL_KEY_STATUS:
			i=0;
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
      		break;	
      		
		case CM_GET_KL25_STATUS:
			i=g_kl25_result;	
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			g_kl25_result &= 0xFF;
			break;
	
		case CM_GET_KL25_ACTION:
			i=g_kl25_action ;	
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			g_kl25_action = 0;
			break;

		case CM_3G_POWER:
			break;	
					
		case CM_3G_RF_OFF:
			break;	
					
		case CM_3G_RESET:
			break;	
					
		case CM_WIFI_CTRL:		
			ntx_wifi_power_ctrl (p);
			break;	
					
		case CM_3G_GET_WAKE_STATUS:	
			i = 0;	
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
	    	break;	
	    	
		case CM_SET_ALARM_WAKEUP:
	    	break;	
	    	
		case CM_GET_UP_VERSION:
			if(1==gptHWCFG->m_val.bMicroP) {
				i = 0;
			}
			else
				i = msp430_deviceid();
			copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
	    	break;

		case CM_AUDIO_GET_VOLUME:
			break;
			
		case CM_AUDIO_SET_VOLUME:
			break;

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE //[
		case CM_FRONT_LIGHT_SET:
			if (2==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) {
				fl_lm3630a_percentageEx (0,p);
				last_FL_duty=p;
			}
			else if(5==gptHWCFG->m_val.bFL_PWM) {
				fl_lm3630a_percentageEx (1,p);
				last_FL_duty=p;
			}
			else
				fl_set_percentage(p);
			break;

		case CM_FL_LM3630_SET:
				if (2==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) {
					fl_lm3630a_percentageEx (0,p);
				}
				else if (5==gptHWCFG->m_val.bFL_PWM) {
					fl_lm3630a_percentageEx (1,p);
				}
			break;
		case CM_FL_LM3630_TABLE:
			if (2==gptHWCFG->m_val.bFL_PWM || 4==gptHWCFG->m_val.bFL_PWM) {
				fl_lm3630a_set_color (p);
				printk ("[%s-%d] Select LM3630a Front light table %d\n",__func__,__LINE__,p);
			}
			else if(5==gptHWCFG->m_val.bFL_PWM) {
				fl_lm3630a_set_colorEx (1,p);
				printk ("[%s-%d] Select LM3630a2 Front light table %d\n",__func__,__LINE__,p);
			}
			break;

		case CM_FL_HT68F20_GETDUTY:
			if(1==gptHWCFG->m_val.bFL_PWM)
			{
				int duty;
				duty = ht68f20_read_reg(0xB0); 
				printk ("[%s-%d] Front light Duty : %d\%\n",__func__,__LINE__,duty);
			}
			break;

		case CM_FL_HT68F20_SETDUTY:
			if(0!=gptHWCFG->m_val.bFrontLight && 1==gptHWCFG->m_val.bFL_PWM)
			{
				fl_pwm_enable (FL_COLOR_FLAGS_W,1);
				if (p) {			
					printk ("\nSet front light duty : %3d\n",p);
					ht68f20_write_reg (0xA6, p&0xFF);
					ht68f20_write_reg (0xA7, p>>8);
					if (0 == last_FL_duty){
						ht68f20_write_reg (0xA3, 0x01);

						fl_pwr_enable(FL_COLOR_FLAGS_W,1,0);
					}
				}
				else {
					printk ("turn off front light\n");
					ht68f20_write_reg (0xA3, 0);

					FL_module_off(FL_COLOR_FLAGS_W);
				}
			}
			break;

		case CM_FRONT_LIGHT_AVAILABLE:
			{
			      	i = (unsigned long) (gptHWCFG->m_val.bFrontLight?1:0) ;
				copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			}
			break;

		case CM_FL_MSP430_R_DUTY:
			if( (0!=gptHWCFG->m_val.bFrontLight) && (4==gptHWCFG->m_val.bFL_PWM) )
			{
				printk ("\nSet front light R PWMCNT : 0x%4X\n",p);
				printk ("Current front light Frequency : (8MHz/%5d)\n",current_FL_freq);
				
				msp430_fl_set_duty(MSP430_FL_IDX_R,p);

			}
			break;

		case CM_FL_MSP430_W_DUTY:
			if(0!=gptHWCFG->m_val.bFrontLight)
			{
				printk ("\nSet front light W PWMCNT : 0x%4X\n",p);
				printk ("Current front light Frequency : (8MHz/%5d)\n",current_FL_freq);

				msp430_fl_set_duty(MSP430_FL_IDX_W,p);
			}
			break;

		case CM_FL_MSP430_FREQUENCY:
			if(0!=gptHWCFG->m_val.bFrontLight)
			{
				if (p) {

					printk ("set front light Frequency : (8MHz/%5d)\n",p);		
					msp430_fl_set_freq(MSP430_FL_IDX_W,p);	
					current_FL_freq = p;
				}
			}
			break;

		case CM_FRONT_LIGHT_R_EN:
			if(0!=gptHWCFG->m_val.bFrontLight)
			{
				printk ("set FL_R_EN : %d\n",p);		
				gpio_direction_output(MX6SL_FL_R_EN, p);
			}
			break;
		case CM_FL_MSP430_W_H_EN:
			if(0!=gptHWCFG->m_val.bFrontLight)
			{
				printk ("set FL_W_H_EN : %d\n",p);		
				gpio_request (MX6SL_FL_W_H_EN, "MX6SL_FL_W_H_EN");
				mxc_iomux_v3_setup_pad(MX6SL_PAD_EPDC_SDCE3__GPIO_1_30);
				gpio_direction_output(MX6SL_FL_W_H_EN, p);
			}
			break;
		case CM_FL_MSP430_PWM_EN:
			if(0!=gptHWCFG->m_val.bFrontLight)
			{
				if(p>0) {
					msp430_fl_endtime(0xffff);// disable fl end time .
					if(1==p) {
						msp430_fl_enable (MSP430_FL_IDX_W,1);
					}
					else if(2==p) {
						msp430_fl_enable (MSP430_FL_IDX_R,1);
					}
					else if(3==p){
						msp430_fl_enable (MSP430_FL_IDX_ALL,1);
					}
				}
				else {
					printk ("turn off MSP430 PWM\n");
					msp430_fl_enable (MSP430_FL_IDX_ALL,0);
				}
			}
			break;
#endif //]CONFIG_BACKLIGHT_CLASS_DEVICE 
		case CM_PLATFORM:
			copy_to_user((void __user *)arg, &platform_type, 32);
			break;
		case CM_HWCONFIG:
			copy_to_user((void __user *)arg, &hwconfig, sizeof(unsigned long
				     long));
			break;
		case CM_SET_HWCONFIG:
			if (!capable(CAP_SYS_ADMIN)) 
				return -EPERM;
			copy_from_user(&hwconfig, (void __user *)arg, sizeof(unsigned long
				       long));
			break;

#ifdef TOUCH_HOME_LED
		case CM_HOME_LED_ONOFF:
			if(0x03==gptHWCFG->m_val.bUIConfig){
				extern void homeled_onoff(int iIsON); 
				switch(p){
					case 0:
						homeled_onoff(0);
						break;
		
					case 1:
						homeled_onoff(1);
						break;

					default:
						homeled_onoff(1);
						break;
				}
			}
			break;
#endif //#ifdef TOUCH_HOME_LED

		default:
			printk("pvi_io : do not get the command [%d]\n", command);
			return -1;
	}
	return 0;
}
	
static struct file_operations driverFops= {
    .owner  	=   THIS_MODULE,
    .open   	=   openDriver,
    .unlocked_ioctl	=   ioctlDriver,
    .release    =   releaseDriver,
};
static struct miscdevice driverDevice = {
	.minor		= DEVICE_MINJOR,
	.name		= DEVICE_NAME,
	.fops		= &driverFops,
};


// ================================= Simulate MC13892 Signaling LED Driver ================================
static struct timer_list green_led_timer, blue_led_timer, red_led_timer;
static unsigned char green_led_dc, blue_led_dc, red_led_dc, \ 
					 green_led_flag, blue_led_flag, red_led_flag;
static int green_led_period,blue_led_period,red_led_period ;

static void ntx_led_set_timer (struct timer_list *pTimer, unsigned char dc, int blink)
{
	int period;
	
	if (0 == dc || 0==blink) {
		del_timer_sync(pTimer);
		return;
	}


	switch (blink) {
	case 1:
		period = 100 / 8;	// 1/8 s
		break;
	case 2:
		period = 100;	// 1 s
		break;
	case 3:
		period = 200;	// 2 s
		break;
	default:
		if(blink>10) {
			period = blink/10;
			break;
		}
		return ;
	}

	mod_timer(pTimer, jiffies + period);
}

static void green_led_blink_func (unsigned long v)
{
	green_led_flag ^= 1;
	led_green(green_led_flag?0:1);
	ntx_led_set_timer (&green_led_timer, green_led_dc, green_led_period);
}

static void blue_led_blink_func (unsigned long v)
{
	blue_led_flag ^= 1;
	led_blue(blue_led_flag?0:1);
	ntx_led_set_timer (&blue_led_timer, blue_led_dc, blue_led_period);
}

static void red_led_blink_func (unsigned long v)
{
	red_led_flag ^= 1;
	led_red(red_led_flag?0:1);
	ntx_led_set_timer (&red_led_timer, red_led_dc, red_led_period);
}

void ntx_led_blink (unsigned int channel, int period)
{
	//printk("%s,period=%d\n",__FUNCTION__,period);
	g_Cus_Ctrl_Led = 1;
	switch (channel) {
	case 3:
		red_led_period = period&3;
		ntx_led_set_timer (&red_led_timer, red_led_dc, red_led_period);
		if(0==period) {
			led_red(0);
		}
		break;
	case 4:
		//green_led_period = period&3;
		green_led_period = period;
		ntx_led_set_timer (&green_led_timer, green_led_dc, green_led_period);
		if(0==period) {
			led_green(0);
		}
		break;
	case 5:
		blue_led_period = period&3;
		ntx_led_set_timer (&blue_led_timer, blue_led_dc, blue_led_period);
		if(0==period) {
			led_blue(0);
		}
		break;
	default:
		break;
	}
}

void ntx_led_dc (unsigned int channel, unsigned char dc)
{
	LED_conitnuous = 0;
	g_Cus_Ctrl_Led = 1;
	switch (channel) {
	case 3:
		red_led_dc = dc;
		red_led_flag = (dc)?0:1;
		led_red(red_led_flag?0:1);
		ntx_led_set_timer (&red_led_timer, red_led_dc, red_led_period);
		break;
	case 4:
		green_led_dc = dc;
		green_led_flag = (dc)?0:1;
		led_green(green_led_flag?0:1);
		ntx_led_set_timer (&green_led_timer, green_led_dc, green_led_period);
		break;
	case 5:
		blue_led_dc = dc;
		blue_led_flag = (dc)?0:1;
		led_blue(blue_led_flag?0:1);
		ntx_led_set_timer (&blue_led_timer, blue_led_dc, blue_led_period);
		break;
	default:
		break;
	}
}

void ntx_led_current (unsigned int channel, unsigned char value)
{
	g_Cus_Ctrl_Led = 1;
	if (!value)
		ntx_led_dc (channel, 0);
}

static void FL_module_off(unsigned short wColorFlags) 
{

	//if(2==gptHWCFG->m_val.bFL_PWM) {
    	// Front light pwm is lm3630a no need trun on/off FL_EN anymore .
		//return;
	//}


	if(wColorFlags&FL_COLOR_FLAGS_W) {
		gpio_direction_input(gMX6SL_FL_W_H_EN);
	}
	fl_pwm_enable (wColorFlags,0);
	fl_pwr_enable (wColorFlags,0,0);
}

void FL_W_off_func(struct work_struct *work)
{
	printk("[%s-%d]FL W PWR off\n",__FUNCTION__,__LINE__);
	FL_module_off(FL_COLOR_FLAGS_W);
}
void FL_R_off_func(struct work_struct *work)
{
	printk("[%s-%d]FL R PWR off\n",__FUNCTION__,__LINE__);
	FL_module_off(FL_COLOR_FLAGS_R);
}

int FL_suspend(void){
	if(delayed_work_pending(&FL_W_off_work)){
		return -1;
	}	
	if(delayed_work_pending(&FL_R_off_work)){
		return -1;
	}	
	return 0;
}

static int sleep_thread(void)
{
int	rc = 0;

	set_current_state(TASK_INTERRUPTIBLE);
	if(signal_pending(current))
	  rc = -EINTR;
	__set_current_state(TASK_RUNNING);
	return rc;
}

static int LED_Thread(void *param)
{
	  sigset_t		thread_signal_mask;	  
  	siginitsetinv(&thread_signal_mask, sigmask(SIGKILL));
	  sigprocmask(SIG_SETMASK, &thread_signal_mask, NULL);
	  
    while(1){
      if(freezing(current)){
          printk("freeze 0 !!!!!!!!!!!!!!!!!!!!\n");
		  try_to_freeze();
      }

      if(LED_conitnuous == 0){
          interruptible_sleep_on(&LED_freeze_WaitQueue);
          if(freezing(current)){
            printk("freeze 1!!!!!!!!!!!!!!!!!!!!\n");
	    	try_to_freeze();
          }
      }
      if (g_Cus_Ctrl_Led) {
      	LED_conitnuous = 0;
      	continue;
      }
			if(9==gptHWCFG->m_val.bCustomer) {
      	led_blue(1);
			}
      led_green(1);
	  while (gKeepPowerAlive) {
	      sleep_on_timeout(&Reset_WaitQueue,HZ*2);
		  msp430_powerkeep(1);
	  }
      //start to blink LED;
      if (2 == LED_conitnuous) {
		spin_lock(&led_flash_lock);
      		LED_Flash_Count = 0;
      		LED_conitnuous = 0;
		spin_unlock(&led_flash_lock);
      	sleep_on_timeout(&LED_blink_WaitQueue,HZ/10);
			if(9==gptHWCFG->m_val.bCustomer) {
      	led_blue(0);
			}
	    led_green(0);
      	sleep_on_timeout(&LED_blink_WaitQueue,HZ/10);
				if(9==gptHWCFG->m_val.bCustomer) {
      		led_blue(1);
				}	
        led_green(1);
      	sleep_on_timeout(&LED_blink_WaitQueue,HZ/10);
      	if (!green_led_dc) {
					if(9==gptHWCFG->m_val.bCustomer) {
      			led_blue(0);
					}
	    		led_green(0);
				}
      	sleep_on_timeout(&LED_blink_WaitQueue,HZ/10);
      }
      else {
      	sleep_on_timeout(&LED_blink_WaitQueue,HZ/2);
      	if (!green_led_dc) {
					if(9==gptHWCFG->m_val.bCustomer) {
      			led_blue(0);
					}
		    	led_green(0);
				}
	    sleep_on_timeout(&LED_blink_WaitQueue,HZ/2);
      }
    }    
	return 0;
}

static struct timer_list power_key_timer;

extern void mxc_kpp_report_key(int isDown,__u16 wKeyCode);
extern void gpiokeys_report_key(int isDown,__u16 wKeyCode);
extern void mxc_kpp_report_power(int isDown);
extern void gpiokeys_report_power(int isDown);
extern void gpiokeys_report_event(unsigned int type, unsigned int code, int value);
extern void mxc_kpp_report_event(unsigned int type, unsigned int code, int value);

void ntx_report_key(int isDown,__u16 wKeyCode)
{
	if(NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,0)) {
		// no keymatrix .
		gpiokeys_report_key(isDown, wKeyCode);
	}else{
		mxc_kpp_report_key(isDown, wKeyCode);
	}
}

void ntx_report_event(unsigned int type, unsigned int code, int value)
{
	if(NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,0)) {
		// no keymatrix .
		gpiokeys_report_event(type,code,value);
	}else{
		mxc_kpp_report_event(type,code,value);
	}
}

void ntx_report_power(int isDown)
{
	ntx_report_key(isDown, KEY_POWER);	
}

static void power_key_chk(unsigned long v)
{
	int iPwrKeyState=power_key_status();

	if (iPwrKeyState) {
		++g_power_key_debounce;
		if (2 == g_power_key_debounce) {
			ntx_report_power(1);
		}
		mod_timer(&power_key_timer, jiffies + 1);
	}
	else {
		ntx_report_power(0);
	}

#if 0 //[ debug code .
	printk("%s():PwrKeyState=%d,debounce=%d,IsCustomUI=%d\n",__FUNCTION__,
		iPwrKeyState,g_power_key_debounce,gIsCustomerUi);
#endif //]
}

void power_key_int_function(void)
{
	gMxcPowerKeyIrqTriggered = 1;
	g_power_key_debounce = 0;
	mod_timer(&power_key_timer, jiffies + 1);
}

static irqreturn_t power_key_int(int irq, void *dev_id)
{
	//printk("%s !!\n",__FUNCTION__);
	power_key_int_function();
	return 0;
}

static irqreturn_t wifi_int(int irq, void *dev_id) {
	return IRQ_HANDLED;
}

int ntx_charge_status (void)
{
	int iIsUSBPlugged = 0;

	if (1==gptHWCFG->m_val.bPMIC) {
		iIsUSBPlugged = ricoh619_dcin_status();
	}
	else {
		iIsUSBPlugged = gpio_get_value (gMX6SL_NTX_ACIN_PG)?0:1;
	}
	if (!iIsUSBPlugged) {
		return 0;
	}
	else {
		return (1 | (gpio_get_value (gMX6SL_NTX_CHG)?0:2));
	}
}

int mxc_usb_plug_getstatus (void)
{
	//if (gIsCustomerUi) 
	{
		int usb_status = 0;
#if 1
		if (1==gptHWCFG->m_val.bPMIC) {
			usb_status = ricoh619_dcin_status();
			g_ioctl_USB_status = usb_status?0:1;
			return usb_status;
		}
		else
#endif 
			usb_status = gpio_get_value (gMX6SL_NTX_ACIN_PG);

		DBG_MSG("%s(),USB status=%d\n",__FUNCTION__,g_ioctl_USB_status);
		g_ioctl_USB_status = usb_status?0:1;
		return g_ioctl_USB_status;
	}
	//else {
		//return 0;
	//}
}

/*!
 *  Key raw pins interrupt handler.
 */
static irqreturn_t gpio_key_row_int(int irq, void *dev_id)
{
//	pr_info(KERN_INFO "key matrix pressed ...\n");
	return 0;
}

// NTX_GPIO_KEYS 
#define NTX_GPIO_KEYS_MAX		5
static int gi_ntx_gpio_buttons_total = 0;
static struct gpio_keys_button ntx_gpio_buttons[NTX_GPIO_KEYS_MAX] = {
	{0,},
};
static struct gpio_keys_platform_data ntx_gpio_key_data = {
  .buttons=ntx_gpio_buttons,
  .nbuttons=0,
  .rep=0,
};
static struct platform_device ntx_gpio_key_device = {
  .name = "mxckpd",
  .id = -1,
  .dev = {
    .platform_data = &ntx_gpio_key_data,
  },
};

static void mxc_pads_dse_setup(iomux_v3_cfg_t *pad_list, unsigned count,iomux_v3_cfg_t tDSE_PAD_VAL) 
{
	iomux_v3_cfg_t *p = pad_list;
	int i;
	iomux_v3_cfg_t tDSE_PAD_MASK=(iomux_v3_cfg_t)0x38<<MUX_PAD_CTRL_SHIFT;
	iomux_v3_cfg_t tDSE_PAD_Current;

	tDSE_PAD_VAL <<= MUX_PAD_CTRL_SHIFT;
	tDSE_PAD_VAL &= tDSE_PAD_MASK;

	for(i=0;i<count;i++) {
		mxc_iomux_v3_get_pad(p);

		tDSE_PAD_Current = *p;
		
		*p &= ~tDSE_PAD_MASK;
		*p |= tDSE_PAD_VAL;
		
		//printk("PAD set 0x%llx->0x%llx \n",tDSE_PAD_Current,*p);
		mxc_iomux_v3_setup_pad(*p);
		p++;
	}
}

#define GPIO_BAT_LOW_INT		IMX_GPIO_NR(3, 28)	/* COL2 */
static irqreturn_t bat_low_int(int irq, void *dev_id)
{
	printk ("[%s-%d] triggered!!\n", __func__, __LINE__);
	return 0;
}

int ntx_is_bat_critical (void)
{
	return (gpio_get_value(GPIO_BAT_LOW_INT))?0:1;
}


void ntx_fl_set_turnon_level(int iON_Lvl)
{
	giFL_ON = iON_Lvl;
	//printk("%s(%d),%d\n",__FUNCTION__,iON_Lvl,giFL_ON);
}

static int gpio_initials(void)
{
	int irq, ret;
	int error;
	
	mxc_iomux_v3_setup_pad(MX6SL_PAD_KEY_COL2__GPIO_3_28);
	gpio_request (GPIO_BAT_LOW_INT, "MX6SL_BAT_LOW");
	gpio_direction_input(GPIO_BAT_LOW_INT);
#if 0
	irq = gpio_to_irq(GPIO_BAT_LOW_INT);
	ret = request_irq(irq, bat_low_int, IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND | IRQF_EARLY_RESUME, "bat_low", 0);
	if (ret)
		pr_info("register on-off key interrupt failed\n");
	else
		enable_irq_wake(irq);
#endif
#ifndef CONFIG_ANDROID//[
	if(0==gptHWCFG->m_val.bUIStyle) {
		// Ebrmain .
		
		power_key_timer.function = power_key_chk;
		init_timer(&power_key_timer);
		/* OFF_CHK */
		#ifdef GPIOFN_PWRKEY//[
		gpiofn_register(&gtNTX_PWR_GPIO_data);
		#else //][!GPIOFN_PWRKEY

		gpio_direction_input(gMX6SL_PWR_SW);
		{
			/* Set power key as wakeup resource */
			irq = gpio_to_irq(gMX6SL_PWR_SW);
			ret = request_irq(irq, power_key_int, IRQF_TRIGGER_FALLING, "power_key", 0);
			if (ret)
				pr_info("register on-off key interrupt failed\n");
			else
				enable_irq_wake(irq);
		}
	}
	#endif //]GPIOFN_PWRKEY
#endif//]CONFIG_ANDROID

	
	gpio_direction_output(gMX6SL_IR_TOUCH_RST, 0);
	msleep(20);
	gpio_direction_input(gMX6SL_IR_TOUCH_RST);

	// MX6SL_FL_EN
	if( 0 != gptHWCFG->m_val.bFrontLight ){

		if( 0 == NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bFrontLight_Flags,0)){
			FL_module_off(FL_COLOR_FLAGS_ALL);
		}
		else {
			fl_pwr_enable(FL_COLOR_FLAGS_W,1,0);
		}
		INIT_DELAYED_WORK(&FL_W_off_work, FL_W_off_func);
		INIT_DELAYED_WORK(&FL_R_off_work, FL_R_off_func);
	}
	
#ifdef _WIFI_ALWAYS_ON_
	set_irq_type(gpio_to_irq(gMX6SL_WIFI_INT), IRQF_TRIGGER_FALLING);
#endif

	if(40==gptHWCFG->m_val.bPCB||49==gptHWCFG->m_val.bPCB||55==gptHWCFG->m_val.bPCB) {
		// E60Q5X / E60QDX / E70Q0X for EMI .
		printk("EMMC DSE set to 48 ohm \n");
		mxc_pads_dse_setup(mx6sl_brd_ntx_sd4_pads, 
				ARRAY_SIZE(mx6sl_brd_ntx_sd4_pads),(iomux_v3_cfg_t)PAD_CTL_DSE_48ohm);
	}
	else if (50==gptHWCFG->m_val.bPCB) {
		// E60QFX for EMI .
		printk("SD2 DSE set to 48 ohm \n");
		mxc_pads_dse_setup(mx6sl_ntx_sd2_wifi_pads,
				ARRAY_SIZE(mx6sl_ntx_sd2_wifi_pads),(iomux_v3_cfg_t)PAD_CTL_DSE_48ohm);
	}
	else if (51==gptHWCFG->m_val.bPCB) {
		// E60QHX for reliabilities .
		printk("SD2 DSE set to 34 ohm \n");
		mxc_pads_dse_setup(mx6sl_ntx_sd2_wifi_pads,
				ARRAY_SIZE(mx6sl_ntx_sd2_wifi_pads),(iomux_v3_cfg_t)PAD_CTL_DSE_34ohm);
	}
	else if (61==gptHWCFG->m_val.bPCB) {
		// E60QKX for ISD stabilities .

		//printk("SD4 DSE set to 48 ohm \n");
		//mxc_pads_dse_setup(mx6sl_brd_ntx_sd4_pads, 
		//		ARRAY_SIZE(mx6sl_brd_ntx_sd4_pads),(iomux_v3_cfg_t)PAD_CTL_DSE_48ohm);

		// E60QKX for reliabilities .
		printk("SD2 ESD DSE set to 34 ohm \n");
		mxc_pads_dse_setup(mx6sl_ntx_sd2_wifi_pads,
				ARRAY_SIZE(mx6sl_ntx_sd2_wifi_pads),(iomux_v3_cfg_t)PAD_CTL_DSE_34ohm);
		
	}

	// initial test point for ESD , Joseph 20100504
	return 0;
}

#include <mach/hardware.h>
#define SSI1_IO_BASE_ADDR	IO_ADDRESS(SSI1_BASE_ADDR)
#define SSI2_IO_BASE_ADDR	IO_ADDRESS(SSI2_BASE_ADDR)
#define SSI1_SCR    ((SSI1_IO_BASE_ADDR) + 0x10)
#define SSI2_SCR    ((SSI2_IO_BASE_ADDR) + 0x10)

#include <mach/arc_otg.h>
#include "crm_regs.h"
extern void __iomem *apll_base;
unsigned long gUart_ucr1;

static iomux_v3_cfg_t ntx_suspend_enter_pads[] = {
	// I2C1,I2C2
	MX6SL_PAD_I2C2_SCL__GPIO_3_14,
	MX6SL_PAD_I2C2_SDA__GPIO_3_15,
	MX6SL_PAD_I2C1_SCL__GPIO_3_12,
	MX6SL_PAD_I2C1_SDA__GPIO_3_13,
	// SD2
	MX6SL_PAD_SD2_CLK__GPIO_5_5,
	MX6SL_PAD_SD2_CMD__GPIO_5_4,
	MX6SL_PAD_SD2_DAT0__GPIO_5_1,
	MX6SL_PAD_SD2_DAT1__GPIO_4_30,
	MX6SL_PAD_SD2_DAT2__GPIO_5_3,
	MX6SL_PAD_SD2_DAT3__GPIO_4_28,
	// SD3
	MX6SL_PAD_SD3_CLK__GPIO_5_18,
	MX6SL_PAD_SD3_CMD__GPIO_5_21,
	MX6SL_PAD_SD3_DAT0__GPIO_5_19,
	MX6SL_PAD_SD3_DAT1__GPIO_5_20,
	MX6SL_PAD_SD3_DAT2__GPIO_5_16,
	MX6SL_PAD_SD3_DAT3__GPIO_5_17,      
	// SD4 
	MX6SL_PAD_FEC_TX_CLK__GPIO_4_21,
	MX6SL_PAD_FEC_MDIO__GPIO_4_20,
	MX6SL_PAD_FEC_RX_ER__GPIO_4_19,
	MX6SL_PAD_FEC_CRS_DV__GPIO_4_25,
	MX6SL_PAD_FEC_RXD1__GPIO_4_18,
	MX6SL_PAD_FEC_TXD0__GPIO_4_24,
	MX6SL_PAD_FEC_MDC__GPIO_4_23,
	MX6SL_PAD_FEC_RXD0__GPIO_4_17,
	MX6SL_PAD_FEC_TX_EN__GPIO_4_22,
	MX6SL_PAD_FEC_TXD1__GPIO_4_16,
	MX6SL_PAD_FEC_REF_CLK__GPIO_4_26,

	// TEST ONLY
	MX6SL_PAD_ECSPI1_MISO__GPIO_4_10,
	MX6SL_PAD_ECSPI1_MOSI__GPIO_4_9,
	MX6SL_PAD_ECSPI1_SCLK__GPIO_4_8,
	MX6SL_PAD_ECSPI1_SS0__GPIO_4_11,

	MX6SL_PAD_ECSPI2_MISO__GPIO_4_14,
	MX6SL_PAD_ECSPI2_MOSI__GPIO_4_13,
	MX6SL_PAD_ECSPI2_SCLK__GPIO_4_12,
	MX6SL_PAD_ECSPI2_SS0__GPIO_4_15,

	MX6SL_PAD_EPDC_BDR0__GPIO_2_5,
	MX6SL_PAD_EPDC_BDR1__GPIO_2_6,
	MX6SL_PAD_EPDC_D0__GPIO_1_7,
	MX6SL_PAD_EPDC_D1__GPIO_1_8,
	MX6SL_PAD_EPDC_D10__GPIO_1_17,
	MX6SL_PAD_EPDC_D11__GPIO_1_18,
	MX6SL_PAD_EPDC_D12__GPIO_1_19,
	MX6SL_PAD_EPDC_D13__GPIO_1_20,
	MX6SL_PAD_EPDC_D14__GPIO_1_21,
	MX6SL_PAD_EPDC_D15__GPIO_1_22,
	MX6SL_PAD_EPDC_D2__GPIO_1_9,
	MX6SL_PAD_EPDC_D3__GPIO_1_10,
	MX6SL_PAD_EPDC_D4__GPIO_1_11,
	MX6SL_PAD_EPDC_D5__GPIO_1_12,
	MX6SL_PAD_EPDC_D6__GPIO_1_13,
	MX6SL_PAD_EPDC_D7__GPIO_1_14,
	MX6SL_PAD_EPDC_D8__GPIO_1_15,
	MX6SL_PAD_EPDC_D9__GPIO_1_16,
	MX6SL_PAD_EPDC_GDCLK__GPIO_1_31,
	MX6SL_PAD_EPDC_GDOE__GPIO_2_0,
	MX6SL_PAD_EPDC_GDRL__GPIO_2_1,
	MX6SL_PAD_EPDC_GDSP__GPIO_2_2,
//	MX6SL_PAD_EPDC_PWRCOM__GPIO_2_11,
	MX6SL_PAD_EPDC_PWRCTRL0__GPIO_2_7,
	MX6SL_PAD_EPDC_PWRCTRL1__GPIO_2_8,
	MX6SL_PAD_EPDC_PWRCTRL2__GPIO_2_9,
	MX6SL_PAD_EPDC_PWRCTRL3__GPIO_2_10,
	MX6SL_PAD_EPDC_PWRINT__GPIO_2_12,
	MX6SL_PAD_EPDC_PWRSTAT__GPIO_2_13,
	MX6SL_PAD_EPDC_PWRWAKEUP__GPIO_2_14,
	MX6SL_PAD_EPDC_SDCE0__GPIO_1_27,
	MX6SL_PAD_EPDC_SDCE1__GPIO_1_28,
	MX6SL_PAD_EPDC_SDCE2__GPIO_1_29,
	MX6SL_PAD_EPDC_SDCE3__GPIO_1_30,
	MX6SL_PAD_EPDC_SDCLK__GPIO_1_23,
	MX6SL_PAD_EPDC_SDLE__GPIO_1_24,
	MX6SL_PAD_EPDC_SDOE__GPIO_1_25,
	MX6SL_PAD_EPDC_SDSHR__GPIO_1_26,
	MX6SL_PAD_EPDC_VCOM0__GPIO_2_3,
	MX6SL_PAD_EPDC_VCOM1__GPIO_2_4,

/*
ffffff80
000077ff
0000f000
53ffff00
003f003a
*/
	MX6SL_PAD_HSIC_DAT__GPIO_3_19,
	MX6SL_PAD_HSIC_STROBE__GPIO_3_20,
	MX6SL_PAD_KEY_COL0__GPIO_3_24,
	MX6SL_PAD_KEY_COL1__GPIO_3_26,
	MX6SL_PAD_KEY_COL2__GPIO_3_28,
	MX6SL_PAD_KEY_COL3__GPIO_3_30,
	MX6SL_PAD_KEY_COL4__GPIO_4_0,
	MX6SL_PAD_KEY_COL5__GPIO_4_2,
	MX6SL_PAD_KEY_COL6__GPIO_4_4,
	MX6SL_PAD_KEY_COL7__GPIO_4_6,
	MX6SL_PAD_KEY_ROW0__GPIO_3_25,
	MX6SL_PAD_KEY_ROW1__GPIO_3_27,
	MX6SL_PAD_KEY_ROW2__GPIO_3_29,
	MX6SL_PAD_KEY_ROW3__GPIO_3_31,
		MX6SL_PAD_KEY_ROW4__GPIO_4_1,
		MX6SL_PAD_KEY_ROW5__GPIO_4_3,
	MX6SL_PAD_KEY_ROW6__GPIO_4_5,
	MX6SL_PAD_KEY_ROW7__GPIO_4_7,
/*
ffffff80
000077ff
ff18f000
57ffffff
003f003a
*/
	MX6SL_PAD_LCD_CLK__GPIO_2_15,
	MX6SL_PAD_LCD_DAT0__GPIO_2_20,
	MX6SL_PAD_LCD_DAT1__GPIO_2_21,
	MX6SL_PAD_LCD_DAT10__GPIO_2_30,
	MX6SL_PAD_LCD_DAT11__GPIO_2_31,
	MX6SL_PAD_LCD_DAT12__GPIO_3_0,
	MX6SL_PAD_LCD_DAT13__GPIO_3_1,
	MX6SL_PAD_LCD_DAT14__GPIO_3_2,
	MX6SL_PAD_LCD_DAT15__GPIO_3_3,
	MX6SL_PAD_LCD_DAT16__GPIO_3_4,
	MX6SL_PAD_LCD_DAT17__GPIO_3_5,
	MX6SL_PAD_LCD_DAT18__GPIO_3_6,
	MX6SL_PAD_LCD_DAT19__GPIO_3_7,
	MX6SL_PAD_LCD_DAT2__GPIO_2_22,
	MX6SL_PAD_LCD_DAT20__GPIO_3_8,
	MX6SL_PAD_LCD_DAT21__GPIO_3_9,
	MX6SL_PAD_LCD_DAT22__GPIO_3_10,
	MX6SL_PAD_LCD_DAT23__GPIO_3_11,
	MX6SL_PAD_LCD_DAT3__GPIO_2_23,
	MX6SL_PAD_LCD_DAT4__GPIO_2_24,
	MX6SL_PAD_LCD_DAT5__GPIO_2_25,
	MX6SL_PAD_LCD_DAT6__GPIO_2_26,
	MX6SL_PAD_LCD_DAT7__GPIO_2_27,
	MX6SL_PAD_LCD_DAT8__GPIO_2_28,
	MX6SL_PAD_LCD_DAT9__GPIO_2_29,
	MX6SL_PAD_LCD_ENABLE__GPIO_2_16,
	MX6SL_PAD_LCD_HSYNC__GPIO_2_17,
	MX6SL_PAD_LCD_RESET__GPIO_2_19,
	MX6SL_PAD_LCD_VSYNC__GPIO_2_18,
	MX6SL_PAD_PWM1__GPIO_3_23,
	MX6SL_PAD_REF_CLK_24M__GPIO_3_21,
	MX6SL_PAD_REF_CLK_32K__GPIO_3_22,
	MX6SL_PAD_SD1_CLK__GPIO_5_15,
	MX6SL_PAD_SD1_CMD__GPIO_5_14,
	MX6SL_PAD_SD1_DAT0__GPIO_5_11,
	MX6SL_PAD_SD1_DAT1__GPIO_5_8,
	MX6SL_PAD_SD1_DAT2__GPIO_5_13,
	MX6SL_PAD_SD1_DAT3__GPIO_5_6,
	MX6SL_PAD_SD1_DAT4__GPIO_5_12,
	MX6SL_PAD_SD1_DAT5__GPIO_5_9,
	MX6SL_PAD_SD1_DAT6__GPIO_5_7,
	MX6SL_PAD_SD1_DAT7__GPIO_5_10,
	MX6SL_PAD_SD2_DAT4__GPIO_5_2,
	MX6SL_PAD_SD2_DAT5__GPIO_4_31,
//		MX6SL_PAD_SD2_DAT6__GPIO_4_29,
	MX6SL_PAD_SD2_DAT7__GPIO_5_0,
	MX6SL_PAD_SD2_RST__GPIO_4_27,
//	MX6SL_PAD_UART1_RXD__GPIO_3_16,
//	MX6SL_PAD_UART1_TXD__GPIO_3_17,
//	MX6SL_PAD_WDOG_B__GPIO_3_18,
/*
0xffffff80
0xfffff7ff
0xfffcffff
0xdfffffff
0x003fffff

    31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
1   1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  0  0  0  0  0  0  0
2   1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  0  1  1  1  1  1  1  1  1  1  1  1
3   1  1  1  1  1  1  1  1  1  1  1  1  1  1  0  0  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
4   1  1  0  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
5   0  0  0  0  0  0  0  0  0  0  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1

*/

};

#define dump_pad_iomux_val(_pad_name,_iomux_v3cfg,pin_func_str,desc_str)	\
		{\
			iomux_v3_cfg_t tDSE_PAD_Current;\
			tDSE_PAD_Current=_iomux_v3cfg;\
			mxc_iomux_v3_get_pad(&tDSE_PAD_Current);\
			printk("%s(%d):%s %s(%s)= 0x%llx \n",__func__,__LINE__,\
					desc_str,#_pad_name,pin_func_str,tDSE_PAD_Current);\
			printk("\tpadctrl=0x%x\n",(tDSE_PAD_Current>>MUX_PAD_CTRL_SHIFT)&0x43ffff);\
			printk("\tmode=0x%x\n",(tDSE_PAD_Current>>MUX_MODE_SHIFT)&0x1f);\
			printk("\tinput=0x%x\n",(tDSE_PAD_Current>>MUX_SEL_INPUT_SHIFT)&0xf);\
		}

static unsigned int ntx_gpio_dir[5],ntx_gpio_insuspend_dir[5];
static iomux_v3_cfg_t local_suspend_enter_pads[ARRAY_SIZE(ntx_suspend_enter_pads)];
static iomux_v3_cfg_t ntx_suspend_exit_pads[ARRAY_SIZE(ntx_suspend_enter_pads)];

#ifdef CONFIG_BACKLIGHT_LM3630A //[
extern int lm3630a_get_FL_current(void);
#else //][!CONFIG_BACKLIGHT_LM3630A
#define lm3630a_get_FL_current()	0
#endif //] CONFIG_BACKLIGHT_LM3630A

extern int fl_level;		// If FL is on, value is 0-100. If FL is off, value is 0;
extern int fl_current;		// Unit is 1uA. If FL is off, value is 0;
extern int slp_state;		// 0:Suspend, 1:Hibernate
extern int idle_current;	// Unit is 1uA.
extern int sus_current;		// Unit is 1uA.
extern int hiber_current;	// Unit is 1uA.
extern bool bat_alert_req_flg;	// 0:Normal, 1:Re-synchronize request from system

static NTX_FW_percent_current_tab *gptNTX_Percent_curr_tab;
void ntx_percent_curr_tab_set(void *pvTable)
{
	printk("%s(%p)\n",__FUNCTION__,pvTable);
	gptNTX_Percent_curr_tab = (NTX_FW_percent_current_tab *)pvTable ;
}

void ricoh_suspend_state_sync(void)
{
	int iTemp;
	const int fl_currentA[] = {
		620  , 720  , 830  , 990  , 1120 , 1300 , 1460 , 1590 , 1750 , 2020 ,	// 01 ~ 10	
		2330 , 2610 , 2900 , 3260 , 3570 , 3900 , 4100 , 4300 , 4510 , 6230 ,	// 11 ~ 20
		6640 , 6840 , 7270 , 7690 , 8100 , 8520 , 8930 , 9350 , 9810 , 10180,	// 21 ~ 30
		10610, 11010, 11430, 11860, 12310, 12700, 13160, 13560, 13960, 14370,	// 31 ~ 40
		14730, 15130, 15530, 15930, 16530, 17330, 17920, 18300, 18720, 19120,	// 41 ~ 50
		19720, 20320, 20930, 21320, 21720, 22330, 22940, 23340, 23760, 24170,	// 51 ~ 60
		24570, 25180, 25780, 26180, 26790, 27190, 27820, 28220, 28630, 29260,	// 61 ~ 70
		29860, 30500, 31090, 31700, 32310, 32740, 33140, 33760, 34200, 35230,	// 71 ~ 80
		35850, 36460, 37910, 39370, 40200, 41050, 41890, 42700, 44400, 45880,	// 81 ~ 90
		47360, 48860, 50560, 51160, 52000, 53070, 54150, 55440, 56710, 58000,	// 91 ~ 100
	};
	const int fl_current_Q5X[] = {
		400  , 490  , 580  , 730  , 840  , 1030 , 1170 , 1330 , 1480 , 1750 ,
		2080 , 2310 , 2610 , 2960 , 3270 , 3580 , 3770 , 3970 , 4170 , 5980 ,
		6390 , 6620 , 7020 , 7430 , 7850 , 8240 , 8630 , 9040 , 9450 , 9880 ,
		10290, 10690, 11090, 11510, 11940, 12350, 12730, 13150, 13570, 13990,
		14420, 14840, 15280, 15670, 16260, 17050, 17640, 18040, 18440, 18840,
		19430, 20030, 20630, 21030, 21430, 22030, 22630, 23030, 23440, 23840,
		24240, 24850, 25460, 25860, 26470, 26880, 27490, 27890, 28010, 28920,
		29530, 30150, 30770, 31390, 32000, 32420, 32830, 33450, 33870, 34910,
		35540, 36160, 37630, 39100, 39950, 40790, 41640, 42490, 44180, 45680,
		47190, 48700, 50440, 51090, 51950, 53050, 54140, 55460, 56780, 58100,
	};
	const int fl_current_Q6X[] = {
		1649 , 1718 , 1806 , 1913 , 2038 , 2209 , 2313 , 2463 , 2598 , 2824 ,
		3107 , 3333 , 3599 , 3943 , 4225 , 4507 , 4988 , 5255 , 6215 , 6488 ,
		6839 , 7039 , 7456 , 7805 , 8248 , 8569 , 9012 , 9432 , 9895 , 10330,
		10660, 11130, 11570, 11930, 12360, 12830, 13310, 13740, 14210, 14650,
		15120, 15570, 16020, 16490, 17060, 17980, 18700, 19160, 19640, 20100,
		20720, 21420, 22130, 22640, 23190, 23840, 24440, 24920, 25400, 25910,
		26390, 27120, 27860, 28360, 28960, 29570, 30190, 30690, 31190, 31940,
		32680, 33430, 34190, 34950, 35680, 36180, 36720, 37460, 37970, 39220,
		40090, 40940, 42640, 44410, 45400, 46530, 47520, 48520, 50670, 52410,
		54310, 56140, 58360, 59130, 60320, 61790, 62750, 62950, 63070, 63090,
	};
	const int fl_current_QFX[] = {
		380  , 410  , 450  , 480  , 510  , 540  , 590  , 620  , 670  , 740  ,
		790  , 830  , 880  , 920  , 960  , 1010 , 1060 , 1100 , 1150 , 1200 ,
		1330 , 1460 , 1620 , 1860 , 2050 , 2250 , 2470 , 2720 , 3010 , 3260 ,
		3460 , 3740 , 3960 , 4240 , 4730 , 5320 , 5930 , 6540 , 6960 , 7590 ,
		8220 , 8640 , 9060 , 9270 , 9690 , 9910 , 10130, 10330, 10560, 10770,
		10990, 11140, 11570, 12220, 12650, 13070, 13510, 13940, 14160, 14590,
		15220, 15820, 16420, 16820, 17430, 18030, 18640, 19250, 19860, 20480,
		21090, 21700, 22530, 23350, 23980, 24610, 25230, 25650, 26270, 26910,
		27540, 28380, 29230, 29860, 30710, 31570, 32210, 33070, 33920, 34580,
		35440, 36100, 36960, 37620, 38500, 39380, 40490, 41590, 42710, 44050, 
	};
	const int fl_current_QHX[] = {
		1508 , 1584 , 1666 , 1741 , 1817 , 1887 , 1957 , 2070 , 2184 , 2290 ,
		2429 , 2547 , 2727 , 2870 , 3040 , 3180 , 3330 , 3486 , 3638 , 3805 ,
		4038 , 4291 , 4532 , 4787 , 5026 , 5267 , 5395 , 5786 , 5997 , 6196 ,
		6411 , 6816 , 7026 , 7235 , 7443 , 7652 , 8067 , 8277 , 8489 , 8702 ,
		9129 , 9331 , 9971 , 10368, 11004, 11426, 11846, 12495, 13177, 13775,
		14570, 15369, 16173, 17183, 17994, 18809, 19627, 20655, 21477, 22306,
		23136, 23972, 24811, 25653, 26496, 27347, 28413, 29272, 30131, 30995,
		31860, 32729, 33824, 34700, 35800, 36911, 37800, 38696, 40040, 40947,
		41853, 42765, 43900, 45053, 45980, 47141, 48067, 49240, 50182, 51362,
		52309, 53736, 54900, 56143, 57350, 58572, 59790, 61030, 62514, 63799, 
	};
	const int fl_current_QKX[] = {
     25,     31,     36,     42,     45,     47,     49,     52,     54,     57,//  1- 10
     60,     63,     66,     69,     73,     76,     77,     82,     84,     91,// 11- 20
     94,     99,    106,    112,    119,    124,    131,    138,    146,    155,// 21- 30
    163,    172,    182,    191,    201,    212,    222,    234,    251,    264,// 31- 40
    277,    293,    308,    328,    345,    364,    385,    409,    429,    456,// 41- 50
    481,    508,    536,    567,    599,    633,    672,    707,    746,    788,// 51- 60
    834,    882,    997,   1052,   1113,   1173,   1229,   1292,   1372,   1441,// 61- 70
   1541,   1612,   1725,   1783,   1888,   2002,   2102,   2253,   2321,   2472,// 71- 80
   2606,   2754,   2883,   3067,   3232,   3418,   3634,   3836,   4042,   4262,// 81- 90
   4509,   4811,   5059,   5367,   5651,   5971,   6313,   6663,   7036,   7441,// 91-100
	};

	fl_level = last_FL_duty;		// If FL is on, value is 0-100. If FL is off, value is 0;

	fl_current = 0;
	if (gptHWCFG->m_val.bFrontLight) {

		switch(gptHWCFG->m_val.bPCB) {
		default :
			if(gptNTX_Percent_curr_tab) {
				if(last_FL_duty) {
					fl_current = gptNTX_Percent_curr_tab->dwCurrentA[last_FL_duty-1];
					printk("fl_current@%d=%d\n",last_FL_duty,fl_current);
				}
				break;
			}
			else
			if(2==gptHWCFG->m_val.bFL_PWM||4==gptHWCFG->m_val.bFL_PWM||
				5==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||
				7==gptHWCFG->m_val.bFL_PWM)
			{

				iTemp = lm3630a_get_FL_current();
				if(iTemp>=0) {
					fl_current = iTemp;
				}
				else {
					printk(KERN_WARNING"\n[WARNING]FL current not avalible(PCB=0x%x)\n\n",
							gptHWCFG->m_val.bPCB);
				}
				break;
			}
			else {
				printk(KERN_WARNING"\n[WARNING]there is no FL current table for this model(0x%x)\n\n",
					gptHWCFG->m_val.bPCB);
			}
		case 40://Q5X
			if(2==gptHWCFG->m_val.bFL_PWM) {
				// 11 colors temperature 2X FL
				iTemp = lm3630a_get_FL_current();
				if(iTemp>=0) 
					fl_current = iTemp;
				else 
					printk(KERN_WARNING"\n[WARNING]Q52 2X FL current not avalible\n\n");
			}
			else {
				if(last_FL_duty) {
					fl_current = fl_current_Q5X[last_FL_duty-1]; 	// Unit is 1uA. If FL is off, value is 0;
				}
			}
			break;
		case 42://Q6X
			if(last_FL_duty) 
				fl_current = fl_current_Q6X[last_FL_duty-1];
			break;
		case 46:// Q9X
			if(last_FL_duty) 
				fl_current = fl_currentA[last_FL_duty-1];
			break;
		case 50://QFX
			if(2==gptHWCFG->m_val.bFL_PWM) {
				// 11 colors temperature 2X FL
				iTemp = lm3630a_get_FL_current();
				if(iTemp>=0) 
					fl_current = iTemp;
				else 
					printk(KERN_WARNING"\n[WARNING]QF2 2X FL current not avalible\n\n");
			}
			else {
				if(last_FL_duty) 
					fl_current = fl_current_QFX[last_FL_duty-1];
			}
			break;
		case 51://QHX
			if(last_FL_duty) 
				fl_current = fl_current_QHX[last_FL_duty-1];
			break;
		case 61://QKX
			if(last_FL_duty) 
				fl_current = fl_current_QKX[last_FL_duty-1];
			break;
		}
	}

	slp_state = gSleep_Mode_Suspend?1:0;		// 0:Suspend, 1:Hibernate

	switch (gptHWCFG->m_val.bPCB) {
	default:
		printk(KERN_WARNING"\n[WARNING]there is no idle/suspend/hibernation current data for PCB(0x%x)\n\n",gptHWCFG->m_val.bPCB);
	case 42://Q6X
		sus_current = 11700;
		hiber_current = 800;
		break;
	case 46://Q9X .
		idle_current = 1000;	// Unit is 1uA.
		sus_current = 1900;		// Unit is 1uA.
		hiber_current = 889;	// Unit is 1uA.
		break;
	case 40://Q5X droid .
	case 47://ED0Q0x droid .
	case 54://ED0Q1x droid .
		idle_current = 17740;	// Unit is 1uA.
		sus_current = 5390;		// Unit is 1uA.
		hiber_current = 769;	// Unit is 1uA.
		break;
	case 50://QFX
		sus_current = 1700;
		hiber_current = 700;
		break;
	case 51://QHX
		sus_current = 2700;
		hiber_current = 700;
		break;
	case 55://E70Q0X
		idle_current = 22200;	// 22.2mA .
		sus_current = 9420; // 9.42mA .
		hiber_current = 810; // 810uA .
		break;
	case 58://E60QJX
		idle_current = 16500;
		sus_current = 3000;
		hiber_current = 1900;
		break;
	case 59://E60QLX
		idle_current = 16500;
		sus_current = 8000;
		hiber_current = 800;
		break;
	case 61://E60QKX
		idle_current = 16500;
		sus_current = 4790;
		hiber_current = 2070;
		break;
	case 60://E60QKX
		sus_current = 5200;
		hiber_current = 800;
		break;
		
	}
	bat_alert_req_flg = 0;	// 0:Normal, 1:Re-synchronize request from system
}



//#define DUMP_PADS	1
extern int mxc_epdc_fb_ep1v8_output(int iIsOutput);

void ntx_gpio_suspend (void)
{
	g_wakeup_by_alarm = 0;


	led_blue(0);
	led_green(0);

	gpiofn_suspend();

	if(61 == gptHWCFG->m_val.bPCB) {  // E60QKX
		if(!gSleep_Mode_Suspend) {
			request_irq(gpio_to_irq(gMX6SL_WIFI_INT),wifi_int, IRQF_TRIGGER_FALLING, "wifi_int", 0);
			enable_irq_wake(gpio_to_irq(gMX6SL_WIFI_INT));
		}
	}

	if (2 == gptHWCFG->m_val.bAudioCodec) {	// ALC5640 codec
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_MCLK__GPIO_1_6 );
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_RXD__GPIO_1_2 );
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXC__GPIO_1_3 );
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXD__GPIO_1_5 );
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXFS__GPIO_1_4 );
		gpio_direction_output (MX6SL_AD_LDO_EN, 0);
		gpio_direction_output (MX6SL_AD_1V8_ON, 0);
		gpio_direction_input (MX6SL_AD_3V3_ON);
	}

	if (!gSleep_Mode_Suspend && (50 == gptHWCFG->m_val.bPCB) && (0 == last_FL_duty)) {
		FL_module_off (FL_COLOR_FLAGS_ALL);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SCL__GPIO_3_12);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SDA__GPIO_3_13);
		gpio_request (IMX_GPIO_NR(3, 12), "MX6SL_i2c1_SCL");
		gpio_direction_output (IMX_GPIO_NR(3, 12), 0);

	}

	if (gSleep_Mode_Suspend) {
	
		ntx_gpio_insuspend_dir[0]=0xffffff80;
		ntx_gpio_insuspend_dir[1]=0xfffff7ff;
		ntx_gpio_insuspend_dir[2]=0xfffcffff;
		ntx_gpio_insuspend_dir[3]=0xdfffffff;
		ntx_gpio_insuspend_dir[4]=0x003fffff;

#ifdef CONFIG_FB_MXC_EINK_PANEL//[	
		if(7==gptHWCFG->m_val.bDisplayCtrl) {
			tps65185_ONOFF(0);
		}
		else if(8==gptHWCFG->m_val.bDisplayCtrl) {
			fp9928_ONOFF(0);
		}
#endif //]CONFIG_FB_MXC_EINK_PANEL
		//mxc_epdc_fb_ep1v8_output(0);
		//gpio_direction_output (GPIO_EP_3V3_ON, 0);
		//gpio_direction_output (MX6SL_EP_PWRALL, 0);
		
		
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SCL__GPIO_3_12);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SDA__GPIO_3_13);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C2_SCL__GPIO_3_14);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C2_SDA__GPIO_3_15);

		if( 3==gptHWCFG->m_val.bTouchType ||
		   (4==gptHWCFG->m_val.bTouchType && 0x03!=gptHWCFG->m_val.bUIConfig) ) {
			// turn off touch power.
			gpio_direction_output (gMX6SL_IR_TOUCH_INT, 0);
 		
			gpio_direction_output (gMX6SL_IR_TOUCH_RST, 0);
			if (1!=gptHWCFG->m_val.bPMIC)
				gpio_direction_output (GPIO_IR_3V3_ON, 0);
		}
		if(3==gptHWCFG->m_val.bTouch2Ctrl) {
			// Waltop digitizer
			if(42==gptHWCFG->m_val.bPCB) {
				gpio_direction_input (MX6SL_WALTOP_RST_3_30);
			}
			else {
				gpio_direction_input (MX6SL_WALTOP_RST);
			}
		}

	}

	gUart_ucr1 = __raw_readl(ioremap(MX6SL_UART1_BASE_ADDR, SZ_4K)+0x80);
	__raw_writel(0, ioremap(MX6SL_UART1_BASE_ADDR, SZ_4K)+0x80);

	if (gSleep_Mode_Suspend) {
	    iomux_v3_cfg_t *p = local_suspend_enter_pads;
	    int i;
       void __iomem *base;
			unsigned long dwDisableBit ;

	    /* Set PADCTRL to 0 for all IOMUX. */
    	for (i = 0; i < ARRAY_SIZE(ntx_suspend_enter_pads); i++) {
			*p = ntx_suspend_exit_pads[i] = ntx_suspend_enter_pads[i];

			if(1==gptHWCFG->m_val.bPMIC) {
				// pads for Ricoh PMIC .
				// I2C3_SCL,I2C3_SDA,CHG,BAT_LOW_INT,PMU_INT 
				if( ((*p)==(MX6SL_PAD_REF_CLK_24M__GPIO_3_21)) ||
						((*p)==(MX6SL_PAD_REF_CLK_32K__GPIO_3_22)) ||
						((*p)==(MX6SL_PAD_SD1_CLK__GPIO_5_15)) ||
						((*p)==(MX6SL_PAD_KEY_COL2__GPIO_3_28)) ||
						((*p)==(MX6SL_PAD_SD1_DAT0__GPIO_5_11)) ||
						((*p)==(MX6SL_PAD_SD1_DAT1__GPIO_5_8)))
				{
					continue ;
				}
			}
			
			if( (*p) == (MX6SL_PAD_SD3_CLK__GPIO_5_18) ||
				(*p) == (MX6SL_PAD_SD3_CMD__GPIO_5_21) ||
				(*p) == (MX6SL_PAD_SD3_DAT0__GPIO_5_19) ||
				(*p) == (MX6SL_PAD_SD3_DAT1__GPIO_5_20) ||
				(*p) == (MX6SL_PAD_SD3_DAT2__GPIO_5_16) ||
				(*p) == (MX6SL_PAD_SD3_DAT3__GPIO_5_17) )
			{	
		        *p &= ~MUX_PAD_CTRL_MASK;
    		    /* Enable the Pull down and the keeper
				 * Set the drive strength to 0.
				 */
				*p |= ((u64)0x3000 << MUX_PAD_CTRL_SHIFT);
			}
			else if( 0==gptHWCFG->m_val.bIFlash &&
				((*p) == (MX6SL_PAD_SD2_CLK__GPIO_5_5) ||
				(*p) == (MX6SL_PAD_SD2_CMD__GPIO_5_4) ||
				(*p) == (MX6SL_PAD_SD2_DAT0__GPIO_5_1) ||
				(*p) == (MX6SL_PAD_SD2_DAT1__GPIO_4_30) ||
				(*p) == (MX6SL_PAD_SD2_DAT2__GPIO_5_3) ||
				(*p) == (MX6SL_PAD_SD2_DAT3__GPIO_4_28)))
			{
		        *p &= ~MUX_PAD_CTRL_MASK;
    		    /* Enable the Pull down and the keeper
				 * Set the drive strength to 0.
				 */
				*p |= ((u64)0x3000 << MUX_PAD_CTRL_SHIFT);
			}
			else if((*p) == (MX6SL_PAD_FEC_MDIO__GPIO_4_20	)||
				(*p) == (MX6SL_PAD_FEC_TX_CLK__GPIO_4_21)||
				(*p) == (MX6SL_PAD_FEC_RX_ER__GPIO_4_19	)||
				(*p) == (MX6SL_PAD_FEC_CRS_DV__GPIO_4_25)||
				(*p) == (MX6SL_PAD_FEC_RXD1__GPIO_4_18	)||
				(*p) == (MX6SL_PAD_FEC_TXD0__GPIO_4_24	)||
				(*p) == (MX6SL_PAD_FEC_MDC__GPIO_4_23	)||
				(*p) == (MX6SL_PAD_FEC_RXD0__GPIO_4_17	)||
				(*p) == (MX6SL_PAD_FEC_TX_EN__GPIO_4_22 )||
				(*p) == (MX6SL_PAD_FEC_TXD1__GPIO_4_16	)||
				(*p) == (MX6SL_PAD_FEC_REF_CLK__GPIO_4_26))
			{
				// EMMC PADS 
#if 1
				if( 2==gptHWCFG->m_val.bIFlash )
#else 
				if (37==gptHWCFG->m_val.bPCB /* E60QBX */|| 
/* >=E60Q30A10 */((36==gptHWCFG->m_val.bPCB) && gptHWCFG->m_val.bPCB_REV>=0x10) ||
            40==gptHWCFG->m_val.bPCB /* E60Q5X */ )
#endif
				{
			        *p &= ~MUX_PAD_CTRL_MASK;
	    		    /* Enable the Pull down and the keeper
					 * Set the drive strength to 0.
					 */
					*p |= ((u64)0x3000 << MUX_PAD_CTRL_SHIFT);
				}				
			}
			else if( (*p) == (MX6SL_PAD_KEY_ROW5__GPIO_4_3) && 
					( ((36==gptHWCFG->m_val.bPCB) && gptHWCFG->m_val.bPCB_REV>=0x10) ||
					 (40==gptHWCFG->m_val.bPCB)||
					 (47==gptHWCFG->m_val.bPCB)||
					 (49==gptHWCFG->m_val.bPCB)||
					 (54==gptHWCFG->m_val.bPCB) ) )
			{
				// E60Q3X revA10 , E60Q5X , ED0Q0X , E60QDX, ED0Q1X
				// GPIO 4_3 for EP_3V3_ON .
			}
			else if((*p) == (MX6SL_PAD_KEY_ROW3__GPIO_3_31) && (giISD_3V3_ON_Ctrl!=-1) )
			{
				//only >=E60Q30A10 can turn off ISD_3V3

				gpio_direction_output (GPIO_ISD_3V3_ON, giISD_3V3_ON_Ctrl?0:1);
			}
			else if( (*p) == (MX6SL_PAD_KEY_ROW4__GPIO_4_1) ||
					(*p) == (MX6SL_PAD_KEY_ROW5__GPIO_4_3) ||
					(*p) == (MX6SL_PAD_EPDC_SDCE2__GPIO_1_29) ) {
				// pull down
                *p &= ~MUX_PAD_CTRL_MASK;
                *p |= ((u64)0x30b0 << MUX_PAD_CTRL_SHIFT);
			}
/*			else if(
				(*p) == MX6SL_PAD_EPDC_BDR0__GPIO_2_5 ||
				(*p) == MX6SL_PAD_EPDC_BDR1__GPIO_2_6 ||
	(*p) ==  MX6SL_PAD_EPDC_D0__GPIO_1_7 ||
	(*p) ==  MX6SL_PAD_EPDC_D1__GPIO_1_8 ||
	(*p) ==  MX6SL_PAD_EPDC_D10__GPIO_1_17 ||
	(*p) ==  MX6SL_PAD_EPDC_D11__GPIO_1_18 ||
	(*p) ==  MX6SL_PAD_EPDC_D12__GPIO_1_19 ||
	(*p) ==  MX6SL_PAD_EPDC_D13__GPIO_1_20 ||
	(*p) ==  MX6SL_PAD_EPDC_D14__GPIO_1_21 ||
	(*p) ==  MX6SL_PAD_EPDC_D15__GPIO_1_22 ||
	(*p) ==  MX6SL_PAD_EPDC_D2__GPIO_1_9 ||
	(*p) ==  MX6SL_PAD_EPDC_D3__GPIO_1_10 ||
	(*p) ==  MX6SL_PAD_EPDC_D4__GPIO_1_11 ||
	(*p) ==  MX6SL_PAD_EPDC_D5__GPIO_1_12 ||
	(*p) ==  MX6SL_PAD_EPDC_D6__GPIO_1_13 ||
	(*p) ==  MX6SL_PAD_EPDC_D7__GPIO_1_14 ||
	(*p) ==  MX6SL_PAD_EPDC_D8__GPIO_1_15 ||
	(*p) ==  MX6SL_PAD_EPDC_D9__GPIO_1_16 ||
	(*p) ==  MX6SL_PAD_EPDC_GDCLK__GPIO_1_31 ||
	(*p) ==  MX6SL_PAD_EPDC_GDOE__GPIO_2_0 ||
	(*p) ==  MX6SL_PAD_EPDC_GDRL__GPIO_2_1 ||
	(*p) ==  MX6SL_PAD_EPDC_GDSP__GPIO_2_2 ||
	(*p) ==  MX6SL_PAD_EPDC_PWRCOM__GPIO_2_11 ||
	(*p) ==  MX6SL_PAD_EPDC_PWRCTRL0__GPIO_2_7 ||
	(*p) ==  MX6SL_PAD_EPDC_PWRCTRL1__GPIO_2_8 ||
	(*p) ==  MX6SL_PAD_EPDC_PWRCTRL2__GPIO_2_9 ||
	(*p) ==  MX6SL_PAD_EPDC_PWRCTRL3__GPIO_2_10 ||
	(*p) ==  MX6SL_PAD_EPDC_PWRINT__GPIO_2_12 ||
	(*p) ==  MX6SL_PAD_EPDC_PWRSTAT__GPIO_2_13 ||
	(*p) ==  MX6SL_PAD_EPDC_PWRWAKEUP__GPIO_2_14 ||
	(*p) ==  MX6SL_PAD_EPDC_SDCE0__GPIO_1_27 ||
	(*p) ==  MX6SL_PAD_EPDC_SDCE1__GPIO_1_28 ||
	(*p) ==  MX6SL_PAD_EPDC_SDCE2__GPIO_1_29 ||
	(*p) ==  MX6SL_PAD_EPDC_SDCE3__GPIO_1_30 ||
	(*p) ==  MX6SL_PAD_EPDC_SDCLK__GPIO_1_23 ||
	(*p) ==  MX6SL_PAD_EPDC_SDLE__GPIO_1_24 ||
	(*p) ==  MX6SL_PAD_EPDC_SDOE__GPIO_1_25 ||
	(*p) ==  MX6SL_PAD_EPDC_SDSHR__GPIO_1_26 ||
	(*p) ==  MX6SL_PAD_EPDC_VCOM0__GPIO_2_3 ||
	(*p) ==  MX6SL_PAD_EPDC_VCOM1__GPIO_2_4 
			) {
                *p &= ~MUX_PAD_CTRL_MASK;
                *p |= ((u64)0x3000 << MUX_PAD_CTRL_SHIFT);
			}
*/
			else if ( (*p) == MX6SL_PAD_SD2_DAT6__GPIO_4_29 ||
					(*p) == MX6SL_PAD_SD1_DAT6__GPIO_5_7  ||
		((IMX_GPIO_NR(5,13)==gMX6SL_ON_LED)&&((*p)==MX6SL_PAD_SD1_DAT2__GPIO_5_13)) ||
		((IMX_GPIO_NR(5,7)==gMX6SL_ON_LED)&&((*p)==MX6SL_PAD_SD1_DAT6__GPIO_5_7)) )
			{
				// pull up
				if( (*p) == MX6SL_PAD_SD1_DAT6__GPIO_5_7&&
					(36==gptHWCFG->m_val.bPCB || 40==gptHWCFG->m_val.bPCB ||
				  49==gptHWCFG->m_val.bPCB || 50==gptHWCFG->m_val.bPCB || 
					IMX_GPIO_NR(5,7)==gMX6SL_ON_LED ) )
				{
          // E60Q3X/E60Q5X/E60QDX
					// skip set pad of GP5_7 for Green LED .
				}
				else if( (*p) == MX6SL_PAD_SD1_DAT2__GPIO_5_13 &&
						( 50==gptHWCFG->m_val.bPCB||IMX_GPIO_NR(5,13)==gMX6SL_ON_LED) )
				{
          // E60QFX
					// skip set pad of GP5_13 for Green LED .
				}
				else {
					*p &= ~MUX_PAD_CTRL_MASK;
					*p |= ((u64)0x0001b0b1 << MUX_PAD_CTRL_SHIFT);
				}
			}
			else if( ((*p) == MX6SL_PAD_EPDC_PWRCTRL3__GPIO_2_10 && \
					(IMX_GPIO_NR(2,10)==gMX6SL_FL_PWR_EN)) || \
				 	((*p) == MX6SL_PAD_KEY_ROW2__GPIO_3_29 && \
					(IMX_GPIO_NR(3,29)==gMX6SL_FL_PWR_EN)) )
			{

				if(0==giFL_ON) {
					// open drain
					*p &= ~MUX_PAD_CTRL_MASK;
					*p |= ((u64)0x000108b0 << MUX_PAD_CTRL_SHIFT);
				}
				else
					gpio_direction_output(gMX6SL_FL_PWR_EN, 0);
			}
			else {
				if( (36==gptHWCFG->m_val.bPCB||
						 40==gptHWCFG->m_val.bPCB||
						 49==gptHWCFG->m_val.bPCB||
						 2==gptHWCFG->m_val.bHOME_LED_PWM) && 
						((*p) == (MX6SL_PAD_SD1_DAT7__GPIO_5_10	)) ) 
				{
					// E60Q3X ,E60Q5X, E60QDX or HOME_LED controlled by GPIO5_10.
					// skip setting home led to floating state .
				}
				else if(50==gptHWCFG->m_val.bPCB && ((*p) == MX6SL_PAD_I2C1_SCL__GPIO_3_12 ||
						(*p) == MX6SL_PAD_I2C1_SDA__GPIO_3_13 || (*p) == MX6SL_PAD_KEY_ROW2__GPIO_3_29 ||
						(*p) == MX6SL_PAD_KEY_ROW3__GPIO_3_31 || (*p) == MX6SL_PAD_KEY_ROW4__GPIO_4_1 ||
						(*p) == MX6SL_PAD_KEY_ROW5__GPIO_4_3))
				{
					// ED0QFX .
					// pull down
	                *p &= ~MUX_PAD_CTRL_MASK;
	                *p |= ((u64)0x30b0 << MUX_PAD_CTRL_SHIFT);
				}
				else {
					// floating
					*p &= ~MUX_PAD_CTRL_MASK;
					*p |= ((u64)0x000110b0 << MUX_PAD_CTRL_SHIFT);
				}
			}
			p++;
		}

		// Direction control .
		mxc_iomux_v3_get_multiple_pads(ntx_suspend_exit_pads,
			ARRAY_SIZE(ntx_suspend_exit_pads));

#ifdef DUMP_PADS//[
		dump_pad_iomux_val(MX6SL_PAD_REF_CLK_24M,MX6SL_PAD_REF_CLK_24M__GPIO_3_21,
				"I2C3_SCL","before hibernation");
		dump_pad_iomux_val(MX6SL_PAD_REF_CLK_32K,MX6SL_PAD_REF_CLK_32K__GPIO_3_22,
				"I2C3_SDA","before hibernation");
#endif//] DUMP_PADS
		mxc_iomux_v3_setup_multiple_pads(local_suspend_enter_pads,
            ARRAY_SIZE(local_suspend_enter_pads));

#ifdef DUMP_PADS//[
		dump_pad_iomux_val(MX6SL_PAD_REF_CLK_24M,MX6SL_PAD_REF_CLK_24M__GPIO_3_21,
				"I2C3_SCL","in hibernation");
		dump_pad_iomux_val(MX6SL_PAD_REF_CLK_32K,MX6SL_PAD_REF_CLK_32K__GPIO_3_22,
				"I2C3_SDA","in hibernation");
#endif//]DUMP_PADS

		if(giFL_ON) {
			// set as output .
			if(IMX_GPIO_NR(2,10)==gMX6SL_FL_PWR_EN) {
				dwDisableBit = (unsigned long)(1<<10); // GP 2_10
				ntx_gpio_insuspend_dir[1] &= ~dwDisableBit; 
			}
			if(IMX_GPIO_NR(3,29)==gMX6SL_FL_PWR_EN) {
				dwDisableBit = (unsigned long)(1<<29); // GP 3_29
				ntx_gpio_insuspend_dir[2] &= ~dwDisableBit; 
			}
		}

		if( (36==gptHWCFG->m_val.bPCB && gptHWCFG->m_val.bPCB_REV>=0x10) ||
			 	40==gptHWCFG->m_val.bPCB || 49==gptHWCFG->m_val.bPCB) 
		{
			// >=E60Q30A1 , E60Q5X , E60QDX
			dwDisableBit = (unsigned long)(1<<3);
			ntx_gpio_insuspend_dir[3] &= ~dwDisableBit; //GP4_3 for EP_3V3_ON output
			dwDisableBit = (unsigned long)(1<<7);
			ntx_gpio_insuspend_dir[4] &= ~dwDisableBit; //GP5_7 for POWER_LED output
			dwDisableBit = (unsigned long)(1<<10);
			ntx_gpio_insuspend_dir[4] &= ~dwDisableBit; //GP5_10 for HOME_LED output
		}
		else if(47==gptHWCFG->m_val.bPCB || 54==gptHWCFG->m_val.bPCB) 
		{
			// ED0Q0X . ED0Q1X
			dwDisableBit = (unsigned long)(1<<3);
			ntx_gpio_insuspend_dir[3] &= ~dwDisableBit; //GP4_3 for EP_3V3/1V8_ON output
			dwDisableBit = (unsigned long)(1<<7);
			ntx_gpio_insuspend_dir[4] &= ~dwDisableBit; //GP5_7 for (green)ACTION_LED output
		}
		else if(50==gptHWCFG->m_val.bPCB)
		{
			dwDisableBit = (unsigned long)(1<<13);
			ntx_gpio_insuspend_dir[4] &= ~dwDisableBit; //GP5_13 for ON_LED output
		}
		else {
			// 
			// gpio direction control by others fields of hwconfig instead of PCB .
			//
			 
			// do not set ON(green/white) led as input .
			if(IMX_GPIO_NR(5,7)==gMX6SL_ON_LED) {
				dwDisableBit = (unsigned long)(1<<7); // GP?_7
				ntx_gpio_insuspend_dir[4] &= ~dwDisableBit; // GP5 .
			}
			else if (IMX_GPIO_NR(5,13)==gMX6SL_ON_LED) {
				dwDisableBit = (unsigned long)(1<<13); // GP?_13 .
				ntx_gpio_insuspend_dir[4] &= ~dwDisableBit; // GP5 .
			}
			if (2 == gptHWCFG->m_val.bAudioCodec) {	// ALC5640 codec
				ntx_gpio_insuspend_dir[3] &= ~0x05; //GP4_2 & GP4_0 for MX6SL_AD_1V8_ON & MX6SL_AD_LDO_EN output
			}

			if(2==gptHWCFG->m_val.bHOME_LED_PWM) {
				// HOME LED is controlled by GPIO @(GPIO5_10) ,do not set it as input .
				dwDisableBit = (unsigned long)(1<<10); // GP?_10 .
				ntx_gpio_insuspend_dir[4] &= ~dwDisableBit; // GP5 .
			}
		}


		if(-1!=giISD_3V3_ON_Ctrl) {
			dwDisableBit = (unsigned long)(1<<31);
			ntx_gpio_insuspend_dir[2] &= ~dwDisableBit; //GP3_31 for ISD_3V3_ON output
		}


		base = IO_ADDRESS(GPIO1_BASE_ADDR);
		ntx_gpio_dir[0] = __raw_readl(base+4);
		__raw_writel( ntx_gpio_dir[0]&(~ntx_gpio_insuspend_dir[0]), base+4);

		base = IO_ADDRESS(GPIO2_BASE_ADDR);
		ntx_gpio_dir[1] = __raw_readl(base+4);
		__raw_writel( ntx_gpio_dir[1]&(~ntx_gpio_insuspend_dir[1]), base+4);

		base = IO_ADDRESS(GPIO3_BASE_ADDR);
		ntx_gpio_dir[2] = __raw_readl(base+4);
		__raw_writel( ntx_gpio_dir[2]&(~ntx_gpio_insuspend_dir[2]), base+4);

		base = IO_ADDRESS(GPIO4_BASE_ADDR);
		ntx_gpio_dir[3] = __raw_readl(base+4);
		__raw_writel( ntx_gpio_dir[3]&(~ntx_gpio_insuspend_dir[3]), base+4);

		base = IO_ADDRESS(GPIO5_BASE_ADDR);
		ntx_gpio_dir[4] = __raw_readl(base+4);
		__raw_writel( ntx_gpio_dir[4]&(~ntx_gpio_insuspend_dir[4]), base+4);
	}
}

void ntx_gpio_resume (void)
{

	if (gSleep_Mode_Suspend) {
		void __iomem *base;

        base = IO_ADDRESS(GPIO1_BASE_ADDR);
        __raw_writel( ntx_gpio_dir[0], base+4);

        base = IO_ADDRESS(GPIO2_BASE_ADDR);
        __raw_writel( ntx_gpio_dir[1], base+4);

        base = IO_ADDRESS(GPIO3_BASE_ADDR);
        __raw_writel( ntx_gpio_dir[2], base+4);

        base = IO_ADDRESS(GPIO4_BASE_ADDR);
        __raw_writel( ntx_gpio_dir[3], base+4);

        base = IO_ADDRESS(GPIO5_BASE_ADDR);
        __raw_writel( ntx_gpio_dir[4], base+4);

	    mxc_iomux_v3_setup_multiple_pads(ntx_suspend_exit_pads,
	        ARRAY_SIZE(ntx_suspend_exit_pads));

#ifdef DUMP_PADS//[
		dump_pad_iomux_val(MX6SL_PAD_REF_CLK_24M,MX6SL_PAD_REF_CLK_24M__GPIO_3_21,
				"I2C3_SCL","exit hibernation");
		dump_pad_iomux_val(MX6SL_PAD_REF_CLK_32K,MX6SL_PAD_REF_CLK_32K__GPIO_3_22,
				"I2C3_SDA","exit hibernation");
#endif//] DUMP_PADS

	}

	if(61 == gptHWCFG->m_val.bPCB) {  // E60QKX
		if(!gSleep_Mode_Suspend) {
			free_irq(gpio_to_irq(gMX6SL_WIFI_INT),0);
		}
	}

	__raw_writel(gUart_ucr1, ioremap(MX6SL_UART1_BASE_ADDR, SZ_4K)+0x80);
	if (gSleep_Mode_Suspend) {

		if(-1!=giISD_3V3_ON_Ctrl) {
			gpio_direction_output (GPIO_ISD_3V3_ON,giISD_3V3_ON_Ctrl?1:0);
		}

		//if(1==gptHWCFG->m_val.bLed) {
			// Blue Led .
			// ON_LED# 
		//	mxc_iomux_v3_setup_pad(MX6SL_PAD_SD1_DAT2__GPIO_5_13_PULLHIGH);
		//}

		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SCL__I2C1_SCL);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SDA__I2C1_SDA);

		if(4==gptHWCFG->m_val.bTouchType && 0x03==gptHWCFG->m_val.bUIConfig) {
			// IR Touch Type & MP/RD Mode for IR tool .
			// reset ir touch
			gpio_direction_output (gMX6SL_IR_TOUCH_RST, 0);
			mdelay (20);
			gpio_direction_output (gMX6SL_IR_TOUCH_RST, 1);
		}
		else {
			// turn on ir touch power.
			if (1!=gptHWCFG->m_val.bPMIC)
				gpio_direction_output (GPIO_IR_3V3_ON, 1);
			gpio_direction_input (gMX6SL_IR_TOUCH_INT);
			mdelay (20);
			gpio_direction_output (gMX6SL_IR_TOUCH_RST, 1);
		}
		if(3==gptHWCFG->m_val.bTouch2Ctrl) {
			// Waltop digitizer
			if(42==gptHWCFG->m_val.bPCB) {
				gpio_direction_output (MX6SL_WALTOP_RST_3_30, 1);
			}
			else {
				gpio_direction_output (MX6SL_WALTOP_RST, 1);
			}
		}
	
		gpio_free(IMX_GPIO_NR(3, 14));
		gpio_free(IMX_GPIO_NR(3, 15));
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C2_SCL__I2C2_SCL);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C2_SDA__I2C2_SDA);
	
		//mxc_epdc_fb_ep1v8_output(1);
		//gpio_direction_output (GPIO_EP_3V3_ON, 1);
		//mdelay (5);
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
		if(7==gptHWCFG->m_val.bDisplayCtrl) {
			tps65185_ONOFF(1);
		}
		else if(8==gptHWCFG->m_val.bDisplayCtrl) {
			fp9928_ONOFF(1);
		}
#endif //]CONFIG_FB_MXC_EINK_PANEL
		//gpio_direction_output (MX6SL_EP_PWRALL, 1);
		if(2==gptHWCFG->m_val.bFL_PWM || 4==gptHWCFG->m_val.bFL_PWM || 5==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) {
			// Front light pwm is lm3630a.
			//fl_pwr_enable(FL_COLOR_FLAGS_ALL,1,0);
			fl_pwr_force_enable(2);
		}
	}

	if (!gSleep_Mode_Suspend && (50 == gptHWCFG->m_val.bPCB) && (0 == last_FL_duty)) {
		FL_module_off (FL_COLOR_FLAGS_ALL);
		gpio_free (IMX_GPIO_NR(3, 12));
		mxc_iomux_v3_setup_pad (MX6SL_PAD_I2C1_SCL__I2C1_SCL);
		mxc_iomux_v3_setup_pad (MX6SL_PAD_I2C1_SDA__I2C1_SDA);
	}

	gpiofn_resume();

	if(9!=gptHWCFG->m_val.bCustomer) {
		if(36==gptHWCFG->m_val.bPCB || 40==gptHWCFG->m_val.bPCB ||
			 49==gptHWCFG->m_val.bPCB || 50==gptHWCFG->m_val.bPCB || 
			 58==gptHWCFG->m_val.bPCB || 
			 ((gptHWCFG->m_val.bPCB>=61)&&(gMX6SL_CHG_LED==gMX6SL_ON_LED)) )
		{
    	// E60Q3X/E60Q5X/E60QDX/E60QFX/E60QJX
			// PCB > E60QKX && G==R LED .
			if(mxc_usb_plug_getstatus()) {
				led_red(1);
			}
			else {
				led_red(0);
			}
		}
	}

	if(3==gptHWCFG->m_val.bRSensor) {	// record KL25 INT status
		gWakeUpbyKL25 = kl25_int_status();
	}

	if (2 == gptHWCFG->m_val.bAudioCodec) {	// ALC5640 codec
		gpio_direction_output (MX6SL_AD_1V8_ON, 1);
		gpio_direction_output (MX6SL_AD_3V3_ON, 0);
		gpio_direction_output (MX6SL_AD_LDO_EN, 1);
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_MCLK__AUDMUX_AUDIO_CLK_OUT );
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_RXD__AUDMUX_AUD3_RXD );
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXC__AUDMUX_AUD3_TXC );
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXD__AUDMUX_AUD3_TXD );
		mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXFS__AUDMUX_AUD3_TXFS );
	}

#ifdef CONFIG_ANDROID //[
#else //][!CONFIG_ANDROID	
	g_power_key_pressed = power_key_status();	// POWER key
	if(0==gptHWCFG->m_val.bUIStyle) {
		// Ebrmain .
		if (g_power_key_pressed) {
			power_key_int_function();
		}
	}

	if (LED_conitnuous) {
		wake_up_interruptible(&LED_freeze_WaitQueue);
	}
	else {
		//ntx_led_blink (3, red_led_period);
		//ntx_led_blink (4, green_led_period);
		//ntx_led_blink (5, blue_led_period);
	}
#endif //]CONFIG_ANDROID
}


static int set_touch_resetting_status(int status)
{
	touch_under_resetting = status;
}

int is_touch_resetting(void)
{
	return touch_under_resetting;
}


#include <linux/regulator/consumer.h>
void ntx_gpio_touch_reset (void)
{
	struct regulator *reg_TP3V3 = NULL;
	
	set_touch_resetting_status(1);

	printk ("[%s-%d] reset touch.\n",__func__,__LINE__); 
	mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SCL__GPIO_3_12);
	mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SDA__GPIO_3_13);

	gpio_request(IMX_GPIO_NR(3, 12),"i2c1_scl");
	gpio_request(IMX_GPIO_NR(3, 13),"i2c1_sda");
	gpio_direction_output (IMX_GPIO_NR(3, 12), 0);
	gpio_direction_output (IMX_GPIO_NR(3, 13), 0);
	// turn off ir touch power.
	gpio_direction_output (gMX6SL_IR_TOUCH_INT, 0);
	gpio_direction_output(gMX6SL_IR_TOUCH_RST, 0);
	if (1==gptHWCFG->m_val.bPMIC) {
		reg_TP3V3 = regulator_get(NULL,"vdd_ir_3v3");
		if (IS_ERR(reg_TP3V3))
			printk ("[%s-%d] regulator_get vdd_ir_3v3 returned NULL!!\n",__func__,__LINE__); 
		else {
			regulator_force_disable (reg_TP3V3);
		}
	}
	else
		gpio_direction_output (GPIO_IR_3V3_ON, 0);
	msleep (200);
	// turn on ir touch power.
	if (1==gptHWCFG->m_val.bPMIC) {
		if (IS_ERR(reg_TP3V3))
			printk ("[%s-%d] regulator_get vdd_ir_3v3 returned NULL!!\n",__func__,__LINE__); 
		else {
			regulator_enable (reg_TP3V3);
			regulator_put (reg_TP3V3);
		}
	}
	else
		gpio_direction_output (GPIO_IR_3V3_ON, 1);
	gpio_free(IMX_GPIO_NR(3, 12));
	gpio_free(IMX_GPIO_NR(3, 13));
	mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SCL__I2C1_SCL);
	mxc_iomux_v3_setup_pad(MX6SL_PAD_I2C1_SDA__I2C1_SDA);
	gpio_direction_input (gMX6SL_IR_TOUCH_INT);
	mdelay (20);
	gpio_direction_output (gMX6SL_IR_TOUCH_RST, 1);

	set_touch_resetting_status(0);
}

void ntx_gpio_touch_easyreset (void)
{
	
	printk ("[%s-%d] pull touch reset pin.\n",__func__,__LINE__); 

	set_touch_resetting_status(1);

	gpio_direction_output(gMX6SL_IR_TOUCH_RST, 0);
	mdelay (20);
	gpio_direction_output (gMX6SL_IR_TOUCH_RST, 1);

	set_touch_resetting_status(0);
}

int ntx_i2c_recovery(int iI2C_Chn_Idx)
{
	int retryCnt=20;
	unsigned long dwGPIO_SDA,dwGPIO_SCL;
	iomux_v3_cfg_t tPAD_SDA_GPIO,tPAD_SCL_GPIO,tPAD_SDA,tPAD_SCL;
	unsigned long dwTickBegin = jiffies;
	unsigned long dwTickTimeout = dwTickBegin+(HZ*5);

	int iRecoverySCL_Cnt;

	switch(iI2C_Chn_Idx) {
	case 0://I2C1
		dwGPIO_SDA = MX6SL_I2C1_SDA;
		dwGPIO_SCL = MX6SL_I2C1_SCL;
		tPAD_SDA_GPIO = MX6SL_PAD_I2C1_SDA__GPIO_3_13;
		tPAD_SCL_GPIO = MX6SL_PAD_I2C1_SCL__GPIO_3_12;
		tPAD_SDA = MX6SL_PAD_I2C1_SDA__I2C1_SDA;
		tPAD_SCL = MX6SL_PAD_I2C1_SCL__I2C1_SCL;
		break;
	case 1://I2C2
		dwGPIO_SDA = MX6SL_I2C2_SDA;
		dwGPIO_SCL = MX6SL_I2C2_SCL;
		tPAD_SDA_GPIO = MX6SL_PAD_I2C2_SDA__GPIO_3_15;
		tPAD_SCL_GPIO = MX6SL_PAD_I2C2_SCL__GPIO_3_14;
		tPAD_SDA = MX6SL_PAD_I2C2_SDA__I2C2_SDA;
		tPAD_SCL = MX6SL_PAD_I2C2_SCL__I2C2_SCL;
		break;
	case 2://I2C3
		dwGPIO_SDA = MX6SL_I2C3_SDA;
		dwGPIO_SCL = MX6SL_I2C3_SCL;
		tPAD_SDA_GPIO = MX6SL_PAD_REF_CLK_32K__GPIO_3_22;
		tPAD_SCL_GPIO = MX6SL_PAD_REF_CLK_24M__GPIO_3_21;
		tPAD_SDA = MX6SL_PAD_REF_CLK_32K__I2C3_SDA;
		tPAD_SCL = MX6SL_PAD_REF_CLK_24M__I2C3_SCL;
		break;

	default:
		printk(KERN_ERR"[WARNING] %s(%d) : chn not supported !!!\n",__FUNCTION__,iI2C_Chn_Idx);
		return 0;
		break;
	}

	gpio_request(dwGPIO_SDA, "i2c_sda");
	gpio_direction_input (dwGPIO_SDA);
	mxc_iomux_v3_setup_pad(tPAD_SDA_GPIO);

	if( gpio_get_value(dwGPIO_SDA) ) {
		// SDA is OK .
		gpio_direction_output (dwGPIO_SDA,0);
		udelay (2);
		gpio_free(dwGPIO_SDA);
		mxc_iomux_v3_setup_pad(tPAD_SDA);

		printk("%s(%d):nothing to do\n",__FUNCTION__,iI2C_Chn_Idx);
		return 0;
	}

	mxc_iomux_v3_setup_pad(tPAD_SCL_GPIO);
	gpio_request(dwGPIO_SCL, "i2c_scl");
	gpio_direction_input (dwGPIO_SCL);

	if(!gpio_get_value(dwGPIO_SCL))
	{
		printk(KERN_ERR"%s(%d):SCL is pulled down by device,waiting for release ...\n",__FUNCTION__,iI2C_Chn_Idx);
		while (!gpio_get_value(dwGPIO_SCL)) {
			if(time_after(jiffies,dwTickTimeout)) {
				printk(KERN_ERR"%s(%d):SCL still pulled down by device after %lu ms\n",__FUNCTION__,iI2C_Chn_Idx,jiffies_to_msecs(dwTickTimeout-dwTickBegin));
				retryCnt=0;
				goto release_gpio;
			}
		}
		printk(KERN_ERR"%s(%d):SCL pulled high ready .\n",__FUNCTION__,iI2C_Chn_Idx);
	}

	iRecoverySCL_Cnt=0;
	gpio_direction_output (dwGPIO_SCL, 1);
	// send clock out until i2c SDA released.
	while (retryCnt-- && !gpio_get_value (dwGPIO_SDA)) {
		gpio_set_value (dwGPIO_SCL,1);
		udelay(20);
		gpio_set_value (dwGPIO_SCL,0);
		//if(in_atomic() || in_interrupt()) {
			udelay(20);
		//}
		//else {
		//	schedule_timeout(1);
		//}
		++iRecoverySCL_Cnt;
	}
	// simulate i2c stop signal
	gpio_direction_output (dwGPIO_SDA,0);

release_gpio:
	gpio_free(dwGPIO_SCL);
	mxc_iomux_v3_setup_pad(tPAD_SCL);

	udelay(2);

	gpio_free(dwGPIO_SDA);
	mxc_iomux_v3_setup_pad(tPAD_SDA);
	mdelay(1);

	if(retryCnt<=0) {
		// recovery failed !!
		printk(KERN_ERR"%s(%d):is failed,recoverySCL=%d\n",
				__FUNCTION__,iI2C_Chn_Idx,iRecoverySCL_Cnt);
		return -1;
	}
	else {
		// recovery ok !!
		printk(KERN_ERR"%s(%d):is successful,recoverySCL=%d\n",
				__FUNCTION__,iI2C_Chn_Idx,iRecoverySCL_Cnt);
		return 1;
	}
}

//void ntx_msp430_i2c_force_release (void)
//{
//	ntx_i2c_recovery(2);// i2c3 .
//}

void ntx_machine_restart(char mode, const char *cmd)
{	
	if (cmd && ('\0' != *cmd)) {
		printk("%s mode=%c,cmd=%s\n",__FUNCTION__,mode,cmd);
		arm_machine_restart (mode, cmd);
	}	
	else {
		printk ("[%s-%d] do hardware reset...\n",__func__,__LINE__);
		ntx_system_reset(0);
	}
}

void ntx_machine_poweroff(void)
{
	ntx_system_poweroff(__FUNCTION__);
}

static int __init initDriver(void)
{
	int ret;
        
	ret = misc_register(&driverDevice);
	if (ret < 0) {
		printk("pvi_io: can't get major number\n");
		return ret;
	}

    gpio_initials();

	//start a kernel thread;
	ret = kernel_thread(LED_Thread,NULL,CLONE_KERNEL);
	if(ret < 0){
	    printk("LED thread creat error\n");
	}

	////////////////////////
	green_led_timer.function = green_led_blink_func;
	init_timer(&green_led_timer);
	blue_led_timer.function = blue_led_blink_func;
	init_timer(&blue_led_timer);
	red_led_timer.function = red_led_blink_func;
	init_timer(&red_led_timer);

	pm_power_off = ntx_machine_poweroff;
	arm_pm_restart = ntx_machine_restart;

	return 0;
}
static void __exit exitDriver(void) {
	misc_deregister(&driverDevice);
}
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joe");
MODULE_VERSION("2007-9-20");
MODULE_DESCRIPTION ("PVI_IO driver");
module_init(initDriver);
module_exit(exitDriver);

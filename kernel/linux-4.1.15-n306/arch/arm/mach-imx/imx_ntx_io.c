

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/power_supply.h>

#include <linux/gpio_keys.h>

#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>

#include <asm/system_misc.h>

#include "ntx_hwconfig.h"
#include "ntx_firmware.h"
#include "ntx_led.h"


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


extern int gSleep_Mode_Suspend;
extern volatile NTX_HWCONFIG *gptHWCFG;

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

static int Driver_Count = -1;
static int gKeepPowerAlive;
int g_wakeup_by_alarm;
static unsigned int g_initialize = 0;


struct gpio_desc *GPIO_WIFI_3V3;
struct gpio_desc *GPIO_WIFI_RST;
struct gpio_desc *GPIO_WIFI_INT;
struct gpio_desc *GPIO_WIFI_DIS;
struct gpio_desc *GPIO_BT_DIS;
struct gpio_desc *GPIO_BT_INT;
static DEFINE_MUTEX(ntx_wifi_power_mutex);
static int gi_wifi_power_status = 0;

static int _Percent_to_NtxBattADC(int iPercent)
{
	int iRet;
	if(0>=iPercent) {
		return 0x8000;
	}
	iRet = 885 + (1023-885)*iPercent/100;
	return iRet;
}


static int giPowerKeyState;
#define MAX_QUEUE 6
char g_PowerKeyQueue[MAX_QUEUE] = {0};
static int g_PKQFront = -1;
static int g_PKQRear = -1;
static int g_PKQFlag = 0 ;
static int PowerKeyQueueIsFull(void)
{
	return (g_PKQRear % MAX_QUEUE) == g_PKQFront ;	
}

static int  PowerKeyQueueIsEmpty(void)
{
	return g_PKQFront == g_PKQRear ;
}

static void PowerKeyEnQueue(int state)
{
	if(PowerKeyQueueIsFull() && g_PKQFlag==1 || g_PKQRear == MAX_QUEUE && g_PKQFront == -1)
	{
		//printk("## PowerKeyQueue is full \r\n");
		return ;
	}
	g_PKQRear = (g_PKQRear + 1) % MAX_QUEUE ;
	g_PowerKeyQueue[g_PKQRear] = state;
	if(g_PKQFront == g_PKQRear)
		g_PKQFlag = 1 ;
}

static int PowerKeyDeQueue(void)
{
	int state = -1 ;
	if(PowerKeyQueueIsEmpty() && g_PKQFlag==0)
	{
		//printk("## PowerKeyQueue is empty \r\n");		
		if(g_PKQRear==-1 && g_PKQFront==-1)
		{
			printk("## Init PowerKeyQueue \r\n");
			return 1;
		}
		return  -1;
	}
	g_PKQFront = (g_PKQFront +1 )% MAX_QUEUE ;
	state = g_PowerKeyQueue[g_PKQFront];
	g_PowerKeyQueue[g_PKQFront] = -1 ;		
	if(g_PKQFront==g_PKQRear)
		g_PKQFlag = 0;
	return state ;
}

static int ntx_power_key_hook(struct gpio_keys_button *I_gpio_btn_data,int state)
{
	printk("%s():state=%d\n",__FUNCTION__,state);
	PowerKeyEnQueue(state);
	if(state) {
		giPowerKeyState = 1;
	}
	else {
		giPowerKeyState = 0;
	}
	return 0;
}

gpio_keys_callback ntx_get_power_key_hookfn(void)
{
	return ntx_power_key_hook;
}

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

static int ntx_wifi_bt_power_init(void)
{
#ifdef CONFIG_MMC
	struct device_node *np;
	struct platform_device *pdev;
	
	np = of_find_node_by_path("/regulators/wifi_regulator");
	if (np)
	{
		pdev = of_find_device_by_node(np);
		if (!pdev) {
			printk(KERN_ERR "%s():device not found !!\n", __FUNCTION__);
			of_node_put(np);
			return 0;
		}
	} else {
		printk(KERN_ERR "%s(): device node not found !!\n", __FUNCTION__);
		return 0;
	}
	
	GPIO_WIFI_3V3 = gpiod_get(&pdev->dev, "wifipwr", GPIOD_ASIS);
	if (IS_ERR(GPIO_WIFI_3V3)) {
		printk("Get GPIO_WIFI_3V3 descripter failed\n");
	}
	gpiod_direction_output(GPIO_WIFI_3V3, 0);
	
	if (11 == gptHWCFG->m_val.bWifi || 12 == gptHWCFG->m_val.bWifi || 14 == gptHWCFG->m_val.bWifi) {
		GPIO_WIFI_DIS = gpiod_get(&pdev->dev, "wifidis", GPIOD_ASIS);
		if (IS_ERR(GPIO_WIFI_DIS)) {
			printk("Get GPIO_WIFI_DIS descripter failed\n");
		}
		gpiod_direction_output(GPIO_WIFI_DIS, 0);
	}
	
	GPIO_WIFI_RST = gpiod_get(&pdev->dev, "wifirst", GPIOD_ASIS);
	if (IS_ERR(GPIO_WIFI_RST)) {
		printk("Get GPIO_WIFI_RST descripter failed\n");
	}
	gpiod_direction_output(GPIO_WIFI_RST, 0);
	
	GPIO_WIFI_INT = gpiod_get(&pdev->dev, "wifiint", GPIOD_ASIS);
	if (IS_ERR(GPIO_WIFI_INT)) {
		printk("Get GPIO_WIFI_INT descripter failed\n");
	}
	gpiod_direction_input(GPIO_WIFI_INT);
	
	if (11 == gptHWCFG->m_val.bWifi || 12 == gptHWCFG->m_val.bWifi || 14 == gptHWCFG->m_val.bWifi) {
		GPIO_BT_DIS = gpiod_get(&pdev->dev, "btdis", GPIOD_ASIS);
		if (IS_ERR(GPIO_BT_DIS)) {
			printk("Get GPIO_BT_DIS descripter failed\n");
		}
		//gpiod_direction_output(GPIO_BT_DIS, 0);
	
		GPIO_BT_INT = gpiod_get(&pdev->dev, "btint", GPIOD_ASIS);
		if (IS_ERR(GPIO_BT_INT)) {
			printk("Get GPIO_BT_INT descripter failed\n");
		}
		//gpiod_direction_input(GPIO_BT_INT);
	}
	
	printk("%s() : done \n",__FUNCTION__);
#endif
	return 1;
}

extern void wifi_card_detect(bool);
static void _ntx_wifi_bt_power_ctrl (int power_status)
{
#ifdef CONFIG_MMC
	int iOldStatus;

	iOldStatus = gi_wifi_power_status;

	if (!g_initialize) {
		printk("%s() : ntx_wifi_power_ctrl return \n",__FUNCTION__);
		return;
	}
	
	printk("%s g_initialize = %d\n", __func__, g_initialize);
	printk("Wifi / BT power control %d\n", power_status);
	
	if (power_status & 0x3)	{
		printk("wifi 3v3 power on\n");
		gpiod_direction_output(GPIO_WIFI_3V3, 1);  /* wifi 3V3 turn on */
	} 
	
	
	/* Bluetooth power control */
	if (11 == gptHWCFG->m_val.bWifi || 12 == gptHWCFG->m_val.bWifi || 14 == gptHWCFG->m_val.bWifi) {
		if ((power_status & 0x2) == (iOldStatus & 0x2)) {  /* Bluetooth dis status */
			printk("Bluetooth already %s.\n",(power_status & 0x2)?"on":"off");
		} else if (power_status & 0x2) {
			printk("Bluetooth On\n");
			gpiod_direction_output(GPIO_BT_DIS, 1);  /* BT dis pull high */
			gi_wifi_power_status |= 2;
		} else {
			printk("Bluetooth Off\n");
			gpiod_direction_output(GPIO_BT_DIS, 0);  /* BT dis pull low */
			gi_wifi_power_status &= ~2;
		}
	}
	
	/* Wifi power control */
	if ((power_status & 0x1) == (iOldStatus & 0x1)) {  /* check Wifi status */
		printk ("Wifi already %s.\n",(power_status & 0x1)?"on":"off");
	} else if (power_status & 0x1) {
		gpiod_direction_output(GPIO_WIFI_RST, 0);
		msleep(20);
		if (11 == gptHWCFG->m_val.bWifi || 12 == gptHWCFG->m_val.bWifi || 14 == gptHWCFG->m_val.bWifi) {
			gpiod_direction_output(GPIO_WIFI_DIS, 1); /* Wifi dis pull high */
			gpiod_direction_input(GPIO_WIFI_INT);
		}
		msleep(20);
		printk("Wifi On\n");
		gpiod_direction_output(GPIO_WIFI_RST, 1);
		
		gi_wifi_power_status |= 1;
	} else {
		gpiod_direction_output(GPIO_WIFI_RST, 0);
		if (11 == gptHWCFG->m_val.bWifi || 12 == gptHWCFG->m_val.bWifi || 14 == gptHWCFG->m_val.bWifi) {
			gpiod_direction_output(GPIO_WIFI_DIS, 0); /* Wifi dis pull low */
			printk("Wifi Off\n");
			gpiod_direction_input(GPIO_WIFI_INT);
		}
		
		gi_wifi_power_status &= ~1;
	}
	
	if ((power_status) && (iOldStatus)) {
		printk("Device already exist !!\n");
	}
	else if (power_status)
		wifi_card_detect(true);
	else
		wifi_card_detect(false);


	if ( !(power_status & 0x3) ){
		printk("wifi 3v3 power off\n");
		gpiod_direction_output(GPIO_WIFI_3V3, 0);  /* wifi 3V3 turn off */
	}

	printk("%s() : done \n",__FUNCTION__);
#endif
}


void ntx_bt_power_ctrl(int iIsBTEnable)
{
	if (!g_initialize) {
		g_initialize = ntx_wifi_bt_power_init();
	}
	
	mutex_lock(&ntx_wifi_power_mutex);
	if (iIsBTEnable)
		_ntx_wifi_bt_power_ctrl(gi_wifi_power_status | 2);
	else
		_ntx_wifi_bt_power_ctrl(gi_wifi_power_status & ~2);
	mutex_unlock(&ntx_wifi_power_mutex);
}
EXPORT_SYMBOL(ntx_bt_power_ctrl);

void ntx_wifi_power_ctrl(int iIsWifiEnable)
{
	if (!g_initialize) {
		g_initialize = ntx_wifi_bt_power_init();
	}
	
	if (11 == gptHWCFG->m_val.bWifi || 12 == gptHWCFG->m_val.bWifi || 14 == gptHWCFG->m_val.bWifi) { 
		mutex_lock(&ntx_wifi_power_mutex);
		if (iIsWifiEnable)
			_ntx_wifi_bt_power_ctrl(gi_wifi_power_status | 1);
		else
			_ntx_wifi_bt_power_ctrl(gi_wifi_power_status & ~1);
		mutex_unlock(&ntx_wifi_power_mutex);
	} else {
		_ntx_wifi_bt_power_ctrl(iIsWifiEnable);
	}
}

EXPORT_SYMBOL(ntx_wifi_power_ctrl);

int ntx_esd_cd (void)
{
	struct device_node *np;

	np = of_find_node_by_path("/soc/aips-bus@02100000/usdhc@02194000");
	if(np)
	{
		int cd_gpio = of_get_named_gpio(np, "cd-gpios", 0);
		if (gpio_is_valid(cd_gpio))
			return gpio_get_value (cd_gpio)?0:1;

		of_node_put(np);
	}
	else {
		printk(KERN_ERR"%s(): /soc/aips-bus@02100000/usdhc@02194000 device node not found !!\n",__FUNCTION__);
	}
	return 0;
}


#ifdef CONFIG_MFD_RICOH619 //[
extern int ricoh619_restart(void);
extern int ricoh619_power_off(void);
#endif //]CONFIG_MFD_RICOH619

static void ntx_system_reset(const char *pszDomain)
{

	if(pszDomain) {
		printk("%s() ---%s reset ---\n",__FUNCTION__,pszDomain);
	}


	while (1) {
		gKeepPowerAlive = 0;
#ifdef CONFIG_MFD_RICOH619 //[
		if (1==gptHWCFG->m_val.bPMIC) {
			// RC5T19 .
			printk("%s() --- RC5T19 restarting system ---\n",__FUNCTION__);
			ricoh619_restart();
		}
		else 
#endif //]CONFIG_MFD_RICOH619
		{
			printk("%s():TBD\n",__FUNCTION__);
		}

		msleep(500);
	}
}

static void ntx_system_poweroff(const char *pszDomain)
{
	if(pszDomain) {
		printk("%s() ---%s poweroff ---\n",__FUNCTION__,pszDomain);
	}

	if (!gKeepPowerAlive) {
#ifdef CONFIG_NTX_LED
		ntx_led_set_auto_blink(0);
		led_green(0);
#endif
		while (1) {
#ifdef CONFIG_MFD_RICOH619 //[
			if(1==gptHWCFG->m_val.bPMIC) {
				printk("RC5T619 ---Power Down ---\n");
				ricoh619_power_off();
			}
			else 
#endif //]CONFIG_MFD_RICOH619
			{
				printk("%s():TBD\n",__FUNCTION__);
			}
			msleep(500);
		}
	}
	else {
		printk("Kernel---in keep alive mode ---\n");
	}
}


void ntx_machine_restart(char mode, const char *cmd)
{	
	if (cmd && ('\0' != *cmd)) {
		printk("%s mode=%c,cmd=%s\n",__FUNCTION__,mode,cmd);
		do_kernel_restart(cmd);
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

#ifdef CONFIG_BACKLIGHT_LM3630A //[
extern int lm3630a_get_FL_current(void);
#else //][!CONFIG_BACKLIGHT_LM3630A
#define lm3630a_get_FL_current()	0
#endif //] CONFIG_BACKLIGHT_LM3630A

#ifdef CONFIG_BACKLIGHT_TLC5947
extern int tlc5947_get_FL_current(void);
#else
#define tlc5947_get_FL_current()	0
#endif

//unsigned int  last_FL_duty = 0;

extern int fl_level;		// If FL is on, value is 0-100. If FL is off, value is 0;
extern int fl_current;		// Unit is 1uA. If FL is off, value is 0;
extern int slp_state;		// 0:Suspend, 1:Hibernate
extern int idle_current;	// Unit is 1uA.
extern int sus_current;		// Unit is 1uA.
extern int hiber_current;	// Unit is 1uA.
extern bool bat_alert_req_flg;	// 0:Normal, 1:Re-synchronize request from system

static NTX_FW_percent_current_tab *gptNTX_Percent_curr_tab;


int ntx_get_fl_percent(int *O_piPercent);

void ntx_percent_curr_tab_set(void *pvTable)
{
	gptNTX_Percent_curr_tab = (NTX_FW_percent_current_tab *)pvTable ;
}
#ifdef CONFIG_MFD_RICOH619 //[


void ricoh_suspend_state_sync(void)
{
	int  last_FL_duty ;
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

	ntx_get_fl_percent(&last_FL_duty);

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
			else if(8==gptHWCFG->m_val.bFL_PWM)
			{
				iTemp = tlc5947_get_FL_current();
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
		if(0==sus_current) {
			printk(KERN_WARNING"\n[WARNING]there is no suspend current data for PCB(0x%x)\n\n",gptHWCFG->m_val.bPCB);
		}
		if(0==hiber_current) {
			printk(KERN_WARNING"\n[WARNING]there is no hibernation current data for PCB(0x%x)\n\n",gptHWCFG->m_val.bPCB);
		}
		break;
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
#ifdef CONFIG_SOC_IMX6SLL//[
		idle_current = 16500;
		sus_current = 8108;
		hiber_current = 576;
#else //][!CONFIG_SOC_IMX6SLL
		idle_current = 16500;
		sus_current = 8000;
		hiber_current = 800;
#endif //]CONFIG_SOC_IMX6SLL
		break;
	case 61://E60QKX
		idle_current = 16500;
		sus_current = 4790;
		hiber_current = 2070;
		break;
	case 60://E60QMX
		sus_current = 5200;
		hiber_current = 800;
		break;
	case 74://E60K00
		sus_current = 3030;	// 3.03mA
		hiber_current = 657;	// 0.657mA
		break;
/*
 * define 
 * ricoh,fg-sus-curr
 * ricoh,fg-hiber-curr
 * in dts 
	case 75://E80K00
		sus_current = 4560;	// 4.56mA
		hiber_current = 690;	// 0.69mA
		break;
*/
	}
	bat_alert_req_flg = 0;	// 0:Normal, 1:Re-synchronize request from system
}

#endif //]CONFIG_MFD_RICOH619


const char bd7181x_bat_psy_name[] = "bd7181x_bat";
const char rc5t619_bat_psy_name[] = "mc13892_bat";

static int _Is_USB_plugged(void)
{

	char *psy_name=0;
	struct power_supply *psy;
	int iChk;
	int iRet = 0;

	if(1==gptHWCFG->m_val.bPMIC) {
		// Ricoh PMIC .
		psy_name = rc5t619_bat_psy_name;
	}
	else if(3==gptHWCFG->m_val.bPMIC) {
		// Rohm PMIC .
		psy_name = bd7181x_bat_psy_name;
	}

	psy = power_supply_get_by_name(psy_name);

	if(psy) {
		union power_supply_propval psy_propval;
		iChk = power_supply_get_property(psy,POWER_SUPPLY_PROP_STATUS,&psy_propval);
		if(iChk>=0) {
			switch (psy_propval.intval) {
			case POWER_SUPPLY_STATUS_DISCHARGING:
				iRet = 0;
				break;
			default :
				iRet = 1;
				break;
			}
		}
		else {
			iRet = 0;
			printk(KERN_ERR"%s():%s power supply propval get failed !!\n",
					__FUNCTION__,psy_name);
		}
	}
	else {
		iRet = 0;
		printk(KERN_ERR"%s power supply getting failed !!\n",psy_name);
	}
	return iRet;
}


static long  ioctlDriver(struct file *filp, unsigned int command, unsigned long arg)
{
	unsigned long i = 0, temp;
	unsigned int p = arg;//*(unsigned int *)arg;
	unsigned long dwChk;
  //struct ebook_device_info info; 
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
	extern void mxc_epdc_fb_shutdown_proc(void);
#endif //]CONFIG_FB_MXC_EINK_PANEL
	char *psy_name=0;

	if(!Driver_Count){
		printk("%s : do not open\n",DEVICE_NAME);
		return -1;	
	}

	if(1==gptHWCFG->m_val.bPMIC) {
		// Ricoh PMIC .
		psy_name = rc5t619_bat_psy_name;
	}
	else if(3==gptHWCFG->m_val.bPMIC) {
		// Rohm PMIC .
		psy_name = bd7181x_bat_psy_name;
	}

	switch(command)
	{
		case POWER_OFF_COMMAND:
			{
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
				mxc_epdc_fb_shutdown_proc();
#endif //]CONFIG_FB_MXC_EINK_PANEL
				ntx_system_poweroff("POWER_OFF_COMMAND");
			}
			break;

		case SYS_RESET_COMMAND:
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
			mxc_epdc_fb_shutdown_proc();
#endif //]CONFIG_FB_MXC_EINK_PANEL
			ntx_system_reset("SYS_RESET_COMMAND");
			break;
			
		case SYS_AUTO_POWER_ON:
			printk("%s():SYS_AUTO_POWER_ON\n",__FUNCTION__);
			ntx_system_reset("SYS_AUTO_POWER_ON");
			break;
			
		case POWER_KEEP_COMMAND:
			printk("Kernel---System Keep Alive --- %d\n",p);
			gKeepPowerAlive=p;
			if(1==gptHWCFG->m_val.bPMIC||3==gptHWCFG->m_val.bPMIC)
				break;
			if (gKeepPowerAlive) {
#ifdef CONFIG_NTX_LED
				ntx_led_set_auto_blink(1);
#endif
			}
			break;
		

		case CM_GET_BATTERY_STATUS:
			{
				struct power_supply *psy = power_supply_get_by_name(psy_name);
				int iChk;
				if(psy) {
					union power_supply_propval psy_propval;
					iChk = power_supply_get_property(psy,POWER_SUPPLY_PROP_CAPACITY,&psy_propval);
					if(iChk>=0) {
						i = _Percent_to_NtxBattADC(psy_propval.intval);
						//printk(KERN_INFO"%s power supply capacity=%d\n",psy_name,psy_propval.intval);
					}
					else {
						i = _Percent_to_NtxBattADC(100);
						printk(KERN_ERR"%s power supply propval get failed !!\n",psy_name);
					}
				}
				else {
					i = _Percent_to_NtxBattADC(100);
					printk(KERN_ERR"%s power supply getting failed !!\n",psy_name);
				}
				dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			}
			break;
			
		case AC_IN:
			printk("%s() : AC_IN\n",__FUNCTION__);
			i = 0;
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;	

		case CM_SD_IN:
			i = ntx_esd_cd();
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case GET_LnBATT_CPU:
			break;
		case GET_VBATT_TH:
			break;
		case CM_AC_CK:
			break;
		case CM_USB_Plug_IN:
		case CM_CHARGE_STATUS:
			i = _Is_USB_plugged();
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
		case CM_PWR_ON2:
			break;
		case CM_AUDIO_PWR:
			break;
		case CM_nLED:
			//printk("CM_nLED %d\n",p);
#ifdef CONFIG_NTX_LED
			led_green(p?1:0);
#endif
			break;			
			
		case CM_nLED_CPU:
			break;			
			
		case CM_SD_PROTECT:
			i = 0;
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_CONTROLLER:
			i = 2;	// 2: Epson controller. for micro window
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_USB_AC_STATUS:
			//printk("%s() : CM_USB_AC_STATUS\n",__FUNCTION__);
			i = 0;
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_RTC_WAKEUP_FLAG:
	        i = g_wakeup_by_alarm;		// Joseph 091221 for slide show test.
	        g_wakeup_by_alarm = 0;
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
			
		case CM_SYSTEM_RESET:
#ifdef CONFIG_FB_MXC_EINK_PANEL//[
			mxc_epdc_fb_shutdown_proc();
#endif //]CONFIG_FB_MXC_EINK_PANEL
			ntx_system_reset("CM_SYSTEM_RESET");
			break;
			
		case CM_USB_HOST_PWR:
			printk("%s() : CM_USB_HOST_PWR\n",__FUNCTION__);
			break;
			
		case CM_BLUETOOTH_PWR:
			ntx_wifi_power_ctrl (p);
			break;
			
		case CM_TELLPID:
			printk("%s() : CM_TELLPID\n",__FUNCTION__);
			break;
			
		case CM_LED_BLINK:
#ifdef CONFIG_NTX_LED
			ntx_led_set_auto_blink(p);	
#endif
			break;
	      
		case CM_TOUCH_LOCK:
			printk("%s() : CM_TOUCH_LOCK\n",__FUNCTION__);
			break;  
      
		case CM_DEVICE_MODULE:
			i = check_hardware_name();
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
              	          	
		case CM_BLUETOOTH_RESET:
			printk("%s() : CM_BLUETOOTH_RESET\n",__FUNCTION__);
			break;
			
		case CM_DEVICE_INFO:
			printk("%s() : CM_DEVICE_INFO\n",__FUNCTION__);
			break;	
      		
		case CM_ROTARY_STATUS:
			//printk("%s() : CM_ROTARY_STATUS\n",__FUNCTION__);
   		break;	
      		
		case CM_ROTARY_ENABLE:	
			printk("%s() : CM_ROTARY_ENABLE\n",__FUNCTION__);
     	break;

		case CM_USB_ID_STATUS:
			printk("%s() : CM_USB_ID_STATUS\n",__FUNCTION__);
			i = 0;
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
     	break;

		case CM_GET_KEYS:
			printk("%s() : CM_GET_KEYS\n",__FUNCTION__);
			i = 0;
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
		case CM_POWER_BTN:
		case CM_GET_KEY_STATUS:	
			//printk("%s() : CM_POWER_BTN/CM_GET_KEY_STATUS\n",__FUNCTION__);
			//i=giPowerKeyState;	
			
			i = PowerKeyDeQueue();			
			if(i==-1)
			{
				i= giPowerKeyState;
			}					
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
     	break;	
      		
		case CM_GET_WHEEL_KEY_STATUS:
			i=0;
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
   		break;
      		
		case CM_GET_KL25_STATUS:
			i=0;	
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			break;
	
		case CM_GET_KL25_ACTION:
			i=0 ;	
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
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
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
    	break;	
	    	
		case CM_SET_ALARM_WAKEUP:
	    break;	
	    	
		case CM_GET_UP_VERSION:
			i = 0;
			dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
    	break;

		case CM_AUDIO_GET_VOLUME:
			break;
			
		case CM_AUDIO_SET_VOLUME:
			break;

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE //[
		case CM_FRONT_LIGHT_SET:
			{
#ifdef CONFIG_BACKLIGHT_LM3630A
				extern int ntx_set_fl_percent(int percent);

				ntx_set_fl_percent(p);
#endif
			}
			break;

		case CM_FL_LM3630_SET:
			break;
		case CM_FL_LM3630_TABLE:
			break;

		case CM_FL_HT68F20_GETDUTY:
			break;

		case CM_FL_HT68F20_SETDUTY:
			break;

		case CM_FRONT_LIGHT_AVAILABLE:
			{
			 	i = (unsigned long) (gptHWCFG->m_val.bFrontLight?1:0) ;
				dwChk = copy_to_user((void __user *)arg, &i, sizeof(unsigned long));
			}
			break;

		case CM_FL_MSP430_R_DUTY:
			break;

		case CM_FL_MSP430_W_DUTY:
			break;

		case CM_FL_MSP430_FREQUENCY:
			break;

		case CM_FRONT_LIGHT_R_EN:
			break;
		case CM_FL_MSP430_W_H_EN:
			break;
		case CM_FL_MSP430_PWM_EN:
			break;
#endif //]CONFIG_BACKLIGHT_CLASS_DEVICE 
		case CM_PLATFORM:
			printk("%s() : CM_PLATFORM\n",__FUNCTION__);
			break;
		case CM_HWCONFIG:
			printk("%s() : CM_HWCONFIG\n",__FUNCTION__);
			break;
		case CM_SET_HWCONFIG:
			if (!capable(CAP_SYS_ADMIN)) 
				return -EPERM;
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

extern void gpiokeys_report_event(unsigned int type, unsigned int code, int value);

void ntx_report_event(unsigned int type, unsigned int code, int value)
{
	gpiokeys_report_event(type,code,value);
}





static int __init initDriver(void)
{
	int ret;
        



#if 1
	pm_power_off = ntx_machine_poweroff;	// this is defined in bd7181x-power.c
	arm_pm_restart = ntx_machine_restart;	// use WDOG to reboot.
#endif

	ret = misc_register(&driverDevice);
	if (ret < 0) {
		printk("pvi_io: can't get major number\n");
		return ret;
	}

	return 0;
}
static void __exit exitDriver(void) {
	misc_deregister(&driverDevice);

}
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gallen");
MODULE_VERSION("2016-12-29");
MODULE_DESCRIPTION ("NTX_IO driver");
module_init(initDriver);
module_exit(exitDriver);


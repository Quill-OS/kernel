#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <mach/hardware.h>
#include <mach/gpio.h>


#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/input.h>


#include "gpiofn.h"
#include "ntx_hwconfig.h"

#define GDEBUG 0
#include <linux/gallen_dbg.h>

#ifdef CONFIG_MACH_MX6SL_NTX//[
	#define TLE4319_PLATFORM_MX6		1
#endif//]CONFIG_MACH_MX6SL_NTX


#ifdef TLE4319_PLATFORM_MX6//[
	#include <mach/iomux-mx6sl.h>
#else //][!TLE4319_PLATFORM_MX6
	#include <mach/iomux-mx50.h>
#endif //]TLE4319_PLATFORM_MX6

extern volatile NTX_HWCONFIG *gptHWCFG;
extern void ntx_report_key(int isDown,__u16 wKeyCode);
extern void ntx_report_event(unsigned int type, unsigned int code, int value);
extern int gIsCustomerUi;
extern void power_key_int_function(void);
extern void ntx_report_power(int isDown);

static int tle4913Q_func(int iGPIOVal,unsigned uGPIO);



#ifdef TLE4319_PLATFORM_MX6//[

static GPIODATA gtTLE4913_GPIO_data = {
	.pfnGPIO_INT = tle4913Q_func,
//	.uGPIO = GPIO_tle4913Q,
	.szName = "TLE4913",
//	.tPADCtrl = MX6SL_PAD_FEC_MDC__GPIO_4_23,
	.uiIRQType = IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
	.iWakeup = 1,
	.dwDebounceTicks = 50,
	.iActive = 0 ,
};

#else //][!TLE4319_PLATFORM_MX6

#define GPIO_tle4913Q 	(4*32+25) /* GPIO_5_25 */
static GPIODATA gtTLE4913_GPIO_data = {
	.pfnGPIO_INT = tle4913Q_func,
	.uGPIO = GPIO_tle4913Q,
	.szName = "TLE4913",
	.tPADCtrl = MX50_PAD_SD3_D5__GPIO_5_25,
	.uiIRQType = IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
	.iWakeup = 1,
	.dwDebounceTicks = 50,
	.iActive = 0 ,
};

#endif //]TLE4319_PLATFORM_MX6




static int tle4913Q_func(int iGPIOVal,unsigned uGPIO)
{
	DBG0_MSG("%s(%d): gpio%u,val=%d\n",__FILE__,__LINE__,uGPIO,iGPIOVal);
	
	if(gIsCustomerUi) {
		if(2==gptHWCFG->m_val.bUIStyle) {
			// ANDROID UI .
			ntx_report_event(EV_SW,SW_LID,iGPIOVal?0:1);
		}
		else {
			// K UI .
			ntx_report_key(iGPIOVal?0:1,KEY_F1);
		}
	}
	else {
		// N UI .
		ntx_report_key(iGPIOVal?0:1,KEY_H);
	}
	
	return 0;
}



void tle4913_init(void)
{
	if(gptHWCFG&&1==gptHWCFG->m_val.bHallSensor) {
		if(31==gptHWCFG->m_val.bPCB||32==gptHWCFG->m_val.bPCB) {
			// E60Q0X & E60Q1X.
			gtTLE4913_GPIO_data.uGPIO = IMX_GPIO_NR(4,23) ;
			gtTLE4913_GPIO_data.tPADCtrl = MX6SL_PAD_FEC_MDC__GPIO_4_23 ;
		}
		else {
			gtTLE4913_GPIO_data.uGPIO = IMX_GPIO_NR(5,12) ;
			gtTLE4913_GPIO_data.tPADCtrl = MX6SL_PAD_SD1_DAT4__GPIO_5_12 ;
		}

		gpiofn_register(&gtTLE4913_GPIO_data);
	}
	else {
		WARNING_MSG("%s : HallSensor not exist !\n",__FUNCTION__);
	}
}

void tle4913_release(void)
{
	if(gptHWCFG&&1==gptHWCFG->m_val.bHallSensor) {
		gpiofn_unregister(&gtTLE4913_GPIO_data);
	}
}


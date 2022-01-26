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



#include <mach/iomux-mx6sl.h>

extern volatile NTX_HWCONFIG *gptHWCFG;
extern void ntx_report_key(int isDown,__u16 wKeyCode);
extern void ntx_report_event(unsigned int type, unsigned int code, int value);
extern int gIsCustomerUi;
extern void power_key_int_function(void);
extern void ntx_report_power(int isDown);

static int headphone_detect_sw_func(int iGPIOVal,unsigned uGPIO);




static GPIODATA gtHPDETECTSW_GPIO_data = {
	.pfnGPIO_INT = headphone_detect_sw_func,
//	.uGPIO = ,
	.szName = "HPDETECT_SW",
//	.tPADCtrl = ,
	.uiIRQType = IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
	.iWakeup = 1,
	.dwDebounceTicks = 2,
	.iActive = 0 ,
};




static int headphone_detect_sw_func(int iGPIOVal,unsigned uGPIO)
{
	DBG0_MSG("%s(%d): gpio%u,val=%d\n",__FILE__,__LINE__,uGPIO,iGPIOVal);
	
	ntx_report_event(EV_SW,SW_HEADPHONE_INSERT,iGPIOVal?0:1);
	
	return 0;
}



void headphone_detect_init(void)
{
	//if( gptHWCFG && NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,6) ) 
	{
		// PCB flag 'HP_DETECT' enabled .
		gtHPDETECTSW_GPIO_data.uGPIO = IMX_GPIO_NR(3,24) ;
		gtHPDETECTSW_GPIO_data.tPADCtrl = MX6SL_PAD_KEY_COL0__GPIO_3_24_FLOAT ;

		gpiofn_register(&gtHPDETECTSW_GPIO_data);
	}
	//else {
	//	WARNING_MSG("%s : headphone detector not exist !\n",__FUNCTION__);
	//}
}

void headphone_detect_release(void)
{
	//if( gptHWCFG && NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,6) ) 
	{
		// PCB flag 'HP_DETECT' enabled .
		gpiofn_unregister(&gtHPDETECTSW_GPIO_data);
	}
}


#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/iomux-mx50.h>

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



extern volatile NTX_HWCONFIG *gptHWCFG;
extern void ntx_report_key(int isDown,__u16 wKeyCode);
extern int gIsCustomerUi;
extern void power_key_int_function(void);
extern void ntx_report_power(int isDown);

#define GPIO_tle4913Q 	(4*32+25) /* GPIO_5_25 */
static int tle4913Q_func(int iGPIOVal,unsigned uGPIO);



static GPIODATA gtTLE4913_GPIO_data = {
	.pfnGPIO_INT = tle4913Q_func,
	.uGPIO = GPIO_tle4913Q,
	.szName = "TLE4913",
	.tPADCtrl = MX50_PAD_SD3_D5__GPIO_5_25,
	.uiIRQType = IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
	.iWakeup = 1,
	.dwDebounceTicks = 1,
	.iActive = 0 ,
};




static int tle4913Q_func(int iGPIOVal,unsigned uGPIO)
{
	DBG0_MSG("%s(%d): gpio%u,val=%d\n",__FILE__,__LINE__,uGPIO,iGPIOVal);
	
	if(gIsCustomerUi) {
		ntx_report_key(iGPIOVal?0:1,KEY_F1);
	}
	else {
		ntx_report_key(iGPIOVal?0:1,KEY_H);
	}
	
	return 0;
}



void tle4913_init(void)
{
	if(gptHWCFG&&1==gptHWCFG->m_val.bHallSensor) {
		switch (gptHWCFG->m_val.bPCB) {
		case 35 :// E606F0B 
			gtTLE4913_GPIO_data.tPADCtrl = MX50_PAD_SD2_D7__GPIO_5_15;
			gtTLE4913_GPIO_data.uGPIO = (4*32+15); /* GPIO_5_15 */
			break;
		case 41 :// E606G0
			gtTLE4913_GPIO_data.tPADCtrl = MX50_PAD_SD2_D5__GPIO_5_13;
			gtTLE4913_GPIO_data.uGPIO = (4*32+13); /* GPIO_5_15 */
			break;
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


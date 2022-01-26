
#ifndef __GPIOFN_H //[
#define __GPIOFN_H


#ifdef CONFIG_MACH_MX6SL_NTX//[
	#define GPIOFN_PLATFORM_MX6		1
#endif//]CONFIG_MACH_MX6SL_NTX


#ifdef GPIOFN_PLATFORM_MX6//[
	#include <mach/iomux-mx6sl.h>
#else //][!GPIOFN_PLATFORM_MX6
	#include <mach/iomux-mx50.h>
#endif //]GPIOFN_PLATFORM_MX6


#include <linux/timer.h>
#include <linux/list.h>

typedef int (*fn_gpio)(int I_iGPIO_val,unsigned uGPIO);

typedef struct tagGPIODATA{
	//-------------------------------
	// public :
	fn_gpio pfnGPIO;
	fn_gpio pfnGPIO_INT;
	unsigned uGPIO;
	char *szName;
	iomux_v3_cfg_t tPADCtrl;
	unsigned int uiIRQType;
	int iWakeup; // 1->enable wakeup device , 0-> disable wakeup device .
	unsigned long dwDebounceTicks;
	int iActive; 
	//--------------------------------
	// private :
	struct work_struct tWork;
	struct workqueue_struct *ptWQ;
	struct timer_list tTimerDebounce;
	unsigned long dwCurrentDebounceTicks;
	int iLastGPIOVal;
	int iCurrentGPIOVal;
	struct list_head list;
}GPIODATA;


int gpiofn_init(void);
int gpiofn_release(void);

int gpiofn_suspend(void);
void gpiofn_resume(void);
void gpiofn_rechk(void);

int gpiofn_register(GPIODATA *I_ptGPIO_Data);
int gpiofn_unregister(GPIODATA *I_ptGPIO_Data);


#endif //] __GPIOFN_H

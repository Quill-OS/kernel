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
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/list.h>


#define GDEBUG 0
#include <linux/gallen_dbg.h>

#include "gpiofn.h"

#ifdef GPIOFN_PLATFORM_MX6//[
	#define set_irq_type(_irq,_irqtype)		irq_set_irq_type((_irq),(_irqtype))
#endif //]GPIOFN_PLATFORM_MX6


static const char gszModuleName[]="GPIOFN";

//static struct work_struct gtGPIOFN_Work;
static struct workqueue_struct *gptGPIOFN_WQ=0;
static int gIsGPIOFN_inited=0;
static LIST_HEAD( gpio_data_list );

static void gpiofn_wq_func(struct work_struct *work)
{
	//struct list_head *ptr;
	GPIODATA *ptGPIO_Data;
	int iGPIOVal;
	
	GALLEN_DBGLOCAL_BEGIN();
	
	ptGPIO_Data = container_of(work,GPIODATA,tWork);
	ASSERT(ptGPIO_Data);
	
	//list_for_each(ptr,&gpio_data_list) 
	{
		GALLEN_DBGLOCAL_RUNLOG(0);
		//ptGPIO_Data = list_entry(ptr,GPIODATA,list);
		//if ( &ptGPIO_Data->tWork == work) 
		{
			GALLEN_DBGLOCAL_RUNLOG(1);
			// 
			if(ptGPIO_Data->pfnGPIO) {
				DBG_MSG("%s(%d):calling \"%s\" gpio function\n",
					__FILE__,__LINE__,ptGPIO_Data->szName);
				
				GALLEN_DBGLOCAL_RUNLOG(2);
				//iGPIOVal = gpio_get_value(ptGPIO_Data->uGPIO);
				iGPIOVal = ptGPIO_Data->iLastGPIOVal;
				ptGPIO_Data->pfnGPIO(iGPIOVal,ptGPIO_Data->uGPIO);
				
				if(iGPIOVal==ptGPIO_Data->iActive) {
					mod_timer(&ptGPIO_Data->tTimerDebounce, \
						jiffies+ptGPIO_Data->dwDebounceTicks);
				}
			}
			
			//break;
		}
	}
	
	GALLEN_DBGLOCAL_END();
}


static void gpio_int_checker(unsigned long I_dwParam)
{
	GPIODATA *ptGPIO_Data = (GPIODATA *)I_dwParam;
	int iGPIOVal;
	
	iGPIOVal = gpio_get_value(ptGPIO_Data->uGPIO);
#if 0
	printk("%s(),\"%s\" gpio=%d,%u,%u\n",__FUNCTION__,
			ptGPIO_Data->szName,iGPIOVal,
			ptGPIO_Data->dwCurrentDebounceTicks,
			ptGPIO_Data->dwDebounceTicks);
#endif

	GALLEN_DBGLOCAL_BEGIN();
#if 0 //[
	if(ptGPIO_Data->dwDebounceTicks==ptGPIO_Data->dwCurrentDebounceTicks)
	{
		GALLEN_DBGLOCAL_RUNLOG(0);
		// 
		if(ptGPIO_Data->pfnGPIO_INT) {
			GALLEN_DBGLOCAL_RUNLOG(2);
			ptGPIO_Data->pfnGPIO_INT(iGPIOVal,ptGPIO_Data->uGPIO);
		}
		else {
			if(ptGPIO_Data->pfnGPIO) {
				GALLEN_DBGLOCAL_RUNLOG(3);
				if ( gptGPIOFN_WQ ) {
					queue_work(gptGPIOFN_WQ,&ptGPIO_Data->tWork);
				}
				else {
					WARNING_MSG("%s() fail ! you should call gpiofn_init() once !\n",__FUNCTION__);
				}
			}
		}
		ptGPIO_Data->iCurrentGPIOVal = iGPIOVal;
		ptGPIO_Data->dwCurrentDebounceTicks = 0;
	}
	else 
#endif //]
	if(0==ptGPIO_Data->dwCurrentDebounceTicks)
	{
		GALLEN_DBGLOCAL_RUNLOG(4);
		//
		del_timer_sync(&ptGPIO_Data->tTimerDebounce);
		
		ptGPIO_Data->dwCurrentDebounceTicks = ptGPIO_Data->dwDebounceTicks;
		if(iGPIOVal!=ptGPIO_Data->iLastGPIOVal) {
			GALLEN_DBGLOCAL_RUNLOG(5);
			if (ptGPIO_Data->pfnGPIO_INT) {
				GALLEN_DBGLOCAL_RUNLOG(6);
				ptGPIO_Data->pfnGPIO_INT(iGPIOVal,ptGPIO_Data->uGPIO);
			}
			else {
				if(ptGPIO_Data->pfnGPIO) {
					GALLEN_DBGLOCAL_RUNLOG(7);
					ptGPIO_Data->iCurrentGPIOVal = iGPIOVal;
					if(gptGPIOFN_WQ) {
						queue_work(gptGPIOFN_WQ,&ptGPIO_Data->tWork);
					}
					else {
						WARNING_MSG("%s() fail ! you should call gpiofn_init() once !\n",__FUNCTION__);
					}
				}
			}
		}
		else {
			GALLEN_DBGLOCAL_RUNLOG(8);
			mod_timer(&ptGPIO_Data->tTimerDebounce,jiffies+1);
		}
	}
	else {
		GALLEN_DBGLOCAL_RUNLOG(9);
		
		--ptGPIO_Data->dwCurrentDebounceTicks;
		mod_timer(&ptGPIO_Data->tTimerDebounce,jiffies+1);
	}
	GALLEN_DBGLOCAL_END();
}




static irqreturn_t gpiofn_inthandler(int irq, void *I_pvParam)
{
	GPIODATA *ptGPIO_Data = (GPIODATA *)I_pvParam;
	
	DBG_MSG("[%s-%d] IRQ%d,\"%s\" nterrupt triggered !\n",
		__func__,__LINE__,irq,ptGPIO_Data->szName);
	
	//if(ptGPIO_Data->dwDebounceTicks) {
		//if(ptGPIO_Data->iActive == gpio_get_value(ptGPIO_Data->uGPIO)) {
			gpio_int_checker((unsigned long) ptGPIO_Data);
		//}
	//}
	//else {
		//gpio_int_checker((unsigned long) I_pvParam);
	//}
	
	return 0;
}


int gpiofn_register(GPIODATA *I_ptGPIO_Data)
{
	int iChk;
	int irq;
	
	GALLEN_DBGLOCAL_BEGIN();
	
	if(!I_ptGPIO_Data) {
		ERR_MSG("%s(%d) : parameter error !\n",
			__FILE__,__LINE__);
		GALLEN_DBGLOCAL_ESC();
		return -1;
	}
	
	if((!I_ptGPIO_Data->pfnGPIO)&&(!I_ptGPIO_Data->pfnGPIO_INT)) {
		ERR_MSG("%s(%d) : parameter error ! no gpio callback !\n",
			__FILE__,__LINE__);
		GALLEN_DBGLOCAL_ESC();
		return -1;
	}
		
	iChk = gpio_request(I_ptGPIO_Data->uGPIO, I_ptGPIO_Data->szName);
	if(0!=iChk) {
		WARNING_MSG("%s(%d) :[warning] request gpio%d for \"%s\" fail !\n",
			__FILE__,__LINE__,I_ptGPIO_Data->uGPIO,I_ptGPIO_Data->szName);
		//return -1;
	}
	else {
		mxc_iomux_v3_setup_pad(I_ptGPIO_Data->tPADCtrl);
	}
	
	gpio_direction_input(I_ptGPIO_Data->uGPIO);
	I_ptGPIO_Data->iLastGPIOVal = -1;
	I_ptGPIO_Data->iCurrentGPIOVal = -1;
	I_ptGPIO_Data->dwCurrentDebounceTicks = I_ptGPIO_Data->dwDebounceTicks;
	GALLEN_DBGLOCAL_PRINTLOG();
	init_timer(&I_ptGPIO_Data->tTimerDebounce);
	I_ptGPIO_Data->tTimerDebounce.data = (unsigned long)I_ptGPIO_Data;
	I_ptGPIO_Data->tTimerDebounce.function = gpio_int_checker;
	GALLEN_DBGLOCAL_PRINTLOG();
	INIT_LIST_HEAD(&I_ptGPIO_Data->list);
	list_add(&I_ptGPIO_Data->list,&gpio_data_list);
#if 0
	{
		struct list_head *ptr;
		GPIODATA *ptGPIO_Data;
		
		DBG_MSG("%s(),adding gpiofn member \n",__FUNCTION__);
		list_for_each(ptr,&gpio_data_list)
		{
			ptGPIO_Data = list_entry(ptr,GPIODATA,list);
			DBG_MSG("%s(),gpio \"%s\" added\n",__FUNCTION__,ptGPIO_Data->szName);
		}
	}
#endif


	GALLEN_DBGLOCAL_PRINTLOG();
	
	/*
	I_ptGPIO_Data->ptWQ = create_rt_workqueue(I_ptGPIO_Data->szName);
	if(!I_ptGPIO_Data->ptWQ) {
		ERR_MSG("%s(%d) : %s workqueue creating fail !\n",
			__FILE__,__LINE__,I_ptGPIO_Data->szName);
		GALLEN_DBGLOCAL_ESC();
		goto err_exit_release_gpio;
	}
	*/
	//INIT_WORK(&I_ptGPIO_Data->tWork, I_ptGPIO_Data->pfnGPIO);
	
	INIT_WORK(&I_ptGPIO_Data->tWork, gpiofn_wq_func);
	
	irq = gpio_to_irq(I_ptGPIO_Data->uGPIO);
	set_irq_type(irq, I_ptGPIO_Data->uiIRQType);
	iChk = request_irq(irq, gpiofn_inthandler, 0, I_ptGPIO_Data->szName, I_ptGPIO_Data);
	if (iChk) {
		ERR_MSG("register %s interrupt fail !\n",I_ptGPIO_Data->szName);
		GALLEN_DBGLOCAL_ESC();
		goto err_exit_release_wq;
	}
	else {
		if(1==I_ptGPIO_Data->iWakeup) {
			enable_irq_wake(irq);
		}
		else if(0==I_ptGPIO_Data->iWakeup){
			disable_irq_wake(irq);
		}
	}
	
	
	GALLEN_DBGLOCAL_END();
	return 0;
	
err_exit_release_wq:
	if(I_ptGPIO_Data->ptWQ) {
		destroy_workqueue(I_ptGPIO_Data->ptWQ);
	}

	list_del(&I_ptGPIO_Data->list);
	
err_exit_release_gpio:
	gpio_free(I_ptGPIO_Data->uGPIO);
	
	return -1;
}


int gpiofn_unregister(GPIODATA *I_ptGPIO_Data)
{
	int iRet;
	
	if(I_ptGPIO_Data) {
		int irq;
		// release interrupt ...
		irq = gpio_to_irq(I_ptGPIO_Data->uGPIO);
		free_irq(irq,0);
		
		if(I_ptGPIO_Data->ptWQ) {
			flush_workqueue(I_ptGPIO_Data->ptWQ);
			destroy_workqueue(I_ptGPIO_Data->ptWQ);
		}
		
		gpio_free(I_ptGPIO_Data->uGPIO);
		list_del(&I_ptGPIO_Data->list);
	}
	else {
		iRet = -1;
	}
	
	return iRet;
}


static int __rechk_gpio_fn(void)
{
	
	if(gIsGPIOFN_inited) {
		struct list_head *ptr;
		GPIODATA *ptGPIO_Data;

		DBG_MSG("%s()\n",__FUNCTION__);
		list_for_each(ptr,&gpio_data_list)
		{
			ptGPIO_Data = list_entry(ptr,GPIODATA,list);
			DBG_MSG("%s(),recheck gpio \"%s\"\n",__FUNCTION__,ptGPIO_Data->szName);
			gpio_int_checker((unsigned long)ptGPIO_Data);
		}
		return 0;
	}
	else {
		return -1;
	}
}

void gpiofn_rechk(void)
{
	__rechk_gpio_fn();
}

int gpiofn_suspend(void)
{
	return 0;
}

void gpiofn_resume(void)
{
	__rechk_gpio_fn();
}

int gpiofn_init(void)
{
	if(gIsGPIOFN_inited>0) {
		WARNING_MSG("[WARNING] %s() >1 (%d) !\n",__FUNCTION__,gIsGPIOFN_inited);
		return -2;
	}

#ifdef GPIOFN_PLATFORM_MX6//[
	gptGPIOFN_WQ = create_singlethread_workqueue(gszModuleName);
#else //][!GPIOFN_PLATFORM_MX6
	gptGPIOFN_WQ = create_rt_workqueue(gszModuleName);
#endif //] GPIOFN_PLATFORM_MX6

	if(!gptGPIOFN_WQ) {
		ERR_MSG("%s(%d) : %s workqueue creating fail !\n",
			__FILE__,__LINE__,gszModuleName);
		return -1;
	}
	
	INIT_LIST_HEAD(&gpio_data_list);

	++gIsGPIOFN_inited;
	//INIT_WORK(&gtGPIOFN_Work, gpiofn_wq_func);
	return 0;
}

int gpiofn_release(void)
{
	if(gIsGPIOFN_inited<=0) {
		WARNING_MSG("[WARNING] %s() <0 (%d)!\n",__FUNCTION__,gIsGPIOFN_inited);
		return -1;
	}

	if(gptGPIOFN_WQ) {
		flush_workqueue(gptGPIOFN_WQ);
		destroy_workqueue(gptGPIOFN_WQ);
	}
	--gIsGPIOFN_inited;
	return 0;
}

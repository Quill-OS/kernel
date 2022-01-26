

/*
 *
 * Purpose : FP9928 driver
 * Author : Gallen Lin
 * versions :
 *
 */ 

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/interrupt.h>


#include <mach/hardware.h>
#include <mach/gpio.h>

#include <mach/iomux-mx6sl.h>


#include "ntx_hwconfig.h"
#include "fake_s1d13522.h"

#define GDEBUG	0
#include <linux/gallen_dbg.h>

#include "lk_fp9928.h"


#define DRIVER_NAME "FP9928"

#define INIT_POWER_STATE	0

#define USE_EPD_PWRSTAT		1
/*
 * RAILPWR_SCHED_WAIT = 0 , use busy wait loop . 
 * RAILPWR_SCHED_WAIT = 1 , use wait_event macro provided by linux kernel (NG)
 * RAILPWR_SCHED_WAIT = 2 , use loop with schedule_timeout(1) .
 */
#define RAILPWR_SCHED_WAIT	0


#define GPIO_FP9928_VIN_PADCFG		MX6SL_PAD_EPDC_PWRWAKEUP__GPIO_2_14
#define GPIO_FP9928_VIN					IMX_GPIO_NR(2,14)

#if 1
//
#define GPIO_FP9928_EN_PADCFG			MX6SL_PAD_EPDC_PWRCTRL1__GPIO_2_8	
#define GPIO_FP9928_EN					IMX_GPIO_NR(2,8)
#define GPIO_FP9928_VCOM_PADCFG			MX6SL_PAD_EPDC_VCOM0__GPIO_2_3	
#define GPIO_FP9928_VCOM				IMX_GPIO_NR(2,3)
#define GPIO_FP9928_EP_3V3_IN								IMX_GPIO_NR(4,3) // EPDC_PWRWAKEUP
#define GPIO_FP9928_EP_3V3_IN_PADCFG				MX6SL_PAD_KEY_ROW5__GPIO_4_3

#define GPIO_FP9928_PWRGOOD							IMX_GPIO_NR(2,13) // EPDC_PWRSTAT
//#define GPIO_FP9928_PWRGOOD_INT_PADCFG			MX6SL_PAD_EPDC_PWRSTAT__GPIO_2_13_PUINT
//#define GPIO_FP9928_PWRGOOD_GPIO_PADCFG			MX6SL_PAD_EPDC_PWRSTAT__GPIO_2_13
#define GPIO_FP9928_PWRGOOD_GPIO_PADCFG		MX6SL_PAD_EPDC_PWRSTAT__GPIO_2_13_FLOATINPUT	

#else
// beta version .
#define GPIO_FP9928_EN_PADCFG			MX6SL_PAD_EPDC_VCOM0__GPIO_2_3	
#define GPIO_FP9928_EN					IMX_GPIO_NR(2,3)
#define GPIO_FP9928_VCOM_PADCFG			MX6SL_PAD_EPDC_PWRCTRL1__GPIO_2_8	
#define GPIO_FP9928_VCOM				IMX_GPIO_NR(2,8)
#endif

#define VIN_ON		1
#define VIN_OFF		0
#define EN_ON			1
#define EN_OFF		0
#define VCOM_ON		1
#define VCOM_OFF	0


//#define FP9928_EP3V3OFF_TICKS_MAX			350
#define FP9928_POWEROFF_TICKS_MAX			0 //
#define FP9928_POWERON_WAIT_TICKS			2 //等待FP9928 POWERON->READY的時間.
#define FP9928_PWROFFDELAYWORK_TICKS	50


#if 0
#define FP9928_VCOM_MV_MAX		(-302)
#define FP9928_VCOM_MV_MIN		(-2501)
//#define FP9928_VCOM_MV_STEP		(11)
#define FP9928_VCOM_UV_STEP		(11000)
#else
#define FP9928_VCOM_MV_MAX		(-302)
#define FP9928_VCOM_MV_MIN		(-6000)
//#define FP9928_VCOM_MV_STEP		(22)
#define FP9928_VCOM_UV_STEP		(21569)
#endif

#define FP9928_WAIT_TICKSTAMP(_TickEnd,_wait_item)	\
{\
	unsigned long dwTickNow=jiffies,dwTicks;\
	int iInInterrupt=in_interrupt();\
	if ( time_before(dwTickNow,_TickEnd) ) {\
		dwTicks = _TickEnd-dwTickNow;\
		DBG_MSG("%s() waiting to %ld ticks for %s ... ",__FUNCTION__,dwTicks,_wait_item);\
		if(iInInterrupt) {\
			mdelay(jiffies_to_msecs(dwTicks));\
			DBG_MSG("done(@INT)\n");\
		}\
		else {\
			msleep(jiffies_to_msecs(dwTicks));\
			DBG_MSG("done\n");\
		}\
	}\
}


typedef struct tagFP9928_PWRDWN_WORK_PARAM {
	struct delayed_work pwrdwn_work;
	int iIsTurnOffChipPwr;
} FP9928_PWRDWN_WORK_PARAM;



typedef struct tagFP9228_data {
	int iCurrent_temprature;
	unsigned short wTempratureData,wReserved;
	struct i2c_adapter *ptI2C_adapter; 
	struct i2c_client *ptI2C_client;
	struct mutex tI2CLock;
	struct mutex tPwrLock;
	int iIsPoweredON;
	int iIsOutputEnabled;
	int iIsOutputPowerDownCounting;
	int iIsVCOMNeedReInit;
	int iCurrent_VCOM_mV;
	unsigned long dwTickPowerOffEnd;
	unsigned long dwTickPowerOnEnd;
	unsigned long dwTickEP3V3OffEnd;
	FP9928_PWRDWN_WORK_PARAM tPwrdwn_work_param;
	int iIsEP3V3_SW_enabled;
	int iChipPwrWaitON_ms; // the times to wait FP9928 ON after turnning it ON .
	int iVEEStableWait_ms; // the times to wait VEE discharged .
#ifdef USE_EPD_PWRSTAT //[
	wait_queue_head_t tWQ_RailPower;
#else //][!USE_EPD_PWRSTAT
	int iRailPwrWaitON_ms; // the times to wait rail power stable after enabling EN pin .
	unsigned long dwTickRailPowerOnEnd;
#endif //] USE_EPD_PWRSTAT

} FP9928_data;


static FP9928_data *gptFP9928_data ;

// externals ...
extern volatile NTX_HWCONFIG *gptHWCFG;
extern volatile int gSleep_Mode_Suspend;


static struct i2c_board_info gtFP9928_BI = {
 .type = "FP9928",
 .addr = 0x48,
 .platform_data = NULL,
};

static const unsigned short gwFP9928_AddrA[] = {
	0x48,
	I2C_CLIENT_END
};


static volatile unsigned char gbFP9928_REG_TMST_addr=0x00;
static volatile unsigned char gbFP9928_REG_TMST=0;

#define FP9928_REG_FUNC_ADJUST_VCOM_ADJ			0x01
#define FP9928_REG_FUNC_ADJUST_FIX_RD_PTR		0x02
static volatile unsigned char gbFP9928_REG_FUNC_ADJUST_addr=0x01;
static volatile unsigned char gbFP9928_REG_FUNC_ADJUST=0x01;

#define FP9928_REG_VCOM_SETTING_ALL					0xff
static volatile unsigned char gbFP9928_REG_VCOM_SETTING_addr=0x02;
static volatile unsigned char gbFP9928_REG_VCOM_SETTING=0x74;


static int _fp9928_set_reg(unsigned char bRegAddr,unsigned char bRegSetVal)
{
	int iRet=FP9928_RET_SUCCESS;
	int iChk;
	unsigned char bA[2] ;
	int iIn_Interrupt = in_interrupt();
	unsigned long dwTickNow = jiffies,dwTicks;

	ASSERT(gptFP9928_data);

	if(!iIn_Interrupt) {
		mutex_lock(&gptFP9928_data->tI2CLock);
	}

	FP9928_WAIT_TICKSTAMP(gptFP9928_data->dwTickPowerOnEnd,"power on stable");

	bA[0]=bRegAddr;
	bA[1]=bRegSetVal;
	iChk = i2c_master_send(gptFP9928_data->ptI2C_client, (const char *)bA, sizeof(bA));
	if (iChk < 0) {
		ERR_MSG("%s(%d):%d=%s(),regAddr=0x%x,regVal=0x%x fail !\n",__FILE__,__LINE__,\
			iChk,"i2c_master_send",bRegAddr,bRegSetVal);
		iRet=FP9928_RET_I2CTRANS_ERR;
	}
	
	if(!iIn_Interrupt) {
		mutex_unlock(&gptFP9928_data->tI2CLock);
	}

	return iRet;
}

static int _fp9928_get_reg(unsigned char bRegAddr,unsigned char *O_pbRegVal)
{
	int iRet=FP9928_RET_SUCCESS;
	int iChk;
	unsigned char bA[1] ;
	int iIn_Interrupt = in_interrupt();
	unsigned long dwTickNow = jiffies,dwTicks;

	ASSERT(gptFP9928_data);

	if(!iIn_Interrupt) {
		mutex_lock(&gptFP9928_data->tI2CLock);
	}

	FP9928_WAIT_TICKSTAMP(gptFP9928_data->dwTickPowerOnEnd,"power on stable");

	bA[0]=bRegAddr;
	iChk = i2c_master_send(gptFP9928_data->ptI2C_client, (const char *)bA, 1);
	if (iChk < 0) {
		ERR_MSG("%s(%d):%s i2c_master_send fail !\n",__FILE__,__LINE__,__FUNCTION__);
		iRet = FP9928_RET_I2CTRANS_ERR;
	}
	

	iChk = i2c_master_recv(gptFP9928_data->ptI2C_client, bA, 1);
	if (iChk < 0) {
		ERR_MSG("%s(%d):%s i2c_master_recv fail !\n",__FILE__,__LINE__,__FUNCTION__);
		iRet = FP9928_RET_I2CTRANS_ERR;
	}


	if(iRet>=0) {
		*O_pbRegVal = bA[0];
	}

	
	//DBG_MSG("%s(0x%x,%p)==>0x%x\n",__FUNCTION__,bRegAddr,O_pbRegVal,bA[0]);

	if(!iIn_Interrupt) {
		mutex_unlock(&gptFP9928_data->tI2CLock);
	}

	return iRet;
}


#define FP9928_REG_SET(_regName,_bFieldName,_bSetVal)		\
({\
	int _iRet=FP9928_RET_SUCCESS;\
	int _iChk;\
	unsigned char _bNewReg,_bFieldMask;\
	\
	_bFieldMask=(unsigned char)FP9928_REG_##_regName##_##_bFieldName;\
	if(0xff==_bFieldMask) {\
		_bNewReg = _bSetVal;\
	}\
	else {\
		_bNewReg=gbFP9928_REG_##_regName;\
		if(_bSetVal) {\
			_bNewReg |= _bFieldMask ;\
		}\
		else {\
			_bNewReg &= ~_bFieldMask;\
		}\
	}\
	\
	_iChk = _fp9928_set_reg(gbFP9928_REG_##_regName##_##addr,_bNewReg);\
	if(_iChk<0) {\
		_iRet = _iChk;\
	}\
	else {\
		DBG_MSG("%s() : FP9928 write reg%s(%02Xh) 0x%02x->0x%02x\n",__FUNCTION__,\
		#_regName,gbFP9928_REG_##_regName##_##addr,gbFP9928_REG_##_regName,_bNewReg);\
		gbFP9928_REG_##_regName = _bNewReg;\
	}\
	_iRet;\
})

#define FP9928_REG_GET(_regName)		\
({\
	int _iChk;\
	unsigned char bReadReg=0;\
	unsigned short _wRet=0;\
	\
	_iChk = _fp9928_get_reg(gbFP9928_REG_##_regName##_##addr,&bReadReg);\
	if(_iChk<0) {\
		_wRet = (unsigned short)(-1);\
	}\
	else {\
		_wRet = bReadReg;\
		gbFP9928_REG_##_regName = bReadReg;\
		DBG_MSG("%s() : FP9928 read reg%s(%02Xh)=0x%02x\n",__FUNCTION__,\
			#_regName,gbFP9928_REG_##_regName##_##addr,bReadReg);\
	}\
	_wRet;\
})

#define FP9928_REG(_regName)	gbFP9928_REG_##_regName






static int _fp9928_gpio_init(void)
{
	int iRet = FP9928_RET_SUCCESS;
	int iVINState;

	GALLEN_DBGLOCAL_BEGIN();

#ifdef USE_EPD_PWRSTAT //[
	mxc_iomux_v3_setup_pad(GPIO_FP9928_PWRGOOD_GPIO_PADCFG);
	gpio_request(GPIO_FP9928_PWRGOOD, "FP9928_PWRGOOD");
	gpio_direction_input(GPIO_FP9928_PWRGOOD);
#endif //] USE_EPD_PWRSTAT

	mxc_iomux_v3_setup_pad(GPIO_FP9928_VIN_PADCFG);
	if(0!=gpio_request(GPIO_FP9928_VIN, "fp9928_VIN")) {
		WARNING_MSG("%s(),request gpio fp9928_VIN fail !!\n",__FUNCTION__);
		//gpio_direction_input(GPIO_FP9928_VIN);
	}

	iVINState = gpio_get_value(GPIO_FP9928_VIN);
	//printk("%s():FP9928 VIN=%d\n",__FUNCTION__,iVINState);
	gptFP9928_data->iIsPoweredON=(VIN_ON==iVINState)?1:0;

	mxc_iomux_v3_setup_pad(GPIO_FP9928_EN_PADCFG);
	if(0!=gpio_request(GPIO_FP9928_EN, "fp9928_EN")) {
		WARNING_MSG("%s(),request gpio fp9928_EN fail !!\n",__FUNCTION__);
		//gpio_direction_input(GPIO_FP9928_EN);
	}
	gpio_direction_output(GPIO_FP9928_EN,EN_OFF);
	gptFP9928_data->iIsOutputEnabled = 0;
	

	mxc_iomux_v3_setup_pad(GPIO_FP9928_VCOM_PADCFG);
	gpio_request(GPIO_FP9928_VCOM, "fp9928_VCOM");
	if(0!=gpio_request(GPIO_FP9928_VCOM, "fp9928_VCOM")) {
		WARNING_MSG("%s(),request gpio fp9928_VCOM fail !!\n",__FUNCTION__);
		//gpio_direction_input(GPIO_FP9928_VCOM);
	}
	gpio_direction_output(GPIO_FP9928_VCOM,VCOM_OFF);

#if defined(GPIO_FP9928_EP_3V3_IN) && (GPIO_FP9928_EP_3V3_IN!=GPIO_FP9928_VIN) //[
	if(gptFP9928_data->iIsEP3V3_SW_enabled) {
		// E60Q3X revA .
		mxc_iomux_v3_setup_pad(GPIO_FP9928_EP_3V3_IN_PADCFG);
		gpio_request(GPIO_FP9928_EP_3V3_IN, "fp9928_EP_3V3");
		if(0!=gpio_request(GPIO_FP9928_EP_3V3_IN, "fp9928_EP_3V3")) {
			WARNING_MSG("%s(),request gpio fp9928_EP_3V3_IN fail !!\n",__FUNCTION__);
		}
		gpio_direction_output(GPIO_FP9928_EP_3V3_IN,0);
	}
#endif //]GPIO_FP9928_EP_3V3_IN_PADCFG

	GALLEN_DBGLOCAL_END();

	return iRet;
}

static void _fp9928_gpio_release(void)
{
	GALLEN_DBGLOCAL_BEGIN();
	if(!gptFP9928_data) {
		WARNING_MSG("%s(%d) : %s cannot work before init !\n",__FILE__,__LINE__,__FUNCTION__);
		GALLEN_DBGLOCAL_ESC();
		return ;
	}
	gpio_free(GPIO_FP9928_VCOM);
	gpio_free(GPIO_FP9928_EN);
	gpio_free(GPIO_FP9928_VIN);

#if defined(GPIO_FP9928_EP_3V3_IN) && (GPIO_FP9928_EP_3V3_IN!=GPIO_FP9928_VIN) //[
	if(gptFP9928_data->iIsEP3V3_SW_enabled) {
		gpio_free(GPIO_FP9928_EP_3V3_IN);
	}
#endif //]GPIO_FP9928_EP_3V3_IN_PADCFG
	GALLEN_DBGLOCAL_END();
}


static int _fp9928_reg_init(void)
{
	//FP9928_REG_SET(FUNC_ADJUST,FIX_RD_PTR,1);
	//FP9928_REG_GET(TMST);
	//FP9928_REG_GET(TMST);
	return 0;
}

static void _fp9928_reinit_vcom(void)
{
	ASSERT(gptFP9928_data);
	if(gptFP9928_data->iIsVCOMNeedReInit) {
		int iRetryCnt;
		int iChk;
		
		for(iRetryCnt=0;iRetryCnt<10;iRetryCnt++)
		{
			DBG_MSG("%s():re-write VCOM to 0x%02X\n",__FUNCTION__,FP9928_REG(VCOM_SETTING));
			iChk = FP9928_REG_SET(VCOM_SETTING,ALL,FP9928_REG(VCOM_SETTING));
			if(iChk>=0) {
				gptFP9928_data->iIsVCOMNeedReInit = 0;
				break;
			}
			else {
				ERR_MSG("%s():re-write VCOM to 0x%02X failed (%d)!!\n",__FUNCTION__,FP9928_REG(VCOM_SETTING),iRetryCnt);
			}
		}
	}
}

static int _fp9928_output_en(int iIsEnable)
{
	int iRet = FP9928_RET_SUCCESS;

	DBG_MSG("%s(%d)\n",__FUNCTION__,iIsEnable);

	if(!gptFP9928_data) {
		WARNING_MSG("%s(%d) : %s cannot work before init !\n",__FILE__,__LINE__,__FUNCTION__);
		return FP9928_RET_NOTINITEDSTATE;
	}

	if(gptFP9928_data->iIsOutputEnabled == iIsEnable) {
		// nothing have to do when state not change .
		if(iIsEnable) {
			DBG_MSG("%s() : output power already enabled\n",__FUNCTION__);
			gpio_direction_output(GPIO_FP9928_VCOM,VCOM_ON);
		}
	}
	else {
		//unsigned long dwTickNow = jiffies,dwTicks ;
		if(iIsEnable) {
			//FP9928_WAIT_TICKSTAMP(gptFP9928_data->dwTickPowerOffEnd,"pwroff stable");
			gpio_direction_output(GPIO_FP9928_EN,EN_ON);
			gptFP9928_data->iIsOutputEnabled = 1;

#ifdef USE_EPD_PWRSTAT //[
#else //][!USE_EPD_PWRSTAT
			gptFP9928_data->dwTickRailPowerOnEnd = jiffies + msecs_to_jiffies(gptFP9928_data->iRailPwrWaitON_ms) ;
#endif //] USE_EPD_PWRSTAT
			

			//msleep(10);
			_fp9928_reinit_vcom();
			//msleep(13);
#ifdef USE_EPD_PWRSTAT //[



#if (RAILPWR_SCHED_WAIT==1) //[
			if(in_interrupt()) 
#endif //]RAILPWR_SCHED_WAIT==1
			{
				unsigned long dwTickWaitStart=jiffies,dwTickTimesout,dwTickWaitStop;
				int iPG=gpio_get_value(GPIO_FP9928_PWRGOOD);
				unsigned long dwTestCnt=0;
			
				dwTickTimesout = dwTickWaitStart + HZ;

				while(!gpio_get_value(GPIO_FP9928_PWRGOOD)) {
					dwTestCnt++;
					if(jiffies>=dwTickTimesout) {
						printk(KERN_WARNING"%s(@INT) : turn on the rail power failed (1s timeout)\n",__FILE__);
						break;
					}
#if (RAILPWR_SCHED_WAIT>0) //[
					schedule_timeout(1);
#else //][RAILPWR_SCHED_WAIT>0
					udelay(10);
#endif //]
				}

				dwTickWaitStop = jiffies;
				DBG_MSG("%s(@INT):%d ticks(%d times) taken for rail power waiting,PG %d->%d\n",\
						__FUNCTION__,dwTickWaitStop-dwTickWaitStart,dwTestCnt,
						iPG,gpio_get_value(GPIO_FP9928_PWRGOOD));
			}
#if (RAILPWR_SCHED_WAIT==1) //[
			else {
				unsigned long dwTickWaitStart=jiffies,dwTickWaitStop;
				int iPG=gpio_get_value(GPIO_FP9928_PWRGOOD);

				wait_event_timeout(gptFP9928_data->tWQ_RailPower,gpio_get_value(GPIO_FP9928_PWRGOOD),HZ);
				if(!gpio_get_value(GPIO_FP9928_PWRGOOD)) {
					printk(KERN_WARNING"%s : turn on the rail power failed (1s timeout)\n",__FILE__);
				}
				dwTickWaitStop = jiffies;
				DBG_MSG("%s():%d ticks taken for rail power waiting,PG %d->%d\n",\
						__FUNCTION__,dwTickWaitStop-dwTickWaitStart,\
						iPG,gpio_get_value(GPIO_FP9928_PWRGOOD));

			}
#endif //]RAILPWR_SCHED_WAIT==1

#else //][!USE_EPD_PWRSTAT
			FP9928_WAIT_TICKSTAMP(gptFP9928_data->dwTickRailPowerOnEnd,"rail power on stable");
#endif //] USE_EPD_PWRSTAT


			gpio_direction_output(GPIO_FP9928_VCOM,VCOM_ON);
			msleep(10);
		}
		else {

			gpio_direction_output(GPIO_FP9928_EN,EN_OFF);
			gptFP9928_data->iIsOutputEnabled = 0;
			gptFP9928_data->dwTickPowerOffEnd = jiffies + FP9928_POWEROFF_TICKS_MAX;
		}
	}

	return iRet ;
}



static int _fp9928_vin_onoff(int iIsON)
{
	int iRet = FP9928_RET_SUCCESS;

	ASSERT(gptFP9928_data);

	DBG_MSG("%s(%d)\n",__FUNCTION__,iIsON);
	
	if(iIsON==gptFP9928_data->iIsPoweredON) {
	}
	else {
		if(iIsON) {
			gpio_direction_output(GPIO_FP9928_VIN,VIN_ON);
			gptFP9928_data->iIsPoweredON = 1;
			//msleep(10);
			gptFP9928_data->dwTickPowerOnEnd = jiffies + msecs_to_jiffies(gptFP9928_data->iChipPwrWaitON_ms) ;
		}
		else {
			_fp9928_output_en(0);
			FP9928_WAIT_TICKSTAMP(gptFP9928_data->dwTickPowerOffEnd,"pwroff stable");
			gpio_direction_output(GPIO_FP9928_VIN,VIN_OFF);
			gptFP9928_data->iIsPoweredON = 0;
			gptFP9928_data->iIsVCOMNeedReInit = 1;
			if(3==gptHWCFG->m_val.bUIConfig) {
				// MP/RD mode .
				gptFP9928_data->dwTickEP3V3OffEnd = jiffies + 0;
			}
			else {
				//gptFP9928_data->dwTickEP3V3OffEnd = jiffies + FP9928_EP3V3OFF_TICKS_MAX;
				gptFP9928_data->dwTickEP3V3OffEnd = jiffies + msecs_to_jiffies(gptFP9928_data->iVEEStableWait_ms);
			}
		}
	}

	return iRet;
}


static void _fp9928_pwrdwn_work_func(struct work_struct *work)
{
	GALLEN_DBGLOCAL_BEGIN();

	mutex_lock(&gptFP9928_data->tPwrLock);

	if(!gptFP9928_data->iIsOutputPowerDownCounting) {
		WARNING_MSG("[WARNING]%s(%d): race condition occured !\n",__FILE__,__LINE__);
		return ;
	}

	_fp9928_output_en(0);
	gptFP9928_data->iIsOutputPowerDownCounting = 0;

	if(gptFP9928_data->tPwrdwn_work_param.iIsTurnOffChipPwr) {
		_fp9928_vin_onoff(0);
	}

	mutex_unlock(&gptFP9928_data->tPwrLock);

	GALLEN_DBGLOCAL_END();
}


/**********************************************************************
 *
 * public functions .
 *
***********************************************************************/

int fp9928_power_onoff(int iIsPowerOn,int iIsOutputPwr)
{
	int iRet = FP9928_RET_SUCCESS;

	GALLEN_DBGLOCAL_BEGIN();
	if(!gptFP9928_data) {
		WARNING_MSG("%s(%d) : %s cannot work before init !\n",__FILE__,__LINE__,__FUNCTION__);
		GALLEN_DBGLOCAL_ESC();
		return FP9928_RET_NOTINITEDSTATE;
	}

		

	mutex_lock(&gptFP9928_data->tPwrLock);
	if (iIsPowerOn) {
		_fp9928_vin_onoff(1);

		if(iIsOutputPwr==1) {
			gptFP9928_data->iIsOutputPowerDownCounting = 0;
			cancel_delayed_work_sync(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work);
			_fp9928_output_en(1);
		}
	}
	else {
		gptFP9928_data->tPwrdwn_work_param.iIsTurnOffChipPwr = 1;
		if(!gptFP9928_data->iIsOutputPowerDownCounting) 
		{
			gptFP9928_data->iIsOutputPowerDownCounting = 1;
			cancel_delayed_work_sync(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work);
			schedule_delayed_work(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work, \
					FP9928_PWROFFDELAYWORK_TICKS);
		}
	}
	mutex_unlock(&gptFP9928_data->tPwrLock);	

	GALLEN_DBGLOCAL_BEGIN();
	return iRet;
}

int fp9928_output_power(int iIsOutputPwr,int iIsChipPowerDown)
{
	int iRet=FP9928_RET_SUCCESS;

	GALLEN_DBGLOCAL_BEGIN();

	if(!gptFP9928_data) {
		WARNING_MSG("%s(%d) : %s cannot work before init !\n",__FILE__,__LINE__,__FUNCTION__);
		GALLEN_DBGLOCAL_ESC();
		return FP9928_RET_NOTINITEDSTATE;
	}


	mutex_lock(&gptFP9928_data->tPwrLock);
	if(iIsOutputPwr) {
		GALLEN_DBGLOCAL_RUNLOG(0);
		gptFP9928_data->iIsOutputPowerDownCounting = 0;
		cancel_delayed_work_sync(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work);

		if(!gptFP9928_data->iIsPoweredON) {
			GALLEN_DBGLOCAL_RUNLOG(1);
			// auto power on chip .
			_fp9928_vin_onoff(1);
		}

		iRet = _fp9928_output_en(1);
	}
	else {
		GALLEN_DBGLOCAL_RUNLOG(2);
		if(!gptFP9928_data->iIsOutputPowerDownCounting) {
			GALLEN_DBGLOCAL_RUNLOG(3);

			udelay(100);gpio_direction_output(GPIO_FP9928_VCOM,VCOM_OFF);

			gptFP9928_data->tPwrdwn_work_param.iIsTurnOffChipPwr = iIsChipPowerDown;
			gptFP9928_data->iIsOutputPowerDownCounting = 1;
			cancel_delayed_work_sync(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work);
			schedule_delayed_work(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work, \
					FP9928_PWROFFDELAYWORK_TICKS);
		}
		else {
			GALLEN_DBGLOCAL_RUNLOG(4);
			DBG_MSG("%s(%d),power down work already exist \n",__FUNCTION__,__LINE__);
		}
	}
	mutex_unlock(&gptFP9928_data->tPwrLock);

	GALLEN_DBGLOCAL_END();
	return iRet;
}

#define FP9928_SUSPEND_ENABLED	1

int fp9928_suspend(void)
{
#ifdef FP9928_SUSPEND_ENABLED //[
	int iRet = FP9928_RET_SUCCESS;
	//int iChk;

	if(!gptFP9928_data) {
		WARNING_MSG("%s(%d) : %s cannot work before init !\n",
				__FILE__,__LINE__,__FUNCTION__);
		return FP9928_RET_NOTINITEDSTATE;
	}

	if(gptFP9928_data->iIsOutputEnabled) {
		WARNING_MSG("%s() : skip suspend when PMIC output enabled !! (%d)\n",__FUNCTION__,
				delayed_work_pending(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work));
		return FP9928_RET_PWRDWNWORKPENDING;
	}

	if(gSleep_Mode_Suspend) {
		mutex_lock(&gptFP9928_data->tPwrLock);
		_fp9928_vin_onoff(0);
		mutex_unlock(&gptFP9928_data->tPwrLock);

#if 0
		//FP9928_WAIT_TICKSTAMP(gptFP9928_data->dwTickEP3V3OffEnd,"pwroff->EP3V3 off stable");
#else
		if(time_before(jiffies,gptFP9928_data->dwTickEP3V3OffEnd)) {
			WARNING_MSG("%s():waiting for VEE stable ,please retry suspend later !!!\n",__FUNCTION__);
			return FP9928_RET_PWRDWNWORKPENDING;
		}
		else {
			#if defined(GPIO_FP9928_EP_3V3_IN) && (GPIO_FP9928_EP_3V3_IN!=GPIO_FP9928_VIN) //[
			if(gptFP9928_data->iIsEP3V3_SW_enabled) {
				gpio_direction_output(GPIO_FP9928_EP_3V3_IN,0);
			}
			#endif //] defined(GPIO_FP9928_EP_3V3_IN) && (GPIO_FP9928_EP_3V3_IN!=GPIO_FP9928_VIN)
		}
#endif
	}

	return iRet;
#else //][! FP9928_SUSPEND_ENABLED
	printk("%s() skipped !\n",__FUNCTION__);
	return 0;
#endif //] FP9928_SUSPEND_ENABLED
}

#define AVOID_ANIMATION_LOOP

void fp9928_shutdown(void)
{
	if(!gptFP9928_data) {
		WARNING_MSG("%s(%d) : %s cannot work before init !\n",
				__FILE__,__LINE__,__FUNCTION__);
		return ;
	}
	if(delayed_work_pending(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work)) {
#ifdef AVOID_ANIMATION_LOOP //[
		cancel_delayed_work_sync(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work);
#else //][!AVOID_ANIMATION_LOOP
		fp9928_power_onoff(0,0);
#endif //] AVOID_ANIMATION_LOOP
	}
#ifdef AVOID_ANIMATION_LOOP //[
	//DBG0_ENTRY_TAG();
	mutex_lock(&gptFP9928_data->tPwrLock);
	
	msleep(jiffies_to_msecs(FP9928_PWROFFDELAYWORK_TICKS));
	//DBG0_ENTRY_TAG();
	_fp9928_output_en(0);
	//DBG0_ENTRY_TAG();
	_fp9928_vin_onoff(0);
	//DBG0_ENTRY_TAG();

#else //][!AVOID_ANIMATION_LOOP 

	while (1) {
		if(gptFP9928_data->iIsOutputEnabled) {
			DBG0_MSG("%s() : waiting for PMIC output disabled !! (%d)\n",__FUNCTION__,
				delayed_work_pending(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work));
			msleep(100);
		}
		else {
			break;
		}
	}// while end .

#endif //] AVOID_ANIMATION_LOOP 

	while (1) {
		if(time_before(jiffies,gptFP9928_data->dwTickEP3V3OffEnd)) {
			DBG0_MSG("%s() : waiting for VEE stable to power off the EP3V3 ...\n",__FUNCTION__);
			msleep(100);
		}
		else {
			#if defined(GPIO_FP9928_EP_3V3_IN) && (GPIO_FP9928_EP_3V3_IN!=GPIO_FP9928_VIN) //[
			if(gptFP9928_data->iIsEP3V3_SW_enabled) {
				gpio_direction_output(GPIO_FP9928_EP_3V3_IN,0);
			}
			#endif //] defined(GPIO_FP9928_EP_3V3_IN) && (GPIO_FP9928_EP_3V3_IN!=GPIO_FP9928_VIN)
			break;
		}
	}// while end .

#ifdef AVOID_ANIMATION_LOOP//[
	mutex_unlock(&gptFP9928_data->tPwrLock);
#endif //]AVOID_ANIMATION_LOOP

}

void fp9928_resume(void)
{
#ifdef FP9928_SUSPEND_ENABLED//[
	if(!gptFP9928_data) {
		WARNING_MSG("%s(%d) : %s cannot work before init !\n",__FILE__,__LINE__,__FUNCTION__);
		return ;
	}

	if(gSleep_Mode_Suspend) {
		mutex_lock(&gptFP9928_data->tPwrLock);	
		_fp9928_vin_onoff(1);
		mutex_unlock(&gptFP9928_data->tPwrLock);

	}
#else
	printk("%s() skipped !\n",__FUNCTION__);
#endif //] FP9928_SUSPEND_ENABLED
}

int fp9928_ONOFF(int iIsON)
{
	int iRet=FP9928_RET_SUCCESS;

	if(!(8==gptHWCFG->m_val.bDisplayCtrl)) {
		WARNING_MSG("%s() display controller (%d) not match !\n",
				__FUNCTION__,(int)gptHWCFG->m_val.bDisplayCtrl);
		return 1;
	}
	


	mutex_lock(&gptFP9928_data->tPwrLock);	
	if(iIsON) {
#if defined(GPIO_FP9928_EP_3V3_IN) && (GPIO_FP9928_EP_3V3_IN!=GPIO_FP9928_VIN) //[
		if(gptFP9928_data->iIsEP3V3_SW_enabled) {
			DBG_MSG("%s() : Trun ON EP_3V3\n",__FUNCTION__);
			gpio_direction_output(GPIO_FP9928_EP_3V3_IN,1);
		}
#endif //] 

		_fp9928_vin_onoff(1);
	}
	else {
		_fp9928_vin_onoff(0);

#if defined(GPIO_FP9928_EP_3V3_IN) && (GPIO_FP9928_EP_3V3_IN!=GPIO_FP9928_VIN) //[
		/*
		if(gptFP9928_data->iIsEP3V3_SW_enabled) 
		{
			DBG_MSG("%s() : Trun OFF EP_3V3\n",__FUNCTION__);
			FP9928_WAIT_TICKSTAMP(gptFP9928_data->dwTickEP3V3OffEnd,
					"pwroff->EP3V3 off stable");
			gpio_direction_output(GPIO_FP9928_EP_3V3_IN,0);
		}
		*/
#endif //] 
	}
	mutex_unlock(&gptFP9928_data->tPwrLock);

	return iRet;
}


int fp9928_get_temperature(int *O_piTemperature)
{
	unsigned short wReg;
	int iRet=FP9928_RET_SUCCESS;

	int iTemp;
	unsigned char bReg,bTemp;
	int iOldPowerState;

	//return FP9928_RET_SUCCESS;

	if(!gptFP9928_data) {
		WARNING_MSG("%s() cannot work if driver is not initialed \n",__FUNCTION__);
		return FP9928_RET_NOTINITEDSTATE;
	}

	iOldPowerState = gptFP9928_data->iIsPoweredON;
	fp9928_output_power(1,0);
	
	wReg = FP9928_REG_GET(TMST);

	fp9928_ONOFF(iOldPowerState);

	if(((unsigned short)(-1))==wReg) {
		ERR_MSG("%s(%d):%s regTMST read fail !\n",__FILE__,__LINE__,__FUNCTION__);
		return FP9928_RET_I2CTRANS_ERR;
	}

	bReg = (unsigned char)wReg;
	gptFP9928_data->wTempratureData = wReg;
	if(bReg&0x80) {
		// negative .
		bTemp=(~bReg)+1;
		iTemp = bTemp;
		iTemp = (~iTemp)+1;
	}
	else {
		// positive .
		iTemp = (int)(bReg);
	}
	gptFP9928_data->iCurrent_temprature = iTemp;
	printk("%s temprature data = 0x%x,%d\n",DRIVER_NAME,wReg,gptFP9928_data->iCurrent_temprature);

	if(O_piTemperature) {
		*O_piTemperature = gptFP9928_data->iCurrent_temprature;
	}

	return iRet;
}





int fp9928_vcom_set(int iVCOM_mV,int iIsWriteToFlash)
{

	const int iVCOM_mV_max=FP9928_VCOM_MV_MAX,iVCOM_mV_min=FP9928_VCOM_MV_MIN;
	int iVCOM_mV_ABS ;
	int iRet=FP9928_RET_SUCCESS;

	if(!gptFP9928_data) {
		WARNING_MSG("%s() cannot work if driver is not initialed \n",__FUNCTION__);
		return FP9928_RET_NOTINITEDSTATE;
	}
	
	if(iVCOM_mV<iVCOM_mV_min) {
		ERR_MSG("%s(%d),VCOM %d cannot < %d mV\n",
				__FUNCTION__,__LINE__,iVCOM_mV,iVCOM_mV_min);
	}
	else if(iVCOM_mV>iVCOM_mV_max) {
		ERR_MSG("%s(%d),VCOM %d cannot > %d\n",
				__FUNCTION__,__LINE__,iVCOM_mV,iVCOM_mV_max);
	}
	else {
		unsigned char bReg;
		int i10uV_Steps;
		int i10uV_Steps_mod10;

		int iOldPowerState ;


		if( iVCOM_mV<(gptFP9928_data->iCurrent_VCOM_mV+FP9928_VCOM_UV_STEP/1000) && \
			  iVCOM_mV>(gptFP9928_data->iCurrent_VCOM_mV-FP9928_VCOM_UV_STEP/1000)	) 
		{
			// the VCOM range you want to set is close to current VCOM.
			return FP9928_RET_SUCCESS;
		}

		if(iVCOM_mV<0) {
			iVCOM_mV_ABS = -iVCOM_mV;
		}
		else {
			iVCOM_mV_ABS = iVCOM_mV;
		}

		iOldPowerState = gptFP9928_data->iIsPoweredON;
		fp9928_ONOFF(1);
		i10uV_Steps = (int)(iVCOM_mV_ABS*10000/FP9928_VCOM_UV_STEP);
		i10uV_Steps_mod10 = (int)(i10uV_Steps%10);
		if(i10uV_Steps_mod10>=5) {
			bReg = (unsigned char) ((i10uV_Steps/10)+1);
		}
		else {
			bReg = (unsigned char) (i10uV_Steps/10);
		}
		printk("%s():want set VCOM %dmV,reg=0x%02X,output %dmV,to flash=%d\n",\
				__FUNCTION__,iVCOM_mV,bReg,bReg*FP9928_VCOM_UV_STEP/1000,iIsWriteToFlash);
		iRet = FP9928_REG_SET(VCOM_SETTING,ALL,bReg);
		if(iRet>=0) {
			gptFP9928_data->iCurrent_VCOM_mV=-((int)FP9928_REG(VCOM_SETTING)*FP9928_VCOM_UV_STEP/1000);
		}
		fp9928_ONOFF(iOldPowerState);

	}

	return iRet;
}


int fp9928_vcom_get(int *O_piVCOM_mV)
{
	int iVCOM_mV;
	unsigned short wReg;
	int iOldPowerState;
	int iRet = FP9928_RET_SUCCESS;

	if(!gptFP9928_data) {
		WARNING_MSG("%s() cannot work if driver is not initialed \n",__FUNCTION__);
		return FP9928_RET_NOTINITEDSTATE;
	}

	iOldPowerState = gptFP9928_data->iIsPoweredON;
	fp9928_ONOFF(1);

	_fp9928_reinit_vcom();

	wReg = FP9928_REG_GET(VCOM_SETTING);
	iVCOM_mV = (int)(wReg);
	iVCOM_mV = -(iVCOM_mV*FP9928_VCOM_UV_STEP/1000);
	//DBG_MSG("%s(%d):iVCOM_mV=%d,wReg=0x%x\n",__FUNCTION__,__LINE__,iVCOM_mV,wReg);

	if(iVCOM_mV!=gptFP9928_data->iCurrent_VCOM_mV) {
		WARNING_MSG("%s(%d) VCOM read from register is 0x%x not equal with stored \n",__FUNCTION__,__LINE__,wReg);
	}

	if(O_piVCOM_mV) {
		*O_piVCOM_mV = iVCOM_mV;
		gptFP9928_data->iCurrent_VCOM_mV=iVCOM_mV;
	}

	fp9928_ONOFF(iOldPowerState);

	return iRet;
}

int fp9928_vcom_get_cached(int *O_piVCOM_mV)
{
	if(!gptFP9928_data) {
		WARNING_MSG("%s() cannot work if driver is not initialed \n",__FUNCTION__);
		return FP9928_RET_NOTINITEDSTATE;
	}

	if(O_piVCOM_mV) {
		*O_piVCOM_mV = gptFP9928_data->iCurrent_VCOM_mV;
	}

	return FP9928_RET_SUCCESS;
}


void fp9928_release(void)
{
	if(!gptFP9928_data) {
		WARNING_MSG("%s() cannot work if driver is not initialed \n",__FUNCTION__);
		return ;
	}


	mutex_lock(&gptFP9928_data->tPwrLock);	
	_fp9928_vin_onoff(0);
	mutex_unlock(&gptFP9928_data->tPwrLock);	

	gptFP9928_data->ptI2C_adapter = 0;
	i2c_unregister_device(gptFP9928_data->ptI2C_client);
	gptFP9928_data->ptI2C_client = 0;
	
	_fp9928_gpio_release();

	kfree(gptFP9928_data);gptFP9928_data = 0;

}

int fp9928_init(int iPort)
{

	int iRet = FP9928_RET_SUCCESS;
	int iChk;

	unsigned long dwSize;

	GALLEN_DBGLOCAL_BEGIN();

	if(gptFP9928_data) {
		WARNING_MSG("skipped %s() calling over twice !!\n",__FUNCTION__);
		return 0;
	}

	dwSize=sizeof(FP9928_data);

	gptFP9928_data = kmalloc(dwSize,GFP_KERNEL);
	if(!gptFP9928_data) {
		iRet = FP9928_RET_MEMNOTENOUGH;
		ERR_MSG("%s(%d) : memory not enough !!\n",__FILE__,__LINE__);
		GALLEN_DBGLOCAL_RUNLOG(0);
		goto MEM_MALLOC_FAIL;
	}

	memset(gptFP9928_data,0,sizeof(FP9928_data));
	
	if(36==gptHWCFG->m_val.bPCB || 40==gptHWCFG->m_val.bPCB) {
		// E60Q3X/E60Q5X
		if((0==gptHWCFG->m_val.bPCB_LVL&&gptHWCFG->m_val.bPCB_REV>=0x10) || 40==gptHWCFG->m_val.bPCB) {
			// >= E60Q30A10 ,E60Q5X
			printk("%s(): EP3V3 switch enabled",__FUNCTION__);
			gptFP9928_data->iIsEP3V3_SW_enabled = 1;
		}
		else {
			gptFP9928_data->iIsEP3V3_SW_enabled = 0;
		}
#ifdef USE_EPD_PWRSTAT //[
#else //][!USE_EPD_PWRSTAT
		gptFP9928_data->iRailPwrWaitON_ms = 10;
#endif //] USE_EPD_PWRSTAT
		
		gptFP9928_data->iChipPwrWaitON_ms = 10;
		gptFP9928_data->iVEEStableWait_ms = 3500;
	}
	else {
#ifdef USE_EPD_PWRSTAT //[
#else //][!USE_EPD_PWRSTAT
		gptFP9928_data->iRailPwrWaitON_ms = 60;
#endif //] USE_EPD_PWRSTAT
		
		gptFP9928_data->iIsEP3V3_SW_enabled = 0;
		gptFP9928_data->iChipPwrWaitON_ms = 50;
		if( 1 == gptHWCFG->m_val.bDisplayResolution) {
			// 1024x758 .
			gptFP9928_data->iVEEStableWait_ms = 6500;
		}
		else {
			gptFP9928_data->iVEEStableWait_ms = 3500;
		}
	}

	gptFP9928_data->dwTickPowerOffEnd = jiffies;
	gptFP9928_data->dwTickPowerOnEnd = jiffies;

#ifdef USE_EPD_PWRSTAT //[
	init_waitqueue_head(&gptFP9928_data->tWQ_RailPower);
#else //][!USE_EPD_PWRSTAT
	gptFP9928_data->dwTickRailPowerOnEnd = jiffies;
#endif //] USE_EPD_PWRSTAT
	
	gptFP9928_data->dwTickEP3V3OffEnd = jiffies;

	iChk = _fp9928_gpio_init();
	if(iChk<0) {
		iRet = FP9928_RET_GPIOINITFAIL;
		ERR_MSG("%s(%d) : gpio init fail !!\n",__FILE__,__LINE__);
		GALLEN_DBGLOCAL_RUNLOG(1);
		goto GPIO_INIT_FAIL;
	}


	_fp9928_vin_onoff(1);


	gptFP9928_data->ptI2C_adapter = i2c_get_adapter(iPort-1);//
	if( NULL == gptFP9928_data->ptI2C_adapter) {
		ERR_MSG ("[Error] %s : FP9928_RET_I2CCHN_NOTFOUND,chn=%d\n",__FUNCTION__,iPort);
		GALLEN_DBGLOCAL_RUNLOG(2);
		iRet=FP9928_RET_I2CCHN_NOTFOUND;
		goto I2CCHN_GET_FAIL;
	}

	gptFP9928_data->ptI2C_client = i2c_new_probed_device(gptFP9928_data->ptI2C_adapter, &gtFP9928_BI,gwFP9928_AddrA,0);
	if( NULL == gptFP9928_data->ptI2C_client ) {
		GALLEN_DBGLOCAL_RUNLOG(3);
		ERR_MSG("[Error] %s : FP9928 probe fail \n",__FUNCTION__);
		goto I2CPROBE_DEVICE_FAIL;
	}

	gptFP9928_data->iCurrent_VCOM_mV=-((int)FP9928_REG(VCOM_SETTING)*FP9928_VCOM_UV_STEP/1000);// default VCOM voltage .
	//fp9928_vcom_get(&gptFP9928_data->iCurrent_VCOM_mV);


	// kernel objects initialize ...
	mutex_init(&gptFP9928_data->tPwrLock);
	mutex_init(&gptFP9928_data->tI2CLock);

	INIT_DELAYED_WORK(&gptFP9928_data->tPwrdwn_work_param.pwrdwn_work,_fp9928_pwrdwn_work_func);


	_fp9928_reg_init();
	fp9928_ONOFF(INIT_POWER_STATE);

	GALLEN_DBGLOCAL_ESC();
	return FP9928_RET_SUCCESS;


	i2c_unregister_device(gptFP9928_data->ptI2C_client);
	gptFP9928_data->ptI2C_client = 0;
I2CPROBE_DEVICE_FAIL:
	gptFP9928_data->ptI2C_adapter = 0;
I2CCHN_GET_FAIL:
	_fp9928_gpio_release();
GPIO_INIT_FAIL:
	kfree(gptFP9928_data);gptFP9928_data = 0;
MEM_MALLOC_FAIL:
	
	GALLEN_DBGLOCAL_END();
	return iRet;
}



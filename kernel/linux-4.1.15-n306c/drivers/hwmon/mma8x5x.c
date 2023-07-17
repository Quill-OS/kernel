/*
 *  mma8x5x.c - Linux kernel modules for 3-Axis Orientation/Motion
 *  Detection Sensor MMA8451/MMA8452/MMA8453/MMA8652/MMA8653
 *
 *  Copyright (C) 2012-2014 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/poll.h>
#include <linux/hwmon.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/bitops.h>
#include "../arch/arm/mach-imx/ntx_hwconfig.h"

//#define MMA8X5X_DUMP_REGS_SYSFS		1
#define irq_to_gpio(irq)	((irq) - gpio_to_irq(0))

#define MMA8X5X_I2C_ADDR        0x1D
#define MMA8451_ID                      0x1A
#define MMA8452_ID                      0x2A
#define MMA8453_ID                      0x3A
#define MMA8652_ID                      0x4A
#define MMA8653_ID                      0x5A


#define POLL_INTERVAL_MIN       1
#define POLL_INTERVAL_MAX       500
#define POLL_INTERVAL           50 		/* defalut 100 msecs */

/* if sensor is standby ,set POLL_STOP_TIME to slow down the poll */
#define POLL_STOP_TIME          200
#define INPUT_FUZZ                      32
#define INPUT_FLAT                      32
#define MODE_CHANGE_DELAY_MS    100

#define MMA8X5X_STATUS_ZYXDR    0x08
#define MMA8X5X_BUF_SIZE    	6
#define MMA8X5X_FIFO_SIZE		32

#define NON_STUCK			0
#define STUCK				1
#define TIMEOUT_1_SEC		1
#define CONST_1024_0_886 	32000	// Z angle 
#define JIFFIES_INTERVAL		40

//#define INTCNT_DBG		1
#define FUNC_INT_DELAY_WORK		1

//	Angle	Cos (value)		CONST_1024_0_886
//	5		0.996194698		32643.30786
//	10		0.984807753		32270.18045
//	15		0.965925826		31651.45747
//	20		0.939692621		30791.8478
//	25		0.906307787		29697.89356
//	30		0.866025404		28377.92044
//	35		0.819152044		26841.97418
//	40		0.766044443		25101.74431
//	45		0.707106781		23170.475
//	50		0.64278761		21062.8644


/* register enum for mma8x5x registers */
enum {
	MMA8X5X_STATUS = 0x00,
	MMA8X5X_OUT_X_MSB,
	MMA8X5X_OUT_X_LSB,
	MMA8X5X_OUT_Y_MSB,
	MMA8X5X_OUT_Y_LSB,
	MMA8X5X_OUT_Z_MSB,
	MMA8X5X_OUT_Z_LSB,

	MMA8X5X_F_SETUP = 0x09,
	MMA8X5X_TRIG_CFG,
	MMA8X5X_SYSMOD,
	MMA8X5X_INT_SOURCE,
	MMA8X5X_WHO_AM_I,
	MMA8X5X_XYZ_DATA_CFG,
	MMA8X5X_HP_FILTER_CUTOFF,

	MMA8X5X_PL_STATUS,
	MMA8X5X_PL_CFG,
	MMA8X5X_PL_COUNT,
	MMA8X5X_PL_BF_ZCOMP,
	MMA8X5X_P_L_THS_REG,

	MMA8X5X_FF_MT_CFG,
	MMA8X5X_FF_MT_SRC,
	MMA8X5X_FF_MT_THS,
	MMA8X5X_FF_MT_COUNT,

	MMA8X5X_TRANSIENT_CFG = 0x1D,
	MMA8X5X_TRANSIENT_SRC,
	MMA8X5X_TRANSIENT_THS,
	MMA8X5X_TRANSIENT_COUNT,

	MMA8X5X_PULSE_CFG,
	MMA8X5X_PULSE_SRC,
	MMA8X5X_PULSE_THSX,
	MMA8X5X_PULSE_THSY,
	MMA8X5X_PULSE_THSZ,
	MMA8X5X_PULSE_TMLT,
	MMA8X5X_PULSE_LTCY,
	MMA8X5X_PULSE_WIND,

	MMA8X5X_ASLP_COUNT,
	MMA8X5X_CTRL_REG1,
	MMA8X5X_CTRL_REG2,
	MMA8X5X_CTRL_REG3,
	MMA8X5X_CTRL_REG4,
	MMA8X5X_CTRL_REG5,

	MMA8X5X_OFF_X,
	MMA8X5X_OFF_Y,
	MMA8X5X_OFF_Z,

	MMA8X5X_REG_END,
};


/*
 * CTRL REG4 : Interrupt Enable register .
 *   bit7 : Auto-SLEEP/WAKE interrupt enable
 *   bit6 : FIFO interrupt enable
 *   bit5 : Transient interrupt enable
 *   bit4 : Orientation interrupt enable
 *   bit3 : Pulse Detection interrupt enable
 *   bit2 : Freefall/Motion interrupt enable
 *   bit1 : reserved .
 *   bit0 : Data ready interrupt enable
 */
//#define ZSTICK_SUSPEND_INTEN		0x20
//#define NORMAL_SUSPEND_INTEN		0x10
#define ZSTICK_DEFAULT_INTEN		0x20
#define NORMAL_DEFAULT_INTEN		0x10
	// Interrupt Enable register (0x34: Transient / Orientation interrupt/Freefall/Motion Interrupt Enable)
	// Interrupt Enable register (0x30: Transient / Orientation interrupt Enable)
	// Interrupt Enable register (0x10: Orientation Interrupt Enable)
	// Interrupt Enable register (0x20: Transient Interrupt Enable)
	

/*
 * CTRL REG3 : Interrupt Control register .
 *   bit7 : FIFO Gate 
 *   bit6 : WAKE_TRANS
 *   bit5 : WAKE_LNDPRT
 *   bit4 : WAKE_PULSE
 *   bit3 : WAKE_FF_MT
 *   bit2 : reserved .
 *   bit1 : IPOL (Interrupt polarity)
 *   bit0 : PP_OD 
 */
//#define ZSTICK_SUSPEND_INTCTRL		0x40
//#define NORMAL_SUSPEND_INTCTRL		0x20
#define ZSTICK_DEFAULT_INTCTRL		0x40
#define NORMAL_DEFAULT_INTCTRL		0x20
	// Wake from Orientation interrupt / Transient interrupt + Freefall/Motion interrupt : 0x68 ,
	// Wake from Orientation interrupt / Transient interrupt : 0x60 ,
	// Wake from Orientation interrupt : 0x20 ,
	// Wake from Transient interrupt : 0x40 ,
#define ZSTICK_DEFAULT_FF_MT_CFG	0xd8
	// FF_MT_CFG , Motion config, ignore Z-event : 0xd8 , all event: 0xf8
#define ZSTICK_DEFAULT_FF_MT_THS	0x10
	// Freefall and Motion Threshold register

/* The sensitivity is represented in counts/g. In 2g mode the
   sensitivity is 1024 counts/g. In 4g mode the sensitivity is 512
   counts/g and in 8g mode the sensitivity is 256 counts/g.
 */
enum {
	MODE_2G = 0,
	MODE_4G,
	MODE_8G,
};

enum {
	MMA_STANDBY = 0,
	MMA_ACTIVED,
};
#pragma pack(1)
struct mma8x5x_data_axis {
	short x;
	short y;
	short z;
};
struct mma8x5x_fifo{
	int count;
	s64 period;
	s64 timestamp;
	struct mma8x5x_data_axis fifo_data[MMA8X5X_FIFO_SIZE];
};
#pragma pack()

struct mma8x5x_data {
	struct i2c_client *client;
	struct input_dev *idev;
	struct delayed_work work;
	struct mutex data_lock;
	struct mma8x5x_fifo fifo;
	wait_queue_head_t fifo_wq;
	atomic_t fifo_ready;
	int active;
	int delay;
	int position;
	u8  chip_id;
	int mode;
	int awaken;			// is just awake from suspend
	s64 period_rel;
	int fifo_wakeup;
	int fifo_timeout;
	u8	pl_cfg;
	u8 	pl_count;
	u8	p_l_ths_reg;
	u8	pl_bf_zcomp;
	u8	aslp_count;

	int iInterruptPending;
	unsigned long int_jiffies;
#ifdef FUNC_INT_DELAY_WORK //[
	struct delayed_work func_int_delay_work;
#endif //] FUNC_INT_DELAY_WORK
#ifdef INTCNT_DBG //[
	unsigned long last_int_jiffies;
	unsigned long dwIntCnt;
#endif //]INTCNT_DBG
};


enum  {
	PORTRAIT_NONE = 0x00,
	PORTRAIT_UP,
	PORTRAIT_DN,
	LANDSCAPE_LEFT,
	LANDSCAPE_RIGHT,
};


static int PL_temp_status = 0; 					// P/L previous status is none
static int Z_previous_value = 0; 				// Stores last time Z value for comparison with next Z value
static int timeout_counter = 0; 				// time_out counter
static int PortraitDn_counter = 0;  			// P/L change counter
static int PortraitUp_counter = 0;  			// P/L change counter
static int LandscapeL_counter = 0;  			// P/L change counter
static int LandscapeR_counter = 0;  			// P/L change counter
static int PL_temp_event = 0;
static int PL_event_counter = 0;
static int PL_Send_temp_event = 0;
static int PL_Send_Event = 0;
static int mma8x5x_z_failed = 1;				// Default: 1 
static int mma8x5x_stick_mode = 0;				// 0: close , 1: open
static int flag_stuck_status = NON_STUCK; 		// Status flag, initially non_stuck 
static unsigned long send_jiffies = 0;
static int gPL_Status =0 ;
static int GetPLStatus(struct mma8x5x_data_axis *data);
static int mma8x5x_read_data(struct i2c_client *client,	struct mma8x5x_data_axis *data);
static int mma8x5x_data_convert(struct mma8x5x_data *pdata,	struct mma8x5x_data_axis *axis_data);
static int mma8x5x_device_stop(struct i2c_client *client);
static int mma8x5x_device_start(struct i2c_client *client);

//------------------------------------------------------------------------


extern void ntx_report_event(unsigned int type, unsigned int code, int value);
static struct mma8x5x_data * p_mma8x5x_data = NULL;
static volatile u8 bFirstINT = 0 ;
static int orient_triggered ;
extern int gSleep_Mode_Suspend;
extern volatile NTX_HWCONFIG *gptHWCFG;
static const char MMA8X5X_NAME[]	= "mma8X5X";
/* Addresses scanned */
static const unsigned short normal_i2c[] = { 0x1c, 0x1d, I2C_CLIENT_END };

static int mma8x5x_chip_id[] = {
	MMA8451_ID,
	MMA8452_ID,
	MMA8453_ID,
	MMA8652_ID,
	MMA8653_ID,
};
static char *mma8x5x_names[] = {
	"mma8451",
	"mma8452",
	"mma8453",
	"mma8652",
	"mma8653",
};
static int mma8x5x_position_setting[8][3][3] = {
	{ { 0,	-1, 0  }, { 1,	0,  0	   }, { 0, 0, 1	     } },
	{ { -1, 0,  0  }, { 0,	-1, 0	   }, { 0, 0, 1	     } },
	{ { 0,	1,  0  }, { -1, 0,  0	   }, { 0, 0, 1	     } },
	{ { 1,	0,  0  }, { 0,	1,  0	   }, { 0, 0, 1	     } },

	{ { 0,	-1, 0  }, { -1, 0,  0	   }, { 0, 0, -1     } },
	{ { -1, 0,  0  }, { 0,	1,  0	   }, { 0, 0, -1     } },
	{ { 0,	1,  0  }, { 1,	0,  0	   }, { 0, 0, -1     } },
	{ { 1,	0,  0  }, { 0,	-1, 0	   }, { 0, 0, -1     } },
};

#define REG_NAME_ADDR(name)	{#name,name}
struct regs {
	char *name;
	u8 addr;
}
static dump_regs[] = {
	REG_NAME_ADDR(MMA8X5X_STATUS),

	REG_NAME_ADDR(MMA8X5X_OUT_X_MSB),
	REG_NAME_ADDR(MMA8X5X_OUT_X_LSB),
	REG_NAME_ADDR(MMA8X5X_OUT_Y_MSB),
	REG_NAME_ADDR(MMA8X5X_OUT_Y_LSB),
	REG_NAME_ADDR(MMA8X5X_OUT_Z_MSB),
	REG_NAME_ADDR(MMA8X5X_OUT_Z_LSB),


	REG_NAME_ADDR(MMA8X5X_F_SETUP),
	REG_NAME_ADDR(MMA8X5X_TRIG_CFG),
	REG_NAME_ADDR(MMA8X5X_SYSMOD),
	REG_NAME_ADDR(MMA8X5X_INT_SOURCE),
	REG_NAME_ADDR(MMA8X5X_WHO_AM_I),
	REG_NAME_ADDR(MMA8X5X_XYZ_DATA_CFG),
	REG_NAME_ADDR(MMA8X5X_HP_FILTER_CUTOFF),
	
	REG_NAME_ADDR(MMA8X5X_PL_STATUS),
	REG_NAME_ADDR(MMA8X5X_PL_CFG),
	REG_NAME_ADDR(MMA8X5X_PL_COUNT),
	REG_NAME_ADDR(MMA8X5X_PL_BF_ZCOMP),
	REG_NAME_ADDR(MMA8X5X_P_L_THS_REG),

	REG_NAME_ADDR(MMA8X5X_FF_MT_CFG),
	REG_NAME_ADDR(MMA8X5X_FF_MT_SRC),
	REG_NAME_ADDR(MMA8X5X_FF_MT_THS),
	REG_NAME_ADDR(MMA8X5X_FF_MT_COUNT),

	REG_NAME_ADDR(MMA8X5X_TRANSIENT_CFG),
	REG_NAME_ADDR(MMA8X5X_TRANSIENT_SRC),
	REG_NAME_ADDR(MMA8X5X_TRANSIENT_THS),
	REG_NAME_ADDR(MMA8X5X_TRANSIENT_COUNT),

	REG_NAME_ADDR(MMA8X5X_PULSE_CFG),
	REG_NAME_ADDR(MMA8X5X_PULSE_SRC),
	REG_NAME_ADDR(MMA8X5X_PULSE_THSX),
	REG_NAME_ADDR(MMA8X5X_PULSE_THSY),
	REG_NAME_ADDR(MMA8X5X_PULSE_THSZ),
	REG_NAME_ADDR(MMA8X5X_PULSE_TMLT),
	REG_NAME_ADDR(MMA8X5X_PULSE_LTCY),
	REG_NAME_ADDR(MMA8X5X_PULSE_WIND),

	REG_NAME_ADDR(MMA8X5X_ASLP_COUNT),
	REG_NAME_ADDR(MMA8X5X_CTRL_REG1),
	REG_NAME_ADDR(MMA8X5X_CTRL_REG2),
	REG_NAME_ADDR(MMA8X5X_CTRL_REG3),
	REG_NAME_ADDR(MMA8X5X_CTRL_REG4),
	REG_NAME_ADDR(MMA8X5X_CTRL_REG5),

	REG_NAME_ADDR(MMA8X5X_OFF_X),
	REG_NAME_ADDR(MMA8X5X_OFF_Y),
	REG_NAME_ADDR(MMA8X5X_OFF_Z),

} ;

static void mma8x5x_regs_dump(struct mma8x5x_data *pdata,const char *pszTitle)
{
	int i;
	u8 bVal;

	printk(KERN_ERR"=== %s === \n",pszTitle);
	if(!pdata) {
		return ;
	}

	for(i=0;i<sizeof(dump_regs)/sizeof(dump_regs[0]);i++) {
		bVal = i2c_smbus_read_byte_data(pdata->client, dump_regs[i].addr);
		printk(KERN_ERR"%s(%02Xh)=0x%x\n",dump_regs[i].name,dump_regs[i].addr,bVal);
	}
}
static char * _int_srcs_strings(u8 int_src)
{
	static char cINTsA[128];
	const char * cINT_SRCA[8]={"DRDY","-","FF_MT","PULSE","LNDPRT","TRANS","FIFO","ASLP"};
	int i,iLen;
	u8 bChk;
	char *pc;

	cINTsA[0] = "\0";
	pc = &cINTsA[0];
	for(i=0,bChk=1;i<8;i++,bChk=bChk<<1) {
		if(int_src&bChk) {
			sprintf(pc,"%s,",cINT_SRCA[i]);
			iLen = strlen(pc);
			pc+=iLen;
		}
	}
	return cINTsA;
}
static int DetectMMA8x5x(struct mma8x5x_data *pdata)
{
	int res = 0  , ret =0;
	char MSB[4] = {0};
	char LSB[4] = {0};

	if(mma8x5x_stick_mode)
		return 0;

	mutex_lock(&pdata->data_lock);

	MSB[0] = i2c_smbus_read_byte_data(pdata->client,MMA8X5X_OUT_Z_MSB);
	LSB[0] = i2c_smbus_read_byte_data(pdata->client,MMA8X5X_OUT_Z_LSB);


	// Detect MMA8X5X Z
	if( (0x80 == MSB[0]  && 0x00 == LSB[0] ) ||
		(0x7f == MSB[0]  && 0xff == LSB[0] ) ||
		(0x7f == MSB[0]  && 0xf0 == LSB[0] )  )
	{
		if(mma8x5x_z_failed==0)
		{
			mma8x5x_z_failed = 1;
			printk(KERN_ERR"---------[%s] Detect MMA8X5X Z FAILED,( Z: 0x%x%x )\n",__FUNCTION__,MSB[0],LSB[0]);
			mma8x5x_device_stop(pdata->client);
			ret = i2c_smbus_write_byte_data(pdata->client, MMA8X5X_CTRL_REG3, ZSTICK_DEFAULT_INTCTRL);
			ret = i2c_smbus_write_byte_data(pdata->client, MMA8X5X_CTRL_REG4, ZSTICK_DEFAULT_INTEN);
			ret = i2c_smbus_write_byte_data(pdata->client, MMA8X5X_FF_MT_CFG, ZSTICK_DEFAULT_FF_MT_CFG);
			ret = i2c_smbus_write_byte_data(pdata->client, MMA8X5X_FF_MT_THS, ZSTICK_DEFAULT_FF_MT_THS);	
			// Transient_CFG register, 0x70~0x00
			ret = i2c_smbus_write_byte_data(pdata->client, MMA8X5X_TRANSIENT_CFG, 0x02);
			// TRANSIENT_THS register, 0x70~0x00	
			ret = i2c_smbus_write_byte_data(pdata->client, MMA8X5X_TRANSIENT_THS, 0x03);
			mma8x5x_device_start(pdata->client);
			res = 1 ;
		}
	}
	else{
		if(mma8x5x_z_failed==1)
		{
			mma8x5x_z_failed = 0 ;
			printk(KERN_ERR"---------[%s] Disable Transient Interrupt (Z Back to normal) \n",__FUNCTION__);
			mma8x5x_device_stop(pdata->client);
			// Wake from Orientation interrupt / Transient interrupt : 0x60 ,
			ret = i2c_smbus_write_byte_data(pdata->client, MMA8X5X_CTRL_REG3, NORMAL_DEFAULT_INTCTRL);
			// Interrupt Enable register (0x30: Transient / Orientation interrupt)
			ret = i2c_smbus_write_byte_data(pdata->client, MMA8X5X_CTRL_REG4, NORMAL_DEFAULT_INTEN);
			mma8x5x_device_start(pdata->client);
		}
	}

	mutex_unlock(&pdata->data_lock);	
	return res;
}


static int _mma8x5x_func_int_check(struct mma8x5x_data *pdata)
{
	u8 int_src;
	u8 tmp_data[1];


	int_src = i2c_smbus_read_byte_data(pdata->client,MMA8X5X_INT_SOURCE);
#ifdef INTCNT_DBG //[
#if 0
	if(pdata->iInterruptPending) {
		printk(KERN_ERR"%s : interrupt to isr = %u ms\n",
			__FUNCTION__,jiffies_to_msecs(jiffies-pdata->int_jiffies));
	}
#endif

	pdata->dwIntCnt++;
	if(time_is_before_jiffies(pdata->last_int_jiffies)) {
		printk(KERN_ERR"intcnt=%d,int_src=0x%x\n",pdata->dwIntCnt,int_src);
		pdata->last_int_jiffies = jiffies+HZ;
		pdata->dwIntCnt=0;
	}
#endif //]INTCNT_DBG

	//printk("%s(),%s\n",__FUNCTION__,_int_srcs_strings(int_src));
	//printk(KERN_ERR"----------INT:%u  \n",int_src);
	if (int_src & 0x10) {	// Orientation triggered
		//printk("[mma8652] Orientation int triggered!!\n"); 	
		tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_STATUS);
		if(2==bFirstINT) {
			goto out;
		}
		orient_triggered = 1;

		//if((tmp_data[0] & 0x80) == 1)
		if(mma8x5x_stick_mode != 1)
		{
			//printk(KERN_ERR"-----PL_STATUS : 0x%x \n",i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_STATUS));
			if(  (tmp_data[0]& 0x06) ==0)
			{
				//printk(KERN_ERR"-----PL_STATUS : Portrait Down \n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_DOWN);	
			}
			else if( (tmp_data[0] & 0x06 ) ==2)
			{
				//printk(KERN_ERR"-----PL_STATUS : Portrait Up \n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_UP);	
			}
			else if( (tmp_data[0] & 0x06 ) ==4)
			{
				//printk(KERN_ERR"-----PL_STATUS : Landscape Right \n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_RIGHT);		
			}
			else if( (tmp_data[0] & 0x06 ) ==6)
			{
				//printk(KERN_ERR"-----PL_STATUS : Landscape Left \n");	
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_LEFT);			
			}
			if( (tmp_data[0] & 0x01 ) ==0)
			{
				//printk(KERN_ERR"-----PL_STATUS : Back \n");	
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_BACK);	
			}
			else if( (tmp_data[0] & 0x01 ) ==1)
			{
				//printk(KERN_ERR"-----PL_STATUS : Front \n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_FRONT);				
			}	
		}
	}
	else if(int_src &0x04) // Freefall/Motion interrup
	{
		tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_FF_MT_SRC);
		//printk(KERN_ERR"----motion interrupt \n");
	}
	else if (int_src & 0x80) // Auto-SLEEP/WAKE
	{
		//printk(KERN_ERR"-----Auto-SLEEP/WAKE \n");
	}
	else if(int_src & 0x20) // Transient interrupt
	{

	}

	if(DetectMMA8x5x(pdata))
		printk("====== [MMA8X5X is failed] ======\n");

out:
	pdata->iInterruptPending = 0;
	return 0;
}

#ifdef FUNC_INT_DELAY_WORK //[
static void mma8x5x_func_int_work_func(struct work_struct *work)
{
	struct mma8x5x_data *pdata = container_of(work,struct mma8x5x_data,func_int_delay_work);
	_mma8x5x_func_int_check(pdata);
}
static irqreturn_t mma8x5x_interrupt(int irq, void *dev)
{
	struct mma8x5x_data *pdata = (struct mma8x5x_data *)dev;

	if(2==bFirstINT) {
		_mma8x5x_func_int_check(pdata);
		printk("skipped first interrupt !\n");
		bFirstINT = 0;
		return IRQ_HANDLED;
	}

	pdata->int_jiffies=jiffies;
	++pdata->iInterruptPending ;
	
	if(delayed_work_pending(&pdata->func_int_delay_work)) {
		cancel_delayed_work_sync(&pdata->func_int_delay_work);
	}
	schedule_delayed_work(&pdata->func_int_delay_work, HZ/20);
	//schedule_delayed_work(&pdata->func_int_delay_work, 0);
	return IRQ_HANDLED;
}
#else //][!FUNC_INT_DELAY_WORK
static irqreturn_t mma8x5x_interrupt(int irq, void *dev)
{
	//printk("[%s-%d] mma8652 interrupt(%d) received\n",__func__,__LINE__,irq);
	struct mma8x5x_data *pdata = (struct mma8x5x_data *)dev;

	_mma8x5x_func_int_check(pdata);

	if(2==bFirstINT) {
		printk("skipped first interrupt !\n");
		bFirstINT = 0;
		return IRQ_HANDLED;
	}

	pdata->int_jiffies=jiffies;

	++pdata->iInterruptPending ;


	return IRQ_HANDLED;
}
#endif //] FUNC_INT_DELAY_WORK


static int mma8x5x_data_convert(struct mma8x5x_data *pdata,
		struct mma8x5x_data_axis *axis_data)
{
	short rawdata[3], data[3];
	int i, j;
	int position = pdata->position;

	if (position < 0 || position > 7)
		position = 0;
	rawdata[0] = axis_data->x;
	rawdata[1] = axis_data->y;
	rawdata[2] = axis_data->z;
	for (i = 0; i < 3; i++) {
		data[i] = 0;
		for (j = 0; j < 3; j++)
			data[i] += rawdata[j] * mma8x5x_position_setting[position][i][j];
	}
	axis_data->x = data[0];
	axis_data->y = data[1];
	axis_data->z = data[2];
	return 0;
}
static int mma8x5x_check_id(int id)
{
	int i = 0;

	for (i = 0; i < sizeof(mma8x5x_chip_id) / sizeof(mma8x5x_chip_id[0]); i++)
		if (id == mma8x5x_chip_id[i])
		{
			printk(KERN_ERR"[%s] CHIP is :%s \n",__FUNCTION__,mma8x5x_names[i]);
			return 1;
		}
	return 0;
}
static char *mma8x5x_id2name(u8 id)
{
	return mma8x5x_names[(id >> 4) - 1];
}

static int mma8x5x_i2c_read_fifo(struct i2c_client *client,u8 reg, char * buf, int len)
{
	char send_buf[] = {reg};
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = send_buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
		printk(KERN_ERR "mma8x5x: transfer error\n");
		return -EIO;
	} else
		return len;
}


/*period is ms, return the real period per event*/
static s64 mma8x5x_odr_set(struct i2c_client * client, int period){
	u8 odr;
	u8 val;
	s64 period_rel;
	val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
	i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1,val &(~0x01));  //standby
	val &= ~(0x07 << 3);
	if(period >= 640){							/*1.56HZ*/
		odr = 0x7;
		period_rel = 640 * NSEC_PER_MSEC;
	}
	else if(period >= 160){						/*6.25HZ*/
		odr = 0x06;
		period_rel = 160 * NSEC_PER_MSEC;
	}
	else if(period >= 80){						/*12.5HZ*/
		odr = 0x05;
		period_rel = 80 * NSEC_PER_MSEC;
	}else if(period >= 20){						/*50HZ*/
		odr = 0x04;
		period_rel = 20 * NSEC_PER_MSEC;
	}else if(period >= 10){						/*100HZ*/
		odr = 0x03;
		period_rel = 10 * NSEC_PER_MSEC;
	}else if(period >= 5){						/*200HZ*/
		odr = 0x02;
		period_rel = 5 * NSEC_PER_MSEC;
	}else if((period * 2) >= 5){ 				/*400HZ*/
		odr = 0x01;
		period_rel = 2500 * NSEC_PER_USEC;
	}else{                /*800HZ*/
		odr = 0x00;
		period_rel = 1250 * NSEC_PER_USEC;
	}
	val |= (odr << 3);
	i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1,val);  //standby
	return period_rel;
}
static int mma8x5x_device_init(struct i2c_client *client,bool init)
{
	int result,val;
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);

	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, 0);
	if (result < 0)
		goto out;
	
	// Trigger Configuration (0x10: Landscape/Portrait/Orientation trigger)
	result = i2c_smbus_write_byte_data(client, MMA8X5X_TRIG_CFG, 0x10);
	// ODR , 400 MHz
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, 0x08);	
	// Enable , auto-sleep , ACTIVE_MODE: low power :0x1f , Disable auto-sleep :  0x1B 
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG2, 0x1f);

	if(mma8x5x_z_failed==0)
	{
		printk(KERN_ERR"-----[%s] Disable Transient Interrupt \n",__FUNCTION__);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG3, NORMAL_DEFAULT_INTCTRL);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG4, NORMAL_DEFAULT_INTEN);
	}
	else{
		printk(KERN_ERR"-----[%s] Enable Transient Interrupt \n",__FUNCTION__); // Can't detect mma8652 in device_init(), so must enable motion interrupr first.
		// Wake from Orientation interrupt / Transient interrupt + Freefall/Motion interrupt : 0x68 ,
		result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG3, ZSTICK_DEFAULT_INTCTRL);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG4, ZSTICK_DEFAULT_INTEN);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_FF_MT_CFG, ZSTICK_DEFAULT_FF_MT_CFG);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_FF_MT_THS, ZSTICK_DEFAULT_FF_MT_THS);	
		// Transient_CFG register, 0x70~0x00
		result = i2c_smbus_write_byte_data(client, MMA8X5X_TRANSIENT_CFG, 0x02);
		// TRANSIENT_THS register, 0x70~0x00	
		result = i2c_smbus_write_byte_data(client, MMA8X5X_TRANSIENT_THS, 0x03);
	}
	
	if(init==true)
	{
		// Portrait/Landscape Configuration register
		p_mma8x5x_data->pl_cfg = 0x00;	// 0xc0 : Enable Portait/Landscape , 0x00: Default
		// Auto-WAKE/SLEEP Detection register
		p_mma8x5x_data->aslp_count = 0x20 ;
		result = i2c_smbus_write_byte_data(client, MMA8X5X_PL_CFG, p_mma8x5x_data->pl_cfg);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_ASLP_COUNT, p_mma8x5x_data->aslp_count);
		// Read default
		p_mma8x5x_data->pl_count = i2c_smbus_read_byte_data(client,MMA8X5X_PL_COUNT);
		p_mma8x5x_data->p_l_ths_reg = i2c_smbus_read_byte_data(client,MMA8X5X_P_L_THS_REG);
		p_mma8x5x_data->pl_bf_zcomp = i2c_smbus_read_byte_data(client,MMA8X5X_PL_BF_ZCOMP);

		printk(KERN_ERR "Default , pl_cfg:%x , pl_count:%x , p_l_ths_reg:%x , pl_bf_zcomp:%x , aslp_count:%x \n",
								p_mma8x5x_data->pl_cfg,p_mma8x5x_data->pl_count
								,p_mma8x5x_data->p_l_ths_reg,p_mma8x5x_data->pl_bf_zcomp
								,p_mma8x5x_data->aslp_count);
	}
	else{
		result = i2c_smbus_write_byte_data(client, MMA8X5X_PL_CFG, p_mma8x5x_data->pl_cfg);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_PL_COUNT, p_mma8x5x_data->pl_count);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_P_L_THS_REG, p_mma8x5x_data->p_l_ths_reg);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_PL_BF_ZCOMP, p_mma8x5x_data->pl_bf_zcomp);
		result = i2c_smbus_write_byte_data(client, MMA8X5X_ASLP_COUNT, p_mma8x5x_data->aslp_count);	
	}

	// END 
	val = i2c_smbus_read_byte_data(client,MMA8X5X_CTRL_REG1);
	result = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val|0x01);  
	if(!result){
		pdata->active = MMA_ACTIVED;
	}	
	
	//pdata->active = MMA_STANDBY;	
	msleep(MODE_CHANGE_DELAY_MS);	
	return 0;
out:
	dev_err(&client->dev, "error when init mma8x5x:(%d)", result);
	return result;
}
static int mma8x5x_device_stop(struct i2c_client *client)
{
	u8 val;
	val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
	i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val & 0xfe);
	bFirstINT=1;
	return 0;
}

static int mma8x5x_device_start(struct i2c_client *client)
{
	u8 val;
	val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
	i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val | 0x01);
	if(1==bFirstINT && p_mma8x5x_data->pl_cfg ) {
		// assumming the TRIG_CFG is 0x10 and if pl interrupt will be triggerred if pl_cfg is set , then we should wait the first PL interrupt signal . 
		bFirstINT=2;
	}
	return 0;
}

static int mma8x5x_read_data(struct i2c_client *client,
		struct mma8x5x_data_axis *data)
{
	u8 tmp_data[MMA8X5X_BUF_SIZE];
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client,
					    MMA8X5X_OUT_X_MSB,
						MMA8X5X_BUF_SIZE, tmp_data);
	if (ret < MMA8X5X_BUF_SIZE) {
		dev_err(&client->dev, "i2c block read failed\n");
		return -EIO;
	}
	data->x = ((tmp_data[0] << 8) & 0xff00) | tmp_data[1];
	data->y = ((tmp_data[2] << 8) & 0xff00) | tmp_data[3];
	data->z = ((tmp_data[4] << 8) & 0xff00) | tmp_data[5];
	return 0;
}
static int mma8x5x_fifo_interrupt(struct i2c_client *client,int enable)
{
	u8 val,sys_mode;
	sys_mode =  i2c_smbus_read_byte_data(client,MMA8X5X_CTRL_REG1);
	i2c_smbus_write_byte_data(client,MMA8X5X_CTRL_REG1,(sys_mode & (~0x01))); //standby
	val = i2c_smbus_read_byte_data(client,MMA8X5X_CTRL_REG4);
	val &= ~(0x01 << 6);
	if(enable)
		val |= (0x01 << 6);
	i2c_smbus_write_byte_data(client,MMA8X5X_CTRL_REG4,val);
	i2c_smbus_write_byte_data(client,MMA8X5X_CTRL_REG1,sys_mode);
	return 0;
}
static int mma8x5x_fifo_setting(struct mma8x5x_data *pdata,int time_out,int is_overwrite)
{
	u8 val,sys_mode,pin_cfg;
	struct i2c_client *client = pdata->client;
	sys_mode =  i2c_smbus_read_byte_data(client,MMA8X5X_CTRL_REG1);
	i2c_smbus_write_byte_data(client,MMA8X5X_CTRL_REG1,(sys_mode & (~0x01))); //standby
	pin_cfg = i2c_smbus_read_byte_data(client,MMA8X5X_CTRL_REG5);
	val = i2c_smbus_read_byte_data(client,MMA8X5X_F_SETUP);
	val &= ~(0x03 << 6);
	if(time_out > 0){
		if(is_overwrite)
			val |= (0x01 << 6);
		else
			val |= (0x02 << 6);
	}
	i2c_smbus_write_byte_data(client,MMA8X5X_F_SETUP,val);
	i2c_smbus_write_byte_data(client,MMA8X5X_CTRL_REG5,pin_cfg |(0x01 << 6));//route to pin 1
	i2c_smbus_write_byte_data(client,MMA8X5X_CTRL_REG1,sys_mode);
	if(time_out > 0){
		 pdata->period_rel = mma8x5x_odr_set(client,time_out/32);  //fifo len is 32
	}
	return 0;
}
static int mma8x5x_read_fifo_data(struct mma8x5x_data *pdata){
	int count,cnt;
	u8 buf[256],val;
	int i,index;
	struct i2c_client *client = pdata->client;
	struct mma8x5x_fifo *pfifo = &pdata->fifo;
	struct timespec ts;
	val = i2c_smbus_read_byte_data(client,MMA8X5X_STATUS);
	if(val & (0x01 << 7)) //fifo overflow
	{
		cnt = (val & 0x3f);
		count = mma8x5x_i2c_read_fifo(client,MMA8X5X_OUT_X_MSB,buf,MMA8X5X_BUF_SIZE * cnt);
		if(count > 0){
			ktime_get_ts(&ts);
			for(i = 0; i < count/MMA8X5X_BUF_SIZE ;i++){
				index = MMA8X5X_BUF_SIZE * i;
				pfifo->fifo_data[i].x = ((buf[index] << 8) & 0xff00) | buf[index + 1];
				pfifo->fifo_data[i].y = ((buf[index + 2] << 8) & 0xff00) | buf[index + 3];
				pfifo->fifo_data[i].z = ((buf[index + 4] << 8) & 0xff00) | buf[index + 5];
				mma8x5x_data_convert(pdata, &pfifo->fifo_data[i]);
			}
			pfifo->period = pdata->period_rel;
			pfifo->count = count / MMA8X5X_BUF_SIZE;
			pfifo->timestamp = ((s64)ts.tv_sec) * NSEC_PER_SEC  + ts.tv_nsec;
			return 0;
		}
	}
	return -1;
}

static void mma8x5x_report_data(struct mma8x5x_data *pdata,struct mma8x5x_data_axis *data)
{
	input_report_abs(pdata->idev, ABS_X, data->x);
	input_report_abs(pdata->idev, ABS_Y, data->y);
	input_report_abs(pdata->idev, ABS_Z, data->z);
	input_sync(pdata->idev);
}
static void mma8x5x_work(struct mma8x5x_data * pdata){
	int delay;
	if(pdata->active == MMA_ACTIVED){
		delay = msecs_to_jiffies(pdata->delay);
		if (delay >= HZ)
			delay = round_jiffies_relative(delay);
	
		if(DetectMMA8x5x(pdata))
			printk("====== [MMA8X5X is failed] %s ======\n",__FUNCTION__);
		schedule_delayed_work(&pdata->work, delay);
	}
}


static int GetPLStatus(struct mma8x5x_data_axis *data)
{
	//Initialization:
	int timeout_threshold = 1;				//TIMEOUT_1_SEC; 	// 1 second time_out for determine a stuck status
	int PL_debounce_counter_threshold = 1;	//TIMEOUT_1_SEC; // 1 second time_out for determine a P/L change status
	int PL_status = 0 ; 					// P/L previous status is none
	int X = 0 ;
	int Y = 0 ;
	int Z = 0 ;

 	//While(1)
	{
		X = data->x ;
		Y = data->y ;
		Z = data->z ;

		if(mma8x5x_stick_mode)
			Z = -32768 ;

		//printk("------X_MSB:%x , X_LSB:%x  \n",i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_OUT_X_MSB),i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_OUT_Z_LSB));
		//printk("------Y_MSB:%x , Y_LSB:%x  \n",i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_OUT_Y_MSB),i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_OUT_Z_LSB));
		//printk("------Z_MSB:%x , Z_LSB:%x  \n",i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_OUT_Z_MSB),i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_OUT_Z_LSB));
		//printk(KERN_ERR"--- flag_stuck_status:%d  , X:%d , Y:%d , Z:%d_%d \n",flag_stuck_status,X,Y,Z,Z_previous_value);

		// Detect Z stiction
		// check if Z remains the same previous value , suppose normally there are added noises on Z value and Z value various around certain level
		if ( Z == Z_previous_value ) 
		{
			// determine Z stuck if it stays at same value longer than certain time (e.g. 1 second) or last status is stuck already
			if (timeout_counter >= timeout_threshold || flag_stuck_status == STUCK ) 
			{ 
				Z = int_sqrt( 32768*32768-X*X -Y*Y ); 
				flag_stuck_status = STUCK; 
			} // replace Z with calculation from XY and 1 g. set stuck status
       		else { 
				timeout_counter ++ ;
			}
		}
		else
		{
			Z_previous_value = Z; 
			timeout_counter =0; 
			flag_stuck_status = NON_STUCK; 
		} // reset timeout_counter and status flag, 

		// manually detect portrait up and portrait down, use 30 degree as Z lock out angle
		if (flag_stuck_status == STUCK)
		{
			if( (abs(Z) < CONST_1024_0_886) && (abs(X) > abs(Y) ) && (X > 0) )
			{
				if( (PortraitUp_counter >=  PL_debounce_counter_threshold) &&   PORTRAIT_UP == PL_temp_status ) { 
					PL_status = PORTRAIT_UP; 
				} 
        		else { 
					PortraitUp_counter =PortraitUp_counter +1; 
					PortraitDn_counter =0; 
					LandscapeL_counter =0; 
					LandscapeR_counter =0; 
				}
        		PL_temp_status =PORTRAIT_UP;
			}
			else if ( ( abs(Z) < CONST_1024_0_886) && (abs(X) > abs(Y) ) && (X < 0) ) 
			{
				if( (PortraitDn_counter >=  PL_debounce_counter_threshold) && PL_temp_status == PORTRAIT_DN) {
					PL_status = PORTRAIT_DN;
				} 
        		else { 
					PortraitDn_counter ++;
					PortraitUp_counter =0; 
					LandscapeL_counter =0; 
					LandscapeR_counter =0; 
				}
        		PL_temp_status =PORTRAIT_DN;
       		}
       		else if ( ( abs(Z) < CONST_1024_0_886) && (abs(X) < abs(Y) ) && (Y > 0) ) 
			{
				if( (LandscapeL_counter >=  PL_debounce_counter_threshold) && PL_temp_status == LANDSCAPE_LEFT) { 
					PL_status = LANDSCAPE_LEFT; 
				} 
            	else { 
					LandscapeL_counter ++; 
					PortraitUp_counter =0; 
					PortraitDn_counter =0; 
					LandscapeR_counter =0; 
					}
              	PL_temp_status =LANDSCAPE_LEFT;
       		}
       		else if ( ( abs(Z) < CONST_1024_0_886) && (abs(X) < abs(Y) ) && (Y < 0) ) 
			{
				if( (LandscapeR_counter >=  PL_debounce_counter_threshold) && PL_temp_status == LANDSCAPE_RIGHT) {
					PL_status = LANDSCAPE_RIGHT; 
				} 
              	else { 
					  LandscapeR_counter ++;
					  PortraitUp_counter =0;
					  PortraitDn_counter =0;
					  LandscapeL_counter =0;
				}
              	PL_temp_status =LANDSCAPE_RIGHT;
       		}
       		else { 
				   PL_temp_status = PORTRAIT_NONE; 
				   PortraitUp_counter =0; 
				   PortraitDn_counter =0;
				   LandscapeL_counter =0;
				   LandscapeR_counter =0;

			}
		}
	}
	
	return PL_status;
}


static void mma8x5x_dev_poll(struct work_struct *work)
{
	struct mma8x5x_data *pdata = container_of(work, struct mma8x5x_data,work.work);
	int pl_status = 0 ;
	struct mma8x5x_data_axis data;
	int ret;
	
	ret = mma8x5x_read_data(pdata->client, &data);
	if(!ret){
		mma8x5x_data_convert(pdata, &data);
	}
	else {
		printk(KERN_ERR"mma8x5x_read_data error !\n");
		return ;
	}

	//printk("%s : (%d,%d,%d)\n",__FUNCTION__,data.x,data.y,data.z);
	if(mma8x5x_z_failed || mma8x5x_stick_mode)
	{
		pl_status = GetPLStatus(&data);
		
		if(PL_temp_event != pl_status && pl_status != PORTRAIT_NONE)	
		{
			//printk("pl_status change : %d->%d\n",PL_temp_event,pl_status);
			send_jiffies = jiffies + JIFFIES_INTERVAL ;	
			PL_Send_temp_event = pl_status ;
			PL_Send_Event = true ;
		}

		if(jiffies > send_jiffies && send_jiffies != 0 && PL_Send_Event == true )
		{
			switch(PL_Send_temp_event)
			{
				case PORTRAIT_UP:	
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_DOWN);
					printk("send : PORTRAIT_DN\n");
					gPL_Status = PORTRAIT_UP ;
					break;
				case PORTRAIT_DN:	
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_UP);
					printk("send : PORTRAIT_UP\n");
					gPL_Status = PORTRAIT_DN;
					break;
				case LANDSCAPE_LEFT:	
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_RIGHT);	
					printk("send : LANDSCAPE_RIGHT\n");
					gPL_Status = LANDSCAPE_LEFT;
					break;
				case LANDSCAPE_RIGHT:		
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_LEFT);	
					printk("send : LANDSCAPE_LEFT\n");
					gPL_Status = LANDSCAPE_RIGHT;
					break;
				default:
					gPL_Status = PORTRAIT_NONE ;
					printk("send : pl_status NONE\n");
					break;
			}
			PL_Send_Event = false;
		}
		PL_temp_event = pl_status ;
	}
	
	mma8x5x_report_data(pdata,&data);
	mma8x5x_work(pdata);
}

static irqreturn_t mma8x5x_irq_handler(int irq, void *dev)
{
	int ret;
	u8 int_src;
	struct mma8x5x_data *pdata = (struct mma8x5x_data *)dev;
	int_src = i2c_smbus_read_byte_data(pdata->client,MMA8X5X_INT_SOURCE);
	//printk("%s(),%s\n",__FUNCTION__,_int_srcs_strings(int_src));
	if(int_src & (0x01 << 6)){
		ret = mma8x5x_read_fifo_data(pdata);
		if(!ret){
			atomic_set(&pdata->fifo_ready, 1);
			wake_up(&pdata->fifo_wq);
		}
		if(pdata->awaken) /*is just awken from suspend*/
		{
			mma8x5x_fifo_setting(pdata,pdata->fifo_timeout,0); //10s timeout
			mma8x5x_fifo_interrupt(pdata->client,1);
			pdata->awaken = 0;
		}
	}
	return IRQ_HANDLED;
}


static ssize_t mma8x5x_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata = dev_get_drvdata(dev);
	struct i2c_client *client = pdata->client;
	u8 val;
	int enable;

	mutex_lock(&pdata->data_lock);
	val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
	if ((val & 0x01) && pdata->active == MMA_ACTIVED)
		enable = 1;
	else
		enable = 0;
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "%d\n", enable);
}

static ssize_t mma8x5x_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mma8x5x_data *pdata = dev_get_drvdata(dev);
	struct i2c_client *client = pdata->client;
	int ret;
	unsigned long enable;
	u8 val = 0;

	enable = simple_strtoul(buf, NULL, 10);
	mutex_lock(&pdata->data_lock);
	enable = (enable > 0) ? 1 : 0;
	if (enable && pdata->active == MMA_STANDBY) {
		val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
		ret = i2c_smbus_write_byte_data(client,MMA8X5X_CTRL_REG1, val | 0x01);
		if (!ret) {
			pdata->active = MMA_ACTIVED;
			if(pdata->fifo_timeout <= 0) //continuous mode
				mma8x5x_work(pdata);
			else{       /*fifo mode*/
				mma8x5x_fifo_setting(pdata,pdata->fifo_timeout,0); //no overwirte fifo
				mma8x5x_fifo_interrupt(client,1);
			}
			printk(KERN_INFO"mma enable setting active \n");
		}
	} else if (enable == 0  && pdata->active == MMA_ACTIVED) {
		val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
		ret = i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val & 0xFE);
		if (!ret) {
			pdata->active = MMA_STANDBY;
			if(pdata->fifo_timeout <= 0) //continuous mode
				cancel_delayed_work_sync(&pdata->work);
			else{ 						/*fifo mode*/
				mma8x5x_fifo_setting(pdata,0,0); //no overwirte fifo
				mma8x5x_fifo_interrupt(client,0);
			}
			printk(KERN_INFO"mma enable setting inactive \n");
		}
	}
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata = dev_get_drvdata(dev);
	int delay;
	mutex_lock(&pdata->data_lock);
	delay  = pdata->delay;
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "%d\n", delay);
}

static ssize_t mma8x5x_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mma8x5x_data *pdata = dev_get_drvdata(dev);
	struct i2c_client * client = pdata->client;
	int delay;
	delay = simple_strtoul(buf, NULL, 10);
	mutex_lock(&pdata->data_lock);
	cancel_delayed_work_sync(&pdata->work);
	pdata->delay = delay;
	if(pdata->active == MMA_ACTIVED && pdata->fifo_timeout <= 0){
		mma8x5x_odr_set(client,delay);
		mma8x5x_work(pdata);
	}
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_fifo_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct mma8x5x_data *pdata = dev_get_drvdata(dev);
	mutex_lock(&pdata->data_lock);
	count = sprintf(&buf[count], "period poll  :%d  ms\n", pdata->delay);
	count += sprintf(&buf[count],"period fifo  :%lld  ns\n",pdata->period_rel);
	count += sprintf(&buf[count],"timeout :%d ms\n", pdata->fifo_timeout);
	count += sprintf(&buf[count],"interrupt wake up: %s\n", (pdata->fifo_wakeup ? "yes" : "no"));  /*is the interrupt enable*/ 
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_fifo_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mma8x5x_data *pdata = dev_get_drvdata(dev);
	struct i2c_client * client = pdata->client;
	int period,timeout,wakeup;
	sscanf(buf,"%d,%d,%d",&period,&timeout,&wakeup);
	printk("period %d ,timeout is %d, wake up is :%d\n",period,timeout,wakeup);
	if(timeout > 0){
		mutex_lock(&pdata->data_lock);
		cancel_delayed_work_sync(&pdata->work);
		pdata->delay = period;
		mutex_unlock(&pdata->data_lock);
		mma8x5x_fifo_setting(pdata,timeout,0); //no overwirte fifo
		mma8x5x_fifo_interrupt(client,1);
		pdata->fifo_timeout = timeout;
		pdata->fifo_wakeup = wakeup;
	}else{
		mma8x5x_fifo_setting(pdata,timeout,0); //no overwirte fifo
		mma8x5x_fifo_interrupt(client,0);
		pdata->fifo_timeout = timeout;
		pdata->fifo_wakeup = wakeup;
		mutex_lock(&pdata->data_lock);
		pdata->delay = period;
		if(pdata->active == MMA_ACTIVED)
			mma8x5x_work(pdata);
		mutex_unlock(&pdata->data_lock);
	}
	return count;
}

static ssize_t mma8x5x_position_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int position = 0;
	mutex_lock(&pdata->data_lock);
	position = pdata->position;
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "%d\n", position);
}

static ssize_t mma8x5x_position_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int position;
	position = simple_strtoul(buf, NULL, 10);
	mutex_lock(&pdata->data_lock);
	pdata->position = position;
	mutex_unlock(&pdata->data_lock);
	return count;
}



#ifdef MMA8X5X_DUMP_REGS_SYSFS //[
static ssize_t mma8x5x_regs_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int count=0;
	u8 bVal;

	int i;
	int iLen;

	if(!pdata) {
		printk(KERN_ERR"%s() driver data not ready !!\n",__FUNCTION__);
		return 0;
	}

	for(i=0;i<sizeof(dump_regs)/sizeof(dump_regs[0]);i++) {
		bVal = i2c_smbus_read_byte_data(pdata->client, dump_regs[i].addr);
		iLen = sprintf(buf,"%s(%02Xh)=0x%x\n",dump_regs[i].name,dump_regs[i].addr,bVal);
		buf+=iLen;
		count+=iLen;
	}
	
	return count;
}

static ssize_t mma8x5x_regs_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	//char cRegNameA[128];
	u8 bRegVal;
	u8 bRegAddr = 0xff;
	int iLen;
	int i;
	char *pch;
	int iChk;

	iLen = strlen(buf);

	if(!pdata) {
		printk(KERN_ERR"%s() driver data not ready !!\n",__FUNCTION__);
		return iLen;
	}

	pch = strchr(buf,'=');
	if(!pch) {
		printk(KERN_ERR"%s() wrong format(=) !! MMA8X5X_XXX_XXX=0xXX\n",__FUNCTION__);
		return iLen;
	}

	*pch = '\0';
	
	if( !(*(pch+1)=='0' && *(pch+2)=='x') ) {
		printk(KERN_ERR"%s() wrong format(0x) !! MMA8X5X_XXX_XXX=0xXX\n",__FUNCTION__);
		return iLen;
	}

	bRegVal =(unsigned char) simple_strtoul(pch+3, NULL, 16);
	
	for(i=0;i<sizeof(dump_regs)/sizeof(dump_regs[0]);i++) {
		if(0==strcmp(dump_regs[i].name,buf)) {
			bRegAddr = dump_regs[i].addr;
			break;
		}
	}

	if(0xff != bRegAddr) {
		iChk = i2c_smbus_write_byte_data(pdata->client, bRegAddr , bRegVal );
	}
	else {
		printk(KERN_ERR"reg %s not found\n",buf);
	}
	printk("reg %s (%02Xh)<= 0x%x,chk(%d)\n",buf,bRegAddr,bRegVal,iChk);

	return iLen;
}
#endif //] MMA8X5X_DUMP_REGS_SYSFS


static ssize_t mma8x5x_pl_status_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];

	if(mma8x5x_z_failed==0  && mma8x5x_stick_mode ==0)
	{
		mutex_lock(&pdata->data_lock);
		tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_STATUS);
		printk(KERN_INFO"[PL_STATUS]: 0x%x \n",tmp_data[0] );
		if(  (tmp_data[0]& 0x06) ==0){
			printk(KERN_INFO"[PL_STATUS]: Portrait Down \n");
		}
		else if( (tmp_data[0] & 0x06 ) ==2)	{
			printk(KERN_INFO"[PL_STATUS]: Portrait Up \n");
		}
		else if( (tmp_data[0] & 0x06 ) ==4)	{
			printk(KERN_INFO"[PL_STATUS]: Landscape Right \n");
		}
		else if( (tmp_data[0] & 0x06 ) ==6)	{
			printk(KERN_INFO"[PL_STATUS]: Landscape Left \n");
		}
		if( (tmp_data[0] & 0x01 ) ==0)	{
			printk(KERN_INFO"[PL_STATUS]: Back \n");
		}
		else if( (tmp_data[0] & 0x01 ) ==1)	{
			printk(KERN_INFO"[PL_STATUS]: Front \n");		
		}	
	}
	else
	{
		tmp_data[0] = 0 ;
		switch(gPL_Status)
		{
			case PORTRAIT_UP:	
				if(75 == gptHWCFG->m_val.bPCB) // E80K02
				{
					tmp_data[0] = 0x02 ;
					printk("[PL_STATUS_STICK]: PORTRAIT_UP\n");	
				}		
				break;
			case PORTRAIT_DN:	
				if(75 == gptHWCFG->m_val.bPCB) // E80K02
				{
					tmp_data[0] = 0x00 ;
					printk("[PL_STATUS_STICK]: PORTRAIT_DN\n");
				}
				break;
			case LANDSCAPE_LEFT:	
				if(75 == gptHWCFG->m_val.bPCB) // E80K02
				{
					tmp_data[0] = 0x06 ;
					printk("[PL_STATUS_STICK]: LANDSCAPE_LEFT\n");
				}
				break;			
			case LANDSCAPE_RIGHT:
				if(75 == gptHWCFG->m_val.bPCB) // E80K02
				{
					tmp_data[0] = 0x04 ;
					printk("[PL_STATUS_STICK]: LANDSCAPE_RIGHT\n");	
				}
				break;
			default:
				printk("[PL_STAUTS_STICK] : pl_status NONE\n");
				break;
		}
	}
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "0x%02x\n", tmp_data[0]);
}


static ssize_t mma8x5x_pl_status_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	//struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	// read_only
	return 0;
}

enum {
	EBRMAIN_UNKOWN=0,
	EBRMAIN_UP = 1,
	EBRMAIN_DWON = 2,
	EBRMAIN_RIGHT = 3,
	EBRMAIN_LEFT = 4
};

// ONLY FOR EBRMAIN
static ssize_t mma8x5x_pl_status_AP_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	u8 res = 0;

	if(mma8x5x_z_failed==0 && mma8x5x_stick_mode ==0)
	{
		mutex_lock(&pdata->data_lock);
		tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_STATUS);
		printk(KERN_INFO"-----PL_STATUS : 0x%x \n",tmp_data[0] );
		if(  (tmp_data[0]& 0x06) ==0){
			if(80 == gptHWCFG->m_val.bPCB) { // E60QU4
				printk(KERN_INFO"-----PL_STATUS : Landscape Left \n");
				res = EBRMAIN_LEFT ;
			}
			else if(75 == gptHWCFG->m_val.bPCB) { // E80K02
				printk(KERN_INFO"-----PL_STATUS : Portrait Down \n");		
				res = EBRMAIN_DWON ;
			}
		}
		else if( (tmp_data[0] & 0x06 ) ==2)	{
			if(80 == gptHWCFG->m_val.bPCB) { // E60QU4
				printk(KERN_INFO"-----PL_STATUS : Landscape Right \n");
				res = EBRMAIN_RIGHT ;
			}
			else if(75 == gptHWCFG->m_val.bPCB) { // E80K02
				printk(KERN_INFO"-----PL_STATUS : Portrait Up \n");
				res = EBRMAIN_UP ;
			}	
		}
		else if( (tmp_data[0] & 0x06 ) ==4)	{
			if(80 == gptHWCFG->m_val.bPCB) { // E60QU4
				printk(KERN_INFO"-----PL_STATUS : Portrait Down \n");
				res = EBRMAIN_DWON ;
			}
			else if(75 == gptHWCFG->m_val.bPCB) { // E80K02
				printk(KERN_INFO"-----PL_STATUS : Landscape Right \n");	
				res = EBRMAIN_RIGHT ;
			}
		}
		else if( (tmp_data[0] & 0x06 ) ==6)	{
			if(80 == gptHWCFG->m_val.bPCB) { // E60QU4
				printk(KERN_INFO"-----PL_STATUS : Portrait Up \n");
				res = EBRMAIN_UP ;
			}
			else if(75 == gptHWCFG->m_val.bPCB) { // E80K02
				printk(KERN_INFO"-----PL_STATUS : Landscape Left \n");
				res = EBRMAIN_RIGHT ;
			}
		}
		if( (tmp_data[0] & 0x01 ) ==0)	{
			printk(KERN_INFO"-----PL_STATUS : Back \n");
		}
		else if( (tmp_data[0] & 0x01 ) ==1)	{
			printk(KERN_INFO"-----PL_STATUS : Front \n");
		}
		mutex_unlock(&pdata->data_lock);
	}
	else
	{
		switch(gPL_Status)
		{
			case PORTRAIT_UP:	
				if(75 == gptHWCFG->m_val.bPCB)
				{
					res = EBRMAIN_UP ;
					printk("[PL_STATUS_STICK]: PORTRAIT_UP\n");	
				}		
				break;
			case PORTRAIT_DN:	
				if(75 == gptHWCFG->m_val.bPCB)
				{
					res = EBRMAIN_DWON ;
					printk("[PL_STATUS_STICK]: PORTRAIT_DN\n");
				}
				break;
			case LANDSCAPE_LEFT:	
				if(75 == gptHWCFG->m_val.bPCB)
				{
					res = EBRMAIN_LEFT ;
					printk("[PL_STATUS_STICK]: LANDSCAPE_LEFT\n");
				}
				break;			
			case LANDSCAPE_RIGHT:
				if(75 == gptHWCFG->m_val.bPCB)
				{
					res = EBRMAIN_RIGHT ;
					printk("[PL_STATUS_STICK]: LANDSCAPE_RIGHT\n");	
				}
				break;
			default:
				printk("[PL_STAUTS_STICK] : pl_status NONE\n");
				break;
		}		
	}
	return sprintf(buf, "%d\n", res);
}


static ssize_t mma8x5x_pl_status_AP_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	//struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	return 0;
}
static ssize_t mma8x5x_pl_cfg_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	mutex_lock(&pdata->data_lock);
	tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_CFG);
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "0x%02x\n", tmp_data[0]);
}

static ssize_t mma8x5x_pl_cfg_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int ret = 0 ;
	unsigned long val;
	char * pHead = NULL;
	mutex_lock(&pdata->data_lock);
	if(pHead = strstr(buf,"0x"))
		val = simple_strtol(buf, NULL, 16);
	else
		val = simple_strtoul(buf, NULL, 10);
	mma8x5x_device_stop(p_mma8x5x_data->client);
	p_mma8x5x_data->pl_cfg = val ;
	ret = i2c_smbus_write_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_CFG, val );
	if(ret)	{
		printk(KERN_ERR"[%s-%d] MMA8X5X_PL_CFG write error",__func__,__LINE__);
	}
	mma8x5x_device_start(p_mma8x5x_data->client);
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_pl_count_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	mutex_lock(&pdata->data_lock);
	tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_COUNT);
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "0x%02x\n", tmp_data[0]);
}

static ssize_t mma8x5x_pl_count_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int ret = 0 ;
	unsigned long val;
	char * pHead = NULL;
	mutex_lock(&pdata->data_lock);
	if(pHead = strstr(buf,"0x"))
		val = simple_strtoul(buf, NULL, 16);
	else
		val = simple_strtoul(buf, NULL, 10);

	mma8x5x_device_stop(p_mma8x5x_data->client);
	p_mma8x5x_data->pl_count = val ;	
	ret = i2c_smbus_write_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_COUNT, val );
	if(ret)	{
		printk(KERN_ERR"[%s-%d] MMA8X5X_PL_COUNT write error",__func__,__LINE__);
	}
	mma8x5x_device_start(p_mma8x5x_data->client);
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_pl_bf_zcomp_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	mutex_lock(&pdata->data_lock);
	tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_BF_ZCOMP);
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "0x%02x\n", tmp_data[0]);
}

static ssize_t mma8x5x_pl_bf_zcomp_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int ret = 0 ;
	unsigned long val;
	char * pHead = NULL;
	mutex_lock(&pdata->data_lock);
	if(pHead = strstr(buf,"0x"))
		val = simple_strtol(buf, NULL, 16);
	else
		val = simple_strtoul(buf, NULL, 10);

	mma8x5x_device_stop(p_mma8x5x_data->client);
	p_mma8x5x_data->pl_bf_zcomp = val ;	
	ret = i2c_smbus_write_byte_data(p_mma8x5x_data->client, MMA8X5X_PL_BF_ZCOMP, val );
	if(ret)	{
		printk(KERN_ERR"[%s-%d] MMA8X5X_PL_BF_ZCOMP write error",__func__,__LINE__);
	}
	mma8x5x_device_start(p_mma8x5x_data->client);
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_p_l_ths_reg_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	mutex_lock(&pdata->data_lock);
	tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_P_L_THS_REG);
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "0x%02x\n", tmp_data[0]);
}

static ssize_t mma8x5x_p_l_ths_reg_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int ret = 0 ;
	unsigned long val;
	char * pHead = NULL;
	mutex_lock(&pdata->data_lock);
	if(pHead = strstr(buf,"0x"))
		val = (int)simple_strtoul(buf, NULL, 16);
	else
		val = simple_strtoul(buf, NULL, 10);
	mma8x5x_device_stop(p_mma8x5x_data->client);
	p_mma8x5x_data->p_l_ths_reg = val;
	ret = i2c_smbus_write_byte_data(p_mma8x5x_data->client, MMA8X5X_P_L_THS_REG, val );
	if(ret)	{
		printk(KERN_ERR"[%s-%d] MMA8X5X_P_L_THS_REG write error",__func__,__LINE__);
	}
	mma8x5x_device_start(p_mma8x5x_data->client);
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_aslp_count_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	mutex_lock(&pdata->data_lock);
	tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_ASLP_COUNT);
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "0x%02x\n", tmp_data[0]);
}

static ssize_t mma8x5x_aslp_count_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int ret = 0 ;
	unsigned long val;
	char * pHead = NULL;
	mutex_lock(&pdata->data_lock);
	if(pHead = strstr(buf,"0x"))
		val = (int)simple_strtoul(buf, NULL, 16);
	else
		val = simple_strtoul(buf, NULL, 10);
	mma8x5x_device_stop(p_mma8x5x_data->client);
	p_mma8x5x_data->aslp_count = val;
	ret = i2c_smbus_write_byte_data(p_mma8x5x_data->client, MMA8X5X_ASLP_COUNT, val );
	if(ret)	{
		printk(KERN_ERR"[%s-%d] MMA8X5X_ASLP_COUNT write error",__func__,__LINE__);
	}
	mma8x5x_device_start(p_mma8x5x_data->client);
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_ff_mt_ths_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	mutex_lock(&pdata->data_lock);
	tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_FF_MT_THS);
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "0x%02x\n", tmp_data[0]);
}

static ssize_t mma8x5x_ff_mt_ths_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int ret = 0 ;
	unsigned long val;
	char * pHead = NULL;

	mutex_lock(&pdata->data_lock);
	if(pHead = strstr(buf,"0x"))
		val = (int)simple_strtoul(buf, NULL, 16);
	else
		val = simple_strtoul(buf, NULL, 10);
	mma8x5x_device_stop(p_mma8x5x_data->client);
	ret = i2c_smbus_write_byte_data(p_mma8x5x_data->client, MMA8X5X_FF_MT_THS, val );
	if(ret)	{
		printk(KERN_ERR"[%s-%d] MMA8X5X_FF_MT_THS write error",__func__,__LINE__);
	}
	mma8x5x_device_start(p_mma8x5x_data->client);
	mutex_unlock(&pdata->data_lock);
	return count;
}

static ssize_t mma8x5x_transient_ths_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	mutex_lock(&pdata->data_lock);
	tmp_data[0] = i2c_smbus_read_byte_data(p_mma8x5x_data->client, MMA8X5X_TRANSIENT_THS);
	mutex_unlock(&pdata->data_lock);
	return sprintf(buf, "0x%02x\n", tmp_data[0]);
}

static ssize_t mma8x5x_transient_ths_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	int ret = 0 ;
	unsigned long val;
	char * pHead = NULL;

	mutex_lock(&pdata->data_lock);
	if(pHead = strstr(buf,"0x"))
		val = (int)simple_strtoul(buf, NULL, 16);
	else
		val = simple_strtoul(buf, NULL, 10);
	mma8x5x_device_stop(p_mma8x5x_data->client);
	ret = i2c_smbus_write_byte_data(p_mma8x5x_data->client, MMA8X5X_TRANSIENT_THS, val );
	if(ret)	{
		printk(KERN_ERR"[%s-%d] MMA8X5X_TRANSIENT_THS write error",__func__,__LINE__);
	}
	mma8x5x_device_start(p_mma8x5x_data->client);
	mutex_unlock(&pdata->data_lock);
	return count;
}


static ssize_t mma8x5x_stick_status_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);
	u8 tmp_data[1];
	int len ;
	mutex_lock(&pdata->data_lock);
	if(mma8x5x_stick_mode==1)
		len = sprintf(buf, "Stick Mode\n");
	else{
		if(mma8x5x_z_failed)
			len = sprintf(buf, "Fail\n");
		else
			len = sprintf(buf, "Normal\n");
	}
	mutex_unlock(&pdata->data_lock);
	return len;
}

static ssize_t mma8x5x_stick_status_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct mma8x5x_data *pdata  = dev_get_drvdata(dev);

	mutex_lock(&pdata->data_lock);
	if(mma8x5x_z_failed)
	{
		printk(KERN_ERR"[MMA8X5X] Can't open/close stick mode \n");
	}
	else{
		if(strstr(buf,"stick_on")){
			mma8x5x_stick_mode = 1 ;
			mma8x5x_z_failed = 1;
			mma8x5x_device_stop(p_mma8x5x_data->client);
			mma8x5x_device_init(p_mma8x5x_data->client,false);
			mma8x5x_device_start(p_mma8x5x_data->client);
			printk(KERN_ERR"[MMA8X5X] Open Stick Mode \n");
		}
		if(strstr(buf,"stick_off")){
			mma8x5x_stick_mode = 0 ;
			printk(KERN_ERR"[MMA8X5X] Close Stick Mode \n");
		}
	}

	mutex_unlock(&pdata->data_lock);
	return count;
}


static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		   mma8x5x_enable_show, mma8x5x_enable_store);
static DEVICE_ATTR(poll_delay, S_IWUSR | S_IRUGO,
		   mma8x5x_delay_show, mma8x5x_delay_store);
#ifdef MMA8X5X_DUMP_REGS_SYSFS//[
static DEVICE_ATTR(regs, S_IWUSR | S_IRUGO,
		   mma8x5x_regs_show, mma8x5x_regs_store);
#endif //]MMA8X5X_DUMP_REGS_SYSFS

static DEVICE_ATTR(fifo, S_IWUSR | S_IRUGO,
		   mma8x5x_fifo_show, mma8x5x_fifo_store);
static DEVICE_ATTR(position, S_IWUSR | S_IRUGO,
		   mma8x5x_position_show, mma8x5x_position_store);
static DEVICE_ATTR(pl_status, S_IWUSR | S_IRUGO,
		   mma8x5x_pl_status_show, mma8x5x_pl_status_store);		   
static DEVICE_ATTR(pl_cfg, S_IWUSR | S_IRUGO,
		   mma8x5x_pl_cfg_show, mma8x5x_pl_cfg_store);
static DEVICE_ATTR(pl_count, S_IWUSR | S_IRUGO,
		   mma8x5x_pl_count_show, mma8x5x_pl_count_store);
static DEVICE_ATTR(pl_bf_zcomp, S_IWUSR | S_IRUGO,
		   mma8x5x_pl_bf_zcomp_show, mma8x5x_pl_bf_zcomp_store);
static DEVICE_ATTR(p_l_ths_reg, S_IWUSR | S_IRUGO,
		   mma8x5x_p_l_ths_reg_show, mma8x5x_p_l_ths_reg_store);
static DEVICE_ATTR(aslp_count, S_IWUSR | S_IRUGO,
		   mma8x5x_aslp_count_show, mma8x5x_aslp_count_store);
static DEVICE_ATTR(ff_mt_ths, S_IWUSR | S_IRUGO,
		   mma8x5x_ff_mt_ths_show, mma8x5x_ff_mt_ths_store);
static DEVICE_ATTR(transient, S_IWUSR | S_IRUGO,
		   mma8x5x_transient_ths_show, mma8x5x_transient_ths_store);
static DEVICE_ATTR(stick_status, S_IWUSR | S_IRUGO,
		   mma8x5x_stick_status_show, mma8x5x_stick_status_store);
static DEVICE_ATTR(pl_status_AP, S_IWUSR | S_IRUGO,
		   mma8x5x_pl_status_AP_show, mma8x5x_pl_status_AP_store);
static struct attribute *mma8x5x_attributes[] = {
	//&dev_attr_enable.attr,
	//&dev_attr_poll_delay.attr,
	//&dev_attr_fifo.attr,
	//&dev_attr_position.attr,
#ifdef MMA8X5X_DUMP_REGS_SYSFS //[
	&dev_attr_regs.attr,
#endif //]MMA8X5X_DUMP_REGS_SYSFS
	&dev_attr_pl_status.attr,	
	&dev_attr_pl_status_AP.attr,
	&dev_attr_pl_cfg.attr,
	&dev_attr_pl_count.attr,
	&dev_attr_pl_bf_zcomp.attr,
	&dev_attr_p_l_ths_reg.attr,
	&dev_attr_aslp_count.attr,
	//&dev_attr_ff_mt_ths.attr,
	//&dev_attr_transient.attr,
	&dev_attr_stick_status.attr,
	NULL
};

static const struct attribute_group mma8x5x_attr_group = {
	.attrs	= mma8x5x_attributes,
};
static int mma8x5x_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int chip_id;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -ENODEV;
	chip_id = i2c_smbus_read_byte_data(client, MMA8X5X_WHO_AM_I);
	if (!mma8x5x_check_id(chip_id))
		return -ENODEV;
	printk(KERN_INFO "check %s i2c address 0x%x \n",
			mma8x5x_id2name(chip_id), client->addr);
	strlcpy(info->type, "mma8x5x", I2C_NAME_SIZE);
	return 0;
}
static int mma8x5x_open(struct inode *inode, struct file *file){
	int err;
	err = nonseekable_open(inode, file);
	if (err)
		return err;
	file->private_data = p_mma8x5x_data;
	return 0;
}
static ssize_t mma8x5x_read(struct file *file, char __user *buf, size_t size, loff_t *ppos){
	struct mma8x5x_data *pdata = file->private_data;
	int ret = 0;
	if (!(file->f_flags & O_NONBLOCK)) {
		ret = wait_event_interruptible(pdata->fifo_wq, (atomic_read(&pdata->fifo_ready) != 0));
		if (ret)
			return ret;
	}
	if (!atomic_read(&pdata->fifo_ready))
		return -ENODEV;
	if(size < sizeof(struct mma8x5x_fifo)){
		printk(KERN_ERR "the buffer leght less than need\n");
		return -ENOMEM;
	}
	if(!copy_to_user(buf,&pdata->fifo,sizeof(struct mma8x5x_fifo))){
		atomic_set(&pdata->fifo_ready,0);
		return size;
	}
	return -ENOMEM ;
}
static unsigned int mma8x5x_poll(struct file * file, struct poll_table_struct * wait){
	struct mma8x5x_data *pdata = file->private_data;
	poll_wait(file, &pdata->fifo_wq, wait);
	if (atomic_read(&pdata->fifo_ready))
		return POLLIN | POLLRDNORM;
	return 0;
}

static const struct file_operations mma8x5x_fops = {
	.owner = THIS_MODULE,
	.open = mma8x5x_open,
	.read = mma8x5x_read,
	.poll = mma8x5x_poll,
};

static struct miscdevice mma8x5x_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mma8x5x",
	.fops = &mma8x5x_fops,
};

static int  mma8x5x_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result, chip_id;
	struct input_dev *idev;
	struct mma8x5x_data *pdata;
	struct i2c_adapter *adapter;

	adapter = to_i2c_adapter(client->dev.parent);
	result = i2c_check_functionality(adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA);
	if (!result)
		goto err_out;

	chip_id = i2c_smbus_read_byte_data(client, MMA8X5X_WHO_AM_I);

	if (!mma8x5x_check_id(chip_id)) {
		dev_err(&client->dev,
			"read chip ID 0x%x is not equal to 0x%x,0x%x,0x%x,0x%x,0x%x!\n",
			chip_id, MMA8451_ID, MMA8452_ID,
			MMA8453_ID, MMA8652_ID, MMA8653_ID);
		result = -EINVAL;
		goto err_out;
	}
	pdata = kzalloc(sizeof(struct mma8x5x_data), GFP_KERNEL);
	if (!pdata) {
		result = -ENOMEM;
		dev_err(&client->dev, "alloc data memory error!\n");
		goto err_out;
	}
	/* Initialize the MMA8X5X chip */
	memset(pdata,0,sizeof(struct mma8x5x_data));
#ifdef INTCNT_DBG //[
	pdata->last_int_jiffies = jiffies+HZ;
#endif //]INTCNT_DBG
	pdata->client = client;
	pdata->chip_id = chip_id;
	pdata->mode = MODE_2G;
	pdata->fifo_wakeup = 0;
	pdata->fifo_timeout = 0;
	//pdata->position = *(int *)client->dev.platform_data;
	pdata->position = 0 ;
	p_mma8x5x_data = pdata;
	mutex_init(&pdata->data_lock);
	i2c_set_clientdata(client, pdata);
	mma8x5x_device_init(client,true);
	idev = input_allocate_device();
	if (!idev) {
		result = -ENOMEM;
		dev_err(&client->dev, "alloc input device failed!\n");
		goto err_alloc_input_device;
	}
	idev->name = "FreescaleAccelerometer";
	idev->uniq = mma8x5x_id2name(pdata->chip_id);
	idev->id.bustype = BUS_I2C;
	idev->evbit[0] = BIT_MASK(EV_ABS);
	input_set_abs_params(idev, ABS_X, -0x7fff, 0x7fff, 0, 0);
	input_set_abs_params(idev, ABS_Y, -0x7fff, 0x7fff, 0, 0);
	input_set_abs_params(idev, ABS_Z, -0x7fff, 0x7fff, 0, 0);
	dev_set_drvdata(&idev->dev,pdata);
	pdata->idev= idev ;
	result = input_register_device(pdata->idev);
	if (result) {
		dev_err(&client->dev, "register input device failed!\n");
		goto err_register_input_device;
	}
	pdata->delay = POLL_INTERVAL;
	INIT_DELAYED_WORK(&pdata->work, mma8x5x_dev_poll);
	result = sysfs_create_group(&idev->dev.kobj, &mma8x5x_attr_group);
	if (result) {
		dev_err(&client->dev, "create device file failed!\n");
		result = -EINVAL;
		goto err_create_sysfs;
	}
	init_waitqueue_head(&pdata->fifo_wq);

	#ifdef FUNC_INT_DELAY_WORK //[
	INIT_DELAYED_WORK(&pdata->func_int_delay_work,mma8x5x_func_int_work_func);
	#endif //] FUNC_INT_DELAY_WORK


	if(client->irq){
#ifdef FUNC_INT_DELAY_WORK //[
		result= request_threaded_irq(client->irq, NULL,mma8x5x_interrupt, 
				  IRQF_TRIGGER_FALLING /*IRQF_TRIGGER_LOW */ | IRQF_ONESHOT, client->dev.driver->name, pdata);
#else //][!FUNC_INT_DELAY_WORK
		result= request_threaded_irq(client->irq, NULL, mma8x5x_interrupt, 
				  IRQF_TRIGGER_FALLING /*IRQF_TRIGGER_LOW */ | IRQF_ONESHOT, client->dev.driver->name, pdata);
#endif //] FUNC_INT_DELAY_WORK
		if (result < 0) {
			dev_err(&client->dev, "failed to register MMA8x5x irq %d!\n",
				client->irq);
			goto err_register_irq;
		}else{
			result = misc_register(&mma8x5x_dev);
			if (result) {
				dev_err(&client->dev,"register fifo device error\n");
				goto err_reigster_dev;
			}
		}
	}

	// Initial Poolling
	pdata->active = MMA_ACTIVED;
	if(pdata->fifo_timeout <= 0) //continuous mode
		mma8x5x_work(pdata);

	printk(KERN_INFO"mma8x5x device driver probe successfully\n");
	return 0;
err_reigster_dev:
	free_irq(client->irq,pdata);
err_register_irq:
	sysfs_remove_group(&idev->dev.kobj,&mma8x5x_attr_group);
err_create_sysfs:
	input_unregister_device(pdata->idev);
err_register_input_device:
	input_free_device(idev);
err_alloc_input_device:
	kfree(pdata);
err_out:
	return result;
}
static int  mma8x5x_remove(struct i2c_client *client)
{
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);
	struct input_dev *idev = pdata->idev;
	mma8x5x_device_stop(client);
	if (pdata) {
		sysfs_remove_group(&idev->dev.kobj,&mma8x5x_attr_group);
		input_unregister_device(pdata->idev);
		input_free_device(pdata->idev);
		kfree(pdata);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mma8x5x_suspend_late(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);

	if(2==bFirstINT) {
		printk(KERN_ERR"%s() : waiting for first interrupt !\n",__FUNCTION__);
		return -1;
	}

	if(pdata->iInterruptPending) {
		printk(KERN_ERR"%s() : func int pending (%d)!\n",__FUNCTION__,pdata->iInterruptPending);
		return -1;
	}

	return 0;
}

static int mma8x5x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);


	if(pdata->fifo_timeout <= 0){
		if (pdata->active == MMA_ACTIVED)
		{
				//mma8x5x_device_stop(client);
		}
	}else{
		if (pdata->active == MMA_ACTIVED){
			if (pdata->fifo_wakeup){
				mma8x5x_fifo_setting(pdata,10000,0); //10s timeout , overwrite
				mma8x5x_fifo_interrupt(client,1);
			}else{
				mma8x5x_fifo_interrupt(client,0);
				mma8x5x_fifo_setting(pdata,10000,1); //10s timeout , overwrite
			}
		}
	}


	if(DetectMMA8x5x(pdata))
		printk("====== [MMA8X5X is failed] ======\n");
	
	if (gSleep_Mode_Suspend) {
		mma8x5x_device_stop(client);
		//free_irq(p_mma8x5x_data->client->irq, MMA8X5X_NAME);
	}
	else {

		if(PL_Send_Event)
			return -1 ;

	
#if defined(ZSTICK_SUSPEND_INTCTRL) && defined(ZSTICK_SUSPEND_INTEN)
		{
			unsigned char bRegCtrlReg3,bRegCtrlReg4;
			unsigned char bRegCtrlReg3_suspending,bRegCtrlReg4_suspending;

			bRegCtrlReg3 = i2c_smbus_read_byte_data(pdata->client, MMA8X5X_CTRL_REG3);
			bRegCtrlReg4 = i2c_smbus_read_byte_data(pdata->client, MMA8X5X_CTRL_REG4);

			if(1==mma8x5x_z_failed) {
				bRegCtrlReg3_suspending = ZSTICK_SUSPEND_INTCTRL;
				bRegCtrlReg4_suspending = ZSTICK_SUSPEND_INTEN;
			}
			else {
				bRegCtrlReg3_suspending = NORMAL_SUSPEND_INTCTRL;
				bRegCtrlReg4_suspending = NORMAL_SUSPEND_INTEN;
			}

			if( (bRegCtrlReg3_suspending!=bRegCtrlReg3) || 
					(bRegCtrlReg4_suspending!=bRegCtrlReg4) ) 
			{
				mutex_lock(&pdata->data_lock);
				mma8x5x_device_stop(pdata->client);

				i2c_smbus_write_byte_data(pdata->client, MMA8X5X_CTRL_REG3, bRegCtrlReg3_suspending);
				i2c_smbus_write_byte_data(pdata->client, MMA8X5X_CTRL_REG4, bRegCtrlReg4_suspending);

				//bReg = i2c_smbus_read_byte_data(pdata->client, MMA8X5X_CTRL_REG4);
				//printk("INTEN=0x%x\n",bReg);
				mma8x5x_device_start(pdata->client);

				// waiting for first interrupt launched by stop/start, or you will be waked by your self  . 
				printk(KERN_ERR"%s() waiting for first INT...",__FUNCTION__); 
				while(2==bFirstINT) {printk(KERN_ERR".");msleep(1);}
				printk(KERN_ERR"done\n");

				mutex_unlock(&pdata->data_lock);
			}
		}
#endif //] defined(ZSTICK_SUSPEND_INTCTRL) && defined(ZSTICK_SUSPEND_INTEN)


		enable_irq_wake(p_mma8x5x_data->client->irq);
	}
	return 0;
}

static int mma8x5x_resume(struct device *dev)
{
	int val = 0 ;
	struct i2c_client *client = to_i2c_client(dev);
	struct mma8x5x_data *pdata = i2c_get_clientdata(client);

	_mma8x5x_func_int_check(pdata);

#if defined(ZSTICK_SUSPEND_INTCTRL) && defined(ZSTICK_SUSPEND_INTEN)
		{
			unsigned char bRegCtrlReg3,bRegCtrlReg4;
			unsigned char bRegCtrlReg3_normal,bRegCtrlReg4_normal;

			bRegCtrlReg3 = i2c_smbus_read_byte_data(pdata->client, MMA8X5X_CTRL_REG3);
			bRegCtrlReg4 = i2c_smbus_read_byte_data(pdata->client, MMA8X5X_CTRL_REG4);

			if(1==mma8x5x_z_failed) {
				bRegCtrlReg3_normal = ZSTICK_DEFAULT_INTCTRL;
				bRegCtrlReg4_normal = ZSTICK_DEFAULT_INTEN;
			}
			else {
				bRegCtrlReg3_normal = NORMAL_DEFAULT_INTCTRL;
				bRegCtrlReg4_normal = NORMAL_DEFAULT_INTEN;
			}

			if( (bRegCtrlReg3_normal!=bRegCtrlReg3) || 
					(bRegCtrlReg4_normal!=bRegCtrlReg4) ) 
			{
				mutex_lock(&pdata->data_lock);
				mma8x5x_device_stop(pdata->client);

				i2c_smbus_write_byte_data(pdata->client, MMA8X5X_CTRL_REG3, bRegCtrlReg3_normal);
				i2c_smbus_write_byte_data(pdata->client, MMA8X5X_CTRL_REG4, bRegCtrlReg4_normal);

				//bReg = i2c_smbus_read_byte_data(pdata->client, MMA8X5X_CTRL_REG4);
				//printk("INTEN=0x%x\n",bReg);
				mma8x5x_device_start(pdata->client);
				mutex_unlock(&pdata->data_lock);
			}
		}
#endif //] defined(ZSTICK_SUSPEND_INTCTRL) && defined(ZSTICK_SUSPEND_INTEN)

	

	if(pdata->fifo_timeout <= 0){
		if (pdata->active == MMA_ACTIVED) {
			val = i2c_smbus_read_byte_data(client, MMA8X5X_CTRL_REG1);
			i2c_smbus_write_byte_data(client, MMA8X5X_CTRL_REG1, val | 0x01);
		}
	}else{
		if (pdata->active == MMA_ACTIVED) {
			mma8x5x_fifo_interrupt(client,1);
			pdata->awaken = 1; //awake from suspend
		}
	}

	if (gSleep_Mode_Suspend) {
		mma8x5x_device_init(p_mma8x5x_data->client,false);
		//ret= request_threaded_irq(client->irq,NULL, mma8x5x_interrupt,
		//		  IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->dev.driver->name, pdata);
		//printk("request_irq returned value = %d\n", ret);
	}
	else {
		//schedule_delayed_work(&mma8652_delay_work, 0);
		disable_irq_wake(p_mma8x5x_data->client->irq);
	}	
	return 0;

}
#endif

static const struct i2c_device_id mma8x5x_id[] = {
	{ "mma8x5x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mma8x5x_id);

//static SIMPLE_DEV_PM_OPS(mma8x5x_pm_ops, mma8x5x_suspend, mma8x5x_resume);
const struct dev_pm_ops mma8x5x_pm_ops = { 
	.suspend = mma8x5x_suspend,
	.resume = mma8x5x_resume,
//	.freeze = mma8x5x_suspend,
//	.thaw = mma8x5x_resume,
//	.poweroff = mma8x5x_suspend,
//	.restore = mma8x5x_resume,
	.suspend_late = mma8x5x_suspend_late,
};
static struct i2c_driver mma8x5x_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver		= {
		.name	= "mma8x5x",
		.owner	= THIS_MODULE,
		.pm	= &mma8x5x_pm_ops,
	},
	.probe		= mma8x5x_probe,
	.remove 	= mma8x5x_remove,	
	//.remove		= __devexit_p(mma8x5x_remove),
	.id_table	= mma8x5x_id,
	.detect		= mma8x5x_detect,
	.address_list	= normal_i2c,
};

static int __init mma8x5x_init(void)
{
	/* register driver */
	int res;

	res = i2c_add_driver(&mma8x5x_driver);
	if (res < 0) {
		printk(KERN_INFO "add mma8x5x i2c driver failed\n");
		return -ENODEV;
	}
	return res;
}

static void __exit mma8x5x_exit(void)
{
	i2c_del_driver(&mma8x5x_driver);
}

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MMA8X5X 3-Axis Orientation/Motion Detection Sensor driver");
MODULE_LICENSE("GPL");

module_init(mma8x5x_init);
module_exit(mma8x5x_exit);

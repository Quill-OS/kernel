/*
 * mma7660.c
 *
 * Copyright 2009 Freescale Semiconductor Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

//#include <linux/generated/autoconf.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/input-polldev.h>
#include <linux/input.h>
#include <linux/hwmon.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>


#include <linux/platform_device.h>
#include <linux/delay.h>

#define GDEBUG 0
#include <linux/gallen_dbg.h>
#include <mach/gpio.h>
#include <mach/iomux-mx50.h>


#include <linux/timer.h>


#include "ntx_hwconfig.h"

#define MMA7660_I2C_ADDR	0x4C
#define ONEGVALUE		21
#define DEVICE_NAME		"mma7660"
#define ABS_DEVICE_NAME		"mma7660abs"
#define POLL_INTERVAL		50
#define POLL_INTERVAL_MIN	0
#define POLL_INTERVAL_MAX	500
#define RES_BUFF_LEN		160
#define ATKBD_SPECIAL		248
#define	AXIS_MAX		(ONEGVALUE/2)

#define ATKBD_SPECIAL		248
#define LSB_G			32f/1.5f

#define DRV_VERSION		"0.0.6"


//#define MMA7660_JOYDEV_ENABLE	1

#define MMA7660_TIMER_POLLING		1 // no poll input device driver use internal timer .


#if !defined(MMA7660_JOYDEV_ENABLE) && !defined(MMA7660_TIMER_POLLING) //[

	#define MMA7660ABS_POLLDEV	1

	/*
	* mma7660 GINT enable mode , if this enabled we must try to process interrupt signal in 
	* suspend/resume , open/close 
	*/ 
	#define MMA7660_GINT_MODE				1 

#endif //] !MMA7660_JOYDEV_ENABLE || !MMA7660_TIMER_POLLING


#if defined(MMA7660_JOYDEV_ENABLE) || defined(MMA7660ABS_POLLDEV) //[
	#define MMA7660_POLLDEV				1
#endif //] MMA7660_JOYDEV_ENABLE || MMA7660ABS_POLLDEV


#define TILT_POLA_LEFT	0x04
#define TILT_POLA_RIGHT	0x08
#define TILT_POLA_UP		0x18
#define TILT_POLA_DOWN	0x14


enum {
	AXIS_DIR_X	= 0,
	AXIS_DIR_X_N,
	AXIS_DIR_Y,
	AXIS_DIR_Y_N,
	AXIS_DIR_Z,
	AXIS_DIR_Z_N,
};

enum {
	SLIDER_X_UP	= 0,
	SLIDER_X_DOWN	,
	SLIDER_Y_UP	,
	SLIDER_Y_DOWN	,
};

static char *axis_dir[6] = {
	[AXIS_DIR_X] 		= "x",
	[AXIS_DIR_X_N] 		= "-x",
	[AXIS_DIR_Y]		= "y",
	[AXIS_DIR_Y_N]		= "-y",
	[AXIS_DIR_Z]		= "z",
	[AXIS_DIR_Z_N]		= "-z",
};

enum {
	REG_XOUT = 0x00,
	REG_YOUT,
	REG_ZOUT,
	REG_TILT,
	REG_SRST,
	REG_SPCNT,
	REG_INTSU,
	REG_MODE,
	REG_SR,
	REG_PDET,
	REG_PD,
	REG_END,
};

enum {
	STANDBY_MODE = 0,
	ACTIVE_MODE,
};


enum {
	INT_1L_2P = 0,
	INT_1P_2L,
	INT_1SP_2P,
};

struct mma7660_status {
	u8 mode;
	u8 ctl1;
	u8 ctl2;
	u8 axis_dir_x;
	u8 axis_dir_y;
	u8 axis_dir_z;
	u8 slider_key[4];
};

static MXC_GPIO_CFGS *gpt_mma7660_int_gpio_cfgs=0;

extern volatile NTX_HWCONFIG *gptHWCFG;
extern volatile int gSleep_Mode_Suspend;


#ifdef MMA7660_JOYDEV_ENABLE//[

static unsigned char atkbd_keycode[512] = {
	  0, 67, 65, 63, 61, 59, 60, 88,  0, 68, 66, 64, 62, 15, 41,117,
	  0, 56, 42, 93, 29, 16,  2,  0,  0,  0, 44, 31, 30, 17,  3,  0,
	  0, 46, 45, 32, 18,  5,  4, 95,  0, 57, 47, 33, 20, 19,  6,183,
	  0, 49, 48, 35, 34, 21,  7,184,  0,  0, 50, 36, 22,  8,  9,185,
	  0, 51, 37, 23, 24, 11, 10,  0,  0, 52, 53, 38, 39, 25, 12,  0,
	  0, 89, 40,  0, 26, 13,  0,  0, 58, 54, 28, 27,  0, 43,  0, 85,
	  0, 86, 91, 90, 92,  0, 14, 94,  0, 79,124, 75, 71,121,  0,  0,
	 82, 83, 80, 76, 77, 72,  1, 69, 87, 78, 81, 74, 55, 73, 70, 99,

	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	217,100,255,  0, 97,165,  0,  0,156,  0,  0,  0,  0,  0,  0,125,
	173,114,  0,113,  0,  0,  0,126,128,  0,  0,140,  0,  0,  0,127,
	159,  0,115,  0,164,  0,  0,116,158,  0,172,166,  0,  0,  0,142,
	157,  0,  0,  0,  0,  0,  0,  0,155,  0, 98,  0,  0,163,  0,  0,
	226,  0,  0,  0,  0,  0,  0,  0,  0,255, 96,  0,  0,  0,143,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,107,  0,105,102,  0,  0,112,
	110,111,108,112,106,103,  0,119,  0,118,109,  0, 99,104,119,  0,

	  0,  0,  0, 65, 99,
};
#endif //]MMA7660_JOYDEV_ENABLE



volatile static unsigned char mma7660_reg_intsu=0x03;//enable PLINT Interrupt
volatile static int giMMA7660abs_open = 0;
volatile static int giMMA7660_open = 0;


static ssize_t mma7660_show(struct device *dev,struct device_attribute *attr, char *buf);
static ssize_t mma7660_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count);
static int mma7660_suspend(struct i2c_client *client, pm_message_t state);
static int mma7660_resume(struct i2c_client *client);
#if 0
static int mma7660_open(struct input_dev *dev);
static int mma7660_close(struct input_dev *dev);
#endif 
static void mma7660_poll_work(struct work_struct *work);

static struct i2c_driver mma7660_driver;
static int mma7660_probe(struct i2c_client *client,const struct i2c_device_id *id);
static int mma7660_remove(struct i2c_client *client);
static int mma7660_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);


static const struct i2c_device_id mma7660_id[] = {
	{"mma7660", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, mma7660_id);

static struct i2c_client 	*mma7660_client;

#ifdef MMA7660_JOYDEV_ENABLE //[

static struct input_polled_dev 	*mma7660_idev;
#endif //] MMA7660_JOYDEV_ENABLE

#ifdef MMA7660ABS_POLLDEV //[

static struct input_polled_dev		*mma7660_abs_poll_idev;
#else	//][! MMA7660ABS_POLLDEV
#endif //] MMA7660ABS_POLLDEV

static struct input_dev		*mma7660_abs_dev;

//static u8			mma_open = 0;
static u8			mma_cal	= 0;

#if (GDEBUG>0) //[ debug ...
	
	static int 			debug = 1;
#else //][! release ...
	static int 			debug = 0;
#endif //]


static int 			poll_int = POLL_INTERVAL;	
static char			res_buff[RES_BUFF_LEN];

static int 			emu_joystick = 0;

static struct workqueue_struct *mma7660_workqueue;

static DECLARE_WORK(mma7660_work, mma7660_poll_work);

#undef DBG
#define DBG(format, arg...) do { if (debug) printk(KERN_DEBUG "%s: " format "\n" , __FILE__ , ## arg); } while (0)
//#define DBG //

static unsigned short normal_i2c[] = {MMA7660_I2C_ADDR,I2C_CLIENT_END};

I2C_CLIENT_INSMOD_1(mma7660);


#ifdef MMA7660_TIMER_POLLING//[

static void mma7660_timer_handler(unsigned long dwParam);

struct timer_list mma7660_timer =
	TIMER_INITIALIZER(mma7660_timer_handler,1,0);


static void mma7660_timer_handler(unsigned long dwParam) 
{
	del_timer_sync(&mma7660_timer);
	if(poll_int>0) {
		if(work_pending(&mma7660_work)) {
			WARNING_MSG("mma7660 work peding !!\n");
		}
		else {
			queue_work(mma7660_workqueue, &mma7660_work);
		}
		mod_timer(&mma7660_timer,jiffies + poll_int);
	}
	else {
		DBG_MSG("%s : mma7660 timer stoped !\n",__FUNCTION__);
	}
}

static void mma7660_timer_start(unsigned long dwParam) 
{
	DBG_MSG("%s : starting mma7660 timer \n",__FUNCTION__);
	mma7660_timer.data = dwParam;
	mod_timer(&mma7660_timer,jiffies + poll_int);
}

static void mma7660_timer_stop(void)
{
	DBG_MSG("%s : stopping mma7660 timer \n",__FUNCTION__);
	del_timer_sync(&mma7660_timer);
}

static int mma7660_timer_is_stoped(void)
{
	return timer_pending(&mma7660_timer)?0:1; 	
}

#endif //]MMA7660_TIMER_POLLING




static struct i2c_driver mma7660_driver = {
	.driver 	= {
				.name = "mma7660",
			},
	.class		= I2C_CLASS_HWMON,
	.probe		= mma7660_probe,
	.remove		= mma7660_remove,
	.id_table	= mma7660_id,
	.detect		= mma7660_detect,
//	.address_data	= &addr_data,
	.suspend 	= mma7660_suspend,
	.resume 	= mma7660_resume,
};

static struct device_attribute mma7660_dev_attr = {
	.attr 	= {
		 .name = "mma7660_ctl",
		 .mode = S_IRUGO | S_IWUGO,
	},
	.show 	= mma7660_show,
	.store 	= mma7660_store,
};

static struct mma7660_status mma_status = {
	.mode 		= 0,
	.ctl1 		= 0,
	.ctl2 		= 0,
	.axis_dir_x	= AXIS_DIR_X,
	.axis_dir_y	= AXIS_DIR_Y,
	.axis_dir_z	= AXIS_DIR_Z_N,
	.slider_key	= {0,0,0,0},
};

enum {
	MMA_REG_R = 0,		/* read register 	*/
	MMA_REG_W,		/* write register 	*/
	MMA_DUMPREG,		/* dump registers	*/
	MMA_REMAP_AXIS,		/* remap directiron of 3-axis	*/
	MMA_DUMPAXIS,		/* dump current axis mapping	*/
	MMA_ENJOYSTICK,		/* enable joystick	emulation	*/
	MMA_DISJOYSTICK,	/* disable joystick emulation	*/
	MMA_SLIDER_KEY,		/* set slider key		*/
	MMA_DUMP_SLKEY,		/* dump slider key code setting	*/
	MMA_POLL_INTERVAL,		/* change polling interval */
	MMA_CMD_MAX
};

static char *command[MMA_CMD_MAX] = {
	[MMA_REG_R] 		= "readreg",
	[MMA_REG_W] 		= "writereg",
	[MMA_DUMPREG]		= "dumpreg",
	[MMA_REMAP_AXIS]	= "remapaxis",
	[MMA_DUMPAXIS]		= "dumpaxis",
	[MMA_ENJOYSTICK]	= "enjoystick",
	[MMA_DISJOYSTICK]	= "disjoystick",
	[MMA_SLIDER_KEY]	= "sliderkey",
	[MMA_DUMP_SLKEY]	= "dumpslkey",
	[MMA_POLL_INTERVAL]	= "pollint",
};
s32 mma7660_Read_Alert(u8 RegAdd);

s32 mma7660_Read_Alert(u8 RegAdd){
	s32 res;

 	do{
		res = i2c_smbus_read_byte_data(mma7660_client, RegAdd);
		if (res < 0){
			return res;
		}
 	} while (res & 0x40);			// while Alert bit = 1;
	
	return res;
}


/* read sensor data from mma7660 */
static int mma7660_read_data(short *x, short *y, short *z, short *tilt) {
	u8	tmp_data[4];
	s32	res;

	res = (i2c_smbus_read_i2c_block_data(mma7660_client,REG_XOUT,REG_TILT-REG_XOUT,tmp_data) < REG_TILT-REG_XOUT);

	if  (res < 0){
		dev_err(&mma7660_client->dev, "i2c block read failed\n");
			return res;
	}

	*x=tmp_data[0];
	*y=tmp_data[1];
	*z=tmp_data[2];
	*tilt=tmp_data[3];

	if (*x & 0x40){
		res = mma7660_Read_Alert(REG_XOUT);
		if (res < 0){
			return res;
		}
		*x = res;
	}

	if (*y & 0x40){	
		res = mma7660_Read_Alert(REG_YOUT);
		if (res < 0){
			return res;
		}
		*y = res;
	}

	if (*z & 0x40){	
		res = mma7660_Read_Alert(REG_ZOUT);
		if (res < 0){
			return res;
		}
		*z = res;
	}

	if (*tilt * 0x40){
		res = mma7660_Read_Alert(REG_TILT);
		if (res < 0){
			return res;
		}
		*tilt = res;
	}

	if (*x & 0x20) {	/* check for the bit 5 */
		*x |= 0xffc0;
	}

	if (*y & 0x20) {	/* check for the bit 5 */
		*y |= 0xffc0;
	}

	if (*z & 0x20) {	/* check for the bit 5 */
		*z |= 0xffc0;
	}

	return 0;
}


/* parse command argument */
static void parse_arg(const char *arg, int *reg, int *value)
{
	const char *p;

	for (p = arg;; p++) {
		if (*p == ' ' || *p == '\0')
			break;
	}

	p++;

	*reg 	= simple_strtoul(arg, NULL, 16);
	*value 	= simple_strtoul(p, NULL, 16);
}

static void cmd_read_reg(const char *arg)
{
	int reg, value, ret;

	parse_arg(arg, &reg, &value);
	ret = i2c_smbus_read_byte_data(mma7660_client, reg);
	dev_info(&mma7660_client->dev, "read reg0x%x = %x\n", reg, ret);
	sprintf(res_buff,"OK:read reg 0x%02x = 0x%02x\n",reg,ret);
}

/* write register command */
static void cmd_write_reg(const char *arg)
{
	int reg, value, ret, modereg;

	parse_arg(arg, &reg, &value);

	modereg = i2c_smbus_read_byte_data(mma7660_client, REG_MODE);

	ret = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg & (~0x01));
	ret = i2c_smbus_write_byte_data(mma7660_client, reg, value);
	ret = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg);

	dev_info(&mma7660_client->dev, "write reg result %s\n",
		 ret ? "failed" : "success");
	sprintf(res_buff, "OK:write reg 0x%02x data 0x%02x result %s\n",reg,value,
		 ret ? "failed" : "success");
}

/* dump register command */
static void cmd_dumpreg(void) {
	int reg,ret;

	sprintf(res_buff,"OK:dumpreg result:\n");

	for (reg = REG_XOUT; reg <= REG_END; reg++) {
		ret = i2c_smbus_read_byte_data(mma7660_client, reg);
		sprintf(&(res_buff[strlen(res_buff)]),"%02x ",ret);
		if ((reg % 16) == 15) {
			sprintf(&(res_buff[strlen(res_buff)]),"\n");
		}
	}

	sprintf(&(res_buff[strlen(res_buff)]),"\n");
}

/* do the axis translation */

static void remap_axis_ex( 
		short *rem_x,short *rem_y,short *rem_z,u8 *rem_tilt,
		short x,short y,short z,u8 tilt)
{

	if(rem_x) {
		switch (mma_status.axis_dir_x) {
			case AXIS_DIR_X:
				*rem_x		= x;
				break;
			case AXIS_DIR_X_N:
				*rem_x		= -x;
				break;
			case AXIS_DIR_Y:
				*rem_x		= y;
				break;
			case AXIS_DIR_Y_N:
				*rem_x		= -y;
				break;
			case AXIS_DIR_Z:
				*rem_x		= z;
				break;
			case AXIS_DIR_Z_N:
				*rem_x		= -z;
				break;
		}
	}

	
	if(rem_y) {
		switch (mma_status.axis_dir_y) {
			case AXIS_DIR_X:
				*rem_y		= x;
				break;
			case AXIS_DIR_X_N:
				*rem_y		= -x;
				break;
			case AXIS_DIR_Y:
				*rem_y		= y;
				break;
			case AXIS_DIR_Y_N:
				*rem_y		= -y;
				break;
			case AXIS_DIR_Z:
				*rem_y		= z;
				break;
			case AXIS_DIR_Z_N:
				*rem_y		= -z;
				break;
		}
	}
	

	if(rem_z) {
		switch (mma_status.axis_dir_z) {
			case AXIS_DIR_X:
				*rem_z		= x;
				break;
			case AXIS_DIR_X_N:
				*rem_z		= -x;
				break;
			case AXIS_DIR_Y:
				*rem_z		= y;
				break;
			case AXIS_DIR_Y_N:
				*rem_z		= -y;
				break;
			case AXIS_DIR_Z:
				*rem_z		= z;
				break;
			case AXIS_DIR_Z_N:
				*rem_z		= -z;
				break;
		}
	}

	if(rem_tilt) {
		unsigned char bMaskBaFro,bMaskPoLa,bTilt_org,bBaFro_org,bPoLa_org;
		unsigned char bBaFro,bPoLa;

		bMaskBaFro = 0x03;// Back/Front Mask.
		bMaskPoLa = 0x1c; // Left/Right/Up/Down Mask.
		bBaFro = 0;
		bPoLa = 0;

		bTilt_org=tilt;
		bBaFro_org=bTilt_org&bMaskBaFro;
		bPoLa_org=bTilt_org&bMaskPoLa;

		tilt &= ~bMaskBaFro;
		tilt &= ~bMaskPoLa;

		if( AXIS_DIR_X==mma_status.axis_dir_x && AXIS_DIR_Y==mma_status.axis_dir_y ) {
			if( TILT_POLA_LEFT==bPoLa_org ) {
				bPoLa = TILT_POLA_UP;
			}
			else if( TILT_POLA_RIGHT==bPoLa_org ) {
				bPoLa = TILT_POLA_DOWN;
			}
			else if( TILT_POLA_UP==bPoLa_org ) {
				bPoLa = TILT_POLA_LEFT;
			}
			else if( TILT_POLA_DOWN==bPoLa_org ) {
				bPoLa = TILT_POLA_RIGHT;
			}
			
		}
		else if(AXIS_DIR_X_N==mma_status.axis_dir_x && AXIS_DIR_Y==mma_status.axis_dir_y) {
			if( TILT_POLA_LEFT==bPoLa_org ) {
				bPoLa = TILT_POLA_UP;
			}
			else if( TILT_POLA_RIGHT==bPoLa_org ) {
				bPoLa = TILT_POLA_DOWN;
			}
			else if( TILT_POLA_UP==bPoLa_org ) {
				bPoLa = TILT_POLA_RIGHT;
			}
			else if( TILT_POLA_DOWN==bPoLa_org ) {
				bPoLa = TILT_POLA_LEFT;
			}
		}
		else if(AXIS_DIR_X_N==mma_status.axis_dir_x && AXIS_DIR_Y_N==mma_status.axis_dir_y) {
			if( TILT_POLA_LEFT==bPoLa_org ) {
				bPoLa = TILT_POLA_DOWN;
			}
			else if( TILT_POLA_RIGHT==bPoLa_org ) {
				bPoLa = TILT_POLA_UP;
			}
			else if( TILT_POLA_UP==bPoLa_org ) {
				bPoLa = TILT_POLA_RIGHT;
			}
			else if( TILT_POLA_DOWN==bPoLa_org ) {
				bPoLa = TILT_POLA_LEFT;
			}
		}
		else if(AXIS_DIR_X==mma_status.axis_dir_x && AXIS_DIR_Y_N==mma_status.axis_dir_y) {
			if( TILT_POLA_LEFT==bPoLa_org ) {
				bPoLa = TILT_POLA_DOWN;
			}
			else if( TILT_POLA_RIGHT==bPoLa_org ) {
				bPoLa = TILT_POLA_UP;
			}
			else if( TILT_POLA_UP==bPoLa_org ) {
				bPoLa = TILT_POLA_LEFT;
			}
			else if( TILT_POLA_DOWN==bPoLa_org ) {
				bPoLa = TILT_POLA_RIGHT;
			}
		}

		else if(AXIS_DIR_X==mma_status.axis_dir_y && AXIS_DIR_Y==mma_status.axis_dir_x ) {
			if( TILT_POLA_LEFT==bPoLa_org ) {
				bPoLa = TILT_POLA_LEFT;
			}
			else if( TILT_POLA_RIGHT==bPoLa_org ) {
				bPoLa = TILT_POLA_RIGHT;
			}
			else if( TILT_POLA_UP==bPoLa_org ) {
				bPoLa = TILT_POLA_UP;
			}
			else if( TILT_POLA_DOWN==bPoLa_org ) {
				bPoLa = TILT_POLA_DOWN;
			}
		}
		else if(AXIS_DIR_X_N==mma_status.axis_dir_y && AXIS_DIR_Y==mma_status.axis_dir_x) {
			if( TILT_POLA_LEFT==bPoLa_org ) {
				bPoLa = TILT_POLA_LEFT;
			}
			else if( TILT_POLA_RIGHT==bPoLa_org ) {
				bPoLa = TILT_POLA_RIGHT;
			}
			else if( TILT_POLA_UP==bPoLa_org ) {
				bPoLa = TILT_POLA_DOWN;
			}
			else if( TILT_POLA_DOWN==bPoLa_org ) {
				bPoLa = TILT_POLA_UP;
			}
		}
		else if(AXIS_DIR_X==mma_status.axis_dir_y && AXIS_DIR_Y_N==mma_status.axis_dir_x) {
			if( TILT_POLA_LEFT==bPoLa_org ) {
				bPoLa = TILT_POLA_RIGHT;
			}
			else if( TILT_POLA_RIGHT==bPoLa_org ) {
				bPoLa = TILT_POLA_LEFT;
			}
			else if( TILT_POLA_UP==bPoLa_org ) {
				bPoLa = TILT_POLA_UP;
			}
			else if( TILT_POLA_DOWN==bPoLa_org ) {
				bPoLa = TILT_POLA_DOWN;
			}
		}
		else if(AXIS_DIR_X_N==mma_status.axis_dir_y && AXIS_DIR_Y_N==mma_status.axis_dir_x) {
			if( TILT_POLA_LEFT==bPoLa_org ) {
				bPoLa = TILT_POLA_RIGHT;
			}
			else if( TILT_POLA_RIGHT==bPoLa_org ) {
				bPoLa = TILT_POLA_LEFT;
			}
			else if( TILT_POLA_UP==bPoLa_org ) {
				bPoLa = TILT_POLA_DOWN;
			}
			else if( TILT_POLA_DOWN==bPoLa_org ) {
				bPoLa = TILT_POLA_UP;
			}
		}
		else {
			WARNING_MSG("%s(%d):do not support this axis convertion .\n",__FUNCTION__,__LINE__);
		}


		switch (mma_status.axis_dir_z) {
			case AXIS_DIR_Z:
				bBaFro = bBaFro_org;
				break;
			case AXIS_DIR_Z_N:
				bBaFro = (unsigned char)((~bBaFro_org)&bMaskBaFro);
				break;
				
			default :
			case AXIS_DIR_X:
			case AXIS_DIR_X_N:
			case AXIS_DIR_Y:
			case AXIS_DIR_Y_N:
				WARNING_MSG("%s(%d):do not support this axis convertion .\n",__FUNCTION__,__LINE__);
				break;
		}


		tilt |= bBaFro|bPoLa;
		*rem_tilt	= tilt;
		DBG_MSG("tilt 0x%02x->0x%02x\n",bTilt_org,tilt);
	}

}

 
static void remap_axis(short *rem_x,short *rem_y,short *rem_z,short x,short y,short z) 
{
	remap_axis_ex(rem_x,rem_y,rem_z,0,x,y,z,0);
}



int mma7660_irq_enable(int iIsEnable)
{
	int iRet = 0;
	MXC_GPIO_CFGS *L_pt_mma7660_gpio_cfgs=0;

	if(2!=gptHWCFG->m_val.bRSensor) {
		DBG_MSG("%s(): not mma7660 in hwconfig !\n",__FUNCTION__);
		return -1;
	}

	if(!mma7660_client) {
		ERR_MSG("%s(): mma7660 should be init first !\n",__FUNCTION__);
		return -2;
	}

	DBG_MSG("%s(%d);\n",__FUNCTION__,iIsEnable);

	L_pt_mma7660_gpio_cfgs = (MXC_GPIO_CFGS *)dev_get_platdata(&mma7660_client->dev);
	
	if(iIsEnable) {
		gpio_direction_input(L_pt_mma7660_gpio_cfgs->uiGPIO);
		enable_irq(L_pt_mma7660_gpio_cfgs->uiIRQ_num);
	}
	else {
		disable_irq(L_pt_mma7660_gpio_cfgs->uiIRQ_num);
		gpio_direction_output(L_pt_mma7660_gpio_cfgs->uiGPIO,0);
	}

	return iRet;
}

int mma7660_irqwake_enable(int iIsEnable) 
{
	MXC_GPIO_CFGS *L_pt_mma7660_gpio_cfgs=0;

	if(2!=gptHWCFG->m_val.bRSensor) {
		DBG_MSG("%s(): not mma7660 in hwconfig !\n",__FUNCTION__);
		return -1;
	}
	if(!mma7660_client) {
		ERR_MSG("%s(): mma7660 should be init first !\n",__FUNCTION__);
		return -2;
	}

	DBG_MSG("%s(%d);\n",__FUNCTION__,iIsEnable);

	L_pt_mma7660_gpio_cfgs = (MXC_GPIO_CFGS *)dev_get_platdata(&mma7660_client->dev);
	if(iIsEnable) {
		enable_irq_wake(L_pt_mma7660_gpio_cfgs->uiIRQ_num);
	}
	else {
		disable_irq_wake(L_pt_mma7660_gpio_cfgs->uiIRQ_num);
	}
	return 0;
}

int mma7660_read_orient (void)
{
	int result;
	u8 tilt_remap;
	int iNtxTilt=0;

	if(2!=gptHWCFG->m_val.bRSensor) {
		DBG_MSG("%s(): not mma7660 in hwconfig !\n",__FUNCTION__);
		return 0;
	}

	if(!mma7660_client) {
		ERR_MSG("%s(): mma7660 should be init first !\n",__FUNCTION__);
		return 0;
	}
	
	result = i2c_smbus_read_byte_data(mma7660_client, REG_TILT);
	remap_axis_ex(0,0,0,&tilt_remap,0,0,0,(u8)result);
	switch ((tilt_remap >> 2) & 0x07) {
		case 1:		// Left
			iNtxTilt = 2;
			break;
		case 2:		// Right
			iNtxTilt = 4;
			break;
		case 5:		// Down
			iNtxTilt = 3;
			break;
		case 6:		// UP
			iNtxTilt = 1;
			break;
	}
	DBG_MSG("%s() : tilt=%x->%x,ntxtilt=%d\n",__FUNCTION__,result>>2,tilt_remap>>2,iNtxTilt);
	return iNtxTilt;
}

/* set the Zslider key mapping */
static void cmd_sliderkey(const char *arg) 
{
	unsigned char key_code[4];
	int i;
	char *endptr,*p;

	p = (char *)arg;

	for ( i = 0; i < 4 ; i++ ) {
		if (*p == '\0') {
			break;
		}

		key_code[i] = (char)simple_strtoul(p,&endptr,16);
		p = endptr +1;

	}

	if (i != 4) {
		sprintf (res_buff,"FAIL:sliderkey command failed,only %d args provided\n",i);
		DBG ("%s: Failed to set slider key, not enough key code in command\n",__FUNCTION__);
		return;
	}


	DBG("%s: set slider key code  %02x %02x %02x %02x \n",__FUNCTION__,
		key_code[0],key_code[1],key_code[2],key_code[3]);


	for (i = 0; i < 4; i++) {
		mma_status.slider_key[i] = key_code[i];
	}

	sprintf(res_buff,"OK:set slider key ok, key code %02x %02x %02x %02x \n",
		key_code[0],key_code[1],key_code[2],key_code[3]);
}

/* remap the axis */
static void cmd_remap_axis(const char *arg,size_t count) 
{
	int 	buff_len; 	
	char	delimiters[] = " ,";

	char 	*token = NULL;
	
	int 	axis_cnt = 0;
	u8	axis_map[3];
	
	if (count > 63) {
		buff_len = 63;
	} else {
		buff_len = count;
	}

	while ((token = strsep((char **)&arg,delimiters)) != NULL) {

		if (strcmp(token,"")) {
			if (strcasecmp(token,"x") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_X;
			} else if (strcasecmp(token,"-x") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_X_N;
			} else if (strcasecmp(token,"y") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_Y;
			} else if (strcasecmp(token,"-y") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_Y_N;
			} else if (strcasecmp(token,"z") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_Z;
			} else if (strcasecmp(token,"-z") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_Z_N;
			} else {
				sprintf (res_buff,"FAIL:remapaxis error,wrong argument\n");
				return;
			}

			axis_cnt ++;

			if (axis_cnt == 3) {
				break;
			}
		}
	}

	if (axis_cnt != 3) {
		sprintf(res_buff,"FAIL:remapaxis error, not enough parameters(%d)\n",axis_cnt);
		return;
	}


	if (((axis_map[0] & 0xfe) == (axis_map[1] & 0xfe)) || 
		((axis_map[0] & 0xfe) == (axis_map[2] & 0xfe)) ||
		((axis_map[2] & 0xfe) == (axis_map[1] & 0xfe))) {

		sprintf(res_buff,"FAIL:remapaxis error, duplicate axis mapping\n");
		return;
	}


	mma_status.axis_dir_x	= axis_map[0];
	mma_status.axis_dir_y	= axis_map[1];
	mma_status.axis_dir_z	= axis_map[2];

	sprintf(res_buff,"OK:remapaxis ok, new mapping : %s %s %s\n",axis_dir[mma_status.axis_dir_x],
		axis_dir[mma_status.axis_dir_y],axis_dir[mma_status.axis_dir_z]);
}

/* parse the command passed into driver, and execute it */
static int exec_command(const char *buf, size_t count)
{
	const char *p, *s;
	const char *arg;
	int i, value = 0;//

	for (p = buf;; p++) {
		if (*p == ' ' || *p == '\0' || p - buf >= count)
			break;
	}
	arg = p + 1;

	DBG_MSG("%s() ,arg=\"%s\"\n",__FUNCTION__,arg);

	for (i = MMA_REG_R; i < MMA_CMD_MAX; i++) {
		s = command[i];
		if (s && !strncmp(buf, s, p - buf - 1)) {
			dev_info(&mma7660_client->dev, "command %s\n", s);
			goto mma_exec_command;
		}
	}

	dev_err(&mma7660_client->dev, "command not found\n");
	sprintf(res_buff,"FAIL:command [%s] not found\n",s);
	return -1;

mma_exec_command:

	//if (i != MMA_REG_R && i != MMA_REG_W)
	//	value = simple_strtoul(arg, NULL, 16);

	switch (i) {
	case MMA_REG_R:
		cmd_read_reg(arg);
		break;
	case MMA_REG_W:
		cmd_write_reg(arg);
		break;
	case MMA_DUMPREG:		/* dump registers	*/
		cmd_dumpreg();
		break;
	case MMA_REMAP_AXIS:		/* remap 3 axis's direction	*/
		cmd_remap_axis(arg,(count - (arg - buf)));
		break;
	case MMA_DUMPAXIS:		/* remap 3 axis's direction	*/
		sprintf(res_buff,"OK:dumpaxis : %s %s %s\n",axis_dir[mma_status.axis_dir_x],
			axis_dir[mma_status.axis_dir_y],axis_dir[mma_status.axis_dir_z]);
		break;
	case MMA_ENJOYSTICK: 
		emu_joystick = 1;
		sprintf(res_buff,"OK:joystick movement emulation enabled\n");
		break;
	case MMA_DISJOYSTICK:
		emu_joystick = 0;
		sprintf(res_buff,"OK:joystick movement emulation disabled\n");
		break;
	case MMA_SLIDER_KEY:		/* load offset drift registers	*/
		cmd_sliderkey(arg);
		break;
	case MMA_DUMP_SLKEY:
		sprintf(res_buff,"OK:dumpslkey 0x%02x 0x%02x 0x%02x 0x%02x\n",mma_status.slider_key[0],
			mma_status.slider_key[1],mma_status.slider_key[2],mma_status.slider_key[3]);
		break;
	case MMA_POLL_INTERVAL:
		value = simple_strtoul(arg, NULL, 0);
		if( (value>=POLL_INTERVAL_MIN) && (value<=POLL_INTERVAL_MAX) ) {
			poll_int=value;
			DBG_MSG("%s set poll interval => %d \n",__FUNCTION__,poll_int);

			if(0!=value) {
				
#ifdef MMA7660_TIMER_POLLING //[

				if(mma7660_timer_is_stoped()) {
					mma7660_timer_start(0);
				}
#endif //] MMA7660_TIMER_POLLING

			}
			sprintf(res_buff,"OK:mma7660 poll interval has been set to %d ticks\n",poll_int);
		}
		else {
			WARNING_MSG("%s set poll interval out of range ,it must be (>=%d&&<=%d) \n",
					__FUNCTION__,POLL_INTERVAL_MIN,POLL_INTERVAL_MAX );
		}
		break;
	default:
		dev_err(&mma7660_client->dev, "command is not found\n");
		sprintf (res_buff,"FAIL:no such command %d\n",i);
		break;
	}

	return 0;
}

/* show the command result */
static ssize_t mma7660_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%s",res_buff);
}

/* accept the command */
static ssize_t mma7660_store(struct device *dev,struct device_attribute *attr, 
		const char *buf,size_t count)
{
	exec_command(buf, count);

	return count;
}


#ifdef MMA7660_JOYDEV_ENABLE //[

typedef enum {NOSLIDE, UPWARD, DOWNWARD} SLIDEOUT;

#define NOSLIDE		0
#define SLIDEUPWARD	1
#define SLIDEUPWARD2	2
#define SLIDEDOWNWARD 	3
#define SLIDEDOWNWARD2	4

#define MAXSLIDEWIDTH 	10
#define SLIDERAVCOUNT 	4

#define SLIDE_THR	16
#define SLIDE_THR2	(36)

struct slider {
	int 		wave[SLIDERAVCOUNT];	/* for long term average */
	unsigned char	wave_idx;
	short		ref;
	unsigned char 	dir;
	unsigned char	report;
	unsigned char 	mode;
	unsigned char	cnt;	
};

static struct slider zsl_x,zsl_y,zsl_z;

/* check for the zslider event */
static int zslider(struct slider *slider, short axis) {
	int i;
	short average = 0;	

	slider->wave[slider->wave_idx] 	= axis;

	for (i = 0; i < SLIDERAVCOUNT; i++) 
		average += slider->wave[i];

	average /= SLIDERAVCOUNT;

	switch (slider->mode) {
		case 0:
			if (slider->wave_idx == (SLIDERAVCOUNT - 1)) 
				slider->mode = 1;
			break;
		case 1:
			if (axis > (average + SLIDE_THR)) {	/* slide up trigger 		*/
				slider->dir	= SLIDEUPWARD;
				slider->mode 	= 2;
				slider->cnt	= MAXSLIDEWIDTH;
				slider->ref	= average;
			} else if (axis < (average - SLIDE_THR)) {	/* slide down trigger 		*/
				slider->dir	= SLIDEDOWNWARD;
				slider->mode 	= 2;
				slider->cnt	= MAXSLIDEWIDTH;
				slider->ref	= average;
			}
			slider->report = NOSLIDE;
			break;
		case 2: 
			slider->cnt--;

			if (!slider->cnt) {				/* return to normal */
				slider->mode	= 1; 	
				slider->dir	= NOSLIDE;
				break;
			}

			if ((axis < (slider->ref + SLIDE_THR)) && (axis > (slider->ref - SLIDE_THR)) && (slider->cnt < MAXSLIDEWIDTH-2)) {
/* report the slide event */
				switch (slider->dir) {
					case SLIDEUPWARD:
						DBG("slide upward\n");
						break;
					case SLIDEDOWNWARD:
						DBG("slide downward\n");
						break;
				}

				slider->mode 	= 3;
				slider->cnt	= 5;    		/* add additonal delay */
				slider->report	= slider->dir; 
			}
			break;

		case 3:
			slider->report = NOSLIDE;
			slider->cnt --;
			if (!slider->cnt) {
				slider->mode = 1;
			}
			break;
		default:
			break;
	}

	slider->wave_idx++;
	if (slider->wave_idx == SLIDERAVCOUNT) 
		slider->wave_idx=0;

        return slider->report;
}

#endif //] MMA7660_JOYDEV_ENABLE


/* workqueue function to poll mma7660 data */
static void mma7660_poll_work(struct work_struct *work)  
{
	short 	x,y,z,tilt;
	short 	x_remap,y_remap,z_remap;
	
	u8 	zslide_x,zslide_y;
	u8	tilt_remap ;
//	int	swing;

	/*
	if(0==giMMA7660abs_open&&0==giMMA7660_open) {
		DBG_MSG("%s,mma7660abs open=%d,mma7660 open=%d\n",__FUNCTION__,\
				giMMA7660abs_open,giMMA7660_open);
		return ;
	}
	*/

	DBG_MSG("%s\n",__FUNCTION__);

	if (mma7660_read_data(&x,&y,&z,&tilt) != 0) {
		DBG("mma7660 data read failed\n");
		return;
	}

	remap_axis_ex(&x_remap,&y_remap,&z_remap,&tilt_remap,x,y,z,(u8)tilt);

	//if(giMMA7660abs_open) 
	{
		#if 1 //[ use remap axis ..

		input_report_abs(mma7660_abs_dev, ABS_X, x_remap);	
		input_report_abs(mma7660_abs_dev, ABS_Y, y_remap);	
		input_report_abs(mma7660_abs_dev, ABS_Z, z_remap);
		input_report_abs(mma7660_abs_dev, ABS_MISC, tilt_remap);
		#else //][!
		/* report the absulate sensor data to input device */
		input_report_abs(mma7660_abs_dev, ABS_X, x);	
		input_report_abs(mma7660_abs_dev, ABS_Y, y);	
		input_report_abs(mma7660_abs_dev, ABS_Z, z);
		input_report_abs(mma7660_abs_dev, ABS_MISC, tilt);
		#endif
		input_sync(mma7660_abs_dev);

	}
/* report joystick movement */

#ifdef MMA7660_JOYDEV_ENABLE //[

	if(giMMA7660_open) 
	{
		if (emu_joystick)  {	
			input_report_abs(mma7660_idev->input, ABS_X, x_remap);
			input_report_abs(mma7660_idev->input, ABS_Y, y_remap);
			input_report_abs(mma7660_idev->input, ABS_Z, z_remap-ONEGVALUE);
			input_sync(mma7660_idev->input);
		}


	/* check the zslider event */

		zslide_x 	= zslider(&zsl_x,x_remap);
		zslide_y	= zslider(&zsl_y,y_remap);
		
		switch (zslide_x) {
			case SLIDEUPWARD: 
				DBG("X report slide upward \n");
				if (mma_status.slider_key[SLIDER_X_UP]) {
					input_report_key(mma7660_idev->input, mma_status.slider_key[SLIDER_X_UP], 1);
					input_report_key(mma7660_idev->input, mma_status.slider_key[SLIDER_X_UP], 0);
				}
				break;
			case SLIDEDOWNWARD:
				DBG("X report slide downward \n");
				if (mma_status.slider_key[SLIDER_X_DOWN]) {
					input_report_key(mma7660_idev->input, mma_status.slider_key[SLIDER_X_DOWN], 1);
					input_report_key(mma7660_idev->input, mma_status.slider_key[SLIDER_X_DOWN], 0);
				}
				break;
		}
		
		switch (zslide_y) {
			case SLIDEUPWARD: 
				DBG("Y report slide upward \n");
				if (mma_status.slider_key[SLIDER_Y_UP]) {
					input_report_key(mma7660_idev->input, mma_status.slider_key[SLIDER_Y_UP], 1);
					input_report_key(mma7660_idev->input, mma_status.slider_key[SLIDER_Y_UP], 0);
				}
				break;
			case SLIDEDOWNWARD:
				DBG("Y report slide downward \n");
				if (mma_status.slider_key[SLIDER_Y_DOWN]) {
					input_report_key(mma7660_idev->input, mma_status.slider_key[SLIDER_Y_DOWN], 1);
					input_report_key(mma7660_idev->input, mma_status.slider_key[SLIDER_Y_DOWN], 0);
				}
				break;
		}
		

		if ((zslide_x) || (zslide_y)) {
			input_sync(mma7660_idev->input);
		};
	}

#endif //] MMA7660_JOYDEV_ENABLE

	return;

}

 
/* polling callback function of input polled device */
static void mma7660_dev_poll(struct input_polled_dev *dev)
{
	if (mma_cal) {
		DBG("mma7660 is now doing calibration\n");
		return;
	}

/* Since the the data reading would take a long time, 
   schedule the real work to a workqueue */
	queue_work(mma7660_workqueue, &mma7660_work);
	return;
}

#if 0
static int mma7660_open(struct input_dev *dev) {
	mma_open = 1;

/* init slider structure */

	DBG("mma7660_open() called\n");
/* reset the zslider structure */
	memset(&zsl_x,0,sizeof(struct slider));
	memset(&zsl_y,0,sizeof(struct slider));
	memset(&zsl_z,0,sizeof(struct slider));

	return 0;
}

static int mma7660_close(struct input_dev *dev) {
	mma_open = 0;
	return 0;
}

#endif 

static irqreturn_t mma7660_isr(int irq, void *dev_id)
{
	
	
#if defined(MMA7660_POLLDEV) || defined(MMA7660_TIMER_POLLING) //[
	
	printk(KERN_INFO "mma7660 sensor triggered ...\n");
#else //][!MMA7660_POLLDEV
	DBG_MSG("%s\n",__FUNCTION__);
	queue_work(mma7660_workqueue, &mma7660_work);
#endif //] MMA7660_POLLDEV

	return 0;
}

int mma7660_open(struct input_dev *dev)
{
	giMMA7660_open++;

	DBG_MSG("%s(),opencnt=%d\n",__FUNCTION__,giMMA7660_open);
	return 0;
}

void mma7660_close(struct input_dev *dev)
{
	giMMA7660_open--;
	DBG_MSG("%s(),opencnt=%d\n",__FUNCTION__,giMMA7660_open);
}

int mma7660abs_open(struct input_dev *dev)
{
	int iRet=0;
	s32 sChk;


#ifdef MMA7660_GINT_MODE //[

	if( !(mma7660_reg_intsu&0x10) ) {//
		int modereg;
		s32 sChk;

		
		modereg = i2c_smbus_read_byte_data(mma7660_client, REG_MODE);

		sChk = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg & (~0x01));

		mma7660_reg_intsu|=0x10;// enable GINT .
		sChk = i2c_smbus_write_byte_data(mma7660_client, REG_INTSU, mma7660_reg_intsu);
		DBG_MSG("%s,enable GINT INTSU=0x%02X ,ret=%d\n",
				__FUNCTION__,mma7660_reg_intsu,sChk);

		sChk = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg);
	}
#endif //] MMA7660_GINT_MODE

#ifdef MMA7660_TIMER_POLLING //[

	mma7660_timer_start(0);
#endif //] MMA7660_TIMER_POLLING

	giMMA7660abs_open++;
	DBG_MSG("%s(),opencnt=%d\n",__FUNCTION__,giMMA7660abs_open);
	return iRet;
}

void mma7660abs_close(struct input_dev *dev)
{
#ifdef MMA7660_GINT_MODE //[

	if( (mma7660_reg_intsu&0x10) ) {//
		int modereg;
		s32 sChk;
		int iRetryCnt;


		disable_irq(gpt_mma7660_int_gpio_cfgs->uiIRQ_num);
		modereg = i2c_smbus_read_byte_data(mma7660_client, REG_MODE);

		sChk = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg & (~0x01));

		mma7660_reg_intsu&=~0x10;// enable GINT .

		iRetryCnt=1;
		do {
			sChk = i2c_smbus_write_byte_data(mma7660_client, REG_INTSU, mma7660_reg_intsu);
			DBG_MSG("%s,disable GINT, INTSU=%02X,ret=%d,retry%d\n",
					__FUNCTION__,mma7660_reg_intsu,sChk,iRetryCnt);
			if(0==sChk) {
				break;
			}
		}while (iRetryCnt++<=10);

		sChk = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg);
		enable_irq(gpt_mma7660_int_gpio_cfgs->uiIRQ_num);
	}
#endif //] MMA7660_GINT_MODE

#ifdef MMA7660_TIMER_POLLING //[

	mma7660_timer_stop();
#endif //] MMA7660_TIMER_POLLING

	giMMA7660abs_open--;
	DBG_MSG("%s(),opencnt=%d\n",__FUNCTION__,giMMA7660abs_open);
}

/*
int mma7660abs_flush(struct input_dev *dev, struct file *file)
{
	int iRet;
	return iRet;
}
*/

/* detecting mma7660 chip */
static int mma7660_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
        struct i2c_adapter *adapter = client->adapter;

        if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE
                                     | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
                return -ENODEV;                                        

       strlcpy(info->type, "mma7660", I2C_NAME_SIZE);

        return 0;
}


static int mma7660_probe(struct i2c_client *client,const struct i2c_device_id *id) {
	struct input_dev 	*idev;
	int 			rc; 
	int 			ret;
	int 			i;



	MXC_GPIO_CFGS *L_pt_mma7660_gpio_cfgs=0;

	if(2!=gptHWCFG->m_val.bRSensor) {
		DBG_MSG("%s(): not mma7660 in hwconfig !\n",__FUNCTION__);
		return -EPERM;
	}

	if(34==gptHWCFG->m_val.bPCB||35==gptHWCFG->m_val.bPCB) {
		// E606FXA/E606FXB .
		u8 x_before_chg=mma_status.axis_dir_x,y_before_chg=mma_status.axis_dir_y;
		mma_status.axis_dir_x = AXIS_DIR_X_N;
		mma_status.axis_dir_y = AXIS_DIR_Y_N;
		DBG_MSG("%s(%d): axis exchange dirx=%d->%d,diry=%d->%d\n",__FUNCTION__,__LINE__,x_before_chg,mma_status.axis_dir_x,y_before_chg,mma_status.axis_dir_y);
	}
	else
	{
		// x<=>y .
		u8 x_before_chg=mma_status.axis_dir_x,y_before_chg=mma_status.axis_dir_y;
		mma_status.axis_dir_x = AXIS_DIR_X;
		mma_status.axis_dir_y = AXIS_DIR_Y;
		DBG_MSG("%s(%d): axis exchange dirx=%d->%d,diry=%d->%d\n",__FUNCTION__,__LINE__,x_before_chg,mma_status.axis_dir_x,y_before_chg,mma_status.axis_dir_y);
	}



	printk(KERN_INFO "try to detect mma7455 chip id %02x\n",client->addr);
	mma7660_workqueue = create_singlethread_workqueue("mma7660");
        if (!mma7660_workqueue) {
	        return -ENOMEM; /* most expected reason */
        }

	//printk(KERN_INFO "msleep 100\n");msleep(100);

	printk(KERN_INFO "1 try to create file\n");


	mma7660_client = client;

	printk(KERN_INFO "2 try to create file\n");

/* create device file in sysfs as user interface */
	ret = device_create_file(&client->dev, &mma7660_dev_attr);
	printk(KERN_INFO "create file\n");
	if (ret) {
		printk(KERN_INFO "create file failed\n");	
		dev_err(&client->dev, "create device file failed!\n");
		rc = -EINVAL;
		goto err_detach_client;
	}

	printk(KERN_INFO "3 try to create file\n");


#ifdef MMA7660_JOYDEV_ENABLE //[

/* allocate & register input polling device  */
	mma7660_idev = input_allocate_polled_device();
	if (!mma7660_idev) {
		dev_err(&client->dev, "allocate poll device failed!\n");
		rc = -ENOMEM;
		goto err_free_input;
	}
	mma7660_idev->poll 		= mma7660_dev_poll;
	mma7660_idev->poll_interval 	= poll_int ;
	mma7660_idev->poll_interval_min = POLL_INTERVAL_MIN;
	mma7660_idev->poll_interval_max = POLL_INTERVAL_MAX;

	idev 			= mma7660_idev->input;
	idev->name 		= DEVICE_NAME;
	idev->id.bustype 	= BUS_I2C;
	idev->dev.parent 	= &client->dev;

	idev->open = mma7660_open;
	idev->close = mma7660_close;

	set_bit(EV_REL,idev->evbit);
	set_bit(EV_KEY,idev->evbit);

	input_set_abs_params(mma7660_idev->input, ABS_X, -AXIS_MAX, AXIS_MAX, 0, 0);
	input_set_abs_params(mma7660_idev->input, ABS_Y, -AXIS_MAX, AXIS_MAX, 0, 0);
	input_set_abs_params(mma7660_idev->input, ABS_Z, -AXIS_MAX, AXIS_MAX, 0, 0);
	set_bit(EV_ABS, mma7660_idev->input->evbit);

	idev->keycode = atkbd_keycode;
	idev->keycodesize = sizeof(unsigned char);
	idev->keycodemax = ARRAY_SIZE(atkbd_keycode);

	for (i = 0; i < 512; i++)
		if (atkbd_keycode[i] && atkbd_keycode[i] < ATKBD_SPECIAL)
			set_bit(atkbd_keycode[i], idev->keybit);


#endif //] MMA7660_JOYDEV_ENABLE


#ifdef MMA7660ABS_POLLDEV //[
	
	mma7660_abs_poll_idev = input_allocate_polled_device();
	if(!mma7660_abs_poll_idev) {
		dev_err(&client->dev, "allocate poll device failed!\n");
		rc = -ENOMEM;
		goto err_free_abs;
	}
	mma7660_abs_poll_idev->poll 		= mma7660_dev_poll;
	mma7660_abs_poll_idev->poll_interval 	= poll_int ;
	mma7660_abs_poll_idev->poll_interval_min = POLL_INTERVAL_MIN;
	mma7660_abs_poll_idev->poll_interval_max = POLL_INTERVAL_MAX;

	mma7660_abs_dev = mma7660_abs_poll_idev->input;

#else	//][! MMA7660ABS_POLLDEV
	mma7660_abs_dev = input_allocate_device();

	if (!mma7660_abs_dev) {
		dev_err(&client->dev, "allocate input device failed!\n");
		rc = -ENOMEM;
		goto err_free_abs;
	}
#endif //] MMA7660ABS_POLLDEV

	mma7660_abs_dev->name		= ABS_DEVICE_NAME;
	mma7660_abs_dev->id.bustype	= BUS_I2C;
	mma7660_abs_dev->dev.parent	= &client->dev;
	mma7660_abs_dev->open = mma7660abs_open;
	mma7660_abs_dev->close = mma7660abs_close;

	set_bit(EV_ABS,mma7660_abs_dev->evbit);
	set_bit(ABS_X,mma7660_abs_dev->absbit);
	set_bit(ABS_Y,mma7660_abs_dev->absbit);
	set_bit(ABS_Z,mma7660_abs_dev->absbit);
	set_bit(ABS_MISC,mma7660_abs_dev->absbit);

#ifdef MMA7660_JOYDEV_ENABLE //[

	ret = input_register_polled_device(mma7660_idev);
	if (ret) {
		dev_err(&client->dev, "register poll device failed!\n");
		rc = -EINVAL;
		goto err_unreg_input;
	}
#endif //] MMA7660_JOYDEV_ENABLE


#ifdef MMA7660ABS_POLLDEV //[

	ret = input_register_polled_device(mma7660_abs_poll_idev);
#else //][!MMA7660ABS_POLLDEV
	ret = input_register_device(mma7660_abs_dev);
#endif //] MMA7660ABS_POLLDEV	
	if (ret) {
		dev_err(&client->dev, "register abs device failed!\n");
		rc = -EINVAL;
		goto err_unreg_abs_input;
	}

#ifdef MMA7660_JOYDEV_ENABLE //[

	memset(&zsl_x,0,sizeof(struct slider));
	memset(&zsl_y,0,sizeof(struct slider));
	memset(&zsl_z,0,sizeof(struct slider));
#endif //] MMA7660_JOYDEV_ENABLE

	memset(res_buff,0,RES_BUFF_LEN);

#if 0

/* enable gSensor mode 8g, measure */
	i2c_smbus_write_byte_data(client, REG_MODE, 0x00);		/* standby mode   */
	i2c_smbus_write_byte_data(client, REG_SPCNT, 0x00);		/* no sleep count */
	i2c_smbus_write_byte_data(client, REG_INTSU, 0x00);		/* no interrupt   */
	i2c_smbus_write_byte_data(client, REG_PDET, 0xE0);		/* disable tap    */
	i2c_smbus_write_byte_data(client, REG_SR, 0x6B);		/* 4 measurement, 16 sample  */
	i2c_smbus_write_byte_data(client, REG_MODE, 0x01);		/* active mode   */
#else

	//Configure MMA7660FC as Portrait/Landscape Detection
	ret = i2c_smbus_write_byte_data(client, REG_MODE, 0x00);		//Standby Mode
	if (0 > ret) {
		goto err_unreg_abs_input;
	}
	i2c_smbus_write_byte_data(client, REG_SPCNT, 0x00);		//No sleep count
	i2c_smbus_write_byte_data(client, REG_INTSU, mma7660_reg_intsu);		

//	i2c_smbus_write_byte_data(client, REG_SR, 	0x34);		//8 samples/s, TILT debounce filter = 2
	i2c_smbus_write_byte_data(client, REG_SR, 	0xDD);		//4 samples/s, TILT debounce filter = 6
	i2c_smbus_write_byte_data(client, REG_PDET, 0xE0);		//No tap detection enabled
	i2c_smbus_write_byte_data(client, REG_PD, 	0x00);		//No tap detection debounce count enabled
	i2c_smbus_write_byte_data(client, REG_MODE, 0x41);		//Active Mode, INT = push-pull and active low
	
#endif


	L_pt_mma7660_gpio_cfgs = (MXC_GPIO_CFGS *)dev_get_platdata(&client->dev);
	gpt_mma7660_int_gpio_cfgs = L_pt_mma7660_gpio_cfgs;

	mxc_iomux_v3_setup_pad(L_pt_mma7660_gpio_cfgs->tPad_Cfg);
	gpio_request(L_pt_mma7660_gpio_cfgs->uiGPIO, L_pt_mma7660_gpio_cfgs->sz_name);
	gpio_direction_input(L_pt_mma7660_gpio_cfgs->uiGPIO);

	set_irq_type(L_pt_mma7660_gpio_cfgs->uiIRQ_num, L_pt_mma7660_gpio_cfgs->uiIRQ_type);
	ret = request_irq(L_pt_mma7660_gpio_cfgs->uiIRQ_num, mma7660_isr, 0,
			L_pt_mma7660_gpio_cfgs->sz_name, 0);
	if (ret) {
		WARNING_MSG("register mma7660 interrupt failed\n");

		gpt_mma7660_int_gpio_cfgs = 0;
	}
	else {
	}

	if (1==L_pt_mma7660_gpio_cfgs->iInterruptWakeup) {
		enable_irq_wake(L_pt_mma7660_gpio_cfgs->uiIRQ_num);
	}
	else {
		disable_irq_wake(L_pt_mma7660_gpio_cfgs->uiIRQ_num);
	}


	return 0;
err_unreg_abs_input:
#ifdef MMA7660ABS_POLLDEV //[

	input_unregister_polled_device(mma7660_abs_poll_idev);
#else //][! MMA7660ABS_POLLDEV
	input_unregister_device(mma7660_abs_dev);
#endif //]  MMA7660ABS_POLLDEV

#ifdef MMA7660_JOYDEV_ENABLE //[

err_unreg_input:
	input_unregister_polled_device(mma7660_idev);
#endif //] MMA7660_JOYDEV_ENABLE

err_free_abs:
#ifdef MMA7660ABS_POLLDEV //[

	input_free_polled_device(mma7660_abs_poll_idev);
	mma7660_abs_poll_idev = 0;
#else //][! MMA7660ABS_POLLDEV
	input_free_device(mma7660_abs_dev);
#endif //]  MMA7660ABS_POLLDEV
	mma7660_abs_dev=0;


#ifdef MMA7660_JOYDEV_ENABLE //[
	
err_free_input:
	input_free_polled_device(mma7660_idev);
#endif //] MMA7660_JOYDEV_ENABLE

err_detach_client:
	destroy_workqueue(mma7660_workqueue);

	kfree(client);
	return rc; 
}


static int mma7660_remove(struct i2c_client *client)  {

#ifdef MMA7660ABS_POLLDEV //[

	input_unregister_polled_device(mma7660_abs_poll_idev);
#else //][! MMA7660ABS_POLLDEV
	input_unregister_device(mma7660_abs_dev);
#endif//]   MMA7660ABS_POLLDEV

#ifdef MMA7660_JOYDEV_ENABLE //[
	
	input_unregister_polled_device(mma7660_idev);
#endif //] MMA7660_JOYDEV_ENABLE

#ifdef MMA7660ABS_POLLDEV //[

	input_free_polled_device(mma7660_abs_poll_idev);
	mma7660_abs_poll_idev=0;
#else //][! MMA7660ABS_POLLDEV
	input_free_device(mma7660_abs_dev);
#endif//]   MMA7660ABS_POLLDEV
	mma7660_abs_dev = 0;

#ifdef MMA7660_JOYDEV_ENABLE //[
	
	input_free_polled_device(mma7660_idev);
#endif //] MMA7660_JOYDEV_ENABLE

	device_remove_file(&client->dev, &mma7660_dev_attr);

	kfree(i2c_get_clientdata(client));
	flush_workqueue(mma7660_workqueue);
	destroy_workqueue(mma7660_workqueue);
	gpt_mma7660_int_gpio_cfgs = 0;
	DBG("MMA7660 device detatched\n");	
        return 0;
}


static int mma7660_suspend(struct i2c_client *client, pm_message_t state)
{
	if(gSleep_Mode_Suspend) {
		u8 bSetMode;

		mma_status.mode = i2c_smbus_read_byte_data(mma7660_client, REG_MODE);
		bSetMode = (u8)(mma_status.mode&(~0x01));
		DBG_MSG("mma7660 current mode=0x%x->0x%x\n",mma_status.mode,bSetMode);

		//i2c_smbus_write_byte_data(mma7660_client, REG_MODE,mma_status.mode & ~0x01);
		i2c_smbus_write_byte_data(mma7660_client, REG_MODE,mma_status.mode);

		mma7660_irqwake_enable(0);
		mma7660_irq_enable(0);
	}
	else 
	{

#ifdef MMA7660_GINT_MODE //[
	
		if( (mma7660_reg_intsu&0x10) ) {//
			int modereg;
			s32 sChk;
			int iRetryCnt;

			
			modereg = i2c_smbus_read_byte_data(mma7660_client, REG_MODE);

			sChk = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg & (~0x01));

			mma7660_reg_intsu&=~0x10;// disable GINT .

			iRetryCnt=1;
			do {
				sChk = i2c_smbus_write_byte_data(mma7660_client, REG_INTSU, mma7660_reg_intsu);
				DBG_MSG("%s,disable GINT, INTSU=%02X,ret=%d,retry%d\n",
						__FUNCTION__,mma7660_reg_intsu,sChk,iRetryCnt);
				if(0==sChk) {
					break;
				}
			}while (iRetryCnt++<=10);

			sChk = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg);
		}
#endif //] MMA7660_GINT_MODE

	}

	DBG_MSG("MMA7660 suspended\n");
	return 0;
}

static int mma7660_resume(struct i2c_client *client)
{

	if(gSleep_Mode_Suspend) {
		mma7660_irq_enable(1);
		mma7660_irqwake_enable(1);

		i2c_smbus_write_byte_data(mma7660_client, REG_MODE, mma_status.mode);
	}
	else {
#ifdef MMA7660_GINT_MODE //[
	
		if( !(mma7660_reg_intsu&0x10) ) {//
			int modereg;
			s32 sChk;

			
			modereg = i2c_smbus_read_byte_data(mma7660_client, REG_MODE);

			sChk = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg & (~0x01));

			mma7660_reg_intsu|=0x10;// enable GINT .
			sChk = i2c_smbus_write_byte_data(mma7660_client, REG_INTSU, mma7660_reg_intsu);
			DBG_MSG("%s,enable GINT INTSU=0x%02X ,ret=%d\n",
					__FUNCTION__,mma7660_reg_intsu,sChk);

			sChk = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg);
		}

#endif //] MMA7660_GINT_MODE
	}

	DBG_MSG("MMA7660 resumed\n");
	return 0;
}



static int __init init_mma7660(void)
{
	int iRet=0;
/* register driver */

	iRet = i2c_add_driver(&mma7660_driver);
	if (iRet < 0){
		printk(KERN_ERR "add mma7660 i2c driver failed\n");
		return -ENODEV;
	}
	printk(KERN_INFO "add mma7660 i2c driver\n");

	return iRet;
}

static void __exit exit_mma7660(void)
{
	printk(KERN_INFO "remove mma7660 i2c driver.\n");
	i2c_del_driver(&mma7660_driver);
}


module_init(init_mma7660);
module_exit(exit_mma7660);

module_param(debug, bool, S_IRUGO | S_IWUSR);
module_param(emu_joystick, bool, S_IRUGO | S_IWUSR);
module_param(poll_int, int, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(debug, "1: Enable verbose debugging messages");
MODULE_PARM_DESC(emu_joystick, "1: Enable emulate joystick movement by tilt");
MODULE_PARM_DESC(poll_int, "set the poll interval of gSensor data (unit: ms)");

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MMA7660 sensor driver");
MODULE_LICENSE("GPL");

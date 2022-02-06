/*
 *  ELAN touchscreen driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
      
#include <linux/input.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include "../../../drivers/misc/ntx-misc.h"

#include "../../../arch/arm/mach-mx6/ntx_hwconfig.h"
extern volatile NTX_HWCONFIG *gptHWCFG;

//#define _NEW_FIRMWARE_

//#ifdef _NEW_FIRMWARE_
static unsigned int xResolution;
static unsigned int yResolution;
static int isNTXDiamond = 0;
static int isCalibrated;
static int gELAN_I2C_format = 0;
static int g_fw_id, g_fw_version;

static uint8_t *gElanBuffer;

//#endif
//#define _WITH_DELAY_WORK_
static const char ELAN_TS_NAME[]	= "elan-touch";
#define TS_POLL_PERIOD	msecs_to_jiffies(10) /* ms delay between samples */

extern int gIsCustomerUi;

//#define DBG_MSG	printk
#define DBG_MSG	

#define tp_int_pin			183

//#define ELAN_TS_X_MAX 		1500<<1
//#define ELAN_TS_Y_MAX 		1000<<1

#define DEFAULT_PANEL_W		1024
#define DEFAULT_PANEL_H		758
static unsigned long ELAN_TS_X_MAX=DEFAULT_PANEL_W;
static unsigned long ELAN_TS_Y_MAX=DEFAULT_PANEL_H;
static unsigned long ELAN_TS_WIDTH=DEFAULT_PANEL_W;
static unsigned long ELAN_TS_HIGHT=DEFAULT_PANEL_H;


#define IDX_QUEUE_SIZE		100
static int IDX_PACKET_SIZE;

#define ELAN_TOUCH_MT_EVENT

enum {
	version_id_packet	= 0x52,
	hello_packet 		= 0x55,
	idx_coordinate_packet 	= 0x5a,
	multi_coordinate_packet = 0x62,
};

enum {
	idx_finger_state = 7,
};

static struct workqueue_struct *elan_wq;
static uint16_t g_touch_pressed, g_touch_triggered;
static uint8_t RECOVERY=0x00;

static struct elan_data {
	int intr_gpio;
	int use_irq;
	struct hrtimer timer;
#ifdef _WITH_DELAY_WORK_
	struct delayed_work	work;
#else
	struct work_struct work;
#endif
	struct i2c_client *client;
	struct input_dev *input;
	wait_queue_head_t wait;
} elan_touch_data;

extern unsigned int msp430_read(unsigned int reg);
/*--------------------------------------------------------------*/
static int elan_touch_detect_int_level(void)
{
	return gpio_get_value(elan_touch_data.intr_gpio);
}

static int __elan_touch_poll(struct i2c_client *client)
{
	int status = 0, retry = 20;

	do {
		status = elan_touch_detect_int_level();
		retry--;
		mdelay(20);
	} while (status == 1 && retry > 0);

	return (status == 0 ? 0 : -ETIMEDOUT);
}

static int elan_touch_poll(struct i2c_client *client)
{
	return __elan_touch_poll(client);
}

static int __hello_packet_handler(struct i2c_client *client)
{
	int rc,i;
	uint8_t buf_recv[20] = { 0 };

	rc = elan_touch_poll(client);

	if (rc < 0) {
		return -EINVAL;
	}

	rc = i2c_master_recv(client, buf_recv, 8);
	if (rc != 8) {
		return -1;	// Joseph 20101025
	} else {
		printk("[elan] %s: hello packet %02x:%02X:%02x:%02x:%02x:%02x:%02x:%02x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3] , buf_recv[4], buf_recv[5], buf_recv[6], buf_recv[7]);
	
		if(buf_recv[0]==0x55 && buf_recv[1]==0x55 && buf_recv[2]==0x80 && buf_recv[3]==0x80)
		{
			RECOVERY=0x80;
			return RECOVERY; 
		}
		for (i = 0; i < 2; i++)
			if (buf_recv[i] != hello_packet)
				return -EINVAL;
	}
	return 0;
}

static inline int elan_touch_parse_xy(uint8_t *data, uint16_t *x, uint16_t *y)
{
#if 0	
	*x = (data[0] & 0xf0);
	*x <<= 4;
	*x |= data[1];

	*y = (data[0] & 0x0f);
	*y <<= 8;
	*y |= data[2];
	
	*x <<= 1;
	*y <<= 1;
#else
	int screenX = ELAN_TS_HIGHT-1 , screenY = ELAN_TS_WIDTH-1; 
	int elanInternalX = 832, elanInternalY = 1280;  //default 1024*758
	if (1==gELAN_I2C_format) {
		elanInternalX = xResolution;
		elanInternalY = yResolution;
	}
	else if (2==gELAN_I2C_format) {
		elanInternalX = 1856;
		elanInternalY = 2490;  // estimated value
	}

	if ( screenX > elanInternalX || screenY > elanInternalY )
	{
		printk("[%s-%d]ERROR : screen size over elan internal resolution!\n", __func__, __LINE__);
		return -1;
	}

	int _x,_y;
	_x = (data[0] & 0xf0);
	_x <<= 4;
	_x |= data[1];

	_y = (data[0] & 0x0f);
	_y <<= 8;
	_y |= data[2];

#if 1
	if( 0==gptHWCFG->m_val.bDisplayResolution ) {  // resolution 800*600
		*x = 799 - (_x * 799 / 1088);
		*y = (_y * 599 / 768);
	}
	else if( 6==gptHWCFG->m_val.bDisplayResolution ){  // 1600*1200
		*x = (_x * screenX / elanInternalX);
		if (gIsCustomerUi)
			*y = _y * screenY / elanInternalY;
		else
			*y = screenX - (_y * screenY / elanInternalY);
	}
	else {
		*x = _y * screenY / elanInternalY;
  		if (gIsCustomerUi) 
			*y = screenX - (_x * screenX / elanInternalX);
		else
			*y = (_x * screenX / elanInternalX);
	}
#else if
	if(1==gptHWCFG->m_val.bDisplayResolution) {  // resolution 1024*758
  		*x = _y * 1023 / rangeY;
  		if (gIsCustomerUi) 
			*y = 757 - (_x * 757 / rangeX);
		else
			*y = (_x * 757 / rangeX);
	}else{
		*x = 799 - (_x * 799 / 1088);
		*y = (_y * 599 / 768);
	}
#endif
//	printk ("[%s-%d] (%d, %d) (%d, %d)\n",__func__,__LINE__,_x,_y,*x,*y);
#endif

//	return 0;

	/* returns 0 if no data (0,0) reported by touch controller*/
	/* returns 1 if any data reported 									 			*/ 
	return ( _x || _y );
}

static void __elan_touch_calibration (struct i2c_client *client)
{
	int rc;
	uint8_t buf_recv[6] = { 0 };
	uint8_t cmd_calib0[] = {0x97, 0xFF, 0xF1, 0x00, 0x00, 0xF1};
	uint8_t cmd_calib1[] = {0x97, 0x04, 0x91, 0x0F, 0xA0, 0xF1};
	uint8_t cmd_calib2[] = {0x97, 0xFF, 0xF0, 0x00, 0x00, 0xF1};
	uint8_t cmd_calib3[] = {0x96, 0x04, 0x91, 0x00, 0x00, 0xF1};

	if (!isNTXDiamond || isCalibrated)
		return;

	i2c_master_send(client, cmd_calib0, 6);
	msleep (20);
	i2c_master_send(client, cmd_calib1, 6);
	msleep (20);
	i2c_master_send(client, cmd_calib2, 6);
	msleep (20);
	i2c_master_send(client, cmd_calib3, 6);
	msleep (20);
	// 0x95 0x04 0x91 0x0F 0xA0 0xF1
	rc = i2c_master_recv(client, buf_recv, 6);
	if (0x95 == buf_recv[0] && 0x04 == buf_recv[1] && 0x91 == buf_recv[2] &&
		0x0F == buf_recv[3] && 0xA0 == buf_recv[4] && 0xF1 == buf_recv[5]) {
		printk ("[%s-%d] set calibration done. \n", __func__, __LINE__);
		isCalibrated = 1;
	}
	else
		printk ("[%s-%d] set calibration failed. \n", __func__, __LINE__);
}

/*	__elan_touch_init -- hand shaking with touch panel
 *
 *	1.recv hello packet
 */
static int __elan_touch_init(struct i2c_client *client)
{
	int rc;
	int major,minor,fw_ver,fw_id;
	uint8_t buf_recv[4] = { 0 };
	uint8_t getFirmwareVer[] = {0x53,0x00,0x00,0x01};
	uint8_t getFirmwareId[] = {0x53,0xF0,0x00,0x01};
	uint8_t cmd_x[] = {0x53, 0x60, 0x00, 0x00}; /*Get x resolution*/
	uint8_t cmd_y[] = {0x53, 0x63, 0x00, 0x00}; /*Get y resolution*/
	uint8_t cmd_bc[] = {0x53, 0x01, 0x00, 0x01};/* Get BootCode Version*/
	
	rc = __hello_packet_handler(client);
	if (rc < 0)
		goto hand_shake_failed;
	
	i2c_master_send(client, getFirmwareVer, 4);
	rc = i2c_master_recv(client, buf_recv, 4);
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	fw_ver = major << 8 | minor;
	printk ("[%s-%d] firmware version %02X %02X %02X %02X (%04X)\n", __func__, __LINE__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3],fw_ver);

	i2c_master_send(client, getFirmwareId, 4);
	rc = i2c_master_recv(client, buf_recv, 4);
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	fw_id = major << 8 | minor;
	printk ("[%s-%d] firmware ID %02X %02X %02X %02X (%04X)\n", __func__, __LINE__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3],fw_id);
	
	if( fw_id==0x2422 ){  //13.3"
		gELAN_I2C_format = 2;
	}
	else if( fw_id!=0x10D3 ){
		//printk("%s():fw_id=0x%x,pcbid=%d\n",__FUNCTION__,fw_id,(int)gptHWCFG->m_val.bPCB);
		if (40==gptHWCFG->m_val.bPCB || 49==gptHWCFG->m_val.bPCB) {
			// E60Q5X , E60QD2
			isNTXDiamond = 1;
			msp430_homepad_sensitivity_set(0x05);
		}
		gELAN_I2C_format = 1;
	}

	if(2==gELAN_I2C_format) {
		IDX_PACKET_SIZE = 34;
	}
	else {
		IDX_PACKET_SIZE = 8;
	}

	gElanBuffer = kmalloc(IDX_PACKET_SIZE*IDX_QUEUE_SIZE*sizeof(uint8_t),GFP_KERNEL);

	if(1==gELAN_I2C_format) {
		i2c_master_send(client, cmd_bc, 4);
		rc = i2c_master_recv(client, buf_recv, 4);
		printk ("[%s-%d] boot code version  %02X %02X %02X %02X\n", __func__, __LINE__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);

		i2c_master_send(client, cmd_x, 4);
		rc = i2c_master_recv(client, buf_recv, 4);
		xResolution = (((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4));
		printk ("[%s-%d] x resolution: %d \n", __func__, __LINE__, xResolution);

		i2c_master_send(client, cmd_y, 4);
		rc = i2c_master_recv(client, buf_recv, 4);
		yResolution =  (((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4));
		printk ("[%s-%d] y resolution: %d \n", __func__, __LINE__, yResolution);

		__elan_touch_calibration (client);
		return 4;
	}

hand_shake_failed:
	return rc;
}

static int elan_touch_recv_data(struct i2c_client *client, uint8_t *buf)
{
	int rc, bytes_to_recv = IDX_PACKET_SIZE;

	if (buf == NULL){
		return -EINVAL;
	}
	memset(buf, 0, bytes_to_recv);
	rc = i2c_master_recv(client, buf, bytes_to_recv);
	if (rc != bytes_to_recv) {
		return -EINVAL;
	}

	return rc;
}

static int gQueueRear, gQueueFront, touch_failed_count;
extern void ntx_gpio_touch_reset(void);

void elan_touch_enqueue (void)
{
	if (0 < elan_touch_recv_data(elan_touch_data.client, gElanBuffer+(gQueueRear*IDX_PACKET_SIZE))) {
		if (((gQueueRear+1)%IDX_QUEUE_SIZE) == gQueueFront)
			printk ("[%s-%d] touch queue full\n",__func__,__LINE__);
		else
			gQueueRear = (gQueueRear+1)%IDX_QUEUE_SIZE;
		touch_failed_count = 0;
	}
	else {
		printk ("[%s-%d] !!!!!!!!!!!!!!!!!!!!!!!!!! touch communication failed !!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__,__LINE__);
		if (3 < touch_failed_count++) {
			ntx_gpio_touch_reset ();
			touch_failed_count = 0;
		}
#ifdef	_WITH_DELAY_WORK_
		schedule_delayed_work(&elan_touch_data.work, 1);
#else
		queue_work(elan_wq, &elan_touch_data.work);
#endif			
	}
}

static void elan_touch_report_data(struct i2c_client *client, uint8_t *buf)
{
	int i, major, minor;
	
	switch (buf[0]) {
	case idx_coordinate_packet:
	case multi_coordinate_packet: {
		static uint16_t last_x, last_y, last_x2, last_y2;
		uint16_t x1, x2, y1, y2;
		uint8_t finger_stat;
		static uint8_t finger1_index=1, finger2_index=4;

		if(1==gELAN_I2C_format) {
			finger_stat = (buf[idx_finger_state] & 0x03);		// for elan old 5A protocol the finger ID is 0x06
		}
		else if(2==gELAN_I2C_format) {
			finger_stat = (buf[1] & 0x03);  // check 2 fingers out of 10
			finger1_index=3;
			finger2_index=6;
		}
		else {
			finger_stat = (buf[idx_finger_state] & 0x06) >> 1;
		}
		if (finger_stat == 0) {
			if (g_touch_pressed & 1) {
				DBG_MSG ("[%s-%d] finger 1 up (%d, %d)\n", __func__, __LINE__, last_x, last_y);
#ifdef ELAN_TOUCH_MT_EVENT
				input_report_abs(elan_touch_data.input, ABS_MT_TRACKING_ID, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_WIDTH_MAJOR, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, last_x);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, last_y);
				input_mt_sync (elan_touch_data.input);
#endif			
				input_report_abs(elan_touch_data.input, ABS_X, last_x);
				input_report_abs(elan_touch_data.input, ABS_Y, last_y);
				input_report_abs(elan_touch_data.input, ABS_PRESSURE, 0);	// Joseph 20101023
				input_report_key(elan_touch_data.input, BTN_TOUCH, 0);
			}
#ifdef ELAN_TOUCH_MT_EVENT
			if (g_touch_pressed & 2) {
				DBG_MSG ("[%s-%d] finger 2 up (%d, %d)\n", __func__, __LINE__, last_x2, last_y2);
				input_report_abs(elan_touch_data.input, ABS_MT_TRACKING_ID, 1);
				input_report_abs(elan_touch_data.input, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_WIDTH_MAJOR, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, last_x2);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, last_y2);
				input_mt_sync (elan_touch_data.input);
			}
#endif			
			g_touch_pressed = 0;
		} 
		//else if (finger_stat == 1) {
		else {
			if(elan_touch_parse_xy(&buf[finger1_index], &x1, &y1)){
#ifdef ELAN_TOUCH_MT_EVENT
				input_report_abs(elan_touch_data.input, ABS_MT_TRACKING_ID, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(elan_touch_data.input, ABS_MT_WIDTH_MAJOR, 1);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, x1);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, y1);
				input_mt_sync (elan_touch_data.input);
#endif
				input_report_abs(elan_touch_data.input, ABS_X, x1);
				input_report_abs(elan_touch_data.input, ABS_Y, y1);
				input_report_abs(elan_touch_data.input, ABS_PRESSURE, 1024);	// Joseph 20101023
				input_report_key(elan_touch_data.input, BTN_TOUCH, 1);
				last_x = x1;
				last_y = y1;
				DBG_MSG ("[%s-%d] finger 1 down (%d, %d).\n", __func__, __LINE__, x1, y1);
				g_touch_pressed |= 1;
			}
			else if (g_touch_pressed & 1) {
#ifdef ELAN_TOUCH_MT_EVENT
				input_report_abs(elan_touch_data.input, ABS_MT_TRACKING_ID, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_WIDTH_MAJOR, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, last_x);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, last_y);
				input_mt_sync (elan_touch_data.input);
#endif
				input_report_abs(elan_touch_data.input, ABS_X, last_x);
				input_report_abs(elan_touch_data.input, ABS_Y, last_y);
				DBG_MSG ("[%s-%d] finger 1 up (%d, %d).\n", __func__, __LINE__, last_x, last_y);
				input_report_abs(elan_touch_data.input, ABS_PRESSURE, 0);	// Joseph 20101023
				input_report_key(elan_touch_data.input, BTN_TOUCH, 0);
				g_touch_pressed &= ~1;
			
			}
			
#ifdef ELAN_TOUCH_MT_EVENT
			if(elan_touch_parse_xy(&buf[finger2_index], &x2, &y2)){
				DBG_MSG ("[%s-%d] finger 2 down (%d, %d).\n", __func__, __LINE__, x2, y2);
				input_report_abs(elan_touch_data.input, ABS_MT_TRACKING_ID, 1);
				input_report_abs(elan_touch_data.input, ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(elan_touch_data.input, ABS_MT_WIDTH_MAJOR, 1);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, x2);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, y2);
				input_mt_sync (elan_touch_data.input);
				last_x2 = x2;
				last_y2 = y2;
				g_touch_pressed |= 2;
			}
			else if (g_touch_pressed & 2) {
				DBG_MSG ("[%s-%d] finger 2 up (%d, %d).\n", __func__, __LINE__, last_x2, last_y2);
				input_report_abs(elan_touch_data.input, ABS_MT_TRACKING_ID, 1);
				input_report_abs(elan_touch_data.input, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_WIDTH_MAJOR, 0);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, last_x2);
				input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, last_y2);
				input_mt_sync (elan_touch_data.input);
				g_touch_pressed &= ~2;
			}
#endif			
		}
		input_sync(elan_touch_data.input);
		schedule();	// Joseph 20101023
		break;
	}
	
	case hello_packet:
		printk ("[%s-%d] Hello packet received!\n", __func__, __LINE__);
		__elan_touch_calibration (client);
		break;
	case version_id_packet:
		major = ((buf[1] & 0x0f) << 4) | ((buf[2] & 0xf0) >> 4);
		minor = ((buf[2] & 0x0f) << 4) | ((buf[3] & 0xf0) >> 4);
		if( (buf[1]>>4) == 0xf) {
			g_fw_id = major << 8 | minor;
//			printk ("[%s-%d] firmware id %04X\n", __func__, __LINE__, g_fw_id);
		} else if ( (buf[1]>>4) == 0x0 ) {
			g_fw_version = major << 8 | minor;
//			printk ("[%s-%d] firmware version %04X\n", __func__, __LINE__, g_fw_version);
		}
		break;
	default:
		printk ("[%s-%d] undefined packet 0x%02X.\n", __func__, __LINE__, buf[0]);
		for (i=0; i<IDX_PACKET_SIZE; i++) {
			printk ("0x%02X ", buf[i]);
			if (idx_coordinate_packet == buf[i]) {
				printk ("[%s-%d] idx_coordinate_packet found in %d\n", __func__, __LINE__, i);
				i2c_master_recv(client, buf, IDX_PACKET_SIZE-i);
			}
		}
		printk ("\n");
		break;
	}
}

static void elan_touch_work_func(struct work_struct *work)
{
	elan_touch_enqueue ();	
	while (gQueueRear != gQueueFront){
		elan_touch_report_data(elan_touch_data.client, gElanBuffer+(gQueueFront*IDX_PACKET_SIZE));
		gQueueFront = (gQueueFront+1)%IDX_QUEUE_SIZE;
	}
#ifdef	_WITH_DELAY_WORK_
	if (g_touch_pressed || !elan_touch_detect_int_level ())
		schedule_delayed_work(&elan_touch_data.work, 1);
	else
		g_touch_triggered = 0;
#else
	g_touch_triggered = 0;
#endif			
}

static irqreturn_t elan_touch_ts_interrupt(int irq, void *dev_id)
{
	g_touch_triggered = 1;
#ifdef	_WITH_DELAY_WORK_
	schedule_delayed_work(&elan_touch_data.work, 0);
#else
//	disable_irq(elan_touch_data.client->irq);
//	elan_touch_enqueue ();	
//	enable_irq(elan_touch_data.client->irq);
	queue_work(elan_wq, &elan_touch_data.work);
#endif			

	return IRQ_HANDLED;
}

void elan_touch_ts_triggered(void)
{
	g_touch_triggered = 1;
//	elan_touch_enqueue ();
#ifdef	_WITH_DELAY_WORK_
	schedule_delayed_work(&elan_touch_data.work, 0);
#else
	queue_work(elan_wq, &elan_touch_data.work);
#endif			
}

static enum hrtimer_restart elan_touch_timer_func(struct hrtimer *timer)
{
	queue_work(elan_wq, &elan_touch_data.work);
	hrtimer_start(&elan_touch_data.timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static int elan_touch_register_interrupt(struct i2c_client *client)
{
	int err = 0;

	if (client->irq) {
		elan_touch_data.use_irq = 1;
		err = request_irq(client->irq, elan_touch_ts_interrupt, IRQF_TRIGGER_FALLING,
				  ELAN_TS_NAME, &elan_touch_data);

		if (err < 0) {
			printk("%s(%s): Can't allocate irq %d\n", __FILE__, __func__, client->irq);
			elan_touch_data.use_irq = 0;
		}
//		enable_irq_wake (client->irq);
	}

	if (!elan_touch_data.use_irq) {
		hrtimer_init(&elan_touch_data.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		elan_touch_data.timer.function = elan_touch_timer_func;
		hrtimer_start(&elan_touch_data.timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

	printk("elan ts starts in %s mode.\n",	elan_touch_data.use_irq == 1 ? "interrupt":"polling");
	
	return 0;
}

extern int gSleep_Mode_Suspend;
static int gTouchDisabled;
static int elan_touch_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (g_touch_pressed || g_touch_triggered || !elan_touch_detect_int_level ()) 
	{
		elan_touch_ts_triggered ();
		printk ("[%s-%d] elan touch event not processed.\n",__func__,__LINE__);
		return -1;
	}
	if (gSleep_Mode_Suspend) {
//		disable_irq_wake (elan_touch_data.client->irq);
		free_irq(elan_touch_data.client->irq, &elan_touch_data);
		gTouchDisabled = 1;
		isCalibrated = 0;
	}
	else
		enable_irq_wake (elan_touch_data.client->irq);
	return 0;
}

static int elan_touch_resume(struct platform_device *pdev)
{
	if (!elan_touch_detect_int_level ()) {
		elan_touch_ts_triggered ();
	}
	if (gSleep_Mode_Suspend && gTouchDisabled)
//		enable_irq_wake (elan_touch_data.client->irq);
		request_irq(elan_touch_data.client->irq, elan_touch_ts_interrupt, IRQF_TRIGGER_FALLING, ELAN_TS_NAME, &elan_touch_data);
	else
		disable_irq_wake (elan_touch_data.client->irq);
	gTouchDisabled = 0;

	return 0;
}

#define PAGERETRY  30
#define IAPRESTART 5

enum
{
	PageSize		= 132,
	PageNum		    = 249,
	ACK_Fail		= 0x00,
	ACK_OK			= 0xAA,
	ACK_REWRITE		= 0x55,
};

void print_progress(int page, int ic_num, int j)
{
	int i, percent,page_tatol,percent_tatol;
	char str[256];
	str[0] = '\0';
	for (i=0; i<((page)/10); i++) {
		str[i] = '#';
		str[i+1] = '\0';
	}
	
	page_tatol=page+249*(ic_num-j);
	percent = ((100*page)/(249));
	percent_tatol = ((100*page_tatol)/(249*ic_num));

	if ((page) == (249))
		percent = 100;

	if ((page_tatol) == (249*ic_num))
		percent_tatol = 100;		

	printk("\rprogress %s| %d%%", str, percent);
	
	if (page == (249))
		printk("\n");
}

int WritePage(uint8_t * szPage, int byte)
{
	int len = 0;

	len = i2c_master_send(elan_touch_data.client, szPage,  byte);
	if (len != byte) 
	{
		printk("[ELAN] ERROR: write page error, write error. byte %d, len=%d\r\n", byte, len);
		return -1;
	}

	return 0;
}

int CheckIapMode(void)
{
	char buff[4] = {0},len = 0;
	int i;

//Check Master & Slave is "55 aa 33 cc"
		for (i=0; i<10;i++) {
			mdelay (10);
			len=i2c_master_recv(elan_touch_data.client, buff, sizeof(buff));
			if (len != sizeof(buff)) 
			{
				printk("[ELAN] ERROR: read data error, write  error. len=%d\r\n", len);
				continue;
			}
			else
				break;
		}
		
		if (len != sizeof(buff)) 
		{
			printk("[ELAN] ERROR: read data error, write  error. len=%d\r\n", len);
			return -1;
		}
		else
		{
			printk("[ELAN] CheckIapMode= 0x%x 0x%x 0x%x 0x%x\r\n", buff[0], buff[1], buff[2], buff[3]);
			if (buff[0] == 0x55 && buff[1] == 0xaa && buff[2] == 0x33 && buff[3] == 0xcc)
			{
				//printf("[ELAN] ic = %2x CheckIapMode is 55 aa 33 cc\n", data);
				//break;
			}
			else// if ( j == 9 )
			{
				printk("[ELAN] ERROR: CheckIapMode error\n");
				return -1;
			}
		}

	return 0;
}

int GetAckData( void )
{
	int len = 0;

	char buff[2] = {0};
	
	len=i2c_master_recv(elan_touch_data.client, buff, sizeof(buff));
	if (len != sizeof(buff)) {
		printk("[ELAN] ERROR: read data error, write 50 times error. len=%d\r\n", len);
		return -1;
	}

//	printk("[ELAN] GetAckData:%x,%x\n",buff[0],buff[1]);
	if (buff[0] == 0xaa) 
		return ACK_OK;
	else if (buff[0] == 0x55 && buff[1] == 0x55)
		return ACK_REWRITE;
	else
		return ACK_Fail;
}

static ssize_t fwupg_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	unsigned char isp_cmd[] = {0x54, 0x00, 0x12, 0x34};
	unsigned char data = 0x15;		// i2c address of touch controller.
	int res = 0,ic_num = 1;
	int rewriteCnt = 0; //rewriteCnt for PAGE_REWRITE
	int restartCnt = 0; // For IAP_RESTART
	
	uint8_t recovery_buffer[4] = {0};
	int byte_count;
	uint8_t *szBuff = NULL;
	int curIndex = 0;
	int pageIndex;
	
	static unsigned char isp_buf[PageSize];
	static int iPage, ispIndex, total;

	total += count;
	if (!iPage) {
		total = count;
//		disable_irq (elan_touch_data.client->irq);
	
		if(RECOVERY != 0x80)
		{
			printk("[ELAN] Firmware upgrade normal mode !\n");
			ntx_gpio_touch_reset ();
			mdelay (500);
//			__hello_packet_handler (elan_touch_data.client);

			for (restartCnt=0; restartCnt <= 10; restartCnt++) {
				if (10 == restartCnt) 
					return -1;
					
				if (sizeof(isp_cmd) != i2c_master_send(elan_touch_data.client, isp_cmd, sizeof(isp_cmd))) {
					printk ("[%s-%d] failed sending isp command.\n",__func__,__LINE__);
		//			enable_irq (elan_touch_data.client->irq);
					mdelay (50);
					continue;
				}
				if (0 == CheckIapMode ()) {
					break;
				}
				mdelay (50);
			}
		} else {
			printk("[ELAN] Firmware upgrade recovery mode !\n");
		}
		
		if(sizeof(data) != i2c_master_send(elan_touch_data.client, &data,  sizeof(data)))
		{
			printk("[ELAN] dummy error.\n");
		}	
		mdelay(50);

		iPage = 1;
		memcpy (isp_buf, buf, PageSize);
		pageIndex = PageSize;
	}
	else {
//		printk ("[%s-%d] ispIndex %d\n",__func__,__LINE__, ispIndex);
		memcpy (&isp_buf[ispIndex], buf, (PageSize-ispIndex));
		pageIndex = PageSize-ispIndex;
		if (PageSize > (count+ispIndex)) {
			 ispIndex += count;
			 return count;
		}
	}
	
	// Start IAP
	for(; (iPage <= PageNum) && (pageIndex <= count); iPage++ ) 
	{
PAGE_REWRITE:
#if 1 // 8byte mode
		// 8 bytes
		//szBuff = fw_data + ((iPage-1) * PageSize); 
		curIndex = 0;
		for(byte_count=1;byte_count<=17;byte_count++)
		{
			szBuff = isp_buf + curIndex;
			if(byte_count!=17)
			{		
				curIndex += 8;
				res = WritePage(szBuff, 8);
			}
			else
			{
				curIndex += 4;
				res = WritePage(szBuff, 4); 
			}
		} // end of for(byte_count=1;byte_count<=17;byte_count++)
#else // 132byte mode		
		szBuff = buf + curIndex;
		curIndex =  curIndex + PageSize;
		res = WritePage(szBuff, PageSize);
#endif
		if(iPage==249 || iPage==1)
		{
			mdelay(600); 			 
		}
		else
		{
			mdelay(50); 			 
		}	
		res = GetAckData();
		if (ACK_OK != res) 
		{
			mdelay(50); 
			printk("[ELAN] ERROR: GetAckData fail! res=%d\r\n", res);
			if ( res == ACK_REWRITE ) 
			{
				rewriteCnt++;
				if (rewriteCnt == PAGERETRY)
				{
					printk("[ELAN] ID 0x%02x %dth page ReWrite %d times fails!\n", data, iPage, PAGERETRY);
					ntx_gpio_touch_reset ();
					iPage = 0;
					return -1;
				}
				else
				{
					printk("[ELAN] ---%d--- page ReWrite %d times!\n",  iPage, rewriteCnt);
					goto PAGE_REWRITE;
				}
			}
			else
			{
				ntx_gpio_touch_reset ();
				iPage = 0;
				return -1;
			}
		}
		else
		{       
//			printk("  data : 0x%02x ",  data);  
//			printk("  iPage : %d index %d ",  iPage, pageIndex);  
			rewriteCnt=0;
			printk("\n");  
			print_progress(iPage,ic_num,0);
			if (count > pageIndex) {
				memcpy (isp_buf, buf+pageIndex, PageSize);
				if (count < (pageIndex+PageSize)) {
					ispIndex = count - pageIndex;
					printk("  %d bytes left for next transfer, page %d \n",  ispIndex, iPage);  
				}
			}
			pageIndex += PageSize;
		}

		mdelay(10);
	} // end of for(iPage = 1; iPage <= PageNum; iPage++)
	if (iPage <= PageNum)
		return count;

	iPage = 0;
#if 1
	ntx_gpio_touch_reset ();
	printk("[ELAN] read Hello packet data!\n"); 	  
	res= __hello_packet_handler(elan_touch_data.client);
	if (res > 0)
		printk("[ELAN] Update ALL Firmware successfully!\n");
		
#endif
//	enable_irq (elan_touch_data.client->irq);
	return count;
}

static ssize_t version_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	g_fw_version = 0;
	uint8_t getFirmwareVer[] = {0x53,0x00,0x00,0x01};

	i2c_master_send(elan_touch_data.client, getFirmwareVer, 4);
	msleep(20);
	return sprintf(buf, "%d",g_fw_version);
}

static ssize_t firmwareId_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	g_fw_id = 0;
	uint8_t getFirmwareId[] = {0x53,0xF0,0x00,0x01};

	i2c_master_send(elan_touch_data.client, getFirmwareId, 4);
	msleep(20);
	return sprintf(buf, "%d",g_fw_id);
}

static DEVICE_ATTR(fwupg, 0666, NULL, fwupg_write);
static DEVICE_ATTR(version, S_IRUGO, version_read, NULL);
static DEVICE_ATTR(firmwareId, S_IRUGO, firmwareId_read, NULL);

static const struct attribute *sysfs_touch_attrs[] = {
	&dev_attr_fwupg.attr,
	&dev_attr_version.attr,
	&dev_attr_firmwareId.attr,
	NULL,
};


static int elan_touch_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	elan_touch_data.client = client;
	elan_touch_data.intr_gpio = (client->dev).platform_data;
	
	err = __elan_touch_init(client);
	if (err < 0) {
	    printk("Read Hello Packet Fail\n");
	    return err;
	}
	
	strlcpy(client->name, ELAN_TS_NAME, I2C_NAME_SIZE);

#ifdef	_WITH_DELAY_WORK_
	INIT_DELAYED_WORK(&elan_touch_data.work, elan_touch_work_func);
#else
	elan_wq = create_singlethread_workqueue("elan_wq");
	if (!elan_wq) {
		err = -ENOMEM;
		goto fail;
	}
	INIT_WORK(&elan_touch_data.work, elan_touch_work_func);
#endif			
	
	elan_touch_data.input = input_allocate_device();
	if (elan_touch_data.input == NULL) {
		err = -ENOMEM;
		goto fail;
	}

	elan_touch_data.input->name = ELAN_TS_NAME;
	elan_touch_data.input->id.bustype = BUS_I2C;
	
	set_bit(EV_SYN, elan_touch_data.input->evbit);
	
	set_bit(EV_KEY, elan_touch_data.input->evbit);
	set_bit(BTN_TOUCH, elan_touch_data.input->keybit);
	set_bit(BTN_2, elan_touch_data.input->keybit);
	
	set_bit(EV_ABS, elan_touch_data.input->evbit);
	set_bit(ABS_X, elan_touch_data.input->absbit);
	set_bit(ABS_Y, elan_touch_data.input->absbit);
	set_bit(ABS_PRESSURE, elan_touch_data.input->absbit);
	set_bit(ABS_HAT0X, elan_touch_data.input->absbit);
	set_bit(ABS_HAT0Y, elan_touch_data.input->absbit);

	if(1==gptHWCFG->m_val.bDisplayResolution) {
		// 1024x758 .
		ELAN_TS_WIDTH=1024;
		ELAN_TS_HIGHT=758;
	}
	else if(2==gptHWCFG->m_val.bDisplayResolution) {
		// 1024x768
		ELAN_TS_WIDTH=1024;
		ELAN_TS_HIGHT=768;
	}
	else if(3==gptHWCFG->m_val.bDisplayResolution) {
		// 1440x1080
		ELAN_TS_WIDTH=1440;
		ELAN_TS_HIGHT=1080;
	}
	else if(5==gptHWCFG->m_val.bDisplayResolution) {
		// 1448x1072
		ELAN_TS_WIDTH=1448;
		ELAN_TS_HIGHT=1072;
	}
	else if(6==gptHWCFG->m_val.bDisplayResolution) {
		// 1600x1200
		ELAN_TS_WIDTH=1600;
		ELAN_TS_HIGHT=1200;
	}
	else {
		// 800x600 
		ELAN_TS_WIDTH=800;
		ELAN_TS_HIGHT=600;
	}

	ELAN_TS_X_MAX=ELAN_TS_WIDTH;
	ELAN_TS_Y_MAX=ELAN_TS_HIGHT;

	input_set_abs_params(elan_touch_data.input, ABS_X, 0, ELAN_TS_X_MAX, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_Y, 0, ELAN_TS_Y_MAX, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_PRESSURE, 0, 2048, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_HAT0X, 0, ELAN_TS_X_MAX, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_HAT0Y, 0, ELAN_TS_Y_MAX, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_MT_POSITION_X, 0, ELAN_TS_X_MAX, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_MT_POSITION_Y, 0, ELAN_TS_Y_MAX, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_MT_TRACKING_ID, 0, 2, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_MT_TOUCH_MAJOR, 0, 2, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_MT_WIDTH_MAJOR, 0, 2, 0, 0);

	err = input_register_device(elan_touch_data.input);
	if (err < 0) {
		goto fail;
	}

	err = sysfs_create_files(&elan_touch_data.input->dev.kobj, sysfs_touch_attrs);
	if (err) {
		pr_debug("Can't create device file!\n");
		return -ENODEV;
	}
	
	elan_touch_register_interrupt(elan_touch_data.client);

	return 0;

fail:
	input_free_device(elan_touch_data.input);
	if (elan_wq)
		destroy_workqueue(elan_wq);
	return err;
}

static int elan_touch_remove(struct i2c_client *client)
{
	if (elan_wq)
		destroy_workqueue(elan_wq);

	kfree(gElanBuffer);

	input_unregister_device(elan_touch_data.input);

	if (elan_touch_data.use_irq)
		free_irq(client->irq, client);
	else
		hrtimer_cancel(&elan_touch_data.timer);
	return 0;
}

/* -------------------------------------------------------------------- */
static const struct i2c_device_id elan_touch_id[] = {
    {"elan-touch", 0 },
	{ }
};
	
static struct i2c_driver elan_touch_driver = {
	.probe		= elan_touch_probe,
	.remove		= elan_touch_remove,
	.suspend	= elan_touch_suspend,
	.resume		= elan_touch_resume,
	.id_table	= elan_touch_id,
	.driver		= {
		.name = "elan-touch",
		.owner = THIS_MODULE,
	},
};

static int __init elan_touch_init(void)
{
	return i2c_add_driver(&elan_touch_driver);
}

static void __exit elan_touch_exit(void)
{
	i2c_del_driver(&elan_touch_driver);
}

module_init(elan_touch_init);
module_exit(elan_touch_exit);

MODULE_AUTHOR("Stanley Zeng <stanley.zeng@emc.com.tw>");
MODULE_DESCRIPTION("ELAN Touch Screen driver");
MODULE_LICENSE("GPL");

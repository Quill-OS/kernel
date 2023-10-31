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
#include <linux/input/mt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#ifdef MSP430_HOMEPAD
#include "../../../drivers/misc/ntx-misc.h"
#endif

#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"
extern volatile unsigned gMX6SL_IR_TOUCH_RST;

//#define _NEW_FIRMWARE_

//#ifdef _NEW_FIRMWARE_
static unsigned int xResolution;
static unsigned int yResolution;
static int isCalibrated;
static int gELAN_I2C_format = 0;
static int g_fw_id, g_fw_version, g_sensor_option;
static int g_manual_iap_flow = -1;

static uint8_t *gElanBuffer;
static uint8_t *packet_buf;

//#endif
//#define _WITH_DELAY_WORK_
#define _THREADED_IRQ_
static const char ELAN_TS_NAME[]	= "elan-touch";
#define TS_POLL_PERIOD	msecs_to_jiffies(10) /* ms delay between samples */

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
	int rst_gpio;
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

#ifdef MSP430_HOMEPAD
extern unsigned int msp430_read(unsigned int reg);
extern int is_touch_resetting(void);
#endif
/*--------------------------------------------------------------*/
static int elan_touch_detect_int_level(void)
{
	return gpio_get_value(elan_touch_data.intr_gpio);
}

static int elan_touch_detect_rst_level(void)
{
	return gpio_get_value(elan_touch_data.rst_gpio);
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
		printk ("[elan] %s: touch_int not low.\n", __func__);
		return -EINVAL;
	}

	rc = i2c_master_recv(client, buf_recv, 8);
	if (rc != 8) {
		printk ("[elan] %s: i2c receive failed.\n", __func__);
		return -1;	// Joseph 20101025
	} else {
		printk("[elan] %s: hello packet %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3] , buf_recv[4], buf_recv[5], buf_recv[6], buf_recv[7]);

		if(buf_recv[0]==0x55 && buf_recv[1]==0x55 && buf_recv[2]==0x80 && buf_recv[3]==0x80)
		{
			RECOVERY=0x80;
			g_sensor_option = ((buf_recv[4] & 0xff) <<8 ) | (buf_recv[5] & 0xff);
			printk ("[%s-%d] in RECOVERY MODE, Sensor option = 0x%04X\n", __func__, __LINE__, g_sensor_option);
			return RECOVERY;
		} else {
			RECOVERY=0x00;
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
	if( 0==1 ) {  // resolution 800*600
		*x = 799 - (_x * 799 / 1088);
		*y = (_y * 599 / 768);
	}
	else if( 6==1 ){  // 1600*1200
		*x = (_x * screenX / elanInternalX);
		if (1) {
			*y = screenX - (_y * screenY / elanInternalY);
		} else {
			*y = _y * screenY / elanInternalY;
		}
		if (0x2422==g_fw_id) {
			*y = screenX - *y;
		}
	}
	else {
		*x = _y * screenY / elanInternalY;
  		if (1)
			*y = screenX - (_x * screenX / elanInternalX);
		else
			*y = (_x * screenX / elanInternalX);
	}
#else if
	if(1==1) {  // resolution 1024*758
  		*x = _y * 1023 / rangeY;
  		if (1)
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

	if (isCalibrated)
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

static void __elan_10A6_calibration (struct i2c_client *client)
{
	int rc;
	uint8_t buf_recv[6] = { 0 };
	uint8_t cmd_calib0[] = {0x97 ,0xFF ,0xF1 ,0x00 ,0x00 ,0xF1} ;//delay 100ms
	uint8_t cmd_calib1[] = {0x97 ,0x04 ,0x88 ,0x00 ,0x00 ,0xF1} ;//delay 10ms
	uint8_t cmd_calib2[] = {0x97 ,0x04 ,0x41 ,0x01 ,0x27 ,0xF1} ;//delay 10ms
	uint8_t cmd_calib3[] = {0x97 ,0x04 ,0x42 ,0x01 ,0x27 ,0xF1} ;//delay 10ms
	uint8_t cmd_calib4[] = {0x97 ,0x04 ,0xb2 ,0x00 ,0x00 ,0xF1} ;//delay 10ms
	uint8_t cmd_calib5[] = {0x97 ,0x04 ,0x54 ,0x00 ,0x06 ,0xF1} ;//delay 10ms
	uint8_t cmd_calib6[] = {0x97 ,0x04 ,0x55 ,0x03 ,0x84 ,0xF1} ;//delay 10ms
	uint8_t cmd_calib7[] = {0x97 ,0x04 ,0x56 ,0x03 ,0x20 ,0xF1} ;//delay 10ms
	uint8_t cmd_calib8[] = {0x97 ,0x04 ,0x75 ,0x02 ,0x58 ,0xF1} ;//delay 10ms
	uint8_t cmd_calib9[] = {0x97 ,0x04 ,0xd2 ,0x00 ,0x0C ,0xF1} ;//delay 10ms
	uint8_t cmd_calib10[] = {0x97 ,0xFF ,0xF0 ,0x00 ,0x00 ,0xF1} ;//delay 200ms

	if (isCalibrated)
		return;

	i2c_master_send(client, cmd_calib0, 6);
	msleep (100);
	i2c_master_send(client, cmd_calib1, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib2, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib3, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib4, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib5, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib6, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib7, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib8, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib9, 6);
	msleep (10);
	i2c_master_send(client, cmd_calib10, 6);
	msleep (200);

	isCalibrated=1;
	printk ("[%s-%d] 10A6 calibration done. \n", __func__, __LINE__);
}

/*	__elan_touch_init -- hand shaking with touch panel
 *
 *	1.recv hello packet
 */
static int __elan_touch_init(struct i2c_client *client)
{
	int rc;
	int major,minor;
	int retry_cnt=50;
	uint8_t buf_recv[4] = { 0 };
	uint8_t getFirmwareVer[] = {0x53,0x00,0x00,0x01};
	uint8_t getFirmwareId[] = {0x53,0xF0,0x00,0x01};
	uint8_t cmd_x[] = {0x53, 0x60, 0x00, 0x00}; /*Get x resolution*/
	uint8_t cmd_y[] = {0x53, 0x63, 0x00, 0x00}; /*Get y resolution*/
	uint8_t cmd_bc[] = {0x53, 0x01, 0x00, 0x01};/* Get BootCode Version*/

	gpio_set_value(elan_touch_data.rst_gpio, 0);
	msleep(10);
	gpio_set_value(elan_touch_data.rst_gpio, 1);

	do {
		rc = __hello_packet_handler(client);
	} while (rc<0 && retry_cnt--);

	if (rc < 0)
		goto hand_shake_failed;

	i2c_master_send(client, getFirmwareVer, 4);
	rc = i2c_master_recv(client, buf_recv, 4);
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	g_fw_version = major << 8 | minor;
	printk ("[%s-%d] firmware version %02X %02X %02X %02X (%04X)\n", __func__, __LINE__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3],g_fw_version);

	i2c_master_send(client, getFirmwareId, 4);
	rc = i2c_master_recv(client, buf_recv, 4);
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	g_fw_id = major << 8 | minor;
	printk ("[%s-%d] firmware ID %02X %02X %02X %02X (%04X)\n", __func__, __LINE__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3],g_fw_id);

	if( 0x2422==g_fw_id || 0x0E00==g_fw_id){  //13.3"
		gELAN_I2C_format = 2;
	}
	else if( g_fw_id!=0x10D3 ){
		//printk("%s():fw_id=0x%x,pcbid=%d\n",__FUNCTION__,fw_id,(int)gptHWCFG->m_val.bPCB);
		//printk("%s():pcbid=%d\n",__FUNCTION__,(int)gptHWCFG->m_val.bPCB); // for me it's 81
		if (40==81 ||
				49==81 ||
				52==81)
		{
#ifdef MSP430_HOMEPAD
			// E60Q5X , E60QD2 , E60QG2
			msp430_homepad_sensitivity_set(0x05);
#endif
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
	packet_buf = kmalloc(IDX_PACKET_SIZE*sizeof(uint8_t),GFP_KERNEL);

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

		if( (0x0DE1==g_fw_id && (0x10A6==g_fw_version||0x10A5==g_fw_version)) ||
			(0x00B6==g_fw_id && 0x10A1==g_fw_version) ) {
			__elan_10A6_calibration (client);
		} else if(0x0499==g_fw_id){
			__elan_touch_calibration (client);
		}
		return 4;
	}

	hand_shake_failed:
	gpio_set_value(elan_touch_data.rst_gpio, 0);
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
#if 0
extern void ntx_gpio_touch_reset(void);
extern void ntx_gpio_touch_easyreset(void);
#else
void ntx_gpio_touch_easyreset(void)
{
	gpio_set_value(elan_touch_data.rst_gpio, 0);
	msleep(10);
	gpio_set_value(elan_touch_data.rst_gpio, 1);
}

#define ntx_gpio_touch_reset	ntx_gpio_touch_easyreset
#endif

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
#ifdef _WITH_DELAY_WORK_
		schedule_delayed_work(&elan_touch_data.work, 1);
#else
		queue_work(elan_wq, &elan_touch_data.work);
#endif
	}
}

static unsigned long gLastTouchReportTime;
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
		gLastTouchReportTime = jiffies;
		schedule();	// Joseph 20101023
		break;
	}

	case hello_packet:
		printk("[%s-%d] Hello packet %02x:%02X:%02x:%02x:%02x:%02x:%02x:%02x received\n", __func__, __LINE__, buf[0], buf[1], buf[2], buf[3] , buf[4], buf[5], buf[6], buf[7]);
		if(buf[0]==0x55 && buf[1]==0x55 && buf[2]==0x80 && buf[3]==0x80)
		{
			RECOVERY=0x80;
			g_sensor_option = ((buf[4] & 0xff) <<8 ) | (buf[5] & 0xff);
			printk ("[%s-%d] in RECOVERY MODE, Sensor option = 0x%04X\n", __func__, __LINE__, g_sensor_option);
		} else {
			RECOVERY=0x00;
		}
		if( (0x0DE1==g_fw_id && (0x10A6==g_fw_version||0x10A5==g_fw_version)) ||
			(0x00B6==g_fw_id && 0x10A1==g_fw_version) ) {
			__elan_10A6_calibration (client);
		} else if(0x0499==g_fw_id){
			__elan_touch_calibration (client);
		}
		break;
	case version_id_packet:
		if( buf[1]==0xD3) {
//			printk ("[%s-%d] Sensor option %02X %02X %02X %02X\n", __func__, __LINE__, buf[0], buf[1], buf[2], buf[3]);
			g_sensor_option = ((buf[2] & 0xff) <<8 ) | (buf[3] & 0xff);
		} else {
			major = ((buf[1] & 0x0f) << 4) | ((buf[2] & 0xf0) >> 4);
			minor = ((buf[2] & 0x0f) << 4) | ((buf[3] & 0xf0) >> 4);
			if( (buf[1]>>4) == 0xf) {
				g_fw_id = major << 8 | minor;
//				printk ("[%s-%d] firmware id %04X\n", __func__, __LINE__, g_fw_id);
			} else if ( (buf[1]>>4) == 0x0 ) {
				g_fw_version = major << 8 | minor;
//				printk ("[%s-%d] firmware version %04X\n", __func__, __LINE__, g_fw_version);
			}
		}
		break;
	default:
		printk ("[%s-%d] undefined packet 0x%02X.(touch pressed %d)\n", __func__, __LINE__, buf[0], g_touch_pressed);
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
#ifdef _THREADED_IRQ_
	int rc;

	if ( elan_touch_detect_int_level ())
	{
		printk("[elan] Detected the jitter on INT pin\n");
		return IRQ_HANDLED;
	}
#if 0	// In iMX6ULL, output gpio will always return 0 .
	if ( !elan_touch_detect_rst_level () || is_touch_resetting() )
	{
		DBG_MSG ("[elan] Control IC resetting, skip INT handler\n");
		return IRQ_HANDLED;
	}
#endif
	rc = elan_touch_recv_data(elan_touch_data.client, packet_buf);
	if (rc < 0)
	{
		printk("[elan] Received the packet Error.\n");
		return IRQ_HANDLED;
	}
//	printk("[elan_debug] %2x,%2x,%2x,%2x,%2x,%2x,%2x,%2x ....., %2x\n",\
		packet_buf[0],packet_buf[1],packet_buf[2],packet_buf[3],packet_buf[4],\
		packet_buf[5],packet_buf[6],packet_buf[7],packet_buf[17]);

	elan_touch_report_data(elan_touch_data.client, packet_buf);
	g_touch_triggered = 0;
#elif	_WITH_DELAY_WORK_
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
#ifdef _THREADED_IRQ_
	elan_touch_recv_data(elan_touch_data.client, packet_buf);
	elan_touch_report_data(elan_touch_data.client, packet_buf);

	g_touch_triggered = 0;
#elif	_WITH_DELAY_WORK_
	schedule_delayed_work(&elan_touch_data.work, 0);
#else
	queue_work(elan_wq, &elan_touch_data.work);
#endif
}

static enum hrtimer_restart elan_touch_timer_func(struct hrtimer *timer)
{
	if (elan_wq)
		queue_work(elan_wq, &elan_touch_data.work);
	hrtimer_start(&elan_touch_data.timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static int elan_touch_register_interrupt(struct i2c_client *client)
{
	int err = 0;
	if (client->irq) {
		elan_touch_data.use_irq = 1;
#ifdef _THREADED_IRQ_
		if (55==81) {  //E70Q02
			err = request_threaded_irq(client->irq, NULL, elan_touch_ts_interrupt, IRQF_TRIGGER_LOW | IRQF_ONESHOT, ELAN_TS_NAME, &elan_touch_data);
		} else
		{
			err = request_threaded_irq(client->irq, NULL, elan_touch_ts_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ELAN_TS_NAME, &elan_touch_data);
		}
#else
		err = request_irq(client->irq, elan_touch_ts_interrupt, IRQF_TRIGGER_FALLING,
				  ELAN_TS_NAME, &elan_touch_data);
#endif
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

static int gTouchDisabled;
static int elan_touch_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (1) {
		gpio_set_value(elan_touch_data.rst_gpio, 0);
		// Edited by szybet to avoid waking up from sleep by touchscreen. i dont know that i do but it works
		disable_irq_wake (elan_touch_data.client->irq); //
		free_irq(elan_touch_data.client->irq, &elan_touch_data);
		gTouchDisabled = 1;
		isCalibrated = 0;
		g_touch_pressed = 0;
		g_touch_triggered = 0;
	}
	else
	if (g_touch_pressed || g_touch_triggered || !elan_touch_detect_int_level ())
	{
		elan_touch_ts_triggered ();
		printk ("[%s-%d] elan touch event not processed (pressed=0x%x,triggered=%d).\n",__func__,__LINE__,g_touch_pressed,g_touch_triggered);
		return -1;
	}
	else if (gLastTouchReportTime && time_before(jiffies, gLastTouchReportTime+50)) {
		printk ("[%s-%d] touch event just sent %d, %d.\n",__func__,__LINE__,gLastTouchReportTime, jiffies);
		return -1;
	}
	else
		// Edited by szybet to avoid waking up from sleep by touchscreen. i dont know that i do but it works
		enable_irq_wake (elan_touch_data.client->irq); //
	return 0;
}

static int elan_touch_resume(struct platform_device *pdev)
{
	if (!elan_touch_detect_int_level ()) {
		elan_touch_ts_triggered ();
	}
	if (1 && gTouchDisabled) {
		gpio_set_value(elan_touch_data.rst_gpio, 1);
		elan_touch_register_interrupt(elan_touch_data.client);
	}
	else
		// Edited by szybet to avoid waking up from sleep by touchscreen. i dont know that i do but it works
		// disable_irq_wake (elan_touch_data.client->irq);
	gTouchDisabled = 0;

	return 0;
}

#define PAGERETRY  30
#define IAPRESTART 5

enum
{
	PageSize		= 132,
	ACK_Fail		= 0x00,
	ACK_OK			= 0xAA,
	ACK_REWRITE		= 0x55,
};
static int PageNum = 249;

void print_progress(int page, int ic_num, int j)
{
	int i, percent,page_tatol,percent_tatol;
	char str[256];
	str[0] = '\0';
	for (i=0; i<((page)/10); i++) {
		str[i] = '#';
		str[i+1] = '\0';
	}

	page_tatol=page+PageNum*(ic_num-j);
	percent = ((100*page)/(PageNum));
	percent_tatol = ((100*page_tatol)/(PageNum*ic_num));

	if ((page) == (PageNum))
		percent = 100;

	if ((page_tatol) == (PageNum*ic_num))
		percent_tatol = 100;

	printk("\rprogress %s| %d%%", str, percent);

	if (page == (PageNum))
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

static int _sensorOption_read(void)
{
	g_sensor_option = 0;
	uint8_t getSensorOption[] = {0x53,0xd3,0x00,0x01};

	i2c_master_send(elan_touch_data.client, getSensorOption, 4);
	msleep(20);
	return g_sensor_option;
}

static int isNewIAPflow(void)
{
	int newIAPflow = 0;

	if(g_manual_iap_flow>=0) { //manual set
		printk("[elan] Manually set iap flow: %d\n", g_manual_iap_flow);
		newIAPflow = g_manual_iap_flow;
	} else if(RECOVERY==0x80) {  //recovery mode
		switch(g_sensor_option) {
			case 0x10D3:
			case 0x00B6:
			case 0x0499:
			case 0x2422:
			case 0x0D01:
			case 0x0DE1:
			case 0x1B50:
				 //those follow old IAP flow
				break;
			default:
				newIAPflow = 1;
				break;
		}
	} else { //normal mode
		switch(g_fw_id) {
			case 0x10D3:
			case 0x00B6:
			case 0x0499:
			case 0x2422:
			case 0x0D01:
			case 0x0DE1:
			case 0x1B50:
				//those follow old IAP flow
				break;
			default:
				newIAPflow = 1;
				break;
		}
	}
	return newIAPflow;
}

static int needIspCmd(void)
{
	if(RECOVERY==0x80 && !isNewIAPflow()){
		return 0;
	} else {
		return 1;
	}
}

static ssize_t fwupg_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	unsigned char isp_cmd[2][4] = {{0x45, 0x49, 0x41, 0x50}, {0x54, 0x00, 0x12, 0x34}};
	unsigned char data = 0x15;		// i2c address of touch controller.
	int res = 0,ic_num = 1;
	int rewriteCnt = 0; //rewriteCnt for PAGE_REWRITE
	int restartCnt = 0; // For IAP_RESTART

	uint8_t recovery_buffer[4] = {0};
	int byte_count;
	uint8_t *szBuff = NULL;
	int curIndex = 0;
	int pageIndex;
	int isp_cmd_cnt = 0;

	static unsigned char isp_buf[PageSize];
	static int iPage, ispIndex, total;

	total += count;
	if (!iPage) {
		total = count;
//		disable_irq (elan_touch_data.client->irq);

		if(needIspCmd())
		{
			ntx_gpio_touch_easyreset ();
			if(isNewIAPflow())
				mdelay (20);
			else
				mdelay (500);
//			__hello_packet_handler (elan_touch_data.client);

			for (restartCnt=0; restartCnt <= 10; restartCnt++) {
				if (10 == restartCnt && isp_cmd_cnt)
				{
					printk("[ELAN] all IAPflow command fwiled.\n");
					return -1;
				}
				else if (10 == restartCnt)
				{
					printk("[ELAN] newIAPflow command failed, try oldIAPflow command.\n");
					restartCnt = 0;
					isp_cmd_cnt++;
				}

				if (sizeof(isp_cmd[isp_cmd_cnt]) != i2c_master_send(elan_touch_data.client, isp_cmd[isp_cmd_cnt], sizeof(isp_cmd[isp_cmd_cnt]))) {
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
		if(iPage==PageNum || iPage==1)
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
					ntx_gpio_touch_easyreset ();
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
				ntx_gpio_touch_easyreset ();
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
	ntx_gpio_touch_easyreset ();
	printk("[ELAN] read Hello packet data!\n");
	res= __hello_packet_handler(elan_touch_data.client);
	if (res > 0)
		printk("[ELAN] Update ALL Firmware successfully!\n");

#endif
//	enable_irq (elan_touch_data.client->irq);
	return count;
}

static ssize_t manualIapFlow_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	sscanf(buf, "%d", &g_manual_iap_flow);
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

static ssize_t sensorOption_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	if(0x80==RECOVERY){
		return sprintf(buf, "%d", g_sensor_option);
	}
	return sprintf(buf, "%d", _sensorOption_read());
}

static DEVICE_ATTR(fwupg, 0660, NULL, fwupg_write);
static DEVICE_ATTR(manualIapFlow, 0660, NULL, manualIapFlow_set);
static DEVICE_ATTR(version, S_IRUGO, version_read, NULL);
static DEVICE_ATTR(firmwareId, S_IRUGO, firmwareId_read, NULL);
static DEVICE_ATTR(sensorOption, S_IRUGO, sensorOption_read, NULL);

static const struct attribute *sysfs_touch_attrs[] = {
	&dev_attr_fwupg.attr,
	&dev_attr_manualIapFlow.attr,
	&dev_attr_version.attr,
	&dev_attr_firmwareId.attr,
	&dev_attr_sensorOption.attr,
	NULL,
};

static int elan_touch_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct device_node *np = client->dev.of_node;

	if (!np)
		return -ENODEV;

	elan_touch_data.intr_gpio = of_get_named_gpio(np, "gpio_intr", 0);
	if (!gpio_is_valid(elan_touch_data.intr_gpio))
		return -ENODEV;

	err = devm_gpio_request_one(&client->dev, elan_touch_data.intr_gpio,
				GPIOF_IN, "gpio_elan_intr");
	if (err < 0) {
		dev_err(&client->dev,
			"request gpio failed: %d\n", err);
		return err;
	}
	client->irq = gpio_to_irq(elan_touch_data.intr_gpio);

	elan_touch_data.rst_gpio = of_get_named_gpio(np, "gpio_elan_rst", 0);
	if (!gpio_is_valid(elan_touch_data.rst_gpio))
		return -ENODEV;

	err = devm_gpio_request_one(&client->dev, elan_touch_data.rst_gpio,
				GPIOF_OUT_INIT_HIGH, "gpio_elan_rst");
	if (err < 0) {
		dev_err(&client->dev,
			"request gpio failed: %d\n", err);
		return err;
	}
	gpio_set_value(elan_touch_data.rst_gpio, 0);
	msleep(10);
	gpio_set_value(elan_touch_data.rst_gpio, 1);

	elan_touch_data.client = client;

	err = __elan_touch_init(client);
	if (err < 0) {
	    printk("Read Hello Packet Fail\n");
	    return err;
	}

	strlcpy(client->name, ELAN_TS_NAME, I2C_NAME_SIZE);

#ifdef _THREADED_IRQ_
#elif _WITH_DELAY_WORK_
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

	if(1==1) {
		// 1024x758 .
		ELAN_TS_WIDTH=1024;
		ELAN_TS_HIGHT=758;
	}
	else if(2==1) {
		// 1024x768
		ELAN_TS_WIDTH=1024;
		ELAN_TS_HIGHT=768;
	}
	else if(3==1) {
		// 1440x1080
		ELAN_TS_WIDTH=1440;
		ELAN_TS_HIGHT=1080;
	}
	else if(5==1) {
		// 1448x1072
		ELAN_TS_WIDTH=1448;
		ELAN_TS_HIGHT=1072;
	}
	else if(6==1) {
		// 1600x1200
		ELAN_TS_WIDTH=1600;
		ELAN_TS_HIGHT=1200;
	}
	else if(8==1) {
		// 1872x1404
		ELAN_TS_WIDTH=1872;
		ELAN_TS_HIGHT=1404;
	}
	else if(14==1) {
		// 1920x1440
		ELAN_TS_WIDTH=1920;
		ELAN_TS_HIGHT=1440;
	}
	else if(15==1) {
		// 1264x1680
		ELAN_TS_WIDTH=1264;
		ELAN_TS_HIGHT=1680;
	}
	else {
		// 800x600
		ELAN_TS_WIDTH=800;
		ELAN_TS_HIGHT=600;
	}

	ELAN_TS_X_MAX=ELAN_TS_WIDTH;
	ELAN_TS_Y_MAX=ELAN_TS_HIGHT;

	if (0x242B==g_fw_id || 55==81) {  //E70Q02
		PageNum = 377;
	}

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
	kfree(packet_buf);

	input_unregister_device(elan_touch_data.input);

	if (elan_touch_data.use_irq) {
		free_irq(client->irq, &elan_touch_data);
	} else {
		hrtimer_cancel(&elan_touch_data.timer);
	}
	gpio_set_value(elan_touch_data.rst_gpio, 0);
	return 0;
}

/* -------------------------------------------------------------------- */
static const struct i2c_device_id elan_touch_id[] = {
    {"elan-touch", 0 },
	{ }
};

static const struct of_device_id elan_dt_ids[] = {
	{
		.compatible = "elan,elan-touch",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, elan_dt_ids);

static const struct dev_pm_ops elan_dev_pm_ops = {
	.suspend = elan_touch_suspend,
	.resume  = elan_touch_resume,
};

static struct i2c_driver elan_touch_driver = {
	.probe		= elan_touch_probe,
	.remove		= elan_touch_remove,
	.id_table	= elan_touch_id,
	.driver		= {
		.name = "elan-touch",
		.owner = THIS_MODULE,
		.of_match_table = elan_dt_ids,
#ifdef CONFIG_PM
		.pm = &elan_dev_pm_ops,
#endif
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

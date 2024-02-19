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
#include "ntx_hwconfig.h"
extern volatile NTX_HWCONFIG *gptHWCFG;

//#define _WITH_DELAY_WORK_
static const char ELAN_TS_NAME[]	= "elan-touch";
#define TS_POLL_PERIOD	msecs_to_jiffies(10) /* ms delay between samples */

extern int gTSC2004_exist;		// Joseph 20100726
extern int gIsCustomerUi;

//#define DBG_MSG	printk
#define DBG_MSG	

#define tp_int_pin			183

#define ELAN_TS_X_MAX 		1500<<1
#define ELAN_TS_Y_MAX 		1000<<1
#define IDX_QUEUE_SIZE		100
#define IDX_PACKET_SIZE		8

#define ELAN_TOUCH_MT_EVENT

enum {
	hello_packet 			= 0x55,
	idx_coordinate_packet 	= 0x5a,
};

enum {
	idx_finger_state = 7,
};

static struct workqueue_struct *elan_wq;
static uint16_t g_touch_pressed, g_touch_triggered;


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
	int rc;
	uint8_t buf_recv[4] = { 0 };

	rc = elan_touch_poll(client);

	if (rc < 0) {
		return -EINVAL;
	}

	rc = i2c_master_recv(client, buf_recv, 4);

	if (rc != 4) {
//		return rc;
		return -1;	// Joseph 20101025
	} else {
		int i;
		printk("hello packet: [0x%02x 0x%02x 0x%02x 0x%02x]\n",
			buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);

		for (i = 0; i < 4; i++)
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
	int _x,_y;
	_x = (data[0] & 0xf0);
	_x <<= 4;
	_x |= data[1];

	_y = (data[0] & 0x0f);
	_y <<= 8;
	_y |= data[2];
	
	if(1==gptHWCFG->m_val.bDisplayResolution) {  // resolution 1024*758
  	*x = _y * 1023 / 1280;
  	if (gIsCustomerUi) 
			*y = 757 - (_x * 757 / 832);
		else
			*y = (_x * 757 / 832);
	}else{
		*x = 799 - (_x * 799 / 1088);
		*y = (_y * 599 / 768);
	}
	
//	printk ("[%s-%d] (%d, %d) (%d, %d)\n",__func__,__LINE__,_x,_y,*x,*y);
#endif

//	return 0;

	/* returns 0 if no data (0,0) reported by touch controller*/
	/* returns 1 if any data reported 									 			*/ 
	return ( _x || _y );
}

/*	__elan_touch_init -- hand shaking with touch panel
 *
 *	1.recv hello packet
 */
static int __elan_touch_init(struct i2c_client *client)
{
	int rc;
	uint8_t buf_recv[4] = { 0 };
	uint8_t getFirmwareVer[] = {0x53,0x00,0x00,0x01};
	uint8_t getFirmwareId[] = {0x53,0xF0,0x00,0x01};
	
	rc = __hello_packet_handler(client);
	if (rc < 0)
		goto hand_shake_failed;
	
	i2c_master_send(client, getFirmwareVer, 4);
	rc = i2c_master_recv(client, buf_recv, 4);
	printk ("[%s-%d] firmware version %02X %02X %02X %02X \n", __func__, __LINE__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);

	i2c_master_send(client, getFirmwareId, 4);
	rc = i2c_master_recv(client, buf_recv, 4);
	printk ("[%s-%d] firmware ID %02X %02X %02X %02X \n", __func__, __LINE__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);


hand_shake_failed:
	return rc;
}

static int elan_touch_recv_data(struct i2c_client *client, uint8_t *buf)
{
	int rc, bytes_to_recv = IDX_PACKET_SIZE;

	if (buf == NULL)
		return -EINVAL;

	memset(buf, 0, bytes_to_recv);
	rc = i2c_master_recv(client, buf, bytes_to_recv);
	if (rc != bytes_to_recv) {
		return -EINVAL;
	}

	return rc;
}

static uint8_t gElanBuffer[IDX_QUEUE_SIZE][IDX_PACKET_SIZE];
static int gQueueRear, gQueueFront, touch_failed_count;
extern void ntx_gpio_touch_reset(void);

void elan_touch_enqueue (void)
{
	if (0 < elan_touch_recv_data(elan_touch_data.client, gElanBuffer[gQueueRear])) {
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
	int i;
	
	switch (buf[0]) {
	case idx_coordinate_packet: {
		static uint16_t last_x, last_y, last_x2, last_y2;
		uint16_t x1, x2, y1, y2;
		uint8_t finger_stat;

		finger_stat = (buf[idx_finger_state] & 0x06) >> 1;

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
			if(elan_touch_parse_xy(&buf[1], &x1, &y1)){
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
			if(elan_touch_parse_xy(&buf[4], &x2, &y2)){
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
		elan_touch_report_data(elan_touch_data.client, gElanBuffer[gQueueFront]);
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
	elan_touch_enqueue ();	
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
		enable_irq_wake (client->irq);
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
		disable_irq_wake (elan_touch_data.client->irq);
		gTouchDisabled = 1;
	}
	return 0;
}

static int elan_touch_resume(struct platform_device *pdev)
{
	if (!elan_touch_detect_int_level ()) {
		elan_touch_ts_triggered ();
	}
	if (gSleep_Mode_Suspend && gTouchDisabled)
		enable_irq_wake (elan_touch_data.client->irq);
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
	
		ntx_gpio_touch_reset ();
		mdelay (500);

//		__hello_packet_handler (elan_touch_data.client);
		
		for (restartCnt=0; restartCnt <= 10; restartCnt++) {
			if (10 == restartCnt) 
				return -1;
			if (sizeof(isp_cmd) != i2c_master_send(elan_touch_data.client, isp_cmd, sizeof(isp_cmd))) {
				printk ("[%s-%d] failed sending isp command.\n",__func__,__LINE__);
	//			enable_irq (elan_touch_data.client->irq);
				mdelay (10);
				continue;
			}
			if (0 == CheckIapMode ()) {
				break;
			}
			mdelay (10);
		}
			
		if(sizeof(data) != i2c_master_send(elan_touch_data.client, &data,  sizeof(data)))
		{
			printk("[ELAN] dummy error.\n");
		}	
		mdelay(10);

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
	uint8_t getFirmwareVer[] = {0x53,0x00,0x00,0x01};
	uint8_t buf_recv[4] = { 0 };
	
	i2c_master_send(elan_touch_data.client, getFirmwareVer, 4);
	i2c_master_recv(elan_touch_data.client, buf_recv, 4);
	sprintf(buf, "%02X.%02X.%02X.%02X", buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);
	printk ("[%s-%d] firmware version %s \n", __func__, __LINE__, buf);

	return strlen(buf);
}

static DEVICE_ATTR(fwupg, 0666, NULL, fwupg_write);
static DEVICE_ATTR(version, 0666, version_read, NULL);

static const struct attribute *sysfs_touch_attrs[] = {
	&dev_attr_fwupg.attr,
	&dev_attr_version.attr,
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
	gTSC2004_exist = 1;
	
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

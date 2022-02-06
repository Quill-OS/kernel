/* 
 * Source for : Waltop ASIC5 pen touch controller.
 * drivers/input/tablet/waltop_i2c.c
 * 
 * Copyright (C) 2008-2013	Waltop International Corp. <waltopRD@waltop.com.tw>
 * 
 * History:
 * Copyright (c) 2011	Martin Chen <MartinChen@waltop.com.tw>
 * Copyright (c) 2012	Taylor Chuang <chuang.pochieh@gmail.com>
 * Copyright (c) 2012	Herman Han <HermanHan@waltop.com>
 * Copyright (c) 2013	Martin Chen <MartinChen@waltop.com.tw>
 * Copyright (c) 2014	Martin Chen <MartinChen@waltop.com>
 * * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 */

#include <linux/unistd.h>  
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
	#include <linux/earlysuspend.h>
#endif 
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/jiffies.h>
#include <linux/dma-mapping.h>

 
/*****************************************************************************
 *	Macro define for different platform
 ****************************************************************************/
#define USE_REGISTER_BOARD_INFO     0	// Use board info in driver
#define USE_IRQ_FUNCTION            0	// If disable_irq() function has problem or down system performance, do not use it in worker

#include "../../../arch/arm/mach-mx6/ntx_hwconfig.h"
#include "../../../arch/arm/mach-mx6/board-mx6sl_ntx.h"

extern volatile NTX_HWCONFIG *gptHWCFG;
/*****************************************************************************
 * Set these pin definition related to your system H/W configurations
 ****************************************************************************/
//#define GPIO_WALTOP_INT		(3*32 + 1)	/*GPIO_4_1 */
static int GPIO_WALTOP_INT = (3*32 + 1);	/*GPIO_4_1 */
static int GPIO_WALTOP_RSTB = (2*32 + 31);	/*GPIO_3_31 */

#define PEN_I2C_BUS_NUM         0	// Pen connect at I2C bus number 0
#define PEN_IRQ_NUM             tp->client->irq	// Pen IRQ number
// Pen related GPIOs
#define GPIO_PEN_INT        GPIO_WALTOP_INT	        // PEN ASIC GPIO7, pen interrupt pin
#define GPIO_PEN_RST_N      GPIO_WALTOP_RSTB	// PEN ASIC RST_N, pen reset pin


/*****************************************************************************
 *	Waltop EMPen characteristics
 ****************************************************************************/
#define WALTOP_I2C_SLAVEADDRESS (0x6E >> 1) // Waltop address, do not change
#define WALTOP_DRIVER_NAME      "waltop_i2c"
//
#define WALTOP_MAX_X            27599//6249    // min value is 0
#define WALTOP_MAX_Y            20799//3999    // min value is 0
#define WALTOP_MAX_P            1023    // min value is 0
#define WALTOP_MIN_P_TIPON      5       // min tips on pressure value


/*****************************************************************************
 *	MACRO definitions and structure
 ****************************************************************************/
#define KLOG_NAME               "[Waltop]" // identifier for kernel log
#define WALTOP_DEVICE_NAME      "waltopPen" // keep "waltop" as input device name identifier
#define WALTOP_VENDER_ID        0x172F
#define WALTOP_MODULE_ID        0x0100
#define I2C_DEV_INFO_PKT_SIZE   9 // Device Information Data packet size
#define I2C_CORD_DATA_PKT_SIZE  8 // Coordinate Data packet size
#define DEV_SUM_BYTE            (I2C_DEV_INFO_PKT_SIZE-1)
#define CORD_SUM_BYTE           (I2C_CORD_DATA_PKT_SIZE-1)
//
//#define REMAP_TO_LCD_SIZE       // mapping to screen resolution
//#define TO_LCD_SWAP_XY          // swap pen_x to LCD_Y
//
// Screen characteristics
static unsigned long gdwScreenMaxY=758;
static unsigned long gdwScreenMaxX=1024;
#define LCD_SCREEN_MAX_Y		   gdwScreenMaxY
#define LCD_SCREEN_MAX_X		   gdwScreenMaxX

#define FW_RESET_DELAY_A2B	30	// Delay 30 ms from A to B
#define FW_RESET_DELAY_B2C	50	// Delay 50 ms from B to C
#define FW_RESET_DELAY_CP	5	// Delay 5 ms after C

#define IAP_FWUPDATE            // firmware update code include

#ifdef IAP_FWUPDATE
#include <linux/wakelock.h>
#include <linux/wait.h>
#include <linux/sysfs.h>
//
#define FW_IODATA_SIZE		8	// 8 bytes data
#define FW_DATAPACKET_SIZE	10	// 10 bytes fwdata packet
#define FW_BLOCK_SIZE		128	// 128 bytes buffer for user ap
#define USE_WAKELOCK
//
#define WALTOP_WRITE_RETRY	5	// retry count
#define WALTOP_RETRY_DELAY	100	// delay 100 us after error
#define UF_CMD_SIZE         	4   	// command is 4 bytes
#define UF_CMD_OK         	0x9201
#define UF_ID_ENTERISP      	0x01
#define UF_ID_NOTIMEOUT		0x08
#endif



/*****************************************************************************
 * Private Structure
 ****************************************************************************/
struct waltop_I2C
{
	struct i2c_client	*client;
	struct input_dev	*input;
	struct work_struct	work;
	struct timer_list	timer;

	char	phys[32];
	// Minimun value of X,Y,P are 0
	__u16	x_max;      // X maximun value
	__u16	y_max;      // Y maximun value
	__u16	p_max;      // Pressure maximun value
	__u16	p_minTipOn; // minimun tip on pressure
	__u16	fw_version; // 110 means 1.10

	__u8	pkt_data[16];// packets data buffer

	// Ensures that only one function can specify the Device Mode at a time
	struct mutex mutex; // reentrant protection for struct

	#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
	#endif
};


/*****************************************************************************
 * Function Prototypes
 ****************************************************************************/
static irqreturn_t waltop_I2C_irq(int irq, void *handle);


/*****************************************************************************
 * Global Variables
 ****************************************************************************/
static struct workqueue_struct *waltop_I2C_wq=NULL;
static struct waltop_I2C *tp = NULL;
static u8 m_device_info[12];
static int m_show_DebugLog=1;	// show Debug debug log on/off
static int empen_early_suspended = 0;	// 1 means in suspend mode
//static int m_driver_probe=0;
#if (!USE_IRQ_FUNCTION)
static int m_wq_Jobs=0;	// initial is 0
#endif

#ifdef IAP_FWUPDATE
static DECLARE_WAIT_QUEUE_HEAD(iap_wait_queue_head);
#ifdef USE_WAKELOCK
struct wake_lock iap_wake_lock;
#endif
static int wait_queue_flag=0;
static int m_loop_write_flag=0;
static int m_request_count=0;	// request bytes counter
static int m_iap_fw_updating=0;	// 1 means updating
static int m_iapIsReadCmd=0;
static int m_iapPageCount=0;
static int m_fw_cmdAckValue=0;	// fw command ack value
static char iap_fw_status[64];	// fw update status string
#endif


#define waltop_DisableIRQ()	disable_irq(PEN_IRQ_NUM)
#define waltop_EnableIRQ()	enable_irq(PEN_IRQ_NUM)

#define waltop_gpio_out(io_pin, value)  gpio_direction_output(io_pin, value)
#define waltop_set_pin_gpio_out(io_pin)	// nothing
#define waltop_gpio_in(io_pin)  gpio_direction_input(io_pin)
#define waltop_PenStop(value)   waltop_gpio_out(GPIO_PEN_RST_N, value)


/*****************************************************************************
 * I2C read functions
 ****************************************************************************/
static int waltop_I2C_read()
{
	int ret = -1;
	struct i2c_msg msg;

	mutex_lock(&tp->mutex);
	msg.addr = tp->client->addr;
	msg.flags = I2C_M_RD; // Read
	msg.len = I2C_CORD_DATA_PKT_SIZE;
	msg.buf = tp->pkt_data;

	ret = i2c_transfer(tp->client->adapter, &msg, 1);
 	if( ret == 1) {	// 1 msg sent OK
		ret = I2C_CORD_DATA_PKT_SIZE;
 	}
 	else {// some error
		printk(KERN_ERR "%s#%d:%s failed? err=%d\n", KLOG_NAME, __LINE__, __func__, ret);
	}
	mutex_unlock(&tp->mutex);
	return ret;
}

static int waltop_I2C_readDeviceInfo()
{
	int i, ret=-1;
	u8 sum=0, buf[1];
	struct i2c_msg msg;

	// sent COMMAND 0x2A to Pen
	buf[0] = 0x2A;
	msg.addr = tp->client->addr;
	msg.flags = 0; // Write
	msg.len = 1;
	msg.buf = (u8 *)buf;
	ret = i2c_transfer(tp->client->adapter, &msg, 1);
 	if( ret == 1)	// 1 msg sent OK
 	{
		// Delay 1 ms, wait for f/w device data ready
		mdelay(1);
		// read back device information
		msg.addr = tp->client->addr;
		msg.flags = I2C_M_RD; // Read
		msg.len = I2C_DEV_INFO_PKT_SIZE;
		msg.buf = m_device_info;
		ret = i2c_transfer(tp->client->adapter, &msg, 1);

		if( ret == 1) {	// 1 msg sent OK
			// Check checksum
			for(i=1; i<DEV_SUM_BYTE; i++) // D1 to D7
				sum = sum + m_device_info[i];
			if( m_show_DebugLog ) {
			   printk("waltop readDeviceInfo is  %02x, %02x, %02x, %02x,   %02x, %02x, %02x, %02x,   %02x, CheckSum=%02x\n",
				m_device_info[0], m_device_info[1], m_device_info[2], m_device_info[3],
				m_device_info[4], m_device_info[5], m_device_info[6], m_device_info[7],
				m_device_info[8], sum);
			}
			if( sum == m_device_info[DEV_SUM_BYTE] )
				ret = I2C_DEV_INFO_PKT_SIZE;
			else { // CheckSum error
				ret = -2;
				printk(KERN_ERR "%s#%d:%sChecksum error!\n", KLOG_NAME, __LINE__, __func__);
			}
		}
		else {// some error
			printk(KERN_ERR "%s#%d:%s failed? err=%d\n", KLOG_NAME, __LINE__, __func__, ret);
		}
	}
	else {// some error
		printk(KERN_ERR "%s#%d:%s 0x2A command failed, ret=%d\n", KLOG_NAME, __LINE__, __func__, ret);
	}
	return ret;
}



#ifdef IAP_FWUPDATE
/*****************************************************************************
 * Firmware update related finctions
 ****************************************************************************/
static int I2C_read_func(unsigned char *read_buf, int read_count)
{
	int ret = -1;
	struct i2c_msg msg;

	mutex_lock(&tp->mutex);
	msg.addr = tp->client->addr;
	msg.flags = I2C_M_RD; // Read
	msg.len = read_count;
	msg.buf = read_buf;

	ret = i2c_transfer(tp->client->adapter, &msg, 1);
 	if( ret == 1) {	// 1 msg sent OK
		ret = read_count;
 	}
 	else {// some error
		printk(KERN_ERR "%s#%d:%s failed? err=%d\n", KLOG_NAME, __LINE__, __func__, ret);
	}
	mutex_unlock(&tp->mutex);
	return ret;
}

static int Read_Ack()
{
	int ret = -1;
	unsigned char tmp_buf[10];
	
	tmp_buf[0]=0;
	tmp_buf[1]=0;
	ret = I2C_read_func(tmp_buf, 2);
 	if( ret == 2) { // read count as we request
		ret = (tmp_buf[0] << 8 | tmp_buf[1]);
	}
//	if( m_show_DebugLog )
//		printk( "FW Ack Value=0x%04x", ret);

	return ret;
}


static int I2C_write_func(unsigned char *write_buf, int write_count)
{
	int ret = -1;
	struct i2c_msg msg;

	if( m_show_DebugLog ) {
		if( write_count == 4 ) {
			printk( "FW CMD=0x%02x, 0x%02x, 0x%02x, 0x%02x\n", 
	        	write_buf[0], write_buf[1], write_buf[2], write_buf[3]);
		}
		else if( write_count == 2 ) {
			printk( "FW CMD=0x%02x, 0x%02x\n", 
	        	write_buf[0], write_buf[1]);
		}
	}
	mutex_lock(&tp->mutex);
	msg.addr = tp->client->addr;
	msg.flags = 0; // Write
	msg.len = write_count;
	msg.buf = write_buf;
	ret = i2c_transfer(tp->client->adapter, &msg, 1);
 	if( ret == 1) {	// 1 msg sent OK
		ret = write_count;
 	}
	else { // negative is error
		printk(KERN_ERR "%s#%d:%s failed? err=%d\n", KLOG_NAME, __LINE__, __func__, ret);
		ret = -EINVAL;
	}
	mutex_unlock(&tp->mutex);
	return ret;
}

static int Write_Data(unsigned char *tx_buf)
{
	int i;
	unsigned char checkSum;
	unsigned char out_buffer[FW_DATAPACKET_SIZE+2];

	checkSum = 0;
	for(i=0; i<FW_IODATA_SIZE; i++) { 
		out_buffer[i] = tx_buf[i];
		checkSum += tx_buf[i];
	}
	out_buffer[FW_IODATA_SIZE] = checkSum;
	out_buffer[FW_IODATA_SIZE+1] = checkSum;
	// 10 bytes I2C packet
	return I2C_write_func(out_buffer, FW_DATAPACKET_SIZE);
}

static ssize_t fwdata_read(struct file *filp, struct kobject *kobj,
			    struct bin_attribute *bin_attr,
			    char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	unsigned char ACKDataOK[2] = {0x92, 0xE1};
	unsigned char ACKPageOK[2] = {0x92, 0xE2};
	unsigned char rx_buf[FW_BLOCK_SIZE+4];
	int i, ret=0, retCount=count, retry=0;

	if(count>FW_BLOCK_SIZE) {
		printk(KERN_ERR "%s#%d:%s Read size=%d over buffer size!\n", KLOG_NAME, __LINE__, __func__, count);
		return -1;
	}

	strcpy(iap_fw_status, "reading");
	m_loop_write_flag = 1;

	for(i=0; i<(count-FW_IODATA_SIZE); i=i+FW_IODATA_SIZE)
	{
		retry=0;
Retry_ReadCMD:
		ret=I2C_read_func(&rx_buf[i], FW_IODATA_SIZE);
		if(FW_IODATA_SIZE==ret) {
			wait_queue_flag = 0;
			ret=I2C_write_func(ACKDataOK, 2);
			// wait for fw INT
			wait_event_interruptible(iap_wait_queue_head, wait_queue_flag!=0);
		}
		else {
			retry++;
			if( retry < WALTOP_WRITE_RETRY ) { //total retry times
				printk(KERN_ERR "%s#%d: Retry_ReadCMD count=%d\n", KLOG_NAME, __LINE__, retry);
				udelay(WALTOP_RETRY_DELAY);
				goto Retry_ReadCMD;
			}
			retCount = 0x92E0;
			strcpy(iap_fw_status, "error");
			break;
		}
	}
	if(retCount<0x9200) // send read page OK and return data
	{
		// Last read
		retry=0;
Retry_ReadLast:
		ret=I2C_read_func(&rx_buf[count-FW_IODATA_SIZE], FW_IODATA_SIZE);
		if(FW_IODATA_SIZE==ret) {
			wait_queue_flag = 0;
			m_iapPageCount--;
			// fwupdate will send final ack code at last read, so this is only for every page
			if( m_iapPageCount>0 ) {
				ret=I2C_write_func(ACKPageOK, 2);
				// wait for fw INT
				wait_event_interruptible(iap_wait_queue_head, wait_queue_flag!=0);
			}
			memcpy(buf, rx_buf, count);
		}
		else {
			retry++;
			if( retry < WALTOP_WRITE_RETRY ) { //total retry times
				printk(KERN_ERR "%s#%d: Retry_ReadLast count=%d\n", KLOG_NAME, __LINE__, retry);
				udelay(WALTOP_RETRY_DELAY);
				goto Retry_ReadLast;
			}
			retCount = 0x92E0;
			strcpy(iap_fw_status, "error");
		}
	}
	wait_queue_flag = 0;
	m_loop_write_flag = 0;
	return retCount;
}

static int isEnterISPCmd(char *inCmdBuf)
{
	u8 cmdEnterISPMode[UF_CMD_SIZE] = {0x84, 0x00, 0x10, 0x14};	// I2C use 0x10 0x14
	int i=0;

	for( i=0; i<UF_CMD_SIZE; i++ ) {
		if( inCmdBuf[i] != cmdEnterISPMode[i] )
			return 0;
	}

	if( m_fw_cmdAckValue==0x92EF )
		return 0;

	return 1;
}

static ssize_t fwdata_write(struct file *filp, struct kobject *kobj,
			    struct bin_attribute *bin_attr,
			    char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	unsigned char tx_buf[FW_BLOCK_SIZE+4];
	int i, ret=0, retCount=count, retry=0;

	if(count>FW_BLOCK_SIZE) {
		printk(KERN_ERR "%s#%d:%s Write size=%d over buffer size!\n", KLOG_NAME, __LINE__, __func__, count);
		return -1;
	}
	memcpy(tx_buf, buf, count);

	// 2013/12/23, Martin add check cmd status in Enter IAP function
	if( (count==4)&&(1==isEnterISPCmd(tx_buf)) ) {
		// Check previous status
		if( m_fw_cmdAckValue==UF_CMD_OK )
			return UF_CMD_SIZE;
		else
			return m_fw_cmdAckValue;// return status
	}
	m_fw_cmdAckValue = 0;
	wait_queue_flag = 0;
	m_loop_write_flag = 1;

	// 2012/12/18, Martin check
	if((count==4)&&(tx_buf[0]==0x84)) // make sure it is fw IAP command
	{
		m_iapIsReadCmd=0;
		retry=0;
Retry_WriteCMD:
		ret=I2C_write_func(tx_buf, count);
		// wait for fw ACK
		wait_event_interruptible(iap_wait_queue_head, wait_queue_flag!=0);
		wait_queue_flag = 0;
		if( m_fw_cmdAckValue == 0x9200 ) { // Command error
			retry++;
			if( retry < WALTOP_WRITE_RETRY ) { //total retry times
				printk(KERN_ERR "%s#%d: Retry_WriteCMD count=%d\n", KLOG_NAME, __LINE__, retry);
				udelay(WALTOP_RETRY_DELAY);
				goto Retry_WriteCMD;
			}
			retCount = m_fw_cmdAckValue;	// return the error code
		}
		else {
			if( tx_buf[1]==0x02 ) {	// Start IAP Read
				m_iapIsReadCmd = 1;
				m_iapPageCount = tx_buf[2]*4;
				// wait for fw INT then back
				wait_event_interruptible(iap_wait_queue_head, wait_queue_flag!=0);
				wait_queue_flag = 0;
			}
		}
	}else if(count == 2 && tx_buf[0] == 0x92){
		dev_info(&tp->client->dev, "Write the finish message 0x%02X,0x%02X\n", tx_buf[0], tx_buf[1]);
		ret=I2C_write_func(tx_buf, count);
	}else // Write data
	{
		strcpy(iap_fw_status, "waiting");
		m_request_count = count;
		for(i=0; i<count; i=i+FW_IODATA_SIZE)
		{
			retry=0;
Retry_WriteData:
			ret=Write_Data(&(tx_buf[i]));
			// wait for fw ACK
			wait_event_interruptible(iap_wait_queue_head, wait_queue_flag!=0);
			wait_queue_flag = 0;
			// 2013/05/03, Martin add error retry
			if( m_fw_cmdAckValue == 0x92E0 ) {
				retry++;
				if( retry < WALTOP_WRITE_RETRY ) { //total retry times
					printk(KERN_ERR "%s#%d: Retry_WriteData count=%d\n", KLOG_NAME, __LINE__, retry);
					udelay(WALTOP_RETRY_DELAY);
					goto Retry_WriteData;
				}
			}
			m_request_count -= FW_IODATA_SIZE;
			// 2013/05/03
			// return the error code 0x9200, 0x92E0, 0x92EE or finished code 0x92EF
			if(m_fw_cmdAckValue == 0x9200 || m_fw_cmdAckValue == 0x92E0 || m_fw_cmdAckValue == 0x92EF) { 
				retCount = m_fw_cmdAckValue;	// return the error code 0x9200, 0x92E0, 0x92EF
				if( m_show_DebugLog && m_fw_cmdAckValue != 0x92EF )
					printk("write page error!\n");
				break;
			}
		}
		if( (retCount==count)&&(m_request_count==0) ) {		
			strcpy(iap_fw_status, "continue");
		}
	}
	m_loop_write_flag = 0;
	return retCount;
}

static struct bin_attribute waltop_I2C_fwdata_attributes = {
	.attr = {
		.name = "fwdata",
		.mode = S_IRUGO|S_IWUGO, // change this to super user only when release
	},
	.size = 0,	// 0 means no limit, but not over 4KB
	.read = fwdata_read,
	.write = fwdata_write,
};
#endif


/*************************************************************************
 * SYSFS related functions
 ************************************************************************/
static ssize_t waltop_show_fw_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	mutex_lock(&tp->mutex);
	waltop_DisableIRQ();
	ret = waltop_I2C_readDeviceInfo();
	if( ret>0 ) {
		tp->fw_version = ((m_device_info[6]&0x7F)*100) + (m_device_info[7]&0x7F);
	}
	waltop_EnableIRQ();
	mutex_unlock(&tp->mutex);
	return sprintf(buf, "%d\n", tp->fw_version);
}

static ssize_t waltop_show_read_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_msg msg;
	int ret = -1, i=0;
	unsigned char read_buf[12]={0};

	mutex_lock(&tp->mutex);
	msg.addr = tp->client->addr;
	msg.flags = I2C_M_RD; // Read
	msg.len = I2C_CORD_DATA_PKT_SIZE;
	msg.buf = read_buf;
	
	ret = i2c_transfer(tp->client->adapter, &msg, 1);
 	if( ret != 1) {	// some error
		printk(KERN_ERR "%s() failed?-%s:%d\n", __FUNCTION__, __FILE__, __LINE__);
		// set the errorpattern
		for( i=0; i<I2C_CORD_DATA_PKT_SIZE; i++ )
			read_buf[i] = 0xf0+i;
	}
	mutex_unlock(&tp->mutex);

	return snprintf(buf, PAGE_SIZE, "%x, %x, %x, %x,   %x, %x, %x, %x\n",
		read_buf[0], read_buf[1], read_buf[2], read_buf[3],
		read_buf[4], read_buf[5], read_buf[6], read_buf[7]);
}

static ssize_t waltop_store_write_cmd(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int buf_tmp[8]={0};
	__u8 cmd_buffer[8]={0};
	int i, ret=0, paramCount=0;

	mutex_lock(&tp->mutex);
	paramCount = sscanf(buf, "%x %x %x %x %x %x %x %x",
		&buf_tmp[0], &buf_tmp[1], &buf_tmp[2], &buf_tmp[3],
		&buf_tmp[4], &buf_tmp[5], &buf_tmp[6], &buf_tmp[7]);

	if( m_show_DebugLog )
		printk( "%x, %x, %x, %x,  %x, %x, %x, %x, count=%d \n", 
			buf_tmp[0], buf_tmp[1], buf_tmp[2], buf_tmp[3],
			buf_tmp[4], buf_tmp[5], buf_tmp[6], buf_tmp[7], paramCount);

	// Check size and save into buffer
	if( paramCount > 8 )
		paramCount = 8;
	for(i=0; i<paramCount; i++)
		cmd_buffer[i] = (__u8)buf_tmp[i];

	ret = i2c_master_send(tp->client, cmd_buffer, paramCount);
	if( ret<0 ) { // negative is error
		printk(KERN_ERR "%s#%d:%s failed? err=%d\n", KLOG_NAME, __LINE__, __func__, ret);
		ret = -EINVAL;
	}
	else { // ret is number of bytes send, we change it to size
		if( m_show_DebugLog )
			printk( "input size=%d, %d bytes send\n", size, ret);
		ret = size;
	}
	mutex_unlock(&tp->mutex);

	return size;
}

static ssize_t waltop_store_ShowDebug(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d", &m_show_DebugLog);
	printk("Set the m_show_DebugLog=%d\n", m_show_DebugLog);
	return size;
}

static ssize_t waltop_show_ShowDebug(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", m_show_DebugLog);
}


static void waltop_initPenDevice(void)
{
	// Set INT pin to GPIO mode and High
	waltop_set_pin_gpio_out(GPIO_PEN_INT);
	waltop_gpio_out(GPIO_PEN_INT, 1);

	// Set Reset pin as GPIO mode, out and high first
	waltop_set_pin_gpio_out(GPIO_PEN_RST_N);
	waltop_gpio_out(GPIO_PEN_RST_N, 1);

	// Wait for stable
	mdelay(5);

	// Then pull RESET low for 30ms, then High for 50ms
	waltop_gpio_out(GPIO_PEN_RST_N, 0);
	mdelay(FW_RESET_DELAY_A2B);
	waltop_gpio_out(GPIO_PEN_RST_N, 1);
	mdelay(FW_RESET_DELAY_B2C);

	// Set INT pin to INT type and in
	waltop_gpio_in(GPIO_PEN_INT);

	// Wait for ready
	mdelay(FW_RESET_DELAY_CP);
}

// Reset EMPen, flagIAP
// 	0, for reset and enter Normal mode but not readDeviceInfo
// 	1, for reset and enter Normal mode and readDeviceInfo
// 	2, for reset and enter IAP mode (for fwupdate)
static void waltop_ResetEMPen(int flagIAP)
{
	int ret=0;

	// Disable IRQ
	waltop_DisableIRQ();

	// Set INT pin to GPIO mode and High
	waltop_set_pin_gpio_out(GPIO_PEN_INT);

	// Set INT pin to high for normal mode, Low for IAP mode
	if( 2==flagIAP )
		waltop_gpio_out(GPIO_PEN_INT, 0);
	else
		waltop_gpio_out(GPIO_PEN_INT, 1);
	mdelay(1);

	// Set Reset pin as GPIO mode, out and high first
	waltop_set_pin_gpio_out(GPIO_PEN_RST_N);
	waltop_gpio_out(GPIO_PEN_RST_N, 1);

	// Wait for stable
	mdelay(5);

	// Then pull RESET low for 30ms, then High for 50ms
	waltop_gpio_out(GPIO_PEN_RST_N, 0);
	mdelay(FW_RESET_DELAY_A2B);
	waltop_gpio_out(GPIO_PEN_RST_N, 1);
	mdelay(FW_RESET_DELAY_B2C);

	if( 2==flagIAP ) {
		// Set INT pin to High first
		waltop_gpio_out(GPIO_PEN_INT, 1);
	}
	else
	{	// Re-read device information
		if( 1==flagIAP ) {
			// Wait for ready
			mdelay(FW_RESET_DELAY_CP);
			// Re-read device information
			ret = waltop_I2C_readDeviceInfo();
			if( ret>0 ) {
				tp->fw_version = ((m_device_info[6]&0x7F)*100) + (m_device_info[7]&0x7F);
			}
		}
		// else 0 for not readDeviceInfo
	}
	// Set INT pin to INT type and in
	waltop_gpio_in(GPIO_PEN_INT);

	// Wait for ready
	mdelay(FW_RESET_DELAY_CP);

	// Re-enable IRQ
	waltop_EnableIRQ();
}

static int pen_set_job_start(void)
{
	printk("pen_set_job_start*******\n");

	if (0 > request_irq(PEN_IRQ_NUM, waltop_I2C_irq, IRQF_TRIGGER_FALLING, WALTOP_DEVICE_NAME, NULL)) {
		printk ("[%s-%d] request irq failed.\n", __func__, __LINE__);
	}
	else
		enable_irq_wake (PEN_IRQ_NUM);
}

static ssize_t waltop_show_reset(struct device *dev, struct device_attribute *attr, char *buf)
{
	mutex_lock(&tp->mutex);
	// Common reset code, 0 for Normal mode but not readDeviceInfo
	waltop_ResetEMPen(0);

	mutex_unlock(&tp->mutex);
	return snprintf(buf, PAGE_SIZE, "Reset finished.\n");
}


#ifdef IAP_FWUPDATE
static int waltop_send_IAPCMD(int cmdIndex)
{
	u8 cmdEnterISPMode[UF_CMD_SIZE] = {0x84, 0x00, 0x10, 0x14};	// I2C use 0x10 0x14
	u8 cmdNoTimeout[UF_CMD_SIZE] = {0x84, 0x0D, 0x55, 0x5C}; // No WatchDog timeout
	int ret=0, retry=0, retCount=UF_CMD_SIZE;

	m_fw_cmdAckValue = 0;
	wait_queue_flag = 0;
	m_loop_write_flag = 1;
	m_iapIsReadCmd=0;
	retry=0;

Retry_WriteIAPCmd:
	if( UF_ID_ENTERISP==cmdIndex )
		ret=I2C_write_func(cmdEnterISPMode, UF_CMD_SIZE);
	else if( UF_ID_NOTIMEOUT==cmdIndex )
		ret=I2C_write_func(cmdNoTimeout, UF_CMD_SIZE);
	else {
		m_loop_write_flag = 0;
		return 0;
	}
	/* wait for fw ACK */
	wait_event_interruptible(iap_wait_queue_head, wait_queue_flag!=0);
	wait_queue_flag = 0;
	if( m_fw_cmdAckValue == 0x9200 ) { // Command error
		retry++;
		if( retry < WALTOP_WRITE_RETRY ) { //total retry times
			udelay(WALTOP_RETRY_DELAY);
			goto Retry_WriteIAPCmd;
		}
		// over error retry
		retCount = m_fw_cmdAckValue;	/* return the error code */
	}
	m_loop_write_flag = 0;
	return retCount;
}

static void waltop_enter_IAP(void)
{
	int ret=0;

	mutex_lock(&tp->mutex);
	m_iap_fw_updating = 1;
#ifdef USE_WAKELOCK
	wake_lock(&iap_wake_lock);
#endif

	strcpy(iap_fw_status, "enterIAP");
	// Enter IAP
	// GPIO6 = Low, and set INT pin Low
	// Reset EMPen, 2 for IAP mode
	waltop_ResetEMPen(2);

	if( m_show_DebugLog )
		printk("waltop_show_enter_IAP-done, m_iap_fw_updating = %d\n", m_iap_fw_updating);

	mutex_unlock(&tp->mutex);
	
	cancel_work_sync(&tp->work);
	flush_workqueue(waltop_I2C_wq);

	// Send EnterIAP CMD to confirm
	ret = waltop_send_IAPCMD(UF_ID_ENTERISP);
	if( ret != UF_CMD_SIZE )
		printk("waltop_show_enter_IAP- Enter IAP CMD error, ret = %d\n", ret);
	else {
		// Disable WatchDog Timeout
		ret = waltop_send_IAPCMD(UF_ID_NOTIMEOUT);
		if( ret != UF_CMD_SIZE )
			printk("waltop_show_enter_IAP- Disable WatchDog Timeout error, ret = %d\n", ret);
		else {
			if( m_fw_cmdAckValue==0x9255 )
				m_fw_cmdAckValue = 0x9201;
		}
	}
}

static void waltop_exit_IAP(void)
{
	mutex_lock(&tp->mutex);
 	// Exit IAP
	// Common reset code, 1 for Normal mode and readDeviceInfo
	waltop_ResetEMPen(1);

	// Reset related variables
	m_iap_fw_updating = 0;
	m_iapIsReadCmd = 0;
	m_iapPageCount = 0;
#ifdef USE_WAKELOCK
	wake_unlock(&iap_wake_lock);
#endif
	strcpy(iap_fw_status, "exitIAP");
	if( m_show_DebugLog )
		printk("waltop_show_exit_IAP-done, m_iap_fw_updating = %d\n", m_iap_fw_updating);

	mutex_unlock(&tp->mutex);
}

static ssize_t waltop_show_enter_IAP(struct device *dev, struct device_attribute *attr, char *buf)
{
	waltop_enter_IAP();
	return snprintf(buf, PAGE_SIZE, "Enter firmware update mode.\n");
}

static ssize_t waltop_show_exit_IAP(struct device *dev, struct device_attribute *attr, char *buf)
{
	waltop_exit_IAP();
	return snprintf(buf, PAGE_SIZE, "Enter normal mode.\n");
}
#endif


// SYSFS : Device Attributes
static DEVICE_ATTR(fwversion,		S_IRUGO,	waltop_show_fw_version,	NULL);
static DEVICE_ATTR(read_data,		S_IRUGO,	waltop_show_read_data,	NULL);
static DEVICE_ATTR(write_cmd,		S_IWUGO,	NULL,	waltop_store_write_cmd);
static DEVICE_ATTR(reset,		S_IRUGO,	waltop_show_reset,	NULL);
static DEVICE_ATTR(show_debug,		S_IRUGO|S_IWUGO,	waltop_show_ShowDebug,	waltop_store_ShowDebug);
#ifdef IAP_FWUPDATE
static DEVICE_ATTR(fwupdate_entry,	S_IRUGO,	waltop_show_enter_IAP,	NULL);
static DEVICE_ATTR(fwupdate_exit,	S_IRUGO,	waltop_show_exit_IAP,	NULL);
#endif

static struct attribute *waltop_I2C_attributes[] = {
	&dev_attr_fwversion.attr,
	&dev_attr_read_data.attr,
	&dev_attr_write_cmd.attr,
	&dev_attr_reset.attr,
	&dev_attr_show_debug.attr,
#ifdef IAP_FWUPDATE
	&dev_attr_fwupdate_entry.attr,
	&dev_attr_fwupdate_exit.attr,
#endif
	NULL
};

static struct attribute_group waltop_I2C_attribute_group = {
    .attrs = waltop_I2C_attributes
};


/*****************************************************************************
 * Interrupt and Workqueue related finctions
 ****************************************************************************/
void waltop_I2C_worker(struct work_struct *work)
{
	struct input_dev *inp = tp->input;
	unsigned int ps=0, dv;
	unsigned long x,y;
	int i, btn_low, in_range, tip=0, ret=0;
	__u8 sum=0;

	//if( m_show_DebugLog >2 )
	//	printk( "waltop_I2C_worker enter\n");

//	if( !m_driver_probe )
//		goto i2cWorker_out;
#ifdef IAP_FWUPDATE
	if(m_iap_fw_updating==1)
	{
		if(m_iapIsReadCmd==0)
		{
			ret = Read_Ack();
//			if( m_show_DebugLog )
//				printk("waltop_I2C_worker m_request_count = %d ret = %x\n", m_request_count,ret);
			
			m_fw_cmdAckValue = ret;
			if(ret == 0x9200 || ret == 0x92E0 || ret == 0x92EE || ret == 0x92EF)
			{
				if(ret == 0x92EF)
					strcpy(iap_fw_status, "finish");
				else
					strcpy(iap_fw_status, "error");
			#ifdef USE_WAKELOCK
				wake_unlock(&iap_wake_lock);
			#endif
			}
		}
		if(m_loop_write_flag) {
			wait_queue_flag = 1;
			wake_up_interruptible(&iap_wait_queue_head);
		}
		goto i2cWorker_out;
	}
#endif

	// do I2C read
	if( waltop_I2C_read() < 0 ){ // some error
		goto i2cWorker_out;
	}
	
	// Log read packet data from firmware for debug
//	if( m_show_DebugLog )
//		printk( "waltop_fwdata %02x, %02x, %02x, %02x,   %02x, %02x, %02x, %02x\n",
//			tp->pkt_data[0], tp->pkt_data[1], tp->pkt_data[2], tp->pkt_data[3],
//			tp->pkt_data[4], tp->pkt_data[5], tp->pkt_data[6], tp->pkt_data[7]);
#if 0
		{
			int i;
			for(i=0;i<8;i++) {
				printk("%02x,",tp->pkt_data[i]);
			}
			printk("\n");
		}
#endif

	// Check checksum
	for(i=1; i<CORD_SUM_BYTE; i++) // D1 to D6
		sum = sum + tp->pkt_data[i];
	if( sum != tp->pkt_data[CORD_SUM_BYTE] )
	{
		printk(KERN_ERR "%s#%d:%s Checksum mismatch, %x, %x\n", 
				KLOG_NAME, __LINE__, __func__, tp->pkt_data[CORD_SUM_BYTE], sum);
		goto i2cWorker_out;
	}

	// do input_sync() here
	in_range = tp->pkt_data[6]&0x20;
//	if( m_show_DebugLog )
//		printk( "in_range=0x%x  ", in_range);

	// report BTN_TOOL_PEN event depend on in range
	input_report_key(inp, BTN_TOOL_PEN, in_range>0 ? 1:0);

	if( in_range )
	{
		x = ((tp->pkt_data[1] << 8) | tp->pkt_data[2]);
		y = ((tp->pkt_data[3] << 8) | tp->pkt_data[4]);
		ps = (((tp->pkt_data[6]&0x03) << 8 ) | tp->pkt_data[5]);
		dv = (tp->pkt_data[6]&0x40);
	
		tip = (tp->pkt_data[6]&0x04);
		btn_low = (tp->pkt_data[6]&0x08);
		//
		// <<<< 2012/11 mirror max/min direction if opposite >>>>
		//
		//x = tp->x_max - x;
		//y = tp->y_max - y;
		//printk( "x2=%d, y2=%d\n",x, y);
		if(2!=gptHWCFG->m_val.bUIStyle) {
			//
			// <<<< 2012/11 scale the resolution here if Android don't do it >>>>
			if(0==gptHWCFG->m_val.bUIStyle) {
				// pen x to LCD_X, pen y to LCD_Y
				x = x * LCD_SCREEN_MAX_X / (tp->x_max);
				y = y * LCD_SCREEN_MAX_Y / (tp->y_max);
				x = LCD_SCREEN_MAX_X - x;
				y = LCD_SCREEN_MAX_Y - y;
			}
			else {
				// pen x to LCD_X, pen y to LCD_Y
				x = x * LCD_SCREEN_MAX_X / (tp->x_max);
				y = y * LCD_SCREEN_MAX_Y / (tp->y_max);
				x = LCD_SCREEN_MAX_X - x;
			}
		}
		// Use standard single touch event
		// Report X, Y Value, 
		// <<<< 2012/11 swap x,y here if need >>>>
		//
#if 0
		if( m_show_DebugLog )
			printk( "x=%d, y=%d, pressure=%d, tip=0x%x, btn_low=0x%x\n",
				x, y, ps, tip, btn_low);
#endif

#if 0
	if(0==gptHWCFG->m_val.bUIStyle) {
		// pen x to LCD_Y, pen y to LCD_X
		input_report_abs(inp, ABS_X, y);
		input_report_abs(inp, ABS_Y, x);
	}
	else 
#endif
	{
		// pen x to LCD_X, pen y to LCD_Y
		input_report_abs(inp, ABS_X, x);
		input_report_abs(inp, ABS_Y, y);
	}

		// Report pressure and Tip as Down/Up
		if( ps > tp->p_minTipOn ) {
			input_report_key(inp, BTN_TOUCH, 1);
			input_report_abs(inp, ABS_PRESSURE, ps);
		}
		else {
			input_report_key(inp, BTN_TOUCH, 0);
			input_report_abs(inp, ABS_PRESSURE, 0);
		}
		// Report side buttons on Pen
//		input_report_key(inp, BTN_STYLUS, btn_low ? 1:0);
	}
	else {
		input_report_key(inp, BTN_TOUCH, 0);
		input_report_abs(inp, ABS_PRESSURE, 0);
		input_report_key(inp, BTN_STYLUS, 0);
	}
	input_sync(inp);

i2cWorker_out:
#if USE_IRQ_FUNCTION
//	waltop_EnableIRQ();
#else
	m_wq_Jobs = 0;
#endif

	return;
}

static irqreturn_t waltop_I2C_irq(int irq, void *handle)
{
	//printk( "%s enter, irq : 0x%x\n", __func__, irq);
//	if( m_show_DebugLog )
//		printk( "waltop_I2C_irq enter\n");

	// disable other irq until this irq is finished
#if USE_IRQ_FUNCTION
//	waltop_DisableIRQ();
#else
	if( m_wq_Jobs ) {
		if( m_show_DebugLog )
			printk( "waltop work queue is working! skip jobs count=%d\n", m_wq_Jobs);
		m_wq_Jobs++;
		goto waltop_irq_out;
	}	
	m_wq_Jobs = 1;
#endif

	// schedule workqueue
	queue_work(waltop_I2C_wq, &tp->work);
waltop_irq_out:
	return IRQ_HANDLED;
} 


/*****************************************************************************
 * Probe and Initialization functions
 ****************************************************************************/
static int waltop_I2C_init_sysfile(struct i2c_client *client)
{
	int ret = 0;
	
	ret = sysfs_create_group(&client->dev.kobj, &waltop_I2C_attribute_group);
	if (ret) {
		dev_err(&(client->dev), "%s-%s: ERROR: sysfs_create_group() failed, err=%d\n", KLOG_NAME, __func__, ret);
	}
	else {
		dev_err(&(client->dev), "%s-%s: sysfs_create_group() succeeded\n", KLOG_NAME, __func__);
	}

#ifdef IAP_FWUPDATE
	ret = sysfs_create_bin_file(&client->dev.kobj, &waltop_I2C_fwdata_attributes);
	if (ret < 0) {
		dev_err(&(client->dev), "%s-%s: ERROR: Binary file attributes creation failed, err=%d\n", KLOG_NAME, __func__, ret);
		ret = -ENODEV;
		goto error_sysfs_create_bin_file;
	}
#ifdef USE_WAKELOCK
	wake_lock_init(&iap_wake_lock, WAKE_LOCK_SUSPEND,"PenIAP_WakeLock");
#endif
	return 0;

error_sysfs_create_bin_file:
	sysfs_remove_group(&client->dev.kobj, &waltop_I2C_attribute_group);
#endif

	return ret;
}

static int waltop_I2C_initialize(struct i2c_client *client)
{
	struct input_dev *input_device;
	int ret = 0;
	printk("waltop_I2C_initialize***\n");

	// create the input device and register it.
	input_device = input_allocate_device();
	if (!input_device) {
		ret = -ENOMEM;
		dev_err(&(client->dev), "%s-%s: ERROR: Could not allocate input device\n", KLOG_NAME, __func__);
		goto error_free_device;
	}

	tp->client = client;
	tp->input = input_device;

	// 2013/01/30, Martin add device information
	ret = waltop_I2C_readDeviceInfo();
	if( ret>0 ) { 
		tp->x_max = ((m_device_info[1] << 8) | m_device_info[2]);
		tp->y_max = ((m_device_info[3] << 8) | m_device_info[4]);
		tp->p_max = ((m_device_info[6]&0x80)>>7)|((m_device_info[7]&0x80)>>6);
		tp->p_max = ((tp->p_max << 8) | (m_device_info[5]));
		tp->fw_version = ((m_device_info[6]&0x7F)*100) + (m_device_info[7]&0x7F);
		if( tp->x_max <= 0 )
			tp->x_max = WALTOP_MAX_X;
		if( tp->y_max <= 0 )
			tp->y_max = WALTOP_MAX_Y;
		if( tp->p_max <= 0 )
			tp->p_max = WALTOP_MAX_P;
	}
	else {
		tp->x_max = WALTOP_MAX_X;
		tp->y_max = WALTOP_MAX_Y;
		tp->p_max = WALTOP_MAX_P;
		tp->fw_version = 0;	/* 100 means fw 1.00, this is 0.00 */
	}
	if( m_show_DebugLog )
		printk("%s: ret=%d, x_max=%d, y_max=%d, p_max=%d, f/w version=%d\n",
			__func__ ,ret, tp->x_max, tp->y_max, tp->p_max, tp->fw_version);
	tp->p_minTipOn = WALTOP_MIN_P_TIPON;

	waltop_I2C_wq = create_singlethread_workqueue("waltop_I2C_wq");
	if (NULL == waltop_I2C_wq) {
		printk(KERN_ERR "%s-%s: ERROR: Could not create the Work Queue due to insufficient memory\n", KLOG_NAME, __func__);
		ret = -ENOMEM;
	}
	// Prepare worker structure prior to set up the timer/ISR
	INIT_WORK(&tp->work, waltop_I2C_worker);

	// set input device information
	snprintf(tp->phys, sizeof(tp->phys), "%s/input0", dev_name(&client->dev));

	input_device->phys = tp->phys;
	input_device->dev.parent = &client->dev;
	input_device->name = WALTOP_DEVICE_NAME;

	// for example, read product name and version from f/w to fill into id information
	input_device->id.bustype = BUS_I2C;
	input_device->id.vendor  = WALTOP_VENDER_ID;
	input_device->id.product = WALTOP_MODULE_ID;	// Module code xxxx
	input_device->id.version = tp->fw_version;	// set to fw_version

	input_set_drvdata(input_device, tp);

	// Use standard single touch event
	set_bit(EV_ABS, input_device->evbit);
	set_bit(EV_KEY, input_device->evbit);
	set_bit(EV_SYN, input_device->evbit);

	set_bit(ABS_X, input_device->absbit);
	set_bit(ABS_Y, input_device->absbit);
	set_bit(ABS_PRESSURE, input_device->absbit);

	set_bit(BTN_TOOL_PEN, input_device->keybit);
	set_bit(BTN_TOUCH, input_device->keybit);
	set_bit(BTN_STYLUS, input_device->keybit);
	//set_bit(INPUT_PROP_DIRECT, input_device->propbit);

	// Set ABS_X, ABS_Y as module's resolution
	if(2!=gptHWCFG->m_val.bUIStyle) {
		// <<<< 2012/11 scaling the resolution here if Android don't do it >>>>
		input_set_abs_params(input_device, ABS_X, 0, LCD_SCREEN_MAX_X, 0, 0);
		input_set_abs_params(input_device, ABS_Y, 0, LCD_SCREEN_MAX_Y, 0, 0);
		/* or */
	}
	else {
		input_set_abs_params(input_device, ABS_X, 0, tp->x_max, 0, 0);
		input_set_abs_params(input_device, ABS_Y, 0, tp->y_max, 0, 0);
	}
	input_set_abs_params(input_device, ABS_PRESSURE, 0, tp->p_max, 0, 0);

	ret = input_register_device(input_device);
	if (0 != ret) {
		dev_err(&(client->dev), "%s-%s ERROR: input_register_device(), err=%d\n", KLOG_NAME, __func__, ret);
		goto error_free_device;
	}

	// Create SYSFS related file
	ret = waltop_I2C_init_sysfile(client);
	if (ret >= 0) {
		i2c_set_clientdata(client, tp);
		goto succeed;
	}

error_free_device:
	if (input_device) {
		input_free_device(input_device);
		tp->input = NULL;
	}
	kfree(tp);
succeed:
	return ret;
}

/*****************************************************************************
 * Early Suspend
 ****************************************************************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void waltop_early_suspend(struct early_suspend *h)
{
	int ret = 0;
	printk("%s-%s: waltop EMPen suspend\n", KLOG_NAME, __func__);

	// Clear all work queue
	ret = cancel_work_sync(&tp->work);
	printk("cancel_work_sync, rc = %d\n", ret);
	flush_workqueue(waltop_I2C_wq);

	// Add turn Power OFF GPIO code here
#ifdef PEN_GPIO_POWER_ENABLE
	gpio_direction_output(PEN_GPIO_POWER_ENABLE, 0);
#else
	waltop_PenStop(0);
#endif
	waltop_DisableIRQ();

	empen_early_suspended = 1;
	return;
}

static void waltop_late_resume(struct early_suspend *h)
{
	int ret = 0;
	printk("%s-%s: waltop EMPen resume\n", KLOG_NAME, __func__);

	// Add turn Power ON GPIO code here
#ifdef PEN_GPIO_POWER_ENABLE
	gpio_direction_output(PEN_GPIO_POWER_ENABLE, 0);
#else
	waltop_initPenDevice();
#endif
	waltop_EnableIRQ();

	empen_early_suspended = 0;
	return;
}
#endif


static int __devinit waltop_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	int err;
	
	printk( "%s-%s enter\n", KLOG_NAME, __func__);
	
	GPIO_WALTOP_RSTB = dev->platform_data;
	GPIO_WALTOP_INT = irq_to_gpio(client->irq);
	printk("%s-%s reset %d\n", KLOG_NAME, __func__,	GPIO_WALTOP_RSTB);
	

	switch(gptHWCFG->m_val.bDisplayResolution) {
	case 1: // 1024x758
		gdwScreenMaxY=758;
		gdwScreenMaxX=1024;
		break;
	case 2: // 1024x768
		gdwScreenMaxY=768;
		gdwScreenMaxX=1024;
		break;
	case 3: // 1440x1080
		gdwScreenMaxY=1080;
		gdwScreenMaxX=1440;
		break;
	case 4: // 1366x768
		gdwScreenMaxY=768;
		gdwScreenMaxX=1366;
		break;
	case 5: // 1448x1072
		gdwScreenMaxY=1072;
		gdwScreenMaxX=1448;
		break;
	case 6: // 1600x1200
		gdwScreenMaxY=1200;
		gdwScreenMaxX=1600;
		break;
	default:
		gdwScreenMaxY=600;
		gdwScreenMaxX=800;
		break;
	}

	// Intialize pen device as normal mode
	waltop_initPenDevice();

	// Check functionality
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -EIO;

	tp = kzalloc(sizeof(struct waltop_I2C), GFP_KERNEL);
	if (NULL == tp) {
		dev_err(&(client->dev), "%s-%s: ERROR: Could not allocate %d bytes of kernel memory\n", KLOG_NAME, __func__, sizeof(struct waltop_I2C));
		err = -ENOMEM;
		goto error_devinit0;
	}
	// Need to initialize the SYSFS mutex before creating the SYSFS entries in waltop_I2C_initialize()
	mutex_init(&tp->mutex);
	err = waltop_I2C_initialize(client);
	if (0 > err) {
		dev_err(&(client->dev), "%s-%s: ERROR: waltop_I2C could not be initialized, err=%d\n", KLOG_NAME, __func__, err);
		goto error_mutex_destroy;
	}
	// Now we are ready
	pen_set_job_start();

#if (!ADD_PALTFORM_DEVICE_DRIVER)
#ifdef CONFIG_HAS_EARLYSUSPEND
	tp->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
	tp->early_suspend.suspend = waltop_early_suspend;
	tp->early_suspend.resume = waltop_late_resume;
	register_early_suspend(&tp->early_suspend);
#endif
#endif
	goto succeed;

error_mutex_destroy:
	mutex_destroy(&tp->mutex);
error_devinit0:
succeed:
	printk("%s: Pen Device Probe %s\n", __func__, (err < 0) ? "Pen Probe FAIL" : "Pen Probe PASS");
//	m_driver_probe = (err < 0) ? 0: 1;
	return err;	
}


static int __devexit waltop_i2c_remove(struct i2c_client *client)
{
	dev_info(&(client->dev), "%s-%s: Driver is unregistering\n", KLOG_NAME, __func__);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&tp->early_suspend);
#endif
	input_unregister_device(tp->input);	

	mutex_lock(&tp->mutex);
	// Remove the SYSFS entries
#ifdef IAP_FWUPDATE
	sysfs_remove_bin_file(&client->dev.kobj, &waltop_I2C_fwdata_attributes);
#ifdef USE_WAKELOCK
	wake_unlock(&iap_wake_lock);
	wake_lock_destroy(&iap_wake_lock);
#endif
#endif
	sysfs_remove_group(&client->dev.kobj, &waltop_I2C_attribute_group);

	mutex_unlock(&tp->mutex);
	mutex_destroy(&tp->mutex);
    
	kfree(tp);
	i2c_set_clientdata(client, NULL);
	dev_info(&(client->dev), "%s-%s: Driver unregistration is complete\n", KLOG_NAME, __func__);
    
	return 0;
}


/*****************************************************************************
 * Device driver data
 ****************************************************************************/
// Device driver data
static const struct i2c_device_id waltop_i2c_idtable[] = { {WALTOP_DRIVER_NAME, 0}, {} };

static struct i2c_driver waltop_i2c_driver =
{
	.driver = {
		.name	= WALTOP_DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table	= waltop_i2c_idtable,
	.probe	= waltop_i2c_probe,
	.remove	= waltop_i2c_remove,

};

#if 0
static int waltop_probe(struct platform_device *pdev)
{
	printk("%s waltop_probe\n", KLOG_NAME);
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&waltop_early_suspend_handler);
#endif

	return i2c_add_driver(&waltop_i2c_driver);
}
 
static int waltop_remove(struct platform_device *pdev)
{
	printk("%s waltop_remove\n", KLOG_NAME);
	i2c_del_driver(&waltop_i2c_driver);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&waltop_early_suspend_handler);
#endif

	return 0;
}

/*****************************************************************************
 * Platform device and driver data
 ****************************************************************************/
// Platform driver structure
static struct platform_driver waltop_Driver =
{
    .probe = waltop_probe,
    .remove = waltop_remove,
    .driver = 
    {
        .name = WALTOP_DRIVER_NAME,
        .owner = THIS_MODULE,
    },
};

// Platform device structure
static struct platform_device waltop_Device = 
{
	.name = WALTOP_DRIVER_NAME,
	.id = -1,
};
#endif

/*****************************************************************************
* Register board info
 ****************************************************************************/
#if USE_REGISTER_BOARD_INFO
static struct i2c_board_info __initdata waltop_I2C_board_info[] =
{   
	{
		I2C_BOARD_INFO(WALTOP_DRIVER_NAME, WALTOP_I2C_SLAVEADDRESS),
		//.irq = PEN_IRQ_NUM,
	},
};
#endif



/*****************************************************************************
 * Init and Exit function
 ****************************************************************************/
static int __init waltop_I2C_init(void)
{
	int ret = 0;
	printk( "%s-%s: Waltop I2C Pen Driver (Built %s @ %s)\n", KLOG_NAME, __func__, __DATE__, __TIME__);

#if USE_REGISTER_BOARD_INFO
	// Register i2c board info
	i2c_register_board_info(PEN_I2C_BUS_NUM, waltop_I2C_board_info,
		ARRAY_SIZE(waltop_I2C_board_info));
#endif

#if 0
	if(platform_device_register(&waltop_Device))
	{
		printk("%s-%s platform_device_register ERROR\n", KLOG_NAME, __func__);
		return -ENODEV;
	}
#endif
	// not platform device, we do add driver here
	ret = i2c_add_driver(&waltop_i2c_driver);
	printk("%s-%s: Pen i2c driver add %s\n", KLOG_NAME, __func__, (ret < 0) ? "FAIL" : "Success");
	mdelay(1);

	return ret;
}

static void __exit waltop_I2C_exit(void)
{
	 //struct platform_driver waltop_Driver;
//	platform_driver_unregister(&waltop_Driver);
	if (waltop_I2C_wq) {
		destroy_workqueue(waltop_I2C_wq);
		waltop_I2C_wq = NULL;
	}
	i2c_del_driver(&waltop_i2c_driver);
}

module_init(waltop_I2C_init);
module_exit(waltop_I2C_exit); 

MODULE_AUTHOR("Waltop");
MODULE_DESCRIPTION("Waltop I2C pen driver");
MODULE_LICENSE("GPL");

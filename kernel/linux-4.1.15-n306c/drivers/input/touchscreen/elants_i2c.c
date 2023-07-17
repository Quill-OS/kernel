/*
 * Elan Microelectronics touch panels with I2C interface
 *
 * Copyright (C) 2014 Elan Microelectronics Corporation.
 * Scott Liu <scott.liu@emc.com.tw>
 *
 * This code is partly based on hid-multitouch.c:
 *
 *  Copyright (c) 2010-2012 Stephane Chatty <chatty@enac.fr>
 *  Copyright (c) 2010-2012 Benjamin Tissoires <benjamin.tissoires@gmail.com>
 *  Copyright (c) 2010-2012 Ecole Nationale de l'Aviation Civile, France
 *
 *
 * This code is partly based on i2c-hid.c:
 *
 * Copyright (c) 2012 Benjamin Tissoires <benjamin.tissoires@gmail.com>
 * Copyright (c) 2012 Ecole Nationale de l'Aviation Civile, France
 * Copyright (c) 2012 Red Hat, Inc
 */

/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/input/mt.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
//#include "../../drivers/gpio/gpiolib.h"
#include <linux/regulator/consumer.h>
#include <asm/unaligned.h>
#include <linux/miscdevice.h>  //for misc_register
//#include <linux/gpio.h>
#include <linux/of_gpio.h>	//for of_get_named_gpio_flags()
//#define USE_IN_SUNXI	1
#ifdef USE_IN_SUNXI
#include "../../../arch/arm/mach-sunxi/ntx_hwconfig.h"
#include "../init-input.h"
#endif //] CONFIG_ARCH_SUNXI

#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"
extern volatile NTX_HWCONFIG *gptHWCFG;
extern volatile unsigned gMX6SL_IR_TOUCH_RST;

extern int volatile giSuspendingState;

/* Device, Driver information */
//#define DEVICE_NAME	"elants_i2c"
#define DEVICE_NAME	"elan_ktf"
#define DRV_VERSION	"1.0.9"

#define ELAN_HID_I2C	/* for ELAN HID over I2C Protocol */
#define ELAN_RAW_DATA   /* for ELAN to read Raw Data */

//#define CALC_REPORT_RATE
//#define PRINT_REPORT_RATE

/* Convert from rows or columns into resolution */
#define ELAN_TS_RESOLUTION(n, m)   (((n) - 1) * (m))

/* FW header data */
#ifdef ELAN_HID_I2C
#define HEADER_SIZE		67
#else
#define HEADER_SIZE		4
#endif
#define FW_HDR_TYPE		0
#define FW_HDR_COUNT		1
#define FW_HDR_LENGTH		2

/* Buffer mode Queue Header information */
#define QUEUE_HEADER_SINGLE	0x62
#define QUEUE_HEADER_NORMAL	0X63
#define QUEUE_HEADER_WAIT	0x64

/* Command header definition */
#define CMD_HEADER_WRITE	0x54
#define CMD_HEADER_READ		0x53
#define CMD_HEADER_6B_READ	0x5B
#define CMD_HEADER_RESP		0x52
#define CMD_HEADER_6B_RESP	0x9B
#define CMD_HEADER_HELLO	0x55
#define CMD_HEADER_REK		0x66
#define CMD_HEADER_ROM_READ	0x96
#ifdef ELAN_HID_I2C
#define CMD_HEADER_HID_I2C	0x43
#endif

#define	ELAN_REMARK_ID_OF_NON_REMARK_IC     0xFFFF

/* FW position data */
#ifdef ELAN_HID_I2C
#define PACKET_SIZE		67
#else 
#define PACKET_SIZE		55
#endif



#define MAX_CONTACT_NUM		10
#define FW_POS_HEADER		0
#define FW_POS_STATE		1
#define FW_POS_TOTAL		2
#define FW_POS_XY		3
#define FW_POS_CHECKSUM		34
#define FW_POS_WIDTH		35
#define FW_POS_PRESSURE		45

#define HID_I2C_FINGER_HEADER	0x3F
//#define HID_I2C_PEN_HEADER		0x13  //0x22
#define HID_I2C_PEN_HEADER_0D		0x0D  //0x22
#define HID_I2C_PEN_HEADER_13		0x13  //0x22
#define HEADER_REPORT_10_FINGER	0x62
#define RSP_LEN               2

/* Header (4 bytes) plus 3 fill 10-finger packets */
#define MAX_PACKET_SIZE		169

#define BOOT_TIME_DELAY_MS	50

/* FW read command, 0x53 0x?? 0x0, 0x01 */
#define E_ELAN_INFO_FW_VER	0x00
#define E_ELAN_INFO_BC_VER	0x10
#define E_ELAN_INFO_TEST_VER	0xE0
#define E_ELAN_INFO_FW_ID	0xF0
#define E_INFO_OSR		0xD6
#define E_INFO_PHY_SCAN		0xD7
#define E_INFO_PHY_DRIVER	0xD8

#define MAX_RETRIES		3
#define MAX_FW_UPDATE_RETRIES	30

#define ELAN_FW_PAGESIZE	132
#define ACK_Fail 0x00
#define ACK_OK   0xAA
#define ACK_REWRITE 0x55
/* calibration timeout definition */
#define ELAN_CALI_TIMEOUT_MSEC	12000

#define ELAN_POWERON_DELAY_USEC	500
#define ELAN_RESET_DELAY_MSEC	20


// For Firmware Update
#ifdef ELAN_RAW_DATA
#define ELAN_IOCTLID	0xD0
#define IOCTL_I2C_SLAVE	_IOW(ELAN_IOCTLID,  1, int)
#define IOCTL_FW_INFO  _IOR(ELAN_IOCTLID, 2, int)
#define IOCTL_MINOR_FW_VER  _IOR(ELAN_IOCTLID, 3, int)
#define IOCTL_RESET  _IOR(ELAN_IOCTLID, 4, int)
#define IOCTL_IAP_MODE_LOCK  _IOR(ELAN_IOCTLID, 5, int)
#define IOCTL_CHECK_RECOVERY_MODE  _IOR(ELAN_IOCTLID, 6, int)
#define IOCTL_FW_VER  _IOR(ELAN_IOCTLID, 7, int)
#define IOCTL_X_RESOLUTION  _IOR(ELAN_IOCTLID, 8, int)
#define IOCTL_Y_RESOLUTION  _IOR(ELAN_IOCTLID, 9, int)
#define IOCTL_FW_ID  _IOR(ELAN_IOCTLID, 10, int)
#define IOCTL_ROUGH_CALIBRATE  _IOR(ELAN_IOCTLID, 11, int)
#define IOCTL_IAP_MODE_UNLOCK  _IOR(ELAN_IOCTLID, 12, int)
#define IOCTL_I2C_INT  _IOR(ELAN_IOCTLID, 13, int)
#define IOCTL_RESUME  _IOR(ELAN_IOCTLID, 14, int)
#define IOCTL_POWER_LOCK  _IOR(ELAN_IOCTLID, 15, int)
#define IOCTL_POWER_UNLOCK  _IOR(ELAN_IOCTLID, 16, int)
#define IOCTL_FW_UPDATE  _IOR(ELAN_IOCTLID, 17, int)
#define IOCTL_BC_VER  _IOR(ELAN_IOCTLID, 18, int)
#define IOCTL_2WIREICE  _IOR(ELAN_IOCTLID, 19, int)
#endif


enum elants_state {
	ELAN_STATE_NORMAL,
	ELAN_WAIT_QUEUE_HEADER,
	ELAN_WAIT_RECALIBRATION,
};

enum elants_iap_mode {
	ELAN_IAP_OPERATIONAL,
	ELAN_IAP_RECOVERY,
};


/* struct elants_data - represents state of Elan touchscreen device */
struct elants_data {
	struct i2c_client *client;
	struct input_dev *input;

	struct regulator *vcc33;
	struct regulator *vccio;
#ifdef USE_IN_SUNXI
	struct gpio_desc *reset_gpio;
#else
    int reset_gpio;
#endif
	int intr_gpio;

	u16 fw_version;
	u8 test_version;
	u8 solution_version;
	u8 bc_version;
	u8 iap_version;
	u16 hw_version;
	unsigned int x_res;	/* resolution in units/mm */
	unsigned int y_res;
	unsigned int x_max;
	unsigned int y_max;
	unsigned int cols;
	unsigned int rows;

	enum elants_state state;
	enum elants_iap_mode iap_mode;

	/* Guards against concurrent access to the device via sysfs */
	struct mutex sysfs_mutex;

	u8 cmd_resp[HEADER_SIZE];
	struct completion cmd_done;

#ifdef ELAN_HID_I2C
	u8 buf[PACKET_SIZE*2];
#else
	u8 buf[MAX_PACKET_SIZE];
#endif

	bool wake_irq_enabled;
	bool keep_power_in_suspend;
	bool unbinding;
	//int hover_flag;
#ifdef ELAN_RAW_DATA
	struct miscdevice firmware;
#endif
	/*for irq bottom half*/
	struct workqueue_struct *elan_wq;
	struct work_struct ts_work;

};

static int screen_max_x;
static int screen_max_y;
static int revert_x_flag; // 0 : normal , 1 : revert_x
static int revert_y_flag; // 0 : normal , 1 : revert_y
static int exchange_x_y_flag; // 0 : normal , 1 : exchange xy
extern int gSleep_Mode_Suspend;

static int suspending_trigger = 0;

static int elan_i2c_detect_int_level(struct elants_data *ts)
{
	return gpio_get_value(ts->intr_gpio);
}

void elan_i2c_ts_triggered(struct elants_data *ts) {
    queue_work(ts->elan_wq, &ts->ts_work);
}


static int nActivePenStatus = 0 ;		// 1: Enable , 2: Disable
uint8_t gReadTS[16] = {0} ;
uint8_t ActivePenState[37] = {0x04,0x00,0x23,0x00,0x03,0x00,0x04,0x53,0xC1,0x00,0x01};	// read setting
uint8_t ActivePenCmd[37] = {0x04,0x00,0x23,0x00,0x03,0x00,0x04,0x54,0xC1,0x03,0x1C};	// Pen is open
							// ActivePenCmd[10] -> 0x03: open , 0x01: close
static DECLARE_WAIT_QUEUE_HEAD(pen_status) ;

#ifdef USE_IN_SUNXI //[
static struct ctp_config_info config_info = {
	.input_type = CTP_TYPE,
	.name = NULL,
	.int_number = 0,
};

static int twi_id = -1;

static const unsigned short normal_i2c[2] = {0x10, I2C_CLIENT_END};

static void ctp_print_info(struct ctp_config_info * info)
{
	pr_debug("info.ctp_used:%d\n", info->ctp_used);
	pr_info("info.name:%s\n", info->name);
	pr_info("info.twi_id:%d\n", info->twi_id);
	pr_info("info.screen_max_x:%d\n", info->screen_max_x);
	pr_info("info.screen_max_y:%d\n", info->screen_max_y);
	pr_info("info.revert_x_flag:%d\n", info->revert_x_flag);
	pr_info("info.revert_y_flag:%d\n", info->revert_y_flag);
	pr_info("info.exchange_x_y_flag:%d\n", info->exchange_x_y_flag);
	pr_debug("info.irq_gpio_number:%d\n", info->irq_gpio.gpio);
	pr_debug("info.wakeup_gpio_number:%d\n", info->wakeup_gpio.gpio);
}

static int ctp_get_system_config(void)
{
	ctp_print_info(&config_info);

	twi_id = config_info.twi_id;
	if ((config_info.screen_max_x == 0) || (config_info.screen_max_y == 0)) {
		pr_warning("%s:screen_max_x || screen_max_y not defined \n", __func__);
		return 0;
	}
	screen_max_x = config_info.screen_max_x - 1;
	screen_max_y = config_info.screen_max_y - 1;
    

	revert_x_flag = config_info.revert_x_flag;
	revert_y_flag = config_info.revert_y_flag;
	exchange_x_y_flag = config_info.exchange_x_y_flag;

	/*
	 * Note: Only consider 16Bits_mirror flag now and it invert revert_y_flag
	 * Todo: 1. 8Bits panel with mirror
	 *       2. Future panels with revert_x_flag needed
	 */
	if( 3 == gptHWCFG->m_val.bDisplayBusWidth ) {  //16Bits_mirror
		//revert_y_flag = revert_y_flag^0x3; // change revert_y_flag between 1 and 2
		revert_y_flag = !revert_y_flag; // change revert_y_flag between 1 and 2
		pr_debug("*** ctp revert y for 16bit mirror panel!\n");
	}

	return 1;
}

extern int enable_gpio_wakeup_src(int para);
extern int disable_gpio_wakeup_src(int para);
#endif //] USE_IN_SUNXI

#ifdef ELAN_RAW_DATA
static struct elants_data *private_ts;
int power_lock=0x00;
#endif

static int elants_i2c_send(struct i2c_client *client,
                           const void *data, size_t size)
{
	int ret;

	ret = i2c_master_send(client, data, size);
	if (ret == size){
		return 0;
	}

	if (ret >= 0)
		ret = -EIO;

	dev_err(&client->dev, "%s failed (%*ph): %d\n",
            __func__, (int)size, data, ret);

	return ret;
}

static int elants_i2c_read(struct i2c_client *client, void *data, size_t size)
{
	int ret;

	ret = i2c_master_recv(client, data, size);
	if (ret == size)
		return 0;

	if (ret >= 0)
		ret = -EIO;

	dev_err(&client->dev, "%s failed: %d\n", __func__, ret);

	return ret;
}

static int elants_i2c_execute_command(struct i2c_client *client,
                                      const u8 *cmd, size_t cmd_size,
                                      u8 *resp, size_t resp_size)
{
	struct i2c_msg msgs[2];
	int ret;
	u8 expected_response;

	switch (cmd[0]) {
	case CMD_HEADER_READ:  //53
		expected_response = CMD_HEADER_RESP;
		break;

	case CMD_HEADER_6B_READ:  //5B
		expected_response = CMD_HEADER_6B_RESP;
		break;
	case 0x04:  //HID_I2C command header
		expected_response = 0x43;
		break;
	default:
		dev_err(&client->dev, "%s: invalid command %*ph\n",
                __func__, (int)cmd_size, cmd);
		return -EINVAL;
	}

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].len = cmd_size;
	msgs[0].buf = (u8 *)cmd;

	msgs[1].addr = client->addr;
	msgs[1].flags = client->flags & I2C_M_TEN;
	msgs[1].flags |= I2C_M_RD;
	msgs[1].len = resp_size;
	msgs[1].buf = resp;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;

	if (ret != ARRAY_SIZE(msgs) || resp[FW_HDR_TYPE] != expected_response)
		return -EIO;

	return 0;
}
#if 0
static int elan_ts_poll(void)
{
	int status = 0, retry = 20;

	do {
		status = gpio_get_value(private_ts->pdata->intr_gpio);
		dev_dbg(&private_ts->client->dev,
				"%s: status = %d\n", __func__, status);
		if (status == 0)
			break;
		retry--;
		msleep(20);
	} while (status == 1 && retry > 0);

	dev_dbg(&private_ts->client->dev,
			"%s: poll interrupt status %s\n",
			__func__, status == 1 ? "high" : "low");

	return status == 0 ? 0 : -ETIMEDOUT;
}
#endif

static int elants_i2c_calibrate(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int ret, error;
#ifdef ELAN_HID_I2C
	static const u8 w_flashkey[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x54, 0xC0, 0xE1, 0x5A };
	static const u8 rek[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x54, 0x29, 0x00, 0x01 };
	static const u8 rek_resp[] = { 0x43, 0x00, 0x02, 0x04, 0x66, 0x66, 0x66, 0x66 };
#else
	static const u8 w_flashkey[] = { 0x54, 0xC0, 0xE1, 0x5A };
	static const u8 rek[] = { 0x54, 0x29, 0x00, 0x01 };
	static const u8 rek_resp[] = { CMD_HEADER_REK, 0x66, 0x66, 0x66 };
#endif

	disable_irq(client->irq);

	ts->state = ELAN_WAIT_RECALIBRATION;
	reinit_completion(&ts->cmd_done);

	error = elants_i2c_send(client, w_flashkey, sizeof(w_flashkey));
	if(error)
		dev_info(&client->dev, "send flah key failed\n");
	else
		dev_info(&client->dev, "[elan] flash key cmd = [%2x, %2x, %2x, %2x]\n",
                 w_flashkey[7], w_flashkey[8], w_flashkey[9], w_flashkey[10]);
				
	msleep(5);  // for debug
	error = elants_i2c_send(client, rek, sizeof(rek));
	if(error)
		dev_info(&client->dev, "send calibrate failed\n");
	else
		dev_info(&client->dev, "[elan] calibration cmd = [%2x, %2x, %2x, %2x]\n",
                 rek[7], rek[8], rek[9], rek[10]);

	enable_irq(client->irq);
	
	ret = wait_for_completion_interruptible_timeout(&ts->cmd_done,
                                                    msecs_to_jiffies(ELAN_CALI_TIMEOUT_MSEC));

	ts->state = ELAN_STATE_NORMAL;

	if (ret <= 0) {
		error = ret < 0 ? ret : -ETIMEDOUT;
		dev_err(&client->dev,
                "error while waiting for calibration to complete: %d\n",
                error);
		return error;
	}

	if (memcmp(rek_resp, ts->cmd_resp, sizeof(rek_resp))) {
		dev_err(&client->dev,
                "unexpected calibration response: %*ph\n",
                (int)sizeof(ts->cmd_resp), ts->cmd_resp);
		return -EINVAL;
	}

	return 0;
}
static int elants_i2c_hw_reset(struct i2c_client *client){
	//struct elants_data *ts = i2c_get_clientdata(client);
    struct elants_data *ts = private_ts;
	
	printk("[elan] enter elants_i2c_hw_reset....\n");
#ifdef USE_IN_SUNXI
	gpiod_direction_output(ts->reset_gpio,0);
	msleep(10);
	gpiod_direction_output(ts->reset_gpio,1);
	msleep(20);
	gpiod_direction_output(ts->reset_gpio,0);
	msleep(10);
#else
	gpio_direction_output(ts->reset_gpio,1);
	msleep(10);
	gpio_direction_output(ts->reset_gpio,0);
	msleep(20);
	gpio_direction_output(ts->reset_gpio,1);
	msleep(10);
#endif
    return 0;
}

static int elants_i2c_sw_reset(struct i2c_client *client)
{
#ifdef ELAN_HID_I2C
	const u8 soft_rst_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x77, 0x77, 0x77, 0x77 };
#else
	const u8 soft_rst_cmd[] = { 0x77, 0x77, 0x77, 0x77 };
#endif
	int error;

	error = elants_i2c_send(client, soft_rst_cmd,
                            sizeof(soft_rst_cmd));
	if (error) {
		dev_err(&client->dev, "software reset failed: %d\n", error);
		return error;
	}

	/*
	 * We should wait at least 10 msec (but no more than 40) before
	 * sending fastboot or IAP command to the device.
	 */
	msleep(30);

	return 0;
}

#ifdef ELAN_RAW_DATA
static int elan_iap_open(struct inode *inode, struct file *filp)
{
	//struct elants_data *ts = container_of(((struct miscdevice*)filp->private_data), struct elants_data, firmware);
	struct elants_data *ts = private_ts;

	printk("[elan] into elan_iap_open\n");

    if (private_ts == NULL)
		printk("ts is NULL\n");
		//dev_err(&private_ts->client->dev,"private_ts is NULL\n");
	else
		printk("irq num = %d\n", private_ts->client->irq);


	if (ts != NULL) {
		filp->private_data = ts;
		printk("irq num = %d\n", ts->client->irq);
	}else
		printk("[elan]ts is NULL\n");

    return 0;
}

static ssize_t elan_iap_read(struct file *filp, char *buff, size_t count, loff_t *offp) 
{
    char *tmp;
    int ret;
    long rc;
	//struct elants_data *ts = (struct elants_data *)filp->private_data;
    //struct i2c_client *client= ts->client;


    printk("[elan] into elan_iap_read\n");

    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);

    if (tmp == NULL)
        return -ENOMEM;

    ret = i2c_master_recv(private_ts->client, tmp, count);
	//ret = i2c_master_recv(ts->client, tmp, count);

    if (ret >= 0)
        rc = copy_to_user(buff, tmp, count);

    kfree(tmp);
    return ret;//(ret == 1) ? count : ret;
}

static ssize_t elan_iap_write(struct file *filp, const char *buff, size_t count, loff_t *offp)
{
    int ret;
    char *tmp;

	//struct elants_data *ts = (struct elants_data *)filp->private_data;
    //struct i2c_client *client= ts->client;

    printk("[elan] into elan_iap_write\n");

    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);

    if (tmp == NULL)
        return -ENOMEM;

    if (copy_from_user(tmp, buff, count)) {
        return -EFAULT;
    }

    ret = i2c_master_send(private_ts->client, tmp, count);
	//ret = i2c_master_send(ts->client, tmp, count);

    kfree(tmp);
    return ret;//(ret == 1) ? count : ret;
}

int elan_iap_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;

    return 0;
}

static long elan_iap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int __user *ip = (int __user *)arg;
	//struct elants_data *ts = (struct elants_data *)filp->private_data;
	struct elants_data *ts = private_ts;
    //struct i2c_client *client= ts->client;

    printk("[elan] into elan_iap_ioctl, cmd value %x\n", cmd);

    switch (cmd) {
    case IOCTL_RESET:
		//elants_i2c_hw_reset(private_ts->client);
		elants_i2c_hw_reset(ts->client);
        break;
    case IOCTL_IAP_MODE_LOCK:
        //disable_irq(private_ts->client->irq);
        disable_irq(ts->client->irq);
		printk("[elan]  ts->fw_version %x\n",  ts->fw_version);
        break;
    case IOCTL_IAP_MODE_UNLOCK:
       // enable_irq(private_ts->client->irq);
        enable_irq(ts->client->irq);
        break;
    case IOCTL_ROUGH_CALIBRATE:
       // elants_i2c_calibrate(private_ts);
        elants_i2c_calibrate(ts);
		break;
    case IOCTL_I2C_INT:
        //put_user(gpio_get_value(private_ts->intr_gpio), ip);
        put_user(gpio_get_value(ts->intr_gpio), ip);
        break;
	case IOCTL_POWER_LOCK:
        power_lock=1;
        break;
    case IOCTL_POWER_UNLOCK:
        power_lock=0;
        break;
	case IOCTL_FW_INFO:
		//__fw_packet_handler(private_ts->client);
		//elants_i2c_initialize(private_ts);
        break;
	case IOCTL_FW_VER:
        //return private_ts->fw_version;
        return ts->fw_version;
        break;
	case IOCTL_FW_ID:
        //return private_ts->hw_version;
        return ts->hw_version;
        break;
	case IOCTL_BC_VER:
		//return private_ts->bc_version;
		return ts->bc_version;
		break;
	case IOCTL_X_RESOLUTION:
        //return private_ts->x_max;
        return ts->x_max;
        break;
    case IOCTL_Y_RESOLUTION:
        //return private_ts->y_max;
        return ts->y_max;
        break;
    default:
        printk("[elan] Un-known IOCTL Command %d\n", cmd);
        break;
    }
    return 0;
}

struct file_operations elan_touch_fops = {
    .open =         elan_iap_open,
    .write =        elan_iap_write,
    .read = 	elan_iap_read,
    .release =	elan_iap_release,
    .unlocked_ioctl=elan_iap_ioctl,
    .compat_ioctl	= elan_iap_ioctl,

};
#endif

#ifdef ELAN_HID_I2C
//HID_I2C
static int elants_i2c_query_hw_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	//const u8 cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x53, 0xf0, 0x00, 0x01}; /*Get firmware ID 53 f0 00 01*/
	const u8 cmd[37] = {0x04 ,0x00, 0x23, 0x00, 0x03, 0x00, 0x06, 0x96, 0x80, 0x80, 0x00 ,0x00, 0x21}; 
	// Read informain (E70K14 2TP)	
	u8 resp[67] = {0};
	int major, minor;

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
                                           resp, sizeof(resp));

		printk("[elan] %s: (Firmware ID) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n", __func__, resp[0], resp[1], 
			resp[2], resp[3] , resp[4],resp[5], resp[6], resp[7], resp[8], resp[9]);
		
		if (!error) {
			
			//ts->hw_version = elants_i2c_parse_version(resp);
			//major = ((resp[5] & 0x0f) << 4) | ((resp[6] & 0xf0) >> 4);
			//minor = ((resp[6] & 0x0f) << 4) | ((resp[7] & 0xf0) >> 4);
			//ts->hw_version = major << 8 | minor;

			// 80 80 xx yy 21 ,  xx yy : Panel ID , 21 End Code
			ts->hw_version = resp[7] << 8 | resp[8];

			if (ts->hw_version != 0xffff)
				return 0;
		}

		dev_dbg(&client->dev, "read fw id error=%d, buf=%*phC\n",
                error, (int)sizeof(resp), resp);
	}

	if (error) {
		dev_err(&client->dev,
                "Failed to read fw id: %d\n", error);
		return error;
	}

	dev_err(&client->dev, "Invalid fw id: %#04x\n", ts->hw_version);

	return -EINVAL;
}

//HID_I2C
static int elants_i2c_query_fw_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	//const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_FW_VER, 0x00, 0x01 };
	const u8 cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x53, 0x00, 0x00, 0x01}; /*Get firmware version 53 00 00 01*/
	u8 resp[67] = {0};
	int major, minor;

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
                                           resp, sizeof(resp));
						   
		printk("[elan] %s: (Firmware version) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n", __func__, resp[0], resp[1], resp[2], resp[3] , resp[4], resp[5], resp[6], resp[7]);
		
		if (!error) {
			//ts->fw_version = elants_i2c_parse_version(resp);
			major = ((resp[5] & 0x0f) << 4) | ((resp[6] & 0xf0) >> 4);
			minor = ((resp[6] & 0x0f) << 4) | ((resp[7] & 0xf0) >> 4);
			ts->fw_version = major << 8 | minor;
	
			if (ts->fw_version != 0x0000 &&
			    ts->fw_version != 0xffff)
				return 0;
		}

		dev_dbg(&client->dev, "read fw version error=%d, buf=%*phC\n",
                error, (int)sizeof(resp), resp);
	}

	dev_err(&client->dev,
            "Failed to read fw version or fw version is invalid\n");

	return -EINVAL;
}

//HID_I2C
static int elants_i2c_query_test_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	u16 version;
	//const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_TEST_VER, 0x00, 0x01 };
	const u8 cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x53, 0xe0, 0x00, 0x01}; /*Get test version 53 e0 00 01*/
	u8 resp[67] = {0};
	int major, minor;

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
                                           resp, sizeof(resp));
		printk("[elan] %s: (test version) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n", __func__, resp[0], resp[1], resp[2], resp[3] , resp[4], resp[5], resp[6], resp[7]);
		
		if (!error) {
			//version = elants_i2c_parse_version(resp);
			major = ((resp[5] & 0x0f) << 4) | ((resp[6] & 0xf0) >> 4);
			minor = ((resp[6] & 0x0f) << 4) | ((resp[7] & 0xf0) >> 4);
			version = major << 8 | minor;
			
			ts->test_version = version >> 8;
			ts->solution_version = version & 0xff;

			return 0;
		}

		dev_dbg(&client->dev,
                "read test version error rc=%d, buf=%*phC\n",
                error, (int)sizeof(resp), resp);
	}

	dev_err(&client->dev, "Failed to read test version\n");

	return -EINVAL;
}

//HID_I2C
static int elants_i2c_query_bc_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	//const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_BC_VER, 0x00, 0x01 };
	const u8 cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x53, 0x10, 0x00, 0x01}; /*Get test version 53 10 00 01*/
	u8 resp[67] = {0};
	int major, minor;
	u16 version;
	int error;

	error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
                                       resp, sizeof(resp));
	printk("[elan] %s: (BC version) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n", __func__, resp[0], resp[1], resp[2], resp[3] , resp[4], resp[5], resp[6], resp[7]);
	
	if (error) {
		dev_err(&client->dev,
                "read BC version error=%d, buf=%*phC\n",
                error, (int)sizeof(resp), resp);
		return error;
	}

	//version = elants_i2c_parse_version(resp);
	major = ((resp[5] & 0x0f) << 4) | ((resp[6] & 0xf0) >> 4);
    minor = ((resp[6] & 0x0f) << 4) | ((resp[7] & 0xf0) >> 4);
    version = major << 8 | minor;
	
	ts->bc_version = version >> 8;
	ts->iap_version = version & 0xff;

	return 0;
}

//HID_I2C
static int elants_i2c_query_ts_info(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error;
	//u8 resp[17];
	u8 resp[67];
	u16 phy_x, phy_y, rows, cols, osr;
	/*
      const u8 get_resolution_cmd[] = {
      CMD_HEADER_6B_READ, 0x00, 0x00, 0x00, 0x00, 0x00
      };
      const u8 get_osr_cmd[] = {
      CMD_HEADER_READ, E_INFO_OSR, 0x00, 0x01
      };
      const u8 get_physical_scan_cmd[] = {
      CMD_HEADER_READ, E_INFO_PHY_SCAN, 0x00, 0x01
      };
      const u8 get_physical_drive_cmd[] = {
      CMD_HEADER_READ, E_INFO_PHY_DRIVER, 0x00, 0x01
      };
	*/
	const u8 get_resolution_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x06, 
                                        CMD_HEADER_6B_READ, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	const u8 get_osr_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 
                                 CMD_HEADER_READ, E_INFO_OSR, 0x00, 0x01
	};
	const u8 get_physical_scan_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 
                                           CMD_HEADER_READ, E_INFO_PHY_SCAN, 0x00, 0x01
	};
	const u8 get_physical_drive_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 
                                            CMD_HEADER_READ, E_INFO_PHY_DRIVER, 0x00, 0x01
	};
	

	/* Get trace number */
	error = elants_i2c_execute_command(client,
                                       get_resolution_cmd,
                                       sizeof(get_resolution_cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get resolution command failed: %d\n",
                error);
		return error;
	}
	printk("[elan] %s: (get resolution) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n", __func__, resp[0], resp[1], resp[2], resp[3] , resp[4], resp[5], resp[6], resp[7]);
	/*  
        rows = resp[2] + resp[6] + resp[10];
        cols = resp[3] + resp[7] + resp[11];
	*/
	rows = resp[6] + resp[10] + resp[14];
	cols = resp[7] + resp[11] + resp[15];
	
	ts->cols = cols;
	ts->rows = rows;

	/* Process mm_to_pixel information */
	error = elants_i2c_execute_command(client,
                                       get_osr_cmd, sizeof(get_osr_cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get osr command failed: %d\n",
                error);
		return error;
	}
	printk("[elan] %s: (get osr) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n", __func__, resp[0], resp[1], resp[2], resp[3] , resp[4], resp[5], resp[6], resp[7]);
	//osr = resp[3];
	osr = resp[7];

	error = elants_i2c_execute_command(client,
                                       get_physical_scan_cmd,
                                       sizeof(get_physical_scan_cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get physical scan command failed: %d\n",
                error);
		return error;
	}
	printk("[elan] %s: (get physical scan) %2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x\n", __func__, resp[0], resp[1], resp[2], resp[3] , resp[4], resp[5], resp[6], resp[7]);
	
	//phy_x = get_unaligned_be16(&resp[2]);
	phy_x = get_unaligned_be16(&resp[6]);

	error = elants_i2c_execute_command(client,
                                       get_physical_drive_cmd,
                                       sizeof(get_physical_drive_cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get physical drive command failed: %d\n",
                error);
		return error;
	}
	
	//phy_y = get_unaligned_be16(&resp[2]);
	phy_y = get_unaligned_be16(&resp[6]);

	dev_dbg(&client->dev, "phy_x=%d, phy_y=%d\n", phy_x, phy_y);

	if (rows == 0 || cols == 0 || osr == 0) {
		dev_warn(&client->dev,
                 "invalid trace number data: %d, %d, %d\n",
                 rows, cols, osr);
	} else {
		/* translate trace number to TS resolution */
		ts->x_max = ELAN_TS_RESOLUTION(rows, osr);
		ts->x_res = DIV_ROUND_CLOSEST(ts->x_max, phy_x);
		ts->y_max = ELAN_TS_RESOLUTION(cols, osr);
		ts->y_res = DIV_ROUND_CLOSEST(ts->y_max, phy_y);
	}

	printk("[elan] %s: ts->cols = %d, ts->rows = %d, x_max = %d,x_res=%d,y_max=%d,y_res=%d\n", __func__, ts->cols, ts->rows, ts->x_max,ts->x_res,ts->y_max,ts->y_res);
	return 0;
}

#else   
//for normal I2C
static u16 elants_i2c_parse_version(u8 *buf)
{
	return get_unaligned_be32(buf) >> 4;
}

static int elants_i2c_query_hw_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_FW_ID, 0x00, 0x01 };
	u8 resp[HEADER_SIZE];

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
                                           resp, sizeof(resp));
		if (!error) {
			ts->hw_version = elants_i2c_parse_version(resp);
			if (ts->hw_version != 0xffff)
				return 0;
		}

		dev_dbg(&client->dev, "read fw id error=%d, buf=%*phC\n",
                error, (int)sizeof(resp), resp);
	}

	if (error) {
		dev_err(&client->dev,
                "Failed to read fw id: %d\n", error);
		return error;
	}

	dev_err(&client->dev, "Invalid fw id: %#04x\n", ts->hw_version);

	return -EINVAL;
}

static int elants_i2c_query_fw_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_FW_VER, 0x00, 0x01 };
	u8 resp[HEADER_SIZE];

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
                                           resp, sizeof(resp));
		if (!error) {
			ts->fw_version = elants_i2c_parse_version(resp);
			if (ts->fw_version != 0x0000 &&
			    ts->fw_version != 0xffff)
				return 0;
		}

		dev_dbg(&client->dev, "read fw version error=%d, buf=%*phC\n",
                error, (int)sizeof(resp), resp);
	}

	dev_err(&client->dev,
            "Failed to read fw version or fw version is invalid\n");

	return -EINVAL;
}

static int elants_i2c_query_test_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, retry_cnt;
	u16 version;
	const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_TEST_VER, 0x00, 0x01 };
	u8 resp[HEADER_SIZE];

	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
                                           resp, sizeof(resp));
		if (!error) {
			version = elants_i2c_parse_version(resp);
			ts->test_version = version >> 8;
			ts->solution_version = version & 0xff;

			return 0;
		}

		dev_dbg(&client->dev,
                "read test version error rc=%d, buf=%*phC\n",
                error, (int)sizeof(resp), resp);
	}

	dev_err(&client->dev, "Failed to read test version\n");

	return -EINVAL;
}

static int elants_i2c_query_bc_version(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	const u8 cmd[] = { CMD_HEADER_READ, E_ELAN_INFO_BC_VER, 0x00, 0x01 };
	u8 resp[HEADER_SIZE];
	u16 version;
	int error;

	error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev,
                "read BC version error=%d, buf=%*phC\n",
                error, (int)sizeof(resp), resp);
		return error;
	}

	version = elants_i2c_parse_version(resp);
	ts->bc_version = version >> 8;
	ts->iap_version = version & 0xff;

	return 0;
}

static int elants_i2c_query_ts_info(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error;
	u8 resp[17];
	u16 phy_x, phy_y, rows, cols, osr;
	const u8 get_resolution_cmd[] = {
		CMD_HEADER_6B_READ, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	const u8 get_osr_cmd[] = {
		CMD_HEADER_READ, E_INFO_OSR, 0x00, 0x01
	};
	const u8 get_physical_scan_cmd[] = {
		CMD_HEADER_READ, E_INFO_PHY_SCAN, 0x00, 0x01
	};
	const u8 get_physical_drive_cmd[] = {
		CMD_HEADER_READ, E_INFO_PHY_DRIVER, 0x00, 0x01
	};

	/* Get trace number */
	error = elants_i2c_execute_command(client,
                                       get_resolution_cmd,
                                       sizeof(get_resolution_cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get resolution command failed: %d\n",
                error);
		return error;
	}

	rows = resp[2] + resp[6] + resp[10];
	cols = resp[3] + resp[7] + resp[11];

	/* Process mm_to_pixel information */
	error = elants_i2c_execute_command(client,
                                       get_osr_cmd, sizeof(get_osr_cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get osr command failed: %d\n",
                error);
		return error;
	}

	osr = resp[3];

	error = elants_i2c_execute_command(client,
                                       get_physical_scan_cmd,
                                       sizeof(get_physical_scan_cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get physical scan command failed: %d\n",
                error);
		return error;
	}

	phy_x = get_unaligned_be16(&resp[2]);

	error = elants_i2c_execute_command(client,
                                       get_physical_drive_cmd,
                                       sizeof(get_physical_drive_cmd),
                                       resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "get physical drive command failed: %d\n",
                error);
		return error;
	}

	phy_y = get_unaligned_be16(&resp[2]);

	dev_dbg(&client->dev, "phy_x=%d, phy_y=%d\n", phy_x, phy_y);

	if (rows == 0 || cols == 0 || osr == 0) {
		dev_warn(&client->dev,
                 "invalid trace number data: %d, %d, %d\n",
                 rows, cols, osr);
	} else {
		/* translate trace number to TS resolution */
		ts->x_max = ELAN_TS_RESOLUTION(rows, osr);
		ts->x_res = DIV_ROUND_CLOSEST(ts->x_max, phy_x);
		ts->y_max = ELAN_TS_RESOLUTION(cols, osr);
		ts->y_res = DIV_ROUND_CLOSEST(ts->y_max, phy_y);
	}

	return 0;
}
#endif

static int elants_i2c_fastboot(struct i2c_client *client)
{
#ifdef ELAN_HID_I2C
	const u8 boot_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x04, 0x4D, 0x61, 0x69, 0x6E };
#else
	const u8 boot_cmd[] = { 0x4D, 0x61, 0x69, 0x6E };
#endif
	int error;

	error = elants_i2c_send(client, boot_cmd, sizeof(boot_cmd));
	if (error) {
		dev_err(&client->dev, "boot failed: %d\n", error);
		return error;
	}

	dev_dbg(&client->dev, "boot success -- 0x%x\n", client->addr);
	return 0;
}

static int elants_i2c_initialize(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	int error, error2, retry_cnt;
#ifndef ELAN_HID_I2C	
	const u8 hello_packet[] = { 0x55, 0x55, 0x55, 0x55 };
	const u8 recov_packet[] = { 0x55, 0x55, 0x80, 0x80 };	
	u8 buf[HEADER_SIZE];
#endif
	printk("[elan] enter elants_i2c_initialize....\n");
	
	for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
		error = elants_i2c_sw_reset(client);
		if (error) {
			/* Continue initializing if it's the last try */
			if (retry_cnt < MAX_RETRIES - 1)
				continue;
		}

		error = elants_i2c_fastboot(client);
		if (error) {
			/* Continue initializing if it's the last try */
			if (retry_cnt < MAX_RETRIES - 1)
				continue;
		}

		/*HID I2C doesn't have hello packet*/
#ifndef ELAN_HID_I2C
		/* Wait for Hello packet */
		msleep(BOOT_TIME_DELAY_MS);

		error = elants_i2c_read(client, buf, sizeof(buf));
		if (error) {
			dev_err(&client->dev,
                    "failed to read 'hello' packet: %d\n", error);
		} else if (!memcmp(buf, hello_packet, sizeof(hello_packet))) {
			ts->iap_mode = ELAN_IAP_OPERATIONAL;
			break;
		} else if (!memcmp(buf, recov_packet, sizeof(recov_packet))) {
			/*
			 * Setting error code will mark device
			 * in recovery mode below.
			 */
			error = -EIO;
			break;
		} else {
			error = -EINVAL;
			dev_err(&client->dev,
                    "invalid 'hello' packet: %*ph\n",
                    (int)sizeof(buf), buf);
		}
#endif
	}

	/* hw version is available even if device in recovery state */
	error2 = elants_i2c_query_hw_version(ts);
	if (!error)
		error = error2;

	if (!error)
		error = elants_i2c_query_fw_version(ts);
	if (!error)
		error = elants_i2c_query_test_version(ts);
	if (!error)
		error = elants_i2c_query_bc_version(ts);
	if (!error)
		error = elants_i2c_query_ts_info(ts);

	if (error)
		ts->iap_mode = ELAN_IAP_RECOVERY;

	return 0;
}

/*
 * Firmware update interface.
 */
#ifndef ELAN_HID_I2C
static int elants_i2c_fw_write_page(struct i2c_client *client,
                                    const void *page)
{
	const u8 ack_ok[] = { 0xaa, 0xaa };
	u8 buf[2];
	int retry;
	int error;


	for (retry = 0; retry < MAX_FW_UPDATE_RETRIES; retry++) {
		error = elants_i2c_send(client, page, ELAN_FW_PAGESIZE);
		if (error) {
			dev_err(&client->dev,
                    "IAP Write Page failed: %d\n", error);
			continue;
		}

		error = elants_i2c_read(client, buf, RSP_LEN);
		if (error) {
			dev_err(&client->dev,
                    "IAP Ack read failed: %d\n", error);
			return error;
		}

		if (!memcmp(buf, ack_ok, sizeof(ack_ok)))
			return 0;

		error = -EIO;
		dev_err(&client->dev,
                "IAP Get Ack Error [%02x:%02x]\n",
                buf[0], buf[1]);
	}

	return error;
}
#endif

#ifndef ELAN_HID_I2C
static int elants_i2c_do_update_firmware(struct i2c_client *client,
                                         const struct firmware *fw,
                                         bool force)
{
	const u8 enter_iap[] = { 0x45, 0x49, 0x41, 0x50 };
	const u8 enter_iap2[] = { 0x54, 0x00, 0x12, 0x34 };
	const u8 iap_ack[] = { 0x55, 0xaa, 0x33, 0xcc };
	const u8 close_idle[] = {0x54, 0x2c, 0x01, 0x01};
	u8 buf[HEADER_SIZE];
	u16 send_id;
	int page, n_fw_pages;
	int error;

	/* Recovery mode detection! */
	if (force) {
		dev_dbg(&client->dev, "Recovery mode procedure\n");
		error = elants_i2c_send(client, enter_iap2, sizeof(enter_iap2));
	} else {
		/* Start IAP Procedure */
		dev_dbg(&client->dev, "Normal IAP procedure\n");
		/* Close idle mode */
		error = elants_i2c_send(client, close_idle, sizeof(close_idle));
		if (error)
			dev_err(&client->dev, "Failed close idle: %d\n", error);
		msleep(60);
		elants_i2c_sw_reset(client);
		msleep(20);
		error = elants_i2c_send(client, enter_iap, sizeof(enter_iap));
	}

	if (error) {
		dev_err(&client->dev, "failed to enter IAP mode: %d\n", error);
		return error;
	}

	msleep(20);

	/* check IAP state */
	error = elants_i2c_read(client, buf, 4);
	if (error) {
		dev_err(&client->dev,
                "failed to read IAP acknowledgement: %d\n",
                error);
		return error;
	}

	if (memcmp(buf, iap_ack, sizeof(iap_ack))) {
		dev_err(&client->dev,
                "failed to enter IAP: %*ph (expected %*ph)\n",
                (int)sizeof(buf), buf, (int)sizeof(iap_ack), iap_ack);
		return -EIO;
	}

	dev_info(&client->dev, "successfully entered IAP mode");

	send_id = client->addr;
	error = elants_i2c_send(client, &send_id, 1);
	if (error) {
		dev_err(&client->dev, "sending dummy byte failed: %d\n",
                error);
		return error;
	}

	/* Clear the last page of Master */
	error = elants_i2c_send(client, fw->data, ELAN_FW_PAGESIZE);
	if (error) {
		dev_err(&client->dev, "clearing of the last page failed: %d\n",
                error);
		return error;
	}

	error = elants_i2c_read(client, buf, 2);
	if (error) {
		dev_err(&client->dev,
                "failed to read ACK for clearing the last page: %d\n",
                error);
		return error;
	}

	n_fw_pages = fw->size / ELAN_FW_PAGESIZE;
	dev_dbg(&client->dev, "IAP Pages = %d\n", n_fw_pages);

	for (page = 0; page < n_fw_pages; page++) {
		error = elants_i2c_fw_write_page(client,
                                         fw->data + page * ELAN_FW_PAGESIZE);
		if (error) {
			dev_err(&client->dev,
                    "failed to write FW page %d: %d\n",
                    page, error);
			return error;
		}
	}

	/* Old iap needs to wait 200ms for WDT and rest is for hello packets */
	msleep(300);

	dev_info(&client->dev, "firmware update completed\n");
	return 0;
}
#endif
#ifdef ELAN_HID_I2C

static int CheckISPstatus(struct i2c_client *client)
{
	int len;
	int i;
	int err = 0;
	const u8 checkstatus[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x18};
	u8 buf[67] = {0};

	len = elants_i2c_send(client, checkstatus, sizeof(checkstatus));
	if (len) {
		dev_err(&client->dev,
				"[elan] %s ERROR: Flash key fail!len=%d\n",__func__, len);
		err =  -EINVAL;
		goto err_send_flash_key_cmd_fail;
	} else
		dev_dbg(&client->dev,
				"[elan] check status cmd = [%x:%x:%x:%x:%x:%x]\n",
				checkstatus[0], checkstatus[1], checkstatus[2],
				checkstatus[3], checkstatus[4], checkstatus[5]);

	msleep(20);
	
	len = elants_i2c_read(client, buf, sizeof(buf));
	if (len) {
		dev_err(&client->dev,
				"[elan] ERROR: Check Address Read Data error.len=%d\n",
				len);
		err = -EINVAL;
		goto err_recv_check_addr_fail;
	} else {
		dev_dbg(&client->dev, "[elan][Check status]: ");
		for (i = 0; i < (37+3)/8; i++) {
			dev_dbg(&client->dev,
					"%02x %02x %02x %02x %02x %02x %02x %02x",
					buf[i*8+0], buf[i*8+1], buf[i*8+2],
					buf[i*8+3], buf[i*8+4], buf[i*8+5],
					buf[i*8+6], buf[i*8+7]);
		}
		if (buf[4] == 0x56)
			return 0x56; /* return recovery mode*/
		else if(buf[4] == 0x20)
			return 0x20;
		else 
			return -1;
	}

	return 0;

 err_recv_check_addr_fail:
 err_send_flash_key_cmd_fail:
	return err;
}

static int HID_RecoveryISP(struct i2c_client *client)
{
	int len;
	int i;
	const u8 flash_key[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
                              0x04, 0x54, 0xc0, 0xe1, 0x5a};
	const u8 check_addr[37] = {0x04, 0x00, 0x23,	0x00, 0x03, 0x00, 0x01, 0x10};
	u8 buf[67] = {0};

	len = elants_i2c_send(client, flash_key, sizeof(flash_key));
	if (len) {
		dev_err(&client->dev,
				"[elan] %s ERROR: Flash key fail!len=%d\n",__func__, len);
		return -1;
	} else
		dev_dbg(&client->dev,
				"[elan] FLASH key cmd = [%2x:%2x:%2x:%2x]\n",
				flash_key[7], flash_key[8],
				flash_key[9], flash_key[10]);

	mdelay(40);

	len = elants_i2c_send(client, check_addr, sizeof(check_addr));
	if (len) {
		dev_err(&client->dev,
				"[elan] ERROR: Check Address fail!len=%d\n",
				len);
		return -1;
	} else
		dev_dbg(&client->dev,
				"[elan] Check Address cmd = [%2x:%2x:%2x:%2x]\n",
				check_addr[7], check_addr[8],
				check_addr[9], check_addr[10]);

	mdelay(20);
	len = elants_i2c_read(client, buf, sizeof(buf));
	if (len) {
		dev_dbg(&client->dev,
				"[elan] ERROR: Check Address Read Data error.len=%d\n",
				len);
		return -1;
	} else {
		dev_dbg(&client->dev, "[elan][Check Addr]: ");
		for (i = 0; i < (37+3)/8; i++) {
			dev_dbg(&client->dev,
					"%02x %02x %02x %02x %02x %02x %02x %02x",
                    buf[i*8+0], buf[i*8+1], buf[i*8+2],
                    buf[i*8+3], buf[i*8+4], buf[i*8+5],
                    buf[i*8+6], buf[i*8+7]);
		}
	}
	return 0;
}

static int HID_EnterISPMode(struct i2c_client *client)
{
	int len;
	int i;
	const u8 flash_key[37]  = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
                               0x04, 0x54, 0xc0, 0xe1, 0x5a};
	const u8 isp_cmd[37]    = {0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
                               0x04, 0x54, 0x00, 0x12, 0x34};
	const u8 check_addr[37] = {0x04, 0x00, 0x23, 0x00, 0x03,
                               0x00, 0x01, 0x10};
	u8 buf[67] = {0};

	len = elants_i2c_send(client, flash_key, sizeof(flash_key));
	if (len) {
		dev_err(&client->dev,
				"[elan] %s ERROR: Flash key fail!len=%d\r\n",__func__, len);
		return -1;
	} else
		dev_dbg(&client->dev,
				"[elan] FLASH key cmd = [%2x, %2x, %2x, %2x]\n",
				flash_key[7], flash_key[8],
				flash_key[9], flash_key[10]);
	mdelay(20);

	len = elants_i2c_send(client, isp_cmd, sizeof(isp_cmd));
	if (len) {
		dev_err(&client->dev,
				"[elan] ERROR: EnterISPMode fail!len=%d\r\n",
				len);
		return -1;
	} else
		dev_info(&client->dev,
                 "[elan] IAPMode data cmd = [%2x, %2x, %2x, %2x]\n",
                 isp_cmd[7], isp_cmd[8],
                 isp_cmd[9], isp_cmd[10]);

	mdelay(20);
	len = elants_i2c_send(client, check_addr, sizeof(check_addr));
	if (len) {
		dev_err(&client->dev,
				"[elan] ERROR: Check Address fail!len=%d\r\n",
				len);
		return -1;
	} else
		dev_dbg(&client->dev,
				"[elan] Check Address cmd = [%2x, %2x, %2x, %2x]\n",
				check_addr[7], check_addr[8],
				check_addr[9], check_addr[10]);

	mdelay(20);
	len = elants_i2c_read(client, buf, sizeof(buf));
	if (len) {
		dev_err(&client->dev,
				"[elan] ERROR: Check Address Read Data error.len=%d\n",
				len);
		return -1;
	} else {
		dev_dbg(&client->dev, "[Check Addr]: ");
		for (i = 0; i < (37+3)/8; i++)
			dev_dbg(&client->dev,
					"%02x %02x %02x %02x %02x %02x %02x %02x",
					buf[i*8+0], buf[i*8+1], buf[i*8+2],
					buf[i*8+3], buf[i*8+4], buf[i*8+5],
					buf[i*8+6], buf[i*8+7]);
	}

	return 0;
}

static int WritePage(struct i2c_client *client, u8 * szPage, int byte, int which)
{
	int len;

	len = elants_i2c_send(client, szPage,  byte);
	if (len) {
		dev_err(&client->dev,
				"[elan] ERROR: write the %d th page error,len=%d\n",
				which, len);
		return -1;
	}

	return 0;
}

static int GetAckData_HID(struct i2c_client *client)
{
	int len;


	u8 ack_buf[67] = { 0 };
    //	len = elan_ts_poll();
	mdelay(100);
	len = elants_i2c_read(client, ack_buf, 67);
	
	dev_info(&client->dev,
             "[elan] %s: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
             __func__ , ack_buf[0], ack_buf[1],
             ack_buf[2], ack_buf[3], ack_buf[4],
             ack_buf[5], ack_buf[6], ack_buf[7],
             ack_buf[8], ack_buf[9], ack_buf[10], ack_buf[11]);

	if (ack_buf[4] == 0xaa && ack_buf[5] == 0xaa)
		return ACK_OK;
	else if (ack_buf[4] == 0x55 && ack_buf[5] == 0x55)
		return ACK_REWRITE;
	else
		return ACK_Fail;

	return 0;
}

static int SendEndCmd(struct i2c_client *client)
{
	int len;
	const u8 send_cmd[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x1A};

	len = elants_i2c_send(client, send_cmd, sizeof(send_cmd));
	if (len) {
		dev_err(&client->dev,
				"[elan] ERROR: Send Cmd fail!len=%d\r\n", len);
		return -1;
	} else
		dev_dbg(&client->dev,
				"[elan] check send cmd = [%x, %x, %x, %x, %x, %x]\n",
				send_cmd[0], send_cmd[1], send_cmd[2],
				send_cmd[3], send_cmd[4], send_cmd[5]);
	return 0;
}

static int elants_i2c_validate_remark_id_HID_I2C(struct elants_data *ts,
					 const struct firmware *fw)
{
	struct i2c_client *client = ts->client;
	int error;
	const u8 cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00, 0x06,
	CMD_HEADER_ROM_READ, 0x80, 0x1F, 0x00, 0x00, 0x21 };
	u8 resp[67] = { 0 };
	u16 ts_remark_id = 0;
	u16 fw_remark_id = 0;
	/* Compare TS Remark ID and FW Remark ID */
	error = elants_i2c_execute_command(client, cmd, sizeof(cmd),
					resp, sizeof(resp));
	if (error) {
		dev_err(&client->dev, "failed to query Remark ID: %d\n", error);
		return error;
	}
	ts_remark_id = get_unaligned_be16(&resp[7]);
	fw_remark_id = get_unaligned_le16(&fw->data[fw->size - 4]);
	
	if(ts_remark_id == ELAN_REMARK_ID_OF_NON_REMARK_IC)
	{
		dev_info(&client->dev, "[ELAN] Pass, Non remark IC.\n");
		return 0;	// Just Pass if Non-Remark IC
	}
	else if(fw_remark_id != ts_remark_id) {
		dev_err(&client->dev,
			"Remark ID Mismatched: ts_remark_id=0x%04x, fw_remark_id=0x%04x.\n",
			ts_remark_id, fw_remark_id);
		return -EINVAL;
	}
	else
		return 0;
}

static int elants_i2c_do_update_firmware_HID_I2C(struct i2c_client *client,
                                                 const struct firmware *fw)
{
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	int res = 0;
	int iPage = 0;
	int j = 0;
	int write_times = 142;
	//u8 data;
	int byte_count;
	const u8 *szBuff;
	const u8 *fw_data;
	//const struct firmware *p_fw_entry;
	u8 write_buf[37] = {0x04, 0x00, 0x23, 0x00, 0x03,
                        0x21, 0x00, 0x00, 0x1c};
    u8 cmd_iap_write[37] = {0x04, 0x00, 0x23, 0x00, 0x03, 0x22};
	//const u8 cmd_idel[37] = {0x04, 0x00, 0x23, 0x00, 0x03,
    //                         0x00, 0x04, 0x54, 0x2c, 0x03, 0x01};
	int curIndex = 0;
	int offset = 0;
	int retry_num = 0;
	int fw_size;
    //	char *fw_name;
	//char fw_local_path[50];
	int PageNum = 0;
	int FlashPageNum = 30;
	int RemainPage;
	int LastStartPage;
	int PageSize = 132;
	int PayloadLen = 28;
	int PayloadLenRemain;
	int PayloadLenRemainLast;
	int error;
	bool check_remark_id = true/*ts->iap_version >= 0x60*/;
	
	dev_dbg(&ts->client->dev, "[elan] enter %s.\n", __func__);
	
	fw_data = fw->data;
	fw_size = fw->size;
	PageNum = fw_size/132;
	
	//disable_irq(client->irq);  //disable outside
	
	RemainPage = PageNum % FlashPageNum;
	LastStartPage = PageNum - RemainPage + 1;
	PayloadLenRemain = (FlashPageNum * PageSize) % PayloadLen;
	PayloadLenRemainLast = (RemainPage * PageSize) % PayloadLen;
	dev_info(&ts->client->dev,
             "[ELAN] PageNum=%d, FlashPageNum=%d, RemainPage=%d, LastStartPage=%d\n",
             PageNum, FlashPageNum, RemainPage, LastStartPage);
	dev_info(&ts->client->dev,
             "[ELAN] PageSize=%d, PayloadLen=%d, PayloadLenRemain=%d, PayloadLenRemainLast=%d\n",
             PageSize, PayloadLen, PayloadLenRemain, PayloadLenRemainLast);
												
 IAP_RESTART:
	//elants_i2c_sw_reset(client);
	elants_i2c_hw_reset(client);
	msleep(200);
	res = CheckISPstatus(ts->client);
	dev_info(&ts->client->dev,
             "[elan] res: 0x%x\n", res);
			 
	//解正印
	if (check_remark_id) {
		error = elants_i2c_validate_remark_id_HID_I2C(ts, fw);
		if (error)
			return error;
	}
		
	if (res == 0x56) { /* 0xa6 recovery mode  */
		//elants_i2c_sw_reset(client);
		//elants_i2c_hw_reset(client);
		//mdelay(200);
		dev_info(&ts->client->dev, "[elan hid iap] Recovery mode\n");
		res = HID_RecoveryISP(ts->client);
	} else if(res == 0x20){
		dev_info(&ts->client->dev, "[elan hid iap] Normal mode\n");
		res = HID_EnterISPMode(ts->client);/*enter HID ISP mode*/
	}else {
		dev_info(&ts->client->dev, "[elan hid iap] CheckISPstatus fail\n");
	}
	
	mdelay(50);
	
	for (iPage = 1; iPage <= PageNum; iPage +=FlashPageNum) {
		offset = 0;
		if (iPage == LastStartPage)
			write_times = (RemainPage * PageSize / PayloadLen) + 1;
		else
			write_times = (FlashPageNum * PageSize / PayloadLen) + 1;  /*30*132=3960, 3960=141*28+12*/

		mdelay(5);
		for (byte_count = 1; byte_count <= write_times; byte_count++) {
			mdelay(1);
			if (byte_count != write_times) {
				/* header + data = 9+28=37 bytes*/
				szBuff = fw_data + curIndex;
				/*payload length = 0x1c => 28??*/
				write_buf[8] = PayloadLen;
				write_buf[7] = offset & 0x00ff;/*===>0?*/
				write_buf[6] = offset >> 8;/*==0?*/
				offset += PayloadLen;
				curIndex =  curIndex + PayloadLen;
				for (j = 0; j < PayloadLen; j++)
					write_buf[j+9] = szBuff[j];
				res = WritePage(ts->client, write_buf,
                                37, iPage);
			} else if ((iPage == LastStartPage) && (byte_count == write_times)) {
				/*the final page, header + data = 9+20=29 bytes,
				 *the rest of bytes are the previous page's data
				 */
				dev_dbg(&ts->client->dev, "[elan] Final Page...\n");
				szBuff = fw_data + curIndex;
				/*payload length = 0x14 => 20*/
				write_buf[8] = PayloadLenRemainLast;
				write_buf[7] = offset & 0x00ff;
				write_buf[6] = offset >> 8;
				curIndex =  curIndex + PayloadLenRemainLast;
				for (j = 0; j < PayloadLenRemainLast; j++)
					write_buf[j+9] = szBuff[j];
				res = WritePage(ts->client, write_buf,
                                37, iPage);
			} else {
				/*last run of this 30 page,
				 *header + data = 9+12=21 bytes,
				 *the rest of bytes are the previous page's data
				 */
				szBuff = fw_data + curIndex;
				/*payload length = 0x0c => 12*/
				write_buf[8] = PayloadLenRemain;
				write_buf[7] = offset & 0x00ff;
				write_buf[6] = offset >> 8;
				curIndex =  curIndex + PayloadLenRemain;
				for (j = 0; j < PayloadLenRemain; j++)
					write_buf[j+9] = szBuff[j];
				res = WritePage(ts->client, write_buf,
                                37, iPage);
			}
		} /*end of for(byte_count=1;byte_count<=17;byte_count++)*/

		mdelay(200);
		res = WritePage(ts->client, cmd_iap_write,
                        37, iPage);
		mdelay(200);
		dev_dbg(&ts->client->dev, "[iap] iPage=%d :", iPage);
		res = GetAckData_HID(ts->client);
		if (res < 0) {
			if (retry_num < 1) {
				dev_err(&ts->client->dev, "[elan] Update Firmware retry_num=%d fail!!\n",
						retry_num);
				retry_num++;
				goto IAP_RESTART;
			} else {
				dev_err(&ts->client->dev,
						"[elan] Update Firmware retry_num=%d!!\n",
						retry_num);
			}
		}
		mdelay(10);
	} /*end of for(iPage = 1; iPage <= PageNum; iPage++)*/
	
	res = SendEndCmd(ts->client);
	mdelay(200);
	//elants_i2c_sw_reset(client);
	elants_i2c_hw_reset(client);
	mdelay(200);
	//elants_i2c_send(ts->client, cmd_idel, 37);
	dev_info(&ts->client->dev, "[elan] Update Firmware successfully!\n");
	//enable_irq(client->irq);   //enable outside

	return 0;
}
#endif

static int elants_i2c_fw_update(struct elants_data *ts)
{
	struct i2c_client *client = ts->client;
	const struct firmware *fw;
	int error;

	dev_info(&client->dev, "requesting firmware 'elants_i2c.bin'\n");
	error = request_firmware(&fw, "elants_i2c.bin", &client->dev);
	if (error) {
		dev_info(&client->dev, "failed to request firmware: %d\n",
                   error);
		return error;
	}

	if (fw->size % ELAN_FW_PAGESIZE) {
		dev_err(&client->dev, "invalid firmware length: %zu\n",
                fw->size);
		error = -EINVAL;
		goto out;
	}

	disable_irq(client->irq);

#ifdef ELAN_HID_I2C
	error = elants_i2c_do_update_firmware_HID_I2C(client, fw);
#else
	error = elants_i2c_do_update_firmware(client, fw,
                                          ts->iap_mode == ELAN_IAP_RECOVERY);
#endif
	if (error) {
		dev_err(&client->dev, "firmware update failed: %d\n", error);
		ts->iap_mode = ELAN_IAP_RECOVERY;
		goto out_enable_irq;
	}

	error = elants_i2c_initialize(ts);
	if (error) {
		dev_err(&client->dev,
                "failed to initialize device after firmware update: %d\n",
                error);
		ts->iap_mode = ELAN_IAP_RECOVERY;
		goto out_enable_irq;
	}

	ts->iap_mode = ELAN_IAP_OPERATIONAL;

 out_enable_irq:
	ts->state = ELAN_STATE_NORMAL;
	enable_irq(client->irq);
	msleep(100);

	if (!error)
		elants_i2c_calibrate(ts);
 out:
	release_firmware(fw);
	return error;
}

/*
 * Event reporting.
 */

static void elants_i2c_mt_event(struct elants_data *ts, u8 *buf)
{
	struct input_dev *input = ts->input;
	unsigned int n_fingers;
	u16 finger_state;
	int i;

	n_fingers = buf[FW_POS_STATE + 1] & 0x0f;
	finger_state = ((buf[FW_POS_STATE + 1] & 0x30) << 4) |
        buf[FW_POS_STATE];

	dev_dbg(&ts->client->dev,
            "n_fingers: %u, state: %04x\n",  n_fingers, finger_state);

	for (i = 0; i < MAX_CONTACT_NUM && n_fingers; i++) {
		if (finger_state & 1) {
			unsigned int x, y, p, w;
			u8 *pos;

			pos = &buf[FW_POS_XY + i * 3];
			x = (((u16)pos[0] & 0xf0) << 4) | pos[1];
			y = (((u16)pos[0] & 0x0f) << 8) | pos[2];
			p = buf[FW_POS_PRESSURE + i];
			w = buf[FW_POS_WIDTH + i];

			dev_dbg(&ts->client->dev, "i=%d x=%d y=%d p=%d w=%d\n",
                    i, x, y, p, w);

			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
			input_event(input, EV_ABS, ABS_MT_POSITION_X, x);
			input_event(input, EV_ABS, ABS_MT_POSITION_Y, y);
			input_event(input, EV_ABS, ABS_MT_PRESSURE, p);
			input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, w);

			n_fingers--;
		}

		finger_state >>= 1;
	}

	input_mt_sync_frame(input);
	pm_wakeup_event(&ts->client->dev, 350);
	input_sync(input);
}

/*
 * HID_I2C Event reporting.
 */
#ifdef ELAN_HID_I2C
static int elan_ktf_hid_parse_xy(u8 *data, uint16_t *x, uint16_t *y)
{
    *x = *y = 0;

    *x = (data[6]);
    *x <<= 8;
    *x |= data[5];

    *y = (data[10]);
    *y <<= 8;
    *y |= data[9];

    return 0;
}

static int elan_ts_parse_pen(u8 *data, uint16_t *x, uint16_t *y, uint16_t *p)
{

	*x = *y = *p = 0;
	*x = data[5];
	*x <<= 8;
	*x |= data[4];

	*y = data[7];
	*y <<= 8;
	*y |= data[6];

	*p = data[9];
	*p <<= 8;
	*p |= data[8];

	return 0;
}

static int mTouchStatus[MAX_CONTACT_NUM] = {0};
void force_release_pos(struct i2c_client *client)
{
    //struct elants_data *ts = i2c_get_clientdata(client);
    struct elants_data *ts = private_ts;
    struct input_dev *idev = ts->input;
    int i;
    for (i=0; i < MAX_CONTACT_NUM; i++) {
        if (mTouchStatus[i] == 0) continue;
        input_mt_slot(idev, i);
        input_mt_report_slot_state(idev, MT_TOOL_FINGER, 0);
        mTouchStatus[i] = 0;
    }

		pm_wakeup_event(&ts->client->dev, 350);
    input_sync(idev);
}

static void elants_i2c_mt_event_HID_I2C_Finger(struct elants_data *ts, u8 *buf)
{
	//struct input_dev *input = ts->input;
	struct input_dev *idev = ts->input;
	//unsigned int n_fingers;
	//u16 finger_state;
	int i;
	unsigned int idx, num;
	int finger_id;
	int finger_num;
	unsigned int active = 0;
	uint16_t x;
	uint16_t y;
	uint16_t p;
	static uint16_t gwLastPressure=0;
	
//	printk("[elan]:enter ELAN_HID_I2C_Finger_PKT\n");
	
	finger_num = buf[1];
	//if (finger_num > 5)
    //finger_num = 5;   /* support 5 fingers */
	idx=3;
    num = finger_num;
	
    for(i = 0; i < finger_num; i++) {
        if ((buf[idx]&0x03) == 0x00)	
			active = 0;   /* 0x03: finger down, 0x00 finger up  */
        else	
			active = 1;

        if ((buf[idx] & 0x03) == 0) 
			num --;
		
        finger_id = (((buf[idx] & 0xfc) >> 2) -1);
		
        input_mt_slot(idev, finger_id);
        input_mt_report_slot_state(idev, MT_TOOL_FINGER, true);
		input_report_key(idev, BTN_TOUCH, 1);
        
            elan_ktf_hid_parse_xy(&buf[idx], &x, &y);
		
//		printk ("X = %d (max_x = %d)\n",x, ts->x_max);
//		printk ("Y = %d (max_y = %d)\n",y, ts->y_max);

//		int tmp_x= x;
//		int tmp_y= y;

		x = (x * screen_max_x / ts->x_max);
		y = (y * screen_max_y / ts->y_max);

//		printk ("original X [%d] = (%d * %d/ %d)\n",x, tmp_x, screen_max_x, ts->x_max);
//		printk ("original Y [%d] = (%d * %d/ %d)\n",y, tmp_y, screen_max_y, ts->y_max);

		if (revert_x_flag) {
			x = screen_max_x - x;
		}
		if (revert_y_flag) {
			y = screen_max_y - y;
		}
		if (exchange_x_y_flag) {
			uint16_t tmp = x;
			x = y;
			y = tmp;
		}
//		printk ("converted X [%d]\n" , x);
//		printk ("converted Y [%d]\n" , y);

			/*x = x * 1600 / 1856;
			y = y * 2560 / 3008;
			y = 2560 - y;
			*/
#if 1
			if(0==p) {
				printk("%s : touch down without pressure,last pressure=%d\n",
						__func__,(int)gwLastPressure);
				p = gwLastPressure;
			}
			else {
				gwLastPressure = p;
			}
#endif
			
		if(active) {
			p = ((((buf[idx + 2] & 0x0f) << 4) |  (buf[idx + 1] & 0x0f)) << 1);
			p = (p > 254 ? 254 : p) << 4;
		}
		else {
			p = 0;
		}
		printk (KERN_DEBUG"elants:[0x%02x] (%d,%d),p=%d\n",buf[idx],x,y,p);
            input_report_abs(idev, ABS_MT_TOUCH_MAJOR, p);
            input_report_abs(idev, ABS_MT_PRESSURE, p);
            input_report_abs(idev, ABS_MT_POSITION_X, x);
            input_report_abs(idev, ABS_MT_POSITION_Y, y);
            //printk("[elan hid] DOWN i=%d finger_id=%d x=%d y=%d Finger NO.=%d \n", i, finger_id, x, y, finger_num);
		if(!active) {
			input_mt_slot(idev, finger_id);
			input_mt_report_slot_state(idev, MT_TOOL_FINGER, false);
			//printk("[elan hid] UP i=%d finger_id=%d NO.=%d \n", i, finger_id, finger_num);
		}
        mTouchStatus[i] = active;
        idx += 11;
    }
    if (num == 0) {
        printk("[elan] Release ALL Finger\n");
        input_report_key(idev, BTN_TOUCH, 0); //for all finger up
        force_release_pos(ts->client);
    }
	
		pm_wakeup_event(&ts->client->dev, 350);
    input_sync(idev);
}

static void elants_i2c_mt_event_HID_I2C_Pen(struct elants_data *ts, u8 *buf)
{
	struct input_dev *idev = ts->input;
	int pen_hover = 0;
	int pen_down = 0;
	int pen_key = 0;
	int battery = 0;
	uint16_t p = 0;
	uint16_t x = 0;
	uint16_t y = 0;

	pen_hover = buf[3] & 0x1;
	pen_down = buf[3] & 0x03;
	pen_key = buf[3];
	battery = buf[10];

#ifdef CONFIG_ANDROID_NTX
	if(pen_key == 0x3 || pen_key == 0x7 || pen_key == 0x11) {
		pen_down = 1;
	} else {
		pen_down = 0;
	}
#endif
//	printk("[elan]:enter ELAN_HID_I2C_Pen_PKT\n");
	

	input_mt_slot(idev, 0);
	//input_mt_report_slot_state(idev, MT_TOOL_FINGER, pen_hover);
	input_mt_report_slot_state(idev, MT_TOOL_PEN, true);
	if (pen_key) {
		dev_dbg(&ts->client->dev, 
				"[elan] report pen key %02x\n", pen_key);
		/*elan_ts_report_key(ts,pen_key);*/
	}
	
	//if (pen_hover) 
	{
		elan_ts_parse_pen(&buf[0], &x, &y, &p);
		y = y * screen_max_y / ((ts->cols-1) * 256);
		x = x * screen_max_x / ((ts->rows-1) * 256);

		dev_dbg(&ts->client->dev,
				"[elan] pen id--------=%d x=%d y=%d\n",
				p, x, y);

		if (revert_x_flag) {
			x = screen_max_x - x;
		}
		if (revert_y_flag) {
			y = screen_max_y - y;
		}
		if (exchange_x_y_flag) {
			uint16_t tmp = x;
			x = y;
			y = tmp;
		}
		
		/*
		 *  0x01 : hover without up/down pen key
		 *  0x05 : hover with up pen key
		 *  0x09 : hover with down pen key
		 *  0x03 : contact without up/down pen key
		 *  0x07 : contact with up pen key
		 *  0x11 : contact with down pen key
		 */
		if (pen_key == 0x01 || pen_key == 0x05 || pen_key == 0x09 || 0==pen_down) {  /* report hover function  */
			input_report_abs(idev, ABS_MT_PRESSURE, 0);
			input_report_abs(idev, ABS_MT_DISTANCE, 15);
			dev_dbg(&ts->client->dev, "[elan pen] Hover DISTANCE=15\n");
		} else {
			input_report_abs(idev, ABS_MT_TOUCH_MAJOR, p);
			input_report_abs(idev, ABS_MT_PRESSURE, p);
			input_report_abs(idev, ABS_MT_DISTANCE, 0);
			
			dev_dbg(&ts->client->dev,
					"[elan pen] PEN PRESSURE=%d\n", p);
		}
#ifdef CONFIG_ANDROID_NTX
		if (pen_down == 1) {
			input_report_key(idev, BTN_TOUCH, 1);
			/*report pen key*/
			if(pen_key == 0x5 || pen_key == 0x7) {
				input_report_key(idev, BTN_STYLUS, 1);	//btn1: barrel
			} else {
				input_report_key(idev, BTN_STYLUS, 0);	//btn1: barrel
			}
            
			if(pen_key == 0x9 || pen_key == 0x11) {
				input_report_key(idev, BTN_STYLUS2, 1);	//btn2: eraser
			} else {
				input_report_key(idev, BTN_STYLUS2, 0);	//btn2: eraser
			}
		}
#else
		input_report_key(idev, BTN_TOUCH, 1);
#endif
        // input_report_abs(ts->input, ABS_MT_TRACKING_ID, 0);
		input_report_abs(idev, ABS_MT_POSITION_X, x);
		input_report_abs(idev, ABS_MT_POSITION_Y, y);
        //	input_mt_sync(ts->input);
			
	} //else
    //input_mt_sync(ts->input);
#ifndef CONFIG_ANDROID_NTX
	/*report pen key*/
	input_report_key(idev, BTN_STYLUS, buf[3] & 0x04);       //btn1: barrel
	input_report_key(idev, BTN_STYLUS2, buf[3] & 0x10);     //btn2: eraser
#endif
		dev_dbg(&ts->client->dev,
            "[elan pen] %x:%x:%x:%x:%x:%x:%x:%x\n",
            buf[0], buf[1], buf[2], buf[3],
            buf[4], buf[5], buf[6], buf[7]);
		dev_dbg(&ts->client->dev,
            "[elan] x=%d y=%d p=%d\n", x, y, p);
	/*
	* battery:
	*	1: normal
	*	0: low battery
	*/
	if (pen_down){
		if(battery) {
			dev_dbg(&ts->client->dev, "normal, battery = %d\n",battery);
		} else {
			dev_dbg(&ts->client->dev, "low battery,  battery = %d\n",battery);
		}
		input_report_key(idev, KEY_BATTERY, battery);	//#define KEY_BATTERY		236
	}

	if (pen_down == 0) {
		/*All Finger Up*/
		//input_report_key(ts->input, BTN_TOUCH, 0);
		//input_report_key(ts->input, BTN_TOOL_PEN, 0);
		//input_mt_sync(ts->input);
		input_mt_slot(idev, 0);
		input_mt_report_slot_state(idev,
                                   MT_TOOL_PEN, false);
		input_report_key(idev, BTN_TOUCH, 0); //for all finger up
#ifdef CONFIG_ANDROID_NTX
		/*report pen key*/
		input_report_key(idev, BTN_STYLUS, 0);	//btn1: barrel
		input_report_key(idev, BTN_STYLUS2, 0);	//btn2: eraser
#endif
		//force_release_pos(ts->client);
	}
	pm_wakeup_event(&ts->client->dev, 350);
	input_sync(idev);
}
#endif

static u8 elants_i2c_calculate_checksum(u8 *buf)
{
	u8 checksum = 0;
	u8 i;

	for (i = 0; i < FW_POS_CHECKSUM; i++)
		checksum += buf[i];

	return checksum;
}

static void elants_i2c_event(struct elants_data *ts, u8 *buf)
{
	u8 checksum = elants_i2c_calculate_checksum(buf);

	if (unlikely(buf[FW_POS_CHECKSUM] != checksum))
		dev_warn(&ts->client->dev,
                 "%s: invalid checksum for packet %02x: %02x vs. %02x\n",
                 __func__, buf[FW_POS_HEADER],
                 checksum, buf[FW_POS_CHECKSUM]);
	else if (unlikely(buf[FW_POS_HEADER] != HEADER_REPORT_10_FINGER))
		dev_warn(&ts->client->dev,
                 "%s: unknown packet type: %02x\n",
                 __func__, buf[FW_POS_HEADER]);
	else
		elants_i2c_mt_event(ts, buf);
}



static void  elan_ts_work_func(struct work_struct *work)
{
	const u8 wait_packet[] = { 0x64, 0x64, 0x64, 0x64 };
	struct elants_data *ts = private_ts;//_dev;
	struct i2c_client *client = ts->client;
	int report_count, report_len;
	int i;
	int len;
	u8 buf1[PACKET_SIZE] = { 0 };

	if (gpio_get_value(ts->intr_gpio))
    {
        printk("[elan] Detected the jitter on INT pin");
		//enable_irq(client->irq);
        return; //IRQ_HANDLED;
    }
	
	len = i2c_master_recv(client, ts->buf, 67/*sizeof(ts->buf)*/);
	if (len < 0) {
		dev_err(&client->dev, "%s: failed to read data: %d\n",
                __func__, len);
		goto out;
	}
	//dev_err(&client->dev, "done nothing\n");
	//goto out;
	//dev_info(&client->dev, "%s: packet %*ph\n",
//             __func__, HEADER_SIZE, ts->buf);
#ifdef CALC_REPORT_RATE//[
	static int giPtCnt = 0;
	static unsigned long gdwLastJiffies = 0;

	unsigned long dwCurJiffies = jiffies,dwDiffTicks;

	if((ts->buf[3]&0x03) == 3){
		giPtCnt++;

		if(0==gdwLastJiffies) {
		gdwLastJiffies = jiffies;
	}
}
#endif //]CALC_REPORT_RATE

	switch (ts->state) {
	case ELAN_WAIT_RECALIBRATION:
#ifndef ELAN_HID_I2C	
		if (ts->buf[FW_HDR_TYPE] == CMD_HEADER_REK) {
			memcpy(ts->cmd_resp, ts->buf, sizeof(ts->cmd_resp));
			complete(&ts->cmd_done);
			ts->state = ELAN_STATE_NORMAL;
		}
#else
		if (ts->buf[FW_HDR_TYPE + 4] == CMD_HEADER_REK) {
			memcpy(ts->cmd_resp, ts->buf, sizeof(ts->cmd_resp));
			complete(&ts->cmd_done);
			ts->state = ELAN_STATE_NORMAL;

			dev_info(&client->dev, "[elan] Calibration: %2x,%2x,%2x,%2x,%2x,%2x,%2x,%2x\n", 
                     ts->buf[0], ts->buf[1], ts->buf[2], ts->buf[3], ts->buf[4], ts->buf[5], ts->buf[6], ts->buf[7]);
		}
#endif
		break;

	case ELAN_WAIT_QUEUE_HEADER:
		if (ts->buf[FW_HDR_TYPE] != QUEUE_HEADER_NORMAL)
			break;

		ts->state = ELAN_STATE_NORMAL;
		/* fall through */

	case ELAN_STATE_NORMAL:

		switch (ts->buf[FW_HDR_TYPE]) {
#ifndef ELAN_HID_I2C		
		case CMD_HEADER_HELLO:  //filter special packet: 0x55 0x52 0x66
		case CMD_HEADER_RESP:
		case CMD_HEADER_REK:
			break;
#else
		case CMD_HEADER_HID_I2C:  //0x43
			if (ts->buf[FW_HDR_TYPE + 4] == CMD_HEADER_REK)  //43 00 02 04 66 XX XX XX
				dev_info(&client->dev, "[elan] Calibration:");
			if (ts->buf[FW_HDR_TYPE + 4] == CMD_HEADER_RESP) //43 00 02 04 52 XX XX XX
				dev_info(&client->dev, "[elan] Response:");

			dev_info(&client->dev, "[elan] %2x,%2x,%2x,%2x,%2x,%2x,%2x,%2x\n", 
                     ts->buf[0], ts->buf[1], ts->buf[2], ts->buf[3], ts->buf[4], ts->buf[5], ts->buf[6], ts->buf[7]);
			memcpy(gReadTS,ts->buf,16);
			nActivePenStatus = 1;
			wake_up_interruptible(&pen_status);
			//printk("=== wake up ====\n");

			break;
#endif

			
			
		case QUEUE_HEADER_WAIT:  //0x64
			if (memcmp(ts->buf, wait_packet, sizeof(wait_packet))) {
				dev_err(&client->dev,
                        "invalid wait packet %*ph\n",
                        HEADER_SIZE, ts->buf);
			} else {
				ts->state = ELAN_WAIT_QUEUE_HEADER;
				udelay(30);
			}
			break;

		case QUEUE_HEADER_SINGLE:  //no buffer mode = 0x62
			elants_i2c_event(ts, &ts->buf[HEADER_SIZE]);
			break;

		case QUEUE_HEADER_NORMAL:  //buffer mode = 0x63
			report_count = ts->buf[FW_HDR_COUNT];
			if (report_count == 0 || report_count > 3) {
				dev_err(&client->dev,
                        "bad report count: %*ph\n",
                        HEADER_SIZE, ts->buf);
				break;
			}

			report_len = ts->buf[FW_HDR_LENGTH] / report_count;
			if (report_len != PACKET_SIZE) {
				dev_err(&client->dev,
                        "mismatching report length: %*ph\n",
                        HEADER_SIZE, ts->buf);
				break;
			}

			for (i = 0; i < report_count; i++) {
				u8 *buf = ts->buf + HEADER_SIZE +
                    i * PACKET_SIZE;
				elants_i2c_event(ts, buf);
			}
			break;
#ifdef ELAN_HID_I2C
		case HID_I2C_FINGER_HEADER: //HID_I2C header = 0x3F
			ts->buf[1] = ts->buf[62];  //store finger number
			if(ts->buf[62] > 5)  //more than 5 finger
            {
				 dev_err(&client->dev, "%s: second packet buf[62]: %x\n",__func__, ts->buf[62]);
                len = i2c_master_recv(client, buf1, sizeof(buf1));
                if (len < 0) {
                    dev_err(&client->dev, "%s: failed to read data: %d\n",
                            __func__, len);
                     goto out;
                }
                for (i=3; i<67; i++)
                    ts->buf[55+i] = buf1[i];
                }

			elants_i2c_mt_event_HID_I2C_Finger(ts, ts->buf);
			break;
			
		//case HID_I2C_PEN_HEADER: //HID_I2C PEN header = 0x0D
		case HID_I2C_PEN_HEADER_0D: //HID_I2C PEN header = 0x0D
		case HID_I2C_PEN_HEADER_13: //HID_I2C PEN header = 0x13
			elants_i2c_mt_event_HID_I2C_Pen(ts, ts->buf);
			break;
#endif
		default:
			dev_err(&client->dev, "unknown packet %*ph\n",
                    HEADER_SIZE, ts->buf);
			break;
		}
		break;
	}

 out:
 #ifdef CALC_REPORT_RATE//[
	if( (3 != (ts->buf[3]&0x03)  && giPtCnt) { //if hover -> clean
#ifdef PRINT_REPORT_RATE//[
		dwDiffTicks = dwCurJiffies-gdwLastJiffies;
		printk("report rate=%d (%d interrupts in %d ms)\n",giPtCnt*1000/jiffies_to_msecs(dwDiffTicks),giPtCnt,jiffies_to_msecs(dwDiffTicks));
#endif //] PRINT_REPORT_RATE
		giPtCnt = 0;
		gdwLastJiffies = 0;
	}
#endif //]CALC_REPORT_RATE
	
	//enable_irq(client->irq);
	return;// IRQ_HANDLED;
}

static irqreturn_t elants_i2c_irq(int irq, void *_dev)
{
	//struct elants_data *ts = (struct elants_data *)_dev;//private_ts;
    struct elants_data *ts = private_ts;//private_ts;
	//dev_err(&ts->client->dev, " %s enter\n", __func__);
	//disable_irq_nosync(ts->client->irq);
    if (giSuspendingState && !suspending_trigger) {
        suspending_trigger = 1;
    } else if (!giSuspendingState) 
        suspending_trigger = 0;
    
	queue_work(ts->elan_wq, &ts->ts_work);

	return IRQ_HANDLED;
}

static int elants_register_interrupt(struct i2c_client *client)
{
	int err = 0;

	if (client->irq) {
		err = request_threaded_irq(client->irq, NULL, elants_i2c_irq, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, DEVICE_NAME, private_ts);
		
		if (err < 0) {
			printk("%s(%s): Can't allocate irq %d\n", __FILE__, __func__, client->irq);
		}
//		enable_irq_wake (client->irq);
	}

	return 0;
}

/*
 * sysfs interface
 */
static ssize_t calibrate_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	struct i2c_client *client = ts->client;
	int error;

	error = mutex_lock_interruptible(&ts->sysfs_mutex);
	if (error)
		return error;

	error = elants_i2c_calibrate(ts);

	mutex_unlock(&ts->sysfs_mutex);
	return error ?: count;
}

static ssize_t write_update_fw(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	struct i2c_client *client = ts->client;
	int error;

	error = mutex_lock_interruptible(&ts->sysfs_mutex);
	if (error)
		return error;

	error = elants_i2c_fw_update(ts);
	dev_dbg(dev, "firmware update result: %d\n", error);

	mutex_unlock(&ts->sysfs_mutex);
	return error ?: count;
}

static ssize_t show_iap_mode(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	struct i2c_client *client = ts->client;

	return sprintf(buf, "%s\n",
                   ts->iap_mode == ELAN_IAP_OPERATIONAL ?
                   "Normal" : "Recovery");
}

static ssize_t hw_reset_H_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	struct i2c_client *client = ts->client;


#ifdef USE_IN_SUNXI
	gpiod_direction_output(ts->reset_gpio,0);
#else
	gpio_direction_output(ts->reset_gpio,0);    
#endif
    return count;
}			      

static ssize_t hw_reset_L_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	struct i2c_client *client = ts->client;

#ifdef USE_IN_SUNXI
	gpiod_direction_output(ts->reset_gpio,1);
#else
	gpio_direction_output(ts->reset_gpio,1);    
#endif
    return count;
}


static ssize_t get_active_pen(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int err = 0;

	nActivePenStatus = 0;
	err = elants_i2c_send(private_ts->client, ActivePenState,  sizeof(ActivePenState));
	if (err) {
		return -1;
	}
	
	wait_event_interruptible(pen_status,nActivePenStatus);

	if (gReadTS[6] & 0x02)
		err = sprintf(buf, "Active Pen Open \n");
	else
		err = sprintf(buf, "Active Pen Closed \n");

	
	return err;
}

static ssize_t set_active_pen(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int val = simple_strtoul(buf,NULL,10);
	int err = 0;


	nActivePenStatus = 0;
	err = elants_i2c_send(private_ts->client, ActivePenState,  sizeof(ActivePenState));
	if (err) {
		return -1;
	}
	wait_event_interruptible(pen_status,nActivePenStatus);

	if(val==1){ // Enable
		memcpy(ActivePenCmd+8,gReadTS+5,3);	// reserve 3 bytes
		ActivePenCmd[9] = (ActivePenCmd[9] | 0x02) ;
		printk("%x_%x_%x_%x\n",ActivePenCmd[7],ActivePenCmd[8],ActivePenCmd[9],ActivePenCmd[10]);

		err = elants_i2c_send(private_ts->client, ActivePenCmd,  sizeof(ActivePenCmd));
		if (err) {
			return -1;
		}
	}
	else if(val==0){ // Disable
		memcpy(ActivePenCmd+8,gReadTS+5,3); // reserve 3 bytes
		ActivePenCmd[9] = ActivePenCmd[9] & 0xfd ;

		printk("%x_%x_%x_%x\n",ActivePenCmd[7],ActivePenCmd[8],ActivePenCmd[9],ActivePenCmd[10]);

		err = elants_i2c_send(private_ts->client, ActivePenCmd,  sizeof(ActivePenCmd));
		if (err) {
			return -1;
		}
	}

	return count;
}

static DEVICE_ATTR(active_pen, 0644, get_active_pen, set_active_pen);
static DEVICE_ATTR(calibrate, S_IWUSR, NULL, calibrate_store);
static DEVICE_ATTR(iap_mode, S_IRUGO, show_iap_mode, NULL);
static DEVICE_ATTR(update_fw, S_IWUSR, NULL, write_update_fw);
static DEVICE_ATTR(reset_h, S_IWUSR, NULL, hw_reset_H_store);
static DEVICE_ATTR(reset_l, S_IWUSR, NULL, hw_reset_L_store);

struct elants_version_attribute {
	struct device_attribute dattr;
	size_t field_offset;
	size_t field_size;
};

#define __ELANTS_FIELD_SIZE(_field)					\
	sizeof(((struct elants_data *)NULL)->_field)
#define __ELANTS_VERIFY_SIZE(_field)                        \
	(BUILD_BUG_ON_ZERO(__ELANTS_FIELD_SIZE(_field) > 2) +   \
	 __ELANTS_FIELD_SIZE(_field))
#define ELANTS_VERSION_ATTR(_field)                                 \
	struct elants_version_attribute elants_ver_attr_##_field = {	\
		.dattr = __ATTR(_field, S_IRUGO,                            \
                        elants_version_attribute_show, NULL),       \
		.field_offset = offsetof(struct elants_data, _field),       \
		.field_size = __ELANTS_VERIFY_SIZE(_field),                 \
	}

static ssize_t elants_version_attribute_show(struct device *dev,
                                             struct device_attribute *dattr,
                                             char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	struct i2c_client *client = ts->client;

	struct elants_version_attribute *attr =
		container_of(dattr, struct elants_version_attribute, dattr);
	u8 *field = (u8 *)((char *)ts + attr->field_offset);
	unsigned int fmt_size;
	unsigned int val;

	if (attr->field_size == 1) {
		val = *field;
		fmt_size = 2; /* 2 HEX digits */
	} else {
		val = *(u16 *)field;
		fmt_size = 4; /* 4 HEX digits */
	}

	return sprintf(buf, "%0*x\n", fmt_size, val);
}

static ELANTS_VERSION_ATTR(fw_version);
static ELANTS_VERSION_ATTR(hw_version);
static ELANTS_VERSION_ATTR(test_version);
static ELANTS_VERSION_ATTR(solution_version);
static ELANTS_VERSION_ATTR(bc_version);
static ELANTS_VERSION_ATTR(iap_version);

static struct attribute *elants_attributes[] = {
	&dev_attr_calibrate.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_iap_mode.attr,
	&dev_attr_reset_h.attr,
	&dev_attr_reset_l.attr,
	&dev_attr_active_pen.attr,

	&elants_ver_attr_fw_version.dattr.attr,
	&elants_ver_attr_hw_version.dattr.attr,
	&elants_ver_attr_test_version.dattr.attr,
	&elants_ver_attr_solution_version.dattr.attr,
	&elants_ver_attr_bc_version.dattr.attr,
	&elants_ver_attr_iap_version.dattr.attr,
	NULL
};

static struct attribute_group elants_attribute_group = {
	.name = DEVICE_NAME,
	.attrs = elants_attributes,
};

static void elants_i2c_remove_sysfs_group(void *_data)
{
	struct elants_data *ts = _data;

	sysfs_remove_group(&ts->client->dev.kobj, &elants_attribute_group);
}

static int elants_i2c_power_on(struct elants_data *ts)
{
	int error;

#ifdef USE_IN_SUNXI
	return 0;
#else
    return 0;
#endif
	/*
	 * If we do not have reset gpio assume platform firmware
	 * controls regulators and does power them on for us.
	 */
	if (IS_ERR_OR_NULL(ts->reset_gpio))
		return 0;

	gpiod_set_value_cansleep(ts->reset_gpio, 1);

	error = regulator_enable(ts->vcc33);
	if (error) {
		dev_err(&ts->client->dev,
                "failed to enable vcc33 regulator: %d\n",
                error);
		goto release_reset_gpio;
	}

	error = regulator_enable(ts->vccio);
	if (error) {
		dev_err(&ts->client->dev,
                "failed to enable vccio regulator: %d\n",
                error);
		regulator_disable(ts->vcc33);
		goto release_reset_gpio;
	}

	/*
	 * We need to wait a bit after powering on controller before
	 * we are allowed to release reset GPIO.
	 */
	udelay(ELAN_POWERON_DELAY_USEC);

 release_reset_gpio:
	gpiod_set_value_cansleep(ts->reset_gpio, 0);
	if (error)
		return error;

	msleep(ELAN_RESET_DELAY_MSEC);

	return 0;
}

static void elants_i2c_power_off(void *_data)
{
	struct elants_data *ts = _data;

#ifdef USE_IN_SUNXI
	return;
#else
	return;    
#endif

	if (ts->unbinding) {
		dev_info(&ts->client->dev,
                 "Not disabling regulators to continue allowing userspace i2c-dev access\n");
		return;
	}

	if (!IS_ERR_OR_NULL(ts->reset_gpio)) {
		/*
		 * Activate reset gpio to prevent leakage through the
		 * pin once we shut off power to the controller.
		 */
		gpiod_set_value_cansleep(ts->reset_gpio, 1);
		regulator_disable(ts->vccio);
		regulator_disable(ts->vcc33);
	}
}

static int elants_i2c_probe(struct i2c_client *client,
                            const struct i2c_device_id *id)
{
	union i2c_smbus_data dummy;
	struct elants_data *ts;
	unsigned long irqflags;
	int error;

	dev_err(&client->dev, "[elan] enter elants_i2c_probe..0823_1..\n");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
                "%s: i2c check functionality error\n", DEVICE_NAME);
		return -ENXIO;
	}

	ts = devm_kzalloc(&client->dev, sizeof(struct elants_data), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;



	mutex_init(&ts->sysfs_mutex);
	init_completion(&ts->cmd_done);

	ts->client = client;
	i2c_set_clientdata(client, ts);

#ifdef ELAN_RAW_DATA
	private_ts = ts;
#endif

	ts->vcc33 = devm_regulator_get(&client->dev, "vcc33");
	if (IS_ERR(ts->vcc33)) {
		error = PTR_ERR(ts->vcc33);
		if (error != -EPROBE_DEFER)
			dev_err(&client->dev,
                    "Failed to get 'vcc33' regulator: %d\n",
                    error);
		return error;
	}

	ts->vccio = devm_regulator_get(&client->dev, "vccio");
	if (IS_ERR(ts->vccio)) {
		error = PTR_ERR(ts->vccio);
		if (error != -EPROBE_DEFER)
			dev_err(&client->dev,
                    "Failed to get 'vccio' regulator: %d\n",
                    error);
		return error;
	}
#ifdef USE_IN_SUNXI //[
	ts->reset_gpio = gpio_to_desc(config_info.wakeup_gpio.gpio);
	set_bit(FLAG_ACTIVE_LOW, &(ts->reset_gpio->flags));
    
	if (IS_ERR(ts->reset_gpio)) {
		dev_err(&client->dev,
				"failed to get reset gpio\n");
		error = PTR_ERR(ts->reset_gpio);

		if (error == -EPROBE_DEFER)
			return error;

		if (error != -ENOENT && error != -ENOSYS) {
			dev_err(&client->dev,
                    "failed to get reset gpio: %d\n",
                    error);
			return error;
		}

		ts->keep_power_in_suspend = true;
	}
#else
	//ts->reset_gpio = devm_gpiod_get(&client->dev, "gpio_elan_rst", GPIOD_OUT_LOW);
    ts->reset_gpio = of_get_named_gpio_flags(client->dev.of_node, "gpio_elan_rst", 0, NULL);
#endif
	error = devm_gpio_request_one(&client->dev, ts->reset_gpio, GPIOF_OUT_INIT_HIGH, "gpio_elan_rst");
	if (error < 0) {
		dev_err(&client->dev,
			"request gpio failed: %d\n", error);
		return error;
	}

	ts->keep_power_in_suspend = true;  //expect power is on when suspend

#ifdef USE_IN_SUNXI //[
	ts->intr_gpio = config_info.int_number;
	//ts->client->irq = gpio_to_irq(ts->intr_gpio);
#else
	//get int status from dts
	ts->intr_gpio = of_get_named_gpio_flags(client->dev.of_node, "gpio_intr", 0, NULL);
#endif
	error = devm_gpio_request_one(&client->dev, ts->intr_gpio, GPIOF_IN, "gpio_elan_intr");
	if (error < 0) {
		dev_err(&client->dev,
			"request gpio failed: %d\n", error);
		return error;
	}
	
	ts->client->irq = gpio_to_irq(ts->intr_gpio);
    
#ifndef USE_IN_SUNXI
    of_property_read_u32(client->dev.of_node, "screen_max_x", &screen_max_x);
    of_property_read_u32(client->dev.of_node, "screen_max_y", &screen_max_y);
    of_property_read_u32(client->dev.of_node, "revert_x_flag", &revert_x_flag);
    of_property_read_u32(client->dev.of_node, "revert_y_flag", &revert_x_flag);
    of_property_read_u32(client->dev.of_node, "exchange_x_y_flag", &exchange_x_y_flag);
    
    printk("get max_x=%d, max_y=%d, revert_x=%d, revert_y=%d, exchange_x_y=%d\n", screen_max_x, screen_max_y, revert_x_flag, revert_y_flag, exchange_x_y_flag);
#endif

	/*error = gpio_request(ts->intr_gpio, "tp_irq");
	gpio_direction_input(ts->intr_gpio);*/

	error = elants_i2c_power_on(ts);
	if (error)
		return error;

	error = devm_add_action(&client->dev, elants_i2c_power_off, ts);
	if (error) {
		dev_err(&client->dev,
                "failed to install power off action: %d\n", error);
		elants_i2c_power_off(ts);
		return error;
	}

	/* Make sure there is something at this address */
	if (i2c_smbus_xfer(client->adapter, client->addr, 0,
                       I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &dummy) < 0) {
		dev_err(&client->dev, "nothing at this address\n");
		return -ENXIO;
	}

	error = elants_i2c_initialize(ts);
	if (error) {
		dev_err(&client->dev, "failed to initialize: %d\n", error);
		return error;
	}

	ts->input = devm_input_allocate_device(&client->dev);
	//ts->input = input_allocate_device();
	if (ts->input == NULL) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	ts->input->name = "Elan Touchscreen";
	ts->input->id.bustype = BUS_I2C;

	__set_bit(BTN_TOUCH, ts->input->keybit);
	__set_bit(EV_ABS, ts->input->evbit);
	__set_bit(EV_KEY, ts->input->evbit);
	__set_bit(KEY_BATTERY, ts->input->keybit);
    __set_bit(BTN_TOOL_PEN, ts->input->keybit);
	
	/*for pen key*/
	__set_bit(BTN_STYLUS, ts->input->keybit);
	__set_bit(BTN_STYLUS2, ts->input->keybit);
	
	/* Single touch input params setup */
	if(exchange_x_y_flag){
		input_set_abs_params(ts->input, ABS_X, 0, screen_max_y, 0, 0);
		input_set_abs_params(ts->input, ABS_Y, 0, screen_max_x, 0, 0);
		input_abs_set_res(ts->input, ABS_X, ts->y_res);
		input_abs_set_res(ts->input, ABS_Y, ts->x_res);
	} else {
		input_set_abs_params(ts->input, ABS_X, 0, screen_max_x, 0, 0);
		input_set_abs_params(ts->input, ABS_Y, 0, screen_max_y, 0, 0);
		input_abs_set_res(ts->input, ABS_X, ts->x_res);
		input_abs_set_res(ts->input, ABS_Y, ts->y_res);
	}
	input_set_abs_params(ts->input, ABS_PRESSURE, 0, 4095, 0, 0);
	
	/* Multitouch input params setup */
	error = input_mt_init_slots(ts->input, MAX_CONTACT_NUM,
                                INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (error) {
		dev_err(&client->dev,
                "failed to initialize MT slots: %d\n", error);
		return error;
	}

	input_set_abs_params(ts->input, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_PRESSURE, 0, 4095, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_DISTANCE, 0, 15, 0, 0);
	if(exchange_x_y_flag){
		input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, screen_max_y, 0, 0);
		input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, screen_max_x, 0, 0);
		input_abs_set_res(ts->input, ABS_MT_POSITION_X, ts->y_res);
		input_abs_set_res(ts->input, ABS_MT_POSITION_Y, ts->x_res);
	} else {
		input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, screen_max_x, 0, 0);
		input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, screen_max_y, 0, 0);
		input_abs_set_res(ts->input, ABS_MT_POSITION_X, ts->x_res);
		input_abs_set_res(ts->input, ABS_MT_POSITION_Y, ts->y_res);
	}
  
	input_set_drvdata(ts->input, ts);

	error = input_register_device(ts->input);
	if (error) {
		dev_err(&client->dev,
                "unable to register input device: %d\n", error);
		return error;
	}

#ifdef USE_IN_SUNXI
	ts->elan_wq = create_highpri_singlethread_workqueue("elan_wq");
#else
	ts->elan_wq = create_singlethread_workqueue("elan_wq");
#endif

	if (IS_ERR(ts->elan_wq)) {
		error = PTR_ERR(ts->elan_wq);
		dev_err(&client->dev,
				"[elan error] failed to create kernel thread: %d\n",
				error);
		return error;
		//goto err_create_workqueue_failed;
	}
	INIT_WORK(&ts->ts_work, elan_ts_work_func);

	/*
	 * Platform code (ACPI, DTS) should normally set up interrupt
	 * for us, but in case it did not let's fall back to using falling
	 * edge to be compatible with older Chromebooks.
	 */
	elants_register_interrupt(client);

	/*
	 * Systems using device tree should set up wakeup via DTS,
	 * the rest will configure device as wakeup source by default.
	 */
	if (!client->dev.of_node)
		device_init_wakeup(&client->dev, true);

	error = sysfs_create_group(&client->dev.kobj, &elants_attribute_group);
	if (error) {
		dev_err(&client->dev, "failed to create sysfs attributes: %d\n",
                error);
		return error;
	}

#ifdef ELAN_RAW_DATA
	//for Firmware Update and read RAW Data
	ts->firmware.minor = MISC_DYNAMIC_MINOR;
    ts->firmware.name = "elan-iap";
    ts->firmware.fops = &elan_touch_fops;
    ts->firmware.mode = S_IFREG|S_IRWXUGO;

	if (misc_register(&ts->firmware) < 0)
        printk("[elan] misc_register failed!!\n");
    else
        printk("[elan] misc_register finished!!\n");

#endif

	error = devm_add_action(&client->dev,
                            elants_i2c_remove_sysfs_group, ts);
	if (error) {
		elants_i2c_remove_sysfs_group(ts);
		dev_err(&client->dev,
                "Failed to add sysfs cleanup action: %d\n",
                error);
		return error;
	}


	return 0;
}

static int elants_i2c_remove(struct i2c_client *client)
{
	//struct elants_data *ts = i2c_get_clientdata(client);
    struct elants_data *ts = private_ts;

	/*
	 * Let elants_i2c_power_off know that it needs to keep
	 * regulators on.
	 */
	ts->unbinding = true;

	return 0;
}

static int __maybe_unused elants_i2c_suspend(struct device *dev)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	struct i2c_client *client = ts->client;

#ifdef ELAN_HID_I2C
	const u8 set_sleep_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
                                   0x04, 0x54, 0x50, 0x00, 0x01 };
#else
	const u8 set_sleep_cmd[] = { 0x54, 0x50, 0x00, 0x01 };
#endif
	int retry_cnt;
	int error;

    /* Command not support in IAP recovery mode */
	if (ts->iap_mode != ELAN_IAP_OPERATIONAL)
		return -EBUSY;
    if(gSleep_Mode_Suspend){
		if(ts->reset_gpio) {
			gpio_set_value(ts->reset_gpio, 0);
		}
		//disable_irq(client->irq);
		free_irq(ts->client->irq, private_ts);
	} else if ( !elan_i2c_detect_int_level(ts)) {
        elan_i2c_ts_triggered(ts);
    } else {
		ts->wake_irq_enabled = true;
        enable_irq_wake(client->irq);
	} 
#ifdef USE_IN_SUNXI //[
    else if (device_may_wakeup(dev)) {

        if (suspending_trigger) {
            suspending_trigger = 0;
            ts->state = ELAN_STATE_NORMAL;
            elan_i2c_ts_triggered(ts);
            return -EBUSY;
        }
        
        disable_irq(client->irq);
 
		enable_gpio_wakeup_src(ts->intr_gpio);
		ts->wake_irq_enabled = true;

		ts->wake_irq_enabled = true;
        enable_irq_wake(client->irq);
    }  else if (ts->keep_power_in_suspend) {
        /*ts->wake_irq_enabled = true;
        enable_irq_wake (client->irq);*/
        for (retry_cnt = 0; retry_cnt < MAX_RETRIES; retry_cnt++) {
            error = elants_i2c_send(client, set_sleep_cmd,
                                    sizeof(set_sleep_cmd));
            if (!error)
                break;

            dev_err(&client->dev,
                    "suspend command failed: %d\n", error);
        }
		ts->wake_irq_enabled = true;
		enable_irq_wake(ts->client->irq);
    } else {
        
    }
#endif //][! USE_IN_SUNXI

	return 0;
}

static int __maybe_unused elants_i2c_resume(struct device *dev)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct elants_data *ts = i2c_get_clientdata(client);
	struct elants_data *ts = private_ts;
	struct i2c_client *client = ts->client;

#ifdef ELAN_HID_I2C
	const u8 set_active_cmd[37] = { 0x04, 0x00, 0x23, 0x00, 0x03, 0x00,
                                    0x04, 0x54, 0x58, 0x00, 0x01 };
#else
	const u8 set_active_cmd[] = { 0x54, 0x58, 0x00, 0x01 };
#endif
	int retry_cnt;
	int error;
	unsigned long irqflags;
	
	if (!elan_i2c_detect_int_level) {
		elan_i2c_ts_triggered(ts);
	}

    if (gSleep_Mode_Suspend) {
		if(ts->reset_gpio) {
			gpio_set_value(ts->reset_gpio, 1);
		}
		elants_i2c_power_on(ts);
		elants_i2c_initialize(ts);
		
		elants_register_interrupt(ts->client);
	} else {
		ts->wake_irq_enabled = false;
		disable_irq_wake(ts->client->irq);
	} 
#ifdef USE_IN_SUNXI //[
    else if (device_may_wakeup(dev)) {
		if (ts->wake_irq_enabled) {

            elan_i2c_ts_triggered(ts);
			disable_gpio_wakeup_src(ts->intr_gpio);
			ts->wake_irq_enabled = false;

			disable_irq_wake(client->irq);
        }
        
        ts->state = ELAN_STATE_NORMAL;
        enable_irq(client->irq);
    } else if (ts->keep_power_in_suspend) {
        disable_irq_wake(client->irq);
    } else {
        
    }
#endif //][! USE_IN_SUNXI
    
    suspending_trigger = 0;
	
    return 0;
}

static const struct dev_pm_ops elants_i2c_pm_ops = {
	.suspend = elants_i2c_suspend,
	.resume = elants_i2c_resume,
};

/*static SIMPLE_DEV_PM_OPS(elants_i2c_pm_ops,
                         elants_i2c_suspend, elants_i2c_resume);*/

/*
 * elan_detect - Device detection callback for automatic device creation
 * return value:
 *                    = 0; success;
 *                    < 0; err
 */
static int elan_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int  i = 0;

	pr_debug("%s\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_info("%s check i2c failed !\n",__func__);
		return -ENODEV;
	}
	//printk("%s 1\n", __func__);
#ifdef USE_IN_SUNXI
	if (twi_id == adapter->nr) {
		/*
		i2c_read_bytes(client, read_chip_value, 3);
		pr_debug("addr:0x%x,chip_id_value:0x%x\n",
					client->addr, read_chip_value[2]);
		*/
		//printk("%s 2\n", __func__);
		strlcpy(info->type, DEVICE_NAME, I2C_NAME_SIZE);
		pr_info("%s: \"%s\" detected .\n", __func__, info->type);
		return 0;
	} else {
		pr_debug("%s(),invalid twi id (%d) @ port%d !\n",__func__,twi_id,adapter->nr);
		return -ENODEV;
	}
#endif
    return 0;
}

static const struct i2c_device_id elants_i2c_id[] = {
	{ DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, elants_i2c_id);

#ifdef CONFIG_ACPI
static const struct acpi_device_id elants_acpi_id[] = {
	{ "ELAN0001", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, elants_acpi_id);
#endif

#ifdef CONFIG_OF
static const struct of_device_id elants_of_match[] = {
	{ .compatible = "elan,ekth3500" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, elants_of_match);
#endif

static struct i2c_driver elants_i2c_driver = {
#ifdef USE_IN_SUNXI //[
	.class = I2C_CLASS_HWMON,
	.address_list = normal_i2c,
	.detect         = elan_detect,
#endif //] USE_IN_SUNXI
	.probe = elants_i2c_probe,
	.remove = elants_i2c_remove,
	.id_table = elants_i2c_id,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.pm = &elants_i2c_pm_ops,
		//.acpi_match_table = ACPI_PTR(elants_acpi_id),
#ifndef USE_IN_SUNXI //[
		.of_match_table = elants_of_match,
#endif //] !USE_IN_SUNXI
		//.probe_type = PROBE_PREFER_ASYNCHRONOUS, // Linux-4.2 or higher
	},
};

static int __init elan_touch_init(void)
{
	int ret = -1;

#ifdef USE_IN_SUNXI //[
	pr_debug("%s(%d)\n",__func__,__LINE__);
	if (input_sensor_startup(&(config_info.input_type))) {
		pr_debug("%s: ctp_startup err.\n", __func__);
		return 0;
	} else {
		ret = input_sensor_init(&(config_info.input_type));
		if (ret != 0)
			pr_warning("%s:ctp_ops.init err.\n", __func__);
	}
	if (config_info.ctp_used == 0) {
		pr_debug("%s() ctp_used = 0 !\n",__func__);
		//pr_debug("if use ctp,please put the sys_config.fex ctp_used set to 1.\n");
		return 0;
	}
	if (!ctp_get_system_config()) {
		pr_warning("%s:read config fail!\n", __func__);
		return 0;
	}

//	input_set_power_enable(&(config_info.input_type), 1);

	if (strncmp(config_info.name, DEVICE_NAME, strlen(DEVICE_NAME))) {
		pr_debug("%s() ctp name not \"%s\"\n",__func__ ,DEVICE_NAME);
		ret = 0;
		goto init_err;
	}
#endif
	
#if 1
	pr_info("%s(%d)\n",__func__,__LINE__);
	ret = i2c_add_driver(&elants_i2c_driver);
	if(ret)
		goto init_err;
#endif
	return ret;

init_err:
//	input_set_power_enable(&(config_info.input_type), 0);
	return ret;
}

static void __exit elan_touch_exit(void)
{
	i2c_del_driver(&elants_i2c_driver);
}

module_init(elan_touch_init);
module_exit(elan_touch_exit);

MODULE_AUTHOR("Scott Liu <scott.liu@emc.com.tw>");
MODULE_DESCRIPTION("Elan I2c Touchscreen driver");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");

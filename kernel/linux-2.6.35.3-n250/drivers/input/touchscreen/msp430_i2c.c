/*
 *  MSP430 touchscreen driver
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

#define GDEBUG 0
#include <linux/gallen_dbg.h>

#include "ntx_hwconfig.h"


extern volatile NTX_HWCONFIG *gptHWCFG;

static const char MSP430_TS_NAME[]	= "MSP430-touch";

static struct workqueue_struct *MSP430_wq;
static uint16_t g_touch_pressed, g_touch_triggered;
static unsigned char g_FW_upgrade_process;
static unsigned char g_is_report_abs;
static unsigned char g_is_touch_enabled;

static char active_command[] = {0x70,0x00,0x01,0x01,0x00};
static char sleep_command[] = {0x70,0x00,0x01,0x00,0x00};

static struct MSP430_data {
	struct delayed_work	work;
	struct i2c_client *client;
	struct input_dev *input;
	wait_queue_head_t wait;
} MSP430_touch_data;

/*--------------------------------------------------------------*/
static int MSP430_touch_recv_data(struct i2c_client *client, uint8_t cmd,uint8_t *buf)
{
	int i2c_ret;
	int retry_count = 5;
	int value = -1;
	u8 buf0[2];
	u16 addr;
	u16 flags;
	struct i2c_msg msg[2] = {
		{0, 0, 1, buf0},
		{0, I2C_M_RD, 4, buf},
	};

	if (!MSP430_touch_data.client)
		retry_count = 1;
	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[1].addr = client->addr;
	msg[1].flags |= client->flags;
	
	while (retry_count--) {
		buf0[0] = cmd & 0xff;
		i2c_ret = i2c_transfer(client->adapter, msg, 2);
		if (i2c_ret >= 0) {
			break;
		}
		pr_err("%s: read reg error : Cmd 0x%02x (%d)\n", __func__, cmd, i2c_ret);
//		ntx_msp430_i2c_force_release ();
		schedule_timeout (50);
	}
	return i2c_ret;
}

static int MSP430_touch_write_data(struct i2c_client *client, uint8_t *buf, int length)
{
	int i2c_ret;
	int retry_count = 5;
	int value = -1;
	struct i2c_msg msg[2] = {
		{0, 0, length, buf},
	};

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	
	while (retry_count--) {
		i2c_ret = i2c_transfer(client->adapter, msg, 1);
		if (i2c_ret >= 0) {
			break;
		}
		pr_err("%s: write reg error : %d bytes\n", __func__, length);
		schedule_timeout (50);
	}
	return i2c_ret;
}

static unsigned char touch_flag = 0;
static void MSP430_touch_report_data(void)
{
	static unsigned char last_pos[2][2];
	unsigned char buf[10];
	
	MSP430_touch_recv_data (MSP430_touch_data.client, 0x40, buf);
	
	if (g_is_report_abs) {
		if(g_touch_triggered && !(buf[0] || buf[1] || buf[2] || buf[3])) {
			DBG_MSG ("[%s-%d] touch triggered but no data.\n",__func__,__LINE__);
		}
			
		if (buf[0] || buf[1]) {
			touch_flag |= 1;
			buf[0] = 8-buf[0];
			buf[1] = 8-buf[1];
			if ((buf[0] != last_pos[0][0]) || (buf[1] != last_pos[0][1])) {
				input_report_abs(MSP430_touch_data.input, ABS_X, buf[0]);
				input_report_abs(MSP430_touch_data.input, ABS_Y, buf[1]);
				input_report_abs(MSP430_touch_data.input, ABS_PRESSURE, 1);
				input_report_abs(MSP430_touch_data.input, ABS_MT_POSITION_X, buf[0]);
				input_report_abs(MSP430_touch_data.input, ABS_MT_POSITION_Y, buf[1]);
				input_report_abs(MSP430_touch_data.input, ABS_MT_TRACKING_ID, 1);
				last_pos[0][0] = buf[0];
				last_pos[0][1] = buf[1];
				input_report_key(MSP430_touch_data.input, KEY_LEFT, 1);
				input_sync(MSP430_touch_data.input);
				DBG_MSG("[%s-%d] Area A %d, %d down.\n",__func__,__LINE__,last_pos[0][0],last_pos[0][1]);
			}
		}
		else if (last_pos[0][0] || last_pos[0][1]) {
			DBG_MSG ("[%s-%d] Area A %d, %d up.\n",__func__,__LINE__,last_pos[0][0],last_pos[0][1]);
			input_report_abs(MSP430_touch_data.input, ABS_X, last_pos[0][0]);
			input_report_abs(MSP430_touch_data.input, ABS_Y, last_pos[0][1]);
			input_report_abs(MSP430_touch_data.input, ABS_PRESSURE, 0);
			input_report_abs(MSP430_touch_data.input, ABS_MT_POSITION_X, last_pos[0][0]);
			input_report_abs(MSP430_touch_data.input, ABS_MT_POSITION_Y, last_pos[0][1]);
			input_report_abs(MSP430_touch_data.input, ABS_MT_TRACKING_ID, 1);
			last_pos[0][0] = 0;
			last_pos[0][1] = 0;
			input_report_key(MSP430_touch_data.input, KEY_LEFT, 0);
			input_sync(MSP430_touch_data.input);
			touch_flag &= 0x02;
		} 
		if (buf[2] || buf[3]) {
			touch_flag |= 2;
			buf[2] = 8-buf[2];
			buf[3] = 8-buf[3];
			if ((buf[2] != last_pos[1][0]) || (buf[3] != last_pos[1][1])) {
				input_report_abs(MSP430_touch_data.input, ABS_X, buf[2]);
				input_report_abs(MSP430_touch_data.input, ABS_Y, buf[3]);
				input_report_abs(MSP430_touch_data.input, ABS_PRESSURE, 1);
				input_report_abs(MSP430_touch_data.input, ABS_MT_POSITION_X, buf[2]);
				input_report_abs(MSP430_touch_data.input, ABS_MT_POSITION_Y, buf[3]);
				input_report_abs(MSP430_touch_data.input, ABS_MT_TRACKING_ID, 0);
				last_pos[1][0] = buf[2];
				last_pos[1][1] = buf[3];
				input_report_key(MSP430_touch_data.input, KEY_RIGHT, 1);
				input_sync(MSP430_touch_data.input);
				DBG_MSG("[%s-%d] Area B %d, %d down.\n",__func__,__LINE__,last_pos[1][0],last_pos[1][1]);
			}
		}
		else if (last_pos[1][0] || last_pos[1][1]) {
			DBG_MSG ("[%s-%d] Area B %d, %d up.\n",__func__,__LINE__,last_pos[1][0],last_pos[1][1]);
			input_report_abs(MSP430_touch_data.input, ABS_X, last_pos[1][0]);
			input_report_abs(MSP430_touch_data.input, ABS_Y, last_pos[1][1]);
			input_report_abs(MSP430_touch_data.input, ABS_PRESSURE, 0);	
			input_report_abs(MSP430_touch_data.input, ABS_MT_POSITION_X, last_pos[1][0]);
			input_report_abs(MSP430_touch_data.input, ABS_MT_POSITION_Y, last_pos[1][1]);
			input_report_abs(MSP430_touch_data.input, ABS_MT_TRACKING_ID, 0);
			last_pos[1][0] = 0;
			last_pos[1][1] = 0;
			input_report_key(MSP430_touch_data.input, KEY_RIGHT, 0);
			input_sync(MSP430_touch_data.input);
			touch_flag &= 0x01;
		}
	} 
	else 
	{
		MSP430_touch_recv_data (MSP430_touch_data.client, 0x80, buf);
		
		if(g_touch_triggered && !(buf[1] || buf[3])) {
			DBG_MSG ("[%s-%d] touch triggered but no data.\n",__func__,__LINE__);
		}
	//	printk ("[%s-%d] key buffer %02x,%02x,%02x,%02x\n",__func__,__LINE__,buf[0],buf[1],buf[2],buf[3]);
			
		if ((buf[1] & 0x01) || (buf[3] & 0x01))
			input_report_key(MSP430_touch_data.input, KEY_UP, 1);
		if ((buf[1] & 0x02) || (buf[3] & 0x02))
			input_report_key(MSP430_touch_data.input, KEY_DOWN, 1);
		if ((buf[1] & 0x04) || (buf[3] & 0x04))
			input_report_key(MSP430_touch_data.input, KEY_LEFT, 1);
		if ((buf[1] & 0x08) || (buf[3] & 0x08))
			input_report_key(MSP430_touch_data.input, KEY_RIGHT, 1);
		if ((buf[1] & 0x10) || (buf[3] & 0x10))
			input_report_key(MSP430_touch_data.input, KEY_ENTER, 1);
		input_sync(MSP430_touch_data.input);
		
		if ((buf[1] & 0x01) || (buf[3] & 0x01))
			input_report_key(MSP430_touch_data.input, KEY_UP, 0);
		if ((buf[1] & 0x02) || (buf[3] & 0x02))
			input_report_key(MSP430_touch_data.input, KEY_DOWN, 0);
		if ((buf[1] & 0x04) || (buf[3] & 0x04))
			input_report_key(MSP430_touch_data.input, KEY_LEFT, 0);
		if ((buf[1] & 0x08) || (buf[3] & 0x08))
			input_report_key(MSP430_touch_data.input, KEY_RIGHT, 0);
		if ((buf[1] & 0x10) || (buf[3] & 0x10))
			input_report_key(MSP430_touch_data.input, KEY_ENTER, 0);
		input_sync(MSP430_touch_data.input);
	}

	schedule();
	if (touch_flag)
		schedule_delayed_work(&MSP430_touch_data.work, 2);
}

static void MSP430_touch_work_func(struct work_struct *work)
{
	if (g_FW_upgrade_process || !g_is_touch_enabled)
		return;
		
	MSP430_touch_report_data();
	g_touch_triggered = 0;
}

static irqreturn_t MSP430_touch_ts_interrupt(int irq, void *dev_id)
{
	g_touch_triggered = 1;
	if (g_FW_upgrade_process){
		printk ("[%s-%d] firmware upgrading %d\n",__func__,__LINE__,g_FW_upgrade_process);
	}
	else if (!touch_flag) {
		if (g_is_report_abs)
			schedule_delayed_work(&MSP430_touch_data.work, 1);
		else
			schedule_delayed_work(&MSP430_touch_data.work, 5);
	}
	return IRQ_HANDLED;
}

static ssize_t fwupg_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return 0;
}

static ssize_t click_area_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	strcpy (buf,"Set click working area with top left and buttom right coordinate.\n"\
				"Ex. write \"1 2 3 4 A\" for zone A (1,2)&(3,4) \n" \
				"Ex. write \"1 2 3 4 B\" for zone B (1,2)&(3,4) \n" );
	return strlen(buf);
}

static ssize_t click_area_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int i;
	unsigned char command[5];
	
	printk ("[%s-%d] count %d, buf : %s\n",__func__,__LINE__,count,buf);
	for (i=0; i<4; i++) {
		if (('0' > buf[i<<1]) || ('8' < buf[i<<1]) || (' ' != buf[(i<<1)+1])) {
			printk ("Set click working area with top left and buttom right coordinate.\n"\
					"Ex. write \"1 2 3 4 A\" for zone A (1,2)&(3,4) \n" \
					"Ex. write \"1 2 3 4 B\" for zone B (1,2)&(3,4) \n" );
			return -1;
		}
	}
	if ('A' == buf[8])
		command[0] = 0x71;
	else
		command[0] = 0x72;
	command[1] = ((buf[0]<<4) | (buf[2]&0x0F));
	command[2] = ((buf[4]<<4) | (buf[6]&0x0F));
	if (command[1] || command[2])
		command[3] = 1;
	else
		command[3] = 0;
	MSP430_touch_write_data (MSP430_touch_data.client, command, 4);
	printk ("[%s-%d] %X %X %X %X\n",__func__, __LINE__,command[0],command[1],command[2],command[3]);
	
	return count;
}	       

static ssize_t scan_area_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	strcpy (buf,"Set scan area.\n0: deactive\n1: all\nA: Zone A\nB: Zone B\n ");
	return strlen(buf);
}

static ssize_t scan_area_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	printk ("[%s-%d] count %d, buf : %s\n",__func__,__LINE__,count,buf);
	if (('0' != buf[0]) && ('1' != buf[0]) && ('A' != buf[0]) && ('B' != buf[0])) {
		printk ("Set scan area.\n0: deactive\n1: all\nA: Zone A\nB: Zone B\n ");
		return -1;
	}
	if (('0' == buf[0]) || ('1' == buf[0]))
		active_command[3] = buf[0] & 0x0F;
	else
		active_command[3] = buf[0]-'A'+0x0A;
	MSP430_touch_write_data (MSP430_touch_data.client, active_command, 4);
	
	return count;
}	       

static ssize_t enable_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	strcpy (buf,"Set back cover touch power. (0:off 1:on) \n ");
	return strlen(buf);
}

extern void MSP430_touch_power (int isOn);
static ssize_t enable_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	if (('0' != buf[0]) && ('1' != buf[0])) {
		printk ("Set back cover touch power. (0:off 1:on)\n ");
		return -1;
	}
	if ('0' == buf[0]) {
		MSP430_touch_power (0);
		g_is_touch_enabled = 0;
	}
	else {
		MSP430_touch_power (1);
		msleep (2000);
		MSP430_touch_write_data (MSP430_touch_data.client, active_command, 4);
		g_is_touch_enabled = 1;
	}
	
	return count;
}	       

static ssize_t event_type_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	strcpy (buf,"Set back cover touch report event type. (ABS: report coordinate, KEY: report key.) \n ");
	return strlen(buf);
}

static ssize_t version_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	char version[10];
	MSP430_touch_recv_data (MSP430_touch_data.client, 0x00, version);
	sprintf(buf, "%02X.%02X", version[0], version[1]);
	return strlen(buf);
}

static ssize_t event_type_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	if (!strncmp (buf, "KEY",3)) {
		printk ("Report key events.\n ");
		g_is_report_abs = 0;
		active_command[1] = 0;
		MSP430_touch_write_data (MSP430_touch_data.client, active_command, 4);
		return count;
	}
	else if (!strncmp (buf, "ABS",3)) {
		printk ("Report coordinate (ABS events).\n ");
		g_is_report_abs = 1;
		active_command[1] = 1;
		MSP430_touch_write_data (MSP430_touch_data.client, active_command, 4);
		return count;
	}
	else
		printk ("Undefined parameter. (ABS: report coordinate, KEY: report key.) \n ");
	return count;
}	       

static int fwupg_wait_ack (int wait)
{
	int loop_count=wait/100;
	
	g_touch_triggered=0;
	while (loop_count--) {
		msleep (100);
		if (g_touch_triggered) {
			msleep (100);
			g_touch_triggered = 0;
			return 0;
		}
	}
	printk ("[%s-%d] timeout waiting for MSP430 int.\n",__func__,__LINE__);
	return -1;
}

static int packet_filter (unsigned char *pBuffer, int length)
{
	unsigned char *pIndex=pBuffer;
	int i, newLength=0;
	
	for (i=0; i<length; i++) {
		if(' '==*pIndex || '\n'==*pIndex || '\r'==*pIndex) {
			++pIndex;
			continue;
		}
		*pBuffer++ = *pIndex++;
		++newLength;
	}
	*pBuffer='\0';
	return newLength;
}

#define FW_UPG_PACKET_SIZE	230
static ssize_t fwupg_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	unsigned char firmware_upgrade_command[]={0x5A, 0xA5, 0xAA, 0x55};
	unsigned char flash_erase_command[]={'G', 'K' , 'S'};
	static unsigned char w_buffer[256];
	static int last_packet_length;
	int w_length, p_length, index, write_pointer = 0;
	unsigned char *pTmp;
	int result = count;

	printk ("[%s-%d] count %d\n",__func__,__LINE__,count);
	if (!g_FW_upgrade_process) {
		if (g_is_touch_enabled) {
			MSP430_touch_power (1);
			g_is_touch_enabled = 1;
		}
		g_FW_upgrade_process = 1;
		printk ("[%s-%d] start program command (0x5A, 0xA5, 0xAA, 0x55)...\n",__func__,__LINE__);
		if (0 > MSP430_touch_write_data (MSP430_touch_data.client, firmware_upgrade_command, 4))
			result = -1;
		msleep (1500);
		g_FW_upgrade_process++;
		printk ("[%s-%d] programing ('G', 'K' , 'S')...\n",__func__,__LINE__);
		if (0 > MSP430_touch_write_data (MSP430_touch_data.client, flash_erase_command, 3)) 
			result = -1;
		if (0 > fwupg_wait_ack (5000))
			result = -2;
		g_FW_upgrade_process++;
	}
	
	while ((0 < result) && (write_pointer < count)) {
		if (last_packet_length) {
			memcpy (w_buffer+last_packet_length, buf+write_pointer, FW_UPG_PACKET_SIZE-last_packet_length);
			w_length = FW_UPG_PACKET_SIZE;
			if (count < (FW_UPG_PACKET_SIZE-last_packet_length))
				w_length = last_packet_length + count;
		}
		else {
			w_length = FW_UPG_PACKET_SIZE;
			if (w_length > (count-write_pointer))
				w_length = count-write_pointer;
			memcpy (w_buffer, buf+write_pointer, w_length);
		}
		index = w_length-1;
		while (('\n' != *(w_buffer+index)) && index)
			index--;
			
		*(w_buffer+w_length) = '\0';
		if (pTmp = strchr (w_buffer, 'q')) {
			*(w_buffer+w_length) = '\0';
			printk ("%s\n",w_buffer);
			p_length = packet_filter (w_buffer, w_length);
			printk ("\n===>%s\n",w_buffer);
			if (0 > MSP430_touch_write_data (MSP430_touch_data.client, w_buffer, p_length))
				result = -1;
			if (0 > fwupg_wait_ack (5000))
				result = -2;
			w_length = 0;
			g_FW_upgrade_process = 0;
			break;
		}	
		else {
			if (index && (count >= (write_pointer+index))) {
				if (pTmp = strchr(w_buffer+1, '@')) {
					index = pTmp-w_buffer-1;
				}
				if (index) {
					*(w_buffer+index+1) = 'S';
					w_length = index+2;
				}
				*(w_buffer+w_length) = '\0';
				printk ("%s\n",w_buffer);
				p_length = packet_filter (w_buffer, w_length);
				printk ("\nwrite %d bytes===>%s \n",p_length,w_buffer);
				if (0 >= MSP430_touch_write_data (MSP430_touch_data.client, w_buffer, p_length))
					result = -1;
				if (0 > fwupg_wait_ack (5000))
					result = -2;
				if (last_packet_length)
					write_pointer += (w_length-1-last_packet_length);
				else
					write_pointer += (w_length-1);
				last_packet_length = 0;
				w_length = 0;
			}
			else {
				*(w_buffer+w_length) = '\0';
				write_pointer += w_length;
				last_packet_length = w_length;
			}	
		}
	}
	
	
	if (strchr (w_buffer, 'q')) {
		printk ("\nLast packet. Reinitial touch. \n");
		msleep (2000);
		MSP430_touch_write_data (MSP430_touch_data.client, active_command, 4);
	}

	if (0 > result)
		g_FW_upgrade_process = 0;
		
	return result;
}

static DEVICE_ATTR(fwupg, 0666, fwupg_read, fwupg_write);
static DEVICE_ATTR(click_area, 0666, click_area_read, click_area_write);
static DEVICE_ATTR(scan_area, 0666, scan_area_read, scan_area_write);
static DEVICE_ATTR(enable, 0666, enable_read, enable_write);
static DEVICE_ATTR(event_type, 0666, event_type_read, event_type_write);
static DEVICE_ATTR(version, 0666, version_read, NULL);

static const struct attribute *sysfs_touch_attrs[] = {
	&dev_attr_fwupg.attr,
	&dev_attr_click_area.attr,
	&dev_attr_scan_area.attr,
	&dev_attr_enable.attr,
	&dev_attr_event_type.attr,
	&dev_attr_version.attr,
	NULL,
};

extern int gSleep_Mode_Suspend;

static int MSP430_touch_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (gSleep_Mode_Suspend && g_is_touch_enabled) {
		MSP430_touch_write_data (MSP430_touch_data.client, sleep_command, 5);
		msleep(200);
		disable_irq_wake(MSP430_touch_data.client->irq);
	}
	return 0;
}

static int MSP430_touch_resume(struct platform_device *pdev)
{
	if (gSleep_Mode_Suspend && g_is_touch_enabled) {
		MSP430_touch_write_data (MSP430_touch_data.client, active_command, 5);
		msleep(200);
		enable_irq_wake(MSP430_touch_data.client->irq);
	}
	return 0;
}

static int msp430_i2c_open(struct input_dev *dev)
{

	return 0;
}

static void msp430_i2c_close(struct input_dev *dev)
{

}

static int MSP430_touch_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	unsigned char buf[5];

	if (MSP430_touch_data.client) {
		printk("[%s-%d] i2c%d back touch already registed!\n", __func__, __LINE__, i2c_adapter_id(to_i2c_adapter(client->dev.parent)));
		return -1;
	}
	if (0 >= MSP430_touch_recv_data (client, 0x00, buf)) {
		printk("[%s-%d] i2c%d back touch not found!\n", __func__, __LINE__, i2c_adapter_id(to_i2c_adapter(client->dev.parent)));
		return -1;
	}
	printk("[%s-%d] cmd 0 0x%02X %02X %02X %02X\n", __func__, __LINE__, buf[0], buf[1], buf[2], buf[3]);
	printk("[%s-%d] registered in i2c%d\n", __func__, __LINE__, i2c_adapter_id(to_i2c_adapter(client->dev.parent)));
	
	MSP430_touch_data.client = client;
	strlcpy(client->name, MSP430_TS_NAME, I2C_NAME_SIZE);

	INIT_DELAYED_WORK(&MSP430_touch_data.work, MSP430_touch_work_func);
	
	MSP430_touch_data.input = input_allocate_device();
	if (MSP430_touch_data.input == NULL) {
		err = -ENOMEM;
		goto fail;
	}


	MSP430_touch_write_data (MSP430_touch_data.client, active_command, 5);

	MSP430_touch_data.input->name = MSP430_TS_NAME;
	MSP430_touch_data.input->id.bustype = BUS_I2C;
	MSP430_touch_data.input->open = msp430_i2c_open;
	MSP430_touch_data.input->close = msp430_i2c_close;

	input_set_drvdata(MSP430_touch_data.input, client);
	
	set_bit(EV_SYN, MSP430_touch_data.input->evbit);
	
	set_bit(EV_KEY, MSP430_touch_data.input->evbit);
	set_bit(KEY_LEFT, MSP430_touch_data.input->keybit);
	set_bit(KEY_RIGHT, MSP430_touch_data.input->keybit);
	set_bit(KEY_UP, MSP430_touch_data.input->keybit);
	set_bit(KEY_DOWN, MSP430_touch_data.input->keybit);
	set_bit(KEY_ENTER, MSP430_touch_data.input->keybit);
	
	set_bit(EV_ABS, MSP430_touch_data.input->evbit);
	set_bit(ABS_X, MSP430_touch_data.input->absbit);
	set_bit(ABS_Y, MSP430_touch_data.input->absbit);
	set_bit(ABS_PRESSURE, MSP430_touch_data.input->absbit);
	set_bit(ABS_MT_POSITION_X, MSP430_touch_data.input->absbit);
	set_bit(ABS_MT_POSITION_Y, MSP430_touch_data.input->absbit);
	set_bit(ABS_MT_TRACKING_ID, MSP430_touch_data.input->absbit);

    input_set_abs_params(MSP430_touch_data.input, ABS_X, 0, 64, 0, 0);
	input_set_abs_params(MSP430_touch_data.input, ABS_Y, 0, 64, 0, 0);
	input_set_abs_params(MSP430_touch_data.input, ABS_PRESSURE, 0, 2048, 0, 0);
    input_set_abs_params(MSP430_touch_data.input, ABS_MT_POSITION_X, 0, 64, 0, 0);
	input_set_abs_params(MSP430_touch_data.input, ABS_MT_POSITION_Y, 0, 64, 0, 0);
	input_set_abs_params(MSP430_touch_data.input, ABS_MT_TRACKING_ID, 0, 64, 0, 0);
	err = input_register_device(MSP430_touch_data.input);
	if (err < 0) {
		pr_debug("Register device file!\n");
		goto fail;
	}

	err = sysfs_create_files(&MSP430_touch_data.input->dev.kobj, sysfs_touch_attrs);
	if (err) {
		pr_debug("Can't create device file!\n");
		return -ENODEV;
	}

	if (0 < MSP430_touch_data.client->irq) {
		printk("[%s-%d] request_irq %d\n", __func__, __LINE__,MSP430_touch_data.client->irq);
		err = request_irq(MSP430_touch_data.client->irq, MSP430_touch_ts_interrupt, 0, MSP430_TS_NAME, MSP430_TS_NAME);
		if (err < 0) {
			printk(KERN_ERR "%s(%s): Can't allocate irq %d\n", __FILE__, __func__, MSP430_touch_data.client->irq);
		    goto fail;
		}
		enable_irq_wake(MSP430_touch_data.client->irq);
	}
	if (i2c_adapter_id(to_i2c_adapter(client->dev.parent)))
		MSP430_touch_power (0);
	else
		g_is_touch_enabled = 1;
	
	return 0;

fail:
	input_free_device(MSP430_touch_data.input);
	if (MSP430_wq)
		destroy_workqueue(MSP430_wq);
	return err;
}

static int MSP430_touch_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_fwupg);
	device_remove_file(&client->dev, &dev_attr_click_area);
	device_remove_file(&client->dev, &dev_attr_scan_area);
	device_remove_file(&client->dev, &dev_attr_enable);
	device_remove_file(&client->dev, &dev_attr_event_type);

	if (MSP430_wq)
		destroy_workqueue(MSP430_wq);

	input_unregister_device(MSP430_touch_data.input);

	free_irq(client->irq, MSP430_TS_NAME);
	return 0;
}

/* -------------------------------------------------------------------- */
static const struct i2c_device_id MSP430_touch_id[] = {
    {"msp430-touch", 0 },
	{ }
};

static struct i2c_driver MSP430_touch_driver = {
	.probe		= MSP430_touch_probe,
	.remove		= MSP430_touch_remove,
	.suspend	= MSP430_touch_suspend,
	.resume		= MSP430_touch_resume,
	.id_table	= MSP430_touch_id,
	.driver		= {
		.name = "msp430-touch",
		.owner = THIS_MODULE,
	},
};

static int __init MSP430_touch_init(void)
{
	return i2c_add_driver(&MSP430_touch_driver);
}

static void __exit MSP430_touch_exit(void)
{
	i2c_del_driver(&MSP430_touch_driver);
}

module_init(MSP430_touch_init);
module_exit(MSP430_touch_exit);

MODULE_AUTHOR("Joseph Lai. ");
MODULE_DESCRIPTION("NTX MSP430 Touch driver");
MODULE_LICENSE("GPL");

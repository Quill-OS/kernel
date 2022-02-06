/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * Includes
 */
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/iomux-mx6q.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>

#include "../../../arch/arm/mach-mx6/ntx_hwconfig.h"

#include "../../../arch/arm/mach-mx6/board-mx6sl_ntx.h"

#define GDEBUG 0
#include <linux/gallen_dbg.h>

#define ACTIVE 1
#define IDLE 0
static int current_mode = ACTIVE;	// default active

extern volatile NTX_HWCONFIG *gptHWCFG;
extern int gSleep_Mode_Suspend;

struct i2c_client *g_kl25_i2c_client;
static struct delayed_work kl25_delay_work;
static struct input_dev *idev;

static const char KL25_NAME[]	= "kl25";
static int kl25_triggered = 0;

static const uint8_t ident_command[] = {0x3E, 0x00, 0x03};
static const uint8_t func_get_command[] = {0x01, 0x01, 0x02};
static const uint8_t wakeup_command[] = {0x00, 0x00, 0x00};

unsigned long g_kl25_result = 0;
unsigned long g_kl25_action = 0;

static int kl25_int_level(void)
{
	unsigned v;
	gpio_direction_input (irq_to_gpio(g_kl25_i2c_client->irq));
	v = gpio_get_value(irq_to_gpio(g_kl25_i2c_client->irq));
//	printk("kl25_int2(%d) = %d\n",irq_to_gpio(g_kl25_i2c_client->irq),v);
	return v;
}

unsigned short kl25_ident(void)
{
	uint8_t buf_recv[5];

	i2c_master_send(g_kl25_i2c_client, ident_command, sizeof(ident_command));
	msleep(20);
	i2c_master_recv(g_kl25_i2c_client, buf_recv, 4);
	printk("[%s-%d] KL25 ident: %02x %02x %02x %02x\n",__func__,__LINE__,buf_recv[0],buf_recv[1],buf_recv[2],buf_recv[3]);
	if(3==buf_recv[0] && 0x56==buf_recv[1]) {
		printk("[%s-%d] KL25 VerBootloader:%02x , VerAP:%02x\n",__func__,__LINE__, buf_recv[2], buf_recv[3]);
	} 
	else {
		printk("[%s-%d] Can't get correct KL25 ident",__func__,__LINE__);
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t kl25_interrupt(int irq, void *dev_id)
{
	kl25_triggered = 1;
//	printk("[%s-%d] kl25 interrupt(%d) received\n",__func__,__LINE__,irq);
	schedule_delayed_work(&kl25_delay_work, 0);
	return IRQ_HANDLED;
}

static void kl25_work_func(struct work_struct *work)
{
	uint8_t buf_recv[5];	
	
	i2c_master_send(g_kl25_i2c_client, func_get_command, sizeof(func_get_command));
	msleep(20);
	i2c_master_recv(g_kl25_i2c_client, buf_recv, 3);
	if(buf_recv[0]==0 && buf_recv[1]==0 && buf_recv[2]==0){
		printk("not waked up by KL25\n");
	}
	else {
		printk("kl25 function and orient: %x %x %x\n",buf_recv[0],buf_recv[1],buf_recv[2]);
	
		g_kl25_result = 0;
		g_kl25_result |= (buf_recv[1] << 8);
		g_kl25_result |= (buf_recv[2]     );
		g_kl25_action = (buf_recv[1]     );

		switch(buf_recv[1])
		{
			case 0x80:
				printk("[%s-%d] Tap triggered\n",__func__,__LINE__);
				input_report_key(idev, KEY_TAP, 1);
				input_report_key(idev, KEY_TAP, 0);
				break;
			case 0x40:
				printk("[%s-%d] Double Tap triggered\n",__func__,__LINE__);
				input_report_key(idev, KEY_DOUBLETAP, 1);
				input_report_key(idev, KEY_DOUBLETAP, 0);
				break;
			case 0x08:
				printk("[%s-%d] Left shake triggered\n",__func__,__LINE__);
				input_report_key(idev, KEY_SHAKELEFT, 1);
				input_report_key(idev, KEY_SHAKELEFT, 0);
				break;
			case 0x04:
				printk("[%s-%d] Right shake triggered\n",__func__,__LINE__);
				input_report_key(idev, KEY_SHAKERIGHT, 1);
				input_report_key(idev, KEY_SHAKERIGHT, 0);
				break;
			case 0x02:
				printk("[%s-%d] Forward shake triggered\n",__func__,__LINE__);
				input_report_key(idev, KEY_SHAKEFORWARD, 1);
				input_report_key(idev, KEY_SHAKEFORWARD, 0);
				break;
			case 0x01:
				printk("[%s-%d] Backward shake triggered\n",__func__,__LINE__);
				input_report_key(idev, KEY_SHAKEBACKWARD, 1);
				input_report_key(idev, KEY_SHAKEBACKWARD, 0);
				break;
			default:
			// undefined fuction
				printk("undefined function\n");
				break;
		}
		input_sync(idev);
	}
	kl25_triggered = 0;
}

static int kl25_ctrl(int mode)
{
	static const uint8_t readBookModeIn_command[] = {0x17, 0x00, 0x00};       // KL25 active
	static const uint8_t readBookModeOut_command[] = {0x18, 0x00, 0x00};	  // KL25 inactive
	if( ACTIVE == mode ) {
		i2c_master_send(g_kl25_i2c_client, wakeup_command, sizeof(wakeup_command));
		msleep(30);
		i2c_master_send(g_kl25_i2c_client, readBookModeIn_command, sizeof(readBookModeIn_command));
		msleep(30);
		i2c_master_send(g_kl25_i2c_client, readBookModeIn_command, sizeof(readBookModeIn_command));
		printk("[%s-%d] set KL25 active mode\n",__func__,__LINE__);
		current_mode = ACTIVE;
	}
	else if( IDLE == mode ) {
		i2c_master_send(g_kl25_i2c_client, wakeup_command, sizeof(wakeup_command));
		msleep(30);
		i2c_master_send(g_kl25_i2c_client, readBookModeOut_command, sizeof(readBookModeOut_command));
		msleep(30);
		i2c_master_send(g_kl25_i2c_client, readBookModeOut_command, sizeof(readBookModeOut_command));
		printk("[%s-%d] set KL25 idle mode\n",__func__,__LINE__);
		current_mode = IDLE;
	}
	return current_mode; //return current_mode only if mode not equal to ACTIVE or IDLE
}

#define FW_UPG_DELAY	70
static int g_FW_upgrade_process;
static unsigned short g_FW_checksum;

static unsigned char ascii_to_hex (char *pAscii) 
{
	unsigned char result_H, result_L;
	result_H = *pAscii - '0';
	if ('9' < *pAscii)
		result_H -= 7;
		
	result_L = *(pAscii+1) - '0';
	if ('9' < *(pAscii+1))
		result_L -= 7;
		
	return ((result_H << 4) | (result_L&0x0F));
}

static void calc_checksum (unsigned char *pPacket, int length)
{
	int i;
	
	if (pPacket) {
		for (i=0; i<length; i+=2) {
#if 0
			// big endian
			g_FW_checksum += (unsigned short)(*(pPacket+i));
#else
			// little endian
			unsigned short temp = (*(pPacket+i+1)<<8)  | *(pPacket+i);
			g_FW_checksum += temp;
#endif		
		}
	}
	else {
		printk ("[%s-%d] none used area (size %d), filled with 0xFFFF \n",__func__,__LINE__, length);
		for (i=0; i<length; i+=2) {
			g_FW_checksum += 0xFFFF;
		}
	}
}

static int write_packet (unsigned char *pPacket, int length) 
{
	int ret=0;
	pPacket[0] = 0x13;		// flash write command
	pPacket[1]= length;
	pPacket[2] = pPacket[3] = 0;	// set addr1, addr2 to 0
	length += 6;
	
#if 0
	{
		int i;
		printk ("[%s-%d] send %d bytes ====================================\n",__func__,__LINE__,length);
		for (i=0; i<length ; i++) {
			printk ("%02X ", *(pPacket+i));
			if (i && !(i&0x0F))
				printk ("\n");
		}
		printk ("\n");
	}
#endif
	i2c_master_send(g_kl25_i2c_client, pPacket, length);
	msleep(FW_UPG_DELAY);
	i2c_master_recv(g_kl25_i2c_client, pPacket, 2);
	if (0x7E != pPacket[1])	{
		printk ("[%s-%d] failed writing 0x%02X%02X (%d)!! return 0x%X, 0x%X\n",__func__,__LINE__, 
			pPacket[4], pPacket[5], length,pPacket[0], pPacket[1]);
		ret = -EINVAL; //write fail return invalid
	} else {
		printk ("[%s-%d] writing 0x%02X%02X (%d) done. \n",__func__,__LINE__, pPacket[4], pPacket[5], length);
	}
	if ( pPacket[4] < 0x7C ) {
		calc_checksum (pPacket+6, length-6); //skip first 6 byte
	}
	msleep(20);

	return (ret<0)?ret:length;
}

static int packet_process (unsigned char *pBuffer, int length)
{
	unsigned char *pIndex=pBuffer, s_length;
	static unsigned char send_buf[80];
	static int s_addr, s_last_addr, s_offset, packet_Length, last_buf_length;
	static int s_last_valid_addr = 0;
	int i;
	int ret=0;
	
	if (5 > length)
		return 0;
	
	while (((pIndex+4) - pBuffer) < length) {
		if ('S' != *pIndex && !last_buf_length ) {
			if ('\r'!= *pIndex && '\n' != *pIndex)
				printk ("[%s-%d] ignore %c\n", __func__, __LINE__, *pIndex);
			++pIndex;
			continue;
		}
		switch (*(++pIndex)) {
			case '0':
				s_length = ascii_to_hex (pIndex+1);
				*(pIndex+(int)(s_length<<1)+3) = 0;
				// printk ("[%s-%d] %s\n", __func__, __LINE__, *(pIndex-1));
				pIndex += ((s_length<<1)+4);
				packet_Length = 0;
				g_FW_upgrade_process = 1;
				break;
			case '1':
				s_length = ascii_to_hex (++pIndex);
				if (length < ((pIndex + (int)(s_length<<1)) - pBuffer)) {
					if (0x40 == packet_Length) {
						ret = write_packet (send_buf, packet_Length);
						packet_Length = 0;
					}
					return  (pIndex-pBuffer-2);
				}
				s_length -= 3;
				pIndex += 2;
				s_addr = (ascii_to_hex (pIndex) << 8) | ascii_to_hex (pIndex+2);
				pIndex += 4;
				if (packet_Length && ((0x40 < (packet_Length+s_length)) || (s_last_addr != s_addr))) {
					if (s_last_addr != s_addr) {
						printk ("[%s-%d] addr %04X, last addr %04X \n", __func__, __LINE__, s_addr, s_last_addr);
						if (0x7BFD > s_addr) {
							calc_checksum (0, (s_addr-s_last_addr));
						} else if (!s_last_valid_addr){
							s_last_valid_addr = s_last_addr;
						}
					}
					ret = write_packet (send_buf, packet_Length);
					packet_Length = 0;
				}
				if (0 == packet_Length) {
					send_buf[4] = s_addr >> 8;
					send_buf[5] = s_addr & 0xFF;
					s_offset = 6;
					s_last_addr = s_addr;
				}
				for (i=0; i < s_length; i++) {
					send_buf[s_offset++] = ascii_to_hex (pIndex);
					pIndex += 2;
				}
				packet_Length += s_length;
				s_last_addr += s_length;
				pIndex += 2;	// skip check sum
				break;
			case '9':
				printk ("[%s-%d] S9 terminated!!\n", __func__, __LINE__);
				if (packet_Length)
					ret = write_packet (send_buf, packet_Length);
				if( s_last_valid_addr ) {
					calc_checksum (0, (0x7BFD-s_last_valid_addr));
					s_last_valid_addr = 0;
				} else {
					calc_checksum (0, (0x7BFD-s_last_addr));
				}
				g_FW_upgrade_process = 9;
				return length;
			default:
				printk ("[%s-%d] undefined S%c packet!!\n", __func__, __LINE__, *pIndex);
				g_FW_upgrade_process = 0;
				return length;
		}
	}
	return (ret<0) ? ret:(pIndex - pBuffer);
}

#define FW_UPG_PACKET_SIZE	230
static ssize_t fwupg_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	unsigned char flash_erase_command[]={0x12,0x04,0x00,0x00,0x00,0x3C,0x00};
	unsigned char flash_start_command[]={0x10};
	unsigned char reflash_start_command[]={0x15,0x00,0x00};
	unsigned char flash_end_command[]={0x13,0x01,0x00,0x00,0x7B,0xFF,0xFF};
	unsigned char checksum_command[]={0x19,0x00,0x00,0x55,0x56,0x57,0x58};
	unsigned char reset_command[]={0x11,0x00,0x00};
	static unsigned char w_buffer[256];
	static int last_packet_length;
	int index;
	int ret = 0;
	unsigned char ack[20];

	if (!g_FW_upgrade_process) {
		g_FW_upgrade_process = 1;
		g_FW_checksum = 0;
		i2c_master_send(g_kl25_i2c_client, ident_command, sizeof(ident_command));
		msleep(20);
		i2c_master_recv(g_kl25_i2c_client, ack, 4);
		printk("[%s-%d] KL25 ident: %02x %02x %02x %02x\n",__func__,__LINE__,ack[0],ack[1],ack[2],ack[3]);
		msleep(20);
		i2c_master_send(g_kl25_i2c_client, reflash_start_command, sizeof(reflash_start_command));
		msleep(50);
		i2c_master_recv(g_kl25_i2c_client, ack, 2);
		printk ("[%s-%d] reflash start return 0x%X, 0x%X\n",__func__,__LINE__, ack[0], ack[1]);
		msleep(20);
		i2c_master_send(g_kl25_i2c_client, flash_start_command, sizeof(flash_start_command));
		msleep(50);
		i2c_master_recv(g_kl25_i2c_client, ack, 2);
		printk ("[%s-%d] flash start return 0x%X, 0x%X\n",__func__,__LINE__, ack[0], ack[1]);

		while (0x7C >= flash_erase_command[5]) {
			i2c_master_send(g_kl25_i2c_client, flash_erase_command, sizeof(flash_erase_command));
			msleep(FW_UPG_DELAY);
			i2c_master_recv(g_kl25_i2c_client, ack, 2);
			printk ("[%s-%d] flash erase %02X00 return 0x%X, 0x%X\n",__func__,__LINE__, flash_erase_command[5], ack[0], ack[1]);
			if (ack[1] != 0x7E)
			{
				printk ("[%s-%d] Erase failed. firmware upgrade failed!\n",__func__,__LINE__);
				g_FW_upgrade_process = 0;
				last_packet_length = 0;
				return -EINVAL;
			}

			flash_erase_command[5] += 4;
			msleep(20);
		}
	}
	
	printk ("[%s-%d] count %d\n",__func__,__LINE__,count);
	if (last_packet_length) {
//		printk ("[%s-%d] last_packet_length %d\n",__func__,__LINE__,last_packet_length);
		for (index=0; index < count ; index++) {
			w_buffer[last_packet_length] = buf[index];
			last_packet_length ++;
			if ('\r' == buf[index] || '\n' == buf[index]) {
				w_buffer[last_packet_length] = 0;
//				printk ("[%s-%d] packet : %s\n",__func__,__LINE__,w_buffer);
				ret = packet_process (w_buffer, last_packet_length);
				break;
			}
		}
		if (index == count)
			return count;
		else {
//			printk ("[%s-%d] index %d\n",__func__,__LINE__,index);
			ret = packet_process (buf+index, (count-index));
			index += ret;
		}
	}
	else {
		ret = packet_process (buf, count);
		index = ret;
	}
	last_packet_length = count - index;
	printk ("[%s-%d] %d left.\n",__func__,__LINE__,last_packet_length);
	if (last_packet_length) 
		memcpy (w_buffer, buf+index, last_packet_length);   
	
	if (9 == g_FW_upgrade_process) {
		g_FW_checksum = 0xFFFF-g_FW_checksum+1; //2's complement
		printk ("[%s-%d] checksum 0x%04X\n",__func__,__LINE__, g_FW_checksum);
		checksum_command[1] = g_FW_checksum>>8;
		checksum_command[2] = g_FW_checksum&0xFF;
		i2c_master_send(g_kl25_i2c_client, checksum_command, sizeof(checksum_command));
		msleep(FW_UPG_DELAY);
		i2c_master_recv(g_kl25_i2c_client, ack, 2);
		printk ("[%s-%d] checksum return 0x%X, 0x%X\n",__func__,__LINE__, ack[0], ack[1]);

		i2c_master_send(g_kl25_i2c_client, flash_end_command, sizeof(flash_end_command));
		msleep(500);
		i2c_master_recv(g_kl25_i2c_client, ack, 2);
		printk ("[%s-%d] flash end return 0x%X, 0x%X\n",__func__,__LINE__, ack[0], ack[1]);
		if (ack[1] != 0x7E)
		{
			ret = -EINVAL; // checksum doesn't match, return invalid
			printk ("[%s-%d] checksum not match. firmware upgrade failed!\n",__func__,__LINE__);
		}

		i2c_master_send(g_kl25_i2c_client, reset_command, sizeof(reset_command));
		printk ("[%s-%d] KL25 reset\n",__func__,__LINE__);
		g_FW_upgrade_process = 0;
		last_packet_length = 0;
	}
	return (ret < 0)?ret:count;
}

static ssize_t fwupg_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return 0;
}

static ssize_t VerBL_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	uint8_t buf_recv[4];

	msleep(25);
	i2c_master_send(g_kl25_i2c_client, wakeup_command, sizeof(wakeup_command));
	msleep(20);
	i2c_master_send(g_kl25_i2c_client, ident_command, sizeof(ident_command));
	msleep(20);
	i2c_master_recv(g_kl25_i2c_client, buf_recv, 4);
	if(3==buf_recv[0] && 0x56==buf_recv[1]) {
		return sprintf(buf, "%d",buf_recv[2]);
	} 
	else {
		printk("[%s-%d] Can't get correct KL25 ident",__func__,__LINE__);
		return -EINVAL;
	}
}

static ssize_t VerAP_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	uint8_t buf_recv[4];

	msleep(25);
	i2c_master_send(g_kl25_i2c_client, wakeup_command, sizeof(wakeup_command));
	msleep(20);
	i2c_master_send(g_kl25_i2c_client, ident_command, sizeof(ident_command));
	msleep(20);
	i2c_master_recv(g_kl25_i2c_client, buf_recv, 4);
	if(3==buf_recv[0] && 0x56==buf_recv[1]) {
		return sprintf(buf, "%d",buf_recv[3]);
	} 
	else {
		printk("[%s-%d] Can't get correct KL25 ident",__func__,__LINE__);
		return -EINVAL;
	}
}

static ssize_t enable_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n",current_mode);
}

static ssize_t enable_set(struct device *dev, struct device_attribute *attr,
			char *buf,  size_t count)
{
	int value;
	value = simple_strtoul(buf, NULL, 10);
	if ( 0 != value  && 1 != value ) {
		printk("[%s-%d] Set 1 to activate, or 0 to inactivate",__func__,__LINE__);
		return -EINVAL;
	}
	kl25_ctrl(value);
	return count;
}

#if 0
static ssize_t cs_read(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	unsigned char flash_start_command[]={0x10};
	unsigned char read_cs_command[]={0x14,0x03,0x00,0x00,0x7C,0x04};
	unsigned char reflash_start_command[]={0x15,0x00,0x00};

	uint8_t buf_recv[4];
	unsigned char ack[20];

	i2c_master_send(g_kl25_i2c_client, wakeup_command, sizeof(wakeup_command));
	msleep(20);
	i2c_master_send(g_kl25_i2c_client, reflash_start_command, sizeof(reflash_start_command));
	msleep(50);
	i2c_master_recv(g_kl25_i2c_client, ack, 2);
	printk ("[%s-%d] reflash start return 0x%X, 0x%X\n",__func__,__LINE__, ack[0], ack[1]);
	i2c_master_send(g_kl25_i2c_client, flash_start_command, sizeof(flash_start_command));
	msleep(50);
	i2c_master_recv(g_kl25_i2c_client, ack, 2);
	printk ("[%s-%d] flash start return 0x%X, 0x%X\n",__func__,__LINE__, ack[0], ack[1]);
			
	i2c_master_send(g_kl25_i2c_client, read_cs_command, sizeof(read_cs_command));
	msleep(FW_UPG_DELAY);
	i2c_master_recv(g_kl25_i2c_client, buf_recv, 3);
	printk ("[%s-%d] CheckSum 0x%X, 0x%X, 0x%X\n",__func__,__LINE__, buf_recv[0], buf_recv[1], buf_recv[2]);

	return 0;
}
#endif
static DEVICE_ATTR(fwupg, 0666, fwupg_read, fwupg_write);
static DEVICE_ATTR(VerBL, S_IRUGO, VerBL_read, NULL);
static DEVICE_ATTR(VerAP, S_IRUGO, VerAP_read, NULL);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, enable_read, enable_set);
//static DEVICE_ATTR(cs, S_IRUGO, cs_read, NULL);

static const struct attribute *sysfs_kl25_attrs[] = {
	&dev_attr_fwupg.attr,
	&dev_attr_VerBL.attr,
	&dev_attr_VerAP.attr,
	&dev_attr_enable.attr,
//	&dev_attr_cs.attr,
	NULL,
};

static __devinit int kl25_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int err = 0;

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
  		printk("%s, functionality check failed\n", __func__);
    		return -ENXIO;
  	}
	
	g_kl25_i2c_client = client;
	
	err = kl25_ident();
	if (err < 0) {
		printk("Ident failed!\n");
		return err;
	}

	idev = input_allocate_device();

	idev->name = "KL25";
	idev->id.bustype = BUS_I2C;
	idev->evbit[0] = BIT_MASK(EV_KEY) ;
	
	input_set_capability(idev, EV_KEY, KEY_DOUBLETAP);
	input_set_capability(idev, EV_KEY, KEY_TAP);
	input_set_capability(idev, EV_KEY, KEY_SHAKELEFT);
	input_set_capability(idev, EV_KEY, KEY_SHAKERIGHT);
	input_set_capability(idev, EV_KEY, KEY_SHAKEFORWARD);
	input_set_capability(idev, EV_KEY, KEY_SHAKEBACKWARD);

	err = input_register_device(idev);	
	if (err < 0) {
		printk("Register device file!\n");
		return err;
	}

	INIT_DELAYED_WORK(&kl25_delay_work, kl25_work_func);

	err = sysfs_create_files(&idev->dev.kobj, sysfs_kl25_attrs);
	if (err) {
		pr_debug("Can't create device file!\n");
		return -ENODEV;
	}

	err = request_irq(g_kl25_i2c_client->irq, kl25_interrupt, IRQF_TRIGGER_FALLING, KL25_NAME, KL25_NAME);
	if (err < 0) {
		printk(KERN_ERR "%s(%s): Can't allocate irq %d\n", __FILE__, __func__, g_kl25_i2c_client->irq);
		cancel_delayed_work_sync (&kl25_delay_work);
		return err;
	}
	return 0;
}

static __devexit int kl25_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static int kl25_suspend(struct platform_device *pdev, pm_message_t state)
{
//	printk ("[%s-%d] %s() %d\n",__FILE__,__LINE__,__func__,gSleep_Mode_Suspend);
	
	/* return immediatly if the driver is still handling touch data */
	if (kl25_triggered) {
		printk("[%s-%d] KL25 still handling data\n",__func__,__LINE__);
		return -EBUSY;
	}

	/* KL25 wants to send data, trigger a read */
	if ( kl25_int_level() ) 
	{
		printk ("[%s-%d] KL25 event not processed.\n",__func__,__LINE__);
		schedule_delayed_work(&kl25_delay_work, 0);
		return -EBUSY;
	}
	
	if (gSleep_Mode_Suspend) {
		free_irq(g_kl25_i2c_client->irq, KL25_NAME);
	}
	else {
//		printk("kl25_suspend,enable irq wakeup source %d\n",g_kl25_i2c_client->irq);
		enable_irq_wake(g_kl25_i2c_client->irq);
	}
	return 0;
}

static int kl25_resume(struct platform_device *pdev)
{
	if (gSleep_Mode_Suspend) {
		request_irq(g_kl25_i2c_client->irq, kl25_interrupt, IRQF_TRIGGER_FALLING, KL25_NAME, KL25_NAME);
		kl25_ctrl(current_mode);
	}
	else {
		schedule_delayed_work(&kl25_delay_work, 0);
//		printk("kl25_resume,disable irq wakeup source %d\n",g_kl25_i2c_client->irq);
		disable_irq_wake(g_kl25_i2c_client->irq);
	}
	return 0;
}

static const struct i2c_device_id kl25_id[] = {
	{"kl25", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, kl25_id);

static struct i2c_driver kl25_i2c_driver = {
	.driver = {
		   .name = "kl25",
		   .owner = THIS_MODULE,
		   },
	.probe = kl25_i2c_probe,
	.remove = __devexit_p(kl25_i2c_remove),
	.suspend = kl25_suspend,
	.resume = kl25_resume,
	.id_table = kl25_id,
};

static int __init kl25_init(void)
{
	return i2c_add_driver(&kl25_i2c_driver);
}

static void __exit kl25_exit(void)
{
	i2c_del_driver(&kl25_i2c_driver);
}

module_init(kl25_init);
module_exit(kl25_exit);

MODULE_DESCRIPTION("KL25 driver");
MODULE_AUTHOR("Netronix Inc.");
MODULE_LICENSE("GPL");

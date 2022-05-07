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
#include <linux/irq.h>

#define ELAN_TS_NAME   "elan-touch"
#define ELAN_I2C_ID 	 	0x1588
#define ELAN_I2C_ADDR 	0x10

#define ELAN_TS_X_MAX 		768
#define ELAN_TS_Y_MAX 		1088
#define IDX_PACKET_SIZE		8

#define SCALE_X           600
#define SCALE_Y           800

#define MAX_CONTACTS      2

/* Externs */
extern int  gpio_neonode_irq(void);
extern void gpio_neonode_request_irq(int enable);
extern bool gpio_neonode_datain(void);
extern void gpio_neonode_bsl_init(void);

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { ELAN_I2C_ADDR, I2C_CLIENT_END };

/*
static struct i2c_client_address_data addr_data = {
        .normal_i2c = normal_addr,
        .probe = ignore,
        .ignore = ignore,
};*/

enum {	
	hello_packet  = 0x55,
	idx_coordinate_packet 	= 0x5a,
	};
enum {	idx_finger_state = 7,};
static struct workqueue_struct *elan_wq = NULL;
static struct elan_data {
	int intr_gpio;
	int use_irq;	
	struct hrtimer timer;	
	struct work_struct work;	
	struct i2c_client *client;	
	struct input_dev *input;	
	wait_queue_head_t wait;
}  elan_touch_data = {0};

static int elan_touch_probe(struct i2c_client *client,
                            const struct i2c_device_id *id);
//static int elan_touch_detach(struct i2c_client *client);
static int elan_touch_remove(struct i2c_client *client);

static int elan_touch_detect_int_level(void)
{
	unsigned v;
	v = gpio_neonode_datain();
	return v;
}
static int __elan_touch_poll(struct i2c_client *client)
{	
	int status = 0, retry = 20;	
	do {		
		status = elan_touch_detect_int_level();
		retry--;	
		mdelay(20);
		} while (status == 1 && retry > 0);
  printk(KERN_INFO "GPIO value in poll : %d\n", elan_touch_detect_int_level());
	return (status == 0 ? 0 : -ETIMEDOUT);
}

static int elan_touch_poll(struct i2c_client *client)
{	
	return __elan_touch_poll(client);
}

static int __hello_packet_handler(struct i2c_client *client)
{	
	int rc;	
	struct i2c_msg xfer[1];
  uint8_t buf_recv[4] = { 0 };

  /*rc = i2c_smbus_write_byte(client, hello_packet);
  if (rc < 0)
    return rc;*/
  
/*  xfer[0].addr = client->addr;
  xfer[0].flags = I2C_M_RD;
  xfer[0].len = 4;
  xfer[0].buf = buf_recv;*/
  
	rc = elan_touch_poll(client);
	if (rc < 0) {
    printk(KERN_ERR "No response from device. GPIO\n");
		return -EINVAL;	
		}	
	rc = i2c_master_recv(client, buf_recv, 4);
  //rc = i2c_transfer(client->adapter, xfer, 1);
  if (rc != 1 || rc < 0) {
		return rc;	
		} 
	else {		
		int i;		
		printk("hello packet: [0x%02x 0x%02x 0x%02x 0x%02x]\n",	buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);	
		for (i = 0; i < 4; i++)	
			if (buf_recv[i] != hello_packet)	
				return -EINVAL;	
			}	
	return 0;
}

static int __elan_touch_init(struct i2c_client *client)
{	
	int rc;	
	rc = __hello_packet_handler(client);	
	return 0;
  if (rc < 0)
		goto hand_shake_failed;
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

static inline int elan_touch_parse_xy(uint8_t *data, uint16_t *x, uint16_t *y)
{	
	*x = (data[0] & 0xf0);
	*x <<= 4;
	*x |= data[1];
	*y = (data[0] & 0x0f);
	*y <<= 8;
	*y |= data[2];
	return 0;
}

static inline void adjustOrigin(uint16_t *x, uint16_t *y)
{
  uint16_t yTmp;

  yTmp = *x;
  
  *x = *y;
  *y = ELAN_TS_Y_MAX - yTmp; 
}

static inline void scaleXY(uint16_t *x, uint16_t *y)
{
  *x = (uint16_t)(((uint32_t)*x * SCALE_X) / ELAN_TS_X_MAX);
  *y = (uint16_t)(((uint32_t)*y * SCALE_Y) / ELAN_TS_Y_MAX);
}

static void elan_touch_report_data(struct i2c_client *client, uint8_t *buf)
{
  printk(KERN_INFO "Reporting touch data : 0x%x\n", buf[0]);
  switch (buf[0]) {
		case idx_coordinate_packet: 
			{		
				uint16_t x1, x2, y1, y2;		
				uint8_t finger_stat;	
				finger_stat = (buf[idx_finger_state] & 0x06) >> 1;
        x1=y1=x2=y2=0;
				if (finger_stat == 0)
        {	
					if ((buf[idx_finger_state] == 1) && !(buf[1]|buf[2]|buf[3]|buf[4]|buf[5]|buf[6])) {
            input_report_key(elan_touch_data.input, BTN_TOUCH, 0);
            input_report_key(elan_touch_data.input, BTN_TOOL_FINGER, 0);
            input_report_key(elan_touch_data.input, BTN_TOOL_DOUBLETAP, 0);
          }	
        } 
				else if (finger_stat == 1)
        {
					elan_touch_parse_xy(&buf[1], &x1, &y1);
          adjustOrigin(&x1, &y1);
          scaleXY(&x1, &y1);
					input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, x1);
					input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, y1);
					input_mt_sync(elan_touch_data.input);
					input_report_key(elan_touch_data.input, BTN_TOUCH, 1);
					input_report_key(elan_touch_data.input, BTN_TOOL_FINGER, 1);
					input_report_key(elan_touch_data.input, BTN_TOOL_DOUBLETAP, 0);		
        } 
				else if (finger_stat == 2)
        {
					elan_touch_parse_xy(&buf[1], &x1, &y1);	
          adjustOrigin(&x1, &y1);
          scaleXY(&x1, &y1);
					input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, x1);
					input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, y1);
					input_mt_sync(elan_touch_data.input);
					elan_touch_parse_xy(&buf[4], &x2, &y2);	
          adjustOrigin(&x2, &y2);
          scaleXY(&x2, &y2);
					input_report_abs(elan_touch_data.input, ABS_MT_POSITION_X, x2);	
					input_report_abs(elan_touch_data.input, ABS_MT_POSITION_Y, y2);
					input_mt_sync(elan_touch_data.input);
					input_report_key(elan_touch_data.input, BTN_TOUCH, 1);
					input_report_key(elan_touch_data.input, BTN_TOOL_FINGER, 1);
					input_report_key(elan_touch_data.input, BTN_TOOL_DOUBLETAP, 1);		
        }		
				input_sync(elan_touch_data.input);
				break;	
			}	
		default:	
			break;	
		}
}

static void elan_touch_work_func(struct work_struct *work)
{	
	int rc;	
	uint8_t buf[IDX_PACKET_SIZE] = { 0 };	
	struct i2c_client *client = elan_touch_data.client;	

	/*if (elan_touch_detect_int_level())
  {
    printk(KERN_INFO "Level is high. We shouldn't be here\n");
		return;	
  }*/
	rc = elan_touch_recv_data(client, buf);
	if (rc < 0)
  {
    printk(KERN_ERR "Error receiving data in interrupt\n");
		return;	
  }
	elan_touch_report_data(client, buf);
}
DECLARE_WORK(elan_touch_work, elan_touch_work_func);

static enum hrtimer_restart elan_touch_timer_func(struct hrtimer *timer)
{	
	queue_work(elan_wq, &elan_touch_data.work);
	hrtimer_start(&elan_touch_data.timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t elan_touch_ts_interrupt(int irq, void *dev_id)
{
  //printk(KERN_INFO "In interrupt handler\n");
	//queue_work(elan_wq, &elan_touch_data.work);
  schedule_work(&elan_touch_work);
	return IRQ_HANDLED;
}

static int elan_touch_register_interrupt(struct i2c_client *client)
{	
  int res;
  
  gpio_neonode_request_irq(1);      // Enable IRQ
  client->irq = gpio_neonode_irq(); // Request IRQ
  set_irq_type(client->irq, IRQF_TRIGGER_FALLING);

  res = request_irq(client->irq, elan_touch_ts_interrupt, 0, "elan-touch", NULL);
  if (res != 0)
  {
    printk(KERN_ERR "IRQF_DISABLED: Could not get IRQ %d\n", client->irq);
    return -1;
  }
  
	if (!elan_touch_data.use_irq) {	
		hrtimer_init(&elan_touch_data.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);	
		elan_touch_data.timer.function = elan_touch_timer_func;
		hrtimer_start(&elan_touch_data.timer, ktime_set(1, 0), HRTIMER_MODE_REL);	
		}	
	printk("elan ts starts in %s mode.\n",	elan_touch_data.use_irq == 1 ? "interrupt":"polling");	
	return 0;
}

static int __devinit elan_touch_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;	
 
  elan_touch_data.input = NULL;
  elan_touch_data.client = client;
  i2c_set_clientdata(client, &elan_touch_data);
  
	/*INIT_WORK(&elan_touch_data.work, elan_touch_work_func);	
	elan_wq = create_singlethread_workqueue("elan_wq");	
	if (!elan_wq) {
		rc = -ENOMEM;
		goto fail;	
	}*/
	
	elan_touch_data.input = input_allocate_device();
	if (elan_touch_data.input == NULL) {
		rc = -ENOMEM;
		goto fail;
		}
	
  elan_touch_data.input->name = ELAN_TS_NAME;
	elan_touch_data.input->id.bustype = BUS_I2C;
	set_bit(EV_SYN, elan_touch_data.input->evbit);
	set_bit(EV_KEY, elan_touch_data.input->evbit);
  set_bit(EV_ABS, elan_touch_data.input->evbit);	
	set_bit(BTN_TOUCH, elan_touch_data.input->keybit);	
	set_bit(BTN_TOOL_FINGER, elan_touch_data.input->keybit);
	set_bit(BTN_TOOL_DOUBLETAP, elan_touch_data.input->keybit); 

//  input_mt_create_slots(elan_touch_data.input, MAX_CONTACTS);
//  input_set_abs_params(elan_touch_data.input, ABS_MT_SLOT, 0, MAX_CONTACTS - 1, 0, 0);
	input_set_abs_params(elan_touch_data.input, ABS_MT_POSITION_X, 0, SCALE_X, 0, 0);	
	input_set_abs_params(elan_touch_data.input, ABS_MT_POSITION_Y, 0, SCALE_Y, 0, 0);	
	
  rc = input_register_device(elan_touch_data.input);	
	if (rc < 0) {		
		goto free_device;
	}	
 
  //gpio_neonode_bsl_init();
  
	//elan_touch_data.intr_gpio = S3C2410_GPG1;
  /*printk(KERN_INFO "Sending HELLO\n");
  rc = i2c_smbus_write_byte(elan_touch_data.client, hello_packet);
  if (rc<0)
  {
    printk(KERN_ERR "Could not write hello packet\n");
    goto fail;
  }*/
  
  printk(KERN_INFO "Doing INIT\n");
  /*rc = __elan_touch_init(client);	
	if (rc < 0)
  {
		printk(KERN_ERR "Read Hello Packet Fail\n");
		goto unregister_device;	
  }*/
  
  printk(KERN_INFO "Getting IRQ\n");
  elan_touch_data.use_irq = 1;
  elan_touch_data.client->irq = 0;
	rc = elan_touch_register_interrupt(elan_touch_data.client);
  if (rc < 0)
  {
    printk(KERN_ERR "Could not get IRQ\n");
    goto unregister_device;
  } 
	
  printk(KERN_INFO "Elan init succeeded\n");
	return 0;
unregister_device:
  input_unregister_device(elan_touch_data.input);
free_device:
	if (NULL != elan_touch_data.input)
	{
		input_free_device(elan_touch_data.input);
		elan_touch_data.input = NULL;
	}
fail:	
	/*if (NULL !=  elan_wq) {
		destroy_workqueue(elan_wq);
		elan_wq = NULL;
	}*/
  if (elan_touch_data.client->irq)
  {
    free_irq(elan_touch_data.client->irq, NULL);
    elan_touch_data.client->irq = 0;
  }
 	printk(KERN_ERR "Run elan touch probe error %d!\n",rc);
	return rc;
}

static int __devexit elan_touch_remove(struct i2c_client *client)
{	
	if (elan_wq)		
		destroy_workqueue(elan_wq);	
	input_unregister_device(elan_touch_data.input);	
	if (elan_touch_data.use_irq)
  {
		free_irq(client->irq, NULL);	
    elan_touch_data.client->irq = 0;
  }
	else		
		hrtimer_cancel(&elan_touch_data.timer);	
	return 0;
}

static const struct i2c_device_id elan_touch_id[] = {
  {ELAN_TS_NAME, 0},
  {},
};

MODULE_DEVICE_TABLE(i2c, elan_touch_id);

static struct i2c_driver elan_touch_driver = {
	//.attach_adapter	= elan_touch_attach,
	// .detach_client	= elan_touch_detach,
	.remove		= __devexit_p(elan_touch_remove),
  .probe    = elan_touch_probe,
	.id_table	= elan_touch_id,
	.driver		= {
		.name = ELAN_TS_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init elan_touch_init(void)
{
  int res;
  
  res = i2c_add_driver(&elan_touch_driver);
  if (res < 0)
  {
    printk(KERN_ERR "add elan_touch_driver failed\n");
    return -ENODEV;
  }
  printk(KERN_INFO "elan_touch_driver added\n");
  return res;
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


/*
 * SONY IR Touchpanel driver
 *
 * Copyright (c) 2010 SONY Coporation.
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/string.h>
#include <linux/serial.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <mach/mxc_uart.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>
#ifdef CONFIG_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/wakelock.h> /* 2011/06/07 */
#if 1 /* E_BOOK *//* bugfix for power leak 2011/06/09 */
#include <mach/iomux-mx50.h>
#endif

#define DRIVER_DESC	"SONY IR Touchpanel driver"

MODULE_AUTHOR("SONY");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

/*
 * Definitions & global arrays.
 */

#define BOARD_ID	(5*32+14)	/* GPIO_6_14 */

/* state */
#define IRTP_NONE 0
#define IRTP_INIT 1
#define IRTP_RUN  2
#define IRTP_SUSPEND 3
#define IRTP_RESUME 4

/* */
#define IRTP_MAX_LENGTH 1024

#define IRTP_MIN_XC 0
#define IRTP_MAX_XC 600
#define IRTP_MIN_YC 0
#define IRTP_MAX_YC 800

/* protocol data */
#define IRTP_PROTO_STARTMARK (0xee)

/* commands id */
#define IRTP_CMD_DEACTIVE	(0x00)
#define IRTP_CMD_ACTIVATE	(0x01)
#define IRTP_CMD_SETRESOLUTION	(0x02)
#define IRTP_CMD_SETCONF	(0x03)
#define IRTP_CMD_TOUCHDATA 	(0x04)
#define IRTP_CMD_SCANFREQ	(0x08)
#define IRTP_CMD_SETMAXAREA	(0x09)
#define IRTP_CMD_VERSION	(0x0a)
#define IRTP_CMD_SIGNALALERT 	(0x0d)
#define IRTP_CMD_FIXEDPULSESTRENGTH	(0x0f)
#define IRTP_CMD_LEDTIMESTRENGTH	(0x1b)
#define IRTP_CMD_LEDLEVEL		(0x1c)
#define IRTP_CMD_INVALIDCMD	(0xfe)

/* TOUCHDATA values */
#define IRTP_DATA_TOUCH 	(0x00)
#define IRTP_DATA_MOVE 		(0x01)
#define IRTP_DATA_RELEASE	(0x02)
#define IRTP_GET_XC(data1,data2) (((data2)<<8) | (data1))
#define IRTP_GET_YC(data1,data2) (((data2)<<8) | (data1))


/* TOUCH Panel ID */
#define IRTP_PANEL_ID_SUBCPU 0
#define IRTP_PANEL_ID_IR 1

/* Parameter ID */
#define IRTP_PRM_SETRES 	0x01
#define IRTP_PRM_SCAN_PEN 	0x02
#define IRTP_PRM_SCAN_FINGER	0x04
#define IRTP_PRM_SCAN_IDLE	0x08
#define IRTP_PRM_TOUCHAREA	0x10

#define IRTP_PRM_ALL		0xff

/* */
#define IRTP_RETRYMAX 1
#define IRTP_RECV_TIMEOUT     1000
#define IRTP_WAIT_CTS_TIMEOUT 1000
#define IRTP_PWR_OFF_DELAY	300
#define IRTP_RECOVER_TIMER	HZ

#define _MX50_GPIO(num, bit)    (((num) - 1) * 32 + (bit))
#define IRTP_PIN_TP_SBWTDIO	(_MX50_GPIO(4,15))
#define IRTP_PIN_TP_SBWTCK	(_MX50_GPIO(4,16))

/* Power Rail switch */
#define IRTP_PWR_RAIL_SW	(_MX50_GPIO(3,22))

/* power state */
#define IRTP_PWROFF 0
#define IRTP_PWRON 1

static void irtp_recover(unsigned long data);

/*
 */
static int irtp_resolution_x = IRTP_MAX_XC;
static int irtp_resolution_y = IRTP_MAX_YC;
static int irtp_scan_idle=33;
static int irtp_scan_finger=33;
static int irtp_scan_pen=70;
static int irtp_max_touch_area=25;

struct input_dev *ir_tp_input_dev = NULL;

/*
 * Per-touchscreen data.
 */

struct irtp {
  char *name;
  struct input_dev *dev;
  int type;
  int slot;
  int idx;
  unsigned char data[IRTP_MAX_LENGTH];
  char phys[32];
  int (*poweron)(struct irtp*);
  int (*poweroff)(struct irtp*);
  int (*init)(struct irtp*);
  int (*deinit)(struct irtp*);
  int (*fwupdate)(struct irtp*);
  int (*exit_fwupdate)(struct irtp*);
  struct mxc_ssw_recept *recept;
  struct semaphore commlock;
  struct completion       completion;
  wait_queue_head_t wait;
  int recvdone;
  int state;
  int pwrstate;
  struct early_suspend earlysuspend;
  struct wake_lock touchlock;  /* wake lock touch. */ /* 2011/06/07 */
	struct wake_lock receiving_lock;
	struct wake_lock recovery_wake_lock;
	struct timer_list recovertimer; /* timer for recovery */
	struct work_struct recoverwork; /* workqueue for recovery */
	struct mutex recovery_lock;
};


static int irtp_cts_off(struct irtp *irtp); 	//CTS HI /* for power leak at cold boot 2011/08/23 */
static int irtp_cts_on(struct irtp *irtp); 		//CTS LOW /* for power leak at cold boot 2011/08/23 */

/*-------------------
 * Power Management 
 --------------------*/

int irtp_power_on(struct irtp *irtp)
{
  struct regulator *regulator;
  int rc;
#if 1 /* E_BOOK *//* bugfix for leak 2011/06/09 */
  struct pad_desc pon_pad_desc1=MX50_PAD_ECSPI1_SS0__GPIO_4_15;
  //struct pad_desc pon_pad_desc2=MX50_PAD_ECSPI2_SCLK__GPIO_4_16;
#endif

  if ( irtp->pwrstate == IRTP_PWRON) {
    /* already power on */
    return 0;
  }
  /* change Serial-Pin to input */
#if 1 /* E_BOOK *//* bugfix for leak 2011/06/09 */
  mxc_iomux_v3_setup_pad(&pon_pad_desc1);
  //mxc_iomux_v3_setup_pad(&pon_pad_desc2);
#endif
  mxc_ssw_port_close(irtp->slot);
  gpio_request(IRTP_PIN_TP_SBWTDIO, "irtp");
  gpio_direction_output(IRTP_PIN_TP_SBWTDIO,0);
  gpio_request(IRTP_PIN_TP_SBWTCK, "irtp");
  gpio_direction_output(IRTP_PIN_TP_SBWTCK,0);
  
#ifdef CONFIG_REGULATOR_TWL4030
  /* power on to Phoenix light */
  regulator = regulator_get(NULL,"LDO7");
  if ( regulator == NULL ) {
    return -ENOENT;
  }
#if 1 /* E_BOOK *//* for power leak 2011/04/19 */
  /* 1st step suplying 2.0V */
  rc = regulator_set_voltage(regulator,2000000,2100000);
  if ( rc < 0 ) {
    printk("IRTP:power set voltage error(2V) %d\n",rc);
    regulator_put(regulator);
    return rc;
  }
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
  msleep(5);
  /* 2ndt step suplying 2.0V */
  rc = regulator_set_voltage(regulator,3200000,3200000);
  if ( rc < 0 ) {
    printk("IRTP:power set voltage error(3.2V)\n");
    regulator_put(regulator);
    return rc;
  }
#if 0  /* E_BOOK *//* bugfix:delete for disable IR power 2011/05/31 */
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
#endif
  msleep(5);
#else
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
#endif
  mdelay(1);
  regulator_put(regulator);
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
  /* power on to WM8321 */
  if ( gpio_get_value(IRTP_PWR_RAIL_SW)) {
    regulator = regulator_get(NULL,"LDO4");
  } else {
    regulator = regulator_get(NULL,"LDO10");
  }
  if ( regulator == NULL ) {
    return -ENOENT;
  }
#if 0 
  /* 1st step suplying 2.0V */
  rc = regulator_set_voltage(regulator,2000000,2100000);
  if ( rc < 0 ) {
    printk("IRTP:power set voltage error(2V) %d\n",rc);
    regulator_put(regulator);
    return rc;
  }
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
  msleep(5);
  /* 2ndt step suplying 2.0V */
  rc = regulator_set_voltage(regulator,3200000,3200000);
  if ( rc < 0 ) {
    printk("IRTP:power set voltage error(3.2V)\n");
    regulator_put(regulator);
    return rc;
  }
#if 0  /* E_BOOK *//* bugfix:delete for disable IR power 2011/05/31 */
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
#endif
  msleep(5);
#else
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
#endif
  //mdelay(1);
  mdelay(5); /* change to 5msec 2011/07/14 */
  regulator_put(regulator);
#endif

  /* change TP_SBWTDIO to High */
  gpio_set_value(IRTP_PIN_TP_SBWTDIO, 1);
  mdelay(1);

  (void)irtp_cts_off(irtp); 	//CTS HI /* delayed CTS off 2011/08/23 */ 

  gpio_direction_input(IRTP_PIN_TP_SBWTDIO);
  gpio_direction_input(IRTP_PIN_TP_SBWTCK);
  gpio_free(IRTP_PIN_TP_SBWTDIO);
  gpio_free(IRTP_PIN_TP_SBWTCK);
  msleep(75);
  /* change Serial-Pin to normal */
  mxc_ssw_port_open(irtp->slot);

  irtp->pwrstate = IRTP_PWRON;
  return 0;
}

int irtp_power_off(struct irtp *irtp)
{
  struct regulator *regulator;
  int rc;
#if 1 /* E_BOOK *//* bugfix for leak 2011/06/09 */
  struct pad_desc poff_pad_desc1=MX50_PAD_ECSPI1_SS0__GPIO_4_15_POFF;
  //struct pad_desc poff_pad_desc2=MX50_PAD_ECSPI2_SCLK__GPIO_4_16_POFF;
#endif
  
  if ( irtp->pwrstate == IRTP_PWROFF) {
    /* already power off */
    return 0;
  }
	
#ifdef CONFIG_REGULATOR_TWL4030
  /* power off to Phoenix light */
  regulator = regulator_get(NULL,"LDO7");
  if ( regulator == NULL ) {
    return -ENOENT;
  }  rc = regulator_disable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power off failed\n");
    regulator_put(regulator);
    return rc;
  }
  regulator_put(regulator);
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
  /* power off to WM8321 */
  if ( gpio_get_value(IRTP_PWR_RAIL_SW)) {
    regulator = regulator_get(NULL,"LDO4");
  } else {
    regulator = regulator_get(NULL,"LDO10");
  }
  if ( regulator == NULL ) {
    return -ENOENT;
  }  rc = regulator_disable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power off failed\n");
    regulator_put(regulator);
    return rc;
  }
  regulator_put(regulator);
#endif

  /* change TP_SBWTDIO to Low */
#if 1 /* E_BOOK *//* bugfix for leak 2011/06/09 */
  mxc_iomux_v3_setup_pad(&poff_pad_desc1);
  //mxc_iomux_v3_setup_pad(&poff_pad_desc2);
#endif
  gpio_set_value(IRTP_PIN_TP_SBWTDIO, 0);
  msleep(100);
  irtp->pwrstate = IRTP_PWROFF;
  return 0;
}
int subcpu_power_on(struct irtp *irtp)
{
  /* nothing */
  irtp->pwrstate = IRTP_PWRON;

  irtp_cts_off(irtp); 	//CTS HI /* delayed CTS off 2011/08/23 */ 

  return 0;
}

int subcpu_power_off(struct irtp *irtp)
{
  /* nothing */
  irtp->pwrstate = IRTP_PWROFF;
  return 0;
}

/*----------------------------------*/
/*-------------------
 * Serial IO
 --------------------*/
// Set CTS LOW
static int irtp_cts_on(struct irtp *irtp)
{
  int rc;
  int status;
  int slot=irtp->slot;
  status = TIOCM_RTS;
  rc = mxc_ssw_set_mctrl(slot,status);

  return rc;
}

// Set CTS HI
static int irtp_cts_off(struct irtp *irtp)
{
  int rc;
  int status;
  int slot=irtp->slot;
  status = 0;
  rc = mxc_ssw_set_mctrl(slot,status);

  return rc;
}

static int irtp_wait_rts_low(int slot,int timeout)
{
  unsigned int status;
  int rc;
  int cnt;

  cnt = 0;
  //timeout *= 1000;
	timeout *= 100;
  while( true ) {
    rc = mxc_ssw_get_mctrl(slot,&status);
    if ( rc < 0 ) {
      return rc;
    }
    if ((status & TIOCM_CTS)!=0) {
      break;
    }
    udelay(1);
    cnt++;
    if ( cnt > timeout) {
      return -ETIMEDOUT;
    }
  }
  return 0;
}
static int irtp_wait_rts_high(int slot,int timeout)
{
  int status;
  int rc;
  int cnt;

  cnt = 0;
  //timeout *= 1000;
	timeout *= 100;
  while( true ) {
    rc = mxc_ssw_get_mctrl(slot,&status);
    if ( rc < 0 ) {
      return rc;
    }
    //printk("sonyirtp:irtp_wait_rts_high:status =%x\n",status);
    if ((status & TIOCM_CTS)==0) {
      break;
    }
    udelay(1);
    cnt++;
    if ( cnt > timeout) {
      return -ETIMEDOUT;
    }
  }
  return 0;
}

static int irtp_send_recv_wait(struct irtp *irtp,char *sendbuf,int size, unsigned char reply)
{
  int retry=0;
  int rc;
  //unsigned long flag;

  down(&irtp->commlock);

  for ( retry = 0 ; retry < IRTP_RETRYMAX; retry++) {
    rc = irtp_wait_rts_high(irtp->slot,IRTP_WAIT_CTS_TIMEOUT);
    if ( rc < 0 ) {
      printk("sonyirtp:send_recv_wait: wait rts high err(before send)(%d)\n",rc);
      continue;
    }
    //local_irq_save(flag);
    irtp_cts_on(irtp);		//CTS LOW
    rc = irtp_wait_rts_low(irtp->slot,IRTP_WAIT_CTS_TIMEOUT);
    if ( rc < 0 ) {
      printk("sonyirtp:send_recv_wait: wait rts low err(before send)(%d)\n",rc);
      //local_irq_restore(flag);
      msleep(1);
      continue;
    }
    //local_irq_restore(flag);
    //udelay(10);
    irtp->recvdone = 0;
    if ( irtp->type == 0 ) {
      irtp_cts_off(irtp);	// CTS HI
    }
    rc = mxc_ssw_send(irtp->slot,sendbuf,size);
    if (rc < 0 ) {
      printk("IRTP:%s send error %d\n",irtp->name,-rc);
      up(&irtp->commlock);
      return rc;
    }
    if ( irtp->type == 1) {
      /* for Subcpu control */
      udelay(500);
      irtp_cts_off(irtp);	// CTS HI
    }
#if 0
    //udelay(10);
    //udelay(10);
    //rc = irtp_wait_rts_high(irtp->slot,IRTP_WAIT_CTS_TIMEOUT);
    if ( rc < 0 ) {
      printk("sonyirtp:send_recv_wait: wait rts high err(%d)\n",rc);
    }
    rc = irtp_wait_rts_low(irtp->slot,IRTP_WAIT_CTS_TIMEOUT);
    if ( rc < 0 ) {
      printk("sonyirtp:send_recv_wait: wait rts low err(%d)\n",rc);
    }
    //udelay(100);
    //msleep(1);
    irtp_cts_on(irtp);		//CTS LOW
#endif
    rc = wait_event_timeout(irtp->wait,(irtp->recvdone!=0),IRTP_RECV_TIMEOUT);
    if ( rc == 0 ) {
      irtp_cts_off(irtp);	// CTS HI
      up(&irtp->commlock);
      printk("IRTP:Recv time out retry\n");
      continue;
    }
    if ( irtp->data[2] == reply ) {
      //irtp_cts_off(irtp);	// CTS HI
      up(&irtp->commlock);
      return 0;
    }
  }
  irtp_cts_off(irtp);	// CTS HI
  up(&irtp->commlock);
  return -ETIMEDOUT;
}
static int irtp_send_only(struct irtp *irtp,char *sendbuf,int size)
{
  int rc;

  down(&irtp->commlock);

  rc = irtp_wait_rts_high(irtp->slot,IRTP_WAIT_CTS_TIMEOUT);
  if ( rc < 0 ) {
    printk("sonyirtp:send_only: wait rts high err(before send)(%d)\n",rc);
    return -ETIMEDOUT;
  }
  irtp_cts_on(irtp);		//CTS LOW
  rc = irtp_wait_rts_low(irtp->slot,IRTP_WAIT_CTS_TIMEOUT);
  if ( rc < 0 ) {
    printk("sonyirtp:send_only: wait rts low err(before send)(%d)\n",rc);
  }
  irtp->recvdone = 0;
  if ( irtp->type == 0 ) {
    irtp_cts_off(irtp);	// CTS HI
  }
  rc = mxc_ssw_send(irtp->slot,sendbuf,size);
  if (rc < 0 ) {
    printk("IRTP:%s send error %d\n",irtp->name,-rc);
    up(&irtp->commlock);
    return rc;
  }
  if ( irtp->type == 1) {
    /* for Subcpu control */
    udelay(500);
    irtp_cts_off(irtp);	// CTS HI
  }
  up(&irtp->commlock);
  return 0;
}
/*---------------------------------
 * parameters  
 *---------------------------------*/
int irtp_activate(struct irtp *irtp)
{
  unsigned char cmd_activate[]={IRTP_CMD_ACTIVATE};
  int rc;

  rc = irtp_send_recv_wait(irtp,cmd_activate,sizeof(cmd_activate),IRTP_CMD_ACTIVATE);
  if ( rc == 0 ) {
    //printk("sonyirtp:activate: data3=%x,rc=%d\n",irtp->data[3],rc);
    if ( irtp->data[3] != 0 ) {
      return -EIO;
    }
  }
  return rc;
}

int irtp_deactive(struct irtp *irtp)
{
  unsigned char cmd_deactive[]={IRTP_CMD_DEACTIVE};
  int rc;

  rc = irtp_send_recv_wait(irtp,cmd_deactive,sizeof(cmd_deactive),IRTP_CMD_DEACTIVE);
  if ( rc == 0 ) {
    if ( irtp->data[3] != 0 ) {
      return -EIO;
    }
  }
  return rc;
}

int irtp_setup_resolution(struct irtp *irtp)
{
  unsigned char cmd_setres[]={0x02,0x58,0x02,0x20,0x03};
  int rc;

  //printk("irtp:setup res x=%d,y=%d\n",irtp_resolution_x,irtp_resolution_y);
  cmd_setres[1] = (irtp_resolution_x     ) & 0xff;
  cmd_setres[2] = (irtp_resolution_x >> 8) & 0xff;
  cmd_setres[3] = (irtp_resolution_y     ) & 0xff;
  cmd_setres[4] = (irtp_resolution_y >> 8) & 0xff;

  rc = irtp_send_recv_wait(irtp,cmd_setres,sizeof(cmd_setres),0x02);
  if ( rc == 0 ) {
    if ( irtp->data[3] != 0 ) {
      return -EIO;
    }
  }
  return rc;
}

int irtp_setup_scan(struct irtp *irtp)
{
  unsigned char cmd_setscan[]={0x08,0x20,0x20,0x20};
  int rc;

  //printk("irtp:setup scan %d,%d,%d\n",irtp_scan_idle,irtp_scan_finger,irtp_scan_pen);
  cmd_setscan[1] = irtp_scan_idle   & 0xff;
  cmd_setscan[2] = irtp_scan_finger & 0xff;
  cmd_setscan[3] = irtp_scan_pen    & 0xff;

  rc = irtp_send_recv_wait(irtp,cmd_setscan,sizeof(cmd_setscan),0x08);
  if ( rc == 0 ) {
    if ( irtp->data[3] != 0 ) {
      return -EIO;
    }
  }
  return rc;
}
int irtp_setup_toucharea(struct irtp *irtp)
{
  unsigned char cmd_settoucharea[]={0x09,0x00,0x5};
  int rc;

  //printk("irtp:setup touch area %d\n",irtp_max_touch_area);
  cmd_settoucharea[1] = 1;
  cmd_settoucharea[2] = irtp_max_touch_area;

  rc = irtp_send_recv_wait(irtp,cmd_settoucharea,sizeof(cmd_settoucharea),0x09);
  if ( rc == 0 ) {
    if ( irtp->data[3] != 0 ) {
      return -EIO;
    }
  }
  return rc;
}
int irtp_setup_parameter(struct irtp *irtp,int flag ) 
{
  int rc;

  msleep(1);
  if ( flag & IRTP_PRM_SETRES ) {
    rc = irtp_setup_resolution(irtp);
    if (rc < 0 ) {
    	printk("Error fail to send resolution command!!!\n");
      return rc;
    }
  }
  msleep(1);
  if ( flag & IRTP_PRM_SCAN_PEN ) {
    rc = irtp_setup_scan(irtp);
    if ( rc < 0 ) {
    	printk("Error fail to send scan frequency command for pen!!!\n");
      return rc;
    }
  }
  msleep(1);
  if ( flag & IRTP_PRM_SCAN_FINGER ) {
    rc = irtp_setup_scan(irtp);
    if ( rc < 0 ) {
    	printk("Error fail to send scan frequency command for finger!!!\n");
      return rc;
    }
  }
  msleep(1);
  if ( flag & IRTP_PRM_SCAN_IDLE ) {
    rc = irtp_setup_scan(irtp);
    if ( rc < 0 ) {
    	printk("Error fail to send scan frequency command for idle!!!\n");
      return rc;
    }
  }
  msleep(1);
  if ( flag & IRTP_PRM_TOUCHAREA ) {
    rc = irtp_setup_toucharea(irtp);
    if ( rc < 0 ) {
    	printk("Error fail to send set max area command!!!\n");
      return rc;
    }
  }
  return 0;
}

int irtp_enable_mt(struct irtp *irtp)
{
  unsigned char cmd_enablemt[]={0x03,0x01,0x00,0x00,0x00};
  int rc;

  //printk("irtp:enable dual touch\n");

  rc = irtp_send_recv_wait(irtp,cmd_enablemt,sizeof(cmd_enablemt),0x03);
  if ( rc == 0 ) {
    if ( irtp->data[3] != 0 ) {
      return -EIO;
    }
  }
  return rc;
}

int irtp_disable_mt(struct irtp *irtp)
{
  unsigned char cmd_disablemt[]={0x03,0x00,0x00,0x00,0x00};
  int rc;

  //printk("irtp:disable dual touch\n");

  rc = irtp_send_recv_wait(irtp,cmd_disablemt,sizeof(cmd_disablemt),0x03);
  if ( rc == 0 ) {
    if ( irtp->data[3] != 0 ) {
      return -EIO;
    }
  }
  return rc;
}
/*---------------------------------*/
int irtp_touchstart(struct irtp *irtp)
{
  unsigned char cmd_touchstart[]={IRTP_CMD_TOUCHDATA};
  int rc;

  rc = irtp_send_only(irtp,cmd_touchstart,sizeof(cmd_touchstart));
  if ( rc ) {
    printk("sonyirtp:touch start error %d\n",rc);
  }
  return rc;
}

#if 1 /* E_BOOK *//* Add IR F/W Update mode 2011/03/11 */
/*
 * f/w update 
 */

int irtp_fwupdate(struct irtp *irtp)
{
  struct regulator *regulator;
  int rc;

  /* power off */
  printk("power off IR TP\n");
  irtp->poweroff(irtp);
  //udelay(200);
  msleep(200);

  /* SBWTDIO=0,SBWTCK=0 */
  gpio_request(IRTP_PIN_TP_SBWTDIO, "irtp");
  gpio_direction_output(IRTP_PIN_TP_SBWTDIO, 0);
  gpio_request(IRTP_PIN_TP_SBWTCK, "irtp");
  gpio_direction_output(IRTP_PIN_TP_SBWTCK, 0);

  /* power on */
#ifdef CONFIG_REGULATOR_TWL4030
  regulator = regulator_get(NULL,"LDO7");
  if ( regulator == NULL ) {
    return -ENOENT;
  }
#if 1 /* E_BOOK *//* for power leak 2011/06/07 */
  /* 1st step suplying 2.0V */
  rc = regulator_set_voltage(regulator,2000000,2100000);
  if ( rc < 0 ) {
    printk("IRTP:power set voltage error(2V) %d\n",rc);
    regulator_put(regulator);
    return rc;
  }
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
  msleep(5);
  /* 2nd step suplying 3.2V */
  rc = regulator_set_voltage(regulator,3200000,3200000);
  if ( rc < 0 ) {
    printk("IRTP:power set voltage error(3.2V)\n");
    regulator_put(regulator);
    return rc;
  }
  msleep(5);
#else
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
#endif
  irtp->pwrstate = IRTP_PWRON;
  mdelay(1);
  regulator_put(regulator);
#endif
	
#ifdef CONFIG_REGULATOR_WM831X
  /* power on */
  if ( gpio_get_value(IRTP_PWR_RAIL_SW)) {
    regulator = regulator_get(NULL,"LDO4");
  } else {
    regulator = regulator_get(NULL,"LDO10");
  }
  if ( regulator == NULL ) {
    return -ENOENT;
  }
  rc = regulator_enable(regulator);
  if ( rc < 0 ) {
    printk("IRTP:power on failed\n");
    regulator_put(regulator);
    return rc;
  }
  irtp->pwrstate = IRTP_PWRON;
  //mdelay(1);
  mdelay(5); /* change to 5msec 2011/07/14 */
  regulator_put(regulator);
#endif
	
  printk("IRTP:Power ON for Update\n");
  msleep(40);
  gpio_set_value(IRTP_PIN_TP_SBWTCK, 1);
  udelay(200);
  gpio_set_value(IRTP_PIN_TP_SBWTCK, 0);
  udelay(200);
  gpio_set_value(IRTP_PIN_TP_SBWTCK, 1);
  udelay(200);
  gpio_set_value(IRTP_PIN_TP_SBWTDIO,1);
  udelay(200);
  gpio_set_value(IRTP_PIN_TP_SBWTCK, 0);  
  gpio_free(IRTP_PIN_TP_SBWTDIO);
  gpio_free(IRTP_PIN_TP_SBWTCK);
  return 0;
}
int irtp_exit_fwmode(struct irtp *irtp)
{
  /* power off */
  printk("power off IR TP\n");
  irtp->poweroff(irtp);
  gpio_request(IRTP_PIN_TP_SBWTDIO, "irtp");
  gpio_direction_input(IRTP_PIN_TP_SBWTDIO);
  gpio_request(IRTP_PIN_TP_SBWTCK, "irtp");
  gpio_direction_input(IRTP_PIN_TP_SBWTCK);
  gpio_free(IRTP_PIN_TP_SBWTDIO);
  gpio_free(IRTP_PIN_TP_SBWTCK);
  return 0;
}
#endif
/*---------------------------------*/

/*
 * sysfs setup
 */
#if 1 /* E_BOOK *//* for diag 2011/1/25 */
static int showtouch=0;
/* add card sleep control */
static ssize_t sonyirtp_touchdata_show (struct device *dev,
					struct device_attribute *attr, char *buf)
{

  if ( showtouch ) {
    snprintf(buf,PAGE_SIZE, "Show Touch data\n");
  } else {
    snprintf(buf,PAGE_SIZE, "Not Show\n");
  }
  return strlen(buf);
}
static ssize_t sonyirtp_touchdata_set (struct device *dev,
				       struct device_attribute *attr, char *buf,size_t size)
{
  int flag;
  flag = simple_strtol(buf,NULL,0);
  if ( flag) {
    showtouch=1;
  } else {
    showtouch=0;
  }
  return strlen(buf);
}

static DEVICE_ATTR(touchdata, 0644, sonyirtp_touchdata_show,sonyirtp_touchdata_set);
#endif

/*---------------------------------*/
#if 1 /* E_BOOK *//* 2011/03/07 */
static int old_single_x=0;
static int old_single_y=0;
static int old_single_w=0;
static int old_single_h=0;
static int old_dual_x[2]={0,0};
static int old_dual_y[2]={0,0};
static int old_dual_w[2]={0,0};
static int old_dual_h[2]={0,0};
static int old_dual_event[2] = {IRTP_DATA_RELEASE,IRTP_DATA_RELEASE};
static int dual_mode=0;
#endif
static void irtp_print_touchdata(char *data)
{
  char *statchar;
  int state;
  state = (data[4]&0xc0) >> 6;
  if ( state == IRTP_DATA_TOUCH) {
    statchar="DOWN";
  } else if (state == IRTP_DATA_RELEASE) {
    statchar="UP";
  } else if (state == IRTP_DATA_MOVE) {
    statchar="MOVE";
  } else {
    statchar="";
  }
  printk(KERN_INFO "IR,%d,%s,%d,%d,%d,%d\n",
	 (data[4] >> 2) & 0x0f,
	 statchar,
	 IRTP_GET_XC(data[0], data[1]),
	 IRTP_GET_YC(data[2], data[3]),
	 ((data[4] & 0x03) << 3) | ((data[5]>>5) & 0x7),
	 (data[5] & 0x1f)
	 );
}

static void irtp_upto_single_event(struct input_dev *dev)
{
  int x1,y1,w,h;
  x1 = old_single_x;
  y1 = old_single_y;
  w = old_single_w;
  h = old_single_h;
  //printk("irtp: s: x=%d,y=%d,w=%d,h=%d\n",x1,y1,w,h);
  input_report_abs(dev, ABS_MT_TOUCH_MAJOR, max(w,h));
  input_report_abs(dev, ABS_MT_POSITION_Y, y1);
  input_report_abs(dev, ABS_MT_POSITION_X, x1);
  input_mt_sync(dev);
  input_sync(dev);  
}
static void irtp_upto_dual_event(struct input_dev *dev)
{
  int i;
  int x1,y1,w,h;
  for ( i = 0 ; i < 2 ; i++) {
    x1 = old_dual_x[i];
    y1 = old_dual_y[i];
    w = old_dual_w[i];
    h = old_dual_h[i];
    //printk("irtp: mt: id=%d,x=%d,y=%d,w=%d,h=%d\n",i,x1,y1,w,h);
    input_report_abs(dev, ABS_MT_TOUCH_MAJOR, max(w,h));
    input_report_abs(dev, ABS_MT_POSITION_Y, y1);
    input_report_abs(dev, ABS_MT_POSITION_X, x1);
    input_mt_sync(dev);
  }
  input_sync(dev);  
}

static void irtp_send_dumy_up_event(void)
{
	int i;
	int x1,y1,w,h;
	
	if(ir_tp_input_dev ==  NULL){
		return;
	}
	
	for ( i = 0 ; i < 2 ; i++) {
		if(old_dual_event[i] != IRTP_DATA_RELEASE){
			old_dual_event[i] = IRTP_DATA_RELEASE;
			x1 = old_dual_x[i];
			y1 = old_dual_y[i];
			w = 0;
			h = 0;
			input_report_abs(ir_tp_input_dev, ABS_MT_TOUCH_MAJOR, max(w,h));
			input_report_abs(ir_tp_input_dev, ABS_MT_POSITION_Y, y1);
			input_report_abs(ir_tp_input_dev, ABS_MT_POSITION_X, x1);
			input_mt_sync(ir_tp_input_dev);
		}
	}
	input_sync(ir_tp_input_dev);  
}

static void irtp_process_format_tablet(struct irtp *irtp)
{
  struct input_dev *dev = irtp->dev;
  int i;
  int count;
  unsigned int x1,y1,w,h,id;
  int datanum;
  int event;
	int checknum;
  
#if 0 /* for DEBUG */
  printk("irtp_process_format_tablet:data[2]=%x\n",irtp->data[2]);
#endif
	switch ( irtp->data[2] ) {
	case IRTP_CMD_TOUCHDATA:
		if ( dev == NULL ) {
			printk("IRTP: inputdev be not registered\n");
			return;
		}
		wake_lock_timeout(&irtp->touchlock,HZ/2);
		count=4;
		/* up to input information */
		datanum = irtp->data[3];
		checknum = (irtp->data[1] - 1)/7;
		if ( (datanum != checknum) || (datanum > 2)) {
			printk(KERN_ERR "IRTP: wrong touch data count(count=%d,size=%d)\n",datanum,irtp->data[1]);
			if(irtp->type == 0){ // IR touchpanel
				/* run recovery */
				irtp_recover((unsigned long)irtp);
			}
			goto errorout;
		}
		for ( i = 0 ; i < datanum; i++) {
		/* pickup touch data */
#if 1 /* E_BOOK *//* for diag 2011/1/25 */
			if ( showtouch ) {
				irtp_print_touchdata(&(irtp->data[count]));
			}
#endif
#if 1 /* E_BOOK *//* Change TOP position 2011/2/21 */
			x1 = IRTP_GET_XC(irtp->data[count],  irtp->data[count+1]);
			y1 = IRTP_GET_YC(irtp->data[count+2],irtp->data[count+3]);
#else
#if 1 /* E_BOOK *//* support different orientation 2011/2/23 */
			if ( irtp->type == 1 ) {
				y1 = IRTP_MAX_XC - IRTP_GET_XC(irtp->data[count],  irtp->data[count+1]);
				x1 = IRTP_GET_YC(irtp->data[count+2],irtp->data[count+3]);
			} else {
				y1 = IRTP_GET_XC(irtp->data[count],  irtp->data[count+1]);
				x1 = IRTP_MAX_YC - IRTP_GET_YC(irtp->data[count+2],irtp->data[count+3]);
			}
#else
			y1 = IRTP_MAX_XC - IRTP_GET_XC(irtp->data[count],  irtp->data[count+1]);
			x1 = IRTP_GET_YC(irtp->data[count+2],irtp->data[count+3]);
#endif
#endif

			w = ((irtp->data[count+4] & 0x03) << 3) | ((irtp->data[count+5]>>5) & 0x7);
			h = (irtp->data[count+5] & 0x1f);
			id = (irtp->data[count+4] >> 2) & 0x0f;
			event = (irtp->data[count+4] & 0xc0 ) >> 6 ;

			if((irtp->type == 1) && (id == 0)){
				id=1;
			}
			
			if ( (id != 1) && (id != 2) ) {
				printk(KERN_ERR "IRTP: wrong touch id(%d)\n",id);
				if(irtp->type == 0){ // IR touchpanel
					/* run recovery */
					irtp_recover((unsigned long)irtp);
				}
				goto errorout;
			}
			
			if (event == IRTP_DATA_RELEASE) {
				x1 = old_dual_x[id-1];
				y1 = old_dual_y[id-1];
				w = 0;
				h = 0;
				old_dual_w[id-1] = 0;
				old_dual_h[id-1] = 0;
				old_dual_event[id-1] = event;
			} else {
				old_dual_x[id-1] = x1;
				old_dual_y[id-1] = y1;
				old_dual_w[id-1] = w;
				old_dual_h[id-1] = h;
      			old_dual_event[id-1] = event;
			}
				count += 7;
		}
		
		if ( datanum == 1 ) {
			if ((dual_mode == 0) || (event == IRTP_DATA_RELEASE)) {
				old_single_x = x1;
				old_single_y = y1;
				old_single_w = w;
				old_single_h = h;
				irtp_upto_single_event(dev);
				dual_mode = 0;
			} else {
				irtp_upto_dual_event(dev);
				dual_mode = 1;
			}
		} else {
			irtp_upto_dual_event(dev);
			dual_mode=1;
		}
		break;
	case IRTP_CMD_SIGNALALERT:
		printk(KERN_INFO "sonyirtp: low singnal alert. dump:");
		for ( i = 3; i < irtp->data[1]; i++ ) {
			printk("%02x ",irtp->data[i]);
		}
		printk("\n");
		break;
	default:
#if 0 /* for DEBUG */
		printk("sonyirtp: wakeup recv\n");
#endif
		irtp->recvdone=1;
		wake_up(&irtp->wait);
		break;
	}

errorout:

#if 1 /* E_BOOK *//* bugfix 2011/04/14 */
	irtp_cts_off(irtp);	// CTS HI
#else
	if ( irtp->type == 1 ) {
		irtp_cts_off(irtp);	// CTS HI
	}
#endif
}

/* For recovery */
static void irtp_runrecover(struct work_struct *work)
{
	struct irtp *irtp = container_of(work, struct irtp, recoverwork);
	
	if(irtp->pwrstate == IRTP_PWRON){
		mutex_lock(&irtp->recovery_lock);
		wake_lock(&irtp->recovery_wake_lock);
		printk(KERN_ERR "*********** irtp: Run recovery IR ***********\n");
		irtp_send_dumy_up_event();
		irtp_cts_on(irtp);	//CTS LOW
		mxc_ssw_port_close(irtp->slot);
		(void)irtp->poweroff(irtp);
		msleep(IRTP_PWR_OFF_DELAY);
		(void)irtp->poweron(irtp);
		irtp_cts_off(irtp);	//CTS HI
		mxc_ssw_port_open(irtp->slot);
		irtp->init(irtp);
		wake_unlock(&irtp->receiving_lock);
		wake_unlock(&irtp->recovery_wake_lock);
		printk(KERN_ERR "*********** irtp: END recovery IR ***********\n");
		mutex_unlock(&irtp->recovery_lock);
	}
	
}

static void iric_reboot(struct irtp *irtp)
{
	mutex_lock(&irtp->recovery_lock);
	printk(KERN_ERR "*********** irtp: Run IR IC Reboot ***********\n");
	wake_lock(&irtp->recovery_wake_lock);
	irtp_cts_on(irtp);	//CTS LOW
	mxc_ssw_port_close(irtp->slot);
	mxc_ssw_port_close(irtp->slot);
	(void)irtp->poweroff(irtp);
	msleep(IRTP_PWR_OFF_DELAY);
	(void)irtp->poweron(irtp);
	irtp_cts_off(irtp);	//CTS HI
	mxc_ssw_port_open(irtp->slot);
	wake_unlock(&irtp->receiving_lock);
	wake_unlock(&irtp->recovery_wake_lock);
	printk(KERN_ERR "*********** irtp: END IR IC Reboot ***********\n");
	mutex_unlock(&irtp->recovery_lock);
}

/* timer function for recovery */
static void irtp_recover(unsigned long data)
{
	struct irtp *irtp=(struct irtp*)data;

	if ( irtp->type == 0 ) {	/* RUN only IR touchpanel */
		/* run recovery */
		schedule_work(&irtp->recoverwork);
	} else {
		wake_unlock(&irtp->receiving_lock);
	}
}

static int irtp_recv(void * arg,unsigned char data)
{
  struct irtp* irtp = (struct irtp*)arg;
	
	if ( irtp->idx == 0 ) {
		if ( data == IRTP_PROTO_STARTMARK ) {
			/* setup timer,wakelock for recovery */
			wake_lock(&irtp->receiving_lock);
			if(irtp->type == 0){ // IR touchpanel
				mod_timer(&irtp->recovertimer, jiffies+IRTP_RECOVER_TIMER);
			}
			irtp->data[irtp->idx] = data;
			(irtp->idx)++;
		} else {
			if ( irtp->dev ) {
				if(irtp->type == 0){ // IR touchpanel
					/* run recovery */
					irtp_recover((unsigned long)irtp);
				}
				printk(KERN_ERR "sonyirtp:invalid startmark %x\n",data);
			}
		}
	} else if ( irtp->idx == 1 ) {
		if ( data <= (IRTP_MAX_LENGTH-2) ) {
			irtp->data[irtp->idx] = data;
			irtp->idx++;
		} else {
			if ( irtp->dev ) {
				if(irtp->type == 0){ // IR touchpanel
					/* run recovery */
					irtp_recover((unsigned long)irtp);
				}
				printk(KERN_ERR "sonyirtp:invalid data length %d\n",data);
			}
			irtp->idx=0;
		}
	} else {
		if ( data <= (IRTP_MAX_LENGTH-2)) {
			irtp->data[irtp->idx] = data;
			irtp->idx++;
		} else {
			if ( irtp->dev ) {
				if(irtp->type == 0){ // IR touchpanel
					/* run recovery */
					irtp_recover((unsigned long)irtp);
				}
				printk(KERN_ERR "sonyirtp:invalid data length %d\n",data);
			}
			irtp->idx=0;
		}
		if ( irtp->data[1] == (irtp->idx-2) ) {
			if(irtp->type == 0){ // IR touchpanel
				del_timer(&irtp->recovertimer);
			}
			irtp_process_format_tablet(irtp);
			irtp->idx=0;
			wake_unlock(&irtp->receiving_lock);
		}
	}
	return 0;
}

/*
 * setup touchpanel
 */
int irtp_setup(struct irtp *irtp) 
{
  int rc;
  //struct ktermios ts={0};
  //struct ktermios old={0};
  int baud = 115200;
  int bits = 8;
  int parity = 'n';
  int flow = 'n';
	int set_flag;
	
	set_flag = gpio_get_value(BOARD_ID);
	
  
#if 0
  ts.c_cflag = CREAD | HUPCL | CLOCAL | CS8 | B115200;
 
  rc = mxc_ssw_set_termios(irtp->slot, &ts,&old);
  if ( rc < 0 ) {
    printk("irtp_setup:set termios error %d\n",rc);
    return rc;
  }
#else
  rc = mxc_ssw_set_options(irtp->slot,(struct console*)NULL,baud, parity, bits, flow);
  if ( rc < 0 ) {
    printk("irtp_setup:set termios error %d\n",rc);
    return rc;
  }
#endif
	
error_retry:	
	
#if 1 /* E_BOOK *//* bugfix  2011/03/07 */
  (void)irtp_cts_off(irtp);	//CTS HI
#endif
	
	irtp->idx = 0;	/* clear buffer */

  //printk("irtp_setup: tp activate\n");
  rc = irtp_activate(irtp);
	if ( rc < 0 ) {
		printk("Error fail to send activate command!!!\n");
#ifndef CONFIG_USB_G_SERIAL_MODULE
		if( set_flag )  {	//In the case of Set.
			iric_reboot(irtp);
			goto error_retry;
		}else{
			return rc;
		}
#else
		printk("Don't retry because kernel is recovery mode.\n");
		return rc;
#endif
  }
 //printk("irtp_setup: dual touch enablet\n");
  rc = irtp_enable_mt(irtp);
  if ( rc < 0 ) {
		printk("Error fail to send enabel dual touch command!!!\n");
		if( set_flag )  {	//In the case of Set.
			iric_reboot(irtp);
			goto error_retry;
		}else{
			return rc;
		}
  }
  //printk("irtp_setup: tp setup param\n");
  rc = irtp_setup_parameter(irtp,/* IRTP_PRM_SETRES*/IRTP_PRM_ALL);
  if ( rc < 0 ) {
		if( set_flag )  {	//In the case of Set.
			iric_reboot(irtp);
			goto error_retry;
		}else{
			return rc;
		}
  }
  //printk("irtp_setup: touch start\n");
  rc = irtp_touchstart(irtp);
  if ( rc < 0 ) {
		printk("Error fail to send touch start command!!!\n");
		if( set_flag )  {	//In the case of Set.
			iric_reboot(irtp);
			goto error_retry;
		}else{
			return rc;
		}
  }

  irtp->state = IRTP_RUN;

  return 0;
}
int irtp_setup_subcpu(struct irtp *irtp) 
{
  int rc;
  //struct ktermios ts={0};
  //struct ktermios old={0};
  int baud = 115200;
  int bits = 8;
  int parity = 'n';
  int flow = 'n';

  if ( irtp->state == IRTP_RUN) {
    return 0;
  }
#if 0
  ts.c_cflag = CREAD | HUPCL | CLOCAL | CS8 | B115200;
 
  rc = mxc_ssw_set_termios(irtp->slot, &ts,&old);
  if ( rc < 0 ) {
    printk("irtp_setup:set termios error %d\n",rc);
    return rc;
  }
#else
  rc = mxc_ssw_set_options(irtp->slot,NULL,baud, parity, bits, flow);
  if ( rc < 0 ) {
    printk("irtp_setup:set termios error %d\n",rc);
    return rc;
  }
#endif
  irtp_cts_off(irtp);	//CTS HI
  //printk("irtp_setup: tp activate\n");
  rc = irtp_activate(irtp);
  if ( rc < 0 ) {
    return rc;
  }
  //printk("irtp_setup: tp setup param\n");
  rc = irtp_setup_parameter(irtp,IRTP_PRM_SETRES);
  if ( rc < 0 ) {
    return rc;
  }
  //printk("irtp_setup: touch start\n");
  rc = irtp_touchstart(irtp);
  if ( rc < 0 ) {
    return rc;
  }
  irtp->state = IRTP_RUN;
  return 0;
}

/*
 * down touchpanel 
 */
int irtp_down(struct irtp *irtp)
{
  //int rc;
#if 0 /* delete for power down 2011/03/23*/
  rc = irtp_deactive(irtp);
  if ( rc < 0 ) {
    return rc;
  }
#endif
  //mxc_ssw_shutdown(irtp->slot);
  return 0;
}

#ifdef CONFIG_EARLYSUSPEND
/* 
 * irtp_suspend: RUN suspend 
 */
static void irtp_early_suspend(struct early_suspend *h)
{
	struct irtp *irtp=container_of(h,struct irtp, earlysuspend);
  
	mutex_lock(&irtp->recovery_lock);
	if (h->pm_mode == EARLY_SUSPEND_MODE_NORMAL) {
		if ( irtp->type == 0 ) {
			del_timer(&irtp->recovertimer);
			mxc_ssw_port_close(irtp->slot);
			irtp->poweroff(irtp);
			wake_unlock(&irtp->receiving_lock);
			wake_unlock(&irtp->recovery_wake_lock);
			irtp_send_dumy_up_event();
		}
		irtp_cts_on(irtp);	//CTS LOW
	} else {
		mxc_ssw_enable_irq_wakeup(irtp->slot);
	}
	mutex_unlock(&irtp->recovery_lock);
}
/* 
 * irtp_resume: resume  toucpanel
 */
static void irtp_late_resume(struct early_suspend *h)
{
  struct irtp *irtp=container_of(h,struct irtp, earlysuspend);
  
  //printk("IRTP:late resume(mode=%d)\n",h->pm_mode);
  if (h->pm_mode == EARLY_SUSPEND_MODE_NORMAL) {
    //printk("IRTP:late resume\n");
    if ( irtp->type == 0 ) {
      irtp->poweron(irtp);
    }
    irtp_cts_off(irtp);	//CTS HI
    if ( irtp->type == 0 ) {
      mxc_ssw_port_open(irtp->slot);
      irtp->init(irtp);
    }
  } else {
    mxc_ssw_disable_irq_wakeup(irtp->slot);
  }
}
#else
/* 
 * irtp_suspend: RUN suspend 
 */
static void irtp_suspend(void *arg)
{
  struct irtp *irtp=(struct irtp*)arg;
  
  if ( irtp->state == IRTP_RUN ) {
    printk("IRTP:suspend\n");
    irtp->deinit(irtp);
#if 1 /* E_BOOK *//* for Low power 2011/3/2 */
    if ( irtp->type == 0 ) {
      mxc_ssw_port_close(irtp->slot);
    }
#endif
    irtp->poweroff(irtp);
    irtp_cts_on(irtp);	//CTS LOW
  }
}
/* 
 * irtp_resume: resume  toucpanel
 */
static void irtp_resume(void *arg)
{
  struct irtp *irtp=(struct irtp*)arg;
  
  if ( irtp->state == IRTP_RUN ) {
    printk("IRTP:resume\n");
    irtp->poweron(irtp);
    irtp_cts_off(irtp);	//CTS HI
#if 1 /* E_BOOK *//* for Low power 2011/3/2 */
    if ( irtp->type == 0 ) {
      mxc_ssw_port_open(irtp->slot);
    }
#endif
    irtp->init(irtp);
  }
}
#endif

/*
 * irtp_disconnect() 
 */
static void irtp_disconnect(void *arg)
{
  	struct irtp *irtp=(struct irtp*)arg;

	input_get_device(irtp->dev);
	input_unregister_device(irtp->dev);
	input_put_device(irtp->dev);

#if 1 /* E_BOOK *//* bugfix 2011/06/24 */
	wake_lock_destroy(&irtp->touchlock);
#endif
	
	/* For Recovery */
	wake_lock_destroy(&irtp->receiving_lock);
	wake_lock_destroy(&irtp->recovery_wake_lock);
	
	irtp->deinit(irtp);
	irtp->poweroff(irtp);
	irtp->state = IRTP_NONE;
}

/*
 * irtp_connect() 
 */

static int irtp_connect(void *arg )
{
  	struct irtp *irtp=(struct irtp*)arg;
	struct input_dev *input_dev;
	int err;

#if 1 /* for DEBUG */
	printk("irtp_connect\n");
#endif
	/* */
	init_waitqueue_head(&irtp->wait);
	sema_init(&irtp->commlock,1);
	
	/* initialize wakelock for touch *//* 2011/06/07 */
	wake_lock_init(&irtp->touchlock,WAKE_LOCK_SUSPEND,irtp->name);
	
	/* For Recovery */
	if ( irtp->type == 0 ) {
		wake_lock_init(&irtp->receiving_lock, WAKE_LOCK_SUSPEND, "receiving_lock_IR_TP");
		wake_lock_init(&irtp->recovery_wake_lock, WAKE_LOCK_SUSPEND, "recovery_wake_lock_IR_TP");
	}else{
		wake_lock_init(&irtp->receiving_lock, WAKE_LOCK_SUSPEND, "receiving_lock_SUB_TP");
		wake_lock_init(&irtp->recovery_wake_lock, WAKE_LOCK_SUSPEND, "recovery_wake_lock_SUB_TP");
	}
	mutex_init(&irtp->recovery_lock);
	setup_timer(&irtp->recovertimer, irtp_recover, (unsigned long)irtp); /* for recovery */
	INIT_WORK(&irtp->recoverwork, irtp_runrecover); /* for recovery */

	//mxc_ssw_set_receive_buffer(irtp->slot,irtp->data,IRTP_MAX_LENGTH);
	/* enable Touchpanel */
	err = irtp->poweron(irtp);
	err = irtp->init(irtp);
	if ( err ) {
	  printk("sonyirtp: init error(%d)\n",err);
	  return err;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
	  printk("sonyirtp: input alloc error\n");
	  err = -ENOMEM;
	  goto fail1;
	}
	
	if(irtp->type == 0){	//Touch panel type is IR
		ir_tp_input_dev = input_dev;
	}

	irtp->dev = input_dev;
	snprintf(irtp->phys, sizeof(irtp->phys), "%s/input%d", "irtp",irtp->slot);

	input_dev->name = irtp->name;
	input_dev->phys = irtp->phys;
	input_dev->id.bustype = BUS_RS232;
	input_dev->id.vendor = 0;
	input_dev->id.product = 0;
	input_dev->id.version = 0x0001;
	//input_dev->dev.parent = &serio->dev;
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
#if 1
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	__set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	//__set_bit(ABS_MT_TOUCH_MINOR, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
#else
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH|BTN_TOOL_FINGER);
#endif
	input_set_abs_params(irtp->dev, ABS_X, IRTP_MIN_YC, IRTP_MAX_YC, 0, 0);
	input_set_abs_params(irtp->dev, ABS_Y, IRTP_MIN_XC, IRTP_MAX_XC, 0, 0);
	input_set_abs_params(irtp->dev, ABS_MT_TOUCH_MAJOR, 0, 31, 0, 0);
	//input_set_abs_params(irtp->dev, ABS_MT_TOUCH_MINOR, IRTP_MIN_YC, IRTP_MAX_YC, 0, 0);
	//input_set_abs_params(irtp->dev, ABS_MT_ORIENTATION, IRTP_MIN_YC, IRTP_MAX_YC, 0, 0);
	//input_set_abs_params(irtp->dev, ABS_MT_POSITION_Y, IRTP_MIN_YC, IRTP_MAX_YC, 0, 0);
	//input_set_abs_params(irtp->dev, ABS_MT_POSITION_X, IRTP_MIN_XC, IRTP_MAX_XC, 0, 0);

	err = input_register_device(irtp->dev);
	if (err) {
	  printk("sonyirtp: input register error %d\n",err);
	  goto fail3;
	}
#if 1 /* E_BOOK *//* for diag 2011/1/25 */
        /* register sysfs */
        {
          int res;
          res =  sysfs_create_file(&input_dev->dev.kobj, &dev_attr_touchdata.attr);
          if ( res < 0 ) {
            printk("sonyirtp:sysfs create error(ret=%d)\n",res);
          }
        }

#endif
	/* send parameter */
	return 0;

 fail3:	
	input_free_device(input_dev);
 fail1:
	irtp->deinit(irtp);
	irtp->poweroff(irtp);
	wake_lock_destroy(&irtp->touchlock);
	wake_lock_destroy(&irtp->receiving_lock);
	wake_lock_destroy(&irtp->recovery_wake_lock);
#if 1 /* for DEBUG */
	printk("irtp_connect return %d\n",err);
#endif
	return err;
}

/*----------------------------------------- 
 * sysfiles 
 -----------------------------------------*/
static ssize_t irtp_resolution_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  snprintf(buf,PAGE_SIZE, "%dx%d\n",irtp_resolution_x,irtp_resolution_y);
  return strlen(buf);
}
static ssize_t irtp_resolution_set(struct device *dev, struct device_attribute *attr, char *buf,size_t size)
{
  int x,y;
  struct irtp *irtp;

  irtp = (struct irtp *)(dev->platform_data);
  sscanf(buf,"%dx%d",&x,&y);
  irtp_resolution_x=x;
  irtp_resolution_y=y;
  irtp_setup_parameter(irtp,IRTP_PRM_SETRES);
  return strlen(buf);
}
static DEVICE_ATTR(resolution, 0644, irtp_resolution_show, irtp_resolution_set);

static ssize_t irtp_scan_pen_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  snprintf(buf,PAGE_SIZE, "%d\n",irtp_scan_pen);
  return strlen(buf);
}
static ssize_t irtp_scan_pen_set(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct irtp *irtp;

  irtp = (struct irtp *)(dev->platform_data);
  irtp_scan_pen = simple_strtol(buf,NULL,0);
  irtp_setup_parameter(irtp,IRTP_PRM_SCAN_PEN);
  return strlen(buf);
}
static DEVICE_ATTR(scan_pen, 0644, irtp_scan_pen_show, irtp_scan_pen_set);

static ssize_t irtp_scan_finger_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  snprintf(buf,PAGE_SIZE, "%d\n",irtp_scan_finger);
  return strlen(buf);
}
static ssize_t irtp_scan_finger_set(struct device *dev, struct device_attribute *attr, char *buf, size_t size)
{
  struct irtp *irtp;

  irtp = (struct irtp *)(dev->platform_data);
  irtp_scan_finger = simple_strtol(buf,NULL,0);
  irtp_setup_parameter(irtp,IRTP_PRM_SCAN_FINGER);
  return strlen(buf);
}
static DEVICE_ATTR(scan_finger, 0644, irtp_scan_finger_show, irtp_scan_finger_set);

static ssize_t irtp_scan_idle_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  snprintf(buf,PAGE_SIZE, "%d\n",irtp_scan_idle);
  return strlen(buf);
}
static ssize_t irtp_scan_idle_set(struct device *dev, struct device_attribute *attr, char *buf, size_t size)
{
  struct irtp *irtp;

  irtp = (struct irtp *)(dev->platform_data);
  irtp_scan_idle = simple_strtol(buf,NULL,0);
  irtp_setup_parameter(irtp,IRTP_PRM_SCAN_IDLE);
  return strlen(buf);
}
static DEVICE_ATTR(scan_idle, 0644, irtp_scan_idle_show, irtp_scan_idle_set);

static ssize_t irtp_max_touch_area_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  snprintf(buf,PAGE_SIZE, "%d\n",irtp_max_touch_area);
  return strlen(buf);
}
static ssize_t irtp_max_touch_area_set(struct device *dev, struct device_attribute *attr, char *buf, size_t size)
{
  struct irtp *irtp;

  irtp = (struct irtp *)(dev->platform_data);
  irtp_max_touch_area = simple_strtol(buf,NULL,0);
  irtp_setup_parameter(irtp,IRTP_PRM_TOUCHAREA);
  return strlen(buf);
}
static DEVICE_ATTR(max_touch_area, 0644, irtp_max_touch_area_show, irtp_max_touch_area_set);

#if 1 /* E_BOOK *//* Add IR F/W Update mode 2011/03/11 */
static int fwupdate_flag=0;
#endif
static ssize_t irtp_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct irtp *irtp;
  int ret ;
  
  irtp = (struct irtp *)(dev->platform_data);
  ret = mxc_ssw_is_serialmode(irtp->slot);
  if ( ret < 0 ) {
    snprintf(buf,PAGE_SIZE,"unknown\n");
  }else if ( ret == 1 ) {
#if 1 /* E_BOOK *//* Add IR F/W Update mode 2011/03/11 */
    if ( fwupdate_flag ) {
      snprintf(buf,PAGE_SIZE,"updating\n");
    } else {
      snprintf(buf,PAGE_SIZE,"com\n");
    }
#else
    snprintf(buf,PAGE_SIZE,"com\n");
#endif
  } else {
    ret = mxc_ssw_is_inputdevmode(irtp->slot);
    if ( ret < 0 ) {
      snprintf(buf,PAGE_SIZE,"unknown\n");
    } else if ( ret == 1 ) {
      snprintf(buf,PAGE_SIZE,"input\n");
    } else {
      snprintf(buf,PAGE_SIZE,"none\n");
    }
  } 
  return strlen(buf);
}
static ssize_t irtp_mode_set(struct device *dev, struct device_attribute *attr, char *buf, size_t size)
{
  struct irtp *irtp;
  int slot;
  
#if 0 /* E_BOOK *//* for DEBUG */
  printk("irtp_mode_set: %s\n",buf);
#endif
  irtp = (struct irtp *)(dev->platform_data);
  slot = irtp->slot;
  if ( strcmp(buf,"input") == 0){
#if 1 /* E_BOOK *//* Add IR F/W Update mode 2011/03/11 */
    if ( fwupdate_flag==1) {
      if ( irtp->exit_fwupdate ) {
	fwupdate_flag=0;
	irtp->exit_fwupdate(irtp);
      }
    }
#endif
    if ( mxc_ssw_is_serialmode(slot) ){
      mxc_ssw_inputdevmode(slot);
      mxc_ssw_startup(slot);
      irtp->init(irtp);
    }
  } else if ( strcmp(buf,"com") == 0){
#if 1 /* E_BOOK *//* Add IR F/W Update mode 2011/03/11 */
    if ( fwupdate_flag==1) {
      if ( irtp->exit_fwupdate ) {
	fwupdate_flag=0;
	irtp->exit_fwupdate(irtp);
      }
    }
#endif
    if ( mxc_ssw_is_inputdevmode(slot) ){
      irtp->deinit(irtp);
      mxc_ssw_shutdown(irtp->slot);
      mxc_ssw_serialmode(slot);
    }
  } if (strcmp(buf,"input\n") == 0){
#if 1 /* E_BOOK *//* Add IR F/W Update mode 2011/03/11 */
    if ( fwupdate_flag==1) {
      if ( irtp->exit_fwupdate ) {
	fwupdate_flag=0;
	irtp->exit_fwupdate(irtp);
      }
    }
#endif
    if ( mxc_ssw_is_serialmode(slot) ){
      mxc_ssw_inputdevmode(slot);
      mxc_ssw_startup(slot);
      irtp->init(irtp);
    }
  } else if ( strcmp(buf,"com\n") == 0){
#if 1 /* E_BOOK *//* Add IR F/W Update mode 2011/03/11 */
    if ( fwupdate_flag==1) {
      if ( irtp->exit_fwupdate ) {
	fwupdate_flag=0;
	irtp->exit_fwupdate(irtp);
      }
    }
#endif
    if ( mxc_ssw_is_inputdevmode(slot) ){
      irtp->deinit(irtp);
      mxc_ssw_shutdown(irtp->slot);
      mxc_ssw_serialmode(slot);
    }
  }
#if 1 /* E_BOOK *//* Add IR F/W Update mode 2011/03/11 */
  else if ( strcmp(buf,"fwupdate") == 0 ||   
	    ( strcmp(buf,"fwupdate\n") == 0 ) ){
    if ( mxc_ssw_is_inputdevmode(slot) ){
      irtp->deinit(irtp);
      mxc_ssw_shutdown(irtp->slot);
      mxc_ssw_serialmode(slot);
    }
    if ( irtp->fwupdate ) {
      irtp->fwupdate(irtp);
      fwupdate_flag=1;
    }
  } else if ( strcmp(buf,"fwdone") == 0 ||   
	    ( strcmp(buf,"fwdone\n") == 0 ) ){
    if ( irtp->exit_fwupdate ) {
      fwupdate_flag=0;
      irtp->exit_fwupdate(irtp);
    }
  }    
#endif
  return strlen(buf);
}
static DEVICE_ATTR(mode,0644, irtp_mode_show, irtp_mode_set);

int irtp_make_sysfiles(struct platform_device *pdev)
{
  int res;
  res =  sysfs_create_file(&pdev->dev.kobj, &dev_attr_resolution.attr);
  if ( res < 0 ) {
    printk("IRTP:sysfs create error(ret=%d)\n",res);
  }
  res =  sysfs_create_file(&pdev->dev.kobj, &dev_attr_scan_pen.attr);
  if ( res < 0 ) {
    printk("IRTP:sysfs create error(ret=%d)\n",res);
  }
  res =  sysfs_create_file(&pdev->dev.kobj, &dev_attr_scan_finger.attr);
  if ( res < 0 ) {
    printk("IRTP:sysfs create error(ret=%d)\n",res);
  }
  res =  sysfs_create_file(&pdev->dev.kobj, &dev_attr_scan_idle.attr);
  if ( res < 0 ) {
    printk("IRTP:sysfs create error(ret=%d)\n",res);
  }
  res =  sysfs_create_file(&pdev->dev.kobj, &dev_attr_max_touch_area.attr);
  if ( res < 0 ) {
    printk("IRTP:sysfs create error(ret=%d)\n",res);
  }
  res =  sysfs_create_file(&pdev->dev.kobj, &dev_attr_mode.attr);
  if ( res < 0 ) {
    printk("IRTP:sysfs create error(ret=%d)\n",res);
  }
  return 0;
}


/*----------------------------------------- 
 -----------------------------------------*/
/*
 * driver structure.
 */
static struct mxc_ssw_recept tp_ir_drv = {
	.connect	= irtp_connect,
	.disconnect	= irtp_disconnect,
	.recv		= irtp_recv,
#ifdef CONFIG_EARLYSUSPEND
	.suspend	= NULL,
	.resume		= NULL,
#else
	.suspend	= irtp_suspend,
	.resume		= irtp_resume,
#endif
};

static struct mxc_ssw_recept tp_subcpu_drv = {
	.connect	= irtp_connect,
	.disconnect	= irtp_disconnect,
	.recv		= irtp_recv,
	.suspend	= NULL,
	.resume		= NULL,
};

static struct irtp irtp_drv_desc[] = {
  {
    .name 	= "SONY IR Touchpanel",
    .slot 	= 1,
    .poweron 	= irtp_power_on,
    .poweroff 	= irtp_power_off,
    .init 	= irtp_setup,
    .deinit 	= irtp_down,
    .recept 	= &tp_ir_drv,
    .fwupdate 	= irtp_fwupdate,
    .exit_fwupdate 	= irtp_exit_fwmode,
    .type	= 0,
    .state	= IRTP_NONE,
#ifdef CONFIG_EARLYSUSPEND
    .earlysuspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = irtp_early_suspend,
	.resume = irtp_late_resume,
    }
#endif
  },
  {
    .name 	= "SUBCPU Touchpanel",
    .slot 	= 0,
    .poweron 	= subcpu_power_on,
    .poweroff 	= subcpu_power_off,
    .init 	= irtp_setup_subcpu,
    .deinit 	= irtp_down,
    .recept 	= &tp_subcpu_drv,
    .type	= 1,
    .state	= IRTP_NONE,
#ifdef CONFIG_EARLYSUSPEND
    .earlysuspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = irtp_early_suspend,
	.resume = irtp_late_resume,
    }
#endif
  }
};

/* 
 * Probing IR Touchpanel
 */
int irtp_probe(struct platform_device *pdev)
{
  int slot = pdev->id;
  int rc;
  pdev->dev.platform_data = (void*)&irtp_drv_desc[slot];
  /* make sys files */
  irtp_make_sysfiles(pdev);
  /* register to switcher */
  rc = mxc_ssw_recept_register(irtp_drv_desc[slot].recept,
			       (void*)&irtp_drv_desc[slot],
			       irtp_drv_desc[slot].slot);
  if ( rc < 0 ) {
    printk("IRTP: %s ssw register error %d\n",irtp_drv_desc[slot].name,-rc);
    return rc;
  }
#ifdef CONFIG_EARLYSUSPEND
  printk("IRTP: register early_suspend\n"); /* for DEBUG */
  register_early_suspend(&irtp_drv_desc[slot].earlysuspend);
#endif  
  return rc;
}

/* 
 * Removing IR Touchpanel
 */
int irtp_remove(struct platform_device *pdev)
{
  struct irtp *irtp;
  int rc;
  
  irtp = (struct irtp *)(pdev->dev.platform_data);
 
#ifdef CONFIG_EARLYSUSPEND
  unregister_early_suspend(&(irtp->earlysuspend));
#endif  
  rc = mxc_ssw_recept_unregister(irtp->slot);
  if ( rc < 0 ) {
    printk("IRTP: %s ssw unregister error %d\n",irtp->name,-rc);
  }
  return 0;
}

struct platform_driver irtp_driver = {
	.driver = {
		   .name = "sonyirtp",
		   },
	.probe = irtp_probe,
	.remove = irtp_remove,
};
/*
 * The functions for inserting/removing us as a module.
 */
static int __init irtp_init(void)
{
#if 1 /* for DEBUG */
  printk("irtp_init\n");
#endif

  return platform_driver_register(&irtp_driver);
}

static void __exit irtp_exit(void)
{
#if 1 /* for DEBUG */
  printk("irtp_exit\n");
#endif
  platform_driver_unregister(&irtp_driver);
  return;
}

module_init(irtp_init);
module_exit(irtp_exit);

/*
 * kx122: tri-axis accelerometer
 *
 * Copyright(C) 2016 Kionix, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#include <linux/regulator/consumer.h>

// Disable QCOM_SENSORS
//#define QCOM_SENSORS
#ifdef QCOM_SENSORS
#include <linux/sensors.h>
#endif

#include "kx122.h"
#include "kx122_registers.h"
//#include "../keyboard/ntx_hwconfig.h"
#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"


//#define NO_FREE_IRQ		1
#define EN_THREAD_IRQ		1
/* Enable debug functionality */
#define DEBUG_SYSFS_ATTRIB
#define DEBUG_I2C_FAILS
#define DEBUG_FIFO
//#define DEBUG_XYZ_DATA
#define DEBUG_DT_PARAMS

#define __debug printk
//#define __debug pr_debug

/* poll defines */
#define POLL_MS_100HZ 10
#define MIN_POLL_RATE_MS 5
#define MAX_POLL_RATE_MS 1000

/* input dev names */
#define KX122_INPUT_DEV_NAME "kx122-accel"
#define KX112_INPUT_DEV_NAME "kx112-accel"

/* Driver state bits */
#define KX122_STATE_STANDBY 0
#define KX122_STATE_STRM BIT(0)
#define KX122_STATE_FIFO BIT(1)
#define KX122_STATE_WUFE BIT(2)

/* POWER SUPPLY VOLTAGE RANGE */
#define KX122_VDD_MIN_UV	1710000
#define KX122_VDD_MAX_UV	3600000
#define KX122_VIO_MIN_UV	1700000
#define KX122_VIO_MAX_UV	3600000

/* Power up and startup time */
#define KX122_POWER_UP_TIME_MS 50
#define KX122_START_UP_TIME_MS 25

/* Calibration ODR and sample count */
#define KX122_CAL_DATA_DELAY_MS 20
#define KX122_CAL_SAMPLE_COUNT 10

/* 1g raw values */
#define KX122_RANGE2G_1G_RAW 16384
#define KX122_RANGE4G_1G_RAW 8192
#define KX122_RANGE8G_1G_RAW 4092

struct kx122_data {
	struct i2c_client *client;
	struct kx122_platform_data pdata;
	struct mutex mutex;
	struct pinctrl*pinctrl;
	struct pinctrl_state *pin_int_default;
	struct pinctrl_state *pin_int_sleep;
	struct regulator *vdd;
	struct regulator *vio;
	bool power_enabled;

	/* accelerometer */
	struct input_dev *accel_input_dev;
	struct hrtimer accel_timer;
	int accel_wkp_flag;
	struct task_struct *accel_task;
	bool accel_delay_change;
	wait_queue_head_t accel_wq;
	u16 accel_poll_rate;

	int accel_cali[3];
#define KX122_CAL_BUF_SIZE 99
	char calibrate_buf[KX122_CAL_BUF_SIZE];

#ifdef QCOM_SENSORS
	struct sensors_classdev accel_cdev;
#endif

	int irq1;
	int irq2;

	/* sensor int1 pin control settings */
	u8 inc1;
	u8 inc4;
	u8 g_range;
	u8 data_res;
	u8 wai; 

	u8 _CNTL1;
	u8 _BUF_CNTL2;
	u16 state;
	u16 Tilt_LL;

	unsigned int max_latency;
	u16 fifo_wmi_threshold;

	ktime_t fifo_last_ts;
	ktime_t fifo_last_read;
	u16 sample_interval_ms;

	bool use_poll_timer;
	int enable_wake;// enable wakeup from suspending . 
	int enable_wake_state;// current wakeup enable/disable state . 
};

/* NOTE; 800,1600,3200,6400,12800,25600 odr not in table */
/* delay to odr map table */
static const struct {
	unsigned int cutoff;
	u8 mask;
} kx122_accel_odr_table[] = {
	{ 5, KX122_ODCNTL_OSA_400},
	{ 10, KX122_ODCNTL_OSA_200},
	{ 20, KX122_ODCNTL_OSA_100},
	{ 40, KX122_ODCNTL_OSA_50},
	{ 80, KX122_ODCNTL_OSA_25},
	{ 160, KX122_ODCNTL_OSA_12P5},
	{ 320, KX122_ODCNTL_OSA_6P25},
	{ 640, KX122_ODCNTL_OSA_3P125},
	{ 1280,	KX122_ODCNTL_OSA_1P563},
	{ 2560, KX122_ODCNTL_OSA_0P781},};

#define KX122_LAST_CUTOFF 2560

extern volatile NTX_HWCONFIG *gptHWCFG;
extern int gSleep_Mode_Suspend;
extern void ntx_report_event(unsigned int type, unsigned int code, int value);
static const char KX122_NAME[]	= "kx122";
static int gRotate_Angle=0;
struct i2c_client *g_kx122_data;
#ifdef QCOM_SENSORS
/* Qualcomm sensors class defines*/

#define POLL_INTERVAL_MIN_MS MIN_POLL_RATE_MS
#define POLL_DEFAULT_INTERVAL_MS 200

#define KX122_SENSOR_NAME "kx122"
#define KX112_SENSOR_NAME "kx112"
/* power is same for both 112 and 122 */
#define KX122_SENSOR_POWER "0.145"


/* 8g*9.806 m/s2 */
#define KX122_8G_RANGE "78.448"
/* 0.00024g*9.806 m/s2 */
#define KX122_8G_RESOLUTION "0.002353"
/* 4g*9.806 m/s2 */
#define KX122_4G_RANGE "39.224"
/* 0.00012g*9.806 m/s2 */
#define KX122_4G_RESOLUTION "0.001176"
/* 2g*9.806 m/s2 */
#define KX122_2G_RANGE "19.612"
/* 0.00006g*9.806 m/s2 */
#define KX122_2G_RESOLUTION "0.000588"

static struct sensors_classdev accel_sensors_cdev = {
	.name = KX122_SENSOR_NAME,
	.vendor = "Kionix",
	.version = 1,
	.handle = SENSORS_ACCELERATION_HANDLE,
	.type = SENSOR_TYPE_ACCELEROMETER,
	.max_range = KX122_4G_RANGE,
	.resolution = KX122_4G_RESOLUTION,
	.sensor_power = KX122_SENSOR_POWER,	/* typical value milliAmps*/
	.min_delay = POLL_INTERVAL_MIN_MS * 1000,/* in microseconds */
	.max_delay = MAX_POLL_RATE_MS,/* in milliseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = POLL_DEFAULT_INTERVAL_MS,	/* in millisecond */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

#endif

/* State update functions */
static int kx122_state_update(struct kx122_data *sdata, u16 state, u16 enable);
static int kx122_state_enable(struct kx122_data *sdata, u16 state);
static int kx122_state_disable(struct kx122_data *sdata, u16 state);

/* Enable/disable sensor feature functions */
static int kx122_strm_enable(struct kx122_data *sdata);
static int kx122_strm_disable(struct kx122_data *sdata);
static int kx122_strm_report_data(struct kx122_data *sdata);
static int kx122_fifo_enable(struct kx122_data *sdata);
static int kx122_fifo_disable(struct kx122_data *sdata);
static int kx122_fifo_report_data(struct kx122_data *sdata);

/* Configure sensor feature functions */
static int kx122_data_delay_set(struct kx122_data *sdata, unsigned long delay);
static int kx122_data_grange_set(struct kx122_data *sdata, u8 range);
static int kx122_data_res_set(struct kx122_data *sdata, u8 res);

static int kx122_sensor_init_config(struct kx122_data *sdata);
static int kx122_data_read_xyz(struct kx122_data *sdata, int *xyz);

/* Interrupt handling */
static int kx122_interrupt_enable(struct kx122_data *sdata, u8 bits, int irq);
static int kx122_interrupt_disable(struct kx122_data *sdata, u8 bits, int irq);

/* Regulator usage */
#if POWER_CONTROL
static int kx122_power_on(struct kx122_data *sdata, bool on);
static int kx122_power_init(struct kx122_data *sdata, bool on);
#endif

/* Timer based data poll thread */
static int kx122_data_strm_thread(void *data);

/* Input dev functions */
static int kx122_input_dev_register(struct kx122_data *sdata);
static void kx122_input_dev_cleanup(struct kx122_data *sdata);
static void kx122_input_report_xyz(struct input_dev *dev, int *xyz, ktime_t ts);


static struct delayed_work kx122_delay_work;
static irqreturn_t kx122_interrupt(int irq, void *dev_id)
{
	//printk(KERN_ERR"==== [%s_%d] ====\n",__FUNCTION__,__LINE__);
	schedule_delayed_work(&kx122_delay_work, 0);
	return IRQ_HANDLED;
}

/* Sensor register read/write functions */
static s32 kx122_reg_write_byte(struct kx122_data *sdata, u8 reg, u8 value)
{
	s32 err;

	err = i2c_smbus_write_byte_data(sdata->client, reg, value);

#ifdef DEBUG_I2C_FAILS
	if(err < 0) {
		__debug("%s fail (%d) : reg 0x%x, value 0x%x \n",
			__FUNCTION__, err, reg, value);
	}
#endif

	return err;
}
static s32 kx122_reg_read_byte(struct kx122_data *sdata, u8 reg)
{
	s32 reg_val;

	reg_val = i2c_smbus_read_byte_data(sdata->client, reg);

#ifdef DEBUG_I2C_FAILS
	if(reg_val < 0) {
		__debug("%s fail (%d) : reg 0x%x \n", __FUNCTION__,
			reg_val, reg);
	}
#endif

	return reg_val;
}

static s32 kx122_reg_read_block(struct kx122_data *sdata, u8 reg, u8 size, u8 *data)
{
	s32 err;

	err = i2c_smbus_read_i2c_block_data(sdata->client, reg,
		size, data);

#ifdef DEBUG_I2C_FAILS
	if (err != size) {
		__debug("%s fail : reg 0x%x bytes %d != %d \n", __FUNCTION__,
			reg, size, err);
	}
#endif
	if (err != size)
		err = -EIO;
	else
		err = 0;

	return err;
}

static int kx122_reg_set_bit(struct kx122_data *sdata, u8 reg, u8 bits)
{
	s32 reg_val;

	reg_val = kx122_reg_read_byte(sdata, reg);

	if (reg_val < 0)
		return reg_val;

	reg_val |= bits;

	reg_val = kx122_reg_write_byte(sdata, reg, reg_val);

	if (reg_val < 0)
		return reg_val;

	return 0;
}

static int kx122_reg_reset_bit(struct kx122_data *sdata,
		u8 reg, u8 bits)
{
	s32 reg_val;

	reg_val = kx122_reg_read_byte(sdata, reg);

	if (reg_val < 0)
		return reg_val;

	reg_val &= ~bits;

	reg_val = kx122_reg_write_byte(sdata, reg, reg_val);

	if (reg_val < 0)
		return reg_val;

	return 0;
}

static int kx122_reg_set_bit_pattern(struct kx122_data *sdata,
		u8 reg, u8 bit_pattern, u8 mask)
{
	s32 reg_val;

	reg_val = kx122_reg_read_byte(sdata, reg);

	if (reg_val < 0)
		return reg_val;

	reg_val &= ~mask;
	reg_val |= bit_pattern;

	reg_val = kx122_reg_write_byte(sdata, reg, reg_val);

	if (reg_val < 0)
		return reg_val;

	return 0;
}

static s32 kx122_reg_read_fifo(struct kx122_data *sdata, u8 reg, u16 size, u8 *data)
{
	s32 err;

	struct i2c_msg msgs[] = {
		{
		 .addr = sdata->client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg,
		},
		{
		 .addr = sdata->client->addr,
		 .flags = I2C_M_RD,
		 .len = size,
		 .buf = data,
		 },
	};

	err = i2c_transfer(sdata->client->adapter, msgs, ARRAY_SIZE(msgs));

#ifdef DEBUG_I2C_FAILS
	if (ARRAY_SIZE(msgs) != err)
		__debug("%s fail (%d) : reg 0x%x \n", __FUNCTION__,err, reg);
#endif

	if (ARRAY_SIZE(msgs) == err)
		err = 0;
	else if (err >= 0)
		err = -EIO;

	return err;
}

static void kx122_work_func(struct work_struct *work)
{
	int s2_status,cur_pos;
	s2_status = kx122_reg_read_byte(g_kx122_data, KX122_INS2);	

	//printk(KERN_ERR"==== [%s_%d] ====\n",__FUNCTION__,__LINE__);
	if(s2_status & KX122_INS2_TPS)
	{
		cur_pos = kx122_reg_read_byte(g_kx122_data, KX122_TSCP);
		if(cur_pos & KX122_TSCP_FU )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Face Up] ====\n");
		}
		else if(cur_pos & KX122_TSCP_FD )
		{
			if(80 == gptHWCFG->m_val.bPCB)	
				printk(KERN_INFO"==== [Face Down] ====\n");
		}
		if(cur_pos & KX122_TSCP_UP )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Up State] ====\n");
		}
		else if(cur_pos & KX122_TSCP_DO )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Down State] ====\n");
		}
		else if(cur_pos & KX122_TSCP_RI )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Right State] ====\n");				
		}
		else if(cur_pos & KX122_TSCP_LE )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Left State] ====\n");
		}
		s2_status = kx122_reg_read_byte(g_kx122_data, KX122_INT_REL);	
	}
}

/* Sensor configure functions */
static int kx122_data_delay_set(struct kx122_data *sdata, unsigned long delay)
{
	int err = 0, i;
	u8 odr_bits;

	if (delay < MIN_POLL_RATE_MS) {
 		delay = MIN_POLL_RATE_MS;
	} else if(delay > MAX_POLL_RATE_MS) {
		delay = MAX_POLL_RATE_MS;
	}

	sdata->accel_poll_rate = delay;
	if (!sdata->power_enabled)
		return 0;
	/* map delay to odr */
	for (i = 0; i < ARRAY_SIZE(kx122_accel_odr_table); i++) {
		if (delay < kx122_accel_odr_table[i].cutoff ||
			kx122_accel_odr_table[i].cutoff == KX122_LAST_CUTOFF) {
			odr_bits = kx122_accel_odr_table[i].mask;
			break;
			}
	}

	/* odr to sample interval */
	if(i == 0)
		sdata->sample_interval_ms = kx122_accel_odr_table[i].cutoff;
	else
		sdata->sample_interval_ms = kx122_accel_odr_table[i-1].cutoff;

	__debug("%s - delay = %lu cutoff = %u mask = 0x%x, interval = ms %d\n",
		__FUNCTION__, delay, kx122_accel_odr_table[i].cutoff,
		odr_bits,
		sdata->sample_interval_ms );

	err = kx122_reg_set_bit_pattern(sdata, KX122_ODCNTL, odr_bits,
		KX122_ODCNTL_OSA_MASK);

	if (sdata->use_poll_timer && !err) {
		hrtimer_cancel(&sdata->accel_timer);
		sdata->accel_delay_change = true;
		sdata->accel_poll_rate = delay;
		if (sdata->state & KX122_STATE_STRM) {
			ktime_t time;
			time = ktime_set(0,sdata->accel_poll_rate * NSEC_PER_MSEC);
			hrtimer_start(&sdata->accel_timer, time, HRTIMER_MODE_REL);
		}
	}

	return err;
}


static int kx122_data_grange_set(struct kx122_data *sdata, u8 range)
{
	int err;
	u8 range_bits;
	
	if (range == KX122_ACC_RANGE_2G) {
		range_bits = KX122_CNTL1_GSEL_2G;
	}
	else if (range == KX122_ACC_RANGE_4G){
		range_bits = KX122_CNTL1_GSEL_4G;
	}
	else if (range == KX122_ACC_RANGE_8G){
		range_bits = KX122_CNTL1_GSEL_8G;
	}
	else {
		return -EINVAL;
	}

	err = kx122_reg_set_bit_pattern(sdata,
		KX122_CNTL1, range_bits, KX122_CNTL1_GSEL_MASK);

	return err;
}

static int kx122_data_res_set(struct kx122_data *sdata, u8 res)
{
	int err;

	if (res == 16)
		err = kx122_reg_set_bit(sdata, KX122_CNTL1, KX122_CNTL1_RES);
	else if (res == 8) 
		err = kx122_reg_reset_bit(sdata, KX122_CNTL1, KX122_CNTL1_RES);
	else 
		err = -EINVAL;

	return err;
}

/* Writes initial register config to sensor. */
static int kx122_sensor_init_config(struct kx122_data *sdata)
{
	int err;

	/* interrupt int1 control settings */
	err = kx122_reg_write_byte(sdata, KX122_INC1, sdata->inc1);
	if (err < 0)
		return err;

	err = kx122_data_delay_set(sdata, sdata->accel_poll_rate);
	if (err < 0)
		return err;

	err = kx122_data_grange_set(sdata, sdata->g_range);
	if (err < 0)
		return err;

	err = kx122_data_res_set(sdata, sdata->data_res);
	if (err < 0)
		return err;

	return 0;
}

#define KX122_SOFT_RESET_WAIT_TIME_MS 2
#define KX122_SOFT_RESET_TOTAL_WAIT_TIME_MS 100

static int kx122_soft_reset(struct kx122_data *sdata)
{
	int err;
	int max_wait_time = KX122_SOFT_RESET_TOTAL_WAIT_TIME_MS;

	/* reboot sensor */
	err = kx122_reg_write_byte(sdata, KX122_CNTL2, KX122_CNTL2_SRST);

	if (err < 0)
		return err;
	
	/* wait sensor reboot to finish */
	while (max_wait_time > 0) {

		mdelay(KX122_SOFT_RESET_WAIT_TIME_MS);
		max_wait_time -= KX122_SOFT_RESET_WAIT_TIME_MS;
		
		err = kx122_reg_read_byte(sdata, KX122_CNTL2);

		if (err < 0)
			return err;

		if (err & KX122_CNTL2_SRST) {
			/* reboot not ready */
			err = -ETIME;
		}
		else {
			/* reboot done */
			err = 0;
			break;
		}
	}

	return err;
}

static int kx122_hw_detect(struct kx122_data *sdata)
{
	int err;

	err = kx122_reg_read_byte(sdata, KX122_WHO_AM_I);
	if (err < 0)
		return err;

	__debug("%s WHO_AM_I_WIA_ID = 0x%x\n", __FUNCTION__, err);

	if (err != KX122_WHO_AM_I_WIA_ID && err != KX112_WHO_AM_I_WIA_ID)
		return -ENODEV;

	sdata->wai = err;

	dev_info(&sdata->client->dev, "Kionix kx122 detected\n");

	return 0;
}

static irqreturn_t kx122_irq_handler(int irq, void *dev)
{
	struct kx122_data *sdata = dev;
	//int s3_status;
	int s2_status , s1_status;
	int cur_pos;

	//s1_status = kx122_reg_read_byte(sdata, KX122_INS1);
	s2_status = kx122_reg_read_byte(sdata, KX122_INS2);
	//s3_status = kx122_reg_read_byte(sdata, KX122_INS3);

	if (s2_status <= 0) {
		return IRQ_HANDLED;
	}

	//printk(KERN_ERR"==== [%s_%d] KX122_INS2 :%d ====\n",__FUNCTION__,__LINE__,s2_status);
	if (s2_status & KX122_INS2_DRDY) {
		kx122_strm_report_data(sdata);
	}
	if (s2_status & KX122_INS2_WMI) {
		mutex_lock(&sdata->mutex);
		if (sdata->state & KX122_STATE_FIFO) {
			kx122_fifo_report_data(sdata);
		}
		mutex_unlock(&sdata->mutex);
	}
	if(s2_status & KX122_INS2_WUFS) {
		printk(KERN_ERR"==== Wake up ====\n");
		//s1_status = kx122_reg_read_byte(sdata, KX122_INT_REL);	
	}

	if(s2_status & KX122_INS2_TPS)
	{
		cur_pos = kx122_reg_read_byte(sdata, KX122_TSCP);
		if(cur_pos & KX122_TSCP_FU )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Face Up] ====\n");
			else if(75 == gptHWCFG->m_val.bPCB) {
				printk(KERN_INFO"==== [Face Up] ====\n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_FRONT);	
			}
			else if (87 == gptHWCFG->m_val.bPCB) {
				printk(KERN_INFO"==== [Face down] ====\n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_BACK);	
			}
		}
		else if(cur_pos & KX122_TSCP_FD )
		{
			if(80 == gptHWCFG->m_val.bPCB)	
				printk(KERN_INFO"==== [Face Down] ====\n");
			else if(75 == gptHWCFG->m_val.bPCB)	{
				printk(KERN_INFO"==== [Face Down] ====\n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_BACK);	
			}
			else if (87 == gptHWCFG->m_val.bPCB) {
				printk(KERN_INFO"==== [Face up] ====\n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_FRONT);	
			}
		}
		if(cur_pos & KX122_TSCP_UP )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Up State] ====\n");
			else if(75 == gptHWCFG->m_val.bPCB)	{
				printk(KERN_INFO"==== [Up State] ====\n");		
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_UP);	
			}
			else if(87 == gptHWCFG->m_val.bPCB)	{
				printk(KERN_INFO"==== [down State] ====\n");		
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_DOWN);	
			}
		}
		else if(cur_pos & KX122_TSCP_DO )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Down State] ====\n");
			else if(75 == gptHWCFG->m_val.bPCB){
				printk(KERN_INFO"==== [Down State] ====\n");	
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_DOWN);	
			}
			else if(87 == gptHWCFG->m_val.bPCB){
				printk(KERN_INFO"==== [up State] ====\n");	
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_UP);	
			}
		}
		else if(cur_pos & KX122_TSCP_RI )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Right State] ====\n");	
			else if(75 == gptHWCFG->m_val.bPCB)	{
				printk(KERN_INFO"==== [Right State] ====\n");	
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_RIGHT);	
			}
			else if(87 == gptHWCFG->m_val.bPCB)	{
				printk(KERN_INFO"==== [Left State] ====\n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_LEFT);
			}
		}
		else if(cur_pos & KX122_TSCP_LE )
		{
			if(80 == gptHWCFG->m_val.bPCB)
				printk(KERN_INFO"==== [Left State] ====\n");
			else if(75 == gptHWCFG->m_val.bPCB)	{
				printk(KERN_INFO"==== [Left State] ====\n");
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_LEFT);
			}
			else if(87 == gptHWCFG->m_val.bPCB)	{
				printk(KERN_INFO"==== [Right State] ====\n");	
				ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_RIGHT);	
			}
		}
	}
	s1_status = kx122_reg_read_byte(sdata, KX122_INT_REL);
	return IRQ_HANDLED;
}

/* Routes sensor interrupt to sensor physical pin based on irq number */
/* and enables irq and sensor interrupt. */
static int kx122_interrupt_enable(struct kx122_data *sdata, u8 bits, int irq)
{
	int err;
	s32 incx_val;
	u8 incx_routing_reg;
	u8 incx_ena_reg;

	if (!bits)
		return -EINVAL;

	/* irq to interrupt report register */
	if (sdata->irq1 == irq) {
		incx_routing_reg = KX122_INC4;
		incx_ena_reg =  KX122_INC1;
	}
	else if(sdata->irq2 == irq) {
		incx_routing_reg = KX122_INC6;
		incx_ena_reg =  KX122_INC5;
	}
	else {
		return -EINVAL;
	}

	incx_val = kx122_reg_read_byte(sdata, incx_routing_reg);

	if (incx_val < 0)
		return incx_val;

	/* interrupt routing to int1/int2 */
	err = kx122_reg_write_byte(sdata, incx_routing_reg, (incx_val|bits));

	if (err < 0)
		return err;

	if (!incx_val) {
		/* first interrupt routed to sensor int pin */
#ifdef CONFIG_SOC_IMX6SLL
		err = pinctrl_select_state(sdata->pinctrl, sdata->pin_int_default);
#else
		//
#endif
		if (err){
			printk(KERN_ERR"[%s_%d] pinctrl_select_state err:%d\n",__FUNCTION__,__LINE__,err);
			return err;
		}

		/* enable driver interrupt handler */
		enable_irq(irq);

		/* enable sensor int pin */
		err = kx122_reg_set_bit(sdata, incx_ena_reg, KX122_INC1_IEN1);
		if (err < 0)
			return err;

		__debug("%s - enable irq %d\n", __FUNCTION__, irq);
	}

	return 0;
}

static int kx122_interrupt_disable(struct kx122_data *sdata, u8 bits, int irq)
{

	int err;
	s32 incx_val;
	u8 incx_routing_reg;
	u8 incx_ena_reg;

	if (!bits)
		return -EINVAL;

	/* map irq to interrupt report register */
	if (sdata->irq1 == irq) {
		incx_routing_reg = KX122_INC4;
		incx_ena_reg = KX122_INC1;
	}
	else if(sdata->irq2 == irq) {
		incx_routing_reg = KX122_INC6;
		incx_ena_reg = KX122_INC5;
	}
	else {
		return -EINVAL;
	}

		incx_val = kx122_reg_read_byte(sdata, incx_routing_reg);

	if (incx_val < 0)
		return incx_val;

	incx_val &= ~bits;

	err = kx122_reg_write_byte(sdata, incx_routing_reg, incx_val);

	if (err < 0)
		return err;

	if (!incx_val) {
		/* last interrupt routed to sensor int pin */
#ifdef CONFIG_SOC_IMX6SLL
		err = pinctrl_select_state(sdata->pinctrl, sdata->pin_int_sleep);
#else

#endif
		if (err)
			return err;

		disable_irq_nosync(irq);

		err = kx122_reg_reset_bit(sdata, incx_ena_reg, KX122_INC1_IEN1);

		if (err < 0)
			return err;

		__debug("%s - disable irq %d\n", __FUNCTION__, irq);
	}

	return 0;
}

#if POWER_CONTROL
static int kx122_sensor_power_on(struct kx122_data *sdata)
{
	int err = 0;

	if (!sdata->power_enabled) {
		err = kx122_power_on(sdata, true);

		if (!err) {
			msleep(KX122_POWER_UP_TIME_MS);
			err = kx122_sensor_init_config(sdata);
		}
	}

	__debug("%s - power_enabled %d, err %d\n", __FUNCTION__, 
		sdata->power_enabled, err);

	return err;
}

static int kx122_sensor_power_off(struct kx122_data *sdata)
{
	int err = 0;

	if (sdata->power_enabled) {
		err = kx122_reg_reset_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);
		/* power off sensor even if PC1 set fails */
		err = kx122_power_on(sdata, false);
	}

	__debug("%s - power_enabled %d, err %d\n", __FUNCTION__, 
		sdata->power_enabled, err);

	return err;
}
#endif

/* Locks mutex, toggles sensor PC1 bit and updates state */
static int kx122_state_update(struct kx122_data *sdata, u16 state, u16 enable)
{
	int err = 0;

	mutex_lock(&sdata->mutex);

	if (enable)
	{
#if POWER_CONTROL
		err = kx122_sensor_power_on(sdata);
#endif
	}

	if (err)
		goto exit_out;

	/* set PC1 bit to zero before changes */
	err = kx122_reg_reset_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);

	if (err)
		goto exit_out;

	if (enable)
		err = kx122_state_enable(sdata, state);
	else
		err = kx122_state_disable(sdata, state);

	if (err)
		goto exit_out;

	/* apply changes set PC1 bit to one */
	err = kx122_reg_set_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);

exit_out:
	/* sensor to standby/off if no active features */
	if (sdata->state == KX122_STATE_STANDBY) {
#if POWER_CONTROL
		err = kx122_sensor_power_off(sdata);
#endif
	}

	mutex_unlock(&sdata->mutex);

	return err;
}

static int kx122_state_enable(struct kx122_data *sdata, u16 state)
{
	int err = 0;

	if (KX122_STATE_STRM == state || KX122_STATE_FIFO == state) {

		/* enable fifo if max_latency is set */
		if (sdata->max_latency > 0)
			err = kx122_fifo_enable(sdata);
		else
			err = kx122_strm_enable(sdata);
	}

	return err;
}

static int kx122_state_disable(struct kx122_data *sdata, u16 state)
{
	int err = 0;

	if (KX122_STATE_STRM == state || KX122_STATE_FIFO == state) {
		/* disable all data reportting */
		err = kx122_fifo_disable(sdata);
		err = kx122_strm_disable(sdata);
	}

	return err;
}

static int kx122_strm_enable(struct kx122_data *sdata)
{
	s32 err;

	if (sdata->state & KX122_STATE_STRM)
		return 0;

	/* set data reportting */
	if (sdata->use_poll_timer) {
		ktime_t time;
		time = ktime_set(0,sdata->accel_poll_rate * NSEC_PER_MSEC);
		hrtimer_start(&sdata->accel_timer, time, HRTIMER_MODE_REL);
	} else {
		/* NOTE, uses irq1 */
		err = kx122_interrupt_enable(sdata, KX122_INC4_DRDYI1, sdata->irq1);
		if (err < 0)
			return err;
		/* enable drdy interrupt */
		err = kx122_reg_set_bit(sdata, KX122_CNTL1, KX122_CNTL1_DRDYE);
		if (err < 0)
			return err;
	}

	sdata->state |= KX122_STATE_STRM;

	return 0;
}

static int kx122_strm_disable(struct kx122_data *sdata)
{

	if (!(sdata->state & KX122_STATE_STRM))
		return 0;

	/* disable data report */ 
	if (!sdata->use_poll_timer) {
		int err;

		err = kx122_interrupt_disable(sdata, KX122_INC4_DRDYI1, sdata->irq1);
		if (err < 0)
			return err;

		err = kx122_reg_reset_bit(sdata, KX122_CNTL1, KX122_CNTL1_DRDYE);
		if (err < 0)
			return err;
	}

	sdata->state &= ~KX122_STATE_STRM;

	return 0;
}

static int kx122_data_calibrate_xyz(struct kx122_data *sdata, int *xyz)
{
	xyz[0] -= sdata->accel_cali[0];
	xyz[1] -= sdata->accel_cali[1];
	xyz[2] -= sdata->accel_cali[2];

	return 0;
}

static int kx122_strm_report_data(struct kx122_data *sdata)
{
	int err;
	int xyz[3];
	ktime_t ts;

	err = kx122_data_read_xyz(sdata, xyz);

	if (err) {
		dev_err(&sdata->client->dev, "i2c read/write error\n");
	}
	else {
		ts = ktime_get_boottime();
		kx122_data_calibrate_xyz(sdata, xyz);
		kx122_input_report_xyz(sdata->accel_input_dev, xyz, ts);
	}

	return 0;
}

#define KX122_FIFO_SIZE_BYTES 2048
#define KX112_FIFO_SAMPLES_SIZE_BYTES 6
#define KX122_FIFO_MAX_SAMPLES (KX122_FIFO_SIZE_BYTES/KX112_FIFO_SAMPLES_SIZE_BYTES)
/* Proper WMI configuration depends on following */
/* - FIFO sample loss may occure if high ORD, low I2C clock, large FIFO wmi */
/*   SW workaroud is implemented to estimate FIFO sample loss. */
/* - kernel/Android HAL sample loss may occure if input dev buffer too small */

#define KX122_FIFO_MAX_WMI_TH 80
#define KX122_INPUT_DEV_EVENTS_NUM 120

static int kx122_fifo_calculate_wmi(struct kx122_data *sdata)
{
	u16 threshold;

	/* how many samples stored to FIFO before wmi */
	threshold = sdata->max_latency / sdata->sample_interval_ms;

	if (threshold > KX122_FIFO_MAX_WMI_TH)
		threshold = KX122_FIFO_MAX_WMI_TH;

#ifdef DEBUG_FIFO
	__debug("%s - max_latency %d, wmi time %d, wmi th %d\n",
	__FUNCTION__,
	sdata->max_latency,
	(threshold*sdata->sample_interval_ms),
	threshold);
#endif

	sdata->fifo_wmi_threshold = threshold;
	return 0;
}

static int kx122_fifo_enable(struct kx122_data *sdata)
{
	s32 err;
	u8 reg_val;

	if (sdata->state & KX122_STATE_FIFO)
		return 0;

	err = kx122_fifo_calculate_wmi(sdata);
	if (err < 0)
		return err;

	/* Set watermark threshold low bits */
	reg_val = 0x00ff & sdata->fifo_wmi_threshold;
	kx122_reg_write_byte(sdata, KX122_BUF_CNTL1, reg_val);

	/* Set watermark hibits and FIFO buffer on at data res 16bit */
	reg_val = 0x0C & (sdata->fifo_wmi_threshold >> 6);
	reg_val |= KX122_BUF_CNTL2_BUFE | KX122_BUF_CNTL2_BRES;
	kx122_reg_write_byte(sdata, KX122_BUF_CNTL2, reg_val);
	
	/* who am I must be read after fifo enable */	
	(void)kx122_reg_read_byte(sdata, KX122_WHO_AM_I);

	/* NOTE, uses irq1 */
	err = kx122_interrupt_enable(sdata, KX122_INC4_WMI1, sdata->irq1);
	if (err < 0)
		return err;

	/* timestamp when fifo started */
	sdata->fifo_last_ts = ktime_get_boottime();
	sdata->fifo_last_read =	sdata->fifo_last_ts;

	sdata->state |= KX122_STATE_FIFO;

	return 0;
}

static int kx122_fifo_disable(struct kx122_data *sdata)
{
	int err;

	if (!(sdata->state & KX122_STATE_FIFO))
		return 0;

	/* FIFO buffer off */
	kx122_reg_write_byte(sdata, KX122_BUF_CNTL2, 0);

	/* clear buffer */
	kx122_reg_write_byte(sdata, KX122_BUF_CLEAR, 0xff);

	/* who am I must be read after fifo disable */	
	(void)kx122_reg_read_byte(sdata, KX122_WHO_AM_I);

	/* NOTE, uses irq1 */
	err = kx122_interrupt_disable(sdata, KX122_INC4_WMI1, sdata->irq1);
	if (err < 0)
		return err;

	sdata->state &= ~KX122_STATE_FIFO;

	return 0;
}

static int kx122_convert_raw_data(struct kx122_data *sdata, int *xyz, s16 *raw_xyz)
{

	raw_xyz[0] = (s16) le16_to_cpu( raw_xyz[0] );
	raw_xyz[1] = (s16) le16_to_cpu( raw_xyz[1] );
	raw_xyz[2] = (s16) le16_to_cpu( raw_xyz[2] );

	xyz[0] = ((sdata->pdata.x_negate) ? (-raw_xyz[sdata->pdata.x_map])
		   : (raw_xyz[sdata->pdata.x_map]));
	xyz[1] = ((sdata->pdata.y_negate) ? (-raw_xyz[sdata->pdata.y_map])
		   : (raw_xyz[sdata->pdata.y_map]));
	xyz[2] = ((sdata->pdata.z_negate) ? (-raw_xyz[sdata->pdata.z_map])
		   : (raw_xyz[sdata->pdata.z_map]));

	return 0;
}


static int kx122_fifo_report_data(struct kx122_data *sdata)
{
	int err = 0;
	s16 raw_xyz[3*KX122_FIFO_MAX_WMI_TH] = {0};
	int xyz[3];
	int i;
	u16 fifo_bytes;
	u16 fifo_sample_set_count;
	ktime_t interval_ns;
	ktime_t fifo_endtime_ns;
	int lost_samples = 0;

	u16 fifo_time_ms;
	u16 delta_time_ms;


	/* get number of bytes in fifo */ 
	err = kx122_reg_read_block(sdata, KX122_BUF_STATUS_1, 2, (u8*)&fifo_bytes);

	if (err < 0)
		return err;
	/* fifo bytes info in lowest 10bits */
	fifo_bytes = le16_to_cpu(fifo_bytes);
	fifo_bytes = 0x07FF & fifo_bytes;
	fifo_sample_set_count = fifo_bytes/KX112_FIFO_SAMPLES_SIZE_BYTES;

	/* safety check fifo sample count */
	if (fifo_sample_set_count > KX122_FIFO_MAX_WMI_TH) {
		fifo_sample_set_count = KX122_FIFO_MAX_WMI_TH;
	}

#ifdef DEBUG_FIFO
	__debug("%s - wmi bytes %d, fifo bytes %d, fifo samples %d \n",
		__FUNCTION__,
		sdata->fifo_wmi_threshold*KX112_FIFO_SAMPLES_SIZE_BYTES,
		fifo_bytes,
		fifo_sample_set_count );
#endif
	/* New samples does not go to FIFO while host is reading data from FIFO */
	/* FIFO may loss samples if ODR is high and I2C clock is slow */

	/* delta time between two FIFO bursts */
	fifo_endtime_ns = ktime_get_boottime();
	delta_time_ms = ktime_to_ms(ktime_sub(fifo_endtime_ns, sdata->fifo_last_read));
	sdata->fifo_last_read = fifo_endtime_ns;

	/* total FIFO samples time */
	fifo_time_ms = fifo_sample_set_count * sdata->sample_interval_ms;

	/* possible losts samples */
	lost_samples = (delta_time_ms - fifo_time_ms) / sdata->sample_interval_ms;

#ifdef DEBUG_FIFO
	__debug("%s - fifo_time_ms %d, delta_time_ms %d, lost_samples %d\n",
		__FUNCTION__, fifo_time_ms, delta_time_ms, lost_samples);
#endif
	/* FIFO sample interval for timestamp */
	interval_ns = ktime_set(0, sdata->sample_interval_ms * NSEC_PER_MSEC);

	err = kx122_reg_read_fifo(sdata, KX122_BUF_READ, (6*fifo_sample_set_count), (u8*)raw_xyz);

	if (err < 0)
		return err;

	/* who am I must be read after fifo data read */	
	(void)kx122_reg_read_byte(sdata, KX122_WHO_AM_I);

	for (i = 0; i < lost_samples; i++) {
		sdata->fifo_last_ts = ktime_add(sdata->fifo_last_ts, interval_ns);
		kx122_convert_raw_data(sdata, xyz, &raw_xyz[3]);
		kx122_data_calibrate_xyz(sdata, xyz);
		kx122_input_report_xyz(sdata->accel_input_dev, xyz, sdata->fifo_last_ts);
	}

	for (i = 0; i < fifo_sample_set_count; i++) {
		sdata->fifo_last_ts = ktime_add(sdata->fifo_last_ts, interval_ns);
		kx122_convert_raw_data(sdata, xyz, &raw_xyz[i*3]);
		kx122_data_calibrate_xyz(sdata, xyz);
		kx122_input_report_xyz(sdata->accel_input_dev, xyz, sdata->fifo_last_ts);
	}

	return 0;
}


static int kx122_data_read_xyz(struct kx122_data *sdata, int *xyz)
{

	int err;
	s16 raw_xyz[3] = { 0 };

	err = kx122_reg_read_block(sdata, KX122_XOUT_L, 6, (u8*)raw_xyz);

	if (err)
		return err;

	raw_xyz[0] = (s16) le16_to_cpu( raw_xyz[0] );
	raw_xyz[1] = (s16) le16_to_cpu( raw_xyz[1] );
	raw_xyz[2] = (s16) le16_to_cpu( raw_xyz[2] );

	xyz[0] = ((sdata->pdata.x_negate) ? (-raw_xyz[sdata->pdata.x_map])
		   : (raw_xyz[sdata->pdata.x_map]));
	xyz[1] = ((sdata->pdata.y_negate) ? (-raw_xyz[sdata->pdata.y_map])
		   : (raw_xyz[sdata->pdata.y_map]));
	xyz[2] = ((sdata->pdata.z_negate) ? (-raw_xyz[sdata->pdata.z_map])
		   : (raw_xyz[sdata->pdata.z_map]));

	return 0;
}

/* input dev functions */
static int kx122_input_dev_register(struct kx122_data *sdata)
{
	int err;

	sdata->accel_input_dev = input_allocate_device();
	if (!sdata->accel_input_dev)
		return -ENOMEM;

	if (sdata->wai == KX112_WHO_AM_I_WIA_ID)
		sdata->accel_input_dev->name = KX112_INPUT_DEV_NAME;
	else
		sdata->accel_input_dev->name = KX122_INPUT_DEV_NAME;

	sdata->accel_input_dev->id.bustype = BUS_I2C;
	sdata->accel_input_dev->id.vendor  = sdata->wai;

	sdata->accel_input_dev->dev.parent = &sdata->client->dev;

	set_bit(EV_ABS, sdata->accel_input_dev->evbit);
	input_set_abs_params(sdata->accel_input_dev, ABS_X, INT_MIN, INT_MAX,0,0);
	input_set_abs_params(sdata->accel_input_dev, ABS_Y, INT_MIN, INT_MAX,0,0);
	input_set_abs_params(sdata->accel_input_dev, ABS_Z, INT_MIN, INT_MAX,0,0);

	/* increase buffer size to avoid input dev FIFO data loss */
	input_set_events_per_packet(sdata->accel_input_dev, KX122_INPUT_DEV_EVENTS_NUM);

	input_set_drvdata(sdata->accel_input_dev, sdata);

	err = input_register_device(sdata->accel_input_dev);

	if (err) {
		input_free_device(sdata->accel_input_dev);
		sdata->accel_input_dev = NULL;
		return err;
	}

	return 0;
}

static void kx122_input_dev_cleanup(struct kx122_data *sdata)
{
	input_unregister_device(sdata->accel_input_dev);
}

#define SYN_TIME_SEC		4
#define SYN_TIME_NSEC		5
static void kx122_input_report_xyz(struct input_dev *dev, int *xyz, ktime_t ts)
{
#ifdef DEBUG_XYZ_DATA
	__debug("x,y,z,ts:, %d, %d, %d, %lld \n", xyz[0], xyz[1], xyz[2],
		ktime_to_ms(ts) );
#endif
	input_report_abs(dev, ABS_X, xyz[0]);
	input_report_abs(dev, ABS_Y, xyz[1]);
	input_report_abs(dev, ABS_Z, xyz[2]);
	input_event(dev, EV_SYN, SYN_TIME_SEC, ktime_to_timespec(ts).tv_sec);
	input_event(dev, EV_SYN, SYN_TIME_NSEC, ktime_to_timespec(ts).tv_nsec);
	input_sync(dev);
}

static enum hrtimer_restart kx122_data_timer_handler(struct hrtimer *handle)
{
	struct kx122_data *sdata = container_of(handle,
				struct kx122_data, accel_timer);

	if (sdata->state & KX122_STATE_STRM) {
		ktime_t time;
		time = ktime_set(0,sdata->accel_poll_rate * NSEC_PER_MSEC);
		hrtimer_forward_now(&sdata->accel_timer, time);
		sdata->accel_wkp_flag = 1;
		wake_up_interruptible(&sdata->accel_wq);

		return HRTIMER_RESTART;
	} else {
		__debug("%s: no restart\n", __FUNCTION__);
		return HRTIMER_NORESTART;
	}
}

static int kx122_data_strm_thread(void *data)
{
	struct kx122_data *sdata = data;


	while (1) {
		wait_event_interruptible(sdata->accel_wq,
			((sdata->accel_wkp_flag != 0) ||
				kthread_should_stop()));
		sdata->accel_wkp_flag = 0;

		if (kthread_should_stop())
			break;

		if (sdata->accel_delay_change) {
#ifdef QCOM_SENSORS
			if (sdata->accel_poll_rate <= POLL_MS_100HZ)
				set_wake_up_idle(true);
			else
				set_wake_up_idle(false);
#endif
			sdata->accel_delay_change = false;
		}

		if(sdata->state & KX122_STATE_STRM) {
			kx122_strm_report_data(sdata);
		}
		else if(sdata->state & KX122_STATE_FIFO)
		{
			kx122_fifo_report_data(sdata);
		}
	}

	return 0;
}

/* sysfs attributes */
static ssize_t kx122_sysfs_get_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	u8 enabled = sdata->state & KX122_STATE_STRM;

	return sprintf(buf, "%u\n", enabled);
}

static ssize_t kx122_sysfs_set_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	int err;
	unsigned long value;
	struct kx122_data *sdata = dev_get_drvdata(dev);

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	/* state update uses mutex */
	err = kx122_state_update(sdata, KX122_STATE_STRM, value);

	return (err < 0) ? err : len;
}

static ssize_t kx122_sysfs_get_delay(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", sdata->accel_poll_rate);
}

static ssize_t kx122_sysfs_set_delay(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	int err;
	unsigned long value;
	struct kx122_data *sdata = dev_get_drvdata(dev);

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&sdata->mutex);
	err = kx122_data_delay_set(sdata, value);
	mutex_unlock(&sdata->mutex);

	return (err < 0) ? err : len;
}

#ifdef DEBUG_SYSFS_ATTRIB
static ssize_t kx122_sysfs_reg_read(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	int err;
	unsigned long value;
	struct kx122_data *sdata = dev_get_drvdata(dev);
	u8 reg;
	s32 reg_val;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&sdata->mutex);

	reg = 0xff & value;

	reg_val = kx122_reg_read_byte(sdata, reg);

	if (reg_val < 0) {
		err = reg_val;
		__debug("%s reg 0x%x fail - %d \n", __FUNCTION__,reg, reg_val);
	}
	else {
		err = 0;
		__debug("%s reg 0x%x val 0x%x \n", __FUNCTION__,reg, reg_val);
	}

	mutex_unlock(&sdata->mutex);

	return (err < 0) ? err : len;
}

static ssize_t kx122_sysfs_reg_write(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int err;
	unsigned long value;
	u8 reg;
	u8 reg_val;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&sdata->mutex);

	reg = 0xff & value;
	reg_val = 0xff & (value >> 8);

	err = kx122_reg_write_byte(sdata, reg, reg_val);

	if (err < 0)
		__debug("%s reg 0x%x fail - %d \n", __FUNCTION__, reg, err);
	else
		__debug("%s reg 0x%x val 0x%x \n",__FUNCTION__, reg, reg_val);

	mutex_unlock(&sdata->mutex);

	return (err < 0) ? err : len;
}
#endif

static ssize_t kx122_sysfs_enable_wake_write(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int err;
	unsigned long value;

	if (kstrtoul(buf, 0, &value)) {
		return -EINVAL;
	}

	mutex_lock(&sdata->mutex);

	if(value) {
		sdata->enable_wake = 1;
	}
	else {
		sdata->enable_wake = 0;
	}

	__debug("%s wakeup from suspending : %d \n", __FUNCTION__, sdata->enable_wake);

	mutex_unlock(&sdata->mutex);

	return len;
}
static ssize_t kx122_sysfs_enable_wake_read(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", sdata->enable_wake);
}



enum {
	EBRMAIN_UNKOWN=0,
	EBRMAIN_UP = 1,
	EBRMAIN_DWON = 2,
	EBRMAIN_RIGHT = 3,
	EBRMAIN_LEFT = 4 ,
	EBRMAIN_FACE_UP =5 ,
	EBRMAIN_FACE_DOWN = 6
};

enum {
	EBRMAIN_ROTATE_R_0 = 0,
	EBRMAIN_ROTATE_R_90 = 1,
	EBRMAIN_ROTATE_R_180 = 2,
	EBRMAIN_ROTATE_R_270 = 3,
};

static ssize_t kx122_sysfs_get_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int res ;

	res = kx122_reg_read_byte(sdata, KX122_TSCP);

	if(res & KX122_TSCP_FU ){
		printk("[Face Down]\n");		
	}
	else if(res & KX122_TSCP_FD ){
		printk("[Face Up]\n");	
	}
	if(res & KX122_TSCP_UP ){
		switch(gRotate_Angle){
			case EBRMAIN_ROTATE_R_0 : 	printk("[Up State]\n");	break;
			case EBRMAIN_ROTATE_R_90 :	printk("[Right State]\n");break;
			case EBRMAIN_ROTATE_R_180 : printk("[Down State]\n");	break;
			case EBRMAIN_ROTATE_R_270 : printk("[Left State]\n");	break;
			default:
				break;
		}			
	}
	else if(res & KX122_TSCP_DO ){
		switch(gRotate_Angle){
			case EBRMAIN_ROTATE_R_0 : 	printk("[Down State]\n");	break;
			case EBRMAIN_ROTATE_R_90 :	printk("[Left  State]\n");	break;
			case EBRMAIN_ROTATE_R_180 : printk("[Up State]\n");		break;
			case EBRMAIN_ROTATE_R_270 : printk("[Right State]\n");	break;
			default:
				break;
		}						
	}
	else if(res & KX122_TSCP_RI ){
		switch(gRotate_Angle){
			case EBRMAIN_ROTATE_R_0 : 	printk("[Right State]\n");	break;
			case EBRMAIN_ROTATE_R_90 :	printk("[Down State]\n");	break;
			case EBRMAIN_ROTATE_R_180 : printk("[Left State]\n");	break;
			case EBRMAIN_ROTATE_R_270 : printk("[Up State]\n");		break;
			default:
				break;
		}				
	}			
	else if(res & KX122_TSCP_LE ){
		switch(gRotate_Angle){
			case EBRMAIN_ROTATE_R_0 : 	printk("[Left State]\n");	break;
			case EBRMAIN_ROTATE_R_90 :	printk("[Up  State]\n");	break;
			case EBRMAIN_ROTATE_R_180 : printk("[Right State]\n");	break;
			case EBRMAIN_ROTATE_R_270 : printk("[Down State]\n");	break;
			default:
				break;
		}			
	}


	return sprintf(buf, "%x\n", res);
}

static ssize_t kx122_sysfs_set_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	//struct kx122_data *sdata = dev_get_drvdata(dev);
	int res = 0;

	return sprintf(buf, "%x\n", res);
}


static ssize_t kx122_sysfs_get_tilt_angle_hl(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int res ,val=0;

	mutex_lock(&sdata->mutex);
	res = kx122_reg_read_byte(sdata, KX122_TILT_ANGLE_HL);
	mutex_unlock(&sdata->mutex);

	return sprintf(buf, "%x\n", res);
}

static ssize_t kx122_sysfs_set_tilt_angle_hl(struct device *dev,
				struct device_attribute *attr, char *buf, size_t count)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int err = 0 ;
	unsigned long value;
	char *pHead = NULL;

	mutex_lock(&sdata->mutex);

	if( pHead = strstr(buf,"0x") )
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

	/* set PC1 bit to zero before changes */
	err = kx122_reg_reset_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);
	if (err)
		goto exit_out;

	err = kx122_reg_write_byte(sdata, KX122_TILT_ANGLE_HL, value);

	/* apply changes set PC1 bit to one */
	err = kx122_reg_set_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);

exit_out:
	if (err < 0)
		__debug("%s KX122_TILT_ANGLE_HL fail - %d \n", __FUNCTION__, err);
	else
		__debug("%s KX122_TILT_ANGLE_HL val 0x%x \n",__FUNCTION__, value);

	mutex_unlock(&sdata->mutex);

	return count;
}

static ssize_t kx122_sysfs_get_tilt_angle_ll(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int res ;

	mutex_lock(&sdata->mutex);
	res = kx122_reg_read_byte(sdata, KX122_TILT_ANGLE_LL);
	mutex_unlock(&sdata->mutex);

	return sprintf(buf, "%x\n", res);
}

static ssize_t kx122_sysfs_set_tilt_angle_ll(struct device *dev,
				struct device_attribute *attr, char *buf, size_t count)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int err;
	unsigned long value;
	char *pHead = NULL;

	printk(KERN_ERR"=== %s_%d  %s====\n",__FUNCTION__,__LINE__,buf);
	mutex_lock(&sdata->mutex);

	if(pHead = strstr(buf,"0x"))
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);



	/* set PC1 bit to zero before changes */
	err = kx122_reg_reset_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);
	if (err)
		goto exit_out;

	err = kx122_reg_write_byte(sdata, KX122_TILT_ANGLE_LL, value);

	/* apply changes set PC1 bit to one */
	err = kx122_reg_set_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);

exit_out:
	if (err < 0)
		__debug("%s KX122_TILT_ANGLE_LL fail - %d \n", __FUNCTION__, err);
	else
		__debug("%s KX122_TILT_ANGLE_LL val 0x%x \n",__FUNCTION__, value);

	mutex_unlock(&sdata->mutex);

	return count;
}

static ssize_t kx122_sysfs_get_hyst(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int res ,val=0;

	mutex_lock(&sdata->mutex);
	res = kx122_reg_read_byte(sdata, KX122_HYST_SET);
	mutex_unlock(&sdata->mutex);

	return sprintf(buf, "%x\n", res);
}

static ssize_t kx122_sysfs_set_hyst(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int err;
	unsigned long value;
	char * pHead = NULL;

	mutex_lock(&sdata->mutex);

	if(pHead = strstr(buf,"0x"))
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

	// set PC1 bit to zero before changes 
	err = kx122_reg_reset_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);
	if (err)
		goto exit_out;
	
	err = kx122_reg_write_byte(sdata, KX122_HYST_SET, value);

	// apply changes set PC1 bit to one 
	err = kx122_reg_set_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);

exit_out:
	if (err < 0)
		__debug("%s KX122_HYST_SET fail - %d \n", __FUNCTION__, err);
	else
		__debug("%s KX122_HYST_SET val 0x%x \n",__FUNCTION__, value);
	mutex_unlock(&sdata->mutex);

	return sprintf(buf, "%x\n", value);
}


static ssize_t kx122_sysfs_get_status_AP(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int res ,val=0;

	res = kx122_reg_read_byte(sdata, KX122_TSCP);

	if(res & KX122_TSCP_FU )
    {
		val = EBRMAIN_FACE_DOWN ;
    }
	else if(res & KX122_TSCP_FD )
    {
		val = EBRMAIN_FACE_UP ;	
    }
	if(res & KX122_TSCP_UP )
    {
		switch(gRotate_Angle){
			case EBRMAIN_ROTATE_R_0 : 	val = EBRMAIN_UP	; break;
			case EBRMAIN_ROTATE_R_90 :	val = EBRMAIN_RIGHT ; break;
			case EBRMAIN_ROTATE_R_180 : val = EBRMAIN_DWON	; break;
			case EBRMAIN_ROTATE_R_270 : val = EBRMAIN_LEFT	; break;
			default:
				break;
		}
    }
    else if(res & KX122_TSCP_DO )
    {
		switch(gRotate_Angle){
			case EBRMAIN_ROTATE_R_0 : 	val = EBRMAIN_DWON	; break;
			case EBRMAIN_ROTATE_R_90 :	val = EBRMAIN_LEFT  ; break;
			case EBRMAIN_ROTATE_R_180 : val = EBRMAIN_UP	; break;
			case EBRMAIN_ROTATE_R_270 : val = EBRMAIN_RIGHT	; break;
			default:
				break;
		}			
    }	    
	else if(res & KX122_TSCP_RI )
    {
		switch(gRotate_Angle){
			case EBRMAIN_ROTATE_R_0 : 	val = EBRMAIN_RIGHT ; break;
			case EBRMAIN_ROTATE_R_90 :	val = EBRMAIN_DWON  ; break;
			case EBRMAIN_ROTATE_R_180 : val = EBRMAIN_LEFT  ; break;
			case EBRMAIN_ROTATE_R_270 : val = EBRMAIN_UP	; break;
			default:
				break;
		};			
    }		
	else if(res & KX122_TSCP_LE )
     {
		switch(gRotate_Angle){
			case EBRMAIN_ROTATE_R_0 : 	val = EBRMAIN_LEFT	; break;
			case EBRMAIN_ROTATE_R_90 :	val = EBRMAIN_UP    ; break;
			case EBRMAIN_ROTATE_R_180 : val = EBRMAIN_RIGHT ; break;
			case EBRMAIN_ROTATE_R_270 : val = EBRMAIN_DWON	; break;
			default:
				break;
		};
    }   


	return sprintf(buf, "%d\n", val);
}

static ssize_t kx122_sysfs_set_status_AP(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int res = 0;

	return sprintf(buf, "%x\n", res);
}

static ssize_t kx122_sysfs_get_xyz(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kx122_data *sdata = dev_get_drvdata(dev);
	int xyz[3],err;
	//ktime_t ts;
	err = kx122_data_read_xyz(sdata, xyz);
	if(err)
	{
		printk(KERN_ERR"READ XYZ failed \n");
	}

	return sprintf(buf, "x:%d , y:%d , z:%d \n", xyz[0],xyz[1],xyz[2]);
}

static ssize_t kx122_sysfs_set_xyz(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	//struct kx122_data *sdata = dev_get_drvdata(dev);
	int res = 0;

	return sprintf(buf, "%x\n", res);
}

static struct device_attribute dev_attr_accel_enable = __ATTR(enable,
		S_IRUGO | S_IWUSR,
		kx122_sysfs_get_enable,
		kx122_sysfs_set_enable);

static struct device_attribute dev_attr_accel_delay = __ATTR(delay,
		S_IRUGO | S_IWUSR,
		kx122_sysfs_get_delay,
		kx122_sysfs_set_delay);

static struct device_attribute dev_attr_status = __ATTR(status,
		S_IRUGO| S_IWUSR,
		kx122_sysfs_get_status,
		kx122_sysfs_set_status);

static struct device_attribute dev_attr_status_AP = __ATTR(status_AP,
		S_IRUGO| S_IWUSR,
		kx122_sysfs_get_status_AP,
		kx122_sysfs_set_status_AP);
static struct device_attribute dev_attr_tilt_angle_ll = __ATTR(tilt_angle_ll,
		S_IRUGO| S_IWUSR,
		kx122_sysfs_get_tilt_angle_ll,
		kx122_sysfs_set_tilt_angle_ll);	
static struct device_attribute dev_attr_tilt_angle_hl = __ATTR(tilt_angle_hl,
		S_IRUGO| S_IWUSR,
		kx122_sysfs_get_tilt_angle_hl,
		kx122_sysfs_set_tilt_angle_hl);	
static struct device_attribute dev_attr_hyst = __ATTR(hyst,
		S_IRUGO| S_IWUSR,
		kx122_sysfs_get_hyst,
		kx122_sysfs_set_hyst);						
static struct device_attribute dev_attr_xyz = __ATTR(xyz,
		S_IRUGO| S_IWUSR,
		kx122_sysfs_get_xyz,
		kx122_sysfs_set_xyz);


#ifdef DEBUG_SYSFS_ATTRIB
static struct device_attribute dev_attr_reg_read = __ATTR(reg_read,
		S_IWUSR,
		NULL,
		kx122_sysfs_reg_read);

static struct device_attribute dev_attr_reg_write = __ATTR(reg_write,
		S_IWUSR,
		NULL,
		kx122_sysfs_reg_write);

#endif

static struct device_attribute dev_attr_enable_wake = __ATTR(enable_wake,
		S_IRUGO|S_IWUSR,
		kx122_sysfs_enable_wake_read,
		kx122_sysfs_enable_wake_write);


static struct attribute *kx122_sysfs_attrs[] = {
	&dev_attr_accel_enable.attr,
	&dev_attr_accel_delay.attr,
	&dev_attr_status.attr,
	&dev_attr_status_AP.attr,	
	&dev_attr_tilt_angle_ll.attr,
	&dev_attr_tilt_angle_hl.attr,
	&dev_attr_hyst.attr,			
	&dev_attr_xyz.attr,
	&dev_attr_enable_wake.attr,
#ifdef DEBUG_SYSFS_ATTRIB
	&dev_attr_reg_read.attr,
	&dev_attr_reg_write.attr,
#endif
	NULL
};

static struct attribute_group kx122_accel_attribute_group = {
	.attrs = kx122_sysfs_attrs
};

static int kx122_set_reg(struct kx122_data *sdata)
{
	int err;
	err = kx122_reg_write_byte(sdata, KX122_INC1, sdata->inc1);
	if(err)
		printk(KERN_ERR"[%s_%d] Write INC1 Failed\n",__FUNCTION__,__LINE__);
	err = kx122_reg_write_byte(sdata, KX122_INC4, sdata->inc4);
	if(err)
		printk(KERN_ERR"[%s_%d] Write INC4 Failed\n",__FUNCTION__,__LINE__);
	err = kx122_reg_write_byte(sdata, KX122_BUF_CNTL2, sdata->_BUF_CNTL2);	
	if(err)
		printk(KERN_ERR"[%s_%d] Write CNTL2 Failed\n",__FUNCTION__,__LINE__);
	err = kx122_reg_write_byte(sdata, KX122_CNTL1, sdata->_CNTL1);
	if(err)
		printk(KERN_ERR"[%s_%d] Write CNTL1 Failed\n",__FUNCTION__,__LINE__);
	err = kx122_reg_write_byte(sdata, KX122_TILT_ANGLE_LL, sdata->Tilt_LL);
	if(err)
		printk(KERN_ERR"[%s_%d] Write ANGLE_LL Failed\n",__FUNCTION__,__LINE__);


	msleep(5);

	return err ;
}

static void kx122_enable_irq(struct kx122_data *sdata)
{
	kx122_reg_reset_bit(sdata,KX122_CNTL1,KX122_CNTL1_PC1);		// SET PC1 to 0
	msleep(5);
	//enable_irq(sdata->irq1);
	kx122_reg_set_bit(sdata,KX122_CNTL1,KX122_CNTL1_PC1);
	msleep(5);
}
static void kx122_disable_irq(struct kx122_data *sdata)
{
	kx122_reg_reset_bit(sdata,KX122_CNTL1,KX122_CNTL1_PC1);		// SET PC1 to 0
}

#ifdef QCOM_SENSORS
/* sensor class interface */
static int kx122_cdev_sensors_enable(struct sensors_classdev *sensors_cdev,
					unsigned int enable)
{
	struct kx122_data *sdata = container_of(sensors_cdev,
						struct kx122_data,
						accel_cdev);
	int err;

	__debug("%s - enable %d\n", __FUNCTION__, enable);

	/* state update uses mutex */
	err = kx122_state_update(sdata, KX122_STATE_STRM, enable);

	return err;
}

static int kx122_cdev_sensors_poll_delay(struct sensors_classdev *sensors_cdev,
					unsigned int delay_ms)
{
	struct kx122_data *sdata = container_of(sensors_cdev,
						struct kx122_data,
						accel_cdev);
	int err;

	__debug("%s - delay_ms %d\n", __FUNCTION__, delay_ms);

	mutex_lock(&sdata->mutex);
	err = kx122_data_delay_set(sdata, delay_ms);
	mutex_unlock(&sdata->mutex);

	return err;
}

static int kx122_cdev_sensors_set_latency(struct sensors_classdev *sensors_cdev,
					unsigned int max_latency)
{

	struct kx122_data *sdata = container_of(sensors_cdev,
						struct kx122_data,
						accel_cdev);

	mutex_lock(&sdata->mutex);
	__debug("%s - max_latency %d\n", __FUNCTION__, max_latency);
	sdata->max_latency = max_latency;
	mutex_unlock(&sdata->mutex);

	return 0;
}

static int kx122_cdev_sensors_calibrate(struct sensors_classdev *sensors_cdev,
		int axis, int apply_now)
{
	struct kx122_data *sdata = container_of(sensors_cdev,
						struct kx122_data,
						accel_cdev);
	int sum_xyz[3] = {0};
	int xyz[3] = {0};
	int err;
	int i;

	mutex_lock(&sdata->mutex);
	/* Do not use any mutex functions in calibrate function */
	__debug("%s - axis=%d, apply_now=%d\n",
				__FUNCTION__,
				axis, apply_now);

	err = kx122_data_delay_set(sdata, KX122_CAL_DATA_DELAY_MS);
	if (err)
		goto exit_out;

	err = kx122_sensor_power_on(sdata);
	if (err)
		goto exit_out;

	/* set PC1 bit to enable sampling. no data ready int */
	err = kx122_reg_set_bit(sdata, KX122_CNTL1, KX122_CNTL1_PC1);

	if (err)
		goto exit_out;

	msleep(KX122_START_UP_TIME_MS);
 
	sdata->accel_cali[0] = 0;
	sdata->accel_cali[1] = 0;
	sdata->accel_cali[2] = 0;

	for(i = 0; i < KX122_CAL_SAMPLE_COUNT; i++) {

		err = kx122_data_read_xyz(sdata, xyz);

		if (err)
			goto exit_out;

		sum_xyz[0] += xyz[0];
		sum_xyz[1] += xyz[1];
		sum_xyz[2] += xyz[2];

		msleep(KX122_CAL_DATA_DELAY_MS);
	}

	sum_xyz[0] = sum_xyz[0] / KX122_CAL_SAMPLE_COUNT;
	sum_xyz[1] = sum_xyz[1] / KX122_CAL_SAMPLE_COUNT;
	sum_xyz[2] = sum_xyz[2] / KX122_CAL_SAMPLE_COUNT;

	/* remove 1g from z-axis */
	if (sdata->g_range == KX122_ACC_RANGE_2G)
		sum_xyz[2] = sum_xyz[2] - KX122_RANGE2G_1G_RAW;
	else if (sdata->g_range == KX122_ACC_RANGE_4G)
		sum_xyz[2] = sum_xyz[2] - KX122_RANGE4G_1G_RAW;
	else if (sdata->g_range == KX122_ACC_RANGE_8G)
		sum_xyz[2] = sum_xyz[2] - KX122_RANGE8G_1G_RAW;

	__debug("%s - cali x=%d, cali y=%d, cali z=%d\n",
			__FUNCTION__,
			sum_xyz[0],
			sum_xyz[1],
			sum_xyz[2]);

	switch (axis) {
	case AXIS_X:
		xyz[0] = sum_xyz[0];
		xyz[1] = 0;
		xyz[2] = 0;
		break;
	case AXIS_Y:
		xyz[0] = 0;
		xyz[1] = sum_xyz[1];
		xyz[2] = 0;
		break;
	case AXIS_Z:
		xyz[0] = 0;
		xyz[1] = 0;
		xyz[2] = sum_xyz[2];
		break;
	case AXIS_XYZ:
		xyz[0] = sum_xyz[0];
		xyz[1] = sum_xyz[1];
		xyz[2] = sum_xyz[2];
		break;
	default:
		dev_err(&sdata->client->dev,
			"%s - can not calibrate accel\n",
			__FUNCTION__);
		break;
	}

	if (apply_now) {
		sdata->accel_cali[0] = xyz[0];
		sdata->accel_cali[1] = xyz[1];
		sdata->accel_cali[2] = xyz[2];
	}

	memset(sdata->calibrate_buf, 0, sizeof(sdata->calibrate_buf));
	snprintf(sdata->calibrate_buf, sizeof(sdata->calibrate_buf),
			"%d,%d,%d", xyz[0], xyz[1], xyz[2]);
	sensors_cdev->params = sdata->calibrate_buf;
	
exit_out:
	/* standby/poweroff*/
	err = kx122_sensor_power_off(sdata);
	mutex_unlock(&sdata->mutex);

	return 0;
}

static int kx122_cdev_sensors_write_cal_params(struct sensors_classdev *sensors_cdev,
		struct cal_result_t *cal_result)
{
	struct kx122_data *sdata = container_of(sensors_cdev,
						struct kx122_data,
						accel_cdev);

	mutex_lock(&sdata->mutex);
	__debug("%s \n", __FUNCTION__);

	sdata->accel_cali[0] = cal_result->offset_x;
	sdata->accel_cali[1] = cal_result->offset_y;
	sdata->accel_cali[2] = cal_result->offset_z;

	memset(sdata->calibrate_buf, 0, sizeof(sdata->calibrate_buf));
	snprintf(sdata->calibrate_buf, sizeof(sdata->calibrate_buf),
			"%d,%d,%d", sdata->accel_cali[0], sdata->accel_cali[1],
			sdata->accel_cali[2]);

	sensors_cdev->params = sdata->calibrate_buf;

	__debug("read accel calibrate bias %d,%d,%d\n", sdata->accel_cali[0],
		sdata->accel_cali[1], sdata->accel_cali[2]);

	mutex_unlock(&sdata->mutex);

	return 0;
}

static int kx122_cdev_sensors_flush(struct sensors_classdev *sensors_cdev)
{
	struct kx122_data *sdata = container_of(sensors_cdev,
						struct kx122_data,
						accel_cdev);

	mutex_lock(&sdata->mutex);
	__debug("%s \n", __FUNCTION__);
	if (sdata->state & KX122_STATE_FIFO) {
		kx122_fifo_report_data(sdata);
	}
	mutex_unlock(&sdata->mutex);

	return 0;
}

#define KX122_FIFO_MAX_EVENT_COUNT KX122_FIFO_MAX_WMI_TH
#define KX122_FIFO_RESERVED_EVENT_COUNT KX122_FIFO_MAX_WMI_TH

int kx122_sensors_cdev_register(struct kx122_data *sdata)
{
	int err;

	/* accel sensor class cdev */
	sdata->accel_cdev = accel_sensors_cdev;
	sdata->accel_cdev.sensors_enable = kx122_cdev_sensors_enable;
	sdata->accel_cdev.sensors_poll_delay = kx122_cdev_sensors_poll_delay;
	sdata->accel_cdev.sensors_calibrate = kx122_cdev_sensors_calibrate;
	sdata->accel_cdev.sensors_write_cal_params = kx122_cdev_sensors_write_cal_params;

	/* sensor name */
	if (sdata->wai == KX112_WHO_AM_I_WIA_ID)
		sdata->accel_cdev.name = KX112_SENSOR_NAME;
	else
		sdata->accel_cdev.name = KX122_SENSOR_NAME;

	/* grange and data resolution */
	if (sdata->g_range == KX122_ACC_RANGE_2G) {
		sdata->accel_cdev.max_range = KX122_2G_RANGE;
		sdata->accel_cdev.resolution = KX122_2G_RESOLUTION;
	}
	else if (sdata->g_range == KX122_ACC_RANGE_4G){
		sdata->accel_cdev.max_range = KX122_4G_RANGE;
		sdata->accel_cdev.resolution = KX122_4G_RESOLUTION;
	}
	else if (sdata->g_range == KX122_ACC_RANGE_8G){
		sdata->accel_cdev.max_range = KX122_8G_RANGE;
		sdata->accel_cdev.resolution = KX122_8G_RESOLUTION;
	}
	else {
		sdata->accel_cdev.max_range = KX122_4G_RANGE;
		sdata->accel_cdev.resolution = KX122_4G_RESOLUTION;
	}

	/* batch/fifo */
	sdata->accel_cdev.sensors_set_latency = kx122_cdev_sensors_set_latency;
	sdata->accel_cdev.sensors_flush = kx122_cdev_sensors_flush;

	sdata->accel_cdev.fifo_reserved_event_count = KX122_FIFO_RESERVED_EVENT_COUNT,
	sdata->accel_cdev.fifo_max_event_count = KX122_FIFO_MAX_EVENT_COUNT;
	/* max fifo size in time (1Hz * fifo events) */
	sdata->accel_cdev.max_delay = MAX_POLL_RATE_MS * KX122_FIFO_MAX_EVENT_COUNT;

	err = sensors_classdev_register(&sdata->accel_input_dev->dev,
		&sdata->accel_cdev);

	return err;
}

void kx122_sensors_cdev_unregister(struct kx122_data *sdata)
{
	sensors_classdev_unregister(&sdata->accel_cdev);
}
#endif

#ifdef CONFIG_OF
static u8 kx122_map_value_to_grange(u8 grange_val)
{
	u8 grange;

	switch (grange_val) {
		case 2: grange = KX122_ACC_RANGE_2G; break;
		case 4: grange = KX122_ACC_RANGE_4G; break;
		case 8: grange = KX122_ACC_RANGE_8G; break;
		default: grange = KX122_ACC_RANGE_4G; break;
	}

	return grange;
}

static int kx122_parse_dt(struct kx122_data *sdata, struct device *dev)
{
	u32 temp_val;
	int err;

	/* mandatory dt parameters */
	err = of_property_read_u32(dev->of_node, "kionix,x-map", &temp_val);
	if (err) {
		dev_err(dev, "Unable to read property x-map. Use default 0.\n");
		temp_val = 0;
	}
	sdata->pdata.x_map = (u8) temp_val;

	err = of_property_read_u32(dev->of_node, "kionix,y-map", &temp_val);
	if (err) {
		dev_err(dev, "Unable to read property y-map. Use default 1.\n");
		temp_val = 1;
	}
	sdata->pdata.y_map = (u8) temp_val;

	err = of_property_read_u32(dev->of_node, "kionix,z-map", &temp_val);
	if (err) {
		dev_err(dev, "Unable to read property z-map. Use default 2.\n");
		temp_val = 2;
	}
	sdata->pdata.z_map = (u8) temp_val;

	err = of_property_read_u32(dev->of_node, "kionix,x-negate", &temp_val);
	if (err) {
		dev_err(dev, "Unable to read property x-negate. Use default 0.\n");
		temp_val = 0;
	}
	sdata->pdata.x_negate = (u8) temp_val;

	err = of_property_read_u32(dev->of_node, "kionix,y-negate", &temp_val);
	if (err) {
		dev_err(dev, "Unable to read property y-negate. Use default 0.\n");
		temp_val = 0;
	}
	sdata->pdata.y_negate = (u8) temp_val;

	err = of_property_read_u32(dev->of_node, "kionix,z-negate", &temp_val);
	if (err) {
		dev_err(dev, "Unable to read property z-negate. Use default 0.\n");
		temp_val = 0;
	}
	sdata->pdata.z_negate = (u8) temp_val;

	err = of_property_read_u32(dev->of_node, "kionix,g-range", &temp_val);
	if (err) {
		dev_err(dev, "Unable to read property g-range. Use default 8g\n");
		temp_val = 8;
	}
	sdata->pdata.g_range = kx122_map_value_to_grange((u8) temp_val);

	/* optional dt parameters i.e. use poll if gpio-int not found */
	sdata->pdata.gpio_int1 = of_get_named_gpio_flags(dev->of_node,
		"kionix,gpio-int1", 0, NULL);

	sdata->pdata.gpio_int2 = of_get_named_gpio_flags(dev->of_node,
		"kionix,gpio-int2", 0, NULL);

	sdata->pdata.use_drdy_int = of_property_read_bool(dev->of_node,
		"kionix,use-drdy-int");

#ifdef DEBUG_DT_PARAMS
	__debug("sdata->pdata.x_map %d\n", sdata->pdata.x_map);
	__debug("sdata->pdata.y_map %d\n", sdata->pdata.y_map);
	__debug("sdata->pdata.z_map %d\n", sdata->pdata.z_map);
	__debug("sdata->pdata.x_negate %d\n", sdata->pdata.x_negate);
	__debug("sdata->pdata.y_negate %d\n", sdata->pdata.y_negate);
	__debug("sdata->pdata.z_negate %d\n", sdata->pdata.z_negate);
	__debug("sdata->pdata.g_range %d\n", sdata->pdata.g_range);
	__debug("sdata->pdata.gpio_int1 %d\n", sdata->pdata.gpio_int1); 
	__debug("sdata->pdata.gpio_int2 %d\n", sdata->pdata.gpio_int2);
	__debug("sdata->pdata.use_drdy_int %d\n", sdata->pdata.use_drdy_int); 
#endif

	return 0;
}
#else
static int kx122_parse_dt(struct kx122_data *sdata, struct device *dev)
{
	return -ENODEV;
}
#endif /* !CONFIG_OF */

#if POWER_CONTROL
static int kx122_power_on(struct kx122_data *sdata, bool on)
{
	int rc = 0;

	if (!on && sdata->power_enabled) {
		rc = regulator_disable(sdata->vdd);
		if (rc) {
			dev_err(&sdata->client->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(sdata->vio);
		if (rc) {
			dev_err(&sdata->client->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(sdata->vdd);
			if (rc) {
				dev_err(&sdata->client->dev,
					"Regulator vdd enable failed rc=%d\n",
					rc);
			}
		}
		sdata->power_enabled = false;
	} else if (on && !sdata->power_enabled) {
		rc = regulator_enable(sdata->vdd);
		if (rc) {
			dev_err(&sdata->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(sdata->vio);
		if (rc) {
			dev_err(&sdata->client->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(sdata->vdd);
		}
		sdata->power_enabled = true;
	} else {
		dev_warn(&sdata->client->dev,
				"Power on=%d. enabled=%d\n",
				on, sdata->power_enabled);
	}

	return rc;
}

static int kx122_power_init(struct kx122_data *sdata, bool on)
{
	int rc;

	if (!on) {
		if (regulator_count_voltages(sdata->vdd) > 0)
			regulator_set_voltage(sdata->vdd, 0, KX122_VDD_MAX_UV);

		regulator_put(sdata->vdd);

		if (regulator_count_voltages(sdata->vio) > 0)
			regulator_set_voltage(sdata->vio, 0, KX122_VIO_MAX_UV);

		regulator_put(sdata->vio);
	} else {
		sdata->vdd = regulator_get(&sdata->client->dev, "vdd");
		if (IS_ERR(sdata->vdd)) {
			rc = PTR_ERR(sdata->vdd);
			dev_err(&sdata->client->dev,
				"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(sdata->vdd) > 0) {
			rc = regulator_set_voltage(sdata->vdd, KX122_VDD_MIN_UV,
						   KX122_VDD_MAX_UV);
			if (rc) {
				dev_err(&sdata->client->dev,
					"Regulator set failed vdd rc=%d\n",
					rc);
				goto reg_vdd_put;
			}
		}

		sdata->vio = regulator_get(&sdata->client->dev, "vio");
		if (IS_ERR(sdata->vio)) {
			rc = PTR_ERR(sdata->vio);
			dev_err(&sdata->client->dev,
				"Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(sdata->vio) > 0) {
			rc = regulator_set_voltage(sdata->vio, KX122_VIO_MIN_UV,
						   KX122_VIO_MAX_UV);
			if (rc) {
				dev_err(&sdata->client->dev,
				"Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;

reg_vio_put:
	regulator_put(sdata->vio);
reg_vdd_set:
	if (regulator_count_voltages(sdata->vdd) > 0)
		regulator_set_voltage(sdata->vdd, 0, KX122_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(sdata->vdd);
	return rc;
}
#endif

#ifdef CONFIG_SOC_IMX6SLL
static int kx122_pinctrl_init(struct kx122_data *sdata)
{
	struct i2c_client *client = sdata->client;

	sdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(sdata->pinctrl)) {
		dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(sdata->pinctrl);
	}

	sdata->pin_int_default = pinctrl_lookup_state(sdata->pinctrl,
			"kx122_int_default");
	if (IS_ERR_OR_NULL(sdata->pin_int_default)) {
		dev_err(&client->dev, "Failed to look up default state\n");
		return PTR_ERR(sdata->pin_int_default);
	}

	sdata->pin_int_sleep = pinctrl_lookup_state(sdata->pinctrl,
			"kx122_int_sleep");
	if (IS_ERR_OR_NULL(sdata->pin_int_sleep)) {
		dev_err(&client->dev, "Failed to look up sleep state\n");
		return PTR_ERR(sdata->pin_int_sleep);
	}

	return 0;
}
#endif

/* probe */
static int kx122_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{	
	struct kx122_data *sdata;
	int err;
	int val;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev,
			"no algorithm associated to the i2c bus\n");
		return -ENODEV;
	}

	sdata = devm_kzalloc(&client->dev,sizeof(struct kx122_data), GFP_KERNEL);
	if (!sdata) {
		dev_err(&client->dev, "no memory available\n");
		return -ENOMEM;
	}

	mutex_init(&sdata->mutex);
	mutex_lock(&sdata->mutex);
	g_kx122_data = sdata;
	sdata->client = client;
	i2c_set_clientdata(client, sdata);

	/* get driver platform data */
	if (client->dev.of_node) {
		err = kx122_parse_dt(sdata, &client->dev);
		if (err) {
			dev_err(&client->dev,
				"Unable to parse dt data err=%d\n", err);
			err = -EINVAL;
			goto free_init;
		}
	}
	else if (client->dev.platform_data) {
		sdata->pdata = *(struct kx122_platform_data *)client->dev.platform_data;
	}
	else {
		dev_err(&client->dev,"platform data is NULL\n");
		err = -EINVAL;
		goto free_init;
	}

#if POWER_CONTROL
	err = kx122_power_init(sdata, true);
	if (err < 0) {
		dev_err(&client->dev, "power init failed! err=%d", err);
		goto free_init;
	}

	err = kx122_power_on(sdata, true);
	if (err < 0) {
		dev_err(&client->dev, "power on failed! err=%d\n", err);
		goto free_power_init;
	}
#endif

	msleep(KX122_POWER_UP_TIME_MS);

	err = kx122_hw_detect(sdata);
	if (err) {
		dev_err(&client->dev, "sensor not recognized\n");
		goto free_power_on;
	}

	err = kx122_soft_reset(sdata);
	if (err) {
		dev_err(&client->dev, "soft reset failed\n");
		goto free_power_on;
	}

	sdata->enable_wake = 1;
	sdata->enable_wake_state = 0;

	if (sdata->pdata.init) {
		err = sdata->pdata.init();
		if (err) {
			dev_err(&client->dev, "pdata.init() call failed \n");
			goto free_power_on;
		}
	}


	/* init accelerometer input dev */
	err = kx122_input_dev_register(sdata);
	if (err < 0) {
		dev_err(&client->dev,
			"accel input register fail\n");
		goto free_src;
	}
	err = sysfs_create_group(&sdata->accel_input_dev->dev.kobj,
					&kx122_accel_attribute_group);
	if (err) {
		dev_err(&client->dev,
			"accel sysfs create fail\n");
		goto free_input_accel;
	}

#ifdef CONFIG_SOC_IMX6SLL
	/* initialize pinctrl */
	if (!kx122_pinctrl_init(sdata)) {
		err = pinctrl_select_state(sdata->pinctrl, sdata->pin_int_sleep);
		if (err) {
			dev_err(&client->dev, "Can't select pinctrl state\n");
			goto free_sysfs_accel;
		}
	}
#else
	// 
#endif
	//printk(KERN_ERR"=======sdata->pdata.use_drdy_int%d ,sdata->pdata.gpio_int1:%d  =====\n",sdata->pdata.use_drdy_int,sdata->pdata.gpio_int1);

	/* gpio to irq */
	if (sdata->pdata.use_drdy_int && gpio_is_valid(sdata->pdata.gpio_int1)) {
		err = gpio_request(sdata->pdata.gpio_int1, "kx122_int1");
		if (err) {
			dev_err(&client->dev,
				"Unable to request interrupt gpio1 %d\n",
				sdata->pdata.gpio_int1);
			goto free_sysfs_accel;
		}

		err = gpio_direction_input(sdata->pdata.gpio_int1);
		if (err) {
			dev_err(&client->dev,
				"Unable to set direction for gpio1 %d\n",
				sdata->pdata.gpio_int1);
			goto free_sysfs_accel;
		}
		sdata->irq1 = gpio_to_irq(sdata->pdata.gpio_int1);
		printk(KERN_ERR"[%s_%d] irq1:%d , gpio1:%d \n",__FUNCTION__,__LINE__,sdata->irq1,sdata->pdata.gpio_int1);
	}

#if 0 // only use IRT1
	if (sdata->pdata.use_drdy_int && gpio_is_valid(sdata->pdata.gpio_int2)) {
		err = gpio_request(sdata->pdata.gpio_int2, "kx122_int2");
		if (err) {
			dev_err(&client->dev,
				"Unable to request interrupt gpio2 %d\n",
				sdata->pdata.gpio_int2);
			goto free_sysfs_accel;
		}

		err = gpio_direction_input(sdata->pdata.gpio_int2);
		if (err) {
			dev_err(&client->dev,
				"Unable to set direction for gpio2 %d\n",
				sdata->pdata.gpio_int2);
			goto free_sysfs_accel;
		}
		sdata->irq2 = gpio_to_irq(sdata->pdata.gpio_int2);
		printk(KERN_ERR"[%s_%d] irq2:%d , gpio2:%d \n",__FUNCTION__,__LINE__,sdata->irq2,sdata->pdata.gpio_int2);
	}
#endif

	/* NOTE; by default all interrupts are routed to int1 */
	/* setup irq handler */
	if (sdata->irq1) {
		INIT_DELAYED_WORK(&kx122_delay_work, kx122_work_func);
#ifdef EN_THREAD_IRQ //[
		err = request_threaded_irq(sdata->irq1, NULL,kx122_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,KX122_NAME, sdata);
#else //][!EN_THREAD_IRQ
		err = request_irq(sdata->irq1,kx122_interrupt/*kx122_irq_handler*/,IRQF_TRIGGER_RISING | IRQF_ONESHOT,KX122_NAME,sdata);
#endif //] EN_THREAD_IRQ
		if (err) {
			dev_err(&client->dev, "unable to request irq1\n");
			cancel_delayed_work_sync (&kx122_delay_work);
			goto free_sysfs_accel;
		}

		//disable_irq(sdata->irq1);
		printk(KERN_ERR"[%s_%d] using irq1:%d  \n",__FUNCTION__,__LINE__,sdata->irq1);
	}

	if (sdata->irq2) {
		err = request_threaded_irq(sdata->irq2, NULL,
				kx122_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				KX122_NAME, sdata);

		if (err) {
			dev_err(&client->dev, "unable to request irq2\n");
			goto free_irq1;
		}
		disable_irq(sdata->irq2);
		printk(KERN_ERR"[%s_%d] using irq2:%d  \n",__FUNCTION__,__LINE__,sdata->irq2);
	}

	/* poll rate for accel */
	if (sdata->pdata.poll_rate_ms < MIN_POLL_RATE_MS)
		sdata->accel_poll_rate = MIN_POLL_RATE_MS;
	else
		sdata->accel_poll_rate = sdata->pdata.poll_rate_ms;
	sdata->accel_delay_change = true;


	/* grange for accel */
	if (sdata->pdata.g_range != KX122_ACC_RANGE_2G &&
		sdata->pdata.g_range != KX122_ACC_RANGE_4G &&
		sdata->pdata.g_range != KX122_ACC_RANGE_8G)
		sdata->g_range = KX122_ACC_RANGE_DEFAULT;
	else
		sdata->g_range = sdata->pdata.g_range;

	printk(KERN_ERR"[%s_%d] g_range:%d \n",__FUNCTION__,__LINE__,sdata->g_range);
	/* poll timer */
	hrtimer_init(&sdata->accel_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sdata->accel_timer.function = kx122_data_timer_handler;

	/* data report thread */
	init_waitqueue_head(&sdata->accel_wq);
	sdata->accel_wkp_flag = 0;
	sdata->accel_task = kthread_run(kx122_data_strm_thread, sdata,"kionix_accel");

	/* set 16 data bit resolution */
	sdata->data_res = 8;	// 16 , 8 : low power

	// Set Register
	if(80 == gptHWCFG->m_val.bPCB) { // E60QU4
		sdata->inc1 = 0 |  KX122_INC1_IEN1 | KX122_INC1_PWSEL1_2XOSA | KX122_INC1_IEL1 ;	// active low
	}
	else{
		sdata->inc1 = KX122_INC1_IEA1|  KX122_INC1_IEN1;	// active high
	}
	sdata->inc4 = KX122_INC4_TPI1 /*| KX122_INC4_WUFI1*/;
	// CNTL1 controls the main feature set of the accelerometer
	sdata->_CNTL1 = KX122_CNTL1_TPE /*| KX122_CNTL1_WUFE*/ | KX122_CNTL1_GSEL_2G ;
	sdata->_BUF_CNTL2 = 0;
	sdata->Tilt_LL = 0x11 ;

	// Write Register
	kx122_set_reg(sdata);

	/* data report using irq or timer*/
	if (sdata->pdata.use_drdy_int && (sdata->irq1 || sdata->irq2 ))
	{
		sdata->use_poll_timer = false;
		printk(KERN_ERR"[%s_%d] using irq \n",__FUNCTION__,__LINE__);
	}
	else
	{
		sdata->use_poll_timer = true;
		printk(KERN_ERR"[%s_%d] using timer \n",__FUNCTION__,__LINE__);
	}

#if POWER_CONTROL
	/* power off sensor */
	err = kx122_power_on(sdata, false);
	if (err) {
		dev_err(&client->dev, "failed to power off sensor\n");
		goto free_sensor_init;
	}
#endif

#ifdef QCOM_SENSORS
	/* register to sensors class */
	err = kx122_sensors_cdev_register(sdata);
	if (err) {
		dev_err(&client->dev, "create class device file failed\n");
		err = -EINVAL;
		goto free_sensor_init;
	}
#endif

	mutex_unlock(&sdata->mutex);

	// Print Register
	val = kx122_reg_read_byte(sdata, KX122_CNTL1);
	printk(KERN_ERR"[kx122-CNTL1]:%x \n",val);
	val = kx122_reg_read_byte(sdata, KX122_CNTL2);
	printk(KERN_ERR"[kx122-CNTL2]:%x \n",val);
	val = kx122_reg_read_byte(sdata, KX122_CNTL3);
	printk(KERN_ERR"[kx122-CNTL3]:%x \n",val);

	val = kx122_reg_read_byte(sdata, KX122_TILT_ANGLE_LL);
	printk(KERN_ERR"[KX122_TILT_ANGLE_LL]:%x \n",val);


	if(80==gptHWCFG->m_val.bPCB || 87==gptHWCFG->m_val.bPCB) // E60QU4, E70K02
		gRotate_Angle = EBRMAIN_ROTATE_R_180 ;
	else if(83==gptHWCFG->m_val.bPCB) // E60QT4
       	gRotate_Angle = EBRMAIN_ROTATE_R_90 ;

	kx122_enable_irq(sdata);
	printk(KERN_ERR"[%s_%d] success \n",__FUNCTION__,__LINE__);
	return 0;

free_sensor_init:
	kthread_stop(sdata->accel_task);
free_irq2:
	if (sdata->irq2)
		free_irq(sdata->irq2, sdata);
free_irq1:
	if (sdata->irq1)
		free_irq(sdata->irq1, sdata);
free_sysfs_accel:
	sysfs_remove_group(&sdata->accel_input_dev->dev.kobj,
			&kx122_accel_attribute_group);
free_input_accel:
	kx122_input_dev_cleanup(sdata);
free_src:
	if (sdata->pdata.release)
		sdata->pdata.release();
free_power_on:
#if POWER_CONTROL
	kx122_power_on(sdata, false);
#endif
free_power_init:
#if POWER_CONTROL
	kx122_power_init(sdata, false);
#endif
free_init:
	mutex_unlock(&sdata->mutex);

	return err;
}

static int kx122_remove(struct i2c_client *client)
{
	struct kx122_data *sdata = i2c_get_clientdata(client);

#ifdef QCOM_SENSORS
	kx122_sensors_cdev_unregister(sdata);
#endif
	sysfs_remove_group(&sdata->accel_input_dev->dev.kobj,
		&kx122_accel_attribute_group);

	if (sdata->accel_task) {
		hrtimer_cancel(&sdata->accel_timer);
		kthread_stop(sdata->accel_task);
		sdata->accel_task = NULL;
	}

	if (sdata->irq1) {
		free_irq(sdata->irq1, sdata);
		sdata->irq1 = 0;
	}

	if (sdata->irq2) {
		free_irq(sdata->irq2, sdata);
		sdata->irq2 = 0;
	}

	if (gpio_is_valid(sdata->pdata.gpio_int1))
		gpio_free(sdata->pdata.gpio_int1);

	if (gpio_is_valid(sdata->pdata.gpio_int2))
		gpio_free(sdata->pdata.gpio_int2);

	kx122_input_dev_cleanup(sdata);

#if POWER_CONTROL
	kx122_power_on(sdata, false);	
	kx122_power_init(sdata, false);
#endif

	if (sdata->pdata.release)
		sdata->pdata.release();

	return 0;
}

static int kx122_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int chip_id;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -ENODEV;
	chip_id = i2c_smbus_read_byte_data(client, KX122_WHO_AM_I);
	printk(KERN_ERR"[%s_%d] KX122 ID:%x \n",__FUNCTION__,__LINE__,chip_id);
	return 0;
}


void kx122_irq1_wake_enable(struct kx122_data *pdata,int enable)
{
	if(enable) {
		if(0==pdata->enable_wake_state) {
			enable_irq_wake(pdata->irq1);
			pdata->enable_wake_state = 1;
		}
	}
	else {
		if(1==pdata->enable_wake_state) {
			disable_irq_wake(pdata->irq1);
			pdata->enable_wake_state = 0;
		}
	}
}

#ifdef CONFIG_PM
static int kx122_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
    struct kx122_data *pdata = i2c_get_clientdata(client);	

	printk("%s(%d),sleep=%d,irq1=%d\n",__FUNCTION__,__LINE__,gSleep_Mode_Suspend,pdata->irq1);
	msleep(5);
	if (gSleep_Mode_Suspend) {
		kx122_disable_irq(pdata);
#ifdef NO_FREE_IRQ//[
		kx122_irq1_wake_enable(pdata,0);
#else //][!NO_FREE_IRQ
		free_irq(pdata->irq1, pdata);
#endif //] NO_FREE_IRQ
	}
	else {
		if(pdata->enable_wake) {
			printk("kx122_suspend,enable irq wakeup source %d\n",pdata->irq1);
			kx122_irq1_wake_enable(pdata,1);
		}
		else {
			kx122_irq1_wake_enable(pdata,0);
		}
	}
	return 0;
}

static int kx122_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kx122_data *pdata = i2c_get_clientdata(client);
	int err = 0;

	printk("%s(%d),sleep=%d,irq1=%d\n",__FUNCTION__,__LINE__,gSleep_Mode_Suspend,pdata->irq1);

	if (gSleep_Mode_Suspend) {
		int ret; 
		kx122_set_reg(pdata);
#ifdef NO_FREE_IRQ//[
		kx122_irq1_wake_enable(pdata,1);
#else //][!NO_FREE_IRQ

#ifdef EN_THREAD_IRQ //[
		ret = request_threaded_irq(pdata->irq1,NULL,kx122_irq_handler,IRQF_TRIGGER_RISING | IRQF_ONESHOT,KX122_NAME,pdata);
#else //][!EN_THREAD_IRQ
		ret = request_irq(pdata->irq1,kx122_interrupt,IRQF_TRIGGER_RISING | IRQF_ONESHOT,KX122_NAME,pdata);
#endif //] EN_THREAD_IRQ
		if (err) {
			dev_err(&client->dev, "unable to request irq1\n");
			cancel_delayed_work_sync (&kx122_delay_work);
			printk(KERN_ERR"[%s_%d] Request irq failed \n",__FUNCTION__,__LINE__);
		}
#endif //] NO_FREE_IRQ
		kx122_enable_irq(pdata);
	}
	else {
		schedule_delayed_work(&kx122_delay_work, 0);
		if(pdata->enable_wake) {
			printk("kx122_resume,disable irq wakeup source %d\n",pdata->irq1);
			kx122_irq1_wake_enable(pdata,0);
		}
		else {
			kx122_irq1_wake_enable(pdata,1);
		}
	}
	return 0;
}
#else
#define kx122_suspend	NULL
#define kx122_resume	NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int kx122_runtime_suspend(struct device *dev)
{
	return 0;
}

static int kx122_runtime_resume(struct device *dev)
{
	return 0;
}
#else
#define kx122_runtime_suspend	NULL
#define kx122_runtime_resume	NULL
#endif

static const struct i2c_device_id kx122_id[] = {
	{ KX122_DEV_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, kx122_id);

static const struct of_device_id kx122_of_match[] = {
	{ .compatible = "kionix,kx122", },
	{ },
};
MODULE_DEVICE_TABLE(of, kx122_of_match);

static const unsigned short normal_i2c[] = {0x1e, 0x1f, I2C_CLIENT_END};
static const struct dev_pm_ops kx122_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(kx122_suspend, kx122_resume)
	SET_RUNTIME_PM_OPS(kx122_runtime_suspend, kx122_runtime_resume, NULL)
};

static struct i2c_driver kx122_driver = {
	.class = I2C_CLASS_HWMON ,
	.driver = {
			.owner = THIS_MODULE,
			.name  = KX122_DEV_NAME,
			.pm    = &kx122_pm_ops,
			.of_match_table = kx122_of_match,
		  },
	.probe    = kx122_probe,
	.remove   = kx122_remove,
	.id_table = kx122_id,
	.detect = kx122_detect,
	.address_list = normal_i2c,	
};

static int __init kx122_init(void)
{
	/* register driver */
	int res;

	res = i2c_add_driver(&kx122_driver);
	if (res < 0) {
		printk(KERN_INFO "add kx122 i2c driver failed\n");
		return -ENODEV;
	}
	return res;
}

static void __exit kx122_exit(void)
{
	i2c_del_driver(&kx122_driver);
}


//module_i2c_driver(kx122_driver);
module_init(kx122_init);
module_exit(kx122_exit);

MODULE_DESCRIPTION("kx122 driver");
MODULE_AUTHOR("Kionix");
MODULE_LICENSE("GPL");

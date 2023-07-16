/* drivers/input/misc/kionix_accel.c - Kionix accelerometer driver
 *
 * Copyright (C) 2012 Kionix, Inc.
 * Written by Kuching Tan <kuchingtan@kionix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "kionix_accel.h"
#include <linux/version.h>
#include <linux/proc_fs.h>
#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#ifdef    CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */

#define EN_THREAD_IRQ		1

/* Debug Message Flags */
#define KIONIX_KMSG_ERR	1	/* Print kernel debug message for error */
#define KIONIX_KMSG_INF	1	/* Print kernel debug message for info */

#if KIONIX_KMSG_ERR
#define KMSGERR(format, ...)	\
		dev_err(format, ## __VA_ARGS__)
#else
#define KMSGERR(format, ...)
#endif

#if KIONIX_KMSG_INF
#define KMSGINF(format, ...)	\
		dev_info(format, ## __VA_ARGS__)
#else
#define KMSGINF(format, ...)
#endif


enum {
	EBRMAIN_UNKOWN=0,
	EBRMAIN_UP = 1,
	EBRMAIN_DWON = 2,
	EBRMAIN_RIGHT = 3,
	EBRMAIN_LEFT = 4 ,
	EBRMAIN_FACE_UP =5 ,
	EBRMAIN_FACE_DOWN = 6
};

/******************************************************************************
 * Accelerometer WHO_AM_I return value
 *****************************************************************************/
#define KIONIX_ACCEL_WHO_AM_I_KXTE9 		0x00
#define KIONIX_ACCEL_WHO_AM_I_KXTF9 		0x01
#define KIONIX_ACCEL_WHO_AM_I_KXTI9_1001 	0x04
#define KIONIX_ACCEL_WHO_AM_I_KXTIK_1004 	0x05
#define KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005 	0x07
#define KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007 	0x08
#define KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008 	0x0A
#define KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009 	0x09
#define KIONIX_ACCEL_WHO_AM_I_KXTJ3_1057 	0x35
#define KIONIX_ACCEL_WHO_AM_I_KXCJK_1013 	0x11

/******************************************************************************
 * Accelerometer Grouping
 *****************************************************************************/
#define KIONIX_ACCEL_GRP1	1	/* KXTE9 */
#define KIONIX_ACCEL_GRP2	2	/* KXTF9/I9-1001/J9-1005 */
#define KIONIX_ACCEL_GRP3	3	/* KXTIK-1004 */
#define KIONIX_ACCEL_GRP4	4	/* KXTJ9-1007/KXCJ9-1008 */
#define KIONIX_ACCEL_GRP5	5	/* KXTJ2-1009 /KXTJ3-1057 */
#define KIONIX_ACCEL_GRP6	6	/* KXCJK-1013 */

/******************************************************************************
 * Registers for Accelerometer Group 1 & 2 & 3
 *****************************************************************************/
#define ACCEL_WHO_AM_I		0x0F

/*****************************************************************************/
/* Registers for Accelerometer Group 1 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP1_XOUT			0x12
/* Control Registers */
#define ACCEL_GRP1_CTRL_REG1	0x1B
/* CTRL_REG1 */
#define ACCEL_GRP1_PC1_OFF		0x7F
#define ACCEL_GRP1_PC1_ON		(1 << 7)
#define ACCEL_GRP1_ODR40		(3 << 3)
#define ACCEL_GRP1_ODR10		(2 << 3)
#define ACCEL_GRP1_ODR3			(1 << 3)
#define ACCEL_GRP1_ODR1			(0 << 3)
#define ACCEL_GRP1_ODR_MASK		(3 << 3)

/*****************************************************************************/
/* Registers for Accelerometer Group 2 & 3 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP2_XOUT_L		0x06
/* Control Registers */
#define ACCEL_GRP2_INT_REL		0x1A
#define ACCEL_GRP2_CTRL_REG1	0x1B
#define ACCEL_GRP2_INT_CTRL1	0x1E
#define ACCEL_GRP2_DATA_CTRL	0x21
/* CTRL_REG1 */
#define ACCEL_GRP2_PC1_OFF		0x7F
#define ACCEL_GRP2_PC1_ON		(1 << 7)
#define ACCEL_GRP2_DRDYE		(1 << 5)
#define ACCEL_GRP2_G_8G			(2 << 3)
#define ACCEL_GRP2_G_4G			(1 << 3)
#define ACCEL_GRP2_G_2G			(0 << 3)
#define ACCEL_GRP2_G_MASK		(3 << 3)
#define ACCEL_GRP2_RES_8BIT		(0 << 6)
#define ACCEL_GRP2_RES_12BIT	(1 << 6)
#define ACCEL_GRP2_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP2_IEA			(1 << 4)
#define ACCEL_GRP2_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP2_ODR12_5		0x00
#define ACCEL_GRP2_ODR25		0x01
#define ACCEL_GRP2_ODR50		0x02
#define ACCEL_GRP2_ODR100		0x03
#define ACCEL_GRP2_ODR200		0x04
#define ACCEL_GRP2_ODR400		0x05
#define ACCEL_GRP2_ODR800		0x06
/*****************************************************************************/

/*****************************************************************************/
/* Registers for Accelerometer Group 4 & 5 & 6 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP4_XOUT_L		0x06
#define ACCEL_GRP4_XOUT_H		0x07
#define ACCEL_GRP4_YOUT_L		0x08
#define ACCEL_GRP4_YOUT_H		0x09
#define ACCEL_GRP4_ZOUT_L		0x0a
#define ACCEL_GRP4_ZOUT_H		0x0b

/* Control Registers */
#define ACCEL_GRP4_INT_REL		0x1A
#define ACCEL_GRP4_CTRL_REG1	0x1B
#define ACCEL_GRP4_INT_CTRL1	0x1E
#define ACCEL_GRP4_DATA_CTRL	0x21
/* CTRL_REG1 */
#define ACCEL_GRP4_PC1_OFF		0x7F
#define ACCEL_GRP4_PC1_ON		(1 << 7)
#define ACCEL_GRP4_DRDYE		(1 << 5)
#define ACCEL_GRP4_WUFS			(1 << 1)
#define ACCEL_GRP4_G_8G			(2 << 3)
#define ACCEL_GRP4_G_4G			(1 << 3)
#define ACCEL_GRP4_G_2G			(0 << 3)
#define ACCEL_GRP4_G_MASK		(3 << 3)
#define ACCEL_GRP4_RES_8BIT		(0 << 6)
#define ACCEL_GRP4_RES_12BIT	(1 << 6)
#define ACCEL_GRP4_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP4_IEA			(1 << 4)
#define ACCEL_GRP4_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP4_ODR0_781		0x08
#define ACCEL_GRP4_ODR1_563		0x09
#define ACCEL_GRP4_ODR3_125		0x0A
#define ACCEL_GRP4_ODR6_25		0x0B
#define ACCEL_GRP4_ODR12_5		0x00
#define ACCEL_GRP4_ODR25		0x01
#define ACCEL_GRP4_ODR50		0x02
#define ACCEL_GRP4_ODR100		0x03
#define ACCEL_GRP4_ODR200		0x04
#define ACCEL_GRP4_ODR400		0x05
#define ACCEL_GRP4_ODR800		0x06
#define ACCEL_GRP4_ODR1600		0x07
/*****************************************************************************/
// controls the operating mode of the kionix.
#define KIONIX_CTRL_REG_PC1 (0x01 << 7)
#define KIONIX_INT_SOURCE1 0x16
#define KIONIX_INT_SOURCE2 0x17
#define KIONIX_INT_WAKEUP_COUNTER	0x29
#define KIONIX_WAKEUP_THRESH_HOLD_H	0x6A
#define KIONIX_WAKEUP_THRESH_HOLD_L	0x6B

/* Input Event Constants */
#define ACCEL_G_MAX			8096
#define ACCEL_FUZZ			3
#define ACCEL_FLAT			3
/* I2C Retry Constants */
#define KIONIX_I2C_RETRY_COUNT		10 	/* Number of times to retry i2c */
#define KIONIX_I2C_RETRY_TIMEOUT	1	/* Timeout between retry (miliseconds) */

/* Earlysuspend Contants */
#define KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT	5000	/* Timeout (miliseconds) */


extern volatile NTX_HWCONFIG *gptHWCFG;
extern int gSleep_Mode_Suspend;
extern void ntx_report_event(unsigned int type, unsigned int code, int value);
int gKionixPosition = 0 ;
int gAccelRange = 2;
/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate (ODR).
 */
static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp1_odr_table[] = {
	{ 100,	ACCEL_GRP1_ODR40 },
	{ 334,	ACCEL_GRP1_ODR10 },
	{ 1000,	ACCEL_GRP1_ODR3  },
	{ 0,	ACCEL_GRP1_ODR1  },
};

static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp2_odr_table[] = {
	{ 3,	ACCEL_GRP2_ODR800 },
	{ 5,	ACCEL_GRP2_ODR400 },
	{ 10,	ACCEL_GRP2_ODR200 },
	{ 20,	ACCEL_GRP2_ODR100 },
	{ 40,	ACCEL_GRP2_ODR50  },
	{ 80,	ACCEL_GRP2_ODR25  },
	{ 0,	ACCEL_GRP2_ODR12_5},
};

static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp4_odr_table[] = {
	{ 2,	ACCEL_GRP4_ODR1600 },
	{ 3,	ACCEL_GRP4_ODR800 },
	{ 5,	ACCEL_GRP4_ODR400 },
	{ 10,	ACCEL_GRP4_ODR200 },
	{ 20,	ACCEL_GRP4_ODR100 },
	{ 40,	ACCEL_GRP4_ODR50  },
	{ 80,	ACCEL_GRP4_ODR25  },
	{ 160,	ACCEL_GRP4_ODR12_5},
	{ 320,	ACCEL_GRP4_ODR6_25},
	{ 640,	ACCEL_GRP4_ODR3_125},
	{ 1280,	ACCEL_GRP4_ODR1_563},
	{ 0,	ACCEL_GRP4_ODR0_781},
};

enum {
	accel_grp1_ctrl_reg1 = 0,
	accel_grp1_regs_count,
};

enum {
	accel_grp2_ctrl_reg1 = 0,
	accel_grp2_data_ctrl,
	accel_grp2_int_ctrl,
	accel_grp2_regs_count,
};

enum {
	accel_grp4_ctrl_reg1 = 0,
	accel_grp4_data_ctrl,
	accel_grp4_int_ctrl,
	accel_grp4_regs_count,
};

struct kionix_accel_driver {
	struct i2c_client *client;
	struct kionix_accel_platform_data accel_pdata;
	struct input_dev *input_dev;
	struct delayed_work accel_work;
	struct workqueue_struct *accel_workqueue;
	wait_queue_head_t wqh_suspend;

	int accel_data[3];
	int accel_cali[3];
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;
	bool negate_x;
	bool negate_y;
	bool negate_z;
	u8 shift;

	unsigned int poll_interval;
	unsigned int poll_delay;
	unsigned int accel_group;
	u8 *accel_registers;

	atomic_t accel_suspended;
	atomic_t accel_suspend_continue;
	atomic_t accel_enabled;
	atomic_t accel_input_event;
	atomic_t accel_enable_resume;
	struct mutex mutex_earlysuspend;
	struct mutex mutex_resume;
	rwlock_t rwlock_accel_data;

	bool accel_drdy;
	int irq1;

	/* Function callback */
	void (*kionix_accel_report_accel_data)(struct kionix_accel_driver *acceld);
	int (*kionix_accel_update_odr)(struct kionix_accel_driver *acceld, unsigned int poll_interval);
	int (*kionix_accel_power_on_init)(struct kionix_accel_driver *acceld);
	int (*kionix_accel_operate)(struct kionix_accel_driver *acceld);
	int (*kionix_accel_standby)(struct kionix_accel_driver *acceld);


	int enable_wake;// enable wakeup from suspending .
	int enable_wake_state;// current wakeup enable/disable state .

#ifdef    CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif /* CONFIG_HAS_EARLYSUSPEND */
};

static int kionix_i2c_read(struct i2c_client *client, u8 addr, u8 *data, int len)
{
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	return i2c_transfer(client->adapter, msgs, 2);
}

static int kionix_strtok(const char *buf, size_t count, char **token, const int token_nr)
{
	char *buf2 = (char *)kzalloc((count + 1) * sizeof(char), GFP_KERNEL);
	char **token2 = token;
	unsigned int num_ptr = 0, num_nr = 0, num_neg = 0;
	int i = 0, start = 0, end = (int)count;

	strcpy(buf2, buf);

	/* We need to breakup the string into separate chunks in order for kstrtoint
	 * or strict_strtol to parse them without returning an error. Stop when the end of
	 * the string is reached or when enough value is read from the string */
	while((start < end) && (i < token_nr)) {
		/* We found a negative sign */
		if(*(buf2 + start) == '-') {
			/* Previous char(s) are numeric, so we store their value first before proceed */
			if(num_nr > 0) {
				/* If there is a pending negative sign, we adjust the variables to account for it */
				if(num_neg) {
					num_ptr--;
					num_nr++;
				}
				*token2 = (char *)kzalloc((num_nr + 2) * sizeof(char), GFP_KERNEL);
				strncpy(*token2, (const char *)(buf2 + num_ptr), (size_t) num_nr);
				*(*token2+num_nr) = '\n';
				i++;
				token2++;
				/* Reset */
				num_ptr = num_nr = 0;
			}
			/* This indicates that there is a pending negative sign in the string */
			num_neg = 1;
		}
		/* We found a numeric */
		else if((*(buf2 + start) >= '0') && (*(buf2 + start) <= '9')) {
			/* If the previous char(s) are not numeric, set num_ptr to current char */
			if(num_nr < 1)
				num_ptr = start;
			num_nr++;
		}
		/* We found an unwanted character */
		else {
			/* Previous char(s) are numeric, so we store their value first before proceed */
			if(num_nr > 0) {
				if(num_neg) {
					num_ptr--;
					num_nr++;
				}
				*token2 = (char *)kzalloc((num_nr + 2) * sizeof(char), GFP_KERNEL);
				strncpy(*token2, (const char *)(buf2 + num_ptr), (size_t) num_nr);
				*(*token2+num_nr) = '\n';
				i++;
				token2++;
			}
			/* Reset all the variables to start afresh */
			num_ptr = num_nr = num_neg = 0;
		}
		start++;
	}

	kfree(buf2);

	return (i == token_nr) ? token_nr : -1;
}

static int kionix_accel_grp1_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;

	if(atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP1_CTRL_REG1, acceld->accel_registers[accel_grp1_ctrl_reg1] | ACCEL_GRP1_PC1_ON);
		if (err < 0)
			return err;
	}
	else {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP1_CTRL_REG1, acceld->accel_registers[accel_grp1_ctrl_reg1]);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp1_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP1_CTRL_REG1, \
			acceld->accel_registers[accel_grp2_ctrl_reg1] | ACCEL_GRP1_PC1_ON);
	if (err < 0)
		return err;

	queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, 0);

	return 0;
}

static int kionix_accel_grp1_standby(struct kionix_accel_driver *acceld)
{
	int err;

	cancel_delayed_work_sync(&acceld->accel_work);

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP1_CTRL_REG1, 0);
	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp1_report_accel_data(struct kionix_accel_driver *acceld)
{
	u8 accel_data[3];
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop = KIONIX_I2C_RETRY_COUNT;

	if(atomic_read(&acceld->accel_enabled) > 0) {
		if(atomic_read(&acceld->accel_enable_resume) > 0)
		{
			while(loop) {
				mutex_lock(&input_dev->mutex);
				err = kionix_i2c_read(acceld->client, ACCEL_GRP1_XOUT, accel_data, 6);
				mutex_unlock(&input_dev->mutex);
				if(err < 0){
					loop--;
					mdelay(KIONIX_I2C_RETRY_TIMEOUT);
				}
				else
					loop = 0;
			}
			if (err < 0) {
				KMSGERR(&acceld->client->dev, "%s: read data output error = %d\n", __func__, err);
			}
			else {
				write_lock(&acceld->rwlock_accel_data);

				x = ((s16) le16_to_cpu(((s16)(accel_data[acceld->axis_map_x] >> 2)) - 32)) << 6;
				y = ((s16) le16_to_cpu(((s16)(accel_data[acceld->axis_map_y] >> 2)) - 32)) << 6;
				z = ((s16) le16_to_cpu(((s16)(accel_data[acceld->axis_map_z] >> 2)) - 32)) << 6;

				acceld->accel_data[acceld->axis_map_x] = (acceld->negate_x ? -x : x) + acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] = (acceld->negate_y ? -y : y) + acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] = (acceld->negate_z ? -z : z) + acceld->accel_cali[acceld->axis_map_z];

				if(atomic_read(&acceld->accel_input_event) > 0) {
					input_report_abs(acceld->input_dev, ABS_X, acceld->accel_data[acceld->axis_map_x]);
					input_report_abs(acceld->input_dev, ABS_Y, acceld->accel_data[acceld->axis_map_y]);
					input_report_abs(acceld->input_dev, ABS_Z, acceld->accel_data[acceld->axis_map_z]);
					input_sync(acceld->input_dev);
				}

				write_unlock(&acceld->rwlock_accel_data);
			}
		}
		else
		{
			atomic_inc(&acceld->accel_enable_resume);
		}
	}
}

static int kionix_accel_grp1_update_odr(struct kionix_accel_driver *acceld, unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp1_odr_table); i++) {
		odr = kionix_accel_grp1_odr_table[i].mask;
		if (poll_interval < kionix_accel_grp1_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update CTRL_REG1 register if the ODR is not changed */
	if((acceld->accel_registers[accel_grp1_ctrl_reg1] & ACCEL_GRP1_ODR_MASK) == odr)
		return 0;
	else {
		acceld->accel_registers[accel_grp1_ctrl_reg1] &= ~ACCEL_GRP1_ODR_MASK;
		acceld->accel_registers[accel_grp1_ctrl_reg1] |= odr;
	}

	/* Do not need to update CTRL_REG1 register if the sensor is not currently turn on */
	if(atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP1_CTRL_REG1, \
				acceld->accel_registers[accel_grp1_ctrl_reg1] | ACCEL_GRP1_PC1_ON);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp2_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;

	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(acceld->client,
					ACCEL_GRP2_CTRL_REG1, 0);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(acceld->client,
					ACCEL_GRP2_DATA_CTRL, acceld->accel_registers[accel_grp2_data_ctrl]);
	if (err < 0)
		return err;

	/* only write INT_CTRL_REG1 if in irq mode */
	if (acceld->client->irq) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP2_INT_CTRL1, acceld->accel_registers[accel_grp2_int_ctrl]);
		if (err < 0)
			return err;
	}

	if(atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP2_CTRL_REG1, acceld->accel_registers[accel_grp2_ctrl_reg1] | ACCEL_GRP2_PC1_ON);
		if (err < 0)
			return err;
	}
	else {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP2_CTRL_REG1, acceld->accel_registers[accel_grp2_ctrl_reg1]);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp2_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_CTRL_REG1, \
			acceld->accel_registers[accel_grp2_ctrl_reg1] | ACCEL_GRP2_PC1_ON);
	if (err < 0)
		return err;

	if(acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, 0);

	return 0;
}

static int kionix_accel_grp2_standby(struct kionix_accel_driver *acceld)
{
	int err;

	if(acceld->accel_drdy == 0)
		cancel_delayed_work_sync(&acceld->accel_work);

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_CTRL_REG1, 0);
	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp2_report_accel_data(struct kionix_accel_driver *acceld)
{
	struct { union {
		s16 accel_data_s16[3];
		s8	accel_data_s8[6];
	}; } accel_data;
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop;

	/* Only read the output registers if enabled */
	if(atomic_read(&acceld->accel_enabled) > 0) {
		if(atomic_read(&acceld->accel_enable_resume) > 0)
		{
			loop = KIONIX_I2C_RETRY_COUNT;
			while(loop) {
				mutex_lock(&input_dev->mutex);
				err = kionix_i2c_read(acceld->client, ACCEL_GRP2_XOUT_L, (u8 *)accel_data.accel_data_s16, 6);
				mutex_unlock(&input_dev->mutex);
				if(err < 0){
					loop--;
					mdelay(KIONIX_I2C_RETRY_TIMEOUT);
				}
				else
					loop = 0;
			}
			if (err < 0) {
				KMSGERR(&acceld->client->dev, "%s: read data output error = %d\n", __func__, err);
			}
			else {
				write_lock(&acceld->rwlock_accel_data);

				x = ((s16) le16_to_cpu(accel_data.accel_data_s16[acceld->axis_map_x])) >> acceld->shift;
				y = ((s16) le16_to_cpu(accel_data.accel_data_s16[acceld->axis_map_y])) >> acceld->shift;
				z = ((s16) le16_to_cpu(accel_data.accel_data_s16[acceld->axis_map_z])) >> acceld->shift;

				acceld->accel_data[acceld->axis_map_x] = (acceld->negate_x ? -x : x) + acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] = (acceld->negate_y ? -y : y) + acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] = (acceld->negate_z ? -z : z) + acceld->accel_cali[acceld->axis_map_z];

				if(atomic_read(&acceld->accel_input_event) > 0) {
					input_report_abs(acceld->input_dev, ABS_X, acceld->accel_data[acceld->axis_map_x]);
					input_report_abs(acceld->input_dev, ABS_Y, acceld->accel_data[acceld->axis_map_y]);
					input_report_abs(acceld->input_dev, ABS_Z, acceld->accel_data[acceld->axis_map_z]);
					input_sync(acceld->input_dev);
				}

				write_unlock(&acceld->rwlock_accel_data);
			}
		}
		else
		{
			atomic_inc(&acceld->accel_enable_resume);
		}
	}

	/* Clear the interrupt if using drdy */
	if(acceld->accel_drdy == 1) {
		loop = KIONIX_I2C_RETRY_COUNT;
		while(loop) {
			err = i2c_smbus_read_byte_data(acceld->client, ACCEL_GRP2_INT_REL);
			if(err < 0){
				loop--;
				mdelay(KIONIX_I2C_RETRY_TIMEOUT);
			}
			else
				loop = 0;
		}
		if (err < 0)
			KMSGERR(&acceld->client->dev, "%s: clear interrupt error = %d\n", __func__, err);
	}
}

static void kionix_accel_grp2_update_g_range(struct kionix_accel_driver *acceld)
{
	acceld->accel_registers[accel_grp2_ctrl_reg1] &= ~ACCEL_GRP2_G_MASK;

	switch (acceld->accel_pdata.accel_g_range) {
		case KIONIX_ACCEL_G_8G:
		case KIONIX_ACCEL_G_6G:
			acceld->shift = 2;
			acceld->accel_registers[accel_grp2_ctrl_reg1] |= ACCEL_GRP2_G_8G;
			break;
		case KIONIX_ACCEL_G_4G:
			acceld->shift = 3;
			acceld->accel_registers[accel_grp2_ctrl_reg1] |= ACCEL_GRP2_G_4G;
			break;
		case KIONIX_ACCEL_G_2G:
		default:
			acceld->shift = 4;
			acceld->accel_registers[accel_grp2_ctrl_reg1] |= ACCEL_GRP2_G_2G;
			break;
	}

	return;
}

static int kionix_accel_grp2_update_odr(struct kionix_accel_driver *acceld, unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp2_odr_table); i++) {
		odr = kionix_accel_grp2_odr_table[i].mask;
		if (poll_interval < kionix_accel_grp2_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update DATA_CTRL_REG register if the ODR is not changed */
	if(acceld->accel_registers[accel_grp2_data_ctrl] == odr)
		return 0;
	else
		acceld->accel_registers[accel_grp2_data_ctrl] = odr;

	/* Do not need to update DATA_CTRL_REG register if the sensor is not currently turn on */
	if(atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_CTRL_REG1, 0);
		if (err < 0)
			return err;

		err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_DATA_CTRL, acceld->accel_registers[accel_grp2_data_ctrl]);
		if (err < 0)
			return err;

		err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_CTRL_REG1, acceld->accel_registers[accel_grp2_ctrl_reg1] | ACCEL_GRP2_PC1_ON);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp4_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;

	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(acceld->client,
					ACCEL_GRP4_CTRL_REG1, 0);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(acceld->client,
					ACCEL_GRP4_DATA_CTRL, acceld->accel_registers[accel_grp4_data_ctrl]);
	if (err < 0)
		return err;

	/* only write INT_CTRL_REG1 if in irq mode */
	if (acceld->client->irq) {
	//if (acceld->accel_drdy) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP4_INT_CTRL1, acceld->accel_registers[accel_grp4_int_ctrl]);
		printk("write INT_CTRL_REG1 err=%d\n",err);
		if (err < 0)
			return err;
		
	}

	if(atomic_read(&acceld->accel_enabled) > 0) {//User need to turn on G-sensor by manual
	//if (acceld->accel_drdy) {//this line will let G-sensor be the IRQ mode at the power-up
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP4_CTRL_REG1, acceld->accel_registers[accel_grp4_ctrl_reg1] | ACCEL_GRP4_PC1_ON |ACCEL_GRP4_DRDYE |ACCEL_GRP4_RES_12BIT);
		printk("write CTRL_REG1 err = %d\n",err);
		if (err < 0)
			return err;
	}
	else {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP4_CTRL_REG1, acceld->accel_registers[accel_grp4_ctrl_reg1]);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp4_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1, \
			acceld->accel_registers[accel_grp4_ctrl_reg1] | ACCEL_GRP4_PC1_ON);
	if (err < 0)
		return err;

	if(acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, 0);

	return 0;
}

static int kionix_accel_grp4_standby(struct kionix_accel_driver *acceld)
{
	int err;

	if(acceld->accel_drdy == 0)
		cancel_delayed_work_sync(&acceld->accel_work);

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1, 0);
	if (err < 0)
		return err;

	return 0;
}
/*Auto-Cali Start*/
#define BUF_RANGE 10
int temp_zbuf[BUF_RANGE]={0};
int temp_zsum=0;
/*Auto-Cali End*/
static void kionix_accel_grp4_report_accel_data(struct kionix_accel_driver *acceld)
{
	struct { union {
		s16 accel_data_s16[3];
		s8	accel_data_s8[6];
	}; } accel_data;
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop;
	int i=0;

	/* Only read the output registers if enabled */
	if(atomic_read(&acceld->accel_enabled) > 0) {
		if(atomic_read(&acceld->accel_enable_resume) > 0)
		{
			loop = KIONIX_I2C_RETRY_COUNT;
			while(loop) {
				mutex_lock(&input_dev->mutex);
				err = kionix_i2c_read(acceld->client, ACCEL_GRP4_XOUT_L, (u8 *)accel_data.accel_data_s16, 6);
				mutex_unlock(&input_dev->mutex);
				if(err < 0){
					loop--;
					mdelay(KIONIX_I2C_RETRY_TIMEOUT);
				}
				else
					loop = 0;
			}
			if (err < 0) {
				KMSGERR(&acceld->client->dev, "%s: read data output error = %d\n", __func__, err);
			}
			else {
				write_lock(&acceld->rwlock_accel_data);

				x = ((s16) le16_to_cpu(accel_data.accel_data_s16[acceld->axis_map_x])) >> acceld->shift;
				y = ((s16) le16_to_cpu(accel_data.accel_data_s16[acceld->axis_map_y])) >> acceld->shift;
				z = ((s16) le16_to_cpu(accel_data.accel_data_s16[acceld->axis_map_z])) >> acceld->shift;
				/*Auto-Cali Start*/
				if((abs(x - acceld->accel_cali[acceld->axis_map_x]) < 50) && 
					(abs(y - acceld->accel_cali[acceld->axis_map_y]) < 50)){
					
					if (abs(z-1024) < 300){ 
						temp_zsum = 0;
						for(i=0;i < BUF_RANGE-1;i++){
							temp_zbuf[i] = temp_zbuf[i+1];
							temp_zsum += temp_zbuf[i];
						}	
						temp_zbuf[BUF_RANGE-1] = z;
						
						temp_zsum +=temp_zbuf[BUF_RANGE-1];
						//printk("KXTJ2 - temp_zsum value = %d\n",temp_zsum/BUF_RANGE);
					
						z=z * 1024/(temp_zsum /BUF_RANGE); //Sensitivity
						//printk("KXTJ2 - final z value = %d\n",z);
					}
				}
				/*Auto-Cali End*/

				acceld->accel_data[acceld->axis_map_x] = (acceld->negate_x ? -x : x) + acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] = (acceld->negate_y ? -y : y) + acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] = (acceld->negate_z ? -z : z) + acceld->accel_cali[acceld->axis_map_z];

				if(atomic_read(&acceld->accel_input_event) > 0) {
					input_report_abs(acceld->input_dev, ABS_X, acceld->accel_data[acceld->axis_map_x]);
					input_report_abs(acceld->input_dev, ABS_Y, acceld->accel_data[acceld->axis_map_y]);
					input_report_abs(acceld->input_dev, ABS_Z, acceld->accel_data[acceld->axis_map_z]);
					input_sync(acceld->input_dev);
				}

				write_unlock(&acceld->rwlock_accel_data);
			}
		}
		else
		{
			atomic_inc(&acceld->accel_enable_resume);
		}
	}

	/* Clear the interrupt if using drdy */
	if(acceld->accel_drdy == 1) {
		loop = KIONIX_I2C_RETRY_COUNT;
		while(loop) {
			err = i2c_smbus_read_byte_data(acceld->client, ACCEL_GRP4_INT_REL);
			if(err < 0){
				loop--;
				mdelay(KIONIX_I2C_RETRY_TIMEOUT);
			}
			else
				loop = 0;
		}
		if (err < 0)
			KMSGERR(&acceld->client->dev, "%s: clear interrupt error = %d\n", __func__, err);
	}
}

static void kionix_accel_grp4_update_g_range(struct kionix_accel_driver *acceld)
{
	acceld->accel_registers[accel_grp4_ctrl_reg1] &= ~ACCEL_GRP4_G_MASK;

	switch (acceld->accel_pdata.accel_g_range) {
		case KIONIX_ACCEL_G_8G:
		case KIONIX_ACCEL_G_6G:
			acceld->shift = 2;
			acceld->accel_registers[accel_grp4_ctrl_reg1] |= ACCEL_GRP4_G_8G;
			break;
		case KIONIX_ACCEL_G_4G:
			acceld->shift = 3;
			acceld->accel_registers[accel_grp4_ctrl_reg1] |= ACCEL_GRP4_G_4G;
			break;
		case KIONIX_ACCEL_G_2G:
		default:
			acceld->shift = 4;
			acceld->accel_registers[accel_grp4_ctrl_reg1] |= ACCEL_GRP4_G_2G;
			break;
	}

	return;
}

static int kionix_accel_grp4_update_odr(struct kionix_accel_driver *acceld, unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;
	// odr (ODR) : Output Data Rate
	KMSGINF(&acceld->client->dev, "kionix_accel_grp4_update_odr()  poll=%d\n",poll_interval);
	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp4_odr_table); i++) {
		odr = kionix_accel_grp4_odr_table[i].mask;
		if (poll_interval < kionix_accel_grp4_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update DATA_CTRL_REG register if the ODR is not changed */
	if(acceld->accel_registers[accel_grp4_data_ctrl] == odr)
		return 0;
	else
		acceld->accel_registers[accel_grp4_data_ctrl] = odr;

	/* Do not need to update DATA_CTRL_REG register if the sensor is not currently turn on */
	if(atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1, 0);
		if (err < 0)
			return err;

		err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_DATA_CTRL, acceld->accel_registers[accel_grp4_data_ctrl]);
		if (err < 0)
			return err;

		err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1, acceld->accel_registers[accel_grp4_ctrl_reg1] | ACCEL_GRP4_PC1_ON);
		if (err < 0)
			return err;

		err = i2c_smbus_read_byte_data(acceld->client, ACCEL_GRP4_DATA_CTRL);
		if (err < 0)
			return err;
		switch(err) {
			case ACCEL_GRP4_ODR0_781:
				dev_info(&acceld->client->dev, "ODR = 0.781 Hz\n");
				break;
			case ACCEL_GRP4_ODR1_563:
				dev_info(&acceld->client->dev, "ODR = 1.563 Hz\n");
				break;
			case ACCEL_GRP4_ODR3_125:
				dev_info(&acceld->client->dev, "ODR = 3.125 Hz\n");
				break;
			case ACCEL_GRP4_ODR6_25:
				dev_info(&acceld->client->dev, "ODR = 6.25 Hz\n");
				break;
			case ACCEL_GRP4_ODR12_5:
				dev_info(&acceld->client->dev, "ODR = 12.5 Hz\n");
				break;
			case ACCEL_GRP4_ODR25:
				dev_info(&acceld->client->dev, "ODR = 25 Hz\n");
				break;
			case ACCEL_GRP4_ODR50:
				dev_info(&acceld->client->dev, "ODR = 50 Hz\n");
				break;
			case ACCEL_GRP4_ODR100:
				dev_info(&acceld->client->dev, "ODR = 100 Hz\n");
				break;
			case ACCEL_GRP4_ODR200:
				dev_info(&acceld->client->dev, "ODR = 200 Hz\n");
				break;
			case ACCEL_GRP4_ODR400:
				dev_info(&acceld->client->dev, "ODR = 400 Hz\n");
				break;
			case ACCEL_GRP4_ODR800:
				dev_info(&acceld->client->dev, "ODR = 800 Hz\n");
				break;
			case ACCEL_GRP4_ODR1600:
				dev_info(&acceld->client->dev, "ODR = 1600 Hz\n");
				break;
			default:
				dev_info(&acceld->client->dev, "Unknown ODR\n");
				break;
		}
	}
	KMSGINF(&acceld->client->dev, "kionix_accel_grp4_update_odr() \n");
	return 0;
}

static int kionix_accel_power_on(struct kionix_accel_driver *acceld)
{
	if (acceld->accel_pdata.power_on)
		return acceld->accel_pdata.power_on();

	return 0;
}

static void kionix_accel_power_off(struct kionix_accel_driver *acceld)
{
	if (acceld->accel_pdata.power_off)
		acceld->accel_pdata.power_off();
}

static irqreturn_t kionix_accel_isr(int irq, void *dev)
{
	struct kionix_accel_driver *acceld = dev;
	//printk("kionix_accel_isr ++\n");
	queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, 0);

	return IRQ_HANDLED;
}

static void kionix_accel_work(struct work_struct *work)
{
	struct kionix_accel_driver *acceld = container_of((struct delayed_work *)work,	struct kionix_accel_driver, accel_work);
	//printk("kionix_accel_work ++ drdy=%d\n",acceld->accel_drdy);
	if(acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, acceld->poll_delay);

	acceld->kionix_accel_report_accel_data(acceld);
}

static void kionix_accel_update_direction(struct kionix_accel_driver *acceld)
{
	unsigned int direction = acceld->accel_pdata.accel_direction;
	unsigned int accel_group = acceld->accel_group;

	write_lock(&acceld->rwlock_accel_data);
	acceld->axis_map_x = ((direction-1)%2);
	acceld->axis_map_y =  (direction%2);
	acceld->axis_map_z =  2;
	acceld->negate_z = ((direction-1)/4);
	switch(accel_group) {
		case KIONIX_ACCEL_GRP3:
		case KIONIX_ACCEL_GRP6:
			acceld->negate_x = (((direction+2)/2)%2);
			acceld->negate_y = (((direction+5)/4)%2);
			break;
		case KIONIX_ACCEL_GRP5:
			acceld->axis_map_x =  (direction%2);
			acceld->axis_map_y = ((direction-1)%2);
			acceld->negate_x =  (((direction+1)/2)%2);
			acceld->negate_y =  (((direction/2)+((direction-1)/4))%2);
			break;
		default:
			acceld->negate_x =  ((direction/2)%2);
			acceld->negate_y = (((direction+1)/4)%2);
			break;
	}
	write_unlock(&acceld->rwlock_accel_data);
	return;
}

static int kionix_accel_enable(struct kionix_accel_driver *acceld)
{
	int err = 0;
	long remaining;

	mutex_lock(&acceld->mutex_earlysuspend);

	atomic_set(&acceld->accel_suspend_continue, 0);

	/* Make sure that the sensor had successfully resumed before enabling it */
	if(atomic_read(&acceld->accel_suspended) == 1) {
		KMSGINF(&acceld->client->dev, "%s: waiting for resume\n", __func__);
		remaining = wait_event_interruptible_timeout(acceld->wqh_suspend, \
				atomic_read(&acceld->accel_suspended) == 0, \
				msecs_to_jiffies(KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT));

		if(atomic_read(&acceld->accel_suspended) == 1) {
			KMSGERR(&acceld->client->dev, "%s: timeout waiting for resume\n", __func__);
			err = -ETIME;
			goto exit;
		}
	}

	err = acceld->kionix_accel_operate(acceld);

	if (err < 0) {
		KMSGERR(&acceld->client->dev, \
				"%s: kionix_accel_operate returned err = %d\n", __func__, err);
		goto exit;
	}

	atomic_inc(&acceld->accel_enabled);

exit:
	mutex_unlock(&acceld->mutex_earlysuspend);

	return err;
}

static int kionix_accel_disable(struct kionix_accel_driver *acceld)
{
	int err = 0;

	mutex_lock(&acceld->mutex_resume);

	atomic_set(&acceld->accel_suspend_continue, 1);

	if(atomic_read(&acceld->accel_enabled) > 0){
		if(atomic_dec_and_test(&acceld->accel_enabled)) {
			if(atomic_read(&acceld->accel_enable_resume) > 0)
				atomic_set(&acceld->accel_enable_resume, 0);
			err = acceld->kionix_accel_standby(acceld);
			if (err < 0) {
				KMSGERR(&acceld->client->dev, \
						"%s: kionix_accel_standby returned err = %d\n", __func__, err);
				goto exit;
			}
			wake_up_interruptible(&acceld->wqh_suspend);
		}
	}

exit:
	mutex_unlock(&acceld->mutex_resume);

	return err;
}

static int kionix_accel_input_open(struct input_dev *input)
{
	struct kionix_accel_driver *acceld = input_get_drvdata(input);

	atomic_inc(&acceld->accel_input_event);

	return 0;
}

static void kionix_accel_input_close(struct input_dev *dev)
{
	struct kionix_accel_driver *acceld = input_get_drvdata(dev);

	atomic_dec(&acceld->accel_input_event);
}

static void kionix_accel_init_input_device(struct kionix_accel_driver *acceld,
					      struct input_dev *input_dev)
{
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, -ACCEL_G_MAX, ACCEL_G_MAX, ACCEL_FUZZ, ACCEL_FLAT);
	input_set_abs_params(input_dev, ABS_Y, -ACCEL_G_MAX, ACCEL_G_MAX, ACCEL_FUZZ, ACCEL_FLAT);
	input_set_abs_params(input_dev, ABS_Z, -ACCEL_G_MAX, ACCEL_G_MAX, ACCEL_FUZZ, ACCEL_FLAT);

	input_dev->name = KIONIX_ACCEL_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &acceld->client->dev;
}

static int kionix_accel_setup_input_device(struct kionix_accel_driver *acceld)
{
	struct input_dev *input_dev;
	int err;

	input_dev = input_allocate_device();
	if (!input_dev) {
		KMSGERR(&acceld->client->dev, "input_allocate_device failed\n");
		return -ENOMEM;
	}

	acceld->input_dev = input_dev;

	input_dev->open = kionix_accel_input_open;
	input_dev->close = kionix_accel_input_close;
	input_set_drvdata(input_dev, acceld);

	kionix_accel_init_input_device(acceld, input_dev);

	err = input_register_device(acceld->input_dev);
	if (err) {
		KMSGERR(&acceld->client->dev, \
				"%s: input_register_device returned err = %d\n", __func__, err);
		input_free_device(acceld->input_dev);
		return err;
	}

	return 0;
}

/* Returns the enable state of device */
static ssize_t kionix_accel_get_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&acceld->accel_enabled) > 0 ? 1 : 0);
}

/* Allow users to enable/disable the device */
static ssize_t kionix_accel_set_status_AP(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{

	return sprintf(buf, "%d\n", 0);
}


/* Returns the enable state of device */
static ssize_t kionix_accel_get_status_AP(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	int val = 0;

	switch(gKionixPosition){
		case 1:
			if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
				//printk(KERN_INFO"[KXTJ3] [AP-Down State]\n");		
				val = EBRMAIN_DWON ;
			}
			break;
		case 2:
			if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
				//printk(KERN_INFO"[KXTJ3] [AP-Up State]\n");
				val = EBRMAIN_UP ;
			}
			break;
		case 3:
			if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
				//printk(KERN_INFO"[KXTJ3] [AP-Right State]\n");
				val = EBRMAIN_RIGHT ;
			}
			break;
		case 4:
			if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
				//printk(KERN_INFO"[KXTJ3] [AP-Left State]\n");
				val = EBRMAIN_LEFT ;
			}
			break;
		case 5:
			break;
		case 6:
			break;
		default:
			break;
	}

	return sprintf(buf, "%d\n", val);
}

/* Allow users to enable/disable the device */
static ssize_t kionix_accel_set_enable(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	char *buf2;
	const int enable_count = 1;
	unsigned long enable;
	int err = 0;

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if(kionix_strtok(buf, count, &buf2, enable_count) < 0) {
		KMSGERR(&acceld->client->dev, \
				"%s: No enable data being read. " \
				"No enable data will be updated.\n", __func__);
	}

	else {
		/* Removes any leading negative sign */
		while(*buf2 == '-')
			buf2++;
		#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
		err = kstrtouint((const char *)buf2, 10, (unsigned int *)&enable);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, \
					"%s: kstrtouint returned err = %d\n", __func__, err);
			goto exit;
		}
		#else
		err = strict_strtoul((const char *)buf2, 10, &enable);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, \
					"%s: strict_strtoul returned err = %d\n", __func__, err);
			goto exit;
		}
		#endif

		if(enable)
			err = kionix_accel_enable(acceld);
		else
			err = kionix_accel_disable(acceld);
	}

exit:
	mutex_unlock(&input_dev->mutex);

	return (err < 0) ? err : count;
}

/* Returns currently selected poll interval (in ms) */
static ssize_t kionix_accel_get_delay(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", acceld->poll_interval);
}

/* Allow users to select a new poll interval (in ms) */
static ssize_t kionix_accel_set_delay(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	char *buf2;
	const int delay_count = 1;
	unsigned long interval;
	int err = 0;
       KMSGINF(&acceld->client->dev, "kionix_accel_set_delay() ++\n");
	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if(kionix_strtok(buf, count, &buf2, delay_count) < 0) {
		KMSGERR(&acceld->client->dev, \
				"%s: No delay data being read. " \
				"No delay data will be updated.\n", __func__);
	}

	else {
		/* Removes any leading negative sign */
		while(*buf2 == '-')
			buf2++;
		#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
		err = kstrtouint((const char *)buf2, 10, (unsigned int *)&interval);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, \
					"%s: kstrtouint returned err = %d\n", __func__, err);
			goto exit;
		}
		#else
		err = strict_strtoul((const char *)buf2, 10, &interval);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, \
					"%s: strict_strtoul returned err = %d\n", __func__, err);
			goto exit;
		}
		#endif

		if(acceld->accel_drdy == 1)
			disable_irq(client->irq);

		/*
		 * Set current interval to the greater of the minimum interval or
		 * the requested interval
		 */
		acceld->poll_interval = max((unsigned int)interval, acceld->accel_pdata.min_interval);
		acceld->poll_delay = msecs_to_jiffies(acceld->poll_interval);

		err = acceld->kionix_accel_update_odr(acceld, acceld->poll_interval);

		if(acceld->accel_drdy == 1)
			enable_irq(client->irq);
	}

exit:
	mutex_unlock(&input_dev->mutex);
       KMSGINF(&acceld->client->dev, "kionix_accel_set_delay() --poll_interval= %d\n",acceld->poll_interval);
	return (err < 0) ? err : count;
}

/* Returns the direction of device */
static ssize_t kionix_accel_get_direct(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", acceld->accel_pdata.accel_direction);
}

/* Allow users to change the direction the device */
static ssize_t kionix_accel_set_direct(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	char *buf2;
	const int direct_count = 1;
	unsigned long direction;
	int err = 0;

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if(kionix_strtok(buf, count, &buf2, direct_count) < 0) {
		KMSGERR(&acceld->client->dev, \
				"%s: No direction data being read. " \
				"No direction data will be updated.\n", __func__);
	}

	else {
		/* Removes any leading negative sign */
		while(*buf2 == '-')
			buf2++;
		#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
		err = kstrtouint((const char *)buf2, 10, (unsigned int *)&direction);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, \
					"%s: kstrtouint returned err = %d\n", __func__, err);
			goto exit;
		}
		#else
		err = strict_strtoul((const char *)buf2, 10, &direction);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, \
					"%s: strict_strtoul returned err = %d\n", __func__, err);
			goto exit;
		}
		#endif

		if(direction < 1 || direction > 8)
			KMSGERR(&acceld->client->dev, "%s: invalid direction = %d\n", __func__, (unsigned int) direction);

		else {
			acceld->accel_pdata.accel_direction = (u8) direction;
			kionix_accel_update_direction(acceld);
		}
	}

exit:
	mutex_unlock(&input_dev->mutex);

	return (err < 0) ? err : count;
}

/* Returns the data output of device */
static ssize_t kionix_accel_get_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	int x, y, z;

	read_lock(&acceld->rwlock_accel_data);

	x = acceld->accel_data[acceld->axis_map_x];
	y = acceld->accel_data[acceld->axis_map_y];
	z = acceld->accel_data[acceld->axis_map_z];

	read_unlock(&acceld->rwlock_accel_data);

	return sprintf(buf, "%d %d %d\n", x, y, z);
}

/* Returns the calibration value of the device */
static ssize_t kionix_accel_get_cali(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	int calibration[3];

	read_lock(&acceld->rwlock_accel_data);

	calibration[0] = acceld->accel_cali[acceld->axis_map_x];
	calibration[1] = acceld->accel_cali[acceld->axis_map_y];
	calibration[2] = acceld->accel_cali[acceld->axis_map_z];

	read_unlock(&acceld->rwlock_accel_data);

	return sprintf(buf, "%d %d %d\n", calibration[0], calibration[1], calibration[2]);
}

/* Allow users to change the calibration value of the device */
static ssize_t kionix_accel_set_cali(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	const int cali_count = 3; /* How many calibration that we expect to get from the string */
	char **buf2;
	long calibration[cali_count];
	int err = 0, i = 0;

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	buf2 = (char **)kzalloc(cali_count * sizeof(char *), GFP_KERNEL);

	if(kionix_strtok(buf, count, buf2, cali_count) < 0) {
		KMSGERR(&acceld->client->dev, \
				"%s: Not enough calibration data being read. " \
				"No calibration data will be updated.\n", __func__);
	}
	else {
		/* Convert string to integers  */
		for(i = 0 ; i < cali_count ; i++) {
			#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
			err = kstrtoint((const char *)*(buf2+i), 10, (int *)&calibration[i]);
			if(err < 0) {
				KMSGERR(&acceld->client->dev, \
						"%s: kstrtoint returned err = %d." \
						"No calibration data will be updated.\n", __func__ , err);
				goto exit;
			}
			#else
			err = strict_strtol((const char *)*(buf2+i), 10, &calibration[i]);
			if(err < 0) {
				KMSGERR(&acceld->client->dev, \
						"%s: strict_strtol returned err = %d." \
						"No calibration data will be updated.\n", __func__ , err);
				goto exit;
			}
			#endif
		}

		write_lock(&acceld->rwlock_accel_data);

		acceld->accel_cali[acceld->axis_map_x] = (int)calibration[0];
		acceld->accel_cali[acceld->axis_map_y] = (int)calibration[1];
		acceld->accel_cali[acceld->axis_map_z] = (int)calibration[2];

		write_unlock(&acceld->rwlock_accel_data);
	}

exit:
	for(i = 0 ; i < cali_count ; i++)
		kfree(*(buf2+i));

	kfree(buf2);

	mutex_unlock(&input_dev->mutex);

	return (err < 0) ? err : count;
}

static DEVICE_ATTR(status_AP,S_IRUGO| S_IWUSR,kionix_accel_get_status_AP,kionix_accel_set_status_AP);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR, kionix_accel_get_enable, kionix_accel_set_enable);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR, kionix_accel_get_delay, kionix_accel_set_delay);
static DEVICE_ATTR(direct, S_IRUGO|S_IWUSR, kionix_accel_get_direct, kionix_accel_set_direct);
static DEVICE_ATTR(data, S_IRUGO, kionix_accel_get_data, NULL);
static DEVICE_ATTR(cali, S_IRUGO|S_IWUSR, kionix_accel_get_cali, kionix_accel_set_cali);

static struct attribute *kionix_accel_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_direct.attr,
	&dev_attr_data.attr,
	&dev_attr_cali.attr,
	&dev_attr_status_AP.attr,
	NULL
};

static struct attribute_group kionix_accel_attribute_group = {
	.attrs = kionix_accel_attributes
};

static int kionix_verify(struct kionix_accel_driver *acceld)
{
	int retval = i2c_smbus_read_byte_data(acceld->client, ACCEL_WHO_AM_I);

#if KIONIX_KMSG_INF
	switch (retval) {
		case KIONIX_ACCEL_WHO_AM_I_KXTE9:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXTE9.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTF9:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXTF9.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTI9_1001:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXTI9-1001.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTIK_1004:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXTIK-1004.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXTJ9-1005.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXTJ9-1007.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXCJ9-1008.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXTJ2-1009.\n");
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTJ3_1057:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXTJ3-1057.\n");
			break;		
		case KIONIX_ACCEL_WHO_AM_I_KXCJK_1013:
			KMSGINF(&acceld->client->dev, "this accelerometer is a KXCJK-1013.\n");
			break;
		default:
			break;
	}
#endif

	return retval;
}

#ifdef    CONFIG_HAS_EARLYSUSPEND
void kionix_accel_earlysuspend_suspend(struct early_suspend *h)
{
	struct kionix_accel_driver *acceld = container_of(h, struct kionix_accel_driver, early_suspend);
	long remaining;

	mutex_lock(&acceld->mutex_earlysuspend);

	/* Only continue to suspend if enable did not intervene */
	if(atomic_read(&acceld->accel_suspend_continue) > 0) {
		/* Make sure that the sensor had successfully disabled before suspending it */
		if(atomic_read(&acceld->accel_enabled) > 0) {
			KMSGINF(&acceld->client->dev, "%s: waiting for disable\n", __func__);
			remaining = wait_event_interruptible_timeout(acceld->wqh_suspend, \
					atomic_read(&acceld->accel_enabled) < 1, \
					msecs_to_jiffies(KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT));

			if(atomic_read(&acceld->accel_enabled) > 0) {
				KMSGERR(&acceld->client->dev, "%s: timeout waiting for disable\n", __func__);
			}
		}

		kionix_accel_power_off(acceld);

		atomic_set(&acceld->accel_suspended, 1);
	}

	mutex_unlock(&acceld->mutex_earlysuspend);

	return;
}

void kionix_accel_earlysuspend_resume(struct early_suspend *h)
{
	struct kionix_accel_driver *acceld = container_of(h, struct kionix_accel_driver, early_suspend);
	int err;

	mutex_lock(&acceld->mutex_resume);

	if(atomic_read(&acceld->accel_suspended) == 1) {
		err = kionix_accel_power_on(acceld);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, "%s: kionix_accel_power_on returned err = %d\n", __func__, err);
			goto exit;
		}

		/* Only needs to reinitialized the registers if Vdd is pulled low during suspend */
		if(err > 0) {
			err = acceld->kionix_accel_power_on_init(acceld);
			if (err) {
				KMSGERR(&acceld->client->dev, "%s: kionix_accel_power_on_init returned err = %d\n", __func__, err);
				goto exit;
			}
		}

		atomic_set(&acceld->accel_suspended, 0);
	}

	wake_up_interruptible(&acceld->wqh_suspend);

exit:
	mutex_unlock(&acceld->mutex_resume);

	return;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int kionix_proc_read(char *buffer, char **buffer_location, off_t offset, int buffer_length, int *eof, void *data )
{
	int ret = 0 ;
	 return ret;
}

static int kionix_proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
	return count; // procfs_buffer_size;
}

static s32 kionix_reg_read_byte(struct kionix_accel_driver *sdata, u8 reg)
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

#define Threshould 8	// 本來是0.8因為無法使用float , 乘以10
static int readAccel(struct kionix_accel_driver *acceld,int position)
{
	int16_t l,h;
	int16_t outRAW;
	int value;

	if (position==0) {
		l=kionix_reg_read_byte(acceld, ACCEL_GRP4_XOUT_L);
		h=kionix_reg_read_byte(acceld, ACCEL_GRP4_XOUT_H);
	}
	else if (position==1) {
		l=kionix_reg_read_byte(acceld, ACCEL_GRP4_YOUT_L);
		h=kionix_reg_read_byte(acceld, ACCEL_GRP4_YOUT_H);
	}
	else if (position==2) {
		l=kionix_reg_read_byte(acceld, ACCEL_GRP4_ZOUT_L);
		h=kionix_reg_read_byte(acceld, ACCEL_GRP4_ZOUT_H);
	}
	outRAW = l + (h << 8 );

	if (gAccelRange==2) 
		value = outRAW * 10 / 15987;
	else if (gAccelRange==4)
		value = outRAW * 10 / 7840;
	else if (gAccelRange==8)
		value = outRAW * 10 / 3883;
	else if (gAccelRange==16)
		value = outRAW * 10 / 1280;
	
	//printk(KERN_ERR"[%d] , outRaw:%d , value:%d \n",position,outRAW,value);
	return  value ;
}


static irqreturn_t kionix_irq_handler(int irq, void *dev)
{
	struct kionix_accel_driver *acceld = dev;
	int err = 0 ;

	err = kionix_reg_read_byte(acceld, KIONIX_INT_SOURCE1);
	if(err < 0)
	{
		printk(KERN_ERR"[%s_%d] read KIONIX_INT_SOURCE1 failed !!\n",__FUNCTION__,__LINE__);
	}
	if( (err >> 4 ) & 1 )
	{
//		printk("[DEBUG] DRDY was 1 \n");
	} 
	else if( (err >> 1 ) & 1 )
	{
		int Position = 0;
		int   ACC_X , ACC_Y , ACC_Z ;
		//printk("[DEBUG] WUFS was 1 \n");


		ACC_X = readAccel(acceld,0);
		ACC_Y = readAccel(acceld,1);
		ACC_Z = readAccel(acceld,2);


		if ( ACC_X > Threshould ) Position = 1;
		if ( ACC_X < -Threshould ) Position = 2;
		if ( ACC_Y > Threshould ) Position = 3;
		if ( ACC_Y < -Threshould ) Position = 4;
		if ( ACC_Z > Threshould ) Position = 5;
		if ( ACC_Z < -Threshould ) Position = 6;

		switch(Position){
			case 1 :
				if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
					printk(KERN_INFO"==== [down State] ====\n");		
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_DOWN);	
				}
				break;
			case 2 :
				if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
					printk(KERN_INFO"==== [Up State] ====\n");		
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_PORTRAIT_UP);	
				}
				break;
			case 3 :
				if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
					printk(KERN_INFO"==== [Right State] ====\n");		
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_RIGHT);	
				}
				break;
			case 4 :
				if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
					printk(KERN_INFO"==== [Left State] ====\n");	
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_LANDSCAPE_LEFT);
				}
				break;
			case 5 :
				if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
					printk(KERN_INFO"==== [Face down] ====\n");
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_BACK);	
				}
				break;
			case 6 :
				if(0x65 == gptHWCFG->m_val.bPCB ) { // E70K14
					printk(KERN_INFO"==== [Face Up] ====\n");
					ntx_report_event(EV_MSC,MSC_RAW,MSC_RAW_GSENSOR_FRONT);	
				}
				break;
		}

		if(Position > 0)
			gKionixPosition = Position;

		printk("[DEBUG] Position:%d\n",Position);

	} 


	err = kionix_reg_read_byte(acceld, ACCEL_GRP4_INT_REL);
//	printk("[DEBUG] INT_REL :%d \n",err);
	return IRQ_HANDLED;
}

static s32 kionix_reg_write_byte(struct kionix_accel_driver *sdata, u8 reg, u8 value)
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



static int kionix_reg_set_bit(struct kionix_accel_driver *sdata, u8 reg, u8 bits)
{
	s32 reg_val;
	reg_val = kionix_reg_read_byte(sdata, reg);

	if (reg_val < 0)
		return reg_val;

	reg_val |= bits;

	reg_val = kionix_reg_write_byte(sdata, reg, reg_val);

	if (reg_val < 0)
		return reg_val;

	return 0;
}

static int kionix_reg_reset_bit(struct kionix_accel_driver *sdata,
		u8 reg, u8 bits)
{
	s32 reg_val;

	reg_val = kionix_reg_read_byte(sdata, reg);
	if (reg_val < 0)
		return reg_val;

	reg_val &= ~bits;
	reg_val = kionix_reg_write_byte(sdata, reg, reg_val);

	if (reg_val < 0)
		return reg_val;

	return 0;
}

/**********************************************************
 * Write kionix's register 
**********************************************************/
static int kionix_set_reg(struct kionix_accel_driver *acceld)
{
	int err,val;

	err = kionix_reg_write_byte(acceld,ACCEL_GRP4_DATA_CTRL, acceld->accel_registers[accel_grp4_data_ctrl]);
	if(err)
		printk(KERN_ERR"[%s_%d] Write DATA_CTRL Failed\n",__FUNCTION__,__LINE__);

	err = kionix_reg_write_byte(acceld,ACCEL_GRP4_INT_CTRL1, acceld->accel_registers[accel_grp4_int_ctrl]);
	if(err)
		printk(KERN_ERR"[%s_%d] Write INT_CTRL Failed\n",__FUNCTION__,__LINE__);

	err = kionix_reg_write_byte(acceld,ACCEL_GRP4_CTRL_REG1, acceld->accel_registers[accel_grp4_ctrl_reg1]);
	if(err)
		printk(KERN_ERR"[%s_%d] Write CTRL_REG1 Failed\n",__FUNCTION__,__LINE__);


	err = kionix_reg_write_byte(acceld,KIONIX_WAKEUP_THRESH_HOLD_H, 0x01);
	if(err)
		printk(KERN_ERR"[%s_%d] Write KIONIX_WAKEUP_THRESH_HOLD_H Failed\n",__FUNCTION__,__LINE__);

	msleep(5);


	// Print Register 
	val = kionix_reg_read_byte(acceld, ACCEL_GRP4_INT_CTRL1);
	printk(KERN_ERR"[kionix-INT_CTRL1]:%x \n",val);

	val = kionix_reg_read_byte(acceld, ACCEL_GRP4_CTRL_REG1);
	printk(KERN_ERR"[kionix-CTRL_REG1]:%x \n",val);

	val = kionix_reg_read_byte(acceld, ACCEL_GRP4_DATA_CTRL);
	printk(KERN_ERR"[kionix-DATA_CTRL1]:%x \n",val);

	switch(val) {
		case ACCEL_GRP4_ODR0_781:KMSGINF(&acceld->client->dev, "ODR = 0.781 Hz\n");	break;
		case ACCEL_GRP4_ODR1_563:KMSGINF(&acceld->client->dev, "ODR = 1.563 Hz\n");break;
		case ACCEL_GRP4_ODR3_125:KMSGINF(&acceld->client->dev, "ODR = 3.125 Hz\n");break;
		case ACCEL_GRP4_ODR6_25:KMSGINF(&acceld->client->dev, "ODR = 6.25 Hz\n");break;
		case ACCEL_GRP4_ODR12_5:KMSGINF(&acceld->client->dev, "ODR = 12.5 Hz\n");break;
		case ACCEL_GRP4_ODR25:KMSGINF(&acceld->client->dev, "ODR = 25 Hz\n");break;
		case ACCEL_GRP4_ODR50:KMSGINF(&acceld->client->dev, "ODR = 50 Hz\n");break;
		case ACCEL_GRP4_ODR100:KMSGINF(&acceld->client->dev, "ODR = 100 Hz\n");break;
		case ACCEL_GRP4_ODR200:KMSGINF(&acceld->client->dev, "ODR = 200 Hz\n");break;
		case ACCEL_GRP4_ODR400:	KMSGINF(&acceld->client->dev, "ODR = 400 Hz\n");break;
		case ACCEL_GRP4_ODR800:	KMSGINF(&acceld->client->dev, "ODR = 800 Hz\n");break;
		case ACCEL_GRP4_ODR1600:KMSGINF(&acceld->client->dev, "ODR = 1600 Hz\n");break;
		default:KMSGINF(&acceld->client->dev, "Unknown ODR\n");break;
	}


	val = kionix_reg_read_byte(acceld, KIONIX_WAKEUP_THRESH_HOLD_H);
	printk(KERN_ERR"[kionix-KIONIX_WAKEUP_THRESH_HOLD_H]:%x \n",val);


	return 0 ;
}

/**********************************************************
 * Enable IRQ (Set PC1 to 1) 
**********************************************************/
static void kionix_enable_irq(struct kionix_accel_driver *sdata)
{
	kionix_reg_reset_bit(sdata,ACCEL_GRP4_CTRL_REG1,KIONIX_CTRL_REG_PC1);		// SET PC1 to 0
	msleep(5);
	//enable_irq(sdata->irq1);
	kionix_reg_set_bit(sdata,ACCEL_GRP4_CTRL_REG1,KIONIX_CTRL_REG_PC1);
	msleep(5);
}

/**********************************************************
 * Disable IRQ (Set PC1 to 0) 
**********************************************************/
static void kionix_disable_irq(struct kionix_accel_driver *sdata)
{
	kionix_reg_reset_bit(sdata,ACCEL_GRP4_CTRL_REG1,KIONIX_CTRL_REG_PC1);		// SET PC1 to 0
}

/**********************************************************
 * 
**********************************************************/
static int kionix_parse_dt(struct kionix_accel_driver *sdata, struct device *dev)
{
	u32 temp_val;
	int err;

#if 0 // Disable  by Ray
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
#endif

	/* optional dt parameters i.e. use poll if gpio-int not found */
	sdata->accel_pdata.gpio_int1 = of_get_named_gpio_flags(dev->of_node,
		"kionix,gpio-int1", 0, NULL);

	sdata->accel_pdata.gpio_int2 = of_get_named_gpio_flags(dev->of_node,
		"kionix,gpio-int2", 0, NULL);

	sdata->accel_pdata.use_drdy_int = of_property_read_bool(dev->of_node,
		"kionix,use-drdy-int");

	sdata->accel_pdata.accel_irq_use_drdy = sdata->accel_pdata.use_drdy_int ;
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

void kionix_irq1_wake_enable(struct kionix_accel_driver *pdata,int enable)
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
static int kionix_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
    struct kionix_accel_driver *pdata = i2c_get_clientdata(client);	

	printk("%s(%d),sleep=%d,irq1=%d\n",__FUNCTION__,__LINE__,gSleep_Mode_Suspend,pdata->irq1);
	msleep(5);
	if (gSleep_Mode_Suspend) {
		kionix_disable_irq(pdata);
#ifdef NO_FREE_IRQ//[
		kionix_irq1_wake_enable(pdata,0);
#else //][!NO_FREE_IRQ
		free_irq(pdata->irq1, pdata);
#endif //] NO_FREE_IRQ
	}
	else {
		if(pdata->enable_wake) {
			printk("kionix_suspend,enable irq wakeup source %d\n",pdata->irq1);
			kionix_irq1_wake_enable(pdata,1);
		}
		else {
			kionix_irq1_wake_enable(pdata,0);
		}
	}
	return 0;
}

static int kionix_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *pdata = i2c_get_clientdata(client);
	int err = 0;

	printk("%s(%d),sleep=%d,irq1=%d\n",__FUNCTION__,__LINE__,gSleep_Mode_Suspend,pdata->irq1);

	if (gSleep_Mode_Suspend) {
		int ret; 
		kionix_set_reg(pdata);
#ifdef NO_FREE_IRQ//[
		kionix_irq1_wake_enable(pdata,1);
#else //][!NO_FREE_IRQ

#ifdef EN_THREAD_IRQ //[
		ret = request_threaded_irq(pdata->irq1,NULL,kionix_irq_handler,IRQF_TRIGGER_RISING | IRQF_ONESHOT,KIONIX_ACCEL_IRQ,pdata);
#else //][!EN_THREAD_IRQ
		ret = request_irq(pdata->irq1,kx122_interrupt,IRQF_TRIGGER_RISING | IRQF_ONESHOT,KX122_NAME,pdata);
#endif //] EN_THREAD_IRQ
		if (err) {
			dev_err(&client->dev, "unable to request irq1\n");
			//cancel_delayed_work_sync (&kx122_delay_work);
			printk(KERN_ERR"[%s_%d] Request irq failed \n",__FUNCTION__,__LINE__);
		}
#endif //] NO_FREE_IRQ
		kionix_enable_irq(pdata);
	}
	else {
		//schedule_delayed_work(&kx122_delay_work, 0);
		if(pdata->enable_wake) {
			printk("kx122_resume,disable irq wakeup source %d\n",pdata->irq1);
			kionix_irq1_wake_enable(pdata,0);
		}
		else {
			kionix_irq1_wake_enable(pdata,1);
		}
	}
	return 0;
}
#else
#define kionix_suspend	NULL
#define kionix_resume	NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int kionix_runtime_suspend(struct device *dev)
{
	return 0;
}

static int kionix_runtime_resume(struct device *dev)
{
	return 0;
}
#else
#define kionix_runtime_suspend	NULL
#define kionix_runtime_resume	NULL
#endif



struct file_operations kionix_accel_proc_fops = {
	.owner = THIS_MODULE,
	.read = kionix_proc_read,
	.write = kionix_proc_write
};


static int kionix_accel_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	const struct kionix_accel_platform_data *accel_pdata = client->dev.platform_data;
	struct kionix_accel_driver *acceld;
	int err;
	struct proc_dir_entry *proc_dir, *proc_entry;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA)) {
		KMSGERR(&client->dev, "client is not i2c capable. Abort.\n");
		return -ENXIO;
	}

#if 0
	if (!accel_pdata) {
		KMSGERR(&client->dev, "platform data is NULL. Abort.\n");
		return -EINVAL;
	}
#endif
	acceld = kzalloc(sizeof(*acceld), GFP_KERNEL);
	if (acceld == NULL) {
		KMSGERR(&client->dev, \
			"failed to allocate memory for module data. Abort.\n");
		return -ENOMEM;
	}

#if 0
	acceld = devm_kzalloc(&client->dev,sizeof(struct kionix_accel_driver), GFP_KERNEL);
	if (!acceld) {
		dev_err(&client->dev, "no memory available\n");
		return -ENOMEM;
	}
#endif

	acceld->client = client;
	if (accel_pdata)
		acceld->accel_pdata = *accel_pdata;

	i2c_set_clientdata(client, acceld);

	/* get driver platform data */
	if (client->dev.of_node) {
		err = kionix_parse_dt(acceld, &client->dev);
		if (err) {
			dev_err(&client->dev,
				"Unable to parse dt data err=%d\n", err);
			err = -EINVAL;
			goto err_free_mem;
		}
	}
	else {
		dev_err(&client->dev,"platform data is NULL\n");
		err = -EINVAL;
		goto err_free_mem;
	}

	/* gpio to irq */
	if (acceld->accel_pdata.use_drdy_int && gpio_is_valid(acceld->accel_pdata.gpio_int1)) {
		err = gpio_request(acceld->accel_pdata.gpio_int1, "kx122_int1");
		if (err) {
			dev_err(&client->dev,
				"Unable to request interrupt gpio1 %d\n",
				acceld->accel_pdata.gpio_int1);
			goto err_free_mem;
		}

		err = gpio_direction_input(acceld->accel_pdata.gpio_int1);
		if (err) {
			dev_err(&client->dev,
				"Unable to set direction for gpio1 %d\n",
				acceld->accel_pdata.gpio_int1);
			goto err_free_mem;
		}
		acceld->irq1 = gpio_to_irq(acceld->accel_pdata.gpio_int1);
		acceld->client->irq = acceld->irq1 ;
		printk(KERN_ERR"[%s_%d] irq1:%d , gpio1:%d \n",__FUNCTION__,__LINE__,acceld->irq1,acceld->accel_pdata.gpio_int1);
	}

	err = kionix_accel_power_on(acceld);
	if (err < 0)
		goto err_free_mem;

	if (acceld->accel_pdata.init) {
		err = acceld->accel_pdata.init();
		if (err < 0)
			goto err_accel_pdata_power_off;
	}

	err = kionix_verify(acceld);
	if (err < 0) {
		KMSGERR(&acceld->client->dev, "%s: kionix_verify returned err = %d. Abort.\n", __func__, err);
		goto err_accel_pdata_exit;
	}
	printk(KERN_ERR"[%s_%d] WHO_AM_I %d \n",__FUNCTION__,__LINE__,err);
	/* Setup group specific configuration and function callback */
	switch (err) {
		case KIONIX_ACCEL_WHO_AM_I_KXTE9:
			acceld->accel_group = KIONIX_ACCEL_GRP1;
			acceld->accel_registers = kzalloc(sizeof(u8)*accel_grp1_regs_count, GFP_KERNEL);
			if (acceld->accel_registers == NULL) {
				KMSGERR(&client->dev, \
					"failed to allocate memory for accel_registers. Abort.\n");
				goto err_accel_pdata_exit;
			}
			acceld->accel_drdy = 0;
			acceld->kionix_accel_report_accel_data	= kionix_accel_grp1_report_accel_data;
			acceld->kionix_accel_update_odr			= kionix_accel_grp1_update_odr;
			acceld->kionix_accel_power_on_init		= kionix_accel_grp1_power_on_init;
			acceld->kionix_accel_operate			= kionix_accel_grp1_operate;
			acceld->kionix_accel_standby			= kionix_accel_grp1_standby;
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTF9:
		case KIONIX_ACCEL_WHO_AM_I_KXTI9_1001:
		case KIONIX_ACCEL_WHO_AM_I_KXTIK_1004:
		case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005:
			if(err == KIONIX_ACCEL_WHO_AM_I_KXTIK_1004)
				acceld->accel_group = KIONIX_ACCEL_GRP3;
			else
				acceld->accel_group = KIONIX_ACCEL_GRP2;
			acceld->accel_registers = kzalloc(sizeof(u8)*accel_grp2_regs_count, GFP_KERNEL);
			if (acceld->accel_registers == NULL) {
				KMSGERR(&client->dev, \
					"failed to allocate memory for accel_registers. Abort.\n");
				goto err_accel_pdata_exit;
			}
			switch(acceld->accel_pdata.accel_res) {
				case KIONIX_ACCEL_RES_6BIT:
				case KIONIX_ACCEL_RES_8BIT:
					acceld->accel_registers[accel_grp2_ctrl_reg1] |= ACCEL_GRP2_RES_8BIT;
					break;
				case KIONIX_ACCEL_RES_12BIT:
				default:
					acceld->accel_registers[accel_grp2_ctrl_reg1] |= ACCEL_GRP2_RES_12BIT;
					break;
			}
			if(acceld->accel_pdata.accel_irq_use_drdy && client->irq) {
				acceld->accel_registers[accel_grp2_int_ctrl] |= ACCEL_GRP2_IEN | ACCEL_GRP2_IEA;
				acceld->accel_registers[accel_grp2_ctrl_reg1] |= ACCEL_GRP2_DRDYE;
				acceld->accel_drdy = 1;
			}
			else
				acceld->accel_drdy = 0;
			kionix_accel_grp2_update_g_range(acceld);
			acceld->kionix_accel_report_accel_data	= kionix_accel_grp2_report_accel_data;
			acceld->kionix_accel_update_odr			= kionix_accel_grp2_update_odr;
			acceld->kionix_accel_power_on_init		= kionix_accel_grp2_power_on_init;
			acceld->kionix_accel_operate			= kionix_accel_grp2_operate;
			acceld->kionix_accel_standby			= kionix_accel_grp2_standby;
			break;
		case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007:
		case KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008:
		case KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009:
		case KIONIX_ACCEL_WHO_AM_I_KXTJ3_1057:	
		case KIONIX_ACCEL_WHO_AM_I_KXCJK_1013:
			if(err == KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009)
				acceld->accel_group = KIONIX_ACCEL_GRP5;
			else if(err == KIONIX_ACCEL_WHO_AM_I_KXTJ3_1057)	
				acceld->accel_group = KIONIX_ACCEL_GRP5;
			else if(err == KIONIX_ACCEL_WHO_AM_I_KXCJK_1013)
				acceld->accel_group = KIONIX_ACCEL_GRP6;
			else
				acceld->accel_group = KIONIX_ACCEL_GRP4;
			acceld->accel_registers = kzalloc(sizeof(u8)*accel_grp4_regs_count, GFP_KERNEL);
			if (acceld->accel_registers == NULL) {
				KMSGERR(&client->dev, \
					"failed to allocate memory for accel_registers. Abort.\n");
				goto err_accel_pdata_exit;
			}
			switch(acceld->accel_pdata.accel_res) {
				case KIONIX_ACCEL_RES_6BIT:
				case KIONIX_ACCEL_RES_8BIT:
					acceld->accel_registers[accel_grp4_ctrl_reg1] |= ACCEL_GRP4_RES_8BIT;
					break;
				case KIONIX_ACCEL_RES_12BIT:
				default:
					acceld->accel_registers[accel_grp4_ctrl_reg1] |= ACCEL_GRP4_RES_12BIT;
					break;
			}
			if(acceld->accel_pdata.accel_irq_use_drdy && client->irq) {
			//if (true){//added by Eason
				acceld->accel_registers[accel_grp4_int_ctrl] |= ACCEL_GRP4_IEN | ACCEL_GRP4_IEA;
				//acceld->accel_registers[accel_grp4_ctrl_reg1] |= ACCEL_GRP4_DRDYE;
				acceld->accel_registers[accel_grp4_ctrl_reg1] |= ACCEL_GRP4_WUFS;
				acceld->accel_drdy = 1;
				printk("Probe() init accel_register value\n");
			}
			else
				acceld->accel_drdy = 0;
			kionix_accel_grp4_update_g_range(acceld);
			acceld->kionix_accel_report_accel_data	= kionix_accel_grp4_report_accel_data;
			acceld->kionix_accel_update_odr			= kionix_accel_grp4_update_odr;
			acceld->kionix_accel_power_on_init		= kionix_accel_grp4_power_on_init;
			acceld->kionix_accel_operate			= kionix_accel_grp4_operate;
			acceld->kionix_accel_standby			= kionix_accel_grp4_standby;
			break;
		default:
			KMSGERR(&acceld->client->dev, \
					"%s: unsupported device, who am i = %d. Abort.\n", __func__, err);
			goto err_accel_pdata_exit;
	}

	err = kionix_accel_setup_input_device(acceld);
	if (err)
		goto err_free_accel_registers;

	atomic_set(&acceld->accel_suspended, 0);
	atomic_set(&acceld->accel_suspend_continue, 1);
	atomic_set(&acceld->accel_enabled, 1);
	atomic_set(&acceld->accel_input_event, 1);
	atomic_set(&acceld->accel_enable_resume, 0);

	mutex_init(&acceld->mutex_earlysuspend);
	mutex_init(&acceld->mutex_resume);
	rwlock_init(&acceld->rwlock_accel_data);

	acceld->poll_interval = acceld->accel_pdata.poll_interval;
	acceld->poll_delay = msecs_to_jiffies(acceld->poll_interval);
	printk("[DEBUG] poll_interval:%d \n",acceld->poll_interval);
	//acceld->kionix_accel_update_odr(acceld, acceld->poll_interval);
	kionix_accel_update_direction(acceld);

	// Set Register
	acceld->accel_registers[accel_grp4_ctrl_reg1] |= ACCEL_GRP4_G_8G ;
	gAccelRange = 8 ;
	acceld->accel_registers[accel_grp4_data_ctrl] = ACCEL_GRP4_ODR200;//ACCEL_GRP4_ODR25	;

	// Write
	kionix_set_reg(acceld);

	proc_dir = proc_mkdir("sensors", NULL);
	if (proc_dir == NULL)
		KMSGERR(&client->dev, "failed to create /proc/sensors\n");
	else {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0))
		proc_create("accelinfo", 0644, NULL, &kionix_accel_proc_fops);
#else
		proc_entry = create_proc_entry( "accelinfo", 0644, proc_dir);
		if (proc_entry == NULL)
			KMSGERR(&client->dev, "failed to create /proc/cpu/accelinfo\n");
#endif
	}

	acceld->accel_workqueue = create_workqueue("Kionix Accel Workqueue");
	INIT_DELAYED_WORK(&acceld->accel_work, kionix_accel_work);
	init_waitqueue_head(&acceld->wqh_suspend);
	
	//acceld->accel_drdy = 1;//added by Eason
	if (acceld->accel_drdy) {
		
		err = request_threaded_irq(acceld->client->irq, NULL,kionix_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,KIONIX_ACCEL_IRQ, acceld);
		//err = request_irq(acceld->irq1, kionix_accel_isr, IRQF_TRIGGER_RISING, KIONIX_ACCEL_IRQ, acceld);

		if (err) {
			KMSGERR(&acceld->client->dev, "%s: request_irq returned err = %d, irq = %d \n", __func__, err, acceld->client->irq);
			KMSGERR(&acceld->client->dev, "%s: running in software polling mode instead\n", __func__);
			acceld->accel_drdy = 0;
		}
		else{
			KMSGINF(&acceld->client->dev, "running in hardware interrupt mode\n");
			KMSGERR(&acceld->client->dev, "%s: ok request_irq returned err = %d, irq = %d \n", __func__, err, acceld->client->irq);
		}	
	} else {
		KMSGINF(&acceld->client->dev, "running in software polling mode\n");
	}

#if 0
	err = acceld->kionix_accel_power_on_init(acceld);
	if (err) {
		KMSGERR(&acceld->client->dev, "%s: kionix_accel_power_on_init returned err = %d. Abort.\n", __func__, err);
		goto err_free_irq;
	}
#endif

	err = sysfs_create_group(&client->dev.kobj, &kionix_accel_attribute_group);
	if (err) {
		KMSGERR(&acceld->client->dev, "%s: sysfs_create_group returned err = %d. Abort.\n", __func__, err);
		goto err_free_irq;
	}

#ifdef    CONFIG_HAS_EARLYSUSPEND
	/* The higher the level, the earlier it resume, and the later it suspend */
	acceld->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 50;
	acceld->early_suspend.suspend = kionix_accel_earlysuspend_suspend;
	acceld->early_suspend.resume = kionix_accel_earlysuspend_resume;
	register_early_suspend(&acceld->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */


	acceld->enable_wake = 1;
	acceld->enable_wake_state = 0;

	kionix_enable_irq(acceld);
	printk(KERN_ERR"[%s_%d] Probe complete !!\n",__FUNCTION__,__LINE__);

	return 0;

err_free_irq:
	if (acceld->accel_drdy)
		free_irq(client->irq, acceld);
	destroy_workqueue(acceld->accel_workqueue);
	input_unregister_device(acceld->input_dev);
err_free_accel_registers:
	kfree(acceld->accel_registers);
err_accel_pdata_exit:
	if (accel_pdata->exit)
		accel_pdata->exit();
err_accel_pdata_power_off:
	kionix_accel_power_off(acceld);
err_free_mem:
	kfree(acceld);
	printk(KERN_ERR"[%s_%d] Probe Failed !!\n",__FUNCTION__,__LINE__);
	return err;
}

static int kionix_accel_remove(struct i2c_client *client)
{
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

#ifdef    CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&acceld->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	sysfs_remove_group(&client->dev.kobj, &kionix_accel_attribute_group);
	if (acceld->accel_drdy)
		free_irq(client->irq, acceld);
	destroy_workqueue(acceld->accel_workqueue);
	input_unregister_device(acceld->input_dev);
	kfree(acceld->accel_registers);
	if (acceld->accel_pdata.exit)
		acceld->accel_pdata.exit();
	kionix_accel_power_off(acceld);
	kfree(acceld);

	return 0;
}

static const struct i2c_device_id kionix_accel_id[] = {
	{ KIONIX_ACCEL_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, kionix_accel_id);

static const struct dev_pm_ops kionix_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(kionix_suspend, kionix_resume)
	SET_RUNTIME_PM_OPS(kionix_runtime_suspend, kionix_runtime_resume, NULL)
};

static struct i2c_driver kionix_accel_driver = {
	.driver = {
		.name	= KIONIX_ACCEL_NAME,
		.pm    = &kionix_pm_ops,
		.owner	= THIS_MODULE,
	},
	.probe		= kionix_accel_probe,
	.remove		= kionix_accel_remove,
	.id_table	= kionix_accel_id,
};

static int __init kionix_accel_init(void)
{
	return i2c_add_driver(&kionix_accel_driver);
}
module_init(kionix_accel_init);

static void __exit kionix_accel_exit(void)
{
	i2c_del_driver(&kionix_accel_driver);
}
module_exit(kionix_accel_exit);

MODULE_DESCRIPTION("Kionix accelerometer driver");
MODULE_AUTHOR("Kuching Tan <kuchingtan@kionix.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("3.3.0");

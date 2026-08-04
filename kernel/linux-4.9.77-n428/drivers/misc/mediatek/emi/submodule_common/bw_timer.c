/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "../mt8512/mt_emi.h"
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include "bw_timer.h"

/* 20ms emi bw */
#define EMI_BW_ARRAY_SIZE	8
static void __iomem *CEN_EMI_BASE;
static void __iomem *EMI_MPU_BASE;
static void __iomem *CHN_EMI_BASE[MAX_CH];

/*****************************************************************************
 *	Macro Definiations
 *****************************************************************************/
/* #define USE_TIMER */

/*****************************************************************************
 *	Type Definitions
 *****************************************************************************/
enum DDRTYPE {
	TYPE_LPDDR3 = 1,
	TYPE_LPDDR4,
	TYPE_LPDDR4X
};

enum {
	EMI_BASE_IDX_EMI = 0,
	EMI_BASE_IDX_EMI_CH0,
	EMI_BASE_IDX_EMI_CH1,

	NR_EMI_BASE_IDX,
};

struct emi_base_addr {
	unsigned int phy_addr;
	unsigned int remap_addr;
};

/*static struct emi_info_t emi_info;*/
static void __iomem *CEN_EMI_BASE;
static void __iomem *CHN_EMI_BASE[MAX_CH];
static void __iomem *EMI_MPU_BASE;
static unsigned long long emi_bw_array[EMI_BW_ARRAY_SIZE];
static int emi_bw_cur_idx;
static int emi_bw_max_idx = EMI_BW_ARRAY_SIZE;
static struct hrtimer hr_timer;
static ktime_t ktime;

/*****************************************************************************
 *	Local Variables
 *****************************************************************************/
static char emi_init_done;
#ifdef USE_TIMER
static unsigned long long last_update_time;
#endif
static emi_update_cntr_cb emi_cntr_user_cb[NR_EMI_USER];

static unsigned int cur_mbw_blk;
static unsigned int cur_mbw_period;

/*****************************************************************************
 *	Global Variables
 *****************************************************************************/
struct emi_idx *emi_idx;
unsigned char dram_ch_num;

/*****************************************************************************
 *	Static Function
 *****************************************************************************/
unsigned long long emi_get_max_bw_in_last_array(unsigned long long arr[],
					unsigned int size)
{
	unsigned int i = 0;
	unsigned long long max = arr[0];

	while (i < size) {
		if (arr[i] > max)
			max = arr[i];
		++i;
	}
	return ((max << 8) >> 20);
}
unsigned long long emi_get_mean_bw_in_last_array(unsigned long long arr[])
{
	unsigned int i = 0;
	unsigned long long mean = 0;

	while (i < EMI_BW_ARRAY_SIZE) {
		mean += arr[i];
		++i;
	}
	return ((mean << 8) >> 23);
}

unsigned long long dvfsrc_get_emi_max_bw(void)
{
	return emi_get_max_bw_in_last_array(emi_bw_array, EMI_BW_ARRAY_SIZE);
}
EXPORT_SYMBOL(dvfsrc_get_emi_max_bw);

unsigned long long dvfsrc_get_emi_bw(void)
{
	return emi_get_mean_bw_in_last_array(emi_bw_array);
}
EXPORT_SYMBOL(dvfsrc_get_emi_bw);

void emi_update_bw_array(unsigned int val)
{
	if (emi_bw_cur_idx == emi_bw_max_idx) {
		/* remove the first array element */
		memmove(&emi_bw_array[0], &emi_bw_array[1],
	   sizeof(unsigned long long) * (emi_bw_max_idx - 1));
		emi_bw_array[emi_bw_max_idx - 1] = (val << 3);
	} else
		emi_bw_array[emi_bw_cur_idx++] = (val << 3);
}

static void emi_counter_reset(void)
{
	writel(EMI_BMEN_DEFAULT_VALUE, EMI_BMEN);
}

static void emi_counter_pause(void)
{
	const unsigned int value = readl(EMI_BMEN);

	/* BW monitor */
	writel(value | BUS_MON_PAUSE, EMI_BMEN);
}

static void emi_counter_continue(void)
{
	const unsigned int value = readl(EMI_BMEN);

	/* BW monitor */
	writel(value & (~BUS_MON_PAUSE), EMI_BMEN);
}

static void emi_counter_enable(const unsigned int enable)
{
	const unsigned int value = readl(EMI_BMEN);

	if (enable == 0) {
		/* disable monitor circuit */
		/*bit3 =1	bit0 = 0-> clear*/
		writel(value | BUS_MON_IDLE, EMI_BMEN);
		writel(((value) | (BUS_MON_IDLE)) & ~(BUS_MON_EN), EMI_BMEN);
		writel(((value) & ~(BUS_MON_IDLE)) & ~(BUS_MON_EN), EMI_BMEN);
	} else {
		/* enable monitor circuit */
		/* bit3 =0	&	bit0=1 */
		writel((value & ~(BUS_MON_IDLE)), EMI_BMEN);
		writel((value & ~(BUS_MON_IDLE)) | (BUS_MON_EN), EMI_BMEN);
	}
}
/*****************************************************************************
 *APIs
 *****************************************************************************/
void emi_mon_start(void)
{
	emi_counter_enable(0);
	emi_counter_reset();
	emi_counter_enable(1);
}

void emi_mon_restart(void)
{
	emi_counter_enable(0);
	emi_counter_continue();
	emi_counter_enable(1);
}

void emi_mon_stop(void)
{
	emi_counter_pause();
}

/* for QoS and power meter to check whether the BW monitor
 *is occupied by someone(ex: MET) or not.
 */
bool emi_is_bwmon_available(void)
{
	if (emi_cntr_user_cb[EMI_USER_MET_EMI])
		return false;
	else
		return true;
}

unsigned int emi_get_data_rate(void)
{
#ifdef CFG_QOS_SUPPORT
	return (unsigned int)(get_cur_ddr_khz() / 1000);
#else
	return 1600;
#endif
}

void emi_register_cb(enum emi_user user, emi_update_cntr_cb fn)
{
	if (user >= NR_EMI_USER)
		return;

	emi_cntr_user_cb[user] = fn;
}

void emi_unregister_cb(enum emi_user user)
{
	if (user >= NR_EMI_USER)
		return;

	emi_cntr_user_cb[user] = (void *)0;

	/* restore to default value after MET stop */
	if (user == EMI_USER_MET_EMI)
		emi_mon_start();
}

static unsigned int bwvl_cpu, bwvl_gpu;
static unsigned int wact;
static unsigned int cur_diff_us;

unsigned int get_emi_bw(unsigned int bw_type)
{
	switch (bw_type) {
	case EMIBM_TOTAL:
	return (wact * 8 / cur_diff_us);
	case EMIBM_CPU:
	return (bwvl_cpu * EMI_BWVL_UNIT);
	case EMIBM_GPU:
	return (bwvl_gpu * EMI_BWVL_UNIT);
	default:
	return 0;
	}
}

static void get_emi_mbw(struct emi_idx *emi_mbw_idx)
{
	unsigned int i;

	emi_idx->bw_raw[EMIBM_TOTAL] = readl(EMI_WSCT);	 /* All */
	emi_idx->bw_raw[EMIBM_TOTAL_W] = readl(EMI_WSCT2); /* Group 1 */
	emi_idx->bw_raw[EMIBM_CPU] = readl(EMI_WSCT3); /* Group 2 */
	emi_idx->bw_raw[EMIBM_GPU] = readl(EMI_WSCT4); /* Group 3 */

	/* Get Latency Trans. */
	for (i = 0; i < EMI_TTYPE_NUM; i++)
		emi_idx->ttype[i] = readl(((EMI_TTYPE1) + (i) * 8));

	/* BACT and BCNT */
	emi_idx->bact = readl(EMI_BACT);
	if (!(BC_OVERRUN & readl(EMI_BMEN)))
		emi_idx->bcnt = readl(EMI_BCNT);
	else
		emi_idx->bcnt = 0x0FFFFFFF;

	/* TACT */
	emi_idx->tact = readl(EMI_TACT);

	/* WACT */
	emi_idx->wact = readl(EMI_WACT);

	/* read idx which is not required for power model */
	if (emi_cntr_user_cb[EMI_USER_MET_EMI]) {
		/* BSCT */
		emi_idx->bsct = readl(EMI_BSCT);
		/* TSCT */
		emi_idx->tsct[0] = readl(EMI_TSCT);
		emi_idx->tsct[1] = readl(EMI_TSCT2);
		emi_idx->tsct[2] = readl(EMI_TSCT3);
		/* MDCT */
		emi_idx->mdct[0] = (readl(EMI_MDCT) >> 16) & 0x7;
		emi_idx->mdct[1] = (readl(EMI_MDCT_2ND) & 0x7);
	}
}

void emi_counter_read(void)
{
#ifdef USE_TIMER
	unsigned long long stop_time, diff_us = 0;
#endif
	unsigned char i;

	if (!emi_init_done)
		return;

	emi_mon_stop();

#ifdef USE_TIMER
	stop_time = ostimer_get_ns();
	if (stop_time > last_update_time)
		diff_us = (stop_time - last_update_time) / 1000;
#endif

	get_emi_mbw(emi_idx);

#ifdef USE_TIMER
	last_update_time = ostimer_get_ns();
#endif
	emi_mon_restart();

	/* update data rate */
	emi_idx->data_rate = emi_get_data_rate();

	if (emi_cntr_user_cb[EMI_USER_MET_EMI]) {
		/* read BW Limit */
		for (i = 0; i < EMI_BW_LIMIT_NUM; i++)
			emi_idx->bw_limit[i] = readl(EMI_ARBA + (i) * 8);
	/* DRS_ST */
	for (i = 0; i < EMI_DRS_ST_NUM * 2; i++) {
		if (i >= EMI_DRS_ST_NUM)
			emi_idx->drs_st[i] =
			readl(EMI_CH1_DRS_ST2 + (i - EMI_DRS_ST_NUM) * 4);
		else
			emi_idx->drs_st[i] = readl(EMI_CH0_DRS_ST2 + (i) * 4);
	}

		/* notify MET */
		emi_cntr_user_cb[EMI_USER_MET_EMI](emi_idx);
	}
}

static ssize_t emi_show_bw(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	unsigned long long var;

	if (!dev) {
		pr_info("dev is null!!\n");
		return 0;
	}

	var = dvfsrc_get_emi_max_bw();

	return scnprintf(buf, PAGE_SIZE, "emi_max_bw:%llu\n", var);
}

static ssize_t emi_store_bw(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}
DEVICE_ATTR(bw, 0664, emi_show_bw, emi_store_bw);

enum hrtimer_restart emi_bw_timer_callback(struct hrtimer *my_timer)
{
	unsigned long long val;

	/* pasue emi monitor for get WACT value*/
	emi_mon_stop();
	val = readl(EMI_WSCT);
	/* pr_info("bw value:%llu\n", val * 8 *256 / 1024 / 1024); */
	emi_update_bw_array(val);
	/* pr_info("max bw value:%llu\n", dvfsrc_get_emi_max_bw()); */
	/* pr_info("mean bw value:%llu\n", dvfsrc_get_emi_bw()); */
	/* set mew timer expires and restart emi monitor */
	hrtimer_forward_now(my_timer, ktime);
	emi_mon_restart();
	return HRTIMER_RESTART;
}

void bw_timer_init(void)
{
	unsigned long delay_in_ms = 4000000L;

	CEN_EMI_BASE = mt_cen_emi_base_get();
	CHN_EMI_BASE[0] = mt_chn_emi_base_get(0);
	EMI_MPU_BASE = mt_emi_mpu_base_get();

	/* start emi bw monitor */
	emi_mon_start();
	ktime = ktime_set(0, delay_in_ms);
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = emi_bw_timer_callback;
	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
	/* debug node */
	cur_mbw_blk = 0;
	cur_mbw_period = 0;
	emi_init_done = 1;
}

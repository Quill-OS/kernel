#ifndef LAB126
#define DEBUG
#endif

/*
 * wm8962.c  --  WM8962 ALSA SoC Audio driver
 *
 * Copyright 2010 Wolfson Microelectronics plc
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * Copyright 2010 Amazon.com, Inc. All rights reserved.
 * Author: Manish Lachwani <lachwani@amazon.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gcd.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/sysdev.h>
#include <linux/miscdevice.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/wm8962.h>

#include "wm8962.h"

static struct snd_soc_codec *wm8962_codec;
struct snd_soc_codec_device soc_codec_dev_wm8962;

#define WM8962_DEFAULT_SPKR_VALUE	0x15f
#define WM8962_OSC_PLL_ENABLE		0xF1
#define WM8962_DAC_VOL_DEFAULT		0xB8

#define WM8960_OFF_THRESHOLD		0	/* Threshold for powering off the codec */

#define WM8962_NUM_SUPPLIES 8
static const char *wm8962_supply_names[WM8962_NUM_SUPPLIES] = {
	"DCVDD",
	"DBVDD",
	"AVDD",
	"CPVDD",
	"MICVDD",
	"PLLVDD",
	"SPKVDD1",
	"SPKVDD2",
};

/* old saved speaker volume */
static int old_speaker_vol = 0;

/* headset status */
static int headset_detected = 0;

static int fll_disabled = 1;

/* Figure out if audio is paused or not */
static int wm962_paused = 0;

static int power_off_timeout = WM8960_OFF_THRESHOLD; /* in msecs */


static int wm8962_codec_diags_mode = 0; /* Codec diags mode */

/* Diags volume settings */
#define WM8962_DIAGS_DAC_VOL	0xc0
#define WM8962_DIAGS_HP_VOL	    0x16e
#define WM8962_DIAGS_SPKR_VOL	0x1f8
#define WM8962_DIAGS_MUTE_VOL   0X100

/**
 * NOTE: this must match with the definition in Audio test. 
 * Select the correct codec diags mode. 
 * \todo Currently the audio codec only support the first 2 
 *       modes.  Need to support other modes so the application
 *       can program the audio volume registers before starting
 *       pcm to avoid the i2c errors when playing audio sound.
 *       For example the audio driver ignores all update to the
 *       audio volume and DAC registers so tests can program
 *       them in advance
 *  
 */

#define WM8962_CODEC_DIAGS_MODE_NORMAL                  0
#define WM8962_CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST  1
#define WM8962_CODEC_DIAGS_MODE_LEFT_HP_TEST            2
#define WM8962_CODEC_DIAGS_MODE_RIGHT_HP_TEST           3
#define WM8962_CODEC_DIAGS_MODE_BOTH_HP_TEST            4
#define WM8962_CODEC_DIAGS_MODE_LEFT_SP_TEST            5
#define WM8962_CODEC_DIAGS_MODE_RIGHT_SP_TEST           6
#define WM8962_CODEC_DIAGS_MODE_BOTH_SP_TEST            7
#define WM8962_CODEC_DIAGS_MODE_MIC_TEST                8
#define WM8962_CODEC_DIAGS_MODE_MAX                     9

#define WM8962_VERBOSE_MODE
#ifdef WM8962_VERBOSE_MODE

/**
 * For debugging only.  Do not enable in release mode. 
 * Utility to print out debug information. 
 * Keep debug information until the audio test is fully 
 * implemented and stable. 
 *  
 */
typedef struct _WM8962_ELM
{
    const char   *name;
    unsigned int  val;
} WM8962_ELM;


WM8962_ELM codec_diags_elm[] =
{
    { "DIAGS_MODE_NORMAL                 ", WM8962_CODEC_DIAGS_MODE_NORMAL                  }, 
    { "DIAGS_MODE_AUTOMATED_SPEAKER_TEST ", WM8962_CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST  },
    { "DIAGS_MODE_LEFT_HP_TEST           ", WM8962_CODEC_DIAGS_MODE_LEFT_HP_TEST            },
    { "DIAGS_MODE_RIGHT_HP_TEST          ", WM8962_CODEC_DIAGS_MODE_RIGHT_HP_TEST           },
    { "DIAGS_MODE_BOTH_HP_TEST           ", WM8962_CODEC_DIAGS_MODE_BOTH_HP_TEST            },
    { "DIAGS_MODE_LEFT_SP_TEST           ", WM8962_CODEC_DIAGS_MODE_LEFT_SP_TEST            },
    { "DIAGS_MODE_RIGHT_SP_TEST          ", WM8962_CODEC_DIAGS_MODE_RIGHT_SP_TEST           },
    { "DIAGS_MODE_BOTH_SP_TEST           ", WM8962_CODEC_DIAGS_MODE_BOTH_SP_TEST            },
    { "DIAGS_MODE_MIC_TEST               ", WM8962_CODEC_DIAGS_MODE_MIC_TEST                },
    { NULL , WM8962_CODEC_DIAGS_MODE_MAX                     }
};

WM8962_ELM dapm_event_elm[] =
{
    { "SND_SOC_DAPM_POST_PMU", SND_SOC_DAPM_POST_PMU  },
    { "SND_SOC_DAPM_PRE_PMD ", SND_SOC_DAPM_PRE_PMD   },
    { NULL , 0xDEADBEEF                     }
};

WM8962_ELM out_pga_event_elm[] =
{
    { "WM8962_HPOUTR_PGA_ENA_SHIFT" , WM8962_HPOUTR_PGA_ENA_SHIFT   },
    { "WM8962_HPOUTL_PGA_ENA_SHIFT" , WM8962_HPOUTL_PGA_ENA_SHIFT   },
    { "WM8962_SPKOUTR_PGA_ENA_SHIFT", WM8962_SPKOUTR_PGA_ENA_SHIFT  },
    { "WM8962_SPKOUTL_PGA_ENA_SHIFT", WM8962_SPKOUTL_PGA_ENA_SHIFT  },
    { NULL , 0xDEADBEEF                     }
};

static int print_wm8962_elm_to_buf(const char *prompt, WM8962_ELM *pElm, unsigned int val, char *buff)
{
    while (pElm && pElm->name != NULL) {
        if (val == pElm->val) {
            return sprintf(buff, "%s: %s, %01d, 0x%02X\n", prompt, pElm->name, val, val);
        }
        pElm++;
    }
    return 0;
}

static void print_wm8962_elm(const char *prompt, WM8962_ELM *pElm, unsigned int val)
{
    char buff[128];

    print_wm8962_elm_to_buf(prompt, pElm, val, buff);
    printk(KERN_WARNING "%s", buff);
}


static void print_wm8962_codec_diags_mode(const char *prompt, unsigned int diags_mode)
{
    print_wm8962_elm(prompt, codec_diags_elm, diags_mode);
}

#endif // WM8962_VERBOSE_MODE

static int wm8962_codec_diags_set_custom_regs(const char *prompt, struct snd_soc_codec *codec);


#define HEADSET_MINOR	165	/* Headset event passing */

static struct miscdevice mxc_headset = {
	HEADSET_MINOR,
	"mxc_ALSA",
	NULL,
};

/**
 * ANH
 */
static int wm8962_irq_value = -1;
static int wm8962_irq_enabled = 0;

/* codec private data */
struct wm8962_priv {
	struct snd_soc_codec codec;
	u16 reg_cache[WM8962_MAX_REGISTER + 1];

	int irq;
	struct delayed_work irq_work;

	struct snd_soc_jack *jack;

	int sysclk;
	int sysclk_rate;

	int bclk;  /* Desired BCLK */
	int lrclk;

	int fll_src;
	int fll_fref;
	int fll_fout;

	struct regulator_bulk_data supplies[WM8962_NUM_SUPPLIES];
	struct notifier_block disable_nb[WM8962_NUM_SUPPLIES];

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	struct input_dev *beep;
	struct work_struct beep_work;
	int beep_rate;
#endif
};

static int wm8962_volatile_register(unsigned int reg)
{
	if (wm8962_reg_access[reg].vol)
		return 1;
	else
		return 0;
}

static int wm8962_readable(unsigned int reg)
{
	if (wm8962_reg_access[reg].read)
		return 1;
	else
		return 0;
}

static int wm8962_reset(struct snd_soc_codec *codec)
{
	int ret;
	
	ret = snd_soc_write(codec, WM8962_SOFTWARE_RESET, 0x6243);
	if (ret != 0)
		return ret;
	
	return snd_soc_write(codec, WM8962_PLL_SOFTWARE_RESET, 0);
}

static const DECLARE_TLV_DB_SCALE(inpga_tlv, -2325, 75, 0);
static const DECLARE_TLV_DB_SCALE(mixin_tlv, -1500, 300, 0);
static const unsigned int mixinpga_tlv[] = {
	TLV_DB_RANGE_HEAD(7),
	0, 1, TLV_DB_SCALE_ITEM(0, 600, 0),
	2, 2, TLV_DB_SCALE_ITEM(1300, 1300, 0),
	3, 4, TLV_DB_SCALE_ITEM(1800, 200, 0),
	5, 5, TLV_DB_SCALE_ITEM(2400, 0, 0),
	6, 7, TLV_DB_SCALE_ITEM(2700, 300, 0),
};
static const DECLARE_TLV_DB_SCALE(beep_tlv, -9600, 600, 1);
static const DECLARE_TLV_DB_SCALE(digital_tlv, -7200, 75, 1);
static const DECLARE_TLV_DB_SCALE(st_tlv, -3600, 300, 0);
static const DECLARE_TLV_DB_SCALE(inmix_tlv, -600, 600, 0);
static const DECLARE_TLV_DB_SCALE(bypass_tlv, -1500, 300, 0);
static const DECLARE_TLV_DB_SCALE(out_tlv, -12100, 100, 1);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -700, 100, 0);
static const unsigned int classd_tlv[] = {
	TLV_DB_RANGE_HEAD(7),
	0, 6, TLV_DB_SCALE_ITEM(0, 150, 0),
	7, 7, TLV_DB_SCALE_ITEM(1200, 0, 0),
};

/*
 * read wm8962 register cache
 */
static inline unsigned int wm8962_read_reg_cache(struct snd_soc_codec *codec,
        unsigned int reg)
{
        u16 *cache = codec->reg_cache;
        if (reg == WM8962_SOFTWARE_RESET)
                return 0;
        if (reg >= WM8962_MAX_REGISTER)
                return -1;
        return cache[reg];
}

/*
 * write wm8962 register cache
 */
static inline void wm8962_write_reg_cache(struct snd_soc_codec *codec,
        u16 reg, unsigned int value)
{
        u16 *cache = codec->reg_cache;
        if (reg >= WM8962_MAX_REGISTER)
                return;
        cache[reg] = value;
}

static inline unsigned int wm8962_read(struct snd_soc_codec *codec,
        unsigned int r)
{
	struct i2c_msg xfer[2];
	u16 reg = cpu_to_be16(r);
	u16 data;
	int ret;
	struct i2c_client *client = codec->control_data;

	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = (u8 *)&reg;

	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 2;
	xfer[1].buf = (u8 *)&data;

	ret = i2c_transfer(client->adapter, xfer, 2);
	if (ret != 2) {
		dev_err(&client->dev, "i2c_transfer() returned %d\n", ret);
		return 0;
	}

	return be16_to_cpu(data);
}

/*
 * write to the WM8960 register space
 */
static int wm8962_write(struct snd_soc_codec *codec, unsigned int reg,
        unsigned int value)
{
	u8 data[4];
	int ret;

	switch (reg) {
	case WM8962_LEFT_INPUT_VOLUME:
	case WM8962_RIGHT_INPUT_VOLUME:
		value |= WM8962_IN_VU;
		break;
	default:
		break;
	}

	dev_dbg(codec->dev, "Writing reg : 0x%x value : 0x%x\n", reg, value);
  
	data[0] = (reg >> 8) & 0xff;
	data[1] = reg & 0xff;
	data[2] = (value >> 8) & 0xff;
	data[3] = value & 0xff;

	ret = i2c_master_send(codec->control_data, data, 4);
	if (ret == 4)
		return 0;

	if (ret < 0)
		return ret;
	else
		return -EIO;
}

/**
 * @brief wm8962_write_through() 
 * Write register and update cache if success.  This operation 
 * is slower, but it can be used to eliminate some cache 
 * coherency problems. 
 * 
 * @param codec 
 * @param reg 
 * @param value 
 * 
 * @return int 
 */
static int wm8962_write_through(struct snd_soc_codec *codec, unsigned int reg,
        unsigned int value)
{
    int rval; 

    rval = wm8962_write(codec, reg, value);
    if (rval == 0) {
        wm8962_write_reg_cache(codec, reg, value); 
    }
    return rval;
}

static int wm8962_lpm_mute(struct snd_soc_codec *codec, int mute)
{
	int val;

	if (mute)
		val = WM8962_DAC_MUTE;
	else
		val = 0;

	return snd_soc_update_bits(codec, WM8962_ADC_DAC_CONTROL_1,
			WM8962_DAC_MUTE, val);
}

/*
 * WM8962 Codec /sys
 */
static int wm8962_reg_number = 0;

/*
 * Debug - dump all the codec registers 
 * WM8962 has too many register to dump.  So just limit to 600 useful registers. 
 */
#define WM8962_MAX_REGISTER_TO_DUMP 600

static void dump_regs(struct snd_soc_codec *codec)
{
        int i = 0;

        for (i=0; i < WM8962_MAX_REGISTER_TO_DUMP; i++)
                printk("Register %d = Value:0x%x\n", i, wm8962_read(codec, i));
}

DEFINE_MUTEX(wm8962_lock);

/*
 * sysfs entries for the codec
 */
static ssize_t wm8962_reg_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
        int value = 0;

        if (sscanf(buf, "%d", &value) <= 0) {
                printk(KERN_ERR "Could not store the codec register value\n");
                return -EINVAL;
        }

        wm8962_reg_number = value;

        return size;
}

static ssize_t wm8962_reg_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
        char *curr = buf;

        curr += sprintf(curr, "WM8962 Register Number: %d\n", wm8962_reg_number);
        curr += sprintf(curr, "\n");

        return curr - buf;
}

static SYSDEV_ATTR(wm8962_reg, 0644, wm8962_reg_show, wm8962_reg_store);

static struct sysdev_class wm8962_reg_sysclass = {
        .name   = "wm8962_reg",
};

static struct sys_device device_wm8962_reg = {
        .id     = 0,
        .cls    = &wm8962_reg_sysclass,
};

static ssize_t wm8962_headset_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", headset_detected);
}

static SYSDEV_ATTR(wm8962_headset, 0644, wm8962_headset_show, NULL);

static struct sysdev_class wm8962_headset_sysclass = {
	.name	= "wm8962_headset",
};

static struct sys_device device_wm8962_headset = {
	.id	= 0,
	.cls	= &wm8962_headset_sysclass,
};

static ssize_t wm8962_register_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
        char *curr = buf;

        if (wm8962_reg_number >= WM8962_MAX_REGISTER_TO_DUMP) {
                /* dump all the regs */
                curr += sprintf(curr, "WM8962 Registers\n");
//                curr += sprintf(curr, "\n");
                dump_regs(wm8962_codec);
        }
        else {
                curr += sprintf(curr, "WM8962 Register %d\n", wm8962_reg_number);
//                curr += sprintf(curr, "\n");
                curr += sprintf(curr, " Value: 0x%x\n",
                                snd_soc_read(wm8962_codec, wm8962_reg_number));
                curr += sprintf(curr, "\n");
        }

        return curr - buf;
}

static ssize_t wm8962_register_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
        int value = 0;

        if (wm8962_reg_number >= WM8962_MAX_REGISTER) {
                printk(KERN_ERR "No codec register %d\n", wm8962_reg_number);
                return size;
        }

        if (sscanf(buf, "%x", &value) <= 0) {
                printk(KERN_ERR "Error setting the value in the register\n");
                return -EINVAL;
       }

        /**
         * Access through sys entry also update the cache so Audio test 
         * can program the volume first before playing audio 
         */
        wm8962_write_through(wm8962_codec, wm8962_reg_number, value);
        return size;
}
static SYSDEV_ATTR(wm8962_register, 0644, wm8962_register_show, wm8962_register_store);

static struct sysdev_class wm8962_register_sysclass = {
        .name   = "wm8962_register",
};

static struct sys_device device_wm8962_register = {
        .id     = 0,
        .cls    = &wm8962_register_sysclass,
};

/*
 * Codec power off timeout
 */
static ssize_t power_off_timeout_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	char *curr = buf;

	curr += sprintf(curr, "%d\n", power_off_timeout);

	return curr - buf;
}

static ssize_t power_off_timeout_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0) {
		printk(KERN_ERR "Error setting the value for timeout\n");
		return -EINVAL;
	}

	power_off_timeout = value;

	return size;
}

static SYSDEV_ATTR(wm8962_power_off_timeout, 0644, power_off_timeout_show, power_off_timeout_store);

static struct sysdev_class wm8962_power_off_timeout_sysclass = {
    .name = "wm8962_power_off_timeout",
};

static struct sys_device device_wm8962_power_off_timeout = {
    .id   = 0,
    .cls  = &wm8962_power_off_timeout_sysclass,
};

static ssize_t wm8962_codec_mute_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) < 0) {
		printk(KERN_ERR "Error setting mute/unmute, mute=1, unmute=0\n");
		return -EINVAL;
	}

	if (value < 0 || value > 1) {
		printk(KERN_ERR "Invalid value, mute=1, unmute=0\n");
		return -EINVAL;
	}

	wm8962_lpm_mute(wm8962_codec, value);

	return size;
}

static SYSDEV_ATTR(wm8962_codec_mute, 0644, NULL, wm8962_codec_mute_store);

static struct sysdev_class wm8962_codec_mute_sysclass = {
    .name = "wm8962_codec_mute",
};

static struct sys_device device_wm8962_codec_mute = {
    .id   = 0,
    .cls  = &wm8962_codec_mute_sysclass,
};

/* Diags mode for codec */
static ssize_t wm8962_codec_diags_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) < 0) {
		printk(KERN_ERR "Error setting diags mode\n");
		return -EINVAL;
	}

	if ((value < 0) || (value >= WM8962_CODEC_DIAGS_MODE_MAX) ) {
		printk(KERN_ERR "Diags mode: Invalid value = %d\n", value);
		return -EINVAL;
	}

    if (wm8962_codec_diags_mode != value)
    {
        /**
         * if the audio codec was int mic test mode, the interrupt was 
         * disabled, now enable it back. 
         */
        if (!wm8962_codec_diags_mode)
        {
            if ((wm8962_irq_value != -1) && (wm8962_irq_enabled == 0))
            {
                enable_irq(wm8962_irq_value);
                wm8962_irq_enabled = 1;
            }
        }
	wm8962_codec_diags_mode = value;
    }
	print_wm8962_elm("codec_diags_store", codec_diags_elm, wm8962_codec_diags_mode); 
//	wm8962_codec_diags_set_custom_regs("codec_diags_store", wm8962_codec);

	return size;
}

static ssize_t wm8962_codec_diags_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
//	return sprintf(buf, "%d\n", wm8962_codec_diags_mode);
	return print_wm8962_elm_to_buf("codec_diags", codec_diags_elm, wm8962_codec_diags_mode, buf); 
}

static SYSDEV_ATTR(wm8962_codec_diags, 0644, wm8962_codec_diags_show, wm8962_codec_diags_store);

static struct sysdev_class wm8962_codec_diags_sysclass = {
    .name = "wm8962_codec_diags",
};

static struct sys_device device_wm8962_codec_diags = {
    .id   = 0,
    .cls  = &wm8962_codec_diags_sysclass,
};

static ssize_t wm8962_fll_disabled_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fll_disabled); 
}

static SYSDEV_ATTR(wm8962_fll_disabled, 0644, wm8962_fll_disabled_show, NULL);

static struct sysdev_class wm8962_fll_disabled_sysclass = {
    .name = "wm8962_fll_disabled",
};

static struct sys_device device_wm8962_fll_disabled = {
    .id   = 0,
    .cls  = &wm8962_fll_disabled_sysclass,
};

static ssize_t wm8962_codec_diags_volume_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
    unsigned int HP_left    ;
    unsigned int HP_right   ;
    unsigned int SP_left    ;
    unsigned int SP_right   ;
    unsigned int DAC        ;

	if (sscanf(buf, "%x %x %x %x %x", &HP_left, &HP_right, &SP_left, &SP_right, &DAC) < 0) {
		printk(KERN_ERR "Error setting diags volume\n");
		return -EINVAL;
	}
    /**
     * Do not need to update DAC for now because it is still 
     * hardcoded. 
     */
    wm8962_write_through(wm8962_codec, WM8962_HPOUTL_VOLUME , HP_left ); 
    wm8962_write_through(wm8962_codec, WM8962_HPOUTR_VOLUME , HP_right); 
    wm8962_write_through(wm8962_codec, WM8962_SPKOUTL_VOLUME, SP_left ); 
    wm8962_write_through(wm8962_codec, WM8962_SPKOUTR_VOLUME, SP_right); 
    
	return size;
}

static ssize_t wm8962_codec_diags_volume_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
    unsigned int HP_left    ;
    unsigned int HP_right   ;
    unsigned int SP_left    ;
    unsigned int SP_right   ;
    unsigned int DAC        ;

    HP_left  = snd_soc_read(wm8962_codec, WM8962_HPOUTL_VOLUME  );
    HP_right = snd_soc_read(wm8962_codec, WM8962_HPOUTR_VOLUME  );
    SP_left  = snd_soc_read(wm8962_codec, WM8962_SPKOUTL_VOLUME );
    SP_right = snd_soc_read(wm8962_codec, WM8962_SPKOUTR_VOLUME );
    DAC      = snd_soc_read(wm8962_codec, WM8962_LEFT_ADC_VOLUME);

	return sprintf(buf, "0x%04X 0x%04X 0x%04X 0x%04X 0x%04X \n", 
                   HP_left   ,
                   HP_right  ,
                   SP_left   ,
                   SP_right  ,
                   DAC       );
}

static SYSDEV_ATTR(wm8962_codec_diags_volume, 0644, wm8962_codec_diags_volume_show, wm8962_codec_diags_volume_store);

static struct sysdev_class wm8962_codec_diags_volume_sysclass = {
    .name = "wm8962_codec_diags_volume",
};

static struct sys_device device_wm8962_codec_diags_volume = {
    .id   = 0,
    .cls  = &wm8962_codec_diags_volume_sysclass,
};

/**
 * @brief wm8962_codec_diags_set_custom_regs() 
 * Turns off Head Phone volume and power. 
 * Turns on Speaker volume and power. 
 * 
 * @param prompt 
 * @param codec 
 * 
 * @return int 
 */
static int wm8962_codec_diags_set_custom_regs(const char *prompt, struct snd_soc_codec *codec)
{
	switch (wm8962_codec_diags_mode) 
    {
    case WM8962_CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST:
        /**
         * Play to SPEAKERS 
         */
        printk(KERN_WARNING "%s: Automated speaker test\n", prompt);
        /**
         * Turn off Head Phone volume
         */
        snd_soc_write(codec, WM8962_HPOUTL_VOLUME , WM8962_DIAGS_MUTE_VOL);
		snd_soc_write(codec, WM8962_HPOUTR_VOLUME , WM8962_DIAGS_MUTE_VOL);
        /**
         * Set Speaker volume to predefined level
         */
		snd_soc_write(codec, WM8962_SPKOUTL_VOLUME, WM8962_DIAGS_SPKR_VOL);
        snd_soc_write(codec, WM8962_SPKOUTR_VOLUME, WM8962_DIAGS_SPKR_VOL); 

        /**
         * Enable DAC, and speaker power. Disable Head Phone power.
         */
        snd_soc_write(codec, WM8962_PWR_MGMT_2       , 
                      WM8962_DACL_ENA | WM8962_DACR_ENA |               // Enable DAC
                      WM8962_SPKOUTL_PGA_ENA | WM8962_SPKOUTR_PGA_ENA | // Enable SPEAKERS only
                      WM8962_HPOUTL_PGA_MUTE | WM8962_HPOUTR_PGA_MUTE); // Mute Headset 
        /**
         * 0x40 = SPKOUT_VU 
         * Enable speaker output. 
         */
        snd_soc_write(codec, WM8962_CLASS_D_CONTROL_1, 0x40 | WM8962_SPKOUTR_ENA | WM8962_SPKOUTL_ENA); 
        break;
    default:
        break;
	}
    return 0;
}

/* The VU bits for the headphones are in a different register to the mute
 * bits and only take effect on the PGA if it is actually powered.
 */
static int wm8962_put_hp_sw(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u16 *reg_cache = codec->reg_cache;
	int ret;

	/* Apply the update (if any) */
    ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret == 0)
		return 0;

    /* If the left PGA is enabled hit that VU bit... */
    if (reg_cache[WM8962_PWR_MGMT_2] & WM8962_HPOUTL_PGA_ENA)
        return snd_soc_write(codec, WM8962_HPOUTL_VOLUME,
                     reg_cache[WM8962_HPOUTL_VOLUME]);

    /* ...otherwise the right.  The VU is stereo. */
    if (reg_cache[WM8962_PWR_MGMT_2] & WM8962_HPOUTR_PGA_ENA)
        return snd_soc_write(codec, WM8962_HPOUTR_VOLUME,
                     reg_cache[WM8962_HPOUTR_VOLUME]);

	return 0;
}

/* The VU bits for the speakers are in a different register to the mute
 * bits and only take effect on the PGA if it is actually powered.
 */
static int wm8962_put_spk_sw(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u16 *reg_cache = codec->reg_cache;
	int ret;
	ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret == 0)
		return 0;

	/* If the left PGA is enabled hit that VU bit... */
	if (reg_cache[WM8962_PWR_MGMT_2] & WM8962_SPKOUTL_PGA_ENA)
		return snd_soc_write(codec, WM8962_SPKOUTL_VOLUME,
				     reg_cache[WM8962_SPKOUTL_VOLUME]);

	/* ...otherwise the right.  The VU is stereo. */
	if (reg_cache[WM8962_PWR_MGMT_2] & WM8962_SPKOUTR_PGA_ENA)
		return snd_soc_write(codec, WM8962_SPKOUTR_VOLUME,
				     reg_cache[WM8962_SPKOUTR_VOLUME]);

	return 0;
}

static const struct snd_kcontrol_new wm8962_snd_controls[] = {
SOC_DOUBLE("Input Mixer Switch", WM8962_INPUT_MIXER_CONTROL_1, 3, 2, 1, 1),

SOC_SINGLE_TLV("MIXINL IN2L Volume", WM8962_LEFT_INPUT_MIXER_VOLUME, 6, 7, 0,
	       mixin_tlv),
SOC_SINGLE_TLV("MIXINL PGA Volume", WM8962_LEFT_INPUT_MIXER_VOLUME, 3, 7, 0,
	       mixinpga_tlv),
SOC_SINGLE_TLV("MIXINL IN3L Volume", WM8962_LEFT_INPUT_MIXER_VOLUME, 0, 7, 0,
	       mixin_tlv),

SOC_SINGLE_TLV("MIXINR IN2R Volume", WM8962_RIGHT_INPUT_MIXER_VOLUME, 6, 7, 0,
	       mixin_tlv),
SOC_SINGLE_TLV("MIXINR PGA Volume", WM8962_RIGHT_INPUT_MIXER_VOLUME, 3, 7, 0,
	       mixinpga_tlv),
SOC_SINGLE_TLV("MIXINR IN3R Volume", WM8962_RIGHT_INPUT_MIXER_VOLUME, 0, 7, 0,
	       mixin_tlv),

SOC_DOUBLE_R_TLV("Digital Capture Volume", WM8962_LEFT_ADC_VOLUME,
		WM8962_RIGHT_ADC_VOLUME, 1, 127, 0, digital_tlv),

SOC_DOUBLE_R_TLV("Capture Volume", WM8962_LEFT_INPUT_VOLUME,
		 WM8962_RIGHT_INPUT_VOLUME, 0, 63, 0, inpga_tlv),
SOC_DOUBLE_R("Capture Switch", WM8962_LEFT_INPUT_VOLUME,
	     WM8962_RIGHT_INPUT_VOLUME, 7, 1, 1),
SOC_DOUBLE_R("Capture ZC Switch", WM8962_LEFT_INPUT_VOLUME,
	     WM8962_RIGHT_INPUT_VOLUME, 6, 1, 1),

SOC_DOUBLE_R_TLV("Sidetone Volume", WM8962_DAC_DSP_MIXING_1,
		 WM8962_DAC_DSP_MIXING_2, 4, 12, 0, st_tlv),

SOC_SINGLE("DAC High Performance Switch", WM8962_ADC_DAC_CONTROL_2, 0, 1, 0),

SOC_SINGLE("ADC High Performance Switch", WM8962_ADDITIONAL_CONTROL_1,
	   5, 1, 0),

SOC_SINGLE_TLV("Beep Volume", WM8962_BEEP_GENERATOR_1, 4, 15, 0, beep_tlv),

SOC_DOUBLE_EXT("Headphone Switch", WM8962_PWR_MGMT_2, 1, 0, 1, 1,
	       snd_soc_get_volsw, wm8962_put_hp_sw),
SOC_DOUBLE_R_TLV("Headphone Volume", WM8962_HPOUTL_VOLUME,
		WM8962_HPOUTR_VOLUME, 0, 127, 0, out_tlv),

SOC_DOUBLE_TLV("Headphone Aux Volume", WM8962_ANALOGUE_HP_2, 3, 6, 7, 0,
	       hp_tlv),

SOC_DOUBLE_R("Headphone Mixer Switch", WM8962_HEADPHONE_MIXER_3,
	     WM8962_HEADPHONE_MIXER_4, 8, 1, 1),

SOC_SINGLE_TLV("HPMIXL IN4L Volume", WM8962_HEADPHONE_MIXER_3,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("HPMIXL IN4R Volume", WM8962_HEADPHONE_MIXER_3,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("HPMIXL MIXINL Volume", WM8962_HEADPHONE_MIXER_3,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("HPMIXL MIXINR Volume", WM8962_HEADPHONE_MIXER_3,
	       6, 1, 1, inmix_tlv),

SOC_SINGLE_TLV("HPMIXR IN4L Volume", WM8962_HEADPHONE_MIXER_4,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("HPMIXR IN4R Volume", WM8962_HEADPHONE_MIXER_4,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("HPMIXR MIXINL Volume", WM8962_HEADPHONE_MIXER_4,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("HPMIXR MIXINR Volume", WM8962_HEADPHONE_MIXER_4,
	       6, 1, 1, inmix_tlv),

SOC_SINGLE_TLV("Speaker Boost Volume", WM8962_CLASS_D_CONTROL_2, 0, 7, 0,
	       classd_tlv),
};

static const struct snd_kcontrol_new wm8962_spk_mono_controls[] = {
SOC_SINGLE_TLV("Speaker Volume", WM8962_SPKOUTL_VOLUME, 0, 127, 0, out_tlv),
SOC_SINGLE_EXT("Speaker Switch", WM8962_CLASS_D_CONTROL_1, 1, 1, 1,
	       snd_soc_get_volsw, wm8962_put_spk_sw),

SOC_SINGLE("Speaker Mixer Switch", WM8962_SPEAKER_MIXER_3, 8, 1, 1),
SOC_SINGLE_TLV("Speaker Mixer IN4L Volume", WM8962_SPEAKER_MIXER_3,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("Speaker Mixer IN4R Volume", WM8962_SPEAKER_MIXER_3,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("Speaker Mixer MIXINL Volume", WM8962_SPEAKER_MIXER_3,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("Speaker Mixer MIXINR Volume", WM8962_SPEAKER_MIXER_3,
	       6, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("Speaker Mixer DACL Volume", WM8962_SPEAKER_MIXER_5,
	       7, 1, 0, inmix_tlv),
SOC_SINGLE_TLV("Speaker Mixer DACR Volume", WM8962_SPEAKER_MIXER_5,
	       6, 1, 0, inmix_tlv),
};

static const struct snd_kcontrol_new wm8962_spk_stereo_controls[] = {
SOC_DOUBLE_R_TLV("Speaker Volume", WM8962_SPKOUTL_VOLUME,
		WM8962_SPKOUTR_VOLUME, 0, 127, 0, out_tlv),
SOC_DOUBLE_EXT("Speaker Switch", WM8962_CLASS_D_CONTROL_1, 1, 0, 1, 1,
	       snd_soc_get_volsw, wm8962_put_spk_sw),

SOC_DOUBLE_R("Speaker Mixer Switch", WM8962_SPEAKER_MIXER_3,
	     WM8962_SPEAKER_MIXER_4, 8, 1, 1),

SOC_SINGLE_TLV("SPKOUTL Mixer IN4L Volume", WM8962_SPEAKER_MIXER_3,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer IN4R Volume", WM8962_SPEAKER_MIXER_3,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer MIXINL Volume", WM8962_SPEAKER_MIXER_3,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer MIXINR Volume", WM8962_SPEAKER_MIXER_3,
	       6, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer DACL Volume", WM8962_SPEAKER_MIXER_5,
	       7, 1, 0, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer DACR Volume", WM8962_SPEAKER_MIXER_5,
	       6, 1, 0, inmix_tlv),

SOC_SINGLE_TLV("SPKOUTR Mixer IN4L Volume", WM8962_SPEAKER_MIXER_4,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer IN4R Volume", WM8962_SPEAKER_MIXER_4,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer MIXINL Volume", WM8962_SPEAKER_MIXER_4,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer MIXINR Volume", WM8962_SPEAKER_MIXER_4,
	       6, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer DACL Volume", WM8962_SPEAKER_MIXER_5,
	       5, 1, 0, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer DACR Volume", WM8962_SPEAKER_MIXER_5,
	       4, 1, 0, inmix_tlv),
};

static int cp_event(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(5);
		break;

	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief hp_event() 
 * When the head phone is detected, program the Head Phone DC 
 * servo, enable and start head phone. 
 * If codec diags mode is in automated speaker test, do not 
 * program the head phone and only enable speakers 
 * 
 * @param w 
 * @param kcontrol 
 * @param event 
 * 
 * @return int 
 */
static int hp_event(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int timeout;
	int reg;
	int expected = (WM8962_DCS_STARTUP_DONE_HP1L |
			WM8962_DCS_STARTUP_DONE_HP1R);

#ifdef DEBUG
    print_wm8962_elm("hp_event", dapm_event_elm, event);
    print_wm8962_codec_diags_mode("hp_event", wm8962_codec_diags_mode);
#endif // DEBUG

    /**
     * Skip if codec diags mode is in automated speaker test.
     */
    switch (wm8962_codec_diags_mode) {
    case WM8962_CODEC_DIAGS_MODE_NORMAL:
        break;
    case WM8962_CODEC_DIAGS_MODE_MIC_TEST:
    case WM8962_CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST:
        dev_err(codec->dev, "%s: Do nothing in MIC and Automated Speaker test\n", __func__);
        return 0;
    }
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_ENA | WM8962_HP1R_ENA,
				    WM8962_HP1L_ENA | WM8962_HP1R_ENA);
		udelay(20);

		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_ENA_DLY | WM8962_HP1R_ENA_DLY,
				    WM8962_HP1L_ENA_DLY | WM8962_HP1R_ENA_DLY);

		/* Start the DC servo */
		snd_soc_update_bits(codec, WM8962_DC_SERVO_1,
				    WM8962_HP1L_DCS_ENA | WM8962_HP1R_DCS_ENA |
				    WM8962_HP1L_DCS_STARTUP |
				    WM8962_HP1R_DCS_STARTUP,
				    WM8962_HP1L_DCS_ENA | WM8962_HP1R_DCS_ENA |
				    WM8962_HP1L_DCS_STARTUP |
				    WM8962_HP1R_DCS_STARTUP);

		/* Wait for it to complete, should be well under 100ms */
		timeout = 0;
		do {
			msleep(1);
			reg = snd_soc_read(codec, WM8962_DC_SERVO_6);
			if (reg < 0) {
				dev_err(codec->dev,
					"Failed to read DCS status: %d\n",
					reg);
				continue;
			}
			dev_dbg(codec->dev, "DCS status: %x\n", reg);
		} while (++timeout < 200 && (reg & expected) != expected);

		if ((reg & expected) != expected)
			dev_err(codec->dev, "%s: DC servo timed out\n", __func__);
		else
			dev_dbg(codec->dev, "%s: DC servo complete after %dms\n", __func__,
				timeout);

		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_ENA_OUTP |
				    WM8962_HP1R_ENA_OUTP,
				    WM8962_HP1L_ENA_OUTP |
				    WM8962_HP1R_ENA_OUTP);
		udelay(20);

		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_RMV_SHORT |
				    WM8962_HP1R_RMV_SHORT,
				    WM8962_HP1L_RMV_SHORT |
				    WM8962_HP1R_RMV_SHORT);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_RMV_SHORT |
				    WM8962_HP1R_RMV_SHORT, 0);

		udelay(20);

		snd_soc_update_bits(codec, WM8962_DC_SERVO_1,
				    WM8962_HP1L_DCS_ENA | WM8962_HP1R_DCS_ENA |
				    WM8962_HP1L_DCS_STARTUP |
				    WM8962_HP1R_DCS_STARTUP,
				    0);

		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_ENA | WM8962_HP1R_ENA |
				    WM8962_HP1L_ENA_DLY | WM8962_HP1R_ENA_DLY |
				    WM8962_HP1L_ENA_OUTP |
				    WM8962_HP1R_ENA_OUTP, 0);
				    
		break;

	default:
		BUG();
		return -EINVAL;
	
	}

	return 0;
}

/* VU bits for the output PGAs only take effect while the PGA is powered */
/**
 * @brief out_pga_event() 
 * When the pga is enabled, if it is in automated speaker mode then only update the speakers volume. 
 * Otherwise, just program the volume register. 
 * 
 * @param w 
 * @param kcontrol 
 * @param event 
 * 
 * @return int 
 */
static int out_pga_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 *reg_cache = codec->reg_cache;
	int reg;

	switch (wm8962_codec_diags_mode) {
	case WM8962_CODEC_DIAGS_MODE_NORMAL:
		break;
	case WM8962_CODEC_DIAGS_MODE_MIC_TEST:
		dev_err(codec->dev, "%s: Do nothing in MIC test\n", __func__);
	return 0;
}

#ifdef DEBUG
    print_wm8962_elm("out_pga_event", dapm_event_elm   , event);
    print_wm8962_elm("out_pga_event", out_pga_event_elm, event);
    print_wm8962_codec_diags_mode("out_pga_event", wm8962_codec_diags_mode);
#endif // DEBUG

	switch (w->shift) {
	case WM8962_HPOUTR_PGA_ENA_SHIFT:
		reg = WM8962_HPOUTR_VOLUME;
        if (wm8962_codec_diags_mode == WM8962_CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST) 
        {
            /**
             * Ignore Head Phone volume in automated speakers test mode.
             */
            printk(KERN_DEBUG "%s: WM8962_HPOUTR_VOLUME, do nothing\n", __func__);
            return 0;
        }
				if (!headset_detected)
					return 0;
		break;
	case WM8962_HPOUTL_PGA_ENA_SHIFT:
		reg = WM8962_HPOUTL_VOLUME;
        if (wm8962_codec_diags_mode == WM8962_CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST) 
        {
            /**
             * Ignore head phone volume in automated speakers test mode
             */
            printk(KERN_DEBUG "%s: WM8962_HPOUTL_VOLUME, do nothing\n", __func__);
            return 0;
        }
				if (!headset_detected)
					return 0;
		break;
	case WM8962_SPKOUTR_PGA_ENA_SHIFT:
		reg = WM8962_SPKOUTR_VOLUME;
        printk(KERN_DEBUG "%s: WM8962_SPKOUTR_VOLUME reg: %d 0x%02X\n", 
               __func__, reg, reg);
		if (wm8962_codec_diags_mode == WM8962_CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST)
					reg_cache[reg] = WM8962_DIAGS_SPKR_VOL;
				else if (headset_detected)
					return 0;
		break;
	case WM8962_SPKOUTL_PGA_ENA_SHIFT:
		reg = WM8962_SPKOUTL_VOLUME;
        printk(KERN_DEBUG "%s: WM8962_SPKOUTL_VOLUME reg: %d 0x%02X\n", 
               __func__, reg, reg);
		if (wm8962_codec_diags_mode == WM8962_CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST)
					reg_cache[reg] = WM8962_DIAGS_SPKR_VOL;
				else if (headset_detected)
					return 0;
		break;
	default:
		BUG();
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		return snd_soc_write(codec, reg, reg_cache[reg]);
	default:
		BUG();
		return -EINVAL;
	}
}

static const char *st_text[] = { "None", "Right", "Left" };

static const struct soc_enum str_enum =
	SOC_ENUM_SINGLE(WM8962_DAC_DSP_MIXING_1, 2, 3, st_text);

static const struct snd_kcontrol_new str_mux =
	SOC_DAPM_ENUM("Right Sidetone", str_enum);

static const struct soc_enum stl_enum =
	SOC_ENUM_SINGLE(WM8962_DAC_DSP_MIXING_2, 2, 3, st_text);

static const struct snd_kcontrol_new stl_mux =
	SOC_DAPM_ENUM("Left Sidetone", stl_enum);

static const char *outmux_text[] = { "DAC", "Mixer" };

static const struct soc_enum spkoutr_enum =
	SOC_ENUM_SINGLE(WM8962_SPEAKER_MIXER_2, 7, 2, outmux_text);

static const struct snd_kcontrol_new spkoutr_mux =
	SOC_DAPM_ENUM("SPKOUTR Mux", spkoutr_enum);

static const struct soc_enum spkoutl_enum =
	SOC_ENUM_SINGLE(WM8962_SPEAKER_MIXER_1, 7, 2, outmux_text);

static const struct snd_kcontrol_new spkoutl_mux =
	SOC_DAPM_ENUM("SPKOUTL Mux", spkoutl_enum);

static const struct soc_enum hpoutr_enum =
	SOC_ENUM_SINGLE(WM8962_HEADPHONE_MIXER_2, 7, 2, outmux_text);

static const struct snd_kcontrol_new hpoutr_mux =
	SOC_DAPM_ENUM("HPOUTR Mux", hpoutr_enum);

static const struct soc_enum hpoutl_enum =
	SOC_ENUM_SINGLE(WM8962_HEADPHONE_MIXER_1, 7, 2, outmux_text);

static const struct snd_kcontrol_new hpoutl_mux =
	SOC_DAPM_ENUM("HPOUTL Mux", hpoutl_enum);

static const struct snd_kcontrol_new inpgal[] = {
SOC_DAPM_SINGLE("IN1L Switch", WM8962_LEFT_INPUT_PGA_CONTROL, 3, 1, 0),
SOC_DAPM_SINGLE("IN2L Switch", WM8962_LEFT_INPUT_PGA_CONTROL, 2, 1, 0),
SOC_DAPM_SINGLE("IN3L Switch", WM8962_LEFT_INPUT_PGA_CONTROL, 1, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_LEFT_INPUT_PGA_CONTROL, 0, 1, 0),
};

static const struct snd_kcontrol_new inpgar[] = {
SOC_DAPM_SINGLE("IN1R Switch", WM8962_RIGHT_INPUT_PGA_CONTROL, 3, 1, 0),
SOC_DAPM_SINGLE("IN2R Switch", WM8962_RIGHT_INPUT_PGA_CONTROL, 2, 1, 0),
SOC_DAPM_SINGLE("IN3R Switch", WM8962_RIGHT_INPUT_PGA_CONTROL, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_RIGHT_INPUT_PGA_CONTROL, 0, 1, 0),
};

static const struct snd_kcontrol_new mixinl[] = {
SOC_DAPM_SINGLE("IN2L Switch", WM8962_INPUT_MIXER_CONTROL_2, 5, 1, 0),
SOC_DAPM_SINGLE("IN3L Switch", WM8962_INPUT_MIXER_CONTROL_2, 4, 1, 0),
SOC_DAPM_SINGLE("PGA Switch", WM8962_INPUT_MIXER_CONTROL_2, 3, 1, 0),
};

static const struct snd_kcontrol_new mixinr[] = {
SOC_DAPM_SINGLE("IN2R Switch", WM8962_INPUT_MIXER_CONTROL_2, 2, 1, 0),
SOC_DAPM_SINGLE("IN3R Switch", WM8962_INPUT_MIXER_CONTROL_2, 1, 1, 0),
SOC_DAPM_SINGLE("PGA Switch", WM8962_INPUT_MIXER_CONTROL_2, 0, 1, 0),
};

static const struct snd_kcontrol_new hpmixl[] = {
SOC_DAPM_SINGLE("DACL Switch", WM8962_HEADPHONE_MIXER_1, 5, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8962_HEADPHONE_MIXER_1, 4, 1, 0),
SOC_DAPM_SINGLE("MIXINL Switch", WM8962_HEADPHONE_MIXER_1, 3, 1, 0),
SOC_DAPM_SINGLE("MIXINR Switch", WM8962_HEADPHONE_MIXER_1, 2, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_HEADPHONE_MIXER_1, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_HEADPHONE_MIXER_1, 0, 1, 0),
};

static const struct snd_kcontrol_new hpmixr[] = {
SOC_DAPM_SINGLE("DACL Switch", WM8962_HEADPHONE_MIXER_2, 5, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8962_HEADPHONE_MIXER_2, 4, 1, 0),
SOC_DAPM_SINGLE("MIXINL Switch", WM8962_HEADPHONE_MIXER_2, 3, 1, 0),
SOC_DAPM_SINGLE("MIXINR Switch", WM8962_HEADPHONE_MIXER_2, 2, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_HEADPHONE_MIXER_2, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_HEADPHONE_MIXER_2, 0, 1, 0),
};

static const struct snd_kcontrol_new spkmixl[] = {
SOC_DAPM_SINGLE("DACL Switch", WM8962_SPEAKER_MIXER_1, 5, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8962_SPEAKER_MIXER_1, 4, 1, 0),
SOC_DAPM_SINGLE("MIXINL Switch", WM8962_SPEAKER_MIXER_1, 3, 1, 0),
SOC_DAPM_SINGLE("MIXINR Switch", WM8962_SPEAKER_MIXER_1, 2, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_SPEAKER_MIXER_1, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_SPEAKER_MIXER_1, 0, 1, 0),
};

static const struct snd_kcontrol_new spkmixr[] = {
SOC_DAPM_SINGLE("DACL Switch", WM8962_SPEAKER_MIXER_2, 5, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8962_SPEAKER_MIXER_2, 4, 1, 0),
SOC_DAPM_SINGLE("MIXINL Switch", WM8962_SPEAKER_MIXER_2, 3, 1, 0),
SOC_DAPM_SINGLE("MIXINR Switch", WM8962_SPEAKER_MIXER_2, 2, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_SPEAKER_MIXER_2, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_SPEAKER_MIXER_2, 0, 1, 0),
};

static const struct snd_soc_dapm_widget wm8962_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("IN1L"),
SND_SOC_DAPM_INPUT("IN1R"),
SND_SOC_DAPM_INPUT("IN2L"),
SND_SOC_DAPM_INPUT("IN2R"),
SND_SOC_DAPM_INPUT("IN3L"),
SND_SOC_DAPM_INPUT("IN3R"),
SND_SOC_DAPM_INPUT("IN4L"),
SND_SOC_DAPM_INPUT("IN4R"),
SND_SOC_DAPM_INPUT("Beep"),

SND_SOC_DAPM_SUPPLY("Class G", WM8962_CHARGE_PUMP_B, 0, 1, NULL, 0),
SND_SOC_DAPM_SUPPLY("SYSCLK", SND_SOC_NOPM, 5, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("Charge Pump", WM8962_CHARGE_PUMP_1, 0, 0, cp_event,
		    SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_SUPPLY("TOCLK", WM8962_ADDITIONAL_CONTROL_1, 0, 0, NULL, 0),

SND_SOC_DAPM_MIXER("INPGAL", WM8962_LEFT_INPUT_PGA_CONTROL, 4, 0,
		   inpgal, ARRAY_SIZE(inpgal)),
SND_SOC_DAPM_MIXER("INPGAR", WM8962_RIGHT_INPUT_PGA_CONTROL, 4, 0,
		   inpgar, ARRAY_SIZE(inpgar)),
SND_SOC_DAPM_MIXER("MIXINL", WM8962_PWR_MGMT_1, 5, 0,
		   mixinl, ARRAY_SIZE(mixinl)),
SND_SOC_DAPM_MIXER("MIXINR", WM8962_PWR_MGMT_1, 4, 0,
		   mixinr, ARRAY_SIZE(mixinr)),

SND_SOC_DAPM_ADC("ADCL", "Capture", WM8962_PWR_MGMT_1, 3, 0),
SND_SOC_DAPM_ADC("ADCR", "Capture", WM8962_PWR_MGMT_1, 2, 0),

SND_SOC_DAPM_MUX("STL", SND_SOC_NOPM, 0, 0, &stl_mux),
SND_SOC_DAPM_MUX("STR", SND_SOC_NOPM, 0, 0, &str_mux),

SND_SOC_DAPM_DAC("DACL", "Playback", WM8962_PWR_MGMT_2, 8, 0),
SND_SOC_DAPM_DAC("DACR", "Playback", WM8962_PWR_MGMT_2, 7, 0),

SND_SOC_DAPM_PGA("Left Bypass", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Bypass", SND_SOC_NOPM, 0, 0, NULL, 0),

SND_SOC_DAPM_MIXER("HPMIXL", WM8962_MIXER_ENABLES, 3, 0,
		   hpmixl, ARRAY_SIZE(hpmixl)),
SND_SOC_DAPM_MIXER("HPMIXR", WM8962_MIXER_ENABLES, 2, 0,
		   hpmixr, ARRAY_SIZE(hpmixr)),

SND_SOC_DAPM_MUX_E("HPOUTL PGA", WM8962_PWR_MGMT_2, 6, 0, &hpoutl_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_MUX_E("HPOUTR PGA", WM8962_PWR_MGMT_2, 5, 0, &hpoutr_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_PGA_E("HPOUT", SND_SOC_NOPM, 0, 0, NULL, 0, hp_event,
		   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_OUTPUT("HPOUTL"),
SND_SOC_DAPM_OUTPUT("HPOUTR"),
};

static const struct snd_soc_dapm_widget wm8962_dapm_spk_mono_widgets[] = {
SND_SOC_DAPM_MIXER("Speaker Mixer", WM8962_MIXER_ENABLES, 1, 0,
		   spkmixl, ARRAY_SIZE(spkmixl)),
SND_SOC_DAPM_MUX_E("Speaker PGA", WM8962_PWR_MGMT_2, 4, 0, &spkoutl_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA("Speaker Output", WM8962_CLASS_D_CONTROL_1, 7, 0, NULL, 0),
SND_SOC_DAPM_OUTPUT("SPKOUT"),
};

static const struct snd_soc_dapm_widget wm8962_dapm_spk_stereo_widgets[] = {
SND_SOC_DAPM_MIXER("SPKOUTL Mixer", WM8962_MIXER_ENABLES, 1, 0,
		   spkmixl, ARRAY_SIZE(spkmixl)),
SND_SOC_DAPM_MIXER("SPKOUTR Mixer", WM8962_MIXER_ENABLES, 0, 0,
		   spkmixr, ARRAY_SIZE(spkmixr)),

SND_SOC_DAPM_MUX_E("SPKOUTL PGA", WM8962_PWR_MGMT_2, 4, 0, &spkoutl_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_MUX_E("SPKOUTR PGA", WM8962_PWR_MGMT_2, 3, 0, &spkoutr_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_PGA("SPKOUTR Output", WM8962_CLASS_D_CONTROL_1, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("SPKOUTL Output", WM8962_CLASS_D_CONTROL_1, 6, 0, NULL, 0),

SND_SOC_DAPM_OUTPUT("SPKOUTL"),
SND_SOC_DAPM_OUTPUT("SPKOUTR"),
};

static const struct snd_soc_dapm_route wm8962_intercon[] = {
	{ "INPGAL", "IN1L Switch", "IN1L" },
	{ "INPGAL", "IN2L Switch", "IN2L" },
	{ "INPGAL", "IN3L Switch", "IN3L" },
	{ "INPGAL", "IN4L Switch", "IN4L" },

	{ "INPGAR", "IN1R Switch", "IN1R" },
	{ "INPGAR", "IN2R Switch", "IN2R" },
	{ "INPGAR", "IN3R Switch", "IN3R" },
	{ "INPGAR", "IN4R Switch", "IN4R" },

	{ "MIXINL", "IN2L Switch", "IN2L" },
	{ "MIXINL", "IN3L Switch", "IN3L" },
	{ "MIXINL", "PGA Switch", "INPGAL" },

	{ "MIXINR", "IN2R Switch", "IN2R" },
	{ "MIXINR", "IN3R Switch", "IN3R" },
	{ "MIXINR", "PGA Switch", "INPGAR" },

	{ "ADCL", NULL, "SYSCLK" },
	{ "ADCL", NULL, "TOCLK" },
	{ "ADCL", NULL, "MIXINL" },

	{ "ADCR", NULL, "SYSCLK" },
	{ "ADCR", NULL, "TOCLK" },
	{ "ADCR", NULL, "MIXINR" },

	{ "STL", "Left", "ADCL" },
	{ "STL", "Right", "ADCR" },

	{ "STR", "Left", "ADCL" },
	{ "STR", "Right", "ADCR" },

	{ "DACL", NULL, "SYSCLK" },
	{ "DACL", NULL, "TOCLK" },
	{ "DACL", NULL, "Beep" },
	{ "DACL", NULL, "STL" },

	{ "DACR", NULL, "SYSCLK" },
	{ "DACR", NULL, "TOCLK" },
	{ "DACR", NULL, "Beep" },
	{ "DACR", NULL, "STR" },

	{ "HPMIXL", "IN4L Switch", "IN4L" },
	{ "HPMIXL", "IN4R Switch", "IN4R" },
	{ "HPMIXL", "DACL Switch", "DACL" },
	{ "HPMIXL", "DACR Switch", "DACR" },
	{ "HPMIXL", "MIXINL Switch", "MIXINL" },
	{ "HPMIXL", "MIXINR Switch", "MIXINR" },

	{ "HPMIXR", "IN4L Switch", "IN4L" },
	{ "HPMIXR", "IN4R Switch", "IN4R" },
	{ "HPMIXR", "DACL Switch", "DACL" },
	{ "HPMIXR", "DACR Switch", "DACR" },
	{ "HPMIXR", "MIXINL Switch", "MIXINL" },
	{ "HPMIXR", "MIXINR Switch", "MIXINR" },

	{ "Left Bypass", NULL, "HPMIXL" },
	{ "Left Bypass", NULL, "Class G" },

	{ "Right Bypass", NULL, "HPMIXR" },
	{ "Right Bypass", NULL, "Class G" },

	{ "HPOUTL PGA", "Mixer", "Left Bypass" },
	{ "HPOUTL PGA", "DAC", "DACL" },

	{ "HPOUTR PGA", "Mixer", "Right Bypass" },
	{ "HPOUTR PGA", "DAC", "DACR" },

	{ "HPOUT", NULL, "HPOUTL PGA" },
	{ "HPOUT", NULL, "HPOUTR PGA" },
	{ "HPOUT", NULL, "Charge Pump" },
	{ "HPOUT", NULL, "SYSCLK" },
	{ "HPOUT", NULL, "TOCLK" },

	{ "HPOUTL", NULL, "HPOUT" },
	{ "HPOUTR", NULL, "HPOUT" },
};

static const struct snd_soc_dapm_route wm8962_spk_mono_intercon[] = {
	{ "Speaker Mixer", "IN4L Switch", "IN4L" },
	{ "Speaker Mixer", "IN4R Switch", "IN4R" },
	{ "Speaker Mixer", "DACL Switch", "DACL" },
	{ "Speaker Mixer", "DACR Switch", "DACR" },
	{ "Speaker Mixer", "MIXINL Switch", "MIXINL" },
	{ "Speaker Mixer", "MIXINR Switch", "MIXINR" },

	{ "Speaker PGA", "Mixer", "Speaker Mixer" },
	{ "Speaker PGA", "DAC", "DACL" },

	{ "Speaker Output", NULL, "Speaker PGA" },
	{ "Speaker Output", NULL, "SYSCLK" },
	{ "Speaker Output", NULL, "TOCLK" },

	{ "SPKOUT", NULL, "Speaker Output" },
};

static const struct snd_soc_dapm_route wm8962_spk_stereo_intercon[] = {
	{ "SPKOUTL Mixer", "IN4L Switch", "IN4L" },
	{ "SPKOUTL Mixer", "IN4R Switch", "IN4R" },
	{ "SPKOUTL Mixer", "DACL Switch", "DACL" },
	{ "SPKOUTL Mixer", "DACR Switch", "DACR" },
	{ "SPKOUTL Mixer", "MIXINL Switch", "MIXINL" },
	{ "SPKOUTL Mixer", "MIXINR Switch", "MIXINR" },

	{ "SPKOUTR Mixer", "IN4L Switch", "IN4L" },
	{ "SPKOUTR Mixer", "IN4R Switch", "IN4R" },
	{ "SPKOUTR Mixer", "DACL Switch", "DACL" },
	{ "SPKOUTR Mixer", "DACR Switch", "DACR" },
	{ "SPKOUTR Mixer", "MIXINL Switch", "MIXINL" },
	{ "SPKOUTR Mixer", "MIXINR Switch", "MIXINR" },

	{ "SPKOUTL PGA", "Mixer", "SPKOUTL Mixer" },
	{ "SPKOUTL PGA", "DAC", "DACL" },

	{ "SPKOUTR PGA", "Mixer", "SPKOUTR Mixer" },
	{ "SPKOUTR PGA", "DAC", "DACR" },

	{ "SPKOUTL Output", NULL, "SPKOUTL PGA" },
	{ "SPKOUTL Output", NULL, "SYSCLK" },
	{ "SPKOUTL Output", NULL, "TOCLK" },

	{ "SPKOUTR Output", NULL, "SPKOUTR PGA" },
	{ "SPKOUTR Output", NULL, "SYSCLK" },
	{ "SPKOUTR Output", NULL, "TOCLK" },

	{ "SPKOUTL", NULL, "SPKOUTL Output" },
	{ "SPKOUTR", NULL, "SPKOUTR Output" },
};

static int wm8962_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_add_controls(codec, wm8962_snd_controls,
			     ARRAY_SIZE(wm8962_snd_controls));
	snd_soc_add_controls(codec, wm8962_spk_stereo_controls,
			     ARRAY_SIZE(wm8962_spk_stereo_controls));


	snd_soc_dapm_new_controls(codec, wm8962_dapm_widgets,
			  ARRAY_SIZE(wm8962_dapm_widgets));
	snd_soc_dapm_new_controls(codec, wm8962_dapm_spk_stereo_widgets,
				  ARRAY_SIZE(wm8962_dapm_spk_stereo_widgets));

	snd_soc_dapm_add_routes(codec, wm8962_intercon,
				ARRAY_SIZE(wm8962_intercon));
	snd_soc_dapm_add_routes(codec, wm8962_spk_stereo_intercon,
				ARRAY_SIZE(wm8962_spk_stereo_intercon));


	snd_soc_dapm_disable_pin(codec, "Beep");

	return 0;
}

static void wm8962_sync_cache(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = codec->private_data;
	int i;

	
#ifdef LAB126
	
	dev_dbg(codec->dev, "Syncing cache\n");
#else

	dev_err(codec->dev, "Syncing cache\n");
#endif
	/* Sync back cached values if they're different from the
	 * hardware default.
	 */
	for (i = 1; i < ARRAY_SIZE(wm8962->reg_cache); i++) {
		if (i == WM8962_SOFTWARE_RESET)
			continue;
		if (wm8962->reg_cache[i] == wm8962_reg[i])
			continue;

		snd_soc_write(codec, i, wm8962->reg_cache[i]);
	}

}

/* -1 for reserved values */
static const int bclk_divs[] = {
	1, -1, 2, 3, 4, -1, 6, 8, -1, 12, 16, 24, -1, 32, 32, 32, 256
};

static void wm8962_configure_bclk(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = codec->private_data;
	int dspclk, i;
	int clocking2 = 0;
	int aif2 = 0;

	if (!wm8962->bclk) {
		
#ifdef   LAB126
		dev_dbg(codec->dev, "No BCLK rate configured\n");
#else

		dev_err(codec->dev, "No BCLK rate configured\n");
#endif
		return;
	}

	dspclk = snd_soc_read(codec, WM8962_CLOCKING1);
	if (dspclk < 0) {
		dev_err(codec->dev, "Failed to read DSPCLK: %d\n", dspclk);
		return;
	}

	dspclk = (dspclk & WM8962_DSPCLK_DIV_MASK) >> WM8962_DSPCLK_DIV_SHIFT;
	switch (dspclk) {
	case 0:
		dspclk = wm8962->sysclk_rate;
		break;
	case 1:
		dspclk = wm8962->sysclk_rate / 2;
		break;
	case 2:
		dspclk = wm8962->sysclk_rate / 4;
		break;
	default:
		dev_warn(codec->dev, "Unknown DSPCLK divisor read back\n");
		dspclk = wm8962->sysclk;
	}

#ifdef LAB126

	dev_dbg(codec->dev, "DSPCLK is %dHz, BCLK %d\n", dspclk, wm8962->bclk);
#else

	dev_err(codec->dev, "DSPCLK is %dHz, BCLK %d\n", dspclk, wm8962->bclk);
#endif
	/* We're expecting an exact match */
	for (i = 0; i < ARRAY_SIZE(bclk_divs); i++) {
		if (bclk_divs[i] < 0)
			continue;

		if (dspclk / bclk_divs[i] == wm8962->bclk) {
			
			
#ifdef LAB126
			dev_dbg(codec->dev, "Selected BCLK_DIV %d for %dHz\n",bclk_divs[i], wm8962->bclk);

#else
			dev_err(codec->dev, "Selected BCLK_DIV %d for %dHz\n",bclk_divs[i], wm8962->bclk);
#endif
			
			clocking2 |= i;
			break;
		}
	}
	if (i == ARRAY_SIZE(bclk_divs)) {
		dev_err(codec->dev, "Unsupported BCLK ratio %d\n",
			dspclk / wm8962->bclk);
		return;
	}

	aif2 |= wm8962->bclk / wm8962->lrclk;


#ifdef LAB126
	
	dev_dbg(codec->dev, "Selected LRCLK divisor %d for %dHz\n",wm8962->bclk / wm8962->lrclk, wm8962->lrclk);
#else

	dev_err(codec->dev, "Selected LRCLK divisor %d for %dHz\n",wm8962->bclk / wm8962->lrclk, wm8962->lrclk);
#endif

	snd_soc_update_bits(codec, WM8962_CLOCKING2,
			    WM8962_BCLK_DIV_MASK, clocking2);
	snd_soc_update_bits(codec, WM8962_AUDIO_INTERFACE_2,
			    WM8962_AIF_RATE_MASK, aif2);
}

static int wm8962_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	struct wm8962_priv *wm8962 = codec->private_data;
	
	if (level == codec->bias_level)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/* VMID 2*50k */
		snd_soc_update_bits(codec, WM8962_PWR_MGMT_1,
				    WM8962_VMID_SEL_MASK, 0x80);
		break;

	case SND_SOC_BIAS_STANDBY:

		if (codec->bias_level == SND_SOC_BIAS_OFF) {
#if 0
			ret = regulator_bulk_enable(ARRAY_SIZE(wm8962->supplies),
						    wm8962->supplies);
			if (ret != 0) {
				dev_err(codec->dev,
					"Failed to enable supplies: %d\n",
					ret);
				return ret;
				}
#endif		

			wm8962_sync_cache(codec);
  
			/* Improve interoperability of IN4 DC measurement */
			snd_soc_write(codec, 0xFD, 0x1);
			snd_soc_write(codec, 0xCC, 0x40);
			snd_soc_write(codec, 0xFD, 0x0);
	
			snd_soc_update_bits(codec, WM8962_ANTI_POP,
					    WM8962_STARTUP_BIAS_ENA |
					    WM8962_VMID_BUF_ENA,
					    WM8962_STARTUP_BIAS_ENA |
					    WM8962_VMID_BUF_ENA);

			/* Bias enable at 2*50k for ramp */
			snd_soc_update_bits(codec, WM8962_PWR_MGMT_1,
					    WM8962_VMID_SEL_MASK |
					    WM8962_BIAS_ENA,
					    WM8962_BIAS_ENA | 0x180);

			/* Enable MICBIAS (permanantly for detection) */
			snd_soc_update_bits(codec, WM8962_PWR_MGMT_1, WM8962_MICBIAS_ENA,
					WM8962_MICBIAS_ENA);
	
			snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_4,
					WM8962_MICDET_ENA, WM8962_MICDET_ENA);
	
			/* Configure microphone detection */
			snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_4,
					WM8962_MICDET_THR_MASK,
					0 << WM8962_MICDET_THR_SHIFT);

			/* Enable Thermal Shutdown */
			snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_4,
					WM8962_TEMP_ENA_HP_MASK | WM8962_TEMP_ENA_SPK_MASK, 
					WM8962_TEMP_ENA_HP | WM8962_TEMP_ENA_SPK);
			
			snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_1,
					WM8962_THERR_ACT_MASK, WM8962_THERR_ACT);
			
			msleep(5);

			snd_soc_update_bits(codec, WM8962_CLOCKING2,
					    WM8962_CLKREG_OVD,
					    WM8962_CLKREG_OVD);
//			printk(KERN_WARNING "wm8962_set_bias_level\n");
			wm8962_configure_bclk(codec);
		}

		/* VMID 2*250k */
		snd_soc_update_bits(codec, WM8962_PWR_MGMT_1,
				    WM8962_VMID_SEL_MASK, 0x100);
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, WM8962_PWR_MGMT_1, 0);
    /*snd_soc_update_bits(codec, WM8962_PWR_MGMT_1,
    		    WM8962_VMID_SEL_MASK | WM8962_BIAS_ENA, 0);*/

		snd_soc_update_bits(codec, WM8962_ANTI_POP,
					WM8962_STARTUP_BIAS_ENA |
					WM8962_VMID_BUF_ENA, 0);
	
		snd_soc_update_bits(codec, WM8962_ADC_DAC_CONTROL_1,
					WM8962_DAC_MUTE, WM8962_DAC_MUTE);
	    
		/* Disable Thermal Shutdown */
		snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_4,
					WM8962_TEMP_ENA_HP_MASK | WM8962_TEMP_ENA_SPK_MASK, 0);
		
		snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_1,
					WM8962_THERR_ACT_MASK, 0);

#if 0
		regulator_bulk_disable(ARRAY_SIZE(wm8962->supplies),
				       wm8962->supplies);
#endif
		break;
	}
	codec->bias_level = level;
	return 0;
}

static const struct {
	int rate;
	int reg;
} sr_vals[] = {
	{ 48000, 0 },
	{ 44100, 0 },
	{ 32000, 1 },
	{ 22050, 2 },
	{ 24000, 2 },
	{ 16000, 3 },
	{ 11025, 4 },
	{ 12000, 4 },
	{ 8000,  5 },
	{ 88200, 6 },
	{ 96000, 6 },
};

static const int sysclk_rates[] = {
	64, 128, 192, 256, 384, 512, 768, 1024, 1408, 1536,
};

static int wm8962_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct wm8962_priv *wm8962 = codec->private_data;
	int rate = params_rate(params);
	int i;
	int aif0 = 0;
	int adctl3 = 0;
	int clocking4 = 0;

	wm8962->bclk = 32 * params_rate(params);
	wm8962->lrclk = params_rate(params);

	for (i = 0; i < ARRAY_SIZE(sr_vals); i++) {
		if (sr_vals[i].rate == rate) {
			adctl3 |= sr_vals[i].reg;
			break;
		}
	}
	if (i == ARRAY_SIZE(sr_vals)) {
		dev_err(codec->dev, "Unsupported rate %dHz\n", rate);
		return -EINVAL;
	}

	if (rate % 8000 == 0)
		adctl3 |= WM8962_SAMPLE_RATE_INT_MODE;

	for (i = 0; i < ARRAY_SIZE(sysclk_rates); i++) {
		if (sysclk_rates[i] == wm8962->sysclk_rate / rate) {
			clocking4 |= i << WM8962_SYSCLK_RATE_SHIFT;
			break;
		}
	}
	if (i == ARRAY_SIZE(sysclk_rates)) {
		dev_err(codec->dev, "Unsupported sysclk ratio %d\n",
			wm8962->sysclk_rate / rate);
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		aif0 |= 0x40;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		aif0 |= 0x80;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		aif0 |= 0xc0;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, WM8962_AUDIO_INTERFACE_0,
			    WM8962_WL_MASK, aif0);
	snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_3,
			    WM8962_SAMPLE_RATE_INT_MODE |
			    WM8962_SAMPLE_RATE_MASK, adctl3);
	snd_soc_update_bits(codec, WM8962_CLOCKING_4,
			    WM8962_SYSCLK_RATE_MASK, clocking4);

	printk(KERN_WARNING "wm8962_hw_params\n");
	wm8962_configure_bclk(codec);

	return 0;
}

static int wm8962_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				 unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wm8962_priv *wm8962 = codec->private_data;
	int src = snd_soc_read(codec, WM8962_CLOCKING2) & WM8962_SYSCLK_ENA;
	snd_soc_update_bits(codec, WM8962_CLOCKING2, WM8962_SYSCLK_ENA, 0);

	switch (clk_id) {
	case WM8962_SYSCLK_MCLK:
		wm8962->sysclk = WM8962_SYSCLK_MCLK;
		break;
	case WM8962_SYSCLK_FLL:
		wm8962->sysclk = WM8962_SYSCLK_FLL;
		src |= 1 << WM8962_SYSCLK_SRC_SHIFT;
		WARN_ON(freq != wm8962->fll_fout);
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, WM8962_CLOCKING2,
				WM8962_SYSCLK_SRC_MASK | WM8962_SYSCLK_ENA,
				src | WM8962_SYSCLK_ENA);	
	
	wm8962->sysclk_rate = freq;

	return 0;
}

static int wm8962_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int aif0 = 0;
	int src = 0;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		aif0 |= WM8962_LRCLK_INV;
	case SND_SOC_DAIFMT_DSP_B:
		aif0 |= 3;

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
		case SND_SOC_DAIFMT_IB_NF:
			break;
		default:
			return -EINVAL;
		}
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		aif0 |= 1;
		break;
	case SND_SOC_DAIFMT_I2S:
		aif0 |= 2;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		aif0 |= WM8962_BCLK_INV;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		aif0 |= WM8962_LRCLK_INV;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		aif0 |= WM8962_BCLK_INV | WM8962_LRCLK_INV;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aif0 |= WM8962_MSTR;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

    /**
     * Need to disable SYSCLK_ENA first before updating the 
     * WM8962_MSTR bit, then restore the original bit 
     */
	src = snd_soc_read(codec, WM8962_CLOCKING2) & WM8962_SYSCLK_ENA;
	snd_soc_update_bits(codec, WM8962_CLOCKING2, WM8962_SYSCLK_ENA, 0);

	snd_soc_update_bits(codec, WM8962_AUDIO_INTERFACE_0,
			    WM8962_FMT_MASK | WM8962_BCLK_INV | WM8962_MSTR |
			    WM8962_LRCLK_INV, aif0);

	snd_soc_update_bits(codec, WM8962_CLOCKING2, WM8962_SYSCLK_ENA,
			    src);

	return 0;
}

struct _fll_div {
	u16 fll_fratio;
	u16 fll_outdiv;
	u16 fll_refclk_div;
	u16 n;
	u16 theta;
	u16 lambda;
};

/* The size in bits of the FLL divide multiplied by 10
 * to allow rounding later */
#define FIXED_FLL_SIZE ((1 << 16) * 10)

static struct {
	unsigned int min;
	unsigned int max;
	u16 fll_fratio;
	int ratio;
} fll_fratios[] = {
	{       0,    64000, 4, 16 },
	{   64000,   128000, 3,  8 },
	{  128000,   256000, 2,  4 },
	{  256000,  1000000, 1,  2 },
	{ 1000000, 13500000, 0,  1 },
};

static int fll_factors(struct _fll_div *fll_div, unsigned int Fref,
		       unsigned int Fout)
{
	unsigned int target;
	unsigned int div;
	unsigned int fratio, gcd_fll;
	int i;

	/* Fref must be <=13.5MHz */
	div = 1;
	fll_div->fll_refclk_div = 0;
	while ((Fref / div) > 13500000) {
		div *= 2;
		fll_div->fll_refclk_div++;

		if (div > 4) {
			pr_err("Can't scale %dMHz input down to <=13.5MHz\n",
			       Fref);
			return -EINVAL;
		}
	}

	pr_debug("FLL Fref=%u Fout=%u\n", Fref, Fout);

	/* Apply the division for our remaining calculations */
	Fref /= div;

	/* Fvco should be 90-100MHz; don't check the upper bound */
	div = 2;
	while (Fout * div < 90000000) {
		div++;
		if (div > 64) {
			pr_err("Unable to find FLL_OUTDIV for Fout=%uHz\n",
			       Fout);
			return -EINVAL;
		}
	}
	target = Fout * div;
	fll_div->fll_outdiv = div - 1;

	pr_debug("FLL Fvco=%dHz\n", target);

	/* Find an appropraite FLL_FRATIO and factor it out of the target */
	for (i = 0; i < ARRAY_SIZE(fll_fratios); i++) {
		if (fll_fratios[i].min <= Fref && Fref <= fll_fratios[i].max) {
			fll_div->fll_fratio = fll_fratios[i].fll_fratio;
			fratio = fll_fratios[i].ratio;
			break;
		}
	}
	if (i == ARRAY_SIZE(fll_fratios)) {
		pr_err("Unable to find FLL_FRATIO for Fref=%uHz\n", Fref);
		return -EINVAL;
	}

	fll_div->n = target / (fratio * Fref);

	if (target % Fref == 0) {
		fll_div->theta = 0;
		fll_div->lambda = 0;
	} else {
		gcd_fll = gcd(target, fratio * Fref);

		fll_div->theta = (target - (fll_div->n * fratio * Fref))
			/ gcd_fll;
		fll_div->lambda = (fratio * Fref) / gcd_fll;
	}

	pr_debug("FLL N=%x THETA=%x LAMBDA=%x\n",
		 fll_div->n, fll_div->theta, fll_div->lambda);
	pr_debug("FLL_FRATIO=%x FLL_OUTDIV=%x FLL_REFCLK_DIV=%x\n",
		 fll_div->fll_fratio, fll_div->fll_outdiv,
		 fll_div->fll_refclk_div);

	return 0;
}

static void wm8962_poweroff_work(struct work_struct *dummy)
{
	struct wm8962_priv *wm8962 = wm8962_codec->private_data;

	if (wm962_paused) {
		return;
	}

	dev_err(wm8962_codec->dev, "FLL disabled\n");

	wm8962->fll_fref = 0;
	wm8962->fll_fout = 0;

	/* DAC Mute */
	wm8962_lpm_mute(wm8962_codec, 1);

	/* Turn off MICDET as there is no audio */
	snd_soc_update_bits(wm8962_codec, WM8962_ADDITIONAL_CONTROL_4,
			WM8962_MICDET_ENA, 0);

	/* VMID off */
	wm8962_set_bias_level(wm8962_codec, SND_SOC_BIAS_OFF);

	snd_soc_update_bits(wm8962_codec, WM8962_FLL_CONTROL_1,
			WM8962_FLL_ENA, 0);
	
	snd_soc_update_bits(wm8962_codec, WM8962_CLOCKING2, WM8962_SYSCLK_ENA, 0);

	/* Turn off the OSC and PLL */
	snd_soc_write(wm8962_codec, WM8962_PLL2, 0x0);

	fll_disabled = 1;
	return;
}
static DECLARE_DELAYED_WORK(wm8962_poweroff_wq, &wm8962_poweroff_work);

static int wm8962_set_fll(struct snd_soc_dai *dai, int fll_id, 
			  unsigned int Fref, unsigned int Fout)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wm8962_priv *wm8962 = codec->private_data;
	struct _fll_div fll_div;
	int ret;
	int fll1 = snd_soc_read(codec, WM8962_FLL_CONTROL_1) & WM8962_FLL_ENA;
	unsigned int reg;

	if (Fout == wm8962->fll_fout && Fref == wm8962->fll_fref)
		return 0;

	if (Fout == 0) {
		schedule_delayed_work(&wm8962_poweroff_wq, msecs_to_jiffies(power_off_timeout));
		return 0;
	}
	cancel_rearming_delayed_work(&wm8962_poweroff_wq);
	fll_disabled = 0;

	/* We are not using the PLL. Dont turn it on */
	//snd_soc_write(codec, WM8962_PLL2, WM8962_OSC_PLL_ENABLE);

	wm8962_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Configure DAC Volume bits */
	wm8962_write(codec, WM8962_RIGHT_DAC_VOLUME, WM8962_DAC_VU | WM8962_DAC_VOL_DEFAULT);
	wm8962_write(codec, WM8962_LEFT_DAC_VOLUME, WM8962_DAC_VU | WM8962_DAC_VOL_DEFAULT);

	ret = fll_factors(&fll_div, Fref, Fout);
	if (ret != 0)
		return ret;

	switch (fll_id) {
	case WM8962_FLL_MCLK:
	case WM8962_FLL_BCLK:
	case WM8962_FLL_OSC:
		fll1 |= (fll_id - 1) << WM8962_FLL_REFCLK_SRC_SHIFT;
		break;
	case WM8962_FLL_INT:
		snd_soc_update_bits(codec, WM8962_FLL_CONTROL_1,
				    WM8962_FLL_OSC_ENA, WM8962_FLL_OSC_ENA);
		snd_soc_update_bits(codec, WM8962_FLL_CONTROL_5,
				    WM8962_FLL_FRC_NCO, WM8962_FLL_FRC_NCO);
		break;
	default:
		dev_err(codec->dev, "Unknown FLL source %d\n", ret);
		return -EINVAL;
	}

	if (fll_div.theta || fll_div.lambda)
		fll1 |= WM8962_FLL_FRAC;

	fll1 |= WM8962_FLL_ENA | WM8962_FLL_FRAC;

	/* Stop the FLL while we reconfigure */
	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_1, WM8962_FLL_ENA, 0);

	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_2,
			    WM8962_FLL_OUTDIV_MASK |
			    WM8962_FLL_REFCLK_DIV_MASK,
			    (fll_div.fll_outdiv << WM8962_FLL_OUTDIV_SHIFT) |
			    (fll_div.fll_refclk_div));

	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_3,
			    WM8962_FLL_FRATIO_MASK, fll_div.fll_fratio);

	snd_soc_write(codec, WM8962_FLL_CONTROL_6, fll_div.theta);
	snd_soc_write(codec, WM8962_FLL_CONTROL_7, fll_div.lambda);
	snd_soc_write(codec, WM8962_FLL_CONTROL_8, fll_div.n);

	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_1,
			    WM8962_FLL_FRAC | WM8962_FLL_REFCLK_SRC_MASK |
			    WM8962_FLL_ENA, fll1);
#ifdef LAB126
	
	dev_dbg(codec->dev, "FLL configured for %dHz->%dHz\n", Fref, Fout);
#else

	dev_err(codec->dev, "FLL configured for %dHz->%dHz\n", Fref, Fout);

#endif
	wm8962->fll_fref = Fref;
	wm8962->fll_fout = Fout;
 
	snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_4,
			WM8962_MICDET_ENA, WM8962_MICDET_ENA);
	reg = snd_soc_read(codec, WM8962_ADDITIONAL_CONTROL_4);

	/* DAC still muted, check for headset */
	if (reg & (WM8962_MICSHORT_STS | WM8962_MICDET_STS)) {
		headset_detected = 1;
		dev_dbg(codec->dev, "Headset detected\n");
		/* Save the old value */
		old_speaker_vol = snd_soc_read(codec, WM8962_SPKOUTL_VOLUME);
		
		if (old_speaker_vol == 0) {
			/* Looks like speaker never initialized */
			old_speaker_vol = WM8962_DEFAULT_SPKR_VALUE;
		}
    
		if (!wm8962_codec_diags_mode) {
			/* Mute the speakers now */
			dev_dbg(codec->dev, "Muting speakers\n");
			snd_soc_write(codec, WM8962_SPKOUTL_VOLUME, 0x100);
			snd_soc_write(codec, WM8962_SPKOUTR_VOLUME, 0x100);
		}
		else {
			/* Mute the headset in diags mode ... sigh */
			snd_soc_write(codec, WM8962_HPOUTL_VOLUME, 0x100);
			snd_soc_write(codec, WM8962_HPOUTR_VOLUME, 0x100);
		}
		if (reg & WM8962_MICDET_STS) {
			/* Also enable support for external MIC */
			reg = snd_soc_read(codec, WM8962_PWR_MGMT_1);
			reg &= ~(0x3C);
			reg |= 0x14;
			snd_soc_write(codec, WM8962_PWR_MGMT_1, reg);
		}

		/* Turn on right side channel for external MIC */
		snd_soc_write(codec, WM8962_RIGHT_INPUT_PGA_CONTROL, 0x18);

		/* Send headset detected event */
		kobject_uevent(&mxc_headset.this_device->kobj, KOBJ_ADD);
	}
	else {
		dev_dbg(codec->dev, "Unmuting headset in set FLL\n");
		headset_detected = 0;
		/* No headset, refer tp default internal MIC */
		snd_soc_write(codec, WM8962_RIGHT_INPUT_PGA_CONTROL, 0);

		/* Enable internal MIC */
		reg = snd_soc_read(codec, WM8962_PWR_MGMT_1);
		reg &= ~(0x3C);
		reg |= 0x28;
		snd_soc_write(codec, WM8962_PWR_MGMT_1, reg);

		/* Turn on left side channel for internal MIC */
		snd_soc_write(codec, WM8962_LEFT_INPUT_PGA_CONTROL, 0x18);
	
		/* Make sure we mute the Headphone */
		wm8962_write_through(codec, WM8962_HPOUTL_VOLUME, 0x100);
		wm8962_write_through(codec, WM8962_HPOUTR_VOLUME, 0x100);

		kobject_uevent(&mxc_headset.this_device->kobj, KOBJ_REMOVE);
	}
    
	 /**
     * Need to program the head phone and speaker volumes when in 
     * codec_diags mode before unmute the audio. 
     */
	wm8962_codec_diags_set_custom_regs("wm8962_set_fll", codec);

  /* Unmute */
	wm8962_lpm_mute(codec, 0);

	return 0;
}

static int wm8962_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	int val;

	if (mute)
		val = WM8962_DAC_MUTE;
	else
		val = 0;

	return snd_soc_update_bits(codec, WM8962_ADC_DAC_CONTROL_1,
				   WM8962_DAC_MUTE, val);
}

/* Add support for PAUSE/STOP triggers */
static int wm8962_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	pr_debug("wm8962_trigger\n");
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		wm962_paused = 1;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		wm962_paused = 0;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		wm962_paused = 0;
		break;
	}
	return 0;
}

#define WM8962_RATES SNDRV_PCM_RATE_8000_96000

#define WM8962_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops wm8962_dai_ops = {
	.hw_params = wm8962_hw_params,
	.set_sysclk = wm8962_set_dai_sysclk,
	.set_fmt = wm8962_set_dai_fmt,
	.set_pll = wm8962_set_fll,
	.digital_mute = wm8962_mute,
	.trigger = wm8962_trigger,
};

struct snd_soc_dai wm8962_dai = {
	.name = "WM8962",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8962_RATES,
		.formats = WM8962_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8962_RATES,
		.formats = WM8962_FORMATS,
	},
	.ops = &wm8962_dai_ops,
	.symmetric_rates = 1,
};
EXPORT_SYMBOL_GPL(wm8962_dai);

static int wm8962_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;
	int error = 0;

	if (wm8962_codec == NULL) {
		dev_err(&pdev->dev, "Codec device not registered\n");
		return -ENODEV;
	}

	socdev->card->codec = wm8962_codec;
	codec = wm8962_codec;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(codec->dev, "failed to create pcms: %d\n", ret);
		goto pcm_err;
	}

	wm8962_add_widgets(codec);

	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		dev_err(codec->dev, "failed to register card: %d\n", ret);
		goto card_err;
	}

        error = sysdev_class_register(&wm8962_reg_sysclass);
        if (!error)
                error = sysdev_register(&device_wm8962_reg);
        if (!error)
                error = sysdev_create_file(&device_wm8962_reg, &attr_wm8962_reg);

        error = sysdev_class_register(&wm8962_register_sysclass);
        if (!error)
                error = sysdev_register(&device_wm8962_register);
        if (!error)
                error = sysdev_create_file(&device_wm8962_register, &attr_wm8962_register);

	error = sysdev_class_register(&wm8962_headset_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8962_headset);
	if (!error)
		error = sysdev_create_file(&device_wm8962_headset, &attr_wm8962_headset);

	error = sysdev_class_register(&wm8962_power_off_timeout_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8962_power_off_timeout);
	if (!error)
		error = sysdev_create_file(&device_wm8962_power_off_timeout,
				&attr_wm8962_power_off_timeout);

	error = sysdev_class_register(&wm8962_codec_mute_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8962_codec_mute);
	if (!error)
		error = sysdev_create_file(&device_wm8962_codec_mute, &attr_wm8962_codec_mute);

	error = sysdev_class_register(&wm8962_codec_diags_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8962_codec_diags);
	if (!error)
		error = sysdev_create_file(&device_wm8962_codec_diags, &attr_wm8962_codec_diags);

	error = sysdev_class_register(&wm8962_fll_disabled_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8962_fll_disabled);
	if (!error)
		error = sysdev_create_file(&device_wm8962_fll_disabled, &attr_wm8962_fll_disabled);

	error = sysdev_class_register(&wm8962_codec_diags_volume_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8962_codec_diags_volume);
	if (!error)
		error = sysdev_create_file(&device_wm8962_codec_diags_volume, &attr_wm8962_codec_diags_volume);

	if (misc_register(&mxc_headset)) {
		printk (KERN_WARNING "mxc_headset: Couldn't register device %d\n", HEADSET_MINOR);
		return -EBUSY;
	}

	return ret;

card_err:
        snd_soc_free_pcms(socdev);
        snd_soc_dapm_free(socdev);

pcm_err:
	return ret;
}

static int wm8962_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	misc_deregister(&mxc_headset);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	sysdev_remove_file(&device_wm8962_reg, &attr_wm8962_reg);
	sysdev_remove_file(&device_wm8962_register, &attr_wm8962_register);
	sysdev_remove_file(&device_wm8962_headset, &attr_wm8962_headset);
	sysdev_remove_file(&device_wm8962_power_off_timeout, &attr_wm8962_power_off_timeout);
	sysdev_remove_file(&device_wm8962_codec_mute, &attr_wm8962_codec_mute);
	sysdev_remove_file(&device_wm8962_codec_diags, &attr_wm8962_codec_diags);
	sysdev_remove_file(&device_wm8962_fll_disabled, &attr_wm8962_fll_disabled);
	sysdev_remove_file(&device_wm8962_codec_diags_volume, &attr_wm8962_codec_diags_volume);

	return 0;
}

#ifdef CONFIG_PM
static int wm8962_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	struct wm8962_priv *wm8962 = codec->private_data;
	u16 *reg_cache = codec->reg_cache;
	int i;

	/* Restore the registers */
	for (i = 1; i < ARRAY_SIZE(wm8962->reg_cache); i++) {
		switch (i) {
		case WM8962_SOFTWARE_RESET:
			continue;
		default:
			break;
		}

		if (reg_cache[i] != wm8962_reg[i])
			snd_soc_write(codec, i, reg_cache[i]);
	}

	return 0;
}

#else
#define wm8962_resume NULL
#endif

struct snd_soc_codec_device soc_codec_dev_wm8962 = {
	.probe = 	wm8962_probe,
	.remove = 	wm8962_remove,
	.resume =	wm8962_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_wm8962);

extern int gpio_headset_value(void);

static void wm8962_irq_work(struct work_struct *work)
{
	struct wm8962_priv *wm8962 = container_of(work, struct wm8962_priv,
					          irq_work.work);
	struct snd_soc_codec *codec = &wm8962->codec;
	struct i2c_client *i2c = container_of(codec->dev, struct i2c_client,
					      dev);
	int active, mask, reg, status, irq_pol;

	mask = snd_soc_read(codec, WM8962_INTERRUPT_STATUS_2_MASK);
	if (mask == 0xFFFF)
	{
		/**
		 * Something wrong, just restore the normal value, 0x1ffe is the 
		 * value before running audio tests. 
		 */
		snd_soc_write(codec, WM8962_INTERRUPT_STATUS_2, 0x1ffe);
	}
	active = snd_soc_read(codec, WM8962_INTERRUPT_STATUS_2);
	/**
	 * Make sure to clear all interrupt bits.  After MIC test, there 
	 * might be some interrupt bits even they are masked. 
	 */
	snd_soc_write(codec, WM8962_INTERRUPT_STATUS_2, active);
	printk(KERN_ERR "WM8962_INTERRUPT_STATUS_2: mask: 0x%4X, active 0x%4X\n", mask, active);
  
	if (active & WM8962_TEMP_SHUT_EINT)
		dev_err(codec->dev, "Thermal shutdown\n");
 
	if (active & WM8962_FIFOS_ERR_EINT)
		dev_err(codec->dev, "FIFO error\n");

	if (active & WM8962_FLL_LOCK_EINT)
		dev_err(codec->dev, "FLL lock\n");

	/**
	 * Mask after check the above bit to debug the interrupt storm. 
	 * The above bits are not masked in normal mode.  When the 
	 * interrupt storm happened, the mask is 0xFFFF.  Mic test needs 
	 * to restore that value. 
	 */
	active &= ~mask;
	if (active == 0)
	{
		/* Special Ack for invalid interrupt */
		/* Without this, the IRQ is unacknowledged causing an IRQ storm. */
		/**
		 * Interrupt storm happens when the MIC detect irq polarity is 
		 * not set properly. 
		 */
		dev_err(codec->dev, "Special Ack for invalid interrupt\n");
		snd_soc_write(codec, WM8962_INTERRUPT_STATUS_2,
				WM8962_MICD_EINT);
	}
	
	reg = snd_soc_read(codec, WM8962_ADDITIONAL_CONTROL_4);
	
	if (reg & (WM8962_MICSHORT_STS | WM8962_MICDET_STS)) {
		headset_detected = 1;
		dev_dbg(codec->dev, "IRQ Headset detected\n");	
		/* Save the old value */
		old_speaker_vol = snd_soc_read(codec, WM8962_SPKOUTL_VOLUME);
		if (old_speaker_vol == 0) {
			/* Looks like speaker never initialized */
			old_speaker_vol = WM8962_DEFAULT_SPKR_VALUE;
		}

		if (!wm8962_codec_diags_mode) {
			/* Mute the speakers now */
			snd_soc_write(codec, WM8962_SPKOUTL_VOLUME, 0x100);
			snd_soc_write(codec, WM8962_SPKOUTR_VOLUME, 0x100);
		}
#if 0
		else {
			/* Mute the headset in diags mode ... sigh */
			printk(KERN_ERR "Mute the headset in diags mode \n");
			snd_soc_write(codec, WM8962_HPOUTL_VOLUME, 0x100);
			snd_soc_write(codec, WM8962_HPOUTR_VOLUME, 0x100);
		}
#endif //0
		if (reg & WM8962_MICDET_STS) {
			/* Also enable support for external MIC */
			reg = snd_soc_read(codec, WM8962_PWR_MGMT_1);
			reg &= ~(0x3C);
			reg |= 0x14;
			snd_soc_write(codec, WM8962_PWR_MGMT_1, reg);
		}

		/* Turn on right side channel for external MIC */
		snd_soc_write(codec, WM8962_RIGHT_INPUT_PGA_CONTROL, 0x18);

		kobject_uevent(&mxc_headset.this_device->kobj, KOBJ_ADD);
	}
	else {
		if (headset_detected) {
			dev_dbg(codec->dev, "IRQ Headset removed\n");
			headset_detected = 0;
			if (!wm8962_codec_diags_mode)
			{
				/* Set the VU bit */
				old_speaker_vol |= 0x100;
				snd_soc_write(codec, WM8962_SPKOUTL_VOLUME, old_speaker_vol);
				snd_soc_write(codec, WM8962_SPKOUTR_VOLUME, old_speaker_vol);
			
				/* Clear out old value */
				old_speaker_vol = 0;
			
				/* Make sure we mute the Headphone */
				snd_soc_write(codec, WM8962_HPOUTL_VOLUME, 0x100);
				snd_soc_write(codec, WM8962_HPOUTR_VOLUME, 0x100);
      
				/* No headset, refer tp default internal MIC */
				snd_soc_write(codec, WM8962_RIGHT_INPUT_PGA_CONTROL, 0);

				/* Enable internal MIC */
				reg = snd_soc_read(codec, WM8962_PWR_MGMT_1);
				reg &= ~(0x3C);
				reg |= 0x28;
				snd_soc_write(codec, WM8962_PWR_MGMT_1, reg);

				/* Turn on left side channel for internal MIC */
				snd_soc_write(codec, WM8962_LEFT_INPUT_PGA_CONTROL, 0x18);
			}

			kobject_uevent(&mxc_headset.this_device->kobj, KOBJ_REMOVE);
		}
	}

	if (active & (WM8962_MICD_EINT | WM8962_MICSCD_EINT)) {
		reg = snd_soc_read(codec, WM8962_ADDITIONAL_CONTROL_4);
		irq_pol = 0;
		status = 0;

		if (reg & WM8962_MICDET_STS) {
			irq_pol |= WM8962_MICD_IRQ_POL;
			status |= SND_JACK_MICROPHONE;
		}

		if (reg & WM8962_MICSHORT_STS) {
			irq_pol |= WM8962_MICSCD_IRQ_POL;
			status |= SND_JACK_BTN_0;
		}
		
		// This causes a crash in this context and is not necessary
		/*snd_soc_jack_report(wm8962->jack, status,
				    SND_JACK_MICROPHONE | SND_JACK_BTN_0);*/
		if (!wm8962_codec_diags_mode)
		{
			snd_soc_update_bits(codec, WM8962_MICINT_SOURCE_POL,
				    WM8962_MICSCD_IRQ_POL |
				    WM8962_MICD_IRQ_POL, irq_pol);
		}
	}
 
	snd_soc_write(codec, WM8962_INTERRUPT_STATUS_2, active);

	switch (wm8962_codec_diags_mode) {
	case WM8962_CODEC_DIAGS_MODE_NORMAL:
		break;
	case WM8962_CODEC_DIAGS_MODE_MIC_TEST:
		printk(KERN_ERR "%s: Do nothing in MIC test\n", __func__);
		return;	
	}

	wm8962_irq_enabled = 1;
	enable_irq(i2c->irq);
}

static irqreturn_t wm8962_irq(int irq, void *data)
{
	struct wm8962_priv *wm8962 = wm8962_codec->private_data;

    
	wm8962_irq_value = irq;

	switch (wm8962_codec_diags_mode) {
	case WM8962_CODEC_DIAGS_MODE_NORMAL:
		break;
	case WM8962_CODEC_DIAGS_MODE_MIC_TEST:
	default:
		printk(KERN_ERR "%s: Do nothing in MIC test\n", __func__);
		wm8962_irq_enabled = 0;
		disable_irq_nosync(irq);
		return 0;
	}
    
	schedule_delayed_work(&wm8962->irq_work, msecs_to_jiffies(200));
	disable_irq_nosync(irq);
	return IRQ_HANDLED;
}

int wm8962_mic_detect(struct snd_soc_codec *codec, struct snd_soc_jack *jack)
{
	struct wm8962_priv *wm8962 = codec->private_data;
	int irq_mask, enable;

	wm8962->jack = jack;

	if (jack) {
		irq_mask = 0;
		enable = WM8962_MICDET_ENA;
	} else {
		irq_mask = WM8962_MICD_EINT | WM8962_MICSCD_EINT;
		enable = 0;
	}

	snd_soc_update_bits(codec, WM8962_INTERRUPT_STATUS_2_MASK,
			    WM8962_MICD_EINT | WM8962_MICSCD_EINT, irq_mask);
	snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_4,
			    WM8962_MICDET_ENA, enable);

	snd_soc_jack_report(wm8962->jack, 0,
			    SND_JACK_MICROPHONE | SND_JACK_BTN_0);

	return 0;
}
EXPORT_SYMBOL_GPL(wm8962_mic_detect);

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
static int beep_rates[] = {
	500, 1000, 2000, 4000,
};

static void wm8962_beep_work(struct work_struct *work)
{
	struct wm8962_priv *wm8962 =
		container_of(work, struct wm8962_priv, beep_work);
	struct snd_soc_codec *codec = &wm8962->codec;
	int i;
	int reg = 0;
	int best = 0;

	if (wm8962->beep_rate) {
		for (i = 0; i < ARRAY_SIZE(beep_rates); i++) {
			if (abs(wm8962->beep_rate - beep_rates[i]) <
			    abs(wm8962->beep_rate - beep_rates[best]))
				best = i;
		}

		dev_err(codec->dev, "Set beep rate %dHz for requested %dHz\n",
			beep_rates[best], wm8962->beep_rate);

		reg = WM8962_BEEP_ENA | (best << WM8962_BEEP_RATE_SHIFT);

		snd_soc_dapm_enable_pin(codec, "Beep");
	} else {
		dev_err(codec->dev, "Disabling beep\n");
		snd_soc_dapm_disable_pin(codec, "Beep");
	}

	snd_soc_update_bits(codec, WM8962_BEEP_GENERATOR_1,
			    WM8962_BEEP_ENA | WM8962_BEEP_RATE_MASK, reg);

	snd_soc_dapm_sync(codec);
}

/* For usability define a way of injecting beep events for the device -
 * many systems will not have a keyboard.
 */
static int wm8962_beep_event(struct input_dev *dev, unsigned int type,
			     unsigned int code, int hz)
{
	struct snd_soc_codec *codec = input_get_drvdata(dev);
	struct wm8962_priv *wm8962 = codec->private_data;

	dev_err(codec->dev, "Beep event %x %x\n", code, hz);

	switch (code) {
	case SND_BELL:
		if (hz)
			hz = 1000;
	case SND_TONE:
		break;
	default:
		return -1;
	}

	/* Kick the beep from a workqueue */
	wm8962->beep_rate = hz;
	schedule_work(&wm8962->beep_work);
	return 0;
}

static ssize_t wm8962_beep_set(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct wm8962_priv *wm8962 = dev_get_drvdata(dev);
	long int time;

	strict_strtol(buf, 10, &time);

	input_event(wm8962->beep, EV_SND, SND_TONE, time);

	return count;
}

static DEVICE_ATTR(beep, 0200, NULL, wm8962_beep_set);

static void wm8962_init_beep(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = codec->private_data;
	int ret;

	wm8962->beep = input_allocate_device();
	if (!wm8962->beep) {
		dev_err(codec->dev, "Failed to allocate beep device\n");
		return;
	}

	INIT_WORK(&wm8962->beep_work, wm8962_beep_work);
	wm8962->beep_rate = 0;

	wm8962->beep->name = "WM8962 Beep Generator";
	wm8962->beep->phys = dev_name(codec->dev);
	wm8962->beep->id.bustype = BUS_I2C;

	wm8962->beep->evbit[0] = BIT_MASK(EV_SND);
	wm8962->beep->sndbit[0] = BIT_MASK(SND_BELL) | BIT_MASK(SND_TONE);
	wm8962->beep->event = wm8962_beep_event;
	wm8962->beep->dev.parent = codec->dev;
	input_set_drvdata(wm8962->beep, codec);

	ret = input_register_device(wm8962->beep);
	if (ret != 0) {
		input_free_device(wm8962->beep);
		wm8962->beep = NULL;
		dev_err(codec->dev, "Failed to register beep device\n");
	}

	ret = device_create_file(codec->dev, &dev_attr_beep);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to create keyclick file: %d\n",
			ret);
	}
}

static void wm8962_free_beep(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = codec->private_data;

	device_remove_file(codec->dev, &dev_attr_beep);
	input_unregister_device(wm8962->beep);
	cancel_work_sync(&wm8962->beep_work);
	wm8962->beep = NULL;

	snd_soc_update_bits(codec, WM8962_BEEP_GENERATOR_1, WM8962_BEEP_ENA,0);
}
#else
static void wm8962_init_beep(struct snd_soc_codec *codec)
{
}

static void wm8962_free_beep(struct snd_soc_codec *codec)
{
}
#endif

static void wm8962_process_req(struct work_struct *work)
{
	printk(KERN_INFO "wm8962_process_req\n");

	/* DAC Mute */
	wm8962_lpm_mute(wm8962_codec, 1);

	wm8962_set_bias_level(wm8962_codec, SND_SOC_BIAS_OFF);

	/* Turn off MICDET as there is no audio */
	snd_soc_update_bits(wm8962_codec, WM8962_ADDITIONAL_CONTROL_4,
				WM8962_MICDET_ENA, 0);

	/* Turn off the OSC and PLL */
	snd_soc_write(wm8962_codec, WM8962_PLL2, 0x0);
}
DECLARE_DELAYED_WORK(pm_work, wm8962_process_req);

static int wm8962_register(struct wm8962_priv *wm8962)
{
	struct snd_soc_codec *codec = &wm8962->codec;
	struct i2c_client *i2c = container_of(codec->dev, struct i2c_client,
					      dev);
	int ret;

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->private_data = wm8962;
	codec->name = "WM8962";
	codec->owner = THIS_MODULE;
	codec->bias_level = SND_SOC_BIAS_OFF;
	codec->set_bias_level = wm8962_set_bias_level;
	codec->dai = &wm8962_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = WM8962_MAX_REGISTER;
	codec->reg_cache = &wm8962->reg_cache;
	codec->read = wm8962_read;
	codec->write = wm8962_write;

	memcpy(codec->reg_cache, wm8962_reg, sizeof(wm8962_reg));

	ret = snd_soc_read(codec, WM8962_SOFTWARE_RESET);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read ID register\n");
		goto err_enable;
	}
	if (ret != wm8962_reg[WM8962_SOFTWARE_RESET]) {
		dev_err(codec->dev, "Device is not a WM8962, ID %x != %x\n",
			ret, wm8962_reg[WM8962_SOFTWARE_RESET]);
	}

	ret = snd_soc_read(codec, WM8962_RIGHT_INPUT_VOLUME);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read device revision: %d\n",
			ret);
		goto err_enable;
	}
	
	dev_info(codec->dev, "customer id %x revision %c\n",
		 (ret & WM8962_CUST_ID_MASK) >> WM8962_CUST_ID_SHIFT,
		 ((ret & WM8962_CHIP_REV_MASK) >> WM8962_CHIP_REV_SHIFT)
		 + 'A');

	ret = wm8962_reset(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to issue reset\n");
		goto err_enable;
	}
	/* Latch volume update bits */
	wm8962->reg_cache[WM8962_LEFT_INPUT_VOLUME] |= WM8962_IN_VU;
	wm8962->reg_cache[WM8962_RIGHT_INPUT_VOLUME] |= WM8962_IN_VU;
	wm8962->reg_cache[WM8962_LEFT_ADC_VOLUME] |= WM8962_ADC_VU;
	wm8962->reg_cache[WM8962_RIGHT_ADC_VOLUME] |= WM8962_ADC_VU;	
	wm8962->reg_cache[WM8962_LEFT_DAC_VOLUME] |= WM8962_DAC_VU;
	wm8962->reg_cache[WM8962_RIGHT_DAC_VOLUME] |= WM8962_DAC_VU;
	wm8962->reg_cache[WM8962_SPKOUTL_VOLUME] |= WM8962_SPKOUT_VU;
	wm8962->reg_cache[WM8962_SPKOUTR_VOLUME] |= WM8962_SPKOUT_VU;
	wm8962->reg_cache[WM8962_HPOUTL_VOLUME] |= WM8962_HPOUT_VU;
	wm8962->reg_cache[WM8962_HPOUTR_VOLUME] |= WM8962_HPOUT_VU;

       	wm8962_dai.dev = codec->dev;

	wm8962_codec = codec;

	/* Get full register access/control: CLKREG_OVD=1, SYSCLK_ENA=0 */		
	snd_soc_update_bits(codec, WM8962_CLOCKING2, WM8962_CLKREG_OVD, WM8962_CLKREG_OVD);
	snd_soc_update_bits(codec, WM8962_CLOCKING2, WM8962_SYSCLK_ENA, 0);	
	snd_soc_update_bits(codec, WM8962_PLL2, WM8962_OSC_ENA | WM8962_PLL2_ENA | 
										WM8962_PLL3_ENA, 0);
	
#ifdef CONFIG_SND_SOC_IMX_YOSHI_WM8962
	/* Fix: Pull Down GPIO5 and GPIO6 */
	snd_soc_write(codec, WM8962_GPIO_5, 0xA100);
	snd_soc_write(codec, WM8962_GPIO_6, 0xA100);	
#endif

	wm8962_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
 
	/* Put GPIO2 & 3 into GPIO mode */
	snd_soc_update_bits(codec, WM8962_ANALOGUE_CLOCKING1,
			    WM8962_CLKOUT3_SEL_MASK | WM8962_CLKOUT2_SEL_MASK,
			    0x28);

	/* Put GPIO2 into IRQ mode */
	snd_soc_update_bits(codec, WM8962_GPIO_2, WM8962_GP2_FN_MASK, 0x3);
  
	INIT_DELAYED_WORK(&wm8962->irq_work, wm8962_irq_work);
	set_irq_type(i2c->irq, IRQF_TRIGGER_HIGH);
	ret = request_irq(i2c->irq, wm8962_irq, IRQF_TRIGGER_HIGH, "wm8962",
			  &wm8962->irq_work);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to requets IRQ %d: %d\n",
			i2c->irq, ret);
	}

	snd_soc_update_bits(codec, WM8962_INTERRUPT_STATUS_2_MASK,
			    WM8962_TEMP_SHUT_EINT | WM8962_FIFOS_ERR_EINT, 0);

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		return ret;
	}

	ret = snd_soc_register_dai(&wm8962_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		snd_soc_unregister_codec(codec);
		return ret;
	}

	wm8962_init_beep(codec);
  
	/* Configure DAC Volume bits */
	wm8962_write(codec, WM8962_RIGHT_DAC_VOLUME, WM8962_DAC_VU | WM8962_DAC_VOL_DEFAULT);
	wm8962_write(codec, WM8962_LEFT_DAC_VOLUME, WM8962_DAC_VU | WM8962_DAC_VOL_DEFAULT);

//    wm8962_codec_diags_set_custom_regs("wm8962_register", codec);
	/* Boot power management */
	schedule_delayed_work(&pm_work, msecs_to_jiffies(5000));

	return 0;

err_enable:
	kfree(wm8962);
	return ret;
}

static void wm8962_unregister(struct wm8962_priv *wm8962)
{
	int i;

	wm8962_free_beep(&wm8962->codec);
	wm8962_set_bias_level(&wm8962->codec, SND_SOC_BIAS_OFF);
	for (i = 0; i < ARRAY_SIZE(wm8962->supplies); i++)
		regulator_unregister_notifier(wm8962->supplies[i].consumer,
					      &wm8962->disable_nb[i]);
	regulator_bulk_free(ARRAY_SIZE(wm8962->supplies), wm8962->supplies);
	snd_soc_unregister_dai(&wm8962_dai);
	snd_soc_unregister_codec(&wm8962->codec);
	kfree(wm8962);
	wm8962_codec = NULL;
}

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static __devinit int wm8962_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm8962_priv *wm8962;
	struct snd_soc_codec *codec;

	printk(KERN_INFO "wm8962_i2c_probe\n");

	wm8962 = kzalloc(sizeof(struct wm8962_priv), GFP_KERNEL);
	if (wm8962 == NULL)
		return -ENOMEM;

	codec = &wm8962->codec;
	codec->hw_write = (hw_write_t)i2c_master_send;

	i2c_set_clientdata(i2c, wm8962);
	codec->control_data = i2c;

	codec->dev = &i2c->dev;

	return wm8962_register(wm8962);
}

static __devexit int wm8962_i2c_remove(struct i2c_client *client)
{
	struct wm8962_priv *wm8962 = i2c_get_clientdata(client);
	wm8962_unregister(wm8962);
	return 0;
}

static const struct i2c_device_id wm8962_i2c_id[] = {
	{ "wm8962", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8962_i2c_id);

static struct i2c_driver wm8962_i2c_driver = {
	.driver = {
		.name = "wm8962",
		.owner = THIS_MODULE,
	},
	.probe =    wm8962_i2c_probe,
	.remove =   __devexit_p(wm8962_i2c_remove),
	.id_table = wm8962_i2c_id,
};
#endif

static int __init wm8962_modinit(void)
{
	int ret;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&wm8962_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register WM8962 I2C driver: %d\n",
		       ret);
	}
#endif
	return 0;
}
module_init(wm8962_modinit);

static void __exit wm8962_exit(void)
{
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&wm8962_i2c_driver);
#endif
}
module_exit(wm8962_exit);

MODULE_DESCRIPTION("ASoC WM8962 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com> and Manish Lachwani <lachwani@amazon.com>");
MODULE_LICENSE("GPL");

/*
 * Copyright (C) 2010-2011 Amazon Technologies Inc.
 * Manish Lachwani (lachwani@lab126.com)
 * Vidhyananth Venkatasamy (venkatas@lab126.com)
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
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/sysdev.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/miscdevice.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#include "wm8960.h"

#define AUDIO_NAME "wm8960"
#define WM8960_VERSION "0.1"

#define WM8960_OFF_THRESHOLD	0	/* Threshold for powering off the codec */

static int wm8960_codec_diags_mode = 0; /* Codec diags mode */

#define CODEC_DIAGS_MODE_NORMAL                  0
#define CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST  1
#define CODEC_DIAGS_MODE_LEFT_HP_TEST            2
#define CODEC_DIAGS_MODE_RIGHT_HP_TEST           3
#define CODEC_DIAGS_MODE_BOTH_HP_TEST            4
#define CODEC_DIAGS_MODE_LEFT_SP_TEST            5
#define CODEC_DIAGS_MODE_RIGHT_SP_TEST           6
#define CODEC_DIAGS_MODE_BOTH_SP_TEST            7
#define CODEC_DIAGS_MODE_MIC_TEST                8
#define CODEC_DIAGS_MODE_MAX                     9

struct snd_soc_codec_device soc_codec_dev_wm8960;

static int wm8960_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level);

static int wm8960_reboot(struct notifier_block *self, unsigned long action, void *cpu);

static struct notifier_block wm8960_reboot_nb =
{
	.notifier_call	= wm8960_reboot,
};

static atomic_t power_status;
static int power_off_timeout = WM8960_OFF_THRESHOLD; /* in msecs */

/* Send event to userspace for headset */

#define HEADSET_MINOR	165	/* Headset event passing */

static struct miscdevice mxc_headset = {
	HEADSET_MINOR,
	"mxc_ALSA",
	NULL,
};

static int power_status_read(void) 
{
    return atomic_read(&power_status);
}

static void power_status_write(int value)
{
    atomic_set(&power_status, value);
}

#define SPKR_ENABLE			0xc0	/* Bits 6 and 7 enable the speakers */
#define HPSWPOLARITY		0x20	/* Turn on the headset polarity bit to detect headset */
#define LOUT2				0x1E7	/* Enable SPKR LEFT Volume and set the levels */
#define ROUT2				0x1E7	/* Enable SPKR RIGHT Volume and set the levels */
#define SPKR_POWER			0x18	/* Enable Power for right and left speakers */
#define LOUT1				0x1CC	/* Enable HEADPHONE LEFT Volume */
#define ROUT1				0x1CC	/* Enable HEADPHONE RIGHT Volume */
#define LDAC				0x1FF	/* Left DAC volume */
#define RDAC				0x1FF	/* Right DAC volume */
#define POWER1				0xc0	/* Power register 1 */
#define POWER2				0x1fb	/* Power register 2 */
#define POWER3				0x0c	/* Power register 3 */
#define SPKR_CONFIG			0xF7	/* Speaker Config */
#define CLASSD3_CONFIG		0x80	/* CLASSD Control 3 */

#define HSDET_THRESHOLD		500		/* 500ms debounce */

/* R25 - Power 1 */
#define WM8960_VREF      	0x40

/* R28 - Anti-pop 1 */
#define WM8960_POBCTRL   	0x80
#define WM8960_BUFDCOPEN 	0x10
#define WM8960_BUFIOEN   	0x08
#define WM8960_SOFT_ST   	0x04
#define WM8960_HPSTBY    	0x01

/* R29 - Anti-pop 2 */
#define WM8960_DISOP     	0x40

#define WM8960_IDLE         0
#define WM8960_ACTIVE       1
unsigned int codec_state = WM8960_IDLE;
DEFINE_MUTEX(wm8960_state_lock);

/*
 * wm8960 register cache
 * We can't read the WM8960 register space when we are
 * using 2 wire for device control, so we cache them instead.
 */
static const u16 wm8960_reg[WM8960_CACHEREGNUM] = {
	0x0097, 0x0097, 0x0000, 0x0000,
	0x0000, 0x0008, 0x0000, 0x000a,
	0x01c0, 0x0000, 0x00ff, 0x00ff,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x007b, 0x0100, 0x0032,
	0x0000, 0x00c3, 0x00c3, 0x01c0,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0100, 0x0100, 0x0050, 0x0050,
	0x0050, 0x0050, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0040, 0x0000,
	0x0000, 0x0050, 0x0050, 0x0000,
	0x0002, 0x0037, 0x004d, 0x0080,
	0x0008, 0x0031, 0x0026, 0x00e9,
};

struct wm8960_priv {
	u16 reg_cache[WM8960_CACHEREGNUM];
	struct snd_soc_codec codec;
};

extern int gpio_headset_status(unsigned int);
extern int gpio_headset_irq(unsigned int);

/*
 * WM8960 Codec /sys
 */

/* Default is Reg #0. Value of > 55 means dump all the codec registers */
static int wm8960_reg_number = 0;

struct snd_soc_codec *wm8960_codec;
static inline unsigned int wm8960_read_reg_cache(struct snd_soc_codec *codec,
				unsigned int reg);
static int wm8960_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int value);

static void wm8960_configure_capture(struct snd_soc_codec *codec);
static void wm8960_headset_mute(int mute);

/*
 * Debug - dump all the codec registers
 */
static void dump_regs(struct snd_soc_codec *codec)
{
	int i = 0;
	u16 *cache = codec->reg_cache;

	for (i=0; i < WM8960_CACHEREGNUM; i++) {
		printk("Register %d - Value:0x%x\n", i, cache[i]);
	}
}

DEFINE_MUTEX(wm8960_codec_lock);

/*
 * sysfs entries for the codec
 */
static ssize_t wm8960_regoff_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0) {
		printk(KERN_ERR "Could not store the codec register value\n");
		return -EINVAL;
	}

	wm8960_reg_number = value;

	return size;
}

static ssize_t wm8960_regoff_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	char *curr = buf;

	curr += sprintf(curr, "WM8960 Register Number: %d\n", wm8960_reg_number);
	curr += sprintf(curr, "\n");

	return curr - buf;
}

static SYSDEV_ATTR(wm8960_regoff, 0644, wm8960_regoff_show, wm8960_regoff_store);

static ssize_t wm8960_regval_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	char *curr = buf;

	if (wm8960_reg_number >= WM8960_CACHEREGNUM) {
		/* dump all the regs */
		curr += sprintf(curr, "WM8960 Registers\n");
		curr += sprintf(curr, "\n");
		dump_regs(wm8960_codec);
	}
	else {
		curr += sprintf(curr, "WM8960 Register %d\n", wm8960_reg_number);
		curr += sprintf(curr, "\n");
		curr += sprintf(curr, " Value: 0x%x\n",
				wm8960_read_reg_cache(wm8960_codec, wm8960_reg_number));
		curr += sprintf(curr, "\n");
	}

	return curr - buf;
}

static ssize_t wm8960_regval_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	if (wm8960_reg_number >= WM8960_CACHEREGNUM) {
		printk(KERN_ERR "No codec register %d\n", wm8960_reg_number);
		return size;
	}

	if (sscanf(buf, "%x", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}
	
	wm8960_write(wm8960_codec, wm8960_reg_number, value);
	return size;
}
static SYSDEV_ATTR(wm8960_regval, 0644, wm8960_regval_show, wm8960_regval_store);

static ssize_t wm8960_headset_status_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	char *curr = buf;

	curr += sprintf(curr, "%d\n", gpio_headset_status(codec_state));

	return curr - buf;
}
static SYSDEV_ATTR(wm8960_headset_status, 0644, wm8960_headset_status_show, NULL);

/*
 * Codec power status
 * NOTE: does not work for the capture case
 */ 

static ssize_t wm8960_power_status_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
    char *curr = buf;

    curr += sprintf(curr, "%d\n", power_status_read());

    return curr - buf;
}
static SYSDEV_ATTR(wm8960_power_status, 0644, wm8960_power_status_show, NULL);

/*
 * Codec power off timeout
 * NOTE: does not work for the capture case
 */
static ssize_t wm8960_power_off_timeout_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
    char *curr = buf;
    
    curr += sprintf(curr, "%d\n", power_off_timeout);

    return curr - buf;
}

static ssize_t wm8960_power_off_timeout_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0) {
		printk(KERN_ERR "Error setting the value for timeout\n");
		return -EINVAL;
	}

        power_off_timeout = value;

	return size;
}
static SYSDEV_ATTR(wm8960_power_off_timeout, 0644, wm8960_power_off_timeout_show, wm8960_power_off_timeout_store);

static ssize_t wm8960_codec_mute_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
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

	wm8960_headset_mute(value);

    return size;
}

static SYSDEV_ATTR(wm8960_codec_mute, 0644, NULL, wm8960_codec_mute_store);

static ssize_t wm8960_diags_volume_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	unsigned int hp_l;	
	unsigned int hp_r;	
	unsigned int spk_l;	
	unsigned int spk_r;	


	if (sscanf(buf, "%x %x %x %x", &hp_l, &hp_r, &spk_l, &spk_r) < 0) {
		printk(KERN_ERR "Error setting audio codec volume \n");
		return -EINVAL;
	}

	wm8960_write(wm8960_codec, WM8960_LOUT1, hp_l);
	wm8960_write(wm8960_codec, WM8960_ROUT1, hp_r);	
	wm8960_write(wm8960_codec, WM8960_LOUT2, spk_l);
	wm8960_write(wm8960_codec, WM8960_ROUT2, spk_r);

	return size;
}

static ssize_t wm8960_diags_volume_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	unsigned int hp_l;	
	unsigned int hp_r;	
	unsigned int spk_l;	
	unsigned int spk_r;	

	hp_l = wm8960_read_reg_cache(wm8960_codec, WM8960_LOUT1);
	hp_r = wm8960_read_reg_cache(wm8960_codec, WM8960_ROUT1);	
	spk_l = wm8960_read_reg_cache(wm8960_codec, WM8960_LOUT2);
	spk_r = wm8960_read_reg_cache(wm8960_codec, WM8960_ROUT2);

	return sprintf(buf, "0x%03X 0x%03X 0x%03X 0x%03X \n", 
				hp_l, hp_r, spk_l, spk_r);
}

static SYSDEV_ATTR(wm8960_diags_volume, 0644, wm8960_diags_volume_show, wm8960_diags_volume_store);

static struct sysdev_class audio_wm8960_sysclass = {
	.name	= "audio_wm8960",
};

static struct sys_device audio_wm8960_device = {
	.id	= 0,
	.cls	= &audio_wm8960_sysclass,
};

/* Diags mode for codec */
static ssize_t wm8960_codec_diags_store(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) < 0) {
		printk(KERN_ERR "Error setting diags mode\n");
		return -EINVAL;
	}

	if ((value < 0) || (value >= CODEC_DIAGS_MODE_MAX) ) {
		printk(KERN_ERR "Diags mode: Invalid value = %d\n", value);
		return -EINVAL;
	}

	if (wm8960_codec_diags_mode != value)
	{
		wm8960_codec_diags_mode = value;
	}
	return size;
}

static ssize_t wm8960_codec_diags_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", wm8960_codec_diags_mode);
}

static SYSDEV_ATTR(wm8960_codec_diags, 0644, wm8960_codec_diags_show, wm8960_codec_diags_store);
/*
 * read wm8960 register cache
 */
static inline unsigned int wm8960_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache;

	mutex_lock(&wm8960_codec_lock);

	cache = codec->reg_cache;
	if (reg == WM8960_RESET) {
		mutex_unlock(&wm8960_codec_lock);
		return 0;
	}

	if (reg >= WM8960_CACHEREGNUM) {
		mutex_unlock(&wm8960_codec_lock);
		return -1;
	}

	mutex_unlock(&wm8960_codec_lock);

	return cache[reg];
}

/*
 * write wm8960 register cache
 */
static inline void wm8960_write_reg_cache(struct snd_soc_codec *codec,
	u16 reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	if (reg >= WM8960_CACHEREGNUM)
		return;
	cache[reg] = value;
}

/*
 * write to the WM8960 register space
 */
static int wm8960_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];
	int ret = 0;

        mutex_lock(&wm8960_codec_lock);

	/* data is
	 *   D15..D9 WM8960 register offset
	 *   D8...D0 register data
	 */
	data[0] = (reg << 1) | ((value >> 8) & 0x0001);
	data[1] = value & 0x00ff;

	wm8960_write_reg_cache (codec, reg, value);
	ret = codec->hw_write(codec->control_data, data, 2);

	if (ret == 2) {
		mutex_unlock(&wm8960_codec_lock);
		return 0;
	}
	else {	
		mutex_unlock(&wm8960_codec_lock);
		return ret;
	}
}

#define wm8960_reset(c)	wm8960_write(c, WM8960_RESET, 0)

/* enumerated controls */
static const char *wm8960_deemph[] = {"None", "32Khz", "44.1Khz", "48Khz"};
static const char *wm8960_polarity[] = {"No Inversion", "Left Inverted",
	"Right Inverted", "Stereo Inversion"};
static const char *wm8960_3d_upper_cutoff[] = {"High", "Low"};
static const char *wm8960_3d_lower_cutoff[] = {"Low", "High"};
static const char *wm8960_alcfunc[] = {"Off", "Right", "Left", "Stereo"};
static const char *wm8960_alcmode[] = {"ALC", "Limiter"};

static const struct soc_enum wm8960_enum[] = {
	SOC_ENUM_SINGLE(WM8960_DACCTL1, 1, 4, wm8960_deemph),
	SOC_ENUM_SINGLE(WM8960_DACCTL1, 5, 4, wm8960_polarity),
	SOC_ENUM_SINGLE(WM8960_DACCTL2, 5, 4, wm8960_polarity),
	SOC_ENUM_SINGLE(WM8960_3D, 6, 2, wm8960_3d_upper_cutoff),
	SOC_ENUM_SINGLE(WM8960_3D, 5, 2, wm8960_3d_lower_cutoff),
	SOC_ENUM_SINGLE(WM8960_ALC1, 7, 4, wm8960_alcfunc),
	SOC_ENUM_SINGLE(WM8960_ALC3, 8, 2, wm8960_alcmode),
};

static const DECLARE_TLV_DB_SCALE(adc_tlv, -9700, 50, 0);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -12700, 50, 1);
static const DECLARE_TLV_DB_SCALE(bypass_tlv, -2100, 300, 0);
static const DECLARE_TLV_DB_SCALE(out_tlv, -12100, 100, 1);

static const struct snd_kcontrol_new wm8960_snd_controls[] = {
SOC_DOUBLE_R_TLV("Capture Volume", WM8960_LINVOL, WM8960_RINVOL,
		 0, 63, 0, adc_tlv),
SOC_DOUBLE_R("Capture Volume ZC Switch", WM8960_LINVOL, WM8960_RINVOL,
	6, 1, 0),
SOC_DOUBLE_R("Capture Switch", WM8960_LINVOL, WM8960_RINVOL,
	7, 1, 0),

SOC_DOUBLE_R_TLV("Playback Volume", WM8960_LDAC, WM8960_RDAC,
		 0, 255, 0, dac_tlv),

SOC_DOUBLE_R_TLV("Headphone Playback Volume", WM8960_LOUT1, WM8960_ROUT1,
		 0, 127, 0, out_tlv),
SOC_DOUBLE_R("Headphone Playback ZC Switch", WM8960_LOUT1, WM8960_ROUT1,
	7, 1, 0),

SOC_DOUBLE_R_TLV("Speaker Playback Volume", WM8960_LOUT2, WM8960_ROUT2,
		 0, 127, 0, out_tlv),
SOC_DOUBLE_R("Speaker Playback ZC Switch", WM8960_LOUT2, WM8960_ROUT2,
	7, 1, 0),
SOC_SINGLE("Speaker DC Volume", WM8960_CLASSD3, 3, 5, 0),
SOC_SINGLE("Speaker AC Volume", WM8960_CLASSD3, 0, 5, 0),

SOC_SINGLE("PCM Playback -6dB Switch", WM8960_DACCTL1, 7, 1, 0),
SOC_ENUM("ADC Polarity", wm8960_enum[1]),
SOC_ENUM("Playback De-emphasis", wm8960_enum[0]),
SOC_SINGLE("ADC High Pass Filter Switch", WM8960_DACCTL1, 0, 1, 0),

SOC_ENUM("DAC Polarity", wm8960_enum[2]),

SOC_ENUM("3D Filter Upper Cut-Off", wm8960_enum[3]),
SOC_ENUM("3D Filter Lower Cut-Off", wm8960_enum[4]),
SOC_SINGLE("3D Volume", WM8960_3D, 1, 15, 0),
SOC_SINGLE("3D Switch", WM8960_3D, 0, 1, 0),

SOC_ENUM("ALC Function", wm8960_enum[5]),
SOC_SINGLE("ALC Max Gain", WM8960_ALC1, 4, 7, 0),
SOC_SINGLE("ALC Target", WM8960_ALC1, 0, 15, 1),
SOC_SINGLE("ALC Min Gain", WM8960_ALC2, 4, 7, 0),
SOC_SINGLE("ALC Hold Time", WM8960_ALC2, 0, 15, 0),
SOC_ENUM("ALC Mode", wm8960_enum[6]),
SOC_SINGLE("ALC Decay", WM8960_ALC3, 4, 15, 0),
SOC_SINGLE("ALC Attack", WM8960_ALC3, 0, 15, 0),

SOC_SINGLE("Noise Gate Threshold", WM8960_NOISEG, 3, 31, 0),
SOC_SINGLE("Noise Gate Switch", WM8960_NOISEG, 0, 1, 0),

SOC_DOUBLE_R("ADC PCM Capture Volume", WM8960_LINPATH, WM8960_RINPATH,
	0, 127, 0),

SOC_SINGLE_TLV("Left Output Mixer Boost Bypass Volume",
	       WM8960_BYPASS1, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Left Output Mixer LINPUT3 Volume",
	       WM8960_LOUTMIX, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Right Output Mixer Boost Bypass Volume",
	       WM8960_BYPASS2, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Right Output Mixer RINPUT3 Volume",
	       WM8960_ROUTMIX, 4, 7, 1, bypass_tlv),
};

static const struct snd_kcontrol_new wm8960_lin_boost[] = {
SOC_DAPM_SINGLE("LINPUT2 Switch", WM8960_LINPATH, 6, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM8960_LINPATH, 7, 1, 0),
SOC_DAPM_SINGLE("LINPUT1 Switch", WM8960_LINPATH, 8, 1, 0),
};

static const struct snd_kcontrol_new wm8960_lin[] = {
SOC_DAPM_SINGLE("Boost Switch", WM8960_LINPATH, 3, 1, 0),
};

static const struct snd_kcontrol_new wm8960_rin_boost[] = {
SOC_DAPM_SINGLE("RINPUT2 Switch", WM8960_RINPATH, 6, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM8960_RINPATH, 7, 1, 0),
SOC_DAPM_SINGLE("RINPUT1 Switch", WM8960_RINPATH, 8, 1, 0),
};

static const struct snd_kcontrol_new wm8960_rin[] = {
SOC_DAPM_SINGLE("Boost Switch", WM8960_RINPATH, 3, 1, 0),
};

static const struct snd_kcontrol_new wm8960_loutput_mixer[] = {
SOC_DAPM_SINGLE("PCM Playback Switch", WM8960_LOUTMIX, 8, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM8960_LOUTMIX, 7, 1, 0),
SOC_DAPM_SINGLE("Boost Bypass Switch", WM8960_BYPASS1, 7, 1, 0),
};

static const struct snd_kcontrol_new wm8960_routput_mixer[] = {
SOC_DAPM_SINGLE("PCM Playback Switch", WM8960_ROUTMIX, 8, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM8960_ROUTMIX, 7, 1, 0),
SOC_DAPM_SINGLE("Boost Bypass Switch", WM8960_BYPASS2, 7, 1, 0),
};

static const struct snd_kcontrol_new wm8960_mono_out[] = {
SOC_DAPM_SINGLE("Left Switch", WM8960_MONOMIX1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Switch", WM8960_MONOMIX2, 7, 1, 0),
};

/* No Dynamic PM at the moment */
static const struct snd_soc_dapm_widget wm8960_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("LINPUT1"),
SND_SOC_DAPM_INPUT("RINPUT1"),
SND_SOC_DAPM_INPUT("LINPUT2"),
SND_SOC_DAPM_INPUT("RINPUT2"),
SND_SOC_DAPM_INPUT("LINPUT3"),
SND_SOC_DAPM_INPUT("RINPUT3"),

SND_SOC_DAPM_MICBIAS("MICB", SND_SOC_NOPM, 0, 0),

SND_SOC_DAPM_MIXER("Left Boost Mixer", SND_SOC_NOPM, 0, 0,
		   wm8960_lin_boost, ARRAY_SIZE(wm8960_lin_boost)),
SND_SOC_DAPM_MIXER("Right Boost Mixer", SND_SOC_NOPM, 4, 0,
		   wm8960_rin_boost, ARRAY_SIZE(wm8960_rin_boost)),

SND_SOC_DAPM_MIXER("Left Input Mixer", SND_SOC_NOPM, 5, 0,
		   wm8960_lin, ARRAY_SIZE(wm8960_lin)),
SND_SOC_DAPM_MIXER("Right Input Mixer", SND_SOC_NOPM, 4, 0,
		   wm8960_rin, ARRAY_SIZE(wm8960_rin)),

SND_SOC_DAPM_ADC("Left ADC", "Capture", SND_SOC_NOPM, 3, 0),
SND_SOC_DAPM_ADC("Right ADC", "Capture", SND_SOC_NOPM, 2, 0),

SND_SOC_DAPM_DAC("Left DAC", "Playback", SND_SOC_NOPM, 8, 0),
SND_SOC_DAPM_DAC("Right DAC", "Playback", SND_SOC_NOPM, 7, 0),

SND_SOC_DAPM_MIXER("Left Output Mixer", SND_SOC_NOPM, 3, 0,
	&wm8960_loutput_mixer[0],
	ARRAY_SIZE(wm8960_loutput_mixer)),
SND_SOC_DAPM_MIXER("Right Output Mixer", SND_SOC_NOPM, 2, 0,
	&wm8960_routput_mixer[0],
	ARRAY_SIZE(wm8960_routput_mixer)),

SND_SOC_DAPM_MIXER("Mono Output Mixer", SND_SOC_NOPM, 7, 0,
	&wm8960_mono_out[0],
	ARRAY_SIZE(wm8960_mono_out)),

SND_SOC_DAPM_PGA("LOUT1 PGA", SND_SOC_NOPM, 6, 0, NULL, 0),
SND_SOC_DAPM_PGA("ROUT1 PGA", SND_SOC_NOPM, 5, 0, NULL, 0),

SND_SOC_DAPM_PGA("Left Speaker PGA", SND_SOC_NOPM, 4, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Speaker PGA", SND_SOC_NOPM, 3, 0, NULL, 0),

SND_SOC_DAPM_PGA("Right Speaker Output", SND_SOC_NOPM, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left Speaker Output", SND_SOC_NOPM, 6, 0, NULL, 0),

SND_SOC_DAPM_OUTPUT("SPK_LP"),
SND_SOC_DAPM_OUTPUT("SPK_LN"),
SND_SOC_DAPM_OUTPUT("HP_L"),
SND_SOC_DAPM_OUTPUT("HP_R"),
SND_SOC_DAPM_OUTPUT("SPK_RP"),
SND_SOC_DAPM_OUTPUT("SPK_RN"),
SND_SOC_DAPM_OUTPUT("OUT3"),
};

static const struct snd_soc_dapm_route audio_paths[] = {
	{ "Left Boost Mixer", "LINPUT1 Switch", "LINPUT1" },
	{ "Left Boost Mixer", "LINPUT2 Switch", "LINPUT2" },
	{ "Left Boost Mixer", "LINPUT3 Switch", "LINPUT3" },

	{ "Left Input Mixer", "Boost Switch", "Left Boost Mixer", },
	{ "Left Input Mixer", NULL, "LINPUT1", },  /* Really Boost Switch */
	{ "Left Input Mixer", NULL, "LINPUT2" },
	{ "Left Input Mixer", NULL, "LINPUT3" },

	{ "Right Boost Mixer", "RINPUT1 Switch", "RINPUT1" },
	{ "Right Boost Mixer", "RINPUT2 Switch", "RINPUT2" },
	{ "Right Boost Mixer", "RINPUT3 Switch", "RINPUT3" },

	{ "Right Input Mixer", "Boost Switch", "Right Boost Mixer", },
	{ "Right Input Mixer", NULL, "RINPUT1", },  /* Really Boost Switch */
	{ "Right Input Mixer", NULL, "RINPUT2" },
	{ "Right Input Mixer", NULL, "LINPUT3" },

	{ "Left ADC", NULL, "Left Input Mixer" },
	{ "Right ADC", NULL, "Right Input Mixer" },

	{ "Left Output Mixer", "LINPUT3 Switch", "LINPUT3" },
	{ "Left Output Mixer", "Boost Bypass Switch", "Left Boost Mixer"} ,
	{ "Left Output Mixer", "PCM Playback Switch", "Left DAC" },

	{ "Right Output Mixer", "RINPUT3 Switch", "RINPUT3" },
	{ "Right Output Mixer", "Boost Bypass Switch", "Right Boost Mixer" } ,
	{ "Right Output Mixer", "PCM Playback Switch", "Right DAC" },

	{ "Mono Output Mixer", "Left Switch", "Left Output Mixer" },
	{ "Mono Output Mixer", "Right Switch", "Right Output Mixer" },

	{ "LOUT1 PGA", NULL, "Left Output Mixer" },
	{ "ROUT1 PGA", NULL, "Right Output Mixer" },

	{ "HP_L", NULL, "LOUT1 PGA" },
	{ "HP_R", NULL, "ROUT1 PGA" },

	{ "Left Speaker PGA", NULL, "Left Output Mixer" },
	{ "Right Speaker PGA", NULL, "Right Output Mixer" },

	{ "Left Speaker Output", NULL, "Left Speaker PGA" },
	{ "Right Speaker Output", NULL, "Right Speaker PGA" },

	{ "SPK_LN", NULL, "Left Speaker Output" },
	{ "SPK_LP", NULL, "Left Speaker Output" },
	{ "SPK_RN", NULL, "Right Speaker Output" },
	{ "SPK_RP", NULL, "Right Speaker Output" },

	{ "OUT3", NULL, "Mono Output Mixer", }
};

static void wm8960_reinit(struct snd_soc_codec *codec);

static int wm8960_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, wm8960_dapm_widgets,
				ARRAY_SIZE(wm8960_dapm_widgets));
	snd_soc_dapm_add_routes(codec, audio_paths, ARRAY_SIZE(audio_paths));

	return 0;
}

static int wm8960_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	wm8960_reinit(codec);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBS_CFM:
		iface |= 0x040; /* Codec is Master */
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x002; /* Codec supports I2S */
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}
	/* Set the word length to 16 bits, i.e. bits 2 and 3 should be 0,0 */
	iface = iface & 0x1f3;	

	/* set iface */
	wm8960_write(codec, WM8960_IFACE1, iface);
	return 0;
}

static int wm8960_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	u16 iface = wm8960_read_reg_cache(codec, WM8960_IFACE1) & 0xfff3;

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	}

	/* set iface */
	wm8960_write(codec, WM8960_IFACE1, iface);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		wm8960_configure_capture(codec);

	return 0;
}

static int wm8960_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = wm8960_read_reg_cache(codec, WM8960_DACCTL1) & 0xfff7;

	if (mute)
		wm8960_write(codec, WM8960_DACCTL1, mute_reg | 0x8);
	else
		wm8960_write(codec, WM8960_DACCTL1, mute_reg);
	return 0;
}

/* PLL divisors */
struct _pll_div {
	u32 pre_div:1;
	u32 n:4;
	u32 k:24;
};

static struct _pll_div pll_div;

/* The size in bits of the pll divide multiplied by 10
 * to allow rounding later */
#define FIXED_PLL_SIZE ((1 << 24) * 10)

/*
 * Currently unused as the sampling rate values will be hard coded
 */
static void pll_factors(unsigned int target, unsigned int source)
{
	unsigned long long Kpart;
	unsigned int K, Ndiv, Nmod;

	Ndiv = target / source;
	if (Ndiv < 6) {
		source >>= 1;
		pll_div.pre_div = 1;
		Ndiv = target / source;
	} else
		pll_div.pre_div = 0;

	if ((Ndiv < 6) || (Ndiv > 12))
		printk(KERN_WARNING
			"WM8960 N value outwith recommended range! N = %d\n",Ndiv);

	pll_div.n = Ndiv;
	Nmod = target % source;
	Kpart = FIXED_PLL_SIZE * (long long)Nmod;

	do_div(Kpart, source);

	K = Kpart & 0xFFFFFFFF;

	/* Check if we need to round */
	if ((K % 10) >= 5)
		K += 5;

	/* Move down to proper range now rounding is done */
	K /= 10;

	pll_div.k = K;
}

/* Capture or playback */
int wm8960_capture = 0;
EXPORT_SYMBOL(wm8960_capture);

static int headset_attached = 0;

static void wm8960_power_on(int power)
{
	if (power) {
		wm8960_write(wm8960_codec, WM8960_APOP1, 0x94);
		wm8960_write(wm8960_codec, WM8960_APOP2, 0x40);

		mdelay(400);

		wm8960_write(wm8960_codec, WM8960_POWER2, 0x62);
		wm8960_write(wm8960_codec, WM8960_APOP2, 0x0);
		wm8960_write(wm8960_codec, WM8960_POWER1, 0x80);
	
		mdelay(100);

		wm8960_write(wm8960_codec, WM8960_POWER1, 0xc0);
		wm8960_write(wm8960_codec, WM8960_APOP1, 0x0);
		wm8960_write(wm8960_codec, WM8960_POWER2, 0x1fb);
		wm8960_write(wm8960_codec, WM8960_POWER3, 0x0c);
		wm8960_write(wm8960_codec, WM8960_LOUTMIX, 0x150);
		wm8960_write(wm8960_codec, WM8960_ROUTMIX, 0x150);
		wm8960_write(wm8960_codec, WM8960_DACCTL2, 0xc);
		wm8960_write(wm8960_codec, WM8960_DACCTL1, 0x0);

                power_status_write(1);
	}
	else {
		wm8960_write(wm8960_codec, WM8960_DACCTL1, 0x8);
		mdelay(20);

		wm8960_write(wm8960_codec, WM8960_POWER2, 0x62);
		mdelay(20);

		wm8960_write(wm8960_codec, WM8960_CLASSD1, 0x37);
		wm8960_write(wm8960_codec, WM8960_APOP1, 0x94);
		mdelay(20);

		wm8960_write(wm8960_codec, WM8960_POWER1, 0x0);
		mdelay(1000);

		wm8960_write(wm8960_codec, WM8960_POWER2, 0x0);

		if (wm8960_capture) {
			wm8960_reset(wm8960_codec);
		}
                
                power_status_write(0);
	}
}

static void wm8960_power_on_startup(void)
{
	/* Pre-set Headphone volume */
	wm8960_write(wm8960_codec, WM8960_LOUT1, LOUT1);
	wm8960_write(wm8960_codec, WM8960_ROUT1, ROUT1);
	/* Pre-set Speaker volume */
	wm8960_write(wm8960_codec, WM8960_LOUT2, LOUT2);
	wm8960_write(wm8960_codec, WM8960_ROUT2, ROUT2);

	wm8960_write(wm8960_codec, WM8960_DACCTL1, 0x8);
	wm8960_write(wm8960_codec, WM8960_POWER2, 0x62);

	wm8960_write(wm8960_codec, WM8960_CLASSD1, 0x37);
	wm8960_write(wm8960_codec, WM8960_APOP1, 0x94);

	wm8960_write(wm8960_codec, WM8960_POWER1, 0x0);
	wm8960_write(wm8960_codec, WM8960_POWER2, 0x0);

	power_status_write(0);
}

static void wm8960_poweroff_work(struct work_struct *dummy)
{
	wm8960_power_on(0);
	wm8960_capture = 0;       

	mutex_lock(&wm8960_state_lock);

	disable_irq(gpio_headset_irq(WM8960_ACTIVE));
	codec_state = WM8960_IDLE;
	enable_irq(gpio_headset_irq(WM8960_IDLE));

	mutex_unlock(&wm8960_state_lock);
}

static DECLARE_DELAYED_WORK(wm8960_poweroff_wq, &wm8960_poweroff_work);

static int wm8960_trigger(struct snd_pcm_substream *substream, int cmd,
					struct snd_soc_dai *codec_dai)
{
    switch(cmd) {
    case SNDRV_PCM_TRIGGER_STOP:
        /* If value of power_off_timeout is less than zero
         * then keep the codec on forever
         */
        if (power_off_timeout >= 0)
            schedule_delayed_work(&wm8960_poweroff_wq, msecs_to_jiffies(power_off_timeout));
        break;

    default:
        break;
    }

    return 0;
}

static void wm8960_configure_capture(struct snd_soc_codec *codec)
{
	wm8960_write(wm8960_codec, WM8960_APOP1, 0x4);
	wm8960_write(wm8960_codec, WM8960_APOP2, 0x0);

	mdelay(400);

	wm8960_write(wm8960_codec, WM8960_POWER2, 0x62);
	wm8960_write(wm8960_codec, WM8960_POWER1, 0x80);

	mdelay(100);	

	/* ADC and DAC control registers */
	wm8960_write(codec, WM8960_DACCTL1, 0x0);
	wm8960_write(codec, WM8960_DACCTL2, 0xc);

	/* Left and right channel output volume */
	wm8960_write(codec, WM8960_ROUT1, 0x179);
	wm8960_write(codec, WM8960_LOUT1, 0x179);

	/* Left and right channel mixer volume */
	wm8960_write(codec, WM8960_ROUTMIX, 0x150);
	wm8960_write(codec, WM8960_LOUTMIX, 0x150);

	/* Set WL8 and ALRCGPIO enabled */
	wm8960_write(codec, WM8960_IFACE2, 0x40);

	/* Configure WL - 16bits, Master mode and I2S format */
	wm8960_write(codec, WM8960_IFACE1, 0x42);

	/* Left Mic Boost */
	wm8960_write(codec, WM8960_LINPATH, 0x108);
	/* Right Mic Boost */
	wm8960_write(codec, WM8960_RINPATH, 0x108);

	/* Left/right input volumes */
	wm8960_write(codec, WM8960_LINVOL, 0x13f);	
	wm8960_write(codec, WM8960_RINVOL, 0x13f);	

	/* Max input gain setting */
	wm8960_write(codec, WM8960_ALC1, 0x0);

	/* Left ADC Channel. No right channel since input is MONO */
	wm8960_write(codec, WM8960_LADC, 0x1cf);
	wm8960_write(codec, WM8960_RADC, 0x1cf);

	/* Power Management: Turn on ADC left and right, MIC and VMID */
	wm8960_write(codec, WM8960_POWER1, 0xfe);

	/* Power Management: Turn on DAC, speaker and PLL */
	wm8960_write(codec, WM8960_POWER2, 0x1fb);

	/* Power Management: Turn on Left/Right MIC and left/right Mixer */
	wm8960_write(codec, WM8960_POWER3, 0x3c);
}

static void wm8960_headset_event(void)
{
	if (gpio_headset_status(codec_state)) {
		if (headset_attached) {
			kobject_uevent(&mxc_headset.this_device->kobj, KOBJ_REMOVE);
			headset_attached = 0;
		}
	}
	else {
		if (!headset_attached) {
			kobject_uevent(&mxc_headset.this_device->kobj, KOBJ_ADD);
			headset_attached = 1;
		}
	}
};

static int wm8960_set_dai_pll(struct snd_soc_dai *codec_dai,
		int pll_id, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;

	if (freq_in == 0 || freq_out == 0) {
		return 0; /* Disable the PLL */
	}

        /* 
         *  Cancel any delayed work
         */
        cancel_delayed_work_sync(&wm8960_poweroff_wq);
        
        /* 
         * Read the power status and if it is still on
         * skip doing anything.
         */
        if (power_status_read()) {
            return 0;
        }

	pll_factors(freq_out * 8, freq_in);

	/* Configure PLLK1, PLLK2 and PLLN for sampling rates */
	reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x1f0;
	reg = reg | 0x20;
	wm8960_write(codec, WM8960_PLLN, reg);

	if (pll_id == 0) {
		/* 11.2896 MHz */
		reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x1f0;
		reg = reg | 0x7;
		wm8960_write(codec, WM8960_PLLN, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK3);
		reg = reg & 0x100;
		reg = reg | 0x26;
		wm8960_write(codec, WM8960_PLLK3, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK2);
		reg = reg & 0x100;
		reg = reg | 0xC2;
		wm8960_write(codec, WM8960_PLLK2, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK1);
		reg = reg & 0x100;
		reg = reg | 0x86;
		wm8960_write(codec, WM8960_PLLK1, reg);		
	}
	else {
		/* 12.288 MHz */
		reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x1f0;
		reg = reg | 0x8;
		wm8960_write(codec, WM8960_PLLN, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK3);
		reg = reg & 0x100;
		reg = reg | 0xE8;
		wm8960_write(codec, WM8960_PLLK3, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK2);
		reg = reg & 0x100;
		reg = reg | 0x26;
		wm8960_write(codec, WM8960_PLLK2, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK1);
		reg = reg & 0x100;
		reg = reg | 0x31;
		wm8960_write(codec, WM8960_PLLK1, reg);
	}
	wm8960_write(codec, WM8960_ADDCTL3,wm8960_read_reg_cache(codec, WM8960_ADDCTL3) | 0x8);

	wm8960_write(codec, WM8960_LDAC, LDAC);
	wm8960_write(codec, WM8960_RDAC, RDAC);

	reg = wm8960_read_reg_cache(codec, WM8960_ADDCTL2);
	reg |= HPSWPOLARITY;
	/**
	 * Need to disable automatic head phone switch in automatic 
	 * speaker test mode 
	 */
	if (wm8960_codec_diags_mode != CODEC_DIAGS_MODE_AUTOMATED_SPEAKER_TEST)
	{
		reg |= 0x40; /* Headphone Switch Enable */
	}
	else
	{
		reg &= ~0x40; /* Headphone Switch Disable */
	}
    wm8960_write(codec, WM8960_ADDCTL2, reg);

	/* ADLRC set */
	wm8960_write(codec, WM8960_IFACE2,wm8960_read_reg_cache(codec, WM8960_IFACE2) | 0x40);
	wm8960_write(codec, WM8960_ADDCTL1,wm8960_read_reg_cache(codec, WM8960_ADDCTL1) | 0x3);

	/* Clear out MONOMIX 1 and 2 */
	wm8960_write(codec, WM8960_MONOMIX1, 0x0);
	wm8960_write(codec, WM8960_MONOMIX2, 0x0);

	if (!wm8960_capture) {	
		wm8960_write(codec, WM8960_CLASSD1, SPKR_CONFIG);
		wm8960_write(codec, WM8960_CLASSD3, CLASSD3_CONFIG);
	}

	reg = wm8960_read_reg_cache(codec, WM8960_ADDCTL4);
	/*
	 * HPSEL should be set to 10, GPIOSEL should be 001. This is needed
	 * for headset/speaker switching. JD2 is used for the switching
	 * in the codec
	 */
	reg &= 0x1BB;
	reg |= 0x8 | 0x10 | 0x21 | 0x2; /* GPIOSEL to be Jack detect output */		
	wm8960_write(codec, WM8960_ADDCTL4, reg);

	mutex_lock(&wm8960_state_lock);

	disable_irq(gpio_headset_irq(WM8960_IDLE));
	codec_state = WM8960_ACTIVE;
	
	if (!wm8960_capture)
		wm8960_power_on(1);
	
	enable_irq(gpio_headset_irq(WM8960_ACTIVE));
	
	mutex_unlock(&wm8960_state_lock);

	return 0;
}

static int wm8960_set_dai_clkdiv(struct snd_soc_dai *codec_dai, 
			int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;

	switch (div_id) {
	case WM8960_SYSCLKSEL:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK1) & 0x1fe;
		wm8960_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_SYSCLKDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK1) & 0x1f9;
		wm8960_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_DACDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK1) & 0x1c7;
		wm8960_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_ADCDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK1) & 0x03f;
		wm8960_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_OPCLKDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x03f;
		wm8960_write(codec, WM8960_PLLN, reg | div);
		break;
	case WM8960_PRESCALEDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x1EF;
		wm8960_write(codec, WM8960_PLLN, reg | div);
		break;
	case WM8960_DCLKDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK2) & 0x03f;
		wm8960_write(codec, WM8960_CLOCK2, reg | div);
		break;
	case WM8960_BCLKDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK2) & 0x1f0;
		wm8960_write(codec, WM8960_CLOCK2, reg | div);
		break;
	case WM8960_TOCLKSEL:
		reg = wm8960_read_reg_cache(codec, WM8960_ADDCTL1) & 0x1fd;
		wm8960_write(codec, WM8960_ADDCTL1, reg | div);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define WM8960_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000)

#define WM8960_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

#define WM8960_CAPTURE_RATES		\
	(SNDRV_PCM_RATE_8000	| 	\
	 SNDRV_PCM_RATE_11025	| 	\
	 SNDRV_PCM_RATE_16000	|	\
	 SNDRV_PCM_RATE_22050	|	\
	 SNDRV_PCM_RATE_32000	|	\
	 SNDRV_PCM_RATE_44100	|	\
	 SNDRV_PCM_RATE_48000)	

static struct snd_soc_dai_ops wm8960_dai_ops = {
	.hw_params = wm8960_hw_params,
	.set_clkdiv = wm8960_set_dai_clkdiv,
	.set_fmt = wm8960_set_dai_fmt,
	.set_pll = wm8960_set_dai_pll,
	.digital_mute = wm8960_mute,
	.trigger = wm8960_trigger,
};

struct snd_soc_dai wm8960_dai = {
	.name = "WM8960",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8960_RATES,
		.formats = WM8960_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8960_CAPTURE_RATES,
		.formats = WM8960_FORMATS,},
	.ops = &wm8960_dai_ops,
	.symmetric_rates = 1,
};
EXPORT_SYMBOL_GPL(wm8960_dai);

/*!
 * Set the bias level
 */
static int wm8960_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	u16 reg = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		/* Set VMID to 2x50k */
		reg = snd_soc_read(codec, WM8960_POWER1);
		reg &= ~0x181;
		reg |= 0x80 | 0x40;
		snd_soc_write(codec, WM8960_POWER1, reg);
		break;

	case SND_SOC_BIAS_OFF:
	case SND_SOC_BIAS_STANDBY:
		/* Enable anti-pop features */
		snd_soc_write(codec, WM8960_APOP1,
			     WM8960_POBCTRL | WM8960_SOFT_ST |
			     WM8960_BUFDCOPEN | WM8960_BUFIOEN);

		/* Discharge HP output */
		reg = WM8960_DISOP;
		snd_soc_write(codec, WM8960_APOP2, reg);

		msleep(400);

		snd_soc_write(codec, WM8960_APOP2, 0);

		/* Enable & ramp VMID at 2x50k */
		reg = snd_soc_read(codec, WM8960_POWER1);
		reg |= 0x80;
		snd_soc_write(codec, WM8960_POWER1, reg);
		msleep(100);

		/* Enable VREF */
		snd_soc_write(codec, WM8960_POWER1, reg | WM8960_VREF);

		/* Disable anti-pop features */
		snd_soc_write(codec, WM8960_APOP1, WM8960_BUFIOEN);

		/* Set VMID to 2x250k */
		reg = snd_soc_read(codec, WM8960_POWER1);
		reg &= ~0x180;
		reg |= 0x100;
		snd_soc_write(codec, WM8960_POWER1, reg);

		break;
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

struct dentry *wm8960_debugfs_root;
struct dentry *wm8960_debugfs_state;

static int wm8960_debugfs_show(struct seq_file *s, void *p)
{
	int t;
	int i = 0;
	u16 *cache = wm8960_codec->reg_cache;

	for (i=0; i < WM8960_CACHEREGNUM; i++) {
		t += seq_printf(s, "Register %d - Value:0x%x\n", i, cache[i]);
	}

	if (gpio_headset_status(codec_state))
		t += seq_printf(s, "wm8960: Headset not plugged in\n");
	else
		t += seq_printf(s, "wm8960: Headset plugged in\n");

	t += seq_printf(s, "WM8960_RATES: 0x%x\n", WM8960_RATES);
	t += seq_printf(s, "WM8960_CAPTURE_RATES: 0x%x\n", WM8960_CAPTURE_RATES);

	return 0;
}

static int wm8960_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, wm8960_debugfs_show, inode->i_private);
}

static const struct file_operations wm8960_debugfs_fops = {
	.owner		= THIS_MODULE,
	.open		= wm8960_debugfs_open,
	.llseek		= seq_lseek,
	.read		= seq_read,
	.release	= single_release,
};

static int wm8960_debugfs_init(void)
{
	struct dentry *root, *state;
	int ret = -1;

	root = debugfs_create_dir("WM8960_Sound", NULL);
	if (IS_ERR(root) || !root)
		goto err_root;

	state = debugfs_create_file("WM8960_Codec_Settings", 0400, root, (void *)0,
				&wm8960_debugfs_fops);

	if (!state)
		goto err_state;

	wm8960_debugfs_root = root;
	wm8960_debugfs_state = state;
	return 0;
err_state:
	debugfs_remove(root);
err_root:
	printk(KERN_ERR "WM8960_Sound: debugfs is not available\n");
	return ret;
}

static void wm8960_debugfs_cleanup(void)
{
	if (wm8960_debugfs_state != NULL) 
		debugfs_remove(wm8960_debugfs_state);
	if (wm8960_debugfs_root != NULL) 
		debugfs_remove(wm8960_debugfs_root);
	wm8960_debugfs_state = NULL;
	wm8960_debugfs_root = NULL;
}

#endif /* CONFIG_DEBUG_FS */

static int wm8960_reboot(struct notifier_block *self, unsigned long action, void *cpu)
{
	wm8960_set_bias_level(wm8960_codec, SND_SOC_BIAS_OFF); 	

	wm8960_write(wm8960_codec, WM8960_POWER1, 0x1);
	wm8960_write(wm8960_codec, WM8960_POWER2, 0x0);
	wm8960_write(wm8960_codec, WM8960_POWER3, 0x0);
	return 0;
}

/*!
 * Suspend the codec
 */
static int wm8960_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	wm8960_set_bias_level(codec, SND_SOC_BIAS_OFF); 
	/*
	 * Turn off the power to the codec components
	 */
	wm8960_write(codec, WM8960_POWER1, 0x1);
	wm8960_write(codec, WM8960_POWER2, 0x0);
	wm8960_write(codec, WM8960_POWER3, 0x0);


	return 0;
}

/*!
 * Resume the codec blocks
 */
static int wm8960_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(wm8960_reg); i++) {
		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->hw_write(codec->control_data, data, 2);
	}

	wm8960_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	wm8960_set_bias_level(codec, codec->suspend_bias_level);

	wm8960_reset(codec);
	return 0;
}

static int wm8960_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;
	int error = 0;

	if (wm8960_codec == NULL) {
		dev_err(&pdev->dev, "Codec device not registered\n");
		return -ENODEV;
	}
	
	socdev->card->codec = wm8960_codec;
	codec = wm8960_codec;
	
	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(codec->dev, "failed to create pcms: %d\n", ret);
		goto pcm_err;
	}
	
	snd_soc_add_controls(codec, wm8960_snd_controls,
	                     ARRAY_SIZE(wm8960_snd_controls));

	wm8960_add_widgets(codec);

	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		dev_err(codec->dev, "failed to register card: %d\n", ret);
		goto card_err;
	}

	error = sysdev_class_register(&audio_wm8960_sysclass);
	if (!error)
		error = sysdev_register(&audio_wm8960_device);
	if (!error) {
		error = sysdev_create_file(&audio_wm8960_device, &attr_wm8960_regoff);
		error = sysdev_create_file(&audio_wm8960_device, &attr_wm8960_regval);
		error = sysdev_create_file(&audio_wm8960_device, &attr_wm8960_headset_status);
		error = sysdev_create_file(&audio_wm8960_device, &attr_wm8960_power_status);
		error = sysdev_create_file(&audio_wm8960_device, &attr_wm8960_codec_mute);
		error = sysdev_create_file(&audio_wm8960_device, &attr_wm8960_power_off_timeout);
		error = sysdev_create_file(&audio_wm8960_device, &attr_wm8960_diags_volume);
        		error = sysdev_create_file(&audio_wm8960_device, &attr_wm8960_codec_diags);
	}

	register_reboot_notifier(&wm8960_reboot_nb);
#ifdef CONFIG_DEBUG_FS
	if (wm8960_debugfs_init() < 0)
		printk(KERN_ERR "WM8960 debugfs init error\n");
#endif
	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	return ret;
}

/* power down chip */
static int wm8960_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	wm8960_set_bias_level(codec, SND_SOC_BIAS_OFF); 

	/*
	 * Turn off the power to the codec components
	 */
	wm8960_write(codec, WM8960_POWER1, 0x1);
	wm8960_write(codec, WM8960_POWER2, 0x0);
	wm8960_write(codec, WM8960_POWER3, 0x0);

	unregister_reboot_notifier(&wm8960_reboot_nb);

	sysdev_remove_file(&audio_wm8960_device, &attr_wm8960_regoff);
	sysdev_remove_file(&audio_wm8960_device, &attr_wm8960_regval);
	sysdev_remove_file(&audio_wm8960_device, &attr_wm8960_headset_status);
	sysdev_remove_file(&audio_wm8960_device, &attr_wm8960_power_status);
	sysdev_remove_file(&audio_wm8960_device, &attr_wm8960_codec_mute);
	sysdev_remove_file(&audio_wm8960_device, &attr_wm8960_power_off_timeout);
	sysdev_remove_file(&audio_wm8960_device, &attr_wm8960_diags_volume);

	sysdev_unregister(&audio_wm8960_device);
	sysdev_class_unregister(&audio_wm8960_sysclass);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

#ifdef CONFIG_DEBUG_FS
	wm8960_debugfs_cleanup();
#endif

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8960 = {
	.probe =        wm8960_probe,
	.remove =       wm8960_remove,
	.suspend =      wm8960_suspend,
	.resume =       wm8960_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_wm8960);

static void wm8960_reinit(struct snd_soc_codec *codec)
{
	unsigned int reg;

	/* 
	 * Do we need to reset codec?
	 *	wm8960_reset(codec);
	 */
	reg = wm8960_read_reg_cache(codec, WM8960_LINVOL);
	wm8960_write(codec, WM8960_LINVOL, reg & 0x017F);

	reg = wm8960_read_reg_cache(codec, WM8960_RINVOL);
	wm8960_write(codec, WM8960_RINVOL, reg & 0x017F);

	reg = wm8960_read_reg_cache(codec, WM8960_ADDCTL2);
}

static void wm8960_headset_mute(int mute)
{
	u16 mute_reg = wm8960_read_reg_cache(wm8960_codec, WM8960_DACCTL1) & 0xfff7;

	if (mute)
		wm8960_write(wm8960_codec, WM8960_DACCTL1, mute_reg | 0x8);
	else
		wm8960_write(wm8960_codec, WM8960_DACCTL1, mute_reg);
}

static void do_hsdet_work(struct work_struct *dummy)
{
	int irq = 0;

	mutex_lock(&wm8960_state_lock);
	/* Get the headset IRQ */
	irq = gpio_headset_irq(codec_state);
	
	wm8960_headset_mute(1);
	msleep(HSDET_THRESHOLD);

	wm8960_headset_event();
	/* Unmute */
	wm8960_headset_mute(0);

	enable_irq(irq);

	mutex_unlock(&wm8960_state_lock);
}
	
static DECLARE_WORK(hsdet_work, do_hsdet_work);

static irqreturn_t headset_detect_irq(int irq, void *devid)
{
	disable_irq_nosync(gpio_headset_irq(codec_state));

	/* Mute */
	schedule_work(&hsdet_work);

	return IRQ_HANDLED;
}

/*
 * initialise the WM8960 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int wm8960_register(struct wm8960_priv *wm8960)
{
	struct wm8960_data *pdata = wm8960->codec.dev->platform_data;
	struct snd_soc_codec *codec = &wm8960->codec;
	int ret;
	u16 reg;
	int irq = 0;

	if (wm8960_codec) {
		dev_err(codec->dev, "Another WM8960 is registered\n");
		return -EINVAL;
	}

	if (!pdata) {
		dev_warn(codec->dev, "No platform data supplied\n");
	} else {
		if (pdata->dres > WM8960_DRES_MAX) {
			dev_err(codec->dev, "Invalid DRES: %d\n", pdata->dres);
			pdata->dres = 0;
		}
	}

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->name = "WM8960";
	codec->owner = THIS_MODULE;
	codec->read = wm8960_read_reg_cache;
	codec->write = wm8960_write;
	codec->bias_level = SND_SOC_BIAS_OFF;
	codec->dai = &wm8960_dai;
	codec->set_bias_level = wm8960_set_bias_level;
	codec->num_dai = 1;
	codec->reg_cache_size = WM8960_CACHEREGNUM;
	codec->reg_cache = &wm8960->reg_cache;

	memcpy(codec->reg_cache, wm8960_reg, sizeof(wm8960_reg));

	ret = wm8960_reset(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to issue reset\n");
		goto err;
	}

	wm8960_dai.dev = codec->dev;

	codec->set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Latch the update bits */
	reg = snd_soc_read(codec, WM8960_LINVOL);
	snd_soc_write(codec, WM8960_LINVOL, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_RINVOL);
	snd_soc_write(codec, WM8960_RINVOL, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_LADC);
	snd_soc_write(codec, WM8960_LADC, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_RADC);
	snd_soc_write(codec, WM8960_RADC, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_LDAC);
	snd_soc_write(codec, WM8960_LDAC, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_RDAC);
	snd_soc_write(codec, WM8960_RDAC, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_LOUT1);
	snd_soc_write(codec, WM8960_LOUT1, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_ROUT1);
	snd_soc_write(codec, WM8960_ROUT1, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_LOUT2);
	snd_soc_write(codec, WM8960_LOUT2, reg | 0x100);
	reg = snd_soc_read(codec, WM8960_ROUT2);
	snd_soc_write(codec, WM8960_ROUT2, reg | 0x100);

	wm8960_codec = codec;

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_dai(&wm8960_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		goto err_codec;
	}

	irq = gpio_headset_irq(WM8960_ACTIVE);
	ret = request_irq(irq, headset_detect_irq, 
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
					"HSdet_codec", NULL);
	if (ret != 0) {
		printk(KERN_ERR "IRQF_DISABLED: Could not get codec IRQ %d\n", irq);
		goto err_irq1;
	}
	disable_irq(gpio_headset_irq(WM8960_ACTIVE));
	
	irq = gpio_headset_irq(WM8960_IDLE);
	ret = request_irq(irq, headset_detect_irq, 
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
					"HSdet_gpio", NULL);
	if (ret != 0) {
		printk(KERN_ERR "IRQF_DISABLED: Could not get gpio IRQ %d\n", irq);
		goto err_irq2;
	}

	wm8960_power_on_startup();

	ret = misc_register(&mxc_headset);
	if (ret != 0) {
		printk (KERN_WARNING "mxc_headset: Couldn't register device %d\n", HEADSET_MINOR);
		goto err_misc;
	}
	return ret;

err_misc:
	free_irq(gpio_headset_irq(WM8960_IDLE), NULL);
err_irq2:
	free_irq(gpio_headset_irq(WM8960_ACTIVE), NULL);
err_irq1:
	snd_soc_unregister_dai(&wm8960_dai);
err_codec:
	snd_soc_unregister_codec(codec);
err:
	kfree(codec->reg_cache);
	return ret;
}
	
static void wm8960_unregister(struct wm8960_priv *wm8960)
{
	misc_deregister(&mxc_headset);	
	free_irq(gpio_headset_irq(WM8960_IDLE), NULL);
	free_irq(gpio_headset_irq(WM8960_ACTIVE), NULL);

	wm8960->codec.set_bias_level(&wm8960->codec, SND_SOC_BIAS_OFF);
	snd_soc_unregister_dai(&wm8960_dai);
	snd_soc_unregister_codec(&wm8960->codec);
	kfree(wm8960);
	wm8960_codec = NULL;
}

static __devinit int wm8960_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm8960_priv *wm8960;
	struct snd_soc_codec *codec;

	wm8960 = kzalloc(sizeof(struct wm8960_priv), GFP_KERNEL);
	if (wm8960 == NULL)
		return -ENOMEM;

	codec = &wm8960->codec;
	codec->hw_write = (hw_write_t)i2c_master_send;

	i2c_set_clientdata(i2c, wm8960);
	codec->control_data = i2c;

	codec->dev = &i2c->dev;

	return wm8960_register(wm8960);
}

static __devexit int wm8960_i2c_remove(struct i2c_client *client)
{
	struct wm8960_priv *wm8960 = i2c_get_clientdata(client);
	wm8960_unregister(wm8960);
	return 0;
}

static const struct i2c_device_id wm8960_i2c_id[] = {
	{ "wm8960", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8960_i2c_id);

static struct i2c_driver wm8960_i2c_driver = {
	.driver = {
		.name = "wm8960",
		.owner = THIS_MODULE,
	},
	.probe =    wm8960_i2c_probe,
	.remove =   __devexit_p(wm8960_i2c_remove),
	.id_table = wm8960_i2c_id,
};

static int __init wm8960_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&wm8960_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register WM8960 I2C driver: %d\n",
		       ret);
	}

	return ret;
}
module_init(wm8960_modinit);

static void __exit wm8960_exit(void)
{
	i2c_del_driver(&wm8960_i2c_driver);
}
module_exit(wm8960_exit);

MODULE_DESCRIPTION("ASoC WM8960 driver");
MODULE_AUTHOR("Manish Lachwani");
MODULE_LICENSE("GPL");

/*
 * cs42l52.c -- CS42L52 ALSA SoC audio driver
 *
 * Copyright 2007 CirrusLogic, Inc.
 *
 * Author: Bo Liu <Bo.Liu@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * Revision history
 * Nov 2007  Initial version.
 * Oct 2008  Updated to 2.6.26
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "cs42l52.h"




#ifdef	DEBUG
#define	SOCDBG(fmt, arg...)		printk(KERN_ERR "%s: %s: " fmt "\n", SOC_CS42L52_NAME, __FUNCTION__, ##arg)
#else
#define	SOCDBG(fmt, arg...)
#endif

#define	SOCINF(fmt, args...)	printk(KERN_INFO "%s: " fmt "\n", SOC_CS42L52_NAME,  ##args)
#define	SOCERR(fmt, args...)	printk(KERN_ERR  "%s: " fmt "\n", SOC_CS42L52_NAME,  ##args)




/*
 * GPIO control private function
 *  arch/arm/mach-mx5/mx50_gpio.c
 */
void
gpio_audio_mute(
	int		/* muting */
);

void
gpio_audio_enable(
	int		/* enable */
);




static	const	struct	cs42l52_clk_param {
	u32		mclk;
	u32		rate;
	u8		speed;
	u8		group;
	u8		videoclk;
	u8		ratio;
	u8		mclkdiv2;
} clk_map_table[] = {
	/*	mclk		rate	speed				group				videoclk			ratio				mclkdiv2	*/
	{	12000000,	 8000,	CLK_CTL_S_QS_MODE,	CLK_CTL_32K_SR,		CLK_CTL_NOT_27M,	CLK_CTL_RATIO_125,	0	},
	{	12000000,	12000,	CLK_CTL_S_QS_MODE,	CLK_CTL_NOT_32K,	CLK_CTL_NOT_27M,	CLK_CTL_RATIO_125,	0	},
	{	12000000,	16000,	CLK_CTL_S_HS_MODE,	CLK_CTL_32K_SR,		CLK_CTL_NOT_27M,	CLK_CTL_RATIO_125,	0	},
	{	12000000,	32000,	CLK_CTL_S_SS_MODE,	CLK_CTL_32K_SR,		CLK_CTL_NOT_27M,	CLK_CTL_RATIO_125,	0	},
	{	12000000,	48000,	CLK_CTL_S_SS_MODE,	CLK_CTL_NOT_32K,	CLK_CTL_NOT_27M,	CLK_CTL_RATIO_125,	0	},
	{	12000000,	96000,	CLK_CTL_S_DS_MODE,	CLK_CTL_NOT_32K,	CLK_CTL_NOT_27M,	CLK_CTL_RATIO_125,	0	},
	{	12000000,	11025,	CLK_CTL_S_QS_MODE,	CLK_CTL_NOT_32K,	CLK_CTL_NOT_27M,	CLK_CTL_RATIO_136,	0	},
	{	12000000,	22050,	CLK_CTL_S_HS_MODE,	CLK_CTL_NOT_32K,	CLK_CTL_NOT_27M,	CLK_CTL_RATIO_136,	0	},
	{	12000000,	44100,	CLK_CTL_S_SS_MODE,	CLK_CTL_NOT_32K,	CLK_CTL_NOT_27M,	CLK_CTL_RATIO_136,	0	},
	{	12000000,	88200,	CLK_CTL_S_DS_MODE,	CLK_CTL_NOT_32K,	CLK_CTL_NOT_27M,	CLK_CTL_RATIO_136,	0	},
};




/*
 * codec private structure
 */
struct	cs42l52_private {
	/* work queue parameters */
	struct	work_struct		work;
	struct	snd_soc_codec*	codec;

	/* stream informations */
	const	struct	cs42l52_clk_param*	clk_param;
	int		stream;
	int		channels;
	int		trigger;

	/* codec status */
	struct	clk*	clk_master;
	u8				reg_cache[SOC_CS42L52_REG_NUM];
	u8				reg_sync[SOC_CS42L52_REG_NUM];

	u_int	sysclk;
	u8		format;
	u8		pwrctl1;
};




/**
 * snd_soc_get_volsw - single mixer get callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to get the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int
cs42l52_get_volsw(struct snd_kcontrol* kcontrol, struct snd_ctl_elem_value* ucontrol)
{
	struct	snd_soc_codec*	codec = snd_kcontrol_chip(kcontrol);
	int						reg = kcontrol->private_value & 0xff;
	int						shift = (kcontrol->private_value >> 8) & 0x0f;
	int						rshift = (kcontrol->private_value >> 12) & 0x0f;
	int						max = (kcontrol->private_value >> 16) & 0xff;
	int						mask = (1 << fls(max)) - 1;
	int						min = (kcontrol->private_value >> 24) & 0xff;

	ucontrol->value.integer.value[0] = ((snd_soc_read(codec, reg) >> shift) - min) & mask;
	if (shift != rshift) {
		ucontrol->value.integer.value[1] = ((snd_soc_read(codec, reg) >> rshift) - min) & mask;
	}

	return 0;
}



/**
 * snd_soc_put_volsw - single mixer put callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to set the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int
cs42l52_put_volsw(struct snd_kcontrol* kcontrol, struct snd_ctl_elem_value* ucontrol)
{
	struct	snd_soc_codec*	codec = snd_kcontrol_chip(kcontrol);
	int						reg = kcontrol->private_value & 0xff;
	int						shift = (kcontrol->private_value >> 8) & 0x0f;
	int						rshift = (kcontrol->private_value >> 12) & 0x0f;
	int						max = (kcontrol->private_value >> 16) & 0xff;
	int						mask = (1 << fls(max)) - 1;
	int						min = (kcontrol->private_value >> 24) & 0xff;
	unsigned short			val, val2, val_mask;

	val = ((ucontrol->value.integer.value[0] + min) & mask);

	val_mask = mask << shift;
	val = val << shift;
	if (shift != rshift) {
		val2 = ((ucontrol->value.integer.value[1] + min) & mask);
		val_mask |= mask << rshift;
		val |= val2 << rshift;
	}

	return snd_soc_update_bits(codec, reg, val_mask, val);
}



/**
 * snd_soc_info_volsw_2r - double mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a double mixer control that
 * spans 2 codec registers.
 *
 * Returns 0 for success.
 */
int
cs42l52_info_volsw_2r(struct snd_kcontrol* kcontrol, struct snd_ctl_elem_info* uinfo)
{
	int		max = (kcontrol->private_value >> 8) & 0xff;

	if (max == 1) {
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	} else {
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	}

	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max;

	return 0;
}



/**
 * snd_soc_get_volsw_2r - double mixer get callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to get the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 */
int
cs42l52_get_volsw_2r(struct snd_kcontrol* kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct	snd_soc_codec*	codec = snd_kcontrol_chip(kcontrol);
	int						reg = kcontrol->private_value & 0xff;
	int						reg2 = (kcontrol->private_value >> 24) & 0xff;
	int						max = (kcontrol->private_value >> 8) & 0xff;
	int						min = (kcontrol->private_value >> 16) & 0xff;
	int						mask = (1<<fls(max))-1;
	int						val, val2;

	val = snd_soc_read(codec, reg);
	val2 = snd_soc_read(codec, reg2);
	ucontrol->value.integer.value[0] = (val - min) & mask;
	ucontrol->value.integer.value[1] = (val2 - min) & mask;

    return 0;
}

/**
 * snd_soc_put_volsw_2r - double mixer set callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to set the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 */
int
cs42l52_put_volsw_2r(struct snd_kcontrol* kcontrol, struct snd_ctl_elem_value* ucontrol)
{
	struct	snd_soc_codec*	codec = snd_kcontrol_chip(kcontrol);
	int						reg = kcontrol->private_value & 0xff;
	int						reg2 = (kcontrol->private_value >> 24) & 0xff;
	int						max = (kcontrol->private_value >> 8) & 0xff;
	int						min = (kcontrol->private_value >> 16) & 0xff;
	int						mask = (1 << fls(max)) - 1;
	unsigned short			val, val2;
	int						result;

	val = (ucontrol->value.integer.value[0] + min) & mask;
	val2 = (ucontrol->value.integer.value[1] + min) & mask;

	result = snd_soc_update_bits(codec, reg, mask, val);
	if (result < 0) {
		return result;
	}

	return snd_soc_update_bits(codec, reg2, mask, val2);
}




#define SOC_SINGLE_CS42L52(xname, reg, shift, max, min) \
	{																\
		.iface			= SNDRV_CTL_ELEM_IFACE_MIXER,				\
		.name			= (xname),									\
		.info			= snd_soc_info_volsw,						\
		.get			= cs42l52_get_volsw,						\
		.put			= cs42l52_put_volsw,						\
		.private_value	=  SOC_SINGLE_VALUE(reg, shift, max, min)	\
	}

#define SOC_DOUBLE_CS42L52(xname, reg, shift_left, shift_right, max, min) \
	{													\
		.iface			= SNDRV_CTL_ELEM_IFACE_MIXER,	\
		.name			= (xname),						\
		.info			= snd_soc_info_volsw,			\
		.get			= cs42l52_get_volsw,			\
		.put			= cs42l52_put_volsw,			\
		.private_value	= (reg) | ((shift_left) << 8) | ((shift_right) << 12) | ((max) << 16) | ((min) << 24)	\
	}

/* No shifts required */
#define SOC_DOUBLE_R_CS42L52(xname, reg_left, reg_right, max, min) \
	{	\
		.iface			= SNDRV_CTL_ELEM_IFACE_MIXER,		\
		.name			= (xname),							\
		.info			= cs42l52_info_volsw_2r,			\
		.get			= cs42l52_get_volsw_2r,				\
		.put			= cs42l52_put_volsw_2r,				\
		.private_value	= (reg_left) | ((max) << 8) | ((min) << 16) | ((reg_right) << 24)	\
	}




static	int
cs42l52_enable(struct snd_soc_codec* codec)
{
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	if (cs42l52->clk_master != NULL) {
		return -EBUSY;
	}

	cs42l52->clk_master = clk_get(codec->dev, "ssi_ext1_clk");
	if (IS_ERR(cs42l52->clk_master)) {
		int		result = PTR_ERR(cs42l52->clk_master);

		cs42l52->clk_master = NULL;
		SOCERR("clk_get: %d", result);

		return result;
	}

	gpio_audio_enable(1);

	return clk_enable(cs42l52->clk_master);		/* MCLK */
}



static	void
cs42l52_disable(struct snd_soc_codec* codec)
{
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	if (cs42l52->clk_master == NULL) {
		return;
	}

	clk_disable(cs42l52->clk_master);
	clk_put(cs42l52->clk_master);
	cs42l52->clk_master = NULL;

	gpio_audio_enable(0);
}



static	void
cs42l52_sync_reg_cache(struct snd_soc_codec* codec)
{
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);
	u8							reg;

	if (cs42l52->clk_master == NULL) {
		return;
	}

	for (reg = 0; reg < SOC_CS42L52_REG_NUM; reg++) {
		if (cs42l52->reg_sync[reg] > 0) {
			u8*		cache = codec->reg_cache;
			u8		data[] = { reg, cache[reg] };

			if (codec->hw_write(codec->control_data, data, sizeof data) != sizeof data) {
				SOCERR("cs42l52_sync_reg_cache: [%02X]<-%02X", reg, cache[reg]);
				continue;
			}
		}
	}
}



static	int
cs42l52_hw_write(struct i2c_client* client, u8* data, int size)
{
	return i2c_master_send(client, data, size);
}



static	void
cs42l52_write_reg_cache(struct snd_soc_codec* codec, u_int reg, u_int val)
{
	if (reg < SOC_CS42L52_REG_NUM) {
		u8*		cache = codec->reg_cache;

		cache[reg] = val & 0xff;
	}
}



static	int
cs42l52_write(struct snd_soc_codec* codec, unsigned reg, u_int val)
{
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	if (cs42l52->clk_master != NULL) {
		u8	data[] = {
			reg & 0xff,		/* reg addr */
			val & 0xff,		/* reg val  */
		};

		if (codec->hw_write(codec->control_data, data, sizeof data) != sizeof data) {
			return -EIO;
		}
	} else {
		u8	reg_sync = cs42l52->reg_sync[reg];

		cs42l52->reg_sync[reg]++;
		if (cs42l52->reg_sync[reg] < reg_sync) {
			/* overflow */
			cs42l52->reg_sync[reg]++;
		}
	}

	cs42l52_write_reg_cache(codec, reg, val);

	return 0;
}



static	unsigned int
cs42l52_hw_read(struct snd_soc_codec* codec, unsigned int addr)
{
	u8		reg = (u8)(addr & 0xff);
	u8		data;
	int		result;

	struct	i2c_client*		client = codec->control_data;
	struct	i2c_msg			xfer[] = {
		/* write reg addr */
		{
			.addr	= client->addr,
			.flags	= client->flags,
			.len	= sizeof reg,
			.buf	= &reg,
		},
		/* read reg data */
		{
			.addr	= client->addr,
			.flags	= client->flags | I2C_M_RD,
			.len	= sizeof data,
			.buf	= &data,
		},
	};

	result = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	if (result != ARRAY_SIZE(xfer)) {
		SOCERR("i2c_transfer: %d", result);
		return 0;
	}

	return data;
}



static	int
cs42l52_read_reg_cache(struct snd_soc_codec* codec, u_int reg)
{
	const	u8*		cache = codec->reg_cache;

	return (reg < SOC_CS42L52_REG_NUM) ? cache[reg]: -EINVAL;
}



static	unsigned int
cs42l52_read(struct snd_soc_codec* codec, u_int reg)
{
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	switch (reg) {
	/* read real reg */
	case CODEC_CS42L52_SPK_STATUS:
		if (cs42l52->clk_master != NULL) {
			break;
		}
		/* fall through */

	/* read cache */
	default:
		return cs42l52_read_reg_cache(codec, reg);
	}

	return codec->hw_read(codec, reg);
}




static	const	char*	cs42l52_mic_bias[] = {
	"0.5VA",
	"0.6VA",
	"0.7VA",
	"0.8VA",
	"0.83VA",
	"0.91VA"
};

static	const	char*	cs42l52_hpf_freeze[] = {
	"Continuous DC Subtraction",
	"Frozen DC Subtraction"
};

static	const	char*	cs42l52_hpf_corner_freq[] = {
	"Normal",
	"119Hz",
	"236Hz",
	"464Hz"
};

static	const	char*	cs42l52_adc_sum[] = {
	"Normal",
	"Sum half",
	"Sub half",
	"Inverted"
};

static	const	char*	cs42l52_sig_polarity[] = {
	"Normal",
	"Inverted"
};

static	const	char*	cs42l52_spk_mono_channel[] = {
	"ChannelA",
	"ChannelB"
};

static	const	char*	cs42l52_beep_type[] = {
	"Off",
	"Single",
	"Multiple",
	"Continuous"
};

static	const	char*	cs42l52_treble_freq[] = {
	"5kHz",
	"7kHz",
	"10kHz",
	"15kHz"
};

static	const	char*	cs42l52_bass_freq[] = {
	"50Hz",
	"100Hz",
	"200Hz",
	"250Hz"
};

static	const	char*	cs42l52_target_sel[] = {
	"Apply Specific",
	"Apply All"
};

static	const	char*	cs42l52_noise_gate_delay[] = {
	"50ms",
	"100ms",
	"150ms",
	"200ms"
};

static	const	char*	cs42l52_adc_mux[] = {
	"AIN1",
	"AIN2",
	"AIN3",
	"AIN4",
	"PGA"
};

static	const	char*	cs42l52_mic_mux[] = {
	"MIC1",
	"MIC2"
};

static	const	char*	cs42l52_stereo_mux[] = {
	"Mono",
	"Stereo"
};

static	const	char*	cs42l52_off[] = {
	"On",
	"Off"
};

static	const	char*	cs42l52_hpmux[] = {
	"Off",
	"On"
};



static	const	struct	soc_enum	soc_cs42l52_enum[] = {
	SOC_ENUM_DOUBLE(	CODEC_CS42L52_ANALOG_HPF_CTL,	4, 6, 2,	cs42l52_hpf_freeze			),	/*0*/
	SOC_ENUM_SINGLE(	CODEC_CS42L52_ADC_HPF_FREQ,		   0, 4,	cs42l52_hpf_corner_freq		),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_ADC_MISC_CTL,		   4, 4,	cs42l52_adc_sum				),
	SOC_ENUM_DOUBLE(	CODEC_CS42L52_ADC_MISC_CTL,		2, 3, 2,	cs42l52_sig_polarity		),
	SOC_ENUM_DOUBLE(	CODEC_CS42L52_PB_CTL1,			2, 3, 2,	cs42l52_sig_polarity		),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_PB_CTL2,			   2, 2,	cs42l52_spk_mono_channel	),	/*5*/
	SOC_ENUM_SINGLE(	CODEC_CS42L52_BEEP_TONE_CTL,	   6, 4,	cs42l52_beep_type			),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_BEEP_TONE_CTL,	   3, 4,	cs42l52_treble_freq			),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_BEEP_TONE_CTL,	   1, 4,	cs42l52_bass_freq			),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_LIMITER_CTL2,		   6, 2,	cs42l52_target_sel			),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_NOISE_GATE_CTL,	   7, 2,	cs42l52_target_sel			),	/*10*/
	SOC_ENUM_SINGLE(	CODEC_CS42L52_NOISE_GATE_CTL,	   0, 4,	cs42l52_noise_gate_delay	),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_ADC_PGA_A,		   5, 5,	cs42l52_adc_mux				),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_ADC_PGA_B,		   5, 5,	cs42l52_adc_mux				),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_MICA_CTL,			   6, 2,	cs42l52_mic_mux				),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_MICB_CTL,			   6, 2,	cs42l52_mic_mux				),	/*15*/
	SOC_ENUM_SINGLE(	CODEC_CS42L52_MICA_CTL,			   5, 2,	cs42l52_stereo_mux			),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_MICB_CTL,			   5, 2,	cs42l52_stereo_mux			),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_IFACE_CTL2,		   0, 6,	cs42l52_mic_bias			),	/*18*/
	SOC_ENUM_SINGLE(	CODEC_CS42L52_PWCTL2,			   0, 2,	cs42l52_off					),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_MISC_CTL,			   6, 2,	cs42l52_hpmux				),
	SOC_ENUM_SINGLE(	CODEC_CS42L52_MISC_CTL,			   7, 2,	cs42l52_hpmux				),
};



static	const	struct	snd_kcontrol_new	soc_cs42l52_controls[] = {

	SOC_ENUM("Mic VA Capture Switch", soc_cs42l52_enum[18]), /*0*/
	SOC_DOUBLE("HPF Capture Switch", CODEC_CS42L52_ANALOG_HPF_CTL, 5, 7, 1, 0),
	SOC_ENUM("HPF Freeze Capture Switch", soc_cs42l52_enum[0]),

	SOC_DOUBLE("Analog SR Capture Switch", CODEC_CS42L52_ANALOG_HPF_CTL, 1, 3, 1, 1),
	SOC_DOUBLE("Analog ZC Capture Switch", CODEC_CS42L52_ANALOG_HPF_CTL, 0, 2, 1, 1),
	SOC_ENUM("HPF corner freq Capture Switch", soc_cs42l52_enum[1]), /*5*/

	SOC_SINGLE("Ganged Ctl Capture Switch", CODEC_CS42L52_ADC_MISC_CTL, 7, 1, 1), /* should be enabled init */
	SOC_ENUM("Mix/Swap Capture Switch",soc_cs42l52_enum[2]),
	SOC_ENUM("Signal Polarity Capture Switch", soc_cs42l52_enum[3]),

	SOC_SINGLE("HP Analog Gain Playback Volume", CODEC_CS42L52_PB_CTL1, 5, 7, 0),
	SOC_SINGLE("Playback B=A Volume Playback Switch", CODEC_CS42L52_PB_CTL1, 4, 1, 0), /*10*/ /*should be enabled init*/
	SOC_ENUM("PCM Signal Polarity Playback Switch",soc_cs42l52_enum[4]),

	SOC_SINGLE("Digital De-Emphasis Playback Switch", CODEC_CS42L52_MISC_CTL, 2, 1, 0),
	SOC_SINGLE("Digital SR Playback Switch", CODEC_CS42L52_MISC_CTL, 1, 1, 0),
	SOC_SINGLE("Digital ZC Playback Switch", CODEC_CS42L52_MISC_CTL, 0, 1, 0),

	SOC_SINGLE("Spk Volume Equal Playback Switch", CODEC_CS42L52_PB_CTL2, 3, 1, 0) , /*15*/ /*should be enabled init*/
	SOC_SINGLE("Spk Mute 50/50 Playback Switch", CODEC_CS42L52_PB_CTL2, 0, 1, 0),
	SOC_ENUM("Spk Swap Channel Playback Switch", soc_cs42l52_enum[5]),
	SOC_SINGLE("Spk Full-Bridge Playback Switch", CODEC_CS42L52_PB_CTL2, 1, 1, 0),
	SOC_DOUBLE_R("Mic Gain Capture Volume", CODEC_CS42L52_MICA_CTL, CODEC_CS42L52_MICB_CTL, 0, 31, 0),

	SOC_DOUBLE_R("ALC SR Capture Switch", CODEC_CS42L52_PGAA_CTL, CODEC_CS42L52_PGAB_CTL, 7, 1, 1), /*20*/
	SOC_DOUBLE_R("ALC ZC Capture Switch", CODEC_CS42L52_PGAA_CTL, CODEC_CS42L52_PGAB_CTL, 6, 1, 1),
	SOC_DOUBLE_R_CS42L52("PGA Capture Volume", CODEC_CS42L52_PGAA_CTL, CODEC_CS42L52_PGAB_CTL, 0x30, 0x18),

	SOC_DOUBLE_R_CS42L52("Passthru Playback Volume", CODEC_CS42L52_PASSTHRUA_VOL, CODEC_CS42L52_PASSTHRUB_VOL, 0x90, 0x88),
	SOC_DOUBLE("Passthru Playback Switch", CODEC_CS42L52_MISC_CTL, 4, 5, 1, 1),
	SOC_DOUBLE_R_CS42L52("ADC Capture Volume", CODEC_CS42L52_ADCA_VOL, CODEC_CS42L52_ADCB_VOL, 0x80, 0xA0),
	SOC_DOUBLE("ADC Capture Switch", CODEC_CS42L52_ADC_MISC_CTL, 0, 1, 1, 1),
	SOC_DOUBLE_R_CS42L52("ADC Mixer Capture Volume", CODEC_CS42L52_ADCA_MIXER_VOL, CODEC_CS42L52_ADCB_MIXER_VOL, 0x7f, 0x19),
	SOC_DOUBLE_R("ADC Mixer Capture Switch", CODEC_CS42L52_ADCA_MIXER_VOL, CODEC_CS42L52_ADCB_MIXER_VOL, 7, 1, 1),
	SOC_DOUBLE_R_CS42L52("PCM Mixer Playback Volume", CODEC_CS42L52_PCMA_MIXER_VOL, CODEC_CS42L52_PCMB_MIXER_VOL, 0x7f, 0x19),
	SOC_DOUBLE_R("PCM Mixer Playback Switch", CODEC_CS42L52_PCMA_MIXER_VOL, CODEC_CS42L52_PCMB_MIXER_VOL, 7, 1, 1),

	SOC_SINGLE("Beep Freq", CODEC_CS42L52_BEEP_FREQ, 4, 15, 0),
	SOC_SINGLE("Beep OnTime", CODEC_CS42L52_BEEP_FREQ, 0, 15, 0), /*30*/
	SOC_SINGLE_CS42L52("Beep Volume", CODEC_CS42L52_BEEP_VOL, 0, 0x1f, 0x07),
	SOC_SINGLE("Beep OffTime", CODEC_CS42L52_BEEP_VOL, 5, 7, 0),
	SOC_ENUM("Beep Type", soc_cs42l52_enum[6]),
	SOC_SINGLE("Beep Mix Switch", CODEC_CS42L52_BEEP_TONE_CTL, 5, 1, 1),

	SOC_ENUM("Treble Corner Freq Playback Switch", soc_cs42l52_enum[7]), /*35*/
	SOC_ENUM("Bass Corner Freq Playback Switch",soc_cs42l52_enum[8]),
	SOC_SINGLE("Tone Control Playback Switch", CODEC_CS42L52_BEEP_TONE_CTL, 0, 1, 0),
	SOC_SINGLE("Treble Gain Playback Volume", CODEC_CS42L52_TONE_CTL, 4, 15, 1),
	SOC_SINGLE("Bass Gain Playback Volume", CODEC_CS42L52_TONE_CTL, 0, 15, 1),

	SOC_DOUBLE_R_CS42L52("Master Playback Volume", CODEC_CS42L52_MASTERA_VOL, CODEC_CS42L52_MASTERB_VOL,0xe4, 0x34), /*40*/
	SOC_DOUBLE_R_CS42L52("HP Digital Playback Volume", CODEC_CS42L52_HPA_VOL, CODEC_CS42L52_HPB_VOL, 0xff, 0x1),
	SOC_DOUBLE("HP Digital Playback Switch", CODEC_CS42L52_PB_CTL2, 6, 7, 1, 1),
	SOC_DOUBLE_R_CS42L52("Speaker Playback Volume", CODEC_CS42L52_SPKA_VOL, CODEC_CS42L52_SPKB_VOL, 0xff, 0x1),
	SOC_DOUBLE("Speaker Playback Switch", CODEC_CS42L52_PB_CTL2, 4, 5, 1, 1),

	SOC_SINGLE("Limiter Max Threshold Playback Volume", CODEC_CS42L52_LIMITER_CTL1, 5, 7, 0),
	SOC_SINGLE("Limiter Cushion Threshold Playback Volume", CODEC_CS42L52_LIMITER_CTL1, 2, 7, 0),
	SOC_SINGLE("Limiter SR Playback Switch", CODEC_CS42L52_LIMITER_CTL1, 1, 1, 0), /*45*/
	SOC_SINGLE("Limiter ZC Playback Switch", CODEC_CS42L52_LIMITER_CTL1, 0, 1, 0),
	SOC_SINGLE("Limiter Playback Switch", CODEC_CS42L52_LIMITER_CTL2, 7, 1, 0),
	SOC_ENUM("Limiter Attnenuate Playback Switch", soc_cs42l52_enum[9]),
	SOC_SINGLE("Limiter Release Rate Playback Volume", CODEC_CS42L52_LIMITER_CTL2, 0, 63, 0),
	SOC_SINGLE("Limiter Attack Rate Playback Volume", CODEC_CS42L52_LIMITER_AT_RATE, 0, 63, 0), /*50*/

	SOC_DOUBLE("ALC Capture Switch",CODEC_CS42L52_ALC_CTL, 6, 7, 1, 0),
	SOC_SINGLE("ALC Attack Rate Capture Volume", CODEC_CS42L52_ALC_CTL, 0, 63, 0),
	SOC_SINGLE("ALC Release Rate Capture Volume", CODEC_CS42L52_ALC_RATE, 0, 63, 0),
	SOC_SINGLE("ALC Max Threshold Capture Volume", CODEC_CS42L52_ALC_THRESHOLD, 5, 7, 0),
	SOC_SINGLE("ALC Min Threshold Capture Volume", CODEC_CS42L52_ALC_THRESHOLD, 2, 7, 0), /*55*/

	SOC_ENUM("Noise Gate Type Capture Switch", soc_cs42l52_enum[10]),
	SOC_SINGLE("Noise Gate Capture Switch", CODEC_CS42L52_NOISE_GATE_CTL, 6, 1, 0),
	SOC_SINGLE("Noise Gate Boost Capture Switch", CODEC_CS42L52_NOISE_GATE_CTL, 5, 1, 1),
	SOC_SINGLE("Noise Gate Threshold Capture Volume", CODEC_CS42L52_NOISE_GATE_CTL, 2, 7, 0),
	SOC_ENUM("Noise Gate Delay Time Capture Switch", soc_cs42l52_enum[11]), /*60*/

	SOC_SINGLE("Batt Compensation Switch", CODEC_CS42L52_BATT_COMPEN, 7, 1, 0),
	SOC_SINGLE("Batt VP Monitor Switch", CODEC_CS42L52_BATT_COMPEN, 6, 1, 0),
	SOC_SINGLE("Batt VP ref", CODEC_CS42L52_BATT_COMPEN, 0, 0x0f, 0),
	SOC_SINGLE("Playback Charge Pump Freq", CODEC_CS42L52_CHARGE_PUMP, 4, 15, 0), /*64*/

};



static	const	struct	snd_kcontrol_new	cs42l52_adca_mux =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[12]
	);

static	const	struct	snd_kcontrol_new	cs42l52_adcb_mux =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[13]
	);

static	const	struct	snd_kcontrol_new	cs42l52_mica_mux =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[14]
	);

static	const	struct	snd_kcontrol_new	cs42l52_micb_mux =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[15]
	);

static	const	struct	snd_kcontrol_new	cs42l52_mica_stereo_mux =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[16]
	);

static	const	struct	snd_kcontrol_new	cs42l52_micb_stereo_mux =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[17]
	);

static	const	struct	snd_kcontrol_new	cs42l52_passa_switch =
	SOC_DAPM_SINGLE(
		"Switch",
		CODEC_CS42L52_MISC_CTL, 6, 1, 0
	);

static	const	struct	snd_kcontrol_new	cs42l52_passb_switch =
	SOC_DAPM_SINGLE(
		"Switch",
		CODEC_CS42L52_MISC_CTL, 7, 1, 0
	);

static	const	struct	snd_kcontrol_new	cs42l52_micbias_switch =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[19]
	);

static	const	struct	snd_kcontrol_new	cs42l52_hpa_mux =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[20]
	);

static	const	struct	snd_kcontrol_new	cs42l52_hpb_mux =
	SOC_DAPM_ENUM(
		"Route",
		soc_cs42l52_enum[21]
	);



static	const	struct	snd_soc_dapm_widget		soc_cs42l52_dapm_widgets[] = {
	/* Input path */
	SND_SOC_DAPM_ADC(	"ADC Left", "Capture",		CODEC_CS42L52_PWCTL1,	1,	1	),
	SND_SOC_DAPM_ADC(	"ADC Right", "Capture",		CODEC_CS42L52_PWCTL1,	2,	1	),

	SND_SOC_DAPM_MUX(	"MICA Mux Capture Switch",			SND_SOC_NOPM,	0,	0,	&cs42l52_mica_mux			),
	SND_SOC_DAPM_MUX(	"MICB Mux Capture Switch",			SND_SOC_NOPM,	0,	0,	&cs42l52_micb_mux			),
	SND_SOC_DAPM_MUX(	"MICA Stereo Mux Capture Switch",	SND_SOC_NOPM,	1,	0,	&cs42l52_mica_stereo_mux	),
	SND_SOC_DAPM_MUX(	"MICB Stereo Mux Capture Switch",	SND_SOC_NOPM,	2,	0,	&cs42l52_micb_stereo_mux	),

	SND_SOC_DAPM_MUX(	"ADC Mux Left Capture Switch",		SND_SOC_NOPM,	1,	1,	&cs42l52_adca_mux			),
	SND_SOC_DAPM_MUX(	"ADC Mux Right Capture Switch",		SND_SOC_NOPM,	2,	1,	&cs42l52_adcb_mux			),

	/* Sum switches */
	SND_SOC_DAPM_PGA(	"AIN1A Switch",		CODEC_CS42L52_ADC_PGA_A,	0,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"AIN2A Switch",		CODEC_CS42L52_ADC_PGA_A,	1,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"AIN3A Switch",		CODEC_CS42L52_ADC_PGA_A,	2,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"AIN4A Switch",		CODEC_CS42L52_ADC_PGA_A,	3,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"MICA Switch" ,		CODEC_CS42L52_ADC_PGA_A,	4,	0,	NULL,	0	),

	SND_SOC_DAPM_PGA(	"AIN1B Switch",		CODEC_CS42L52_ADC_PGA_B,	0,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"AIN2B Switch",		CODEC_CS42L52_ADC_PGA_B,	1,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"AIN3B Switch",		CODEC_CS42L52_ADC_PGA_B,	2,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"AIN4B Switch",		CODEC_CS42L52_ADC_PGA_B,	3,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"MICB Switch" ,		CODEC_CS42L52_ADC_PGA_B,	4,	0,	NULL,	0	),

	/* MIC PGA Power */
	SND_SOC_DAPM_PGA(	"PGA MICA",		CODEC_CS42L52_PWCTL2,	PWCTL2_PDN_MICA_SHIFT,	1,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"PGA MICB",		CODEC_CS42L52_PWCTL2,	PWCTL2_PDN_MICB_SHIFT,	1,	NULL,	0	),

	/* MIC bias switch */
	SND_SOC_DAPM_MUX(	"Mic Bias Capture Switch",	SND_SOC_NOPM,			0,	0,	&cs42l52_micbias_switch		),
	SND_SOC_DAPM_PGA(	"Mic-Bias",					CODEC_CS42L52_PWCTL2,	0,	1,	NULL,	0					),

	/* PGA Power */
	SND_SOC_DAPM_PGA(	"PGA Left",		CODEC_CS42L52_PWCTL1, PWCTL1_PDN_PGAA_SHIFT,	1,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"PGA Right",	CODEC_CS42L52_PWCTL1, PWCTL1_PDN_PGAB_SHIFT,	1,	NULL,	0	),

	/* Output path */
	SND_SOC_DAPM_MUX(	"Passthrough Left Playback Switch",		SND_SOC_NOPM,	0,	0,	&cs42l52_hpa_mux	),
	SND_SOC_DAPM_MUX(	"Passthrough Right Playback Switch",	SND_SOC_NOPM,	0,	0,	&cs42l52_hpb_mux	),

	SND_SOC_DAPM_DAC(	"DAC Left",		"Playback",		SND_SOC_NOPM,	0,	0	),
	SND_SOC_DAPM_DAC(	"DAC Right",	"Playback",		SND_SOC_NOPM,	0,	0	),

	SND_SOC_DAPM_PGA(	"HP Amp Left",		CODEC_CS42L52_PWCTL3,	5,	0,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"HP Amp Right",		CODEC_CS42L52_PWCTL3,	7,	0,	NULL,	0	),

	SND_SOC_DAPM_PGA(	"SPK Pwr Left",		CODEC_CS42L52_PWCTL3,	0,	1,	NULL,	0	),
	SND_SOC_DAPM_PGA(	"SPK Pwr Right",	CODEC_CS42L52_PWCTL3,	2,	1,	NULL,	0	),

	SND_SOC_DAPM_MICBIAS(	"MICBIAS",	SND_SOC_NOPM,	0,	0	),

	SND_SOC_DAPM_OUTPUT(	"HPA"		),
	SND_SOC_DAPM_OUTPUT(	"HPB"		),
	SND_SOC_DAPM_OUTPUT(	"SPKA"		),
	SND_SOC_DAPM_OUTPUT(	"SPKB"		),

	SND_SOC_DAPM_INPUT(		"INPUT1A"	),
	SND_SOC_DAPM_INPUT(		"INPUT2A"	),
	SND_SOC_DAPM_INPUT(		"INPUT3A"	),
	SND_SOC_DAPM_INPUT(		"INPUT4A"	),
	SND_SOC_DAPM_INPUT(		"INPUT1B"	),
	SND_SOC_DAPM_INPUT(		"INPUT2B"	),
	SND_SOC_DAPM_INPUT(		"INPUT3B"	),
	SND_SOC_DAPM_INPUT(		"INPUT4B"	),
	SND_SOC_DAPM_INPUT(		"MICA"		),
	SND_SOC_DAPM_INPUT(		"MICB"		),
};



static	const	struct	snd_soc_dapm_route	soc_cs42l52_audio_map[] = {

	/* Input map */
	/* adc select path */
	{	"ADC Mux Left Capture Switch",		"AIN1",		"INPUT1A"	},
	{	"ADC Mux Right Capture Switch",		"AIN1",		"INPUT1B"	},
	{	"ADC Mux Left Capture Switch",		"AIN2",		"INPUT2A"	},
	{	"ADC Mux Right Capture Switch",		"AIN2",		"INPUT2B"	},
	{	"ADC Mux Left Capture Switch",		"AIN3",		"INPUT3A"	},
	{	"ADC Mux Right Capture Switch",		"AIN3",		"INPUT3B"	},
	{	"ADC Mux Left Capture Switch",		"AIN4",		"INPUT4A"	},
	{	"ADC Mux Right Capture Switch",		"AIN4",		"INPUT4B"	},

	/* left capture part */
	{	"AIN1A Switch",		NULL,	"INPUT1A"			},
	{	"AIN2A Switch",		NULL,	"INPUT2A"			},
	{	"AIN3A Switch",		NULL,	"INPUT3A"			},
	{	"AIN4A Switch",		NULL,	"INPUT4A"			},
	{	"MICA Switch", 		NULL,	"MICA"				},
	{	"PGA MICA",			NULL,	"MICA Switch"		},

	{	"PGA Left",			NULL,	"AIN1A Switch"		},
	{	"PGA Left",			NULL,	"AIN2A Switch"		},
	{	"PGA Left",			NULL,	"AIN3A Switch"		},
	{	"PGA Left",			NULL,	"AIN4A Switch"		},
	{	"PGA Left",			NULL,	"PGA MICA"			},

	/* right capture part */
	{	"AIN1B Switch",		NULL,	"INPUT1B"			},
	{	"AIN2B Switch",		NULL,	"INPUT2B"			},
	{	"AIN3B Switch",		NULL,	"INPUT3B"			},
	{	"AIN4B Switch",		NULL,	"INPUT4B"			},
	{	"MICB Switch",		NULL,	"MICB"				},
	{	"PGA MICB",			NULL,	"MICB Switch"		},

	{	"PGA Right",		 NULL,	"AIN1B Switch"		},
	{	"PGA Right",		 NULL,	"AIN2B Switch"		},
	{	"PGA Right",		 NULL,	"AIN3B Switch"		},
	{	"PGA Right",		 NULL,	"AIN4B Switch"		},
	{	"PGA Right",		 NULL,	"PGA MICB"			},

	{	"ADC Mux Left Capture Switch",		"PGA",		"PGA Left"							},
    {	"ADC Mux Right Capture Switch",		"PGA",		"PGA Right"							},
	{	"ADC Left",							NULL,		"ADC Mux Left Capture Switch"		},
	{	"ADC Right",						NULL,		"ADC Mux Right Capture Switch"		},

	/* Mic Bias */
	{	"Mic Bias Capture Switch",				"On",	"PGA MICA"						},
	{	"Mic Bias Capture Switch",				"On",	"PGA MICB"						},
	{	"Mic-Bias",								NULL,	"Mic Bias Capture Switch"		},
	{	"Mic-Bias",								NULL,	"Mic Bias Capture Switch"		},
	{	"ADC Mux Left Capture Switch",			"PGA",	"Mic-Bias"						},
	{	"ADC Mux Right Capture Switch",			"PGA",	"Mic-Bias"						},
	{	"Passthrough Left Playback Switch",		"On",	"Mic-Bias"						},
	{	"Passthrough Right Playback Switch",	"On",	"Mic-Bias"						},

	/* loopback path */
	{	"Passthrough Left Playback Switch",		"On",	"PGA Left"		},
	{	"Passthrough Right Playback Switch",	"On",	"PGA Right"		},
	{	"Passthrough Left Playback Switch",		"Off",	"DAC Left"		},
	{	"Passthrough Right Playback Switch",	"Off",	"DAC Right"		},

	/* Output map */
	/* Headphone */
	{	"HP Amp Left",		NULL,	"Passthrough Left Playback Switch"		},
	{	"HP Amp Right",		NULL,	"Passthrough Right Playback Switch"		},
	{	"HPA",				NULL,	"HP Amp Left"							},
	{	"HPB",				NULL,	"HP Amp Right"							},

	/* Speakers */
	{	"SPK Pwr Left",		NULL,	"DAC Left"			},
	{	"SPK Pwr Right",	NULL,	"DAC Right"			},
	{	"SPKA",				NULL,	"SPK Pwr Left"		},
	{	"SPKB",				NULL,	"SPK Pwr Right"		},
};




static	int
cs42l52_add_controls(struct snd_soc_codec* codec)
{
	int		index;

	for (index = 0; index < ARRAY_SIZE(soc_cs42l52_controls); index++) {
		int		result;

		result = snd_ctl_add(codec->card, snd_soc_cnew(&soc_cs42l52_controls[index], codec, NULL));
		if (result < 0) {
			SOCERR("snd_ctl_add: %d\n", result);
			return result;
		}
	}

	return 0;
}



static	int
cs42l52_add_widgets(struct snd_soc_codec* soc_codec)
{
	snd_soc_dapm_new_controls(soc_codec, soc_cs42l52_dapm_widgets, ARRAY_SIZE(soc_cs42l52_dapm_widgets));
	snd_soc_dapm_add_routes(soc_codec, soc_cs42l52_audio_map, ARRAY_SIZE(soc_cs42l52_audio_map));
	snd_soc_dapm_new_widgets(soc_codec);

	return 0;
}




#define SOC_CS42L52_RATES	(	\
	SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_11025 | \
	SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
	SNDRV_PCM_RATE_96000 ) /*refer to cs42l52 datasheet page35*/

#define SOC_CS42L52_FORMATS	(	\
	SNDRV_PCM_FMTBIT_S16_LE  | SNDRV_PCM_FMTBIT_U16_LE  | \
	SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_U18_3LE | \
	SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_U20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE  | SNDRV_PCM_FMTBIT_U24_LE )




static	const	struct	cs42l52_clk_param*
cs42l52_get_clk(int mclk, int rate)
{
	int		index;

	for (index = 0; index < ARRAY_SIZE(clk_map_table); index++) {
		const	struct	cs42l52_clk_param*	clk_param = &clk_map_table[index];

		if (clk_param->rate == rate && clk_param->mclk == mclk) {
			return clk_param;
		}
	}

	return NULL;
}



static	int
cs42l52_set_format_bit(struct snd_soc_codec* codec, const struct snd_pcm_hw_params* params)
{
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	cs42l52->format &= ~IFACE_CTL1_WL_MASK;
	switch (params->msbits) {
	case 32:
		cs42l52->format |= IFACE_CTL1_WL_32BIT;
		break;

	case 24:
		cs42l52->format |= IFACE_CTL1_WL_24BIT;
		break;

	case 20:
		cs42l52->format |= IFACE_CTL1_WL_20BIT;
		break;

	case 16:
		cs42l52->format |= IFACE_CTL1_WL_16BIT;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}




static	int
cs42l52_set_bias_level(struct snd_soc_codec* codec, enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		SOCDBG("prepare");
		break;

	case SND_SOC_BIAS_ON:
		SOCDBG("on");
		break;

	case SND_SOC_BIAS_STANDBY:
		SOCDBG("standby");
		break;

	case SND_SOC_BIAS_OFF:
		SOCDBG("off");
		break;

	default:
		return -EINVAL;
	}

	codec->bias_level = level;

	return 0;
}




/* page 37 from cs42l52 datasheet (** un-official **) */

static	void
cs42l52_required_setup(struct snd_soc_codec *codec)
{
	cs42l52_write(codec, 0x00, 0x99);
	cs42l52_write(codec, 0x3e, 0xba);
	cs42l52_write(codec, 0x47, 0x80);

	cs42l52_write(
		codec,
		CODEC_CS42L52_TEM_CTL,
		cs42l52_read(codec, CODEC_CS42L52_TEM_CTL) |  (1 << 7)
	);
	cs42l52_write(
		codec,
		CODEC_CS42L52_TEM_CTL,
		cs42l52_read(codec, CODEC_CS42L52_TEM_CTL) & ~(1 << 7)
	);

	cs42l52_write(codec, 0x00, 0x00);
}



/*
 * codec private work_queue handler
 */

static	 void
cs42l52_work_handler(struct work_struct* work)
{
	struct	cs42l52_private*	cs42l52 = (struct cs42l52_private*)work;
	struct	snd_soc_codec*		codec = cs42l52->codec;

	if (cs42l52->stream != SNDRV_PCM_STREAM_PLAYBACK) {
		return;
	}

	switch (cs42l52->trigger) {
	case SNDRV_PCM_TRIGGER_START:
		/* enable codec */
		cs42l52_enable(codec);
		/* required setup */
		cs42l52_required_setup(codec);
		cs42l52_sync_reg_cache(codec);
		/* clock */
		cs42l52_write(
			codec,
			CODEC_CS42L52_CLK_CTL,
			cs42l52->clk_param->mclkdiv2
			| (cs42l52->clk_param->speed    << CLK_CTL_SPEED_SHIFT   )
			| (cs42l52->clk_param->group    << CLK_CTL_32K_SR_SHIFT  )
			| (cs42l52->clk_param->videoclk << CLK_CTL_27M_MCLK_SHIFT)
			| (cs42l52->clk_param->ratio    << CLK_CTL_RATIO_SHIFT   )
		);
		/* format */
		cs42l52_write(codec, CODEC_CS42L52_IFACE_CTL1, cs42l52->format);
		/* channels */
		cs42l52_write(
			codec,
			CODEC_CS42L52_ADC_PCM_MIXER,
			(cs42l52_read(codec, CODEC_CS42L52_ADC_PCM_MIXER) & ~(CHANNEL_MIXER_MASK << CHANNEL_MIXER_PCMB_SHIFT))
				| ((cs42l52->channels != SOC_CS42L52_DEFAULT_MAX_CHANS) ? CHANNEL_MIXER_SWAP: CHANNEL_MIXER_NOSWAP) << CHANNEL_MIXER_PCMB_SHIFT
		);
		/* digital SR/ZC */
		cs42l52_write(
			codec,
			CODEC_CS42L52_MISC_CTL,
			cs42l52_read(codec, CODEC_CS42L52_MISC_CTL) | MISC_CTL_DIGSFT
		);
		/* release output device muting */
		cs42l52_write(
			codec,
			CODEC_CS42L52_PB_CTL2,
			cs42l52_read(codec, CODEC_CS42L52_PB_CTL2) &
				~(PB_CTL2_HPB_MUTE | PB_CTL2_HPA_MUTE | PB_CTL2_SPKB_MUTE | PB_CTL2_SPKA_MUTE)
		);
		/* codec on */
		cs42l52_write(codec, CODEC_CS42L52_PWCTL1, cs42l52->pwrctl1 & ~PWCTL1_PDN_CODEC);
		/* release hard muting */
		gpio_audio_mute(0);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		/* output device muting */
		cs42l52_write(codec,
			CODEC_CS42L52_PB_CTL2,
			cs42l52_read(codec, CODEC_CS42L52_PB_CTL2)
				| PB_CTL2_HPB_MUTE  | PB_CTL2_HPA_MUTE
				| PB_CTL2_SPKB_MUTE | PB_CTL2_SPKA_MUTE
		);
		cs42l52_write(codec,
			CODEC_CS42L52_MISC_CTL,
			cs42l52_read(codec, CODEC_CS42L52_MISC_CTL) & ~(MISC_CTL_DIGSFT | MISC_CTL_DIGZC)
		);
		/* hard muting */
		gpio_audio_mute(1);
		/* codec off */
		cs42l52_write(codec, CODEC_CS42L52_PWCTL1, cs42l52->pwrctl1 | PWCTL1_PDN_CODEC);
		/* disable codec */
		cs42l52_disable(codec);
		break;
	}
}




static	int
cs42l52_pcm_startup(struct snd_pcm_substream* substream, struct snd_soc_dai* dai)
{
	struct	snd_soc_codec*		codec = dai->codec;
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	cs42l52->pwrctl1 = 0;
	cs42l52_set_bias_level(codec, SND_SOC_BIAS_PREPARE);

	return 0;
}



static	int
cs42l52_pcm_hw_params(struct snd_pcm_substream* substream, struct snd_pcm_hw_params* params, struct snd_soc_dai* dai)
{
	struct	snd_soc_codec*		codec = dai->codec;
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	cs42l52->clk_param = cs42l52_get_clk(cs42l52->sysclk, params_rate(params));
	if (cs42l52->clk_param == NULL) {
		return -EINVAL;
	}

	cs42l52->stream = substream->stream;
	cs42l52->channels = params_channels(params);
	cs42l52_set_format_bit(codec, params);

	return 0;
}



static	int
cs42l52_pcm_prepare(struct snd_pcm_substream* substream, struct snd_soc_dai* dai)
{
	static	const	u8	capture = PWCTL1_PDN_CHRG | PWCTL1_PDN_PGAB | PWCTL1_PDN_PGAA | PWCTL1_PDN_ADCB | PWCTL1_PDN_ADCA;

	struct	snd_soc_codec*			codec = dai->codec;
	struct	cs42l52_private*		cs42l52 = snd_soc_codec_get_drvdata(codec);

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		cs42l52->pwrctl1 |= capture;
		break;

	case SNDRV_PCM_STREAM_CAPTURE:
		break;

	default:
		return -EINVAL;
	}

	return 0;
}



static	int
cs42l52_pcm_hw_free(struct snd_pcm_substream* substream, struct snd_soc_dai* dai)
{
	return 0;
}



static	void
cs42l52_pcm_shutdown(struct snd_pcm_substream* substream, struct snd_soc_dai* dai)
{
}



static	int
cs42l52_pcm_trigger(struct snd_pcm_substream* substream, int trigger, struct snd_soc_dai* dai)
{
	struct	snd_soc_codec*		codec = dai->codec;
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	switch (trigger) {
	case SNDRV_PCM_TRIGGER_ENABLE:
		cs42l52->trigger = SNDRV_PCM_TRIGGER_START;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		cs42l52->trigger = SNDRV_PCM_TRIGGER_STOP;
		break;

	default:
		return 0;
	}

	schedule_work(&cs42l52->work);

	return 0;
}



static	int
cs42l52_set_sysclk(struct snd_soc_dai* dai, int clk_id, u_int freq, int dir)
{
	struct	snd_soc_codec*		codec = dai->codec;
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	if (SOC_CS42L52_MIN_CLK <= freq && freq <= SOC_CS42L52_MAX_CLK) {
		cs42l52->sysclk = freq;
		SOCDBG("sysclk %d", cs42l52->sysclk);
	} else {
		SOCERR("invalid parameter: clk_id=%d, freq=%u, dir=%d", clk_id, freq, dir);
		return -EINVAL;
	}

	return 0;
}



static	int
cs42l52_set_fmt(struct snd_soc_dai* dai, u_int fmt)
{
	struct	snd_soc_codec*		codec = dai->codec;
	struct	cs42l52_private*	cs42l52 = snd_soc_codec_get_drvdata(codec);

	cs42l52->format = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {

	case SND_SOC_DAIFMT_CBM_CFM:
		SOCDBG("codec dai fmt master");
		cs42l52->format |= IFACE_CTL1_MASTER;
		break;

	case SND_SOC_DAIFMT_CBS_CFS:
		SOCDBG("codec dai fmt slave");
		break;

	default:
		SOCDBG("invaild format");
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {

	case SND_SOC_DAIFMT_I2S:
		SOCDBG("codec dai fmt i2s");
		cs42l52->format |= IFACE_CTL1_ADC_FMT_I2S | IFACE_CTL1_DAC_FMT_I2S;
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
		SOCDBG("codec dai fmt right justified");
		cs42l52->format |= IFACE_CTL1_DAC_FMT_RIGHT_J;
		SOCINF("warning only playback stream support this format");
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		SOCDBG("codec dai fmt left justified");
		cs42l52->format |= IFACE_CTL1_ADC_FMT_LEFT_J | IFACE_CTL1_DAC_FMT_LEFT_J;
		break;

	case SND_SOC_DAIFMT_DSP_A:
		cs42l52->format |= IFACE_CTL1_DSP_MODE_EN;
		break;

	case SND_SOC_DAIFMT_DSP_B:
	default:
		SOCINF("unsupported format: %08X", fmt);
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) { /*tonyliu*/

	case SND_SOC_DAIFMT_NB_NF:
		SOCDBG("codec dai fmt normal sclk");
		break;

	case SND_SOC_DAIFMT_IB_IF:
		SOCDBG("codec dai fmt inversed sclk");
		cs42l52->format |= IFACE_CTL1_INV_SCLK;
		break;

	case SND_SOC_DAIFMT_IB_NF:
		cs42l52->format |= IFACE_CTL1_INV_SCLK;
		break;

	case SND_SOC_DAIFMT_NB_IF:
		break;

	default:
		SOCDBG("unsupported format");
		return -EINVAL;
	}

	return 0;
}



static	int
cs42l52_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct	snd_soc_codec*	codec = dai->codec;
	u8						mute_val = cs42l52_read(codec, CODEC_CS42L52_PB_CTL1) & PB_CTL1_MUTE_MASK;

	if (mute != 0) {
		mute_val |= PB_CTL1_MSTB_MUTE | PB_CTL1_MSTA_MUTE;
	}

	cs42l52_write(codec, CODEC_CS42L52_PB_CTL1, mute_val);

	return 0;
}




struct	snd_soc_dai_ops		soc_cs42l52_dai_ops = {
	.startup		= cs42l52_pcm_startup,
	.hw_params		= cs42l52_pcm_hw_params,
	.prepare		= cs42l52_pcm_prepare,
	.hw_free		= cs42l52_pcm_hw_free,
	.shutdown		= cs42l52_pcm_shutdown,

	.trigger		= cs42l52_pcm_trigger,

	.set_sysclk		= cs42l52_set_sysclk,
	.set_fmt		= cs42l52_set_fmt,
	.digital_mute	= cs42l52_digital_mute,
};



struct	snd_soc_dai		soc_cs42l52_dai = {
	.name = SOC_CS42L52_NAME " dai",
	.playback = {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= SOC_CS42L52_DEFAULT_MAX_CHANS,
		.rates			= SOC_CS42L52_RATES,
		.formats		= SOC_CS42L52_FORMATS,
	},
	.capture = {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= SOC_CS42L52_DEFAULT_MAX_CHANS,
		.rates			= SOC_CS42L52_RATES,
		.formats		= SOC_CS42L52_FORMATS,
	},
	.ops = &soc_cs42l52_dai_ops,
};
EXPORT_SYMBOL_GPL(soc_cs42l52_dai);




static	int
cs42l52_setup_codec(struct snd_soc_codec* codec)
{
	u8		reg;
	u8*		reg_cache = codec->reg_cache;
	int		result = codec->hw_read(codec, CODEC_CS42L52_CHIP);

	if ((result & CHIP_ID_MASK) != CHIP_ID) {
		return -ENODEV;
	}

	/* build reg cache */
	for (reg = 0; reg < SOC_CS42L52_REG_NUM; reg++) {
		reg_cache[reg] = codec->hw_read(codec, reg);
	}

	/* chip id & revision */
	result &= CHIP_REV_MASK;
	SOCINF("CODEC revision %c%d", 'A' + (result >> 1), result & 1);

	return 0;
}



static	int
cs42l52_i2c_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct	snd_soc_codec*		codec = (struct snd_soc_codec*)id->driver_data;
	struct	cs42l52_private*	cs42l52;
	int							result;

	/* setup private codec */
	cs42l52 = kzalloc(sizeof(struct cs42l52_private), GFP_KERNEL);
	if (cs42l52 == NULL) {
		SOCERR("kzalloc: cs42l52_private");
		result = -ENOMEM;
		goto error_cs42l52_i2c_probe;
	}
	INIT_WORK(&cs42l52->work, cs42l52_work_handler);
	cs42l52->codec = codec;
	/* setup codec */
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	codec->reg_cache = cs42l52->reg_cache;
	snd_soc_codec_set_drvdata(codec, cs42l52);

	i2c_set_clientdata(client, codec);
	codec->dev = &client->dev;
	codec->control_data = client;

	cs42l52_enable(codec);
	result = cs42l52_setup_codec(codec);
	cs42l52_disable(codec);
	if (result != 0) {
		SOCERR("cs42l52_setup_codec: %d", result);
		goto error_cs42l52_i2c_probe;
	}

	/* register */
	result = snd_soc_register_codec(codec);
	if (result != 0) {
		SOCERR("snd_soc_register_codec: %d", result);
		goto error_cs42l52_i2c_probe;
	}
	soc_cs42l52_dai.dev = &client->dev;
	result = snd_soc_register_dai(&soc_cs42l52_dai);
	if (result != 0) {
		SOCERR("snd_soc_register_dai: %d", result);
		goto error_cs42l52_i2c_probe;
	}

	return 0;


error_cs42l52_i2c_probe:
	if (cs42l52 != NULL) {
		kfree(cs42l52);
	}

	return result;
}



static	int
cs42l52_i2c_remove(struct i2c_client* client)
{
	struct	snd_soc_codec*		codec;
	struct	cs42l52_private*	cs42l52;

	if (client == NULL) {
		return -ENODEV;
	}

	codec = i2c_get_clientdata(client);
	if (codec == NULL) {
		return 0;
	}

	snd_soc_unregister_dai(&soc_cs42l52_dai);
	snd_soc_unregister_codec(codec);

	cs42l52 = snd_soc_codec_get_drvdata(codec);
	kfree(cs42l52);

	return 0;
}



static	struct	snd_soc_codec	cs42l52_codec = {
	.name				= SOC_CS42L52_NAME " codec",
	.owner				= THIS_MODULE,
	.write				= cs42l52_write,
	.read				= cs42l52_read,
	.set_bias_level		= cs42l52_set_bias_level,
	.dai				= &soc_cs42l52_dai,
	.bias_level			= SND_SOC_BIAS_OFF,
	.num_dai			= 1,
	.hw_write			= (hw_write_t)cs42l52_hw_write,
	.hw_read			= cs42l52_hw_read,
	.reg_cache_size		= SOC_CS42L52_REG_NUM,
};



static	const	struct	i2c_device_id cs42l52_i2c_id[] = {
	{ "cs42l52-i2c", (kernel_ulong_t)&cs42l52_codec },		/* i2c_board_info table key-name */
	{ }
};
MODULE_DEVICE_TABLE(i2c, cs42l52_i2c_id);



static	struct	i2c_driver	cs42l52_i2c_drv = {
	.driver = {
		.name	= SOC_CS42L52_NAME " i2c",
		.owner	= THIS_MODULE,
	},
	.probe		= cs42l52_i2c_probe,
	.remove		= cs42l52_i2c_remove,
	.id_table	= cs42l52_i2c_id,
};




static	int
cs42l52_probe(struct platform_device* pdev)
{
	struct	snd_soc_device*		socdev = platform_get_drvdata(pdev);
	int							result;

	/* initialize sound-card */
	socdev->card->codec = &cs42l52_codec;
	result = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (result != 0) {
		SOCERR("snd_soc_new_pcms: %d", result);
		goto error_cs42l52_probe;
	}
	cs42l52_add_controls(socdev->card->codec);
	cs42l52_add_widgets(socdev->card->codec);

	return 0;


error_cs42l52_probe:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	return result;
}



static	int
cs42l52_remove(struct platform_device* pdev)
{
	struct	snd_soc_device*		socdev = platform_get_drvdata(pdev);
	struct	snd_soc_codec*		codec = socdev->card->codec;

	if (codec->control_data) {
		cs42l52_set_bias_level(codec, SND_SOC_BIAS_OFF);
	}

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	return 0;
}



static	int
cs42l52_suspend(struct platform_device* pdev, pm_message_t state)
{
	struct	snd_soc_device*		socdev = platform_get_drvdata(pdev);
	struct	snd_soc_codec*		codec = socdev->card->codec;

	cs42l52_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}



static	int
cs42l52_resume(struct platform_device* pdev)
{
	struct	snd_soc_device*		socdev = platform_get_drvdata(pdev);
	struct	snd_soc_codec*		codec = socdev->card->codec;

	/* charge cs42l52 codec */
	if (codec->suspend_bias_level == SND_SOC_BIAS_ON) {
		cs42l52_set_bias_level(codec, SND_SOC_BIAS_PREPARE);
		codec->bias_level = SND_SOC_BIAS_ON;
	}

	return 0;
}



struct	snd_soc_codec_device	soc_codec_cs42l52_device = {
	.probe		= cs42l52_probe,
	.remove		= cs42l52_remove,
	.suspend	= cs42l52_suspend,
	.resume		= cs42l52_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_cs42l52_device);




static	int		__init
cs42l52_init(void)
{
	return i2c_add_driver(&cs42l52_i2c_drv);
}
module_init(cs42l52_init);



static	void	__exit
cs42l52_exit(void)
{
	i2c_del_driver(&cs42l52_i2c_drv);
}
module_exit(cs42l52_exit);




MODULE_DESCRIPTION("ALSA SoC " SOC_CS42L52_NAME " Codec");
MODULE_AUTHOR("Sony Corporation.");
MODULE_LICENSE("GPL");

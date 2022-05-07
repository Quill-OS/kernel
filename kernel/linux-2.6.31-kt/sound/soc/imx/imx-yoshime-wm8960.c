/*
 * imx-yoshime-wm8960.c  --  SoC 5.1 audio for imx
 *
 * Copyright 2011 Amazon.com, Inc. All rights reserved.
 * Author: Manish Lachwani <lachwani@amazon.com>
 * 		   Vidhyananth Venkatasamy <venkatas@lab126.com>
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include <mach/hardware.h>
#include <mach/clock.h>
#include <mach/dam.h>

#include "imx-pcm.h"
#include "imx-ssi.h"
#include "../codecs/wm8960.h"

extern int wm8960_capture;
void gpio_audio_external_micbias(int enable);

struct imx_yoshi_pcm_state {
	int lr_clk_active;
};

static struct imx_yoshi_pcm_state clk_state;
extern int mx50_audio_playing_flag;

static int imx_yoshi_startup(struct snd_pcm_substream *substream)
{
	clk_state.lr_clk_active++;

	return 0;
}

static void imx_yoshi_shutdown(struct snd_pcm_substream *substream)
{
	clk_state.lr_clk_active--;
}

static int imx_yoshi_surround_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *pcm_link = rtd->dai;
	struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
	struct snd_soc_dai *codec_dai = pcm_link->codec_dai;
	u32 codec_dai_format, cpu_dai_format;
	struct imx_ssi *ssi_mode = (struct imx_ssi *)cpu_dai->private_data;
	unsigned int pll_out = 0, bclk = 0, fmt = 0;
	int ret = 0;
	unsigned int sysclk_div = 0, pll_prescale = 0, dacdiv = 0, adcdiv = 0;
	unsigned int channels = params_channels(params);
	int pll_id = 0;

	/*
	 * The WM8960 is better at generating accurate audio clocks than the
	 * MX35 SSI controller, so we will use it as master when we can.
	 */
	switch (params_rate(params)) {
	case 8000:
	case 16000:
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		bclk = WM8960_BCLK_DIV_8;
		pll_out = 11289600;
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		dacdiv = 0x28;
		adcdiv = 0x140;
		pll_id = 0;
		break;
	case 48000:
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		pll_out = 12288000;
		if (channels == 1) {
			/* Mono mode */
			dacdiv = 0x10;	/* 24.0 KHz */
			bclk = WM8960_BCLK_DIV_8;
		}
		else {	
			/* Stereo Mode */
			dacdiv = 0x0;	/* 48.0 KHz */
			bclk = WM8960_BCLK_DIV_4;
		}
		pll_id = 1;
		break;
	case 32000:
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		bclk = WM8960_BCLK_DIV_8;
		pll_out = 11289600;
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		dacdiv = 0x0;
		break;
	case 11025:
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		bclk = WM8960_BCLK_DIV_8;
		pll_out = 11289600;
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		dacdiv = 0x20;
		break;
	case 22050:
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		if (channels == 1) {
			/* Mono Mode */
			bclk = WM8960_BCLK_DIV_16;
			dacdiv = 0x20;	/* 11.025 KHz */
		}
		else {
			/* Stereo Mode */
			bclk = WM8960_BCLK_DIV_8;
			dacdiv = 0x10;	/* 22.05 KHz */
		}
		pll_out = 11289600;
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		break;
	case 44100:
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		if (channels == 1) {
			/* Mono mode */
			dacdiv = 0x10;	/* 22.05 KHz */
			bclk = WM8960_BCLK_DIV_16;
		}
		else {	
			/* Stereo Mode */
			dacdiv = 0x0;	/* 44.1 KHz */
			bclk = WM8960_BCLK_DIV_8;
		}
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		pll_out = 11289600;
		break;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		wm8960_capture = 0;
	else
		wm8960_capture = 1;

	codec_dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | fmt;
	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, codec_dai_format);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		cpu_dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_IF | fmt;
		ssi_mode->sync_mode = 0;		
	} else {
		cpu_dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | fmt; 	
		ssi_mode->sync_mode = 1;	
	}
	ssi_mode->network_mode = 0;	

	ret = snd_soc_dai_set_fmt(cpu_dai, cpu_dai_format);
	if (ret < 0)
		return ret;

	/* No clockout from IMX SSI since it is slave */
	ret = snd_soc_dai_set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_SYSCLKSEL, 1);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_SYSCLKDIV, sysclk_div);
	if (ret < 0)
		return ret;	

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_PRESCALEDIV, pll_prescale);
	if (ret < 0)
		return ret;
	
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_DACDIV, dacdiv);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_ADCDIV, adcdiv);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_BCLKDIV, bclk);
	if (ret < 0)
		return ret;

	/* 24MHz CLKO coming from MX50 */
	ret = snd_soc_dai_set_pll(codec_dai, pll_id, 24000000, pll_out);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops imx_yoshi_surround_ops = {
	.startup = imx_yoshi_startup,
	.shutdown = imx_yoshi_shutdown,
	.hw_params = imx_yoshi_surround_hw_params,
};

static int mic_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)
{
	/* Logic level output on GPIO enables/disables external MICBIAS regulator */
	if (SND_SOC_DAPM_EVENT_ON(event))
		gpio_audio_external_micbias(1);
	else
		gpio_audio_external_micbias(0);
	
	return 0;
}

static const struct snd_soc_dapm_widget imx_yoshi_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Speakers", NULL),
	SND_SOC_DAPM_HP("Headset HP", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", mic_event),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* mic is connected to mic1 - with bias */
	{"Headset HP", NULL, "HP_L"},
	{"Headset HP", NULL, "HP_R"},
	{"Speakers", NULL, "SPK_LN"},
	{"Speakers", NULL, "SPK_LP"},
	{"Speakers", NULL, "SPK_RN"},
	{"Speakers", NULL, "SPK_RP"},
	{"RINPUT2", NULL, "Headset Mic"},
	{"LINPUT2", NULL, "Headset Mic"},
	{"LINPUT1", NULL, "Internal Mic"},
};

static struct snd_soc_jack headset;

static struct snd_soc_jack_pin headset_pins[] = {
	{ .pin = "Headset Mic", .mask = SND_JACK_MICROPHONE, .invert = false },
	{ .pin = "Internal Mic", .mask = SND_JACK_MICROPHONE, .invert = true },

	{ .pin = "Headset HP", .mask = SND_JACK_MICROPHONE, .invert = false },
	{ .pin = "Speakers", .mask = SND_JACK_MICROPHONE, .invert = true },
};

/*
 * DAM/AUDMUX setting
 *
 * SSI1 (FIFO 0) is connected to PORT #1
 * WM8962 Codec is connected to PORT #4
 */
static void imx_yoshi_dam_init(void)
{
	int source_port = port_2; /* ssi */
	int target_port = port_3; /* dai */

	dam_reset_register(source_port);
	dam_reset_register(target_port);

	dam_select_mode(source_port, normal_mode);
	dam_select_mode(target_port, normal_mode);

	dam_set_synchronous(source_port, true);
	dam_set_synchronous(target_port, true);

	dam_select_RxD_source(source_port, target_port);
	dam_select_RxD_source(target_port, source_port);

	dam_select_TxFS_direction(source_port, signal_out);
	dam_select_TxFS_source(source_port, false, target_port);

	dam_select_TxClk_direction(source_port, signal_out);
	dam_select_TxClk_source(source_port, false, target_port);

	dam_select_RxFS_direction(source_port, signal_out);
	dam_select_RxFS_source(source_port, false, target_port);

	dam_select_RxClk_direction(source_port, signal_out);
	dam_select_RxClk_source(source_port, false, target_port);
}

static int imx_wm8960_init(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, imx_yoshi_dapm_widgets,
				  ARRAY_SIZE(imx_yoshi_dapm_widgets));
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_jack_new(codec->socdev->card, "Headset",
		         SND_JACK_MICROPHONE | SND_JACK_BTN_0, &headset);

	snd_soc_jack_add_pins(&headset, ARRAY_SIZE(headset_pins),
			      headset_pins);
	snd_soc_dapm_sync(codec);

	/* Configure the AUDMUX */
	imx_yoshi_dam_init();

	return 0;
}

static struct snd_soc_dai_link imx_yoshi_dai = {
	.name = "wm8960",
	.stream_name = "wm8960",
	.init = imx_wm8960_init,
	.ops = &imx_yoshi_surround_ops,
};

static int imx_yoshi_card_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	kfree(socdev->codec_data);
	return 0;
}

static struct snd_soc_card snd_soc_card_imx_yoshi = {
	.name = "imx-yoshime",
	.platform = &imx_soc_platform,
	.dai_link = &imx_yoshi_dai,
	.num_links = 1,
	.remove = imx_yoshi_card_remove,
};

static struct snd_soc_device imx_yoshi_snd_devdata = {
	.card = &snd_soc_card_imx_yoshi,
	.codec_dev = &soc_codec_dev_wm8960,
};

/*
 * This function will register the snd_soc_pcm_link drivers.
 */
static int __devinit imx_wm8960_probe(struct platform_device *pdev)
{
	imx_yoshi_dai.cpu_dai = imx_ssi_dai[2];		
	imx_yoshi_dai.codec_dai = &wm8960_dai;

	return 0;
}

static int __devexit imx_wm8960_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver imx_wm8960_driver = {
	.probe = imx_wm8960_probe,
	.remove = __devexit_p(imx_wm8960_remove),
	.driver = {
		   .name = "imx-yoshime-wm8960",
		   .owner = THIS_MODULE,
		   },
};

static struct platform_device *imx_yoshi_snd_device;

static int __init imx_yoshi_asoc_init(void)
{
	int ret;

	ret = platform_driver_register(&imx_wm8960_driver);
	if (ret < 0)
		goto exit;
	imx_yoshi_snd_device = platform_device_alloc("soc-audio", 1);
	if (!imx_yoshi_snd_device)
		goto err_device_alloc;
	platform_set_drvdata(imx_yoshi_snd_device, &imx_yoshi_snd_devdata);
	imx_yoshi_snd_devdata.dev = &imx_yoshi_snd_device->dev;
	ret = platform_device_add(imx_yoshi_snd_device);
	if (0 == ret)
		goto exit;

	platform_device_put(imx_yoshi_snd_device);
err_device_alloc:
	platform_driver_unregister(&imx_wm8960_driver);
exit:
	return ret;
}

static void __exit imx_yoshi_asoc_exit(void)
{
	platform_driver_unregister(&imx_wm8960_driver);
	platform_device_unregister(imx_yoshi_snd_device);
}

module_init(imx_yoshi_asoc_init);
module_exit(imx_yoshi_asoc_exit);

/* Module information */
MODULE_DESCRIPTION("Manish Lachwani <lachwani@amazon.com>");
MODULE_LICENSE("GPL");

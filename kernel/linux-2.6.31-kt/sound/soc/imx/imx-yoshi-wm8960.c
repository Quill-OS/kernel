/*
 * imx-yoshi-wm8962.c  --  SoC 5.1 audio for imx_yoshi
 *
 * Copyright 2010 Amazon.com, Inc. All rights reserved.
 * Author: Manish Lachwani <lachwani@amazon.com>
 */

/*
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
#include "../codecs/wm8962.h"

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

static int fll_shutdown_ev(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol,
		 	   int event)
{
	struct snd_soc_codec *codec;
	struct snd_soc_dai *codec_dai;
 
	if (!w) {
		pr_err("No widget!\n");
		return 0;
	}

 	codec = w->codec;
	if (!codec)
		return 0;

	codec_dai = codec->dai;

	if (clk_state.lr_clk_active)
		return 0;

	/* Power down the FLL if audio is inactive */
	snd_soc_dai_set_sysclk(codec_dai, WM8962_SYSCLK_MCLK,
				4000000, SND_SOC_CLOCK_IN);
	snd_soc_dai_set_pll(codec_dai, 0, 0, 0);

	mx50_audio_playing_flag = 0;

	return 0;
}

static int imx_yoshi_surround_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *pcm_link = rtd->dai;
	struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
	struct snd_soc_dai *codec_dai = pcm_link->codec_dai;
	unsigned int rate = params_rate(params);
	u32 codec_dai_format, cpu_dai_format;
	unsigned int pll_out = 0, lrclk_ratio = 0;
	unsigned int mult = 256;
	struct imx_ssi *ssi_mode = (struct imx_ssi *)cpu_dai->private_data;

	if (clk_state.lr_clk_active > 1)
		return 0;

	switch (rate) {
	case 8000:
		lrclk_ratio = 5;
		pll_out = 6144000;
		break;
	case 11025:
		lrclk_ratio = 4;
		pll_out = 5644800;
		break;
	case 16000:
		lrclk_ratio = 3;
		pll_out = 6144000;
		break;
	case 32000:
		lrclk_ratio = 3;
		pll_out = 12288000;
		break;
	case 48000:
		lrclk_ratio = 2;
		pll_out = 12288000;
		break;
	case 64000:
		lrclk_ratio = 1;
		pll_out = 12288000;
		break;
	case 96000:
		lrclk_ratio = 2;
		pll_out = 24576000;
		break;
	case 128000:
		lrclk_ratio = 1;
		pll_out = 24576000;
		break;
	case 22050:
		lrclk_ratio = 4;
		pll_out = 11289620;
		break;
	case 44100:
		lrclk_ratio = 2;
		pll_out = 11289620;
		break;
	case 88200:
		lrclk_ratio = 0;
		pll_out = 11289620;
		break;
	case 176400:
		lrclk_ratio = 0;
		pll_out = 22579200;
		break;
	case 192000:
		lrclk_ratio = 0;
		pll_out = 24576000;
		break;
	default:
		pr_info("Rate not support.\n");
		return -EINVAL;;
	}

	codec_dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		cpu_dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_NB_IF;
		ssi_mode->sync_mode = 0;
	} else {
		ssi_mode->sync_mode = 1;
		cpu_dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM;
	}

	ssi_mode->network_mode = 0;

	snd_soc_dai_set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0, SND_SOC_CLOCK_IN);

	/* Set the CODEC FLL to provide a 256fs/512fs SYSCLK, taking care to 
	 * clock off MCLK directly while we do in order to avoid having
	 * SYSCLK enabled with no input clock.
   */
	if (rate < 44100)
		mult = 512;
	else
		mult = 256;
  
	snd_soc_dai_set_sysclk(codec_dai, WM8962_SYSCLK_MCLK,
				4000000, SND_SOC_CLOCK_IN);
	snd_soc_dai_set_pll(codec_dai, WM8962_FLL_MCLK, 4000000, params_rate(params) * mult);
	snd_soc_dai_set_sysclk(codec_dai, WM8962_SYSCLK_FLL,
				params_rate(params) * mult, SND_SOC_CLOCK_OUT);

	/* Fix: I2S master is enabled after setting sysclk 
	 * 		to avoid any hardware related timing issue (PH-14411)
	 */ 
	/* set codec DAI configuration */
	snd_soc_dai_set_fmt(codec_dai, codec_dai_format);

	/* set cpu DAI configuration */
	snd_soc_dai_set_fmt(cpu_dai, cpu_dai_format);

	mx50_audio_playing_flag = 1;

	return 0;
}

static struct snd_soc_ops imx_yoshi_surround_ops = {
	.startup = imx_yoshi_startup,
	.shutdown = imx_yoshi_shutdown,
	.hw_params = imx_yoshi_surround_hw_params,
};

static int mic_ev(struct snd_soc_dapm_widget *w,
		  struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = w->codec;

	/* Logic level output on GPIO3 enables/disables bias */
	if (SND_SOC_DAPM_EVENT_ON(event))
		snd_soc_write(codec, WM8962_GPIO_3, 0x41);
	else
		snd_soc_write(codec, WM8962_GPIO_3, 0x01);

	return 0;
}

static const struct snd_soc_dapm_widget imx_yoshi_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Speakers", NULL),
	SND_SOC_DAPM_HP("Headset HP", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", mic_ev),
	SND_SOC_DAPM_POST("FLL shutdown", fll_shutdown_ev),
};

static const struct snd_soc_dapm_route audio_map[] = {
	{ "Headset HP", NULL, "HPOUTL" },
	{ "Headset HP", NULL, "HPOUTR" },
	{ "IN2L", NULL, "Headset Mic" },
	{ "IN2R", NULL, "Headset Mic" },

	{ "IN1L", NULL, "Internal Mic" },

	{ "Speakers", NULL, "SPKOUTL" },
	{ "Speakers", NULL, "SPKOUTR" },
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

static int imx_yoshi_wm8962_init(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, imx_yoshi_dapm_widgets,
				  ARRAY_SIZE(imx_yoshi_dapm_widgets));

	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_jack_new(codec->socdev->card, "Headset",
		         SND_JACK_MICROPHONE | SND_JACK_BTN_0,
			 &headset);

	snd_soc_jack_add_pins(&headset, ARRAY_SIZE(headset_pins),
			      headset_pins);

	wm8962_mic_detect(codec, &headset);

	snd_soc_dapm_sync(codec);

	/* Configure the AUDMUX */
	imx_yoshi_dam_init();

	return 0;
}

static struct snd_soc_dai_link imx_yoshi_dai = {
	.name = "wm8962",
	.stream_name = "wm8962",
	.init = imx_yoshi_wm8962_init,
	.ops = &imx_yoshi_surround_ops,
};

static int imx_yoshi_card_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	kfree(socdev->codec_data);
	return 0;
}

static struct snd_soc_card snd_soc_card_imx_yoshi = {
	.name = "imx-yoshi",
	.platform = &imx_soc_platform,
	.dai_link = &imx_yoshi_dai,
	.num_links = 1,
	.remove = imx_yoshi_card_remove,
};

static struct snd_soc_device imx_yoshi_snd_devdata = {
	.card = &snd_soc_card_imx_yoshi,
	.codec_dev = &soc_codec_dev_wm8962,
};

/*
 * This function will register the snd_soc_pcm_link drivers.
 */
static int __devinit imx_yoshi_wm8962_probe(struct platform_device *pdev)
{
	imx_yoshi_dai.cpu_dai = imx_ssi_dai[2];
	imx_yoshi_dai.codec_dai = &wm8962_dai;

	return 0;
}

static int __devexit imx_yoshi_wm8962_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver imx_yoshi_wm8962_driver = {
	.probe = imx_yoshi_wm8962_probe,
	.remove = __devexit_p(imx_yoshi_wm8962_remove),
	.driver = {
		   .name = "imx-yoshi-wm8962",
		   .owner = THIS_MODULE,
		   },
};

static struct platform_device *imx_yoshi_snd_device;

static int __init imx_yoshi_asoc_init(void)
{
	int ret;

	ret = platform_driver_register(&imx_yoshi_wm8962_driver);
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
	platform_driver_unregister(&imx_yoshi_wm8962_driver);
      exit:
	return ret;
}

static void __exit imx_yoshi_asoc_exit(void)
{
	platform_driver_unregister(&imx_yoshi_wm8962_driver);
	platform_device_unregister(imx_yoshi_snd_device);
}

module_init(imx_yoshi_asoc_init);
module_exit(imx_yoshi_asoc_exit);

/* Module information */
MODULE_DESCRIPTION("Manish Lachwani <lachwani@amazon.com>");
MODULE_LICENSE("GPL");

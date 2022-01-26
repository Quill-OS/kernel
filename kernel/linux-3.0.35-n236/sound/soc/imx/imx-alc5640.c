/*
 * imx-alc5640.c
 *
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc. All Rights Reserved.
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
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/fsl_devices.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/switch.h>
#include <linux/kthread.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/jack.h>
#include <mach/dma.h>
#include <mach/clock.h>
#include <mach/audmux.h>
#include <mach/gpio.h>
#include <asm/mach-types.h>

#include "imx-ssi.h"
#include "../codecs/rt5640.h"

struct imx_priv {
	int sysclk;         /*mclk from the outside*/
	int codec_sysclk;
	int dai_aif1;
	int hp_irq;
	int hp_status;
	struct platform_device *pdev;
	struct switch_dev sdev;
	struct snd_pcm_substream *first_stream;
	struct snd_pcm_substream *second_stream;
	int (*amp_enable) (int enable);
};
static unsigned int sample_format = SNDRV_PCM_FMTBIT_S16_LE;
static struct imx_priv card_priv;
static struct snd_soc_card snd_soc_card_imx;
static struct snd_soc_codec *gcodec;

static int imx_aif1_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct imx_priv *priv = &card_priv;
	struct mxc_audio_platform_data *plat = priv->pdev->dev.platform_data;

	if (!codec_dai->active)
		plat->clock_enable(1);

	if (card_priv.amp_enable) 
		card_priv.amp_enable(1);

	return 0;
}

static void imx_aif1_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct imx_priv *priv = &card_priv;
	struct mxc_audio_platform_data *plat = priv->pdev->dev.platform_data;

	snd_soc_dai_digital_mute(codec_dai, 1);
	if (!codec_dai->active)
		plat->clock_enable(0);

	return;
}

static int imx_aif1_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct imx_priv *priv = &card_priv;
	unsigned int channels = params_channels(params);
	unsigned int sample_rate = 44100;
	int ret = 0;
	u32 dai_format;
	unsigned int pll_out;

	if (!priv->first_stream)
		priv->first_stream = substream;
	else
		priv->second_stream = substream;

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBM_CFM;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, dai_format);
	if (ret < 0)
		return ret;

	/* set i.MX active slot mask */
	snd_soc_dai_set_tdm_slot(cpu_dai,
				 channels == 1 ? 0xfffffffe : 0xfffffffc,
				 channels == 1 ? 0xfffffffe : 0xfffffffc,
				 2, 32);

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_IF |
		SND_SOC_DAIFMT_CBM_CFM;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, dai_format);
	if (ret < 0)
		return ret;

	sample_rate = params_rate(params);
	sample_format = params_format(params);

	if (sample_format == SNDRV_PCM_FORMAT_S24_LE)
		pll_out = sample_rate * 192;
	else
		pll_out = sample_rate * 256 * 2;

	ret = snd_soc_dai_set_pll(codec_dai, RT5640_PLL1_S_MCLK,
				  RT5640_PLL1_S_MCLK, priv->sysclk,
				  pll_out);
	if (ret < 0)
		pr_err("Failed to start FLL: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5640_SCLK_S_PLL1, pll_out, 0);
	if (ret < 0) {
		pr_err("Failed to set SYSCLK: %d\n", ret);
		return ret;
	}
	
	return 0;
}

static const struct snd_soc_dapm_widget alc5640_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	//SND_SOC_DAPM_SPK("Speaker", NULL),
};

static const struct snd_soc_dapm_route alc5640_route[] = {
	{ "Headphone Jack",	NULL,	"HPOL" },
	{ "Headphone Jack",	NULL,	"HPOR" },
#if 0
	{"Speaker",		NULL,	"SPOLP"},
	{"Speaker",		NULL,	"SPOLN"},
	{"Speaker",		NULL,	"SPORP"},
	{"Speaker",		NULL,	"SPORN"},
#endif
};

static int imx_alc5640_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	printk ("[%s-%d] ...\n", __func__, __LINE__);
	snd_soc_dapm_new_controls(dapm, alc5640_dapm_widgets,
				ARRAY_SIZE(alc5640_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, alc5640_route, ARRAY_SIZE(alc5640_route));

	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
//	snd_soc_dapm_enable_pin(dapm, "Speaker");

	snd_soc_dapm_sync(dapm);

	if (card_priv.amp_enable)
		card_priv.amp_enable(1);
	return 0;
}

static struct snd_soc_ops imx_aif1_ops = {
	.hw_params = imx_aif1_hw_params,
	.startup = imx_aif1_startup,
	.shutdown = imx_aif1_shutdown,
};

static struct snd_soc_dai_link imx_dai[] = {
	{
		.name = "ALC5640",
		.stream_name = "ALC5640 AIF1",
		.codec_dai_name	= "rt5640-aif1",
		.codec_name	= "rt5640.0-001c",
		.cpu_dai_name	= "imx-ssi.1",
		.platform_name	= "imx-pcm-audio.1",
		.init		= imx_alc5640_init,
		.ops		= &imx_aif1_ops,
	},
};

static struct snd_soc_card snd_soc_card_imx = {
	.name		= "alc5640-audio",
	.dai_link	= imx_dai,
	.num_links	= ARRAY_SIZE(imx_dai),
};

static int imx_audmux_config(int slave, int master)
{
	unsigned int ptcr, pdcr;
	slave = slave - 1;
	master = master - 1;

	ptcr = MXC_AUDMUX_V2_PTCR_SYN |
		MXC_AUDMUX_V2_PTCR_TFSDIR |
		MXC_AUDMUX_V2_PTCR_TFSEL(master) |
		MXC_AUDMUX_V2_PTCR_TCLKDIR |
		MXC_AUDMUX_V2_PTCR_TCSEL(master);
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(master);
	mxc_audmux_v2_configure_port(slave, ptcr, pdcr);

	ptcr = MXC_AUDMUX_V2_PTCR_SYN;
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(slave);
	mxc_audmux_v2_configure_port(master, ptcr, pdcr);

	return 0;
}

/*
 * This function will register the snd_soc_pcm_link drivers.
 */
static int __devinit imx_alc5640_probe(struct platform_device *pdev)
{

	struct mxc_audio_platform_data *plat = pdev->dev.platform_data;
	struct imx_priv *priv = &card_priv;
	int ret = 0;

	priv->pdev = pdev;

	imx_audmux_config(plat->src_port, plat->ext_port);

	if (plat->init && plat->init()) {
		ret = -EINVAL;
		return ret;
	}

	priv->sysclk = plat->sysclk;

#ifdef CONFIG_ANDROID
	priv->sdev.name = "h2w";
	ret = switch_dev_register(&priv->sdev);
	if (ret < 0) {
		ret = -EINVAL;
		return ret;
	}

	if (plat->hp_gpio != -1) {
		priv->hp_status = gpio_get_value(plat->hp_gpio);
		if (priv->hp_status != plat->hp_active_low)
			switch_set_state(&priv->sdev, 2);
		else
			switch_set_state(&priv->sdev, 0);
	}
	else {
		priv->hp_status = plat->hp_active_low;
		switch_set_state(&priv->sdev, 0);
	}
#endif	
	priv->amp_enable = plat->amp_enable;
	if (priv->amp_enable)
		priv->amp_enable(1);
	priv->first_stream = NULL;
	priv->second_stream = NULL;

	return ret;
}

static int __devexit imx_alc5640_remove(struct platform_device *pdev)
{
	struct mxc_audio_platform_data *plat = pdev->dev.platform_data;
	struct imx_priv *priv = &card_priv;

	if (card_priv.amp_enable)
		card_priv.amp_enable(0);
	plat->clock_enable(0);

	if (plat->finit)
		plat->finit();
#ifdef CONFIG_ANDROID
	switch_dev_unregister(&priv->sdev);
#endif
	return 0;
}

static struct platform_driver imx_alc5640_driver = {
	.probe = imx_alc5640_probe,
	.remove = imx_alc5640_remove,
	.driver = {
		   .name = "imx-alc5640",
		   .owner = THIS_MODULE,
		   },
};

static struct platform_device *imx_snd_device;

static int __init imx_asoc_init(void)
{
	int ret;

	printk ("[alc5640 %s-%d] ...\n", __func__, __LINE__);
	ret = platform_driver_register(&imx_alc5640_driver);
	if (ret < 0)
		goto exit;

	imx_snd_device = platform_device_alloc("soc-audio", -1);
	if (!imx_snd_device)
		goto err_device_alloc;

	platform_set_drvdata(imx_snd_device, &snd_soc_card_imx);

	ret = platform_device_add(imx_snd_device);

	if (0 == ret)
		goto exit;

	platform_device_put(imx_snd_device);

err_device_alloc:
	platform_driver_unregister(&imx_alc5640_driver);
exit:
	return ret;
}

static void __exit imx_asoc_exit(void)
{
	platform_driver_unregister(&imx_alc5640_driver);
	platform_device_unregister(imx_snd_device);
}

module_init(imx_asoc_init);
module_exit(imx_asoc_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC imx alc5640");
MODULE_LICENSE("GPL");

/*
 *  sound/soc/omap/omap4_wm8994.c
 *
 *  Copyright (c) 2009 Samsung Electronics Co. Ltd
 *
 *  This program is free software; you can redistribute  it and/or  modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <sound/core.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/regulator/machine.h>
#include <linux/input.h>
#include <linux/delay.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>

#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/mux.h>
#include <plat/mcbsp.h>
#include <linux/gpio.h>
#include <linux/pm_qos_params.h>

#include "omap-pcm.h"
#include "omap-mcbsp.h"
#include "../codecs/wm8994.h"

#include "../../../arch/arm/mach-omap2/mux.h"
#include "../../../arch/arm/mach-omap2/omap_muxtbl.h"

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO)
#include "../../../arch/arm/mach-omap2/board-espresso.h"
#elif defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10)
#include "../../../arch/arm/mach-omap2/board-espresso10.h"
#endif

#define WM8994_DAI_AIF1	0
#define WM8994_DAI_AIF2	1
#define WM8994_DAI_AIF3	2

struct snd_soc_codec *the_codec;
int dock_status;
static struct pm_qos_request_list pm_qos_handle;

static struct gpio main_mic_bias = {
	.flags  = GPIOF_OUT_INIT_LOW,
	.label  = "MICBIAS_EN",
};

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
static struct gpio sub_mic_bias = {
	.flags  = GPIOF_OUT_INIT_LOW,
	.label  = "SUB_MICBIAS_EN",
};
#endif

static struct gpio ear_select = {
	.flags = GPIOF_OUT_INIT_LOW,
	.label = "EAR_GND_SEL",
};

static int hp_output_mode;
const char *hp_analogue_text[] = {
	"VoiceCall Mode", "Playback Mode"
};

static int aif2_mode;
const char *aif2_mode_text[] = {
	"Slave", "Master"
};

static int pm_mode;
const char *pm_mode_text[] = {
	"Off", "On"
};

static const struct soc_enum hp_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(hp_analogue_text), hp_analogue_text),
};

static int get_hp_output_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = hp_output_mode;
	return 0;
}

static int set_hp_output_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (hp_output_mode == ucontrol->value.integer.value[0])
		return 0;

	hp_output_mode = ucontrol->value.integer.value[0];
	gpio_set_value(ear_select.gpio, hp_output_mode);

	pr_debug("set hp mode : %s\n", hp_analogue_text[hp_output_mode]);

	return 0;
}

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static int get_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = aif2_mode;
	return 0;
}

static int set_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (aif2_mode == ucontrol->value.integer.value[0])
		return 0;

	aif2_mode = ucontrol->value.integer.value[0];

	pr_info("set aif2 mode : %s\n", aif2_mode_text[aif2_mode]);

	return 0;
}

static const struct soc_enum pm_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pm_mode_text), pm_mode_text),
};

static int get_pm_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = pm_mode;
	return 0;
}

static int set_pm_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (pm_mode == ucontrol->value.integer.value[0])
		return 0;

	if (pm_mode)
		pm_qos_update_request(&pm_qos_handle, PM_QOS_DEFAULT_VALUE);
	else
		pm_qos_update_request(&pm_qos_handle, 7);

	pm_mode = ucontrol->value.integer.value[0];

	pr_info("set pm mode : %s\n", pm_mode_text[pm_mode]);

	return 0;
}

static int main_mic_bias_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	gpio_set_value(main_mic_bias.gpio, SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
static int sub_mic_bias_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	gpio_set_value(sub_mic_bias.gpio, SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}
#endif

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
void notify_dock_status(int status)
{
	if (!the_codec)
		return;

	pr_info("%s: status=%d", __func__, status);
	dock_status = status;

	if (status)
		wm8994_vmid_mode(the_codec, WM8994_VMID_FORCE);
	else
		wm8994_vmid_mode(the_codec, WM8994_VMID_NORMAL);
}
#else
void notify_dock_status(int status)
{
	return;
}
#endif

static void omap4_wm8994_start_fll1(struct snd_soc_dai *aif1_dai)
{
	int ret;

	dev_info(aif1_dai->dev, "Moving to audio clocking settings\n");

	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(aif1_dai,
				  WM8994_FLL1,
				  WM8994_FLL_SRC_MCLK1,
				  26000000, 44100 * 256);
	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to start FLL1: %d\n", ret);

	/* Then switch AIF1CLK to it */
	ret = snd_soc_dai_set_sysclk(aif1_dai,
				     WM8994_SYSCLK_FLL1,
				     44100 * 256,
				     SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to switch to FLL1: %d\n", ret);

}

static int omap4_hifi_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	ret = snd_soc_dai_set_fmt(codec_dai,
				SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		pr_err("can't set codec DAI configuration\n");
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai,
				SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		pr_err("can't set CPU DAI configuration\n");
		return ret;
	}

	omap4_wm8994_start_fll1(codec_dai);

	return 0;
}

static struct snd_soc_ops hifi_ops = {
	.hw_params = omap4_hifi_hw_params,
};

static int omap4_wm8994_aif2_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;
	int prate;
	int bclk;

	pr_debug("%s: enter, aif2_mode=%d\n", __func__, aif2_mode);

	prate = params_rate(params);
	switch (prate) {
	case 8000:
	case 16000:
		break;
	default:
		dev_warn(codec_dai->dev, "Unsupported LRCLK %d, falling back to 8000Hz\n",
			(int)params_rate(params));
		prate = 8000;
	}

	/* Set the codec DAI configuration */
	if (aif2_mode == 0) {
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBS_CFS);
	} else {
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	}
	if (ret < 0)
		return ret;

	switch (prate) {
	case 8000:
		bclk = 256000;
		break;
	case 16000:
		bclk = 512000;
		break;
	default:
		return -EINVAL;
	}

	if (aif2_mode == 0) {
		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
				WM8994_FLL_SRC_BCLK,
				bclk, prate * 256);
	} else {
		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
				WM8994_FLL_SRC_MCLK1,
				26000000, prate * 256);
	}
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to configure FLL2: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL2,
				prate * 256, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to switch to FLL2: %d\n", ret);

	return 0;
}

static struct snd_soc_ops omap4_wm8994_aif2_ops = {
	.hw_params = omap4_wm8994_aif2_hw_params,
};

static int omap4_wm8994_aif3_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)

{
	pr_err("%s: enter\n", __func__);
	return 0;
}

static struct snd_soc_ops omap4_wm8994_aif3_ops = {
	.hw_params = omap4_wm8994_aif3_hw_params,
};

static const struct snd_kcontrol_new omap4_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("LINEOUT"),

	SOC_DAPM_PIN_SWITCH("Main Mic"),
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
#endif
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_ENUM_EXT("HP Output Mode", hp_mode_enum[0],
		get_hp_output_mode, set_hp_output_mode),
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),
	SOC_ENUM_EXT("PM Constraints Mode", pm_mode_enum[0],
		get_pm_mode, set_pm_mode),
};

const struct snd_soc_dapm_widget omap4_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("LINEOUT", NULL),

	SND_SOC_DAPM_MIC("Main Mic", main_mic_bias_event),
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
	SND_SOC_DAPM_MIC("Sub Mic", sub_mic_bias_event),
#endif
	SND_SOC_DAPM_MIC("Headset Mic", NULL),

};

const struct snd_soc_dapm_route omap4_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "RCV", NULL, "HPOUT2N" },
	{ "RCV", NULL, "HPOUT2P" },

	{ "LINEOUT", NULL, "LINEOUT1N" },
	{ "LINEOUT", NULL, "LINEOUT1P" },

	{ "IN1LP", NULL, "Main Mic" },
	{ "IN1LN", NULL, "Main Mic" },
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
	{ "IN2RP:VXRP", NULL, "Sub Mic" },
	{ "IN2RN", NULL, "Sub Mic" },
#endif
	{ "IN1RP", NULL, "Headset Mic" },
	{ "IN1RN", NULL, "Headset Mic" },
};

int omap4_wm8994_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *aif1_dai = &rtd->codec_dai[0];
	int ret;

	the_codec = codec;

	ret = snd_soc_add_controls(codec, omap4_controls,
				ARRAY_SIZE(omap4_controls));

	ret = snd_soc_dapm_new_controls(dapm, omap4_dapm_widgets,
					ARRAY_SIZE(omap4_dapm_widgets));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add DAPM widgets: %d\n", ret);

	ret = snd_soc_dapm_add_routes(dapm, omap4_dapm_routes,
					ARRAY_SIZE(omap4_dapm_routes));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add DAPM routes: %d\n", ret);

	/* set up NC codec pins */
	snd_soc_dapm_nc_pin(dapm, "IN2LP:VXRN");
	snd_soc_dapm_nc_pin(dapm, "IN2LN");
#ifdef CONFIG_MACH_SAMSUNG_ESPRESSO_10
	snd_soc_dapm_nc_pin(dapm, "IN2RP:VXRP");
	snd_soc_dapm_nc_pin(dapm, "IN2RN");
#endif

	snd_soc_dapm_ignore_suspend(dapm, "RCV");
	snd_soc_dapm_ignore_suspend(dapm, "SPK");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT");
	snd_soc_dapm_ignore_suspend(dapm, "HP");
	snd_soc_dapm_ignore_suspend(dapm, "Main Mic");
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
	snd_soc_dapm_ignore_suspend(dapm, "Sub Mic");
#endif
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1ADCDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2ADCDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3ADCDAT");

	ret = snd_soc_dai_set_sysclk(aif1_dai, WM8994_SYSCLK_MCLK2,
				     32768, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to boot clocking\n");

	return snd_soc_dapm_sync(dapm);
}

static struct snd_soc_dai_driver ext_dai[] = {
{
	.name = "CP",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 16000,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 16000,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
{
	.name = "BT",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 16000,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 16000,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
};

static struct snd_soc_dai_link omap4_dai[] = {
{
	.name = "MCBSP AIF1",
	.stream_name = "HIFI MCBSP Tx/RX",
	.cpu_dai_name = "omap-mcbsp-dai.2",
	.codec_dai_name = "wm8994-aif1",
	.platform_name = "omap-pcm-audio",
	.codec_name = "wm8994-codec",
	.init = omap4_wm8994_init,
	.ops = &hifi_ops,
},
{
	.name = "WM1811 Voice",
	.stream_name = "Voice Tx/Rx",
	.cpu_dai_name = "CP",
	.codec_dai_name = "wm8994-aif2",
	.platform_name = "snd-soc-dummy",
	.codec_name = "wm8994-codec",
	.ignore_suspend = 1,
	.ops = &omap4_wm8994_aif2_ops,
},
{
	.name = "WM1811 BT",
	.stream_name = "BT Tx/Rx",
	.cpu_dai_name = "BT",
	.codec_dai_name = "wm8994-aif3",
	.platform_name = "snd-soc-dummy",
	.codec_name = "wm8994-codec",
	.ignore_suspend = 1,
	.ops = &omap4_wm8994_aif3_ops,
},
};

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
static int wm8994_suspend_pre(struct snd_soc_card *card)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(the_codec);

	if (dock_status == 1 && wm8994->vmid_mode == WM8994_VMID_FORCE) {
		pr_info("%s: entering force vmid mode\n", __func__);
		wm8994_vmid_mode(the_codec, WM8994_VMID_NORMAL);
	}
	return 0;
}

static int wm8994_resume_post(struct snd_soc_card *card)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(the_codec);

	if (dock_status == 1 && wm8994->vmid_mode == WM8994_VMID_NORMAL) {
		pr_info("%s: entering normal vmid mode\n", __func__);
		wm8994_vmid_mode(the_codec, WM8994_VMID_FORCE);
	}
	return 0;
}
#else
#define wm8994_resume_post NULL
#define wm8994_suspend_pre NULL
#endif

static struct snd_soc_card omap4_wm8994 = {
	.name = "omap4_wm8994",
	.dai_link = omap4_dai,
	.num_links = ARRAY_SIZE(omap4_dai),
	.suspend_pre = wm8994_suspend_pre,
	.resume_post = wm8994_resume_post,
};

static struct platform_device *omap4_wm8994_snd_device;

static int __init omap4_audio_init(void)
{
	int ret;

	hp_output_mode = 1;
	ear_select.gpio = omap_muxtbl_get_gpio_by_name(ear_select.label);
	if (ear_select.gpio == -EINVAL)
		return -EINVAL;

	ret = gpio_request(ear_select.gpio, "ear_select");
	if (ret < 0)
		goto ear_select_err;

	gpio_direction_output(ear_select.gpio, hp_output_mode);

	main_mic_bias.gpio = omap_muxtbl_get_gpio_by_name(main_mic_bias.label);
	if (main_mic_bias.gpio == -EINVAL) {
		pr_err("failed to get gpio name for %s\n", main_mic_bias.label);
		ret = -EINVAL;
		goto main_mic_err;
	}

	ret = gpio_request(main_mic_bias.gpio, "main_mic_bias");
	if (ret < 0)
		goto main_mic_err;

	gpio_direction_output(main_mic_bias.gpio, 0);

	pm_qos_add_request(&pm_qos_handle, PM_QOS_CPU_DMA_LATENCY,
						PM_QOS_DEFAULT_VALUE);

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
	sub_mic_bias.gpio = omap_muxtbl_get_gpio_by_name(sub_mic_bias.label);
	if (sub_mic_bias.gpio == -EINVAL) {
		pr_err("failed to get gpio name for %s\n", sub_mic_bias.label);
		ret = -EINVAL;
		goto sub_mic_err;
	}

	ret = gpio_request(sub_mic_bias.gpio, "sub_mic_bias");
	if (ret < 0)
		goto sub_mic_err;

	gpio_direction_output(sub_mic_bias.gpio, 0);
#endif

	omap4_wm8994_snd_device = platform_device_alloc("soc-audio",  -1);
	if (!omap4_wm8994_snd_device) {
		pr_err("Platform device allocation failed\n");
		ret = -ENOMEM;
		goto device_err;
	}

	ret = snd_soc_register_dais(&omap4_wm8994_snd_device->dev,
				ext_dai, ARRAY_SIZE(ext_dai));
	if (ret != 0) {
		pr_err("Failed to register external DAIs: %d\n", ret);
		goto dai_err;
	}

	platform_set_drvdata(omap4_wm8994_snd_device, &omap4_wm8994);

	ret = platform_device_add(omap4_wm8994_snd_device);
	if (ret) {
		pr_err("Platform device allocation failed\n");
		goto err;
	}
	return ret;

err:
	snd_soc_unregister_dai(&omap4_wm8994_snd_device->dev);
dai_err:
	platform_device_put(omap4_wm8994_snd_device);
device_err:
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
	gpio_free(sub_mic_bias.gpio);
sub_mic_err:
#endif
	gpio_free(main_mic_bias.gpio);
main_mic_err:
	gpio_free(ear_select.gpio);
ear_select_err:
	return ret;
}
module_init(omap4_audio_init);

static void __exit omap4_audio_exit(void)
{
	platform_device_unregister(omap4_wm8994_snd_device);
	pm_qos_remove_request(&pm_qos_handle);
}
module_exit(omap4_audio_exit);

MODULE_AUTHOR("Quartz.Jang <quartz.jang@samsung.com");
MODULE_DESCRIPTION("ALSA Soc WM8994 omap4");
MODULE_LICENSE("GPL");

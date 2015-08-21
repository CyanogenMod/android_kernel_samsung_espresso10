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
#include <linux/regulator/machine.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/delay.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>

#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>
#ifndef CONFIG_SAMSUNG_JACK
#include <linux/mfd/wm8994/gpio.h>
#endif /* not CONFIG_SAMSUNG_JACK */

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

#define WM8994_DEFAULT_MCLK1	26000000
#define WM8994_DEFAULT_MCLK2	32768
#define WM8994_DEFAULT_SYNC_CLK	11289600

#ifndef CONFIG_SAMSUNG_JACK

#define WM8994_JACKDET_MODE_NONE  0x0000
#define WM8994_JACKDET_MODE_JACK  0x0100
#define WM8994_JACKDET_MODE_MIC   0x0080
#define WM8994_JACKDET_MODE_AUDIO 0x0180

#define WM8994_JACKDET_BTN0	0x04
#define WM8994_JACKDET_BTN1	0x10
#define WM8994_JACKDET_BTN2	0x08

static struct wm8958_micd_rate wm1811_det_rates[] = {
	{ WM8994_DEFAULT_MCLK2,     true,  0,  0 },
	{ WM8994_DEFAULT_MCLK2,    false,  0,  0 },
	{ WM8994_DEFAULT_SYNC_CLK,  true,  7,  7 },
	{ WM8994_DEFAULT_SYNC_CLK, false,  7,  7 },
};

static struct wm8958_micd_rate wm1811_jackdet_rates[] = {
	{ WM8994_DEFAULT_MCLK2,     true,  0,  0 },
	{ WM8994_DEFAULT_MCLK2,    false,  0,  0 },
	{ WM8994_DEFAULT_SYNC_CLK,  true, 12, 12 },
	{ WM8994_DEFAULT_SYNC_CLK, false,  7,  8 },
};

struct wm1811_machine_priv {
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct delayed_work mic_work;
	struct wake_lock jackdet_wake_lock;
};

#ifdef CONFIG_FACTORY_PBA_JACK_TEST_SUPPORT
/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;

#endif /* CONFIG_FACTORY_PBA_JACK_TEST_SUPPORT */
#endif /* not CONFIG_SAMSUNG_JACK */

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
struct snd_soc_codec *the_codec;
int dock_status;
#endif /* defined ESPRESSO */

static struct pm_qos_request_list pm_qos_handle;

static struct gpio mclk = {
	.flags = GPIOF_OUT_INIT_LOW,
	.label = "CODEC_CLK_REQ",
};

static struct gpio main_mic_bias = {
	.flags  = GPIOF_OUT_INIT_LOW,
	.label  = "MICBIAS_EN",
};

#ifdef CONFIG_SND_USE_SUB_MIC
static struct gpio sub_mic_bias = {
	.flags  = GPIOF_OUT_INIT_LOW,
	.label  = "SUB_MICBIAS_EN",
};
#endif /* CONFIG_SND_USE_SUB_MIC */

#ifdef CONFIG_SND_EAR_GND_SEL
static struct gpio ear_select = {
	.flags = GPIOF_OUT_INIT_LOW,
	.label = "EAR_GND_SEL",
};

static int hp_output_mode;
const char *hp_analogue_text[] = {
	"VoiceCall Mode", "Playback Mode"
};
#endif /* CONFIG_SND_EAR_GND_SEL */

#ifdef CONFIG_SND_USE_LINEOUT_SWITCH
static struct gpio lineout_select = {
	.flags = GPIOF_OUT_INIT_LOW,
	.label = "VPS_SOUND_EN",
};

static int lineout_mode;
const char *lineout_mode_text[] = {
	"Off", "On"
};
#endif /* CONFIG_SND_USE_LINEOUT_SWITCH */

static int input_clamp;
const char *input_clamp_text[] = {
	"Off", "On"
};

static int aif2_mode;
const char *aif2_mode_text[] = {
	"Slave", "Master"
};

static int pm_mode;
const char *pm_mode_text[] = {
	"Off", "On"
};

static void set_mclk(bool on)
{
	if (on)
		gpio_set_value(mclk.gpio, 1);
	else
		gpio_set_value(mclk.gpio, 0);
}

static int main_mic_bias_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	gpio_set_value(main_mic_bias.gpio, SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}

#ifdef CONFIG_SND_USE_SUB_MIC
static int sub_mic_bias_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	gpio_set_value(sub_mic_bias.gpio, SND_SOC_DAPM_EVENT_ON(event));
	return 0;
}
#endif /* CONFIG_SND_USE_SUB_MIC */

#ifdef CONFIG_SND_EAR_GND_SEL
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
#endif /* CONFIG_SND_EAR_GND_SEL */

#ifdef CONFIG_SND_USE_LINEOUT_SWITCH
static const struct soc_enum lineout_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lineout_mode_text), lineout_mode_text),
};

static int get_lineout_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = lineout_mode;
	return 0;
}

static int set_lineout_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	lineout_mode = ucontrol->value.integer.value[0];

	gpio_set_value(lineout_select.gpio, lineout_mode);

	dev_dbg(codec->dev, "set lineout mode : %s\n",
		lineout_mode_text[lineout_mode]);
	return 0;

}

static int omap_lineout_switch(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(codec->dev, "%s event is %02X", w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		gpio_set_value(lineout_select.gpio, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		gpio_set_value(lineout_select.gpio, 0);
		break;
	}
	return 0;
}
#endif /* CONFIG_SND_USE_LINEOUT_SWITCH */

static const struct soc_enum input_clamp_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(input_clamp_text), input_clamp_text),
};

static int get_input_clamp(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = input_clamp;
	return 0;
}

static int set_input_clamp(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	input_clamp = ucontrol->value.integer.value[0];

	if (input_clamp) {
		snd_soc_update_bits(codec, WM8994_INPUT_MIXER_1,
				WM8994_INPUTS_CLAMP, WM8994_INPUTS_CLAMP);
		msleep(100);
	} else {
		snd_soc_update_bits(codec, WM8994_INPUT_MIXER_1,
				WM8994_INPUTS_CLAMP, 0);
	}
	pr_info("set fm input_clamp : %s\n", input_clamp_text[input_clamp]);

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

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
void notify_dock_status(int status)
{
	if (!the_codec)
		return;

	dock_status = status;
	pr_info("%s: status=%d", __func__, dock_status);

	if (the_codec->suspended)
		return;

	if (status)
		wm8994_vmid_mode(the_codec, WM8994_VMID_FORCE);
	else
		wm8994_vmid_mode(the_codec, WM8994_VMID_NORMAL);
}
#endif /* defined ESPRESSO */

#ifndef CONFIG_SAMSUNG_JACK
static void wm1811_micd_set_rate(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	int best, i, sysclk, val;
	bool idle;
	const struct wm8958_micd_rate *rates = NULL;
	int num_rates = 0;

	idle = !wm8994->jack_mic;

	sysclk = snd_soc_read(codec, WM8994_CLOCKING_1);
	if (sysclk & WM8994_SYSCLK_SRC)
		sysclk = wm8994->aifclk[1];
	else
		sysclk = wm8994->aifclk[0];

	if (wm8994->jackdet) {
		rates = wm1811_jackdet_rates;
		num_rates = ARRAY_SIZE(wm1811_jackdet_rates);
		wm8994->pdata->micd_rates = wm1811_jackdet_rates;
		wm8994->pdata->num_micd_rates = num_rates;
	} else {
		rates = wm1811_det_rates;
		num_rates = ARRAY_SIZE(wm1811_det_rates);
		wm8994->pdata->micd_rates = wm1811_det_rates;
		wm8994->pdata->num_micd_rates = num_rates;
	}

	best = 0;
	for (i = 0; i < num_rates; i++) {
		if (rates[i].idle != idle)
			continue;
		if (abs(rates[i].sysclk - sysclk) <
		    abs(rates[best].sysclk - sysclk))
			best = i;
		else if (rates[best].idle != idle)
			best = i;
	}

	val = rates[best].start << WM8958_MICD_BIAS_STARTTIME_SHIFT
		| rates[best].rate << WM8958_MICD_RATE_SHIFT;

	snd_soc_update_bits(codec, WM8958_MIC_DETECT_1,
			    WM8958_MICD_BIAS_STARTTIME_MASK |
			    WM8958_MICD_RATE_MASK, val);
}

static void wm1811_micdet(u16 status, void *data)
{
	struct wm1811_machine_priv *wm1811 = data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(wm1811->codec);
	int report;

	wake_lock_timeout(&wm1811->jackdet_wake_lock, 5 * HZ);

	/* Either nothing present or just starting detection */
	if (!(status & WM8958_MICD_STS)) {
		if (!wm8994->jackdet) {
			/* If nothing present then clear our statuses */
			dev_dbg(wm1811->codec->dev, "Detected open circuit\n");
			wm8994->jack_mic = false;
			wm8994->mic_detecting = true;

			wm1811_micd_set_rate(wm1811->codec);

			snd_soc_jack_report(wm8994->micdet[0].jack, 0,
					    wm8994->btn_mask |
					     SND_JACK_HEADSET);
		}
		/*ToDo*/
		/*return;*/
	}

	/* If the measurement is showing a high impedence we've got a
	 * microphone.
	 */
	if (wm8994->mic_detecting && (status & 0x400)) {
		dev_info(wm1811->codec->dev, "Detected microphone\n");

		wm8994->mic_detecting = false;
		wm8994->jack_mic = true;

		wm1811_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADSET,
				    SND_JACK_HEADSET);
	}

	if (wm8994->mic_detecting && (status & 0x4)) {
		dev_info(wm1811->codec->dev, "Detected headphone\n");
		wm8994->mic_detecting = false;

		wm1811_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADPHONE,
				    SND_JACK_HEADSET);

		/* If we have jackdet that will detect removal */
		if (wm8994->jackdet) {
			mutex_lock(&wm8994->accdet_lock);

			snd_soc_update_bits(wm1811->codec, WM8958_MIC_DETECT_1,
					    WM8958_MICD_ENA, 0);

			if (wm8994->active_refcount) {
				snd_soc_update_bits(wm1811->codec,
					WM8994_ANTIPOP_2,
					WM1811_JACKDET_MODE_MASK,
					WM8994_JACKDET_MODE_AUDIO);
			}

			mutex_unlock(&wm8994->accdet_lock);

			if (wm8994->pdata->jd_ext_cap) {
				mutex_lock(&wm1811->codec->mutex);
				snd_soc_dapm_disable_pin(&wm1811->codec->dapm,
							 "MICBIAS2");
				snd_soc_dapm_sync(&wm1811->codec->dapm);
				mutex_unlock(&wm1811->codec->mutex);
			}
		}
	}

	/* Report short circuit as a button */
	if (wm8994->jack_mic) {
		report = 0;
		if (status & WM8994_JACKDET_BTN0)
			report |= SND_JACK_BTN_0;

		if (status & WM8994_JACKDET_BTN1)
			report |= SND_JACK_BTN_1;

		if (status & WM8994_JACKDET_BTN2)
			report |= SND_JACK_BTN_2;

		dev_dbg(wm1811->codec->dev, "Detected Button: %08x (%08X)\n",
			report, status);

		snd_soc_jack_report(wm8994->micdet[0].jack, report,
				    wm8994->btn_mask);
	}
}
#endif /* not CONFIG_SAMSUNG_JACK */

static int omap4_wm8994_start_fll1(struct snd_soc_dai *aif1_dai)
{
	int ret;

	dev_info(aif1_dai->dev, "Moving to audio clocking settings\n");

	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(aif1_dai,
				  WM8994_FLL1,
				  WM8994_FLL_SRC_MCLK1,
				  WM8994_DEFAULT_MCLK1, 44100 * 256);
	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to start FLL1: %d\n", ret);

	/* Then switch AIF1CLK to it */
	ret = snd_soc_dai_set_sysclk(aif1_dai,
				     WM8994_SYSCLK_FLL1,
				     44100 * 256,
				     SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to switch to FLL1: %d\n", ret);

	return ret;
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

	ret = omap4_wm8994_start_fll1(codec_dai);
	if (ret < 0) {
		pr_err("can't start fll1\n");
		return ret;
	}

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
				WM8994_DEFAULT_MCLK1, prate * 256);
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

#ifdef CONFIG_SND_USE_SUB_MIC
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
#endif /* CONFIG_SND_USE_SUB_MIC */

	SOC_DAPM_PIN_SWITCH("Headset Mic"),

#ifdef CONFIG_FM_RADIO
	SOC_DAPM_PIN_SWITCH("FM In"),
#endif /* CONFIG_FM_RADIO */

#ifdef CONFIG_SND_EAR_GND_SEL
	SOC_ENUM_EXT("HP Output Mode", hp_mode_enum[0],
		get_hp_output_mode, set_hp_output_mode),
#endif /* CONFIG_SND_EAR_GND_SEL */

#ifdef CONFIG_SND_USE_LINEOUT_SWITCH
	SOC_ENUM_EXT("LineoutSwitch Mode", lineout_mode_enum[0],
		get_lineout_mode, set_lineout_mode),
#endif /* CONFIG_SND_USE_LINEOUT_SWITCH */

	SOC_ENUM_EXT("Input Clamp", input_clamp_enum[0],
		get_input_clamp, set_input_clamp),
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),
	SOC_ENUM_EXT("PM Constraints Mode", pm_mode_enum[0],
		get_pm_mode, set_pm_mode),
};

const struct snd_soc_dapm_widget omap4_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
#ifdef CONFIG_SND_USE_LINEOUT_SWITCH
	SND_SOC_DAPM_LINE("LINEOUT", omap_lineout_switch),
#else /* CONFIG_SND_USE_LINEOUT_SWITCH */
	SND_SOC_DAPM_LINE("LINEOUT", NULL),
#endif /* not CONFIG_SND_USE_LINEOUT_SWITCH */

	SND_SOC_DAPM_MIC("Main Mic", main_mic_bias_event),

#ifdef CONFIG_SND_USE_SUB_MIC
	SND_SOC_DAPM_MIC("Sub Mic", sub_mic_bias_event),
#endif /* CONFIG_SND_USE_SUB_MIC */

	SND_SOC_DAPM_MIC("Headset Mic", NULL),

#ifdef CONFIG_FM_RADIO
	SND_SOC_DAPM_LINE("FM In", NULL),
#endif /* CONFIG_FM_RADIO */
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

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
	{ "LINEOUT", NULL, "LINEOUT1N" },
	{ "LINEOUT", NULL, "LINEOUT1P" },
#else /* defined ESPRESSO */
	{ "LINEOUT", NULL, "LINEOUT2N" },
	{ "LINEOUT", NULL, "LINEOUT2P" },
#endif /* not ESPRESSO for OMAP4470 Project */

	{ "IN1LP", NULL, "Main Mic" },
	{ "IN1LN", NULL, "Main Mic" },

#ifdef CONFIG_SND_USE_SUB_MIC
	{ "IN2RP:VXRP", NULL, "Sub Mic" },
	{ "IN2RN", NULL, "Sub Mic" },
#endif /* CONFIG_SND_USE_SUB_MIC */

#ifndef CONFIG_SAMSUNG_JACK
	{ "IN1RP", NULL, "Headset Mic" },
	{ "IN1RN", NULL, "Headset Mic" },
#else /* not CONFIG_SAMSUNG_JACK */
	{ "IN1RP", NULL, "MICBIAS2" },
	{ "IN1RN", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },
#endif /* CONFIG_SAMSUNG_JACK */

#ifdef CONFIG_FM_RADIO
	{ "IN2LN", NULL, "FM In" },
#ifdef CONFIG_SND_USE_SUB_MIC
	{ "IN2LP:VXRN", NULL, "FM In" },
#else /* CONFIG_SND_USE_SUB_MIC */
	{ "IN2RN", NULL, "FM In" },
#endif /* not CONFIG_SND_USE_SUB_MIC */
#endif /* CONFIG_FM_RADIO */
};

#ifndef CONFIG_SAMSUNG_JACK
#ifdef CONFIG_FACTORY_PBA_JACK_TEST_SUPPORT
static ssize_t earjack_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	int report = 0;

	if ((wm8994->micdet[0].jack->status & SND_JACK_HEADPHONE) ||
		(wm8994->micdet[0].jack->status & SND_JACK_HEADSET)) {
		report = 1;
	}

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_key_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	int report = 0;

	if (wm8994->micdet[0].jack->status & SND_JACK_BTN_0)
		report = 1;

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_key_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	wm8994->mic_detecting = false;
	wm8994->jack_mic = true;

	wm1811_micd_set_rate(codec);

	if ((!size) || (buf[0] != '1')) {
		snd_soc_jack_report(wm8994->micdet[0].jack,
				    0, SND_JACK_HEADSET);
		dev_info(codec->dev, "Forced remove microphone\n");
	} else {

		snd_soc_jack_report(wm8994->micdet[0].jack,
				    SND_JACK_HEADSET, SND_JACK_HEADSET);
		dev_info(codec->dev, "Forced detect microphone\n");
	}

	return size;
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_key_state_show, earjack_key_state_store);

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_state_show, earjack_state_store);
#endif /* CONFIG_FACTORY_PBA_JACK_TEST_SUPPORT */
#endif /* not CONFIG_SAMSUNG_JACK */

int omap4_wm8994_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
#ifndef CONFIG_SAMSUNG_JACK
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);
	struct wm8994 *control = codec->control_data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
#endif /* not CONFIG_SAMSUNG_JACK */
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *aif1_dai = rtd->codec_dai;
	int ret;

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10) \
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
	the_codec = codec;
#endif	/* defined ESPRESSO */

	set_mclk(true); /* enable 26M CLK */

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

	ret = snd_soc_dai_set_sysclk(aif1_dai, WM8994_SYSCLK_MCLK2,
				     WM8994_DEFAULT_MCLK2, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to boot clocking\n");

	ret = snd_soc_dapm_force_enable_pin(dapm, "AIF1CLK");
	if (ret < 0)
		dev_err(codec->dev, "Failed to enable AIF1CLK: %d\n", ret);

	/* set up NC codec pins */
#ifdef CONFIG_FM_RADIO
#ifndef CONFIG_SND_USE_SUB_MIC
	snd_soc_dapm_nc_pin(dapm, "IN2LP:VXRN");
	snd_soc_dapm_nc_pin(dapm, "IN2RP:VXRP");
#endif /* not CONFIG_SND_USE_SUB_MIC */
#else /* CONFIG_FM_RADIO */
	snd_soc_dapm_nc_pin(dapm, "IN2LP:VXRN");
	snd_soc_dapm_nc_pin(dapm, "IN2LN");
#endif /* not CONFIG_FM_RADIO */

	/* set up ignore pins */
	snd_soc_dapm_ignore_suspend(dapm, "RCV");
	snd_soc_dapm_ignore_suspend(dapm, "SPK");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT");
	snd_soc_dapm_ignore_suspend(dapm, "HP");
	snd_soc_dapm_ignore_suspend(dapm, "Main Mic");
#ifdef CONFIG_SND_USE_SUB_MIC
	snd_soc_dapm_ignore_suspend(dapm, "Sub Mic");
#endif /* CONFIG_SND_USE_SUB_MIC */
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
#ifdef CONFIG_FM_RADIO
	snd_soc_dapm_ignore_suspend(dapm, "FM In");
#endif /* CONFIG_FM_RADIO */
	snd_soc_dapm_ignore_suspend(dapm, "AIF1DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1ADCDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2ADCDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3ADCDAT");

#ifdef CONFIG_SAMSUNG_JACK
	/* By default use idle_bias_off, will override for WM8994 */
	codec->dapm.idle_bias_off = 0;
#else /* CONFIG_SAMSUNG_JACK */
	wm1811->codec = codec;

	wm1811_micd_set_rate(codec);

	wm1811->jack.status = 0;

	ret = snd_soc_jack_new(codec, "Wm1811 Jack",
				SND_JACK_HEADSET | SND_JACK_BTN_0 |
				SND_JACK_BTN_1 | SND_JACK_BTN_2,
				&wm1811->jack);
	if (ret < 0)
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_0,
							KEY_MEDIA);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_MEDIA: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_1,
							KEY_VOLUMEDOWN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEUP: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_2,
							KEY_VOLUMEUP);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEDOWN: %d\n", ret);

	if (wm8994->revision > 1) {
		dev_info(codec->dev, "wm1811: Rev %c support mic detection\n",
			'A' + wm8994->revision);
		ret = wm8958_mic_detect(codec, &wm1811->jack, wm1811_micdet,
			wm1811);

		if (ret < 0)
			dev_err(codec->dev, "Failed start detection: %d\n",
				ret);
	} else {
		dev_info(codec->dev, "wm1811: Rev %c doesn't support mic detection\n",
			'A' + wm8994->revision);
		codec->dapm.idle_bias_off = 0;
	}

	/* To wakeup for earjack event in suspend mode */
	enable_irq_wake(control->irq);

	wake_lock_init(&wm1811->jackdet_wake_lock,
					WAKE_LOCK_SUSPEND, "wm1811_jackdet");

#ifdef CONFIG_FACTORY_PBA_JACK_TEST_SUPPORT
	/* To support PBA function test */
	jack_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(jack_class))
		pr_err("Failed to create class\n");

	jack_dev = device_create(jack_class, NULL, 0, codec, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	if (device_create_file(jack_dev, &dev_attr_key_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_key_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_state.attr.name);

#endif /* CONFIG_FACTORY_PBA_JACK_TEST_SUPPORT */
#endif /* not CONFIG_SAMSUNG_JACK */

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
	struct snd_soc_codec *codec = card->rtd->codec;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	if (dock_status == 1 && wm8994->vmid_mode == WM8994_VMID_FORCE) {
		pr_info("%s: entering force vmid mode\n", __func__);
		wm8994_vmid_mode(codec, WM8994_VMID_NORMAL);
	}

	snd_soc_dapm_disable_pin(&codec->dapm, "AIF1CLK");

	return 0;
}

static int wm8994_resume_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	if (dock_status == 1 && wm8994->vmid_mode == WM8994_VMID_NORMAL) {
		pr_info("%s: entering normal vmid mode\n", __func__);
		wm8994_vmid_mode(codec, WM8994_VMID_FORCE);
	}

	snd_soc_dapm_force_enable_pin(&codec->dapm, "AIF1CLK");

	return 0;
}
#else /* defined ESPRESSO */
#define wm8994_resume_post NULL
#define wm8994_suspend_pre NULL
#endif /* not ESPRESSO for OMAP4470 Project */

static int wm8994_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *aif2_dai = card->rtd[1].codec_dai;
	int ret;

	if (!codec->active) {

#ifndef CONFIG_SAMSUNG_JACK
		ret = snd_soc_dai_set_sysclk(aif2_dai,
					     WM8994_SYSCLK_MCLK2,
					     WM8994_DEFAULT_MCLK2,
					     SND_SOC_CLOCK_IN);

		if (ret < 0)
			dev_err(codec->dev, "Unable to switch to MCLK2: %d\n",
				ret);

		ret = snd_soc_dai_set_pll(aif2_dai, WM8994_FLL2, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL2\n");

		ret = snd_soc_dai_set_sysclk(aif1_dai,
					     WM8994_SYSCLK_MCLK2,
					     WM8994_DEFAULT_MCLK2,
					     SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(codec->dev, "Unable to switch to MCLK2\n");

		ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL1\n");
#endif /* not CONFIG_SAMSUNG_JACK */

		set_mclk(false); /* disble 26M CLK */
	}
	return 0;
}

static int wm8994_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	int ret;
	int reg = 0;

	set_mclk(true); /* enable 26M CLK */

#ifndef CONFIG_SAMSUNG_JACK
	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1,
				  WM8994_FLL_SRC_MCLK1,
				  WM8994_DEFAULT_MCLK1,
				  WM8994_DEFAULT_SYNC_CLK);

	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to start FLL1: %d\n", ret);

	/* Then switch AIF1CLK to it */
	ret = snd_soc_dai_set_sysclk(aif1_dai,
				     WM8994_SYSCLK_FLL1,
				     WM8994_DEFAULT_SYNC_CLK,
				     SND_SOC_CLOCK_IN);

	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to switch to FLL1: %d\n", ret);

    /* workaround for jack detection
	* sometimes WM8994_GPIO_1 type changed wrong function type
	* so if type mismatched, update to IRQ type
	*/
	reg = snd_soc_read(codec, WM8994_GPIO_1);
	if ((reg & WM8994_GPN_FN_MASK) != WM8994_GP_FN_IRQ) {
		dev_err(codec->dev, "%s: GPIO1 type 0x%x\n", __func__, reg);
		snd_soc_write(codec, WM8994_GPIO_1, WM8994_GP_FN_IRQ);
	}
#endif /* not CONFIG_SAMSUNG_JACK */

	return 0;
}

static struct snd_soc_card omap4_wm8994 = {
	.name = "omap4_wm8994",
	.dai_link = omap4_dai,
	.num_links = ARRAY_SIZE(omap4_dai),
	.suspend_post = wm8994_suspend_post,
	.resume_pre = wm8994_resume_pre,
	.suspend_pre = wm8994_suspend_pre,
	.resume_post = wm8994_resume_post,
};

static struct platform_device *omap4_wm8994_snd_device;

static int __init omap4_audio_init(void)
{
#ifndef CONFIG_SAMSUNG_JACK
	struct wm1811_machine_priv *wm1811;
#endif /* not CONFIG_SAMSUNG_JACK */
	int ret;

	mclk.gpio = omap_muxtbl_get_gpio_by_name(mclk.label);
	if (mclk.gpio == -EINVAL) {
		pr_err("failed to get gpio name for %s\n", mclk.label);
		ret = -EINVAL;
		goto mclk_err;
	}
	ret = gpio_request(mclk.gpio, "mclk");
	if (ret < 0)
		goto mclk_err;
	gpio_direction_output(mclk.gpio, 0);

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

#ifdef CONFIG_SND_USE_SUB_MIC
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
#endif /* CONFIG_SND_USE_SUB_MIC */

#ifdef CONFIG_SND_EAR_GND_SEL
	hp_output_mode = 1;
	ear_select.gpio = omap_muxtbl_get_gpio_by_name(ear_select.label);
	if (ear_select.gpio == -EINVAL)
		return -EINVAL;
	ret = gpio_request(ear_select.gpio, "ear_select");
	if (ret < 0)
		goto ear_select_err;
	gpio_direction_output(ear_select.gpio, hp_output_mode);
#endif /* CONFIG_SND_EAR_GND_SEL */

#ifdef CONFIG_SND_USE_LINEOUT_SWITCH
	lineout_mode = 0;
	lineout_select.gpio = \
			omap_muxtbl_get_gpio_by_name(lineout_select.label);
	if (lineout_select.gpio == -EINVAL)
		return -EINVAL;
	ret = gpio_request(lineout_select.gpio, "lineout_select");
	if (ret < 0)
		goto lineout_select_err;
	gpio_direction_output(lineout_select.gpio, lineout_mode);
#endif /* CONFIG_SND_USE_LINEOUT_SWITCH */

	pm_qos_add_request(&pm_qos_handle, PM_QOS_CPU_DMA_LATENCY,
						PM_QOS_DEFAULT_VALUE);

#ifndef CONFIG_SAMSUNG_JACK
	wm1811 = kzalloc(sizeof *wm1811, GFP_KERNEL);
	if (!wm1811) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_kzalloc;
	}
	snd_soc_card_set_drvdata(&omap4_wm8994, wm1811);
#endif /* not CONFIG_SAMSUNG_JACK */

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
	snd_soc_unregister_dais(&omap4_wm8994_snd_device->dev,
				ARRAY_SIZE(ext_dai));
dai_err:
	platform_device_put(omap4_wm8994_snd_device);
device_err:
#ifndef CONFIG_SAMSUNG_JACK
	kfree(wm1811);
err_kzalloc:
#endif /* not CONFIG_SAMSUNG_JACK */
#ifdef CONFIG_SND_USE_LINEOUT_SWITCH
	gpio_free(lineout_select.gpio);
lineout_select_err:
#endif /* CONFIG_SND_USE_LINEOUT_SWITCH */
#ifdef CONFIG_SND_EAR_GND_SEL
	gpio_free(ear_select.gpio);
ear_select_err:
#endif /* CONFIG_SND_EAR_GND_SEL */
#ifdef CONFIG_SND_USE_SUB_MIC
	gpio_free(sub_mic_bias.gpio);
sub_mic_err:
#endif /* CONFIG_SND_USE_SUB_MIC */
	gpio_free(main_mic_bias.gpio);
main_mic_err:
	gpio_free(mclk.gpio);
mclk_err:

	return ret;
}
module_init(omap4_audio_init);

static void __exit omap4_audio_exit(void)
{
#ifndef CONFIG_SAMSUNG_JACK
	struct snd_soc_card *card = &omap4_wm8994;
	struct wm1811_machine_priv *wm1811 = snd_soc_card_get_drvdata(card);
#endif /* not CONFIG_SAMSUNG_JACK */

	platform_device_unregister(omap4_wm8994_snd_device);
	pm_qos_remove_request(&pm_qos_handle);

#ifndef	CONFIG_SAMSUNG_JACK
	kfree(wm1811);
#endif /* not CONFIG_SAMSUNG_JACK */
}
module_exit(omap4_audio_exit);

MODULE_AUTHOR("Quartz.Jang <quartz.jang@samsung.com");
MODULE_DESCRIPTION("ALSA Soc WM8994 omap4");
MODULE_LICENSE("GPL");

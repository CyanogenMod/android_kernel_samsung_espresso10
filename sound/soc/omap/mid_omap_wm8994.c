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
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/regulator/machine.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/switch.h>
#include <linux/suspend.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/pm_qos_params.h>

#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>

#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/mux.h>
#include <plat/mcbsp.h>

#include "omap-pcm.h"
#include "omap-mcbsp.h"
#include "../codecs/wm8994.h"

#include "../../../arch/arm/mach-omap2/mux.h"
#include "../../../arch/arm/mach-omap2/omap_muxtbl.h"

#define WM8994_DAI_AIF1	0
#define WM8994_DAI_AIF2	1
#define WM8994_DAI_AIF3	2

#define USE_TO_DELAY
#define REDUCE_DOCK_NOISE
/* #define CONFIG_WM1811_CODEC_ANALOG_TUNING */

/* not used in the harrsion(su26.lee) */
#define DISABLE_NEEDLESS_GPIO_PIN

/* PM Constraint feature for the VOIP time stamp */
static struct pm_qos_request_list pm_qos_handle;

struct snd_soc_codec *the_codec;
int dock_status;
/* Analog tuning program */
/* Support function : delay time */
#ifdef CONFIG_WM1811_CODEC_ANALOG_TUNING
#include <linux/string.h>
#include <linux/kernel.h>;

enum {TUNING_VOICE_RECORDING,
		TUNING_VOICE_RECGNITION,
		TUNING_VOICE_CAMCORDING};
enum {TUNING_FAIL = -1, TUNING_OK = 0};

int isLoadSoundConfig = TUNING_OK;
static int tuing_delay_time;

int ReadSoundConfigFile(char *Filename)
{
	struct file *filp;
	mm_segment_t oldfs;
	int BytesRead;
	char *Buffer;
	char *token;
	char *last;

	Buffer = kmalloc(256, GFP_KERNEL);
	if (Buffer == NULL)
		return TUNING_FAIL;

	filp = filp_open(Filename, 00, O_RDONLY);
	if (IS_ERR(filp) || (filp == NULL))	{
		pr_info("%s file isn't exist", Filename);

		/* Or do something else */
		goto TUNING_FAIL_NO_FILE;
	}

	if (filp->f_op->read == NULL)	{
		pr_info("%s file doesn't allow reads");

		/* File(system) doesn't allow reads */
		goto TUNING_FAIL_NO_DATA;
	}

	/* Now read 4096 bytes from postion "StartPos" */
	filp->f_pos = 0;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	BytesRead = filp->f_op->read(filp, Buffer, 256, &filp->f_pos);
	set_fs(oldfs);

	last = Buffer;
	token = strsep(&last, ",");
	tuing_delay_time = kstrtoull(token, NULL, 10);

	pr_info("TUNING_DELAY_TIME=%d", tuing_delay_time);

	/* Close the file */
	filp_close(filp, NULL);

	/* release allocate memeory */
	kfree(Buffer);

	return TUNING_OK;

TUNING_FAIL_NO_FILE:
	kfree(Buffer);
	return TUNING_FAIL;

TUNING_FAIL_NO_DATA:
	filp_close(filp, NULL);
	kfree(Buffer);
	return TUNING_FAIL;
}
#endif		/* CONFIG_WM1811_CODEC_ANALOG_TUNING */

#ifndef DISABLE_NEEDLESS_GPIO_PIN
static struct gpio main_mic_bias = {
	.flags  = GPIOF_OUT_INIT_LOW,
	.label  = "MICBIAS_EN",
};

static struct gpio ear_select = {
	.flags = GPIOF_OUT_INIT_LOW,
	.label = "EAR_GND_SEL",
};
#endif

/* Determines the mclk of the board. */
unsigned int board_mclk = 38400000;

static struct gpio mute_ic = {
	.flags = GPIOF_OUT_INIT_LOW,
	.label = "MUTE_ON",
};

static int hp_output_mode;
const char *hp_analogue_text[] = {
	"VoiceCall Mode", "Playback Mode"
};

static int aif2_mode;
const char *aif2_mode_text[] = {
	"Slave", "Master"
};

static int mute_ic_mode;
const char *mute_ic_mode_text[] = {
	"OFF", "ON"
};

/* PM Constraint feature for the VOIP time stamp */
static int pm_mode;
const char *pm_mode_text[] = {
	"Off", "On"
};

static int fmradio_mute;
const char *fmradio_mute_text[] = {
	"OFF", "ON"
};

static int playback_lp_mode;
const char *playback_lp_mode_text[] = {
	"None", "Ready"
};

#ifdef USE_TO_DELAY	/* Additional delay control */
static int delay_ms;
const char *delay_ms_text[] = {"0"};
#endif

#ifdef REDUCE_DOCK_NOISE	/* Additional control : Reduce docking noise */
/* #define DOCK_LOG */

#define DOCK_OUT_HEADPHONE_VOLUME 0x37
#define DOCK_OUT_SPEAKER_VOLUME 0x3d

enum {DOCK_OUT_MONO = 0, DOCK_OUT_STEREO};

static int dock_out_hp_volume_up_down;
const char *dock_out_hp_volume_up_down_text[] = {
	"Down", "Up"
};

static int dock_out_spk_volume_up_down;
const char *dock_out_spk_volume_up_down_text[] = {
	"Down", "Up"
};
#endif

#ifdef CONFIG_MACH_SAMSUNG_HARRISON
#define CONFIG_SEC_DEV_JACK
#endif

#ifndef CONFIG_SEC_DEV_JACK
/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;
#endif

/* PM Constraint feature for the VOIP time stamp */
#define OMAP4_AUDIO_PM_CONSTRAINT


/* End of CONFIG_MACH_SAMSUNG_HARRISON feature */
#ifndef CONFIG_SEC_DEV_JACK

#define OMAP4_DEFAULT_MCLK2	32768
#define OMAP4_DEFAULT_SYNC_CLK	11289600

#define WM1811_JACKDET_MODE_NONE  0x0000
#define WM1811_JACKDET_MODE_JACK  0x0100
#define WM1811_JACKDET_MODE_MIC   0x0080
#define WM1811_JACKDET_MODE_AUDIO 0x0180

#define WM1811_JACKDET_BTN0	0x04
#define WM1811_JACKDET_BTN1	0x10
#define WM1811_JACKDET_BTN2	0x08

struct wm1811_machine_priv {
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct delayed_work mic_work;
	struct wake_lock jackdet_wake_lock;

#ifdef JACK_DETECT_QUEUE
	struct workqueue_struct *jack_det_queue;
	struct delayed_work jack_det_work;

	int jack_det_delayed_work_time;
#endif
};

static struct wm8958_micd_rate omap4_det_rates[] = {
	{ OMAP4_DEFAULT_MCLK2,     true,  0,  0 },
	{ OMAP4_DEFAULT_MCLK2,    false,  0,  0 },
	{ OMAP4_DEFAULT_SYNC_CLK,  true,  7,  7 },
	{ OMAP4_DEFAULT_SYNC_CLK, false,  7,  7 },
};

static struct wm8958_micd_rate omap4_jackdet_rates[] = {
	{ OMAP4_DEFAULT_MCLK2,     true,  0,  0 },
	{ OMAP4_DEFAULT_MCLK2,    false,  0,  0 },
	{ OMAP4_DEFAULT_SYNC_CLK,  true, 10, 10 },
	{ OMAP4_DEFAULT_SYNC_CLK, false,  7,  8 },
};

/* To support PBA function test */
#ifdef CONFIG_SUPPORT_PBA_FUNCTION_TEST
static struct class *jack_class;
static struct device *jack_dev;
#endif	/* CONFIG_SUPPORT_PBA_FUNCTION_TEST */

#endif	/* CONFIG_SEC_DEV_JACK */

static const struct soc_enum mute_ic_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mute_ic_mode_text), mute_ic_mode_text),
};

static int get_mute_ic_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mute_ic_mode;
	return 0;
}

static int set_mute_ic_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mute_ic_mode = ucontrol->value.integer.value[0];

	hp_output_mode = ucontrol->value.integer.value[0];
	gpio_set_value(mute_ic.gpio, mute_ic_mode);

	pr_info("set mute_ic mode : %s, %d\n",
		mute_ic_mode_text[mute_ic_mode], gpio_get_value(mute_ic.gpio));

	return 0;
}

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
#ifndef DISABLE_NEEDLESS_GPIO_PIN
	gpio_set_value(ear_select.gpio, hp_output_mode);
#endif

	pr_debug("set hp mode : %s\n", hp_analogue_text[hp_output_mode]);

	return 0;
}

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static const struct soc_enum fmradio_mute_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fmradio_mute_text), fmradio_mute_text),
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

/*
* set aif mute for the FM noise recovery.
*/
void set_aif2_mute_enable(bool en)
{
	unsigned int value = 0;

	if (!en)
		value |= WM8994_AIF2DACL_TO_DAC1L;

	snd_soc_update_bits(the_codec, WM8994_DAC1_RIGHT_MIXER_ROUTING,
		    WM8994_AIF2DACR_TO_DAC1R_MASK, value);
	snd_soc_update_bits(the_codec, WM8994_DAC1_LEFT_MIXER_ROUTING,
		    WM8994_AIF2DACL_TO_DAC1L_MASK, value);

	pr_info("set_aif2_enable : %d\n", en);
}
EXPORT_SYMBOL_GPL(set_aif2_mute_enable);


/*
* FM Radio mute : control AIF2 to DAC path
* FM --> AIF2(CODEC) --> DAC
*/
static int get_fmradio_mute_enable(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = fmradio_mute;
	return 0;
}

static int set_fmradio_mute_enable(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	fmradio_mute = ucontrol->value.integer.value[0];

	set_aif2_mute_enable(fmradio_mute);

	pr_info("set fmradio_mute mode : %s, %d\n",
		fmradio_mute_text[fmradio_mute], fmradio_mute);

	return 0;
}

#ifdef USE_TO_DELAY	/* Additional delay control */
static const struct soc_enum delay_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(delay_ms_text), delay_ms_text),
};

static int get_delay_ms(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return delay_ms;
}

static int set_delay_ms(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#ifdef CONFIG_WM1811_CODEC_ANALOG_TUNING	/* Analog tuning program */
	if (TUNING_OK == isLoadSoundConfig)	{
		if (TUNING_OK == (ReadSoundConfigFile("/data/delay.txt"))) {
			isLoadSoundConfig = TUNING_OK;
			delay_ms = tuing_delay_time;
		}
	}
#else
	delay_ms = ucontrol->value.integer.value[0];
#endif		/* CONFIG_WM1811_CODEC_ANALOG_TUNING */

	msleep(delay_ms);

	pr_info("delay time=%d\n", delay_ms);

	return 0;
}
#endif

#ifdef REDUCE_DOCK_NOISE
/* Additional control : To reduce the docking noist */
static const struct soc_enum dock_out_hp_volume_up_down_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dock_out_hp_volume_up_down_text),
		dock_out_hp_volume_up_down_text),
};

static const struct soc_enum dock_out_spk_volume_up_down_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dock_out_spk_volume_up_down_text),
		dock_out_spk_volume_up_down_text),
};

static int analog_fade_in_out(struct snd_soc_codec *ptr_codec, int reg,
	int shift, int mask, int target, int step,
	bool is_stereo, bool is_Volume_Up)
{
	int i, init;

	if (mask < target)
		return -1;

#ifdef DOCK_LOG
	pr_info("shift=%d, target=%x, step=%d, is_stereo=%d, is_Volume_Up=%d\n",
		shift, target, step, is_stereo, is_Volume_Up);
#endif

	if (is_Volume_Up)	{
		init = 0;
		for (i = init; i <= target; i += step)	{
			snd_soc_update_bits(ptr_codec, reg, mask, i);
			if (is_stereo)
				snd_soc_update_bits(ptr_codec, reg+1, mask, i);

#ifdef DOCK_LOG
			pr_info("vol=%x,%x\n",
				snd_soc_read(the_codec, reg),
				snd_soc_read(the_codec, reg+1)
				);
#endif
		}
	} else {
		init = target;
		target = 0;
		for (i = init; i >= target; i -= step)	{
			snd_soc_update_bits(ptr_codec, reg, mask, i);
			if (is_stereo)
				snd_soc_update_bits(ptr_codec, reg+1, mask, i);

#ifdef DOCK_LOG
			pr_info("vol=%x,%x\n",
				snd_soc_read(the_codec, reg),
				snd_soc_read(the_codec, reg+1)
				);
#endif
		}
	}

	/* insert last value focily */
	snd_soc_update_bits(ptr_codec, reg, mask, target);
	if (is_stereo)
		snd_soc_update_bits(ptr_codec, reg+1, mask, target);

	return 0;
}

static int get_dock_out_hp_volume_up_down_widget(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return dock_out_hp_volume_up_down;
}

static int set_dock_out_hp_volume_up_down_widget(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int reg;

	dock_out_hp_volume_up_down = ucontrol->value.integer.value[0];

#ifdef DOCK_LOG
	reg = snd_soc_read(the_codec, WM8994_LEFT_OPGA_VOLUME);
	pr_info("WM8994_LEFT_OPGA_VOLUME before fade=%x\n", reg);
#endif

	/* Target volume : 55(-2dB) */
	analog_fade_in_out(the_codec, WM8994_LEFT_OPGA_VOLUME,
	0, WM8994_MIXOUTL_VOL_MASK, DOCK_OUT_HEADPHONE_VOLUME, 7,
	DOCK_OUT_STEREO, dock_out_hp_volume_up_down);

#ifdef DOCK_LOG
	reg = snd_soc_read(the_codec, WM8994_LEFT_OPGA_VOLUME);
	pr_info("WM8994_LEFT_OPGA_VOLUME after fade=%x\n", reg);
#endif

	return 0;
}

static int get_dock_out_spk_volume_up_down_widget(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return dock_out_spk_volume_up_down;
}

static int set_dock_out_spk_volume_up_down_widget(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int reg;

	dock_out_spk_volume_up_down = ucontrol->value.integer.value[0];

#ifdef DOCK_LOG
	reg = snd_soc_read(the_codec, WM8994_SPEAKER_VOLUME_LEFT);
	pr_info("WM8994_SPEAKER_VOLUME_LEFT before fade=%x\n", reg);
#endif

	analog_fade_in_out(the_codec, WM8994_SPEAKER_VOLUME_LEFT,
	0, WM8994_SPKOUTL_VOL_MASK, DOCK_OUT_SPEAKER_VOLUME, 5,
	DOCK_OUT_STEREO, dock_out_spk_volume_up_down);

#ifdef DOCK_LOG
	reg = snd_soc_read(the_codec, WM8994_SPEAKER_VOLUME_LEFT);
	pr_info(" after fade=%x\n", reg);
#endif

	return 0;
}
#endif

static int main_mic_bias_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	/* Harrison feature (use Codec Mic bias) */
	struct snd_soc_codec *codec = w->codec;
	int val = SND_SOC_DAPM_EVENT_ON(event);

	pr_info("main_mic_bias_event val=%x\n", val);

	snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
				   WM8994_MICB1_ENA_MASK, val<<4);
	return 0;
}

/* PM Constraint feature for the VOIP time stamp */
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

static const struct soc_enum playback_lp_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		ARRAY_SIZE(playback_lp_mode_text), playback_lp_mode_text),
};

static int get_playback_lp_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = playback_lp_mode;
	return 0;
}

static int set_playback_lp_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (playback_lp_mode == ucontrol->value.integer.value[0])
		return 0;

	playback_lp_mode = ucontrol->value.integer.value[0];

	pr_info("set playback_lp : %s\n",
		playback_lp_mode_text[playback_lp_mode]);

	return 0;
}

bool is_playback_lpmode_available(void)
{
	return (bool)playback_lp_mode;
}
EXPORT_SYMBOL_GPL(is_playback_lpmode_available);

/**
 * Hidden Register settings for the bias level adjust for wm1811's
 * VMID level re-adjust.
 */
static void omap4_set_wm1811_hidden_register(struct wm8994 *control)
{
	wm8994_reg_write(control, 0x102, 0x3);
	wm8994_reg_write(control, 0xcb, 0x5151);
	wm8994_reg_write(control, 0xd3, 0x3f3f);
	wm8994_reg_write(control, 0xd4, 0x3f3f);
	wm8994_reg_write(control, 0xd5, 0x3f3f);
	wm8994_reg_write(control, 0xd6, 0x3226);
	wm8994_reg_write(control, 0x102, 0x0);
	wm8994_reg_write(control, 0xd1, 0x87);
	wm8994_reg_write(control, 0x3b, 0x9);
	wm8994_reg_write(control, 0x3c, 0x2);

	return;
}

#ifndef CONFIG_SEC_DEV_JACK

/**
 * Microphone detection rates, used to tune response rates and power
 * consumption for WM1811 microphone detection.
 */
static void omap4_micd_set_rate(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	int best, i, sysclk, val;
	bool idle;
	const struct wm8958_micd_rate *rates = NULL;
	int num_rates = 0;

	idle = !wm8994->jack_mic;

	dev_info(codec->dev, "omap4_micd_set_rate\n");

	sysclk = snd_soc_read(codec, WM8994_CLOCKING_1);
	if (sysclk & WM8994_SYSCLK_SRC)
		sysclk = wm8994->aifclk[1];
	else
		sysclk = wm8994->aifclk[0];

	if (wm8994->jackdet) {
		rates = omap4_jackdet_rates;
		num_rates = ARRAY_SIZE(omap4_jackdet_rates);
		wm8994->pdata->micd_rates = omap4_jackdet_rates;
		wm8994->pdata->num_micd_rates = num_rates;
	} else {
		rates = omap4_det_rates;
		num_rates = ARRAY_SIZE(omap4_det_rates);
		wm8994->pdata->micd_rates = omap4_det_rates;
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

/**
 * Detection handler of the mnic bias state
 * consumption for WM1811 microphone detection.
 *
 * @status: Value for MICD status
 * @data: Value for detect data in detail.
 */
static void omap4_micdet(u16 status, void *data)
{
	struct wm1811_machine_priv *wm1811 = data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(wm1811->codec);
	int report;

	wake_lock_timeout(&wm1811->jackdet_wake_lock, 5 * HZ);

	/* Either nothing present or just starting detection */
	if (!(status & WM8958_MICD_STS)) {
		if (!wm8994->jackdet) {
			/* If nothing present then clear our statuses */
			dev_info(wm1811->codec->dev, "Detected open circuit\n");
			wm8994->jack_mic = false;
			wm8994->mic_detecting = true;

			omap4_micd_set_rate(wm1811->codec);

			snd_soc_jack_report(wm8994->micdet[0].jack, 0,
					    wm8994->btn_mask |
					     SND_JACK_HEADSET);
		}
		/*ToDo*/
		/*return;*/
	}

	/*
	 * If the measurement is showing a high impedence we've got a
	 * microphone.
	 * 0x200 -> 0x400
	 */
	if (wm8994->mic_detecting && (status & 0x400)) {
		dev_info(wm1811->codec->dev, "Detected microphone\n");

		wm8994->mic_detecting = false;
		wm8994->jack_mic = true;

		omap4_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADSET,
				    SND_JACK_HEADSET);
	}

	/* Let's think about the impedence later. */
	if (wm8994->mic_detecting && (status & 0x4)) {
		dev_info(wm1811->codec->dev, "Detected headphone\n");
		wm8994->mic_detecting = false;

		omap4_micd_set_rate(wm1811->codec);

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
					WM1811_JACKDET_MODE_AUDIO);
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

#ifdef JACK_DETECT_QUEUE
		if (delayed_work_pending(&wm1811->jack_det_work))
			cancel_delayed_work_sync(&wm1811->jack_det_work);
		queue_delayed_work(wm1811->jack_det_queue,
			&wm1811->jack_det_work,
			msecs_to_jiffies(wm1811->jack_det_delayed_work_time));
#endif
	}

	/* Report short circuit as a button */
	if (wm8994->jack_mic) {
		report = 0;
		if (status & WM1811_JACKDET_BTN0)
			report |= SND_JACK_BTN_0;

		if (status & WM1811_JACKDET_BTN1)
			report |= SND_JACK_BTN_1;

		if (status & WM1811_JACKDET_BTN2)
			report |= SND_JACK_BTN_2;

		dev_info(wm1811->codec->dev, "Detected Button: %08x (%08X)\n",
			report, status);

		snd_soc_jack_report(wm8994->micdet[0].jack, report,
				    wm8994->btn_mask);
	}
}
#endif	/* CONFIG_SEC_DEV_JACK */


#ifdef JACK_DETECT_QUEUE
/* Check Time : DET_CHECK_TIME_MS */
#define DET_CHECK_TIME_MS 300 /* 300ms */
#define DET_SLEEP_TIME_MS 10
#define DET_4POLE 0x400

void jack_detect_work(struct work_struct *work)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(the_codec);

	int time_left_ms = DET_CHECK_TIME_MS;
	int status = 0x0;
	bool is_jack_det = false;

	pr_info("[JACK_DETECT_QUEUE]jack_detect_work\n");

	snd_soc_update_bits(the_codec,
		WM8958_MIC_DETECT_1,
		WM8958_MICD_ENA_MASK, WM8958_MICD_ENA);

	while (time_left_ms > 0) {
		is_jack_det = (snd_soc_read(
			the_codec, WM1811_JACKDET_CTRL) & WM1811_JACKDET_LVL);
		status = snd_soc_read(the_codec, WM8958_MIC_DETECT_3);
		pr_info("is_jack_det=%d, status=%x\n", is_jack_det, status);

		if (is_jack_det) {   /* Jack detected */
			/* Jack detect : OK, 4pole : OK */
			if (status & DET_4POLE) {
				pr_info("[JACK_DETECT_QUEUE]Detected microphone\n");

				wm8994->mic_detecting = false;
				wm8994->jack_mic = true;

				omap4_micd_set_rate(the_codec);

				snd_soc_jack_report(wm8994->micdet[0].jack,
					SND_JACK_HEADSET,
					SND_JACK_HEADSET);

				return;
			}
		} else {   /* Jack not detected */
			pr_info("[JACK_DETECT_QUEUE]Jack not detected\n");

			break;
		}
		msleep(DET_SLEEP_TIME_MS);
		time_left_ms -= DET_SLEEP_TIME_MS;
	}

	snd_soc_update_bits(the_codec,
		WM8958_MIC_DETECT_1,
		WM8958_MICD_ENA_MASK, 0);
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
				  board_mclk, 44100 * 256);
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
	struct snd_soc_codec *codec = codec_dai->codec;
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

	snd_soc_update_bits(codec, WM8994_ANTIPOP_1,
			    WM8994_LINEOUT_VMID_BUF_ENA,
			    WM8994_LINEOUT_VMID_BUF_ENA);

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
	/* The source of fll is actually the mclk. */
	int mclk;

	pr_info("%s: enter\n", __func__);

	prate = params_rate(params);
	switch (prate) {
	/* FM use the 44.1KHz sync clk. */
	case 8000:
	case 16000:
	case 44100:
		break;
	default:
		dev_warn(codec_dai->dev, "Unsupported LRCLK %d, falling back to 8000Hz\n",
			(int)params_rate(params));
		prate = 8000;
	}

	/* Set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	switch (prate) {
	/* mclk is changed to 38.4KHz since Rev 0.6 */
	case 8000:
	case 16000:
	case 44100:
		mclk = board_mclk;
		break;
	default:
		return -EINVAL;
	}

	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
				WM8994_FLL_SRC_MCLK1,
				mclk, prate * 256);

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

	SOC_DAPM_PIN_SWITCH("VMID_OUTPUT"),

	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_ENUM_EXT("HP Output Mode", hp_mode_enum[0],
		get_hp_output_mode, set_hp_output_mode),
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),
	SOC_ENUM_EXT("Mute IC", mute_ic_mode_enum[0],
		get_mute_ic_mode, set_mute_ic_mode),
#ifdef USE_TO_DELAY	/* Additional delay control */
	SOC_ENUM_EXT("Delay", delay_enum[0], get_delay_ms, set_delay_ms),
#endif

#ifdef REDUCE_DOCK_NOISE	/* Additional control : Reduce docking noise */
	SOC_ENUM_EXT("Dock HP Volume", dock_out_hp_volume_up_down_enum[0],
		get_dock_out_hp_volume_up_down_widget,
		set_dock_out_hp_volume_up_down_widget),

	SOC_ENUM_EXT("Dock SPK Volume", dock_out_spk_volume_up_down_enum[0],
		get_dock_out_spk_volume_up_down_widget,
		set_dock_out_spk_volume_up_down_widget),
#endif
	SOC_ENUM_EXT("Playback LP Mode", playback_lp_mode_enum[0],
	get_playback_lp_mode, set_playback_lp_mode),
	/* additional VU ctrl. */
	SOC_DOUBLE_R("Headphone VU", WM8994_LEFT_OUTPUT_VOLUME,
	     WM8994_RIGHT_OUTPUT_VOLUME, 8, 1, 0),
	/* PM Constraint feature for the VOIP time stamp */
	SOC_ENUM_EXT("PM Constraints Mode", pm_mode_enum[0],
		get_pm_mode, set_pm_mode),
	/* additonal AIF2 mute control */
	SOC_ENUM_EXT("FM Mute", fmradio_mute_enum[0],
		get_fmradio_mute_enable, set_fmradio_mute_enable),
};

const struct snd_soc_dapm_widget omap4_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("LINEOUT", NULL),
	SND_SOC_DAPM_MIC("Main Mic", main_mic_bias_event),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),

	SND_SOC_DAPM_INPUT("VMID_INPUT"),
	SND_SOC_DAPM_OUTPUT("VMID_OUTPUT"),
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

	{ "VMID_OUTPUT", NULL, "VMID_INPUT" },
	{ "VMID_OUTPUT", NULL, "VMID" },

	{ "IN1LP", NULL, "Main Mic" },
	{ "IN1LN", NULL, "Main Mic" },

	{ "IN1RP", NULL, "Headset Mic" },
	{ "IN1RN", NULL, "Headset Mic" },
};

#ifndef CONFIG_SEC_DEV_JACK
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

	omap4_micd_set_rate(codec);

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

static ssize_t reselect_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);
	return 0;
}

static ssize_t reselect_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	int reg = 0;

	reg = snd_soc_read(codec, WM8958_MIC_DETECT_3);
	if (reg == 0x402) {
		dev_info(codec->dev, "Detected open circuit\n");

		snd_soc_update_bits(codec, WM8958_MICBIAS2,
				    WM8958_MICB2_DISCH, WM8958_MICB2_DISCH);
		/* Enable debounce while removed */
		snd_soc_update_bits(codec, WM1811_JACKDET_CTRL,
				    WM1811_JACKDET_DB, WM1811_JACKDET_DB);

		wm8994->mic_detecting = false;
		wm8994->jack_mic = false;
		snd_soc_update_bits(codec, WM8958_MIC_DETECT_1,
				    WM8958_MICD_ENA, 0);

		if (wm8994->active_refcount) {
			snd_soc_update_bits(codec,
				WM8994_ANTIPOP_2,
				WM1811_JACKDET_MODE_MASK,
				WM1811_JACKDET_MODE_AUDIO);
		} else {
			snd_soc_update_bits(codec,
				WM8994_ANTIPOP_2,
				WM1811_JACKDET_MODE_MASK,
				WM1811_JACKDET_MODE_JACK);
		}

		snd_soc_jack_report(wm8994->micdet[0].jack, 0,
				    SND_JACK_MECHANICAL | SND_JACK_HEADSET |
				    wm8994->btn_mask);
	}
	return size;
}

static DEVICE_ATTR(reselect_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		reselect_jack_show, reselect_jack_store);

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_key_state_show, earjack_key_state_store);

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_state_show, earjack_state_store);
#endif	/* CONFIG_SEC_DEV_JACK */

/*
 * FSA Switch driver notifies
 * that the dock is inserted.
 */
void notify_dock_status(int status)
{
	if (!the_codec)
		return;

	dock_status = status;

	if (the_codec->suspended)
		return;

	if (status)
		wm8994_vmid_mode(the_codec, WM8994_VMID_FORCE);
	else
		wm8994_vmid_mode(the_codec, WM8994_VMID_NORMAL);

	return;
}

int omap4_wm8994_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	/* For Hidden Register */
	struct wm8994 *control = codec->control_data;
#ifndef CONFIG_SEC_DEV_JACK
	struct wm1811_machine_priv *wm1811;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
#endif

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

	/* Wolfson patch : Modified AIF1CLK to prevent call mute */
	ret = snd_soc_dapm_force_enable_pin(&codec->dapm, "AIF1CLK");
	if (ret < 0)
		dev_err(codec->dev, "Failed to enable AIF1CLK: %d\n", ret);

	snd_soc_dapm_ignore_suspend(dapm, "RCV");
	snd_soc_dapm_ignore_suspend(dapm, "SPK");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT");
	snd_soc_dapm_ignore_suspend(dapm, "HP");
	snd_soc_dapm_ignore_suspend(dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3DACDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1ADCDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF2ADCDAT");
	snd_soc_dapm_ignore_suspend(dapm, "AIF3ADCDAT");
	snd_soc_dapm_ignore_suspend(dapm, "VMID_OUTPUT");

#ifdef CONFIG_SEC_DEV_JACK
	/* By default use idle_bias_off, will override for WM8994 */
	codec->dapm.idle_bias_off = 0;
#else

	/* Getting ready for the jack driver */
	wm1811 = kmalloc(sizeof *wm1811, GFP_KERNEL);
	if (!wm1811) {
		dev_err(codec->dev, "Failed to allocate memory!");
		return -ENOMEM;
	}

	wm1811->codec = codec;

	/*
	* CONFIG_SEC_DEV_JACK
	* Initialize the ALSA jack & button detect here.
	*/
	wm1811->jack.status = 0;

	ret = snd_soc_jack_new(codec, "Omap4 Jack",
				SND_JACK_HEADSET | SND_JACK_BTN_0 |
				SND_JACK_BTN_1 | SND_JACK_BTN_2,
				&wm1811->jack);

	if (ret < 0)
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

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
		ret = wm8958_mic_detect(codec, &wm1811->jack, omap4_micdet,
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
					WAKE_LOCK_SUSPEND, "omap4_jackdet");

#ifdef JACK_DETECT_QUEUE
	/* Jack detect queue */
	wm1811->jack_det_queue = alloc_workqueue("wm8994-codec", 0, 0);

	if (!wm1811->jack_det_queue)
		return -ENOMEM;

	INIT_DELAYED_WORK(&wm1811->jack_det_work, jack_detect_work);

	wm1811->jack_det_delayed_work_time = 2000; /* 2ms */
#endif

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

	if (device_create_file(jack_dev, &dev_attr_reselect_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_reselect_jack.attr.name);
#endif /* CONFIG_SEC_DEV_JACK */

	/* Must re-set the hidden registers for ensure the vmid level. */
	omap4_set_wm1811_hidden_register(control);

	return snd_soc_dapm_sync(dapm);
}

static struct snd_soc_dai_driver ext_dai[] = {
{
	.name = "CP",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 44100,
		.rates = SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_44100,
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

/**
 * omap4_card_resume_post
 * Wolfson patch : Modified AIF1CLK to prevent call mute
 * AIF1CLK ctrl allows to enter to the proper sleep current.
 */
static int omap4_card_resume_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct wm8994 *control = codec->control_data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(the_codec);

	pr_info("omap4_card_resume_post");

	snd_soc_dapm_force_enable_pin(&codec->dapm, "AIF1CLK");
	snd_soc_dapm_sync(&codec->dapm);

	/* Hidden register has to be set every wake from sleep time. */
	omap4_set_wm1811_hidden_register(control);

	/* Enable the thermal detection */
	snd_soc_update_bits(codec,
	WM8994_POWER_MANAGEMENT_2,
	WM8994_TSHUT_ENA_MASK,
	WM8994_TSHUT_ENA);

	if ((dock_status > 0) && (wm8994->vmid_mode == WM8994_VMID_NORMAL)) {
		pr_info("%s: entering normal vmid mode\n", __func__);
		wm8994_vmid_mode(the_codec, WM8994_VMID_FORCE);
	}

	return 0;
}

/**
 * omap4_card_suspend_pre
 * Codec Suspend must be done before the suspend itself (suspend_pre).
 * The following sequences make the proper sleep current.
 */
static int omap4_card_suspend_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(the_codec);
#ifndef CONFIG_SEC_DEV_JACK
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *aif2_dai = card->rtd[1].codec_dai;
	int ret;

	/*
	* There are 4 conditions to achieve the low power mode
	*
	* Jack detect mode is to be less power mode.
	* Thermal detecting function is to be off.
	* LDO1 discharge function is to be off.(Leakage happens.)
	* The MCLK to the CODEC Chip is to be 32kHz.
	*/
	if (!codec->active) {

		pr_info("omap4_card_suspend_post");

		ret = snd_soc_dai_set_sysclk(aif1_dai,
					     WM8994_SYSCLK_MCLK2,
					     OMAP4_DEFAULT_MCLK2,
					     SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(codec->dev, "Unable to switch to MCLK2\n");

		ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL1\n");

		ret = snd_soc_dai_set_sysclk(aif2_dai,
					     WM8994_SYSCLK_MCLK2,
					     OMAP4_DEFAULT_MCLK2,
					     SND_SOC_CLOCK_IN);

		if (ret < 0)
			dev_err(codec->dev, "Unable to switch to MCLK2: %d\n",
				ret);

		ret = snd_soc_dai_set_pll(aif2_dai, WM8994_FLL2, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL2\n");

		snd_soc_update_bits(codec,
		WM8994_ANTIPOP_2,
		WM1811_JACKDET_MODE_MASK,
		WM1811_JACKDET_MODE_MIC);

		snd_soc_update_bits(codec,
		WM8994_LDO_1,
		WM8994_LDO1_DISCH_MASK,
		0);

		snd_soc_update_bits(codec,
		WM8994_POWER_MANAGEMENT_2,
		WM8994_TSHUT_ENA_MASK,
		0);
	}
#endif

	pr_info("omap4_card_suspend_pre");

	if ((dock_status > 0) && (wm8994->vmid_mode == WM8994_VMID_FORCE)) {
		pr_info("%s: entering force vmid mode\n", __func__);
		wm8994_vmid_mode(the_codec, WM8994_VMID_NORMAL);
	}

	/* We can't skip this cause of the sleep current issue. */
	snd_soc_dapm_disable_pin(&codec->dapm, "AIF1CLK");
	snd_soc_dapm_sync(&codec->dapm);

	return 0;
}

static struct snd_soc_card omap4_wm8994 = {
	.name = "omap4_wm8994",
	.dai_link = omap4_dai,
	.num_links = ARRAY_SIZE(omap4_dai),

	.resume_post = omap4_card_resume_post,
	.suspend_pre = omap4_card_suspend_pre
};

static struct platform_device *omap4_wm8994_snd_device;

static int __init omap4_audio_init(void)
{
	int ret;

	mute_ic.gpio = omap_muxtbl_get_gpio_by_name(mute_ic.label);
	if (mute_ic.gpio == -EINVAL)
		return -EINVAL;

	ret = gpio_request(mute_ic.gpio, "mute_ic");
	if (ret < 0)
		goto mute_ic_err;

	gpio_direction_output(mute_ic.gpio, 0);

#ifndef DISABLE_NEEDLESS_GPIO_PIN
	ear_select.gpio = omap_muxtbl_get_gpio_by_name(ear_select.label);
	if (ear_select.gpio == -EINVAL)
		return -EINVAL;

	ret = gpio_request(ear_select.gpio, "ear_select");
	if (ret < 0)
		goto ear_select_err;

	gpio_direction_output(ear_select.gpio, 0);

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
#endif

	/* getting pm qos handle */
	pm_qos_add_request(&pm_qos_handle, PM_QOS_CPU_DMA_LATENCY,
						PM_QOS_DEFAULT_VALUE);

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
#ifndef DISABLE_NEEDLESS_GPIO_PIN
	gpio_free(main_mic_bias.gpio);
main_mic_err:
	gpio_free(ear_select.gpio);
ear_select_err:
	gpio_free(mute_ic.gpio);
#endif
mute_ic_err:
	return ret;
}
module_init(omap4_audio_init);

static void __exit omap4_audio_exit(void)
{
	platform_device_unregister(omap4_wm8994_snd_device);
	/* Freeing pm qos handle */
	pm_qos_remove_request(&pm_qos_handle);
}
module_exit(omap4_audio_exit);

MODULE_AUTHOR("Quartz.Jang <quartz.jang@samsung.com");
MODULE_DESCRIPTION("ALSA Soc WM8994 omap4");
MODULE_LICENSE("GPL");

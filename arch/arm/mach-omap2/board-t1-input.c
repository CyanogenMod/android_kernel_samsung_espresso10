/* arch/arm/mach-omap2/board-t1-input.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd
 *
 * Based on mach-omap2/board-tuna-input.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/keyreset.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <linux/i2c/mxt224_t1.h>
#include <asm/mach-types.h>
#include <plat/omap4-keypad.h>
#include <linux/input/cypress-touchkey.h>

#include "board-t1.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include <linux/usb/otg.h>
#include <linux/notifier.h>

#define TA_CHARGER_CONNECT      1
#define TA_CHARGER_DISCONNECT   0

static void mxt_power_on(int enable);

static struct charging_status_callbacks {
	void (*tsp_set_charging_cable) (int type);
	struct work_struct noise_threshold_work;
	bool ta_status;
	struct otg_transceiver *transceiver;
	struct notifier_block otg_nb;
} charging_cbs;

enum {
	GPIO_EXT_WAKEUP = 0,
};

static struct gpio keys_map_high_gpios[] __initdata = {
	[GPIO_EXT_WAKEUP] = {
		.label	= "EXT_WAKEUP",
	},
};

static struct gpio_event_direct_entry t1_gpio_keypad_keys_map_high[] = {
	[GPIO_EXT_WAKEUP] = {
		.code	= KEY_POWER,
	},
};

static struct gpio_event_input_info t1_gpio_keypad_keys_info_high = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= t1_gpio_keypad_keys_map_high,
	.keymap_size		= ARRAY_SIZE(t1_gpio_keypad_keys_map_high),
	.flags			= GPIOEDF_ACTIVE_HIGH,
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

enum {
	GPIO_VOL_UP = 0,
	GPIO_VOL_DOWN,
	GPIO_HOME_KEY
};

static struct gpio keys_map_low_gpios[] __initdata = {
	[GPIO_VOL_UP] = {
		.label	= "VOL_UP",
	},
	[GPIO_VOL_DOWN] = {
		.label	= "VOL_DOWN",
	},
	[GPIO_HOME_KEY] = {
		.label	= "HOME_KEY"
	},
};

static struct gpio_event_direct_entry t1_gpio_keypad_keys_map_low[] = {
	[GPIO_VOL_UP] = {
		.code	= KEY_VOLUMEUP,
	},
	[GPIO_VOL_DOWN] = {
		.code	= KEY_VOLUMEDOWN,
	},
	[GPIO_HOME_KEY] = {
		.code	= KEY_HOME,
	},
};

static struct gpio_event_input_info t1_gpio_keypad_keys_info_low = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= t1_gpio_keypad_keys_map_low,
	.keymap_size		= ARRAY_SIZE(t1_gpio_keypad_keys_map_low),
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

static struct gpio_event_info *t1_gpio_keypad_info[] = {
	&t1_gpio_keypad_keys_info_high.info,
	&t1_gpio_keypad_keys_info_low.info,
};

static struct gpio_event_platform_data t1_gpio_keypad_data = {
	.name		= "sec_key",
	.info		= t1_gpio_keypad_info,
	.info_count	= ARRAY_SIZE(t1_gpio_keypad_info)
};

static struct platform_device t1_gpio_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= 0,
	.dev = {
		.platform_data	= &t1_gpio_keypad_data,
	},
};

enum {
	GPIO_TOUCH_nINT = 0,
	GPIO_TOUCH_EN
};

static struct gpio tsp_gpios[] = {
	[GPIO_TOUCH_nINT] = {
		.flags	= GPIOF_IN,
		.label	= "TOUCH_nINT",
	},
	[GPIO_TOUCH_EN] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "TOUCH_EN",
	},
};

enum {
	GPIO_TOUCHKEY_EN = 0,
	GPIO_TOUCHKEY_LED_EN,
	GPIO_TOUCHKEY_INT
};

static struct gpio tk_gpios[] = {
	[GPIO_TOUCHKEY_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "3_TOUCH_EN",
	},
	[GPIO_TOUCHKEY_LED_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "3_TOUCH_LED_EN",
	},
	[GPIO_TOUCHKEY_INT] = {
		.flags	= GPIOF_IN,
		.label	= "3_TOUCH_INT",
	},
};

/*atmel_mxt224E*/
static void mxt224_power_on(void)
{
	mxt_power_on(1);
}

static void mxt224_power_off(void)
{
	mxt_power_on(0);
}

static void mxt224_ta_work(struct work_struct *wq)
{
	if (charging_cbs.tsp_set_charging_cable)
		charging_cbs.tsp_set_charging_cable(charging_cbs.ta_status);
}

static int otg_handle_notification(struct notifier_block *nb,
	unsigned long event, void *unused)
{
	switch (event) {
	case USB_EVENT_VBUS:
	case USB_EVENT_CHARGER:
		charging_cbs.ta_status = TA_CHARGER_CONNECT;
		break;
	case USB_EVENT_NONE:
		charging_cbs.ta_status = TA_CHARGER_DISCONNECT;
		break;
	default:
		return NOTIFY_OK;
	}
	if (charging_cbs.transceiver)
		schedule_work(&charging_cbs.noise_threshold_work);
	return NOTIFY_OK;
}

static void mxt224_register_callback(void *function)
{
	int ret;
	charging_cbs.tsp_set_charging_cable = function;
	INIT_WORK(&charging_cbs.noise_threshold_work, mxt224_ta_work);
	charging_cbs.transceiver = otg_get_transceiver();
	if (charging_cbs.transceiver) {
		charging_cbs.otg_nb.notifier_call = otg_handle_notification;
		ret = otg_register_notifier
		(charging_cbs.transceiver, &charging_cbs.otg_nb);
		if (ret) {
			pr_err("%s failure to register otg notifier\n"
			, __func__);
			if (charging_cbs.transceiver)
				otg_put_transceiver(charging_cbs.transceiver);
		} else if (charging_cbs.transceiver->last_event ==
		USB_EVENT_NONE) {
			charging_cbs.ta_status = TA_CHARGER_DISCONNECT;
		}
	} else {
		pr_err("%s failure to get otg transceiver\n", __func__);
	}
}

static void mxt224_unregister_callback(void)
{
	cancel_work_sync(&charging_cbs.noise_threshold_work);
	if (charging_cbs.transceiver)
		otg_put_transceiver(charging_cbs.transceiver);
}

static void mxt224_read_ta_status(bool *ta_status)
{
	*ta_status = charging_cbs.ta_status;
}

#define MXT224_MAX_MT_FINGERS		10

/*
    Configuration for MXT224
*/
#define MXT224_THRESHOLD_BATT		40
#define MXT224_THRESHOLD_BATT_INIT	55
#define MXT224_THRESHOLD_CHRG		70
#define MXT224_NOISE_THRESHOLD_BATT	30
#define MXT224_NOISE_THRESHOLD_CHRG	40
#define MXT224_MOVFILTER_BATT		11
#define MXT224_MOVFILTER_CHRG		46
#define MXT224_ATCHCALST		4
#define MXT224_ATCHCALTHR		35

static u8 t7_config[] = { GEN_POWERCONFIG_T7, 48, 255, 25 };
static u8 t8_config[] = { GEN_ACQUISITIONCONFIG_T8, 10, 0, 5, 1, 0, 0,
			  MXT224_ATCHCALST, MXT224_ATCHCALTHR };
static u8 t9_config[] = { TOUCH_MULTITOUCHSCREEN_T9, 131, 0, 0, 19, 11, 0, 32,
			  MXT224_THRESHOLD_BATT, 2, 1, 0, 15, 1,
			  MXT224_MOVFILTER_BATT, MXT224_MAX_MT_FINGERS, 5, 40,
			  10, 31, 3, 223, 1, 0, 0, 0, 0, 143, 55, 143, 90, 18 };
static u8 t18_config[] = { SPT_COMCONFIG_T18, 0, 1 };
static u8 t20_config[] = { PROCI_GRIPFACESUPPRESSION_T20, 7, 0, 0, 0, 0, 0, 0,
			   30, 20, 4, 15, 10 };
static u8 t22_config[] = { PROCG_NOISESUPPRESSION_T22, 143, 0, 0, 0, 0, 0, 0, 3,
			   MXT224_NOISE_THRESHOLD_BATT, 0, 0, 29, 34, 39, 49,
			   58, 3 };
static u8 t28_config[] = { SPT_CTECONFIG_T28, 0, 0, 3, 16, 19, 60 };
static u8 end_config[] = { RESERVED_T255 };

static const u8 *mxt224_config[] = {
	t7_config,
	t8_config,
	t9_config,
	t18_config,
	t20_config,
	t22_config,
	t28_config,
	end_config,
};

/*
    Configuration for MXT224-E
*/

#define MXT224_CHRGTIME				22

#define MXT224E_ATCHFRCCALTHR_NORMAL		40
#define MXT224E_ATCHFRCCALRATIO_NORMAL		55

#define MXT224E_BLEN_BATT			0
#define MXT224E_BLEN_BATT_ERR			0
#define MXT224E_BLEN_CHRG			0
#define MXT224E_BLEN_CHRG_ERR2			0

#define MXT224E_THRESHOLD_BATT			35
#define MXT224E_THRESHOLD_BATT_ERR		40
#define MXT224E_THRESHOLD_CHRG			40
#define MXT224E_THRESHOLD_CHRG_ERR1		45
#define MXT224E_THRESHOLD_CHRG_ERR2		45

#define MXT224E_MOVFILTER_BATT			46
#define MXT224E_MOVFILTER_BATT_ERR		81
#define MXT224E_MOVFILTER_CHRG			47
#define MXT224E_MOVFILTER_CHRG_ERR1		80
#define MXT224E_MOVFILTER_CHRG_ERR2		65

#define MXT224E_XEDGECTRL_BATT			160
#define MXT224E_XEDGECTRL_BATT_ERR		160
#define MXT224E_XEDGECTRL_CHRG			160

#define MXT224E_XEDGEDIST_BATT			50
#define MXT224E_XEDGEDIST_BATT_ERR		50
#define MXT224E_XEDGEDIST_CHRG			50

#define MXT224E_TCHHYST_BATT			15
#define MXT224E_TCHHYST_CHRG			10

#define MXT224E_NEXTTCHDI_BATT			0
#define MXT224E_NEXTTCHDI_BATT_ERR		0
#define MXT224E_NEXTTCHDI_CHRG			0

#define MXT224E_ACTVSYNCSPERX			32

#define MXT224E_CALCFG_BATT			114
#define MXT224E_CALCFG_BATT_ERR			114
#define MXT224E_CALCFG_CHRG			114

#define MXT224E_BASEPREQ_BATT			20
#define MXT224E_BASEPREQ_BATT_ERR		20
#define MXT224E_BASEPREQ_CHRG			0
#define MXT224E_BASEPREQ_CHRG_ERR2		10

#define MXT224E_MFFREQ0_BATT			1
#define MXT224E_MFFREQ0_BATT_ERR		1
#define MXT224E_MFFREQ0_CHRG			15
#define MXT224E_MFFREQ0_CHRG_ERR2		0

#define MXT224E_MFFREQ1_BATT			2
#define MXT224E_MFFREQ1_BATT_ERR		2
#define MXT224E_MFFREQ1_CHRG			15
#define MXT224E_MFFREQ1_CHRG_ERR2		0

#define MXT224E_GCMAXADCSPERX_BATT		48
#define MXT224E_GCMAXADCSPERX_BATT_ERR		100
#define MXT224E_GCMAXADCSPERX_CHRG		64

#define MXT224E_GCLIMITMAX_BATT			48
#define MXT224E_GCLIMITMAX_BATT_ERR		64
#define MXT224E_GCLIMITMAX_CHRG			64

#define MXT224E_MFINVLDDIFFTHR_BATT		10
#define MXT224E_MFINVLDDIFFTHR_BATT_ERR		100
#define MXT224E_MFINVLDDIFFTHR_CHRG		9
#define MXT224E_MFINVLDDIFFTHR_CHRG_ERR1	12
#define MXT224E_MFINVLDDIFFTHR_CHRG_ERR2	100

#define MXT224E_MFERRORTHR0_BATT		20
#define MXT224E_MFERRORTHR0_BATT_ERR		100
#define MXT224E_MFERRORTHR0_CHRG		15
#define MXT224E_MFERRORTHR0_CHRG_ERR1		19
#define MXT224E_MFERRORTHR0_CHRG_ERR2		100

#define MXT224E_XLOCLIP_BATT			235
#define MXT224E_XLOCLIP_BATT_ERR		235
#define MXT224E_XLOCLIP_CHRG			235

#define MXT224E_XHICLIP_BATT			235
#define MXT224E_XHICLIP_BATT_ERR		235
#define MXT224E_XHICLIP_CHRG			235

static u8 t7_config_e[] = { GEN_POWERCONFIG_T7, 48, 255, 25 };
static u8 t8_config_e[] = { GEN_ACQUISITIONCONFIG_T8, MXT224_CHRGTIME,
			    0, 5, 1, 0, 0, MXT224_ATCHCALST, MXT224_ATCHCALTHR,
			    MXT224E_ATCHFRCCALTHR_NORMAL,
			    MXT224E_ATCHFRCCALRATIO_NORMAL };
static u8 t9_config_e[] = { TOUCH_MULTITOUCHSCREEN_T9, 139, 0, 0, 19, 11, 0,
			    MXT224E_BLEN_BATT, MXT224E_THRESHOLD_BATT, 2, 1,
			    10, 15, 1, MXT224E_MOVFILTER_BATT,
			    MXT224_MAX_MT_FINGERS, 5, 40, 10, 31, 3, 223, 1,
			    MXT224E_XLOCLIP_BATT, MXT224E_XHICLIP_BATT, 10, 10,
			    MXT224E_XEDGECTRL_BATT, MXT224E_XEDGEDIST_BATT, 143,
			    80, 18, MXT224E_TCHHYST_BATT, 50, 50,
			    MXT224E_NEXTTCHDI_BATT };
static u8 t15_config_e[] = { TOUCH_KEYARRAY_T15, 0, 0,
			     0, 0, 0, 0, 0, 0, 0, 0, 0 };
static u8 t18_config_e[] = { SPT_COMCONFIG_T18, 0, 0 };
static u8 t19_config_e[] = { SPT_GPIOPWM_T19, 0, 0, 0, 0, 0,
			     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static u8 t23_config_e[] = { TOUCH_PROXIMITY_T23, 0, 0, 0, 0,
			     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static u8 t25_config_e[] = { SPT_SELFTEST_T25, 0, 0, 0, 0, 0,
			     0, 0, 0, 0, 0, 0, 0, 0, 0 };
static u8 t40_config_e[] = { PROCI_GRIPSUPPRESSION_T40, 0, 0, 0, 0, 0 };
static u8 t42_config_e[] = { PROCI_TOUCHSUPPRESSION_T42,
			     0, 0, 0, 0, 0, 0, 0, 0 };
static u8 t46_config_e[] = { SPT_CTECONFIG_T46, 0, 3, 16,
			     MXT224E_ACTVSYNCSPERX, 0, 0, 1, 0 };
static u8 t47_config_e[] = { PROCI_STYLUS_T47, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static u8 t48_config_e[] = { PROCG_NOISESUPPRESSION_T48, 3, 132,
			     MXT224E_CALCFG_BATT, MXT224E_BASEPREQ_BATT, 0, 0,
			     0, 0, MXT224E_MFFREQ0_BATT, MXT224E_MFFREQ1_BATT,
			     0, 0, 0, 6, 6, 0, 0, MXT224E_GCMAXADCSPERX_BATT, 4,
			     MXT224E_GCLIMITMAX_BATT, 10, 0,
			     MXT224E_MFINVLDDIFFTHR_BATT, 5, 0,
			     MXT224E_MFERRORTHR0_BATT, 0, 5, 0, 0, 0, 0, 0, 0,
			     MXT224E_BLEN_BATT, MXT224E_THRESHOLD_BATT, 2, 15,
			     1, MXT224E_MOVFILTER_BATT, MXT224_MAX_MT_FINGERS,
			     5, 40, MXT224E_XLOCLIP_BATT, MXT224E_XHICLIP_BATT,
			     10, 10, MXT224E_XEDGECTRL_BATT,
			     MXT224E_XEDGEDIST_CHRG, 143, 80, 18,
			     MXT224E_TCHHYST_BATT, MXT224E_NEXTTCHDI_BATT };
static u8 t48_config_batt_err_e[] = { PROCG_NOISESUPPRESSION_T48, 3, 132,
			     MXT224E_CALCFG_BATT_ERR,
			     MXT224E_BASEPREQ_BATT_ERR, 0, 0, 0, 0,
			     MXT224E_MFFREQ0_BATT_ERR,
			     MXT224E_MFFREQ1_BATT_ERR, 0, 0, 0, 6, 6, 0, 0,
			     MXT224E_GCMAXADCSPERX_BATT_ERR, 4,
			     MXT224E_GCLIMITMAX_BATT_ERR, 10, 0,
			     MXT224E_MFINVLDDIFFTHR_BATT_ERR, 5, 0,
			     MXT224E_MFERRORTHR0_BATT_ERR, 0, 5, 0, 0, 0, 0, 0,
			     0, MXT224E_BLEN_BATT_ERR,
			     MXT224E_THRESHOLD_BATT_ERR, 2, 15, 1,
			     MXT224E_MOVFILTER_BATT_ERR, MXT224_MAX_MT_FINGERS,
			     5, 40, MXT224E_XLOCLIP_BATT_ERR,
			     MXT224E_XHICLIP_BATT_ERR,
			     10, 10, MXT224E_XEDGECTRL_BATT_ERR,
			     MXT224E_XEDGEDIST_BATT_ERR, 143, 80, 18,
			     MXT224E_TCHHYST_BATT, MXT224E_NEXTTCHDI_BATT };
static u8 t48_config_chrg_e[] = { PROCG_NOISESUPPRESSION_T48, 3, 132,
			     MXT224E_CALCFG_CHRG, MXT224E_BASEPREQ_CHRG, 0, 0,
			     0, 0, MXT224E_MFFREQ0_CHRG, MXT224E_MFFREQ1_CHRG,
			     0, 0, 0, 6, 6, 0, 0, MXT224E_GCMAXADCSPERX_CHRG, 4,
			     MXT224E_GCLIMITMAX_CHRG, 10, 0,
			     MXT224E_MFINVLDDIFFTHR_CHRG, 5, 0,
			     MXT224E_MFERRORTHR0_CHRG, 0, 5, 0, 0, 0, 0, 0, 0,
			     MXT224E_BLEN_CHRG, MXT224E_THRESHOLD_CHRG, 2, 15,
			     1, MXT224E_MOVFILTER_CHRG, MXT224_MAX_MT_FINGERS,
			     5, 40, MXT224E_XLOCLIP_CHRG, MXT224E_XHICLIP_CHRG,
			     10, 10, MXT224E_XEDGECTRL_CHRG,
			     MXT224E_XEDGEDIST_CHRG, 143, 80, 18,
			     MXT224E_TCHHYST_CHRG, MXT224E_NEXTTCHDI_CHRG };
static u8 t48_config_chrg_err1_e[] = { PROCG_NOISESUPPRESSION_T48, 3, 132,
			     MXT224E_CALCFG_CHRG, MXT224E_BASEPREQ_CHRG, 0, 0,
			     0, 0, MXT224E_MFFREQ0_CHRG, MXT224E_MFFREQ1_CHRG,
			     0, 0, 0, 6, 6, 0, 0, MXT224E_GCMAXADCSPERX_CHRG, 4,
			     MXT224E_GCLIMITMAX_CHRG, 10, 0,
			     MXT224E_MFINVLDDIFFTHR_CHRG_ERR1, 5, 0,
			     MXT224E_MFERRORTHR0_CHRG_ERR1, 0, 5, 0, 0, 0, 0,
			     0, 0, MXT224E_BLEN_CHRG,
			     MXT224E_THRESHOLD_CHRG_ERR1, 2, 15, 1,
			     MXT224E_MOVFILTER_CHRG_ERR1,
			     MXT224_MAX_MT_FINGERS, 5, 40,
			     MXT224E_XLOCLIP_CHRG, MXT224E_XHICLIP_CHRG, 10,
			     10, MXT224E_XEDGECTRL_CHRG,
			     MXT224E_XEDGEDIST_CHRG, 143, 80, 18,
			     MXT224E_TCHHYST_CHRG, MXT224E_NEXTTCHDI_CHRG };
static u8 t48_config_chrg_err2_e[] = { PROCG_NOISESUPPRESSION_T48, 3, 132,
			     MXT224E_CALCFG_CHRG, MXT224E_BASEPREQ_CHRG_ERR2,
			     0, 0, 0, 0, MXT224E_MFFREQ0_CHRG_ERR2,
			     MXT224E_MFFREQ1_CHRG_ERR2, 0, 0, 0, 6, 6, 0, 0,
			     MXT224E_GCMAXADCSPERX_CHRG, 4,
			     MXT224E_GCLIMITMAX_CHRG, 10, 0,
			     MXT224E_MFINVLDDIFFTHR_CHRG_ERR2, 5, 0,
			     MXT224E_MFERRORTHR0_CHRG_ERR2, 0, 5, 0, 0, 0, 0,
			     0, 0, MXT224E_BLEN_CHRG_ERR2,
			     MXT224E_THRESHOLD_CHRG_ERR2, 2, 15, 1,
			     MXT224E_MOVFILTER_CHRG_ERR2, MXT224_MAX_MT_FINGERS,
			     5, 40, MXT224E_XLOCLIP_CHRG, MXT224E_XHICLIP_CHRG,
			     10, 10, MXT224E_XEDGECTRL_CHRG,
			     MXT224E_XEDGEDIST_CHRG, 143, 80, 18,
			     MXT224E_TCHHYST_CHRG, MXT224E_NEXTTCHDI_CHRG };
static u8 end_config_e[] = { RESERVED_T255 };

static const u8 *mxt224e_config[] = {
	t7_config_e,
	t8_config_e,
	t9_config_e,
	t15_config_e,
	t18_config_e,
	t19_config_e,
	t23_config_e,
	t25_config_e,
	t40_config_e,
	t42_config_e,
	t46_config_e,
	t47_config_e,
	t48_config_e,
	end_config_e,
};

static struct mxt224_platform_data mxt224_data = {
	.max_finger_touches	= MXT224_MAX_MT_FINGERS,
	.config			= mxt224_config,
	.config_e		= mxt224e_config,
	.min_x			= 0,
	.max_x			= 479,
	.min_y			= 0,
	.max_y			= 799,
	.min_z			= 0,
	.max_z			= 255,
	.min_w			= 0,
	.max_w			= 30,
	.atchcalst		= MXT224_ATCHCALST,
	.atchcalsthr		= MXT224_ATCHCALTHR,
	.tchthr_batt		= MXT224_THRESHOLD_BATT,
	.tchthr_batt_init	= MXT224_THRESHOLD_BATT_INIT,
	.tchthr_charging	= MXT224_THRESHOLD_CHRG,
	.noisethr_batt		= MXT224_NOISE_THRESHOLD_BATT,
	.noisethr_charging	= MXT224_NOISE_THRESHOLD_CHRG,
	.movfilter_batt		= MXT224_MOVFILTER_BATT,
	.movfilter_charging	= MXT224_MOVFILTER_CHRG,
	.atchfrccalthr_e	= MXT224E_ATCHFRCCALTHR_NORMAL,
	.atchfrccalratio_e	= MXT224E_ATCHFRCCALRATIO_NORMAL,
	.t48_config_batt_e	= t48_config_e,
	.t48_config_batt_err_e	= t48_config_batt_err_e,
	.t48_config_chrg_e	= t48_config_chrg_e,
	.t48_config_chrg_err1_e	= t48_config_chrg_err1_e,
	.t48_config_chrg_err2_e	= t48_config_chrg_err2_e,
	.power_on		= mxt224_power_on,
	.power_off		= mxt224_power_off,
	.register_cb		= mxt224_register_callback,
	.read_ta_status		= mxt224_read_ta_status,
	.unregister_cb		= mxt224_unregister_callback,
};

static void mxt_power_on(int enable)
{
	struct regulator *regulator;
	regulator = regulator_get(NULL, "VTI_1.8V");
	if (IS_ERR_OR_NULL(regulator)) {
		pr_err("(%s): failed to get VAUX2 regulator.\n", __func__);
		return;
	}

	if (enable) {
		regulator_enable(regulator);
		if (likely(system_rev >= 4)) {
			gpio_direction_output(tsp_gpios[GPIO_TOUCH_EN].gpio,
					      1);
			msleep(100);
		}
	} else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		if (likely(system_rev >= 4)) {
			gpio_direction_output(tsp_gpios[GPIO_TOUCH_EN].gpio,
					      0);
		}
	}
	regulator_put(regulator);
}

static struct i2c_board_info __initdata t1_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO(MXT224_DEV_NAME, 0x4A),
		.platform_data = &mxt224_data,
		.flags		= I2C_CLIENT_WAKE,
	},
};

static void tk_power_on(int enable)
{
	struct regulator *regulator;
	regulator = regulator_get(NULL, "vaux2");
	if (IS_ERR_OR_NULL(regulator)) {
		pr_err("(%s): failed to get VAUX2 regulator.\n", __func__);
		return;
	}

	if (enable) {
		regulator_enable(regulator);
		gpio_direction_output(tk_gpios[GPIO_TOUCHKEY_EN].gpio,
				1);
		gpio_direction_output(tk_gpios[GPIO_TOUCHKEY_LED_EN].gpio,
				1);
		msleep(50);
	} else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		gpio_direction_output(tk_gpios[GPIO_TOUCHKEY_EN].gpio,
				0);
		gpio_direction_output(tk_gpios[GPIO_TOUCHKEY_LED_EN].gpio,
				0);
	}
	regulator_put(regulator);
}

static const int tk_keymap[] = { 0, KEY_MENU, KEY_BACK, };

#define TOUCH_MODULE_V01    0x01
#define TOUCH_FIRMWARE_V0B  0x0B
static struct cptk_platform_data cptk_data = {
	.power		= tk_power_on,
	.mod_ver	= TOUCH_MODULE_V01,
	.firm_ver	= TOUCH_FIRMWARE_V0B,
	.keymap		= tk_keymap,
	.keymap_size	= ARRAY_SIZE(tk_keymap),
	.fw_name = "fw_cypress_t1.bin"
};

static struct i2c_board_info __initdata t1_tk_boardinfo[] = {
	{
		I2C_BOARD_INFO("cypress_touchkey", 0x20),
		.platform_data = &cptk_data,
	},
};

ssize_t sec_key_pressed_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	unsigned int key_press_status = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(t1_gpio_keypad_keys_map_high); i++) {
		if (unlikely
		    (t1_gpio_keypad_keys_map_high[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		    ((gpio_get_value(t1_gpio_keypad_keys_map_high[i].gpio)
		      << i));
	}

	for (i = 0; i < ARRAY_SIZE(t1_gpio_keypad_keys_map_low); i++) {
		if (unlikely
		    (t1_gpio_keypad_keys_map_low[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		    ((!gpio_get_value(t1_gpio_keypad_keys_map_low[i].gpio)
		      << (i + ARRAY_SIZE(t1_gpio_keypad_keys_map_high))));
	}

	return sprintf(buf, "%u\n", key_press_status);
}

static DEVICE_ATTR(sec_key_pressed, S_IRUGO, sec_key_pressed_show, NULL);

static int t1_create_sec_key_dev(void)
{
	struct device *sec_key;
	sec_key = device_create(sec_class, NULL, 0, NULL, "sec_key");
	if (!sec_key) {
		pr_err("Failed to create sysfs(sec_key)!\n");
		return -ENOMEM;
	}

	if (device_create_file(sec_key, &dev_attr_sec_key_pressed) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_sec_key_pressed.attr.name);

	return 0;
}

static void __init t1_gpio_keypad_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(keys_map_high_gpios); i++)
		t1_gpio_keypad_keys_map_high[i].gpio =
			omap_muxtbl_get_gpio_by_name(
				keys_map_high_gpios[i].label);

	for (i = 0; i < ARRAY_SIZE(keys_map_low_gpios); i++)
		t1_gpio_keypad_keys_map_low[i].gpio =
			omap_muxtbl_get_gpio_by_name(
				keys_map_low_gpios[i].label);
}

static void __init t1_tsp_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tsp_gpios); i++)
		tsp_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(tsp_gpios[i].label);
	gpio_request_array(tsp_gpios, ARRAY_SIZE(tsp_gpios));

	mxt224_data.gpio_read_done = tsp_gpios[GPIO_TOUCH_nINT].gpio;

	t1_i2c3_boardinfo[0].irq =
		gpio_to_irq(tsp_gpios[GPIO_TOUCH_nINT].gpio);
}

static void __init t1_tk_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tk_gpios); i++)
		tk_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(tk_gpios[i].label);
	gpio_request_array(tk_gpios, ARRAY_SIZE(tk_gpios));

	t1_tk_boardinfo[0].irq =
		gpio_to_irq(tk_gpios[GPIO_TOUCHKEY_INT].gpio);
	cptk_data.gpio = tk_gpios[GPIO_TOUCHKEY_INT].gpio;
	cptk_data.scl_pin = omap_muxtbl_get_gpio_by_name("3-TOUCHKEY_SCL");
	cptk_data.sda_pin = omap_muxtbl_get_gpio_by_name("3-TOUCHKEY_SDA");
	cptk_data.en_pin = tk_gpios[GPIO_TOUCHKEY_EN].gpio;
	cptk_data.en_led_pin = tk_gpios[GPIO_TOUCHKEY_LED_EN].gpio;
}

void __init omap4_t1_input_init(void)
{
	t1_gpio_keypad_gpio_init();
	t1_tsp_gpio_init();
	t1_tk_gpio_init();

	if (system_rev >= 4) {
		i2c_register_board_info(3, t1_i2c3_boardinfo,
				ARRAY_SIZE(t1_i2c3_boardinfo));
		i2c_register_board_info(6, t1_tk_boardinfo,
				ARRAY_SIZE(t1_tk_boardinfo));
	} else {
		i2c_register_board_info(3, t1_i2c3_boardinfo,
				ARRAY_SIZE(t1_i2c3_boardinfo));
		i2c_register_board_info(3, t1_tk_boardinfo,
				ARRAY_SIZE(t1_tk_boardinfo));
	}

	t1_create_sec_key_dev();

	platform_device_register(&t1_gpio_keypad_device);
}

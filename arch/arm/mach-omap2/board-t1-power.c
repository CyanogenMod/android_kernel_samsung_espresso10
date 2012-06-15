/* arch/arm/mach-omap2/board-t1-power.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-tuna-power.c
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
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/twl6030-madc.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/max17040_battery.h>
#include <linux/moduleparam.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>

#include <plat/cpu.h>
#include <plat/omap-pm.h>

#include "board-t1.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "pm.h"
#include "sec_common.h"

#define ADC_NUM_SAMPLES			5
#define ADC_LIMIT_ERR_COUNT		5
#define ISET_ADC_CHANNEL		4
#define TEMP_ADC_CHANNEL		1

#define CHARGE_FULL_ADC			388

#define HIGH_BLOCK_TEMP_T1		650
#define HIGH_RECOVER_TEMP_T1		430
#define LOW_BLOCK_TEMP_T1		(-3)
#define LOW_RECOVER_TEMP_T1		0

/**
** temp_adc_table_data
** @adc_value : thermistor adc value
** @temperature : temperature(C) * 10
**/
struct temp_adc_table_data {
	int adc_value;
	int temperature;
};

static DEFINE_SPINLOCK(charge_en_lock);
static int charger_state;
static bool is_charging_mode;

static struct temp_adc_table_data temper_table_t1[] = {
	/* ADC, Temperature (C/10) */
	{70, 700},
	{72, 690},
	{75, 680},
	{77, 670},
	{80, 660},
	{82, 650},
	{85, 640},
	{88, 630},
	{91, 620},
	{94, 610},
	{97, 600},
	{101, 590},
	{104, 580},
	{108, 570},
	{111, 560},
	{115, 550},
	{119, 540},
	{124, 530},
	{129, 520},
	{133, 510},
	{137, 500},
	{141, 490},
	{146, 480},
	{150, 470},
	{155, 460},
	{160, 450},
	{166, 440},
	{171, 430},
	{177, 420},
	{184, 410},
	{190, 400},
	{196, 390},
	{203, 380},
	{212, 370},
	{219, 360},
	{226, 350},
	{234, 340},
	{242, 330},
	{250, 320},
	{258, 310},
	{266, 300},
	{275, 290},
	{284, 280},
	{294, 270},
	{303, 260},
	{312, 250},
	{322, 240},
	{333, 230},
	{344, 220},
	{354, 210},
	{364, 200},
	{375, 190},
	{387, 180},
	{399, 170},
	{410, 160},
	{422, 150},
	{431, 140},
	{443, 130},
	{456, 120},
	{468, 110},
	{480, 100},
	{493, 90},
	{506, 80},
	{519, 70},
	{532, 60},
	{545, 50},
	{558, 40},
	{571, 30},
	{582, 20},
	{595, 10},
	{608, 0},
	{620, (-10)},
	{632, (-20)},
	{645, (-30)},
	{658, (-40)},
	{670, (-50)},
	{681, (-60)},
	{696, (-70)},
	{708, (-80)},
	{720, (-90)},
	{732, (-100)},
};

static struct temp_adc_table_data *temper_table = temper_table_t1;
static int temper_table_size = ARRAY_SIZE(temper_table_t1);

static bool enable_sr = true;
module_param(enable_sr, bool, S_IRUSR | S_IRGRP | S_IROTH);

enum {
	GPIO_CHG_ING_N = 0,
	GPIO_TA_nCONNECTED,
	GPIO_CHG_EN
};

static struct gpio charger_gpios[] = {
	[GPIO_CHG_ING_N] = {
		.flags = GPIOF_IN,
		.label = "CHG_ING_N"
	},
	[GPIO_TA_nCONNECTED] = {
		.flags = GPIOF_IN,
		.label = "TA_nCONNECTED"
	},
	[GPIO_CHG_EN] = {
		.flags = GPIOF_OUT_INIT_HIGH,
		.label = "CHG_EN"
	},
};

static int twl6030_get_adc_data(int ch)
{
	int adc_data;
	int adc_max = -1;
	int adc_min = 1 << 11;
	int adc_total = 0;
	int i, j;

	for (i = 0; i < ADC_NUM_SAMPLES; i++) {
		adc_data = twl6030_get_madc_conversion(ch);
		if (adc_data == -EAGAIN) {
			for (j = 0; j < ADC_LIMIT_ERR_COUNT; j++) {
				msleep(20);
				adc_data = twl6030_get_madc_conversion(ch);
				if (adc_data > 0)
					break;
			}
			if (j >= ADC_LIMIT_ERR_COUNT) {
				pr_err("%s: Retry count exceeded[ch:%d]\n",
				       __func__, ch);
				return adc_data;
			}
		} else if (adc_data < 0) {
			pr_err("%s: Failed read adc value : %d [ch:%d]\n",
			       __func__, adc_data, ch);
			return adc_data;
		}

		if (adc_data > adc_max)
			adc_max = adc_data;
		if (adc_data < adc_min)
			adc_min = adc_data;

		adc_total += adc_data;
	}
	return (adc_total - adc_max - adc_min) / (ADC_NUM_SAMPLES - 2);
}

static int iset_adc_value(void)
{
	return twl6030_get_adc_data(ISET_ADC_CHANNEL);
}

static int temp_adc_value(void)
{
	return twl6030_get_adc_data(TEMP_ADC_CHANNEL);
}

static bool check_charge_full(void)
{
	int ret;

	ret = iset_adc_value();
	if (ret < 0) {
		pr_err("%s: invalid iset adc value [%d]\n", __func__, ret);
		return false;
	}
	pr_debug("%s : iset adc value : %d\n", __func__, ret);

	return ret < CHARGE_FULL_ADC;
}

static int get_bat_temp_by_adc(int *batt_temp)
{
	int array_size = temper_table_size;
	int temp_adc = temp_adc_value();
	int mid;
	int left_side = 0;
	int right_side = array_size - 1;
	int temp = 0;

	if (temp_adc < 0) {
		pr_err("%s : Invalid temperature adc value [%d]\n",
		       __func__, temp_adc);
		return temp_adc;
	}

	while (left_side <= right_side) {
		mid = (left_side + right_side) / 2;
		if (mid == 0 || mid == array_size - 1 ||
		    (temper_table[mid].adc_value <= temp_adc &&
		     temper_table[mid + 1].adc_value > temp_adc)) {
			temp = temper_table[mid].temperature;
			break;
		} else if (temp_adc - temper_table[mid].adc_value > 0) {
			left_side = mid + 1;
		} else {
			right_side = mid - 1;
		}
	}

	pr_debug("%s: temp adc : %d, temp : %d\n", __func__, temp_adc, temp);
	*batt_temp = temp;
	return 0;
}

static int charger_init(struct device *dev)
{
	/*
	 * CHG_EN gpio is HIGH by default. This is good for normal boot.
	 * However, in case of lpm boot, CHG_EN is set LOW by bootloader.
	 * So, initialize CHG_EN with LOW. If not, charging will be disabled
	 * until charger and usb drivers are initialized. The side effect of
	 * this is, if the device boots in LPM mode with fully drained battery,
	 * it will cause the unexpected device resets.
	 */
	int ret;
	if (sec_bootmode == 5)
		charger_gpios[GPIO_CHG_EN].flags = GPIOF_OUT_INIT_LOW;

	ret = gpio_request_array(charger_gpios, ARRAY_SIZE(charger_gpios));
	if (ret == 0)
		t1_init_ta_nconnected(charger_gpios[GPIO_TA_nCONNECTED].gpio);
	return ret;
}

static void charger_exit(struct device *dev)
{
	gpio_free_array(charger_gpios, ARRAY_SIZE(charger_gpios));
}

static void set_charge_en(int state)
{
	gpio_set_value(charger_gpios[GPIO_CHG_EN].gpio, !state);
}

enum charger_mode {
	USB500 = 0,
	ISET,
};

static void set_charger_mode(int cable_type)
{
	int i;
	int mode;

	if (cable_type == PDA_POWER_CHARGE_AC)
		mode = ISET;
	else if (cable_type == PDA_POWER_CHARGE_USB)
		mode = USB500;
	else {
		/* switch off charger */
		gpio_set_value(charger_gpios[GPIO_CHG_EN].gpio, 1);
		return;
	}

	gpio_set_value(charger_gpios[GPIO_CHG_EN].gpio, 0);

	for (i = 0; i < mode; i++) {
		udelay(200);
		gpio_set_value(charger_gpios[GPIO_CHG_EN].gpio, 1);
		udelay(200);
		gpio_set_value(charger_gpios[GPIO_CHG_EN].gpio, 0);
	}

	return;
}

static void charger_set_charge(int state)
{
	unsigned long flags;

	spin_lock_irqsave(&charge_en_lock, flags);
	charger_state = state;
	set_charger_mode(state);
	spin_unlock_irqrestore(&charge_en_lock, flags);
}

static void charger_set_only_charge(int state)
{
	unsigned long flags;

	spin_lock_irqsave(&charge_en_lock, flags);
	if (charger_state)
		set_charge_en(state);
	spin_unlock_irqrestore(&charge_en_lock, flags);
	/* CHG_ING_N level changed after set charge_en and 150ms */
	msleep(150);
}

static int charger_is_online(void)
{
	return !gpio_get_value(charger_gpios[GPIO_TA_nCONNECTED].gpio);
}

static int charger_is_charging(void)
{
	return !gpio_get_value(charger_gpios[GPIO_CHG_ING_N].gpio);
}

static int get_charging_source(void)
{
	return charger_state;
}

static int get_full_charge_irq(void)
{
	return gpio_to_irq(charger_gpios[GPIO_CHG_ING_N].gpio);
}

static char *t1_charger_supplied_to[] = {
	"battery",
};

static __initdata struct pda_power_pdata charger_pdata = {
	.init			= charger_init,
	.exit			= charger_exit,
	.set_charge		= charger_set_charge,
	.wait_for_status	= 500,
	.wait_for_charger	= 500,
	.supplied_to		= t1_charger_supplied_to,
	.num_supplicants	= ARRAY_SIZE(t1_charger_supplied_to),
	.use_otg_notifier	= true,
};

static struct max17040_platform_data max17043_pdata = {
	.charger_online		= charger_is_online,
	.charger_enable		= charger_is_charging,
	.allow_charging		= charger_set_only_charge,
	.skip_reset		= true,
	.min_capacity		= 3,
	.is_full_charge		= check_charge_full,
	.get_bat_temp		= get_bat_temp_by_adc,
	.get_charging_source	= get_charging_source,
	.high_block_temp	= HIGH_BLOCK_TEMP_T1,
	.high_recover_temp	= HIGH_RECOVER_TEMP_T1,
	.low_block_temp		= LOW_BLOCK_TEMP_T1,
	.low_recover_temp	= LOW_RECOVER_TEMP_T1,
	.fully_charged_vol	= 4150000,
	.recharge_vol		= 4140000,
	.limit_charging_time	= 21600,	/* 6 hours */
	.limit_recharging_time	= 5400,	/* 90 min */
	.full_charge_irq	= get_full_charge_irq,
};

static __initdata struct i2c_board_info max17043_i2c[] = {
	{
		I2C_BOARD_INFO("max17040", (0x6C >> 1)),
		.platform_data = &max17043_pdata,
	}
};

static int __init t1_boot_mode_setup(char *str)
{
	if (!str)
		return 0;

	if (kstrtoint(str, 0, &max17043_pdata.bootmode))
		pr_err("t1 power: error in geting bootmode\n");

	return 1;
}

__setup("bootmode=", t1_boot_mode_setup);

static int __init t1_charger_mode_setup(char *str)
{
	if (!str)		/* No mode string */
		return 0;

	is_charging_mode = !strcmp(str, "charger");

	pr_debug("Charge mode string = \"%s\" charger mode = %d\n", str,
		 is_charging_mode);

	return 1;
}

__setup("androidboot.mode=", t1_charger_mode_setup);

static void __init omap4_t1_power_init_gpio(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(charger_gpios); i++)
		charger_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(charger_gpios[i].label);

	max17043_i2c[0].irq =
	    gpio_to_irq(omap_muxtbl_get_gpio_by_name("FUEL_ALERT"));
}

void __init omap4_t1_power_init(void)
{
	struct platform_device *pdev;

	omap4_t1_power_init_gpio();

	/* Update temperature data from board type */
	temper_table = temper_table_t1;
	temper_table_size = ARRAY_SIZE(temper_table_t1);

	/* Update oscillator information */
	omap_pm_set_osc_lp_time(15000, 1);

	pdev = platform_device_register_resndata(NULL, "pda-power", -1,
						 NULL, 0, &charger_pdata,
						 sizeof(charger_pdata));
	if (IS_ERR_OR_NULL(pdev))
		pr_err("cannot register pda-power\n");

	max17043_pdata.use_fuel_alert = !is_charging_mode;
	i2c_register_board_info(7, max17043_i2c, ARRAY_SIZE(max17043_i2c));

	if (enable_sr)
		omap_enable_smartreflex_on_init();

	omap_pm_enable_off_mode();
}

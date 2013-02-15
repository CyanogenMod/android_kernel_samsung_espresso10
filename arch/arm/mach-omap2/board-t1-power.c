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
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

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
#define VF_ADC_CHANNEL			0

#define CHARGE_FULL_ADC_PRE	388
#define CHARGE_FULL_ADC		250 /*188*/

#define CHARGE_FULL_COUNT_PRE	5
#define CHARGE_FULL_COUNT	5

#define HIGH_BLOCK_TEMP_T1		650
#define HIGH_RECOVER_TEMP_T1		430
#define LOW_BLOCK_TEMP_T1		(-30)
#define LOW_RECOVER_TEMP_T1		0

#define TEMP_BLOCK_FOR_CAM_RECORDING_TH	390

#define BAT_REMOVAL_ADC_VALUE 1000

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
static bool batt_full_pre;
static bool batt_full;
static int check_full_pre;
static int check_full;
static bool charging_state;
static bool charger_start_state;

static struct temp_adc_table_data temper_table_t1[] = {
	/* ADC, Temperature (C/10) */
	{69, 700},
	{70, 690},
	{72, 680},
	{75, 670},
	{78, 660},
	{81, 650},
	{83, 640},
	{86, 630},
	{89, 620},
	{92, 610},
	{95, 600},
	{99, 590},
	{103, 580},
	{107, 570},
	{111, 560},
	{115, 550},
	{119, 540},
	{123, 530},
	{127, 520},
	{131, 510},
	{135, 500},
	{139, 490},
	{144, 480},
	{149, 470},
	{154, 460},
	{159, 450},
	{164, 440},
	{169, 430},
	{176, 420},
	{182, 410},
	{189, 400},
	{196, 390},
	{204, 380},
	{211, 370},
	{218, 360},
	{225, 350},
	{233, 340},
	{240, 330},
	{247, 320},
	{254, 310},
	{262, 300},
	{271, 290},
	{280, 280},
	{290, 270},
	{299, 260},
	{309, 250},
	{318, 240},
	{327, 230},
	{337, 220},
	{346, 210},
	{356, 200},
	{367, 190},
	{379, 180},
	{390, 170},
	{402, 160},
	{414, 150},
	{425, 140},
	{437, 130},
	{449, 120},
	{460, 110},
	{472, 100},
	{484, 90},
	{496, 80},
	{509, 70},
	{521, 60},
	{533, 50},
	{545, 40},
	{557, 30},
	{570, 20},
	{582, 10},
	{594, 0},
	{600, (-10)},
	{607, (-20)},
	{613, (-30)},
	{621, (-40)},
	{629, (-50)},
	{637, (-60)},
	{646, (-70)},
	{654, (-80)},
	{662, (-90)},
	{670, (-100)},
};

static struct temp_adc_table_data *temper_table = temper_table_t1;
static int temper_table_size = ARRAY_SIZE(temper_table_t1);

static bool enable_sr = true;
module_param(enable_sr, bool, S_IRUSR | S_IRGRP | S_IROTH);

enum {
	GPIO_CHG_ING_N = 0,
	GPIO_TA_nCONNECTED,
	GPIO_CHG_EN,
	BAT_REMOVAL
};

enum {
	NOT_FULL = 0,
	FULL_CHARGED_PRE,
	FULL_CHARGED
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
	[BAT_REMOVAL] = {
		.flags = GPIOF_IN,
		.label = "BAT_REMOVAL"
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

static int get_vf_adc_value(void)
{
	return twl6030_get_adc_data(VF_ADC_CHANNEL);
}

static int check_vf_present(void)
{
	int vf_adc = 0;
	vf_adc = get_vf_adc_value();

	if (vf_adc > BAT_REMOVAL_ADC_VALUE) {
		pr_info("%s, vf_adc : %d\n", __func__, vf_adc);
		return 0;
	}

	return 1;
}

static int check_charge_full(bool batt_chg)
{
	int iset_adc;
	int ret = 0;

	if (!batt_chg) {
		batt_full_pre = false;
		batt_full = false;
		check_full_pre = 0;
		check_full = 0;

		return ret;
	}

	iset_adc = iset_adc_value();
	if (iset_adc < 0) {
		pr_err("%s: invalid iset adc value [%d]\n", __func__, iset_adc);
		return -EINVAL;
	}

	pr_debug("%s : iset adc value : %d\n", __func__, iset_adc);

	if (batt_chg && charging_state) {
		if (!batt_full_pre && iset_adc < CHARGE_FULL_ADC_PRE) {
			if (check_full_pre >= CHARGE_FULL_COUNT_PRE) {
				pr_info("%s : battery fully charged (pre)\n",
					__func__);
				batt_full_pre = true;
			} else
				check_full_pre++;
		} else if (check_full_pre > 0)
			check_full_pre = 0;

		if (!batt_full && iset_adc < CHARGE_FULL_ADC) {
			if (check_full >= CHARGE_FULL_COUNT) {
				pr_info("%s : battery fully charged !!!\n",
					__func__);
				batt_full = true;
			} else
				check_full++;
		}
	} else {
		batt_full_pre = false;
		batt_full = false;
		check_full_pre = 0;
		check_full = 0;
	}

	if (batt_full_pre)
		ret = FULL_CHARGED_PRE;

	if (batt_full)
		ret = FULL_CHARGED;

	return ret;
}

static int get_bat_temp_by_adc(int *batt_temp, int *batt_temp_adc)
{
	int array_size = temper_table_size;
	int temp_adc = temp_adc_value();
	int mid;
	int left_side = 0;
	int right_side = array_size - 1;
	int temp = 0;

	*batt_temp_adc = temp_adc;

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
	charging_state = state ? true : false;

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

	charging_state = cable_type ? true : false;

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

static void adjust_charger_mode(int state)
{
	if (charger_state &&
	    charger_state != state) {
		set_charger_mode(0);
		usleep_range(2000, 2100);
		set_charger_mode(state);
	}
	pr_info("%s : Charging current is adjusted : %d\n",
		__func__, state);
}

static void restore_charger_mode(void)
{
	if (charger_state) {
		set_charger_mode(0);
		usleep_range(2000, 2100);
		set_charger_mode(charger_state);
	}
	pr_info("%s : Charging current is restored : %d\n",
		__func__, charger_state);
}

static void set_start_state_for_charger(void)
{
	charger_start_state = true;
}

static bool check_charger_start_state(void)
{
	return charger_start_state;
}

static void set_charger_work(void)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	int ret = 0;

	if (!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	value.intval = POWER_SUPPLY_TYPE_BATTERY;

	if (psy) {
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
		}
	}
}

static bool check_temp_block_state(int temp)
{
	return (temp > TEMP_BLOCK_FOR_CAM_RECORDING_TH);
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

static bool get_charging_state(void)
{
	return charging_state;
}

static int get_full_charge_irq(void)
{
	return gpio_to_irq(charger_gpios[GPIO_CHG_ING_N].gpio);
}

static int get_bat_removal_irq(void)
{
	return gpio_to_irq(charger_gpios[BAT_REMOVAL].gpio);
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
	.charger_work		= set_charger_work,
	.get_charger_start_state	= check_charger_start_state,
	.supplied_to		= t1_charger_supplied_to,
	.num_supplicants	= ARRAY_SIZE(t1_charger_supplied_to),
	.use_otg_notifier	= true,
};

static struct max17040_platform_data max17043_pdata = {
	.charger_online		= charger_is_online,
	.charger_enable		= charger_is_charging,
	.allow_charging		= charger_set_only_charge,
	.adjust_charger_mode	= adjust_charger_mode,
	.restore_charger_mode	= restore_charger_mode,
	.check_temp_block_state = check_temp_block_state,
	.set_charger_start_state = set_start_state_for_charger,
	.skip_reset		= true,
	.min_capacity		= 1,
	.is_full_charge		= check_charge_full,
	.get_bat_temp		= get_bat_temp_by_adc,
	.get_charging_source	= get_charging_source,
	.get_charging_state	= get_charging_state,
	.high_block_temp	= HIGH_BLOCK_TEMP_T1,
	.high_recover_temp	= HIGH_RECOVER_TEMP_T1,
	.low_block_temp		= LOW_BLOCK_TEMP_T1,
	.low_recover_temp	= LOW_RECOVER_TEMP_T1,
	.fully_charged_vol	= 4150000,
	.recharge_vol		= 4140000,
	.limit_charging_time	= 21600,	/* 6 hours */
	.limit_recharging_time	= 5400,	/* 90 min */
	.full_charge_irq	= get_full_charge_irq,
	.bat_removal_irq	= get_bat_removal_irq,
	.vf_adc_value		= get_vf_adc_value,
	.battery_online		= check_vf_present,
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

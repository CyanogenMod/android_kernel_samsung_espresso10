/* Power support for Samsung Gokey Board.
 *
 * Copyright (C) 2011 SAMSUNG, Inc.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/twl6030-madc.h>
#include <linux/power/smb136_charger.h>
#include <linux/power/max17042_battery.h>
#include <linux/bat_manager.h>
#include <linux/i2c/bq2415x.h>
#include <linux/i2c/stc3115_battery.h>
#include "board-gokey.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "sec_common.h"
#include <linux/bat_manager.h>

#define TA_CHG_ING_N	0
#define TA_ENABLE	1

#define TEMP_ADC_CHANNEL	1
#define EAR_ADC_CHANNEL		2
#define ADC_NUM_SAMPLES		5
#define ADC_LIMIT_ERR_COUNT	5

struct fuelgauge_callbacks *fuelgauge_callback;
struct bq2415x_charger_callbacks *charger_callback;
struct battery_manager_callbacks *cable_callbacks;

static struct gpio charger_gpios[] = {
	/* TA_nCHG, 153*/
	{ .flags = GPIOF_IN, .label = "TA_nCHG" },
	/* TA_nCONNECTED, 13*/
	{ .flags = GPIOF_OUT_INIT_LOW, .label = "TA_nCONNECTED" },
};

static struct temp_adc_table_data temper_table[] = {
	/* ADC, Temperature (C/10) */
	/* adjusted in 20120407 */
	{ 322,   700     },
	{ 329,   690     },
	{ 336,   680     },
	{ 343,   670     },
	{ 350,   660     },
	{ 357,   650     },
	{ 364,   640     },
	{ 371,   630     },
	{ 378,   620     },
	{ 385,   610     },
	{ 392,   600     },
	{ 401,   590     },
	{ 410,   580     },
	{ 425,   570     },
	{ 442,   560     },
	{ 464,   550     },
	{ 482,   540     },
	{ 499,   530     },
	{ 518,   520     },
	{ 536,   510     },
	{ 560,   500     },
	{ 584,   490     },
	{ 603,   480     },
	{ 618,   470     },
	{ 631,   460     },
	{ 643,   450     },
	{ 661,   440     },
	{ 683,   430     },
	{ 700,   420     },
	{ 746,   410     },
	{ 796,   400     },
	{ 834,   390     },
	{ 879,   380     },
	{ 922,   370     },
	{ 963,   360     },
	{ 1005,   350     },
	{ 1043,   340     },
	{ 1083,  330     },
	{ 1125,  320     },
	{ 1169,  310     },
	{ 1206,  300     },
	{ 1243,  290     },
	{ 1280,  280     },
	{ 1317,  270     },
	{ 1354,  260     },
	{ 1391,  250     },
	{ 1428,  240     },
	{ 1465,  230     },
	{ 1502,  220     },
	{ 1539,  210     },
	{ 1576,  200     },
	{ 1613,  190     },
	{ 1650,  180     },
	{ 1687,  170     },
	{ 1724,  160     },
	{ 1761,  150     },
	{ 1798,  140     },
	{ 1835,  130     },
	{ 1872,  120     },
	{ 1909,  110     },
	{ 1946,  100     },
	{ 1983,  90      },
	{ 2020,  80      },
	{ 2040,  70      },
	{ 2070,  60      },
	{ 2140,  50      },
	{ 2180,  40      },
	{ 2225,  30      },
	{ 2320,  20      },
	{ 2365,  10      },
	{ 2415,  0       },
	{ 2495,  (-10)   },
	{ 2598,  (-20)   },
	{ 2658,  (-30)   },
	{ 2695,  (-40)   },
	{ 2764,  (-50)   },
	{ 2835,  (-60)   },
	{ 2905,  (-70)   },
	{ 2974,  (-80)   },
	{ 3027,  (-90)   },
	{ 3080,  (-100)  },
};

static int twl6030_get_adc_data(int ch)
{
	int adc_data;
	int adc_max = -1;
	int adc_min = 1 << 11;
	int adc_total = 0;
	int i, j;

	msleep(200);
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

static int temp_adc_value(void)
{
	int array_size = ARRAY_SIZE(temper_table);
	int temp_adc = twl6030_get_adc_data(TEMP_ADC_CHANNEL);
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
				 temper_table[mid+1].adc_value > temp_adc)) {
			temp = temper_table[mid].temperature;

			break;
		} else if (temp_adc - temper_table[mid].adc_value > 0) {
			left_side = mid + 1;
		} else {
			right_side = mid - 1;
		}
	}

	/*return twl6030_get_adc_data(TEMP_ADC_CHANNEL);*/
	return temp;
}

static int get_adc_temperature(void)
{
	return 3400 - twl6030_get_adc_data(TEMP_ADC_CHANNEL);
}

static int get_temperature(void)
{
	return temp_adc_value();
}

int get_temp(void)
{
	return temp_adc_value();
}

int omap4_get_adc_earjack(void)
{
	usleep_range(100000, 110000);
	return twl6030_get_madc_conversion(EAR_ADC_CHANNEL);
}

static void charger_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(charger_gpios); i++) {
		charger_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(charger_gpios[i].label);
	}

	gpio_request_array(charger_gpios, ARRAY_SIZE(charger_gpios));
}

static void charger_enble_set(int state)
{
	/*if battery is not detected, make charging set disable*/
	gpio_set_value(charger_gpios[TA_ENABLE].gpio, !state);

	pr_info("%s: Set charge status : %d, current status: %d\n",
		__func__, state,
		gpio_get_value(charger_gpios[TA_ENABLE].gpio));
}

static struct i2c_gpio_platform_data gokey_gpio_i2c7_pdata = {
	.udelay = 2,
	.timeout = 0,
};

static struct platform_device gokey_gpio_i2c7_device = {
	.name = "i2c-gpio",
	.id = 7,
	.dev = {
		.platform_data = &gokey_gpio_i2c7_pdata,
	},
};

static void __init gokey_gpio_i2c_init(void)
{
	/*Fuel gauge I2C : gpio-i2c 7*/
	gokey_gpio_i2c7_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("FUEL_SDA_1.8V");
	gokey_gpio_i2c7_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("FUEL_SCL_1.8V");
}

static void bq24157_charger_register_callbacks(
		struct bq2415x_charger_callbacks *ptr)
{
	charger_callback = ptr;
}

static void set_chg_state(int cable_type)
{
	if (charger_callback && charger_callback->set_charge_current)
		charger_callback->set_charge_current(charger_callback,
				cable_type);
}

static void set_termination_current(int term_type)
{
	if (charger_callback && charger_callback->set_termination_current)
		charger_callback->set_termination_current(charger_callback,
							term_type);
}

static void get_charger_register(void)
{
	if (charger_callback && charger_callback->get_charger_register)
		charger_callback->get_charger_register(charger_callback);
}

static void set_charger_voreg(void)
{
	if (charger_callback && charger_callback->set_charger_voreg)
		charger_callback->set_charger_voreg(charger_callback);
}

static void set_control_limit(void)
{
	if (charger_callback && charger_callback->set_control_limit)
		charger_callback->set_control_limit(charger_callback);
}

static void set_control_800mA(void)
{
	if (charger_callback && charger_callback->set_control_800)
		charger_callback->set_control_800(charger_callback);
}

static void set_chg_current(int cable_type)
{
	if (charger_callback && charger_callback->set_charge_current)
		charger_callback->set_charge_current(charger_callback,
							cable_type);
}

static void set_full_charge_notify(void)
{
	if (cable_callbacks && cable_callbacks->set_full_charge)
		cable_callbacks->set_full_charge(cable_callbacks);
}

static void set_charge_fault_notify(void)
{
	if (cable_callbacks && cable_callbacks->set_charge_fault)
		cable_callbacks->set_charge_fault(cable_callbacks);
}

static void connector_register_charger_callbacks(
		struct battery_manager_callbacks *ptr)
{
	cable_callbacks = ptr;
}

static struct bq2415x_platform_data bq24157_pdata = {
	.max_charger_currentmA = 750,
	.max_charger_voltagemV = 4360,
	.first_term_currentmA = 150,
	.second_term_currentmA = 50,
	.third_term_currentmA = 50,
	.cin_limit_current = 750,
	.charge_ac_current = 750,
	.charge_usb_current = 550,
	.set_full_charge = set_full_charge_notify,
	.set_charge_fault = set_charge_fault_notify,
	.set_charge = charger_enble_set,
	.register_callbacks = bq24157_charger_register_callbacks,
};

static const __initdata struct i2c_board_info bq24157_i2c[] = {
		{
				I2C_BOARD_INFO("bq2415x_charger", 0x6A),
				.platform_data = &bq24157_pdata,
		},
};

static void fuelgauge_register_callbacks(
		struct fuelgauge_callbacks *ptr)
{
	fuelgauge_callback = ptr;
}

static int read_fuel_value(enum fuel_property fg_prop)
{
	if (fuelgauge_callback && fuelgauge_callback->get_value)
		return fuelgauge_callback->get_value(fuelgauge_callback,
						fg_prop);
	return 0;
}

static int check_charger_type(void)
{
	int cable_type = CABLE_TYPE_NONE;
	char *cable_type_str[] = {"NONE", "USB", "AC"};

	cable_type = gokey_get_charging_type();
#ifdef CONFIG_MP3_LP_MODE
	if (cable_type)
		cpufreq_lpmode.cable_attached = 1;
	else
		cpufreq_lpmode.cable_attached = 0;
#endif
	pr_info("%s: Charger type is [%s]\n",
			__func__, cable_type_str[cable_type]);

	return cable_type;
}

static struct batman_platform_data battery_manager_pdata = {
	.get_fuel_value = read_fuel_value,
	.set_charger_state = set_chg_state,
	.set_charger_en = charger_enble_set,
	.set_term_current = set_termination_current,
	.set_charger_voreg = set_charger_voreg,
	.set_control_limit = set_control_limit,
	.set_control_800mA = set_control_800mA,
	.get_charger_register = get_charger_register,
	.get_temp = get_temperature,
	.get_temp_adc = get_adc_temperature,
	.get_charger_type = check_charger_type,
	.register_callbacks = connector_register_charger_callbacks,
	.high_block_temp = 600,
	.high_recover_temp = 420,
	.low_block_temp = -50,
	.low_recover_temp = 0,
	.limit_charging_time = 28800,	/* 8 hours */
	.limit_recharging_time = 7200,	/* 2 hours */
	.recharge_voltage = 4290000,
};

static struct platform_device battery_manager_device = {
	.name   = "battery_manager",
	.id     = -1,
	.dev    = {
		.platform_data = &battery_manager_pdata,
	},
};

static int null_fn(void)
{
	return 0;	/* for discharging status */
}

static int Temperature_fn(void)
{
	return 25;
}

static struct stc311x_platform_data stc3115_data = {
	.battery_online = NULL,
	/* used in stc311x_get_status() */
	.charger_online = null_fn,
	.charger_enable = null_fn,
	.power_supply_register = NULL,
	.power_supply_unregister = NULL,
	/*REG_MODE, BIT_VMODE 1-Voltage mode, 0-mixed mode */
	.Vmode = 0,
	/* SOC alm level %*/
	.Alm_SOC = 10,
	/* Vbat alm level mV*/
	.Alm_Vbat = 3600,
	/* nominal CC_cnf, coming from battery characterisation*/
	.CC_cnf = 849,
	/* nominal VM cnf , coming from battery characterisation*/
	.VM_cnf = 459,
	/* nominal capacity in mAh, coming from battery characterisation*/
	.Cnom = 2100,
	/* sense resistor mOhms*/
	.Rsense = 20,
	/* current for relaxation in mA (< C/20) */
	.RelaxCurrent = 95,
	/* 1=Adaptive mode enabled, 0=Adaptive mode disabled */
	.Adaptive = 1,
	/* capacity derating in 0.1% */
	.CapDerating[6] = 190,	/* for temp = -20 C */
	.CapDerating[5] = 70,	/* for temp = -10 C */
	.CapDerating[4] = 30,	/* for temp = 0  C */
	.CapDerating[3] = 0,	/* for temp = 10 C */
	.CapDerating[2] = 0,	/* for temp = 25 C */
	.CapDerating[1] = -20,	/* for temp = 40 C */
	.CapDerating[0] = -40,	/* for temp = 60 C */

	.OCVOffset[15] = -123,	/* OCV curve adjustment */
	.OCVOffset[14] = -30,	/* OCV curve adjustment */
	.OCVOffset[13] = -12,	/* OCV curve adjustment */
	.OCVOffset[12] = -27,	/* OCV curve adjustment */
	.OCVOffset[11] = 0,		/* OCV curve adjustment */
	.OCVOffset[10] = -27,	/* OCV curve adjustment */
	.OCVOffset[9] = 4,		/* OCV curve adjustment */
	.OCVOffset[8] = 1,		/* OCV curve adjustment */
	.OCVOffset[7] = 7,		/* OCV curve adjustment */
	.OCVOffset[6] = 9,		/* OCV curve adjustment */
	.OCVOffset[5] = 9,		/* OCV curve adjustment */
	.OCVOffset[4] = 16,		/* OCV curve adjustment */
	.OCVOffset[3] = 33,		/* OCV curve adjustment */
	.OCVOffset[2] = 34,		/* OCV curve adjustment */
	.OCVOffset[1] = 46,		/* OCV curve adjustment */
	.OCVOffset[0] = -3,		/* OCV curve adjustment */


	.OCVOffset2[15] = -45,	/* OCV curve adjustment */
	.OCVOffset2[14] = -35,	/* OCV curve adjustment */
	.OCVOffset2[13] = -21,	/* OCV curve adjustment */
	.OCVOffset2[12] = -32,	/* OCV curve adjustment */
	.OCVOffset2[11] = -8,	/* OCV curve adjustment */
	.OCVOffset2[10] = -9,	/* OCV curve adjustment */
	.OCVOffset2[9] = -1,		/* OCV curve adjustment */
	.OCVOffset2[8] = 3,		/* OCV curve adjustment */
	.OCVOffset2[7] = 2,		/* OCV curve adjustment */
	.OCVOffset2[6] = 3,		/* OCV curve adjustment */
	.OCVOffset2[5] = 4,		/* OCV curve adjustment */
	.OCVOffset2[4] = -2,	/* OCV curve adjustment */
	.OCVOffset2[3] = 23,	/* OCV curve adjustment */
	.OCVOffset2[2] = 57,	/* OCV curve adjustment */
	.OCVOffset2[1] = 90,	/* OCV curve adjustment */
	.OCVOffset2[0] = 0,	/* OCV curve adjustment */

	/*if the application temperature data is preferred
		than the STC3115 temperature*/
	/*External temperature fonction, return C*/
	.ExternalTemperature = Temperature_fn,
	/* 1=External temperature, 0=STC3115 temperature */
	.ForceExternalTemperature = 0,

	.register_callbacks = &fuelgauge_register_callbacks,
};

static const __initdata struct i2c_board_info beagle_i2c2_boardinfo[] = {

	{
		I2C_BOARD_INFO("stc3115", 0x70),
		.platform_data = &stc3115_data,
	},

};

void __init omap4_gokey_charger_init(void)
{
	int ret;
	charger_gpio_init();

	gokey_gpio_i2c_init();

	battery_manager_pdata.ta_gpio =
			omap_muxtbl_get_gpio_by_name("TA_nCHG");

	bq24157_pdata.ta_gpio =
				omap_muxtbl_get_gpio_by_name("TA_nCHG");

	bq24157_pdata.ta_irq =
				gpio_to_irq(bq24157_pdata.ta_gpio);

	bq24157_pdata.ta_enable_gpio =
				omap_muxtbl_get_gpio_by_name("TA_nCONNECTED");

	battery_manager_pdata.bat_removal =
				omap_muxtbl_get_gpio_by_name("BAT_REMOVAL");

	if (system_rev < 2)
		omap_mux_set_gpio(OMAP_PIN_INPUT_PULLUP |
			OMAP_MUX_MODE3, battery_manager_pdata.bat_removal);

	bq24157_pdata.vf_gpio = battery_manager_pdata.bat_removal;
	bq24157_pdata.vf_irq = gpio_to_irq(bq24157_pdata.vf_gpio);

	battery_manager_pdata.jig_on =
				omap_muxtbl_get_gpio_by_name("JIG_ON_18");

	battery_manager_pdata.vbus_gpio  =
				omap_muxtbl_get_gpio_by_name("5V_DET");

	battery_manager_pdata.bootmode = sec_bootmode;

	pr_info("%s: bootmode is %d\n",
		__func__, battery_manager_pdata.bootmode);

/*
	ret = platform_device_register(&gokey_gpio_i2c5_device);
	if (ret < 0)
		pr_err("%s: gpio_i2c5 device register fail\n", __func__);
*/
	ret = platform_device_register(&gokey_gpio_i2c7_device);
	if (ret < 0)
		pr_err("%s: gpio_i2c7 device register fail\n", __func__);

	i2c_register_board_info(4, bq24157_i2c, ARRAY_SIZE(bq24157_i2c));
	i2c_register_board_info(7, beagle_i2c2_boardinfo,
		ARRAY_SIZE(beagle_i2c2_boardinfo));
	ret = platform_device_register(&battery_manager_device);
	if (ret < 0)
		pr_err("%s: battery monitor device register fail\n", __func__);
}

/* Sensor support for Samsung Tuna Board.
 *
 * Copyright (C) 2011 Google, Inc.
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

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "mux.h"
#include "omap_muxtbl.h"

#include <linux/gp2a.h>
#include <linux/i2c/twl6030-madc.h>
#include <linux/regulator/consumer.h>
#include <linux/bh1721fvc.h>
#include <linux/yas.h>
#include <linux/al3201.h>

#include "board-espresso.h"

#define YAS_TA_OFFSET_ESPRESSO {0, 0, 0}
#define YAS_USB_OFFSET_ESPRESSO {0, 0, 0}
#define YAS_TA_OFFSET_ESPRESSO10 {200, -4600, -1100}
#define YAS_USB_OFFSET_ESPRESSO10 {0, -1100, -300}
#define YAS_FULL_OFFSET {0, 0, 0}

enum {
	NUM_ALS_INT = 0,
	NUM_PS_VOUT,
	NUM_MSENSE_IRQ,
};

struct gpio sensors_gpios[] = {
	[NUM_ALS_INT] = {
		.flags = GPIOF_IN,
		.label = "ALS_INT_18",
	},
	[NUM_PS_VOUT] = {
		.flags = GPIOF_IN,
		.label = "PS_VOUT",
	},
	[NUM_MSENSE_IRQ] = {
		.flags = GPIOF_IN,
		.label = "MSENSE_IRQ",
	},
};

static int bh1721fvc_light_sensor_reset(void)
{
	pr_info("%s\n", __func__);

	omap_mux_init_gpio(sensors_gpios[NUM_ALS_INT].gpio,
		OMAP_PIN_OUTPUT);

	gpio_free(sensors_gpios[NUM_ALS_INT].gpio);

	gpio_request(sensors_gpios[NUM_ALS_INT].gpio, "LIGHT_SENSOR_RESET");

	gpio_direction_output(sensors_gpios[NUM_ALS_INT].gpio, 0);

	udelay(2);

	gpio_direction_output(sensors_gpios[NUM_ALS_INT].gpio, 1);

	return 0;

}

static struct bh1721fvc_platform_data bh1721fvc_pdata = {
	.reset = bh1721fvc_light_sensor_reset,
};

#define GP2A_LIGHT_ADC_CHANNEL	4

static int gp2a_light_adc_value(void)
{
	if (system_rev >= 6)
		return twl6030_get_madc_conversion(GP2A_LIGHT_ADC_CHANNEL)/4;
	else
		return twl6030_get_madc_conversion(GP2A_LIGHT_ADC_CHANNEL);
}

static void gp2a_power(bool on)
{

}

static void omap4_espresso_sensors_regulator_on(bool on)
{
	struct regulator *reg_v28;
	struct regulator *reg_v18;

	reg_v28 =
		regulator_get(NULL, "VAP_IO_2.8V");
	if (IS_ERR(reg_v28)) {
		pr_err("%s [%d] failed to get v2.8 regulator.\n",
			__func__, __LINE__);
		goto done;
	}
	reg_v18 =
		regulator_get(NULL, "VDD_IO_1.8V");
	if (IS_ERR(reg_v18)) {
		pr_err("%s [%d] failed to get v1.8 regulator.\n",
			__func__, __LINE__);
		goto done;
	}
	if (on) {
		pr_info("sensor ldo on.\n");
		regulator_enable(reg_v28);
		regulator_enable(reg_v18);
	} else {
		pr_info("sensor ldo off.\n");
		regulator_disable(reg_v18);
		regulator_disable(reg_v28);
	}
	regulator_put(reg_v28);
	regulator_put(reg_v18);
	if (!board_is_espresso10())
		msleep(20);
done:
	return;
}

static struct gp2a_platform_data gp2a_pdata = {
	.power = gp2a_power,
	.p_out = 0,
	.light_adc_value = gp2a_light_adc_value,
	.ldo_on = omap4_espresso_sensors_regulator_on,
};

struct mag_platform_data magnetic_pdata = {
	.power_on = omap4_espresso_sensors_regulator_on,
	.offset_enable = 0,
	.chg_status = CABLE_TYPE_NONE,
	.ta_offset.v = YAS_TA_OFFSET_ESPRESSO,
	.usb_offset.v = YAS_USB_OFFSET_ESPRESSO,
	.full_offset.v = YAS_FULL_OFFSET,
};

void omap4_espresso_set_chager_type(int type)
{
	static int prev = CABLE_TYPE_NONE;
	magnetic_pdata.chg_status = type;

	if (board_is_espresso10()) {
		if (prev != type)
			magnetic_pdata.offset_enable = 1;
		prev = type;
	}
}

struct acc_platform_data accelerometer_pdata = {
	.cal_path = "/efs/calibration_data",
	.ldo_on = omap4_espresso_sensors_regulator_on,
};

static struct al3201_platform_data al3201_pdata = {
	.power_on = omap4_espresso_sensors_regulator_on,
};

static struct i2c_board_info __initdata espresso10_sensors_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("bh1721fvc", 0x23),
		.platform_data = &bh1721fvc_pdata,
	},
};

static struct i2c_board_info __initdata espresso_sensors_i2c4_boardinfo_rf[] = {
	{
		I2C_BOARD_INFO("gp2a", 0x44),
		.platform_data = &gp2a_pdata,
	},
};

static struct i2c_board_info __initdata espresso_sensors_i2c4_boardinfo_wf[] = {
	{
		I2C_BOARD_INFO("AL3201", 0x1c),
		.platform_data = &al3201_pdata,
	},
};

static struct i2c_board_info __initdata espresso_common_sensors_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("accelerometer", 0x18),
		.platform_data = &accelerometer_pdata,
	 },
	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
		.platform_data = &magnetic_pdata,
	},
};


void __init omap4_espresso_sensors_init(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(sensors_gpios); i++)
		sensors_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(sensors_gpios[i].label);

	gpio_request_array(sensors_gpios, ARRAY_SIZE(sensors_gpios));

	omap_mux_init_gpio(sensors_gpios[NUM_MSENSE_IRQ].gpio,
		OMAP_PIN_OUTPUT);

	gpio_free(sensors_gpios[NUM_MSENSE_IRQ].gpio);

	gpio_request(sensors_gpios[NUM_MSENSE_IRQ].gpio, "MSENSE_IRQ");

	gpio_direction_output(sensors_gpios[NUM_MSENSE_IRQ].gpio, 1);

	gp2a_pdata.p_out = sensors_gpios[NUM_PS_VOUT].gpio;

	if (!board_is_espresso10()) {
		magnetic_pdata.orientation = 8;
		accelerometer_pdata.orientation = 8;
		if (board_has_modem()) {
			i2c_register_board_info(4, espresso_sensors_i2c4_boardinfo_rf,
				ARRAY_SIZE(espresso_sensors_i2c4_boardinfo_rf));
		} else {
			i2c_register_board_info(4, espresso_sensors_i2c4_boardinfo_wf,
				ARRAY_SIZE(espresso_sensors_i2c4_boardinfo_wf));
		}
	} else {
		magnetic_pdata.orientation = 7;
		accelerometer_pdata.orientation = 6;
		int32_t ta_offset_espresso10[] = YAS_TA_OFFSET_ESPRESSO10;
		int32_t usb_offset_espresso10[] = YAS_USB_OFFSET_ESPRESSO10;
		magnetic_pdata.ta_offset.v[0] = ta_offset_espresso10[0];
		magnetic_pdata.ta_offset.v[1] = ta_offset_espresso10[1];
		magnetic_pdata.ta_offset.v[2] = ta_offset_espresso10[2];
		magnetic_pdata.usb_offset.v[0] = usb_offset_espresso10[0];
		magnetic_pdata.usb_offset.v[1] = usb_offset_espresso10[1];
		magnetic_pdata.usb_offset.v[2] = usb_offset_espresso10[2];
		i2c_register_board_info(4, espresso10_sensors_i2c4_boardinfo,
			ARRAY_SIZE(espresso10_sensors_i2c4_boardinfo));
	}

	i2c_register_board_info(4, espresso_common_sensors_i2c4_boardinfo,
			ARRAY_SIZE(espresso_common_sensors_i2c4_boardinfo));
}

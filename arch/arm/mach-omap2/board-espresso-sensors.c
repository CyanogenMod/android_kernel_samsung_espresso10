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

#include <linux/gp2a.h>
#include <linux/i2c/twl6030-gpadc.h>
#include <linux/regulator/consumer.h>
#include <linux/bh1721fvc.h>
#include <linux/yas.h>
#include <linux/al3201.h>

#include "board-espresso.h"

#define GPIO_ALS_INT		33
#define GPIO_PS_VOUT		1
#define GPIO_MSENSE_IRQ		157

#define GP2A_LIGHT_ADC_CHANNEL	4

#define YAS_TA_OFFSET_ESPRESSO {0, 0, 0}
#define YAS_USB_OFFSET_ESPRESSO {0, 0, 0}
#define YAS_TA_OFFSET_ESPRESSO10 {200, -4600, -1100}
#define YAS_USB_OFFSET_ESPRESSO10 {0, -1100, -300}
#define YAS_FULL_OFFSET {0, 0, 0}

static int bh1721fvc_light_sensor_reset(void)
{
	pr_debug("%s\n", __func__);

	omap_mux_init_gpio(GPIO_ALS_INT, OMAP_PIN_OUTPUT);

	gpio_free(GPIO_ALS_INT);

	gpio_request(GPIO_ALS_INT, "LIGHT_SENSOR_RESET");

	gpio_direction_output(GPIO_ALS_INT, 0);

	udelay(2);

	gpio_direction_output(GPIO_ALS_INT, 1);

	return 0;
}

static struct bh1721fvc_platform_data bh1721fvc_pdata = {
	.reset = bh1721fvc_light_sensor_reset,
};

static int gp2a_light_adc_value(void)
{
	if (system_rev >= 6)
		return twl6030_get_gpadc_conversion(GP2A_LIGHT_ADC_CHANNEL) / 4;
	else
		return twl6030_get_gpadc_conversion(GP2A_LIGHT_ADC_CHANNEL);
}

static void gp2a_power(bool on)
{

}

static void omap4_espresso_sensors_regulator_on(bool on)
{
	struct regulator *reg_v28;
	struct regulator *reg_v18;

	reg_v28 = regulator_get(NULL, "VAP_IO_2.8V");
	if (IS_ERR(reg_v28)) {
		pr_err("%s [%d] failed to get v2.8 regulator\n",
			__func__, __LINE__);
		return;
	}
	reg_v18 = regulator_get(NULL, "VDD_IO_1.8V");
	if (IS_ERR(reg_v18)) {
		pr_err("%s [%d] failed to get v1.8 regulator\n",
			__func__, __LINE__);
		return;
	}

	pr_debug("%s: %d\n", __func__, on);

	if (on) {
		regulator_enable(reg_v28);
		regulator_enable(reg_v18);
	} else {
		regulator_disable(reg_v18);
		regulator_disable(reg_v28);
	}
	regulator_put(reg_v28);
	regulator_put(reg_v18);
	if (!board_is_espresso10())
		msleep(20);
}

static struct gp2a_platform_data gp2a_pdata = {
	.power = gp2a_power,
	.p_out = GPIO_PS_VOUT,
	.light_adc_value = gp2a_light_adc_value,
	.ldo_on = omap4_espresso_sensors_regulator_on,
};

struct mag_platform_data magnetic_pdata = {
	.power_on = omap4_espresso_sensors_regulator_on,
	.offset_enable = 0,
	.orientation = 8, /* P31xx default */
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
	.orientation = 8, /* P31xx default */
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
	int32_t ta_offset_espresso10[] = YAS_TA_OFFSET_ESPRESSO10;
	int32_t usb_offset_espresso10[] = YAS_USB_OFFSET_ESPRESSO10;
	struct gpio sensors_gpios[] = {
		{
			.flags = GPIOF_IN,
			.gpio  = GPIO_ALS_INT,
			.label = "ALS_INT_18",
		},
		{
			.flags = GPIOF_IN,
			.gpio  = GPIO_PS_VOUT,
			.label = "PS_VOUT",
		},
		{
			.flags = GPIOF_IN,
			.gpio  = GPIO_MSENSE_IRQ,
			.label = "MSENSE_IRQ",
		},
	};
	gpio_request_array(sensors_gpios, ARRAY_SIZE(sensors_gpios));

	omap_mux_init_gpio(GPIO_MSENSE_IRQ,
		OMAP_PIN_OUTPUT);

	gpio_free(GPIO_MSENSE_IRQ);

	gpio_request(GPIO_MSENSE_IRQ, "MSENSE_IRQ");

	gpio_direction_output(GPIO_MSENSE_IRQ, 1);

	if (!board_is_espresso10()) {
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

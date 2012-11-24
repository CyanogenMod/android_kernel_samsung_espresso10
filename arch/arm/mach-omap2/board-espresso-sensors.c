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
#include <linux/mpu.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "mux.h"
#include "omap_muxtbl.h"

#include <linux/gp2a.h>
#include <linux/i2c/twl6030-madc.h>
#include <linux/regulator/consumer.h>
#include <linux/yas.h>
#include <linux/al3201.h>

#include "board-espresso.h"

#define YAS_TA_OFFSET {0, 0, 0}
#define YAS_USB_OFFSET {0, 0, 0}
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
	.ta_offset.v = YAS_TA_OFFSET,
	.usb_offset.v = YAS_USB_OFFSET,
	.full_offset.v = YAS_FULL_OFFSET,
};

void omap4_espresso_set_chager_type(int type)
{
	magnetic_pdata.chg_status = type;
}

struct acc_platform_data accelerometer_pdata = {
	.cal_path = "/efs/calibration_data",
	.ldo_on = omap4_espresso_sensors_regulator_on,
};

static struct al3201_platform_data al3201_pdata = {
	.power_on = omap4_espresso_sensors_regulator_on,
};

static struct i2c_board_info __initdata espresso_sensors_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("accelerometer", 0x18),
		.platform_data = &accelerometer_pdata,
	 },
	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
		.platform_data = &magnetic_pdata,
	 },
	{
		I2C_BOARD_INFO("gp2a", 0x44),
		.platform_data = &gp2a_pdata,
	},
	{
		I2C_BOARD_INFO("AL3201", 0x1c),
		.platform_data = &al3201_pdata,
	},
};

static struct i2c_board_info __initdata espresso_sensors_i2c4_boardinfo_rf[] = {
	{
		I2C_BOARD_INFO("accelerometer", 0x18),
		.platform_data = &accelerometer_pdata,
	 },
	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
		.platform_data = &magnetic_pdata,
	 },
	{
		I2C_BOARD_INFO("gp2a", 0x44),
		.platform_data = &gp2a_pdata,
	},
};

static struct i2c_board_info __initdata espresso_sensors_i2c4_boardinfo_wf[] = {
	{
		I2C_BOARD_INFO("accelerometer", 0x18),
		.platform_data = &accelerometer_pdata,
	 },
	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
		.platform_data = &magnetic_pdata,
	 },
	{
		I2C_BOARD_INFO("AL3201", 0x1c),
		.platform_data = &al3201_pdata,
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

	pr_info("%s: hw rev = %d, board type = %d\n",
		__func__, system_rev, omap4_espresso_get_board_type());

	if (system_rev < 7) {
		i2c_register_board_info(4, espresso_sensors_i2c4_boardinfo,
			ARRAY_SIZE(espresso_sensors_i2c4_boardinfo));
	} else {
		if (omap4_espresso_get_board_type()
			== SEC_MACHINE_ESPRESSO) {
			i2c_register_board_info(4,
				espresso_sensors_i2c4_boardinfo_rf,
				ARRAY_SIZE(espresso_sensors_i2c4_boardinfo_rf));
		} else {
			i2c_register_board_info(4,
				espresso_sensors_i2c4_boardinfo_wf,
				ARRAY_SIZE(espresso_sensors_i2c4_boardinfo_wf));
		}
	}
}


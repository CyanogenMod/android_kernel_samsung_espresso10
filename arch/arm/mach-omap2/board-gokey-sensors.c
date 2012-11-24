/* Sensor support for Samsung Gokey Board.
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

#include <linux/i2c/twl6030-madc.h>
#include <linux/yas.h>

#include "board-gokey.h"

#define ACCEL_CAL_PATH  "/efs/calibration_data"

enum {
	GPIO_SENSOR_EN = 0,
	GPIO_ACCEL_INT,
	GPIO_MSENSE_IRQ,
};

struct gpio sensors_gpios[] = {
	[GPIO_SENSOR_EN] = {
	.flags = GPIOF_OUT_INIT_HIGH,
	.label = "SENSOR_EN",
	},

	[GPIO_ACCEL_INT] = {
	.flags = GPIOF_IN,
	.label = "ACCEL_INT",
	},

	[GPIO_MSENSE_IRQ] = {
	.flags = GPIOF_IN,
	.label = "MSENSE_IRQ",
	},
};

struct acc_platform_data accelerometer_pdata = {
	.cal_path = ACCEL_CAL_PATH,
};

static struct i2c_board_info __initdata gokey_sensors_i2c4_boardinfo[] = {
#if defined(CONFIG_INPUT_YAS_ACCELEROMETER)
	{
		I2C_BOARD_INFO("accelerometer", 0x19),
		.platform_data = &accelerometer_pdata,
	},
#endif
#if defined(CONFIG_INPUT_YAS_MAGNETOMETER)
	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
	},
#endif
};

void __init omap4_gokey_sensors_init(void)
{
	int i;
	int ret = -ENODEV;

	for (i = 0; i < ARRAY_SIZE(sensors_gpios); i++)
		sensors_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(sensors_gpios[i].label);

	gpio_request_array(sensors_gpios, ARRAY_SIZE(sensors_gpios));
	gpio_free(sensors_gpios[GPIO_SENSOR_EN].gpio);
	ret = gpio_request(sensors_gpios[GPIO_SENSOR_EN].gpio, "SENSOR_EN");
	if (ret < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
		__func__, sensors_gpios[GPIO_SENSOR_EN].gpio, ret);
	}

	gpio_direction_output(sensors_gpios[GPIO_SENSOR_EN].gpio, 0);
	mdelay(70);
	gpio_direction_output(sensors_gpios[GPIO_SENSOR_EN].gpio, 1);

	i2c_register_board_info(4, gokey_sensors_i2c4_boardinfo,
	ARRAY_SIZE(gokey_sensors_i2c4_boardinfo));
}

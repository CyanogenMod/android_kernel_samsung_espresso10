/* arch/arm/mach-omap2/board-espresso10-sensor.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-espresso-sensor.c
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
#include <linux/bh1721fvc.h>
#include <linux/yas.h>

#include "board-espresso10.h"

#define ACCEL_CAL_PATH	"/efs/calibration_data"

#define YAS_TA_OFFSET {200, -4600, -1100}
#define YAS_USB_OFFSET {0, -1100, -300}
#define YAS_FULL_OFFSET {0, 0, 0}

enum {
	GPIO_ALS_INT = 0,
	GPIO_PS_VOUT,
	GPIO_MSENSE_IRQ,
};

struct gpio sensors_gpios[] = {
	[GPIO_ALS_INT] = {
		.flags = GPIOF_IN,
		.label = "ALS_INT_18",
	},
	[GPIO_PS_VOUT] = {
		.flags = GPIOF_IN,
		.label = "PS_VOUT",
	},
	[GPIO_MSENSE_IRQ] = {
		.flags = GPIOF_IN,
		.label = "MSENSE_IRQ",
	},
};

static int bh1721fvc_light_sensor_reset(void)
{

	printk(KERN_INFO " bh1721_light_sensor_reset !!\n");

	omap_mux_init_gpio(sensors_gpios[GPIO_ALS_INT].gpio,
		OMAP_PIN_OUTPUT);

	gpio_free(sensors_gpios[GPIO_ALS_INT].gpio);

	gpio_request(sensors_gpios[GPIO_ALS_INT].gpio, "LIGHT_SENSOR_RESET");

	gpio_direction_output(sensors_gpios[GPIO_ALS_INT].gpio, 0);

	udelay(2);

	gpio_direction_output(sensors_gpios[GPIO_ALS_INT].gpio, 1);

	return 0;

}

static struct bh1721fvc_platform_data bh1721fvc_pdata = {
	.reset = bh1721fvc_light_sensor_reset,
};

struct mag_platform_data magnetic_pdata = {
	.offset_enable = 0,
	.chg_status = CABLE_TYPE_NONE,
	.ta_offset.v = YAS_TA_OFFSET,
	.usb_offset.v = YAS_USB_OFFSET,
	.full_offset.v = YAS_FULL_OFFSET,
};

void omap4_espresso_set_chager_type(int type)
{
	static int prev = CABLE_TYPE_NONE;
	magnetic_pdata.chg_status = type;
	if (prev != type)
		magnetic_pdata.offset_enable = 1;
	prev = type;
}

struct acc_platform_data accelerometer_pdata = {
	.cal_path = ACCEL_CAL_PATH,
	.ldo_ctl = true,
};

static struct i2c_board_info __initdata espresso10_sensors_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("accelerometer", 0x18),
		.platform_data = &accelerometer_pdata,
	 },

	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
		.platform_data = &magnetic_pdata,
	 },

	{
		I2C_BOARD_INFO("bh1721fvc", 0x23),
		.platform_data = &bh1721fvc_pdata,
	 },
};

void __init omap4_espresso10_sensors_init(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(sensors_gpios); i++)
		sensors_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(sensors_gpios[i].label);

	gpio_request_array(sensors_gpios, ARRAY_SIZE(sensors_gpios));

	omap_mux_init_gpio(sensors_gpios[GPIO_MSENSE_IRQ].gpio,
		OMAP_PIN_OUTPUT);

	gpio_free(sensors_gpios[GPIO_MSENSE_IRQ].gpio);

	gpio_request(sensors_gpios[GPIO_MSENSE_IRQ].gpio, "MSENSE_IRQ");

	gpio_direction_output(sensors_gpios[GPIO_MSENSE_IRQ].gpio, 1);

	i2c_register_board_info(4, espresso10_sensors_i2c4_boardinfo,
				ARRAY_SIZE(espresso10_sensors_i2c4_boardinfo));

}


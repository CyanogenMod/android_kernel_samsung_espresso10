/* arch/arm/mach-omap2/board-palau-sensors.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
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

#include "board-palau.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include <linux/regulator/consumer.h>

#include <linux/mpu6050_input.h>
#include <linux/yas.h>
#include <linux/gp2ap020.h>

enum {
	MPU6050 = 0,
	COMPASS,
	PROX_LIGHT,
};

enum {
	GPIO_MSENSE_INT = 0,
	GPIO_PS_ALS_INT,
	GPIO_GYRO_INT,
	GPIO_PS_ON,
};

enum {
	GP2AP020 = 0,
	GP2AP030,
};


struct gpio sensors_gpios[] = {
	[GPIO_MSENSE_INT] = {
		.flags = GPIOF_IN,
		.label = "MSENSE_INT",
	},
	[GPIO_PS_ALS_INT] = {
		.flags = GPIOF_IN,
		.label = "PS_ALS_INT",
	},
	[GPIO_GYRO_INT] = {
		.flags = GPIOF_IN,
		.label = "GYRO_INT",
	},
	[GPIO_PS_ON] = {
		.flags = GPIOF_OUT_INIT_HIGH,
		.label = "PS_ON",
	},
};

static struct mpu6050_input_platform_data mpu6050_pdata = {
	.orientation = {-1, 0, 0,
				0, 1, 0,
				0, 0, -1},
	.acc_cal_path = "/efs/calibration_data",
	.gyro_cal_path = "/efs/gyro_cal_data",
};

static struct mpu6050_input_platform_data mpu6050_pdata_rev02 = {
	.orientation = {-1, 0, 0,
				0, -1, 0,
				0, 0, 1},
	.acc_cal_path = "/efs/calibration_data",
	.gyro_cal_path = "/efs/gyro_cal_data",
};

static void gp2a_set_led_regulator(bool on)
{
	struct regulator *gp2a_vled;

	gp2a_vled = regulator_get(NULL, "PS_VLED_3.0V");

	if (IS_ERR(gp2a_vled)) {
		pr_err("gp2a: cannot get PS_VLED_3.0V\n");
		goto done;
	}

	if (on) {
		pr_info("gp2a: led on.\n");
		regulator_enable(gp2a_vled);
	} else {
		pr_info("gp2a: led off.\n");
		regulator_disable(gp2a_vled);
	}

	regulator_put(gp2a_vled);
done:
	return;
}

static void gp2a_led_on(bool on)
{
	if (system_rev > 0)
		gp2a_set_led_regulator(on);
}

static void gp2a_power_on(bool on)
{
	if (unlikely(!system_rev))
		gpio_direction_output(sensors_gpios[GPIO_PS_ON].gpio, on);
}

static struct gp2ap020_pdata gp2a020_data = {
	.power_on = gp2a_power_on,
	.led_on = gp2a_led_on,
	.p_out = 0,
	.version = GP2AP020,
	.prox_cal_path = "/efs/prox_cal",
};

struct mag_platform_data magnetic_pdata = {
	.offset_enable = 0,
	.chg_status = CABLE_TYPE_NONE,
	.ta_offset.v = {0, 0, 0},
	.usb_offset.v = {0, 0, 0},
	.full_offset.v = {0, 0, 0},
};

static struct i2c_board_info __initdata palau_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("mpu6050_input", 0x68),
		.platform_data = &mpu6050_pdata,
	},
	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
		.platform_data = &magnetic_pdata,
	},
	{
		I2C_BOARD_INFO("gp2a020", 0x39),
		.platform_data = &gp2a020_data,
	},
};

void __init omap4_palau_sensors_init(void)
{
	int i = ARRAY_SIZE(sensors_gpios) - 1;

	do {
		sensors_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(sensors_gpios[i].label);
	} while (i-- != 0);
	gpio_request_array(sensors_gpios, ARRAY_SIZE(sensors_gpios));

	palau_i2c4_boardinfo[MPU6050].irq =
		OMAP_GPIO_IRQ(sensors_gpios[GPIO_GYRO_INT].gpio);
	gp2a020_data.p_out =
		sensors_gpios[GPIO_PS_ALS_INT].gpio;
	if (likely(system_rev > 1))
		gp2a020_data.version = GP2AP030;
	if (likely((system_rev == 2) || (system_rev > 3))) {
		magnetic_pdata.orientation = YAS532_POSITION_3;
		palau_i2c4_boardinfo[MPU6050].platform_data =
							&mpu6050_pdata_rev02;
	}

	i2c_register_board_info(4, palau_i2c4_boardinfo,
		ARRAY_SIZE(palau_i2c4_boardinfo));
}

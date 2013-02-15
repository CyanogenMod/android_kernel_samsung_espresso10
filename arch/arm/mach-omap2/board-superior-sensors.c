/* arch/arm/mach-omap2/board-superior-sensors.c
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

#include "board-superior.h"
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
	NUM_PS_ALS_INT = 0,
	NUM_GYRO_INT,
};

enum {
	GP2AP020 = 0,
	GP2AP030,
};


struct gpio sensors_gpios[] = {
	[NUM_PS_ALS_INT] = {
		.flags = GPIOF_IN,
		.label = "PS_ALS_INT",
	},
	[NUM_GYRO_INT] = {
		.flags = GPIOF_IN,
		.label = "GYRO_INT",
	},
};

static void superior_sensors_regulator_on(bool on)
{
	struct regulator *reg_v28;
	struct regulator *reg_v18;

	reg_v28 =
		regulator_get(NULL, "SENSOR_IO_2.8V");
	if (IS_ERR(reg_v28)) {
		pr_err("%s [%d] failed to get v2.8 regulator.\n",
			__func__, __LINE__);
		goto done;
	}
	reg_v18 =
		regulator_get(NULL, "SENSOR_IO_1.8V");
	if (IS_ERR(reg_v18)) {
		pr_err("%s [%d] failed to get v1.8 regulator.\n",
			__func__, __LINE__);
		goto done;
	}
	if (on) {
		pr_info("sensor ldo on.\n");
		regulator_enable(reg_v28);
		usleep_range(1000, 1100);
		regulator_enable(reg_v18);
	} else {
		pr_info("sensor ldo off.\n");
		regulator_disable(reg_v18);
		usleep_range(1000, 1100);
		regulator_disable(reg_v28);
	}
	regulator_put(reg_v28);
	regulator_put(reg_v18);
	usleep_range(10000, 11000);
done:
	return;
}

static struct mpu6050_input_platform_data mpu6050_pdata = {
	.power_on = superior_sensors_regulator_on,
	.orientation = {0, 1, 0,
				-1, 0, 0,
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
		gp2a_set_led_regulator(on);
}

static struct gp2ap020_pdata gp2a020_data = {
	.power_on = superior_sensors_regulator_on,
	.led_on = gp2a_led_on,
	.p_out = 0,
	.version = GP2AP030,
	.prox_cal_path = "/efs/prox_cal",
};

struct mag_platform_data magnetic_pdata = {
	.power_on = superior_sensors_regulator_on,
	.offset_enable = 0,
	.chg_status = CABLE_TYPE_NONE,
	.ta_offset.v = {0, 0, 0},
	.usb_offset.v = {0, 0, 0},
	.full_offset.v = {0, 0, 0},
};

static struct i2c_board_info __initdata superior_i2c4_boardinfo[] = {
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
#if defined(CONFIG_OMAP4_TMP102_SENSOR)
	{
		I2C_BOARD_INFO("tmp102_temp_sensor", 0x48),
	},
#endif
};

void __init omap4_superior_sensors_init(void)
{
	int i = ARRAY_SIZE(sensors_gpios) - 1;

	do {
		sensors_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(sensors_gpios[i].label);
	} while (i-- != 0);
	gpio_request_array(sensors_gpios, ARRAY_SIZE(sensors_gpios));

	superior_i2c4_boardinfo[MPU6050].irq =
		OMAP_GPIO_IRQ(sensors_gpios[NUM_GYRO_INT].gpio);
	gp2a020_data.p_out =
		sensors_gpios[NUM_PS_ALS_INT].gpio;

	gp2a020_data.d0_value[D0_BND] = 91;
	gp2a020_data.d0_value[D0_COND1] = 41;
	gp2a020_data.d0_value[D0_COND1_A] = 839;
	gp2a020_data.d0_value[D0_COND2] = 62;
	gp2a020_data.d0_value[D0_COND2_A] = 2121;
	gp2a020_data.d0_value[D0_COND2_B] = 3124;
	gp2a020_data.d0_value[D0_COND3_A] = 574;
	gp2a020_data.d0_value[D0_COND3_B] = 631;

	if (unlikely(system_rev < 4))
		magnetic_pdata.orientation = YAS532_POSITION_3;

	i2c_register_board_info(4, superior_i2c4_boardinfo,
		ARRAY_SIZE(superior_i2c4_boardinfo));
}

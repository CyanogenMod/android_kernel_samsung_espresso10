/* arch/arm/mach-omap2/board-kona-sensors.c
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

#include "board-kona.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include <linux/regulator/consumer.h>

#include <linux/mpu6050_input.h>
#include <linux/yas.h>
#include <linux/gp2a.h>
#include <linux/i2c/twl6030-madc.h>
#include <linux/gp2ap020.h>
#include <linux/al3201.h>
#include <plat/tmp102_temp_sensor.h>

enum {
	MPU6050 = 0,
	ACCEL = 0,
	COMPASS,
	PROX_LIGHT,
	LIGHT,
};

enum {
	NUM_PS_VOUT = 0,
	NUM_PS_ALS_INT,
	NUM_GYRO_INT,
	NUM_ACC_INT,
};

struct gpio sensors_gpios[] = {
	[NUM_PS_VOUT] = {
		.flags = GPIOF_IN,
		.label = "PS_VOUT",
	},
	[NUM_PS_ALS_INT] = {
		.flags = GPIOF_IN,
		.label = "PS_ALS_INT",
	},
	[NUM_GYRO_INT] = {
		.flags = GPIOF_IN,
		.label = "GYRO_INT",
	},
	[NUM_ACC_INT] = {
		.flags = GPIOF_IN,
		.label = "ACC_INT",
	},
};

enum {
	GP2AP020 = 0,
	GP2AP030,
};

#define GP2A_LIGHT_ADC_CHANNEL	3
#define GP2A_DUMMY 1

static void kona_sensors_regulator_on(bool on)
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

static int gp2a_light_adc_value(void)
{
	int adc;

	if (GP2A_DUMMY) {
		adc = 1000;
	} else {
		adc =
			twl6030_get_madc_conversion(
			GP2A_LIGHT_ADC_CHANNEL);
	}
	return adc;
}

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

static struct gp2a_platform_data gp2a_pdata = {
	.power = kona_sensors_regulator_on,
	.led_on = gp2a_led_on,
	.p_out = 0,
	.light_adc_value = gp2a_light_adc_value,
};

static struct gp2ap020_pdata gp2a020_data = {
	.power_on = kona_sensors_regulator_on,
	.led_on = gp2a_led_on,
	.p_out = 0,
	.version = GP2AP030,
	.prox_cal_path = "/efs/prox_cal",
};

static struct mpu6050_input_platform_data mpu6050_pdata = {
	.power_on = kona_sensors_regulator_on,
	.orientation = {1, 0, 0,
		0, -1, 0,
		0, 0, -1},
	.acc_cal_path = "/efs/calibration_data",
	.gyro_cal_path = "/efs/gyro_cal_data",
};

static struct mpu6050_input_platform_data mpu6050_pdata_rev02 = {
	.power_on = kona_sensors_regulator_on,
	.orientation = {0, 1, 0,
		1, 0, 0,
		0, 0, -1},
	.acc_cal_path = "/efs/calibration_data",
	.gyro_cal_path = "/efs/gyro_cal_data",
};

static struct mag_platform_data magnetic_pdata = {
	.power_on = kona_sensors_regulator_on,
	.offset_enable = 0,
	.chg_status = CABLE_TYPE_NONE,
	.ta_offset.v = {0, 0, 0},
	.usb_offset.v = {0, 0, 0},
	.full_offset.v = {0, 0, 0},
};

static struct acc_platform_data accelerometer_pdata = {
	.cal_path = "/efs/calibration_data",
	.ldo_on = kona_sensors_regulator_on,
};

static struct al3201_platform_data al3201_pdata = {
	.power_on = kona_sensors_regulator_on,
};

static struct tmp102_platform_data tmp102_data = {
	.power_on = kona_sensors_regulator_on,
};

static struct i2c_board_info __initdata kona_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("mpu6050_input", 0x68),
		.platform_data = &mpu6050_pdata,
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

static struct i2c_board_info __initdata kona_i2c4_boardinfo_rev02[] = {
	{
		I2C_BOARD_INFO("mpu6050_input", 0x68),
		.platform_data = &mpu6050_pdata_rev02,
	},
	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
		.platform_data = &magnetic_pdata,
	},
	{
		I2C_BOARD_INFO("gp2a020", 0x39),
		.platform_data = &gp2a020_data,
	},
	{
		I2C_BOARD_INFO("AL3201", 0x1c),
		.platform_data = &al3201_pdata,
	},
};

static struct i2c_board_info __initdata kona_i2c4_boardinfo_rev03[] = {
	{
		I2C_BOARD_INFO("accelerometer", 0x18),
		.platform_data = &accelerometer_pdata,
	},
	{
		I2C_BOARD_INFO("geomagnetic", 0x2e),
		.platform_data = &magnetic_pdata,
	},
	{
		I2C_BOARD_INFO("gp2a020", 0x39),
		.platform_data = &gp2a020_data,
	},
	{
		I2C_BOARD_INFO("AL3201", 0x1c),
		.platform_data = &al3201_pdata,
	},
#if defined(CONFIG_OMAP4_TMP102_SENSOR)
	{
		I2C_BOARD_INFO("tmp102_temp_sensor", 0x48),
		.platform_data = &tmp102_data,
	},
#endif
};

void __init omap4_kona_sensors_init(void)
{
	int i = ARRAY_SIZE(sensors_gpios) - 1;

	do {
		sensors_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(sensors_gpios[i].label);
	} while (i-- != 0);
	gpio_request_array(sensors_gpios, ARRAY_SIZE(sensors_gpios));

	kona_i2c4_boardinfo[MPU6050].irq =
		OMAP_GPIO_IRQ(sensors_gpios[NUM_GYRO_INT].gpio);
	kona_i2c4_boardinfo_rev02[MPU6050].irq =
		OMAP_GPIO_IRQ(sensors_gpios[NUM_GYRO_INT].gpio);

	gp2a_pdata.p_out = sensors_gpios[NUM_PS_VOUT].gpio;
	gp2a020_data.p_out = sensors_gpios[NUM_PS_ALS_INT].gpio;

	if (system_rev > 4) {
		kona_i2c4_boardinfo_rev03[ACCEL].irq =
			OMAP_GPIO_IRQ(sensors_gpios[NUM_ACC_INT].gpio);
		i2c_register_board_info(4, kona_i2c4_boardinfo_rev03,
			ARRAY_SIZE(kona_i2c4_boardinfo_rev03));
		accelerometer_pdata.orientation = YAS532_POSITION_4;
		magnetic_pdata.orientation = YAS532_POSITION_2;
	} else if (system_rev > 2) {
		kona_i2c4_boardinfo_rev03[ACCEL].irq =
			OMAP_GPIO_IRQ(sensors_gpios[NUM_ACC_INT].gpio);
		i2c_register_board_info(4, kona_i2c4_boardinfo_rev03,
			ARRAY_SIZE(kona_i2c4_boardinfo_rev03));
		magnetic_pdata.orientation = YAS532_POSITION_2;
	} else if (system_rev > 1) {
		i2c_register_board_info(4, kona_i2c4_boardinfo_rev02,
			ARRAY_SIZE(kona_i2c4_boardinfo_rev02));
		magnetic_pdata.orientation = YAS532_POSITION_7;
	} else
		i2c_register_board_info(4, kona_i2c4_boardinfo,
			ARRAY_SIZE(kona_i2c4_boardinfo));
}

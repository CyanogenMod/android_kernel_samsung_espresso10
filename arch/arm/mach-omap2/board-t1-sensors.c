/* arch/arm/mach-omap2/board-t1-sensors.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd
 *
 * Based on mach-omap2/board-tuna-sensors.c
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
#include <linux/mpu_v333.h>
#include <linux/gpio.h>
#include <linux/cm3663.h>
#include <linux/i2c/ak8975.h>
#include "mux.h"
#include "omap_muxtbl.h"

enum {
	MPU3050 = 0,
	KXTF9,
	COMPASS,
	CM3663,
};

enum {
	GPIO_SENSOR_EN = 0,
	GPIO_GYRO_INT,
	GPIO_PS_ALS_INT_18,
	GPIO_PS_ON,
};


struct gpio sensors_gpios[] = {
	[GPIO_SENSOR_EN] = {
		.flags = GPIOF_OUT_INIT_HIGH,
		.label = "SENSOR_EN",
	},
	[GPIO_GYRO_INT] = {
		.flags = GPIOF_IN,
		.label = "GYRO_INT",
	},
	[GPIO_PS_ALS_INT_18] = {
		.flags = GPIOF_IN,
		.label = "PS_ALS_INT_18",
	},
	[GPIO_PS_ON] = {
		.flags = GPIOF_OUT_INIT_HIGH,
		.label = "PS_ON",
	},
};

/* Platform data for rev 3-6 */
static struct mpu3050_platform_data mpu_data_rev_03 = {
	.int_config  = 0x12,
	.orientation = {  -1,  0,  0,
			  0,  -1,  0,
			  0,  0,  1 },

	.accel = {
		.adapt_num   = 4,
		.bus         = EXT_SLAVE_BUS_SECONDARY,
		.address     = 0x0F,
		.orientation = {  0, 1, 0,
				  1, 0, 0,
				  0, 0, -1 },
	},

	.compass = {
		.adapt_num   = 4,
		.bus         = EXT_SLAVE_BUS_PRIMARY,
		.address     = 0x2E,
		.orientation = {  1,  0,  0,
				  0,  1,  0,
				  0,  0,  1 },
	},
};

/* Platform data for revision 7 */
static struct mpu3050_platform_data mpu_data_rev_07 = {
	.int_config = 0x12,
	.orientation =  {-1, 0, 0,
			0, -1, 0,
			0, 0, 1},

	.accel = {
		  .adapt_num = 4,
		  .bus = EXT_SLAVE_BUS_SECONDARY,
		  .address = 0x0F,
		  .orientation = {0, 1, 0,
				  1, 0, 0,
				  0, 0, -1},
		  },

	.compass = {
		    .adapt_num = 4,
		    .bus = EXT_SLAVE_BUS_PRIMARY,
		    .address = 0x0C,
		    .orientation = {1, 0, 0,
				    0, 1, 0,
				    0, 0, 1},
		    },
};

static int cm3663_power(bool on)
{
	gpio_direction_output(sensors_gpios[GPIO_PS_ON].gpio, on);
	return 0;
}

static struct cm3663_platform_data cm3663_pdata = {
	.proximity_power = cm3663_power,
};

/* For hardware revisions 3-6 */
static struct i2c_board_info __initdata t1_i2c4_boardinfo_3[] = {
	{
		I2C_BOARD_INFO("mpu3050", 0x68),
	},
	{
		I2C_BOARD_INFO("kxtf9", 0x0f),
	},
	{
		I2C_BOARD_INFO("yas530", 0x2e),
	},
	{
		I2C_BOARD_INFO("cm3663", 0x11),
	},

};
/* For hardware revision 7 */
static struct i2c_board_info __initdata t1_i2c4_boardinfo_7[] = {
	{
		I2C_BOARD_INFO("mpu3050", 0x68),
	},
	{
		I2C_BOARD_INFO("kxtf9", 0x0f),
	},
	{
		I2C_BOARD_INFO("ak8975", 0x0c),
	},
	{
		I2C_BOARD_INFO("cm3663", 0x11),
	},

};

void __init omap4_t1_sensors_init(void)
{

	int i;
	for (i = 0; i < ARRAY_SIZE(sensors_gpios); i++)
		sensors_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(sensors_gpios[i].label);
	gpio_request_array(sensors_gpios, ARRAY_SIZE(sensors_gpios));
	if (system_rev < 7) {
		t1_i2c4_boardinfo_3[MPU3050].platform_data = &mpu_data_rev_03;

		t1_i2c4_boardinfo_3[COMPASS].platform_data =
						&mpu_data_rev_03.compass;
		t1_i2c4_boardinfo_3[CM3663].platform_data =
						&cm3663_pdata;
		t1_i2c4_boardinfo_3[MPU3050].irq =
			OMAP_GPIO_IRQ(sensors_gpios[GPIO_GYRO_INT].gpio);

		cm3663_pdata.irq =
			sensors_gpios[GPIO_PS_ALS_INT_18].gpio;
	}

	else {
		t1_i2c4_boardinfo_7[MPU3050].platform_data =
						&mpu_data_rev_07;
		t1_i2c4_boardinfo_7[COMPASS].platform_data =
						&mpu_data_rev_07.compass;
		t1_i2c4_boardinfo_7[CM3663].platform_data =
						&cm3663_pdata;
		t1_i2c4_boardinfo_7[MPU3050].irq =
			OMAP_GPIO_IRQ(sensors_gpios[GPIO_GYRO_INT].gpio);
		cm3663_pdata.irq =
			sensors_gpios[GPIO_PS_ALS_INT_18].gpio;

	}

		if (system_rev < 7)
			i2c_register_board_info(4, t1_i2c4_boardinfo_3,
					ARRAY_SIZE(t1_i2c4_boardinfo_3));
		else
			i2c_register_board_info(4, t1_i2c4_boardinfo_7,
					ARRAY_SIZE(t1_i2c4_boardinfo_7));
}



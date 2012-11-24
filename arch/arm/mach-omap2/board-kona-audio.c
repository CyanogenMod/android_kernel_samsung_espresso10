/* arch/arm/mach-omap2/board-kona-audio.c
 *
 * Based on mach-omap2/board-palau-audio.c
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>

#include "board-kona.h"
#include "mux.h"
#include "omap_muxtbl.h"

static const struct regulator_consumer_supply vbatt_supplies[] = {
	REGULATOR_SUPPLY("LDO1VDD", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD1", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "1-001a"),
	REGULATOR_SUPPLY("AVDD2", "1-001a"),
	REGULATOR_SUPPLY("CPVDD", "1-001a"),
	REGULATOR_SUPPLY("DBVDD1", "1-001a"),
	REGULATOR_SUPPLY("DBVDD2", "1-001a"),
	REGULATOR_SUPPLY("DBVDD3", "1-001a"),
};

static const struct regulator_init_data vbatt_initdata = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(vbatt_supplies),
	.consumer_supplies = vbatt_supplies,
};

static const struct fixed_voltage_config vbatt_config = {
	.init_data = &vbatt_initdata,
	.microvolts = 1800000,
	.supply_name = "VBATT",
	.gpio = -EINVAL,
};

static struct platform_device vbatt_device = {
	.name	= "reg-fixed-voltage",
	.id	= -1,
	.dev = {
		.platform_data = &vbatt_config,
	},
};

static const struct regulator_consumer_supply wm1811_ldo1_supplies[] = {
	REGULATOR_SUPPLY("AVDD1", "1-001a"),
};

static const struct regulator_init_data wm1811_ldo1_initdata = {
	.constraints = {
		.name = "WM1811 LDO1",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo1_supplies),
	.consumer_supplies = wm1811_ldo1_supplies,
};

static const struct regulator_consumer_supply wm1811_ldo2_supplies[] = {
	REGULATOR_SUPPLY("DCVDD", "1-001a"),
};

static const struct regulator_init_data wm1811_ldo2_initdata = {
	.constraints = {
		.name = "WM1811 LDO2",
		.always_on = true,  /* Actually status changed by LDO1 */
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo2_supplies),
	.consumer_supplies = wm1811_ldo2_supplies,
};

static struct wm8994_pdata wm1811_pdata = {
	.gpio_defaults = {
		[0] = WM8994_GP_FN_IRQ,
		[7] = WM8994_GPN_DIR | WM8994_GP_FN_PIN_SPECIFIC,
		[8] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
		[9] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
		[10] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
	},

	/* for using wm1811 jack detect */
	.irq_base = TWL6040_CODEC_IRQ_BASE,

	.ldo = {
		{
			.init_data = &wm1811_ldo1_initdata,
		},
		{
			.init_data = &wm1811_ldo2_initdata,
		}
	},

	/* Support external capacitors*/
	.jd_ext_cap = 1,

	/* Regulated mode at highest output voltage */
	.micbias = { 0x2f, 0x29 },

	.micd_lvl_sel = 0xFF,

	.ldo_ena_always_driven = true,
};

static struct i2c_board_info kona_audio_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("wm1811", 0x34 >> 1),
		.platform_data = &wm1811_pdata,
	},
};

void __init omap4_kona_audio_init(void)
{
	unsigned int gpio_ear_send_end;
	unsigned int gpio_ear_bias_discharge;

	platform_device_register(&vbatt_device);

	wm1811_pdata.ldo[0].enable =
		omap_muxtbl_get_gpio_by_name("CODEC_LDO_EN");

	gpio_ear_send_end =
		omap_muxtbl_get_gpio_by_name("EAR_SEND_END");
	gpio_request(gpio_ear_send_end, "EAR_SEND_END");
	kona_audio_i2c1_board_info[0].irq = gpio_to_irq(gpio_ear_send_end);

	/* board rev 2,3,4 only use ear-bias-discharge */
	if ((1 < system_rev) && (system_rev < 5)) {
		gpio_ear_bias_discharge =
		omap_muxtbl_get_gpio_by_name("EAR_BIAS_DISCHARGE");
		gpio_request(gpio_ear_bias_discharge, "EAR_BIAS_DISCHARGE");
		gpio_direction_output(gpio_ear_bias_discharge, 0);
	}

	i2c_register_board_info(1, kona_audio_i2c1_board_info,
				ARRAY_SIZE(kona_audio_i2c1_board_info));
}

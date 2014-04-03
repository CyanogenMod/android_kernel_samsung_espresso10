/* arch/arm/mach-omap2/board-gokey-audio.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
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

#include "board-gokey.h"
#include "mux.h"
#include "omap_muxtbl.h"

#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>


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

/* set the DRC, when builtin mic enable */
static const struct wm8994_drc_cfg wm1811_drc_cfgs[] = {
	[0] = {
		.name	= "AIF1DRC Mode",
		.regs[0]	= 0x019A,
		.regs[1]	= 0x0052,
		.regs[2]	= 0x0020,
		.regs[3]	= 0x03DE,
	},
	[1] = {
		.name	= "None",
	},
	[2] = {
		.name	= "AIF2DRC Mode",
		.regs[0]	= 0x0098,
		.regs[1]	= 0x0845,
		.regs[2]	= 0x0000,
		.regs[3]	= 0x0000,
		.regs[3]	= 0x0000,
	},
};

static struct wm8994_pdata wm1811_pdata = {
	.gpio_defaults = {
		[0] = WM8994_GP_FN_IRQ,
		[7] = WM8994_GPN_DIR | WM8994_GP_FN_PIN_SPECIFIC,
		[8] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
		[9] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
		[10] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
	},


	/* for using wm1811 jack detect				*/
	/* This line should be remained for next board.	*/
	.irq_base = TWL6040_CODEC_IRQ_BASE,

	/* Gokey has  external decoupling capacitors at mic bias2. */
	.jd_ext_cap = 1,

	.ldo = {
		{
			.init_data = &wm1811_ldo1_initdata,
		},
		{
			.init_data = &wm1811_ldo2_initdata,
		}
	},

	/* Regulated mode at highest output voltage */
	.micbias = { 0x2f, 0x29 },

	.ldo_ena_always_driven = true,

	.num_drc_cfgs = 1,
	.drc_cfgs = wm1811_drc_cfgs,
};

static struct i2c_board_info gokey_audio_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("wm1811", 0x34>>1),
		.flags		   = I2C_CLIENT_WAKE,
		.platform_data  = &wm1811_pdata,
	},
};

/*
 * Initialing the audio platform data
 * for controlling the device audio main power feature.
 */
void __init omap4_gokey_audio_init(void)
{
	platform_device_register(&vbatt_device);

	wm1811_pdata.ldo[0].enable =
		omap_muxtbl_get_gpio_by_name("CODEC_LDO_EN");

	/* wm1811 i2c's irq to the sendend gpio. */
	gokey_audio_i2c1_board_info[0].irq
		= gpio_to_irq(omap_muxtbl_get_gpio_by_name("EAR_SEND_END"));

	i2c_register_board_info(1, gokey_audio_i2c1_board_info,
				ARRAY_SIZE(gokey_audio_i2c1_board_info));
}

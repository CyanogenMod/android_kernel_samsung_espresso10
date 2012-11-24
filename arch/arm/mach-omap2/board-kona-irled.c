/* arch/arm/mach-omap2/board-kona-irled.c
 *
 * Copyright (C) 2012 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>

#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "board-kona.h"

#include <linux/sec_irled.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>

enum {
	GPIO_IRDA_EN = 0,
	GPIO_IRDA_WAKE,
	GPIO_IRDA_IRQ,
};

static struct gpio irled_gpios[] = {
	[GPIO_IRDA_EN] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "IRDA_EN",
	},
	[GPIO_IRDA_WAKE] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "IRDA_WAKE",
	},
	[GPIO_IRDA_IRQ] = {
		.flags = GPIOF_IN,
		.label = "IRDA_IRQ",
	},
};

static void irda_wake_en(bool onoff)
{
	gpio_direction_output(irled_gpios[GPIO_IRDA_WAKE].gpio, onoff);
	gpio_direction_output(irled_gpios[GPIO_IRDA_EN].gpio, onoff);
}

static void irda_vdd_onoff(bool onoff)
{
	int ret = 0;
	static struct regulator *irled_vdd;

	if (unlikely(!irled_vdd)) {
		irled_vdd = regulator_get(NULL, "IrLED_2.0V");
		if (IS_ERR(irled_vdd)) {
			pr_err("irled: could not get irled_vdd regulator\n");
			return;
		}
	}

	if (onoff) {
		ret = regulator_enable(irled_vdd);
		if (ret) {
			pr_err("irled: failed to enable irled_vdd regulator\n");
			return;
		}
		msleep(100);
	} else {
		if (regulator_is_enabled(irled_vdd))
			ret = regulator_disable(irled_vdd);

		if (ret) {
			pr_err("irled: failed to disable irled_vdd regulator\n");
			return;
		}
		msleep(20);
	}
}

static struct sec_irled_platform_data mc96_pdata = {
	.ir_wake_en = irda_wake_en,
	.ir_vdd_onoff = irda_vdd_onoff,
};

static struct i2c_board_info __initdata kona_irled_boardinfo[] = {
	{
		I2C_BOARD_INFO("mc96", (0xA0 >> 1)),
		.platform_data = &mc96_pdata,
	},
};

static void __init kona_irled_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irled_gpios); i++)
		irled_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(irled_gpios[i].label);
	gpio_request_array(irled_gpios, ARRAY_SIZE(irled_gpios));
}

void __init omap4_kona_irled_init(void)
{
	kona_irled_gpio_init();

	i2c_register_board_info(11, kona_irled_boardinfo,
				ARRAY_SIZE(kona_irled_boardinfo));
}

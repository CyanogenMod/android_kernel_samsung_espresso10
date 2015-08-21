/* arch/arm/mach-omap2/board-superior-fmradio.c
 *
 * Based on mach-omap2/board-t1-fmradio.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd
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
#include <linux/delay.h>
#include <linux/i2c/si47xx_common.h>

#include "board-superior.h"
#include "mux.h"
#include "omap_muxtbl.h"

enum {
	GPIO_FM_RST = 0,
	GPIO_FM_INT
};

static struct gpio fm_gpios[] = {
	[GPIO_FM_RST] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "FM_RST",
	},
	[GPIO_FM_INT] = {
		.flags	= GPIOF_IN,
		.label	= "FM_INT",
	},
};

static void fmradio_power(int on)
{
	if (on) {
		omap_mux_set_gpio(OMAP_PIN_OUTPUT | OMAP_MUX_MODE3,
						fm_gpios[GPIO_FM_INT].gpio);
		gpio_set_value(fm_gpios[GPIO_FM_INT].gpio, 0);
		usleep_range(10, 15);
		gpio_set_value(fm_gpios[GPIO_FM_RST].gpio, 1);
		usleep_range(5, 10);
		omap_mux_set_gpio(OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP \
						| OMAP_PIN_OFF_WAKEUPENABLE,
						fm_gpios[GPIO_FM_INT].gpio);
		usleep_range(10, 15);
	} else {
		gpio_set_value(fm_gpios[GPIO_FM_RST].gpio, 0);
	}
}

static struct si47xx_platform_data si47xx_pdata = {
	.rx_vol = {0x0, 0x13, 0x16, 0x19, 0x1C, 0x1F, 0x22, 0x25,
				0x28, 0x2B, 0x2E, 0x31, 0x34, 0x37, 0x3A, 0x3D},
	.power = fmradio_power,
};

static struct i2c_board_info __initdata superior_fmradio_i2c_binfo[] = {
	{
		I2C_BOARD_INFO("Si47xx", (0x22 >> 1)),
		.platform_data	= &si47xx_pdata,
	},
};

static void __init superior_fmradio_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fm_gpios); i++)
		fm_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(fm_gpios[i].label);
	gpio_request_array(fm_gpios, ARRAY_SIZE(fm_gpios));

	superior_fmradio_i2c_binfo[0].irq =
		gpio_to_irq(fm_gpios[GPIO_FM_INT].gpio);

	omap_mux_set_gpio(OMAP_PIN_OUTPUT | OMAP_MUX_MODE3,
					fm_gpios[GPIO_FM_INT].gpio);
	gpio_set_value(fm_gpios[GPIO_FM_INT].gpio, 1);
	gpio_set_value(fm_gpios[GPIO_FM_RST].gpio, 0);
}

void __init omap4_superior_fmradio_init(void)
{
	superior_fmradio_gpio_init();

	i2c_register_board_info(4, superior_fmradio_i2c_binfo,
			ARRAY_SIZE(superior_fmradio_i2c_binfo));
}

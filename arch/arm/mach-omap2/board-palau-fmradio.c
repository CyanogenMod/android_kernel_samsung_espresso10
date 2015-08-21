/* arch/arm/mach-omap2/board-palau-fmradio.c
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
#include <linux/platform_data/fm-si470x.h>

#include "board-palau.h"
#include "mux.h"
#include "omap_muxtbl.h"
static void palau_fmradio_reset_gpio_on(int enable);

enum {
	GPIO_FM_RST = 0,
	GPIO_FM_INT
};

static struct gpio fm_gpios[] = {
	[GPIO_FM_RST] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "FM_RST",
	},
	[GPIO_FM_INT] = {
		.flags	= GPIOF_IN,
		.label	= "FM_INT",
	},
};

static struct si470x_platform_data palau_si470x_pdata = {
	.reset_gpio_on	= palau_fmradio_reset_gpio_on,
};

static void palau_fmradio_reset_gpio_on(int enable)
{
	if (enable) {
		palau_si470x_pdata.enabled = true;
		gpio_set_value(fm_gpios[GPIO_FM_RST].gpio, 1);
	} else {
		gpio_set_value(fm_gpios[GPIO_FM_RST].gpio, 0);
		palau_si470x_pdata.enabled = false;
	}
}

static struct i2c_board_info __initdata palau_fmradio_i2c_binfo[] = {
	{
		I2C_BOARD_INFO("Si47xx", (0x22 >> 1)),
		.platform_data	= &palau_si470x_pdata,
	},
};

static void __init palau_fmradio_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fm_gpios); i++)
		fm_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(fm_gpios[i].label);
	gpio_request_array(fm_gpios, ARRAY_SIZE(fm_gpios));

	palau_fmradio_i2c_binfo[0].irq =
		gpio_to_irq(fm_gpios[GPIO_FM_INT].gpio);
	palau_si470x_pdata.gpio = fm_gpios[GPIO_FM_INT].gpio;
}

void __init omap4_palau_fmradio_init(void)
{
/* TODO: hw_rev check is not corrected, it need to check later */
#if 0
	if (unlikely(system_rev < 2))
		return;
#endif

	palau_fmradio_gpio_init();

	i2c_register_board_info(4, palau_fmradio_i2c_binfo,
			ARRAY_SIZE(palau_fmradio_i2c_binfo));
}

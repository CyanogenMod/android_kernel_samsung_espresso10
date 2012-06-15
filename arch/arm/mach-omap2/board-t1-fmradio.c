/* arch/arm/mach-omap2/board-t1-fmradio.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd
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

#include "board-t1.h"
#include "mux.h"
#include "omap_muxtbl.h"
static void t1_fmradio_reset_gpio_on(int enable);

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

static struct si470x_platform_data t1_si470x_pdata = {
	.reset_gpio_on	= t1_fmradio_reset_gpio_on,
};

static void t1_fmradio_reset_gpio_on(int enable)
{
	if (enable) {
		t1_si470x_pdata.enabled = true;
		gpio_set_value(fm_gpios[GPIO_FM_RST].gpio, 1);
	} else {
		gpio_set_value(fm_gpios[GPIO_FM_RST].gpio, 0);
		t1_si470x_pdata.enabled = false;
	}
}

static struct i2c_board_info t1_fmradio_i2c_binfo[] = {
	{
		I2C_BOARD_INFO("si470x", 0x10),
		.platform_data	= &t1_si470x_pdata,
	},
};

static void __init t1_fmradio_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fm_gpios); i++)
		fm_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(fm_gpios[i].label);
	gpio_request_array(fm_gpios, ARRAY_SIZE(fm_gpios));

	t1_fmradio_i2c_binfo[0].irq =
		gpio_to_irq(fm_gpios[GPIO_FM_INT].gpio);
	t1_si470x_pdata.gpio = fm_gpios[GPIO_FM_INT].gpio;
}

void __init omap4_t1_fmradio_init(void)
{
	t1_fmradio_gpio_init();

	i2c_register_board_info(3, t1_fmradio_i2c_binfo,
			ARRAY_SIZE(t1_fmradio_i2c_binfo));
}

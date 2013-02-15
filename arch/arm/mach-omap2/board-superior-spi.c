/* arch/arm/mach-omap2/board-superior-spi.c
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

#include "control.h"

#include "mux.h"
#include "omap_muxtbl.h"

#include <linux/phone_svn/ipc_spi.h>
#include <plat/mcspi.h>
#include <linux/irq.h>
#include <linux/spi/spi.h>

static struct ipc_spi_platform_data spi_modem_data;

static struct omap2_mcspi_device_config board_spi_mcspi_config = {
	.turbo_mode =	1,
	.single_channel = 1,
};

static struct spi_board_info superior_omap4_spi_board_info[] __initdata = {
	[0] = {
		.modalias = "if_spi_driver",
		.bus_num = 2,
		.chip_select = 0,
		.max_speed_hz = 12000000,
		.controller_data = &board_spi_mcspi_config,
	},
};

static struct platform_device spi_modem = {
	.name = "if_spi_platform_driver",
	.id = -1,
	.dev = {
		.platform_data = &spi_modem_data,
	},
};

enum {
	GPIO_AP_CP_MRDY = 0,
	GPIO_AP_CP_SRDY,
	GPIO_AP_CP_SUB_MRDY,
	GPIO_AP_CP_SUB_SRDY
};

struct gpio spi_gpios[] __initdata = {
	[GPIO_AP_CP_MRDY] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "AP_CP_MRDY",
	},
	[GPIO_AP_CP_SRDY] = {
		.flags	= GPIOF_IN,
		.label	= "AP_CP_SRDY",
	 },
	[GPIO_AP_CP_SUB_MRDY] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "AP_CP_SUB_MRDY",
	 },
	[GPIO_AP_CP_SUB_SRDY] = {
		.flags	= GPIOF_IN,
		.label	= "AP_CP_SUB_SRDY",
	 }
};

static void __init spi_modem_cfg_gpio(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(spi_gpios); i++)
		spi_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(spi_gpios[i].label);
	gpio_request_array(spi_gpios, ARRAY_SIZE(spi_gpios));

	spi_modem_data.gpio_ipc_mrdy = spi_gpios[GPIO_AP_CP_MRDY].gpio;
	spi_modem_data.gpio_ipc_srdy = spi_gpios[GPIO_AP_CP_SRDY].gpio;
	spi_modem_data.gpio_ipc_sub_mrdy = spi_gpios[GPIO_AP_CP_SUB_MRDY].gpio;
	spi_modem_data.gpio_ipc_sub_srdy = spi_gpios[GPIO_AP_CP_SUB_SRDY].gpio;

	pr_info("[SPI] %s done\n", __func__);

}

static int __init init_spi(void)
{
	pr_info("[SPI] %s\n", __func__);
	spi_modem_cfg_gpio();
	platform_device_register(&spi_modem);

	spi_register_board_info(superior_omap4_spi_board_info,
		ARRAY_SIZE(superior_omap4_spi_board_info));
	return 0;
}

module_init(init_spi);

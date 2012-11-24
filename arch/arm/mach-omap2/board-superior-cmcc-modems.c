/* linux/arch/arm/mach-xxxx/board-superior-cmcc-modems.c
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <mach/omap4-common.h>
#include <linux/platform_data/modem_v2.h>

#include "board-superior.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"

/* tdscdma target platform data */
static struct modem_io_t tdscdma_io_devices[] = {
	[0] = {
		.name = "td_ipc0",
		.id = 0x1,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[1] = {
		.name = "td_rfs0",
		.id = 0x41,
		.format = IPC_RFS,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[2] = {
		.name = "rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[3] = {
		.name = "td_boot0",
		.id = 0x0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[4] = {
		.name = "rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[5] = {
		.name = "rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[6] = {
		.name = "multipdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[7] = {
		.name = "td_ramdump0",
		.id = 0x0,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[8] = {
		.name = "td_boot1",
		.id = 0x0,
		.format = IPC_BOOT_2,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[9] = {
		.name = "umts_router", /* AT Iface & Dial-up */
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[10] = {
		.name = "umts_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[11] = {
		.name = "umts_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
};

enum {
	GPIO_CP_ON = 0,
	GPIO_PDA_ACTIVE,
	GPIO_PHONE_ACTIVE,
	GPIO_CP_DUMP_INT,
	GPIO_AP_CP_INT2,
	GPIO_UART_SEL,
};

struct gpio modem_gpios[] __initdata = {
	[GPIO_CP_ON] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "CP_ON",
	},
	[GPIO_PDA_ACTIVE] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "PDA_ACTIVE",
	},
	[GPIO_PHONE_ACTIVE] = {
		.flags  = GPIOF_IN,
		.label  = "PHONE_ACTIVE",
	},
	[GPIO_CP_DUMP_INT] = {
		.flags  = GPIOF_IN,
		.label  = "CP_DUMP_INT",
	},
	[GPIO_AP_CP_INT2] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "AP_CP_INT2",
	},
	[GPIO_UART_SEL] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "UART_SEL",
	},
};

static struct modem_data tdscdma_modem_data = {
	.name = "sprd8803",

	.modem_type = SPRD_SC8803,
	.link_types = LINKTYPE(LINKDEV_SPI),
	.modem_net = TDSCDMA_NETWORK,
	.use_handover = false,
	.ipc_version = SIPC_VER_40,

	.num_iodevs = ARRAY_SIZE(tdscdma_io_devices),
	.iodevs = tdscdma_io_devices,
};

static void __init tdscdma_modem_cfg_gpio(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(modem_gpios); i++)
		modem_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(modem_gpios[i].label);
	gpio_request_array(modem_gpios, ARRAY_SIZE(modem_gpios));

	tdscdma_modem_data.gpio_cp_on = modem_gpios[GPIO_CP_ON].gpio;
	tdscdma_modem_data.gpio_pda_active = modem_gpios[GPIO_PDA_ACTIVE].gpio;
	tdscdma_modem_data.gpio_phone_active =
		modem_gpios[GPIO_PHONE_ACTIVE].gpio;
	tdscdma_modem_data.gpio_cp_dump_int =
		modem_gpios[GPIO_CP_DUMP_INT].gpio;
	tdscdma_modem_data.gpio_ap_cp_int2 = modem_gpios[GPIO_AP_CP_INT2].gpio;
	tdscdma_modem_data.gpio_uart_sel = modem_gpios[GPIO_UART_SEL].gpio;

	pr_info("tdscdma_modem_cfg_gpio done\n");
}

/* if use more than one modem device, then set id num */
static struct platform_device tdscdma_modem = {
	.name = "mif_sipc4",
	.id = -1,
	.dev = {
		.platform_data = &tdscdma_modem_data,
	},
};

static int __init init_modem(void)
{
	tdscdma_modem_cfg_gpio();
	platform_device_register(&tdscdma_modem);

	mif_info("board init_tdscdma_modem done\n");
	return 0;
}
late_initcall(init_modem);

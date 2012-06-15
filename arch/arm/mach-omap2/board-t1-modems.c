/* linux/arch/arm/mach-xxxx/board-tuna-modems.c
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
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
#include <linux/platform_data/modem.h>
#include "mux.h"
#include "omap_muxtbl.h"

/* umts target platform data */
static struct modem_io_t umts_io_devices[] = {
	[0] = {
		.name = "umts_ipc0",
		.id = 0x1,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.link = LINKDEV_MIPI,
	},
	[1] = {
		.name = "umts_rfs0",
		.id = 0x41,
		.format = IPC_RFS,
		.io_type = IODEV_MISC,
		.link = LINKDEV_MIPI,
	},
	[2] = {
		.name = "rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.link = LINKDEV_MIPI,
	},
	[3] = {
		.name = "umts_boot0",
		.id = 0x0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.link = LINKDEV_MIPI,
	},
	[4] = {
		.name = "rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.link = LINKDEV_MIPI,
	},
	[5] = {
		.name = "rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.link = LINKDEV_MIPI,
	},
	[6] = {
		.name = "multipdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.link = LINKDEV_MIPI,
	},
	[7] = {
		.name = "umts_ramdump0",
		.id = 0x0,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.link = LINKDEV_MIPI,
	},
	[8] = {
		.name = "umts_boot1",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.link = LINKDEV_MIPI,
	},
	[9] = {
		.name = "umts_router", /* AT Iface & Dial-up */
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.link = LINKDEV_MIPI,
	},
	[10] = {
		.name = "umts_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.link = LINKDEV_MIPI,
	},
	[11] = {
		.name = "umts_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.link = LINKDEV_MIPI,
	},
};

enum {
	GPIO_CP_ON = 0,
	GPIO_CP_RST,
	GPIO_RESET_REQ,
	GPIO_PDA_ACTIVE,
	GPIO_PHONE_ACTIVE,
	GPIO_CP_DUMP_INT
};

struct gpio modem_gpios[] = {
	[GPIO_CP_ON] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "CP_ON",
	},
	[GPIO_CP_RST] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "CP_RST",
	 },
	[GPIO_RESET_REQ] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "RESET_REQ_N",
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
	 }
};

static struct modem_data umts_modem_data = {
	.name = "xmm6260",

	.modem_type = IMC_XMM6260,
	.link_type = LINKDEV_MIPI,
	.modem_net = UMTS_NETWORK,

	.num_iodevs = ARRAY_SIZE(umts_io_devices),
	.iodevs = umts_io_devices,
};

/* To get modem state, register phone active irq using resource */
static struct resource umts_modem_res[] = {
	[0] = {
		.name = "umts_phone_active",
		.flags = IORESOURCE_IRQ,
	},
};

static void umts_modem_cfg_gpio(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(modem_gpios); i++)
		modem_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(modem_gpios[i].label);
	gpio_request_array(modem_gpios, ARRAY_SIZE(modem_gpios));

	umts_modem_data.gpio_cp_on = modem_gpios[GPIO_CP_ON].gpio;
	umts_modem_data.gpio_reset_req_n = modem_gpios[GPIO_RESET_REQ].gpio;
	umts_modem_data.gpio_cp_reset = modem_gpios[GPIO_CP_RST].gpio;
	umts_modem_data.gpio_pda_active = modem_gpios[GPIO_PDA_ACTIVE].gpio;
	umts_modem_data.gpio_phone_active = modem_gpios[GPIO_PHONE_ACTIVE].gpio;
	umts_modem_data.gpio_cp_dump_int = modem_gpios[GPIO_CP_DUMP_INT].gpio;
	umts_modem_res[0].start =
		OMAP_GPIO_IRQ(modem_gpios[GPIO_PHONE_ACTIVE].gpio);
	umts_modem_res[0].end =
		OMAP_GPIO_IRQ(modem_gpios[GPIO_PHONE_ACTIVE].gpio);
	pr_debug("umts_modem_cfg_gpio done\n");
}


/* if use more than one modem device, then set id num */
static struct platform_device umts_modem = {
	.name = "modem_if",
	.id = -1,
	.num_resources = ARRAY_SIZE(umts_modem_res),
	.resource = umts_modem_res,
	.dev = {
		.platform_data = &umts_modem_data,
	},
};

static int __init init_modem(void)
{
	pr_debug("[MODEM_IF] init_modem\n");
	umts_modem_cfg_gpio();
	platform_device_register(&umts_modem);
	return 0;
}
late_initcall(init_modem);

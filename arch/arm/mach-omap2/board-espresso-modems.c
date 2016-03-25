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
#include <linux/platform_data/modem_v2.h>

#include "board-espresso.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"

/* umts target platform data */
static struct modem_io_t umts_io_devices[] = {
	[0] = {
		.name = "umts_ipc0",
		.id = 0x1,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[1] = {
		.name = "umts_rfs0",
		.id = 0x41,
		.format = IPC_RFS,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[2] = {
		.name = "rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[3] = {
		.name = "umts_boot0",
		.id = 0x0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[4] = {
		.name = "rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[5] = {
		.name = "rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[6] = {
		.name = "multipdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[7] = {
		.name = "umts_ramdump0",
		.id = 0x0,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[8] = {
		.name = "umts_boot1",
		.id = 0x0,
		.format = IPC_BOOT_2,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[9] = {
		.name = "umts_router", /* AT Iface & Dial-up */
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[10] = {
		.name = "umts_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
	[11] = {
		.name = "umts_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_MIPI),
	},
};

enum {
	GPIO_CP_ON = 0,
	GPIO_CP_RST,
	GPIO_RESET_REQ,
	GPIO_PDA_ACTIVE,
	GPIO_PHONE_ACTIVE,
	GPIO_CP_DUMP_INT,
	GPIO_SIM_DETECT,
};

struct gpio modem_gpios[] __initdata = {
	[GPIO_CP_ON] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "CP_ON",
	},
	[GPIO_CP_RST] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "CP_PMU_RST",
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
	 },
	[GPIO_SIM_DETECT] = {
		.flags  = GPIOF_IN,
		.label  = "SIM_DETECT",
	},
};

static struct omap_board_mux mux_none_modem[] __initdata = {
	/* [-N-C-] usbb1_ulpitll_stp - gpio_85 - MIPI_HSI_TX_DATA */
	OMAP4_MUX(USBB1_ULPITLL_STP,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_dir - gpio_86 - MIPI_HSI_TX_FLG */
	OMAP4_MUX(USBB1_ULPITLL_DIR,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_nxt - gpio_87 - MIPI_HSI_TX_RDY */
	OMAP4_MUX(USBB1_ULPITLL_NXT,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_dat0 - gpio_88 - MIPI_HSI_RX_WAKE */
	OMAP4_MUX(USBB1_ULPITLL_DAT0,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_dat1 - gpio_89 - MIPI_HSI_RX_DATA */
	OMAP4_MUX(USBB1_ULPITLL_DAT1,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_dat2 - gpio_90 - MIPI_HSI_RX_FLG */
	OMAP4_MUX(USBB1_ULPITLL_DAT2,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_dat3 - gpio_91 - MIPI_HSI_RX_RDY */
	OMAP4_MUX(USBB1_ULPITLL_DAT3,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

static struct modem_data umts_modem_data = {
	.name = "xmm6262",

	.modem_type = IMC_XMM6262,
	.link_types = LINKTYPE(LINKDEV_MIPI),
	.use_handover = false,
	.num_iodevs = ARRAY_SIZE(umts_io_devices),
	.iodevs = umts_io_devices,
};

static void __init umts_modem_cfg_gpio(void)
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
	if (!board_is_espresso10()) {
		umts_modem_data.gpio_sim_detect = modem_gpios[GPIO_SIM_DETECT].gpio;
	}

	pr_debug("umts_modem_cfg_gpio done\n");
}

static void __init none_modem_cfg_mux(void)
{
	int i;
	struct omap_mux_partition *partition;
	struct omap_mux_partition *core = omap_mux_get("core");
	struct omap_mux_partition *wkup = omap_mux_get("wkup");
	struct omap_muxtbl *tbl;

	omap_mux_write_array(core, mux_none_modem);

	for (i = 0; i < ARRAY_SIZE(modem_gpios); i++) {
		tbl = omap_muxtbl_find_by_name(modem_gpios[i].label);
		if (!tbl)
			continue;
		if (tbl->domain == OMAP4_MUXTBL_DOMAIN_WKUP)
			partition = wkup;
		else
			partition = core;

		omap_mux_write(partition,
			OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
			tbl->mux.reg_offset);
	}
}

/* if use more than one modem device, then set id num */
static struct platform_device umts_modem = {
	.name = "mif_sipc4",
	.id = -1,
	.dev = {
		.platform_data = &umts_modem_data,
	},
};

void __init omap4_espresso_none_modem_init(void)
{
	if (!board_has_modem())
		none_modem_cfg_mux();
}

static int __init init_modem(void)
{
	if (!board_has_modem())
		return 0;

	umts_modem_cfg_gpio();
	platform_device_register(&umts_modem);

	mif_info("board init_modem done\n");
	return 0;
}
late_initcall(init_modem);

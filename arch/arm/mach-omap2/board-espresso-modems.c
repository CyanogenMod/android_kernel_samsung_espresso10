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

#define GPIO_CP_ON		37
#define GPIO_CP_PMU_RST	39
#define GPIO_CP_PMU_RST_OLDREV	2
#define GPIO_RESET_REQ_N	50
#define GPIO_PDA_ACTIVE	119
#define GPIO_PHONE_ACTIVE	120
#define GPIO_CP_DUMP_INT	56
#define GPIO_SIM_DETECT	35

struct gpio modem_gpios[] __initdata = {
	{
		.flags  = GPIOF_OUT_INIT_LOW,
		.gpio   = GPIO_CP_ON,
		.label  = "CP_ON",
	},
	{
		.flags  = GPIOF_OUT_INIT_LOW,
		.gpio   = GPIO_CP_PMU_RST,
		.label  = "CP_PMU_RST",
	},
	{
		.flags  = GPIOF_OUT_INIT_LOW,
		.gpio   = GPIO_RESET_REQ_N,
		.label  = "RESET_REQ_N",
	},
	{
		.flags  = GPIOF_OUT_INIT_LOW,
		.gpio 	= GPIO_PDA_ACTIVE,
		.label  = "PDA_ACTIVE",
	},
	{
		.flags  = GPIOF_IN,
		.gpio   = GPIO_PHONE_ACTIVE,
		.label  = "PHONE_ACTIVE",
	},
	{
		.flags  = GPIOF_IN,
		.gpio   = GPIO_CP_DUMP_INT,
		.label  = "CP_DUMP_INT",
	},
	{
		.flags  = GPIOF_IN,
		.gpio   = GPIO_SIM_DETECT,
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
	/* [--OUT] gpmc_ncs0.gpio_50 - RESET_REQ_N */
	OMAP4_MUX(GPMC_NCS0,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [--OUT] abe_dmic_clk1.gpio_119 - PDA_ACTIVE */
	OMAP4_MUX(ABE_DMIC_CLK1,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [IN---] abe_dmic_din1.gpio_120 - PHONE_ACTIVE */
	OMAP4_MUX(ABE_DMIC_DIN1,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [IN---] gpmc_nadv_ale.gpio_56 - CP_DUMP_INT */
	OMAP4_MUX(GPMC_NADV_ALE,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [IN---] gpmc_ad11.gpio_35 - SIM_DETECT */
	OMAP4_MUX(GPMC_AD11,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [--OUT] gpmc_ad13.gpio_37 - CP_ON */
	OMAP4_MUX(GPMC_AD13,
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

	.gpio_cp_on = GPIO_CP_ON,
	.gpio_reset_req_n = GPIO_RESET_REQ_N,
	.gpio_cp_reset = GPIO_CP_PMU_RST,
	.gpio_pda_active = GPIO_PDA_ACTIVE,
	.gpio_phone_active = GPIO_PHONE_ACTIVE,
	.gpio_cp_dump_int = GPIO_CP_DUMP_INT,
};

static void __init umts_modem_cfg_gpio(void)
{
	if ((board_is_espresso10() && system_rev < 8) ||
		(!board_is_espresso10() && system_rev < 10)) {
		modem_gpios[1].gpio = GPIO_CP_PMU_RST_OLDREV;
		umts_modem_data.gpio_cp_reset = GPIO_CP_PMU_RST_OLDREV;
	}

	gpio_request_array(modem_gpios, ARRAY_SIZE(modem_gpios));

	if (!board_is_espresso10()) {
		umts_modem_data.gpio_sim_detect = GPIO_SIM_DETECT;
	}

	pr_debug("umts_modem_cfg_gpio done\n");
}

static void __init none_modem_cfg_mux(void)
{
	struct omap_mux_partition *core = omap_mux_get("core");
	struct omap_mux_partition *wkup = omap_mux_get("wkup");

	omap_mux_write_array(core, mux_none_modem);

	if ((board_is_espresso10() && system_rev < 8) ||
		(!board_is_espresso10() && system_rev < 10)) {
		omap_mux_write(wkup,
			OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
			OMAP4_CTRL_MODULE_PAD_SIM_RESET_OFFSET);
	} else {
		omap_mux_write(core,
			OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
			OMAP4_CTRL_MODULE_PAD_GPMC_AD15_OFFSET);
	}
}

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

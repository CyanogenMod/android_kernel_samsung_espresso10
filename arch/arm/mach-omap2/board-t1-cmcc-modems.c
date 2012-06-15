/* arch/arm/mach-omap2/board-t1-cmcc-modems.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <mach/omap4-common.h>

#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/phone_svn/modemctl.h>

#include "board-t1.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"

static int uart_sel;

static struct modemctl_platform_data mdmctl_data;

enum {
	GPIO_PHONE_ON = 0,
	GPIO_PHONE_ACTIVE,
	GPIO_PDA_ACTIVE,
	GPIO_CP_DUMP_INT,
	GPIO_AP_CP_INT2,
};

struct gpio modem_gpios[] __initdata = {
	[GPIO_PHONE_ON] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "CP_ON",
	},
	[GPIO_PHONE_ACTIVE] = {
		.flags	= GPIOF_IN,
		.label	= "PHONE_ACTIVE",
	 },
	[GPIO_PDA_ACTIVE] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "PDA_ACTIVE",
	 },
	[GPIO_CP_DUMP_INT] = {
		.flags	= GPIOF_IN,
		.label	= "CP_DUMP_INT",
	 },
	[GPIO_AP_CP_INT2] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "AP_CP_INT2",
	 },
};

static void __init sprd_modem_cfg_gpio(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(modem_gpios); i++)
		modem_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(modem_gpios[i].label);
	gpio_request_array(modem_gpios, ARRAY_SIZE(modem_gpios));

	mdmctl_data.gpio_phone_on = modem_gpios[GPIO_PHONE_ON].gpio;
	mdmctl_data.gpio_phone_active = modem_gpios[GPIO_PHONE_ACTIVE].gpio;
	mdmctl_data.gpio_pda_active = modem_gpios[GPIO_PDA_ACTIVE].gpio;
	mdmctl_data.gpio_cp_dump_int = modem_gpios[GPIO_CP_DUMP_INT].gpio;
	mdmctl_data.gpio_ap_cp_int2 = modem_gpios[GPIO_AP_CP_INT2].gpio;

	pr_debug("[MODEM_IF] %s done\n", __func__);
}

static void sprd_on(struct modemctl *mc)
{
	if (!mc->gpio_pda_active || !mc->gpio_phone_on)
		return;

	pr_info("[%s]\n", __func__);

	gpio_set_value(mc->gpio_pda_active, 0);
	gpio_set_value(mc->gpio_phone_on, 0);
	msleep(100);
	gpio_set_value(mc->gpio_phone_on, 1);
	gpio_set_value(mc->gpio_pda_active, 1);
}

static void sprd_off(struct modemctl *mc)
{
	pr_debug("[%s]\n", __func__);
	if (!mc->gpio_phone_on)
		return;

	gpio_set_value(mc->gpio_phone_on, 0);
}

static void sprd_boot(struct modemctl *mc)
{
	pr_debug("[%s]\n", __func__);
}

static void sprd_reset(struct modemctl *mc)
{
	pr_debug("[%s]\n", __func__);
}

/* move the PDA_ACTIVE Pin control to sleep_gpio_table */
static void sprd_suspend(struct modemctl *mc)
{
	gpio_set_value(mc->gpio_pda_active, 0);
}

static void sprd_resume(struct modemctl *mc)
{
	gpio_set_value(mc->gpio_pda_active, 1);
}

static struct modemctl_platform_data mdmctl_data = {
	.name = "sc8803",

	.ops = {
		.modem_on = sprd_on,
		.modem_off = sprd_off,
		.modem_boot = sprd_boot,
		.modem_reset = sprd_reset,
		.modem_suspend = sprd_suspend,
		.modem_resume = sprd_resume,
	}
};

struct platform_device modemctl = {
	.name = "modemctl",
	.id = -1,
	.dev = {
		.platform_data = &mdmctl_data,
	},
};

#if 0
static struct omap_board_mux mux_none_modem[] __initdata = {
	/* [-N-C-] mcspi1_clk - gpio_134 - AP_SPI_CLK */
	OMAP4_MUX(MCSPI1_CLK,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] mcspi1_somi - gpio_135 - AP_SPI_SOMI */
	OMAP4_MUX(MCSPI1_SOMI,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] mcspi1_simo - gpio_136 - AP_SPI_SIMO */
	OMAP4_MUX(MCSPI1_SIMO,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_clk - gpio_84 - AP_CP_MRDY */
	OMAP4_MUX(USBB1_ULPITLL_CLK,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] fref_clk4_req - gpio_wk7 - AP_CP_SRDY */
	OMAP4_MUX(FREF_CLK4_REQ,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_dat2 - gpio_90 - AP_CP_SUB_MRDY */
	OMAP4_MUX(USBB1_ULPITLL_DAT2,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	/* [-N-C-] usbb1_ulpitll_dat3 - gpio_91 - AP_CP_SUB_SRDY */
	OMAP4_MUX(USBB1_ULPITLL_DAT3,
		  OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

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

void __init omap4_espresso_none_modem_init(void)
{
	unsigned int board_type = omap4_espresso_get_board_type();

	if (board_type == SEC_MACHINE_ESPRESSO_WIFI ||
		board_type == SEC_MACHINE_ESPRESSO_USA_BBY)
		none_modem_cfg_mux();
}
#endif

static int __init init_modem(void)
{
	pr_debug("[MODEM_IF] %s\n", __func__);
	sprd_modem_cfg_gpio();
	return 0;
}

module_init(init_modem);

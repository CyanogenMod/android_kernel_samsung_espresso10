/* arch/arm/mach-omap2/board-espresso-muxset-r02.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
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

#include "board-espresso.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [-N-C-] gpmc_ad10 - gpio_34 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD10, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     34, "gpmc_ad10.nc"),
	/* [--OUT] gpmc_nbe0_cle - gpio_59 - IRDA_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NBE0_CLE, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     59, "IRDA_EN"),
	/* [IN---] sim_clk - gpio_wk1 - PS_VOUT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     SIM_CLK,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     1, "PS_VOUT"),
	/* [--OUT] mcspi4_cs0 - gpio_154 - USB_SEL1 */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     MCSPI4_CS0, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     154, "USB_SEL1"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO, 2, muxtbl);

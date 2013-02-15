/* arch/arm/mach-omap2/board-espresso-muxset-r07.c
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
	/* [--OUT] gpmc_ad10.gpio_34 - TF_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD10, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     34, "TF_EN"),
	/* [-N-C-] gpmc_a19.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A19, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     43, "gpmc_a19.nc"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO, 7, muxtbl);

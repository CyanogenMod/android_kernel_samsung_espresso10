/* arch/arm/mach-omap2/board-espresso-cmcc-muxset-r09.c
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

#include "board-espresso.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

/* CMCC board rev0.2 */
static struct omap_muxtbl muxtbl[] __initdata = {
	/* [-----] gpmc_a18 - gpio_42 - AP_DUMP_INT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A18, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		     42, "AP_DUMP_INT"),
	/* [--OUT] gpmc_a19 - gpio_43 - TF_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A19, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     43, "TF_EN"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO, 9, muxtbl);

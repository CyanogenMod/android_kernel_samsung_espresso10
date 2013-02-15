/* arch/arm/mach-omap2/board-espresso10-muxset-r07.c
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

#include "board-espresso10.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [IN---] usbb1_ulpitll_dat4.gpio_92 - TSP_VENDOR3 */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_DAT4, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     92, "TSP_VENDOR3"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO10, 7, muxtbl);

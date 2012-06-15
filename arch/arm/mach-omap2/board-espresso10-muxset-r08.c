/* arch/arm/mach-omap2/board-espresso10-muxset-r08.c
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
	/* [--OUT] gpmc_ad15 - gpio_39 - CP_PMU_RST */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD15,
		     OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT |
		     OMAP_PIN_OFF_OUTPUT_HIGH,
		     39, "CP_PMU_RST"),
	/* [IN---] sim_reset - gpio_wk2 - ACCESSORY_INT_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     SIM_RESET,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     2, "ACCESSORY_INT_1.8V"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO10, 8, muxtbl);

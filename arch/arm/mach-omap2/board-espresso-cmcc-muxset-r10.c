/* arch/arm/mach-omap2/board-espresso-cmcc-muxset-r10.c
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

/* CMCC board rev0.3 */
static struct omap_muxtbl muxtbl[] __initdata = {
	/* [-----] gpmc_ad15 - gpio_39 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD15, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     39, "ACCESSORY_INT_1.8V.nc"),
	/* [IN---] sys_pwron_reset_out - gpio_wk29 - ACCESSORY_INT_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     SYS_PWRON_RESET_OUT, OMAP_MUX_MODE3 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     29, "ACCESSORY_INT_1.8V"),
	/* [--OUT] gpmc_ad9 - gpio_33 - ALS_OUT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
			GPMC_AD9, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
			33, "ALS_OUT"),
	/* [-----] usbb1_ulpitll_stp - gpio_85 - MIPI_HSI_TX_DATA */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
			USBB1_ULPITLL_STP,
			OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
			85, "MIPI_HSI_TX_DATA"),
	/* [-----] usbb1_ulpitll_dir - gpio_86 - MIPI_HSI_TX_FLG */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
			USBB1_ULPITLL_DIR,
			OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
			86, "MIPI_HSI_TX_FLG"),
	/* [-----] usbb1_ulpitll_nxt - gpio_87 - MIPI_HSI_TX_RDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
			USBB1_ULPITLL_NXT,
			OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
			87, "MIPI_HSI_TX_RDY"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO, 10, muxtbl);

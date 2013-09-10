/* arch/arm/mach-omap2/board-superior-cmcc-muxset-r07.c
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

#include "board-superior.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [-N-C-] gpmc_ncs3.safe_mode - nc */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS3, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     53, "gpmc_ncs3.nc"),
	/* [--OUT] gpmc_ncs3.gpio_102 - CP_ON */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS5,
			 OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT | OMAP_PIN_OFF_NONE,
		     102, "CP_ON"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_SUPERIOR, 7, muxtbl);

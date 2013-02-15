/* arch/arm/mach-omap2/board-superior-muxset-r02.c
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
	/* [-N-C-] gpmc_wait2.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_WAIT2, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     100, "gpmc_wait2.nc"),
	/* [-N-C-] dpm_emu0.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU0, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     11, "dpm_emu0.nc"),
	/* [-N-C-] dpm_emu1.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU1, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     12, "dpm_emu1.nc"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_SUPERIOR, 2, muxtbl);

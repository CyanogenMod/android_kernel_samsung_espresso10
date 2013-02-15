/* arch/arm/mach-omap2/board-kona-muxset-r05.c
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

#include "board-kona.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [-N-C-] cam_globalreset.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     CAM_GLOBALRESET, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     83, "cam_globalreset.nc"),
	/* [IN---] dpm_emu10.gpio_21 - PEN_IRQ_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU10, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     21, "PEN_IRQ_1.8V"),
	/* [--OUT] dpm_emu11.gpio_22 - PEN_FWE1_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU11, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     22, "PEN_FWE1_1.8V"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_KONA, 5, muxtbl);

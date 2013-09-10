/* arch/arm/mach-omap2/board-superior-cmcc-muxset-r06.c
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
	/* [--OUT] dpm_emu11.gpio_22 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU11, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     22, "dpm_emu11.nc"),
	/* [IN---] dpm_emu14.gpio_25 - SYS_DRM_MSEC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU14, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     25, "SYS_DRM_MSEC"),
	/* [-N-C-] dpm_emu17.safe_mode -  VT_STBY*/
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU17, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     28, "VT_STBY"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_SUPERIOR, 6, muxtbl);

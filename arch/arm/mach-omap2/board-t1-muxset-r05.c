/* arch/arm/mach-omap2/board-t1-muxset-r05.c
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

#include "board-t1.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [-N-C-] kpd_row4.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW4, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     176, "kpd_row4.nc"),
	/* [IN---] fref_clk3_out.gpio_wk31 - HOME_KEY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     FREF_CLK3_OUT,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     31, "HOME_KEY"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_T1, 5, muxtbl);

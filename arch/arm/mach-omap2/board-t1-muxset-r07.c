/* arch/arm/mach-omap2/board-t1-muxset-r07.c
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
	/* [--OUT] usbb1_ulpitll_dat7 - gpio_95 - MOTOR_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_DAT7, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     95, "MOTOR_EN"),
	/* [IN---] sim_pwrctrl - gpio_wk4 - EAR_SEND_END */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     SIM_PWRCTRL,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     4, "EAR_SEND_END"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_T1, 7, muxtbl);

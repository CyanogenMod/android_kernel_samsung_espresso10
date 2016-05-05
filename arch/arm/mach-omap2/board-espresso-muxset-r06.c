/* arch/arm/mach-omap2/board-espresso-muxset-r06.c
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

#include "board-espresso.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [-N-C-] gpmc_ad9.gpio_33 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD9, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     33, "gpmc_ad9.nc"),
	/* [IN---] gpmc_ad14.gpio_38 - MOTOR_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD14, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		     38, "MOTOR_EN"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO, 6, muxtbl);

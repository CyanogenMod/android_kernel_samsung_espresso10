/* arch/arm/mach-omap2/board-t1-muxset-r04.c
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
	/* [--OUT] gpmc_ncs3 - gpio_53 - SENSOR_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS3, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     53, "SENSOR_EN"),
	/* [--OUT] gpmc_nbe0_cle - gpio_59 - MHL_SEL */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NBE0_CLE, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     59, "MHL_SEL"),
	/* [IN---] mcspi1_cs2 - gpio_139 - 3-TOUCHKEY_SCL */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     MCSPI1_CS2,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP |
		     OMAP_PIN_OFF_INPUT_PULLDOWN,
		     139, "3-TOUCHKEY_SCL"),
	/* [IN---] mcspi1_cs3 - gpio_140 - 3-TOUCHKEY_SDA */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     MCSPI1_CS3,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP |
		     OMAP_PIN_OFF_INPUT_PULLDOWN,
		     140, "3-TOUCHKEY_SDA"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_T1, 4, muxtbl);

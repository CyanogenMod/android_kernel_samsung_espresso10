/* arch/arm/mach-omap2/board-kona-muxset-r07.c
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
	/* [IN---] abe_mcbsp2_dr.gpio_111 - LCD_FREQ_SDA */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_DR, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     111, "LCD_FREQ_SDA"),
	/* [IN---] abe_mcbsp2_dx.gpio_112 - LCD_FREQ_SCL */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_DX, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     112, "LCD_FREQ_SCL"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_KONA, 7, muxtbl);

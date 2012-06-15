/* arch/arm/mach-omap2/board-espresso10-usa-bby-muxset-r07.c
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

#include "board-espresso10.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [--OUT] gpmc_a19 - gpio_43 - MHL_RST */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A19, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     43, "MHL_RST"),
	/* [--OUT] gpmc_ncs5 - gpio_102 - HDMI_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS5, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     102, "HDMI_EN"),
	/* [IN---] hdmi_hpd - gpio_63 - HDMI_HPD */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     HDMI_HPD, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     63, "HDMI_HPD"),
	/* [-----] hdmi_cec - gpio_64 - NC  */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     HDMI_CEC, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     64, "hdmi_cec.nc"),
	/* [-----] hdmi_ddc_scl - gpio_65 - DDC_SCL_3.3V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     HDMI_DDC_SCL,
		     OMAP_MUX_MODE0 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_INPUT_PULLDOWN,
		     65, "DDC_SCL_3.3V"),
	/* [-----] hdmi_ddc_sda - gpio_66 - DDC_SDA_3.3V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     HDMI_DDC_SDA,
		     OMAP_MUX_MODE0 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_INPUT_PULLDOWN,
		     66, "DDC_SDA_3.3V"),
	/* [IN---] usbc1_icusb_dp - gpio_98 - MHL_SDA_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBC1_ICUSB_DP, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     98, "MHL_SDA_1.8V"),
	/* [IN---] usbc1_icusb_dm - gpio_99 - MHL_SCL_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBC1_ICUSB_DM, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     99, "MHL_SCL_1.8V"),
	/* [IN---] abe_mcbsp2_clkx - gpio_110 - CHG_SCL_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_CLKX, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     110, "CHG_SCL_1.8V"),
	/* [IN---] abe_mcbsp2_fsx - gpio_113 - CHG_SDA_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_FSX, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     113, "CHG_SDA_1.8V"),
	/* [IN---] mcspi1_clk - gpio_134 - ADC_I2C_SCL_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     MCSPI1_CLK, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     134, "ADC_I2C_SCL_1.8V"),
	/* [IN---] mcspi1_cs0 - gpio_137 - ADC_I2C_SDA_1.8V */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     MCSPI1_CS0, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     137, "ADC_I2C_SDA_1.8V"),
	/* [IN---] kpd_row3 - gpio_175 - MHL_INT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW3, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     175, "MHL_INT"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO10_USA_BBY, 7, muxtbl);

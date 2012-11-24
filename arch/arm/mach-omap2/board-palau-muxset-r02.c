/* arch/arm/mach-omap2/board-palau-muxset-r02.c
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

#include "board-palau.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [--OUT] gpmc_a19.gpio_43 - CAM_PMIC_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A19, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     43, "CAM_PMIC_EN"),
	/* [--OUT] gpmc_a25.gpio_49 - VPS_SOUND_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A25, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     49, "VPS_SOUND_EN"),
	/* [-N-C-] abe_mcbsp1_clkx.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_CLKX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     114, "abe_mcbsp1_clkx.nc"),
	/* [-N-C-] abe_mcbsp1_dr.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_DR, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     115, "abe_mcbsp1_dr.nc"),
	/* [-N-C-] abe_mcbsp1_dx.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_DX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     116, "abe_mcbsp1_dx.nc"),
	/* [-N-C-] abe_mcbsp1_fsx.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_FSX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     117, "abe_mcbsp1_fsx.nc"),
	/* [-----] abe_dmic_clk1.uart4_cts - NFC_UART_CTS */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_DMIC_CLK1, OMAP_MUX_MODE5 | OMAP_PIN_INPUT_PULLUP,
		     119, "NFC_UART_CTS"),
	/* [-----] abe_dmic_din1.uart4_rts - NFC_UART_RTS */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_DMIC_DIN1, OMAP_MUX_MODE5 | OMAP_PIN_INPUT_PULLUP,
		     120, "NFC_UART_RTS"),
	/* [--OUT] abe_dmic_din2.gpio_121 - PDA_ACTIVE */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_DMIC_DIN2,
		     OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT | OMAP_PIN_OFF_OUTPUT_LOW,
		     121, "PDA_ACTIVE"),
	/* [IN---] abe_dmic_din3.gpio_122 - PHONE_ACTIVE */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_DMIC_DIN3, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     122, "PHONE_ACTIVE"),
	/* [-N-C-] mcspi1_simo.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     MCSPI1_SIMO, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     136, "mcspi1_simo.nc"),
	/* [-N-C-] uart4_rx.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     UART4_RX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     155, "uart4_rx.nc"),
	/* [-N-C-] uart4_tx.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     UART4_TX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     156, "uart4_tx.nc"),
	/* [-N-C-] kpd_row3.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW3, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     175, "kpd_row3.nc"),
	/* [-N-C-] kpd_row4.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW4, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     176, "kpd_row4.nc"),
	/* [IN---] fref_clk4_req.gpio_wk7 - NFC_IRQ */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     FREF_CLK4_REQ,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     7, "NFC_IRQ"),
	/* [--OUT] dpm_emu6.gpio_17 - SUBPM_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU6, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     17, "SUBPM_EN"),
	/* [-N-C-] dpm_emu15.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU15, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     26, "dpm_emu15.nc"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_PALAU, 2, muxtbl);

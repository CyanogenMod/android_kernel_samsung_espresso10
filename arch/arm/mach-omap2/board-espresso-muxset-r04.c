/* arch/arm/mach-omap2/board-espresso-muxset-r04.c
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
	/* [--OUT] gpmc_ad9 - gpio_33 - ALS_OUT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD9, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     33, "ALS_OUT"),
	/* [-N-C-] gpmc_ad14 - gpio_38 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD14, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     38, "gpmc_ad14.nc"),
	/* [--OUT] gpmc_a21 - gpio_45 - CODEC_LDO_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A21, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     45, "CODEC_LDO_EN"),
	/* [--OUT] gpmc_ncs4 - gpio_101 - 26M_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS4, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     101, "26M_EN"),
	/* [-N-C-] abe_mcbsp2_clkx - gpio_110 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_CLKX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     110, "abe_mcbsp2_clkx.nc"),
	/* [-N-C-] abe_mcbsp2_dr - gpio_111 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_DR, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     111, "abe_mcbsp2_dr.nc"),
	/* [-N-C-] abe_mcbsp2_dx - gpio_112 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_DX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     112, "abe_mcbsp2_dx.nc"),
	/* [-N-C-] abe_mcbsp2_fsx - gpio_113 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_FSX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     113, "abe_mcbsp2_fsx.nc"),
	/* [-N-C-] abe_mcbsp1_clkx - gpio_114 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_CLKX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     114, "abe_mcbsp1_clkx.nc"),
	/* [-N-C-] abe_mcbsp1_dr - gpio_115 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_DR, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     115, "abe_mcbsp1_dr.nc"),
	/* [-N-C-] abe_mcbsp1_dx - gpio_116 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_DX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     116, "abe_mcbsp1_dx.nc"),
	/* [-N-C-] abe_mcbsp1_fsx - gpio_117 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_FSX, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     117, "abe_mcbsp1_fsx.nc"),
	/* [-----] abe_pdm_ul_data -  - AP_I2S_DIN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_PDM_UL_DATA, OMAP_MUX_MODE1 | OMAP_PIN_INPUT,
		     OMAP_MUXTBL_NO_GPIO, "AP_I2S_DIN"),
	/* [-----] abe_pdm_dl_data -  - AP_I2S_DOUT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_PDM_DL_DATA, OMAP_MUX_MODE1 | OMAP_PIN_OUTPUT,
		     OMAP_MUXTBL_NO_GPIO, "AP_I2S_DOUT"),
	/* [-----] abe_pdm_frame -  - AP_I2S_CLK */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_PDM_FRAME, OMAP_MUX_MODE1 | OMAP_PIN_INPUT,
		     OMAP_MUXTBL_NO_GPIO, "AP_I2S_CLK"),
	/* [-----] abe_pdm_lb_clk -  - AP_I2S_SYNC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_PDM_LB_CLK, OMAP_MUX_MODE1 | OMAP_PIN_INPUT,
		     OMAP_MUXTBL_NO_GPIO, "AP_I2S_SYNC"),
	/* [-N-C-] abe_clks - gpio_118 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_CLKS, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     118, "abe_clks.nc"),
	/* [-N-C-] hdq_sio - gpio_127 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     HDQ_SIO, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     127, "hdq_sio.nc"),
	/* [-N-C-] kpd_col2 - gpio_1 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_COL2, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     1, "kpd_col2.nc"),
	/* [-N-C-] sys_nirq2 - gpio_183 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     SYS_NIRQ2, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     183, "sys_nirq2.nc"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO, 4, muxtbl);

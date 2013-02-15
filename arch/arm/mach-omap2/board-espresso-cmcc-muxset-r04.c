/* arch/arm/mach-omap2/board-espresso-muxset-r04.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
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
	/* [--OUT] gpmc_ad14 - gpio_38 - MOTOR_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_AD14, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		     38, "MOTOR_EN"),
	/* [--OUT] gpmc_a17 - gpio_41 - PS_VOUT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A17, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     41, "PS_VOUT"),
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
	/* [-----] abe_mcbsp2_dr - gpio_111 - REC_PCM_IN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		ABE_MCBSP2_DR, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		111, "LVDS_nSHDN"),
	/* [-----] abe_mcbsp2_dx - gpio_112 - REC_PCM_OUT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		ABE_MCBSP2_DX, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		112, "LCD_EN"),
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
	/* [-----] mcspi1_clk - gpio_134 - AP_SPI_CLK */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		MCSPI1_CLK, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP,
		OMAP_MUXTBL_NO_GPIO, "mcspi1_clk"),
	/* [-----] mcspi1_somi - gpio_135 - AP_SPI_SOMI */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		MCSPI1_SOMI, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN,
		OMAP_MUXTBL_NO_GPIO, "mcspi1_somi"),
	/* [-----] mcspi1_simo - gpio_136 - AP_SPI_SIMO */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		MCSPI1_SIMO, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP,
		OMAP_MUXTBL_NO_GPIO, "mcspi1_simo"),
	/* [-----] usbb1_ulpitll_clk - gpio_84 - AP_CP_MRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		USBB1_ULPITLL_CLK,
		OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		84, "AP_CP_MRDY"),
	/* [-----] fref_clk4_req - gpio_wk7 - AP_CP_SRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		FREF_CLK4_REQ, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN |
		OMAP_PIN_OFF_WAKEUPENABLE,
		7, "AP_CP_SRDY"),
	/* [-----] usbb1_ulpitll_dat2 - gpio_90 - AP_CP_SUB_MRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		 USBB1_ULPITLL_DAT2,
		 OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		 90, "AP_CP_SUB_MRDY"),
	/* [-----] usbb1_ulpitll_dat3 - gpio_91 - AP_CP_SUB_SRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		 USBB1_ULPITLL_DAT3, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		 91, "AP_CP_SUB_SRDY"),
	/* [IN---] abe_dmic_din1 - gpio_120 - PHONE_ACTIVE */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		 ABE_DMIC_DIN1, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		 120, "PHONE_ACTIVE"),
	/* [IN---] sim_clk - gpio_wk1 - CP_DUMP_INT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		 SIM_CLK,
		 OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN |
		 OMAP_PIN_OFF_WAKEUPENABLE,
		 1, "CP_DUMP_INT"),
	/* [-----] usbb1_ulpitll_dat0 - gpio_88 - AP_CP_INT1 */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		 USBB1_ULPITLL_DAT0,
		 OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		 88, "AP_CP_INT1"),
	/* [-----] usbb1_ulpitll_dat1 - gpio_89 - AP_CP_INT2 */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		 USBB1_ULPITLL_DAT1,
		 OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		 89, "AP_CP_INT2"),
	/* [-N-C-] gpmc_ad10 - gpio_34 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		GPMC_AD10, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		34, "CP_USB_EN"),
	/* [--OUT] gpmc_ad13 - gpio_37 - CP_ON */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		GPMC_AD13,
		OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT | OMAP_PIN_OFF_NONE,
		37, "CP_ON"),
	/* [IN---] gpmc_nadv_ale - gpio_56 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		GPMC_NADV_ALE, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		56, "CP_DUMP_INT.nc"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_ESPRESSO, 4, muxtbl);

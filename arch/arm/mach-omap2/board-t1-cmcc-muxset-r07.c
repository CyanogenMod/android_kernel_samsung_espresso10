/* arch/arm/mach-omap2/board-t1-cmcc-muxset-r07.c
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
	/* [-----] usbb1_ulpitll_dir - gpio_86 - AP_CP_SRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		USBB1_ULPITLL_DIR, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN |
		OMAP_PIN_OFF_WAKEUPENABLE,
		86, "AP_CP_SRDY"),
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
	/* [--OUT] gpmc_ad12 - gpio_36 - CP_ON */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		GPMC_AD12,
		OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT | OMAP_PIN_OFF_NONE,
		36, "CP_ON"),
	/* [-----] kpd_col2 - gpio_1 - USB_OTG_ID.nc */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		KPD_COL2,
		OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		1, "USB_OTG_ID.nc"),
	/* [IN---] uart3_rx_irrx - gpio_143 - AP_FLM_RXD */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		UART3_RX_IRRX, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		143, "AP_FLM_RXD"),
	/* [IN---] uart3_tx_irtx - gpio_144 - AP_FLM_TXD */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		UART3_TX_IRTX, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		144, "AP_FLM_TXD"),
	/* [IN---] kpd_col2 - gpio_1 - USB_OTG_ID */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		KPD_COL2,
		OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLUP |
		OMAP_PIN_OFF_WAKEUPENABLE,
		1, "USB_OTG_ID"),
	/* [--OUT] kpd_col3 - gpio_171 - USB_OTG_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		KPD_COL3, OMAP_MUX_MODE7 | OMAP_PIN_OUTPUT,
		171, "USB_OTG_EN"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_T1, 7, muxtbl);

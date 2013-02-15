/* arch/arm/mach-omap2/board-t1-muxset-r03.c
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
	/* [IN---] gpmc_nadv_ale.gpio_56 - CP_DUMP_INT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NADV_ALE, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     56, "CP_DUMP_INT"),
	/* [-----] usbb1_ulpitll_dat6.dmtimer10_pwm_evt - MOTOR_PWM */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_DAT6,
		     OMAP_MUX_MODE1 | OMAP_PIN_OUTPUT | OMAP_PIN_OFF_OUTPUT_LOW,
		     94, "MOTOR_PWM"),
	/* [IN---] usbb1_ulpitll_dat7.gpio_95 - EAR_SEND_END */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_DAT7,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     95, "EAR_SEND_END"),
	/* [-N-C-] sim_clk.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     SIM_CLK, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     1, "sim_clk.nc"),
	/* [IN---] uart3_rts_sd.gpio_142 - OLED_ID */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     UART3_RTS_SD, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     142, "OLED_ID"),
	/* [-N-C-] kpd_col1.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_COL1, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     0, "kpd_col1.nc"),
	/* [IN---] kpd_col2.gpio_1 - USB_OTG_ID */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_COL2,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     1, "USB_OTG_ID"),
	/* [-N-C-] kpd_row1.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW1, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     2, "kpd_row1.nc"),
	/* [-N-C-] kpd_row2.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW2, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     3, "kpd_row2.nc"),
	/* [IN---] fref_clk3_req.gpio_wk30 - VOL_UP */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     FREF_CLK3_REQ,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     30, "VOL_UP"),
	/* [IN---] fref_clk4_out.gpio_wk8 - VOL_DOWN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     FREF_CLK4_OUT,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     8, "VOL_DOWN"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_T1, 3, muxtbl);

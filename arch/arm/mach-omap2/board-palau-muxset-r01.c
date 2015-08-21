/* arch/arm/mach-omap2/board-palau-muxset-r01.c
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
	/* [-N-C-] gpmc_a19.gpio_43 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A19, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		     43, "gpmc_a19.nc"),
	/* [IN---] gpmc_a23.gpio_47 - LCD_ID */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A23, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     47, "LCD_ID"),
	/* [-N-C-] gpmc_a25.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A25, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     49, "gpmc_a25.nc"),
	/* [-N-C-] gpmc_nwp.gpio_54 - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NWP, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		     54, "gpmc_nwp.nc"),
	/* [IN---] gpmc_wait0.gpio_61 - FUEL_I2C_SCL */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_WAIT0, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     61, "FUEL_I2C_SCL"),
	/* [IN---] gpmc_wait1.gpio_62 - FUEL_I2C_SDA */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_WAIT1, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     62, "FUEL_I2C_SDA"),
	/* [--OUT] cam_shutter.gpio_81 - CAM_FLASH_SET */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     CAM_SHUTTER, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     81, "CAM_FLASH_SET"),
	/* [--OUT] cam_strobe.gpio_82 - CAM_FLASH_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     CAM_STROBE, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     82, "CAM_FLASH_EN"),
	/* [IN---] gpio_wk0.gpio_wk0 - EAR_DET_GND */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     GPIO_WK0, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN,
		     0, "EAR_DET_GND"),
	/* [IN---] gpio_98.gpio_98 - IMA_I2C_SDA */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPIO_98, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     98, "IMA_I2C_SDA"),
	/* [IN---] gpio_99.gpio_99 - IMA_I2C_SCL */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPIO_99, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP,
		     99, "IMA_I2C_SCL"),
	/* [-----] abe_mcbsp1_dx.abe_mcbsp1_dx - FM_I2S_DO */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP1_DX, OMAP_MUX_MODE0 | OMAP_PIN_INPUT,
		     116, "FM_I2S_DO"),
	/* [-N-C-] usbb2_ulpitll_stp.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB2_ULPITLL_STP,
		     OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     158, "usbb2_ulpitll_stp.nc"),
	/* [-N-C-] usbb2_ulpitll_nxt.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB2_ULPITLL_NXT,
		     OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     160, "usbb2_ulpitll_nxt.nc"),
	/* [-N-C-] usbb2_ulpitll_dat0.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB2_ULPITLL_DAT0,
		     OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     161, "usbb2_ulpitll_dat0.nc"),
	/* [-N-C-] usbb2_ulpitll_dat1.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB2_ULPITLL_DAT1,
		     OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     162, "usbb2_ulpitll_dat1.nc"),
	/* [-N-C-] kpd_col3.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_COL3, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     171, "kpd_col3.nc"),
	/* [-N-C-] kpd_row4.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW4, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     176, "kpd_row4.nc"),
	/* [IN---] fref_clk3_out.gpo_wk31 - PS_ALS_INT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     FREF_CLK3_OUT,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     31, "PS_ALS_INT"),
	/* [-N-C-] sys_nirq2.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     SYS_NIRQ2, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     183, "sys_nirq2.nc"),
	/* [-N-C-] dpm_emu5.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU5, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     16, "dpm_emu5.nc"),
	/* [-N-C-] dpm_emu7.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU7, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     18, "dpm_emu7.nc"),
	/* [-N-C-] dpm_emu8.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU8, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     19, "dpm_emu8.nc"),
	/* [-----] dpm_emu10.safe_mode - PS_ALS_INT(nc) */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU10, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     21, "PS_ALS_INT(nc)"),
	/* [-N-C-] dpm_emu11.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU11, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     22, "dpm_emu11.nc"),
	/* [-N-C-] dpm_emu14.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU14, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     25, "dpm_emu14.nc"),
	/* [-N-C-] dpm_emu17.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU17, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     28, "dpm_emu17.nc"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_PALAU, 1, muxtbl);

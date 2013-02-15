/* arch/arm/mach-omap2/board-superior-cmcc-muxset-r01.c
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

#include "board-superior.h"

#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "sec_muxtbl.h"

static struct omap_muxtbl muxtbl[] __initdata = {
	/* [-N-C-] gpmc_a19.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A19, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     43, "UART_SEL"),
	/* [IN---] gpmc_a21.gpio_45 - NFC_IRQ */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_A21,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     45, "NFC_IRQ"),
	/* [--OUT] gpmc_ncs1.gpio_51 - USB_PWR_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS1, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     51, "USB_PWR_EN"),
	/* [--OUT] gpmc_wait2.gpio_100 - 2TOUCH_EN */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_WAIT2, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     100, "2TOUCH_EN"),
	/* [-N-C-] gpmc_ncs4.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS4, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     101, "gpmc_ncs4.nc"),
	/* [-N-C-] gpmc_ncs5.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS5, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     102, "gpmc_ncs5.nc"),
	/* [--OUT] cam_globalreset.gpio_83 - EAR_BIAS_DISCHARGE */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     CAM_GLOBALRESET, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     83, "EAR_BIAS_DISCHARGE"),
	/* [-N-C-] gpio_98.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPIO_98, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     98, "gpio_98.nc"),
	/* [-N-C-] gpio_99.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPIO_99, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     99, "gpio_99.nc"),
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
	/* [-N-C-] usbb2_ulpitll_stp.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB2_ULPITLL_STP,
		     OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     158, "usbb2_ulpitll_stp.nc"),
	/* [-N-C-] usbb2_ulpitll_dat1.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB2_ULPITLL_DAT1,
		     OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     162, "usbb2_ulpitll_dat1.nc"),
	/* [-N-C-] kpd_row3.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW3, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     175, "kpd_row3.nc"),
	/* [-N-C-] kpd_row4.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     KPD_ROW4, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     176, "kpd_row4.nc"),
	/* [IN---] fref_clk4_req.gpio_wk7 - GYRO_INT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     FREF_CLK4_REQ,
		     OMAP_MUX_MODE3 | OMAP_PIN_INPUT |
		     OMAP_PIN_OFF_WAKEUPENABLE,
		     7, "GYRO_INT"),
	/* [-----] sys_boot7.safe_mode - SYS_BOOT7 */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		     SYS_BOOT7, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     10, "SYS_BOOT7"),
	/* [-N-C-] dpm_emu2.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU2, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     13, "dpm_emu2.nc"),
	/* [-N-C-] dpm_emu6.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU6, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     17, "dpm_emu6.nc"),
	/* [-N-C-] dpm_emu9.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU9, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     20, "dpm_emu9.nc"),
	/* [-N-C-] dpm_emu10.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU10, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     21, "dpm_emu10.nc"),
	/* [--OUT] dpm_emu11.gpio_22 - SYS_DRM_MSEC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU11, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     22, "SYS_DRM_MSEC"),
	/* [--OUT] dpm_emu13.gpio_24 - VBUS_DETECT */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU13, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     24, "VBUS_DETECT"),
	/* [IN---] dpm_emu14.gpio_25 - VT_STBY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU14, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     25, "VT_STBY"),
	/* [IN---] dpm_emu15.gpio_26 - OLED_DET */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU15, OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		     26, "OLED_DET"),
	/* [-N-C-] dpm_emu17.safe_mode - NC */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     DPM_EMU17, OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		     28, "dpm_emu17.nc"),
	/* [--OUT] gpmc_ncs3.gpio_53 - CP_ON */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     GPMC_NCS3,
		     OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT | OMAP_PIN_OFF_NONE,
		     53, "CP_ON"),
	/* [IN---] fref_clk0_out.gpio_wk6 - AP_CP_SRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_WKUP,
		    FREF_CLK0_OUT, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLDOWN |
		    OMAP_PIN_OFF_WAKEUPENABLE,
		    6, "AP_CP_SRDY"),
	/* [--OUT] usbb1_ulpitll_clk.hsi1_cawake - AP_CP_MRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_CLK,
		     OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     84, "AP_CP_MRDY"),
	/* [--OUT] usbb1_ulpitll_dat0.hsi1_acwake - AP_CP_INT1 */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_DAT0, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     88, "AP_CP_INT1"),
	/* [--OUT] usbb1_ulpitll_dat1.hsi1_acdata - AP_CP_INT2 */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_DAT1, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     89, "AP_CP_INT2"),
	/* [--OUT] usbb1_ulpitll_dat2.hsi1_acflag - AP_CP_SUB_MRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_DAT2, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT,
		     90, "AP_CP_SUB_MRDY"),
	/* [IN---] usbb1_ulpitll_dat3.hsi1_caready - AP_CP_SUB_SRDY */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     USBB1_ULPITLL_DAT3, OMAP_MUX_MODE3 |
		     OMAP_PIN_INPUT_PULLDOWN, 91, "AP_CP_SUB_SRDY"),
	/* [-----] abe_mcbsp2_clkx.safe_mode - mcspi2_clk */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_CLKX, OMAP_MUX_MODE1 | OMAP_PIN_INPUT_PULLUP,
		     OMAP_MUXTBL_NO_GPIO, "mcspi2_clk"),
	/* [-----] abe_mcbsp2_dr.safe_mode - mcspi2_somi */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_DR, OMAP_MUX_MODE1 | OMAP_PIN_INPUT_PULLDOWN,
		     OMAP_MUXTBL_NO_GPIO, "mcspi2_somi"),
	/* [-----] abe_mcbsp2_dx.safe_mode - mcspi2_simo */
	OMAP4_MUXTBL(OMAP4_MUXTBL_DOMAIN_CORE,
		     ABE_MCBSP2_DX, OMAP_MUX_MODE1 | OMAP_PIN_INPUT_PULLUP,
		     OMAP_MUXTBL_NO_GPIO, "mcspi2_simo"),
};

add_sec_muxtbl_to_list(SEC_MACHINE_SUPERIOR, 1, muxtbl);

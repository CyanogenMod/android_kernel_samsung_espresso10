/*
  * This file configures the internal USB PHY in OMAP4430.
  *
  * Copyright (C) 2012 Samsung Electronics Co, Ltd.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * Author: Aquie KANG <aquie.kang@samsung.com>
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  *
  */

#include <linux/io.h>
#include <linux/err.h>
#include <linux/usb.h>

#include <plat/usb.h>
#include "control.h"

#define OCP2SCP_TIMING_OFFSET 0xAB018
#define USB2PHYCM_TRIM_OFFSET 0xAB0B8
#define TREMINATIOM_CONTROL 0xAB080
#define USBPHY_ANA_CONFIG2 0xAB0D4
#define USBPHY_RX_CALIB 0xAB084

#define swcap_trim_check_offset			(1<<0)
#define bg_trim_check_offset				(1<<1)
#define rterm_rmx_check_offset			(1<<2)
#define rterm_cal_check_offset			(1<<3)
#define sq_off_code_dac3_check_offset	(1<<4)

struct omap_phy_tune {
	u32		swcap_trim_cal;
	u32		bg_trim_cal;
	u32		rterm_rmx_cal;
	u32		rterm_cal;
	u32		sq_off_code_dac3;
	u32		cal_check;
};

static void __iomem *ctrl_base;
static struct otg_transceiver *transceiver;
static struct omap_phy_tune phy_tune;

/* commenting unused functions */
#if 0
static void omap4430_phy_init_for_eyediagram_ref_gen_test(u32 ref_gen_test)
{
	u32 read_val = 0;
	u32 out_val = 0;

	transceiver = otg_get_transceiver();

	/* If clock is disabled, enable clock */
	if (!otg_is_active(transceiver))
		otg_set_suspend(transceiver, 0);

	ctrl_base = ioremap(OMAP443X_SCM_BASE, SZ_1M);

	/* USBPHY_ANA_CONFIG2 setting */
	read_val = __raw_readl(ctrl_base + USBPHY_ANA_CONFIG2);
	out_val = read_val;
	out_val &= ~(0x7<<24);
	out_val |= (ref_gen_test<<24);
	__raw_writel(out_val, ctrl_base + USBPHY_ANA_CONFIG2);
	pr_info("%s, USBPHY_ANA_CONFIG2 = 0x%x , 0x%x\n",
		__func__,
		read_val,
		__raw_readl(ctrl_base + USBPHY_ANA_CONFIG2));


#ifndef CONFIG_USB_SWITCH_FSA9480
	iounmap(ctrl_base);
#endif	/* CONFIG_USB_SWITCH_FSA9480 */
}
#endif

static void omap4460_phy_tuning_for_eyediagram(
	int rterm_cal_offset, int sq_off_code_dac3_offset, u32 hs_code_sel)
{
	u32 read_val = 0;
	u32 rterm_cal = 0;
	u32 sq_off_code_dac3 = 0;

	pr_info("%s, rterm_cal=%d sq_off_code_dac3=%d hs_code_sel=%d\n",
			__func__, rterm_cal_offset,
				sq_off_code_dac3_offset, hs_code_sel);

	transceiver = otg_get_transceiver();

	/* If clock is disabled, enable clock */
	if (!otg_is_active(transceiver))
		otg_set_suspend(transceiver, 0);

	ctrl_base = ioremap(OMAP443X_SCM_BASE, SZ_1M);

	if (__raw_readl(ctrl_base + OCP2SCP_TIMING_OFFSET) != 0x0000000F)
		__raw_writel(0x00000000F, ctrl_base + OCP2SCP_TIMING_OFFSET);

	read_val = __raw_readl(ctrl_base + TREMINATIOM_CONTROL);
	pr_info("%s, prev TREMINATIOM_CONTROL=0x%x\n", __func__, read_val);

	if (!(phy_tune.cal_check & rterm_cal_check_offset)) {
		phy_tune.rterm_rmx_cal = (read_val & 0x7F);
		phy_tune.cal_check |= rterm_cal_check_offset;
	}

	if (rterm_cal_offset) {

		if ((int)(phy_tune.rterm_rmx_cal + rterm_cal_offset) < 0)
			rterm_cal = 0;
		else if ((int)(phy_tune.rterm_rmx_cal
				+ rterm_cal_offset) > 0x7F)
			rterm_cal = 0x7F;
		else
			rterm_cal = phy_tune.rterm_rmx_cal + rterm_cal_offset;

		read_val &= ~(0x7F);
		read_val |= rterm_cal;
		read_val |= (0x1<<7); /* USE_RTERM_CAL_REG = 1 */
		read_val |= (0x1<<29); /* ALWAYS_UPDATE = 1 */
		read_val |= (0x1<<9); /* RESTART_RTERM_CAL = 1 */
	} else {
		read_val &= ~(0x1<<7); /* USE_RTERM_RMX_REG = 0 */
		read_val &= ~(0x1<<29); /* ALWAYS_UPDATE = 0 */
		read_val &= ~(0x1<<9); /* RESTART_RTERM_CAL = 0 */
	}

	read_val &= ~(0x7<<11);

	if (hs_code_sel > 0x7)
		hs_code_sel = 0x7;
	else if (hs_code_sel < 0)
		hs_code_sel = 0;

	read_val |= (hs_code_sel<<11);

	__raw_writel(read_val, ctrl_base + TREMINATIOM_CONTROL);

	read_val = __raw_readl(ctrl_base + USBPHY_RX_CALIB);
	pr_info("%s, prev USBPHY_RX_CALIB=0x%x\n", __func__, read_val);

	if (!(phy_tune.cal_check & sq_off_code_dac3_check_offset)) {
		phy_tune.sq_off_code_dac3 = (read_val & (0x1F<<3))>>3;
		phy_tune.cal_check |= sq_off_code_dac3_check_offset;
	}

	if (sq_off_code_dac3_offset) {

		if ((int)(phy_tune.sq_off_code_dac3
				+ sq_off_code_dac3_offset) < 0)
			sq_off_code_dac3 = 0;
		else if ((int)(phy_tune.sq_off_code_dac3
				+ sq_off_code_dac3_offset) > 0x1F)
			sq_off_code_dac3 = 0x1F;
		else
			sq_off_code_dac3 =
				phy_tune.sq_off_code_dac3
					+ sq_off_code_dac3_offset;

		read_val &= ~(0x1F<<3);
		read_val |= (sq_off_code_dac3<<3);
		read_val |= (0x1<<8); /* USE_SQ_OFF_DAC3 = 1 */
	} else
		read_val &= ~(0x1<<8); /* USE_SQ_OFF_DAC3 = 0 */

	__raw_writel(read_val, ctrl_base + USBPHY_RX_CALIB);

	read_val = __raw_readl(ctrl_base + TREMINATIOM_CONTROL);
	pr_info("%s, TREMINATIOM_CONTROL=0x%x\n", __func__, read_val);
	read_val = __raw_readl(ctrl_base + USBPHY_RX_CALIB);
	pr_info("%s, USBPHY_RX_CALIB=0x%x\n", __func__, read_val);

	iounmap(ctrl_base);
}

static void omap4430_phy_init_for_eyediagram(
	int swcap_trim_offset, int bg_trim_offset, int rterm_rmx_offset)
{
	u32 read_val = 0;
	u32 swcap_trim = 0;
	u32 bg_trim = 0;
	u32 rterm_rmx = 0;

	transceiver = otg_get_transceiver();

	/* If clock is disabled, enable clock */
	if (!otg_is_active(transceiver))
		otg_set_suspend(transceiver, 0);

	pr_info("%s, swcap_trim=%d bg_trim=%d rterm_rmx=%d\n",
		__func__, swcap_trim_offset, bg_trim_offset, rterm_rmx_offset);

	ctrl_base = ioremap(OMAP443X_SCM_BASE, SZ_1M);

	if (__raw_readl(ctrl_base + OCP2SCP_TIMING_OFFSET) != 0x0000000F)
		__raw_writel(0x00000000F, ctrl_base + OCP2SCP_TIMING_OFFSET);

	read_val = __raw_readl(ctrl_base + USB2PHYCM_TRIM_OFFSET);
	pr_info("%s, prev USB2PHYCM_TRIM=0x%x\n", __func__, read_val);

	swcap_trim = (read_val & 0x00007F00) >> 8;

	if (!(phy_tune.cal_check & swcap_trim_check_offset)) {
		phy_tune.swcap_trim_cal = swcap_trim;
		phy_tune.cal_check |= swcap_trim_check_offset;
	}

	if (swcap_trim_offset) {
		if ((int)(phy_tune.swcap_trim_cal + swcap_trim_offset) < 0)
			swcap_trim = 0;
		else if ((int)(phy_tune.swcap_trim_cal
			+ swcap_trim_offset) > 0x7f)
			swcap_trim = 0x7f;
		else
			swcap_trim = phy_tune.swcap_trim_cal +
			swcap_trim_offset;

		read_val &= ~0x00007F00;
		read_val |= swcap_trim << 8;
		read_val |= 0x00008000; /* USE_SW_TRIM = 1 */
	} else
		read_val &= ~0x00008000; /* USE_SW_TRIM = 0 */



	if (!(phy_tune.cal_check & bg_trim_check_offset)) {
		phy_tune.bg_trim_cal = (read_val & (0x7FFF<<16))>>16;
		phy_tune.cal_check |= bg_trim_check_offset;
	}

	if (bg_trim_offset) {

		if ((int)(phy_tune.bg_trim_cal + bg_trim_offset) < 0)
			bg_trim = 0;
		else if ((int)(phy_tune.bg_trim_cal + bg_trim_offset) > 0x7FFF)
			bg_trim = 0x7FFF;
		else
			bg_trim = phy_tune.bg_trim_cal + bg_trim_offset;

		read_val &= ~(0xFFFF<<16);
		read_val |= bg_trim<<16;
		read_val |= (1<<31);  /* USE_BG_TRIM = 1 */
	} else
		read_val &= ~(1<<31);  /* USE_BG_TRIM = 0 */

	__raw_writel(read_val, ctrl_base + USB2PHYCM_TRIM_OFFSET);

	read_val = __raw_readl(ctrl_base + TREMINATIOM_CONTROL);
	pr_info("%s, prev TREMINATIOM_CONTROL=0x%x\n", __func__, read_val);

	if (!(phy_tune.cal_check & rterm_rmx_check_offset)) {
		phy_tune.rterm_rmx_cal = (read_val & (0x7F<<14))>>14;
		phy_tune.cal_check |= rterm_rmx_check_offset;
	}

	if (rterm_rmx_offset) {

		if ((int)(phy_tune.rterm_rmx_cal + rterm_rmx_offset) < 0)
			rterm_rmx = 0;
		else if ((int)(phy_tune.rterm_rmx_cal
				+ rterm_rmx_offset) > 0x7F)
			rterm_rmx = 0x7F;
		else
			rterm_rmx = phy_tune.rterm_rmx_cal + rterm_rmx_offset;

		read_val &= ~(0x7F<<14);
		read_val |= rterm_rmx<<14;
		read_val |= (0x1<<21); /* USE_RTERM_RMX_REG = 1 */
	} else
		read_val &= ~(0x1<<21); /* USE_RTERM_RMX_REG = 0 */

	__raw_writel(read_val, ctrl_base + TREMINATIOM_CONTROL);

	read_val = __raw_readl(ctrl_base + USB2PHYCM_TRIM_OFFSET);
	pr_info("%s, USB2PHYCM_TRIM=0x%x\n", __func__, read_val);
	read_val = __raw_readl(ctrl_base + TREMINATIOM_CONTROL);
	pr_info("%s, TREMINATIOM_CONTROL=0x%x\n", __func__, read_val);

#ifndef CONFIG_USB_SWITCH_FSA9480
	iounmap(ctrl_base);
#endif	/* CONFIG_USB_SWITCH_FSA9480 */
}

/* commenting unused function */
#if 0
static void omap4430_phy_remove_for_eyediagram(void)
{
	iounmap(ctrl_base);
}
#endif

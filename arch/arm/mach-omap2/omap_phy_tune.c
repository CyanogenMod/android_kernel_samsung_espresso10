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

static void __iomem *ctrl_base;
static struct otg_transceiver *transceiver;

#define OCP2SCP_TIMING_OFFSET 0xAB018
#define USB2PHYCM_TRIM_OFFSET 0xAB0B8

static void omap4430_phy_init_for_eyediagram(u32 swcap_trim_offset)
{
	u32 read_val = 0;
	u32 swcap_trim = 0;
	transceiver = otg_get_transceiver();

	/* If clock is disabled, enable clock */
	if (!otg_is_active(transceiver))
		otg_set_suspend(transceiver, 0);

	ctrl_base = ioremap(OMAP443X_SCM_BASE, SZ_1M);

	if (__raw_readl(ctrl_base + OCP2SCP_TIMING_OFFSET) != 0x0000000F)
		__raw_writel(0x00000000F, ctrl_base + OCP2SCP_TIMING_OFFSET);

	read_val = __raw_readl(ctrl_base + USB2PHYCM_TRIM_OFFSET);
	swcap_trim = (read_val & 0x00007F00) >> 8;

	/* 0x4E(default) + 0x22(SWCAP_TRIM_OFFSET) = 0xF0*/
	if (swcap_trim != (0x4E + swcap_trim_offset)) {
		swcap_trim = 0x4E + swcap_trim_offset;

		read_val &= ~0x00007F00;
		read_val |= swcap_trim << 8;
		read_val |= 0x00008000; /* USE_SW_TRIM = 1 */

		__raw_writel(read_val, ctrl_base + USB2PHYCM_TRIM_OFFSET);
	}
	pr_info("%s, usb swcap_trim_offset = 0x%x, USB2PHYCM_TRIM = 0x%x\n",
		__func__,
		swcap_trim_offset,
		__raw_readl(ctrl_base + USB2PHYCM_TRIM_OFFSET));
#ifndef CONFIG_USB_SWITCH_FSA9480
	iounmap(ctrl_base);
#endif	/* CONFIG_USB_SWITCH_FSA9480 */
}

static void omap4430_phy_remove_for_eyediagram(void)
{
	iounmap(ctrl_base);
}

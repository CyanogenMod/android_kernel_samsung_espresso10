/* arch/arm/mach-omap2/omap44xx_muxtbl.c
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

#include <plat/io.h>

#include <mach/ctrl_module_pad_core_44xx.h>
#include <mach/ctrl_module_pad_wkup_44xx.h>

#include "control.h"
#include "mux.h"
#include "mux44xx.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"

#if defined CONFIG_ARCH_OMAP4

static struct omap_mux_partition *core_part;
static struct omap_mux_partition *wkup_part;

static int __init omap4_muxtbl_is_pbias_gpio(struct omap_board_mux *mux)
{
	struct omap_board_mux wkup_mux[] = {
		OMAP4_MUX(SIM_IO, OMAP_MUX_MODE3),
		OMAP4_MUX(SIM_CLK, OMAP_MUX_MODE3),
		OMAP4_MUX(SIM_RESET, OMAP_MUX_MODE3),
	};
	unsigned int i = ARRAY_SIZE(wkup_mux) - 1;

	do {
		if (wkup_mux[i].reg_offset == mux->reg_offset &&
		    (wkup_mux[i].value & mux->value))
			return 0;
	} while (i-- != 0);

	return -1;
}

static void __init omap4_muxtbl_set_pbias_gpio_pre(
		struct omap_mux_partition *partition,
		struct omap_board_mux *mux)
{
	unsigned int reg_val;
	const unsigned int pad_core_base = OMAP4_CTRL_MODULE_PAD_CORE;
	const unsigned int pad_wkup_base = OMAP4_CTRL_MODULE_PAD_WKUP;

	if (partition != wkup_part || omap4_muxtbl_is_pbias_gpio(mux))
		return;

	reg_val = omap_readl(pad_core_base +
			     OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);
	reg_val &= ~(OMAP4_USIM_PBIASLITE_PWRDNZ_MASK |
		     OMAP4_USBC1_ICUSB_PWRDNZ_MASK);
	omap_writel(reg_val, pad_core_base +
		    OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);

	reg_val = omap_readl(pad_wkup_base +
			     OMAP4_CTRL_MODULE_PAD_WKUP_CONTROL_USIMIO);
	reg_val &= ~(OMAP4_USIM_PBIASLITE_PWRDNZ_MASK);
	omap_writel(reg_val, pad_wkup_base +
		    OMAP4_CTRL_MODULE_PAD_WKUP_CONTROL_USIMIO);

	reg_val = omap_readl(pad_core_base +
			     OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);
	reg_val &= ~(OMAP4_USIM_PBIASLITE_HIZ_MODE_MASK);
	omap_writel(reg_val, pad_core_base +
		    OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);
}

static void __init omap4_muxtbl_set_pbias_gpio_post(
		struct omap_mux_partition *partition,
		struct omap_board_mux *mux)
{
	unsigned int reg_val;
	const unsigned int pad_core_base = OMAP4_CTRL_MODULE_PAD_CORE;
	const unsigned int pad_wkup_base = OMAP4_CTRL_MODULE_PAD_WKUP;

	if (partition != wkup_part || omap4_muxtbl_is_pbias_gpio(mux))
		return;

	reg_val = omap_readl(pad_core_base +
			     OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_MMC1);
	reg_val &= ~(OMAP4_SDMMC1_DR0_SPEEDCTRL_MASK |
		     OMAP4_USBC1_DR0_SPEEDCTRL_MASK);
	omap_writel(reg_val, pad_core_base +
		    OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_MMC1);

	reg_val = omap_readl(pad_wkup_base +
			     OMAP4_CTRL_MODULE_PAD_WKUP_CONTROL_USIMIO);
	reg_val &= ~(OMAP4_PAD_USIM_CLK_LOW_MASK |
		     OMAP4_PAD_USIM_RST_LOW_MASK);
	omap_writel(reg_val, pad_wkup_base +
		    OMAP4_CTRL_MODULE_PAD_WKUP_CONTROL_USIMIO);

	reg_val = omap_readl(pad_core_base +
			     OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);
	reg_val &= ~(OMAP4_USIM_PBIASLITE_VMODE_MASK |
		     OMAP4_MMC1_PBIASLITE_VMODE_ERROR_MASK);
	omap_writel(reg_val, pad_core_base +
		    OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);

	reg_val = omap_readl(pad_core_base +
			     OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);
	reg_val |= (OMAP4_USIM_PBIASLITE_PWRDNZ_MASK |
		    OMAP4_USBC1_ICUSB_PWRDNZ_MASK);
	omap_writel(reg_val, pad_core_base +
		    OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);

	reg_val = omap_readl(pad_wkup_base +
			     OMAP4_CTRL_MODULE_PAD_WKUP_CONTROL_USIMIO);
	reg_val |= OMAP4_USIM_PWRDNZ_MASK;
	omap_writel(reg_val,
		    pad_wkup_base + OMAP4_CTRL_MODULE_PAD_WKUP_CONTROL_USIMIO);

	reg_val = omap_readl(pad_core_base +
			     OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);
	reg_val |= (OMAP4_USIM_PBIASLITE_PWRDNZ_MASK |
		    OMAP4_USBC1_ICUSB_PWRDNZ_MASK);
	omap_writel(reg_val, pad_core_base +
		    OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE);
}

static void __init omap_muxtbl_set_usbbx_gpio(struct omap_muxtbl *muxtbl)
{
	unsigned int reg_base;
	unsigned int reg_val;
	unsigned int shift;
	unsigned int cfg_val;
	unsigned int mux_val;

	/* not a gpio mode */
	if (likely((muxtbl->mux.value & OMAP_MUX_MODE7) != OMAP_MUX_MODE3))
		return;

	switch (muxtbl->gpio.gpio) {
	case 96:	/* usbb1_hsic_data */
		shift = OMAP4_USBB1_HSIC_DATA_WD_SHIFT;
		break;
	case 97:	/* usbb1_hsic_strobe */
		shift = OMAP4_USBB1_HSIC_STROBE_WD_SHIFT;
		break;
	case 169:	/* usbb2_hsic_data */
		shift = OMAP4_USBB2_HSIC_DATA_WD_SHIFT;
		break;
	case 170:	/* usbb2_hsic_strobe */
		shift = OMAP4_USBB2_HSIC_STROBE_WD_SHIFT;
		break;
	default:
		return;
	}

	mux_val = muxtbl->mux.value;
	if ((mux_val & OMAP_PIN_INPUT_PULLUP) == OMAP_PIN_INPUT_PULLUP)
		cfg_val = 0x1;
	else if ((mux_val & OMAP_PIN_INPUT_PULLDOWN) == OMAP_PIN_INPUT_PULLDOWN)
		cfg_val = 0x2;
	else if ((mux_val & OMAP_PIN_INPUT) == OMAP_PIN_INPUT)
		cfg_val = 0x0;
	else
		cfg_val = 0x3;

	reg_base = OMAP4_CTRL_MODULE_PAD_CORE;
	reg_val = omap_readl(reg_base +
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_USBB_HSIC);
	reg_val &= ~(0x3 << shift);
	reg_val |= cfg_val << shift;
	omap_writel(reg_val, reg_base +
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_USBB_HSIC);
}

int __init omap4_muxtbl_init(int flags)
{
	omap4_mux_init(NULL, NULL, flags);

	core_part = omap_mux_get("core");
	wkup_part = omap_mux_get("wkup");

	return 0;
}

static int __init omap4_muxtbl_in_gpio_expander(struct omap_muxtbl *muxtbl)
{
	if (unlikely(muxtbl->gpio.gpio >= OMAP_MAX_GPIO_LINES &&
		     muxtbl->gpio.gpio != OMAP_MUXTBL_NO_GPIO))
		return 1;
	return 0;
}

int __init omap4_muxtbl_add_mux(struct omap_muxtbl *muxtbl)
{
	struct omap_board_mux *mux = &muxtbl->mux;
	int wk_mux = muxtbl->domain;
	struct omap_mux_partition *partition;

	if (omap4_muxtbl_in_gpio_expander(muxtbl))
		return 0;

	if (unlikely(wk_mux))
		partition = wkup_part;
	else
		partition = core_part;

	omap4_muxtbl_set_pbias_gpio_pre(partition, mux);

	omap_mux_write(partition, mux->value, mux->reg_offset);

	omap_muxtbl_set_usbbx_gpio(muxtbl);

	omap4_muxtbl_set_pbias_gpio_post(partition, mux);

	return 0;
}

#else /* CONFIG_ARCH_OMAP4 */

int __init omap4_muxtbl_init(int flags)
{
	return 0;
}

int __init omap4_muxtbl_add_mux(struct omap_muxtbl *muxtbl)
{
	return 0;
}

#endif /* CONFIG_ARCH_OMAP4 */

/* arch/arm/mach-omap2/omap44xx_muxtbl.h
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

#ifndef __OMAP44XX_MUXTBL_H__
#define __OMAP44XX_MUXTBL_H__

#include "mux44xx.h"

#define OMAP4_MUXTBL_DOMAIN_CORE	0
#define OMAP4_MUXTBL_DOMAIN_WKUP	1

#define OMAP4_CTRL_MODULE_PAD_EXT_GPIO000_OFFSET	0x00

#define OMAP4_MUXTBL(_domain, _M0, _mux_value, _gpio, _label)	\
{									\
	.gpio = {							\
		.gpio = _gpio,						\
		.label = _label,					\
	},								\
	.domain = _domain,						\
	.mux = OMAP4_MUX(_M0, _mux_value),				\
	.pin = #_M0,							\
}

extern int __init omap4_muxtbl_init(int flags);

extern int __init omap4_muxtbl_add_mux(struct omap_muxtbl *muxtbl);

#endif /* __OMAP44XX_MUXTBL_H__ */

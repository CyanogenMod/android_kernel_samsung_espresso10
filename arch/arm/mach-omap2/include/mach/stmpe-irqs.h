/* arch/arm/mach-omap2/include/mach/stmpe-irqs.h
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

#ifndef __STMPE_IRQS_H__
#define __STMPE_IRQS_H__

#ifdef CONFIG_MFD_STMPE

#define STMPE_NR_INTERNAL_IRQS	9
#define STMPE_INT_GPIO(x)	(STMPE_NR_INTERNAL_IRQS + (x))

#define STMPE_NR_GPIOS		24
#define STMPE_NR_IRQS		STMPE_INT_GPIO(STMPE_NR_GPIOS)

/* TODO: use IRQ_END of the previous IP/device */
#define __OMAP_STMPE_IRQ_BASE	(OMAP_GPMC_IRQ_END)

#define OMAP_STMPE_IRQ_BASE(id)	\
	(__OMAP_STMPE_IRQ_BASE + (STMPE_NR_IRQS * (id - 1)))
#define OMAP_STMPE_IRQ_END	\
	(__OMAP_STMPE_IRQ_BASE + (STMPE_NR_IRQS * CONFIG_OMAP_STMPE_NR))

#undef NR_IRQS
#define NR_IRQS			OMAP_STMPE_IRQ_END

#else /* CONFIG_MFD_STMPE */
/* TODO: use a same IRQ_END if this device is not used */
#define OMAP_STMPE_IRQ_END	(OMAP_GPMC_IRQ_END)
#endif /* CONFIG_MFD_STMPE */

#endif /* __STMPE_IRQS_H__ */

/* arch/arm/mach-omap2/include/mach/max77693-irqs.h
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
#ifndef __MAX77693_IRQS_H__
#define __MAX77693_IRQS_H__
#ifdef CONFIG_MFD_MAX77693

/* TODO: use IRQ_END of the previous IP/device */
#define OMAP_MAX77693_IRQ_BASE	(OMAP_STMPE_IRQ_END)
#define MAX77693_NR_IRQS	29
#define OMAP_MAX77693_IRQ_END	(OMAP_MAX77693_IRQ_BASE + MAX77693_NR_IRQS)

#undef NR_IRQS
#define NR_IRQS			(OMAP_MAX77693_IRQ_END)

#else /* CONFIG_MFD_MAX77693 */

/* TODO: use a same IRQ_END if this device is not used */
#define OMAP_MAX77693_IRQ_END	(OMAP_STMPE_IRQ_END)

#endif /* CONFIG_MFD_MAX77693 */
#endif /* __MAX77693_IRQS_H__ */

/* arch/arm/mach-omap2/omap_muxtbl.h
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

#ifndef __OMAP_MUXTBL_H__
#define __OMAP_MUXTBL_H__

#include <linux/rbtree.h>

#define OMAP_MUXTBL_NO_GPIO		0xFFFFFFFF

#define OMAP_MUXTBL_ERR_INSERT_TREE	1
#define OMAP_MUXTBL_ERR_ADD_MUX		2
#define OMAP_MUXTBL_ERR_INT_GPIO	3

struct omap_muxtbl {
	struct gpio gpio;

	unsigned int domain;
	struct omap_board_mux mux;
	char *pin;

	/* rb-tree by label */
	struct rb_node node_l;
	unsigned crc32_l;

	/* rb-tree by pin */
	struct rb_node node_p;
	unsigned crc32_p;
};

struct omap_muxset {
	unsigned int rev;
	struct omap_muxtbl *muxtbl;
	unsigned int size;
};

extern int __init omap_muxtbl_init(int flags);

extern int __init omap_muxtbl_add_muxset(struct omap_muxset *muxset);

extern struct omap_muxtbl *omap_muxtbl_find_by_name(const char *label);

extern struct omap_muxtbl *omap_muxtbl_find_by_pin(const char *pin);

extern int __init omap_muxtbl_get_gpio_by_name(const char *label);

#endif /* __OMAP_MUXTBL_H__ */

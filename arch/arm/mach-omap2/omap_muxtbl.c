/* arch/arm/mach-omap2/omap_muxtbl.c
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

#include <linux/crc32.h>
#include <linux/gpio.h>
#include <linux/rbtree.h>

#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"

static struct rb_root rb_muxtbl_l __initdata = RB_ROOT;
static struct rb_root rb_muxtbl_p __initdata = RB_ROOT;

#define make_fn_omap_muxtbl_rb_search_x(__x)				\
static struct omap_muxtbl __init *omap_muxtbl_rb_search_##__x		\
	(unsigned int crc32)						\
{									\
	struct rb_node *node = rb_muxtbl_##__x.rb_node;			\
	struct omap_muxtbl *muxtbl;					\
									\
	while (node) {							\
		muxtbl = container_of(node, struct omap_muxtbl,		\
				      node_##__x);			\
									\
		if (crc32 < muxtbl->crc32_##__x)			\
			node = node->rb_left;				\
		else if (crc32 > muxtbl->crc32_##__x)			\
			node = node->rb_right;				\
		else							\
			return muxtbl;					\
	}								\
									\
	return NULL;							\
}

make_fn_omap_muxtbl_rb_search_x(l);
make_fn_omap_muxtbl_rb_search_x(p);

#define make_fn_omap_muxtbl_rb_insert_x(__x)				\
static int __init omap_muxtbl_rb_insert_##__x				\
	(struct omap_muxtbl *muxtbl)					\
{									\
	struct rb_node **new = &(rb_muxtbl_##__x.rb_node);		\
	struct rb_node *parent = NULL;					\
	struct omap_muxtbl *this;					\
									\
	while (*new) {							\
		this = container_of(*new, struct omap_muxtbl,		\
				    node_##__x);			\
									\
		parent = *new;						\
		if (muxtbl->crc32_##__x < this->crc32_##__x)		\
			new = &((*new)->rb_left);			\
		else if (muxtbl->crc32_##__x > this->crc32_##__x)	\
			new = &((*new)->rb_right);			\
		else							\
			return -1;					\
	}								\
									\
	rb_link_node(&muxtbl->node_##__x, parent, new);			\
	rb_insert_color(&muxtbl->node_##__x, &rb_muxtbl_##__x);		\
									\
	return 0;							\
}

make_fn_omap_muxtbl_rb_insert_x(l);
make_fn_omap_muxtbl_rb_insert_x(p);

static struct omap_muxtbl __init *omap_muxtbl_rb_search(unsigned crc32_l,
							unsigned crc32_p)
{
	struct omap_muxtbl *muxtbl_l = omap_muxtbl_rb_search_l(crc32_l);
	struct omap_muxtbl *muxtbl_p = omap_muxtbl_rb_search_p(crc32_p);

	if (muxtbl_l)
		return muxtbl_l;
	else if (muxtbl_p)
		return muxtbl_p;
	else
		return NULL;
}

static int __init omap_muxtbl_rb_insert(struct omap_muxtbl *muxtbl)
{
	int err;

	err = omap_muxtbl_rb_insert_l(muxtbl);
	if (unlikely(err))
		return err;

	err = omap_muxtbl_rb_insert_p(muxtbl);
	if (unlikely(err))
		return err;

	return 0;
}

int __init omap_muxtbl_init(int flags)
{
	if (cpu_is_omap44xx())
		return omap4_muxtbl_init(flags);

	return -1;
}

static int __init omap_muxtbl_add_mux(struct omap_muxtbl *muxtbl)
{
	if (cpu_is_omap44xx())
		return omap4_muxtbl_add_mux(muxtbl);

	return -1;
}

int __init omap_muxtbl_add_muxset(struct omap_muxset *muxset)
{
	struct omap_muxtbl *muxtbl = muxset->muxtbl;
	struct omap_muxtbl *this;
	unsigned int i = muxset->size;
	unsigned int crc32_l;
	unsigned int crc32_p;
	int err;

	if (!muxtbl)
		return 0;

	do {
		crc32_l = crc32(0, muxtbl->gpio.label,
				strlen(muxtbl->gpio.label));
		crc32_p = crc32(0, muxtbl->pin, strlen(muxtbl->pin));
		muxtbl->crc32_l = crc32_l;
		muxtbl->crc32_p = crc32_p;

		this = omap_muxtbl_rb_search(crc32_l, crc32_p);
		if (this)
			continue;

		err = omap_muxtbl_rb_insert(muxtbl);
		if (unlikely(err))
			return -OMAP_MUXTBL_ERR_INSERT_TREE;

		err = omap_muxtbl_add_mux(muxtbl);
		if (unlikely(err))
			return -OMAP_MUXTBL_ERR_ADD_MUX;
	} while (--i && ++muxtbl);

	return 0;
}

struct omap_muxtbl __init *omap_muxtbl_find_by_name(const char *label)
{
	unsigned int crc32 = crc32(0, label, strlen(label));

	return omap_muxtbl_rb_search(crc32, 0);
}

int __init omap_muxtbl_get_gpio_by_name(const char *label)
{
	int gpio;
	struct omap_muxtbl *muxtbl = omap_muxtbl_find_by_name(label);

	if (unlikely(muxtbl == NULL))
		gpio = -EINVAL;
	else
		gpio = muxtbl->gpio.gpio;

	return gpio;
}

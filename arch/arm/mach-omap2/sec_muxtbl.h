/* arch/arm/mach-omap2/sec_muxtbl.h
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

#ifndef __SEC_MUXTBL_H__
#define __SEC_MUXTBL_H__

#define SEC_MUXTBL_TYPE_ANY		0xFFFFFFFF

#define __sec_muxtbl_s_attr__		\
		__attribute__((used, __section__(".sec_muxtbl_start")))
#define __sec_muxtbl_d_attr__		\
		__attribute__((used, __section__(".sec_muxtbl_data")))
#define __sec_muxtbl_e_attr__		\
		__attribute__((used, __section__(".sec_muxtbl_end")))

#define add_sec_muxtbl_to_list(_type, _rev, _muxtbl)			\
static struct omap_muxset						\
__sec_muxset_data_##_type##_##_rev __initdata = {			\
	.rev = _rev,							\
	.muxtbl = _muxtbl,						\
	.size = ARRAY_SIZE(_muxtbl),					\
};									\
									\
static struct sec_muxtbl_node						\
__sec_muxtbl_data_##_type##_##_rev __sec_muxtbl_d_attr__ = {		\
	.type = _type,							\
	.muxset_ptr = &__sec_muxset_data_##_type##_##_rev,		\
};

struct sec_muxtbl_node {
	unsigned int type;
	struct omap_muxset *muxset_ptr;
	struct list_head list;
};

/**
 * sec_muxtbl_init - initialize OMAP MUX Table
 * @type: hardware type
 * @rev: hardware revision
 */
int __init sec_muxtbl_init(unsigned int type, unsigned int rev);

#endif /* __SEC_MUXTBL_H__ */

/* arch/arm/mach-omap2/sec_common.h
 *
 * Copyright (C) 2010-2011 Samsung Electronics Co, Ltd.
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

#ifndef __SEC_COMMON_H__
#define __SEC_COMMON_H__

int sec_common_init_early(void);

int sec_common_init(void);

extern struct class *sec_class;

extern unsigned int system_rev;

extern char sec_androidboot_mode[16];

#endif /* __SEC_COMMON_H__ */

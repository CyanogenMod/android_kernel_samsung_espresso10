/* arch/arm/mach-omap2/sec_muxtbl.c
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
#include <linux/list.h>
#include <linux/list_sort.h>

#include "mux.h"
#include "omap_muxtbl.h"
#include "sec_muxtbl.h"

static struct sec_muxtbl_node __sec_muxtbl_data_s __sec_muxtbl_s_attr__;
static struct sec_muxtbl_node __sec_muxtbl_data_e __sec_muxtbl_e_attr__;

static int __init __sec_muxtbl_sort_cmp(void *priv, struct list_head *a,
					struct list_head *b)
{
	struct sec_muxtbl_node *entry_a =
	    list_entry(a, struct sec_muxtbl_node, list);
	struct sec_muxtbl_node *entry_b =
	    list_entry(b, struct sec_muxtbl_node, list);

	return (entry_a->muxset_ptr->rev > entry_b->muxset_ptr->rev) ? -1 : 1;
}

static int __init __sec_muxtbl_setup_list(unsigned int type,
					  struct sec_muxtbl_node **head,
					  unsigned int rev)
{
	unsigned int cnt = 0;
	struct sec_muxtbl_node *muxtbl_cur = &__sec_muxtbl_data_s;

	INIT_LIST_HEAD(&__sec_muxtbl_data_s.list);

	while (++muxtbl_cur < &__sec_muxtbl_data_e) {
		if (unlikely(muxtbl_cur->type != type))
			continue;
		if (muxtbl_cur->muxset_ptr->rev > rev)
			continue;
		list_add_tail(&(muxtbl_cur->list), &__sec_muxtbl_data_s.list);
		cnt++;
	}

	if (!cnt) {
		pr_warn("(%s): no available mux_data.\n", __func__);
		return -1;
	}

	list_sort(NULL, &__sec_muxtbl_data_s.list, __sec_muxtbl_sort_cmp);

	*head = &__sec_muxtbl_data_s;

	return 0;
}

int __init sec_muxtbl_init(unsigned int type, unsigned int rev)
{
	struct sec_muxtbl_node *head = NULL;
	struct sec_muxtbl_node *cur;
	struct list_head *pos;
	int package = OMAP_PACKAGE_CBS;
	int err;
	static unsigned int init_done;

	if (!init_done) {
		if (omap_rev() == OMAP4430_REV_ES1_0)
			package = OMAP_PACKAGE_CBL;

		err = omap_muxtbl_init(package);
		if (err)
			return err;

		init_done = 1;
	}

	if (__sec_muxtbl_setup_list(type, &head, rev))
		return 0;

	list_for_each(pos, &(head->list)) {
		cur = list_entry(pos, struct sec_muxtbl_node, list);
		err = omap_muxtbl_add_muxset(cur->muxset_ptr);
		if (unlikely(err))
			return err;
	}

	return 0;
}

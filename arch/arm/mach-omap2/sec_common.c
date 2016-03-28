/* arch/arm/mach-omap2/sec_common.c
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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sort.h>

#include <mach/hardware.h>
#include <mach/id.h>

#include <plat/io.h>
#include <plat/system.h>

#include "mux.h"
#include "omap_muxtbl.h"
#include "sec_common.h"

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

char sec_androidboot_mode[16];
EXPORT_SYMBOL(sec_androidboot_mode);

static __init int setup_androidboot_mode(char *opt)
{
	strncpy(sec_androidboot_mode, opt, 15);
	return 0;
}

__setup("androidboot.mode=", setup_androidboot_mode);

/*
 * Store a handy board information string which we can use elsewhere like
 * like in panic situation
 */
static char sec_panic_string[256];
static void __init sec_common_set_panic_string(void)
{
	char *cpu_type = "UNKNOWN";

#if defined(CONFIG_ARCH_OMAP3)
	cpu_type = cpu_is_omap34xx() ? "OMAP3430" : "OMAP3630";
#elif defined(CONFIG_ARCH_OMAP4)
	cpu_type = cpu_is_omap443x() ? "OMAP4430" :
		   cpu_is_omap446x() ? "OMAP4460" :
		   cpu_is_omap447x() ? "OMAP4470" : "Unknown";
#endif /* CONFIG_ARCH_OMAP* */

	snprintf(sec_panic_string, ARRAY_SIZE(sec_panic_string),
		"%02X, cpu %s ES%d.%d",
		system_rev, cpu_type,
		(GET_OMAP_REVISION() >> 4) & 0xf,
		GET_OMAP_REVISION() & 0xf);

	mach_panic_string = sec_panic_string;
}

int __init sec_common_init_early(void)
{
	sec_common_set_panic_string();

	return 0;
}				/* end fn sec_common_init_early */

int __init sec_common_init(void)
{
	char *hwrev_gpio[] = {
		"HW_REV0", "HW_REV1", "HW_REV2", "HW_REV3"
	};
	int gpio_pin;
	int i;

	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class))
		pr_err("Class(sec) Creating Fail!!!\n");

	for (i = 0; i < ARRAY_SIZE(hwrev_gpio); i++) {
		gpio_pin = omap_muxtbl_get_gpio_by_name(hwrev_gpio[i]);
		if (likely(gpio_pin != -EINVAL))
			gpio_request(gpio_pin, hwrev_gpio[i]);
	}

	return 0;
}				/* end fn sec_common_init */

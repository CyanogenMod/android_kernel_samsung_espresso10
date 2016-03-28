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
#include <linux/reboot.h>
#include <linux/sort.h>

#include <mach/hardware.h>
#include <mach/id.h>

#include <plat/io.h>
#include <plat/system.h>

#include "mux.h"
#include "omap_muxtbl.h"
#include "sec_common.h"

#if defined(CONFIG_ARCH_OMAP3)
#define SEC_REBOOT_MODE_ADDR		(OMAP343X_CTRL_BASE + 0x0918)
#define SEC_REBOOT_FLAG_ADDR		(OMAP343X_CTRL_BASE + 0x09C4)
#define SEC_REBOOT_CMD_ADDR		NULL
#elif defined(CONFIG_ARCH_OMAP4)
#define OMAP_SW_BOOT_CFG_ADDR		0x4A326FF8
#define SEC_REBOOT_MODE_ADDR		(OMAP_SW_BOOT_CFG_ADDR)
#define SEC_REBOOT_FLAG_ADDR		(OMAP_SW_BOOT_CFG_ADDR - 0x04)
/* -0x08/-0x0C are reserved for debug */
#define SEC_REBOOT_CMD_ADDR		(OMAP_SW_BOOT_CFG_ADDR - 0x10)
#else
#error "unsupported mach-type for OMAP-Samsung"
#endif

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

void (*__arch_reset)(char, const char *);
static void sec_common_arch_reset(char mode, const char *cmd);

char sec_androidboot_mode[16];
EXPORT_SYMBOL(sec_androidboot_mode);

static __init int setup_androidboot_mode(char *opt)
{
	strncpy(sec_androidboot_mode, opt, 15);
	return 0;
}

__setup("androidboot.mode=", setup_androidboot_mode);

struct sec_reboot_code {
	char *cmd;
	int mode;
};

static int __sec_common_cmp_reboot_cmd(const void *code0, const void *code1)
{
	struct sec_reboot_code *r_code0 = (struct sec_reboot_code *)code0;
	struct sec_reboot_code *r_code1 = (struct sec_reboot_code *)code1;

	return (strlen(r_code0->cmd) > strlen(r_code1->cmd)) ? -1 : 1;
}

static int __sec_common_reboot_call(struct notifier_block *this,
				    unsigned long code, void *cmd)
{
	int mode = REBOOT_MODE_NONE;
	unsigned int value;
	size_t cmd_len;

	struct sec_reboot_code reboot_tbl[] = {
		{"arm11_fota", REBOOT_MODE_PREFIX | REBOOT_MODE_ARM11_FOTA},
		{"arm9_fota", REBOOT_MODE_PREFIX | REBOOT_MODE_ARM9_FOTA},
		{"recovery", REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY},
		{"cp_crash", REBOOT_MODE_PREFIX | REBOOT_MODE_CP_CRASH},
		{"fota", REBOOT_MODE_PREFIX | REBOOT_MODE_ARM11_FOTA},
		{"fota_bl", REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA_BL},
		{"debug", REBOOT_SET_PREFIX | REBOOT_SET_DEBUG},
		{"swsel", REBOOT_SET_PREFIX | REBOOT_SET_SWSEL},
		{"sud", REBOOT_SET_PREFIX | REBOOT_SET_SUD},
	};
	size_t i, n;

	sort(reboot_tbl, ARRAY_SIZE(reboot_tbl),
	     sizeof(struct sec_reboot_code),
	     __sec_common_cmp_reboot_cmd, NULL);

	if ((code == SYS_RESTART) && cmd) {
		n = ARRAY_SIZE(reboot_tbl);
		for (i = 0; i < n; i++) {
			cmd_len = strlen(reboot_tbl[i].cmd);
			if (!strncmp((char *)cmd,
				     reboot_tbl[i].cmd, cmd_len)) {
				mode = reboot_tbl[i].mode;
				break;
			}
		}

		if (mode == (REBOOT_SET_PREFIX | REBOOT_SET_DEBUG) ||
		    mode == (REBOOT_SET_PREFIX | REBOOT_SET_SWSEL) ||
		    mode == (REBOOT_SET_PREFIX | REBOOT_SET_SUD)) {
			if (!kstrtouint((char *)cmd + cmd_len, 0, &value))
				mode |= value;
			else
				mode = REBOOT_MODE_NONE;
		}
		pr_info("(%s): mode = 0x%x, cmd = %s\n",
			__func__, mode, (char *)cmd);
	}

	if (SEC_REBOOT_CMD_ADDR)
		omap_writel(mode, SEC_REBOOT_CMD_ADDR);

	return NOTIFY_DONE;
}				/* end fn __sec_common_reboot_call */

static struct notifier_block __sec_common_reboot_notifier = {
	.notifier_call = __sec_common_reboot_call,
};

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

	/* replacing arch_reset */
	__arch_reset = arch_reset;
	arch_reset = sec_common_arch_reset;

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

int __init sec_common_init_post(void)
{
	register_reboot_notifier(&__sec_common_reboot_notifier);

	return 0;
}				/* end fn sec_common_init_post */

struct sec_reboot_mode {
	char *cmd;
	char mode;
};

static char __sec_common_convert_reboot_mode(char mode,  const char *cmd)
{
	char new_mode = mode;
	struct sec_reboot_mode mode_tbl[] = {
		{"arm11_fota", 'f'},
		{"arm9_fota", 'f'},
		{"recovery", 'r'},
        {"bootloader", 'b'},
		{"download", 'd'},
		{"cp_crash", 'C'}
	};
	size_t i, n;
	if (cmd == NULL)
		goto __return;
	n = ARRAY_SIZE(mode_tbl);
	for (i = 0; i < n; i++) {
		if (!strcmp(cmd, mode_tbl[i].cmd)) {
			new_mode = mode_tbl[i].mode;
			goto __return;
		}
	}

__return:
	return new_mode;
}

static int sec_common_update_reboot_reason(char mode, const char *cmd)
{
	u32 scpad = 0;
	const u32 scpad_addr = SEC_REBOOT_MODE_ADDR;
	u32 reason = REBOOTMODE_NORMAL;
	char *rebootflag = "RESET";

#if defined(CONFIG_ARCH_OMAP3)
	scpad = omap_readl(scpad_addr);
#endif /* CONFIG_ARCH_OMAP3 */

	/* for the compatibility with LSI chip-set based products */
	mode = __sec_common_convert_reboot_mode(mode, cmd);

	switch (mode) {
	case 'r':		/* reboot mode = recovery */
		reason = REBOOTMODE_RECOVERY;
		break;
	case 'f':		/* reboot mode = fota */
		reason = REBOOTMODE_FOTA;
		break;
	case 't':		/* reboot mode = shutdown with TA */
	case 'u':		/* reboot mode = shutdown with USB */
	case 'j':		/* reboot mode = shutdown with JIG */
		reason = REBOOTMODE_SHUTDOWN;
		rebootflag = "POFF";
		break;
	case 'b':		/* reboot mode = bootloader */
		reason = REBOOTMODE_DOWNLOAD;
		break;
	case 'd':		/* reboot mode = download */
		reason = REBOOTMODE_DOWNLOAD;
		break;
	default:		/* reboot mode = normal */
		reason = REBOOTMODE_NORMAL;
		break;
	}

	omap_writel(scpad | reason, scpad_addr);
	omap_writel(*(u32 *)rebootflag, SEC_REBOOT_FLAG_ADDR);

	return (int)mode;
}				/* sec_common_update_reboot_reason */

static void sec_common_arch_reset(char mode, const char *cmd)
{
	mode = sec_common_update_reboot_reason(mode, cmd);

	if (__arch_reset)
		__arch_reset(mode, cmd);
}

/* arch/arm/mach-omap2/board-espresso-wifi.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-tuna-wifi.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <asm/setup.h>
#include <linux/if.h>
#include <linux/wlan_plat.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <plat/mmc.h>

#include <linux/random.h>
#include <linux/jiffies.h>

#include "board-espresso.h"
#include "hsmmc.h"

#define GPIO_WLAN_HOST_WAKE	81
#define GPIO_WLAN_EN		104

#define WLC_CNTRY_BUF_SZ	4

/* wifi private data */
static int espresso_wifi_cd; /* WIFI virtual 'card detect' status */
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

static unsigned char espresso_mac_addr[IFHWADDRLEN]
	= { 0, 0x90, 0x4c, 0, 0, 0 };

static struct resource espresso_wifi_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.flags	= IORESOURCE_IRQ
			| IORESOURCE_IRQ_HIGHLEVEL
			| IORESOURCE_IRQ_SHAREABLE,
	},
};

static int espresso_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;

	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;

	return 0;
}

static unsigned int espresso_wifi_status(struct device *dev)
{
	return espresso_wifi_cd;
}

struct mmc_platform_data espresso_wifi_data = {
	.ocr_mask		= MMC_VDD_165_195 | MMC_VDD_20_21,
	.built_in		= 1,
	.status			= espresso_wifi_status,
	.card_present		= 0,
	.register_status_notify	= espresso_wifi_status_register,
};

static int espresso_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	espresso_wifi_cd = val;

	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		pr_warning("%s: Nobody to notify\n", __func__);

	return 0;
}

struct fixed_voltage_data {
	struct regulator_desc desc;
	struct regulator_dev *dev;
	int microvolts;
	int gpio;
	unsigned startup_delay;
	bool enable_high;
	bool is_enabled;
};

static struct regulator_consumer_supply espresso_vmmc5_supply = {
	.supply		= "vmmc",
	.dev_name	= "omap_hsmmc.4",
};

static struct regulator_init_data espresso_vmmc5 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &espresso_vmmc5_supply,
};

static struct fixed_voltage_config espresso_vwlan = {
	.supply_name		= "vwl1271",
	.microvolts		= 2000000, /* 2.0V */
	.startup_delay		= 70000, /* 70msec */
	.gpio			= GPIO_WLAN_EN,
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &espresso_vmmc5,
};

static struct platform_device omap_vwlan_device = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data = &espresso_vwlan,
	},
};

static int espresso_wifi_power(int on)
{
	pr_debug("%s: %d\n", __func__, on);
	gpio_set_value(espresso_vwlan.gpio, on);
	msleep(300);
	return 0;
}

static int espresso_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	return 0;
}

static int __init espresso_mac_addr_setup(char *str)
{
	char macstr[IFHWADDRLEN*3];
	char *macptr = macstr;
	char *token;
	int i = 0;

	if (!str)
		return 0;

	pr_debug("wlan MAC = %s\n", str);
	if (strlen(str) >= sizeof(macstr))
		return 0;

	strcpy(macstr, str);

	while ((token = strsep(&macptr, ":")) != NULL) {
		unsigned long val;
		int res;

		if (i >= IFHWADDRLEN)
			break;
		res = strict_strtoul(token, 0x10, &val);
		if (res < 0)
			return 0;
		espresso_mac_addr[i++] = (u8)val;
	}

	return 1;
}
__setup("androidboot.macaddr=", espresso_mac_addr_setup);

static int espresso_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;

	if (!buf)
		return -EFAULT;

	if ((espresso_mac_addr[4] == 0) && (espresso_mac_addr[5] == 0)) {
		srandom32((uint)jiffies);
		rand_mac = random32();
		espresso_mac_addr[3] = (unsigned char)rand_mac;
		espresso_mac_addr[4] = (unsigned char)(rand_mac >> 8);
		espresso_mac_addr[5] = (unsigned char)(rand_mac >> 16);
	}
	memcpy(buf, espresso_mac_addr, IFHWADDRLEN);

	return 0;
}

/* Customized Locale table : OPTIONAL feature */
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int  custom_locale_rev;
};

static struct cntry_locales_custom espresso_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XY", 4},  /* universal */
	{"US", "US", 69}, /* input ISO "US" to : US regrev 69 */
	{"CA", "US", 69}, /* input ISO "CA" to : US regrev 69 */
	{"EU", "EU", 5},  /* European union countries */
	{"AT", "EU", 5},
	{"BE", "EU", 5},
	{"BG", "EU", 5},
	{"CY", "EU", 5},
	{"CZ", "EU", 5},
	{"DK", "EU", 5},
	{"EE", "EU", 5},
	{"FI", "EU", 5},
	{"FR", "EU", 5},
	{"DE", "EU", 5},
	{"GR", "EU", 5},
	{"HU", "EU", 5},
	{"IE", "EU", 5},
	{"IT", "EU", 5},
	{"LV", "EU", 5},
	{"LI", "EU", 5},
	{"LT", "EU", 5},
	{"LU", "EU", 5},
	{"MT", "EU", 5},
	{"NL", "EU", 5},
	{"PL", "EU", 5},
	{"PT", "EU", 5},
	{"RO", "EU", 5},
	{"SK", "EU", 5},
	{"SI", "EU", 5},
	{"ES", "EU", 5},
	{"SE", "EU", 5},
	{"GB", "EU", 5},  /* input ISO "GB" to : EU regrev 05 */
	{"IL", "IL", 0},
	{"CH", "CH", 0},
	{"TR", "TR", 0},
	{"NO", "NO", 0},
	{"KR", "XY", 3},
	{"AU", "XY", 3},
	{"CN", "XY", 3},  /* input ISO "CN" to : XY regrev 03 */
	{"TW", "XY", 3},
	{"AR", "XY", 3},
	{"MX", "XY", 3}
};

static void *espresso_wifi_get_country_code(char *ccode)
{
	int size = ARRAY_SIZE(espresso_wifi_translate_custom_table);
	int i;

	if (!ccode)
		return NULL;

	for (i = 0; i < size; i++)
		if (strcmp(ccode,
			espresso_wifi_translate_custom_table[i].iso_abbrev)
				== 0)
			return &espresso_wifi_translate_custom_table[i];

	return &espresso_wifi_translate_custom_table[0];
}

static struct wifi_platform_data espresso_wifi_control = {
	.set_power		= espresso_wifi_power,
	.set_reset		= espresso_wifi_reset,
	.set_carddetect		= espresso_wifi_set_carddetect,
	.get_mac_addr		= espresso_wifi_get_mac_addr,
	.get_country_code	= espresso_wifi_get_country_code,
};

static struct platform_device espresso_wifi_device = {
		.name           = "bcmdhd_wlan",
		.id             = 1,
		.num_resources  = ARRAY_SIZE(espresso_wifi_resources),
		.resource       = espresso_wifi_resources,
		.dev            = {
			.platform_data	= &espresso_wifi_control,
		},
};

static void __init espresso_wlan_gpio(void)
{
	pr_debug("%s\n", __func__);

	espresso_wifi_resources[0].start =
		gpio_to_irq(GPIO_WLAN_HOST_WAKE);
	espresso_wifi_resources[0].end =
		espresso_wifi_resources[0].start;
	gpio_request(GPIO_WLAN_HOST_WAKE, "WLAN_HOST_WAKE");
	gpio_direction_input(GPIO_WLAN_HOST_WAKE);
}

void __init omap4_espresso_wifi_init(void)
{
	pr_debug("%s\n", __func__);
	espresso_wlan_gpio();
	platform_device_register(&omap_vwlan_device);

	platform_device_register(&espresso_wifi_device);
}

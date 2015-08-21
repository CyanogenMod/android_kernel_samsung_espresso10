/* arch/arm/mach-omap2/board-palau-wifi.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-t1-wifi.c
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
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <plat/mmc.h>

#include <linux/random.h>
#include <linux/jiffies.h>

#include "board-palau.h"
#include "control.h"
#include "hsmmc.h"
#include "mux.h"
#include "omap_muxtbl.h"

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wifi_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wifi_mem_prealloc
wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
static void *palau_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;
	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int __init palau_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0; (i < PREALLOC_WLAN_NUMBER_OF_SECTIONS); i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (!wifi_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}
	wlan_static_scan_buf0 = kmalloc(65536, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;
	wlan_static_scan_buf1 = kmalloc(65536, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_mem_alloc;
	printk(KERN_INFO"%s: WIFI MEM Allocated\n", __func__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wifi_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

#define WLC_CNTRY_BUF_SZ	4

/* wifi private data */
static int palau_wifi_cd; /* WIFI virtual 'card detect' status */
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

static int palau_wifi_power_state;
static int palau_wifi_reset_state;

static unsigned char palau_mac_addr[IFHWADDRLEN]
	= { 0, 0x90, 0x4c, 0, 0, 0 };

static struct resource palau_wifi_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.flags	= IORESOURCE_IRQ
			| IORESOURCE_IRQ_HIGHLEVEL
			| IORESOURCE_IRQ_SHAREABLE,
	},
};

static int palau_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;

	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;

	return 0;
}

static unsigned int palau_wifi_status(struct device *dev)
{
	return palau_wifi_cd;
}

struct mmc_platform_data palau_wifi_data = {
	.ocr_mask		= MMC_VDD_165_195 | MMC_VDD_20_21,
	.built_in		= 1,
	.status			= palau_wifi_status,
	.card_present		= 0,
	.register_status_notify	= palau_wifi_status_register,
};

static int palau_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	palau_wifi_cd = val;

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

static struct regulator_consumer_supply palau_vmmc5_supply = {
	.supply		= "vmmc",
	.dev_name	= "omap_hsmmc.4",
};

static struct regulator_init_data palau_vmmc5 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &palau_vmmc5_supply,
};

static struct fixed_voltage_config palau_vwlan = {
	.supply_name		= "vwl1271",
	.microvolts		= 2000000, /* 2.0V */
	.startup_delay		= 70000, /* 70msec */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &palau_vmmc5,
};

static struct platform_device omap_vwlan_device = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data	= &palau_vwlan,
	},
};

static int palau_wifi_power(int on)
{
	pr_debug("%s: %d\n", __func__, on);
	gpio_set_value(palau_vwlan.gpio, on);

	palau_wifi_power_state = on;

	return 0;
}

static int palau_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	palau_wifi_reset_state = on;

	return 0;
}

static int __init palau_mac_addr_setup(char *str)
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
		palau_mac_addr[i++] = (u8)val;
	}

	return 1;
}

__setup("androidboot.macaddr=", palau_mac_addr_setup);

static int palau_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;

	if (!buf)
		return -EFAULT;

	if ((palau_mac_addr[4] == 0) && (palau_mac_addr[5] == 0)) {
		srandom32((uint)jiffies);
		rand_mac = random32();
		palau_mac_addr[3] = (unsigned char)rand_mac;
		palau_mac_addr[4] = (unsigned char)(rand_mac >> 8);
		palau_mac_addr[5] = (unsigned char)(rand_mac >> 16);
	}
	memcpy(buf, palau_mac_addr, IFHWADDRLEN);

	return 0;
}

/* Customized Locale table : OPTIONAL feature */
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int  custom_locale_rev;
};

static struct cntry_locales_custom palau_wifi_translate_custom_table[] = {
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

static void *palau_wifi_get_country_code(char *ccode)
{
	int size = ARRAY_SIZE(palau_wifi_translate_custom_table);
	int i;

	if (!ccode)
		return NULL;

	for (i = 0; i < size; i++)
		if (strcmp(ccode,
			palau_wifi_translate_custom_table[i].iso_abbrev)
				== 0)
			return &palau_wifi_translate_custom_table[i];

	return &palau_wifi_translate_custom_table[0];
}

static struct wifi_platform_data palau_wifi_control = {
	.set_power		= palau_wifi_power,
	.set_reset		= palau_wifi_reset,
	.set_carddetect		= palau_wifi_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc		= palau_wifi_mem_prealloc,
#endif
	.get_mac_addr		= palau_wifi_get_mac_addr,
	.get_country_code	= palau_wifi_get_country_code,
};

static struct platform_device palau_wifi_device = {
		.name           = "bcmdhd_wlan",
		.id             = 1,
		.num_resources  = ARRAY_SIZE(palau_wifi_resources),
		.resource       = palau_wifi_resources,
		.dev            = {
			.platform_data = &palau_wifi_control,
		},
};

static void __init palau_wlan_gpio(void)
{
	unsigned int gpio_wlan_int =
		omap_muxtbl_get_gpio_by_name("WLAN_INT");

	pr_debug("%s: start\n", __func__);

	palau_vwlan.gpio = omap_muxtbl_get_gpio_by_name("WLAN_EN");

	if (gpio_wlan_int != -EINVAL) {
		palau_wifi_resources[0].start =
			gpio_to_irq(gpio_wlan_int);
		palau_wifi_resources[0].end =
			palau_wifi_resources[0].start;
		gpio_request(gpio_wlan_int, "WLAN_INT");
		gpio_direction_input(gpio_wlan_int);
	}
}

void __init omap4_palau_wifi_init(void)
{
	pr_debug("%s: start\n", __func__);
	palau_wlan_gpio();
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	palau_init_wifi_mem();
#endif
	platform_device_register(&omap_vwlan_device);
	platform_device_register(&palau_wifi_device);
}

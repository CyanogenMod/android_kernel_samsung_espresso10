/* arch/arm/mach-omap2/board-kona.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-palau.c
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
#include <linux/input.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/ion.h>
#include <linux/memblock.h>
#include <linux/omap_ion.h>
#include <linux/ramoops.h>
#include <linux/reboot.h>
#include <linux/sysfs.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/cpu.h>
#include <plat/remoteproc.h>
#include <plat/usb.h>

#ifdef CONFIG_OMAP_HSI_DEVICE
#include <plat/omap_hsi.h>
#endif

#include <mach/dmm.h>
#include <mach/omap4-common.h>
#include <mach/id.h>
#ifdef CONFIG_ION_OMAP
#include <mach/omap4_ion.h>
#endif

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "board-kona.h"
#include "control.h"
#include "mux.h"
#include "omap4-sar-layout.h"
#include "omap_muxtbl.h"

#include "sec_common.h"
#include "sec_debug.h"
#include "sec_getlog.h"
#include "sec_muxtbl.h"

/* gpio to classify 3G / WiFi-Only
 *
 * HW_REV4 | HIGH | LOW
 * --------+------+------
 *         | 3G O | 3G X
 */
#define GPIO_HW_REV4		170

/* gpio to classify With/Without WACOM
 *
 * HW_REV5 | HIGH | LOW
 * --------+------+------
 *         | WCOM |  X
 */
#define GPIO_HW_REV5		169

#define KONA_MEM_BANK_0_SIZE		0x20000000
#define KONA_MEM_BANK_0_ADDR		0x80000000
#define KONA_MEM_BANK_1_SIZE		0x20000000
#define KONA_MEM_BANK_1_ADDR		0xA0000000

#define KONA_RAMCONSOLE_START		(PLAT_PHYS_OFFSET + SZ_512M)
#define KONA_RAMCONSOLE_SIZE		SZ_2M
#define KONA_RAMOOPS_START		(KONA_RAMCONSOLE_START + \
					 KONA_RAMCONSOLE_SIZE)
#define KONA_RAMOOPS_SIZE		SZ_1M

#if defined(CONFIG_ANDROID_RAM_CONSOLE)
static struct resource ramconsole_resources[] = {
	{
		.flags	= IORESOURCE_MEM,
		.start	= KONA_RAMCONSOLE_START,
		.end	= KONA_RAMCONSOLE_START
			+ KONA_RAMCONSOLE_SIZE - 1,
	 },
};

static struct platform_device ramconsole_device = {
	.name		= "ram_console",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ramconsole_resources),
	.resource	= ramconsole_resources,
};
#endif /* CONFIG_ANDROID_RAM_CONSOLE */

static struct ramoops_platform_data ramoops_pdata = {
	.mem_size	= KONA_RAMOOPS_SIZE,
	.mem_address	= KONA_RAMOOPS_START,
	.record_size	= SZ_32K,
	.dump_oops	= 0,	/* only for panic */
};

static struct platform_device ramoops_device = {
	.name		= "ramoops",
	.dev		= {
		.platform_data	= &ramoops_pdata,
	},
};

static struct platform_device bcm4334_bluetooth_device = {
	.name	= "bcm4334_bluetooth",
	.id	= -1,
};

static struct platform_device *kona_dbg_devices[] __initdata = {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
	&ramconsole_device,
#endif
	&ramoops_device,
};

static struct platform_device *kona_devices[] __initdata = {
	&bcm4334_bluetooth_device,
};

static void __init kona_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
	omap4_kona_display_early_init();
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_UTMI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode		= MUSB_OTG,
#else
	.mode		= MUSB_PERIPHERAL,
#endif
	.power		= 500,
};

static unsigned int board_type = SEC_MACHINE_KONA_WACOM;

static void __init omap4_kona_update_board_type(void)
{
	unsigned int types[] = {
		SEC_MACHINE_KONA_WIFI,
		SEC_MACHINE_KONA,
		SEC_MACHINE_KONA_WACOM_WIFI,
		SEC_MACHINE_KONA_WACOM,
	};
	unsigned int id;
	unsigned int reg_val;

	if (unlikely(system_rev < 4))
		return;

	/* because omap4_mux_init is not called when this function is
	 * called, padconf reg must be configured by low-level function. */

	/* GPIO_HW_REV4 - 170 */
	omap_writew(OMAP_MUX_MODE3,
		    OMAP4_CTRL_MODULE_PAD_CORE_MUX_PBASE +
		    OMAP4_CTRL_MODULE_PAD_USBB2_HSIC_STROBE_OFFSET);
	reg_val = omap_readl(OMAP4_CTRL_MODULE_PAD_CORE +
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_USBB_HSIC);
	reg_val &= ~(0x3 << OMAP4_USBB2_HSIC_STROBE_WD_SHIFT);
	omap_writel(reg_val, OMAP4_CTRL_MODULE_PAD_CORE +
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_USBB_HSIC);
	gpio_request(GPIO_HW_REV4, "HW_REV4");

	/* GPIO_HW_REV5 - 169 */
	omap_writew(OMAP_MUX_MODE3,
		    OMAP4_CTRL_MODULE_PAD_CORE_MUX_PBASE +
		    OMAP4_CTRL_MODULE_PAD_USBB2_HSIC_DATA_OFFSET);
	reg_val = omap_readl(OMAP4_CTRL_MODULE_PAD_CORE +
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_USBB_HSIC);
	reg_val &= ~(0x3 << OMAP4_USBB2_HSIC_DATA_WD_SHIFT);
	omap_writel(reg_val, OMAP4_CTRL_MODULE_PAD_CORE +
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_USBB_HSIC);
	gpio_request(GPIO_HW_REV5, "HW_REV5");

	id = gpio_get_value(GPIO_HW_REV4);
	id |= gpio_get_value(GPIO_HW_REV5) << 1;

	board_type = types[id];
}

unsigned int __init omap4_kona_get_board_type(void)
{
	return board_type;
}

static void kona_power_off_charger(void)
{
	pr_err("Rebooting into bootloader for charger.\n");
	arm_pm_restart('t', NULL);
}

static unsigned int gpio_ta_nconnected;

static int kona_reboot_call(struct notifier_block *this,
				unsigned long code, void *cmd)
{
	if (code == SYS_POWER_OFF && !gpio_get_value(gpio_ta_nconnected))
		pm_power_off = kona_power_off_charger;

	return 0;
}

static struct notifier_block kona_reboot_notifier = {
	.notifier_call = kona_reboot_call,
};

static void __init omap4_kona_reboot_init(void)
{
	gpio_ta_nconnected = omap_muxtbl_get_gpio_by_name("TA_nCONNECTED");

	if (unlikely(gpio_ta_nconnected != -EINVAL))
		register_reboot_notifier(&kona_reboot_notifier);
}

static void __init kona_init_machine(void)
{
	sec_common_init_early();
	omap4_kona_update_board_type();

	omap4_kona_emif_init();
	sec_muxtbl_init(SEC_MACHINE_KONA, system_rev);

	/* initialize sec common infrastructures */
	sec_common_init();
	sec_debug_init_crash_key(NULL);

	/* initialize each drivers */
	omap4_kona_serial_init();
	omap4_kona_pmic_init();
	omap4_kona_sdio_init();
#ifdef CONFIG_ION_OMAP
	omap4_register_ion();
#endif
	platform_add_devices(kona_devices, ARRAY_SIZE(kona_devices));
	omap_dmm_init();
	usb_musb_init(&musb_board_data);
	omap4_kona_connector_init();
	omap4_kona_charger_init();
	omap4_kona_display_init();
	omap4_kona_reboot_init();
	omap4_kona_input_init();
	omap4_kona_audio_init();
	omap4_kona_wifi_init();
	omap4_kona_sensors_init();
	omap4_kona_irled_init();
	omap4_kona_vibrator_init();

#ifdef CONFIG_OMAP_HSI_DEVICE
	/* Allow HSI omap_device to be registered later */
	omap_hsi_allow_registration();
#endif

	if (sec_debug_get_level())
		platform_add_devices(kona_dbg_devices,
				     ARRAY_SIZE(kona_dbg_devices));

	sec_common_init_post();
}

static void __init kona_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();

	sec_getlog_supply_meminfo(KONA_MEM_BANK_0_SIZE,
				  KONA_MEM_BANK_0_ADDR,
				  KONA_MEM_BANK_1_SIZE,
				  KONA_MEM_BANK_1_ADDR);
}

static void omap4_kona_init_carveout_sizes(
		struct omap_ion_platform_data *ion)
{
	ion->tiler1d_size = (SZ_1M * 90);
	/*
	 * REVIST:
	 * wfdhdcp_size = SZ_16M; will be adopted once
	 * ducati side carveout wfd section is up.
	 */
	ion->secure_output_wfdhdcp_size = 0;
	ion->ducati_heap_size = (SZ_1M * 105);
	ion->nonsecure_tiler2d_size = (SZ_1M * 19);
	ion->tiler2d_size = (SZ_1M * 81);
}

static void __init kona_reserve(void)
{
#ifdef CONFIG_ION_OMAP
	omap_init_ram_size();
	omap4_kona_memory_display_init();
	omap4_kona_init_carveout_sizes(get_omap_ion_platform_data());
	omap_ion_init();
#endif

	/* do the static reservations first */
	if (sec_debug_get_level()) {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
		memblock_remove(KONA_RAMCONSOLE_START,
				KONA_RAMCONSOLE_SIZE);
#endif
#if defined(CONFIG_RAMOOPS)
		memblock_remove(KONA_RAMOOPS_START,
				KONA_RAMOOPS_SIZE);
#endif
	}
	memblock_remove(PHYS_ADDR_SMC_MEM, PHYS_ADDR_SMC_SIZE);
	memblock_remove(PHYS_ADDR_DUCATI_MEM, PHYS_ADDR_DUCATI_SIZE);

	/* ipu needs to recognize secure input buffer area as well */
	omap_ipu_set_static_mempool(PHYS_ADDR_DUCATI_MEM,
				    PHYS_ADDR_DUCATI_SIZE +
				    OMAP4_ION_HEAP_SECURE_INPUT_SIZE +
				    OMAP4_ION_HEAP_SECURE_OUTPUT_WFDHDCP_SIZE);
	omap_reserve();
}

MACHINE_START(OMAP4_SAMSUNG, "Kona")
	/* Maintainer: Samsung Electronics Co, Ltd. */
	.boot_params	= 0x80000100,
	.reserve	= kona_reserve,
	.map_io		= kona_map_io,
	.init_early	= kona_init_early,
	.init_irq	= gic_init_irq,
	.init_machine	= kona_init_machine,
	.timer		= &omap_timer,
MACHINE_END

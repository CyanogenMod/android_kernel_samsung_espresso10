/* arch/arm/mach-omap2/board-t1.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-tuna.c
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
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/ion.h>
#include <linux/memblock.h>
#include <linux/omap_ion.h>
#include <linux/platform_data/ram_console.h>
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
#include <mach/omap4_ion.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "board-t1.h"
#include "control.h"
#include "mux.h"
#include "omap4-sar-layout.h"
#include "omap_muxtbl.h"

#include "sec_common.h"
#include "sec_debug.h"
#include "sec_getlog.h"
#include "sec_muxtbl.h"

#define T1_MEM_BANK_0_SIZE	0x20000000
#define T1_MEM_BANK_0_ADDR	0x80000000
#define T1_MEM_BANK_1_SIZE	0x20000000
#define T1_MEM_BANK_1_ADDR	0xA0000000

#define T1_RAMCONSOLE_START	(PLAT_PHYS_OFFSET + SZ_512M)
#define T1_RAMCONSOLE_SIZE	SZ_2M
#define T1_RAMOOPS_START	(T1_RAMCONSOLE_START + T1_RAMCONSOLE_SIZE)
#define T1_RAMOOPS_SIZE		SZ_1M

#define UART_NUM_FOR_GPS	0

#if defined(CONFIG_ANDROID_RAM_CONSOLE)
static struct resource ramconsole_resources[] = {
	{
		.flags  = IORESOURCE_MEM,
		.start	= T1_RAMCONSOLE_START,
		.end	= T1_RAMCONSOLE_START + T1_RAMCONSOLE_SIZE - 1,
	},
};

static struct ram_console_platform_data ramconsole_pdata;

static struct platform_device ramconsole_device = {
	.name           = "ram_console",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(ramconsole_resources),
	.resource       = ramconsole_resources,
	.dev		= {
		.platform_data = &ramconsole_pdata,
	},
};
#endif /* CONFIG_ANDROID_RAM_CONSOLE */

static struct ramoops_platform_data ramoops_pdata = {
	.mem_size	= T1_RAMOOPS_SIZE,
	.mem_address	= T1_RAMOOPS_START,
	.record_size	= SZ_32K,
	.dump_oops	= 0,	/* only for panic */
};

static struct platform_device ramoops_device = {
	.name		= "ramoops",
	.dev		= {
		.platform_data	= &ramoops_pdata,
	},
};

static struct platform_device bcm4330_bluetooth_device = {
	.name = "bcm4330_bluetooth",
	.id = -1,
};

static struct platform_device t1_mcasp_device = {
	.name		= "omap-mcasp-dai",
	.id		= 0,
};

static struct platform_device t1_spdif_dit_device = {
	.name		= "spdif-dit",
	.id		= 0,
};

static struct platform_device *t1_dbg_devices[] __initdata = {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
	&ramconsole_device,
#endif
	&ramoops_device,
};

static struct platform_device *t1_devices[] __initdata = {
	&bcm4330_bluetooth_device,
	&t1_mcasp_device,
	&t1_spdif_dit_device,
};

static unsigned int t1_crash_keycode[] = {
	KEY_HOME,
	KEY_VOLUMEUP,
};

static struct sec_crash_key t1_crash_key = {
	.keycode	= t1_crash_keycode,
	.size		= ARRAY_SIZE(t1_crash_keycode),
};

static void __init t1_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
	omap4_t1_display_early_init();
	omap4_t1_serial_early_init();
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_UTMI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode	= MUSB_OTG,
#elif defined(CONFIG_USB_MUSB_HDRC_HCD)
	.mode	= MUSB_HOST,
#elif defined(CONFIG_USB_GADGET_MUSB_HDRC)
	.mode	= MUSB_PERIPHERAL,
#endif
	.power	= 500,
};

static void t1_power_off_charger(void)
{
	pr_err("Rebooting into bootloader for charger.\n");
	arm_pm_restart('t', NULL);
}

static unsigned int gpio_ta_nconnected;

static int t1_reboot_call(struct notifier_block *this,
				unsigned long code, void *cmd)
{
	if (code == SYS_POWER_OFF && !gpio_get_value(gpio_ta_nconnected))
		pm_power_off = t1_power_off_charger;

	return 0;
}

static struct notifier_block t1_reboot_notifier = {
	.notifier_call = t1_reboot_call,
};

static void __init omap4_t1_reboot_init(void)
{
	gpio_ta_nconnected = omap_muxtbl_get_gpio_by_name("TA_nCONNECTED");

	if (unlikely(gpio_ta_nconnected != -EINVAL))
		register_reboot_notifier(&t1_reboot_notifier);
}


static void __init t1_init(void)
{
	sec_common_init_early();

	omap4_t1_emif_init();
	sec_muxtbl_init(SEC_MACHINE_T1, system_rev);

	/* initialize sec common infrastructures */
	sec_common_init();
	sec_debug_init_crash_key(&t1_crash_key);

	/* initialize each drivers */
	omap4_t1_serial_init();
	omap4_t1_pmic_init();
	omap4_register_ion();
	platform_add_devices(t1_devices, ARRAY_SIZE(t1_devices));
	omap4_t1_sdio_init();
	usb_musb_init(&musb_board_data);
	omap_dmm_init();
	omap4_t1_display_init();
	omap4_t1_connector_init();
	omap4_t1_input_init();
	omap4_t1_sensors_init();
	omap4_t1_power_init();
	omap4_t1_jack_init();
	omap4_t1_wifi_init();
	omap4_t1_vibrator_init();
	omap4_t1_fmradio_init();
	omap4_t1_reboot_init();
	omap4_t1_cam_init();
#ifdef CONFIG_OMAP_HSI_DEVICE
	/* Allow HSI omap_device to be registered later */
	omap_hsi_allow_registration();
#endif

	if (sec_debug_get_level())
		platform_add_devices(t1_dbg_devices,
				     ARRAY_SIZE(t1_dbg_devices));
	sec_common_init_post();
}

static void __init t1_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();

	sec_getlog_supply_meminfo(T1_MEM_BANK_0_SIZE, T1_MEM_BANK_0_ADDR,
				  T1_MEM_BANK_1_SIZE, T1_MEM_BANK_1_ADDR);
}

static void omap4_t1_init_carveout_sizes(
		struct omap_ion_platform_data *ion)
{
	ion->tiler1d_size = (SZ_1M * 90);
	/* WFD is not supported in T1 So the size is zero */
	ion->secure_output_wfdhdcp_size = 0;
	ion->ducati_heap_size = (SZ_1M * 105);
	ion->nonsecure_tiler2d_size = (SZ_1M * 15);
	ion->tiler2d_size = (SZ_1M * 81);
}

static void __init t1_reserve(void)
{
	omap_init_ram_size();
	omap4_t1_display_memory_init();
	omap4_t1_init_carveout_sizes(get_omap_ion_platform_data());
	omap_ion_init();

	/* do the static reservations first */
	if (sec_debug_get_level()) {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
		memblock_remove(T1_RAMCONSOLE_START, T1_RAMCONSOLE_SIZE);
#endif
#if defined(CONFIG_RAMOOPS)
		memblock_remove(T1_RAMOOPS_START, T1_RAMOOPS_SIZE);
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

MACHINE_START(OMAP4_SAMSUNG, "T1 Samsung board")
	/* Maintainer: Shankar Bandal Samsung India (shankar.b@samsung.com) */
	.boot_params	= 0x80000100,
	.reserve	= t1_reserve,
	.map_io		= t1_map_io,
	.init_early	= t1_init_early,
	.init_irq	= gic_init_irq,
	.init_machine	= t1_init,
	.timer		= &omap_timer,
MACHINE_END

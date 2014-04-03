/* arch/arm/mach-omap2/board-superior.c
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

#include "board-superior.h"
#include "control.h"
#include "mux.h"
#include "omap4-sar-layout.h"
#include "omap_muxtbl.h"

#include "sec_common.h"
#include "sec_debug.h"
#include "sec_getlog.h"
#include "sec_muxtbl.h"

#define SUPERIOR_MEM_BANK_0_SIZE		0x20000000
#define SUPERIOR_MEM_BANK_0_ADDR		0x80000000
#define SUPERIOR_MEM_BANK_1_SIZE		0x20000000
#define SUPERIOR_MEM_BANK_1_ADDR		0xA0000000

#define SUPERIOR_RAMCONSOLE_START		(PLAT_PHYS_OFFSET + SZ_512M)
#define SUPERIOR_RAMCONSOLE_SIZE		SZ_2M

#define SUPERIOR_RAMOOPS_START		(SUPERIOR_RAMCONSOLE_START + \
					 SUPERIOR_RAMCONSOLE_SIZE)
#define SUPERIOR_RAMOOPS_SIZE		SZ_1M

#if defined(CONFIG_ANDROID_RAM_CONSOLE)
static struct resource ramconsole_resources[] = {
	{
		.flags	= IORESOURCE_MEM,
		.start	= SUPERIOR_RAMCONSOLE_START,
		.end	= SUPERIOR_RAMCONSOLE_START
			+ SUPERIOR_RAMCONSOLE_SIZE - 1,
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
	.mem_size	= SUPERIOR_RAMOOPS_SIZE,
	.mem_address	= SUPERIOR_RAMOOPS_START,
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

static struct platform_device *superior_dbg_devices[] __initdata = {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
	&ramconsole_device,
#endif
	&ramoops_device,
};

static struct platform_device *superior_devices[] __initdata = {
	&bcm4334_bluetooth_device,
};

static void __init superior_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
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

static void superior_power_off_charger(void)
{
	pr_err("Rebooting into bootloader for charger.\n");
	arm_pm_restart('t', NULL);
}

static unsigned int gpio_ta_nconnected;

static int superior_reboot_call(struct notifier_block *this,
				unsigned long code, void *cmd)
{
	if (code == SYS_POWER_OFF && !gpio_get_value(gpio_ta_nconnected))
		pm_power_off = superior_power_off_charger;

	return 0;
}

static struct notifier_block superior_reboot_notifier = {
	.notifier_call = superior_reboot_call,
};

static void __init omap4_superior_reboot_init(void)
{
	gpio_ta_nconnected = omap_muxtbl_get_gpio_by_name("TA_nCONNECTED");

	if (unlikely(gpio_ta_nconnected != -EINVAL))
		register_reboot_notifier(&superior_reboot_notifier);
}

static void __init superior_init_machine(void)
{
	sec_common_init_early();

	omap4_superior_emif_init();
	sec_muxtbl_init(SEC_MACHINE_SUPERIOR, system_rev);

	/* initialize sec common infrastructures */
	sec_common_init();
	sec_debug_init_crash_key(NULL);

	/* initialize each drivers */
	omap4_superior_serial_init();
	omap4_superior_pmic_init();
	omap4_superior_sdio_init();
#ifdef CONFIG_ION_OMAP
	omap4_register_ion();
#endif
	platform_add_devices(superior_devices, ARRAY_SIZE(superior_devices));
	omap_dmm_init();
	usb_musb_init(&musb_board_data);
	omap4_superior_connector_init();
	omap4_superior_reboot_init();
	omap4_superior_input_init();
	omap4_superior_audio_init();
	omap4_superior_display_init();
	omap4_superior_wifi_init();
	omap4_superior_charger_init();
	omap4_superior_vibrator_init();
	omap4_superior_sensors_init();
	omap4_superior_nfc_init();
	omap4_superior_fmradio_init();
	omap4_superior_camera_init();
	omap4_superior_spiflash_init();

#ifdef CONFIG_OMAP_HSI_DEVICE
	/* Allow HSI omap_device to be registered later */
	omap_hsi_allow_registration();
#endif

	if (sec_debug_get_level())
		platform_add_devices(superior_dbg_devices,
				     ARRAY_SIZE(superior_dbg_devices));

	sec_common_init_post();
}

static void __init superior_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();

	sec_getlog_supply_meminfo(SUPERIOR_MEM_BANK_0_SIZE,
				  SUPERIOR_MEM_BANK_0_ADDR,
				  SUPERIOR_MEM_BANK_1_SIZE,
				  SUPERIOR_MEM_BANK_1_ADDR);
}

static void omap4_superior_init_carveout_sizes(
		struct omap_ion_platform_data *ion)
{
	ion->tiler1d_size = (SZ_1M * 90);
	/*
	 * REVIST:
	 * wfdhdcp_size = SZ_16M; will be adopted once
	 * ducati side carveout wfd section is up.
	 */
	ion->secure_output_wfdhdcp_size = 0;
	ion->ducati_heap_size = (SZ_1M * 128);
	ion->nonsecure_tiler2d_size = (SZ_1M * 15);
	ion->tiler2d_size = (SZ_1M * 81);
}

static void __init superior_reserve(void)
{
#ifdef CONFIG_ION_OMAP
	omap_init_ram_size();
	omap4_superior_memory_display_init();
	omap4_superior_init_carveout_sizes(get_omap_ion_platform_data());
	omap_ion_init();
#endif
	/* do the static reservations first */
	if (sec_debug_get_level()) {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
		memblock_remove(SUPERIOR_RAMCONSOLE_START,
				SUPERIOR_RAMCONSOLE_SIZE);
#endif
#if defined(CONFIG_RAMOOPS)
		memblock_remove(SUPERIOR_RAMOOPS_START,
				SUPERIOR_RAMOOPS_SIZE);
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

MACHINE_START(OMAP4_SAMSUNG, "Superior")
	/* Maintainer: Samsung Electronics Co, Ltd. */
	.boot_params	= 0x80000100,
	.reserve	= superior_reserve,
	.map_io		= superior_map_io,
	.init_early	= superior_init_early,
	.init_irq	= gic_init_irq,
	.init_machine	= superior_init_machine,
	.timer		= &omap_timer,
MACHINE_END

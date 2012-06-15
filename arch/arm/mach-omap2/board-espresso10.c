/* arch/arm/mach-omap2/board-espresso10.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-espresso.c
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

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "board-espresso10.h"
#include "control.h"
#include "mux.h"
#include "omap4-sar-layout.h"
#include "omap_muxtbl.h"

#include "sec_common.h"
#include "sec_debug.h"
#include "sec_getlog.h"
#include "sec_muxtbl.h"

/* gpio to distinguish WiFi and USA-BBY
 *
 * HW_REV4 | HIGH | LOW
 * --------+------+------
 *         |IrDA O|IrDA X
 */
#define GPIO_HW_REV4		41

#define ESPRESSO10_MEM_BANK_0_SIZE	0x20000000
#define ESPRESSO10_MEM_BANK_0_ADDR	0x80000000
#define ESPRESSO10_MEM_BANK_1_SIZE	0x20000000
#define ESPRESSO10_MEM_BANK_1_ADDR	0xA0000000

#define ESPRESSO10_RAMCONSOLE_START	(PLAT_PHYS_OFFSET + SZ_512M)
#define ESPRESSO10_RAMCONSOLE_SIZE	SZ_2M
#define ESPRESSO10_RAMOOPS_START	(ESPRESSO10_RAMCONSOLE_START + \
					 ESPRESSO10_RAMCONSOLE_SIZE)
#define ESPRESSO10_RAMOOPS_SIZE		SZ_1M

static struct resource ramconsole_resources[] = {
	{
		.flags	= IORESOURCE_MEM,
		.start	= ESPRESSO10_RAMCONSOLE_START,
		.end	= ESPRESSO10_RAMCONSOLE_START
			+ ESPRESSO10_RAMCONSOLE_SIZE - 1,
	 },
};

static struct platform_device ramconsole_device = {
	.name		= "ram_console",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ramconsole_resources),
	.resource	= ramconsole_resources,
};

static struct ramoops_platform_data ramoops_pdata = {
	.mem_size	= ESPRESSO10_RAMOOPS_SIZE,
	.mem_address	= ESPRESSO10_RAMOOPS_START,
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
	.name		= "bcm4330_bluetooth",
	.id		= -1,
};

#define PHYS_ADDR_SMC_SIZE	(SZ_1M * 3)
#define PHYS_ADDR_DUCATI_SIZE	(SZ_1M * 105)
#define OMAP4_ION_HEAP_SECURE_INPUT_SIZE	(SZ_1M * 90)
#define OMAP4_ION_HEAP_TILER_SIZE		(SZ_1M * 77)
#define OMAP4_ION_HEAP_NONSECURE_TILER_SIZE	(SZ_1M * 19)

#define PHYS_ADDR_SMC_MEM	(0x80000000 + SZ_1G - PHYS_ADDR_SMC_SIZE)
#define PHYS_ADDR_DUCATI_MEM	(PHYS_ADDR_SMC_MEM - \
				 PHYS_ADDR_DUCATI_SIZE - \
				 OMAP4_ION_HEAP_SECURE_INPUT_SIZE)

static struct ion_platform_data omap4_ion_data = {
	.nr	= 3,
	.heaps = {
		{
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.id	= OMAP_ION_HEAP_SECURE_INPUT,
			.name	= "secure_input",
			.base	= PHYS_ADDR_SMC_MEM
				- OMAP4_ION_HEAP_SECURE_INPUT_SIZE,
			.size	= OMAP4_ION_HEAP_SECURE_INPUT_SIZE,
		},
		{
			.type	= OMAP_ION_HEAP_TYPE_TILER,
			.id	= OMAP_ION_HEAP_TILER,
			.name	= "tiler",
			.base	= PHYS_ADDR_DUCATI_MEM
				- OMAP4_ION_HEAP_TILER_SIZE,
			.size	= OMAP4_ION_HEAP_TILER_SIZE,
		},
		{
			.type	= OMAP_ION_HEAP_TYPE_TILER,
			.id	= OMAP_ION_HEAP_NONSECURE_TILER,
			.name	= "nonsecure_tiler",
			.base	= PHYS_ADDR_DUCATI_MEM -
					OMAP4_ION_HEAP_TILER_SIZE -
					OMAP4_ION_HEAP_NONSECURE_TILER_SIZE,
			.size = OMAP4_ION_HEAP_NONSECURE_TILER_SIZE,
		},
	},
};

static struct platform_device omap4_ion_device = {
	.name	= "ion-omap4",
	.id	= -1,
	.dev	= {
		.platform_data	= &omap4_ion_data,
	},
};

static struct platform_device *espresso10_dbg_devices[] __initdata = {
	&ramconsole_device,
	&ramoops_device,
};

static struct platform_device *espresso10_devices[] __initdata = {
	&omap4_ion_device,
	&bcm4330_bluetooth_device,
};

static void __init espresso10_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);

	omap4_espresso10_display_early_init();
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

#define CARRIER_WIFI_ONLY	"wifi-only"

static unsigned int board_type = SEC_MACHINE_ESPRESSO10;

static int __init espresso10_set_board_type(char *str)
{
	if (!strncmp(str, CARRIER_WIFI_ONLY, strlen(CARRIER_WIFI_ONLY)))
		board_type = SEC_MACHINE_ESPRESSO10_WIFI;

	return 0;
}
__setup("androidboot.carrier=", espresso10_set_board_type);

static void __init omap4_espresso10_update_board_type(void)
{
	const unsigned int gpio_hw_rev4 = GPIO_HW_REV4;

	if (system_rev < 6)
		return;

	/* because omap4_mux_init is not called when this function is
	 * called, padconf reg must be configured by low-level function. */
	omap_writew(OMAP_MUX_MODE3 | OMAP_PIN_INPUT,
		    OMAP4_CTRL_MODULE_PAD_CORE_MUX_PBASE +
		    OMAP4_CTRL_MODULE_PAD_GPMC_A17_OFFSET);

	gpio_request(gpio_hw_rev4, "HW_REV4");
	if (gpio_get_value(gpio_hw_rev4))
		board_type = SEC_MACHINE_ESPRESSO10_USA_BBY;
}

unsigned int __init omap4_espresso10_get_board_type(void)
{
	return board_type;
}

static void espresso10_power_off_charger(void)
{
	pr_err("Rebooting into bootloader for charger.\n");
	arm_pm_restart('t', NULL);
}

static unsigned int gpio_ta_nconnected;

static int espresso10_reboot_call(struct notifier_block *this,
				unsigned long code, void *cmd)
{
	if (code == SYS_POWER_OFF && !gpio_get_value(gpio_ta_nconnected))
		pm_power_off = espresso10_power_off_charger;

	return 0;
}

static struct notifier_block espresso10_reboot_notifier = {
	.notifier_call = espresso10_reboot_call,
};

static void __init omap4_espresso10_reboot_init(void)
{
	gpio_ta_nconnected = omap_muxtbl_get_gpio_by_name("TA_nCONNECTED");

	if (unlikely(gpio_ta_nconnected != -EINVAL))
		register_reboot_notifier(&espresso10_reboot_notifier);
}

static void __init espresso10_init(void)
{
	sec_common_init_early();
	omap4_espresso10_update_board_type();

	omap4_espresso10_emif_init();
	if (board_type == SEC_MACHINE_ESPRESSO10_USA_BBY &&
	    system_rev >= 7)
		sec_muxtbl_init(SEC_MACHINE_ESPRESSO10_USA_BBY, system_rev);
	sec_muxtbl_init(SEC_MACHINE_ESPRESSO10, system_rev);

	/* initialize sec common infrastructures */
	sec_common_init();
	sec_debug_init_crash_key(NULL);

	/* initialize each drivers */
	omap4_espresso10_serial_init();
	omap4_espresso10_charger_init();
	omap4_espresso10_pmic_init();
	platform_add_devices(espresso10_devices,
			     ARRAY_SIZE(espresso10_devices));
	omap_dmm_init();
	omap4_espresso10_sdio_init();
	usb_musb_init(&musb_board_data);
	omap4_espresso10_connector_init();
	omap4_espresso10_wifi_init();
	omap4_espresso10_display_init();
	omap4_espresso10_input_init();
	omap4_espresso10_sensors_init();
	omap4_espresso10_jack_init();
	omap4_espresso10_camera_init();
	omap4_espresso10_reboot_init();
	omap4_espresso10_none_modem_init();

#ifdef CONFIG_OMAP_HSI_DEVICE
	/* Allow HSI omap_device to be registered later */
	omap_hsi_allow_registration();
#endif

	if (sec_debug_get_level())
		platform_add_devices(espresso10_dbg_devices,
				     ARRAY_SIZE(espresso10_dbg_devices));

	sec_common_init_post();
}

static void __init espresso10_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();

	sec_getlog_supply_meminfo(ESPRESSO10_MEM_BANK_0_SIZE,
				  ESPRESSO10_MEM_BANK_0_ADDR,
				  ESPRESSO10_MEM_BANK_1_SIZE,
				  ESPRESSO10_MEM_BANK_1_ADDR);
}

static void __init espresso10_reserve(void)
{
	int i;
	int ret;

	/* do the static reservations first */
	if (sec_debug_get_level()) {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
		memblock_remove(ESPRESSO10_RAMCONSOLE_START,
				ESPRESSO10_RAMCONSOLE_SIZE);
#endif
#if defined(CONFIG_RAMOOPS)
		memblock_remove(ESPRESSO10_RAMOOPS_START,
				ESPRESSO10_RAMOOPS_SIZE);
#endif
	}

	memblock_remove(PHYS_ADDR_SMC_MEM, PHYS_ADDR_SMC_SIZE);
	memblock_remove(PHYS_ADDR_DUCATI_MEM, PHYS_ADDR_DUCATI_SIZE);

	for (i = 0; i < omap4_ion_data.nr; i++)
		if (omap4_ion_data.heaps[i].type == ION_HEAP_TYPE_CARVEOUT ||
		    omap4_ion_data.heaps[i].type == OMAP_ION_HEAP_TYPE_TILER) {
			ret = memblock_remove(omap4_ion_data.heaps[i].base,
					      omap4_ion_data.heaps[i].size);
			if (ret)
				pr_err("memblock remove of %x@%lx failed\n",
				       omap4_ion_data.heaps[i].size,
				       omap4_ion_data.heaps[i].base);
		}

	/* ipu needs to recognize secure input buffer area as well */
	omap_ipu_set_static_mempool(PHYS_ADDR_DUCATI_MEM,
				    PHYS_ADDR_DUCATI_SIZE +
				    OMAP4_ION_HEAP_SECURE_INPUT_SIZE);
	omap_reserve();
}

MACHINE_START(OMAP4_SAMSUNG, "Espresso10")
	/* Maintainer: Samsung Electronics Co, Ltd. */
	.boot_params	= 0x80000100,
	.reserve	= espresso10_reserve,
	.map_io		= espresso10_map_io,
	.init_early	= espresso10_init_early,
	.init_irq	= gic_init_irq,
	.init_machine	= espresso10_init,
	.timer		= &omap_timer,
MACHINE_END

/* arch/arm/mach-omap2/board-gokey.c
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

#include <mach/dmm.h>
#include <mach/omap4-common.h>
#include <mach/id.h>
#ifdef CONFIG_ION_OMAP
#include <mach/omap4_ion.h>
#endif

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "board-gokey.h"
#include "control.h"
#include "mux.h"
#include "omap4-sar-layout.h"
#include "omap_muxtbl.h"

#include "sec_common.h"
#include "sec_debug.h"
#include "sec_getlog.h"
#include "sec_muxtbl.h"
#include "sec_log_buf.h"
#include <linux/module.h>
#include <linux/platform_device.h>
#ifdef CONFIG_MP3_LP_MODE
#include <linux/earlysuspend.h>
#include <mach/cpufreq_limits.h>
#include <linux/cpu.h>
#include <linux/usb/otg.h>
#include "pm.h"
#define PM_LPMODE_DVFS_FREQ	300000
#endif

#define GOKEY_MEM_BANK_0_SIZE	0x20000000
#define GOKEY_MEM_BANK_0_ADDR	0x80000000
#define GOKEY_MEM_BANK_1_SIZE	0x20000000
#define GOKEY_MEM_BANK_1_ADDR	0xA0000000

#define GOKEY_RAMCONSOLE_START	(PLAT_PHYS_OFFSET + SZ_512M)
#define GOKEY_RAMCONSOLE_SIZE	SZ_2M
#define GOKEY_RAMOOPS_START		(GOKEY_RAMCONSOLE_START + \
					 GOKEY_RAMCONSOLE_SIZE)
#define GOKEY_RAMOOPS_SIZE		SZ_1M

#ifdef CONFIG_MP3_LP_MODE
struct cpufreq_lpmode_info cpufreq_lpmode;
#endif

static struct resource ramconsole_resources[] = {
	{
	 .flags = IORESOURCE_MEM,
	 .start = GOKEY_RAMCONSOLE_START,
	 .end = GOKEY_RAMCONSOLE_START + GOKEY_RAMCONSOLE_SIZE - 1,
	 },
};

static struct platform_device ramconsole_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources = ARRAY_SIZE(ramconsole_resources),
	.resource = ramconsole_resources,
};

static struct ramoops_platform_data ramoops_pdata = {
	.mem_size = GOKEY_RAMOOPS_SIZE,
	.mem_address = GOKEY_RAMOOPS_START,
	.record_size = SZ_32K,
	.dump_oops = 0,		/* only for panic */
};

static struct platform_device ramoops_device = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ramoops_pdata,
		},
};

static struct platform_device bcm4334_bluetooth_device = {
	.name = "bcm4334_bluetooth",
	.id = -1,
};

static struct platform_device omap4_gokey_pm_device = {
	.name = "omap4_gokey_pm",
	.id = -1,
};
static struct platform_device *gokey_dbg_devices[] __initdata = {
	&ramconsole_device,
	&ramoops_device,
};

static struct platform_device *gokey_devices[] __initdata = {
	&bcm4334_bluetooth_device,
	&omap4_gokey_pm_device,
};
static void omap4_gokey_early_init(void)
{
	struct omap_hwmod *uart4_hwmod;

	/* correct uart4 hwmod flag settings for gokey board. */
	uart4_hwmod = omap_hwmod_lookup("uart4");
	if (likely(uart4_hwmod))
		uart4_hwmod->flags = HWMOD_SWSUP_SIDLE;

}

static struct cpuidle_params omap443x_gokey_normal_params_table[] = {
	/* C1 - CPUx WFI + MPU ON  + CORE ON */
	{.exit_latency = 2 + 2,	.target_residency = 5, .valid = 1},
	/* C2 - CPU0 INA + CPU1 INA + MPU INA  + CORE INA */
	{.exit_latency = 1100, .target_residency = 1100, .valid = 1},
	/* C3 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE CSWR */
	{.exit_latency = 1200, .target_residency = 1200, .valid = 1},
#ifdef CONFIG_OMAP_ALLOW_OSWR
	/* C4 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE OSWR */
	{.exit_latency = 1500, .target_residency = 1500, .valid = 1},
#else
	{.exit_latency = 1500, .target_residency = 1500, .valid = 0},
#endif
};

static struct cpuidle_params omap443x_gokey_mp3_lpmode_params_table[] = {
	/* C1 - CPUx WFI + MPU ON  + CORE ON */
	{.exit_latency = 2 + 2,	.target_residency = 4, .valid = 1},
	/* C2 - CPU0 INA + CPU1 INA + MPU INA  + CORE INA */
	{.exit_latency = 300, .target_residency = 300, .valid = 1},
	/* C3 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE CSWR */
	{.exit_latency = 1000, .target_residency = 10000, .valid = 1},
#ifdef CONFIG_OMAP_ALLOW_OSWR
	/* C4 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE OSWR */
	{.exit_latency = 1200, .target_residency = 35000, .valid = 1},
#else
	{.exit_latency = 1200, .target_residency = 35000, .valid = 0},
#endif
};

DECLARE_PER_CPU(struct cpuidle_device, omap4_idle_dev);

void update_cpuidle_params(void)
{
	int cpu_id = 0, i, count = 0;
	struct omap4_processor_cx *cx;
	struct cpuidle_state *state;
	struct cpuidle_device *dev;
	for_each_possible_cpu(cpu_id) {
		dev = &per_cpu(omap4_idle_dev, cpu_id);
		dev->cpu = cpu_id;
		count = 0;
		for (i = OMAP4_STATE_C1; i < OMAP4_MAX_STATES; i++) {
			cx = &omap4_power_states[i];
			state = &dev->states[count];
			if (!cx->valid)
				continue;
			cpuidle_set_statedata(state, cx);
			state->exit_latency = cx->exit_latency;
			state->target_residency = cx->target_residency;
			count++;
		}
	}
}

struct delayed_work gokey_init_delay_work;

static int __devexit omap4_gokey_pm_remove(struct platform_device *pdev)
{
	return 0;
}
static void gokey_init_param_work(struct work_struct *work)
{
	omap4_init_power_states(omap443x_gokey_normal_params_table);
	update_cpuidle_params();
}
static int __devinit omap4_gokey_pm_probe(struct platform_device *pdev)
{

	INIT_DELAYED_WORK(&gokey_init_delay_work, gokey_init_param_work);

	schedule_delayed_work(&gokey_init_delay_work, 500);
	return 0;
}

static int omap4_gokey_pm_suspend(struct platform_device *pdev,
				    pm_message_t state)
{
	return 0;
}

static int omap4_gokey_pm_resume(struct platform_device *pdev,
				    pm_message_t state)
{
	return 0;
}

static void omap4_gokey_pm_shutdown(struct platform_device *pdev,
				    pm_message_t state)
{
	msleep(3000);
}

static const struct dev_pm_ops omap4_gokey_pm_ops = {
	.suspend    = omap4_gokey_pm_suspend,
	.resume     = omap4_gokey_pm_resume,
};
static struct platform_driver omap4_gokey_pm_driver = {
	.probe = omap4_gokey_pm_probe,
	.remove = omap4_gokey_pm_remove,
	.shutdown = omap4_gokey_pm_shutdown,
	.driver = {
		.name = "omap4_gokey_pm",
		.pm   = &omap4_gokey_pm_ops,
	},
};
static int __init omap4_gokey_pm_init(void)
{
	return platform_driver_register(&omap4_gokey_pm_driver);
}
module_init(omap4_gokey_pm_init);

static void __init gokey_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);

	omap4_gokey_display_early_init();
	omap4_gokey_early_init();
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type = MUSB_INTERFACE_UTMI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode = MUSB_OTG,
#else
	.mode = MUSB_PERIPHERAL,
#endif
	.power = 200,
};

#ifdef CONFIG_MP3_LP_MODE
static void board_gokey_early_suspend(struct early_suspend *h)
{

	struct omap_hwmod *uart4_hwmod;
	struct omap_hwmod *mcbsp3_hwmod;

	/* correct uart4 hwmod flag settings for gokey board. */
	uart4_hwmod = omap_hwmod_lookup("uart4");
	if (likely(uart4_hwmod))
		uart4_hwmod->flags = HWMOD_SWSUP_SIDLE;

	if (!cpufreq_lpmode.wifi_enabled  && !cpufreq_lpmode.bt_enabled
		&& is_playback_lpmode_available()
		&& (gokey_get_charging_type() != USB_EVENT_VBUS)) {
		cpu_down(1);
		omap4_init_power_states(omap443x_gokey_mp3_lpmode_params_table);
		update_cpuidle_params();
		cpufreq_lpmode.lp_mode_enabled = true;
		omap_cpufreq_max_limit(DVFS_LOCK_ID_PM, PM_LPMODE_DVFS_FREQ);
	} else
		cpufreq_lpmode.lp_mode_enabled = false;
	if (!is_playback_lpmode_available()) {
		/* correct mcbsp3 hwmod flag settings for gokey board. */
		mcbsp3_hwmod = omap_hwmod_lookup("mcbsp3");
		if (likely(mcbsp3_hwmod))
			mcbsp3_hwmod->flags = HWMOD_SWSUP_SIDLE;
	}
}

static void board_gokey_late_resume(struct early_suspend *h)
{

	struct omap_hwmod *uart4_hwmod;
	struct omap_hwmod *mcbsp3_hwmod;

	if (cpufreq_lpmode.lp_mode_enabled) {

		omap4_init_power_states(omap443x_gokey_normal_params_table);
		update_cpuidle_params();
		if (!cpu_online(1))
			cpu_up(1);
		omap_cpufreq_max_limit_free(DVFS_LOCK_ID_PM);
		cpufreq_lpmode.lp_mode_enabled = false;
	}
	/* correct uart4 hwmod flag settings for gokey board. */
	uart4_hwmod = omap_hwmod_lookup("uart4");
	if (likely(uart4_hwmod))
		uart4_hwmod->flags = 0;

	/* correct mcbsp3 hwmod flag settings for gokey board. */
	mcbsp3_hwmod = omap_hwmod_lookup("mcbsp3");
	if (likely(mcbsp3_hwmod))
		mcbsp3_hwmod->flags = 0;

}

static struct early_suspend board_gokey_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 40,
	.suspend = board_gokey_early_suspend,
	.resume = board_gokey_late_resume,
};
#endif
static void __init gokey_init(void)
{

#ifdef CONFIG_MP3_LP_MODE
	register_early_suspend(&board_gokey_early_suspend_handler);
#endif

	sec_common_init_early();

	omap4_gokey_emif_init();
	sec_muxtbl_init(SEC_MACHINE_GOKEY, system_rev);

	/* initialize sec common infrastructures */
	sec_common_init();
	sec_debug_init_crash_key(NULL);

	/* initialize each drivers */
	omap4_gokey_serial_init();
	omap4_gokey_charger_init();
	omap4_gokey_pmic_init();
	omap4_gokey_audio_init();
#ifdef CONFIG_ION_OMAP
	omap4_register_ion();
#endif
	platform_add_devices(gokey_devices, ARRAY_SIZE(gokey_devices));
	omap_dmm_init();
	omap4_gokey_sdio_init();
	usb_musb_init(&musb_board_data);
	omap4_gokey_connector_init();
	omap4_gokey_display_init();
	omap4_gokey_input_init();
	omap4_gokey_wifi_init();
	omap4_gokey_sensors_init();
	omap4_gokey_camera_init();

	if (sec_debug_get_level())
		platform_add_devices(gokey_dbg_devices,
				     ARRAY_SIZE(gokey_dbg_devices));

	sec_common_init_post();
}

static void __init gokey_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();

	sec_getlog_supply_meminfo(GOKEY_MEM_BANK_0_SIZE,
				  GOKEY_MEM_BANK_0_ADDR,
				  GOKEY_MEM_BANK_1_SIZE,
				  GOKEY_MEM_BANK_1_ADDR);
}

static void omap4_gokey_init_carveout_sizes(struct omap_ion_platform_data
					       *ion)
{
	ion->tiler1d_size = (SZ_1M * 14);
	/* WFD is not supported in gokey So the size is zero */
	ion->secure_output_wfdhdcp_size = 0;
	ion->ducati_heap_size = (SZ_1M * 65);
	ion->nonsecure_tiler2d_size = (SZ_1M * 8);
	ion->tiler2d_size = (SZ_1M * 81);
}

static void __init gokey_reserve(void)
{

#ifdef CONFIG_ION_OMAP
	omap_init_ram_size();
	omap4_gokey_memory_display_init();
	omap4_gokey_init_carveout_sizes(get_omap_ion_platform_data());
	omap_ion_init();
#endif
	/* do the static reservations first */
	if (sec_debug_get_level()) {
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
		memblock_remove(GOKEY_RAMCONSOLE_START,
				GOKEY_RAMCONSOLE_SIZE);
#endif
#if defined(CONFIG_RAMOOPS)
		memblock_remove(GOKEY_RAMOOPS_START, GOKEY_RAMOOPS_SIZE);
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

	sec_log_buf_reserve();
}

MACHINE_START(OMAP4_SAMSUNG, "gokey")
	/* Maintainer: Samsung Electronics Co, Ltd. */
	.boot_params = 0x80000100,
	.reserve = gokey_reserve,
	.map_io = gokey_map_io,
	.init_early = gokey_init_early,
	.init_irq = gic_init_irq,
	.init_machine = gokey_init,
	.timer = &omap_timer,
MACHINE_END

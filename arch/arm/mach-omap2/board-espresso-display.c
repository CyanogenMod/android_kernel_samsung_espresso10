/* arch/arm/mach-omap2/board-espresso-display.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-tuna-display.c
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

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/omapfb.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/panel-ltn070nl01.h>

#include <plat/vram.h>
#include <plat/omap_hwmod.h>
#include <plat/android-display.h>

#include <video/omapdss.h>
#include <video/omap-panel-generic-dpi.h>

#include "board-espresso.h"
#include "control.h"
#include "mux.h"
#include "omap_muxtbl.h"

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include <plat/clock.h>
#include <linux/clk.h>
#endif

#define ESPRESSO_FB_RAM_SIZE		SZ_16M	/* ~1280*720*4 * 2 */

static struct ltn070nl01_panel_data espresso_panel_data;
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
static struct clk *dss_ick, *dss_sys_fclk, *dss_dss_fclk;
#endif


static void espresso_lcd_set_power(bool enable)
{
	pr_debug("(%s): espresso_lcd_set_power, enable=%d\n",
		 __func__, enable);

	gpio_set_value(espresso_panel_data.lcd_en_gpio, enable);

}
static void espresso_lcd_set_gptimer_idle(void)
{
	struct omap_hwmod *timer10_hwmod;
	pr_debug("espresso_lcd_set_gptimer_idle\n");

	timer10_hwmod = omap_hwmod_lookup("timer10");
	if (likely(timer10_hwmod))
		omap_hwmod_idle(timer10_hwmod);
}



#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
static void dss_clks_disable(void)
{
	clk_disable(dss_ick);
	clk_disable(dss_dss_fclk);
	clk_disable(dss_sys_fclk);
}
#endif
static struct omap_dss_device espresso_lcd_device = {
	.name			= "lcd",
	.driver_name		= "ltn070nl01_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	.data			= &espresso_panel_data,
#if 0
	.clocks = {
		.dispc = {
			.channel = {
				.lck_div = 1,	/* LCD */
				.pck_div = 2,	/* PCD */
				.lcd_clk_src = OMAP_DSS_CLK_SRC_FCK,
			},
		.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
	},
},
#endif
	.channel		= OMAP_DSS_CHANNEL_LCD2,
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init		= true,
	.dss_clks_disable	= dss_clks_disable,

#else
	.skip_init		= false,
#endif
	.panel = {
		.timings	= {
			.x_res		= 1024,
			.y_res		= 600,
			.pixel_clock	= 56000,
			.hfp		= 186,
			.hsw		= 50,
			.hbp		= 210,
			.vfp		= 24,
			.vsw		= 10,
			.vbp		= 11,
		},
		.width_in_um	= 153600,
		.height_in_um	= 90000,
	},

};

static struct omap_dss_device *espresso_dss_devices[] = {
	&espresso_lcd_device,
};

static struct omap_dss_board_info espresso_dss_data = {
	.num_devices	= ARRAY_SIZE(espresso_dss_devices),
	.devices	= espresso_dss_devices,
	.default_device	= &espresso_lcd_device,
};

static struct omapfb_platform_data espresso_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = ESPRESSO_FB_RAM_SIZE,
			},
		},
	},
};

void __init omap4_espresso_display_early_init(void)
{
	struct omap_hwmod *timer10_hwmod;
	struct omap_hwmod *gpio_hwmod;
	char *gpio_no_reset[] = {
		"gpio3",
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
		"gpio4",
#else
		"gpio5",
#endif
	};
	unsigned int i;

	/* correct timer10 hwmod flag settings for espresso board. */
	timer10_hwmod = omap_hwmod_lookup("timer10");
	if (likely(timer10_hwmod))
		timer10_hwmod->flags =
			(HWMOD_INIT_NO_IDLE | HWMOD_INIT_NO_RESET);

	/* correct gpio hwmod flag settings for espresso board. */
	for (i = 0; i < ARRAY_SIZE(gpio_no_reset); i++) {
		gpio_hwmod = omap_hwmod_lookup(gpio_no_reset[i]);
		if (likely(gpio_hwmod))
			gpio_hwmod->flags = HWMOD_INIT_NO_RESET;
	}
}

static __init int setup_current_panel(char *opt)
{
	return kstrtoint(opt, 0, &espresso_panel_data.panel_id);
}
__setup("lcd_panel_id=", setup_current_panel);

void __init omap4_espresso_memory_display_init(void)
{
	omap_android_display_setup(&espresso_dss_data,
				   NULL,
				   NULL,
				   &espresso_fb_pdata,
				   get_omap_ion_platform_data());
}

void __init omap4_espresso_display_init(void)
{
	struct ltn070nl01_panel_data *panel;
	int ret, i;
	/* Default setting vlaue for BOE panel*/
	int platform_brightness[] = {
		BRIGHTNESS_OFF, BRIGHTNESS_DIM, BRIGHTNESS_MIN,
		BRIGHTNESS_25, BRIGHTNESS_DEFAULT, BRIGHTNESS_MAX};
	int kernel_brightness[] = {0, 1, 3, 8, 35, 94};

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	dss_ick = clk_get(NULL, "ick");
	if (IS_ERR(dss_ick)) {
		pr_err("Could not get dss interface clock\n");
		/* return -ENOENT; */
	 }

	dss_sys_fclk = omap_clk_get_by_name("dss_sys_clk");
	if (IS_ERR(dss_sys_fclk)) {
		pr_err("Could not get dss system clock\n");
		/* return -ENOENT; */
	}
	clk_enable(dss_sys_fclk);
	dss_dss_fclk = omap_clk_get_by_name("dss_dss_clk");
	if (IS_ERR(dss_dss_fclk)) {
		pr_err("Could not get dss functional clock\n");
		/* return -ENOENT; */
	 }
#endif

	if (espresso_panel_data.panel_id == PANEL_LCD) {
		kernel_brightness[2] = 2;
		kernel_brightness[3] = 7;
		kernel_brightness[4] = 30;
		kernel_brightness[5] = 80;
	}

	espresso_panel_data.lvds_nshdn_gpio =
	    omap_muxtbl_get_gpio_by_name("LVDS_nSHDN");
	espresso_panel_data.lcd_en_gpio =
	    omap_muxtbl_get_gpio_by_name("LCD_EN");
	espresso_panel_data.led_backlight_reset_gpio =
	    omap_muxtbl_get_gpio_by_name("LED_BACKLIGHT_RESET");
	espresso_panel_data.backlight_gptimer_num = 10;
	espresso_panel_data.set_power = espresso_lcd_set_power;
	espresso_panel_data.set_gptimer_idle = espresso_lcd_set_gptimer_idle;

	for (i = 0 ; i < NUM_BRIGHTNESS_LEVEL ; i++) {
		espresso_panel_data.brightness_table.platform_value[i] =
			platform_brightness[i];
		espresso_panel_data.brightness_table.kernel_value[i] =
			kernel_brightness[i];
	}

	ret = gpio_request(espresso_panel_data.lcd_en_gpio, "lcd_en");
	if (ret < 0) {
		pr_err("(%s): gpio_request %d failed!\n", __func__,
		       espresso_panel_data.lcd_en_gpio);
		/*goto err; */
	}
	gpio_direction_output(espresso_panel_data.lcd_en_gpio, 1);

	panel = &espresso_panel_data;

	espresso_lcd_device.data = panel;

	omap_display_init(&espresso_dss_data);
}

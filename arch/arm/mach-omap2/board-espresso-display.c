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
#include <linux/platform_data/panel-ltn.h>

#include <plat/omap_hwmod.h>
#include <plat/android-display.h>

#include <video/omapdss.h>

#include "board-espresso.h"

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include <plat/clock.h>
#include <linux/clk.h>
#endif

#define GPIO_LED_BACKLIGHT_RESET	95
#define GPIO_LCD_EN			135
#define GPIO_LVDS_NSHDN		136

static void espresso_lcd_set_power(bool enable)
{
	pr_debug("%s: %d\n", __func__, enable);
	gpio_set_value(GPIO_LCD_EN, enable);
}

static void espresso_lcd_set_gptimer_idle(void)
{
	struct omap_hwmod *timer10_hwmod;
	pr_debug("espresso_lcd_set_gptimer_idle\n");

	timer10_hwmod = omap_hwmod_lookup("timer10");
	if (likely(timer10_hwmod))
		omap_hwmod_idle(timer10_hwmod);
}

static struct ltn_panel_data espresso_panel_data = {
	.set_power			= espresso_lcd_set_power,
	.set_gptimer_idle		= espresso_lcd_set_gptimer_idle,
	.lvds_nshdn_gpio		= GPIO_LVDS_NSHDN,
	.led_backlight_reset_gpio	= GPIO_LED_BACKLIGHT_RESET,
	.backlight_gptimer_num		= 10,
	.brightness_table = {
		.platform_value	= {BRIGHTNESS_OFF, BRIGHTNESS_DIM, BRIGHTNESS_MIN,
				BRIGHTNESS_25, BRIGHTNESS_DEFAULT, BRIGHTNESS_MAX},
		.kernel_value	= { 0, 2, 3, 8, 40, 85 },
	},
};

static struct omap_dss_device espresso_lcd_device = {
	.name			= "lcd",
	.driver_name		= "ltn_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	.data			= &espresso_panel_data,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init		= true,
#else
	.skip_init		= false,
#endif
	.panel = {
		.timings	= {
			.x_res		= 1024,
			.y_res		= 600,
			.pixel_clock	= 56888, /* closest to 56000 */
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

static struct omap_dss_device espresso10_lcd_config = {
	.panel = {
		.timings	= {
			.x_res		= 1280,
			.y_res		= 800,
			.pixel_clock	= 69818, /* closest to 69000 */
			.hfp		= 16,
			.hsw		= 48,
			.hbp		= 64,
			.vfp		= 16,
			.vsw		= 3,
			.vbp		= 11,
		},
		.width_in_um	= 216960,
		.height_in_um	= 135600,
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
	},
};

void __init omap4_espresso_memory_display_init(void)
{
	if (board_is_espresso10())
		espresso_dss_data.devices[0]->panel = espresso10_lcd_config.panel;

	omap_android_display_setup(&espresso_dss_data,
				   NULL,
				   NULL,
				   &espresso_fb_pdata,
				   get_omap_ion_platform_data());
}

void __init omap4_espresso_display_early_init(void)
{
	struct omap_hwmod *timer10_hwmod;
	struct omap_hwmod *gpio3_hwmod;
	struct omap_hwmod *gpio5_hwmod;

	/* correct timer10 hwmod flag settings for espresso board. */
	timer10_hwmod = omap_hwmod_lookup("timer10");
	if (likely(timer10_hwmod))
		timer10_hwmod->flags =
			(HWMOD_INIT_NO_IDLE | HWMOD_INIT_NO_RESET);

	/* correct gpio3 hwmod flag settings for espresso board. */
	gpio3_hwmod = omap_hwmod_lookup("gpio3");
	if (likely(gpio3_hwmod))
		gpio3_hwmod->flags = HWMOD_INIT_NO_RESET;

	/* correct gpio5 hwmod flag settings for espresso board. */
	gpio5_hwmod = omap_hwmod_lookup("gpio5");
	if (likely(gpio5_hwmod))
		gpio5_hwmod->flags = HWMOD_INIT_NO_RESET;
}

void __init omap4_espresso_display_init(void)
{
	int ret;
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	struct clk *dss_dss_fclk;
#endif

	if (board_is_espresso10())
		espresso_panel_data.pwm_duty_max = 1600; /* 25kHz */
	else
		espresso_panel_data.pwm_duty_max = 1200; /* 32kHz */

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	dss_dss_fclk = omap_clk_get_by_name("dss_dss_clk");
	if (IS_ERR(dss_dss_fclk)) {
		pr_err("Could not get dss functional clock\n");
		/* return -ENOENT; */
	 }
	 clk_enable(dss_dss_fclk);
#endif

	ret = gpio_request(GPIO_LCD_EN, "lcd_en");
	if (ret < 0)
		pr_err("%s: gpio_request %d failed!\n", __func__, GPIO_LCD_EN);

	gpio_direction_output(GPIO_LCD_EN, 1);

	omap_display_init(&espresso_dss_data);
}

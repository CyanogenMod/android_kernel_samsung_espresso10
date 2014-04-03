/* arch/arm/mach-omap2/board-kona-display.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
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

#include <video/cmc624.h>

#include <linux/platform_data/lp855x.h>

#include <plat/vram.h>
#include <plat/omap_hwmod.h>
#include <plat/android-display.h>

#include <video/omapdss.h>

#include "board-kona.h"
#include "board-kona-display-cmc624.h"
#include "control.h"
#include "mux.h"
#include "omap_muxtbl.h"

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include  <linux/clk.h>
#endif

#define KONA_FB_RAM_SIZE		SZ_16M

#define KONA_BRIGHTNESS_DEFAULT	0x80
#define KONA_CMC624_PWM_DEFAULT_INTENSITY	130

#define MAX_CMC624_PWM_COUNTER	0x07FF	/* PWM_COUNTER field has 11 bit */
#define KONA_MAX_PWM_CNT	0x04E2
#define KONA_DEF_PWM_CNT	0x027B
#define KONA_MIN_PWM_CNT	0x000C

#define GET_PWR_LUT(_org, _ratio) \
	((_org) * (_ratio) / 10000 + (((_org) * (_ratio) % 10000) ? 1 : 0))

/*
 * GPIO
 */
enum {
	GPIO_LCD_EN = 0,
	GPIO_LED_BACKLIGHT_EN,
	GPIO_IMA_SLEEP,
	GPIO_IMA_NRST,
	GPIO_IMA_CMC_EN,
};

static struct gpio display_gpios[] = {
	[GPIO_LCD_EN]	= {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "LCD_EN",
	},
	[GPIO_LED_BACKLIGHT_EN]	= {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "LED_BACKLIGHT_EN",
	},
	[GPIO_IMA_SLEEP]	= {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "IMA_SLEEP",
	},
	[GPIO_IMA_NRST]	= {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "IMA_nRST",
	},
	[GPIO_IMA_CMC_EN]	= {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "IMA_CMC_EN",
	},
};

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include <plat/clock.h>
static struct clk *dss_ick, *dss_sys_fclk, *dss_dss_fclk;
#endif

static void kona_lcd_set_power(bool enable)
{
	gpio_set_value(display_gpios[GPIO_LCD_EN].gpio, enable);
}

/* Defined clk_disable for disabling the interface clock
 * (dss_fck) and functional clock (dss_dss_fclk) in the suspend path */
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
static void dss_clks_disable(void)
{
	clk_disable(dss_ick);
	clk_disable(dss_dss_fclk);
}
#endif

static int kona_init_tune_list(void)
{
	u32 id;

	/* init command */
	TUNE_DATA_ID(id, MENU_CMD_INIT, MENU_SKIP, MENU_SKIP, MENU_SKIP,
					MENU_SKIP, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, cmc624_init, ARRAY_SIZE(cmc624_init));

	/* Dynamic UI CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_UI,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_ui_cabcoff,
					ARRAY_SIZE(tune_dynamic_ui_cabcoff));

	/* Dynamic UI CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_UI,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_ui_cabcon,
					ARRAY_SIZE(tune_dynamic_ui_cabcon));

	/* Dynamic Video CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VIDEO,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_video_cabcoff,
					ARRAY_SIZE(tune_dynamic_video_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VIDEO_WARM,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_video_cabcoff,
					ARRAY_SIZE(tune_dynamic_video_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VIDEO_COLD,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_video_cabcoff,
					ARRAY_SIZE(tune_dynamic_video_cabcoff));

	/* Dynamic Video CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VIDEO,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_video_cabcon,
					ARRAY_SIZE(tune_dynamic_video_cabcon));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VIDEO_WARM,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_video_cabcon,
					ARRAY_SIZE(tune_dynamic_video_cabcon));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VIDEO_COLD,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_video_cabcon,
					ARRAY_SIZE(tune_dynamic_video_cabcon));

	/* Dynamic Gallery CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_GALLERY,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_gallery_cabcoff,
				ARRAY_SIZE(tune_dynamic_gallery_cabcoff));

	/* Dynamic Gallery CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_GALLERY,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_gallery_cabcon,
				ARRAY_SIZE(tune_dynamic_gallery_cabcon));

	/* Dynamic VT-call CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VT,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_vtcall_cabcoff,
				ARRAY_SIZE(tune_dynamic_vtcall_cabcoff));

	/* Dynamic VT-call CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VT,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_dynamic_vtcall_cabcon,
				ARRAY_SIZE(tune_dynamic_vtcall_cabcon));

	/* Standard UI CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_UI,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_ui_cabcoff,
				ARRAY_SIZE(tune_standard_ui_cabcoff));

	/* Standard UI CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_UI,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_ui_cabcon,
				ARRAY_SIZE(tune_standard_ui_cabcon));

	/* Standard Video CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VIDEO,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_video_cabcoff,
				ARRAY_SIZE(tune_standard_video_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VIDEO_WARM,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_video_cabcoff,
				ARRAY_SIZE(tune_standard_video_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VIDEO_COLD,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_video_cabcoff,
				ARRAY_SIZE(tune_standard_video_cabcoff));

	/* Standard Video CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VIDEO,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_video_cabcon,
				ARRAY_SIZE(tune_standard_video_cabcon));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VIDEO_WARM,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_video_cabcon,
				ARRAY_SIZE(tune_standard_video_cabcon));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VIDEO_COLD,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_video_cabcon,
				ARRAY_SIZE(tune_standard_video_cabcon));

	/* Standard Gallery CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_GALLERY,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_gallery_cabcoff,
				ARRAY_SIZE(tune_standard_gallery_cabcoff));

	/* Standard Gallery CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_GALLERY,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_gallery_cabcon,
				ARRAY_SIZE(tune_standard_gallery_cabcon));

	/* Standard VT-call CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VT,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_vtcall_cabcoff,
				ARRAY_SIZE(tune_standard_vtcall_cabcoff));

	/* Standard VT-call CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VT,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_standard_vtcall_cabcon,
				ARRAY_SIZE(tune_standard_vtcall_cabcon));

	/* Movie UI CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_UI,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_movie_ui_cabcoff,
				ARRAY_SIZE(tune_movie_ui_cabcoff));

	/* Movie UI CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_UI,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_movie_ui_cabcon,
				ARRAY_SIZE(tune_movie_ui_cabcon));

	/* Movie Video CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_VIDEO,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_movie_video_cabcoff,
				ARRAY_SIZE(tune_movie_video_cabcoff));

	/* Movie Video CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_VIDEO,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_movie_video_cabcon,
				ARRAY_SIZE(tune_movie_video_cabcon));

	/* Movie Gallery CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_GALLERY,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_movie_gallery_cabcoff,
				ARRAY_SIZE(tune_movie_gallery_cabcoff));

	/* Movie Gallery CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_GALLERY,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_movie_gallery_cabcon,
				ARRAY_SIZE(tune_movie_gallery_cabcon));

	/* Movie VT-call CABC-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_VT,
			MENU_SKIP, MENU_SKIP, MENU_CABC_OFF, MENU_SKIP);
	cmc624_register_tune_data(id, tune_movie_vtcall_cabcoff,
				ARRAY_SIZE(tune_movie_vtcall_cabcoff));

	/* Movie VT-call CABC-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_VT,
			MENU_SKIP, MENU_SKIP, MENU_CABC_ON, MENU_SKIP);
	cmc624_register_tune_data(id, tune_movie_vtcall_cabcon,
				ARRAY_SIZE(tune_movie_vtcall_cabcon));

	/* Camera Outdoor-off (only CABC-off) */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_APP_CAMERA,
			MENU_SKIP, MENU_OUT_OFF, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_camera_cabcoff,
				ARRAY_SIZE(tune_camera_cabcoff));

	/* Camera Outdoor-on (only CABC-off) */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_APP_CAMERA,
			MENU_SKIP, MENU_OUT_ON, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_camera_outdoor_cabcoff,
				ARRAY_SIZE(tune_camera_outdoor_cabcoff));

	/* Temperature-Normal Outdoor-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_SKIP,
			MENU_TEMP_NORMAL, MENU_OUT_OFF, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_bypass,
				ARRAY_SIZE(tune_bypass));

	/* Temperature-Normal Outdoor-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_SKIP,
			MENU_TEMP_NORMAL, MENU_OUT_ON, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_normal_outdoor,
				ARRAY_SIZE(tune_normal_outdoor));

	/* Temperature-Cold Outdoor-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_SKIP,
			MENU_TEMP_COLD, MENU_OUT_OFF, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_cold,
				ARRAY_SIZE(tune_cold));

	/* Temperature-Cold Outdoor-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_SKIP,
			MENU_TEMP_COLD, MENU_OUT_ON, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_cold_outdoor,
				ARRAY_SIZE(tune_cold_outdoor));

	/* Temperature-Warm Outdoor-off */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_SKIP,
			MENU_TEMP_WARM, MENU_OUT_OFF, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_warm,
				ARRAY_SIZE(tune_warm));

	/* Temperature-Warm Outdoor-on */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_SKIP,
			MENU_TEMP_WARM, MENU_OUT_ON, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_warm_outdoor,
				ARRAY_SIZE(tune_warm_outdoor));

	/* Negative */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_NEGATIVE, MENU_SKIP,
			MENU_SKIP, MENU_SKIP, MENU_SKIP, MENU_SKIP);
	cmc624_register_tune_data(id, tune_negative,
				ARRAY_SIZE(tune_negative));
	return 0;
}

static int kona_power_vima_1_1V(int on)
{
	struct regulator *reg_smpsn8;
	int ret;

	reg_smpsn8 = regulator_get(NULL, "V_IMA_1.1V");
	if (IS_ERR(reg_smpsn8)) {
		pr_err("%s: failed to get v1.1 regulator.\n", __func__);
		return -ENODATA;
	}

	if (on)
		ret = regulator_enable(reg_smpsn8);
	else
		ret = regulator_disable(reg_smpsn8);

	regulator_put(reg_smpsn8);

	if (ret < 0) {
		pr_err("%s: failed to %s v1.1 regulator.\n",
				__func__, on ? "enable" : "disable");
		return ret;
	}

	return 0;
}

static int kona_power_vima_1_8V(int on)
{
	struct regulator *reg_ldon27;
	int ret;

	reg_ldon27 = regulator_get(NULL, "V_IMA_1.8V");
	if (IS_ERR(reg_ldon27)) {
		pr_err("%s: failed to get v1.8 regulator.\n", __func__);
		return -ENODATA;
	}

	if (on)
		ret = regulator_enable(reg_ldon27);
	else
		ret = regulator_disable(reg_ldon27);

	regulator_put(reg_ldon27);

	if (ret < 0) {
		pr_err("%s: failed to %s v1.8 regulator.\n",
				__func__, on ? "enable" : "disable");
		return ret;
	}

	return 0;
}

static struct cabcoff_pwm_cnt_tbl kona_pwm_tbl = {
	.max	= KONA_MAX_PWM_CNT,
	.def	= KONA_DEF_PWM_CNT,
	.min	= KONA_MIN_PWM_CNT,
};

static struct cabcon_pwr_lut_tbl kona_pwr_luts;

static struct cmc624_panel_data kona_panel_data_cmc624 = {
	.lcd_name		= "NOVATEK_NT51012",
	.power_lcd		= kona_lcd_set_power,
	.init_tune_list		= kona_init_tune_list,
	.power_vima_1_1V	= kona_power_vima_1_1V,
	.power_vima_1_8V	= kona_power_vima_1_8V,

	.pwm_tbl		= &kona_pwm_tbl,
	.pwr_luts		= &kona_pwr_luts,
};

/* cmc624 i2c board info */
static struct i2c_board_info __initdata kona_i2c7_boardinfo[] = {
	{
		I2C_BOARD_INFO("sec_cmc624_i2c", 0x38),
	},
};


/* POWERLUT_xx values for CABC mode PWM control */
static u16 cmc624_pwr_luts[MAX_PWRLUT_LEV][MAX_PWRLUT_MODE][NUM_PWRLUT_REG] = {
	{ /* Indoor power look up table, 72% */
		/* UI scenario */
		{0x547, 0x4E1, 0x4F5, 0x5C2, 0x547, 0x50A, 0x4A3, 0x0CD},
		/* Video scenario */
		{0x47A, 0x4E1, 0x428, 0x5C2, 0x47A, 0x43D, 0x3D7, 0x0CD},
	},
	{ /* Outdoor power look up table for outdoor 1 (1k~15k), 82% */
		{0x547, 0x5AD, 0x4F5, 0x68F, 0x547, 0x50A, 0x4A3, 0x0CD},
		{0x47A, 0x4E1, 0x428, 0x5C2, 0x47A, 0x43D, 0x3D7, 0x0CD},
	},
	{ /* Outdoor power look up table (15k ~), 100% */
		{0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF},
		{0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF},
	},
};

static void kona_init_cmc624_pwr_luts(void)
{
	int ratio_max;
	int ratio_def;
	int ratio_min;
	int i;
	int j;
	int k;

	ratio_max = kona_pwm_tbl.max * 10000 / MAX_CMC624_PWM_COUNTER;
	ratio_def = kona_pwm_tbl.def * 10000 / MAX_CMC624_PWM_COUNTER;
	ratio_min = kona_pwm_tbl.min * 10000 / MAX_CMC624_PWM_COUNTER;

	pr_info("%s: ratio: max=%d, def=%d, min=%d\n", __func__,
					ratio_max, ratio_def, ratio_min);

	for (i = 0; i < MAX_PWRLUT_LEV; i++) {
		for (j = 0; j < MAX_PWRLUT_MODE; j++) {
			for (k = 0; k < NUM_PWRLUT_REG; k++) {
				kona_pwr_luts.max[i][j][k] = GET_PWR_LUT(
					cmc624_pwr_luts[i][j][k], ratio_max);
				kona_pwr_luts.def[i][j][k] = GET_PWR_LUT(
					cmc624_pwr_luts[i][j][k], ratio_def);
				kona_pwr_luts.min[i][j][k] = GET_PWR_LUT(
					cmc624_pwr_luts[i][j][k], ratio_min);
			}
		}
	}
}

static struct omap_dss_device kona_lcd_device = {
	.name			= "lcd",
	.driver_name		= "sec_cmc624",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.phy.dsi		= {
		.type		= OMAP_DSS_DSI_TYPE_VIDEO_MODE,
		.clk_lane	= 1,
		.clk_pol	= 0,
		.data1_lane	= 2,
		.data1_pol	= 0,
		.data2_lane	= 3,
		.data2_pol	= 0,
		.data3_lane	= 4,
		.data3_pol	= 0,
		.data4_lane	= 5,
		.data4_pol	= 0,
		.line_bufs	= 2,
	},
	.panel = {
		.timings        = {
			.x_res		= 1280,
			.y_res		= 800,
			.pixel_clock	= 64800,
			.hfp		= 17,
			.hsw		= 18,
			.hbp		= 17,
			.vfp		= 3,
			.vsw		= 6,
			.vbp		= 3,
		},
		.acbi		= 0,
		.acb		= 40,
		.width_in_um	= 153600,
		.height_in_um	= 90000,
	},
	.clocks = {
		.dispc = {
			.channel = {
				.lck_div = 1,	/* LCD */
				.pck_div = 2,	/* PCD */
				.lcd_clk_src =
					OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},
		.dsi = {
			.regn		= 16,   /* DSI_PLL_REGN */
			.regm		= 162,  /* DSI_PLL_REGM */

			.regm_dispc	= 6,	/* PLL_CLK1 (M4) */
			.regm_dsi	= 5,    /* PLL_CLK2 (M5) */
			.lp_clk_div	= 18,	/* LPDIV */
			.offset_ddr_clk	= 122,  /* DDR PRE & DDR POST
						 * offset increase
						 */

			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
			.tlpx	= 14,
			.tclk	= {
				.zero	 = 65,
				.prepare = 17,
				.trail   = 17,
			},
			.ths = {
				.zero    = 26,
				.prepare = 20,
				.exit    = 36,
				.trail   = 20,
			},
		},
	},

	.channel		= OMAP_DSS_CHANNEL_LCD,
	.ctrl.pixel_size	= 24,

	.data			= &kona_panel_data_cmc624,

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init				= true,
	.dss_clks_disable                       = dss_clks_disable,
#else
	.skip_init				= false,
#endif
};

#ifdef CONFIG_OMAP4_DSS_HDMI
static void __init kona_hdmi_mux_init(void)
{
	u32 r;
	/* strong pullup on DDC lines using unpublished register */
	r = OMAP4_HDMI_DDC_SCL_PULLUPRESX_MASK
			| OMAP4_HDMI_DDC_SDA_PULLUPRESX_MASK;
	omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_1);
}

static struct omap_dss_device kona_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.clocks = {
		.dispc  = {
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},
		.hdmi = {
			.regn = 15,
			.regm2 = 1,
			.max_pixclk_khz = 75000,
		},
	},
	.channel = OMAP_DSS_CHANNEL_DIGIT,
};
#endif

static struct omap_dss_device *kona_dss_devices[] = {
	&kona_lcd_device,
#ifdef CONFIG_OMAP4_DSS_HDMI
	&kona_hdmi_device,
#endif
};

static struct omap_dss_board_info kona_dss_data = {
	.num_devices	= ARRAY_SIZE(kona_dss_devices),
	.devices	= kona_dss_devices,
	.default_device	= &kona_lcd_device,
};

static struct omapfb_platform_data kona_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = KONA_FB_RAM_SIZE,
			},
		},
	},
};

/*
 * Backlight
 */
static int kona_bl_set_power(int on)
{
	if (on)
		gpio_set_value(display_gpios[GPIO_LED_BACKLIGHT_EN].gpio, 1);
	else
		gpio_set_value(display_gpios[GPIO_LED_BACKLIGHT_EN].gpio, 0);

	return 0;
}

static int kona_send_intensity(int intensity)
{
	return cmc624_set_pwm(intensity);
}

static void kona_set_auto_brt(int auto_brightness)
{
	cmc624_set_auto_brightness(auto_brightness);
}

static struct lp855x_pdata kona_bl_pdata = {
	.default_intensity	= KONA_BRIGHTNESS_DEFAULT,
	.ps_mode		= PS_MODE_4_4,
	.brt_mode		= BRT_MODE_PWM,
	.set_power		= kona_bl_set_power,
	.send_intensity		= kona_send_intensity,
	.set_auto_brt		= kona_set_auto_brt,
};

static struct i2c_board_info __initdata kona_i2c13_boardinfo[] = {
	{
		I2C_BOARD_INFO("lp8556", 0x58 >> 1),
		.platform_data	= &kona_bl_pdata,
	},
};

/*
 * no reset gpios
 *	gpio bank1: gpio13(backlight_enable), gpio19(lcd_en)
 *	gpio bank4: gpio101(IMA_SLEEP), gpio102(IMA_nRST)
 *	gpio bank5: gpio134(IMA_CMC_EN)
 */
void __init omap4_kona_display_early_init(void)
{
	struct omap_hwmod *gpio_hwmod;
	char *gpio_no_reset[] = {
			"gpio1",
			"gpio4",
			"gpio5",
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(gpio_no_reset); i++) {
		gpio_hwmod = omap_hwmod_lookup(gpio_no_reset[i]);
		if (likely(gpio_hwmod))
			gpio_hwmod->flags = HWMOD_INIT_NO_RESET;
	}
}

static void kona_display_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(display_gpios); i++)
		display_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(display_gpios[i].label);

	gpio_request_array(display_gpios, ARRAY_SIZE(display_gpios));

#ifdef CONFIG_OMAP4_DSS_HDMI
	kona_hdmi_device.hpd_gpio =
		omap_muxtbl_get_gpio_by_name("HDMI_HPD");
	kona_hdmi_mux_init();
#endif
}

void __init omap4_kona_memory_display_init(void)
{
	omap_android_display_setup(&kona_dss_data,
				   NULL,
				   NULL,
				   &kona_fb_pdata,
				   get_omap_ion_platform_data());
}


void __init omap4_kona_display_init(void)
{
	/* Removed ENABLE_ON_INIT flag for dss_sys_clk(functional clock)
	 * in arch/arm/mach-omap2/clock44xx_data.c, manually
	 * enabling the functional clock by getting dss_sys_fclk.
	 * NOTE: It will be disabled, during disable path.
	 */

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	dss_ick = clk_get(NULL, "ick");
	if (IS_ERR(dss_ick)) {
		pr_err("Could not get dss interface clock\n");
		return;
	}
	dss_sys_fclk = omap_clk_get_by_name("dss_sys_clk");
	if (IS_ERR(dss_sys_fclk)) {
		pr_err("Could not get dss system clock\n");
		return;
	}
	clk_enable(dss_sys_fclk);
	dss_dss_fclk = omap_clk_get_by_name("dss_dss_clk");
	if (IS_ERR(dss_dss_fclk)) {
		pr_err("Could not get dss functional clock\n");
		return;
	}
#endif

	kona_display_gpio_init();

	kona_panel_data_cmc624.gpio_ima_sleep =
		display_gpios[GPIO_IMA_SLEEP].gpio;
	kona_panel_data_cmc624.gpio_ima_nrst =
		display_gpios[GPIO_IMA_NRST].gpio;
	kona_panel_data_cmc624.gpio_ima_cmc_en =
		display_gpios[GPIO_IMA_CMC_EN].gpio;

	kona_init_cmc624_pwr_luts();

	omap_display_init(&kona_dss_data);

	i2c_register_board_info(13, kona_i2c13_boardinfo,
					ARRAY_SIZE(kona_i2c13_boardinfo));

	i2c_register_board_info(7, kona_i2c7_boardinfo,
					ARRAY_SIZE(kona_i2c7_boardinfo));

	pr_info("%s: real board display\n", __func__);
}

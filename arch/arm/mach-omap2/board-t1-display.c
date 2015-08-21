/* arch/arm/mach-omap2/board-t1-display.c
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
#include <linux/lcd.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>

#include <linux/platform_data/panel-ld9040.h>

#include <mach/omap4_ion.h>

#include <plat/omap_hwmod.h>
#include <plat/vram.h>
#include <plat/mcspi.h>
#include <plat/android-display.h>

#include <video/omapdss.h>
#include <video/omap-panel-generic-dpi.h>

#include "board-t1.h"
#include "control.h"
#include "mux.h"
#include "omap_muxtbl.h"
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include <plat/clock.h>
#include <linux/clk.h>
#endif

#define T1_FB_RAM_SIZE		SZ_16M /* ~800*480*4 * 2 */

struct regulator *t1_oled_reg;
struct ld9040_panel_data t1_panel_data;
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
static struct clk *dss_ick, *dss_sys_fclk, *dss_dss_fclk;
#endif

static int lcd_power_on(struct lcd_device *ld, int enable)
{

	struct regulator *regulator;
	regulator = regulator_get(NULL, "vlcd");
	if (IS_ERR_OR_NULL(regulator)) {
		pr_err("*** %s [%d] failed to get VAUX3 regulator.\n",
				__func__, __LINE__);
		return 0;
	}

	if (enable) {
		regulator_enable(regulator);
	} else {
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		gpio_set_value(t1_panel_data.reset_gpio, 0);
	}
	regulator_put(regulator);

	return 1;
}

static int reset_lcd(struct lcd_device *ld)
{
	gpio_set_value(t1_panel_data.reset_gpio, 0);
	mdelay(1);
	gpio_set_value(t1_panel_data.reset_gpio, 1);
	return 1;
}

static struct lcd_platform_data t1_oled_data = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
	/* it indicates whether lcd panel is enabled from u-boot. */
	.lcd_enabled = 0,
	.reset_delay = 20,  /* 20ms */
	.power_on_delay = 20,   /* 20ms */
	.power_off_delay = 120, /* 120ms */
	.pdata = &t1_panel_data,
};
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
static void dss_clks_disable(void)
{
	clk_disable(dss_ick);
	clk_disable(dss_dss_fclk);
	clk_disable(dss_sys_fclk);
}
#endif
static struct omap_dss_device t1_oled_device = {
	.name			= "lcd",
	.driver_name		= "ld9040_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines = 24,
#if 0
	.clocks = {
		.dispc		= {
			.channel = {
				.lck_div	= 1,	/* LCD */
				.pck_div	= 2,	/* PCD */
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_FCK,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
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
		.timings = {
			.x_res = 480,
			.y_res = 800,
			.pixel_clock = 25600,
			.hfp = 16,
			.hsw = 2,
			.hbp = 16,
			.vfp = 10,
			.vsw = 2,
			.vbp = 4,
		},
		.acb		= 0,
		.width_in_um	= 56000,
		.height_in_um	= 93000,
	},
};

static struct omap2_mcspi_device_config ld9040_mcspi_config = {
	.turbo_mode = 0,
	.single_channel = 1,    /* 0: slave, 1: master */
};

static struct spi_board_info t1_spi_board_info[] __initdata = {
	{
		.controller_data = &ld9040_mcspi_config,
		.modalias = "ld9040",
		.bus_num = 4,
		.chip_select = 0,
		.max_speed_hz = 375000,
		.platform_data = (void *)&t1_oled_data,
	},
};

static void t1_hdmi_mux_init(void)
{
	u32 r;

	/* strong pullup on DDC lines using unpublished register */
	r = ((1 << 24) | (1 << 28)) ;
	omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_1);

}

static struct omap_dss_device t1_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.clocks	= {
		.dispc	= {
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},
		.hdmi	= {
			.regn	= 15,
			.regm2	= 1,
			.max_pixclk_khz = 75000,
		},
	},
	.channel = OMAP_DSS_CHANNEL_DIGIT,
};

static struct omap_dss_device *t1_dss_devices[] = {
	&t1_oled_device,
	&t1_hdmi_device,
};

static struct omap_dss_board_info t1_dss_data = {
	.num_devices	= ARRAY_SIZE(t1_dss_devices),
	.devices	= t1_dss_devices,
	.default_device	= &t1_oled_device,
};

static struct omapfb_platform_data t1_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = T1_FB_RAM_SIZE,
			},
		},
	},
};

static void __init omap_t1_display_gpio_init(void)
{

	t1_panel_data.reset_gpio =
		omap_muxtbl_get_gpio_by_name("MLCD_RST");

	t1_hdmi_device.hpd_gpio =
		omap_muxtbl_get_gpio_by_name("HDMI_HPD");

}

void __init omap4_t1_display_early_init(void)
{
	struct omap_hwmod *mlcd_rst_hwmod;

	/* bydefault TI release kernel is doing hwmod reset
	 * for gpio2 bank, which causing problem for MLCD RST
	 * gpio. Issue is fixed by adding NO_RESET flag for gpio2
	 * bank. */
	mlcd_rst_hwmod = omap_hwmod_lookup("gpio2");
	if (likely(mlcd_rst_hwmod))
		mlcd_rst_hwmod->flags = HWMOD_CONTROL_OPT_CLKS_IN_RESET
				      | HWMOD_INIT_NO_RESET;
}

void __init omap4_t1_display_memory_init(void)
{
	omap_android_display_setup(&t1_dss_data,
				   NULL,
				   NULL,
				   &t1_fb_pdata,
				   get_omap_ion_platform_data());
}

#define MUX_DISPLAY_OUT (OMAP_PIN_OUTPUT | OMAP_MUX_MODE5)
void __init omap4_t1_display_init(void)
{
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

	omap_t1_display_gpio_init();

	t1_hdmi_mux_init();
	spi_register_board_info(t1_spi_board_info,
			ARRAY_SIZE(t1_spi_board_info));
	omap_display_init(&t1_dss_data);
}

#define SLEEPMSEC		0x1000
#define ENDDEF			0x2000
#define	DEFMASK			0xFF00
#define COMMAND_ONLY		0xFE
#define DATA_ONLY		0xFF

static const unsigned short SEQ_INIT_DISPLAY_SETTING[] = {
	/* USER_SETTING */
	0xF0, 0x5A,
	DATA_ONLY, 0x5A,
	/* ACL ON */
	0xC1, 0x4D,
	DATA_ONLY, 0x96, DATA_ONLY, 0x1D, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x01, DATA_ONLY, 0xDF, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x03, DATA_ONLY, 0x1F, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x01,
	DATA_ONLY, 0x08, DATA_ONLY, 0x0F, DATA_ONLY, 0x16, DATA_ONLY, 0x1D,
	DATA_ONLY, 0x24, DATA_ONLY, 0x2A, DATA_ONLY, 0x31, DATA_ONLY, 0x38,
	DATA_ONLY, 0x3F, DATA_ONLY, 0x46,
	/* PANEL CONDITION */
	0xF8, 0x05,
	DATA_ONLY, 0x5E,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x6B,
	DATA_ONLY, 0x7D,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x32,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	/* DISPLAY_CONDITION */
	0xF2, 0x02,
	DATA_ONLY, 0x06,
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x10,
	/* GTCON */
	0xF7, 0x09,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	/* MANPWR */
	0xB0, 0x04,
	/* PWR_CTRL */
	0xF4, 0x0A,
	DATA_ONLY, 0x87,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x6A,
	DATA_ONLY, 0x44,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x88,
	/* ELVSS_ON */
	0xB1, 0x0D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,
	ENDDEF, 0x00
};

static const unsigned short SEQ_INIT_ETC_SETTING[] = {
	/* GAMMA_SET1 */
	0xF9, 0x00,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7B,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA4,
	/* GAMMA_CTRL */
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	/* SLEEP OUT */
	0x11, COMMAND_ONLY,
	ENDDEF, 0x00
};

static const unsigned short SEQ_USER_SETTING[] = {
	0xF0, 0x5A,

	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};
static const unsigned short SEQ_ACL_ON[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96, DATA_ONLY, 0x1D, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x01, DATA_ONLY, 0xDF, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x03, DATA_ONLY, 0x1F, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x01,
	DATA_ONLY, 0x08, DATA_ONLY, 0x0F, DATA_ONLY, 0x16, DATA_ONLY, 0x1D,
	DATA_ONLY, 0x24, DATA_ONLY, 0x2A, DATA_ONLY, 0x31, DATA_ONLY, 0x38,
	DATA_ONLY, 0x3F, DATA_ONLY, 0x46,

	0xC0, 0x01,

	ENDDEF, 0x00
};

static const unsigned short SEQ_ACL_OFF[] = {
	0xC0, 0x00,

	ENDDEF, 0x00
};


static const unsigned short SEQ_ACL_40P[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96, DATA_ONLY, 0x1D, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x01, DATA_ONLY, 0xDF, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x03, DATA_ONLY, 0x1F, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x01,
	DATA_ONLY, 0x06, DATA_ONLY, 0x11, DATA_ONLY, 0x1A, DATA_ONLY, 0x20,
	DATA_ONLY, 0x25, DATA_ONLY, 0x29, DATA_ONLY, 0x2D, DATA_ONLY, 0x30,
	DATA_ONLY, 0x33, DATA_ONLY, 0x35,

	0xC0, 0x01,

	ENDDEF, 0x00
};


static const unsigned short SEQ_ACL_43P[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96, DATA_ONLY, 0x1D, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x01, DATA_ONLY, 0xDF, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x03, DATA_ONLY, 0x1F, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x01,
	DATA_ONLY, 0x07, DATA_ONLY, 0x12, DATA_ONLY, 0x1C, DATA_ONLY, 0x23,
	DATA_ONLY, 0x29, DATA_ONLY, 0x2D, DATA_ONLY, 0x31, DATA_ONLY, 0x34,
	DATA_ONLY, 0x37, DATA_ONLY, 0x3A,

	0xC0, 0x01,

	ENDDEF, 0x00
};




static const unsigned short SEQ_ACL_45P[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96, DATA_ONLY, 0x1D, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x01, DATA_ONLY, 0xDF, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x03, DATA_ONLY, 0x1F, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x01,
	DATA_ONLY, 0x07, DATA_ONLY, 0x13, DATA_ONLY, 0x1E, DATA_ONLY, 0x25,
	DATA_ONLY, 0x2B, DATA_ONLY, 0x30, DATA_ONLY, 0x34, DATA_ONLY, 0x37,
	DATA_ONLY, 0x3A, DATA_ONLY, 0x3D,

	0xC0, 0x01,

	ENDDEF, 0x00
};


static const unsigned short SEQ_ACL_47P[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96, DATA_ONLY, 0x1D, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x01, DATA_ONLY, 0xDF, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x03, DATA_ONLY, 0x1F, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x01,
	DATA_ONLY, 0x07, DATA_ONLY, 0x14, DATA_ONLY, 0x20, DATA_ONLY, 0x28,
	DATA_ONLY, 0x2E, DATA_ONLY, 0x33, DATA_ONLY, 0x37, DATA_ONLY, 0x3B,
	DATA_ONLY, 0x3E, DATA_ONLY, 0x41,

	0xC0, 0x01,

	ENDDEF, 0x00
};

static const unsigned short SEQ_ACL_48P[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96, DATA_ONLY, 0x1D, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x01, DATA_ONLY, 0xDF, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x03, DATA_ONLY, 0x1F, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x01,
	DATA_ONLY, 0x08, DATA_ONLY, 0x15, DATA_ONLY, 0x20, DATA_ONLY, 0x29,
	DATA_ONLY, 0x2F, DATA_ONLY, 0x34, DATA_ONLY, 0x39, DATA_ONLY, 0x3D,
	DATA_ONLY, 0x40, DATA_ONLY, 0x43,

	0xC0, 0x01,

	ENDDEF, 0x00
};

static const unsigned short SEQ_ACL_50P[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96, DATA_ONLY, 0x1D, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x01, DATA_ONLY, 0xDF, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x03, DATA_ONLY, 0x1F, DATA_ONLY, 0x00, DATA_ONLY, 0x00,
	DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x00, DATA_ONLY, 0x01,
	DATA_ONLY, 0x08, DATA_ONLY, 0x16, DATA_ONLY, 0x22, DATA_ONLY, 0x2B,
	DATA_ONLY, 0x31, DATA_ONLY, 0x37, DATA_ONLY, 0x3B, DATA_ONLY, 0x3F,
	DATA_ONLY, 0x43, DATA_ONLY, 0x46,

	0xC0, 0x01,

	ENDDEF, 0x00
};

static const unsigned short *ACL_cutoff_set[] = {
	SEQ_ACL_OFF,
	SEQ_ACL_40P,
	SEQ_ACL_43P,
	SEQ_ACL_45P,
	SEQ_ACL_47P,
	SEQ_ACL_48P,
	SEQ_ACL_50P,
};

static const unsigned short SEQ_ELVSS_ON[] = {
	0xB1, 0x0F,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,
	ENDDEF, 0x00
};


static const unsigned short SEQ_ELVSS_49[] = {
	0xB2, 0x10,

	DATA_ONLY, 0x10,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x10,
	ENDDEF, 0x00
};

static const unsigned short SEQ_ELVSS_41[] = {
	0xB2, 0x17,
	DATA_ONLY, 0x17,
	DATA_ONLY, 0x17,
	DATA_ONLY, 0x17,
	ENDDEF, 0x00
};

static const unsigned short SEQ_ELVSS_39[] = {
	0xB2, 0x1A,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x1A,
	ENDDEF, 0x00
};

static const unsigned short SEQ_ELVSS_35[] = {
	0xB2, 0x1E,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x1E,
	ENDDEF, 0x00
};

static const unsigned short *SEQ_ELVSS_set[] = {
	SEQ_ELVSS_35,
	SEQ_ELVSS_39,
	SEQ_ELVSS_41,
	SEQ_ELVSS_49,
};
static const unsigned short SEQ_SM2_ELVSS_44[] = {
	0xB2, 0x15,

	DATA_ONLY, 0x15,
	DATA_ONLY, 0x15,
	DATA_ONLY, 0x15,
	ENDDEF, 0x00
};

static const unsigned short SEQ_SM2_ELVSS_37[] = {
	0xB2, 0x1C,
	DATA_ONLY, 0x1C,
	DATA_ONLY, 0x1C,
	DATA_ONLY, 0x1C,
	ENDDEF, 0x00
};

static const unsigned short SEQ_SM2_ELVSS_34[] = {
	0xB2, 0x1F,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x1F,
	ENDDEF, 0x00
};

static const unsigned short SEQ_SM2_ELVSS_30[] = {
	0xB2, 0x23,
	DATA_ONLY, 0x23,
	DATA_ONLY, 0x23,
	DATA_ONLY, 0x23,
	ENDDEF, 0x00
};
static const unsigned short *SEQ_SM2_ELVSS_set[] = {
	SEQ_SM2_ELVSS_30,
	SEQ_SM2_ELVSS_34,
	SEQ_SM2_ELVSS_37,
	SEQ_SM2_ELVSS_44,
};

static const unsigned short SEQ_GTCON[] = {
	0xF7, 0x09,

	ENDDEF, 0x00
};

static const unsigned short SEQ_PANEL_CONDITION[] = {
	0xF8, 0x05,
	DATA_ONLY, 0x5E,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x6B,
	DATA_ONLY, 0x7D,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x32,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x05,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	ENDDEF, 0x00
};

static const unsigned short SEQ_GAMMA_SET1[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x87,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA4,
	ENDDEF, 0x00
};
static const unsigned short SEQ_SM2_GAMMA_SET1[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xCE,
	ENDDEF, 0x00
};

static const unsigned short SEQ_GAMMA_CTRL[] = {
	0xFB, 0x02,

	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short SEQ_GAMMA_START[] = {
	0xF9, COMMAND_ONLY,

	ENDDEF, 0x00
};

static const unsigned short SEQ_APON[] = {
	0xF3, 0x00,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_DISPCTL[] = {
	0xF2, 0x02,

	DATA_ONLY, 0x06,
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x10,
	ENDDEF, 0x00
};


static const unsigned short SEQ_MANPWR[] = {
	0xB0, 0x04,
	ENDDEF, 0x00
};

static const unsigned short SEQ_PWR_CTRL[] = {
	0xF4, 0x0A,

	DATA_ONLY, 0x87,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x6A,
	DATA_ONLY, 0x44,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_SM2_A2_PWR_CTRL[] = {
	0xF4, 0x0A,

	DATA_ONLY, 0xA7,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x6A,
	DATA_ONLY, 0x44,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_SLPOUT[] = {
	0x11, COMMAND_ONLY,
	SLEEPMSEC, 120,
	ENDDEF, 0x00
};

static const unsigned short SEQ_SLPIN[] = {
	0x10, COMMAND_ONLY,
	ENDDEF, 0x00
};

static const unsigned short SEQ_DISPON[] = {
	0x29, COMMAND_ONLY,
	ENDDEF, 0x00
};

static const unsigned short SEQ_DISPOFF[] = {
	0x28, COMMAND_ONLY,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VCI1_1ST_EN[] = {
	0xF3, 0x10,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VL1_EN[] = {
	0xF3, 0x11,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VL2_EN[] = {
	0xF3, 0x13,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VCI1_2ND_EN[] = {
	0xF3, 0x33,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VL3_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VREG1_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0x01,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VGH_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0x11,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VGL_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0x31,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VMOS_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xB1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VINT_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xF1,
	/* DATA_ONLY, 0x71,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

static const unsigned short SEQ_VBH_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xF9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	ENDDEF, 0x00
};

static const unsigned short SEQ_VBL_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	ENDDEF, 0x00
};

static const unsigned short SEQ_GAM_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

static const unsigned short SEQ_SD_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x80,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

static const unsigned short SEQ_GLS_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x81,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

static const unsigned short SEQ_ELS_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x83,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

static const unsigned short SEQ_EL_ON[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x87,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

#define MAX_GAMMA_LEVEL		25
#define GAMMA_TABLE_COUNT		21

/* [U1] OCTA 4.27 XVGA - gamma value: 2.2 */

static const unsigned short ld9040_sm2_a2_22_300[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xCC,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_290[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0x9D,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC9,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_280[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0x9D,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC7,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_270[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC4,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_260[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0x9E,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC1,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_250[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xBD,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_240[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x98,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB9,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_230[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x95,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB6,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_220[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x93,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB3,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_210[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x90,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAF,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_200[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8C,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAB,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_190[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8A,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA8,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_180[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x87,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA4,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_170[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x83,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9C,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA0,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_160[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x7F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x98,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9B,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_150[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x7B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x93,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x97,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_140[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x78,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x93,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_130[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x74,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8E,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_120[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x70,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x87,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x89,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_110[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x6B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x9E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x82,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x83,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_100[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x66,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x9E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x7C,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x7E,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_90[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x62,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x99,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x77,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x79,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_80[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x5D,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x99,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x71,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x73,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_70[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x58,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x81,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x69,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x6A,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_60[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x52,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x80,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x62,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x63,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_50[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x4A,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x85,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x5C,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x5B,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_40[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x42,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x7E,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x53,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x53,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_22_30_dimming[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x3A,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x7E,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x49,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x49,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short *psm2_a2_22Gamma_set[] = {
	ld9040_sm2_a2_22_50,
	ld9040_sm2_a2_22_60,
	ld9040_sm2_a2_22_70,
	ld9040_sm2_a2_22_80,
	ld9040_sm2_a2_22_100,
	ld9040_sm2_a2_22_110,
	ld9040_sm2_a2_22_120,
	ld9040_sm2_a2_22_130,
	ld9040_sm2_a2_22_140,
	ld9040_sm2_a2_22_150,
	ld9040_sm2_a2_22_160,
	ld9040_sm2_a2_22_170,
	ld9040_sm2_a2_22_180,
	ld9040_sm2_a2_22_190,
	ld9040_sm2_a2_22_200,
	ld9040_sm2_a2_22_210,
	ld9040_sm2_a2_22_220,
	ld9040_sm2_a2_22_230,
	ld9040_sm2_a2_22_240,
	ld9040_sm2_a2_22_250,
	ld9040_sm2_a2_22_260,
	ld9040_sm2_a2_22_270,
	ld9040_sm2_a2_22_280,
	ld9040_sm2_a2_22_290,
	ld9040_sm2_a2_22_300,
};

/*  OCTA 4.52 XVGA - gamma value: 1.9 */
static const unsigned short ld9040_sm2_a2_19_300[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xCC,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_290[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xCA,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_280[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC8,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_270[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC5,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_260[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9D,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xC1,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_250[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9A,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xBD,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_240[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x97,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xBA,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_230[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x95,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB7,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_220[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x91,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xB3,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_210[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8E,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAF,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_200[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xAB,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_190[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x88,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA8,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_180[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x85,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA4,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_170[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x82,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0xA0,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_160[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x7F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x97,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x9C,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_150[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x7B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x93,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x97,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_140[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x77,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x92,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_130[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x73,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x8D,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_120[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x6F,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x86,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x89,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_110[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x6B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x81,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x84,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_100[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x66,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x7C,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x7E,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_90[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x61,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x77,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x78,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_80[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x5B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x71,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x72,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_70[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xCF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x56,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xCF,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x6B,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x6C,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_60[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x50,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x64,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x65,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_50[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x4A,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xD1,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x5C,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x5D,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_40[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x42,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xD2,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x53,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xD1,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x53,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a2_19_30_dimming[] = {
	0xF9, 0x0C,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xD1,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x38,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x97,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xD3,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x49,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xD2,
	DATA_ONLY, 0x0,
	DATA_ONLY, 0x48,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short *psm2_a2_19Gamma_set[] = {
	ld9040_sm2_a2_19_50,
	ld9040_sm2_a2_19_60,
	ld9040_sm2_a2_19_70,
	ld9040_sm2_a2_19_80,
	ld9040_sm2_a2_19_100,
	ld9040_sm2_a2_19_110,
	ld9040_sm2_a2_19_120,
	ld9040_sm2_a2_19_130,
	ld9040_sm2_a2_19_140,
	ld9040_sm2_a2_19_150,
	ld9040_sm2_a2_19_160,
	ld9040_sm2_a2_19_170,
	ld9040_sm2_a2_19_180,
	ld9040_sm2_a2_19_190,
	ld9040_sm2_a2_19_200,
	ld9040_sm2_a2_19_210,
	ld9040_sm2_a2_19_220,
	ld9040_sm2_a2_19_230,
	ld9040_sm2_a2_19_240,
	ld9040_sm2_a2_19_250,
	ld9040_sm2_a2_19_260,
	ld9040_sm2_a2_19_270,
	ld9040_sm2_a2_19_280,
	ld9040_sm2_a2_19_290,
	ld9040_sm2_a2_19_300,
};



static const unsigned short ld9040_sm2_a1_22_300[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xCE,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};



static const unsigned short ld9040_sm2_a1_22_290[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xCC,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_280[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xCA,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_270[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9E,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xC8,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_260[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9C,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xC5,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_250[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9A,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xC2,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_240[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBE,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};
static const unsigned short ld9040_sm2_a1_22_230[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x94,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBB,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_220[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x91,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB7,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_210[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x8E,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB4,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_22_200[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x8B,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xA5,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB1,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_190[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x88,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA1,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9C,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xAD,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_180[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x85,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x99,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA9,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_22_170[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x82,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x95,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA5,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_160[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7F,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x9D,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x91,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA1,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_150[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7C,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x9B,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x8E,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9D,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_140[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x78,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x9B,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x89,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x99,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_130[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x75,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x9A,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x86,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x95,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_120[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x71,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x97,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x82,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x90,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_110[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x6D,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7D,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x8C,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_22_100[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x69,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x93,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x79,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x87,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_22_90[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x65,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x92,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x74,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x82,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};



static const unsigned short ld9040_sm2_a1_22_80[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x60,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x8E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x6F,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7C,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_22_70[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x5B,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x8C,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x69,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x76,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_22_60[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x56,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x89,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x63,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x6F,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_50[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0x9E,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x50,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x88,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x5D,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x68,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_40[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x49,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x85,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x55,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x60,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_22_30_dimming[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0x81,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x41,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x80,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x4C,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x56,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short *psm2_a1_22Gamma_set[] = {
	ld9040_sm2_a1_22_50,
	ld9040_sm2_a1_22_60,
	ld9040_sm2_a1_22_70,
	ld9040_sm2_a1_22_80,
	ld9040_sm2_a1_22_100,
	ld9040_sm2_a1_22_110,
	ld9040_sm2_a1_22_120,
	ld9040_sm2_a1_22_130,
	ld9040_sm2_a1_22_140,
	ld9040_sm2_a1_22_150,
	ld9040_sm2_a1_22_160,
	ld9040_sm2_a1_22_170,
	ld9040_sm2_a1_22_180,
	ld9040_sm2_a1_22_190,
	ld9040_sm2_a1_22_200,
	ld9040_sm2_a1_22_210,
	ld9040_sm2_a1_22_220,
	ld9040_sm2_a1_22_230,
	ld9040_sm2_a1_22_240,
	ld9040_sm2_a1_22_250,
	ld9040_sm2_a1_22_260,
	ld9040_sm2_a1_22_270,
	ld9040_sm2_a1_22_280,
	ld9040_sm2_a1_22_290,
	ld9040_sm2_a1_22_300,
};

static const unsigned short ld9040_sm2_a1_19_300[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xCF,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};



static const unsigned short ld9040_sm2_a1_19_290[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA2,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xCD,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_280[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xCA,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_270[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9E,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xC8,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_260[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9C,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xC5,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_250[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x99,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xC1,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};



static const unsigned short ld9040_sm2_a1_19_240[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBE,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_230[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x94,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBB,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_220[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x91,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB7,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_210[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x8F,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB5,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_200[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x8C,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA0,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB1,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_190[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x89,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9C,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xAD,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_180[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x85,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x99,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA9,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_170[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x82,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xAF,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x95,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA5,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_160[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7F,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x91,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA1,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_150[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7C,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x8D,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x9D,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};



static const unsigned short ld9040_sm2_a1_19_140[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x79,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x89,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x99,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_130[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x75,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xB0,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x86,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xC8,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x95,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_120[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x71,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x81,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x90,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_110[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x6E,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7D,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xCA,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x8C,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_100[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x6A,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x78,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x87,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_90[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x66,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x74,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xCB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x82,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_80[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xC2,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x61,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x6E,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7C,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

static const unsigned short ld9040_sm2_a1_19_70[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xC3,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0xCC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x5C,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0xA4,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xCE,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x69,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xCD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x75,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};



static const unsigned short ld9040_sm2_a1_19_60[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0xCF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x55,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x9F,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x63,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x6E,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short ld9040_sm2_a1_19_50[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0xBE,
	DATA_ONLY, 0xCF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x4F,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x9B,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB6,
	DATA_ONLY, 0xCF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x5D,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x67,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};



static const unsigned short ld9040_sm2_a1_19_40[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xAD,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xD2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x48,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x97,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xD1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x55,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xD2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x5F,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};
static const unsigned short ld9040_sm2_a1_19_30_dimming[] = {
	0xF9, 0x2E,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xC5,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0xD5,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x40,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x93,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB8,
	DATA_ONLY, 0xD2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x4C,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xB9,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xD2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x56,
	0xFB, 0x02,
	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};


static const unsigned short *psm2_a1_19Gamma_set[] = {
	ld9040_sm2_a1_19_50,
	ld9040_sm2_a1_19_60,
	ld9040_sm2_a1_19_70,
	ld9040_sm2_a1_19_80,
	ld9040_sm2_a1_19_100,
	ld9040_sm2_a1_19_110,
	ld9040_sm2_a1_19_120,
	ld9040_sm2_a1_19_130,
	ld9040_sm2_a1_19_140,
	ld9040_sm2_a1_19_150,
	ld9040_sm2_a1_19_160,
	ld9040_sm2_a1_19_170,
	ld9040_sm2_a1_19_180,
	ld9040_sm2_a1_19_190,
	ld9040_sm2_a1_19_200,
	ld9040_sm2_a1_19_210,
	ld9040_sm2_a1_19_220,
	ld9040_sm2_a1_19_230,
	ld9040_sm2_a1_19_240,
	ld9040_sm2_a1_19_250,
	ld9040_sm2_a1_19_260,
	ld9040_sm2_a1_19_270,
	ld9040_sm2_a1_19_280,
	ld9040_sm2_a1_19_290,
	ld9040_sm2_a1_19_300,
};


struct ld9040_panel_data t1_panel_data = {
	.seq_display_set = SEQ_INIT_DISPLAY_SETTING,
	.seq_etc_set = SEQ_INIT_ETC_SETTING,
	.seq_user_set = SEQ_USER_SETTING,
	.seq_panelcondition_set = SEQ_PANEL_CONDITION,
	.seq_displayctl_set = SEQ_DISPCTL,
	.seq_gtcon_set = SEQ_GTCON,
	.seq_manpwr_set = SEQ_MANPWR,
	.seq_pwrctl_set = SEQ_PWR_CTRL,
	.seq_sm2_a2_pwrctl_set = SEQ_SM2_A2_PWR_CTRL,
	.seq_gamma_set1 = SEQ_GAMMA_SET1,
	.seq_sm2_gamma_set1 = SEQ_SM2_GAMMA_SET1,
	.display_on = SEQ_DISPON,
	.display_off = SEQ_DISPOFF,
	.sleep_in = SEQ_SLPIN,
	.sleep_out = SEQ_SLPOUT,
	.gamma_start = SEQ_GAMMA_START,
	.gamma_ctrl = SEQ_GAMMA_CTRL,
	.gamma_sm2_a2_19_table = psm2_a2_19Gamma_set,
	.gamma_sm2_a2_22_table = psm2_a2_22Gamma_set,
	.gamma_sm2_a1_19_table = psm2_a1_19Gamma_set,
	.gamma_sm2_a1_22_table = psm2_a1_22Gamma_set,
	.acl_table = ACL_cutoff_set,
	.elvss_table = SEQ_ELVSS_set,
	.elvss_sm2_table = SEQ_SM2_ELVSS_set,
	.acl_on = SEQ_ACL_ON,
	.elvss_on = SEQ_ELVSS_ON,
	.gamma_table_size = ARRAY_SIZE(psm2_a2_22Gamma_set),
};

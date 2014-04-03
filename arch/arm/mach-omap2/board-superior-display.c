/* Display panel support for Samsung superior Board.
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
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

#include <linux/platform_data/panel-s6e8aa0a01.h>

#include <mach/omap4_ion.h>

#include <plat/vram.h>
#include <plat/android-display.h>

#include <video/omapdss.h>
#include <video/omap-panel-generic-dpi.h>

#include "board-superior.h"
#include "control.h"
#include "mux.h"
#include "omap_muxtbl.h"

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include  <linux/clk.h>
#endif

#define SUPERIOR_FB_RAM_SIZE		SZ_16M /* ~1280*720*4 * 2 */

static unsigned int panel_id;
struct regulator *superior_oled_reg;
struct regulator *superior_oled_reg_iovcc;

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include <plat/clock.h>
static struct clk *dss_ick, *dss_sys_fclk, *dss_dss_fclk;
#endif

static void superior_oled_set_power(bool enable)
{
	if (IS_ERR_OR_NULL(superior_oled_reg)) {
		superior_oled_reg = regulator_get(NULL, "vlcd_vcc_3.1V");
		if (IS_ERR_OR_NULL(superior_oled_reg)) {
			pr_err("Can't get vlcd for display!\n");
			return;
		}
	}

	if (IS_ERR_OR_NULL(superior_oled_reg_iovcc)) {
		superior_oled_reg_iovcc = regulator_get(NULL, "vlcd_io_2.2V");
		if (IS_ERR_OR_NULL(superior_oled_reg_iovcc)) {
			pr_err("Can't get vlcd for display!\n");
			return;
		}
	}

	if (enable) {
		regulator_enable(superior_oled_reg_iovcc);
		regulator_enable(superior_oled_reg);
	} else {
		regulator_disable(superior_oled_reg);
		regulator_disable(superior_oled_reg_iovcc);
	}
}

/* Defined clk_disable for disabling the interface clock
 * (dss_fck) and functional clock(dss_dss_clk)in the suspend path */
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
static void dss_clks_disable(void)
{
	clk_disable(dss_ick);
	clk_disable(dss_dss_fclk);
}
#endif

#ifdef CONFIG_AID_DIMMING
static const struct s6e8aa0a01_acl_parameters superior_oled_acl[] = {
	{
		.cd = GAMMA_20CD,
		.acl_val = 0,
		.regs = {
			0xC1,
			0x00, 0x00
		},
	},
	{
		.cd = GAMMA_30CD,
		.acl_val = 33,
		.regs = {
			0xC1,
			0x47, 0x53, 0x13, 0x53, 0x00, 0x00,
			0x02, 0xCF, 0x00, 0x00, 0x04, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00,

			0x01, 0x06, 0x0A, 0x0F, 0x14, 0x19,
			0x1D, 0x22, 0x27, 0x2B, 0x30
		},
	},
	{
		.cd = GAMMA_250CD,
		.acl_val = 40,
		.regs = {
			0xC1,
			0x47, 0x53, 0x13, 0x53, 0x00, 0x00,
			0x02, 0xCF, 0x00, 0x00, 0x04, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00,

			0x01, 0x07, 0x0C, 0x12, 0x17, 0x1D,
			0x23, 0x28, 0x2E, 0x33, 0x39
		},
	},
	{
		.cd = GAMMA_300CD,
		.acl_val = 50,
		.regs = {
			0xC1,
			0x47, 0x53, 0x13, 0x53, 0x00, 0x00,
			0x02, 0xCF, 0x00, 0x00, 0x04, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00,

			0x01, 0x09, 0x10, 0x18, 0x1F, 0x27,
			0x2E, 0x36, 0x3D, 0x45, 0x4C
		},
	},
};
#else
static const struct s6e8aa0a01_acl_parameters superior_oled_acl[] = {
	{
		.cd = GAMMA_40CD,
		.acl_val = 0,
		.regs = {
			0xC1,
			0x00, 0x00
		},
	},
	{
		.cd = GAMMA_50CD,
		.acl_val = 20,
		.regs = {
			0xC1,
			0x47, 0x53, 0x13, 0x53, 0x00, 0x00,
			0x02, 0xCF, 0x00, 0x00, 0x04, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00,

			0x01, 0x04, 0x07, 0x0A, 0x0D, 0x10,
			0x12, 0x15, 0x18, 0x1B, 0x1E
		},
	},
	{
		.cd = GAMMA_60CD,
		.acl_val = 33,
		.regs = {
			0xC1,
			0x47, 0x53, 0x13, 0x53, 0x00, 0x00,
			0x02, 0xCF, 0x00, 0x00, 0x04, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00,

			0x01, 0x06, 0x0A, 0x0F, 0x14, 0x19,
			0x1D, 0x22, 0x27, 0x2B, 0x30
		},
	},
	{
		.cd = GAMMA_250CD,
		.acl_val = 40,
		.regs = {
			0xC1,
			0x47, 0x53, 0x13, 0x53, 0x00, 0x00,
			0x02, 0xCF, 0x00, 0x00, 0x04, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00,

			0x01, 0x07, 0x0C, 0x12, 0x17, 0x1D,
			0x23, 0x28, 0x2E, 0x33, 0x39
		},
	},
	{
		.cd = GAMMA_300CD,
		.acl_val = 50,
		.regs = {
			0xC1,
			0x47, 0x53, 0x13, 0x53, 0x00, 0x00,
			0x02, 0xCF, 0x00, 0x00, 0x04, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00,

			0x01, 0x09, 0x10, 0x18, 0x1F, 0x27,
			0x2E, 0x36, 0x3D, 0x45, 0x4C
		},
	},
};
#endif

static const u8 superior_oled_cmd_init_pre0[] = {
	0xF0,
	0x5A,
	0x5A,
};

static const u8 superior_oled_cmd_init_pre1[] = {
	0xF1,
	0x5A,
	0x5A,
};

static const u8 superior_oled_cmd_sleep_out[] = {
	0x11,
};

static const u8 superior_oled_cmd_init_panel[] = {
	0xF8, /* Panel Condition Set */
	0x3D, /* DOTC[0:1], GTCON[2:4], SS, DOTC_H[6:7] */
	0x35, /* FLTE[0:7] */
	0x00,
	0x00,
	0x00,
	0x93, /* FLWE */
	0x00,
	0x3C, /* SCTE[0:7] */
	0x7D, /* SCWE */
	0x08, /* INTE */
	0x27,
	0x7D, /* INTE[0:7] */
	0x3F, /* INWE[0:7] */
	0x00, /* EMPS */
	0x00,
	0x00,
	0x20,
	0x04, /* E_FLWE_H[0:7] */
	0x08, /* E_SCTE[0:7] */
	0x6E, /* E_SCWE[0:7] */
	0x00,
	0x00,
	0x00,
	0x02,
	0x08,
	0x08,
	0x23,
	0x23,
	0xC0,
	0xC8, /* CLK2_CON[0:2], CLK1_CON[3:5], CLK2_DC, CLK1_DC */
	0x08, /* INT2_CON[0:2], INT1_CON[3:5], INT2_DC, INT1_DC */
	0x48, /* BICTLB_CON[0:2], BICTL_CON[3:5], BICTLB_DC, BICTL_DC */
	0xC1,
	0x00,
	0xC1, /* EM_FLM_CON[0:2], ACL_FLM_CON[3:5], EM_FLM_DC, ACL_FLM_DC */
	0xFF, /* EM_CLK1B_CON[0:2], EM_CLK1_CON[3:5], EM_CLK1B_DC, EM_CLK1_DC */
	0xFF, /* EM_CLK2B_CON[0:2], EM_CLK2_CON[3:5], EM_CLK2B_DC, EM_CLK2_DC */
	0xC8, /* EM_INT2_CON[0:2], EM_INT1_CON[3:5], EM_INT2_DC, EM_INT1_DC */
};

static const u8 superior_oled_cmd_init_display[] = {
	0xF2, /* Display Condition set */
	0x80, /* Display area */
	0x03, /* VBP : 3 HsYNC */
	0x0D, /* VFP : 13HSYNC */
};

static const struct s6e8aa0a01_sequence_entry
	superior_oled_seq_display_set[] = {
	{
		.cmd = superior_oled_cmd_init_pre0,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_init_pre0),
	},
	{
		.cmd = superior_oled_cmd_init_pre1,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_init_pre1),
	},
	{
		.cmd = superior_oled_cmd_sleep_out,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_sleep_out),
		.msleep = 5,
	},
	{
		.cmd = superior_oled_cmd_init_panel,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_init_panel),
	},
	{
		.cmd = superior_oled_cmd_init_display,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_init_display),
	},
};

static const u8 superior_oled_cmd_gamma_ltps_update[] = {
	0xF7,
	0x03, /* Gamma/LTPS update */
};

static const u8 superior_oled_cmd_init_post0[] = {
	0xF6,
	0x00,
	0x02,
	0x00,
};

static const u8 superior_oled_cmd_init_post1[] = {
	0xB6,
	0x0C,
	0x02,
	0x03,
	0x32,
	0xC0,
	0x44,
	0x44,
	0xC0,
	0x00,
};

static const u8 superior_oled_cmd_init_post2[] = {
	0xD9,
	0x14,
	0x40,
	0x0C,
	0xCB,
	0xCE,
	0x6E,
	0xC4,
	0x07, /* COLUMN_CHOP, FRAME_CHOP, LINE_CHOP, CHOP_EN */
	0xC0,
	0x41, /* ELVSS_CON : 1 */
	0xC5, /* ELVSS -4.9V */
	0x00,
	0x60,
	0x19,
};

static const u8 superior_oled_cmd_power_ctrl[] = {
	0xF4, /* Power Control */
	0xCF,
	0x0A,
	0x12, /* Vreg1 : 4.6V */
	0x10, /* VGH : 5.2v(default) */
	0x1E, /* VGL : -8.0v */
	0x33,
	0x02,
};

static const u8 superior_oled_cmd_display_on[] = {
	0x29,
};

static const struct s6e8aa0a01_sequence_entry superior_oled_seq_etc_set[] = {
	{
		.cmd = superior_oled_cmd_gamma_ltps_update,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_gamma_ltps_update),
	},
	{
		.cmd = superior_oled_cmd_init_post0,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_init_post0),
	},
	{
		.cmd = superior_oled_cmd_init_post1,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_init_post1),
	},
	{
		.cmd = superior_oled_cmd_init_post2,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_init_post2),
	},
	{
		.cmd = superior_oled_cmd_power_ctrl,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_power_ctrl),
		.msleep = 120,
	},
	{
		.cmd = superior_oled_cmd_display_on,
		.cmd_len = ARRAY_SIZE(superior_oled_cmd_display_on),
	},
};

static struct panel_s6e8aa0a01_data superior_oled_data_a1 = {
	.set_power		= superior_oled_set_power,
	.seq_display_set	= superior_oled_seq_display_set,
	.seq_display_set_size	= ARRAY_SIZE(superior_oled_seq_display_set),
	.seq_etc_set		= superior_oled_seq_etc_set,
	.seq_etc_set_size	= ARRAY_SIZE(superior_oled_seq_etc_set),
	.seq_panel_condition_set = superior_oled_cmd_init_panel,
	.seq_panel_condition_set_size =
		ARRAY_SIZE(superior_oled_cmd_init_panel),
	.acl_table		= superior_oled_acl,
	.acl_table_size		= ARRAY_SIZE(superior_oled_acl),
};

static struct panel_s6e8aa0a01_data superior_oled_data_a2 = {
	.set_power		= superior_oled_set_power,
	.seq_display_set	= superior_oled_seq_display_set,
	.seq_display_set_size	= ARRAY_SIZE(superior_oled_seq_display_set),
	.seq_etc_set		= superior_oled_seq_etc_set,
	.seq_etc_set_size	= ARRAY_SIZE(superior_oled_seq_etc_set),
	.seq_panel_condition_set = superior_oled_cmd_init_panel,
	.seq_panel_condition_set_size =
		ARRAY_SIZE(superior_oled_cmd_init_panel),
	.acl_table		= superior_oled_acl,
	.acl_table_size		= ARRAY_SIZE(superior_oled_acl),
};

static struct omap_dss_device superior_oled_device = {
	.name			= "lcd",
	.driver_name		= "s6e8aa0a01",
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
		.timings = {
			.x_res = 720,
			.y_res = 1280,
			.pixel_clock = 80842,
			.hfp = 158,
			.hsw = 2,
			.hbp = 160,
			.vfp = 13,
			.vsw = 1,
			.vbp = 2,
		},
		.width_in_um	= 58000,
		.height_in_um	= 102000,
	},

	.ctrl = {
		.pixel_size = 24,
	},

	.clocks = {
		.dispc = {
			.channel = {
				.lck_div	= 1,	/* LCD */
				.pck_div	= 2,	/* PCD */
				.lcd_clk_src
					= OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},
		.dsi = {
			.regn		= 19,	/* DSI_PLL_REGN */
			.regm		= 240,	/* DSI_PLL_REGM */

			.regm_dispc	= 6,	/* PLL_CLK1 (M4) */
			.regm_dsi	= 6,	/* PLL_CLK2 (M5) */
			.lp_clk_div	= 8,	/* LPDIV */
			.offset_ddr_clk	= 122,	/* DDR PRE & DDR POST
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
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init				= true,
	.dss_clks_disable                       = dss_clks_disable,
#else
	.skip_init				= false,
#endif
};


static struct omap_dss_device superior_hdmi_device = {
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
	.hpd_gpio = -1,
};

static struct omap_dss_device *superior_dss_devices[] = {
	&superior_oled_device,
#ifdef CONFIG_OMAP4_DSS_HDMI
	&superior_hdmi_device,
#endif

};

static struct omap_dss_board_info superior_dss_data = {
	.num_devices	= ARRAY_SIZE(superior_dss_devices),
	.devices	= superior_dss_devices,
	.default_device	= &superior_oled_device,
};

static struct omapfb_platform_data superior_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = SUPERIOR_FB_RAM_SIZE,
			},
		},
	},
};

void __init omap4_superior_memory_display_init(void)
{
	omap_android_display_setup(&superior_dss_data,
				   NULL,
				   NULL,
				   &superior_fb_pdata,
				   get_omap_ion_platform_data());
}

void __init omap4_superior_display_init(void)
{
	struct panel_s6e8aa0a01_data *panel;
	int oled_det_gpio;

	/* Removed ENABLE_ON_INIT flag for dss_sys_clk(functional clock)
	 * in arch/arm/mach-omap2/clock44xx_data.c, manually
	 * enabling the functional clock by getting dss_sys_fclk.
	 * NOTE: It will be disabled, during disable path.
	 */
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

	if (panel_id == SM2A2)
		panel = &superior_oled_data_a2;
	else
		panel = &superior_oled_data_a1;

	superior_oled_device.data = panel;

	omap4_ctrl_pad_writel(0x1FF80000,
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);

	panel->reset_gpio =
	    omap_muxtbl_get_gpio_by_name("MLCD_RST");
	panel->oled_id_gpio =
		omap_muxtbl_get_gpio_by_name("OLED_ID");

	oled_det_gpio = omap_muxtbl_get_gpio_by_name("OLED_DET");
	gpio_request(oled_det_gpio, "OLED_DET");
	gpio_direction_input(oled_det_gpio);
	panel->oled_det_irq = gpio_to_irq(oled_det_gpio);

	omap_display_init(&superior_dss_data);
}

static int __init get_panel_id(char *str)
{
	long value;
	int ret;

	ret = strict_strtol(str, 0, &value);
	if (ret < 0)
		return ret;

	panel_id = (unsigned int)value;
	return 0;
}
__setup("mms_ts.panel_id=", get_panel_id);

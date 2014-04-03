/* arch/arm/mach-omap2/board-gokey-display.c
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
#include <linux/types.h>

#include <linux/platform_data/panel-lms501kf07.h>

#include <plat/vram.h>
#include <plat/omap_hwmod.h>
#include <plat/android-display.h>

#include <video/omapdss.h>

#include "board-gokey.h"
#include "control.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "sec_common.h"


#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include  <linux/clk.h>
#endif

#define GOKEY_FB_RAM_SIZE	SZ_4M	/* 800*480*4*2 = 3000kb */


static const u8 lms501kf07_cmd_setextc[] = {
	0xb9,
	0xff, 0x83, 0x69,
};

static const u8 lms501kf07_cmd_setmipi[] = {
	0xba,
	0x11, 0x00, 0x16, 0xC6, 0x80, 0x0A, 0x00, 0x10, 0x24, 0x02,
	0x21, 0x21, 0x9A, 0x11, 0x14,
};

static const u8 lms501kf07_cmd_setgip[] = {
	0xd5,
	0x00, 0x00, 0x09, 0x03, 0x2D, 0x00, 0x00, 0x12, 0x31, 0x23,
	0x00, 0x00, 0x10, 0x70, 0x37, 0x00, 0x00, 0x0D, 0x01, 0x40,
	0x37, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xEF, 0x00, 0x13, 0x57,
	0x71, 0x00, 0x00, 0x00, 0xEF, 0xEF, 0x00, 0x64, 0x20, 0x06,
	0x00, 0x00, 0x00, 0xEF, 0xEF, 0x00, 0x02, 0x46, 0x60, 0x00,
	0x00, 0x00, 0xEF, 0xEF, 0x00, 0x75, 0x31, 0x17, 0x00, 0x00,
	0x00, 0xEF, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0F,
	0xFC, 0x0C, 0xFC, 0xFF, 0x0F, 0xFC, 0x0C, 0xFC, 0xFF, 0x00,
	0x00, 0x5A,
};

static const u8 lms501kf07_cmd_setpower[] = {
	0xb1,
	0x0A, 0x83, 0x77, 0x00, 0x8F, 0x0F, 0x1C, 0x1C, 0x0C, 0x2A,
	0x20, 0xCE,
};

static const u8 lms501kf07_cmd_setpower_rev2[] = {
	0xb1,
	0x0A, 0x83, 0x77, 0x00, 0x91, 0x0F, 0x1C, 0x1C, 0x0C, 0x2A,
	0x20, 0x4E,
};

static const u8 lms501kf07_cmd_sleepout[] = {
	0x11,
};

static const u8 lms501kf07_cmd_setrgb[] = {
	0xb3,
	0x03, 0x00, 0x30, 0x0b,
};

static const u8 lms501kf07_cmd_setcyc[] = {
	0xb4,
	0x02,
};

static const u8 lms501kf07_cmd_setvcom1[] = {
	0xb6,
	0xB1, 0xB1, 0x00,
};

static const u8 lms501kf07_cmd_setvcom1_rev2[] = {
	0xb6,
	0xB1, 0xA8, 0x00,
};

static const u8 lms501kf07_cmd_setptba[] = {
	0xbf,
	0x5F, 0x00, 0x00, 0x06,
};

static const u8 lms501kf07_cmd_setpanel[] = {
	0xcc,
	0x02,
};

static const u8 lms501kf07_cmd_setpanel_rev2[] = {
	0xcc,
	0x0E,
};

static const u8 lms501kf07_cmd_setdgc[] = {
	0xc1,
	0x00,
};

static const u8 lms501kf07_cmd_setstba[] = {
	0xc0,
	0x73, 0x50, 0x00, 0x2C, 0xC4, 0x04,
};

static const u8 lms501kf07_cmd_setstba_rev2[] = {
	0xc0,
	0x73, 0x50, 0x00, 0x1F, 0x04, 0x04,
};

static const u8 lms501kf07_cmd_seteq[] = {
	0xe3,
	0x03, 0x03, 0x03, 0x03,
};

static const u8 lms501kf07_cmd_setvcom2[] = {
	0xea,
	0x7A,
};

static const u8 lms501kf07_cmd_seteco[] = {
	0xc6,
	0x40,
};

static const u8 lms501kf07_cmd_setgamma[] = {
	0xe0,
	0x00, 0x1C, 0x21, 0x35, 0x3A, 0x3F, 0x31, 0x4D, 0x08, 0x0E,
	0x0E, 0x10, 0x13, 0x11, 0x13, 0x18, 0x1D, 0x00, 0x1C, 0x21,
	0x35, 0x3A, 0x3F, 0x31, 0x4D, 0x08, 0x0E, 0x0E, 0x10, 0x13,
	0x11, 0x13, 0x18, 0x1D, 0x01,
};

static const u8 lms501kf07_cmd_setgamma_rev2[] = {
	0xe0,
	0x00, 0x1C, 0x22, 0x35, 0x3A, 0x3F, 0x32, 0x4D, 0x08, 0x0E,
	0x0E, 0x11, 0x13, 0x11, 0x13, 0x18, 0x1D, 0x00, 0x1C, 0x22,
	0x35, 0x3A, 0x3F, 0x32, 0x4D, 0x08, 0x0E, 0x0E, 0x11, 0x13,
	0x11, 0x13, 0x18, 0x1D, 0x01,
};

static const u8 lms501kf07_cmd_cabcpwm[] = {
	0xc9,
	0x0f, 0x00,
};

static const u8 lms501kf07_cmd_dispon[] = {
	0x29,
};

static const u8 lms501kf07_cmd_ctrldisp[] = {
	0x53,
	0x24,
};

static const u8 lms501kf07_cmd_dispoff[] = {
	0x28,
};

static const u8 lms501kf07_cmd_sleepin[] = {
	0x10,
};

static const u8 mdnie_ui_reg[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x02, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x6b, 0x1f, 0x10, 0x1f, 0x85,
	0x1f, 0xd1, 0x04, 0xa9, 0x1f, 0x86, 0x1f, 0xd1, 0x1f, 0x10,
	0x05, 0x1f,
};

static const u8 mdnie_gall_reg[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x6b, 0x1f, 0x10, 0x1f, 0x85,
	0x1f, 0xd1, 0x04, 0xa9, 0x1f, 0x86, 0x1f, 0xd1, 0x1f, 0x10,
	0x05, 0x1f,
};

static const u8 mdnie_vid_reg[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x6b, 0x1f, 0x10, 0x1f, 0x85,
	0x1f, 0xd1, 0x04, 0xa9, 0x1f, 0x86, 0x1f, 0xd1, 0x1f, 0x10,
	0x05, 0x1f,

};

static const u8 mdnie_vidwarm_reg[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xd0, 0x00, 0xFc, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x6b, 0x1f, 0x10, 0x1f, 0x85,
	0x1f, 0xd1, 0x04, 0xa9, 0x1f, 0x86, 0x1f, 0xd1, 0x1f, 0x10,
	0x05, 0x1f,
};

static const u8 mdnie_vidcold_reg[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0xeb, 0x00, 0xe8, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x6b, 0x1f, 0x10, 0x1f, 0x85,
	0x1f, 0xd1, 0x04, 0xa9, 0x1f, 0x86, 0x1f, 0xd1, 0x1f, 0x10,
	0x05, 0x1f,
};

static const u8 mdnie_cam_reg[] = {
	0xE6,
	0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x06, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00,
	0x20, 0x00, 0x20, 0x00, 0x05, 0x6b, 0x1f, 0x10, 0x1f, 0x85,
	0x1f, 0xd1, 0x04, 0xa9, 0x1f, 0x86, 0x1f, 0xd1, 0x1f, 0x10,
	0x05, 0x1f,
};

static const struct lms501kf07_mdnie_data gokey_mdnie_data = {
	.ui = (u8 *)mdnie_ui_reg,
	.gallery = (u8 *)mdnie_gall_reg,
	.video = (u8 *)mdnie_vid_reg,
	.video_warm = (u8 *)mdnie_vidwarm_reg,
	.video_cold = (u8 *)mdnie_vidcold_reg,
	.camera = (u8 *)mdnie_cam_reg,
	.size = ARRAY_SIZE(mdnie_ui_reg),	/* same size for all */
};

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
#include <plat/clock.h>
static struct clk *dss_ick, *dss_sys_fclk, *dss_dss_fclk;

/* Defined clk_disable for disabling the interface clock
 * (dss_fck) and functional clock (dss_dss_fclk) in the suspend path */
static void dss_clks_disable(void)
{
	clk_disable(dss_ick);
	clk_disable(dss_dss_fclk);
}

#endif

static struct regulator *vlcd_3p0v;	/* LDO3 VCC_3.0V_LCD */
static struct regulator *vlcd_1p8v;	/* LDO5 V_LCD_1.8V */

static void gokey_lcd_set_power(bool enable)
{
	if (vlcd_3p0v == NULL) {
		vlcd_3p0v = regulator_get(NULL, "VLCD_3P0V");
		if (IS_ERR_OR_NULL(vlcd_3p0v)) {
			pr_err("Can't get VCC_3.0V_LCD for LCD!\n");
			return;
		}
	}

	if (vlcd_1p8v == NULL) {
		vlcd_1p8v = regulator_get(NULL, "VLCD_1P8V");
		if (IS_ERR_OR_NULL(vlcd_1p8v)) {
			regulator_put(vlcd_3p0v);
			pr_err("Can't get V_LCD_1.8V for LCD!\n");
			return;
		}
	}

	if (enable) {
		regulator_enable(vlcd_1p8v);
		regulator_enable(vlcd_3p0v);
		printk(KERN_INFO "Enable LDO for LCD!\n");
	} else {
		regulator_disable(vlcd_3p0v);
		regulator_disable(vlcd_1p8v);
		printk(KERN_INFO "Disable LDO for LCD!\n");
	}
}

static const struct lms501kf07_sequence_entry gokey_lcd_seq_display_on[] = {
	{
	 .cmd = lms501kf07_cmd_setextc,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setextc),
	 .msleep = 5,
	 },
	{
	 .cmd = lms501kf07_cmd_setmipi,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setmipi),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setgip,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setgip),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setpower,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setpower),
	 .msleep = 5,
	 },
	{
	 .cmd = lms501kf07_cmd_sleepout,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_sleepout),
	 .msleep = 125,
	 },
	{
	 .cmd = lms501kf07_cmd_setrgb,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setrgb),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setcyc,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setcyc),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setvcom1,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setvcom1),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setptba,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setptba),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setpanel,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setpanel),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setdgc,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setdgc),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setstba,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setstba),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_seteq,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_seteq),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setvcom2,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setvcom2),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_seteco,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_seteco),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setgamma,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setgamma),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_cabcpwm,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_cabcpwm),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_dispon,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_dispon),
	 .msleep = 100,
	 },
	{
	 .cmd = lms501kf07_cmd_ctrldisp,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_ctrldisp),
	 .msleep = 0,
	 },
};

static const struct lms501kf07_sequence_entry gokey_lcd_seq_display_on_rev2[]
= {
	{
	 .cmd = lms501kf07_cmd_setextc,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setextc),
	 .msleep = 5,
	 },
	{
	 .cmd = lms501kf07_cmd_setmipi,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setmipi),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setgip,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setgip),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setpower_rev2,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setpower_rev2),
	 .msleep = 5,
	 },
	{
	 .cmd = lms501kf07_cmd_sleepout,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_sleepout),
	 .msleep = 125,
	 },
	{
	 .cmd = lms501kf07_cmd_setrgb,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setrgb),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setcyc,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setcyc),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setvcom1_rev2,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setvcom1_rev2),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setptba,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setptba),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setpanel_rev2,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setpanel_rev2),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setdgc,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setdgc),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setstba_rev2,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setstba_rev2),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_seteq,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_seteq),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setvcom2,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setvcom2),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_seteco,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_seteco),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_setgamma_rev2,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_setgamma_rev2),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_cabcpwm,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_cabcpwm),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_dispon,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_dispon),
	 .msleep = 100,
	 },
	{
	 .cmd = lms501kf07_cmd_ctrldisp,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_ctrldisp),
	 .msleep = 0,
	 },
};

static const struct lms501kf07_sequence_entry gokey_lcd_seq_display_off[] = {
	{
	 .cmd = lms501kf07_cmd_dispoff,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_dispoff),
	 .msleep = 0,
	 },
	{
	 .cmd = lms501kf07_cmd_sleepin,
	 .cmd_len = ARRAY_SIZE(lms501kf07_cmd_sleepin),
	 }
};

static struct panel_lms501kf07_data gokey_lcd_data = {
	.set_power = gokey_lcd_set_power,
	.seq_display_on = gokey_lcd_seq_display_on,
	.seq_display_on_size = ARRAY_SIZE(gokey_lcd_seq_display_on),
	.seq_display_off = gokey_lcd_seq_display_off,
	.seq_display_off_size = ARRAY_SIZE(gokey_lcd_seq_display_off),
	.mdnie_data = &gokey_mdnie_data,
};

static struct omap_dsi_timings lms501kf07_dsi_timings = {
	.hbp = 221,
	.hfp = 149,
	.hsa = 40,		/* DSI_VM_TIMING1 */
	.vbp = 12,
	.vfp = 8,
	.vsa = 4,		/* DSI_VM_TIMING2 */
	.vact = 800,
	.tl = 1137,		/* DSI_VM_TIMING3 */
	.hsa_hs_int = 0,
	.hfp_hs_int = 0,
	.hbp_hs_int = 0,	/* DSI_VM_TIMING4 */
	.hsa_lp_int = 0,
	.hfp_lp_int = 0,
	.hbp_lp_int = 2,	/* DSI_VM_TIMING5 */
	.bl_lp_int = 21,
	.bl_hs_int = 0,		/* DSI_VM_TIMING6 */
	.exit_lat = 16,
	.enter_lat = 16,	/* DSI_VM_TIMING7 */
};

static struct omap_dss_device gokey_lcd_device = {
	.name = "lcd",
	.driver_name = "lms501kf07",
	.type = OMAP_DISPLAY_TYPE_DSI,
	.data = &gokey_lcd_data,
	.phy.dsi = {
		    .type = OMAP_DSS_DSI_TYPE_VIDEO_MODE,
		    .clk_lane = 1,
		    .clk_pol = 0,
		    .data1_lane = 2,
		    .data1_pol = 0,
		    .data2_lane = 3,
		    .data2_pol = 0,
		    .data3_lane = 0,	/* data lane 3&4 not used */
		    .data3_pol = 0,
		    .data4_lane = 0,
		    .data4_pol = 0,
		    .line_bufs = 2,
		    },
	.panel = {
		  .timings = {
			      .x_res = 480,
			      .y_res = 800,
			      .pixel_clock = 37476,
			      .hfp = 271,
			      .hsw = 6,
			      .hbp = 1,
			      .vfp = 8,
			      .vsw = 4,
			      .vbp = 12,
			      },
		  .acbi = 0,
		  .acb = 40,
		  .width_in_um = 75000,
		  .height_in_um = 130000,
		  },
	.clocks = {
		   .dispc = {
			     .channel = {
					 .lck_div = 1,	/* LCD */
					 .pck_div = 4,	/* PCD */
					 .lcd_clk_src =
					 OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
					 },
			     .dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
			     },
		   .dsi = {
			   .regn = 21,	/* DSI_PLL_REGN */
			   .regm = 246,	/* DSI_PLL_REGM */
			   .regm_dispc = 6,	/* PLL_CLK1 (M4) */
			   .regm_dsi = 5,	/* PLL_CLK2 (M5) */
			   .lp_clk_div = 9,	/* LPDIV */
			   .offset_ddr_clk = 1,	/* DDR PRE&DDR POST increase */
			   .dsi_fclk_src = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,

			   .tlpx = 0xC,
			   .tclk = {
				    .zero = 0x3B,
				    .prepare = 0xF,
				    .trail = 0x10,
				    },
			   .ths = {
				   .zero = 0x18,
				   .prepare = 0x12,
				   .exit = 0x21,
				   .trail = 0x13,
				   },
			   },
		   },
	.channel = OMAP_DSS_CHANNEL_LCD,
	.ctrl.pixel_size = 24,

#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init = true,
	.dss_clks_disable = dss_clks_disable,
#else
	.skip_init = false,
#endif

	.dsi_timings = &lms501kf07_dsi_timings,
};

static struct omap_dss_device *gokey_dss_devices[] = {
	&gokey_lcd_device,
};

static struct omap_dss_board_info gokey_dss_data = {
	.num_devices = ARRAY_SIZE(gokey_dss_devices),
	.devices = gokey_dss_devices,
	.default_device = &gokey_lcd_device,
};

static struct omapfb_platform_data gokey_fb_pdata = {
	.mem_desc = {
		     .region_cnt = 1,
		     .region = {
				[0] = {
				       .size = GOKEY_FB_RAM_SIZE,
				       },
				},
		     },
};

/* Don't Reset GPIO2(for LCD_nRST) and GPIO4(for BL_CTRL).
   This is to prevent LCD blinking when executing Kernel */
void __init omap4_gokey_display_early_init(void)
{
	struct omap_hwmod *gpio_hwmod;
	char *gpio_no_reset[] = { "gpio4", "gpio2" };
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(gpio_no_reset); i++) {
		gpio_hwmod = omap_hwmod_lookup(gpio_no_reset[i]);
		if (likely(gpio_hwmod))
			gpio_hwmod->flags = HWMOD_INIT_NO_RESET;
	}
}

void __init omap4_gokey_memory_display_init(void)
{
	omap_android_display_setup(&gokey_dss_data,
				   NULL,
				   NULL,
				   &gokey_fb_pdata,
				   get_omap_ion_platform_data());
}

void __init omap4_gokey_display_init(void)
{
	struct panel_lms501kf07_data *panel = &gokey_lcd_data;

	/* Removed ENABLE_ON_INIT flag for dss_fck in
	 * arch/arm/mach-omap2/clock44xx_data.c, manually
	 * enabling the dss interface clock by getting ick,
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

	omap4_ctrl_pad_writel(0x1FF80000,
			      OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);

	panel->reset_gpio = omap_muxtbl_get_gpio_by_name("LCD_nRST");
	panel->blctrl_gpio = omap_muxtbl_get_gpio_by_name("BL_CTRL");

	if (system_rev >= 2)
		panel->seq_display_on = gokey_lcd_seq_display_on_rev2;

	/* If this flag is true, the panel driver will turn on backlight after
	 * waking up from sleep mode and initializing the panel. Generally
	 * there is no problem for doing like this because Android platform
	 * instatntly draws lock screen and lights up backlight. But if this
	 * procedure is not done quickly main menu will be shown because
	 * the panel driver already turned on backlight and the lock screen is
	 * not yet drawn. So the panel driver will not turn on backlight and
	 * let Android controls it if this flag is false.
	 * But we still need to turn it on for some independent programs
	 * like Recovery boot or LPM charing programs.
	 */
	if (!strncmp(sec_androidboot_mode, "reboot_normal", 13) ||
	    !strncmp(sec_androidboot_mode, "power_key", 9))
		panel->bl_on_after_init = false;
	else
		panel->bl_on_after_init = true;

	omap_display_init(&gokey_dss_data);
}

/* Display panel support for Samsung Palau Board.
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

#include <mach/omap4_ion.h>

#include <plat/vram.h>
#include <plat/android-display.h>

#include <video/omapdss.h>
#include <video/cmc624.h>

#include "control.h"
#include "mux.h"
#include "omap_muxtbl.h"

#define PALAU_FB_RAM_SIZE		SZ_16M /* ~1280*720*4 * 2 */

/*
 * GPIO
 */
enum {
	GPIO_LCD_EN	= 0,
	GPIO_MLCD_RST,
	GPIO_IMA_SLEEP,
	GPIO_IMA_NRST,
	GPIO_IMA_CMC_EN,
	GPIO_IMA_PWR_EN,
};

static struct gpio display_gpios[] = {
	[GPIO_LCD_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "LCD_EN",
	},
	[GPIO_MLCD_RST] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "MLCD_RST",
	},
	[GPIO_IMA_SLEEP] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "IMA_SLEEP",
	},
	[GPIO_IMA_NRST] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "IMA_nRST",
	},
	[GPIO_IMA_CMC_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "IMA_CMC_EN",
	},
	[GPIO_IMA_PWR_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "IMA_PWR_EN",
	},
};

/*
 * CMC624 Setting
 */

static const struct cmc624_register_set cmc624_init_seq[] = {
	/* CLOCK_TOP */
	{0x00, 0x0002},	/* BANK 2 */
	{0x30, 0x0007},	/* S.PLL DISEN, NORMAL, P = 7   (Fout = 340.11MHz) */
	{0x31, 0x20F8},	/* S = 2, M = 248               (Fout = 340.11MHz) */
	{0x30, 0x1007},	/* S.PLL EN, NORMAL, P = 7      (Fout = 340.11MHz) */
	{0x3A, 0x0C04},	/* CLOCK DIVIDER VALUE (I2C = 12[28.34MHz], A = 4) */
	{0x3B, 0x040D},	/* CLOCK DIVIDER VALUE (M1 = 4, M2 = 13) */
	{0x3C, 0x1203},	/* CLOCK DIVIDER VALUE (B = 18, P = 3) */
	{0x3D, 0x0400},	/* CLOCK DIVIDER VALUE (PWM = 1024) */
	{0x39, 0x007F},	/* CLOCK DIVIDER ENABLE */
	{0x3E, 0x2223},	/* CLOCK MUX SEL */
	{SLEEPMSEC, 1},

	{0x00, 0x0000},	/* BANK 0 */
	{0xFD, 0x0000},	/* MODULE REG MASK RELEASE */
	{0xFE, 0x0004},	/* MODULE REG MASK RELEASE */

	{0xFF, 0x0000},	/* REG MASK RELEASE */

	/* INPUT IF */
	{0x00, 0x0002},	/* BANK 2 */
	/* RGB BOTTOM ALIGNMENT, VSYNC/HSYNC LOW ACTIVE, DE HIHG ACTIVE */
	{0x50, 0x0061},

	/* TCON */
	{0x00, 0x0002},	/* BANK 2 */
	{0x60, 0x5400},	/* TCON VSYNC DELAY */
	{0x63, 0x0810}, /* OUTPUT COLOR MAP, TCON OUTPUT POL(LLH) */
	{0x67, 0x0002},	/* VSYNC PULSE WIDTH = 2 */
	{0x68, 0x0004},	/* HSYNC PULSE WIDTH = 4 */
	{0x69, 0x0001},	/* VBP = 1 */
	{0x6A, 0x000D},	/* VFP = 13 */
	{0x6B, 0x00A0},	/* HBP = 160 */
	{0x6C, 0x00A2},	/* HFP = 162 */
	{0x66, 0x8000},	/* CDC */

	/* IP */
	{0x00, 0x0000},	/* BANK 0 */
	{0x01, 0x0077},	/* HSYNC/DE MASK RELEASE */
	{0x03, 0x02D0},	/* WIDTH = 720 */
	{0x04, 0x0500},	/* HEIGHT = 1280 */
	{0x08, 0x0000},	/* ALGO MODULE OFF */
	{0x09, 0x0000},	/* ALGO MODULE OFF */
	{0x0A, 0x0000},	/* ALGO MODULE OFF */

	/* CONV */
	{0x00, 0x0003},	/* BANK 3 */
	{0x01, 0x0000},	/* I2C TO MIPI */
	{0x43, 0x8000},	/* M_BAND_CTL */
	{0x42, 0x2321},	/* M_PLL SETTING (Fout = 480.00MHz) */
	{0x41, 0x8000},	/* M_PLL SETTING & M_PLL ENABLE */
	{0x44, 0x000A},	/* S_HSSETTLE */
	{0x40, 0x0030},	/* M/S CLOCK LANE ENABLE */

	{0x00, 0x0000},	/* BANK 0 */
	{0xFD, 0xFFFF},	/* MODULE REG MASK RELEASE */
	{0xFE, 0xFFFF},	/* MODULE REG MASK RELEASE */
	{0xFF, 0x0000},	/* MASK RELEASE */

	/* DSI HOST */
	{0x00, 0x0003},	/* BANK 3 */
	{0x84, 0xFFFF},	/* INTERRUPT ENABLE */
	{0x85, 0x1FEF},	/* INTERRUPT ENABLE */
	/* DSI FUNCTION(CMD  8 BIT DATA, RGB888,
	CMD VC 0, VIDEO VC 0, 4 LANE) */
	{0x86, 0x6204},
	{0x88, 0xFFFF},	/* HIGH SPEED RECEIVE TIMEOUT */
	{0x89, 0x00FF},	/* HIGH SPEED RECEIVE TIMEOUT */
	{0x8A, 0xFFFF},	/* LOW POWER RECEIVE TIMEOUT */
	{0x8B, 0x00FF},	/* LOW POWER RECEIVE TIMEOUT */
	{0x8C, 0x001F},	/* TURN AROUND TIMEOUT */
	{0x8E, 0x00FF},	/* DEVICE RESET TIMER */
	{0x90, 0x02D0},	/* HORIZANTAL RESOLUTION = 720 */
	{0x91, 0x0500},	/* VERTICAL RESOLUTION = 1280 */

	{0x94, 0x0003},	/* HORIZANTAL SYNC PADDING COUNT */
	{0x96, 0x0078},	/* HORIZANTAL BACK PORCH COUNT */
	{0x98, 0x007A},	/* HORIZANTAL FRONT FORCH COUNT */
	{0x9A, 0x021C},	/* HORIZANTAL ACTIVE AREA */

	{0x9C, 0x0002},	/* VERTICAL SYNC PADDING COUNT */
	{0x9E, 0x0001},	/* VERTICAL BACK PORCH COUNT */
	{0xA0, 0x000D},	/* VERTICAL FRONT FORCH COUNT */
	{0xA8, 0x0d07},	/* MASTER INIT TIME */
	{0xAC, 0x0002},	/* VIDEO MODE FORMAT = NON BURST SYNC EVENT */
	{0xB0, 0x0008},	/* VSYNC, HSYNC, COLOR MODE, SHUT DOWN POLARITY */

	{0xB4, 0x0004},	/* LP EQUIVALENT BYTECLK */
	/* HIGH SPEED <-> LOW POWER SWITCHING COUNTER FOR DATA LANE */
	{0xA2, 0x0014},
	/* HIGH SPEED -> LOW POWER SWITCHING COUNTER FOR CLOCK LANE */
	{0xB2, 0x000B},
	/* HIGH SPEED <- LOW POWER SWITCHING COUNTER FOR CLOCK LANE */
	{0xB3, 0x001C},

	{0x80, 0x0001},	/* DEVICE READY */
	{0xA4, 0x0002},	/* COLOR MODE OFF, DPI ON */

	/* DSI DEVICE */
	{0x00, 0x0003},	/* BANK 3 */
	{0xC4, 0xFFFF},	/* INTERRUPT ENABLE */
	{0xC5, 0x01FF},	/* INTERRUPT ENABLE */
	{0xC6, 0x0064},	/* DSI FUNCTION : BURST & NON BURST SYNC EVENT */
	{0xC8, 0xFFFF},	/* HIGH SPEED RECEIVE TIMEOUT */
	{0xC9, 0xFFFF},	/* HIGH SPEED RECEIVE TIMEOUT */
	{0xCA, 0x005E},	/* LOW POWER RECEIVE TIMEOUT */
	{0xCB, 0x0000},	/* LOW POWER RECEIVE TIMEOUT */
	{0xCC, 0x0025},	/* TURN AROUND TIMEOUT */
	{0xCE, 0x07D0},	/* DEVICE RESET TIMER */
	{0xD2, 0x0000},	/* CRC, ECC, EOT ENABLE */
	{0xD4, 0x0004},	/* HSYNC COUNT = 4 */
	{0xD5, 0x0002},	/* VSYNC COUNT = 2 */
	{0xC0, 0x0001},	/* DEVICE READY */


	{0x00, 0x0002},	/* BANK 2 */
	{0x3F, 0x011B},	/* MON_CLK : TXBYTECLKHS ( 60MHz ) */

	/* CONV */
	{0x00, 0x0003},	/* BANK 3 */
	{0x01, 0x0000},	/* I2C TO MIPI */
	{0x00, 0x0002},	/* BANK 2 */
	{0x52, 0x0001},	/* RGB IF ENABLE */
	{0x00, 0x0003},	/* BANK 3 good */

};

static const struct cmc624_register_set sony_init_seq[] = {
	{0x00, 0x0003},
	{0x07, 0x1000},
	/* SLEEP OUT */
	{0x03, 0x0001},
	{0x02, 0x0000},
	{0x04, 0x0011},
	{SLEEPMSEC, 140},
	/* PRE0 */
	{0x03, 0x0003},
	{0x02, 0x1300},
	{0x05, 0x00F0},
	{0x05, 0x005A},
	{0x05, 0x005A},
	/* PRE1 */
	{0x03, 0x0003},
	{0x02, 0x1300},
	{0x05, 0x00F1},
	{0x05, 0x005A},
	{0x05, 0x005A},
	/* DSCTL */
	{0x03, 0x0002},
	{0x02, 0x1300},
	{0x05, 0x0036},
	{0x05, 0x0080},
	/* TOP_ENL */
	{0x03, 0x0002},
	{0x02, 0x1300},
	{0x05, 0x00F7},
	{0x05, 0x0001},
	/* WRDISBV */
	{0x03, 0x0002},
	{0x02, 0x1300},
	{0x05, 0x0051},
	{0x05, 0x00FF},
	/* WRCTRLD */
	{0x03, 0x0002},
	{0x02, 0x1300},
	{0x05, 0x0053},
	{0x05, 0x002C},
	/* WRCABC */
	{0x03, 0x0002},
	{0x02, 0x1300},
	{0x05, 0x0055},
	{0x05, 0x0001},
	/* DISPLAY ON */
	{0x03, 0x0001},
	{0x02, 0x0000},
	{0x04, 0x0029},

	{SLEEPMSEC, 10},
};

static const struct cmc624_register_set cmc624_Bypass[] = {
/*start Palau bypass*/
	{0x0000, 0x0000},	/* BANK 0*/
	{0x0008, 0x0000},	/* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/* MCM off*/
	{0x000a, 0x0000},	/* UC off*/
	{0x0030, 0x0000},	/* FA cs1 | de8 hdr2 fa1 */
	{0x00ff, 0x0000},	/* Mask Release */
/*end*/
};

static const struct cmc624_register_set standard_video_cabcoff[] = {
/*start Palau standard video*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0060},	/*DE pe*/
	{0x00b3, 0x0060},	/*DE pf*/
	{0x00b4, 0x0060},	/*DE pb*/
	{0x00b5, 0x0060},	/*DE ne*/
	{0x00b6, 0x0060},	/*DE nf*/
	{0x00b7, 0x0060},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2004},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00f0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};
static const struct cmc624_register_set standard_video_cabcon[] = {
/*start D2 standard video lowpower*/
	{0x0000, 0x0000},    /*BANK 0*/
	{0x0008, 0x0230},    /*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},    /*MCM off*/
	{0x000a, 0x0000},    /*UC off*/
	{0x0030, 0x0000},    /*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0060},    /*DE pe*/
	{0x00b3, 0x0060},    /*DE pf*/
	{0x00b4, 0x0060},    /*DE pb*/
	{0x00b5, 0x0060},    /*DE ne*/
	{0x00b6, 0x0060},    /*DE nf*/
	{0x00b7, 0x0060},    /*DE nb*/
	{0x00b8, 0x1000},    /*DE max ratio*/
	{0x00b9, 0x0100},    /*DE min ratio*/
	{0x00c0, 0x1010},    /*CS hg ry*/
	{0x00c1, 0x1010},    /*CS hg gc*/
	{0x00c2, 0x1010},    /*CS hg bm*/
	{0x00c3, 0x1604},    /*CS weight grayTH*/
	{0x0000, 0x0001},    /*BANK 1*/
	{0x0071, 0xf000},    /*SCR RrCr*/
	{0x0072, 0x00f0},    /*SCR RgCg*/
	{0x0073, 0x00f0},    /*SCR RbCb*/
	{0x0074, 0x00f0},    /*SCR GrMr*/
	{0x0075, 0xf000},    /*SCR GgMg*/
	{0x0076, 0x00f0},    /*SCR GbMb*/
	{0x0077, 0x00f0},    /*SCR BrYr*/
	{0x0078, 0x00f0},    /*SCR BgYg*/
	{0x0079, 0xf000},    /*SCR BbYb*/
	{0x007a, 0x00ff},    /*SCR KrWr*/
	{0x007b, 0x00ff},    /*SCR KgWg*/
	{0x007c, 0x00ff},    /*SCR KbWb*/
	{0x00ff, 0x0000},    /*Mask Release*/
/*end*/
};

static const struct cmc624_register_set standard_dmb_cabcoff[] = {
/*start P8 standard dmb*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0036},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0080, 0x1001},	/*NR col con ring bl*/
	{0x0081, 0x3f04},	/*NR blRedTH maxmin*/
	{0x0082, 0x1006},	/*NR edgeTH blDecTH*/
	{0x0083, 0x3f04},	/*NR conRedTH maxmin*/
	{0x0090, 0x0430},	/*WIENER br*/
	{0x0091, 0x0040},	/*WIENER hf*/
	{0x0092, 0x1000},	/*WIENER lf*/
	{0x00b2, 0x0100},	/*DE pe*/
	{0x00b3, 0x0100},	/*DE pf*/
	{0x00b4, 0x0100},	/*DE pb*/
	{0x00b5, 0x0100},	/*DE ne*/
	{0x00b6, 0x0100},	/*DE nf*/
	{0x00b7, 0x0100},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0010},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1804},	/*CS weight grayTH*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set standard_ui_cabcoff[] = {
/*start Palau standard ui*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0020},	/*DE pe*/
	{0x00b3, 0x0020},	/*DE pf*/
	{0x00b4, 0x0020},	/*DE pb*/
	{0x00b5, 0x0020},	/*DE ne*/
	{0x00b6, 0x0020},	/*DE nf*/
	{0x00b7, 0x0020},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2004},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};
static const struct cmc624_register_set standard_ui_cabcon[] = {
/*start D2 standard ui lowpower*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0020},	/*DE pe*/
	{0x00b3, 0x0020},	/*DE pf*/
	{0x00b4, 0x0020},	/*DE pb*/
	{0x00b5, 0x0020},	/*DE ne*/
	{0x00b6, 0x0020},	/*DE nf*/
	{0x00b7, 0x0020},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1a04},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xf000},	/*SCR RrCr*/
	{0x0072, 0x00f0},	/*SCR RgCg*/
	{0x0073, 0x00f0},	/*SCR RbCb*/
	{0x0074, 0x00f0},	/*SCR GrMr*/
	{0x0075, 0xf000},	/*SCR GgMg*/
	{0x0076, 0x00f0},	/*SCR GbMb*/
	{0x0077, 0x00f0},	/*SCR BrYr*/
	{0x0078, 0x00f0},	/*SCR BgYg*/
	{0x0079, 0xf000},	/*SCR BbYb*/
	{0x007a, 0x00f0},	/*SCR KrWr*/
	{0x007b, 0x00f0},	/*SCR KgWg*/
	{0x007c, 0x00f0},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set standard_gallery_cabcoff[] = {
/* start Palau standard gallery */
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0060},	/*DE pe*/
	{0x00b3, 0x0060},	/*DE pf*/
	{0x00b4, 0x0060},	/*DE pb*/
	{0x00b5, 0x0060},	/*DE ne*/
	{0x00b6, 0x0060},	/*DE nf*/
	{0x00b7, 0x0060},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2004},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00f0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/* end */
};

static const struct cmc624_register_set standard_vtcall_cabcoff[] = {
/*start Palau standard vtcall*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0232},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0080, 0x1111},	/*NR col con ring bl*/
	{0x0081, 0x3f04},	/*NR blRedTH maxmin*/
	{0x0082, 0x1004},	/*NR edgeTH blDecTH*/
	{0x0083, 0x3f04},	/*NR conRedTH maxmin*/
	{0x00b2, 0x00c0},	/*DE pe*/
	{0x00b3, 0x00c0},	/*DE pf*/
	{0x00b4, 0x00c0},	/*DE pb*/
	{0x00b5, 0x00c0},	/*DE ne*/
	{0x00b6, 0x00c0},	/*DE nf*/
	{0x00b7, 0x00c0},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2004},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set movie_video_cabcoff[] = {
/*start Palau movie video*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0200},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00f0},	/*SCR KgWg*/
	{0x007c, 0x00e6},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set movie_dmb_cabcoff[] = {
/*start P8 movie dmb*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0232},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0080, 0x1001},	/*NR col con ring bl*/
	{0x0081, 0x3f04},	/*NR blRedTH maxmin*/
	{0x0082, 0x1004},	/*NR edgeTH blDecTH*/
	{0x0083, 0x3f04},	/*NR conRedTH maxmin*/
	{0x00b2, 0x0080},	/*DE pe*/
	{0x00b3, 0x0080},	/*DE pf*/
	{0x00b4, 0x0080},	/*DE pb*/
	{0x00b5, 0x0080},	/*DE ne*/
	{0x00b6, 0x0080},	/*DE nf*/
	{0x00b7, 0x0080},	/*DE nb*/
	/*{0x00b8, 0x1000},*/	/*DE max ratio*/
	{0x00b9, 0x0010},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1404},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xd5af},	/*SCR RrCr*/
	{0x0072, 0x37ff},	/*SCR RgCg*/
	{0x0073, 0x30fb},	/*SCR RbCb*/
	{0x0074, 0xa5fe},	/*SCR GrMr*/
	{0x0075, 0xff4d},	/*SCR GgMg*/
	{0x0076, 0x63ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00f7},	/*SCR BgYg*/
	{0x0079, 0xff69},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00f8},	/*SCR KgWg*/
	{0x007c, 0x00f0},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set movie_ui_cabcoff[] = {
/*start Palau movie ui*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0200},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00f0},	/*SCR KgWg*/
	{0x007c, 0x00e6},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set movie_gallery_cabcoff[] = {
/*start Palau movie gallery*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0200},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00f0},	/*SCR KgWg*/
	{0x007c, 0x00e6},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set movie_vtcall_cabcoff[] = {
/*start Palau movie vtcall*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0232},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0080, 0x1111},	/*NR col con ring bl*/
	{0x0081, 0x3f04},	/*NR blRedTH maxmin*/
	{0x0082, 0x1004},	/*NR edgeTH blDecTH*/
	{0x0083, 0x3f04},	/*NR conRedTH maxmin*/
	{0x00b2, 0x0040},	/*DE pe*/
	{0x00b3, 0x0040},	/*DE pf*/
	{0x00b4, 0x0040},	/*DE pb*/
	{0x00b5, 0x0040},	/*DE ne*/
	{0x00b6, 0x0040},	/*DE nf*/
	{0x00b7, 0x0040},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1204},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00f0},	/*SCR KgWg*/
	{0x007c, 0x00e6},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set natural_video_cabcoff[] = {
/*start D2 natural video*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0060},	/*DE pe*/
	{0x00b3, 0x0060},	/*DE pf*/
	{0x00b4, 0x0060},	/*DE pb*/
	{0x00b5, 0x0060},	/*DE ne*/
	{0x00b6, 0x0060},	/*DE nf*/
	{0x00b7, 0x0060},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1804},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xd9a9},	/*SCR RrCr*/
	{0x0072, 0x35ff},	/*SCR RgCg*/
	{0x0073, 0x2bf2},	/*SCR RbCb*/
	{0x0074, 0x9fff},	/*SCR GrMr*/
	{0x0075, 0xff3f},	/*SCR GgMg*/
	{0x0076, 0x58ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00fa},	/*SCR BgYg*/
	{0x0079, 0xff5f},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00fa},	/*SCR KgWg*/
	{0x007c, 0x00f8},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set natural_dmb_cabcoff[] = {
/*start P8 natural dmb*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0232},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0080, 0x1001},	/*NR col con ring bl*/
	{0x0081, 0x3f04},	/*NR blRedTH maxmin*/
	{0x0082, 0x1004},	/*NR edgeTH blDecTH*/
	{0x0083, 0x3f04},	/*NR conRedTH maxmin*/
	{0x00b2, 0x00c0},	/*DE pe*/
	{0x00b3, 0x00c0},	/*DE pf*/
	{0x00b4, 0x00c0},	/*DE pb*/
	{0x00b5, 0x00c0},	/*DE ne*/
	{0x00b6, 0x00c0},	/*DE nf*/
	{0x00b7, 0x00c0},	/*DE nb*/
	/*{0x00b8, 0x1000},*/	/*DE max ratio*/
	{0x00b9, 0x0010},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1804},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xd5af},	/*SCR RrCr*/
	{0x0072, 0x37ff},	/*SCR RgCg*/
	{0x0073, 0x30fb},	/*SCR RbCb*/
	{0x0074, 0xa5fe},	/*SCR GrMr*/
	{0x0075, 0xff4d},	/*SCR GgMg*/
	{0x0076, 0x63ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00f7},	/*SCR BgYg*/
	{0x0079, 0xff69},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00fa},	/*SCR KgWg*/
	{0x007c, 0x00f8},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set natural_ui_cabcoff[] = {
/*start D2 natural ui*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0020},	/*DE pe*/
	{0x00b3, 0x0020},	/*DE pf*/
	{0x00b4, 0x0020},	/*DE pb*/
	{0x00b5, 0x0020},	/*DE ne*/
	{0x00b6, 0x0020},	/*DE nf*/
	{0x00b7, 0x0020},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1804},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xd9a9},	/*SCR RrCr*/
	{0x0072, 0x35ff},	/*SCR RgCg*/
	{0x0073, 0x2bf2},	/*SCR RbCb*/
	{0x0074, 0x9fff},	/*SCR GrMr*/
	{0x0075, 0xff3f},	/*SCR GgMg*/
	{0x0076, 0x58ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00fa},	/*SCR BgYg*/
	{0x0079, 0xff5f},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00fa},	/*SCR KgWg*/
	{0x007c, 0x00f8},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set natural_gallery_cabcoff[] = {
/*start D2 natural gallery*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0060},	/*DE pe*/
	{0x00b3, 0x0060},	/*DE pf*/
	{0x00b4, 0x0060},	/*DE pb*/
	{0x00b5, 0x0060},	/*DE ne*/
	{0x00b6, 0x0060},	/*DE nf*/
	{0x00b7, 0x0060},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1804},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xd9a9},	/*SCR RrCr*/
	{0x0072, 0x35ff},	/*SCR RgCg*/
	{0x0073, 0x2bf2},	/*SCR RbCb*/
	{0x0074, 0x9fff},	/*SCR GrMr*/
	{0x0075, 0xff3f},	/*SCR GgMg*/
	{0x0076, 0x58ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00fa},	/*SCR BgYg*/
	{0x0079, 0xff5f},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00fa},	/*SCR KgWg*/
	{0x007c, 0x00f8},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set natural_vtcall_cabcoff[] = {
/* start D2 natural vtcall */
	{0x0000, 0x0000},	/*BANK 0 */
	{0x0008, 0x0232},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0080, 0x1111},	/*NR col con ring bl*/
	{0x0081, 0x3f04},	/*NR blRedTH maxmin*/
	{0x0082, 0x1006},	/*NR edgeTH blDecTH*/
	{0x0083, 0x3f04},	/*NR conRedTH maxmin*/
	{0x00b2, 0x00c0},	/*DE pe*/
	{0x00b3, 0x00c0},	/*DE pf*/
	{0x00b4, 0x00c0},	/*DE pb*/
	{0x00b5, 0x00c0},	/*DE ne*/
	{0x00b6, 0x00c0},	/*DE nf*/
	{0x00b7, 0x00c0},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0010},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1804},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xd9a9},	/*SCR RrCr*/
	{0x0072, 0x35ff},	/*SCR RgCg*/
	{0x0073, 0x2bf2},	/*SCR RbCb*/
	{0x0074, 0x9fff},	/*SCR GrMr*/
	{0x0075, 0xff3f},	/*SCR GgMg*/
	{0x0076, 0x58ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00fa},	/*SCR BgYg*/
	{0x0079, 0xff5f},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00fa},	/*SCR KgWg*/
	{0x007c, 0x00f8},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set dynamic_video_cabcoff[] = {
/*start Palau dynamic video*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0330},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0080},	/*DE pe*/
	{0x00b3, 0x0080},	/*DE pf*/
	{0x00b4, 0x0080},	/*DE pb*/
	{0x00b5, 0x0080},	/*DE ne*/
	{0x00b6, 0x0080},	/*DE nf*/
	{0x00b7, 0x0080},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2204},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x003f, 0x0080},	/*CC chsel strength*/
	{0x0040, 0x0000},	/*CC lut r	0*/
	{0x0041, 0x0a94},	/*CC lut r	16 144*/
	{0x0042, 0x18a6},	/*CC lut r	32 160*/
	{0x0043, 0x28b8},	/*CC lut r	48 176*/
	{0x0044, 0x3ac9},	/*CC lut r	64 192*/
	{0x0045, 0x4cd9},	/*CC lut r	80 208*/
	{0x0046, 0x5ee7},	/*CC lut r	96 224*/
	{0x0047, 0x70f4},	/*CC lut r 112 240*/
	{0x0048, 0x82ff},	/*CC lut r 128 255*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set dynamic_dmb_cabcoff[] = {
/*start P8 dynamic dmb*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0132},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0080, 0x1001},	/*NR col con ring bl*/
	{0x0081, 0x3f04},	/*NR blRedTH maxmin*/
	{0x0082, 0x1004},	/*NR edgeTH blDecTH*/
	{0x0083, 0x3f04},	/*NR conRedTH maxmin*/
	{0x00b2, 0x00e0},	/*DE pe*/
	{0x00b3, 0x00e0},	/*DE pf*/
	{0x00b4, 0x00e0},	/*DE pb*/
	{0x00b5, 0x00e0},	/*DE ne*/
	{0x00b6, 0x00e0},	/*DE nf*/
	{0x00b7, 0x00e0},	/*DE nb*/
	/*{0x00b8, 0x1000},*/	/*DE max ratio*/
	{0x00b9, 0x0010},	/*DE min ratio*/
	{0x00c0, 0x0808},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2804},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x003f, 0x0080},	/*CC chsel strength*/
	{0x0040, 0x0000},	/*CC lut r  0*/
	{0x0041, 0x0d93},	/*CC lut r  16 144*/
	{0x0042, 0x1aa5},	/*CC lut r  32 160*/
	{0x0043, 0x29b7},	/*CC lut r  48 176*/
	{0x0044, 0x39c8},	/*CC lut r  64 192*/
	{0x0045, 0x4bd8},	/*CC lut r  80 208*/
	{0x0046, 0x5de6},	/*CC lut r  96 224*/
	{0x0047, 0x6ff4},	/*CC lut r 112 240*/
	{0x0048, 0x81ff},	/*CC lut r 128 255*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set dynamic_ui_cabcoff[] = {
/*start Palau dynamic ui*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0330},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0040},	/*DE pe*/
	{0x00b3, 0x0040},	/*DE pf*/
	{0x00b4, 0x0040},	/*DE pb*/
	{0x00b5, 0x0040},	/*DE ne*/
	{0x00b6, 0x0040},	/*DE nf*/
	{0x00b7, 0x0040},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2204},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x003f, 0x0080},	/*CC chsel strength*/
	{0x0040, 0x0000},	/*CC lut r	0*/
	{0x0041, 0x0a94},	/*CC lut r	16 144*/
	{0x0042, 0x18a6},	/*CC lut r	32 160*/
	{0x0043, 0x28b8},	/*CC lut r	48 176*/
	{0x0044, 0x3ac9},	/*CC lut r	64 192*/
	{0x0045, 0x4cd9},	/*CC lut r	80 208*/
	{0x0046, 0x5ee7},	/*CC lut r	96 224*/
	{0x0047, 0x70f4},	/*CC lut r 112 240*/
	{0x0048, 0x82ff},	/*CC lut r 128 255*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set dynamic_gallery_cabcoff[] = {
/*start Palau dynamic gallery*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0330},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0080},	/*DE pe*/
	{0x00b3, 0x0080},	/*DE pf*/
	{0x00b4, 0x0080},	/*DE pb*/
	{0x00b5, 0x0080},	/*DE ne*/
	{0x00b6, 0x0080},	/*DE nf*/
	{0x00b7, 0x0080},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2204},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x003f, 0x0080},	/*CC chsel strength*/
	{0x0040, 0x0000},	/*CC lut r	0*/
	{0x0041, 0x0a94},	/*CC lut r	16 144*/
	{0x0042, 0x18a6},	/*CC lut r	32 160*/
	{0x0043, 0x28b8},	/*CC lut r	48 176*/
	{0x0044, 0x3ac9},	/*CC lut r	64 192*/
	{0x0045, 0x4cd9},	/*CC lut r	80 208*/
	{0x0046, 0x5ee7},	/*CC lut r	96 224*/
	{0x0047, 0x70f4},	/*CC lut r 112 240*/
	{0x0048, 0x82ff},	/*CC lut r 128 255*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set dynamic_vtcall_cabcoff[] = {
/*start Palau dynamic vtcall*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0330},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0080, 0x1111},	/*NR col con ring bl*/
	{0x0081, 0x3f04},	/*NR blRedTH maxmin*/
	{0x0082, 0x1004},	/*NR edgeTH blDecTH*/
	{0x0083, 0x3f04},	/*NR conRedTH maxmin*/
	{0x00b2, 0x00e0},	/*DE pe*/
	{0x00b3, 0x00e0},	/*DE pf*/
	{0x00b4, 0x00e0},	/*DE pb*/
	{0x00b5, 0x00e0},	/*DE ne*/
	{0x00b6, 0x00e0},	/*DE nf*/
	{0x00b7, 0x00e0},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2204},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x003f, 0x0080},	/*CC chsel strength*/
	{0x0040, 0x0000},	/*CC lut r	0*/
	{0x0041, 0x0a94},	/*CC lut r	16 144*/
	{0x0042, 0x18a6},	/*CC lut r	32 160*/
	{0x0043, 0x28b8},	/*CC lut r	48 176*/
	{0x0044, 0x3ac9},	/*CC lut r	64 192*/
	{0x0045, 0x4cd9},	/*CC lut r	80 208*/
	{0x0046, 0x5ee7},	/*CC lut r	96 224*/
	{0x0047, 0x70f4},	/*CC lut r 112 240*/
	{0x0048, 0x82ff},	/*CC lut r 128 255*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set camera_cabcoff[] = {
/*start Palau camera*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0060},	/*DE pe*/
	{0x00b3, 0x0060},	/*DE pf*/
	{0x00b4, 0x0060},	/*DE pb*/
	{0x00b5, 0x0060},	/*DE ne*/
	{0x00b6, 0x0060},	/*DE nf*/
	{0x00b7, 0x0060},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x2004},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};
static const struct cmc624_register_set camera_cabcon[] = {
/* start D2 camera lowpower*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0060},	/*DE pe*/
	{0x00b3, 0x0060},	/*DE pf*/
	{0x00b4, 0x0060},	/*DE pb*/
	{0x00b5, 0x0060},	/*DE ne*/
	{0x00b6, 0x0060},	/*DE nf*/
	{0x00b7, 0x0060},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg ry*/
	{0x00c1, 0x1010},	/*CS hg gc*/
	{0x00c2, 0x1010},	/*CS hg bm*/
	{0x00c3, 0x1204},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xf800},	/*SCR RrCr*/
	{0x0072, 0x00f8},	/*SCR RgCg*/
	{0x0073, 0x00f8},	/*SCR RbCb*/
	{0x0074, 0x00f8},	/*SCR GrMr*/
	{0x0075, 0xf800},	/*SCR GgMg*/
	{0x0076, 0x00f8},	/*SCR GbMb*/
	{0x0077, 0x00f8},	/*SCR BrYr*/
	{0x0078, 0x00f8},	/*SCR BgYg*/
	{0x0079, 0xf800},	/*SCR BbYb*/
	{0x007a, 0x00f8},	/*SCR KrWr*/
	{0x007b, 0x00f8},	/*SCR KgWg*/
	{0x007c, 0x00f8},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set camera_ove[] = {
/*start Palau camera outdoor*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0230},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0001},	/*UC*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x00b2, 0x0060},	/*DE pe*/
	{0x00b3, 0x0060},	/*DE pf*/
	{0x00b4, 0x0060},	/*DE pb*/
	{0x00b5, 0x0060},	/*DE ne*/
	{0x00b6, 0x0060},	/*DE nf*/
	{0x00b7, 0x0060},	/*DE nb*/
	{0x00b8, 0x1000},	/*DE max ratio*/
	{0x00b9, 0x0100},	/*DE min ratio*/
	{0x00c0, 0x1010},	/*CS hg RY*/
	{0x00c1, 0x1010},	/*CS hg GC*/
	{0x00c2, 0x1010},	/*CS hg BM*/
	{0x00c3, 0x2004},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xff00},	/*SCR RrCr*/
	{0x0072, 0x00ff},	/*SCR RgCg*/
	{0x0073, 0x00ff},	/*SCR RbCb*/
	{0x0074, 0x00ff},	/*SCR GrMr*/
	{0x0075, 0xff00},	/*SCR GgMg*/
	{0x0076, 0x00ff},	/*SCR GbMb*/
	{0x0077, 0x00ff},	/*SCR BrYr*/
	{0x0078, 0x00d0},	/*SCR BgYg*/
	{0x0079, 0xff00},	/*SCR BbYb*/
	{0x007a, 0x00ff},	/*SCR KrWr*/
	{0x007b, 0x00ff},	/*SCR KgWg*/
	{0x007c, 0x00ff},	/*SCR KbWb*/
	{0x00e0, 0x01c0},	/*UC y*/
	{0x00e1, 0x01ff},	/*UC cs*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set cold_ove_cabcoff[] = {
/*start Palau cold outdoor*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0009, 0x0001},	/*MCM*/
	{0x000a, 0x0001},	/*UC*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0001, 0x0064},	/*MCM 10000K*/
	{0x0006, 0xffff},	/*MCM LSF4 LSF5*/
	{0x0009, 0xa08e},	/*MCM 5cb 1cr W*/
	{0x000b, 0x7979},	/*MCM 4cr 5cr W*/
	{0x000e, 0x8080},	/*MCM 5cb 1cr K*/
	{0x0010, 0x8080},	/*MCM 4cr 5cr K*/
	{0x0011, 0x8080},	/*MCM gray axis*/
	{0x00e0, 0x01c0},	/*UC y*/
	{0x00e1, 0x01ff},	/*UC cs*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set cold_cabcoff[] = {
/*start Palau cold*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0009, 0x0001},	/*MCM*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0001, 0x0064},	/*MCM 10000K*/
	{0x0006, 0xffff},	/*MCM LSF4 LSF5*/
	{0x0009, 0xa08e},	/*MCM 5cb 1cr W*/
	{0x000b, 0x7979},	/*MCM 4cr 5cr W*/
	{0x000e, 0x8080},	/*MCM 5cb 1cr K*/
	{0x0010, 0x8080},	/*MCM 4cr 5cr K*/
	{0x0011, 0x8080},	/*MCM gray axis*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set warm_ove_cabcoff[] = {
/*start Palau warm outdoor*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0009, 0x0001},	/*MCM*/
	{0x000a, 0x0001},	/*UC*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0001, 0x0028},	/*MCM 4000K*/
	{0x0004, 0x64ff},	/*MCM CT5 LSF1*/
	{0x0007, 0x7575},	/*MCM 1cb 2cb W*/
	{0x0009, 0xa08e},	/*MCM 5cb 1cr W*/
	{0x000c, 0x8080},	/*MCM 1cb 2cb K*/
	{0x000e, 0x8080},	/*MCM 5cb 1cr K*/
	{0x0011, 0x8080},	/*MCM gray axis*/
	{0x00e0, 0x01c0},	/*UC y*/
	{0x00e1, 0x01ff},	/*UC cs*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set warm_cabcoff[] = {
/*start Palau warm*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0009, 0x0001},	/*MCM*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0001, 0x0028},	/*MCM 4000K*/
	{0x0004, 0x64ff},	/*MCM CT5 LSF1*/
	{0x0007, 0x7575},	/*MCM 1cb 2cb W*/
	{0x0009, 0xa08e},	/*MCM 5cb 1cr W*/
	{0x000c, 0x8080},	/*MCM 1cb 2cb K*/
	{0x000e, 0x8080},	/*MCM 5cb 1cr K*/
	{0x0011, 0x8080},	/*MCM gray axis*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set ove_cabcoff[] = {
/*start Palau outdoor*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0001},	/*UC*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x00e0, 0x01c0},	/*UC y*/
	{0x00e1, 0x01ff},	/*UC cs*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set browser_tone1_tune[] = {
/*start P8 browser tone 1*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0200},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xaf00},	/*SCR RrCr*/
	{0x0072, 0x00b7},	/*SCR RgCg*/
	{0x0073, 0x00bc},	/*SCR RbCb*/
	{0x0074, 0x00af},	/*SCR GrMr*/
	{0x0075, 0xb700},	/*SCR GgMg*/
	{0x0076, 0x00bc},	/*SCR GbMb*/
	{0x0077, 0x00af},	/*SCR BrYr*/
	{0x0078, 0x00b7},	/*SCR BgYg*/
	{0x0079, 0xbc00},	/*SCR BbYb*/
	{0x007a, 0x00af},	/*SCR KrWr*/
	{0x007b, 0x00b7},	/*SCR KgWg*/
	{0x007c, 0x00bc},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set browser_tone2_tune[] = {
/*start P8 browser tone 2*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0200},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0xa000},	/*SCR RrCr*/
	{0x0072, 0x00a8},	/*SCR RgCg*/
	{0x0073, 0x00b2},	/*SCR RbCb*/
	{0x0074, 0x00a0},	/*SCR GrMr*/
	{0x0075, 0xa800},	/*SCR GgMg*/
	{0x0076, 0x00b2},	/*SCR GbMb*/
	{0x0077, 0x00a0},	/*SCR BrYr*/
	{0x0078, 0x00a8},	/*SCR BgYg*/
	{0x0079, 0xb200},	/*SCR BbYb*/
	{0x007a, 0x00a0},	/*SCR KrWr*/
	{0x007b, 0x00a8},	/*SCR KgWg*/
	{0x007c, 0x00b2},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set browser_tone3_tune[] = {
/*start P8 browser tone 3*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0200},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0x9100},	/*SCR RrCr*/
	{0x0072, 0x0099},	/*SCR RgCg*/
	{0x0073, 0x00a3},	/*SCR RbCb*/
	{0x0074, 0x0091},	/*SCR GrMr*/
	{0x0075, 0x9900},	/*SCR GgMg*/
	{0x0076, 0x00a3},	/*SCR GbMb*/
	{0x0077, 0x0091},	/*SCR BrYr*/
	{0x0078, 0x0099},	/*SCR BgYg*/
	{0x0079, 0xa300},	/*SCR BbYb*/
	{0x007a, 0x0091},	/*SCR KrWr*/
	{0x007b, 0x0099},	/*SCR KgWg*/
	{0x007c, 0x00a3},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
/*end*/
};

static const struct cmc624_register_set cmc624_tune_dmb_test[] = {
	/*start P8 dmb test*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0136},	/*CC CS DE*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0082, 0x1005},	/*edgeTH BLDecTH*/
	{0x0083, 0x3f00},	/*ConRedTH maxmin*/
	{0x0090, 0x0420},	/*Wiener Br*/
	{0x0091, 0x0500},	/*Wiener hf*/
	{0x00b2, 0x00a0},	/*DE pe*/
	{0x00b3, 0x0030},	/*DE pf*/
	{0x00b4, 0x0028},	/*DE pb*/
	{0x00b5, 0x00a0},	/*DE ne*/
	{0x00b6, 0x0030},	/*DE nf*/
	{0x00b7, 0x0028},	/*DE nb*/
	{0x00c0, 0x1010},	/*hg RY*/
	{0x00c1, 0x1010},	/*hg GC*/
	{0x00c2, 0x1010},	/*hg BM*/
	{0x00c3, 0x1804},	/*CS weight grayTH*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x003f, 0x0080},	/*CC chsel strength*/
	{0x0040, 0x0000},
	{0x0041, 0x1090},	/*0x0c96,*/
	{0x0042, 0x20a0},	/*0x18a8,*/
	{0x0043, 0x30b0},	/*0x2aba,*/
	{0x0044, 0x40c0},	/*0x3ccc,*/
	{0x0045, 0x50d0},	/*0x4edc,*/
	{0x0046, 0x60e0},	/*0x60ea,*/
	{0x0047, 0x70F0},	/*0x72f6,*/
	{0x0048, 0x80ff},	/*0x84ff,*/
	{0x00ff, 0x0000},	/*Mask Release*/
	/*end*/
};
static const struct cmc624_register_set cmc624_tune_tone_reversal[] = {
	/*start D2 tone reversal*/
	{0x0000, 0x0000},	/*BANK 0*/
	{0x0008, 0x0200},	/*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1*/
	{0x0009, 0x0000},	/*MCM off*/
	{0x000a, 0x0000},	/*UC off*/
	{0x0030, 0x0000},	/*FA cs1 de8 hdr2 fa1*/
	{0x0000, 0x0001},	/*BANK 1*/
	{0x0071, 0x00ff},	/*SCR RrCr*/
	{0x0072, 0xff00},	/*SCR RgCg*/
	{0x0073, 0xff00},	/*SCR RbCb*/
	{0x0074, 0xff00},	/*SCR GrMr*/
	{0x0075, 0x00ff},	/*SCR GgMg*/
	{0x0076, 0xff00},	/*SCR GbMb*/
	{0x0077, 0xff00},	/*SCR BrYr*/
	{0x0078, 0xff00},	/*SCR BgYg*/
	{0x0079, 0x00ff},	/*SCR BbYb*/
	{0x007a, 0xff00},	/*SCR KrWr*/
	{0x007b, 0xff00},	/*SCR KgWg*/
	{0x007c, 0xff00},	/*SCR KbWb*/
	{0x00ff, 0x0000},	/*Mask Release*/
	/*end*/
};

static int palau_init_tune_list(void)
{
	u32 id;

	/* init sequence list */
	TUNE_DATA_ID(id, MENU_CMD_INIT, MAX_BACKGROUND_MODE, MAX_mDNIe_MODE,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MAX_CABC_MODE, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, cmc624_init_seq,
					ARRAY_SIZE(cmc624_init_seq));

	TUNE_DATA_ID(id, MENU_CMD_INIT_LDI, MAX_BACKGROUND_MODE, MAX_mDNIe_MODE,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MAX_CABC_MODE, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, sony_init_seq,
					ARRAY_SIZE(sony_init_seq));

	/* tuning sequence list */
	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VIDEO,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, standard_video_cabcoff,
					ARRAY_SIZE(standard_video_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VIDEO,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_ON, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, standard_video_cabcon,
					ARRAY_SIZE(standard_video_cabcon));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_DMB,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, standard_dmb_cabcoff,
					ARRAY_SIZE(standard_dmb_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_UI,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, standard_ui_cabcoff,
					ARRAY_SIZE(standard_ui_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_UI,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_ON, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, standard_ui_cabcon,
					ARRAY_SIZE(standard_ui_cabcon));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_GALLERY,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, standard_gallery_cabcoff,
					ARRAY_SIZE(standard_gallery_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_STANDARD, MENU_APP_VT,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, standard_vtcall_cabcoff,
					ARRAY_SIZE(standard_vtcall_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_VIDEO,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, movie_video_cabcoff,
					ARRAY_SIZE(movie_video_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_DMB,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, movie_dmb_cabcoff,
					ARRAY_SIZE(movie_dmb_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_UI,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, movie_ui_cabcoff,
					ARRAY_SIZE(movie_ui_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_GALLERY,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, movie_gallery_cabcoff,
					ARRAY_SIZE(movie_gallery_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_MOVIE, MENU_APP_VT,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, movie_vtcall_cabcoff,
					ARRAY_SIZE(movie_vtcall_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_NATURAL, MENU_APP_VIDEO,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, natural_video_cabcoff,
					ARRAY_SIZE(natural_video_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_NATURAL, MENU_APP_DMB,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, natural_dmb_cabcoff,
					ARRAY_SIZE(natural_dmb_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_NATURAL, MENU_APP_UI,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, natural_ui_cabcoff,
					ARRAY_SIZE(natural_ui_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_NATURAL, MENU_APP_GALLERY,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, natural_gallery_cabcoff,
					ARRAY_SIZE(natural_gallery_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_NATURAL, MENU_APP_VT,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, natural_vtcall_cabcoff,
					ARRAY_SIZE(natural_vtcall_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VIDEO,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, dynamic_video_cabcoff,
					ARRAY_SIZE(dynamic_video_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_DMB,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, dynamic_dmb_cabcoff,
					ARRAY_SIZE(dynamic_dmb_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_UI,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, dynamic_ui_cabcoff,
					ARRAY_SIZE(dynamic_ui_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_GALLERY,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, dynamic_gallery_cabcoff,
					ARRAY_SIZE(dynamic_gallery_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_DYNAMIC, MENU_APP_VT,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, dynamic_vtcall_cabcoff,
					ARRAY_SIZE(dynamic_vtcall_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MENU_APP_CAMERA,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, camera_cabcoff,
					ARRAY_SIZE(camera_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MENU_APP_CAMERA,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MENU_CABC_ON, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, camera_cabcon,
					ARRAY_SIZE(camera_cabcon));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MAX_mDNIe_MODE,
		MENU_TEMP_COLD, MENU_OUT_OFF, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, cold_cabcoff,
					ARRAY_SIZE(cold_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MAX_mDNIe_MODE,
		MENU_TEMP_WARM, MENU_OUT_ON, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, warm_ove_cabcoff,
					ARRAY_SIZE(warm_ove_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MAX_mDNIe_MODE,
		MENU_TEMP_WARM, MENU_OUT_OFF, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, warm_cabcoff,
					ARRAY_SIZE(warm_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MAX_mDNIe_MODE,
		MENU_TEMP_NORMAL, MENU_OUT_ON, MENU_CABC_OFF, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, ove_cabcoff,
					ARRAY_SIZE(ove_cabcoff));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MENU_APP_BROWSER,
	MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MAX_CABC_MODE, MENU_SPEC_TONE1);
	cmc624_register_tune_data(id, browser_tone1_tune,
					ARRAY_SIZE(browser_tone1_tune));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MENU_APP_BROWSER,
	MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MAX_CABC_MODE, MENU_SPEC_TONE2);
	cmc624_register_tune_data(id, browser_tone2_tune,
					ARRAY_SIZE(browser_tone2_tune));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MENU_APP_BROWSER,
	MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MAX_CABC_MODE, MENU_SPEC_TONE3);
	cmc624_register_tune_data(id, browser_tone3_tune,
					ARRAY_SIZE(browser_tone3_tune));

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MAX_BACKGROUND_MODE, MENU_MODE_NEGATIVE,
		MAX_TEMP_MODE, MAX_OUTDOOR_MODE, MAX_CABC_MODE, COLOR_TONE_MAX);
	cmc624_register_tune_data(id, cmc624_tune_tone_reversal,
					ARRAY_SIZE(cmc624_tune_tone_reversal));

	return 0;
}

static void palau_power_lcd(bool enable)
{

	if (enable) {
		gpio_set_value(display_gpios[GPIO_LCD_EN].gpio, 1);
		gpio_set_value(display_gpios[GPIO_MLCD_RST].gpio, 1);
	} else {
		gpio_set_value(display_gpios[GPIO_LCD_EN].gpio, 0);
		gpio_set_value(display_gpios[GPIO_MLCD_RST].gpio, 0);
	}
}

static int palau_power_vima_1_1V(int on)
{
	struct regulator *reg_smpsn8;
	int ret = 0;

	reg_smpsn8 = regulator_get(NULL, "V_IMA_1.1V");
	if (IS_ERR(reg_smpsn8)) {
		pr_err("[CMC624] %s [%d] failed to get v1.1 regulator.\n",
			__func__, __LINE__);
		ret = -ENODATA;
		goto out;
	}

	if (on) {
		if (unlikely(system_rev < 1))
			gpio_set_value(display_gpios[GPIO_IMA_PWR_EN].gpio, 1);
		regulator_enable(reg_smpsn8);
	} else {
		if (unlikely(system_rev < 1))
			gpio_set_value(display_gpios[GPIO_IMA_PWR_EN].gpio, 0);
		regulator_disable(reg_smpsn8);
	}

	regulator_put(reg_smpsn8);

out:
	return ret;
}

static int palau_power_vima_1_8V(int on)
{
	struct regulator *reg_ldon27;
	int ret = 0;

	reg_ldon27 = regulator_get(NULL, "V_IMA_1.8V");
	if (IS_ERR(reg_ldon27)) {
		pr_err("[CMC624] %s [%d] failed to get v1.8 regulator.\n",
			__func__, __LINE__);
		ret = -ENODATA;
		goto out;
	}

	if (on)
		regulator_enable(reg_ldon27);
	else
		regulator_disable(reg_ldon27);

	regulator_put(reg_ldon27);

out:
	return ret;
}

static struct cmc624_panel_data palau_panel_pdata = {
	.init_tune_list		= palau_init_tune_list,
	.power_lcd		= palau_power_lcd,
	.power_vima_1_1V	= palau_power_vima_1_1V,
	.power_vima_1_8V	= palau_power_vima_1_8V,
};

static struct omap_dss_device palau_lcd_device = {
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
	},
	.panel = {
		.timings	= {
			.x_res			= 720,
			.y_res			= 1280,
			.pixel_clock	= 80842,
			.hfp			= 158,
			.hsw			= 2,
			.hbp			= 160,
			.vfp			= 13,
			.vsw			= 1,
			.vbp			= 2,
		},
		.acbi = 0,
		.acb = 40,
		.width_in_um	= 58000,
		.height_in_um	= 102000,
	},
	.clocks = {
		.dispc		= {
			.channel = {
				.lck_div	= 1,	/* LCD */
				.pck_div	= 2,	/* PCD */
				.lcd_clk_src
					= OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},
		.dsi		= {
			.regn		= 19,	/* DSI_PLL_REGN */
			.regm		= 236,	/* DSI_PLL_REGM */

			.regm_dispc	= 6,	/* PLL_CLK1 (M4) */
			.regm_dsi	= 6,	/* PLL_CLK2 (M5) */
			.lp_clk_div	= 8,	/* LPDIV */
			.offset_ddr_clk	= 122,	/* DDR PRE & DDR POST
						 * offset increase
						 */

			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},

	.channel		= OMAP_DSS_CHANNEL_LCD,
	.ctrl.pixel_size = 24,

	.skip_init              = false,
	.data = &palau_panel_pdata,
};

static struct omap_dss_device *palau_dss_devices[] = {
	&palau_lcd_device,
};

static struct omap_dss_board_info palau_dss_data = {
	.num_devices	= ARRAY_SIZE(palau_dss_devices),
	.devices	= palau_dss_devices,
	.default_device	= &palau_lcd_device,
};

static struct omapfb_platform_data palau_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = PALAU_FB_RAM_SIZE,
			},
		},
	},
};

/* cmc624 i2c board info */
static struct i2c_board_info __initdata palau_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("sec_cmc624_i2c", 0x38),
	},
};

/* cmc624 backlight platform data*/
static struct platform_device cmc624_pwm_backlight = {
	.name	= "omap_cmc624_bl",
	.id	= -1,
};

static void palau_display_gpio_init(void)
{
	int i;
	int array_size = ARRAY_SIZE(display_gpios) - 1;

	if (unlikely(system_rev < 1))
		array_size += 1;

	for (i = 0; i < array_size; i++)
		display_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(display_gpios[i].label);

	gpio_request_array(display_gpios, array_size);
}

static void palau_display_regulator_init(void)
{
	struct regulator *reg_ldon27;
	struct regulator *reg_smpsn8;
	reg_ldon27 =
		regulator_get(NULL, "V_IMA_1.8V");
	if (IS_ERR(reg_ldon27)) {
		pr_err("[CMC624] %s [%d] failed to get v1.8 regulator.\n",
			__func__, __LINE__);
		return;
	}

	reg_smpsn8 =
		regulator_get(NULL, "V_IMA_1.1V");
	if (IS_ERR(reg_smpsn8)) {
		pr_err("[CMC624] %s [%d] failed to get v1.1 regulator.\n",
			__func__, __LINE__);
		return;
	}

	regulator_enable(reg_smpsn8);
	regulator_disable(reg_smpsn8);

	regulator_enable(reg_ldon27);
	regulator_disable(reg_ldon27);

	regulator_put(reg_ldon27);
	regulator_put(reg_smpsn8);
}

void __init omap4_palau_display_memory_init(void)
{
	omap_android_display_setup(&palau_dss_data,
				   NULL,
				   NULL,
				   &palau_fb_pdata,
				   get_omap_ion_platform_data());
}

void __init omap4_palau_display_init(void)
{
	omap4_ctrl_pad_writel(0x1FF80000,
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);

	/* gpio */
	palau_display_gpio_init();

	palau_panel_pdata.gpio_ima_sleep = display_gpios[GPIO_IMA_SLEEP].gpio;
	palau_panel_pdata.gpio_ima_nrst = display_gpios[GPIO_IMA_NRST].gpio;
	palau_panel_pdata.gpio_ima_cmc_en = display_gpios[GPIO_IMA_CMC_EN].gpio;

	/* regulator */
	if (!palau_lcd_device.skip_init)
		palau_display_regulator_init();

	/* cmc624 dss */
	omap_display_init(&palau_dss_data);

	/* cmc624 i2c */
	i2c_register_board_info((system_rev < 1) ? 3 : 7,
					palau_i2c3_boardinfo,
					ARRAY_SIZE(palau_i2c3_boardinfo));

	/* backlight */
	platform_device_register(&cmc624_pwm_backlight);
}

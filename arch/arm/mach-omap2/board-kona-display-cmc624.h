/* arch/arm/mach-omap2/board-kona-display.c
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

#include <video/cmc624.h>

/* CMC624 Register Setting */
static const struct cmc624_register_set cmc624_init[] = {
	/* CLOCK_TOP */
	{0x00, 0x0002}, /* BANK 2 */
	{0x30, 0x0007}, /* S.PLL DISEN, NORMAL, P = 7   (Fout = 340.11MHz) */
	{0x31, 0x20F8}, /* S = 2, M = 248               (Fout = 340.11MHz) */
	{0x30, 0x1007}, /* S.PLL EN, NORMAL, P = 7      (Fout = 340.11MHz) */
	{0x3A, 0x0C04}, /* CLOCK DIVIDER VALUE (I2C = 12[28.34MHz], A = 4) */
	{0x3B, 0x040A}, /* CLOCK DIVIDER VALUE (M1 = 4, M2 = 10) */
	{0x3C, 0x1203}, /* CLOCK DIVIDER VALUE (B = 18, P = 3) */
	{0x3D, 0x0032}, /* CLOCK DIVIDER VALUE (PWM = 50) */
	{0x39, 0x007F}, /* CLOCK DIVIDER ENABLE */
	{0x3E, 0x2223}, /* CLOCK MUX SEL */
	{SLEEPMSEC, 1},

	{0x00, 0x0000}, /* BANK 0 */
	{0xFD, 0x0000}, /* MODULE REG MASK RELEASE */
	{0xFE, 0x0004}, /* MODULE REG MASK RELEASE */

	{0xFF, 0x0000}, /* REG MASK RELEASE */

	/* INPUT IF */
	{0x00, 0x0002}, /* BANK 2 */
	/* RGB BOTTOM ALIGNMENT, VSYNC/HSYNC LOW ACTIVE, DE HIHG ACTIVE */
	{0x50, 0x0061},

	/* TCON */
	{0x00, 0x0002}, /* BANK 2 */
	{0x60, 0x5400}, /* TCON VSYNC DELAY */

	{0x63, 0x0810}, /* OUTPUT COLOR MAP, TCON OUTPUT POL(LLH) */

	{0x67, 0x0006}, /* VSYNC PULSE WIDTH = 6 */
	{0x68, 0x0012}, /* HSYNC PULSE WIDTH = 18 */
	{0x69, 0x0003}, /* VBP = 3 */
	{0x6A, 0x0003}, /* VFP = 3 */
	{0x6B, 0x0011}, /* HBP = 17 */
	{0x6C, 0x0011}, /* HFP = 17 */
	{0x66, 0x8000}, /* CDC */

	/* IP */
	{0x00, 0x0000}, /* BANK 0 */
	{0x01, 0x0077}, /* HSYNC/DE MASK RELEASE */
	{0x03, 0x0500}, /* WIDTH = 1280 */
	{0x04, 0x0320}, /* HEIGHT = 800 */
	{0x08, 0x0800}, /* CABC ALGO MODULE ON */
	{0x09, 0x0000}, /* ALGO MODULE OFF */
	{0x0A, 0x0000}, /* ALGO MODULE OFF */

	/* PWM */
	{0x00, 0x0001}, /* BANK 1 */
	{0xF8, 0x0011}, /* PWM HIGH ACTIVE, USE REGISTER VALUE */
	{0xF9, 0x0333}, /* 2048 * 2 / 5 */

	{0xBB, 0x021C}, /* POWERLUT_0: 0x0547 * 2 / 5 */
	{0xBC, 0x0245}, /* POWERLUT_1: 0x05AD * 2 / 5 */
	{0xBD, 0x01FB}, /* POWERLUT_2: 0x04F5 * 2 / 5 */
	{0xBE, 0x029F}, /* POWERLUT_3: 0x068F * 2 / 5 */
	{0xBF, 0x021C}, /* POWERLUT_4: 0x0547 * 2 / 5 */
	{0xC0, 0x0204}, /* POWERLUT_5: 0x050A * 2 / 5 */
	{0xC1, 0x01DA}, /* POWERLUT_6: 0x04A3 * 2 / 5 */
	{0xC2, 0x0052}, /* POWERLUT_7: 0x00CD * 2 / 5 */

	/* CONV */
	{0x00, 0x0003}, /* BANK 3 */
	{0x01, 0x0000}, /* I2C TO MIPI */
	{0x43, 0x6000}, /* M_BAND_CTL */
	{0x42, 0x2289}, /* M_PLL SETTING (Fout = 388.80MHz) */
	{0x41, 0x8000}, /* M_PLL SETTING & M_PLL ENABLE */
	{0x44, 0x0008}, /* S_HSSETTLE */
	{0x40, 0x0030}, /* M/S CLOCK LANE ENABLE */

	{0x00, 0x0000}, /* BANK 0 */
	{0xFD, 0xFFFF}, /* MODULE REG MASK RELEASE */
	{0xFE, 0xFFFF}, /* MODULE REG MASK RELEASE */
	{0xFF, 0x0000}, /* MASK RELEASE */

	/* DSI HOST */
	{0x00, 0x0003}, /* BANK 3 */
	{0x84, 0xFFFF}, /* INTERRUPT ENABLE */

	{0x85, 0x1FEF}, /* INTERRUPT ENABLE */
	/* DSI FUNCTION(CMD  8 BIT DATA, RGB888,
	CMD VC 0, VIDEO VC 0, 4 LANE) */
	{0x86, 0x6204},
	{0x88, 0xFFFF}, /* HIGH SPEED RECEIVE TIMEOUT */
	{0x89, 0x00FF}, /* HIGH SPEED RECEIVE TIMEOUT */
	{0x8A, 0xFFFF}, /* LOW POWER RECEIVE TIMEOUT */
	{0x8B, 0x00FF}, /* LOW POWER RECEIVE TIMEOUT */
	{0x8C, 0x001F}, /* TURN AROUND TIMEOUT */
	{0x8E, 0x00FF}, /* DEVICE RESET TIMER */
	{0x90, 0x0500}, /* HORIZANTAL RESOLUTION = 1280 */
	{0x91, 0x0320}, /* VERTICAL RESOLUTION = 800 */

	{0x94, 0x000E}, /* HORIZANTAL SYNC PADDING COUNT */
	{0x96, 0x000D}, /* HORIZANTAL BACK PORCH COUNT */
	{0x98, 0x000D}, /* HORIZANTAL FRONT FORCH COUNT */
	{0x9A, 0x03C0}, /* HORIZANTAL ACTIVE AREA */

	{0x9C, 0x0006}, /* VERTICAL SYNC PADDING COUNT */
	{0x9E, 0x0003}, /* VERTICAL BACK PORCH COUNT */
	{0xA0, 0x0003}, /* VERTICAL FRONT FORCH COUNT */
	{0xA8, 0x0d07}, /* MASTER INIT TIME */
	{0xAC, 0x0002}, /* VIDEO MODE FORMAT = NON BURST SYNC EVENT */
	{0xB0, 0x0008}, /* VSYNC, HSYNC, COLOR MODE, SHUT DOWN POLARITY */

	{0xB4, 0x0004}, /* LP EQUIVALENT BYTECLK */
	/* HIGH SPEED <-> LOW POWER SWITCHING COUNTER FOR DATA LANE */
	{0xA2, 0x0011},
	/* HIGH SPEED -> LOW POWER SWITCHING COUNTER FOR CLOCK LANE */
	{0xB2, 0x000A},
	/* HIGH SPEED <- LOW POWER SWITCHING COUNTER FOR CLOCK LANE */
	{0xB3, 0x0017},

	{0x80, 0x0001}, /* DEVICE READY */
	{0xA4, 0x0002}, /* COLOR MODE OFF, DPI ON */

	/* DSI DEVICE */
	{0x00, 0x0003}, /* BANK 3 */
	{0xC4, 0xFFFF}, /* INTERRUPT ENABLE */
	{0xC5, 0x01FF}, /* INTERRUPT ENABLE */
	{0xC6, 0x0064}, /* DSI FUNCTION : BURST & NON BURST SYNC EVENT */
	{0xC8, 0xFFFF}, /* HIGH SPEED RECEIVE TIMEOUT */
	{0xC9, 0xFFFF}, /* HIGH SPEED RECEIVE TIMEOUT */
	{0xCA, 0x005E}, /* LOW POWER RECEIVE TIMEOUT */
	{0xCB, 0x0000}, /* LOW POWER RECEIVE TIMEOUT */
	{0xCC, 0x0025}, /* TURN AROUND TIMEOUT */
	{0xCE, 0x07D0}, /* DEVICE RESET TIMER */
	{0xD2, 0x0000}, /* CRC, ECC, EOT ENABLE */
	{0xD4, 0x000E}, /* HSYNC COUNT = 18 */
	{0xD5, 0x0006}, /* VSYNC COUNT = 6 */
	{0xC0, 0x0001}, /* DEVICE READY */


	{0x00, 0x0002}, /* BANK 2 */
	{0x3F, 0x011B}, /* MON_CLK : TXBYTECLKHS ( 60MHz ) */

	/* CONV */
	{0x00, 0x0003}, /* BANK 3 */
	{0x01, 0x0000}, /* I2C TO MIPI */
	{0x00, 0x0002}, /* BANK 2 */
	{0x52, 0x0001}, /* RGB IF ENABLE */
	{0x00, 0x0003}, /* BANK 3 good */
};

static const struct cmc624_register_set tune_bypass[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0000}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_camera_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0230}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0000}, /* DE pe */
	{0x00b3, 0x0060}, /* DE pf */
	{0x00b4, 0x0060}, /* DE pb */
	{0x00b5, 0x0060}, /* DE ne */
	{0x00b6, 0x0060}, /* DE nf */
	{0x00b7, 0x0060}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_camera_outdoor_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0230}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0001}, /* UC */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0000}, /* DE pe */
	{0x00b3, 0x0060}, /* DE pf */
	{0x00b4, 0x0060}, /* DE pb */
	{0x00b5, 0x0060}, /* DE ne */
	{0x00b6, 0x0060}, /* DE nf */
	{0x00b7, 0x0060}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x00e0, 0x01a0}, /* UC y */
	{0x00e1, 0x01ff}, /* UC cs */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_cold[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0009, 0x0001}, /* MCM */
	{0x000a, 0x0000}, /* UC off */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0001, 0x0064}, /* MCM 10000K */
	{0x0006, 0xf8f8}, /* MCM LSF4 LSF5 */
	{0x0009, 0x9f9f}, /* MCM 5cb 1cr W */
	{0x000b, 0x7878}, /* MCM 4cr 5cr W */
	{0x000e, 0x8080}, /* MCM 5cb 1cr K */
	{0x0010, 0x8080}, /* MCM 4cr 5cr K */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_cold_outdoor[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0009, 0x0001}, /* MCM */
	{0x000a, 0x0001}, /* UC */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0001, 0x0064}, /* MCM 10000K */
	{0x0006, 0xf8f8}, /* MCM LSF4 LSF5 */
	{0x0009, 0x9f9f}, /* MCM 5cb 1cr W */
	{0x000b, 0x7878}, /* MCM 4cr 5cr W */
	{0x000e, 0x8080}, /* MCM 5cb 1cr K */
	{0x0010, 0x8080}, /* MCM 4cr 5cr K */
	{0x00e0, 0x01a0}, /* UC y */
	{0x00e1, 0x01ff}, /* UC cs */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_dynamic_gallery_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0330}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0020}, /* DE pe */
	{0x00b3, 0x0080}, /* DE pf */
	{0x00b4, 0x0080}, /* DE pb */
	{0x00b5, 0x0080}, /* DE ne */
	{0x00b6, 0x0080}, /* DE nf */
	{0x00b7, 0x0080}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1a04}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x003f, 0x0080}, /* CC chsel strength */
	{0x0040, 0x0000}, /* CC lut r   0 */
	{0x0041, 0x0b94}, /* CC lut r  16 144 */
	{0x0042, 0x18a6}, /* CC lut r  32 160 */
	{0x0043, 0x28b8}, /* CC lut r  48 176 */
	{0x0044, 0x3ac9}, /* CC lut r  64 192 */
	{0x0045, 0x4cd9}, /* CC lut r  80 208 */
	{0x0046, 0x5ee7}, /* CC lut r  96 224 */
	{0x0047, 0x70f4}, /* CC lut r 112 240 */
	{0x0048, 0x82ff}, /* CC lut r 128 255 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_dynamic_gallery_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0b30}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0020}, /* DE pe */
	{0x00b3, 0x0080}, /* DE pf */
	{0x00b4, 0x0080}, /* DE pb */
	{0x00b5, 0x0080}, /* DE ne */
	{0x00b6, 0x0080}, /* DE nf */
	{0x00b7, 0x0080}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1a04}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x003f, 0x0080}, /* CC chsel strength */
	{0x0040, 0x0000}, /* CC lut r   0 */
	{0x0041, 0x0b94}, /* CC lut r  16 144 */
	{0x0042, 0x18a6}, /* CC lut r  32 160 */
	{0x0043, 0x28b8}, /* CC lut r  48 176 */
	{0x0044, 0x3ac9}, /* CC lut r  64 192 */
	{0x0045, 0x4cd9}, /* CC lut r  80 208 */
	{0x0046, 0x5ee7}, /* CC lut r  96 224 */
	{0x0047, 0x70f4}, /* CC lut r 112 240 */
	{0x0048, 0x82ff}, /* CC lut r 128 255 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_dynamic_ui_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0320}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1a04}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x003f, 0x0080}, /* CC chsel strength */
	{0x0040, 0x0000}, /* CC lut r   0 */
	{0x0041, 0x0b94}, /* CC lut r  16 144 */
	{0x0042, 0x18a6}, /* CC lut r  32 160 */
	{0x0043, 0x28b8}, /* CC lut r  48 176 */
	{0x0044, 0x3ac9}, /* CC lut r  64 192 */
	{0x0045, 0x4cd9}, /* CC lut r  80 208 */
	{0x0046, 0x5ee7}, /* CC lut r  96 224 */
	{0x0047, 0x70f4}, /* CC lut r 112 240 */
	{0x0048, 0x82ff}, /* CC lut r 128 255 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_dynamic_ui_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0b20}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1a04}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x003f, 0x0080}, /* CC chsel strength */
	{0x0040, 0x0000}, /* CC lut r   0 */
	{0x0041, 0x0b94}, /* CC lut r  16 144 */
	{0x0042, 0x18a6}, /* CC lut r  32 160 */
	{0x0043, 0x28b8}, /* CC lut r  48 176 */
	{0x0044, 0x3ac9}, /* CC lut r  64 192 */
	{0x0045, 0x4cd9}, /* CC lut r  80 208 */
	{0x0046, 0x5ee7}, /* CC lut r  96 224 */
	{0x0047, 0x70f4}, /* CC lut r 112 240 */
	{0x0048, 0x82ff}, /* CC lut r 128 255 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_dynamic_video_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0330}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0080}, /* DE pe */
	{0x00b3, 0x0080}, /* DE pf */
	{0x00b4, 0x0080}, /* DE pb */
	{0x00b5, 0x0080}, /* DE ne */
	{0x00b6, 0x0080}, /* DE nf */
	{0x00b7, 0x0080}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1a04}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x003f, 0x0080}, /* CC chsel strength */
	{0x0040, 0x0000}, /* CC lut r   0 */
	{0x0041, 0x0b94}, /* CC lut r  16 144 */
	{0x0042, 0x18a6}, /* CC lut r  32 160 */
	{0x0043, 0x28b8}, /* CC lut r  48 176 */
	{0x0044, 0x3ac9}, /* CC lut r  64 192 */
	{0x0045, 0x4cd9}, /* CC lut r  80 208 */
	{0x0046, 0x5ee7}, /* CC lut r  96 224 */
	{0x0047, 0x70f4}, /* CC lut r 112 240 */
	{0x0048, 0x82ff}, /* CC lut r 128 255 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_dynamic_video_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0b30}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0080}, /* DE pe */
	{0x00b3, 0x0080}, /* DE pf */
	{0x00b4, 0x0080}, /* DE pb */
	{0x00b5, 0x0080}, /* DE ne */
	{0x00b6, 0x0080}, /* DE nf */
	{0x00b7, 0x0080}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1a04}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x003f, 0x0080}, /* CC chsel strength */
	{0x0040, 0x0000}, /* CC lut r   0 */
	{0x0041, 0x0b94}, /* CC lut r  16 144 */
	{0x0042, 0x18a6}, /* CC lut r  32 160 */
	{0x0043, 0x28b8}, /* CC lut r  48 176 */
	{0x0044, 0x3ac9}, /* CC lut r  64 192 */
	{0x0045, 0x4cd9}, /* CC lut r  80 208 */
	{0x0046, 0x5ee7}, /* CC lut r  96 224 */
	{0x0047, 0x70f4}, /* CC lut r 112 240 */
	{0x0048, 0x82ff}, /* CC lut r 128 255 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_dynamic_vtcall_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0332}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0080, 0x1111}, /* NR col con ring bl */
	{0x0081, 0x3f04}, /* NR blRedTH maxmin */
	{0x0082, 0x1006}, /* NR edgeTH blDecTH */
	{0x0083, 0x3f04}, /* NR conRedTH maxmin */
	{0x00b2, 0x00e0}, /* DE pe */
	{0x00b3, 0x00e0}, /* DE pf */
	{0x00b4, 0x00e0}, /* DE pb */
	{0x00b5, 0x00e0}, /* DE ne */
	{0x00b6, 0x00e0}, /* DE nf */
	{0x00b7, 0x00e0}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0010}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1a04}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x003f, 0x0080}, /* CC chsel strength */
	{0x0040, 0x0000}, /* CC lut r   0 */
	{0x0041, 0x0b94}, /* CC lut r  16 144 */
	{0x0042, 0x18a6}, /* CC lut r  32 160 */
	{0x0043, 0x28b8}, /* CC lut r  48 176 */
	{0x0044, 0x3ac9}, /* CC lut r  64 192 */
	{0x0045, 0x4cd9}, /* CC lut r  80 208 */
	{0x0046, 0x5ee7}, /* CC lut r  96 224 */
	{0x0047, 0x70f4}, /* CC lut r 112 240 */
	{0x0048, 0x82ff}, /* CC lut r 128 255 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_dynamic_vtcall_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0b32}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0080, 0x1111}, /* NR col con ring bl */
	{0x0081, 0x3f04}, /* NR blRedTH maxmin */
	{0x0082, 0x1006}, /* NR edgeTH blDecTH */
	{0x0083, 0x3f04}, /* NR conRedTH maxmin */
	{0x00b2, 0x00e0}, /* DE pe */
	{0x00b3, 0x00e0}, /* DE pf */
	{0x00b4, 0x00e0}, /* DE pb */
	{0x00b5, 0x00e0}, /* DE ne */
	{0x00b6, 0x00e0}, /* DE nf */
	{0x00b7, 0x00e0}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0010}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1a04}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x003f, 0x0080}, /* CC chsel strength */
	{0x0040, 0x0000}, /* CC lut r   0 */
	{0x0041, 0x0b94}, /* CC lut r  16 144 */
	{0x0042, 0x18a6}, /* CC lut r  32 160 */
	{0x0043, 0x28b8}, /* CC lut r  48 176 */
	{0x0044, 0x3ac9}, /* CC lut r  64 192 */
	{0x0045, 0x4cd9}, /* CC lut r  80 208 */
	{0x0046, 0x5ee7}, /* CC lut r  96 224 */
	{0x0047, 0x70f4}, /* CC lut r 112 240 */
	{0x0048, 0x82ff}, /* CC lut r 128 255 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_movie_gallery_cabcoff[] = {

	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0200}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00f2}, /* SCR KgWg */
	{0x007c, 0x00e9}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_movie_gallery_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0a00}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00f2}, /* SCR KgWg */
	{0x007c, 0x00e9}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_movie_ui_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0200}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00f2}, /* SCR KgWg */
	{0x007c, 0x00e9}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_movie_ui_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0a00}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00f2}, /* SCR KgWg */
	{0x007c, 0x00e9}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_movie_video_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0200}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00f2}, /* SCR KgWg */
	{0x007c, 0x00e9}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_movie_video_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0a00}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00f2}, /* SCR KgWg */
	{0x007c, 0x00e9}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_movie_vtcall_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0232}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0080, 0x1111}, /* NR col con ring bl */
	{0x0081, 0x3f04}, /* NR blRedTH maxmin */
	{0x0082, 0x1006}, /* NR edgeTH blDecTH */
	{0x0083, 0x3f04}, /* NR conRedTH maxmin */
	{0x00b2, 0x0040}, /* DE pe */
	{0x00b3, 0x0040}, /* DE pf */
	{0x00b4, 0x0040}, /* DE pb */
	{0x00b5, 0x0040}, /* DE ne */
	{0x00b6, 0x0040}, /* DE nf */
	{0x00b7, 0x0040}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1204}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00f2}, /* SCR KgWg */
	{0x007c, 0x00e9}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_movie_vtcall_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0a32}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0080, 0x1111}, /* NR col con ring bl */
	{0x0081, 0x3f04}, /* NR blRedTH maxmin */
	{0x0082, 0x1006}, /* NR edgeTH blDecTH */
	{0x0083, 0x3f04}, /* NR conRedTH maxmin */
	{0x00b2, 0x0040}, /* DE pe */
	{0x00b3, 0x0040}, /* DE pf */
	{0x00b4, 0x0040}, /* DE pb */
	{0x00b5, 0x0040}, /* DE ne */
	{0x00b6, 0x0040}, /* DE nf */
	{0x00b7, 0x0040}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1204}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00f2}, /* SCR KgWg */
	{0x007c, 0x00e9}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_normal_outdoor[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0001}, /* UC */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x00e0, 0x01a0}, /* UC y */
	{0x00e1, 0x01ff}, /* UC cs */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_standard_gallery_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0230}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0000}, /* DE pe */
	{0x00b3, 0x0060}, /* DE pf */
	{0x00b4, 0x0060}, /* DE pb */
	{0x00b5, 0x0060}, /* DE ne */
	{0x00b6, 0x0060}, /* DE nf */
	{0x00b7, 0x0060}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_standard_gallery_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0a30}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0000}, /* DE pe */
	{0x00b3, 0x0060}, /* DE pf */
	{0x00b4, 0x0060}, /* DE pb */
	{0x00b5, 0x0060}, /* DE ne */
	{0x00b6, 0x0060}, /* DE nf */
	{0x00b7, 0x0060}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_standard_ui_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0220}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_standard_ui_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0a20}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_standard_video_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0230}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0060}, /* DE pe */
	{0x00b3, 0x0060}, /* DE pf */
	{0x00b4, 0x0060}, /* DE pb */
	{0x00b5, 0x0060}, /* DE ne */
	{0x00b6, 0x0060}, /* DE nf */
	{0x00b7, 0x0060}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_standard_video_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0a30}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x00b0, 0x0080}, /* DE egth */
	{0x00b2, 0x0060}, /* DE pe */
	{0x00b3, 0x0060}, /* DE pf */
	{0x00b4, 0x0060}, /* DE pb */
	{0x00b5, 0x0060}, /* DE ne */
	{0x00b6, 0x0060}, /* DE nf */
	{0x00b7, 0x0060}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_standard_vtcall_cabcoff[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0232}, /* SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0080, 0x1111}, /* NR col con ring bl */
	{0x0081, 0x3f04}, /* NR blRedTH maxmin */
	{0x0082, 0x1006}, /* NR edgeTH blDecTH */
	{0x0083, 0x3f04}, /* NR conRedTH maxmin */
	{0x00b2, 0x00c0}, /* DE pe */
	{0x00b3, 0x00c0}, /* DE pf */
	{0x00b4, 0x00c0}, /* DE pb */
	{0x00b5, 0x00c0}, /* DE ne */
	{0x00b6, 0x00c0}, /* DE nf */
	{0x00b7, 0x00c0}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_standard_vtcall_cabcon[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0008, 0x0a32}, /* ABC8 CP4 SCR2 CC1|CS2 DE1|LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /* MCM off */
	{0x000a, 0x0000}, /* UC off */
	{0x0030, 0x0000}, /* FA cs1 de8 hdr2 fa1 */
	{0x0080, 0x1111}, /* NR col con ring bl */
	{0x0081, 0x3f04}, /* NR blRedTH maxmin */
	{0x0082, 0x1006}, /* NR edgeTH blDecTH */
	{0x0083, 0x3f04}, /* NR conRedTH maxmin */
	{0x00b2, 0x00c0}, /* DE pe */
	{0x00b3, 0x00c0}, /* DE pf */
	{0x00b4, 0x00c0}, /* DE pb */
	{0x00b5, 0x00c0}, /* DE ne */
	{0x00b6, 0x00c0}, /* DE nf */
	{0x00b7, 0x00c0}, /* DE nb */
	{0x00b8, 0x1000}, /* DE max ratio */
	{0x00b9, 0x0100}, /* DE min ratio */
	{0x00c0, 0x1010}, /* CS hg ry */
	{0x00c1, 0x1010}, /* CS hg gc */
	{0x00c2, 0x1010}, /* CS hg bm */
	{0x00c3, 0x1804}, /* CS weight grayTH */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0071, 0xff00}, /* SCR RrCr */
	{0x0072, 0x00ff}, /* SCR RgCg */
	{0x0073, 0x00ff}, /* SCR RbCb */
	{0x0074, 0x00ff}, /* SCR GrMr */
	{0x0075, 0xff00}, /* SCR GgMg */
	{0x0076, 0x00ff}, /* SCR GbMb */
	{0x0077, 0x00ff}, /* SCR BrYr */
	{0x0078, 0x00e0}, /* SCR BgYg */
	{0x0079, 0xff00}, /* SCR BbYb */
	{0x007a, 0x00ff}, /* SCR KrWr */
	{0x007b, 0x00ff}, /* SCR KgWg */
	{0x007c, 0x00ff}, /* SCR KbWb */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_warm[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0009, 0x0001}, /* MCM */
	{0x000a, 0x0000}, /* UC off */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0001, 0x0028}, /* MCM 4000K */
	{0x0004, 0x64ff}, /* MCM CT5 LSF1 */
	{0x0007, 0x7674}, /* MCM 1cb 2cb W */
	{0x0009, 0x988d}, /* MCM 5cb 1cr W */
	{0x000c, 0x8080}, /* MCM 1cb 2cb K */
	{0x000e, 0x8080}, /* MCM 5cb 1cr K */
	{0x0011, 0x8080}, /* MCM gray axis */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_warm_outdoor[] = {
	{0x0000, 0x0000}, /* BANK 0 */
	{0x0009, 0x0001}, /* MCM */
	{0x000a, 0x0001}, /* UC */
	{0x0000, 0x0001}, /* BANK 1 */
	{0x0001, 0x0028}, /* MCM 4000K */
	{0x0004, 0x64ff}, /* MCM CT5 LSF1 */
	{0x0007, 0x7674}, /* MCM 1cb 2cb W */
	{0x0009, 0x988d}, /* MCM 5cb 1cr W */
	{0x000c, 0x8080}, /* MCM 1cb 2cb K */
	{0x000e, 0x8080}, /* MCM 5cb 1cr K */
	{0x0011, 0x8080}, /* MCM gray axis */
	{0x00e0, 0x01a0}, /* UC y */
	{0x00e1, 0x01ff}, /* UC cs */
	{0x00ff, 0x0000}, /* Mask Release */
};

static const struct cmc624_register_set tune_negative[] = {
	{0x0000, 0x0000}, /*BANK 0 */
	{0x0008, 0x0200}, /*SCR2 CC1 | CS2 DE1 | LoG8 WIENER4 NR2 HDR1 */
	{0x0009, 0x0000}, /*MCM off */
	{0x000a, 0x0000}, /*UC off */
	{0x0030, 0x0000}, /*FA cs1 de8 hdr2 fa1 */
	{0x0000, 0x0001}, /*BANK 1 */
	{0x0071, 0x00ff}, /*SCR RrCr */
	{0x0072, 0xff00}, /*SCR RgCg */
	{0x0073, 0xff00}, /*SCR RbCb */
	{0x0074, 0xff00}, /*SCR GrMr */
	{0x0075, 0x00ff}, /*SCR GgMg */
	{0x0076, 0xff00}, /*SCR GbMb */
	{0x0077, 0xff00}, /*SCR BrYr */
	{0x0078, 0xff00}, /*SCR BgYg */
	{0x0079, 0x00ff}, /*SCR BbYb */
	{0x007a, 0xff00}, /*SCR KrWr */
	{0x007b, 0xff00}, /*SCR KgWg */
	{0x007c, 0xff00}, /*SCR KbWb */
	{0x00ff, 0x0000}, /*Mask Release */
};

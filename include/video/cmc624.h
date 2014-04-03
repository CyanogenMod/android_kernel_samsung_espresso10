/* include/video/cmc624.h
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

#ifndef CMC624_HEADER
#define CMC624_HEADER

/* SFR Bank selection */
#define CMC624_REG_SELBANK		0x00

/* A stage configuration */
#define CMC624_REG_DNRHDTROVE		0x01
#define CMC624_REG_DITHEROFF		0x06
#define CMC624_REG_CLKCONT		0x10
#define CMC624_REG_CLKGATINGOFF		0x0a
#define CMC624_REG_INPUTIFCON		0x24
#define CMC624_REG_CLKMONCONT		0x11  /* Clock Monitor Ctrl Register */
#define CMC624_REG_HDRTCEOFF		0x3a
#define CMC624_REG_I2C			0x0d
#define CMC624_REG_BSTAGE		0x0e
#define CMC624_REG_CABCCTRL		0x7c
#define CMC624_REG_PWMCTRL		0xb4
#define CMC624_REG_OVEMAX		0x54

/* A stage image size */
#define CMC624_REG_1280			0x22
#define CMC624_REG_800			0x23

/* B stage image size */
#define CMC624_REG_SCALERINPH		0x09
#define CMC624_REG_SCALERINPV		0x0a
#define CMC624_REG_SCALEROUTH		0x0b
#define CMC624_REG_SCALEROUTV		0x0c

/* EDRAM configuration */
#define CMC624_REG_EDRBFOUT40		0x01
#define CMC624_REG_EDRAUTOREF		0x06
#define CMC624_REG_EDRACPARAMTIM	0x07

/* Vsync Calibartion */
#define CMC624_REG_CALVAL10		0x65

/* tcon output polarity */
#define CMC624_REG_TCONOUTPOL		0x68

/* tcon RGB configuration */
#define CMC624_REG_TCONRGB1		0x6c
#define CMC624_REG_TCONRGB2		0x6d
#define CMC624_REG_TCONRGB3		0x6e

/* Reg update */
#define CMC624_REG_REGMASK		0x28
#define CMC624_REG_SWRESET		0x09
#define CMC624_REG_RGBIFEN		0x26

#define SLEEPMSEC			0xFFFF

/*
 * TUNING DATA LIST ID
 *
 * ------------------------------------------------------------------
 * FIELD | SPEC(4)  CABC(1)  OUT(1)  TEMP(2)  APP(4)  MODE(4)  CMD(4)
 * ------|-----------------------------------------------------------
 * BIT	 | 19-16    15       14      13-12    11-8    7-4      3-0
 * ------------------------------------------------------------------
 */

#define BITPOS_CMD	(0)
#define BITPOS_MODE	(4)
#define BITPOS_APP	(8)
#define BITPOS_TEMP	(12)
#define BITPOS_OUT	(14)
#define BITPOS_CABC	(15)
#define BITPOS_SPEC	(16)

#define BITMASK_CMD	(0x0F << BITPOS_CMD)
#define BITMASK_MODE	(0x0F << BITPOS_MODE)
#define BITMASK_APP	(0x0F << BITPOS_APP)
#define BITMASK_TEMP	(0x03 << BITPOS_TEMP)
#define BITMASK_OUT	(0x01 << BITPOS_OUT)
#define BITMASK_CABC	(0x01 << BITPOS_CABC)
#define BITMASK_SPEC	(0x0F << BITPOS_SPEC)

#define BITMASK_MAIN_TUNE	(BITMASK_CMD | BITMASK_MODE | BITMASK_APP | \
				 BITMASK_CABC)
#define BITMASK_SUB_TUNE	(BITMASK_CMD | BITMASK_TEMP | BITMASK_OUT | \
				 BITMASK_CABC)
#define BITMASK_BROWSER_TUNE	(BITMASK_CMD | BITMASK_APP | BITMASK_SPEC)

#define TUNE_DATA_ID(_id, _cmd,  _mode, _app, _temp, _out, _cabc, _spec) { \
	(_id) = (((_cmd) << BITPOS_CMD & BITMASK_CMD)	|	\
		((_mode) << BITPOS_MODE & BITMASK_MODE)	|	\
		((_app) << BITPOS_APP & BITMASK_APP)	|	\
		((_temp) << BITPOS_TEMP & BITMASK_TEMP)	|	\
		((_out) << BITPOS_OUT & BITMASK_OUT)	|	\
		((_cabc) << BITPOS_CABC & BITMASK_CABC)	|	\
		((_spec) << BITPOS_SPEC & BITMASK_SPEC));	\
	pr_debug("TUNE: id=%d (%d, %d, %d, %d, %d, %d, %d)\n",	\
			(_id), (_cmd), (_mode), (_app),		\
			(_temp), (_out), (_cabc), (_spec));	\
}

#define MENU_SKIP	0xFFFF

#define NUM_PWRLUT_REG	8

/*
 * enum
 */
enum tune_menu_cmd {
	MENU_CMD_TUNE = 0,
	MENU_CMD_INIT,
	MENU_CMD_INIT_LDI,
};

enum tune_menu_mode {
	MENU_MODE_DYNAMIC = 0,
	MENU_MODE_STANDARD,
	MENU_MODE_MOVIE,
	MENU_MODE_NATURAL,
	MENU_MODE_NEGATIVE,
	MAX_BACKGROUND_MODE,
};

enum tune_menu_app {
	MENU_APP_UI = 0,
	MENU_APP_VIDEO,
	MENU_APP_VIDEO_WARM,
	MENU_APP_VIDEO_COLD,
	MENU_APP_CAMERA,
	MENU_APP_NAVI,
	MENU_APP_GALLERY,
	MENU_APP_DMB,
	MENU_APP_VT,
	MENU_APP_BROWSER,
	MAX_mDNIe_MODE,
};

enum tune_menu_temp {
	MENU_TEMP_NORMAL = 0,
	MENU_TEMP_WARM,
	MENU_TEMP_COLD,
	MAX_TEMP_MODE,
};

enum tune_menu_outdoor {
	MENU_OUT_OFF = 0,
	MENU_OUT_ON,
	MAX_OUTDOOR_MODE,
};

enum tune_menu_cabc {
	MENU_CABC_OFF = 0,
	MENU_CABC_ON,
	MAX_CABC_MODE,
};

enum tune_menu_tone {
	MENU_SPEC_TONE1 = 0,
	MENU_SPEC_TONE2,
	MENU_SPEC_TONE3,
	COLOR_TONE_MAX,
};

enum auto_brt_val {
	AUTO_BRIGHTNESS_MANUAL = 0,
	AUTO_BRIGHTNESS_VAL1,
	AUTO_BRIGHTNESS_VAL2,
	AUTO_BRIGHTNESS_VAL3,
	AUTO_BRIGHTNESS_VAL4,
	AUTO_BRIGHTNESS_VAL5,
	MAX_AUTO_BRIGHTNESS,
};

enum power_lut_mode {
	PWRLUT_MODE_UI = 0,
	PWRLUT_MODE_VIDEO,
	MAX_PWRLUT_MODE,
};

enum power_lut_level {
	PWRLUT_LEV_INDOOR = 0,
	PWRLUT_LEV_OUTDOOR1,
	PWRLUT_LEV_OUTDOOR2,
	MAX_PWRLUT_LEV,
};

/*
 * structures
 */
struct cmc624_register_set {
	unsigned int reg_addr;
	unsigned int data;
};

struct cabcoff_pwm_cnt_tbl {
	u16 max;
	u16 def;
	u16 min;
};

struct cabcon_pwr_lut_tbl {
	u16 max[MAX_PWRLUT_LEV][MAX_PWRLUT_MODE][NUM_PWRLUT_REG];
	u16 def[MAX_PWRLUT_LEV][MAX_PWRLUT_MODE][NUM_PWRLUT_REG];
	u16 min[MAX_PWRLUT_LEV][MAX_PWRLUT_MODE][NUM_PWRLUT_REG];
};

struct cmc624_panel_data {
	int gpio_ima_sleep;
	int gpio_ima_nrst;
	int gpio_ima_cmc_en;
	u8 *lcd_name;

	int (*init_tune_list)(void);
	void (*power_lcd)(bool enable);
	int (*power_vima_1_1V)(int on);
	int (*power_vima_1_8V)(int on);

	/* PWM control */
	struct cabcoff_pwm_cnt_tbl *pwm_tbl;
	struct cabcon_pwr_lut_tbl *pwr_luts;
};

struct tune_data {
	struct list_head list;
	u32 id;
	const struct cmc624_register_set *value;
	u32 size;
};

/*
 * extern functions
 */
extern int cmc624_register_tune_data(u32 id,
		const struct cmc624_register_set *cmc_set, u32 tune_size);
extern int cmc624_set_pwm(int intensity);
extern void cmc624_set_auto_brightness(enum auto_brt_val auto_brt);

#endif /*CMC624_HEADER */

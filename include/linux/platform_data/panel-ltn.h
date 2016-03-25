/*inclue/linux/ltn.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * Header file for Samsung Display Panel(LCD) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/types.h>

#define BRIGHTNESS_OFF			0
#define BRIGHTNESS_DIM			20
#define BRIGHTNESS_MIN			30
#define BRIGHTNESS_25			86
#define BRIGHTNESS_DEFAULT		140
#define BRIGHTNESS_MAX			255

#define NUM_BRIGHTNESS_LEVEL	6 /* off, dim, min, 25, default and max */

struct brightness_data {
	int platform_value[NUM_BRIGHTNESS_LEVEL];
	int kernel_value[NUM_BRIGHTNESS_LEVEL];
};

struct ltn_panel_data {
	int lvds_nshdn_gpio;
	int lcd_en_gpio;
	int led_backlight_reset_gpio;
	int backlight_gptimer_num;
	int panel_id;
	void (*set_power) (bool enable);
	void (*set_gptimer_idle) (void);
	struct brightness_data brightness_table;
};

enum { PANEL_BOE_7, PANEL_SEC };
enum { PANEL_AUO, PANEL_HYDIS, PANEL_BOE_10, PANEL_LCD };

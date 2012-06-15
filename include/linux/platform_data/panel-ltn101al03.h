/*inclue/linux/ltn101al03.h
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

struct ltn101al03_panel_data {
	int lvds_nshdn_gpio;
	int lcd_en_gpio;
	int led_backlight_reset_gpio;
	int backlight_gptimer_num;
	void (*set_power) (bool enable);
	void (*set_gptimer_idle) (void);
};

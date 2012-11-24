/*
 * Copyright (C) 2012 Samsung Electronics
 * Seungjin Kim <nayaksj@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef _LINUX_LP855X_H_
#define _LINUX_LP855X_H_

#define LP855X_BRIGHTNESS_MAX		0xFF
#define LP855X_BRIGHTNESS_MIN		0x0

#define PS_MODE_6_6	0x0	/* 6-phase, 6 drivers */
#define PS_MODE_5_5	0x01	/* 5-phase, 5 drivers */
#define PS_MODE_4_4	0x02	/* 4-phase, 4 drivers */
#define PS_MODE_3_3	0x03	/* 3-phase, 3 drivers */
#define PS_MODE_2_2	0x04	/* 2-phase, 2 drivers */
#define PS_MODE_3_6	0x05	/* 3-phase, 3 drivers */
#define PS_MODE_2_6	0x06	/* 2-phase, 2 drivers */
#define PS_MODE_1_6	0x07	/* 1-phase, 1 drivers */

#define BRT_MODE_PWM				0x0
#define BRT_MODE_REG_PWM			0x1
#define BRT_MODE_REG				0x2
#define BRT_MODE_REG_PWM_AFTER_SHARPER		0x3

struct lp855x_pdata {
	int default_intensity;
	u8 ps_mode;
	u8 brt_mode;
	int (*set_power)(int on);
	int (*send_intensity)(int intensity);
	void (*set_auto_brt)(int auto_brightness);
};

#endif

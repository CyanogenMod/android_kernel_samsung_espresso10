/*
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
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

#ifndef __FM_SI470X_H__
#define __FM_SI470X_H__

struct si470x_platform_data {
	void (*reset_gpio_on)(int enable);
	int gpio;
	bool enabled;
};

#endif /* __FM_SI470X_H__ */

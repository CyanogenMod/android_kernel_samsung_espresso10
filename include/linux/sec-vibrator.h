/*
 * Copyright (C) 2011 Samsung Electronics
 *
 * Author: Shankar Bandal <shankar.b@samsung.com>
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

#ifndef _SEC_VIBRATOR_H_
#define _SEC_VIBRATOR_H_

#define VIB_DEVNAME "tspdrv"

struct secvib_platform_data {
	int (*vib_enable)(int actr_idx, int on);

	int (*pwm_init)(void);
	int (*pwm_set)(int actr_idx, unsigned long force);

	bool enabled;
	unsigned gpio_en;
	int num_actuators;
};

#endif /* _SEC_VIBRATOR_H_ */

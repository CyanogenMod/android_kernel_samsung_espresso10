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

#ifndef _CYPRESS_TOUCHKEY_H_
#define _CYPRESS_TOUCHKEY_H_

#include <linux/i2c.h>

struct cptk_platform_data {
	void (*power)(int on);
	int gpio;
	int mod_ver;
	int firm_ver;
	const int *keymap;
	unsigned int keymap_size;
	const char *fw_name;
	int scl_pin;
	int sda_pin;
	int en_pin;
	int en_led_pin;
};

extern int touchkey_flash_firmware(struct cptk_platform_data *, const u8 *);

#endif /* _CYPRESS_TOUCHKEY_H_ */

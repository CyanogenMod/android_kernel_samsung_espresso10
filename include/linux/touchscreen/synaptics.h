/* drivers/input/touchscreen/synaptics_fw_updater.h
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_SYNAPTICS_FW_UPDATER_H
#define _LINUX_SYNAPTICS_FW_UPDATER_H

#include <linux/i2c.h>

#ifdef CONFIG_TOUCHSCREEN_OLD_FW_STD
struct synaptics_fw_info {
	char version[5];
	const char release_date[5];
};
#else
struct synaptics_fw_info {
	const char vendor[2];
	int version;
	int hw_id;
	const char release_date[5];
};
#endif

void synaptics_set_i2c_client(struct i2c_client *);
bool synaptics_fw_update(struct i2c_client *, const u8 *, const int);
bool F54_SetRawCapData(struct i2c_client *, s16 *);
bool F54_SetRxToRxData(struct i2c_client *, s16 *);
bool F54_TxToTest(struct i2c_client *, s16 *, int);
int synaptics_set_low_temp_bit(const bool);
#endif

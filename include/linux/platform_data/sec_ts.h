/* include/linux/sec_ts.h
 * platform data structure for Samsung common touch driver
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

#ifndef _LINUX_SEC_TS_H
#define _LINUX_SEC_TS_H

#define SEC_TS_NAME "sec_touchscreen"

#include <linux/gpio.h>

enum {
	CABLE_TA = 0,
	CABLE_USB,
	CABLE_NONE,
};

struct touch_key {
	char *name;
	u32 code;
};

/**
 * struct sec_ts_platform_data - represent specific touch device
 * @model_name : name of device name
 * @panel_name : name of sensor panel name
 * @fw_name : intenal firmware file name
 * @fw_info : represent device firmware informations
 * @ext_fw_name : external(ex. sd card or interal ROM) firmware file name
 * with path
 * @key : informaition of intergrated touch key. it usually using LCD panel.
 * @key_size : number of touch keys.
 * @rx_channel_no : receive channel number of touch sensor
 * @tx_channel_no : transfer channel number of touch sensor
 * @x_pixel_size : maximum of x axis pixel size
 * @y_pixel_size : maximum of y axis pixel size
 * @pivot : change x, y coordination
 * @ta_state : represent of charger connect state
 * @driver_data : pointer back to the struct class that this structure is
 * associated with.
 * @private_data : pointer that needed by vendor specific information
 * @gpio_irq : physical gpio using to interrupt
 * @gpio_en : physical gpio using to IC enable
 * @gpio_scl : physical gpio using to i2c communication
 * @gpio_sda : physical gpio using to i2c communication
 * @set_ta_mode : callback function when TA, USB connected or disconnected
 * @set_power : control touch screen IC power gpio pin
 * @set_dvfs : force control AP frequency
 */
struct sec_ts_platform_data {
	const char *model_name;
	const char *panel_name;
	const char *fw_name;
	const void *fw_info;
	const char *ext_fw_name;
	const struct touch_key *key;
	size_t key_size;
	int tx_channel_no;
	int rx_channel_no;
	int x_pixel_size;
	int y_pixel_size;
	bool pivot;
	int ta_state;
	void *driver_data;
	void *private_data;
	u32 gpio_irq;
	u32 gpio_en;
	u32 gpio_scl;
	u32 gpio_sda;
	void (*set_ta_mode)(int *);
	void (*set_power)(bool);
	void (*set_dvfs)(bool);
};
#endif

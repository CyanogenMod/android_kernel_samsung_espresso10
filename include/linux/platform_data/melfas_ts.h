/* include/linux/melfas_ts.h - platform data structure for MCS Series sensor
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

#ifndef _LINUX_MELFAS_TS_H
#define _LINUX_MELFAS_TS_H

#define MELFAS_TS_NAME "melfas-ts"

#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/battery.h>

enum {
	GPIO_TOUCH_nINT = 0,
	GPIO_TOUCH_EN,
	GPIO_TOUCH_SCL,
	GPIO_TOUCH_SDA
};

/**
 * struct melfas_platform_data - represent specific touch device
 * @model_name : name of device name
 * @panel_name : name of sensor panel name
 * @rx_channel_no : receive channel number of touch sensor
 * @tx_channel_no : transfer channel number of touch sensor
 * @x_pixel_size : maximum of x axis pixel size
 * @y_pixel_size : maximum of y axis pixel size
 * @ta_state : represent of charger connect state
 * @link : pointer that represent touch screen driver struct
 * @gpio_set : physical gpios used by touch screen IC
 * @set_ta_mode : callback function when TA, USB connected or disconnected
 * @set_power : control touch screen IC power gpio pin
 * @set_i2c_to_gpio : control IO pins using i2c or gpio for melfas fw. update
 */
struct melfas_platform_data {
	char *model_name;
	int tx_channel_no;
	int rx_channel_no;
	int x_pixel_size;
	int y_pixel_size;
	int ta_state;
	void *link;
	struct gpio *gpio_set;
	void (*set_ta_mode)(int *);
	void (*set_power)(bool);
	int (*set_i2c_to_gpio)(bool);
};
#endif /* _LINUX_MELFAS_TS_H */

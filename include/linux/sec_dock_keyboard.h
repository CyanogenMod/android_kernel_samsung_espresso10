/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * Author:
 *	Heetae Ahn <heetae82.ahn@samsung.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/earlysuspend.h>
#include <linux/serio.h>

#ifndef _SEC_DOCK_KEYBOARD_H_
#define _SEC_DOCK_KEYBOARD_H_

#define KBD_DRV_NAME "sec_keyboard"

#define KEYBOARD_SIZE	128
#define US_KEYBOARD 0xEB
#define UK_KEYBOARD 0xEC

#define KBD_MIN	0x4
#define KBD_MAX	0x7F

#define MAX_BUF 255

/* for the remap key */
enum REMAPKEY_STATE {
	REMAPKEY_RELEASED = 0,
	REMAPKEY_PRESSED,
};

enum KEY_LAYOUT {
	UNKNOWN_KEYLAYOUT = 0,
	US_KEYLAYOUT,
	UK_KEYLAYOUT,
};

struct dock_keyboard_data {
	struct input_dev *input_dev;
	struct serio *serio;
	struct device *kbd_dev;
	struct work_struct work_msg;
	struct delayed_work dwork_off;
	struct early_suspend early_suspend;
	struct timer_list key_timer;
	bool led_on;
	int dock_irq_gpio;
	unsigned int kl;
	bool handshaking;
	int release_cnt;
	bool remap_state;
	unsigned char remap_keycode;
	int buf_front;
	int buf_rear;
	unsigned char key_buf[MAX_BUF];
	bool dockconnected;
	bool keyboard_enable;
	void (*power)(bool on);
};

struct dock_keyboard_platform_data {
	int dock_irq_gpio;
	void (*power)(bool on);
	void (*register_cb)(struct input_dev *, void *);
};

struct key_table {
	int keycode;
	bool pressed;
};

struct sec_dock_keyboard_driver {
	struct platform_driver plat_drv;
	struct serio_driver serio_drv;
	struct dock_keyboard_data *private_data;
};

#endif /*_SEC_DOCK_KEYBOARD_H_ */

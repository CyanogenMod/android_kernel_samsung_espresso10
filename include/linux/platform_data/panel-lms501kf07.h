/*
 * Samsung lms501kf07 panel support
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_PLATFORM_DATA_PANEL_LMS501KF07_H
#define __LINUX_PLATFORM_DATA_PANEL_LMS501KF07_H

struct lms501kf07_sequence_entry {
	const u8 *cmd;
	int cmd_len;
	unsigned int msleep;
};

struct lms501kf07_mdnie_data {
	u8 *ui;
	u8 *gallery;
	u8 *video;
	u8 *video_warm;
	u8 *video_cold;
	u8 *camera;
	int size;
};

struct panel_lms501kf07_data {
	int reset_gpio;
	int blctrl_gpio;
	bool bl_on_after_init;

	void (*set_power)(bool enable);
	void (*set_gptimer_idle)(void);

	const struct lms501kf07_sequence_entry *seq_display_on;
	int seq_display_on_size;

	const struct lms501kf07_sequence_entry *seq_display_off;
	int seq_display_off_size;

	const struct lms501kf07_mdnie_data *mdnie_data;
};

#endif

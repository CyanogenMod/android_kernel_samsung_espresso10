/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MAX17042_BATTERY_MANAGER_H_
#define __MAX17042_BATTERY_MANAGER_H_

#include <linux/battery.h>

#define LOW_BATT_COMP_RANGE_NUM	5
#define LOW_BATT_COMP_LEVEL_NUM	2
#define MAX_LOW_BATT_CHECK_CNT	10

enum {
	UNKNOWN_TYPE = 0,
	SDI_BATTERY_TYPE,
	BYD_BATTERY_TYPE,
};

struct max17042_fuelgauge_callbacks {
	int (*get_value)(struct max17042_fuelgauge_callbacks *ptr,
			enum fuel_property fg_prop);
	int (*fg_reset_soc)(struct max17042_fuelgauge_callbacks *ptr);
	void (*full_charged_compensation)(
		struct max17042_fuelgauge_callbacks *ptr,
		u32 is_recharging, u32 pre_update);
	void (*set_adjust_capacity)(struct max17042_fuelgauge_callbacks *ptr);
	int (*check_low_batt_compensation)(
		struct max17042_fuelgauge_callbacks *ptr,
		struct bat_information bat_info);
	void (*check_vf_fullcap_range)(
		struct max17042_fuelgauge_callbacks *ptr);
	void (*reset_capacity)(struct max17042_fuelgauge_callbacks *ptr);
	int (*check_cap_corruption)(struct max17042_fuelgauge_callbacks *ptr);
	void (*update_remcap_to_fullcap)(
		struct max17042_fuelgauge_callbacks *ptr);
	int (*get_register_value)(struct max17042_fuelgauge_callbacks *ptr,
		u8 addr);
};

struct max17042_current_range {
	int range1;
	int range2;
	int range3;
	int range4;
	int range5;
	int range_max;
	int range_max_num;
};

struct max17042_sdi_compensation {
	int range1_1_slope;
	int range1_1_offset;
	int range1_3_slope;
	int range1_3_offset;
	int range2_1_slope;
	int range2_1_offset;
	int range2_3_slope;
	int range2_3_offset;
	int range3_1_slope;
	int range3_1_offset;
	int range3_3_slope;
	int range3_3_offset;
	int range4_1_slope;
	int range4_1_offset;
	int range4_3_slope;
	int range4_3_offset;
	int range5_1_slope;
	int range5_1_offset;
	int range5_3_slope;
	int range5_3_offset;
};

struct max17042_platform_data {
	bool enable_current_sense;
	void (*register_callbacks)(struct max17042_fuelgauge_callbacks *ptr);
	int sdi_capacity;
	int sdi_vfcapacity;
	int sdi_low_bat_comp_start_vol;
	int byd_capacity;
	int byd_vfcapacity;
	int byd_low_bat_comp_start_vol;
	int jig_on;
	struct max17042_current_range current_range;
	struct max17042_sdi_compensation sdi_compensation;
};

#endif /* __MAX17042_BATTERY_MANAGER_H_ */

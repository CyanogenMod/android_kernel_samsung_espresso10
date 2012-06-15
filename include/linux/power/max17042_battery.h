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

#ifndef __MAX17042_BATTERY_H_
#define __MAX17042_BATTERY_H_

#include <linux/battery.h>

#define LOW_BATT_COMP_RANGE_NUM	5
#define LOW_BATT_COMP_LEVEL_NUM	2
#define MAX_LOW_BATT_CHECK_CNT	10

/* Battery parameter */
/* Current range for P2(not dependent on battery type */
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO) || \
	defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
#define CURRENT_RANGE1	0
#define CURRENT_RANGE2	-100
#define CURRENT_RANGE3	-750
#define CURRENT_RANGE4	-1250
#define CURRENT_RANGE_MAX	CURRENT_RANGE4
#define CURRENT_RANGE_MAX_NUM	4
/* SDI type low battery compensation offset */
#define SDI_Range4_1_Offset		3320
#define SDI_Range4_3_Offset		3410
#define SDI_Range3_1_Offset		3451
#define SDI_Range3_3_Offset		3454
#define SDI_Range2_1_Offset		3461
#define SDI_Range2_3_Offset		3544
#define SDI_Range1_1_Offset		3456
#define SDI_Range1_3_Offset		3536
#define SDI_Range4_1_Slope		0
#define SDI_Range4_3_Slope		0
#define SDI_Range3_1_Slope		97
#define SDI_Range3_3_Slope		27
#define SDI_Range2_1_Slope		96
#define SDI_Range2_3_Slope		134
#define SDI_Range1_1_Slope		0
#define SDI_Range1_3_Slope		0
/* ATL type low battery compensation offset */
#define ATL_Range5_1_Offset		3277
#define ATL_Range5_3_Offset		3293
#define ATL_Range4_1_Offset		3312
#define ATL_Range4_3_Offset		3305
#define ATL_Range3_1_Offset		3310
#define ATL_Range3_3_Offset		3333
#define ATL_Range2_1_Offset		3335
#define ATL_Range2_3_Offset		3356
#define ATL_Range1_1_Offset		3325
#define ATL_Range1_3_Offset		3342
#define ATL_Range5_1_Slope		0
#define ATL_Range5_3_Slope		0
#define ATL_Range4_1_Slope		30
#define ATL_Range4_3_Slope		667
#define ATL_Range3_1_Slope		20
#define ATL_Range3_3_Slope		40
#define ATL_Range2_1_Slope		60
#define ATL_Range2_3_Slope		76
#define ATL_Range1_1_Slope		0
#define ATL_Range1_3_Slope		0
#elif defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10)	/* P4W battery parameter */
/* Current range for P4W(not dependent on battery type */
#define CURRENT_RANGE1	0
#define CURRENT_RANGE2	-200
#define CURRENT_RANGE3	-600
#define CURRENT_RANGE4	-1500
#define CURRENT_RANGE5	-2500
#define CURRENT_RANGE_MAX	CURRENT_RANGE5
#define CURRENT_RANGE_MAX_NUM	5
/* SDI type low battery compensation offset */
#define SDI_Range5_1_Offset		3318
#define SDI_Range5_3_Offset		3383
#define SDI_Range4_1_Offset		3451
#define SDI_Range4_3_Offset		3618
#define SDI_Range3_1_Offset		3453
#define SDI_Range3_3_Offset		3615
#define SDI_Range2_1_Offset		3447
#define SDI_Range2_3_Offset		3606
#define SDI_Range1_1_Offset		3438
#define SDI_Range1_3_Offset		3591
#define SDI_Range5_1_Slope		0
#define SDI_Range5_3_Slope		0
#define SDI_Range4_1_Slope		53
#define SDI_Range4_3_Slope		94
#define SDI_Range3_1_Slope		54
#define SDI_Range3_3_Slope		92
#define SDI_Range2_1_Slope		45
#define SDI_Range2_3_Slope		78
#define SDI_Range1_1_Slope		0
#define SDI_Range1_3_Slope		0
/* Default value for build */
/* ATL type low battery compensation offset */
#define ATL_Range4_1_Offset		3298
#define ATL_Range4_3_Offset		3330
#define ATL_Range3_1_Offset		3375
#define ATL_Range3_3_Offset		3445
#define ATL_Range2_1_Offset		3371
#define ATL_Range2_3_Offset		3466
#define ATL_Range1_1_Offset		3362
#define ATL_Range1_3_Offset		3443
#define ATL_Range4_1_Slope		0
#define ATL_Range4_3_Slope		0
#define ATL_Range3_1_Slope		50
#define ATL_Range3_3_Slope		77
#define ATL_Range2_1_Slope		40
#define ATL_Range2_3_Slope		111
#define ATL_Range1_1_Slope		0
#define ATL_Range1_3_Slope		0
#endif


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
};

struct max17042_platform_data {
	bool enable_current_sense;
	void (*register_callbacks)(struct max17042_fuelgauge_callbacks *ptr);
	int capacity;
	int vfcapacity;
	int low_bat_comp_start_vol;
	int jig_on;
};

#endif /* __MAX17042_BATTERY_H_ */

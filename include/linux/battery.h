/*
 *  Copyright (C) 2011 Samsung Electronics
 *  HongMin Son <hongmin.son@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_BATTERY_H_
#define __SEC_BATTERY_H_

enum fuel_property {
	READ_FG_VCELL = 0,
	READ_FG_SOC,
	READ_FG_TEMP,
	READ_FG_CURRENT,
	READ_FG_AVG_CURRENT,
	READ_FG_STATUS,

};

enum cable_type_t {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
};

/**
** temp_adc_table_data
** @adc_value : thermistor adc value
** @temperature : temperature(C) * 10
**/
struct temp_adc_table_data {
	int adc_value;
	int temperature;
};

struct bat_information {
	int soc;
	int vcell;
	int temp;
	int health;
	int fg_current;
	int avg_current;
};

#endif

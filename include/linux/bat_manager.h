/*
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BATTERY_MONITOR_H_
#define __BATTERY_MONITOR_H_

#include <linux/battery.h>

struct battery_manager_callbacks {
	void (*set_cable)(struct battery_manager_callbacks *ptr,
			enum cable_type_t status);
	void (*set_full_charge)(struct battery_manager_callbacks *ptr);
	void (*fuel_alert_lowbat)(struct battery_manager_callbacks *ptr);
};

struct batman_platform_data {
	int bootmode;
	int ta_gpio;
	int jig_on;
	int (*get_temp_adc)(void);
	int (*get_temp)(void);
	void (*register_callbacks)(struct battery_manager_callbacks *ptr);
	int (*get_fuel_value)(enum fuel_property fg_prop);
	int (*low_bat_compensation)(struct bat_information bat_info);
	int (*check_cap_corruption)(void);
	void (*check_vf_fullcap_range)(void);
	void (*reset_fuel_soc)(void);
	void (*set_charger_state)(int flag);
	void (*set_charger_en)(int state);
	void (*fg_adjust_capacity)(void);
	void (*update_fullcap_value)(void);
	int (*get_fg_register)(u8 addr);
	void (*full_charger_comp)(u32 is_recharging, u32 pre_update);
	int (*get_charger_type)(void);
	int high_block_temp;
	int high_recover_temp;
	int low_block_temp;
	int low_recover_temp;
	int recharge_voltage;
	int limit_charging_time;   /* 6hour */
	int limit_recharging_time;  /* 90min */
	void (*set_term_current)(int term_type);
	void (*set_charge_current)(int cable_type);
	int bat_removal;
};

#endif

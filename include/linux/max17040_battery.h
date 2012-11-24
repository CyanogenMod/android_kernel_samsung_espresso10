/*
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17040_BATTERY_H_
#define __MAX17040_BATTERY_H_

struct max17040_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
	int (*is_full_charge)(bool);
	int (*get_bat_temp)(int *, int *);
	int (*get_charging_source)(void);
	bool (*get_charging_state)(void);
	int bootmode;
	bool skip_reset;
	int min_capacity; /* minimum allowable capacity. The reported capacity
			     will be scaled from [<min_capacity>,100] to
			     [0,100] */
	void (*allow_charging)(int en);
	void (*adjust_charger_mode)(int state);
	void (*restore_charger_mode)(void);
	bool (*check_temp_block_state)(int temp);
	void (*set_charger_start_state)(void);
	int high_block_temp;
	int high_recover_temp;
	int low_block_temp;
	int low_recover_temp;
	int fully_charged_vol;
	int recharge_vol;
	int limit_charging_time;
	int limit_recharging_time;
	bool use_fuel_alert;
	int (*full_charge_irq)(void);
	int (*bat_removal_irq) (void);
	int (*vf_adc_value) (void);
};

#endif

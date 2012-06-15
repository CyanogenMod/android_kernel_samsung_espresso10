/*
 *  Copyright (C) 2011 Samsung Electronics
 *  Ikkeun Kim <iks.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SMB136_CHARGER_H_
#define __SMB136_CHARGER_H_

struct smb_charger_callbacks {
	void (*set_charging_state) (struct smb_charger_callbacks *ptr,
		int cable_status);
	int (*get_status_reg) (struct smb_charger_callbacks *prt);
};

struct smb_charger_data {
	void (*register_callbacks)(struct smb_charger_callbacks *ptr);
	void (*unregister_callbacks)(void);
	void (*set_charge)(int state);
	int enable;
	int stat;
	int ta_nconnected;
	int hw_revision;
};

#endif

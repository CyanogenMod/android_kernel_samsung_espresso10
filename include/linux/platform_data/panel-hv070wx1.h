/* inclue/linux/panel-hv070wx1.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _PANEL_HV070WX1_H_
#define _PANEL_HV070WX1_H_

#include <linux/types.h>

enum {PANEL_AUO, PANEL_HYDIS, PANEL_BOE, PANEL_LCD};

struct hv070wx1_panel_data {
	void (*set_power) (bool enable);
};

#endif	/* _PANEL_HV070WX1_H_ */

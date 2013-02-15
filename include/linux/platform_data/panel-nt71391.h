/* inclue/linux/panel-nt71391.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _PANEL_NT71391_H_
#define _PANEL_NT71391_H_

#include <linux/types.h>

struct nt71391_panel_data {
	void (*set_power) (bool enable);
};

#endif	/* _PANEL_NT71391_H_ */

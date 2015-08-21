/* include/linux/touchscreen/melfas.h
 * header for melfas touchscreen vendor functions.
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_MELFAS_H
#define _LINUX_MELFAS_H

#include <linux/platform_data/sec_ts.h>

struct melfas_fw_info {
	const char product_code[7];
	const char core_version;
	char private_version;
	char public_version;
};

extern void touch_i2c_to_gpio(bool to_gpios);

bool isp_updater(const u8 *fw_data, const size_t fw_size,
				const struct sec_ts_platform_data *sec_pdata);

bool isc_updater(const u8 *fw_data, const size_t fw_size,
				const struct sec_ts_platform_data *sec_pdata);
#endif

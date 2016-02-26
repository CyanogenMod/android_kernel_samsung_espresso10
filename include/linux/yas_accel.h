/*
 * Copyright (c) 2010-2011 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef __YAS_ACCEL_H__
#define __YAS_ACCEL_H__
#include <linux/yas_cfg.h>

int yas_acc_driver_BMA25X_init(struct yas_acc_driver *f);

enum {
	BMA25X_ENABLED,
	MAX_CHIP_NUM,
};

struct accel_platform_data {
	int used_chip;
};
#define MAX_LEN_OF_NAME 10

#endif /*__YAS_ACCEL_H__ */

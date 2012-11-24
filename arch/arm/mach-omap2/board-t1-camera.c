
/* arch/arm/mach-omap2/board-t1-camera.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/err.h>
#include "sec_common.h"

struct device *cam_dev_front;

static ssize_t front_camera_type_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char camType[] = "SLSI_S5K5BAFX_NONE\n";
	return sprintf(buf, "%s", camType);
}
static DEVICE_ATTR(camtype_front, S_IRUGO, front_camera_type_show, NULL);


void __init omap4_t1_cam_init(void)
{
	cam_dev_front = device_create(sec_class, NULL, 0, NULL,
				 "sec_s5k5bafx");

	if (IS_ERR(cam_dev_front))
		pr_err("Failed to create device(sec_cam)!\n");

	if (device_create_file(cam_dev_front, &dev_attr_camtype_front) < 0) {
		pr_debug("%s: failed to create device file, %s\n",
				__FILE__, dev_attr_camtype_front.attr.name);
	}
}

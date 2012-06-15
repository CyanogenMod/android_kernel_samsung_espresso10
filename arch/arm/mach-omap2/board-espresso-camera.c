/*
 * Copyright (C) 2011 Google, Inc.
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

#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/err.h>

#include "board-espresso.h"

#define CAM_MAJOR	119

static ssize_t back_camera_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char cam_type[] = "SLSI_S5K5CCGX_NONE\n";

	return sprintf(buf, "%s", cam_type);
}

static ssize_t front_camera_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char cam_type[] = "SF_SR030PC50_NONE\n";

	return sprintf(buf, "%s", cam_type);
}

static struct device_attribute dev_attr_camtype_back =
	__ATTR(rear_camtype, S_IRUGO, back_camera_type_show, NULL);

static struct device_attribute dev_attr_camtype_front =
	__ATTR(front_camtype, S_IRUGO, front_camera_type_show, NULL);

void __init omap4_espresso_camera_init(void)
{
	struct class *camera_dev_class;
	struct device *cam_dev_back;
	struct device *cam_dev_front;

	camera_dev_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_dev_class)) {
		pr_err("Failed to create class(camera_dev_class)!\n");
		goto OUT5;
	}

	cam_dev_back = device_create(camera_dev_class, NULL,
		MKDEV(CAM_MAJOR, 0), NULL, "rear");
	if (IS_ERR(cam_dev_back)) {
		pr_err("Failed to create device(cam_dev_back)!\n");
		goto OUT4;
	}

	if (device_create_file(cam_dev_back, &dev_attr_camtype_back) < 0) {
		pr_err("Failed to create device file: %s\n",
				dev_attr_camtype_back.attr.name);
		goto OUT3;
	}

	cam_dev_front = device_create(camera_dev_class, NULL,
		MKDEV(CAM_MAJOR, 1), NULL, "front");
	if (IS_ERR(cam_dev_front)) {
		pr_err("Failed to create device(cam_dev_front)!\n");
		goto OUT2;
	}

	if (device_create_file(cam_dev_front, &dev_attr_camtype_front) < 0) {
		pr_err("Failed to create device file: %s\n",
				dev_attr_camtype_front.attr.name);
		goto OUT1;
	}

	return;

OUT1:
	device_destroy(sec_class, MKDEV(CAM_MAJOR, 1));
OUT2:
	device_remove_file(cam_dev_back, &dev_attr_camtype_back);
OUT3:
	device_destroy(sec_class, MKDEV(CAM_MAJOR, 0));
OUT4:
	class_destroy(camera_dev_class);
OUT5:
	return;
}

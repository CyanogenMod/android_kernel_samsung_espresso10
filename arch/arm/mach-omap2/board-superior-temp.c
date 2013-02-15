/* arch/arm/mach-omap2/board-superior-temp.c
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
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kallsyms.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>

#include "sec_common.h"

static int superior_temp_ap_temp_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct platform_device *pdev;
	void *temp_sensor;
	int (*read_current_temp) (void *);
	int temp;

	pdev = to_platform_device(
			bus_find_device_by_name(&platform_bus_type,
						NULL,
						"omap_temp_sensor.0"));
	temp_sensor = platform_get_drvdata(pdev);
	read_current_temp =
		(void *)kallsyms_lookup_name("omap_read_current_temp");

	if (IS_ERR(temp_sensor) && IS_ERR(read_current_temp))
		return -ENODEV;

	/* temp = (temperature in Celsius) x 1000 */
	temp = read_current_temp(temp_sensor);

	/* TODO: nomalize 'temp' variable, if required in here. */
	temp /= 100;	/* same as LSI chipset */

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR(ap_temp, S_IRUGO, superior_temp_ap_temp_show, NULL);

static int omap4_superior_temp_init(void)
{
	struct device *temperature;
	int err;

	temperature = device_create(sec_class, NULL, 0, NULL,
			"temperature");
	if (unlikely(!temperature))
		pr_warning("(%s): failed to create sysfs!\n", __func__);

	err = device_create_file(temperature, &dev_attr_ap_temp);
	if (unlikely(err < 0)) {
		pr_warning("(%s): failed to create device file (%s)\n",
			__func__, dev_attr_ap_temp.attr.name);
		device_destroy(temperature, 0);
	}

	return 0;
}
late_initcall(omap4_superior_temp_init);

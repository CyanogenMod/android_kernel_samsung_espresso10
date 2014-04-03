/* arch/arm/mach-omap2/board-espresso-irled.c
 *
 * Copyright (C) 2012 Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <mach/cpufreq_limits.h>

#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "board-espresso.h"

#define CLOCK_VALUE 800000
#define ON_OFFSET_VALUE -2
#define OFF_OFFSET_VALUE 0

#define MAX_SIZE 1024
#define NANO_SEC 1000000000
#define MICRO_SEC 1000000

enum {
	GPIO_IRDA_EN = 0,
	GPIO_IRDA_CONTROL,
};

static struct gpio irled_gpios[] = {
	[GPIO_IRDA_EN] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "IRDA_EN",
	},
	[GPIO_IRDA_CONTROL] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "IRDA_CONTROL",
	},
};

static struct irled_data {
	struct work_struct work;
	int cpu_frequency;
	int on_offset;
	int off_offset;
	unsigned int signal[MAX_SIZE];
} ir_data;

static void irled_work(struct work_struct *work)
{
	unsigned int period;
	unsigned int off_period;
	unsigned int duty;
	unsigned int on;
	unsigned int off;
	unsigned int i;
	unsigned int j;
	int ret;

	ret =
	    omap_cpufreq_max_limit(DVFS_LOCK_ID_IR_LED, ir_data.cpu_frequency);
	ret |=
	    omap_cpufreq_min_limit(DVFS_LOCK_ID_IR_LED, ir_data.cpu_frequency);

	if (unlikely(ret < 0))
		pr_err("irled: failed to lock cpufreq\n");

	gpio_direction_output(irled_gpios[GPIO_IRDA_EN].gpio, 1);

	__udelay(1000);

	period = (MICRO_SEC / ir_data.signal[0]) + ir_data.on_offset;

	duty = period / 4;
	on = duty;
	off = period - duty;

	local_irq_disable();
	for (i = 1; i < MAX_SIZE; i += 2) {
		if (ir_data.signal[i] == 0)
			break;

		for (j = 0; j < ir_data.signal[i]; j++) {
			gpio_direction_output(irled_gpios
					      [GPIO_IRDA_CONTROL].gpio, 1);
			__udelay(on);
			gpio_direction_output(irled_gpios
					      [GPIO_IRDA_CONTROL].gpio, 0);
			__udelay(off);
		}

		period = (MICRO_SEC / ir_data.signal[0]) + ir_data.off_offset;

		off_period = ir_data.signal[i + 1] * period;

		if (off_period <= 9999) {
			if (off_period > 1000) {
				__udelay(off_period % 1000);
				mdelay(off_period / 1000);
			} else
				__udelay(off_period);
		} else {
			local_irq_enable();
			__udelay(off_period % 1000);
			mdelay(off_period / 1000);
			local_irq_disable();
		}
	}
	gpio_direction_output(irled_gpios[GPIO_IRDA_CONTROL].gpio, 1);
	__udelay(on);
	gpio_direction_output(irled_gpios[GPIO_IRDA_CONTROL].gpio, 0);
	__udelay(off);

	local_irq_enable();

	omap_cpufreq_min_limit_free(DVFS_LOCK_ID_IR_LED);
	omap_cpufreq_max_limit_free(DVFS_LOCK_ID_IR_LED);

	gpio_direction_output(irled_gpios[GPIO_IRDA_EN].gpio, 0);
}

static ssize_t irled_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int i;
	unsigned int _data;

	for (i = 0; i < MAX_SIZE; i++) {
		if (sscanf(buf++, "%u", &_data) == 1) {
			ir_data.signal[i] = _data;
			if (ir_data.signal[i] == 0)
				break;
			while (_data > 0) {
				buf++;
				_data /= 10;
			}
		} else {
			ir_data.signal[i] = 0;
			break;
		}
	}

	if (!work_pending(&ir_data.work))
		schedule_work(&ir_data.work);

	return size;
}

static ssize_t irled_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int i;
	char *bufp = buf;

	for (i = 0; i < MAX_SIZE; i++) {
		if (ir_data.signal[i] == 0)
			break;
		else {
			bufp += sprintf(bufp, "%u,", ir_data.signal[i]);
			pr_info("%u,", ir_data.signal[i]);
		}
	}
	return strlen(buf);
}

static ssize_t check_ir_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int _data = 1;
	return sprintf(buf, "%d\n", _data);
}

static ssize_t clock_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	unsigned int _data;
	if (sscanf(buf, "%u", &_data) == 1)
		if (_data == 300000 || _data == 600000 || _data == 800000
		    || _data == 1008000)
			ir_data.cpu_frequency = _data;

	return size;
}

static ssize_t clock_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	sprintf(buf, "%u\n", ir_data.cpu_frequency);
	return strlen(buf);
}

static ssize_t on_offset_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	int _data;
	if (sscanf(buf, "%d\n", &_data) == 1)
		ir_data.on_offset = _data;

	return size;
}

static ssize_t on_offset_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	sprintf(buf, "%d\n", ir_data.on_offset);
	return strlen(buf);
}

static ssize_t off_offset_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	int _data;
	if (sscanf(buf, "%d\n", &_data) == 1)
		ir_data.off_offset = _data;

	return size;
}

static ssize_t off_offset_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	sprintf(buf, "%d\n", ir_data.off_offset);
	return strlen(buf);
}

static DEVICE_ATTR(ir_send, S_IRUGO | S_IWUSR | S_IWGRP,
		   irled_show, irled_store);
static DEVICE_ATTR(check_ir, S_IRUGO, check_ir_show, NULL);

static DEVICE_ATTR(clock, S_IRUGO | S_IWUSR | S_IWGRP, clock_show, clock_store);
static DEVICE_ATTR(on_offset, S_IRUGO | S_IWUSR | S_IWGRP,
		   on_offset_show, on_offset_store);
static DEVICE_ATTR(off_offset, S_IRUGO | S_IWUSR | S_IWGRP,
		   off_offset_show, off_offset_store);

static int __init irled_init(void)
{
	int ret;
	struct device *irled_dev;

	ir_data.cpu_frequency = CLOCK_VALUE;
	ir_data.on_offset = ON_OFFSET_VALUE;
	ir_data.off_offset = OFF_OFFSET_VALUE;

	INIT_WORK(&ir_data.work, irled_work);

	irled_dev = device_create(sec_class, NULL, 0, &ir_data, "sec_ir");

	if (unlikely(IS_ERR(irled_dev))) {
		pr_err("irled: failed create irled device\n");
		goto err_create_dev;
	}

	ret = device_create_file(irled_dev, &dev_attr_ir_send);
	if (unlikely(ret < 0)) {
		pr_err("irled: failed create device file\n");
		goto err_create_dev_file1;
	}

	ret = device_create_file(irled_dev, &dev_attr_check_ir);
	if (unlikely(ret < 0)) {
		pr_err("irled: failed create device file\n");
		goto err_create_dev_file2;
	}

	ret = device_create_file(irled_dev, &dev_attr_clock);
	if (unlikely(ret < 0)) {
		pr_err("irled: failed create device file\n");
		goto err_create_dev_file3;
	}

	ret = device_create_file(irled_dev, &dev_attr_on_offset);
	if (unlikely(ret < 0)) {
		pr_err("irled: failed create device file\n");
		goto err_create_dev_file4;
	}

	ret = device_create_file(irled_dev, &dev_attr_off_offset);
	if (unlikely(ret < 0)) {
		pr_err("irled: failed create device file\n");
		goto err_create_dev_file5;
	}

	return 0;

err_create_dev_file5:
	device_remove_file(irled_dev, &dev_attr_on_offset);
err_create_dev_file4:
	device_remove_file(irled_dev, &dev_attr_clock);
err_create_dev_file3:
	device_remove_file(irled_dev, &dev_attr_check_ir);
err_create_dev_file2:
	device_remove_file(irled_dev, &dev_attr_ir_send);
err_create_dev_file1:
	device_destroy(sec_class, irled_dev->devt);
err_create_dev:
	return -1;
}

int __init omap4_espresso_irled_init(void)
{
	int ret = 0;
	int i;
	unsigned int boardtype = omap4_espresso_get_board_type();

	if (system_rev > 6 && boardtype != SEC_MACHINE_ESPRESSO_USA_BBY)
		return 0;

	for (i = 0; i < ARRAY_SIZE(irled_gpios); i++)
		irled_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(irled_gpios[i].label);
	gpio_request_array(irled_gpios, ARRAY_SIZE(irled_gpios));

	ret = irled_init();
	if (ret < 0) {
		pr_err("irled: irled_init failed\n");
		for (i = 0; i < ARRAY_SIZE(irled_gpios); i++)
			gpio_free(irled_gpios[i].gpio);
	}

	return ret;
}

late_initcall(omap4_espresso_irled_init);

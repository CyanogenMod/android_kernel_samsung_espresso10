/* arch/arm/mach-omap2/board-t1-vibrator.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd
 *
 * Based on mach-omap2/board-tuna-vibrator.c
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

#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <linux/sec-vibrator.h>
#include <plat/dmtimer.h>
#include <mach/cpufreq_limits.h>

#include "mux.h"
#include "board-t1.h"
#include "omap_muxtbl.h"


#define VIB_GPTIMER_NUM		10
#define PWM_DUTY_MAX		1463

static int vibrator_enable(int actr_idx, int on);
static int pwm_init(void);
static int pwm_set(int actr_idx, unsigned long force);

static struct omap_dm_timer *gptimer;
static unsigned long pwmval = PWM_DUTY_MAX;

static ssize_t pwmvalue_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	int count;

	count = sprintf(buf, "%lu\n", pwmval);
	pr_info("secvib: pwmval: %lu\n", pwmval);

	return count;
}

ssize_t pwmvalue_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{

	if (kstrtoul(buf, 0, &pwmval))
		pr_err("secvib: error in storing pwm value\n");

	pr_info("secvib: pwmval: %lu\n", pwmval);

	return size;
}
static DEVICE_ATTR(pwmvalue, S_IRUGO | S_IWUSR,
		pwmvalue_show, pwmvalue_store);

static int t1_create_secvib_sysfs(void)
{

	int ret;
	struct kobject *vibetonz_kobj;
	vibetonz_kobj = kobject_create_and_add("vibetonz", NULL);
	if (unlikely(!vibetonz_kobj))
		return -ENOMEM;

	ret = sysfs_create_file(vibetonz_kobj,
			&dev_attr_pwmvalue.attr);
	if (unlikely(ret < 0)) {
		pr_err("secvib: sysfs_create_file failed: %d\n", ret);
		return ret;
	}

	return 0;

}

static struct secvib_platform_data vib_pdata = {
	.vib_enable = vibrator_enable,
	.pwm_init = pwm_init,
	.pwm_set = pwm_set,
	.num_actuators = 1,
};

static int vibrator_enable(int actr_idx, int on)
{
	int ret;
	static bool cpufreq_lock;

	/* actr_idx will not be used for t1 becase we have only one */
	if (unlikely(gptimer == NULL))
		return -EINVAL;

	if (on) {
		if (!cpufreq_lock) {
			cpufreq_lock = true;
			omap_cpufreq_min_limit(DVFS_LOCK_ID_VIB, 1200000);
		}
		gpio_set_value(vib_pdata.gpio_en, 1);
		ret = omap_dm_timer_start(gptimer);
		if (unlikely(ret < 0))
			return ret;

	} else {
		omap_dm_timer_stop(gptimer);
		gpio_set_value(vib_pdata.gpio_en, 0);
		if (cpufreq_lock) {
			omap_cpufreq_min_limit_free(DVFS_LOCK_ID_VIB);
			cpufreq_lock = false;
		}
	}
	return 0;
}

static int pwm_init(void)
{
	int ret;


	gptimer = omap_dm_timer_request_specific(VIB_GPTIMER_NUM);
	if (unlikely(gptimer == NULL))
		return -EINVAL;

	ret = omap_dm_timer_set_source(gptimer,
		OMAP_TIMER_SRC_SYS_CLK);
	if (unlikely(ret < 0))
		goto err_dm_timer_src;

	omap_dm_timer_set_load(gptimer, 1, -PWM_DUTY_MAX);
	omap_dm_timer_set_match(gptimer, 1, -PWM_DUTY_MAX+10);
	omap_dm_timer_set_pwm(gptimer, 0, 1,
		OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
	omap_dm_timer_enable(gptimer);
	omap_dm_timer_write_counter(gptimer, -2);
	omap_dm_timer_disable(gptimer);

	return 0;

err_dm_timer_src:
	omap_dm_timer_free(gptimer);
	gptimer = NULL;

	return ret;
}

static int pwm_set(int actr_idx, unsigned long force)
{

	int pwm_duty;

	if (unlikely(gptimer == NULL))
		return -EINVAL;

	/*
	 * Formula for matching the user space force (-127 to +127)
	 * to Duty cycle.
	 * Duty cycle will vary from 0 to 45('0' means 0% duty cycle,
	 * '45' means 100% duty cycle.
	 * Also if user space force equals to -127 then duty
	 * cycle will be 0 (0%), if force equals to 0 duty cycle
	 * will be 22.5(50%), if +127 then duty cycle will
	 * be 45(100%)
	 */

	pwm_duty = ((force + 128)
			* (pwmval >> 1)/128);

	omap_dm_timer_set_load(gptimer, 1, -pwmval);
	omap_dm_timer_set_match(gptimer, 1, -pwm_duty);
	omap_dm_timer_set_pwm(gptimer, 0, 1,
			OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
	omap_dm_timer_enable(gptimer);
	omap_dm_timer_write_counter(gptimer, -2);

	return 0;
}

static struct platform_device vibrator_device = {
	.name =     VIB_DEVNAME,
	.id =       -1,
	.dev = {
		.platform_data = &vib_pdata,
	},
};

void __init omap4_t1_vibrator_init(void)
{

	vib_pdata.gpio_en = omap_muxtbl_get_gpio_by_name("MOTOR_EN");
	gpio_request(vib_pdata.gpio_en, "MOTOR_EN");
	gpio_direction_output(vib_pdata.gpio_en, 0);

	platform_device_register(&vibrator_device);
	t1_create_secvib_sysfs();

	return;
}

/* arch/arm/mach-omap2/board-espresso-vibrator.c
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

#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <asm/mach-types.h>

#include <../../../drivers/staging/android/timed_output.h>

#include "mux.h"
#include "board-espresso.h"

#define GPIO_MOTOR_EN	38

#define MAX_TIMEOUT		10000 /* 10sec */

static struct vibrator {
	struct wake_lock wklock;
	struct hrtimer timer;
	struct mutex lock;
	bool enabled;
} vibdata;

static void vibrator_off(void)
{
	if (!vibdata.enabled)
		return;
	gpio_set_value(GPIO_MOTOR_EN, 0);
	vibdata.enabled = false;
	wake_unlock(&vibdata.wklock);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibdata.timer)) {
		ktime_t r = hrtimer_get_remaining(&vibdata.timer);
		return ktime_to_ms(r);
	}

	return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	mutex_lock(&vibdata.lock);

	/* cancel previous timer and set GPIO according to value */
	hrtimer_cancel(&vibdata.timer);

	if (value) {
		wake_lock(&vibdata.wklock);

		gpio_set_value(GPIO_MOTOR_EN, 1);

		vibdata.enabled = true;

		if (value > 0) {
			if (value > MAX_TIMEOUT)
				value = MAX_TIMEOUT;

			hrtimer_start(&vibdata.timer,
				ns_to_ktime((u64)value * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
		}
	} else {
		vibrator_off();
	}

	mutex_unlock(&vibdata.lock);
}

static struct timed_output_dev to_dev = {
	.name		= "vibrator",
	.get_time	= vibrator_get_time,
	.enable		= vibrator_enable,
};

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	vibrator_off();
	return HRTIMER_NORESTART;
}

static void __init omap_vibrator_none_pads_cfg_mux(void)
{
	omap_mux_write(omap_mux_get("core"),
		OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
		OMAP4_CTRL_MODULE_PAD_GPMC_AD14_OFFSET);
}

static int __init vibrator_init(void)
{
	int ret;

	vibdata.enabled = false;

	hrtimer_init(&vibdata.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibdata.timer.function = vibrator_timer_func;

	wake_lock_init(&vibdata.wklock, WAKE_LOCK_SUSPEND, "vibrator");
	mutex_init(&vibdata.lock);

	ret = timed_output_dev_register(&to_dev);
	if (ret < 0)
		goto err_to_dev_reg;

	return 0;

err_to_dev_reg:
	mutex_destroy(&vibdata.lock);
	wake_lock_destroy(&vibdata.wklock);

	return -1;
}

int __init omap4_espresso_vibrator_init(void)
{
	int ret;

	if (!board_has_modem()) {
		omap_vibrator_none_pads_cfg_mux();
		return 0;
	}

	gpio_request_one(GPIO_MOTOR_EN, GPIOF_OUT_INIT_LOW, "MOTOR_EN");

	ret = vibrator_init();
	if (ret < 0) {
		pr_err("vib: vibrator_init fail.");
		gpio_free(GPIO_MOTOR_EN);
	}

	return ret;
}

/*
 * This is needed because the vibrator is dependent on omap_dm_timers which get
 * initialized at device_init time
 */
late_initcall(omap4_espresso_vibrator_init);

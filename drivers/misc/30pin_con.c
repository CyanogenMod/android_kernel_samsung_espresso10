/*
 * Copyright (C) 2008 Samsung Electronics, Inc.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/30pin_con.h>
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/mfd/tps6586x.h>
#include <asm/irq.h>

enum accessory_type {
	ACCESSORY_NONE = 0,
	ACCESSORY_OTG,
	ACCESSORY_TVOUT,
	ACCESSORY_LINEOUT,
	ACCESSORY_CARMOUNT,
	ACCESSORY_UNKNOWN,
};

enum dock_type {
	DOCK_NONE = 0,
	DOCK_DESK,
	DOCK_KEYBOARD,
};

struct acc_con_info {
	struct device *acc_dev;
	struct acc_con_platform_data *pdata;
	enum accessory_type current_accessory;
	enum dock_type current_dock;
	int accessory_irq;
	int dock_irq;
	int jig_irq;
	struct wake_lock wake_lock;
	struct delayed_work dwork;
	struct early_suspend early_suspend;
	struct delayed_work acc_con_work;
	struct mutex lock;
};

static void connector_detect_change(struct acc_con_info *acc, s16 *adc_res)
{
	int i;
	s16 adc_sum = 0;
	s16 adc_buff[5];
	s16 mili_volt;
	s16 adc_min;
	s16 adc_max;

	if (!acc->pdata->get_accessory_adc) {
		pr_err("30pin_c: no function to get adc.\n");
		return;
	}

	for (i = 0; i < 5; i++) {
		/*change this reading ADC function  */
		mili_volt = acc->pdata->get_accessory_adc();
		adc_buff[i] = mili_volt;
		adc_sum += adc_buff[i];
		if (i == 0) {
			adc_min = adc_buff[0];
			adc_max = adc_buff[0];
		} else {
			if (adc_max < adc_buff[i])
				adc_max = adc_buff[i];
			else if (adc_min > adc_buff[i])
				adc_min = adc_buff[i];
		}
		msleep(20);
	}
	*adc_res = (adc_sum - adc_max - adc_min) / 3;
}

static void _detected(struct acc_con_info *acc, int device, bool connected)
{
	pr_info("30pin_c: detect: device=%d\n", device);
	acc->pdata->detected(device, connected);
}

static void acc_dock_check(struct acc_con_info *acc, bool connected)
{
	char *env_ptr = "DOCK=none";
	char *stat_ptr;
	char *envp[3];
	int dock_state;

	if (connected) {
		stat_ptr = "STATE=online";
		if (acc->pdata->dock_keyboard_cb) {
			dock_state = acc->pdata->dock_keyboard_cb(connected);
			if (dock_state) {
				env_ptr = "DOCK=keyboard";
				acc->current_dock = DOCK_KEYBOARD;
				_detected(acc, P30_KEYBOARDDOCK, 1);
			} else {
				env_ptr = "DOCK=deskdock";
				acc->current_dock = DOCK_DESK;
				_detected(acc, P30_DESKDOCK, 1);
			}
		} else {
			env_ptr = "DOCK=deskdock";
			dock_state = DOCK_DESK;
			_detected(acc, P30_DESKDOCK, connected);
		}
	} else {
		if (acc->pdata->dock_keyboard_cb)
			acc->pdata->dock_keyboard_cb(connected);
		stat_ptr = "STATE=offline";

		if (acc->current_dock == DOCK_KEYBOARD)
			_detected(acc, P30_KEYBOARDDOCK, 0);
		else
			_detected(acc, P30_DESKDOCK, 0);
	}

	envp[0] = env_ptr;
	envp[1] = stat_ptr;
	envp[2] = NULL;
	kobject_uevent_env(&acc->acc_dev->kobj, KOBJ_CHANGE, envp);
}

static irqreturn_t acc_con_interrupt(int irq, void *ptr)
{
	struct acc_con_info *acc = ptr;
	static int post_state = -1;
	int cur_state;

	/* check the flag MHL or keyboard */
	cur_state = gpio_get_value(acc->pdata->accessory_irq_gpio);

	pr_info("accessory_int gpio stat = 0x%x\n", cur_state);

	if (post_state == cur_state) {
		pr_warning("30pin: duplication dock detect, ignore(%d)",
								cur_state);
		return IRQ_HANDLED;
	}

	wake_lock(&acc->wake_lock);

	if (cur_state)
		acc_dock_check(acc, false);
	else
		acc_dock_check(acc, true);

	post_state = cur_state;
	wake_unlock(&acc->wake_lock);

	return IRQ_HANDLED;
}

static int acc_con_interrupt_init(struct acc_con_info *acc)
{
	int ret;

	if (!acc->pdata->accessory_irq_gpio) {
		pr_warning("no accessory_irq gpio registed.");
		return -1;
	}

	acc->accessory_irq = gpio_to_irq(acc->pdata->accessory_irq_gpio);

	ret = request_threaded_irq(acc->accessory_irq, NULL, acc_con_interrupt,
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				   IRQF_NO_SUSPEND, "accessory_detect", acc);
	if (unlikely(ret < 0)) {
		pr_err("30pin_c: request accessory_irq failed.\n");
		return ret;
	}

	ret = enable_irq_wake(acc->accessory_irq);
	if (unlikely(ret < 0)) {
		pr_err("30pin_c: enable accessory_irq failed.\n");
		return ret;
	}

	return 0;
}

static void acc_notified(struct acc_con_info *acc, s16 acc_adc)
{
	char *env_ptr;
	char *stat_ptr;
	char *envp[3];

	pr_info("adc change notified: acc_adc = %d\n", acc_adc);

	/*
	 * P30 Standard
	 * -------------------------------------------------------------
	 * Accessory		Vacc [V]		adc
	 * -------------------------------------------------------------
	 * OTG			2.2 (2.1~2.3)		2731 (2606~2855)
	 * Analog TV Cable	1.8 (1.7~1.9)		2234 (2110~2359)
	 * Car Mount		1.38 (1.28~1.48)	1715 (1590~1839)
	 * 3-pole Earjack	0.99 (0.89~1.09)	1232 (1107~1356)
	 * -------------------------------------------------------------
	 */

	if (acc_adc) {
		if ((2600 < acc_adc) && (acc_adc < 2860)) {
			/* Camera Connection Kit */
			env_ptr = "ACCESSORY=OTG";
			acc->current_accessory = ACCESSORY_OTG;
			_detected(acc, P30_OTG, 1);
		} else if ((2110 < acc_adc) && (acc_adc < 2360)) {
			/* Analog TV Out Cable */
			env_ptr = "ACCESSORY=TV";
			acc->current_accessory = ACCESSORY_TVOUT;
			_detected(acc, P30_ANAL_TV_OUT, 1);
		} else if ((1590 < acc_adc) && (acc_adc < 1840)) {
			/* Car Mount (charge 5V/2A) */
			env_ptr = "ACCESSORY=carmount";
			acc->current_accessory = ACCESSORY_CARMOUNT;
			_detected(acc, P30_CARDOCK, 1);
		} else if ((1100 < acc_adc) && (acc_adc < 1360)) {
			/* 3-Pole Ear-Jack with Deskdock*/
			env_ptr = "ACCESSORY=lineout";
			acc->current_accessory = ACCESSORY_LINEOUT;
			_detected(acc, P30_EARJACK_WITH_DOCK, 1);
		} else {
			pr_warning("wrong adc range filter.\n");
			return;
		}

		stat_ptr = "STATE=online";
		envp[0] = env_ptr;
		envp[1] = stat_ptr;
		envp[2] = NULL;

		if (acc->current_accessory == ACCESSORY_OTG)
			msleep(50);

		kobject_uevent_env(&acc->acc_dev->kobj, KOBJ_CHANGE, envp);
	} else {
		if (acc->current_accessory == ACCESSORY_OTG) {
			env_ptr = "ACCESSORY=OTG";
			_detected(acc, P30_OTG, 0);
		} else if (acc->current_accessory == ACCESSORY_TVOUT) {
			env_ptr = "ACCESSORY=TV";
			_detected(acc, P30_ANAL_TV_OUT, 0);
		} else if (acc->current_accessory == ACCESSORY_LINEOUT) {
			env_ptr = "ACCESSORY=lineout";
			_detected(acc, P30_EARJACK_WITH_DOCK, 0);
		} else if (acc->current_accessory == ACCESSORY_CARMOUNT) {
			env_ptr = "ACCESSORY=carmount";
			_detected(acc, P30_CARDOCK, 0);
		} else
			env_ptr = "ACCESSORY=unknown";

		acc->current_accessory = ACCESSORY_NONE;
		stat_ptr = "STATE=offline";
		envp[0] = env_ptr;
		envp[1] = stat_ptr;
		envp[2] = NULL;
		kobject_uevent_env(&acc->acc_dev->kobj, KOBJ_CHANGE, envp);
	}
}

static irqreturn_t acc_id_interrupt(int irq, void *ptr)
{
	struct acc_con_info *acc = ptr;
	int acc_id_val;
	s16 adc_val;
	int delay = 10;
	static int post_state = -1;

	acc_id_val = gpio_get_value(acc->pdata->dock_irq_gpio);
	pr_info("accessorry_id irq handler: dock_irq gpio val = %d\n",
			acc_id_val);

	if (acc_id_val == post_state) {
		pr_warning("30pin: duplicated otg connection event,ignore(%d).",
								acc_id_val);
		return IRQ_HANDLED;
	}

	while (delay > 0) {
		if (acc_id_val != gpio_get_value(acc->pdata->dock_irq_gpio))
			return IRQ_HANDLED;
		usleep_range(10000, 11000);
		delay--;
	}

	if (acc_id_val) {
		pr_info("Accessory detached");
		acc_notified(acc, false);
	} else {
		wake_lock(&acc->wake_lock);
		msleep(420); /* workaround for jack */

		connector_detect_change(acc, &adc_val);
		pr_info("Accessory attached, adc=%d\n", adc_val);

		acc_notified(acc, adc_val);
		wake_unlock(&acc->wake_lock);
	}

	post_state = acc_id_val;

	return IRQ_HANDLED;
}

static int acc_id_interrupt_init(struct acc_con_info *acc)
{
	int ret;

	if (!acc->pdata->dock_irq_gpio) {
		pr_warning("acc_id_interrupt_init: no dock_irq gpio registed.");
		return -1;
	}

	acc->dock_irq = gpio_to_irq(acc->pdata->dock_irq_gpio);
	ret = request_threaded_irq(acc->dock_irq, NULL, acc_id_interrupt,
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				   IRQF_NO_SUSPEND, "dock_detect", acc);
	if (unlikely(ret < 0)) {
		pr_err("request dock_irq failed.\n");
		return ret;
	}

	ret = enable_irq_wake(acc->dock_irq);
	if (unlikely(ret < 0)) {
		pr_err("30pin_c: enable dock_irq failed.\n");
		return ret;
	}

	return 0;
}

static irqreturn_t jig_on_int_handler(int irq, void *ptr)
{
	struct acc_con_info *acc = ptr;
	int cur_state;
	int post_state = -1;

	cur_state = gpio_get_value(acc->pdata->jig_on_gpio);
	pr_info("jig_on_int_handler, jig_on gpio val = %d\n", cur_state);

	if (post_state == cur_state) {
		pr_warning("30pin_c: jig_on: duplication jig detect, ignore");
		return IRQ_HANDLED;
	}

	post_state = cur_state;

	if (cur_state)
		_detected(acc, P30_JIG, 1);
	else
		_detected(acc, P30_JIG, 0);

	return IRQ_HANDLED;
}

static int jig_on_interrupt_init(struct acc_con_info *acc)
{
	int ret;

	if (!acc->pdata->jig_on_gpio) {
		pr_warning("30pin_c: no jig_on gpio registed.");
		return -1;
	}

	acc->jig_irq = gpio_to_irq(acc->pdata->jig_on_gpio);
	ret = request_threaded_irq(acc->jig_irq, NULL, jig_on_int_handler,
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				   IRQF_NO_SUSPEND, "jig_detect", acc);
	if (unlikely(ret < 0)) {
		pr_err("request jig_irq failed.\n");
		return ret;
	}

	ret = enable_irq_wake(acc->jig_irq);
	if (unlikely(ret < 0)) {
		pr_err("30pin_c: enable jig_irq failed.\n");
		return ret;
	}

	return 0;
}

static void initial_connection_check(struct acc_con_info *acc)
{
	int dock_state;
	int otg_state;
	int jig_state;
	s16 adc_val;

	/* checks dock connectivity before registers dock irq */
	dock_state = gpio_get_value(acc->pdata->accessory_irq_gpio);
	if (!dock_state)
		acc_dock_check(acc, true);

	/* checks otg connectivity before registers otg irq */
	otg_state = gpio_get_value(acc->pdata->dock_irq_gpio);
	if (!otg_state) {
		wake_lock(&acc->wake_lock);
		msleep(420); /* workaround for jack */
		connector_detect_change(acc, &adc_val);
		acc_notified(acc, adc_val);
		wake_unlock(&acc->wake_lock);
	}

	/* checks jig connectivity before registers jig irq */
	jig_state = gpio_get_value(acc->pdata->jig_on_gpio);
	if (jig_state)
		_detected(acc, P30_JIG, 1);
}

static void acc_delay_worker(struct work_struct *work)
{
	struct acc_con_info *acc = container_of(work,
		struct acc_con_info, dwork.work);
	int ret;

	initial_connection_check(acc);

	ret = acc_con_interrupt_init(acc);
	if (unlikely(ret < 0)) {
		pr_err("[Keyboard] acc_con_interrupt_init");
		free_irq(acc->accessory_irq, acc);
	}

	ret = acc_id_interrupt_init(acc);
	if (ret) {
		pr_err("[Keyboard] acc_id_interrupt_init");
		free_irq(acc->dock_irq, acc);
	}

	ret = jig_on_interrupt_init(acc);
	if (ret) {
		pr_err("[Keyboard] jig_on_interrupt_init");
		free_irq(acc->jig_irq, acc);
	}
}

static int acc_con_probe(struct platform_device *pdev)
{
	struct acc_con_info *acc;
	struct acc_con_platform_data *pdata = pdev->dev.platform_data;

	if (pdata == NULL) {
		pr_err("30pin_c: no pdata\n");
		return -ENODEV;
	}

	acc = kzalloc(sizeof(*acc), GFP_KERNEL);
	if (!acc)
		return -ENOMEM;

	acc->pdata = pdata;
	acc->current_dock = DOCK_NONE;
	acc->current_accessory = ACCESSORY_NONE;

	dev_set_drvdata(&pdev->dev, acc);

	acc->acc_dev = &pdev->dev;

	wake_lock_init(&acc->wake_lock, WAKE_LOCK_SUSPEND, "30pin_con");

	INIT_DELAYED_WORK(&acc->dwork, acc_delay_worker);
	schedule_delayed_work(&acc->dwork, msecs_to_jiffies(10000));

	return 0;
}

static int acc_con_remove(struct platform_device *pdev)
{
	struct acc_con_info *acc = platform_get_drvdata(pdev);

	free_irq(acc->accessory_irq, acc);
	free_irq(acc->dock_irq, acc);
	free_irq(acc->jig_irq, acc);
	kfree(acc);

	return 0;
}

static struct platform_driver acc_con_driver = {
	.probe		= acc_con_probe,
	.remove		= acc_con_remove,
	.driver		= {
		.name		= "acc_con",
		.owner		= THIS_MODULE,
	},
};

static int __init acc_con_init(void)
{
	return platform_driver_register(&acc_con_driver);
}

static void __exit acc_con_exit(void)
{
	platform_driver_unregister(&acc_con_driver);
}

late_initcall(acc_con_init);
module_exit(acc_con_exit);

MODULE_AUTHOR("Kyungrok Min <gyoungrok.min@samsung.com>");
MODULE_DESCRIPTION("acc connector driver");
MODULE_LICENSE("GPL");

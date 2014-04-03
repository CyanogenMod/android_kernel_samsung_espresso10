/*
 * SAMSUNG battery manager driver for Android
 *
 * Copyright (C) 2011 SAMSUNG ELECTRONICS.
 * HongMin Son(hongmin.son@samsung.com)
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/android_alarm.h>
#include <linux/proc_fs.h>
#include <linux/usb/otg.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/battery.h>
#include <linux/bat_manager.h>
#include <linux/gpio.h>

#define FAST_POLL	(1 * 40)
#define SLOW_POLL	(10 * 60)

#define STATUS_BATT_REAL_FULL		0x1
#define STATUS_BATT_CHARGE_TIME_OUT	0x2
#define STATUS_BATT_ABNOMAL_TEMP	0x4
#define STATUS_BATT_CHARGE_1ST_FULL	0x8
#define STATUS_BATT_CF_OPEN 0x10

struct charger_device_info {
	struct device		*dev;
	struct work_struct	bat_work;
	struct work_struct	charger_work;
	struct batman_platform_data *pdata;
	struct battery_manager_callbacks callbacks;
	struct bat_information	bat_info;

	struct otg_transceiver	*transceiver;
	struct notifier_block	otg_nb;

	struct power_supply	psy_bat;
	struct power_supply	psy_usb;
	struct power_supply	psy_ac;
	struct alarm		alarm;
	struct workqueue_struct *monitor_wqueue;
	struct wake_lock	work_wake_lock;

	enum cable_type_t	cable_status;
	int			present;
	int			charge_status;
	int			discharge_status;
	unsigned long           chg_limit_time;
	unsigned int		polling_interval;
	unsigned int		full_charged_step;
	int                     slow_poll;
	bool			once_fully_charged;
	bool			is_cable_attached;
	ktime_t                 last_poll;
};

static char *supply_list[] = {
	"battery",
};

static enum power_supply_property batman_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static enum power_supply_property batman_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static void set_cable(struct battery_manager_callbacks *ptr,
		enum cable_type_t status)
{
	struct charger_device_info *di = container_of(ptr,
			struct charger_device_info, callbacks);

	dev_info(di->dev, "cable type : %d\n", status);
	di->cable_status = status;
	if (gpio_get_value(di->pdata->bat_removal)) {
		di->charge_status = di->cable_status ?
			POWER_SUPPLY_STATUS_CHARGING :
				POWER_SUPPLY_STATUS_DISCHARGING;
	} else {
		di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	if (di->cable_status == CABLE_TYPE_NONE) {
		di->once_fully_charged = false;
		di->chg_limit_time = 0;
		di->pdata->set_term_current(1);
		di->discharge_status = 0;
		di->full_charged_step = 0;
	} else if (di->cable_status == CABLE_TYPE_AC) {
		di->pdata->set_charge_current(CABLE_TYPE_AC);
	} else {
		di->pdata->set_charge_current(CABLE_TYPE_USB);
	}

	power_supply_changed(&di->psy_ac);
	power_supply_changed(&di->psy_usb);

	wake_lock(&di->work_wake_lock);
	queue_work(di->monitor_wqueue, &di->bat_work);
}

static void batman_set_full(struct battery_manager_callbacks *ptr)
{
	struct charger_device_info *di = container_of(ptr,
			struct charger_device_info, callbacks);
	di->once_fully_charged = true;
	di->discharge_status = STATUS_BATT_REAL_FULL;
	di->pdata->set_term_current(1);
	di->pdata->set_charger_en(0);
	di->full_charged_step = 1;

	wake_lock(&di->work_wake_lock);
	queue_work(di->monitor_wqueue, &di->bat_work);
}

static int batman_bat_get_property(struct power_supply *bat_ps,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_device_info *di = container_of(bat_ps,
			struct charger_device_info, psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!gpio_get_value(di->pdata->bat_removal) &&
			gpio_get_value(di->pdata->jig_on)) {
				di->bat_info.health =
					POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			di->discharge_status = STATUS_BATT_CF_OPEN;
		}
		val->intval = di->charge_status;
		dev_dbg(di->dev, "charge_status : %d\n",
				val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = di->bat_info.health;
		dev_dbg(di->dev, "health value : %d\n",
				val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = di->bat_info.temp;
		dev_dbg(di->dev, "temperature value : %d\n",
				val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* battery is always online */
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = di->bat_info.vcell;
		dev_info(di->dev, "voltage value : %d\n",
				val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = di->bat_info.soc;
		dev_info(di->dev, "soc value : %d\n",
				val->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int batman_usb_get_property(struct power_supply *ps,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_device_info *di =
		container_of(ps, struct charger_device_info, psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = (di->cable_status == CABLE_TYPE_USB);

	return 0;
}

static int batman_ac_get_property(struct power_supply *ps,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_device_info *di =
		container_of(ps, struct charger_device_info, psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (di->cable_status == CABLE_TYPE_AC);

	return 0;
}

static void batman_get_temp_status(struct charger_device_info *di)
{
	if (!di->pdata->get_temp) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_UNKNOWN;
		return;
	}

	di->bat_info.temp = di->pdata->get_temp();

	if (di->bat_info.temp >= di->pdata->high_block_temp) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (di->bat_info.temp <= di->pdata->high_recover_temp &&
			di->bat_info.temp >= di->pdata->low_recover_temp) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (di->bat_info.temp <= di->pdata->low_block_temp) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_COLD;
	}
}

static int batman_charging_status_check(struct charger_device_info *di)
{
	int ret = 0;
	ktime_t ktime;
	struct timespec cur_time;
	ktime = alarm_get_elapsed_realtime();
	cur_time = ktime_to_timespec(ktime);

	if (!gpio_get_value(di->pdata->bat_removal) &&
		gpio_get_value(di->pdata->jig_on)) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		di->discharge_status = STATUS_BATT_CF_OPEN;
	}

	switch (di->charge_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (di->chg_limit_time &&
				cur_time.tv_sec > di->chg_limit_time &&
				di->bat_info.soc == 100) {
			di->once_fully_charged = true;
			di->charge_status = POWER_SUPPLY_STATUS_FULL;
			di->discharge_status = STATUS_BATT_CHARGE_TIME_OUT;
		} else if (di->bat_info.health ==
				POWER_SUPPLY_HEALTH_OVERHEAT ||
				di->bat_info.health ==
				POWER_SUPPLY_HEALTH_COLD) {
			di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			di->discharge_status = STATUS_BATT_ABNOMAL_TEMP;
		} else if (di->full_charged_step) {
			di->charge_status = POWER_SUPPLY_STATUS_FULL;
		}
		break;

	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (di->bat_info.temp <= di->pdata->high_recover_temp &&
			di->bat_info.temp >=
			di->pdata->low_recover_temp &&
			di->bat_info.health !=
				POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
			di->discharge_status = 0x0;
			di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
		}
		break;

	case POWER_SUPPLY_STATUS_FULL:
		if (di->pdata->get_fuel_value) {
			di->bat_info.vcell =
				di->pdata->get_fuel_value(READ_FG_VCELL);
		}
		if (di->bat_info.vcell < di->pdata->recharge_voltage) {
			di->discharge_status = 0x0;
		} else if (di->chg_limit_time &&
				cur_time.tv_sec > di->chg_limit_time) {
			di->discharge_status = STATUS_BATT_CHARGE_TIME_OUT;
		} else if (di->bat_info.health ==
				POWER_SUPPLY_HEALTH_OVERHEAT ||
				di->bat_info.health ==
				POWER_SUPPLY_HEALTH_COLD) {
			di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			di->discharge_status = STATUS_BATT_ABNOMAL_TEMP;
		}
		break;
	}

	if (!di->discharge_status) {

		di->pdata->set_charger_en(1);

		if (!di->chg_limit_time) {
			di->chg_limit_time =
				di->once_fully_charged ?
				cur_time.tv_sec +
				di->pdata->limit_recharging_time :
				cur_time.tv_sec +
				di->pdata->limit_charging_time;
		}

	} else {
		di->pdata->set_charger_en(0);
		di->chg_limit_time = 0;
	}

	return ret;
}

static void charger_detect_work(struct work_struct *work)
{
	struct charger_device_info *di =
		container_of(work, struct charger_device_info, charger_work);

	if (di->is_cable_attached)
		di->cable_status = di->pdata->get_charger_type();

	else
		di->cable_status = CABLE_TYPE_NONE;

/*
	if ((di->cable_status == CABLE_TYPE_AC) && (di->pdata->bootmode != 5))
		bq2415x_config_control_reg_limit();
*/

	di->pdata->set_charger_state(di->cable_status);
	di->pdata->set_charger_en(di->is_cable_attached);

/*
	if ((di->cable_status == CABLE_TYPE_AC) && (di->pdata->bootmode != 5)) {
		msleep(500);
		bq2415x_config_control_reg_nolimit();
	}
*/
	if (di->cable_status == CABLE_TYPE_NONE) {
		di->once_fully_charged = false;
		di->chg_limit_time = 0;
		di->pdata->set_term_current(1);
		di->discharge_status = 0;
		di->full_charged_step = 0;
	}

	if (!gpio_get_value(di->pdata->bat_removal) &&
			gpio_get_value(di->pdata->jig_on)) {
		di->bat_info.health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		di->discharge_status = STATUS_BATT_CF_OPEN;
	}
	power_supply_changed(&di->psy_ac);
	power_supply_changed(&di->psy_usb);
	queue_work(di->monitor_wqueue, &di->bat_work);
}

static void batman_program_alarm(struct charger_device_info *di, int seconds)
{
	ktime_t low_interval = ktime_set(seconds - 10, 0);
	ktime_t slack = ktime_set(20, 0);
	ktime_t next;

	next = ktime_add(di->last_poll, low_interval);
	alarm_start_range(&di->alarm, next, ktime_add(next, slack));
}

static void battery_manager_work(struct work_struct *work)
{
	struct charger_device_info *di =
		container_of(work, struct charger_device_info, bat_work);

	int prev_status = di->charge_status;
	int prev_soc = di->bat_info.soc;

	unsigned long flags;
	struct timespec ts;

	batman_get_temp_status(di);

	if (di->pdata->get_fuel_value) {
		di->bat_info.vcell =
			di->pdata->get_fuel_value(READ_FG_VCELL);
		di->bat_info.soc =
			di->pdata->get_fuel_value(READ_FG_SOC);
		di->bat_info.fg_current =
			di->pdata->get_fuel_value(READ_FG_CURRENT);
#if 0
		di->bat_info.batt_online =
			di->pdata->get_fuel_value(READ_FG_STATUS);
#endif
	}
	di->bat_info.temp =  di->pdata->get_temp();

	if (di->cable_status != CABLE_TYPE_NONE)
		batman_charging_status_check(di);

	if ((di->bat_info.soc != prev_soc) ||
		(di->charge_status != prev_status) || (di->bat_info.soc == 0))
		power_supply_changed(&di->psy_bat);

	di->last_poll = alarm_get_elapsed_realtime();
	ts = ktime_to_timespec(di->last_poll);

	pr_info("%s: charge_status : %d, ", di->charge_status);
	pr_info("health : %d, ", di->bat_info.health);
	pr_info("temp : %d, ", di->bat_info.temp);
	pr_info("vcell : %d, ", di->bat_info.vcell);
	pr_info("soc : %d, " , di->bat_info.soc);
	pr_info("fg_current : %d\n", di->bat_info.fg_current);

	local_irq_save(flags);
	wake_unlock(&di->work_wake_lock);
	batman_program_alarm(di, FAST_POLL);
	local_irq_restore(flags);
}

static void batman_battery_alarm(struct alarm *alarm)
{
	struct charger_device_info *di =
		container_of(alarm, struct charger_device_info, alarm);

	wake_lock(&di->work_wake_lock);
	queue_work(di->monitor_wqueue, &di->bat_work);
}

static int otg_handle_notification(struct notifier_block *nb,
	unsigned long event, void *unused)
{
	struct charger_device_info *di = container_of(nb,
			struct charger_device_info, otg_nb);

	switch (event) {
	case USB_EVENT_CHARGER:
		pr_info("%s: TA Charger Connected\n", __func__);
		di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
		di->is_cable_attached = true;
		break;
	case USB_EVENT_VBUS:
		pr_info("%s: USB Charger Connected\n", __func__);
		di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
		di->is_cable_attached = true;
		break;
	case USB_EVENT_NONE:
		pr_info("%s: Charger Disconnect\n", __func__);
		di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->is_cable_attached = false;
		break;
	default:
		return NOTIFY_OK;
	}

	wake_lock(&di->work_wake_lock);
	schedule_work(&di->charger_work);
	return NOTIFY_OK;
}

enum {
	BATT_VOL = 0,
	BATT_VOL_ADC,
	BATT_TEMP,
	BATT_TEMP_ADC,
	BATT_CHARGING_SOURCE,
	BATT_FG_SOC,
	BATT_RESET_SOC,
	BATT_TEMP_CHECK,
	BATT_FULL_CHECK,
	BATT_TYPE,
	BATT_CHARGE_MODE,
	BATT_DMB_CHECK,
};

static ssize_t battery_manager_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t battery_manager_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define SEC_BATTERY_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664, /*.owner = THIS_MODULE*/ },\
	.show = battery_manager_show_attrs,				\
	.store = battery_manager_store_attrs,				\
}

static struct device_attribute battery_manager_attrs[] = {
	SEC_BATTERY_ATTR(batt_vol),
	SEC_BATTERY_ATTR(batt_vol_adc),
	SEC_BATTERY_ATTR(batt_temp),
	SEC_BATTERY_ATTR(batt_temp_adc),
	SEC_BATTERY_ATTR(batt_charging_source),
	SEC_BATTERY_ATTR(fg_read_soc),
	SEC_BATTERY_ATTR(fg_reset_soc),
	SEC_BATTERY_ATTR(batt_temp_check),
	SEC_BATTERY_ATTR(batt_full_check),
	SEC_BATTERY_ATTR(batt_type),
	SEC_BATTERY_ATTR(batt_lp_charging),
};


static ssize_t battery_manager_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct power_supply *psy = dev_get_drvdata(dev);
	struct charger_device_info *di
		= container_of(psy, struct charger_device_info, psy_bat);

	int i = 0;
	int ret = 0;
	const ptrdiff_t off = attr - battery_manager_attrs;

	switch (off) {
	case BATT_VOL:
		if (di->pdata->get_fuel_value)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				di->pdata->get_fuel_value(READ_FG_VCELL));
		break;
	case BATT_TEMP:
		if (di->pdata->get_temp)
			i += scnprintf(buf + i, PAGE_SIZE - i,
					"%d\n", di->pdata->get_temp);
		break;
	case BATT_TEMP_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i,
				"%d\n", di->pdata->get_temp_adc());
		break;
	case BATT_FG_SOC:
		if (di->pdata->get_fuel_value)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					di->pdata->get_fuel_value(READ_FG_SOC));
		break;
	case BATT_CHARGE_MODE:
		ret = (di->pdata->bootmode == 5) ? 1 : 0;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", ret);
		break;
	case BATT_CHARGING_SOURCE:
		if (di->pdata->get_charger_type)
			i += scnprintf(buf + i, PAGE_SIZE - i,
					"%d\n",  di->pdata->get_charger_type());
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

static ssize_t battery_manager_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int x = 0;
	int ret = 0;

	struct power_supply *psy = dev_get_drvdata(dev);
	struct charger_device_info *di
		= container_of(psy, struct charger_device_info, psy_bat);

	const ptrdiff_t off = attr - battery_manager_attrs;

	switch (off) {
	case BATT_DMB_CHECK:
	if (sscanf(buf, "%d\n", &x) == 1) {
		if (x == 20) {
			di->pdata->set_charge_current(CABLE_TYPE_USB);
		} else {
			if (di->cable_status == CABLE_TYPE_AC)
				di->pdata->set_charge_current(CABLE_TYPE_AC);
			else if (di->cable_status == CABLE_TYPE_USB)
				di->pdata->set_charge_current(CABLE_TYPE_USB);
		}

		if (di->cable_status != CABLE_TYPE_NONE &&
			di->discharge_status == 0) {
				di->pdata->set_charger_en(0);
				msleep(500);
				di->pdata->set_charger_en(1);
		}
		ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int battery_manager_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(battery_manager_attrs); i++) {
		rc = device_create_file(dev, &battery_manager_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &battery_manager_attrs[i]);
succeed:
	return rc;
}

static int __devinit batman_probe(struct platform_device *pdev)
{
	int ret = 0;
	int charger_in = 0;
	struct charger_device_info *di;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->dev = &pdev->dev;
	di->pdata = pdev->dev.platform_data;

	di->psy_bat.name = "battery";
	di->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->psy_bat.properties = batman_battery_properties;
	di->psy_bat.num_properties = ARRAY_SIZE(batman_battery_properties);
	di->psy_bat.get_property = batman_bat_get_property;

	di->psy_usb.name = "usb";
	di->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	di->psy_usb.supplied_to = supply_list;
	di->psy_usb.num_supplicants = ARRAY_SIZE(supply_list);
	di->psy_usb.properties = batman_power_properties;
	di->psy_usb.num_properties = ARRAY_SIZE(batman_power_properties);
	di->psy_usb.get_property = batman_usb_get_property;

	di->psy_ac.name = "ac",
		di->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	di->psy_ac.supplied_to = supply_list;
	di->psy_ac.num_supplicants = ARRAY_SIZE(supply_list);
	di->psy_ac.properties = batman_power_properties;
	di->psy_ac.num_properties = ARRAY_SIZE(batman_power_properties);
	di->psy_ac.get_property = batman_ac_get_property;

	di->discharge_status = 0;
	di->cable_status = CABLE_TYPE_NONE;
	di->bat_info.health = POWER_SUPPLY_HEALTH_GOOD;

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &di->psy_bat);
	if (ret) {
		pr_err("Failed to register power supply psy_bat\n");
		goto err_supply_reg_bat;
	}
	ret = power_supply_register(&pdev->dev, &di->psy_usb);
	if (ret) {
		pr_err("Failed to register power supply psy_usb\n");
		goto err_supply_reg_usb;
	}
	ret = power_supply_register(&pdev->dev, &di->psy_ac);
	if (ret) {
		pr_err("Failed to register power supply psy_ac\n");
		goto err_supply_reg_ac;
	}

	di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;

	platform_set_drvdata(pdev, di);

	charger_in = gpio_get_value(di->pdata->ta_gpio) ? 0 : 1;

	di->transceiver = otg_get_transceiver();
	if (di->transceiver) {
		di->otg_nb.notifier_call = otg_handle_notification;
		ret = otg_register_notifier(di->transceiver, &di->otg_nb);
		if (ret) {
			pr_err("failure to register otg notifier\n");
			goto otg_reg_notifier_failed;
		}
	} else {
		pr_err("failure to otg get transceiver\n");
	}

	INIT_WORK(&di->bat_work, battery_manager_work);
	INIT_WORK(&di->charger_work, charger_detect_work);

	di->monitor_wqueue = create_workqueue(dev_name(&pdev->dev));
	if (!di->monitor_wqueue) {
		pr_err("Failed to create freezeable workqueue\n");
		ret = -ENOMEM;
		goto err_wqueue;
	}

	di->callbacks.set_cable = set_cable;
	di->callbacks.set_full_charge = batman_set_full;

	if (di->pdata->register_callbacks)
		di->pdata->register_callbacks(&di->callbacks);
	else
		pr_err("Failed to register set_cable callback function.\n");

	ret = battery_manager_create_attrs(di->psy_bat.dev);
	if (ret)
		pr_err("%s : Failed to create_attrs\n", __func__);

	wake_lock_init(&di->work_wake_lock, WAKE_LOCK_SUSPEND,
			"wakelock-charger");

	alarm_init(&di->alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
			batman_battery_alarm);

	if (charger_in) {
		pr_info("%s: power on checked TA\n", __func__);
		di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
		di->is_cable_attached = true;
	}

	schedule_work(&di->charger_work);
	pr_info("%s: Battery manager probe done\n", __func__);

	return 0;

err_wqueue:
	if (di->transceiver)
		otg_put_transceiver(di->transceiver);
otg_reg_notifier_failed:
	power_supply_unregister(&di->psy_ac);
err_supply_reg_ac:
	power_supply_unregister(&di->psy_usb);
err_supply_reg_usb:
	power_supply_unregister(&di->psy_bat);
err_supply_reg_bat:
	kfree(di);

	return ret;
}

static int __devexit batman_remove(struct platform_device *pdev)
{
	struct charger_device_info *di = platform_get_drvdata(pdev);

	power_supply_unregister(&di->psy_bat);
	power_supply_unregister(&di->psy_usb);
	power_supply_unregister(&di->psy_ac);
	alarm_cancel(&di->alarm);
	cancel_work_sync(&di->charger_work);
	flush_workqueue(di->monitor_wqueue);
	destroy_workqueue(di->monitor_wqueue);
	if (di->transceiver)
		otg_put_transceiver(di->transceiver);
	wake_lock_destroy(&di->work_wake_lock);

	return 0;
}

#ifdef CONFIG_PM
static int batman_suspend(struct device *pdev)
{
	struct charger_device_info *di = dev_get_drvdata(pdev);
	if (!di->is_cable_attached) {
		batman_program_alarm(di, SLOW_POLL);
		di->slow_poll = 1;
	}
	return 0;
}

static void batman_resume(struct device *pdev)
{
	struct charger_device_info *di = dev_get_drvdata(pdev);
	if (di->slow_poll) {
		batman_program_alarm(di, FAST_POLL);
		di->slow_poll = 0;
	}
}

#else
#define batman_suspend NULL
#define batman_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops batman_pm_ops = {
	.prepare = batman_suspend,
	.complete = batman_resume,
};

struct platform_driver battery_manager_pdrv = {
	.probe = batman_probe,
	.remove = __devexit_p(batman_remove),
	.driver = {
		.name = "battery_manager",
		.pm = &batman_pm_ops,
	},
};

static int __init batman_init(void)
{
	return platform_driver_register(&battery_manager_pdrv);
}
late_initcall(batman_init);

static void __exit batman_exit(void)
{
	platform_driver_unregister(&battery_manager_pdrv);
}
module_exit(batman_exit);

MODULE_AUTHOR("HongMin Son<hongmin.son@samsung.com>");
MODULE_LICENSE("GPL");

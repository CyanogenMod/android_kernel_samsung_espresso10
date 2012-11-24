/*
 *  max17040_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/max17040_battery.h>
#include <linux/slab.h>
#include <linux/android_alarm.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/workqueue.h>


#ifdef CONFIG_TEMP_BLOCK_FOR_CAM_RECORDING
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#endif


#define MAX17040_VCELL_MSB	0x02
#define MAX17040_VCELL_LSB	0x03
#define MAX17040_SOC_MSB	0x04
#define MAX17040_SOC_LSB	0x05
#define MAX17040_MODE_MSB	0x06
#define MAX17040_MODE_LSB	0x07
#define MAX17040_VER_MSB	0x08
#define MAX17040_VER_LSB	0x09
#define MAX17040_RCOMP_MSB	0x0C
#define MAX17040_RCOMP_LSB	0x0D
#define MAX17040_CMD_MSB	0xFE
#define MAX17040_CMD_LSB	0xFF

#define MAX17040_BATTERY_FULL	100
#define MAX17040_VCELL_FULL	4200000

#define HAS_ALERT_INTERRUPT(ver)	(ver >= 3)

#define FAST_POLL		(1 * 30)
#define SLOW_POLL		(10 * 60)

#define STATUS_CHARGABLE	0x0
#define STATUS_CHARGE_FULL	0x1
#define STATUS_ABNORMAL_TEMP	0x2
#define STATUS_CHARGE_TIMEOVER	0x3

#define MIN_SOC_CAPACITY	300
#define MAX_SOC_CAPACITY	96100

#define PDA_POWER_CHARGE_AC  (1 << 0)
#define PDA_POWER_CHARGE_USB (1 << 1)

#define OFFSET_MP3		(0x1 << 0)
#define OFFSET_VIDEO		(0x1 << 1)
#define OFFSET_CAMERA		(0x1 << 2)
#define OFFSET_CAMERA_RECORD	(0x1 << 3)

#define EVT_CASE (OFFSET_MP3 | \
		  OFFSET_VIDEO | \
		  OFFSET_CAMERA)

enum charger_mode {
	USB500 = 0,
	ISET,
};

enum {
	NOT_FULL = 0,
	FULL_CHARGED_PRE,
	FULL_CHARGED
};

struct max17040_chip {
	struct i2c_client		*client;
	struct work_struct		work;
	struct power_supply		battery;
	struct max17040_platform_data	*pdata;

	/* State Of Connect */
	int online;
	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int raw_soc;
	/* Raw State Of Charge */
	int status;
	/* Health of Battery */
	int bat_health;
	/* Temperature of Battery */
	int bat_temp;
	int bat_temp_adc;

	struct notifier_block	pm_notifier;
	struct wake_lock	work_wake_lock;
	struct wake_lock	lowbat_wake_lock;

#ifdef CONFIG_TEMP_BLOCK_FOR_CAM_RECORDING
	struct proc_dir_entry *max17040_battery_proc;
	struct proc_dir_entry *cam_recording_proc;
	char cam_recording_value[2];
#endif

	struct alarm	alarm;
	ktime_t last_poll;
	int slow_poll;
	int shutdown;
	/* chip version */
	u16 ver;

	int charger_status;
	bool is_full_charged;
	bool is_full_charged_pre;
	bool current_adjusted;
	bool lowbat_state;
	bool lowbat_irq;

	unsigned long chg_limit_time;
	bool event_mode;
	u32 event_list;
	u8 rcomp_val;
	u8 athd_val;

	bool is_timer_flag;
	struct delayed_work full_batt_work;
	int bat_removal_irq;
	struct work_struct bat_removal_work;
};

static struct wake_lock full_batt_wake_lock;

static int max17040_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->bat_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (!chip->pdata->get_bat_temp)
			return -ENODATA;
		val->intval = chip->bat_temp;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17040_set_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 const union power_supply_propval *val)
{
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		wake_lock(&chip->work_wake_lock);
		schedule_work(&chip->work);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max17040_write_reg(struct i2c_client *client, int reg, u16 val)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, cpu_to_be16(val));

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17040_read_reg(struct i2c_client *client, int reg, u16 *val)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		*val = 0;
		return ret;
	}

	*val = be16_to_cpu(ret);
	return 0;
}

static void max17040_reset(struct i2c_client *client)
{
	max17040_write_reg(client, MAX17040_CMD_MSB, 0x5400);

	msleep(125);

	max17040_write_reg(client, MAX17040_MODE_MSB, 0x4000);
}

static void max17040_get_vcell(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u16 val;

	if (!max17040_read_reg(client, MAX17040_VCELL_MSB, &val))
		chip->vcell = (val >> 4) * 1250;
	else
		dev_warn(&client->dev, "i2c error, not updating vcell\n");
}


static void max17040_get_soc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u16 regval;
	int val;
	int ret;
	unsigned char buf[2];

	if (max17040_read_reg(client, MAX17040_SOC_MSB, &regval)) {
		dev_warn(&client->dev, "i2c error, not updating soc\n");
		return;
	}
	val = (int) regval;

	buf[0] = (val >> 8);
	buf[1] = (val & 0xFF);

	val = buf[0];

	val = val * 1000;
	val = val + (buf[1]*3);

	chip->raw_soc = val;

	if (val <= MIN_SOC_CAPACITY) {
		chip->soc = 0;
		return;
	}
	ret = (val-MIN_SOC_CAPACITY)*100/(MAX_SOC_CAPACITY-MIN_SOC_CAPACITY);

	chip->soc =  (ret > 100) ? 100 : ret;
}

static void max17040_get_version(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u16 val;

	if (!max17040_read_reg(client, MAX17040_VER_MSB, &val)) {
		chip->ver = val;
		dev_info(&client->dev, "MAX17040 Fuel-Gauge Ver %d\n", val);
	} else {
		dev_err(&client->dev,
			"Error reading version, some features disabled\n");
	}
}

static void max17040_get_online(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	if (chip->pdata->battery_online)
		chip->online = chip->pdata->battery_online();
	else
		chip->online = 1;
}

static void max17040_get_status(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

#ifdef CONFIG_USE_CHG_ADC_POLLING
	if (!chip->pdata->charger_online ||
	    !chip->pdata->charger_enable ||
	    !chip->pdata->get_charging_source ||
	    !chip->pdata->get_charging_state) {
#else
	if (!chip->pdata->charger_online ||
	    !chip->pdata->get_charging_source ||
	    !chip->pdata->charger_enable) {
#endif
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if (chip->pdata->charger_online() &&
	    (chip->pdata->get_charging_source() ||
	     chip->pdata->bootmode == 5)) {
#ifdef CONFIG_USE_CHG_ADC_POLLING
		if (chip->charger_status == STATUS_CHARGABLE &&
		    chip->is_full_charged_pre)
			chip->status = POWER_SUPPLY_STATUS_FULL;
		else if (chip->pdata->get_charging_state())
#else
		if (chip->pdata->charger_enable())
#endif
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->status =
				chip->charger_status == STATUS_CHARGE_FULL ?
				POWER_SUPPLY_STATUS_FULL :
				POWER_SUPPLY_STATUS_NOT_CHARGING;

	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
		chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		chip->chg_limit_time = 0;
		chip->charger_status = STATUS_CHARGABLE;
		chip->is_full_charged = false;
#ifdef CONFIG_USE_CHG_ADC_POLLING
		chip->is_full_charged_pre = false;
#endif
	}
}

static void max17040_get_vf_status(struct max17040_chip *chip)
{
	int vf_present = 0;

	if (chip->pdata->battery_online) {
		vf_present = chip->pdata->battery_online();
		if (!vf_present &&
		    chip->bat_health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
			chip->bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			dev_info(&chip->client->dev, "vf error occur");
		} else if (vf_present &&
			   chip->bat_health ==
			   POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
			chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
			dev_info(&chip->client->dev, "vf error is recovered");
		}
	}
}

static void max17040_get_temp_status(struct max17040_chip *chip)
{
	int r;
	int t;
	int t_adc;

	if (!chip->pdata->get_bat_temp)
		return;

	r = chip->pdata->get_bat_temp(&t, &t_adc);

	if (r < 0) {
		dev_err(&chip->client->dev,
			"error %d reading battery temperature\n", r);
		chip->bat_health = POWER_SUPPLY_HEALTH_UNKNOWN;
		return;
	}

	chip->bat_temp = t;
	chip->bat_temp_adc = t_adc;

	if (chip->bat_temp >= chip->pdata->high_block_temp) {
		chip->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (chip->bat_temp <= chip->pdata->high_recover_temp &&
		chip->bat_temp >= chip->pdata->low_recover_temp) {
		chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->bat_temp <= chip->pdata->low_block_temp) {
		chip->bat_health = POWER_SUPPLY_HEALTH_COLD;
	}
}

static void max17040_charger_update(struct max17040_chip *chip)
{
	ktime_t ktime;
	struct timespec cur_time;
	int full_charge_state;

	if (!chip->pdata->is_full_charge || !chip->pdata->allow_charging)
		return;

	ktime = alarm_get_elapsed_realtime();
	cur_time = ktime_to_timespec(ktime);

	switch (chip->charger_status) {
	case STATUS_CHARGABLE:
#ifdef CONFIG_USE_CHG_ADC_POLLING
		if (chip->soc >= MAX17040_BATTERY_FULL &&
		    chip->vcell > chip->pdata->fully_charged_vol) {
			full_charge_state = NOT_FULL;
			if (chip->pdata->is_full_charge)
				full_charge_state =
					chip->pdata->is_full_charge(true);

			if (full_charge_state == FULL_CHARGED_PRE) {
				chip->is_full_charged_pre = true;
				chip->is_full_charged = false;
			} else if (full_charge_state == FULL_CHARGED) {
				chip->is_full_charged_pre = true;
				chip->is_full_charged = true;
			}
		}
#endif
		if (chip->is_full_charged &&
			chip->soc >= MAX17040_BATTERY_FULL &&
				chip->vcell > chip->pdata->fully_charged_vol) {
			chip->charger_status = STATUS_CHARGE_FULL;
			chip->is_timer_flag = true;
			chip->chg_limit_time = 0;
			chip->pdata->allow_charging(0);
		} else if (chip->chg_limit_time &&
				 cur_time.tv_sec > chip->chg_limit_time) {
			chip->charger_status = STATUS_CHARGE_TIMEOVER;
			chip->is_timer_flag = true;
			chip->chg_limit_time = 0;
			chip->pdata->allow_charging(0);
		} else if (chip->bat_health == POWER_SUPPLY_HEALTH_OVERHEAT ||
			   chip->bat_health == POWER_SUPPLY_HEALTH_COLD) {
			chip->charger_status = STATUS_ABNORMAL_TEMP;
			chip->chg_limit_time = 0;
			chip->pdata->allow_charging(0);
		} else if (chip->bat_health ==
			   POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
			chip->chg_limit_time = 0;
			chip->pdata->allow_charging(0);
		}

		if (chip->charger_status == STATUS_CHARGABLE  &&
		    chip->bat_health == POWER_SUPPLY_HEALTH_GOOD) {
			if (chip->pdata->get_charging_state) {
				if (!chip->pdata->get_charging_state()) {
					if (chip->pdata->restore_charger_mode)
						chip->pdata->
							restore_charger_mode();
					else
						chip->pdata->allow_charging(1);
				}
			} else if (chip->pdata->charger_enable) {
				if (!chip->pdata->charger_enable()) {
					if (chip->pdata->restore_charger_mode)
						chip->pdata->
							restore_charger_mode();
					else
						chip->pdata->allow_charging(1);
				}
			}
		}

#ifdef CONFIG_USE_CHG_ADC_POLLING
		if (!(chip->charger_status == STATUS_CHARGABLE ||
		      chip->charger_status == STATUS_CHARGE_FULL)) {
			chip->is_full_charged_pre = false;
			chip->is_full_charged = false;
			if (chip->pdata->is_full_charge)
				chip->pdata->is_full_charge(false);
		}
#endif
		break;

	case STATUS_CHARGE_FULL:
		if (chip->vcell <= chip->pdata->recharge_vol) {
			chip->charger_status = STATUS_CHARGABLE;
#ifdef CONFIG_USE_CHG_ADC_POLLING
			chip->is_full_charged = false;
			if (chip->pdata->is_full_charge)
				chip->pdata->is_full_charge(false);

#endif
			if (chip->pdata->restore_charger_mode)
				chip->pdata->
					restore_charger_mode();
			else
				chip->pdata->allow_charging(1);
		}
		break;

	case STATUS_ABNORMAL_TEMP:
		if (chip->bat_temp <= chip->pdata->high_recover_temp &&
			chip->bat_temp >=
				chip->pdata->low_recover_temp) {
			chip->charger_status = STATUS_CHARGABLE;
			if (chip->pdata->restore_charger_mode)
				chip->pdata->
					restore_charger_mode();
			else
				chip->pdata->allow_charging(1);
		}
		break;

	case STATUS_CHARGE_TIMEOVER:
		if (chip->vcell <= chip->pdata->fully_charged_vol) {
			chip->charger_status = STATUS_CHARGABLE;
			if (chip->pdata->restore_charger_mode)
				chip->pdata->
					restore_charger_mode();
			else
				chip->pdata->allow_charging(1);
		}
		break;

	default:
		dev_err(&chip->client->dev, "%s : invalid status [%d]\n",
			__func__, chip->charger_status);
	}

	if (!chip->chg_limit_time &&
			chip->charger_status == STATUS_CHARGABLE) {
		chip->chg_limit_time =
			chip->is_timer_flag ?
			cur_time.tv_sec + chip->pdata->limit_recharging_time :
			cur_time.tv_sec + chip->pdata->limit_charging_time;
	}

#ifdef CONFIG_ADJUST_EVENT_CHARGING_CURRENT
	if (chip->event_list & EVT_CASE) {
		if (!chip->event_mode)
			chip->event_mode = true;
	} else
		chip->event_mode = false;

#ifdef CONFIG_TEMP_BLOCK_FOR_CAM_RECORDING
	if (chip->event_list & OFFSET_CAMERA_RECORD) {
		if (chip->pdata->check_temp_block_state) {
			if (!chip->event_mode &&
			    chip->pdata->check_temp_block_state(chip->bat_temp))
				chip->event_mode = true;
		}
	}
#endif
	if (chip->charger_status == STATUS_CHARGABLE &&
	    chip->pdata->get_charging_source &&
	    chip->pdata->get_charging_source() == PDA_POWER_CHARGE_AC) {
		if (chip->event_mode) {
			if (!chip->current_adjusted) {
				chip->pdata->adjust_charger_mode(
					PDA_POWER_CHARGE_USB);
				chip->current_adjusted = true;
			}
		} else {
			if (chip->current_adjusted) {
				if (chip->pdata->restore_charger_mode)
					chip->pdata->restore_charger_mode();
				chip->current_adjusted = false;
			}
		}
	} else {
		if (chip->current_adjusted)
			chip->current_adjusted = false;
	}
#endif

	dev_dbg(&chip->client->dev, "%s, Charger Status : %d, Limit Time : %ld\n",
			__func__, chip->charger_status, chip->chg_limit_time);
}

static void max17040_check_lowbat_condition(struct max17040_chip *chip)
{

	if (!chip->lowbat_state &&
	   chip->soc <= chip->pdata->min_capacity &&
	   chip->status != POWER_SUPPLY_STATUS_CHARGING) {
		chip->lowbat_state = true;
		wake_lock(&chip->lowbat_wake_lock);
		dev_info(&chip->client->dev,
			 "low battery wakelock : lock(%d) "
			 "(soc:%d%%, status:%d)\n",
			 chip->lowbat_state,
			 chip->soc, chip->status);
	} else if (chip->lowbat_state &&
		  (chip->soc > chip->pdata->min_capacity ||
		   chip->status == POWER_SUPPLY_STATUS_CHARGING)) {
		chip->lowbat_state = false;
		wake_unlock(&chip->lowbat_wake_lock);
		dev_info(&chip->client->dev,
			 "low battery wakelock : unlock(%d)"
			 "(soc:%d%%, status:%d)\n",
			 chip->lowbat_state,
			 chip->soc, chip->status);
	}

	if (chip->lowbat_state) {
		dev_info(&chip->client->dev,
			 "low battery wakelock : %d, flag : %d, "
			 "(soc:%d%%, status:%d)\n",
			 wake_lock_active(&chip->lowbat_wake_lock),
			 chip->lowbat_state,
			 chip->soc, chip->status);
	}
}

static void max17040_rewrite_rcomp(struct max17040_chip *chip)
{
	u16 temp;

	chip->lowbat_irq = false;
	max17040_read_reg(chip->client, MAX17040_RCOMP_MSB, &temp);
	dev_info(&chip->client->dev,
		 "before re-write RCOMP : 0x%x\n", temp);

	temp &= 0x00e0;
	temp |= (chip->rcomp_val << 8);
	temp |= chip->athd_val;

	max17040_write_reg(chip->client, MAX17040_RCOMP_MSB, temp);
	dev_info(&chip->client->dev,
		 "after re-write RCOMP : 0x%x\n", temp);
}

static void max17040_update(struct max17040_chip *chip)
{
	int prev_status = chip->status;
	int prev_soc = chip->soc;
	int prev_temp = chip->bat_temp;

	if (chip->lowbat_irq)
		max17040_rewrite_rcomp(chip);

	max17040_get_vcell(chip->client);
	max17040_get_soc(chip->client);
	max17040_get_online(chip->client);
	max17040_get_temp_status(chip);

	if (chip->pdata->charger_online()) {
		max17040_get_vf_status(chip);
		max17040_charger_update(chip);
	} else {
		chip->is_timer_flag = false;
#ifdef CONFIG_USE_CHG_ADC_POLLING
		if (chip->pdata->is_full_charge)
			chip->pdata->is_full_charge(false);
#endif
#ifdef CONFIG_ADJUST_EVENT_CHARGING_CURRENT
		if (chip->current_adjusted)
			chip->current_adjusted = false;
#endif
	}

	max17040_get_status(chip->client);
	max17040_check_lowbat_condition(chip);

	if ((chip->soc != prev_soc) || (chip->status != prev_status) ||
	    (chip->bat_temp != prev_temp))
		power_supply_changed(&chip->battery);

#ifdef CONFIG_USE_CHG_ADC_POLLING
	if (chip->status != prev_status) {
		if (chip->status == POWER_SUPPLY_STATUS_FULL)
			wake_lock(&full_batt_wake_lock);
		else {
			if (wake_lock_active(&full_batt_wake_lock))
				wake_unlock(&full_batt_wake_lock);
		}
	}
#endif

	dev_info(&chip->client->dev, "online = %d vcell = %d soc = %d "
		"status = %d health = %d temp = %d "
		"charger status = %d\n", chip->online, chip->vcell,
		chip->soc, chip->status, chip->bat_health, chip->bat_temp,
		chip->charger_status);
}

static void max17040_program_alarm(struct max17040_chip *chip, int seconds)
{
	ktime_t low_interval = ktime_set(seconds - 10, 0);
	ktime_t slack = ktime_set(20, 0);
	ktime_t next;

	next = ktime_add(chip->last_poll, low_interval);
	alarm_start_range(&chip->alarm, next, ktime_add(next, slack));
}

static void max17040_work(struct work_struct *work)
{
	unsigned long flags;
	struct timespec ts;
	struct max17040_chip *chip;

	chip = container_of(work, struct max17040_chip, work);

	max17040_update(chip);

	chip->last_poll = alarm_get_elapsed_realtime();
	ts = ktime_to_timespec(chip->last_poll);

	local_irq_save(flags);
	wake_unlock(&chip->work_wake_lock);
	if (!chip->shutdown)
		max17040_program_alarm(chip, FAST_POLL);
	local_irq_restore(flags);
}

static void max17040_battery_alarm(struct alarm *alarm)
{
	struct max17040_chip *chip =
		container_of(alarm, struct max17040_chip, alarm);

	wake_lock(&chip->work_wake_lock);
	schedule_work(&chip->work);

}

static void max17040_ext_power_changed(struct power_supply *psy)
{
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, battery);

	wake_lock(&chip->work_wake_lock);
	schedule_work(&chip->work);
}

static enum power_supply_property max17040_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	/* must be last */
	POWER_SUPPLY_PROP_TEMP,
};

static int max17040_pm_notifier(struct notifier_block *notifier,
		unsigned long pm_event,
		void *unused)
{
	struct max17040_chip *chip =
		container_of(notifier, struct max17040_chip, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
#ifdef CONFIG_USE_CHG_ADC_POLLING
		if (chip->pdata->get_charging_state) {
			if (!chip->pdata->get_charging_state()) {
				cancel_work_sync(&chip->work);
				max17040_program_alarm(chip, SLOW_POLL);
			}
		}
#else
		/* After charge completion interrupt, if there is no other
		 * wakeup, the battery charging will not be re-enabled even
		 * when TA stays connected. So, wakeup after sometime so that
		 * charging can be re-enable if required.
		 */
		cancel_work_sync(&chip->work);
		if (chip->charger_status == STATUS_CHARGE_FULL)
			max17040_program_alarm(chip, SLOW_POLL);
		else
			alarm_cancel(&chip->alarm);
#endif
		break;

	case PM_POST_SUSPEND:
		/* We might be on a slow sample cycle.  If we're
		 * resuming we should resample the battery state
		 * if it's been over a minute since we last did
		 * so, and move back to sampling every minute until
		 * we suspend again.
		 */
		max17040_program_alarm(chip, 1);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block max17040_pm_notifier_block = {
	.notifier_call = max17040_pm_notifier,
};

static irqreturn_t max17040_alert(int irq, void *data)
{
	struct max17040_chip *chip = data;
	struct i2c_client *client = chip->client;

	dev_info(&client->dev, "Low battery alert...\n");

	wake_lock(&chip->work_wake_lock);
	chip->lowbat_irq = true;
	schedule_work(&chip->work);

	return IRQ_HANDLED;
}

#ifndef CONFIG_USE_CHG_ADC_POLLING
static irqreturn_t full_charge_alert(int irq, void *data)
{
	struct max17040_chip *chip = data;

	/* Provide enough time for the application to display the status */
	wake_lock_timeout(&full_batt_wake_lock, 10*HZ);

	/* It is observed that flase interupt occurs when usb is connected and
	 * disconnected. Also the charger_online() returns true. This resulted
	 * in false full-battery condition. Hence, checking for the full battery
	 * condition after some delay.
	 */
	schedule_delayed_work(&chip->full_batt_work, msecs_to_jiffies(200));

	return IRQ_HANDLED;
}

void full_charge_workhandler(struct work_struct *work)
{
	struct max17040_chip *chip = container_of(work,
			struct max17040_chip, full_batt_work.work);
	struct i2c_client *client = chip->client;
	/*
	 * Indication for full battery charge
	 * Charger should be online and Charger should not be charging
	 */
	max17040_get_soc(chip->client);
	max17040_get_vcell(chip->client);
	if (chip->pdata->charger_online() && !chip->pdata->charger_enable()
			&& chip->vcell > MAX17040_VCELL_FULL
			&& chip->soc == MAX17040_BATTERY_FULL) {
		dev_info(&client->dev, "Full battery alert...!!!!\n");
		chip->is_full_charged = true;
		power_supply_changed(&chip->battery);
	} else {
		wake_unlock(&full_batt_wake_lock);
	}
}
#endif

#ifdef CONFIG_TEMP_BLOCK_FOR_CAM_RECORDING
static int proc_read_cam_recording(char *page, char **start,
		off_t off, int count, int *eof, void *data)
{
	struct max17040_chip *chip = container_of(data,
		  struct max17040_chip, cam_recording_value);
	int len;

	len = sprintf(page, "%d\n",
		      (chip->event_list &
		       ~(0x1 << OFFSET_CAMERA_RECORD))
		      >> OFFSET_CAMERA_RECORD);

	return len;
}

static int proc_write_cam_recording(struct file *filp,
	       const char __user *buffer, unsigned long count, void *data)
{
	struct max17040_chip *chip = container_of(data,
			  struct max17040_chip, cam_recording_value);
	struct i2c_client *client = chip->client;

	int len = count;
	unsigned long value;

	if (copy_from_user(chip->cam_recording_value, buffer, len))
		return -EFAULT;


	chip->cam_recording_value[1] = '\0';

	if (kstrtoul(chip->cam_recording_value, 10, &value))
			return -EFAULT;

	if (value)
		chip->event_list |= OFFSET_CAMERA_RECORD;
	else
		chip->event_list &= ~OFFSET_CAMERA_RECORD;

	schedule_work(&chip->work);

	dev_info(&client->dev, "Camera recording state : %d, 0x%x\n",
		 (int)value, chip->event_list);

	return len;
}

static int max17040_proc_init(struct max17040_chip *chip)
{
	int ret = 1;

	chip->max17040_battery_proc = NULL;
	chip->cam_recording_proc = NULL;

	chip->max17040_battery_proc = proc_mkdir("battery", NULL);
	if (chip->max17040_battery_proc == NULL)
		goto out;

	chip->cam_recording_proc =
		create_proc_entry("camera_recording",
				  0666,
				  chip->max17040_battery_proc);

	if (chip->cam_recording_proc == NULL)
		goto out;

	chip->cam_recording_proc->data = chip->cam_recording_value;
	chip->cam_recording_proc->read_proc = proc_read_cam_recording;
	chip->cam_recording_proc->write_proc = proc_write_cam_recording;

	return 0;
out:
	return ret;
}

static void max17040_proc_remove(struct max17040_chip *chip)
{
	remove_proc_entry("camera_recording",
			  chip->max17040_battery_proc);
	remove_proc_entry("battery", NULL);
}
#endif

static void bat_work(struct work_struct *work)
{
	struct max17040_chip *chip = container_of(work,
			struct max17040_chip, bat_removal_work);

	if (chip->pdata->battery_online) {
		if (!chip->pdata->battery_online())
			chip->pdata->allow_charging(0);
	}
}

static irqreturn_t bat_removal_isr(int irq, void *data)
{
	struct max17040_chip *chip = data;
	disable_irq_nosync(chip->bat_removal_irq);
	schedule_work(&chip->bat_removal_work);
	return IRQ_HANDLED;
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
};

static ssize_t max17040_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t max17040_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define SEC_BATTERY_ATTR(_name)                     \
{                                   \
	.attr = {.name = #_name, \
			 .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,\
			},\
	.show = max17040_show_attrs,             \
	.store = max17040_store_attrs,             \
}

static struct device_attribute max17040_attrs[] = {
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

static ssize_t max17040_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct power_supply *psy = dev_get_drvdata(dev);

	struct max17040_chip *chip
		= container_of(psy, struct max17040_chip, battery);

	const ptrdiff_t off = attr - max17040_attrs;
	int ret;

	switch (off) {
	case BATT_VOL:
		max17040_get_vcell(chip->client);
		ret = sprintf(buf, "%d\n", chip->vcell);
		break;

	case BATT_CHARGING_SOURCE:
		if (unlikely(!chip->pdata->get_charging_source))
			return -EINVAL;

		ret = sprintf(buf, "%d\n",
				chip->pdata->get_charging_source());
		break;

	case BATT_FG_SOC:
		max17040_get_soc(chip->client);
		ret = sprintf(buf, "%d\n", chip->soc);
		break;

	case BATT_TEMP:
		max17040_get_temp_status(chip);
		ret = sprintf(buf, "%d\n", chip->bat_temp);
		break;

	case BATT_TEMP_ADC:
		max17040_get_temp_status(chip);
		ret = sprintf(buf, "%d\n", chip->bat_temp_adc);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t max17040_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	struct power_supply *psy = dev_get_drvdata(dev);

	struct max17040_chip *chip
		= container_of(psy, struct max17040_chip, battery);

	const ptrdiff_t off = attr - max17040_attrs;

	switch (off) {
	case BATT_RESET_SOC:
		max17040_reset(chip->client);
		ret = 1;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int max17040_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(max17040_attrs); i++) {
		rc = device_create_file(dev, &max17040_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto succeed;
create_attrs_failed:
	while (i--)
		device_remove_file(dev, &max17040_attrs[i]);
succeed:
	return rc;
}


static int __devinit max17040_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17040_chip *chip;
	int ret;
#ifndef CONFIG_USE_CHG_ADC_POLLING
	int irq;
#endif
	u16 val;
	u16 athd;

	int num_props = ARRAY_SIZE(max17040_battery_props);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	if (!chip->pdata->get_bat_temp)
		num_props--;

	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17040_get_property;
	chip->battery.set_property	= max17040_set_property;
	chip->battery.properties	= max17040_battery_props;
	chip->battery.num_properties	= num_props;
	chip->battery.external_power_changed	= max17040_ext_power_changed;

	chip->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	chip->charger_status = STATUS_CHARGABLE;
	chip->is_timer_flag = false;
	chip->chg_limit_time = 0;
	chip->is_full_charged = false;
	chip->is_full_charged_pre = false;

	if (!chip->pdata->high_block_temp)
		chip->pdata->high_block_temp = 500;
	if (!chip->pdata->high_recover_temp)
		chip->pdata->high_recover_temp = 420;
	if (!chip->pdata->low_block_temp)
		chip->pdata->low_block_temp = -50;
	if (!chip->pdata->fully_charged_vol)
		chip->pdata->fully_charged_vol = 4150000;
	if (!chip->pdata->recharge_vol)
		chip->pdata->recharge_vol = 4140000;
	if (!chip->pdata->limit_charging_time)
		chip->pdata->limit_charging_time = 21600;
	if (!chip->pdata->limit_recharging_time)
		chip->pdata->limit_recharging_time = 5400;

	chip->last_poll = alarm_get_elapsed_realtime();
	alarm_init(&chip->alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
			max17040_battery_alarm);

	wake_lock_init(&chip->work_wake_lock, WAKE_LOCK_SUSPEND,
			"max17040-battery");

	wake_lock_init(&chip->lowbat_wake_lock, WAKE_LOCK_SUSPEND,
		       "lowbat_wake_lock");

	wake_lock_init(&full_batt_wake_lock, WAKE_LOCK_SUSPEND,
			"full-batt-alert");

	if (!chip->pdata->skip_reset)
		max17040_reset(client);

	max17040_get_version(client);
	INIT_WORK(&chip->work, max17040_work);
#ifndef CONFIG_USE_CHG_ADC_POLLING
	INIT_DELAYED_WORK(&chip->full_batt_work, full_charge_workhandler);
#endif
	INIT_WORK(&chip->bat_removal_work, bat_work);
	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto err_battery_supply_register;
	}

	ret = max17040_create_attrs(chip->battery.dev);
	if (ret)
		pr_err("%s : Failed to create_attrs\n", __func__);

	/* i2c-core does not support dev_pm_ops.prepare and .complete
	 * So, used pm_notifier for use android_alarm.
	 */
	chip->pm_notifier = max17040_pm_notifier_block;
	ret = register_pm_notifier(&chip->pm_notifier);
	if (ret) {
		dev_err(&client->dev, "failed: register pm notifier\n");
		goto err_pm_notifier;
	}
	schedule_work(&chip->work);

#ifndef CONFIG_USE_CHG_ADC_POLLING
	/* Bootloader can enable the battery charging. Consider a scenario
	 * where fully charged battery is inserted into the device and it
	 * booted in LPM mode by connecting TA/USB. Charging will be enabled
	 * by the bootloader. However, before the kernel boots, charger IC may
	 * disable charging as the battery is completely charged. In such
	 * situation CHG_ING_N line will be HIGH before initializing this
	 * driver. Since CHG_ING_N is already high, we never get the charging
	 * completion interrupt.
	 */
	if (chip->pdata->charger_online() && !chip->pdata->charger_enable()) {
		dev_info(&client->dev, "Full battery alert...!!!!\n");
		chip->is_full_charged = true;
		power_supply_changed(&chip->battery);
	}
#endif

	if (HAS_ALERT_INTERRUPT(chip->ver) && chip->pdata->use_fuel_alert) {

		/* setting the low SOC alert threshold */
		if (!max17040_read_reg(client, MAX17040_RCOMP_MSB, &val)) {
			athd = chip->pdata->min_capacity;
			max17040_write_reg(client, MAX17040_RCOMP_MSB,
					   (val & ~0x1f) | (-athd & 0x1f));
		} else {
			dev_err(&client->dev,
				"Error setting battery alert threshold\n");
		}

		/* add alert irq handler */
		ret = request_threaded_irq(client->irq, NULL, max17040_alert,
				IRQF_TRIGGER_FALLING, "fuel gauge alert", chip);
		if (ret < 0) {
			dev_err(&client->dev,
				"request_threaded_irq() failed: %d", ret);
			goto err_pm_notifier;
		}
	}

	max17040_read_reg(client, MAX17040_RCOMP_MSB, &val);
	chip->rcomp_val = (val >> 8);
	chip->athd_val = (val & 0x1f);

	dev_info(&client->dev, "RCOMP : 0x%x, ATHD : 0x%x\n",
		 chip->rcomp_val, chip->athd_val);

	if (chip->pdata->bat_removal_irq) {
		chip->bat_removal_irq = chip->pdata->bat_removal_irq();
		ret = request_threaded_irq(chip->bat_removal_irq, NULL,
			 bat_removal_isr,
			IRQF_DISABLED | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND
			| IRQF_TRIGGER_RISING,
			"battery_removed", chip);
	}

#ifndef CONFIG_USE_CHG_ADC_POLLING
	/*Registering IRQ for full battery charge indication*/
	if (chip->pdata->full_charge_irq) {
		irq = chip->pdata->full_charge_irq();
		ret = request_threaded_irq(irq, NULL, full_charge_alert,
				IRQF_NO_SUSPEND | IRQF_ONESHOT |
				IRQF_TRIGGER_RISING,
				"full_charge_alert", chip);
		if (ret < 0) {
			dev_err(&client->dev,
				"request irq failed for full_charge: %d", ret);
			goto err_pm_notifier;
		}
		enable_irq_wake(irq);
	}
#endif

#ifdef CONFIG_TEMP_BLOCK_FOR_CAM_RECORDING
	ret = max17040_proc_init(chip);
	if (ret)
		dev_err(&client->dev,
			"failed to make proc node");
#endif

	if (chip->pdata->set_charger_start_state)
		chip->pdata->set_charger_start_state();

	return 0;

err_pm_notifier:
	power_supply_unregister(&chip->battery);
err_battery_supply_register:
	wake_lock_destroy(&full_batt_wake_lock);
	wake_lock_destroy(&chip->lowbat_wake_lock);
	wake_lock_destroy(&chip->work_wake_lock);
	alarm_cancel(&chip->alarm);
	kfree(chip);

	return ret;
}

static int __devexit max17040_remove(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	chip->shutdown = 1;
	unregister_pm_notifier(&chip->pm_notifier);
	power_supply_unregister(&chip->battery);
	alarm_cancel(&chip->alarm);
	cancel_work_sync(&chip->work);
	wake_lock_destroy(&full_batt_wake_lock);
	wake_lock_destroy(&chip->lowbat_wake_lock);
	wake_lock_destroy(&chip->work_wake_lock);
	if (HAS_ALERT_INTERRUPT(chip->ver) && chip->pdata->use_fuel_alert)
		free_irq(client->irq, chip);
	if (chip->pdata->full_charge_irq)
		free_irq(chip->pdata->full_charge_irq(), chip);
#ifdef CONFIG_TEMP_BLOCK_FOR_CAM_RECORDING
	max17040_proc_remove(chip);
#endif
	kfree(chip);
	return 0;
}

static const struct i2c_device_id max17040_id[] = {
	{ "max17040", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17040_id);

static struct i2c_driver max17040_i2c_driver = {
	.driver	= {
		.name	= "max17040",
	},
	.probe		= max17040_probe,
	.remove		= __devexit_p(max17040_remove),
	.id_table	= max17040_id,
};

static int __init max17040_init(void)
{
	return i2c_add_driver(&max17040_i2c_driver);
}
module_init(max17040_init);

static void __exit max17040_exit(void)
{
	i2c_del_driver(&max17040_i2c_driver);
}
module_exit(max17040_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("MAX17040 Fuel Gauge");
MODULE_LICENSE("GPL");

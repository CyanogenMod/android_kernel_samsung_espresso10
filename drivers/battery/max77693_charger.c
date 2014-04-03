/*
 *  max77693_charger.c
 *  Samsung max77693 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#define ENABLE 1
#define DISABLE 0

#define MAX77693_TERMINATION_CURRENT_MASK 0x7

static struct dentry	*max77693_dentry;

static void max77693_register_lock(struct sec_charger_info *charger, int enable)
{
	u8 reg_data = 0;

	/* unlock charger setting protect */
	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_06, &reg_data);

	if (enable)
		reg_data &= ~0x0c;
	else
		reg_data |= 0x0c;

	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_06, reg_data);

}

static int max77693_get_battery_present(struct sec_charger_info *charger)
{
	u8 reg_data;

	max77693_read_reg(charger->client,
			MAX77693_CHG_REG_CHG_INT_OK, &reg_data);
	pr_debug("%s: CHG_INT_OK(0x%02x)\n", __func__, reg_data);

	reg_data = ((reg_data & MAX77693_DETBAT) >> MAX77693_DETBAT_SHIFT);

	return !reg_data;
}

static void max77693_set_charger_state(struct sec_charger_info *charger,
		int enable)
{
	u8 reg_data;

	max77693_read_reg(charger->client,
			MAX77693_CHG_REG_CHG_CNFG_00, &reg_data);

	if (enable)
		reg_data |= MAX77693_MODE_CHGR;
	else
		reg_data &= ~MAX77693_MODE_CHGR;

	max77693_register_lock(charger, 0);

	pr_debug("%s: CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
	max77693_write_reg(charger->client,
			MAX77693_CHG_REG_CHG_CNFG_00, reg_data);

	max77693_register_lock(charger, 1);
}

static void max77693_set_buck(struct sec_charger_info *charger,
		int enable)
{
	u8 reg_data;

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_00, &reg_data);

	if (enable)
		reg_data |= MAX77693_MODE_BUCK;
	else
		reg_data &= ~MAX77693_MODE_BUCK;

	pr_debug("%s: CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_00, reg_data);

}

static void max77693_set_termination_current(struct sec_charger_info *charger,
		int term_current)
{
	u8 reg_data;
	u8 value;

	value = term_current <= 200 ? (term_current - 100) / 25 :
		term_current / 50;

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_03, &reg_data);

	reg_data &= ~MAX77693_TERMINATION_CURRENT_MASK;
	reg_data |= value;

	pr_debug("%s: CHG_CNFG_03(0x%02x)\n", __func__, reg_data);
	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_03, reg_data);
}

static void max77693_set_input_current(struct sec_charger_info *charger,
		int cur)
{
	int set_current_reg, now_current_reg;
	int step;

	max77693_register_lock(charger, 0);

	if (!cur) {
		max77693_write_reg(charger->client,
			MAX77693_CHG_REG_CHG_CNFG_09, 0);
		max77693_set_buck(charger, DISABLE);
		return;
	} else
		max77693_set_buck(charger, ENABLE);

	set_current_reg = cur / 20;

	msleep(SOFT_CHG_STEP_DUR);

	step = 0;

	do {
		now_current_reg = ((SOFT_CHG_START_CURR +
					(SOFT_CHG_CURR_STEP * (++step))) / 20);
		now_current_reg = min(now_current_reg, set_current_reg);
		max77693_write_reg(charger->client,
			MAX77693_CHG_REG_CHG_CNFG_09, now_current_reg);
		msleep(SOFT_CHG_STEP_DUR);
	} while (now_current_reg < set_current_reg);

	pr_info("%s: reg_data(0x%02x)\n", __func__, set_current_reg);

	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_09, set_current_reg);

	max77693_register_lock(charger, 1);

}

static int max77693_get_input_current(struct sec_charger_info *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_09, &reg_data);
	pr_info("%s: CHG_CNFG_09(0x%02x)\n", __func__, reg_data);

	get_current = reg_data * 20;

	pr_debug("%s: get input current: %dmA\n", __func__, get_current);
	return get_current;
}

static void max77693_set_charge_current(struct sec_charger_info *charger,
		int cur)
{
	u8 reg_data;

	max77693_register_lock(charger, 0);

	if (!cur) {
		/* No charger */
		reg_data |= MAX77693_OTG_ILIM;
		max77693_write_reg(charger->client,
			MAX77693_CHG_REG_CHG_CNFG_02, reg_data);
	} else {
		reg_data &= ~MAX77693_CHG_CC;
		reg_data |= ((cur * 3 / 100) << 0);
		reg_data |= MAX77693_OTG_ILIM;

		pr_info("%s: charge current %d mA, reg_data(0x%02x)\n",
						__func__, cur, reg_data);

		max77693_write_reg(charger->client,
			MAX77693_CHG_REG_CHG_CNFG_02, reg_data);
	}

	max77693_register_lock(charger, 1);

}

static int max77693_get_charge_current(struct sec_charger_info *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_02, &reg_data);
	pr_debug("%s: CHG_CNFG_02(0x%02x)\n", __func__, reg_data);

	reg_data &= MAX77693_CHG_CC;
	get_current = charger->charging_current = reg_data * 333 / 10;

	pr_debug("%s: get charge current: %dmA\n", __func__, get_current);

	return get_current;
}

static int max77693_get_vbus_state(struct sec_charger_info *charger)
{
	u8 reg_data;

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_DTLS_00, &reg_data);
	reg_data = ((reg_data & MAX77693_CHGIN_DTLS) >>
			MAX77693_CHGIN_DTLS_SHIFT);

	switch (reg_data) {
	case 0x00:
		pr_info("%s: VBUS is invalid. CHGIN < CHGIN_UVLO\n",
			__func__);
		break;
	case 0x01:
		pr_info("%s: VBUS is invalid. CHGIN < MBAT+CHGIN2SYS"
			"and CHGIN > CHGIN_UVLO\n", __func__);
		break;
	case 0x02:
		pr_info("%s: VBUS is invalid. CHGIN > CHGIN_OVLO",
			__func__);
		break;
	case 0x03:
		pr_info("%s: VBUS is valid. CHGIN < CHGIN_OVLO", __func__);
		break;
	default:
		break;
	}

	return reg_data;
}

static int max77693_get_charger_state(struct sec_charger_info *charger)
{
	int state;
	u8 reg_data;

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_DTLS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_CHG_DTLS) >> MAX77693_CHG_DTLS_SHIFT);
	pr_info("%s: CHG_DTLS : 0x%2x\n", __func__, reg_data);

	switch (reg_data) {
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
		state = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x4:
		state = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x5:
	case 0x6:
	case 0x7:
		state = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x8:
	case 0xA:
	case 0xB:
		state = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		state = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return state;
}

static int max77693_get_health_state(struct sec_charger_info *charger)
{
	int state;
	int vbus_state;
	int chg_state;
	u8 reg_data;

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_DTLS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_BAT_DTLS) >> MAX77693_BAT_DTLS_SHIFT);
	pr_info("%s: BAT_DTLS : %d", __func__, reg_data);
	switch (reg_data) {
	case 0x00:
		pr_info("%s: No battery and the charger is suspended\n",
			__func__);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
	case 0x01:
		state = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	case 0x02:
		pr_info("%s: battery dead\n", __func__);
		state = POWER_SUPPLY_HEALTH_DEAD;
		break;
	case 0x03:
		pr_info("%s: battery good\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x04:
		pr_info("%s: battery is okay "
			"but its voltage is low\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x05:
		pr_info("%s: battery ovp\n", __func__);
		state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;
	default:
		pr_info("%s: battery unknown : 0x%d\n", __func__, reg_data);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	if (state == POWER_SUPPLY_HEALTH_GOOD) {
		/* VBUS OVP state return battery OVP state */
		vbus_state = max77693_get_vbus_state(charger);
		/* read CHG_DTLS and detecting battery terminal error */
		chg_state = max77693_get_charger_state(charger);
		/* OVP is higher priority */
		if (vbus_state == 0x02) { /* CHGIN_OVLO */
			pr_info("%s: vbus ovp\n", __func__);
			state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		} else if (reg_data == 0x04 &&
				chg_state == POWER_SUPPLY_STATUS_FULL) {
			pr_info("%s: battery terminal error\n", __func__);
			state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		}
	}

	return state;
}

static int sec_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sec_charger_info *charger =
		container_of(psy, struct sec_charger_info, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77693_get_charger_state(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77693_get_health_state(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = min(max77693_get_input_current(charger),
			max77693_get_charge_current(charger));
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77693_get_battery_present(charger);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sec_charger_info *charger =
		container_of(psy, struct sec_charger_info, psy_chg);

	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY)
			charger->is_charging = false;
		else
			charger->is_charging = true;

		/* current setting */
		charger->charging_current =
			charger->pdata->charging_current[
			val->intval].fast_charging_current;
		max77693_set_charge_current(charger,
				charger->charging_current);
		max77693_set_input_current(charger, charger->charging_current);
		max77693_set_charger_state(charger, charger->is_charging);
		max77693_set_termination_current(charger,
			charger->pdata->charging_current[
				val->intval].full_check_current_1st);
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->charging_current = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void max77693_charger_initialize(struct sec_charger_info *charger)
{
	u8 reg_data;
	pr_debug("%s\n", __func__);

	/* unlock charger setting protect */
	max77693_register_lock(charger, 0);
	/*
	 * fast charge timer 10hrs
	 * restart threshold disable
	 * pre-qual charge enable
	 */

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_01, &reg_data);

	reg_data = 0xb0;

	pr_info("%s: CHG_REG_CHG_CNFG_01 : 0x%02x\n", __func__, reg_data);

	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_01, reg_data);

	/*
	 * charge current 466mA(default)
	 * otg current limit 900mA
	 */

	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_02, &reg_data);

	reg_data |= MAX77693_OTG_ILIM;

	pr_info("%s: CHG_REG_CHG_CNFG_02 : 0x%02x\n", __func__, reg_data);

	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_02, reg_data);

	/*
	 * top off current 100mA
	 * top off timer 0min
	 */
	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_03, &reg_data);

	reg_data = 0x18;

	pr_info("%s: CHG_REG_CHG_CNFG_03 : 0x%02x\n", __func__, reg_data);

	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_03, reg_data);

	/*
	 * cv voltage 4.2V or 4.35V
	 * MINVSYS 3.6V(default)
	 */
	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_04, &reg_data);

	reg_data  = (reg_data & 0x80) | (0xDD << 0);

	pr_info("%s: battery cv voltage %s, (sysrev %d)\n", __func__,
		(((reg_data & MAX77693_CHG_PRM_MASK) == \
		(0x1D << MAX77693_CHG_PRM_SHIFT)) ? "4.35V" : "4.2V"),
		system_rev);

	pr_info("%s: CHG_REG_CHG_CNFG_04 : 0x%02x\n", __func__, reg_data);

	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_04, reg_data);

	/* VBYPSET 5V */
	max77693_read_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_11, &reg_data);

	reg_data = (reg_data & 0x7f) | 0x50;
	max77693_write_reg(charger->client,
		MAX77693_CHG_REG_CHG_CNFG_11, reg_data);

	pr_info("%s: CHG_REG_CHG_CNFG_11 : 0x%02x\n", __func__, reg_data);

	max77693_register_lock(charger, 1);
}

static void sec_chg_isr_work(struct work_struct *work)
{
	struct sec_charger_info *charger =
		container_of(work, struct sec_charger_info, isr_work.work);

	union power_supply_propval val;

	if (charger->pdata->full_check_type ==
			SEC_BATTERY_FULLCHARGED_CHGINT) {

		val.intval = max77693_get_charger_state(charger);

		switch (val.intval) {
		case POWER_SUPPLY_STATUS_DISCHARGING:
			pr_err("%s: Interrupted but Discharging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_NOT_CHARGING:
			pr_err("%s: Interrupted but NOT Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_FULL:
			pr_info("%s: Interrupted by Full\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_STATUS, val);
			break;

		case POWER_SUPPLY_STATUS_CHARGING:
			pr_err("%s: Interrupted but Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Status\n", __func__);
			break;
		}
	}

	if (charger->pdata->ovp_uvlo_check_type ==
			SEC_BATTERY_OVP_UVLO_CHGINT) {

		val.intval = max77693_get_health_state(charger);

		switch (val.intval) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_COLD:
			pr_err("%s: Interrupted but Hot/Cold\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_DEAD:
			pr_err("%s: Interrupted but Dead\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, val);
			break;

		case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
			pr_err("%s: Interrupted but Unspec\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_GOOD:
			pr_err("%s: Interrupted but Good\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Health\n", __func__);
			break;
		}
	}
}

static int max77693_debugfs_show(struct seq_file *s, void *data)
{
	struct sec_charger_info *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "MAX77693 CHARGER IC :\n");
	seq_printf(s, "==================\n");
	for (reg = 0xB0; reg <= 0xC5; reg++) {
		max77693_read_reg(charger->client, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");

	return 0;
}

static int max77693_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77693_debugfs_show, inode->i_private);
}

static const struct file_operations max77693_debugfs_fops = {
	.open           = max77693_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static irqreturn_t sec_chg_irq_thread(int irq, void *irq_data)
{
	struct sec_charger_info *charger = irq_data;

	pr_info("%s: Charger interrupt occured\n", __func__);

	if ((charger->pdata->full_check_type ==
				SEC_BATTERY_FULLCHARGED_CHGINT) ||
			(charger->pdata->ovp_uvlo_check_type ==
			 SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

static __devinit int max77693_charger_probe(struct platform_device *pdev)
{
	struct max77693_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77693_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct sec_charger_info *charger;
	int ret = 0;

	pr_info("%s: MAX77693 Charger driver probe\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->pdata = pdata->charger_data;
	charger->client = iodev->i2c;

	platform_set_drvdata(pdev, charger);

	charger->psy_chg.name           = "sec-charger";
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_BATTERY;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = sec_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);

	if (charger->pdata->chg_gpio_init) {
		if (!charger->pdata->chg_gpio_init()) {
			pr_err("%s: Failed to Initialize GPIO\n", __func__);
			goto err_free;
		}
	}

	max77693_charger_initialize(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_free;
	}

	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK_DEFERRABLE(
				&charger->isr_work, sec_chg_isr_work);

		ret = request_threaded_irq(charger->pdata->chg_irq,
				NULL, sec_chg_irq_thread,
				charger->pdata->chg_irq_attr,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
			goto err_irq;
		}
	}

	max77693_dentry = debugfs_create_file("max77693-regs",
			S_IRUSR, NULL, charger,	&max77693_debugfs_fops);

	return 0;

err_irq:
	power_supply_unregister(&charger->psy_chg);
err_free:
	kfree(charger);

	return ret;

}

static int __devexit max77693_charger_remove(struct platform_device *pdev)
{
	struct sec_charger_info *charger =
				platform_get_drvdata(pdev);

	if (!IS_ERR_OR_NULL(max77693_dentry))
		debugfs_remove(max77693_dentry);

	power_supply_unregister(&charger->psy_chg);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int max77693_charger_suspend(struct device *dev)
{
	return 0;
}

static int max77693_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define max77693_charger_suspend NULL
#define max77693_charger_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(max77693_charger_pm_ops, max77693_charger_suspend,
		max77693_charger_resume);

static struct platform_driver max77693_charger_driver = {
	.driver = {
		.name = "max77693-charger",
		.owner = THIS_MODULE,
		.pm = &max77693_charger_pm_ops,
	},
	.probe = max77693_charger_probe,
	.remove = __devexit_p(max77693_charger_remove),
};

static int __init max77693_charger_init(void)
{
	pr_info("func:%s\n", __func__);
	return platform_driver_register(&max77693_charger_driver);
}
module_init(max77693_charger_init);

static void __exit max77693_charger_exit(void)
{
	platform_driver_register(&max77693_charger_driver);
}

module_exit(max77693_charger_exit);

MODULE_DESCRIPTION("max77693 charger driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

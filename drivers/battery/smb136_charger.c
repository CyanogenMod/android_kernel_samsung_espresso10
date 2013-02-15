/*
 *  smb136_charger.c
 *  Samsung SMB136 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/battery/sec_charger.h>
static int smb136_i2c_write(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	return ret;
}

static int smb136_i2c_read(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	return ret;
}

static void smb136_i2c_write_array(struct i2c_client *client,
				u8 *buf, int size)
{
	int i;
	for (i = 0; i < size; i += 3)
		smb136_i2c_write(client, (u8) (*(buf + i)), (buf + i) + 1);
}

static void smb136_set_command(struct i2c_client *client,
				int reg, int datum)
{
	int val;
	u8 data = 0;
	val = smb136_i2c_read(client, reg, &data);
	if (val >= 0) {
		dev_dbg(&client->dev, "%s : reg(0x%02x): 0x%02x",
			__func__, reg, data);
		if (data != datum) {
			data = datum;
			if (smb136_i2c_write(client, reg, &data) < 0)
				dev_err(&client->dev,
					"%s : error!\n", __func__);
			val = smb136_i2c_read(client, reg, &data);
			if (val >= 0)
				dev_dbg(&client->dev, " => 0x%02x\n", data);
		}
	}
}

static void smb136_test_read(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;
	for (addr = 0; addr <= 0x0C; addr++) {
		smb136_i2c_read(client, addr, &data);
		dev_dbg(&client->dev,
			"smb136 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
	for (addr = 0x31; addr <= 0x3D; addr++) {
		smb136_i2c_read(client, addr, &data);
		dev_dbg(&client->dev,
			"smb136 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}

static void smb136_read_regs(struct i2c_client *client, char *str)
{
	u8 data = 0;
	u32 addr = 0;

	for (addr = 0; addr <= 0x0C; addr++) {
		smb136_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}

	/* "#" considered as new line in application */
	sprintf(str+strlen(str), "#");

	for (addr = 0x30; addr <= 0x3D; addr++) {
		smb136_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}
}


static int smb136_get_charging_status(struct i2c_client *client)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 data_d = 0;
	u8 data_e = 0;

	smb136_i2c_read(client, SMB136_STATUS_D, &data_d);
	dev_info(&client->dev,
		"%s : charger status D(0x%02x)\n", __func__, data_d);
	smb136_i2c_read(client, SMB136_STATUS_E, &data_e);
	dev_info(&client->dev,
		"%s : charger status E(0x%02x)\n", __func__, data_e);

	/* At least one charge cycle terminated,
	 * Charge current < Termination Current
	 */
	if ((data_e & 0x40) == 0x40) {
		/* top-off by full charging */
		status = POWER_SUPPLY_STATUS_FULL;
		goto charging_status_end;
	}

	/* Is enabled ? */
	if (data_e & 0x01) {
		/* check for 0x06 : no charging (0b00) */
		/* not charging */
		if (!(data_e & 0x06)) {
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			goto charging_status_end;
		} else {
			status = POWER_SUPPLY_STATUS_CHARGING;
			goto charging_status_end;
		}
	} else
		status = POWER_SUPPLY_STATUS_DISCHARGING;
charging_status_end:
	return (int)status;
}

static int smb136_get_charging_health(struct i2c_client *client)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 data_d = 0;
	u8 data_e = 0;

	smb136_i2c_read(client, SMB136_STATUS_D, &data_d);
	dev_info(&client->dev,
		"%s : charger status D(0x%02x)\n", __func__, data_d);
	smb136_i2c_read(client, SMB136_STATUS_E, &data_e);
	dev_info(&client->dev,
		"%s : charger status E(0x%02x)\n", __func__, data_e);

	/* Is enabled ? */
	if (data_e & 0x01) {
		if ((data_d & 0x40) != 0x40)	/* Input current is NOT OK */
			health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	}
	return (int)health;
}

static void smb136_allow_volatile_writes(struct i2c_client *client)
{
	int val, reg;
	u8 data;
	reg = SMB136_COMMAND_A;
	val = smb136_i2c_read(client, reg, &data);
	if ((val >= 0) && !(data & 0x80)) {
		dev_dbg(&client->dev,
			"%s : reg(0x%02x): 0x%02x", __func__, reg, data);
		data |= (0x1 << 7);
		if (smb136_i2c_write(client, reg, &data) < 0)
			dev_err(&client->dev, "%s : error!\n", __func__);
		val = smb136_i2c_read(client, reg, &data);
		if (val >= 0) {
			data = (u8) data;
			dev_dbg(&client->dev, " => 0x%02x\n", data);
		}
	}
}

static u8 smb136_get_float_voltage_data(
			int float_voltage)
{
	u8 data;

	if (float_voltage < 3460)
		float_voltage = 3460;

	data = (float_voltage - 3460) / 10;

	return data;
}

static u8 smb136_get_input_current_limit_data(
			struct sec_charger_info *charger, int input_current)
{
	u8 data;

	if (input_current <= 700)
		data = 0x0;
	else if (input_current <= 800)
		data = 0x20;
	else if (input_current <= 900)
		data = 0x40;
	else if (input_current <= 1000)
		data = 0x60;
	else if (input_current <= 1100)
		data = 0x80;
	else if (input_current <= 1200)
		data = 0xA0;
	else if (input_current <= 1300)
		data = 0xC0;
	else if (input_current <= 1400)
		data = 0xE0;
	else
		data = 0;

	return data;
}

static u8 smb136_get_termination_current_limit_data(
			int termination_current)
{
	u8 data;

	if (termination_current <= 35)
		data = 0x6;
	else if (termination_current <= 50)
		data = 0x0;
	else if (termination_current <= 100)
		data = 0x2;
	else if (termination_current <= 150)
		data = 0x4;
	else
		data = 0x6;

	return data;
}

static u8 smb136_get_fast_charging_current_data(
			int fast_charging_current)
{
	u8 data;

	if (fast_charging_current <= 500)
		data = 0x0;
	else if (fast_charging_current <= 650)
		data = 0x20;
	else if (fast_charging_current <= 750)
		data = 0x40;
	else if (fast_charging_current <= 850)
		data = 0x60;
	else if (fast_charging_current <= 950)
		data = 0x80;
	else if (fast_charging_current <= 1100)
		data = 0xA0;
	else if (fast_charging_current <= 1300)
		data = 0xC0;
	else if (fast_charging_current <= 1500)
		data = 0xE0;
	else
		data = 0;

	return data;
}

static void smb136_charger_function_conrol(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
	u8 cur_reg;
	u8 chg_cur_reg;

	if (charger->charging_current < 0) {
		dev_dbg(&client->dev,
			"%s : OTG is activated. Ignore command!\n", __func__);
		return;
	}
	smb136_allow_volatile_writes(client);

	if (charger->cable_type ==
		POWER_SUPPLY_TYPE_BATTERY) {
		/* turn off charger */
		smb136_set_command(client,
			SMB136_COMMAND_A, 0x80);
	} else {
		/* Pre-charge curr 250mA */
		dev_dbg(&client->dev,
			"%s : fast charging current (%dmA)\n",
			__func__, charger->charging_current);
		dev_dbg(&client->dev,
			"%s : termination current (%dmA)\n",
			__func__, charger->pdata->charging_current[
			charger->cable_type].full_check_current_1st);

		smb136_i2c_read(client, SMB136_CHARGE_CURRENT, &chg_cur_reg);
		chg_cur_reg &= 0x18;
		data = smb136_get_fast_charging_current_data(
			charger->charging_current) | chg_cur_reg;
		data |= smb136_get_termination_current_limit_data(
			charger->pdata->charging_current[
			charger->cable_type].full_check_current_1st);
		smb136_set_command(client,
			SMB136_CHARGE_CURRENT, data);

		/* Pin enable control */
		/* DCIN Input Pre-bias Enable */
		smb136_i2c_read(client, SMB136_PIN_ENABLE_CONTROL, &data);
		if (charger->pdata->chg_gpio_en)
			data |= 0x40;
		smb136_set_command(client,
			SMB136_PIN_ENABLE_CONTROL, data);

		/* Input current limit */
		dev_dbg(&client->dev, "%s : input current (%dmA)\n",
			__func__, charger->pdata->charging_current
			[charger->cable_type].input_current_limit);
		data = 0;
		data = smb136_get_input_current_limit_data(
			charger,
			charger->pdata->charging_current
			[charger->cable_type].input_current_limit);
		smb136_i2c_read(client, SMB136_INPUT_CURRENTLIMIT, &cur_reg);
		data |= (cur_reg & 0xF);
		smb136_set_command(client,
			SMB136_INPUT_CURRENTLIMIT, data);

		/* Float voltage, Vprechg : 2.4V */
		dev_dbg(&client->dev, "%s : float voltage (%dmV)\n",
				__func__, charger->pdata->chg_float_voltage);
		data = 0;
		data |= smb136_get_float_voltage_data(
			charger->pdata->chg_float_voltage);
		smb136_set_command(client,
			SMB136_FLOAT_VOLTAGE, data);

		/* 4. Automatic Recharge Disabed */
		data = 0x8C;
		smb136_i2c_write(client, SMB136_CHARGE_CONTROL_A, &data);

		/* 5. Safty timer Disabled */
		data = 0x28;
		smb136_i2c_write(client, SMB136_CHARGE_CONTROL_B, &data);

		/* 6. Disable USB D+/D- Detection */
		data = 0x28;
		smb136_i2c_write(client, SMB136_OTG_CONTROL, &data);

		/* 9. Re-load Enable */
		data = 0x4B;
		smb136_i2c_write(client, SMB136_SAFTY, &data);

		/* HC or USB5 mode */
		smb136_i2c_read(client, SMB136_COMMAND_A, &data);
		data &= 0xF3;
		switch (charger->cable_type) {
		case POWER_SUPPLY_TYPE_MAINS:
		case POWER_SUPPLY_TYPE_MISC:
			/* High-current mode */
			data |= 0x0C;
			break;
		case POWER_SUPPLY_TYPE_USB:
		case POWER_SUPPLY_TYPE_USB_DCP:
		case POWER_SUPPLY_TYPE_USB_CDP:
		case POWER_SUPPLY_TYPE_USB_ACA:
			/* USB5 */
			data |= 0x08;
			break;
		default:
			/* USB1 */
			data = 0x00;
			break;
		}
		smb136_set_command(client,
			SMB136_COMMAND_A, data);
	}
}

static void smb136_charger_otg_conrol(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	smb136_allow_volatile_writes(client);
	if (charger->cable_type ==
		POWER_SUPPLY_TYPE_BATTERY) {
		/* turn off charger */
		smb136_set_command(client,
			SMB136_COMMAND_A, 0x80);
	} else {
		/* turn on OTG */
		smb136_set_command(client,
			SMB136_COMMAND_A, (0x1 << 4));
	}
}

bool sec_hal_chg_init(struct i2c_client *client)
{
	smb136_test_read(client);
	return true;
}

bool sec_hal_chg_suspend(struct i2c_client *client)
{
	return true;
}

bool sec_hal_chg_resume(struct i2c_client *client)
{
	return true;
}

bool sec_hal_chg_get_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb136_get_charging_status(client);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb136_get_charging_health(client);
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_chg_set_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current < 0)
			smb136_charger_otg_conrol(client);
		else if (charger->charging_current > 0)
			smb136_charger_function_conrol(client);
		else {
			smb136_charger_function_conrol(client);
			smb136_charger_otg_conrol(client);
		}
		smb136_test_read(client);
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_chg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_charger_info *chg =
		container_of(psy, struct sec_charger_info, psy_chg);
	int i = 0;
	char *str = NULL;

	switch (offset) {
	case CHG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
			chg->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		smb136_read_regs(chg->client, str);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			str);

		kfree(str);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sec_hal_chg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_charger_info *chg =
		container_of(psy, struct sec_charger_info, psy_chg);
	int ret = 0;
	int x = 0;
	u8 data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			chg->reg_addr = x;
			smb136_i2c_read(chg->client,
				chg->reg_addr, &data);
			chg->reg_data = data;
			dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, chg->reg_data);
			ret = count;
		}
		break;
	case CHG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data = (u8)x;
			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, data);
			smb136_i2c_write(chg->client,
				chg->reg_addr, &data);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

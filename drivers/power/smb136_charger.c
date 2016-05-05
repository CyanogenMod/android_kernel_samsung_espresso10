/*
 *  smb136_charger.c
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Ikkeun Kim <iks.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/power/smb_charger.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/battery.h>

/* SMB136 Registers. */
#define SMB136_CHARGE_CURRENT			0x00
#define SMB136_INPUT_CURRENTLIMIT		0x01
#define SMB136_FLOAT_VOLTAGE			0x02
#define SMB136_CHARGE_CONTROL_A			0x03
#define SMB136_CHARGE_CONTROL_B			0x04
#define SMB136_PIN_ENABLE_CONTROL		0x05
#define SMB136_OTG_CONTROL			0x06
#define SMB136_SAFTY				0x09

#define SMB136_COMMAND_A			0x31
#define SMB136_STATUS_D				0x35
#define SMB136_STATUS_E				0x36

#define CHARGER_STATUS_FULL			0x1
#define CHARGER_STATUS_CHARGERERR		0x2
#define CHARGER_STATUS_USB_FAIL			0x3
#define CHARGER_VBATT_UVLO			0x4

struct smb136_chg_data {
	struct device *dev;
	struct i2c_client *client;
	struct smb_charger_data *pdata;
	struct smb_charger_callbacks callbacks;
};

static int smb136_i2c_read(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret = 0;

	if (!client)
		return -ENODEV;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return -EIO;
	}

	*data = ret & 0xff;
	return 0;
}

static int smb136_i2c_write(struct i2c_client *client, u8 reg, u8 data)
{
	int ret;

	if (!client)
		return -ENODEV;

	ret = i2c_smbus_write_byte_data(client, reg, data);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int smb136_read_status(struct smb_charger_callbacks *ptr)
{
	struct smb136_chg_data *chg = container_of(ptr,
			struct smb136_chg_data, callbacks);
	u8 reg_d;
	u8 reg_e;
	u8 res = 0;
	int ret;

	ret = smb136_i2c_read(chg->client, SMB136_STATUS_D, &reg_d);
	if (ret < 0) {
		dev_err(&chg->client->dev, "%s: I2C read fail addr: 0x%x\n",
			__func__, SMB136_STATUS_D);
		msleep(50);
		smb136_i2c_read(chg->client, SMB136_STATUS_D, &reg_d);
	}

	ret = smb136_i2c_read(chg->client, SMB136_STATUS_E, &reg_e);
	if (ret < 0) {
		dev_err(&chg->client->dev, "%s: I2C read fail addr: 0x%x\n",
				__func__, SMB136_STATUS_E);
		msleep(50);
		smb136_i2c_read(chg->client, SMB136_STATUS_E, &reg_e);
	}

	dev_dbg(&chg->client->dev,
		"addr: 0x%x, data: 0x%x, addr: 0x%x, data: 0x%x\n",
		SMB136_STATUS_D, reg_d, SMB136_STATUS_E, reg_e);

	if (reg_e & 0x40) {
		dev_dbg(&chg->client->dev,
			"Charge current under termination current\n");
		res = CHARGER_STATUS_FULL;
	} else if (reg_e & 0x8) {
		dev_info(&chg->client->dev,
			"Charger status charger err\n");
		res = CHARGER_STATUS_CHARGERERR;
	} else if (reg_d & 0x80) {
		dev_info(&chg->client->dev,
			"USBIN<Vusb-fail OR USBIN>Vovlo\n");
		res = CHARGER_STATUS_USB_FAIL;
	} else if (reg_d & 0x1) {
		dev_info(&chg->client->dev,
			"USBIN<Vusb-fail OR USBIN>Vovlo\n");
		res = CHARGER_VBATT_UVLO;
	}

	return res;
}

static void smb136_set_charging_state(struct smb_charger_callbacks *ptr,
		int cable_status)
{
	struct smb136_chg_data *chg = container_of(ptr,
			struct smb136_chg_data, callbacks);
	u8 data;

	switch (cable_status) {
	case CABLE_TYPE_AC:
		dev_info(&chg->client->dev,
			"charging current limit set to 1.3A/1.5A\n");
		/* HC mode */
		smb136_i2c_write(chg->client, SMB136_COMMAND_A, 0x8c);

		/* Set charge current */
		/* Over HW Rev 09 : 1.5A, else 1.3A */
		data = 0xF4;
		if (chg->pdata->hw_revision < 0x9)
			data = 0xD4;
		smb136_i2c_write(chg->client, SMB136_CHARGE_CURRENT, data);
		break;

	case CABLE_TYPE_USB:
		/* Prevent in-rush current */
		dev_info(&chg->client->dev,
			"charging current limit set to 0.5A\n");

		/* USBIN 500mA mode */
		smb136_i2c_write(chg->client, SMB136_COMMAND_A, 0x88);

		/* Set charge current to 500mA */
		smb136_i2c_write(chg->client, SMB136_CHARGE_CURRENT, 0x14);
		break;

	case CABLE_TYPE_NONE:
	default:
		dev_info(&chg->client->dev,
			"charging current limit set to 0.1A\n");
		/* USB 100mA Mode, USB5/1 Current Levels */
		/* Prevent in-rush current */
		smb136_i2c_write(chg->client, SMB136_COMMAND_A, 0x80);
		udelay(10);

		/* Set charge current to 100mA */
		/* Prevent in-rush current */
		smb136_i2c_write(chg->client, SMB136_CHARGE_CURRENT, 0x14);
		udelay(10);
	}

	/* 2. Change USB5/1/HC Control from Pin to I2C */
	smb136_i2c_write(chg->client, SMB136_PIN_ENABLE_CONTROL, 0x8);

	/* 4. Disable Automatic Input Current Limit */
	/* Over HW Rev 09 : 1.3A, else 1.0A */
	data = 0xE6;
	if (chg->pdata->hw_revision < 0x09)
		data = 0x66;
	smb136_i2c_write(chg->client, SMB136_INPUT_CURRENTLIMIT, data);

	/* 4. Automatic Recharge Disabed */
	smb136_i2c_write(chg->client, SMB136_CHARGE_CONTROL_A, 0x8c);

	/* 5. Safty timer Disabled */
	smb136_i2c_write(chg->client, SMB136_CHARGE_CONTROL_B, 0x28);

	/* 6. Disable USB D+/D- Detection */
	smb136_i2c_write(chg->client, SMB136_OTG_CONTROL, 0x28);

	/* 7. Set Output Polarity for STAT */
	smb136_i2c_write(chg->client, SMB136_FLOAT_VOLTAGE, 0xCA);

	/* 9. Re-load Enable */
	smb136_i2c_write(chg->client, SMB136_SAFTY, 0x4b);
}

static int smb136_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct smb136_chg_data *chg;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chg = kzalloc(sizeof(struct smb136_chg_data), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->client = client;
	if (!chg->client) {
		dev_err(&client->dev, "%s: no client\n", __func__);
		ret = -EINVAL;
		goto err_client;
	} else {
		chg->dev = &client->dev;
	}

	chg->pdata = client->dev.platform_data;
	if (!chg->pdata) {
		dev_err(&client->dev, "%s: no platform data supplied\n", __func__);
		ret = -EINVAL;
		goto err_pdata;
	}

	i2c_set_clientdata(client, chg);

	chg->callbacks.set_charging_state = smb136_set_charging_state;
	chg->callbacks.get_status_reg = smb136_read_status;
	if (chg->pdata->register_callbacks)
		chg->pdata->register_callbacks(&chg->callbacks);

	dev_info(&client->dev, "probed\n");

	return 0;

err_pdata:
err_client:
	dev_err(&client->dev, "%s: probe failed\n", __func__);
	kfree(chg);
	return ret;
}

static int __devexit smb136_remove(struct i2c_client *client)
{
	struct smb136_chg_data *chg = i2c_get_clientdata(client);

	if (chg->pdata && chg->pdata->unregister_callbacks)
		chg->pdata->unregister_callbacks();

	kfree(chg);
	return 0;
}

static const struct i2c_device_id smb136_id[] = {
	{ "smb136-charger", 0 },
	{ }
};


static struct i2c_driver smb136_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "smb136-charger",
	},
	.id_table	= smb136_id,
	.probe	= smb136_i2c_probe,
	.remove	= __devexit_p(smb136_remove),
	.command = NULL,
};


MODULE_DEVICE_TABLE(i2c, smb136_id);

static int __init smb136_init(void)
{
	return i2c_add_driver(&smb136_i2c_driver);
}

static void __exit smb136_exit(void)
{
	i2c_del_driver(&smb136_i2c_driver);
}

module_init(smb136_init);
module_exit(smb136_exit);

MODULE_AUTHOR("Ikkeun Kim <iks.kim@samsung.com>");
MODULE_DESCRIPTION("smb136 charger driver");
MODULE_LICENSE("GPL");

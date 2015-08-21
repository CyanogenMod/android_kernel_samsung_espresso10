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
#include <linux/power/smb136_charger.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/battery.h>

/* Slave address */
#define SMB136_SLAVE_ADDR		0x9A

/* SMB136 Registers. */
#define SMB_ChargeCurrent		0x00
#define SMB_InputCurrentLimit		0x01
#define SMB_FloatVoltage		0x02
#define SMB_ControlA			0x03
#define SMB_ControlB			0x04
#define SMB_PinControl			0x05
#define SMB_OTGControl			0x06
#define SMB_Fault			0x07
#define SMB_Temperature			0x08
#define SMB_SafetyTimer			0x09
#define SMB_VSYS			0x0A
#define SMB_I2CAddr			0x0B

#define SMB_IRQreset			0x30
#define SMB_CommandA			0x31
#define SMB_StatusA			0x32
#define SMB_StatusB			0x33
#define SMB_StatusC			0x34
#define SMB_StatusD			0x35
#define SMB_StatusE			0x36
#define SMB_StatusF			0x37
#define SMB_StatusG			0x38
#define SMB_StatusH			0x39
#define SMB_DeviceID			0x3B
#define SMB_CommandB			0x3C

/* SMB_StatusC register bit. */
#define SMB_USB				1
#define SMB_CHARGER			0
#define Compelete			1
#define Busy				0
#define InputCurrent275			0xE
#define InputCurrent500			0xF
#define InputCurrent700			0x0
#define InputCurrent800			0x1
#define InputCurrent900			0x2
#define InputCurrent1000		0x3
#define InputCurrent1100		0x4
#define InputCurrent1200		0x5
#define InputCurrent1300		0x6
#define InputCurrent1400		0x7

/* SIOP */
#define CHARGING_CURRENT_HIGH_LOW_STANDARD	450

#define CHARGER_STATUS_FULL		0x1
#define CHARGER_STATUS_CHARGERERR	0x2
#define CHARGER_STATUS_USB_FAIL		0x3
#define CHARGER_VBATT_UVLO		0x4

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

	if (!client) {
		dev_err(&client->dev, "%s: err\n", __func__);
		return -ENODEV;
	}

	ret = i2c_smbus_write_byte_data(client, reg, data);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static void smb136_test_read(struct smb136_chg_data *chg)
{
	u8 data = 0;
	u32 addr = 0;

	for (addr = 0; addr < 0x0c; addr++) {
		smb136_i2c_read(chg->client, addr, &data);
		dev_dbg(&chg->client->dev,
			"SMB136 addr : 0x%02x data : 0x%02x\n",	addr, data);
	}

	for (addr = 0x31; addr < 0x3D; addr++) {
		smb136_i2c_read(chg->client, addr, &data);
		dev_dbg(&chg->client->dev,
			"SMB136 addr : 0x%02x data : 0x%02x\n",	addr, data);
	}
}

static int smb136_read_status(struct smb_charger_callbacks *ptr)
{
	struct smb136_chg_data *chg = container_of(ptr,
			struct smb136_chg_data, callbacks);
	u8 reg_d;
	u8 reg_e;
	u8 res = 0;
	int ret;

	ret = smb136_i2c_read(chg->client, SMB_StatusD, &reg_d);
	if (ret < 0) {
		dev_err(&chg->client->dev, "%s : I2C read fail addr: 0x%x\n",
			__func__, SMB_StatusD);
		msleep(50);
		smb136_i2c_read(chg->client, SMB_StatusD, &reg_d);
	}

	ret = smb136_i2c_read(chg->client, SMB_StatusE, &reg_e);
	if (ret < 0) {
		dev_err(&chg->client->dev, "%s : I2C read fail addr: 0x%x\n",
				__func__, SMB_StatusE);
		msleep(50);
		smb136_i2c_read(chg->client, SMB_StatusE, &reg_e);
	}

	dev_info(&chg->client->dev,
	"addr : 0x%x, data : 0x%x, addr : 0x%x, data : 0x%x\n",
		SMB_StatusD, reg_d, SMB_StatusE, reg_e);

	if (reg_e & 0x40) {
		dev_info(&chg->client->dev,
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

static int smb136_get_charging_current(struct smb136_chg_data *chg)
{
	u8 data = 0;
	int get_current = 0;

	smb136_i2c_read(chg->client, SMB_ChargeCurrent, &data);
	switch (data >> 5) {
	case 0:
		get_current = 500;
		break;
	case 1:
		get_current = 650;
		break;
	case 2:
		get_current = 750;
		break;
	case 3:
		get_current = 850;
		break;
	case 4:
		get_current = 950;
		break;
	case 5:
		get_current = 1100;
		break;
	case 6:
		get_current = 1300;
		break;
	case 7:
		get_current = 1500;
		break;
	default:
		get_current = 500;
		break;
	}

	dev_info(&chg->client->dev, "%s: Get charging current as %dmA.\n",
		__func__, get_current);

	return get_current;
}

static void smb136_set_charging_state(struct smb_charger_callbacks *ptr,
		int cable_status)
{
	struct smb136_chg_data *chg = container_of(ptr,
			struct smb136_chg_data, callbacks);
	u8 data = 0;

	switch (cable_status) {
	case CABLE_TYPE_AC:
		dev_info(&chg->client->dev,
				"%s: set charger current TA\n",
				__func__);
		/* HC mode */
		data = 0x8c;
		smb136_i2c_write(chg->client, SMB_CommandA, data);

		/* Set charge current */
		/* Over HW Rev 09 : 1.5A, else 1.3A */
		data = 0xF4;
		if (chg->pdata->hw_revision < 0x9)
			data = 0xD4;
		smb136_i2c_write(chg->client, SMB_ChargeCurrent, data);
		break;

	case CABLE_TYPE_USB:
		/* Prevent in-rush current */
		dev_info(&chg->client->dev,
				"%s: set charger current USB & Default\n",
				__func__);

		/* USBIN 500mA mode */
		data = 0x88;
		smb136_i2c_write(chg->client, SMB_CommandA, data);

		/* Set charge current to 500mA */
		data = 0x14;
		smb136_i2c_write(chg->client, SMB_ChargeCurrent, data);
		break;

	case CABLE_TYPE_NONE:
	default:
		dev_info(&chg->client->dev, "%s: Set discharging default\n",
				__func__);
		/* USB 100mA Mode, USB5/1 Current Levels */
		/* Prevent in-rush current */
		data = 0x80;
		smb136_i2c_write(chg->client, SMB_CommandA, data);
		udelay(10);

		/* Set charge current to 100mA */
		/* Prevent in-rush current */
		data = 0x14;
		smb136_i2c_write(chg->client, SMB_ChargeCurrent, data);
		udelay(10);
	}

	/* 2. Change USB5/1/HC Control from Pin to I2C */
	smb136_i2c_write(chg->client, SMB_PinControl, 0x8);

	/* 4. Disable Automatic Input Current Limit */
	/* Over HW Rev 09 : 1.3A, else 1.0A */
	data = 0xE6;
	if (chg->pdata->hw_revision < 0x09)
		data = 0x66;
	smb136_i2c_write(chg->client, SMB_InputCurrentLimit, data);

	/* 4. Automatic Recharge Disabed */
	data = 0x8c;
	smb136_i2c_write(chg->client, SMB_ControlA, data);

	/* 5. Safty timer Disabled */
	data = 0x28;
	smb136_i2c_write(chg->client, SMB_ControlB, data);

	/* 6. Disable USB D+/D- Detection */
	data = 0x28;
	smb136_i2c_write(chg->client, SMB_OTGControl, data);

	/* 7. Set Output Polarity for STAT */
	data = 0xCA;
	smb136_i2c_write(chg->client, SMB_FloatVoltage, data);

	/* 9. Re-load Enable */
	data = 0x4b;
	smb136_i2c_write(chg->client, SMB_SafetyTimer, data);
}

static int smb136_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct smb136_chg_data *chg;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	dev_info(&client->dev, "%s : SMB136 Charger Driver Loading\n",
		__func__);

	chg = kzalloc(sizeof(struct smb136_chg_data), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->client = client;
	if (!chg->client) {
		pr_err("%s : No client\n", __func__);
		ret = -EINVAL;
		goto err_client;
	} else {
		chg->dev = &client->dev;
	}

	chg->pdata = client->dev.platform_data;
	if (!chg->pdata) {
		pr_err("%s : No platform data supplied\n", __func__);
		ret = -EINVAL;
		goto err_pdata;
	}

	i2c_set_clientdata(client, chg);

	chg->callbacks.set_charging_state = smb136_set_charging_state;
	chg->callbacks.get_status_reg = smb136_read_status;
	if (chg->pdata->register_callbacks)
		chg->pdata->register_callbacks(&chg->callbacks);

	dev_info(&client->dev, "Smb136 charger attach success!!!\n");
	return 0;

err_pdata:
err_client:
	dev_info(&client->dev, "%s: SMB136 probe fail !!!!!\n",
		__func__);
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

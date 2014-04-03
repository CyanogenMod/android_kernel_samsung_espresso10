/*
 * drivers/power/bq2415x_battery.c
 *
 * BQ24153 / BQ24156 battery charging driver
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
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
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/bq2415x.h>
#include <linux/interrupt.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>

static int timer_fault;

struct charge_params {
	unsigned long		currentmA;
	unsigned long		voltagemV;
	unsigned long		term_currentmA;
	unsigned long		enable_iterm;
	bool			enable;
};

struct bq2415x_device_info *dev_info_irq;

struct bq2415x_device_info {
	struct device		*dev;
	struct work_struct	bq24157_work;
	struct work_struct	bq24157_vf_work;
	struct i2c_client	*client;
	struct bq2415x_platform_data *pdata;
	struct bq2415x_charger_callbacks callbacks;
	struct charge_params	params;

	unsigned short		status_reg;
	unsigned short		control_reg;
	unsigned short		voltage_reg;
	unsigned short		bqchip_version;
	unsigned short		current_reg;
	unsigned short		special_charger_reg;

	unsigned int		cin_limit;
	unsigned int		currentmA;
	unsigned int		voltagemV;
	unsigned int		max_currentmA;
	unsigned int		max_voltagemV;
	unsigned int		term_currentmA;

	bool			cfg_params;
	bool			enable_iterm;
	bool			active;
};

static int bq2415x_write_block(struct bq2415x_device_info *di, u8 *value,
						u8 reg, unsigned num_bytes)
{
	struct i2c_msg msg[1];
	int ret;

	*value		= reg;

	msg[0].addr	= di->client->addr;
	msg[0].flags	= 0;
	msg[0].buf	= value;
	msg[0].len	= num_bytes + 1;

	ret = i2c_transfer(di->client->adapter, msg, 1);

	/* i2c_transfer returns number of messages transferred */
	if (ret != 1) {
		dev_err(di->dev,
			"i2c_write failed to transfer all messages\n");
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}

static int bq2415x_read_block(struct bq2415x_device_info *di, u8 *value,
						u8 reg, unsigned num_bytes)
{
	struct i2c_msg msg[2];
	u8 buf;
	int ret;

	buf		= reg;

	msg[0].addr	= di->client->addr;
	msg[0].flags	= 0;
	msg[0].buf	= &buf;
	msg[0].len	= 1;

	msg[1].addr	= di->client->addr;
	msg[1].flags	= I2C_M_RD;
	msg[1].buf	= value;
	msg[1].len	= num_bytes;

	ret = i2c_transfer(di->client->adapter, msg, 2);

	/* i2c_transfer returns number of messages transferred */
	if (ret != 2) {
		dev_err(di->dev,
			"i2c_write failed to transfer all messages\n");
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}

static int bq2415x_write_byte(struct bq2415x_device_info *di, u8 value, u8 reg)
{
	/* 2 bytes offset 1 contains the data offset 0 is used by i2c_write */
	u8 temp_buffer[2] = { 0 };

	/* offset 1 contains the data */
	temp_buffer[1] = value;
	return bq2415x_write_block(di, temp_buffer, reg, 1);
}

static int bq2415x_read_byte(struct bq2415x_device_info *di, u8 *value, u8 reg)
{
	return bq2415x_read_block(di, value, reg, 1);
}


static void bq2415x_config_status_reg(struct bq2415x_device_info *di)
{
	di->status_reg = (TIMER_RST | ENABLE_STAT_PIN);
	bq2415x_write_byte(di, di->status_reg, REG_STATUS_CONTROL);
	return;
}

static void bq2415x_config_control_reg(struct bq2415x_device_info *di)
{
	u8 Iin_limit;

	if (di->cin_limit <= 100)
		Iin_limit = 0;
	else if (di->cin_limit > 100 && di->cin_limit <= 500)
		Iin_limit = 1;
	else if (di->cin_limit > 500 && di->cin_limit <= 800)
		Iin_limit = 2;
	else
		Iin_limit = 3;

	di->control_reg = ((Iin_limit << INPUT_CURRENT_LIMIT_SHIFT)
				| (1 << ENABLE_ITERM_SHIFT));
	bq2415x_write_byte(di, di->control_reg, REG_CONTROL_REGISTER);
	return;
}

void bq2415x_config_control_reg_nolimit()
{
	u8 read_reg = 0;
	struct bq2415x_device_info *di = dev_info_irq;

	bq2415x_read_byte(di, &read_reg, REG_CONTROL_REGISTER);

	di->control_reg = ((3 << INPUT_CURRENT_LIMIT_SHIFT)
				| (1 << ENABLE_ITERM_SHIFT));

	bq2415x_write_byte(di, di->control_reg, REG_CONTROL_REGISTER);
	bq2415x_read_byte(di, &read_reg, REG_CONTROL_REGISTER);
	return;
}

void bq2415x_config_control_reg_limit()
{
	u8 read_reg = 0;
	struct bq2415x_device_info *di = dev_info_irq;

	bq2415x_read_byte(di, &read_reg, REG_CONTROL_REGISTER);

	di->control_reg = ((1 << INPUT_CURRENT_LIMIT_SHIFT)
				| (1 << ENABLE_ITERM_SHIFT));

	bq2415x_write_byte(di, di->control_reg, REG_CONTROL_REGISTER);
	bq2415x_read_byte(di, &read_reg, REG_CONTROL_REGISTER);
	return;
}

static void bq2415x_config_voltage_reg(struct bq2415x_device_info *di)
{
	unsigned int voltagemV;
	u8 Voreg;

	voltagemV = di->voltagemV;
	if (voltagemV < 3500)
		voltagemV = 3500;
	else if (voltagemV > 4440)
		voltagemV = 4440;

	Voreg = (voltagemV - 3500)/20;
	di->voltage_reg = (Voreg << VOLTAGE_SHIFT);
	bq2415x_write_byte(di, di->voltage_reg, REG_BATTERY_VOLTAGE);
	return;
}

static void bq2415x_config_current_reg(struct bq2415x_device_info *di)
{
	unsigned int currentmA = 0;
	unsigned int term_currentmA = 0;
	u8 Vichrg = 0;
	u8 shift = 0;
	u8 Viterm = 0;
	u8 read_reg = 0;

	currentmA = di->currentmA;
	term_currentmA = di->term_currentmA;
	dev_info(di->dev, "charge current : %d\n", di->currentmA);

	if (currentmA < 550)
		currentmA = 550;

	if ((di->bqchip_version & (BQ24153 | BQ24158 | BQ24157))) {
		shift = BQ24153_CURRENT_SHIFT;
		if (currentmA > 1250)
			currentmA = 1250;
	}

	if ((di->bqchip_version & BQ24156)) {
		shift = BQ24156_CURRENT_SHIFT;
		if (currentmA > 1550)
			currentmA = 1550;
	}

	if (term_currentmA > 350)
		term_currentmA = 350;

	Vichrg = (currentmA - 550)/100;
	Viterm = term_currentmA/50 - 1;

	di->current_reg = (Vichrg << shift | Viterm);
	bq2415x_write_byte(di, di->current_reg, REG_BATTERY_CURRENT);
	return;
}

static void bq2415x_config_special_charger_reg(struct bq2415x_device_info *di)
{
	u8 Vsreg = 2;	/* DPM voltage :  4.36V for HARRISON 20120704*/

	di->special_charger_reg = Vsreg;
	bq2415x_write_byte(di, di->special_charger_reg,
					REG_SPECIAL_CHARGER_VOLTAGE);
	return;
}

static void bq2415x_config_safety_reg(struct bq2415x_device_info *di,
						unsigned int max_currentmA,
						unsigned int max_voltagemV)
{
	u8 Vmchrg;
	u8 Vmreg;
	u8 limit_reg;

	if (max_currentmA < 550)
		max_currentmA = 550;
	else if (max_currentmA > 1550)
		max_currentmA = 1550;


	if (max_voltagemV < 4200)
		max_voltagemV = 4200;
	else if (max_voltagemV > 4440)
		max_voltagemV = 4440;

	di->max_voltagemV = max_voltagemV;
	di->max_currentmA = max_currentmA;
	di->voltagemV = max_voltagemV;
	di->currentmA = max_currentmA;

	Vmchrg = (max_currentmA - 550)/100;
	Vmreg = (max_voltagemV - 4200)/20;
	limit_reg = ((Vmchrg << MAX_CURRENT_SHIFT) | Vmreg);
	bq2415x_write_byte(di, limit_reg, REG_SAFETY_LIMIT);
	return;
}

static void
bq2415x_charger_update_status(struct bq2415x_device_info *di)
{
	u8 read_reg[7] = {0};
	int i = 0;

	timer_fault = 0;
	bq2415x_read_block(di, &read_reg[0], 0, 7);

	for (i = 0; i < 7; i++)
		dev_info(di->dev, "Register value [%d] : 0x%x\n",
			i, read_reg[i]);

	dev_dbg(di->dev, "Charging Status : %x\n", read_reg[0]);
	if ((read_reg[0] & 0x30) == 0x20)
		dev_info(di->dev, "CHARGE DONE\n");

	if ((read_reg[0] & 0x7) == 0x6)
		timer_fault = 1;

	if (read_reg[0] & 0x7) {
		di->cfg_params = 1;
		dev_err(di->dev, "CHARGER FAULT %x\n", read_reg[0]);
	}

	if ((timer_fault == 1) || (di->cfg_params == 1)) {
		bq2415x_write_byte(di, di->control_reg, REG_CONTROL_REGISTER);
		bq2415x_write_byte(di, di->voltage_reg, REG_BATTERY_VOLTAGE);
		bq2415x_write_byte(di, di->current_reg, REG_BATTERY_CURRENT);
		bq2415x_config_special_charger_reg(di);
		di->cfg_params = 0;
	}

	/* reset 32 second timer */
	bq2415x_config_status_reg(di);

	return;
}

static void bq24157_irq_work(struct work_struct *bq24157_work)
{
	u8 read_reg = 0;
	struct bq2415x_device_info *di = dev_info_irq;

	/* 500ms delay is required * change delay IRQ  */
	msleep(500);

	bq2415x_read_block(di, &read_reg, 0, 1);

	if ((read_reg & 0x30) == 0x20) {
		if (di->term_currentmA == di->pdata->first_term_currentmA) {
				dev_info(di->dev, "Full charged\n");
				di->pdata->set_full_charge();
		}
	}
}

static irqreturn_t bq2415x_charging_isr(int irq, void *data)
{
	struct bq2415x_device_info *di = data;
	schedule_work(&di->bq24157_work);
	return IRQ_HANDLED;
}

static void bq24157_irq_vf_work(struct work_struct *bq24157_vf_work)
{
	u8 read_reg = 0;
	int state = 0;
	struct bq2415x_device_info *di = dev_info_irq;

	/*if battery is not detected, make charging set disable*/
	if (!gpio_get_value(di->pdata->vf_gpio))
		state = 0;
	else
		state = 1;

	gpio_set_value(di->pdata->ta_enable_gpio, !state);

	pr_debug("%s: Set charge status : %d, current status: %d\n",
		__func__, state,
		gpio_get_value(di->pdata->ta_enable_gpio));

}

static irqreturn_t bq2415x_vf_isr(int irq, void *data)
{
	struct bq2415x_device_info *di = data;
	schedule_work(&di->bq24157_vf_work);
	return IRQ_HANDLED;
}


static void set_charge_current(struct bq2415x_charger_callbacks *ptr,
			int cable_type)
{
	struct bq2415x_device_info *di = container_of(ptr,
	struct bq2415x_device_info, callbacks);

	if (cable_type == 2)
		di->currentmA = di->pdata->charge_ac_current; /* Cable AC */
	else if (cable_type == 1)
		di->currentmA = di->pdata->charge_usb_current; /* Cable USB */

	bq2415x_config_current_reg(di);
}

static void set_term_current(struct bq2415x_charger_callbacks *ptr,
			int term_type)
{
	struct bq2415x_device_info *di = container_of(ptr,
	struct bq2415x_device_info, callbacks);

	di->term_currentmA = di->pdata->first_term_currentmA;
	di->enable_iterm = 1;
	bq2415x_config_current_reg(di);
}

static int __devinit bq2415x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bq2415x_device_info *di;
	int ret;
	u8 read_reg = 0;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;
	dev_info_irq = di;
	di->dev = &client->dev;
	di->client = client;
	i2c_set_clientdata(client, di);
	di->pdata = client->dev.platform_data;

	ret = bq2415x_read_byte(di, &read_reg, REG_PART_REVISION);
	if (ret < 0) {
		dev_err(&client->dev, "chip not present at address %x\n",
								client->addr);
		ret = -EINVAL;
		goto err_kfree;
	}

	if ((read_reg & 0x18) == 0x10 && ((client->addr == 0x6B) ||
			(client->addr == 0x6A)))
		di->bqchip_version = BQ24157;

	if (di->bqchip_version == 0) {
		dev_info(&client->dev, "charger chip rev error \r");
		dev_info(&client->dev, "unknown bq chip\n");
		dev_info(&client->dev, "Chip address %x", client->addr);
		dev_info(&client->dev, "chip version reg %x", read_reg);
		ret = -EINVAL;
		goto err_kfree;
	}

	bq2415x_config_safety_reg(di, di->pdata->max_charger_currentmA,
					di->pdata->max_charger_voltagemV);
	di->cin_limit = di->pdata->cin_limit_current;
	di->currentmA = di->pdata->charge_ac_current;
	di->term_currentmA = di->pdata->first_term_currentmA;
	bq2415x_config_control_reg(di);
	bq2415x_config_voltage_reg(di);
	bq2415x_config_current_reg(di);

	di->active = 0;
	di->params.enable = 1;
	di->cfg_params = 1;
	di->enable_iterm = 1;

	di->callbacks.set_charge_current = set_charge_current;
	di->callbacks.set_termination_current = set_term_current;
	if (di->pdata->register_callbacks)
		di->pdata->register_callbacks(&di->callbacks);

	INIT_WORK(&di->bq24157_work, bq24157_irq_work);

	if (di->pdata->ta_irq < 0) {
		ret = di->pdata->ta_irq;
		goto err_kfree;
	}

	ret = request_irq(di->pdata->ta_irq, bq2415x_charging_isr,
			  IRQF_TRIGGER_RISING, "TA_nCHG", di);

	if (ret < 0) {
		dev_err(&client->dev,
				"request_threaded_irq() failed: %d", ret);
		goto err_kfree;
	}

	ret = enable_irq_wake(di->pdata->ta_irq);

	if (ret) {
		pr_err("%s : Failed to enable_irq_wake.\n", __func__);
		goto err_enable_irq_wake;
	}

	INIT_WORK(&di->bq24157_vf_work, bq24157_irq_vf_work);

	if (di->pdata->vf_irq < 0) {
		ret = di->pdata->vf_irq;
		goto err_kfree;
	}

	ret = request_irq(di->pdata->vf_irq, bq2415x_vf_isr,
			  IRQF_TRIGGER_FALLING, "BAT_REMOVAL", di);

	if (ret < 0) {
		dev_err(&client->dev,
				"request_threaded_irq() failed: %d", ret);
		goto err_kfree;
	}

	ret = bq2415x_read_byte(di, &read_reg, REG_SPECIAL_CHARGER_VOLTAGE);
	if (!(read_reg & 0x08))
		di->active = 1;

	bq2415x_charger_update_status(di);

	return 0;

err_enable_irq_wake:
	free_irq(di->pdata->ta_irq, di);

err_kfree:
	kfree(di);

	return ret;
}

static int __devexit bq2415x_charger_remove(struct i2c_client *client)
{
	struct bq2415x_device_info *di = i2c_get_clientdata(client);

	kfree(di);

	return 0;
}

static const struct i2c_device_id bq2415x_id[] = {
	{ "bq2415x_charger", 0 },
	{},
};

static struct i2c_driver bq2415x_charger_driver = {
	.probe		= bq2415x_charger_probe,
	.remove		= __devexit_p(bq2415x_charger_remove),
	.id_table	= bq2415x_id,
	.driver		= {
	.name	= "bq2415x_charger",
	},
};

static int __init bq2415x_charger_init(void)
{
	return i2c_add_driver(&bq2415x_charger_driver);
}
module_init(bq2415x_charger_init);

static void __exit bq2415x_charger_exit(void)
{
	i2c_del_driver(&bq2415x_charger_driver);
}
module_exit(bq2415x_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments Inc");

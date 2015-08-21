/*
 * Copyright (C) 2011 Samsung Electronics
 *
 * Authors: Shankar Bandal <shankar.b@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/earlysuspend.h>
#include <linux/firmware.h>

#include "../../../arch/arm/mach-omap2/sec_common.h"

#define DEVICE_NAME "sec_touchkey"
#define I2C_M_WR 0      /* for i2c */
#define FW_SIZE 8192

#define CYPRESS_GEN		0x00
#define CYPRESS_DATA_UPDATE	0x40

/*
 * Cypress touchkey register
 */
#define KEYCODE_REG	0x00
#define CMD_REG		0x03
#define THRESHOLD_REG	0x04
#define AUTOCAL_REG	0x05
#define IDAC_REG	0x06
#define DIFF_DATA_REG	0x0A
#define RAW_DATA_REG	0x0E

/* Command for 0x00 */
#define AUTO_CAL_MODE_CMD	0x50
#define LED_ON_CMD		0x10
#define LED_OFF_CMD		0x20

/* Command for 0x03 */
#define AUTO_CAL_EN_CMD		0x01
#define SENS_EN_CMD		0x40

#define UPDOWN_EVENT_BIT	0x08
#define KEYCODE_BIT		0x07
#define COMMAND_BIT		0xF0
#define TK_BIT_AUTOCAL		0x80

struct cptk_data {
	struct cptk_platform_data	*pdata;
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct device *sec_touchkey;
	struct early_suspend early_suspend;
	struct mutex    i2c_lock;
	struct mutex    lock;
	u8 led_status;
	u8 cur_firm_ver[3];
	int touchkey_update_status;
	bool enable;

};

static irqreturn_t cptk_irq_thread(int irq, void *data);

static int cptk_i2c_write(struct cptk_data *cptk, u8 cmd,
		u8 val)
{
	int ret = 0;
	u8 data[2];
	struct i2c_msg msg[1];
	int retry = 2;

	if (!cptk->enable) {
		pr_debug("cptk :key is not enabled.\n");
		return -ENODEV;
	}

	mutex_lock(&cptk->i2c_lock);

	while (retry--) {
		data[0] = cmd;
		data[1] = val;
		msg->addr = cptk->client->addr;
		msg->flags = I2C_M_WR;
		msg->len = 2;
		msg->buf = data;
		ret = i2c_transfer(cptk->client->adapter, msg, 1);
		if (ret >= 0) {
			mutex_unlock(&cptk->i2c_lock);
			return 0;
		}
		pr_debug("cptk :i2c transfer retor for cmd:%d\n",
		       cmd);
		msleep(20);
	}

	mutex_unlock(&cptk->i2c_lock);

	return ret;
}

static int cptk_i2c_read(struct cptk_data *cptk, u8 cmd,
		u8 *val, int len)
{
	int ret = 0;
	int retry = 10;
	struct i2c_msg msg[2];

	if (!cptk->enable) {
		pr_debug("cptk :key is not enabled.\n");
		return -ENODEV;
	}

	mutex_lock(&cptk->i2c_lock);

	while (retry--) {
		msg[0].addr = cptk->client->addr;
		msg[0].flags = I2C_M_WR;
		msg[0].len = 1;
		msg[0].buf = &cmd;

		msg[1].addr = cptk->client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = len;
		msg[1].buf = val;

		ret = i2c_transfer(cptk->client->adapter, msg, 2);
		if (ret >= 0) {
			mutex_unlock(&cptk->i2c_lock);
			return 0;
		}
		pr_debug("cptk :i2c transfer retor for cmd:%d\n",
		       cmd);
		msleep(20);
	}
	mutex_unlock(&cptk->i2c_lock);

	return ret;
}

static irqreturn_t cptk_irq_thread(int irq, void *data)
{
	struct cptk_data *cptk = data;
	int ret;
	u8 keycode;

	mutex_lock(&cptk->lock);
	if (!gpio_get_value(cptk->pdata->gpio)) {
		ret = cptk_i2c_read(cptk, KEYCODE_REG, &keycode, 1);
		if (keycode & UPDOWN_EVENT_BIT)
			input_report_key(cptk->input_dev,
					cptk->pdata->keymap[keycode &
					KEYCODE_BIT], 0);
		else
			input_report_key(cptk->input_dev,
					cptk->pdata->keymap[keycode &
					KEYCODE_BIT], 1);

		input_sync(cptk->input_dev);
	}
	mutex_unlock(&cptk->lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static int cptk_early_suspend(struct early_suspend *h)
{
	int i;

	struct cptk_data *cptk = container_of(h,
			struct cptk_data,
			early_suspend);

	disable_irq(cptk->client->irq);
	mutex_lock(&cptk->lock);
	if (cptk && cptk->pdata->power)
		cptk->pdata->power(0);
	cptk->enable = false;

	/* releae key */
	for (i = 1; i < cptk->pdata->keymap_size; i++)
		input_report_key(cptk->input_dev,
				 cptk->pdata->keymap[i], 0);
	mutex_unlock(&cptk->lock);
	return 0;
}

static int cptk_late_resume(struct early_suspend *h)
{
	struct cptk_data *cptk = container_of(h,
			struct cptk_data,
			early_suspend);

	mutex_lock(&cptk->lock);
	if (cptk && cptk->pdata->power)
		cptk->pdata->power(1);
	cptk->enable = true;

	cptk_i2c_write(cptk, KEYCODE_REG, AUTO_CAL_MODE_CMD);
	cptk_i2c_write(cptk, CMD_REG, AUTO_CAL_EN_CMD);
	msleep(50);

	if (cptk->led_status == LED_ON_CMD)
		cptk_i2c_write(cptk, KEYCODE_REG, LED_ON_CMD);

	mutex_unlock(&cptk->lock);
	enable_irq(cptk->client->irq);
	return 0;
}
#endif
static void cptk_update_firmware_cb(const struct firmware *fw,
		void *context)
{

	int ret;
	struct cptk_data *cptk = context;
	struct device *dev = &cptk->input_dev->dev;
	int retries = 3;

	pr_info("cptk: firware download start\n");

	if (fw->size != FW_SIZE) {
		dev_err(dev, "%s: Firmware file size invalid size:%d\n",
				__func__, fw->size);
		return;
	}

	mutex_lock(&cptk->lock);

	disable_irq(cptk->client->irq);
	/* Lock the i2c bus since the firmware updater accesses it */
	i2c_lock_adapter(cptk->client->adapter);
	while (retries--) {
		ret = touchkey_flash_firmware(cptk->pdata, fw->data);
		if (!ret)
			break;
	}
	if (ret) {
		cptk->touchkey_update_status = -1;
		dev_err(dev, "%s: Firmware update failed\n", __func__);
	} else {
		cptk->touchkey_update_status = 0;
		pr_info("cptk: firware download finished\n");
	}

	i2c_unlock_adapter(cptk->client->adapter);
	enable_irq(cptk->client->irq);

	release_firmware(fw);
	mutex_unlock(&cptk->lock);

	cptk_i2c_read(cptk, KEYCODE_REG, cptk->cur_firm_ver,
						sizeof(cptk->cur_firm_ver));

	pr_info("cptk: current firm ver = 0x%.2x, latest firm ver = 0x%.2x\n",
				cptk->cur_firm_ver[1], cptk->pdata->firm_ver);

	return;
}

static int cptk_update_firmware(struct cptk_data *cptk)
{
	int ret;
	struct device *dev = &cptk->input_dev->dev;
	cptk->touchkey_update_status = 1;
	if (!cptk->pdata->fw_name) {
		dev_err(dev, "%s: Device firmware name is not set\n", __func__);
		return -EINVAL;
	}

	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			cptk->pdata->fw_name, dev, GFP_KERNEL, cptk,
			cptk_update_firmware_cb);

	if (ret) {
		dev_err(dev, "%s: Can't open firmware file from %s\n", __func__,
			cptk->pdata->fw_name);
		return ret;
	}

	return 0;
}

static ssize_t set_touchkey_firm_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct cptk_data *cptk = dev_get_drvdata(dev);

	if (*buf == 'S' || *buf == 'F') {
		if ((*buf != 'F' &&
		cptk->cur_firm_ver[1] >=
		cptk->pdata->firm_ver) &&
		cptk->cur_firm_ver[1] != 0xFF) {
			cptk->touchkey_update_status = 0;

			pr_debug("cptk: already updated latest version\n");
			return size;
		}
		cptk_update_firmware(cptk);
	}

	return size;
}
static DEVICE_ATTR(touchkey_firm_update,
		S_IWUSR | S_IWGRP,
		NULL, set_touchkey_firm_update_store);

static ssize_t set_touchkey_firm_status_show
(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct cptk_data *cptk = dev_get_drvdata(dev);

	if (cptk->touchkey_update_status == 0)
		count = sprintf(buf, "PASS\n");
	else if (cptk->touchkey_update_status == 1)
		count = sprintf(buf, "DOWNLOADING\n");
	else if (cptk->touchkey_update_status == -1)
		count = sprintf(buf, "FAIL\n");

	return count;
}
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO,
					set_touchkey_firm_status_show, NULL);

static ssize_t set_touchkey_firm_version_show
(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

	struct cptk_data *cptk = dev_get_drvdata(dev);
	count = sprintf(buf, "0x%.2X\n", cptk->cur_firm_ver[1]);
	pr_debug("cptk: touchkey_firm_version 0x%.2X\n", cptk->cur_firm_ver[1]);

	return count;
}
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO,
set_touchkey_firm_version_show, NULL);

static ssize_t set_touchkey_firm_version_read_show
(struct device *dev, struct device_attribute *attr, char *buf)
{
	char data[3] = { 0, };
	int count, ret;
	struct cptk_data *cptk = dev_get_drvdata(dev);

	mutex_lock(&cptk->lock);
	ret = cptk_i2c_read(cptk, KEYCODE_REG, data, 3);
	if (ret) {
		pr_err("cptk : %s: error in cptk_i2c_read\n" , __func__);
		mutex_unlock(&cptk->lock);
		return ret;
	}
	mutex_unlock(&cptk->lock);
	count = sprintf(buf, "0x%.2X\n", data[1]);
	pr_debug("cptk :touch_version_read 0x%.2X\n", data[1]);

	return count;
}
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO,
set_touchkey_firm_version_read_show, NULL);

static ssize_t touch_led_control(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct cptk_data *cptk = dev_get_drvdata(dev);
	int data;
	int ret;

	mutex_lock(&cptk->lock);
	ret = sscanf(buf, "%d\n", &data);
	if (unlikely(ret != 1)) {
		pr_err("cptk: %s err\n", __func__);
		mutex_unlock(&cptk->lock);
		return -EINVAL;
	}

	data = data<<4;
	cptk_i2c_write(cptk, KEYCODE_REG, data);
	cptk->led_status = data;
	mutex_unlock(&cptk->lock);

	return size;
}
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, touch_led_control);

static ssize_t touchkey_menu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cptk_data *cptk = dev_get_drvdata(dev);
	u8 data[2];
	int ret;
	int menu_sensitivity;

	mutex_lock(&cptk->lock);
	ret = cptk_i2c_read(cptk, DIFF_DATA_REG,
			data, sizeof(data));

	menu_sensitivity = ((0x00FF&data[0])<<8)|data[1];
	pr_debug("cptk : menu_sensitivity = %d\n", menu_sensitivity);

	mutex_unlock(&cptk->lock);

	return sprintf(buf, "%d\n", menu_sensitivity);
}
static DEVICE_ATTR(touchkey_menu, S_IRUGO, touchkey_menu_show, NULL);

static ssize_t touchkey_back_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	u8 data[2];
	int ret;
	int back_sensitivity;

	struct cptk_data *cptk = dev_get_drvdata(dev);
	mutex_lock(&cptk->lock);
	ret = cptk_i2c_read(cptk, DIFF_DATA_REG + 2,
			data, sizeof(data));

	back_sensitivity = ((0x00FF&data[0])<<8)|data[1];
	pr_debug("cptk : back_sensitivity = %d\n", back_sensitivity);

	mutex_unlock(&cptk->lock);

	return sprintf(buf, "%d\n", back_sensitivity);
}
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_back_show, NULL);

static ssize_t touch_sensitivity_control(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct cptk_data *cptk = dev_get_drvdata(dev);

	mutex_lock(&cptk->lock);
	cptk_i2c_write(cptk, KEYCODE_REG, SENS_EN_CMD);
	mutex_unlock(&cptk->lock);

	return size;
}
static DEVICE_ATTR(touch_sensitivity,
		S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, touch_sensitivity_control);

static ssize_t touchkey_raw_data0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cptk_data *tkey_i2c = dev_get_drvdata(dev);
	u8 data[2];
	int ret;
	u16 raw_data0;

	ret = cptk_i2c_read(tkey_i2c, RAW_DATA_REG, data, sizeof(data));
	raw_data0 = ((0x00FF & data[0]) << 8) | data[1];

	return sprintf(buf, "%d\n", raw_data0);
}
static DEVICE_ATTR(touchkey_raw_data0, S_IRUGO, touchkey_raw_data0_show, NULL);

static ssize_t touchkey_raw_data1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cptk_data *tkey_i2c = dev_get_drvdata(dev);
	u8 data[2];
	int ret;
	u16 raw_data1;

	ret = cptk_i2c_read(tkey_i2c, RAW_DATA_REG + 2, data, sizeof(data));
	raw_data1 = ((0x00FF & data[0]) << 8) | data[1];

	return sprintf(buf, "%d\n", raw_data1);
}
static DEVICE_ATTR(touchkey_raw_data1, S_IRUGO, touchkey_raw_data1_show, NULL);

static ssize_t touchkey_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cptk_data *tkey_i2c = dev_get_drvdata(dev);
	u8 data;
	int ret;

	ret = cptk_i2c_read(tkey_i2c, THRESHOLD_REG, &data, sizeof(data));

	return sprintf(buf, "%d\n", data);

}
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold_show, NULL);

static ssize_t touchkey_autocal_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cptk_data *tkey_i2c = dev_get_drvdata(dev);
	u8 data;
	int ret;

	ret = cptk_i2c_read(tkey_i2c, AUTOCAL_REG, &data, sizeof(data));
	if (data & TK_BIT_AUTOCAL)
		return sprintf(buf, "Enabled\n");

	return sprintf(buf, "Disabled\n");
}
static DEVICE_ATTR(autocal_stat, S_IRUGO, touchkey_autocal_status_show, NULL);

static ssize_t touchkey_idac0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cptk_data *tkey_i2c = dev_get_drvdata(dev);
	u8 data;
	int ret;

	ret = cptk_i2c_read(tkey_i2c, IDAC_REG, &data, sizeof(data));

	return sprintf(buf, "%d\n", data);
}
static DEVICE_ATTR(touchkey_idac0, S_IRUGO,
		touchkey_idac0_show, NULL);

static ssize_t touchkey_idac1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cptk_data *tkey_i2c = dev_get_drvdata(dev);
	u8 data;
	int ret;

	ret = cptk_i2c_read(tkey_i2c, IDAC_REG+1, &data, sizeof(data));

	return sprintf(buf, "%d\n", data);
}
static DEVICE_ATTR(touchkey_idac1, S_IRUGO,
		touchkey_idac1_show, NULL);

static int cptk_create_sec_touchkey(struct cptk_data *cptk)
{
	int ret;

	cptk->sec_touchkey = device_create(sec_class, NULL,
			0, NULL, "sec_touchkey");
	if (IS_ERR(cptk->sec_touchkey))
		goto err;

	ret = device_create_file(cptk->sec_touchkey, &dev_attr_brightness);
	if (ret < 0) {
		pr_err("cptk :Failed to create device file %s\n",
				dev_attr_brightness.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
		&dev_attr_touchkey_firm_update);
	if (ret < 0) {
		pr_err("cptk: Failed to create device file %s\n",
			dev_attr_touchkey_firm_update.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
		&dev_attr_touchkey_firm_update_status);
	if (ret < 0) {
		pr_err("cptk: Failed to create device file(%s)!\n",
		dev_attr_touchkey_firm_update_status.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
		&dev_attr_touchkey_firm_version_phone);
	if (ret < 0) {
		pr_err("cptk: Failed to create device file(%s)!\n",
		dev_attr_touchkey_firm_version_phone.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
		&dev_attr_touchkey_firm_version_panel);
	if (ret < 0) {
		pr_err("cptk :Failed to create device file(%s)!\n",
		dev_attr_touchkey_firm_version_panel.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_touchkey_menu);
	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_touchkey_menu.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_touchkey_back);
	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_touchkey_back.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_touch_sensitivity);
	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_touch_sensitivity.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_touchkey_raw_data0);
	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_touchkey_raw_data0.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_touchkey_raw_data1);
	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_touchkey_raw_data1.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_touchkey_threshold);
	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_touchkey_threshold.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_autocal_stat);

	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_autocal_stat.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_touchkey_idac0);

	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_touchkey_idac0.attr.name);
		goto err;
	}

	ret = device_create_file(cptk->sec_touchkey,
			&dev_attr_touchkey_idac1);

	if (ret < 0) {
		pr_err("cptk : Failed to create device file %s\n",
		dev_attr_touchkey_idac1.attr.name);
		goto err;
	}

	dev_set_drvdata(cptk->sec_touchkey, cptk);

	return 0;
err:
	return -EINVAL;
}
static int __devinit cptk_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct cptk_data *cptk;
	int ret;
	int i;

	cptk = kzalloc(sizeof(struct cptk_data), GFP_KERNEL);
	if (!cptk) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	cptk->input_dev = input_allocate_device();
	if (!cptk->input_dev)
		return -ENOMEM;

	cptk->pdata = client->dev.platform_data;
	if (!cptk->pdata) {
		ret = -EINVAL;
		goto err_exit1;
	}
	cptk->client = client;
	strlcpy(cptk->client->name, "cypress-touchkey", I2C_NAME_SIZE);
	cptk->client->dev.init_name = DEVICE_NAME;
	i2c_set_clientdata(client, cptk);

	cptk->input_dev->name = DEVICE_NAME;
	cptk->input_dev->phys = "cypress-touchkey/input0";
	cptk->input_dev->id.bustype = BUS_HOST;

	set_bit(EV_SYN, cptk->input_dev->evbit);
	set_bit(EV_KEY, cptk->input_dev->evbit);
	set_bit(EV_LED, cptk->input_dev->evbit);
	set_bit(LED_MISC, cptk->input_dev->ledbit);

	for (i = 1; i < cptk->pdata->keymap_size; i++)
		set_bit(cptk->pdata->keymap[i], cptk->input_dev->keybit);

	ret = input_register_device(cptk->input_dev);
	if (ret) {
		input_free_device(cptk->input_dev);
		return ret;
	}

	mutex_init(&cptk->i2c_lock);
	mutex_init(&cptk->lock);

	if (cptk->pdata->power)
		cptk->pdata->power(1);
	cptk->enable = true;

	/* Check Touch Key IC connect properly && read IC fw. version */
	ret = cptk_i2c_read(cptk, KEYCODE_REG, cptk->cur_firm_ver,
						sizeof(cptk->cur_firm_ver));
	if (ret < 0) {
		pr_err("cptk: %s: touch key IC is not connected.\n", __func__);
		goto err_exit1;
	}

	pr_info("cptk: module ver = 0x%.2x, IC firm ver = 0x%.2x, binary firm ver = 0x%.2x\n",
			cptk->cur_firm_ver[2],
			cptk->cur_firm_ver[1],
			cptk->pdata->firm_ver);

	if (cptk->cur_firm_ver[2] == cptk->pdata->mod_ver) {
		if (cptk->cur_firm_ver[1] < cptk->pdata->firm_ver) {
			pr_info("cptk: force firmware update\n");
			ret = cptk_update_firmware(cptk);
			if (ret)
				goto err_exit1;
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	cptk->early_suspend.level =
		EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 2;
	cptk->early_suspend.suspend = (void *) cptk_early_suspend;
	cptk->early_suspend.resume = (void *) cptk_late_resume;
	register_early_suspend(&cptk->early_suspend);
#endif

	cptk_i2c_write(cptk, KEYCODE_REG, AUTO_CAL_MODE_CMD);
	cptk_i2c_write(cptk, CMD_REG, AUTO_CAL_EN_CMD);

	ret = request_threaded_irq(client->irq, NULL, cptk_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   DEVICE_NAME, cptk);
	if (ret < 0)
		goto err_exit2;

	ret = cptk_create_sec_touchkey(cptk);
	if (ret < 0)
		goto err_exit2;

	return 0;

err_exit2:
err_exit1:
	kfree(cptk);
	return ret;
}

static int __devexit cptk_remove(struct i2c_client *client)
{
	return 0;
}

static void cptk_shutdown(struct i2c_client *client)
{
	struct cptk_data *cptk = i2c_get_clientdata(client);

	disable_irq(client->irq);
	if (cptk && cptk->pdata->power)
		cptk->pdata->power(0);
}

static const struct i2c_device_id cptk_id[] = {
	{"cypress_touchkey", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, cptk_id);

static struct i2c_driver cptk_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "cypress_touchkey",
	},
	.id_table = cptk_id,
	.probe = cptk_i2c_probe,
	.remove = __devexit_p(cptk_remove),
	.shutdown = cptk_shutdown,
	.command = NULL,
};

static int __init cptk_init(void)
{
	int ret;

	ret = i2c_add_driver(&cptk_i2c_driver);
	if (ret < 0)
		return ret;

	return ret;
}

static void __exit cptk_exit(void)
{
	i2c_del_driver(&cptk_i2c_driver);
}

module_init(cptk_init);
module_exit(cptk_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("shankar bandal <shankar.b@samsung.com>");
MODULE_DESCRIPTION("cypress touch keypad");

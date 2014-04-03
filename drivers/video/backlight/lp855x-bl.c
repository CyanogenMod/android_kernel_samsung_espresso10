/*
 * LP855X Backlight Driver
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
 * Author: Seungjin Kim <nayaksj.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/platform_data/lp855x.h>

/* LP855X I2C registers */
#define LP855X_REG_BRT		0x00
#define LP855X_REG_CTRL		0x01
#define LP855X_REG_ID		0x03
#define LP855X_REG_CFG3		0xA3
#define LP855X_REG_CFG5		0xA5

/* LP855X I2C control */
#define PSMODE_SHIFT	4
#define PSMODE_MASK	(0x7 << PSMODE_SHIFT)

#define BRTMODE_SHIFT	1
#define BRTMODE_MASK	(0x3 << BRTMODE_SHIFT)

enum auto_brt_val {
	AUTO_BRIGHTNESS_MANUAL = 0,
	AUTO_BRIGHTNESS_VAL1,
	AUTO_BRIGHTNESS_VAL2,
	AUTO_BRIGHTNESS_VAL3,
	AUTO_BRIGHTNESS_VAL4,
	AUTO_BRIGHTNESS_VAL5,
};

struct lp855x_info {
	struct i2c_client *client;
	struct lp855x_pdata *pdata;
	struct backlight_device *bd;
	struct mutex lock;
	struct early_suspend early_suspend;
	int enabled;
	enum auto_brt_val auto_brightness;
};

static u8 lp855x_read_reg(struct i2c_client *client, u8 reg)
{
	struct lp855x_info *info = i2c_get_clientdata(client);
	u8 val;

	mutex_lock(&info->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&info->lock);

	if (unlikely(val < 0))
		dev_err(&client->dev, "failed to read reg[%x], ret=%d\n",
								reg, val);

	return val;
}

static int lp855x_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	struct lp855x_info *info = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&info->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&info->lock);

	if (unlikely(ret < 0))
		dev_err(&client->dev, "failed to write reg[%x, %x], ret=%d\n",
								reg, val, ret);

	return ret;
}

static int lp855x_update_reg(struct i2c_client *client, u8 reg, u8 val,
							u8 mask, u8 shift)
{
	u8 cur;
	u8 new;
	int ret;

	cur = lp855x_read_reg(client, reg);
	if (unlikely(cur < 0))
		return cur;

	new = (cur & ~mask) | ((val << shift) & mask);

	ret = lp855x_write_reg(client, reg, new);
	if (unlikely(ret < 0))
		return ret;

	return 0;
}

static int lp855x_send_intensity(struct backlight_device *bd)
{
	struct lp855x_info *info = dev_get_drvdata(&bd->dev);
	struct i2c_client *client = info->client;
	struct lp855x_pdata *pdata = info->pdata;
	u8 intensity = bd->props.brightness;
	u8 brt;
	int ret;

	if (!info->enabled)
		return -EPERM;

	if (bd->props.power != FB_BLANK_UNBLANK ||
			bd->props.fb_blank != FB_BLANK_UNBLANK) {
		dev_info(&bd->dev, "%s: set intensity: %d to 0\n",
							__func__, intensity);
		intensity = 0;
	}

	if (pdata->brt_mode != BRT_MODE_REG) {
		if (pdata->send_intensity)
			ret = pdata->send_intensity(intensity);
		if (unlikely(ret < 0)) {
			dev_err(&client->dev, "failed to send intensity\n");
			return ret;
		}
	}

	if (pdata->brt_mode != BRT_MODE_PWM) {
		brt = intensity;
		if (brt > LP855X_BRIGHTNESS_MAX)
			brt = LP855X_BRIGHTNESS_MAX;

		ret = lp855x_write_reg(client, LP855X_REG_BRT, brt);
		if (unlikely(ret < 0)) {
			dev_err(&client->dev, "failed to set brt\n");
			return ret;
		}
	}

	return 0;
}

static int lp855x_get_intensity(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops lp855x_ops = {
	.get_brightness = lp855x_get_intensity,
	.update_status  = lp855x_send_intensity,
};

/* Initialilzes LP8556 chip register */
static int lp855x_config(struct lp855x_info *info)
{
	struct i2c_client *client = info->client;
	u8 ps_mode = info->pdata->ps_mode;
	u8 brt_mode = info->pdata->brt_mode;
	u8 brightness = info->bd->props.brightness;
	u8 val;
	int ret;

	/* BRIGHTNESS CONTROL: If brightness source is not only LP8556 register
	 * value, this bit has no effect.
	 */
	if (brt_mode == BRT_MODE_REG) {
		ret = lp855x_write_reg(client, LP855X_REG_BRT, brightness);
		if (unlikely(ret < 0)) {
			dev_err(&client->dev, "failed to set brightness\n");
			goto err;
		}
	}

	/* DEVICE CONTROL: No FAST bit to prevent LP8556 register reset,
	 * BRT_MODE and BL_CTL from platform data
	 */
	val = brt_mode <<  BRTMODE_SHIFT | 0x81;
	ret = lp855x_write_reg(client, LP855X_REG_CTRL, val);
	if (unlikely(ret < 0)) {
		dev_err(&client->dev, "failed to set device ctrl\n");
		goto err;
	}

	/* CFG3: SCURVE_EN is linear transitions, SLOPE = 200ms,
	 * FILTER = heavy smoothing,
	 * PWM_INPUT_HYSTERESIS = 1-bit hysteresis with 12-bit resolution
	 */
	ret = lp855x_write_reg(client, LP855X_REG_CFG3, 0x5E);
	if (unlikely(ret < 0)) {
		dev_err(&client->dev, "failed to set cfg3\n");
		goto err;
	}

	/* CFG5: No PWM_DIRECT, PS_MODE from platform data, PWM_FREQ = 9616Hz */
	val = ps_mode << PSMODE_SHIFT | 0x04;
	ret = lp855x_write_reg(client, LP855X_REG_CFG5, val);
	if (unlikely(ret < 0)) {
		dev_err(&client->dev, "failed to set cfg5\n");
		goto err;
	}

	dev_info(&client->dev, "%s: brt_mode=0x%x, ps_mode=0x%x\n",
						__func__, brt_mode, ps_mode);
	return 0;

err:
	dev_err(&client->dev, "%s: failed lp855x config(%d)\n", __func__, ret);
	return ret;
}

static void lp855x_power(struct lp855x_info *info, int on)
{
	struct lp855x_pdata *pdata = info->pdata;

	if (pdata->set_power)
		pdata->set_power(on);
}

static int lp855x_enable(struct lp855x_info *info)
{
	lp855x_power(info, 1);
	usleep_range(1000, 1100);
	lp855x_config(info);

	info->enabled = 1;

	return 0;
}

static int lp855x_disable(struct lp855x_info *info)
{
	info->enabled = 0;
	lp855x_power(info, 0);

	return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void lp855x_early_suspend(struct early_suspend *h)
{
	struct lp855x_info *info =
		container_of(h, struct lp855x_info, early_suspend);
	int ret;

	ret = lp855x_disable(info);
	if (unlikely(ret < 0)) {
		pr_err("%s: failed to disable lp855x\n", __func__);
		return;
	}
}

static void lp855x_late_resume(struct early_suspend *h)
{
	struct lp855x_info *info =
		container_of(h, struct lp855x_info, early_suspend);
	int ret;

	ret = lp855x_enable(info);
	if (unlikely(ret < 0)) {
		pr_err("%s: failed to enable lp855x\n", __func__);
		return;
	}

	backlight_update_status(info->bd);
}
#endif


static ssize_t lp8556_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp855x_info *info = dev_get_drvdata(dev);
	u8 val_ctrl;
	u8 val_cfg5;

	val_ctrl = lp855x_read_reg(info->client, LP855X_REG_CTRL);
	val_cfg5 = lp855x_read_reg(info->client, LP855X_REG_CFG5);

	return sprintf(buf, "CTRL=%x,CFG5=%x\n", val_ctrl, val_cfg5);
}

static ssize_t lp8556_mode_store(struct device *dev,
		struct device_attribute *attr, char *buf, size_t count)
{
	struct lp855x_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int input;
	u8 psmode;
	u8 brtmode;
	int ret;

	ret = kstrtoint(buf, 10, &input);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to get input value, buf=%s\n",
				__func__, buf);
		return count;
	}

	if (input & 0x01)
		psmode = 0x01;
	else
		psmode = 0x02;

	lp855x_update_reg(client, LP855X_REG_CFG5, psmode,
					PSMODE_MASK, PSMODE_SHIFT);

	if (input & 0x02)
		brtmode = 0x02;
	else
		brtmode = 0x00;

	lp855x_update_reg(client, LP855X_REG_CTRL, brtmode,
					BRTMODE_MASK, BRTMODE_SHIFT);

	dev_info(&client->dev, "%s: psmode=0x%x, brtmode=0x%x\n",
						__func__, psmode, brtmode);

	return count;
}

static DEVICE_ATTR(mode, S_IRWXUGO, lp8556_mode_show, lp8556_mode_store);

static ssize_t auto_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lp855x_info *info = dev_get_drvdata(dev);

	pr_info("%s: auto_brightness=%d\n", __func__, info->auto_brightness);

	return sprintf(buf, "%d\n", info->auto_brightness);
}

static ssize_t auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct lp855x_info *info = dev_get_drvdata(dev);
	struct lp855x_pdata *pdata = info->pdata;
	int value;

	sscanf(buf, "%d", &value);

	if (value > AUTO_BRIGHTNESS_VAL5 || value < AUTO_BRIGHTNESS_MANUAL) {
		pr_err("%s: invalid input value(%s)\n", __func__, buf);
		return count;
	}
	dev_info(dev, "%s: value = %d\n", __func__, value);

	if (value == info->auto_brightness)
		return count;

	info->auto_brightness = value;

	if (pdata->set_auto_brt)
		pdata->set_auto_brt(value);

	lp855x_send_intensity(info->bd);

	return count;
}

static DEVICE_ATTR(auto_brightness, S_IRUGO|S_IWUSR|S_IWGRP,
				auto_brightness_show, auto_brightness_store);

static struct attribute *lp855x_attributes[] = {
	&dev_attr_mode.attr,
	&dev_attr_auto_brightness.attr,
	NULL,
};

static const struct attribute_group lp855x_group = {
	.attrs  = lp855x_attributes,
};

static int __devinit lp855x_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct lp855x_pdata *pdata = client->dev.platform_data;
	struct lp855x_info *info;
	struct backlight_properties props;
	int ret;
	u8 val;

	if (!pdata) {
		dev_err(&client->dev, "missing platform data\n");
		return -EINVAL;
	}

	info = kzalloc(sizeof(struct lp855x_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	mutex_init(&info->lock);
	info->client = client;
	info->pdata = pdata;
	i2c_set_clientdata(client, info);

	lp855x_power(info, 1);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.brightness = pdata->default_intensity;
	props.max_brightness = LP855X_BRIGHTNESS_MAX;
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK;

	info->bd = backlight_device_register("panel", &client->dev,
						info, &lp855x_ops, &props);
	if (IS_ERR(info->bd)) {
		dev_err(&client->dev, "failed to register backlight\n");
		ret = PTR_ERR(info->bd);
		goto err_bl;
	}

	ret = sysfs_create_group(&info->bd->dev.kobj, &lp855x_group);
	if (unlikely(ret < 0)) {
		dev_warn(&client->dev, "failed to create sysfs\n");
		goto err_sysfs;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	info->early_suspend.suspend = lp855x_early_suspend;
	info->early_suspend.resume = lp855x_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	val = lp855x_read_reg(client, LP855X_REG_ID);
	if (val < 0) {
		dev_err(&client->dev, "failed to get LP8556 ID\n");
		ret = val;
		goto err_i2c;
	}

	dev_info(&client->dev, "LP855X chip ID = 0x%x\n", val);

	lp855x_config(info);

	info->enabled = 1;

	return 0;

err_i2c:
	sysfs_remove_group(&info->bd->dev.kobj, &lp855x_group);
err_sysfs:
	backlight_device_unregister(info->bd);
err_bl:
	kfree(info);

	return ret;
}

static int lp855x_remove(struct i2c_client *client)
{
	struct lp855x_info *info = i2c_get_clientdata(client);
	struct backlight_device *bd = info->bd;

	bd->props.power = 0;
	bd->props.brightness = 0;
	backlight_update_status(bd);
	lp855x_power(info, 0);

	backlight_device_unregister(bd);
	i2c_set_clientdata(client, NULL);
	mutex_destroy(&info->lock);
	kfree(info);

	return 0;
}

static void lp855x_shutdown(struct i2c_client *client)
{
	struct lp855x_info *info = i2c_get_clientdata(client);

	lp855x_power(info, 0);
}

static const struct i2c_device_id lp855x_i2c_id[] = {
	{"lp8556", 0},
	{}
};

static struct i2c_driver lp855x_i2c_driver = {
	.driver		= {
		.name	= "lp855x-bl",
	},
	.id_table	= lp855x_i2c_id,
	.probe		= lp855x_probe,
	.remove		= lp855x_remove,
	.shutdown	= lp855x_shutdown,
};

static int __init lp855x_init(void)
{
	return i2c_add_driver(&lp855x_i2c_driver);
}

static void __exit lp855x_exit(void)
{
	i2c_del_driver(&lp855x_i2c_driver);
}

module_init(lp855x_init);
module_exit(lp855x_exit);

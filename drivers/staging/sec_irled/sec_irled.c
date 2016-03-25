/*
 * driver/staging/sec_irled
 *
 * SEC IrLED driver using AVOV MC96 IC for Smart Remote
 *
 * Copyright (C) 2012 Samsung Electronics
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/sec_irled.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include "mc96_fw.h"

#define MC96_READ_LENGTH	8

#define USE_STOP_MODE

#define LENG_OFFSET	0
#define FREQ_OFFSET	2
#define SIG_OFFSET		5
#define CSUM_LENG		2
#define MAX_SIZE		(SIG_OFFSET + CSUM_LENG + 2048)

#define I2C_RETRY_COUNT	5

struct sec_irled_data {
	struct i2c_client *client;
	struct sec_irled_platform_data *pdata;
	struct early_suspend early_suspend;
	bool busy_flag;
	spinlock_t lock;
	char signal[MAX_SIZE];
	int length;
	int count;
	int dev_id;
	int ir_freq;
	int ir_sum;
	int on_off;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_irled_early_suspend(struct early_suspend *h);
static void sec_irled_late_resume(struct early_suspend *h);
#endif

static int mc96_fw_update(struct sec_irled_data *ir_data)
{
	struct sec_irled_data *data = ir_data;
	struct i2c_client *client = data->client;
	int i, ret;
	u8 buf_ir_test[8];
	unsigned int retry_count = I2C_RETRY_COUNT - 1;

	data->pdata->ir_vdd_onoff(0);
	data->pdata->ir_wake_en(1);
	data->pdata->ir_vdd_onoff(1);

	do {
		ret = i2c_master_recv(client, buf_ir_test, MC96_READ_LENGTH);
		if (ret < 0) {
			pr_err("irled: i2c read error %d\n", ret);
			if (!retry_count--) {
				pr_err("irled: i2c error is not recovered\n");
				goto err_i2c_fail;
			}
		}
	} while (!ret);

	ret = buf_ir_test[2] << 8 | buf_ir_test[3];
	if (ret < FW_VERSION) {
		pr_info("irled: chip : %04x, bin : %04x, need update!\n",
							ret, FW_VERSION);
		data->pdata->ir_vdd_onoff(0);
		data->pdata->ir_wake_en(0);
		data->pdata->ir_vdd_onoff(1);

		ret = i2c_master_recv(client, buf_ir_test, MC96_READ_LENGTH);
		if (ret < 0)
			pr_err("irled: i2c read error %d\n", ret);

		ret = buf_ir_test[6] << 8 | buf_ir_test[7];

		if (ret == 0x01fe)
			pr_info("irled: boot mode, FW download start\n");
		else
			goto err_bootmode;
		msleep(30);

		for (i = 0; i < FRAME_COUNT; i++) {
			if (i == FRAME_COUNT-1) {
				ret = i2c_master_send(client,
						&FW_binary[i * 70], 6);
				if (ret < 0)
					goto err_update;
			} else {
				ret = i2c_master_send(client,
						&FW_binary[i * 70], 70);
				if (ret < 0)
					goto err_update;
			}
			msleep(30);
		}

		ret = i2c_master_recv(client, buf_ir_test, MC96_READ_LENGTH);
		if (ret < 0)
			pr_err("irled: i2c read error %d\n", ret);

		ret = buf_ir_test[6] << 8 | buf_ir_test[7];

		if (ret == 0x02a3)
			pr_info("irled: boot down complete\n");
		else
			goto err_bootmode;

		data->pdata->ir_vdd_onoff(0);
		data->pdata->ir_wake_en(1);
		data->pdata->ir_vdd_onoff(1);

		ret = i2c_master_recv(client, buf_ir_test, MC96_READ_LENGTH);

		ret = buf_ir_test[2] << 8 | buf_ir_test[3];
		pr_info("irled: user mode dev: %04x\n", ret);
		data->pdata->ir_vdd_onoff(0);
		data->on_off = 0;
	} else {
		pr_info("irled: chip : %04x, bin : %04x, not update\n",
							ret, FW_VERSION);
		data->pdata->ir_wake_en(0);
		data->pdata->ir_vdd_onoff(0);
		data->on_off = 0;
	}

	return 0;
err_i2c_fail:
	pr_err("irled: update fail! i2c ret : %x\n", ret);
err_update:
	pr_err("irled: update fail! count : %x, ret = %x\n", i, ret);
err_bootmode:
	pr_err("irled: update fail, ret = %x\n", ret);
	data->pdata->ir_vdd_onoff(0);
	data->on_off = 0;
	return ret;
}

static void mc96_add_checksum_length(struct sec_irled_data *ir_data)
{
	struct sec_irled_data *data = ir_data;
	int i = 0, csum = 0;

	data->signal[LENG_OFFSET] = data->count >> 8;
	data->signal[LENG_OFFSET+1] = data->count & 0xff;

	while (i < data->count)
		csum += data->signal[i++];

	pr_debug("irled: checksum: %04x\n", csum);

	data->signal[data->count] = csum >> 8;
	data->signal[data->count + 1] = csum & 0xff;

	data->count += CSUM_LENG;

	pr_debug("irled: length: %04x\n", data->count);
}

static int mc96_read_device_info(struct sec_irled_data *ir_data)
{
	struct sec_irled_data *data = ir_data;
	struct i2c_client *client = data->client;
	u8 buf_ir_test[8];
	int ret;
	unsigned int retry_count = I2C_RETRY_COUNT - 1;

	data->pdata->ir_wake_en(1);
	data->pdata->ir_vdd_onoff(1);

	do {
		ret = i2c_master_recv(client, buf_ir_test, MC96_READ_LENGTH);
		if (ret < 0) {
			pr_err("irled: i2c read error %d\n", ret);
			if (!retry_count--) {
				pr_err("irled: i2c error is not recovered\n");
				goto dev_info_err;
			}
		}
	} while (!ret);

	pr_info("irled: buf_ir dev_id: 0x%02x, 0x%02x\n",
					buf_ir_test[2], buf_ir_test[3]);
	ret = data->dev_id = (buf_ir_test[2] << 8 | buf_ir_test[3]);

	data->pdata->ir_wake_en(0);
	data->pdata->ir_vdd_onoff(0);
	data->on_off = 0;

dev_info_err:
	return ret;
}

static void sec_irled_send(struct sec_irled_data *ir_data)
{

	struct sec_irled_data *data = ir_data;
	struct i2c_client *client = data->client;

	int ret;
	int sleep_timing;
	int end_data;
	int emission_time;
	unsigned int retry_count = I2C_RETRY_COUNT - 1;

	do {
		ret = i2c_master_send(client, data->signal, data->count);
		if (ret < 0) {
			pr_err("irled: i2c write error %d\n", ret);
			if (!retry_count--) {
				pr_err("irled: i2c error is not recovered\n");
				return;
			}
			data->pdata->ir_vdd_onoff(0);
			data->pdata->ir_vdd_onoff(1);
			data->on_off = 1;
		}
	} while (!ret);

	end_data = data->signal[data->count - 4] << 8
					| data->signal[data->count - 3];
	emission_time =
		(1000 * (data->ir_sum - end_data) / (data->ir_freq)) + 10;
	sleep_timing = emission_time - 130;
	if (sleep_timing > 0)
		msleep(sleep_timing);
	pr_debug("irled: sleep_timing = %d\n", sleep_timing);
#ifndef USE_STOP_MODE
	data->pdata->ir_vdd_onoff(0);
	data->on_off = 0;
	data->pdata->ir_wake_en(0);
#endif
	data->ir_freq = 0;
	data->ir_sum = 0;
}

static ssize_t remocon_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_irled_data *data = dev_get_drvdata(dev);
	unsigned int _data;

	if (sscanf(buf++, "%u", &_data) == 1) {
		if (unlikely(!_data)) {
			pr_err("irled: invalid input\n");
			return -EINVAL;
		}
	} else {
		pr_err("irled: invalid write operation\n");
		return -EINVAL;
	}

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);


	if (data->on_off) {
		data->pdata->ir_wake_en(0);
		data->pdata->ir_wake_en(1);
		msleep(20);
	} else {
		data->pdata->ir_wake_en(1);
		data->pdata->ir_vdd_onoff(1);
		data->on_off = 1;
	}

	data->ir_freq = _data;
	data->signal[FREQ_OFFSET] = _data >> 16;
	data->signal[FREQ_OFFSET+1] = (_data >> 8) & 0xFF;
	data->signal[FREQ_OFFSET+2] = _data & 0xFF;

	while (_data > 0) {
		buf++;
		_data /= 10;
	}

	for (data->count = SIG_OFFSET; data->count < MAX_SIZE;) {
		if (sscanf(buf++, "%u", &_data) == 1) {
			if (!_data)
				break;

			data->ir_sum += _data;
			data->signal[data->count++] = _data >> 8;
			data->signal[data->count++] = _data & 0xFF;

			while (_data > 0) {
				buf++;
				_data /= 10;
			}
		} else
			break;
	}

	mc96_add_checksum_length(data);
	sec_irled_send(data);

	spin_lock(&data->lock);
		data->busy_flag = false;
	spin_unlock(&data->lock);

	return size;
}

static ssize_t remocon_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_irled_data *data = dev_get_drvdata(dev);
	int i;
	char *bufp = buf;

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	if (data->count) {
		bufp += sprintf(bufp, "%u,", data->signal[FREQ_OFFSET] << 16
			| data->signal[FREQ_OFFSET+1] << 8
			| data->signal[FREQ_OFFSET+2]);
		pr_debug("%u,", data->signal[FREQ_OFFSET] << 16
			| data->signal[FREQ_OFFSET+1] << 8
			| data->signal[FREQ_OFFSET+2]);
	}

	for (i = SIG_OFFSET; i < MAX_SIZE; i += 2) {
		if (data->signal[i] == 0 && data->signal[i+1] == 0)
			break;
		else {
			bufp += sprintf(bufp, "%u,",
				data->signal[i] << 8 | data->signal[i+1]);
			pr_debug("%u,",
				data->signal[i] << 8 | data->signal[i+1]);
		}
	}

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return strlen(buf);
}

static DEVICE_ATTR(ir_send, S_IRUGO | S_IWUSR | S_IWGRP,
				remocon_show, remocon_store);

static ssize_t check_ir_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_irled_data *data = dev_get_drvdata(dev);
	int ret;

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	ret = mc96_read_device_info(data);

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return snprintf(buf, 4, "%d\n", ret);
}

static DEVICE_ATTR(check_ir, S_IRUGO, check_ir_show, NULL);


static int __devinit sec_irled_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sec_irled_data *data;
	struct device *sec_irled_dev;
	int ret;

	pr_info("irled: probe\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	data = kzalloc(sizeof(struct sec_irled_data), GFP_KERNEL);
	if (unlikely(!data)) {
		pr_err("irled: failed to allocate memory\n");
		return -ENOMEM;
	}

	data->client = client;
	data->pdata = client->dev.platform_data;

	if (unlikely(!data->pdata->ir_vdd_onoff
					|| !data->pdata->ir_wake_en)) {
		pr_err("irled: invalid platform_data\n");
		ret = -EINVAL;
		goto err_free_mem;
	}

	i2c_set_clientdata(client, data);

	mc96_fw_update(data);

	spin_lock_init(&data->lock);

	if (!sec_class) {
		pr_err("irled: sec_class is invalid\n");
		ret = -EINVAL;
		goto err_free_mem;
	}

	sec_irled_dev = device_create(sec_class, NULL,
		client->dev.devt, data, "sec_ir");

	if (IS_ERR(sec_irled_dev)) {
		pr_err("irled: failed to create sec_irled_dev device\n");
		ret = -ENOMEM;
		goto err_create_dev;
	}

	if (device_create_file(sec_irled_dev, &dev_attr_ir_send) < 0) {
		pr_err("irled: failed to create device file(%s)!\n",
				dev_attr_ir_send.attr.name);
		ret = -ENOMEM;
		goto err_create_dev_file;
	}

	if (device_create_file(sec_irled_dev, &dev_attr_check_ir) < 0) {
		pr_err("irled: failed to create device file(%s)!\n",
				dev_attr_check_ir.attr.name);
		ret = -ENOMEM;
		goto err_create_dev_file;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = sec_irled_early_suspend;
	data->early_suspend.resume = sec_irled_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	return 0;

err_create_dev_file:
	device_destroy(sec_class, client->dev.devt);
err_create_dev:
	i2c_set_clientdata(client, NULL);
err_free_mem:
	kfree(data);
	return ret;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int sec_irled_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sec_irled_data *data = i2c_get_clientdata(client);

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	data->pdata->ir_vdd_onoff(0);
	data->on_off = 0;
	data->pdata->ir_wake_en(0);

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return 0;
}

static int sec_irled_resume(struct device *dev)
{
	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_irled_early_suspend(struct early_suspend *h)
{
	struct sec_irled_data *data;
	data = container_of(h, struct sec_irled_data, early_suspend);
	sec_irled_suspend(&data->client->dev);
}

static void sec_irled_late_resume(struct early_suspend *h)
{
	struct sec_irled_data *data;
	data = container_of(h, struct sec_irled_data, early_suspend);
	sec_irled_resume(&data->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops sec_irled_pm_ops = {
	.suspend	= sec_irled_suspend,
	.resume	= sec_irled_resume,
};
#endif

static int __devexit sec_irled_remove(struct i2c_client *client)
{
	struct sec_irled_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	kfree(data);
	return 0;
}

static const struct i2c_device_id mc96_id[] = {
	{"mc96", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, mc96_id);

static struct i2c_driver sec_irled_i2c_driver = {
	.driver = {
		.name = "mc96",
	},
	.probe = sec_irled_probe,
	.remove = __devexit_p(sec_irled_remove),
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
	.pm	= &sec_irled_pm_ops,
#endif

	.id_table = mc96_id,
};

static int __init sec_irled_init(void)
{
	return i2c_add_driver(&sec_irled_i2c_driver);
}
module_init(sec_irled_init);

static void __exit sec_irled_exit(void)
{
	i2c_del_driver(&sec_irled_i2c_driver);
}
module_exit(sec_irled_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SEC IrLED driver");

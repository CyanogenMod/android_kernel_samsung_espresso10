/* drivers/input/touchscreen/melfas_ts.c
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define DEBUG_PRINT	0

#if DEBUG_PRINT
#define	tsp_log(fmt, args...) \
				pr_info("tsp: %s: " fmt, __func__, ## args)
#else
#define tsp_log(fmt, args...)
#endif

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/semaphore.h>
#include <linux/firmware.h>
#include <linux/battery.h>
#include <linux/uaccess.h>

#include <linux/touchscreen/melfas.h>
#include <linux/platform_data/sec_ts.h>

#include "../../../arch/arm/mach-omap2/board-espresso.h"

#define MELFAS_MAX_TOUCH		10

struct ts_data {
	u16			addr;
	u32			flags;
	bool			finger_state[MELFAS_MAX_TOUCH];
	struct semaphore	poll;
	struct i2c_client	*client;
	struct input_dev	*input_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
	struct sec_ts_platform_data *platform_data;
};

static int ts_read_reg_data(struct ts_data *ts, u8 address, int size, u8 *buf)
{
	if (i2c_master_send(ts->client, &address, 1) < 0)
		return -1;

	/* Need to minimum 50 usec. delay time between next i2c comm.*/
	udelay(50);

	if (i2c_master_recv(ts->client, buf, size) < 0)
		return -1;
	udelay(50);

	return 1;
}

static int ts_write_reg_data(struct ts_data *ts, u8 address, int size, u8 *buf)
{
	int ret = 1;
	u8 *msg_buf;

	msg_buf = kzalloc(size + 1, GFP_KERNEL);
	msg_buf[0] = address;
	memcpy(msg_buf + 1, buf, size);

	if (i2c_master_send(ts->client, msg_buf, size + 1) < 0)
		ret = -1;

	kfree(msg_buf);
	return ret;
}

static void reset_points(struct ts_data *ts)
{
	int i;

	for (i = 0; i < MELFAS_MAX_TOUCH; i++) {
		ts->finger_state[i] = 0;
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
									false);
	}
	input_sync(ts->input_dev);
	tsp_log("reset_all_fingers");
}

#define TS_MODE_CONTROL_REG		0x01

static bool init_tsp(struct ts_data *ts)
{
	u8 buf = 0x02;
	/* b'0 000 001 0
	 * Set TSP to Active mode and
	 * Scheduled Multi-event base Interrupt-driven Report Mode
	 */
	if (ts_write_reg_data(ts, TS_MODE_CONTROL_REG, 1, &buf) < 0) {
		pr_err("tsp: init_tsp: i2c_master_send() failed!!\n");
		return false;
	}
	reset_points(ts);

	return true;
}

static void reset_tsp(struct ts_data *ts)
{
	ts->platform_data->set_power(false);
	mdelay(200);
	ts->platform_data->set_power(true);
	mdelay(200);
	init_tsp(ts);

	tsp_log("reset tsp ic done");
}

#define TS_READ_VERSION_ADDR		0xF0

static bool fw_updater(struct ts_data *ts, char const *mode)
{
	const struct firmware *fw;
	const u32 fw_version = *(u32 *)ts->platform_data->fw_info;
	struct i2c_client *client = ts->client;
	u8 buf[4] = {0, };
	bool ret = true, updated = true;

	tsp_log("Enter the fw_updater");

	if (request_firmware(&fw, ts->platform_data->fw_name, &client->dev)) {
		pr_err("tsp: fail to request built-in firmware\n");
		ret = false;
		goto out;
	}

	if (ts_read_reg_data(ts, TS_READ_VERSION_ADDR, 4, buf) > 0) {
		pr_info("tsp: binary fw. ver: 0x%.2x, IC fw. ver: 0x%.2x\n",
							fw_version, buf[0]);
	} else {
		pr_err("tsp: fw. ver. read fail!!\n");
		mode = "force";
	}

	if ((!strcmp("force", mode)) || (buf[0] < fw_version)) {
		pr_info("tsp: fw_updater: fw. force upload.\n");
		ret = isp_updater(fw->data, fw->size, ts->platform_data);
	} else {
		pr_info("tsp: fw_updater: No need to fw. update.\n");
		updated = false;
	}

	if (updated) {
		reset_tsp(ts);
		if (ts_read_reg_data(ts, TS_READ_VERSION_ADDR, 4, buf) > 0)
			pr_info("tsp: fw. ver. : new.(%.2x), cur.(%.2x)\n",
							fw_version, buf[0]);
		else
			pr_err("tsp: fw. ver. read fail!!\n");
	}

	release_firmware(fw);
out:
	return ret;
}

#define TS_TA_DETECT_REG		0x09

static void set_ta_mode(int *ta_state)
{
	u8 buf;
	struct sec_ts_platform_data *platform_data =
		container_of(ta_state, struct sec_ts_platform_data, ta_state);
	struct ts_data *ts = (struct ts_data *) platform_data->driver_data;

	switch (*ta_state) {
	case CABLE_TYPE_USB:
		buf = 0x00;
		tsp_log("USB cable attached");
		break;
	case CABLE_TYPE_AC:
		buf = 0x01;
		tsp_log("TA attached");
		break;
	case CABLE_TYPE_NONE:
	default:
		buf = 0x00;
		tsp_log("external cable detached");
	}

	if (ts) {
		if (ts_write_reg_data(ts, TS_TA_DETECT_REG, 1, &buf) < 0) {
			pr_err("tsp: %s: set a ta state failed.", __func__);
		}
	}
}

static ssize_t mms136_pivot_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ts_data *ts = dev_get_drvdata(dev);
	int count;

	count = sprintf(buf, "%d\n", ts->platform_data->pivot);
	pr_info("tsp: pivot mode=%d\n", ts->platform_data->pivot);

	return count;
}

ssize_t mms136_pivot_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ts_data *ts = dev_get_drvdata(dev);
	int pivot;

	if (kstrtoint(buf, 0, &pivot))
		pr_err("tsp: failed storing pivot value\n");

	if (pivot < 0) {
		pivot = 0;
	} else if (pivot > 1) {
		pivot = 1;
	}

	if (ts->platform_data->pivot != pivot) {
		swap(ts->platform_data->x_pixel_size,
					ts->platform_data->y_pixel_size);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0,
					ts->platform_data->x_pixel_size, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,
					ts->platform_data->y_pixel_size, 0, 0);

		ts->platform_data->pivot = pivot;
		pr_info("tsp: pivot mode=%d\n", pivot);
	}

	return size;
}

static DEVICE_ATTR(pivot, S_IRUGO | S_IWUSR, mms136_pivot_show, mms136_pivot_store);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_pivot.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

#define TRACKING_COORD			0

#define TS_INPUT_PACKET_SIZE_REG	0x0F
#define TS_INPUT_INFOR_REG		0x10
#define TS_WRONG_RESPONSE		0x0F

static irqreturn_t ts_irq_handler(int irq, void *handle)
{
	struct ts_data *ts = (struct ts_data *)handle;
	int ret = 0, i;
	int event_packet_size, id, x, y;
	u8 buf[6 * MELFAS_MAX_TOUCH] = {0, };
	static u32 cnt;

	if (ts_read_reg_data(ts, TS_INPUT_PACKET_SIZE_REG, 1, buf) < 0) {
		pr_err("tsp: ts_irq_event: Read finger num failed!!\n");
		/* force reset when I2C time out occured. */
		reset_tsp(ts);
		return IRQ_HANDLED;
	}

	event_packet_size = (int) buf[0];
#if TRACKING_COORD
	pr_info("tsp: event_packet_size: %.2x", event_packet_size);
#endif
	if (event_packet_size <= 0 ||
	    event_packet_size > MELFAS_MAX_TOUCH * 6) {
		pr_err("tsp: Ghost IRQ.");
		return IRQ_HANDLED;
	}
	ret = ts_read_reg_data(ts, TS_INPUT_INFOR_REG, event_packet_size, buf);
	if (ret < 0 || buf[0] == TS_WRONG_RESPONSE || buf[0] == 0) {
		reset_tsp(ts);
		return IRQ_HANDLED;
	}

	for (i = 0; i < event_packet_size; i += 6) {
		id = (buf[i] & 0x0F) - 1;
		x = (buf[i + 1] & 0x0F) << 8 | buf[i + 2];
		y = (buf[i + 1] & 0xF0) << 4 | buf[i + 3];

		if (ts->platform_data->pivot) {
			swap(x, y);
			x = ts->platform_data->x_pixel_size - x;
		}

		if (id < 0 || id >= MELFAS_MAX_TOUCH ||
		    x < 0 || x > ts->platform_data->x_pixel_size ||
		    y < 0 || y > ts->platform_data->y_pixel_size) {
			pr_err("tsp: abnormal touch data inputed.\n");
			reset_tsp(ts);
			return IRQ_HANDLED;
		}

		if ((buf[i] & 0x80) == 0) {
			cnt--;
#if TRACKING_COORD
			pr_debug("tsp: finger %d up (%d, %d)\n", id, x, y);
#else
			pr_debug("tsp: finger %d up remain: %d", id, cnt);
#endif
			input_mt_slot(ts->input_dev, id);
			input_mt_report_slot_state(ts->input_dev,
							MT_TOOL_FINGER, false);
			input_sync(ts->input_dev);

			ts->finger_state[id] = 0;
			continue;
		}

		input_mt_slot(ts->input_dev, id);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, buf[i + 4]);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, buf[i + 5]);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);

		input_sync(ts->input_dev);

		if (ts->finger_state[id] == 0) {
			ts->finger_state[id] = 1;
			cnt++;
#if TRACKING_COORD
			pr_debug("tsp: finger %d down (%d, %d)\n", id, x, y);
#else
			pr_debug("tsp: finger %d down remain: %d", id, cnt);
#endif
		} else {
#if TRACKING_COORD
			pr_debug("tsp: finger %d move (%d, %d)\n", id, x, y);
#endif
		}
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ts_early_suspend(struct early_suspend *h)
{
	struct ts_data *ts;

	ts = container_of(h, struct ts_data, early_suspend);
	disable_irq(ts->client->irq);
	reset_points(ts);
	ts->platform_data->set_power(false);
}

static void ts_late_resume(struct early_suspend *h)
{
	struct ts_data *ts;

	ts = container_of(h, struct ts_data, early_suspend);
	ts->platform_data->set_power(true);
	mdelay(100);
	init_tsp(ts);
	enable_irq(ts->client->irq);
}
#endif

#define TS_MAX_Z_TOUCH			255
#define TS_MAX_W_TOUCH			100

static int __devinit ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct ts_data *ts;
	int ret = 0, i;
	struct device *fac_dev_ts;

	tsp_log("enter");

	/* Return 1 if adapter supports everything we need, 0 if not. */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("tsp: ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct ts_data), GFP_KERNEL);
	if (unlikely(ts == NULL)) {
		pr_err("tsp: ts_probe: failed to malloc ts_data!!\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);

	ts->platform_data = client->dev.platform_data;
	ts->platform_data->set_ta_mode = set_ta_mode;
	ts->platform_data->driver_data = ts;

	ts->input_dev = input_allocate_device();
	if (unlikely(ts->input_dev == NULL)) {
		pr_err("tsp: ts_probe: Not enough memory\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	if (ts->platform_data->pivot)
		swap(ts->platform_data->x_pixel_size,
					ts->platform_data->y_pixel_size);

	input_mt_init_slots(ts->input_dev, MELFAS_MAX_TOUCH);

	ts->input_dev->name = "melfas_ts";
	__set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0,
			     ts->platform_data->x_pixel_size, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,
			     ts->platform_data->y_pixel_size, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0,
			     TS_MAX_Z_TOUCH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0,
			     TS_MAX_W_TOUCH, 0, 0);

	if (input_register_device(ts->input_dev) < 0) {
		pr_err("tsp: ts_probe: Failed to register input device!!\n");
		ret = -ENOMEM;
		goto err_input_register_device_failed;
	}

	tsp_log("succeed to register input device");

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = ts_early_suspend;
	ts->early_suspend.resume = ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	/* Check to fw. update necessity */
	if (!fw_updater(ts, "normal")) {
		i = 3;
		pr_err("tsp: ts_probe: fw. update failed. retry %d", i);
		while (i--) {
			if (fw_updater(ts, "force"))
				break;
		}
	}

	if (ts->client->irq) {
		tsp_log("trying to request irq: %s-%d",
			ts->client->name, ts->client->irq);
		ret = request_threaded_irq(client->irq, NULL,
					ts_irq_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					ts->client->name, ts);
		if (ret > 0) {
			pr_err("tsp: probe: Can't register irq %d, ret %d\n",
				ts->client->irq, ret);
			ret = -EBUSY;
			goto err_request_irq;
		}
	}

	fac_dev_ts = device_create(sec_class, NULL, 0, ts, "tsp");
	if (!fac_dev_ts)
		pr_err("tsp: Failed to create factory tsp dev\n");

	if (sysfs_create_group(&fac_dev_ts->kobj, &touchscreen_attr_group))
		pr_err("tsp: Failed to create sysfs (touchscreen_attr_group).\n");

	sema_init(&ts->poll, 1);

	/* To set TA connet mode when boot while keep TA, USB be connected. */
	set_ta_mode(&(ts->platform_data->ta_state));

	pr_info("tsp: ts_probe: Start touchscreen. name: %s, irq: %d\n",
		ts->client->name, ts->client->irq);
	return 0;

err_request_irq:
	pr_err("tsp: ts_probe: err_request_irq failed!!\n");
	free_irq(client->irq, ts);

err_input_register_device_failed:
	pr_err("tsp: ts_probe: err_input_register_device failed!!\n");
	input_unregister_device(ts->input_dev);

err_input_dev_alloc_failed:
	pr_err("tsp:ts_probe: err_input_dev_alloc failed!!\n");
	input_free_device(ts->input_dev);
	kfree(ts);
	return ret;

err_alloc_data_failed:
	pr_err("tsp: ts_probe: err_alloc_data failed!!\n");
	return ret;

err_check_functionality_failed:
	pr_err("tsp: ts_probe: err_check_functionality failed!!\n");
	return ret;
}

static int __devexit ts_remove(struct i2c_client *client)
{
	struct ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static const struct i2c_device_id melfas_ts_id[] = {
	{"melfas_ts", 0},
	{}
};

static struct i2c_driver melfas_ts_driver = {
	.driver = {
		.name = "melfas_ts",
	},
	.id_table = melfas_ts_id,
	.probe = ts_probe,
	.remove = __devexit_p(ts_remove),
};

static int __devinit ts_init(void)
{
	return i2c_add_driver(&melfas_ts_driver);
}

static void __exit ts_exit(void)
{
	i2c_del_driver(&melfas_ts_driver);
}

MODULE_DESCRIPTION("Driver for Melfas MMS-136 Touchscreen Controller");
MODULE_AUTHOR("John Park <lomu.park@samsung.com>");
MODULE_LICENSE("GPL");

module_init(ts_init);
module_exit(ts_exit);

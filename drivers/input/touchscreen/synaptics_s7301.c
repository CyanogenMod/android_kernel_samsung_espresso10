/* drivers/input/touchscreen/synaptics_s7301.c
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

#define DEBUG_PRINT		0

#if DEBUG_PRINT
#define	tsp_debug(fmt, args...) \
				pr_info("tsp: %s: " fmt, __func__, ## args)
#else
#define tsp_debug(fmt, args...)
#endif

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/platform_data/sec_ts.h>
#include <linux/touchscreen/synaptics.h>
#include <linux/battery.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

#include "../../../arch/arm/mach-omap2/board-espresso.h"

#define MAX_TOUCH_NUM			10

struct ts_data {
	struct i2c_client		*client;
	struct input_dev		*input_dev;
	struct early_suspend		early_suspend;
	struct sec_ts_platform_data	*platform_data;
	struct synaptics_fw_info	*fw_info;
	int				finger_state[MAX_TOUCH_NUM];
};

static int ts_read_reg_data(const struct i2c_client *client, u8 address,
			u8 *buf, u8 size)
{
	int ret = 0;

	if (size > 32) {
		pr_err("tsp: %s: data size: %d, SMBus allows at most 32 bytes.",
								__func__, size);
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, address, size, buf);
	if (ret < size) {
		pr_err("tsp: %s: i2c read failed. %d", __func__, ret);
		return ret;
	}
	return 1;
}

static void set_ta_mode(int *ta_state)
{
	struct sec_ts_platform_data *platform_data =
	container_of(ta_state, struct sec_ts_platform_data, ta_state);
	struct ts_data *ts = (struct ts_data *) platform_data->driver_data;

	if (ts) {
		switch (*ta_state) {
		case CABLE_TYPE_AC:
		F01_SetTABit(ts->client, true);
		pr_info("tsp: TA attached\n");
		break;
		case CABLE_TYPE_USB:
		F01_SetTABit(ts->client, false);
		pr_info("tsp: USB attached\n");
		break;
		case CABLE_TYPE_NONE:
		default:
		F01_SetTABit(ts->client, false);
		pr_info("tsp: No attached cable\n");
		break;
		}
	}
}

#define FW_ADDRESS			0x34

static u8 get_reg_address(const struct i2c_client *client, const int reg_name)
{
	u8 ret = 0;
	u8 address;
	u8 buffer[6];

	for (address = 0xE9; address > 0xD0; address -= 6) {
		ts_read_reg_data(client, address, buffer, 6);

		if (buffer[5] == 0)
			break;
		switch (buffer[5]) {
		case FW_ADDRESS:
			ret = buffer[2];
			break;
		}
	}

	return ret;
}

static bool fw_updater(struct ts_data *ts, char *mode)
{
	u8 buf[5] = {0, };
	bool ret = false;
	const struct firmware *fw;

	tsp_debug("Enter the fw_updater.");

	/* To check whether touch IC in bootloader mode.
	 * It means that fw. update failed at previous booting.
	 */
	if (ts_read_reg_data(ts->client, 0x14, buf, 1) > 0) {
		if (buf[0] == 0x01)
			mode = "force";
	}

	if (!ts->platform_data->fw_name) {
		pr_err("tsp: can't find firmware file name.");
		return false;
	}

	if (request_firmware(&fw, ts->platform_data->fw_name,
							&ts->client->dev)) {
		pr_err("tsp: fail to request built-in firmware\n");
		goto out;
	}

	ts->fw_info->version[0] = fw->data[0xb100];
	ts->fw_info->version[1] = fw->data[0xb101];
	ts->fw_info->version[2] = fw->data[0xb102];
	ts->fw_info->version[3] = fw->data[0xb103];
	ts->fw_info->version[4] = 0;

	if (!strcmp("force", mode)) {
		pr_info("tsp: fw_updater: force upload.\n");
		ret = synaptics_fw_update(ts->client, fw->data,
						ts->platform_data->gpio_irq);
	} else if (!strcmp("normal", mode)) {
		if (ts_read_reg_data(ts->client,
			get_reg_address(ts->client, FW_ADDRESS), buf, 4) < 0) {
			pr_err("tsp: fw. ver. read failed.");
			goto out;
		}

		pr_info("tsp: binary fw. ver: 0x%s, IC fw. ver: 0x%s\n",
						(char *)ts->fw_info->version,
						(char *)buf);

		if (strncmp(ts->fw_info->version, buf, 4) > 0) {
			pr_info("tsp: fw_updater: FW upgrade enter.\n");
			ret = synaptics_fw_update(ts->client, fw->data,
						ts->platform_data->gpio_irq);
		} else {
			pr_info("tsp: fw_updater: No need FW update.\n");
			ret = true;
		}
	}
out:
	release_firmware(fw);
	return ret;
}

static void reset_points(struct ts_data *ts)
{
	int i;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		ts->finger_state[i] = 0;
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
					false);
	}
	input_sync(ts->input_dev);

	tsp_debug("reset_all_fingers.");
}

#define REG_INTERRUPT_STATUS		0x14

static void init_tsp(struct ts_data *ts)
{
	u8 buf;

	reset_points(ts);

	/* To high interrupt pin */
	if (ts_read_reg_data(ts->client, REG_INTERRUPT_STATUS, &buf, 1) < 0)
		pr_err("tsp: init_tsp: read reg_data failed.");

	set_ta_mode(&(ts->platform_data->ta_state));

	tsp_debug("init_tsp done.");
}

static void reset_tsp(struct ts_data *ts)
{
	if (ts->platform_data->set_power) {
		ts->platform_data->set_power(false);
		ts->platform_data->set_power(true);
	}
	init_tsp(ts);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ts_early_suspend(struct early_suspend *h)
{
	struct ts_data *ts;

	ts = container_of(h, struct ts_data, early_suspend);
	disable_irq(ts->client->irq);
	reset_points(ts);
	if (ts->platform_data->set_power)
		ts->platform_data->set_power(false);
}

static void ts_late_resume(struct early_suspend *h)
{
	struct ts_data *ts;
	u8 buf[2];

	ts = container_of(h, struct ts_data, early_suspend);
	if (ts->platform_data->set_power)
		ts->platform_data->set_power(true);

	init_tsp(ts);

	ts_read_reg_data(ts->client, REG_INTERRUPT_STATUS, buf, 1);
	enable_irq(ts->client->irq);
}
#endif

#define TRACKING_COORD			0

#define REG_DEVICE_STATUS		0x13
#define REG_FINGER_STATUS		0x15
#define REG_POINT_INFO			0x18

static irqreturn_t ts_irq_handler(int irq, void *handle)
{
	struct ts_data *ts = (struct ts_data *)handle;

	int i, j;
	int cur_state, id;
#if TRACKING_COORD
	static u32 cnt;
#endif
	u16 x, y;
	u8 buf;
	u8 state[3] = {0, };
	u8 point[5] = {0, };

	if (ts_read_reg_data(ts->client, REG_DEVICE_STATUS, &buf, 1) < 0) {
		pr_err("tsp: ts_irq_event: i2c failed\n");
		return IRQ_HANDLED;
	}

	if ((buf & 0x0F) == 0x03) {
		pr_err("tsp: ts_irq_event: esd detect\n");
		reset_tsp(ts);
		return IRQ_HANDLED;
	}

	if (ts_read_reg_data(ts->client, REG_FINGER_STATUS, state, 3) < 0) {
		pr_err("tsp: ts_irq_event: i2c failed\n");
		return IRQ_HANDLED;
	}
#if TRACKING_COORD
	pr_info("tsp: finger state regigster %.2x, %.2x, %.2x\n",
						state[0], state[1], state[2]);
#endif
	for (i = 0, id = 0; i < 3; i++) {
		for (j = 0; j < 4 && id < MAX_TOUCH_NUM; j++, id++) {
			/* check the new finger state */
			cur_state = ((state[i] >> (2*j)) & 0x01);

			if (cur_state == 0 && ts->finger_state[id] == 0)
				continue;

			if (ts_read_reg_data(ts->client,
				(REG_POINT_INFO + (id * 5)), point, 5) < 0) {
				pr_err("tsp: read_points: read point failed\n");
				return IRQ_HANDLED;
			}

			x = (point[0] << 4) + (point[2] & 0x0F);
			y = (point[1] << 4) + ((point[2] & 0xF0) >> 4);

			if (ts->platform_data->pivot) {
				swap(x, y);
				x = ts->platform_data->x_pixel_size - x;
			}

			if (cur_state == 0 && ts->finger_state[id] == 1) {
#if TRACKING_COORD
				tsp_debug("%d up (%d, %d, %d)\n",
							id, x, y, point[4]);
#else
				tsp_debug("%d up. remain: %d\n", id, --cnt);
#endif
				input_mt_slot(ts->input_dev, id);
				input_mt_report_slot_state(ts->input_dev,
							MT_TOOL_FINGER, false);
				input_sync(ts->input_dev);

				ts->finger_state[id] = 0;
				continue;
			}

			if (cur_state == 1 && ts->finger_state[id] == 0) {
#if TRACKING_COORD
				tsp_debug("%d dn (%d, %d, %d)\n",
							id, x, y, point[4]);
#else
				tsp_debug("%d dn. remain: %d\n", id, ++cnt);
#endif
				ts->finger_state[id] = 1;
			}

			input_mt_slot(ts->input_dev, id);
			input_mt_report_slot_state(ts->input_dev,
						MT_TOOL_FINGER, true);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
						point[3]);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
						point[4]);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
			input_sync(ts->input_dev);
#if TRACKING_COORD
			tsp_debug("tsp: %d drag (%d, %d, %d)\n",
							id, x, y, point[4]);
#endif
		}
	}

	/* to high interrupt pin */
	if (ts_read_reg_data(ts->client, REG_INTERRUPT_STATUS, state, 1) < 0) {
		pr_err("tsp: ts_irq_event: i2c failed\n");
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

#define TS_MAX_Z_TOUCH			255
#define TS_MAX_W_TOUCH			100

static int __devinit ts_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct ts_data *ts;
	int ret = 0;

	pr_info("tsp: ts_probe\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("tsp: ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct ts_data), GFP_KERNEL);
	if (unlikely(ts == NULL)) {
		pr_err("tsp: ts_probe: failed to create a ts_data.\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->platform_data = client->dev.platform_data;
	ts->platform_data->set_ta_mode = set_ta_mode;
	ts->platform_data->driver_data = ts;

	ts->fw_info = (struct synaptics_fw_info *)ts->platform_data->fw_info;

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		pr_err("tsp: ts_probe: failed to input_allocate_device.\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	input_mt_init_slots(ts->input_dev, MAX_TOUCH_NUM);

	ts->input_dev->name = SEC_TS_NAME;
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

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

	tsp_debug("succeed to register input device\n");

	/* Power on touch IC */
	if (ts->platform_data->set_power)
		ts->platform_data->set_power(true);

	/* Check the new fw. and update */
	fw_updater(ts, "normal");

	if (ts->client->irq) {
		tsp_debug("trying to request irq: %s %d\n", ts->client->name,
							ts->client->irq);

		ret = request_threaded_irq(ts->client->irq, NULL,
					 ts_irq_handler,
					 IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					 ts->client->name, ts);
		if (ret > 0) {
			pr_err("tsp: ts_probe: Can't register irq %d, ret %d\n",
				ts->client->irq, ret);
			ret = -EBUSY;
			goto err_request_irq;
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 2;
	ts->early_suspend.suspend = ts_early_suspend;
	ts->early_suspend.resume = ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	pr_info("tsp: ts_probe: Start touchscreen. name: %s, irq: %d\n",
					ts->client->name, ts->client->irq);

	return 0;

err_request_irq:
	pr_err("tsp: ts_probe: err_request_irq failed.\n");
	free_irq(client->irq, ts);

err_input_register_device_failed:
	pr_err("tsp: ts_probe: err_input_register_device failed.\n");
	input_unregister_device(ts->input_dev);

err_input_dev_alloc_failed:
	pr_err("tsp:ts_probe: err_input_dev_alloc failed.\n");
	input_free_device(ts->input_dev);
	kfree(ts);
	return ret;

err_alloc_data_failed:
	pr_err("tsp: ts_probe: err_alloc_data failed.\n");
	return ret;

err_check_functionality_failed:
	pr_err("tsp: ts_probe: err_check_functionality failed.\n");
	return ret;
}

static int ts_remove(struct i2c_client *client)
{
	struct ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	kfree(ts);

	return 0;
}

static void ts_shutdown(struct i2c_client *client)
{
	struct ts_data *ts = i2c_get_clientdata(client);

	disable_irq(client->irq);
	reset_points(ts);
	if (ts->platform_data->set_power)
		ts->platform_data->set_power(false);
}

static const struct i2c_device_id ts_id[] = {
	{"synaptics_ts", 0},
	{}
};

static struct i2c_driver ts_driver = {
	.driver = {
		.name = "synaptics_ts",
	},
	.id_table = ts_id,
	.probe = ts_probe,
	.remove = __devexit_p(ts_remove),
	.shutdown = ts_shutdown,
};

static int __devinit ts_init(void)
{
	return i2c_add_driver(&ts_driver);
}

static void __exit ts_exit(void)
{
	i2c_del_driver(&ts_driver);
}

MODULE_DESCRIPTION("Driver for Synaptics S7301 Touchscreen Controller");
MODULE_AUTHOR("John Park <lomu.park@samsung.com>");
MODULE_LICENSE("GPL");

module_init(ts_init);
module_exit(ts_exit);

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

#define DEBUG_PRINT			1

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
#include <linux/io.h>
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

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
#define TSP_VENDOR			"MELFAS"
#define TSP_IC				"MMS-136"

#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN		512
#define TSP_CMD_PARAM_NUM		8

struct factory_data {
	struct list_head	cmd_list_head;
	u8			cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	char			cmd_buff[TSP_CMD_RESULT_STR_LEN];
	struct mutex		cmd_lock;
	bool			cmd_is_running;
};

struct node_data {
	s16			*cm_delta_data;
	s16			*cm_abs_data;
	s16			*intensity_data;
	s16			*reference_data;
};

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func
#define TOSTRING(x) #x

enum {	/* this is using by cmd_state valiable. */
	WAITING = 0,
	RUNNING,
	OK,
	FAIL,
	NOT_APPLICABLE,
};

struct tsp_cmd {
	struct list_head	list;
	const char		*cmd_name;
	void			(*cmd_func)(void *device_data);
};
#endif

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
#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	struct factory_data	*factory_data;
	struct node_data	*node_data;
#endif
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
	if (ts->platform_data->set_dvfs)
		ts->platform_data->set_dvfs(false);
	tsp_log("reset_all_fingers");
	return;
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
	return;
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

	} else if (!strcmp("file", mode)) {
		long fw_size;
		u8 *fw_data;
		struct file *filp;
		mm_segment_t oldfs;
		char file_name[20] = "/sdcard/mms136.bin";

		tsp_log("force upload from external file.");

		oldfs = get_fs();
		set_fs(KERNEL_DS);

		filp = filp_open(file_name, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(filp)) {
			pr_err("tsp: file open error:%d\n", (s32)filp);
			ret = false;
			goto out;
		}

		fw_size = filp->f_path.dentry->d_inode->i_size;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		if (!fw_data) {
			pr_err("tsp: failed to allocation memory.");
			filp_close(filp, current->files);
			ret = false;
			goto out;
		}

		if (vfs_read(filp, (char __user *)fw_data, fw_size,
						&filp->f_pos) != fw_size) {
			pr_err("tsp: failed to read file (ret = %d).", ret);
			filp_close(filp, current->files);
			kfree(fw_data);
			ret = false;
			goto out;
		}

		filp_close(filp, current->files);
		set_fs(oldfs);

		ret = isp_updater(fw_data, (size_t)fw_size, ts->platform_data);
		kfree(fw_data);

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
			return;
		}
	}

	return;
}

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
enum {
	CM_DELTA	= 0x02,
	CM_ABS		= 0x03,
	INTENSITY_DATA	= 0x04,
	RAW_DATA	= 0x06,
	REFERENCE_DATA	= 0x07
};

#define TS_VENDER_COMMAND_ID		0xB0
#define TS_VENDER_COMMAND_RESULT	0xBF

static void set_node_data(struct ts_data *ts_data, const u8 data_type,
						int *max_value, int *min_value)
{
	u8 writebuf[5] = {0, }, readbuf[5] = {0, }, itr_gpio_value;
	u32 x, y;
	int temp = 0;
	const u32 rx = ts_data->platform_data->rx_channel_no;
	const u32 tx = ts_data->platform_data->tx_channel_no;

	disable_irq(ts_data->client->irq);

	writebuf[0] = 0x1A; /* Samsung TSP Standardization Mode */
	writebuf[4] = 0x01; /* Enter the Test Mode */

	if (data_type == CM_DELTA || data_type == CM_ABS) {
		if (ts_write_reg_data(ts_data, TS_VENDER_COMMAND_ID,
							5, writebuf) < 0) {
			pr_err("tsp factory: i2c communication failed");
			goto out;
		}

		do {
			/* wait TSP IC into test mode
			 * INTR would be Low,
			 * when Touch IC is entered the Test Mode.
			 */
			itr_gpio_value = gpio_get_value(
					ts_data->platform_data->gpio_irq);
			udelay(100);
		} while (itr_gpio_value);
	}

	writebuf[4] = data_type;
	for (x = 0; x < rx; x++) {
		for (y = 0; y < tx; y++) {
			writebuf[1] = y;
			writebuf[2] = x;
			if (ts_write_reg_data(ts_data, TS_VENDER_COMMAND_ID,
					ARRAY_SIZE(writebuf), writebuf) < 0) {
				pr_err("tsp factory: i2c communication failed");
				goto out;
			}
			if (ts_read_reg_data(ts_data, TS_VENDER_COMMAND_RESULT,
					2, readbuf) < 0) {
				pr_err("tsp factory: i2c communication failed");
				goto out;
			}

			switch (data_type) {
			case CM_DELTA:
			temp = ts_data->node_data->cm_delta_data[x * tx + y] =
					((s16)readbuf[1] << 8) | readbuf[0];
			if (x == 0 && y == 0)
				*max_value = *min_value = temp;

			tsp_log("cm_delta: rx %d tx %d value %d", x, y,
				ts_data->node_data->cm_delta_data[x * tx + y]);
			break;

			case CM_ABS:
			temp = ts_data->node_data->cm_abs_data[x * tx + y] =
					((s16)readbuf[1] << 8) | readbuf[0];
			if (x == 0 && y == 0)
				*max_value = *min_value = temp;

			tsp_log("cm_abs: rx %d tx %d value %d", x, y,
				ts_data->node_data->cm_abs_data[x * tx + y]);
			break;

			case INTENSITY_DATA:
			temp = ts_data->node_data->intensity_data[x * tx + y] =
					(s8)readbuf[0];
			if (x == 0 && y == 0)
				*max_value = *min_value = temp;

			tsp_log("intensity: rx %d tx %d value %d", x, y,
				ts_data->node_data->intensity_data[x * tx + y]);
			break;

			case REFERENCE_DATA:
			temp = ts_data->node_data->reference_data[x * tx + y] =
					((s16)readbuf[1] << 8) | readbuf[0];
			if (x == 0 && y == 0)
				*max_value = *min_value = temp;

			tsp_log("reference: rx %d tx %d value %d", x, y,
				ts_data->node_data->reference_data[x * tx + y]);
			break;

			default:
			;
			}
			*max_value = max(*max_value, temp);
			*min_value = min(*min_value, temp);
		}
	}
out:
	reset_tsp(ts_data); /* Return to normal mode */

	enable_irq(ts_data->client->irq);

	return;
}

static void set_default_result(struct factory_data *data)
{
	char delim = ':';

	memset(data->cmd_result, 0x00, ARRAY_SIZE(data->cmd_result));
	memset(data->cmd_buff, 0x00, ARRAY_SIZE(data->cmd_buff));
	memcpy(data->cmd_result, data->cmd, strlen(data->cmd));
	strncat(data->cmd_result, &delim, 1);
}

static void set_cmd_result(struct factory_data *data, char *buff, int len)
{
	strncat(data->cmd_result, buff, len);
}

static void not_support_cmd(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", "NA");
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = NOT_APPLICABLE;
	pr_info("tsp factory : %s: \"%s(%d)\"\n", __func__,
				data->cmd_buff,	strlen(data->cmd_buff));
	return;
}

static void fw_update(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	disable_irq(ts_data->client->irq);

	if (data->cmd_param[0] == 1) {
		if (!fw_updater(ts_data, "file"))
			data->cmd_state = FAIL;
		else
			data->cmd_state = OK;
	} else {
		if (!fw_updater(ts_data, "force"))
			data->cmd_state = FAIL;
		else
			data->cmd_state = OK;
	}

	enable_irq(ts_data->client->irq);

	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	const u32 fw_version = *(u32 *)ts_data->platform_data->fw_info;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%.2x", fw_version);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	u8 buf[2];

	data->cmd_state = RUNNING;

	if (ts_read_reg_data(ts_data, TS_READ_VERSION_ADDR, 1, buf) < 0) {
		pr_err("tsp: i2c read data failed.");
		data->cmd_state = FAIL;
		return;
	}

	set_default_result(data);
	sprintf(data->cmd_buff, "%.2x", buf[0]);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

#define TS_READ_FW_DATE			0xC6

static void get_config_ver(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	u8 buf[20] = {0, };

	data->cmd_state = RUNNING;

	if (ts_read_reg_data(ts_data, TS_READ_FW_DATE, 4, buf) < 0) {
		pr_err("tsp: i2c read data failed.");
		data->cmd_state = FAIL;
		return;
	}

	set_default_result(data);
	sprintf(data->cmd_buff, "%s_%d%d%d%d",
				TSP_VENDOR,
				buf[0], buf[1], buf[2], buf[3]);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

#define TS_READ_THRESHOLD		0x05

static void get_threshold(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	u8 buf[1];

	data->cmd_state = RUNNING;

	if (ts_read_reg_data(ts_data, TS_READ_THRESHOLD, 1, buf) < 0) {
		pr_err("tsp: i2c read data failed.");
		data->cmd_state = FAIL;
		return;
	}

	set_default_result(data);
	sprintf(data->cmd_buff, "%.3u", buf[0]);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_panel_vendor(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", ts_data->platform_data->panel_name);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void module_off_master(void *device_data)
{

}

static void module_on_master(void *device_data)
{

}

static void get_chip_vendor(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", TSP_VENDOR);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_chip_name(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", TSP_IC);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_x_num(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%d", ts_data->platform_data->rx_channel_no);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_y_num(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%d", ts_data->platform_data->tx_channel_no);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_reference(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	const u32 tx_channel_no = ts_data->platform_data->tx_channel_no;
	u32 buf, rx, tx;

	data->cmd_state = RUNNING;
	rx = data->cmd_param[0];
	tx = data->cmd_param[1];

	if (tx > ts_data->platform_data->tx_channel_no ||
	    rx > ts_data->platform_data->rx_channel_no) {
		pr_err("tsp factory: param data is abnormal.\n");
		data->cmd_state = FAIL;
		return;
	}

	buf = ts_data->node_data->reference_data[rx * tx_channel_no + tx];
	set_default_result(data);
	sprintf(data->cmd_buff, "%d", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_cm_abs(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	const u32 tx_channel_no = ts_data->platform_data->tx_channel_no;
	u32 buf, rx, tx;

	data->cmd_state = RUNNING;
	rx = data->cmd_param[0];
	tx = data->cmd_param[1];

	if (tx > ts_data->platform_data->tx_channel_no ||
	    rx > ts_data->platform_data->rx_channel_no) {
		pr_err("tsp factory: param data is abnormal.\n");
		data->cmd_state = FAIL;
		return;
	}

	buf = ts_data->node_data->cm_abs_data[rx * tx_channel_no + tx];
	set_default_result(data);
	sprintf(data->cmd_buff, "%d", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_cm_delta(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	const u32 tx_channel_no = ts_data->platform_data->tx_channel_no;
	u32 buf, rx, tx;

	data->cmd_state = RUNNING;
	rx = data->cmd_param[0];
	tx = data->cmd_param[1];

	if (tx > ts_data->platform_data->tx_channel_no ||
	    rx > ts_data->platform_data->rx_channel_no) {
		pr_err("tsp factory: param data is abnormal.\n");
		data->cmd_state = FAIL;
		return;
	}

	buf = ts_data->node_data->cm_delta_data[rx * tx_channel_no + tx];
	set_default_result(data);
	sprintf(data->cmd_buff, "%d", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_intensity(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	const u32 tx_channel_no = ts_data->platform_data->tx_channel_no;
	u32 buf, rx, tx;

	data->cmd_state = RUNNING;
	rx = data->cmd_param[0];
	tx = data->cmd_param[1];

	if (tx > ts_data->platform_data->tx_channel_no ||
	    rx > ts_data->platform_data->rx_channel_no) {
		pr_err("tsp factory: param data is abnormal.\n");
		data->cmd_state = FAIL;
		return;
	}

	buf = ts_data->node_data->intensity_data[rx * tx_channel_no + tx];
	set_default_result(data);
	sprintf(data->cmd_buff, "%d", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void run_reference_read(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	int max_value, min_value;

	data->cmd_state = RUNNING;

	set_node_data(ts_data, REFERENCE_DATA, &max_value, &min_value);

	set_default_result(data);
	sprintf(data->cmd_buff, "%d,%d", min_value, max_value);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = OK;
	return;
}

#define UNIVERSAL_CMD_ID			0xA0
#define UNIVERSAL_CMD_PARAM_1			0xA1
#define UNIVERSAL_CMD_PARAM_2			0xA2
#define UNIVERSAL_CMD_RESULT_SIZE		0xAE
#define UNIVERSAL_CMD_RESULT			0xAF

#define ENTER_TEST_MODE				0x40
#define TEST_CM_ABS				0x43
#define READ_CM_ABS				0x44

static void run_cm_abs_read(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	int temp, x, y, max_value = 0, min_value = 0;
	const int rx = ts_data->platform_data->rx_channel_no;
	const int tx = ts_data->platform_data->tx_channel_no;
	u8 command[4], buf[4] = {0, };
	int itr_gpio_value;

	data->cmd_state = RUNNING;

	disable_irq(ts_data->client->irq);

	command[0] = ENTER_TEST_MODE;

	if (ts_write_reg_data(ts_data, UNIVERSAL_CMD_ID, 1, command) < 0)
		goto fail;
	do {
		/* wait TSP IC into test mode
		 * INTR would be Low,
		 * when Touch IC is entered the Test Mode.
		 */
		itr_gpio_value = gpio_get_value(
				ts_data->platform_data->gpio_irq);
		udelay(100);
	} while (itr_gpio_value);

	command[0] = TEST_CM_ABS;
	if (ts_write_reg_data(ts_data, UNIVERSAL_CMD_ID, 1, command) < 0)
		goto fail;
	do {
		itr_gpio_value = gpio_get_value(
				ts_data->platform_data->gpio_irq);
		udelay(100);
	} while (itr_gpio_value);

	if (ts_read_reg_data(ts_data, UNIVERSAL_CMD_RESULT_SIZE, 1, buf) < 0)
		goto fail;

	for (x = 0; x < rx; x++) {
		for (y = 0; y < tx; y++) {
			command[0] = READ_CM_ABS;
			command[1] = y;
			command[2] = x;
			if (ts_write_reg_data(ts_data, UNIVERSAL_CMD_ID,
							3, command) < 0)
				goto fail;
			do {
				itr_gpio_value = gpio_get_value(
					ts_data->platform_data->gpio_irq);
				udelay(100);
			} while (itr_gpio_value);

			if (ts_read_reg_data(ts_data, UNIVERSAL_CMD_RESULT_SIZE,
							1, buf) < 0)
				goto fail;
			if (ts_read_reg_data(ts_data, UNIVERSAL_CMD_RESULT,
							buf[0], buf) < 0)
				goto fail;

			temp = ts_data->node_data->cm_abs_data[x * tx + y] =
							buf[0] | buf[1] << 8;
			if (x == 0 && y == 0)
				max_value = min_value = temp;

			max_value = max(max_value, temp);
			min_value = min(min_value, temp);
			tsp_log("cm_abs: rx %d tx %d value %d", x, y, temp);
		}
	}
out:
	reset_tsp(ts_data);
	enable_irq(ts_data->client->irq);

	set_default_result(data);
	sprintf(data->cmd_buff, "%d,%d", min_value, max_value);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = OK;
	return;
fail:
	pr_err("tsp: %s: cm_abs read failed.", __func__);
	goto out;
}

static void run_cm_delta_read(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	int max_value, min_value;

	data->cmd_state = RUNNING;

	set_node_data(ts_data, CM_DELTA, &max_value, &min_value);

	set_default_result(data);
	sprintf(data->cmd_buff, "%d,%d", min_value, max_value);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = OK;
	return;
}

static void run_intensity_read(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	int max_value, min_value;

	data->cmd_state = RUNNING;

	set_node_data(ts_data, INTENSITY_DATA, &max_value, &min_value);

	set_default_result(data);
	sprintf(data->cmd_buff, "%d,%d", min_value, max_value);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = OK;
	return;
}

struct tsp_cmd tsp_cmds_mms[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("get_panel_vendor", get_panel_vendor),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("get_cm_abs", get_cm_abs),},
	{TSP_CMD("get_cm_delta", get_cm_delta),},
	{TSP_CMD("get_intensity", get_intensity),},
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("run_cm_abs_read", run_cm_abs_read),},
	{TSP_CMD("run_cm_delta_read", run_cm_delta_read),},
	{TSP_CMD("run_intensity_read", run_intensity_read),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t cmd_store(struct device *dev, struct device_attribute *devattr,
						const char *buf, size_t count)
{
	struct ts_data *ts_data = dev_get_drvdata(dev);
	struct factory_data *data = ts_data->factory_data;
	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0, };
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	if (data == NULL) {
		pr_err("factory_data is NULL.\n");
		goto err_out;
	}

	if (data->cmd_is_running == true) {
		pr_err("tsp cmd: other cmd is running.\n");
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = true;
	mutex_unlock(&data->cmd_lock);

	data->cmd_state = RUNNING;

	for (i = 0; i < ARRAY_SIZE(data->cmd_param); i++)
		data->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(data->cmd, 0x00, ARRAY_SIZE(data->cmd));
	memcpy(data->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &data->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &data->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		do {
			memset(buff, 0x00, ARRAY_SIZE(buff));
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				if (kstrtoint(buff, 10,
					data->cmd_param + param_cnt) < 0)
					break;
				start = cur + 1;
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	pr_info("cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		pr_info("cmd param %d= %d\n", i, data->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(ts_data);

err_out:
	return count;
}

static ssize_t cmd_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ts_data *ts_data = dev_get_drvdata(dev);
	struct factory_data *data = ts_data->factory_data;
	char buff[16];

	pr_info("tsp cmd: status:%d\n", data->cmd_state);

	switch (data->cmd_state) {
	case WAITING:
		sprintf(buff, "%s", TOSTRING(WAITING));
		break;
	case RUNNING:
		sprintf(buff, "%s", TOSTRING(RUNNING));
		break;
	case OK:
		sprintf(buff, "%s", TOSTRING(OK));
		break;
	case FAIL:
		sprintf(buff, "%s", TOSTRING(FAIL));
		break;
	case NOT_APPLICABLE:
		sprintf(buff, "%s", TOSTRING(NOT_APPLICABLE));
		break;
	default:
		sprintf(buff, "%s", TOSTRING(NOT_APPLICABLE));
		break;
	}

	return sprintf(buf, "%s\n", buff);
}

static ssize_t cmd_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ts_data *ts_data = dev_get_drvdata(dev);
	struct factory_data *data = ts_data->factory_data;

	pr_info("tsp factory : tsp cmd: result: \"%s(%d)\"\n",
				data->cmd_result, strlen(data->cmd_result));

	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = false;
	mutex_unlock(&data->cmd_lock);

	data->cmd_state = WAITING;

	return sprintf(buf, "%s\n", data->cmd_result);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, cmd_store);
static DEVICE_ATTR(cmd_status, S_IRUGO, cmd_status_show, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, cmd_result_show, NULL);

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
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_pivot.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};
#endif

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
			pr_info("tsp: finger %d up (%d, %d)\n", id, x, y);
#else
			pr_info("tsp: finger %d up remain: %d", id, cnt);
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
			pr_info("tsp: finger %d down (%d, %d)\n", id, x, y);
#else
			pr_info("tsp: finger %d down remain: %d", id, cnt);
#endif
		} else {
#if TRACKING_COORD
			pr_info("tsp: finger %d move (%d, %d)\n", id, x, y);
#endif
		}
	}

	if (ts->platform_data->set_dvfs)
		ts->platform_data->set_dvfs(!!cnt);

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
	return;
}

static void ts_late_resume(struct early_suspend *h)
{
	struct ts_data *ts;

	ts = container_of(h, struct ts_data, early_suspend);
	ts->platform_data->set_power(true);
	mdelay(100);
	init_tsp(ts);
	enable_irq(ts->client->irq);

	return;
}
#endif

#define TS_MAX_Z_TOUCH			255
#define TS_MAX_W_TOUCH			100

static int __devinit ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct ts_data *ts;
	int ret = 0, i;
#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	struct device *fac_dev_ts;
	struct factory_data *factory_data;
	struct node_data *node_data;
	u32 rx, tx;
#endif
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

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	rx = ts->platform_data->rx_channel_no;
	tx = ts->platform_data->tx_channel_no;

	node_data = kzalloc(sizeof(struct node_data), GFP_KERNEL);
	if (unlikely(node_data == NULL)) {
		ret = -ENOMEM;
		goto err_alloc_node_data_failed;
	}

	node_data->cm_delta_data = kzalloc(sizeof(s16) * rx * tx, GFP_KERNEL);
	node_data->cm_abs_data = kzalloc(sizeof(s16) * rx * tx, GFP_KERNEL);
	node_data->intensity_data = kzalloc(sizeof(s16) * rx * tx, GFP_KERNEL);
	node_data->reference_data = kzalloc(sizeof(s16)  * rx * tx, GFP_KERNEL);
	if (unlikely(node_data->cm_delta_data == NULL ||
				node_data->cm_abs_data == NULL ||
				node_data->intensity_data == NULL ||
				node_data->reference_data == NULL)) {
		ret = -ENOMEM;
		goto err_alloc_node_data_failed;
	}

	factory_data = kzalloc(sizeof(struct factory_data), GFP_KERNEL);
	if (unlikely(factory_data == NULL)) {
		ret = -ENOMEM;
		goto err_alloc_factory_data_failed;
	}

	INIT_LIST_HEAD(&factory_data->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds_mms); i++)
		list_add_tail(&tsp_cmds_mms[i].list, &factory_data->cmd_list_head);

	mutex_init(&factory_data->cmd_lock);
	factory_data->cmd_is_running = false;

	fac_dev_ts = device_create(sec_class, NULL, 0, ts, "tsp");
	if (!fac_dev_ts)
		pr_err("tsp: Failed to create factory tsp dev\n");

	if (sysfs_create_group(&fac_dev_ts->kobj, &touchscreen_attr_group))
		pr_err("tsp: Failed to create sysfs (touchscreen_attr_group).\n");

	ts->factory_data = factory_data;
	ts->node_data = node_data;
#endif
	sema_init(&ts->poll, 1);

	/* To set TA connet mode when boot while keep TA, USB be connected. */
	set_ta_mode(&(ts->platform_data->ta_state));

	pr_info("tsp: ts_probe: Start touchscreen. name: %s, irq: %d\n",
		ts->client->name, ts->client->irq);
	return 0;

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
err_alloc_factory_data_failed:
	pr_err("tsp: ts_probe: err_alloc_factory_data failed.\n");

err_alloc_node_data_failed:
	pr_err("tsp: ts_probe: err_alloc_node_data failed.\n");
	kfree(ts->node_data->reference_data);
	kfree(ts->node_data->intensity_data);
	kfree(ts->node_data->cm_abs_data);
	kfree(ts->node_data->cm_delta_data);
	kfree(ts->node_data);
#endif

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
#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	kfree(ts->node_data->reference_data);
	kfree(ts->node_data->intensity_data);
	kfree(ts->node_data->cm_abs_data);
	kfree(ts->node_data->cm_delta_data);
	kfree(ts->node_data);
	kfree(ts->factory_data);
#endif
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

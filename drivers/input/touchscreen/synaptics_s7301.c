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

#define DEBUG_PRINT			1

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

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

#include "../../../arch/arm/mach-omap2/board-espresso.h"

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
#define TSP_VENDOR			"SYNAPTICS"
#define TSP_IC				"S7301"

#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN		512
#define TSP_CMD_PARAM_NUM		8

struct factory_data {
	struct list_head		cmd_list_head;
	u8				cmd_state;
	char				cmd[TSP_CMD_STR_LEN];
	int				cmd_param[TSP_CMD_PARAM_NUM];
	char				cmd_result[TSP_CMD_RESULT_STR_LEN];
	char				cmd_buff[TSP_CMD_RESULT_STR_LEN];
	struct mutex			cmd_lock;
	bool				cmd_is_running;
};

struct node_data {
	s16				*raw_cap_data;
	s16				*rx_to_rx_data;
	s16				*tx_to_tx_data;
	s16				*tx_to_gnd_data;
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
	struct list_head		list;
	const char			*cmd_name;
	void				(*cmd_func)(void *device_data);
};
#endif

#define MAX_TOUCH_NUM			10

struct ts_data {
	struct i2c_client		*client;
	struct input_dev		*input_dev;
	struct early_suspend		early_suspend;
	struct sec_ts_platform_data	*platform_data;
	struct synaptics_fw_info	*fw_info;
	int				finger_state[MAX_TOUCH_NUM];
#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	struct factory_data		*factory_data;
	struct node_data		*node_data;
#endif
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
		case CABLE_TA:
		F01_SetTABit(ts->client, true);
		pr_info("tsp: TA attached\n");
		break;
		case CABLE_USB:
		F01_SetTABit(ts->client, false);
		pr_info("tsp: USB attached\n");
		break;
		case CABLE_NONE:
		default:
		F01_SetTABit(ts->client, false);
		pr_info("tsp: No attached cable\n");
		break;
		}
	}

	return;
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
	} else if (!strcmp("file", mode)) {
		long fw_size;
		u8 *fw_data;
		struct file *filp;
		mm_segment_t oldfs;

		pr_info("tsp: fw_updater: force upload from external file.");

		oldfs = get_fs();
		set_fs(KERNEL_DS);

		filp = filp_open(ts->platform_data->ext_fw_name, O_RDONLY, 0);
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

		ret = synaptics_fw_update(ts->client, fw_data,
						ts->platform_data->gpio_irq);
		kfree(fw_data);

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

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
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
	struct factory_data *data =
				((struct ts_data *)device_data)->factory_data;
	set_default_result(data);
	sprintf(data->cmd_buff, "%s", "NA");
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = NOT_APPLICABLE;
	pr_info("tsp factory: %s: \"%s(%d)\"\n", __func__,
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

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", ts_data->fw_info->version);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	u8 buf[5] = {0, };

	data->cmd_state = RUNNING;

	ts_read_reg_data(ts_data->client,
			get_reg_address(ts_data->client, FW_ADDRESS), buf, 4);

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_config_ver(void *device_data)
{
	struct ts_data *ts = (struct ts_data *)device_data;
	struct factory_data *data = ts->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s_%s",
					TSP_VENDOR,
					ts->fw_info->release_date);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

#define THRESHOLD_REG			0x60

static void get_threshold(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	u8 buf[3] = {0, };

	data->cmd_state = RUNNING;

	ts_read_reg_data(ts_data->client, THRESHOLD_REG, buf, 1);

	set_default_result(data);
	sprintf(data->cmd_buff, "%.3d", buf[0]);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
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
	sprintf(data->cmd_buff, "%d", ts_data->platform_data->tx_channel_no);
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
	sprintf(data->cmd_buff, "%d", ts_data->platform_data->rx_channel_no);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_raw_cap(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	u32 buf, rx, tx;

	data->cmd_state = RUNNING;
	tx = data->cmd_param[0]; /* X */
	rx = data->cmd_param[1]; /* Y */

	if (tx < 0 || tx > ts_data->platform_data->tx_channel_no ||
	    rx < 0 || rx > ts_data->platform_data->rx_channel_no) {
		pr_err("tsp factory: param data is abnormal.\n");
		data->cmd_state = FAIL;
		return;
	}

	buf = ts_data->node_data->
		raw_cap_data[tx * ts_data->platform_data->rx_channel_no + rx];
	set_default_result(data);
	sprintf(data->cmd_buff, "%d", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_rx_to_rx(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	const int rx_num = ts_data->platform_data->rx_channel_no;
	u32 buf, rx1, rx2;

	data->cmd_state = RUNNING;
	rx1 = data->cmd_param[0]; /* Y */
	rx2 = data->cmd_param[1]; /* Y */

	if (rx1 < 0 || rx1 > rx_num || rx2 < 0 || rx2 > rx_num) {
		pr_err("tsp factory: param data is abnormal.\n");
		data->cmd_state = FAIL;
		return;
	}

	buf = ts_data->node_data->rx_to_rx_data[rx1 * rx_num + rx2];
	set_default_result(data);
	sprintf(data->cmd_buff, "%d", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_tx_to_tx(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	u32 buf, tx;

	data->cmd_state = RUNNING;
	tx = data->cmd_param[0]; /* X */

	if (tx < 0 || tx > ts_data->platform_data->tx_channel_no) {
		pr_err("tsp factory: param data is abnormal.\n");
		data->cmd_state = FAIL;
		return;
	}

	buf = ts_data->node_data->tx_to_tx_data[tx];
	set_default_result(data);
	sprintf(data->cmd_buff, "%d", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_tx_to_gnd(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	u32 buf, tx;

	data->cmd_state = RUNNING;
	tx = data->cmd_param[0]; /* X */

	if (tx < 0 || tx > ts_data->platform_data->tx_channel_no) {
		pr_err("tsp factory: param data is abnormal.\n");
		data->cmd_state = FAIL;
		return;
	}

	buf = ts_data->node_data->tx_to_gnd_data[tx];
	set_default_result(data);
	sprintf(data->cmd_buff, "%d", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void run_raw_cap_read(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	int temp, x, y, max_value, min_value;
	const int rx = ts_data->platform_data->rx_channel_no;
	const int tx = ts_data->platform_data->tx_channel_no;

	data->cmd_state = RUNNING;

	disable_irq(ts_data->client->irq);

	if (!F54_SetRawCapData(ts_data->client,
					ts_data->node_data->raw_cap_data)) {
		data->cmd_state = FAIL;
		return;
	}

	max_value = min_value = ts_data->node_data->raw_cap_data[0];

	for (x = 0; x < tx; x++) {
		for (y = 0; y < rx; y++) {
			temp = ts_data->node_data->raw_cap_data[x * rx + y];
			max_value = max(max_value, temp);
			min_value = min(min_value, temp);

			tsp_debug("%d, %d, data: %d", x, y,
				ts_data->node_data->raw_cap_data[x * rx + y]);
		}
	}

	enable_irq(ts_data->client->irq);

	set_default_result(data);
	sprintf(data->cmd_buff, "%d,%d", min_value, max_value);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = OK;
	return;
}

static void run_rx_to_rx_read(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	int temp, x, y, max_value, min_value;
	const int rx = ts_data->platform_data->rx_channel_no;

	data->cmd_state = RUNNING;

	disable_irq(ts_data->client->irq);

	if (!F54_SetRxToRxData(ts_data->client,
					ts_data->node_data->rx_to_rx_data)) {
		data->cmd_state = FAIL;
		return;
	}

	max_value = min_value = ts_data->node_data->rx_to_rx_data[0];

	for (x = 0; x < rx; x++) {
		for (y = 0; y < rx; y++) {
			temp = ts_data->node_data->rx_to_rx_data[x * rx + y];
			max_value = max(max_value, temp);
			min_value = min(min_value, temp);

			tsp_debug("%d, %d, data: %d", x, y,
				ts_data->node_data->rx_to_rx_data[x * rx + y]);
		}
	}

	enable_irq(ts_data->client->irq);

	set_default_result(data);
	sprintf(data->cmd_buff, "%d,%d", min_value, max_value);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = OK;
	return;
}

#define TX_TO_TX_TEST_MODE		0x05

static void run_tx_to_tx_read(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	int temp, x, max_value, min_value;
	const int tx = ts_data->platform_data->tx_channel_no;

	data->cmd_state = RUNNING;

	disable_irq(ts_data->client->irq);

	if (!F54_TxToTest(ts_data->client, ts_data->node_data->tx_to_tx_data,
							TX_TO_TX_TEST_MODE)) {
		data->cmd_state = FAIL;
		return;
	}

	max_value = min_value = ts_data->node_data->tx_to_tx_data[0];

	for (x = 0; x < tx; x++) {
		temp = ts_data->node_data->tx_to_tx_data[x];
		max_value = max(max_value, temp);
		min_value = min(min_value, temp);

		tsp_debug("%d, data: %d", x,
					ts_data->node_data->tx_to_tx_data[x]);
	}

	enable_irq(ts_data->client->irq);

	set_default_result(data);
	sprintf(data->cmd_buff, "%d,%d", min_value, max_value);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = OK;
	return;
}

#define TX_TO_GND_TEST_MODE		0x10

static void run_tx_to_gnd_read(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	int temp, x, max_value, min_value;
	const int tx = ts_data->platform_data->tx_channel_no;

	data->cmd_state = RUNNING;

	disable_irq(ts_data->client->irq);

	if (!F54_TxToTest(ts_data->client, ts_data->node_data->tx_to_gnd_data,
							TX_TO_GND_TEST_MODE)) {
		data->cmd_state = FAIL;
		return;
	}

	max_value = min_value = ts_data->node_data->tx_to_gnd_data[0];

	for (x = 0; x < tx; x++) {
		temp = ts_data->node_data->tx_to_gnd_data[x];
		max_value = max(max_value, temp);
		min_value = min(min_value, temp);

		tsp_debug("%d, data: %d", x,
					ts_data->node_data->tx_to_gnd_data[x]);
	}

	enable_irq(ts_data->client->irq);

	set_default_result(data);
	sprintf(data->cmd_buff, "%d,%d", min_value, max_value);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = OK;
	return;
}

struct tsp_cmd tsp_cmds_synaptics[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", not_support_cmd),},
	{TSP_CMD("module_on_master", not_support_cmd),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_rawcap", get_raw_cap),},
	{TSP_CMD("get_rx_to_rx", get_rx_to_rx),},
	{TSP_CMD("get_tx_to_tx", get_tx_to_tx),},
	{TSP_CMD("get_tx_to_gnd", get_tx_to_gnd),},
	{TSP_CMD("run_rawcap_read", run_raw_cap_read),},
	{TSP_CMD("run_rx_to_rx_read", run_rx_to_rx_read),},
	{TSP_CMD("run_tx_to_tx_read", run_tx_to_tx_read),},
	{TSP_CMD("run_tx_to_gnd_read", run_tx_to_gnd_read),},
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

	pr_info("tsp factory: tsp cmd: result: \"%s(%d)\"\n",
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

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};
#endif

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
	return;
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
	return;
}

static void reset_tsp(struct ts_data *ts)
{
	if (ts->platform_data->set_power) {
		ts->platform_data->set_power(false);
		ts->platform_data->set_power(true);
	}
	init_tsp(ts);

	return;
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

	return ;
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

	return ;
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
	static u32 cnt;
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
#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	struct device *fac_dev_ts;
	int i;
	struct factory_data *factory_data;
	struct node_data *node_data;
	u32 rx, tx;
#endif

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

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	rx = ts->platform_data->rx_channel_no;
	tx = ts->platform_data->tx_channel_no;

	node_data = kmalloc(sizeof(struct node_data), GFP_KERNEL);
	if (unlikely(node_data == NULL)) {
		ret = -ENOMEM;
		goto err_alloc_node_data_failed;
	}

	node_data->raw_cap_data = kzalloc(sizeof(s16) * tx * rx, GFP_KERNEL);
	node_data->rx_to_rx_data = kzalloc(sizeof(s16) * rx * rx, GFP_KERNEL);
	node_data->tx_to_tx_data = kzalloc(sizeof(s16) * tx, GFP_KERNEL);
	node_data->tx_to_gnd_data = kzalloc(sizeof(s16) * tx, GFP_KERNEL);
	if (unlikely(node_data->raw_cap_data == NULL ||
				node_data->rx_to_rx_data == NULL ||
				node_data->tx_to_tx_data == NULL ||
				node_data->tx_to_gnd_data == NULL)) {
		ret = -ENOMEM;
		goto err_alloc_node_data_failed;
	}

	factory_data = kzalloc(sizeof(struct factory_data), GFP_KERNEL);

	if (unlikely(factory_data == NULL)) {
		pr_err("tsp: ts_probe: failed to create a factory_data.\n");
		ret = -ENOMEM;
		goto err_alloc_factory_data_failed;
	}

	INIT_LIST_HEAD(&factory_data->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds_synaptics); i++)
		list_add_tail(&tsp_cmds_synaptics[i].list, &factory_data->cmd_list_head);

	mutex_init(&factory_data->cmd_lock);
	factory_data->cmd_is_running = false;
	fac_dev_ts = device_create(sec_class, NULL, 0, ts, "tsp");
	if (!fac_dev_ts)
		pr_err("tsp factory: Failed to create fac tsp dev\n");

	if (sysfs_create_group(&fac_dev_ts->kobj, &touchscreen_attr_group))
		pr_err("tsp factory: Failed to create sysfs (touchscreen_attr_group).\n");

	ts->factory_data = factory_data;
	ts->node_data = node_data;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = ts_early_suspend;
	ts->early_suspend.resume = ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	pr_info("tsp: ts_probe: Start touchscreen. name: %s, irq: %d\n",
					ts->client->name, ts->client->irq);

	return 0;

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
err_alloc_factory_data_failed:
	pr_err("tsp: ts_probe: err_alloc_factory_data failed.\n");

err_alloc_node_data_failed:
	pr_err("tsp: ts_probe: err_alloc_node_data failed.\n");
	kfree(ts->node_data->raw_cap_data);
	kfree(ts->node_data->rx_to_rx_data);
	kfree(ts->node_data->tx_to_tx_data);
	kfree(ts->node_data->tx_to_gnd_data);
	kfree(ts->node_data);
#endif
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
#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	kfree(ts->factory_data);
#endif
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

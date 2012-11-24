/* drivers/video/omap2/displays/cmc624.c
*
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/uaccess.h>
#include <video/omapdss.h>
#include <video/cmc624.h>

/*
 * DEFINE
 */
#define CMC624_MAX_SETTINGS	 100
#define SUB_TUNE	0
#define MAIN_TUNE	1
#define BROW_TUNE	2

#define BROWSER_COLOR_TONE_MIN	40
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/tuning/"

#define MAX_BRIGHTNESS_LEVEL	255
#define DEF_BRIGHTNESS_LEVEL	140
#define MIN_BRIGHTNESS_LEVEL	20

#define IS_SCENARIO_COLOR(__val) \
	((__val) >= BROWSER_COLOR_TONE_MIN  && \
	 (__val) < (BROWSER_COLOR_TONE_MIN + COLOR_TONE_MAX))

#define IS_SCENARIO_VALID(_val) \
	(IS_SCENARIO_COLOR(_val) || ((_val) < MAX_mDNIe_MODE && (_val) >= 0))

/*
 * STRUCTURE
 */
struct cmc624_state_type {
	enum tune_menu_cabc cabc_mode;
	unsigned int brightness;
	unsigned int suspended;
	enum tune_menu_app scenario;
	enum tune_menu_tone browser_scenario;
	enum tune_menu_mode background;

	/*This value must reset to 0 (standard value) when change scenario*/
	enum tune_menu_temp temperature;
	enum tune_menu_outdoor outdoor;
	int negative;
};

struct cmc624_info {
	struct device *dev;
	struct i2c_client *client;
	struct cmc624_panel_data *pdata;
	struct cmc624_register_set cmc624_tune_seq[CMC624_MAX_SETTINGS];
	struct class *mdnie_class;
	struct cmc624_state_type state;
	struct mutex tune_lock;

	int init_tune_flag[3];
	unsigned long last_cmc624_Bank;
	unsigned long last_cmc624_Algorithm;
	char tuning_filename[MAX_FILE_NAME];

	enum power_lut_level pwrlut_lev;
};
static struct cmc624_info *g_cmc624_info;

static LIST_HEAD(tune_data_list);
static DEFINE_MUTEX(tune_data_list_mutex);

/*
 * FUNCTIONS
 */

static int cmc624_reg_write(struct cmc624_info *info, u8 reg, u16 value)
{
	int ret;
	struct i2c_client *client;
	u16 temp_h, temp_l;
	u16 val;

	if (!info->client) {
		dev_err(info->dev, "%s: client is NULL\n", __func__);
		return -ENODEV;
	}

	client = info->client;

	if (info->state.suspended) {
		dev_err(info->dev, "skip to set cmc624 if LCD is off(%x,%x)\n",
								reg, value);
		return 0;
	}

	if (reg == 0x0000) {
		if (value == info->last_cmc624_Bank)
			return 0;
		info->last_cmc624_Bank = value;
	} else if (reg == 0x0001)
		info->last_cmc624_Algorithm = value;

	temp_h = (value >> 8) & 0xFF;
	temp_l = (value & 0xFF) << 8;
	val = temp_h | temp_l;
	ret = i2c_smbus_write_word_data(client, reg, val);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err(%d), reg=0x%x,value=0x%x\n",
				__func__, ret, reg, value);
		return ret;
	}

	return 0;
}

static u16 cmc624_reg_read(struct cmc624_info *info, u8 reg)
{
	struct i2c_client *client;
	int value;

	if (!info->client) {
		dev_err(info->dev, "%s: client is NULL\n", __func__);
		return -ENODEV;
	}

	client = info->client;

	if (reg == 0x01) {
		value = info->last_cmc624_Algorithm;
		return 0;
	}

	value = i2c_smbus_read_word_data(client, reg);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	value = ((value & 0xFF) << 8) | (value >> 8);

	return value;
}

static struct tune_data *cmc624_get_tune_data(u32 id, u32 mask)
{
	struct tune_data *data;
	u32 target_id;

	target_id = id & mask;

	list_for_each_entry(data, &tune_data_list, list) {
		if ((data->id & mask) == target_id)
			goto found;
	}

	pr_err("%s: failed to find tune data, id=0x%x, mask=0x%x\n",
							__func__, id, mask);
	return NULL;

found:
	return data;
}

int cmc624_register_tune_data(u32 id, const struct cmc624_register_set *cmc_set,
								u32 tune_size)
{
	struct tune_data *data;

	list_for_each_entry(data, &tune_data_list, list) {
		if (data->id == id)
			goto dupulicate;
	}

	data = kzalloc(sizeof(struct tune_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->id = id;
	data->value = cmc_set;
	data->size = tune_size;

	list_add(&data->list, &tune_data_list);

	return 0;

dupulicate:
	pr_warning("%s: dupulication tune_data register,id=%x\n", __func__, id);
	return -EINVAL;
}
EXPORT_SYMBOL(cmc624_register_tune_data);

static int cmc624_send_cmd(const struct cmc624_register_set *value,
						 unsigned int array_size)
{
	struct cmc624_info *info = g_cmc624_info;
	int i;
	int ret = 0;

	if (!value) {
		dev_err(info->dev, "%s: tune data is NULL!\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->tune_lock);

	for (i = 0; i < array_size; i++) {
		if (value[i].reg_addr == SLEEPMSEC) {
			usleep_range(value[i].data*1000,
						value[i].data*1000+100);
			continue;
		}

		ret = cmc624_reg_write(info, value[i].reg_addr, value[i].data);
		if (ret < 0) {
			dev_err(info->dev, "%s: failed to send cmd(%d)\n",
								__func__, ret);
			goto set_error;
		}
	}

set_error:
	mutex_unlock(&info->tune_lock);
	return ret;
}

void cmc624_set_auto_brightness(enum auto_brt_val auto_brt)
{
	struct cmc624_info *info = g_cmc624_info;

	/* if auto brightness value is 0, turn off cabc */
	if (auto_brt)
		info->state.cabc_mode = MENU_CABC_ON;
	else
		info->state.cabc_mode = MENU_CABC_OFF;

	/* select cmc624 power lut table to control pwm with  cabc-on mode */
	if (auto_brt < AUTO_BRIGHTNESS_VAL4)
		info->pwrlut_lev = PWRLUT_LEV_INDOOR;
	else if (auto_brt == AUTO_BRIGHTNESS_VAL4)
		info->pwrlut_lev = PWRLUT_LEV_OUTDOOR1;
	else
		info->pwrlut_lev = PWRLUT_LEV_OUTDOOR2;
}
EXPORT_SYMBOL(cmc624_set_auto_brightness);

/*
 * CMC624 PWM control
 */

static struct cmc624_register_set pwm_cabcoff[] = {
	{0x00, 0x0001}, /* BANK 1 */
	{0xF8, 0x0011}, /* PWM HIGH ACTIVE, USE REGISTER VALUE */
	{0xF9, },	/* PWM Coujter */

	{0x00, 0x0000}, /* BANK 0 */
	{0xFD, 0xFFFF}, /* MODULE REG MASK RELEASE */
	{0xFE, 0xFFFF}, /* MODULE REG MASK RELEASE */
	{0xFF, 0x0000}, /* MASK RELEASE */
};

static struct cmc624_register_set pwm_cabcon[] = {
	{0x00, 0x0001}, /* BANK 1 */
	{0xF8, 0x0010}, /* PWM HIGH ACTIVE, USE CABC POWER RATE VALUE */
	{0xBB, },	/* POWERLUT_0 */
	{0xBC, },	/* POWERLUT_1 */
	{0xBD, },	/* POWERLUT_2 */
	{0xBE, },	/* POWERLUT_3 */
	{0xBF, },	/* POWERLUT_4 */
	{0xC0, },	/* POWERLUT_5 */
	{0xC1, },	/* POWERLUT_6 */
	{0xC2, },	/* POWERLUT_7 */

	{0x00, 0x0000}, /* BANK 0 */
	{0xFD, 0xFFFF}, /* MODULE REG MASK RELEASE */
	{0xFE, 0xFFFF}, /* MODULE REG MASK RELEASE */
	{0xFF, 0x0000}, /* MASK RELEASE */
};

static u16 cmc624_get_pwm_val(int intensity, u16 max, u16 def, u16 min)
{
	u16 val;

	if (intensity > DEF_BRIGHTNESS_LEVEL)
		val = (intensity - DEF_BRIGHTNESS_LEVEL) * (max - def) /
			(MAX_BRIGHTNESS_LEVEL - DEF_BRIGHTNESS_LEVEL) + def;
	else if (intensity > MIN_BRIGHTNESS_LEVEL)
		val = (intensity - MIN_BRIGHTNESS_LEVEL) * (def - min) /
			(DEF_BRIGHTNESS_LEVEL - MIN_BRIGHTNESS_LEVEL) + min;
	else
		val = intensity * min / MIN_BRIGHTNESS_LEVEL;

	pr_debug("%s: val=%d, intensity=%d, max=%d, def=%d, min=%d\n",
				__func__, val, intensity, max, def, min);

	return val;
}

static int _cmc624_set_pwm(struct cmc624_info *info, int intensity)
{
	enum power_lut_mode mode;
	enum power_lut_level level;
	struct cabcoff_pwm_cnt_tbl *pwm_tbl = info->pdata->pwm_tbl;
	struct cabcon_pwr_lut_tbl *pwr_luts = info->pdata->pwr_luts;

	int i;
	int ret;

	if (info->state.cabc_mode == MENU_CABC_ON) {
		if (info->state.scenario == MENU_APP_UI)
			mode = PWRLUT_MODE_UI;
		else
			mode = PWRLUT_MODE_VIDEO;
		level = info->pwrlut_lev;

		pr_debug("%s: cabc on: intensity=%d\n", __func__, intensity);
		/* set power lut value for cabc-on pwm */
		for (i = 0; i < NUM_PWRLUT_REG; i++) {
			pwm_cabcon[i + 2].data = cmc624_get_pwm_val(intensity,
						pwr_luts->max[level][mode][i],
						pwr_luts->def[level][mode][i],
						pwr_luts->min[level][mode][i]);
			pr_debug("pwr lut=%d\n", pwm_cabcon[i + 2].data);
		}

		ret = cmc624_send_cmd(pwm_cabcon, ARRAY_SIZE(pwm_cabcon));
	} else {
		/* set pwm counter value for cabc-off pwm */
		pwm_cabcoff[2].data = cmc624_get_pwm_val(intensity,
				pwm_tbl->max, pwm_tbl->def, pwm_tbl->min);
		pr_debug("%s: cabc off: intensity=%d,  pwm cnt=%d\n",
				__func__, intensity, pwm_cabcoff[2].data);

		ret = cmc624_send_cmd(pwm_cabcoff, ARRAY_SIZE(pwm_cabcoff));
	}

	if (unlikely(ret < 0)) {
		pr_err("%s: failed to set brightness(%d)\n", __func__, ret);
		return ret;
	}

	return 0;
}

int cmc624_set_pwm(int intensity)
{
	struct cmc624_info *info = g_cmc624_info;
	return _cmc624_set_pwm(info, intensity);
}
EXPORT_SYMBOL(cmc624_set_pwm);

static int cmc624_set_tune_value(u32 id, u32 mask)
{
	struct tune_data *data;
	int ret;

	/* get tune_data */
	data = cmc624_get_tune_data(id, mask);
	if (!data) {
		pr_err("%s: failed to get cmc624 data, id=%d, mask=0x%x\n",
							__func__, id, mask);
		return -EINVAL;
	}

	/* set tune_value */
	ret = cmc624_send_cmd(data->value, data->size);
	if (ret < 0)
		return ret;

	return 0;
}

static int apply_sub_tune_value(struct cmc624_info *info,
				enum tune_menu_temp temp,
				enum tune_menu_outdoor outdoor)
{
	u32 id;
	int ret;

	if ((info->state.temperature == temp) &&
		(info->state.outdoor == outdoor)) {
		dev_info(info->dev, "%s: dupulication setting(%d, %d)\n",
						__func__, temp, outdoor);
		return 0;
	}

	if (info->state.negative)
		goto skip_tune;

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_SKIP,
		temp, outdoor, MENU_SKIP, MENU_SKIP);
	ret = cmc624_set_tune_value(id, BITMASK_SUB_TUNE);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to set sub tune value\n",
								__func__);
		return ret;
	}

skip_tune:
	info->state.temperature = temp;
	info->state.outdoor = outdoor;

	return 0;
}

static int apply_main_tune_value(struct cmc624_info *info,
				enum tune_menu_mode bg, enum tune_menu_app ui,
				int force)
{
	enum tune_menu_cabc cabc = info->state.cabc_mode;
	u32 id;
	int ret;

	if ((!force) && (info->state.scenario == ui) &&
			(info->state.background == bg)) {
		dev_info(info->dev, "%s: dupulication setting(%d, %d)\n",
							__func__, ui, bg);
		return 0;
	}

	if (info->state.scenario == MENU_APP_BROWSER)
		ui = MENU_APP_UI;

	if (info->state.negative)
		goto skip_main_tune;

	TUNE_DATA_ID(id, MENU_CMD_TUNE, bg, ui, MENU_SKIP, MENU_SKIP,
		cabc, MENU_SKIP);
	ret = cmc624_set_tune_value(id, BITMASK_MAIN_TUNE);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to set main tune\n", __func__);
		return ret;
	}

skip_main_tune:
	info->state.background = bg;
	info->state.scenario = ui;

	return 0;
}

static int apply_tune_value(struct cmc624_info *info,
		enum tune_menu_mode bg, enum tune_menu_app ui, int force)
{
	enum tune_menu_temp temp;
	enum tune_menu_outdoor outdoor;
	int app;
	int ret;

	/* set main tune */
	ret = apply_main_tune_value(info, bg, ui, force);
	if (ret < 0) {
		dev_err(info->dev, "%s: failed to set main tune\n", __func__);
		return ret;
	}

	/* set sub tune */
	if ((ui == MENU_APP_VIDEO_WARM) || (ui == MENU_APP_VIDEO_COLD) ||
			(ui == MENU_APP_VIDEO) || (ui == MENU_APP_DMB)) {
		app = ui;
		switch (app) {
		case MENU_APP_VIDEO_WARM:
			temp = MENU_TEMP_WARM;
			break;
		case MENU_APP_VIDEO_COLD:
			temp = MENU_TEMP_COLD;
			break;
		default:
			temp = MENU_TEMP_NORMAL;
			break;
		}
		outdoor = info->state.outdoor;

		ret = apply_sub_tune_value(info, temp, outdoor);
		if (ret < 0) {
			dev_err(info->dev, "%s: failed to set sub tune\n",
								__func__);
			return ret;
		}
	}

	return 0;
}

static int apply_browser_tune_value(struct cmc624_info *info,
				enum tune_menu_tone browser_mode, int force)
{
	u32 id;
	int ret;

	if (info->state.negative)
		goto skip_tune;

	if (!info->init_tune_flag[BROW_TUNE]) {
		info->init_tune_flag[MAIN_TUNE] = 0;
		info->init_tune_flag[SUB_TUNE] = 0;
		info->init_tune_flag[BROW_TUNE] = 1;
		force = 1;
	}

	if ((!force) &&
		(info->state.browser_scenario == browser_mode)) {
		dev_info(info->dev, "%s: dupulicatiopn setting(%d)\n",
						__func__ , browser_mode);
		return 0;
	}

	TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_SKIP, MENU_APP_BROWSER,
		MENU_SKIP, MENU_SKIP, MENU_SKIP, browser_mode);
	ret = cmc624_set_tune_value(id, BITMASK_BROWSER_TUNE);
	if (ret < 0) {
		dev_err(info->dev, "%s: set browser tune value falied\n",
								__func__);
		return ret;
	}

skip_tune:
	info->state.browser_scenario = browser_mode;
	info->state.scenario = MENU_APP_BROWSER;
	return 0;
}

static int apply_negative_tune_value(struct cmc624_info *info, int negative)
{
	u32 id;
	int ret;

	info->state.negative = negative;

	if (negative) {
		TUNE_DATA_ID(id, MENU_CMD_TUNE, MENU_MODE_NEGATIVE, MENU_SKIP,
			MENU_SKIP, MENU_SKIP, MENU_SKIP, MENU_SKIP);
		ret = cmc624_set_tune_value(id, BITMASK_CMD | BITMASK_APP);
		if (ret < 0) {
			dev_err(info->dev, "%s:failed to set negative tune\n",
								__func__);
			info->state.negative = !negative;
			return ret;
		}
	} else {
		/* release negative mode */
		ret = apply_tune_value(info, info->state.background,
						info->state.scenario, 1);
		if (ret < 0) {
			dev_err(info->dev, "%s: failed to restore tune\n",
								__func__);
			info->state.negative = !negative;
			return ret;
		}
	}


	return 0;
}

static int cmc624_parse_text(struct cmc624_info *info, char *src, int len)
{
	int i, count, ret;
	int index = 0;
	char *str_line[CMC624_MAX_SETTINGS];
	char *sstart;
	char *c;
	unsigned int data1;
	unsigned int data2;

	c = src;
	count = 0;
	sstart = c;

	for (i = 0; i < len; i++, c++) {
		if (*c == '\r' || *c == '\n') {
			if (c > sstart) {
				str_line[count] = sstart;
				count++;
			}
			*c = '\0';
			sstart = c + 1;
		}
	}

	if (c > sstart) {
		str_line[count] = sstart;
		count++;
	}

	for (i = 0; i < count; i++) {
		ret = sscanf(str_line[i], "0x%x,0x%x\n", &data1, &data2);
		dev_dbg(info->dev, "%s: Result => [0x%2x 0x%4x] %s\n",
					__func__, data1, data2,
					(ret == 2) ? "Ok" : "Not available");
		if (ret == 2) {
			info->cmc624_tune_seq[index].reg_addr = data1;
			info->cmc624_tune_seq[index++].data = data2;
		}
	}
	return index;
}

static int cmc624_load_tuning_data(struct cmc624_info *info)
{
	char *filename = info->tuning_filename;
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	int cmc624_tune_seq_len = 0;
	mm_segment_t fs;

	dev_dbg(info->dev, "%s: called loading file name : %s\n",
							__func__, filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		dev_err(info->dev, "%s: failed to open file(%s)\n",
							__func__, filename);
		ret = -ENODEV;
		goto err_file_open;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	dev_dbg(info->dev, "%s: File Size : %ld(bytes)", __func__, l);

	dp = kzalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		dev_err(info->dev, "%s: failed memory allocation\n", __func__);
		ret = -ENOMEM;
		goto err_kzalloc;
	}
	pos = 0;

	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	if (ret != l) {
		dev_err(info->dev, "%s: failed to read vfs(%d)\n",
							__func__, ret);
		ret = -ENODEV;
		goto err_vfs_read;
	}

	cmc624_tune_seq_len = cmc624_parse_text(info, dp, l);

	if (!cmc624_tune_seq_len) {
		dev_err(info->dev, "%s: failed to parse\n", __func__);
		kfree(dp);
		return -ENODEV;

	}

	dev_dbg(info->dev, "%s: Loading Tuning Value's Count = %d\n",
						__func__, cmc624_tune_seq_len);

	/* set tune_value */
	ret = cmc624_send_cmd(info->cmc624_tune_seq, cmc624_tune_seq_len);

	ret = cmc624_tune_seq_len;

err_vfs_read:
	kfree(dp);
err_kzalloc:
	filp_close(filp, current->files);
err_file_open:
	set_fs(fs);
	return ret;
}

/**************************************************************************
 * SYSFS Start
 ***************************************************************************/

static ssize_t lcdtype_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);

	dev_info(dev, "type: %s\n", info->pdata->lcd_name);
	return sprintf(buf, info->pdata->lcd_name);

}

static DEVICE_ATTR(lcdtype, S_IRUGO, lcdtype_show, NULL);

static ssize_t scenario_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);

	dev_dbg(dev, "%s called\n", __func__);
	return sprintf(buf, "Current Scenario Mode : %d\n",
		info->state.scenario);
}

static ssize_t scenario_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t size)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	int ret;
	int value;
	int browser_mode;

	sscanf(buf, "%d", &value);
	dev_dbg(dev, "%s: scenario = %d\n", __func__, value);

	if (!IS_SCENARIO_VALID(value)) {
		dev_err(dev, "%s: wrong scenario mode value : %d\n",
							__func__, value);
		return size;
	}

	if (info->state.suspended == true) {
		if (IS_SCENARIO_COLOR(value)) {
			info->state.browser_scenario = value;
			info->state.scenario = MENU_APP_BROWSER;
		} else
			info->state.scenario = value;
		return size;
	}

	if (IS_SCENARIO_COLOR(value)) {
		browser_mode = value - BROWSER_COLOR_TONE_MIN;
		ret = apply_browser_tune_value(info, browser_mode, 0);
	} else
		ret = apply_tune_value(info, info->state.background, value, 0);

	if (ret != 0)
		dev_err(dev, "%s: failed to set tune value, value=%d, ret=%d\n",
							__func__, value, ret);
	return size;
}

static DEVICE_ATTR(scenario, S_IRUGO|S_IWUSR|S_IWGRP,
	scenario_show, scenario_store);

static ssize_t outdoor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "Current OVE Value : %s\n",
		(info->state.outdoor == 0) ? "Disabled" : "Enabled");
}

static ssize_t outdoor_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t size)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	int ret;
	int value;

	sscanf(buf, "%d", &value);
	dev_dbg(dev, "%s: set outdoor : %d\n", __func__,  value);

	if (value < MENU_OUT_OFF || value >= MAX_OUTDOOR_MODE) {
		dev_dbg(dev, "wrong outdoor mode value : %d\n", value);
		return size;
	}

	if (value == info->state.outdoor) {
		dev_dbg(dev, "%s: duplicate setting: %d\n", __func__, value);
		return size;
	}

	if (info->state.suspended == true) {
		info->state.outdoor = value;
		return size;
	}

	ret = apply_sub_tune_value(info, info->state.temperature, value);
	if (ret != 0)
		dev_err(dev, "%s: failed to set sub tune value\n", __func__);

	return size;
}

static DEVICE_ATTR(outdoor, S_IRUGO|S_IWUSR|S_IWGRP,
	outdoor_show, outdoor_store);

static ssize_t negative_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "Current negative Value : %s\n",
		(info->state.negative == 0) ? "Disabled" : "Enabled");
}

static ssize_t negative_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t size)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	int ret;
	int value;

	sscanf(buf, "%d", &value);
	dev_dbg(dev, "%s: set negative : %d\n", __func__, value);

	if (value != 0 && value != 1) {
		dev_warn(dev, "%s: invalid negative value(%d)\n",
							__func__, value);
		return size;
	}

	if (value == info->state.negative) {
		dev_dbg(dev, "%s: duplicate setting: %d\n", __func__, value);
		return size;
	}

	if (info->state.suspended == true) {
		info->state.negative = value;
		return size;
	}

	ret = apply_negative_tune_value(info, value);
	if (ret != 0)
		dev_err(dev, "%s: failed to set negative value(%d)\n",
							__func__, value);

	return size;
}

static DEVICE_ATTR(negative, S_IRUGO|S_IWUSR|S_IWGRP,
	negative_show, negative_store);

static ssize_t mdnie_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	const char *temp_name[MAX_TEMP_MODE] = {
		"STANDARD",
		"WARM",
		"COLD",
	};

	return sprintf(buf, "Current Color Temperature Mode : %s\n",
					temp_name[info->state.temperature]);
}

static ssize_t mdnie_temp_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	int ret;
	int value;

	sscanf(buf, "%d", &value);
	dev_dbg(dev, "%s: set color temperature : %d\n", __func__, value);

	if (value < MENU_TEMP_NORMAL || value >= MAX_TEMP_MODE) {
		dev_err(dev, "%s: invalid color temperature mode value :%d\n",
				__func__, value);
		return size;
	}

	if (value == info->state.temperature) {
		dev_dbg(dev, "%s: duplicate setting: %d\n", __func__, value);
		return size;
	}

	if (info->state.suspended == true) {
		info->state.temperature = value;
		return size;
	}
	ret = apply_sub_tune_value(info, value, info->state.outdoor);
	if (ret)
		dev_err(dev, "%s: failed to set sub tune value (%d)\n",
							__func__, value);

	return size;
}

static DEVICE_ATTR(mdnie_temp, S_IRUGO|S_IWUSR|S_IWGRP,
	mdnie_temp_show, mdnie_temp_store);

static ssize_t mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	const char *name[MAX_BACKGROUND_MODE] = {
		"DYNAMIC",
		"STANDARD",
		"MOVIE",
		"NATURAL",
		"NEGATIVE",
	};

	return sprintf(buf, "Current Background Mode : %s\n",
						name[info->state.background]);
}

static ssize_t mode_store(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t size)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	int ret;
	int value;

	sscanf(buf, "%d", &value);
	dev_dbg(dev, "%s: set background mode : %d\n", __func__,  value);

	if (value < MENU_MODE_DYNAMIC || value >= MAX_BACKGROUND_MODE) {
		dev_err(dev, "%s: invalid backgound mode value : %d\n",
							__func__, value);
		return size;
	}

	if (value == info->state.background) {
		dev_dbg(dev, "%s: duplicate setting: %d\n", __func__, value);
		return size;
	}

	if (info->state.suspended == true) {
		info->state.background = value;
		return size;
	}
	ret = apply_tune_value(info, value, info->state.scenario, 0);
	if (ret != 0)
		dev_err(dev, "%s: failed to set main tune value(%d), ret=%d\n",
				__func__, value, ret);
	return size;
}

static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR | S_IWGRP, mode_show, mode_store);

static ssize_t tuning_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	int ret = 0;
	ret = sprintf(buf, "Tunned File Name : %s\n",
		info->tuning_filename);

	return ret;
}

static ssize_t tuning_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	int ret;

	memset(info->tuning_filename, 0, sizeof(info->tuning_filename));
	sprintf(info->tuning_filename, "%s%s", TUNING_FILE_PATH, buf);
	info->tuning_filename[strlen(info->tuning_filename)-1] = 0;

	dev_dbg(dev, "%s: file name = %s\n", __func__, info->tuning_filename);

	ret = cmc624_load_tuning_data(info);
	if (ret <= 0)
		dev_err(dev, "%s: failed to load tuning data(%d)\n",
								__func__, ret);
		return size;

	return size;
}

static DEVICE_ATTR(tuning, S_IRUGO|S_IWUSR|S_IWGRP, tuning_show, tuning_store);

static ssize_t pwm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	struct cabcoff_pwm_cnt_tbl *pwm_tbl = info->pdata->pwm_tbl;
	struct cabcon_pwr_lut_tbl *pwr_luts = info->pdata->pwr_luts;
	int i;
	int j;
	int k;

	dev_info(dev, "%s: pwm counter: max=0x%04x, def=0x%04x, min=0x%04x\n",
			__func__, pwm_tbl->max, pwm_tbl->def, pwm_tbl->min);

	for (i = 0; i < MAX_PWRLUT_LEV; i++) {
		for (j = 0; j < MAX_PWRLUT_MODE; j++) {
			for (k = 0; k < NUM_PWRLUT_REG; k++) {
				dev_info(dev, "max=%04x, def=%04x, min=%04x\n",
						pwr_luts->max[i][j][k],
						pwr_luts->def[i][j][k],
						pwr_luts->min[i][j][k]);
			}
		}
	}

	return sprintf(buf, "max=%d,def=%d,min=%d",
			pwm_tbl->max, pwm_tbl->def, pwm_tbl->min);
}

static DEVICE_ATTR(pwm, S_IRUGO, pwm_show, NULL);

static ssize_t cabc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cmc624_info *info = dev_get_drvdata(dev);

	dev_info(dev, "%s: cabc mode = %d\n", __func__, info->state.cabc_mode);

	return sprintf(buf, "%d", info->state.cabc_mode);
}

static ssize_t cabc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cmc624_info *info = dev_get_drvdata(dev);
	int value;

	sscanf(buf, "%d", &value);

	if (value)
		info->state.cabc_mode = MENU_CABC_ON;
	else
		info->state.cabc_mode = MENU_CABC_OFF;

	dev_info(dev, "%s: cabc mode = %d\n", __func__, info->state.cabc_mode);

	return size;
}

static DEVICE_ATTR(cabc, S_IRUGO|S_IWUSR|S_IWGRP, cabc_show, cabc_store);

static struct attribute *manual_cmc624_attributes[] = {
	&dev_attr_lcdtype.attr,
	&dev_attr_scenario.attr,
	&dev_attr_outdoor.attr,
	&dev_attr_negative.attr,
	&dev_attr_mdnie_temp.attr,
	&dev_attr_mode.attr,
	&dev_attr_tuning.attr,
	&dev_attr_pwm.attr,
	&dev_attr_cabc.attr,
	NULL,
};

static const struct attribute_group manual_cmc624_group = {
	.attrs = manual_cmc624_attributes,
};

/**************************************************************************
 * SYSFS -- END
 ***************************************************************************/

static int cmc624_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	g_cmc624_info->client = client;

	return 0;
}

static int __devexit cmc624_i2c_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);

	dev_info(&client->dev, "cmc624 i2c remove success!!!\n");

	return 0;
}

static const struct i2c_device_id sec_cmc624_ids[] = {
	{ "sec_cmc624_i2c", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, sec_cmc624_ids);

struct i2c_driver sec_cmc624_i2c_driver = {
	.driver	= {
		.name	= "sec_cmc624_i2c",
		.owner = THIS_MODULE,
	},
	.probe	= cmc624_i2c_probe,
	.remove	= __devexit_p(cmc624_i2c_remove),
	.id_table	= sec_cmc624_ids,
};

static int cmc624_power_off_seq(struct omap_dss_device *dssdev)
{
	struct cmc624_info *info = dev_get_drvdata(&dssdev->dev);
	int ret;
	if (!info) {
		dev_err(&dssdev->dev, "%s: cmc624 is not initialized\n",
								__func__);
		ret = -1;
		goto out;
	}

	/* 1.2V/1.8V/3.3V may be on */

	/* CMC623[0x07] := 0x0004 */
	mutex_lock(&info->tune_lock);
	cmc624_reg_write(info, 0x07, 0x0004);
	mutex_unlock(&info->tune_lock);

	info->state.suspended = true;

	/* power off sequence */
	gpio_set_value(info->pdata->gpio_ima_cmc_en, 0);
	udelay(200);
	gpio_set_value(info->pdata->gpio_ima_nrst, 1);
	udelay(200);
	gpio_set_value(info->pdata->gpio_ima_sleep, 1);
	udelay(200);
	info->pdata->power_lcd(0);
	udelay(200);

	/* power off */
	ret = info->pdata->power_vima_1_8V(0);
	if (ret) {
		dev_err(&dssdev->dev, "%s: failed to control V_IMA_1.8V\n",
								__func__);
		goto out;
	}
	mdelay(20);

	ret = info->pdata->power_vima_1_1V(0);
	if (ret) {
		dev_err(&dssdev->dev, "%s: failed to control V_IMA_1.1V\n",
								__func__);
		goto out;
	}
	udelay(100);

	msleep(200);

out:
	return ret;
}

static int cmc624_power_on_seq(struct omap_dss_device *dssdev)
{
	struct cmc624_info *info = dev_get_drvdata(&dssdev->dev);
	struct tune_data *data;
	u32 id;
	int ret;

	info->state.suspended = false;

	if (!dssdev->skip_init) {
		/* set registers using I2C */
		TUNE_DATA_ID(id, MENU_CMD_INIT, MENU_SKIP,
				MENU_SKIP, MENU_SKIP, MENU_SKIP,
				MENU_SKIP, MENU_SKIP);
		data = cmc624_get_tune_data(id, BITMASK_CMD);
		if (!data)
			return -1;
		cmc624_send_cmd(data->value, data->size);
	}

	/* VIDEO ON */
	dsi_bus_lock(dssdev);
	dssdev->first_vsync = false;
	ret = omapdss_dsi_display_enable(dssdev);
	if (ret) {
		dev_err(&dssdev->dev, "%s: failed to enable DSI\n", __func__);
		return ret;
	}
	dssdev->manager->enable(dssdev->manager);

	/* DSI_DT_PXLSTREAM_24BPP_PACKED; */
	if (!dssdev->skip_init)
		dsi_video_mode_enable(dssdev, 0x3E);
	if (dssdev->skip_init)
		dssdev->skip_init = false;
	dsi_bus_unlock(dssdev);

	if (!dssdev->skip_init) {
		/* LDI sequence */
		TUNE_DATA_ID(id, MENU_CMD_INIT_LDI, MENU_SKIP,
				MENU_SKIP, MENU_SKIP, MENU_SKIP,
				MENU_SKIP, MENU_SKIP);
		data = cmc624_get_tune_data(id, BITMASK_CMD);
		if (!data)
			return -1;
		cmc624_send_cmd(data->value, data->size);
	}

	return 0;

}

static int __devinit cmc624_probe(struct omap_dss_device *dssdev)
{
	struct cmc624_info *info;
	int i;
	int ret = 0;

	info = kzalloc(sizeof(struct cmc624_info), GFP_KERNEL);
	if (!info) {
		dev_err(&dssdev->dev, "Failed to allocate memory for info\n");
		return -ENOMEM;
	}

	mutex_init(&info->tune_lock);

	/* set initial cmc624 state */
	info->state.cabc_mode = MENU_CABC_OFF;
	info->state.brightness = 42;
	info->state.suspended = 0;
	info->state.scenario = MENU_APP_UI;
	info->state.browser_scenario = MENU_SPEC_TONE1;
	info->state.background = MENU_MODE_STANDARD;
	info->state.temperature = MENU_TEMP_NORMAL;
	info->state.outdoor = MENU_OUT_OFF;
	info->state.negative = 0;
	for (i = 0; i < 3; i++)
		info->init_tune_flag[i] = 0;

	info->last_cmc624_Algorithm = 0xFFFF;
	info->last_cmc624_Bank = 0xFFFF;
	info->pdata = dssdev->data;
	dev_set_drvdata(&dssdev->dev, info);

	if (!info->pdata) {
		dev_err(&dssdev->dev, "%s: no panel data\n", __func__);
		ret = -ENODEV;
		goto no_panel_data;
	}

	if (!info->pdata->init_tune_list ||
			!info->pdata->power_lcd ||
			!info->pdata->power_vima_1_1V ||
			!info->pdata->power_vima_1_8V) {
		dev_err(&dssdev->dev, "%s: no panel callback functions\n",
								__func__);
		ret = -ENODEV;
		goto no_panel_data;
	}

	/* set global cmc624 data */
	g_cmc624_info = info;

	dssdev->panel.config = OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS |
		OMAP_DSS_LCD_IPC | OMAP_DSS_LCD_IHS | OMAP_DSS_LCD_ONOFF;

	/* register tune data list */
	ret = info->pdata->init_tune_list();
	if (ret < 0) {
		dev_err(&dssdev->dev, "%s: failed to register tune list\n",
								__func__);
		ret = -1;
		goto cmc624_init_tune_list;
	}


	/* add i2c driver */
	ret = i2c_add_driver(&sec_cmc624_i2c_driver);
	if (ret < 0)
		goto i2c_add_driver_fail;

	/* sysfs */
	info->mdnie_class = class_create(THIS_MODULE, "mdnie");
	if (IS_ERR(info->mdnie_class)) {
		dev_err(&dssdev->dev, "%s: failed to create mdnie class\n",
								__func__);
		ret = -1;
		goto class_create_fail;
	}
	info->dev = device_create(info->mdnie_class,
		NULL, 0, NULL, "mdnie");
	if (IS_ERR(info->dev)) {
		dev_err(&dssdev->dev, "%s: failed to create mdnie file\n",
								__func__);
		ret = -1;
		goto device_create_fail;
	}

	dev_set_drvdata(info->dev, info);

	ret = sysfs_create_group(&info->dev->kobj, &manual_cmc624_group);
	if (ret < 0) {
		dev_err(&dssdev->dev, "%s: failed to create sysfs group(%d)\n",
								__func__, ret);
		goto sysfs_create_group_fail;
	}

	return 0;

sysfs_create_group_fail:
	device_destroy(info->mdnie_class, 0);
device_create_fail:
	class_destroy(info->mdnie_class);
class_create_fail:
	i2c_del_driver(&sec_cmc624_i2c_driver);
i2c_add_driver_fail:
no_panel_data:
cmc624_init_tune_list:
	kfree(info);

	return ret;
}

static void __devexit cmc624_remove(struct omap_dss_device *dssdev)
{
	struct cmc624_info *info = dev_get_drvdata(&dssdev->dev);

	if (!info) {
		dev_err(&dssdev->dev, "%s cmc624 is not initialized\n",
								__func__);
		return;
	}

	device_destroy(info->mdnie_class, 0);
	class_destroy(info->mdnie_class);
	i2c_del_driver(&sec_cmc624_i2c_driver);
	kfree(info);
}

static int cmc624_enable(struct omap_dss_device *dssdev)
{
	int r = 0;
	struct cmc624_info *info = dev_get_drvdata(&dssdev->dev);

	dev_info(&dssdev->dev, "CMC624 enable\n");

	if (!info) {
		dev_err(&dssdev->dev, "%s: cmc624 is not initialized\n",
								__func__);
		return -1;
	}

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		goto err_state;

	/* Delay recommended by panel DATASHEET */
	mdelay(4);
	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r)
			goto err_platform_enable;
	}

	/* Power on LCD panel */
	info->pdata->power_lcd(1);

	if (!dssdev->skip_init) {
		/* Set power */
		gpio_set_value(info->pdata->gpio_ima_nrst, 1);
		gpio_set_value(info->pdata->gpio_ima_cmc_en, 0);
		gpio_set_value(info->pdata->gpio_ima_sleep, 1);
		udelay(200);
	}


	/* V_IMA_1.1V EN*/
	r = info->pdata->power_vima_1_1V(1);
	if (r) {
		dev_err(&dssdev->dev, "%s: failed to control V_IMA_1.1V r=%d\n",
								__func__, r);
		goto err_power_vima_1_1V;
	}
	udelay(20);

	/* V_IMA_1.8V, LCD VDD EN */
	r = info->pdata->power_vima_1_8V(1);
	if (r) {
		dev_err(&dssdev->dev, "%s failed to control V_IMA_1.8V r=%d\n",
								__func__, r);
		goto err_power_vima_1_8V;
	}
	udelay(10);

	if (!dssdev->skip_init) {
		/* FAIL_SAFEB HIGH */
		gpio_set_value(info->pdata->gpio_ima_cmc_en, 1);
		udelay(20);

		/* RESETB LOW */
		gpio_set_value(info->pdata->gpio_ima_nrst, 0);
		usleep_range(40000, 44000);

		/* RESETB HIGH */
		gpio_set_value(info->pdata->gpio_ima_nrst, 1);
	}

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	cmc624_power_on_seq(dssdev);

	return 0;

err_power_vima_1_8V:
	info->pdata->power_vima_1_1V(0);
err_power_vima_1_1V:
	gpio_set_value(info->pdata->gpio_ima_nrst, 0);
	gpio_set_value(info->pdata->gpio_ima_cmc_en, 0);
	gpio_set_value(info->pdata->gpio_ima_sleep, 0);
err_platform_enable:
	dsi_bus_lock(dssdev);
	omapdss_dsi_display_disable(dssdev, 0, 0);
	dsi_bus_unlock(dssdev);
err_state:
	return r;
}

static void cmc624_disable(struct omap_dss_device *dssdev)
{
	dev_info(&dssdev->dev, "CMC624 disable\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
	mdelay(4);

	dsi_bus_lock(dssdev);
	omapdss_dsi_display_disable(dssdev, 0, 0);
	dsi_bus_unlock(dssdev);

	cmc624_power_off_seq(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int cmc624_suspend(struct omap_dss_device *dssdev)
{
	cmc624_disable(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	return 0;
}

static int cmc624_resume(struct omap_dss_device *dssdev)
{
	cmc624_enable(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	return 0;
}

static int cmc624_set_update_mode(struct omap_dss_device *dssdev,
			       enum omap_dss_update_mode mode)
{
	if (mode != OMAP_DSS_UPDATE_AUTO)
		return -EINVAL;

	return 0;
}

static enum omap_dss_update_mode cmc624_get_update_mode(struct omap_dss_device
						     *dssdev)
{
	return OMAP_DSS_UPDATE_AUTO;
}

static void cmc624_framedone_cb(int err, void *data)
{
	struct omap_dss_device *dssdev = data;
	dev_dbg(&dssdev->dev, "framedone, err %d\n", err);
	dsi_bus_unlock(dssdev);
}

static int cmc624_update(struct omap_dss_device *dssdev,
		      u16 x, u16 y, u16 w, u16 h)
{
	int r;

	dsi_bus_lock(dssdev);

	r = omap_dsi_prepare_update(dssdev, &x, &y, &w, &h, true);
	if (r)
		goto err;

	/* We use VC(0) for VideoPort Data and VC(1) for commands */
	r = omap_dsi_update(dssdev, 0, x, y, w, h, cmc624_framedone_cb, dssdev);
	if (r)
		goto err;

	dsi_bus_unlock(dssdev);

	return 0;
err:
	dsi_bus_unlock(dssdev);
	return r;
}

static int cmc624_sync(struct omap_dss_device *dssdev)
{
	return 0;
}

static void cmc624_get_resolution(struct omap_dss_device *dssdev,
			       u16 *xres, u16 *yres)
{
		*xres = dssdev->panel.timings.x_res;
		*yres = dssdev->panel.timings.y_res;
}

static void cmc624_get_timings(struct omap_dss_device *dssdev,
			    struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void cmc624_set_timings(struct omap_dss_device *dssdev,
			    struct omap_video_timings *timings)
{
	dssdev->panel.timings.x_res = timings->x_res;
	dssdev->panel.timings.y_res = timings->y_res;
	dssdev->panel.timings.pixel_clock = timings->pixel_clock;
	dssdev->panel.timings.hsw = timings->hsw;
	dssdev->panel.timings.hfp = timings->hfp;
	dssdev->panel.timings.hbp = timings->hbp;
	dssdev->panel.timings.vsw = timings->vsw;
	dssdev->panel.timings.vfp = timings->vfp;
	dssdev->panel.timings.vbp = timings->vbp;
}

static int cmc624_check_timings(struct omap_dss_device *dssdev,
			     struct omap_video_timings *timings)
{
	return 0;
}

static struct omap_dss_driver cmc624_omap_dss_driver =  {
	.probe  = cmc624_probe,
	.remove = cmc624_remove,
	.enable   = cmc624_enable,
	.disable  = cmc624_disable,
	.suspend  = cmc624_suspend,
	.resume   = cmc624_resume,
	.set_update_mode = cmc624_set_update_mode,
	.get_update_mode = cmc624_get_update_mode,
	.update = cmc624_update,
	.sync = cmc624_sync,
	.get_resolution = cmc624_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,
	.get_timings = cmc624_get_timings,
	.set_timings = cmc624_set_timings,
	.check_timings = cmc624_check_timings,
	.driver = {
		.name = "sec_cmc624",
		.owner  = THIS_MODULE,
	},
};

static int __init cmc624_init(void)
{
	return omap_dss_register_driver(&cmc624_omap_dss_driver);
}

static void __exit cmc624_exit(void)
{
	omap_dss_unregister_driver(&cmc624_omap_dss_driver);
}

module_init(cmc624_init);
module_exit(cmc624_exit);

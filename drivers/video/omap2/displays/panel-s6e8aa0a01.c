/* drivers/video/omap2/displays/panel-s6e8aa0a01.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
 *
 * Bsed on panel-s6e8aa0.c
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

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <video/omapdss.h>
#include <linux/platform_data/panel-s6e8aa0a01.h>

#include "../dss/dss.h"
#include "s6e8aa0_gamma_l.h"

#ifdef CONFIG_AID_DIMMING
#include "aid_s6e8aa0.h"
#endif

#ifdef CONFIG_SMART_DIMMING
#include "smart_dimming.h"

#define PANEL_A1_M3			SM2A1
#define PANEL_A2_M3			SM2A2

#define LDI_MTP_LENGTH			24
#define LDI_MTP_ADDR			0xD3

#define DYNAMIC_ELVSS_MIN_VALUE		0x81
#define DYNAMIC_ELVSS_MAX_VALUE		0x9F

#define ELVSS_MODE0_MIN_VOLTAGE		62
#define ELVSS_MODE1_MIN_VOLTAGE		52

struct str_elvss {
	u8 reference;
};
#endif /* CONFIG_SMART_DIMMING */

#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS			255

#define MAX_GAMMA			300
#define DEFAULT_BRIGHTNESS		160
#define DEFAULT_GAMMA_LEVEL		GAMMA_160CD

#define LDI_ID_REG			0xD1
#define LDI_ID_LEN			3

/* DSI Command Virtual channel */
#define CMD_VC_CHANNEL			1

#define DRIVER_NAME			"s6e8aa0a01_i2c"
#define DEVICE_NAME			"s6e8aa0a01_i2c"

static int s6e8aa0a01_update(struct omap_dss_device *dssdev,
			     u16 x, u16 y, u16 w, u16 h);
static void s6e8aa0a01_disable(struct omap_dss_device *dssdev);
static int s6e8aa0a01_enable(struct omap_dss_device *dssdev);

struct s6e8aa0a01_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;
	struct backlight_device *bd;
	bool enabled;
	u8 rotate;
	bool mirror;
	bool use_dsi_bl;
	unsigned int bl;
	unsigned int auto_brightness;
	unsigned int acl_enable;
	unsigned int acl_cur;
	unsigned int current_bl;
	unsigned int current_elvss;

	unsigned long hw_guard_end;	/* next value of jiffies when we can
					 * issue the next sleep in/out command
					 */
	unsigned long hw_guard_wait;	/* max guard time in jiffies */

	atomic_t do_update;
	struct {
		u16 x;
		u16 y;
		u16 w;
		u16 h;
	} update_region;

	bool cabc_broken;
	unsigned cabc_mode;

	bool force_update;
	struct omap_video_timings *timings;

	struct panel_s6e8aa0a01_data *pdata;

	u8 panel_id[LDI_ID_LEN];

	unsigned char **gamma_table;
	unsigned char **elvss_table;

#ifdef CONFIG_SMART_DIMMING
	unsigned int support_elvss;
	struct str_smart_dim smart;
	struct str_elvss elvss;
#endif
#ifdef CONFIG_AID_DIMMING
	unsigned int support_aid;
	unsigned char f8[GAMMA_MAX][39];
#endif

	int connected;
};

static const u8 s6e8aa0a01_mtp_unlock[] = {
	0xF1,
	0x5A,
	0x5A,
};

static const u8 s6e8aa0a01_mtp_lock[] = {
	0xF1,
	0xA5,
	0xA5,
};

static int s6e8aa0a01_write_reg(struct omap_dss_device *dssdev, u8 reg, u8 val)
{
	u8 buf[2];
	buf[0] = reg;
	buf[1] = val;

	return dsi_vc_dcs_write(dssdev, 1, buf, 2);
}

static int s6e8aa0a01_write_block(struct omap_dss_device *dssdev,
				  const u8 *data, int len)
{
	/* XXX: dsi_vc_dsc_write should take a const u8 */
	return dsi_vc_dcs_write(dssdev, 1, (u8 *) data, len);
}

static int s6e8aa0a01_write_block_nosync(struct omap_dss_device *dssdev,
					 const u8 *data, int len)
{
	return dsi_vc_dcs_write_nosync(dssdev, 1, (u8 *) data, len);
}

static int s6e8aa0a01_read_block(struct omap_dss_device *dssdev,
				 u8 cmd, u8 *data, int len)
{
	return dsi_vc_dcs_read(dssdev, 1, cmd, data, len);
}

static void s6e8aa0a01_write_sequence(struct omap_dss_device *dssdev,
				      const struct s6e8aa0a01_sequence_entry
				      *seq, int seq_len)
{
	while (seq_len--) {
		if (seq->cmd_len)
			s6e8aa0a01_write_block(dssdev, seq->cmd, seq->cmd_len);
		if (seq->msleep)
			msleep(seq->msleep);
		seq++;
	}
}


#ifdef CONFIG_AID_DIMMING
/* CONFIG_AID_DIMMING */
static const u32 candela_table[GAMMA_MAX] = {
	20, 30, 40, 50, 60, 70, 80, 90, 100,
	110, 120, 130, 140, 150, 160, 170, 180,
	182, 184, 186, 188,
	190, 200, 210, 220, 230, 240, 250, 290, MAX_GAMMA - 1
};

/* CONFIG_AID_DIMMING */
static u32 aid_candela_table[GAMMA_MAX] = {
	  base_20to100,   base_20to100,   base_20to100,   base_20to100,
	  base_20to100,   base_20to100,   base_20to100,   base_20to100,
	  base_20to100, AOR40_BASE_110, AOR40_BASE_120, AOR40_BASE_130,
	AOR40_BASE_140, AOR40_BASE_150, AOR40_BASE_160, AOR40_BASE_170,
	AOR40_BASE_180, AOR40_BASE_182, AOR40_BASE_184, AOR40_BASE_186,
	AOR40_BASE_188, 190, 200, 210, 220, 230, 240, 250,
	290, MAX_GAMMA - 1
};

/* CONFIG_AID_DIMMING */
static u32 elvss_offset_table[ELVSS_STATUS_MAX] = {
	ELVSS_OFFSET_110,
	ELVSS_OFFSET_120,
	ELVSS_OFFSET_130,
	ELVSS_OFFSET_140,
	ELVSS_OFFSET_150,
	ELVSS_OFFSET_160,
	ELVSS_OFFSET_170,
	ELVSS_OFFSET_180,
	ELVSS_OFFSET_190,
	ELVSS_OFFSET_200,
	ELVSS_OFFSET_210,
	ELVSS_OFFSET_220,
	ELVSS_OFFSET_230,
	ELVSS_OFFSET_240,
	ELVSS_OFFSET_250,
	ELVSS_OFFSET_260,
	ELVSS_OFFSET_270,
	ELVSS_OFFSET_280,
	ELVSS_OFFSET_290,
	ELVSS_OFFSET_300
};

/* CONFIG_AID_DIMMING */
static u32 s6e8aa0a01_brightness2backlight_level(u32 brightness)
{
	int backlightlevel;

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is only supported from 0 to 24 */
	switch (brightness) {
	case 0 ... 29:
		backlightlevel = GAMMA_20CD;
		break;
	case 30 ... 39:
		backlightlevel = GAMMA_30CD;
		break;
	case 40 ... 49:
		backlightlevel = GAMMA_40CD;
		break;
	case 50 ... 59:
		backlightlevel = GAMMA_50CD;
		break;
	case 60 ... 69:
		backlightlevel = GAMMA_60CD;
		break;
	case 70 ... 79:
		backlightlevel = GAMMA_70CD;
		break;
	case 80 ... 89:
		backlightlevel = GAMMA_80CD;
		break;
	case 90 ... 99:
		backlightlevel = GAMMA_90CD;
		break;
	case 100 ... 109:
		backlightlevel = GAMMA_100CD;
		break;
	case 110 ... 119:
		backlightlevel = GAMMA_110CD;
		break;
	case 120 ... 129:
		backlightlevel = GAMMA_120CD;
		break;
	case 130 ... 139:
		backlightlevel = GAMMA_130CD;
		break;
	case 140 ... 149:
		backlightlevel = GAMMA_140CD;
		break;
	case 150 ... 159:
		backlightlevel = GAMMA_150CD;
		break;
	case 160 ... 169:
		backlightlevel = GAMMA_160CD;
		break;
	case 170 ... 179:
		backlightlevel = GAMMA_170CD;
		break;
	case 180 ... 181:
		backlightlevel = GAMMA_180CD;
		break;
	case 182 ... 183:
		backlightlevel = GAMMA_182CD;
		break;
	case 184 ... 185:
		backlightlevel = GAMMA_184CD;
		break;
	case 186 ... 187:
		backlightlevel = GAMMA_186CD;
		break;
	case 188 ... 189:
		backlightlevel = GAMMA_188CD;
		break;
	case 190 ... 199:
		backlightlevel = GAMMA_190CD;
		break;
	case 200 ... 209:
		backlightlevel = GAMMA_200CD;
		break;
	case 210 ... 219:
		backlightlevel = GAMMA_210CD;
		break;
	case 220 ... 229:
		backlightlevel = GAMMA_220CD;
		break;
	case 230 ... 239:
		backlightlevel = GAMMA_230CD;
		break;
	case 240 ... 249:
		backlightlevel = GAMMA_240CD;
		break;
	case 250 ... 254:
		backlightlevel = GAMMA_250CD;
		break;
	case 255:
		backlightlevel = GAMMA_300CD;
		break;
	default:
		backlightlevel = DEFAULT_GAMMA_LEVEL;
		break;
	}

	return backlightlevel;
}

/* CONFIG_AID_DIMMING */
static int s6e8aa0a01_aid_parameter_ctl(struct s6e8aa0a01_data *lcd, bool force)
{
	struct omap_dss_device *dssdev = lcd->dssdev;

	if (likely(lcd->support_aid)) {
		if ((lcd->f8[lcd->bl][0x12] !=
				lcd->f8[lcd->current_bl][0x12]) ||
		    (lcd->f8[lcd->bl][0x01] !=
				lcd->f8[lcd->current_bl][0x01]) || (force))
			s6e8aa0a01_write_block(dssdev, lcd->f8[lcd->bl],
				lcd->pdata->seq_panel_condition_set_size);
	}

	return 0;
}

/* CONFIG_AID_DIMMING */
static int s6e8aa0a01_set_elvss(struct s6e8aa0a01_data *s6, bool force)
{
	int ret = 0, elvss_level = 0;
	u32 candela = candela_table[s6->bl];
	struct omap_dss_device *dssdev = s6->dssdev;

	switch (candela) {
	case 0 ... 110:
		elvss_level = ELVSS_110;
		break;
	case 111 ... 120:
		elvss_level = ELVSS_120;
		break;
	case 121 ... 130:
		elvss_level = ELVSS_130;
		break;
	case 131 ... 140:
		elvss_level = ELVSS_140;
		break;
	case 141 ... 150:
		elvss_level = ELVSS_150;
		break;
	case 151 ... 160:
		elvss_level = ELVSS_160;
		break;
	case 161 ... 170:
		elvss_level = ELVSS_170;
		break;
	case 171 ... 180:
		elvss_level = ELVSS_180;
		break;
	case 181 ... 190:
		elvss_level = ELVSS_190;
		break;
	case 191 ... 200:
		elvss_level = ELVSS_200;
		break;
	case 201 ... 210:
		elvss_level = ELVSS_210;
		break;
	case 211 ... 220:
		elvss_level = ELVSS_220;
		break;
	case 221 ... 230:
		elvss_level = ELVSS_230;
		break;
	case 231 ... 240:
		elvss_level = ELVSS_240;
		break;
	case 241 ... 250:
		elvss_level = ELVSS_250;
		break;
	case 251 ... 260:
		elvss_level = ELVSS_260;
		break;
	case 261 ... 270:
		elvss_level = ELVSS_270;
		break;
	case 271 ... 280:
		elvss_level = ELVSS_280;
		break;
	case 281 ... 290:
		elvss_level = ELVSS_290;
		break;
	case 291 ... 300:
		elvss_level = ELVSS_300;
		break;
	}

	if ((s6->current_elvss != s6->elvss_table[elvss_level][2]) || force) {
		ret = s6e8aa0a01_write_block_nosync(dssdev,
					     s6->elvss_table[elvss_level],
					     ELVSS_PARAM_SIZE);
		s6->current_elvss = s6->elvss_table[elvss_level][2];
	}

	dev_dbg(&dssdev->dev, "elvss = %x\n", s6->elvss_table[elvss_level][2]);

	if (ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

/* CONFIG_AID_DIMMING */
static int s6e8aa0a01_init_gamma_table(struct s6e8aa0a01_data *s6)
{
	int i, ret = 0;

	s6->gamma_table = kzalloc(GAMMA_MAX * sizeof(u8 *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(s6->gamma_table)) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		s6->gamma_table[i] =
		    kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(s6->gamma_table[i])) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
		s6->gamma_table[i][0] = 0xFA;
		s6->gamma_table[i][1] = 0x01;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		if (candela_table[i] < 190)
			calc_gamma_table(&s6->smart, aid_candela_table[i],
					 &s6->gamma_table[i][2], G_21);
		else
			calc_gamma_table(&s6->smart, aid_candela_table[i],
					 &s6->gamma_table[i][2], G_22);
	}

	return 0;

err_alloc_gamma:
	while (i > 0)
		kfree(s6->gamma_table[--i]);

	kfree(s6->gamma_table);
err_alloc_gamma_table:
	return ret;
}

/* CONFIG_AID_DIMMING */
static int s6e8aa0a01_init_aid_dimming_table(struct s6e8aa0a01_data *s6)
{
	unsigned int i, j;

	for (i = 0; i < ARRAY_SIZE(aid_rgb_fix_table); i++) {
		j = (aid_rgb_fix_table[i].gray * 3 +
		     aid_rgb_fix_table[i].rgb) + 2;
		s6->gamma_table[aid_rgb_fix_table[i].candela_idx][j] +=
		    aid_rgb_fix_table[i].offset;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		memcpy(s6->f8[i], s6->pdata->seq_panel_condition_set,
		       s6->pdata->seq_panel_condition_set_size);
		s6->f8[i][0x12] = aid_command_table[i][0];
		s6->f8[i][0x01] = aid_command_table[i][1];
	}

	return 0;
}
#else /* CONFIG_AID_DIMMING */
static const u32 candela_table[GAMMA_MAX] = {
	 20, 30,  40 , 50 , 60,  70,  80,  90, 100, 110,
	120, 130, 140, 150, 160, 170, 180, 190, 200, 210,
	220, 230, 240, 250, 300, MAX_GAMMA
};

static u32 elvss_offset_table[ELVSS_STATUS_MAX] = {
	ELVSS_OFFSET_MIN,
	ELVSS_OFFSET_1,
	ELVSS_OFFSET_2,
	ELVSS_OFFSET_MAX
};

static u32 s6e8aa0a01_brightness2backlight_level(u32 brightness)
{
	u32 backlightlevel;

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is only supported from 0 to 24 */
	switch (brightness) {
	case 0 ... 29:
		backlightlevel = GAMMA_30CD;
		break;
	case 30 ... 39:
		backlightlevel = GAMMA_40CD;
		break;
	case 40 ... 254:
		backlightlevel = (brightness - candela_table[0]) / 10;
		break;
	case 255:
		backlightlevel = ARRAY_SIZE(candela_table) - 1;
		break;
	default:
		backlightlevel = DEFAULT_GAMMA_LEVEL;
		break;
	}

	return backlightlevel;
}

static int s6e8aa0a01_set_elvss(struct s6e8aa0a01_data *s6, bool force)
{
	int ret = 0, elvss_level = 0;
	u32 candela = candela_table[s6->bl];
	struct omap_dss_device *dssdev = s6->dssdev;

	switch (candela) {
	case 0 ... 100:
		elvss_level = ELVSS_MIN;
		break;
	case 101 ... 160:
		elvss_level = ELVSS_1;
		break;
	case 161 ... 200:
		elvss_level = ELVSS_2;
		break;
	case 201 ... 300:
		elvss_level = ELVSS_MAX;
		break;
	default:
		break;
	}

	if ((s6->current_elvss != s6->elvss_table[elvss_level][2]) || force) {
		ret = s6e8aa0a01_write_block_nosync(dssdev,
					     s6->elvss_table[elvss_level],
					     ELVSS_PARAM_SIZE);
		s6->current_elvss = s6->elvss_table[elvss_level][2];
	}

	dev_dbg(&dssdev->dev, "elvss = %x\n", s6->elvss_table[elvss_level][2]);

	if (ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

static int s6e8aa0a01_init_gamma_table(struct s6e8aa0a01_data *s6)
{
	int i, ret = 0;

	s6->gamma_table = kzalloc(GAMMA_MAX * sizeof(u8 *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(s6->gamma_table)) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		s6->gamma_table[i] =
		    kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(s6->gamma_table[i])) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
		s6->gamma_table[i][0] = 0xFA;
		s6->gamma_table[i][1] = 0x01;
		calc_gamma_table(&s6->smart,
				 candela_table[i] - 1, s6->gamma_table[i] + 2);
	}

	return 0;

err_alloc_gamma:
	while (i > 0)
		kfree(s6->gamma_table[--i]);

	kfree(s6->gamma_table);
err_alloc_gamma_table:
	return ret;
}
#endif /* CONFIG_AID_DIMMING */

static int s6e8aa0a01_rotate(struct omap_dss_device *dssdev, u8 rotate)
{
	return 0;
}

static u8 s6e8aa0a01_get_rotate(struct omap_dss_device *dssdev)
{
	return 0;
}

static int s6e8aa0a01_mirror(struct omap_dss_device *dssdev, bool enable)
{
	return 0;
}

static bool s6e8aa0a01_get_mirror(struct omap_dss_device *dssdev)
{
	return 0;
}

static void s6e8aa0a01_get_timings(struct omap_dss_device *dssdev,
				   struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void s6e8aa0a01_set_timings(struct omap_dss_device *dssdev,
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

static int s6e8aa0a01_check_timings(struct omap_dss_device *dssdev,
				    struct omap_video_timings *timings)
{
	return 0;
}

static void s6e8aa0a01_get_resolution(struct omap_dss_device *dssdev,
				      u16 *xres, u16 * yres)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);

	if (s6->rotate == 0 || s6->rotate == 2) {
		*xres = dssdev->panel.timings.x_res;
		*yres = dssdev->panel.timings.y_res;
	} else {
		*yres = dssdev->panel.timings.x_res;
		*xres = dssdev->panel.timings.y_res;
	}
}

static int s6e8aa0a01_enable_te(struct omap_dss_device *dssdev, bool enable)
{
	return 0;
}

static int s6e8aa0a01_hw_reset(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);

	gpio_set_value(s6->pdata->reset_gpio, 0);
	msleep(20);
	gpio_set_value(s6->pdata->reset_gpio, 1);
	msleep(40);

	return 0;
}

static void s6e8aa0a01_read_id_info(struct s6e8aa0a01_data *s6)
{
	struct omap_dss_device *dssdev = s6->dssdev;
	int ret;
	u8 cmd = 0xD1;

	if (!s6->connected) {
		dev_info(&dssdev->dev,
			 "*** s6e8aa0a01 panel is not connected!\n");
		s6->panel_id[0] = 0;
		s6->panel_id[1] = 0;
		s6->panel_id[2] = 0;
		return;
	}

	dsi_vc_set_max_rx_packet_size(dssdev, 1, 3);
	ret = s6e8aa0a01_read_block(dssdev, cmd, s6->panel_id,
				    ARRAY_SIZE(s6->panel_id));
	dsi_vc_set_max_rx_packet_size(dssdev, 1, 1);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to read id data\n", __func__);
}

#ifdef CONFIG_SMART_DIMMING
static void s6e8aa0a01_read_mtp_info(struct s6e8aa0a01_data *s6, u8 * mtp_data)
{
	int ret;
	struct omap_dss_device *dssdev = s6->dssdev;

	if (!s6->connected) {
		dev_info(&dssdev->dev,
			 "*** s6e8aa0a01 panel is not connected!\n");
		return;
	}

	dsi_vc_set_max_rx_packet_size(dssdev, 1, LDI_MTP_LENGTH);
	ret = s6e8aa0a01_read_block(dssdev, LDI_MTP_ADDR,
				    mtp_data, LDI_MTP_LENGTH);
	dsi_vc_set_max_rx_packet_size(dssdev, 1, 1);
	if (unlikely(ret < 0))
		pr_err("%s: Failed to read mtp data\n", __func__);
}

static void s6e8aa0a01_check_id(struct s6e8aa0a01_data *s6)
{
	unsigned int i;

	for (i = 0; i < LDI_ID_LEN; i++)
		s6->smart.panelid[i] = s6->panel_id[i];

	if ((s6->panel_id[0] == PANEL_A1_M3) ||
	    (s6->panel_id[0] == PANEL_A2_M3)) {
		s6->support_elvss = 1;
		s6->elvss.reference = s6->panel_id[2];
		pr_debug("Dynamic ELVSS Information, 0x%x\n",
			 s6->elvss.reference);
	} else
		s6->support_elvss = 0;
}

static u8 get_elvss_value(struct s6e8aa0a01_data *s6, u8 elvss_level)
{
	u8 ref = 0;
	u8 offset;
	ref = s6->elvss.reference;
	offset = elvss_offset_table[elvss_level];

	ref += offset;

	if (ref < DYNAMIC_ELVSS_MIN_VALUE)
		ref = DYNAMIC_ELVSS_MIN_VALUE;
	else if (ref > DYNAMIC_ELVSS_MAX_VALUE)
		ref = DYNAMIC_ELVSS_MAX_VALUE;

	return ref;
}

static int init_elvss_table(struct s6e8aa0a01_data *s6)
{
	int i, ret = 0;

	s6->elvss_table = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

	if (IS_ERR_OR_NULL(s6->elvss_table)) {
		pr_err("failed to allocate elvss table\n");
		ret = -ENOMEM;
		goto err_alloc_elvss_table;
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		s6->elvss_table[i] =
		    kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(s6->elvss_table[i])) {
			pr_err("failed to allocate elvss\n");
			ret = -ENOMEM;
			goto err_alloc_elvss;
		}
		s6->elvss_table[i][0] = 0xB1;
		s6->elvss_table[i][1] = 0x04;
		s6->elvss_table[i][2] = get_elvss_value(s6, i);
	}

	return 0;

err_alloc_elvss:
	while (i > 0)
		kfree(s6->elvss_table[--i]);
	kfree(s6->elvss_table);
err_alloc_elvss_table:
	return ret;
}
#endif /* CONFIG_SMART_DIMMING */

static int s6e8aa0a01_gamma_ctl(struct s6e8aa0a01_data *lcd)
{
	struct omap_dss_device *dssdev = lcd->dssdev;
	const unsigned char seq_gamma_update[] = { 0xF7, 0x03, 0x00 };

	s6e8aa0a01_write_block_nosync(dssdev,
				      lcd->gamma_table[lcd->bl],
				      GAMMA_PARAM_SIZE);

	/* Gamma Set Update */
	s6e8aa0a01_write_block(dssdev, seq_gamma_update,
			       ARRAY_SIZE(seq_gamma_update));

	return 0;
}

static void s6e8aa0a01_set_acl(struct s6e8aa0a01_data *lcd)
{
	struct omap_dss_device *dssdev = lcd->dssdev;
	struct panel_s6e8aa0a01_data *pdata = lcd->pdata;
	int i;
	unsigned int cd;
	unsigned int max_cd = 0;
	const struct s6e8aa0a01_acl_parameters *acl;
	const unsigned char seq_acl_off[] = { 0xC0, 0x00, 0x00 };
	const unsigned char seq_acl_on[] = { 0xC0, 0x01, 0x00 };

	/* Quietly return if you don't have a table */
	if (!pdata->acl_table_size)
		return;

	max_cd = pdata->acl_table[pdata->acl_table_size - 1].cd;

	cd = lcd->bl;
	if (cd > max_cd)
		cd = max_cd;

	if (lcd->acl_enable) {
		for (i = 0; i < pdata->acl_table_size; i++)
			if (cd <= pdata->acl_table[i].cd)
				break;

		if (i == pdata->acl_table_size)
			i = pdata->acl_table_size - 1;

		acl = &pdata->acl_table[i];
		if (lcd->acl_cur != acl->acl_val) {
			s6e8aa0a01_write_block_nosync(dssdev, acl->regs,
						      sizeof(acl->regs));
			s6e8aa0a01_write_block(dssdev,
					     seq_acl_on,
					     ARRAY_SIZE(seq_acl_on));

			lcd->acl_cur = acl->acl_val;
		}
	} else {
		if (lcd->acl_cur != 0) {
			lcd->acl_cur = 0;
			s6e8aa0a01_write_block(dssdev,
					     seq_acl_off,
					     ARRAY_SIZE(seq_acl_off));
		}
	}
	pr_debug("%s : acl_cur=%d, %d\n", __func__, lcd->acl_cur,
		 lcd->acl_enable);
}

static int s6e8aa0a01_update_brightness(struct omap_dss_device *dssdev,
					bool force)
{
	struct s6e8aa0a01_data *lcd = dev_get_drvdata(&dssdev->dev);
	u32 brightness;

	brightness = lcd->bd->props.brightness;

	if (unlikely(!lcd->auto_brightness && brightness > 250))
		brightness = 250;

	lcd->bl = s6e8aa0a01_brightness2backlight_level(brightness);

	if ((force) || ((lcd->enabled) && (lcd->current_bl != lcd->bl))) {

		s6e8aa0a01_gamma_ctl(lcd);
#ifdef CONFIG_AID_DIMMING
		s6e8aa0a01_aid_parameter_ctl(lcd, force);
#endif
		s6e8aa0a01_set_acl(lcd);

		s6e8aa0a01_set_elvss(lcd, force);

		lcd->current_bl = lcd->bl;

		dev_info(&dssdev->dev, "brightness=%d, bl=%d, candela=%d\n",
			 brightness, lcd->bl, candela_table[lcd->bl]);
	}

	return 0;
}

static int s6e8aa0a01_set_brightness(struct backlight_device *bd)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(&bd->dev);
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	int bl = s6e8aa0a01_brightness2backlight_level(bd->props.brightness);
	int ret = 0;

	if (bl == s6->bl || !s6->connected)
		return 0;

	s6->bl = bl;
	mutex_lock(&s6->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		dsi_bus_lock(dssdev);
		ret = s6e8aa0a01_update_brightness(dssdev, 0);
		dsi_bus_unlock(dssdev);
	}
	mutex_unlock(&s6->lock);

	return ret;
}

static int s6e8aa0a01_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops s6e8aa0a01_backlight_ops = {
	.get_brightness	= s6e8aa0a01_get_brightness,
	.update_status	= s6e8aa0a01_set_brightness,
};

static int s6e8aa0a01_start(struct omap_dss_device *dssdev);
static void s6e8aa0a01_stop(struct omap_dss_device *dssdev);

static ssize_t s6e8aa0a01_sysfs_lcd_type_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	char *lcd_type = "SDC_AMS465GS37";

	return sprintf(buf, "%s\n", lcd_type);
}

static ssize_t s6e8aa0a01_sysfs_lcd_power_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t len)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	int ret;
	int err;
	int lcd_enable;

	err = kstrtoint(buf, 0, &lcd_enable);
	if (unlikely(err < 0))
		return err;
	dev_info(dev, "s6e8aa0a01_sysfs_store_lcd_power - %d\n", lcd_enable);

	mutex_lock(&s6->lock);
	if (lcd_enable) {
		if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
			ret = -EINVAL;
			goto out;
		}
		ret = s6e8aa0a01_start(dssdev);
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	} else {
		if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
			ret = -EINVAL;
			goto out;
		}
		s6e8aa0a01_stop(dssdev);
		dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
	}

out:
	mutex_unlock(&s6->lock);
	return len;
}

static DEVICE_ATTR(lcd_type, S_IRUGO,
		   s6e8aa0a01_sysfs_lcd_type_show, NULL);
static DEVICE_ATTR(lcd_power, S_IWUSR | S_IWGRP,
		   NULL, s6e8aa0a01_sysfs_lcd_power_store);

static ssize_t s6e8aa0a01_sysfs_power_reduce_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(dev);
	struct s6e8aa0a01_data *lcd = dev_get_drvdata(&dssdev->dev);

	return sprintf(buf, "%d\n", lcd->acl_enable);
}

static ssize_t s6e8aa0a01_sysfs_power_reduce_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(dev);
	struct s6e8aa0a01_data *lcd = dev_get_drvdata(&dssdev->dev);
	int value;
	int err;

	err = kstrtoint(buf, 0, &value);
	if (unlikely(err < 0))
		return err;

	mutex_lock(&lcd->lock);
	if (lcd->acl_enable != value) {
		dev_info(dev, "%s - %d, %d\n", __func__,
			 lcd->acl_enable, value);
		dsi_bus_lock(dssdev);
		lcd->acl_enable = value;
		if (lcd->enabled)
			s6e8aa0a01_set_acl(lcd);
		dsi_bus_unlock(dssdev);
	}
	mutex_unlock(&lcd->lock);

	return size;
}

static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR,
		   s6e8aa0a01_sysfs_power_reduce_show,
		   s6e8aa0a01_sysfs_power_reduce_store);

static ssize_t s6e8aa0a01_auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(dev);
	struct s6e8aa0a01_data *lcd = dev_get_drvdata(&dssdev->dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->auto_brightness);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t s6e8aa0a01_auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(dev);
	struct s6e8aa0a01_data *lcd = dev_get_drvdata(&dssdev->dev);
	int value;
	int err;

	err = kstrtoint(buf, 0, &value);
	if (unlikely(err < 0))
		return err;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s - %d, %d\n",
				__func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->lock);
			if (lcd->enabled && !lcd->auto_brightness) {
				dsi_bus_lock(dssdev);
				s6e8aa0a01_update_brightness(lcd->dssdev, 0);
				dsi_bus_unlock(dssdev);
			}
		}
	}
	return size;
}

static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR,
		s6e8aa0a01_auto_brightness_show,
		s6e8aa0a01_auto_brightness_store);


static struct class *lcd_class;

static irqreturn_t s6e8aa0a01_oled_det_thread(int irq, void *data)
{
	struct s6e8aa0a01_data *s6 = data;
	struct omap_dss_device *dssdev = s6->dssdev;
	int oled_det_gpio = irq_to_gpio(irq);

	if (gpio_get_value(oled_det_gpio)) {
		dev_dbg(&dssdev->dev, "%s: skip: oled det gpio level is high\n",
								__func__);
		return IRQ_HANDLED;
	}

	dev_info(&dssdev->dev, "%s: prevent ESD shock\n", __func__);

	/*
	 * ESD shock, which asynchronizes HS clock, can be detected by VGH
	 * and oled_det gpio level drop. So if oled_det gpio irq occurs,
	 * calls dss disable and enable function for HS toggling.
	 */
	s6e8aa0a01_disable(dssdev);
	s6e8aa0a01_enable(dssdev);

	return IRQ_HANDLED;
}

static int s6e8aa0a01_probe(struct omap_dss_device *dssdev)
{
	struct device *lcd_dev;
	struct backlight_properties props = {
		.brightness	= 255,
		.max_brightness = 255,
		.type		= BACKLIGHT_RAW,
	};
	struct s6e8aa0a01_data *s6;
	int ret = 0;
#ifdef CONFIG_SMART_DIMMING
	u8 mtp_data[LDI_MTP_LENGTH] = { 0, };
#endif
	dev_dbg(&dssdev->dev, "%s is called", __func__);

	if (unlikely(!dssdev->data)) {
		dev_err(&dssdev->dev, "no platform data!\n");
		return -EINVAL;
	}

	dssdev->panel.config = OMAP_DSS_LCD_TFT;

	dssdev->panel.acbi = 0;
	dssdev->panel.acb = 40;

	s6 = kzalloc(sizeof(*s6), GFP_KERNEL);
	if (unlikely(!s6))
		return -ENOMEM;

	s6->dssdev = dssdev;
	s6->pdata = dssdev->data;

	s6->bl = s6e8aa0a01_brightness2backlight_level(props.brightness);
	s6->current_bl = GAMMA_MAX;

	if (!s6->pdata->seq_display_set || !s6->pdata->seq_etc_set) {
		dev_err(&dssdev->dev, "Invalid platform data\n");
		ret = -EINVAL;
		goto err;
	}

	ret = gpio_request(s6->pdata->reset_gpio, "s6e8aa0a01_reset");
	if (unlikely(ret < 0)) {
		dev_err(&dssdev->dev, "gpio_request %d failed!\n",
			s6->pdata->reset_gpio);
		goto err;
	}
	gpio_direction_output(s6->pdata->reset_gpio, 1);

	ret = gpio_request(s6->pdata->oled_id_gpio, "s6e8aa0a01_connected");
	if (unlikely(ret < 0)) {
		dev_err(&dssdev->dev, "gpio_request %d failed!\n",
			s6->pdata->oled_id_gpio);
		goto err;
	}
	gpio_direction_input(s6->pdata->oled_id_gpio);
	s6->connected = gpio_get_value(s6->pdata->oled_id_gpio);
	if (!s6->connected)
		dev_info(&dssdev->dev,
			 "*** s6e8aa0a01 panel is not connected!\n\n");

	ret = request_threaded_irq(s6->pdata->oled_det_irq, NULL,
				s6e8aa0a01_oled_det_thread,
				IRQF_TRIGGER_FALLING, "oled_detection", s6);
	if (ret)
		dev_err(&dssdev->dev, "Failed to request IRQ %d: %d\n",
			s6->pdata->oled_det_irq, ret);

	disable_irq_nosync(s6->pdata->oled_det_irq);

	mutex_init(&s6->lock);

	atomic_set(&s6->do_update, 0);

	dev_set_drvdata(&dssdev->dev, s6);

	/* Register DSI backlight  control */
	s6->bd = backlight_device_register("panel", &dssdev->dev, dssdev,
					   &s6e8aa0a01_backlight_ops, &props);
	if (IS_ERR(s6->bd)) {
		ret = PTR_ERR(s6->bd);
		goto err_backlight_device_register;
	}

	s6->acl_enable = true;

	if (cpu_is_omap44xx())
		s6->force_update = true;

	lcd_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR(lcd_class)) {
		pr_err("Failed to create lcd_class!");
		goto err_backlight_device_register;
	}

	lcd_dev = device_create(lcd_class, NULL, 0, NULL, "panel");
	if (IS_ERR(lcd_dev)) {
		pr_err("Failed to create device(panel)!\n");
		goto err_lcd_class;
	}

	dev_set_drvdata(lcd_dev, &dssdev->dev);

	ret = device_create_file(lcd_dev, &dev_attr_lcd_type);
	if (unlikely(ret < 0)) {
		dev_err(lcd_dev, "failed to add 'lcd_type' sysfs entries\n");
		goto err_lcd_device;
	}

	ret = device_create_file(lcd_dev, &dev_attr_lcd_power);
	if (unlikely(ret < 0)) {
		dev_err(lcd_dev, "failed to add 'lcd_power' sysfs entries\n");
		goto err_lcd_type;
	}

	ret = device_create_file(lcd_dev, &dev_attr_power_reduce);
	if (unlikely(ret < 0)) {
		dev_err(lcd_dev, "failed to add 'power_reduce' sysfs entries\n");
		goto err_lcd_power;
	}

	ret = device_create_file(&s6->bd->dev, &dev_attr_auto_brightness);
	if (unlikely(ret < 0)) {
		dev_err(lcd_dev, "failed to add 'auto_brightness' sysfs entries\n");
		goto err_power_reduce;
	}

	dsi_bus_lock(dssdev);
	s6e8aa0a01_read_id_info(s6);
	dev_info(&dssdev->dev, "ID: %x, %x, %x\n",
		 s6->panel_id[0], s6->panel_id[1], s6->panel_id[2]);

#ifdef CONFIG_SMART_DIMMING
	s6e8aa0a01_check_id(s6);

	init_table_info(&s6->smart);

	s6e8aa0a01_read_mtp_info(s6, mtp_data);

	calc_voltage_table(&s6->smart, mtp_data);

	if (s6->support_elvss)
		ret = init_elvss_table(s6);
	else {
		s6->elvss_table = (unsigned char **)ELVSS_TABLE;
		ret = 0;
	}

	ret += s6e8aa0a01_init_gamma_table(s6);

#ifdef CONFIG_AID_DIMMING
	if (s6->panel_id[1] == 0x01 || s6->panel_id[1] == 0x02) {
		pr_info("AID Dimming is started. %d\n", s6->panel_id[1]);
		s6->support_aid = 1;
		ret += s6e8aa0a01_init_aid_dimming_table(s6);
	}
#endif /* CONFIG_AID_DIMMING */

	if (unlikely(ret)) {
		pr_warn("gamma table generation is failed\n");
		s6->gamma_table = (unsigned char **)gamma22_table;
		s6->elvss_table = (unsigned char **)ELVSS_TABLE;
	}
#endif /* CONFIG_SMART_DIMMING */

	dsi_bus_unlock(dssdev);

	dev_dbg(&dssdev->dev, "s6e8aa0a01_probe\n");
	return ret;

err_power_reduce:
	device_remove_file(lcd_dev, &dev_attr_power_reduce);
err_lcd_power:
	device_remove_file(lcd_dev, &dev_attr_lcd_power);
err_lcd_type:
	device_remove_file(lcd_dev, &dev_attr_lcd_type);
err_lcd_device:
	device_destroy(lcd_class, 0);
err_lcd_class:
	class_destroy(lcd_class);
err_backlight_device_register:
	mutex_destroy(&s6->lock);
	gpio_free(s6->pdata->reset_gpio);
	gpio_free(s6->pdata->oled_id_gpio);
err:
	kfree(s6);

	return ret;
}

static void s6e8aa0a01_remove(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);

	backlight_device_unregister(s6->bd);
	mutex_destroy(&s6->lock);
	gpio_free(s6->pdata->reset_gpio);
	gpio_free(s6->pdata->oled_id_gpio);
	kfree(s6);
}

/**
 * s6e8aa0a01_config - Configure S6E8AA0A01
 *
 * Initial configuration for S6E8AA0A01 configuration registers, PLL...
 */
static void s6e8aa0a01_config(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	struct panel_s6e8aa0a01_data *pdata = s6->pdata;

	if (!s6->connected)
		return;

	s6e8aa0a01_write_sequence(dssdev, pdata->seq_display_set,
				  pdata->seq_display_set_size);

	s6e8aa0a01_update_brightness(dssdev, 1);

	s6e8aa0a01_write_sequence(dssdev, pdata->seq_etc_set,
				  pdata->seq_etc_set_size);
}

static int s6e8aa0a01_power_on(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	pr_info(" %s, called + ", __func__);
	/* At power on the first vsync has not been received yet */
	dssdev->first_vsync = false;

	if (s6->enabled != 1) {
		if (s6->pdata->set_power)
			s6->pdata->set_power(true);

		ret = omapdss_dsi_display_enable(dssdev);
		if (ret) {
			dev_err(&dssdev->dev, "failed to enable DSI\n");
			goto err;
		}

		/* reset s6e8aa0a01 bridge */
		if (!dssdev->skip_init) {
			s6e8aa0a01_hw_reset(dssdev);

			/* XXX */
			msleep(100);
			s6e8aa0a01_config(dssdev);

			/* DSI_DT_PXLSTREAM_24BPP_PACKED; */
			dsi_video_mode_enable(dssdev, 0x3E);
		}

		s6->enabled = 1;
	}

	if (dssdev->skip_init)
		dssdev->skip_init = false;
	pr_info(" %s, called - ", __func__);
err:
	return ret;
}

static void s6e8aa0a01_power_off(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);

	pr_info(" %s, called + ", __func__);
	gpio_set_value(s6->pdata->reset_gpio, 0);
	usleep_range(10 * 1000, 11 * 1000);

	s6->enabled = 0;
	omapdss_dsi_display_disable(dssdev, 0, 0);

	if (s6->pdata->set_power)
		s6->pdata->set_power(false);
	pr_info(" %s, called - ", __func__);
}

static int s6e8aa0a01_start(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	unsigned long pclk;
	int ret;

	dsi_bus_lock(dssdev);

	ret = s6e8aa0a01_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (ret) {
		dev_dbg(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
		dssdev->manager->enable(dssdev->manager);
	}

	/* fixup pclk based on pll config */
	pclk = dispc_pclk_rate(dssdev->channel);
	if (pclk)
		dssdev->panel.timings.pixel_clock = (pclk + 500) / 1000;

	enable_irq(s6->pdata->oled_det_irq);

	return ret;
}

static void s6e8aa0a01_stop(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);

	disable_irq_nosync(s6->pdata->oled_det_irq);

	dssdev->manager->disable(dssdev->manager);

	dsi_bus_lock(dssdev);

	s6e8aa0a01_power_off(dssdev);

	dsi_bus_unlock(dssdev);
}

static void s6e8aa0a01_disable(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "%s\n", __func__);

	mutex_lock(&s6->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		s6e8aa0a01_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	mutex_unlock(&s6->lock);
}

static int s6e8aa0a01_enable(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "%s\n", __func__);

	mutex_lock(&s6->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		ret = -EINVAL;
		goto out;
	}

	ret = s6e8aa0a01_start(dssdev);
out:
	mutex_unlock(&s6->lock);
	return ret;
}

static void s6e8aa0a01_framedone_cb(int err, void *data)
{
	struct omap_dss_device *dssdev = data;

	dev_dbg(&dssdev->dev, "framedone, err %d\n", err);
	dsi_bus_unlock(dssdev);
}

static int s6e8aa0a01_update(struct omap_dss_device *dssdev,
			     u16 x, u16 y, u16 w, u16 h)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	dev_dbg(&dssdev->dev, "update %d, %d, %d x %d\n", x, y, w, h);

	mutex_lock(&s6->lock);
	dsi_bus_lock(dssdev);

	if (!s6->enabled) {
		ret = 0;
		goto out;
	}

	ret = omap_dsi_prepare_update(dssdev, &x, &y, &w, &h, true);
	if (ret)
		goto out;

	/* We use VC(0) for VideoPort Data and VC(1) for commands */
	ret = omap_dsi_update(dssdev, 0, x, y, w, h,
			      s6e8aa0a01_framedone_cb, dssdev);

out:
	dsi_bus_unlock(dssdev);
	mutex_unlock(&s6->lock);
	return ret;
}

static int s6e8aa0a01_sync(struct omap_dss_device *dssdev)
{
	/* TODO: */
	return 0;
}

static int s6e8aa0a01_set_update_mode(struct omap_dss_device *dssdev,
				      enum omap_dss_update_mode mode)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);

	if (s6->force_update) {
		if (mode != OMAP_DSS_UPDATE_AUTO)
			return -EINVAL;
	} else {
		if (mode != OMAP_DSS_UPDATE_MANUAL)
			return -EINVAL;
	}

	return 0;
}

static enum omap_dss_update_mode
s6e8aa0a01_get_update_mode(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);

	if (s6->force_update)
		return OMAP_DSS_UPDATE_AUTO;
	else
		return OMAP_DSS_UPDATE_MANUAL;
}

#ifdef CONFIG_PM
static int s6e8aa0a01_resume(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "resume\n");

	mutex_lock(&s6->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		ret = -EINVAL;
		goto out;
	}
	ret = s6e8aa0a01_start(dssdev);

out:
	mutex_unlock(&s6->lock);
	return ret;
}

static int s6e8aa0a01_suspend(struct omap_dss_device *dssdev)
{
	struct s6e8aa0a01_data *s6 = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	dev_dbg(&dssdev->dev, "suspend\n");

	mutex_lock(&s6->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		ret = -EINVAL;
		goto out;
	}
	s6e8aa0a01_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

out:
	mutex_unlock(&s6->lock);
	return ret;
}
#endif /* CONFIG_PM */

static struct omap_dss_driver s6e8aa0a01_driver = {
	.probe		= s6e8aa0a01_probe,
	.remove		= s6e8aa0a01_remove,
	.enable		= s6e8aa0a01_enable,
	.disable	= s6e8aa0a01_disable,
#ifdef CONFIG_PM
	.suspend	= s6e8aa0a01_suspend,
	.resume		= s6e8aa0a01_resume,
#endif
	.set_update_mode = s6e8aa0a01_set_update_mode,
	.get_update_mode = s6e8aa0a01_get_update_mode,

	.update		= s6e8aa0a01_update,
	.sync		= s6e8aa0a01_sync,

	.get_resolution = s6e8aa0a01_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	/* dummy entry start */
	.enable_te	= s6e8aa0a01_enable_te,
	.set_rotate	= s6e8aa0a01_rotate,
	.get_rotate	= s6e8aa0a01_get_rotate,
	.set_mirror	= s6e8aa0a01_mirror,
	.get_mirror	= s6e8aa0a01_get_mirror,
	/* dummy entry end */

	.get_timings	= s6e8aa0a01_get_timings,
	.set_timings	= s6e8aa0a01_set_timings,
	.check_timings	= s6e8aa0a01_check_timings,

	.driver = {
		.name	= "s6e8aa0a01",
		.owner	= THIS_MODULE,
	},
};

static int __init s6e8aa0a01_init(void)
{
	omap_dss_register_driver(&s6e8aa0a01_driver);

	return 0;
}

static void __exit s6e8aa0a01_exit(void)
{
	device_destroy(lcd_class, 0);
	class_destroy(lcd_class);
	omap_dss_unregister_driver(&s6e8aa0a01_driver);
}

module_init(s6e8aa0a01_init);
module_exit(s6e8aa0a01_exit);

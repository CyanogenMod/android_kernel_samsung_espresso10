/*
 * Samsung lms501kf07 panel support
 *
 * Derived from drivers/video/omap2/displays/panel-s6e8aa0.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/regulator/consumer.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <plat/dmtimer.h>
#include <video/omapdss.h>
#include <linux/platform_data/panel-lms501kf07.h>

#include <asm/mach/arch.h>

#include "../dss/dss.h"

/* DSI Command Virtual channel */
#define CMD_VC_CHANNEL 1

#define CABC_DISABLE	0
#define CABC_ENABLE		1
#define CABC_INVALID	(-1)
#define CABC_OFF		0
#define CABC_UI			1
#define CABC_PICTURE	2
#define CABC_MOVIE		3
#define CABC_DEFAULT	(CABC_MOVIE)

/* mdnie scenario */
#define MDNIE_INVALID		(-1)

#define MDNIE_UI_MODE			0
#define MDNIE_VIDEO_MODE		1
#define MDNIE_VIDEO_WARM_MODE	2
#define MDNIE_VIDEO_COLD_MODE	3
#define MDNIE_CAMERA_MODE		4
#define MDNIE_NAVI_MODE			5
#define MDNIE_GALLERY_MODE		6
#define MDNIE_VT_MODE			7
#define MDNIE_COLOR_TONE_1		40
#define MDNIE_COLOR_TONE_2		41
#define MDNIE_COLOR_TONE_3		42

/* mdnie mode */
#define MDNIE_DYNAMIC	0
#define MDNIE_STANDARD	1
#define MDNIE_MOVIE		2

#define MDNIE_DISABLE	0
#define MDNIE_ENABLE	1

static int lms501kf07_update(struct omap_dss_device *dssdev,
			     u16 x, u16 y, u16 w, u16 h);

static struct omap_video_timings lms501kf07_timings = {
	.x_res = 480,
	.y_res = 800,
	/*(x_res+hfp+hbp+hsw)*(y_res+vfp+fbp+vsw)*60HZ/1000 */
	.pixel_clock = 37476,
	.hfp = 271,
	.hsw = 6,
	.hbp = 1,
	.vfp = 8,
	.vsw = 4,
	.vbp = 12,
};

struct mdnie {
	int cabc;		/* 0 : Disable, 1 : Enable */
	int scenario;		/* See the define above : MDNIE_XXX_MODE */
	int mode;		/* See the define above : MDNIE_YYY */
	int outdoor;		/* 0 : Disable, 1 : Enable */
	int negative;		/* 0 : Disable, 1 : Enable */
};

struct lms501kf07_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;
	struct backlight_device *bd;
	struct dentry *debug_dir;
	struct class *lcd_class;	/* /sys/class/lcd/panel/lcd_type&power*/
	struct device *dev;	/* /sys/class/lcd/panel/lcd_type & lcd_power */
	struct class *mdnie_class;	/* /sys/class/lcd/mdnie/mdnie/XXXXXXX */
	struct device *mdnie_dev;	/* /sys/class/lcd/mdnie/mdnie/XXXXXXX */
	struct mdnie mdnie_data;	/* store the setting of mdnie */
	bool enabled;
	u8 rotate;
	bool mirror;
	bool use_dsi_bl;
	unsigned int plat_bl;
	unsigned int bl;
	unsigned int current_brightness;

	bool sync_lost_error;

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

	struct panel_lms501kf07_data *pdata;

	u8 panel_id[3];
};

static struct lms501kf07_data *gp_lcd;

struct LCD_BRIGHTNESS {
	int off;
	int deflt;
	int dim;
	int min;
	int min_center;
	int center;
	int center_max;
	int max;
};

static struct LCD_BRIGHTNESS bright_tbl_plat = {
	.off = 0,
	.deflt = 130,
	.dim = 20,
	.min = 30,
	.min_center = 87,
	.center = 130,
	.center_max = 199,
	.max = 255,
};

static struct LCD_BRIGHTNESS bright_tbl_normal = {
	.off = 0,
	.deflt = 102,
	.dim = 13,
	.min = 13,
	.min_center = 20,
	.center = 102,
	.center_max = 136,
	.max = 200,
};

static struct LCD_BRIGHTNESS bright_tbl_cabc = {
	.off = 0,
	.deflt = 93,
	.dim = 14,
	.min = 14,
	.min_center = 21,
	.center = 93,
	.center_max = 130,
	.max = 185,
};

/*

Brightness Interval from Platform

0    A          B          C          D          E
+----+----------+----------+----------+----------+

Brightness Interval for LCD backlight

0'   A'         B'         C'         D'         E'
+----+----------+----------+----------+----------+

0 : bright_tbl_plat.dim
A : bright_tbl_plat.min
B : bright_tbl_plat.min_center
C : bright_tbl_plat.center
D : bright_tbl_plat.center_max
E : bright_tbl_plat.max

0' : bright_tbl_XXXX.dim
A' : bright_tbl_XXXX.min
B' : bright_tbl_XXXX.min_center
C' : bright_tbl_XXXX.center
D' : bright_tbl_XXXX.center_max
E' : bright_tbl_XXXX.max

(XXXX => normal or cabc)

We need to map the Brightness Interval from Platform to that of LCD.

*/
static int lms501kf07_convert_brightness(const int plat_bl, bool is_cabc)
{
	struct LCD_BRIGHTNESS *bright_tbl;
	int lcd_bl = plat_bl;

	if (is_cabc == true)
		bright_tbl = &bright_tbl_cabc;
	else
		bright_tbl = &bright_tbl_normal;

	if (plat_bl <= 0) {
		lcd_bl = bright_tbl->off;
	} else if (plat_bl < bright_tbl_plat.min) {
		lcd_bl = bright_tbl->dim;
	} else if (plat_bl < bright_tbl_plat.min_center) {
		lcd_bl -= bright_tbl_plat.min;
		lcd_bl *=
		    ((bright_tbl->min_center -
		      bright_tbl->min) * 100) / (bright_tbl_plat.min_center -
						 bright_tbl_plat.min);
		/* *100/100 is to prevent integer lcd_bl becomes 0 */
		lcd_bl /= 100;
		lcd_bl += bright_tbl->min;
	} else if (plat_bl < bright_tbl_plat.center) {
		lcd_bl -= bright_tbl_plat.min_center;
		lcd_bl *=
		    ((bright_tbl->center -
		      bright_tbl->min_center) * 100) / (bright_tbl_plat.center -
							bright_tbl_plat.
							min_center);
		lcd_bl /= 100;
		lcd_bl += bright_tbl->min_center;
	} else if (plat_bl < bright_tbl_plat.center_max) {
		lcd_bl -= bright_tbl_plat.center;
		lcd_bl *=
		    ((bright_tbl->center_max -
		      bright_tbl->center) * 100) / (bright_tbl_plat.center_max -
						    bright_tbl_plat.center);
		lcd_bl /= 100;
		lcd_bl += bright_tbl->center;
	} else if (plat_bl < bright_tbl_plat.max) {
		lcd_bl -= bright_tbl_plat.center_max;
		lcd_bl *=
		    ((bright_tbl->max -
		      bright_tbl->center_max) * 100) / (bright_tbl_plat.max -
							bright_tbl_plat.
							center_max);
		lcd_bl /= 100;
		lcd_bl += bright_tbl->center_max;
	} else {
		lcd_bl = bright_tbl->max;
	}

	/* printk("%s : %d => %d\n", __func__, plat_bl, lcd_bl); */

	return lcd_bl;
}

static int lms501kf07_write_reg(struct omap_dss_device *dssdev, u8 reg, u8 val)
{
	u8 buf[2];
	buf[0] = reg;
	buf[1] = val;

	return dsi_vc_dcs_write(dssdev, 1, buf, 2);
}

static int lms501kf07_write_reg_nosync(struct omap_dss_device *dssdev, u8 reg,
				       u8 val)
{
	u8 buf[2];
	buf[0] = reg;
	buf[1] = val;

	return dsi_vc_dcs_write_nosync(dssdev, 1, buf, 2);
}

static int lms501kf07_write_block(struct omap_dss_device *dssdev,
				  const u8 *data, int len)
{
	return dsi_vc_dcs_write(dssdev, 1, (u8 *) data, len);
}

static int lms501kf07_write_block_nosync(struct omap_dss_device *dssdev,
					 const u8 *data, int len)
{
	return dsi_vc_dcs_write_nosync(dssdev, 1, (u8 *) data, len);
}

static int lms501kf07_read_block(struct omap_dss_device *dssdev,
				 u8 cmd, u8 *data, int len)
{
	return dsi_vc_dcs_read(dssdev, 1, cmd, data, len);
}

static void lms501kf07_write_sequence(struct omap_dss_device *dssdev,
				      const struct lms501kf07_sequence_entry
				      *seq, int seq_len)
{
	while (seq_len--) {
		if (seq->cmd_len)
			lms501kf07_write_block(dssdev, seq->cmd, seq->cmd_len);
		if (seq->msleep)
			usleep_range(seq->msleep * USEC_PER_MSEC,
					seq->msleep * USEC_PER_MSEC + 100);
		seq++;
	}
}

/***********************
*** DUMMY FUNCTIONS ****
***********************/

static int lms501kf07_rotate(struct omap_dss_device *dssdev, u8 rotate)
{
	return 0;
}

static u8 lms501kf07_get_rotate(struct omap_dss_device *dssdev)
{
	return 0;
}

static int lms501kf07_mirror(struct omap_dss_device *dssdev, bool enable)
{
	return 0;
}

static bool lms501kf07_get_mirror(struct omap_dss_device *dssdev)
{
	return 0;
}

static void lms501kf07_get_timings(struct omap_dss_device *dssdev,
				   struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void lms501kf07_set_timings(struct omap_dss_device *dssdev,
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

static int lms501kf07_check_timings(struct omap_dss_device *dssdev,
				    struct omap_video_timings *timings)
{
	return 0;
}

static void lms501kf07_get_resolution(struct omap_dss_device *dssdev,
				      u16 *xres, u16 *yres)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	if (lcd->rotate == 0 || lcd->rotate == 2) {
		*xres = dssdev->panel.timings.x_res;
		*yres = dssdev->panel.timings.y_res;
	} else {
		*yres = dssdev->panel.timings.x_res;
		*xres = dssdev->panel.timings.y_res;
	}
}

static int lms501kf07_get_recommended_bpp(struct omap_dss_device *dssdev)
{
	return 24;
}

static int lms501kf07_enable_te(struct omap_dss_device *dssdev, bool enable)
{
	return 0;
}

static int lms501kf07_hw_reset(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	gpio_set_value(lcd->pdata->reset_gpio, 0);
	usleep_range(1 * USEC_PER_MSEC, 1 * USEC_PER_MSEC + 100);
	gpio_set_value(lcd->pdata->reset_gpio, 1);
	gpio_set_value(lcd->pdata->blctrl_gpio, 1);
	usleep_range(10 * USEC_PER_MSEC, 10 * USEC_PER_MSEC + 100);

	return 0;
}

static int lms501kf07_update_brightness(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	lcd->current_brightness = lcd->bl;

	lms501kf07_write_reg_nosync(dssdev, 0x51, lcd->bl);

	/* update cabc minimum brightness according to the brightness value */
	if (lcd->mdnie_data.cabc == CABC_ENABLE) {
		/* 3/4 => 75% of bl */
		int cabc_min_bright = (lcd->bl * 3) >> 2;
		lms501kf07_write_reg_nosync(dssdev, 0x5E, cabc_min_bright);
	}

	return 0;
}

static u64 lms501kf07_limit_brightness(u64 bc[3], u64 bcmax)
{
	return 0;
}

static void lms501kf07_read_id_info(struct lms501kf07_data *lcd)
{
}

static int lms501kf07_set_brightness(struct backlight_device *bd)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(&bd->dev);
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	int bl = bd->props.brightness;
	int ret = 0;

	/* printk("%s : level = %d\n", __func__, bl); */

	if (bl == lcd->bl)
		return 0;

	lcd->plat_bl = bl;

	if (lcd->mdnie_data.cabc == CABC_ENABLE)
		lcd->bl = lms501kf07_convert_brightness(bl, true);
	else
		lcd->bl = lms501kf07_convert_brightness(bl, false);

	mutex_lock(&lcd->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		dsi_bus_lock(dssdev);
		ret = lms501kf07_update_brightness(dssdev);
		dsi_bus_unlock(dssdev);
	}
	mutex_unlock(&lcd->lock);

	return ret;
}

static int lms501kf07_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops lms501kf07_backlight_ops = {
	.get_brightness = lms501kf07_get_brightness,
	.update_status = lms501kf07_set_brightness,
};

static void lms501kf07_remove(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	debugfs_remove_recursive(lcd->debug_dir);
	backlight_device_unregister(lcd->bd);
	mutex_destroy(&lcd->lock);
	gpio_free(lcd->pdata->reset_gpio);
	kfree(lcd);
}

/**
 * lms501kf07_config - Configure LMS501KF07
 *
 * Initial configuration for LMS501KF07 configuration registers, PLL...
 */
static void lms501kf07_config(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	struct panel_lms501kf07_data *pdata = lcd->pdata;
	struct lms501kf07_mdnie_data *mdnie = pdata->mdnie_data;

	lms501kf07_write_sequence(dssdev, pdata->seq_display_on,
				  pdata->seq_display_on_size);

	switch (lcd->mdnie_data.scenario) {
	case MDNIE_UI_MODE:
		dev_info(&dssdev->dev, "%s :: MDNIE_MODE recover for ui\n",
			 __func__);
		lms501kf07_write_block(dssdev, mdnie->ui, mdnie->size);
		break;
	case MDNIE_GALLERY_MODE:
		dev_info(&dssdev->dev, "%s :: MDNIE_MODE recover for Gallery\n",
			 __func__);
		lms501kf07_write_block(dssdev, mdnie->gallery, mdnie->size);
		break;
	case MDNIE_VIDEO_MODE:
		dev_info(&dssdev->dev, "%s :: MDNIE_MODE recover for Video\n",
			 __func__);
		lms501kf07_write_block(dssdev, mdnie->video, mdnie->size);
		break;
	case MDNIE_VIDEO_WARM_MODE:
		dev_info(&dssdev->dev,
			 "%s :: MDNIE_MODE recover for Video Warm\n", __func__);
		lms501kf07_write_block(dssdev, mdnie->video_warm, mdnie->size);
		break;
	case MDNIE_VIDEO_COLD_MODE:
		dev_info(&dssdev->dev,
			 "%s :: MDNIE_MODE recover for Video Cold\n", __func__);
		lms501kf07_write_block(dssdev, mdnie->video_cold, mdnie->size);
		break;
	case MDNIE_CAMERA_MODE:
		dev_info(&dssdev->dev, "%s :: MDNIE_MODE recover for Camera\n",
			 __func__);
		lms501kf07_write_block(dssdev, mdnie->camera, mdnie->size);
		break;
	default:
		dev_err(&dssdev->dev, "%s :: MDNIE SCENARIO FAIL!\n", __func__);
		lms501kf07_write_block(dssdev, mdnie->ui, mdnie->size);
	}

	/* turn on cabc if cabc was set before entering sleep mode */
	if (lcd->mdnie_data.cabc == CABC_ENABLE)
		lms501kf07_write_reg_nosync(dssdev, 0x55, CABC_DEFAULT);

	/* Don't turn on LCD backlight after initializing LCD panel.
	   Let Android control it. But we still need to turn it on
	   when Android doesn't started. For an example, Recovery boot
	   program is an independent program from Android. */
	if (pdata->bl_on_after_init == true)
		lms501kf07_write_reg_nosync(dssdev, 0x51,
						    bright_tbl_normal.deflt);

	/* If lcd was turned off due to sync lost error, we should turn on
	   backlight here because Appl won't send brightness value for this
	   case */
	if (lcd->sync_lost_error == true) {
		printk(KERN_ERR "%s : turn on backlight for sync lost error!\n",
		       __func__);
		lms501kf07_write_reg_nosync(dssdev, 0x51,
					    lcd->current_brightness);
		lcd->sync_lost_error = false;
	}
}

static int lms501kf07_power_on(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	/* At power on the first vsync has not been received yet */
	dssdev->first_vsync = false;

	if (lcd->enabled != 1) {
		if (lcd->pdata->set_power)
			lcd->pdata->set_power(true);

		if (!dssdev->skip_init)
			lms501kf07_hw_reset(dssdev);

		ret = omapdss_dsi_display_enable(dssdev);
		if (ret) {
			dev_err(&dssdev->dev, "failed to enable DSI\n");
			goto err;
		}

		/* reset lms501kf07 bridge */
		if (!dssdev->skip_init) {
			lms501kf07_config(dssdev);
			/* DSI_DT_PXLSTREAM_24BPP_PACKED; */
			dsi_video_mode_enable(dssdev, 0x3E);
		}

		lcd->enabled = 1;
	}

	if (dssdev->skip_init)
		dssdev->skip_init = false;

 err:
	return ret;
}

static void lms501kf07_power_off(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	if (dssdev->sync_lost_error == 1) {
		printk(KERN_ERR "%s : due to sync lost error!\n", __func__);
		lcd->sync_lost_error = true;
	}

	lms501kf07_write_reg_nosync(dssdev, 0x28, 0x00);	/* disp off */
	lms501kf07_write_reg_nosync(dssdev, 0x10, 0x00);	/* sleep in */

	gpio_set_value(lcd->pdata->blctrl_gpio, 0);
	gpio_set_value(lcd->pdata->reset_gpio, 0);
	usleep_range(10 * USEC_PER_MSEC, 10 * USEC_PER_MSEC + 100);

	if (lcd->pdata->set_power)
		lcd->pdata->set_power(false);

	lcd->enabled = 0;
	omapdss_dsi_display_disable(dssdev, 0, 0);
}

static int lms501kf07_start(struct omap_dss_device *dssdev)
{
	int r = 0;
	unsigned long pclk;

	dsi_bus_lock(dssdev);

	r = lms501kf07_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r) {
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

	return r;
}

static void lms501kf07_stop(struct omap_dss_device *dssdev)
{
	dssdev->manager->disable(dssdev->manager);

	dsi_bus_lock(dssdev);

	lms501kf07_power_off(dssdev);

	dsi_bus_unlock(dssdev);
}

static void lms501kf07_disable(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		lms501kf07_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	mutex_unlock(&lcd->lock);
}

static int lms501kf07_enable(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "enable\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		ret = -EINVAL;
		goto out;
	}

	ret = lms501kf07_start(dssdev);
 out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static ssize_t lms501kf07_sysfs_show_lcd_type(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	char temp[17];

	sprintf(temp, "SMD_LMS501KF07L\n");
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t lms501kf07_sysfs_store_lcd_power(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t len)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;
	int rc;
	int lcd_enable;

	rc = kstrtoint(buf, 0, &lcd_enable);
	if (rc < 0)
		return rc;

	dev_info(dev, "lms501kf07_sysfs_store_lcd_power - %d\n", lcd_enable);

	mutex_lock(&lcd->lock);
	if (lcd_enable) {
		if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
			ret = -EINVAL;
			goto out;
		}
		ret = lms501kf07_start(dssdev);
		lms501kf07_write_reg_nosync(dssdev, 0x51, lcd->bl);
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	} else {
		if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
			ret = -EINVAL;
			goto out;
		}
		lms501kf07_stop(dssdev);
		dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
	}

 out:
	mutex_unlock(&lcd->lock);
	return len;
}

static ssize_t lms501kf07_sysfs_store_lcd_cabc(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t len)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	int cabc_state = 0;	/* 0=> off, 1=> ui, 2=> picture, 3=> movie */
	int ret = 0;

	char *cabc_type[] = { "OFF", "UI", "PICTURE", "MOVIE" };

	if (lcd->enabled == 0) {
		dev_err(&dssdev->dev,
			"CABC setting failed because LCD isn't enabled.\n");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 0, &cabc_state);
	if (ret < 0) {
		dev_err(&dssdev->dev, "Invalid CABC state.\n");
		return -EINVAL;
	}

	if (cabc_state < 0 || cabc_state > 3) {
		dev_err(&dssdev->dev, "Invalid CABC state.\n");
		return -EINVAL;
	}

	mutex_lock(&lcd->lock);
	dsi_bus_lock(lcd->dssdev);

	lms501kf07_write_reg_nosync(dssdev, 0x55, cabc_state);

	dsi_bus_unlock(lcd->dssdev);
	mutex_unlock(&lcd->lock);

	dev_dbg(&dssdev->dev, "LCD CABC : %s\n", cabc_type[cabc_state]);

	return len;
}

static ssize_t lms501kf07_sysfs_store_lcd_cabc_min_bright(struct device *dev,
							  struct
							  device_attribute
							  *attr,
							  const char *buf,
							  size_t len)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	int cabc_min_bright = 0;
	int ret = 0;

	if (lcd->enabled == 0) {
		dev_err(&dssdev->dev,
			"CABC setting failed because LCD isn't enabled.\n");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 0, &cabc_min_bright);
	if (ret < 0) {
		dev_err(&dssdev->dev, "Invalid CABC minimum brightness.\n");
		return -EINVAL;
	}

	if (cabc_min_bright < 0 || cabc_min_bright > 255) {
		dev_err(&dssdev->dev, "Invalid CABC minimum brightness.\n");
		return -EINVAL;
	}

	mutex_lock(&lcd->lock);
	dsi_bus_lock(lcd->dssdev);

	lms501kf07_write_reg_nosync(dssdev, 0x5E, cabc_min_bright);

	dsi_bus_unlock(lcd->dssdev);
	mutex_unlock(&lcd->lock);

	dev_dbg(&dssdev->dev, "LCD CABC : Minimum brightness is %d\n",
		cabc_min_bright);

	return len;
}

static ssize_t lms501kf07_sysfs_store_lcd_bright(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t len)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	int bright = 0;
	int ret = 0;

	if (lcd->enabled == 0) {
		dev_err(&dssdev->dev,
			"Brightness setting failed because LCD isn't enabled.\n");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 0, &bright);
	if (ret < 0) {
		dev_err(&dssdev->dev, "Invalid brightness.\n");
		return -EINVAL;
	}

	if (bright < 0 || bright > 255) {
		dev_err(&dssdev->dev, "Invalid brightness.\n");
		return -EINVAL;
	}

	mutex_lock(&lcd->lock);
	dsi_bus_lock(lcd->dssdev);

	lms501kf07_write_reg_nosync(dssdev, 0x51, bright);

	dsi_bus_unlock(lcd->dssdev);
	mutex_unlock(&lcd->lock);

	dev_dbg(&dssdev->dev, "LCD : brightness is %d\n", bright);

	return len;
}

static char tuning_file_name[50] = { 0, };
static unsigned char mdnie_data[113] = { 0, };

static int set_mdnie(const u8 *data, int size)
{
	struct lms501kf07_data *lcd = gp_lcd;

	if (lcd->enabled == 0)
		return -EINVAL;

	mutex_lock(&lcd->lock);
	/*prevent entry of sleep state in display active state */
	if (lcd->dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		dsi_bus_lock(lcd->dssdev);

		lms501kf07_write_block_nosync(lcd->dssdev, data, size);
		omapdss_dsi_vc_enable_hs(lcd->dssdev, 1, false);

		dsi_bus_unlock(lcd->dssdev);
	}
	mutex_unlock(&lcd->lock);

	return 0;
}

static int parse_text(char *src, int len)
{
	int i, count, ret;
	int index = 0;
	char *str_line[120];
	char *sstart;
	char *c;
	unsigned int data1, data2;

	c = src;
	count = 0;
	sstart = c;

	for (i = 0; i < len; i++, c++) {
		char a = *c;
		if (a == '\r' || a == '\n') {
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

	printk(KERN_INFO
	       "----------------------------- Total number of lines:%d\n",
	       count);

	mdnie_data[index++] = 0xE6;	/* set mDNIe command */

	for (i = 0; i < count; i++) {
		printk(KERN_INFO "line:%d, [start]%s[end]\n", i, str_line[i]);
		ret = sscanf(str_line[i], "0x%x\n", &data2);
		printk(KERN_INFO "Result => [0x%2x] %s\n", data2,
		       (ret == 1) ? "Ok" : "Not available");
		if (ret == 1)
			mdnie_data[index++] = (unsigned char)data2;
	}
	return index;
}

int mdnie_txtbuf_to_parsing(char const *pFilepath)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret, num;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(get_ds());

	printk(KERN_INFO "%s:", pFilepath);

	if (!pFilepath) {
		printk(KERN_ERR
		       "Error : mdnie_txtbuf_to_parsing has invalid filepath.\n");
		goto parse_err;
	}

	filp = filp_open(pFilepath, O_RDONLY, 0);

	if (IS_ERR(filp)) {
		printk(KERN_ERR "file open error:%d\n", (s32) filp);
		goto parse_err;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	/* add cushion : origianl code is 'dp = kmalloc(l, GFP_KERNEL);' */
	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		printk(KERN_INFO "Out of Memory!\n");
		filp_close(filp, current->files);
		goto parse_err;
	}
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);

	if (ret != l) {
		printk(KERN_INFO
		       "<LMS501KF07> Failed to read file (ret = %d)\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		goto parse_err;
	}

	filp_close(filp, current->files);
	set_fs(fs);
	num = parse_text(dp, l);
	if (!num) {
		printk(KERN_ERR "Nothing to parse!\n");
		kfree(dp);
		goto parse_err;
	}

	set_mdnie(mdnie_data, ARRAY_SIZE(mdnie_data));

	kfree(dp);

	num = num / 2;
	return num;

 parse_err:
	return -EPERM;
}

static ssize_t lms501kf07_sysfs_show_tuning(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	char temp[128];

	sprintf(temp, "%s\n", tuning_file_name);
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t lms501kf07_sysfs_store_tuning(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	static bool tuning;

	if (!strncmp(buf, "0", 1)) {
		tuning = false;
		dev_info(dev, "%s :: tuning is disabled.\n", __func__);
	} else if (!strncmp(buf, "1", 1)) {
		tuning = true;
		dev_info(dev, "%s :: tuning is enabled.\n", __func__);
	} else {
		if (!tuning) {
			dev_info(dev, "%s :: Enable tuning first.\n", __func__);
			return count;
		}
		memset(tuning_file_name, 0, sizeof(tuning_file_name));
		strcpy(tuning_file_name, "/sdcard/mdnie/");
		strncat(tuning_file_name, buf, count - 1);

		mdnie_txtbuf_to_parsing(tuning_file_name);

		dev_info(dev, "%s :: %s\n", __func__, tuning_file_name);
	}

	return count;
}

static struct device_attribute lcd_attributes[] = {
	__ATTR(lcd_type, S_IRUGO|S_IWUSR|S_IWGRP,
				lms501kf07_sysfs_show_lcd_type, NULL),
	__ATTR(lcd_power, S_IRUGO|S_IWUSR|S_IWGRP,
				NULL, lms501kf07_sysfs_store_lcd_power),
	__ATTR(tuning, S_IRUGO|S_IWUSR|S_IWGRP,
				NULL, lms501kf07_sysfs_store_tuning),
	__ATTR(lcd_cabc, S_IRUGO|S_IWUSR|S_IWGRP,
				NULL, lms501kf07_sysfs_store_lcd_cabc),
	__ATTR(lcd_cabc_min_bright, S_IRUGO|S_IWUSR|S_IWGRP,
			NULL, lms501kf07_sysfs_store_lcd_cabc_min_bright),
	__ATTR(lcd_bright, S_IRUGO|S_IWUSR|S_IWGRP,
				NULL, lms501kf07_sysfs_store_lcd_bright),
	__ATTR_NULL,
};

static void lms501kf07_framedone_cb(int err, void *data)
{
	struct omap_dss_device *dssdev = data;
	dev_dbg(&dssdev->dev, "framedone, err %d\n", err);
	dsi_bus_unlock(dssdev);
}

static int lms501kf07_update(struct omap_dss_device *dssdev,
			     u16 x, u16 y, u16 w, u16 h)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	int r;
	dev_dbg(&dssdev->dev, "update %d, %d, %d x %d\n", x, y, w, h);

	mutex_lock(&lcd->lock);

	dsi_bus_lock(dssdev);

	if (!lcd->enabled) {
		r = 0;
		goto err;
	}

	r = omap_dsi_prepare_update(dssdev, &x, &y, &w, &h, true);
	if (r)
		goto err;

	/* We use VC(0) for VideoPort Data and VC(1) for commands */
	r = omap_dsi_update(dssdev, 0, x, y, w, h, lms501kf07_framedone_cb,
			    dssdev);
	if (r)
		goto err;

	dsi_bus_unlock(dssdev);
	/* note: no bus_unlock here. unlock is in framedone_cb */
	mutex_unlock(&lcd->lock);
	return 0;
 err:
	dsi_bus_unlock(dssdev);
	mutex_unlock(&lcd->lock);
	return r;
}

static int lms501kf07_sync(struct omap_dss_device *dssdev)
{
	/* TODO? */
	return 0;
}

static int lms501kf07_set_update_mode(struct omap_dss_device *dssdev,
				      enum omap_dss_update_mode mode)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	if (lcd->force_update) {
		if (mode != OMAP_DSS_UPDATE_AUTO)
			return -EINVAL;
	} else {
		if (mode != OMAP_DSS_UPDATE_MANUAL)
			return -EINVAL;
	}

	return 0;
}

static enum omap_dss_update_mode lms501kf07_get_update_mode(
			struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	if (lcd->force_update)
		return OMAP_DSS_UPDATE_AUTO;
	else
		return OMAP_DSS_UPDATE_MANUAL;
}

static ssize_t mode_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	return sprintf(buf, "%d\n", lcd->mdnie_data.mode);
}

static ssize_t mode_store(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t count)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		goto mode_store_err;

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	switch (value) {
	case MDNIE_DYNAMIC:
		dev_info(dev, "%s : MDNIE_DYNAMIC set!\n", __func__);
		break;
	case MDNIE_STANDARD:
		dev_info(dev, "%s : MDNIE_STANDARD set!\n", __func__);
		break;
	case MDNIE_MOVIE:
		dev_info(dev, "%s : MDNIE_MOVIE set!\n", __func__);
		break;
	default:
		goto mode_store_err;
	}

	lcd->mdnie_data.mode = value;

	return count;

mode_store_err:
	dev_err(dev, "%s : MDNIE MODE set error!\n", __func__);
	lcd->mdnie_data.mode = MDNIE_INVALID;

	return -EINVAL;
}

static ssize_t scenario_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	return sprintf(buf, "%d\n", lcd->mdnie_data.scenario);
}

static ssize_t scenario_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	struct lms501kf07_mdnie_data *mdnie = lcd->pdata->mdnie_data;

	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		goto scenario_store_err;

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	switch (value) {
	case MDNIE_UI_MODE:
		dev_info(dev, "%s :: MDNIE_MODE for ui\n", __func__);
		set_mdnie(mdnie->ui, mdnie->size);
		break;
	case MDNIE_GALLERY_MODE:
		dev_info(dev, "%s :: MDNIE_MODE for Gallery\n", __func__);
		set_mdnie(mdnie->gallery, mdnie->size);
		break;
	case MDNIE_VIDEO_MODE:
		dev_info(dev, "%s :: MDNIE_MODE for Video\n", __func__);
		set_mdnie(mdnie->video, mdnie->size);
		break;
	case MDNIE_VIDEO_WARM_MODE:
		dev_info(dev, "%s :: MDNIE_MODE for Video Warm\n", __func__);
		set_mdnie(mdnie->video_warm, mdnie->size);
		break;
	case MDNIE_VIDEO_COLD_MODE:
		dev_info(dev, "%s :: MDNIE_MODE for Video Cold\n", __func__);
		set_mdnie(mdnie->video_cold, mdnie->size);
		break;
	case MDNIE_CAMERA_MODE:
		dev_info(dev, "%s :: MDNIE_MODE for Camera\n", __func__);
		set_mdnie(mdnie->camera, mdnie->size);
		break;
	default:
		goto scenario_store_err;
	}

	lcd->mdnie_data.scenario = value;

	return count;

scenario_store_err:
	dev_err(dev, "%s :: MDNIE SCENARIO FAIL!\n", __func__);
	set_mdnie(mdnie->ui, mdnie->size);
	lcd->mdnie_data.scenario = MDNIE_UI_MODE;

	return -EINVAL;
}

static ssize_t outdoor_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	return sprintf(buf, "%d\n", lcd->mdnie_data.outdoor);
}

static ssize_t outdoor_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		goto outdoor_store_err;

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	switch (value) {
	case MDNIE_DISABLE:
		dev_info(dev, "%s :: MDNIE OUTDOOR DISABLE!\n", __func__);
		break;
	case MDNIE_ENABLE:
		dev_info(dev, "%s :: MDNIE OUTDOOR ENABLE!\n", __func__);
		break;
	default:
		goto outdoor_store_err;
	}

	lcd->mdnie_data.outdoor = value;

	return count;

outdoor_store_err:
	dev_err(dev, "%s :: MDNIE OUTDOOR FAIL!\n", __func__);
	lcd->mdnie_data.outdoor = MDNIE_INVALID;

	return -EINVAL;
}

static ssize_t cabc_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	return sprintf(buf, "%d\n", lcd->mdnie_data.cabc);
}

static ssize_t cabc_store(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t count)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	int cabc_mode = CABC_OFF;	/* 0:off, 1:ui, 2:picture, 3:movie */
	int cabc_min_bright = 0;

	int value = 0;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		goto cabc_store_err;

	switch (value) {
	case CABC_DISABLE:
		cabc_mode = CABC_OFF;
		lcd->bl = lms501kf07_convert_brightness(lcd->plat_bl, false);
		dev_info(dev, "%s :: CABC DISABLE!\n", __func__);
		break;
	case CABC_ENABLE:
		cabc_mode = CABC_DEFAULT;
		lcd->bl = lms501kf07_convert_brightness(lcd->plat_bl, true);
		/* 3/4 => 75% of lcd->bl */
		cabc_min_bright = (lcd->bl * 3) >> 2;
		dev_info(dev, "%s :: CABC ENABLE!\n", __func__);
		break;
	default:
		goto cabc_store_err;
	}

	mutex_lock(&lcd->lock);

	/*prevent entry of sleep state in display active state */
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		dsi_bus_lock(lcd->dssdev);

		lms501kf07_write_reg_nosync(dssdev, 0x5E, cabc_min_bright);
		lms501kf07_write_reg_nosync(dssdev, 0x55, cabc_mode);
		lms501kf07_write_reg_nosync(dssdev, 0x51, lcd->bl);

		dsi_bus_unlock(lcd->dssdev);
	}
	mutex_unlock(&lcd->lock);

	lcd->current_brightness = lcd->bl;
	lcd->mdnie_data.cabc = value;

	dev_info(dev, "%s :: value=%d\n", __func__, value);
	dev_info(dev, "%s :: LDI cabc mode = %d, min bright = %d\n", __func__,
		 cabc_mode, cabc_min_bright);
	dev_info(dev, "%s :: New Brightness (%d -> %d)\n", __func__,
		 lcd->plat_bl, lcd->bl);

	return count;

cabc_store_err:
	dev_err(dev, "%s :: CABC FAIL!\n", __func__);
	lcd->mdnie_data.cabc = CABC_INVALID;
	return -EINVAL;
}

static ssize_t negative_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	return sprintf(buf, "%d\n", lcd->mdnie_data.negative);
}

static ssize_t negative_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);

	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		goto negative_store_err;

	switch (value) {
	case MDNIE_DISABLE:
		dev_info(dev, "%s :: MDNIE NEGATIVE DISABLE!\n", __func__);
		break;
	case MDNIE_ENABLE:
		dev_info(dev, "%s :: MDNIE NEGATIVE ENABLE!\n", __func__);
		break;
	default:
		goto negative_store_err;
	}

	lcd->mdnie_data.negative = value;

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	return count;

negative_store_err:
	dev_err(dev, "%s :: MDNIE NEGATIVE FAIL!\n", __func__);
	lcd->mdnie_data.negative = MDNIE_INVALID;

	return -EINVAL;
}

static ssize_t tunning_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	char temp[128] = {0,};

	if (buf == NULL)
		buf[0] = '\0';

	sprintf(temp, "%s\n", tuning_file_name);
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t tunning_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	static tuning;

	if (!strncmp(buf, "0", 1)) {
		tuning = false;
		dev_info(dev, "%s :: tuning is disabled.\n", __func__);
	} else if (!strncmp(buf, "1", 1)) {
		tuning = true;
		dev_info(dev, "%s :: tuning is enabled.\n", __func__);
	} else {
		if (!tuning) {
			dev_info(dev, "%s :: Enable tuning first.\n", __func__);
			return count;
		}
		memset(tuning_file_name, 0, sizeof(tuning_file_name));
		strcpy(tuning_file_name, "/sdcard/mdnie/");
		strncat(tuning_file_name, buf, count - 1);

		mdnie_txtbuf_to_parsing(tuning_file_name);

		dev_info(dev, "%s :: %s\n", __func__, tuning_file_name);
	}

	return count;
}

static struct device_attribute mdnie_attributes[] = {
	__ATTR(cabc, S_IRUGO|S_IWUSR|S_IWGRP, cabc_show, cabc_store),
	__ATTR(mode, S_IRUGO|S_IWUSR|S_IWGRP, mode_show, mode_store),
	__ATTR(scenario, S_IRUGO|S_IWUSR|S_IWGRP, scenario_show,
							scenario_store),
	__ATTR(outdoor, S_IRUGO|S_IWUSR|S_IWGRP, outdoor_show, outdoor_store),
	__ATTR(negative, S_IRUGO|S_IWUSR|S_IWGRP, negative_show,
							negative_store),
	__ATTR(tunning, S_IRUGO|S_IWUSR|S_IWGRP, tunning_show, tunning_store),
	__ATTR_NULL,
};

static int lms501kf07_probe(struct omap_dss_device *dssdev)
{
	int ret = 0;
	struct backlight_properties props = {
		.brightness = bright_tbl_normal.deflt,
		.max_brightness = 255,
		.type = BACKLIGHT_RAW,
	};
	struct lms501kf07_data *lcd = NULL;

	dev_dbg(&dssdev->dev, "lms501kf07_probe\n");

	if (dssdev->data == NULL) {
		dev_err(&dssdev->dev, "no platform data!\n");
		return -EINVAL;
	}

	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	dssdev->panel.timings = lms501kf07_timings;

	dssdev->ctrl.pixel_size = 24;
	dssdev->panel.acbi = 0;
	dssdev->panel.acb = 40;

	lcd = kzalloc(sizeof(*lcd), GFP_KERNEL);
	if (!lcd)
		return -ENOMEM;

	gp_lcd = lcd;

	lcd->dssdev = dssdev;
	lcd->pdata = dssdev->data;

	lcd->bl = props.brightness;

	ret = gpio_request(lcd->pdata->reset_gpio, "lms501kf07_reset");
	if (ret < 0) {
		dev_err(&dssdev->dev, "gpio_request %d failed!\n",
			lcd->pdata->reset_gpio);
		goto err;
	}
	gpio_direction_output(lcd->pdata->reset_gpio, 1);

	ret = gpio_request(lcd->pdata->blctrl_gpio, "lms501kf07_blctrl");
	if (ret < 0) {
		dev_err(&dssdev->dev, "gpio_request %d failed!\n",
			lcd->pdata->blctrl_gpio);
		goto err;
	}
	gpio_direction_output(lcd->pdata->blctrl_gpio, 1);

	mutex_init(&lcd->lock);

	atomic_set(&lcd->do_update, 0);

	dev_set_drvdata(&dssdev->dev, lcd);

	/* Register DSI backlight  control */
	lcd->bd = backlight_device_register("panel", &dssdev->dev, dssdev,
					    &lms501kf07_backlight_ops, &props);
	if (IS_ERR(lcd->bd)) {
		ret = PTR_ERR(lcd->bd);
		goto err_backlight_device_register;
	}

	lcd->mdnie_class = class_create(THIS_MODULE, "mdnie");
	if (IS_ERR(lcd->lcd_class)) {
		pr_err("Failed to create lcd_class!");
		goto err_class_create_mdnie;
	}
	lcd->mdnie_class->dev_attrs = mdnie_attributes;
	lcd->mdnie_dev =
	    device_create(lcd->mdnie_class, &dssdev->dev, 0, &dssdev->dev,
			  "mdnie");

	/* creaet /sys/class/lcd */
	lcd->lcd_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR(lcd->lcd_class)) {
		pr_err("Failed to create lcd_class!");
		goto err_class_create_lcd;
	}
	lcd->lcd_class->dev_attrs = lcd_attributes;
	lcd->mdnie_dev =
	    device_create(lcd->lcd_class, &dssdev->dev, 0, &dssdev->dev,
			  "panel");

	if (cpu_is_omap44xx())
		lcd->force_update = true;

	dev_dbg(&dssdev->dev, "lms501kf07_probe\n");

	return ret;

 err_class_create_lcd:
	class_destroy(lcd->lcd_class);
 err_class_create_mdnie:
	class_destroy(lcd->mdnie_class);
 err_class_create:
	backlight_device_unregister(lcd->bd);
 err_backlight_device_register:
	mutex_destroy(&lcd->lock);
	gpio_free(lcd->pdata->reset_gpio);
 err:
	kfree(lcd);

	return ret;
}

#ifdef CONFIG_PM
static int lms501kf07_resume(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "resume\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		ret = -EINVAL;
		goto out;
	}

	ret = lms501kf07_start(dssdev);
 out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int lms501kf07_suspend(struct omap_dss_device *dssdev)
{
	struct lms501kf07_data *lcd = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	dev_dbg(&dssdev->dev, "suspend\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		ret = -EINVAL;
		goto out;
	}

	lms501kf07_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
 out:
	mutex_unlock(&lcd->lock);
	return ret;
}
#endif

static struct omap_dss_driver lms501kf07_driver = {
	.probe = lms501kf07_probe,
	.remove = lms501kf07_remove,

	.enable = lms501kf07_enable,
	.disable = lms501kf07_disable,
#ifdef CONFIG_PM
	.suspend = lms501kf07_suspend,
	.resume = lms501kf07_resume,
#endif

	.set_update_mode = lms501kf07_set_update_mode,
	.get_update_mode = lms501kf07_get_update_mode,

	.update = lms501kf07_update,
	.sync = lms501kf07_sync,

	.get_resolution = lms501kf07_get_resolution,
	.get_recommended_bpp = lms501kf07_get_recommended_bpp,

	/* dummy entry start */
	.enable_te = lms501kf07_enable_te,
	.set_rotate = lms501kf07_rotate,
	.get_rotate = lms501kf07_get_rotate,
	.set_mirror = lms501kf07_mirror,
	.get_mirror = lms501kf07_get_mirror,
	/* dummy entry end */

	.get_timings = lms501kf07_get_timings,
	.set_timings = lms501kf07_set_timings,
	.check_timings = lms501kf07_check_timings,

	.driver = {
		   .name = "lms501kf07",
		   .owner = THIS_MODULE,
		   },
};

static int __init lms501kf07_init(void)
{
	omap_dss_register_driver(&lms501kf07_driver);
	return 0;
}

static void __exit lms501kf07_exit(void)
{
	omap_dss_unregister_driver(&lms501kf07_driver);
}

module_init(lms501kf07_init);
module_exit(lms501kf07_exit);

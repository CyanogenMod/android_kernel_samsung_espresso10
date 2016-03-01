/*
 * ltn101al03 LCD panel driver.
 *
 * Author: Donghwa Lee  <dh09.lee@samsung.com>
 *
 * Derived from drivers/video/omap/lcd-apollon.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/serial_core.h>
#include <linux/platform_data/panel-ltn.h>
#include <linux/platform_data/panel-ltn101al03.h>
#include <linux/platform_device.h>
#include <plat/hardware.h>
#include <video/omapdss.h>
#include <asm/mach-types.h>
#include <mach/omap4-common.h>

#include <plat/dmtimer.h>

#define DUTY_DIM	3
#define DUTY_MIN	3
#define DUTY_25		8
#define DUTY_DEFAULT	47
#define DUTY_MAX	81
#define PWM_DUTY_MAX			1600 /* 25kHz */

struct ltn101al03 {
	struct device *dev;
	struct omap_dss_device *dssdev;
	struct ltn_panel_data *pdata;
	bool enabled;
	unsigned int current_brightness;
	unsigned int bl;
	struct mutex lock;
	struct backlight_device *bd;
	struct omap_dm_timer *gptimer;	/*For OMAP4430 "gptimer" */
	struct class *lcd_class;
};

static struct brightness_data ltn101al03_brightness_data;

static void backlight_gptimer_update(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);

	omap_dm_timer_set_load(lcd->gptimer, 1, -PWM_DUTY_MAX);
	omap_dm_timer_set_match(lcd->gptimer, 1,	/* 0~25 */
				-PWM_DUTY_MAX + lcd->current_brightness);
	omap_dm_timer_set_pwm(lcd->gptimer, 0, 1,
			      OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
	omap_dm_timer_enable(lcd->gptimer);
	omap_dm_timer_write_counter(lcd->gptimer, -2);
	omap_dm_timer_disable(lcd->gptimer);

	omap_dm_timer_start(lcd->gptimer);
}

static void backlight_gptimer_stop(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;
	ret = omap_dm_timer_stop(lcd->gptimer);
	if (ret)
		pr_err("failed to stop pwm timer. ret=%d\n", ret);
}

static int backlight_gptimer_init(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	pr_info("(%s): called (@%d)\n", __func__, __LINE__);

	if (lcd->pdata->set_gptimer_idle)
		lcd->pdata->set_gptimer_idle();

	lcd->gptimer =
	    omap_dm_timer_request_specific(lcd->pdata->backlight_gptimer_num);

	if (lcd->gptimer == NULL) {
		pr_err("failed to request pwm timer\n");
		ret = -ENODEV;
		goto err_dm_timer_request;
	}

	ret = omap_dm_timer_set_source(lcd->gptimer, OMAP_TIMER_SRC_SYS_CLK);
	if (ret < 0)
		goto err_dm_timer_src;

	return ret;

err_dm_timer_src:
	omap_dm_timer_free(lcd->gptimer);
	lcd->gptimer = NULL;
err_dm_timer_request:
	return ret;
}

static int ltn101al03_hw_reset(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	pr_info("(%s): called (@%d)\n", __func__, __LINE__);

	gpio_set_value(lcd->pdata->lvds_nshdn_gpio, 0);
	mdelay(1);
	gpio_set_value(lcd->pdata->lvds_nshdn_gpio, 1);
	msleep(300);

	return 0;
}

static int get_gamma_value_from_bl(int bl)
{
	int gamma_value = 0;
	int i;

	if (bl == ltn101al03_brightness_data.platform_value[0])
		gamma_value = ltn101al03_brightness_data.kernel_value[0];
	for (i = 1 ; i < NUM_BRIGHTNESS_LEVEL ; i++) {
		if (bl > ltn101al03_brightness_data.platform_value[i])
			continue;
		else {
			gamma_value =
			 (bl - ltn101al03_brightness_data.platform_value[i-1])
			 * ((PWM_DUTY_MAX
			 * ltn101al03_brightness_data.kernel_value[i])
			 - (PWM_DUTY_MAX
			 * ltn101al03_brightness_data.kernel_value[i-1]))
			 / (ltn101al03_brightness_data.platform_value[i]
			 - ltn101al03_brightness_data.platform_value[i-1])
			 + (PWM_DUTY_MAX
			 * ltn101al03_brightness_data.kernel_value[i-1]);
			break;
		}
	}

	return gamma_value/100;
}

static void update_brightness(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);

	lcd->current_brightness = lcd->bl;

	if (lcd->current_brightness == BRIGHTNESS_OFF)
		backlight_gptimer_stop(dssdev);
	else
		backlight_gptimer_update(dssdev);

}

static int ltn101al03_power_on(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	pr_info("(%s): called (@%d)\n", __func__, __LINE__);

	if (lcd->enabled != 1) {
		if (lcd->pdata->set_power)
			lcd->pdata->set_power(true);

		ret = omapdss_dpi_display_enable(dssdev);
		if (ret) {
			dev_err(&dssdev->dev, "failed to enable DPI\n");
			goto err;
		}

		/* reset ltn101al03 bridge */
		if (!dssdev->skip_init) {
			ltn101al03_hw_reset(dssdev);

			msleep(100);

			gpio_set_value(lcd->pdata->led_backlight_reset_gpio, 1);
			mdelay(10);
			omap_dm_timer_start(lcd->gptimer);

			usleep_range(2000, 2100);
			update_brightness(dssdev);
		}

		lcd->enabled = 1;
	}

	if (dssdev->skip_init)
		dssdev->skip_init = false;

err:
	return ret;
}

static int ltn101al03_power_off(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);

	lcd->enabled = 0;

	gpio_set_value(lcd->pdata->led_backlight_reset_gpio, 0);

	backlight_gptimer_stop(dssdev);
	msleep(250);

	gpio_set_value(lcd->pdata->lvds_nshdn_gpio, 0);

	omapdss_dpi_display_disable(dssdev);

	if (lcd->pdata->set_power)
		lcd->pdata->set_power(false);

	msleep(300);

	return 0;
}

static int ltn101al03_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int ltn101al03_set_brightness(struct backlight_device *bd)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(&bd->dev);
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	int bl = bd->props.brightness;
	int ret = 0;

	if (bl < BRIGHTNESS_OFF)
		bl = BRIGHTNESS_OFF;
	else if (bl > BRIGHTNESS_MAX)
		bl = BRIGHTNESS_MAX;

	lcd->bl = get_gamma_value_from_bl(bl);

	mutex_lock(&lcd->lock);
	if ((dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) &&
		(lcd->enabled) &&
		(lcd->current_brightness != lcd->bl)) {
		update_brightness(dssdev);
		dev_info(&bd->dev, "[%d] brightness=%d, bl=%d\n",
			 lcd->pdata->panel_id, bd->props.brightness, lcd->bl);
	}
	mutex_unlock(&lcd->lock);
	return ret;
}

static const struct backlight_ops ltn101al03_backlight_ops = {
	.get_brightness = ltn101al03_get_brightness,
	.update_status = ltn101al03_set_brightness,
};

static int ltn101al03_start(struct omap_dss_device *dssdev);
static void ltn101al03_stop(struct omap_dss_device *dssdev);

static ssize_t lcd_type_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{

	char temp[15];
	sprintf(temp, "SMD_LTN101AL03\n");
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, S_IRUGO, lcd_type_show, NULL);

static ssize_t ltn101al03_sysfs_store_lcd_power(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t len)
{
	struct omap_dss_device *dssdev = to_dss_device(dev_get_drvdata(dev));
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;
	int rc;
	int lcd_enable;

	rc = kstrtoint(buf, 0, &lcd_enable);
	if (rc < 0)
		return rc;

	dev_info(dev, "ltn101al03_sysfs_store_lcd_power - %d\n", lcd_enable);

	mutex_lock(&lcd->lock);
	if (lcd_enable) {
		if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
			ret = -EINVAL;
			goto out;
		}
		ret = ltn101al03_start(dssdev);
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	} else {
		if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
			ret = -EINVAL;
			goto out;
		}
		ltn101al03_stop(dssdev);
		dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
	}

out:
	mutex_unlock(&lcd->lock);
	return len;
}

static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR | S_IWGRP
	, NULL, ltn101al03_sysfs_store_lcd_power);

static int ltn101al03_panel_probe(struct omap_dss_device *dssdev)
{
	int ret = 0;
	struct ltn101al03 *lcd = NULL;

	struct backlight_properties props = {
		.brightness = BRIGHTNESS_DEFAULT,
		.max_brightness = 255,
		.type = BACKLIGHT_RAW,
	};
	pr_info("(%s): called (@%d)\n", __func__, __LINE__);
	dev_dbg(&dssdev->dev, "ltn101al03_probe\n");

	lcd = kzalloc(sizeof(*lcd), GFP_KERNEL);
	if (!lcd)
		return -ENOMEM;

	if (dssdev->data == NULL) {
		dev_err(&dssdev->dev, "no platform data!\n");
		ret = -EINVAL;
		goto err_no_platform_data;
	}

	dssdev->panel.config = OMAP_DSS_LCD_TFT
			     | OMAP_DSS_LCD_IVS
			     /*| OMAP_DSS_LCD_IEO */
			     | OMAP_DSS_LCD_IPC
			     | OMAP_DSS_LCD_IHS
			     | OMAP_DSS_LCD_ONOFF;

	dssdev->panel.acb = 0;

	lcd->dssdev = dssdev;
	lcd->pdata = dssdev->data;

	ltn101al03_brightness_data = lcd->pdata->brightness_table;

	lcd->bl = get_gamma_value_from_bl(props.brightness);

	ret = gpio_request(lcd->pdata->lvds_nshdn_gpio, "lvds_nshdn");
	if (ret < 0) {
		dev_err(&dssdev->dev, "gpio_request %d failed!\n",
			lcd->pdata->lvds_nshdn_gpio);
		goto err_no_platform_data;
	}
	gpio_direction_output(lcd->pdata->lvds_nshdn_gpio, 1);

	ret = gpio_request(lcd->pdata->led_backlight_reset_gpio,
		"led_backlight_reset");
	if (ret < 0) {
		dev_err(&dssdev->dev, "gpio_request %d failed!\n",
			lcd->pdata->led_backlight_reset_gpio);
		goto err_backlight_reset_gpio_request;
	}
	gpio_direction_output(lcd->pdata->led_backlight_reset_gpio, 1);

	mutex_init(&lcd->lock);

	dev_set_drvdata(&dssdev->dev, lcd);

	/* Register DSI backlight  control */
	lcd->bd = backlight_device_register("panel", &dssdev->dev, dssdev,
					    &ltn101al03_backlight_ops, &props);
	if (IS_ERR(lcd->bd)) {
		ret = PTR_ERR(lcd->bd);
		goto err_backlight_device_register;
	}

	lcd->lcd_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR(lcd->lcd_class)) {
		pr_err("Failed to create lcd_class!");
		goto err_class_create;
	}

	lcd->dev = device_create(lcd->lcd_class, NULL, 0, NULL, "panel");
	if (IS_ERR(lcd->dev)) {
		pr_err("Failed to create device(panel)!\n");
		goto err_device_create;
	}

	dev_set_drvdata(lcd->dev, &dssdev->dev);

	ret = device_create_file(lcd->dev, &dev_attr_lcd_type);
	if (ret < 0) {
		dev_err(&dssdev->dev,
			"failed to add 'lcd_type' sysfs entries\n");
		goto err_lcd_device;
	}
	ret = device_create_file(lcd->dev, &dev_attr_lcd_power);
	if (ret < 0) {
		dev_err(&dssdev->dev,
			"failed to add 'lcd_power' sysfs entries\n");
		goto err_lcd_type;
	}

	ret = backlight_gptimer_init(dssdev);
	if (ret < 0) {
		dev_err(&dssdev->dev,
			"backlight_gptimer_init failed!\n");
		goto err_gptimer_init;
	}

	/*
	 * if lcd panel was on from bootloader like u-boot then
	 * do not lcd on.
	 */
	if (dssdev->skip_init)
		lcd->enabled = 1;

	update_brightness(dssdev);

	dev_dbg(&dssdev->dev, "%s\n", __func__);
	return ret;

err_gptimer_init:
	device_remove_file(&(lcd->bd->dev), &dev_attr_lcd_power);
err_lcd_type:
	device_remove_file(&(lcd->bd->dev), &dev_attr_lcd_type);
err_lcd_device:
	device_destroy(lcd->lcd_class, 0);
err_device_create:
	class_destroy(lcd->lcd_class);
err_class_create:
	backlight_device_unregister(lcd->bd);
err_backlight_device_register:
	mutex_destroy(&lcd->lock);
	gpio_free(lcd->pdata->led_backlight_reset_gpio);
err_backlight_reset_gpio_request:
	gpio_free(lcd->pdata->lvds_nshdn_gpio);
err_no_platform_data:
	kfree(lcd);

	return ret;
}

static void ltn101al03_panel_remove(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	device_remove_file(&(lcd->bd->dev), &dev_attr_lcd_power);
	device_remove_file(&(lcd->bd->dev), &dev_attr_lcd_type);
	device_destroy(lcd->lcd_class, 0);
	class_destroy(lcd->lcd_class);
	backlight_device_unregister(lcd->bd);
	mutex_destroy(&lcd->lock);
	gpio_free(lcd->pdata->led_backlight_reset_gpio);
	gpio_free(lcd->pdata->lvds_nshdn_gpio);
	kfree(lcd);
}

static int ltn101al03_start(struct omap_dss_device *dssdev)
{
	int r = 0;

	r = ltn101al03_power_on(dssdev);

	if (r) {
		dev_dbg(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
		dssdev->manager->enable(dssdev->manager);
	}

	return r;
}

static void ltn101al03_stop(struct omap_dss_device *dssdev)
{
	dssdev->manager->disable(dssdev->manager);

	ltn101al03_power_off(dssdev);
}

static int ltn101al03_panel_enable(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "enable\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		ret = -EINVAL;
		goto out;
	}

	ret = ltn101al03_start(dssdev);
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void ltn101al03_panel_disable(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		ltn101al03_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	mutex_unlock(&lcd->lock);
}

static int ltn101al03_panel_suspend(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	pr_info("Enter ltn101al03_panel_suspend\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		ret = -EINVAL;
		goto out;
	}

	ltn101al03_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int ltn101al03_panel_resume(struct omap_dss_device *dssdev)
{
	struct ltn101al03 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	pr_info("Enter ltn101al03_panel_resume\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		ret = -EINVAL;
		goto out;
	}

	ret = ltn101al03_start(dssdev);
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void ltn101al03_panel_get_resolution(struct omap_dss_device *dssdev,
					    u16 *xres, u16 *yres)
{

	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}

static void ltn101al03_panel_set_timings(struct omap_dss_device *dssdev,
					 struct omap_video_timings *timings)
{
	dpi_set_timings(dssdev, timings);
}

static void ltn101al03_panel_get_timings(struct omap_dss_device *dssdev,
					 struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int ltn101al03_panel_check_timings(struct omap_dss_device *dssdev,
					  struct omap_video_timings *timings)
{
	return dpi_check_timings(dssdev, timings);
}

static struct omap_dss_driver ltn101al03_omap_dss_driver = {
	.probe		= ltn101al03_panel_probe,
	.remove		= ltn101al03_panel_remove,

	.enable		= ltn101al03_panel_enable,
	.disable	= ltn101al03_panel_disable,
	.get_resolution	= ltn101al03_panel_get_resolution,
	.suspend	= ltn101al03_panel_suspend,
	.resume		= ltn101al03_panel_resume,

	.set_timings	= ltn101al03_panel_set_timings,
	.get_timings	= ltn101al03_panel_get_timings,
	.check_timings	= ltn101al03_panel_check_timings,

	.driver = {
		.name	= "ltn101al03_panel",
		.owner	= THIS_MODULE,
	},
};

static int __init ltn101al03_init(void)
{
	omap_dss_register_driver(&ltn101al03_omap_dss_driver);

	return 0;
}

static void __exit ltn101al03_exit(void)
{
	omap_dss_unregister_driver(&ltn101al03_omap_dss_driver);
}

module_init(ltn101al03_init);
module_exit(ltn101al03_exit);

MODULE_AUTHOR("Donghwa Lee <dh09.lee@samsung.com>");
MODULE_DESCRIPTION("ltn101al03 LCD Driver");
MODULE_LICENSE("GPL");

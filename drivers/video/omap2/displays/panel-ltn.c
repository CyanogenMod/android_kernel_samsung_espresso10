/*
 * Samsung LTN070NL01/LTN101AL03 LCD panel driver.
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
#include <linux/platform_device.h>
#include <plat/hardware.h>
#include <video/omapdss.h>
#include <asm/mach-types.h>
#include <mach/omap4-common.h>

#include <plat/dmtimer.h>

struct ltn {
	struct device *dev;
	struct omap_dss_device *dssdev;
	struct ltn_panel_data *pdata;
	bool enabled;
	unsigned int current_brightness;
	unsigned int bl;
	struct mutex lock;
	struct backlight_device *bd;
	struct omap_dm_timer *gptimer;	/*For OMAP4430 "gptimer" */
};

static struct brightness_data ltn_brightness_data;

static void backlight_gptimer_update(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "%s\n", __func__);

	omap_dm_timer_set_load(lcd->gptimer, 1, -lcd->pdata->pwm_duty_max);
	omap_dm_timer_set_match(lcd->gptimer, 1,	/* 0~25 */
				-lcd->pdata->pwm_duty_max + lcd->current_brightness);
	omap_dm_timer_set_pwm(lcd->gptimer, 0, 1,
			      OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
	omap_dm_timer_enable(lcd->gptimer);
	omap_dm_timer_write_counter(lcd->gptimer, -2);
	omap_dm_timer_disable(lcd->gptimer);

	omap_dm_timer_start(lcd->gptimer);
}

static void backlight_gptimer_stop(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "%s\n", __func__);

	ret = omap_dm_timer_stop(lcd->gptimer);
	if (ret)
		dev_err(&dssdev->dev, "failed to stop pwm timer. ret=%d\n", ret);
}

static int backlight_gptimer_init(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "%s\n", __func__);

	if (lcd->pdata->set_gptimer_idle)
		lcd->pdata->set_gptimer_idle();

	lcd->gptimer =
	    omap_dm_timer_request_specific(lcd->pdata->backlight_gptimer_num);

	if (lcd->gptimer == NULL) {
		dev_err(&dssdev->dev, "failed to request pwm timer\n");
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

static int ltn_hw_reset(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "hw_reset\n");

	gpio_set_value(lcd->pdata->led_backlight_reset_gpio, 0);
	mdelay(1);
	gpio_set_value(lcd->pdata->led_backlight_reset_gpio, 1);
	usleep_range(10000, 11000);

	gpio_set_value(lcd->pdata->lvds_nshdn_gpio, 0);
	mdelay(1);
	gpio_set_value(lcd->pdata->lvds_nshdn_gpio, 1);
	msleep(200);

	return 0;
}

static int get_gamma_value_from_bl(struct omap_dss_device *dssdev, int bl)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	int gamma_value = 0;
	int i;

	if (bl == ltn_brightness_data.platform_value[0])
		gamma_value = ltn_brightness_data.kernel_value[0];
	for (i = 1 ; i < NUM_BRIGHTNESS_LEVEL ; i++) {
		if (bl > ltn_brightness_data.platform_value[i])
			continue;
		else {
			gamma_value =
			 (bl - ltn_brightness_data.platform_value[i-1])
			 * ((lcd->pdata->pwm_duty_max
			 * ltn_brightness_data.kernel_value[i])
			 - (lcd->pdata->pwm_duty_max
			 * ltn_brightness_data.kernel_value[i-1]))
			 / (ltn_brightness_data.platform_value[i]
			 - ltn_brightness_data.platform_value[i-1])
			 + (lcd->pdata->pwm_duty_max
			 * ltn_brightness_data.kernel_value[i-1]);
			break;
		}
	}

	return gamma_value/100;
}

static void update_brightness(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);

	lcd->current_brightness = lcd->bl;

	if (lcd->current_brightness == BRIGHTNESS_OFF)
		backlight_gptimer_stop(dssdev);
	else
		backlight_gptimer_update(dssdev);
}

static int ltn_power_on(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	dev_dbg(&dssdev->dev, "%s\n", __func__);

	if (lcd->enabled != 1) {
		if (lcd->pdata->set_power)
			lcd->pdata->set_power(true);

		ret = omapdss_dpi_display_enable(dssdev);
		if (ret) {
			dev_err(&dssdev->dev, "failed to enable DPI\n");
			goto err;
		}

		/* reset ltn bridge */
		if (!dssdev->skip_init) {
			ltn_hw_reset(dssdev);

			msleep(100);
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

static int ltn_power_off(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "power_off\n");

	lcd->enabled = 0;

	if (lcd->bl != BRIGHTNESS_OFF) {
		backlight_gptimer_stop(dssdev);
		msleep(200);
	}

	gpio_set_value(lcd->pdata->lvds_nshdn_gpio, 0);

	omapdss_dpi_display_disable(dssdev);

	gpio_set_value(lcd->pdata->led_backlight_reset_gpio, 0);

	if (lcd->pdata->set_power)
		lcd->pdata->set_power(false);

	msleep(300);

	return 0;
}

static int ltn_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int ltn_set_brightness(struct backlight_device *bd)
{
	struct omap_dss_device *dssdev = dev_get_drvdata(&bd->dev);
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	int bl = bd->props.brightness;
	int ret = 0;

	if (bl < BRIGHTNESS_OFF)
		bl = BRIGHTNESS_OFF;
	else if (bl > BRIGHTNESS_MAX)
		bl = BRIGHTNESS_MAX;

	lcd->bl = get_gamma_value_from_bl(dssdev, bl);

	mutex_lock(&lcd->lock);
	if ((dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) &&
		(lcd->enabled) &&
		(lcd->current_brightness != lcd->bl)) {
		update_brightness(dssdev);
		dev_dbg(&bd->dev, "[%d] brightness=%d, bl=%d\n",
			 lcd->pdata->panel_id, bd->props.brightness, lcd->bl);
	}
	mutex_unlock(&lcd->lock);
	return ret;
}

static const struct backlight_ops ltn_backlight_ops = {
	.get_brightness = ltn_get_brightness,
	.update_status = ltn_set_brightness,
};

static int ltn_start(struct omap_dss_device *dssdev);
static void ltn_stop(struct omap_dss_device *dssdev);

static int ltn_panel_probe(struct omap_dss_device *dssdev)
{
	int ret = 0;
	struct ltn *lcd = NULL;

	struct backlight_properties props = {
		.brightness = BRIGHTNESS_DEFAULT,
		.max_brightness = 255,
		.type = BACKLIGHT_RAW,
	};

	dev_dbg(&dssdev->dev, "probing\n");

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

	ltn_brightness_data = lcd->pdata->brightness_table;

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

	lcd->bl = get_gamma_value_from_bl(dssdev, props.brightness);

	/* Register DSI backlight  control */
	lcd->bd = backlight_device_register("panel", &dssdev->dev, dssdev,
					    &ltn_backlight_ops, &props);
	if (IS_ERR(lcd->bd)) {
		ret = PTR_ERR(lcd->bd);
		goto err_backlight_device_register;
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

	dev_dbg(&dssdev->dev, "probed\n");
	return ret;

err_gptimer_init:
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

static void ltn_panel_remove(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	backlight_device_unregister(lcd->bd);
	mutex_destroy(&lcd->lock);
	gpio_free(lcd->pdata->led_backlight_reset_gpio);
	gpio_free(lcd->pdata->lvds_nshdn_gpio);
	kfree(lcd);
}

static int ltn_start(struct omap_dss_device *dssdev)
{
	int r = 0;

	dev_dbg(&dssdev->dev, "start\n");

	r = ltn_power_on(dssdev);

	if (r) {
		dev_dbg(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
		dssdev->manager->enable(dssdev->manager);
	}

	return r;
}

static void ltn_stop(struct omap_dss_device *dssdev)
{
	dev_dbg(&dssdev->dev, "stop\n");

	dssdev->manager->disable(dssdev->manager);

	ltn_power_off(dssdev);
}

static int ltn_panel_enable(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "enable\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		ret = -EINVAL;
		goto out;
	}

	ret = ltn_start(dssdev);
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void ltn_panel_disable(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		ltn_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	mutex_unlock(&lcd->lock);
}

static int ltn_panel_suspend(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	dev_dbg(&dssdev->dev, "suspend\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		ret = -EINVAL;
		goto out;
	}

	ltn_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int ltn_panel_resume(struct omap_dss_device *dssdev)
{
	struct ltn *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	dev_dbg(&dssdev->dev, "resume\n");

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		ret = -EINVAL;
		goto out;
	}

	ret = ltn_start(dssdev);
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void ltn_panel_get_resolution(struct omap_dss_device *dssdev,
					    u16 *xres, u16 *yres)
{

	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}

static void ltn_panel_set_timings(struct omap_dss_device *dssdev,
					 struct omap_video_timings *timings)
{
	dpi_set_timings(dssdev, timings);
}

static void ltn_panel_get_timings(struct omap_dss_device *dssdev,
					 struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int ltn_panel_check_timings(struct omap_dss_device *dssdev,
					  struct omap_video_timings *timings)
{
	return dpi_check_timings(dssdev, timings);
}

static struct omap_dss_driver ltn_omap_dss_driver = {
	.probe		= ltn_panel_probe,
	.remove		= ltn_panel_remove,

	.enable		= ltn_panel_enable,
	.disable	= ltn_panel_disable,
	.get_resolution	= ltn_panel_get_resolution,
	.suspend	= ltn_panel_suspend,
	.resume		= ltn_panel_resume,

	.set_timings	= ltn_panel_set_timings,
	.get_timings	= ltn_panel_get_timings,
	.check_timings	= ltn_panel_check_timings,

	.driver = {
		.name	= "ltn_panel",
		.owner	= THIS_MODULE,
	},
};

static int __init ltn_init(void)
{
	omap_dss_register_driver(&ltn_omap_dss_driver);

	return 0;
}

static void __exit ltn_exit(void)
{
	omap_dss_unregister_driver(&ltn_omap_dss_driver);
}

module_init(ltn_init);
module_exit(ltn_exit);

MODULE_AUTHOR("Donghwa Lee <dh09.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung LTN070NL01/LTN101AL03 LCD Driver");
MODULE_LICENSE("GPL");

/*
 * NT71391 LCD panel driver.
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
 * Author: Seungjin Kim <nayaksj.kim@samsung.com>
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
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/lcd.h>
#include <linux/serial_core.h>
#include <linux/platform_data/panel-nt71391.h>
#include <linux/platform_device.h>
#include <video/omapdss.h>
#include <video/mipi_display.h>
#include <asm/mach-types.h>
#include <mach/omap4-common.h>

struct nt71391 {
	struct device *dev;
	struct omap_dss_device *dssdev;
	struct nt71391_panel_data *pdata;
	unsigned int current_brightness;
	unsigned int bl;
	struct mutex lock;
};

static int nt71391_panel_probe(struct omap_dss_device *dssdev)
{
	struct nt71391 *lcd = NULL;

	if (dssdev->data == NULL) {
		dev_err(&dssdev->dev, "no platform data!\n");
		return -EINVAL;
	}

	lcd = kzalloc(sizeof(*lcd), GFP_KERNEL);
	if (!lcd)
		return -ENOMEM;

	dssdev->panel.config = OMAP_DSS_LCD_TFT
			     | OMAP_DSS_LCD_IVS
			     | OMAP_DSS_LCD_IPC
			     | OMAP_DSS_LCD_IHS
			     | OMAP_DSS_LCD_ONOFF;
	dssdev->panel.acb = 0;

	lcd->dssdev = dssdev;
	lcd->pdata = dssdev->data;
	mutex_init(&lcd->lock);

	dev_set_drvdata(&dssdev->dev, lcd);

	return 0;
}

static void nt71391_panel_remove(struct omap_dss_device *dssdev)
{
	struct nt71391 *lcd = dev_get_drvdata(&dssdev->dev);

	mutex_destroy(&lcd->lock);
	kfree(lcd);
}

static int nt71391_power_on(struct omap_dss_device *dssdev)
{
	struct nt71391 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	/* At power on the first vsync has not been received yet*/
	dssdev->first_vsync = false;

	/* VDD 3.3V power on */
	if (lcd->pdata->set_power)
		lcd->pdata->set_power(true);

	usleep_range(10000, 11000);

	/* VIDEO ON */
	ret = omapdss_dsi_display_enable(dssdev);
	if (ret) {
		pr_err("Failed to enable DSI\n");
		return ret;
	}

	/* reset s6e8aa0 bridge */
	if (!dssdev->skip_init) {
		/* DSI_DT_PXLSTREAM_24BPP_PACKED; */
		dsi_video_mode_enable(dssdev, MIPI_DSI_PACKED_PIXEL_STREAM_24);
	}

	if (dssdev->skip_init)
		dssdev->skip_init = false;

	return 0;
}

static void nt71391_power_off(struct omap_dss_device *dssdev)
{
	struct nt71391 *lcd = dev_get_drvdata(&dssdev->dev);

	omapdss_dsi_display_disable(dssdev, 0, 0);

	if (lcd->pdata->set_power)
		lcd->pdata->set_power(false);

	msleep(330);
}

static int nt71391_start(struct omap_dss_device *dssdev)
{
	int ret;

	dsi_bus_lock(dssdev);
	ret = nt71391_power_on(dssdev);
	dsi_bus_unlock(dssdev);

	if (ret) {
		dev_dbg(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
		dssdev->manager->enable(dssdev->manager);
	}

	return ret;
}

static void nt71391_stop(struct omap_dss_device *dssdev)
{
	/* manager disable */
	dssdev->manager->disable(dssdev->manager);

	dsi_bus_lock(dssdev);
	nt71391_power_off(dssdev);
	dsi_bus_unlock(dssdev);
}

static int nt71391_panel_enable(struct omap_dss_device *dssdev)
{
	struct nt71391 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		ret = -EINVAL;
		goto out;
	}

	ret = nt71391_start(dssdev);
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void nt71391_panel_disable(struct omap_dss_device *dssdev)
{
	struct nt71391 *lcd = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&lcd->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		nt71391_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	mutex_unlock(&lcd->lock);
}

static int nt71391_panel_suspend(struct omap_dss_device *dssdev)
{
	struct nt71391 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		ret = -EINVAL;
		goto out;
	}

	nt71391_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int nt71391_panel_resume(struct omap_dss_device *dssdev)
{
	struct nt71391 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		ret = -EINVAL;
		goto out;
	}

	ret = nt71391_start(dssdev);
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void nt71391_panel_get_resolution(struct omap_dss_device *dssdev,
					    u16 *xres, u16 *yres)
{
	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}

static void nt71391_panel_set_timings(struct omap_dss_device *dssdev,
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

static void nt71391_panel_get_timings(struct omap_dss_device *dssdev,
					 struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int nt71391_panel_check_timings(struct omap_dss_device *dssdev,
					  struct omap_video_timings *timings)
{
	return 0;
}

static struct omap_dss_driver nt71391_omap_dss_driver = {
	.probe		= nt71391_panel_probe,
	.remove		= nt71391_panel_remove,

	.enable		= nt71391_panel_enable,
	.disable	= nt71391_panel_disable,
	.get_resolution	= nt71391_panel_get_resolution,
	.suspend	= nt71391_panel_suspend,
	.resume		= nt71391_panel_resume,

	.set_timings	= nt71391_panel_set_timings,
	.get_timings	= nt71391_panel_get_timings,
	.check_timings	= nt71391_panel_check_timings,

	.driver = {
		.name	= "nt71391_panel",
		.owner	= THIS_MODULE,
	},
};

static int __init nt71391_init(void)
{
	omap_dss_register_driver(&nt71391_omap_dss_driver);

	return 0;
}

static void __exit nt71391_exit(void)
{
	omap_dss_unregister_driver(&nt71391_omap_dss_driver);
}

module_init(nt71391_init);
module_exit(nt71391_exit);

MODULE_AUTHOR("Seungjin Kim <nayaksj.kim@samsung.com>");
MODULE_DESCRIPTION("nt71391 LCD Driver");
MODULE_LICENSE("GPL");

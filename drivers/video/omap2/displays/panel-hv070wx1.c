/*
 * hv070wx1 LCD panel driver.
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

#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/lcd.h>
#include <linux/serial_core.h>
#include <linux/platform_data/panel-hv070wx1.h>
#include <linux/platform_device.h>
#include <video/omapdss.h>
#include <asm/mach-types.h>
#include <mach/omap4-common.h>
#include <video/mipi_display.h>

struct hv070wx1 {
	struct device *dev;
	struct omap_dss_device *dssdev;
	struct hv070wx1_panel_data *pdata;
	unsigned int current_brightness;
	unsigned int bl;
	struct mutex lock;
};

static int hv070wx1_write_reg(struct omap_dss_device *dssdev, u8 reg, u8 val)
{
	u8 buf[2];
	buf[0] = reg;
	buf[1] = val;

	return dsi_vc_dcs_write(dssdev, 1, buf, 2);
}

static int hv070wx1_panel_probe(struct omap_dss_device *dssdev)
{
	int ret;
	struct hv070wx1 *lcd = NULL;

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

static void hv070wx1_panel_remove(struct omap_dss_device *dssdev)
{
	struct hv070wx1 *lcd = dev_get_drvdata(&dssdev->dev);

	mutex_destroy(&lcd->lock);
	kfree(lcd);
}

static void hv070wx1_config(struct omap_dss_device *dssdev)
{
	hv070wx1_write_reg(dssdev, 0xAE, 0x0D);	/* Control setting */
	hv070wx1_write_reg(dssdev, 0xB0, 0xF8);	/* Reverse setting */
}

static int hv070wx1_power_on(struct omap_dss_device *dssdev)
{
	struct hv070wx1 *lcd = dev_get_drvdata(&dssdev->dev);
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
		hv070wx1_config(dssdev);

		/* DSI_DT_PXLSTREAM_24BPP_PACKED; */
		dsi_video_mode_enable(dssdev, MIPI_DSI_PACKED_PIXEL_STREAM_24);
	}

	if (dssdev->skip_init)
		dssdev->skip_init = false;

	return 0;
}

static void hv070wx1_power_off(struct omap_dss_device *dssdev)
{
	struct hv070wx1 *lcd = dev_get_drvdata(&dssdev->dev);

	omapdss_dsi_display_disable(dssdev, 0, 0);

	if (lcd->pdata->set_power)
		lcd->pdata->set_power(false);

	msleep(330);
}

static int hv070wx1_start(struct omap_dss_device *dssdev)
{
	int ret;

	dsi_bus_lock(dssdev);
	ret = hv070wx1_power_on(dssdev);
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

static void hv070wx1_stop(struct omap_dss_device *dssdev)
{
	/* manager disable */
	dssdev->manager->disable(dssdev->manager);

	dsi_bus_lock(dssdev);
	hv070wx1_power_off(dssdev);
	dsi_bus_unlock(dssdev);
}

static int hv070wx1_panel_enable(struct omap_dss_device *dssdev)
{
	struct hv070wx1 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		ret = -EINVAL;
		goto out;
	}

	ret = hv070wx1_start(dssdev);
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void hv070wx1_panel_disable(struct omap_dss_device *dssdev)
{
	struct hv070wx1 *lcd = dev_get_drvdata(&dssdev->dev);

	mutex_lock(&lcd->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		hv070wx1_stop(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	mutex_unlock(&lcd->lock);
}

static int hv070wx1_panel_suspend(struct omap_dss_device *dssdev)
{
	struct hv070wx1 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		ret = -EINVAL;
		goto out;
	}

	hv070wx1_stop(dssdev);
	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int hv070wx1_panel_resume(struct omap_dss_device *dssdev)
{
	struct hv070wx1 *lcd = dev_get_drvdata(&dssdev->dev);
	int ret;

	mutex_lock(&lcd->lock);
	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		ret = -EINVAL;
		goto out;
	}

	ret = hv070wx1_start(dssdev);
out:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void hv070wx1_panel_get_resolution(struct omap_dss_device *dssdev,
					    u16 *xres, u16 *yres)
{
	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}

static void hv070wx1_panel_set_timings(struct omap_dss_device *dssdev,
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

static void hv070wx1_panel_get_timings(struct omap_dss_device *dssdev,
					 struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int hv070wx1_panel_check_timings(struct omap_dss_device *dssdev,
					  struct omap_video_timings *timings)
{
	return 0;
}

static struct omap_dss_driver hv070wx1_omap_dss_driver = {
	.probe		= hv070wx1_panel_probe,
	.remove		= hv070wx1_panel_remove,

	.enable		= hv070wx1_panel_enable,
	.disable	= hv070wx1_panel_disable,
	.get_resolution	= hv070wx1_panel_get_resolution,
	.suspend	= hv070wx1_panel_suspend,
	.resume		= hv070wx1_panel_resume,

	.set_timings	= hv070wx1_panel_set_timings,
	.get_timings	= hv070wx1_panel_get_timings,
	.check_timings	= hv070wx1_panel_check_timings,

	.driver = {
		.name	= "hv070wx1_panel",
		.owner	= THIS_MODULE,
	},
};

static int __init hv070wx1_init(void)
{
	omap_dss_register_driver(&hv070wx1_omap_dss_driver);

	return 0;
}

static void __exit hv070wx1_exit(void)
{
	omap_dss_unregister_driver(&hv070wx1_omap_dss_driver);
}

module_init(hv070wx1_init);
module_exit(hv070wx1_exit);

MODULE_AUTHOR("Seungjin Kim <nayaksj.kim@samsung.com>");
MODULE_DESCRIPTION("hv070wx1 LCD Driver");
MODULE_LICENSE("GPL");

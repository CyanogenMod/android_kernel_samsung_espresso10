/* arch/arm/mach-omap2/board-gokey-sdio.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
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

#include <linux/i2c/twl.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <plat/irqs.h>
#include <plat/mmc.h>

#include "board-gokey.h"
#include <linux/gpio.h>
#include "mux.h"
#include "omap_muxtbl.h"
#include "hsmmc.h"

/*for sysfs to update sd detect pin's status*/
static struct device *sd_detection_cmd_dev;

static struct omap2_hsmmc_info gokey_mmc_info[] = {
	{
	 .mmc = 2,
	 .nonremovable = true,
	 .external_ldo = true,
	 .caps = MMC_CAP_8_BIT_DATA | MMC_CAP_1_8V_DDR | MMC_CAP_UHS_DDR50,
	 .ocr_mask = MMC_VDD_165_195,
	 .gpio_wp = -EINVAL,
	 .gpio_cd = -EINVAL,
#ifdef CONFIG_PM_RUNTIME
	 .power_saving = false,
#endif
	 },
	{
	 .mmc = 1,
	 .external_ldo = true,
	 .caps = MMC_CAP_8_BIT_DATA,
	 .gpio_wp = -EINVAL,
	 .gpio_cd = -EINVAL,
	 .ocr_mask = MMC_VDD_30_31,
#ifdef CONFIG_PM_RUNTIME
	 .power_saving = false,
#endif
	 },
	{
	 .name = "omap_wlan",
	 .mmc = 5,
	 .caps = MMC_CAP_4_BIT_DATA,
	 .gpio_wp = -EINVAL,
	 .gpio_cd = -EINVAL,
	 .ocr_mask = MMC_VDD_165_195 | MMC_VDD_20_21,
	 .nonremovable = false,
	 .mmc_data = &gokey_wifi_data,
	 },
	{}			/* Terminator */
};

static ssize_t sd_detection_cmd_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int ret = -EIO;
	u8 read_reg = 0;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0, &read_reg, TWL6030_MMCCTRL);

	if (ret >= 0)
		ret = read_reg & STS_MMC;

	if (ret) {
		pr_debug("SD : card inserted.\n");
		return sprintf(buf, "Insert\n");
	} else {
		pr_debug("SD : card removed.\n");
		return sprintf(buf, "Remove\n");
	}

}

static DEVICE_ATTR(status, S_IRUGO|S_IRUSR|S_IRGRP,
			sd_detection_cmd_show, NULL);

static int gokey_hsmmc_late_init(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev =
	    container_of(dev, struct platform_device, dev);
	struct omap_mmc_platform_data *pdata = dev->platform_data;

	/* Setting MMC1 Card detect IRQ */
	if (pdev->id == 0) {
		ret = twl6030_mmc_card_detect_config();
		if (ret)
			pr_err("(%s): failed configuring MMC1 card detect\n",
			       __func__);
		pdata->slots[0].card_detect_irq =
		    TWL6030_IRQ_BASE + MMCDETECT_INTR_OFFSET;
		pdata->slots[0].card_detect = twl6030_mmc_card_detect;

		if (sd_detection_cmd_dev == NULL) {
			/*create sysfs file for detect pin */
			sd_detection_cmd_dev = device_create(sec_class,
							     NULL, 0, NULL,
							     "sdcard");
			if (IS_ERR(sd_detection_cmd_dev))
				pr_err("Failed to create sdcard sysfs dev\n");

			if (device_create_file(sd_detection_cmd_dev,
					       &dev_attr_status) < 0)
				pr_err
				    ("Fail to create sysfs sdcard/status\n");
		}

	}

	return ret;
}

static void __init gokey_hsmmc_set_late_init(struct device *dev)
{
	struct omap_mmc_platform_data *pdata;

	/* dev can be null if CONFIG_MMC_OMAP_HS is not set */
	if (!dev) {
		pr_err("(%s): failed!\n", __func__);
		return;
	}

	pdata = dev->platform_data;
	pdata->init = gokey_hsmmc_late_init;
}

static int __init gokey_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;

	/* Board specific GPIO pin for External LDO control */
	for (c = controllers; c->mmc; c++) {
		if ((c->mmc == 2) && (c->external_ldo == true))
			c->gpio_for_ldo =
			    omap_muxtbl_get_gpio_by_name("eMMC_LDO");
		else if ((c->mmc == 1) && (c->external_ldo == true))
			c->gpio_for_ldo =
			    omap_muxtbl_get_gpio_by_name("SD_LDO");
	}

	omap2_hsmmc_init(controllers);

	for (c = controllers; c->mmc; c++)
		gokey_hsmmc_set_late_init(c->dev);

	return 0;
}

void __init omap4_gokey_sdio_init(void)
{
	gokey_hsmmc_init(gokey_mmc_info);
}

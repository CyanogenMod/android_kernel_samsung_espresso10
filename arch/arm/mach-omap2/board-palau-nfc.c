/* arch/arm/mach-omap2/board-palau-nfc.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-tuna-nfc.c
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>

#include <plat/serial.h>

#include "board-palau.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"

#define NFC_PWR_OFF			0
#define NFC_PWR_ON			1
#define NFC_PWR_ON_FW			2

enum {
	GPIO_NFC_EN = 0,
	GPIO_NFC_FW,
	GPIO_NFC_IRQ,
};

static struct gpio palau_nfc_gpios[] = {
	[GPIO_NFC_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "NFC_EN",
	},
	[GPIO_NFC_FW] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "NFC_FIRMWARE",
	},
	[GPIO_NFC_IRQ] = {
		.flags	= GPIOF_IN,
		.label	= "NFC_IRQ",
	},
};

/* omap_uart_wake() counts from 1 */
static unsigned int nfc_uart_num = 4;

static unsigned int nfc_power;
static struct wake_lock nfc_wake_lock;

static void palau_nfc_power_apply(void)
{
	unsigned int gpio_nfc_en = palau_nfc_gpios[GPIO_NFC_EN].gpio;
	unsigned int gpio_nfc_fw = palau_nfc_gpios[GPIO_NFC_FW].gpio;

	switch (nfc_power) {
	case NFC_PWR_OFF:
		pr_info("%s OFF\n", __func__);
		gpio_set_value(gpio_nfc_fw, 0);
		gpio_set_value(gpio_nfc_en, 0);
		msleep(60);
		break;
	case NFC_PWR_ON:
		pr_info("%s ON\n", __func__);
		gpio_set_value(gpio_nfc_fw, 0);
		gpio_set_value(gpio_nfc_en, 1);
		msleep(20);
		break;
	case NFC_PWR_ON_FW:
		pr_info("%s ON (firmware download)\n", __func__);
		gpio_set_value(gpio_nfc_fw, 1);
		gpio_set_value(gpio_nfc_en, 1);
		msleep(20);
		gpio_set_value(gpio_nfc_en, 0);	/* fw mode requires reset */
		msleep(60);
		gpio_set_value(gpio_nfc_en, 1);
		msleep(20);
		break;
	}
}

static ssize_t palau_nfc_power_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", nfc_power);
}

static ssize_t palau_nfc_power_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int rc;
	unsigned int val;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val > NFC_PWR_ON_FW)
		return -EINVAL;

	nfc_power = val;
	palau_nfc_power_apply();

	return count;
}

static DEVICE_ATTR(nfc_power, S_IWUSR | S_IRUGO, palau_nfc_power_show,
		   palau_nfc_power_store);

static irqreturn_t palau_nfc_irq_isr(int irq, void *dev)
{
	omap_uart_wake(nfc_uart_num);

	/*
	 * take a 500ms wakelock, to give time for higher layers
	 * to either take their own wakelock or finish processing
	 */
	wake_lock_timeout(&nfc_wake_lock, msecs_to_jiffies(500));

	return IRQ_HANDLED;
}

static void __init palau_nfc_init_gpio(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(palau_nfc_gpios); i++)
		palau_nfc_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(palau_nfc_gpios[i].label);
	gpio_request_array(palau_nfc_gpios, ARRAY_SIZE(palau_nfc_gpios));
}

void __init omap4_palau_nfc_init(void)
{
	struct platform_device *pdev;
	int irq;

	wake_lock_init(&nfc_wake_lock, WAKE_LOCK_SUSPEND, "nfc");

	palau_nfc_init_gpio();

	irq = gpio_to_irq(palau_nfc_gpios[GPIO_NFC_IRQ].gpio);
	if (request_irq
	    (irq, palau_nfc_irq_isr, IRQF_TRIGGER_RISING, "nfc_irq", NULL)) {
		pr_err("%s: request_irq() failed\n", __func__);
		return;
	}

	if (enable_irq_wake(irq)) {
		pr_err("%s: irq_set_irq_wake() failed\n", __func__);
		return;
	}

	nfc_power = NFC_PWR_OFF;

	pdev = platform_device_register_simple("nfc-power", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		pr_err("%s: platform_device_register_simple() failed\n",
		       __func__);
		return;
	}
	if (device_create_file(&pdev->dev, &dev_attr_nfc_power))
		pr_err("%s: device_create_file() failed\n", __func__);
}

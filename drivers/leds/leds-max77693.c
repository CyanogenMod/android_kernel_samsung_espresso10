/*
 * LED driver for Maxim MAX77693 - leds-max77673.c
 *
 * Copyright (C) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/leds-max77693.h>
#include <linux/ctype.h>

/*
 * MAX77693 LED Register Setting
 */

/* MAX77693_IFLASH */
#define MAX77693_FLASH_IOUT		0x3F

/* MAX77693_ITORCH */
#define MAX77693_TORCH_IOUT1		0x0F
#define MAX77693_TORCH_IOUT2		0xF0

/* MAX77693_TORCH_TIMER */
#define MAX77693_TORCH_TMR_DUR		0x0F
#define MAX77693_DIS_TORCH_TMR		0x40
#define MAX77693_TORCH_TMR_MODE		0x80
#define MAX77693_TORCH_TMR_MODE_ONESHOT	0x00
#define MAX77693_TORCH_TMR_MDOE_MAXTIMER	0x01

/* MAX77693_FLASH_TIMER */
#define MAX77693_FLASH_TMR_DUR		0x0F
#define MAX77693_FLASH_TMR_MODE		0x80
/* MAX77693_FLASH_TMR_MODE value */
#define MAX77693_FLASH_TMR_MODE_ONESHOT	0x00
#define MAX77693_FLASH_TMR_MDOE_MAXTIMER	0x01

/* MAX77693_FLASH_EN */
#define MAX77693_FLASH_FLED1_EN_SHIFT	6
#define MAX77693_FLASH_FLED2_EN_SHIFT	4
#define MAX77693_TORCH_FLED1_EN_SHIFT	2
#define MAX77693_TORCH_FLED2_EN_SHIFT	0

#define MAX77693_FLASH_FLED1_EN_MASK	(0x03 << MAX77693_FLASH_FLED1_EN_SHIFT)
#define MAX77693_FLASH_FLED2_EN_MASK	(0x03 << MAX77693_FLASH_FLED2_EN_SHIFT)
#define MAX77693_TORCH_FLED1_EN_MASK	(0x03 << MAX77693_TORCH_FLED1_EN_SHIFT)
#define MAX77693_TORCH_FLED2_EN_MASK	(0x03 << MAX77693_TORCH_FLED2_EN_SHIFT)

/* MAX77693_TORCH_FLEDx_EN value */
#define MAX77693_FLED_EN_OFF		0x00
#define MAX77693_FLED_EN_BY_FLASH	0x01
#define MAX77693_FLED_EN_BY_TORCH	0x02
#define MAX77693_FLED_EN_I2C		0x03

/* MAX77693_VOUT_CNTL */
#define MAX77693_BOOST_FLASH_MODE	0x07
#define MAX77693_BOOST_FLASH_FLEDNUM	0x80

/* MAX77693_BOOST_FLASH_MODE vaule*/
#define MAX77693_BOOST_FLASH_MODE_OFF	0x00
#define MAX77693_BOOST_FLASH_MODE_FLED1	0x01
#define MAX77693_BOOST_FLASH_MODE_FLED2	0x02
#define MAX77693_BOOST_FLASH_MODE_BOTH	0x03
#define MAX77693_BOOST_FLASH_MODE_FIXED	0x04

/* MAX77693_BOOST_FLASH_FLEDNUM vaule*/
#define MAX77693_BOOST_FLASH_FLEDNUM_1	0x00
#define MAX77693_BOOST_FLASH_FLEDNUM_2	0x80

/* MAX77693_VOUT_FLASH1 */
#define MAX77693_BOOST_VOUT_FLASH	0x7F
#define MAX77693_BOOST_VOUT_FLASH_FROM_VOLT(mV)				\
		((mV) <= 3300 ? 0x00 :					\
		((mV) <= 5500 ? (((mV) - 3300) / 25 + 0x0C) : 0x7F))

#define MAX_FLASH_CURRENT	1000	/* 1000mA(0x1f) */
#define MAX_TORCH_CURRENT	250	/* 250mA(0x0f) */
#define MAX_FLASH_DRV_LEVEL	63	/* 15.625 + 15.625*63 mA */
#define MAX_TORCH_DRV_LEVEL	15	/* 15.625 + 15.625*15 mA */

struct max77693_led_data {
	struct led_classdev led;
	struct max77693_dev *max77693;
	struct max77693_led *data;
	struct i2c_client *i2c;
	struct work_struct work;
	struct mutex lock;
	spinlock_t value_lock;
	int brightness;
	int chip_brightness;
};

static struct max77693_led_data **g_max77693_led_datas;

static u8 fled_en_shift[MAX77693_LED_MAX] = {
	MAX77693_FLASH_FLED1_EN_SHIFT,
	MAX77693_FLASH_FLED2_EN_SHIFT,
	MAX77693_TORCH_FLED1_EN_SHIFT,
	MAX77693_TORCH_FLED2_EN_SHIFT,
};

static u8 fled_en_mask[MAX77693_LED_MAX] = {
	MAX77693_FLASH_FLED1_EN_MASK,
	MAX77693_FLASH_FLED2_EN_MASK,
	MAX77693_TORCH_FLED1_EN_MASK,
	MAX77693_TORCH_FLED2_EN_MASK,
};

static u8 fled_current_reg[MAX77693_LED_MAX] = {
	MAX77693_LED_REG_IFLASH1,
	MAX77693_LED_REG_IFLASH2,
	MAX77693_LED_REG_ITORCH,
	MAX77693_LED_REG_ITORCH,
};

static u8 fled_current_mask[MAX77693_LED_MAX] = {
	MAX77693_FLASH_IOUT,
	MAX77693_FLASH_IOUT,
	MAX77693_TORCH_IOUT1,
	MAX77693_TORCH_IOUT2
};

static u8 fled_current_shift[MAX77693_LED_MAX] = {
	0,
	0,
	0,
	4,
};

int max77693_get_flash_brightness(void)
{
	struct led_classdev *led_cdev = &g_max77693_led_datas[2]->led;

	return led_cdev->brightness;
}
EXPORT_SYMBOL(max77693_get_flash_brightness);

void max77693_set_flash_brightness(int brightness)
{
	struct led_classdev *led_cdev = &g_max77693_led_datas[2]->led;

	if (brightness > led_cdev->max_brightness)
		brightness = led_cdev->max_brightness;
	led_cdev->brightness = brightness;

	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->brightness_set(led_cdev, brightness);
}
EXPORT_SYMBOL(max77693_set_flash_brightness);

static int max77693_led_get_en_value(int id, int on)
{
	int val;

	if (on)
		val = MAX77693_FLED_EN_I2C;
	else {
		if (id < MAX77693_TORCH_LED_1)
			val = MAX77693_FLED_EN_BY_FLASH;
		else
			val = MAX77693_FLED_EN_BY_TORCH;
	}

	return val << fled_en_shift[id];
}

static void max77693_led_set(struct led_classdev *led_cdev,
						enum led_brightness val)
{
	unsigned long flags;
	struct max77693_led_data *led_data
		= container_of(led_cdev, struct max77693_led_data, led);

	pr_debug("[LED] %s\n", __func__);

	spin_lock_irqsave(&led_data->value_lock, flags);
	led_data->chip_brightness = min_t(int, val, MAX77693_FLASH_IOUT);
	spin_unlock_irqrestore(&led_data->value_lock, flags);

	schedule_work(&led_data->work);
}

static void led_set(struct max77693_led_data *led_data)
{
	int ret;
	struct max77693_led *data = led_data->data;
	int id = data->id;
	u8 shift = fled_current_shift[id];
	int val;
	struct i2c_client *i2c = led_data->i2c;

	/* Set current */
	if (led_data->chip_brightness == LED_OFF)
		val = led_data->brightness << shift;
	else
		val = led_data->chip_brightness << shift;

	ret = max77693_update_reg(i2c, fled_current_reg[id], val,
							fled_current_mask[id]);
	if (unlikely(ret))
		pr_err("%s: failed to set led level\n", __func__);

	/* Turn on/off LED */
	if (led_data->chip_brightness == LED_OFF)
		val = max77693_led_get_en_value(id, 0);
	else
		val = max77693_led_get_en_value(id, 1);

	ret = max77693_update_reg(i2c, MAX77693_LED_REG_FLASH_EN, val,
							fled_en_mask[id]);
	if (unlikely(ret))
		pr_err("%s: failed to set led contrl\n", __func__);
}

static void max77693_led_work(struct work_struct *work)
{
	struct max77693_led_data *led_data
		= container_of(work, struct max77693_led_data, work);

	pr_debug("[LED] %s\n", __func__);

	mutex_lock(&led_data->lock);
	led_set(led_data);
	mutex_unlock(&led_data->lock);
}

static int max77693_led_init_set(struct max77693_led_data *led_data)
{
	struct i2c_client *i2c = led_data->i2c;
	struct max77693_led *data = led_data->data;
	int id = data->id;
	int val;
	int ret = 0;

	max77693_write_reg(i2c, MAX77693_LED_REG_VOUT_CNTL,
				  MAX77693_BOOST_FLASH_FLEDNUM_2
				| MAX77693_BOOST_FLASH_MODE_BOTH);
	ret |= max77693_write_reg(i2c, MAX77693_LED_REG_VOUT_FLASH1,
				  MAX77693_BOOST_VOUT_FLASH_FROM_VOLT(5000));
	ret |= max77693_write_reg(i2c,
				MAX77693_LED_REG_MAX_FLASH1, 0x80);
	ret |= max77693_write_reg(i2c,
				MAX77693_LED_REG_MAX_FLASH2, 0x00);

	val = max77693_led_get_en_value(id, 0);
	ret |= max77693_update_reg(i2c, MAX77693_LED_REG_FLASH_EN, val,
							fled_en_mask[id]);

	/* Set TORCH_TMR_DUR or FLASH_TMR_DUR */
	if (id < MAX77693_TORCH_LED_1)
		ret |= max77693_write_reg(i2c, MAX77693_LED_REG_FLASH_TIMER,
					(data->timer | data->timer_mode << 7));
	else
		ret |= max77693_write_reg(i2c,
				MAX77693_LED_REG_ITORCHTORCHTIMER, 0x40);

	/* Set current */
	ret |= max77693_update_reg(i2c, fled_current_reg[id],
				led_data->brightness << fled_current_shift[id],
				fled_current_mask[id]);

	return ret;
}

static int max77693_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct max77693_dev *max77693 = dev_get_drvdata(pdev->dev.parent);
	struct max77693_platform_data *max77693_pdata
		= dev_get_platdata(max77693->dev);
	struct max77693_led_platform_data *pdata = max77693_pdata->led_data;
	struct max77693_led_data *led_data;
	struct max77693_led *data;
	struct max77693_led_data **led_datas;

	pr_info("%s is called\n", __func__);

	if (pdata == NULL) {
		pr_err("[LED] no platform data for this led is found\n");
		return -EFAULT;
	}

	led_datas = kzalloc(sizeof(struct max77693_led_data *)
			    * MAX77693_LED_MAX, GFP_KERNEL);
	if (unlikely(!led_datas)) {
		pr_err("[LED] memory allocation error %s", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, led_datas);

	g_max77693_led_datas = led_datas;

	pr_debug("[LED] %s\n", __func__);

	for (i = 0; i != pdata->num_leds; ++i) {
		data = &(pdata->leds[i]);

		led_data = kzalloc(sizeof(struct max77693_led_data),
				   GFP_KERNEL);
		led_datas[i] = led_data;
		if (unlikely(!led_data)) {
			pr_err("[LED] memory allocation error %s\n", __func__);
			ret = -ENOMEM;
			continue;
		}

		led_data->max77693 = max77693;
		led_data->i2c = max77693->i2c;
		led_data->data = data;
		led_data->led.name = data->name;
		led_data->led.brightness_set = max77693_led_set;
		led_data->led.brightness = LED_OFF;
		led_data->brightness = data->brightness;
		led_data->led.flags = 0;
		led_data->led.max_brightness = data->id < 2
			? MAX_FLASH_DRV_LEVEL : MAX_TORCH_DRV_LEVEL;

		mutex_init(&led_data->lock);
		spin_lock_init(&led_data->value_lock);
		INIT_WORK(&led_data->work, max77693_led_work);

		ret = led_classdev_register(&pdev->dev, &led_data->led);
		if (unlikely(ret)) {
			pr_err("unable to register LED\n");
			kfree(led_data);
			ret = -EFAULT;
			continue;
		}

		ret = max77693_led_init_set(led_data);
		if (unlikely(ret)) {
			pr_err("unable to register LED\n");
			mutex_destroy(&led_data->lock);
			led_classdev_unregister(&led_data->led);
			kfree(led_data);
			ret = -EFAULT;
		}
	}

	return ret;
}

static int __devexit max77693_led_remove(struct platform_device *pdev)
{
	struct max77693_led_data **led_datas = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i != MAX77693_LED_MAX; ++i) {
		if (led_datas[i] == NULL)
			continue;

		cancel_work_sync(&led_datas[i]->work);
		mutex_destroy(&led_datas[i]->lock);
		led_classdev_unregister(&led_datas[i]->led);
		kfree(led_datas[i]);
	}
	kfree(led_datas);

	return 0;
}

void max77693_led_shutdown(struct device *dev)
{
	struct max77693_led_data **led_datas = dev_get_drvdata(dev);

	/* Turn off LED */
	max77693_update_reg(led_datas[2]->i2c, MAX77693_LED_REG_FLASH_EN,
				0x02 << fled_en_shift[2], fled_en_mask[2]);
}

static struct platform_driver max77693_led_driver = {
	.probe		= max77693_led_probe,
	.remove		= __devexit_p(max77693_led_remove),
	.driver		= {
		.name	= "max77693-led",
		.owner	= THIS_MODULE,
		.shutdown = max77693_led_shutdown,
	},
};

static int __init max77693_led_init(void)
{
	return platform_driver_register(&max77693_led_driver);
}
module_init(max77693_led_init);

static void __exit max77693_led_exit(void)
{
	platform_driver_unregister(&max77693_led_driver);
}
module_exit(max77693_led_exit);

MODULE_DESCRIPTION("MAX77693 LED driver");
MODULE_LICENSE("GPL");

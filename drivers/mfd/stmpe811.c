/* drivers/mfd/stmpe811.c
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

#include <linux/types.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/wakelock.h>
#include <linux/blkdev.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/mfd/stmpe811.h>
#include <mach/gpio.h>

#define STMPE811_CHIP_ID	0x00
#define STMPE811_ID_VER		0x02
#define STMPE811_SYS_CTRL1	0x03
#define STMPE811_SYS_CTRL2	0x04
#define STMPE811_INT_CTRL	0x09
#define STMPE811_INT_EN		0x0A
#define STMPE811_INT_STA	0x0B
#define STMPE811_ADC_INT_EN	0x0E
#define STMPE811_ADC_INT_STA	0x0F
#define STMPE811_ADC_CTRL1	0x20
#define STMPE811_ADC_CTRL2	0x21
#define STMPE811_ADC_CAPT	0x22
#define STMPE811_ADC_DATA_CH0	0x30
#define STMPE811_ADC_DATA_CH1	0x32
#define STMPE811_ADC_DATA_CH2	0x34
#define STMPE811_ADC_DATA_CH3	0x36
#define STMPE811_ADC_DATA_CH4	0x38
#define STMPE811_ADC_DATA_CH5	0x3A
#define STMPE811_ADC_DATA_CH6	0x3C
#define STMPE811_ADC_DATA_CH7	0x3E
#define STMPE811_GPIO_AF	0x17
#define STMPE811_TSC_CTRL	0x40

static struct device *sec_adc_dev;

static const struct file_operations stmpe811_fops = {
	.owner = THIS_MODULE,
};

static struct miscdevice stmpe811_adc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sec_adc",
	.fops = &stmpe811_fops,
};

static struct i2c_driver stmpe811_adc_i2c_driver;
static struct i2c_client *stmpe811_adc_i2c_client;

struct stmpe811_adc_state {
	struct i2c_client	*client;
	struct mutex	adc_lock;
};

struct stmpe811_adc_state *stmpe811_adc_state;

static int stmpe811_i2c_read(struct i2c_client *client, u8 reg, u8 *data,
								u8 length)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, (u8)reg, length, data);
	if (ret < 0)
		pr_err("Failed to stmpe811 read, reg: %d\n", reg);

	return ret;
}

static int stmpe811_i2c_write(struct i2c_client *client, u8 reg, u8 *data,
								u8 length)
{
	u16 value;
	int ret;

	value = (*(data + 1)) | (*(data) << 8) ;

	ret = i2c_smbus_write_word_data(client, (u8)reg, swab16(value));
	if (ret < 0)
		pr_err("Failed to stmpe811 write, reg: %d\n", reg);

	return ret;
}

int stmpe811_write_register(u8 addr, u16 w_data)
{
	struct i2c_client *client = stmpe811_adc_i2c_client;
	u8 data[2];
	int ret;

	data[0] = w_data & 0xFF;
	data[1] = (w_data >> 8);

	ret = stmpe811_i2c_write(client, addr, data, (u8)2) ;

	return ret;
}

int stmpe811_adc_get_value(u8 channel)
{
	struct i2c_client *client = stmpe811_adc_i2c_client;
	struct stmpe811_adc_state *adc = i2c_get_clientdata(client);
	int ret;
	u8 data[2];
	u16 w_data;
	int data_channel_addr = 0;
	int count = 0;
	u8 completed;

	if (!adc) {
		pr_err("stmpe811: no adc device. probe failed.\n");
		return -1;
	}

	mutex_lock(&adc->adc_lock);

	ret = stmpe811_write_register(STMPE811_ADC_CAPT, (1 << channel)) ;
	if (ret < 0) {
		pr_err("Failed to get stmpe811 adc value,");
		goto adc_get_fail;
	}

	while (1) {
		usleep_range(1000, 1100);

		stmpe811_i2c_read(client, STMPE811_ADC_CAPT, &completed, (u8)1);
		if (completed & (1 << channel))
			break;

		count++;
		if (count > 10) {
			pr_err("timeout to get stmpe811 adc value,");
			goto adc_get_fail;
		}
	}

	/* read value from ADC */
	data_channel_addr = STMPE811_ADC_DATA_CH0 + (channel * 2);
	ret = stmpe811_i2c_read(client, data_channel_addr, data, (u8)2);
	if (ret < 0) {
		pr_err("Failed to get stmpe811 adc value,");
		goto adc_get_fail;
	}

	w_data = ((data[0] << 8) | data[1]) & 0x0FFF;
	ret = w_data;

adc_get_fail:
	mutex_unlock(&adc->adc_lock);

	return ret;
}
EXPORT_SYMBOL(stmpe811_adc_get_value);

static int __init stmpe811_adc_init(void)
{
	int ret = 0;

	pr_info("init!");

	/*misc device registration*/
	ret = misc_register(&stmpe811_adc_device);
	if (ret < 0) {
		pr_err("misc_register failed");
		return ret;
	}

	if (i2c_add_driver(&stmpe811_adc_i2c_driver)) {
		pr_err("%s: Can't add fg i2c drv\n", __func__);
		goto err_i2c;
	}

	return 0;

err_i2c:
	misc_deregister(&stmpe811_adc_device);

	return ret;
}

static void __init stmpe811_adc_exit(void)
{
	pr_info("exit!");

	i2c_del_driver(&stmpe811_adc_i2c_driver);

	misc_deregister(&stmpe811_adc_device);
}


static int stmpe811_adc_i2c_remove(struct i2c_client *client)
{
	struct stmpe811_adc_state *adc = i2c_get_clientdata(client);

	mutex_destroy(&adc->adc_lock);
	kfree(adc);

	return 0;
}

static int stmpe811_adc_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct stmpe811_adc_state *adc;
	u8 data[2];
	u16 w_data;

	pr_info("stmpe811 driver probe\n");

	adc = kzalloc(sizeof(struct stmpe811_adc_state), GFP_KERNEL);
	if (adc == NULL) {
		pr_err("failed to allocate memory\n");
		return -ENOMEM;
	}

	adc->client = client;
	i2c_set_clientdata(client, adc);

	stmpe811_adc_i2c_client = client;

	mutex_init(&adc->adc_lock);

	if (stmpe811_i2c_read(client, STMPE811_CHIP_ID, data, (u8)2) < 0) {
		pr_err("Failed to read STMPE811_CHIP_ID.\n");
		kfree(adc);
		return -1;
	}

	w_data = (data[0] << 8) | data[1];
	pr_info("CHIP_ID = 0x%x\n", w_data);

	/* init stmpe811 adc driver */
	stmpe811_write_register(STMPE811_SYS_CTRL1, 0x02);

	msleep(20);

	/* enable adc & ts clock */
	stmpe811_write_register(STMPE811_SYS_CTRL2, 0x00);
	stmpe811_i2c_read(client, STMPE811_SYS_CTRL2, data, (u8)1);
	pr_info("STMPE811_SYS_CTRL2 = 0x%x..\n", data[0]);

	/* disable interrupt */
	stmpe811_write_register(STMPE811_INT_EN, 0x00);
	stmpe811_i2c_read(client, STMPE811_INT_EN, data, (u8)1);
	pr_info("STMPE811_INT_EN = 0x%x..\n", data[0]);

	/* 64, 12bit, internal */
	stmpe811_write_register(STMPE811_ADC_CTRL1, 0x3C);
	stmpe811_i2c_read(client, STMPE811_ADC_CTRL1, data, (u8)1);
	pr_info("STMPE811_ADC_CTRL1 = 0x%x..\n", data[0]);

	/* clock speed 6.5MHz */
	stmpe811_write_register(STMPE811_ADC_CTRL2, 0x03);
	stmpe811_i2c_read(client, STMPE811_ADC_CTRL2, data, (u8)1);
	pr_info("STMPE811_ADC_CTRL2 = 0x%x..\n", data[0]);

	/* ADC settings. The value should be 0x00 instead of 0xFF
	 * gpio 0-3 -> ADC
	 */
	stmpe811_write_register(STMPE811_GPIO_AF, 0x00);

	stmpe811_i2c_read(client, STMPE811_GPIO_AF, data, (u8)1);
	pr_info("STMPE811_GPIO_AF = 0x%x..\n", data[0]);

	stmpe811_write_register(STMPE811_TSC_CTRL, 0x00);
	stmpe811_i2c_read(client, STMPE811_TSC_CTRL, data, (u8)1);
	pr_info("STMPE811_TSC_CTRL = 0x%x..\n", data[0]);

	pr_info("adc_i2c_probe success!!!\n");

	return 0;
}

static const struct i2c_device_id stmpe811_adc_device_id[] = {
	{"stmpe811", 0},
};
MODULE_DEVICE_TABLE(i2c, stmpe811_adc_device_id);

static struct i2c_driver stmpe811_adc_i2c_driver = {
	.driver = {
		.name = "stmpe811",
		.owner = THIS_MODULE,
	},
	.probe	= stmpe811_adc_i2c_probe,
	.remove	= stmpe811_adc_i2c_remove,
	.id_table	= stmpe811_adc_device_id,
};

module_init(stmpe811_adc_init);
module_exit(stmpe811_adc_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("Samsung STMPE811 ADC driver");
MODULE_LICENSE("GPL");

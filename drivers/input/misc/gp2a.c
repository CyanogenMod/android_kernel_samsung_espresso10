/* linux/driver/input/misc/gp2a.c
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/gp2a.h>
#include <linux/sensors_core.h>
#include <linux/regulator/consumer.h>

/* Note about power vs enable/disable:
 *  The chip has two functions, proximity and ambient light sensing.
 *  There is no separate power enablement to the two functions (unlike
 *  the Capella CM3602/3623).
 *  This module implements two drivers: /dev/proximity and /dev/light.
 *  When either driver is enabled (via sysfs attributes), we give power
 *  to the chip.  When both are disabled, we remove power from the chip.
 *  In suspend, we remove power if light is disabled but not if proximity is
 *  enabled (proximity is allowed to wakeup from suspend).
 *
 *  There are no ioctls for either driver interfaces.  Output is via
 *  input device framework and control via sysfs attributes.
 */


#define gp2a_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)

/* ADDSEL is LOW */
#define REGS_PROX		0x0 /* Read  Only */
#define REGS_GAIN		0x1 /* Write Only */
#define REGS_HYS		0x2 /* Write Only */
#define REGS_CYCLE		0x3 /* Write Only */
#define REGS_OPMOD		0x4 /* Write Only */
#if defined(CONFIG_GP2A_MODE_B)
#define REGS_CON	0x6 /* Write Only */
#endif

/* sensor type */
#define LIGHT           0
#define PROXIMITY	1
#define ALL		2

#define DELAY_LOWBOUND	(100 * NSEC_PER_MSEC)

/* start time delay for light sensor in nano seconds */
#define LIGHT_SENSOR_START_TIME_DELAY 50000000

static u8 reg_defaults[5] = {
	0x00, /* PROX: read only register */
	0x08, /* GAIN: large LED drive level */
	0xC2, /* HYS: receiver sensitivity */
	0x04, /* CYCLE: */
	0x01, /* OPMOD: normal operating mode */
};
static u8 reg_b_mode[5] = {
	0x00, /* PROX: read only register */
	0x08, /* GAIN: large LED drive level */
	0x40, /* HYS: receiver sensitivity */
	0x04, /* CYCLE: */
	0x03, /* OPMOD: normal operating mode */
};

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

/* driver data */
struct gp2a_data {
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct gp2a_platform_data *pdata;
	struct i2c_client *i2c_client;
	int irq;
	struct work_struct work_light;
	struct work_struct work_prox;
	struct hrtimer timer;
	ktime_t light_poll_delay;
	bool on;
	u8 power_state;
	struct mutex power_lock;
	struct wake_lock prx_wake_lock;
	struct workqueue_struct *wq;
	struct workqueue_struct *prox_wq;
	int prox_state;
	struct device *light_sensor_device;
	struct device *proximity_sensor_device;
};

int gp2a_i2c_read(struct gp2a_data *gp2a, u8 reg, u8 *val)
{

	int err;
	u8 buf[1];
	struct i2c_msg msg[2];
	struct i2c_client *client = gp2a->i2c_client;
	int retry = 2;

	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = 1;
	msg[0].len = 2;
	msg[0].buf = buf;

	do {
		err = i2c_transfer(client->adapter, msg, 1);
		if (unlikely(err < 0))
			pr_err("%s, slave addr=%02x, reg addr=%02x, err = %d\n",
				__func__, client->addr, reg, err);
	} while (unlikely(err < 0) && retry--);
	if (likely(err >= 0))
		*val = buf[1];
	return err;
}

int gp2a_i2c_write(struct gp2a_data *gp2a, u8 reg, u8 *val)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 2;
	struct i2c_client *client = gp2a->i2c_client;

	if (unlikely((client == NULL) || (!client->adapter))) {
		err = -ENODEV;
		goto done;
	}

	data[0] = reg;
	data[1] = *val;
	msg->addr = client->addr;
	msg->flags = 0; /* write */
	msg->len = 2;
	msg->buf = data;

	do {
		err = i2c_transfer(client->adapter, msg, 1);
		if (err >= 0)
			err = 0;
	} while (unlikely(err < 0) && retry--);
done:
	return err;
}

static void gp2a_light_enable(struct gp2a_data *gp2a)
{
	gp2a_dbgmsg("starting poll timer, delay %lldns\n",
		    ktime_to_ns(gp2a->light_poll_delay));
	/*
	 * Set far out of range ABS_MISC value, -1024, to enable real value to
	 * go through next.
	 */
	input_abs_set_val(gp2a->light_input_dev, ABS_MISC, -1024);
	hrtimer_start(&gp2a->timer, ktime_set(0, LIGHT_SENSOR_START_TIME_DELAY),
					HRTIMER_MODE_REL);
}

static void gp2a_light_disable(struct gp2a_data *gp2a)
{
	gp2a_dbgmsg("cancelling poll timer\n");
	hrtimer_cancel(&gp2a->timer);
	cancel_work_sync(&gp2a->work_light);
}

static ssize_t poll_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	return sprintf(buf, "%lld\n", ktime_to_ns(gp2a->light_poll_delay));
}


static ssize_t poll_delay_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;

	err = strict_strtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	gp2a_dbgmsg("new delay = %lldns, old delay = %lldns\n",
		    new_delay, ktime_to_ns(gp2a->light_poll_delay));

	if (new_delay < DELAY_LOWBOUND) {
		gp2a_dbgmsg("new delay less than low bound, so set delay "
			"to %lld\n", (int64_t)DELAY_LOWBOUND);
		new_delay = DELAY_LOWBOUND;
	}

	mutex_lock(&gp2a->power_lock);
	if (new_delay != ktime_to_ns(gp2a->light_poll_delay)) {
		gp2a->light_poll_delay = ns_to_ktime(new_delay);
		if (gp2a->power_state & LIGHT_ENABLED) {
			gp2a_light_disable(gp2a);
			gp2a_light_enable(gp2a);
		}
	}
	mutex_unlock(&gp2a->power_lock);

	return size;
}

static ssize_t light_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		       (gp2a->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static ssize_t proximity_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		       (gp2a->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static ssize_t light_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&gp2a->power_lock);
	gp2a_dbgmsg("new_value = %d, old state = %d\n",
		    new_value, (gp2a->power_state & LIGHT_ENABLED) ? 1 : 0);
	if (new_value && !(gp2a->power_state & LIGHT_ENABLED)) {
		if (!gp2a->power_state)
			gp2a->pdata->power(true);
		gp2a->power_state |= LIGHT_ENABLED;
		gp2a_light_enable(gp2a);
	} else if (!new_value && (gp2a->power_state & LIGHT_ENABLED)) {
		gp2a_light_disable(gp2a);
		gp2a->power_state &= ~LIGHT_ENABLED;
		if (!gp2a->power_state)
			gp2a->pdata->power(false);
	}
	mutex_unlock(&gp2a->power_lock);
	return size;
}

static ssize_t proximity_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	bool new_value;
	u8 value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&gp2a->power_lock);
	gp2a_dbgmsg("new_value = %d, old state = %d\n",
		    new_value, (gp2a->power_state & PROXIMITY_ENABLED) ? 1 : 0);
	if (new_value && !(gp2a->power_state & PROXIMITY_ENABLED)) {
		if (!gp2a->power_state)
			gp2a->pdata->power(true);
		gp2a->power_state |= PROXIMITY_ENABLED;

		input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, 2);
		input_sync(gp2a->proximity_input_dev);

		input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, 1);
		input_sync(gp2a->proximity_input_dev);

		enable_irq(gp2a->irq);
		enable_irq_wake(gp2a->irq);
		gp2a_i2c_write(gp2a, REGS_GAIN, &reg_defaults[1]);
		value = 0x40;
		gp2a_i2c_write(gp2a, REGS_HYS, &value);
		gp2a_i2c_write(gp2a, REGS_CYCLE, &reg_defaults[3]);
		gp2a_i2c_write(gp2a, REGS_OPMOD, &reg_defaults[4]);
	} else if (!new_value && (gp2a->power_state & PROXIMITY_ENABLED)) {
		disable_irq_wake(gp2a->irq);
		disable_irq(gp2a->irq);
#if defined(CONFIG_GP2A_MODE_B)
		value = 0x02;	/* VCON enable, SSD disable */
		gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
#else
		gp2a_i2c_write(gp2a, REGS_OPMOD, &reg_defaults[0]);
#endif
		gp2a->power_state &= ~PROXIMITY_ENABLED;
		if (!gp2a->power_state)
			gp2a->pdata->power(false);
	}
	mutex_unlock(&gp2a->power_lock);
	return size;
}

static ssize_t gp2a_proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", gp2a->prox_state);
}

static ssize_t gp2a_light_sensor_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	int adc = gp2a->pdata->light_adc_value();
	if (adc < 8)
		adc = 0;
	msleep(50);
	return sprintf(buf, "%d\n", adc);
}
static ssize_t gp2a_light_vendor_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "%s\n", "SHARP");
}
static struct device_attribute dev_attr_light_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, gp2a_light_vendor_show, NULL);
static struct device_attribute dev_attr_proximity_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, gp2a_light_vendor_show, NULL);


static ssize_t gp2a_light_name_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "%s\n", "GP2AP002");
}
static struct device_attribute dev_attr_light_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, gp2a_light_name_show, NULL);
static struct device_attribute dev_attr_proximity_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, gp2a_light_name_show, NULL);

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   poll_delay_show, poll_delay_store);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       light_enable_show, light_enable_store);

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       proximity_enable_show, proximity_enable_store);

static struct device_attribute dev_attr_light_sensor_lux =
	__ATTR(lux, S_IRUSR | S_IRGRP, gp2a_light_sensor_lux_show, NULL);
static struct device_attribute dev_attr_light_sensor_raw_data =
	__ATTR(raw_data, S_IRUSR | S_IRGRP, gp2a_light_sensor_lux_show, NULL);

static struct device_attribute dev_attr_proximity_sensor_state =
	__ATTR(state, S_IRUSR | S_IRGRP, gp2a_proximity_state_show, NULL);
static struct device_attribute dev_attr_proximity_sensor_raw_data =
	__ATTR(raw_data, S_IRUSR | S_IRGRP, gp2a_proximity_state_show, NULL);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static struct device_attribute *additional_light_attrs[] = {
	&dev_attr_light_sensor_lux,
	&dev_attr_light_sensor_raw_data,
	&dev_attr_light_sensor_vendor,
	&dev_attr_light_sensor_name,
	NULL,
};

static struct device_attribute *additional_proximity_attrs[] = {
	&dev_attr_proximity_sensor_state,
	&dev_attr_proximity_sensor_raw_data,
	&dev_attr_proximity_sensor_vendor,
	&dev_attr_proximity_sensor_name,
	NULL,
};

static void gp2a_work_func_light(struct work_struct *work)
{
	struct gp2a_data *gp2a = container_of(work, struct gp2a_data,
					      work_light);
	int adc = gp2a->pdata->light_adc_value();
	if (adc < 0) {
		pr_err("adc returned error %d\n", adc);
		return;
	}
	if (!adc)
		adc = 1;
	gp2a_dbgmsg("adc returned light value %d\n", adc);
	input_report_rel(gp2a->light_input_dev, REL_MISC, adc);
	input_sync(gp2a->light_input_dev);
}

/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */
static enum hrtimer_restart gp2a_timer_func(struct hrtimer *timer)
{
	struct gp2a_data *gp2a = container_of(timer, struct gp2a_data, timer);
	queue_work(gp2a->wq, &gp2a->work_light);
	hrtimer_forward_now(&gp2a->timer, gp2a->light_poll_delay);
	return HRTIMER_RESTART;
}

#if defined(CONFIG_GP2A_MODE_B)
static void gp2a_work_func_prox(struct work_struct *work)
{
	struct gp2a_data *gp2a = container_of(work, struct gp2a_data,
						work_prox);
	unsigned char value;
	unsigned char int_val;
	unsigned char vout;

	int_val = REGS_PROX | (1 << 7);

	gp2a_i2c_read(gp2a, int_val, &value);
	vout = value & 0x01;
	pr_info("[PROXIMITY] value = %d\n", vout);

	wake_lock_timeout(&gp2a->prx_wake_lock, 3*HZ);

	/* Report proximity information */
	gp2a->prox_state = !vout;
	/* 0 is close, 1 is far */
	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, !vout);
	input_sync(gp2a->proximity_input_dev);

	mdelay(1);

	/* Write HYS Register */
	if (!vout)
		value = 0x40;
	else
		value = 0x20;

	gp2a_i2c_write(gp2a, REGS_HYS, &value);

	enable_irq(gp2a->irq);

	/* enabling VOUT terminal in nomal operation */
	value = 0x00;

	gp2a_i2c_write(gp2a, REGS_CON, &value);

}
#endif

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t gp2a_irq_handler(int irq, void *data)
{
	struct gp2a_data *gp2a = data;

#if defined(CONFIG_GP2A_MODE_B)
	pr_info("[PROXIMITY] gp2a->irq = %d\n", gp2a->irq);
	if (gp2a->irq != -1) {
		disable_irq_nosync(gp2a->irq);
		pr_info("[PROXIMITY] disable_irq\n");
		queue_work(gp2a->prox_wq, &gp2a->work_prox);
	}
#else
	int val = gpio_get_value(gp2a->pdata->p_out);
	if (val < 0) {
		pr_err("%s: gpio_get_value error %d\n", __func__, val);
		return IRQ_HANDLED;
	}
	gp2a->prox_state = !val;
	gp2a_dbgmsg("gp2a: proximity val=%d\n", val);

	/* 0 is close, 1 is far */
	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, val);
	input_sync(gp2a->proximity_input_dev);
	wake_lock_timeout(&gp2a->prx_wake_lock, 3*HZ);
#endif
	return IRQ_HANDLED;
}

static int gp2a_setup_irq(struct gp2a_data *gp2a)
{
	int rc = -EIO;
	struct gp2a_platform_data *pdata = gp2a->pdata;
	int irq;

	gp2a_dbgmsg("start\n");

	gpio_free(pdata->p_out);

	rc = gpio_request(pdata->p_out, "gpio_proximity_out");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->p_out, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->p_out);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, pdata->p_out, rc);
		goto err_gpio_direction_input;
	}

	irq = gpio_to_irq(pdata->p_out);
	rc = request_irq(irq,
			 gp2a_irq_handler,
			 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			 "proximity_int",
			 gp2a);
	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, irq,
			pdata->p_out, rc);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(irq);
	gp2a->irq = irq;

	/* sync input device with proximity gpio pin default value */
	gp2a_irq_handler(gp2a->irq, gp2a);

	gp2a_dbgmsg("success\n");

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(pdata->p_out);
done:
	return rc;
}

static int gp2a_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct input_dev *input_dev;
	struct gp2a_data *gp2a;
	struct gp2a_platform_data *pdata = client->dev.platform_data;
	u8 value;

	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		return ret;
	}
	if (!pdata->power || !pdata->light_adc_value) {
		pr_err("%s: incomplete pdata!\n", __func__);
		return ret;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		return ret;
	}

	gp2a = kzalloc(sizeof(struct gp2a_data), GFP_KERNEL);
	if (!gp2a) {
		pr_err("%s: failed to alloc memory for module data\n",
		       __func__);
		return -ENOMEM;
	}

	gp2a->pdata = pdata;
	gp2a->i2c_client = client;
	i2c_set_clientdata(client, gp2a);

	if (pdata->ldo_on != NULL)
		pdata->ldo_on(true);

	if (pdata->led_on != NULL)
		pdata->led_on(true);

	ret = gp2a_i2c_read(gp2a, REGS_PROX, &value);
	if (ret < 0) {
		pr_err("%s: can't access i2c\n", __func__);
		goto err_i2c_access_fail;
	}

#if defined(CONFIG_GP2A_MODE_B)
	gp2a->prox_wq = create_singlethread_workqueue("gp2a_wq_prox");
	if (!gp2a->prox_wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create porx workqueue\n", __func__);
		goto err_create_prox_workqueue;
	}
	INIT_WORK(&gp2a->work_prox, gp2a_work_func_prox);
#endif
	wake_lock_init(&gp2a->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");
	mutex_init(&gp2a->power_lock);

	/* allocate proximity input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		goto err_input_allocate_device_proximity;
	}

#if defined(CONFIG_GP2A_MODE_B)
	gp2a_i2c_write(gp2a, REGS_GAIN, &reg_b_mode[1]);
	gp2a_i2c_write(gp2a, REGS_HYS, &reg_b_mode[2]);
	gp2a_i2c_write(gp2a, REGS_CYCLE, &reg_b_mode[3]);
	gp2a_i2c_write(gp2a, REGS_OPMOD, &reg_b_mode[4]);
#endif
	gp2a->proximity_input_dev = input_dev;
	input_set_drvdata(input_dev, gp2a);
	input_dev->name = "proximity_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 2, 0, 0);

	gp2a_dbgmsg("registering proximity input device\n");
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		goto err_input_register_device_proximity;
	}

	ret = gp2a_setup_irq(gp2a);
	if (ret) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &proximity_attribute_group);
	if (ret) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_proximity;
	}

	/* hrtimer settings.  we poll for light values using a timer. */
	hrtimer_init(&gp2a->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gp2a->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	gp2a->timer.function = gp2a_timer_func;
	gp2a->prox_state = 0;

	/* the timer just fires off a work queue request.  we need a thread
	 * to read the i2c (can be slow and blocking)
	 */
	gp2a->wq = create_singlethread_workqueue("gp2a_wq");
	if (!gp2a->wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&gp2a->work_light, gp2a_work_func_light);

	/* allocate lightsensor-level input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_light;
	}
	input_set_drvdata(input_dev, gp2a);
	input_dev->name = "light_sensor";
	input_set_capability(input_dev, EV_REL, REL_MISC);

	gp2a_dbgmsg("registering lightsensor-level input device\n");
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_light;
	}
	gp2a->light_input_dev = input_dev;
	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &light_attribute_group);
	if (ret) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_light;
	}
	ret = sensors_register(gp2a->light_sensor_device,
		gp2a, additional_light_attrs, "light_sensor");
	if (ret) {
		pr_err("%s: cound not register sensor device\n", __func__);
		goto err_sysfs_create_factory_light;
	}
	ret = sensors_register(gp2a->proximity_sensor_device,
		gp2a, additional_proximity_attrs, "proximity_sensor");
	if (ret) {
		pr_err("%s: cound not register sensor device\n", __func__);
		goto err_sysfs_create_factory_proximity;
	}

#if defined(CONFIG_GP2A_MODE_B)
	value = 0x02;	/* VCON enable, SSD disable */
	gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
#endif

	goto done;

	/* error, unwind it all */
err_sysfs_create_factory_proximity:
	sensors_unregister(gp2a->light_sensor_device);
err_sysfs_create_factory_light:
	sysfs_remove_group(&input_dev->dev.kobj,
			   &light_attribute_group);
err_sysfs_create_group_light:
	input_unregister_device(gp2a->light_input_dev);
	input_free_device(gp2a->light_input_dev);
err_input_register_device_light:
err_input_allocate_device_light:
	destroy_workqueue(gp2a->wq);
err_create_workqueue:
	sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
err_sysfs_create_group_proximity:
	free_irq(gp2a->irq, gp2a);
	gpio_free(gp2a->pdata->p_out);
err_setup_irq:
	input_unregister_device(gp2a->proximity_input_dev);
err_input_register_device_proximity:
	input_free_device(gp2a->proximity_input_dev);
err_input_allocate_device_proximity:
	mutex_destroy(&gp2a->power_lock);
	wake_lock_destroy(&gp2a->prx_wake_lock);
	destroy_workqueue(gp2a->prox_wq);
err_create_prox_workqueue:
err_i2c_access_fail:
	if (pdata->led_on != NULL)
		pdata->led_on(false);
	if (pdata->ldo_on != NULL)
		pdata->ldo_on(false);
	kfree(gp2a);
done:
	return ret;
}

static int gp2a_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	 * is enabled, we leave power on because proximity is allowed
	 * to wake up device.  We remove power without changing
	 * gp2a->power_state because we use that state in resume
	 */
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2a_data *gp2a = i2c_get_clientdata(client);
	if (gp2a->power_state & LIGHT_ENABLED)
		gp2a_light_disable(gp2a);
	if (gp2a->power_state == LIGHT_ENABLED)
		gp2a->pdata->power(false);
	return 0;
}

static int gp2a_resume(struct device *dev)
{
	/* Turn power back on if we were before suspend. */
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2a_data *gp2a = i2c_get_clientdata(client);
	if (gp2a->power_state == LIGHT_ENABLED)
		gp2a->pdata->power(true);
	if (gp2a->power_state & LIGHT_ENABLED)
		gp2a_light_enable(gp2a);
	return 0;
}

static int gp2a_i2c_remove(struct i2c_client *client)
{
	struct gp2a_data *gp2a = i2c_get_clientdata(client);
	sensors_unregister(gp2a->proximity_sensor_device);
	sensors_unregister(gp2a->light_sensor_device);
	sysfs_remove_group(&gp2a->light_input_dev->dev.kobj,
			   &light_attribute_group);
	sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
	free_irq(gp2a->irq, gp2a);
	destroy_workqueue(gp2a->wq);
	input_unregister_device(gp2a->light_input_dev);
	input_unregister_device(gp2a->proximity_input_dev);
	gpio_free(gp2a->pdata->p_out);
	if (gp2a->power_state) {
		gp2a->power_state = 0;
		if (gp2a->power_state & LIGHT_ENABLED)
			gp2a_light_disable(gp2a);
		gp2a->pdata->power(false);
	}
	mutex_destroy(&gp2a->power_lock);
	wake_lock_destroy(&gp2a->prx_wake_lock);
	kfree(gp2a);
	return 0;
}

static void gp2a_i2c_shutdown(struct i2c_client *client)
{
	struct gp2a_data *gp2a = i2c_get_clientdata(client);

	if (gp2a->power_state) {
		gp2a->power_state = 0;
		if (gp2a->power_state & LIGHT_ENABLED)
			gp2a_light_disable(gp2a);
		gp2a->pdata->power(false);
	}

	if (gp2a->pdata->led_on != NULL)
		gp2a->pdata->led_on(false);
	if (gp2a->pdata->ldo_on != NULL)
		gp2a->pdata->ldo_on(false);
}

static const struct i2c_device_id gp2a_device_id[] = {
	{"gp2a", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, gp2a_device_id);

static const struct dev_pm_ops gp2a_pm_ops = {
	.suspend = gp2a_suspend,
	.resume = gp2a_resume
};

static struct i2c_driver gp2a_i2c_driver = {
	.driver = {
		.name = "gp2a",
		.owner = THIS_MODULE,
		.pm = &gp2a_pm_ops
	},
	.probe = gp2a_i2c_probe,
	.remove = gp2a_i2c_remove,
	.shutdown = gp2a_i2c_shutdown,
	.id_table = gp2a_device_id,
};


static int __init gp2a_init(void)
{
	return i2c_add_driver(&gp2a_i2c_driver);
}

static void __exit gp2a_exit(void)
{
	i2c_del_driver(&gp2a_i2c_driver);
}

module_init(gp2a_init);
module_exit(gp2a_exit);

MODULE_AUTHOR("mjchen@sta.samsung.com");
MODULE_DESCRIPTION("Optical Sensor driver for gp2ap002a00f");
MODULE_LICENSE("GPL");

/*
 * Copyright (c) 2010-2011 Yamaha Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>

#define __LINUX_KERNEL_DRIVER__
#include <linux/yas.h>
#include <linux/sensors_core.h>
#include "yas_acc_driver.c"
#include <linux/yas_accel.h>

#define YAS_ACC_KERNEL_VERSION	"4.4.702a"
#define YAS_ACC_KERNEL_NAME	"accelerometer"
#define CAL_SUCCESS	1
#define CAL_FAIL	0

#ifdef CONFIG_YAS_ACC_MULTI_SUPPORT
char vendor_name[MAX_CHIP_NUM][MAX_LEN_OF_NAME] = {
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA250)
	{"BOSCH"},
#elif (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
	{"STM"},
#endif
};

char chip_name[MAX_CHIP_NUM][MAX_LEN_OF_NAME] = {
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA250)
	{"BMA254"},
#elif (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
	{"K2DH"},
#endif
};
#endif

/* ABS axes parameter range [um/s^2] (for input event) */
#define GRAVITY_EARTH		9806550
#define ABSMAX_2G		(GRAVITY_EARTH * 2)
#define ABSMIN_2G		(-GRAVITY_EARTH * 2)
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
#define HAL_RESOLUTION	1024
#else
#define HAL_RESOLUTION	256
#endif
#define delay_to_jiffies(d)	((d) ? msecs_to_jiffies(d) : 1)
#define actual_delay(d)	(jiffies_to_msecs(delay_to_jiffies(d)))

/* ------------ *
   Function prototype declaration
 * -------------- */
static struct yas_acc_private_data *yas_acc_get_data(void);
static void yas_acc_set_data(struct yas_acc_private_data *);

static int yas_acc_lock(void);
static int yas_acc_unlock(void);
static int yas_acc_i2c_open(void);
static int yas_acc_i2c_close(void);
static int yas_acc_i2c_write(uint8_t, const uint8_t *, int);
static int yas_acc_i2c_read(uint8_t, uint8_t *, int);
static void yas_acc_msleep(int);

static int yas_acc_core_driver_init(struct yas_acc_private_data *);
static void yas_acc_core_driver_fini(struct yas_acc_private_data *);
static int yas_acc_get_enable(struct yas_acc_driver *);
static int yas_acc_set_enable(struct yas_acc_driver *, int);
static int yas_acc_get_delay(struct yas_acc_driver *);
static int yas_acc_set_delay(struct yas_acc_driver *, int);
static int yas_acc_get_position(struct yas_acc_driver *);
static int yas_acc_set_position(struct yas_acc_driver *, int);
static int yas_acc_get_threshold(struct yas_acc_driver *);
static int yas_acc_set_threshold(struct yas_acc_driver *, int);
static int yas_acc_get_filter_enable(struct yas_acc_driver *);
static int yas_acc_set_filter_enable(struct yas_acc_driver *, int);
static int yas_acc_measure(struct yas_acc_driver *, struct yas_acc_data *);
static int yas_acc_input_init(struct yas_acc_private_data *);
static void yas_acc_input_fini(struct yas_acc_private_data *);
static int yas_acc_set_offset(struct yas_acc_driver *, struct yas_vector *);
static int yas_acc_set_enable_factory_test(struct yas_acc_driver *, int);

static ssize_t yas_acc_enable_show
	(struct device *, struct device_attribute *, char *);
static ssize_t yas_acc_enable_store
	(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t yas_acc_delay_show
	(struct device *, struct device_attribute *, char *);
static ssize_t yas_acc_delay_store
	(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t yas_acc_position_show
	(struct device *, struct device_attribute *, char *);
static ssize_t yas_acc_position_store
	(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t yas_acc_threshold_show
	(struct device *, struct device_attribute *, char *);
static ssize_t yas_acc_threshold_store
	(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t yas_acc_filter_enable_show
	(struct device *, struct device_attribute *, char *);
static ssize_t yas_acc_filter_enable_store
	(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t yas_acc_wake_store
	(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t yas_acc_private_data_show
	(struct device *, struct device_attribute *, char *);
#if DEBUG
static ssize_t yas_acc_debug_reg_show
	(struct device *, struct device_attribute *, char *);
static int yas_acc_suspend(struct i2c_client *, pm_message_t);
static int yas_acc_resume(struct i2c_client *);
#endif

static void yas_acc_work_func(struct work_struct *);
static int yas_acc_probe(struct i2c_client *, const struct i2c_device_id *);
static int yas_acc_remove(struct i2c_client *);
static int yas_acc_suspend(struct i2c_client *, pm_message_t);
static int yas_acc_resume(struct i2c_client *);

/*----------------------------------------------- *
   Driver private data
 * ------------------------------------------------ */
struct yas_acc_private_data {
	struct mutex driver_mutex;
	struct mutex data_mutex;
	struct mutex enable_mutex;
	struct i2c_client *client;
	struct input_dev *input;
	struct yas_acc_driver *driver;
	struct delayed_work work;
	struct work_struct alert_work;
	struct wake_lock reactive_wake_lock;
	struct yas_acc_data last;
	int suspend_enable;
	int calibrate;
#if DEBUG
	struct mutex suspend_mutex;
	int suspend;
#endif
#ifdef YAS_SENSOR_KERNEL_DEVFILE_INTERFACE
	struct list_head devfile_list;
#endif
	struct device *accel_sensor_device;
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
	struct yas_vector cal_data;
#else
	struct accel_cal cal_data;
#endif
	struct acc_platform_data *acc_pdata;
	int enabled;
#ifdef CONFIG_YAS_ACC_MULTI_SUPPORT
	int used_chip;
#endif
	atomic_t reactive_state;
	atomic_t reactive_enable;
	bool factory_mode;
};

static struct yas_acc_private_data *yas_acc_private_data;
static struct yas_acc_private_data *yas_acc_get_data(void)
{
	return yas_acc_private_data;
}

static int read_accel_raw_xyz(struct yas_vector *acc)
{
	struct yas_acc_data accel;
	struct yas_acc_private_data *data = yas_acc_get_data();

	mutex_lock(&data->data_mutex);

	yas_acc_measure(data->driver, &accel);

	mutex_unlock(&data->data_mutex);

	acc->v[0] = accel.xyz.v[0];

	acc->v[1] = accel.xyz.v[1];

	acc->v[2] = accel.xyz.v[2];

	return 0;
}


static int accel_open_calibration(void)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;
	struct yas_acc_private_data *data = yas_acc_get_data();

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(data->acc_pdata->cal_path, O_RDONLY,
		S_IRUGO | S_IWUSR | S_IWGRP);

	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
		(char *)&data->cal_data, 3 * sizeof(s32), &cal_filp->f_pos);
	if (err != 3 * sizeof(s32)) {
#else
		(char *)&data->cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
#endif
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	pr_info("%s: (%d,%d,%d)\n", __func__,
		data->cal_data.v[0], data->cal_data.v[1], data->cal_data.v[2]);
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
	if (data->cal_data.v[0] == 0
		&& data->cal_data.v[1] == 0
		&& data->cal_data.v[2] == 0)
		data->calibrate = CAL_FAIL;
	else
		data->calibrate = CAL_SUCCESS;
#endif
	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}



static int accel_do_calibrate(int enable)
{
	struct file *cal_filp;
	int sum[3] = { 0, };
	int err;
	int i;
	struct yas_vector raw;
	mm_segment_t old_fs;
	struct yas_acc_private_data *data = yas_acc_get_data();

	mutex_lock(&data->data_mutex);
	yas_acc_set_enable_factory_test(data->driver, 1);
	mutex_unlock(&data->data_mutex);

	for (i = 0; i < 100; i++) {
		err = read_accel_raw_xyz(&raw);
		if (err < 0) {
			pr_err("%s: accel_read_accel_raw_xyz() "
				"failed in the %dth loop\n", __func__, i);
			return err;
		}

		sum[0] += raw.v[0];
		sum[1] += raw.v[1];
		sum[2] += raw.v[2];
	}

	mutex_lock(&data->data_mutex);
	yas_acc_set_enable_factory_test(data->driver, 0);
	mutex_unlock(&data->data_mutex);

	if (enable) {
		data->cal_data.v[0] =
			(sum[0] / 100);
		data->cal_data.v[1] =
			(sum[1] / 100);
		data->cal_data.v[2] =
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
			((sum[2] / 100) - HAL_RESOLUTION);
		data->calibrate = CAL_SUCCESS;
#else
			((sum[2] / 100) - 256);
#endif
	} else {
		data->cal_data.v[0] = 0;
		data->cal_data.v[1] = 0;
		data->cal_data.v[2] = 0;
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
		data->calibrate = CAL_FAIL;
#endif
	}

	pr_info("%s: cal data (%d,%d,%d)\n", __func__,
			data->cal_data.v[0], data->cal_data.v[1],
				data->cal_data.v[2]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);


	cal_filp = filp_open(data->acc_pdata->cal_path,
			O_CREAT | O_TRUNC | O_WRONLY,
			S_IRUGO | S_IWUSR | S_IWGRP);

	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->write(cal_filp,
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
		(char *)&data->cal_data, 3 * sizeof(s32), &cal_filp->f_pos);
	if (err != 3 * sizeof(s32)) {
#else
		(char *)&data->cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
#endif
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
		data->calibrate = CAL_FAIL;
#endif
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static void yas_acc_set_data(struct yas_acc_private_data *data)
{
	yas_acc_private_data = data;
}

#ifdef YAS_SENSOR_KERNEL_DEVFILE_INTERFACE

#include <linux/miscdevice.h>
#define SENSOR_NAME "accelerometer"
#define MAX_COUNT (64)

struct sensor_device {
	struct list_head list;
	struct mutex lock;
	wait_queue_head_t waitq;
	struct input_event events[MAX_COUNT];
	int head, num_event;
};

static void get_time_stamp(struct timeval *tv)
{
	struct timespec ts;
	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
}

static void make_event(struct input_event *ev, int type, int code, int value)
{
	struct timeval tv;
	get_time_stamp(&tv);
	ev->type = type;
	ev->code = code;
	ev->value = value;
	ev->time = tv;
}

static void
make_event_w_time(struct input_event *ev, int type,
		  int code, int value, struct timeval *tv)
{
	ev->type = type;
	ev->code = code;
	ev->value = value;
	ev->time = *tv;
}

static void sensor_enq(struct sensor_device *kdev, struct input_event *ev)
{
	int idx;

	idx = kdev->head + kdev->num_event;
	if (MAX_COUNT <= idx)
		idx -= MAX_COUNT;
	kdev->events[idx] = *ev;
	kdev->num_event++;
	if (MAX_COUNT < kdev->num_event) {
		kdev->num_event = MAX_COUNT;
		kdev->head++;
		if (MAX_COUNT <= kdev->head)
			kdev->head -= MAX_COUNT;
	}
}

static int sensor_deq(struct sensor_device *kdev, struct input_event *ev)
{
	if (kdev->num_event == 0)
		return 0;

	*ev = kdev->events[kdev->head];
	kdev->num_event--;
	kdev->head++;
	if (MAX_COUNT <= kdev->head)
		kdev->head -= MAX_COUNT;
	return 1;
}

static void
sensor_event(struct list_head *devlist, struct input_event *ev, int num)
{
	struct sensor_device *kdev;
	int i;
	list_for_each_entry(kdev, devlist, list) {
		mutex_lock(&kdev->lock);
		for (i = 0; i < num; i++)
			sensor_enq(kdev, &ev[i]);
		mutex_unlock(&kdev->lock);
		wake_up_interruptible(&kdev->waitq);
	}
}

static ssize_t
sensor_write(struct file *f, const char __user * buf, size_t count,
	loff_t *pos)
{
	struct yas_acc_private_data *data = yas_acc_get_data();
	struct sensor_device *kdev;
	struct input_event ev[MAX_COUNT];
	int num, i;

	if (count < sizeof(struct input_event))
		return -EINVAL;

	num = count / sizeof(struct input_event);
	if (MAX_COUNT < num)
		num = MAX_COUNT;
	if (copy_from_user(ev, buf, num * sizeof(struct input_event)))
		return -EFAULT;

	list_for_each_entry(kdev, &data->devfile_list, list) {
		mutex_lock(&kdev->lock);
		for (i = 0; i < num; i++)
			sensor_enq(kdev, &ev[i]);

		mutex_unlock(&kdev->lock);
		wake_up_interruptible(&kdev->waitq);
	}

	return count;
}

static ssize_t
sensor_read(struct file *f, char __user * buf, size_t count, loff_t * pos)
{
	struct sensor_device *kdev = f->private_data;
	int rt, num;
	struct input_event ev[MAX_COUNT];

	if (count < sizeof(struct input_event))
		return -EINVAL;

	rt = wait_event_interruptible(kdev->waitq, kdev->num_event != 0);
	if (rt)
		return rt;

	mutex_lock(&kdev->lock);
	for (num = 0; num < count / sizeof(struct input_event); num++) {
		if (!sensor_deq(kdev, &ev[num]))
			break;
	}
	mutex_unlock(&kdev->lock);

	if (copy_to_user(buf, ev, num * sizeof(struct input_event)))
		return -EFAULT;

	return num * sizeof(struct input_event);
}

static unsigned int sensor_poll(struct file *f, struct poll_table_struct *wait)
{
	struct sensor_device *kdev = f->private_data;

	poll_wait(f, &kdev->waitq, wait);
	if (kdev->num_event != 0)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int sensor_open(struct inode *inode, struct file *f)
{
	struct yas_acc_private_data *data = yas_acc_get_data();
	struct sensor_device *kdev;

	kdev = kzalloc(sizeof(struct sensor_device), GFP_KERNEL);
	if (!kdev)
		return -ENOMEM;

	mutex_init(&kdev->lock);
	init_waitqueue_head(&kdev->waitq);
	f->private_data = kdev;
	kdev->head = 0;
	kdev->num_event = 0;
	list_add(&kdev->list, &data->devfile_list);
	return 0;
}

static int sensor_release(struct inode *inode, struct file *f)
{
	struct sensor_device *kdev = f->private_data;

	list_del(&kdev->list);
	kfree(kdev);

	return 0;
}

const struct file_operations sensor_fops = {
	.owner = THIS_MODULE,
	.open = sensor_open,
	.release = sensor_release,
	.write = sensor_write,
	.read = sensor_read,
	.poll = sensor_poll,
};

static struct miscdevice sensor_devfile = {
	.name = SENSOR_NAME,
	.fops = &sensor_fops,
	.minor = MISC_DYNAMIC_MINOR,
};

#endif

/* --------------- *
   Accelerlomete core driver callback function
 * --------------- */
static int yas_acc_lock(void)
{
	struct yas_acc_private_data *data = yas_acc_get_data();

	mutex_lock(&data->driver_mutex);

	return 0;
}

static int yas_acc_unlock(void)
{
	struct yas_acc_private_data *data = yas_acc_get_data();

	mutex_unlock(&data->driver_mutex);

	return 0;
}

static int yas_acc_i2c_open(void)
{
	return 0;
}

static int yas_acc_i2c_close(void)
{
	return 0;
}

static int yas_acc_i2c_write(uint8_t adr, const uint8_t *buf, int len)
{
	struct yas_acc_private_data *data = yas_acc_get_data();
	struct i2c_msg msg[2];
	char buffer[16];
	uint8_t reg;
	int err;
	int i;

	if (len > 15) {
		err = -1;
		goto done;
	}
	reg = adr;
	buffer[0] = reg;
	for (i = 0; i < len; i++)
		buffer[i + 1] = buf[i];

	msg[0].addr = data->client->addr;
	msg[0].flags = 0;
	msg[0].len = len + 1;
	msg[0].buf = buffer;
	err = i2c_transfer(data->client->adapter, msg, 1);
	if (err != 1) {
		dev_err(&data->client->dev,
			"i2c_transfer() write error: slave_addr=%02x, reg_addr=%02x, err=%d\n",
			data->client->addr, adr, err);
	} else
		err = 0;
done:
	return err;
}

static int yas_acc_i2c_read(uint8_t adr, uint8_t *buf, int len)
{
	struct yas_acc_private_data *data = yas_acc_get_data();
	struct i2c_msg msg[2];
	uint8_t reg;
	int err;
	int retry = 2;

	reg = adr;
	msg[0].addr = data->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;
	msg[1].addr = data->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	do {
		err = i2c_transfer(data->client->adapter, msg, 2);
		if (err != 2) {
			dev_err(&data->client->dev,
				"i2c_transfer() read error: slave_addr=%02x, reg_addr=%02x, err=%d\n",
				data->client->addr, adr, err);
			if (retry < 1)
				goto done;
		} else {
			err = 0;
			goto done;
		}
	} while (unlikely(err < 0) && retry--);
done:
	return err;
}

static void yas_acc_msleep(int msec)
{
	usleep_range(msec * 999, msec * 1000);
}

/* ------------------------------- *
   Accelerometer core driver access function
 * -------------------------------- */
static int yas_acc_core_driver_init(struct yas_acc_private_data *data)
{
	struct yas_acc_driver_callback *cbk;
	struct yas_acc_driver *driver;
	int err;

	data->driver = driver = kzalloc(sizeof(struct yas_acc_driver),
					GFP_KERNEL);
	if (!driver) {
		err = -ENOMEM;
		return err;
	}

	cbk = &driver->callback;
	cbk->lock = yas_acc_lock;
	cbk->unlock = yas_acc_unlock;
	cbk->device_open = yas_acc_i2c_open;
	cbk->device_close = yas_acc_i2c_close;
	cbk->device_write = yas_acc_i2c_write;
	cbk->device_read = yas_acc_i2c_read;
	cbk->msleep = yas_acc_msleep;

#ifdef CONFIG_YAS_ACC_MULTI_SUPPORT
	if (data->used_chip == K2DH_ENABLED)
		err = yas_acc_driver_lis3dh_init(driver);
	else if (data->used_chip == BMA25X_ENABLED)
		err = yas_acc_driver_BMA25X_init(driver);
#else
	err = yas_acc_driver_init(driver);
#endif
	if (err != YAS_NO_ERROR) {
		kfree(driver);
		return err;
	}

	err = driver->init();
	if (err != YAS_NO_ERROR) {
		kfree(driver);
		return err;
	}
	if (data->acc_pdata) {
		if (data->acc_pdata->orientation) {
			pr_info("%s: set from board file.\n", __func__);
			err = driver->set_position(
				data->acc_pdata->orientation
				- YAS532_POSITION_OFFSET);
		} else {
			pr_info("%s: set from defconfig.\n", __func__);
			err = driver->set_position(
				CONFIG_INPUT_YAS_ACCELEROMETER_POSITION);
		}
	} else {
		err = driver->set_position(
			CONFIG_INPUT_YAS_ACCELEROMETER_POSITION);
	}
	if (err != YAS_NO_ERROR) {
		kfree(driver);
		return err;
	}
	pr_info("%s: accelerometer position is %d\n", __func__,
				driver->get_position());
	return 0;
}

static void yas_acc_core_driver_fini(struct yas_acc_private_data *data)
{
	struct yas_acc_driver *driver = data->driver;

	driver->term();
	kfree(driver);
}

static int yas_acc_get_enable(struct yas_acc_driver *driver)
{
	int enable;

	enable = driver->get_enable();

	return enable;
}

static int yas_acc_set_enable(struct yas_acc_driver *driver, int enable)
{
	struct yas_acc_private_data *data = yas_acc_get_data();
	int delay = driver->get_delay();

	if (yas_acc_get_enable(data->driver) != enable) {
		if (enable) {
			driver->set_enable(enable);
			schedule_delayed_work(&data->work,
					      delay_to_jiffies(delay) + 1);
		} else {
			cancel_delayed_work_sync(&data->work);
			driver->set_enable(enable);
		}
	}

	return 0;
}

static int yas_acc_set_enable_factory_test(struct yas_acc_driver *driver,
	int enable)
{
	struct yas_acc_private_data *data = yas_acc_get_data();

	if (!(data->enabled))
		driver->set_enable(enable);

	return 0;
}

static int yas_acc_get_delay(struct yas_acc_driver *driver)
{
	return driver->get_delay();
}

static int yas_acc_set_delay(struct yas_acc_driver *driver, int delay)
{
	struct yas_acc_private_data *data = yas_acc_get_data();

	mutex_lock(&data->enable_mutex);

	if (yas_acc_get_enable(data->driver)) {
		cancel_delayed_work_sync(&data->work);
		driver->set_delay(actual_delay(delay));
		schedule_delayed_work(&data->work, delay_to_jiffies(delay) + 1);
	} else {
		driver->set_delay(actual_delay(delay));
	}

	mutex_unlock(&data->enable_mutex);

	return 0;
}

static int yas_acc_get_offset(struct yas_acc_driver *driver,
			      struct yas_vector *offset)
{
	return driver->get_offset(offset);
}

static int yas_acc_set_offset(struct yas_acc_driver *driver,
			      struct yas_vector *offset)
{
	return driver->set_offset(offset);
}

static int yas_acc_get_position(struct yas_acc_driver *driver)
{
	return driver->get_position();
}

static int yas_acc_set_position(struct yas_acc_driver *driver, int position)
{
	return driver->set_position(position);
}

static int yas_acc_get_threshold(struct yas_acc_driver *driver)
{
	struct yas_acc_filter filter;

	driver->get_filter(&filter);

	return filter.threshold;
}

static int yas_acc_set_threshold(struct yas_acc_driver *driver, int threshold)
{
	struct yas_acc_filter filter;

	filter.threshold = threshold;

	return driver->set_filter(&filter);
}

static int yas_acc_get_filter_enable(struct yas_acc_driver *driver)
{
	return driver->get_filter_enable();
}

static int yas_acc_set_filter_enable(struct yas_acc_driver *driver, int enable)
{
	return driver->set_filter_enable(enable);
}

static int yas_acc_measure(struct yas_acc_driver *driver,
			   struct yas_acc_data *accel)
{
	int err;

	err = driver->measure(accel);
	if (err != YAS_NO_ERROR)
		return err;

#if 0
	pr_info("data(%10d %10d %10d) raw(%5d %5d %5d)\n",
	       accel->xyz.v[0], accel->xyz.v[1], accel->xyz.v[2],
	       accel->raw.v[0], accel->raw.v[1], accel->raw.v[2]);
#endif

	return err;
}

/* ----------------- *
   Input device interface
 * ------------------ */
static int yas_acc_input_init(struct yas_acc_private_data *data)
{
	struct input_dev *dev;
	int err;

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;
	dev->name = "accelerometer";
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_capability(dev, EV_ABS, ABS_RUDDER);
	input_set_abs_params(dev, ABS_X, ABSMIN_2G, ABSMAX_2G, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN_2G, ABSMAX_2G, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN_2G, ABSMAX_2G, 0, 0);
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	data->input = dev;

	return 0;
}

static void yas_acc_input_fini(struct yas_acc_private_data *data)
{
	struct input_dev *dev = data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

static ssize_t yas_acc_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	int enable = 0;

	mutex_lock(&data->enable_mutex);

	enable = yas_acc_get_enable(data->driver);

	mutex_unlock(&data->enable_mutex);

	return sprintf(buf, "%d\n", enable);
}

static ssize_t yas_acc_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	unsigned long enable = 0;

	if (strict_strtoul(buf, 10, &enable))
		return -EINVAL;
	accel_open_calibration();

	mutex_lock(&data->enable_mutex);

	yas_acc_set_enable(data->driver, enable);

	mutex_unlock(&data->enable_mutex);

	data->enabled = enable;

	return count;
}

static ssize_t yas_acc_delay_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", yas_acc_get_delay(data->driver));
}

static ssize_t yas_acc_delay_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	unsigned long delay = 0;
	if (strict_strtoul(buf, 10, &delay))
		return -EINVAL;

	yas_acc_set_delay(data->driver, delay);

	return count;
}

static ssize_t yas_acc_offset_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	struct yas_vector offset;
	yas_acc_get_offset(data->driver, &offset);
	return sprintf(buf, "%d %d %d\n", offset.v[0], offset.v[1],
		       offset.v[2]);
}

static ssize_t yas_acc_offset_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	struct yas_vector offset;

	sscanf(buf, "%d %d %d", &offset.v[0], &offset.v[1], &offset.v[2]);

	yas_acc_set_offset(data->driver, &offset);

	return count;
}

static ssize_t yas_acc_position_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", yas_acc_get_position(data->driver));
}

static ssize_t yas_acc_position_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	unsigned long position = 0;
	if (strict_strtoul(buf, 10, &position))
		return -EINVAL;

	yas_acc_set_position(data->driver, position);

	return count;
}

static ssize_t yas_acc_threshold_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", yas_acc_get_threshold(data->driver));
}

static ssize_t yas_acc_threshold_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	unsigned long threshold = 0;
	if (strict_strtoul(buf, 10, &threshold))
		return -EINVAL;

	yas_acc_set_threshold(data->driver, threshold);

	return count;
}

static ssize_t yas_acc_filter_enable_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", yas_acc_get_filter_enable(data->driver));
}

static ssize_t yas_acc_filter_enable_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	unsigned long enable = 0;
	if (strict_strtoul(buf, 10, &enable))
		return -EINVAL;

	yas_acc_set_filter_enable(data->driver, enable);

	return count;
}

static ssize_t yas_acc_wake_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	static atomic_t serial = ATOMIC_INIT(0);
#ifdef YAS_SENSOR_KERNEL_DEVFILE_INTERFACE
	struct yas_acc_private_data *data = input_get_drvdata(input);
	struct input_event ev[1];
	make_event(ev, EV_ABS, ABS_MISC, atomic_inc_return(&serial));
	sensor_event(&data->devfile_list, ev, 1);
#else
	input_report_abs(input, ABS_MISC, atomic_inc_return(&serial));
#endif

	return count;
}

static ssize_t yas_acc_private_data_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	struct yas_acc_data accel;

	mutex_lock(&data->data_mutex);
	accel = data->last;
	mutex_unlock(&data->data_mutex);

	return sprintf(buf, "%d %d %d\n", accel.xyz.v[0], accel.xyz.v[1],
		       accel.xyz.v[2]);
}

#if DEBUG
#if YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA150
#define ADR_MAX (0x16)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA222 || \
	YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA250 ||	\
	YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA254
#define ADR_MAX (0x40)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXSD9
#define ADR_MAX (0x0f)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTE9
#define ADR_MAX (0x5c)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTF9
#define ADR_MAX (0x60)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DL  || \
	YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DLH || \
	YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DLM
#define ADR_MAX (0x40)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH
#define ADR_MAX (0x3e)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_ADXL345
#define ADR_MAX (0x3a)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_ADXL346
#define ADR_MAX (0x3d)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_MMA8452Q || \
	YAS_ACC_DRIVER == YAS_ACC_DRIVER_MMA8453Q
#define ADR_MAX (0x32)
#else
#define ADR_MAX (0x16)
#endif
static uint8_t reg[ADR_MAX];

static ssize_t yas_acc_debug_reg_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;
	ssize_t count = 0;
	int ret;
	int i;

	memset(reg, -1, ADR_MAX);
	for (i = 0; i < ADR_MAX; i++) {
		ret = data->driver->get_register(i, &reg[i]);
		if (ret != 0) {
			dev_err(&client->dev, "get_register() erorr %d (%d)\n",
				ret, i);
		} else {
			count += sprintf(&buf[count], "%02x: %d\n", i, reg[i]);
		}
	}

	return count;
}

static ssize_t yas_acc_debug_suspend_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	int suspend = 0;

	mutex_lock(&data->suspend_mutex);

	suspend = sprintf(buf, "%d\n", data->suspend);

	mutex_unlock(&data->suspend_mutex);

	return suspend;
}

static ssize_t yas_acc_debug_suspend_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct yas_acc_private_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;
	unsigned long suspend = 0;
	pm_message_t msg;

	if (strict_strtoul(buf, 10, &suspend))
		return -EINVAL;

	mutex_lock(&data->suspend_mutex);

	if (suspend) {
		yas_acc_suspend(client, msg);
		data->suspend = 1;
	} else {
		yas_acc_resume(client);
		data->suspend = 0;
	}

	mutex_unlock(&data->suspend_mutex);

	return count;
}
#endif /* DEBUG */

static DEVICE_ATTR(enable,
		   S_IRUGO | S_IWUSR | S_IWGRP,
		   yas_acc_enable_show, yas_acc_enable_store);
static DEVICE_ATTR(delay,
		   S_IRUGO | S_IWUSR | S_IWGRP,
		   yas_acc_delay_show, yas_acc_delay_store);
static DEVICE_ATTR(offset,
		   S_IRUGO | S_IWUSR,
		   yas_acc_offset_show, yas_acc_offset_store);
static DEVICE_ATTR(position,
		   S_IRUGO | S_IWUSR,
		   yas_acc_position_show, yas_acc_position_store);
static DEVICE_ATTR(threshold,
		   S_IRUGO | S_IWUSR,
		   yas_acc_threshold_show, yas_acc_threshold_store);
static DEVICE_ATTR(filter_enable,
		   S_IRUGO | S_IWUSR,
		   yas_acc_filter_enable_show, yas_acc_filter_enable_store);
static DEVICE_ATTR(wake, S_IWUSR | S_IWGRP, NULL, yas_acc_wake_store);
static DEVICE_ATTR(data, S_IRUGO, yas_acc_private_data_show, NULL);
#if DEBUG
static DEVICE_ATTR(debug_reg, S_IRUGO, yas_acc_debug_reg_show, NULL);
static DEVICE_ATTR(debug_suspend,
		   S_IRUGO | S_IWUSR,
		   yas_acc_debug_suspend_show, yas_acc_debug_suspend_store);
#endif /* DEBUG */

static struct attribute *yas_acc_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_offset.attr,
	&dev_attr_position.attr,
	&dev_attr_threshold.attr,
	&dev_attr_filter_enable.attr,
	&dev_attr_wake.attr,
	&dev_attr_data.attr,
#if DEBUG
	&dev_attr_debug_reg.attr,
	&dev_attr_debug_suspend.attr,
#endif /* DEBUG */
	NULL
};

static struct attribute_group yas_acc_attribute_group = {
	.attrs = yas_acc_attributes
};

static void yas_acc_work_func(struct work_struct *work)
{
	struct yas_acc_private_data *data =
	    container_of((struct delayed_work *)work,
			 struct yas_acc_private_data, work);
	struct yas_acc_data accel, last;
	unsigned long delay = delay_to_jiffies(yas_acc_get_delay(data->driver));
	static int cnt;
#ifdef YAS_SENSOR_KERNEL_DEVFILE_INTERFACE
	struct input_event ev[4];
	struct timeval tv;
#endif

	accel.xyz.v[0] = accel.xyz.v[1] = accel.xyz.v[2] = 0;
	yas_acc_measure(data->driver, &accel);

	mutex_lock(&data->data_mutex);
	last = data->last;
	mutex_unlock(&data->data_mutex);

#ifdef YAS_SENSOR_KERNEL_DEVFILE_INTERFACE
	get_time_stamp(&tv);
	make_event_w_time(&ev[0], EV_ABS, ABS_X, accel.xyz.v[0], &tv);
	make_event_w_time(&ev[1], EV_ABS, ABS_Y, accel.xyz.v[1], &tv);
	make_event_w_time(&ev[2], EV_ABS, ABS_Z, accel.xyz.v[2], &tv);
	make_event_w_time(&ev[3], EV_SYN, 0, 0, &tv);
	sensor_event(&data->devfile_list, ev, 4);
#else
	accel.xyz.v[0] = accel.xyz.v[0] - data->cal_data.v[0];
	accel.xyz.v[1] = accel.xyz.v[1] - data->cal_data.v[1];
	accel.xyz.v[2] = accel.xyz.v[2] - data->cal_data.v[2];

	input_report_abs(data->input, ABS_X, accel.xyz.v[0]);
	input_report_abs(data->input, ABS_Y, accel.xyz.v[1]);
	input_report_abs(data->input, ABS_Z, accel.xyz.v[2]);
	if (last.xyz.v[0] == accel.xyz.v[0]
	    && last.xyz.v[1] == accel.xyz.v[1]
	    && last.xyz.v[2] == accel.xyz.v[2]) {
		input_report_abs(data->input, ABS_RUDDER, cnt++);
	}
	input_sync(data->input);
#endif

	mutex_lock(&data->data_mutex);
	data->last = accel;
	mutex_unlock(&data->data_mutex);

	schedule_delayed_work(&data->work, delay);
}


static ssize_t acc_data_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_acc_data accel;
	struct yas_acc_private_data *data = yas_acc_get_data();
	int32_t x;
	int32_t y;
	int32_t z;

	mutex_lock(&data->data_mutex);
	yas_acc_set_enable_factory_test(data->driver, 1);
	usleep_range(1000, 2000);
	yas_acc_measure(data->driver, &accel);
	yas_acc_set_enable_factory_test(data->driver, 1);
	mutex_unlock(&data->data_mutex);
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
	x = (accel.xyz.v[0] - data->cal_data.v[0]);
	y = (accel.xyz.v[1] - data->cal_data.v[1]);
	z = (accel.xyz.v[2] - data->cal_data.v[2]);

	return sprintf(buf, "%d, %d, %d\n", x, y, z);
#else
	x = (accel.xyz.v[0] - data->cal_data.v[0]) * 4;
	y = (accel.xyz.v[1] - data->cal_data.v[1]) * 4;
	z = (accel.xyz.v[2] - data->cal_data.v[2]) * 4;
	pr_info("acc_data_read x: %d y: %d z: %d\n",
		x, y, z);

	return sprintf(buf, "%d, %d, %d\n", x, y, z);
#endif
}

static DEVICE_ATTR(raw_data, S_IRUGO, acc_data_read,
	NULL);

static ssize_t accel_calibration_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int count = 0;
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
	s32 x;
	s32 y;
	s32 z;
#else
	s16 x;
	s16 y;
	s16 z;
#endif
	int err;
	struct yas_acc_private_data *data = yas_acc_get_data();

	x = data->cal_data.v[0];
	y = data->cal_data.v[1];
	z = data->cal_data.v[2];
	pr_info(" accel_calibration_show %d %d %d\n", x, y, z);

	if (!x && !y && !z)
		err = -1;
	else
		err = 1;

	count = sprintf(buf, "%d %d %d %d\n", err, x, y, z);
	return count;
}

static ssize_t accel_calibration_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int err;
	int count;
	unsigned long enable;
#if (YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH)
	s32 x;
	s32 y;
	s32 z;
#else
	s16 x;
	s16 y;
	s16 z;
#endif
	char tmp[64];

	struct yas_acc_private_data *data = yas_acc_get_data();
	if (strict_strtoul(buf, 10, &enable))
		return -EINVAL;

	err = accel_do_calibrate(enable);
	if (err < 0)
		pr_err("%s: accel_do_calibrate() failed\n", __func__);

	x = data->cal_data.v[0];
	y = data->cal_data.v[1];
	z = data->cal_data.v[2];

	pr_info("accel_calibration_store %d %d %d\n", x, y, z);
	if (err > 0)
		err = 0;
	count = sprintf(tmp, "%d\n", err);
	return count;
}

static DEVICE_ATTR(calibration, 0664, accel_calibration_show,
	accel_calibration_store);

#ifdef CONFIG_INPUT_YAS_ACC_ALERT_INT
static ssize_t yas_acc_reactive_enable_show(struct device *dev,
					struct device_attribute
						*attr, char *buf)
{
	struct yas_acc_private_data *data = yas_acc_get_data();
	pr_info("%s: state =%d\n", __func__,
		atomic_read(&data->reactive_state));
	return sprintf(buf, "%d\n",
		atomic_read(&data->reactive_state));
}

static ssize_t yas_acc_reactive_enable_store(struct device *dev,
					struct device_attribute
						*attr, const char *buf,
							size_t count)
{
	struct yas_acc_private_data *data = yas_acc_get_data();
	bool onoff = false;
	bool factory_test = false;
	unsigned long value = 0;
	int err = count;

	if (strict_strtoul(buf, 10, &value)) {
		err = -EINVAL;
		goto done;
	}
	switch (value) {
	case 0:
		break;
	case 1:
		onoff = true;
		break;
	case 2:
		onoff = true;
		factory_test = true;
		break;
	default:
		err = -EINVAL;
		pr_err("%s: invalid value %d\n", __func__, *buf);
		goto done;
	}

	if (data->client->irq) {
		if (!value) {
			disable_irq_wake(data->client->irq);
			disable_irq(data->client->irq);
		} else {
			enable_irq(data->client->irq);
			enable_irq_wake(data->client->irq);
		}
	}
	mutex_lock(&data->data_mutex);
	atomic_set(&data->reactive_enable, onoff);
	if (data->client->irq)
		data->driver->set_motion_interrupt(onoff, factory_test);
	atomic_set(&data->reactive_state, false);
	mutex_unlock(&data->data_mutex);

	pr_info("%s: onoff = %d, state =%d\n", __func__,
		atomic_read(&data->reactive_enable),
		atomic_read(&data->reactive_state));
done:
	return err;
}
static struct device_attribute dev_attr_reactive_alert =
	__ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
	yas_acc_reactive_enable_show,
	yas_acc_reactive_enable_store);
#endif

static ssize_t accel_vendor_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
#ifdef CONFIG_YAS_ACC_MULTI_SUPPORT
	struct yas_acc_private_data *data = yas_acc_get_data();
	return sprintf(buf, "%s\n", vendor_name[data->used_chip]);
#else
	return sprintf(buf, "%s\n", VENDOR_NAME);
#endif
}
static struct device_attribute dev_attr_accel_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, accel_vendor_show, NULL);

static ssize_t accel_name_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
#ifdef CONFIG_YAS_ACC_MULTI_SUPPORT
	struct yas_acc_private_data *data = yas_acc_get_data();
	return sprintf(buf, "%s\n", chip_name[data->used_chip]);
#else
	return sprintf(buf, "%s\n", CHIP_NAME);
#endif
}
static struct device_attribute dev_attr_accel_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, accel_name_show, NULL);

static struct device_attribute *accel_sensor_attrs[] = {
	&dev_attr_raw_data,
	&dev_attr_calibration,
#ifdef CONFIG_INPUT_YAS_ACC_ALERT_INT
	&dev_attr_reactive_alert,
#endif
	&dev_attr_accel_sensor_vendor,
	&dev_attr_accel_sensor_name,
	NULL,
};

#ifdef CONFIG_INPUT_YAS_ACC_ALERT_INT
static void yas_work_func_alert(struct work_struct *work)
{
	struct yas_acc_private_data *data =
		container_of(work, struct yas_acc_private_data, alert_work);
	int result;

	result = data->driver->get_motion_interrupt();

	if (result || data->factory_mode) {
		/*handle motion recognition*/
		atomic_set(&data->reactive_state, true);
		data->factory_mode = false;
		pr_info("%s: motion interrupt happened\n",
			__func__);
		wake_lock_timeout(&data->reactive_wake_lock,
			msecs_to_jiffies(2000));
	}
}

static irqreturn_t yas_acc_irq_thread(int irq, void *dev)
{
	struct yas_acc_private_data *data = yas_acc_get_data();
	schedule_work(&data->alert_work);
	return IRQ_HANDLED;
}
#endif

static int yas_acc_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct yas_acc_private_data *data;
	struct acc_platform_data *pdata;
#ifdef CONFIG_YAS_ACC_MULTI_SUPPORT
	struct accel_platform_data *platform_data;

	if (client->dev.platform_data != NULL)
		platform_data = client->dev.platform_data;

#endif
	int err;

	/* Setup private data */
	data = kzalloc(sizeof(struct yas_acc_private_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto ERR1;
	}
	yas_acc_set_data(data);
#ifdef CONFIG_YAS_ACC_MULTI_SUPPORT
	data->used_chip = platform_data->used_chip;
#endif

	pdata = (struct acc_platform_data *) client->dev.platform_data;
	data->acc_pdata = pdata;

	mutex_init(&data->driver_mutex);
	mutex_init(&data->data_mutex);
	mutex_init(&data->enable_mutex);
#if DEBUG
	mutex_init(&data->suspend_mutex);
#endif

	/* Setup i2c client */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto ERR2;
	}
	i2c_set_clientdata(client, data);
	data->client = client;

	if (data->acc_pdata) {
		if (data->acc_pdata->ldo_on != NULL)
			data->acc_pdata->ldo_on(true);
	}
	/* Setup accelerometer core driver */
	err = yas_acc_core_driver_init(data);
	if (err < 0)
		goto ERR2;

	/* Setup driver interface */
	INIT_DELAYED_WORK(&data->work, yas_acc_work_func);

#ifdef CONFIG_INPUT_YAS_ACC_ALERT_INT
		INIT_WORK(&data->alert_work, yas_work_func_alert);
		wake_lock_init(&data->reactive_wake_lock, WAKE_LOCK_SUSPEND,
			"reactive_wake_lock");
#endif

	/* Setup input device interface */
	err = yas_acc_input_init(data);
	if (err < 0)
		goto ERR3;

	/* Setup sysfs */
	err =
	    sysfs_create_group(&data->input->dev.kobj,
			       &yas_acc_attribute_group);
	if (err < 0)
		goto ERR4;
#ifdef YAS_SENSOR_KERNEL_DEVFILE_INTERFACE
	INIT_LIST_HEAD(&data->devfile_list);
	if (misc_register(&sensor_devfile) < 0)
		goto ERR4;
#endif
	pr_info("yas_acc_probe -\n");

#ifdef CONFIG_INPUT_YAS_ACC_ALERT_INT
	if (client->irq > 0) {
		err = request_threaded_irq(client->irq,
			NULL, yas_acc_irq_thread,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"accelerometer", data);
		pr_info("%s: irq = %d\n", __func__, client->irq);
		if (err < 0) {
			pr_err("%s request_threaded_irq fail err=%d\n",
				__func__, err);
			goto ERR4;
		}
		disable_irq(client->irq);
	}
#endif
	err = sensors_register(data->accel_sensor_device,
		NULL, accel_sensor_attrs,
			"accelerometer_sensor");
	if (err) {
		pr_err("%s: cound not register accelerometer sensor device(%d).\n",
			__func__, err);
		goto out_sensor_register_failed;
	}

	return 0;

out_sensor_register_failed:
ERR4:
	yas_acc_input_fini(data);
ERR3:
#ifdef CONFIG_INPUT_YAS_ACC_ALERT_INT
	wake_lock_destroy(&data->reactive_wake_lock);
#endif
	yas_acc_core_driver_fini(data);
ERR2:
	if (data->acc_pdata) {
		if (data->acc_pdata->ldo_on != NULL)
			data->acc_pdata->ldo_on(false);
	}
	kfree(data);
ERR1:
	return err;
}

static int yas_acc_remove(struct i2c_client *client)
{
	struct yas_acc_private_data *data = i2c_get_clientdata(client);
	struct yas_acc_driver *driver = data->driver;

#ifdef YAS_SENSOR_KERNEL_DEVFILE_INTERFACE
	misc_deregister(&sensor_devfile);
#endif
	yas_acc_set_enable(driver, 0);
	sysfs_remove_group(&data->input->dev.kobj, &yas_acc_attribute_group);
	yas_acc_input_fini(data);
#ifdef CONFIG_INPUT_YAS_ACC_ALERT_INT
	wake_lock_destroy(&data->reactive_wake_lock);
#endif
	yas_acc_core_driver_fini(data);
	kfree(data);

	return 0;
}

static void yas_acc_shutdown(struct i2c_client *client)
{
	struct yas_acc_private_data *data = i2c_get_clientdata(client);
	struct yas_acc_driver *driver = data->driver;

	yas_acc_set_enable(driver, 0);
	if (data->acc_pdata) {
		if (data->acc_pdata->ldo_on != NULL)
			data->acc_pdata->ldo_on(false);
	}
}

static int yas_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct yas_acc_private_data *data = i2c_get_clientdata(client);
	struct yas_acc_driver *driver = data->driver;

	mutex_lock(&data->enable_mutex);

	data->suspend_enable = yas_acc_get_enable(driver);
	if (data->suspend_enable)
		yas_acc_set_enable(driver, 0);

	mutex_unlock(&data->enable_mutex);

	return 0;
}

static int yas_acc_resume(struct i2c_client *client)
{
	struct yas_acc_private_data *data = i2c_get_clientdata(client);
	struct yas_acc_driver *driver = data->driver;

	mutex_lock(&data->enable_mutex);

	if (data->suspend_enable)
		yas_acc_set_enable(driver, 1);

	mutex_unlock(&data->enable_mutex);

	return 0;
}

static const struct i2c_device_id yas_acc_id[] = {
	{YAS_ACC_KERNEL_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, yas_acc_id);

struct i2c_driver yas_acc_driver = {
	.driver = {
		.name = "accelerometer",
		.owner = THIS_MODULE,
	},
	.probe = yas_acc_probe,
	.remove = yas_acc_remove,
	.suspend = yas_acc_suspend,
	.resume = yas_acc_resume,
	.shutdown = yas_acc_shutdown,
	.id_table = yas_acc_id,
};

/* ----------- *
   Module init and exit
 * ------------ */
static int __init yas_acc_init(void)
{
	return i2c_add_driver(&yas_acc_driver);
}

module_init(yas_acc_init);

static void __exit yas_acc_exit(void)
{
	i2c_del_driver(&yas_acc_driver);
}

module_exit(yas_acc_exit);

MODULE_DESCRIPTION("accelerometer kernel driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(YAS_ACC_KERNEL_VERSION);

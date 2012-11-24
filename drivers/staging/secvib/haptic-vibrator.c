/* drivers/staging/haptic-vibrator.c
 *
 * Copyright (C) 2012 Samsung Electronics
 * Author: Shankar Bandal <shankar.b@samsung.com>
 *
 * Based on drivers/staging/android/timed_gpio.c
 * and tspdrv.c from Immersion Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <../../../drivers/staging/android/timed_output.h>
#include <linux/sec-vibrator.h>

#define MAX_TIMEOUT     10000 /* 10s */

/*
 * Device name and version information
 * DO NOT CHANGE - this is auto-generated
 */
#define VERSION_STR " v3.4.55.8\n"

/*
 * account extra space for future extra digits
 * in version number
 */
#define VERSION_STR_LEN					16

#define TSPDRV_MAGIC_NUMBER			0x494D4D52
#define TSPDRV_STOP_KERNEL_TIMER _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 1)
#define TSPDRV_ENABLE_AMP		_IO(TSPDRV_MAGIC_NUMBER & 0xFF, 3)
#define TSPDRV_DISABLE_AMP		_IO(TSPDRV_MAGIC_NUMBER & 0xFF, 4)
#define TSPDRV_GET_NUM_ACTUATORS _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 5)


#define VIBE_MAX_DEVICE_NAME_LENGTH		64
/* DO NOT CHANGE - SPI buffer header size */
#define SPI_HEADER_SIZE					3
/* DO NOT CHANGE - maximum number of samples */
#define VIBE_OUTPUT_SAMPLE_SIZE			50
/* This is size of SPI block for one actuator */
#define SPI_BUFFER_SIZE (VIBE_OUTPUT_SAMPLE_SIZE + SPI_HEADER_SIZE)

#define WATCHDOG_TIMEOUT    10  /* 10 timer cycles = 50ms */

struct samples_buffer {
	u_int8_t actr_index;	/* 1st byte is actuator index */
	u_int8_t bit_depth;		/* 2nd byte is bit depth */
	u_int8_t bufsize;		/* 3rd byte is data size */
	u_int8_t buf[VIBE_OUTPUT_SAMPLE_SIZE];
};

struct actuator_samples_buffer {
	int8_t playingbuf_idx;
	u_int8_t outputval_idx;
	/* Use 2 buffers to receive samples from user mode */
	struct samples_buffer samples_buf[2];
};

struct secvib_data {
	struct miscdevice miscdev;
	struct timed_output_dev dev;
	struct secvib_platform_data   *pdata;
	struct hrtimer timer;
	struct hrtimer haptic_timer;
	bool is_haptic_timer_started;
	bool stop_requested;
	int wtchdg_cnt;
	struct semaphore vib_sem;
	struct mutex lock;
	int	max_timeout;
	struct wake_lock wklock;
	bool enabled;
	unsigned long magic_no;
	char *dev_name;
	size_t dev_name_size;
	struct actuator_samples_buffer *actr_buf;
	/*
	 * TBD: This parameter may not be needed, if we
	 * correct logic in secvib_write
	 */
	char *write_buf;
	struct workqueue_struct *vib_wq;
	struct work_struct vib_work;
};

static int secvib_vibrator_enable(struct secvib_data *secvib,
		int actr_idx, int on)
{

	int ret;

	if (on) {
		ret = secvib->pdata->vib_enable(actr_idx, on);
		secvib->enabled = true;
		wake_lock(&secvib->wklock);
	} else {

		if (!secvib->enabled)
			return 0;
		ret = secvib->pdata->vib_enable(actr_idx, on);
		secvib->enabled = false;
		wake_unlock(&secvib->wklock);

	}
	return ret;
}

static void secvib_timer_worker(struct work_struct *work)
{
	struct secvib_data *secvib =
		container_of(work, struct secvib_data, vib_work);

	secvib_vibrator_enable(secvib, 0, 0);
}

static enum hrtimer_restart secvib_timer_func(struct hrtimer *timer)
{
	struct secvib_data *secvib =
		container_of(timer, struct secvib_data, timer);

	queue_work(secvib->vib_wq, &secvib->vib_work);
	return HRTIMER_NORESTART;
}

static int secvib_get_time(struct timed_output_dev *dev)
{
	struct secvib_data	*secvib =
		container_of(dev, struct secvib_data, dev);

	if (hrtimer_active(&secvib->timer)) {
		ktime_t r = hrtimer_get_remaining(&secvib->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void secvib_enable(struct timed_output_dev *dev, int value)
{
	struct secvib_data	*secvib =
		container_of(dev, struct secvib_data, dev);

	pr_debug("secvib: vibrator force:%d\n", value);
	mutex_lock(&secvib->lock);

	/* cancel previous timer and set GPIO according to value */
	hrtimer_cancel(&secvib->timer);

	if (value) {
		secvib_vibrator_enable(secvib, 0, 1);

		if (value > 0) {
			if (value > secvib->max_timeout)
				value = secvib->max_timeout;

			hrtimer_start(&secvib->timer,
					ktime_set(value / 1000,
					(value % 1000) * 1000000),
					HRTIMER_MODE_REL);
		}

	} else
		secvib_vibrator_enable(secvib, 0, 0);
	mutex_unlock(&secvib->lock);
}

static int secvib_get_dev_name(u_int8_t actr_idx,
		char *devname, int size)
{
	    return 0;
}

static inline int secvib_is_vib_sem(struct semaphore *lock)
{

	return (lock->count) != 1;
}

static enum hrtimer_restart secvib_haptic_timer_func(struct hrtimer *timer)
{
	struct secvib_data *secvib =
		container_of(timer, struct secvib_data, haptic_timer);

	/* Scheduling next timeout value right away */
	hrtimer_forward_now(&secvib->haptic_timer, ktime_set(0, 5000000));

	if ((secvib->is_haptic_timer_started) &&
			secvib_is_vib_sem(&secvib->vib_sem))
		up(&secvib->vib_sem);

	return HRTIMER_RESTART;
}

static void secvib_stop_haptic_timer(struct secvib_data *secvib)
{

	int i;

	if (secvib->is_haptic_timer_started) {
		secvib->is_haptic_timer_started = false;
		hrtimer_cancel(&secvib->haptic_timer);
	}

	/* Reset samples buffers */
	for (i = 0; i < secvib->pdata->num_actuators; i++) {
		secvib->actr_buf[i].playingbuf_idx = -1;
		secvib->actr_buf[i].samples_buf[0].bufsize = 0;
		secvib->actr_buf[i].samples_buf[1].bufsize = 0;
	}

	secvib->stop_requested = false;
	secvib->enabled = false;

	return;
}

static int secvib_forceout_set_samples(struct secvib_data *secvib,
		u_int8_t actr_index,
		u_int16_t bit_depth,
		u_int16_t bufsize,
		int8_t *buf)
{
	int8_t vib_force;
	static int8_t pre_vib_force;

	switch (bit_depth) {
	case 8:
		/* buf is expected to contain 1 byte */
		if (bufsize != 1) {
			pr_err("secvib: wrong  bufsize =  %d\n", bufsize);
			goto error_vibe;
		}
		vib_force = buf[0];
		break;

	case 16:
		/* buf is expected to contain 2 byte */
		if (bufsize != 2)
			goto error_vibe;

		/* Map 16-bit value to 8-bit */
		vib_force = ((int16_t *)buf)[0] >> 8;
		break;

	default:
		/* Unexpected bit depth */
		goto error_vibe;
	}

	if (vib_force == 0)
		/* Set 50% duty cycle or disable amp */
		secvib_vibrator_enable(secvib, actr_index, 0);
	else if (pre_vib_force != vib_force) {
		secvib->pdata->pwm_set(actr_index, vib_force);
		secvib_vibrator_enable(secvib, actr_index, 1);
	}

	pre_vib_force = vib_force;

	return 0;

error_vibe:
	return -1;

}

static int secvib_process_vib_data(struct secvib_data *secvib)
{
	int ret, i;
	int actr_not_playing = 0;

	for (i = 0; i < secvib->pdata->num_actuators; i++) {

		struct actuator_samples_buffer *tmp = &(secvib->actr_buf[i]);

		if (tmp->playingbuf_idx == -1) {

			actr_not_playing++;

			if ((secvib->pdata->num_actuators == actr_not_playing)
				&& ((++secvib->wtchdg_cnt)
					> WATCHDOG_TIMEOUT)) {

				int8_t zero[1] = {0};

				/*
				 * Nothing to play for all actuators, turn off
				 * the timer when we reach the watchdog tick
				 * count limit
				 */
				secvib_forceout_set_samples(secvib,
						i, 8, 1, zero);
				secvib_vibrator_enable(secvib, i, 0);
				secvib_stop_haptic_timer(secvib);

				/* Reset watchdog counter */
				secvib->wtchdg_cnt = 0;
			}
		} else {
			int8_t idx = (int)tmp->playingbuf_idx;

			/* Play the current buffer */
			ret = secvib_forceout_set_samples(
					secvib,
					tmp->samples_buf[idx].actr_index,
					tmp->samples_buf[idx].bit_depth,
					tmp->samples_buf[idx].bufsize,
					tmp->samples_buf[idx].buf);
			if (unlikely(ret < 0))
				hrtimer_forward_now(&secvib->haptic_timer,
						ktime_set(0, 5000000));

			tmp->outputval_idx += tmp->samples_buf[idx].bufsize;

			if (tmp->outputval_idx
					>= tmp->samples_buf[idx].bufsize) {
				/* Reach the end of the current buffer */
				tmp->samples_buf[idx].bufsize = 0;

				/* Switch buffer */
				tmp->playingbuf_idx ^= 1;
				tmp->outputval_idx = 0;

				/* Finished playing, disable amp for actuator */
				if (secvib->stop_requested) {
					tmp->playingbuf_idx = -1;
					secvib_vibrator_enable(secvib, i, 0);
				}
			}
		}
	}

	/* If finished playing, stop timer */
	if (secvib->stop_requested) {
		secvib_stop_haptic_timer(secvib);

		/* Reset watchdog counter */
		secvib->wtchdg_cnt = 0;

		if (secvib_is_vib_sem(&secvib->vib_sem))
			up(&secvib->vib_sem);
		/* tell the caller this is the last iteration */
		return 1;
	}

	return 0;

}

static void secvib_start_haptic_timer(struct secvib_data *secvib)
{

	int i, ret;
	if (!secvib->is_haptic_timer_started) {

		if (!secvib_is_vib_sem(&secvib->vib_sem)) {
			ret = down_interruptible(&secvib->vib_sem);
			if (unlikely(ret != 0))
				pr_info("secvib: down_interruptible interrupted by a signal.\n");
		}

		secvib->is_haptic_timer_started = true;

		/* Start the timer */
		hrtimer_start(&secvib->haptic_timer,
				ktime_set(0, 5000000),
				HRTIMER_MODE_REL);

		/*
		 * TBD: Below for loop may not be required
		 * Not sure, need to check
		 */

		/*
		 * Don't block the write() function after the first sample to
		 * allow the host sending the next samples with no delay
		 */
		for (i = 0; i < secvib->pdata->num_actuators; i++) {

			if ((secvib->actr_buf[i].samples_buf[0].bufsize)
				|| (secvib->actr_buf[i]
					.samples_buf[1].bufsize)) {
				secvib->actr_buf[i].outputval_idx = 0;
				return;
			}
		}
	}

	ret = secvib_process_vib_data(secvib);
	if (ret != 0)
		return;

	/*
	 * Use interruptible version of down to be safe
	 * (try to not being stuck here if the mutex
	 * is not freed for any reason)
	 * wait for the mutex to be freed by the timer
	 */
	ret = down_interruptible(&secvib->vib_sem);
	if (unlikely(ret != 0))
		pr_info("secvib: down_interruptible interrupted by a signal.\n");

	return;
}

static void secvib_terminate_haptic_timer(struct secvib_data *secvib)
{

	secvib_stop_haptic_timer(secvib);
	if (secvib_is_vib_sem(&secvib->vib_sem))
		up(&secvib->vib_sem);

	return;
}

static ssize_t secvib_read(struct file *file,
		char __user *buf, size_t count, loff_t *ppos)
{

	int ret;
	struct secvib_data *secvib =
		container_of(file->private_data, struct secvib_data, miscdev);
	const size_t bufsize = (secvib->dev_name_size > (size_t)(*ppos))
		? min(count, secvib->dev_name_size - (size_t)(*ppos)) : 0;

	pr_debug("secvib: %s[%d]\n", __func__, __LINE__);

	/* End of buffer, exit */
	if (unlikely(bufsize == 0))
		return 0;

	mutex_lock(&secvib->lock);
	ret = copy_to_user(buf, secvib->dev_name + (*ppos), bufsize);
	if (unlikely(ret != 0)) {
		pr_err("secvib: read failed\n");
		mutex_unlock(&secvib->lock);
		return 0;
	}

	/* Update file position and return copied buffer size */
	*ppos += bufsize;

	mutex_unlock(&secvib->lock);

	return bufsize;

}

static ssize_t secvib_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{

	struct secvib_data *secvib =
		container_of(file->private_data, struct secvib_data, miscdev);
	int i = 0;
	int free_buf_idx;   /* initialized below */

	pr_debug("secvib: %s[%d]\n", __func__, __LINE__);
	*ppos = 0;  /* file position not used, always set to 0 */

	mutex_lock(&secvib->lock);
	/*
	 * prevent unauthorized caller to write data.
	 * touchsense service is the only valid caller.
	 */
	if (secvib->magic_no != TSPDRV_MAGIC_NUMBER) {
		pr_err("secvib: unauthorized write.\n");
		goto out;
	}

	/* copy immediately the input buffer */
	if (0 != copy_from_user(secvib->write_buf, buf, count)) {
		/* failed to copy all the data, exit */
		pr_err("secvib: write failed to copy all the data\n");
		goto out;
	}

	/* Check buffer size */
	if ((count <= SPI_HEADER_SIZE)
			|| (count > (SPI_BUFFER_SIZE
					* secvib->pdata->num_actuators))) {
		pr_err("secvib: invalid write buffer size.\n");
		goto out;
	}

	while (i < count) {

		struct samples_buffer *temp_buf =
			(struct samples_buffer *)(&secvib->write_buf[i]);

		/*
		 * Index is about to go beyond the buffer size.
		 * (Should never happen).
		 */
		if ((i + SPI_HEADER_SIZE) >= count)
			pr_err("secvib: invalid buffer index.\n");

		if (temp_buf->bit_depth != 8)
			pr_warn("secvib: invalid bit depth. Use default value (8).\n");

		/*
		 * Index is about to go beyond the buffer size.
		 * (Should never happen).
		 */
		if ((i + SPI_HEADER_SIZE + temp_buf->bufsize) > count)
			pr_warn("secvib: invalid data size.\n");

		if (secvib->pdata->num_actuators <= temp_buf->actr_index) {
			pr_err("secvib: invalid actuator index.\n");
			i += (SPI_HEADER_SIZE + temp_buf->bufsize);
			continue;
		}

		if (secvib->actr_buf[temp_buf->actr_index]
				.samples_buf[0].bufsize == 0)
			free_buf_idx = 0;
		else if (secvib->actr_buf[temp_buf->actr_index]
				.samples_buf[1].bufsize == 0)   {
			free_buf_idx = 1;
		} else {
			/* No room to store new samples  */
			pr_err("secvib: no room to store new samples.\n");
			goto out;
		}

		/* Store the data in the free buffer of the given actuator */
		memcpy(
			&(secvib->actr_buf[temp_buf->actr_index]
				.samples_buf[free_buf_idx]),
			&secvib->write_buf[i],
			(SPI_HEADER_SIZE + temp_buf->bufsize));

		if (secvib->actr_buf[temp_buf->actr_index]
				.playingbuf_idx == -1) {
			secvib->actr_buf[temp_buf->actr_index]
				.playingbuf_idx = free_buf_idx;
			secvib->actr_buf[temp_buf->actr_index]
				.outputval_idx = 0;
		}

		/* Increment buffer index */
		i += (SPI_HEADER_SIZE + temp_buf->bufsize);
	}

	/* Start the timer after receiving new output force */
	secvib_start_haptic_timer(secvib);
	secvib->enabled = true;

	mutex_unlock(&secvib->lock);
	return 0;

out:
	mutex_unlock(&secvib->lock);
	return -EINVAL;


}

/* ioctl - I/O control */
static long secvib_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{

	int ret;
	struct secvib_data *secvib =
		container_of(file->private_data, struct secvib_data, miscdev);

	pr_debug("secvib: %s[%d]\n", __func__, __LINE__);
	mutex_lock(&secvib->lock);

	switch (cmd) {
	case TSPDRV_STOP_KERNEL_TIMER:
		/*
		 * As we send one sample ahead of time, we need
		 *  to finish playing the last sample
		 *  before stopping the timer. So we just set a flag here.
		 */
		if (secvib->enabled == true)
			secvib->stop_requested = true;

		/* Last data processing to disable amp and stop timer */
		ret = secvib_process_vib_data(secvib);

		break;

	case TSPDRV_MAGIC_NUMBER:
		secvib->magic_no = TSPDRV_MAGIC_NUMBER;
		ret = 0;
		break;

	case TSPDRV_ENABLE_AMP:
		ret = secvib_vibrator_enable(secvib, arg, 1);
		pr_debug("secvib: vibrator enabled\n");
		break;

	case TSPDRV_DISABLE_AMP:
		/*
		 * Small fix for now to handle proper combination
		 * of TSPDRV_STOP_KERNEL_TIMER and
		 * TSPDRV_DISABLE_AMP together
		 * If a stop was requested, ignore the request as
		 * the amp will be disabled by the timer
		 * proc when it's ready
		 */
		if (!secvib->stop_requested)
			ret = secvib_vibrator_enable(secvib, arg, 0);
		else
			ret = 0;
		break;

	case TSPDRV_GET_NUM_ACTUATORS:
		ret = secvib->pdata->num_actuators;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&secvib->lock);

	return ret;
}

static int secvib_open(struct inode *inode, struct file *file)
{

	struct secvib_data *secvib =
		container_of(file->private_data, struct secvib_data, miscdev);

	pr_info("secvib: %s[%d]opening %s\n",
			__func__, __LINE__, secvib->miscdev.name);
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	return 0;
}

static int secvib_release(struct inode *inode, struct file *file)
{

	struct secvib_data *secvib =
		container_of(file->private_data, struct secvib_data, miscdev);

	pr_info("secvib: %s[%d] releasing %s\n",
			__func__, __LINE__, secvib->miscdev.name);

	secvib_terminate_haptic_timer(secvib);

	module_put(THIS_MODULE);

	return 0;

}

static const struct file_operations secvib_fops = {
	.owner =    THIS_MODULE,
	.read =     secvib_read,
	.write =    secvib_write,
	.unlocked_ioctl =    secvib_ioctl,
	.open =     secvib_open,
	.release =  secvib_release,
	.llseek =   default_llseek
};

static int secvib_probe(struct platform_device *pdev)
{
	struct secvib_platform_data *pdata = pdev->dev.platform_data;
	struct secvib_data *secvib;
	int ret, i;


	if (!pdata)
		return -EBUSY;

	secvib = kzalloc(sizeof(struct secvib_data), GFP_KERNEL);
	if (!secvib)
		return -ENOMEM;

	hrtimer_init(&secvib->timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	secvib->timer.function = secvib_timer_func;

	hrtimer_init(&secvib->haptic_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	secvib->haptic_timer.function = secvib_haptic_timer_func;
	secvib->is_haptic_timer_started = false;

	mutex_init(&secvib->lock);
	sema_init(&secvib->vib_sem, 1);
	wake_lock_init(&secvib->wklock, WAKE_LOCK_SUSPEND, "vibrator");

	secvib->max_timeout = MAX_TIMEOUT;
	secvib->pdata = pdata;

	if (pdata->pwm_init)
		pdata->pwm_init();

	secvib->dev.name = "vibrator";
	secvib->dev.get_time = secvib_get_time;
	secvib->dev.enable = secvib_enable;
	ret = timed_output_dev_register(&secvib->dev);
	if (unlikely(ret < 0))
		goto err_to_dev_reg;

	secvib->miscdev.minor = MISC_DYNAMIC_MINOR;
	secvib->miscdev.name = VIB_DEVNAME;
	secvib->miscdev.fops = &secvib_fops;
	ret = misc_register(&secvib->miscdev);
	if (unlikely(ret < 0))
		goto err_to_dev_reg;

	secvib->dev_name = kzalloc(sizeof(char) *
			((VIBE_MAX_DEVICE_NAME_LENGTH
			 + VERSION_STR_LEN)
			* pdata->num_actuators), GFP_KERNEL);
	if (!secvib->dev_name) {
		ret = -ENOMEM;
		goto err_to_dev_reg;
	}

	secvib->actr_buf = kzalloc(sizeof(struct actuator_samples_buffer)
			* pdata->num_actuators, GFP_KERNEL);
	if (!secvib->actr_buf) {
		ret = -ENOMEM;
		goto err_to_dev_reg;
	}

	secvib->write_buf = kzalloc(sizeof(char)
			* (pdata->num_actuators
				* SPI_BUFFER_SIZE),
				GFP_KERNEL);
	if (!secvib->write_buf) {
		ret = -ENOMEM;
		goto err_to_dev_reg;
	}

	secvib->dev_name_size = 0;

    /* Get and concatenate device name and initialize data buffer */
	for (i = 0; i < pdata->num_actuators; i++) {
		char *tmp = secvib->dev_name + secvib->dev_name_size;
		secvib_get_dev_name(i, tmp, VIBE_MAX_DEVICE_NAME_LENGTH);

		/* Append version information and get buffer length */
		strcat(tmp, VERSION_STR);
		secvib->dev_name_size += strlen(tmp);

		secvib->actr_buf[i].playingbuf_idx = -1; /* Not playing */
		secvib->actr_buf[i].samples_buf[0].bufsize = 0;
		secvib->actr_buf[i].samples_buf[1].bufsize = 0;
	}

	secvib->vib_wq = create_workqueue("secvib");
	if (unlikely(!secvib->vib_wq)) {
		pr_err("secvib: failed to create workqueue\n");
		ret = -EINVAL;
		goto err_to_dev_reg;
	}
	INIT_WORK(&secvib->vib_work, secvib_timer_worker);

	platform_set_drvdata(pdev, secvib);
	return 0;

err_to_dev_reg:
	mutex_destroy(&secvib->lock);
	wake_lock_destroy(&secvib->wklock);
	return ret;
}

static int secvib_remove(struct platform_device *pdev)
{
	struct secvib_data *secvib = platform_get_drvdata(pdev);

	kfree(secvib);

	return 0;
}

static struct platform_driver secvib_driver = {
	.probe		= secvib_probe,
	.remove		= secvib_remove,
	.driver		= {
		.name		= VIB_DEVNAME,
		.owner		= THIS_MODULE,
	},
};

static int __init secvib_init(void)
{
	return platform_driver_register(&secvib_driver);
}

static void __exit secvib_exit(void)
{
	platform_driver_unregister(&secvib_driver);
}

late_initcall(secvib_init);
module_exit(secvib_exit);

MODULE_AUTHOR("Shankar Bandal <shankar.b@samsung.com>");
MODULE_DESCRIPTION("Haptic vibrator driver");
MODULE_LICENSE("GPL");

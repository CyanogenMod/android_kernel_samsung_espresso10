/*
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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#ifdef CONFIG_OMAP_PM
#include <plat/omap-pm.h>
static struct pm_qos_request_list pm_qos_dpll_handle;
#endif

#include "radio-si470x.h"
#include "radio-si470x-dev.h"
#define SI4709_DRIVER_NAME "fmradio"
#define RDS_TIMEOUT	1000

struct wake_lock fm_prevent_suspend_lock;

static int si470x_dev_powerup(struct si470x_device *radio)
{
	int ret;

	wake_lock_init(&fm_prevent_suspend_lock, WAKE_LOCK_SUSPEND,
			"fm_prevent_suspend");
	wake_lock(&fm_prevent_suspend_lock);

	/* Resetting the device */
	enable_irq(radio->si470x_irq);

	radio->pdata->reset_gpio_on(0);
	msleep(20);
	radio->pdata->reset_gpio_on(1);

	radio->registers[POWERCFG] = POWERCFG_DMUTE | POWERCFG_ENABLE;
	radio->registers[POWERCFG] &= ~POWERCFG_DISABLE;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
		goto err;
	} else {
		/*Si4709/09 datasheet: Table 7 */
		msleep(110);
	}

	radio->registers[POWERCFG] |= (0x01 << 11) & POWERCFG_RDSM;
	radio->registers[POWERCFG] &= ~POWERCFG_SKMODE;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
		goto err;
	}

	radio->registers[SYSCONFIG1] = (((0x01 << 11) & SYSCONFIG1_DE) |
				((0x01 << 14) & SYSCONFIG1_STCIEN) |
				((0x01 << 12) & SYSCONFIG1_RDS) |
				(0x01 << 2));
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDSIEN;
	ret = si470x_set_register(radio, SYSCONFIG1);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconfig1 reg\n", __func__);
		ret = -1;
		goto err;
	}

	radio->registers[SYSCONFIG2] |= ((0x01 << 4) & SYSCONFIG2_SPACE_100KHZ)
					| (0x0F & SYSCONFIG2_VOLUME)
					| (0x9 << 8);
	ret = si470x_set_register(radio, SYSCONFIG2);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconfig2 reg\n", __func__);
		ret = -1;
		goto err;
	}

	radio->registers[SYSCONFIG3] |= ((0x04 << 4) & SYSCONFIG3_SKSNR_MIN4)
					|0x04;

	ret = si470x_set_register(radio, SYSCONFIG3);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf3 reg\n", __func__);
		ret = -1;
		goto err;
	}
	return 0;
err:
	wake_unlock(&fm_prevent_suspend_lock);
	wake_lock_destroy(&fm_prevent_suspend_lock);
	return ret;
}

static int si470x_dev_powerdown(struct si470x_device *radio)
{
	int ret;

	msleep(500);		/* To avoid turn off pop noise */

	radio->registers[SYSCONFIG1]  &= ~SYSCONFIG1_RDS;
	radio->registers[SYSCONFIG1]  |= (0x01 << 3) & SYSCONFIG1_GPO_LOW;
	ret = si470x_set_register(radio, SYSCONFIG1);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf1 reg\n", __func__);
		ret = -1;
		goto err;
	}

	radio->registers[POWERCFG] |= (POWERCFG_DISABLE | POWERCFG_ENABLE);
	radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
		goto err;
	}

	/* Resetting the device */
	radio->pdata->reset_gpio_on(0);
	radio->pdata->reset_gpio_on(1);
	radio->pdata->reset_gpio_on(0);

	disable_irq(radio->si470x_irq);

	wake_unlock(&fm_prevent_suspend_lock);
	wake_lock_destroy(&fm_prevent_suspend_lock);

	return 0;

err:
	wake_unlock(&fm_prevent_suspend_lock);
	wake_lock_destroy(&fm_prevent_suspend_lock);
	return ret;
}

static int si470x_dev_band_set(struct si470x_device *radio, int band)
{
	int ret;

	switch (band) {
	case BAND_87500_108000_kHz:
		radio->registers[SYSCONFIG2] &= ~SYSCONFIG2_BAND_875MHZ;
		break;
	case BAND_76000_108000_kHz:
		radio->registers[SYSCONFIG2] |= SYSCONFIG2_BAND_76MHZ;
		break;
	case BAND_76000_90000_kHz:
		radio->registers[SYSCONFIG2] |= SYSCONFIG2_BAND;
		break;
	default:
		ret = -1;
		goto err;
	}

	ret = si470x_set_register(radio, SYSCONFIG2);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf2 reg\n", __func__);
		ret = -1;
		goto err;
	}
	return 0;

err:
	return ret;
}

static int si470x_dev_ch_spacing_set(struct si470x_device *radio,
				     int ch_spacing)
{
	int ret;

	switch (ch_spacing) {
	case CHAN_SPACING_200_kHz:
		radio->registers[SYSCONFIG2] &= ~SYSCONFIG2_SPACE;
		break;
	case CHAN_SPACING_100_kHz:
		radio->registers[SYSCONFIG2] |= SYSCONFIG2_SPACE_100KHZ;
		break;
	case CHAN_SPACING_50_kHz:
		radio->registers[SYSCONFIG2] |= SYSCONFIG2_SPACE_50KHZ;
		break;
	default:
		ret = -1;
		goto err;
	}

	ret = si470x_set_register(radio, SYSCONFIG2);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf2 reg\n", __func__);
		ret = -1;
		goto err;
	}
	return 0;

err:
	return ret;
}

static int si470x_dev_chan_select(struct si470x_device *radio, u32 frequency)
{

	int ret;
	ret = si470x_set_freq(radio, frequency);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting the freq\n", __func__);
		ret = -1;
	}
	return ret;
}
static int si470x_dev_rssi_seek_th_set(struct si470x_device *radio, u8 seek_th)
{
	int ret;

	radio->registers[SYSCONFIG2] &= SYSCONFIG2_BAND_SPA_VOL;
	radio->registers[SYSCONFIG2] |= (seek_th << 8) & SYSCONFIG2_SEEKTH;
	ret = si470x_set_register(radio, SYSCONFIG2);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf2 reg\n", __func__);
		ret = -1;
	}

	return ret;
}

static int si470x_dev_seek_snr_th_set(struct si470x_device *radio, u8 seek_snr)
{
	int ret;

	radio->registers[SYSCONFIG3] &= SYSCONFIG3_SKNR_CLR;
	radio->registers[SYSCONFIG3] |= (seek_snr << 4) & SYSCONFIG3_SKSNR;
	ret = si470x_set_register(radio, SYSCONFIG3);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf2 reg\n", __func__);
		ret = -1;
	}
	return ret;
}

static int si470x_dev_seek_fm_id_th_set(struct si470x_device *radio,
					u8 seek_fm_id_th)
{
	int ret;

	radio->registers[SYSCONFIG3] &= SYSCONFIG3_SKCNT_CLR;
	radio->registers[SYSCONFIG3] |= seek_fm_id_th & SYSCONFIG3_SKCNT;
	ret = si470x_set_register(radio, SYSCONFIG3);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf2 reg\n", __func__);
		ret = -1;
	}
	return ret;
}

static int si470x_dev_de_set(struct si470x_device *radio, u8 de_tc)
{
	int ret;

	switch (de_tc) {
	case DE_TIME_CONSTANT_50:
		radio->registers[SYSCONFIG1] |= SYSCONFIG1_DE;
		break;
	case DE_TIME_CONSTANT_75:
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_DE;
		break;
	default:
		ret = -1;
		goto err;
	}

	ret = si470x_set_register(radio, SYSCONFIG1);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf1 reg\n", __func__);
		ret = -1;
		goto err;
	}
	return 0;

err:
	return ret;
}

static int si470x_dev_volext_enb(struct si470x_device *radio)
{
	int ret;

	radio->registers[SYSCONFIG3] |= SYSCONFIG3_VOLEXT_EN;
	ret = si470x_set_register(radio, SYSCONFIG3);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf3 reg\n", __func__);
		ret = -1;
	}

	return ret;
}

static int si470x_dev_volext_disb(struct si470x_device *radio)
{
	int ret;

	radio->registers[SYSCONFIG3] &= ~SYSCONFIG3_VOLEXT_EN;
	ret = si470x_set_register(radio, SYSCONFIG3);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf3 reg\n", __func__);
		ret = -1;
	}

	return ret;
}

static int si470x_dev_volume_set(struct si470x_device *radio, u8 volume)
{
	int ret;

	radio->registers[SYSCONFIG2] &= SYSCONFIG2_VOLUME_CLR;
	radio->registers[SYSCONFIG2] |= (volume & SYSCONFIG2_VOLUME);
	ret = si470x_set_register(radio, SYSCONFIG2);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconf2 reg\n", __func__);
		ret = -1;
	}

	return ret;
}

static int si470x_dev_volume_get(struct si470x_device *radio, u8 *volume)
{
	int ret;

	ret = si470x_get_register(radio, SYSCONFIG2);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while getting sysconfig2 reg\n", __func__);
		ret = -1;
	}
	if (radio->registers[SYSCONFIG2] & SYSCONFIG2_VOLUME)
		*volume = radio->registers[SYSCONFIG2] & SYSCONFIG2_VOLUME;

	return ret;
}

static int si470x_dev_dsmute_on(struct si470x_device *radio)
{
	int ret;

	radio->registers[POWERCFG] &= ~POWERCFG_DSMUTE;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
	}

	return ret;
}

static int si470x_dev_dsmute_off(struct si470x_device *radio)
{
	int ret;

	radio->registers[POWERCFG] |= POWERCFG_DSMUTE;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
	}

	return ret;
}

static int si470x_dev_mute_on(struct si470x_device *radio)
{
	int ret;

	radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
	}

	return ret;
}

static int si470x_dev_mute_off(struct si470x_device *radio)
{
	int ret;

	radio->registers[POWERCFG] |= POWERCFG_DMUTE;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
	}

	return ret;
}

static int si470x_dev_mono_set(struct si470x_device *radio)
{
	int ret;
	radio->registers[POWERCFG] |= POWERCFG_MONO;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
	}
	return ret;
}

static int si470x_dev_stereo_set(struct si470x_device *radio)
{
	int ret;
	radio->registers[POWERCFG] &= ~POWERCFG_MONO;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting powercfg reg\n", __func__);
		ret = -1;
	}
	return ret;
}

static int si470x_dev_rds_enable(struct si470x_device *radio)
{
	int ret;
	radio->registers[POWERCFG] |= (0x00 << 11) & POWERCFG_RDSM;
	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting RDS mode\n", __func__);
		ret = -1;
		goto err;
	}

	radio->registers[SYSCONFIG1] |= ((0x01 << 12) & SYSCONFIG1_RDS) |
				 ((0x01 << 15) & SYSCONFIG1_RDSIEN)
				| (0x01 << 2);
	ret = si470x_set_register(radio, SYSCONFIG1);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconfig1 reg\n", __func__);
		ret = -1;
		goto err;
	}

err:
	return ret;
}

static int si470x_dev_rds_disable(struct si470x_device *radio)
{
	int ret = 0;
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;
	ret = si470x_set_register(radio, SYSCONFIG1) ;
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting sysconfig1 reg\n", __func__);
		ret = -1;
	}
	return ret;
}

static int si470x_dev_get_freq(struct si470x_device *radio,
				      unsigned int *freq)
{
	return si470x_get_freq(radio, freq);
}

static int si470x_dev_set_seek(struct si470x_device *radio,
			       unsigned int wrap_around,
			       unsigned int seek_upward, u32 *frequency)
{
	int ret;
	unsigned long timeout;
	bool timed_out = 0;
	unsigned int seek_timeout = 5000;
#ifdef CONFIG_OMAP_PM
	static bool pm_qos_request_added;
#endif

#ifdef CONFIG_OMAP_PM
	if (!pm_qos_request_added) {
		pm_qos_request_added = true;

	pm_qos_add_request(&pm_qos_dpll_handle,
			PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	}
	pm_qos_update_request(&pm_qos_dpll_handle, 7);
#endif

	/* start seeking */
	radio->registers[POWERCFG] |= POWERCFG_SEEK;
	if (wrap_around == 1)
		radio->registers[POWERCFG] &= ~POWERCFG_SKMODE;
	else
		radio->registers[POWERCFG] |= POWERCFG_SKMODE;
	if (seek_upward == 1)
		radio->registers[POWERCFG] |= POWERCFG_SEEKUP;
	else
		radio->registers[POWERCFG] &= ~POWERCFG_SEEKUP;

	ret = si470x_set_register(radio, POWERCFG);
	if (unlikely(ret < 0))
		goto done;

	/* currently I2C driver only uses interrupt way to seek */
	if (radio->stci_enabled) {
		INIT_COMPLETION(radio->completion);
		/* wait till seek operation has completed */
		ret = wait_for_completion_timeout(&radio->completion,
				msecs_to_jiffies(seek_timeout));
		if (unlikely(!ret))
			timed_out = true;
	} else {
		/* wait till seek operation has completed */
		timeout = jiffies + msecs_to_jiffies(seek_timeout);
		do {
			ret = si470x_get_register(radio, STATUSRSSI);
			if (unlikely(ret < 0))
				goto stop;
			timed_out = time_after(jiffies, timeout);
		} while (((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
				&& (!timed_out));
	}

	ret = si470x_dev_get_freq(radio, frequency);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while getting freq\n", __func__);
		ret = -1;
	}

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
		pr_warn("seek doesnt complete\n");
	if (radio->registers[STATUSRSSI] & STATUSRSSI_SF)
		pr_warn("seek failed/ band limit reached\n");
	if (timed_out)
		pr_warn("seek timed out\n");

stop:
	/*stop seeking*/
	radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
	ret = si470x_set_register(radio, POWERCFG);
#ifdef CONFIG_OMAP_PM
	pm_qos_update_request(&pm_qos_dpll_handle, -1);
#endif
done:
	if ((ret == 0) && timed_out)
		ret = -EAGAIN;

	return ret;
}

static int si470x_dev_chan_get(struct si470x_device *radio,
			       unsigned int *frequency)
{
	int ret;
	ret = si470x_dev_get_freq(radio, frequency);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while getting the freq\n", __func__);
		ret = -1;
	}
	return ret;
}

static int si470x_dev_seek_up(struct si470x_device *radio, u32 *frequency)
{
	int ret;
	ret = si470x_dev_set_seek(radio, 1, 1, frequency);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting the seekup\n", __func__);
		ret = -1;
	}
	return ret;
}

static int si470x_dev_seek_down(struct si470x_device *radio, u32 *frequency)
{
	int ret;
	ret = si470x_dev_set_seek(radio, 1, 0, frequency);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while setting seek down\n", __func__);
		ret = -1;
	}
	return ret;
}

static int si470x_dev_rssi_get(struct si470x_device *radio, u32 *rssi)
{
	int ret;
	ret = si470x_get_register(radio, SYSCONFIG2);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while getting the sysconfig2\n", __func__);
		ret = -1;
	}
	if ((radio->registers[SYSCONFIG2] & SYSCONFIG2_SEEKTH) >> 8)
		*rssi = ((radio->registers[SYSCONFIG2] &
						SYSCONFIG2_SEEKTH) >> 8);
	return ret;
}

static int si470x_dev_sksnr_get(struct si470x_device *radio, u32 *sksnr)
{
	int ret;
	ret = si470x_get_register(radio, SYSCONFIG3);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while getting the sysconfig2\n", __func__);
		ret = -1;
	}
	if ((radio->registers[SYSCONFIG3] & SYSCONFIG3_SKSNR) >> 4)
		*sksnr = ((radio->registers[SYSCONFIG3] &
					SYSCONFIG3_SKSNR) >> 4);
	return ret;
}

static int si470x_dev_skcnt_get(struct si470x_device *radio, u32 *skcnt)
{
	int ret;
	ret = si470x_get_register(radio, SYSCONFIG3);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while getting the sysconfig2\n", __func__);
		ret = -1;
	}
	if (radio->registers[SYSCONFIG3] & SYSCONFIG3_SKCNT)
		*skcnt = radio->registers[SYSCONFIG3] & SYSCONFIG3_SKCNT;
	return ret;
}

static int si470x_dev_afcrl_get(struct si470x_device *radio, u8 *afcrl)
{
	int ret;
	ret = si470x_get_register(radio, STATUSRSSI);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while getting the statusrssi\n", __func__);
		ret = -1;
	}
	if (radio->registers[STATUSRSSI] & STATUSRSSI_AFCRL)
		*afcrl = 1;
	else
		*afcrl = 0;

	return ret;
}

static int si470x_dev_cur_rssi_get(struct si470x_device *radio, u32 *cur_rssi)
{
	int ret;
	ret = si470x_get_register(radio, STATUSRSSI);
	if (unlikely(ret < 0)) {
		pr_err("(%s):err while getting the statusrssi\n", __func__);
		ret = -1;
	}
	if (radio->registers[STATUSRSSI] & STATUSRSSI_RSSI)
		*cur_rssi = (radio->registers[STATUSRSSI] & STATUSRSSI_RSSI);
	return ret;
}

static int si470x_dev_rds_get(struct si470x_device *radio,
					struct radio_data *data)
{
	int i, ret = 0;
	mutex_lock(&radio->lock);
	while (radio->wr_index == radio->rd_index) {
		if (wait_event_interruptible_timeout(radio->read_queue,
						radio->wr_index !=
						radio->rd_index,
						msecs_to_jiffies(RDS_TIMEOUT)
						) <= 0) {
			ret = -EINTR;
			goto done;
		}
	}
	i = 0;
	data->rdsa = radio->rds_data_buff[i++ + 4 * radio->rd_index];
	data->rdsb = radio->rds_data_buff[i++ + 4 * radio->rd_index];
	data->rdsc = radio->rds_data_buff[i++ + 4 * radio->rd_index];
	data->rdsd = radio->rds_data_buff[i++ + 4 * radio->rd_index];

	memset(&radio->rds_data_buff[0 + 4 * radio->rd_index], 0, 8);
	radio->rd_index++;
	if (radio->rd_index >= RDS_BUF_LEN)
		radio->rd_index = 0;

done:
	mutex_unlock(&radio->lock);
	return ret;
}
/* file operations */
static int si470x_dev_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

static ssize_t si470x_dev_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *pos)
{
	int ret;
	unsigned int block_count;
	struct si470x_device *radio = container_of(filp->private_data,
						   struct si470x_device,
						   miscdev);

	/* switch on rds reception */
	mutex_lock(&radio->lock);
	if (!(radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS)) {
		mutex_unlock(&radio->lock);
		return -1;
	}

	/* block if no new data available */
	while (radio->wr_index == radio->rd_index) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EWOULDBLOCK;
			goto done;
		}
		if (wait_event_interruptible(radio->read_queue,
					     radio->wr_index !=
					     radio->rd_index) < 0) {
			ret = -EINTR;
			goto done;
		}
	}

	/* calculate block count from byte count */
	count /= 3;

	/* copy RDS block out of internal buffer and to user buffer */
	while (block_count < count) {
		if (radio->rd_index == radio->wr_index)
			break;

		/* always transfer rds complete blocks */
		if (copy_to_user(buf, &radio->buffer[radio->rd_index], 3))
			break;

		/* increment and wrap read pointer */
		radio->rd_index = 3;
		if (radio->rd_index >= radio->buf_size)
			radio->rd_index = 0;

		/* increment counters */
		block_count++;
		buf += 3;
		ret += 3;
	}

done:
	mutex_unlock(&radio->lock);
	return ret;
}

static unsigned int si470x_dev_poll(struct file *filp,
				    struct poll_table_struct *pts)
{
	struct si470x_device *radio = container_of(filp->private_data,
						   struct si470x_device,
						   miscdev);
	int retval;

	/* switch on rds reception */
	mutex_lock(&radio->lock);
	if (!(radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS)) {
		mutex_unlock(&radio->lock);
		return -1;
	}
	mutex_unlock(&radio->lock);

	poll_wait(filp, &radio->read_queue, pts);

	if (radio->rd_index != radio->wr_index)
		retval = POLLIN | POLLRDNORM;

	return retval;
}

static int si470x_dev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

#define m_si470x_dev_ioctl_operate(_radio, _ret, _fn)			\
do {									\
	_ret = _fn(_radio);						\
	if (unlikely(_ret < 0))						\
		pr_warn("(%s): operation (%s) failed\n",		\
			__func__, #_fn);				\
} while (0)

#define m_si470x_dev_ioctl_copy_from_user(_radio, _ret, _argp, _fn, _var)\
do {									\
	if (copy_from_user((void *)&_var, _argp, sizeof(_var))) {	\
		_ret = -EFAULT;						\
	} else {							\
		_ret = _fn(_radio, _var);				\
		if (unlikely(_ret < 0))					\
			pr_warn("(%s): operation (%s) failed\n",	\
				__func__, #_fn);			\
	}								\
} while (0)

#define m_si470x_dev_ioctl_copy_to_user(_radio,	_ret, _argp, _fn, _var)	\
do {									\
	_ret = _fn(_radio, &_var);					\
	if (unlikely(_ret < 0))						\
		pr_warn("(%s): operation (%s) failed\n",		\
			__func__, #_fn);				\
	else if (copy_to_user(_argp, (void *)&_var, sizeof(_var)))	\
		_ret = -EFAULT;						\
} while (0)

static long si470x_dev_ioctl(struct file *filp, unsigned int ioctl_cmd,
			     unsigned long arg)
{
	long ret;
	void __user *argp = (void __user *)arg;
	struct si470x_device *radio = container_of(filp->private_data,
						   struct si470x_device,
						   miscdev);
	s32 buf_s32;
	u32 buf_u32;
	u8 buf_u8;
	struct radio_data rds;

	if (_IOC_TYPE(ioctl_cmd) != SI470X_IOC_MAGIC) {
		pr_err("(%s): nappropriate ioctl 1 0x%x\n",
		       __func__, ioctl_cmd);
		return -ENOTTY;
	}

	if (_IOC_NR(ioctl_cmd) > SI470X_IOC_NR_MAX) {
		pr_err("(%s): nappropriate ioctl 2 0x%x\n",
		       __func__, ioctl_cmd);
		return -ENOTTY;
	}
	pr_debug("(%s): valid ioctl 0x%x\n", __func__, ioctl_cmd);
	switch (ioctl_cmd) {
	case SI470X_IOC_POWERUP:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_powerup);
		break;
	case SI470X_IOC_POWERDOWN:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_powerdown);
		break;
	case SI470X_IOC_BAND_SET:
		m_si470x_dev_ioctl_copy_from_user(radio, ret, argp,
						  si470x_dev_band_set, buf_s32);
		break;
	case SI470X_IOC_CHAN_SPACING_SET:
		m_si470x_dev_ioctl_copy_from_user(radio, ret, argp,
						  si470x_dev_ch_spacing_set,
						  buf_s32);
		break;
	case SI470X_IOC_CHAN_SELECT:
		m_si470x_dev_ioctl_copy_from_user(radio, ret, argp,
						  si470x_dev_chan_select,
						  buf_u32);
		break;
	case SI470X_IOC_CHAN_GET:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_chan_get, buf_u32);
		break;
	case SI470X_IOC_SEEK_UP:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_seek_up, buf_u32);
		break;
	case SI470X_IOC_SEEK_DOWN:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_seek_down,
						buf_u32);
		break;
	case SI470X_IOC_RSSI_SEEK_TH_SET:
		m_si470x_dev_ioctl_copy_from_user(radio, ret, argp,
						  si470x_dev_rssi_seek_th_set,
						  buf_u8);
		break;
	case SI470X_IOC_SEEK_SNR_SET:
		m_si470x_dev_ioctl_copy_from_user(radio, ret, argp,
						  si470x_dev_seek_snr_th_set,
						  buf_u8);
		break;
	case SI470X_IOC_SEEK_CNT_SET:
		m_si470x_dev_ioctl_copy_from_user(radio, ret, argp,
						  si470x_dev_seek_fm_id_th_set,
						  buf_u8);
		break;
	case SI470X_IOC_VOLEXT_ENB:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_volext_enb);
		break;
	case SI470X_IOC_VOLEXT_DISB:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_volext_disb);
		break;
	case SI470X_IOC_VOLUME_SET:
		m_si470x_dev_ioctl_copy_from_user(radio, ret, argp,
						  si470x_dev_volume_set,
						  buf_u8);
		break;
	case SI470X_IOC_VOLUME_GET:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_volume_get, buf_u8);
		break;
	case SI470X_IOC_DSMUTE_ON:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_dsmute_on);
		break;
	case SI470X_IOC_DSMUTE_OFF:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_dsmute_off);
		break;
	case SI470X_IOC_MUTE_ON:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_mute_on);
		break;
	case SI470X_IOC_MUTE_OFF:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_mute_off);
		break;
	case SI470X_IOC_MONO_SET:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_mono_set);
		break;
	case SI470X_IOC_STEREO_SET:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_stereo_set);
		break;
	case SI470X_IOC_RDS_ENABLE:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_rds_enable);
		break;
	case SI470X_IOC_RDS_DISABLE:
		m_si470x_dev_ioctl_operate(radio, ret, si470x_dev_rds_disable);
		break;
	case SI470X_IOC_RSSI_GET:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_rssi_get,
						buf_u32);
		break;
	case SI470X_IOC_SKSNR_GET:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_sksnr_get,
						buf_u32);
		break;
	case SI470X_IOC_SKCNT_GET:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_skcnt_get,
						buf_u32);
		break;
	case SI470X_IOC_AFCRL_GET:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_afcrl_get,
						buf_u8);
		break;
	case SI470X_IOC_STATUS_RSSI_GET:
		m_si470x_dev_ioctl_copy_to_user(radio, ret, argp,
						si470x_dev_cur_rssi_get,
						buf_u32);
		break;
	case SI470X_IOC_RDS_GET:
	{
		memset(&rds, 0, sizeof(struct radio_data));
		ret = si470x_dev_rds_get(radio, &rds);
		if (unlikely(ret < 0))
			pr_debug("(%s):operation (%s) failed\n",
				__func__, "si470x_dev_rds_get");
		if (copy_to_user(argp, (void *)&rds, sizeof(struct radio_data)))
			ret = -EFAULT;
	}
		break;
	case SI470X_IOC_DE_SET:
		ret = si470x_dev_de_set(radio, buf_u32);
		if (unlikely(ret))
			pr_debug("(%s): operation (%s) failed\n",
				 __func__, "si470x_dev_de_set");
		break;
	}

	return ret;
}

const struct file_operations si470x_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= si470x_dev_open,
	.read		= si470x_dev_read,
	.unlocked_ioctl	= si470x_dev_ioctl,
	.poll		= si470x_dev_poll,
	.release	= si470x_dev_release,
};

int si470x_dev_make_node(struct si470x_device *radio,
			  struct i2c_client *client)
{
	int ret;

	radio->miscdev.minor = MISC_DYNAMIC_MINOR;
	radio->miscdev.name = SI4709_DRIVER_NAME;
	radio->miscdev.fops = &si470x_dev_fops;
	radio->miscdev.parent = &client->dev;

	ret = misc_register(&radio->miscdev);
	if (unlikely(ret < 0)) {
		pr_err("(%s): misc register failed\n", __func__);
		return ret;
	}
	radio->pdata = client->dev.platform_data;
	radio->pdata->reset_gpio_on(0);
	msleep(20);
	radio->pdata->reset_gpio_on(1);
	radio->si470x_irq = client->irq;
	return 0;
}

int si470x_dev_rdsbuff_init(struct si470x_device *radio)
{
	radio->rds_data_buff = kzalloc(RDS_BUF_LEN * 8, GFP_KERNEL);
	if (!radio->rds_data_buff) {
		pr_err("(%s):Not sufficient memory\n", __func__);
		return -ENOMEM;
	}
	return 0;
}

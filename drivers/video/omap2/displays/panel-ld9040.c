/*
 * ld9040 AMOLED LCD panel driver.
 *
 * Author: Donghwa Lee  <dh09.lee@samsung.com>
 *
 * Derived from drivers/video/omap/lcd-apollon.c
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/serial_core.h>
#include <linux/platform_data/panel-ld9040.h>
#include <linux/platform_device.h>
#include <plat/hardware.h>
#include <video/omapdss.h>
#include <asm/mach-types.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <mach/omap4-common.h>

/* aka. default brightness 120 */
#define DEFAULT_GAMMA_LEVEL		10
#define MAX_GAMMA_LEVEL		25
#define GAMMA_TABLE_COUNT		21

#define SLEEPMSEC		0x1000
#define ENDDEF			0x2000
#define	DEFMASK			0xFF00
#define COMMAND_ONLY		0xFE
#define DATA_ONLY		0xFF

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS	120
#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)

#define DIM_BL 20
#define MIN_BL 30
#define MAX_BL 255
#define MAX_GAMMA_VALUE 24

static bool panel_ld9040_enabled;

static unsigned int get_lcdtype;
struct ld9040 *g_lcd; /* TBD need to decide right place - SHANKAR*/

module_param_named(get_lcdtype, get_lcdtype, uint, S_IRUGO);
MODULE_PARM_DESC(get_lcdtype, " get_lcdtype  in Bootloader");
struct ld9040 {
	struct device			*dev;
	struct spi_device		*spi;
	unsigned int			power;
	unsigned int			gamma_mode;
	unsigned int			current_gamma_mode;
	unsigned int			current_brightness;
	unsigned int			gamma_table_count;
	unsigned int			bl;
	unsigned int			beforepower;
	unsigned int			ldi_enable;
	unsigned int			acl_enable;
	unsigned int			cur_acl;
	struct mutex	lock;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct lcd_platform_data	*lcd_pd;
	struct early_suspend    early_suspend;
	struct class *lcd_class;
};

static int ld9040_spi_write_byte(struct ld9040 *lcd, int addr, int data)
{
	u16 buf[1];
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len		= 2,
		.tx_buf		= buf,
	};

	buf[0] = (addr << 8) | data;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	return spi_sync(lcd->spi, &msg);
}

static int ld9040_spi_write(struct ld9040 *lcd,
	unsigned char address, unsigned char command)
{
	int ret = 0;

	if (address != DATA_ONLY)
		ret = ld9040_spi_write_byte(lcd, 0x0, address);
	if (command != COMMAND_ONLY)
		ret = ld9040_spi_write_byte(lcd, 0x1, command);

	return ret;
}

static int ld9040_panel_send_sequence(struct ld9040 *lcd,
	const unsigned short *seq)
{
	int ret = 0, i = 0;
	const unsigned short *wbuf;

	mutex_lock(&lcd->lock);

	wbuf = seq;

	while ((wbuf[i] & DEFMASK) != ENDDEF) {
		if ((wbuf[i] & DEFMASK) != SLEEPMSEC) {
			ret = ld9040_spi_write(lcd, wbuf[i], wbuf[i+1]);
			if (ret)
				break;
		} else
			udelay(wbuf[i+1]*1000);
		i += 2;
	}

	mutex_unlock(&lcd->lock);

	return ret;
}

static int get_gamma_value_from_bl(int bl)
{
	int gamma_value = 0;
	int gamma_val_x10 = 0;

	if (bl >= MIN_BL) {
		gamma_val_x10 = 10 * (MAX_GAMMA_VALUE - 1)
			* bl/(MAX_BL-MIN_BL) + (10 - 10 * (MAX_GAMMA_VALUE - 1)
			* (MIN_BL)/(MAX_BL-MIN_BL));
		gamma_value = (gamma_val_x10 + 5)/10;
	} else
		gamma_value = 0;

	return gamma_value;
}

static int ld9040_gamma_ctl(struct ld9040 *lcd)
{
	int ret = 0;
	const unsigned short *gamma;
	struct ld9040_panel_data *pdata = lcd->lcd_pd->pdata;

	if (get_lcdtype == 1) { /* SM2 A2 */
		if (lcd->gamma_mode)
			gamma = pdata->gamma_sm2_a2_19_table[lcd->bl];
		else
			gamma = pdata->gamma_sm2_a2_22_table[lcd->bl];
	} else { /* SM2 A1 */
		if (lcd->gamma_mode)
			gamma = pdata->gamma_sm2_a1_19_table[lcd->bl];
		else
			gamma = pdata->gamma_sm2_a1_22_table[lcd->bl];
	}

	ret = ld9040_panel_send_sequence(lcd, gamma);
	if (ret) {
		dev_err(lcd->dev, "failed to disable gamma table updating.\n");
		goto gamma_err;
	}

	lcd->current_brightness = lcd->bl;
	lcd->current_gamma_mode = lcd->gamma_mode;
gamma_err:
	return ret;
}

static int ld9040_set_elvss(struct ld9040 *lcd)
{
	int ret = 0;
	struct ld9040_panel_data *pdata = lcd->lcd_pd->pdata;

	if (get_lcdtype) {  /* for SM2 A2*/
		switch (lcd->bl) {
		case 0 ... 5: /* 30cd ~ 100cd */
			ret = ld9040_panel_send_sequence(lcd,
					pdata->elvss_sm2_table[0]);
			break;
		case 6 ... 11: /* 110cd ~ 160cd */
			ret = ld9040_panel_send_sequence(lcd,
					pdata->elvss_sm2_table[1]);
			break;
		case 12 ... 15: /* 170cd ~ 200cd */
			ret = ld9040_panel_send_sequence(lcd,
					pdata->elvss_sm2_table[2]);
			break;
		case 16 ... 24: /* 210cd ~ 290cd (300cd) */
			ret = ld9040_panel_send_sequence(lcd,
					pdata->elvss_sm2_table[3]);
			break;
		default:
			break;
		}
	} else {	/* for SM2 A1*/
		switch (lcd->bl) {
		case 0 ... 6: /* 30cd ~ 100cd */
			ret = ld9040_panel_send_sequence(lcd,
					pdata->elvss_sm2_table[0]);
			break;
		case 7 ... 12: /* 110cd ~ 160cd */
			ret = ld9040_panel_send_sequence(lcd,
					pdata->elvss_sm2_table[1]);
			break;
		case 13 ... 16: /* 170cd ~ 200cd */
			ret = ld9040_panel_send_sequence(lcd,
					pdata->elvss_sm2_table[2]);
			break;
		case 17 ... 24: /* 210cd ~ 280cd (300cd) */
			ret = ld9040_panel_send_sequence(lcd,
					pdata->elvss_sm2_table[3]);
			break;
		default:
			break;
		}
	}
	dev_dbg(lcd->dev, "level  = %d\n", lcd->bl);

	if (ret) {
		dev_err(lcd->dev, "failed to initialize ldi.\n");
		return -EIO;
	}

	return ret;
}

static int ld9040_set_acl(struct ld9040 *lcd)
{
	int ret = 0;
	struct ld9040_panel_data *pdata = lcd->lcd_pd->pdata;

	if (lcd->acl_enable) {
		if (lcd->cur_acl == 0) {
			if (lcd->bl == 0 || lcd->bl == 1) {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_table[0]);
				dev_dbg(lcd->dev,
						"ACL_cutoff_set Percentage : off!!\n");
			} else {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_on);
			}
		}
		switch (lcd->bl) {
		case 0 ... 2: /* 30cd ~ 50cd */
			if (lcd->cur_acl != 0) {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_table[0]);
				dev_dbg(lcd->dev,
						"ACL_cutoff_set Percentage : off!!\n");
				lcd->cur_acl = 0;
			}
			break;
		case 3 ... 14: /* 70cd ~ 180cd */
			if (lcd->cur_acl != 40) {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_table[1]);
				dev_dbg(lcd->dev,
						"ACL_cutoff_set Percentage : 40!!\n");
				lcd->cur_acl = 40;
			}
			break;
		case 15: /* 190cd */
			if (lcd->cur_acl != 43) {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_table[2]);
				dev_dbg(lcd->dev,
						"ACL_cutoff_set Percentage : 43!!\n");
				lcd->cur_acl = 43;
			}
			break;
		case 16: /* 200cd */
			if (lcd->cur_acl != 45) {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_table[3]);
				dev_dbg(lcd->dev,
						"ACL_cutoff_set Percentage : 45!!\n");
				lcd->cur_acl = 45;
			}
			break;
		case 17: /* 210cd */
			if (lcd->cur_acl != 47) {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_table[4]);
				dev_dbg(lcd->dev,
						"ACL_cutoff_set Percentage : 47!!\n");
				lcd->cur_acl = 47;
			}
			break;
		case 18: /* 220cd */
			if (lcd->cur_acl != 48) {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_table[5]);
				dev_dbg(lcd->dev,
						"ACL_cutoff_set Percentage : 48!!\n");
				lcd->cur_acl = 48;
			}
			break;
		default:
			if (lcd->cur_acl != 50) {
				ret = ld9040_panel_send_sequence(lcd,
						pdata->acl_table[6]);
				dev_dbg(lcd->dev,
						"ACL_cutoff_set Percentage : 50!!\n");
				lcd->cur_acl = 50;
			}
			break;
		}
	} else {
			ret = ld9040_panel_send_sequence(lcd,
					pdata->acl_table[0]);
			lcd->cur_acl = 0;
			dev_dbg(lcd->dev,
					"ACL_cutoff_set Percentage : off!!\n");
	}

	if (ret) {
		dev_err(lcd->dev, "failed to initialize ldi.\n");
		return -EIO;
	}

	return ret;
}

static int ld9040_ldi_init(struct ld9040 *lcd)
{
	int ret, i;
	struct ld9040_panel_data *pdata = lcd->lcd_pd->pdata;
	if (get_lcdtype) {  /* for SM2 A2 */
		const unsigned short *init_seq[] = {
			pdata->seq_user_set,
			pdata->seq_displayctl_set,
			pdata->seq_gtcon_set,
			pdata->acl_on,
			pdata->seq_panelcondition_set,
			pdata->sleep_out,
			pdata->elvss_on,
			pdata->seq_sm2_a2_pwrctl_set,
			pdata->seq_gamma_set1,
			pdata->gamma_ctrl,
		};
		for (i = 0; i < ARRAY_SIZE(init_seq); i++) {
			ret = ld9040_panel_send_sequence(lcd, init_seq[i]);
			if (ret)
				break;
		}

	} else { /* for SM2 */
		const unsigned short *init_seq_sm2[] = {
			pdata->seq_user_set,
			pdata->seq_displayctl_set,
			pdata->seq_gtcon_set,
			pdata->acl_on,
			pdata->seq_panelcondition_set,
			pdata->sleep_out,
			pdata->elvss_on,
			pdata->seq_pwrctl_set,
			pdata->seq_sm2_gamma_set1,
			pdata->gamma_ctrl,
		};
		for (i = 0; i < ARRAY_SIZE(init_seq_sm2); i++) {
			ret = ld9040_panel_send_sequence(lcd, init_seq_sm2[i]);
			if (ret)
				break;
		}

	}
	return ret;
}

static int ld9040_ldi_enable(struct ld9040 *lcd)
{
	int ret = 0;
	struct ld9040_panel_data *pdata = lcd->lcd_pd->pdata;

	ret = ld9040_panel_send_sequence(lcd, pdata->display_on);

	return ret;
}

static int ld9040_ldi_disable(struct ld9040 *lcd)
{
	int ret;
	struct ld9040_panel_data *pdata = lcd->lcd_pd->pdata;

	ret = ld9040_panel_send_sequence(lcd, pdata->display_off);
	ret = ld9040_panel_send_sequence(lcd, pdata->sleep_in);

	return ret;
}

static int update_brightness(struct ld9040 *lcd)
{
	int ret;

	ret = ld9040_set_elvss(lcd);
	if (ret) {
		dev_err(lcd->dev, "lcd brightness setting failed.\n");
		return -EIO;
	}

	ret = ld9040_set_acl(lcd);
	if (ret) {
		dev_err(lcd->dev, "lcd brightness setting failed.\n");
		return -EIO;
	}

	ret = ld9040_gamma_ctl(lcd);
	if (ret) {
		dev_err(lcd->dev, "lcd brightness setting failed.\n");
		return -EIO;
	}

	return 0;
}


static int ld9040_power_on(struct ld9040 *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;
	pd = lcd->lcd_pd;

	if (!pd) {
		dev_err(lcd->dev, "platform data is NULL.\n");
		return -EFAULT;
	}

	if (!pd->power_on) {
		dev_err(lcd->dev, "power_on is NULL.\n");
		return -EFAULT;
	} else {
		pd->power_on(lcd->ld, 1);
		mdelay(pd->power_on_delay);
	}

	if (!pd->reset) {
		dev_err(lcd->dev, "reset is NULL.\n");
		return -EFAULT;
	} else {
		pd->reset(lcd->ld);
		mdelay(pd->reset_delay);
	}

	ret = ld9040_ldi_init(lcd);

	if (ret) {
		dev_err(lcd->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = ld9040_ldi_enable(lcd);
	if (ret) {
		dev_err(lcd->dev, "failed to enable ldi.\n");
		goto err;
	}

	update_brightness(lcd);

	lcd->ldi_enable = 1;

err:

	return ret;
}

static int ld9040_power_off(struct ld9040 *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;

	pd = lcd->lcd_pd;
	if (!pd) {
		dev_err(lcd->dev, "platform data is NULL.\n");
		return -EFAULT;
	}

	ret = ld9040_ldi_disable(lcd);
	if (ret) {
		dev_err(lcd->dev, "lcd setting failed.\n");
		ret = -EIO;
		goto err;
	}
	mdelay(pd->power_off_delay);

	if (!pd->power_on) {
		dev_err(lcd->dev, "power_on is NULL.\n");
		ret = -EFAULT;
		goto err;
	} else {
		pd->power_on(lcd->ld, 0);
		msleep(pd->power_off_delay);
	}

	lcd->ldi_enable = 0;

err:
	return ret;
}

static int ld9040_power(struct ld9040 *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = ld9040_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = ld9040_power_off(lcd);
	else
		dev_err(lcd->dev, "[LCD] %s => [%d, %d] called\n", __func__,
				power, lcd->power);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int ld9040_set_power(struct lcd_device *ld, int power)
{
	struct ld9040 *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return ld9040_power(lcd, power);
}

static int ld9040_get_power(struct lcd_device *ld)
{
	struct ld9040 *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int ld9040_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int ld9040_set_brightness(struct backlight_device *bd)
{
	int ret = 0, bl = bd->props.brightness;
	unsigned int temp_bl;
	struct ld9040 *lcd = bl_get_data(bd);

	if (bl < MIN_BRIGHTNESS ||
		bl > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS, bl);
		return -EINVAL;
	}

	temp_bl = get_gamma_value_from_bl(bl);
	lcd->bl = temp_bl;

	if ((lcd->ldi_enable) && (lcd->current_brightness != lcd->bl)) {
		ret = update_brightness(lcd);
		dev_info(lcd->dev, "lcd_id=%d, brightness=%d, bl=%d\n",
				get_lcdtype, bd->props.brightness, lcd->bl);
		if (ret < 0)
			dev_err(&bd->dev, "update brightness failed.\n");
	}

	return ret;
}

static const struct backlight_ops ld9040_backlight_ops  = {
	.get_brightness = ld9040_get_brightness,
	.update_status = ld9040_set_brightness,
};

#if 0
static ssize_t ld9040_sysfs_backlihgt_level_test(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct ld9040 *lcd = dev_get_drvdata(dev);
	unsigned long brightness;
	int rc;

	rc = strict_strtoul(buf, 0, &brightness);
	if (rc < 0)
		return rc;
	else
		lcd->bd->props.brightness = brightness;

	ld9040_set_brightness(lcd->bd);
	return 0;
}

static DEVICE_ATTR(baktst, 0666,
		NULL, ld9040_sysfs_backlihgt_level_test);
#endif
static ssize_t power_reduce_show(struct device *dev, struct
device_attribute *attr, char *buf)
{
	struct spi_device *spi_dev = to_spi_device(dev_get_drvdata(dev));
	struct ld9040 *lcd = dev_get_drvdata(&spi_dev->dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->acl_enable);
	strcpy(buf, temp);

	return strlen(buf);
}
static ssize_t power_reduce_store(struct device *dev, struct
device_attribute *attr, const char *buf, size_t size)
{
	struct spi_device *spi_dev = to_spi_device(dev_get_drvdata(dev));
	struct ld9040 *lcd = dev_get_drvdata(&spi_dev->dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;
	else{
		dev_info(dev, "power_reduce_store - %d, %d\n",
				lcd->acl_enable, value);
		if (lcd->acl_enable != value) {
			lcd->acl_enable = value;
			if (lcd->ldi_enable)
				ld9040_set_acl(lcd);
		}
		return size;
	}
}

static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP,
		power_reduce_show, power_reduce_store);

static ssize_t lcd_type_show(struct device *dev, struct
device_attribute *attr, char *buf)
{

	char temp[15];
	sprintf(temp, "SMD_AMS427G13\n");
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, S_IRUGO,
		lcd_type_show, NULL);
static ssize_t ld9040_sysfs_show_gamma_mode(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct spi_device *spi_dev = to_spi_device(dev_get_drvdata(dev));
	struct ld9040 *lcd = dev_get_drvdata(&spi_dev->dev);
	char temp[10];

	switch (lcd->gamma_mode) {
	case 0:
		sprintf(temp, "2.2 mode\n");
		strcat(buf, temp);
		break;
	case 1:
		sprintf(temp, "1.9 mode\n");
		strcat(buf, temp);
		break;
	default:
		dev_info(dev, "gamma mode could be 0:2.2, 1:1.9 or 2:1.7)n");
		break;
	}

	return strlen(buf);
}

static ssize_t ld9040_sysfs_store_gamma_mode(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct spi_device *spi_dev = to_spi_device(dev_get_drvdata(dev));
	struct ld9040 *lcd = dev_get_drvdata(&spi_dev->dev);
	int rc;

	rc = kstrtouint(buf, 0, &lcd->gamma_mode);

	if (rc < 0)
		return rc;

	if (lcd->gamma_mode > 1) {
		lcd->gamma_mode = 0;
		dev_err(dev, "there are only 2 types of gamma mode(0:2.2, 1:1.9)\n");
	} else
		dev_info(dev, "%s :: gamma_mode=%d\n", __func__,
				lcd->gamma_mode);

	if (lcd->ldi_enable) {
		if ((lcd->current_brightness == lcd->bl)
				&& (lcd->current_gamma_mode == lcd->gamma_mode))
			dev_dbg(dev, "there is no gamma_mode & brightness changed\n");
		else
			ld9040_gamma_ctl(lcd);
	}
	return len;
}

static DEVICE_ATTR(gamma_mode, S_IRUGO | S_IWUSR | S_IWGRP,
		ld9040_sysfs_show_gamma_mode, ld9040_sysfs_store_gamma_mode);

static ssize_t ld9040_sysfs_show_gamma_table(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct spi_device *spi_dev = to_spi_device(dev_get_drvdata(dev));
	struct ld9040 *lcd = dev_get_drvdata(&spi_dev->dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->gamma_table_count);
	strcpy(buf, temp);

	return strlen(buf);
}

static DEVICE_ATTR(gamma_table, S_IRUGO,
		ld9040_sysfs_show_gamma_table, NULL);


static ssize_t ld9040_sysfs_store_lcd_power(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	int rc;
	int lcd_enable;
	struct spi_device *spi_dev = to_spi_device(dev_get_drvdata(dev));
	struct ld9040 *lcd = dev_get_drvdata(&spi_dev->dev);

	dev_info(dev, "ld9040_sysfs_store_lcd_power\n");

	rc = kstrtoint(buf, 0, &lcd_enable);
	if (rc < 0)
		return rc;

	if (lcd_enable)
		ld9040_power(lcd, FB_BLANK_UNBLANK);
	else
		ld9040_power(lcd, FB_BLANK_POWERDOWN);

	return len;
}

static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, ld9040_sysfs_store_lcd_power);

#if 0
void ld9040_power_down(struct ld9040 *lcd)
{
	ld9040_power(lcd, FB_BLANK_POWERDOWN);
}

void ld9040_power_up(struct ld9040 *lcd)
{
	ld9040_power(lcd, FB_BLANK_UNBLANK);
}
#endif

static int ld9040_panel_probe(struct omap_dss_device *dssdev)
{
	dssdev->panel.config = OMAP_DSS_LCD_TFT
		| OMAP_DSS_LCD_IVS
		| OMAP_DSS_LCD_IEO
		| OMAP_DSS_LCD_IPC
		| OMAP_DSS_LCD_IHS
		| OMAP_DSS_LCD_ONOFF;

	if (NULL != g_lcd)
		dev_set_drvdata(&dssdev->dev, g_lcd);
	else {
		printk(KERN_ERR " %s [%d]ld9040_probe, probe failed\n",
				__func__, __LINE__);
		return -1;
	}
	return 0;
}

static void ld9040_panel_remove(struct omap_dss_device *dssdev)
{
    /* TBD */
}

bool display_panel_enabled_aftboot(void)
{
	return panel_ld9040_enabled;
}

static int ld9040_panel_enable(struct omap_dss_device *dssdev)
{
	struct ld9040 *lcd = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	spi_setup(lcd->spi);

	r = omapdss_dpi_display_enable(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to enable DPI\n");
		return r;
	}

	if (dssdev->platform_enable)
		r = dssdev->platform_enable(dssdev);

	if (!lcd->lcd_pd->lcd_enabled)
		ld9040_power(lcd, FB_BLANK_UNBLANK);

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	if (!panel_ld9040_enabled)
		panel_ld9040_enabled = true;


	return r;
}

static void ld9040_panel_disable(struct omap_dss_device *dssdev)
{
	struct ld9040 *lcd = dev_get_drvdata(&dssdev->dev);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;


	spi_setup(lcd->spi);

	ld9040_power(lcd, FB_BLANK_POWERDOWN);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	omapdss_dpi_display_disable(dssdev);

	lcd->lcd_pd->lcd_enabled = 0;
	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;

}

static int ld9040_panel_suspend(struct omap_dss_device *dssdev)
{


	ld9040_panel_disable(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	/* Shankar temp fix */
	/* dpll_cascading_blocker_release(lcd->dev); */

	return 0;
}

static int ld9040_panel_resume(struct omap_dss_device *dssdev)
{

	/* Shankar temp fix */
	/* dpll_cascading_blocker_hold(lcd->dev); */

	ld9040_panel_enable(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	return 0;
}

static void  ld9040_panel_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{

	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}

static void  ld9040_panel_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{

	dpi_set_timings(dssdev, timings);
}

static void  ld9040_panel_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int  ld9040_panel_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	return dpi_check_timings(dssdev, timings);
}



static struct omap_dss_driver ld9040_omap_dss_driver = {
	.probe          = ld9040_panel_probe,
	.remove         = ld9040_panel_remove,

	.enable         = ld9040_panel_enable,
	.disable        = ld9040_panel_disable,
	.get_resolution = ld9040_panel_get_resolution,
	.suspend        = ld9040_panel_suspend,
	.resume         = ld9040_panel_resume,

	.set_timings    = ld9040_panel_set_timings,
	.get_timings    = ld9040_panel_get_timings,
	.check_timings  = ld9040_panel_check_timings,

	.driver     = {
		.name   = "ld9040_panel",
		.owner  = THIS_MODULE,
	},
};

static __init int setup_current_panel(char *opt)
{
	get_lcdtype = (u32)memparse(opt, &opt);
	return 0;
}
__setup("lcd_panel_id=", setup_current_panel);

static int ld9040_probe(struct spi_device *spi)
{
	int ret = 0;
	struct ld9040 *lcd = NULL;
	struct ld9040_panel_data *pdata;

	lcd = kzalloc(sizeof(struct ld9040), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	mutex_init(&lcd->lock);

	/* ld9040 lcd panel uses 3-wire 9bits SPI Mode. */
	spi->bits_per_word = 9;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi setup failed.\n");
		goto out_free_lcd;
	}

	lcd->spi = spi;

	lcd->lcd_pd = (struct lcd_platform_data *)spi->dev.platform_data;
	if (!lcd->lcd_pd) {
		dev_err(&spi->dev, "platform data is NULL.\n");
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", &spi->dev,
		lcd, &ld9040_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		ret = PTR_ERR(lcd->bd);
		goto out_free_lcd;
	}

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = DEFAULT_GAMMA_LEVEL;
	lcd->current_brightness = lcd->bl;

	lcd->acl_enable = 0;
	lcd->cur_acl = 0;

	/*
	 * it gets gamma table count available so it gets user
	 * know that.
	 */
	pdata = lcd->lcd_pd->pdata;

	lcd->gamma_table_count =
	   pdata->gamma_table_size / (MAX_GAMMA_LEVEL * sizeof(int));

	lcd->lcd_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR(lcd->lcd_class))
		dev_err(lcd->dev, "Failed to create lcd_class!");

	lcd->dev = device_create(lcd->lcd_class, NULL, 0, NULL, "panel");
	if (IS_ERR(lcd->dev))
		dev_err(lcd->dev, "Failed to create device(panel)!\n");

	dev_set_drvdata(lcd->dev, &spi->dev);

	ret = device_create_file(lcd->dev, &dev_attr_gamma_mode);
	if (ret < 0)
		dev_err(lcd->dev,  "failed to add sysfs entries\n");

	ret = device_create_file(lcd->dev, &dev_attr_gamma_table);
	if (ret < 0)
		dev_err(lcd->dev,  "failed to add sysfs entries\n");
#if 0
	ret = device_create_file(&(spi->dev), &dev_attr_baktst);
	if (ret < 0)
		dev_err(&(spi->dev), "failed to add sysfs entries\n");
#endif
	ret = device_create_file(lcd->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(lcd->dev,  "failed to add sysfs entries\n");

	ret = device_create_file(lcd->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(lcd->dev,  "failed to add sysfs entries\n");

	ret = device_create_file(lcd->dev, &dev_attr_lcd_power);
	if (ret < 0)
		dev_err(lcd->dev,  "failed to add sysfs entries\n");


/* Do not turn off lcd during booting */
#ifndef CONFIG_FB_OMAP_BOOTLOADER_INIT
	lcd->lcd_pd->lcd_enabled = 0;
#else
	lcd->lcd_pd->lcd_enabled = 1;
#endif
	/*
	 * if lcd panel was on from bootloader like u-boot then
	 * do not lcd on.
	 */
	if (!lcd->lcd_pd->lcd_enabled) {
		/*
		 * if lcd panel was off from bootloader then
		 * current lcd status is powerdown and then
		 * it enables lcd panel.
		 */
		lcd->power = FB_BLANK_POWERDOWN;

		ld9040_power(lcd, FB_BLANK_UNBLANK);
	} else {
		lcd->lcd_pd->power_on(lcd->ld, 1);
		mdelay(lcd->lcd_pd->power_on_delay);
		lcd->power = FB_BLANK_UNBLANK;
		lcd->lcd_pd->lcd_enabled = 1;
		lcd->ldi_enable = 1;
	}


	dev_set_drvdata(&spi->dev, lcd);

	g_lcd = lcd;

	/* Register this driver with OMAP4 DSS subsystem */
	omap_dss_register_driver(&ld9040_omap_dss_driver);

	if (get_lcdtype == 1) { /* SM2 A2 */
		printk(KERN_INFO "[LD9040 PROBE LOG] LCDTYPE : SM2 A2\n");
	} else { /* SM2 A1 */
		printk(KERN_INFO "[LD9040 PROBE LOG] LCDTYPE : SM2\n");
	}

	dev_info(&spi->dev, "ld9040 panel driver has been probed.\n");
	return 0;

out_free_lcd:
	mutex_destroy(&lcd->lock);
	kfree(lcd);
err_alloc:
	return ret;
}

static int __devexit ld9040_remove(struct spi_device *spi)
{
	struct ld9040 *lcd = dev_get_drvdata(&spi->dev);

	ld9040_power(lcd, FB_BLANK_POWERDOWN);
	kfree(lcd);

	return 0;
}

/* Power down all displays on reboot, poweroff or halt. */
static void ld9040_shutdown(struct spi_device *spi)
{
	struct ld9040 *lcd = dev_get_drvdata(&spi->dev);
	printk(KERN_INFO " +++ ld9040_shutdown.\n");
	ld9040_power(lcd, FB_BLANK_POWERDOWN);
}

static struct spi_driver ld9040_driver = {
	.driver = {
		.name	= "ld9040",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= ld9040_probe,
	.remove		= __devexit_p(ld9040_remove),
	.shutdown	= ld9040_shutdown,
};

static int __init ld9040_init(void)
{
	return spi_register_driver(&ld9040_driver);
}

static void __exit ld9040_exit(void)
{
	spi_unregister_driver(&ld9040_driver);
}

module_init(ld9040_init);
module_exit(ld9040_exit);

MODULE_AUTHOR("Donghwa Lee <dh09.lee@samsung.com>");
MODULE_DESCRIPTION("ld9040 LCD Driver");
MODULE_LICENSE("GPL");

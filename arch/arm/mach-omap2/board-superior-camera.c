/*
 * Copyright (C) 2011 Google, Inc.
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

#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/leds-max77693.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "mux.h"
#include "control.h"
#include "omap_muxtbl.h"
#include "board-superior.h"

#define CAM_MAJOR	119
#define CAM_REG_MAX 1

static int power_enable;
static int cam_regulator_request(unsigned int ldo_enable);
static void cam_regulator_release(void);

static char *regulator_name[CAM_REG_MAX] = {
	"CAM_SENSOR_IO_1.8V",      /* ldon17 required for cam eeprom*/
};

struct cam_regulator_depot {
	struct regulator *reg_p;
	u32 orig_uv;
};

struct cam_regulator {
	u32 id;
	u32 min_uv;
	u32 max_uv;
};

static int torch_on;

struct max77693_led_platform_data max77693_led_pdata = {
	.num_leds = 4,

	.leds[0].name = "leds-sec1",
	.leds[0].id = MAX77693_FLASH_LED_1,
	.leds[0].timer = MAX77693_FLASH_TIME_500MS,
	.leds[0].timer_mode = MAX77693_TIMER_MODE_MAX_TIMER,
	.leds[0].cntrl_mode = MAX77693_LED_CTRL_BY_FLASHSTB,
	.leds[0].brightness = 0x1F,

	.leds[1].name = "leds-sec2",
	.leds[1].id = MAX77693_FLASH_LED_2,
	.leds[1].timer = MAX77693_FLASH_TIME_500MS,
	.leds[1].timer_mode = MAX77693_TIMER_MODE_MAX_TIMER,
	.leds[1].cntrl_mode = MAX77693_LED_CTRL_BY_FLASHSTB,
	.leds[1].brightness = 0x1F,

	.leds[2].name = "torch-sec1",
	.leds[2].id = MAX77693_TORCH_LED_1,
	.leds[2].cntrl_mode = MAX77693_LED_CTRL_BY_FLASHSTB,
	.leds[2].brightness = 0x3F,

	.leds[3].name = "torch-sec2",
	.leds[3].id = MAX77693_TORCH_LED_2,
	.leds[3].cntrl_mode = MAX77693_LED_CTRL_BY_I2C,
	.leds[3].brightness = 0x4F,
};

static ssize_t rear_camera_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char cam_type[] = "SONY_IMX175_NONE\n";

	return sprintf(buf, "%s", cam_type);
}

static ssize_t front_camera_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char cam_type[] = "SLSI_S5K6A3YX_NONE\n";

	return sprintf(buf, "%s", cam_type);
}

static struct device_attribute dev_attr_camtype_rear =
	__ATTR(rear_camtype, S_IRUGO, rear_camera_type_show, NULL);

static struct device_attribute dev_attr_camtype_front =
	__ATTR(front_camtype, S_IRUGO, front_camera_type_show, NULL);

#define VENDOR_POSITION 88
#define HW_REV_POSITION 92
static int firmware_check_from_calib(const char *filename,
		char *buf_forward, char *buf_backward)
{

	struct file *fp = NULL;
	char *Buffer;
	char hw_vendor[2];
	char hw_revision[3];
	int BytesRead;

	memset(hw_vendor, 0, 2);
	memset(hw_revision, 0, 3);
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(get_ds());

	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: failed open file : %s\n",
				__func__, filename);
		set_fs(old_fs);
		return -EPERM;
	}

	Buffer = kmalloc(128, GFP_KERNEL);
	if (!Buffer) {
		filp_close(fp, current->files);
		set_fs(old_fs);
		return -ENOMEM;
	}
	BytesRead = fp->f_op->read(fp, Buffer, 128, &fp->f_pos);

	hw_vendor[0] = *(Buffer + VENDOR_POSITION);
	memcpy(buf_forward, hw_vendor, 1);
	hw_revision[0] = (*(Buffer + HW_REV_POSITION) + 0x11);
	hw_revision[1] = (*(Buffer + HW_REV_POSITION + 1) + 0x11);

	memcpy(buf_backward, hw_revision, 2);

	kfree(Buffer);
	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;
}

static int firmware_check_from_ducati(const char *filename, char *buf)
{
	struct file *fp = NULL;
	char str[5];
	char *Buffer;
	long l;
	int BytesRead;
	int k, i;
	char pattern[8] = "FW_CHECK";

	mm_segment_t old_fs;

	memset(str, 0, 5);

	old_fs = get_fs();
	set_fs(get_ds());

	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: failed open file : %s\n",
				__func__, filename);
		set_fs(old_fs);
		return -EPERM;
	}

	l = fp->f_path.dentry->d_inode->i_size;

	Buffer = kmalloc(1024, GFP_KERNEL);
	if (!Buffer) {
		filp_close(fp, current->files);
		set_fs(old_fs);
		return -ENOMEM;
	}
	BytesRead = fp->f_op->read(fp, Buffer, 1024, &fp->f_pos);

	for (k = 0; k < 1024; k++) {
		if (*(Buffer + k) == pattern[0]) {
			for (i = 1; i < 8; i++) {
				if (*(Buffer + k + i) == pattern[i]) {
					continue;
				} else {
					k = k + i;
					break;
				}
			}
			/* Find pattern */
			k = k + i;
			break;
		}
	}
	if (k != 1024) {
		str[0] = *(Buffer + k + 1);
		str[1] = *(Buffer + k + 2);
		str[2] = *(Buffer + k + 3);
		str[3] = *(Buffer + k + 4);
	} else {
		memcpy(str, "FA01", 4);
	}

	kfree(Buffer);
	filp_close(fp, current->files);
	set_fs(old_fs);

	memcpy(buf, str, 4);

	return 0;
}

static ssize_t rear_camera_fw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char firmware_rear[5];
	char firmware_vendor[2];
	char firmware_revision[3];
	char fixed_str[] = "08T00";
	char fw_version[13];
	char calib_path[] = "/data/misc/camera/calib.bin";
	char calib_path_alt[120];
	memset(firmware_rear, 0, 5);
	memset(firmware_vendor, 0, 2);
	memset(firmware_revision, 0, 3);
	memset(fw_version, 0, 13);
	memset(calib_path_alt, 0, 120);

	strcat(calib_path_alt, "/data/misc/camera/");
	strcat(calib_path_alt, "R8_MVEN001_LD2_ND0_IR0_SH0_FL1_SVE");
	strcat(calib_path_alt, "N001_DCCID1066/calib.bin");
	firmware_check_from_ducati("/system/vendor/firmware/ducati-m3.bin",
			firmware_rear);
	if (firmware_check_from_calib(calib_path,
				firmware_vendor, firmware_revision) < 0) {
		if (firmware_check_from_calib(calib_path_alt,
					firmware_vendor,
					firmware_revision) < 0) {

			sprintf(firmware_vendor,   "%s", "S");
			sprintf(firmware_revision, "%s", "AA");
		}
	}
	strcat(fw_version, firmware_vendor);
	strcat(fw_version, fixed_str);
	strcat(fw_version, firmware_revision);
	strcat(fw_version, firmware_rear);

	return sprintf(buf, "%s", fw_version);
}

unsigned int vt_stby_gpio;
static ssize_t front_camera_fw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char fw_front[5];
	char fw_version[13];
	char fw_vendor[2];
	char fixed_str[] = "02T00AA";
	int  ret = 0;

	memset(fw_vendor, 0, 2);
	memset(fw_front, 0, 5);
	memset(fw_version, 0, 13);

	ret = gpio_get_value(vt_stby_gpio);

	if (ret == 0) {/* Module Vendor : CAMSYS */
		strcpy(fw_vendor, "C");
	} else if (ret == 1) { /* Module Vendor : Partron */
		strcpy(fw_vendor, "P");
	}

	firmware_check_from_ducati("/system/vendor/firmware/ducati-m3.bin",
			fw_front);
	strcat(fw_version, fw_vendor);
	strcat(fw_version, fixed_str);
	strcat(fw_version, fw_front);

	return sprintf(buf, "%s", fw_version);
}

static struct device_attribute dev_attr_camfw_rear =
	__ATTR(rear_camfw, S_IRUGO, rear_camera_fw_show, NULL);

static struct device_attribute dev_attr_camfw_front =
	__ATTR(front_camfw, S_IRUGO, front_camera_fw_show, NULL);

static ssize_t show_camera_flash(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int brightness = max77693_get_flash_brightness();

	return sprintf(buf, "%d", brightness);
}

static ssize_t set_camera_flash(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int brightness;
	int ret;

	ret = kstrtoint(buf, 10, &brightness);
	if (unlikely(ret < 0)) {
		pr_err("%s: failed to get brightness from input value.\n",
				__func__);
		return count;
	}

	max77693_set_flash_brightness(brightness);

	return count;
}

static DEVICE_ATTR(rear_flash, S_IWUSR | S_IRUGO,
		show_camera_flash, set_camera_flash);

static ssize_t cam_power_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%u\n", power_enable);
}

static ssize_t cam_power_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int rc;
	unsigned int val;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	power_enable = val;

	if (power_enable == 1)
		cam_regulator_request(power_enable);
	else
		cam_regulator_release();

	return count;
}

static DEVICE_ATTR(cam_power, S_IWUSR | S_IRUGO,
				cam_power_show, cam_power_store);

static int __init cam_regulator_eeprom_create_sysfs(void)
{
	struct class *cam_dev_class;
	struct device *cam_ldo_dev;

	cam_dev_class = class_create(THIS_MODULE, "cam");
	if (IS_ERR(cam_dev_class)) {
		pr_err("Failed to create class(camera_dev_class)!\n");
		return -EAGAIN;
	}

	cam_ldo_dev = device_create(cam_dev_class, NULL, 0,
					NULL, "cam_eeprom_power");

	if (IS_ERR(cam_ldo_dev)) {
		pr_err("(%s): failed to create device\n", __func__);
		class_destroy(cam_dev_class);
		return -EAGAIN;
	}

	if (device_create_file(cam_ldo_dev, &dev_attr_cam_power)) {
		pr_err("(%s): failed to create device file(log)!\n", __func__);
		device_destroy(cam_dev_class, 0);
		class_destroy(cam_dev_class);
		return -EAGAIN;
	}
	return 0;
}

static
int cam_regulator_request(unsigned int ldo_enable)
{
	int ret = 0, i = 0;
	struct cam_regulator_depot cam_reg_depot[CAM_REG_MAX];
	char *reg_name = NULL;
	int min_uv = 0, max_uv = 0;

	memset(&cam_reg_depot, 0, sizeof(struct cam_regulator_depot));

	for (i = 0; i < CAM_REG_MAX; i++) {
		reg_name = regulator_name[i];
		if (NULL == cam_reg_depot[i].reg_p)
			cam_reg_depot[i].reg_p = regulator_get(NULL, reg_name);

		if (IS_ERR_OR_NULL(cam_reg_depot[i].reg_p)) {
			pr_err("%s: error providing regulator %s\n",
					 __func__, reg_name);
			ret = -EINVAL;
		}

		cam_reg_depot[i].orig_uv = regulator_get_voltage(
		cam_reg_depot[i].reg_p);

		switch (i) {
		case  0:
			min_uv = 1800000;
			max_uv = 1800000;
			break;
			/* TODO: Further case statements can be added if new LDO
				regulators are added to the list */
		default:
				min_uv = 1800000;
				max_uv = 1800000;
			break;
		}
		ret = regulator_set_voltage(
				cam_reg_depot[i].reg_p, min_uv, max_uv);
		ret |= regulator_enable(cam_reg_depot[i].reg_p);
		regulator_put(cam_reg_depot[i].reg_p);
		if (ret < 0)
			pr_err("\n Error while trying to Enable CAM EEPROM LDO ");
	}
	return ret;
}

static void cam_regulator_release(void)
{
	struct cam_regulator_depot cam_reg_depot[CAM_REG_MAX];
	char *reg_name = NULL;
	unsigned int i = 0, ret = 0;

	memset(&cam_reg_depot, 0, sizeof(struct cam_regulator_depot));

	for (i = 0; i < CAM_REG_MAX; i++) {
		reg_name = regulator_name[i];
		if (NULL == cam_reg_depot[i].reg_p)
			cam_reg_depot[i].reg_p = regulator_get(NULL, reg_name);

		if (IS_ERR_OR_NULL(cam_reg_depot[i].reg_p)) {
			pr_err("%s: error providing regulator %s\n",
					__func__, reg_name);
			return;
		}
		if (regulator_is_enabled(cam_reg_depot[i].reg_p)) {
			ret = regulator_disable(cam_reg_depot[i].reg_p);
			if (ret < 0)
				pr_err("\n Error while disabling the CAM EEPROM LDO ");
		}
		regulator_put(cam_reg_depot[i].reg_p);
	}
}

void __init omap4_superior_camera_init(void)
{
	struct class *camera_dev_class;
	struct device *cam_dev_rear;
	struct device *cam_dev_front;

	camera_dev_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_dev_class)) {
		pr_err("Failed to create class(camera_dev_class)!\n");
		goto OUT5;
	}

	cam_dev_rear = device_create(camera_dev_class, NULL,
		MKDEV(CAM_MAJOR, 0), NULL, "rear");
	if (IS_ERR(cam_dev_rear)) {
		pr_err("Failed to create device(cam_dev_back)!\n");
		goto OUT4;
	}

	if (device_create_file(cam_dev_rear, &dev_attr_camtype_rear) < 0) {
		pr_err("Failed to create device file: %s\n",
				dev_attr_camtype_rear.attr.name);
		goto OUT3;
	}

	if (device_create_file(cam_dev_rear, &dev_attr_camfw_rear) < 0) {
		pr_err("Failed to create device file: %s\n",
				dev_attr_camfw_rear.attr.name);
		goto OUT3;
	}
	if (device_create_file(cam_dev_rear, &dev_attr_rear_flash) < 0) {
		pr_err("Failed to create device file: %s\n",
				dev_attr_rear_flash.attr.name);
		goto OUT3;
	}
	cam_dev_front = device_create(camera_dev_class, NULL,
		MKDEV(CAM_MAJOR, 1), NULL, "front");
	if (IS_ERR(cam_dev_front)) {
		pr_err("Failed to create device(cam_dev_front)!\n");
		goto OUT2;
	}

	if (device_create_file(cam_dev_front, &dev_attr_camtype_front) < 0) {
		pr_err("Failed to create device file: %s\n",
				dev_attr_camtype_front.attr.name);
		goto OUT1;
	}

	if (device_create_file(cam_dev_front, &dev_attr_camfw_front) < 0) {
		pr_err("Failed to create device file: %s\n",
				dev_attr_camfw_front.attr.name);
		goto OUT1;
	}
	if (cam_regulator_eeprom_create_sysfs())
		pr_err("Failed to create sysfs cam eeprom LDO(cam_eeprom_power)\n");

	/* Initialize */
	vt_stby_gpio = omap_muxtbl_get_gpio_by_name("VT_STBY");
	torch_on = 0;

	return;

OUT1:
	device_destroy(sec_class, MKDEV(CAM_MAJOR, 1));
OUT2:
	device_remove_file(cam_dev_rear, &dev_attr_camtype_rear);
	device_remove_file(cam_dev_rear, &dev_attr_camfw_rear);
	device_remove_file(cam_dev_rear, &dev_attr_rear_flash);
OUT3:
	device_destroy(sec_class, MKDEV(CAM_MAJOR, 0));
OUT4:
	class_destroy(camera_dev_class);
OUT5:
	return;
}

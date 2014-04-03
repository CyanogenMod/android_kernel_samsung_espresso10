/*
 * TMP102 Temperature sensor driver file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Steven King <sfking@fdwdc.com>
 * Author: Sabatier, Sebastien" <s-sabatier1@ti.com>
 * Author: Mandrenko, Ievgen" <ievgen.mandrenko@ti.com>
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
 *
 */

#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <plat/common.h>
#include <plat/tmp102_temp_sensor.h>

#include <plat/omap_device.h>
#include <plat/omap-pm.h>

#include <linux/thermal_framework.h>

#define	TMP102_TEMP_REG			0x00
#define	TMP102_CONF_REG			0x01
/* note: these bit definitions are byte swapped */
#define		TMP102_CONF_SD		0x0100
#define		TMP102_CONF_TM		0x0200
#define		TMP102_CONF_POL		0x0400
#define		TMP102_CONF_F0		0x0800
#define		TMP102_CONF_F1		0x1000
#define		TMP102_CONF_R0		0x2000
#define		TMP102_CONF_R1		0x4000
#define		TMP102_CONF_OS		0x8000
#define		TMP102_CONF_EM		0x0010
#define		TMP102_CONF_AL		0x0020
#define		TMP102_CONF_CR0		0x0040
#define		TMP102_CONF_CR1		0x0080
#define		TMP102_TLOW_REG		0x02
#define		TMP102_THIGH_REG	0x03

/*
 * omap_temp_sensor structure
 * @iclient - I2c client pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for sysfs, irq and PM
 * @therm_fw - thermal device
 */
struct tmp102_temp_sensor {
	struct i2c_client *iclient;
	struct device *dev;
	struct mutex sensor_mutex;
	struct thermal_dev *therm_fw;
	struct tmp102_platform_data *pdata;
	u16 config_orig;
	unsigned long last_update;
	int temp[3];
	int debug_temp;
	struct platform_device *siop_pdev;
};

/* SMBus specifies low byte first, but the TMP102 returns high byte first,
 * so we have to swab16 the values */
static inline int tmp102_read_reg(struct i2c_client *client, u8 reg)
{
	int result = i2c_smbus_read_word_data(client, reg);

	return result < 0 ? result : swab16(result);
}

static inline int tmp102_write_reg(struct i2c_client *client,
				u8 reg, u16 val)
{
	return i2c_smbus_write_word_data(client, reg, swab16(val));
}

/* convert left adjusted 13-bit TMP102 register value to milliCelsius */
static inline int tmp102_reg_to_mC(s16 val)
{
	return ((val & ~0x01) * 1000) / 128;
}

/* convert milliCelsius to left adjusted 13-bit TMP102 register value */
static inline u16 tmp102_mC_to_reg(int val)
{
	return (val * 128) / 1000;
}

static const u8 tmp102_reg[] = {
	TMP102_TEMP_REG,
	TMP102_TLOW_REG,
	TMP102_THIGH_REG,
};
#if defined(CONFIG_SAMSUNG_SIOP_SUPPORT)

#define SIOP_LEVEL0_TEMP	0
/*42C*/
#define SIOP_LEVEL1_TEMP	560
/*45C*/
#define SIOP_LEVEL2_TEMP	600
/*48C*/
#define SIOP_LEVEL3_TEMP	670

#define SIOP_HYTERESIS		30

#define SIOP_LEVEL_COUNT	4
struct sec_siop_info {
	struct tmp102_temp_sensor *tmp102;
	int siop_level_threshold[SIOP_LEVEL_COUNT];
	int siop_hysteresis;
};

static ssize_t sec_therm_show_temperature(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_siop_info *info = dev_get_drvdata(dev);
	struct tmp102_temp_sensor *tmp102 = info->tmp102;

	return sprintf(buf, "%d\n", tmp102->therm_fw->current_temp/100);
}

static ssize_t sec_therm_show_temp_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_siop_info *info = dev_get_drvdata(dev);
	struct tmp102_temp_sensor *tmp102 = info->tmp102;

	return sprintf(buf, "%d\n", tmp102->therm_fw->current_temp);
}

#define sysfs_siop_level(l) \
static ssize_t sec_siop_show_level##l(struct device *dev,	\
			struct device_attribute *attr,			\
			char *buf)	\
{						\
	struct sec_siop_info *info = dev_get_drvdata(dev);\
	return sprintf(buf, "%d\n", info->siop_level_threshold[l]);	\
}	\
static ssize_t sec_siop_store_level##l(struct device *dev,\
			struct device_attribute *attr,	\
			char *buf, size_t count)		\
{	\
						\
	int ret;			\
	unsigned long val;	\
	struct sec_siop_info *info = dev_get_drvdata(dev);\
					\
	ret = strict_strtoul(buf, 0, &val);	\
	if (ret < 0)					\
		return ret;					\
									\
	info->siop_level_threshold[l] = val;	\
	return count;					\
}

sysfs_siop_level(0)
sysfs_siop_level(1)
sysfs_siop_level(2)
sysfs_siop_level(3)

#define siop_device_attr(num) \
static DEVICE_ATTR(siop_level##num, S_IRUGO | S_IWUSR, \
		sec_siop_show_level##num,		\
		sec_siop_store_level##num)

static DEVICE_ATTR(temperature, S_IRUGO, sec_therm_show_temperature, NULL);
static DEVICE_ATTR(temp_adc, S_IRUGO, sec_therm_show_temp_adc, NULL);
static DEVICE_ATTR(siop_level0, S_IRUGO,
		sec_siop_show_level0,
		sec_siop_store_level0);

siop_device_attr(1);
siop_device_attr(2);
siop_device_attr(3);

static struct attribute *sec_therm_attributes[] = {
	&dev_attr_temperature.attr,
	&dev_attr_temp_adc.attr,
	&dev_attr_siop_level0.attr,
	&dev_attr_siop_level1.attr,
	&dev_attr_siop_level2.attr,
	&dev_attr_siop_level3.attr,
	NULL
};

static const struct attribute_group sec_therm_group = {
	.attrs = sec_therm_attributes,
};

static int find_samsung_siop_level(struct sec_siop_info *info, int increase)
{
	unsigned int level = 0;
	int i = 0;
	int hysteresis = (increase) ? 0 : SIOP_HYTERESIS;
	int current_temp = info->tmp102->therm_fw->current_temp/100;

	for (i = 0 ; i < SIOP_LEVEL_COUNT ; i++) {
		if (current_temp >=
				(info->siop_level_threshold[i] - hysteresis))
			level = i;
	}


	return level;
}
static void notify_change_of_temperature(struct tmp102_temp_sensor *tmp102)
{
	char temp_buf[20];
	char siop_buf[20];
	char *envp[2];
	int env_offset = 0;
	int siop_level = 0;
	int ret = 0;
	static int prev_temp ;
	struct sec_siop_info *info = dev_get_drvdata(&tmp102->siop_pdev->dev);

	if (tmp102->therm_fw->current_temp/1000 == prev_temp/1000)
		return;

	snprintf(temp_buf, sizeof(temp_buf), "TEMPERATURE=%d",
			tmp102->therm_fw->current_temp/100);
	envp[env_offset++] = temp_buf;

	siop_level = find_samsung_siop_level(info,
			(tmp102->therm_fw->current_temp/100 > prev_temp/100));
	snprintf(siop_buf, sizeof(siop_buf),
			"SIOP_LEVEL=%d", siop_level);
	envp[env_offset++] = siop_buf;

	envp[env_offset] = NULL;

	dev_info(&tmp102->siop_pdev->dev, "%s: uevent: %s\n",
					__func__, temp_buf);
	dev_info(&tmp102->siop_pdev->dev, "%s: uevent: %s\n",
					__func__, siop_buf);

	ret = kobject_uevent_env(&tmp102->siop_pdev->dev.kobj,
			KOBJ_CHANGE, envp);
	if (ret < 0)
		dev_err(&tmp102->siop_pdev->dev,
				"Uevent Notify fail [%d]\n", ret);

	prev_temp = tmp102->therm_fw->current_temp;

	return;
}


static int __devinit samsung_siop_init(struct tmp102_temp_sensor *tmp102)
{
	int ret = 0;
	struct sec_siop_info *siop_info;
	tmp102->siop_pdev = platform_device_alloc("sec-thermistor", -1);

	siop_info = kmalloc(sizeof(struct sec_siop_info), GFP_KERNEL);
	ret = platform_device_add(tmp102->siop_pdev);
	if (ret)
		goto fail_register;

	if (!siop_info) {
		ret = -ENOMEM;
		goto fail_sysfs;
	}
	siop_info->tmp102 = tmp102;
	siop_info->siop_level_threshold[0] = SIOP_LEVEL0_TEMP;
	siop_info->siop_level_threshold[1] = SIOP_LEVEL1_TEMP;
	siop_info->siop_level_threshold[2] = SIOP_LEVEL2_TEMP;
	siop_info->siop_level_threshold[3] = SIOP_LEVEL3_TEMP;

	platform_set_drvdata(tmp102->siop_pdev, siop_info);


	ret = sysfs_create_group(&tmp102->siop_pdev->dev.kobj,
						&sec_therm_group);
	if (ret)
		goto fail_sysfs;


	dev_info(&tmp102->siop_pdev->dev, "Samsung SIOP init\n");

	return 0;

fail_sysfs:
	platform_device_put(tmp102->siop_pdev);
fail_register:
	return ret;
}
#else
static int __devinit samsung_siop_init
	(struct tmp102_temp_sensor *tmp102) { return 0; }

#endif
static int tmp102_read_current_temp(struct device *dev)
{
	int index = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	tmp102 = i2c_get_clientdata(client);

	mutex_lock(&tmp102->sensor_mutex);
	if (time_after(jiffies, tmp102->last_update + 4 * HZ)) {
		int status = tmp102_read_reg(client, tmp102_reg[index]);
		if (status > -1)
			tmp102->temp[index] = tmp102_reg_to_mC(status);
		tmp102->last_update = jiffies;
	}
	mutex_unlock(&tmp102->sensor_mutex);

	return tmp102->temp[index];
}

static int tmp102_get_temp(struct thermal_dev *tdev)
{
	int current_temp = 0;
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct tmp102_temp_sensor *tmp102 = platform_get_drvdata(pdev);
	current_temp =	tmp102_read_current_temp(tdev->dev);

	if (current_temp < 0)
		current_temp = 0;


	if (current_temp/1000 != tmp102->therm_fw->current_temp/1000)
		dev_info(&pdev->dev, "temperature = %d\n", current_temp);

	tmp102->therm_fw->current_temp = current_temp;

#if defined(CONFIG_SAMSUNG_SIOP_SUPPORT)
	notify_change_of_temperature(tmp102);
#endif

	return tmp102->therm_fw->current_temp;
}

/*
 * sysfs hook functions
 */
static ssize_t tmp102_show_temp_user_space(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", tmp102->debug_temp);
}

static ssize_t tmp102_set_temp_user_space(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);
	long val;

	if (strict_strtol(buf, 10, &val)) {
		count = -EINVAL;
		goto out;
	}

	/* Set new temperature */
	tmp102->debug_temp = val;

	tmp102->therm_fw->current_temp = val;
	thermal_sensor_set_temp(tmp102->therm_fw);
	/* Send a kobj_change */
	kobject_uevent(&tmp102->dev->kobj, KOBJ_CHANGE);

out:
	return count;
}

static int tmp102_temp_sensor_read_temp(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int temp = tmp102_read_current_temp(dev);

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR(debug_user, S_IWUSR | S_IRUGO, tmp102_show_temp_user_space,
			  tmp102_set_temp_user_space);
static DEVICE_ATTR(temp1_input, S_IRUGO, tmp102_temp_sensor_read_temp,
			  NULL);


static struct attribute *tmp102_temp_sensor_attributes[] = {
	&dev_attr_temp1_input.attr,
	&dev_attr_debug_user.attr,
	NULL
};

static const struct attribute_group tmp102_temp_sensor_attr_group = {
	.attrs = tmp102_temp_sensor_attributes,
};

static struct thermal_dev_ops tmp102_temp_sensor_ops = {
	.report_temp = tmp102_get_temp,
};

#define TMP102_CONFIG  (TMP102_CONF_TM | TMP102_CONF_EM | TMP102_CONF_CR1)
#define TMP102_CONFIG_RD_ONLY (TMP102_CONF_R0 | TMP102_CONF_R1 | TMP102_CONF_AL)

static int __devinit tmp102_temp_sensor_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tmp102_temp_sensor *tmp102;
	struct tmp102_platform_data *tmp102_data = NULL;

	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "adapter doesn't support SMBus word "
			"transactions\n");

		return -ENODEV;
	}

	tmp102 = kzalloc(sizeof(struct tmp102_temp_sensor), GFP_KERNEL);
	if (!tmp102)
		return -ENOMEM;

	mutex_init(&tmp102->sensor_mutex);

	tmp102->iclient = client;
	tmp102->dev = &client->dev;

	if (client->dev.platform_data) {
		tmp102_data = client->dev.platform_data;
		tmp102->pdata = tmp102_data;
	}

	kobject_uevent(&client->dev.kobj, KOBJ_ADD);
	i2c_set_clientdata(client, tmp102);

	if (tmp102_data && tmp102_data->power_on)
		tmp102_data->power_on(true);

	ret = tmp102_read_reg(client, TMP102_CONF_REG);
	if (ret < 0) {
		dev_err(&client->dev, "error reading config register\n");
		goto free_err;
	}
	tmp102->config_orig = ret;
	ret = tmp102_write_reg(client, TMP102_CONF_REG, TMP102_CONFIG);
	if (ret < 0) {
		dev_err(&client->dev, "error writing config register\n");
		goto restore_config_err;
	}
	ret = tmp102_read_reg(client, TMP102_CONF_REG);
	if (ret < 0) {
		dev_err(&client->dev, "error reading config register\n");
		goto restore_config_err;
	}
	ret &= ~TMP102_CONFIG_RD_ONLY;
	if (ret != TMP102_CONFIG) {
		dev_err(&client->dev, "config settings did not stick\n");
		ret = -ENODEV;
		goto restore_config_err;
	}
	tmp102->last_update = jiffies - HZ;
	mutex_init(&tmp102->sensor_mutex);

	ret = sysfs_create_group(&client->dev.kobj,
		&tmp102_temp_sensor_attr_group);
	if (ret)
		goto sysfs_create_err;

	tmp102->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (tmp102->therm_fw) {
		tmp102->therm_fw->name = TMP102_SENSOR_NAME;
		tmp102->therm_fw->domain_name = "pcb";
		tmp102->therm_fw->dev = tmp102->dev;
		tmp102->therm_fw->dev_ops = &tmp102_temp_sensor_ops;
		thermal_sensor_dev_register(tmp102->therm_fw);
	} else {
		ret = -ENOMEM;
		goto therm_fw_alloc_err;
	}


	ret = samsung_siop_init(tmp102);

	dev_info(&client->dev, "initialized\n");
	if (ret)
		goto sysfs_create_err;


	return ret;

sysfs_create_err:
	thermal_sensor_dev_unregister(tmp102->therm_fw);
	kfree(tmp102->therm_fw);
restore_config_err:
	tmp102_write_reg(client, TMP102_CONF_REG, tmp102->config_orig);
therm_fw_alloc_err:
free_err:
	if (tmp102_data && tmp102_data->power_on)
		tmp102_data->power_on(false);
	mutex_destroy(&tmp102->sensor_mutex);
	kfree(tmp102);

	return ret;
}

static int __devexit tmp102_temp_sensor_remove(struct i2c_client *client)
{
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &tmp102_temp_sensor_attr_group);

	/* Stop monitoring if device was stopped originally */
	if (tmp102->config_orig & TMP102_CONF_SD) {
		int config;

		config = tmp102_read_reg(client, TMP102_CONF_REG);
		if (config >= 0)
			tmp102_write_reg(client, TMP102_CONF_REG,
					 config | TMP102_CONF_SD);
	}

	kfree(tmp102);

	return 0;
}

static void tmp102_temp_sensor_shutdown(struct i2c_client *client)
{
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	if (tmp102->pdata && tmp102->pdata->power_on)
		tmp102->pdata->power_on(false);
}

#ifdef CONFIG_PM
static int tmp102_temp_sensor_suspend(struct i2c_client *client,
			pm_message_t mesg)
{
	int conf = tmp102_read_reg(client, TMP102_CONF_REG);

	if (conf < 0)
		return conf;
	conf |= TMP102_CONF_SD;

	return tmp102_write_reg(client, TMP102_CONF_REG, conf);
}

static int tmp102_temp_sensor_resume(struct i2c_client *client)
{
	int conf = tmp102_read_reg(client, TMP102_CONF_REG);

	if (conf < 0)
		return conf;
	conf &= ~TMP102_CONF_SD;

	return tmp102_write_reg(client, TMP102_CONF_REG, conf);
}

#else

tmp102_temp_sensor_suspend NULL
tmp102_temp_sensor_resume NULL

#endif /* CONFIG_PM */

static int tmp102_detect(struct i2c_client *new_client,
		       struct i2c_board_info *info)
{
	strlcpy(info->type, "tmp102_temp_sensor", I2C_NAME_SIZE);

	return 0;
}

static const struct i2c_device_id tmp102_id[] = {
	{ "tmp102_temp_sensor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmp102_id);

static struct i2c_driver tmp102_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe = tmp102_temp_sensor_probe,
	.remove = tmp102_temp_sensor_remove,
	.shutdown = tmp102_temp_sensor_shutdown,
	.suspend = tmp102_temp_sensor_suspend,
	.resume = tmp102_temp_sensor_resume,
	.driver = {
		.name = "tmp102_temp_sensor",
	},
	.id_table	= tmp102_id,
	.detect		= tmp102_detect,
};

static int __init tmp102_init(void)
{
	if (!cpu_is_omap447x())
		return 0;

	return i2c_add_driver(&tmp102_driver);
}
module_init(tmp102_init);

static void __exit tmp102_exit(void)
{
	i2c_del_driver(&tmp102_driver);
}
module_exit(tmp102_exit);

MODULE_DESCRIPTION("OMAP447X TMP102 Temperature Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("Texas Instruments Inc");

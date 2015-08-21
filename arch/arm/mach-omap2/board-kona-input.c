/* arch/arm/mach-omap2/board-kona-input.c
 *
 * Based on mach-omap2/board-palau-input.c
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
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
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/keyreset.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/sec_ts.h>
#include <linux/delay.h>
#include <linux/wacom_i2c.h>
#include <linux/slab.h>

#include <mach/cpufreq_limits.h>

#include <asm/mach-types.h>
#include <plat/omap4-keypad.h>

#include "board-kona.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "control.h"		/* Used at tsp_set_power func. */
#include "sec_debug.h"

#define TOUCH_DVFS_FREQ	800000
#define TOUCH_MAX_SLOT		10

struct touch_dvfs_data {
	struct work_struct dvfs_work;
	bool mode;
	bool enable;
};

static struct touch_dvfs_data dvfs_data;

static void touch_dvfs_work(struct work_struct *work)
{
	if (dvfs_data.enable)
		omap_cpufreq_min_limit(DVFS_LOCK_ID_TSP, TOUCH_DVFS_FREQ);
	else
		omap_cpufreq_min_limit_free(DVFS_LOCK_ID_TSP);
}

static void touch_dvfs_event(struct input_handle *handle,
				unsigned int type, unsigned int code, int value)
{
	static int slot;
	static int cnt;
	static bool state[TOUCH_MAX_SLOT];
	static bool epen_state;

	if (type == EV_ABS) {
		switch (code) {
		case ABS_MT_SLOT:
			slot = value;
			break;
		case ABS_MT_POSITION_X:
		case ABS_MT_POSITION_Y:
			if (!state[slot]) {
				state[slot] = true;
				cnt++;
			}
			break;
		case ABS_MT_TRACKING_ID:
			if (value == -1 && state[slot]) {
				state[slot] = false;
				cnt--;
			}
		}
	} else if (type == EV_KEY &&
			(code == BTN_TOOL_PEN || code == BTN_TOOL_RUBBER))
		epen_state = !!value;

	if ((cnt || epen_state) && !dvfs_data.enable) {
		dvfs_data.enable = true;
		schedule_work(&dvfs_data.dvfs_work);
	} else if (!cnt && !epen_state && dvfs_data.enable) {
		dvfs_data.enable = false;
		schedule_work(&dvfs_data.dvfs_work);
	}
}

static bool touch_dvfs_match(struct input_handler *handler,
							struct input_dev *dev)
{
	if (test_bit(EV_ABS, dev->evbit) &&
			test_bit(INPUT_PROP_DIRECT, dev->propbit))
		return true;
	else if (test_bit(EV_KEY, dev->evbit) && test_bit(EV_ABS, dev->evbit) &&
			test_bit(BTN_TOOL_PEN, dev->keybit) &&
			test_bit(BTN_TOOL_RUBBER, dev->keybit))
		return true;
	else
		return false;
}

static int touch_dvfs_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "touch_dvfs";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

err_unregister_handle:
	input_unregister_handle(handle);
err_free_handle:
	kfree(handle);
	return error;
}

static void touch_dvfs_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id touch_dvfs_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
	},
	{ },
};

static struct input_handler touch_dvfs_handler = {
	.event = touch_dvfs_event,
	.match = touch_dvfs_match,
	.connect = touch_dvfs_connect,
	.disconnect = touch_dvfs_disconnect,
	.name = "touch_dvfs",
	.id_table = touch_dvfs_ids,
};

/* keypad setting */
enum {
	GPIO_EXT_WAKEUP = 0,
	GPIO_VOL_UP,
	GPIO_VOL_DOWN,
};

static struct gpio keys_map_low_gpios[] __initdata = {
	[GPIO_EXT_WAKEUP] = {
		.label	= "EXT_WAKEUP",
	},
	[GPIO_VOL_UP] = {
		.label	= "VOL_UP",
	},
	[GPIO_VOL_DOWN] = {
		.label	= "VOL_DOWN",
	},
};

static struct gpio_event_direct_entry kona_gpio_keypad_keys_map_low[] = {
	[GPIO_EXT_WAKEUP] = {
		.code	= KEY_POWER,
	},
	[GPIO_VOL_DOWN] = {
		.code	= KEY_VOLUMEDOWN,
	},
	[GPIO_VOL_UP] = {
		.code	= KEY_VOLUMEUP,
	},
};

static struct gpio_event_input_info kona_gpio_keypad_keys_info_low = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= kona_gpio_keypad_keys_map_low,
	.keymap_size	= ARRAY_SIZE(kona_gpio_keypad_keys_map_low),
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

static struct gpio_event_info *kona_gpio_keypad_info[] = {
	&kona_gpio_keypad_keys_info_low.info,
};

static struct gpio_event_platform_data kona_gpio_keypad_data = {
	.name		= "sec_key",
	.info		= kona_gpio_keypad_info,
	.info_count	= ARRAY_SIZE(kona_gpio_keypad_info)
};

static struct platform_device kona_gpio_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= 0,
	.dev = {
		.platform_data = &kona_gpio_keypad_data,
	},
};

ssize_t sec_key_pressed_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	unsigned int key_press_status = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(kona_gpio_keypad_keys_map_low); i++) {
		if (unlikely(kona_gpio_keypad_keys_map_low[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		       (!gpio_get_value(kona_gpio_keypad_keys_map_low[i].gpio))
			<< i;
	}

	return sprintf(buf, "%u\n", key_press_status);
}

static DEVICE_ATTR(sec_key_pressed, S_IRUGO, sec_key_pressed_show, NULL);

static int kona_create_sec_key_dev(void)
{
	struct device *sec_key;
	sec_key = device_create(sec_class, NULL, 0, NULL, "sec_key");
	if (!sec_key) {
		pr_err("Failed to create sysfs(sec_key)!\n");
		return -ENOMEM;
	}

	if (device_create_file(sec_key, &dev_attr_sec_key_pressed) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_sec_key_pressed.attr.name);

	return 0;
}

static void __init kona_gpio_keypad_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(keys_map_low_gpios); i++)
		kona_gpio_keypad_keys_map_low[i].gpio =
		    omap_muxtbl_get_gpio_by_name(keys_map_low_gpios[i].label);
}

static void __init kona_input_keyboard_init(void)
{
	kona_gpio_keypad_gpio_init();
	kona_create_sec_key_dev();

	if (unlikely(sec_debug_get_level()))
		kona_gpio_keypad_keys_info_low.flags |= GPIOEDF_PRINT_KEYS;

	platform_device_register(&kona_gpio_keypad_device);
}

/* touch device settings for synaptics rmi control program */
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_RMI4)

#include <linux/rmi.h>

struct syna_gpio_data {
	u16 gpio_number;
	char *gpio_name;
};

static int SYNA_ts_power(bool on)
{
	struct regulator *tsp_vdd, *tsp_pull_up;

	tsp_vdd = regulator_get(NULL, "TSP_3.3V");
	tsp_pull_up = regulator_get(NULL, "TSP_1.8V");

	if (IS_ERR(tsp_vdd) || IS_ERR(tsp_pull_up)) {
		pr_err("tsp: cannot get touch regulator.\n");
		return 0;
	}

	if (on) {
		pr_info("tsp: power on.\n");
		regulator_enable(tsp_pull_up);
		regulator_enable(tsp_vdd);
		mdelay(300); /* for guarantee a boot time */
	} else {
		pr_info("tsp: power off.\n");
		regulator_disable(tsp_vdd);
		regulator_disable(tsp_pull_up);
	}

	return 0;
}

static int synaptics_touchpad_gpio_setup(void *gpio_data, bool configure)
{
	int retval = 0;
	struct syna_gpio_data *data = gpio_data;

	pr_info("%s: RMI4 gpio configuration set to %d.\n",
							__func__, configure);

	if (configure) {
		retval = gpio_request(data->gpio_number, "rmi4_attn");
		if (retval) {
			pr_err("%s: Failed to get attn gpio %d. Code: %d.",
			       __func__, data->gpio_number, retval);
			return retval;
		}

		retval = gpio_direction_input(data->gpio_number);
		if (retval) {
			pr_err("%s: Failed to setup attn gpio %d. Code: %d.",
			       __func__, data->gpio_number, retval);
			gpio_free(data->gpio_number);
		}
	} else {
		pr_warn("%s: No way to deconfigure gpio %d.",
						__func__, data->gpio_number);
	}

	return SYNA_ts_power(configure);
}

static struct syna_gpio_data SYNA_gpiodata = {
	.gpio_name	= "TOUCH_nINT",
};

static struct rmi_device_platform_data SYNA_platformdata = {
	.driver_name	= "rmi_generic",
	.sensor_name	= "kona",
	.attn_polarity	= RMI_ATTN_ACTIVE_LOW,
	.level_triggered = true,
	.gpio_data	= &SYNA_gpiodata,
	.gpio_config	= synaptics_touchpad_gpio_setup,
	.axis_align	= { },
};

static struct i2c_board_info __initdata synaptics_i2c_devices[] = {
	{
		I2C_BOARD_INFO("rmi_i2c", 0x20),
		.platform_data = &SYNA_platformdata,
	},
};

static void __init kona_input_tsp_init(void)
{
	SYNA_platformdata.attn_gpio = SYNA_gpiodata.gpio_number =
			omap_muxtbl_get_gpio_by_name(SYNA_gpiodata.gpio_name);

	i2c_register_board_info(3, synaptics_i2c_devices,
				ARRAY_SIZE(synaptics_i2c_devices));
}

/* touch device settings for samsung synaptics driver */
#else

enum {
	GPIO_TOUCH_nINT = 0,
	GPIO_TOUCH_SCL,
	GPIO_TOUCH_SDA,
};

static struct gpio tsp_gpios[] = {
	[GPIO_TOUCH_nINT] = {
		.flags	= GPIOF_IN,
		.label	= "TOUCH_nINT",
	},
	[GPIO_TOUCH_SCL] = {
		.label	= "TSP_I2C_SCL",
	},
	[GPIO_TOUCH_SDA] = {
		.label	= "TSP_I2C_SDA",
	},
};

static void tsp_set_power_regulator(bool on)
{
	static struct regulator *tsp_vdd, *tsp_pull_up;
	static bool boot_on;

	if (unlikely(!tsp_vdd || !tsp_pull_up)) {
		tsp_vdd = regulator_get(NULL, "TSP_3.3V");
		tsp_pull_up = regulator_get(NULL, "TSP_1.8V");

		if (IS_ERR(tsp_vdd) || IS_ERR(tsp_pull_up)) {
			pr_err("tsp: cannot get touch regulator.\n");
			return;
		}
	}

	if (on) {
		pr_info("tsp: power on.\n");
		regulator_enable(tsp_pull_up);
		regulator_enable(tsp_vdd);
		if (likely(boot_on))
			msleep(300); /* for guarantee a boot time */
		else
			boot_on = true;
	} else {
		pr_info("tsp: power off.\n");
		regulator_disable(tsp_vdd);
		regulator_disable(tsp_pull_up);
		msleep(50);
	}

	return;
}

static struct sec_ts_platform_data kona_ts_pdata = {
	.model_name	= "N5100",
	.fw_name	= "synaptics/n5100.fw",
	.ext_fw_name	= "/mnt/sdcard/n5100.img",
	.rx_channel_no	= 41, /* Rx ch. */
	.tx_channel_no	= 26, /* Tx ch. */
	.x_pixel_size	= 799,
	.y_pixel_size	= 1279,
	.pivot		= false,
	.ta_state	= CABLE_NONE,
	.set_power	= tsp_set_power_regulator,
};

static struct i2c_board_info kona_i2c3_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("synaptics_ts", 0x20),
		.platform_data	= &kona_ts_pdata,
	},
};

static void __init kona_tsp_gpio_init(void)
{
	int i, n = ARRAY_SIZE(tsp_gpios);

	for (i = 0; i < n; i++)
		tsp_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(tsp_gpios[i].label);

	if (gpio_request_array(tsp_gpios, n) < 0)
		pr_err("tsp: gpio_request failed.");

	kona_i2c3_boardinfo[0].irq =
				gpio_to_irq(tsp_gpios[GPIO_TOUCH_nINT].gpio);

	kona_ts_pdata.gpio_irq = tsp_gpios[GPIO_TOUCH_nINT].gpio;
	kona_ts_pdata.gpio_scl = tsp_gpios[GPIO_TOUCH_SCL].gpio;
	kona_ts_pdata.gpio_sda = tsp_gpios[GPIO_TOUCH_SDA].gpio;
}

static void __init kona_input_tsp_init(void)
{
	kona_tsp_gpio_init();

	i2c_register_board_info(3, kona_i2c3_boardinfo,
				ARRAY_SIZE(kona_i2c3_boardinfo));
}

void omap4_kona_tsp_ta_detect(int cable_type)
{
	struct regulator *tsp_vdd;
	tsp_vdd = regulator_get(NULL, "TSP_3.3V");

	switch (cable_type) {
	case POWER_SUPPLY_TYPE_MAINS:
	kona_ts_pdata.ta_state = CABLE_TA;
	break;
	case POWER_SUPPLY_TYPE_USB:
	kona_ts_pdata.ta_state = CABLE_USB;
	break;
	case POWER_SUPPLY_TYPE_BATTERY:
	default:
	kona_ts_pdata.ta_state = CABLE_NONE;
	}

	/* Conditions to prevent kernel panic */
	if (kona_ts_pdata.set_ta_mode && regulator_is_enabled(tsp_vdd))
		kona_ts_pdata.set_ta_mode(&kona_ts_pdata.ta_state);
	return;
}
#endif

/* wacom device setting */
enum {
	GPIO_PEN_LDO_EN = 0,
	GPIO_PEN_IRQ,
	GPIO_PEN_PDCT,
	GPIO_PEN_DETECT,
	GPIO_PEN_FWE1,
};

static struct gpio wacom_gpios[] = {
	[GPIO_PEN_LDO_EN] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "PEN_LDO_EN",
	},
	[GPIO_PEN_IRQ] = {
		.flags	= GPIOF_IN,
		.label	= "PEN_IRQ_1.8V",
	},
	[GPIO_PEN_PDCT] = {
		.flags	= GPIOF_IN,
		.label	= "PEN_PDCT_1.8V",
	},
	[GPIO_PEN_DETECT] = {
		.flags	= GPIOF_IN,
		.label	= "PEN_DETECT",
	},
	[GPIO_PEN_FWE1] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "PEN_FWE1_1.8V",
	},
};

void wacom_power_on(bool on)
{
	if (on != gpio_get_value(wacom_gpios[GPIO_PEN_LDO_EN].gpio))
		gpio_set_value(wacom_gpios[GPIO_PEN_LDO_EN].gpio, on);
}


static struct wacom_platform_data wacom_pdata = {
	.x_invert = false,
	.y_invert = false,
	.xy_switch = false,
	.boot_on = true,
	.boot_addr = 0x09,
	.binary_fw_path = "wacom/n5100.fw",
	.file_fw_path = "/sdcard/firmware/wacom_firm.bin",
	.fw_version = 0x338,
	.fw_checksum = {0x1F, 0xF9, 0xE6, 0x18, 0x6A},
	.power = wacom_power_on,
};

static struct i2c_board_info kona_i2c10_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO(WACNAME, 0x56),
		.platform_data = &wacom_pdata,
	},
};

static void __init kona_wacom_gpio_init(void)
{
	int i, n = ARRAY_SIZE(wacom_gpios);

	for (i = 0; i < n; i++)
		wacom_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(wacom_gpios[i].label);

	if (system_rev  < 5)
		n--;
	else if (system_rev  < 4)
		n -= 2;

	if (gpio_request_array(wacom_gpios, n) < 0)
		pr_err("wacom: gpio_request failed.");

	kona_i2c10_boardinfo[0].irq =
				gpio_to_irq(wacom_gpios[GPIO_PEN_IRQ].gpio);

	wacom_pdata.gpio_pendct = wacom_gpios[GPIO_PEN_PDCT].gpio;
	wacom_pdata.gpio_pen_insert = wacom_gpios[GPIO_PEN_DETECT].gpio;
	wacom_pdata.gpio_pen_fwe1 = wacom_gpios[GPIO_PEN_FWE1].gpio;
}

static void __init kona_input_wacom_init(void)
{
	kona_wacom_gpio_init();

	i2c_register_board_info(10, kona_i2c10_boardinfo,
				ARRAY_SIZE(kona_i2c10_boardinfo));
}

static void touch_dvfs_handler_init(void)
{
	int ret;

	INIT_WORK(&dvfs_data.dvfs_work, touch_dvfs_work);
	ret = input_register_handler(&touch_dvfs_handler);
	if (ret < 0)
		pr_err("tsp: failed to register touch dvfs handler\n");
}

void __init omap4_kona_input_init(void)
{
	unsigned int board_type;

	kona_input_keyboard_init();
	kona_input_tsp_init();

	board_type = omap4_kona_get_board_type();

	if (board_type == SEC_MACHINE_KONA_WACOM
			|| board_type == SEC_MACHINE_KONA_WACOM_WIFI)
		kona_input_wacom_init();
	touch_dvfs_handler_init();
}

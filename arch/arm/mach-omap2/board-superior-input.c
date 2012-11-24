/* arch/arm/mach-omap2/board-superior-input.c
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
#include <linux/battery.h>
#include <linux/regulator/consumer.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/touchscreen/melfas.h>
#include <linux/platform_data/sec_ts.h>
#include <linux/delay.h>
#include <asm/mach-types.h>
#include <plat/omap4-keypad.h>
#include <linux/slab.h>

#include <mach/cpufreq_limits.h>

#include "board-superior.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "control.h"		/* Used at tsp_set_power func. */
#include "sec_debug.h"

#define TOUCH_DVFS_FREQ	800000
#define TOUCH_MAX_SLOT		10

struct touch_dvfs_data {
	struct work_struct dvfs_work;
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
	}
	if (cnt && !dvfs_data.enable) {
		dvfs_data.enable = true;
		schedule_work(&dvfs_data.dvfs_work);
	} else if (!cnt && dvfs_data.enable) {
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
	GPIO_HOME_KEY,
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
	[GPIO_HOME_KEY] = {
		.label	= "HOME_KEY",
	},
};

static struct gpio_event_direct_entry superior_gpio_keypad_keys_map_low[] = {
	[GPIO_EXT_WAKEUP] = {
		.code	= KEY_POWER,
	},
	[GPIO_VOL_DOWN] = {
		.code	= KEY_VOLUMEDOWN,
	},
	[GPIO_VOL_UP] = {
		.code	= KEY_VOLUMEUP,
	},
	[GPIO_HOME_KEY] = {
		.code	= KEY_HOMEPAGE,
	},
};

static struct gpio_event_input_info superior_gpio_keypad_keys_info_low = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= superior_gpio_keypad_keys_map_low,
	.keymap_size	= ARRAY_SIZE(superior_gpio_keypad_keys_map_low),
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

static struct gpio_event_info *superior_gpio_keypad_info[] = {
	&superior_gpio_keypad_keys_info_low.info,
};

static struct gpio_event_platform_data superior_gpio_keypad_data = {
	.name		= "sec_key",
	.info		= superior_gpio_keypad_info,
	.info_count	= ARRAY_SIZE(superior_gpio_keypad_info)
};

static struct platform_device superior_gpio_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= 0,
	.dev = {
		.platform_data = &superior_gpio_keypad_data,
	},
};

ssize_t sec_key_pressed_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	unsigned int key_press_status = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(superior_gpio_keypad_keys_map_low); i++) {
		if (unlikely(
			superior_gpio_keypad_keys_map_low[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		       (!gpio_get_value(
		       superior_gpio_keypad_keys_map_low[i].gpio))
			<< i;
	}

	return sprintf(buf, "%u\n", key_press_status);
}

static DEVICE_ATTR(sec_key_pressed, S_IRUGO, sec_key_pressed_show, NULL);

static int superior_create_sec_key_dev(void)
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

static void __init superior_gpio_keypad_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(keys_map_low_gpios); i++)
		superior_gpio_keypad_keys_map_low[i].gpio =
		    omap_muxtbl_get_gpio_by_name(keys_map_low_gpios[i].label);
}

static void __init superior_input_keyboard_init(void)
{
	superior_gpio_keypad_gpio_init();
	superior_create_sec_key_dev();

	if (unlikely(sec_debug_get_level()))
		superior_gpio_keypad_keys_info_low.flags |= GPIOEDF_PRINT_KEYS;

	platform_device_register(&superior_gpio_keypad_device);
}

/* touch keypad IC setting */
enum {
	GPIO_TOUCHKEY_INT = 0,
	GPIO_TOUCHKEY_EN,
};

static struct gpio tk_gpios[] = {
	[GPIO_TOUCHKEY_INT] = {
		.flags	= GPIOF_IN,
		.label	= "2TOUCH_INT",
	},
	[GPIO_TOUCHKEY_EN] = {
		.flags	= GPIOF_DIR_OUT,
		.label	= "2TOUCH_EN",
	},
};

static void tk_power_on_rev01(int enable)
{
	struct regulator *tk_led_33v;

	tk_led_33v = regulator_get(NULL, "KEYLED_3.3V");

	if (unlikely(IS_ERR_OR_NULL(tk_led_33v))) {
		pr_err("cptk: failed to get KEYLED_3.3V regulator.\n");
		return;
	}

	if (enable) {
		gpio_set_value(tk_gpios[GPIO_TOUCHKEY_EN].gpio, 1);
		regulator_enable(tk_led_33v);
		msleep(50);
	} else {
		gpio_set_value(tk_gpios[GPIO_TOUCHKEY_EN].gpio, 0);
		if (regulator_is_enabled(tk_led_33v))
			regulator_disable(tk_led_33v);
	}
	regulator_put(tk_led_33v);

	return;
}

static void tk_power_on_rev02(int enable)
{
	static struct regulator *tk_33v, *tk_led_33v;

	if (unlikely(!tk_33v || !tk_led_33v)) {
		tk_33v = regulator_get(NULL, "2TOUCH_3.3V");
		tk_led_33v = regulator_get(NULL, "KEYLED_3.3V");
	}

	if (unlikely(IS_ERR_OR_NULL(tk_33v) || IS_ERR_OR_NULL(tk_led_33v))) {
		pr_err("cptk: failed to get 2TOUCH regulator.\n");
		return;
	}

	if (enable) {
		regulator_enable(tk_33v);
		regulator_enable(tk_led_33v);
	} else {
		if (regulator_is_enabled(tk_33v) &&
					regulator_is_enabled(tk_led_33v)) {
			regulator_disable(tk_33v);
			regulator_disable(tk_led_33v);
		}
	}

	return;
}

static void tk_power_on_rev03(int enable)
{
	static struct regulator *tk_18v, *tk_33v, *tk_led_33v;

	if (unlikely(!tk_18v || !tk_led_33v || !tk_led_33v)) {
		tk_18v = regulator_get(NULL, "2TOUCH_1.8V");
		tk_33v = regulator_get(NULL, "2TOUCH_3.3V");
		tk_led_33v = regulator_get(NULL, "KEYLED_3.3V");
	}

	if (unlikely(IS_ERR_OR_NULL(tk_18v) || IS_ERR_OR_NULL(tk_33v) ||
				IS_ERR_OR_NULL(tk_led_33v))) {
		pr_err("cptk: failed to get 2TOUCH regulator.\n");
		return;
	}

	if (enable) {
		regulator_enable(tk_18v);
		regulator_enable(tk_33v);
		regulator_enable(tk_led_33v);
	} else if (regulator_is_enabled(tk_18v) &&
					regulator_is_enabled(tk_33v) &&
					regulator_is_enabled(tk_led_33v)) {
		regulator_disable(tk_18v);
		regulator_disable(tk_33v);
		regulator_disable(tk_led_33v);
	}

	return;
}

static const int tk_keymap[] = { 0, KEY_MENU, KEY_BACK, };

static struct cptk_platform_data superior_tk_data = {
	.power		= tk_power_on_rev03,
	.mod_ver	= 0x03,
	.firm_ver	= 0x06,
	.keymap		= tk_keymap,
	.keymap_size	= ARRAY_SIZE(tk_keymap),
	.fw_name	= "cypress/i9260.fw",
};

static struct i2c_board_info __initdata superior_tk_boardinfo[] = {
	{
		I2C_BOARD_INFO("cypress_touchkey", 0x20),
		.platform_data = &superior_tk_data,
	},
};

static void __init superior_tk_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tk_gpios); i++)
		tk_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(tk_gpios[i].label);
	gpio_request_array(tk_gpios, ARRAY_SIZE(tk_gpios));

	superior_tk_boardinfo[0].irq =
				gpio_to_irq(tk_gpios[GPIO_TOUCHKEY_INT].gpio);

	superior_tk_data.gpio = tk_gpios[GPIO_TOUCHKEY_INT].gpio;
	superior_tk_data.scl_pin = omap_muxtbl_get_gpio_by_name("2TOUCH_SCL");
	superior_tk_data.sda_pin = omap_muxtbl_get_gpio_by_name("2TOUCH_SDA");
}

static void __init superior_input_tk_init(void)
{
	superior_tk_gpio_init();
	i2c_register_board_info(8, superior_tk_boardinfo,
					ARRAY_SIZE(superior_tk_boardinfo));
	if (unlikely(system_rev == 2)) {
		superior_tk_data.power = tk_power_on_rev02;
	} else if (unlikely(system_rev == 1)) {
		superior_tk_data.power = tk_power_on_rev01;
		superior_tk_data.firm_ver = 0x00;
	}
}

/* touch screen IC setting */
enum {
	GPIO_TOUCH_INT = 0,
	GPIO_TOUCH_SCL,
	GPIO_TOUCH_SDA,
};

static struct gpio tsp_gpios[] = {
	[GPIO_TOUCH_INT] = {
		.flags	= GPIOF_IN,
		.label	= "TOUCH_nINT",
	},
	[GPIO_TOUCH_SCL] = {
		.label	= "AP_I2C_SCL",
	},
	[GPIO_TOUCH_SDA] = {
		.label	= "AP_I2C_SDA",
	},
};

static void tsp_set_power_regulator(bool on)
{
	static struct regulator *tsp_vdd;
	static bool boot_on;

	if (unlikely(!tsp_vdd)) {
		tsp_vdd = regulator_get(NULL, "TSP_AVDD_3.3V");

		if (IS_ERR(tsp_vdd)) {
			pr_err("tsp: cannot get TSP_AVDD_3.3V regulator.\n");
			return;
		}
	}

	if (on) {
		pr_info("tsp: power on.\n");
		regulator_enable(tsp_vdd);
		if (likely(boot_on))
			mdelay(200); /* for guarantee a boot time */
		else
			boot_on = true;
	} else if (!on && regulator_is_enabled(tsp_vdd)) {
		pr_info("tsp: power off.\n");
		regulator_disable(tsp_vdd);
		mdelay(50);
	}
}

static struct melfas_fw_info superior_tsp_fw_info = {
	.product_code = "465GS37",
	.core_version = 0x45,
};

static struct sec_ts_platform_data superior_ts_pdata = {
	.model_name	= "I9260",
	.fw_name	= "melfas/i9260.fw",
	.ext_fw_name	= "/mnt/sdcard/i9260.bin",
	.rx_channel_no	= 14,
	.tx_channel_no	= 26,
	.x_pixel_size	= 719,
	.y_pixel_size	= 1279,
	.private_data	= &superior_tsp_fw_info,
	.set_power	= tsp_set_power_regulator,
};

static struct i2c_board_info superior_i2c3_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("melfas_ts", 0x48),
		.platform_data	= &superior_ts_pdata,
	},
};

static void __init superior_tsp_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tsp_gpios); i++)
		tsp_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(tsp_gpios[i].label);

	gpio_request_array(tsp_gpios, ARRAY_SIZE(tsp_gpios));

	superior_i2c3_boardinfo[0].irq =
				gpio_to_irq(tsp_gpios[GPIO_TOUCH_INT].gpio);

	superior_ts_pdata.gpio_irq = tsp_gpios[GPIO_TOUCH_INT].gpio;
	superior_ts_pdata.gpio_scl = tsp_gpios[GPIO_TOUCH_SCL].gpio;
	superior_ts_pdata.gpio_sda = tsp_gpios[GPIO_TOUCH_SDA].gpio;
}

static void __init superior_input_tsp_init(void)
{
	superior_tsp_gpio_init();
	i2c_register_board_info(3, superior_i2c3_boardinfo,
				ARRAY_SIZE(superior_i2c3_boardinfo));
}

void touch_i2c_to_gpio(bool to_gpios)
{
	if (to_gpios) {
		gpio_direction_output(tsp_gpios[GPIO_TOUCH_INT].gpio, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
						tsp_gpios[GPIO_TOUCH_INT].gpio);

		gpio_direction_output(tsp_gpios[GPIO_TOUCH_SCL].gpio, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
						tsp_gpios[GPIO_TOUCH_SCL].gpio);

		gpio_direction_output(tsp_gpios[GPIO_TOUCH_SDA].gpio, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
						tsp_gpios[GPIO_TOUCH_SDA].gpio);
	} else {
		gpio_direction_output(tsp_gpios[GPIO_TOUCH_INT].gpio, 1);
		gpio_direction_input(tsp_gpios[GPIO_TOUCH_INT].gpio);
		omap_mux_set_gpio(OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE3,
						tsp_gpios[GPIO_TOUCH_INT].gpio);

		gpio_direction_output(tsp_gpios[GPIO_TOUCH_SCL].gpio, 1);
		gpio_direction_input(tsp_gpios[GPIO_TOUCH_SCL].gpio);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE0,
						tsp_gpios[GPIO_TOUCH_SCL].gpio);

		gpio_direction_output(tsp_gpios[GPIO_TOUCH_SDA].gpio, 1);
		gpio_direction_input(tsp_gpios[GPIO_TOUCH_SDA].gpio);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE0,
						tsp_gpios[GPIO_TOUCH_SDA].gpio);
	}

	return;
}

static void touch_dvfs_handler_init(void)
{
	int ret;

	INIT_WORK(&dvfs_data.dvfs_work, touch_dvfs_work);
	ret = input_register_handler(&touch_dvfs_handler);
	if (ret < 0)
		pr_err("tsp: failed to register touch dvfs handler\n");
}

void __init omap4_superior_input_init(void)
{
	superior_input_keyboard_init();
	superior_input_tk_init();
	superior_input_tsp_init();
	touch_dvfs_handler_init();
}

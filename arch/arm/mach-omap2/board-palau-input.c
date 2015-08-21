/* arch/arm/mach-omap2/board-palau-input.c
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
#include <linux/touchscreen/melfas.h>
#include <linux/platform_data/sec_ts.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <plat/omap4-keypad.h>

#include "board-palau.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "control.h"		/* Used at tsp_set_power func. */
#include "sec_debug.h"

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

static struct gpio_event_direct_entry palau_gpio_keypad_keys_map_low[] = {
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

static struct gpio_event_input_info palau_gpio_keypad_keys_info_low = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= palau_gpio_keypad_keys_map_low,
	.keymap_size	= ARRAY_SIZE(palau_gpio_keypad_keys_map_low),
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

static struct gpio_event_info *palau_gpio_keypad_info[] = {
	&palau_gpio_keypad_keys_info_low.info,
};

static struct gpio_event_platform_data palau_gpio_keypad_data = {
	.name		= "sec_key",
	.info		= palau_gpio_keypad_info,
	.info_count	= ARRAY_SIZE(palau_gpio_keypad_info)
};

static struct platform_device palau_gpio_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= 0,
	.dev = {
		.platform_data = &palau_gpio_keypad_data,
	},
};

ssize_t sec_key_pressed_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	unsigned int key_press_status = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(palau_gpio_keypad_keys_map_low); i++) {
		if (unlikely(palau_gpio_keypad_keys_map_low[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		       (!gpio_get_value(palau_gpio_keypad_keys_map_low[i].gpio))
			<< i;
	}

	return sprintf(buf, "%u\n", key_press_status);
}

static DEVICE_ATTR(sec_key_pressed, S_IRUGO, sec_key_pressed_show, NULL);

static int palau_create_sec_key_dev(void)
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

static void __init palau_gpio_keypad_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(keys_map_low_gpios); i++)
		palau_gpio_keypad_keys_map_low[i].gpio =
		    omap_muxtbl_get_gpio_by_name(keys_map_low_gpios[i].label);
}

static void __init palau_input_keyboard_init(void)
{
	palau_gpio_keypad_gpio_init();
	palau_create_sec_key_dev();

	if (unlikely(sec_debug_get_level()))
		palau_gpio_keypad_keys_info_low.flags |= GPIOEDF_PRINT_KEYS;

	platform_device_register(&palau_gpio_keypad_device);
}

enum {
	GPIO_TOUCH_nINT = 0,
	GPIO_TOUCH_SCL,
	GPIO_TOUCH_SDA,
	GPIO_TOUCH_EN,
};

static struct gpio tsp_gpios[] = {
	[GPIO_TOUCH_nINT] = {
		.flags	= GPIOF_IN,
		.label	= "TOUCH_nINT",
	},
	[GPIO_TOUCH_SCL] = {
		.label	= "AP_I2C_SCL",
	},
	[GPIO_TOUCH_SDA] = {
		.label	= "AP_I2C_SDA",
	},
	[GPIO_TOUCH_EN] = {
		.flags	= GPIOF_DIR_OUT,
		.label	= "TOUCH_EN",
	},
};

static struct touch_key palau_touch_keys[] = {
	{
		.name = "menu",
		.code = KEY_MENU,
	},
	{
		.name = "back",
		.code = KEY_BACK,
	},
};

static void tsp_set_power_gpio(bool on)
{
	u32 r;

	if (on) {
		pr_info("tsp: power on.\n");
		gpio_set_value(tsp_gpios[GPIO_TOUCH_EN].gpio, 1);

		r = omap4_ctrl_pad_readl(
				OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
		r &= ~OMAP4_I2C3_SDA_PULLUPRESX_MASK;
		r &= ~OMAP4_I2C3_SCL_PULLUPRESX_MASK;
		omap4_ctrl_pad_writel(r,
				OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
					tsp_gpios[GPIO_TOUCH_nINT].gpio);
	} else {
		pr_info("tsp: power off.\n");
		gpio_set_value(tsp_gpios[GPIO_TOUCH_EN].gpio, 0);

		/* Below register settings needed by prevent current leakage. */
		r = omap4_ctrl_pad_readl(
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
		r |= OMAP4_I2C3_SDA_PULLUPRESX_MASK;
		r |= OMAP4_I2C3_SCL_PULLUPRESX_MASK;
		omap4_ctrl_pad_writel(r,
				OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
					tsp_gpios[GPIO_TOUCH_nINT].gpio);
	}
	return;
}

static void tsp_set_power_regulator(bool on)
{
	struct regulator *tsp_vdd;

	tsp_vdd = regulator_get(NULL, "TSP_AVDD_3.3V");

	if (IS_ERR(tsp_vdd)) {
		pr_err("tsp: cannot get TSP_AVDD_3.3V regulator.\n");
		return;
	}

	if (on) {
		pr_info("tsp: power on.\n");
		regulator_enable(tsp_vdd);
		mdelay(300); /* for guarantee a boot time */
	} else {
		pr_info("tsp: power off.\n");
		regulator_disable(tsp_vdd);
	}

	return;
}

static struct melfas_fw_info palau_tsp_fw_info = {
	.product_code = "465GS37",
	.core_version = 0x45,
};

static struct sec_ts_platform_data palau_ts_pdata = {
	.model_name	= "I9270",
	.fw_name	= "melfas/mms144_palau.fw",
	.rx_channel_no	= 13, /* Rx ch. */
	.tx_channel_no	= 22, /* Tx ch. */
	.x_pixel_size	= 720,
	.y_pixel_size	= 1280,
	.ta_state	= CABLE_TYPE_NONE,
	.private_data	= &palau_tsp_fw_info,
	.set_power	= tsp_set_power_regulator,
	.key		= palau_touch_keys,
	.key_size	= ARRAY_SIZE(palau_touch_keys),
};

static struct i2c_board_info palau_i2c3_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("melfas_ts", 0x48),
		.platform_data	= &palau_ts_pdata,
	},
};

static void __init palau_tsp_gpio_init(void)
{
	int i, n = ARRAY_SIZE(tsp_gpios);

	/* To do not request a TOUCH_EN gpio at above rev.1 boards */
	n = (system_rev < 1) ? n : n - 1;

	for (i = 0; i < n; i++)
		tsp_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(tsp_gpios[i].label);

	if (gpio_request_array(tsp_gpios, n) < 0)
		pr_err("tsp: gpio_request failed.");

	palau_i2c3_boardinfo[0].irq =
				gpio_to_irq(tsp_gpios[GPIO_TOUCH_nINT].gpio);
}

static void __init palau_input_tsp_init(void)
{
	palau_tsp_gpio_init();

	i2c_register_board_info(3, palau_i2c3_boardinfo,
				ARRAY_SIZE(palau_i2c3_boardinfo));

	if (unlikely(system_rev < 1)) {
		palau_ts_pdata.set_power = tsp_set_power_gpio;
		palau_ts_pdata.gpio_en = tsp_gpios[GPIO_TOUCH_EN].gpio;
	}
}

void touch_i2c_to_gpio(bool to_gpios)
{
	if (to_gpios) {
		gpio_direction_output(tsp_gpios[GPIO_TOUCH_nINT].gpio, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
						tsp_gpios[GPIO_TOUCH_nINT].gpio);

		gpio_direction_output(tsp_gpios[GPIO_TOUCH_SCL].gpio, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
						tsp_gpios[GPIO_TOUCH_SCL].gpio);

		gpio_direction_output(tsp_gpios[GPIO_TOUCH_SDA].gpio, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3,
						tsp_gpios[GPIO_TOUCH_SDA].gpio);
	} else {
		gpio_direction_output(tsp_gpios[GPIO_TOUCH_nINT].gpio, 1);
		gpio_direction_input(tsp_gpios[GPIO_TOUCH_nINT].gpio);
		omap_mux_set_gpio(OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE3,
						tsp_gpios[GPIO_TOUCH_nINT].gpio);

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

void omap4_palau_tsp_ta_detect(int cable_type)
{
	palau_ts_pdata.ta_state = cable_type;

	/* Conditions to prevent kernel panic */
	if (palau_ts_pdata.set_ta_mode &&
				gpio_get_value(tsp_gpios[GPIO_TOUCH_EN].gpio))
		palau_ts_pdata.set_ta_mode(&palau_ts_pdata.ta_state);
	return;
}

void __init omap4_palau_input_init(void)
{
	palau_input_keyboard_init();
	palau_input_tsp_init();
}

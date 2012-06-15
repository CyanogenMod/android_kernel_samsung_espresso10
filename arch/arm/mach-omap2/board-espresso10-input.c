/*
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
#include <linux/platform_data/synaptics_s7301.h>
#include <asm/mach-types.h>
#include <plat/omap4-keypad.h>

#include "board-espresso10.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "control.h"
#include "sec_debug.h"

enum {
	GPIO_EXT_WAKEUP = 0,
};

static struct gpio keys_map_high_gpios[] __initdata = {
	[GPIO_EXT_WAKEUP] = {
		.label	= "EXT_WAKEUP",
	},
};

enum {
	GPIO_VOL_UP = 0,
	GPIO_VOL_DOWN,
};

static struct gpio keys_map_low_gpios[] __initdata = {
	[GPIO_VOL_UP] = {
		.label	= "VOL_UP",
	},
	[GPIO_VOL_DOWN] = {
		.label	= "VOL_DN",
	},
};

static struct gpio_event_direct_entry espresso_gpio_keypad_keys_map_high[] = {
	{
		.code	= KEY_POWER,
	},
};

static struct gpio_event_input_info espresso_gpio_keypad_keys_info_high = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= espresso_gpio_keypad_keys_map_high,
	.keymap_size	= ARRAY_SIZE(espresso_gpio_keypad_keys_map_high),
	.flags			= GPIOEDF_ACTIVE_HIGH,
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

static struct gpio_event_direct_entry espresso_gpio_keypad_keys_map_low[] = {
	[GPIO_VOL_DOWN] = {
		.code	= KEY_VOLUMEDOWN,
	},
	[GPIO_VOL_UP] = {
		.code	= KEY_VOLUMEUP,
	},
};

static struct gpio_event_input_info espresso_gpio_keypad_keys_info_low = {
	.info.func		= gpio_event_input_func,
	.info.no_suspend	= true,
	.type			= EV_KEY,
	.keymap			= espresso_gpio_keypad_keys_map_low,
	.keymap_size	= ARRAY_SIZE(espresso_gpio_keypad_keys_map_low),
	.debounce_time.tv64	= 2 * NSEC_PER_MSEC,
};

static struct gpio_event_info *espresso_gpio_keypad_info[] = {
	&espresso_gpio_keypad_keys_info_high.info,
	&espresso_gpio_keypad_keys_info_low.info,
};

static struct gpio_event_platform_data espresso_gpio_keypad_data = {
	.name		= "sec_key",
	.info		= espresso_gpio_keypad_info,
	.info_count	= ARRAY_SIZE(espresso_gpio_keypad_info)
};

static struct platform_device espresso_gpio_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= 0,
	.dev = {
		.platform_data = &espresso_gpio_keypad_data,
	},
};

enum {
	TSP_VENDOR1 = 0,
	TSP_VENDOR2,
	TSP_VENDOR3
};

static struct gpio ts_panel_gpios[] = {
	[TSP_VENDOR1] = {
		.label	= "TSP_VENDOR1",
	},
	[TSP_VENDOR2] = {
		.label	= "TSP_VENDOR2",
	},
	[TSP_VENDOR3] = {
		.label	= "TSP_VENDOR3",
	},
};

static struct gpio tsp_gpios[] = {
	[GPIO_TOUCH_nINT] = {
		.flags	= GPIOF_IN,
		.label	= "TSP_INT",
	},
	[GPIO_TOUCH_EN] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "TSP_LDO_ON",
	},
	[GPIO_TOUCH_SCL] = {
		.label	= "TSP_I2C_SCL_1.8V",
	},
	[GPIO_TOUCH_SDA] = {
		.label	= "TSP_I2C_SDA_1.8V",
	},
};

static void tsp_set_power(bool on)
{
	u32 r;

	if (on) {
		pr_debug("tsp: power on.\n");
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
		pr_debug("tsp: power off.\n");
		gpio_set_value(tsp_gpios[GPIO_TOUCH_EN].gpio, 0);

		/* Below register settings need to prevent current leakage. */
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

static struct synaptics_platform_data synaptics_ts_pdata = {
	.rx_channel_no = 42,	/* Y channel line */
	.tx_channel_no = 27,	/* X channel line */
	.x_pixel_size = 1279,
	.y_pixel_size = 799,
	.ta_state = CABLE_TYPE_NONE,
	.gpio_set = tsp_gpios,
	.set_power = tsp_set_power,
};

static struct i2c_board_info __initdata espresso_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("synaptics-ts", 0x20),
		.platform_data	= &synaptics_ts_pdata,
	},
};

ssize_t sec_key_pressed_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	unsigned int key_press_status = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(espresso_gpio_keypad_keys_map_high); i++) {
		if (unlikely
		    (espresso_gpio_keypad_keys_map_high[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		    ((gpio_get_value(espresso_gpio_keypad_keys_map_high[i].gpio)
		      << i));
	}

	for (i = 0; i < ARRAY_SIZE(espresso_gpio_keypad_keys_map_low); i++) {
		if (unlikely
		    (espresso_gpio_keypad_keys_map_low[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		    ((!gpio_get_value(espresso_gpio_keypad_keys_map_low[i].gpio)
		      << (i + ARRAY_SIZE(espresso_gpio_keypad_keys_map_high))));
	}

	return sprintf(buf, "%u\n", key_press_status);
}

static DEVICE_ATTR(sec_key_pressed, S_IRUGO, sec_key_pressed_show, NULL);

static int espresso_create_sec_key_dev(void)
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

static void __init espresso10_gpio_keypad_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(keys_map_high_gpios); i++)
		espresso_gpio_keypad_keys_map_high[i].gpio =
		    omap_muxtbl_get_gpio_by_name(keys_map_high_gpios[i].label);

	for (i = 0; i < ARRAY_SIZE(keys_map_low_gpios); i++)
		espresso_gpio_keypad_keys_map_low[i].gpio =
		    omap_muxtbl_get_gpio_by_name(keys_map_low_gpios[i].label);
}

static char *panel_name[] = {"iljin", "o-film", "s-mac"};

static void __init espresso10_ts_panel_setup(void)
{
	int i, panel_id = 0;

	for (i = 0; i < ARRAY_SIZE(ts_panel_gpios); i++)
		ts_panel_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(ts_panel_gpios[i].label);
	gpio_request_array(ts_panel_gpios, ARRAY_SIZE(ts_panel_gpios));

	for (i = 0; i < ARRAY_SIZE(ts_panel_gpios); i++)
		panel_id |= gpio_get_value(ts_panel_gpios[i].gpio) << i;

	synaptics_ts_pdata.panel_name = panel_name[panel_id];
}

static void __init espresso10_tsp_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tsp_gpios); i++)
		tsp_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(tsp_gpios[i].label);
	gpio_request_array(tsp_gpios, ARRAY_SIZE(tsp_gpios));

	espresso_i2c3_boardinfo[0].irq =
	    gpio_to_irq(tsp_gpios[GPIO_TOUCH_nINT].gpio);
}

void omap4_espresso10_tsp_ta_detect(int cable_type)
{
	synaptics_ts_pdata.ta_state = cable_type;

	/* Conditions for prevent kernel panic */
	if (synaptics_ts_pdata.set_ta_mode &&
				gpio_get_value(tsp_gpios[GPIO_TOUCH_EN].gpio))
		synaptics_ts_pdata.set_ta_mode(&synaptics_ts_pdata.ta_state);
	return;
}

void __init omap4_espresso10_input_init(void)
{
	u32 boardtype = omap4_espresso10_get_board_type();

	if (boardtype == SEC_MACHINE_ESPRESSO10_WIFI)
		synaptics_ts_pdata.model_name = "P5110";
	else if (boardtype == SEC_MACHINE_ESPRESSO10_USA_BBY)
		synaptics_ts_pdata.model_name = "P5113";
	else
		synaptics_ts_pdata.model_name = "P5100";

	if (system_rev >= 7)
		espresso10_ts_panel_setup();
	espresso10_gpio_keypad_gpio_init();
	espresso10_tsp_gpio_init();

	i2c_register_board_info(3, espresso_i2c3_boardinfo,
				ARRAY_SIZE(espresso_i2c3_boardinfo));

	espresso_create_sec_key_dev();

	if (sec_debug_get_level()) {
		espresso_gpio_keypad_keys_info_high.flags |= GPIOEDF_PRINT_KEYS;
		espresso_gpio_keypad_keys_info_low.flags |= GPIOEDF_PRINT_KEYS;
	}

	platform_device_register(&espresso_gpio_keypad_device);
}

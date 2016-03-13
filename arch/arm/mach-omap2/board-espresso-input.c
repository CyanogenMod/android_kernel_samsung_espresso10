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
#include <linux/delay.h>
#include <linux/platform_data/sec_ts.h>
#include <linux/touchscreen/synaptics.h>
#include <asm/mach-types.h>
#include <plat/omap4-keypad.h>

#include "board-espresso.h"
#include "mux.h"
#include "control.h"

#define GPIO_EXT_WAKEUP	3

/* Reversed on p51xx */
#define GPIO_VOL_UP		30
#define GPIO_VOL_DN		8

#define GPIO_TSP_INT		46
#define GPIO_TSP_LDO_ON	54
#define GPIO_TSP_I2C_SCL	130
#define GPIO_TSP_I2C_SDA	131

#define GPIO_TSP_VENDOR1	71
#define GPIO_TSP_VENDOR2	72
#define GPIO_TSP_VENDOR3	92

static struct gpio_event_direct_entry espresso_gpio_keypad_keys_map_high[] = {
	{
		.code	= KEY_POWER,
		.gpio	= GPIO_EXT_WAKEUP,
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
	{
		.code	= KEY_VOLUMEDOWN,
		.gpio	= GPIO_VOL_DN,
	},
	{
		.code	= KEY_VOLUMEUP,
		.gpio	= GPIO_VOL_UP,
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
	.name		= "espresso-gpio-keypad",
	.info		= espresso_gpio_keypad_info,
	.info_count	= ARRAY_SIZE(espresso_gpio_keypad_info)
};

static struct platform_device espresso_gpio_keypad_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id	= 0,
	.dev = {
		.platform_data = &espresso_gpio_keypad_data,
	},
};

static void tsp_set_power(bool on)
{
	u32 r;

	pr_debug("%s: %d\n", __func__, on);

	if (on) {
		gpio_set_value(GPIO_TSP_LDO_ON, 1);

		r = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
		r &= ~OMAP4_I2C3_SDA_PULLUPRESX_MASK;
		r &= ~OMAP4_I2C3_SCL_PULLUPRESX_MASK;
		omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);

		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3, GPIO_TSP_INT);
		if (board_is_espresso10()) msleep(300);
	} else {
		gpio_set_value(GPIO_TSP_LDO_ON, 0);

		/* Below register settings needed by prevent current leakage. */
		r = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
		r |= OMAP4_I2C3_SDA_PULLUPRESX_MASK;
		r |= OMAP4_I2C3_SCL_PULLUPRESX_MASK;
		omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);

		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3, GPIO_TSP_INT);
		if (board_is_espresso10()) msleep(50);
	}
}

const u32 espresso_tsp_fw_info = 0x17;

static struct synaptics_fw_info espresso10_tsp_fw_info = {
	.release_date = "1308",
};

static struct sec_ts_platform_data espresso_ts_pdata = {
	.fw_name	= "melfas/p3100.fw",
	.fw_info	= &espresso_tsp_fw_info,
	.rx_channel_no	= 13,
	.tx_channel_no	= 22,
	.x_pixel_size	= 1023,
	.y_pixel_size	= 599,
	.pivot		= false,
	.ta_state	= CABLE_TYPE_NONE,
	.set_power	= tsp_set_power,
	.gpio_irq	= GPIO_TSP_INT,
	.gpio_scl	= GPIO_TSP_I2C_SCL,
	.gpio_sda	= GPIO_TSP_I2C_SDA,
};

static struct i2c_board_info __initdata espresso_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("melfas_ts", 0x48),
		.platform_data	= &espresso_ts_pdata,
	},
};

static struct i2c_board_info __initdata espresso10_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("synaptics_ts", 0x20),
		.platform_data	= &espresso_ts_pdata,
	},
};

void touch_i2c_to_gpio(bool to_gpios)
{
	if (to_gpios) {
		gpio_direction_output(GPIO_TSP_INT, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3, GPIO_TSP_INT);

		gpio_direction_output(GPIO_TSP_I2C_SCL, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3, GPIO_TSP_I2C_SCL);

		gpio_direction_output(GPIO_TSP_I2C_SDA, 0);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE3, GPIO_TSP_I2C_SDA);
	} else {
		gpio_direction_output(GPIO_TSP_INT, 1);
		gpio_direction_input(GPIO_TSP_INT);
		omap_mux_set_gpio(OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE3, GPIO_TSP_INT);

		gpio_direction_output(GPIO_TSP_I2C_SCL, 1);
		gpio_direction_input(GPIO_TSP_I2C_SCL);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE0, GPIO_TSP_I2C_SCL);

		gpio_direction_output(GPIO_TSP_I2C_SDA, 1);
		gpio_direction_input(GPIO_TSP_I2C_SDA);
		omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_MUX_MODE0, GPIO_TSP_I2C_SDA);
	}
}

static void __init espresso_gpio_keypad_gpio_init(void)
{
	if (board_is_espresso10()) {
		espresso_gpio_keypad_keys_map_low[0].gpio = GPIO_VOL_UP;
		espresso_gpio_keypad_keys_map_low[1].gpio = GPIO_VOL_DN;
	}
}

static void __init espresso_tsp_gpio_init(void)
{
	struct gpio tsp_gpios[] = {
		{
			.flags	= GPIOF_IN,
			.gpio	= GPIO_TSP_INT,
			.label	= "TSP_INT",
		},
		{
			.flags	= GPIOF_OUT_INIT_HIGH,
			.gpio	= GPIO_TSP_LDO_ON,
			.label	= "TSP_LDO_ON",
		},
		{
			.label	= "TSP_I2C_SCL_1.8V",
			.gpio	= GPIO_TSP_I2C_SCL,
		},
		{
			.label	= "TSP_I2C_SDA_1.8V",
			.gpio	= GPIO_TSP_I2C_SDA,
		},
	};

	gpio_request_array(tsp_gpios, ARRAY_SIZE(tsp_gpios));

	espresso_i2c3_boardinfo[0].irq = gpio_to_irq(GPIO_TSP_INT);
	espresso10_i2c3_boardinfo[0].irq = gpio_to_irq(GPIO_TSP_INT);
}

static void __init espresso_ts_panel_setup(void)
{
	int i, panel_id = 0;
	const char *panel_name[8] = { "ILJIN", "DIGITECH", "iljin", "o-film", "s-mac" };
	struct gpio ts_panel_gpios[] = {
		{
			.label	= "TSP_VENDOR1",
			.gpio	= GPIO_TSP_VENDOR1,
			.flags	= GPIOF_IN
		},
		{
			.label	= "TSP_VENDOR2",
			.gpio	= GPIO_TSP_VENDOR2,
			.flags	= GPIOF_IN
		},
		{
			.label	= "TSP_VENDOR3",
			.gpio	= GPIO_TSP_VENDOR3,
			.flags	= GPIOF_IN
		},
	};

	gpio_request_array(ts_panel_gpios, ARRAY_SIZE(ts_panel_gpios));

	for (i = 0; i < ARRAY_SIZE(ts_panel_gpios); i++)
		panel_id |= gpio_get_value(ts_panel_gpios[i].gpio) << i;

	if (board_is_espresso10()) {
		espresso_ts_pdata.fw_name = "synaptics/p5100.fw";
		espresso_ts_pdata.fw_info = &espresso10_tsp_fw_info,
		espresso_ts_pdata.rx_channel_no	= 42,
		espresso_ts_pdata.tx_channel_no	= 27,
		espresso_ts_pdata.x_pixel_size	= 1279,
		espresso_ts_pdata.y_pixel_size	= 799,
		panel_id += 2;
	}
	espresso_ts_pdata.panel_name = panel_name[clamp(panel_id, 0, 7)];
}

void omap4_espresso_tsp_ta_detect(int cable_type)
{
	espresso_ts_pdata.ta_state = cable_type;

	/* Conditions for prevent kernel panic */
	if (espresso_ts_pdata.set_ta_mode && gpio_get_value(GPIO_TSP_LDO_ON))
		espresso_ts_pdata.set_ta_mode(&espresso_ts_pdata.ta_state);
}

void __init omap4_espresso_input_init(void)
{
	espresso_gpio_keypad_gpio_init();
	espresso_tsp_gpio_init();
	espresso_ts_panel_setup();

	if (!board_is_espresso10()) {
		i2c_register_board_info(3, espresso_i2c3_boardinfo,
				ARRAY_SIZE(espresso_i2c3_boardinfo));
	} else {
		i2c_register_board_info(3, espresso10_i2c3_boardinfo,
				ARRAY_SIZE(espresso10_i2c3_boardinfo));
	}

	platform_device_register(&espresso_gpio_keypad_device);
}

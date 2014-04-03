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
#include <linux/platform_data/cypress_cyttsp4.h>
#include <linux/platform_data/sec_ts.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <asm/mach-types.h>
#include <plat/omap4-keypad.h>

#include "board-gokey.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "control.h"
#include "sec_debug.h"

#define CY_I2C_TCH_ADR	0x24
#define CY_I2C_LDR_ADR	0x24

enum {
	GPIO_EXT_WAKEUP = 0,
};

static struct gpio keys_map_high_gpios[] __initdata = {
	[GPIO_EXT_WAKEUP] = {
			     .label = "EXT_WAKEUP",
			     },
};

enum {
	GPIO_VOL_UP = 0,
	GPIO_VOL_DOWN,
	GPIO_HOME_KEY,
};

static struct gpio keys_map_low_gpios[] __initdata = {
	[GPIO_VOL_UP] = {
			 .label = "VOL_UP",
			 },
	[GPIO_VOL_DOWN] = {
			   .label = "VOL_DN",
			   },
	[GPIO_HOME_KEY] = {
			   .label = "HOME_KEY",
			   },
};

static struct gpio_event_direct_entry gokey_gpio_keypad_keys_map_high[] = {
	{
	 .code = KEY_POWER,
	 },
};

static struct gpio_event_input_info gokey_gpio_keypad_keys_info_high = {
	.info.func = gpio_event_input_func,
	.info.no_suspend = true,
	.type = EV_KEY,
	.keymap = gokey_gpio_keypad_keys_map_high,
	.keymap_size = ARRAY_SIZE(gokey_gpio_keypad_keys_map_high),
	.debounce_time.tv64 = 2 * NSEC_PER_MSEC,
};

static struct gpio_event_direct_entry gokey_gpio_keypad_keys_map_low[] = {
	[GPIO_VOL_DOWN] = {
			   .code = KEY_VOLUMEDOWN,
			   },
	[GPIO_VOL_UP] = {
			 .code = KEY_VOLUMEUP,
			 },
	[GPIO_HOME_KEY] = {
			   .code = KEY_HOMEPAGE,
			   },
};

static struct gpio_event_input_info gokey_gpio_keypad_keys_info_low = {
	.info.func = gpio_event_input_func,
	.info.no_suspend = true,
	.type = EV_KEY,
	.keymap = gokey_gpio_keypad_keys_map_low,
	.keymap_size = ARRAY_SIZE(gokey_gpio_keypad_keys_map_low),
	.debounce_time.tv64 = 2 * NSEC_PER_MSEC,
};

static struct gpio_event_info *gokey_gpio_keypad_info[] = {
	&gokey_gpio_keypad_keys_info_high.info,
	&gokey_gpio_keypad_keys_info_low.info,
};

static struct gpio_event_platform_data gokey_gpio_keypad_data = {
	.name = "sec_key",
	.info = gokey_gpio_keypad_info,
	.info_count = ARRAY_SIZE(gokey_gpio_keypad_info)
};

static struct platform_device gokey_gpio_keypad_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev = {
		.platform_data = &gokey_gpio_keypad_data,
		},
};

static struct gpio tsp_gpios[] = {
	[GPIO_TOUCH_nINT] = {
		.flags = GPIOF_IN,
		.label = "TSP_INT",
	},
	[GPIO_TOUCH_EN] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "TOUCH_EN",
	},
	[GPIO_TOUCH_SCL] = {
		.label = "TSP_I2C_SCL_1.8V",
	},
	[GPIO_TOUCH_SDA] = {
		.label = "TSP_I2C_SDA_1.8V",
	},
};

static struct touch_key gokey_touch_keys[] = {
	{
		.name = "menu",
		.code = KEY_MENU,
	},
	{
		.name = "back",
		.code = KEY_BACK,
	},
};

static int cyttsp4_hw_reset(void)
{
	int retval = 0;

	pr_info("%s: strobe RST(%d) pin\n", __func__,
		tsp_gpios[GPIO_TOUCH_EN].gpio);
	gpio_set_value(tsp_gpios[GPIO_TOUCH_EN].gpio, 1);
	msleep(20);
	gpio_set_value(tsp_gpios[GPIO_TOUCH_EN].gpio, 0);
	msleep(40);
	gpio_set_value(tsp_gpios[GPIO_TOUCH_EN].gpio, 1);
	msleep(20);

	return retval;
}

static void tsp_set_power_gpio(bool on)
{
	if (on) {
		pr_info("%s: TSP LDO enable(%d)\n", __func__,
			tsp_gpios[GPIO_TOUCH_EN].gpio);
		gpio_set_value(tsp_gpios[GPIO_TOUCH_EN].gpio, 1);
		msleep(20);
	} else {
		pr_info("%s: TSP LDO disable(%d)\n", __func__,
			tsp_gpios[GPIO_TOUCH_EN].gpio);
		gpio_set_value(tsp_gpios[GPIO_TOUCH_EN].gpio, 0);
		msleep(40);
	}
}

static int key_led_power(bool on)
{
	struct regulator *vreg_led;

	vreg_led = regulator_get(NULL, "KEYLED_3P3V");
	if (IS_ERR(vreg_led)) {
		pr_err("tsp: Fail to register vreg_led(KEYLED_3P3V) in touch driver\n");
		return PTR_ERR(vreg_led);
	}

	if (on) {
		if (!regulator_is_enabled(vreg_led))
			regulator_enable(vreg_led);
	} else {
		if (regulator_is_enabled(vreg_led))
			regulator_disable(vreg_led);
	}

	regulator_put(vreg_led);

	return 0;
}

#define CY_WAKE_DFLT		99	/* causes wake strobe on INT line
					 * in sample board configuration
					 * platform data->hw_recov() function
					 */
static int cyttsp4_hw_recov(int on)
{
	int retval = 0;

	switch (on) {
	case 0:
		cyttsp4_hw_reset();
		retval = 0;
		break;
	case CY_WAKE_DFLT:
		retval = gpio_direction_output
			(tsp_gpios[GPIO_TOUCH_nINT].gpio, 0);
		if (retval < 0) {
			pr_err("%s: Fail switch IRQ pin to OUT r=%d\n",
				__func__, retval);
		} else {
			udelay(2000);
			retval = gpio_direction_input
				(tsp_gpios[GPIO_TOUCH_nINT].gpio);
			if (retval < 0) {
				pr_err("%s: Fail switch IRQ pin to IN"
					" r=%d\n", __func__, retval);
			}
		}
		break;
	default:
		retval = -ENOSYS;
		break;
	}

	return retval;
}

static int cyttsp4_irq_stat(void)
{
	int irq_stat = 0;

	irq_stat = gpio_get_value(tsp_gpios[GPIO_TOUCH_nINT].gpio);

	return irq_stat;
}

/* Button to keycode conversion */
static u16 cyttsp4_btn_keys[] = {
	/* use this table to map buttons to keycodes (see input.h) */
	KEY_MENU,		/* 139 */
	KEY_BACK,		/* 158 */
};

static struct touch_settings cyttsp4_sett_btn_keys = {
	.data = (uint8_t *) &cyttsp4_btn_keys[0],
	.size = ARRAY_SIZE(cyttsp4_btn_keys),
	.tag = 0,
};

struct touch_platform_data cyttsp4_i2c_touch_platform_data = {
	.sett = {
		 NULL,		/* Reserved */
		 NULL,		/* Command Registers */
		 NULL,		/* Touch Report */
		 NULL,		/* Cypress Data Record */
		 NULL,		/* Test Record */
		 NULL,		/* Panel Configuration Record */
		 NULL,		/* &cyttsp4_sett_param_regs, */
		 NULL,		/* &cyttsp4_sett_param_size, */
		 NULL,		/* Reserved */
		 NULL,		/* Reserved */
		 NULL,		/* Operational Configuration Record */
		 NULL,		/* &cyttsp4_sett_ddata, Design Data Record */
		 NULL,		/* &cyttsp4_sett_mdata, Manufact. Data Record */
		 NULL,		/* Config and Test Registers */
		 &cyttsp4_sett_btn_keys,	/* button-to-keycode table */
		 },
	.addr = {CY_I2C_TCH_ADR, CY_I2C_LDR_ADR},
	.flags = 0x00,
	.hw_reset = cyttsp4_hw_reset,
	.hw_recov = cyttsp4_hw_recov,
	.irq_stat = cyttsp4_irq_stat,
	.led_power = key_led_power,
};

static struct sec_ts_platform_data gokey_ts_pdata = {
	.model_name	= "gh1",
	.rx_channel_no	= 14,
	.tx_channel_no	= 26,
	.x_pixel_size	= 480,
	.y_pixel_size	= 800,
	.private_data	= &cyttsp4_i2c_touch_platform_data,
	.key		= gokey_touch_keys,
	.key_size	= ARRAY_SIZE(gokey_touch_keys),
	.set_power	= tsp_set_power_gpio,
};

static struct i2c_board_info __initdata gokey_i2c3_boardinfo[] = {
	{
	 I2C_BOARD_INFO("cyttsp4", CY_I2C_TCH_ADR),
	 .platform_data = &gokey_ts_pdata,
	 },
};

ssize_t sec_key_pressed_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	unsigned int key_press_status = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(gokey_gpio_keypad_keys_map_high); i++) {
		if (unlikely
		    (gokey_gpio_keypad_keys_map_high[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		    ((!gpio_get_value
		      (gokey_gpio_keypad_keys_map_high[i].gpio)
		      << i));
	}

	for (i = 0; i < ARRAY_SIZE(gokey_gpio_keypad_keys_map_low); i++) {
		if (unlikely
		    (gokey_gpio_keypad_keys_map_low[i].gpio == -EINVAL))
			continue;
		key_press_status |=
		    ((!gpio_get_value(gokey_gpio_keypad_keys_map_low[i].gpio)
		      << (i + ARRAY_SIZE(gokey_gpio_keypad_keys_map_high))));
	}

	if (key_press_status)
		sprintf(buf, "PRESSED");
	else
		sprintf(buf, "RELEASED");

	return strlen(buf);
}

static DEVICE_ATTR(sec_key_pressed, S_IRUGO, sec_key_pressed_show, NULL);

static int gokey_create_sec_key_dev(void)
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

static void __init gokey_gpio_keypad_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(keys_map_high_gpios); i++)
		gokey_gpio_keypad_keys_map_high[i].gpio =
		    omap_muxtbl_get_gpio_by_name(keys_map_high_gpios[i].label);

	for (i = 0; i < ARRAY_SIZE(keys_map_low_gpios); i++)
		gokey_gpio_keypad_keys_map_low[i].gpio =
		    omap_muxtbl_get_gpio_by_name(keys_map_low_gpios[i].label);
}

static void __init gokey_tsp_gpio_init(void)
{
	int i;
	u32 r;

	for (i = 0; i < ARRAY_SIZE(tsp_gpios); i++)
		tsp_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(tsp_gpios[i].label);
	gpio_request_array(tsp_gpios, ARRAY_SIZE(tsp_gpios));

	gokey_i2c3_boardinfo[0].irq =
		gpio_to_irq(tsp_gpios[GPIO_TOUCH_nINT].gpio);
	/* i2c3 line using external pullup */
	r = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
	r |= (1 << OMAP4_I2C3_SDA_PULLUPRESX_SHIFT);
	r |= (1 << OMAP4_I2C3_SCL_PULLUPRESX_SHIFT);
	omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
	omap_mux_set_gpio(OMAP_PIN_INPUT | OMAP_PIN_OFF_WAKEUPENABLE
			| OMAP_MUX_MODE3, tsp_gpios[GPIO_TOUCH_nINT].gpio);
}

void __init omap4_gokey_input_init(void)
{
	gokey_gpio_keypad_gpio_init();
	gokey_tsp_gpio_init();

	i2c_register_board_info(3, gokey_i2c3_boardinfo,
				ARRAY_SIZE(gokey_i2c3_boardinfo));

	gokey_create_sec_key_dev();

	if (sec_debug_get_level()) {
		gokey_gpio_keypad_keys_info_high.flags |= GPIOEDF_PRINT_KEYS;
		gokey_gpio_keypad_keys_info_low.flags |= GPIOEDF_PRINT_KEYS;
	}
	platform_device_register(&gokey_gpio_keypad_device);
}

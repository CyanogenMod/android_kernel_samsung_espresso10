/* arch/arm/mach-omap2/board-kona-serial.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/hwspinlock.h>
#include <plat/common.h>
#include <plat/omap_hwmod.h>
#include <plat/omap-serial.h>

#include "board-kona.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"

#include "control.h"


static struct i2c_board_info __initdata kona_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("ducati", 0x20),
		.irq		= OMAP44XX_IRQ_I2C2,
		.ext_master	= true,
	},
};

static void __init omap_i2c_hwspinlock_init(int bus_id, int spinlock_id,
				struct omap_i2c_bus_board_data *pdata)
{
	/* spinlock_id should be -1 for a generic lock request */
	if (spinlock_id < 0)
		pdata->handle = hwspin_lock_request();
	else
		pdata->handle = hwspin_lock_request_specific(spinlock_id);

	if (pdata->handle != NULL) {
		pdata->hwspin_lock_timeout = hwspin_lock_timeout;
		pdata->hwspin_unlock = hwspin_unlock;
	} else {
		pr_err("I2C hwspinlock request failed for bus %d\n", \
								bus_id);
	}
}

static struct omap_i2c_bus_board_data __initdata kona_i2c_1_bus_pdata;
static struct omap_i2c_bus_board_data __initdata kona_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata kona_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata kona_i2c_4_bus_pdata;

static void __init kona_i2c_init(void)
{
	u32 r;

	omap_i2c_hwspinlock_init(1, 0, &kona_i2c_1_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &kona_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &kona_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &kona_i2c_4_bus_pdata);

	omap_register_i2c_bus_board_data(1, &kona_i2c_1_bus_pdata);
	omap_register_i2c_bus_board_data(2, &kona_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &kona_i2c_3_bus_pdata);
	omap_register_i2c_bus_board_data(4, &kona_i2c_4_bus_pdata);
	/*
	 * Phoenix Audio IC needs I2C1 to
	 * start with 400 KHz or less
	 */
	omap_register_i2c_bus(1, 400, NULL, 0);
	omap_register_i2c_bus(2, 400, kona_i2c_board_info,
			      ARRAY_SIZE(kona_i2c_board_info));
	omap_register_i2c_bus(3, 400, NULL, 0);
	omap_register_i2c_bus(4, 400, NULL, 0);

	r = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
	r |= (1 << OMAP4_I2C3_SDA_PULLUPRESX_SHIFT);
	r |= (1 << OMAP4_I2C3_SCL_PULLUPRESX_SHIFT);
	omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
}

static struct i2c_gpio_platform_data kona_gpio_i2c5_pdata = {
	/* .sda_pin = (CHG_SDA_1.8V), */
	/* .scl_pin = (CHG_SCL_1.8V), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device kona_gpio_i2c5_device = {
	.name = "i2c-gpio",
	.id = 5,
	.dev = {
		.platform_data = &kona_gpio_i2c5_pdata,
	},
};

static struct i2c_gpio_platform_data kona_gpio_i2c6_pdata = {
	/* .sda_pin = (FUEL_SDA_1.8V), */
	/* .scl_pin = (FUEL_SCL_1.8V), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device kona_gpio_i2c6_device = {
	.name = "i2c-gpio",
	.id = 6,
	.dev = {
		.platform_data = &kona_gpio_i2c6_pdata,
	},
};

static struct i2c_gpio_platform_data kona_gpio_i2c7_pdata = {
	/* .sda_pin = (IMA_I2C_SDA), */
	/* .scl_pin = (IMA_I2C_SCL), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device kona_gpio_i2c7_device = {
	.name = "i2c-gpio",
	.id = 7,
	.dev = {
		.platform_data = &kona_gpio_i2c7_pdata,
	},
};

static struct i2c_gpio_platform_data kona_gpio_i2c8_pdata = {
	/* .sda_pin = (ADC_I2C_SDA_1.8V), */
	/* .scl_pin = (ADC_I2C_SCL_1.8V), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device kona_gpio_i2c8_device = {
	.name = "i2c-gpio",
	.id = 8,
	.dev = {
		.platform_data = &kona_gpio_i2c8_pdata,
	},
};

static struct i2c_gpio_platform_data kona_gpio_i2c9_pdata = {
	/* .sda_pin = (MHL_SDA_1.8V), */
	/* .scl_pin = (MHL_SCL_1.8V), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device kona_gpio_i2c9_device = {
	.name = "i2c-gpio",
	.id = 9,
	.dev = {
		.platform_data = &kona_gpio_i2c9_pdata,
	},
};

static struct i2c_gpio_platform_data kona_gpio_i2c10_pdata = {
	/* .sda_pin = (PEN_SDA_1.8V), */
	/* .scl_pin = (PEN_SCL_1.8V), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device kona_gpio_i2c10_device = {
	.name = "i2c-gpio",
	.id = 10,
	.dev = {
		.platform_data = &kona_gpio_i2c10_pdata,
	},
};

static struct i2c_gpio_platform_data kona_gpio_i2c11_pdata = {
	/* .sda_pin = (IRDA_I2C_SDA), */
	/* .scl_pin = (IRDA_I2C_SCL), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device kona_gpio_i2c11_device = {
	.name = "i2c-gpio",
	.id = 11,
	.dev = {
		.platform_data = &kona_gpio_i2c11_pdata,
	},
};

static struct i2c_gpio_platform_data kona_gpio_i2c13_pdata = {
	/* .sda_pin = (BL_I2C_SDA), */
	/* .scl_pin = (BL_I2C_SCL), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device kona_gpio_i2c13_device = {
	.name = "i2c-gpio",
	.id = 13,
	.dev = {
		.platform_data = &kona_gpio_i2c13_pdata,
	},
};

#define KONA_SET_GPIO_4_I2C(_num, _sda, _scl)				\
do {									\
	kona_gpio_i2c##_num##_pdata.sda_pin =				\
		omap_muxtbl_get_gpio_by_name(_sda);			\
	kona_gpio_i2c##_num##_pdata.scl_pin =				\
		omap_muxtbl_get_gpio_by_name(_scl);			\
} while (0)

static void __init kona_gpio_i2c_init(void)
{
	/* gpio-i2c 5 */
	KONA_SET_GPIO_4_I2C(5, "CHG_SDA_1.8V", "CHG_SCL_1.8V");

	/* gpio-i2c 6 */
	KONA_SET_GPIO_4_I2C(6, "FUEL_SDA_1.8V", "FUEL_SCL_1.8V");

	/* gpio-i2c 7 */
	KONA_SET_GPIO_4_I2C(7, "IMA_I2C_SDA", "IMA_I2C_SCL");

	/* gpio-i2c 8 */
	KONA_SET_GPIO_4_I2C(8, "ADC_I2C_SDA_1.8V", "ADC_I2C_SCL_1.8V");

	/* gpio-i2c 9 */
	KONA_SET_GPIO_4_I2C(9, "MHL_SDA_1.8V", "MHL_SCL_1.8V");

	/* gpio-i2c 10 */
	KONA_SET_GPIO_4_I2C(10, "PEN_SDA_1.8V", "PEN_SCL_1.8V");

	/* gpio-i2c 11 */
	KONA_SET_GPIO_4_I2C(11, "IRDA_I2C_SDA", "IRDA_I2C_SCL");

	/* gpio-i2c 13 */
	KONA_SET_GPIO_4_I2C(13, "BL_I2C_SDA", "BL_I2C_SCL");
}

enum {
	GPIO_GPS_PWR_EN = 0,
};

static void kona_bcmgps_init(void)
{
	struct device *gps_dev;
	struct gpio gps_gpios[] = {
		[GPIO_GPS_PWR_EN] = {
			.flags = GPIOF_OUT_INIT_LOW,
			.label = "GPS_PWR_EN",
		},
	};
	int i;

	gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
	if (IS_ERR(gps_dev)) {
		pr_err("(%s): failed to created device (gps)!\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(gps_gpios); i++)
		gps_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(gps_gpios[i].label);

	gpio_request_array(gps_gpios, ARRAY_SIZE(gps_gpios));

	gpio_export(gps_gpios[GPIO_GPS_PWR_EN].gpio, 1);

	gpio_export_link(gps_dev, gps_gpios[GPIO_GPS_PWR_EN].label,
			 gps_gpios[GPIO_GPS_PWR_EN].gpio);
}

static struct omap_device_pad kona_uart1_pads[] __initdata = {
	{
		.name	= "mcspi1_cs3.uart1_rts",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE1,
	},
	{
		.name	= "mcspi1_cs2.uart1_cts",
		.enable	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE1,
	},
	{
		.name   = "uart3_cts_rctx.uart1_tx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE1,
	},
	{
		.name   = "mcspi1_cs1.uart1_rx",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE1,
		.idle   = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE1,
	},
};

static struct omap_device_pad kona_uart2_pads[] __initdata = {
	{
		.name	= "uart2_cts.uart2_cts",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart2_rts.uart2_rts",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart2_tx.uart2_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart2_rx.uart2_rx",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad kona_uart3_pads[] __initdata = {
	{
		.name   = "uart3_tx_irtx.uart3_tx_irtx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart3_rx_irrx.uart3_rx_irrx",
		.enable = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad kona_uart4_pads[] __initdata = {
	{
		.name	= "uart4_tx.uart4_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart4_rx.uart4_rx",
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_uart_port_info kona_uart2_info __initdata = {
	.use_dma		= 0,
	.dma_rx_buf_size	= DEFAULT_RXDMA_BUFSIZE,
	.dma_rx_poll_rate	= DEFAULT_RXDMA_POLLRATE,
	.dma_rx_timeout		= DEFAULT_RXDMA_TIMEOUT,
	.auto_sus_timeout	= 0,
	.wake_peer		= bcm_bt_lpm_exit_lpm_locked,
	.rts_mux_driver_control	= 1,
};

static void __init kona_uart_init(void)
{
	omap_serial_init_port_pads(0, kona_uart1_pads,
				   ARRAY_SIZE(kona_uart1_pads), NULL);
	omap_serial_init_port_pads(1, kona_uart2_pads,
				   ARRAY_SIZE(kona_uart2_pads),
				   &kona_uart2_info);
	omap_serial_init_port_pads(2, kona_uart3_pads,
				   ARRAY_SIZE(kona_uart3_pads), NULL);
	omap_serial_init_port_pads(3, kona_uart4_pads,
				   ARRAY_SIZE(kona_uart4_pads), NULL);
	kona_bcmgps_init();
}

static struct platform_device *kona_serial_devices[] __initdata = {
	&kona_gpio_i2c5_device,
	&kona_gpio_i2c6_device,
	&kona_gpio_i2c7_device,
	&kona_gpio_i2c8_device,
	&kona_gpio_i2c9_device,
	&kona_gpio_i2c10_device,
	&kona_gpio_i2c11_device,
	&kona_gpio_i2c13_device,
};

void __init omap4_kona_serial_init(void)
{
	size_t nr_gpio_i2c = ARRAY_SIZE(kona_serial_devices);

	kona_i2c_init();
	kona_gpio_i2c_init();
	kona_uart_init();

	platform_add_devices(kona_serial_devices, nr_gpio_i2c);
}

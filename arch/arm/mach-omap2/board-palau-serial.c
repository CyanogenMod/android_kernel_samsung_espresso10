/* arch/arm/mach-omap2/board-palau-serial.c
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

#include "board-palau.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"

static struct i2c_board_info __initdata palau_i2c_board_info[] = {
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

static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_1_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_4_bus_pdata;


static void __init palau_i2c_init(void)
{

	omap_i2c_hwspinlock_init(1, 0, &sdp4430_i2c_1_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &sdp4430_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &sdp4430_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &sdp4430_i2c_4_bus_pdata);

	omap_register_i2c_bus_board_data(1, &sdp4430_i2c_1_bus_pdata);
	omap_register_i2c_bus_board_data(2, &sdp4430_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &sdp4430_i2c_3_bus_pdata);
	omap_register_i2c_bus_board_data(4, &sdp4430_i2c_4_bus_pdata);
	/*
	 * Phoenix Audio IC needs I2C1 to
	 * start with 400 KHz or less
	 */
	omap_register_i2c_bus(1, 400, NULL, 0);
	omap_register_i2c_bus(2, 400, palau_i2c_board_info,
			      ARRAY_SIZE(palau_i2c_board_info));
	omap_register_i2c_bus(3, 400, NULL, 0);
	omap_register_i2c_bus(4, 400, NULL, 0);
}

static struct i2c_gpio_platform_data palau_gpio_i2c5_pdata = {
	/* .sda_pin = (IF_PMIC_SDA), */
	/* .scl_pin = (IF_PMIC_SCL), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device palau_gpio_i2c5_device = {
	.name = "i2c-gpio",
	.id = 5,
	.dev = {
		.platform_data = &palau_gpio_i2c5_pdata,
	},
};

static struct i2c_gpio_platform_data palau_gpio_i2c6_pdata = {
	/* .sda_pin = (FUEL_I2C_SCL), */
	/* .scl_pin = (FUEL_I2C_SDA), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device palau_gpio_i2c6_device = {
	.name = "i2c-gpio",
	.id = 6,
	.dev = {
		.platform_data = &palau_gpio_i2c6_pdata,
	},
};

static struct i2c_gpio_platform_data palau_gpio_i2c7_pdata = {
	/* .sda_pin = (IMA_I2C_SDA), */
	/* .scl_pin = (IMA_I2C_SCL), */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device palau_gpio_i2c7_device = {
	.name = "i2c-gpio",
	.id = 7,
	.dev = {
		.platform_data = &palau_gpio_i2c7_pdata,
	},
};

static void __init palau_gpio_i2c_init(void)
{
	/* gpio-i2c 5 */
	palau_gpio_i2c5_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("IF_PMIC_SDA");
	palau_gpio_i2c5_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("IF_PMIC_SCL");

	/* Emul00 only has gpio-i2c 5 */
	if (unlikely(system_rev < 1))
		return;

	/* gpio-i2c 6 */
	palau_gpio_i2c6_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("FUEL_I2C_SDA");
	palau_gpio_i2c6_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("FUEL_I2C_SCL");

	/* gpio-i2c 7 */
	palau_gpio_i2c7_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("IMA_I2C_SDA");
	palau_gpio_i2c7_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("IMA_I2C_SCL");
}

enum {
	GPIO_GPS_PWR_EN = 0,
};

static void palau_bcmgps_init(void)
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

static struct omap_device_pad palau_uart1_pads[] __initdata = {
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

static struct omap_device_pad palau_uart2_pads[] __initdata = {
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

static struct omap_device_pad palau_uart3_pads[] __initdata = {
	{
		.name   = "uart3_tx_irtx.uart3_tx_irtx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart3_rx_irrx.uart3_rx_irrx",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle   = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad palau_uart4_pads[] __initdata = {
	{
		.name	= "abe_dmic_clk1.uart4_cts",
		.enable	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE5,
	},
	{
		.name	= "abe_dmic_din1.uart4_rts",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE5,
	},
	{
		.name	= "uart4_tx.uart4_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart4_rx.uart4_rx",
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad palau_uart4_pads_r00[] __initdata = {
	{
		.name	= "uart4_tx.uart4_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart4_rx.uart4_rx",
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_uart_port_info palau_uart2_info __initdata = {
	.use_dma		= 0,
	.dma_rx_buf_size	= DEFAULT_RXDMA_BUFSIZE,
	.dma_rx_poll_rate	= DEFAULT_RXDMA_POLLRATE,
	.dma_rx_timeout		= DEFAULT_RXDMA_TIMEOUT,
	.auto_sus_timeout	= 0,
	.wake_peer		= bcm_bt_lpm_exit_lpm_locked,
	.rts_mux_driver_control	= 1,
};

static void __init palau_uart_init(void)
{
	omap_serial_init_port_pads(0, palau_uart1_pads,
				   ARRAY_SIZE(palau_uart1_pads), NULL);
	omap_serial_init_port_pads(1, palau_uart2_pads,
				   ARRAY_SIZE(palau_uart2_pads),
				   &palau_uart2_info);
	omap_serial_init_port_pads(2, palau_uart3_pads,
				   ARRAY_SIZE(palau_uart3_pads), NULL);
	if (likely(system_rev >= 2))
		omap_serial_init_port_pads(3, palau_uart4_pads,
				   ARRAY_SIZE(palau_uart4_pads), NULL);
	else
		omap_serial_init_port_pads(3, palau_uart4_pads_r00,
				   ARRAY_SIZE(palau_uart4_pads_r00), NULL);

	palau_bcmgps_init();
}

static struct platform_device *palau_serial_devices[] __initdata = {
	&palau_gpio_i2c5_device,
	/* rev01 or higher */
	&palau_gpio_i2c6_device,
	&palau_gpio_i2c7_device,
};

void __init omap4_palau_serial_init(void)
{
	size_t nr_serial_dev;

	palau_i2c_init();
	palau_gpio_i2c_init();
	palau_uart_init();

	if (likely(system_rev >= 1))
		nr_serial_dev = ARRAY_SIZE(palau_serial_devices);
	else
		nr_serial_dev = 1;

	platform_add_devices(palau_serial_devices, nr_serial_dev);
}

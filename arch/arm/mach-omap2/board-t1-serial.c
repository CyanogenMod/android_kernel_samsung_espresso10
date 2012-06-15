/* arch/arm/mach-omap2/board-t1-serial.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
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

#include <plat/common.h>
#include <plat/omap_hwmod.h>
#include <plat/omap-serial.h>

#include "board-t1.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap44xx_muxtbl.h"
#include "control.h"

#define LOAD_860_OHM	0x2
#define PULLUP_ENABLE(r, ln, num) (r &= ~OMAP4_##num##_##ln##_PULLUPRESX_MASK)
#define PULLUP_DISABLE(r, ln, num) (r |= OMAP4_##num##_##ln##_PULLUPRESX_MASK)
#define SET_LOAD(r, ln, num, load)				  \
do {								  \
	r &= ~OMAP4_##num##_##ln##_LOAD_BITS_MASK;		  \
	r |= load << OMAP4_##num##_##ln##_LOAD_BITS_SHIFT;        \
} while (0)

static struct i2c_board_info __initdata t1_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("ducati", 0x20),
		.irq		= OMAP44XX_IRQ_I2C2,
		.ext_master	= true,
	},
};

static void __init t1_i2c_init(void)
{
	u32 reg_val;
	/* Disable internal pull ups for i2c 1 and 2
				and enable for i2c 3 and 4 */
	reg_val = omap4_ctrl_pad_readl(
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);
	PULLUP_DISABLE(reg_val, SCL, I2C1);
	PULLUP_DISABLE(reg_val, SDA, I2C1);
	PULLUP_DISABLE(reg_val, SCL, I2C2);
	PULLUP_DISABLE(reg_val, SDA, I2C2);
	PULLUP_ENABLE(reg_val, SCL, I2C3);
	PULLUP_ENABLE(reg_val, SDA, I2C3);
	PULLUP_ENABLE(reg_val, SCL, I2C4);
	PULLUP_ENABLE(reg_val, SDA, I2C4);
	SET_LOAD(reg_val, SCL, I2C3, LOAD_860_OHM);
	SET_LOAD(reg_val, SDA, I2C3, LOAD_860_OHM);
	SET_LOAD(reg_val, SCL, I2C4, LOAD_860_OHM);
	SET_LOAD(reg_val, SDA, I2C4, LOAD_860_OHM);
	omap4_ctrl_pad_writel(reg_val,
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_0);

	/* 860 k, SR Enable Internal Pull up */
	reg_val = omap4_ctrl_wk_pad_readl(
			OMAP4_CTRL_MODULE_PAD_WKUP_CONTROL_I2C_2);
	PULLUP_ENABLE(reg_val, SCL, SR);
	PULLUP_ENABLE(reg_val, SDA, SR);
	SET_LOAD(reg_val, SCL, SR, LOAD_860_OHM);
	SET_LOAD(reg_val, SDA, SR, LOAD_860_OHM);
	omap4_ctrl_wk_pad_writel(reg_val,
			OMAP4_CTRL_MODULE_PAD_WKUP_CONTROL_I2C_2);
	/*
	 * Phoenix Audio IC needs I2C1 to
	 * start with 400 KHz or less
	 */
	omap_register_i2c_bus(1, 400, NULL, 0);
	omap_register_i2c_bus(2, 400, t1_i2c_board_info,
			      ARRAY_SIZE(t1_i2c_board_info));
	omap_register_i2c_bus(3, 400, NULL, 0);
	omap_register_i2c_bus(4, 400, NULL, 0);
}

static struct i2c_gpio_platform_data t1_gpio_i2c5_pdata = {
	/*.sda_pin	= (MHL_SDA_1.8V),*/
	/*.scl_pin	= (MHL_SCL_1.8V),*/
	.udelay		= 3,
	.timeout	= 0,
};

static struct platform_device t1_gpio_i2c5_device = {
	.name		= "i2c-gpio",
	.id		= 5,
	.dev = {
		.platform_data = &t1_gpio_i2c5_pdata,
	},
};

static struct i2c_gpio_platform_data t1_gpio_i2c6_pdata = {
	/* .sda_pin = OMAP_GPIO_3_TOUCH_SDA, */
	/* .scl_pin = OMAP_GPIO_3_TOUCH_SCL, */
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device t1_gpio_i2c6_device = {
	.name = "i2c-gpio",
	.id = 6,
	.dev = {
		.platform_data = &t1_gpio_i2c6_pdata,
	}
};
static struct i2c_gpio_platform_data t1_gpio_i2c7_pdata = {
	/*.sda_pin	= (FUEL_I2C_SDA),*/
	/*.scl_pin	= (FUEL_I2C_SCL),*/
	.udelay		= 3,
	.timeout	= 0,
};

static struct platform_device t1_gpio_i2c7_device = {
	.name		= "i2c-gpio",
	.id		= 7,
	.dev = {
		.platform_data = &t1_gpio_i2c7_pdata,
	},
};

static void __init t1_gpio_i2c_init(void)
{
	/* gpio-i2c 5 */
	t1_gpio_i2c5_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("MHL_SDA_1.8V");
	t1_gpio_i2c5_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("MHL_SCL_1.8V");

	/* gpio-i2c 5 */
	t1_gpio_i2c6_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("3-TOUCHKEY_SDA");
	t1_gpio_i2c6_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("3-TOUCHKEY_SCL");

	/* gpio-i2c 7 */
	t1_gpio_i2c7_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("FUEL_I2C_SDA");
	t1_gpio_i2c7_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("FUEL_I2C_SCL");
}

enum {
	GPIO_AP_AGPS_TSYNC = 0,
	GPIO_GPS_PWR_EN,
	GPIO_GPS_nRST
};

static void t1_gsd4t_init(void)
{
	struct device *gps_dev;
	struct gpio gps_gpios[] = {
		[GPIO_AP_AGPS_TSYNC] = {
			.flags = GPIOF_OUT_INIT_LOW,
			.label = "AP_AGPS_TSYNC",
		},
		[GPIO_GPS_PWR_EN] = {
			.flags = GPIOF_OUT_INIT_LOW,
			.label = "GPS_PWR_EN",
		},
		[GPIO_GPS_nRST] = {
			.flags = GPIOF_OUT_INIT_HIGH,
			.label = "GPS_nRST",
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
	gpio_export(gps_gpios[GPIO_GPS_nRST].gpio, 1);

	gpio_export_link(gps_dev, gps_gpios[GPIO_GPS_PWR_EN].label,
			 gps_gpios[GPIO_GPS_PWR_EN].gpio);
	gpio_export_link(gps_dev, gps_gpios[GPIO_GPS_nRST].label,
			 gps_gpios[GPIO_GPS_nRST].gpio);
}

static struct omap_device_pad t1_uart1_pads[] __initdata = {
	{
		.name	= "uart3_cts_rctx.uart1_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE1,
	},
	{
		.name	= "mcspi1_cs1.uart1_rx",
		.flags	= OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE1,
		.idle	= OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE1,
	},
};

static struct omap_device_pad t1_uart2_pads[] __initdata = {
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

static struct omap_device_pad t1_uart3_pads[] __initdata = {
	{
		.name	= "uart3_tx_irtx.uart3_tx_irtx",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name   = "uart3_rx_irrx.uart3_rx_irrx",
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static struct omap_device_pad t1_uart4_pads[] __initdata = {
	{
		.name	= "uart4_tx.uart4_tx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart4_rx.uart4_rx",
		.flags	= OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle	= OMAP_PIN_INPUT | OMAP_MUX_MODE7,
	},
};

static struct omap_uart_port_info t1_uart2_info __initdata = {
	.use_dma		= 0,
	.dma_rx_buf_size	= DEFAULT_RXDMA_BUFSIZE,
	.dma_rx_poll_rate	= DEFAULT_RXDMA_POLLRATE,
	.dma_rx_timeout		= DEFAULT_RXDMA_TIMEOUT,
	.auto_sus_timeout	= 0,
	.wake_peer		= bcm_bt_lpm_exit_lpm_locked,
	.rts_mux_driver_control	= 1,
};

static void __init omap_serial_none_pads_cfg_mux(void)
{
	int i;
	struct omap_mux_partition *partition;
	struct omap_mux_partition *core = omap_mux_get("core");
	struct omap_mux_partition *wkup = omap_mux_get("wkup");
	struct omap_muxtbl *tbl;
	char *none_pins[] = {
		"AP_FLM_RXD(nc)",
		"AP_FLM_TXD(nc)",
	};

	for (i = 0; i < ARRAY_SIZE(none_pins); i++) {
		tbl = omap_muxtbl_find_by_name(none_pins[i]);
		if (!tbl)
			continue;
		if (tbl->domain == OMAP4_MUXTBL_DOMAIN_WKUP)
			partition = wkup;
		else
			partition = core;

		omap_mux_write(partition,
			OMAP_MUX_MODE7 | OMAP_PIN_INPUT_PULLDOWN,
			tbl->mux.reg_offset);
	}
}

static void __init t1_uart_init(void)
{

	omap_serial_init_port_pads(0, t1_uart1_pads,
			ARRAY_SIZE(t1_uart1_pads), NULL);
	omap_serial_init_port_pads(1, t1_uart2_pads,
			ARRAY_SIZE(t1_uart2_pads), &t1_uart2_info);
	omap_serial_init_port_pads(2, t1_uart3_pads,
			ARRAY_SIZE(t1_uart3_pads), NULL);
	omap_serial_init_port_pads(3, t1_uart4_pads,
			ARRAY_SIZE(t1_uart4_pads), NULL);

	t1_gsd4t_init();
}

void __init omap4_t1_serial_early_init(void)
{
	struct omap_hwmod *uart3_hwmod;
	struct omap_hwmod *uart4_hwmod;

	/* correct uart3 hwmod flag settings for t1 board. */
	uart3_hwmod = omap_hwmod_lookup("uart3");
	if (likely(uart3_hwmod))
		uart3_hwmod->flags = 0;

	/* correct uart4 hwmod flag settings for t1 board. */
	uart4_hwmod = omap_hwmod_lookup("uart4");
	if (likely(uart4_hwmod))
		uart4_hwmod->flags = 0;

}

static struct platform_device *t1_serial_devices[] __initdata = {
	&t1_gpio_i2c5_device,
	&t1_gpio_i2c6_device,
	&t1_gpio_i2c7_device,
};

void __init omap4_t1_serial_init(void)
{
	t1_i2c_init();
	t1_gpio_i2c_init();
	t1_uart_init();

	platform_add_devices(t1_serial_devices,
			     ARRAY_SIZE(t1_serial_devices));
}

int __init omap4_t1_serial_late_init(void)
{
#if !defined(CONFIG_MACH_SAMSUNG_T1_CHN_CMCC)
	/* Not USE AP_FLM_TX/RX in T1_ICS */
	omap_serial_none_pads_cfg_mux();
#endif
	return 0;
}

late_initcall(omap4_t1_serial_late_init);

/* arch/arm/mach-omap2/board-espresso10-connector.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-espresso-connector.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/platform_data/fsa9480.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/otg.h>
#include <linux/delay.h>
#include <linux/sii9234.h>
#include <linux/i2c/twl.h>
#include <linux/mutex.h>
#include <linux/switch.h>
#include <linux/30pin_con.h>
#include <linux/mfd/stmpe811.h>
#include <linux/sec_dock_keyboard.h>
#include <linux/battery.h>
#include <linux/irq.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif

#include <plat/usb.h>

#include "board-espresso10.h"
#include "mux.h"
#include "omap_muxtbl.h"

#include "omap_phy_tune.c"

#define CHARGERUSB_CTRL1		0x8
#define CHARGERUSB_CTRL3		0xA
#define CHARGERUSB_CINLIMIT		0xE

#define ESPRESSO10_MANUAL_USB_NONE	0
#define ESPRESSO10_MANUAL_USB_MODEM	1
#define ESPRESSO10_MANUAL_USB_AP		2

#define ESPRESSO10_MANUAL_UART_NONE	0
#define ESPRESSO10_MANUAL_UART_MODEM	1
#define ESPRESSO10_MANUAL_UART_AP		2

#define IF_UART_SEL_CP		0
#define IF_UART_SEL_AP		1

#define ADC_CHANNEL_IN0		4
#define ADC_CHANNEL_IN1		5
#define ADC_CHANNEL_IN2		6
#define ADC_CHANNEL_IN3		7

#define MAX_ADC_VAL     4096
#define MIN_ADC_VAL     0

#define MASK_SWITCH_USB_AP	0x01
#define MASK_SWITCH_UART_AP	0x02

#define SWCAP_TRIM_OFFSET			0x31

static char *device_names[] = {
	[P30_OTG]			= "otg",
	[P30_EARJACK_WITH_DOCK]		= "earjack",
	[P30_CARDOCK]			= "car-dock",
	[P30_ANAL_TV_OUT]		= "TV-Outline",
	[P30_KEYBOARDDOCK]		= "keboard-dock",
	[P30_DESKDOCK]			= "desk-dock",
	[P30_JIG]			= "jig",
	[P30_USB]			= "USB",
	[P30_TA]			= "TA",
};

struct omap4_otg {
	struct otg_transceiver otg;
	struct device dev;

	struct regulator *vusb;
	struct work_struct set_vbus_work;
	struct mutex lock;

	bool reg_on;
	bool need_vbus_drive;
	int usb_manual_mode;
	int uart_manual_mode;
	int current_device;
	int ta_nconnected;

	struct switch_dev dock_switch;
	struct switch_dev audio_switch;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct host_notifier_platform_data *pdata;
#endif
};

static struct omap4_otg espresso10_otg_xceiv;

static struct device *sec_switch_dev;
static int init_switch_sel;

enum {
	UEVENT_DOCK_NONE = 0,
	UEVENT_DOCK_DESK,
	UEVENT_DOCK_CAR,
	UEVENT_DOCK_KEYBOARD = 9,
};

enum {
	 UEVENT_EARJACK_DETACHED = 0,
	 UEVENT_EARJACK_ATTACHED,
};

enum {
	GPIO_ACCESSORY_EN = 0,
	GPIO_ACCESSORY_INT,
	GPIO_DOCK_INT,
	GPIO_JIG_ON,
};

static struct gpio connector_gpios[] = {
	[GPIO_ACCESSORY_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label = "ACCESSORY_EN",
	},
	[GPIO_ACCESSORY_INT] = {
		.flags = GPIOF_IN,
		.label = "ACCESSORY_INT_1.8V",
	},
	[GPIO_DOCK_INT] = {
		.flags = GPIOF_IN,
		.label = "DOCK_INT",
	},
	[GPIO_JIG_ON] = {
		.flags = GPIOF_IN,
		.label = "JIG_ON_18",
	},
};

enum {
	GPIO_USB_SEL1 = 0,
	GPIO_USB_SEL2,
	GPIO_UART_SEL
};

static struct gpio uart_sw_gpios[] = {
	[GPIO_USB_SEL1] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "USB_SEL1",
	},
	[GPIO_USB_SEL2] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "USB_SEL2",
	},
	[GPIO_UART_SEL] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "UART_SEL",
	}
};

enum {
	GPIO_MHL_RST = 0,
	GPIO_MHL_INT,
	GPIO_HDMI_EN,
	GPIO_HDMI_HPD,
};

static struct gpio mhl_gpios[] = {
	[GPIO_MHL_RST] = {
		.flags  = GPIOF_OUT_INIT_LOW,
		.label  = "MHL_RST",
	},
	[GPIO_MHL_INT] = {
		.flags = GPIOF_IN,
		.label  = "MHL_INT",
	},
	[GPIO_HDMI_EN] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "HDMI_EN",
	},
	[GPIO_HDMI_HPD] = {
		.flags = GPIOF_IN,
		.label  = "HDMI_HPD",
	},
};

static BLOCKING_NOTIFIER_HEAD(acc_notifier);

int acc_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&acc_notifier, nb);
}

int acc_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&acc_notifier, nb);
}

static int acc_notify(int event)
{
	return blocking_notifier_call_chain(&acc_notifier, event, NULL);
}

/* STMPE811 */
static struct i2c_board_info __initdata espresso10_i2c6_boardinfo[] = {
	{
		I2C_BOARD_INFO("stmpe811", 0x82>>1),
	},
};

static ssize_t espresso10_usb_sel_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf);
static ssize_t espresso10_usb_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size);

static ssize_t espresso10_uart_sel_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf);
static ssize_t espresso10_uart_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size);

static ssize_t espresso10_usb_state_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t espresso10_jig_on_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t espresso10_adc_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static DEVICE_ATTR(usb_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
		espresso10_usb_sel_show, espresso10_usb_sel_store);
static DEVICE_ATTR(uart_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
		espresso10_uart_sel_show, espresso10_uart_sel_store);
static DEVICE_ATTR(usb_state, S_IRUGO, espresso10_usb_state_show, NULL);
static DEVICE_ATTR(jig_on, S_IRUSR | S_IWUSR, espresso10_jig_on_show, NULL);
static DEVICE_ATTR(adc, S_IRUSR | S_IRGRP, espresso10_adc_show, NULL);

static struct attribute *manual_switch_sel_attributes[] = {
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_jig_on.attr,
	&dev_attr_adc.attr,
	NULL,
};

static const struct attribute_group manual_switch_sel_group = {
	.attrs = manual_switch_sel_attributes,
};

static void espresso10_set_dock_switch(int state)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	switch_set_state(&espresso10_otg->dock_switch, state);
}

static void espresso10_set_audio_switch(int state)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	switch_set_state(&espresso10_otg->audio_switch, state);
}

static void omap4_vusb_enable(struct omap4_otg *otg, bool enable)
{
	/* delay getting the regulator until later */
	if (IS_ERR_OR_NULL(otg->vusb)) {
		otg->vusb = regulator_get(&otg->dev, "vusb");
		if (IS_ERR(otg->vusb)) {
			dev_err(&otg->dev, "cannot get vusb regulator\n");
			return;
		}
	}

	if (enable) {
		regulator_enable(otg->vusb);
		otg->reg_on = true;
	} else if (otg->reg_on) {
		regulator_disable(otg->vusb);
		otg->reg_on = false;
	}
}

static void espresso_accessory_power(u32 device, bool enable)
{
	int gpio_acc_en = connector_gpios[GPIO_ACCESSORY_EN].gpio;
	static u32 acc_device;

	/*
		token info
		0 : power off,
		1 : Keyboard dock
		2 : USB
	*/

	pr_info("accessory_power: acc_device 0x%x, new %d : %s\n",
			acc_device, device, enable ? "ON" : "OFF");

	if (enable) {
		acc_device |= (1 << device);
		gpio_set_value(gpio_acc_en, 1);

	} else {

		if (device == 0) {
			pr_info("accessory_power: force turn off\n");
			gpio_set_value(gpio_acc_en, 0);

		} else {
			acc_device &= ~(1 << device);
			if (acc_device == 0) {
				pr_info("accessory_power: turn off\n");
				gpio_set_value(gpio_acc_en, 0);
			} else
				pr_info("accessory_power: skip\n");
		}
	}
}

#ifdef USB_TWL6030_5VOUT
static void espresso10_set_vbus_drive(bool enable)
{
	if (enable) {
		/* Set the VBUS current limit to 500mA */
		twl_i2c_write_u8(TWL_MODULE_MAIN_CHARGE, 0x09,
				 CHARGERUSB_CINLIMIT);

		/* The TWL6030 has a feature to automatically turn on
		 * boost mode (VBUS Drive) when the ID signal is not
		 * grounded.  This feature needs to be disabled on Tuna
		 * as the ID signal is not hooked up to the TWL6030.
		 */
		twl_i2c_write_u8(TWL_MODULE_MAIN_CHARGE, 0x21,
				 CHARGERUSB_CTRL3);
		twl_i2c_write_u8(TWL_MODULE_MAIN_CHARGE, 0x40,
				 CHARGERUSB_CTRL1);
	} else {
		twl_i2c_write_u8(TWL_MODULE_MAIN_CHARGE, 0x01,
				 CHARGERUSB_CTRL3);
		twl_i2c_write_u8(TWL_MODULE_MAIN_CHARGE, 0x00,
				 CHARGERUSB_CTRL1);
	}
}
#else
static void espresso10_set_vbus_drive(bool enable)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;
	if (espresso10_otg->pdata->usbhostd_wakeup && enable)
		espresso10_otg->pdata->usbhostd_wakeup();
	espresso_accessory_power(2, enable);
}
#endif

static void espresso10_ap_usb_attach(struct omap4_otg *otg)
{
	omap4_vusb_enable(otg, true);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_VBUS_CHARGER;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_VBUS_CHARGER,
			otg->otg.gadget);
}

static void espresso10_ap_usb_detach(struct omap4_otg *otg)
{
	omap4_vusb_enable(otg, false);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;

	if (otg->otg.last_event != USB_EVENT_VBUS_CHARGER) {
		otg->otg.last_event = USB_EVENT_NONE;
		atomic_notifier_call_chain(&otg->otg.notifier,
				USB_EVENT_NONE,
				otg->otg.gadget);
	} else {
		otg->otg.last_event = USB_EVENT_NONE;
		pr_info("VBUS OFF before detecting the cable type\n");
	}

	atomic_notifier_call_chain(&otg->otg.notifier,
				USB_EVENT_CHARGER_NONE,
				otg->otg.gadget);
}

static void espresso10_usb_host_attach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);
	/* Accessory power down needed */
	espresso_accessory_power(0, 0);

#ifdef CONFIG_USB_HOST_NOTIFY
	if (otg->pdata && otg->pdata->usbhostd_start) {
		otg->pdata->ndev.mode = NOTIFY_HOST_MODE;
		otg->pdata->usbhostd_start();
	}
#endif

	omap4_vusb_enable(otg, true);

	otg->otg.state = OTG_STATE_A_IDLE;
	otg->otg.default_a = true;
	otg->otg.last_event = USB_EVENT_ID;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_ID,
			otg->otg.gadget);
}

static void espresso10_usb_host_detach(struct omap4_otg *otg)
{
#ifdef CONFIG_USB_HOST_NOTIFY
	if (otg->pdata && otg->pdata->usbhostd_stop) {
		otg->pdata->ndev.mode = NOTIFY_NONE_MODE;
		otg->pdata->usbhostd_stop();
	}
#endif

	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.default_a = false;
	otg->otg.last_event = USB_EVENT_NONE;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_HOST_NONE,
			otg->otg.gadget);

	espresso10_set_vbus_drive(false);
	omap4_vusb_enable(otg, false);
}

static void espresso10_cp_usb_attach(void)
{
	gpio_set_value(uart_sw_gpios[GPIO_USB_SEL1].gpio, 0);
	gpio_set_value(uart_sw_gpios[GPIO_USB_SEL2].gpio, 0);

	sysfs_notify(&sec_switch_dev->kobj, NULL, "usb_sel");
}

static void espresso10_cp_usb_detach(void)
{
	gpio_set_value(uart_sw_gpios[GPIO_USB_SEL1].gpio, 1);
	gpio_set_value(uart_sw_gpios[GPIO_USB_SEL2].gpio, 0);

	sysfs_notify(&sec_switch_dev->kobj, NULL, "usb_sel");
}

static void espresso10_ap_uart_actions(void)
{
	gpio_set_value(uart_sw_gpios[GPIO_UART_SEL].gpio, IF_UART_SEL_AP);
}

static void espresso10_cp_uart_actions(void)
{
	gpio_set_value(uart_sw_gpios[GPIO_UART_SEL].gpio, IF_UART_SEL_CP);
}

static void espresso10_gpio_set_for_adc_check_1(void)
{
	gpio_set_value(uart_sw_gpios[GPIO_USB_SEL1].gpio, 0);
	gpio_set_value(uart_sw_gpios[GPIO_USB_SEL2].gpio, 1);
}

static void espresso10_gpio_rel_for_adc_check_1(void)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	if (espresso10_otg->usb_manual_mode == ESPRESSO10_MANUAL_USB_MODEM)
		espresso10_cp_usb_attach();
	else
		espresso10_cp_usb_detach();
}

int omap4_espresso10_get_adc(enum espresso10_adc_ch ch)
{
	int adc;
	int i;
	int adc_tmp;
	int adc_min = MAX_ADC_VAL;
	int adc_max = MIN_ADC_VAL;
	int adc_sum = 0;
	u8 stmpe811_ch = ADC_CHANNEL_IN2;

	if (ch == REMOTE_SENSE)
		stmpe811_ch = ADC_CHANNEL_IN1;
	else if (ch == ADC_CHECK_1)
		stmpe811_ch = ADC_CHANNEL_IN2;
	else if (ch == ACCESSORY_ID)
		stmpe811_ch = ADC_CHANNEL_IN3;
	else if (ch == EAR_ADC_35)
		stmpe811_ch = ADC_CHANNEL_IN0;

	if (ch == ADC_CHECK_1) {
		/* HQRL Standard defines that time margin from Vbus5V detection
		 * to ADC_CHECK_1 voltage up should be more than 400ms.
		 */
		msleep(400);	/* delay for slow increase of line voltage */

		espresso10_gpio_set_for_adc_check_1();

		msleep(150);    /* delay for slow increase of line voltage */

		for (i = 0; i < 5; i++) {
			usleep_range(5000, 5500);
			adc_tmp = stmpe811_adc_get_value(stmpe811_ch);
			pr_info("adc_check_1 adc=%d\n", adc_tmp);
			adc_sum += adc_tmp;
			if (adc_max < adc_tmp)
				adc_max = adc_tmp;

			if (adc_min > adc_tmp)
				adc_min = adc_tmp;
		}
		espresso10_gpio_rel_for_adc_check_1();
		adc = (adc_sum - adc_max - adc_min) / 3;
	} else
		adc = stmpe811_adc_get_value(stmpe811_ch);

	return adc;
}

static void espresso10_con_usb_charger_attached(struct omap4_otg *otg)
{
	int val;

	/* USB cable connected */
	pr_info("%s, USB_EVENT_VBUS\n", __func__);

	val = gpio_get_value(otg->ta_nconnected);
	if (val < 0) {
		pr_err("usb ta_nconnected: gpio_get_value error %d\n", val);
		return;
	}

	if (!val) { /* connected */
		otg->otg.default_a = false;
		otg->otg.state = OTG_STATE_B_IDLE;
		otg->otg.last_event = USB_EVENT_VBUS;

		atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_VBUS, otg->otg.gadget);
	} else {  /* disconnected */
		pr_info("%s, VBUS OFF : USB_EVENT_VBUS is not sent\n",
						__func__);
	}
}

static void espresso10_con_ta_charger_attached(struct omap4_otg *otg)
{
	int val;

	/* Change to USB_EVENT_CHARGER for sleep */
	pr_info("%s, USB_EVENT_CHARGER\n", __func__);

	val = gpio_get_value(otg->ta_nconnected);
	if (val < 0) {
		pr_err("usb ta_nconnected: gpio_get_value error %d\n", val);
		return;
	}

	if (!val) { /* connected */
		otg->otg.default_a = false;
		otg->otg.state = OTG_STATE_B_IDLE;
		otg->otg.last_event = USB_EVENT_CHARGER;

		atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_CHARGER,
			otg->otg.gadget);
	} else { /* disconnected */
		pr_info("%s, VBUS OFF : USB_EVENT_CHARGER is not sent\n",
						__func__);
	}
}

static void espresso10_con_charger_detached(void)
{
}

static void switch_to_mhl_path(bool enable)
{
	/* TODO */
}

static void sii9234_power(int on)
{
	struct omap_mux_partition *p = omap_mux_get("core");
	u16 mux;

	mux = omap_mux_read(p, OMAP4_CTRL_MODULE_PAD_HDMI_HPD_OFFSET);
	if (on) {
		gpio_set_value(mhl_gpios[GPIO_HDMI_EN].gpio, 1);
		msleep(20);
		gpio_set_value(mhl_gpios[GPIO_MHL_RST].gpio, 1);

		omap_mux_write(p, mux | OMAP_PULL_UP,
				OMAP4_CTRL_MODULE_PAD_HDMI_HPD_OFFSET);
	} else {
		omap_mux_write(p, mux & ~OMAP_PULL_UP,
				OMAP4_CTRL_MODULE_PAD_HDMI_HPD_OFFSET);

		gpio_set_value(mhl_gpios[GPIO_HDMI_EN].gpio, 0);
		gpio_set_value(mhl_gpios[GPIO_MHL_RST].gpio, 0);
	}
}

static void sii9234_enable_vbus(bool enable)
{
	/*TODO */
}

static void sii9234_connect(bool on, u8 *devcap)
{
	/* TODO */
}

/* TODO: Need to modify sii9234_platform_data as per Espresso10
 * H/W requirements.Currently, introducing dummy functions to avoid
 * kernel panics and build-fail etc
*/
static struct sii9234_platform_data sii9234_pdata = {
	.prio		= 0,
	.enable		= switch_to_mhl_path,
	.power		= sii9234_power,
	.enable_vbus	= sii9234_enable_vbus,
	.connect	= sii9234_connect,
	.reg_notifier	= acc_register_notifier,
	.unreg_notifier	= acc_unregister_notifier,
	.dongle		= DONGLE_NONE,
	.swing_level	= DEFAULT_MHL_SWING_LEVEL,
	.early_read_devcap = NULL,
};

static void espresso10_deskdock_attached(void)
{
	int ret = 0;

	ret = acc_notify(DONGLE_ATTACHED);
	if (ret <= 0) {
		if (sii9234_pdata.dongle == DONGLE_9292)
			pr_info("Error attaching MHL dongle_9292\n");
		/*
		 * TRICKY: MHL driver always return error value
		 * in case of old dongles,because MHL driver can not
		 * tell whether old dongle has successfuly connected
		 * or not.
		 */
		else if (sii9234_pdata.dongle == DONGLE_9290)
			pr_info("Attaching old MHL dongle_9290??\n");
		espresso10_set_dock_switch(UEVENT_DOCK_DESK);
	}
}

static void espresso10_deskdock_detached(void)
{
	int ret = 0;

	ret = acc_notify(DONGLE_DETACHED);
	if (ret <= 0)
		espresso10_set_dock_switch(UEVENT_DOCK_NONE);
}

static void espresso10_30pin_detected(int device, bool connected)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	if (connected)
		espresso10_otg->current_device |= BIT(device);
	else
		espresso10_otg->current_device &= ~(BIT(device));

	pr_info("cable detect:%s %s, current device = 0x%04x\n",
			device_names[device], (connected) ? "attach" : "detach",
			espresso10_otg->current_device);

	switch (device) {
	case P30_OTG:
		if (connected)
			espresso10_usb_host_attach(espresso10_otg);
		else
			espresso10_usb_host_detach(espresso10_otg);
		break;
	case P30_KEYBOARDDOCK:
		if (connected) {
			espresso10_set_dock_switch(UEVENT_DOCK_KEYBOARD);
			notify_dock_status(1);
		} else {
			espresso10_set_dock_switch(UEVENT_DOCK_NONE);
			notify_dock_status(0);
		}
		break;
	case P30_DESKDOCK:
		if (connected) {
			espresso10_deskdock_attached();
			notify_dock_status(1);
		 } else {
			espresso10_deskdock_detached();
			notify_dock_status(0);
		 }
		break;
	case P30_CARDOCK:
		if (connected) {
			espresso10_set_dock_switch(UEVENT_DOCK_CAR);
			notify_dock_status(1);
		 } else {
			espresso10_set_dock_switch(UEVENT_DOCK_NONE);
			notify_dock_status(0);
		 }
		break;
	case P30_JIG:
		if (connected) {
			check_jig_status(1);
			if (espresso10_otg->uart_manual_mode ==
					ESPRESSO10_MANUAL_UART_MODEM)
				espresso10_cp_uart_actions();
			else
				espresso10_ap_uart_actions();
		} else
			check_jig_status(0);
		break;
	case P30_USB:
		if (connected)
			espresso10_con_usb_charger_attached(espresso10_otg);
		else
			espresso10_con_charger_detached();
		break;
	case P30_TA:
		if (connected)
			espresso10_con_ta_charger_attached(espresso10_otg);
		else
			espresso10_con_charger_detached();
		break;
	case P30_EARJACK_WITH_DOCK:
		if (connected)
			espresso10_set_audio_switch(UEVENT_EARJACK_ATTACHED);
		else
			espresso10_set_audio_switch(UEVENT_EARJACK_DETACHED);
		break;
	case P30_ANAL_TV_OUT:
		pr_warning("This accessory is not supported.\n");
		break;
	default:
		pr_warning("wrong cable detection information!(%d)\n", device);
		break;
	}
}

void omap4_espresso10_usb_detected(int cable_type)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	switch (cable_type) {
	case CABLE_TYPE_USB:
		espresso10_30pin_detected(P30_USB, 1);
		break;
	case CABLE_TYPE_AC:
		espresso10_30pin_detected(P30_TA, 1);
		break;
	case CABLE_TYPE_NONE:
		if (espresso10_otg->current_device & BIT(P30_USB))
			espresso10_30pin_detected(P30_USB, 0);
		else
			espresso10_30pin_detected(P30_TA, 0);
		break;
	default:
		pr_warning("wrong usb detected information!\n");
		break;

	}
}

static s16 espresso10_get_accessory_adc(void)
{
	return omap4_espresso10_get_adc(ACCESSORY_ID);
}

/* 30pin connector */
struct acc_con_platform_data espresso10_con_pdata = {
	.detected		= espresso10_30pin_detected,
	.get_accessory_adc	= espresso10_get_accessory_adc,
};

struct platform_device espresso10_device_connector = {
	.name	= "acc_con",
	.id	= -1,
	.dev	= {
		.platform_data = &espresso10_con_pdata,
	},
};

/* uart_switch, usb_sel */
static ssize_t espresso10_uart_sel_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;
	const char *mode;

	switch (espresso10_otg->uart_manual_mode) {
	case ESPRESSO10_MANUAL_UART_AP:
		mode = "AP";
		break;
	case ESPRESSO10_MANUAL_UART_MODEM:
		mode = "CP";
		break;
	default:
		mode = "NONE";
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t espresso10_uart_sel_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	mutex_lock(&espresso10_otg->lock);

	if (!strncasecmp(buf, "AP", 2)) {
		espresso10_otg->uart_manual_mode = ESPRESSO10_MANUAL_UART_AP;

		if (espresso10_otg->current_device & BIT(P30_JIG))
			espresso10_ap_uart_actions();
	} else if (!strncasecmp(buf, "CP", 2)) {
		espresso10_otg->uart_manual_mode = ESPRESSO10_MANUAL_UART_MODEM;

		if (espresso10_otg->current_device & BIT(P30_JIG))
			espresso10_cp_uart_actions();
	} else if (!strncasecmp(buf, "NONE", 4)) {
		espresso10_otg->uart_manual_mode = ESPRESSO10_MANUAL_UART_NONE;

		if (espresso10_otg->current_device & BIT(P30_JIG))
			espresso10_ap_uart_actions();
	}

	mutex_unlock(&espresso10_otg->lock);

	return size;

}

static ssize_t espresso10_usb_sel_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;
	const char *mode;

	switch (espresso10_otg->usb_manual_mode) {
	case ESPRESSO10_MANUAL_USB_AP:
		mode = "PDA";
		break;
	case ESPRESSO10_MANUAL_USB_MODEM:
		mode = "MODEM";
		break;
	default:
		mode = "NONE";
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t espresso10_usb_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;
	int old_mode;

	mutex_lock(&espresso10_otg->lock);

	old_mode = espresso10_otg->usb_manual_mode;

	if (!strncasecmp(buf, "PDA", 3)) {
		espresso10_otg->usb_manual_mode = ESPRESSO10_MANUAL_USB_AP;

		/* If we are transitioning from CP USB to AP USB then notify the
		 * USB stack that is now attached.
		 */
		if ((espresso10_otg->current_device == BIT(P30_USB)) &&
				(old_mode != ESPRESSO10_MANUAL_USB_AP)) {
			espresso10_cp_usb_detach();
			espresso10_ap_usb_attach(espresso10_otg);
		}
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		espresso10_otg->usb_manual_mode = ESPRESSO10_MANUAL_USB_MODEM;

		/* If we are transitioning from AP USB to CP USB then notify the
		 * USB stack that is has been detached.
		 */
		if ((espresso10_otg->current_device == BIT(P30_USB)) &&
				(old_mode != ESPRESSO10_MANUAL_USB_MODEM)) {
			espresso10_ap_usb_detach(espresso10_otg);
			espresso10_cp_usb_attach();
		}
	} else if (!strncasecmp(buf, "NONE", 5)) {
		espresso10_otg->usb_manual_mode = ESPRESSO10_MANUAL_USB_NONE;

		/* If we are transitioning from CP USB to AP USB then notify the
		 * USB stack that it is now attached.
		 */
		if ((espresso10_otg->current_device == BIT(P30_USB)) &&
				(old_mode != ESPRESSO10_MANUAL_USB_MODEM)) {
			espresso10_cp_usb_detach();
			espresso10_ap_usb_attach(espresso10_otg);
		}
	}

	mutex_unlock(&espresso10_otg->lock);

	return size;
}

static ssize_t espresso10_usb_state_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;
	const char *mode;

	if (espresso10_otg->current_device & BIT(P30_USB))
		mode = "USB_STATE_CONFIGURED";
	else
		mode = "USB_STATE_NOT_CONFIGURED";

	return sprintf(buf, "%s\n", mode);
}

static ssize_t espresso10_jig_on_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;
	const char *mode;

	if (espresso10_otg->current_device & BIT(P30_JIG))
		mode = "1";
	else
		mode = "0";

	return sprintf(buf, "%s\n", mode);
}

/* Factory test application reads /sys/class/sec/switch/adc.
   If key_string_on value is 0x1C, the application enables key-strings.
   If key_string_on value is 0, the application disables key-strings.
   This is Samsung standard.
 */
static ssize_t espresso10_adc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	s16 adc_val;
	u8 key_string_on;

	adc_val = espresso10_get_accessory_adc();
	pr_info("accessory_id adc value = %d\n", adc_val);

	if ((3600 < adc_val) && (adc_val < 3800))
		key_string_on = 0x1C;
	else
		key_string_on = 0;

	return sprintf(buf, "%x\n", key_string_on);
}

static int espresso10_otg_set_host(struct otg_transceiver *otg,
				 struct usb_bus *host)
{
	otg->host = host;
	if (!host)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static int espresso10_otg_set_peripheral(struct otg_transceiver *otg,
				       struct usb_gadget *gadget)
{
	otg->gadget = gadget;
	if (!gadget)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static void espresso10_otg_work(struct work_struct *data)
{
	struct omap4_otg *espresso10_otg =
		container_of(data, struct omap4_otg, set_vbus_work);

	pr_info("otg %s(%d): current device 0x%04x4x4x4x\n",
			__func__, __LINE__,
			espresso10_otg->current_device);

	mutex_lock(&espresso10_otg->lock);

	/* Only allow VBUS drive when in host mode. */
	if (!(espresso10_otg->current_device & BIT(P30_OTG))) {
		pr_info("otg current device is not USB Host.\n");
		mutex_unlock(&espresso10_otg->lock);
		return;
	}

	espresso10_set_vbus_drive(espresso10_otg->need_vbus_drive);

	mutex_unlock(&espresso10_otg->lock);
}

static int espresso10_otg_set_vbus(struct otg_transceiver *otg, bool enabled)
{
	struct omap4_otg *espresso10_otg =
	    container_of(otg, struct omap4_otg, otg);
	dev_dbg(otg->dev, "vbus %s\n", enabled ? "on" : "off");

	espresso10_otg->need_vbus_drive = enabled;
	schedule_work(&espresso10_otg->set_vbus_work);

	return 0;
}

static int espresso10_otg_phy_init(struct otg_transceiver *otg)
{
	if (otg->last_event == USB_EVENT_ID)
		omap4430_phy_power(otg->dev, 1, 1);
	else
		omap4430_phy_power(otg->dev, 0, 1);
	return 0;
}

static void espresso10_otg_phy_shutdown(struct otg_transceiver *otg)
{
	omap4430_phy_power(otg->dev, 0, 0);
}

static int espresso10_otg_set_suspend(struct otg_transceiver *otg, int suspend)
{
	return omap4430_phy_suspend(otg->dev, suspend);
}

static int espresso10_otg_is_active(struct otg_transceiver *otg)
{
	return omap4430_phy_is_active(otg->dev);
}

static irqreturn_t ta_nconnected_irq(int irq, void *_otg)
{
	struct omap4_otg *otg = _otg;
	int val;

	val = gpio_get_value(otg->ta_nconnected);
	if (val < 0) {
		pr_err("usb ta_nconnected: gpio_get_value error %d\n", val);
		return IRQ_HANDLED;
	}

	pr_info("usb ta_nconnected_irq : VBUS %s\n", val ? "OFF" : "ON");

	irq_set_irq_type(irq, val ?
			IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (!val) { /* connected */
		/* TODO: check ADC here. */
		espresso10_ap_usb_attach(otg);
		acc_notify(DONGLE_POWER_ATTACHED);

	} else { /* disconnected */
		/* TODO: save current device. */
		espresso10_ap_usb_detach(otg);
		acc_notify(DONGLE_POWER_DETACHED);
	}

	return IRQ_HANDLED;
}

static int espresso10_vbus_detect_init(struct omap4_otg *otg)
{
	int status = 0;
	int irq = 0;
	int val;
	int ta_nconnected =
		omap_muxtbl_get_gpio_by_name("TA_nCONNECTED");

	dev_info(&otg->dev, "init TA_nCONNECTED : %d\n", ta_nconnected);

	status = gpio_request(ta_nconnected, "TA_nCONNECTED");
	if (status < 0) {
		dev_err(&otg->dev, "gpio %d request failed.\n", ta_nconnected);
		return status;
	}

	status = gpio_direction_input(ta_nconnected);
	if (status < 0) {
		dev_err(&otg->dev, "failed to set gpio %d as input\n",
				ta_nconnected);
		return status;
	}

	otg->ta_nconnected = ta_nconnected;
	irq = gpio_to_irq(ta_nconnected);
	dev_info(&otg->dev, "request_irq : %d(gpio: %d)\n", irq, ta_nconnected);
	val = gpio_get_value(ta_nconnected);

	status = request_threaded_irq(irq, NULL, ta_nconnected_irq,
			(val ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH) | \
			IRQF_ONESHOT | IRQF_NO_SUSPEND,
			"TA_nConnected", otg);
	if (status < 0) {
		dev_err(&otg->dev, "request irq %d failed for gpio %d\n",
				irq, ta_nconnected);
		return status;
	}

	return 0;
}

/* dock keyboard */
static struct dock_keyboard_callback {
	struct input_dev *dev;
	int (*cb) (struct input_dev *dev, bool connected);
} espresso10_dock_keyboard_cb;

static int espresso10_dock_keyboard_callback(bool connected)
{
	if (espresso10_dock_keyboard_cb.dev && espresso10_dock_keyboard_cb.cb)
		return espresso10_dock_keyboard_cb.
		    cb(espresso10_dock_keyboard_cb.dev, connected);
	return 0;
}

static void espresso10_dock_keyboard_power(bool on)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	if (on) {
		if (espresso10_otg->uart_manual_mode ==
		    ESPRESSO10_MANUAL_UART_MODEM) {
			pr_info("kbd: switch UART IF to AP\n");
			espresso10_ap_uart_actions();
		}
		espresso_accessory_power(1, 1);
	} else {
		espresso_accessory_power(1, 0);
		if (espresso10_otg->uart_manual_mode ==
		    ESPRESSO10_MANUAL_UART_MODEM) {
			pr_info("kbd: switch UART IF to CP\n");
			espresso10_cp_uart_actions();
		}
	}
}

static void espresso10_dock_keyboard_register_cb(struct input_dev *dev,
						 void *cb)
{
	espresso10_dock_keyboard_cb.dev = dev;
	espresso10_dock_keyboard_cb.cb = cb;
}

struct dock_keyboard_platform_data espresso10_dock_keyboard_pdata = {
	.power = espresso10_dock_keyboard_power,
	.register_cb = espresso10_dock_keyboard_register_cb,
};

struct platform_device espresso10_device_dock_keyboard = {
	.name = KBD_DRV_NAME,
	.id = 0,
	.dev = {
		.platform_data = &espresso10_dock_keyboard_pdata,
		},
};

static void __init espresso10_switch_initial_setup(void)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	if (init_switch_sel & MASK_SWITCH_UART_AP) {
		espresso10_ap_uart_actions();
		espresso10_otg->uart_manual_mode = ESPRESSO10_MANUAL_UART_AP;
	} else {
		espresso10_cp_uart_actions();
		espresso10_otg->uart_manual_mode = ESPRESSO10_MANUAL_UART_MODEM;
	}

	if (init_switch_sel & MASK_SWITCH_USB_AP) {
		espresso10_cp_usb_detach();
		espresso10_otg->usb_manual_mode = ESPRESSO10_MANUAL_USB_AP;
	} else {
		espresso10_cp_usb_attach();
		espresso10_otg->usb_manual_mode = ESPRESSO10_MANUAL_USB_MODEM;
	}

}

static int __init espresso10_save_init_switch_param(char *str)
{
	int ret;

	ret = kstrtoint(str, 10, &init_switch_sel);
	if (ret < 0)
		return ret;

	return 0;
}
__setup("switch_sel=", espresso10_save_init_switch_param);


#ifdef CONFIG_USB_HOST_NOTIFY
static void espresso10_booster(int enable)
{
	espresso10_set_vbus_drive(!!enable);
}

static struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name	= "usb_otg",
	.booster	= espresso10_booster,
	.thread_enable = 1,
};

static struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};

static void espresso10_host_notifier_init(struct omap4_otg *otg)
{
	int acc_out =
		omap_muxtbl_get_gpio_by_name("V_ACCESSORY_OUT_5.0V");

	if (acc_out < 0) {
		dev_err(&otg->dev, "V_ACCESSORY_OUT_5.0V is invalid.\n");
		return;
	}

	host_notifier_pdata.gpio = acc_out;
	otg->pdata = &host_notifier_pdata;

	platform_device_register(&host_notifier_device);
}
#endif

static struct i2c_board_info __initdata espresso10_i2c8_boardinfo[] = {
	{
		I2C_BOARD_INFO("sii9234_mhl_tx", 0x72>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("sii9234_tpi", 0x7A>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("sii9234_hdmi_rx", 0x92>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("sii9234_cbus", 0xC8>>1),
		.platform_data = &sii9234_pdata,
	},
};

static void connector_gpio_init(void)
{
	int i;

	u8 board_type;

	for (i = 0; i < ARRAY_SIZE(connector_gpios); i++)
		connector_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(connector_gpios[i].label);

	for (i = 0; i < ARRAY_SIZE(uart_sw_gpios); i++)
		uart_sw_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(uart_sw_gpios[i].label);

	gpio_request_array(connector_gpios, ARRAY_SIZE(connector_gpios));
	gpio_request_array(uart_sw_gpios, ARRAY_SIZE(uart_sw_gpios));

	board_type = omap4_espresso10_get_board_type();
	if (board_type == SEC_MACHINE_ESPRESSO10_USA_BBY) {

		for (i = 0; i < ARRAY_SIZE(mhl_gpios); i++)
			mhl_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(mhl_gpios[i].label);

		gpio_request_array(mhl_gpios, ARRAY_SIZE(mhl_gpios));

		espresso10_i2c8_boardinfo[0].irq =
			gpio_to_irq(mhl_gpios[GPIO_MHL_INT].gpio);
	}
}

static int __init espresso10_plugged_usb_cable_init(void)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;

	/* USB connected */
	if (gpio_get_value(espresso10_otg->ta_nconnected) == 0)
		omap4_vusb_enable(espresso10_otg, true);

	return 0;
}

fs_initcall(espresso10_plugged_usb_cable_init);

void __init omap4_espresso10_connector_init(void)
{
	struct omap4_otg *espresso10_otg = &espresso10_otg_xceiv;
	int ret;

	connector_gpio_init();
	mutex_init(&espresso10_otg->lock);
	INIT_WORK(&espresso10_otg->set_vbus_work, espresso10_otg_work);
	device_initialize(&espresso10_otg->dev);
	dev_set_name(&espresso10_otg->dev, "%s", "espresso10_otg");
	ret = device_add(&espresso10_otg->dev);
	if (ret) {
		pr_err("%s: cannot reg device '%s' (%d)\n", __func__,
		       dev_name(&espresso10_otg->dev), ret);
		return;
	}

	dev_set_drvdata(&espresso10_otg->dev, espresso10_otg);

	espresso10_otg->otg.dev			= &espresso10_otg->dev;
	espresso10_otg->otg.label		= "espresso10_otg_xceiv";
	espresso10_otg->otg.set_host		= espresso10_otg_set_host;
	espresso10_otg->otg.set_peripheral	= espresso10_otg_set_peripheral;
	espresso10_otg->otg.set_suspend		= espresso10_otg_set_suspend;
	espresso10_otg->otg.set_vbus		= espresso10_otg_set_vbus;
	espresso10_otg->otg.init		= espresso10_otg_phy_init;
	espresso10_otg->otg.shutdown		= espresso10_otg_phy_shutdown;
	espresso10_otg->otg.is_active		= espresso10_otg_is_active;

	ATOMIC_INIT_NOTIFIER_HEAD(&espresso10_otg->otg.notifier);

	ret = otg_set_transceiver(&espresso10_otg->otg);
	if (ret)
		pr_err("espresso10_otg: cannot set transceiver (%d)\n", ret);

	omap4430_phy_init(&espresso10_otg->dev);
	omap4430_phy_init_for_eyediagram(SWCAP_TRIM_OFFSET);
	espresso10_otg_set_suspend(&espresso10_otg->otg, 0);
	espresso10_vbus_detect_init(espresso10_otg);
#ifdef CONFIG_USB_HOST_NOTIFY
	espresso10_host_notifier_init(espresso10_otg);
#endif

	sec_switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(sec_switch_dev)) {
		pr_err("(%s): failed to created device (switch_dev)!\n",
								__func__);
		goto switch_dev_fail;
	}

	ret = sysfs_create_group(&sec_switch_dev->kobj,
						&manual_switch_sel_group);
	if (ret < 0)
		pr_err("fail to  create manual switch_sel sysfs group (%d)\n",
									ret);
switch_dev_fail:

	espresso10_switch_initial_setup();

	/* dock keyboard */
	espresso10_dock_keyboard_pdata.dock_irq_gpio =
	    connector_gpios[GPIO_ACCESSORY_INT].gpio;

	platform_device_register(&espresso10_device_dock_keyboard);

	/* ADC IC (STMPE811) */
	i2c_register_board_info(6, espresso10_i2c6_boardinfo,
					ARRAY_SIZE(espresso10_i2c6_boardinfo));

	/* MHL (SII9244) */
	i2c_register_board_info(8, espresso10_i2c8_boardinfo,
					ARRAY_SIZE(espresso10_i2c8_boardinfo));

	/* 30pin connector */
	espresso10_con_pdata.accessory_irq_gpio =
				connector_gpios[GPIO_ACCESSORY_INT].gpio;
	espresso10_con_pdata.dock_irq_gpio =
				connector_gpios[GPIO_DOCK_INT].gpio;
	espresso10_con_pdata.jig_on_gpio = connector_gpios[GPIO_JIG_ON].gpio;
	espresso10_con_pdata.dock_keyboard_cb =
	    espresso10_dock_keyboard_callback;

	platform_device_register(&espresso10_device_connector);

	espresso10_otg->dock_switch.name = "dock";
	switch_dev_register(&espresso10_otg->dock_switch);

	espresso10_otg->audio_switch.name = "usb_audio";
	switch_dev_register(&espresso10_otg->audio_switch);

	espresso10_otg->current_device = 0;
}

int __init omap4_espresso10_connector_late_init(void)
{
	unsigned int board_type = omap4_espresso10_get_board_type();

	if (system_rev < 7 || board_type != SEC_MACHINE_ESPRESSO10)
		if (gpio_get_value(connector_gpios[GPIO_JIG_ON].gpio))
			uart_set_l3_cstr(true);

	return 0;
}

late_initcall(omap4_espresso10_connector_late_init);

/* arch/arm/mach-omap2/board-gokey-connector.c
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
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/usb/otg.h>
#include <linux/delay.h>
#include <linux/sii9234.h>
#include <linux/i2c/twl.h>
#include <linux/mutex.h>
#include <linux/switch.h>
#include <linux/battery.h>
#include <linux/irq.h>
#include <linux/usb/otg_id.h>
#include <linux/platform_data/fsa9480.h>
#include <linux/regulator/consumer.h>

#include <plat/usb.h>

#include "board-gokey.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "omap_phy_tune.c"

#define CHARGERUSB_CTRL1		0x8
#define CHARGERUSB_CTRL3		0xA
#define CHARGERUSB_CINLIMIT		0xE

#define TWL6030_VBUS_IRQ       (TWL6030_IRQ_BASE + USB_PRES_INTR_OFFSET)
#define TWL6030_VBUS_FLAGS     (IRQF_TRIGGER_FALLING | IRQF_ONESHOT)

#define TWL_REG_CONTROLLER_INT_MASK	0x00
#define TWL_CONTROLLER_MVBUS_DET	BIT(1)
#define TWL_CONTROLLER_RSVD		BIT(5)
#define TWL_REG_CONTROLLER_STAT1       0x03

#define GOKEY_MANUAL_USB_NONE	0
#define GOKEY_MANUAL_USB_MODEM	1	/* not used for wifi or mid model */
#define GOKEY_MANUAL_USB_AP	2

#define GOKEY_MANUAL_UART_NONE		0
#define GOKEY_MANUAL_UART_FACTORY	1
#define GOKEY_MANUAL_UART_AP		2

#define IF_UART_SEL_FACTORY	0
#define IF_UART_SEL_AP		1

#define MASK_SWITCH_USB_AP	0x01
#define MASK_SWITCH_UART_AP	0x02

#define GOKEY_OTG_ID_FSA9480_PRIO	INT_MIN
#define GOKEY_OTG_ID_FSA9480_LAST_PRIO	INT_MAX

#define GOKEY_MHL_SWING_LEVEL		0xF4

#define SWCAP_TRIM_OFFSET	(0x01)
#define REF_GEN_TEST	(0x06)
#define VBUS_DELAY 0

#if !defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
static int otg_en;
#endif

/* type => CABLE_TYPE_NONE(0), _USB(1), or _AC(2) */
static int g_charging_type = CABLE_TYPE_NONE;
static bool device_isupdated;
static bool usb_attached;

static bool uart_l3_cstr_flag;	/* flag to disable PM for uart */

struct omap4_otg {
	struct otg_transceiver otg;
	struct device dev;

	struct regulator *vusb;
	struct work_struct set_vbus_work;
	struct mutex lock;
	struct delayed_work vbus_detection_work;

	bool reg_on;
	bool need_vbus_drive;
	int usb_manual_mode;
	int uart_manual_mode;
	int current_device;
	int gpio_vbus_detect;
	int irq_vbus_detect;

	struct switch_dev dock_switch;
	struct switch_dev audio_switch;
};

static struct omap4_otg gokey_otg_xceiv;

static struct device *sec_switch_dev;
static int init_switch_sel;

enum {
	UEVENT_DOCK_NONE = 0,
	UEVENT_DOCK_DESK,
	UEVENT_DOCK_CAR,
	UEVENT_DOCK_KEYBOARD,
	UEVENT_DOCK_UNKNOWN,
};

enum {
	GPIO_TA_CURRENT_SEL_AP = 0,
	GPIO_JIG_ON,
	GPIO_VBUS_DETECT,
	GPIO_BOOT_MODE,
};

static struct gpio connector_gpios[] = {
	[GPIO_TA_CURRENT_SEL_AP] = {
				    .flags = GPIOF_IN,
				    .label = "TA_CURRENT_SEL_AP",
				    },
	[GPIO_JIG_ON] = {
			 .flags = GPIOF_IN,
			 .label = "JIG_ON_18",
			 },
	[GPIO_VBUS_DETECT] = {
			      .flags = GPIOF_IN,
			      .label = "5V_DET",
			      },
	[GPIO_BOOT_MODE] = {
			    .flags = GPIOF_IN,
			    .label = "BOOT_MODE",
			    },
};

enum {
	GPIO_UART_SEL
};

static struct gpio uart_sw_gpios[] = {
	[GPIO_UART_SEL] = {
			   .flags = GPIOF_OUT_INIT_LOW,
			   .label = "UART_SEL",
			   }
};

static ssize_t gokey_otg_usb_sel_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t gokey_otg_usb_sel_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size);

static ssize_t gokey_otg_uart_switch_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf);
static ssize_t gokey_otg_uart_switch_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size);
static ssize_t gokey_usb_state_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

static DEVICE_ATTR(uart_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
		   gokey_otg_uart_switch_show,
		   gokey_otg_uart_switch_store);
static DEVICE_ATTR(usb_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
		   gokey_otg_usb_sel_show, gokey_otg_usb_sel_store);
static DEVICE_ATTR(usb_state, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
		   gokey_usb_state_show, NULL);

static struct attribute *manual_switch_sel_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_usb_state.attr,
	NULL,
};

static const struct attribute_group manual_switch_sel_group = {
	.attrs = manual_switch_sel_attributes,
};

static void gokey_set_dock_switch(int state)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;

	switch_set_state(&gokey_otg->dock_switch, state);
}

static void gokey_mux_usb_to_fsa(bool enable)
{
	;	/* for compatible issue with fsa9480 code */
}

static void gokey_vusb_enable(struct omap4_otg *otg, bool enable)
{
	if (IS_ERR_OR_NULL(otg->vusb)) {
		otg->vusb = regulator_get(&otg->dev, "vusb");
		if (IS_ERR(otg->vusb)) {
			dev_err(&otg->dev, "cannot get vusb regulator\n");
			return;
		}
	}

	if (enable) {
		if (!otg->reg_on) {
			regulator_enable(otg->vusb);
			otg->reg_on = true;
		}
	} else if (otg->reg_on) {
		regulator_disable(otg->vusb);
		otg->reg_on = false;
	}
}

#ifdef USB_TWL6030_5VOUT
static void gokey_set_vbus_drive(bool enable)
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
static void gokey_set_vbus_drive(bool enable)
{
#if !defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
	int gpio_acc_en = connector_gpios[GPIO_TA_CURRENT_SEL_AP].gpio;

	pr_info("otg gokey_set_vbus_drive gpio %d : %s\n",
		gpio_acc_en, enable ? "ON" : "OFF");

	gpio_set_value(gpio_acc_en, !!enable);
#endif
}
#endif

static int gokey_is_vbus_on(void)
{
	return gpio_get_value(connector_gpios[GPIO_VBUS_DETECT].gpio);
}

static void gokey_usb_attach(struct omap4_otg *otg)
{
	gokey_vusb_enable(otg, true);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_VBUS;
	usb_attached = true;

	atomic_notifier_call_chain(&otg->otg.notifier,
				   USB_EVENT_VBUS, otg->otg.gadget);
}

static void gokey_usb_detach(struct omap4_otg *otg)
{
	gokey_vusb_enable(otg, false);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_NONE;
	usb_attached = false;

	atomic_notifier_call_chain(&otg->otg.notifier,
				   USB_EVENT_NONE, otg->otg.gadget);
}

static void gokey_charger_attach(struct omap4_otg *otg)
{
	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_CHARGER;

	atomic_notifier_call_chain(&otg->otg.notifier,
				   USB_EVENT_CHARGER, otg->otg.gadget);
}

static void gokey_charger_detach(struct omap4_otg *otg)
{
	if (otg->otg.last_event == USB_EVENT_CHARGER) {
		otg->otg.default_a = false;
		otg->otg.state = OTG_STATE_B_IDLE;
		otg->otg.last_event = USB_EVENT_NONE;

		atomic_notifier_call_chain(&otg->otg.notifier,
					   USB_EVENT_NONE, otg->otg.gadget);
	}
}

int gokey_get_charging_type(void)
{
	return g_charging_type;
}

void gokey_set_charging_type(int device)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;
	switch (device) {
	case FSA9480_DETECT_USB:
		g_charging_type = CABLE_TYPE_USB;
		break;
	case FSA9480_DETECT_AV_365K:
		if (gokey_is_vbus_on()) {
			g_charging_type = CABLE_TYPE_USB; /* chg cur(mA)=USB */
			gokey_charger_attach(gokey_otg);
		} else
			g_charging_type = CABLE_TYPE_NONE;
		break;
	case FSA9480_DETECT_CHARGER:
		g_charging_type = CABLE_TYPE_AC;
		break;
	default:
		g_charging_type = CABLE_TYPE_NONE;
	}
	device_isupdated = true;
}
static void gokey_ap_uart_actions(void)
{
	gpio_set_value(uart_sw_gpios[GPIO_UART_SEL].gpio, IF_UART_SEL_AP);
}

static void gokey_factory_uart_actions(void)
{
	gpio_set_value(uart_sw_gpios[GPIO_UART_SEL].gpio, IF_UART_SEL_FACTORY);
}

static void gokey_otg_mask_vbus_irq(void)
{
	twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
					REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
					REG_INT_MSK_STS_C);
}

static void gokey_otg_unmask_vbus_irq(void)
{
	/* null function : vbus irq of pmic will not be enabled
	 * to handle vbus event by gokey connector driver
	 * because unmask will not be done.
	 */
}

static ssize_t gokey_otg_uart_switch_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;
	const char *mode;

	switch (gokey_otg->uart_manual_mode) {
	case GOKEY_MANUAL_UART_AP:
		mode = "AP";
		break;
	case GOKEY_MANUAL_UART_FACTORY:
		mode = "CP";	/* emulates uart of CP for mid or wifi */
		break;
	default:
		mode = "NONE";
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t gokey_otg_uart_switch_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;

	mutex_lock(&gokey_otg->lock);

	if (!strncasecmp(buf, "AP", 2)) {
		gokey_otg->uart_manual_mode = GOKEY_MANUAL_UART_AP;
		if (gokey_otg->current_device == FSA9480_DETECT_JIG)
			gokey_ap_uart_actions();
	} else if (!strncasecmp(buf, "CP", 2)) {
		/* emulates uart of CP for mid or wifi */
		gokey_otg->uart_manual_mode = GOKEY_MANUAL_UART_FACTORY;
		if (gokey_otg->current_device == FSA9480_DETECT_JIG)
			gokey_factory_uart_actions();
	} else if (!strncasecmp(buf, "NONE", 4)) {
		gokey_otg->uart_manual_mode = GOKEY_MANUAL_UART_NONE;
		if (gokey_otg->current_device == FSA9480_DETECT_JIG)
			gokey_ap_uart_actions();
	}

	mutex_unlock(&gokey_otg->lock);

	return size;
}

static ssize_t gokey_otg_usb_sel_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;
	const char *mode;

	if (gokey_otg->usb_manual_mode == GOKEY_MANUAL_USB_AP)
		mode = "PDA";
	else
		mode = "NONE";

	return sprintf(buf, "%s\n", mode);
}

static ssize_t gokey_otg_usb_sel_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;

	mutex_lock(&gokey_otg->lock);

	if (!strncasecmp(buf, "PDA", 3))
		gokey_otg->usb_manual_mode = GOKEY_MANUAL_USB_AP;
	else
		gokey_otg->usb_manual_mode = GOKEY_MANUAL_USB_NONE;

	mutex_unlock(&gokey_otg->lock);

	return size;
}

static ssize_t gokey_usb_state_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;
	const char *mode;

	if (gokey_otg->current_device == FSA9480_DETECT_USB)
		mode = "USB_STATE_CONFIGURED";
	else
		mode = "USB_STATE_NOT_CONFIGURED";

	return sprintf(buf, "%s\n", mode);
}

static bool gokey_otg_vbus_present(void)
{
	/* This is used to distinguish AV_365K and AV_365K_CHARGER by
	 * fsa9480 driver. For gokey, no need to distinguish them.
	 * return 0 makes fsa9480 driver treats both of them as AV_365K.
	 */

	return 0;
}

static u16 fsa9480_devtype;	/* DevType2(Higher 8bit):DevType1(Lower 8bit) */
static u8 fsa9480_adc;		/* for the future use.. */

/* called by fas9480 driver to save device type(1&2) and adc value */
void fsa9480_save_dev_adc(u16 dev, u8 adc)
{
	fsa9480_devtype = dev;
	fsa9480_adc = adc;
}

static void gokey_fsa_usb_detected(int device)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;
	int old_device;

	mutex_lock(&gokey_otg->lock);

	old_device = gokey_otg->current_device;
	gokey_otg->current_device = device;

	pr_info("%s: device detected --> 0x%x\n", __func__, device);

	pr_debug("detected 0x%x\n", device);

	gokey_set_charging_type(device);

	switch (device) {
	case FSA9480_DETECT_USB:
		break;
	case FSA9480_DETECT_CHARGER:
		break;
	case FSA9480_DETECT_AV_365K:
		/* if TA/USB is attached to AV DOCK */
		gokey_set_dock_switch(UEVENT_DOCK_DESK);
		break;
	case FSA9480_DETECT_NONE:
		gokey_mux_usb_to_fsa(true);

		switch (old_device) {
		case FSA9480_DETECT_UART:
			if (uart_l3_cstr_flag)
				uart_set_l3_cstr(false);
			if (gokey_otg->uart_manual_mode ==
			    GOKEY_MANUAL_UART_NONE)
				gokey_ap_uart_actions();
			break;
		case FSA9480_DETECT_JIG:
			/* Enable PM for uart if JIG is unplugged */
			if (uart_l3_cstr_flag)
				uart_set_l3_cstr(false);
			if (gokey_otg->uart_manual_mode ==
			    GOKEY_MANUAL_UART_NONE)
				gokey_ap_uart_actions();
			break;
		case FSA9480_DETECT_AV_365K:
			if (!gokey_is_vbus_on())
				gokey_set_dock_switch(UEVENT_DOCK_NONE);
			break;
		case FSA9480_DETECT_USB:
			break;
		case FSA9480_DETECT_CHARGER:
			break;
		default:
			gokey_usb_detach(gokey_otg);
			break;
		};
		break;
		/* UART is the same with DETECT_JIG_UART_OFF, JIG_UART_ON */
	case FSA9480_DETECT_UART:
		if (uart_l3_cstr_flag)
			uart_set_l3_cstr(true);
		switch (gokey_otg->uart_manual_mode) {
		case GOKEY_MANUAL_UART_FACTORY:
			gokey_factory_uart_actions();
			break;
		case GOKEY_MANUAL_UART_AP:
		default:
			gokey_ap_uart_actions();
			break;
		};
		break;
	case FSA9480_DETECT_JIG:
		/* Disble PM for uart if JIG is unplugged */
		if (uart_l3_cstr_flag)
			uart_set_l3_cstr(true);
		switch (gokey_otg->uart_manual_mode) {
		case GOKEY_MANUAL_UART_FACTORY:
			gokey_factory_uart_actions();
			break;
		case GOKEY_MANUAL_UART_AP:
		default:
			gokey_ap_uart_actions();
			break;
		};

		/* Thiis for waking up whole system when JIG UART ON cable
		 * is connected. Send undocking event immediately to hide
		 * docking icon at status bar. Btw, (0x1 << 10) is JIG_UART_ON
		 * for fsa9480 muic.
		 */
		if (fsa9480_devtype & (0x1 << 10)) {
			gokey_set_dock_switch(UEVENT_DOCK_CAR);
			gokey_set_dock_switch(UEVENT_DOCK_NONE);
		}
		break;
	}

	mutex_unlock(&gokey_otg->lock);
}

static struct fsa9480_detect_set fsa_detect_sets[] = {
	{
	 .prio = GOKEY_OTG_ID_FSA9480_PRIO,
#if !defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
	 .mask = FSA9480_DETECT_ALL,
#else
	 .mask = FSA9480_DETECT_ALL
		 & ~(FSA9480_DETECT_USB_HOST)
		 & ~(FSA9480_DETECT_AV_365K_CHARGER), /* charge by connector */
#endif
	 },
	{
	 .prio = GOKEY_OTG_ID_FSA9480_LAST_PRIO,
	 .mask = 0,
	 .fallback = true,
	 },
};

static struct fsa9480_platform_data gokey_fsa9480_pdata = {
	.detect_time = 500,
	.detect_sets = fsa_detect_sets,
	.num_sets = ARRAY_SIZE(fsa_detect_sets),

	.enable = gokey_mux_usb_to_fsa,
	.detected = gokey_fsa_usb_detected,
	.external_vbus_irq      = TWL6030_VBUS_IRQ,
	.external_vbus_flags    = TWL6030_VBUS_FLAGS,
	.mask_vbus_irq          = gokey_otg_mask_vbus_irq,
	.unmask_vbus_irq        = gokey_otg_unmask_vbus_irq,
	.vbus_present = gokey_otg_vbus_present,
	.save_dev_adc = fsa9480_save_dev_adc,
};

static struct i2c_board_info __initdata gokey_connector_i2c4_boardinfo[] = {
	{
	 I2C_BOARD_INFO("fsa9480", 0x4A >> 1),
	 .platform_data = &gokey_fsa9480_pdata,
	 },
};

static int gokey_otg_set_host(struct otg_transceiver *otg,
				 struct usb_bus *host)
{
	otg->host = host;

	if (!host)
		otg->state = OTG_STATE_UNDEFINED;

	return 0;
}

static int gokey_otg_set_peripheral(struct otg_transceiver *otg,
				       struct usb_gadget *gadget)
{
	otg->gadget = gadget;

	if (!gadget)
		otg->state = OTG_STATE_UNDEFINED;

	return 0;
}

static void gokey_otg_work(struct work_struct *data)
{
	struct omap4_otg *gokey_otg =
	    container_of(data, struct omap4_otg, set_vbus_work);

	pr_info("otg %s(%d): current device %d\n",
		__func__, __LINE__, gokey_otg->current_device);

	mutex_lock(&gokey_otg->lock);

	/* Only allow VBUS drive when in host mode. */
	if (gokey_otg->current_device != FSA9480_DETECT_USB_HOST) {
		pr_info("otg current device is not USB Host.\n");
		mutex_unlock(&gokey_otg->lock);
		return;
	}

	gokey_set_vbus_drive(gokey_otg->need_vbus_drive);

	mutex_unlock(&gokey_otg->lock);
}

static void vbus_detect_work(struct work_struct *work)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;
	int count;

	/*it take a time to define cable type*/
	/*at least 6xx msec*/

	for (count = 0; count < 0xf; count++) {
		msleep(100);
		if (device_isupdated) {
			pr_info("%s: device is updated for %dmsec\n",
				__func__, count);
			break;
		} else
			pr_info("%s: device is not updated for %dmsec\n",
				__func__, count);
	}

	if (!gpio_get_value(gokey_otg->gpio_vbus_detect)) {
		if (usb_attached)
			gokey_usb_detach(gokey_otg);
		else
			gokey_charger_detach(gokey_otg);
		pr_info("%s : vbus detach\n", __func__);
		}
	else {
			if (g_charging_type == CABLE_TYPE_USB)
				gokey_usb_attach(gokey_otg);
			else
				gokey_charger_attach(gokey_otg);

			pr_info("%s : vbus attach\n", __func__);
		}
	device_isupdated = false;
}

static int gokey_otg_phy_init(struct otg_transceiver *otg)
{
	if (otg->last_event == USB_EVENT_ID)
		omap4430_phy_power(otg->dev, 1, 1);
	else
		omap4430_phy_power(otg->dev, 0, 1);

	return 0;
}

static void gokey_otg_phy_shutdown(struct otg_transceiver *otg)
{
	omap4430_phy_power(otg->dev, 0, 0);
}

static int gokey_otg_set_suspend(struct otg_transceiver *otg, int suspend)
{
	return omap4430_phy_suspend(otg->dev, suspend);
}

/* this is required to check phy for boot with USB cable */
static int gokey_otg_is_active(struct otg_transceiver *otg)
{
	return omap4430_phy_is_active(otg->dev);
}

static void gokey_uart_sw_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(uart_sw_gpios); i++)
		uart_sw_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(uart_sw_gpios[i].label);

	gpio_request_array(uart_sw_gpios, ARRAY_SIZE(uart_sw_gpios));
}

static void gokey_connector_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(connector_gpios); i++)
		connector_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(connector_gpios[i].label);

	gpio_request_array(connector_gpios, ARRAY_SIZE(connector_gpios));
}

static void gokey_fsa9480_gpio_init(void)
{
#if !defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
	gokey_fsa9480_pdata.external_id =
	    omap_muxtbl_get_gpio_by_name("USB_OTG_ID");
#endif

	gokey_connector_i2c4_boardinfo[0].irq =
	    gpio_to_irq(omap_muxtbl_get_gpio_by_name("JACK_nINT"));

#if !defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
	/* USB_OTG_EN need to be low by default, otherwise
	 * its affecting MHL functionality.
	 * OTG HOST driver will enable it when OMAP is
	 * configure as HOST.
	 */
	otg_en = omap_muxtbl_get_gpio_by_name("USB_OTG_EN");
	gpio_request(otg_en, "USB_OTG_EN");
	gpio_direction_output(otg_en, 0);
#endif
}

/* Basically TA/USB attach and detach is handled by muic handler
 * gokey_fsa_usb_detected(). FSA9480 can detect TA/USB while AV DOCK
 * is already attached, but it CAN NOT detect TA/USB detach event.
 * So external vbus irq is needed. T1 uses internal vbus irq of PMIC
 * but sadly it doesn't work well with GOKEY. As a result one gpio
 * is reserved to detect vbus and used to detect TA/USB detach from DOCK.
 * (NOTE: DOCK treats everything TA since ID+- doesn't exist.)
 */
static irqreturn_t vbus_detection_irq(int irq, void *_otg)
{
	struct omap4_otg *gokey_otg = _otg;

	pr_info("%s : VBUS OFF  vbus_detect : %d\n", __func__,
		gpio_get_value(gokey_otg->gpio_vbus_detect));

	schedule_delayed_work(&gokey_otg->vbus_detection_work, VBUS_DELAY);
	return IRQ_HANDLED;
}

static int gokey_vbus_detected_init(struct omap4_otg *otg)
{
	int vbus_gpio = omap_muxtbl_get_gpio_by_name("5V_DET");
	int ret = 0;
	int val = 0;

	ret = gpio_request(vbus_gpio, "5V_DET");
	if (ret > 0) {
		dev_err(&otg->dev, "gpio %d request failed.\n", vbus_gpio);
		return ret;
	}

	ret = gpio_direction_input(vbus_gpio);

	if (ret > 0) {
		dev_err(&otg->dev, "failed to set gpio %d as input\n",
								vbus_gpio);
		return ret;
	}

	otg->gpio_vbus_detect = vbus_gpio;
	otg->irq_vbus_detect = gpio_to_irq(vbus_gpio);
	val = gpio_get_value(vbus_gpio);

	/* detect TA/USB detach only, attach is handled by muic handler */
	ret = request_threaded_irq(otg->irq_vbus_detect,
				NULL, vbus_detection_irq,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"5V_DET", otg);

	if (ret > 0) {
		dev_err(&otg->dev, "%s : request irq %d failed for gpio %d\n",
		__func__, otg->irq_vbus_detect, vbus_gpio);
		return ret;
	}

	ret = enable_irq_wake(otg->irq_vbus_detect);

	return 0;
}

static void __init gokey_switch_initial_setup(void)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;

	if (init_switch_sel & MASK_SWITCH_UART_AP) {
		gokey_ap_uart_actions();
		gokey_otg->uart_manual_mode = GOKEY_MANUAL_UART_AP;
	} else {
		gokey_factory_uart_actions();
		gokey_otg->uart_manual_mode = GOKEY_MANUAL_UART_FACTORY;
	}

	gokey_otg->usb_manual_mode = GOKEY_MANUAL_USB_AP;
}

static int __init gokey_save_init_switch_param(char *str)
{
	int ret;

	ret = kstrtoint(str, 10, &init_switch_sel);
	if (ret < 0)
		return ret;

	return 0;
}

__setup("switch_sel=", gokey_save_init_switch_param);

void __init omap4_gokey_connector_init(void)
{
	struct omap4_otg *gokey_otg = &gokey_otg_xceiv;
	int ret;

	/* Disable PM for UART when JIG is plugged. Otherwise UART3 RX buffer
	 * will be corrupted and AT Command doesn't work correctly.
	 */
	uart_l3_cstr_flag = true;
	device_isupdated = false;
	usb_attached = false;

	gokey_uart_sw_gpio_init();
	gokey_connector_gpio_init();
	gokey_fsa9480_gpio_init();

	mutex_init(&gokey_otg->lock);

	INIT_WORK(&gokey_otg->set_vbus_work, gokey_otg_work);
	INIT_DELAYED_WORK(&gokey_otg->vbus_detection_work, vbus_detect_work);

	device_initialize(&gokey_otg->dev);
	dev_set_name(&gokey_otg->dev, "%s", "gokey_otg");

	ret = device_add(&gokey_otg->dev);
	if (ret) {
		pr_err("%s: cannot reg device '%s' (%d)\n", __func__,
		       dev_name(&gokey_otg->dev), ret);
		return;
	}

	dev_set_drvdata(&gokey_otg->dev, gokey_otg);

	gokey_otg->otg.dev = &gokey_otg->dev;
	gokey_otg->otg.label = "gokey_otg_xceiv";
	gokey_otg->otg.set_host = gokey_otg_set_host;
	gokey_otg->otg.set_peripheral = gokey_otg_set_peripheral;
	gokey_otg->otg.set_suspend = gokey_otg_set_suspend;
	gokey_otg->otg.init = gokey_otg_phy_init;
	gokey_otg->otg.shutdown = gokey_otg_phy_shutdown;
	gokey_otg->otg.is_active = gokey_otg_is_active;

	ATOMIC_INIT_NOTIFIER_HEAD(&gokey_otg->otg.notifier);

	ret = otg_set_transceiver(&gokey_otg->otg);
	if (ret)
		pr_err("gokey_otg: cannot set transceiver (%d)\n", ret);

	omap4430_phy_init(&gokey_otg->dev);
	omap4430_phy_init_for_eyediagram(SWCAP_TRIM_OFFSET, 0, 0);
	omap4430_phy_init_for_eyediagram_ref_gen_test(REF_GEN_TEST);
	gokey_otg_set_suspend(&gokey_otg->otg, 0);
	gokey_vbus_detected_init(gokey_otg);

	sec_switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(sec_switch_dev)) {
		pr_err("(%s): failed to created device (switch_dev)!\n",
								__func__);
	} else {
		ret = sysfs_create_group(&sec_switch_dev->kobj,
					 &manual_switch_sel_group);
		if (ret < 0)
			pr_err("fail to create manual switch_sel sysfs group"
				"(%d)\n", ret);
	}

	gokey_switch_initial_setup();

	i2c_register_board_info(4, gokey_connector_i2c4_boardinfo,
				ARRAY_SIZE(gokey_connector_i2c4_boardinfo));

	/*
	 * Switch device name is defined as dock and usb_audio at the Platform
	 * side. We have to match these names up together.
	 */

	gokey_otg->dock_switch.name = "dock";
	switch_dev_register(&gokey_otg->dock_switch);

	gokey_otg->audio_switch.name = "usb_audio";
	switch_dev_register(&gokey_otg->audio_switch);

	gokey_otg->current_device = 0;
}

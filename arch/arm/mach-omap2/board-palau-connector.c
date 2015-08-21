/* arch/arm/mach-omap2/board-palau-connector.c
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

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/switch.h>
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/otg.h>
#include <linux/irq.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif

#include "mux.h"
#include "omap_muxtbl.h"
#include "board-palau.h"

#define MASK_SWITCH_USB_AP	BIT(0)
#define MASK_SWITCH_UART_AP	BIT(1)

#define NAME_USB_PATH_AP	"PDA"
#define NAME_USB_PATH_CP	"MODEM"
#define NAME_USB_PATH_NONE	"NONE"
#define NAME_UART_PATH_AP	"AP"
#define NAME_UART_PATH_CP	"CP"
#define NAME_UART_PATH_NONE	"NONE"

#define DEFAULT_UART_SEL_CP	0
#define DEFAULT_USB_SEL_AP	1

struct  omap4_otg {
	struct otg_transceiver otg;
	struct device dev;

	struct regulator *vusb;
	struct work_struct set_vbus_work;

	struct mutex vusb_lock;
	struct mutex vbus_drive_lock;
	int current_accessory;
	struct switch_dev dock_switch;
	struct device *switch_dev;

	bool reg_on;
	bool vbus_on;
	bool need_vbus_drive;

#ifdef CONFIG_USB_HOST_NOTIFY
	struct host_notifier_platform_data *pdata;
#endif
};

enum {
	GPIO_JIG_ON = 0,
};

enum {
	GPIO_USB_PWR_EN = 0,
};

static struct gpio connector_gpios[] = {
	[GPIO_JIG_ON] = {
		.flags	= GPIOF_IN,
		.label	= "JIG_ON_18",
	},
};

static struct gpio usb_gpios[] = {
	[GPIO_USB_PWR_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label = "USB_PWR_EN",
	},
};

static struct omap4_otg palau_otg_xceiv;

static ssize_t palau_usb_sel_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int usb_sel;
	const char *mode;

	usb_sel = max77693_muic_get_usb_sel();

	switch (usb_sel) {
	case USB_SEL_AP:
		mode = NAME_USB_PATH_AP;
		break;
	case USB_SEL_CP:
		mode = NAME_USB_PATH_CP;
		break;
	default:
		mode = NAME_USB_PATH_NONE;
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t palau_usb_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	if (!strncasecmp(buf, NAME_USB_PATH_AP, strlen(NAME_USB_PATH_AP)))
		max77693_muic_set_usb_sel(USB_SEL_AP);
	else if (!strncasecmp(buf, NAME_USB_PATH_CP, strlen(NAME_USB_PATH_CP)))
		max77693_muic_set_usb_sel(USB_SEL_CP);
	else
		pr_err("%s: input '%s' or '%s'",
				buf, NAME_USB_PATH_AP, NAME_USB_PATH_CP);

	return size;
}

static ssize_t palau_uart_sel_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int uart_sel;
	const char *mode;

	uart_sel = max77693_muic_get_uart_sel();

	switch (uart_sel) {
	case UART_SEL_AP:
		mode = NAME_UART_PATH_AP;
		break;
	case UART_SEL_CP:
		mode = NAME_UART_PATH_CP;
		break;
	default:
		mode = NAME_UART_PATH_NONE;
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t palau_uart_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	if (!strncasecmp(buf, NAME_UART_PATH_AP, strlen(NAME_UART_PATH_AP)))
		max77693_muic_set_uart_sel(USB_SEL_AP);
	else if (!strncasecmp(buf, NAME_UART_PATH_CP,
				strlen(NAME_UART_PATH_CP)))
		max77693_muic_set_uart_sel(USB_SEL_CP);
	else
		pr_err("%s: input '%s' or '%s'",
				buf, NAME_UART_PATH_AP, NAME_UART_PATH_CP);

	return size;
}

static ssize_t palau_usb_state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct omap4_otg *palau_otg = dev_get_drvdata(dev);
	const char *mode;

	/* current accessory information is supposed to be bitmap */
	/* to be changed */

	if (palau_otg->current_accessory == CABLE_TYPE_USB_MUIC ||
			palau_otg->current_accessory == CABLE_TYPE_JIG_USB)
		mode = "USB_STATE_CONFIGURED";
	else
		mode = "USB_STATE_NOT_CONFIGURED";

	return sprintf(buf, "%s\n", mode);
}

static ssize_t palau_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int adc;

	adc = max77693_muic_get_status1_adc_value();
	pr_info("adc value = %d\n", adc);

	return sprintf(buf, "%x\n", adc);
}

static DEVICE_ATTR(usb_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			palau_usb_sel_show, palau_usb_sel_store);
static DEVICE_ATTR(uart_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			palau_uart_sel_show, palau_uart_sel_store);
static DEVICE_ATTR(usb_state, S_IRUGO, palau_usb_state_show, NULL);
static DEVICE_ATTR(adc, S_IRUSR | S_IRGRP, palau_adc_show, NULL);

static struct attribute *manual_switch_sel_attributes[] = {
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_adc.attr,
	NULL,
};

static const struct attribute_group manual_switch_sel_group = {
	.attrs	= manual_switch_sel_attributes,
};

static int palau_otg_set_host(struct otg_transceiver *otg,
				 struct usb_bus *host)
{
	otg->host = host;
	if (!host)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static int palau_otg_set_peripheral(struct otg_transceiver *otg,
				       struct usb_gadget *gadget)
{
	otg->gadget = gadget;
	if (!gadget)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static int palau_vusb_enable(struct omap4_otg *otg, bool enable)
{
	int ret = 0;
	int pwr_en_gpio = 0;
	pr_info("[%s] enable=%d\n", __func__, enable);

	mutex_lock(&otg->vusb_lock);

	if (otg->reg_on != enable) {
		pwr_en_gpio = usb_gpios[GPIO_USB_PWR_EN].gpio;

		if (unlikely(pwr_en_gpio == -EINVAL))
			pr_err("%s gpio error. value is %d\n",
				__func__, pwr_en_gpio);
		else {
			gpio_set_value(pwr_en_gpio, (int)!!enable);
			otg->reg_on = enable;
		}
	} else
		pr_err("%s error. already set\n", __func__);

	mutex_unlock(&otg->vusb_lock);

	return ret;
}

static void palau_set_vbus_drive(struct omap4_otg *otg, bool enable)
{
	pr_info("[%s] enable=%d\n", __func__, enable);

	mutex_lock(&otg->vbus_drive_lock);

	if (otg->vbus_on != enable) {
		if (enable)
			otg_control(1);
		else
			otg_control(0);

		otg->vbus_on = enable;
	} else
		pr_err("%s error. already set\n", __func__);

	mutex_unlock(&otg->vbus_drive_lock);
	return;
}

static void palau_otg_work(struct work_struct *data)
{
	struct omap4_otg *palau_otg =
		container_of(data, struct omap4_otg, set_vbus_work);

	palau_set_vbus_drive(palau_otg, palau_otg->need_vbus_drive);
}

static int palau_otg_set_vbus(struct otg_transceiver *otg, bool enabled)
{
	struct omap4_otg *palau_otg =
	    container_of(otg, struct omap4_otg, otg);
	dev_info(otg->dev, "%s %s\n", __func__, enabled ? "on" : "off");

	palau_otg->need_vbus_drive = enabled;
	schedule_work(&palau_otg->set_vbus_work);

	return 0;
}

static int palau_otg_phy_init(struct otg_transceiver *otg)
{
	dev_info(otg->dev, "%s last_event=%d\n", __func__, otg->last_event);
	if (otg->last_event == USB_EVENT_ID)
		omap4430_phy_power(otg->dev, 1, 1);
	else
		omap4430_phy_power(otg->dev, 0, 1);
	return 0;
}

static void palau_otg_phy_shutdown(struct otg_transceiver *otg)
{
	struct omap4_otg *palau_otg =
	    container_of(otg, struct omap4_otg, otg);
	dev_info(otg->dev, "%s\n", __func__);

	omap4430_phy_power(otg->dev, 0, 0);
	palau_vusb_enable(palau_otg, false);
}

static int palau_otg_set_suspend(struct otg_transceiver *otg, int suspend)
{
	dev_info(otg->dev, "%s = %d\n", __func__, suspend);
	return omap4430_phy_suspend(otg->dev, suspend);
}

static int palau_otg_is_active(struct otg_transceiver *otg)
{
	return omap4430_phy_is_active(otg->dev);
}

static int palau_set_safeout(int path)
{
	return 0;
}

static void palau_init_cb(void)
{
}

static void palau_ap_usb_attach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);

	palau_vusb_enable(otg, true);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_VBUS;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_VBUS,
			otg->otg.gadget);
}

static void palau_ap_usb_detach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_NONE;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_NONE,
			otg->otg.gadget);
}

static void palau_usb_host_attach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);

#ifdef CONFIG_USB_HOST_NOTIFY
	if (otg->pdata && otg->pdata->usbhostd_start) {
		otg->pdata->ndev.mode = NOTIFY_HOST_MODE;
		otg->pdata->usbhostd_start();
	}
#endif

	palau_vusb_enable(otg, true);

	otg->otg.state = OTG_STATE_A_IDLE;
	otg->otg.default_a = true;
	otg->otg.last_event = USB_EVENT_ID;

	atomic_notifier_call_chain(&otg->otg.notifier,
				   USB_EVENT_ID, otg->otg.gadget);
	return;
}

static void palau_usb_host_detach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);

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
				   USB_EVENT_HOST_NONE, otg->otg.gadget);
	return;
}

static void palau_detected(int cable, bool attach)
{
	struct omap4_otg *palau_otg = &palau_otg_xceiv;

	pr_info("[%s] cable=%d, attach=%d, last_event=%d",
		__func__, cable, attach, palau_otg->otg.last_event);

	switch (cable) {
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB:
		if (attach) {
			if (palau_otg->otg.last_event == USB_EVENT_NONE)
				palau_ap_usb_attach(palau_otg);
		} else {
			if (palau_otg->otg.last_event != USB_EVENT_NONE)
				palau_ap_usb_detach(palau_otg);
		}
		break;
	case CABLE_TYPE_OTG_MUIC:
		if (attach)
			palau_usb_host_attach(palau_otg);
		else
			palau_usb_host_detach(palau_otg);
		break;
	case CABLE_TYPE_CHARGER:
	case CABLE_TYPE_TA_MUIC:
		break;
	case CABLE_TYPE_DESKDOCK_MUIC:
		break;
	case CABLE_TYPE_CARDOCK_MUIC:
		break;
	case CABLE_TYPE_JIG_UART:
		break;
	case CABLE_TYPE_MHL_MUIC:
		break;
	case CABLE_TYPE_SMARTDOCK_MUIC:
		break;
	case CABLE_TYPE_UNKNOWN_MUIC:
		break;
	case CABLE_TYPE_NONE_MUIC:
	default:
		break;
	}

	if (attach)
		palau_otg->current_accessory = cable;
	else
		palau_otg->current_accessory = CABLE_TYPE_NONE_MUIC;
}

struct max77693_muic_data max77693_muic = {
	.init_cb		= palau_init_cb,
	.set_safeout		= palau_set_safeout,
	.detected		= palau_detected,
	.usb_sel		= DEFAULT_USB_SEL_AP,
	.uart_sel		= DEFAULT_UART_SEL_CP,
};

static int __init palau_save_init_switch_param(char *str)
{
	int init_switch_sel;
	int ret;

	ret = kstrtoint(str, 10, &init_switch_sel);
	if (ret < 0)
		return ret;

	if (init_switch_sel & MASK_SWITCH_USB_AP)
		max77693_muic.usb_sel = USB_SEL_AP;
	else
		max77693_muic.usb_sel = USB_SEL_CP;

	if (init_switch_sel & MASK_SWITCH_UART_AP)
		max77693_muic.uart_sel = UART_SEL_AP;
	else
		max77693_muic.uart_sel = UART_SEL_CP;

	return 0;
}
__setup("switch_sel=", palau_save_init_switch_param);

#ifdef CONFIG_USB_HOST_NOTIFY
static void palau_booster(int enable)
{
	struct otg_transceiver *otg_t;

	pr_info("[%s] enable=%d\n", __func__, enable);

	otg_t = otg_get_transceiver();
	if (!otg_t) {
		pr_err("[%s] otg tranceiver is NULL", __func__);
		return;
	}

	palau_otg_set_vbus(otg_t, !!enable);
	return;
}

struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name     = "usb_otg",
	.booster       = palau_booster,
	.thread_enable = 0,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};

static void palau_host_notifier_init(struct omap4_otg *otg)
{
	int acc_out =
	omap_muxtbl_get_gpio_by_name("VBUS_DETECT");
	if (unlikely(acc_out == -EINVAL))
		dev_err(&otg->dev, "VBUS_DETECT is invalid.\n");
	host_notifier_pdata.gpio = acc_out;
	host_notifier_pdata.inverse_vbus_trigger = 1;
	otg->pdata = &host_notifier_pdata;
	platform_device_register(&host_notifier_device);
}
#endif

static void palau_connector_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(connector_gpios); i++)
		connector_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(connector_gpios[i].label);

	gpio_request_array(connector_gpios, ARRAY_SIZE(connector_gpios));
}

static void palau_usb_gpio_init(void)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(usb_gpios); i++) {
		usb_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(usb_gpios[i].label);
		ret = gpio_request_one(usb_gpios[i].gpio,
			usb_gpios[i].flags, usb_gpios[i].label);
		if (ret)
			pr_err("%s %s error gpio=%d\n", __func__
				, usb_gpios[i].label, usb_gpios[i].gpio);
	}
}

void __init omap4_palau_connector_init(void)
{
	struct omap4_otg *palau_otg = &palau_otg_xceiv;
	int ret;

	pr_info("[%s]\n", __func__);

	palau_connector_gpio_init();
	palau_usb_gpio_init();
	mutex_init(&palau_otg->vusb_lock);
	mutex_init(&palau_otg->vbus_drive_lock);
	INIT_WORK(&palau_otg->set_vbus_work, palau_otg_work);

	device_initialize(&palau_otg->dev);
	dev_set_name(&palau_otg->dev, "%s", "palau_otg");
	ret = device_add(&palau_otg->dev);
	if (ret) {
		pr_err("%s: cannot reg device '%s' (%d)\n", __func__,
		       dev_name(&palau_otg->dev), ret);
		return;
	}

	dev_set_drvdata(&palau_otg->dev, palau_otg);

	palau_otg->otg.dev			= &palau_otg->dev;
	palau_otg->otg.label			= "palau_otg_xceiv";
	palau_otg->otg.set_host		= palau_otg_set_host;
	palau_otg->otg.set_peripheral	= palau_otg_set_peripheral;
	palau_otg->otg.set_suspend		= palau_otg_set_suspend;
	palau_otg->otg.set_vbus		= palau_otg_set_vbus;
	palau_otg->otg.init			= palau_otg_phy_init;
	palau_otg->otg.shutdown		= palau_otg_phy_shutdown;
	palau_otg->otg.is_active		= palau_otg_is_active;

	ATOMIC_INIT_NOTIFIER_HEAD(&palau_otg->otg.notifier);

	ret = otg_set_transceiver(&palau_otg->otg);
	if (ret)
		pr_err("palau_otg: cannot set transceiver (%d)\n", ret);

	palau_otg->switch_dev =
			device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(palau_otg->switch_dev)) {
		pr_err("(%s): failed to created device (switch_dev)!\n",
								__func__);
		goto switch_dev_fail;
	}

	omap4430_phy_init(&palau_otg->dev);
	palau_otg_set_suspend(&palau_otg->otg, 0);
#ifdef CONFIG_USB_HOST_NOTIFY
	palau_host_notifier_init(palau_otg);
#endif

	dev_set_drvdata(palau_otg->switch_dev, palau_otg);
	ret = sysfs_create_group(&palau_otg->switch_dev->kobj,
						&manual_switch_sel_group);
	if (ret < 0)
		pr_err("fail to  create switch_sel sysfs group (%d)\n", ret);

switch_dev_fail:

	palau_otg->dock_switch.name = "dock";
	switch_dev_register(&palau_otg->dock_switch);
}

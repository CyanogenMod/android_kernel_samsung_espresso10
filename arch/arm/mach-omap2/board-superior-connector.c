/* arch/arm/mach-omap2/board-superior-connector.c
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
#include <linux/power_supply.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif

#include "mux.h"
#include "omap_muxtbl.h"
#include "board-superior.h"
#include "omap_phy_tune.c"

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

/* 0x4E(default) + 0x22(SWCAP_TRIM_OFFSET) = 0xF0*/
#define SWCAP_TRIM_OFFSET			(0x02 - 0x4E)

static const char const *cable_names[] = {
	[CABLE_TYPE_NONE_MUIC]		= "none",
	[CABLE_TYPE_USB_MUIC]		= "usb",
	[CABLE_TYPE_OTG_MUIC]		= "otg",
	[CABLE_TYPE_TA_MUIC]		= "ta",
	[CABLE_TYPE_DESKDOCK_MUIC]	= "deskdock",
	[CABLE_TYPE_DESKDOCK_WITH_TA]	= "deskdock-ta",
	[CABLE_TYPE_CARDOCK_MUIC]	= "cardock",
	[CABLE_TYPE_JIG_UART]		= "jig-uart",
	[CABLE_TYPE_JIG_USB]		= "jig-usb",
	[CABLE_TYPE_MHL_MUIC]		= "mhl",
	[CABLE_TYPE_SMARTDOCK_MUIC]	= "smartdock",
	[CABLE_TYPE_SMARTDOCK_WITH_TA]	= "smartdock-ta",
	[CABLE_TYPE_UNKNOWN_MUIC]	= "unknown",
	[CABLE_TYPE_CHARGER]		= "charger",
};


static struct power_supply *sec_battery;

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
	SUPERIOR_DOCK_NONE = 0,
	SUPERIOR_DOCK_DESK,
	SUPERIOR_DOCK_CAR,
};

enum {
	GPIO_JIG_ON = 0,
};

enum {
	GPIO_USB_PWR_EN = 0,
};

#if defined(CONFIG_MACH_SAMSUNG_SUPERIOR_CHN_CMCC)
enum {
	GPIO_AP_CP_INT1 = 0,
};
#endif

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

#if defined(CONFIG_MACH_SAMSUNG_SUPERIOR_CHN_CMCC)
static struct gpio ap_cp_int1_gpios[] = {
	[GPIO_AP_CP_INT1] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label = "AP_CP_INT1",
	},
};
#endif

static struct omap4_otg superior_otg_xceiv;

static ssize_t superior_usb_sel_show(struct device *dev,
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

static ssize_t superior_usb_sel_store(struct device *dev,
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

static ssize_t superior_uart_sel_show(struct device *dev,
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

static ssize_t superior_uart_sel_store(struct device *dev,
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

static ssize_t superior_usb_state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct omap4_otg *superior_otg = dev_get_drvdata(dev);
	const char *mode;

	/* current accessory information is supposed to be bitmap */
	/* to be changed */

	if (superior_otg->current_accessory == CABLE_TYPE_USB_MUIC ||
			superior_otg->current_accessory == CABLE_TYPE_JIG_USB)
		mode = "USB_STATE_CONFIGURED";
	else
		mode = "USB_STATE_NOT_CONFIGURED";

	return sprintf(buf, "%s\n", mode);
}

static ssize_t superior_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int adc;

	adc = max77693_muic_get_status1_adc_value();
	pr_info("adc value = %d\n", adc);

	return sprintf(buf, "%x\n", adc);
}

static ssize_t superior_jig_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct omap4_otg *superior_otg = dev_get_drvdata(dev);
	const char *mode;

	if (superior_otg->current_accessory == CABLE_TYPE_JIG_UART ||
			superior_otg->current_accessory == CABLE_TYPE_JIG_USB)
		mode = "1";
	else
		mode = "0";

	return sprintf(buf, "%s\n", mode);
}

static ssize_t superior_accessory_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct omap4_otg *superior_otg = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n",
			cable_names[superior_otg->current_accessory]);
}

static DEVICE_ATTR(usb_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			superior_usb_sel_show, superior_usb_sel_store);
static DEVICE_ATTR(uart_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			superior_uart_sel_show, superior_uart_sel_store);
static DEVICE_ATTR(usb_state, S_IRUGO, superior_usb_state_show, NULL);
static DEVICE_ATTR(adc, S_IRUSR | S_IRGRP, superior_adc_show, NULL);
static DEVICE_ATTR(jig_on, S_IRUSR | S_IRGRP, superior_jig_on_show, NULL);
static DEVICE_ATTR(accessory, S_IRUGO, superior_accessory_show, NULL);

static struct attribute *manual_switch_sel_attributes[] = {
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_adc.attr,
	&dev_attr_jig_on.attr,
	&dev_attr_accessory.attr,
	NULL,
};

static const struct attribute_group manual_switch_sel_group = {
	.attrs	= manual_switch_sel_attributes,
};

static int superior_otg_set_host(struct otg_transceiver *otg,
				 struct usb_bus *host)
{
	otg->host = host;
	if (!host)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static int superior_otg_set_peripheral(struct otg_transceiver *otg,
				       struct usb_gadget *gadget)
{
	otg->gadget = gadget;
	if (!gadget)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static int superior_vusb_enable(struct omap4_otg *otg, bool enable)
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

static void superior_set_vbus_drive(struct omap4_otg *otg, bool enable)
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

static void superior_otg_work(struct work_struct *data)
{
	struct omap4_otg *superior_otg =
		container_of(data, struct omap4_otg, set_vbus_work);

	superior_set_vbus_drive(superior_otg, superior_otg->need_vbus_drive);
}

static int superior_otg_set_vbus(struct otg_transceiver *otg, bool enabled)
{
	struct omap4_otg *superior_otg =
	    container_of(otg, struct omap4_otg, otg);
	dev_info(otg->dev, "%s %s\n", __func__, enabled ? "on" : "off");

	superior_otg->need_vbus_drive = enabled;
	schedule_work(&superior_otg->set_vbus_work);

	return 0;
}

static int superior_otg_phy_init(struct otg_transceiver *otg)
{
	dev_info(otg->dev, "%s last_event=%d\n", __func__, otg->last_event);
	if (otg->last_event == USB_EVENT_ID)
		omap4430_phy_power(otg->dev, 1, 1);
	else
		omap4430_phy_power(otg->dev, 0, 1);
	return 0;
}

static void superior_otg_phy_shutdown(struct otg_transceiver *otg)
{
	struct omap4_otg *superior_otg =
	    container_of(otg, struct omap4_otg, otg);
	dev_info(otg->dev, "%s\n", __func__);

	omap4430_phy_power(otg->dev, 0, 0);
	superior_vusb_enable(superior_otg, false);
}

static int superior_otg_set_suspend(struct otg_transceiver *otg, int suspend)
{
	dev_info(otg->dev, "%s = %d\n", __func__, suspend);
	return omap4430_phy_suspend(otg->dev, suspend);
}

static int superior_otg_is_active(struct otg_transceiver *otg)
{
	return omap4430_phy_is_active(otg->dev);
}

static int superior_otg_vbus_reset(struct otg_transceiver *otg)
{
	struct omap4_otg *superior_otg =
	    container_of(otg, struct omap4_otg, otg);
#ifdef CONFIG_USB_HOST_NOTIFY
	host_notify_set_ovc_en(&superior_otg->pdata->ndev, NOTIFY_SET_OFF);
	superior_otg->pdata->ndev.booster = NOTIFY_POWER_OFF;
#endif
	superior_otg_set_vbus(otg, 0);
#ifdef CONFIG_USB_HOST_NOTIFY
	if (superior_otg->pdata->ndev.mode == NOTIFY_HOST_MODE)
#endif
		superior_otg_set_vbus(otg, 1);
#ifdef CONFIG_USB_HOST_NOTIFY
	host_notify_set_ovc_en(&superior_otg->pdata->ndev, NOTIFY_SET_ON);
#endif
	return 0;
}

static int superior_set_safeout(int path)
{
	struct regulator *regulator;

	/* Superior board support only safeout2 ldo for CP USB. */
	regulator = regulator_get(NULL, "safeout2");	/* USB_VBUS_CP_4.9V */
	if (IS_ERR(regulator))
		return -ENODEV;

	if (path == PATH_USB_CP) {
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	} else {
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
	}

	regulator_put(regulator);

	return 0;
}

static void superior_ap_usb_attach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);

	superior_vusb_enable(otg, true);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_VBUS;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_VBUS,
			otg->otg.gadget);
}

static void superior_ap_usb_detach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_NONE;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_NONE,
			otg->otg.gadget);
}

static void superior_usb_host_attach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);

#ifdef CONFIG_USB_HOST_NOTIFY
	if (otg->pdata && otg->pdata->usbhostd_start) {
		otg->pdata->ndev.mode = NOTIFY_HOST_MODE;
		otg->pdata->usbhostd_start();
	}
#endif

	superior_vusb_enable(otg, true);

	otg->otg.state = OTG_STATE_A_IDLE;
	otg->otg.default_a = true;
	otg->otg.last_event = USB_EVENT_ID;

	atomic_notifier_call_chain(&otg->otg.notifier,
				   USB_EVENT_ID, otg->otg.gadget);
	return;
}

static void superior_usb_host_detach(struct omap4_otg *otg)
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

static void superior_detected_usb(struct omap4_otg *otg, bool attach)
{
	union power_supply_propval value;
	int ret;

	if (attach) {
		if (otg->otg.last_event == USB_EVENT_NONE)
			superior_ap_usb_attach(otg);

		value.intval = POWER_SUPPLY_TYPE_USB;
	} else {
		if (otg->otg.last_event != USB_EVENT_NONE)
			superior_ap_usb_detach(otg);
		value.intval = POWER_SUPPLY_TYPE_BATTERY;
	}

	if (!sec_battery)
		sec_battery = power_supply_get_by_name("battery");

	if (!sec_battery)
		pr_err("%s: failed to get sec_battery power supply\n",
				__func__);

	if (sec_battery) {
		ret = sec_battery->set_property(sec_battery,
				POWER_SUPPLY_PROP_ONLINE,
				&value);
		if (ret)
			pr_err("%s:failed to set online mode\n",
					__func__);
	}
}

static void superior_detected_otg(struct omap4_otg *otg, bool attach)
{
	if (attach)
		superior_usb_host_attach(otg);
	else
		superior_usb_host_detach(otg);
}

static void superior_detected_ta(struct omap4_otg *otg, bool attach)
{
	union power_supply_propval value;
	int ret;

	value.intval = attach ? POWER_SUPPLY_TYPE_MAINS :
		POWER_SUPPLY_TYPE_BATTERY;
	if (sec_battery) {
		ret = sec_battery->set_property(sec_battery,
				POWER_SUPPLY_PROP_ONLINE,
				&value);
		if (ret)
			pr_err("%s:failed to set online mode\n",
					__func__);
	}
}

static void superior_detected_deskdock(struct omap4_otg *otg, bool attach)
{
	if (attach)
		switch_set_state(&otg->dock_switch, SUPERIOR_DOCK_DESK);
	else
		switch_set_state(&otg->dock_switch, SUPERIOR_DOCK_NONE);
}

static void superior_detected_cardock(struct omap4_otg *otg, bool attach)
{
	if (attach)
		switch_set_state(&otg->dock_switch, SUPERIOR_DOCK_CAR);
	else
		switch_set_state(&otg->dock_switch, SUPERIOR_DOCK_NONE);
}

static void superior_detected_jig(struct omap4_otg *otg, bool attach)
{
}

static void superior_detected(int cable, bool attach)
{
	struct omap4_otg *otg = &superior_otg_xceiv;
	int ret;

	pr_info("[%s] cable detected: %s %s, last event=%d\n", __func__,
			cable_names[cable], (attach) ? "attach" : "detach",
			otg->otg.last_event);

#if defined(CONFIG_MACH_SAMSUNG_SUPERIOR_CHN_CMCC)
	if (attach) {
		if (cable == CABLE_TYPE_JIG_UART)
			gpio_set_value(ap_cp_int1_gpios[GPIO_AP_CP_INT1].gpio,
					1);
		pr_info("%s : AP_CP_INT1 = HIGH\n", __func__);
	} else {
		if (gpio_get_value(ap_cp_int1_gpios[GPIO_AP_CP_INT1].gpio) == 1)
			gpio_set_value(ap_cp_int1_gpios[GPIO_AP_CP_INT1].gpio,
					0);
		pr_info("%s : AP_CP_INT1 = LOW\n", __func__);
	}
#endif

	switch (cable) {
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB:
		superior_detected_usb(otg, attach);
		break;
	case CABLE_TYPE_OTG_MUIC:
		superior_detected_otg(otg, attach);
		break;
	case CABLE_TYPE_CHARGER:
	case CABLE_TYPE_TA_MUIC:
		superior_detected_ta(otg, attach);
		break;
	case CABLE_TYPE_DESKDOCK_MUIC:
		superior_detected_deskdock(otg, attach);
		break;
	case CABLE_TYPE_DESKDOCK_WITH_TA:
		superior_detected_deskdock(otg, attach);
		superior_detected_ta(otg, attach);
		break;
	case CABLE_TYPE_CARDOCK_MUIC:
		superior_detected_cardock(otg, attach);
		break;
	case CABLE_TYPE_JIG_UART:
		superior_detected_jig(otg, attach);
		break;
	case CABLE_TYPE_SMARTDOCK_MUIC:
		break;
	case CABLE_TYPE_SMARTDOCK_WITH_TA:
		superior_detected_ta(otg, attach);
		break;
	case CABLE_TYPE_UNKNOWN_MUIC:
		break;
	case CABLE_TYPE_NONE_MUIC:
	default:
		pr_err("%s: invalid cable value (%d)\n", __func__, cable);
		return;
	}

	if (attach)
		otg->current_accessory = cable;
	else
		otg->current_accessory = CABLE_TYPE_NONE_MUIC;
}

struct max77693_muic_data max77693_muic = {
	.set_safeout		= superior_set_safeout,
	.detected		= superior_detected,
	.usb_sel		= DEFAULT_USB_SEL_AP,
	.uart_sel		= DEFAULT_UART_SEL_CP,
};

static int __init superior_save_init_switch_param(char *str)
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
__setup("switch_sel=", superior_save_init_switch_param);

#ifdef CONFIG_USB_HOST_NOTIFY
static void superior_booster(int enable)
{
	struct otg_transceiver *otg_t;

	pr_info("[%s] enable=%d\n", __func__, enable);

	otg_t = otg_get_transceiver();
	if (!otg_t) {
		pr_err("[%s] otg tranceiver is NULL", __func__);
		return;
	}

	superior_otg_set_vbus(otg_t, !!enable);
	return;
}

struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name     = "usb_otg",
	.booster       = superior_booster,
	.thread_enable = 0,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};

static void superior_host_notifier_init(struct omap4_otg *otg)
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

static void superior_connector_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(connector_gpios); i++)
		connector_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(connector_gpios[i].label);

	gpio_request_array(connector_gpios, ARRAY_SIZE(connector_gpios));
}

static void superior_usb_gpio_init(void)
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

#if defined(CONFIG_MACH_SAMSUNG_SUPERIOR_CHN_CMCC)
static void superior_ap_cp_int1_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ap_cp_int1_gpios); i++)
		ap_cp_int1_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(ap_cp_int1_gpios[i].label);

	gpio_request_array(ap_cp_int1_gpios, ARRAY_SIZE(ap_cp_int1_gpios));
}
#endif

void __init omap4_superior_connector_init(void)
{
	struct omap4_otg *superior_otg = &superior_otg_xceiv;
	int ret;

	pr_info("[%s]", __func__);

	superior_connector_gpio_init();
	superior_usb_gpio_init();
#if defined(CONFIG_MACH_SAMSUNG_SUPERIOR_CHN_CMCC)
	superior_ap_cp_int1_gpio_init();
#endif
	mutex_init(&superior_otg->vusb_lock);
	mutex_init(&superior_otg->vbus_drive_lock);
	INIT_WORK(&superior_otg->set_vbus_work, superior_otg_work);

	device_initialize(&superior_otg->dev);
	dev_set_name(&superior_otg->dev, "%s", "superior_otg");
	ret = device_add(&superior_otg->dev);
	if (ret) {
		pr_err("%s: cannot reg device '%s' (%d)\n", __func__,
		       dev_name(&superior_otg->dev), ret);
		return;
	}

	dev_set_drvdata(&superior_otg->dev, superior_otg);

	superior_otg->otg.dev			= &superior_otg->dev;
	superior_otg->otg.label			= "superior_otg_xceiv";
	superior_otg->otg.set_host		= superior_otg_set_host;
	superior_otg->otg.set_peripheral	= superior_otg_set_peripheral;
	superior_otg->otg.set_suspend		= superior_otg_set_suspend;
	superior_otg->otg.set_vbus		= superior_otg_set_vbus;
	superior_otg->otg.init			= superior_otg_phy_init;
	superior_otg->otg.shutdown		= superior_otg_phy_shutdown;
	superior_otg->otg.is_active		= superior_otg_is_active;
/* start_hnp function is used for vbus reset. */
	superior_otg->otg.start_hnp		= superior_otg_vbus_reset;

	ATOMIC_INIT_NOTIFIER_HEAD(&superior_otg->otg.notifier);

	ret = otg_set_transceiver(&superior_otg->otg);
	if (ret)
		pr_err("superior_otg: cannot set transceiver (%d)\n", ret);

	superior_otg->switch_dev =
			device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(superior_otg->switch_dev)) {
		pr_err("(%s): failed to created device (switch_dev)!\n",
								__func__);
		goto switch_dev_fail;
	}

	omap4430_phy_init(&superior_otg->dev);
	omap4430_phy_init_for_eyediagram(SWCAP_TRIM_OFFSET);
	superior_otg_set_suspend(&superior_otg->otg, 0);
#ifdef CONFIG_USB_HOST_NOTIFY
	superior_host_notifier_init(superior_otg);
#endif

	dev_set_drvdata(superior_otg->switch_dev, superior_otg);
	ret = sysfs_create_group(&superior_otg->switch_dev->kobj,
						&manual_switch_sel_group);
	if (ret < 0)
		pr_err("fail to  create switch_sel sysfs group (%d)\n", ret);

switch_dev_fail:

	superior_otg->dock_switch.name = "dock";
	switch_dev_register(&superior_otg->dock_switch);
}

static int __init omap4_superior_connector_late_init(void)
{
	if (!gpio_get_value(connector_gpios[GPIO_JIG_ON].gpio) &&
	    !strncmp(sec_androidboot_mode, "jig", 3)) {
		uart_set_l3_cstr(true);
		pr_info("conn: UART PM constraint enabled for DFMS\n");
	}

	return 0;
}

late_initcall(omap4_superior_connector_late_init);

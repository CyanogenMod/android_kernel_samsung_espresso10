/* arch/arm/mach-omap2/board-t1-connector.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-tuna-connector.c
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
#include <linux/reboot.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif

#include <plat/usb.h>

#include "board-t1.h"
#include "mux.h"
#include "omap_muxtbl.h"

#include "omap_phy_tune.c"

#define FSA3200_AP_SEL_AP		0
#define FSA3200_MHL_SEL_AP		0
#define FSA3200_AP_SEL_FSA		1
#define FSA3200_MHL_SEL_FSA		0
#define FSA3200_AP_SEL_MHL		1
#define FSA3200_MHL_SEL_MHL		1

#define USB_ID_SEL_FSA			0
#define USB_ID_SEL_MHL			1

#define IF_UART_SEL_DEFAULT		1
#define IF_UART_SEL_AP			1
#define IF_UART_SEL_CP			0

#define T1_MANUAL_USB_NONE		0
#define T1_MANUAL_USB_MODEM		1
#define T1_MANUAL_USB_AP		2

#define T1_MANUAL_UART_NONE		0
#define T1_MANUAL_UART_MODEM		1
#define T1_MANUAL_UART_AP		2

#define T1_OTG_ID_FSA9480_PRIO		INT_MIN
#define T1_OTG_ID_SII9234_PRIO		(INT_MIN + 1)
#define T1_OTG_ID_FSA9480_LAST_PRIO	INT_MAX

#define CHARGERUSB_CTRL1		0x8
#define CHARGERUSB_CTRL3		0xA
#define CHARGERUSB_CINLIMIT		0xE

#define TWL6030_VBUS_IRQ       (TWL6030_IRQ_BASE + USB_PRES_INTR_OFFSET)
#define TWL6030_VBUS_FLAGS     (IRQF_TRIGGER_FALLING | IRQF_ONESHOT)

#define TWL_REG_CONTROLLER_INT_MASK	0x00
#define TWL_CONTROLLER_MVBUS_DET	BIT(1)
#define TWL_CONTROLLER_RSVD		BIT(5)
#define TWL_REG_CONTROLLER_STAT1       0x03
#define TWL_STAT1_VBUS_DET             BIT(2)

#define MASK_SWITCH_USB_AP		0x01
#define MASK_SWITCH_UART_AP		0x02


#define T1_MHL_SWING_LEVEL		0xFA

#define SWCAP_TRIM_OFFSET		(0x30)
#define SWCAP_TRIM_OFFSET_HOST			(0x00)
#define BGTRIM_TRIM_OFFSET_HOST			(0x00)
#define RTERM_RMX_OFFSET_HOST			(0x00)
#define REF_GEN_TEST	0x06
#define REF_GEN_TEST_HOST	0x00
#define RTERM_CAL_OFFSET_HOST			(0x00)
#define HS_CODE_SEL_HOST	(0x7)
#define SQ_OFF_CODE_DAC3_OFFSET_HOST	(0x3)

#if !defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
static int otg_en;
#endif
static void *fsa9480_usbsw;
static bool boot_jig;

struct t1_otg {
	struct otg_transceiver	otg;
	struct device		dev;

	struct regulator	*vusb;
	struct work_struct	set_vbus_work;
	struct delayed_work	usb_disconn_w;
	struct mutex		lock;

	bool			reg_on;
	bool			vbus_on;
	bool			need_vbus_drive;
	bool			usb_phy_suspend_lock;
	int			usb_manual_mode;
	int			uart_manual_mode;
	int			current_device;
	int                     gpio_ta_nconnected;
	int                     irq_ta_nconnected;
	bool			jig_uart_connected;
	bool                    car_dock_connected;
	u8                      desk_dock_charger_status;

	struct switch_dev	dock_switch;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct host_notifier_platform_data *pdata;
#endif
};
static struct t1_otg t1_otg_xceiv;

#define USB_PHY_SUSPEND_LOCK()		\
do {		\
	t1_otg_xceiv.usb_phy_suspend_lock = 1;	\
} while (0)

#define USB_PHY_SUSPEND_UNLOCK()	\
do {		\
	t1_otg_xceiv.usb_phy_suspend_lock = 0;	\
} while (0)

#define IS_USB_PHY_SUSPEND_LOCK()	\
	(t1_otg_xceiv.usb_phy_suspend_lock == 1 ? 1 : 0)

enum {
	T1_DESKDOCK_CHARGER_DEFAULT = 0,
	T1_CHARGER_CONNECTED_TO_DESKDOCK,
	T1_CHARGER_REMOVED_FROM_DESKDOCK,
};
enum {
	T1_DOCK_NONE = 0,
	T1_DOCK_DESK,
	T1_DOCK_CAR,
};

static int init_switch_sel;

enum {
	T1_USB_MUX_FSA = 0,
	T1_USB_MUX_MHL,
	NUM_T1_USB_MUX,

	T1_USB_MUX_DEFAULT = T1_USB_MUX_FSA,
};

static struct {
	int mhl_sel;
} t1_fsa3200_mux_states[] = {
	[T1_USB_MUX_FSA] = { FSA3200_MHL_SEL_FSA },
	[T1_USB_MUX_MHL] = { FSA3200_MHL_SEL_MHL },
};

enum {
	GPIO_CP_USB_ON = 0,
	GPIO_UART_SEL,
#if defined(CONFIG_MACH_SAMSUNG_T1_CHN_CMCC)
	GPIO_AP_CP_INT1
#endif
};

static struct gpio uart_sw_gpios[] = {
	[GPIO_CP_USB_ON] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "CP_USB_ON",
	},
	[GPIO_UART_SEL] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "UART_SEL",
	},
#if defined(CONFIG_MACH_SAMSUNG_T1_CHN_CMCC)
	[GPIO_AP_CP_INT1] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "AP_CP_INT1",
	 }
#endif
};

enum {
	GPIO_MHL_SEL = 0,
	GPIO_MHL_RST,
	GPIO_MHL_INT,
	GPIO_HDMI_EN,
	GPIO_HDMI_HPD
};

static struct gpio mhl_gpios[] = {
	[GPIO_MHL_SEL] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "MHL_SEL",
	},
	[GPIO_MHL_RST] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "MHL_RST",
	},
	[GPIO_MHL_INT] = {
		.flags	= GPIOF_IN,
		.label	= "MHL_INT",
	},
	[GPIO_HDMI_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "HDMI_EN",
	},
	[GPIO_HDMI_HPD] = {
		.flags	= GPIOF_IN,
		.label	= "HDMI_HPD",
	},
};

static ssize_t t1_otg_usb_sel_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf);
static ssize_t t1_otg_usb_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size);
static ssize_t t1_otg_uart_switch_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);
static ssize_t t1_otg_uart_switch_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size);
static ssize_t t1_usb_state_show(struct device *dev,
				struct device_attribute *attr,
				char *buf);

static DEVICE_ATTR(usb_sel, S_IRUSR | S_IWUSR,
		   t1_otg_usb_sel_show, t1_otg_usb_sel_store);
static DEVICE_ATTR(uart_sel, S_IRUSR | S_IWUSR,
		   t1_otg_uart_switch_show, t1_otg_uart_switch_store);
static DEVICE_ATTR(usb_state, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			t1_usb_state_show, NULL);
static struct attribute *manual_switch_sel_attributes[] = {
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_state.attr,
	NULL,
};

static const struct attribute_group manual_switch_sel_group = {
	.attrs = manual_switch_sel_attributes,
};

static bool t1_twl_chgctrl_init;
static void t1_fsa3200_mux(int state)
{
	BUG_ON(state >= NUM_T1_USB_MUX);

	pr_debug("mux to %d\n", state);
	gpio_direction_output(mhl_gpios[GPIO_MHL_SEL].gpio,
			      t1_fsa3200_mux_states[state].mhl_sel);
}

static void t1_mux_usb_to_fsa(bool enable)
{
	t1_fsa3200_mux(enable ? T1_USB_MUX_FSA : T1_USB_MUX_DEFAULT);
}

static void t1_mux_usb_to_mhl(bool enable)
{
	t1_fsa3200_mux(enable ? T1_USB_MUX_MHL : T1_USB_MUX_DEFAULT);
}

static void t1_otg_set_dock_switch(int state)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;

	switch_set_state(&t1_otg->dock_switch, state);
}

static void t1_vusb_enable(struct t1_otg *t1_otg, bool enable)
{
	pr_info("[%s] enable=%d\n", __func__, enable);

	if (t1_otg->reg_on  != enable) {
		/* delay getting the regulator until later */
		if (IS_ERR_OR_NULL(t1_otg->vusb)) {
			t1_otg->vusb = regulator_get(&t1_otg->dev, "vusb");
			if (IS_ERR(t1_otg->vusb)) {
				dev_err(&t1_otg->dev, "cannot get vusb regulator\n");
				return;
			}
		}

		if (IS_USB_PHY_SUSPEND_LOCK() && !enable) {
			pr_err("%s phy suspend is locked\n", __func__);
			return;
		}

		if (enable) {
			regulator_enable(t1_otg->vusb);
			t1_otg->reg_on = true;
		} else if (t1_otg->reg_on) {
			regulator_disable(t1_otg->vusb);
			t1_otg->reg_on = false;
		}
	} else
		pr_err("%s error. already set\n", __func__);
}

static void t1_set_vbus_drive(bool enable)
{
#if !defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
	struct t1_otg *t1_otg = &t1_otg_xceiv;
	pr_info("otg t1_set_vbus_drive gpio %d : %s\n",
			otg_en,
			enable ? "ON" : "OFF");

	if (t1_otg->vbus_on != enable) {
		if (t1_otg->pdata->usbhostd_wakeup && !!enable)
			t1_otg->pdata->usbhostd_wakeup();
		if (enable)
			host_notify_set_ovc_en
				(&t1_otg->pdata->ndev, NOTIFY_SET_ON);
		else
			host_notify_set_ovc_en
				(&t1_otg->pdata->ndev, NOTIFY_SET_OFF);
		gpio_set_value(otg_en, !!enable);
		t1_otg->vbus_on = enable;
	} else
		pr_err("%s error. already set\n", __func__);
#endif
}

static void t1_ap_usb_attach(struct t1_otg *t1_otg)
{
	pr_info("[%s]\n", __func__);
	USB_PHY_SUSPEND_LOCK();
	if (!cancel_delayed_work_sync(&t1_otg->usb_disconn_w))
		t1_vusb_enable(t1_otg, true);
	omap4430_phy_init_for_eyediagram(SWCAP_TRIM_OFFSET, 0, 0);
	omap4460_phy_tuning_for_eyediagram(0, 0, 0x1);
	USB_PHY_SUSPEND_UNLOCK();

	t1_mux_usb_to_fsa(true);
	fsa9480_set_switch(fsa9480_usbsw, "AUTO");

	t1_otg->otg.state = OTG_STATE_B_IDLE;
	t1_otg->otg.default_a = false;
	t1_otg->otg.last_event = USB_EVENT_VBUS;

	atomic_notifier_call_chain(&t1_otg->otg.notifier,
				   USB_EVENT_VBUS, t1_otg->otg.gadget);

}

static void t1_ap_usb_detach(struct t1_otg *t1_otg)
{
	pr_info("[%s]\n", __func__);

	t1_otg->otg.state = OTG_STATE_B_IDLE;
	t1_otg->otg.default_a = false;
	t1_otg->otg.last_event = USB_EVENT_NONE;
	omap4430_phy_remove_for_eyediagram();
	atomic_notifier_call_chain(&t1_otg->otg.notifier,
				   USB_EVENT_NONE, t1_otg->otg.gadget);
	/*
	 * Turning off regulator after usb disconnecting housekeeping
	 * function completes so scheduling the delayed work fn
	 * after 1ms
	 *
	 */
	schedule_delayed_work(&t1_otg->usb_disconn_w, usecs_to_jiffies(1000));
}

static void t1_cp_usb_attach(struct t1_otg *t1_otg)
{
	gpio_set_value(uart_sw_gpios[GPIO_CP_USB_ON].gpio, 1);

	t1_mux_usb_to_fsa(true);
	fsa9480_set_switch(fsa9480_usbsw, "AUDIO");
	t1_otg->otg.state = OTG_STATE_B_IDLE;
	t1_otg->otg.default_a = false;
	t1_otg->otg.last_event = USB_EVENT_VBUS;
	atomic_notifier_call_chain(&t1_otg->otg.notifier,
				   USB_EVENT_VBUS, t1_otg->otg.gadget);
}

static void t1_cp_usb_detach(struct t1_otg *t1_otg)
{
	gpio_set_value(uart_sw_gpios[GPIO_CP_USB_ON].gpio, 0);
	t1_otg->otg.state = OTG_STATE_B_IDLE;
	t1_otg->otg.default_a = false;
	t1_otg->otg.last_event = USB_EVENT_NONE;
	atomic_notifier_call_chain(&t1_otg->otg.notifier,
				   USB_EVENT_NONE, t1_otg->otg.gadget);
}

static void t1_usb_host_attach(struct t1_otg *t1_otg)
{
	pr_info("[%s]\n", __func__);
	t1_set_vbus_drive(false);
#ifdef CONFIG_USB_HOST_NOTIFY
	if (t1_otg->pdata && t1_otg->pdata->usbhostd_start) {
		host_notify_set_ovc_en
			(&t1_otg->pdata->ndev, NOTIFY_SET_OFF);
		t1_otg->pdata->ndev.mode = NOTIFY_HOST_MODE;
		t1_otg->pdata->usbhostd_start();
	}
#endif

	USB_PHY_SUSPEND_LOCK();
	t1_vusb_enable(t1_otg, true);
	omap4430_phy_init_for_eyediagram
		(SWCAP_TRIM_OFFSET_HOST, BGTRIM_TRIM_OFFSET_HOST
				, RTERM_RMX_OFFSET_HOST);
	omap4460_phy_tuning_for_eyediagram
		(RTERM_CAL_OFFSET_HOST, SQ_OFF_CODE_DAC3_OFFSET_HOST
				, HS_CODE_SEL_HOST);
	USB_PHY_SUSPEND_UNLOCK();

	t1_mux_usb_to_fsa(true);

	t1_otg->otg.state = OTG_STATE_A_IDLE;
	t1_otg->otg.default_a = true;
	t1_otg->otg.last_event = USB_EVENT_ID;

	atomic_notifier_call_chain(&t1_otg->otg.notifier,
				   USB_EVENT_ID, t1_otg->otg.gadget);
}

static void t1_usb_host_detach(struct t1_otg *t1_otg)
{
	pr_info("[%s]\n", __func__);
#ifdef CONFIG_USB_HOST_NOTIFY
	if (t1_otg->pdata && t1_otg->pdata->usbhostd_stop) {
		t1_otg->pdata->ndev.mode = NOTIFY_NONE_MODE;
		t1_otg->pdata->usbhostd_stop();
	}
#endif

	t1_otg->otg.state = OTG_STATE_B_IDLE;
	t1_otg->otg.default_a = false;
	t1_otg->otg.last_event = USB_EVENT_NONE;

	omap4430_phy_remove_for_eyediagram();

	atomic_notifier_call_chain(&t1_otg->otg.notifier,
				   USB_EVENT_HOST_NONE, t1_otg->otg.gadget);

	/* Make sure the VBUS drive is turned off */
	t1_set_vbus_drive(false);
	t1_vusb_enable(t1_otg, false);
}

static int __t1_usb_host_shutdown_call(struct notifier_block *this,
				       unsigned long code, void *cmd)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;

#if defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
	if (code == SYS_POWER_OFF)
		t1_usb_host_detach(t1_otg);
#else
	if (code == SYS_POWER_OFF && gpio_get_value(otg_en))
		t1_usb_host_detach(t1_otg);
#endif

	return 0;
}

static struct notifier_block t1_usb_host_reboot_notifier = {
	.notifier_call = __t1_usb_host_shutdown_call,
};

static void __init t1_usb_host_reboot_init(void)
{
	int acc_out =
	omap_muxtbl_get_gpio_by_name("OTG_OUT_5V");

	if (unlikely(acc_out != -EINVAL))
		register_reboot_notifier(&t1_usb_host_reboot_notifier);
}

static int t1_usb_host_check_notify_test_mode
				(struct t1_otg *t1_otg)
{
	return (t1_otg->pdata->ndev.mode == NOTIFY_TEST_MODE) ? 1 : 0;
}

static void t1_ap_uart_actions(struct t1_otg *t1_otg)
{
	struct omap_mux_partition *p = omap_mux_get("core");
	u16 mux;
	t1_mux_usb_to_fsa(true);
	gpio_set_value(uart_sw_gpios[GPIO_UART_SEL].gpio, IF_UART_SEL_AP);
	mux = omap_mux_read(p, OMAP4_CTRL_MODULE_PAD_UART4_RX_OFFSET);
	omap_mux_write(p, mux & ~OMAP_MUX_MODE7,
		OMAP4_CTRL_MODULE_PAD_UART4_RX_OFFSET);
}

static void t1_cp_uart_actions(struct t1_otg *t1_otg)
{
	struct omap_mux_partition *p = omap_mux_get("core");
	u16 mux;
	t1_mux_usb_to_fsa(true);
	/* When CP UART mode is selected, AP RXD is set to safe mode.
	 * This is done to prevent any stray signal on AP RXD line.
	 */
	mux = omap_mux_read(p, OMAP4_CTRL_MODULE_PAD_UART4_RX_OFFSET);
	omap_mux_write(p, mux | OMAP_MUX_MODE7,
		OMAP4_CTRL_MODULE_PAD_UART4_RX_OFFSET);
	gpio_set_value(uart_sw_gpios[GPIO_UART_SEL].gpio, IF_UART_SEL_CP);
}

static void t1_otg_mask_vbus_irq(void)
{
	twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
					REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
					REG_INT_MSK_STS_C);
}

static void t1_otg_unmask_vbus_irq(void)
{
	if (!t1_twl_chgctrl_init) {
		int r;

		r = twl_i2c_write_u8(TWL_MODULE_MAIN_CHARGE,
				(u8) ~(TWL_CONTROLLER_RSVD |
					TWL_CONTROLLER_MVBUS_DET),
				TWL_REG_CONTROLLER_INT_MASK);

		if (r)
			pr_err_once("%s:Err writing twlcharge ctrl int mask\n",
					__func__);
		else
			t1_twl_chgctrl_init = true;
	}

	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
					REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
					REG_INT_MSK_STS_C);
}

static bool t1_otg_vbus_present(void)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;

	/* u8 vbus_state;
	 * twl_i2c_read_u8(TWL_MODULE_MAIN_CHARGE, &vbus_state,
	 * TWL_REG_CONTROLLER_STAT1);
	 *
	 *  return !!(vbus_state & TWL_STAT1_VBUS_DET);
	 */

	return !gpio_get_value(t1_otg->gpio_ta_nconnected);
}

static void t1_set_usbsw(void *usbsw)
{
	fsa9480_usbsw = usbsw;
}

/* When charger is connected to desk dock,
 * check for ta_Nconnected to detect.
 */
static void t1_desk_dock_charger_connected(u32 curr_dev)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;
	if (curr_dev == FSA9480_DETECT_AV_365K &&
		!gpio_get_value(t1_otg->gpio_ta_nconnected))
		t1_otg->desk_dock_charger_status =
			T1_CHARGER_CONNECTED_TO_DESKDOCK;
	return;
}

/* When charger is removed from desk dock,
 * check for adc value 0x1a which indicates
 * the presence of desk dock.
 * This is required to know the presence
 * of desk dock when charger is removed.
 */
static void t1_desk_dock_charger_removal(u8 adc_val)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;
	if (adc_val == 0x1a)
		t1_otg->desk_dock_charger_status =
			T1_CHARGER_REMOVED_FROM_DESKDOCK;
	return;
}

static void t1_fsa_usb_detected(int device)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;
	int old_device;

	mutex_lock(&t1_otg->lock);

	old_device = t1_otg->current_device;
	t1_otg->current_device = device;

	pr_debug("detected %x\n", device);
	switch (device) {
	case FSA9480_DETECT_USB:
		if (t1_otg->usb_manual_mode == T1_MANUAL_USB_MODEM)
			t1_cp_usb_attach(t1_otg);
		else
			t1_ap_usb_attach(t1_otg);
		break;
	case FSA9480_DETECT_UART:
		t1_otg->jig_uart_connected = true;
		if ((!gpio_get_value(t1_otg->gpio_ta_nconnected)) &&
				(!t1_usb_host_check_notify_test_mode(t1_otg))) {
			t1_mux_usb_to_fsa(true);
			t1_otg->otg.state = OTG_STATE_B_IDLE;
			t1_otg->otg.default_a = false;
			t1_otg->otg.last_event = USB_EVENT_CHARGER;
			atomic_notifier_call_chain
				(&t1_otg->otg.notifier,
				 USB_EVENT_CHARGER, t1_otg->otg.gadget);
		}
		if (t1_otg->irq_ta_nconnected)
			enable_irq(t1_otg->irq_ta_nconnected);
		break;
	case FSA9480_DETECT_AV_365K_CHARGER:
		t1_otg_set_dock_switch(T1_DOCK_DESK);
		/* intentional fall-through */
	case FSA9480_DETECT_CHARGER:
		t1_mux_usb_to_fsa(true);

		t1_otg->otg.state = OTG_STATE_B_IDLE;
		t1_otg->otg.default_a = false;
		t1_otg->otg.last_event = USB_EVENT_CHARGER;
		atomic_notifier_call_chain(&t1_otg->otg.notifier,
					   USB_EVENT_CHARGER,
					   t1_otg->otg.gadget);
		break;
	case FSA9480_DETECT_USB_HOST:
		t1_usb_host_attach(t1_otg);
		break;
	case FSA9480_DETECT_NONE:
		t1_mux_usb_to_fsa(true);

		switch (old_device) {
		case FSA9480_DETECT_JIG:
			if (t1_otg->car_dock_connected) {
				if (t1_otg->irq_ta_nconnected)
					disable_irq(t1_otg->irq_ta_nconnected);
				fsa9480_set_switch(fsa9480_usbsw, "AUTO");
				t1_otg_set_dock_switch(T1_DOCK_NONE);
				t1_otg->car_dock_connected = false;
			} else if (t1_otg->uart_manual_mode ==
				T1_MANUAL_UART_NONE)
				t1_ap_uart_actions(t1_otg);
#if defined(CONFIG_MACH_SAMSUNG_T1_CHN_CMCC)
			gpio_set_value(uart_sw_gpios[GPIO_AP_CP_INT1].gpio, 1);
#endif
			break;
		case FSA9480_DETECT_AV_365K_CHARGER:
			if (!(t1_otg->desk_dock_charger_status ==
				T1_CHARGER_REMOVED_FROM_DESKDOCK))
				t1_otg_set_dock_switch(T1_DOCK_NONE);
			t1_otg->desk_dock_charger_status =
				T1_DESKDOCK_CHARGER_DEFAULT;
			/* intentional fall-through */
		case FSA9480_DETECT_USB:
			if (t1_otg->usb_manual_mode == T1_MANUAL_USB_MODEM)
				t1_cp_usb_detach(t1_otg);
			else
				t1_ap_usb_detach(t1_otg);
			break;
		case FSA9480_DETECT_USB_HOST:
			t1_usb_host_detach(t1_otg);
			break;
		case FSA9480_DETECT_AV_365K:
			if (!(t1_otg->desk_dock_charger_status ==
				T1_CHARGER_CONNECTED_TO_DESKDOCK))
				t1_otg_set_dock_switch(T1_DOCK_NONE);
			t1_otg->desk_dock_charger_status =
				T1_DESKDOCK_CHARGER_DEFAULT;
			break;
		case FSA9480_DETECT_UART:
			if (t1_otg->jig_uart_connected) {
				if (t1_otg->irq_ta_nconnected)
					disable_irq(t1_otg->irq_ta_nconnected);
				t1_otg->jig_uart_connected = false;
			}
			break;
		case FSA9480_DETECT_CHARGER:
		default:
			t1_ap_usb_detach(t1_otg);
			break;
		};
		break;
	case FSA9480_DETECT_JIG:
		if (!boot_jig && !strncasecmp(sec_androidboot_mode, "jig", 3)) {
			switch (t1_otg->uart_manual_mode) {
			case T1_MANUAL_UART_MODEM:
				t1_cp_uart_actions(t1_otg);
				break;
			case T1_MANUAL_UART_AP:
			default:
				t1_ap_uart_actions(t1_otg);
				break;
			}
		} else {
			fsa9480_set_switch(fsa9480_usbsw, "VAUDIO");
			t1_otg_set_dock_switch(T1_DOCK_CAR);
			if ((!gpio_get_value(t1_otg->gpio_ta_nconnected)) &&
				(!t1_usb_host_check_notify_test_mode(t1_otg))) {
				t1_mux_usb_to_fsa(true);
				t1_otg->otg.state = OTG_STATE_B_IDLE;
				t1_otg->otg.default_a = false;
				t1_otg->otg.last_event = USB_EVENT_CHARGER;
				atomic_notifier_call_chain
					(&t1_otg->otg.notifier,
					USB_EVENT_CHARGER, t1_otg->otg.gadget);
			}
			t1_otg->car_dock_connected = true;
			if (t1_otg->irq_ta_nconnected)
				enable_irq(t1_otg->irq_ta_nconnected);
		}
#if defined(CONFIG_MACH_SAMSUNG_T1_CHN_CMCC)
		gpio_set_value(uart_sw_gpios[GPIO_AP_CP_INT1].gpio, 1);
#endif
		boot_jig = true;
		break;
	case FSA9480_DETECT_AV_365K:
		t1_otg_set_dock_switch(T1_DOCK_DESK);
		break;
	}

	mutex_unlock(&t1_otg->lock);
}

static struct fsa9480_detect_set fsa_detect_sets[] = {
	{
		.prio		= T1_OTG_ID_FSA9480_PRIO,
		.mask		= FSA9480_DETECT_ALL,
	},
	{
		.prio		= T1_OTG_ID_FSA9480_LAST_PRIO,
		.mask		= 0,
		.fallback	= true,
	},
};

static struct fsa9480_platform_data t1_fsa9480_pdata = {
	.detect_time	= 500,
	.detect_sets	= fsa_detect_sets,
	.num_sets	= ARRAY_SIZE(fsa_detect_sets),

	.enable		= t1_mux_usb_to_fsa,
	.detected	= t1_fsa_usb_detected,
	.external_vbus_irq      = TWL6030_VBUS_IRQ,
	.external_vbus_flags    = TWL6030_VBUS_FLAGS,
	.mask_vbus_irq          = t1_otg_mask_vbus_irq,
	.unmask_vbus_irq        = t1_otg_unmask_vbus_irq,
	.vbus_present           = t1_otg_vbus_present,
	.set_usbsw              = t1_set_usbsw,
	.desk_dock_charger_connected = t1_desk_dock_charger_connected,
	.desk_dock_charger_removal = t1_desk_dock_charger_removal,
};

static struct i2c_board_info __initdata t1_connector_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("fsa9480", 0x4A >> 1),
		.platform_data = &t1_fsa9480_pdata,
	},
};

static int t1_otg_set_host(struct otg_transceiver *otg, struct usb_bus *host)
{
	otg->host = host;
	if (!host)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static int t1_otg_set_peripheral(struct otg_transceiver *otg,
				 struct usb_gadget *gadget)
{
	otg->gadget = gadget;
	if (!gadget)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static void usb_disconnect_work_fn(struct work_struct *w)
{
	struct t1_otg *t1_otg = container_of(w, struct t1_otg,
					     usb_disconn_w);
	t1_vusb_enable(t1_otg, false);
}

static void t1_otg_work(struct work_struct *data)
{
	struct t1_otg *t1_otg = container_of(data, struct t1_otg,
					     set_vbus_work);

	mutex_lock(&t1_otg->lock);

	/* Only allow VBUS drive when in host mode. */
	if (t1_otg->current_device != FSA9480_DETECT_USB_HOST) {
		mutex_unlock(&t1_otg->lock);
		return;
	}

	t1_set_vbus_drive(t1_otg->need_vbus_drive);

	mutex_unlock(&t1_otg->lock);
}

static int t1_otg_set_vbus(struct otg_transceiver *otg, bool enabled)
{
	struct t1_otg *t1_otg = container_of(otg, struct t1_otg, otg);
	dev_info(otg->dev, "vbus %s\n", enabled ? "on" : "off");

	t1_otg->need_vbus_drive = enabled;
	schedule_work(&t1_otg->set_vbus_work);

	return 0;
}

static int t1_otg_phy_init(struct otg_transceiver *otg)
{
	if (otg->last_event == USB_EVENT_ID)
		omap4430_phy_power(otg->dev, 1, 1);
	else
		omap4430_phy_power(otg->dev, 0, 1);
	return 0;
}

static void t1_otg_phy_shutdown(struct otg_transceiver *otg)
{
	omap4430_phy_power(otg->dev, 0, 0);
}

static int t1_otg_set_suspend(struct otg_transceiver *otg, int suspend)
{
	int ret = 0;
	dev_info(otg->dev, "%s = %d\n", __func__, suspend);

	if (IS_USB_PHY_SUSPEND_LOCK() && suspend)
		dev_info(otg->dev, "phy suspend is locked\n");
	else
		ret = omap4430_phy_suspend(otg->dev, suspend);

	return ret;
}

static int t1_otg_is_active(struct otg_transceiver *otg)
{
	return omap4430_phy_is_active(otg->dev);
}

static int t1_otg_vbus_reset(struct otg_transceiver *otg)
{
	struct t1_otg *t1_otg =
	    container_of(otg, struct t1_otg, otg);
#ifdef CONFIG_USB_HOST_NOTIFY
	host_notify_set_ovc_en(&t1_otg->pdata->ndev, NOTIFY_SET_OFF);
	t1_otg->pdata->ndev.booster = NOTIFY_POWER_OFF;
#endif
	t1_otg_set_vbus(otg, 0);
#ifdef CONFIG_USB_HOST_NOTIFY
	if (t1_otg->pdata->ndev.mode == NOTIFY_HOST_MODE)
#endif
		t1_otg_set_vbus(otg, 1);
#ifdef CONFIG_USB_HOST_NOTIFY
	host_notify_set_ovc_en(&t1_otg->pdata->ndev, NOTIFY_SET_ON);
#endif
	return 0;
}

static ssize_t t1_otg_usb_sel_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct t1_otg *t1_otg = dev_get_drvdata(dev);
	const char *mode;
	/* Data router needs string
	 * "PDA" and "MODEM". Phoneutil is
	 * not calling this function.
	 */
	switch (t1_otg->usb_manual_mode) {
	case T1_MANUAL_USB_AP:
		mode = "PDA";
		break;
	case T1_MANUAL_USB_MODEM:
		mode = "MODEM";
		break;
	default:
		mode = "NONE";
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t t1_otg_usb_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct t1_otg *t1_otg = dev_get_drvdata(dev);
	int old_mode;

	mutex_lock(&t1_otg->lock);

	old_mode = t1_otg->usb_manual_mode;
	/* For AP USB, check for strings "1" and "PDA"
	 * For CP USB, check for strings "0" and "MODEM"
	 * This is required because Phoneutil uses
	 * "0" and "1". But Data router uses "PDA"
	 * and "MODEM".
	 */
	if (!strncasecmp(buf, "1", 1) || !strncasecmp(buf, "PDA", 3)) {
		t1_otg->usb_manual_mode = T1_MANUAL_USB_AP;

		/* If we are transitioning from CP USB to AP USB then notify the
		 * USB stack that is now attached.
		 */
		if (t1_otg->current_device == FSA9480_DETECT_USB &&
		    old_mode == T1_MANUAL_USB_MODEM) {
			t1_cp_usb_detach(t1_otg);
			t1_ap_usb_attach(t1_otg);
		}
	} else if (!strncasecmp(buf, "0", 1) || !strncasecmp(buf, "MODEM", 5)) {
		t1_otg->usb_manual_mode = T1_MANUAL_USB_MODEM;

		/* If we are transitioning from AP USB to CP USB then notify the
		 * USB stack that is has been detached.
		 */
		if (t1_otg->current_device == FSA9480_DETECT_USB &&
		    (old_mode == T1_MANUAL_USB_AP ||
		     old_mode == T1_MANUAL_USB_NONE)) {
			t1_ap_usb_detach(t1_otg);
			t1_cp_usb_attach(t1_otg);
		}
	} else if (!strncasecmp(buf, "NONE", 5)) {
		t1_otg->usb_manual_mode = T1_MANUAL_USB_NONE;

		/* If we are transitioning from CP USB to AP USB then notify the
		 * USB stack that it is now attached.
		 */
		if (t1_otg->current_device == FSA9480_DETECT_USB &&
		    old_mode == T1_MANUAL_USB_MODEM) {
			t1_cp_usb_detach(t1_otg);
			t1_ap_usb_attach(t1_otg);
		}
	}

	mutex_unlock(&t1_otg->lock);

	return size;
}

static ssize_t t1_usb_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct t1_otg *t1_otg = dev_get_drvdata(dev);
	const char *mode;

	if (t1_otg->current_device == FSA9480_DETECT_USB)
		mode = "USB_STATE_CONFIGURED";
	else
	mode = "USB_STATE_NOT_CONFIGURED";

	return sprintf(buf, "%s\n", mode);
}

static ssize_t t1_otg_uart_switch_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct t1_otg *t1_otg = dev_get_drvdata(dev);
	const char *mode;
	switch (t1_otg->uart_manual_mode) {
	case T1_MANUAL_UART_AP:
		mode = "1";
		break;
	case T1_MANUAL_UART_MODEM:
		mode = "0";
		break;
	default:
		mode = "NONE";
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t t1_otg_uart_switch_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct t1_otg *t1_otg = dev_get_drvdata(dev);
	mutex_lock(&t1_otg->lock);

	if (!strncasecmp(buf, "1", 1) || !strncasecmp(buf, "PDA", 3)) {
		t1_otg->uart_manual_mode = T1_MANUAL_UART_AP;

		if (t1_otg->current_device == FSA9480_DETECT_JIG)
			t1_ap_uart_actions(t1_otg);
	} else if (!strncasecmp(buf, "0", 1) || !strncasecmp(buf, "MODEM", 5)) {
		t1_otg->uart_manual_mode = T1_MANUAL_UART_MODEM;

		if (t1_otg->current_device == FSA9480_DETECT_JIG)
			t1_cp_uart_actions(t1_otg);
	} else if (!strncasecmp(buf, "NONE", 5)) {
		t1_otg->uart_manual_mode = T1_MANUAL_UART_NONE;

		if (t1_otg->current_device == FSA9480_DETECT_JIG)
			t1_ap_uart_actions(t1_otg);
	}

	mutex_unlock(&t1_otg->lock);

	return size;
}
#ifdef CONFIG_OMAP_PM
#include <plat/omap-pm.h>
static struct pm_qos_request_list pm_qos_dpll_handle;
#endif

static void sii9234_power(int on)
{
	struct omap_mux_partition *p = omap_mux_get("core");
	u16 mux;
#ifdef CONFIG_OMAP_PM
	static bool pm_qos_request_added;
#endif

	mux = omap_mux_read(p, OMAP4_CTRL_MODULE_PAD_HDMI_HPD_OFFSET);

	if (on) {
#ifdef CONFIG_OMAP_PM
		if (!pm_qos_request_added) {
			pm_qos_request_added = true;
			pm_qos_add_request(&pm_qos_dpll_handle,
				PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		}
		pm_qos_update_request(&pm_qos_dpll_handle, 7);
#endif

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

#ifdef CONFIG_OMAP_PM
		pm_qos_update_request(&pm_qos_dpll_handle, -1);
#endif
	}
}

static void sii9234_enable_vbus(bool enable)
{
}

static void sii9234_connect(bool on, u8 *devcap)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;
	unsigned long val;
	int dock = 0;

	if (on) {
		val = USB_EVENT_VBUS;
		if (devcap) {
			u16 adopter_id =
				(devcap[MHL_DEVCAP_ADOPTER_ID_H] << 8) |
				devcap[MHL_DEVCAP_ADOPTER_ID_L];
			u16 device_id =
				(devcap[MHL_DEVCAP_DEVICE_ID_H] << 8) |
				devcap[MHL_DEVCAP_DEVICE_ID_L];

			if (adopter_id == 0x3333 || adopter_id == 321) {
				if (devcap[MHL_DEVCAP_RESERVED] == 2)
					val = USB_EVENT_CHARGER;

				if (device_id == 0x1234)
					dock = 1;
			}
		}
	} else {
		val = USB_EVENT_NONE;
	}

	t1_otg->otg.state = OTG_STATE_B_IDLE;
	t1_otg->otg.default_a = false;
	t1_otg->otg.last_event = val;

	atomic_notifier_call_chain(&t1_otg->otg.notifier,
				   val, t1_otg->otg.gadget);
	/* we don't use mhl dock for T1
	 * t1_otg_set_dock_switch(dock);
	 */
}

void fsa_enable_adc_change(void)
{
	fsa9480_set_raw_data_bit(fsa9480_usbsw);
}

void t1_otg_pogo_charger(bool on)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;

	t1_otg->otg.state = OTG_STATE_B_IDLE;
	t1_otg->otg.default_a = false;
	t1_otg->otg.last_event = on ? USB_EVENT_CHARGER : USB_EVENT_NONE;
	atomic_notifier_call_chain(&t1_otg->otg.notifier,
				   on ? USB_EVENT_CHARGER : USB_EVENT_NONE,
				   t1_otg->otg.gadget);
}

/* needed devcap ids to check charger */
static bool early_read_devcap[DEVCAP_COUNT_MAX] = {
				[MHL_DEVCAP_ADOPTER_ID_H] = true,
				[MHL_DEVCAP_ADOPTER_ID_L] = true,
				[MHL_DEVCAP_DEVICE_ID_H] = true,
				[MHL_DEVCAP_DEVICE_ID_L] = true,
				[MHL_DEVCAP_RESERVED] = true,
};

static struct sii9234_platform_data sii9234_pdata = {
	.prio		= T1_OTG_ID_SII9234_PRIO,
	.enable		= t1_mux_usb_to_mhl,
	.power		= sii9234_power,
	.enable_vbus	= sii9234_enable_vbus,
	.connect	= sii9234_connect,
	.swing_level	= T1_MHL_SWING_LEVEL,
	.early_read_devcap = early_read_devcap,
	.enable_adc_change = fsa_enable_adc_change,
};

static struct i2c_board_info __initdata t1_connector_i2c5_boardinfo[] = {
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

static void t1_uart_sw_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(uart_sw_gpios); i++)
		uart_sw_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(uart_sw_gpios[i].label);
	gpio_request_array(uart_sw_gpios, ARRAY_SIZE(uart_sw_gpios));
}

static void t1_mhl_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mhl_gpios); i++)
		mhl_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(mhl_gpios[i].label);
	gpio_request_array(mhl_gpios, ARRAY_SIZE(mhl_gpios));

	t1_connector_i2c5_boardinfo[0].irq =
	    gpio_to_irq(mhl_gpios[GPIO_MHL_INT].gpio);
}

#ifdef CONFIG_USB_HOST_NOTIFY
static void t1_booster(int enable)
{
	t1_set_vbus_drive(!!enable);
}

struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name     = "usb_otg",
	.booster       = t1_booster,
	.thread_enable = 1,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};

static void t1_host_notifier_init(struct t1_otg *t1_otg)
{
	int acc_out =
	omap_muxtbl_get_gpio_by_name("OTG_OUT_5V");
	if (acc_out < 0) {
		dev_err(&t1_otg->dev, "OTG_OUT_5V is invalid.\n");
		return;
	}
	host_notifier_pdata.gpio = acc_out;
	t1_otg->pdata = &host_notifier_pdata;
	platform_device_register(&host_notifier_device);
}
#endif

static void t1_fsa9480_gpio_init(void)
{
#if !defined(CONFIG_USB_SWITCH_FSA9480_DISABLE_OTG)
	t1_fsa9480_pdata.external_id =
	    omap_muxtbl_get_gpio_by_name("USB_OTG_ID");
#endif

	t1_connector_i2c4_boardinfo[0].irq =
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

/* ISR to determine charger connected
   to car dock. This is required because charger
   can be connected to car dock anytime and not
   necessary when target is connected to car dock.
   Enable the IRQ only during car dock case.
 */
static irqreturn_t charge_detection_irq(int irq, void *_data)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;
	if (!t1_otg->car_dock_connected && !t1_otg->jig_uart_connected)
		return IRQ_HANDLED;
	t1_mux_usb_to_fsa(true);
	if ((!gpio_get_value(t1_otg->gpio_ta_nconnected)) &&
		(!t1_usb_host_check_notify_test_mode(t1_otg))) {
		t1_otg->otg.state = OTG_STATE_B_IDLE;
		t1_otg->otg.default_a = false;
		t1_otg->otg.last_event = USB_EVENT_CHARGER;
		atomic_notifier_call_chain(&t1_otg->otg.notifier,
			USB_EVENT_CHARGER, t1_otg->otg.gadget);
	} else if (t1_otg->otg.last_event != USB_EVENT_NONE) {
		t1_otg->otg.state = OTG_STATE_B_IDLE;
		t1_otg->otg.default_a = false;
		t1_otg->otg.last_event = USB_EVENT_NONE;
		atomic_notifier_call_chain(&t1_otg->otg.notifier,
			USB_EVENT_NONE, t1_otg->otg.gadget);
	}
	return IRQ_HANDLED;
}

void t1_init_ta_nconnected(int ta_nconnected)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;
	int ret;
	t1_otg->gpio_ta_nconnected = ta_nconnected;
	t1_otg->irq_ta_nconnected = gpio_to_irq(ta_nconnected);
	ret = request_threaded_irq(t1_otg->irq_ta_nconnected,
			NULL, charge_detection_irq,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
			"Charge_det", NULL);
	if (ret) {
		dev_err(&t1_otg->dev, "ta_nconnected request threaded irq failed\n");
		return;
	}
	disable_irq(t1_otg->irq_ta_nconnected);
}
EXPORT_SYMBOL_GPL(t1_init_ta_nconnected);

static void __init t1_switch_initial_setup(void)
{

	struct t1_otg *t1_otg = &t1_otg_xceiv;

	if (init_switch_sel & MASK_SWITCH_UART_AP) {
		t1_ap_uart_actions(t1_otg);
		t1_otg->uart_manual_mode = T1_MANUAL_UART_AP;
	} else {
		t1_cp_uart_actions(t1_otg);
		t1_otg->uart_manual_mode = T1_MANUAL_UART_MODEM;
	}

	if (init_switch_sel & MASK_SWITCH_USB_AP) {
		gpio_set_value(uart_sw_gpios[GPIO_CP_USB_ON].gpio, 0);
		t1_otg->usb_manual_mode = T1_MANUAL_USB_AP;
	} else {
		gpio_set_value(uart_sw_gpios[GPIO_CP_USB_ON].gpio, 1);
		t1_otg->usb_manual_mode = T1_MANUAL_USB_MODEM;
	}

}

static int __init t1_save_init_switch_param(char *str)
{

	int ret;

	ret = kstrtoint(str, 10, &init_switch_sel);

	return ret;
}
__setup("switch_sel=", t1_save_init_switch_param);


void __init omap4_t1_connector_init(void)
{
	struct t1_otg *t1_otg = &t1_otg_xceiv;
	int ret;

	t1_uart_sw_gpio_init();
	t1_mhl_gpio_init();
	t1_fsa9480_gpio_init();

	t1_fsa3200_mux(T1_USB_MUX_DEFAULT);

	mutex_init(&t1_otg->lock);
	INIT_WORK(&t1_otg->set_vbus_work, t1_otg_work);
	INIT_DELAYED_WORK(&t1_otg->usb_disconn_w, usb_disconnect_work_fn);
	device_initialize(&t1_otg->dev);
	dev_set_name(&t1_otg->dev, "%s", "t1_otg");
	ret = device_add(&t1_otg->dev);
	if (ret) {
		pr_err("%s: cannot reg device '%s' (%d)\n", __func__,
		       dev_name(&t1_otg->dev), ret);
		return;
	}

	dev_set_drvdata(&t1_otg->dev, t1_otg);

	t1_otg->otg.dev			= &t1_otg->dev;
	t1_otg->otg.label		= "t1_otg_xceiv";
	t1_otg->otg.set_host		= t1_otg_set_host;
	t1_otg->otg.set_peripheral	= t1_otg_set_peripheral;
	t1_otg->otg.set_suspend		= t1_otg_set_suspend;
	t1_otg->otg.set_vbus		= t1_otg_set_vbus;
	t1_otg->otg.init		= t1_otg_phy_init;
	t1_otg->otg.shutdown		= t1_otg_phy_shutdown;
	t1_otg->otg.is_active		= t1_otg_is_active;
/* start_hnp function is used for vbus reset. */
	t1_otg->otg.start_hnp		= t1_otg_vbus_reset;

	ATOMIC_INIT_NOTIFIER_HEAD(&t1_otg->otg.notifier);

	ret = otg_set_transceiver(&t1_otg->otg);
	if (ret)
		pr_err("t1_otg: cannot set transceiver (%d)\n", ret);

	omap4430_phy_init(&t1_otg->dev);
	t1_otg_set_suspend(&t1_otg->otg, 0);
#ifdef CONFIG_USB_HOST_NOTIFY
	t1_host_notifier_init(t1_otg);
#endif
	ret = sysfs_create_group(&t1_otg->dev.kobj, &manual_switch_sel_group);

	if (ret)
		pr_err("t1_otg: Unable to create manual mode sysfs group"
		       "(%d)\n", ret);

	t1_switch_initial_setup();

	i2c_register_board_info(4, t1_connector_i2c4_boardinfo,
				ARRAY_SIZE(t1_connector_i2c4_boardinfo));

	i2c_register_board_info(5, t1_connector_i2c5_boardinfo,
				ARRAY_SIZE(t1_connector_i2c5_boardinfo));

	t1_otg->dock_switch.name = "dock";
	switch_dev_register(&t1_otg->dock_switch);

	t1_usb_host_reboot_init();
}

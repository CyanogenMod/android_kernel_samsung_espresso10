/* arch/arm/mach-omap2/board-kona-connector.c
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
#include <linux/regulator/consumer.h>
#include <linux/usb/otg.h>
#include <linux/delay.h>
#include <linux/sii9234.h>
#include <linux/i2c/twl.h>
#include <linux/mutex.h>
#include <linux/switch.h>
#include <linux/30pin_con.h>
#include <linux/mfd/stmpe811.h>
#include <linux/irq.h>
#include <linux/sec_dock_keyboard.h>

#include <plat/usb.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif

#include "board-kona.h"
#include "mux.h"
#include "omap_muxtbl.h"

#define KONA_MANUAL_USB_NONE	0
#define KONA_MANUAL_USB_MODEM	1
#define KONA_MANUAL_USB_AP		2

#define KONA_MANUAL_UART_NONE	0
#define KONA_MANUAL_UART_MODEM	1
#define KONA_MANUAL_UART_AP		2

#define NAME_USB_PATH_AP        "PDA"
#define NAME_USB_PATH_CP        "MODEM"
#define NAME_USB_PATH_NONE      "NONE"
#define NAME_UART_PATH_AP       "AP"
#define NAME_UART_PATH_CP       "CP"
#define NAME_UART_PATH_NONE     "NONE"

#define IF_UART_SEL_CP		0
#define IF_UART_SEL_AP		1

#define AP_UART_SEL_DOCK	0
#define AP_UART_SEL_FACTORY	1

#define ADC_CHANNEL_IN0		4
#define ADC_CHANNEL_IN1		5
#define ADC_CHANNEL_IN2		6
#define ADC_CHANNEL_IN3		7

#define MAX_ADC_VAL	4096
#define MIN_ADC_VAL	0

#define MASK_SWITCH_USB_AP	0x01
#define MASK_SWITCH_UART_AP	0x02

#define ADC_DETECT_TA	4000

#define USB_INDEX	1
#define CHARGER_INDEX	2

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
	struct mutex vusb_lock;
	struct mutex vbus_drive_lock;

	struct device *switch_dev;

	bool reg_on;
	u32 usb_ldo_inst;
	bool vbus_on;
	bool need_vbus_drive;
	int usb_manual_mode;
	int uart_manual_mode;
	int current_device;

	struct switch_dev dock_switch;
	struct switch_dev audio_switch;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct host_notifier_platform_data *pdata;
#endif
};

static struct omap4_otg kona_otg_xceiv;

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

	GPIO_USB_SEL0,
	GPIO_USB_SEL1,
	GPIO_UART_SEL,
	GPIO_UART_SEL_FAC,	/* system_rev >= 3 */
};

enum {
	GPIO_USB_PWR_EN = 0,
};

static struct gpio connector_gpios[] = {
	[GPIO_ACCESSORY_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label = "ACCESSORY_EN",
	},
	[GPIO_ACCESSORY_INT] = {
		.flags = GPIOF_IN,
		.label = "ACCESSORY_INT",
	},
	[GPIO_DOCK_INT] = {
		.flags = GPIOF_IN,
		.label = "DOCK_INT",
	},
	[GPIO_JIG_ON] = {
		.flags = GPIOF_IN,
		.label = "JIG_ON_18",
	},

	[GPIO_USB_SEL0] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "USB_SEL0",
	},
	[GPIO_USB_SEL1] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "USB_SEL1",
	},
	[GPIO_UART_SEL] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "UART_SEL",
	},
	[GPIO_UART_SEL_FAC] = {
		.flags	= GPIOF_OUT_INIT_HIGH,
		.label	= "UART_SEL_FAC",
	},
};

static struct gpio usb_gpios[] = {
	[GPIO_USB_PWR_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label = "USB_PWR_EN",
	},
};

/* STMPE811 */
static struct i2c_board_info __initdata kona_i2c8_boardinfo[] = {
	{
		I2C_BOARD_INFO("stmpe811", 0x82>>1),
	},
};

static void kona_set_dock_switch(int state)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;

	switch_set_state(&kona_otg->dock_switch, state);
}

static void kona_set_audio_switch(int state)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;

	switch_set_state(&kona_otg->audio_switch, state);
}

static void kona_cp_usb_attach(void)
{
	gpio_set_value(connector_gpios[GPIO_USB_SEL0].gpio, 0);
	gpio_set_value(connector_gpios[GPIO_USB_SEL1].gpio, 1);
}

static void kona_cp_usb_detach(void)
{
	gpio_set_value(connector_gpios[GPIO_USB_SEL0].gpio, 1);
	gpio_set_value(connector_gpios[GPIO_USB_SEL1].gpio, 1);
}

static void kona_ap_uart_actions(void)
{
	gpio_set_value(connector_gpios[GPIO_UART_SEL].gpio, IF_UART_SEL_AP);
}

static void kona_cp_uart_actions(void)
{
	gpio_set_value(connector_gpios[GPIO_UART_SEL].gpio, IF_UART_SEL_CP);
}

static void kona_dock_uart_actions(void)
{
	WARN_ON(system_rev < 3);
	gpio_set_value(connector_gpios[GPIO_UART_SEL_FAC].gpio,
			AP_UART_SEL_DOCK);
}

static void kona_factory_uart_actions(void)
{
	WARN_ON(system_rev < 3);
	gpio_set_value(connector_gpios[GPIO_UART_SEL_FAC].gpio,
			AP_UART_SEL_FACTORY);
}

static void kona_gpio_set_for_adc_check_1(void)
{
	gpio_set_value(connector_gpios[GPIO_USB_SEL0].gpio, 1);
	gpio_set_value(connector_gpios[GPIO_USB_SEL1].gpio, 0);
}

static void kona_gpio_rel_for_adc_check_1(void)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;

	if (kona_otg->usb_manual_mode == KONA_MANUAL_USB_MODEM)
		kona_cp_usb_attach();
	else
		kona_cp_usb_detach();
}

static void kona_accessory_power(u32 device, bool enable)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;
	int acc_en_gpio = 0;
	static u32 acc_device;

	/*
		token info
		0 : power off,
		1 : Keyboard dock
		2 : USB
	*/

	pr_info("accessory_power: acc_device 0x%x, new %d : %s\n",
			acc_device, device, enable ? "ON" : "OFF");

	mutex_lock(&kona_otg->vbus_drive_lock);
	acc_en_gpio = connector_gpios[GPIO_ACCESSORY_EN].gpio;

	if (unlikely(acc_en_gpio == -EINVAL))
		pr_err("%s gpio error. value is %d\n",
			__func__, acc_en_gpio);
	else {
		if (enable) {
			acc_device |= (1 << device);
			gpio_set_value(acc_en_gpio, 1);

		} else {
			if (device == 0) {
				pr_info("accessory_power: force turn off\n");
				gpio_set_value(acc_en_gpio, 0);

			} else {
				acc_device &= ~(1 << device);
				if (acc_device == 0) {
					pr_info("accessory_power: turn off\n");
					gpio_set_value(acc_en_gpio, 0);
				} else
					pr_info("accessory_power: skip\n");
			}
		}
	}

	mutex_unlock(&kona_otg->vbus_drive_lock);
}

static int kona_otg_set_host(struct otg_transceiver *otg,
				 struct usb_bus *host)
{
	otg->host = host;
	if (!host)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static int kona_otg_set_peripheral(struct otg_transceiver *otg,
				       struct usb_gadget *gadget)
{
	otg->gadget = gadget;
	if (!gadget)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

/*
kona_vusb_enable
u32 device index : assigned index
	1: usb
	2: charger
	...: reserved
*/
int kona_vusb_enable(u32 device_index, bool enable)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;
	int ret = 0;
	int skip = 0;
	int pwr_en_gpio = 0;
	u32 index = 0;
	pr_info("[%s] index=%d enable=%d\n", __func__, device_index, enable);

	mutex_lock(&kona_otg->vusb_lock);

	index = (1 << device_index);
	if (enable) {
		if (kona_otg->usb_ldo_inst & index) {
			pr_err("%s error. already set index=%x\n",
				__func__, index);
			skip = 1;
		} else if (kona_otg->reg_on) {
			pr_info("%s other index already enabled\n", __func__);
			skip = 1;
		}
		kona_otg->usb_ldo_inst |= index;
	} else {
		if (!(kona_otg->usb_ldo_inst & index)) {
			pr_err("%s error. already clear index=%x\n"
				, __func__, index);
			skip = 1;
		} else if (!(kona_otg->reg_on)) {
			pr_info("%s other index already disabled\n", __func__);
			skip = 1;
		}
		kona_otg->usb_ldo_inst &= ~(index);
		if (kona_otg->usb_ldo_inst) {
			pr_info("%s ldo instance remain %x\n",
				__func__, kona_otg->usb_ldo_inst);
			skip = 1;
		}
	}

	if (!skip) {
		pwr_en_gpio = usb_gpios[GPIO_USB_PWR_EN].gpio;

		if (unlikely(pwr_en_gpio == -EINVAL))
			pr_err("%s gpio error. value is %d\n",
				__func__, pwr_en_gpio);
		else {
			gpio_set_value(pwr_en_gpio, (int)!!enable);
			kona_otg->reg_on = enable;
			pr_info("%s execute enable=%d\n", __func__, enable);
		}
	}

	mutex_unlock(&kona_otg->vusb_lock);

	return ret;
}

static void kona_set_vbus_drive(bool enable)
{
	kona_accessory_power(2, enable);
	return;
}

static void kona_otg_work(struct work_struct *data)
{
	struct omap4_otg *kona_otg =
		container_of(data, struct omap4_otg, set_vbus_work);

	kona_set_vbus_drive(kona_otg->need_vbus_drive);
}

static int kona_otg_set_vbus(struct otg_transceiver *otg, bool enabled)
{
	struct omap4_otg *kona_otg =
	    container_of(otg, struct omap4_otg, otg);
	dev_info(otg->dev, "%s %s\n", __func__, enabled ? "on" : "off");

	kona_otg->need_vbus_drive = enabled;
	schedule_work(&kona_otg->set_vbus_work);

	return 0;
}

static int kona_otg_phy_init(struct otg_transceiver *otg)
{
	dev_info(otg->dev, "%s last_event=%d\n", __func__, otg->last_event);
	if (otg->last_event == USB_EVENT_ID)
		omap4430_phy_power(otg->dev, 1, 1);
	else
		omap4430_phy_power(otg->dev, 0, 1);
	return 0;
}

static void kona_otg_phy_shutdown(struct otg_transceiver *otg)
{
	dev_info(otg->dev, "%s\n", __func__);

	omap4430_phy_power(otg->dev, 0, 0);
	kona_vusb_enable(USB_INDEX, false);
}

static int kona_otg_set_suspend(struct otg_transceiver *otg, int suspend)
{
	dev_info(otg->dev, "%s = %d\n", __func__, suspend);
	return omap4430_phy_suspend(otg->dev, suspend);
}

static int kona_otg_is_active(struct otg_transceiver *otg)
{
	return omap4430_phy_is_active(otg->dev);
}

static int kona_otg_vbus_reset(struct otg_transceiver *otg)
{
	struct omap4_otg *kona_otg =
	    container_of(otg, struct omap4_otg, otg);
#ifdef CONFIG_USB_HOST_NOTIFY
	host_notify_set_ovc_en(&kona_otg->pdata->ndev, NOTIFY_SET_OFF);
	kona_otg->pdata->ndev.booster = NOTIFY_POWER_OFF;
#endif
	kona_otg_set_vbus(otg, 0);
#ifdef CONFIG_USB_HOST_NOTIFY
	if (kona_otg->pdata->ndev.mode == NOTIFY_HOST_MODE)
#endif
		kona_otg_set_vbus(otg, 1);
#ifdef CONFIG_USB_HOST_NOTIFY
	host_notify_set_ovc_en(&kona_otg->pdata->ndev, NOTIFY_SET_ON);
#endif
	return 0;
}

static void kona_ap_usb_attach(struct omap4_otg *otg)
{
	pr_debug("[%s]", __func__);

	kona_vusb_enable(USB_INDEX, true);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_VBUS;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_VBUS,
			otg->otg.gadget);
}

static void kona_ap_usb_detach(struct omap4_otg *otg)
{
	pr_debug("[%s]", __func__);

	otg->otg.default_a = false;
	otg->otg.state = OTG_STATE_B_IDLE;
	otg->otg.last_event = USB_EVENT_NONE;

	atomic_notifier_call_chain(&otg->otg.notifier,
			USB_EVENT_NONE,
			otg->otg.gadget);
}

static void kona_usb_host_attach(struct omap4_otg *otg)
{
	pr_info("[%s]\n", __func__);

	kona_accessory_power(0, 0);

#ifdef CONFIG_USB_HOST_NOTIFY
	if (otg->pdata && otg->pdata->usbhostd_start) {
		otg->pdata->ndev.mode = NOTIFY_HOST_MODE;
		otg->pdata->usbhostd_start();
	}
#endif

	kona_vusb_enable(USB_INDEX, true);

	otg->otg.state = OTG_STATE_A_IDLE;
	otg->otg.default_a = true;
	otg->otg.last_event = USB_EVENT_ID;

	atomic_notifier_call_chain(&otg->otg.notifier,
				   USB_EVENT_ID, otg->otg.gadget);
	return;
}

static void kona_usb_host_detach(struct omap4_otg *otg)
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

int omap4_kona_get_adc(enum kona_adc_ch ch)
{
	int adc;
	u8 stmpe811_ch;
	int i;
	int adc_tmp;
	int adc_min = MAX_ADC_VAL;
	int adc_max = MIN_ADC_VAL;
	int adc_sum = 0;

	if (ch == REMOTE_SENSE)
		stmpe811_ch = ADC_CHANNEL_IN1;
	else if (ch == ADC_CHECK_1)
		stmpe811_ch = ADC_CHANNEL_IN2;
	else if (ch == ACCESSORY_ID)
		stmpe811_ch = ADC_CHANNEL_IN3;
	else if (ch == EAR_ADC_35)
		stmpe811_ch = ADC_CHANNEL_IN0;
	else {
		pr_err("%s: invalid channel input: %d\n", __func__, ch);
		return -EINVAL;
	}

	if (ch == ADC_CHECK_1) {
		/* HQRL Standard defines that time margin from Vbus5V detection
		 * to ADC_CHECK_1 voltage up should be more than 400ms.
		 */
		msleep(400);	/* delay for unstable cable connection */

		kona_gpio_set_for_adc_check_1();

		msleep(150);	/* delay for slow increase of line voltage */

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
		kona_gpio_rel_for_adc_check_1();
		adc = (adc_sum - adc_max - adc_min) / 3;
	} else
		adc = stmpe811_adc_get_value(stmpe811_ch);

	pr_info("%s: adc=%d\n", __func__, adc);

	return adc;
}

#ifdef CONFIG_SII9234
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

static void kona_deskdock_attached(void)
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
		kona_set_dock_switch(UEVENT_DOCK_DESK);
	}
}

static void kona_deskdock_detached(void)
{
	int ret = 0;

	ret = acc_notify(DONGLE_DETACHED);
	if (ret <= 0)
		kona_set_dock_switch(UEVENT_DOCK_NONE);
}

static struct i2c_board_info __initdata kona_i2c9_boardinfo[] = {
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
#else
static void kona_deskdock_attached(void)
{
	kona_set_dock_switch(UEVENT_DOCK_DESK);
}

static void kona_deskdock_detached(void)
{
	kona_set_dock_switch(UEVENT_DOCK_NONE);
}
#endif

void kona_30pin_detected(int device, bool connected)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;

	if (connected)
		kona_otg->current_device |= BIT(device);
	else
		kona_otg->current_device &= ~(BIT(device));

	pr_info("cable detect:%s %s, current device = 0x%04x\n",
			device_names[device], (connected) ? "attach" : "detach",
			kona_otg->current_device);

	switch (device) {
	case P30_OTG:
		if (connected)
			kona_usb_host_attach(kona_otg);
		else
			kona_usb_host_detach(kona_otg);
		break;
	case P30_KEYBOARDDOCK:
		if (connected)
			kona_set_dock_switch(UEVENT_DOCK_KEYBOARD);
		else
			kona_set_dock_switch(UEVENT_DOCK_NONE);
		break;
	case P30_DESKDOCK:
		if (connected)
			kona_deskdock_attached();
/*			kona_set_dock_switch(UEVENT_DOCK_DESK);*/
		else
			kona_deskdock_detached();
/*			kona_set_dock_switch(UEVENT_DOCK_NONE);*/
		break;
	case P30_CARDOCK:
		if (connected)
			kona_set_dock_switch(UEVENT_DOCK_CAR);
		else
			kona_set_dock_switch(UEVENT_DOCK_NONE);
		break;
	case P30_JIG:
		if (connected) {
			if (kona_otg->uart_manual_mode ==
					KONA_MANUAL_UART_MODEM)
				kona_cp_uart_actions();
			else {
				kona_ap_uart_actions();
				kona_factory_uart_actions();
			}
		}
		break;
	case P30_USB:
		if (connected) {
			if (kona_otg->otg.last_event == USB_EVENT_NONE)
				kona_ap_usb_attach(kona_otg);
		} else {
			if (kona_otg->otg.last_event != USB_EVENT_NONE)
				kona_ap_usb_detach(kona_otg);
		}
		break;
	case P30_TA:
		break;
	case P30_EARJACK_WITH_DOCK:
		if (connected)
			kona_set_audio_switch(UEVENT_EARJACK_ATTACHED);
		else
			kona_set_audio_switch(UEVENT_EARJACK_DETACHED);
		break;
	case P30_ANAL_TV_OUT:
		pr_warning("This accessory is not supported.\n");
		break;
	default:
		pr_warning("wrong cable detection information!(%d)\n", device);
		break;
	}
}

static s16 kona_get_accessory_adc(void)
{
	return omap4_kona_get_adc(ACCESSORY_ID);
}

/* 30pin connector */
struct acc_con_platform_data kona_con_pdata = {
	.detected		= kona_30pin_detected,
	.get_accessory_adc	= kona_get_accessory_adc,
};

struct platform_device kona_device_connector = {
	.name	= "acc_con",
	.id	= -1,
	.dev	= {
		.platform_data = &kona_con_pdata,
	},
};

/*
 * SYSFS
 */
static ssize_t kona_usb_sel_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;
	const char *mode;

	switch (kona_otg->usb_manual_mode) {
	case KONA_MANUAL_USB_AP:
		mode = "PDA";
		break;
	case KONA_MANUAL_USB_MODEM:
		mode = "MODEM";
		break;
	default:
		mode = "NONE";
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t kona_usb_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;
	int old_mode;

	mutex_lock(&kona_otg->lock);

	old_mode = kona_otg->usb_manual_mode;

	if (!strncasecmp(buf, "PDA", 3)) {
		kona_otg->usb_manual_mode = KONA_MANUAL_USB_AP;

		/* If we are transitioning from CP USB to AP USB then notify the
		 * USB stack that is now attached.
		 */
		if ((kona_otg->current_device & BIT(P30_USB)) &&
				(old_mode != KONA_MANUAL_USB_AP)) {
			kona_cp_usb_detach();
			kona_ap_usb_attach(kona_otg);
		}
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		kona_otg->usb_manual_mode = KONA_MANUAL_USB_MODEM;

		/* If we are transitioning from AP USB to CP USB then notify the
		 * USB stack that is has been detached.
		 */
		if ((kona_otg->current_device & BIT(P30_USB)) &&
				(old_mode != KONA_MANUAL_USB_MODEM)) {
			kona_ap_usb_detach(kona_otg);
			kona_cp_usb_attach();
		}
	} else if (!strncasecmp(buf, "NONE", 5)) {
		kona_otg->usb_manual_mode = KONA_MANUAL_USB_NONE;

		/* If we are transitioning from CP USB to AP USB then notify the
		 * USB stack that it is now attached.
		 */
		if ((kona_otg->current_device & BIT(P30_USB)) &&
				(old_mode != KONA_MANUAL_USB_MODEM)) {
			kona_cp_usb_detach();
			kona_ap_usb_attach(kona_otg);
		}
	}

	mutex_unlock(&kona_otg->lock);

	return size;

}

static ssize_t kona_usb_state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{

	struct omap4_otg *kona_otg = &kona_otg_xceiv;
	const char *mode;

	if (kona_otg->current_device & BIT(P30_USB))
		mode = "USB_STATE_CONFIGURED";
	else
		mode = "USB_STATE_NOT_CONFIGURED";

	return sprintf(buf, "%s\n", mode);

}

static ssize_t kona_uart_sel_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;
	const char *mode;

	switch (kona_otg->uart_manual_mode) {
	case KONA_MANUAL_UART_AP:
		mode = NAME_UART_PATH_AP;
		break;
	case KONA_MANUAL_UART_MODEM:
		mode = NAME_UART_PATH_CP;
		break;
	default:
		mode = NAME_UART_PATH_NONE;
	};

	return sprintf(buf, "%s\n", mode);
}

static ssize_t kona_uart_sel_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;

	mutex_lock(&kona_otg->lock);

	if (!strncasecmp(buf, "AP", 2)) {
		kona_otg->uart_manual_mode = KONA_MANUAL_UART_AP;

		if (kona_otg->current_device & BIT(P30_JIG))
			kona_ap_uart_actions();
	} else if (!strncasecmp(buf, "CP", 2)) {
		kona_otg->uart_manual_mode = KONA_MANUAL_UART_MODEM;

		if (kona_otg->current_device & BIT(P30_JIG))
			kona_cp_uart_actions();
	} else if (!strncasecmp(buf, "NONE", 4)) {
		kona_otg->uart_manual_mode = KONA_MANUAL_UART_NONE;

		if (kona_otg->current_device & BIT(P30_JIG))
			kona_ap_uart_actions();
	}

	mutex_unlock(&kona_otg->lock);

	return size;
}

static ssize_t kona_jig_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;
	const char *mode;

	if (kona_otg->current_device & BIT(P30_JIG))
		mode = "1";
	else
		mode = "0";

	return sprintf(buf, "%s\n", mode);
}

static ssize_t kona_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	s16 adc_val;
	u8 key_string_on;

	adc_val = kona_get_accessory_adc();
	pr_info("accessory_id adc value = %d\n", adc_val);

	if ((3600 < adc_val) && (adc_val < 3800))
		key_string_on = 0x1C;
	else
		key_string_on = 0;

	return sprintf(buf, "%x\n", key_string_on);
}


static DEVICE_ATTR(usb_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			kona_usb_sel_show, kona_usb_sel_store);
static DEVICE_ATTR(uart_sel, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			kona_uart_sel_show, kona_uart_sel_store);
static DEVICE_ATTR(usb_state, S_IRUGO, kona_usb_state_show, NULL);
static DEVICE_ATTR(jig_on, S_IRUSR | S_IWUSR, kona_jig_on_show, NULL);
static DEVICE_ATTR(adc, S_IRUSR | S_IRGRP, kona_adc_show, NULL);

static struct attribute *manual_switch_sel_attributes[] = {
	&dev_attr_usb_sel.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_jig_on.attr,
	&dev_attr_adc.attr,
	NULL,
};

static const struct attribute_group manual_switch_sel_group = {
	.attrs	= manual_switch_sel_attributes,
};

static void __init kona_switch_initial_setup(void)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;

	if (init_switch_sel & MASK_SWITCH_UART_AP) {
		kona_ap_uart_actions();
		kona_otg->uart_manual_mode = KONA_MANUAL_UART_AP;
	} else {
		kona_cp_uart_actions();
		kona_otg->uart_manual_mode = KONA_MANUAL_UART_MODEM;
	}

	if (init_switch_sel & MASK_SWITCH_USB_AP) {
		kona_cp_usb_detach();
		kona_otg->usb_manual_mode = KONA_MANUAL_USB_AP;
	} else {
		kona_cp_usb_attach();
		kona_otg->usb_manual_mode = KONA_MANUAL_USB_MODEM;
	}

}

static int __init kona_save_init_switch_param(char *str)
{
	int ret;

	ret = kstrtoint(str, 10, &init_switch_sel);
	if (ret < 0)
		return ret;

	return 0;
}
__setup("switch_sel=", kona_save_init_switch_param);

#ifdef CONFIG_USB_HOST_NOTIFY
static void kona_booster(int enable)
{
	struct otg_transceiver *otg_t;

	pr_info("[%s] enable=%d\n", __func__, enable);

	otg_t = otg_get_transceiver();
	if (!otg_t) {
		pr_err("[%s] otg tranceiver is NULL", __func__);
		return;
	}

	kona_otg_set_vbus(otg_t, !!enable);
	return;
}

struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name     = "usb_otg",
	.booster       = kona_booster,
	.thread_enable = 0,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};

static void kona_host_notifier_init(struct omap4_otg *otg)
{
	int acc_out =
	omap_muxtbl_get_gpio_by_name("V_ACCESSORY_OUT_5V");
	if (unlikely(acc_out == -EINVAL))
		dev_err(&otg->dev, "V_ACCESSORY_OUT_5V is invalid.\n");
	host_notifier_pdata.gpio = acc_out;
	otg->pdata = &host_notifier_pdata;
	platform_device_register(&host_notifier_device);
}
#endif

/* dock keyboard */
static struct dock_keyboard_callback {
	struct input_dev *dev;
	int (*cb) (struct input_dev *dev, bool connected);
} kona_dock_keyboard_cb;

static int kona_dock_keyboard_callback(bool connected)
{
	if (kona_dock_keyboard_cb.dev && kona_dock_keyboard_cb.cb)
		return kona_dock_keyboard_cb.cb(kona_dock_keyboard_cb.dev,
								connected);
	return 0;
}

static void kona_dock_keyboard_power(bool on)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;

	printk(KERN_DEBUG "kbd: dock_keyboard_power %d\n", on);

	if (on) {
		uart_set_l3_cstr(true);
		if (kona_otg->uart_manual_mode ==
		    KONA_MANUAL_UART_MODEM) {
			pr_info("kbd: switch UART IF to AP\n");
			kona_ap_uart_actions();
		}
		kona_dock_uart_actions();
		kona_accessory_power(1, 1);
	} else {
		kona_accessory_power(1, 0);
		if (kona_otg->uart_manual_mode ==
		    KONA_MANUAL_UART_MODEM) {
			pr_info("kbd: switch UART IF to CP\n");
			kona_cp_uart_actions();
		}
		kona_factory_uart_actions();
		uart_set_l3_cstr(false);
	}
}

static void kona_dock_keyboard_register_cb(struct input_dev *dev, void *cb)
{
	kona_dock_keyboard_cb.dev = dev;
	kona_dock_keyboard_cb.cb = cb;
}

struct dock_keyboard_platform_data kona_dock_keyboard_pdata = {
	.power = kona_dock_keyboard_power,
	.register_cb = kona_dock_keyboard_register_cb,
};

struct platform_device kona_device_dock_keyboard = {
	.name = KBD_DRV_NAME,
	.id = 0,
	.dev = {
		.platform_data = &kona_dock_keyboard_pdata,
		},
};

static void connector_gpio_init(void)
{
	unsigned int nr_gpio_con = ARRAY_SIZE(connector_gpios);
	unsigned int i;

	if (unlikely(system_rev < 3))
		nr_gpio_con--;

	i = nr_gpio_con - 1;
	do {
		connector_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(connector_gpios[i].label);
		if (connector_gpios[i].gpio < 0) {
			pr_err("%s: failed to get gpio, label = %s\n",
					__func__, connector_gpios[i].label);
			return;
		}
	} while (i-- != 0);

	gpio_request_array(connector_gpios, nr_gpio_con);

#ifdef CONFIG_SII9234
	for (i = 0; i < ARRAY_SIZE(mhl_gpios); i++)
		mhl_gpios[i].gpio =
		omap_muxtbl_get_gpio_by_name(mhl_gpios[i].label);

	gpio_request_array(mhl_gpios, ARRAY_SIZE(mhl_gpios));

	kona_i2c9_boardinfo[0].irq =
		gpio_to_irq(mhl_gpios[GPIO_MHL_INT].gpio);
#endif
}

static void kona_usb_gpio_init(void)
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

void __init omap4_kona_connector_init(void)
{
	struct omap4_otg *kona_otg = &kona_otg_xceiv;
	int ret;

	connector_gpio_init();
	kona_usb_gpio_init();
	mutex_init(&kona_otg->lock);
	mutex_init(&kona_otg->vusb_lock);
	mutex_init(&kona_otg->vbus_drive_lock);
	INIT_WORK(&kona_otg->set_vbus_work, kona_otg_work);

	device_initialize(&kona_otg->dev);
	dev_set_name(&kona_otg->dev, "%s", "kona_otg");
	ret = device_add(&kona_otg->dev);
	if (ret) {
		pr_err("%s: cannot reg device '%s' (%d)\n", __func__,
		       dev_name(&kona_otg->dev), ret);
		return;
	}

	dev_set_drvdata(&kona_otg->dev, kona_otg);

	kona_otg->otg.dev			= &kona_otg->dev;
	kona_otg->otg.label			= "kona_otg_xceiv";
	kona_otg->otg.set_host		= kona_otg_set_host;
	kona_otg->otg.set_peripheral	= kona_otg_set_peripheral;
	kona_otg->otg.set_suspend		= kona_otg_set_suspend;
	kona_otg->otg.set_vbus		= kona_otg_set_vbus;
	kona_otg->otg.init			= kona_otg_phy_init;
	kona_otg->otg.shutdown		= kona_otg_phy_shutdown;
	kona_otg->otg.is_active		= kona_otg_is_active;
/* start_hnp function is used for vbus reset. */
	kona_otg->otg.start_hnp		= kona_otg_vbus_reset;

	ATOMIC_INIT_NOTIFIER_HEAD(&kona_otg->otg.notifier);

	ret = otg_set_transceiver(&kona_otg->otg);
	if (ret)
		pr_err("kona_otg: cannot set transceiver (%d)\n", ret);

	kona_otg->switch_dev =
			device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(kona_otg->switch_dev)) {
		pr_err("(%s): failed to created device (switch_dev)!\n",
								__func__);
		goto switch_dev_fail;
	}

	omap4430_phy_init(&kona_otg->dev);
	kona_otg_set_suspend(&kona_otg->otg, 0);
#ifdef CONFIG_USB_HOST_NOTIFY
	kona_host_notifier_init(kona_otg);
#endif

	dev_set_drvdata(kona_otg->switch_dev, kona_otg);
	ret = sysfs_create_group(&kona_otg->switch_dev->kobj,
						&manual_switch_sel_group);
	if (ret < 0)
		pr_err("fail to  create switch_sel sysfs group (%d)\n", ret);

switch_dev_fail:


	kona_switch_initial_setup();

	/* dock keyboard */
	kona_dock_keyboard_pdata.dock_irq_gpio =
	    connector_gpios[GPIO_ACCESSORY_INT].gpio;

	platform_device_register(&kona_device_dock_keyboard);

	/* ADC IC (STMPE811) */
	i2c_register_board_info(8, kona_i2c8_boardinfo,
					ARRAY_SIZE(kona_i2c8_boardinfo));
#ifdef CONFIG_SII9234
	/* MHL (SII9244) */
	i2c_register_board_info(9, kona_i2c9_boardinfo,
					ARRAY_SIZE(kona_i2c9_boardinfo));
#endif

	/* 30pin connector */
	kona_con_pdata.accessory_irq_gpio =
				connector_gpios[GPIO_ACCESSORY_INT].gpio;
	kona_con_pdata.dock_irq_gpio = connector_gpios[GPIO_DOCK_INT].gpio;
	kona_con_pdata.jig_on_gpio = connector_gpios[GPIO_JIG_ON].gpio;
	kona_con_pdata.dock_keyboard_cb = kona_dock_keyboard_callback;

	platform_device_register(&kona_device_connector);

	kona_otg->dock_switch.name = "dock";
	switch_dev_register(&kona_otg->dock_switch);

	kona_otg->audio_switch.name = "usb_audio";
	switch_dev_register(&kona_otg->audio_switch);

	kona_otg->current_device = 0;
}

static int __init omap4_kona_connector_late_init(void)
{
	bool factory_mode;

	factory_mode = gpio_get_value(connector_gpios[GPIO_JIG_ON].gpio) &&
		!strncmp(sec_androidboot_mode, "jig", 3);

	if (unlikely(factory_mode))
		uart_set_l3_cstr(true);

	return 0;
}

late_initcall(omap4_kona_connector_late_init);

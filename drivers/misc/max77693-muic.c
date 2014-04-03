/*
 * max77693-muic.c - MUIC driver for the Maxim 77693
 *
 *  Copyright (C) 2012 Samsung Electronics
 *  <sukdong.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/delay.h>

/* USB ID ADC Value */
#define ADC_GND			0x00
#define ADC_MHL			0x01
#define ADC_DOCK_PREV_KEY	0x04
#define ADC_DOCK_NEXT_KEY	0x07
#define ADC_DOCK_VOL_DN		0x0a /* 0x01010 14.46K ohm */
#define ADC_DOCK_VOL_UP		0x0b /* 0x01011 17.26K ohm */
#define ADC_DOCK_PLAY_PAUSE_KEY 0x0d
#define ADC_SMARTDOCK		0x10 /* 0x10000 40.2K ohm */
#define ADC_CEA936ATYPE1_CHG	0x17 /* 0x10111 200K ohm */
#define ADC_JIG_USB_OFF		0x18 /* 0x11000 255K ohm */
#define ADC_JIG_USB_ON		0x19 /* 0x11001 301K ohm */
#define ADC_DESKDOCK		0x1a /* 0x11010 365K ohm */
#define ADC_CEA936ATYPE2_CHG	0x1b /* 0x11011 442K ohm */
#define ADC_JIG_UART_OFF	0x1c /* 0x11100 523K ohm */
#define ADC_JIG_UART_ON		0x1d /* 0x11101 619K ohm */
#define ADC_CARDOCK		0x1d /* 0x11101 619K ohm */
#define ADC_OPEN		0x1f

/* MAX77693 MUIC CHG_TYP setting values */
enum {
	/* No Valid voltage at VB (Vvb < Vvbdet) */
	CHGTYP_NO_VOLTAGE	= 0x00,
	/* Unknown (D+/D- does not present a valid USB charger signature) */
	CHGTYP_USB		= 0x01,
	/* Charging Downstream Port */
	CHGTYP_DOWNSTREAM_PORT	= 0x02,
	/* Dedicated Charger (D+/D- shorted) */
	CHGTYP_DEDICATED_CHGR	= 0x03,
	/* Special 500mA charger, max current 500mA */
	CHGTYP_500MA		= 0x04,
	/* Special 1A charger, max current 1A */
	CHGTYP_1A		= 0x05,
	/* Reserved for Future Use */
	CHGTYP_RFU		= 0x06,
	/* Dead Battery Charging, max current 100mA */
	CHGTYP_DB_100MA		= 0x07,
	CHGTYP_MAX,

	CHGTYP_INIT,
	CHGTYP_MIN = CHGTYP_NO_VOLTAGE
};

enum {
	DOCK_KEY_NONE			= 0,
	DOCK_KEY_VOL_UP_PRESSED,
	DOCK_KEY_VOL_UP_RELEASED,
	DOCK_KEY_VOL_DOWN_PRESSED,
	DOCK_KEY_VOL_DOWN_RELEASED,
	DOCK_KEY_PREV_PRESSED,
	DOCK_KEY_PREV_RELEASED,
	DOCK_KEY_PLAY_PAUSE_PRESSED,
	DOCK_KEY_PLAY_PAUSE_RELEASED,
	DOCK_KEY_NEXT_PRESSED,
	DOCK_KEY_NEXT_RELEASED,
};

struct max77693_muic_info {
	struct device		*dev;
	struct max77693_dev	*max77693;
	struct i2c_client	*muic;
	struct max77693_muic_data *muic_data;
	int			irq_adc;
	int			irq_chgtype;
	int			irq_vbvolt;
	int			irq_adc1k;
	int			mansw;

	enum cable_type_muic	cable_type;
	struct delayed_work	init_work;
	struct delayed_work	usb_work;
	struct delayed_work	mhl_work;
	struct mutex		mutex;

	struct input_dev	*input;
	int			previous_key;
	u8 chg_int_state; /* For restore charger interrupt states */
};
static struct max77693_muic_info *gInfo;	/* for providing API */

static const char const *cable_type_name[] = {
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

/* support otg control */
static void max77693_otg_control(struct max77693_muic_info *info, int enable)
{
	u8 int_mask, cdetctrl1, chg_cnfg_00;
	pr_info("%s: enable(%d)\n", __func__, enable);

	if (enable) {
		/* disable charger interrupt */
		max77693_read_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_INT_MASK, &int_mask);
		info->chg_int_state = int_mask;
		int_mask |= (1 << 4);	/* disable chgin intr */
		int_mask |= (1 << 6);	/* disable chg */
		int_mask &= ~(1 << 0);	/* enable byp intr */
		max77693_write_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_INT_MASK, int_mask);

		/* disable charger detection */
		max77693_read_reg(info->max77693->muic,
			MAX77693_MUIC_REG_CDETCTRL1, &cdetctrl1);
		cdetctrl1 &= ~(1 << 0);
		max77693_write_reg(info->max77693->muic,
			MAX77693_MUIC_REG_CDETCTRL1, cdetctrl1);

		/* OTG on, boost on, DIS_MUIC_CTRL=1 */
		max77693_read_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, &chg_cnfg_00);
		chg_cnfg_00 &= ~(CHG_CNFG_00_CHG_MASK
				| CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BUCK_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		chg_cnfg_00 |= (CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		max77693_write_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, chg_cnfg_00);
	} else {
		/* OTG off, boost off, (buck on),
		   DIS_MUIC_CTRL = 0 unless CHG_ENA = 1 */
		max77693_read_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, &chg_cnfg_00);
		chg_cnfg_00 &= ~(CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		chg_cnfg_00 |= CHG_CNFG_00_BUCK_MASK;
		max77693_write_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, chg_cnfg_00);

		msleep(50);

		/* enable charger detection */
		max77693_read_reg(info->max77693->muic,
			MAX77693_MUIC_REG_CDETCTRL1, &cdetctrl1);
		cdetctrl1 |= (1 << 0);
		max77693_write_reg(info->max77693->muic,
			MAX77693_MUIC_REG_CDETCTRL1, cdetctrl1);

		/* enable charger interrupt */
		max77693_write_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_INT_MASK, info->chg_int_state);
		max77693_read_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_INT_MASK, &int_mask);
	}

	pr_debug("%s: INT_MASK(0x%x), CDETCTRL1(0x%x), CHG_CNFG_00(0x%x)\n",
				__func__, int_mask, cdetctrl1, chg_cnfg_00);
}

/* use in mach for otg */
void otg_control(int enable)
{
	pr_debug("%s: enable(%d)\n", __func__, enable);

	max77693_otg_control(gInfo, enable);
}
EXPORT_SYMBOL(otg_control);

static void max77693_powered_otg_control(struct max77693_muic_info *info,
								int enable)
{
	u8 reg_data = 0;
	pr_debug("%s: enable(%d)\n", __func__, enable);

	if (enable) {
		/* OTG on, boost on */
		max77693_write_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, 0x05);

		reg_data = 0x0E;
		reg_data |= MAX77693_OTG_ILIM;
		max77693_write_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_02, reg_data);
	} else {
		/* OTG off, boost off, (buck on) */
		max77693_write_reg(info->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, 0x04);
	}
}

/* use in mach for powered-otg */
void powered_otg_control(int enable)
{
	pr_debug("%s: enable(%d)\n", __func__, enable);

	max77693_powered_otg_control(gInfo, enable);
}
EXPORT_SYMBOL(powered_otg_control);

int max77693_muic_get_cable_type(void)
{
	return gInfo->cable_type;
}
EXPORT_SYMBOL(max77693_muic_get_cable_type);

int max77693_muic_get_status1_adc1k_value(void)
{
	u8 adc1k;
	int ret;

	ret = max77693_read_reg(gInfo->muic, MAX77693_MUIC_REG_STATUS1, &adc1k);
	if (ret) {
		dev_err(gInfo->dev, "%s: fail to read muic reg(%d)\n",
					__func__, ret);
		return -EINVAL;
	}
	adc1k = adc1k & STATUS1_ADC1K_MASK ? 1 : 0;

	pr_debug("func:%s, adc1k: %d\n", __func__, adc1k);
	/* -1:err, 0:adc1k not detected, 1:adc1k detected */
	return adc1k;
}
EXPORT_SYMBOL(max77693_muic_get_status1_adc1k_value);

int max77693_muic_get_status1_adc_value(void)
{
	u8 adc;
	int ret;

	ret = max77693_read_reg(gInfo->muic,
		MAX77693_MUIC_REG_STATUS1, &adc);
	if (ret) {
		dev_err(gInfo->dev, "%s: fail to read muic reg(%d)\n",
			__func__, ret);
		return -EINVAL;
	}

	return adc & STATUS1_ADC_MASK;
}
EXPORT_SYMBOL(max77693_muic_get_status1_adc_value);

static void max77693_muic_set_adc_debound(struct max77693_muic_info *info,
				       int value)
{
	int ret;
	u8 val;

	dev_info(info->dev, "set adc debound time(%x)\n", value);

	if (value > 3) {
		dev_err(info->dev, "%s: invalid value(%x)\n", __func__, value);
		return;
	}

	if (!info->muic) {
		dev_err(info->dev, "%s: no muic i2c client\n", __func__);
		return;
	}

	val = value << CTRL3_ADCDBSET_SHIFT;
	ret = max77693_update_reg(info->muic, MAX77693_MUIC_REG_CTRL3, val,
							CTRL3_ADCDBSET_MASK);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to update reg\n", __func__);
}

static int max77693_muic_handle_dock_vol_key(struct max77693_muic_info *info,
					     u8 adc)
{
	struct input_dev *input = info->input;
	int pre_key = info->previous_key;
	unsigned int code;
	int state;

	if (adc == ADC_OPEN) {
		switch (pre_key) {
		case DOCK_KEY_VOL_UP_PRESSED:
			code = KEY_VOLUMEUP;
			info->previous_key = DOCK_KEY_VOL_UP_RELEASED;
			break;
		case DOCK_KEY_VOL_DOWN_PRESSED:
			code = KEY_VOLUMEDOWN;
			info->previous_key = DOCK_KEY_VOL_DOWN_RELEASED;
			break;
		case DOCK_KEY_PREV_PRESSED:
			code = KEY_PREVIOUSSONG;
			info->previous_key = DOCK_KEY_PREV_RELEASED;
			break;
		case DOCK_KEY_PLAY_PAUSE_PRESSED:
			code = KEY_PLAYPAUSE;
			info->previous_key = DOCK_KEY_PLAY_PAUSE_RELEASED;
			break;
		case DOCK_KEY_NEXT_PRESSED:
			code = KEY_NEXTSONG;
			info->previous_key = DOCK_KEY_NEXT_RELEASED;
			break;
		default:
			return 0;
		}

		input_event(input, EV_KEY, code, 0);
		input_sync(input);
		return 0;
	}

	if (pre_key == DOCK_KEY_NONE) {
		/*
		if (adc != ADC_DOCK_VOL_UP && adc != ADC_DOCK_VOL_DN && \
		adc != ADC_DOCK_PREV_KEY && adc != ADC_DOCK_PLAY_PAUSE_KEY \
		&& adc != ADC_DOCK_NEXT_KEY)
		*/
		if ((adc < 0x03) || (adc > 0x0d))
			return 0;
	}

	dev_info(info->dev, "%s: dock vol key(%d)\n", __func__, pre_key);

	state = 1;

	switch (adc) {
	case ADC_DOCK_VOL_UP:
		code = KEY_VOLUMEUP;
		info->previous_key = DOCK_KEY_VOL_UP_PRESSED;
		break;
	case ADC_DOCK_VOL_DN:
		code = KEY_VOLUMEDOWN;
		info->previous_key = DOCK_KEY_VOL_DOWN_PRESSED;
		break;
	case ADC_DOCK_PREV_KEY-1 ... ADC_DOCK_PREV_KEY+1:
		code = KEY_PREVIOUSSONG;
		info->previous_key = DOCK_KEY_PREV_PRESSED;
		break;
	case ADC_DOCK_PLAY_PAUSE_KEY-1 ... ADC_DOCK_PLAY_PAUSE_KEY+1:
		code = KEY_PLAYPAUSE;
		info->previous_key = DOCK_KEY_PLAY_PAUSE_PRESSED;
		break;
	case ADC_DOCK_NEXT_KEY-1 ... ADC_DOCK_NEXT_KEY+1:
		code = KEY_NEXTSONG;
		info->previous_key = DOCK_KEY_NEXT_PRESSED;
		break;
	case ADC_DESKDOCK: /* key release routine */
		state = 0;
		if (pre_key == DOCK_KEY_VOL_UP_PRESSED) {
			code = KEY_VOLUMEUP;
			info->previous_key = DOCK_KEY_VOL_UP_RELEASED;
		} else if (pre_key == DOCK_KEY_VOL_DOWN_PRESSED) {
			code = KEY_VOLUMEDOWN;
			info->previous_key = DOCK_KEY_VOL_DOWN_RELEASED;
		} else if (pre_key == DOCK_KEY_PREV_PRESSED) {
			code = KEY_PREVIOUSSONG;
			info->previous_key = DOCK_KEY_PREV_RELEASED;
		} else if (pre_key == DOCK_KEY_PLAY_PAUSE_PRESSED) {
			code = KEY_PLAYPAUSE;
			info->previous_key = DOCK_KEY_PLAY_PAUSE_RELEASED;
		} else if (pre_key == DOCK_KEY_NEXT_PRESSED) {
			code = KEY_NEXTSONG;
			info->previous_key = DOCK_KEY_NEXT_RELEASED;
		} else {
			dev_warn(info->dev, "%s:%d should not reach here\n",
				 __func__, __LINE__);
			return 0;
		}
		break;
	default:
		dev_warn(info->dev, "%s: unsupported ADC(0x%02x)\n", __func__,
			 adc);
		return 0;
	}

	input_event(input, EV_KEY, code, state);
	input_sync(input);

	return 1;
}

static int max77693_muic_set_path(struct max77693_muic_info *info, int path)
{
	struct i2c_client *client = info->muic;
	struct max77693_muic_data *mdata = info->muic_data;
	int ret;
	u8 ctrl1_val, ctrl1_msk;
	u8 ctrl2_val, ctrl2_msk;
	int val;

	dev_info(info->dev, "set path to (%d)\n", path);

	if (mdata->set_safeout) {
		ret = mdata->set_safeout(path);
		if (ret) {
			dev_err(info->dev, "fail to set safout!\n");
			return ret;
		}
	}

	if ((info->max77693->pmic_rev == MAX77693_REV_PASS1) &&
			((path == PATH_USB_CP) || (path == PATH_UART_CP))) {
		dev_info(info->dev, "MAX77693 PASS1 version doesn't support "
				"manual path setting to CP.\n");
		if (path == PATH_USB_CP)
			path = PATH_USB_AP;
		else if (path == PATH_UART_CP)
			path = PATH_UART_AP;
	}

	switch (path) {
	case PATH_USB_AP:
		val = MAX77693_MUIC_CTRL1_BIN_1_001;
		break;
	case PATH_AUDIO:
		val = MAX77693_MUIC_CTRL1_BIN_2_010;
		break;
	case PATH_UART_AP:
		val = MAX77693_MUIC_CTRL1_BIN_3_011;
		break;
	case PATH_USB_CP:
		val = MAX77693_MUIC_CTRL1_BIN_4_100;
		break;
	case PATH_UART_CP:
		val = MAX77693_MUIC_CTRL1_BIN_5_101;
		break;
	default:
		dev_warn(info->dev, "invalid path(%d)\n", path);
		return -EINVAL;
	}

	ctrl1_val = (val << COMN1SW_SHIFT) | (val << COMP2SW_SHIFT);
	ctrl1_msk = COMN1SW_MASK | COMP2SW_MASK;

	if (path == PATH_AUDIO) {
		ctrl1_val |= 0 << MICEN_SHIFT;
		ctrl1_msk |= MICEN_MASK;
	}

	max77693_update_reg(client, MAX77693_MUIC_REG_CTRL1, ctrl1_val,
								ctrl1_msk);

	ctrl2_val = CTRL2_CPEn1_LOWPWD0;
	ctrl2_msk = CTRL2_CPEn_MASK | CTRL2_LOWPWD_MASK;

	max77693_update_reg(client, MAX77693_MUIC_REG_CTRL2, ctrl2_val,
								ctrl2_msk);

	return 0;
}

static int set_muic_path(struct max77693_muic_info *info, int cable)
{
	int ret = 0;

	switch (cable) {
	case CABLE_TYPE_JIG_UART:
		/* set path to uart following uart_sel */
		if (info->muic_data->uart_sel == UART_SEL_CP)
			ret = max77693_muic_set_path(info, PATH_UART_CP);
		else
			ret = max77693_muic_set_path(info, PATH_UART_AP);
		break;
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB:
		/* set path to usb following usb_sel */
		if (info->muic_data->usb_sel == USB_SEL_CP)
			ret = max77693_muic_set_path(info, PATH_USB_CP);
		else
			ret = max77693_muic_set_path(info, PATH_USB_AP);
		break;
	case CABLE_TYPE_OTG_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
		/* set path to ap usb */
		ret = max77693_muic_set_path(info, PATH_USB_AP);
		msleep(40);
		break;
	case CABLE_TYPE_DESKDOCK_MUIC:
	case CABLE_TYPE_CARDOCK_MUIC:
		/* set path to audio? */
		ret = max77693_muic_set_path(info, PATH_AUDIO);
		break;
	default:
		break;
	}

	return ret;
}

void max77693_muic_set_usb_sel(int usb_sel)
{
	struct max77693_muic_info *info = gInfo;

	if (info->muic_data->usb_sel == usb_sel)
		return;

	info->muic_data->usb_sel = usb_sel;
	set_muic_path(info, info->cable_type);
}
EXPORT_SYMBOL(max77693_muic_set_usb_sel);

void max77693_muic_set_uart_sel(int uart_sel)
{
	struct max77693_muic_info *info = gInfo;

	if (info->muic_data->uart_sel == uart_sel)
		return;

	info->muic_data->uart_sel = uart_sel;
	set_muic_path(info, info->cable_type);
}
EXPORT_SYMBOL(max77693_muic_set_uart_sel);

int max77693_muic_get_usb_sel(void)
{
	struct max77693_muic_info *info = gInfo;
	return info->muic_data->usb_sel;
}
EXPORT_SYMBOL(max77693_muic_get_usb_sel);

int max77693_muic_get_uart_sel(void)
{
	struct max77693_muic_info *info = gInfo;
	return info->muic_data->uart_sel;
}
EXPORT_SYMBOL(max77693_muic_get_uart_sel);

static void _detected(struct max77693_muic_info *info, int cable, bool attach)
{
	enum cable_type_muic prev_cbl = info->cable_type;

	dev_info(info->dev, "[%s] prev_cbl = %d, cable = %d, attach = %d\n",
		__func__, prev_cbl, cable, attach);

	/* skip duplicate detection */
	if (attach) {
		if (prev_cbl == cable) {
			dev_info(info->dev, "skip duplicate connection\n");
			return;
		}
	} else {
		if (prev_cbl == CABLE_TYPE_NONE_MUIC) {
			dev_info(info->dev, "skip duplicate disconnection\n");
			return;
		}
	}

	/* If attached cable status changed without disconnection,
	 * muic detects new cable without previous cable detached event.
	 * This calls previous cable detachment detection
	 * before new cable attachment detection.
	 */
	if (attach && prev_cbl != CABLE_TYPE_NONE_MUIC) {
		dev_info(info->dev, "CABLE_TYPE_NONE_MUIC\n");
		_detected(info, prev_cbl, 0);
		return;
	}

	if (attach)
		info->cable_type = cable;
	else
		info->cable_type = CABLE_TYPE_NONE_MUIC;

	dev_info(info->dev, "cable detect change. from '%s' to '%s'\n",
					cable_type_name[prev_cbl],
					cable_type_name[info->cable_type]);

	if (attach)
		set_muic_path(info, cable);

	if (info->muic_data->detected)
		info->muic_data->detected(cable, attach);
}

static int max77693_muic_handle_attach(struct max77693_muic_info *info,
				       u8 status1, u8 status2, int irq)
{
	u8 adc;
	u8 vbvolt;
	u8 chgtyp;
	u8 chgdetrun;
	u8 adc1k;
	enum cable_type_muic new_cbl;

	adc = status1 & STATUS1_ADC_MASK;
	adc1k = status1 & STATUS1_ADC1K_MASK;
	chgtyp = status2 & STATUS2_CHGTYP_MASK;
	vbvolt = status2 & STATUS2_VBVOLT_MASK;
	chgdetrun = status2 & STATUS2_CHGDETRUN_MASK;

	dev_info(info->dev, "st1:0x%x st2:0x%x cable_type:%d, adc=0x%x\n",
				status1, status2, info->cable_type, adc);

	/* Workaround for Factory mode in MUIC PASS2.
	 * Abandon adc interrupt of approximately +-100K range
	 * if previous cable status was JIG UART BOOT OFF.
	 * In uart path cp, adc is unstable state
	 * MUIC PASS2 turn to AP_UART mode automatically
	 * So, in this state set correct path manually.
	 */
	if (info->max77693->pmic_rev > MAX77693_REV_PASS1) {
		if ((info->cable_type == CABLE_TYPE_JIG_UART) &&
			((adc == ADC_JIG_UART_OFF + 1 ||
			(adc == ADC_JIG_UART_OFF - 1))) &&
			(info->muic_data->uart_sel == PATH_UART_CP)) {
			_detected(info, CABLE_TYPE_JIG_UART, 1);
			dev_warn(info->dev, "abandon ADC\n");
			return 0;
		}
	}

	new_cbl = CABLE_TYPE_NONE_MUIC;

	switch (adc) {
	case ADC_GND:
		if (chgtyp == CHGTYP_NO_VOLTAGE)
			new_cbl = CABLE_TYPE_OTG_MUIC;
		else if (chgtyp == CHGTYP_USB ||
				chgtyp == CHGTYP_DOWNSTREAM_PORT ||
				chgtyp == CHGTYP_DEDICATED_CHGR ||
				chgtyp == CHGTYP_500MA ||
				chgtyp == CHGTYP_1A) {
			dev_info(info->dev, "OTG charging pump\n");
			_detected(info, CABLE_TYPE_CHARGER, 1);
			break;
		}
		break;
	case ADC_SMARTDOCK:
		if (vbvolt)
			new_cbl = CABLE_TYPE_SMARTDOCK_WITH_TA;
		else
			new_cbl = CABLE_TYPE_SMARTDOCK_MUIC;
		break;
	case ADC_JIG_UART_OFF:
	case ADC_CARDOCK:
		/* Some jigs have same adc with cardock. */
		new_cbl = CABLE_TYPE_JIG_UART;
		break;
	case ADC_JIG_USB_OFF:
		new_cbl = CABLE_TYPE_USB_MUIC;
		break;
	case ADC_JIG_USB_ON:
		new_cbl = CABLE_TYPE_JIG_USB;
		break;
	case ADC_DESKDOCK:
		if (vbvolt)
			new_cbl = CABLE_TYPE_DESKDOCK_WITH_TA;
		else
			new_cbl = CABLE_TYPE_DESKDOCK_MUIC;

		break;
	case ADC_CEA936ATYPE1_CHG:
	case ADC_CEA936ATYPE2_CHG:
	case ADC_OPEN:
		switch (chgtyp) {
		case CHGTYP_USB:
			if (adc == ADC_CEA936ATYPE1_CHG ||
					adc == ADC_CEA936ATYPE2_CHG)
				break;
			new_cbl = CABLE_TYPE_USB_MUIC;
			break;
		case CHGTYP_DOWNSTREAM_PORT:
		case CHGTYP_DEDICATED_CHGR:
		case CHGTYP_500MA:
		case CHGTYP_1A:
			new_cbl = CABLE_TYPE_TA_MUIC;
			break;
		default:
			break;
		}
		break;
	default:
		new_cbl = CABLE_TYPE_UNKNOWN_MUIC;
		dev_warn(info->dev, "unsupported adc=0x%x\n", adc);
		break;
	}

	if (new_cbl == CABLE_TYPE_NONE_MUIC) {
		dev_warn(info->dev, "Failed to get cable type (adc=0x%x)\n",
				adc);
		return -1;
	}

	_detected(info, new_cbl, 1);

	return 0;
}

static int max77693_muic_handle_detach(struct max77693_muic_info *info, int irq)
{
	struct i2c_client *client = info->muic;
	u8 ctrl2_val;

	/* Workaround: irq doesn't occur after detaching mHL cable */
	max77693_write_reg(client, MAX77693_MUIC_REG_CTRL1,
				MAX77693_MUIC_CTRL1_BIN_0_000);

	/* Enable Factory Accessory Detection State Machine */
	ctrl2_val =  (1 << CTRL2_ACCDET_SHIFT);
	max77693_update_reg(client, MAX77693_MUIC_REG_CTRL2, ctrl2_val,
							CTRL2_ACCDET_MASK);

	max77693_update_reg(client, MAX77693_MUIC_REG_CTRL2,
				CTRL2_CPEn0_LOWPWD1,
				CTRL2_CPEn_MASK | CTRL2_LOWPWD_MASK);

	max77693_read_reg(client, MAX77693_MUIC_REG_CTRL2, &ctrl2_val);
	dev_info(info->dev, "%s: CNTL2(0x%02x)\n", __func__, ctrl2_val);

	info->previous_key = DOCK_KEY_NONE;

	_detected(info, info->cable_type, 0);

	return 0;
}

static void max77693_muic_detect_dev(struct max77693_muic_info *info, int irq)
{
	struct i2c_client *client = info->muic;
	u8 status[2];
	u8 adc;
	u8 chgtyp;
	u8 adcerr;
	int intr = INT_ATTACH;
	int ret;

	dev_info(info->dev, "detected accesories, irq:%d\n", irq);

	ret = max77693_bulk_read(client, MAX77693_MUIC_REG_STATUS1, 2, status);
	if (ret) {
		dev_err(info->dev, "fail to read muic status1(%d)\n", ret);
		return;
	}
	dev_info(info->dev, "STATUS1:0x%x, 2:0x%x\n", status[0], status[1]);

	adc = status[0] & STATUS1_ADC_MASK;
	adcerr = status[0] & STATUS1_ADCERR_MASK;
	chgtyp = status[1] & STATUS2_CHGTYP_MASK;

	dev_info(info->dev, "adc:%x adcerr:%x chgtyp:%x cable_type:%x\n",
					adc, adcerr, chgtyp, info->cable_type);

	if (adcerr) {
		dev_err(info->dev, "adc error has occured.\n");
		return;
	}

	if ((info->cable_type == CABLE_TYPE_DESKDOCK_MUIC) &&
			(irq == info->irq_adc) && (adc != ADC_OPEN)) {
		/* To Do: adc condition should be modifed */
		max77693_muic_handle_dock_vol_key(info, adc);
		return;
	}

	if (adc == ADC_OPEN) {
		if (chgtyp == CHGTYP_NO_VOLTAGE)
			intr = INT_DETACH;
		else if (chgtyp == CHGTYP_USB ||
			 chgtyp == CHGTYP_DOWNSTREAM_PORT ||
			 chgtyp == CHGTYP_DEDICATED_CHGR ||
			 chgtyp == CHGTYP_500MA || chgtyp == CHGTYP_1A) {
			if (info->cable_type == CABLE_TYPE_OTG_MUIC ||
			    info->cable_type == CABLE_TYPE_DESKDOCK_MUIC ||
			    info->cable_type == CABLE_TYPE_CARDOCK_MUIC)
				intr = INT_DETACH;
		}
	}

	if (intr == INT_ATTACH) {
		dev_info(info->dev, "%s: ATTACHED\n", __func__);
		max77693_muic_handle_attach(info, status[0], status[1], irq);
	} else {
		dev_info(info->dev, "%s: DETACHED\n", __func__);
		max77693_muic_handle_detach(info, irq);
	}

	return;
}
static irqreturn_t max77693_muic_irq(int irq, void *data)
{
	struct max77693_muic_info *info = data;
	dev_info(info->dev, "%s: irq:%d\n", __func__, irq);

	mutex_lock(&info->mutex);
	max77693_muic_detect_dev(info, irq);
	mutex_unlock(&info->mutex);

	return IRQ_HANDLED;
}

#define REQUEST_IRQ(_irq, _name)					\
do {									\
	ret = request_threaded_irq(_irq, NULL, max77693_muic_irq,	\
				    0, _name, info);			\
	if (ret < 0)							\
		dev_err(info->dev, "Failed to request IRQ #%d: %d\n",	\
			_irq, ret);					\
} while (0)

static int max77693_muic_irq_init(struct max77693_muic_info *info)
{
	int ret;
	u8 val;

	dev_info(info->dev, "%s: system_rev=%x\n", __func__, system_rev);

	/* INTMASK1  3:ADC1K 2:ADCErr 1:ADCLow 0:ADC */
	/* INTMASK2  0:Chgtype */
	max77693_write_reg(info->muic, MAX77693_MUIC_REG_INTMASK1, 0x09);
	max77693_write_reg(info->muic, MAX77693_MUIC_REG_INTMASK2, 0x11);
	max77693_write_reg(info->muic, MAX77693_MUIC_REG_INTMASK3, 0x00);

	REQUEST_IRQ(info->irq_adc, "muic-adc");
	REQUEST_IRQ(info->irq_chgtype, "muic-chgtype");
	REQUEST_IRQ(info->irq_vbvolt, "muic-vbvolt");
	REQUEST_IRQ(info->irq_adc1k, "muic-adc1k");

	dev_info(info->dev, "adc:%d chgtype:%d adc1k:%d vbvolt:%d",
		info->irq_adc, info->irq_chgtype,
		info->irq_adc1k, info->irq_vbvolt);

	max77693_read_reg(info->muic, MAX77693_MUIC_REG_INTMASK1, &val);
	dev_info(info->dev, "%s: reg=%x, val=%x\n", __func__,
		 MAX77693_MUIC_REG_INTMASK1, val);

	max77693_read_reg(info->muic, MAX77693_MUIC_REG_INTMASK2, &val);
	dev_info(info->dev, "%s: reg=%x, val=%x\n", __func__,
		 MAX77693_MUIC_REG_INTMASK2, val);

	max77693_read_reg(info->muic, MAX77693_MUIC_REG_INTMASK3, &val);
	dev_info(info->dev, "%s: reg=%x, val=%x\n", __func__,
		 MAX77693_MUIC_REG_INTMASK3, val);

	max77693_read_reg(info->muic, MAX77693_MUIC_REG_INT1, &val);
	dev_info(info->dev, "%s: reg=%x, val=%x\n", __func__,
		 MAX77693_MUIC_REG_INT1, val);
	max77693_read_reg(info->muic, MAX77693_MUIC_REG_INT2, &val);
	dev_info(info->dev, "%s: reg=%x, val=%x\n", __func__,
		 MAX77693_MUIC_REG_INT2, val);
	max77693_read_reg(info->muic, MAX77693_MUIC_REG_INT3, &val);
	dev_info(info->dev, "%s: reg=%x, val=%x\n", __func__,
		 MAX77693_MUIC_REG_INT3, val);

	return 0;
}

static void max77693_muic_init_detect(struct work_struct *work)
{
	struct max77693_muic_info *info =
	    container_of(work, struct max77693_muic_info, init_work.work);

	dev_info(info->dev, "func:%s\n", __func__);

	mutex_lock(&info->mutex);
	max77693_muic_detect_dev(info, -1);
	mutex_unlock(&info->mutex);
}

static void max77693_muic_init_usb_detect(struct work_struct *work)
{
	struct max77693_muic_info *info =
	    container_of(work, struct max77693_muic_info, usb_work.work);

	dev_info(info->dev, "usb path=%d\n", info->muic_data->usb_sel);

	mutex_lock(&info->mutex);
	if (info->cable_type == CABLE_TYPE_USB_MUIC ||
			info->cable_type == CABLE_TYPE_JIG_USB ||
			info->cable_type == CABLE_TYPE_OTG_MUIC ||
			info->cable_type == CABLE_TYPE_SMARTDOCK_MUIC)
		_detected(info, info->cable_type, 1);
	mutex_unlock(&info->mutex);
}

static void max77693_muic_init_mhl_detect(struct work_struct *work)
{
	struct max77693_muic_info *info =
	    container_of(work, struct max77693_muic_info, mhl_work.work);

	dev_info(info->dev, "initial mhl detect: cbl=%d\n", info->cable_type);
	mutex_lock(&info->mutex);

	if (info->cable_type == CABLE_TYPE_MHL_MUIC)
		_detected(info, info->cable_type, 1);
	mutex_unlock(&info->mutex);
}

int max77693_muic_charger_detect(void)
{
	u8 status[2];
	u8 adc, chgtyp;
	int ret;

	ret = max77693_bulk_read(gInfo->muic,
			MAX77693_MUIC_REG_STATUS1, 2, status);
	if (ret) {
		dev_err(gInfo->dev, "fail to read muic status1(%d)\n", ret);
		return 0;
	}

	adc = status[0] & STATUS1_ADC_MASK;
	chgtyp = status[1] & STATUS2_CHGTYP_MASK;

	if ((adc == ADC_OPEN && chgtyp == CHGTYP_USB) ||
			(adc == ADC_JIG_USB_OFF))
		ret = CABLE_TYPE_USB_MUIC;
	else if (adc == ADC_OPEN && (chgtyp == CHGTYP_1A ||
				chgtyp == CHGTYP_DOWNSTREAM_PORT ||
				chgtyp == CHGTYP_DEDICATED_CHGR ||
				chgtyp == CHGTYP_500MA))
		ret = CABLE_TYPE_TA_MUIC;

	return ret;
}
EXPORT_SYMBOL(max77693_muic_charger_detect);

static int __devinit max77693_muic_probe(struct platform_device *pdev)
{
	struct max77693_dev *max77693 = dev_get_drvdata(pdev->dev.parent);
	struct max77693_platform_data *pdata = dev_get_platdata(max77693->dev);
	struct max77693_muic_info *info;
	struct input_dev *input;
	int ret;

	info = kzalloc(sizeof(struct max77693_muic_info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "%s: failed to allocate info\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}
	input = input_allocate_device();
	if (!input) {
		dev_err(&pdev->dev, "%s: failed to allocate input\n", __func__);
		ret = -ENOMEM;
		goto err_kfree;
	}
	info->dev = &pdev->dev;
	info->max77693 = max77693;
	info->muic = max77693->muic;
	info->input = input;
	info->irq_adc = max77693->irq_base + MAX77693_MUIC_IRQ_INT1_ADC;
	info->irq_chgtype = max77693->irq_base + MAX77693_MUIC_IRQ_INT2_CHGTYP;
	info->irq_vbvolt = max77693->irq_base + MAX77693_MUIC_IRQ_INT2_VBVOLT;
	info->irq_adc1k = max77693->irq_base + MAX77693_MUIC_IRQ_INT1_ADC1K;
	info->muic_data = pdata->muic;
	info->cable_type = CABLE_TYPE_NONE_MUIC;
	gInfo = info;

	platform_set_drvdata(pdev, info);
	dev_info(info->dev, "adc:%d chgtype:%d, adc1k%d\n",
		 info->irq_adc, info->irq_chgtype, info->irq_adc1k);

	input->name = pdev->name;
	input->phys = "deskdock-key/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0001;

	/* Enable auto repeat feature of Linux input subsystem */
	__set_bit(EV_REP, input->evbit);

	input_set_capability(input, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(input, EV_KEY, KEY_VOLUMEDOWN);
	input_set_capability(input, EV_KEY, KEY_PLAYPAUSE);
	input_set_capability(input, EV_KEY, KEY_PREVIOUSSONG);
	input_set_capability(input, EV_KEY, KEY_NEXTSONG);

	ret = input_register_device(input);
	if (ret) {
		dev_err(info->dev, "%s: Unable to register input device, "
			"error: %d\n", __func__, ret);
		goto err_input;
	}

	if (info->muic_data->init_cb)
		info->muic_data->init_cb();

	mutex_init(&info->mutex);

	/* Set ADC debounce time: 25ms */
	max77693_muic_set_adc_debound(info, 2);

	ret = max77693_muic_irq_init(info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to initialize MUIC irq:%d\n", ret);
		goto fail;
	}

	/* initial cable detection */
	INIT_DELAYED_WORK(&info->init_work, max77693_muic_init_detect);
	schedule_delayed_work(&info->init_work, msecs_to_jiffies(3000));

	INIT_DELAYED_WORK(&info->usb_work, max77693_muic_init_usb_detect);
	schedule_delayed_work(&info->usb_work, msecs_to_jiffies(17000));

	INIT_DELAYED_WORK(&info->mhl_work, max77693_muic_init_mhl_detect);
	schedule_delayed_work(&info->mhl_work, msecs_to_jiffies(25000));

	return 0;

 fail:
	if (info->irq_adc)
		free_irq(info->irq_adc, NULL);
	if (info->irq_chgtype)
		free_irq(info->irq_chgtype, NULL);
	if (info->irq_vbvolt)
		free_irq(info->irq_vbvolt, NULL);
	if (info->irq_adc1k)
		free_irq(info->irq_adc1k, NULL);
	mutex_destroy(&info->mutex);
 err_input:
	platform_set_drvdata(pdev, NULL);
	input_free_device(input);
 err_kfree:
	kfree(info);
 err_return:
	return ret;
}

static int __devexit max77693_muic_remove(struct platform_device *pdev)
{
	struct max77693_muic_info *info = platform_get_drvdata(pdev);

	if (info) {
		dev_info(info->dev, "func:%s\n", __func__);
		input_unregister_device(info->input);
		cancel_delayed_work(&info->init_work);
		cancel_delayed_work(&info->usb_work);
		cancel_delayed_work(&info->mhl_work);
		free_irq(info->irq_adc, info);
		free_irq(info->irq_chgtype, info);
		free_irq(info->irq_vbvolt, info);
		free_irq(info->irq_adc1k, info);
		mutex_destroy(&info->mutex);
		kfree(info);
	}
	return 0;
}

void max77693_muic_shutdown(struct device *dev)
{
	struct max77693_muic_info *info = dev_get_drvdata(dev);
	int ret;
	u8 val;
	dev_info(info->dev, "func:%s\n", __func__);
	if (!info->muic) {
		dev_err(info->dev, "%s: no muic i2c client\n", __func__);
		return;
	}

	dev_info(info->dev, "%s: JIGSet: auto detection\n", __func__);
	val = (0 << CTRL3_JIGSET_SHIFT) | (0 << CTRL3_BOOTSET_SHIFT);

	ret = max77693_update_reg(info->muic, MAX77693_MUIC_REG_CTRL3, val,
			CTRL3_JIGSET_MASK | CTRL3_BOOTSET_MASK);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update reg\n", __func__);
		return;
	}
}

static struct platform_driver max77693_muic_driver = {
	.driver		= {
		.name	= "max77693-muic",
		.owner	= THIS_MODULE,
		.shutdown = max77693_muic_shutdown,
	},
	.probe		= max77693_muic_probe,
	.remove		= __devexit_p(max77693_muic_remove),
};

static int __init max77693_muic_init(void)
{
	return platform_driver_register(&max77693_muic_driver);
}
module_init(max77693_muic_init);

static void __exit max77693_muic_exit(void)
{
	pr_debug("func:%s\n", __func__);
	platform_driver_unregister(&max77693_muic_driver);
}
module_exit(max77693_muic_exit);

MODULE_DESCRIPTION("Maxim MAX77693 MUIC driver");
MODULE_AUTHOR("<sukdong.kim@samsung.com>");
MODULE_LICENSE("GPL");

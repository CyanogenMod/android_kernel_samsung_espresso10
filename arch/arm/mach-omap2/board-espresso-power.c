/* Power support for Samsung Gerry Board.
 *
 * Copyright (C) 2011 SAMSUNG, Inc.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/twl.h>
#include <linux/power/smb_charger.h>
#include <linux/power/max17042_battery.h>
#include <linux/bat_manager.h>
#include <linux/battery.h>
#include <linux/irq.h>

#include "board-espresso.h"
#include "mux.h"
#include "omap_muxtbl.h"

#define TA_CHG_ING_N	0
#define TA_ENABLE	1
#define FUEL_ALERT	2

#define TEMP_ADC_CHANNEL	1
#define ADC_NUM_SAMPLES		5
#define ADC_LIMIT_ERR_COUNT	5

#define CHARGER_STATUS_FULL             0x1
#define CHARGER_STATUS_CHARGERERR       0x2
#define CHARGER_STATUS_USB_FAIL         0x3
#define CHARGER_VBATT_UVLO              0x4

#define CABLE_DETECT_VALUE	1150
#define HIGH_BLOCK_TEMP         500
#define HIGH_RECOVER_TEMP       420
#define LOW_BLOCK_TEMP          (-50)
#define LOW_RECOVER_TEMP        0

#define BB_HIGH_BLOCK_TEMP         480
#define BB_HIGH_RECOVER_TEMP       440
#define BB_LOW_BLOCK_TEMP          (-40)
#define BB_LOW_RECOVER_TEMP        0

u32 bootmode;
struct max17042_fuelgauge_callbacks *fuelgauge_callback;
struct smb_charger_callbacks *espresso_charger_callbacks;
struct battery_manager_callbacks *batman_callback;

static struct gpio charger_gpios[] = {
	{ .flags = GPIOF_IN, .label = "TA_nCHG" },		/* TA_nCHG */
	{ .flags = GPIOF_OUT_INIT_LOW, .label = "TA_EN" },	/* TA_EN */
	{ .flags = GPIOF_IN, .label = "FUEL_ALERT" },
};

static irqreturn_t charger_state_isr(int irq, void *_data)
{
	int res = 0;
	int val;

	val = gpio_get_value(charger_gpios[TA_CHG_ING_N].gpio);

	irq_set_irq_type(irq, val ?
		IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (val) {
		if (espresso_charger_callbacks && espresso_charger_callbacks->get_status_reg)
			res = espresso_charger_callbacks->
				get_status_reg(espresso_charger_callbacks);

		if (res == CHARGER_STATUS_FULL &&
			batman_callback &&
				batman_callback->set_full_charge)
			batman_callback->set_full_charge(batman_callback);
	}

	return IRQ_HANDLED;
}

static irqreturn_t fuel_alert_isr(int irq, void *_data)
{
	int val;

	val = gpio_get_value(charger_gpios[FUEL_ALERT].gpio);
	pr_info("%s: fuel alert interrupt occured : %d\n", __func__, val);

	if (batman_callback && batman_callback->fuel_alert_lowbat)
		batman_callback->fuel_alert_lowbat(batman_callback);

	return IRQ_HANDLED;
}

static void charger_gpio_init(void)
{
	int i;
	int irq, fuel_irq;
	int ret;

	for (i = 0; i < ARRAY_SIZE(charger_gpios); i++) {
		charger_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(charger_gpios[i].label);
	}

	gpio_request_array(charger_gpios, ARRAY_SIZE(charger_gpios));

	irq = gpio_to_irq(charger_gpios[TA_CHG_ING_N].gpio);
	ret = request_threaded_irq(irq, NULL, charger_state_isr,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT | \
			IRQF_NO_SUSPEND,
			"Charge_Status", NULL);
	if (unlikely(ret < 0))
		pr_err("request irq %d failed for gpio %d\n",
			irq, charger_gpios[TA_CHG_ING_N].gpio);

	fuel_irq = gpio_to_irq(charger_gpios[FUEL_ALERT].gpio);
	ret = request_threaded_irq(fuel_irq, NULL, fuel_alert_isr,
			IRQF_TRIGGER_FALLING, "Fuel Alert irq",
			NULL);
	if (unlikely(ret < 0))
		pr_err("request fuel alert irq %d failed for gpio %d\n",
			fuel_irq, charger_gpios[FUEL_ALERT].gpio);
}

static void charger_enble_set(int state)
{
	gpio_set_value(charger_gpios[TA_ENABLE].gpio, !state);
	pr_debug("%s: Set charge status : %d, current status: %d\n",
		__func__, state,
		gpio_get_value(charger_gpios[TA_ENABLE].gpio));
}

static struct i2c_gpio_platform_data espresso_gpio_i2c5_pdata = {
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device espresso_gpio_i2c5_device = {
	.name = "i2c-gpio",
	.id = 5,
	.dev = {
		.platform_data = &espresso_gpio_i2c5_pdata,
	}
};

static struct i2c_gpio_platform_data espresso_gpio_i2c7_pdata = {
	.udelay = 3,
	.timeout = 0,
};

static struct platform_device espresso_gpio_i2c7_device = {
	.name = "i2c-gpio",
	.id = 7,
	.dev = {
		.platform_data = &espresso_gpio_i2c7_pdata,
	},
};

static void __init espresso_gpio_i2c_init(void)
{
	/* gpio-i2c 5 */
	espresso_gpio_i2c5_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("CHG_SDA_1.8V");
	espresso_gpio_i2c5_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("CHG_SCL_1.8V");

	/* gpio-i2c 7 */
	espresso_gpio_i2c7_pdata.sda_pin =
		omap_muxtbl_get_gpio_by_name("FUEL_SDA_1.8V");
	espresso_gpio_i2c7_pdata.scl_pin =
		omap_muxtbl_get_gpio_by_name("FUEL_SCL_1.8V");
}

static void smb_charger_register_callbacks(
		struct smb_charger_callbacks *ptr)
{
	espresso_charger_callbacks = ptr;
}

static void set_chg_state(int cable_type)
{
	if (espresso_charger_callbacks && espresso_charger_callbacks->set_charging_state)
		espresso_charger_callbacks->set_charging_state(espresso_charger_callbacks,
				cable_type);

	omap4_espresso_usb_detected(cable_type);
	omap4_espresso_tsp_ta_detect(cable_type);
}

static struct smb_charger_data smb_pdata = {
	.set_charge = charger_enble_set,
	.register_callbacks = smb_charger_register_callbacks,
};

static const __initdata struct i2c_board_info smb136_i2c[] = {
	{
		I2C_BOARD_INFO("smb136-charger", 0x4D), /* 9A >> 1 */
		.platform_data = &smb_pdata,
	},
};

static const __initdata struct i2c_board_info smb347_i2c[] = {
	{
		I2C_BOARD_INFO("smb347-charger", 0x0C >> 1),
		.platform_data = &smb_pdata,
	},
};


static void max17042_fuelgauge_register_callbacks(
		struct max17042_fuelgauge_callbacks *ptr)
{
	fuelgauge_callback = ptr;
}

static struct max17042_platform_data max17042_pdata = {
	.register_callbacks = &max17042_fuelgauge_register_callbacks,
	.enable_current_sense = true,
	.sdi_capacity = 0x1F40,
	.sdi_vfcapacity = 0x29AB,
	.sdi_low_bat_comp_start_vol = 3550,
	.current_range = {
		.range1 = 0,
		.range2 = -100,
		.range3 = -750,
		.range4 = -1250,
		.range5 = 0, /* ignored */
		.range_max = -1250,
		.range_max_num = 4,
	},
	.sdi_compensation = {
		.range1_1_slope = 0,
		.range1_1_offset = 3456,
		.range1_3_slope = 0,
		.range1_3_offset = 3536,
		.range2_1_slope = 96,
		.range2_1_offset = 3461,
		.range2_3_slope = 134,
		.range2_3_offset = 3544,
		.range3_1_slope = 97,
		.range3_1_offset = 3451,
		.range3_3_slope = 27,
		.range3_3_offset = 3454,
		.range4_1_slope = 0,
		.range4_1_offset = 3320,
		.range4_3_slope = 0,
		.range4_3_offset = 3410,
		.range5_1_slope = 0,
		.range5_1_offset = 3318,
		.range5_3_slope = 0,
		.range5_3_offset = 3383,
	},
};

static const __initdata struct i2c_board_info max17042_i2c[] = {
	{
		I2C_BOARD_INFO("max17042", 0x36),
		.platform_data = &max17042_pdata,
	},
};

static int read_fuel_value(enum fuel_property fg_prop)
{
	if (fuelgauge_callback && fuelgauge_callback->get_value)
		return fuelgauge_callback->get_value(fuelgauge_callback,
						fg_prop);
	return 0;
}

static int check_charger_type(void)
{
	int cable_type;
	short adc;

	adc = omap4_espresso_get_adc(ADC_CHECK_1);
	cable_type = adc > CABLE_DETECT_VALUE ?
			CABLE_TYPE_AC :
			CABLE_TYPE_USB;

	pr_info("%s : Charger type is [%s], adc = %d\n",
		__func__,
		cable_type == CABLE_TYPE_AC ? "TA" : "USB",
		adc);

	return cable_type;
}

static void fuel_gauge_reset_soc(void)
{
	if (fuelgauge_callback && fuelgauge_callback->fg_reset_soc)
		fuelgauge_callback->fg_reset_soc(fuelgauge_callback);
}

static void fuel_gauge_adjust_capacity(void)
{
	if (fuelgauge_callback && fuelgauge_callback->set_adjust_capacity)
		fuelgauge_callback->set_adjust_capacity(fuelgauge_callback);
}

static void fuel_gauge_full_comp(u32 is_recharging, u32 pre_update)
{
	if (fuelgauge_callback &&
			fuelgauge_callback->full_charged_compensation)
		fuelgauge_callback->
			full_charged_compensation(fuelgauge_callback,
					is_recharging, pre_update);
}

static void fuel_gauge_vf_fullcap_range(void)
{
	if (fuelgauge_callback && fuelgauge_callback->check_vf_fullcap_range)
		fuelgauge_callback->check_vf_fullcap_range(fuelgauge_callback);
}

static int fuel_gauge_lowbat_compensation(struct bat_information bat_info)
{
	if (fuelgauge_callback &&
			fuelgauge_callback->check_low_batt_compensation) {
		return fuelgauge_callback->
			check_low_batt_compensation(fuelgauge_callback,
					bat_info);
	}
	return 0;
}

static int fuel_gauge_check_cap_corruption(void)
{
	if (fuelgauge_callback &&
			fuelgauge_callback->check_cap_corruption) {
		return fuelgauge_callback->
			check_cap_corruption(fuelgauge_callback);
	}
	return 0;
}

static void fuel_gauge_update_fullcap(void)
{
	if (fuelgauge_callback &&
			fuelgauge_callback->update_remcap_to_fullcap)
		fuelgauge_callback->
			update_remcap_to_fullcap(fuelgauge_callback);
}

static int fuelgauge_register_value(u8 addr)
{
	if (fuelgauge_callback &&
			fuelgauge_callback->get_register_value)
		return fuelgauge_callback->
			get_register_value(fuelgauge_callback, addr);

	return 0;
}

static void battery_manager_register_callbacks(
		struct battery_manager_callbacks *ptr)
{
	batman_callback = ptr;
}

static struct batman_platform_data battery_manager_pdata = {
	.get_fuel_value = read_fuel_value,
	.set_charger_state = set_chg_state,
	.set_charger_en = charger_enble_set,
	.get_charger_type = check_charger_type,
	.reset_fuel_soc = fuel_gauge_reset_soc,
	.full_charger_comp = fuel_gauge_full_comp,
	.update_fullcap_value = fuel_gauge_update_fullcap,
	.fg_adjust_capacity = fuel_gauge_adjust_capacity,
	.low_bat_compensation = fuel_gauge_lowbat_compensation,
	.check_vf_fullcap_range = fuel_gauge_vf_fullcap_range,
	.check_cap_corruption = fuel_gauge_check_cap_corruption,
	.register_callbacks = battery_manager_register_callbacks,
	.get_fg_register = fuelgauge_register_value,
	.high_block_temp = HIGH_BLOCK_TEMP,
	.high_recover_temp = HIGH_RECOVER_TEMP,
	.low_block_temp = LOW_BLOCK_TEMP,
	.low_recover_temp = LOW_RECOVER_TEMP,
	.recharge_voltage = 4150000,
	.limit_charging_time = 36000,   /* 10hour */
	.limit_recharging_time = 5400,  /* 90min */
};

static struct platform_device battery_manager_device = {
	.name   = "battery_manager",
	.id     = -1,
	.dev    = {
		.platform_data = &battery_manager_pdata,
	},
};

void check_jig_status(int status)
{
	if (status) {
		pr_info("%s: JIG On so reset fuel gauge capacity\n", __func__);
		if (fuelgauge_callback && fuelgauge_callback->reset_capacity)
			fuelgauge_callback->reset_capacity(fuelgauge_callback);
	}

	max17042_pdata.jig_on = status;
	battery_manager_pdata.jig_on = status;
}

static __init int setup_boot_mode(char *str)
{
	unsigned int _bootmode;

	if (!kstrtouint(str, 0, &_bootmode))
		bootmode = _bootmode;

	return 0;
}
__setup("bootmode=", setup_boot_mode);

void __init omap4_espresso_charger_init(void)
{
	int ret;

	charger_gpio_init();
	espresso_gpio_i2c_init();
	if (board_is_espresso10() && board_is_bestbuy_variant() && bootmode == 5) {
		battery_manager_pdata.high_block_temp = BB_HIGH_BLOCK_TEMP;
		battery_manager_pdata.high_recover_temp = BB_HIGH_RECOVER_TEMP;
		battery_manager_pdata.low_block_temp = BB_LOW_BLOCK_TEMP;
		battery_manager_pdata.low_recover_temp = BB_LOW_RECOVER_TEMP;
	}

	battery_manager_pdata.bootmode = bootmode;
	if (!board_is_espresso10())
		smb_pdata.hw_revision = system_rev;

	battery_manager_pdata.ta_gpio =
			omap_muxtbl_get_gpio_by_name("TA_nCONNECTED");

	if (!gpio_is_valid(battery_manager_pdata.ta_gpio))
		gpio_request(battery_manager_pdata.ta_gpio, "TA_nCONNECTED");

	ret = platform_device_register(&espresso_gpio_i2c5_device);
	if (ret < 0)
		pr_err("%s: gpio_i2c5 device register fail\n", __func__);

	ret = platform_device_register(&espresso_gpio_i2c7_device);
	if (ret < 0)
		pr_err("%s: gpio_i2c7 device register fail\n", __func__);

	if (board_is_espresso10()) {
		i2c_register_board_info(5, smb347_i2c, ARRAY_SIZE(smb347_i2c));
		max17042_pdata.sdi_capacity = 0x3730;
		max17042_pdata.sdi_vfcapacity = 0x4996;
		max17042_pdata.byd_capacity = 0x36B0;
		max17042_pdata.byd_vfcapacity = 0x48EA;
		max17042_pdata.sdi_low_bat_comp_start_vol = 3600;
		max17042_pdata.byd_low_bat_comp_start_vol = 3650;
		max17042_pdata.current_range.range2 = -200;
		max17042_pdata.current_range.range3 = -600;
		max17042_pdata.current_range.range4 = -1500;
		max17042_pdata.current_range.range5 = -2500;
		max17042_pdata.current_range.range_max = -2500;
		max17042_pdata.current_range.range_max_num = 5;
		max17042_pdata.sdi_compensation.range1_1_offset = 3438;
		max17042_pdata.sdi_compensation.range1_3_offset = 3591;
		max17042_pdata.sdi_compensation.range2_1_slope = 45;
		max17042_pdata.sdi_compensation.range2_1_offset = 3447;
		max17042_pdata.sdi_compensation.range2_3_slope = 78;
		max17042_pdata.sdi_compensation.range2_3_offset = 3606;
		max17042_pdata.sdi_compensation.range3_1_slope = 54;
		max17042_pdata.sdi_compensation.range3_1_offset = 3453;
		max17042_pdata.sdi_compensation.range3_3_slope = 92;
		max17042_pdata.sdi_compensation.range3_3_offset = 3615;
		max17042_pdata.sdi_compensation.range4_1_slope = 53;
		max17042_pdata.sdi_compensation.range4_1_offset = 3451;
		max17042_pdata.sdi_compensation.range4_3_slope = 94;
		max17042_pdata.sdi_compensation.range4_3_offset = 3618;
	} else {
		i2c_register_board_info(5, smb136_i2c, ARRAY_SIZE(smb136_i2c));
	}

	i2c_register_board_info(7, max17042_i2c, ARRAY_SIZE(max17042_i2c));

	ret = platform_device_register(&battery_manager_device);
	if (ret < 0)
		pr_err("%s: battery monitor device register fail\n", __func__);
}

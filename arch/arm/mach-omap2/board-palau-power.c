/* arch/arm/mach-omap2/board-hershey-power.c
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
#include <linux/i2c-gpio.h>
#include <linux/regulator/machine.h>

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>

#include "mux.h"
#include "omap_muxtbl.h"
#include "board-palau.h"

#define TA_NCONNECTED 0
#define FUEL_ALERT 1

#define SEC_FUELGAUGE_I2C_ID 4
#define SEC_MAX77693MFD_I2C_ID 5

#define SEC_BATTERY_PMIC_NAME ""

static struct gpio battery_gpios[] = {
	{ .flags = GPIOF_IN, .label = "TA_nCONNECTED" },	/* TA_nCHG */
	{ .flags = GPIOF_IN, .label = "FUEL_ALERT" },
};

static void sec_bat_initial_check(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;
	int ret = 0;

	value.intval = gpio_get_value(battery_gpios[TA_NCONNECTED].gpio);
	pr_debug("%s: %d\n", __func__, value.intval);

	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
	}
}

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{
	return true;
}

static bool sec_chg_gpio_init(void)
{
	return true;
}

static bool sec_bat_is_lpm(void)
{
	return sec_bootmode == 5 ? true : false;
}

static bool sec_bat_check_jig_status(void)
{
	return false;
}

static int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

static int sec_bat_check_cable_callback(void)
{
	current_cable_type = !gpio_get_value(battery_gpios[0].gpio) ?
				POWER_SUPPLY_TYPE_MAINS :
				POWER_SUPPLY_TYPE_BATTERY;

	return current_cable_type;
}

static bool sec_bat_check_cable_result_callback(int cable_type)
{
	bool ret = true;
	current_cable_type = cable_type;

	switch (cable_type) {
	case POWER_SUPPLY_TYPE_USB:
		pr_info("%s set vbus applied\n",
				__func__);
		break;
	case POWER_SUPPLY_TYPE_BATTERY:
		pr_info("%s set vbus cut\n",
				__func__);
		break;
	case POWER_SUPPLY_TYPE_MAINS:
		break;
	default:
		pr_err("%s cable type (%d)\n",
				__func__, cable_type);
		ret = false;
		break;
	}

	return ret;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void) { return true; }
static bool sec_bat_check_result_callback(void) { return true; }

/* callback for OVP/UVLO check
 * return : int
 * battery health
 */
static int sec_bat_ovp_uvlo_callback(void)
{
	int health;
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static bool sec_bat_ovp_uvlo_result_callback(int health) { return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) { return true; }

static bool sec_fg_fuelalert_process(bool is_fuel_alerted) { return true; }

static sec_bat_adc_region_t cable_adc_value_table[] = {
	{ 0,    500 },  /* POWER_SUPPLY_TYPE_BATTERY */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_UPS */
	{ 1000, 1500 }, /* POWER_SUPPLY_TYPE_MAINS */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_USB */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_OTG */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_DOCK */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_MISC */
};

static sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UPS */
	{700,   700,    256,    0},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   500,    256,    0},     /* POWER_SUPPLY_TYPE_USB */
	{500,   500,    256,    0},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{500,   500,    256,    0},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   500,    256,    0},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_OTG */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_DOCK */
	{500,   500,    256,    0},     /* POWER_SUPPLY_TYPE_MISC */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_WIRELESS */
};

/* unit: seconds */
static int polling_time_table[] = {
	10,     /* BASIC */
	30,     /* CHARGING */
	30,     /* DISCHARGING */
	30,     /* NOT_CHARGING */
	300,    /* SLEEP */
};

static bool sec_bat_adc_none_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_none_exit(void) { return true; }
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }
static int sec_bat_adc_ap_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ic_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ic_exit(void) { return true; }
static int sec_bat_adc_ic_read(unsigned int channel) { return 0; }

static sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.check_jig_status = sec_bat_check_jig_status,
	.check_cable_callback =
		sec_bat_check_cable_callback,
	.check_cable_result_callback =
		sec_bat_check_cable_result_callback,
	.check_battery_callback =
		sec_bat_check_callback,
	.check_battery_result_callback =
		sec_bat_check_result_callback,
	.ovp_uvlo_callback = sec_bat_ovp_uvlo_callback,
	.ovp_uvlo_result_callback =
		sec_bat_ovp_uvlo_result_callback,
	.fuelalert_process = sec_fg_fuelalert_process,
	.get_temperature_callback =
		sec_bat_get_temperature_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
	},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
		.read = sec_bat_adc_ap_read
	},
	.adc_api[SEC_BATTERY_ADC_TYPE_IC] = {
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
	},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 7,
	.adc_type = {
		SEC_BATTERY_ADC_TYPE_IC,        /* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,      /* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,      /* TEMP */
		SEC_BATTERY_ADC_TYPE_NONE,      /* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_NONE,      /* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.bat_polarity_ta_nconnected = 1,        /* active HIGH */
	.bat_irq_attr = 0,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE |
		SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type = SEC_BATTERY_CABLE_SOURCE_CALLBACK,

	.event_check = false,
	.event_waiting_time = 60,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_NONE,
	.check_count = 3,

	/* Battery check by ADC */
	.check_adc_max = 0,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGINT,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_FG,

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 3,
	.temp_high_threshold_event = 25000, /* set temp value */
	.temp_high_recovery_event = 450,
	.temp_low_threshold_event = 0,
	.temp_low_recovery_event = -50,
	.temp_high_threshold_normal = 250000,
	.temp_high_recovery_normal = 400,
	.temp_low_threshold_normal = 0,
	.temp_low_recovery_normal = -30,
	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 420,
	.temp_low_threshold_lpm = 2,
	.temp_low_recovery_lpm = -30,

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGINT,
	.full_check_count = 3,
	.full_check_adc_1st = 26500,    /* CHECK ME */
	.full_check_adc_2nd = 25800,    /* CHECK ME */
	.chg_polarity_full_check = 1,
	.full_condition_type =
		SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_OCV,
	.full_condition_soc = 99,
	.full_condition_ocv = 4170,

	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_SOC |
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4150,
	.recharge_condition_vcell = 4150,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 10 * 60,

	/* Fuel Gauge */
	.fg_irq_attr = IRQF_TRIGGER_LOW | IRQF_ONESHOT,
	.fuel_alert_soc = 1,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW,
	.capacity_max = 1000,
	.capacity_min = 0,

	/* Charger */
	.chg_polarity_en = 0,   /* active LOW charge enable */
	.chg_polarity_status = 0,
	.chg_irq_attr = 0,
	.chg_float_voltage = 4200,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

static struct i2c_board_info sec_brdinfo_fuelgauge[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				SEC_FUELGAUGE_I2C_SLAVEADDR),
		.platform_data  = &sec_battery_pdata,
	},
};

static struct regulator_consumer_supply safeout1_supply[] = {
	REGULATOR_SUPPLY("safeout1", NULL),
};

static struct regulator_consumer_supply safeout2_supply[] = {
	REGULATOR_SUPPLY("safeout2", NULL),
};

static struct regulator_consumer_supply charger_supply[] = {
	REGULATOR_SUPPLY("vinchg1", "charger-manager.0"),
	REGULATOR_SUPPLY("vinchg1", NULL),
};

static struct regulator_init_data safeout1_init_data = {
	.constraints	= {
		.name		= "safeout1 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout1_supply),
	.consumer_supplies	= safeout1_supply,
};

static struct regulator_init_data safeout2_init_data = {
	.constraints	= {
		.name		= "safeout2 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 0,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout2_supply),
	.consumer_supplies	= safeout2_supply,
};

static struct regulator_init_data charger_init_data = {
	.constraints	= {
		.name		= "CHARGER",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
		REGULATOR_CHANGE_CURRENT,
		.boot_on	= 1,
		.min_uA		= 60000,
		.max_uA		= 2580000,
	},
	.num_consumer_supplies  = ARRAY_SIZE(charger_supply),
	.consumer_supplies      = charger_supply,
};

static struct max77693_regulator_data max77693_regulators[] = {
	{MAX77693_ESAFEOUT1, &safeout1_init_data,},
	{MAX77693_ESAFEOUT2, &safeout2_init_data,},
	{MAX77693_CHARGER, &charger_init_data,},
};

static struct max77693_haptic_platform_data haptic_pdata;

static struct max77693_platform_data palau_max77693_pdata = {
	.irq_base		= OMAP_MAX77693_IRQ_BASE,
	.wakeup			= 1,
	.regulators		= max77693_regulators,
	.num_regulators		= MAX77693_REG_MAX,
	.charger_data		= &sec_battery_pdata,
	.muic			= &max77693_muic,
	.haptic_data		= &haptic_pdata,
};

static struct i2c_board_info palau_pmic_i2c_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("max77693", (0xCC >> 1)),
		.platform_data = &palau_max77693_pdata,
	},
};

static struct platform_device *sec_battery_devices[] __initdata = {
	&sec_device_battery,
};

void max77693_haptic_enable(bool on)
{
	int ret;
	u8 value = MOTOR_LRA | EXT_PWM | DIVIDER_128;
	u8 lscnfg_val = 0x00;

	if (on) {
		value |= MOTOR_EN;
		lscnfg_val = 0x80;
	}

	ret = max77693_update_reg(palau_max77693_pdata.haptic_data->pmic_i2c
				, MAX77693_PMIC_REG_LSCNFG, lscnfg_val, 0x80);
	if (ret)
		pr_err("max77693: pmic i2c error %d\n", ret);

	ret = max77693_write_reg(palau_max77693_pdata.haptic_data->haptic_i2c
					, MAX77693_HAPTIC_REG_CONFIG2, value);
	if (ret)
		pr_err("max77693: haptic i2c error %d\n", ret);
}

static void palau_power_gpio_init(void)
{
	int i;

	palau_max77693_pdata.irq_gpio =
			omap_muxtbl_get_gpio_by_name("JACK_nINT");
	palau_max77693_pdata.irq_gpio_label = "JACK_nINT";
	palau_pmic_i2c_boardinfo[0].platform_data = &palau_max77693_pdata;

	for (i = 0; i < ARRAY_SIZE(battery_gpios); i++)
		battery_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(battery_gpios[i].label);

	gpio_request_array(battery_gpios, ARRAY_SIZE(battery_gpios));

	sec_battery_pdata.bat_irq =
		OMAP_MAX77693_IRQ_BASE + MAX77693_CHG_IRQ_CHGIN_I;

	sec_battery_pdata.fg_irq =
		gpio_to_irq(battery_gpios[FUEL_ALERT].gpio);

	sec_battery_pdata.chg_irq =
		OMAP_MAX77693_IRQ_BASE + MAX77693_CHG_IRQ_CHG_I;
}

void __init omap4_palau_charger_init(void)
{
	int fuelgauge_i2c_id;

	pr_info("%s: palau charger init\n", __func__);
	palau_power_gpio_init();

	fuelgauge_i2c_id = system_rev < 1 ? 4 : 6;

	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	/* max77693 */
	i2c_register_board_info(SEC_MAX77693MFD_I2C_ID,
			palau_pmic_i2c_boardinfo,
			ARRAY_SIZE(palau_pmic_i2c_boardinfo));

	i2c_register_board_info(fuelgauge_i2c_id,
		sec_brdinfo_fuelgauge, ARRAY_SIZE(sec_brdinfo_fuelgauge));
}

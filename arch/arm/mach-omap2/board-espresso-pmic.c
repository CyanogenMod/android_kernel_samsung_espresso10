/* arch/arm/mach-omap2/board-espresso-pmic.c
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
#include <linux/i2c/twl.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#ifdef CONFIG_SND_OMAP_SOC_ESPRESSO
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>
#endif

#include <plat/omap-pm.h>
#include "pm.h"

#include "board-espresso.h"
#include "common-board-devices.h"

#define GPIO_EMMC_EN		53
#define GPIO_CODEC_LDO_EN	45
#define GPIO_SYS_DRM_MSEC	6
#define GPIO_TF_EN		34

#define GPIO_SUB_MICBIAS_EN	177
#define GPIO_CODEC_CLK_REQ	101
#define GPIO_MICBIAS_EN	48
#define GPIO_EAR_GND_SEL	171

#define TWL6030_BBSPOR_CFG			0xE6
#define TWL6030_PHOENIX_MSK_TRANSITION		0x20

#define TWL_REG_CONTROLLER_INT_MASK	0x00
#define TWL_CONTROLLER_MVBUS_DET	(1 << 1)
#define TWL_CONTROLLER_RSVD		(1 << 5)

#define TWL6030_PHEONIX_MSK_TRANS_SHIFT	0x05

#define TWL_BBSPOR_CFG_VRTC_PWEN	(1 << 4)
#define TWL_BBSPOR_CFG_VRTC_EN_OFF_STS	(1 << 5)
#define TWL_BBSPOR_CFG_VRTC_EN_SLP_STS	(1 << 6)

#define TWL6030_CFG_LDO_PD2	0xF5

static bool enable_sr = true;
module_param(enable_sr, bool, S_IRUSR | S_IRGRP | S_IROTH);

#ifdef CONFIG_SND_OMAP_SOC_ESPRESSO
static struct regulator_consumer_supply vbatt_supplies[] = {
	REGULATOR_SUPPLY("LDO1VDD", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD1", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "1-001a"),
	REGULATOR_SUPPLY("AVDD2", "1-001a"),
	REGULATOR_SUPPLY("CPVDD", "1-001a"),
	REGULATOR_SUPPLY("DBVDD1", "1-001a"),
	REGULATOR_SUPPLY("DBVDD2", "1-001a"),
	REGULATOR_SUPPLY("DBVDD3", "1-001a"),
};

static struct regulator_init_data vbatt_initdata = {
	.constraints = {
		.always_on = true,
	},
	.num_consumer_supplies = ARRAY_SIZE(vbatt_supplies),
	.consumer_supplies = vbatt_supplies,
};

static struct fixed_voltage_config vbatt_config = {
	.init_data = &vbatt_initdata,
	.microvolts = 1800000,
	.supply_name = "VBATT",
	.gpio = -EINVAL,
};

static struct platform_device vbatt_device = {
	.name	= "reg-fixed-voltage",
	.id	= -1,
	.dev = {
		.platform_data = &vbatt_config,
	},
};

static struct regulator_consumer_supply wm1811_ldo1_supplies[] = {
	REGULATOR_SUPPLY("AVDD1", "1-001a"),
};

static struct regulator_init_data wm1811_ldo1_initdata = {
	.constraints = {
		.name = "WM1811 LDO1",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo1_supplies),
	.consumer_supplies = wm1811_ldo1_supplies,
};

static struct regulator_consumer_supply wm1811_ldo2_supplies[] = {
	REGULATOR_SUPPLY("DCVDD", "1-001a"),
};

static struct regulator_init_data wm1811_ldo2_initdata = {
	.constraints = {
		.name = "WM1811 LDO2",
		.always_on = true,  /* Actually status changed by LDO1 */
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo2_supplies),
	.consumer_supplies = wm1811_ldo2_supplies,
};

static struct wm8994_pdata wm1811_pdata = {
	.gpio_defaults = {
		[0] = WM8994_GP_FN_IRQ,
		[7] = WM8994_GPN_DIR | WM8994_GP_FN_PIN_SPECIFIC,
		[8] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
		[9] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
		[10] = WM8994_CONFIGURE_GPIO | WM8994_GP_FN_PIN_SPECIFIC,
	},

	/* for using wm1811 jack detect
	 * This line should be remained for next board */
	/*.irq_base = TWL6040_CODEC_IRQ_BASE,*/

	.ldo = {
		{
			.init_data = &wm1811_ldo1_initdata,
			.enable = GPIO_CODEC_LDO_EN,
		},
		{
			.init_data = &wm1811_ldo2_initdata,
		}
	},

	/* Regulated mode at highest output voltage */
	.micbias = { 0x2f, 0x29 },

	.ldo_ena_always_driven = true,

	.ear_select_gpio = GPIO_EAR_GND_SEL,
	.main_mic_bias_gpio = GPIO_MICBIAS_EN,
	.mclk_gpio = GPIO_CODEC_CLK_REQ,
};
#endif

static struct regulator_init_data espresso_vaux1 = {
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
	},
};

static struct regulator_consumer_supply espresso_vaux2_supplies[] = {
	REGULATOR_SUPPLY("VAP_IO_2.8V", NULL),
	REGULATOR_SUPPLY("SENSOR_2.8V", "4-0018"),
	REGULATOR_SUPPLY("SENSOR_2.8V", "4-0044"),
};

static struct regulator_init_data espresso_vaux2 = {
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso_vaux2_supplies),
	.consumer_supplies	= espresso_vaux2_supplies,
};

static struct regulator_consumer_supply espresso_vmmc_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.0"),
};

static struct regulator_init_data espresso_vmmc = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso_vmmc_supply),
	.consumer_supplies	= espresso_vmmc_supply,
};

static struct regulator_init_data espresso_vusim = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.enabled        = true,
		},
	},
};

static struct regulator_init_data espresso_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
	},
};

static struct regulator_consumer_supply espresso_vcxio_supply[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};

static struct regulator_init_data espresso_vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso_vcxio_supply),
	.consumer_supplies	= espresso_vcxio_supply,
};

static struct regulator_consumer_supply espresso_vusb_supply[] = {
	REGULATOR_SUPPLY("vusb", "espresso_otg"),
};

static struct regulator_init_data espresso_vusb = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso_vusb_supply),
	.consumer_supplies	= espresso_vusb_supply,
};

static struct regulator_init_data espresso_clk32kg = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
};

static struct regulator_init_data espresso_clk32kaudio = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
};

static struct twl4030_madc_platform_data espresso_madc = {
	.irq_line	= -1,
	.features	= TWL6032_SUBCLASS,
};

static struct platform_device espresso_madc_device = {
	.name		= "twl6030_madc",
	.id		= -1,
	.dev = {
		.platform_data		= &espresso_madc,
	},
};

static void espresso_twl6030_init(void)
{
	int ret;
	u8 val;

	/*
	 * If disable GPADC_IN1, BAT_TEMP_OVRANGE interrupt is signaled.
	 * Disable all interrupt of charger block except VBUS_DET.
	 * We need only VBUS_DET interrupt of charger block fot usb otg.
	 */
	val = ~(TWL_CONTROLLER_RSVD | TWL_CONTROLLER_MVBUS_DET);
	ret = twl_i2c_write_u8(TWL_MODULE_MAIN_CHARGE, val,
					TWL_REG_CONTROLLER_INT_MASK);

	ret |= twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
					REG_INT_MSK_LINE_C);

	if (ret)
		pr_err("%s: disable charger interrupt fail!\n", __func__);

	/* use only preq1 of twl6032 */
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0,
			~(DEV_GRP_P1) << TWL6030_PHEONIX_MSK_TRANS_SHIFT,
			TWL6030_PHOENIX_MSK_TRANSITION);
	if (ret)
		pr_err("%s: PHOENIX_MSK_TRANSITION write fail!\n", __func__);


	if (board_is_espresso10()) {
		/*
		 * Enable charge backup battery and set charging voltage to 2.6V.
		 * Set VRTC low power mode in off/sleep and standard power mode in on.
		 */
		val = TWL_BBSPOR_CFG_VRTC_EN_SLP_STS | TWL_BBSPOR_CFG_VRTC_EN_OFF_STS |
				TWL_BBSPOR_CFG_VRTC_PWEN;
	} else {
		ret = twl_i2c_read_u8(TWL6030_MODULE_ID0,
			&val, TWL6030_BBSPOR_CFG);

		/* disable backup battery charge */
		val &= ~(1 << 3);

		/* configure in low power mode */
		val |= (1 << 6 | 1 << 5);
	}

	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0, val,
					TWL6030_BBSPOR_CFG);
	if (ret)
		pr_err("%s: TWL6030 BBSPOR_CFG write fail!\n", __func__);


	if (system_rev >= 8) {
		ret = twl_i2c_read_u8(TWL6030_MODULE_ID0,
				&val, TWL6030_CFG_LDO_PD2);

		/* TI recommand
		 * recommended to leave vpp_cust turn off(float).
		 * disable internal pull-down when vpp_cust is turned off
		 */
		val &= ~(1<<1); /*LDO7*/
		ret |= twl_i2c_write_u8(TWL6030_MODULE_ID0,
				val, TWL6030_CFG_LDO_PD2);
		if (ret)
			pr_err("%s:TWL6030 CFG_LDO_PD2 write fail!\n",
					__func__);
	}
}

static struct twl4030_resconfig espresso_rconfig[] = {
	{ .resource = RES_LDO2, .devgroup = 0, },
	{ .resource = RES_LDO7, .devgroup = 0, },
	{ .resource = RES_LDOLN, .devgroup = 0, },
	{ .resource = TWL4030_RESCONFIG_UNDEF, 0},
};

static struct twl4030_power_data espresso_power_data = {
	.twl4030_board_init	= espresso_twl6030_init,
	.resource_config = espresso_rconfig,
};

static struct regulator_init_data espresso_ldo2_nc = {
	.constraints = {
		.min_uV = 1000000,
		.max_uV = 3300000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
	},
};

static struct regulator_init_data espresso_ldo7_nc = {
	.constraints = {
		.min_uV = 1000000,
		.max_uV = 3300000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
	},
};

static struct regulator_init_data espresso_ldoln_nc = {
	.constraints = {
		.min_uV = 1000000,
		.max_uV = 3300000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
	},
};

struct twl4030_rtc_data espresso_rtc = {
	.auto_comp = 1,
	.comp_value = -3200,
};

static struct regulator_consumer_supply espresso_vdd_io_1V8_supplies[] = {
	REGULATOR_SUPPLY("VDD_IO_1.8V", NULL),
	REGULATOR_SUPPLY("SENSOR_1.8V", "4-0018"),
	REGULATOR_SUPPLY("SENSOR_1.8V", "4-0044"),
};

static struct regulator_init_data espresso_ldo5 = {
	.constraints = {
		.min_uV = 1800000,
		.max_uV = 1800000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso_vdd_io_1V8_supplies),
	.consumer_supplies	= espresso_vdd_io_1V8_supplies,
};

static struct twl4030_platform_data espresso_twl6032_pdata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* pmic power data*/
	.power		= &espresso_power_data,

	/* TWL6025 LDO regulators */
	.vana		= &espresso_vana,
	.ldo1		= &espresso_vaux1,
	.ldo2		= &espresso_ldo2_nc,
	.ldo3		= &espresso_vusim,
	.ldo4		= &espresso_vaux2,
	.ldo5		= &espresso_ldo5,
	.ldo6		= &espresso_vcxio,
	.ldo7		= &espresso_ldo7_nc,
	.ldoln		= &espresso_ldoln_nc,
	.ldousb		= &espresso_vusb,
	.clk32kg	= &espresso_clk32kg,
	.clk32kaudio	= &espresso_clk32kaudio,

	/* children */
	.madc		= &espresso_madc,
};

static struct platform_device *espresso_pmic_devices[] __initdata = {
	&espresso_madc_device,
};

static struct i2c_board_info espresso_twl6032_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("twl6032", 0x48),
		.flags		= I2C_CLIENT_WAKE,
		.irq		= OMAP44XX_IRQ_SYS_1N,
		.platform_data	= &espresso_twl6032_pdata,
	},
#ifdef CONFIG_SND_OMAP_SOC_ESPRESSO
	{
		I2C_BOARD_INFO("wm1811", 0x34>>1),
		.platform_data = &wm1811_pdata,
	}
#endif
};

static struct fixed_voltage_config espresso_vmmc_config = {
	.supply_name		= "vmmc",
	.microvolts		= 2800000, /* 2.8V */
	.gpio			= GPIO_TF_EN,
	.startup_delay		= 0,
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &espresso_vmmc,
};

static struct platform_device espresso_vmmc_device = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev = {
		.platform_data	= &espresso_vmmc_config,
	},
};

static struct regulator_consumer_supply espresso_vmmc_external_supplies = {
	.supply		= "vmmc",
	.dev_name	= "omap_hsmmc.1",
};

static struct regulator_init_data espresso_vmmc_external_data = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies = &espresso_vmmc_external_supplies,
};

static struct fixed_voltage_config espresso_vmmc_external = {
	.supply_name		= "eMMC_LDO",
	.gpio			= GPIO_EMMC_EN,
	.microvolts		= 1800000, /* 1.8V */
	.startup_delay		= 100000, /* 100 ms */
	.enable_high		= 1,
	.enabled_at_boot	= 1,
	.init_data		= &espresso_vmmc_external_data,
};

static struct platform_device espresso_vmmc_external_device = {
	.name		= "reg-fixed-voltage",
	.id		= 3,
	.dev = {
		.platform_data	= &espresso_vmmc_external,
	},
};

static void __init espresso_audio_init(void)
{
#ifdef CONFIG_SND_OMAP_SOC_ESPRESSO
	platform_device_register(&vbatt_device);

	if (!board_is_espresso10()) {
		wm1811_pdata.use_submic = true;
		wm1811_pdata.submic_gpio = GPIO_SUB_MICBIAS_EN;
	}
#endif
}

void __init omap4_espresso_pmic_init(void)
{
	/* Update oscillator information */
	omap_pm_set_osc_lp_time(15000, 1);

	/*
	 * This will allow unused regulator to be shutdown. This flag
	 * should be set in the board file. Before regulators are registered.
	 */
	regulator_has_full_constraints();

	if (board_is_espresso10())
		espresso_twl6032_pdata.rtc = &espresso_rtc;

	espresso_audio_init();

	platform_add_devices(espresso_pmic_devices,
			ARRAY_SIZE(espresso_pmic_devices));

	i2c_register_board_info(1, espresso_twl6032_i2c1_board_info,
			ARRAY_SIZE(espresso_twl6032_i2c1_board_info));

	/*
	 * Register fixed regulators to control external LDO.
	 */
	platform_device_register(&espresso_vmmc_device);
	platform_device_register(&espresso_vmmc_external_device);

	/*
	 * Drive MSECURE high for TWL6030 write access.
	 */
	gpio_request(GPIO_SYS_DRM_MSEC, "SYS_DRM_MSEC");
	gpio_direction_output(GPIO_SYS_DRM_MSEC, 1);

	if (enable_sr)
		omap_enable_smartreflex_on_init();

	/* enable off-mode */
	omap_pm_enable_off_mode();
}

/* arch/arm/mach-omap2/board-espresso10-pmic.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-espresso-pmic.c
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

#ifdef CONFIG_SND_SOC_WM8994
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>
#endif

#include <plat/omap-pm.h>
#include "pm.h"

#include "board-espresso10.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "common-board-devices.h"

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

char *rpmsg_cam_regulator_name[] = {
	"cam2pwr"
};

#ifdef CONFIG_SND_SOC_WM8994
static const struct regulator_consumer_supply vbatt_supplies[] = {
	REGULATOR_SUPPLY("LDO1VDD", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD1", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "1-001a"),
	REGULATOR_SUPPLY("AVDD2", "1-001a"),
	REGULATOR_SUPPLY("CPVDD", "1-001a"),
	REGULATOR_SUPPLY("DBVDD1", "1-001a"),
	REGULATOR_SUPPLY("DBVDD2", "1-001a"),
	REGULATOR_SUPPLY("DBVDD3", "1-001a"),
};

static const struct regulator_init_data vbatt_initdata = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(vbatt_supplies),
	.consumer_supplies = vbatt_supplies,
};

static const struct fixed_voltage_config vbatt_config = {
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

static const struct regulator_consumer_supply wm1811_ldo1_supplies[] = {
	REGULATOR_SUPPLY("AVDD1", "1-001a"),
};

static const struct regulator_init_data wm1811_ldo1_initdata = {
	.constraints = {
		.name = "WM1811 LDO1",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo1_supplies),
	.consumer_supplies = wm1811_ldo1_supplies,
};

static const struct regulator_consumer_supply wm1811_ldo2_supplies[] = {
	REGULATOR_SUPPLY("DCVDD", "1-001a"),
};

static const struct regulator_init_data wm1811_ldo2_initdata = {
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
		},
		{
			.init_data = &wm1811_ldo2_initdata,
		}
	},

	/* Regulated mode at highest output voltage */
	.micbias = { 0x2f, 0x29 },

	.ldo_ena_always_driven = true,
};
#endif

static struct regulator_init_data espresso10_vaux1 = {
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct regulator_consumer_supply espresso_vaux2_supplies[] = {
	REGULATOR_SUPPLY("VAP_IO_2.8V", NULL),
	REGULATOR_SUPPLY("SENSOR_2.8V", "4-0018"),
};

static struct regulator_init_data espresso10_vaux2 = {
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso_vaux2_supplies),
	.consumer_supplies	= espresso_vaux2_supplies,
};

static struct regulator_init_data espresso10_vaux3 = {
	.constraints = {
		.min_uV			= 3000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem		= {
			.disabled = true,
		},
	},
};

static struct regulator_consumer_supply espresso10_vmmc_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.0"),
};

static struct regulator_init_data espresso10_vmmc = {
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
	.num_consumer_supplies	= ARRAY_SIZE(espresso10_vmmc_supply),
	.consumer_supplies	= espresso10_vmmc_supply,
};

static struct fixed_voltage_config espresso10_vmmc_config = {
	.supply_name		= "vmmc",
	.microvolts		= 2800000,
	.startup_delay		= 0,
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &espresso10_vmmc,
};

static struct platform_device espresso10_vmmc_device = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev = {
		.platform_data	= &espresso10_vmmc_config,
	},
};

static struct regulator_init_data espresso10_vpp = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 2500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem		= {
			.disabled	= true,
		},
	},
};

static struct regulator_init_data espresso10_vusim = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct regulator_init_data espresso10_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct regulator_consumer_supply espresso10_vcxio_supply[] = {
	REGULATOR_SUPPLY("VCXIO_1.8V", NULL),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};

static struct regulator_init_data espresso10_vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem		= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso10_vcxio_supply),
	.consumer_supplies	= espresso10_vcxio_supply,
};

static struct regulator_consumer_supply espresso10_vdac_supply[] = {
	{
		.supply		= "hdmi_vref",
	},
};

static struct regulator_init_data espresso10_vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso10_vdac_supply),
	.consumer_supplies	= espresso10_vdac_supply,
};

static struct regulator_consumer_supply espresso10_vusb_supply[] = {
	REGULATOR_SUPPLY("VUSB_3.3V", NULL),
	REGULATOR_SUPPLY("vusb", NULL),
};

static struct regulator_init_data espresso10_vusb = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem		= {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso10_vusb_supply),
	.consumer_supplies	= espresso10_vusb_supply,
};

static struct regulator_init_data espresso10_clk32kg = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
};

static struct regulator_init_data espresso10_clk32kaudio = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
};

static struct regulator_init_data espresso10_vmem = {
	.constraints = {
		.min_uV			= 1225000,
		.max_uV			= 1225000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct regulator_consumer_supply espresso10_v2v1_supply[] = {
	REGULATOR_SUPPLY("VSEL_2.1V", NULL),
};

static struct regulator_init_data espresso10_v2v1 = {
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
	.num_consumer_supplies	= ARRAY_SIZE(espresso10_v2v1_supply),
	.consumer_supplies	= espresso10_v2v1_supply,
};

#ifdef CONFIG_TWL6040_CODEC
static struct twl4030_codec_audio_data espresso10_audio = {
	/* single-step ramp for headset and handsfree */
	.hs_left_step		= 0x0f,
	.hs_right_step		= 0x0f,
	.hf_left_step		= 0x1d,
	.hf_right_step		= 0x1d,
	.ep_step		= 0x0f,
};

static struct twl4030_codec_data espresso10_codec = {
	.audio		= &espresso10_audio,
	.naudint_irq	= OMAP44XX_IRQ_SYS_2N,
	.irq_base	= TWL6040_CODEC_IRQ_BASE,
};
#endif

static struct twl4030_madc_platform_data espresso10_madc = {
	.irq_line	= -1,
	.features	= TWL6030_CLASS | TWL6032_SUBCLASS,
};

static struct platform_device espresso10_madc_device = {
	.name		= "twl6030_madc",
	.id		= -1,
	.dev = {
		.platform_data		= &espresso10_madc,
	},
};

static void espresso10_twl6030_init(void)
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
		pr_err("%s:disable charger interrupt fail!\n", __func__);

	/* espresso10 use only preq1 of twl6032 */
	val = ~(DEV_GRP_P1) << TWL6030_PHEONIX_MSK_TRANS_SHIFT;
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0, val,
					TWL6030_PHOENIX_MSK_TRANSITION);
	if (ret)
		pr_err("%s:PHOENIX_MSK_TRANSITION write fail!\n", __func__);

	/*
	 * Enable charge backup battery and set charging voltage to 2.6V.
	 * Set VRTC low power mode in off/sleep and standard power mode in on.
	 */
	val = TWL_BBSPOR_CFG_VRTC_EN_SLP_STS | TWL_BBSPOR_CFG_VRTC_EN_OFF_STS |
		TWL_BBSPOR_CFG_VRTC_PWEN;
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0, val,
					TWL6030_BBSPOR_CFG);
	if (ret)
		pr_err("%s: BBSPOR_CFG write fail!\n", __func__);


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

	return;
}

static struct twl4030_power_data espresso10_power_data = {
	.twl4030_board_init	= espresso10_twl6030_init,
};

static struct twl4030_platform_data espresso10_twl6030_pdata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* pmic power data*/
	.power		= &espresso10_power_data,

	/* Regulators */
	.vusim		= &espresso10_vusim,
	.vmmc		= &espresso10_vmmc,
	.vpp		= &espresso10_vpp,
	.vana		= &espresso10_vana,
	.vcxio		= &espresso10_vcxio,
	.vdac		= &espresso10_vdac,
	.vusb		= &espresso10_vusb,
	.vaux1		= &espresso10_vaux1,
	.vaux2		= &espresso10_vaux2,
	.vaux3		= &espresso10_vaux3,
	.clk32kg	= &espresso10_clk32kg,
	.clk32kaudio	= &espresso10_clk32kaudio,
	.vmem		= &espresso10_vmem,
	.v2v1		= &espresso10_v2v1,

	/* children */
#ifdef CONFIG_TWL6040_CODEC
	.codec		= &espresso10_codec,
#endif
	.madc		= &espresso10_madc,
};

static struct regulator_init_data espresso10_ldo2_nc = {
	.constraints = {
		.min_uV = 1000000,
		.max_uV = 3300000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem		= {
			.disabled	= true,
		},
	},
};

static struct regulator_consumer_supply espresso_vdd_io_1V8_supplies[] = {
	REGULATOR_SUPPLY("VDD_IO_1.8V", NULL),
	REGULATOR_SUPPLY("SENSOR_1.8V", "4-0018"),
};

static struct regulator_init_data espresso10_ldo5 = {
	.constraints = {
		.min_uV = 1800000,
		.max_uV = 1800000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(espresso_vdd_io_1V8_supplies),
	.consumer_supplies	= espresso_vdd_io_1V8_supplies,
};

static struct regulator_init_data espresso10_ldo7_nc = {
	.constraints = {
		.min_uV = 1000000,
		.max_uV = 3300000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem		= {
			.disabled	= true,
		},
	},
};

static struct regulator_init_data espresso10_ldoln_nc = {
	.constraints = {
		.min_uV = 1000000,
		.max_uV = 3300000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem		= {
			.disabled	= true,
		},
	},
};

/* espresso10 use lod2 for VAP_IO_2.8V and ldo4 is NC in rev0.2 */
static struct twl4030_platform_data espresso10_twl6032_pdata_rev02 = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* pmic power data*/
	.power		= &espresso10_power_data,

	/* TWL6025 LDO regulators */
	.vana		= &espresso10_vana,
	.ldo1		= &espresso10_vaux1,
	.ldo2		= &espresso10_vaux2,
	.ldo3		= &espresso10_vusim,
	.ldo4		= &espresso10_vpp,
	.ldo5		= &espresso10_vmmc,
	.ldo6		= &espresso10_vcxio,
	.ldo7		= &espresso10_ldo7_nc,
	.ldoln		= &espresso10_ldoln_nc,
	.ldousb		= &espresso10_vusb,
	.clk32kg	= &espresso10_clk32kg,
	.clk32kaudio	= &espresso10_clk32kaudio,

	/* children */
#ifdef CONFIG_TWL6040_CODEC
	.codec		= &espresso10_codec,
#endif
	.madc		= &espresso10_madc,
};


struct twl4030_rtc_data espresso10_rtc = {
	.auto_comp = 1,
	.comp_value = -3200,
};
/*
 * Use lod4 for VAP_IO_2.8V and ldo2 is NC from rev0.3
 * use ldo5 for VDD_IO_1.8V and there's ext ldo for mmc slot.
 */
static struct twl4030_platform_data espresso10_twl6032_pdata_rev03 = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* pmic power data*/
	.power		= &espresso10_power_data,

	/* TWL6025 LDO regulators */
	.vana		= &espresso10_vana,
	.ldo1		= &espresso10_vaux1,
	.ldo2		= &espresso10_ldo2_nc,
	.ldo3		= &espresso10_vusim,
	.ldo4		= &espresso10_vaux2,
	.ldo5		= &espresso10_ldo5,
	.ldo6		= &espresso10_vcxio,
	.ldo7		= &espresso10_ldo7_nc,
	.ldoln		= &espresso10_ldoln_nc,
	.ldousb		= &espresso10_vusb,
	.clk32kg	= &espresso10_clk32kg,
	.clk32kaudio	= &espresso10_clk32kaudio,

	/* children */
#ifdef CONFIG_TWL6040_CODEC
	.codec		= &espresso10_codec,
#endif
	.madc		= &espresso10_madc,
	.rtc		= &espresso10_rtc,
};

static struct platform_device *espresso10_pmic_devices[] __initdata = {
	&espresso10_madc_device,
};

static struct i2c_board_info
		espresso10_twl6030_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("twl6030", 0x48),
		.flags		= I2C_CLIENT_WAKE,
		.irq		= OMAP44XX_IRQ_SYS_1N,
		.platform_data	= &espresso10_twl6030_pdata,
	},
#ifdef CONFIG_SND_SOC_WM8994
	{
		I2C_BOARD_INFO("wm1811", 0x34>>1),
		.platform_data = &wm1811_pdata,
	}
#endif
};

static struct i2c_board_info
		espresso10_twl6032_i2c1_board_info_rev02[] __initdata = {
	{
		I2C_BOARD_INFO("twl6025", 0x48),
		.flags		= I2C_CLIENT_WAKE,
		.irq		= OMAP44XX_IRQ_SYS_1N,
		.platform_data	= &espresso10_twl6032_pdata_rev02,
	},
#ifdef CONFIG_SND_SOC_WM8994
	{
		I2C_BOARD_INFO("wm1811", 0x34>>1),
		.platform_data = &wm1811_pdata,
	}
#endif
};

static struct i2c_board_info
		espresso10_twl6032_i2c1_board_info_rev03[] __initdata = {
	{
		I2C_BOARD_INFO("twl6032", 0x48),
		.flags		= I2C_CLIENT_WAKE,
		.irq		= OMAP44XX_IRQ_SYS_1N,
		.platform_data	= &espresso10_twl6032_pdata_rev03,
	},
#ifdef CONFIG_SND_SOC_WM8994
	{
		I2C_BOARD_INFO("wm1811", 0x34>>1),
		.platform_data = &wm1811_pdata,
	}
#endif
};

static void __init espresso10_audio_init(void)
{
#ifdef CONFIG_TWL6040_CODEC
	espresso10_codec.audpwron_gpio =
		omap_muxtbl_get_gpio_by_name("AUD_PWRON");
#endif

#ifdef CONFIG_SND_SOC_WM8994
	platform_device_register(&vbatt_device);

	wm1811_pdata.ldo[0].enable =
		omap_muxtbl_get_gpio_by_name("CODEC_LDO_EN");
#endif
}

void __init omap4_espresso10_pmic_init(void)
{
	unsigned int board_type = omap4_espresso10_get_board_type();
	unsigned int gpio_sys_drm_msec =
		omap_muxtbl_get_gpio_by_name("SYS_DRM_MSEC");

	/* Update oscillator information */
	omap_pm_set_osc_lp_time(15000, 1);

	/*
	 * This will allow unused regulator to be shutdown. This flag
	 * should be set in the board file. Before regulators are registered.
	 */
	regulator_has_full_constraints();

	espresso10_audio_init();

	platform_add_devices(espresso10_pmic_devices,
			     ARRAY_SIZE(espresso10_pmic_devices));

	/*
	 * PMIC is change from twl6030 to twl6032 from rev0.2.
	 * Espresso10 rev0.2 board have 5 as system_rev.
	 * ldo4 is used for VAP_IO_2.8V and ldo is nc from rev0.3
	 * Espresso10 rev0.3 board have 6 as system_rev.
	 */
	if (system_rev >= 6)
		i2c_register_board_info(1,
			espresso10_twl6032_i2c1_board_info_rev03,
			ARRAY_SIZE(espresso10_twl6032_i2c1_board_info_rev03));
	else if (system_rev == 5)
		i2c_register_board_info(1,
			espresso10_twl6032_i2c1_board_info_rev02,
			ARRAY_SIZE(espresso10_twl6032_i2c1_board_info_rev02));
	else
		i2c_register_board_info(1,
			espresso10_twl6030_i2c1_board_info,
			ARRAY_SIZE(espresso10_twl6030_i2c1_board_info));

	/*
	 * Use external ldo for tflash from rev0.3
	 * Register fixed regulator to control ldo which is used by tflash.
	 */
	if (system_rev >= 6) {
		espresso10_vmmc_config.gpio =
		omap_muxtbl_get_gpio_by_name("TF_EN");
		platform_device_register(&espresso10_vmmc_device);
	}

	/*
	 * only best buy Wi-Fi verstion support MHL from rev0.4
	 * Set lodln regulator as VDAC regulator which is used by MHL.
	 */
	if (board_type == SEC_MACHINE_ESPRESSO10_USA_BBY && system_rev >= 7)
		espresso10_twl6032_pdata_rev03.ldoln = &espresso10_vdac;


	/*
	 * Drive MSECURE high for TWL6030 write access.
	 */
	gpio_request(gpio_sys_drm_msec, "SYS_DRM_MSEC");
	gpio_direction_output(gpio_sys_drm_msec, 1);

	if (enable_sr)
		omap_enable_smartreflex_on_init();

	/*enable off-mode*/
	omap_pm_enable_off_mode();
}

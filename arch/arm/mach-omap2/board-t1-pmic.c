/* arch/arm/mach-omap2/board-t1-pmic.c
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
#include <plat/usb.h>

#include "board-t1.h"
#include "mux.h"
#include "omap_muxtbl.h"

#define TWL_REG_CONTROLLER_INT_MASK	0x00
#define TWL_CONTROLLER_MVBUS_DET	(1 << 1)
#define TWL_CONTROLLER_RSVD		(1 << 5)

#define TWL6030_PHEONIX_MSK_TRANS_SHIFT	0x05

#define TWL_BBSPOR_CFG_BBSEL_2_6V	(0x02 << 1)
#define TWL_BBSPOR_CFG_BB_CHG_EN	(1 << 3)
#define TWL_BBSPOR_CFG_VRTC_PWEN	(1 << 4)
#define TWL_BBSPOR_CFG_VRTC_EN_OFF_STS	(1 << 5)
#define TWL_BBSPOR_CFG_VRTC_EN_SLP_STS	(1 << 6)

static struct regulator_init_data t1_vaux1 = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};
static struct regulator_consumer_supply t1_vaux2_supplies[] = {
	{
		.supply = "vaux2",
		.dev_name = NULL,
	},
	{
		.supply = "VTI_1.8V",
		.dev_name = NULL,
	},
};

static struct regulator_init_data t1_vaux2 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(t1_vaux2_supplies),
	.consumer_supplies = t1_vaux2_supplies,
};

static struct regulator_consumer_supply t1_vaux3_supplies[] = {
	{
		.supply		= "vlcd",
	},
};

static struct regulator_init_data t1_vaux3 = {
	.constraints = {
		.min_uV			= 3000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(t1_vaux3_supplies),
	.consumer_supplies	= t1_vaux3_supplies,
};

static struct regulator_consumer_supply t1_vmmc_supply[] = {
	{
		.supply		= "vmmc",
		.dev_name	= "omap_hsmmc.0",
	},
};

static struct regulator_init_data t1_vmmc = {
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
	.num_consumer_supplies	= ARRAY_SIZE(t1_vmmc_supply),
	.consumer_supplies	= t1_vmmc_supply,
};

static struct regulator_init_data t1_vpp = {
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

static struct regulator_init_data t1_vusim = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
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

static struct regulator_init_data t1_vana = {
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

static struct regulator_consumer_supply t1_vcxio_supply[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};
static struct regulator_init_data t1_vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem		= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(t1_vcxio_supply),
	.consumer_supplies	= t1_vcxio_supply,
};

static struct regulator_consumer_supply t1_vdac_supply[] = {
	{
		.supply		= "hdmi_vref",
	},
};

static struct regulator_init_data t1_vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(t1_vdac_supply),
	.consumer_supplies	= t1_vdac_supply,
};

static struct regulator_consumer_supply t1_vusb_supply[] = {
	REGULATOR_SUPPLY("vusb", "t1_otg"),
};

static struct regulator_init_data t1_vusb = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem		= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(t1_vusb_supply),
	.consumer_supplies	= t1_vusb_supply,
};

/* sysen is a twl6030 signal that contorl ext ldo modeled as a regulator */
static struct regulator_consumer_supply t1_sysen_supply[] = {
	{
		.supply		= "vmmc",
		.dev_name	= "omap_hsmmc.1",
	},
};
static struct regulator_init_data t1_sysen = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = ARRAY_SIZE(t1_sysen_supply),
	.consumer_supplies	= t1_sysen_supply,
};

/* clk32kg is a twl6030 32khz clock modeled as a regulator, used by GPS */
static struct regulator_init_data t1_clk32kg = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
};

static struct regulator_consumer_supply t1_clk32kaudio_supply[] = {
	{
		.supply		= "clk32kaudio",
	},
	{
		.supply		= "twl6040_clk32k",
	}
};

static struct regulator_init_data t1_clk32kaudio = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(t1_clk32kaudio_supply),
	.consumer_supplies	= t1_clk32kaudio_supply,
};

static struct regulator_init_data t1_vmem = {
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

static struct regulator_init_data t1_v2v1 = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem		= {
			.disabled	= true,
		},
	},
};

static struct twl4030_codec_audio_data t1_audio = {
	/* single-step ramp for headset and handsfree */
	.hs_left_step		= 0x0f,
	.hs_right_step		= 0x0f,
	.hf_left_step		= 0x1d,
	.hf_right_step		= 0x1d,
	.ep_step		= 0x0f,
};

static struct twl4030_codec_data t1_codec = {
	.audio		= &t1_audio,
	.naudint_irq	= OMAP44XX_IRQ_SYS_2N,
	.irq_base	= TWL6040_CODEC_IRQ_BASE,
};

static struct twl4030_madc_platform_data t1_madc = {
	.irq_line	= -1,
};

static struct platform_device t1_madc_device = {
	.name		= "twl6030_madc",
	.id		= -1,
	.dev = {
		.platform_data		= &t1_madc,
	},
};

static void t1_twl6030_init(void)
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

	/* T1 use only preq1 of twl6030 */
	val = ~(DEV_GRP_P1) << TWL6030_PHEONIX_MSK_TRANS_SHIFT;
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0, val,
					TWL6030_PHOENIX_MSK_TRANSITION);
	if (ret)
		pr_err("%s:PHOENIX_MSK_TRANSITION write fail!\n", __func__);


	/*
	 * Enable charge backup battery and set charging voltage to 2.6V.
	 * Set VRTC low power mode in off/sleep and standard power mode in on.
	 */
	val = TWL_BBSPOR_CFG_BB_CHG_EN |
		TWL_BBSPOR_CFG_BBSEL_2_6V |
		TWL_BBSPOR_CFG_VRTC_EN_SLP_STS |
		TWL_BBSPOR_CFG_VRTC_EN_OFF_STS |
		TWL_BBSPOR_CFG_VRTC_PWEN;
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0, val,
					TWL6030_BBSPOR_CFG);
	if (ret)
		pr_err("%s: BBSPOR_CFG write fail!\n", __func__);

	return;
}

static struct twl4030_power_data t1_power_data = {
	.twl4030_board_init	= t1_twl6030_init,
};

static struct twl4030_platform_data t1_twl_pdata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* pmic power data*/
	.power		= &t1_power_data,

	/* Regulators */
	.vusim		= &t1_vusim,
	.vmmc		= &t1_vmmc,
	.vpp		= &t1_vpp,
	.vana		= &t1_vana,
	.vcxio		= &t1_vcxio,
	.vdac		= &t1_vdac,
	.vusb		= &t1_vusb,
	.vaux1		= &t1_vaux1,
	.vaux2		= &t1_vaux2,
	.vaux3		= &t1_vaux3,
	.sysen		= &t1_sysen,
	.clk32kg	= &t1_clk32kg,
	.clk32kaudio	= &t1_clk32kaudio,
	.vmem		= &t1_vmem,
	.v2v1		= &t1_v2v1,

	/* children */
	.codec		= &t1_codec,
	.madc		= &t1_madc,
};

static struct platform_device *t1_pmic_devices[] __initdata = {
	&t1_madc_device,
};

static struct i2c_board_info t1_pmic_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("twl6030", 0x48),
		.flags		= I2C_CLIENT_WAKE,
		.irq		= OMAP44XX_IRQ_SYS_1N,
		.platform_data	= &t1_twl_pdata,
	},
};

static void __init t1_audio_init(void)
{
	t1_codec.audpwron_gpio = omap_muxtbl_get_gpio_by_name("AUD_PWRON");
}

void __init omap4_t1_pmic_init(void)
{
	unsigned int gpio_sys_drm_msec =
		omap_muxtbl_get_gpio_by_name("SYS_DRM_MSEC");

	/*
	 * This will allow unused regulator to be shutdown. This flag
	 * should be set in the board file. Before regulators are registered.
	 */
	regulator_has_full_constraints();

	t1_audio_init();

	platform_add_devices(t1_pmic_devices, ARRAY_SIZE(t1_pmic_devices));

	i2c_register_board_info(1, t1_pmic_i2c1_board_info,
				ARRAY_SIZE(t1_pmic_i2c1_board_info));

	/*
	 * Drive MSECURE high for TWL6030 write access.
	 */
	gpio_request(gpio_sys_drm_msec, "SYS_DRM_MSEC");
	gpio_direction_output(gpio_sys_drm_msec, 1);
}

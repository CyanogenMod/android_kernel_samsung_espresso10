/* arch/arm/mach-omap2/board-gokey-pmic.c
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
#include <plat/omap-pm.h>
#include "pm.h"

#include "board-gokey.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "common-board-devices.h"

#define TWL_REG_CONTROLLER_INT_MASK	0x00
#define TWL_CONTROLLER_MVBUS_DET	(1 << 1)
#define TWL_CONTROLLER_RSVD		(1 << 5)

#define TWL6030_PHEONIX_MSK_TRANS_SHIFT	0x05

static bool enable_sr = true;
module_param(enable_sr, bool, S_IRUSR | S_IRGRP | S_IROTH);

char *rpmsg_cam_regulator_name[] = {
	"cam2pwr"
};

static struct regulator_consumer_supply gokey_vaux1_supplies[] = {
	REGULATOR_SUPPLY("GPS_LNA_1.8V", NULL),
};

static struct regulator_init_data gokey_vaux1 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.enabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vaux1_supplies),
	.consumer_supplies	= gokey_vaux1_supplies,
};

static struct regulator_consumer_supply gokey_vaux2_supplies[] = {
	REGULATOR_SUPPLY("KEYLED_3P3V", NULL),
};

static struct regulator_init_data gokey_vaux2 = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vaux2_supplies),
	.consumer_supplies	= gokey_vaux2_supplies,
};

static struct regulator_consumer_supply gokey_vaux3_supply[] = {
	REGULATOR_SUPPLY("VLCD_3P0V", NULL),
};

static struct regulator_init_data gokey_vaux3 = {
	.constraints = {
			.min_uV = 3300000,
			.max_uV = 3300000,
			.apply_uV = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL
					  | REGULATOR_MODE_STANDBY,
			.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
						| REGULATOR_CHANGE_MODE
						| REGULATOR_CHANGE_STATUS,
			.state_mem = {
				.disabled = true,
			},
			.state_standby = {
				.disabled = true,
			},
			.boot_on = true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vaux3_supply),
	.consumer_supplies	= gokey_vaux3_supply,
};


static struct regulator_consumer_supply gokey_vmmc_supply[] = {
	REGULATOR_SUPPLY("VLCD_1P8V", NULL),
};

static struct regulator_init_data gokey_vmmc = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
		.state_standby = {
			.disabled = true,
		},
		.boot_on = true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vmmc_supply),
	.consumer_supplies	= gokey_vmmc_supply,
};

static struct regulator_consumer_supply gokey_vpp_supply[] = {
	REGULATOR_SUPPLY("VDD_VPP", NULL),
};

static struct regulator_init_data gokey_vpp = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 2500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vpp_supply),
	.consumer_supplies	= gokey_vpp_supply,
};

/* not used for harrison */
static struct regulator_consumer_supply gokey_vusim_supply[] = {
	REGULATOR_SUPPLY("VAP_IO_3.3V", NULL),
};

static struct regulator_init_data gokey_vusim = {
	.constraints = {
			.min_uV = 3300000,
			.max_uV = 3300000,
			.apply_uV = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL
					  | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
			.state_mem = {
				.enabled = true,
			},
			.always_on = true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vusim_supply),
	.consumer_supplies	= gokey_vusim_supply,
};

static struct regulator_consumer_supply gokey_vana_supply[] = {
	REGULATOR_SUPPLY("VDD_ANA", NULL),
};

static struct regulator_init_data gokey_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.enabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vana_supply),
	.consumer_supplies	= gokey_vana_supply,
};

static struct regulator_consumer_supply gokey_vcxio_supply[] = {
	REGULATOR_SUPPLY("VCXIO_1.8V", NULL),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};

static struct regulator_init_data gokey_vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vcxio_supply),
	.consumer_supplies	= gokey_vcxio_supply,
};

static struct regulator_consumer_supply gokey_vdac_supply[] = {
	{
		.supply		= "hdmi_vref",
	},
};

/* not connected */
static struct regulator_init_data gokey_vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vdac_supply),
	.consumer_supplies	= gokey_vdac_supply,
};

static struct regulator_consumer_supply gokey_vusb_supply[] = {
	REGULATOR_SUPPLY("VUSB_3.3V", NULL),
	REGULATOR_SUPPLY("vusb", "gokey_otg"),
};

static struct regulator_init_data gokey_vusb = {
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
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vusb_supply),
	.consumer_supplies	= gokey_vusb_supply,
};

static struct regulator_consumer_supply gokey_clk32kg_supply[] = {
	REGULATOR_SUPPLY("GPS_BT_CLK32K", NULL),
};

static struct regulator_init_data gokey_clk32kg = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_clk32kg_supply),
	.consumer_supplies	= gokey_clk32kg_supply,
};

static struct regulator_consumer_supply gokey_clk32kaudio_supply[] = {
	REGULATOR_SUPPLY("CLK32K_AUDIO", NULL),
	REGULATOR_SUPPLY("clk32kaudio", NULL),
};

static struct regulator_init_data gokey_clk32kaudio = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_clk32kaudio_supply),
	.consumer_supplies	= gokey_clk32kaudio_supply,
};

static struct regulator_consumer_supply gokey_vmem_supply[] = {
	REGULATOR_SUPPLY("VMEM_1.2V", NULL),
};

static struct regulator_init_data gokey_vmem = {
	.constraints = {
		.min_uV			= 1225000,
		.max_uV			= 1225000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.enabled = true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gokey_vmem_supply),
	.consumer_supplies	= gokey_vmem_supply,
};

static struct regulator_consumer_supply gokey_v2v1_supply[] = {
	REGULATOR_SUPPLY("VSEL_2.1V", NULL),
};

static struct regulator_init_data gokey_v2v1 = {
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
	.num_consumer_supplies	= ARRAY_SIZE(gokey_v2v1_supply),
	.consumer_supplies	= gokey_v2v1_supply,
};

static struct twl4030_madc_platform_data gokey_madc = {
	.irq_line	= -1,
	.features	= TWL6030_CLASS | TWL6032_SUBCLASS,
};

static struct platform_device gokey_madc_device = {
	.name		= "twl6030_madc",
	.id		= -1,
	.dev = {
		.platform_data		= &gokey_madc,
	},
};

/* add audio clock */
/* <Regulator>_CFG_TRANS registers */
#define ON_STATE_TO_ON		0x03
#define ON_STATE_TO_AMS		0x01
#define ON_STATE_TO_OFF		0x00

#define SLEEP_STATE_TO_ON	0x0C
#define SLEEP_STATE_TO_AMS	0x04
#define SLEEP_STATE_TO_OFF	0x00

#define OFF_STATE_TO_ON		0x30
#define OFF_STATE_TO_AMS	0x10
#define OFF_STATE_TO_OFF	0x00

/* <Regulator>_CFG_STATE registers */
#define STATE_MOD_NOGRP		0x00
#define STATE_MOD_APP		0x20
#define STATE_MOD_CON		0x40
#define STATE_MOD_MOD		0x80

#define STATE_MOD_ON		0x01
#define STATE_MOD_SLEEP		0x03
#define STATE_MOD_OFF		0x00

#define TWL6030_REG_CLK32KG_CFG_GRP			0xBC
#define TWL6030_REG_CLK32KG_CFG_TRANS			0xBD
#define TWL6030_REG_CLK32KG_CFG_STATE			0xBE

#define TWL6030_REG_CLK32KAUDIO_CFG_GRP			0xBF
#define TWL6030_REG_CLK32KAUDIO_CFG_TRANS		0xC0
#define TWL6030_REG_CLK32KAUDIO_CFG_STATE		0xC1

static void gokey_twl6030_init(void)
{
	int ret;
	u8	val;

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
	/*only preq1 of twl6030 */
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0,
			~(DEV_GRP_P1) << TWL6030_PHEONIX_MSK_TRANS_SHIFT,
			TWL6030_PHOENIX_MSK_TRANSITION);
	if (ret)
		pr_err("%s:PHOENIX_MSK_TRANSITION write fail!\n", __func__);


	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0,
			&val, TWL6030_BBSPOR_CFG);


	/*enable backkup battery charge*/
	val |= (1<<3);
	/*configure in low power mode*/
	val |= (1<<6 | 1<<5);
	/*	3.15V 00:3.0, 01:2.5, 10:3.15 11:VSYS */
	val |= (1<<2);
	val &= ~(1<<1);
	ret |= twl_i2c_write_u8(TWL6030_MODULE_ID0,
			val, TWL6030_BBSPOR_CFG);

	/* Set CLK32KG/CLK32KAUDIO alaways on */
	twl_i2c_write_u8(TWL6030_MODULE_ID0,
			OFF_STATE_TO_OFF | SLEEP_STATE_TO_AMS | ON_STATE_TO_AMS,
			TWL6030_REG_CLK32KG_CFG_TRANS);
	twl_i2c_write_u8(TWL6030_MODULE_ID0, STATE_MOD_ON,
			TWL6030_REG_CLK32KG_CFG_STATE);

	twl_i2c_write_u8(TWL6030_MODULE_ID0,
			OFF_STATE_TO_OFF | SLEEP_STATE_TO_AMS | ON_STATE_TO_AMS,
			TWL6030_REG_CLK32KAUDIO_CFG_TRANS);
	twl_i2c_write_u8(TWL6030_MODULE_ID0, STATE_MOD_ON,
			TWL6030_REG_CLK32KAUDIO_CFG_STATE);

	if (ret)
		pr_err("%s:TWL6030 BBSPOR_CFG write fail!\n", __func__);

	/* Work around for Vcore rises suddenly */
	if (system_rev == 0) {
		/* #define SMPS1_CFG_TRANS 0x52 */
		twl_i2c_write_u8(TWL6030_MODULE_ID0, 0x0F, 0x53);
		pr_info("%s: system_rev is 0.cpu1 force pwm mode\n", __func__);
	}

	return;
}

/* controlled by PREQ1_RES_ASS_B*/
/* refer twl6030-power.c*/
static struct twl4030_resconfig gokey_rconfig[] __initdata = {
	{ .resource = RES_LDO1, .devgroup = 0, },
	{ .resource = RES_LDO2, .devgroup = 0, },
	{ .resource = RES_LDO3, .devgroup = 0, },
	{ .resource = RES_LDO4, .devgroup = 0, },
	{ .resource = RES_LDO5, .devgroup = 0, },
	{ .resource = RES_LDO7, .devgroup = 0, },
	{ .resource = RES_LDOLN, .devgroup = 0, },
	{ .resource = RES_LDOUSB, .devgroup = 0, },
	{ .resource = TWL4030_RESCONFIG_UNDEF, 0},
};

static struct twl4030_power_data gokey_power_data = {
	.twl4030_board_init	= gokey_twl6030_init,
	.resource_config = gokey_rconfig,
};

static struct regulator_init_data gokey_ldo2_nc = {
	.constraints = {
		.min_uV = 1800000,
		.max_uV = 1800000,
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
static struct regulator_init_data gokey_ldo7_nc = {
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
static struct regulator_init_data gokey_ldoln_nc = {
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
static struct regulator_init_data gokey_vdd_io_1V8 = {
	.constraints = {
		.min_uV = 1800000,
		.max_uV = 1800000,
		.apply_uV = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on = true,
		},
};


static struct twl4030_platform_data gokey_twl6032_pdata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* pmic power data*/
	.power		= &gokey_power_data,
	/* TWL6032 LDO regulators */
	.vana		= &gokey_vana,
	.ldo1		= &gokey_vaux1,
	.ldo2		= &gokey_ldo2_nc,
	.ldo3		= &gokey_vaux3,
	.ldo4		= &gokey_vaux2,
	.ldo5		= &gokey_vmmc,
	.ldo6		= &gokey_vcxio,
	.ldo7		= &gokey_ldo7_nc,
	.ldoln		= &gokey_ldoln_nc,
	.ldousb		= &gokey_vusb,
	.clk32kg	= &gokey_clk32kg,
	.clk32kaudio	= &gokey_clk32kaudio,
	/* TWL6032 DCDC regulators */
	/*
	.smps3		= &gokey_vsel1v2;
	.smps4		= &gokey_vap_io_1v8;
	*/
	.madc		= &gokey_madc,
};

static struct platform_device *gokey_pmic_devices[] __initdata = {
	&gokey_madc_device,
};

static struct i2c_board_info gokey_twl6032_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("twl6032", 0x48),
		.flags		= I2C_CLIENT_WAKE,
		.irq		= OMAP44XX_IRQ_SYS_1N,
		.platform_data	= &gokey_twl6032_pdata,
	},
};

void __init omap4_gokey_pmic_init(void)
{
	unsigned int gpio_sys_drm_msec =
		omap_muxtbl_get_gpio_by_name("SYS_DRM_MSEC");

	/* Update oscillator information */
	omap_pm_set_osc_lp_time(15000, 1);

	/*
	 * This will allow unused regulator to be shutdown. This flag
	 * should be set in the board file. Before regulators are registered.
	 */
	regulator_has_full_constraints();

	platform_add_devices(gokey_pmic_devices,
			     ARRAY_SIZE(gokey_pmic_devices));

	i2c_register_board_info(1, gokey_twl6032_i2c1_board_info,
				ARRAY_SIZE(gokey_twl6032_i2c1_board_info));

	gokey_twl6032_pdata.ldoln = &gokey_vdac;

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

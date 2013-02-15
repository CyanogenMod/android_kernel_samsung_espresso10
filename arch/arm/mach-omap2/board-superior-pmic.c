/* arch/arm/mach-omap2/board-superior-pmic.c
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>

#include <plat/omap-pm.h>
#include "pm.h"

#include "board-superior.h"
#include "mux.h"
#include "omap_muxtbl.h"
#include "common-board-devices.h"

#define TWL_REG_CONTROLLER_INT_MASK	0x00
#define TWL_CONTROLLER_MVBUS_DET	(1 << 1)
#define TWL_CONTROLLER_RSVD		(1 << 5)

#define TWL6030_PHEONIX_MSK_TRANS_SHIFT	0x05
#define TWL6030_CFG_LDO_PD1		0xF4

#define TWL6030_PHEONIX_MSK_TRANS_SHIFT	0x05

static bool enable_sr = true;
module_param(enable_sr, bool, S_IRUSR | S_IRGRP | S_IROTH);

char *rpmsg_cam_regulator_name[] = {
	"CAM_SENSOR_IO_1.8V",      /* ldon17 */
	"VT_CAM_1.8V",             /* ldon20 */
	"CAM_AF_A2.8V",            /* ldon22 */
	"CAM_SENSOR_CORE_1.2V",    /* ldon24 */
	"CAM_SENSOR_A2.8V",        /* ldon25 */
};

/* twl6034 init data */
static struct regulator_consumer_supply superior_smps3_supplies[] = {
	REGULATOR_SUPPLY("VSEL_1.2V", NULL),
};

static struct regulator_init_data superior_smps3 = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 1200000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_smps3_supplies),
	.consumer_supplies	= superior_smps3_supplies,
};

static struct regulator_consumer_supply superior_smps4_supplies[] = {
	REGULATOR_SUPPLY("VAP_IO_1.8V", NULL),
};

static struct regulator_init_data superior_smps4 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_smps4_supplies),
	.consumer_supplies	= superior_smps4_supplies,
};

static struct regulator_consumer_supply superior_smps7_supplies[] = {
	REGULATOR_SUPPLY("VSEL_2.1V", NULL),
};

static struct regulator_init_data superior_smps7 = {
	.constraints = {
		.name			= "SMPSn7",
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on = true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_smps7_supplies),
	.consumer_supplies	= superior_smps7_supplies,
};

static struct regulator_consumer_supply superior_smps8_supplies[] = {
	REGULATOR_SUPPLY("V_IMA_1.1V", NULL),
};

static struct regulator_init_data superior_smps8 = {
	.constraints = {
		.min_uV			= 1100000,
		.max_uV			= 1100000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_smps8_supplies),
	.consumer_supplies	= superior_smps8_supplies,
};

static struct regulator_consumer_supply superior_smps9_supplies[] = {
	REGULATOR_SUPPLY("VSMPSn9", NULL),
};

static struct regulator_init_data superior_smps9 = {
	.constraints = {
		.name			= "SMPSn9",
		.min_uV			= 3000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on = true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_smps9_supplies),
	.consumer_supplies	= superior_smps9_supplies,
};

static struct regulator_consumer_supply superior_smps10_supplies[] = {
	REGULATOR_SUPPLY("VMEM_VDDF_2.85V", NULL),
	REGULATOR_SUPPLY("vmmc_aux", "omap_hsmmc.1"),
};

static struct regulator_init_data superior_smps10 = {
	.constraints = {
		.min_uV			= 2842000,
		.max_uV			= 2842000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_smps10_supplies),
	.consumer_supplies	= superior_smps10_supplies,
};

/* twl6034 init data */
static struct regulator_consumer_supply superior_vana_supply[] = {
	REGULATOR_SUPPLY("VDD_AP_ANA", NULL),
};

static struct regulator_init_data superior_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_vana_supply),
	.consumer_supplies	= superior_vana_supply,
};

static struct regulator_consumer_supply superior_ldo1_supplies[] = {
	REGULATOR_SUPPLY("VDD_VPP", NULL),
};

static struct regulator_init_data superior_ldo1 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldo1_supplies),
	.consumer_supplies	= superior_ldo1_supplies,
};

static struct regulator_consumer_supply superior_ldo2_supplies[] = {
	REGULATOR_SUPPLY("VAP_3.0V", NULL),
};

static struct regulator_init_data superior_ldo2 = {
	.constraints = {
		.min_uV			= 3000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldo2_supplies),
	.consumer_supplies	= superior_ldo2_supplies,
};

static struct regulator_consumer_supply superior_ldo3_supply[] = {
	REGULATOR_SUPPLY("2TOUCH_3.3V", NULL),
};

static struct regulator_init_data superior_ldo3 = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldo3_supply),
	.consumer_supplies	= superior_ldo3_supply,
};

static struct regulator_consumer_supply superior_ldo4_supply[] = {
	REGULATOR_SUPPLY("VCC_3.1V_LCD", NULL),
	REGULATOR_SUPPLY("vlcd_vcc_3.1V", NULL),
};
static struct regulator_init_data superior_ldo4 = {
	.constraints = {
		.min_uV			= 3100000,
		.max_uV			= 3100000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldo4_supply),
	.consumer_supplies	= superior_ldo4_supply,
};

static struct regulator_consumer_supply superior_ldo5_supply[] = {
	REGULATOR_SUPPLY("VSD_3.0V", NULL),
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.0"),
};

static struct regulator_init_data superior_ldo5 = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldo5_supply),
	.consumer_supplies	= superior_ldo5_supply,
};

static struct regulator_consumer_supply superior_ldo6_supply[] = {
	REGULATOR_SUPPLY("VCXIO_1.8V", NULL),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};

static struct regulator_init_data superior_ldo6 = {
	.constraints = {
			.min_uV = 1800000,
			.max_uV = 1800000,
			.apply_uV = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL
					  | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
			.always_on = true,
			.state_mem = {
				.disabled = true,
			},
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldo6_supply),
	.consumer_supplies	= superior_ldo6_supply,
};

static struct regulator_consumer_supply superior_ldo7_supply[] = {
	REGULATOR_SUPPLY("LCD_IO_2.2V", NULL),
	REGULATOR_SUPPLY("vlcd_io_2.2V", NULL),
};

static struct regulator_init_data superior_ldo7 = {
	.supply_regulator		= "SMPSn9",
	.constraints = {
		.min_uV			= 2200000,
		.max_uV			= 2200000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldo7_supply),
	.consumer_supplies	= superior_ldo7_supply,
};

static struct regulator_consumer_supply superior_ldoln_supply[] = {
	REGULATOR_SUPPLY("VDAC_1.8V", NULL),
};

static struct regulator_init_data superior_ldoln = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldoln_supply),
	.consumer_supplies	= superior_ldoln_supply,
};

static struct regulator_consumer_supply superior_ldon12_supply[] = {
	REGULATOR_SUPPLY("TSP_AVDD_3.3V", NULL),
};

static struct regulator_init_data superior_ldon12 = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.boot_on		= true,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon12_supply),
	.consumer_supplies	= superior_ldon12_supply,
};

static struct regulator_consumer_supply superior_ldon13_supply[] = {
	REGULATOR_SUPPLY("SENSOR_IO_1.8V", NULL),
};

static struct regulator_init_data superior_ldon13 = {
	.supply_regulator		= "SMPSn7",
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon13_supply),
	.consumer_supplies	= superior_ldon13_supply,
};

static struct regulator_consumer_supply superior_ldon14_supply[] = {
	REGULATOR_SUPPLY("SENSOR_IO_2.8V", NULL),
};

static struct regulator_init_data superior_ldon14 = {
	.supply_regulator		= "SMPSn9",
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon14_supply),
	.consumer_supplies	= superior_ldon14_supply,
};

static struct regulator_init_data superior_ldon15 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_consumer_supply superior_ldon16_supply[] = {
	REGULATOR_SUPPLY("KEYLED_3.3V", NULL),
};

static struct regulator_init_data superior_ldon16 = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon16_supply),
	.consumer_supplies	= superior_ldon16_supply,
};

static struct regulator_consumer_supply superior_ldon17_supply[] = {
	REGULATOR_SUPPLY("CAM_SENSOR_IO_1.8V", NULL),
};

static struct regulator_init_data superior_ldon17 = {
	.supply_regulator		= "SMPSn7",
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon17_supply),
	.consumer_supplies	= superior_ldon17_supply,
};

static struct regulator_consumer_supply superior_ldon18_supply[] = {
	REGULATOR_SUPPLY("PS_ALS_VCC_3.0V", NULL),
};

static struct regulator_init_data superior_ldon18 = {
	.constraints = {
		.min_uV			= 3000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on = true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon18_supply),
	.consumer_supplies	= superior_ldon18_supply,
};

static struct regulator_consumer_supply superior_ldon19_supply[] = {
	REGULATOR_SUPPLY("PS_VLED_3.0V", NULL),
};

static struct regulator_init_data superior_ldon19 = {
	.constraints = {
		.min_uV			= 3000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon19_supply),
	.consumer_supplies	= superior_ldon19_supply,
};

static struct regulator_consumer_supply superior_ldon20_supply[] = {
	REGULATOR_SUPPLY("VT_CAM_1.8V", NULL),
};

static struct regulator_init_data superior_ldon20 = {
	.supply_regulator		= "SMPSn7",
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon20_supply),
	.consumer_supplies	= superior_ldon20_supply,
};

static struct regulator_consumer_supply superior_ldon21_supply[] = {
	REGULATOR_SUPPLY("VCC_SUB_2.0V", NULL),
};

static struct regulator_init_data superior_ldon21 = {
	.constraints = {
		.min_uV			= 2000000,
		.max_uV			= 2000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon21_supply),
	.consumer_supplies	= superior_ldon21_supply,
};

static struct regulator_consumer_supply superior_ldon22_supply[] = {
	REGULATOR_SUPPLY("CAM_AF_A2.8V", NULL),
};

static struct regulator_init_data superior_ldon22 = {
	.supply_regulator		= "SMPSn9",
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon22_supply),
	.consumer_supplies	= superior_ldon22_supply,
};

static struct regulator_consumer_supply superior_ldon23_supply[] = {
	REGULATOR_SUPPLY("2TOUCH_1.8V", NULL),
};

static struct regulator_init_data superior_ldon23 = {
	.supply_regulator		= "SMPSn7",
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon23_supply),
	.consumer_supplies	= superior_ldon23_supply,
};

static struct regulator_consumer_supply superior_ldon24_supply[] = {
	REGULATOR_SUPPLY("CAM_SENSOR_CORE_1.2V", NULL),
};

static struct regulator_init_data superior_ldon24 = {
	.supply_regulator		= "SMPSn7",
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 1200000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon24_supply),
	.consumer_supplies	= superior_ldon24_supply,
};

static struct regulator_consumer_supply superior_ldon25_supply[] = {
	REGULATOR_SUPPLY("CAM_SENSOR_A2.8V", NULL),
};

static struct regulator_init_data superior_ldon25 = {
	.supply_regulator		= "SMPSn9",
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS
					| REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon25_supply),
	.consumer_supplies	= superior_ldon25_supply,
};

static struct regulator_consumer_supply superior_ldon26_supply[] = {
	REGULATOR_SUPPLY("V_MOTOR_3.3V", NULL),
};

static struct regulator_init_data superior_ldon26 = {
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon26_supply),
	.consumer_supplies	= superior_ldon26_supply,
};

static struct regulator_consumer_supply superior_ldon27_supply[] = {
	REGULATOR_SUPPLY("V_IMA_1.8V", NULL),
};

static struct regulator_init_data superior_ldon27 = {
	.supply_regulator		= "SMPSn7",
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon27_supply),
	.consumer_supplies	= superior_ldon27_supply,
};

static struct regulator_consumer_supply superior_ldon28_supply[] = {
	REGULATOR_SUPPLY("VMEM_VDD_1.8V", NULL),
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.1"),
};

static struct regulator_init_data superior_ldon28 = {
	.supply_regulator		= "SMPSn7",
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_ldon28_supply),
	.consumer_supplies	= superior_ldon28_supply,
};

static struct regulator_consumer_supply superior_clk32kg_supply[] = {
	REGULATOR_SUPPLY("GPS_CLK32K", NULL),
};

static struct regulator_init_data superior_clk32kg = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_clk32kg_supply),
	.consumer_supplies	= superior_clk32kg_supply,
};

static struct regulator_consumer_supply superior_clk32kaudio_supply[] = {
	REGULATOR_SUPPLY("CLK32K_AUDIO", NULL),
};

static struct regulator_init_data superior_clk32kaudio = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(superior_clk32kaudio_supply),
	.consumer_supplies	= superior_clk32kaudio_supply,
};

static struct twl4030_madc_platform_data superior_madc = {
	.irq_line	= -1,
	.features	= TWL6030_CLASS | TWL6032_SUBCLASS | TWL6034_SUBCLASS,
};

static struct platform_device superior_madc_device = {
	.name		= "twl6030_madc",
	.id		= -1,
	.dev = {
		.platform_data		= &superior_madc,
	},
};

static void superior_twl6034_init(void)
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
	/*only preq1 of twl6030 */
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0,
			~(DEV_GRP_P1) << TWL6030_PHEONIX_MSK_TRANS_SHIFT,
			TWL6030_PHOENIX_MSK_TRANSITION);
	if (ret)
		pr_err("%s:PHOENIX_MSK_TRANSITION write fail!\n", __func__);


	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0, &val, TWL6030_BBSPOR_CFG);

	/* enable backkup battery charge and set charging voltage to 3.15V */
	val |= (0x01 << 0x03) | (0x02 << 0x01);

	/* configure in low power mode of VRTC */
	val |= (0x01 << 0x06) | (0x01 << 0x05);

	ret |= twl_i2c_write_u8(TWL6030_MODULE_ID0, val, TWL6030_BBSPOR_CFG);
	if (ret)
		pr_err("%s:TWL6034 BBSPOR_CFG write fail!\n", __func__);


	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0,
			&val, TWL6030_CFG_LDO_PD1);

	/* TI recommand
	 * recommended to leave vpp_cust turn off(float).
	 * disable internal pull-down when vpp_cust is turned off
	 */
	val &= ~(1<<7); /*LDO7*/
	ret |= twl_i2c_write_u8(TWL6030_MODULE_ID0,
			val, TWL6030_CFG_LDO_PD1);
	if (ret)
		pr_err("%s:TWL6030 CFG_LDO_PD2 write fail!\n",
				__func__);

	return;
}

static struct twl4030_resconfig superior_rconfig[] __initdata = {
	{ .resource = RES_SMPS8, .devgroup = 0, },
	{ .resource = RES_LDO3, .devgroup = 0, },
	{ .resource = RES_LDO4, .devgroup = 0, },
	{ .resource = RES_LDOn15, .devgroup = 0, },
	{ .resource = RES_LDOn17, .devgroup = 0, },
	{ .resource = RES_LDOn20, .devgroup = 0, },
	{ .resource = RES_LDOn21, .devgroup = 0, },
	{ .resource = RES_LDOn22, .devgroup = 0, },
	{ .resource = RES_LDOn24, .devgroup = 0, },
	{ .resource = RES_LDOn25, .devgroup = 0, },
	{ .resource = RES_LDOn27, .devgroup = 0, },
	{ .resource = TWL4030_RESCONFIG_UNDEF, 0},
};

static struct twl4030_power_data superior_power_data __initdata = {
	.twl4030_board_init	= superior_twl6034_init,
	.resource_config = superior_rconfig,
};

static struct platform_device *superior_pmic_devices[] __initdata = {
	&superior_madc_device,
};

struct twl4030_rtc_data __initdata superior_rtc = {
	.rtc_default_time = {
		.tm_sec = 0,
		.tm_min = 0,
		.tm_mday = 1,
		.tm_hour = 0,
		.tm_mon = 0,
		.tm_year = 112,
	},
};

static struct twl4030_platform_data superior_twl6034_pdata __initdata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* pmic power data*/
	.power		= &superior_power_data,

	/* Regulators */
	.vana		= &superior_vana,
	.ldo1		= &superior_ldo1,
	.ldo2		= &superior_ldo2,
	.ldo3		= &superior_ldo3,
	.ldo4		= &superior_ldo4,
	.ldo5		= &superior_ldo5,
	.ldo6		= &superior_ldo6,
	.ldo7		= &superior_ldo7,
	.ldoln		= &superior_ldoln,

	.ldo12		= &superior_ldon12,
	.ldo13		= &superior_ldon13,
	.ldo14		= &superior_ldon14,
	.ldo15		= &superior_ldon15,
	.ldo16		= &superior_ldon16,
	.ldo17		= &superior_ldon17,
	.ldo18		= &superior_ldon18,
	.ldo19		= &superior_ldon19,
	.ldo20		= &superior_ldon20,
	.ldo21		= &superior_ldon21,
	.ldo22		= &superior_ldon22,
	.ldo23		= &superior_ldon23,
	.ldo24		= &superior_ldon24,
	.ldo25		= &superior_ldon25,
	.ldo26		= &superior_ldon26,
	.ldo27		= &superior_ldon27,
	.ldo28		= &superior_ldon28,

	/* DCDC */
	.smps3		= &superior_smps3,
	.smps4		= &superior_smps4,
	.smps7		= &superior_smps7,
	.smps8		= &superior_smps8,
	.smps9		= &superior_smps9,
	.smps10		= &superior_smps10,

	/* 32k clock */
	.clk32kg	= &superior_clk32kg,
	.clk32kaudio	= &superior_clk32kaudio,

	/* children */
	.madc		= &superior_madc,
	.rtc		= &superior_rtc,
};

static struct i2c_board_info superior_twl6034_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("twl6034", 0x48),
		.flags		= I2C_CLIENT_WAKE,
		.irq		= OMAP44XX_IRQ_SYS_1N,
	},
};

void __init omap4_superior_pmic_init(void)
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

	platform_add_devices(superior_pmic_devices,
			     ARRAY_SIZE(superior_pmic_devices));

	superior_twl6034_i2c1_board_info[0].platform_data =
						&superior_twl6034_pdata;

	i2c_register_board_info(1, superior_twl6034_i2c1_board_info,
				ARRAY_SIZE(superior_twl6034_i2c1_board_info));

	/*
	 * Drive MSECURE high for TWL6034 write access.
	 */
	gpio_request(gpio_sys_drm_msec, "SYS_DRM_MSEC");
	gpio_direction_output(gpio_sys_drm_msec, 1);

	if (enable_sr)
		omap_enable_smartreflex_on_init();

	/*enable off-mode*/
	omap_pm_enable_off_mode();
}

/*
 * LPDDR2 data as per SAMSUNG data sheet
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <mach/emif.h>
#include "board-gokey.h"

static const struct lpddr2_timings gokey_lpddr2_samsung_timings_400_mhz = {
	.max_freq = 400000000,
	.RL = 6,
	.tRPab = 21,
	.tRCD = 18,
	.tWR = 15,
	.tRASmin = 42,
	.tRRD = 10,
	.tWTRx2 = 15,
	.tXSR = 140,
	.tXPx2 = 15,
	.tRFCab = 130,
	.tRTPx2 = 15,
	.tCKE = 3,
	.tCKESR = 15,
	.tZQCS = 90,
	.tZQCL = 360,
	.tZQINIT = 1000,
	.tDQSCKMAXx2 = 11,
	.tRASmax = 70,
	.tFAW = 50,
};

static const struct lpddr2_timings gokey_lpddr2_samsung_timings_200_mhz = {
	.max_freq = 200000000,
	.RL = 3,
	.tRPab = 21,
	.tRCD = 18,
	.tWR = 15,
	.tRASmin = 42,
	.tRRD = 10,
	.tWTRx2 = 20,
	.tXSR = 140,
	.tXPx2 = 15,
	.tRFCab = 130,
	.tRTPx2 = 15,
	.tCKE = 3,
	.tCKESR = 15,
	.tZQCS = 90,
	.tZQCL = 360,
	.tZQINIT = 1000,
	.tDQSCKMAXx2 = 11,
	.tRASmax = 70,
	.tFAW = 50
};

static const struct lpddr2_min_tck gokey_lpddr2_samsung_min_tck = {
	.tRL = 3,
	.tRP_AB = 3,
	.tRCD = 3,
	.tWR = 3,
	.tRAS_MIN = 3,
	.tRRD = 2,
	.tWTR = 2,
	.tXP = 2,
	.tRTP = 2,
	.tCKE = 3,
	.tCKESR = 3,
	.tFAW = 8
};

static struct lpddr2_device_info gokey_lpddr2_samsung_2G_S4_dev = {
	.device_timings = {
			   &gokey_lpddr2_samsung_timings_200_mhz,
			   &gokey_lpddr2_samsung_timings_400_mhz
			  },
	.min_tck = &gokey_lpddr2_samsung_min_tck,
	.type = LPDDR2_TYPE_S4,
	.density = LPDDR2_DENSITY_2Gb,
	.io_width = LPDDR2_IO_WIDTH_32,
	.emif_ddr_selfrefresh_cycles = 262144,
};

/*
 * LPDDR2 Configeration Data:
 * The memory organisation is as below :
 *	EMIF1 - CS0 -	2 Gb
 *	        CS1 -	2 Gb
 *	EMIF2 - CS0 -	2 Gb
 *	        CS1 -	2 Gb
 *	--------------------
 *	TOTAL -		8 Gb
 *
 * Same devices installed on EMIF1 and EMIF2
 */
static __initdata struct emif_device_details gokey_emif_devices_qdp = {
	.cs0_device = &gokey_lpddr2_samsung_2G_S4_dev,
	.cs1_device = &gokey_lpddr2_samsung_2G_S4_dev
};

void __init omap4_gokey_emif_init(void)
{
	omap_emif_setup_device_details(&gokey_emif_devices_qdp,
				       &gokey_emif_devices_qdp);
}

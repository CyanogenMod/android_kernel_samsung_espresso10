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
#include <mach/lpddr2-jedec.h>
#include "board-espresso.h"

struct lpddr2_device_info lpddr2_samsung_4G_S4_dev = {
	.device_timings = {
		&lpddr2_jedec_timings_200_mhz,
		&lpddr2_jedec_timings_400_mhz
	},
	.min_tck	= &lpddr2_jedec_min_tck,
	.type		= LPDDR2_TYPE_S4,
	.density	= LPDDR2_DENSITY_4Gb,
	.io_width	= LPDDR2_IO_WIDTH_32,
	.emif_ddr_selfrefresh_cycles = 262144,
};

/*
 * LPDDR2 Configuration Data:
 * The memory organisation is as below :
 *	EMIF1 - CS0 -	4 Gb
 *	EMIF2 - CS0 -	4 Gb
 *	--------------------
 *	TOTAL -		8 Gb
 *
 * Same devices installed on EMIF1 and EMIF2
 */
static __initdata struct emif_device_details emif_devices = {
	.cs0_device = &lpddr2_samsung_4G_S4_dev,
};

void __init omap4_espresso_emif_init(void)
{
	omap_emif_setup_device_details(&emif_devices, &emif_devices);
}

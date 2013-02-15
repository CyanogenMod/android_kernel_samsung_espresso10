/* arch/arm/mach-omap2/board-superior-spiflash.c
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

#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <plat/mcspi.h>

#include "board-superior.h"

/* SPI flash memory in camera module */
#define F_ROM_SPI_BUS_NUM       3
#define F_ROM_SPI_CS            0
#define F_ROM_SPI_SPEED_HZ      24000000

static const struct flash_platform_data w25q80_pdata = {
	.name = "fm25m08",
	.type = "fm25m08",
};

static struct omap2_mcspi_device_config flash_mcspi_config = {
	.turbo_mode     = 0,
	.single_channel = 1,    /* 0: slave, 1: master */
	.swap_datalines = 0,
};

static struct spi_board_info palau_flash_rom[] __initdata = {
	{
		.modalias = "m25p80",
		.controller_data = &flash_mcspi_config,
		.platform_data = &w25q80_pdata,
		.bus_num = F_ROM_SPI_BUS_NUM,
		.chip_select = F_ROM_SPI_CS,
		.max_speed_hz = F_ROM_SPI_SPEED_HZ,
		.mode = SPI_MODE_0,
	},
};


void omap4_superior_spiflash_init(void)
{
	int err = 0;
	err = spi_register_board_info(palau_flash_rom,
					ARRAY_SIZE(palau_flash_rom));
	if (err)
		pr_err("failed to register SPI F-ROM\n");
}

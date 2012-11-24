/*
 * Support for the TI OMAP4 Tablet Application board.
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include <linux/kernel.h>
#include <linux/i2c.h>

#include <plat/omap_apps_brd_id.h>

static int board_revision;

bool omap_is_board_version(int req_board_version)
{
	if (req_board_version == board_revision)
		return true;

	return false;
}

int omap_get_board_version(void)
{
	return board_revision;
}

__init int omap_init_board_version(void)
{
	switch (system_rev) {
	case OMAP4_TABLET_1_0:
		board_revision = OMAP4_TABLET_1_0;
		break;
	case OMAP4_TABLET_2_0:
		board_revision = OMAP4_TABLET_2_0;
		break;
	case OMAP4_BLAZE_ID:
		board_revision = OMAP4_BLAZE_ID;
		break;
	default:
		board_revision = -1;
	}

	return board_revision;
}

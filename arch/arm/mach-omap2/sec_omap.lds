/* arch/arm/mach-omap2/sec_omap.lds
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

SECTIONS {
	/* begining of sec_muxtbl data section */
	.sec_muxtbl_start : {
		. = ALIGN(4);
		*(.sec_muxtbl_start)
	}
	/* all variables defined by __sec_muxtbl_list will be here */
	.sec_muxtbl_data : {
		. = ALIGN(4);
		*(.sec_muxtbl_data)
	}
	/* end of sec_muxtbl data section */
	.sec_muxtbl_end : {
		. = ALIGN(4);
		*(.sec_muxtbl_end)
	}
}

/* arch/arm/mach-omap2/sec_board_id.h
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

#ifndef __SEC_BOARD_ID_H__
#define __SEC_BOARD_ID_H__

/* GT-I9100G */
#define SEC_MACHINE_T1			0x00

/* GT-P3100 / GT-P3110 / GT-P3113 */
/* Reference Device */
#define SEC_MACHINE_ESPRESSO		0x01
/* Non-Modem Device */
#define SEC_MACHINE_ESPRESSO_WIFI	0x03
/* Non-Modem Device for Best Buy */
#define SEC_MACHINE_ESPRESSO_USA_BBY	0x05

/* GT-P5100 / GT-P5110 / GT-5113 */
/* Reference Device */
#define SEC_MACHINE_ESPRESSO10		0x02
/* Non-Modem Device */
#define SEC_MACHINE_ESPRESSO10_WIFI	0x04
/* Non-Modem Device for Best Buy */
#define SEC_MACHINE_ESPRESSO10_USA_BBY	0x06

/* GT-I9260 */
#define SEC_MACHINE_SUPERIOR		0x07

/* GT-I9270 */
#define SEC_MACHINE_PALAU		0x08

/* GT-N5100 */
/* Without WACOM */
#define SEC_MACHINE_KONA		0x09
/* Non-Modem Device, Without WACOM */
#define SEC_MACHINE_KONA_WIFI		0x0A
/* Reference Device */
#define SEC_MACHINE_KONA_WACOM		0x0B
/* Non-Modem Device */
#define SEC_MACHINE_KONA_WACOM_WIFI	0x0C

/* YP-GH1 */
#define SEC_MACHINE_GOKEY		0x0D

#endif /* __SEC_BOARD_ID_H__ */

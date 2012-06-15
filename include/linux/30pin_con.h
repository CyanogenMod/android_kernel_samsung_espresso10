/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_ACC_CONN_H
#define __ASM_ARCH_ACC_CONN_H

/*
 *	ACCESSORY		DETECTION
 *	-----------------------------------------------
 *	OTG			DOCK_INT gpio
 *	EARJACK with DESKDOCK	DOCK_INT gpio
 *	CARDOCK			DOCK_INT gpio
 *	TV OUTPUT LINE		DOCK_INT gpio
 *	KEYBOARD-DOCK		ACCESSORY_INT_1.8V gpio
 *	DESK-DOCK		ACCESSORY_INT_1.8V gpio
 *	JIG			JIG_ON_18 gpio
 *	USB			TA_nCONNECTED gpio
 *	TA			TA_nCONNECTED gpio
 */
enum acc_type {
	P30_OTG = 0,
	P30_EARJACK_WITH_DOCK,
	P30_CARDOCK,
	P30_ANAL_TV_OUT,
	P30_KEYBOARDDOCK,
	P30_DESKDOCK,
	P30_JIG,
	P30_USB,
	P30_TA,
};


struct acc_con_platform_data {
	int accessory_irq_gpio;
	int dock_irq_gpio;
	int jig_on_gpio;
	void (*detected) (int device, bool connected);
	int (*dock_keyboard_cb) (bool connected);
	s16 (*get_accessory_adc) (void);
};

#endif

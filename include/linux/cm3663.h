/*
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */


#ifndef __LINUX_CM3663_H
#define __LINUX_CM3663_H

#include <linux/types.h>

#define ALS_BUFFER_NUM	10
#define LIGHT_BUFFER_NUM	5
#define PROX_READ_NUM	40

/* ADDSEL is LOW */
#define REGS_ARA		0x18
#define REGS_ALS_CMD		0x20
#define REGS_ALS_MSB		0x21
#define REGS_INIT		0x22
#define REGS_ALS_LSB		0x23
#define REGS_PS_CMD		0xB0
#define REGS_PS_DATA		0xB1
#define REGS_PS_THD		0xB2

#define PROXIMITY_THRESHOLD	0x0A
#define DEFAULT_POLL_DELAY	200000000

#ifdef __KERNEL__
static u8 reg_defaults[8] = {
	0x00, /* ARA: read only register */
	0x00, /* ALS_CMD: als cmd */
	0x00, /* ALS_MSB: read only register */
	0x20, /* INIT: interrupt disable */
	0x00, /* ALS_LSB: read only register */
	0x30, /* PS_CMD: interrupt disable */
	0x00, /* PS_DATA: read only register */
	PROXIMITY_THRESHOLD, /* PS_THD: 10 */
};

static const int adc_table[4] = {
	15,
	150,
	1500,
	15000,
};

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

static int brightness_step_table[] = { 0, 15, 150, 1500, 15000, 10000000 };
static int adc_step_table[5] = { 0, 12, 40, 490, 7900};

struct cm3663_platform_data {
	int irq;	/* proximity-sensor-output gpio */
	int (*proximity_power)(bool); /* ldo power for the proximity */
	int adc_step_table[5];
};
#endif /* __KERNEL__ */

#endif

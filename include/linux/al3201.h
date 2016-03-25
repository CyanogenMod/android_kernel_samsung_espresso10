/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef AL3201_H
#define AL3201_H

#define SENSOR_AL3201_ADDR		0x1c
#define SENSOR_BH1721FVC_ADDR	0x23

#define al3201_DRV_NAME		"AL3201"
#define DRIVER_VERSION		"1.0"

#define LUX_MIN_VALUE		0

#define LUX_MAX_VALUE		65528

#define LIMIT_RESET_COUNT	5

#define LIGHT_BUFFER_NUM	5
#define ALS_BUFFER_NUM		10

#define AL3201_RAN_COMMAND	0x01
#define AL3201_RAN_MASK		0x01
#define AL3201_RAN_SHIFT	(0)
#define AL3201_RST_MASK		0x03
#define AL3201_RST_SHIFT	(2)

#define AL3201_RT_COMMAND	0x02
#define AL3201_RT_MASK		0x03
#define AL3201_RT_SHIFT	(0)

#define AL3201_POW_COMMAND	0x00
#define AL3201_POW_MASK		0x03
#define AL3201_POW_UP		0x03
#define AL3201_POW_DOWN		0x00
#define AL3201_POW_SHIFT	(0)

#define	AL3201_ADC_LSB		0x0c
#define	AL3201_ADC_MSB		0x0d

#define AL3201_NUM_CACHABLE_REGS	5

static const u8 al3201_reg[AL3201_NUM_CACHABLE_REGS] = {
	0x00,
	0x01,
	0x02,
	0x0c,
	0x0d
};

static const int adc_table[5] = {
	20,
	180,
	1750,
	17000,
	65000,
};

struct al3201_platform_data {
	void (*power_on) (bool);
};

enum {
	OFF = 0,
	ON,
};

#endif

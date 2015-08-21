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

#ifndef _COMMON_H
#define _COMMON_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <mach/gpio.h>
#include <linux/gpio.h>

#include "../../../arch/arm/mach-omap2/mux.h"
#include "../../../arch/arm/mach-omap2/omap_muxtbl.h"

/* #define Si47xx_DEBUG */

#define error(fmt, arg...) printk(KERN_CRIT fmt "\n", ##arg)

#ifdef Si47xx_DEBUG
#define debug(fmt, arg...) printk(KERN_CRIT "--------" fmt "\n", ##arg)
#else
#define debug(fmt, arg...)
#endif

#define GPIO_FM_INT	"FM_INT"
#define GPIO_FM_RST	"FM_RST"


/* VNVS:7-JUNE'10 : RDS Interrupt ON Always */
/* (Enabling interrupt when RDS is enabled) */
#define RDS_INTERRUPT_ON_ALWAYS

/* VNVS:18-JUN'10 : For testing RDS */
/* Enable only for debugging RDS */
/* #define RDS_TESTING */
#ifdef RDS_TESTING
#define debug_rds(fmt, arg...) printk(KERN_CRIT "--------" fmt "\n", ##arg)
#define GROUP_TYPE_2A     (2 * 2 + 0)
#define GROUP_TYPE_2B     (2 * 2 + 1)
#else
#define debug_rds(fmt, arg...)
#endif

extern wait_queue_head_t Si47xx_waitq;


#endif

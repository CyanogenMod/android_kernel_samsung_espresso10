/**
 * SAMSUNG MODEM IPC header
 *
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

#ifndef __PACKET_DATA_PROTOCOL_H__
#define __PACKET_DATA_PROTOCOL_H__

#include <linux/netdevice.h>

#define PDP_HARD_HEADER_LEN	0
#define PDP_ADDR_LEN		0
#define PDP_TX_QUEUE_LEN	1000
#define PDP_WATCHDOG_TIMEO	(5 * HZ)

struct pdp_priv {
	int channel;
	struct net_device *parent;
};

extern struct net_device *create_pdp(int channel, struct net_device *parent);
extern void destroy_pdp(struct net_device **);
extern int vnet_start_xmit(struct sk_buff *skb, struct net_device *ndev);

#endif /* __PACKET_DATA_PROTOCOL_H__ */

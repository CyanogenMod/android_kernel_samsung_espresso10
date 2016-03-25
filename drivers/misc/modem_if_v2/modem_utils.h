/*
 * Copyright (C) 2012 Samsung Electronics.
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

#ifndef __MODEM_UTILS_H__
#define __MODEM_UTILS_H__

#include <linux/rbtree.h>

#define IS_CONNECTED(iod, ld) ((iod)->link_types & LINKTYPE((ld)->link_type))

#define MAX_MIF_BUFF_SIZE 0x80000 /* 512kb */
#define MAX_IPC_SKB_SIZE 4096
#define MAX_LOG_SIZE 64

#define MAX_LOG_CNT (MAX_MIF_BUFF_SIZE / MAX_LOG_SIZE)
#define MIF_ID_SIZE sizeof(enum mif_log_id)

#define MAX_IPC_LOG_SIZE \
	(MAX_LOG_SIZE - sizeof(enum mif_log_id) \
	 - sizeof(unsigned long long) - sizeof(size_t))
#define MAX_IRQ_LOG_SIZE \
	(MAX_LOG_SIZE - sizeof(enum mif_log_id) \
	 - sizeof(unsigned long long) - sizeof(struct mif_irq_map))
#define MAX_COM_LOG_SIZE \
	(MAX_LOG_SIZE - sizeof(enum mif_log_id) \
	 - sizeof(unsigned long long))
#define MAX_TIM_LOG_SIZE \
	(MAX_LOG_SIZE - sizeof(enum mif_log_id) \
	 - sizeof(unsigned long long) - sizeof(struct timespec))

enum mif_log_id {
	MIF_IPC_RL2AP = 1,
	MIF_IPC_AP2CP,
	MIF_IPC_CP2AP,
	MIF_IPC_AP2RL,
	MIF_IRQ,
	MIF_COM,
	MIF_TIME
};

struct mif_irq_map {
	u16 magic;
	u16 access;

	u16 fmt_tx_in;
	u16 fmt_tx_out;
	u16 fmt_rx_in;
	u16 fmt_rx_out;

	u16 raw_tx_in;
	u16 raw_tx_out;
	u16 raw_rx_in;
	u16 raw_rx_out;

	u16 cp2ap;
};

struct mif_ipc_block {
	enum mif_log_id id;
	unsigned long long time;
	size_t len;
	char buff[MAX_IPC_LOG_SIZE];
};

struct mif_irq_block {
	enum mif_log_id id;
	unsigned long long time;
	struct mif_irq_map map;
	char buff[MAX_IRQ_LOG_SIZE];
};

struct mif_common_block {
	enum mif_log_id id;
	unsigned long long time;
	char buff[MAX_COM_LOG_SIZE];
};

struct mif_time_block {
	enum mif_log_id id;
	unsigned long long time;
	struct timespec epoch;
	char buff[MAX_TIM_LOG_SIZE];
};

int mif_dump_dpram(struct io_device *);
int mif_dump_log(struct modem_shared *, struct io_device *);

#define mif_irq_log(msd, map, data, len) \
	_mif_irq_log(MIF_IRQ, msd, map, data, len)
#define mif_com_log(msd, format, ...) \
	_mif_com_log(MIF_COM, msd, pr_fmt(format), ##__VA_ARGS__)
#define mif_time_log(msd, epoch, data, len) \
	_mif_time_log(MIF_TIME, msd, epoch, data, len)

void mif_ipc_log(enum mif_log_id,
	struct modem_shared *, const char *, size_t);
void _mif_irq_log(enum mif_log_id,
	struct modem_shared *, struct mif_irq_map, const char *, size_t);
void _mif_com_log(enum mif_log_id,
	struct modem_shared *, const char *, ...);
void _mif_time_log(enum mif_log_id,
	struct modem_shared *, struct timespec, const char *, size_t);

/** find_linkdev - find a link device
 * @msd:	struct modem_shared *
 */
static inline struct link_device *find_linkdev(struct modem_shared *msd,
		enum modem_link link_type)
{
	struct link_device *ld;
	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (ld->link_type == link_type)
			return ld;
	}
	return NULL;
}

/** countbits - count number of 1 bits as fastest way
 * @n: number
 */
static inline unsigned int countbits(unsigned int n)
{
	unsigned int i;
	for (i = 0; n != 0; i++)
		n &= (n - 1);
	return i;
}

/* print buffer as hex string */
int pr_buffer(const char *tag, const char *data, size_t data_len,
							size_t max_len);

/* print a sk_buff as hex string */
#define pr_skb(tag, skb) \
	pr_buffer(tag, (char *)((skb)->data), (size_t)((skb)->len), (size_t)16)

/* print a urb as hex string */
#define pr_urb(tag, urb) \
	pr_buffer(tag, (char *)((urb)->transfer_buffer), \
			(size_t)((urb)->actual_length), (size_t)16)

/* get iod from tree functions */

struct io_device *get_iod_with_format(struct modem_shared *msd,
					enum dev_format format);
struct io_device *get_iod_with_channel(struct modem_shared *msd,
					unsigned channel);

static inline struct io_device *link_get_iod_with_format(
			struct link_device *ld, enum dev_format format)
{
	struct io_device *iod = get_iod_with_format(ld->msd, format);
	return (iod && IS_CONNECTED(iod, ld)) ? iod : NULL;
}

static inline struct io_device *link_get_iod_with_channel(
			struct link_device *ld, unsigned channel)
{
	struct io_device *iod = get_iod_with_channel(ld->msd, channel);
	return (iod && IS_CONNECTED(iod, ld)) ? iod : NULL;
}

/* insert iod to tree functions */
struct io_device *insert_iod_with_format(struct modem_shared *msd,
			enum dev_format format, struct io_device *iod);
struct io_device *insert_iod_with_channel(struct modem_shared *msd,
			unsigned channel, struct io_device *iod);

/* iodev for each */
typedef void (*action_fn)(struct io_device *iod, void *args);
void iodevs_for_each(struct modem_shared *msd, action_fn action, void *args);

/* netif wake/stop queue of iod */
void iodev_netif_wake(struct io_device *iod, void *args);
void iodev_netif_stop(struct io_device *iod, void *args);

#endif/*__MODEM_UTILS_H__*/

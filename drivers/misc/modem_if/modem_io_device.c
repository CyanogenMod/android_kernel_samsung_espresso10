/* /linux/drivers/misc/modem_if/modem_io_device.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
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

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/ratelimit.h>
#include <linux/device.h>

#include <linux/platform_data/modem.h>
#include "modem_prj.h"


#define HDLC_START	0x7F
#define HDLC_END	0x7E
#define SIZE_OF_HDLC_START	1
#define SIZE_OF_HDLC_END	1
#define MAX_RXDATA_SIZE		(4096 - 512)
#define MAX_MTU_TX_DATA_SIZE	1550

static const char hdlc_start[1] = { HDLC_START };
static const char hdlc_end[1] = { HDLC_END };

struct fmt_hdr {
	u16 len;
	u8 control;
} __packed;

struct raw_hdr {
	u32 len;
	u8 channel;
	u8 control;
} __packed;

struct rfs_hdr {
	u32 len;
	u8 cmd;
	u8 id;
} __packed;

static const char const *modem_state_name[] = {
	[STATE_OFFLINE]		= "OFFLINE",
	[STATE_CRASH_RESET]	= "CRASH_RESET",
	[STATE_CRASH_EXIT]	= "CRASH_EXIT",
	[STATE_BOOTING]		= "BOOTING",
	[STATE_ONLINE]		= "ONLINE",
	[STATE_LOADER_DONE]	= "LOADER_DONE",
};

static int rx_iodev_skb(struct io_device *iod);

static ssize_t show_waketime(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int msec;
	char *p = buf;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = container_of(miscdev, struct io_device,
			miscdev);

	msec = jiffies_to_msecs(iod->waketime);

	p += sprintf(buf, "raw waketime : %ums\n", msec);

	return p - buf;
}

static ssize_t store_waketime(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long msec;
	int ret;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = container_of(miscdev, struct io_device,
			miscdev);

	ret = strict_strtoul(buf, 10, &msec);
	if (ret)
		return count;

	iod->waketime = msecs_to_jiffies(msec);

	return count;
}

static struct device_attribute attr_waketime =
	__ATTR(waketime, S_IRUGO | S_IWUSR, show_waketime, store_waketime);

static int get_header_size(struct io_device *iod)
{
	switch (iod->format) {
	case IPC_FMT:
		return sizeof(struct fmt_hdr);

	case IPC_RAW:
	case IPC_MULTI_RAW:
		return sizeof(struct raw_hdr);

	case IPC_RFS:
		return sizeof(struct rfs_hdr);

	case IPC_BOOT:
		/* minimum size for transaction align */
		return 4;

	case IPC_RAMDUMP:
	default:
		return 0;
	}
}

static int get_hdlc_size(struct io_device *iod, char *buf)
{
	struct fmt_hdr *fmt_header;
	struct raw_hdr *raw_header;
	struct rfs_hdr *rfs_header;

	pr_debug("[MODEM_IF] buf : %02x %02x %02x (%d)\n", *buf, *(buf + 1),
				*(buf + 2), __LINE__);

	switch (iod->format) {
	case IPC_FMT:
		fmt_header = (struct fmt_hdr *)buf;
		return fmt_header->len;
	case IPC_RAW:
	case IPC_MULTI_RAW:
		raw_header = (struct raw_hdr *)buf;
		return raw_header->len;
	case IPC_RFS:
		rfs_header = (struct rfs_hdr *)buf;
		return rfs_header->len;
	default:
		break;
	}
	return 0;
}

static void *get_header(struct io_device *iod, size_t count,
			char *frame_header_buf)
{
	struct fmt_hdr *fmt_h;
	struct raw_hdr *raw_h;
	struct rfs_hdr *rfs_h;

	switch (iod->format) {
	case IPC_FMT:
		fmt_h = (struct fmt_hdr *)frame_header_buf;

		fmt_h->len = count + sizeof(struct fmt_hdr);
		fmt_h->control = 0;

		return (void *)frame_header_buf;

	case IPC_RAW:
	case IPC_MULTI_RAW:
		raw_h = (struct raw_hdr *)frame_header_buf;

		raw_h->len = count + sizeof(struct raw_hdr);
		raw_h->channel = iod->id & 0x1F;
		raw_h->control = 0;

		return (void *)frame_header_buf;

	case IPC_RFS:
		rfs_h = (struct rfs_hdr *)frame_header_buf;

		rfs_h->len = count + sizeof(struct raw_hdr);
		rfs_h->id = iod->id;

		return (void *)frame_header_buf;

	default:
		return 0;
	}
}

static inline int rx_hdlc_head_start_check(char *buf)
{
	/* check hdlc head and return size of start byte */
	return (buf[0] == HDLC_START) ? SIZE_OF_HDLC_START : -EBADMSG;
}

static inline int rx_hdlc_tail_check(char *buf)
{
	/* check hdlc tail and return size of tail byte */
	return (buf[0] == HDLC_END) ? SIZE_OF_HDLC_END : -EBADMSG;
}

/* remove hdlc header and store IPC header */
static int rx_hdlc_head_check(struct io_device *iod, char *buf, unsigned rest)
{
	struct header_data *hdr = &iod->h_data;
	int head_size = get_header_size(iod);
	int done_len = 0;
	int len = 0;

	/* first frame, remove start header 7F */
	if (!hdr->start) {
		len = rx_hdlc_head_start_check(buf);
		if (len < 0) {
			pr_err("[MODEM_IF] Wrong HDLC start: 0x%x(%s)\n",
						*buf, iod->name);
			return len; /*Wrong hdlc start*/
		}

		pr_debug("[MODEM_IF] check len : %d, rest : %d (%d)\n", len,
					rest, __LINE__);

		/* set the start flag of current packet */
		hdr->start = HDLC_START;
		hdr->len = 0;

		buf += len;
		done_len += len;
		rest -= len; /* rest, call by value */
	}

	pr_debug("[MODEM_IF] check len : %d, rest : %d (%d)\n", len, rest,
				__LINE__);

	/* store the IPC header to iod priv */
	if (hdr->len < head_size) {
		len = min(rest, head_size - hdr->len);
		memcpy(hdr->hdr + hdr->len, buf, len);
		hdr->len += len;
		done_len += len;
	}

	pr_debug("[MODEM_IF] check done_len : %d, rest : %d (%d)\n", done_len,
				rest, __LINE__);
	return done_len;
}

/* alloc skb and copy dat to skb */
static int rx_hdlc_data_check(struct io_device *iod, char *buf, unsigned rest)
{
	struct header_data *hdr = &iod->h_data;
	struct sk_buff *skb = iod->skb_recv;
	int head_size = get_header_size(iod);
	int data_size = get_hdlc_size(iod, hdr->hdr) - head_size;
	int alloc_size = min(data_size, MAX_RXDATA_SIZE);
	int len;
	int done_len = 0;
	int rest_len = data_size - hdr->flag_len;

	/* first payload data - alloc skb */
	if (!skb) {
		switch (iod->format) {
		case IPC_RFS:
			alloc_size = min(data_size + head_size,
					MAX_RXDATA_SIZE);
			skb = alloc_skb(alloc_size, GFP_ATOMIC);
			if (unlikely(!skb))
				return -ENOMEM;
			/* copy the RFS haeder to skb->data */
			memcpy(skb_put(skb, head_size), hdr->hdr, head_size);
			break;

		case IPC_MULTI_RAW:
			if (data_size > MAX_RXDATA_SIZE) {
				pr_err("%s: %s: packet size too large (%d)\n",
						__func__, iod->name, data_size);
				return -EINVAL;
			}

			if (iod->net_typ == UMTS_NETWORK)
				skb = alloc_skb(alloc_size, GFP_ATOMIC);
			else
				skb = alloc_skb(alloc_size +
					sizeof(struct ethhdr), GFP_ATOMIC);
			if (unlikely(!skb))
				return -ENOMEM;

			if (iod->net_typ != UMTS_NETWORK)
				skb_reserve(skb, sizeof(struct ethhdr));
			break;

		default:
			skb = alloc_skb(alloc_size, GFP_ATOMIC);
			if (unlikely(!skb))
				return -ENOMEM;
			break;
		}
		iod->skb_recv = skb;
	}

	while (rest > 0) {
		len = min(rest,  alloc_size - skb->len);
		len = min(len, rest_len);
		memcpy(skb_put(skb, len), buf, len);
		buf += len;
		done_len += len;
		hdr->flag_len += len;
		rest -= len;
		rest_len -= len;

		if (!rest_len || !rest)
			break;

		rx_iodev_skb(iod);
		iod->skb_recv =  NULL;

		alloc_size = min(rest_len, MAX_RXDATA_SIZE);
		skb = alloc_skb(alloc_size, GFP_ATOMIC);
		if (unlikely(!skb))
			return -ENOMEM;
		iod->skb_recv = skb;
	}

	return done_len;
}

static int rx_iodev_skb_raw(struct io_device *iod)
{
	int err;
	struct sk_buff *skb = iod->skb_recv;
	struct net_device *ndev;
	struct iphdr *ip_header;
	struct ethhdr *ehdr;
	const char source[ETH_ALEN] = SOURCE_MAC_ADDR;

	switch (iod->io_typ) {
	case IODEV_MISC:
		skb_queue_tail(&iod->sk_rx_q, iod->skb_recv);
		wake_up(&iod->wq);
		return 0;

	case IODEV_NET:
		ndev = iod->ndev;
		if (!ndev)
			return NET_RX_DROP;

		skb->dev = ndev;
		ndev->stats.rx_packets++;
		ndev->stats.rx_bytes += skb->len;

		/* check the version of IP */
		ip_header = (struct iphdr *)skb->data;
		if (ip_header->version == IP6VERSION)
			skb->protocol = htons(ETH_P_IPV6);
		else
			skb->protocol = htons(ETH_P_IP);

		if (iod->net_typ == UMTS_NETWORK) {
			skb_reset_mac_header(skb);
		} else {
			ehdr = (void *)skb_push(skb, sizeof(struct ethhdr));
			memcpy(ehdr->h_dest, ndev->dev_addr, ETH_ALEN);
			memcpy(ehdr->h_source, source, ETH_ALEN);
			ehdr->h_proto = skb->protocol;
			skb->ip_summed = CHECKSUM_NONE;
			skb_reset_mac_header(skb);

			skb_pull(skb, sizeof(struct ethhdr));
		}

		err = netif_rx_ni(skb);
		if (err != NET_RX_SUCCESS)
			dev_err(&ndev->dev, "rx error: %d\n", err);
		return err;

	default:
		pr_err("[MODEM_IF] wrong io_type : %d\n", iod->io_typ);
		return -EINVAL;
	}
}

static void rx_iodev_work(struct work_struct *work)
{
	int ret;
	struct sk_buff *skb;
	struct io_device *real_iod;
	struct io_device *iod = container_of(work, struct io_device,
				rx_work.work);

	skb = skb_dequeue(&iod->sk_rx_q);
	while (skb) {
		real_iod = *((struct io_device **)skb->cb);
		real_iod->skb_recv = skb;

		ret = rx_iodev_skb_raw(real_iod);
		if (ret == NET_RX_DROP) {
			pr_err("[MODEM_IF] %s: queue delayed work!\n",
								__func__);
			skb_queue_head(&iod->sk_rx_q, skb);
			schedule_delayed_work(&iod->rx_work,
						msecs_to_jiffies(20));
			break;
		} else if (ret < 0 && skb)
			dev_kfree_skb_any(skb);

		skb = skb_dequeue(&iod->sk_rx_q);
	}
}

/* hadling modem initiated loopback packet
 * packet path : Modem -> LINK -> IOD -> LINK -> MODEM
 */
static int rx_loopback(struct sk_buff *skb, struct io_device *iod)
{
	int headroom, tailroom;
	struct sk_buff *skb_new;
	struct raw_hdr raw_header;

	pr_debug("[MODEM_IF] %s: CP LB DATA RECEIVED: size=%d\n",
					__func__, skb->len);

	headroom = sizeof(raw_header) + sizeof(hdlc_start);
	tailroom = sizeof(hdlc_end);
	if (unlikely(skb_headroom(skb) < headroom) ||
			unlikely(skb_tailroom(skb) < tailroom)) {
		skb_new = skb_copy_expand(skb, headroom, tailroom, GFP_ATOMIC);
		/* skb_copy_expand success or not, free old skb from caller */
		dev_kfree_skb_any(skb);
		if (!skb_new)
			return -1;
	} else
		skb_new = skb;

	/* mark loopback header */
	raw_header.len = skb_new->len + sizeof(raw_header);
	raw_header.channel = CP_LOOPBACK_CHANNEL;
	raw_header.control = 0x03;

	/* fill header data and HDLC framing */
	memcpy(skb_push(skb_new, sizeof(raw_header)), &raw_header,
						sizeof(raw_header));
	memcpy(skb_push(skb_new, sizeof(hdlc_start)), hdlc_start,
						sizeof(hdlc_start));
	memcpy(skb_put(skb_new, sizeof(hdlc_end)), hdlc_end, sizeof(hdlc_end));

	pr_debug("[MODEM_IF] %s: CP LB DATA SEND WITH HDLC: size=%d\n",
					__func__, skb_new->len);
	iod->link->send(iod->link, iod, skb_new);
	return 0;
}

static int rx_multipdp(struct io_device *iod)
{
	u8 ch;
	struct raw_hdr *raw_header = (struct raw_hdr *)&iod->h_data.hdr;
	struct io_raw_devices *io_raw_devs =
				(struct io_raw_devices *)iod->private_data;
	struct io_device *real_iod;

	ch = raw_header->channel;

	if (ch == CP_LOOPBACK_CHANNEL)
		return rx_loopback(iod->skb_recv, iod);

	real_iod = io_raw_devs->raw_devices[ch];
	if (!real_iod) {
		pr_err("[MODEM_IF] %s: wrong channel %d\n", __func__, ch);
		return -1;
	}

	*((struct io_device **)iod->skb_recv->cb) = real_iod;
	skb_queue_tail(&iod->sk_rx_q, iod->skb_recv);
	pr_debug("sk_rx_qlen:%d\n", iod->sk_rx_q.qlen);

	schedule_delayed_work(&iod->rx_work, 0);
	return 0;
}

/* de-mux function draft */
static int rx_iodev_skb(struct io_device *iod)
{
	switch (iod->format) {
	case IPC_MULTI_RAW:
		return rx_multipdp(iod);

	case IPC_FMT:
	case IPC_RFS:
	default:
		skb_queue_tail(&iod->sk_rx_q, iod->skb_recv);

		pr_debug("[MODEM_IF] wake up fmt,rfs skb\n");
		wake_up(&iod->wq);
		return 0;
	}
}

static int rx_hdlc_packet(struct io_device *iod, const char *data,
		unsigned recv_size)
{
	unsigned rest = recv_size;
	char *buf = (char *)data;
	int err = 0;
	int len;

	if (rest <= 0)
		goto exit;

	pr_debug("[MODEM_IF] RX_SIZE=%d\n", rest);

	if (iod->h_data.flag_len)
		goto data_check;

next_frame:
	err = len = rx_hdlc_head_check(iod, buf, rest);
	if (err < 0)
		goto exit; /* buf++; rest--; goto next_frame; */
	pr_debug("[MODEM_IF] check len : %d, rest : %d (%d)\n", len, rest,
				__LINE__);

	buf += len;
	rest -= len;
	if (rest <= 0)
		goto exit;

data_check:
	err = len = rx_hdlc_data_check(iod, buf, rest);
	if (err < 0)
		goto exit;
	pr_debug("[MODEM_IF] check len : %d, rest : %d (%d)\n", len, rest,
				__LINE__);

	buf += len;
	rest -= len;

	if (!rest && iod->h_data.flag_len)
		return 0;
	else if (rest <= 0)
		goto exit;

	err = len = rx_hdlc_tail_check(buf);
	if (err < 0) {
		pr_err("[MODEM_IF] Wrong HDLC end: 0x%x(%s), rest: %d,"
			" recv_size:%d\n", *buf, iod->name, rest, recv_size);
		goto exit;
	}
	pr_debug("[MODEM_IF] check len : %d, rest : %d (%d)\n", len, rest,
				__LINE__);

	buf += len;
	rest -= len;
	if (rest < 0)
		goto exit;

	err = rx_iodev_skb(iod);
	if (err < 0)
		goto exit;

	/* initialize header & skb */
	iod->skb_recv = NULL;
	memset(&iod->h_data, 0x00, sizeof(struct header_data));

	if (rest)
		goto next_frame;

exit:
	/* free buffers. mipi-hsi re-use recv buf */
	if (rest < 0)
		err = -ERANGE;

	if (err < 0) {
		/* clear headers */
		memset(&iod->h_data, 0x00, sizeof(struct header_data));

		if (iod->skb_recv) {
			dev_kfree_skb_any(iod->skb_recv);
			iod->skb_recv = NULL;
		}
	}

	return err;
}

/* called from link device when a packet arrives for this io device */
static int io_dev_recv_data_from_link_dev(struct io_device *iod,
			const char *data, unsigned int len)
{
	struct sk_buff *skb;
	int err;

	switch (iod->format) {
	case IPC_FMT:
	case IPC_RAW:
	case IPC_RFS:
	case IPC_MULTI_RAW:
		if (iod->waketime)
			wake_lock_timeout(&iod->wakelock, iod->waketime);
		err = rx_hdlc_packet(iod, data, len);
		if (err < 0)
			pr_err("[MODEM_IF] fail process hdlc fram\n");
		return err;

	case IPC_CMD:
		/* TODO- handle flow control command from CP */
		return 0;

	case IPC_BOOT:
	case IPC_RAMDUMP:
		/* save packet to sk_buff */
		skb = alloc_skb(len, GFP_ATOMIC);
		if (!skb) {
			pr_err("[MODEM_IF] fail alloc skb (%d)\n", __LINE__);
			return -ENOMEM;
		}

		pr_debug("[MODEM_IF] boot len : %d\n", len);

		memcpy(skb_put(skb, len), data, len);
		skb_queue_tail(&iod->sk_rx_q, skb);
		pr_debug("[MODEM_IF] skb len : %d\n", skb->len);

		wake_up(&iod->wq);
		return len;

	default:
		return -EINVAL;
	}
}

/* inform the IO device that the modem is now online or offline or
 * crashing or whatever...
 */
static void io_dev_modem_state_changed(struct io_device *iod,
			enum modem_state state)
{
	iod->mc->phone_state = state;
	pr_info("[MODEM_IF] %s state changed: %s\n", iod->name,
				modem_state_name[state]);

	if ((state == STATE_CRASH_RESET) || (state == STATE_CRASH_EXIT))
		wake_up(&iod->wq);
}

/**
 * io_dev_sim_state_changed
 * @iod: IPC's io_device
 * @sim_online: SIM is online?
 */
static void io_dev_sim_state_changed(struct io_device *iod, bool sim_online)
{
	if (atomic_read(&iod->opened) == 0)
		pr_err("[MODEM_IF] %s iod is not opened: %s\n", __func__,
				iod->name);
	else if (iod->mc->sim_state.online == sim_online)
		pr_info("[MODEM_IF] %s sim state not changed.\n", __func__);
	else {
		iod->mc->sim_state.online = sim_online;
		iod->mc->sim_state.changed = true;

		pr_info("[MODEM_IF] %s sim state changed. (iod: %s, state: "
				"[online=%d, changed=%d])\n", __func__,
				iod->name, iod->mc->sim_state.online,
				iod->mc->sim_state.changed);
		wake_up(&iod->wq);
	}
}

static int misc_open(struct inode *inode, struct file *filp)
{
	struct io_device *iod = to_io_device(filp->private_data);
	filp->private_data = (void *)iod;

	pr_info("[MODEM_IF] misc_open : %s\n", iod->name);
	atomic_inc(&iod->opened);

	if (iod->link->init_comm)
		return iod->link->init_comm(iod->link, iod);
	return 0;
}

static int misc_release(struct inode *inode, struct file *filp)
{
	struct io_device *iod = (struct io_device *)filp->private_data;

	pr_info("[MODEM_IF] misc_release : %s\n", iod->name);
	atomic_dec(&iod->opened);

	if (iod->link->terminate_comm)
		iod->link->terminate_comm(iod->link, iod);

	skb_queue_purge(&iod->sk_rx_q);
	return 0;
}

static unsigned int misc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct io_device *iod = (struct io_device *)filp->private_data;

	poll_wait(filp, &iod->wq, wait);

	if ((!skb_queue_empty(&iod->sk_rx_q))
				&& (iod->mc->phone_state != STATE_OFFLINE))
		return POLLIN | POLLRDNORM;
	else if ((iod->mc->phone_state == STATE_CRASH_RESET) ||
			(iod->mc->phone_state == STATE_CRASH_EXIT) ||
			iod->mc->sim_state.changed)
		return POLLHUP;
	else
		return 0;
}

static long misc_ioctl(struct file *filp, unsigned int cmd, unsigned long _arg)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	int s_state;

	pr_debug("[MODEM_IF] misc_ioctl : 0x%x\n", cmd);

	switch (cmd) {
	case IOCTL_MODEM_ON:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_ON\n");
		return iod->mc->ops.modem_on(iod->mc);

	case IOCTL_MODEM_OFF:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_OFF\n");
		return iod->mc->ops.modem_off(iod->mc);

	case IOCTL_MODEM_RESET:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_RESET\n");
		return iod->mc->ops.modem_reset(iod->mc);

	case IOCTL_MODEM_FORCE_CRASH_EXIT:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_FORCE_CRASH_EXIT\n");
		if (iod->mc->ops.modem_force_crash_exit)
			return iod->mc->ops.modem_force_crash_exit(iod->mc);
		return 0;

	case IOCTL_MODEM_DUMP_RESET:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_FORCE_CRASH_EXIT\n");
		return iod->mc->ops.modem_dump_reset(iod->mc);

	case IOCTL_MODEM_BOOT_ON:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_BOOT_ON\n");
		return iod->mc->ops.modem_boot_on(iod->mc);

	case IOCTL_MODEM_BOOT_OFF:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_BOOT_OFF\n");
		return iod->mc->ops.modem_boot_off(iod->mc);

	/* TODO - will remove this command after ril updated */
	case IOCTL_MODEM_START:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_START\n");
		return 0;

	case IOCTL_MODEM_STATUS:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_STATUS\n");

		if (iod->mc->sim_state.changed) {
			s_state = iod->mc->sim_state.online ?
					STATE_SIM_ATTACH : STATE_SIM_DETACH;
			iod->mc->sim_state.changed = false;
			return s_state;
		}

		return iod->mc->phone_state;

	case IOCTL_MODEM_DUMP_START:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_DUMP_START\n");
		return iod->link->dump_start(iod->link, iod);

	case IOCTL_MODEM_DUMP_UPDATE:
		pr_debug("[MODEM_IF] misc_ioctl : IOCTL_MODEM_DUMP_UPDATE\n");
		return iod->link->dump_update(iod->link, iod, _arg);

	case IOCTL_MODEM_GOTA_START:
		pr_debug("[GOTA] misc_ioctl : IOCTL_MODEM_GOTA_START\n");
		return iod->link->gota_start(iod->link, iod);

	case IOCTL_MODEM_FW_UPDATE:
		pr_debug("[GOTA] misc_ioctl : IOCTL_MODEM_FW_UPDATE\n");
		return iod->link->modem_update(iod->link, iod, _arg);

	case IOCTL_MODEM_CP_UPLOAD:
		pr_err("[MODEM_IF] misc_ioctl : IOCTL_MODEM_CP_UPLOAD\n");
		panic("CP Crash");
		return 0;

	default:
		return -EINVAL;
	}
}

static ssize_t misc_write(struct file *filp, const char __user * buf,
			size_t count, loff_t *ppos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	int frame_len = 0;
	char frame_header_buf[sizeof(struct raw_hdr)];
	struct sk_buff *skb;
	int err;
	size_t tx_size;
	size_t data_size = count;
	size_t tx_done_len = 0;

send_remain_data:

	switch (iod->format) {
	case IPC_BOOT:
	case IPC_RAMDUMP:
		frame_len = data_size + get_header_size(iod);
		break;

	case IPC_RAW:
		if (data_size > MAX_MTU_TX_DATA_SIZE) {
			pr_debug("[MODEM_IF] TX size is over than MTU. "
				"data_size : %d\n", data_size);
			data_size = MAX_MTU_TX_DATA_SIZE;
		}
	default:
		frame_len = data_size + SIZE_OF_HDLC_START
				+ get_header_size(iod) + SIZE_OF_HDLC_END;
		break;
	}

	skb = alloc_skb(frame_len, GFP_KERNEL);
	if (!skb) {
		pr_err("[MODEM_IF] fail alloc skb (%d)\n", __LINE__);
		return -ENOMEM;
	}

	switch (iod->format) {
	case IPC_BOOT:
	case IPC_RAMDUMP:
		if (copy_from_user(skb_put(skb, data_size), buf,
						data_size) != 0) {
			dev_kfree_skb_any(skb);
			return -EFAULT;
		}
		break;

	case IPC_RFS:
		memcpy(skb_put(skb, SIZE_OF_HDLC_START), hdlc_start,
				SIZE_OF_HDLC_START);
		if (copy_from_user(skb_put(skb, data_size), buf,
						data_size) != 0) {
			dev_kfree_skb_any(skb);
			return -EFAULT;
		}
		memcpy(skb_put(skb, SIZE_OF_HDLC_END), hdlc_end,
					SIZE_OF_HDLC_END);
		break;

	default:
		memcpy(skb_put(skb, SIZE_OF_HDLC_START), hdlc_start,
				SIZE_OF_HDLC_START);
		memcpy(skb_put(skb, get_header_size(iod)),
			get_header(iod, data_size, frame_header_buf),
			get_header_size(iod));
		if (copy_from_user(skb_put(skb, data_size), buf + tx_done_len,
						data_size) != 0) {
			dev_kfree_skb_any(skb);
			return -EFAULT;
		}
		memcpy(skb_put(skb, SIZE_OF_HDLC_END), hdlc_end,
					SIZE_OF_HDLC_END);
		break;
	}
	tx_done_len += data_size;


	/* send data with sk_buff, link device will put sk_buff
	 * into the specific sk_buff_q and run work-q to send data
	 */
	tx_size = skb->len;
	err = iod->link->send(iod->link, iod, skb);
	if (err < 0)
		return err;

	if (err != tx_size)
		pr_err("[MODEM_IF] %s: WARNNING: wrong tx size: %s, format=%d "
			"count=%d, tx_size=%d, return_size=%d, data_size=%d\n",
			__func__, iod->name, iod->format, count, tx_size,
			err, data_size);

	if (tx_done_len < count) {
		pr_debug("[MODEM_IF] Send remain data. tx_done_len=%d, count=%d, "
			"data_size : %d\n", tx_done_len, count, data_size);
		data_size = count - tx_done_len;
		goto send_remain_data;
	}

	return count;
}

static ssize_t misc_read(struct file *filp, char *buf, size_t count,
			loff_t *f_pos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct sk_buff *skb;
	int pktsize = 0;

	skb = skb_dequeue(&iod->sk_rx_q);
	if (!skb) {
		printk_ratelimited(KERN_ERR "[MODEM_IF] no data from sk_rx_q, "
			"modem_state : %s(%s)\n",
			modem_state_name[iod->mc->phone_state], iod->name);
		return 0;
	}

	if (skb->len > count) {
		pr_err("[MODEM_IF] skb len is too big = %d,%d!(%d)\n",
				count, skb->len, __LINE__);
		dev_kfree_skb_any(skb);
		return -EIO;
	}
	pr_debug("[MODEM_IF] skb len : %d\n", skb->len);

	pktsize = skb->len;
	if (copy_to_user(buf, skb->data, pktsize) != 0) {
		dev_kfree_skb_any(skb);
		return -EIO;
	}
	dev_kfree_skb_any(skb);

	pr_debug("[MODEM_IF] copy to user : %d\n", pktsize);

	return pktsize;
}

static const struct file_operations misc_io_fops = {
	.owner = THIS_MODULE,
	.open = misc_open,
	.release = misc_release,
	.poll = misc_poll,
	.unlocked_ioctl = misc_ioctl,
	.write = misc_write,
	.read = misc_read,
};

static int vnet_open(struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);
	netif_start_queue(ndev);
	atomic_inc(&vnet->iod->opened);
	return 0;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);
	atomic_dec(&vnet->iod->opened);
	netif_stop_queue(ndev);
	return 0;
}

static int vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	int ret;
	struct raw_hdr hd;
	struct sk_buff *skb_new;
	struct vnet *vnet = netdev_priv(ndev);
	struct io_device *iod = vnet->iod;

	/* umts doesn't need to discard ethernet header */
	if (iod->net_typ != UMTS_NETWORK) {
		if (iod->id >= PSD_DATA_CHID_BEGIN &&
			iod->id <= PSD_DATA_CHID_END)
			skb_pull(skb, sizeof(struct ethhdr));
	}

	hd.len = skb->len + sizeof(hd);
	hd.control = 0;
	hd.channel = iod->id & 0x1F;

	skb_new = skb_copy_expand(skb, sizeof(hd) + sizeof(hdlc_start),
				sizeof(hdlc_end), GFP_ATOMIC);
	if (!skb_new) {
		dev_kfree_skb_any(skb);
		return -ENOMEM;
	}

	memcpy(skb_push(skb_new, sizeof(hd)), &hd, sizeof(hd));
	memcpy(skb_push(skb_new, sizeof(hdlc_start)), hdlc_start,
				sizeof(hdlc_start));
	memcpy(skb_put(skb_new, sizeof(hdlc_end)), hdlc_end, sizeof(hdlc_end));

	ret = iod->link->send(iod->link, iod, skb_new);
	if (ret < 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_BUSY;
	}

	ndev->stats.tx_packets++;
	ndev->stats.tx_bytes += skb->len;
	dev_kfree_skb_any(skb);

	return NETDEV_TX_OK;
}

static struct net_device_ops vnet_ops = {
	.ndo_open = vnet_open,
	.ndo_stop = vnet_stop,
	.ndo_start_xmit = vnet_xmit,
};

static void vnet_setup(struct net_device *ndev)
{
	ndev->netdev_ops = &vnet_ops;
	ndev->type = ARPHRD_PPP;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	ndev->addr_len = 0;
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}

static void vnet_setup_ether(struct net_device *ndev)
{
	ndev->netdev_ops = &vnet_ops;
	ndev->type = ARPHRD_ETHER;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST | IFF_SLAVE;
	ndev->addr_len = ETH_ALEN;
	random_ether_addr(ndev->dev_addr);
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}

int init_io_device(struct io_device *iod)
{
	int ret = 0;
	struct vnet *vnet;

	/* get modem state from modem control device */
	iod->modem_state_changed = io_dev_modem_state_changed;
	/* get data from link device */
	iod->recv = io_dev_recv_data_from_link_dev;
	/* to send SIM change event */
	iod->sim_state_changed = io_dev_sim_state_changed;

	INIT_LIST_HEAD(&iod->list);

	/* register misc or net drv */
	switch (iod->io_typ) {
	case IODEV_MISC:
		init_waitqueue_head(&iod->wq);
		skb_queue_head_init(&iod->sk_rx_q);
		INIT_DELAYED_WORK(&iod->rx_work, rx_iodev_work);

		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			pr_err("failed to register misc io device : %s\n",
						iod->name);

		break;

	case IODEV_NET:
		if (iod->net_typ == UMTS_NETWORK)
			iod->ndev = alloc_netdev(0, iod->name, vnet_setup);
		else
			iod->ndev = alloc_netdev(0, iod->name,
						vnet_setup_ether);
		if (!iod->ndev) {
			pr_err("failed to alloc netdev\n");
			return -ENOMEM;
		}

		ret = register_netdev(iod->ndev);
		if (ret)
			free_netdev(iod->ndev);

		pr_debug("%s: %d(iod:0x%p)\n", __func__, __LINE__, iod);
		vnet = netdev_priv(iod->ndev);
		pr_debug("%s: %d(vnet:0x%p)\n", __func__, __LINE__, vnet);
		vnet->iod = iod;

		break;

	case IODEV_DUMMY:
		skb_queue_head_init(&iod->sk_rx_q);
		INIT_DELAYED_WORK(&iod->rx_work, rx_iodev_work);

		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			pr_err("failed to register misc io device : %s\n",
						iod->name);
		ret = device_create_file(iod->miscdev.this_device,
				&attr_waketime);
		if (ret)
			pr_err("failed to create sysfs file : %s\n", iod->name);

		break;

	default:
		pr_err("wrong io_type : %d\n", iod->io_typ);
		return -EINVAL;
	}

	pr_debug("[MODEM_IF] %s(%d) : init_io_device() done : %d\n",
				iod->name, iod->io_typ, ret);
	return ret;
}


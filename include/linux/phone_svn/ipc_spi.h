/**
 * header for ipc_spi driver
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

#ifndef __IPC_SPI_H__
#define __IPC_SPI_H__

#include <linux/ioport.h>
#include <linux/types.h>

#define DRVNAME "onedram"

struct ipc_spi_platform_data {
	const char *name;
	unsigned gpio_ipc_mrdy;
	unsigned gpio_ipc_srdy;
	unsigned gpio_ipc_sub_mrdy;
	unsigned gpio_ipc_sub_srdy;

	void (*cfg_gpio)(void);
};

extern int onedram_register_handler(void (*handler)(u32, void *), void *data);
extern int onedram_unregister_handler(void (*handler)(u32, void *));
extern struct resource *onedram_request_region(resource_size_t start,
	resource_size_t size, const char *name);
extern void onedram_release_region(resource_size_t start,
	resource_size_t size);
extern int onedram_read_mailbox(u32 *);
extern int onedram_write_mailbox(u32);
extern int onedram_get_auth(u32 cmd);
extern int onedram_put_auth(int release);
extern int onedram_rel_sem(void);
extern int onedram_read_sem(void);
extern void onedram_get_vbase(void **);

extern int sec_bootmode;

#define ONEDRAM_GET_AUTH _IOW('o', 0x20, u32)
#define ONEDRAM_CP_CRASH _IO('o', 0x21)
#define ONEDRAM_REL_SEM _IO('o', 0x22)
#define ONEDRAM_SEMA_INIT _IO('o', 0x23)

#define ONEDRAM_REG_OFFSET	0xFFF800
#define ONEDRAM_REG_SIZE		0x800

#define MB_VALID					0x0080
#define MB_COMMAND				0x0040

#define MB_CMD(x)			(MB_VALID | MB_COMMAND | x)
#define MB_DATA(x)			(MB_VALID | x)

#define MBD_SEND_FMT			0x0002
#define MBD_SEND_RAW			0x0001
#define MBD_SEND_RFS				0x0100
#define MBD_REQ_ACK_FMT		0x0020
#define MBD_REQ_ACK_RAW		0x0010
#define MBD_REQ_ACK_RFS		0x0400
#define MBD_RES_ACK_FMT		0x0008
#define MBD_RES_ACK_RAW		0x0004
#define MBD_RES_ACK_RFS			0x0200


#define FMT_OUT 0x0FE000
#define FMT_IN		0x10E000
#define FMT_SZ		0x10000   /* 65536 bytes */

#define RAW_OUT 0x11E000
#define RAW_IN		0x21E000
#define RAW_SZ		0x100000 /* 1 MB */

#define RFS_OUT 0x31E000
#define RFS_IN		0x41E000
#define RFS_SZ		0x100000 /* 1 MB */

/* Send SPRD main image through SPI */
#define CP_VER_2
/* #define SPRD_TRANSLATE_PACKET */
#define SPRD_BLOCK_SIZE	32768

#ifdef CP_VER_2
enum image_type {
	MODEM_MAIN,
	MODEM_DSP,
	MODEM_NV,
	MODEM_EFS,
	MODEM_RUN,
};
#else
enum image_type {
	MODEM_KERNEL,
	MODEM_USER,
	MODEM_DSP,
	MODEM_NV,
	MODEM_RUN,
};
#endif

struct image_buf {
	unsigned int length;
	unsigned int offset;
	unsigned int address;
	unsigned char *buf;
};

struct sprd_image_buf {
	u8 *tx_b;
	u8 *rx_b;
	u8 *encoded_tx_b;
	u8 *decoded_rx_b;

	int tx_size;
	int rx_size;
	int encoded_tx_size;
	int decoded_rx_size;
};

/* CRC */
#define CRC_16_POLYNOMIAL		0x1021
#define CRC_16_L_POLYNOMIAL	0x8408
#define CRC_16_L_SEED	0xFFFF
#define CRC_TAB_SIZE	256	/* 2^CRC_TAB_BITS	   */
#define CRC_16_L_OK	0x0
#define HDLC_FLAG	0x7E
#define HDLC_ESCAPE	0x7D
#define HDLC_ESCAPE_MASK	0x20
#define CRC_CHECK_SIZE	0x02

#define M_32_SWAP(a) {					\
		u32 _tmp;				\
		_tmp = a;					\
		((u8 *)&a)[0] = ((u8 *)&_tmp)[3]; \
		((u8 *)&a)[1] = ((u8 *)&_tmp)[2]; \
		((u8 *)&a)[2] = ((u8 *)&_tmp)[1]; \
		((u8 *)&a)[3] = ((u8 *)&_tmp)[0]; \
		}

#define M_16_SWAP(a) {					\
		u16 _tmp;					\
		_tmp = (u16)a;			\
		((u8 *)&a)[0] = ((u8 *)&_tmp)[1];	\
		((u8 *)&a)[1] = ((u8 *)&_tmp)[0];	\
		}

#endif /* __IPC_SPI_H__ */

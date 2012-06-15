/**
 * Samsung Virtual Network driver using IpcSpi device
 *
 * Copyright (C) 2012 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPI_APP_H_
#define _SPI_APP_H_

#include "spi_main.h"
#include "spi_data.h"
#include "spi_os.h"

#define MB_VALID					0x0080
#define MB_DATA(x)				(MB_VALID | x)
#define MBD_SEND_FMT			0x0002
#define MBD_SEND_RAW			0x0001
#define MBD_SEND_RFS			0x0100

extern void spi_send_msg_to_app(void);

extern struct ipc_spi *ipc_spi;
extern void ipc_spi_make_data_interrupt(u32 cmd, struct ipc_spi *od);

#endif

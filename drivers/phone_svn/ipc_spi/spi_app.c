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

/*
 *
 *    spi_app.c
 *
 *
 *
 *	interface spi and app
 *
 *
 *
 *	This is MASTER side.
 *
 */


/*
 *
 *	Preprocessor by common
 *
*/

#include "spi_main.h"
#include "spi_app.h"
#include "spi_os.h"
#include "spi_data.h"


/**********************************************************
Prototype	void spi_send_msg ( void )
Type			function
Description	Dequeue a spi data from spi_data_queue_XXX_rx
			Unpack the spi data for ipc, raw or rfs data
			Send msg to other task until that all queues are empty
			CP use this functions for other task as below
			IPC : ipc_cmd_send_queue
			RAW : data_send_queue
			RFS : rfs_send_queue
Param input	(none)
Return value	(none)
***********************************************************/
void spi_send_msg_to_app(void)
{
	u32 int_cmd;
	struct ipc_spi *od_spi;

	od_spi = ipc_spi;

	if (spi_data_queue_is_empty(SPI_DATA_QUEUE_TYPE_IPC_RX) == 0) {
		int_cmd = MB_DATA(MBD_SEND_FMT);
		ipc_spi_make_data_interrupt(int_cmd, od_spi);
	}

	if (spi_data_queue_is_empty(SPI_DATA_QUEUE_TYPE_RAW_RX) == 0) {
		int_cmd = MB_DATA(MBD_SEND_RAW);
		ipc_spi_make_data_interrupt(int_cmd, od_spi);
	}

	if (spi_data_queue_is_empty(SPI_DATA_QUEUE_TYPE_RFS_RX) == 0) {
		int_cmd = MB_DATA(MBD_SEND_RFS);
		ipc_spi_make_data_interrupt(int_cmd, od_spi);
	}
}


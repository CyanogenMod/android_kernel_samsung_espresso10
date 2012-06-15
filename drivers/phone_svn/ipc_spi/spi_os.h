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

#ifndef __SPI_OS_H__
#define __SPI_OS_H__

#include "spi_main.h"

#include <linux/kernel.h>
#include <linux/workqueue.h>

#define SPI_OS_TRACE_DUMP_PER_LINE 16

struct spi_work {
	struct work_struct work;
	int signal_code;
};

struct spi_os_msg {
	unsigned int signal_code;
	unsigned int data_length;
	void *data;
};

/* Memory */
extern void *spi_os_malloc(unsigned int length);
extern void *spi_os_vmalloc(unsigned int length);
extern int      spi_os_free(void *addr);
extern int		spi_os_vfree(void *addr);
extern int      spi_os_memcpy(void *dest, void *src, unsigned int length);
extern void *spi_os_memset(void *addr, int value, unsigned int length);

/* Timer */
extern void spi_os_sleep(unsigned long msec);
extern void spi_os_loop_delay(unsigned long cnt);

/* Log */
extern unsigned long spi_os_get_tick(void);
extern void spi_os_trace_dump_low(char *name, void *data, int length);
#endif

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

/**************************************************************
	spi_os.c

	adapt os api and spi api

	This is MATER side.
***************************************************************/

/**************************************************************
	Preprocessor by common
***************************************************************/

#include "spi_main.h"
#include "spi_os.h"


/**************************************************************
	Preprocessor by platform
	(Android)
***************************************************************/

#include <linux/vmalloc.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/in.h>
#include <linux/time.h>

/**********************************************************
Prototype		void * spi_os_malloc ( unsigned int length )

Type		function

Description	allocate memory

Param input	length	: length of size

Return value	0		: fail
			(other)	: address of memory allocated
***********************************************************/
void *spi_os_malloc(unsigned int length)
{
	if (length == 0) {
		pr_err("[SPI] ERROR : spi_os_malloc fail : len 0\n");
		return 0;
	} else if (length > PAGE_SIZE * 32)
		return 0;

	return kmalloc(length, GFP_ATOMIC);
}


/*====================================
Prototype		void * spi_os_vmalloc ( unsigned int length )

Type		function

Description	allocate memory with vmalloc

Param input	length	: length of size

Return value	0		: fail
			(other)	: address of memory allocated
====================================*/
void *spi_os_vmalloc(unsigned int length)
{
	if (length == 0) {
		pr_err("spi_os_malloc fail : length is 0\n");
		return 0;
	}
	return vmalloc(length);
}


/**********************************************************
Prototype		int spi_os_free ( void * addr )

Type		function

Description	free memory

Param input	addr	: address of memory to be released

Return value	0	: fail
			1	: success
***********************************************************/
int spi_os_free(void *addr)
{
	if (addr == 0) {
		pr_err("[SPI] ERROR : spi_os_free fail : addr is 0\n");
		return 0;
	}

	kfree(addr);
	return 1;
}


/**********************************************************
Prototype		int spi_os_vfree ( void * addr )

Type		function

Description	free memory with vfree

Param input	addr	: address of memory to be released

Return value	0	: fail
			1	: success
***********************************************************/
int spi_os_vfree(void *addr)
{
	if (addr == 0) {
		pr_err("spi_os_free fail : addr is 0\n");
		return 0;
	}

	vfree(addr);

	return 1;
}


/**********************************************************
Prototype	int spi_os_memcpy ( void * dest, void * src, unsigned int length )

Type		function

Description	copy memory

Param input	dest	: address of memory to be save
			src	: address of memory to copy
			length	: length of memory to copy

Return value	0	: fail
			1	: success
***********************************************************/
int spi_os_memcpy(void *dest, void *src, unsigned int length)
{
	if (dest == 0 || src == 0 || length == 0) {
		pr_err("[SPI] ERROR : spi_os_memcpy fail\n");
		return 0;
	}

	memcpy(dest, src, length);

	return 1;
}


/**********************************************************
Prototype	void * spi_os_memset ( void * addr, int value, unsigned int length )

Type		function

Description	set memory as parameter

Param input	addr	: address of memory to be set
			value	: value to set
			length	: length of memory to be set

Return value	(none)
***********************************************************/
void *spi_os_memset(void *addr, int value, unsigned int length)
{
	if (addr == 0 || length == 0) {
		pr_err("[SPI] ERROR : spi_os_memset fail\n");
		return 0;
	}

	return memset(addr, value, length);
}


/**********************************************************
Prototype		void spi_os_sleep ( unsigned long msec )

Type		function

Description	sleep os

Param input	msec	: time to sleep

Return value	(none)
***********************************************************/
void spi_os_sleep(unsigned long msec)
{
	if (msec == 0) {
		pr_err("[SPI] ERROR : spi_os_sleep fail\n");
		return;
	} else if (msec < 20)
		usleep_range(msec * 1000, msec * 1000);
	else
		msleep(msec);
}


/**********************************************************
Prototype		void spi_os_loop_delay ( unsigned long cnt )

Type		function

Description	delay task with loop

Param input	cnt	: delay count

Return value	(none)
***********************************************************/
void spi_os_loop_delay(unsigned long cnt)
{
	unsigned int timeout;
	timeout = 0;
	while (++timeout < cnt)
		;
}


/**********************************************************
Prototype		unsigned long spi_os_get_tick (void)

Type		function

Description	get system tick

Param input	(none)

Return value	system tick

***********************************************************/
unsigned long spi_os_get_tick(void)
{
	return jiffies_to_msecs(jiffies);
}


/**********************************************************
Prototype		void spi_os_trace_dump (char * name, void * data, int length)

Description	print buffer value by hex code as SPI_OS_TRACE

			this function print only 16 byte

Param input	name	: print name
			data		: buffer for print
			length	: print length

Return value	(none)
***********************************************************/

void spi_os_trace_dump_low(char *name, void *data, int length)
{
	int i;
	char buf[SPI_OS_TRACE_DUMP_PER_LINE * 3 + 1] = {0,};
	char *pdata = NULL;
	char ch;

	pr_info("[SPI] spi_os_trace_dump_low (%s length[%d])\n",
		name, length);

	spi_os_memset(buf, 0x00, sizeof(buf));

	if (length > SPI_OS_TRACE_DUMP_PER_LINE)
		length = SPI_OS_TRACE_DUMP_PER_LINE;

	pdata = data;
	for (i = 0 ; i < length ; i++) {
		ch = (*pdata&0xF0)>>4;
		buf[(i%SPI_OS_TRACE_DUMP_PER_LINE)*3] =
			((ch > 9) ? (ch-10 + 'A') : (ch +  '0'));
		ch = (*pdata&0x0F);
		buf[(i%SPI_OS_TRACE_DUMP_PER_LINE)*3+1] =
			((ch > 9) ? (ch-10 + 'A') : (ch +  '0'));
		buf[(i%SPI_OS_TRACE_DUMP_PER_LINE)*3+2] = 0x20;
		pdata++;
	}

	if (buf[0] != '\0')
		pr_err("%s\n", buf);
}

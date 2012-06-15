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

	spi_dev.c

	adapt device api and spi api

	This is MASTER side.

***************************************************************/


/**************************************************************

	Preprocessor by common

***************************************************************/

#include "spi_main.h"
#include "spi_dev.h"
#include "spi_os.h"



/**************************************************************

	Preprocessor by platform
	(OMAP4430 && MSM7X30)

***************************************************************/

#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/phone_svn/ipc_spi.h>

/**************************************************************

	Definition of Variables and Functions by common

***************************************************************/

static int spi_dev_gpio_mrdy;
static int spi_dev_gpio_srdy;
static int spi_dev_gpio_submrdy;
static int spi_dev_gpio_subsrdy;

static int spi_is_restart;


/**********************************************************

Prototype		void  spi_set_is_restart(int spi_set_restart)

Type		function

Description	set static spi_is_restart value

Param input	set value

***********************************************************/
void  spi_set_is_restart(int spi_set_restart)
{
	spi_is_restart = spi_set_restart;
}


/**********************************************************

Prototype		int spi_get_is_restart(void)

Type		function

Description	return static spi_is_restart value

Return value   spi_is_restart variable value

***********************************************************/
int spi_get_is_restart(void)
{
	return spi_is_restart;
}


/**********************************************************

Prototype		void spi_dev_init ( void * data)

Type		function

Description	init spi gpio info

Param input	pdata

***********************************************************/
void spi_dev_init(void *data)
{
	struct ipc_spi_platform_data *pdata;


	pr_info("[SPI] spi_dev_init\n");


	pdata = (struct ipc_spi_platform_data *)data;

	spi_dev_gpio_mrdy = (int) pdata->gpio_ipc_mrdy;
	spi_dev_gpio_srdy = (int) pdata->gpio_ipc_srdy;
	spi_dev_gpio_submrdy = (int) pdata->gpio_ipc_sub_mrdy;
	spi_dev_gpio_subsrdy = (int) pdata->gpio_ipc_sub_srdy;

	spi_dev_set_gpio(SPI_DEV_GPIOPIN_MRDY, SPI_DEV_GPIOLEVEL_LOW);
	if (spi_is_restart == 0)
		spi_dev_set_gpio(SPI_DEV_GPIOPIN_SUBMRDY,
			SPI_DEV_GPIOLEVEL_LOW);
}


/**********************************************************

Prototype		void spi_dev_destroy( void )

Type		function

Description	unregister irq handler

***********************************************************/

void spi_dev_destroy(void)
{
	spi_dev_unreigster_irq_handler(spi_dev_gpio_srdy, 0);
	spi_dev_unreigster_irq_handler(spi_dev_gpio_subsrdy, 0);
}



/**********************************************************

Prototype		int spi_dev_send ( void * buf, unsigned int length )

Type		function

Description	starting data send by DMA(CP side)
			starting send clock for data switching(AP side)

Param input	buf : data for send
			length : data size. this lengt must be fixed

Return value	0 : success
			(others) : error cause

***********************************************************/

int spi_dev_send(void *buf, unsigned int length)
{
	return ipc_spi_tx_rx_sync(buf, 0, length);
}


/**********************************************************

Prototype		int spi_dev_receive ( void * buf, unsigned int length )

Type		function

Description	starting data receive by DMA(CP side)
			starting send clock for data switching(AP side)

Param input	buf : buffer for saving data
			length : data size. this lengt must be fixed

Return value	0 : success
			(others) : error cause

***********************************************************/

int spi_dev_receive(void *buf, unsigned int length)
{
	return ipc_spi_tx_rx_sync(0, buf, length);
}


/**********************************************************

Prototype		void spi_dev_set_gpio

Type		function

Description	set gpio pin state

Param input	gpio_pin : gpio pin index
			value : SPI_DEV_GPIOLEVEL_HIGH for raising pin state up
			: SPI_DEV_GPIOLEVEL_LOW for getting pin state down

Return value	(none)

***********************************************************/
void spi_dev_set_gpio(enum SPI_DEV_GPIOPIN_T gpio_pin,
		enum SPI_DEV_GPIOLEVEL_T value)
{
	int gpio_id;

	switch (gpio_pin) {
	case SPI_DEV_GPIOPIN_MRDY:
		gpio_id = spi_dev_gpio_mrdy;
		break;
	case SPI_DEV_GPIOPIN_SUBMRDY:
		gpio_id = spi_dev_gpio_submrdy;
		break;
	case SPI_DEV_GPIOPIN_SRDY:
		gpio_id = spi_dev_gpio_srdy;
		break;
	case SPI_DEV_GPIOPIN_SUBSRDY:
		gpio_id = spi_dev_gpio_subsrdy;
		break;
	default:
		pr_info("unmatched gpio_pin. forced set mrdy\n");
		gpio_id = spi_dev_gpio_mrdy;
		break;
	}

	pr_debug("%s gpio_id =[%d], value =[%d]\n",
		"[SPI] spi_dev_set_gpio :", gpio_id, (int) value);

	gpio_set_value((unsigned int) gpio_id,
		(value == SPI_DEV_GPIOLEVEL_HIGH ? 1 : 0));
}



/**********************************************************

Prototype		int spi_dev_get_gpio(enum SPI_DEV_GPIOPIN_T gpio_pin)

Type		function

Description	get gpio pin state

Param input	gpio_pin : gpio pin index

Return value	SPI_DEV_GPIOLEVEL

***********************************************************/
int spi_dev_get_gpio(enum SPI_DEV_GPIOPIN_T gpio_pin)
{
	int value = SPI_DEV_GPIOLEVEL_LOW;
	int level;
	int gpio_id;

	switch (gpio_pin) {
	case SPI_DEV_GPIOPIN_MRDY:
		gpio_id = spi_dev_gpio_mrdy;
		break;
	case SPI_DEV_GPIOPIN_SUBMRDY:
		gpio_id = spi_dev_gpio_submrdy;
		break;
	case SPI_DEV_GPIOPIN_SRDY:
		gpio_id = spi_dev_gpio_srdy;
		break;
	case SPI_DEV_GPIOPIN_SUBSRDY:
		gpio_id = spi_dev_gpio_subsrdy;
		break;
	default:
		pr_info("unmatched gpio_pin. forced set mrdy\n");
		gpio_id = spi_dev_gpio_mrdy;
		break;
	}

	level = gpio_get_value((unsigned int) gpio_id);

	if (level == 0)
		value = SPI_DEV_GPIOLEVEL_LOW;
	else if (level == 1)
		value = SPI_DEV_GPIOLEVEL_HIGH;

	return value;
}


/**********************************************************

Prototype		int spi_dev_reigster_irq_handler

Type		function

Description	regist irq callback function to each gpio interrupt

Param input	gpio_pin	: gpio pin index
			tigger	: interrupt detection mode
			handler	: interrupt handler function
			name : register name

Return value	0	: fail
			1	: success

***********************************************************/
int spi_dev_reigster_irq_handler(enum SPI_DEV_GPIOPIN_T gpio_pin,
	enum SPI_DEV_IRQ_TRIGGER_T trigger,
	SPI_DEV_IRQ_HANDLER_T handler,
	char *name, void *data)
{
	int value;
	int irq;
	int _trigger = IRQF_TRIGGER_NONE;
	int gpio_id;

	switch (gpio_pin) {
	case SPI_DEV_GPIOPIN_MRDY:
		gpio_id = spi_dev_gpio_mrdy;
		break;
	case SPI_DEV_GPIOPIN_SUBMRDY:
		gpio_id = spi_dev_gpio_submrdy;
		break;
	case SPI_DEV_GPIOPIN_SRDY:
		gpio_id = spi_dev_gpio_srdy;
		break;
	case SPI_DEV_GPIOPIN_SUBSRDY:
		gpio_id = spi_dev_gpio_subsrdy;
		break;
	default:
		pr_info("unmatched gpio_pin. forced set mrdy\n");
		gpio_id = spi_dev_gpio_mrdy;
		break;
	}

	switch (trigger) {
	case SPI_DEV_IRQ_TRIGGER_RISING:
		_trigger = IRQF_TRIGGER_RISING;
		break;
	case SPI_DEV_IRQ_TRIGGER_FALLING:
		_trigger = IRQF_TRIGGER_FALLING;
		break;
	default:
		_trigger = IRQF_TRIGGER_NONE;
		break;
	}

	irq = gpio_to_irq(gpio_id);
	value = request_irq(irq, handler, _trigger, name, data);
	if (value != 0) {
		pr_err("spi_dev_reigster_irq_handler: regist fail(%d)",
			value);
		return 0;
	}
	enable_irq_wake(irq);

	return 1;
}


/**********************************************************

void spi_dev_unreigster_irq_handler (int gpio_id, void * data)

Type		function

Description	unregister irq hanger by gpio api

Param input	gpio_id	: gpio pin id
			data	: param

Return value	(none)

***********************************************************/

void spi_dev_unreigster_irq_handler(int gpio_id, void *data)
{
	free_irq(gpio_to_irq(gpio_id), data);
}

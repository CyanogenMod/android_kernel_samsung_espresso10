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

#ifndef _SPI_DEV_H_
#define _SPI_DEV_H_

#include "spi_main.h"

#include <linux/interrupt.h>

#define SPI_DEV_MAX_PACKET_SIZE (2048 * 6)

enum SPI_DEV_SYNC_STATE_T {
	SPI_DEV_SYNC_OFF = 0,
	SPI_DEV_SYNC_ON
};

enum SPI_DEV_GPIOPIN_T {
	SPI_DEV_GPIOPIN_MRDY = 0,
	SPI_DEV_GPIOPIN_SUBMRDY,
	SPI_DEV_GPIOPIN_SRDY,
	SPI_DEV_GPIOPIN_SUBSRDY
};

enum SPI_DEV_GPIOLEVEL_T {
	SPI_DEV_GPIOLEVEL_LOW	= 0,
	SPI_DEV_GPIOLEVEL_HIGH
};

enum SPI_DEV_IRQ_TRIGGER_T {
	SPI_DEV_IRQ_TRIGGER_RISING	= 1,
	SPI_DEV_IRQ_TRIGGER_FALLING
};

typedef irqreturn_t (*SPI_DEV_IRQ_HANDLER_T)(int, void *);
#define SPI_DEV_IRQ_HANDLER(X) irqreturn_t X(int irq, void *data)

extern void spi_set_is_restart(int spi_set_restart);
extern int spi_get_is_restart(void);

extern void spi_dev_init(void *data);
extern void spi_dev_destroy(void);
extern int spi_dev_send(void *buf, unsigned int length);
extern int spi_dev_receive(void *buf, unsigned int length);
extern void spi_dev_set_gpio(enum SPI_DEV_GPIOPIN_T gpio_pin,
	enum SPI_DEV_GPIOLEVEL_T value);
extern int spi_dev_get_gpio(enum SPI_DEV_GPIOPIN_T gpio_pin);
extern int spi_dev_reigster_irq_handler(enum SPI_DEV_GPIOPIN_T gpio_pin,
	enum SPI_DEV_IRQ_TRIGGER_T trigger,
	SPI_DEV_IRQ_HANDLER_T handler,
	char *name, void *data);
extern void spi_dev_unreigster_irq_handler(int gpio_id, void *data);

extern int ipc_spi_tx_rx_sync(u8 *tx_d, u8 *rx_d, unsigned len);

#endif

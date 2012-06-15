/*
 $License:
    Copyright (C) 2010 InvenSense Corporation, All Rights Reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
  $
 */

#ifndef __MSSL_H__
#define __MSSL_H__

#include "mltypes.h"
#include "mpu_v333.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------ */
/* - Defines. - */
/* ------------ */
/* acceleration data */
struct acc_data {
	s16 x;
	s16 y;
	s16 z;
};


/*
 * NOTE : to properly support Yamaha compass reads,
 * the max transfer size should be at least 9 B.
 * Length in bytes, typically a power of 2 >= 2
 */
#define SERIAL_MAX_TRANSFER_SIZE 128

/* ---------------------- */
/* - Types definitions. - */
/* ---------------------- */

/* --------------------- */
/* - Function p-types. - */
/* --------------------- */

	unsigned char MLSLSerialOpen(char const *port,
				void **sl_handle);
	unsigned char MLSLSerialReset(void *sl_handle);
	unsigned char MLSLSerialClose(void *sl_handle);

	unsigned char MLSLSerialWriteSingle(void *sl_handle,
				       unsigned char slaveAddr,
				       unsigned char registerAddr,
				       unsigned char data);

	unsigned char MLSLSerialRead(void *sl_handle,
				unsigned char slaveAddr,
				unsigned char registerAddr,
				unsigned short length,
				unsigned char *data);

	unsigned char MLSLSerialWrite(void *sl_handle,
				 unsigned char slaveAddr,
				 unsigned short length,
				 unsigned char const *data);

	unsigned char MLSLSerialReadMem(void *sl_handle,
				   unsigned char slaveAddr,
				   unsigned short memAddr,
				   unsigned short length,
				   unsigned char *data);

	unsigned char MLSLSerialWriteMem(void *sl_handle,
				    unsigned char slaveAddr,
				    unsigned short memAddr,
				    unsigned short length,
				    unsigned char const *data);

	unsigned char MLSLSerialReadFifo(void *sl_handle,
				    unsigned char slaveAddr,
				    unsigned short length,
				    unsigned char *data);

	unsigned char MLSLSerialWriteFifo(void *sl_handle,
				     unsigned char slaveAddr,
				     unsigned short length,
				     unsigned char const *data);

	unsigned char MLSLWriteCal(unsigned char *cal, unsigned int len);
	unsigned char MLSLReadCal(unsigned char *cal, unsigned int len);
	unsigned char MLSLGetCalLength(unsigned int *len);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
#endif				/* MLSL_H */

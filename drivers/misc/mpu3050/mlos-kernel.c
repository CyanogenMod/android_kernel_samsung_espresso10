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
/**
 * @defgroup
 * @brief
 *
 * @{
 * @file     mlos-kernel.c
 * @brief
 *
 *
 */

#include "mlos.h"
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/time.h>

void *MLOSMalloc(unsigned int numBytes)
{
	return kmalloc(numBytes, GFP_KERNEL);
}

unsigned char MLOSFree(void *ptr)
{
	kfree(ptr);
	return ML_SUCCESS;
}

unsigned char MLOSCreateMutex(unsigned int *mutex)
{
	/* @todo implement if needed */
	return ML_ERROR_FEATURE_NOT_IMPLEMENTED;
}

unsigned char MLOSLockMutex(unsigned int mutex)
{
	/* @todo implement if needed */
	return ML_ERROR_FEATURE_NOT_IMPLEMENTED;
}

unsigned char MLOSUnlockMutex(unsigned int mutex)
{
	/* @todo implement if needed */
	return ML_ERROR_FEATURE_NOT_IMPLEMENTED;
}

unsigned char MLOSDestroyMutex(unsigned int handle)
{
	/* @todo implement if needed */
	return ML_ERROR_FEATURE_NOT_IMPLEMENTED;
}

unsigned int *MLOSFOpen(char *filename)
{
	/* @todo implement if needed */
	return NULL;
}

void MLOSFClose(unsigned int *fp)
{
	/* @todo implement if needed */
}

void MLOSSleep(int mSecs)
{
	msleep(mSecs);
}

unsigned long MLOSGetTickCount(void)
{
	struct timespec now;

	getnstimeofday(&now);

	return (long)(now.tv_sec * 1000L + now.tv_nsec / 1000000L);
}

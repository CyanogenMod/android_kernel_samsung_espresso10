/*
 * Copyright (c) 2010-2012 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef __YAS_CFG_H__
#define __YAS_CFG_H__

/*----------------------------------------------------------------------------*/
/*                     Accelerometer Filter Configuration                     */
/*----------------------------------------------------------------------------*/

#define YAS_ACC_DEFAULT_FILTER_THRESH       (76612)


/*----------------------------------------------------------------------------*/
/*                    Geomagnetic Calibration Configuration                   */
/*----------------------------------------------------------------------------*/

#define YAS_DEFAULT_MAGCALIB_THRESHOLD      (1)
#define YAS_DEFAULT_MAGCALIB_DISTORTION     (15)
#define YAS_MAGCALIB_SHAPE_NUM              (2)
#define YAS_MAG_MANUAL_OFFSET


/*----------------------------------------------------------------------------*/
/*                      Geomagnetic Filter Configuration                      */
/*----------------------------------------------------------------------------*/

#define YAS_MAG_MAX_FILTER_LEN              (30)
#define YAS_MAG_DEFAULT_FILTER_NOISE_X      (144)	/* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_NOISE_Y      (144)	/* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_NOISE_Z      (144)	/* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_LEN          (20)
#define YAS_MAG_DEFAULT_FILTER_THRESH       (100)

#endif

/* arch/arm/mach-omap2/sec_logbuf.h
 *
 * Copyright (C) 2010-2011 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SEC_LOG_BUF_H__
#define __SEC_LOG_BUF_H__

struct sec_log_buf {
	unsigned int *flag;
	unsigned int *count;
	char *data;
	bool enable;
};

#endif /* __SEC_LOG_BUF_H__ */

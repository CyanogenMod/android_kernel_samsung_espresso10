/* arch/arm/mach-omap2/sec_logger.
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
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

#ifndef __SEC_LOGGER_H__
#define __SEC_LOGGER_H__

/*
 * Note: Add parts from drivers/staging/android/logger.h,
 * it's still needed here after removing Android logger driver
 *
 * The structure for version 2 of the logger_entry ABI.
 * This structure is returned to userspace if ioctl(LOGGER_SET_VERSION)
 * is called with version >= 2
 */
struct logger_entry {
	__u16		len;		/* length of the payload */
	__u16		hdr_size;	/* sizeof(struct logger_entry_v2) */
	__s32		pid;		/* generating process's pid */
	__s32		tid;		/* generating process's tid */
	__s32		sec;		/* seconds since Epoch */
	__s32		nsec;		/* nanoseconds */
	uid_t		euid;		/* effective UID of logger */
	char		msg[0];		/* the entry's payload */
};

#define LOGGER_LOG_SYSTEM	"log_system"	/* system/framework messages */
#define LOGGER_LOG_MAIN		"log_main"	/* everything else */

/*
 * drivers/staging/android/logger.h parts end here
 */

#if defined(CONFIG_SAMSUNG_USE_LOGGER_ADDON)

#if defined(CONFIG_SAMSUNG_PRINT_PLATFORM_LOG)
extern int sec_logger_add_log_ram_console(void *logp, size_t orig);
#else
#define sec_logger_add_log_ram_console(logp, orig)
#endif /* CONFIG_SAMSUNG_PRINT_PLATFORM_LOG */

extern void sec_logger_update_buffer(const char *log_str, int count);

extern void sec_logger_print_buffer(void);

#else /* CONFIG_SAMSUNG_USE_LOGGER_ADDON */

#define sec_logger_add_log_ram_console(logp, orig)
#define sec_logger_update_buffer(log_str, count)
#define sec_logger_print_buffer()

#endif /* CONFIG_SAMSUNG_USE_LOGGER_ADDON */

#endif /* __SEC_LOGGER_H__ */

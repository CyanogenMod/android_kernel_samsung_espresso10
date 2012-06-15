/**
 * arch/arm/mach-omap2/include/mach/sec_param.h
 *
 * Copyright (C) 2010-2011, Samsung Electronics, Co., Ltd. All Rights Reserved.
 *  Written by System S/W Group, Open OS S/W R&D Team,
 *  Mobile Communication Division.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/**
 * Project Name : OMAP-Samsung Linux Kernel for Android
 *
 * Project Description :
 *
 * Comments : tabstop = 8, shiftwidth = 8, noexpandtab
 */

/**
 * File Name : sec_param.h
 *
 * File Description :
 *
 * Author : System Platform 2
 * Dept : System S/W Group (Open OS S/W R&D Team)
 * Created : 11/Mar/2011
 * Version : Baby-Raccoon
 */

#ifndef __SEC_PARAM_H__
#define __SEC_PARAM_H__

#include "sec_common.h"

#define PARAM_MAGIC			0x72726624
#define PARAM_VERSION			0x13	/* Rev 1.3 */
#define PARAM_STRING_SIZE		1024	/* 1024 Characters */

#define MAX_PARAM			20
#define MAX_STRING_PARAM		5

/* Default Parameter Values */

#define SERIAL_SPEED			7	/* Baudrate */
#define LCD_LEVEL			0x061	/* Backlight Level */
#define BOOT_DELAY			0	/* Boot Wait Time */
#define LOAD_RAMDISK			0	/* Enable Ramdisk Loading */
#define SWITCH_SEL			1	/* Switch Setting
						   (UART[1], USB[0]) */
#define PHONE_DEBUG_ON			0	/* Enable Phone Debug Mode */
#define LCD_DIM_LEVEL			0x011	/* Backlight Dimming Level */
#define LCD_DIM_TIME			0
#define MELODY_MODE			0	/* Melody Mode */
#define REBOOT_MODE			0	/* Reboot Mode */
#define NATION_SEL			0	/* Language Configuration */
#define LANGUAGE_SEL			0
#define SET_DEFAULT_PARAM		0	/* Set Param to Default */
#define VERSION_LINE			"I8315XXIE00"	/* Set Image Info */
#define COMMAND_LINE			"console=ttySAC2,115200"
#define BOOT_VERSION			" version=Sbl(1.0.0) "

enum {
	__SERIAL_SPEED,
	__LOAD_RAMDISK,
	__BOOT_DELAY,
	__LCD_LEVEL,
	__SWITCH_SEL,
	__PHONE_DEBUG_ON,
	__LCD_DIM_LEVEL,
	__LCD_DIM_TIME,
	__MELODY_MODE,
	__REBOOT_MODE,
	__NATION_SEL,
	__LANGUAGE_SEL,
	__SET_DEFAULT_PARAM,
	__DEBUG_BLOCKPOPUP,
	__PARAM_INT_14,		/* Reserved. */
	__VERSION,
	__CMDLINE,
	__PARAM_STR_2,
	__DEBUG_LEVEL,
	__PARAM_STR_4		/* Reserved. */
};

struct _param_int_t {
	unsigned int ident;
	int value;
};

struct _param_str_t {
	unsigned int ident;
	char value[PARAM_STRING_SIZE];
};

struct _status_t {
	int param_magic;
	int param_version;
	struct _param_int_t param_list[MAX_PARAM - MAX_STRING_PARAM];
	struct _param_str_t param_str_list[MAX_STRING_PARAM];
};

extern void (*sec_set_param_value) (int idx, void *value);
extern void (*sec_get_param_value) (int idx, void *value);

#define USB_SEL_MASK			(1 << 0)
#define UART_SEL_MASK			(1 << 1)

#endif /* __SEC_PARAM_H__ */

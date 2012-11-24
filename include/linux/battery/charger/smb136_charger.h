/*
 * smb136_charger.h
 * Samsung SMB136 Charger Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SMB136_CHARGER_H
#define __SMB136_CHARGER_H __FILE__

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#define SEC_CHARGER_I2C_SLAVEADDR	(0x9A >> 1)

/* Register define */
#define SMB136_CHARGE_CURRENT			0x00
#define SMB136_INPUT_CURRENTLIMIT		0x01
#define SMB136_FLOAT_VOLTAGE			0x02
#define SMB136_CHARGE_CONTROL_A			0x03
#define SMB136_CHARGE_CONTROL_B                 0x04
#define SMB136_PIN_ENABLE_CONTROL		0x05
#define SMB136_OTG_CONTROL                      0x06
#define SMB136_FAULT				0x07
#define SMB136_TEMPERATURE			0x08
#define SMB136_SAFTY				0x09
#define SMB136_VSYS				0x0A
#define SMB136_I2C_ADDRESS			0x0B
#define SMB136_FAULT_INTERRUPT			0x0C
#define SMB136_STATUS_INTERRUPT			0x0D
#define SMB136_I2C_BUS_SLAVE_ADDR		0x0E

#define SMB136_IRQ_RESET			0x30
#define SMB136_COMMAND_A			0x31
#define SMB136_STATUS_D				0x35
#define SMB136_STATUS_E				0x36
#define SMB136_STATUS_F				0x37
#define SMB136_STATUS_H				0x39
#define SMB136_DEVICE_ID			0x3B
#define SMB136_COMMAND_B                        0x3C

#endif /* __SMB136_CHARGER_H */

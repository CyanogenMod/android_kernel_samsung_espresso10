/*
 * max77693.h - Driver for the Maxim 77693
 *
 *  Copyright (C) 2012 Samsung Electrnoics
 *  SangYoung Son <hello.son@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.h
 *
 * MAX77693 has Charger, Flash LED, Haptic, MUIC devices.
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __LINUX_MFD_MAX77693_H
#define __LINUX_MFD_MAX77693_H

#include <linux/regulator/consumer.h>
#include <linux/battery/sec_charger.h>

#define MAX8997_MOTOR_REG_CONFIG2	0x2
#define MOTOR_LRA			(1<<7)
#define MOTOR_EN			(1<<6)
#define EXT_PWM				(0<<5)
#define DIVIDER_128			(1<<1)

enum {
	MAX77693_MUIC_DETACHED = 0,
	MAX77693_MUIC_ATTACHED
};

enum usb_cable_status {
	USB_CABLE_DETACHED = 0,
	USB_CABLE_ATTACHED,
	USB_OTGHOST_DETACHED,
	USB_OTGHOST_ATTACHED,
	USB_POWERED_HOST_DETACHED,
	USB_POWERED_HOST_ATTACHED,
	USB_CABLE_DETACHED_WITHOUT_NOTI,
};

/* MAX77686 regulator IDs */
enum max77693_regulators {
	MAX77693_ESAFEOUT1 = 0,
	MAX77693_ESAFEOUT2,

	MAX77693_CHARGER,

	MAX77693_REG_MAX,
};

enum cable_type_muic {
	CABLE_TYPE_NONE_MUIC = 0,
	CABLE_TYPE_USB_MUIC,
	CABLE_TYPE_OTG_MUIC,
	CABLE_TYPE_TA_MUIC,
	CABLE_TYPE_DESKDOCK_MUIC,
	CABLE_TYPE_DESKDOCK_WITH_TA,
	CABLE_TYPE_CARDOCK_MUIC,
	CABLE_TYPE_JIG_UART,
	CABLE_TYPE_JIG_USB,
	CABLE_TYPE_MHL_MUIC,
	CABLE_TYPE_SMARTDOCK_MUIC,
	CABLE_TYPE_SMARTDOCK_WITH_TA,
	CABLE_TYPE_UNKNOWN_MUIC,
	CABLE_TYPE_CHARGER,
};

enum {
	PATH_OPEN = 0,
	PATH_USB_AP,
	PATH_AUDIO,
	PATH_UART_AP,
	PATH_USB_CP,
	PATH_UART_CP,
};

enum {
	USB_SEL_AP = 0,
	USB_SEL_CP,
};

enum {
	UART_SEL_AP = 0,
	UART_SEL_CP,
};

struct max77693_charger_reg_data {
	u8 addr;
	u8 data;
};

struct max77693_charger_platform_data {
	struct max77693_charger_reg_data *init_data;
	int num_init_data;
	/* WPC Charger */
	int wpc_irq_gpio;
	int vbus_irq_gpio;
	bool wc_pwr_det;
};

struct max77693_haptic_platform_data {
	struct i2c_client *pmic_i2c;
	struct i2c_client *haptic_i2c;
};

struct max77693_led_platform_data;

struct max77693_regulator_data {
	int id;
	struct regulator_init_data *initdata;
};

struct max77693_muic_data {
	void (*init_cb) (void);
	int (*set_safeout) (int path);
	void (*detected) (int cable, bool attach);

	int usb_sel;
	int uart_sel;
};

struct max77693_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	char *irq_gpio_label;
	int wakeup;
	struct max77693_muic_data *muic;
	bool (*is_default_uart_path_cp) (void);
	struct max77693_regulator_data *regulators;
	int num_regulators;
	/* haptic motor data */
	struct max77693_haptic_platform_data *haptic_data;
	struct max77693_led_platform_data *led_data;
	/* charger data */
	sec_battery_platform_data_t *charger_data;
};
#endif				/* __LINUX_MFD_MAX77693_H */

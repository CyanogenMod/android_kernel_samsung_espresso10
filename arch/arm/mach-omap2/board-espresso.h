/* arch/arm/mach-omap2/board-espresso.h
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

#ifndef __BOARD_ESPRESSO_H__
#define __BOARD_ESPRESSO_H__

#include <linux/serial_core.h>

extern struct class *sec_class;

#define SEC_MACHINE_ESPRESSO			0x01
#define SEC_MACHINE_ESPRESSO10			0x02
#define SEC_MACHINE_ESPRESSO10_USA_BBY		0x06

enum espresso_adc_ch {
	REMOTE_SENSE = 0,
	ADC_CHECK_1,	/* TA detection */
	ACCESSORY_ID,	/* OTG detection */
	EAR_ADC_35,	/* Earjack detection */
};

bool board_is_espresso10(void);
bool board_has_modem(void);
bool board_is_bestbuy_variant(void);

/** @category common */
unsigned int omap4_espresso_get_board_type(void);

/** @category LCD, HDMI */
void omap4_espresso_display_init(void);
void omap4_espresso_memory_display_init(void);

/** @category LCD */
void __init omap4_espresso_display_early_init(void);

/** @category Key, TSP, Touch-Key */
void omap4_espresso_input_init(void);
void omap4_espresso_tsp_ta_detect(int);

/** @category Jack, Dock */
void omap4_espresso_jack_init(void);

/** @category Charger, Battery */
void omap4_espresso_power_init(void);

/** @category Motion Sensor */
void omap4_espresso_sensors_init(void);
void omap4_espresso_set_chager_type(int type);

/** @category mUSB-IC, MHL */
void omap4_espresso_connector_init(void);
int omap4_espresso_get_adc(enum espresso_adc_ch ch);
void omap4_espresso_usb_detected(int cable_type);

/** @category LPDDR2 */
void omap4_espresso_emif_init(void);

/** @category TWL6030, TWL6040 */
void omap4_espresso_pmic_init(void);

/** @category I2C, UART(GPS) */
void omap4_espresso_serial_init(void);

/** @category MMCHS, WiFi */
void omap4_espresso_sdio_init(void);
extern struct mmc_platform_data espresso_wifi_data;

/** @category WiFi */
void omap4_espresso_wifi_init(void);

/** @category Bluetooth */
void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

/** @category charger */
void omap4_espresso_charger_init(void);

/** @category modem*/
void omap4_espresso_none_modem_init(void);

void check_jig_status(int status);

void notify_dock_status(int status);
#endif /* __BOARD_ESPRESSO_H__ */

/* arch/arm/mach-omap2/board-espresso10.h
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * Based on mach-omap2/board-espresso.h
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

#ifndef __BOARD_ESPRESSO10_H__
#define __BOARD_ESPRESSO10_H__

#include <linux/serial_core.h>

#include "sec_common.h"
/* Reference Device */
#define SEC_MACHINE_ESPRESSO10		0x02
/* Non-Modem Device */
#define SEC_MACHINE_ESPRESSO10_WIFI	0x04
/* Non-Modem Device for Best Buy */
#define SEC_MACHINE_ESPRESSO10_USA_BBY	0x06

enum espresso10_adc_ch {
	REMOTE_SENSE = 0,
	ADC_CHECK_1,	/* TA detection */
	ACCESSORY_ID,	/* OTG detection */
	EAR_ADC_35,	/* Earjack detection */
};

/** @category common */
unsigned int omap4_espresso10_get_board_type(void);

/** @category LCD, HDMI */
void omap4_espresso10_display_init(void);
void omap4_espresso10_display_early_init(void);

/** @category Key, TSP, Touch-Key */
void omap4_espresso10_input_init(void);
void omap4_espresso10_tsp_ta_detect(int);

/** @category Jack, Dock */
void omap4_espresso10_jack_init(void);

/** @category Charger, Battery */
void omap4_espresso10_power_init(void);

/** @category Motion Sensor */
void omap4_espresso10_sensors_init(void);
void omap4_espresso_set_chager_type(int type);

/** @category mUSB-IC, USB-OTG */
void omap4_espresso10_connector_init(void);
int omap4_espresso10_get_adc(enum espresso10_adc_ch ch);
void omap4_espresso10_usb_detected(int cable_type);

/** @category LPDDR2 */
void omap4_espresso10_emif_init(void);

/** @category I2c, UART(GPS) */
void omap4_espresso10_serial_init(void);

/** @category TWL6030, TWL6040 */
void omap4_espresso10_pmic_init(void);

/** @category MMCHS, WiFi */
void omap4_espresso10_sdio_init(void);
extern struct mmc_platform_data espresso10_wifi_data;

/** @category WiFi */
void omap4_espresso10_wifi_init(void);

/** @category Bluetooth */
void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

/** @category charger */
void omap4_espresso10_charger_init(void);

/** @category camera */
void omap4_espresso10_camera_init(void);

/** @category modem*/
void omap4_espresso10_none_modem_init(void);

void check_jig_status(int status);

void notify_dock_status(int status);
#endif /* __BOARD_ESPRESSO_H__ */

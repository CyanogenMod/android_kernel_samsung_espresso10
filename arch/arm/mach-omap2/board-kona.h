/* arch/arm/mach-omap2/board-kona.h
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
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

#ifndef __BOARD_KONA_H__
#define __BOARD_KONA_H__

#include <linux/serial_core.h>

#include "sec_board_id.h"
#include "sec_common.h"

enum kona_adc_ch {
	REMOTE_SENSE = 0,
	ADC_CHECK_1,	/* TA detection */
	ACCESSORY_ID,	/* OTG detection */
	EAR_ADC_35,	/* Earjack detection */
};

/** @category common */
unsigned int omap4_kona_get_board_type(void);

/** @category LCD, HDMI */
void omap4_kona_display_init(void);
void omap4_kona_memory_display_init(void);
void __init omap4_kona_display_early_init(void);

/** @category Key, TSP, Touch-Key */
void omap4_kona_input_init(void);
void omap4_kona_tsp_ta_detect(int cable_type);

/** @category Jack, Dock */
void omap4_kona_jack_init(void);

/** @category Motion Sensor */
void omap4_kona_sensors_init(void);

int omap4_kona_get_adc(enum kona_adc_ch ch);

/** @category mUSB-IC, MHL */
void omap4_kona_connector_init(void);
extern struct max77693_muic_data max77693_muic;

/** @category USB */
extern int omap4430_phy_is_active(struct device *dev);
extern int omap4430_phy_power(struct device *dev, int ID, int on);
extern int omap4430_phy_suspend(struct device *dev, int suspend);
extern int omap4430_phy_init(struct device *dev);
int kona_vusb_enable(u32 device_index, bool enable);

/** @category LPDDR2 */
void omap4_kona_emif_init(void);

/** @category TWL6030, TWL6040 */
void omap4_kona_pmic_init(void);

/** @category I2C, UART(GPS) */
void omap4_kona_serial_init(void);

/** @category MMCHS, WiFi */
void omap4_kona_sdio_init(void);

/** @category WiFi */
void omap4_kona_wifi_init(void);
extern struct mmc_platform_data kona_wifi_data;

/** @category Bluetooth */
void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

/** @category charger */
void omap4_kona_charger_init(void);

/** @category camera */
void omap4_kona_camera_init(void);

/** @category NFC */
void omap4_kona_nfc_init(void);

/** @category audio */
void omap4_kona_audio_init(void);

/** @category FM-Radio */
void omap4_kona_fmradio_init(void);

/** @enable usb */
void kona_30pin_detected(int device, bool connected);

/** @category IRLED */
void omap4_kona_irled_init(void);

/** @category vibrator */
void omap4_kona_vibrator_init(void);

#endif /* __BOARD_KONA_H__ */

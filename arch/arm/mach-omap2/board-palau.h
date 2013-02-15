/* arch/arm/mach-omap2/board-palau.h
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

#ifndef __BOARD_PALAU_H__
#define __BOARD_PALAU_H__

#include <linux/serial_core.h>

#include "sec_board_id.h"
#include "sec_common.h"

/** @category LCD */
void omap4_palau_display_init(void);
void omap4_palau_display_early_init(void);
void omap4_palau_display_memory_init(void);

/** @category Key, TSP, Touch-Key */
void omap4_palau_input_init(void);

/** @category Jack, Dock */
void omap4_palau_jack_init(void);

/** @category Motion Sensor */
void omap4_palau_sensors_init(void);

/** @category mUSB-IC, MHL */
void omap4_palau_connector_init(void);
extern struct max77693_muic_data max77693_muic;
extern int omap4430_phy_is_active(struct device *dev);
extern int omap4430_phy_power(struct device *dev, int ID, int on);
extern int omap4430_phy_suspend(struct device *dev, int suspend);
extern int omap4430_phy_init(struct device *dev);

/** @category LPDDR2 */
void omap4_palau_emif_init(void);

/** @category TWL6030, TWL6040 */
void omap4_palau_pmic_init(void);

/** @category I2C, UART(GPS) */
void omap4_palau_serial_init(void);

/** @category MMCHS, WiFi */
void omap4_palau_sdio_init(void);

/** @category WiFi */
void omap4_palau_wifi_init(void);
extern struct mmc_platform_data palau_wifi_data;

/** @category Bluetooth */
void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

/** @category charger */
void omap4_palau_charger_init(void);

/** @category camera */
void omap4_palau_camera_init(void);

/** @category NFC */
void omap4_palau_nfc_init(void);

/** @category audio */
void omap4_palau_audio_init(void);

/** @category FM-Radio */
void omap4_palau_fmradio_init(void);

/** @category vibrator */
void max77693_haptic_enable(bool enable);
void omap4_palau_vibrator_init(void);

#endif /* __BOARD_PALAU_H__ */

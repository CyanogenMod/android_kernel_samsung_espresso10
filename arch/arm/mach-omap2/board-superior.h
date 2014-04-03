/* arch/arm/mach-omap2/board-superior.h
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

#ifndef __BOARD_SUPERIOR_H__
#define __BOARD_SUPERIOR_H__

#include <linux/serial_core.h>

#include "sec_board_id.h"
#include "sec_common.h"

/** @category LCD, HDMI */
void omap4_superior_display_init(void);
void omap4_superior_memory_display_init(void);

/** @category LCD */
void __init omap4_superior_display_early_init(void);

/** @category Key, TSP, Touch-Key */
void omap4_superior_input_init(void);

/** @category Jack, Dock */
void omap4_superior_jack_init(void);

/** @category Motion Sensor */
void omap4_superior_sensors_init(void);

/** @category mUSB-IC, MHL */
void omap4_superior_connector_init(void);
extern struct max77693_muic_data max77693_muic;
extern int omap4430_phy_is_active(struct device *dev);
extern int omap4430_phy_power(struct device *dev, int ID, int on);
extern int omap4430_phy_suspend(struct device *dev, int suspend);
extern int omap4430_phy_init(struct device *dev);

/** @category LPDDR2 */
void omap4_superior_emif_init(void);

/** @category TWL6030, TWL6040 */
void omap4_superior_pmic_init(void);

/** @category I2C, UART(GPS) */
void omap4_superior_serial_init(void);

/** @category MMCHS, WiFi */
void omap4_superior_sdio_init(void);

/** @category WiFi */
void omap4_superior_wifi_init(void);
extern struct mmc_platform_data superior_wifi_data;

/** @category Bluetooth */
void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

/** @category charger */
void omap4_superior_charger_init(void);

/** @category camera */
void omap4_superior_camera_init(void);
extern struct max77693_led_platform_data max77693_led_pdata;

/** @category NFC */
void omap4_superior_nfc_init(void);

/** @category audio */
void omap4_superior_audio_init(void);

/** @category FM-Radio */
void omap4_superior_fmradio_init(void);

/** @category vibrator */
void max77693_haptic_enable(bool enable);
void omap4_superior_vibrator_init(void);

/** @category spiflash */
void omap4_superior_spiflash_init(void);

#endif /* __BOARD_SUPERIOR_H__ */

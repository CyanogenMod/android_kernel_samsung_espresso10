/* arch/arm/mach-omap2/board-t1.h
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

#ifndef __BOARD_T1_H__
#define __BOARD_T1_H__

#include <linux/serial_core.h>

#include "sec_board_id.h"
#include "sec_common.h"

/** @category LCD, HDMI */
void omap4_t1_display_init(void);
void omap4_t1_display_early_init(void);
void omap4_t1_display_memory_init(void);

/** @category Key, TSP, Touch-Key */
void omap4_t1_input_init(void);

/** @category Jack, Dock */
void omap4_t1_jack_init(void);

/** @category Charger, Battery */
void omap4_t1_power_init(void);

/** @category Motion Sensor */
void omap4_t1_sensors_init(void);

/** @category mUSB-IC, MHL */
void omap4_t1_connector_init(void);

/** @category LPDDR2 */
void omap4_t1_emif_init(void);

/** @category TWL6030, TWL6040 */
void omap4_t1_pmic_init(void);

/** @category I2C, UART(GPS) */
void omap4_t1_serial_init(void);

/** @category  UART( 2, 3) */
void __init omap4_t1_serial_early_init(void);

/** @category MMCHS, WiFi */
void omap4_t1_sdio_init(void);

/** @category WiFi */
void omap4_t1_wifi_init(void);
extern struct mmc_platform_data t1_wifi_data;

/** @category Bluetooth */
void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

/** @category vibrator */
void __init omap4_t1_vibrator_init(void);

/** @category FM-Radio */
void omap4_t1_fmradio_init(void);

/** @category Connector */
extern void t1_init_ta_nconnected(int);

void __init omap4_t1_cam_init(void);
#endif /* __BOARD_T1_H__ */

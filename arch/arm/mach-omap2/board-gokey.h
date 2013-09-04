/* arch/arm/mach-omap2/board-gokey.h
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

#ifndef __BOARD_GOKEY_H__
#define __BOARD_GOKEY_H__

#include <linux/serial_core.h>

#include "sec_board_id.h"
#include "sec_common.h"
#include <linux/cpuidle.h>

enum gokey_adc_ch {
	REMOTE_SENSE = 0,
	ADC_CHECK_1,		/* TA detection */
	ACCESSORY_ID,		/* OTG detection */
	EAR_ADC_35,		/* Earjack detection */
};

int get_temp(void);

/** @category LCD, HDMI */
void omap4_gokey_display_init(void);
void omap4_gokey_memory_display_init(void);

/** @category LCD */
void __init omap4_gokey_display_early_init(void);

/** @category Key, TSP, Touch-Key */
void omap4_gokey_input_init(void);

/** @category Jack, Dock */
void omap4_gokey_jack_init(void);

/** @category Charger, Battery */
void omap4_gokey_power_init(void);

/** @category Motion Sensor */
void omap4_gokey_sensors_init(void);

/** @category mUSB-IC */
void omap4_gokey_connector_init(void);
int gokey_get_charging_type(void);

/** @category LPDDR2 */
void omap4_gokey_emif_init(void);

/** @category TWL6030, TWL6040 */
void omap4_gokey_pmic_init(void);

/** @category WM1811 */
void omap4_gokey_audio_init(void);

/** @category EarJack */
int omap4_get_adc_earjack(void);

/** @category I2C, UART(GPS) */
void omap4_gokey_serial_init(void);

/** @category  UART( 2, 3) */
void __init omap4_gokey_serial_early_init(void);

/** @category MMCHS, WiFi */
void omap4_gokey_sdio_init(void);
extern struct mmc_platform_data gokey_wifi_data;

/** @category WiFi */
void omap4_gokey_wifi_init(void);

/** @category Bluetooth */
void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

/** @category charger */
void omap4_gokey_charger_init(void);

/** @category camera */
void omap4_gokey_camera_init(void);

#ifdef CONFIG_TDMB
/** @category TDMB */
void omap4_gokey_tdmb_init(void);
#endif

#ifdef CONFIG_MP3_LP_MODE
extern bool is_playback_lpmode_available(void);
struct cpufreq_lpmode_info {
	bool bt_enabled;
	bool wifi_enabled;
	bool cable_attached;
	bool lp_mode_enabled;
};
#define OMAP4_STATE_C1		0
#define OMAP4_MAX_STATES	4
struct omap4_processor_cx {
	u8 valid;
	u8 type;
	u32 exit_latency;
	u32 target_residency;
	u32 mpu_state;
	u32 mpu_logic_state;
	u32 core_state;
	u32 core_logic_state;
	const char *desc;
};
extern struct cpufreq_lpmode_info cpufreq_lpmode;
extern void omap4_init_power_states(const struct cpuidle_params
					*cpuidle_params_table);
extern struct omap4_processor_cx omap4_power_states[OMAP4_MAX_STATES];
#endif

#endif				/* __BOARD_GOKEY_H__ */

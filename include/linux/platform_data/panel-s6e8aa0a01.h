/* include/linux/platform_data/panel-s6e8aa0a01.h
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
 *
 * Bsed on panel-s6e8aa0.h
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

#ifndef __PANEL_S6E8AA0A01_H__
#define __PANEL_S6E8AA0A01_H__

#define GAMMA_PARAM_SIZE		26
#define ELVSS_PARAM_SIZE		3

#ifdef CONFIG_AID_DIMMING
#define ELVSS_OFFSET_300		0x00
#define ELVSS_OFFSET_290		0x01
#define ELVSS_OFFSET_280		0x01
#define ELVSS_OFFSET_270		0x02
#define ELVSS_OFFSET_260		0x03
#define ELVSS_OFFSET_250		0x04
#define ELVSS_OFFSET_240		0x04
#define ELVSS_OFFSET_230		0x05
#define ELVSS_OFFSET_220		0x06
#define ELVSS_OFFSET_210		0x06
#define ELVSS_OFFSET_200		0x07
#define ELVSS_OFFSET_190		0x08
#define ELVSS_OFFSET_180		0x03
#define ELVSS_OFFSET_170		0x04
#define ELVSS_OFFSET_160		0x05
#define ELVSS_OFFSET_150		0x06
#define ELVSS_OFFSET_140		0x07
#define ELVSS_OFFSET_130		0x08
#define ELVSS_OFFSET_120		0x09
#define ELVSS_OFFSET_110		0x09

enum {
	ELVSS_110 = 0,
	ELVSS_120,
	ELVSS_130,
	ELVSS_140,
	ELVSS_150,
	ELVSS_160,
	ELVSS_170,
	ELVSS_180,
	ELVSS_190,
	ELVSS_200,
	ELVSS_210,
	ELVSS_220,
	ELVSS_230,
	ELVSS_240,
	ELVSS_250,
	ELVSS_260,
	ELVSS_270,
	ELVSS_280,
	ELVSS_290,
	ELVSS_300,
	ELVSS_STATUS_MAX,
};

#else
#define ELVSS_OFFSET_300		0x00
#define ELVSS_OFFSET_200		0x08
#define ELVSS_OFFSET_160		0x0D
#define ELVSS_OFFSET_100		0x12

#define ELVSS_OFFSET_MAX		ELVSS_OFFSET_300
#define ELVSS_OFFSET_1			ELVSS_OFFSET_160
#define ELVSS_OFFSET_2			ELVSS_OFFSET_1
#define ELVSS_OFFSET_MIN		ELVSS_OFFSET_100

enum {
	ELVSS_MIN = 0,
	ELVSS_1,
	ELVSS_2,
	ELVSS_MAX,
	ELVSS_STATUS_MAX,
};
#endif

enum {
	GAMMA_20CD,
	GAMMA_30CD,
	GAMMA_40CD,
	GAMMA_50CD,
	GAMMA_60CD,
	GAMMA_70CD,
	GAMMA_80CD,
	GAMMA_90CD,
	GAMMA_100CD,
	GAMMA_110CD,
	GAMMA_120CD,
	GAMMA_130CD,
	GAMMA_140CD,
	GAMMA_150CD,
	GAMMA_160CD,
	GAMMA_170CD,
	GAMMA_180CD,
#ifdef CONFIG_AID_DIMMING
	GAMMA_182CD,
	GAMMA_184CD,
	GAMMA_186CD,
	GAMMA_188CD,
#endif
	GAMMA_190CD,
	GAMMA_200CD,
	GAMMA_210CD,
	GAMMA_220CD,
	GAMMA_230CD,
	GAMMA_240CD,
	GAMMA_250CD,
	GAMMA_290CD,
	GAMMA_300CD,
	GAMMA_MAX
};

static const unsigned char SEQ_ELVSS_32[] = {
	0xB1,
	0x04, 0x9F
};

static const unsigned char SEQ_ELVSS_34[] = {
	0xB1,
	0x04, 0x9D
};

static const unsigned char SEQ_ELVSS_38[] = {
	0xB1,
	0x04, 0x99
};

static const unsigned char SEQ_ELVSS_47[] = {
	0xB1,
	0x04, 0x90
};

static const unsigned char *ELVSS_TABLE[] = {
	SEQ_ELVSS_32,
	SEQ_ELVSS_34,
	SEQ_ELVSS_38,
	SEQ_ELVSS_47,
};

/* 4.65" Panel ID Info (D1h 1st Para) */
#define SM2A1				0xA1
#define SM2A2				0xA2

struct s6e8aa0a01_sequence_entry {
	const u8 *cmd;
	int cmd_len;
	unsigned int msleep;
};

struct s6e8aa0a01_acl_parameters {
	unsigned int cd;
	unsigned int acl_val;
	u8 regs[29];
};

struct panel_s6e8aa0a01_data {
	int reset_gpio;
	int oled_id_gpio;
	int oled_det_irq;
	void (*set_power) (bool enable);

	const struct s6e8aa0a01_sequence_entry *seq_display_set;
	int seq_display_set_size;
	const struct s6e8aa0a01_sequence_entry *seq_etc_set;
	int seq_etc_set_size;
	const unsigned char *seq_panel_condition_set;
	int seq_panel_condition_set_size;

	const struct s6e8aa0a01_acl_parameters *acl_table;
	unsigned int acl_table_size;
};

#endif /* __PANEL_S6E8AA0A01_H__ */

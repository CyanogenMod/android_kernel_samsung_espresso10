/* drivers/video/omap2/displays/smart_dimming.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
 *
 * Minwoo Kim, <minwoo7945.kim@samsung.com>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>

#include "smart_dimming.h"
#include "s6e8aa0_volt_tbl.h"

#define MTP_REVERSE			1
#define VALUE_DIM_1000			1000
#ifdef CONFIG_AID_DIMMING
#define V1_255_div_1000			600000
#define V1_255_calc_param		(V1_255_div_1000 / VREG_OUT_1000)
#endif

#define VREG_OUT_1000			4606

static const u8 v1_offset_table[14] = {
	47, 42, 37, 32,
	27, 23, 19, 15,
	12, 9, 6, 4,
	2, 0
};

static const u8 v15_offset_table[20] = {
	66, 62, 58, 54,
	50, 46, 43, 38,
	34, 30, 27, 24,
	21, 18, 15, 12,
	9, 6, 3, 0,
};

static const u8 range_table_count[IV_TABLE_MAX] = {
	1, 14, 20, 24, 28, 84, 84, 1
};

static const u32 table_radio[IV_TABLE_MAX] = {
	0, 630, 468, 1365, 1170, 390, 390, 0
};

static const u32 dv_value[IV_MAX] = {
	0, 15, 35, 59, 87, 171, 255
};

static const char color_name[3] = { 'R', 'G', 'B' };

static const u8 *offset_table[IV_TABLE_MAX] = {
	[1] = v1_offset_table,
	[2] = v15_offset_table,
};

/* SM2, Superior A1 Panel */
static const u8 gamma_300cd_a1[] = {
	0x5D, 0x4A, 0x64, 0xB8, 0xB4, 0xA4,
	0xB6, 0xC1, 0xAC, 0xC5, 0xC7, 0xBF,
	0x96, 0x99, 0x8E, 0xAE, 0xAE, 0xA9,
	0x00, 0xB5, 0x00, 0xB1, 0x00, 0xD3,
};

/* SM2, Superior A2 Panel */
static const u8 gamma_300cd_a2[] = {
	0x41, 0x0A, 0x47, 0xAB, 0xBE, 0xA8,
	0xAF, 0xC5, 0xB7, 0xC3, 0xCC, 0xC3,
	0x9A, 0xA3, 0x96, 0xB1, 0xB7, 0xAF,
	0x00, 0xBD, 0x00, 0xAC, 0x00, 0xDE,
};

/* SM2, Superior A1, A2 Ver B Panel */
static const u8 gamma_300cd_ver_b[] = {
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
};

static const u8 *gamma_300cd_list[GAMMA_300CD_MAX] = {
	gamma_300cd_a1,
	gamma_300cd_a2,
	gamma_300cd_ver_b,
};

static const u8 gamma_id_list[GAMMA_300CD_MAX] = {
	0xa1, 0xa2
};

static s16 s9_to_s16(s16 v)
{
	return (s16) (v << 7) >> 7;
}

static u32 calc_v1_volt(s16 gamma, int rgb_index,
			u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 ret;

	ret = volt_table_v1[gamma] >> 10;

	return ret;
}

static u32 calc_v15_volt(s16 gamma, int rgb_index,
			 u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	int ret;
	u32 v1, v35;
	u32 ratio;

	v1 = adjust_volt[rgb_index][AD_IV1];
	v35 = adjust_volt[rgb_index][AD_IV35];
	ratio = volt_table_cv_20_dv_320[gamma];

	ret = (v1 << 10) - ((v1 - v35) * ratio);
	ret = ret >> 10;

	return ret;
}

static u32 calc_v35_volt(s16 gamma, int rgb_index,
			 u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	int ret;
	u32 v1, v59;
	u32 ratio;

	v1 = adjust_volt[rgb_index][AD_IV1];
	v59 = adjust_volt[rgb_index][AD_IV59];
	ratio = volt_table_cv_65_dv_320[gamma];

	ret = (v1 << 10) - ((v1 - v59) * ratio);
	ret = ret >> 10;

	return ret;
}

static u32 calc_v50_v59_volt(s16 gamma, int rgb_index,
			     u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	int ret;
	u32 v1, v87;
	u32 ratio;

	v1 = adjust_volt[rgb_index][AD_IV1];
	v87 = adjust_volt[rgb_index][AD_IV87];
	ratio = volt_table_cv_65_dv_320[gamma];

	ret = (v1 << 10) - ((v1 - v87) * ratio);
	ret = ret >> 10;

	return ret;
}

static u32 calc_v87_volt(s16 gamma, int rgb_index,
			 u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	int ret;
	u32 v1, v171;
	u32 ratio;

	v1 = adjust_volt[rgb_index][AD_IV1];
	v171 = adjust_volt[rgb_index][AD_IV171];
	ratio = volt_table_cv_65_dv_320[gamma];

	ret = (v1 << 10) - ((v1 - v171) * ratio);
	ret = ret >> 10;

	return ret;
}

static u32 calc_v171_volt(s16 gamma, int rgb_index,
			  u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	int ret;
	u32 v1, v255;
	u32 ratio;

	v1 = adjust_volt[rgb_index][AD_IV1];
	v255 = adjust_volt[rgb_index][AD_IV255];
	ratio = volt_table_cv_65_dv_320[gamma];

	ret = (v1 << 10) - ((v1 - v255) * ratio);
	ret = ret >> 10;

	return ret;
}

static u32 calc_v255_volt(s16 gamma, int rgb_index,
			  u32 adjust_volt[CI_MAX][AD_IVMAX])
{
	u32 ret = 0;

	ret = volt_table_v255[gamma] >> 10;

	return ret;
}

u8 calc_voltage_table(struct str_smart_dim *smart, const u8 *mtp)
{
	int c, i, j;
#if defined(MTP_REVERSE)
	int offset1 = 0;
#endif
	int offset = 0;
	s16 t1;
	u16 t2;
	u16 adjust_mtp[CI_MAX][IV_MAX];
	/* u32 adjust_volt[CI_MAX][AD_IVMAX] = {0, }; */
	u8 range_index;
	u8 table_index = 0;

	u32 v1, v2;
	u32 ratio;

	u32(*calc_volt[IV_MAX]) (s16 gamma, int rgb_index,
				 u32 adjust_volt[CI_MAX][AD_IVMAX]) = {
	calc_v1_volt,
		    calc_v15_volt,
		    calc_v35_volt,
		    calc_v50_v59_volt,
		    calc_v87_volt, calc_v171_volt, calc_v255_volt,};

	u8 calc_seq[6] = { IV_1, IV_171, IV_87, IV_59, IV_35, IV_15 };
	u8 ad_seq[6] = { AD_IV1, AD_IV171, AD_IV87, AD_IV59, AD_IV35, AD_IV15 };

	memset(adjust_mtp, 0, sizeof(adjust_mtp));

	for (c = CI_RED; c < CI_MAX; c++) {
		offset = IV_255 * CI_MAX + c * 2;
#if defined(MTP_REVERSE)
		offset1 = IV_255 * (c + 1) + (c * 2);
		t1 = s9_to_s16(mtp[offset1] << 8 | mtp[offset1 + 1]);
#else
		t1 = s9_to_s16(mtp[offset] << 8 | mtp[offset + 1]);
#endif
		t2 = (u16)(smart->default_gamma[offset] << 8 |
			       smart->default_gamma[offset + 1]) + t1;
		smart->mtp[c][IV_255] = t1;
		adjust_mtp[c][IV_255] = t2;
		smart->adjust_volt[c][AD_IV255] =
		    calc_volt[IV_255] (t2, c, smart->adjust_volt);

		/* for V0 All RGB Voltage Value is Reference Voltage */
		smart->adjust_volt[c][AD_IV0] = VREG_OUT_1000;
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		for (i = IV_1; i < IV_255; i++) {
#if defined(MTP_REVERSE)
			t1 = (s8) mtp[(calc_seq[i]) + (c * 8)];
#else
			t1 = (s8) mtp[CI_MAX * calc_seq[i] + c];
#endif
			t2 = smart->default_gamma[CI_MAX * calc_seq[i] + c] +
			    t1;

			smart->mtp[c][calc_seq[i]] = t1;
			adjust_mtp[c][calc_seq[i]] = t2;
			smart->adjust_volt[c][ad_seq[i]] =
			    calc_volt[calc_seq[i]] (t2, c, smart->adjust_volt);
		}
	}

	for (i = 0; i < AD_IVMAX; i++) {
		for (c = CI_RED; c < CI_MAX; c++)
			smart->ve[table_index].v[c] = smart->adjust_volt[c][i];

		range_index = 0;
		for (j = table_index + 1;
		     j < table_index + range_table_count[i]; j++) {
			for (c = CI_RED; c < CI_MAX; c++) {
				if (smart->t_info[i].offset_table != NULL)
					ratio =
					    smart->
					    t_info[i].offset_table[range_index]
					    * smart->t_info[i].rv;
				else
					ratio = (range_table_count[i] -
						 (range_index + 1)) *
					    smart->t_info[i].rv;

				v1 = smart->adjust_volt[c][i + 1] << 15;
				v2 = (smart->adjust_volt[c][i] -
				      smart->adjust_volt[c][i + 1]) * ratio;
				smart->ve[j].v[c] = ((v1 + v2) >> 15);
			}
			range_index++;
		}
		table_index = j;
	}

	return 0;
}

int init_table_info(struct str_smart_dim *smart)
{
	int i;
	int offset = 0;

	for (i = 0; i < IV_TABLE_MAX; i++) {
		smart->t_info[i].count = (u8) range_table_count[i];
		smart->t_info[i].offset_table = offset_table[i];
		smart->t_info[i].rv = table_radio[i];
		offset += range_table_count[i];
	}
	smart->flooktbl = flookup_table;
	smart->g300_gra_tbl = gamma_300_gra_table;
#ifdef CONFIG_AID_DIMMING
	smart->gamma_table[G_21] = gamma_21_table;
	smart->gamma_table[G_215] = gamma_215_table;
	smart->gamma_table[G_22] = gamma_22_table;
#else
	smart->g22_tbl = gamma_22_table;
#endif
	if (smart->panelid[1] == 0x02) {
		smart->default_gamma = gamma_300cd_list[2];
		goto out;
	}

	for (i = 0; i < GAMMA_300CD_MAX; i++) {
		if (smart->panelid[0] == gamma_id_list[i])
			break;
	}

	if (i >= GAMMA_300CD_MAX) {
		printk(KERN_ERR
		       "[SMART DIMMING-WARNING] %s Can't found default gamma table\n",
		       __func__);
		smart->default_gamma = gamma_300cd_list[GAMMA_300CD_MAX - 1];
	} else
		smart->default_gamma = gamma_300cd_list[i];
out:
	return 0;
}

static u32 lookup_vtbl_idx(struct str_smart_dim *smart, u32 gamma)
{
	u32 lookup_index;
	u16 table_count, table_index;
	u32 gap, i;
	u32 minimum = smart->g300_gra_tbl[255];
	u32 candidate = 0;
	u32 offset = 0;

	lookup_index = (gamma / VALUE_DIM_1000) + 1;
	if (lookup_index > MAX_GRADATION) {
		printk(KERN_ERR "ERROR Wrong input value  LOOKUP INDEX : %d\n",
		       lookup_index);
		return 0;
	}

	if (smart->flooktbl[lookup_index].count) {
		if (smart->flooktbl[lookup_index - 1].count) {
			table_index = smart->flooktbl[lookup_index - 1].entry;
			table_count = smart->flooktbl[lookup_index].count +
			    smart->flooktbl[lookup_index - 1].count;
		} else {
			table_index = smart->flooktbl[lookup_index].entry;
			table_count = smart->flooktbl[lookup_index].count;
		}
	} else {
		offset += 1;
		while (!(smart->flooktbl[lookup_index + offset].count ||
			 smart->flooktbl[lookup_index - offset].count))
			offset++;

		if (smart->flooktbl[lookup_index - offset].count)
			table_index =
			    smart->flooktbl[lookup_index - offset].entry;
		else
			table_index =
			    smart->flooktbl[lookup_index + offset].entry;
		table_count = smart->flooktbl[lookup_index + offset].count +
		    smart->flooktbl[lookup_index - offset].count;
	}

	for (i = 0; i < table_count; i++) {
		if (gamma > smart->g300_gra_tbl[table_index])
			gap = gamma - smart->g300_gra_tbl[table_index];
		else
			gap = smart->g300_gra_tbl[table_index] - gamma;

		if (gap == 0) {
			candidate = table_index;
			break;
		}

		if (gap < minimum) {
			minimum = gap;
			candidate = table_index;
		}
		table_index++;
	}

	return candidate;
}

static u32 calc_v1_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
	u32 ret;
	u32 v1;

	v1 = dv[ci][IV_1];
#ifdef CONFIG_AID_DIMMING
	ret = (595 * 1000) - (V1_255_calc_param * v1);
#else
	ret = (595 * 1000) - (130 * v1);
#endif
	ret = ret / 1000;

	return ret;
}

static u32 calc_v15_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
	u32 t1, t2;
	u32 v1, v15, v35;
	u32 ret;

	v1 = dv[ci][IV_1];
	v15 = dv[ci][IV_15];
	v35 = dv[ci][IV_35];

	t1 = (v1 - v15) << 10;
	t2 = (v1 - v35) ? (v1 - v35) : (v1) ? v1 : 1;
	ret = (320 * (t1 / t2)) - (20 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v35_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
	u32 t1, t2;
	u32 v1, v35, v57;
	u32 ret;

	v1 = dv[ci][IV_1];
	v35 = dv[ci][IV_35];
	v57 = dv[ci][IV_59];

	t1 = (v1 - v35) << 10;
	t2 = (v1 - v57) ? (v1 - v57) : (v1) ? v1 : 1;
	ret = (320 * (t1 / t2)) - (65 << 10);

	ret >>= 10;

	return ret;
}

#ifdef CONFIG_AID_DIMMING
static u32 calc_v59_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
	u32 t1, t2;
	u32 v1, v59, v87;
	u32 ret;

	v1 = dv[ci][IV_1];
	v59 = dv[ci][IV_59];
	v87 = dv[ci][IV_87];

	t1 = (v1 - v59) << 10;
	t2 = (v1 - v87) ? (v1 - v87) : (v1) ? v1 : 1;
	ret = (320 * (t1 / t2)) - (65 << 10);
	ret >>= 10;

	return ret;
}
#else
static u32 calc_v50_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
	u32 t1, t2;
	u32 v1, v57, v87;
	u32 ret;

	v1 = dv[ci][IV_1];
	v57 = dv[ci][IV_59];
	v87 = dv[ci][IV_87];

	t1 = (v1 - v57) << 10;
	t2 = (v1 - v87) ? (v1 - v87) : (v1) ? v1 : 1;
	ret = (320 * (t1 / t2)) - (65 << 10);
	ret >>= 10;
	return ret;
}
#endif /* CONFIG_AID_DIMMING */

static u32 calc_v87_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
	u32 t1, t2;
	u32 v1, v87, v171;
	u32 ret;

	v1 = dv[ci][IV_1];
	v87 = dv[ci][IV_87];
	v171 = dv[ci][IV_171];

	t1 = (v1 - v87) << 10;
	t2 = (v1 - v171) ? (v1 - v171) : (v1) ? v1 : 1;
	ret = (320 * (t1 / t2)) - (65 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v171_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
	u32 t1, t2;
	u32 v1, v171, v255;
	u32 ret;

	v1 = dv[ci][IV_1];
	v171 = dv[ci][IV_171];
	v255 = dv[ci][IV_255];

	t1 = (v1 - v171) << 10;
	t2 = (v1 - v255) ? (v1 - v255) : (v1) ? v1 : 1;
	ret = (320 * (t1 / t2)) - (65 << 10);
	ret >>= 10;

	return ret;
}

static u32 calc_v255_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
	u32 ret;
	u32 v255;

	v255 = dv[ci][IV_255];

#ifdef CONFIG_AID_DIMMING
	ret = (500 * 1000) - (V1_255_calc_param * v255);
#else
	ret = (500 * 1000) - (130 * v255);
#endif
	ret = ret / 1000;

	return ret;
}

#ifdef CONFIG_AID_DIMMING
u32 calc_gamma_table(struct str_smart_dim *smart,
		     u32 gv, u8 result[], u8 gamma_curve)
{
	u32 i, c;
	u32 temp;
	u32 lidx;
	u32 dv[CI_MAX][IV_MAX];
	s16 gamma[CI_MAX][IV_MAX];
	u16 offset;
	u32(*calc_reg[IV_MAX]) (int ci, u32 dv[CI_MAX][IV_MAX]) = {
	calc_v1_reg,
		    calc_v15_reg,
		    calc_v35_reg,
		    calc_v59_reg, calc_v87_reg, calc_v171_reg, calc_v255_reg,};

	memset(gamma, 0, sizeof(gamma));

	for (c = CI_RED; c < CI_MAX; c++)
		dv[c][IV_1] = smart->ve[AD_IV1].v[c];

	for (i = IV_15; i < IV_MAX; i++) {
		temp = (smart->gamma_table[gamma_curve][dv_value[i]]
			* gv) / 1000;
		lidx = lookup_vtbl_idx(smart, temp);
		for (c = CI_RED; c < CI_MAX; c++)
			dv[c][i] = smart->ve[lidx].v[c];
	}

	/* for IV1 does not calculate value */
	/* just use default gamma value (IV1) */
	for (c = CI_RED; c < CI_MAX; c++)
		gamma[c][IV_1] = smart->default_gamma[c];

	for (i = IV_15; i < IV_MAX; i++) {
		for (c = CI_RED; c < CI_MAX; c++)
			gamma[c][i] =
			    (s16) calc_reg[i] (c, dv) - smart->mtp[c][i];
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		offset = IV_255 * CI_MAX + c * 2;
		result[offset] = (gamma[c][IV_255]>>8)&0xFF;
		result[offset + 1] = gamma[c][IV_255]&0xFF;
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		for (i = IV_1; i < IV_255; i++)
			result[(CI_MAX * i) + c] = gamma[c][i];
	}

	return 0;
}

u32 calc_gamma_table_215_190(struct str_smart_dim *smart, u32 gv, u8 result[])
{
	u32 i, c;
	u32 temp;
	u32 lidx_215_190;
	u32 dv[CI_MAX][IV_MAX];
	s16 gamma_215_190[CI_MAX][IV_MAX];
	u16 offset;
	u32(*calc_reg[IV_MAX]) (int ci, u32 dv[CI_MAX][IV_MAX]) = {
		calc_v1_reg,
		calc_v15_reg,
		calc_v35_reg,
		calc_v59_reg,
		calc_v87_reg,
		calc_v171_reg,
		calc_v255_reg,
	};

	memset(gamma_215_190, 0, sizeof(gamma_215_190));

	for (c = CI_RED; c < CI_MAX; c++)
		dv[c][IV_1] = smart->ve[AD_IV1].v[c];

	for (i = IV_15; i < IV_MAX; i++) {
		if (i < IV_171) {
			temp = (smart->gamma_table[G_215][dv_value[i]] * gv)
				/ 1000;
			lidx_215_190 = lookup_vtbl_idx(smart, temp) + 1;
			printk(KERN_INFO "look index : %d\n", lidx_215_190);
		} else {
			temp = (smart->gamma_table[G_215][dv_value[i]] * gv)
				/ 1000;
			lidx_215_190 = lookup_vtbl_idx(smart, temp);
		}
		for (c = CI_RED; c < CI_MAX; c++)
			dv[c][i] = smart->ve[lidx_215_190].v[c];
	}

	/* for IV1 does not calculate value */
	/* just use default gamma value (IV1) */
	for (c = CI_RED; c < CI_MAX; c++)
		gamma_215_190[c][IV_1] = smart->default_gamma[c];

	for (i = IV_15; i < IV_MAX; i++) {
		for (c = CI_RED; c < CI_MAX; c++)
			gamma_215_190[c][i] =
			    (s16) calc_reg[i] (c, dv) - smart->mtp[c][i];
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		offset = IV_255 * CI_MAX + c * 2;
		result[offset] = (gamma_215_190[c][IV_255]>>8)&0xFF;
		result[offset + 1] = gamma_215_190[c][IV_255]&0xFF;
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		for (i = IV_1; i < IV_255; i++)
			result[(CI_MAX * i) + c] = gamma_215_190[c][i];
	}

	return 0;
}
#else /* CONFIG_AID_DIMMING */
u32 calc_gamma_table(struct str_smart_dim *smart, u32 gv, u8 result[])
{
	u32 i, c;
	u32 temp;
	u32 lidx;
	u32 dv[CI_MAX][IV_MAX];
	s16 gamma[CI_MAX][IV_MAX];
	u16 offset;
	u32(*calc_reg[IV_MAX]) (int ci, u32 dv[CI_MAX][IV_MAX]) = {
		calc_v1_reg,
		calc_v15_reg,
		calc_v35_reg,
		calc_v50_reg,
		calc_v87_reg,
		calc_v171_reg,
		calc_v255_reg,
	};

	memset(gamma, 0, sizeof(gamma));

	for (c = CI_RED; c < CI_MAX; c++)
		dv[c][0] = smart->adjust_volt[c][AD_IV1];

	for (i = IV_15; i < IV_MAX; i++) {
		temp = smart->g22_tbl[dv_value[i]] * gv;
		lidx = lookup_vtbl_idx(smart, temp);
		for (c = CI_RED; c < CI_MAX; c++)
			dv[c][i] = smart->ve[lidx].v[c];
	}

	/* for IV1 does not calculate value */
	/* just use default gamma value (IV1) */
	for (c = CI_RED; c < CI_MAX; c++)
		gamma[c][IV_1] = smart->default_gamma[c];

	for (i = IV_15; i < IV_MAX; i++) {
		for (c = CI_RED; c < CI_MAX; c++)
			gamma[c][i] =
			    (s16) calc_reg[i] (c, dv) - smart->mtp[c][i];
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		offset = IV_255 * CI_MAX + c * 2;
		result[offset + 1] = gamma[c][IV_255];
	}

	for (c = CI_RED; c < CI_MAX; c++) {
		for (i = IV_1; i < IV_255; i++)
			result[(CI_MAX * i) + c] = gamma[c][i];
	}

	return 0;
}
#endif /* CONFIG_AID_DIMMING */

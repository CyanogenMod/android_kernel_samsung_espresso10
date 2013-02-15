/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
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
 * This driver is based on max17040_battery.c
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/battery.h>
#include <linux/power/max17042_battery.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#define LOW_BATT_COMP_RANGE_NUM	5
#define LOW_BATT_COMP_LEVEL_NUM	2

#define QUICKSTART_POWER_OFF_VOLTAGE	3400

enum max17042_register {
	MAX17042_STATUS		= 0x00,
	MAX17042_VALRT_Th	= 0x01,
	MAX17042_TALRT_Th	= 0x02,
	MAX17042_SALRT_Th	= 0x03,
	MAX17042_AtRate		= 0x04,
	MAX17042_RepCap		= 0x05,
	MAX17042_RepSOC		= 0x06,
	MAX17042_Age		= 0x07,
	MAX17042_TEMP		= 0x08,
	MAX17042_VCELL		= 0x09,
	MAX17042_Current	= 0x0A,
	MAX17042_AvgCurrent	= 0x0B,
	MAX17042_Qresidual	= 0x0C,
	MAX17042_SOC		= 0x0D,
	MAX17042_AvSOC		= 0x0E,
	MAX17042_RemCap		= 0x0F,
	MAX17402_FullCAP	= 0x10,
	MAX17042_TTE		= 0x11,
	MAX17042_V_empty	= 0x12,

	MAX17042_RSLOW		= 0x14,

	MAX17042_AvgTA		= 0x16,
	MAX17042_Cycles		= 0x17,
	MAX17042_DesignCap	= 0x18,
	MAX17042_AvgVCELL	= 0x19,
	MAX17042_MinMaxTemp	= 0x1A,
	MAX17042_MinMaxVolt	= 0x1B,
	MAX17042_MinMaxCurr	= 0x1C,
	MAX17042_CONFIG		= 0x1D,
	MAX17042_ICHGTerm	= 0x1E,
	MAX17042_AvCap		= 0x1F,
	MAX17042_ManName	= 0x20,
	MAX17042_DevName	= 0x21,
	MAX17042_DevChem	= 0x22,
	MAX17042_FullCAP_Nom	= 0x23,

	MAX17042_TempNom	= 0x24,
	MAX17042_TempCold	= 0x25,
	MAX17042_TempHot	= 0x26,
	MAX17042_AIN		= 0x27,
	MAX17042_LearnCFG	= 0x28,
	MAX17042_SHFTCFG	= 0x29,
	MAX17042_RelaxCFG	= 0x2A,
	MAX17042_MiscCFG	= 0x2B,
	MAX17042_TGAIN		= 0x2C,
	MAx17042_TOFF		= 0x2D,
	MAX17042_CGAIN		= 0x2E,
	MAX17042_COFF		= 0x2F,

	MAX17042_Q_empty	= 0x33,
	MAX17042_T_empty	= 0x34,

	MAX17042_RCOMP0		= 0x38,
	MAX17042_TempCo		= 0x39,
	MAX17042_Rx		= 0x3A,
	MAX17042_T_empty0	= 0x3B,
	MAX17042_TaskPeriod	= 0x3C,
	MAX17042_FSTAT		= 0x3D,

	MAX17042_SHDNTIMER	= 0x3F,

	MAX17042_DQACC		= 0x45,
	MAX17042_DPACC		= 0x46,
	MAX17042_VFRemCap	= 0x4A,

	MAX17042_QH		= 0x4D,
	MAX17042_QL		= 0x4E,
	MAX17042_VFOCV		= 0xFB,
	MAX17042_VFSOC		= 0xFF,
};

struct max17042_info {
	/* test print count */
	int pr_cnt;
	/* battery type */
	int battery_type;
	/* full charge comp */
	u32 previous_fullcap;
	u32 previous_vffullcap;
	u32 full_charged_cap;
	/* capacity and vfcapacity */
	u16 capacity;
	u16 vfcapacity;
	int soc_restart_flag;
	/* cap corruption check */
	u32 previous_repsoc;
	u32 previous_vfsoc;
	u32 previous_remcap;
	u32 previous_mixcap;
	u32 previous_fullcapacity;
	u32 previous_vfcapacity;
	u32 previous_vfocv;
	/* low battery comp */
	int low_batt_comp_cnt[LOW_BATT_COMP_RANGE_NUM][LOW_BATT_COMP_LEVEL_NUM];
	int check_start_vol;
	int low_batt_comp_flag;
};

struct max17042_chip {
	struct i2c_client *client;
	struct max17042_platform_data *pdata;
	struct max17042_fuelgauge_callbacks callbacks;
	struct max17042_info info;
	struct mutex fg_lock;
};

static int max17042_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17042_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17042_get_register(struct max17042_fuelgauge_callbacks *ptr,
		u8 addr)
{
	int reg_data;
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);

	reg_data = max17042_read_reg(chip->client, addr);

	pr_debug("%s: addr(%d), cap(0x%x, %d)\n", __func__,
			addr, reg_data, (reg_data / 2));

	return reg_data;
}

static int max17042_get_vcell(struct i2c_client *client)
{
	u32 vcell;
	u16 data;

	data = max17042_read_reg(client, MAX17042_VCELL);

	vcell = (data & 0xFFF) * 78125 / 1000000;
	vcell += ((((data & 0xF000) >> 4) * 78125) / 1000000) << 4;
	dev_info(&client->dev, "VCELL : %d, data : 0x%x\n",
			vcell, data);

	return vcell * 1000;
}

static int max17042_get_soc(struct i2c_client *client)
{
	u16 data;
	u16 soc;

	data = max17042_read_reg(client, MAX17042_RepSOC);
	soc = clamp((data/256), 0, 100);
	dev_info(&client->dev, "SOC : %d, data : 0x%x\n",
			soc, data);

	return soc;
}

static int max17042_check_battery_present(struct i2c_client *client)
{
	u16 status_data;
	int ret = 1;

	status_data = max17042_read_reg(client, MAX17042_STATUS);
	dev_dbg(&client->dev, "%s: status data : 0x%x\n",
		__func__, status_data);
	if (status_data & (0x1 << 3)) {
		dev_dbg(&client->dev, "%s : battery is not present!!\n",
			__func__);
		ret = 0;
	} else {
		dev_dbg(&client->dev, "%s : battery is present!!\n",
			__func__);
	}

	return ret;
}

static int max17042_get_temperature(struct i2c_client *client)
{
	u16 data = 0;
	int temper;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	if (max17042_check_battery_present(client)) {
		data = max17042_read_reg(client, MAX17042_TEMP);

		if ((data >> 8) & (0x1 << 7)) {
			temper = ((~(data >> 8)) & 0xFF) + 1;
			temper *= (-10);
		} else {
			temper = (data >> 8) & 0x7f;
			temper *= 1000;
			temper += (data & 0xFF) * 39 / 10;
			temper /= 100;
		}
	} else {
		temper = 200;
	}

	if (chip->info.battery_type != SDI_BATTERY_TYPE) {
		if (temper > 350)
			temper = (27 * temper) / 10 - 595;
		else if (temper <= (-10))
			temper = ((7 * temper) / 10 + 7) + 20;
	}

	dev_info(&client->dev, "TEMPERATURE : %d, data :0x%x\n",
		temper, data);

	return temper;
}

static int max17042_get_avg_current(struct i2c_client *client)
{
	u16 data;
	u32 avg_current;

	data = max17042_read_reg(client, MAX17042_AvgCurrent);

	avg_current = (data & (0x1 << 15)) ?
		((((~data & 0xFFFF) + 1) * 15625) / 100000) * (-1) :
		(data * 15625) / 100000;

	dev_info(&client->dev, "AVG Current : %d, data :0x%x\n",
		avg_current, data);
	return avg_current;
}

static int max17042_get_current(struct i2c_client *client)
{
	u16 data;
	u32 fg_current;

	data = max17042_read_reg(client, MAX17042_Current);

	fg_current = (data & (0x1 << 15)) ?
		((((~data & 0xFFFF) + 1) * 15625) / 100000) * (-1) :
		(data * 15625) / 100000;
	dev_info(&client->dev, "Current : %d, data :0x%x\n",
		fg_current, data);

	return fg_current;
}

static int max17042_get_vfocv(struct i2c_client *client)
{
	u16 data;
	u32 vfocv = 0;
	u32 temp;

	data = max17042_read_reg(client, MAX17042_VFOCV);

	vfocv = ((data & 0xFFF) * 78125) / 1000000;

	temp = (((data & 0xF000) >> 4) * 78125) / 1000000;
	vfocv += (temp << 4);

	dev_dbg(&client->dev, "vfocv : %d, data :0x%x\n",
		vfocv, data);

	return vfocv;
}

static int max17042_get_vfsoc(struct i2c_client *client)
{
	u16 data;

	data = max17042_read_reg(client, MAX17042_VFSOC);
	dev_dbg(&client->dev, "vfsoc : 0x%x\n", data);

	return (int)(data >> 8);
}

static int max17042_reset_soc(struct max17042_fuelgauge_callbacks *ptr)
{
	u16 data;
	u32 fullcap;
	int vfocv;

	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);

	/* delay for current stablization */
	msleep(500);
	dev_info(&chip->client->dev,
		"%s: %s - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d), "
		"current(%d), avg current(%d)\n",
		__func__, "Before quick-start",
		max17042_get_vcell(chip->client),
		max17042_get_vfocv(chip->client),
		max17042_get_vfsoc(chip->client),
		max17042_get_soc(chip->client),
		max17042_get_current(chip->client),
		max17042_get_avg_current(chip->client));

	if (!chip->pdata->jig_on) {
		dev_info(&chip->client->dev,
			"%s: JIG Not connected -> return\n", __func__);
		return 0;
	}

	max17042_write_reg(chip->client, MAX17042_Cycles, 0);

	data = max17042_read_reg(chip->client, MAX17042_MiscCFG);

	data |= (0x1 << 10);
	max17042_write_reg(chip->client, MAX17042_MiscCFG, data);

	msleep(250);
	max17042_write_reg(chip->client, MAX17402_FullCAP, chip->info.capacity);
	msleep(500);

	dev_info(&chip->client->dev,
		"%s: %s - VCELL(%d), VFOCV(%d), VfSOC(%d), RepSOC(%d), "
		"current(%d), avg current(%d)\n",
		__func__, "After quick-start",
		max17042_get_vcell(chip->client),
		max17042_get_vfocv(chip->client),
		max17042_get_vfsoc(chip->client),
		max17042_get_soc(chip->client),
		max17042_get_current(chip->client),
		max17042_get_avg_current(chip->client));

	max17042_write_reg(chip->client, MAX17042_Cycles, 0x00a0);

	vfocv = max17042_get_vfocv(chip->client);
	if (vfocv < QUICKSTART_POWER_OFF_VOLTAGE) {
		dev_info(&chip->client->dev,
			"%s: Power off condition(%d)\n", __func__, vfocv);

		fullcap = max17042_read_reg(chip->client, MAX17402_FullCAP);
		/* FullCAP * 0.009 */
		max17042_write_reg(chip->client, MAX17042_RepCap,
			(u16)(fullcap * 9 / 1000));
		msleep(200);
		dev_info(&chip->client->dev, "%s : new soc=%d, vfocv=%d\n",
			__func__, max17042_get_soc(chip->client), vfocv);
	}

	dev_info(&chip->client->dev,
		"%s : Additional step - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
		__func__, max17042_get_vfocv(chip->client),
		max17042_get_vfsoc(chip->client),
		max17042_get_soc(chip->client));

	return 0;
}

static void max17042_set_battery_type(struct max17042_chip *chip)
{
	u16 data;

	data = max17042_read_reg(chip->client, MAX17042_DesignCap);

	if ((data == chip->pdata->sdi_vfcapacity) ||
			(data == chip->pdata->sdi_vfcapacity-1))
		chip->info.battery_type = SDI_BATTERY_TYPE;
	else if ((data == chip->pdata->byd_vfcapacity) ||
			(data == chip->pdata->byd_vfcapacity-1))
		chip->info.battery_type = BYD_BATTERY_TYPE;
	else {
		pr_info("%s : Unknown battery is set to SDI type.\n", __func__);
		chip->info.battery_type = SDI_BATTERY_TYPE;
	}

	pr_info("%s : DesignCAP(0x%04x), Battery type(%s)\n",
			__func__, data,
			chip->info.battery_type == SDI_BATTERY_TYPE ?
			"SDI_TYPE_BATTERY" : "BYD_TYPE_BATTERY");

	switch (chip->info.battery_type) {
	case BYD_BATTERY_TYPE:
		chip->info.capacity = chip->pdata->byd_capacity;
		chip->info.vfcapacity = chip->pdata->byd_vfcapacity;
		chip->info.check_start_vol =
			chip->pdata->sdi_low_bat_comp_start_vol;
		break;

	case SDI_BATTERY_TYPE:
	default:
		chip->info.capacity = chip->pdata->sdi_capacity;
		chip->info.vfcapacity = chip->pdata->sdi_vfcapacity;
		chip->info.check_start_vol =
			chip->pdata->byd_low_bat_comp_start_vol;
		break;
	}
}

static void max17042_periodic_read(struct i2c_client *client)
{
	u8 reg;
	int i;
	int data[0x10];
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	pr_debug("[MAX17042] %d/%d/%d %02d:%02d,",
			tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
			tm.tm_hour, tm.tm_min);

	for (i = 0; i < 16; i++) {
		for (reg = 0; reg < 0x10; reg++)
			data[reg] = max17042_read_reg(client, reg + i * 0x10);

		pr_debug("%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,"
				"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
				data[0x00], data[0x01], data[0x02], data[0x03],
				data[0x04], data[0x05], data[0x06], data[0x07],
				data[0x08], data[0x09], data[0x0a], data[0x0b],
				data[0x0c], data[0x0d], data[0x0e], data[0x0f]);
		if (i == 4)
			i = 13;
	}
	pr_debug("\n");
}

static void max17042_reset_capacity(struct max17042_fuelgauge_callbacks *ptr)
{
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);

	max17042_write_reg(chip->client,
		MAX17042_DesignCap, chip->info.vfcapacity-1);
}

static void max17042_adjust_capacity(struct max17042_fuelgauge_callbacks *ptr)
{
	u16 data = 0;
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);

	/* 1. Write RemCapREP(05h)=0; */
	max17042_write_reg(chip->client, MAX17042_RepCap, data);
	msleep(200);
	dev_info(&chip->client->dev,
		"%s : After adjust - RepSOC(%d)\n", __func__,
		max17042_get_soc(chip->client));

	chip->info.soc_restart_flag = 1;
}

int max17042_check_cap_corruption(struct max17042_fuelgauge_callbacks *ptr)
{
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);

	int vfsoc = max17042_get_vfsoc(chip->client);
	int repsoc = max17042_get_soc(chip->client);
	int mixcap = max17042_read_reg(chip->client, MAX17042_RemCap);
	int vfocv = max17042_read_reg(chip->client, MAX17042_VFOCV);
	int remcap = max17042_read_reg(chip->client, MAX17042_RepCap);
	int fullcapacity =
		max17042_read_reg(chip->client, MAX17402_FullCAP);
	int vffullcapacity =
		max17042_read_reg(chip->client, MAX17042_FullCAP_Nom);
	u32 temp, temp2, new_vfocv, pr_vfocv;
	int ret = 0;

	/* If usgin Jig or low batt compensation flag is set,
	   then skip checking. */
	if (chip->pdata->jig_on) {
		dev_info(&chip->client->dev,
			"%s : Return by Using Jig(%d)\n", __func__,
			chip->pdata->jig_on);
		return 0;
	}

	if (vfsoc < 0 || repsoc < 0 || mixcap < 0 || vfocv < 0 ||
			remcap < 0 || fullcapacity < 0 || vffullcapacity < 0)
		return 0;

	/* Check full charge learning case. */
	if (((vfsoc >= 70)
		&& ((remcap >= (fullcapacity * 995 / 1000))
		&& (remcap <= (fullcapacity * 1005 / 1000))))
		|| chip->info.low_batt_comp_flag
			|| chip->info.soc_restart_flag) {
		chip->info.previous_repsoc = repsoc;
		chip->info.previous_remcap = remcap;
		chip->info.previous_fullcapacity = fullcapacity;
		if (chip->info.soc_restart_flag)
			chip->info.soc_restart_flag = 0;

		ret = 1;
	}

	/* ocv calculation for print */
	temp = (vfocv & 0xFFF) * 78125;
	pr_vfocv = temp / 1000000;

	temp = ((vfocv & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	pr_vfocv += (temp2 << 4);

	/* MixCap differ is greater than 265mAh */
	if ((((vfsoc+5) < chip->info.previous_vfsoc)
		|| (vfsoc > (chip->info.previous_vfsoc+5)))
		|| (((repsoc+5) < chip->info.previous_repsoc)
		|| (repsoc > (chip->info.previous_repsoc+5)))
		|| (((mixcap+530) < chip->info.previous_mixcap)
			|| (mixcap > (chip->info.previous_mixcap+530)))) {
		max17042_periodic_read(chip->client);
		dev_info(&chip->client->dev,
			"[FG_Recovery] (B) VfSOC(%d), prevVfSOC(%d),",
			vfsoc, chip->info.previous_vfsoc);
		dev_info(&chip->client->dev,
			" RepSOC(%d), prevRepSOC(%d), MixCap(%d),",
			repsoc, chip->info.previous_repsoc, (mixcap/2));
		dev_info(&chip->client->dev,
			" prevMixCap(%d),VfOCV(0x%04x, %d)\n",
			(chip->info.previous_mixcap/2), vfocv, pr_vfocv);

		mutex_lock(&chip->fg_lock);
		max17042_write_reg(chip->client, MAX17042_RemCap,
			chip->info.previous_mixcap);
		max17042_write_reg(chip->client, MAX17042_VFOCV,
			chip->info.previous_vfocv);
		mdelay(200);

		max17042_write_reg(chip->client, MAX17042_RepCap,
			chip->info.previous_remcap);
		vfsoc = max17042_read_reg(chip->client, MAX17042_VFSOC);
		max17042_write_reg(chip->client, 0x60, 0x0080);
		max17042_write_reg(chip->client, 0x48, vfsoc);
		max17042_write_reg(chip->client, 0x60, 0x0000);

		max17042_write_reg(chip->client, 0x45,
				(chip->info.previous_vfcapacity / 4));
		max17042_write_reg(chip->client, 0x46, 0x3200);
		max17042_write_reg(chip->client, MAX17402_FullCAP,
				chip->info.previous_fullcapacity);
		max17042_write_reg(chip->client, MAX17042_FullCAP_Nom,
				chip->info.previous_vfcapacity);

		mutex_unlock(&chip->fg_lock);

		msleep(200);

		/* ocv calculation for print */
		new_vfocv = max17042_read_reg(chip->client, MAX17042_VFOCV);
		temp = (new_vfocv & 0xFFF) * 78125;
		pr_vfocv = temp / 1000000;

		temp = ((new_vfocv & 0xF000) >> 4) * 78125;
		temp2 = temp / 1000000;
		pr_vfocv += (temp2 << 4);

		dev_info(&chip->client->dev,
			"[FG_Recovery] (A) newVfSOC(%d), newRepSOC(%d),",
			max17042_get_vfsoc(chip->client),
			max17042_get_soc(chip->client));
		dev_info(&chip->client->dev,
			" newMixCap(%d), newVfOCV(0x%04x, %d)\n",
			(max17042_read_reg(chip->client, MAX17042_RemCap)/2),
			new_vfocv, pr_vfocv);

		max17042_periodic_read(chip->client);

		ret = 1;
	} else {
		chip->info.previous_vfsoc = vfsoc;
		chip->info.previous_repsoc = repsoc;
		chip->info.previous_remcap = remcap;
		chip->info.previous_mixcap = mixcap;
		chip->info.previous_fullcapacity = fullcapacity;
		chip->info.previous_vfcapacity = vffullcapacity;
		chip->info.previous_vfocv = vfocv;
	}

	return ret;
}

static void low_batt_compensation(struct i2c_client *client, u32 level)
{
	int read_val;
	u32 temp;

	dev_info(&client->dev, "%s : Adjust SOCrep to %d!!\n", __func__, level);

	read_val = max17042_read_reg(client, MAX17402_FullCAP);
	if (read_val < 0)
		return;

	if (read_val > 2)	/* 3% compensation */
		/* RemCapREP (05h) = FullCap(10h) x 0.0301 */
		temp = read_val * (level * 100 + 1) / 10000;
	else				/* 1% compensation */
		/* RemCapREP (05h) = FullCap(10h) x 0.0090 */
		temp = read_val * (level * 90) / 10000;

	max17042_write_reg(client, MAX17042_RepCap, (u16)temp);
	/* chip->info.low_batt_comp_flag = 1; */
}

#define POWER_OFF_SOC_HIGH_MARGIN	0x200
#define POWER_OFF_VOLTAGE_HIGH_MARGIN	3500000
static void prevent_early_poweroff(struct i2c_client *client,
			int vcell, int *fg_soc)
{
	int repsoc_data = 0;
	int read_val;
	int retry_cnt = 0;

	repsoc_data = max17042_read_reg(client, MAX17042_RepSOC);

	if (repsoc_data > POWER_OFF_SOC_HIGH_MARGIN)
		return;

	pr_info("%s : soc=%d%%(0x%04x), vcell=%d\n", __func__,
		repsoc_data >> 8, repsoc_data, vcell);

	if (vcell > POWER_OFF_VOLTAGE_HIGH_MARGIN) {
		read_val = max17042_read_reg(client, MAX17402_FullCAP);
rewrite:
		/* FullCAP * 0.019 */
		max17042_write_reg(client, MAX17042_RepCap,
			(u16)(read_val * 19 / 1000));

		msleep(200);
		*fg_soc = max17042_get_soc(client);
		if (*fg_soc < 1 && retry_cnt < 3) {
			retry_cnt++;
			goto rewrite;
		}

		pr_info("%s: new soc=%d, vcell=%d\n", __func__, *fg_soc, vcell);
	}
}

static void reset_low_batt_comp_cnt(struct max17042_chip *chip)
{
	memset(chip->info.low_batt_comp_cnt, 0,
			sizeof(chip->info.low_batt_comp_cnt));
}

static void add_low_batt_comp_cnt(struct max17042_chip *chip,
			int range, int level)
{
	int i;
	int j;

	/* Increase the requested count value, and reset others. */
	chip->info.low_batt_comp_cnt[range-1][level/2]++;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (i == range-1 && j == level/2)
				continue;
			else
				chip->info.low_batt_comp_cnt[i][j] = 0;
		}
	}
}

static int get_low_batt_threshold(int range, int level, int nCurrent)
{
	int ret = 0;

	switch (range) {
	/* P4 & P8 needs one more level */
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10)
	case 5:
		if (level == 1)
			ret = SDI_Range5_1_Offset +
			      ((nCurrent * SDI_Range5_1_Slope) / 1000);
		else if (level == 3)
			ret = SDI_Range5_3_Offset +
			      ((nCurrent * SDI_Range5_3_Slope) / 1000);
		break;
#endif
	case 4:
		if (level == 1)
			ret = SDI_Range4_1_Offset + \
			      ((nCurrent * SDI_Range4_1_Slope) / 1000);
		else if (level == 3)
			ret = SDI_Range4_3_Offset + \
			      ((nCurrent * SDI_Range4_3_Slope) / 1000);
		break;

	case 3:
		if (level == 1)
			ret = SDI_Range3_1_Offset + \
			      ((nCurrent * SDI_Range3_1_Slope) / 1000);
		else if (level == 3)
			ret = SDI_Range3_3_Offset + \
			      ((nCurrent * SDI_Range3_3_Slope) / 1000);
		break;

	case 2:
		if (level == 1)
			ret = SDI_Range2_1_Offset + \
			      ((nCurrent * SDI_Range2_1_Slope) / 1000);
		else if (level == 3)
			ret = SDI_Range2_3_Offset + \
			      ((nCurrent * SDI_Range2_3_Slope) / 1000);
		break;

	case 1:
		if (level == 1)
			ret = SDI_Range1_1_Offset + \
			      ((nCurrent * SDI_Range1_1_Slope) / 1000);
		else if (level == 3)
			ret = SDI_Range1_3_Offset + \
			      ((nCurrent * SDI_Range1_3_Slope) / 1000);
		break;

	default:
		break;
	}

	return ret;
}

static int check_low_batt_comp_condition(struct max17042_chip *chip,
		int *nLevel)
{
	int i;
	int j;
	int ret = 0;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (chip->info.low_batt_comp_cnt[i][j] \
					>= MAX_LOW_BATT_CHECK_CNT) {
				/* display_low_batt_comp_cnt(); */
				ret = 1;
				*nLevel = j*2 + 1;
				break;
			}
		}
	}

	return ret;
}

static int max17042_low_batt_compensation(
		struct max17042_fuelgauge_callbacks *ptr,
		struct bat_information bat_info)
{
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);
	int fg_avg_current = 0;
	int fg_min_current = 0;
	int new_level = 0;
	int bCntReset = 0;

	/* Not charging, flag is none, Under 3.60V or 3.45V */
	if (!chip->info.low_batt_comp_flag &&
			(bat_info.vcell <= chip->info.check_start_vol)) {
		fg_avg_current = max17042_get_avg_current(chip->client);
		fg_min_current = min(fg_avg_current, bat_info.fg_current);

		if (fg_min_current < CURRENT_RANGE_MAX) {
			if (bat_info.soc >= 2 &&
				bat_info.vcell < get_low_batt_threshold(
					CURRENT_RANGE_MAX_NUM,
					1, fg_min_current))
				add_low_batt_comp_cnt(chip,
					CURRENT_RANGE_MAX_NUM, 1);
			else if (bat_info.soc >= 4 &&
				bat_info.vcell < get_low_batt_threshold(
					CURRENT_RANGE_MAX_NUM,
					3, fg_min_current))
				add_low_batt_comp_cnt(chip,
					CURRENT_RANGE_MAX_NUM, 3);
			else
				bCntReset = 1;
		}
		/* P4 & P8 needs more level */
#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10)
		else if (fg_min_current >= CURRENT_RANGE5 &&
				fg_min_current < CURRENT_RANGE4) {
			if (bat_info.soc >= 2 && bat_info.vcell <
				get_low_batt_threshold(4, 1,
					fg_min_current))
				add_low_batt_comp_cnt(chip, 4, 1);
			else if (bat_info.soc >= 4 && bat_info.vcell <
				get_low_batt_threshold(4, 3,
					fg_min_current))
				add_low_batt_comp_cnt(chip, 4, 3);
			else
				bCntReset = 1;
		}
#endif
		else if (fg_min_current >= CURRENT_RANGE4 &&
				fg_min_current < CURRENT_RANGE3) {
			if (bat_info.soc >= 2 && bat_info.vcell <
				get_low_batt_threshold(3, 1,
					fg_min_current))

				add_low_batt_comp_cnt(chip, 3, 1);
			else if (bat_info.soc >= 4 && bat_info.vcell <
				get_low_batt_threshold(3, 3,
					fg_min_current))
				add_low_batt_comp_cnt(chip, 3, 3);
			else
				bCntReset = 1;
		} else if (fg_min_current >= CURRENT_RANGE3 &&
				fg_min_current < CURRENT_RANGE2) {
			if (bat_info.soc >= 2 && bat_info.vcell <
				get_low_batt_threshold(2, 1,
					fg_min_current))
				add_low_batt_comp_cnt(chip, 2, 1);
			else if (bat_info.soc >= 4 && bat_info.vcell <
				get_low_batt_threshold(2, 3,
					fg_min_current))
				add_low_batt_comp_cnt(chip, 2, 3);
			else
				bCntReset = 1;
		} else if (fg_min_current >= CURRENT_RANGE2 &&
				fg_min_current < CURRENT_RANGE1) {
			if (bat_info.soc >= 2 && bat_info.vcell <
				get_low_batt_threshold(1, 1,
					fg_min_current))
				add_low_batt_comp_cnt(chip, 1, 1);
			else if (bat_info.soc >= 4 && bat_info.vcell <
				get_low_batt_threshold(1, 3,
					fg_min_current))
				add_low_batt_comp_cnt(chip, 1, 3);
			else
				bCntReset = 1;
		}

		if (check_low_batt_comp_condition(chip, &new_level)) {
			/* Disable 3% low battery compensation */
			/* duplicated action with 1% low battery compensation */
			if (new_level < 2)
				low_batt_compensation(chip->client, new_level);

			reset_low_batt_comp_cnt(chip);
		}

		if (bCntReset)
			reset_low_batt_comp_cnt(chip);

		/* if compensation finished, then read SOC again!!*/
		if (chip->info.low_batt_comp_flag) {
			pr_info("%s : MIN_CURRENT(%d), AVG_CURRENT(%d),",
				__func__, fg_min_current, fg_avg_current);
			pr_info(" CURRENT(%d), SOC(%d), VCELL(%d)\n",
				bat_info.fg_current, bat_info.soc,
				bat_info.vcell);

			bat_info.soc = max17042_get_soc(chip->client);
			pr_info("%s : SOC is set to %d\n",
				__func__, bat_info.soc);
		}
	}
	/* Prevent power off over 3500mV */
	prevent_early_poweroff(chip->client, bat_info.vcell, &bat_info.soc);

	return bat_info.soc;
}

static int max17042_alert_init(struct i2c_client *client)
{
	u16 misccgf_data;
	u16 salrt_data;
	u16 config_data;
	u16 valrt_data;
	u16 talrt_data;
	u16 read_data = 0;

	/* Using RepSOC */
	misccgf_data = max17042_read_reg(client, MAX17042_MiscCFG);
	misccgf_data &= ~(0x03);

	if (max17042_write_reg(client, MAX17042_MiscCFG, misccgf_data) < 0) {
		dev_err(&client->dev,
			"%s: Failed to write MISCCFG_REG\n", __func__);
		return -1;
	}

	/* SALRT Threshold setting */
	salrt_data = 0xff01;
	if (max17042_write_reg(client, MAX17042_SALRT_Th, salrt_data) < 0) {
		dev_err(&client->dev,
			"%s: Failed to write SALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = max17042_read_reg(client, MAX17042_SALRT_Th);
	if (read_data != 0xff01)
		dev_err(&client->dev,
			"%s : SALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	/* Reset VALRT Threshold setting (disable) */
	valrt_data = 0xFF00;
	if (max17042_write_reg(client, MAX17042_VALRT_Th, valrt_data) < 0) {
		dev_err(&client->dev,
			"%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = max17042_read_reg(client, MAX17042_VALRT_Th);
	if (read_data != 0xff00)
		dev_err(&client->dev,
			"%s : VALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	/* Reset TALRT Threshold setting (disable) */
	talrt_data = 0x7F80;
	if (max17042_write_reg(client, MAX17042_TALRT_Th, talrt_data) < 0) {
		dev_err(&client->dev,
			"%s: Failed to write TALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = max17042_read_reg(client, MAX17042_TALRT_Th);
	if (read_data != 0x7f80)
		dev_err(&client->dev,
			"%s : TALRT_THRESHOLD_REG is not valid (0x%x)\n",
			__func__, read_data);

	mdelay(100);

	/* Enable SOC alerts */
	config_data = max17042_read_reg(client, MAX17042_CONFIG);
	config_data |= (0x1 << 2);

	if (max17042_write_reg(client, MAX17042_CONFIG, config_data) < 0) {
		dev_err(&client->dev,
			"%s: Failed to write CONFIG_REG\n", __func__);
		return -1;
	}

	return 1;
}

static int max17042_check_status_reg(struct i2c_client *client)
{
	u16 status_data;
	int ret = 0;

	/* 1. Check Smn was generatedread */
	status_data = max17042_read_reg(client, MAX17042_STATUS);
	dev_info(&client->dev, "%s - addr(0x00), data(0x%04x)\n",
			__func__, status_data);

	if (status_data & (0x1 << 10))
		ret = 1;

	/* 2. clear Status reg */
	status_data &= 0xFF;
	if (max17042_write_reg(client, MAX17042_STATUS, status_data) < 0) {
		dev_info(&client->dev,
			"%s: Failed to write STATUS_REG\n", __func__);
		return -1;
	}

	return ret;
}

static void max17042_fullcharged_compensation(
		struct max17042_fuelgauge_callbacks *ptr,
		u32 is_recharging, u32 pre_update)
{
	int new_fullcap;
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);

	dev_info(&chip->client->dev,
			"%s: is_recharging(%d), pre_update(%d)\n",
			__func__, is_recharging, pre_update);

	new_fullcap = max17042_read_reg(chip->client, MAX17402_FullCAP);

	if (new_fullcap < 0)
		new_fullcap = chip->info.capacity;

	if (new_fullcap > (chip->info.capacity * 110 / 100)) {
		dev_info(&chip->client->dev,
			"%s: [Case 1] previous_fullcap = 0x%04x,"
			" NewFullCap = 0x%04x\n",
			__func__, chip->info.previous_fullcap, new_fullcap);

		new_fullcap = (chip->info.capacity * 110) / 100;
		max17042_write_reg(chip->client,
			MAX17042_RepCap, (u16)(new_fullcap));
		max17042_write_reg(chip->client,
			MAX17402_FullCAP, (u16)(new_fullcap));
	} else if (new_fullcap < (chip->info.capacity * 50 / 100)) {
		dev_info(&chip->client->dev,
			"%s : [Case 5] previous_fullcap = 0x%04x,"
			" NewFullCap = 0x%04x\n",
			__func__, chip->info.previous_fullcap, new_fullcap);

		new_fullcap = (chip->info.capacity * 50) / 100;
		max17042_write_reg(chip->client,
			MAX17042_RepCap, (u16)(new_fullcap));
		max17042_write_reg(chip->client,
			MAX17402_FullCAP, (u16)(new_fullcap));
	} else {
		if (new_fullcap > (chip->info.previous_fullcap * 110 / 100)) {
			dev_info(&chip->client->dev,
				"%s : [Case 2] previous_fullcap = 0x%04x,"
				" NewFullCap = 0x%04x\n", __func__,
				chip->info.previous_fullcap, new_fullcap);

			new_fullcap = (chip->info.previous_fullcap * 110) / 100;
			max17042_write_reg(chip->client,
				MAX17042_RepCap, (u16)(new_fullcap));
			max17042_write_reg(chip->client,
				MAX17402_FullCAP, (u16)(new_fullcap));
		} else if (new_fullcap < (chip->info.previous_fullcap*90/100)) {
			dev_info(&chip->client->dev,
				"%s : [Case 3] previous_fullcap = 0x%04x,"
				" NewFullCap = 0x%04x\n", __func__,
				chip->info.previous_fullcap, new_fullcap);

			new_fullcap = (chip->info.previous_fullcap * 90) / 100;
			max17042_write_reg(chip->client,
				MAX17042_RepCap, (u16)(new_fullcap));
			max17042_write_reg(chip->client,
				MAX17402_FullCAP, (u16)(new_fullcap));
		} else {
			dev_info(&chip->client->dev,
				"%s : [Case 4] previous_fullcap = 0x%04x,"
				" NewFullCap = 0x%04x\n", __func__,
				chip->info.previous_fullcap, new_fullcap);
		}
	}

	/* 4. Write RepSOC(06h)=100%; */
	max17042_write_reg(chip->client,
		MAX17042_RepSOC, (u16)(0x64 << 8));

	/* 5. Write MixSOC(0Dh)=100%; */
	max17042_write_reg(chip->client,
		MAX17042_SOC, (u16)(0x64 << 8));

	/* 6. Write AVSOC(0Eh)=100%; */
	max17042_write_reg(chip->client,
		MAX17042_AvSOC, (u16)(0x64 << 8));

	/* if pre_update case, skip updating PrevFullCAP value. */
	if (!pre_update)
		chip->info.previous_fullcap =
			max17042_read_reg(chip->client, MAX17402_FullCAP);

	dev_info(&chip->client->dev, "%s : (A) FullCap = 0x%04x, RemCap = 0x%04x\n",
		 __func__,
		max17042_read_reg(chip->client, MAX17402_FullCAP),
		max17042_read_reg(chip->client, MAX17042_RepCap));

	max17042_periodic_read(chip->client);
}

static void max17042_check_vf_fullcap_range(
		struct max17042_fuelgauge_callbacks *ptr)
{
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);
	static int new_vffullcap;
	u16 print_flag = 1;

	new_vffullcap =
		max17042_read_reg(chip->client, MAX17042_FullCAP_Nom);
	if (new_vffullcap < 0)
		new_vffullcap = chip->info.vfcapacity;

	if (new_vffullcap > (chip->info.vfcapacity * 110 / 100)) {
		dev_info(&chip->client->dev,
			"%s : [Case 1] previous_vffullcap = 0x%04x,"
			" NewVfFullCap = 0x%04x\n", __func__,
			chip->info.previous_vffullcap, new_vffullcap);

		new_vffullcap = (chip->info.vfcapacity * 110) / 100;

		max17042_write_reg(chip->client, MAX17042_DQACC,
			(u16)(new_vffullcap / 4));
		max17042_write_reg(chip->client, MAX17042_DPACC, (u16)0x3200);
	} else if (new_vffullcap < (chip->info.vfcapacity * 50 / 100)) {
		dev_info(&chip->client->dev,
			"%s : [Case 5] previous_vffullcap = 0x%04x,"
			" NewVfFullCap = 0x%04x\n", __func__,
			chip->info.previous_vffullcap, new_vffullcap);

		new_vffullcap = (chip->info.vfcapacity * 50) / 100;

		max17042_write_reg(chip->client, MAX17042_DQACC,
			(u16)(new_vffullcap / 4));
		max17042_write_reg(chip->client, MAX17042_DPACC, (u16)0x3200);
	} else {
		if (new_vffullcap > (chip->info.previous_vffullcap*110 / 100)) {
			dev_info(&chip->client->dev,
				"%s : [Case 2] previous_vffullcap = 0x%04x,"
				" NewVfFullCap = 0x%04x\n", __func__,
				chip->info.previous_vffullcap, new_vffullcap);

			new_vffullcap =
				(chip->info.previous_vffullcap * 110) / 100;
			max17042_write_reg(chip->client, MAX17042_DQACC,
				(u16)(new_vffullcap / 4));
			max17042_write_reg(chip->client,
				MAX17042_DPACC, (u16)0x3200);
		} else if (new_vffullcap <
				(chip->info.previous_vffullcap * 90 / 100)) {
			dev_info(&chip->client->dev,
				"%s : [Case 3] previous_vffullcap = 0x%04x,"
				" NewVfFullCap = 0x%04x\n", __func__,
				chip->info.previous_vffullcap, new_vffullcap);

			new_vffullcap = (chip->info.previous_vffullcap*90)/100;

			max17042_write_reg(chip->client, MAX17042_DQACC,
				(u16)(new_vffullcap / 4));
			max17042_write_reg(chip->client,
				MAX17042_DPACC, (u16)0x3200);
		} else {
			dev_info(&chip->client->dev,
				"%s : [Case 4] previous_vffullcap = 0x%04x,"
				" NewVfFullCap = 0x%04x\n", __func__,
				chip->info.previous_vffullcap,
					new_vffullcap);
			print_flag = 0;
		}
	}

	/* delay for register setting (dQacc, dPacc) */
	if (print_flag)
		msleep(300);

	chip->info.previous_vffullcap =
			max17042_read_reg(chip->client, MAX17042_FullCAP_Nom);

	if (print_flag)
		dev_info(&chip->client->dev,
			"%s:VfFullCap(0x%04x), dQacc(0x%04x), dPacc(0x%04x)\n",
			__func__,
			max17042_read_reg(chip->client, MAX17042_FullCAP_Nom),
			max17042_read_reg(chip->client, MAX17042_DQACC),
			max17042_read_reg(chip->client, MAX17042_DPACC));

}

static void max17042_update_remcap_to_fullcap(
		struct max17042_fuelgauge_callbacks *ptr)
{
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);

	int remcap = max17042_read_reg(chip->client, MAX17042_RepCap);
	int fullcap = max17042_read_reg(chip->client, MAX17402_FullCAP);

	max17042_write_reg(chip->client, MAX17402_FullCAP, (u16)remcap);
	msleep(200);

	dev_info(&chip->client->dev,
		"Before FullCap : 0x%x, After Fullcap : 0x%x, SOC : %d\n",
		fullcap, max17042_read_reg(chip->client, MAX17402_FullCAP),
		max17042_get_soc(chip->client));
}

static int max17042_get_value(struct max17042_fuelgauge_callbacks *ptr,
		enum fuel_property fg_prop)
{
	int value;
	struct max17042_chip *chip =
		container_of(ptr, struct max17042_chip, callbacks);

	switch (fg_prop) {
	case READ_FG_VCELL:
		value = max17042_get_vcell(chip->client);
		dev_dbg(&chip->client->dev, "READ FG VCELL : %d\n",
				value);
		break;
	case READ_FG_SOC:
		value = max17042_get_soc(chip->client);
		dev_dbg(&chip->client->dev, "READ FG SOC : %d\n",
				value);
		break;
	case READ_FG_TEMP:
		value = max17042_get_temperature(chip->client);
		max17042_periodic_read(chip->client);
		dev_dbg(&chip->client->dev, "READ FG TEMPERATURE : %d\n",
			value);
		break;
	case READ_FG_CURRENT:
		value = max17042_get_current(chip->client);
		dev_dbg(&chip->client->dev, "READ FG CURRENT : %d\n",
				value);
		break;
	case READ_FG_AVG_CURRENT:
		value = max17042_get_avg_current(chip->client);
		dev_dbg(&chip->client->dev, "READ FG AVG_CURRENT : %d\n",
				value);
		break;
	case READ_FG_STATUS:
		value = max17042_check_status_reg(chip->client);
		dev_dbg(&chip->client->dev, "READ FG STATUS : %d\n",
			value);
		break;
	default:
		dev_info(&chip->client->dev, "UNKNOWN FUEL PROP : %d\n",
				fg_prop);
		return -EINVAL;
	}

	return value;
}

static int __devinit max17042_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17042_chip *chip;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;
	if (!chip->pdata) {
		dev_err(&client->dev, "No exist platform data\n");
		ret = -EINVAL;
		goto err_pdata;
	} else {
		max17042_set_battery_type(chip);
	}

	/* Init parameters to prevent wrong compensation. */
	chip->info.previous_fullcap =
			max17042_read_reg(client, MAX17402_FullCAP);
	chip->info.previous_vffullcap =
			max17042_read_reg(client, MAX17042_FullCAP_Nom);
	/* Init FullCAP of first full charging. */
	chip->info.full_charged_cap = chip->info.previous_fullcap;

	chip->info.previous_vfsoc = max17042_get_vfsoc(client);
	chip->info.previous_repsoc = max17042_get_soc(client);
	chip->info.previous_remcap = max17042_read_reg(client, MAX17042_RepCap);
	chip->info.previous_mixcap = max17042_read_reg(client, MAX17042_RemCap);
	chip->info.previous_vfocv = max17042_read_reg(client, MAX17042_VFOCV);
	chip->info.previous_fullcapacity = chip->info.previous_fullcap;
	chip->info.previous_vfcapacity = chip->info.previous_vffullcap;

	i2c_set_clientdata(client, chip);

	chip->callbacks.get_value = max17042_get_value;
	chip->callbacks.fg_reset_soc = max17042_reset_soc;
	chip->callbacks.set_adjust_capacity = max17042_adjust_capacity;
	chip->callbacks.reset_capacity = max17042_reset_capacity;
	chip->callbacks.check_low_batt_compensation =
				max17042_low_batt_compensation;
	chip->callbacks.full_charged_compensation =
				max17042_fullcharged_compensation;
	chip->callbacks.check_vf_fullcap_range =
				max17042_check_vf_fullcap_range;
	chip->callbacks.check_cap_corruption =
				max17042_check_cap_corruption;
	chip->callbacks.update_remcap_to_fullcap =
				max17042_update_remcap_to_fullcap;
	chip->callbacks.get_register_value = max17042_get_register;

	if (chip->pdata->register_callbacks)
		chip->pdata->register_callbacks(&chip->callbacks);

	if (!chip->pdata->enable_current_sense) {
		max17042_write_reg(client, MAX17042_CGAIN, 0x0000);
		max17042_write_reg(client, MAX17042_MiscCFG, 0x0003);
		max17042_write_reg(client, MAX17042_LearnCFG, 0x0007);
	}

	mutex_init(&chip->fg_lock);

	max17042_write_reg(client, MAX17042_CONFIG, 0x2214);
	max17042_periodic_read(client);

	max17042_alert_init(client);
	dev_info(&client->dev, "%s: probe done\n", __func__);

	return 0;

err_pdata:
	kfree(chip);

	return ret;
}

static int __devexit max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	mutex_destroy(&chip->fg_lock);
	kfree(chip);
	return 0;
}

static const struct i2c_device_id max17042_id[] = {
	{ "max17042", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17042_id);

static struct i2c_driver max17042_i2c_driver = {
	.driver	= {
		.name	= "max17042",
	},
	.probe		= max17042_probe,
	.remove		= __devexit_p(max17042_remove),
	.id_table	= max17042_id,
};

static int __init max17042_init(void)
{
	return i2c_add_driver(&max17042_i2c_driver);
}
module_init(max17042_init);

static void __exit max17042_exit(void)
{
	i2c_del_driver(&max17042_i2c_driver);
}
module_exit(max17042_exit);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");

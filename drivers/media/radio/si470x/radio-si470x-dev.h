/*
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

#ifndef __RADIO_SI470X_DEV_H__
#define	__RADIO_SI470X_DEV_H__

/******************************************************************************
 * Additional Register Definitions
 ******************************************************************************/

#define SYSCONFIG1_GPO_LOW		0x0008	/* bits [1:0]=10 Low */

#define SYSCONFIG2_SPACE_100KHZ		0x0010	/*bits 05..04-> Europe,Japan */
#define SYSCONFIG2_SPACE_50KHZ		0x0020	/*bits 05..04-> */
#define SYSCONFIG2_BAND_875MHZ		0x00C0	/*bits 07..06->band US/EUR */
#define SYSCONFIG2_BAND_76MHZ		0x0040	/*bits 07..06->band Japan */
#define SYSCONFIG2_BAND_SPA_VOL		0x00ff	/*bits 07..00->
						max val of BAND,SPACE,VOLUME */
#define SYSCONFIG2_VOLUME_CLR		0xfff0	/*bits 03..00->
						   just to clear volume */

#define SYSCONFIG3_SKSNR_MIN		0x0001	/*bits 07..04->
						   for most stops */
#define SYSCONFIG3_SKNR_CLR		0xff0f	/*bits 07..04->
						   just to clear the SKNR */
#define SYSCONFIG3_SKCNT_CLR		0xfff0	/*bits 03..00->
						   just to clear the SKNT */
#define SYSCONFIG3_VOLEXT_EN		0x0100	/*bit 08 Extended Vol range ->
						   enable */

#define SYSCONFIG3_SKSNR_MIN4         0x0040   /*bits 07..04-> for sufficient
						stops */
/*****************************************************************************
* RDS DATA Struct
*****************************************************************************/
struct radio_data {
	unsigned short rdsa;
	unsigned short rdsb;
	unsigned short rdsc;
	unsigned short rdsd;
	unsigned char cur_rssi;
	unsigned int cur_chan;
	unsigned char blera;
	unsigned char blerb;
	unsigned char blerc;
	unsigned char blerd;
};


/******************************************************************************
 * IOCTL
 ******************************************************************************/

#define SI470X_IOC_MAGIC		0xFA
#define SI470X_IOC_NR_MAX		40

#define SI470X_IOC_POWERUP		_IO(SI470X_IOC_MAGIC, 0)
#define SI470X_IOC_POWERDOWN		_IO(SI470X_IOC_MAGIC, 1)
#define SI470X_IOC_BAND_SET		_IOW(SI470X_IOC_MAGIC, 2, int)
#define SI470X_IOC_CHAN_SPACING_SET	_IOW(SI470X_IOC_MAGIC, 3, int)
#define SI470X_IOC_CHAN_SELECT		_IOW(SI470X_IOC_MAGIC, 4, u32)
#define SI470X_IOC_CHAN_GET		_IOR(SI470X_IOC_MAGIC, 5, u32)
#define SI470X_IOC_SEEK_UP		_IOR(SI470X_IOC_MAGIC, 6, u32)
#define SI470X_IOC_SEEK_DOWN		_IOR(SI470X_IOC_MAGIC, 7, u32)
#define SI470X_IOC_RSSI_SEEK_TH_SET	_IOW(SI470X_IOC_MAGIC, 9, u8)
#define SI470X_IOC_SEEK_SNR_SET		_IOW(SI470X_IOC_MAGIC, 10, u8)
#define SI470X_IOC_SEEK_CNT_SET		_IOW(SI470X_IOC_MAGIC, 11, u8)
#define SI470X_IOC_VOLEXT_ENB		_IO(SI470X_IOC_MAGIC, 12)
#define SI470X_IOC_VOLEXT_DISB		_IO(SI470X_IOC_MAGIC, 13)
#define SI470X_IOC_VOLUME_SET		_IOW(SI470X_IOC_MAGIC, 14, u8)
#define SI470X_IOC_VOLUME_GET		_IOR(SI470X_IOC_MAGIC, 15, u8)
#define SI470X_IOC_MUTE_ON		_IO(SI470X_IOC_MAGIC, 16)
#define SI470X_IOC_MUTE_OFF		_IO(SI470X_IOC_MAGIC, 17)
#define SI470X_IOC_MONO_SET		_IO(SI470X_IOC_MAGIC, 18)
#define SI470X_IOC_STEREO_SET		_IO(SI470X_IOC_MAGIC, 19)
#define SI470X_IOC_RDS_ENABLE		_IO(SI470X_IOC_MAGIC, 20)
#define SI470X_IOC_RDS_DISABLE		_IO(SI470X_IOC_MAGIC, 21)
#define SI470X_IOC_DSMUTE_ON		_IOW(SI470X_IOC_MAGIC, 22, u8)
#define SI470X_IOC_DSMUTE_OFF		_IOW(SI470X_IOC_MAGIC, 23, u8)
#define SI470X_IOC_DE_SET		_IOW(SI470X_IOC_MAGIC, 24, u8)
#define SI470X_IOC_RSSI_GET             _IOR(SI470X_IOC_MAGIC, 25, u32)
#define SI470X_IOC_SKSNR_GET            _IOR(SI470X_IOC_MAGIC, 26, u32)
#define SI470X_IOC_SKCNT_GET            _IOR(SI470X_IOC_MAGIC, 27, u32)
#define SI470X_IOC_AFCRL_GET            _IOR(SI470X_IOC_MAGIC, 28, u8)
#define SI470X_IOC_STATUS_RSSI_GET      _IOR(SI470X_IOC_MAGIC, 29, u32)
#define SI470X_IOC_RDS_GET		_IOR(SI470X_IOC_MAGIC, 30, \
							struct radio_data)

/*dev settings*/
/*band*/
#define BAND_87500_108000_kHz		1
#define BAND_76000_108000_kHz		2
#define BAND_76000_90000_kHz		3

/*channel spacing*/
#define CHAN_SPACING_200_kHz		20	/*US*/
#define CHAN_SPACING_100_kHz		10	/*Europe,Japan */
#define CHAN_SPACING_50_kHz		5

/*DE-emphasis Time Constant*/
#define DE_TIME_CONSTANT_50		1
/*Europe,Japan,Australia */
#define DE_TIME_CONSTANT_75		0	/*US*/

#define FREQ_87500_kHz			8750
#define FREQ_76000_kHz			7600

/* RDS Buffer length */
#define RDS_BUF_LEN                      50

/* function to create control node */
int si470x_dev_make_node(struct si470x_device *radio,
			 struct i2c_client *client);
/* function to create rds buff */
int si470x_dev_rdsbuff_init(struct si470x_device *radio);

#endif /* __RADIO_SI470X_DEV_H__ */

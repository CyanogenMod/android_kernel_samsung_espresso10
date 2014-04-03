/*
 *  Copyright (C) 2011 STMicroelectronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STC3115_BATTERY_H_
#define __STC3115_BATTERY_H_

#include <linux/battery.h>

#define GG_VERSION "1.05a"



struct fuelgauge_callbacks {
	int (*get_value)(struct fuelgauge_callbacks *ptr,
		enum fuel_property fg_prop);
};

struct stc311x_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
	int (*power_supply_register)(struct device *parent,
		struct power_supply *psy);
	void (*power_supply_unregister)(struct power_supply *psy);
	void (*register_callbacks)(struct fuelgauge_callbacks *ptr);

	int Vmode;       /* 1=Voltage mode, 0=mixed mode */
	int Alm_SOC;     /* SOC alm level %*/
	int Alm_Vbat;    /* Vbat alm level mV*/
	int CC_cnf;      /* nominal CC_cnf */
	int VM_cnf;      /* nominal VM cnf */
	int Cnom;        /* nominal capacity in mAh */
	int Rsense;      /* sense resistor mOhms*/
	int RelaxCurrent; /* current for relaxation in mA (< C/20) */
	/* 1=Adaptive mode enabled, 0=Adaptive mode disabled */
	int Adaptive;
	/* capacity derating in 0.1% */
	/* for temp = 60, 40, 25, 10,   0, -10 C,-20C */
	int CapDerating[7];
	int OCVOffset[16];    /* OCV curve adjustment */
	/*External temperature fonction, return C*/
	int (*ExternalTemperature) (void);
	/* 1=External temperature, 0=STC3115 temperature */
	int ForceExternalTemperature;
};

/*Function declaration*/
int STC31xx_SetPowerSavingMode(void);
int STC31xx_StopPowerSavingMode(void);
int STC31xx_AlarmSet(void);
int STC31xx_AlarmStop(void);
int STC31xx_AlarmGet(void);
int STC31xx_AlarmClear(void);
int STC31xx_AlarmSetVoltageThreshold(int VoltThresh);
int STC31xx_AlarmSetSOCThreshold(int SOCThresh);
int STC31xx_RelaxTmrSet(int CurrentThreshold);

/* -------------------------------------- */
/*            STC311x DEVICE SELECTION              */
/*            STC3115 version only                         */
/* -------------------------------------- */
#define STC3115
#define BATD_UC8
/* ************************************** */

/* Private define ---------------------------*/

/* ************************************** */
/*        SPECIAL FUNCTIONS                              */
/* -------------------------------------- */
/*                                                                    */
/* define TEMPCOMP_SOC to enable
SOC temperature compensation */
#define TEMPCOMP_SOC

/* ************************************** */

/* ************************************** */
/*        INTERNAL PARAMETERS                          */
/*   TO BE ADJUSTED ACCORDING TO BATTERY/AP
     PLICATION CHARACTERISTICS                      */
/* -------------------------------------- */
/*                                        */
/* min voltage at the end of the charge (mV)      */
#define BATT_CHG_VOLTAGE   4350
/* empty battery detection level (mV)                 */
#define BATT_MIN_VOLTAGE   3300
/* 100% in 1/512% units*/
#define MAX_HRSOC          51200
#define MAX_SOC            1000
/*                                                                    */
 /* min charge current in mA */
#define CHG_MIN_CURRENT     150
/* end charge current in mA */
#define CHG_END_CURRENT      20
/*minimum application current consumption in mA*/
#define APP_MIN_CURRENT     (-5)
/* application cut-off voltage                    */
#define APP_MIN_VOLTAGE	    3000
/* minimum temperature for gain adjustment */
#define TEMP_MIN_ADJ		 (-5)

/* normalized VM_CNF at 60, 40, 25, 10, 0, -10, -20 C */
#define VMTEMPTABLE        { 85, 90, 100, 160, 320, 440, 840 }

#define AVGFILTER           4  /* average filter constant */

/* **************************************** */

/* Private define ---------------------------- */

/* STC31xx 8-bit address byte */
#define STC31xx_SLAVE_ADDRESS            0xE0

/*Address of the STC311x register ------------- */

/* Mode Register             */
#define STC311x_REG_MODE     0x00
/* Control and Status Register */
#define STC311x_REG_CTRL      0x01
/* SOC Data (2 bytes) */
#define STC311x_REG_SOC     0x02
/* Number of Conversion (2 bytes) */
#define STC311x REG_COUNTER     0x04
/* Battery Current (2 bytes) */
#define STC311x_REG_CURRENT   0x06
/* Battery Voltage (2 bytes) */
#define STC311x_REG_VOLTAGE   0x08
/* Temperature               */
#define STC311x_REG_TEMPERATURE    0x0A
/* CC adjustement     */
#define STC311x_REG_CC_ADJ     0x0B
/* VM adjustement     */
#define STC311x_REG_VM_ADJ     0x0C
/* Battery OCV (2 bytes) */
#define STC311x_REG_OCV      0x0D
/* CC configuration (2 bytes)    */
#define STC311x_REG_CC_CNF      0x0F
/* VM configuration (2 bytes)    */
#define STC311x_REG_VM_CNF      0x11
 /* SOC alarm level         */
#define STC311x_REG_ALARM_SOC     0x13
/* Low voltage alarm level */
#define STC311x_REG_ALARM_VOLTAGE    0x14
 /* Current threshold for relaxation */
#define STC311x_REG_CURRENT_THRES     0x15
 /* Voltage relaxation counter   */
#define STC311x_REG_RELAX_COUNT      0x16
/* Voltage relaxation max count */
#define STC311x_REG_RELAX_MAX       0x17

/*Bit mask definition*/

/* Voltage mode bit mask     */
#define STC311x_VMODE     0x01
/* Alarm enable bit mask     */
#define STC311x_ALM_ENA    0x08
/* Alarm enable bit mask     */
#define STC311x_GG_RUN	    0x10
/* Force CC bit mask     */
#define STC311x_FORCE_CC	0x20
 /* Force VM bit mask     */
#define STC311x_FORCE_VM	0x40
 /* soft reset     */
#define STC311x_SOFTPOR	0x11

/* Chip ID (1 byte)       */
#define STC311x_REG_ID      0x18
/* STC3115 ID */
#define STC311x_ID      0x13

/* General Purpose RAM Registers */
#define STC311x_REG_RAM                  0x20
/* Total RAM size of STC3115 in bytes */
#define RAM_SIZE                         16

/* OCVTAB size of STC3115 in bytes */
#define STC311x_REG_OCVTAB               0x30
#define OCVTAB_SIZE                      16

/* counter value for 1st current/temp measurements */
#define VCOUNT				 4

/* GG_RUN & PORDET mask in STC311x_BattDataTypeDef status word */
#define M_STAT 0x1010
/* BATFAIL & PORDET mask */
#define M_RST  0x1800
/* GG_RUN mask in STC311x_BattDataTypeDef status word */
#define M_RUN  0x0010
/* GG_VM mask */
#define M_GGVM 0x0400
/* BATFAIL mask*/
#define M_BATFAIL 0x0800
/* VMODE mask */
#define M_VMOD 0x0001

#define OK 0

/* Battery charge state definition for BattState */
#define  BATT_CHARGING  3
#define  BATT_ENDCHARG  2
#define  BATT_FULCHARG  1
#define  BATT_IDLE      0
#define  BATT_DISCHARG (-1)
#define  BATT_LOWBATT  (-2)

/* STC311x RAM test word */
#define RAM_TSTWORD 0x53A9

/* Gas gauge states */
#define GG_INIT     'I'
#define GG_RUNNING  'R'
#define GG_POWERDN  'D'

#define VM_MODE 1
#define CC_MODE 0

#endif

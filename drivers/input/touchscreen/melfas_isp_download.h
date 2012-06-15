/* Melfas MMS100 Series Download base v1.0 2010.04.05 */
#ifndef __MELFAS_FIRMWARE_DOWNLOAD_H__
#define __MELFAS_FIRMWARE_DOWNLOAD_H__

#include <linux/gpio.h>
#include <linux/platform_data/melfas_ts.h>

/* MELFAS Firmware download pharameters
 * MELFAS_TRANSFER_LENGTH is fixed value.
 */
#define MELFAS_TRANSFER_LENGTH				(32/8)
#define MELFAS_FIRMWARE_MAX_SIZE			(32*1024)

/* 0 : 1Chip Download,
 * 1 : 2Chip Download
 */
#define MELFAS_2CHIP_DOWNLOAD_ENABLE			0

#define ISP_MODE_ERASE_FLASH				0x01
#define ISP_MODE_SERIAL_WRITE				0x02
#define ISP_MODE_SERIAL_READ				0x03
#define ISP_MODE_NEXT_CHIP_BYPASS			0x04

/* Return values of download function */
#define MCSDL_RET_SUCCESS				0x00
#define MCSDL_RET_ERASE_FLASH_VERIFY_FAILED		0x01
#define MCSDL_RET_PROGRAM_VERIFY_FAILED			0x02

#define MCSDL_RET_PROGRAM_SIZE_IS_WRONG			0x10
#define MCSDL_RET_VERIFY_SIZE_IS_WRONG			0x11
#define MCSDL_RET_WRONG_BINARY				0x12

#define MCSDL_RET_READING_HEXFILE_FAILED		0x21
#define MCSDL_RET_FILE_ACCESS_FAILED			0x22
#define MCSDL_RET_MELLOC_FAILED				0x23

#define MCSDL_RET_WRONG_MODULE_REVISION			0x30

/* When you can't control VDD nor CE.
 * Set this value 1
 * Then Melfas Chip can prepare chip reset.
 */
#define MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD	0

#define MCSDL_USE_VDD_CONTROL				1
#define MCSDL_USE_RESETB_CONTROL			1

#define GPIO_TSP_INT			tsp_gpio_set[GPIO_TOUCH_nINT].gpio
#define GPIO_TSP_EN			tsp_gpio_set[GPIO_TOUCH_EN].gpio
#define GPIO_TSP_SCL			tsp_gpio_set[GPIO_TOUCH_SCL].gpio
#define GPIO_TSP_SDA			tsp_gpio_set[GPIO_TOUCH_SDA].gpio

/* Touch Screen Interface Specification Multi Touch (V0.5)
 * Registers
 *
 * MCSTS_STATUS_REG        : Status
 * MCSTS_MODE_CONTROL_REG  : Mode Control
 * MCSTS_RESOL_HIGH_REG    : Resolution(High Byte)
 * MCSTS_RESOL_X_LOW_REG   : Resolution(X Low Byte)
 * MCSTS_RESOL_Y_LOW_REG   : Resolution(Y Low Byte)
 * MCSTS_INPUT_INFO_REG    : Input Information
 * MCSTS_POINT_HIGH_REG    : Point(High Byte)
 * MCSTS_POINT_X_LOW_REG   : Point(X Low Byte)
 * MCSTS_POINT_Y_LOW_REG   : Point(Y Low Byte)
 * MCSTS_STRENGTH_REG      : Strength
 * MCSTS_MODULE_VER_REG    : H/W Module Revision
 * MCSTS_FIRMWARE_VER_REG  : F/W Version
 */
#define MCSTS_STATUS_REG				0x00
#define MCSTS_MODE_CONTROL_REG				0x01
#define MCSTS_RESOL_HIGH_REG				0x02
#define MCSTS_RESOL_X_LOW_REG				0x08
#define MCSTS_RESOL_Y_LOW_REG				0x0a
#define MCSTS_INPUT_INFO_REG				0x10
#define MCSTS_POINT_HIGH_REG				0x11
#define MCSTS_POINT_X_LOW_REG				0x12
#define MCSTS_POINT_Y_LOW_REG				0x13
#define MCSTS_STRENGTH_REG				0x14
#define MCSTS_MODULE_VER_REG				0x30
#define MCSTS_FIRMWARE_VER_REG				0x31

/* Porting factors for Baseband */
#include "melfas_isp_download_porting.h"

/* Intenal fw. version */
extern const u32 FW_VERSION;

/* mcsdl_download_binary_data : with binary type .c   file.
 * mcsdl_download_binary_file : with binary type .bin file.
 */
int mcsdl_download_binary_data(struct gpio *);
int mcsdl_download_binary_file(struct gpio *);
void mcsdl_vdd_on(void);
void mcsdl_vdd_off(void);

#endif

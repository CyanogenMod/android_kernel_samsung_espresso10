/* Melfas MCS8000 Series Download base v1.0 2010.04.05 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/preempt.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

/* FW header file */
#include "melfas_espresso_fw.h"

#include "melfas_isp_download.h"

/* You may malloc *ucVerifyBuffer instead of this */
u8 ucVerifyBuffer[MELFAS_TRANSFER_LENGTH];
static struct gpio *tsp_gpio_set;

static int mcsdl_download(const u8 *pData, const u16 nLength, char IdxNum);
static void mcsdl_set_ready(void);
static void mcsdl_reboot_mcs(void);
static int mcsdl_erase_flash(char IdxNum);
static int mcsdl_program_flash(u8 *pDataOriginal, u16 unLength, char IdxNum);
static void mcsdl_program_flash_part(u8 *pData);
static int mcsdl_verify_flash(u8 *pData, u16 nLength, char IdxNum);
static void mcsdl_read_flash(u8 *pBuffer);
static int mcsdl_read_flash_from(u8 *pBuffer, u16 unStart_addr, u16 unLength,
				char IdxNum);
static void mcsdl_select_isp_mode(u8 ucMode);
static void mcsdl_unselect_isp_mode(void);
static void mcsdl_read_32bits(u8 *pData);
static void mcsdl_write_bits(u32 wordData, int nBits);
static void mcsdl_scl_toggle_twice(void);

static void mcsdl_delay(u32 nCount);

#if MELFAS_ENABLE_DBG_PRINT
static void mcsdl_print_result(int nRet);
#endif

/* Main Download Function */
int mcsdl_download_binary_data(struct gpio *tsp_gpio)
{
	int nRet;
	tsp_gpio_set = tsp_gpio;

#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
	melfas_send_download_enable_command();
	mcsdl_delay(MCSDL_DELAY_100US);
#endif
	MELFAS_DISABLE_BASEBAND_ISR();
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();

	nRet = mcsdl_download((const u8 *)MELFAS_binary,
				(const u16)MELFAS_binary_nLength, 0);
#if MELFAS_2CHIP_DOWNLOAD_ENABLE
	nRet = mcsdl_download((const u8 *)MELFAS_binary_2,
				(const u16)MELFAS_binary_nLength_2, 1);
#endif

	MELFAS_ROLLBACK_BASEBAND_ISR();
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();
	return (nRet == MCSDL_RET_SUCCESS);
}

/* Download FW. from bin. file */
int mcsdl_download_binary_file(struct gpio *tsp_gpio)
{
	int i = 0, ret = 0;
	long fw_size = 0;
	unsigned char *fw_data;
	struct file *filp;
	loff_t pos;
	mm_segment_t oldfs;

	tsp_gpio_set = tsp_gpio;

	MELFAS_DISABLE_BASEBAND_ISR();
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET();

	oldfs = get_fs();
	set_fs(get_ds());

	filp = filp_open("/sdcard/melfas_fw.bin", O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("file open error:%d\n", (s32)filp);
		return -1;
	}

	fw_size = filp->f_path.dentry->d_inode->i_size;
	pr_info("Size of the file : %ld(bytes)\n", fw_size);

	fw_data = kzalloc(fw_size, GFP_KERNEL);
	memset(fw_data, 0, fw_size);

	pos = 0;
	ret = vfs_read(filp, (char __user *)fw_data, fw_size, &pos);
	if (ret != fw_size) {
		pr_err("Failed to read file (ret = %d)\n", ret);
		kfree(fw_data);
		filp_close(filp, current->files);
		return 0;
	}

	filp_close(filp, current->files);

	set_fs(oldfs);

	for (i = 0; i < 3; i++) {
		pr_info("tsp fw: ADB - Firmware update! try : %d", i + 1);
		ret = mcsdl_download(fw_data, (u16)fw_size, 0);
		if (ret != MCSDL_RET_SUCCESS)
			continue;
		break;
	}

	kfree(fw_data);

	MELFAS_ROLLBACK_BASEBAND_ISR();
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET();

#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_print_result(ret);
#endif
	return (ret == MCSDL_RET_SUCCESS);
}

static int mcsdl_download(const u8 *pBianry, const u16 unLength, char IdxNum)
{
	int i;
	int nRet;
	u8 Check_IC = 0xFF;
	u8 readBuffer[32];
	u8 writeBuffer[4];
	u32 wordData;

	/* Check Binary Size */
	if (unLength >= MELFAS_FIRMWARE_MAX_SIZE) {
		nRet = MCSDL_RET_PROGRAM_SIZE_IS_WRONG;
		goto mcsdl_download_finish;
	}
#if MELFAS_ENABLE_DBG_PRINT
	pr_info("tsp fw: Melfas FW download : Starting download...\n");
#endif
	mcsdl_set_ready();
	mcsdl_delay(MCSDL_DELAY_20MS);

	/* IC Information read from Flash */
	if (IdxNum == 0) {
		mcsdl_select_isp_mode(ISP_MODE_SERIAL_READ);
		wordData = ((0x1F00 & 0x1FFF) << 1) | 0x0;
		wordData <<= 14;
		mcsdl_write_bits(wordData, 18);
		mcsdl_read_flash(readBuffer);
		MCSDL_GPIO_SDA_SET_LOW();
		MCSDL_GPIO_SDA_SET_OUTPUT(0);
		mcsdl_unselect_isp_mode();

		Check_IC = readBuffer[3];
#if MELFAS_ENABLE_DBG_PRINT
		pr_info("tsp fw: Melfas FW download : IC Information :0x%02X, 0x%02X\n",
			readBuffer[3], Check_IC);
#endif
		mcsdl_delay(MCSDL_DELAY_20MS);
	}
#if MELFAS_ENABLE_DBG_PRINT
	pr_info("tsp fw: Melfas FW download : Erase\n");
#endif
	/* Erase Flash */
	preempt_disable();
	nRet = mcsdl_erase_flash(IdxNum);
	preempt_enable();
	if (nRet != MCSDL_RET_SUCCESS)
		goto mcsdl_download_finish;

	mcsdl_delay(MCSDL_DELAY_20MS);
	/* IC Information write */
	if (IdxNum == 0) {
		mcsdl_select_isp_mode(ISP_MODE_SERIAL_WRITE);

		wordData = ((0x1F00 & 0x1FFF) << 1) | 0x0;
		wordData = wordData << 14;
		mcsdl_write_bits(wordData, 18);

		writeBuffer[0] = Check_IC;
		writeBuffer[1] = 0xFF;
		writeBuffer[2] = 0xFF;
		writeBuffer[3] = 0xFF;
		mcsdl_program_flash_part(writeBuffer);

		MCSDL_GPIO_SDA_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_40US);

		for (i = 0; i < 6; i++) {
			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_20US);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_40US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_10US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_10US);
		}
		MCSDL_GPIO_SDA_SET_LOW();

		mcsdl_unselect_isp_mode();
		mcsdl_delay(MCSDL_DELAY_20MS);
	}

#if MELFAS_ENABLE_DBG_PRINT
	pr_info("tsp fw: Melfas FW download : F/W Flash\n");
#endif

	preempt_disable();
	nRet = mcsdl_program_flash((u8 *)pBianry, (u16)unLength, IdxNum);
	preempt_enable();

	if (nRet != MCSDL_RET_SUCCESS)
		goto mcsdl_download_finish;
	mcsdl_delay(MCSDL_DELAY_20MS);

#if MELFAS_ENABLE_DBG_PRINT
	pr_info("tsp fw: Melfas FW download : F/W Verify\n");
#endif
	preempt_disable();
	nRet = mcsdl_verify_flash((u8 *)pBianry, (u16)unLength, IdxNum);
	preempt_enable();
	if (nRet != MCSDL_RET_SUCCESS)
		goto mcsdl_download_finish;
	mcsdl_delay(MCSDL_DELAY_20MS);
	nRet = MCSDL_RET_SUCCESS;

mcsdl_download_finish:

#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_print_result(nRet);
#endif

#if MELFAS_ENABLE_DBG_PRINT
	pr_info("tsp fw: Melfas FW download : Rebooting\n");
	pr_info("tsp fw: Melfas FW download : Fin.\n\n");
#endif
	mcsdl_reboot_mcs();

	return nRet;
}

static int mcsdl_erase_flash(char IdxNum)
{
	if (IdxNum > 0) {
		mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
		mcsdl_delay(MCSDL_DELAY_3US);
	}
	mcsdl_select_isp_mode(ISP_MODE_ERASE_FLASH);
	mcsdl_unselect_isp_mode();

	return MCSDL_RET_SUCCESS;
}

static int mcsdl_program_flash(u8 *pDataOriginal, u16 unLength,
			       char IdxNum)
{
	int i;

	u8 *pData;
	u8 ucLength;

	u16 addr;
	u32 header;

	addr = 0;
	pData = pDataOriginal;
	ucLength = MELFAS_TRANSFER_LENGTH;

	while ((addr * 4) < (int)unLength) {
		if ((unLength - (addr * 4)) < MELFAS_TRANSFER_LENGTH)
			ucLength = (u8) (unLength - (addr * 4));

		/* Select ISP Mode */
		mcsdl_delay(MCSDL_DELAY_40US);
		if (IdxNum > 0) {
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
			mcsdl_delay(MCSDL_DELAY_3US);
		}
		mcsdl_select_isp_mode(ISP_MODE_SERIAL_WRITE);

		/* Header */
		header = ((addr & 0x1FFF) << 1) | 0x0;
		header = header << 14;
		mcsdl_write_bits(header, 18);

		/* Writing */
		addr += 1;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		printk(KERN_INFO "#");
#endif
		mcsdl_program_flash_part(pData);
		pData += ucLength;

		/* Tail */
		MCSDL_GPIO_SDA_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_40US);

		for (i = 0; i < 6; i++) {
			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_20US);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_40US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_10US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_10US);
		}
		MCSDL_GPIO_SDA_SET_LOW();

		mcsdl_unselect_isp_mode();
		mcsdl_delay(MCSDL_DELAY_300US);
	}

	return MCSDL_RET_SUCCESS;
}

static void mcsdl_program_flash_part(u8 *pData)
{
	u32 data;

	data = (u32) pData[0] << 0;
	data |= (u32) pData[1] << 8;
	data |= (u32) pData[2] << 16;
	data |= (u32) pData[3] << 24;
	mcsdl_write_bits(data, 32);
}

static int mcsdl_verify_flash(u8 *pDataOriginal, u16 unLength,
			      char IdxNum)
{
	int i;
	int nRet;

	u8 *pData;
	u8 ucLength;

	u16 addr;
	u32 wordData;

	addr = 0;
	pData = (u8 *) pDataOriginal;
	ucLength = MELFAS_TRANSFER_LENGTH;

	while ((addr * 4) < (int)unLength) {
		if ((unLength - (addr * 4)) < MELFAS_TRANSFER_LENGTH)
			ucLength = (u8) (unLength - (addr * 4));
		mcsdl_delay(MCSDL_DELAY_40US);

		/* Select ISP Mode */
		if (IdxNum > 0) {
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
			mcsdl_delay(MCSDL_DELAY_3US);
		}
		mcsdl_select_isp_mode(ISP_MODE_SERIAL_READ);

		wordData = ((addr & 0x1FFF) << 1) | 0x0;
		wordData <<= 14;
		mcsdl_write_bits(wordData, 18);
		addr += 1;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		printk(KERN_INFO "#");
#endif
		mcsdl_read_flash(ucVerifyBuffer);

		MCSDL_GPIO_SDA_SET_LOW();
		MCSDL_GPIO_SDA_SET_OUTPUT(0);

		/* Comparing */
		if (IdxNum == 0) {
			for (i = 0; i < (int)ucLength; i++) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
				pr_info("tsp fw:  %02X", ucVerifyBuffer[i]);
#endif
				if (ucVerifyBuffer[i] != pData[i]) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
					pr_info
					    ("\n [Error] Address : 0x%04X : "
					     "0x%02X - 0x%02X\n",
					     addr, pData[i], ucVerifyBuffer[i]);
#endif
					nRet = MCSDL_RET_PROGRAM_VERIFY_FAILED;
					goto mcsdl_verify_flash_finish;
				}
			}
		} else {	/* slave */

			for (i = 0; i < (int)ucLength; i++) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
				pr_info("tsp fw:  %02X", ucVerifyBuffer[i]);
#endif
				if ((0xff - ucVerifyBuffer[i]) != pData[i]) {
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
					pr_info
					    ("\n [Error] Address : 0x%04X : "
					     "0x%02X - 0x%02X\n",
					     addr, pData[i], ucVerifyBuffer[i]);
#endif
					nRet = MCSDL_RET_PROGRAM_VERIFY_FAILED;
					goto mcsdl_verify_flash_finish;
				}
			}
		}
		pData += ucLength;
		mcsdl_unselect_isp_mode();
	}

	nRet = MCSDL_RET_SUCCESS;

mcsdl_verify_flash_finish:
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n");
#endif
	mcsdl_unselect_isp_mode();
	return nRet;
}

static void mcsdl_read_flash(u8 *pBuffer)
{
	int i;

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_40US);

	for (i = 0; i < 6; i++) {
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_10US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_10US);
	}

	mcsdl_read_32bits(pBuffer);
}

static int mcsdl_read_flash_from(u8 *pBuffer, u16 unStart_addr,
				 u16 unLength, char IdxNum)
{
	int i;
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	int j;
#endif
	u8 ucLength;
	u16 addr;
	u32 wordData;

	if (unLength >= MELFAS_FIRMWARE_MAX_SIZE)
		return MCSDL_RET_PROGRAM_SIZE_IS_WRONG;

	addr = 0;
	ucLength = MELFAS_TRANSFER_LENGTH;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	pr_info("tsp fw:  %04X : ", unStart_addr);
#endif
	for (i = 0; i < (int)unLength; i += (int)ucLength) {
		addr = (u16) i;
		if (IdxNum > 0) {
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
			mcsdl_delay(MCSDL_DELAY_3US);
		}

		mcsdl_select_isp_mode(ISP_MODE_SERIAL_READ);
		wordData = (((unStart_addr + addr) & 0x1FFF) << 1) | 0x0;
		wordData <<= 14;

		mcsdl_write_bits(wordData, 18);
		if ((unLength - addr) < MELFAS_TRANSFER_LENGTH)
			ucLength = (u8) (unLength - addr);

		/* Read flash */
		mcsdl_read_flash(&pBuffer[addr]);

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		for (j = 0; j < (int)ucLength; j++)
			printk(KERN_INFO "%02X ", pBuffer[j]);
#endif
		mcsdl_unselect_isp_mode();
	}
	return MCSDL_RET_SUCCESS;
}

static void mcsdl_set_ready(void)
{
	MCSDL_VDD_SET_LOW();

	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_OUTPUT(0);

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SCL_SET_OUTPUT(0);

	MCSDL_RESETB_SET_LOW();
	MCSDL_RESETB_SET_OUTPUT(0);

	mcsdl_delay(MCSDL_DELAY_25MS);	/* Delay for Stable VDD */

	MCSDL_VDD_SET_HIGH();

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_40MS);
}

static void mcsdl_reboot_mcs(void)
{
	MCSDL_VDD_SET_LOW();

	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT(1);

	MCSDL_GPIO_SCL_SET_HIGH();
	MCSDL_GPIO_SCL_SET_OUTPUT(1);

	MCSDL_RESETB_SET_LOW();
	MCSDL_RESETB_SET_OUTPUT(0);

	mcsdl_delay(MCSDL_DELAY_25MS);	/* Delay for Stable VDD */

	MCSDL_VDD_SET_HIGH();

	MCSDL_RESETB_SET_HIGH();
	MCSDL_RESETB_SET_INPUT();
	MCSDL_GPIO_SCL_SET_INPUT();
	MCSDL_GPIO_SDA_SET_INPUT();

	mcsdl_delay(MCSDL_DELAY_30MS);
}

/* Write ISP Mode entering signal */
static void mcsdl_select_isp_mode(u8 ucMode)
{
	int i;

	u8 enteringCodeMassErase[16] = {
		0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1 };
	u8 enteringCodeSerialWrite[16] = {
		0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
	u8 enteringCodeSerialRead[16] = {
		0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1 };
	u8 enteringCodeNextChipBypass[16] = {
		1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1 };

	u8 *pCode = NULL;

	/* Entering ISP mode : Part 1 */
	if (ucMode == ISP_MODE_ERASE_FLASH)
		pCode = enteringCodeMassErase;
	else if (ucMode == ISP_MODE_SERIAL_WRITE)
		pCode = enteringCodeSerialWrite;
	else if (ucMode == ISP_MODE_SERIAL_READ)
		pCode = enteringCodeSerialRead;
	else if (ucMode == ISP_MODE_NEXT_CHIP_BYPASS)
		pCode = enteringCodeNextChipBypass;

	MCSDL_RESETB_SET_LOW();
	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_HIGH();

	for (i = 0; i < 16; i++) {
		if (pCode[i])
			MCSDL_RESETB_SET_HIGH();
		else
			MCSDL_RESETB_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);
	}

	MCSDL_RESETB_SET_LOW();

	/* Entering ISP mode : Part 2   - Only Mass Erase */
	mcsdl_delay(MCSDL_DELAY_7US);

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_HIGH();
	if (ucMode == ISP_MODE_ERASE_FLASH) {
		mcsdl_delay(MCSDL_DELAY_7US);
		for (i = 0; i < 4; i++) {
			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_25MS);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_150US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_3US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_7US);
		}
	}
	MCSDL_GPIO_SDA_SET_LOW();
}

static void mcsdl_unselect_isp_mode(void)
{
	int i;

	MCSDL_RESETB_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_3US);

	for (i = 0; i < 10; i++) {
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);
	}
}

static void mcsdl_read_32bits(u8 *pData)
{
	int i, j;

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_LOW();

	MCSDL_GPIO_SDA_SET_INPUT();

	for (i = 3; i >= 0; i--) {
		pData[i] = 0;
		for (j = 0; j < 8; j++) {
			pData[i] <<= 1;

			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_1US);
			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_1US);
			if (MCSDL_GPIO_SDA_IS_HIGH())
				pData[i] |= 0x01;
		}
	}

	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_OUTPUT(0);
}

static void mcsdl_write_bits(u32 wordData, int nBits)
{
	int i;

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SDA_SET_LOW();

	for (i = 0; i < nBits; i++) {
		if (wordData & 0x80000000)
			MCSDL_GPIO_SDA_SET_HIGH();
		else
			MCSDL_GPIO_SDA_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);

		wordData <<= 1;
	}
}

static void mcsdl_scl_toggle_twice(void)
{
	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT(1);

	MCSDL_GPIO_SCL_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_20US);
	MCSDL_GPIO_SCL_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_20US);

	MCSDL_GPIO_SCL_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_20US);
	MCSDL_GPIO_SCL_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_20US);
}

static void mcsdl_delay(u32 nCount)
{
	switch (nCount) {
	case MCSDL_DELAY_1US:{
			udelay(1);
			break;
		}
	case MCSDL_DELAY_2US:{
			udelay(2);
			break;
		}
	case MCSDL_DELAY_3US:{
			udelay(3);
			break;
		}
	case MCSDL_DELAY_5US:{
			udelay(5);
			break;
		}
	case MCSDL_DELAY_7US:{
			udelay(7);
			break;
		}
	case MCSDL_DELAY_10US:{
			udelay(10);
			break;
		}
	case MCSDL_DELAY_15US:{
			udelay(15);
			break;
		}
	case MCSDL_DELAY_20US:{
			udelay(20);
			break;
		}
	case MCSDL_DELAY_40US:{
			udelay(40);
			break;
		}
	case MCSDL_DELAY_100US:{
			udelay(100);
			break;
		}
	case MCSDL_DELAY_150US:{
			udelay(150);
			break;
		}
	case MCSDL_DELAY_300US:{
			udelay(300);
			break;
		}
	case MCSDL_DELAY_500US:{
			udelay(500);
			break;
		}
	case MCSDL_DELAY_800US:{
			udelay(800);
			break;
		}
	case MCSDL_DELAY_20MS:{
			mdelay(20);
			break;
		}
	case MCSDL_DELAY_25MS:{
			mdelay(25);
			break;
		}
	case MCSDL_DELAY_30MS:{
			mdelay(30);
			break;
		}
	case MCSDL_DELAY_40MS:{
			mdelay(40);
			break;
		}
	case MCSDL_DELAY_45MS:{
			mdelay(45);
			break;
		}
	case MCSDL_DELAY_60MS:{
			mdelay(60);
			break;
		}
	default:{
			break;
		}
	}
}

#ifdef MELFAS_ENABLE_DBG_PRINT
static void mcsdl_print_result(int nRet)
{
	if (nRet == MCSDL_RET_SUCCESS)
		pr_info("tsp fw:  > MELFAS Firmware downloading SUCCESS.\n");
	else {
		pr_info("tsp fw:  > MELFAS Firmware downloading FAILED  :  ");
		switch (nRet) {
		case MCSDL_RET_SUCCESS:
			pr_info("tsp fw: MCSDL_RET_SUCCESS\n");
			break;
		case MCSDL_RET_ERASE_FLASH_VERIFY_FAILED:
			pr_info("tsp fw: MCSDL_RET_ERASE_FLASH_VERIFY_FAILED\n");
			break;
		case MCSDL_RET_PROGRAM_VERIFY_FAILED:
			pr_info("tsp fw: MCSDL_RET_PROGRAM_VERIFY_FAILED\n");
			break;
		case MCSDL_RET_PROGRAM_SIZE_IS_WRONG:
			pr_info("tsp fw: MCSDL_RET_PROGRAM_SIZE_IS_WRONG\n");
			break;
		case MCSDL_RET_VERIFY_SIZE_IS_WRONG:
			pr_info("tsp fw: MCSDL_RET_VERIFY_SIZE_IS_WRONG\n");
			break;
		case MCSDL_RET_WRONG_BINARY:
			pr_info("tsp fw: MCSDL_RET_WRONG_BINARY\n");
			break;
		case MCSDL_RET_READING_HEXFILE_FAILED:
			pr_info("tsp fw: MCSDL_RET_READING_HEXFILE_FAILED\n");
			break;
		case MCSDL_RET_FILE_ACCESS_FAILED:
			pr_info("tsp fw: MCSDL_RET_FILE_ACCESS_FAILED\n");
			break;
		case MCSDL_RET_MELLOC_FAILED:
			pr_info("tsp fw: MCSDL_RET_MELLOC_FAILED\n");
			break;
		case MCSDL_RET_WRONG_MODULE_REVISION:
			pr_info("tsp fw: MCSDL_RET_WRONG_MODULE_REVISION\n");
			break;
		default:
			pr_info("tsp fw: UNKNOWN ERROR. [0x%02X].\n", nRet);
			break;
		}
		printk("\n");
	}
}
#endif

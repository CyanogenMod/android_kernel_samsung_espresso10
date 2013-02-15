/* drivers/input/touchscreen/melfas_isp.c
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/preempt.h>
#include <linux/touchscreen/melfas.h>

#include "../../../arch/arm/mach-omap2/mux.h"

static unsigned GPIO_IRQ;
static unsigned GPIO_SDA;
static unsigned GPIO_SCL;
void (*set_power)(bool);

enum {
	ISP_MODE_FLASH_ERASE	= 0x59F3,
	ISP_MODE_FLASH_WRITE	= 0x62CD,
	ISP_MODE_FLASH_READ	= 0x6AC9,
};

/* each address addresses 4-byte words */
#define ISP_MAX_FW_SIZE		(0x1F00 * 4)
#define ISP_IC_INFO_ADDR	0x1F00

static void hw_reboot(bool bootloader)
{
	set_power(false);
	gpio_direction_output(GPIO_SDA, bootloader ? 0 : 1);
	gpio_direction_output(GPIO_SCL, bootloader ? 0 : 1);
	gpio_direction_output(GPIO_IRQ, 0);
	msleep(30);
	set_power(true);
	msleep(30);

	if (bootloader) {
		gpio_set_value(GPIO_SCL, 0);
		gpio_set_value(GPIO_SDA, 1);
	} else {
		gpio_set_value(GPIO_IRQ, 1);
		gpio_direction_input(GPIO_IRQ);
		gpio_direction_input(GPIO_SCL);
		gpio_direction_input(GPIO_SDA);
	}
	msleep(40);
}

static inline void hw_reboot_bootloader(void)
{
	hw_reboot(true);
}

static inline void hw_reboot_normal(void)
{
	hw_reboot(false);
}

static inline void mms_pwr_on_reset(void)
{
	touch_i2c_to_gpio(true);

	set_power(false);
	gpio_direction_output(GPIO_SDA, 1);
	gpio_direction_output(GPIO_SCL, 1);
	gpio_direction_output(GPIO_IRQ, 1);
	msleep(50);
	set_power(true);
	msleep(50);

	touch_i2c_to_gpio(false);

	/* TODO: Seems long enough for the firmware to boot.
	 * Find the right value */
	msleep(250);
}

static void isp_toggle_clk(int start_lvl, int end_lvl, int hold_us)
{
	gpio_direction_output(GPIO_SCL, start_lvl);
	udelay(hold_us);
	gpio_direction_output(GPIO_SCL, end_lvl);
	udelay(hold_us);
}

/* 1 <= cnt <= 32 bits to write */
static void isp_send_bits(u32 data, int cnt)
{
	gpio_direction_output(GPIO_IRQ, 0);
	gpio_direction_output(GPIO_SCL, 0);
	gpio_direction_output(GPIO_SDA, 0);

	/* clock out the bits, msb first */
	while (cnt--) {
		gpio_set_value(GPIO_SDA, (data >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(1, 0, 3);
	}
}

/* 1 <= cnt <= 32 bits to read */
static u32 isp_recv_bits(int cnt)
{
	u32 data = 0;

	gpio_direction_output(GPIO_IRQ, 0);
	gpio_direction_output(GPIO_SCL, 0);
	gpio_set_value(GPIO_SDA, 0);
	gpio_direction_input(GPIO_SDA);

	/* clock in the bits, msb first */
	while (cnt--) {
		isp_toggle_clk(0, 1, 1);
		data = (data << 1) | (!!gpio_get_value(GPIO_SDA));
	}

	gpio_direction_output(GPIO_SDA, 0);
	return data;
}

static void isp_enter_mode(u32 mode)
{
	int cnt;

	gpio_direction_output(GPIO_IRQ, 0);
	gpio_direction_output(GPIO_SCL, 0);
	gpio_direction_output(GPIO_SDA, 1);

	mode &= 0xffff;
	for (cnt = 15; cnt >= 0; cnt--) {
		gpio_set_value(GPIO_IRQ, (mode >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(1, 0, 3);
	}

	gpio_set_value(GPIO_IRQ, 0);
}

static void isp_exit_mode(void)
{
	int i;

	gpio_direction_output(GPIO_IRQ, 0);
	udelay(3);

	for (i = 0; i < 10; i++)
		isp_toggle_clk(1, 0, 3);
}

static void flash_set_address(u16 addr)
{
	/* Only 13 bits of addr are valid.
	 * The addr is in bits 13:1 of cmd */
	isp_send_bits((u32)(addr & 0x1fff) << 1, 18);
}

static void flash_erase(void)
{
	isp_enter_mode(ISP_MODE_FLASH_ERASE);

	gpio_direction_output(GPIO_IRQ, 0);
	gpio_direction_output(GPIO_SCL, 0);
	gpio_direction_output(GPIO_SDA, 1);

	/* 4 clock cycles with different timings for the erase to
	 * get processed, clk is already 0 from above */
	udelay(7);
	isp_toggle_clk(1, 0, 3);
	udelay(7);
	isp_toggle_clk(1, 0, 3);
	usleep_range(25000, 35000);
	isp_toggle_clk(1, 0, 3);
	usleep_range(150, 200);
	isp_toggle_clk(1, 0, 3);

	gpio_set_value(GPIO_SDA, 0);

	isp_exit_mode();
}

static u32 flash_readl(u16 addr)
{
	int i;
	u32 val;

	preempt_disable();
	isp_enter_mode(ISP_MODE_FLASH_READ);
	flash_set_address(addr);

	gpio_direction_output(GPIO_SCL, 0);
	gpio_direction_output(GPIO_SDA, 0);
	udelay(40);

	/* data load cycle */
	for (i = 0; i < 6; i++)
		isp_toggle_clk(1, 0, 10);

	val = isp_recv_bits(32);
	isp_exit_mode();
	preempt_enable();

	return val;
}

static void flash_writel(u16 addr, u32 val)
{
	preempt_disable();
	isp_enter_mode(ISP_MODE_FLASH_WRITE);
	flash_set_address(addr);
	isp_send_bits(val, 32);

	gpio_direction_output(GPIO_SDA, 1);
	/* 6 clock cycles with different timings for the data to get written
	 * into flash */
	isp_toggle_clk(0, 1, 3);
	isp_toggle_clk(0, 1, 3);
	isp_toggle_clk(0, 1, 6);
	isp_toggle_clk(0, 1, 12);
	isp_toggle_clk(0, 1, 3);
	isp_toggle_clk(0, 1, 3);
	isp_toggle_clk(1, 0, 1);

	gpio_direction_output(GPIO_SDA, 0);
	isp_exit_mode();
	preempt_enable();
	usleep_range(300, 400);
}

static bool flash_is_erased(void)
{
	u32 val;
	u16 addr;

	for (addr = 0; addr < (ISP_MAX_FW_SIZE / 4); addr++) {
		udelay(40);
		val = flash_readl(addr);

		if (val != 0xffffffff) {
			pr_err("tsp fw.: addr 0x%x not erased: 0x%08x != 0xffffffff\n",
				addr, val);
			return false;
		}
	}
	return true;
}

static int fw_write_image(const u8 *data, size_t len)
{
	u16 addr = 0;

	for (addr = 0; addr < (len / 4); addr++, data += 4) {
		u32 val = get_unaligned_le32(data);
		u32 verify_val;
		int retries = 3;

		while (retries--) {
			flash_writel(addr, val);
			verify_val = flash_readl(addr);
			if (val == verify_val)
				break;
			pr_err("tsp fw.: mismatch @ addr 0x%x: 0x%x != 0x%x\n",
				addr, verify_val, val);
			hw_reboot_bootloader();
			continue;
		}
		if (retries < 0)
			return -ENXIO;
	}

	return 0;
}

static int fw_download(const u8 *data, size_t len)
{
	u32 val;
	int ret = 0;

	if (len % 4) {
		pr_err("tsp fw.: fw image size (%d) must be a multiple of 4 bytes\n",
			len);
		return -EINVAL;
	} else if (len > ISP_MAX_FW_SIZE) {
		pr_err("tsp fw.: fw image is too big, %d > %d\n", len,
							ISP_MAX_FW_SIZE);
		return -EINVAL;
	}

	pr_info("tsp fw.: fw download start\n");

	gpio_direction_output(GPIO_SDA, 0);
	gpio_direction_output(GPIO_SCL, 0);
	gpio_direction_output(GPIO_IRQ, 0);

	hw_reboot_bootloader();

	val = flash_readl(ISP_IC_INFO_ADDR);
	pr_info("tsp fw.: IC info: 0x%02x (%x)\n", val & 0xff, val);

	pr_info("tsp fw.: fw erase...\n");
	flash_erase();
	if (!flash_is_erased()) {
		ret = -ENXIO;
		goto err;
	}
	pr_info("tsp fw.: fw erase done...\n");

	pr_info("tsp fw.: fw write...\n");
	/* XXX: what does this do?! */
	flash_writel(ISP_IC_INFO_ADDR, 0xffffff00 | (val & 0xff));
	usleep_range(1000, 1500);
	ret = fw_write_image(data, len);
	if (ret)
		goto err;
	usleep_range(1000, 1500);

	hw_reboot_normal();
	usleep_range(1000, 1500);
	pr_info("tsp fw.: fw download done...\n");

	return 0;
err:
	pr_err("tsp fw.: fw download failed...\n");
	hw_reboot_normal();
	return ret;
}

bool isp_updater(const u8 *fw_data, const size_t fw_size,
				const struct sec_ts_platform_data *sec_pdata)
{
	bool result = true;

	set_power = sec_pdata->set_power;
	GPIO_IRQ = sec_pdata->gpio_irq;
	GPIO_SCL = sec_pdata->gpio_scl;
	GPIO_SDA = sec_pdata->gpio_sda;

	touch_i2c_to_gpio(true);
	if (fw_download(fw_data, fw_size) < 0)
		result = false;
	touch_i2c_to_gpio(false);

	return result;
}

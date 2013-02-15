/*
 * Copyright 2010, Cypress Semiconductor Corporation.
 * Copyright (C) 2010-2011, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor
 * Boston, MA  02110-1301, USA.
 *
 */

#include <linux/string.h>
#include <linux/delay.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/gpio.h>
#include <linux/preempt.h>

#include "cypress-touchkey-firmware-20234.h"

#define WRITE_BYTE_START	0x90
#define WRITE_BYTE_END		0xE0
#define NUM_BLOCK_END		0xE0

static void touchkey_init_gpio(struct cptk_platform_data *pdata)
{
	gpio_direction_input(pdata->sda_pin);
	gpio_direction_input(pdata->scl_pin);
	pdata->power(true);
	mdelay(1);
}

static void touchkey_run_clk(struct cptk_platform_data *pdata,
								int cycles)
{
	int i;

	for (i = 0; i < cycles; i++) {
		gpio_direction_output(pdata->scl_pin, 0);
		gpio_direction_output(pdata->scl_pin, 1);
	}
}

static u8 touchkey_get_data(struct cptk_platform_data *pdata)
{
	return gpio_get_value(pdata->sda_pin);
}

static u8 touchkey_read_bit(struct cptk_platform_data *pdata)
{
	touchkey_run_clk(pdata, 1);
	return touchkey_get_data(pdata);
}

static u8 touchkey_read_byte(struct cptk_platform_data *pdata)
{
	int i;
	u8 byte = 0;

	preempt_disable();
	for (i = 7; i >= 0; i--)
		byte |= touchkey_read_bit(pdata) << i;
	preempt_enable();

	return byte;
}

static void touchkey_send_bits(struct cptk_platform_data *pdata,
							u8 data, int num_bits)
{
	int i;

	preempt_disable();
	for (i = 0; i < num_bits; i++, data <<= 1) {
		gpio_direction_output(pdata->sda_pin, !!(data & 0x80));
		gpio_direction_output(pdata->scl_pin, 1);
		gpio_direction_output(pdata->scl_pin, 0);
	}
	preempt_enable();
}

static void touchkey_send_vector(struct cptk_platform_data *pdata,
					const struct issp_vector *vector_data)
{
	int i;
	u16 num_bits;

	gpio_direction_output(pdata->sda_pin, 0);
	for (i = 0, num_bits = vector_data->num_bits; num_bits > 7;
							num_bits -= 8, i++)
		touchkey_send_bits(pdata, vector_data->data[i], 8);

	if (num_bits)
		touchkey_send_bits(pdata, vector_data->data[i], num_bits);
	gpio_direction_input(pdata->sda_pin);
}

static bool touchkey_wait_transaction(struct cptk_platform_data *pdata)
{
	int i;

	for (i = 0; i < TRANSITION_TIMEOUT; i++) {
		gpio_direction_output(pdata->scl_pin, 0);
		if (touchkey_get_data(pdata))
			break;
		gpio_direction_output(pdata->scl_pin, 1);
	}

	if (i == TRANSITION_TIMEOUT)
		return true;

	for (i = 0; i < TRANSITION_TIMEOUT; i++) {
		gpio_direction_output(pdata->scl_pin, 0);
		if (!touchkey_get_data(pdata))
			break;
		gpio_direction_output(pdata->scl_pin, 1);
	}

	return i == TRANSITION_TIMEOUT;
}

static int touchkey_issp_pwr_init(struct cptk_platform_data *pdata)
{
	touchkey_init_gpio(pdata);

	if (touchkey_wait_transaction(pdata))
		return -1;

	gpio_direction_output(pdata->scl_pin, 0);

	touchkey_send_vector(pdata, &wait_and_poll_end);
	touchkey_send_vector(pdata, &id_setup_1);

	if (touchkey_wait_transaction(pdata))
		return -1;

	touchkey_send_vector(pdata, &wait_and_poll_end);

	mdelay(5);	/* wait for IC into upload mode */

	return 0;
}

static int touchkey_verify_product_id(struct cptk_platform_data *pdata)
{
	u8 product_id[2];

	touchkey_send_vector(pdata, &id_setup_2);
	if (touchkey_wait_transaction(pdata))
		return -1;
	touchkey_send_vector(pdata, &wait_and_poll_end);

	touchkey_send_vector(pdata, &read_id_1);
	touchkey_run_clk(pdata, 2);
	product_id[0] = touchkey_read_byte(pdata);
	touchkey_run_clk(pdata, 1);

	touchkey_send_vector(pdata, &read_id_2);
	touchkey_run_clk(pdata, 2);
	product_id[1] = touchkey_read_byte(pdata);
	touchkey_run_clk(pdata, 1);

	touchkey_send_bits(pdata, 0x00, 1);

	if (product_id[0] != target_id[0] || product_id[1] != target_id[1]) {
		pr_err("cptk: %s: product id not match: %x %x",
					__func__, product_id[0], product_id[1]);
		return -1;
	}

	return 0;
}

static int touchkey_erase_target(struct cptk_platform_data *pdata)
{
	touchkey_send_vector(pdata, &erase);
	if (touchkey_wait_transaction(pdata))
		return -1;

	touchkey_send_vector(pdata, &wait_and_poll_end);

	return 0;
}

static u16 touchkey_load_target(struct cptk_platform_data *pdata,
					const u8 *target_data_output, int len)
{
	u16 checksum_data = 0;
	u8 addr;
	int i;

	for (i = 0, addr = 0; i < len; i++, addr += 4) {
		checksum_data += target_data_output[i];
		touchkey_send_bits(pdata, WRITE_BYTE_START, 5);
		touchkey_send_bits(pdata, addr, 6);
		touchkey_send_bits(pdata, target_data_output[i], 8);
		touchkey_send_bits(pdata, WRITE_BYTE_END, 3);
	}

	return checksum_data;
}

static int touchkey_program_target_block(struct cptk_platform_data *pdata,
								u8 block)
{
	touchkey_send_vector(pdata, &set_block_num);
	touchkey_send_bits(pdata, block, 8);
	touchkey_send_bits(pdata, NUM_BLOCK_END, 3);
	touchkey_send_vector(pdata, &program);

	if (touchkey_wait_transaction(pdata))
		return -1;
	touchkey_send_vector(pdata, &wait_and_poll_end);

	return 0;
}

static int touchkey_verify_setup(struct cptk_platform_data *pdata, u8 block)
{
	touchkey_send_vector(pdata, &set_block_num);
	touchkey_send_bits(pdata, block, 8);
	touchkey_send_bits(pdata, NUM_BLOCK_END, 3);
	touchkey_send_vector(pdata, &verify_setup);

	if (touchkey_wait_transaction(pdata))
		return -1;
	touchkey_send_vector(pdata, &wait_and_poll_end);

	return 0;
}

static int touchkey_verify_data(struct cptk_platform_data *pdata,
						const u8 *data, int base)
{
	u8 i, addr, temp;

	for (i = 0, addr = 0; i < BLOCK_SIZE; i++, addr += 4) {
		touchkey_send_bits(pdata, 0xB0, 5);
		touchkey_send_bits(pdata, addr, 6);
		touchkey_run_clk(pdata, 2);

		gpio_direction_input(pdata->sda_pin);
		temp = touchkey_read_byte(pdata);
		touchkey_run_clk(pdata, 1);
		touchkey_send_bits(pdata, 0x80, 1);

		if (temp != data[i]) {
			pr_err("%s: not match!! addr: %d, read: 0x%.2X fw.: 0x%.2X",
					__func__,  i + base, temp, data[i]);
			return -1;
		}
	}

	return 0;
}

static int touchkey_target_bank_checksum(struct cptk_platform_data *pdata,
								u16 *checksum)
{
	touchkey_send_vector(pdata, &checksum_setup);
	if (touchkey_wait_transaction(pdata))
		return -1;

	touchkey_send_vector(pdata, &wait_and_poll_end);
	touchkey_send_vector(pdata, &read_checksum_v1);
	touchkey_run_clk(pdata, 2);
	*checksum = touchkey_read_byte(pdata) << 8;

	touchkey_run_clk(pdata, 1);
	touchkey_send_vector(pdata, &read_checksum_v2);
	touchkey_run_clk(pdata, 2);
	*checksum |= touchkey_read_byte(pdata);
	touchkey_run_clk(pdata, 1);
	touchkey_send_bits(pdata, 0x80, 1);

	return 0;
}

static void touchkey_reset_target(struct cptk_platform_data *pdata)
{
	gpio_direction_input(pdata->scl_pin);
	gpio_direction_input(pdata->sda_pin);
	pdata->power(false);
	mdelay(300);
	touchkey_init_gpio(pdata);
}

static int touchkey_secure_target_flash(struct cptk_platform_data *pdata)
{
	u8 addr;
	int i;

	for (i = 0, addr = 0; i < SECURITY_BYTES_PER_BANK; i++, addr += 4) {
		touchkey_send_bits(pdata, WRITE_BYTE_START, 5);
		touchkey_send_bits(pdata, addr, 6);
		touchkey_send_bits(pdata, 0x00, 8);
		touchkey_send_bits(pdata, WRITE_BYTE_END, 3);
	}

	touchkey_send_vector(pdata, &secure);
	if (touchkey_wait_transaction(pdata))
		return -1;

	touchkey_send_vector(pdata, &wait_and_poll_end);

	return 0;
}

int touchkey_flash_firmware(struct cptk_platform_data *pdata, const u8 *fw_data)
{
	u16 chksumtgt = 0;
	u16 chksumdat = 0;
	u8 i;

	pdata->power(false);

	if (touchkey_issp_pwr_init(pdata)) {
		pr_err("cptk: %s: error powering up\n", __func__);
		goto error_trap;
	}

	/* This function do not working. Check to cypress support team. */

	if (touchkey_verify_product_id(pdata)) {
		pr_err("cptk: %s: error verify product id\n", __func__);
		goto error_trap;
	}

	if (touchkey_erase_target(pdata)) {
		pr_err("cptk: %s: error erasing flash\n", __func__);
		goto error_trap;
	}

	/* write firmware data */
	for (i = 0; i < BLOCKS_PER_BANK; i++) {
		int retry = 3;
retry:
		chksumdat += touchkey_load_target(pdata,
					fw_data + i * BLOCK_SIZE, BLOCK_SIZE);

		if (touchkey_program_target_block(pdata, i)) {
			pr_err("cptk: %s: error programming flash\n", __func__);
			goto error_trap;
		}

		if (touchkey_verify_setup(pdata, i)) {
			pr_err("cptk: %s: error verify setup\n", __func__);
			goto error_trap;
		}

		if (touchkey_verify_data(pdata, fw_data + i * BLOCK_SIZE,
							      i * BLOCK_SIZE)) {
			if (retry--)
				goto retry;
			else
				goto error_trap;
		}
	}

	/* security start */
	if (touchkey_secure_target_flash(pdata)) {
		pr_err("cptk: %s: error securing flash\n", __func__);
		goto error_trap;
	}

	if (touchkey_target_bank_checksum(pdata, &chksumtgt)) {
		pr_err("cptk: %s: error reading checksum\n", __func__);
		goto error_trap;
	}

	if (chksumtgt != chksumdat) {
		pr_err("cptk: %s: error at checksum. chksumtgt: %d, chksumdat: %d\n",
						__func__, chksumtgt, chksumdat);
		goto error_trap;
	}

	touchkey_reset_target(pdata);
	return 0;

error_trap:
	gpio_direction_input(pdata->scl_pin);
	gpio_direction_input(pdata->sda_pin);
	pdata->power(false);
	mdelay(20);
	return -1;
}

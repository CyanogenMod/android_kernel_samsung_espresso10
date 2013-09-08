/* /linux/drivers/misc/modem_if/modem_modemctl_device_cbp7.1.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
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

#include <linux/init.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <linux/platform_data/modem.h>
#include "modem_prj.h"
#include "modem_link_device_dpram.h"

#define PIF_TIMEOUT		(180 * HZ)
#define DPRAM_INIT_TIMEOUT	(15 * HZ)

static int cbp71_on(struct modem_ctl *mc)
{
	int ret;
	struct dpram_link_device *dpram_ld =
				to_dpram_link_device(mc->iod->link);

	pr_info("[MODEM_IF] cbp71_on()\n");

	if (!mc->gpio_cp_off || !mc->gpio_cp_reset) {
		pr_err("[MODEM_IF] no gpio data\n");
		return -ENXIO;
	}
	gpio_set_value(mc->gpio_cp_reset, 0);
	gpio_set_value(mc->gpio_cp_off, 1);
	msleep(600);
	gpio_set_value(mc->gpio_cp_reset, 1);
	gpio_set_value(mc->gpio_cp_off, 0);

	msleep(300);
	gpio_set_value(mc->gpio_pda_active, 1);

	mc->iod->modem_state_changed(mc->iod, STATE_BOOTING);

	/* Wait here until the PHONE is up.
	* Waiting as the this called from IOCTL->UM thread */
	pr_debug("[MODEM_IF] power control waiting for INT_MASK_CMD_PIF_INIT_DONE\n");

	dpram_ld->clear_interrupt(dpram_ld);

	ret = wait_for_completion_interruptible_timeout(
			&dpram_ld->dpram_init_cmd, DPRAM_INIT_TIMEOUT);
	if (!ret) {
		/* ret will be 0 on timeout, < zero if interrupted */
		pr_warn("[MODEM_IF] INIT_START cmd was not arrived.\n");
		pr_warn("init_cmd_wait_condition is 0 and wait timeout happend\n");
		return -ENXIO;
	}

	ret = wait_for_completion_interruptible_timeout(
			&dpram_ld->modem_pif_init_done, PIF_TIMEOUT);
	if (!ret) {
		pr_warn("[MODEM_IF] PIF init failed\n");
		pr_warn("pif_init_wait_condition is 0 and wait timeout happend\n");
		return -ENXIO;
	}

	pr_debug("[MODEM_IF] complete cbp71_on\n");

	mc->iod->modem_state_changed(mc->iod, STATE_ONLINE);

	return 0;
}

static int cbp71_off(struct modem_ctl *mc)
{
	pr_info("[MODEM_IF] cbp71_off()\n");

	if (!mc->gpio_cp_off || !mc->gpio_cp_reset) {
		pr_err("[MODEM_IF] no gpio data\n");
		return -ENXIO;
	}

	gpio_set_value(mc->gpio_cp_off, 1);
	gpio_set_value(mc->gpio_cp_reset, 0);

	mc->iod->modem_state_changed(mc->iod, STATE_OFFLINE);

	return 0;
}

static int cbp71_reset(struct modem_ctl *mc)
{
	int ret = 0;

	pr_debug("[MODEM_IF] cbp71_reset()\n");

	ret = cbp71_off(mc);
	if (ret)
		return -ENXIO;

	msleep(100);

	ret = cbp71_on(mc);
	if (ret)
		return -ENXIO;

	return 0;
}

static int cbp71_boot_on(struct modem_ctl *mc)
{
	pr_debug("[MODEM_IF] cbp71_boot_on()\n");

	if (!mc->gpio_cp_reset) {
		pr_err("[MODEM_IF] no gpio data\n");
		return -ENXIO;
	}
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(600);
	gpio_set_value(mc->gpio_cp_reset, 1);

	mc->iod->modem_state_changed(mc->iod, STATE_BOOTING);

	return 0;
}

static int cbp71_boot_off(struct modem_ctl *mc)
{
	pr_debug("[MODEM_IF] cbp71_boot_off()\n");
	return 0;
}

static irqreturn_t phone_active_irq_handler(int irq, void *_mc)
{
	int phone_reset = 0;
	int phone_active_value = 0;
	int phone_state = 0;
	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	if (!mc->gpio_cp_reset || !mc->gpio_phone_active) {
		pr_err("[MODEM_IF] no gpio data\n");
		return IRQ_HANDLED;
	}

	phone_reset = gpio_get_value(mc->gpio_cp_reset);
	phone_active_value = gpio_get_value(mc->gpio_phone_active);

	if (phone_reset && phone_active_value) {
		phone_state = STATE_ONLINE;
		if (mc->iod && mc->iod->modem_state_changed)
			mc->iod->modem_state_changed(mc->iod, phone_state);
	} else if (phone_reset && !phone_active_value) {
		if (mc->phone_state == STATE_ONLINE) {
			phone_state = STATE_CRASH_EXIT;
			if (mc->iod && mc->iod->modem_state_changed)
				mc->iod->modem_state_changed(mc->iod,
						phone_state);
		}
	} else {
		phone_state = STATE_OFFLINE;
		if (mc->iod && mc->iod->modem_state_changed)
			mc->iod->modem_state_changed(mc->iod, phone_state);
	}

	if (phone_active_value)
		irq_set_irq_type(mc->irq_phone_active, IRQ_TYPE_LEVEL_LOW);
	else
		irq_set_irq_type(mc->irq_phone_active, IRQ_TYPE_LEVEL_HIGH);

	pr_info("phone_active_irq_handler : phone_state=%d\n", phone_state);

	return IRQ_HANDLED;
}

static void cbp71_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = cbp71_on;
	mc->ops.modem_off = cbp71_off;
	mc->ops.modem_reset = cbp71_reset;
	mc->ops.modem_boot_on = cbp71_boot_on;
	mc->ops.modem_boot_off = cbp71_boot_off;
}

int cbp71_init_modemctl_device(struct modem_ctl *mc,
			struct modem_data *pdata)
{
	int ret = 0;
	struct platform_device *pdev;

	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_reset_req_n = pdata->gpio_reset_req_n;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_cp_dump_int = pdata->gpio_cp_dump_int;
	mc->gpio_flm_uart_sel = pdata->gpio_flm_uart_sel;
	mc->gpio_cp_warm_reset = pdata->gpio_cp_warm_reset;
	mc->gpio_cp_off = pdata->gpio_cp_off;

	pdev = to_platform_device(mc->dev);
	mc->irq_phone_active = platform_get_irq(pdev, 0);

	cbp71_get_ops(mc);

	ret = request_irq(mc->irq_phone_active, phone_active_irq_handler,
				IRQF_TRIGGER_HIGH, "phone_active", mc);
	if (ret) {
		pr_err("[MODEM_IF]failed to irq_phone_active request_irq: %d\n"
			, ret);
		return ret;
	}

	ret = enable_irq_wake(mc->irq_phone_active);
	if (ret) {
		pr_err("[MODEM_IF] %s: failed to enable_irq_wake:%d\n",
					__func__, ret);
		free_irq(mc->irq_phone_active, mc);
		return ret;
	}

	return ret;
}

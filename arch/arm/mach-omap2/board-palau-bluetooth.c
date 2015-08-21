/* arch/arm/mach-omap2/board-palau-bluetooth.c
 * Bluetooth Broadcomm  and low power control via GPIO
 *
 *  Copyright (C) 2011 Samsung, Inc.
 *  Copyright (C) 2011 Google, Inc.
 *
 *  Based on mach-omap2/board-espresso-bluetooth.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/rfkill.h>
#include <linux/serial_core.h>
#include <linux/wakelock.h>

#include <plat/serial.h>

#include "mux.h"
#include "omap_muxtbl.h"

static struct rfkill *bt_rfkill;
static bool bt_enabled;
static bool host_wake_uart_enabled;
static bool wake_uart_enabled;

struct bcm_bt_lpm {
	int wake;
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct uart_port *uport;

	struct wake_lock wake_lock;
	char wake_lock_name[100];
} bt_lpm;

enum {
	GPIO_BT_EN = 0,
	GPIO_BT_WAKE,
	GPIO_BT_HOST_WAKE
};

static struct gpio bt_gpios[] = {
	[GPIO_BT_EN] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "BT_EN",
	},
	[GPIO_BT_WAKE] = {
		.flags	= GPIOF_OUT_INIT_LOW,
		.label	= "BT_WAKE",
	},
	[GPIO_BT_HOST_WAKE] = {
		.flags	= GPIOF_IN,
		.label	= "BT_HOST_WAKE",
	},
};

static void bt_gpio_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bt_gpios); i++)
		bt_gpios[i].gpio =
		    omap_muxtbl_get_gpio_by_name(bt_gpios[i].label);
	gpio_request_array(bt_gpios, ARRAY_SIZE(bt_gpios));
}

static int bcm4334_bt_rfkill_set_power(void *data, bool blocked)
{
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	if (!blocked) {
		pr_info("[BT] Bluetooth Power On.\n");
		gpio_set_value(bt_gpios[GPIO_BT_EN].gpio, 1);

	} else {
		pr_info("[BT] Bluetooth Power Off.\n");
		gpio_set_value(bt_gpios[GPIO_BT_EN].gpio, 0);
	}

	bt_enabled = !blocked;

	return 0;
}

static const struct rfkill_ops bcm4334_bt_rfkill_ops = {
	.set_block = bcm4334_bt_rfkill_set_power,
};

static void set_wake_locked(int wake)
{
	bt_lpm.wake = wake;

	if (!wake)
		wake_unlock(&bt_lpm.wake_lock);

	if (!wake_uart_enabled && wake)
		omap_uart_enable(2);

	gpio_set_value(bt_gpios[GPIO_BT_WAKE].gpio, wake);

	if (wake_uart_enabled && !wake)
		omap_uart_disable(2);

	wake_uart_enabled = wake;
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer)
{
	unsigned long flags;
	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	set_wake_locked(0);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return HRTIMER_NORESTART;
}

void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport)
{
	bt_lpm.uport = uport;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);

	set_wake_locked(1);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
		HRTIMER_MODE_REL);
}
EXPORT_SYMBOL(bcm_bt_lpm_exit_lpm_locked);

static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;

	bt_lpm.host_wake = host_wake;

	if (host_wake) {
		wake_lock(&bt_lpm.wake_lock);
		if (!host_wake_uart_enabled)
			omap_uart_enable(2);
	} else  {
		if (host_wake_uart_enabled)
			omap_uart_disable(2);
		/* Take a timed wakelock, so that upper layers can take it.
		 * The chipset deasserts the hostwake lock, when there is no
		 * more data to send.
		 */
		wake_lock_timeout(&bt_lpm.wake_lock, HZ/2);
	}

	host_wake_uart_enabled = host_wake;

}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;
	unsigned long flags;

	host_wake = gpio_get_value(bt_gpios[GPIO_BT_HOST_WAKE].gpio);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	update_host_wake_locked(host_wake);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return IRQ_HANDLED;
}

static int bcm_bt_lpm_init(struct platform_device *pdev)
{
	int irq;
	int ret;

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC,
				HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.host_wake = 0;

	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name),
			"BTLowPower");
	wake_lock_init(&bt_lpm.wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.wake_lock_name);

	irq = gpio_to_irq(bt_gpios[GPIO_BT_HOST_WAKE].gpio);
	ret = request_irq(irq, host_wake_isr, IRQF_TRIGGER_HIGH,
		"bt host_wake", NULL);
	if (ret) {
		gpio_free(bt_gpios[GPIO_BT_WAKE].gpio);
		gpio_free(bt_gpios[GPIO_BT_HOST_WAKE].gpio);
		return ret;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		gpio_free(bt_gpios[GPIO_BT_WAKE].gpio);
		gpio_free(bt_gpios[GPIO_BT_HOST_WAKE].gpio);
		return ret;
	}

	gpio_direction_output(bt_gpios[GPIO_BT_WAKE].gpio, 0);
	gpio_direction_input(bt_gpios[GPIO_BT_HOST_WAKE].gpio);

	return 0;
}

static int bcm4334_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;
	int ret = 0;

	bt_gpio_init();

	gpio_direction_output(bt_gpios[GPIO_BT_EN].gpio, 0);

	bt_rfkill = rfkill_alloc("bcm4334 Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bcm4334_bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
		gpio_free(bt_gpios[GPIO_BT_EN].gpio);
		return -ENOMEM;
	}

	rfkill_init_sw_state(bt_rfkill, 0);

	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		rfkill_destroy(bt_rfkill);
		gpio_free(bt_gpios[GPIO_BT_EN].gpio);
		return -1;
	}

	rfkill_set_sw_state(bt_rfkill, true);

	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);

		gpio_free(bt_gpios[GPIO_BT_EN].gpio);
	}

	return ret;
}

static int bcm4334_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(bt_gpios[GPIO_BT_EN].gpio);
	gpio_free(bt_gpios[GPIO_BT_WAKE].gpio);
	gpio_free(bt_gpios[GPIO_BT_HOST_WAKE].gpio);

	wake_lock_destroy(&bt_lpm.wake_lock);
	return 0;
}

int bcm4430_bluetooth_suspend(struct platform_device *pdev, pm_message_t state)
{
	int irq = gpio_to_irq(bt_gpios[GPIO_BT_HOST_WAKE].gpio);
	int host_wake;

	disable_irq(irq);
	host_wake = gpio_get_value(bt_gpios[GPIO_BT_HOST_WAKE].gpio);

	if (host_wake) {
		enable_irq(irq);
		return -EBUSY;
	}

	return 0;
}

int bcm4430_bluetooth_resume(struct platform_device *pdev)
{
	int irq = gpio_to_irq(bt_gpios[GPIO_BT_HOST_WAKE].gpio);
	enable_irq(irq);
	return 0;
}

static struct platform_driver bcm4334_bluetooth_platform_driver = {
	.probe = bcm4334_bluetooth_probe,
	.remove = bcm4334_bluetooth_remove,
	.driver = {
		   .name = "bcm4334_bluetooth",
		   .owner = THIS_MODULE,
		   },
};

static int __init bcm4334_bluetooth_init(void)
{
	bt_enabled = false;
	return platform_driver_register(&bcm4334_bluetooth_platform_driver);
}

static void __exit bcm4334_bluetooth_exit(void)
{
	platform_driver_unregister(&bcm4334_bluetooth_platform_driver);
}


module_init(bcm4334_bluetooth_init);
module_exit(bcm4334_bluetooth_exit);

MODULE_ALIAS("platform:bcm4334");
MODULE_DESCRIPTION("bcm4334_bluetooth");
MODULE_AUTHOR("Jaikumar Ganesh <jaikumar@google.com>");
MODULE_LICENSE("GPL");

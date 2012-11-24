/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * Author:
 *	Heetae Ahn <heetae82.ahn@samsung.com>
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
 */

#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/serio.h>
#include <linux/slab.h>
#include <linux/sec_dock_keyboard.h>

#define SCAN_CODE_HS	0x00	/* handshake scan code */
#define SCAN_CODE_BS	0x31	/* backslash(US)/pound(UK) scan code */
#define SCAN_CODE_REW	0x45	/* rewind/prev song scan code */
#define SCAN_CODE_FF	0x48	/* fast forware/next song scan code */

#define CMD_LED_ON	0xCA	/* capslock LED on command */
#define CMD_LED_OFF	0xCB	/* capslock LED off command */

#define CMD_SLEEP	0x10	/* keyboard sleep command */

#define MASK_PRESS	0x80	/* press mask */
#define MASK_SCAN_CODE	0x7F	/* scan code mask */

static int __devinit dock_keyboard_probe(struct platform_device *pdev);
static int __devexit dock_keyboard_remove(struct platform_device *pdev);
static int dock_keyboard_serio_connect(struct serio *serio,
				       struct serio_driver *drv);
static void dock_keyboard_serio_disconnect(struct serio *serio);
static irqreturn_t dock_keyboard_serio_interrupt(struct serio *serio,
						 unsigned char keycode,
						 unsigned int flags);

static struct serio_device_id dock_keyboard_serio_ids[] = {
	{
	 .type = SERIO_RS232,
	 .proto = SERIO_SAMSUNG,
	 .id = SERIO_ANY,
	 .extra = SERIO_ANY,
	 },
	{0}
};

static struct sec_dock_keyboard_driver sec_dock_kbd_driver = {
	.plat_drv = {
		     .probe = dock_keyboard_probe,
		     .remove = __devexit_p(dock_keyboard_remove),
		     .driver = {
				.name = KBD_DRV_NAME,
				.owner = THIS_MODULE,
				}
		     },
	.serio_drv = {
		      .driver = {
				 .name = KBD_DRV_NAME,
				 },
		      .id_table = dock_keyboard_serio_ids,
		      .connect = dock_keyboard_serio_connect,
		      .disconnect = dock_keyboard_serio_disconnect,
		      .interrupt = dock_keyboard_serio_interrupt,
		      }
};

static struct key_table dock_keycodes[KEYBOARD_SIZE] = {
	/* keycode, pressed             decimal hex */
	{KEY_RESERVED, false},	/* 0    0 */
	{KEY_RESERVED, false},	/* 1    1 */
	{KEY_RESERVED, false},	/* 2    2 */
	{KEY_RESERVED, false},	/* 3    3 */
	{KEY_A, false},		/* 4    4 */
	{KEY_B, false},		/* 5    5 */
	{KEY_C, false},		/* 6    6 */
	{KEY_D, false},		/* 7    7 */
	{KEY_E, false},		/* 8    8 */
	{KEY_F, false},		/* 9    9 */
	{KEY_G, false},		/* 10   0A */
	{KEY_H, false},		/* 11   0B */
	{KEY_I, false},		/* 12   0C */
	{KEY_J, false},		/* 13   0D */
	{KEY_K, false},		/* 14   0E */
	{KEY_L, false},		/* 15   0F */
	{KEY_M, false},		/* 16   10 */
	{KEY_N, false},		/* 17   11 */
	{KEY_O, false},		/* 18   12 */
	{KEY_P, false},		/* 19   13 */
	{KEY_Q, false},		/* 20   14 */
	{KEY_R, false},		/* 21   15 */
	{KEY_S, false},		/* 22   16 */
	{KEY_T, false},		/* 23   17 */
	{KEY_U, false},		/* 24   18 */
	{KEY_V, false},		/* 25   19 */
	{KEY_W, false},		/* 26   1A */
	{KEY_X, false},		/* 27   1B */
	{KEY_Y, false},		/* 28   1C */
	{KEY_Z, false},		/* 29   1D */
	{KEY_1, false},		/* 30   1E */
	{KEY_2, false},		/* 31   1F */
	{KEY_3, false},		/* 32   20 */
	{KEY_4, false},		/* 33   21 */
	{KEY_5, false},		/* 34   22 */
	{KEY_6, false},		/* 35   23 */
	{KEY_7, false},		/* 36   24 */
	{KEY_8, false},		/* 37   25 */
	{KEY_9, false},		/* 38   26 */
	{KEY_0, false},		/* 39   27 */
	{KEY_ENTER, false},	/* 40   28 */
	{KEY_BACK, false},	/* 41   29 */
	{KEY_BACKSPACE, false},	/* 42   2A */
	{KEY_TAB, false},	/* 43   2B */
	{KEY_SPACE, false},	/* 44   2C */
	{KEY_MINUS, false},	/* 45   2D */
	{KEY_EQUAL, false},	/* 46   2E */
	{KEY_LEFTBRACE, false},	/* 47   2F */
	{KEY_RIGHTBRACE, false},	/* 48   30 */
	{KEY_HOME, false},	/* 49   31 */
	{KEY_RESERVED, false},	/* 50   32 */
	{KEY_SEMICOLON, false},	/* 51   33 */
	{KEY_APOSTROPHE, false},	/* 52   34 */
	{KEY_GRAVE, false},	/* 53   35 */
	{KEY_COMMA, false},	/* 54   36 */
	{KEY_DOT, false},	/* 55   37 */
	{KEY_SLASH, false},	/* 56   38 */
	{KEY_CAPSLOCK, false},	/* 57   39 */
	{KEY_TIME, false},	/* 58   3A */
	{KEY_F3, false},	/* 59   3B */
	{KEY_WWW, false},	/* 60   3C */
	{KEY_EMAIL, false},	/* 61   3D */
	{KEY_SCREENLOCK, false},	/* 62   3E */
	{KEY_BRIGHTNESSDOWN, false},	/* 63   3F */
	{KEY_BRIGHTNESSUP, false},	/* 64   40 */
	{KEY_MUTE, false},	/* 65   41 */
	{KEY_VOLUMEDOWN, false},	/* 66   42 */
	{KEY_VOLUMEUP, false},	/* 67   43 */
	{KEY_PLAY, false},	/* 68   44 */
	{KEY_REWIND, false},	/* 69   45 */
	{KEY_F15, false},	/* 70   46 */
	{KEY_RESERVED, false},	/* 71   47 */
	{KEY_FASTFORWARD, false},	/* 72   48 */
	{KEY_MENU, false},	/* 73   49 */
	{KEY_RESERVED, false},	/* 74   4A */
	{KEY_RESERVED, false},	/* 75   4B */
	{KEY_DELETE, false},	/* 76   4C */
	{KEY_RESERVED, false},	/* 77   4D */
	{KEY_RESERVED, false},	/* 78   4E */
	{KEY_RIGHT, false},	/* 79   4F */
	{KEY_LEFT, false},	/* 80   50 */
	{KEY_DOWN, false},	/* 81   51 */
	{KEY_UP, false},	/* 82   52 */
	{KEY_NUMLOCK, false},	/* 83   53 */
	{KEY_KPSLASH, false},	/* 84   54 */
	{KEY_APOSTROPHE, false},	/* 85   55 */
	{KEY_KPMINUS, false},	/* 86   56 */
	{KEY_KPPLUS, false},	/* 87   57 */
	{KEY_KPENTER, false},	/* 88   58 */
	{KEY_KP1, false},	/* 89   59 */
	{KEY_KP2, false},	/* 90   5A */
	{KEY_KP3, false},	/* 91   5B */
	{KEY_KP4, false},	/* 92   5C */
	{KEY_KP5, false},	/* 93   5D */
	{KEY_KP6, false},	/* 94   5E */
	{KEY_KP7, false},	/* 95   5F */
	{KEY_KP8, false},	/* 96   60 */
	{KEY_KP9, false},	/* 97   61 */
	{KEY_KPDOT, false},	/* 98   62 */
	{KEY_RESERVED, false},	/* 99   63 */
	{KEY_BACKSLASH, false},	/* 100  64 */
	{KEY_F22, false},	/* 101  65 */
	{KEY_RESERVED, false},	/* 102  66 */
	{KEY_RESERVED, false},	/* 103  67 */
	{KEY_RESERVED, false},	/* 104  68 */
	{KEY_RESERVED, false},	/* 105  69 */
	{KEY_RESERVED, false},	/* 106  6A */
	{KEY_RESERVED, false},	/* 107  6B */
	{KEY_RESERVED, false},	/* 108  6C */
	{KEY_RESERVED, false},	/* 109  6D */
	{KEY_RESERVED, false},	/* 110  6E */
	{KEY_RESERVED, false},	/* 111  6F */
	{KEY_HANGEUL, false},	/* 112  70 */
	{KEY_HANJA, false},	/* 113  71 */
	{KEY_LEFTCTRL, false},	/* 114  72 */
	{KEY_LEFTSHIFT, false},	/* 115  73 */
	{KEY_F20, false},	/* 116  74 */
	{KEY_SEARCH, false},	/* 117  75 */
	{KEY_RIGHTALT, false},	/* 118  76 */
	{KEY_RIGHTSHIFT, false},	/* 119  77 */
	{KEY_F21, false},	/* 120  78 */
	{KEY_RESERVED, false},	/* 121  79 */
	{KEY_RESERVED, false},	/* 122  7A */
	{KEY_RESERVED, false},	/* 123  7B */
	{KEY_RESERVED, false},	/* 124  7C */
	{KEY_RESERVED, false},	/* 125  7D */
	{KEY_RESERVED, false},	/* 126  7E */
	{KEY_F17, false},	/* 127  7F */
};

static void remapkey_timer(unsigned long arg)
{
	struct dock_keyboard_data *data = (struct dock_keyboard_data *)arg;
	unsigned int keycode;

	if ((dock_keycodes[SCAN_CODE_REW].pressed)
	    || (dock_keycodes[SCAN_CODE_FF].pressed)) {
		data->remap_state = REMAPKEY_PRESSED;
		keycode = data->remap_keycode;
		input_report_key(data->input_dev,
				 dock_keycodes[keycode].keycode, 1);
		input_sync(data->input_dev);
	} else {
		data->remap_state = REMAPKEY_RELEASED;

		if (data->remap_keycode == SCAN_CODE_FF)
			keycode = KEY_NEXTSONG;
		else
			keycode = KEY_PREVIOUSSONG;
		input_report_key(data->input_dev, keycode, 1);
		input_report_key(data->input_dev, keycode, 0);
		input_sync(data->input_dev);
	}
}

static void release_all_keys(struct dock_keyboard_data *data)
{
	int i;
	for (i = 0; i < KBD_MAX; i++) {
		if (dock_keycodes[i].pressed) {
			input_report_key(data->input_dev,
					 dock_keycodes[i].keycode, 0);
			dock_keycodes[i].pressed = false;
		}
	}
	input_sync(data->input_dev);
}

static void dock_keyboard_off_work(struct work_struct *work)
{
	struct dock_keyboard_data *data =
		container_of(work, struct dock_keyboard_data, dwork_off.work);

	data->keyboard_enable = false;
	if (data->power)
		data->power(false);
	data->dockconnected = false;
	data->kl = UNKNOWN_KEYLAYOUT;
	data->handshaking = false;
	release_all_keys(data);
}

static void dock_keyboard_process_scancode(struct dock_keyboard_data *data,
				    unsigned char scancode)
{
	bool press;
	unsigned int keycode;

	/* Do not send the key_event durring the handshake time */
	if (data->handshaking) {
		if (scancode == SCAN_CODE_HS) {
			release_all_keys(data);
			data->release_cnt = 16;
		} else {
			press = ((scancode & MASK_PRESS) != MASK_PRESS);
			keycode = (scancode & MASK_SCAN_CODE);

			if (keycode >= KBD_MIN && keycode <= KBD_MAX) {
				if (press) {
					/* workaround keyboard issue */
					if (dock_keycodes[keycode].pressed) {
						input_report_key
						    (data->input_dev,
						     dock_keycodes
						     [keycode].keycode, 0);
						msleep(20);
					}
					dock_keycodes[keycode].pressed = true;

				} else {
					dock_keycodes[keycode].pressed = false;
				}

				/* for the remap keys */
				if (keycode == SCAN_CODE_REW
				    || keycode == SCAN_CODE_FF) {
					if (press) {
						data->remap_keycode = keycode;
						mod_timer
						    (&data->key_timer,
						     jiffies + HZ / 3);
					} else {
						if (data->remap_state
						    == REMAPKEY_PRESSED) {
							data->remap_state
							    = REMAPKEY_RELEASED;
							input_report_key
							    (data->input_dev,
							     dock_keycodes
							     [keycode].keycode,
							     press);
							input_sync
							    (data->input_dev);
						}
					}
				} else {
					input_report_key
					    (data->input_dev,
					     dock_keycodes
					     [keycode].keycode, press);
					input_sync(data->input_dev);
				}
			}
			if (data->release_cnt > 0) {
				if (press) {
					input_report_key
					    (data->input_dev,
					     dock_keycodes[keycode].keycode, 0);
				}
				data->release_cnt--;
			}
		}
	}
}

static void key_event_work(struct work_struct *work)
{
	unsigned char scancode;
	struct dock_keyboard_data *data = container_of(work,
						       struct
						       dock_keyboard_data,
						       work_msg);

	while (data->buf_front != data->buf_rear) {
		data->buf_front = (1 + data->buf_front) % MAX_BUF;
		scancode = data->key_buf[data->buf_front];
		/* keyboard driver need the contry code */
		if (data->kl == UNKNOWN_KEYLAYOUT) {
			switch (scancode) {
			case US_KEYBOARD:
				data->kl = US_KEYLAYOUT;
				dock_keycodes[49].keycode = KEY_BACKSLASH;
				data->handshaking = true;
				pr_info("kbd: US keyboard is attacted.\n");
				break;

			case UK_KEYBOARD:
				data->kl = UK_KEYLAYOUT;
				dock_keycodes[49].keycode = KEY_NUMERIC_POUND;
				data->handshaking = true;
				pr_info("kbd: UK keyboard is attacted.\n");
				break;

			default:
				pr_warning("kbd: unknown key layout : %x\n",
					   scancode);
				break;
			}
		} else
			dock_keyboard_process_scancode(data, scancode);
	}
}

static void dock_keyboard_write(struct dock_keyboard_data *data,
				unsigned char chr)
{
	if (data->keyboard_enable && data->serio)
		serio_write(data->serio, chr);
}

static int dock_keyboard_event(struct input_dev *dev,
			       unsigned int type, unsigned int code, int value)
{
	struct dock_keyboard_data *data = input_get_drvdata(dev);

	switch (type) {
	case EV_LED:
		if (value)
			dock_keyboard_write(data, CMD_LED_ON);
		else
			dock_keyboard_write(data, CMD_LED_OFF);
		return 0;
	}
	return -EPERM;
}

static int dock_keyboard_cb(struct input_dev *dev, bool connected)
{
	struct dock_keyboard_data *data = input_get_drvdata(dev);
	int try_cnt = 0;
	int max_cnt = 10;

	if (!connected)
		data->dockconnected = false;
	else {
		/* for wakeup case */
		cancel_delayed_work_sync(&data->dwork_off);
		if (data->handshaking) {
			pr_info("kbd: keyboard is reattached\n");
			data->dockconnected = true;
		}

		if (!data->dockconnected) {
			/* for sure 30pin connector is fully connected */
			msleep(400);

			/* for checking handshake */
			data->kl = UNKNOWN_KEYLAYOUT;

			if (!data->keyboard_enable) {
				if (data->power)
					data->power(true);
				data->keyboard_enable = true;
			}

			/* try to get handshake data */
			for (try_cnt = 0; try_cnt < max_cnt; try_cnt++) {
				msleep(50);
				if (data->kl != UNKNOWN_KEYLAYOUT) {
					data->dockconnected = true;
					break;
				}

				/* the accessory is dettached. */
				if (gpio_get_value(data->dock_irq_gpio)) {
					data->dockconnected = false;
					break;
				}
			}
		}
	}

	if (data->dockconnected)
		return 1;
	else {
		cancel_delayed_work_sync(&data->dwork_off);
		schedule_delayed_work(&data->dwork_off, HZ * 2 / 3);
		return 0;
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void dock_keyboard_early_suspend(struct early_suspend *early_sus)
{
	struct dock_keyboard_data *data = container_of(early_sus,
						       struct
						       dock_keyboard_data,
						       early_suspend);
	/* caps lock led off */
	if (data->serio && data->handshaking) {
		dock_keyboard_write(data, CMD_LED_OFF);
		msleep(20);
		dock_keyboard_write(data, CMD_SLEEP);
	}
}

static void dock_keyboard_late_resume(struct early_suspend *early_sus)
{
}
#endif

static int __devinit dock_keyboard_probe(struct platform_device *pdev)
{
	struct dock_keyboard_platform_data *pdata = pdev->dev.platform_data;
	struct dock_keyboard_data *data;
	struct input_dev *input;
	int i;
	int ret;

	pr_debug("kbd: probe\n");

	if (!pdata) {
		pr_err("kbd: invalid platform_data.\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct dock_keyboard_data), GFP_KERNEL);
	if (unlikely(IS_ERR(data))) {
		ret = -ENOMEM;
		goto err_alloc_mem;
	}

	sec_dock_kbd_driver.private_data = data;

	INIT_WORK(&data->work_msg, key_event_work);
	INIT_DELAYED_WORK(&data->dwork_off, dock_keyboard_off_work);
	platform_set_drvdata(pdev, data);

	input = input_allocate_device();
	if (unlikely(IS_ERR(input))) {
		pr_err("kbd: failed to allocate input device.\n");
		ret = -ENOMEM;
		goto err_alloc_input_dev;
	}

	data->input_dev = input;
	input_set_drvdata(data->input_dev, data);
	data->kl = UNKNOWN_KEYLAYOUT;

	input->name = pdev->name;
	input->dev.parent = &pdev->dev;
	input->id.bustype = BUS_RS232;
	input->event = dock_keyboard_event;

	set_bit(EV_SYN, input->evbit);
	set_bit(EV_KEY, input->evbit);
	set_bit(EV_LED, input->evbit);
	set_bit(LED_CAPSL, input->ledbit);

	for (i = 0; i < KEYBOARD_SIZE; i++) {
		if (KEY_RESERVED != dock_keycodes[i].keycode) {
			input_set_capability(input, EV_KEY,
					     dock_keycodes[i].keycode);
		}
	}

	/* for the UK keyboard */
	input_set_capability(input, EV_KEY, KEY_NUMERIC_POUND);

	/* for the remaped keys */
	input_set_capability(input, EV_KEY, KEY_NEXTSONG);
	input_set_capability(input, EV_KEY, KEY_PREVIOUSSONG);

	ret = input_register_device(data->input_dev);
	if (unlikely(ret)) {
		pr_err("kbd: failed to register input device.\n");
		goto err_reg_input_dev;
	}

	data->dock_irq_gpio = pdata->dock_irq_gpio;
	data->power = pdata->power;

	if (pdata->register_cb)
		pdata->register_cb(input, dock_keyboard_cb);

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = dock_keyboard_early_suspend;
	data->early_suspend.resume = dock_keyboard_late_resume;
	register_early_suspend(&data->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	init_timer(&data->key_timer);
	data->key_timer.data = (unsigned long)data;
	data->key_timer.expires = jiffies + HZ / 3;
	data->key_timer.function = remapkey_timer;

	ret = serio_register_driver(&sec_dock_kbd_driver.serio_drv);
	if (unlikely(ret)) {
		pr_err("kbd: failed to register serio driver!\n");
		goto err_reg_serio_drv;
	}

	return 0;

err_reg_serio_drv:
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	if (pdata->register_cb)
		pdata->register_cb(NULL, NULL);
	input_unregister_device(input);
err_reg_input_dev:
	input_free_device(input);
	input_set_drvdata(input, NULL);
err_alloc_input_dev:
	kfree(data);
err_alloc_mem:
	return ret;
}

static int __devexit dock_keyboard_remove(struct platform_device *pdev)
{
	struct dock_keyboard_platform_data *pdata = pdev->dev.platform_data;
	struct dock_keyboard_data *data = platform_get_drvdata(pdev);

	pr_debug("kbd: remove\n");

	serio_unregister_driver(&sec_dock_kbd_driver.serio_drv);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	if (pdata->register_cb)
		pdata->register_cb(NULL, NULL);
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	input_set_drvdata(data->input_dev, NULL);
	kfree(data);

	return 0;
}

static int dock_keyboard_serio_connect(struct serio *serio,
				       struct serio_driver *drv)
{
	int ret;
	struct dock_keyboard_data *data;

	pr_debug("kbd: serio_connect\n");

	data = sec_dock_kbd_driver.private_data;
	data->serio = serio;
	serio_set_drvdata(serio, data);

	ret = serio_open(serio, drv);

	if (unlikely(ret)) {
		pr_err("kbd: failed to open serio!\n");
		goto err_open_serio;
	}

	if (unlikely(!sec_dock_kbd_driver.private_data)) {
		pr_err("kbd: failed to get platform device data!\n");
		ret = -ENODEV;
		goto err_plat_dev;
	}

	return 0;

err_plat_dev:
err_open_serio:
	serio_set_drvdata(serio, NULL);
	return ret;
}

static void dock_keyboard_serio_disconnect(struct serio *serio)
{
	struct dock_keyboard_data *data = serio_get_drvdata(serio);

	pr_debug("kbd: serio_disconnect\n");

	data->serio = NULL;
	serio_close(serio);
	serio_set_drvdata(serio, NULL);
}

static irqreturn_t dock_keyboard_serio_interrupt(struct serio *serio,
						 unsigned char keycode,
						 unsigned int flags)
{
	struct dock_keyboard_data *data = serio_get_drvdata(serio);

	if (data->keyboard_enable) {
		data->buf_rear = (1 + data->buf_rear) % MAX_BUF;
		if (data->buf_front == data->buf_rear) {
			if (data->buf_rear == 0)
				data->buf_rear = MAX_BUF;
			else
				data->buf_rear--;
		} else
			data->key_buf[data->buf_rear] = keycode;

		if (!work_pending(&data->work_msg))
			schedule_work(&data->work_msg);
	}

	return IRQ_HANDLED;
}

static int __init dock_keyboard_init(void)
{
	return platform_driver_register(&sec_dock_kbd_driver.plat_drv);
}

static void __exit dock_keyboard_exit(void)
{
	platform_driver_unregister(&sec_dock_kbd_driver.plat_drv);
}

late_initcall(dock_keyboard_init);
module_exit(dock_keyboard_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SEC P series Dock Keyboard driver");

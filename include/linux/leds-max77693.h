/*
 * leds-max77693.h - Flash-led driver for Maxim MAX77693
 *
 * Copyright (C) 2011 Samsung Electronics
 * ByungChang Cha <bc.cha@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LEDS_MAX77693_H__
#define __LEDS_MAX77693_H__

#define MAX_FLASH_CURRENT       1000    /* 1000mA(0x1f) */
#define MAX_TORCH_CURRENT       250     /* 250mA(0x0f) */
#define MAX_FLASH_DRV_LEVEL     63      /* 15.625 + 15.625*63 mA */
#define MAX_TORCH_DRV_LEVEL     15      /* 15.625 + 15.625*15 mA */

enum max77693_led_id {
	MAX77693_FLASH_LED_1 = 0,
	MAX77693_FLASH_LED_2,
	MAX77693_TORCH_LED_1,
	MAX77693_TORCH_LED_2,
	MAX77693_LED_MAX,
};

enum max77693_led_time {
	MAX77693_FLASH_TIME_62P5MS,
	MAX77693_FLASH_TIME_125MS,
	MAX77693_FLASH_TIME_187P5MS,
	MAX77693_FLASH_TIME_250MS,
	MAX77693_FLASH_TIME_312P5MS,
	MAX77693_FLASH_TIME_375MS,
	MAX77693_FLASH_TIME_437P5MS,
	MAX77693_FLASH_TIME_500MS,
	MAX77693_FLASH_TIME_562P5MS,
	MAX77693_FLASH_TIME_625MS,
	MAX77693_FLASH_TIME_687P5MS,
	MAX77693_FLASH_TIME_750MS,
	MAX77693_FLASH_TIME_812P5MS,
	MAX77693_FLASH_TIME_875MS,
	MAX77693_FLASH_TIME_937P5MS,
	MAX77693_FLASH_TIME_1000MS,
	MAX77693_FLASH_TIME_MAX,
};

enum max77693_torch_time {
	MAX77693_TORCH_TIME_262MS,
	MAX77693_TORCH_TIME_524MS,
	MAX77693_TORCH_TIME_786MS,
	MAX77693_TORCH_TIME_1048MS,
	MAX77693_TORCH_TIME_1572MS,
	MAX77693_TORCH_TIME_2096MS,
	MAX77693_TORCH_TIME_2620MS,
	MAX77693_TORCH_TIME_3114MS,
	MAX77693_TORCH_TIME_4193MS,
	MAX77693_TORCH_TIME_5242MS,
	MAX77693_TORCH_TIME_6291MS,
	MAX77693_TORCH_TIME_7340MS,
	MAX77693_TORCH_TIME_9437MS,
	MAX77693_TORCH_TIME_11534MS,
	MAX77693_TORCH_TIME_13631MS,
	MAX77693_TORCH_TIME_15728MS,
	MAX77693_TORCH_TIME_MAX,
};

enum max77693_timer_mode {
	MAX77693_TIMER_MODE_ONE_SHOT,
	MAX77693_TIMER_MODE_MAX_TIMER,
};

enum max77693_led_cntrl_mode {
	MAX77693_LED_CTRL_BY_FLASHSTB,
	MAX77693_LED_CTRL_BY_I2C,
};

struct max77693_led {
	const char			*name;
	const char			*default_trigger;
	int				id;
	int				timer;
	int				brightness;
	enum max77693_timer_mode	timer_mode;
	enum max77693_led_cntrl_mode	cntrl_mode;
};

struct max77693_led_platform_data {
	int num_leds;
	struct max77693_led leds[MAX77693_LED_MAX];
};

int max77693_get_flash_brightness(enum max77693_led_id id);
void max77693_set_flash_brightness(enum max77693_led_id id, int brightness);

#endif

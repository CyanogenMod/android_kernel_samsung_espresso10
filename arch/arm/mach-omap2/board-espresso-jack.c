/* arch/arm/mach-omap2/board-espresso-jack.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd
 *
 * Based on mach-omap2/board-espresso.c
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

#include <linux/gpio.h>
#include <linux/i2c/twl6030-madc.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sec_jack.h>

#include "board-espresso.h"
#include "control.h"
#include "mux.h"
#include "omap_muxtbl.h"

#define ADC_CHANNEL_JACK	2

static unsigned int gpio_ear_micbias_en;

static void sec_jack_set_micbias_state(bool on)
{
	gpio_set_value(gpio_ear_micbias_en, on);
}

static struct sec_jack_zone sec_jack_zones[] = {
	{
		/* adc < 350, unstable zone, default to 3pole if it stays
		* in this range for a half second (20ms delays, 25 samples)
		*/
		.adc_high	= 350,
		.delay_ms	= 20,
		.check_count	= 25,
		.jack_type	= SEC_HEADSET_3POLE,
	},
	{
		/* 350 < adc <= 759, unstable zone,
		* default to 3pole if it stays
		* in this range for a second (10ms delays, 100 samples)
		*/
		.adc_high	= 759,
		.delay_ms	= 10,
		.check_count	= 25,
		.jack_type	= SEC_HEADSET_3POLE,
	},
	{
		/* 759 < adc <= 950, unstable zone, default to 4pole if it
		* stays in this range for a second (10ms delays, 100 samples)
		*/
		.adc_high	= 950,
		.delay_ms	= 10,
		.check_count	= 25,
		.jack_type	= SEC_HEADSET_4POLE,
	},
	{
		/* 950 < adc <= 2100, 4 pole zone, default to 4pole if it
		* stays in this range for 200ms (20ms delays, 10 samples)
		*/
		.adc_high	= 2100,
		.delay_ms	= 20,
		.check_count	= 10,
		.jack_type	= SEC_HEADSET_4POLE,
	},
	{
		/* adc > 2100, unstable zone, default to 3pole if it stays
		* in this range for a second (10ms delays, 100 samples)
		*/
		.adc_high	= 0x7fffffff,
		.delay_ms	= 10,
		.check_count	= 100,
		.jack_type	= SEC_HEADSET_3POLE,
	},
};

/* To support 3-buttons earjack */
static struct sec_jack_buttons_zone sec_jack_buttons_zones[] = {
	{
		/* 0 <= adc <= 108, stable zone */
		.code		= KEY_MEDIA,
		.adc_low	= 0,
		.adc_high	= 108,
	},
	{
		/* 109 <= adc <= 250, stable zone */
		.code		= KEY_VOLUMEUP,
		.adc_low	= 109,
		.adc_high	= 250,
	},
	{
		/* 251 <= adc <= 530, stable zone */
		.code		= KEY_VOLUMEDOWN,
		.adc_low	= 251,
		.adc_high	= 530,
	},
};

static int sec_jack_get_adc_value(void)
{
	int value;
	value = omap4_espresso_get_adc(EAR_ADC_35);
	return (int)(3300*value) / 4095;
}

struct sec_jack_platform_data sec_jack_pdata = {
	.set_micbias_state	= sec_jack_set_micbias_state,
	.get_adc_value		= sec_jack_get_adc_value,
	.zones			= sec_jack_zones,
	.num_zones		= ARRAY_SIZE(sec_jack_zones),
	.buttons_zones		= sec_jack_buttons_zones,
	.num_buttons_zones	= ARRAY_SIZE(sec_jack_buttons_zones),
#ifdef CONFIG_JACK_RESELECTOR_SUPPORT
	.ear_reselector_zone    = 1960,
#endif
};

static struct platform_device sec_device_jack = {
	.name			= "sec_jack",
	.id			= 1, /* will be used also for gpio_event id */
	.dev.platform_data	= &sec_jack_pdata,
};

enum {
	GPIO_DET_3_5 = 0,
	GPIO_EAR_SEND_END,
	GPIO_EAR_MICBIAS_EN
};

void __init omap4_espresso_jack_init(void)
{
	struct gpio jack_gpios[] = {
		[GPIO_DET_3_5] = {
			.flags	= GPIOF_IN,
			.label	= "DET_3.5",
		},
		[GPIO_EAR_SEND_END] = {
			.flags	= GPIOF_IN,
			.label	= "EAR_SEND_END",
		},
		[GPIO_EAR_MICBIAS_EN] = {
			.flags	= GPIOF_OUT_INIT_LOW,
			.label	= "EAR_MICBIAS_EN",
		},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(jack_gpios); i++)
		jack_gpios[i].gpio =
			omap_muxtbl_get_gpio_by_name(jack_gpios[i].label);
	gpio_request_array(jack_gpios, ARRAY_SIZE(jack_gpios));

	sec_jack_pdata.det_gpio = jack_gpios[GPIO_DET_3_5].gpio;
	sec_jack_pdata.send_end_gpio = jack_gpios[GPIO_EAR_SEND_END].gpio;

	gpio_ear_micbias_en = jack_gpios[GPIO_EAR_MICBIAS_EN].gpio;

	gpio_free(sec_jack_pdata.det_gpio);
	gpio_free(sec_jack_pdata.send_end_gpio);

	platform_device_register(&sec_device_jack);
}

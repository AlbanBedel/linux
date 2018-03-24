/*
 *  TP-LINK TL-WR1043N/ND board support
 *
 *  Copyright (C) 2015 Alban Bedel <albeu@free.fr>
 *  Copyright (C) 2009-2012 Gabor Juhos <juhosg@openwrt.org>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/platform_device.h>

#include <asm/mach-ath79/ath79.h>
#include <asm/mach-ath79/ar71xx_regs.h>

#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-spi.h"
#include "dev-usb.h"
#include "dev-wmac.h"
#include "machtypes.h"

#define TL_WR1043ND_GPIO_LED_USB        1
#define TL_WR1043ND_GPIO_LED_SYSTEM     2
#define TL_WR1043ND_GPIO_LED_QSS        5
#define TL_WR1043ND_GPIO_LED_WLAN       9

#define TL_WR1043ND_GPIO_BTN_RESET      3
#define TL_WR1043ND_GPIO_BTN_QSS        7

#define TL_WR1043ND_GPIO_RTL8366_SDA	18
#define TL_WR1043ND_GPIO_RTL8366_SCK	19

#define TL_WR1043ND_KEYS_POLL_INTERVAL	20	/* msecs */
#define TL_WR1043ND_KEYS_DEBOUNCE_INTERVAL (3 * TL_WR1043ND_KEYS_POLL_INTERVAL)

static struct gpio_led tl_wr1043nd_leds_gpio[] __initdata = {
	{
		.name		= "tp-link:green:usb",
		.gpio		= TL_WR1043ND_GPIO_LED_USB,
		.active_low	= 1,
	}, {
		.name		= "tp-link:green:system",
		.gpio		= TL_WR1043ND_GPIO_LED_SYSTEM,
		.active_low	= 1,
	}, {
		.name		= "tp-link:green:qss",
		.gpio		= TL_WR1043ND_GPIO_LED_QSS,
		.active_low	= 0,
	}, {
		.name		= "tp-link:green:wlan",
		.gpio		= TL_WR1043ND_GPIO_LED_WLAN,
		.active_low	= 1,
	}
};

static struct gpio_keys_button tl_wr1043nd_gpio_keys[] __initdata = {
	{
		.desc		= "reset",
		.type		= EV_KEY,
		.code		= KEY_RESTART,
		.debounce_interval = TL_WR1043ND_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= TL_WR1043ND_GPIO_BTN_RESET,
		.active_low	= 1,
	}, {
		.desc		= "qss",
		.type		= EV_KEY,
		.code		= KEY_WPS_BUTTON,
		.debounce_interval = TL_WR1043ND_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= TL_WR1043ND_GPIO_BTN_QSS,
		.active_low	= 1,
	}
};

static struct spi_board_info tl_wr1043nd_spi_info[] = {
	{
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 25000000,
		.modalias	= "s25sl064a",
	}
};

static struct ath79_spi_platform_data tl_wr1043nd_spi_data = {
	.bus_num	= 0,
	.num_chipselect = 1,
};

static void __init tl_wr1043nd_setup(void)
{
	u8 *eeprom = (u8 *) KSEG1ADDR(0x1fff1000);

	ath79_register_leds_gpio(-1, ARRAY_SIZE(tl_wr1043nd_leds_gpio),
				 tl_wr1043nd_leds_gpio);
	ath79_register_gpio_keys_polled(-1, TL_WR1043ND_KEYS_POLL_INTERVAL,
					ARRAY_SIZE(tl_wr1043nd_gpio_keys),
					tl_wr1043nd_gpio_keys);

	ath79_register_spi(&tl_wr1043nd_spi_data, tl_wr1043nd_spi_info,
			   ARRAY_SIZE(tl_wr1043nd_spi_info));
	ath79_register_usb();
	ath79_register_wmac(eeprom);
}

MIPS_MACHINE(ATH79_MACH_TL_WR1043ND, "TL-WR1043ND", "TP-LINK TL-WR1043ND",
	     tl_wr1043nd_setup);

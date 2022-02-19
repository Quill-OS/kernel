/*
* Simple driver for Texas Instruments TLC5947 LED Flash driver chip
* Copyright (C) 2012 Texas Instruments
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/

#ifndef __LINUX_TLC5947_H
#define __LINUX_TLC5947_H

#define TLC5947_NAME "tlc5947_bl"

#define TLC5947_MAX_BRIGHTNESS 255
struct tlc5947_platform_data {
	int gpio_power_on;
	int gpio_xlat;
	int gpio_blank;
};

#endif /* __LINUX_TLC5947_H */

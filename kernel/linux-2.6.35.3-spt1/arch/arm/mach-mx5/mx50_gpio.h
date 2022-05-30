/*
 * Copyright 2007-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef	__MX50_GPIO_H__
#define	__MX50_GPIO_H__




#define	_MX50_GPIO(num, bit)	(((num) - 1) * 32 + (bit))


#define	WAN_POWER_EN	_MX50_GPIO(4,  3)
#define	WWAN_DISABLE	_MX50_GPIO(4,  7)
#define WAN_WAKE	_MX50_GPIO(4,  4) /* add 2011/06/15 */

#define	AUD_MUTE		_MX50_GPIO(5, 14)
#define	AUD_RESET		_MX50_GPIO(6,  3)
#define	HP_DETECT		_MX50_GPIO(5, 15)


#define	LOG_INT			_MX50_GPIO(5, 24)



void
gpio_audio_activate(
	void
);

void
gpio_audio_mute(
	int		/* mute */
);

void
gpio_audio_enable(
	int		/* enable */
);


int
gpio_logint_activate(
	void
);

void gpio_wwan_active(void);


#endif	/* #ifndef __MX50_GPIO_H__ */

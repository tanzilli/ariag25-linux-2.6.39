/*
 * Real Time Clock workaround header file
 * 	apply to at91sam9x5 family Engineering Samples
 *
 * Copyright (C) 2010 Atmel, Nicolas Ferre <nicolas.ferre@atmel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

static u32 sam9x5es_rtc_imr = 0;

#define at91_sys_read(x)	(		\
		(x) == AT91_RTC_IMR? 		\
			sam9x5es_rtc_imr:	\
			at91_sys_read(x)	\
	)

#define at91_sys_write(y, x)	do {			\
	if ((y) == AT91_RTC_IDR) {			\
		at91_sys_write(AT91_RTC_IDR, (x));	\
		sam9x5es_rtc_imr &= ~(x);		\
	} else if ((y) == AT91_RTC_IER) {		\
		sam9x5es_rtc_imr |= (x);		\
		at91_sys_write(AT91_RTC_IER, (x));	\
	} else {					\
		at91_sys_write((y), (x));		\
	}						\
	} while (0)

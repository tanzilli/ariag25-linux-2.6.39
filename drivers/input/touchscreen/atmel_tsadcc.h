/*
 *  Header file for AT91/AT32 ADC + touchscreen Controller
 *
 *  Data structure and register user interface
 *
 *  Copyright (C) 2010 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ATMEL_TSADCC_H__
#define __ATMEL_TSADCC_H__

/* Register definitions based on AT91SAM9RL64 preliminary draft datasheet */
#define ATMEL_TSADCC_CR		0x00	/* Control register */
#define   ATMEL_TSADCC_SWRST	(1 << 0)	/* Software Reset*/
#define	  ATMEL_TSADCC_START	(1 << 1)	/* Start conversion */

#define ATMEL_TSADCC_MR		0x04	/* Mode register */
#define	  ATMEL_TSADCC_TSAMOD	(3    <<  0)	/* ADC mode */
#define	    ATMEL_TSADCC_TSAMOD_ADC_ONLY_MODE	(0x0)	/* ADC Mode */
#define	    ATMEL_TSADCC_TSAMOD_TS_ONLY_MODE	(0x1)	/* Touch Screen Only Mode */
#define	  ATMEL_TSADCC_LOWRES	(1    <<  4)	/* Resolution selection */
#define	  ATMEL_TSADCC_SLEEP	(1    <<  5)	/* Sleep mode */
#define	  ATMEL_TSADCC_PENDET	(1    <<  6)	/* Pen Detect selection */
#define   ATMEL_TSADCC_FWUP	(1    <<  6)	/* Fast Wake Up selection (5series) */
#define	  ATMEL_TSADCC_PRES	(1    <<  7)	/* Pressure Measurement Selection */
#define	  ATMEL_TSADCC_PRESCAL	(0x3f <<  8)	/* Prescalar Rate Selection */
#define	  ATMEL_TSADCC_EPRESCAL	(0xff <<  8)	/* Prescalar Rate Selection (Extended) */
#define	  ATMEL_TSADCC_STARTUP	(0x7f << 16)	/* Start Up time */
#define	  ATMEL_TSADCC_SHTIM	(0xf  << 24)	/* Sample & Hold time */
#define	  ATMEL_TSADCC_PENDBC	(0xf  << 28)	/* Pen Detect debouncing time */

#define ATMEL_TSADCC_TRGR	0x08	/* Trigger register */
#define	  ATMEL_TSADCC_TRGMOD	(7      <<  0)	/* Trigger mode */
#define	    ATMEL_TSADCC_TRGMOD_NONE		(0 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_RISING	(1 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_FALLING	(2 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_ANY		(3 << 0)
#define     ATMEL_TSADCC_TRGMOD_PENDET		(4 << 0)
#define     ATMEL_TSADCC_TRGMOD_PERIOD		(5 << 0)
#define     ATMEL_TSADCC_TRGMOD_CONTINUOUS	(6 << 0)
#define   ATMEL_TSADCC_TRGPER	(0xffff << 16)	/* Trigger period */

#define ATMEL_TSADCC_TSR	0x0C	/* Touch Screen register */
#define	  ATMEL_TSADCC_TSFREQ	(0xf <<  0)	/* TS Frequency in Interleaved mode */
#define	  ATMEL_TSADCC_TSSHTIM	(0xf << 24)	/* Sample & Hold time */

#define ATMEL_TSADCC_CHER	0x10	/* Channel Enable register */
#define ATMEL_TSADCC_CHDR	0x14	/* Channel Disable register */
#define ATMEL_TSADCC_CHSR	0x18	/* Channel Status register */
#define	  ATMEL_TSADCC_CH(n)	(1 << (n))	/* Channel number */

#define ATMEL_TSADCC_SR		0x1C	/* Status register */
#define	  ATMEL_TSADCC_EOC(n)	(1 << ((n)+0))	/* End of conversion for channel N */
#define	  ATMEL_TSADCC_OVRE(n)	(1 << ((n)+8))	/* Overrun error for channel N */
#define	  ATMEL_TSADCC_DRDY	(1 << 16)	/* Data Ready */
#define	  ATMEL_TSADCC_GOVRE	(1 << 17)	/* General Overrun Error */
#define	  ATMEL_TSADCC_ENDRX	(1 << 18)	/* End of RX Buffer */
#define	  ATMEL_TSADCC_RXBUFF	(1 << 19)	/* TX Buffer full */
#define	  ATMEL_TSADCC_PENCNT	(1 << 20)	/* Pen contact */
#define	  ATMEL_TSADCC_NOCNT	(1 << 21)	/* No contact */

#define ATMEL_TSADCC_LCDR	0x20	/* Last Converted Data register */
#define	  ATMEL_TSADCC_DATA	(0x3ff << 0)	/* Channel data */

#define ATMEL_TSADCC_IER	0x24	/* Interrupt Enable register */
#define ATMEL_TSADCC_IDR	0x28	/* Interrupt Disable register */
#define ATMEL_TSADCC_IMR	0x2C	/* Interrupt Mask register */
#define ATMEL_TSADCC_CDR0	0x30	/* Channel Data 0 */
#define ATMEL_TSADCC_CDR1	0x34	/* Channel Data 1 */
#define ATMEL_TSADCC_CDR2	0x38	/* Channel Data 2 */
#define ATMEL_TSADCC_CDR3	0x3C	/* Channel Data 3 */
#define ATMEL_TSADCC_CDR4	0x40	/* Channel Data 4 */
#define ATMEL_TSADCC_CDR5	0x44	/* Channel Data 5 */

#define ATMEL_TSADCC_XPOS	0x50
#define ATMEL_TSADCC_Z1DAT	0x54
#define ATMEL_TSADCC_Z2DAT	0x58

#define ATMEL_TSADCC_CONVERSION_END		(ATMEL_TSADCC_EOC(3))

/* Register definitions based on AT91SAM9X5 preliminary draft datasheet */
#define	  ATMEL_TSADCC_TRACKTIM		(0x0f << 24)	/* Tracking Time */

#define ATMEL_TSADCC_ISR	0x30	/* Interrupt Status register */
#define	  ATMEL_TSADCC_XRDY		(1 << 20)	/* Measure XPOS Ready */
#define	  ATMEL_TSADCC_YRDY		(1 << 21)	/* Measure YPOS Ready */
#define	  ATMEL_TSADCC_PRDY		(1 << 22)	/* Measure Pressure Ready */
#define	  ATMEL_TSADCC_COMPE		(1 << 26)	/* Comparison Event */
#define	  ATMEL_TSADCC_PEN		(1 << 29)	/* Pen Contact */
#define	  ATMEL_TSADCC_NOPEN		(1 << 30)	/* No Pen Contact */
#define	  ATMEL_TSADCC_PENDET_STATUS	(1 << 31)	/* Pen Detect Status (not interrupt source) */

#define ATMEL_TSADCC_ACR	0x94	/* Analog Control Register */
#define	  ATMEL_TSADCC_PENDET_SENSITIVITY	(0x3 << 0)	/* ADC internal resistor */

#define ATMEL_TSADCC_TSMR	0xb0
#define	  ATMEL_TSADCC_TSMODE		(3 <<  0)	/* Touch Screen Mode */
#define	    ATMEL_TSADCC_TSMODE_NO		(0 << 0)	/* No Touch Screen */
#define	    ATMEL_TSADCC_TSMODE_4WIRE_NO_PRESS	(1 << 0)	/* 4-wire Touch Screen without pressure measurement */
#define	    ATMEL_TSADCC_TSMODE_4WIRE_PRESS	(2 << 0)	/* 4-wire Touch Screen with pressure measurement */
#define	    ATMEL_TSADCC_TSMODE_5WIRE		(3 << 0)	/* 5-wire Touch Screen */
#define	  ATMEL_TSADCC_TSAV		(3 <<  4)	/* Touch Screen Average */
#define	    ATMEL_TSADCC_TSAV_1			(0 << 4)	/* No filtering. Only one conversion ADC per measure */
#define	    ATMEL_TSADCC_TSAV_2			(1 << 4)	/* Averages 2 ADC conversions */
#define	    ATMEL_TSADCC_TSAV_4			(2 << 4)	/* Averages 4 ADC conversions */
#define	    ATMEL_TSADCC_TSAV_8			(3 << 4)	/* Averages 8 ADC conversions */
#define	  ATMEL_TSADCC_TSSCTIM		(0x0f << 16)	/* Touch Screen switches closure time */

#define	  ATMEL_TSADCC_NOTSDMA		(1 << 22)	/* No Touchscreen DMA */
#define	  ATMEL_TSADCC_PENDET_DIS	(0 << 24)	/* Pen contact detection disable */
#define	  ATMEL_TSADCC_PENDET_ENA	(1 << 24)	/* Pen contact detection enable */

#define ATMEL_TSADCC_XPOSR	0xb4
#define	  ATMEL_TSADCC_XSCALE		(0x3ff << 16)	/* Scale of X Position */

#define ATMEL_TSADCC_YPOSR	0xb8
#define	  ATMEL_TSADCC_YPOS		(0x3ff <<  0)	/* Y Position */
#define	  ATMEL_TSADCC_YSCALE		(0x3ff << 16)	/* Scale of Y Position */

#define ATMEL_TSADCC_PRESSR	0xbc	/* Touchscreen Pressure Register */
#define	  ATMEL_TSADCC_PRESSR_Z1	(0x3ff <<  0)	/* Data of Z1 Measurement */
#define	  ATMEL_TSADCC_PRESSR_Z2	(0x3ff << 16)	/* Data of Z2 Measurement */

/* 9x5 ADC registers which conflict with previous definition */
#ifdef CONFIG_ARCH_AT91SAM9X5
#undef	 ATMEL_TSADCC_TRGR
#undef	 ATMEL_TSADCC_SR
#define ATMEL_TSADCC_SR		ATMEL_TSADCC_ISR
#define ATMEL_TSADCC_TRGR	0xc0

/* For code compatiable, redefine with 9x5 value */
#undef	  ATMEL_TSADCC_STARTUP
#define	  ATMEL_TSADCC_STARTUP		(0x0f << 16)	/* Startup Time */
#undef	  ATMEL_TSADCC_DRDY
#define	  ATMEL_TSADCC_DRDY		(1 << 24)	/* Data Ready */
#undef	  ATMEL_TSADCC_GOVRE
#define	  ATMEL_TSADCC_GOVRE		(1 << 25)	/* General Overrun */
#undef	  ATMEL_TSADCC_TSFREQ
#define	  ATMEL_TSADCC_TSFREQ		(0x0f <<  8)	/* Touch Screen Frequency */
#undef	  ATMEL_TSADCC_PENDET
#define	  ATMEL_TSADCC_PENDET		(1 << 24)	/* Pen Contact Detection Enable */
#undef	  ATMEL_TSADCC_XPOS
#define	  ATMEL_TSADCC_XPOS		(0x3ff <<  0)	/* X Position */

#undef	  ATMEL_TSADCC_NOCNT
#define	  ATMEL_TSADCC_NOCNT	ATMEL_TSADCC_NOPEN
#undef	  ATMEL_TSADCC_PENCNT
#define	  ATMEL_TSADCC_PENCNT	ATMEL_TSADCC_PEN
#undef	  ATMEL_TSADCC_CONVERSION_END
#define	  ATMEL_TSADCC_CONVERSION_END	(ATMEL_TSADCC_XRDY | ATMEL_TSADCC_YRDY | ATMEL_TSADCC_PRDY)

#endif

/* Retrieve prescaler value */
#define PRESCALER_VAL(x)	((x) >> 8)

#endif /* __ATMEL_TSADCC_H__ */

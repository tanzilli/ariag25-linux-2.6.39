/*
 *  CPU module specific setup code for the AT91SAM9x5 family
 *
 *  Copyright (C) 2011 Atmel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/flash.h>
#include <linux/spi/spi.h>
#include <linux/fb.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/clk.h>
#include <mach/cpu.h>

#include <video/atmel_lcdfb.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/at91sam9_smc.h>
#include <mach/at91_shdwc.h>

#include "sam9_smc.h"
#include "generic.h"
#include <mach/board-sam9x5.h>

void __init cm_map_io(void)
{
	/* Initialize processor: 12.000 MHz crystal */
	at91sam9x5_initialize(12000000);

	/* DGBU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

void __init cm_init_irq(void)
{
	at91sam9x5_init_interrupts(NULL);
}

/*
 * SPI devices.
 */
static struct mtd_partition cm_spi_flash_parts[] = {
	{
		.name = "full",
		.offset = 0,
		.size = MTDPART_SIZ_FULL,
	},
	{
		.name = "little",
		.offset = 0,
		.size = 24 * SZ_1K,
	},
	{
		.name = "remaining",
		.offset = MTDPART_OFS_NXTBLK,
		.size = MTDPART_SIZ_FULL,
	},
};

static const struct flash_platform_data cm_spi_flash_data = {
		/*.type           = "sst25vf032b",*/
		.name           = "spi_flash",
		.parts		= cm_spi_flash_parts,
		.nr_parts	= ARRAY_SIZE(cm_spi_flash_parts),
};

static struct spi_board_info cm_spi_devices[] = {
#if defined(CONFIG_SPI_ATMEL) || defined(CONFIG_SPI_ATMEL_MODULE)
#if defined(CONFIG_MTD_M25P80)
	{	/* serial flash chip */
		.modalias	= "m25p80",
		.chip_select	= 0,
		.max_speed_hz	= 15 * 1000 * 1000,
		.bus_num	= 0,
		.mode		= SPI_MODE_0,
		.platform_data  = &cm_spi_flash_data,
		.irq            = -1,
	},
#endif
#endif
};


/*
 * LEDs
 */
static struct gpio_led cm_leds[] = {
	{	/* Green led on Aria G25 SoM */
		.name			= "aria_led",
		.gpio			= AT91_PIN_PB8,
		.default_trigger	= "heartbeat",
	},
};

/*
 * I2C Devices
 */
static struct i2c_board_info __initdata cm_i2c_devices[] = {
	{
		I2C_BOARD_INFO("24c512", 0x50)
	},
};

void __init cm_board_init(u32 *cm_config)
{
	int i;

	*cm_config = 0;

	/* SPI */
	at91_add_device_spi(cm_spi_devices, ARRAY_SIZE(cm_spi_devices));
	/* Check SPI0 usage to take decision in mother board */
	for (i = 0; i < ARRAY_SIZE(cm_spi_devices); i++) {
		if (cm_spi_devices[i].bus_num == 0) {
			*cm_config |= CM_CONFIG_SPI0_ENABLE;
			break;
		}
	}
	/* I2C */
	at91_add_device_i2c(0, cm_i2c_devices, ARRAY_SIZE(cm_i2c_devices));
	*cm_config |= CM_CONFIG_I2C0_ENABLE;
	/* LEDs */
	at91_gpio_leds(cm_leds, ARRAY_SIZE(cm_leds));
}

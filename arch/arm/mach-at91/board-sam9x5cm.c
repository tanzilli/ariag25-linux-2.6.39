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

#include <video/atmel_lcdc.h>

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
 * NAND flash
 */
static struct mtd_partition __initdata cm_nand_partition[] = {
	{
		.name	= "Partition 1",
		.offset	= 0,
		.size	= SZ_64M,
	},
	{
		.name	= "Partition 2",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct mtd_partition * __init nand_partitions(int size, int *num_partitions)
{
	*num_partitions = ARRAY_SIZE(cm_nand_partition);
	return cm_nand_partition;
}

/* det_pin is not connected */
static struct atmel_nand_data __initdata cm_nand_data = {
	.ale		= 21,
	.cle		= 22,
	.enable_pin	= AT91_PIN_PD4,
	.partition_info	= nand_partitions,
#if defined(CONFIG_MTD_NAND_AT91_BUSWIDTH_16)
	.bus_width_16	= 1,
#endif
};

static struct sam9_smc_config __initdata cm_nand_smc_config = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 1,
	.ncs_write_setup	= 0,
	.nwe_setup		= 1,

	.ncs_read_pulse		= 6,
	.nrd_pulse		= 4,
	.ncs_write_pulse	= 5,
	.nwe_pulse		= 3,

	.read_cycle		= 6,
	.write_cycle		= 5,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE | AT91_SMC_EXNWMODE_DISABLE,
	.tdf_cycles		= 1,
};

static void __init cm_add_device_nand(void)
{
	/* setup bus-width (8 or 16) */
	if (cm_nand_data.bus_width_16)
		cm_nand_smc_config.mode |= AT91_SMC_DBW_16;
	else
		cm_nand_smc_config.mode |= AT91_SMC_DBW_8;

	/* revision of board modify NAND wiring */
	if (cm_is_revA()) {
		cm_nand_data.bus_on_d0 = 1;
		cm_nand_data.rdy_pin = AT91_PIN_PD6;
	} else {
		cm_nand_data.bus_on_d0 = 0;
		cm_nand_data.rdy_pin = AT91_PIN_PD5;
	}

	/* configure chip-select 3 (NAND) */
	sam9_smc_configure(3, &cm_nand_smc_config);

	at91_add_device_nand(&cm_nand_data);
}

/*
 * LEDs
 */
static struct gpio_led cm_leds[] = {
	{	/* "left" led, blue, userled1 */
		.name			= "d1",
		.gpio			= AT91_PIN_PB18,
		.default_trigger	= "heartbeat",
	},
	{	/* "right" led, red, userled2 */
		.name			= "d2",
		.gpio			= AT91_PIN_PD21,
		.active_low		= 1,
		.default_trigger	= "mmc0",
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
	/* NAND */
	cm_add_device_nand();
	/* I2C */
	at91_add_device_i2c(0, cm_i2c_devices, ARRAY_SIZE(cm_i2c_devices));
	*cm_config |= CM_CONFIG_I2C0_ENABLE;
	/* LEDs */
	at91_gpio_leds(cm_leds, ARRAY_SIZE(cm_leds));

	/* TODO Remove: only for debugging */
	if (cm_is_revA())
		printk(KERN_CRIT "AT91: CM rev A\n");
	else
		printk(KERN_CRIT "AT91: CM rev B and higher\n");
}

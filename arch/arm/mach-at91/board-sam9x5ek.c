/*
 *  Board-specific setup code for the AT91SAM9x5 Evaluation Kit family
 *
 *  Copyright (C) 2010 Atmel Corporation.
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
#include <mach/atmel_hlcdc.h>
#include <mach/at91sam9_smc.h>
#include <mach/at91_shdwc.h>

#include "sam9_smc.h"
#include "generic.h"
#include <mach/board-sam9x5.h>

static void __init ek_map_io(void)
{
	/* Initialize processor and DBGU */
	cm_map_io();

	/* USART0 on ttyS1. (Rx, Tx) */
	at91_register_uart(AT91SAM9X5_ID_USART0, 1, 0);
}

/*
 * USB Host port (OHCI)
 */
/* Port A is shared with gadget port & Port C is full-speed only */
static struct at91_usbh_data __initdata ek_usbh_fs_data = {
	.ports		= 3,

};

/*
 * USB HS Host port (EHCI)
 */
/* Port A is shared with gadget port */
static struct at91_usbh_data __initdata ek_usbh_hs_data = {
	.ports		= 2,
};


/*
 * USB HS Device port
 */
static struct usba_platform_data __initdata ek_usba_udc_data;


/*
 * MACB Ethernet devices
 */
static struct at91_eth_data __initdata ek_macb0_data = {
	.is_rmii	= 1,
};

static struct at91_eth_data __initdata ek_macb1_data = {
	.phy_irq_pin	= AT91_PIN_PC26,
	.is_rmii	= 1,
};


/*
 * MCI (SD/MMC)
 */
/* mci0 detect_pin is revision dependent */
static struct mci_platform_data __initdata mci0_data = {
	.slot[0] = {
		.bus_width	= 4,
		.wp_pin		= -1,
	},
};

static struct mci_platform_data __initdata mci1_data = {
	.slot[0] = {
		.bus_width	= 4,
		.detect_pin	= AT91_PIN_PD14,
		.wp_pin		= -1,
	},
};


/*
 * LCD Controller
 */
#if defined(CONFIG_FB_ATMEL) || defined(CONFIG_FB_ATMEL_MODULE)
static struct fb_videomode at91_tft_vga_modes[] = {
	{
		.name           = "LG",
		.refresh	= 60,
		.xres		= 800,		.yres		= 480,
		.pixclock	= KHZ2PICOS(22223),

		.left_margin	= 64,		.right_margin	= 64,
		.upper_margin	= 22,		.lower_margin	= 21,
		.hsync_len	= 128,		.vsync_len	= 2,

		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
};

static struct fb_monspecs at91fb_default_monspecs = {
	.manufacturer	= "LG",
	.monitor        = "LB043WQ1",

	.modedb		= at91_tft_vga_modes,
	.modedb_len	= ARRAY_SIZE(at91_tft_vga_modes),
	.hfmin		= 15000,
	.hfmax		= 17640,
	.vfmin		= 57,
	.vfmax		= 67,
};

/* Default output mode is TFT 24 bit */
#define AT91SAM9X5_DEFAULT_LCDCFG5	(LCDC_LCDCFG5_MODE_OUTPUT_24BPP)

/* Driver datas */
static struct atmel_lcdfb_info __initdata ek_lcdc_data = {
	.lcdcon_is_backlight		= true,
	.alpha_enabled			= false,
	.default_bpp			= 24,
	/* In 9x5 default_lcdcon2 is used for LCDCFG5 */
	.default_lcdcon2		= AT91SAM9X5_DEFAULT_LCDCFG5,
	.default_monspecs		= &at91fb_default_monspecs,
	.guard_time			= 9,
	.lcd_wiring_mode		= ATMEL_LCDC_WIRING_RGB,
};

#else
static struct atmel_lcdfb_info __initdata ek_lcdc_data;
#endif

/*
 * Touchscreen
 */
static struct at91_tsadcc_data ek_tsadcc_data = {
	.adc_clock		= 300000,
	/*
	 * XXX: ukl: disable averaging for now at it's broken without a hardware
	 * change
	 */
	.filtering_average	= 0x00,	/* averages 2^filtering_average ADC conversions */
	.pendet_debounce	= 0x0d,
	.pendet_sensitivity	= 0x03,
	.ts_sample_hold_time	= 0x0a,
};

/*
 * GPIO Buttons
 */
#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
static struct gpio_keys_button ek_buttons[] = {
	{	/* BP3, "leftclic" */
		.code		= BTN_LEFT,
		.gpio		= AT91_PIN_PD18,
		.active_low	= 1,
		.desc		= "left_click",
		.wakeup		= 1,
	},
	{	/* BP4, "rightclic" */
		.code		= BTN_RIGHT,
		.gpio		= AT91_PIN_PD19,
		.active_low	= 1,
		.desc		= "right_click",
		.wakeup		= 1,
	},
};

static struct gpio_keys_platform_data ek_button_data = {
	.buttons	= ek_buttons,
	.nbuttons	= ARRAY_SIZE(ek_buttons),
};

static struct platform_device ek_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &ek_button_data,
	}
};

static void __init ek_add_device_buttons(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ek_buttons); i++) {
		at91_set_pulldown(ek_buttons[i].gpio, 0);
		at91_set_gpio_input(ek_buttons[i].gpio, 1);
		at91_set_deglitch(ek_buttons[i].gpio, 1);
	}

	platform_device_register(&ek_button_device);
}
#else
static void __init ek_add_device_buttons(void) {}
#endif

/*
 * I2C Devices
 */
static struct i2c_board_info __initdata ek_i2c_devices[] = {
	{
		I2C_BOARD_INFO("wm8731", 0x1a)
	},
#if defined(CONFIG_KEYBOARD_QT1070)
	{
		I2C_BOARD_INFO("qt1070", 0x1b),
		.irq = AT91_PIN_PA7,
		.flags = I2C_CLIENT_WAKE,
	},
#endif
};

static void __init ek_board_configure_pins(void)
{
	if (ek_is_revA()) {
		/* Port A is shared with gadget port */
		/*ek_usbh_fs_data.vbus_pin[0] = AT91_PIN_PD9;*/
		/*ek_usbh_hs_data.vbus_pin[0] = AT91_PIN_PD9;*/
		ek_usbh_fs_data.vbus_pin[1] = AT91_PIN_PD10;
		ek_usbh_hs_data.vbus_pin[1] = AT91_PIN_PD10;
		/* Port C is full-speed only */
		ek_usbh_fs_data.vbus_pin[2] = AT91_PIN_PD11;

		ek_usba_udc_data.vbus_pin = AT91_PIN_PB8;

		ek_macb0_data.phy_irq_pin = 0;

		mci0_data.slot[0].detect_pin = AT91_PIN_PD13;
	} else {
		/* Port A is shared with gadget port */
		/*ek_usbh_fs_data.vbus_pin[0] = AT91_PIN_PD18;*/
		/*ek_usbh_hs_data.vbus_pin[0] = AT91_PIN_PD18;*/
		ek_usbh_fs_data.vbus_pin[1] = AT91_PIN_PD19;
		ek_usbh_hs_data.vbus_pin[1] = AT91_PIN_PD19;
		/* Port C is full-speed only */
		ek_usbh_fs_data.vbus_pin[2] = AT91_PIN_PD20;

		ek_usba_udc_data.vbus_pin = AT91_PIN_PB16;

		ek_macb0_data.phy_irq_pin = AT91_PIN_PB8;

		mci0_data.slot[0].detect_pin = AT91_PIN_PD15;

#if defined(CONFIG_KEYBOARD_QT1070)
		if (!cpu_is_at91sam9g25())
			/* conflict with ISI */
			at91_set_gpio_input(ek_i2c_devices[1].irq, 1);
#endif
	}
}

static void __init ek_board_init(void)
{
	u32 cm_config;

	cm_board_init(&cm_config);
	ek_board_configure_pins();
	/* Serial */
	at91_add_device_serial();
	/* USB HS Host */
	at91_add_device_usbh_ohci(&ek_usbh_fs_data);
	at91_add_device_usbh_ehci(&ek_usbh_hs_data);
	/* USB HS Device */
	at91_add_device_usba(&ek_usba_udc_data);
	/* Ethernet */
	at91_add_device_eth(0, &ek_macb0_data);
	at91_add_device_eth(1, &ek_macb1_data);
	/* MMC */
	at91_add_device_mci(0, &mci0_data);
	/* Conflict between SPI0 and MCI1 pins */
	if (!(cm_config & CM_CONFIG_SPI0_ENABLE))
		at91_add_device_mci(1, &mci1_data);
	/* I2C */
	if (cm_config & CM_CONFIG_I2C0_ENABLE)
		i2c_register_board_info(0,
				ek_i2c_devices, ARRAY_SIZE(ek_i2c_devices));
	else
		at91_add_device_i2c(0,
				ek_i2c_devices, ARRAY_SIZE(ek_i2c_devices));

	if (!cpu_is_at91sam9g25() && !cpu_is_at91sam9x25()) {
		/* LCD Controller */
		at91_add_device_lcdc(&ek_lcdc_data);
		/* Touch Screen */
		at91_add_device_tsadcc(&ek_tsadcc_data);
	}

#if 0
	if (cpu_is_at91sam9x25() || cpu_is_at91sam9x35())
		/*
		 * open jumper/solderdrop JP11 to activate CAN0
		 *
		 * _note_: this will deactivate the debug uart
		 */
		at91_add_device_can(0, NULL);
#endif

	if (cpu_is_at91sam9x25() || cpu_is_at91sam9x35())
		/* XXX: this conflicts with usart.1 */
		at91_add_device_can(1, NULL);

	/* Push Buttons */
	if (ek_is_revA())
		ek_add_device_buttons();

	/* SSC (for WM8731) */
	at91_add_device_ssc(AT91SAM9X5_ID_SSC, ATMEL_SSC_TX | ATMEL_SSC_RX);

	/* TODO Remove: only for debugging */
	if (ek_is_revA())
		printk(KERN_CRIT "AT91: EK rev A\n");
	else
		printk(KERN_CRIT "AT91: EK rev B and higher\n");
}

MACHINE_START(AT91SAM9X5EK, "Atmel AT91SAM9X5-EK")
	/* Maintainer: Atmel */
/* XXX/ukl: can we drop .boot_params? */
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91sam926x_timer,
	.map_io		= ek_map_io,
	.init_irq	= cm_init_irq,
	.init_machine	= ek_board_init,
MACHINE_END

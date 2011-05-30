/*
 *  On-Chip devices setup code for the AT91SAM9x5 family
 *
 *  Copyright (C) 2010 Atmel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/i2c-gpio.h>
#include <linux/atmel-mci.h>
#include <linux/fb.h>

#include <video/atmel_lcdfb.h>

#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/atmel_hlcdc.h>
#include <mach/cpu.h>
#include <mach/at91sam9x5.h>
#include <mach/at91sam9x5_matrix.h>
#include <mach/at91sam9_smc.h>
#include <mach/at_hdmac.h>
#include <mach/atmel-mci.h>

#include "generic.h"

/* --------------------------------------------------------------------
 *  HDMAC - AHB DMA Controller
 * -------------------------------------------------------------------- */

#if defined(CONFIG_AT_HDMAC) || defined(CONFIG_AT_HDMAC_MODULE)
static u64 hdmac_dmamask = DMA_BIT_MASK(32);

/* a single platform data for both DMA controllers as they share
 * the same characteristics */
static struct at_dma_platform_data atdma_pdata = {
	.nr_channels	= 8,
};

static struct resource hdmac0_resources[] = {
	[0] = {
		.start	= AT91_BASE_SYS + AT91_DMA0,
		.end	= AT91_BASE_SYS + AT91_DMA0 + SZ_512 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_DMA0,
		.end	= AT91SAM9X5_ID_DMA0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at_hdmac0_device = {
	.name		= "at_hdmac",
	.id		= 0,
	.dev		= {
				.dma_mask		= &hdmac_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &atdma_pdata,
	},
	.resource	= hdmac0_resources,
	.num_resources	= ARRAY_SIZE(hdmac0_resources),
};

static struct resource hdmac1_resources[] = {
	[0] = {
		.start	= AT91_BASE_SYS + AT91_DMA1,
		.end	= AT91_BASE_SYS + AT91_DMA1 + SZ_512 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_DMA1,
		.end	= AT91SAM9X5_ID_DMA1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at_hdmac1_device = {
	.name	= "at_hdmac",
	.id	= 1,
	.dev	= {
			.dma_mask			= &hdmac_dmamask,
			.coherent_dma_mask		= DMA_BIT_MASK(32),
			.platform_data			= &atdma_pdata,
	},
	.resource	= hdmac1_resources,
	.num_resources	= ARRAY_SIZE(hdmac1_resources),
};

void __init at91_add_device_hdmac(void)
{
	dma_cap_set(DMA_MEMCPY, atdma_pdata.cap_mask);
	dma_cap_set(DMA_SLAVE, atdma_pdata.cap_mask);
	dma_cap_set(DMA_CYCLIC, atdma_pdata.cap_mask);
	at91_clock_associate("dma0_clk", &at_hdmac0_device.dev, "dma_clk");
	platform_device_register(&at_hdmac0_device);
	at91_clock_associate("dma1_clk", &at_hdmac1_device.dev, "dma_clk");
	platform_device_register(&at_hdmac1_device);
}
#else
void __init at91_add_device_hdmac(void) {}
#endif


/* --------------------------------------------------------------------
 *  USB Host (OHCI)
 * -------------------------------------------------------------------- */

#if defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)
static u64 ohci_dmamask = DMA_BIT_MASK(32);
static struct at91_usbh_data usbh_ohci_data;

static struct resource usbh_ohci_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_OHCI_BASE,
		.end	= AT91SAM9X5_OHCI_BASE + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_UHPHS,
		.end	= AT91SAM9X5_ID_UHPHS,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91_usbh_ohci_device = {
	.name		= "at91_ohci",
	.id		= -1,
	.dev		= {
				.dma_mask		= &ohci_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &usbh_ohci_data,
	},
	.resource	= usbh_ohci_resources,
	.num_resources	= ARRAY_SIZE(usbh_ohci_resources),
};

void __init at91_add_device_usbh_ohci(struct at91_usbh_data *data)
{
	int i;

	if (!data)
		return;

	/* Enable VBus control for UHP ports */
	for (i = 0; i < data->ports; i++) {
		if (data->vbus_pin[i])
			at91_set_gpio_output(data->vbus_pin[i], 0);
	}

	usbh_ohci_data = *data;
	platform_device_register(&at91_usbh_ohci_device);
}
#else
void __init at91_add_device_usbh_ohci(struct at91_usbh_data *data) {}
#endif


/* --------------------------------------------------------------------
 *  USB Host HS (EHCI)
 *  Needs an OHCI host for low and full speed management
 * -------------------------------------------------------------------- */

#if defined(CONFIG_USB_EHCI_HCD) || defined(CONFIG_USB_EHCI_HCD_MODULE)
static u64 ehci_dmamask = DMA_BIT_MASK(32);
static struct at91_usbh_data usbh_ehci_data;

static struct resource usbh_ehci_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_EHCI_BASE,
		.end	= AT91SAM9X5_EHCI_BASE + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_UHPHS,
		.end	= AT91SAM9X5_ID_UHPHS,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91_usbh_ehci_device = {
	.name		= "atmel-ehci",
	.id		= -1,
	.dev		= {
				.dma_mask		= &ehci_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &usbh_ehci_data,
	},
	.resource	= usbh_ehci_resources,
	.num_resources	= ARRAY_SIZE(usbh_ehci_resources),
};

void __init at91_add_device_usbh_ehci(struct at91_usbh_data *data)
{
	int i;

	if (!data)
		return;

	/* Enable VBus control for UHP ports */
	for (i = 0; i < data->ports; i++) {
		if (data->vbus_pin[i])
			at91_set_gpio_output(data->vbus_pin[i], 0);
	}

	usbh_ehci_data = *data;
	at91_clock_associate("uhphs_clk", &at91_usbh_ehci_device.dev, "ehci_clk");
	platform_device_register(&at91_usbh_ehci_device);
}
#else
void __init at91_add_device_usbh_ehci(struct at91_usbh_data *data) {}
#endif


/* --------------------------------------------------------------------
 *  USB HS Device (Gadget)
 * -------------------------------------------------------------------- */

#if defined(CONFIG_USB_GADGET_ATMEL_USBA) || defined(CONFIG_USB_GADGET_ATMEL_USBA_MODULE)
static struct resource usba_udc_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_UDPHS_FIFO,
		.end	= AT91SAM9X5_UDPHS_FIFO + SZ_512K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_BASE_UDPHS,
		.end	= AT91SAM9X5_BASE_UDPHS + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= AT91SAM9X5_ID_UDPHS,
		.end	= AT91SAM9X5_ID_UDPHS,
		.flags	= IORESOURCE_IRQ,
	},
};

#define EP(nam, idx, maxpkt, maxbk, dma, isoc)			\
	[idx] = {						\
		.name		= nam,				\
		.index		= idx,				\
		.fifo_size	= maxpkt,			\
		.nr_banks	= maxbk,			\
		.can_dma	= dma,				\
		.can_isoc	= isoc,				\
	}

static struct usba_ep_data usba_udc_ep[] __initdata = {
	EP("ep0", 0, 64, 1, 0, 0),
	EP("ep1", 1, 1024, 2, 1, 1),
	EP("ep2", 2, 1024, 2, 1, 1),
	EP("ep3", 3, 1024, 3, 1, 0),
	EP("ep4", 4, 1024, 3, 1, 0),
	EP("ep5", 5, 1024, 3, 1, 1),
	EP("ep6", 6, 1024, 3, 1, 1),
};

#undef EP

/*
 * pdata doesn't have room for any endpoints, so we need to
 * append room for the ones we need right after it.
 */
static struct {
	struct usba_platform_data pdata;
	struct usba_ep_data ep[7];
} usba_udc_data;

static struct platform_device at91_usba_udc_device = {
	.name		= "atmel_usba_udc",
	.id		= -1,
	.dev		= {
				.platform_data	= &usba_udc_data.pdata,
	},
	.resource	= usba_udc_resources,
	.num_resources	= ARRAY_SIZE(usba_udc_resources),
};

void __init at91_add_device_usba(struct usba_platform_data *data)
{
	usba_udc_data.pdata.vbus_pin = -EINVAL;
	usba_udc_data.pdata.num_ep = ARRAY_SIZE(usba_udc_ep);
	memcpy(usba_udc_data.ep, usba_udc_ep, sizeof(usba_udc_ep));

	if (data && data->vbus_pin > 0) {
		at91_set_gpio_input(data->vbus_pin, 0);
		at91_set_deglitch(data->vbus_pin, 1);
		usba_udc_data.pdata.vbus_pin = data->vbus_pin;
	}

	/* Pullup pin is handled internally by USB device peripheral */

	/* Clocks */
	at91_clock_associate("utmi_clk", &at91_usba_udc_device.dev, "hclk");
	at91_clock_associate("udphs_clk", &at91_usba_udc_device.dev, "pclk");

	platform_device_register(&at91_usba_udc_device);
}
#else
void __init at91_add_device_usba(struct usba_platform_data *data) {}
#endif

/* --------------------------------------------------------------------
 *  Ethernet
 * -------------------------------------------------------------------- */

#if defined(CONFIG_MACB) || defined(CONFIG_MACB_MODULE)
static u64 eth0_dmamask = DMA_BIT_MASK(32);
static struct at91_eth_data eth0_data;

static struct resource eth0_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_EMAC0,
		.end	= AT91SAM9X5_BASE_EMAC0 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_EMAC0,
		.end	= AT91SAM9X5_ID_EMAC0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_eth0_device = {
	.name		= "macb",
	.id		= 0,
	.dev		= {
				.dma_mask		= &eth0_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &eth0_data,
	},
	.resource	= eth0_resources,
	.num_resources	= ARRAY_SIZE(eth0_resources),
};

static u64 eth1_dmamask = DMA_BIT_MASK(32);
static struct at91_eth_data eth1_data;

static struct resource eth1_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_EMAC1,
		.end	= AT91SAM9X5_BASE_EMAC1 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_EMAC1,
		.end	= AT91SAM9X5_ID_EMAC1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_eth1_device = {
	.name		= "macb",
	.id		= 1,
	.dev		= {
				.dma_mask		= &eth1_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &eth1_data,
	},
	.resource	= eth1_resources,
	.num_resources	= ARRAY_SIZE(eth1_resources),
};

void __init at91_add_device_eth(short eth_id, struct at91_eth_data *data)
{
	if (!data)
		return;

	if (cpu_is_at91sam9g15())
		return;

	if (eth_id && !cpu_is_at91sam9x25())
		return;

	if (data->phy_irq_pin) {
		at91_set_gpio_input(data->phy_irq_pin, 0);
		at91_set_deglitch(data->phy_irq_pin, 1);
	}

	if (eth_id == 0) {
		/* Pins used for MII and RMII */
		at91_set_A_periph(AT91_PIN_PB4,  0);	/* ETXCK_EREFCK */
		at91_set_A_periph(AT91_PIN_PB3,  0);	/* ERXDV */
		at91_set_A_periph(AT91_PIN_PB0,  0);	/* ERX0 */
		at91_set_A_periph(AT91_PIN_PB1,  0);	/* ERX1 */
		at91_set_A_periph(AT91_PIN_PB2,  0);	/* ERXER */
		at91_set_A_periph(AT91_PIN_PB7,  0);	/* ETXEN */
		at91_set_A_periph(AT91_PIN_PB9,  0);	/* ETX0 */
		at91_set_A_periph(AT91_PIN_PB10, 0);	/* ETX1 */
		at91_set_A_periph(AT91_PIN_PB5,  0);	/* EMDIO */
		at91_set_A_periph(AT91_PIN_PB6,  0);	/* EMDC */

		if (!data->is_rmii) {
			at91_set_A_periph(AT91_PIN_PB16, 0);	/* ECRS */
			at91_set_A_periph(AT91_PIN_PB17, 0);	/* ECOL */
			at91_set_A_periph(AT91_PIN_PB13, 0);	/* ERX2 */
			at91_set_A_periph(AT91_PIN_PB14, 0);	/* ERX3 */
			at91_set_A_periph(AT91_PIN_PB15, 0);	/* ERXCK */
			at91_set_A_periph(AT91_PIN_PB11, 0);	/* ETX2 */
			at91_set_A_periph(AT91_PIN_PB12, 0);	/* ETX3 */
			at91_set_A_periph(AT91_PIN_PB8,  0);	/* ETXER */
		}

		/* Clock */
		at91_clock_associate("macb0_clk", &at91sam9x5_eth0_device.dev, "macb_clk");

		eth0_data = *data;
		platform_device_register(&at91sam9x5_eth0_device);
	} else {
		if (!data->is_rmii)
			pr_warn("AT91: Only RMII available on interface %s %d.\n",
				at91sam9x5_eth0_device.name, eth_id);

		/* Pins used for RMII */
		at91_set_B_periph(AT91_PIN_PC29,  0);	/* ETXCK_EREFCK */
		at91_set_B_periph(AT91_PIN_PC28,  0);	/* ECRSDV */
		at91_set_B_periph(AT91_PIN_PC20,  0);	/* ERX0 */
		at91_set_B_periph(AT91_PIN_PC21,  0);	/* ERX1 */
		at91_set_B_periph(AT91_PIN_PC16,  0);	/* ERXER */
		at91_set_B_periph(AT91_PIN_PC27,  0);	/* ETXEN */
		at91_set_B_periph(AT91_PIN_PC18,  0);	/* ETX0 */
		at91_set_B_periph(AT91_PIN_PC19,  0);	/* ETX1 */
		at91_set_B_periph(AT91_PIN_PC31,  0);	/* EMDIO */
		at91_set_B_periph(AT91_PIN_PC30,  0);	/* EMDC */

		/* Clock */
		at91_clock_associate("macb1_clk", &at91sam9x5_eth1_device.dev, "macb_clk");

		eth1_data = *data;
		platform_device_register(&at91sam9x5_eth1_device);
	}
}
#else
void __init at91_add_device_eth(short eth_id, struct at91_eth_data *data) {}
#endif


/* --------------------------------------------------------------------
 *  MMC / SD
 * -------------------------------------------------------------------- */

#if defined(CONFIG_MMC_ATMELMCI) || defined(CONFIG_MMC_ATMELMCI_MODULE)
static u64 mmc_dmamask = DMA_BIT_MASK(32);
static struct mci_platform_data mmc0_data, mmc1_data;

static struct resource mmc0_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_MCI0,
		.end	= AT91SAM9X5_BASE_MCI0 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_MCI0,
		.end	= AT91SAM9X5_ID_MCI0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_mmc0_device = {
	.name		= "atmel_mci",
	.id		= 0,
	.dev		= {
				.dma_mask		= &mmc_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &mmc0_data,
	},
	.resource	= mmc0_resources,
	.num_resources	= ARRAY_SIZE(mmc0_resources),
};

static struct resource mmc1_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_MCI1,
		.end	= AT91SAM9X5_BASE_MCI1 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_MCI1,
		.end	= AT91SAM9X5_ID_MCI1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_mmc1_device = {
	.name		= "atmel_mci",
	.id		= 1,
	.dev		= {
				.dma_mask		= &mmc_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &mmc1_data,
	},
	.resource	= mmc1_resources,
	.num_resources	= ARRAY_SIZE(mmc1_resources),
};

/* Consider only one slot : slot 0 */
void __init at91_add_device_mci(short mmc_id, struct mci_platform_data *data)
{

	if (!data)
		return;

	/* Must have at least one usable slot */
	if (!data->slot[0].bus_width)
		return;

#if defined(CONFIG_AT_HDMAC) || defined(CONFIG_AT_HDMAC_MODULE)
	{
	struct at_dma_slave	*atslave;
	struct mci_dma_data	*alt_atslave;

	alt_atslave = kzalloc(sizeof(struct mci_dma_data), GFP_KERNEL);
	atslave = &alt_atslave->sdata;

	/* DMA slave channel configuration */
	atslave->reg_width = AT_DMA_SLAVE_WIDTH_32BIT;
	atslave->cfg = ATC_FIFOCFG_HALFFIFO
			| ATC_SRC_H2SEL_HW | ATC_DST_H2SEL_HW;
	atslave->ctrla = ATC_SCSIZE_16 | ATC_DCSIZE_16;
	if (mmc_id == 0) {	/* MCI0 */
		atslave->cfg |= ATC_SRC_PER(AT_DMA_ID_MCI0)
			      | ATC_DST_PER(AT_DMA_ID_MCI0);
		atslave->dma_dev = &at_hdmac0_device.dev;

	} else {		/* MCI1 */
		atslave->cfg |= ATC_SRC_PER(AT_DMA_ID_MCI1)
			      | ATC_DST_PER(AT_DMA_ID_MCI1);
		atslave->dma_dev = &at_hdmac1_device.dev;
	}

	data->dma_slave = alt_atslave;
	}
#endif

	/* input/irq */
	if (data->slot[0].detect_pin) {
		at91_set_gpio_input(data->slot[0].detect_pin, 1);
		at91_set_deglitch(data->slot[0].detect_pin, 1);
	}
	if (data->slot[0].wp_pin)
		at91_set_gpio_input(data->slot[0].wp_pin, 1);

	if (mmc_id == 0) {		/* MCI0 */

		/* CLK */
		at91_set_A_periph(AT91_PIN_PA17, 0);

		/* CMD */
		at91_set_A_periph(AT91_PIN_PA16, 1);

		/* DAT0, maybe DAT1..DAT3 */
		at91_set_A_periph(AT91_PIN_PA15, 1);
		if (data->slot[0].bus_width == 4) {
			at91_set_A_periph(AT91_PIN_PA18, 1);
			at91_set_A_periph(AT91_PIN_PA19, 1);
			at91_set_A_periph(AT91_PIN_PA20, 1);
		}

		mmc0_data = *data;
		at91_clock_associate("mci0_clk", &at91sam9x5_mmc0_device.dev, "mci_clk");
		platform_device_register(&at91sam9x5_mmc0_device);

	} else {			/* MCI1 */

		/* CLK */
		at91_set_B_periph(AT91_PIN_PA13, 0);

		/* CMD */
		at91_set_B_periph(AT91_PIN_PA12, 1);

		/* DAT0, maybe DAT1..DAT3 */
		at91_set_B_periph(AT91_PIN_PA11, 1);
		if (data->slot[0].bus_width == 4) {
			at91_set_B_periph(AT91_PIN_PA2, 1);
			at91_set_B_periph(AT91_PIN_PA3, 1);
			at91_set_B_periph(AT91_PIN_PA4, 1);
		}

		mmc1_data = *data;
		at91_clock_associate("mci1_clk", &at91sam9x5_mmc1_device.dev, "mci_clk");
		platform_device_register(&at91sam9x5_mmc1_device);

	}
}
#else
void __init at91_add_device_mci(short mmc_id, struct mci_platform_data *data) {}
#endif


/* --------------------------------------------------------------------
 *  NAND / SmartMedia
 * -------------------------------------------------------------------- */

#if defined(CONFIG_MTD_NAND_ATMEL) || defined(CONFIG_MTD_NAND_ATMEL_MODULE)
static struct atmel_nand_data nand_data;

#define NAND_BASE	AT91_CHIPSELECT_3

static struct resource nand_resources[] = {
	[0] = {
		.start	= NAND_BASE,
		.end	= NAND_BASE + SZ_256M - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91_BASE_SYS + AT91_PMECC,
		.end	= AT91_BASE_SYS + AT91_PMECC + SZ_512 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= AT91_BASE_SYS + AT91_PMERRLOC,
		.end	= AT91_BASE_SYS + AT91_PMERRLOC + SZ_512 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= AT91SAM9X5_ROM_BASE,
		.end	= AT91SAM9X5_ROM_BASE + AT91SAM9X5_ROM_SIZE,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device at91sam9x5_nand_device = {
	.name		= "atmel_nand",
	.id		= -1,
	.dev		= {
				.platform_data	= &nand_data,
	},
	.resource	= nand_resources,
	.num_resources	= ARRAY_SIZE(nand_resources),
};

void __init at91_add_device_nand(struct atmel_nand_data *data)
{
	unsigned long csa;

	if (!data)
		return;

	csa = at91_sys_read(AT91_MATRIX_EBICSA);
	csa |= AT91_MATRIX_EBI_CS3A_SMC_NANDFLASH;

	if (!data->bus_on_d0) {
		csa |= AT91_MATRIX_NFD0_ON_D16;
	       if (!data->bus_width_16)
			csa |= AT91_MATRIX_MP_ON;
	} else
		csa &= ~(AT91_MATRIX_NFD0_ON_D16 | AT91_MATRIX_MP_ON);

	at91_sys_write(AT91_MATRIX_EBICSA, csa);

	/* enable pin */
	if (data->enable_pin)
		at91_set_gpio_output(data->enable_pin, 1);

	/* ready/busy pin */
	if (data->rdy_pin)
		at91_set_gpio_input(data->rdy_pin, 1);

	/* card detect pin */
	if (data->det_pin)
		at91_set_gpio_input(data->det_pin, 1);

	/* configure NANDOE */
	at91_set_A_periph(AT91_PIN_PD0, 1);
	/* configure NANDWE */
	at91_set_A_periph(AT91_PIN_PD1, 1);
	/* configure ALE */
	at91_set_A_periph(AT91_PIN_PD2, 1);
	/* configure CLE */
	at91_set_A_periph(AT91_PIN_PD3, 1);

	/* configure multiplexed pins for D16~D31 */
	if (!data->bus_on_d0) {
		at91_set_A_periph(AT91_PIN_PD6, 1);
		at91_set_A_periph(AT91_PIN_PD7, 1);
		at91_set_A_periph(AT91_PIN_PD8, 1);
		at91_set_A_periph(AT91_PIN_PD9, 1);
		at91_set_A_periph(AT91_PIN_PD10, 1);
		at91_set_A_periph(AT91_PIN_PD11, 1);
		at91_set_A_periph(AT91_PIN_PD12, 1);
		at91_set_A_periph(AT91_PIN_PD13, 1);

		if (data->bus_width_16) {
			at91_set_A_periph(AT91_PIN_PD14, 1);
			at91_set_A_periph(AT91_PIN_PD15, 1);
			at91_set_A_periph(AT91_PIN_PD16, 1);
			at91_set_A_periph(AT91_PIN_PD17, 1);
			at91_set_A_periph(AT91_PIN_PD18, 1);
			at91_set_A_periph(AT91_PIN_PD19, 1);
			at91_set_A_periph(AT91_PIN_PD20, 1);
			at91_set_A_periph(AT91_PIN_PD21, 1);
		}

	}

	nand_data = *data;
	platform_device_register(&at91sam9x5_nand_device);
}
#else
void __init at91_add_device_nand(struct atmel_nand_data *data) {}
#endif

/* --------------------------------------------------------------------
 *  TWI (i2c)
 * -------------------------------------------------------------------- */

/*
 * Prefer the GPIO code since the TWI controller isn't robust
 * (gets overruns and underruns under load) and can only issue
 * repeated STARTs in one scenario (the driver doesn't yet handle them).
 */
#if defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C_GPIO_MODULE)
static struct i2c_gpio_platform_data pdata_i2c0 = {
	.sda_pin		= AT91_PIN_PA30,
	.sda_is_open_drain	= 1,
	.scl_pin		= AT91_PIN_PA31,
	.scl_is_open_drain	= 1,
	.udelay			= 2,		/* ~100 kHz */
};

static struct platform_device at91sam9x5_twi0_device = {
	.name			= "i2c-gpio",
	.id			= 0,
	.dev.platform_data	= &pdata_i2c0,
};

void __init at91_add_device_i2c(short i2c_id, struct i2c_board_info *devices, int nr_devices)
{
	i2c_register_board_info(i2c_id, devices, nr_devices);

	if (i2c_id == 0) {
		at91_set_GPIO_periph(AT91_PIN_PA30, 1);		/* TWD (SDA) */
		at91_set_multi_drive(AT91_PIN_PA30, 1);

		at91_set_GPIO_periph(AT91_PIN_PA31, 1);		/* TWCK (SCL) */
		at91_set_multi_drive(AT91_PIN_PA31, 1);

		platform_device_register(&at91sam9x5_twi0_device);
	}
}

#elif defined(CONFIG_I2C_AT91) || defined(CONFIG_I2C_AT91_MODULE)
static struct resource twi0_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_TWI0,
		.end	= AT91SAM9X5_BASE_TWI0 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_TWI0,
		.end	= AT91SAM9X5_ID_TWI0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_twi0_device = {
	.name		= "at91_i2c",
	.id		= 0,
	.resource	= twi0_resources,
	.num_resources	= ARRAY_SIZE(twi0_resources),
};

void __init at91_add_device_i2c(short i2c_id, struct i2c_board_info *devices, int nr_devices)
{
	i2c_register_board_info(i2c_id, devices, nr_devices);

	/* pins used for TWI interface */
	if (i2c_id == 0) {
		at91_set_A_periph(AT91_PIN_PA30, 0);		/* TWD */
		at91_set_multi_drive(AT91_PIN_PA30, 1);

		at91_set_A_periph(AT91_PIN_PA31, 0);		/* TWCK */
		at91_set_multi_drive(AT91_PIN_PA31, 1);

		platform_device_register(&at91sam9x5_twi0_device);
	}
}
#else
void __init at91_add_device_i2c(short i2c_id, struct i2c_board_info *devices, int nr_devices) {}
#endif

/* --------------------------------------------------------------------
 *  SPI
 * -------------------------------------------------------------------- */

#if defined(CONFIG_SPI_ATMEL) || defined(CONFIG_SPI_ATMEL_MODULE)
static u64 spi_dmamask = DMA_BIT_MASK(32);
static struct at_dma_slave spi0_sdata, spi1_sdata;

static struct resource spi0_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_SPI0,
		.end	= AT91SAM9X5_BASE_SPI0 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_SPI0,
		.end	= AT91SAM9X5_ID_SPI0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_spi0_device = {
	.name		= "atmel_spi",
	.id		= 0,
	.dev		= {
				.dma_mask		= &spi_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &spi0_sdata,
	},
	.resource	= spi0_resources,
	.num_resources	= ARRAY_SIZE(spi0_resources),
};

static const unsigned spi0_standard_cs[4] = { AT91_PIN_PA14, AT91_PIN_PA7, AT91_PIN_PA1, AT91_PIN_PB3 };

static struct resource spi1_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_SPI1,
		.end	= AT91SAM9X5_BASE_SPI1 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_SPI1,
		.end	= AT91SAM9X5_ID_SPI1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_spi1_device = {
	.name		= "atmel_spi",
	.id		= 1,
	.dev		= {
				.dma_mask		= &spi_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &spi1_sdata,
	},
	.resource	= spi1_resources,
	.num_resources	= ARRAY_SIZE(spi1_resources),
};

static const unsigned spi1_standard_cs[4] = { AT91_PIN_PA8, AT91_PIN_PA0, AT91_PIN_PA31, AT91_PIN_PA30 };

void __init at91_add_device_spi(struct spi_board_info *devices, int nr_devices)
{
	int i;
	unsigned long cs_pin;
	short enable_spi0 = 0;
	short enable_spi1 = 0;
#if defined(CONFIG_AT_HDMAC) || defined(CONFIG_AT_HDMAC_MODULE)
	struct at_dma_slave *atslave;
#endif

	/* Choose SPI chip-selects */
	for (i = 0; i < nr_devices; i++) {
		if (devices[i].controller_data)
			cs_pin = (unsigned long) devices[i].controller_data;
		else if (devices[i].bus_num == 0)
			cs_pin = spi0_standard_cs[devices[i].chip_select];
		else
			cs_pin = spi1_standard_cs[devices[i].chip_select];

		if (devices[i].bus_num == 0)
			enable_spi0 = 1;
		else
			enable_spi1 = 1;

		/* enable chip-select pin */
		at91_set_gpio_output(cs_pin, 1);

		/* pass chip-select pin to driver */
		devices[i].controller_data = (void *) cs_pin;
	}

	spi_register_board_info(devices, nr_devices);


	/* Configure SPI bus(es) */
	if (enable_spi0) {
		at91_set_A_periph(AT91_PIN_PA11, 0);	/* SPI0_MISO */
		at91_set_A_periph(AT91_PIN_PA12, 0);	/* SPI0_MOSI */
		at91_set_A_periph(AT91_PIN_PA13, 0);	/* SPI0_SPCK */

#if defined(CONFIG_AT_HDMAC) || defined(CONFIG_AT_HDMAC_MODULE)
		atslave = at91sam9x5_spi0_device.dev.platform_data;

		/* DMA slave channel configuration */
		atslave->dma_dev = &at_hdmac0_device.dev;
		atslave->reg_width = AT_DMA_SLAVE_WIDTH_8BIT; /* or 16bits??????? */
		atslave->cfg = ATC_FIFOCFG_HALFFIFO
				| ATC_SRC_H2SEL_HW | ATC_DST_H2SEL_HW
				| ATC_SRC_PER(AT_DMA_ID_SPI0_RX)
				| ATC_DST_PER(AT_DMA_ID_SPI0_TX);
		/*atslave->ctrla = ATC_SCSIZE_16 | ATC_DCSIZE_16;*/ /* Chunk size to 0????? */
#endif

		at91_clock_associate("spi0_clk", &at91sam9x5_spi0_device.dev, "spi_clk");
		platform_device_register(&at91sam9x5_spi0_device);
	}
	if (enable_spi1) {
		at91_set_B_periph(AT91_PIN_PA21, 0);	/* SPI1_MISO */
		at91_set_B_periph(AT91_PIN_PA22, 0);	/* SPI1_MOSI */
		at91_set_B_periph(AT91_PIN_PA23, 0);	/* SPI1_SPCK */

#if defined(CONFIG_AT_HDMAC) || defined(CONFIG_AT_HDMAC_MODULE)
		atslave = at91sam9x5_spi1_device.dev.platform_data;

		/* DMA slave channel configuration */
		atslave->dma_dev = &at_hdmac1_device.dev;
		atslave->reg_width = AT_DMA_SLAVE_WIDTH_8BIT; /* or 16bits??????? */
		atslave->cfg = ATC_FIFOCFG_HALFFIFO
				| ATC_SRC_H2SEL_HW | ATC_DST_H2SEL_HW
				| ATC_SRC_PER(AT_DMA_ID_SPI1_RX)
				| ATC_DST_PER(AT_DMA_ID_SPI1_TX);
		/*atslave->ctrla = ATC_SCSIZE_16 | ATC_DCSIZE_16;*/ /* Chunk size to 0????? */
#endif

		at91_clock_associate("spi1_clk", &at91sam9x5_spi1_device.dev, "spi_clk");
		platform_device_register(&at91sam9x5_spi1_device);
	}
}
#else
void __init at91_add_device_spi(struct spi_board_info *devices, int nr_devices) {}
#endif


/* --------------------------------------------------------------------
 * CAN Controllers
 * -------------------------------------------------------------------- */

#if defined(CONFIG_CAN_AT91) || defined(CONFIG_CAN_AT91_MODULE)
static struct resource can_resources[][2] = {
	{
		{
			.start	= AT91SAM9X5_BASE_CAN0,
			.end	= AT91SAM9X5_BASE_CAN0 + SZ_16K - 1,
			.flags	= IORESOURCE_MEM,
		}, {
			.start	= AT91SAM9X5_ID_CAN0,
			.end	= AT91SAM9X5_ID_CAN0,
			.flags	= IORESOURCE_IRQ,
		},
	}, {
		{
			.start	= AT91SAM9X5_BASE_CAN1,
			.end	= AT91SAM9X5_BASE_CAN1 + SZ_16K - 1,
			.flags	= IORESOURCE_MEM,
		}, {
			.start	= AT91SAM9X5_ID_CAN1,
			.end	= AT91SAM9X5_ID_CAN1,
			.flags	= IORESOURCE_IRQ,
		},
	},
};

static struct platform_device at91sam9x5_can_device[] = {
	{
		.name = "at91sam9x5_can",
		.id = 0,
		.resource = can_resources[0],
		.num_resources = ARRAY_SIZE(can_resources[0]),
	}, {
		.name = "at91sam9x5_can",
		.id = 1,
		.resource = can_resources[1],
		.num_resources = ARRAY_SIZE(can_resources[1]),
	},
};

static const struct {
	unsigned txpin;
	unsigned rxpin;
} at91sam9x5_can_pins[] __initconst = {
	{
		.txpin = AT91_PIN_PA10,
		.rxpin = AT91_PIN_PA9,
	}, {
		.txpin = AT91_PIN_PA5,
		.rxpin = AT91_PIN_PA6,
	},
};

void __init at91_add_device_can(int id, struct at91_can_data *data)
{
	at91_clock_associate("can0_clk", &at91sam9x5_can_device[0].dev, "can_clk");
	at91_clock_associate("can1_clk", &at91sam9x5_can_device[1].dev, "can_clk");
	at91_set_B_periph(at91sam9x5_can_pins[id].txpin, 0);
	at91_set_B_periph(at91sam9x5_can_pins[id].rxpin, 0);
	at91sam9x5_can_device[id].dev.platform_data = data;

	platform_device_register(&at91sam9x5_can_device[id]);
}
#else
void __init at91_add_device_can(int id, struct at91_can_data *data) {}
#endif

/* --------------------------------------------------------------------
 *  LCD Controller
 * -------------------------------------------------------------------- */

#if defined(CONFIG_FB_ATMEL) || defined(CONFIG_FB_ATMEL_MODULE)
static u64 lcdc_dmamask = DMA_BIT_MASK(32);
static struct atmel_lcdfb_info lcdc_data;

static struct resource lcdc_base_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_LCDC,
		.end	= AT91SAM9X5_BASE_LCDC + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_BASE_LCDC + ATMEL_LCDC_BASECLUT,
		.end	= AT91SAM9X5_BASE_LCDC + ATMEL_LCDC_BASECLUT + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= AT91SAM9X5_ID_LCDC,
		.end	= AT91SAM9X5_ID_LCDC,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91_lcdc_base_device = {
	.name		= "atmel_hlcdfb_base",
	.id		= 0,
	.dev		= {
				.dma_mask		= &lcdc_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &lcdc_data,
	},
	.resource	= lcdc_base_resources,
	.num_resources	= ARRAY_SIZE(lcdc_base_resources),
};

static struct resource lcdc_ovl1_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_LCDC + 0x100,
		.end	= AT91SAM9X5_BASE_LCDC + 0x27f,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_BASE_LCDC + ATMEL_LCDC_OVR1CLUT,
		.end	= AT91SAM9X5_BASE_LCDC + ATMEL_LCDC_OVR1CLUT + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device at91_lcdc_ovl_device = {
	.name		= "atmel_hlcdfb_ovl",
	.id		= 0,
	.dev		= {
				.dma_mask		= &lcdc_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &lcdc_data,
	},
	.resource	= lcdc_ovl1_resources,
	.num_resources	= ARRAY_SIZE(lcdc_ovl1_resources),
};

void __init at91_add_device_lcdc(struct atmel_lcdfb_info *data)
{
	if (!data)
		return;

	at91_set_A_periph(AT91_PIN_PC26, 0);	/* LCDPWM */

	at91_set_A_periph(AT91_PIN_PC27, 0);	/* LCDVSYNC */
	at91_set_A_periph(AT91_PIN_PC28, 0);	/* LCDHSYNC */

	at91_set_A_periph(AT91_PIN_PC24, 0);	/* LCDDISP */
	at91_set_A_periph(AT91_PIN_PC29, 0);	/* LCDDEN */
	at91_set_A_periph(AT91_PIN_PC30, 0);	/* LCDPCK */

	at91_set_A_periph(AT91_PIN_PC0, 0);	/* LCDD0 */
	at91_set_A_periph(AT91_PIN_PC1, 0);	/* LCDD1 */
	at91_set_A_periph(AT91_PIN_PC2, 0);	/* LCDD2 */
	at91_set_A_periph(AT91_PIN_PC3, 0);	/* LCDD3 */
	at91_set_A_periph(AT91_PIN_PC4, 0);	/* LCDD4 */
	at91_set_A_periph(AT91_PIN_PC5, 0);	/* LCDD5 */
	at91_set_A_periph(AT91_PIN_PC6, 0);	/* LCDD6 */
	at91_set_A_periph(AT91_PIN_PC7, 0);	/* LCDD7 */
	at91_set_A_periph(AT91_PIN_PC8, 0);	/* LCDD8 */
	at91_set_A_periph(AT91_PIN_PC9, 0);	/* LCDD9 */
	at91_set_A_periph(AT91_PIN_PC10, 0);	/* LCDD10 */
	at91_set_A_periph(AT91_PIN_PC11, 0);	/* LCDD11 */
	at91_set_A_periph(AT91_PIN_PC12, 0);	/* LCDD12 */
	at91_set_A_periph(AT91_PIN_PC13, 0);	/* LCDD13 */
	at91_set_A_periph(AT91_PIN_PC14, 0);	/* LCDD14 */
	at91_set_A_periph(AT91_PIN_PC15, 0);	/* LCDD15 */
	at91_set_A_periph(AT91_PIN_PC16, 0);	/* LCDD16 */
	at91_set_A_periph(AT91_PIN_PC17, 0);	/* LCDD17 */
	at91_set_A_periph(AT91_PIN_PC18, 0);	/* LCDD18 */
	at91_set_A_periph(AT91_PIN_PC19, 0);	/* LCDD19 */
	at91_set_A_periph(AT91_PIN_PC20, 0);	/* LCDD20 */
	at91_set_A_periph(AT91_PIN_PC21, 0);	/* LCDD21 */
	at91_set_A_periph(AT91_PIN_PC22, 0);	/* LCDD22 */
	at91_set_A_periph(AT91_PIN_PC23, 0);	/* LCDD23 */

	lcdc_data = *data;
	platform_device_register(&at91_lcdc_base_device);
	platform_device_register(&at91_lcdc_ovl_device);
}
#else
void __init at91_add_device_lcdc(struct atmel_lcdfb_info *data) {}
#endif

/* --------------------------------------------------------------------
 *  Timer/Counter block
 * -------------------------------------------------------------------- */

#ifdef CONFIG_ATMEL_TCLIB
static struct resource tcb0_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_TCB0,
		.end	= AT91SAM9X5_BASE_TCB0 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_TCB,
		.end	= AT91SAM9X5_ID_TCB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_tcb0_device = {
	.name		= "atmel_tcb",
	.id		= 0,
	.resource	= tcb0_resources,
	.num_resources	= ARRAY_SIZE(tcb0_resources),
};

/* TCB1 begins with TC3 */
static struct resource tcb1_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_TCB1,
		.end	= AT91SAM9X5_BASE_TCB1 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_TCB,
		.end	= AT91SAM9X5_ID_TCB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_tcb1_device = {
	.name		= "atmel_tcb",
	.id		= 1,
	.resource	= tcb1_resources,
	.num_resources	= ARRAY_SIZE(tcb1_resources),
};

static void __init at91_add_device_tc(void)
{
	/* this chip has one clock and irq for all six TC channels */
	at91_clock_associate("tcb0_clk", &at91sam9x5_tcb0_device.dev, "t0_clk");
	platform_device_register(&at91sam9x5_tcb0_device);
	at91_clock_associate("tcb1_clk", &at91sam9x5_tcb1_device.dev, "t0_clk");
	platform_device_register(&at91sam9x5_tcb1_device);
}
#else
static void __init at91_add_device_tc(void) { }
#endif

/* --------------------------------------------------------------------
 *  RTC
 * -------------------------------------------------------------------- */

#if defined(CONFIG_RTC_DRV_AT91RM9200) || defined(CONFIG_RTC_DRV_AT91RM9200_MODULE)
static struct platform_device at91sam9x5_rtc_device = {
	.name		= "at91_rtc",
	.id		= -1,
	.num_resources	= 0,
};

static void __init at91_add_device_rtc(void)
{
	platform_device_register(&at91sam9x5_rtc_device);
}
#else
static void __init at91_add_device_rtc(void) {}
#endif

/* --------------------------------------------------------------------
 *  Touchscreen
 * -------------------------------------------------------------------- */

#if defined(CONFIG_TOUCHSCREEN_ATMEL_TSADCC) || defined(CONFIG_TOUCHSCREEN_ATMEL_TSADCC_MODULE)
static u64 tsadcc_dmamask = DMA_BIT_MASK(32);
static struct at91_tsadcc_data tsadcc_data;

static struct resource tsadcc_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_ADC,
		.end	= AT91SAM9X5_BASE_ADC + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_ADC,
		.end	= AT91SAM9X5_ID_ADC,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device at91sam9x5_tsadcc_device = {
	.name		= "atmel_tsadcc",
	.id		= -1,
	.dev		= {
				.dma_mask		= &tsadcc_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &tsadcc_data,
	},
	.resource	= tsadcc_resources,
	.num_resources	= ARRAY_SIZE(tsadcc_resources),
};

void __init at91_add_device_tsadcc(struct at91_tsadcc_data *data)
{
	if (!data)
		return;

	/* In 9x5ek, using default pins for touch screen. */

	tsadcc_data = *data;
	at91_clock_associate("adc_clk", &at91sam9x5_tsadcc_device.dev, "tsc_clk");
	platform_device_register(&at91sam9x5_tsadcc_device);
}
#else
void __init at91_add_device_tsadcc(struct at91_tsadcc_data *data) {}
#endif

/* --------------------------------------------------------------------
 *  Watchdog
 * -------------------------------------------------------------------- */

#if defined(CONFIG_AT91SAM9X_WATCHDOG) || defined(CONFIG_AT91SAM9X_WATCHDOG_MODULE)
static struct platform_device at91sam9x5_wdt_device = {
	.name		= "at91_wdt",
	.id		= -1,
	.num_resources	= 0,
};

static void __init at91_add_device_watchdog(void)
{
	platform_device_register(&at91sam9x5_wdt_device);
}
#else
static void __init at91_add_device_watchdog(void) {}
#endif


/* --------------------------------------------------------------------
 *  PWM
 * --------------------------------------------------------------------*/

#if defined(CONFIG_ATMEL_PWM) || defined(CONFIG_ATMEL_PWM_MODULE)
static u32 pwm_mask;

static struct resource pwm_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_PWMC,
		.end	= AT91SAM9X5_BASE_PWMC + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_PWMC,
		.end	= AT91SAM9X5_ID_PWMC,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_pwm_device = {
	.name	= "atmel_pwm",
	.id	= -1,
	.dev	= {
		.platform_data		= &pwm_mask,
	},
	.resource	= pwm_resources,
	.num_resources	= ARRAY_SIZE(pwm_resources),
};

void __init at91_add_device_pwm(u32 mask)
{
	if (mask & (1 << AT91_PWM0))
		at91_set_B_periph(AT91_PIN_PB11, 1);	/* enable PWM0 */

	if (mask & (1 << AT91_PWM1))
		at91_set_B_periph(AT91_PIN_PB12, 1);	/* enable PWM1 */

	if (mask & (1 << AT91_PWM2))
		at91_set_B_periph(AT91_PIN_PB13, 1);	/* enable PWM2 */

	if (mask & (1 << AT91_PWM3))
		at91_set_B_periph(AT91_PIN_PB14, 1);	/* enable PWM3 */

	pwm_mask = mask;

	platform_device_register(&at91sam9x5_pwm_device);
}
#else
void __init at91_add_device_pwm(u32 mask) {}
#endif


/* --------------------------------------------------------------------
 *  SSC -- Synchronous Serial Controller
 * -------------------------------------------------------------------- */

#if defined(CONFIG_ATMEL_SSC) || defined(CONFIG_ATMEL_SSC_MODULE)
static u64 ssc_dmamask = DMA_BIT_MASK(32);
static struct at_dma_slave ssc_sdata;

static struct resource ssc_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_SSC,
		.end	= AT91SAM9X5_BASE_SSC + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_SSC,
		.end	= AT91SAM9X5_ID_SSC,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9x5_ssc_device = {
	.name	= "ssc",
	.id	= 0,
	.dev	= {
		.dma_mask		= &ssc_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &ssc_sdata,
	},
	.resource	= ssc_resources,
	.num_resources	= ARRAY_SIZE(ssc_resources),
};

static inline void configure_ssc_pins(unsigned pins)
{
	if (pins & ATMEL_SSC_TF)
		at91_set_B_periph(AT91_PIN_PA25, 1);
	if (pins & ATMEL_SSC_TK)
		at91_set_B_periph(AT91_PIN_PA24, 1);
	if (pins & ATMEL_SSC_TD)
		at91_set_B_periph(AT91_PIN_PA26, 1);
	if (pins & ATMEL_SSC_RD)
		at91_set_B_periph(AT91_PIN_PA27, 1);
	if (pins & ATMEL_SSC_RK)
		at91_set_B_periph(AT91_PIN_PA28, 1);
	if (pins & ATMEL_SSC_RF)
		at91_set_B_periph(AT91_PIN_PA29, 1);
}

/*
 * SSC controllers are accessed through library code, instead of any
 * kind of all-singing/all-dancing driver.  For example one could be
 * used by a particular I2S audio codec's driver, while another one
 * on the same system might be used by a custom data capture driver.
 */
void __init at91_add_device_ssc(unsigned id, unsigned pins)
{
	struct platform_device *pdev;

	/*
	 * NOTE: caller is responsible for passing information matching
	 * "pins" to whatever will be using each particular controller.
	 */
	if (id == AT91SAM9X5_ID_SSC) {
#if defined(CONFIG_AT_HDMAC) || defined(CONFIG_AT_HDMAC_MODULE)
		struct at_dma_slave *atslave;

		atslave = at91sam9x5_ssc_device.dev.platform_data;

		/* DMA slave channel configuration */
		atslave->dma_dev = &at_hdmac0_device.dev;
		atslave->reg_width = AT_DMA_SLAVE_WIDTH_16BIT;
		atslave->cfg = ATC_FIFOCFG_HALFFIFO
				| ATC_SRC_H2SEL_HW | ATC_DST_H2SEL_HW
				| ATC_SRC_PER(AT_DMA_ID_SSC_RX)
				| ATC_DST_PER(AT_DMA_ID_SSC_TX);
#endif

		pdev = &at91sam9x5_ssc_device;
		configure_ssc_pins(pins);
		at91_clock_associate("ssc_clk", &pdev->dev, "pclk");
	}
	else
		return;

	platform_device_register(pdev);
}

#else
void __init at91_add_device_ssc(unsigned id, unsigned pins) {}
#endif


/* --------------------------------------------------------------------
 *  UART
 * -------------------------------------------------------------------- */

#if defined(CONFIG_SERIAL_ATMEL)
static struct resource dbgu_resources[] = {
	[0] = {
		.start	= AT91_VA_BASE_SYS + AT91_DBGU,
		.end	= AT91_VA_BASE_SYS + AT91_DBGU + SZ_512 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91_ID_SYS,
		.end	= AT91_ID_SYS,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct atmel_uart_data dbgu_data = {
	.use_dma_tx	= 0,
	.use_dma_rx	= 0,
	.regs		= (void __iomem *)(AT91_VA_BASE_SYS + AT91_DBGU),
};

static u64 dbgu_dmamask = DMA_BIT_MASK(32);

static struct platform_device at91sam9x5_dbgu_device = {
	.name		= "atmel_usart",
	.id		= 0,
	.dev		= {
				.dma_mask		= &dbgu_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &dbgu_data,
	},
	.resource	= dbgu_resources,
	.num_resources	= ARRAY_SIZE(dbgu_resources),
};

static inline void configure_dbgu_pins(void)
{
	at91_set_A_periph(AT91_PIN_PA9, 0);		/* DRXD */
	at91_set_A_periph(AT91_PIN_PA10, 1);		/* DTXD */
}

static struct resource usart0_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_USART0,
		.end	= AT91SAM9X5_BASE_USART0 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_USART0,
		.end	= AT91SAM9X5_ID_USART0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct atmel_uart_data usart0_data = {
	.use_dma_tx	= 1,
	.use_dma_rx	= 0,				/* doesn't support */
};

static u64 usart0_dmamask = DMA_BIT_MASK(32);

static struct platform_device at91sam9x5_usart0_device = {
	.name		= "atmel_usart",
	.id		= 1,
	.dev		= {
				.dma_mask		= &usart0_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &usart0_data,
	},
	.resource	= usart0_resources,
	.num_resources	= ARRAY_SIZE(usart0_resources),
};

static inline void configure_usart0_pins(unsigned pins)
{
	at91_set_A_periph(AT91_PIN_PA0, 1);		/* TXD0 */
	at91_set_A_periph(AT91_PIN_PA1, 0);		/* RXD0 */

	if (pins & ATMEL_UART_RTS)
		at91_set_A_periph(AT91_PIN_PA2, 0);	/* RTS0 */
	if (pins & ATMEL_UART_CTS)
		at91_set_A_periph(AT91_PIN_PA3, 0);	/* CTS0 */
}


static struct resource usart1_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_USART1,
		.end	= AT91SAM9X5_BASE_USART1 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_USART1,
		.end	= AT91SAM9X5_ID_USART1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct atmel_uart_data usart1_data = {
	.use_dma_tx	= 1,
	.use_dma_rx	= 1,
};

static u64 usart1_dmamask = DMA_BIT_MASK(32);

static struct platform_device at91sam9x5_usart1_device = {
	.name		= "atmel_usart",
	.id		= 2,
	.dev		= {
				.dma_mask		= &usart1_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &usart1_data,
	},
	.resource	= usart1_resources,
	.num_resources	= ARRAY_SIZE(usart1_resources),
};

static inline void configure_usart1_pins(unsigned pins)
{
	at91_set_A_periph(AT91_PIN_PA5, 1);		/* TXD1 */
	at91_set_A_periph(AT91_PIN_PA6, 0);		/* RXD1 */

	if (pins & ATMEL_UART_RTS)
		at91_set_C_periph(AT91_PIN_PC27, 0);	/* RTS1 */
	if (pins & ATMEL_UART_CTS)
		at91_set_C_periph(AT91_PIN_PC28, 0);	/* CTS1 */
}

static struct resource usart2_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_USART2,
		.end	= AT91SAM9X5_BASE_USART2 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_USART2,
		.end	= AT91SAM9X5_ID_USART2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct atmel_uart_data usart2_data = {
	.use_dma_tx	= 1,
	.use_dma_rx	= 1,
};

static u64 usart2_dmamask = DMA_BIT_MASK(32);

static struct platform_device at91sam9x5_usart2_device = {
	.name		= "atmel_usart",
	.id		= 3,
	.dev		= {
				.dma_mask		= &usart2_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &usart2_data,
	},
	.resource	= usart2_resources,
	.num_resources	= ARRAY_SIZE(usart2_resources),
};

static inline void configure_usart2_pins(unsigned pins)
{
	at91_set_A_periph(AT91_PIN_PA7, 1);		/* TXD2 */
	at91_set_A_periph(AT91_PIN_PA8, 0);		/* RXD2 */

	if (pins & ATMEL_UART_RTS)
		at91_set_B_periph(AT91_PIN_PB0, 0);	/* RTS2 */
	if (pins & ATMEL_UART_CTS)
		at91_set_B_periph(AT91_PIN_PB1, 0);	/* CTS2 */
}

static struct resource usart3_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_USART3,
		.end	= AT91SAM9X5_BASE_USART3 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_USART3,
		.end	= AT91SAM9X5_ID_USART3,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct atmel_uart_data usart3_data = {
	.use_dma_tx	= 1,
	.use_dma_rx	= 1,
};

static u64 usart3_dmamask = DMA_BIT_MASK(32);

static struct platform_device at91sam9x5_usart3_device = {
	.name		= "atmel_usart",
	.id		= 4,
	.dev		= {
				.dma_mask		= &usart3_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &usart3_data,
	},
	.resource	= usart3_resources,
	.num_resources	= ARRAY_SIZE(usart3_resources),
};

static inline void configure_usart3_pins(unsigned pins)
{
	at91_set_B_periph(AT91_PIN_PC22, 1);		/* TXD3 */
	at91_set_B_periph(AT91_PIN_PC23, 0);		/* RXD3 */

	if (pins & ATMEL_UART_RTS)
		at91_set_B_periph(AT91_PIN_PC24, 0);	/* RTS3 */
	if (pins & ATMEL_UART_CTS)
		at91_set_B_periph(AT91_PIN_PC25, 0);	/* CTS3 */
}

static struct resource uart0_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_UART0,
		.end	= AT91SAM9X5_BASE_UART0 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_UART0,
		.end	= AT91SAM9X5_ID_UART0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct atmel_uart_data uart0_data = {
	.use_dma_tx	= 1,
	.use_dma_rx	= 1,
};

static u64 uart0_dmamask = DMA_BIT_MASK(32);

static struct platform_device at91sam9x5_uart0_device = {
	.name		= "atmel_usart",
	.id		= 5,
	.dev		= {
				.dma_mask		= &uart0_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &uart0_data,
	},
	.resource	= uart0_resources,
	.num_resources	= ARRAY_SIZE(uart0_resources),
};

static inline void configure_uart0_pins(unsigned pins)
{
	at91_set_C_periph(AT91_PIN_PC8, 1);		/* UTXD0 */
	at91_set_C_periph(AT91_PIN_PC9, 0);		/* URXD0 */
}

static struct resource uart1_resources[] = {
	[0] = {
		.start	= AT91SAM9X5_BASE_UART1,
		.end	= AT91SAM9X5_BASE_UART1 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9X5_ID_UART1,
		.end	= AT91SAM9X5_ID_UART1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct atmel_uart_data uart1_data = {
	.use_dma_tx	= 1,
	.use_dma_rx	= 1,
};

static u64 uart1_dmamask = DMA_BIT_MASK(32);

static struct platform_device at91sam9x5_uart1_device = {
	.name		= "atmel_usart",
	.id		= 6,
	.dev		= {
				.dma_mask		= &uart1_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &uart1_data,
	},
	.resource	= uart1_resources,
	.num_resources	= ARRAY_SIZE(uart1_resources),
};

static inline void configure_uart1_pins(unsigned pins)
{
	at91_set_C_periph(AT91_PIN_PC16, 1);		/* UTXD1 */
	at91_set_C_periph(AT91_PIN_PC17, 0);		/* URXD1 */
}

static struct platform_device *__initdata at91_usarts[ATMEL_MAX_UART];	/* the USARTs to use */
struct platform_device *atmel_default_console_device;	/* the serial console device */

void __init at91_register_uart(unsigned id, unsigned portnr, unsigned pins)
{
	struct platform_device *pdev;

	switch (id) {
		case 0:		/* DBGU */
			pdev = &at91sam9x5_dbgu_device;
			configure_dbgu_pins();
			at91_clock_associate("mck", &pdev->dev, "usart");
			break;
		case AT91SAM9X5_ID_USART0:
			pdev = &at91sam9x5_usart0_device;
			configure_usart0_pins(pins);
			at91_clock_associate("usart0_clk", &pdev->dev, "usart");
			break;
		case AT91SAM9X5_ID_USART1:
			pdev = &at91sam9x5_usart1_device;
			configure_usart1_pins(pins);
			at91_clock_associate("usart1_clk", &pdev->dev, "usart");
			break;
		case AT91SAM9X5_ID_USART2:
			pdev = &at91sam9x5_usart2_device;
			configure_usart2_pins(pins);
			at91_clock_associate("usart2_clk", &pdev->dev, "usart");
			break;
		case AT91SAM9X5_ID_USART3:
			pdev = &at91sam9x5_usart3_device;
			configure_usart3_pins(pins);
			at91_clock_associate("usart3_clk", &pdev->dev, "usart");
			break;
		case AT91SAM9X5_ID_UART0:
			pdev = &at91sam9x5_uart0_device;
			configure_uart0_pins(pins);
			at91_clock_associate("uart0_clk", &pdev->dev, "usart");
			break;
		case AT91SAM9X5_ID_UART1:
			pdev = &at91sam9x5_uart1_device;
			configure_uart1_pins(pins);
			at91_clock_associate("uart1_clk", &pdev->dev, "usart");
			break;
		default:
			return;
	}
	pdev->id = portnr;		/* update to mapped ID */

	if (portnr < ATMEL_MAX_UART)
		at91_usarts[portnr] = pdev;
}

void __init at91_set_serial_console(unsigned portnr)
{
	if (portnr < ATMEL_MAX_UART)
		atmel_default_console_device = at91_usarts[portnr];
}

void __init at91_add_device_serial(void)
{
	int i;

	for (i = 0; i < ATMEL_MAX_UART; i++) {
		if (at91_usarts[i]) {
#if defined(CONFIG_AT_HDMAC) || defined(CONFIG_AT_HDMAC_MODULE)
			int peripheral_id		= platform_get_irq(at91_usarts[i], 0);
			struct atmel_uart_data *pdata	= at91_usarts[i]->dev.platform_data;

			if (pdata->use_dma_tx) {
				struct at_dma_slave	*atslave;

				atslave = kzalloc(sizeof(struct at_dma_slave), GFP_KERNEL);

				/* DMA slave channel configuration */
				if (peripheral_id == AT91SAM9X5_ID_USART0
				    || peripheral_id == AT91SAM9X5_ID_USART1
				    || peripheral_id == AT91SAM9X5_ID_UART0)
					atslave->dma_dev = &at_hdmac0_device.dev;
				else
					atslave->dma_dev = &at_hdmac1_device.dev;

				atslave->reg_width = DW_DMA_SLAVE_WIDTH_8BIT;
				atslave->cfg = ATC_FIFOCFG_HALFFIFO
						| ATC_SRC_H2SEL_SW | ATC_DST_H2SEL_HW
						| (AT_DMA_ID_USART0_TX << 4); /*ATC_DST_PER(peripheral_id);*/

				pdata->dma_tx_slave = atslave;
			}
#endif
			platform_device_register(at91_usarts[i]);
		}
	}

	if (!atmel_default_console_device)
		printk(KERN_INFO "AT91: No default serial console defined.\n");
}
#else
void __init at91_register_uart(unsigned id, unsigned portnr, unsigned pins) {}
void __init at91_set_serial_console(unsigned portnr) {}
void __init at91_add_device_serial(void) {}
#endif


/* -------------------------------------------------------------------- */
/*
 * These devices are always present and don't need any board-specific
 * setup.
 */
static int __init at91_add_standard_devices(void)
{
	at91_add_device_hdmac();
	at91_add_device_rtc();
	at91_add_device_watchdog();
	at91_add_device_tc();
	return 0;
}

arch_initcall(at91_add_standard_devices);

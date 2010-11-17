/*
 *  Atmel Touch Screen Driver
 *
 *  Copyright (c) 2008 ATMEL
 *  Copyright (c) 2008 Dan Liang
 *  Copyright (c) 2008 TimeSys Corporation
 *  Copyright (c) 2008 Justin Waters
 *
 *  Based on touchscreen code from Atmel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/cpu.h>

#include "atmel_tsadcc.h"

#define cpu_has_9x5_adc() (cpu_is_at91sam9x5())

#define ADC_DEFAULT_CLOCK	100000

struct atmel_tsadcc {
	struct input_dev	*input;
	char			phys[32];
	struct clk		*clk;
	int			irq;
	unsigned int		prev_absx;
	unsigned int		prev_absy;
	unsigned char		bufferedmeasure;
};

static void __iomem		*tsc_base;

#define atmel_tsadcc_read(reg)		__raw_readl(tsc_base + (reg))
#define atmel_tsadcc_write(reg, val)	__raw_writel((val), tsc_base + (reg))

static irqreturn_t atmel_tsadcc_interrupt(int irq, void *dev)
{
	struct atmel_tsadcc	*ts_dev = (struct atmel_tsadcc *)dev;
	struct input_dev	*input_dev = ts_dev->input;

	unsigned int status;
	unsigned int reg;

	status = atmel_tsadcc_read(ATMEL_TSADCC_SR);
	status &= atmel_tsadcc_read(ATMEL_TSADCC_IMR);

	if (status & ATMEL_TSADCC_NOCNT) {
		/* Contact lost */
		if (cpu_has_9x5_adc()) {
			/* 9X5 using TSMR to set PENDBC time */
			reg = atmel_tsadcc_read(ATMEL_TSADCC_TSMR) | ATMEL_TSADCC_PENDBC;
			atmel_tsadcc_write(ATMEL_TSADCC_TSMR, reg);
		} else {
			reg = atmel_tsadcc_read(ATMEL_TSADCC_MR) | ATMEL_TSADCC_PENDBC;
			atmel_tsadcc_write(ATMEL_TSADCC_MR, reg);
		}
		atmel_tsadcc_write(ATMEL_TSADCC_TRGR, ATMEL_TSADCC_TRGMOD_NONE);
		atmel_tsadcc_write(ATMEL_TSADCC_IDR,
				   ATMEL_TSADCC_CONVERSION_END | ATMEL_TSADCC_NOCNT);
		atmel_tsadcc_write(ATMEL_TSADCC_IER, ATMEL_TSADCC_PENCNT);

		input_report_key(input_dev, BTN_TOUCH, 0);
		ts_dev->bufferedmeasure = 0;
		input_sync(input_dev);

	} else if (status & ATMEL_TSADCC_PENCNT) {
		/* Pen detected */
		if (cpu_has_9x5_adc()) {
			reg = atmel_tsadcc_read(ATMEL_TSADCC_TSMR);
			reg &= ~ATMEL_TSADCC_PENDBC;
			atmel_tsadcc_write(ATMEL_TSADCC_TSMR, reg);
		} else {
			reg = atmel_tsadcc_read(ATMEL_TSADCC_MR);
			reg &= ~ATMEL_TSADCC_PENDBC;
			atmel_tsadcc_write(ATMEL_TSADCC_MR, reg);
		}

		atmel_tsadcc_write(ATMEL_TSADCC_IDR, ATMEL_TSADCC_PENCNT);
		atmel_tsadcc_write(ATMEL_TSADCC_IER,
				   ATMEL_TSADCC_CONVERSION_END | ATMEL_TSADCC_NOCNT);
		atmel_tsadcc_write(ATMEL_TSADCC_TRGR,
				   ATMEL_TSADCC_TRGMOD_PERIOD | (0x0FFF << 16));

	} else if ((status & ATMEL_TSADCC_CONVERSION_END) == ATMEL_TSADCC_CONVERSION_END) {
		/* Conversion finished */

		if (ts_dev->bufferedmeasure) {
			/* Last measurement is always discarded, since it can
			 * be erroneous.
			 * Always report previous measurement */
			dev_dbg(&input_dev->dev, "x = %d, y = %d\n",
					ts_dev->prev_absx, ts_dev->prev_absy);
			input_report_abs(input_dev, ABS_X, ts_dev->prev_absx);
			input_report_abs(input_dev, ABS_Y, ts_dev->prev_absy);
			input_report_key(input_dev, BTN_TOUCH, 1);
			input_sync(input_dev);
		} else
			ts_dev->bufferedmeasure = 1;

		/* Now make new measurement */
		if (cpu_has_9x5_adc()) {
			ts_dev->prev_absx = atmel_tsadcc_read(ATMEL_TSADCC_XPOSR) & 0xffff;
			ts_dev->prev_absy = atmel_tsadcc_read(ATMEL_TSADCC_YPOSR) & 0xffff;
		} else {
			ts_dev->prev_absx = atmel_tsadcc_read(ATMEL_TSADCC_CDR3) << 10;
			ts_dev->prev_absx /= atmel_tsadcc_read(ATMEL_TSADCC_CDR2);

			ts_dev->prev_absy = atmel_tsadcc_read(ATMEL_TSADCC_CDR1) << 10;
			ts_dev->prev_absy /= atmel_tsadcc_read(ATMEL_TSADCC_CDR0);
		}
	}

	return IRQ_HANDLED;
}

/*
 * The functions for inserting/removing us as a module.
 */

static int __devinit atmel_tsadcc_probe(struct platform_device *pdev)
{
	struct atmel_tsadcc	*ts_dev;
	struct input_dev	*input_dev;
	struct resource		*res;
	struct at91_tsadcc_data *pdata = pdev->dev.platform_data;
	int		err = 0;
	unsigned int	prsc;
	unsigned int	reg;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mmio resource defined.\n");
		return -ENXIO;
	}

	/* Allocate memory for device */
	ts_dev = kzalloc(sizeof(struct atmel_tsadcc), GFP_KERNEL);
	if (!ts_dev) {
		dev_err(&pdev->dev, "failed to allocate memory.\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, ts_dev);

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&pdev->dev, "failed to allocate input device.\n");
		err = -EBUSY;
		goto err_free_mem;
	}

	ts_dev->irq = platform_get_irq(pdev, 0);
	if (ts_dev->irq < 0) {
		dev_err(&pdev->dev, "no irq ID is designated.\n");
		err = -ENODEV;
		goto err_free_dev;
	}

	if (!request_mem_region(res->start, resource_size(res),
				"atmel tsadcc regs")) {
		dev_err(&pdev->dev, "resources is unavailable.\n");
		err = -EBUSY;
		goto err_free_dev;
	}

	tsc_base = ioremap(res->start, resource_size(res));
	if (!tsc_base) {
		dev_err(&pdev->dev, "failed to map registers.\n");
		err = -ENOMEM;
		goto err_release_mem;
	}

	err = request_irq(ts_dev->irq, atmel_tsadcc_interrupt, IRQF_DISABLED,
			pdev->dev.driver->name, ts_dev);
	if (err) {
		dev_err(&pdev->dev, "failed to allocate irq.\n");
		goto err_unmap_regs;
	}

	ts_dev->clk = clk_get(&pdev->dev, "tsc_clk");
	if (IS_ERR(ts_dev->clk)) {
		dev_err(&pdev->dev, "failed to get ts_clk\n");
		err = PTR_ERR(ts_dev->clk);
		goto err_free_irq;
	}

	ts_dev->input = input_dev;
	ts_dev->bufferedmeasure = 0;

	snprintf(ts_dev->phys, sizeof(ts_dev->phys),
		 "%s/input0", dev_name(&pdev->dev));

	input_dev->name = "atmel touch screen controller";
	input_dev->phys = ts_dev->phys;
	input_dev->dev.parent = &pdev->dev;

	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 0x3FF, 0, 0);

	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

	/* clk_enable() always returns 0, no need to check it */
	clk_enable(ts_dev->clk);

	prsc = clk_get_rate(ts_dev->clk);
	dev_info(&pdev->dev, "Master clock is set at: %d Hz\n", prsc);

	if (!pdata)
		goto err_fail;

	if (!pdata->adc_clock)
		pdata->adc_clock = ADC_DEFAULT_CLOCK;

	prsc = (prsc / (2 * pdata->adc_clock)) - 1;

	/* saturate if this value is too high */
	if (cpu_is_at91sam9rl()) {
		if (prsc > PRESCALER_VAL(ATMEL_TSADCC_PRESCAL))
			prsc = PRESCALER_VAL(ATMEL_TSADCC_PRESCAL);
	} else {
		if (prsc > PRESCALER_VAL(ATMEL_TSADCC_EPRESCAL))
			prsc = PRESCALER_VAL(ATMEL_TSADCC_EPRESCAL);
	}

	dev_info(&pdev->dev, "Prescaler is set at: %d\n", prsc);

	if (cpu_has_9x5_adc()) {
		reg = 	((0x01 << 5) & ATMEL_TSADCC_SLEEP)	|	/* Sleep Mode */
			(prsc << 8)				|
			((0x8 << 16) & ATMEL_TSADCC_STARTUP)	|
			((pdata->ts_sample_hold_time << 24) & ATMEL_TSADCC_TRACKTIM);
	} else {
		reg = ATMEL_TSADCC_TSAMOD_TS_ONLY_MODE		|
			((0x00 << 5) & ATMEL_TSADCC_SLEEP)	|	/* Normal Mode */
			((0x01 << 6) & ATMEL_TSADCC_PENDET)	|	/* Enable Pen Detect */
			(prsc << 8)				|
			((0x26 << 16) & ATMEL_TSADCC_STARTUP)	|
			((pdata->pendet_debounce << 28) & ATMEL_TSADCC_PENDBC);
	}

	atmel_tsadcc_write(ATMEL_TSADCC_CR, ATMEL_TSADCC_SWRST);
	atmel_tsadcc_write(ATMEL_TSADCC_MR, reg);
	atmel_tsadcc_write(ATMEL_TSADCC_TRGR, ATMEL_TSADCC_TRGMOD_NONE);

	if (cpu_has_9x5_adc()) {
		atmel_tsadcc_write(ATMEL_TSADCC_TSMR,
					ATMEL_TSADCC_TSMODE_4WIRE_NO_PRESS	|
					ATMEL_TSADCC_NOTSDMA			|
					ATMEL_TSADCC_PENDET_ENA			|
					(pdata->pendet_debounce << 28)		|
					(0x0 << 8));
	} else {
		atmel_tsadcc_write(ATMEL_TSADCC_TSR,
			(pdata->ts_sample_hold_time << 24) & ATMEL_TSADCC_TSSHTIM);
	}

	atmel_tsadcc_read(ATMEL_TSADCC_SR);
	atmel_tsadcc_write(ATMEL_TSADCC_IER, ATMEL_TSADCC_PENCNT);

	/* All went ok, so register to the input system */
	err = input_register_device(input_dev);
	if (err)
		goto err_fail;

	return 0;

err_fail:
	clk_disable(ts_dev->clk);
	clk_put(ts_dev->clk);
err_free_irq:
	free_irq(ts_dev->irq, ts_dev);
err_unmap_regs:
	iounmap(tsc_base);
err_release_mem:
	release_mem_region(res->start, resource_size(res));
err_free_dev:
	input_free_device(ts_dev->input);
err_free_mem:
	kfree(ts_dev);
	return err;
}

static int __devexit atmel_tsadcc_remove(struct platform_device *pdev)
{
	struct atmel_tsadcc *ts_dev = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	free_irq(ts_dev->irq, ts_dev);

	input_unregister_device(ts_dev->input);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iounmap(tsc_base);
	release_mem_region(res->start, resource_size(res));

	clk_disable(ts_dev->clk);
	clk_put(ts_dev->clk);

	kfree(ts_dev);

	return 0;
}

static struct platform_driver atmel_tsadcc_driver = {
	.probe		= atmel_tsadcc_probe,
	.remove		= __devexit_p(atmel_tsadcc_remove),
	.driver		= {
		.name	= "atmel_tsadcc",
	},
};

static int __init atmel_tsadcc_init(void)
{
	return platform_driver_register(&atmel_tsadcc_driver);
}

static void __exit atmel_tsadcc_exit(void)
{
	platform_driver_unregister(&atmel_tsadcc_driver);
}

module_init(atmel_tsadcc_init);
module_exit(atmel_tsadcc_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Atmel TouchScreen Driver");
MODULE_AUTHOR("Dan Liang <dan.liang@atmel.com>");


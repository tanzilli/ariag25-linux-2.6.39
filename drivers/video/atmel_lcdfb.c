/*
 *  Driver for AT91/AT32 LCD Controller
 *
 *  Copyright (C) 2007-2010 Atmel Corporation
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <video/atmel_lcdc.h>

#ifdef CONFIG_PM

static int atmel_lcdfb_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	struct atmel_lcdfb_info *sinfo = info->par;

	/*
	 * We don't want to handle interrupts while the clock is
	 * stopped. It may take forever.
	 */
	lcdc_writel(sinfo, ATMEL_LCDC_IDR, ~0UL);

	sinfo->saved_lcdcon = lcdc_readl(sinfo, ATMEL_LCDC_CONTRAST_VAL);
	lcdc_writel(sinfo, ATMEL_LCDC_CONTRAST_CTR, 0);

	if (sinfo->atmel_lcdfb_power_control)
		sinfo->atmel_lcdfb_power_control(0);

	atmel_lcdfb_stop(sinfo);
	atmel_lcdfb_stop_clock(sinfo);

	return 0;
}

static int atmel_lcdfb_resume(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	struct atmel_lcdfb_info *sinfo = info->par;

	atmel_lcdfb_start_clock(sinfo);
	atmel_lcdfb_start(sinfo);
	if (sinfo->atmel_lcdfb_power_control)
		sinfo->atmel_lcdfb_power_control(1);

	lcdc_writel(sinfo, ATMEL_LCDC_CONTRAST_CTR, sinfo->saved_lcdcon);

	/* Enable FIFO & DMA errors */
	lcdc_writel(sinfo, ATMEL_LCDC_IER, ATMEL_LCDC_UFLWI
			| ATMEL_LCDC_OWRI | ATMEL_LCDC_MERI);

	return 0;
}

#else
#define atmel_lcdfb_suspend	NULL
#define atmel_lcdfb_resume	NULL
#endif

static int __init atmel_lcdfb_probe(struct platform_device *pdev)
{
	return __atmel_lcdfb_probe(pdev);
}
static int __exit atmel_lcdfb_remove(struct platform_device *pdev)
{
	return __atmel_lcdfb_remove(pdev);
}

static struct platform_driver atmel_lcdfb_driver = {
	.remove		= __exit_p(atmel_lcdfb_remove),
	.suspend	= atmel_lcdfb_suspend,
	.resume		= atmel_lcdfb_resume,

	.driver		= {
		.name	= "atmel_lcdfb",
		.owner	= THIS_MODULE,
	},
};

static int __init atmel_lcdfb_init(void)
{
	return platform_driver_probe(&atmel_lcdfb_driver, atmel_lcdfb_probe);
}
module_init(atmel_lcdfb_init);

static void __exit atmel_lcdfb_exit(void)
{
	platform_driver_unregister(&atmel_lcdfb_driver);
}
module_exit(atmel_lcdfb_exit);

MODULE_DESCRIPTION("AT91/AT32 LCD Controller framebuffer driver");
MODULE_AUTHOR("Nicolas Ferre <nicolas.ferre@atmel.com>");
MODULE_LICENSE("GPL");

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
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <mach/board.h>
#include <mach/cpu.h>
#include <mach/atmel_lcdc.h>

#include <video/atmel_lcdfb.h>

/* configurable parameters */
#define ATMEL_LCDC_CVAL_DEFAULT		0xc8
#define ATMEL_LCDC_DMA_BURST_LEN	8	/* words */
#define ATMEL_LCDC_FIFO_SIZE		512	/* words */

static u32 contrast_ctr = ATMEL_LCDC_PS_DIV8
		| ATMEL_LCDC_POL_POSITIVE
		| ATMEL_LCDC_ENA_PWMENABLE;

#if defined(CONFIG_ARCH_AT91)
#define	ATMEL_LCDFB_FBINFO_DEFAULT	(FBINFO_DEFAULT \
					 | FBINFO_PARTIAL_PAN_OK \
					 | FBINFO_HWACCEL_YPAN)

static inline void atmel_lcdfb_update_dma2d(struct atmel_lcdfb_info *sinfo,
					struct fb_var_screeninfo *var)
{

}
#elif defined(CONFIG_AVR32)
#define	ATMEL_LCDFB_FBINFO_DEFAULT	(FBINFO_DEFAULT \
					| FBINFO_PARTIAL_PAN_OK \
					| FBINFO_HWACCEL_XPAN \
					| FBINFO_HWACCEL_YPAN)

static void atmel_lcdfb_update_dma2d(struct atmel_lcdfb_info *sinfo,
				     struct fb_var_screeninfo *var)
{
	u32 dma2dcfg;
	u32 pixeloff;

	pixeloff = (var->xoffset * var->bits_per_pixel) & 0x1f;

	dma2dcfg = ((var->xres_virtual - var->xres) * var->bits_per_pixel) / 8;
	dma2dcfg |= pixeloff << ATMEL_LCDC_PIXELOFF_OFFSET;
	lcdc_writel(sinfo, ATMEL_LCDC_DMA2DCFG, dma2dcfg);

	/* Update configuration */
	lcdc_writel(sinfo, ATMEL_LCDC_DMACON,
		    lcdc_readl(sinfo, ATMEL_LCDC_DMACON)
		    | ATMEL_LCDC_DMAUPDT);
}
#endif

static void atmel_lcdfb_update_dma(struct fb_info *info,
			       struct fb_var_screeninfo *var)
{
	struct atmel_lcdfb_info *sinfo = info->par;
	struct fb_fix_screeninfo *fix = &info->fix;
	unsigned long dma_addr;

	dma_addr = (fix->smem_start + var->yoffset * fix->line_length
		    + var->xoffset * var->bits_per_pixel / 8);

	dma_addr &= ~3UL;

	/* Set framebuffer DMA base address and pixel offset */
	lcdc_writel(sinfo, ATMEL_LCDC_DMABADDR1, dma_addr);

	atmel_lcdfb_update_dma2d(sinfo, var);
}

/* some bl->props field just changed */
static int atmel_bl_update_status(struct backlight_device *bl)
{
	struct atmel_lcdfb_info *sinfo = bl_get_data(bl);
	int power = sinfo->bl_power;
	int brightness = bl->props.brightness;

	/* REVISIT there may be a meaningful difference between
	 * fb_blank and power ... there seem to be some cases
	 * this doesn't handle correctly.
	 */
	if (bl->props.fb_blank != sinfo->bl_power)
		power = bl->props.fb_blank;
	else if (bl->props.power != sinfo->bl_power)
		power = bl->props.power;

	if (brightness < 0 && power == FB_BLANK_UNBLANK)
		brightness = lcdc_readl(sinfo, ATMEL_LCDC_CONTRAST_VAL);
	else if (power != FB_BLANK_UNBLANK)
		brightness = 0;

	lcdc_writel(sinfo, ATMEL_LCDC_CONTRAST_VAL, brightness);
	lcdc_writel(sinfo, ATMEL_LCDC_CONTRAST_CTR,
			brightness ? contrast_ctr : 0);

	bl->props.fb_blank = bl->props.power = sinfo->bl_power = power;

	return 0;
}

static int atmel_bl_get_brightness(struct backlight_device *bl)
{
	struct atmel_lcdfb_info *sinfo = bl_get_data(bl);

	return lcdc_readl(sinfo, ATMEL_LCDC_CONTRAST_VAL);
}

static const struct backlight_ops atmel_lcdc_bl_ops = {
	.update_status = atmel_bl_update_status,
	.get_brightness = atmel_bl_get_brightness,
};

static void atmel_lcdfb_init_contrast(struct atmel_lcdfb_info *sinfo)
{
	/* contrast pwm can be 'inverted' */
	if (sinfo->lcdcon_pol_negative)
			contrast_ctr &= ~(ATMEL_LCDC_POL_POSITIVE);

	/* have some default contrast/backlight settings */
	lcdc_writel(sinfo, ATMEL_LCDC_CONTRAST_CTR, contrast_ctr);
	lcdc_writel(sinfo, ATMEL_LCDC_CONTRAST_VAL, ATMEL_LCDC_CVAL_DEFAULT);
}

void atmel_lcdfb_start(struct atmel_lcdfb_info *sinfo)
{
	lcdc_writel(sinfo, ATMEL_LCDC_DMACON, sinfo->default_dmacon);
	lcdc_writel(sinfo, ATMEL_LCDC_PWRCON,
		(sinfo->guard_time << ATMEL_LCDC_GUARDT_OFFSET)
		| ATMEL_LCDC_PWR);
}

static void atmel_lcdfb_stop(struct atmel_lcdfb_info *sinfo, u32 flags)
{
	/* Turn off the LCD controller and the DMA controller */
	lcdc_writel(sinfo, ATMEL_LCDC_PWRCON,
			sinfo->guard_time << ATMEL_LCDC_GUARDT_OFFSET);

	/* Wait for the LCDC core to become idle */
	while (lcdc_readl(sinfo, ATMEL_LCDC_PWRCON) & ATMEL_LCDC_BUSY)
		msleep(10);

	lcdc_writel(sinfo, ATMEL_LCDC_DMACON, 0);

	if (!(flags & ATMEL_LCDC_STOP_NOWAIT))
		/* Wait for DMA engine to become idle... */
		while (lcdc_readl(sinfo, ATMEL_LCDC_DMACON) & ATMEL_LCDC_DMABUSY)
			msleep(10);
}

static unsigned long compute_hozval(unsigned long xres, unsigned long lcdcon2)
{
	unsigned long value;

	if (!(cpu_is_at91sam9261() || cpu_is_at91sam9g10()
		|| cpu_is_at32ap7000()))
		return xres;

	value = xres;
	if ((lcdcon2 & ATMEL_LCDC_DISTYPE) != ATMEL_LCDC_DISTYPE_TFT) {
		/* STN display */
		if ((lcdcon2 & ATMEL_LCDC_DISTYPE) == ATMEL_LCDC_DISTYPE_STNCOLOR) {
			value *= 3;
		}
		if ( (lcdcon2 & ATMEL_LCDC_IFWIDTH) == ATMEL_LCDC_IFWIDTH_4
		   || ( (lcdcon2 & ATMEL_LCDC_IFWIDTH) == ATMEL_LCDC_IFWIDTH_8
		      && (lcdcon2 & ATMEL_LCDC_SCANMOD) == ATMEL_LCDC_SCANMOD_DUAL ))
			value = DIV_ROUND_UP(value, 4);
		else
			value = DIV_ROUND_UP(value, 8);
	}

	return value;
}

static int atmel_lcdfb_setup_core(struct fb_info *info)
{
	struct atmel_lcdfb_info *sinfo = info->par;
	unsigned long hozval_linesz;
	unsigned long value;
	unsigned long clk_value_khz;
	unsigned long pix_factor = 2;

	/* ...set frame size and burst length = 8 words (?) */
	value = (info->var.yres * info->var.xres * info->var.bits_per_pixel) / 32;
	value |= ((ATMEL_LCDC_DMA_BURST_LEN - 1) << ATMEL_LCDC_BLENGTH_OFFSET);
	lcdc_writel(sinfo, ATMEL_LCDC_DMAFRMCFG, value);

	/* Set pixel clock */
	if (cpu_is_at91sam9g45() && !cpu_is_at91sam9g45es())
		pix_factor = 1;

	clk_value_khz = clk_get_rate(sinfo->lcdc_clk) / 1000;

	value = DIV_ROUND_UP(clk_value_khz, PICOS2KHZ(info->var.pixclock));

	if (value < pix_factor) {
		dev_notice(info->device, "Bypassing pixel clock divider\n");
		lcdc_writel(sinfo, ATMEL_LCDC_LCDCON1, ATMEL_LCDC_BYPASS);
	} else {
		value = (value / pix_factor) - 1;
		dev_dbg(info->device, "  * programming CLKVAL = 0x%08lx\n",
				value);
		lcdc_writel(sinfo, ATMEL_LCDC_LCDCON1,
				value << ATMEL_LCDC_CLKVAL_OFFSET);
		info->var.pixclock =
			KHZ2PICOS(clk_value_khz / (pix_factor * (value + 1)));
		dev_dbg(info->device, "  updated pixclk:     %lu KHz\n",
					PICOS2KHZ(info->var.pixclock));
	}


	/* Initialize control register 2 */
	value = sinfo->default_lcdcon2;

	if (!(info->var.sync & FB_SYNC_HOR_HIGH_ACT))
		value |= ATMEL_LCDC_INVLINE_INVERTED;
	if (!(info->var.sync & FB_SYNC_VERT_HIGH_ACT))
		value |= ATMEL_LCDC_INVFRAME_INVERTED;

	switch (info->var.bits_per_pixel) {
	case 1:
		value |= ATMEL_LCDC_PIXELSIZE_1;
		break;
	case 2:
		value |= ATMEL_LCDC_PIXELSIZE_2;
		break;
	case 4:
		value |= ATMEL_LCDC_PIXELSIZE_4;
		break;
	case 8:
		value |= ATMEL_LCDC_PIXELSIZE_8;
		break;
	case 15: /* fall through */
	case 16:
		value |= ATMEL_LCDC_PIXELSIZE_16;
		break;
	case 24:
		value |= ATMEL_LCDC_PIXELSIZE_24;
		break;
	case 32:
		value |= ATMEL_LCDC_PIXELSIZE_32;
		break;
	default:
		BUG();
		break;
	}
	dev_dbg(info->device, "  * LCDCON2 = %08lx\n", value);
	lcdc_writel(sinfo, ATMEL_LCDC_LCDCON2, value);

	/* Vertical timing */
	value = (info->var.vsync_len - 1) << ATMEL_LCDC_VPW_OFFSET;
	value |= info->var.upper_margin << ATMEL_LCDC_VBP_OFFSET;
	value |= info->var.lower_margin;
	dev_dbg(info->device, "  * LCDTIM1 = %08lx\n", value);
	lcdc_writel(sinfo, ATMEL_LCDC_TIM1, value);

	/* Horizontal timing */
	value = (info->var.right_margin - 1) << ATMEL_LCDC_HFP_OFFSET;
	value |= (info->var.hsync_len - 1) << ATMEL_LCDC_HPW_OFFSET;
	value |= (info->var.left_margin - 1);
	dev_dbg(info->device, "  * LCDTIM2 = %08lx\n", value);
	lcdc_writel(sinfo, ATMEL_LCDC_TIM2, value);

	/* Horizontal value (aka line size) */
	hozval_linesz = compute_hozval(info->var.xres,
				lcdc_readl(sinfo, ATMEL_LCDC_LCDCON2));

	/* Display size */
	value = (hozval_linesz - 1) << ATMEL_LCDC_HOZVAL_OFFSET;
	value |= info->var.yres - 1;
	dev_dbg(info->device, "  * LCDFRMCFG = %08lx\n", value);
	lcdc_writel(sinfo, ATMEL_LCDC_LCDFRMCFG, value);

	/* FIFO Threshold: Use formula from data sheet */
	value = ATMEL_LCDC_FIFO_SIZE - (2 * ATMEL_LCDC_DMA_BURST_LEN + 3);
	lcdc_writel(sinfo, ATMEL_LCDC_FIFO, value);

	/* Toggle LCD_MODE every frame */
	lcdc_writel(sinfo, ATMEL_LCDC_MVAL, 0);

	/* Disable all interrupts */
	lcdc_writel(sinfo, ATMEL_LCDC_IDR, ~0UL);
	/* Enable FIFO & DMA errors */
	lcdc_writel(sinfo, ATMEL_LCDC_IER, ATMEL_LCDC_UFLWI | ATMEL_LCDC_OWRI | ATMEL_LCDC_MERI);

	/* ...wait for DMA engine to become idle... */
	while (lcdc_readl(sinfo, ATMEL_LCDC_DMACON) & ATMEL_LCDC_DMABUSY)
		msleep(10);

	return 0;
}

static void atmelfb_limit_screeninfo(struct fb_var_screeninfo *var)
{
	/* Saturate vertical and horizontal timings at maximum values */
	var->vsync_len = min_t(u32, var->vsync_len,
			(ATMEL_LCDC_VPW >> ATMEL_LCDC_VPW_OFFSET) + 1);
	var->upper_margin = min_t(u32, var->upper_margin,
			ATMEL_LCDC_VBP >> ATMEL_LCDC_VBP_OFFSET);
	var->lower_margin = min_t(u32, var->lower_margin,
			ATMEL_LCDC_VFP);
	var->right_margin = min_t(u32, var->right_margin,
			(ATMEL_LCDC_HFP >> ATMEL_LCDC_HFP_OFFSET) + 1);
	var->hsync_len = min_t(u32, var->hsync_len,
			(ATMEL_LCDC_HPW >> ATMEL_LCDC_HPW_OFFSET) + 1);
	var->left_margin = min_t(u32, var->left_margin,
			ATMEL_LCDC_HBP + 1);
}

static irqreturn_t atmel_lcdfb_interrupt(int irq, void *dev_id)
{
	struct fb_info *info = dev_id;
	struct atmel_lcdfb_info *sinfo = info->par;
	u32 status;

	status = lcdc_readl(sinfo, ATMEL_LCDC_ISR);
	if (status & ATMEL_LCDC_UFLWI) {
		dev_warn(info->device, "FIFO underflow %#x\n", status);
		/* reset DMA and FIFO to avoid screen shifting */
		schedule_work(&sinfo->task);
	}
	lcdc_writel(sinfo, ATMEL_LCDC_ICR, status);
	return IRQ_HANDLED;
}

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

	atmel_lcdfb_stop(sinfo, 0);
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

static struct atmel_lcdfb_devdata dev_data = {
	.setup_core = atmel_lcdfb_setup_core,
	.start = atmel_lcdfb_start,
	.stop = atmel_lcdfb_stop,
	.isr = atmel_lcdfb_interrupt,
	.update_dma = atmel_lcdfb_update_dma,
	.bl_ops = &atmel_lcdc_bl_ops,
	.init_contrast = atmel_lcdfb_init_contrast,
	.limit_screeninfo = atmelfb_limit_screeninfo,
	.fbinfo_flags = ATMEL_LCDFB_FBINFO_DEFAULT,
};

static int __init atmel_lcdfb_probe(struct platform_device *pdev)
{
	return __atmel_lcdfb_probe(pdev, &dev_data);
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

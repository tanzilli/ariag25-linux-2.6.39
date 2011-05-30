/*
 *  Header file for AT91/AT32 LCD Controller
 *
 *  Data structure and register user interface
 *
 *  Copyright (C) 2007 Atmel Corporation
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
#ifndef __ATMEL_LCDC_H__
#define __ATMEL_LCDC_H__

#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/backlight.h>

/* Way LCD wires are connected to the chip:
 * Some Atmel chips use BGR color mode (instead of standard RGB)
 * A swapped wiring onboard can bring to RGB mode.
 */
#define ATMEL_LCDC_WIRING_BGR	0
#define ATMEL_LCDC_WIRING_RGB	1
#define ATMEL_LCDC_WIRING_RGB555	2

#define ATMEL_LCDC_STOP_NOWAIT (1 << 0)

struct atmel_lcdfb_info;

struct atmel_lcdfb_devdata {
	int (*setup_core)(struct fb_info *info);
	void (*start)(struct atmel_lcdfb_info *sinfo);
	void (*stop)(struct atmel_lcdfb_info *sinfo, u32 flags);
	irqreturn_t (*isr)(int irq, void *dev_id);
	void (*update_dma)(struct fb_info *info, struct fb_var_screeninfo *var);
	void (*init_contrast)(struct atmel_lcdfb_info *sinfo);
	void (*limit_screeninfo)(struct fb_var_screeninfo *var);
	const struct backlight_ops *bl_ops;
	int fbinfo_flags;
	int dma_desc_size;
};

extern void atmel_lcdfb_start_clock(struct atmel_lcdfb_info *sinfo);
extern void atmel_lcdfb_stop_clock(struct atmel_lcdfb_info *sinfo);
extern int __atmel_lcdfb_probe(struct platform_device *pdev,
				struct atmel_lcdfb_devdata *devdata);
extern int __atmel_lcdfb_remove(struct platform_device *pdev);

 /* LCD Controller info data structure, stored in device platform_data */
struct atmel_lcdfb_info {
	spinlock_t		lock;
	struct fb_info		*info;
	void __iomem		*mmio;
	void __iomem		*clut;
	int			irq_base;
	struct atmel_lcdfb_devdata *dev_data;
	struct work_struct	task;

	void			*dma_desc;
	dma_addr_t		dma_desc_phys;

	unsigned int		guard_time;
	unsigned int 		smem_len;
	struct platform_device	*pdev;
	struct clk		*bus_clk;
	struct clk		*lcdc_clk;

#ifdef CONFIG_BACKLIGHT_ATMEL_LCDC
	struct backlight_device	*backlight;
	u8			bl_power;
#endif
	bool			lcdcon_is_backlight;
	bool			lcdcon_pol_negative;
	bool			alpha_enabled;
	u8			saved_lcdcon;

	u8			default_bpp;
	u8			lcd_wiring_mode;
	unsigned int		default_lcdcon2;
	unsigned int		default_dmacon;
	void (*atmel_lcdfb_power_control)(int on);
	struct fb_monspecs	*default_monspecs;
	u32			pseudo_palette[16];
};

#define lcdc_readl(sinfo, reg)		__raw_readl((sinfo)->mmio+(reg))
#define lcdc_writel(sinfo, reg, val)	__raw_writel((val), (sinfo)->mmio+(reg))

#endif /* __ATMEL_LCDC_H__ */

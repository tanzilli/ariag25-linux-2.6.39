/*
 * atmel-pcm.c  --  ALSA PCM interface for the Atmel atmel SoC.
 *
 *  Copyright (C) 2005 SAN People
 *  Copyright (C) 2008 Atmel
 *
 * Authors: Sedji Gaouaou <sedji.gaouaou@atmel.com>
 *
 * Based on at91-pcm. by:
 * Frank Mandarino <fmandarino@endrelia.com>
 * Copyright 2006 Endrelia Technologies Inc.
 *
 * Based on pxa2xx-pcm.c by:
 *
 * Author:	Nicolas Pitre
 * Created:	Nov 30, 2004
 * Copyright:	(C) 2004 MontaVista Software, Inc.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/atmel_pdc.h>
#include <linux/dmaengine.h>
#include <linux/atmel-ssc.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/at_hdmac.h>

#include "atmel-pcm.h"


/*--------------------------------------------------------------------------*\
 * Hardware definition
\*--------------------------------------------------------------------------*/
/* TODO: These values were taken from the AT91 platform driver, check
 *	 them against real values for AT32
 */
static const struct snd_pcm_hardware atmel_pcm_pdc_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min	= 32,
	.period_bytes_max	= 8192,
	.periods_min		= 2,
	.periods_max		= 1024,
	.buffer_bytes_max	= 32 * 1024,
};

static const struct snd_pcm_hardware atmel_pcm_dma_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_PAUSE,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min	= 256,		/* not too low to absorb DMA programming overhead */
	.period_bytes_max	= 2 * 0xffff,	/* if 2 bytes format */
	.periods_min		= 8,
	.periods_max		= 1024,		/* no limit */
	.buffer_bytes_max	= 64 * 1024,	/* 64KiB */
};

static const struct snd_pcm_hardware *atmel_pcm_hardware;


/*--------------------------------------------------------------------------*\
 * Data types
\*--------------------------------------------------------------------------*/
struct atmel_runtime_data {
	struct atmel_pcm_dma_params *params;
	dma_addr_t dma_buffer;		/* physical address of dma buffer */
	dma_addr_t dma_buffer_end;	/* first address beyond DMA buffer */
	size_t period_size;

	dma_addr_t period_ptr;		/* physical address of next period */
	int periods;			/* period index of period_ptr */

	/* PDC register save */
	u32 pdc_xpr_save;
	u32 pdc_xcr_save;
	u32 pdc_xnpr_save;
	u32 pdc_xncr_save;

	/* dmaengine data */
	struct at_dma_slave atslave;
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;
	struct dma_chan *dma_chan;
};


/*--------------------------------------------------------------------------*\
 * Helper functions
\*--------------------------------------------------------------------------*/
static int atmel_pcm_preallocate_dma_buffer(struct snd_pcm *pcm,
	int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = atmel_pcm_hardware->buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
					  &buf->addr, GFP_KERNEL);
	pr_debug("atmel-pcm:"
		"preallocate_dma_buffer: area=%p, addr=%p, size=%d\n",
		(void *) buf->area,
		(void *) buf->addr,
		size);

	if (!buf->area)
		return -ENOMEM;

	buf->bytes = size;
	return 0;
}
/*--------------------------------------------------------------------------*\
 * ISR
\*--------------------------------------------------------------------------*/
static void atmel_pcm_pdc_irq(u32 ssc_sr,
	struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;
	static int count;

	count++;

	if (ssc_sr & params->mask->ssc_endbuf) {
		pr_warning("atmel-pcm: buffer %s on %s"
				" (SSC_SR=%#x, count=%d)\n",
				substream->stream == SNDRV_PCM_STREAM_PLAYBACK
				? "underrun" : "overrun",
				params->name, ssc_sr, count);

		/* re-start the PDC */
		ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
			   params->mask->pdc_disable);
		prtd->period_ptr += prtd->period_size;
		if (prtd->period_ptr >= prtd->dma_buffer_end)
			prtd->period_ptr = prtd->dma_buffer;

		ssc_writex(params->ssc->regs, params->pdc->xpr,
			   prtd->period_ptr);
		ssc_writex(params->ssc->regs, params->pdc->xcr,
			   prtd->period_size / params->data_xfer_size);
		ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
			   params->mask->pdc_enable);
	}

	if (ssc_sr & params->mask->ssc_endx) {
		/* Load the PDC next pointer and counter registers */
		prtd->period_ptr += prtd->period_size;
		if (prtd->period_ptr >= prtd->dma_buffer_end)
			prtd->period_ptr = prtd->dma_buffer;

		ssc_writex(params->ssc->regs, params->pdc->xnpr,
			   prtd->period_ptr);
		ssc_writex(params->ssc->regs, params->pdc->xncr,
			   prtd->period_size / params->data_xfer_size);
	}

	snd_pcm_period_elapsed(substream);
}

/**
 * atmel_pcm_dma_irq: SSC interrupt handler for DMAENGINE enabled SSC
 *
 * We use DMAENGINE to send/receive data to/from SSC so this ISR is only to
 * check if any overrun occured.
 */
static void atmel_pcm_dma_irq(u32 ssc_sr,
	struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;

	if (ssc_sr & params->mask->ssc_error) {
		if (snd_pcm_running(substream))
			pr_warning("atmel-pcm: buffer %s on %s"
					" (SSC_SR=%#x)\n",
					substream->stream == SNDRV_PCM_STREAM_PLAYBACK
					? "underrun" : "overrun",
					params->name, ssc_sr);

		/* stop RX and capture stream: will be enabled again at restart */
		ssc_writex(params->ssc->regs, SSC_CR, params->mask->ssc_disable);
		snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);

		/* now drain RHR and read status to remove xrun condition */
		ssc_readx(params->ssc->regs, SSC_RHR);
		ssc_readx(params->ssc->regs, SSC_SR);
	}
}

/*--------------------------------------------------------------------------*\
 * DMAENGINE operations
\*--------------------------------------------------------------------------*/
static bool filter(struct dma_chan *chan, void *slave)
{
	struct	at_dma_slave		*sl = slave;

	if (sl->dma_dev == chan->device->dev) {
		chan->private = sl;
		return true;
	} else {
		return false;
	}
}

static int atmel_pcm_dma_alloc(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;
	struct ssc_device *ssc = prtd->params->ssc;
	struct at_dma_slave *sdata = NULL;

	if (ssc->pdev)
		sdata = ssc->pdev->dev.platform_data;

	if (sdata && sdata->dma_dev) {
		dma_cap_mask_t mask;

		/* setup DMA addresses */
		sdata->rx_reg = (dma_addr_t)ssc->phybase + SSC_RHR;
		sdata->tx_reg = (dma_addr_t)ssc->phybase + SSC_THR;

		/* Try to grab a DMA channel */
		dma_cap_zero(mask);
		dma_cap_set(DMA_CYCLIC, mask);
		prtd->dma_chan = dma_request_channel(mask, filter, sdata);
	}
	if (!prtd->dma_chan) {
		pr_err("atmel-pcm: "
			"DMA channel not available, unable to use SSC-audio\n");
		return -EBUSY;
	}

	return 0;
}

static void audio_dma_irq(void *data)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;

	prtd->period_ptr += prtd->period_size;
	if (prtd->period_ptr >= prtd->dma_buffer_end)
		prtd->period_ptr = prtd->dma_buffer;

	snd_pcm_period_elapsed(substream);
}

static void atmel_pcm_dma_slave_config(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;
	struct dma_chan *chan = prtd->dma_chan;
	struct dma_slave_config slave_config;
	enum dma_slave_buswidth buswidth;

	switch (prtd->params->data_xfer_size) {
	case 1:
		buswidth = DMA_SLAVE_BUSWIDTH_1_BYTE;
		break;
	case 2:
		buswidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case 4:
		buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		return;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config.direction = DMA_TO_DEVICE;
		slave_config.dst_addr_width = buswidth;
	} else {
		slave_config.direction = DMA_FROM_DEVICE;
		slave_config.src_addr_width = buswidth;
	}

	chan->device->device_control(chan, DMA_SLAVE_CONFIG,
			(unsigned long)&slave_config);

}

static int atmel_pcm_dma_prep(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;
	struct dma_chan *chan = prtd->dma_chan;

	if (prtd->desc)
		/* already been there: redo the prep job */
		chan->device->device_control(chan, DMA_TERMINATE_ALL, 0);

	/* setup dma configuration */
	atmel_pcm_dma_slave_config(substream);

	prtd->desc = chan->device->device_prep_dma_cyclic(chan, prtd->dma_buffer,
			prtd->period_size * prtd->periods,
			prtd->period_size,
			substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			DMA_TO_DEVICE : DMA_FROM_DEVICE);
	if (!prtd->desc) {
		dev_err(&chan->dev->device, "cannot prepare slave dma\n");
		return -EINVAL;
	}

	prtd->desc->callback = audio_dma_irq;
	prtd->desc->callback_param = substream;

	return 0;
}

/*--------------------------------------------------------------------------*\
 * PCM operations
\*--------------------------------------------------------------------------*/
static int atmel_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret;

	/* this may get called several times by oss emulation
	 * with different params */

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);

	prtd->params = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	prtd->dma_buffer = runtime->dma_addr;
	prtd->dma_buffer_end = runtime->dma_addr + runtime->dma_bytes;
	prtd->period_size = params_period_bytes(params);
	prtd->periods = params_periods(params);

	if (ssc_use_dmaengine()) {
		if (prtd->dma_chan == NULL) {
			ret = atmel_pcm_dma_alloc(substream, params);
			if (ret)
				return ret;
		}
		ret = atmel_pcm_dma_prep(substream);
		if (ret) {
			dma_release_channel(prtd->dma_chan);
			prtd->dma_chan = NULL;
			return ret;
		}

		prtd->params->dma_intr_handler = atmel_pcm_dma_irq;
	} else {
		prtd->params->dma_intr_handler = atmel_pcm_pdc_irq;
	}

	pr_debug("atmel-pcm: "
		"hw_params: %s%s for %s initialized "
		"(dma_bytes=%u, period_size=%u)\n",
		prtd->dma_chan ? "DMA " : "PDC",
		prtd->dma_chan ? dma_chan_name(prtd->dma_chan): "",
		prtd->params->name,
		runtime->dma_bytes,
		prtd->period_size);
	return 0;
}

static int atmel_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;

	if (params != NULL) {
		if (ssc_use_dmaengine()) {
			struct dma_chan *chan = prtd->dma_chan;

			if (chan) {
				chan->device->device_control(chan,
							DMA_TERMINATE_ALL, 0);
				prtd->cookie = 0;
				prtd->desc = NULL;
				dma_release_channel(chan);
				prtd->dma_chan = NULL;
			}
		} else {
			ssc_writex(params->ssc->regs, SSC_PDC_PTCR,
				   params->mask->pdc_disable);
		}
		prtd->params->dma_intr_handler = NULL;
	}

	return 0;
}

/*--------------------------------------------------------------------------*\
 * PCM callbacks using PDC
\*--------------------------------------------------------------------------*/
static int atmel_pcm_pdc_prepare(struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;

	ssc_writex(params->ssc->regs, SSC_IDR,
		   params->mask->ssc_endx | params->mask->ssc_endbuf);
	ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
		   params->mask->pdc_disable);
	return 0;
}

static int atmel_pcm_pdc_trigger(struct snd_pcm_substream *substream,
	int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	struct atmel_runtime_data *prtd = rtd->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;
	int ret = 0;

	pr_debug("atmel-pcm:buffer_size = %ld,"
		"dma_area = %p, dma_bytes = %u\n",
		rtd->buffer_size, rtd->dma_area, rtd->dma_bytes);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->period_ptr = prtd->dma_buffer;

		ssc_writex(params->ssc->regs, params->pdc->xpr,
			   prtd->period_ptr);
		ssc_writex(params->ssc->regs, params->pdc->xcr,
			   prtd->period_size / params->data_xfer_size);

		prtd->period_ptr += prtd->period_size;
		ssc_writex(params->ssc->regs, params->pdc->xnpr,
			   prtd->period_ptr);
		ssc_writex(params->ssc->regs, params->pdc->xncr,
			   prtd->period_size / params->data_xfer_size);

		pr_debug("atmel-pcm: trigger: "
			"period_ptr=%lx, xpr=%u, "
			"xcr=%u, xnpr=%u, xncr=%u\n",
			(unsigned long)prtd->period_ptr,
			ssc_readx(params->ssc->regs, params->pdc->xpr),
			ssc_readx(params->ssc->regs, params->pdc->xcr),
			ssc_readx(params->ssc->regs, params->pdc->xnpr),
			ssc_readx(params->ssc->regs, params->pdc->xncr));

		ssc_writex(params->ssc->regs, SSC_IER,
			   params->mask->ssc_endx | params->mask->ssc_endbuf);
		ssc_writex(params->ssc->regs, SSC_PDC_PTCR,
			   params->mask->pdc_enable);

		pr_debug("sr=%u imr=%u\n",
			ssc_readx(params->ssc->regs, SSC_SR),
			ssc_readx(params->ssc->regs, SSC_IMR));
		break;		/* SNDRV_PCM_TRIGGER_START */

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
			   params->mask->pdc_disable);
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
			   params->mask->pdc_enable);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t atmel_pcm_pdc_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;
	dma_addr_t ptr;
	snd_pcm_uframes_t x;

	ptr = (dma_addr_t) ssc_readx(params->ssc->regs, params->pdc->xpr);
	x = bytes_to_frames(runtime, ptr - prtd->dma_buffer);

	if (x == runtime->buffer_size)
		x = 0;

	return x;
}

/*--------------------------------------------------------------------------*\
 * PCM callbacks using DMAENGINE
\*--------------------------------------------------------------------------*/
static int atmel_pcm_dma_prepare(struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;

	ssc_writex(params->ssc->regs, SSC_IDR, params->mask->ssc_error);
	return 0;
}

static int atmel_pcm_dma_trigger(struct snd_pcm_substream *substream,
	int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	struct atmel_runtime_data *prtd = rtd->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;
	dma_cookie_t cookie;
	int ret = 0;

	pr_debug("atmel-pcm: trigger %d: buffer_size = %ld,"
		" dma_area = %p, dma_bytes = %u\n",
		cmd, rtd->buffer_size, rtd->dma_area, rtd->dma_bytes);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:

		if (prtd->cookie < DMA_MIN_COOKIE) {
			cookie = prtd->desc->tx_submit(prtd->desc);
			if (dma_submit_error(cookie)) {
				ret = -EINVAL;
				break;
			}
			prtd->cookie = cookie;
			prtd->period_ptr = prtd->dma_buffer;
		}


		pr_debug("atmel-pcm: trigger: start sr=0x%08x imr=0x%08u\n",
			ssc_readx(params->ssc->regs, SSC_SR),
			ssc_readx(params->ssc->regs, SSC_IMR));

		/* fallback */
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		prtd->dma_chan->device->device_control(prtd->dma_chan,
							DMA_RESUME, 0);

		/* It not already done or comming from xrun state */
		ssc_readx(params->ssc->regs, SSC_SR);
		ssc_writex(params->ssc->regs, SSC_IER, params->mask->ssc_error);
		ssc_writex(params->ssc->regs, SSC_CR, params->mask->ssc_enable);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("atmel-pcm: trigger: stop cmd = %d\n", cmd);

		ssc_writex(params->ssc->regs, SSC_IDR, params->mask->ssc_error);

		/* fallback */
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		prtd->dma_chan->device->device_control(prtd->dma_chan,
							DMA_PAUSE, 0);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t atmel_pcm_dma_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;
	snd_pcm_uframes_t x;

	dev_vdbg(substream->pcm->card->dev, "%s: 0x%08x %ld\n",
		__func__, prtd->period_ptr,
		bytes_to_frames(runtime, prtd->period_ptr - prtd->dma_buffer));

	x = bytes_to_frames(runtime, prtd->period_ptr - prtd->dma_buffer);

	if (x >= runtime->buffer_size)
		x = 0;

	return x;
}


/*--------------------------------------------------------------------------*\
 * PCM open/close/mmap
\*--------------------------------------------------------------------------*/
static int atmel_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd;
	int ret = 0;

	snd_soc_set_runtime_hwparams(substream, atmel_pcm_hardware);

	/* ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	prtd = kzalloc(sizeof(struct atmel_runtime_data), GFP_KERNEL);
	if (prtd == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	runtime->private_data = prtd;

 out:
	return ret;
}

static int atmel_pcm_close(struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;

	kfree(prtd);
	return 0;
}

static int atmel_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	return remap_pfn_range(vma, vma->vm_start,
		       substream->dma_buffer.addr >> PAGE_SHIFT,
		       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static struct snd_pcm_ops atmel_pcm_ops = {
	.open		= atmel_pcm_open,
	.close		= atmel_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= atmel_pcm_hw_params,
	.hw_free	= atmel_pcm_hw_free,
	.prepare	= atmel_pcm_pdc_prepare,
	.trigger	= atmel_pcm_pdc_trigger,
	.pointer	= atmel_pcm_pdc_pointer,
	.mmap		= atmel_pcm_mmap,
};


/*--------------------------------------------------------------------------*\
 * ASoC platform driver
\*--------------------------------------------------------------------------*/
static u64 atmel_pcm_dmamask = 0xffffffff;

static int atmel_pcm_new(struct snd_card *card,
	struct snd_soc_dai *dai, struct snd_pcm *pcm)
{
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &atmel_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (dai->driver->playback.channels_min) {
		ret = atmel_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->driver->capture.channels_min) {
		pr_debug("atmel-pcm:"
				"Allocating PCM capture DMA buffer\n");
		ret = atmel_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
 out:
	return ret;
}

static void atmel_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
		dma_free_coherent(pcm->card->dev, buf->bytes,
				  buf->area, buf->addr);
		buf->area = NULL;
	}
}

#ifdef CONFIG_PM
static int atmel_pcm_suspend(struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = dai->runtime;
	struct atmel_runtime_data *prtd;
	struct atmel_pcm_dma_params *params;

	if (!runtime)
		return 0;

	prtd = runtime->private_data;
	params = prtd->params;

	if (!ssc_use_dmaengine()) {
		/* disable the PDC and save the PDC registers */

		ssc_writel(params->ssc->regs, PDC_PTCR, params->mask->pdc_disable);

		prtd->pdc_xpr_save = ssc_readx(params->ssc->regs, params->pdc->xpr);
		prtd->pdc_xcr_save = ssc_readx(params->ssc->regs, params->pdc->xcr);
		prtd->pdc_xnpr_save = ssc_readx(params->ssc->regs, params->pdc->xnpr);
		prtd->pdc_xncr_save = ssc_readx(params->ssc->regs, params->pdc->xncr);
	}

	return 0;
}

static int atmel_pcm_resume(struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = dai->runtime;
	struct atmel_runtime_data *prtd;
	struct atmel_pcm_dma_params *params;

	if (!runtime)
		return 0;

	prtd = runtime->private_data;
	params = prtd->params;

	if (!ssc_use_dmaengine()) {
		/* restore the PDC registers and enable the PDC */
		ssc_writex(params->ssc->regs, params->pdc->xpr, prtd->pdc_xpr_save);
		ssc_writex(params->ssc->regs, params->pdc->xcr, prtd->pdc_xcr_save);
		ssc_writex(params->ssc->regs, params->pdc->xnpr, prtd->pdc_xnpr_save);
		ssc_writex(params->ssc->regs, params->pdc->xncr, prtd->pdc_xncr_save);

		ssc_writel(params->ssc->regs, PDC_PTCR, params->mask->pdc_enable);
	}
	return 0;
}
#else
#define atmel_pcm_suspend	NULL
#define atmel_pcm_resume	NULL
#endif

static struct snd_soc_platform_driver atmel_soc_platform = {
	.ops		= &atmel_pcm_ops,
	.pcm_new	= atmel_pcm_new,
	.pcm_free	= atmel_pcm_free_dma_buffers,
	.suspend	= atmel_pcm_suspend,
	.resume		= atmel_pcm_resume,
};

static int __devinit atmel_soc_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &atmel_soc_platform);
}

static int __devexit atmel_soc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver atmel_pcm_driver = {
	.driver = {
			.name = "atmel-pcm-audio",
			.owner = THIS_MODULE,
	},

	.probe = atmel_soc_platform_probe,
	.remove = __devexit_p(atmel_soc_platform_remove),
};

static int __init snd_atmel_pcm_init(void)
{
	if (ssc_use_dmaengine()) {
		atmel_pcm_ops.prepare = atmel_pcm_dma_prepare;
		atmel_pcm_ops.trigger = atmel_pcm_dma_trigger;
		atmel_pcm_ops.pointer = atmel_pcm_dma_pointer;

		atmel_pcm_hardware = &atmel_pcm_dma_hardware;
	} else {
		atmel_pcm_hardware = &atmel_pcm_pdc_hardware;
	}
	return platform_driver_register(&atmel_pcm_driver);
}
module_init(snd_atmel_pcm_init);

static void __exit snd_atmel_pcm_exit(void)
{
	platform_driver_unregister(&atmel_pcm_driver);
}
module_exit(snd_atmel_pcm_exit);

MODULE_AUTHOR("Sedji Gaouaou <sedji.gaouaou@atmel.com>");
MODULE_DESCRIPTION("Atmel PCM module");
MODULE_LICENSE("GPL");

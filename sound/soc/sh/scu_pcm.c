/*
 * sound/soc/sh/scu_pcm.c
 *     This file is ALSA SoC driver for SCU peripheral.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This file is based on the sound/soc/sh/siu_pcm.c
 *
 * siu_pcm.c - ALSA driver for Renesas SH7343, SH7722 SIU peripheral.
 *
 * Copyright (C) 2009-2010 Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 * Copyright (C) 2006 Carlos Munoz <carlos@kenati.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/sh_scu.h>

#undef DEBUG
#ifdef DEBUG
#define FNC_ENTRY	pr_info("entry:%s:%d\n", __func__, __LINE__);
#define FNC_EXIT	pr_info("exit:%s:%d\n", __func__, __LINE__);
#define DBG_POINT()	pr_info("check:%s:%d\n", __func__, __LINE__);
#define DBG_MSG(args...)	pr_info(args)
#else  /* DEBUG */
#define FNC_ENTRY
#define FNC_EXIT
#define DBG_POINT()
#define DBG_MSG(args...)
#endif /* DEBUG */

static u64 dma_mask = DMA_BIT_MASK(32);

static struct snd_soc_dai *scu_get_dai(struct snd_pcm_substream *ss)
{
	struct snd_soc_pcm_runtime *rtd = ss->private_data;

	return  rtd->cpu_dai;
}

static void scu_dma_callback(struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;
	struct snd_soc_dai *dai = scu_get_dai(ss);
	struct snd_pcm_runtime *runtime = ss->runtime;
	int dir = ss->stream == SNDRV_PCM_STREAM_CAPTURE;
	int buf_pos;
	u32 dma_size;
	u32 dma_paddr;

	FNC_ENTRY

	buf_pos = pcminfo->period & (SCU_PERIODS_MAX - 1);
	dma_size = frames_to_bytes(runtime, runtime->period_size);
	dma_paddr = runtime->dma_addr + (buf_pos * dma_size);
	dma_sync_single_for_cpu(dai->dev, dma_paddr, dma_size, DMA_DIR(dir));

	pcminfo->tran_period++;

	/* Notify alsa: a period is done */
	snd_pcm_period_elapsed(ss);

	/* stop dma */
	if (pcminfo->flag_start == 0)
		return;

	schedule_work(&pcminfo->work);

	FNC_EXIT
}

static bool filter_audma(struct dma_chan *chan, void *slave)
{
	struct shdma_slave *param = slave;
	struct platform_device *pdev = to_platform_device(chan->device->dev);

	DBG_MSG("%s: pdev->id=%d, slave_id=%d\n",
				__func__, pdev->id, param->slave_id);

	if ((pdev->id != SHDMA_DEVID_AUDIO_LO) &&
	    (pdev->id != SHDMA_DEVID_AUDIO_UP))
		return false;

	chan->private = param;
	return true;
}

static bool filter_audmapp(struct dma_chan *chan, void *slave)
{
	struct shdma_slave *param = slave;
	struct platform_device *pdev = to_platform_device(chan->device->dev);

	DBG_MSG("%s: pdev->id=%d, slave_id=%d\n",
				__func__, pdev->id, param->slave_id);

	if (pdev->id != SHDMA_DEVID_AUDIOPP)
		return false;

	chan->private = param;
	return true;
}

static int scu_dmae_req_chan(int sid, int did, struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;
	struct sh_dmadesc_slave *param = &pcminfo->de_param[sid];
	dma_cap_mask_t mask;
	int ret = 0;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	DBG_MSG("sid=%d, did=%d\n", sid, did);

	FNC_ENTRY
	/* set dma slave id */
	param->shdma_slave.slave_id = sid;

	/* request dma channel */
	if (pcminfo->de_chan[sid] == NULL) {
		if (did == SHDMA_DEVID_AUDIO)
			pcminfo->de_chan[sid] = dma_request_channel(mask,
							filter_audma, param);
		else /* did == SHDMA_DEVID_AUDIOPP */
			pcminfo->de_chan[sid] = dma_request_channel(mask,
							filter_audmapp, param);
		if (!pcminfo->de_chan[sid]) {
			printk(KERN_ERR "DMA channel request error\n");
			ret = -EBUSY;
		}
	}

	DBG_MSG("chan=0x%08x\n", (int)pcminfo->de_chan[sid]);

	FNC_EXIT
	return ret;
}

static void scu_dmae_rel_chan(int sid, struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;

	FNC_ENTRY

	/* release dma channel */
	if (pcminfo->de_chan[sid]) {
		dma_release_channel(pcminfo->de_chan[sid]);
		pcminfo->de_chan[sid] = NULL;
	}

	FNC_EXIT
	return;
}

static int scu_dmae_request(struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;
	int dir = ss->stream == SNDRV_PCM_STREAM_CAPTURE;
	int ret = 0;

	FNC_ENTRY
	if (!dir) { /* playback */
		/* ssi0 */
		if (pcminfo->routeinfo->pcb.init_ssi) {
			/* dma channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_MEM_SSI0,
						SHDMA_DEVID_AUDIO, ss);
		}

		/* ssi0 via src0 */
		if (pcminfo->routeinfo->pcb.init_ssi_src &&
		    pcminfo->routeinfo->pcb.init_src) {
			/* dma(mem->src) channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_MEM_SRC0,
						SHDMA_DEVID_AUDIO, ss);

			/* dma(src->ssi) channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_SRC0_SSI0,
						SHDMA_DEVID_AUDIOPP, ss);
		}

		/* ssi0 via src0,dvc0 */
		if (pcminfo->routeinfo->pcb.init_ssi_dvc &&
		    pcminfo->routeinfo->pcb.init_src &&
		    pcminfo->routeinfo->pcb.init_dvc) {
			/* dma(mem->src) channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_MEM_SRC0,
						SHDMA_DEVID_AUDIO, ss);

			/* dma(cmd->ssi) channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_CMD0_SSI0,
						SHDMA_DEVID_AUDIOPP, ss);
		}
	} else { /* capture */
		/* ssi1 */
		if (pcminfo->routeinfo->ccb.init_ssi) {
			/* dma channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_SSI1_MEM,
						SHDMA_DEVID_AUDIO, ss);
		}

		/* ssi1 via src1 */
		if (pcminfo->routeinfo->ccb.init_ssi_src &&
		    pcminfo->routeinfo->ccb.init_src) {
			/* dma(src->mem) channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_SRC1_MEM,
						SHDMA_DEVID_AUDIO, ss);

			/* dma(ssi->src) channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_SSI1_SRC1,
						SHDMA_DEVID_AUDIOPP, ss);
		}

		/* ssi1 via src1,dvc1 */
		if (pcminfo->routeinfo->ccb.init_ssi_dvc &&
		    pcminfo->routeinfo->ccb.init_src_dvc &&
		    pcminfo->routeinfo->ccb.init_dvc) {
			/* dma(cmd->mem) channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_CMD1_MEM,
						SHDMA_DEVID_AUDIO, ss);

			/* dma(ssi->src) channel allocation */
			ret = scu_dmae_req_chan(SHDMA_SLAVE_PCM_SSI1_SRC1,
						SHDMA_DEVID_AUDIOPP, ss);
		}
	}

	FNC_EXIT
	return ret;
}

static int scu_dmae_release(struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;
	int dir = ss->stream == SNDRV_PCM_STREAM_CAPTURE;
	int ret = 0;

	FNC_ENTRY
	if (!dir) { /* playback */
		/* ssi */
		if (pcminfo->routeinfo->pcb.init_ssi)
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_MEM_SSI0, ss);

		/* ssi via src */
		if (pcminfo->routeinfo->pcb.init_ssi_src &&
		    pcminfo->routeinfo->pcb.init_src) {
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_MEM_SRC0, ss);
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_SRC0_SSI0, ss);
		}

		/* ssi via src/dvc */
		if (pcminfo->routeinfo->pcb.init_ssi_dvc &&
		    pcminfo->routeinfo->pcb.init_src &&
		    pcminfo->routeinfo->pcb.init_dvc) {
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_MEM_SRC0, ss);
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_CMD0_SSI0, ss);
		}
	} else { /* capture */
		/* ssi */
		if (pcminfo->routeinfo->ccb.init_ssi)
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_SSI1_MEM, ss);

		/* ssi via src */
		if (pcminfo->routeinfo->ccb.init_ssi_src &&
		    pcminfo->routeinfo->ccb.init_src) {
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_SRC1_MEM, ss);
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_SSI1_SRC1, ss);
		}

		/* ssi via src/dvc */
		if (pcminfo->routeinfo->ccb.init_ssi_dvc &&
		    pcminfo->routeinfo->ccb.init_src_dvc &&
		    pcminfo->routeinfo->ccb.init_dvc) {
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_CMD1_MEM, ss);
			scu_dmae_rel_chan(SHDMA_SLAVE_PCM_SSI1_SRC1, ss);
		}
	}

	FNC_EXIT
	return ret;
}

static int scu_audma_start(int sid, struct snd_pcm_substream *ss)
{
	int dir = ss->stream == SNDRV_PCM_STREAM_CAPTURE;
	int buf_pos;
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct scu_pcm_info *pcminfo = runtime->private_data;
	struct device *dev = ss->pcm->card->dev;
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;
	u32 dma_size;
	u32 dma_paddr;
	struct snd_soc_dai *dai;

	FNC_ENTRY
	dai = scu_get_dai(ss);

	/* buffer control */
	buf_pos = pcminfo->period & (SCU_PERIODS_MAX - 1);
	DBG_MSG("buf_pos=%d\n", buf_pos);

	/* DMA size */
	dma_size = frames_to_bytes(runtime, runtime->period_size);
	DBG_MSG("dma_size=%d\n", dma_size);

	/* DMA physical adddress */
	dma_paddr = runtime->dma_addr + (buf_pos * dma_size);
	DBG_MSG("dma_paddr=0x%08x\n", dma_paddr);

	dma_sync_single_for_device(dai->dev, dma_paddr, dma_size, DMA_DIR(dir));

	desc = dmaengine_prep_slave_single(pcminfo->de_chan[sid], dma_paddr,
		dma_size, DMA_DIR(dir), DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(dai->dev, "dmaengine_prep_slave_sg_single() fail\n");
		return -ENOMEM;
	}

	desc->callback = (dma_async_tx_callback)scu_dma_callback;
	desc->callback_param = ss;

	cookie = dmaengine_submit(desc);
	if (cookie < 0) {
		dev_err(dev, "Failed to submit a dma transfer\n");
		FNC_EXIT
		return cookie;
	}

	dma_async_issue_pending(pcminfo->de_chan[sid]);

	/* Update period */
	pcminfo->period++;

	FNC_EXIT
	return 0;
}

static int scu_audma_stop(int sid, struct snd_pcm_substream *ss)
{
	FNC_ENTRY
	FNC_EXIT
	return 0;
}

static void scu_pcm_start(struct snd_pcm_substream *ss, int first_flag)
{
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;
	int dir = ss->stream == SNDRV_PCM_STREAM_CAPTURE;

	FNC_ENTRY
	if (!dir) { /* playback */
		/* ssi */
		if (pcminfo->routeinfo->pcb.init_ssi) {
			/* start dma */
			scu_audma_start(SHDMA_SLAVE_PCM_MEM_SSI0, ss);

			if (first_flag) {
				/* start ssi */
				pcminfo->routeinfo->pcb.init_ssi();
			}
		}

		/* ssi via src */
		if (pcminfo->routeinfo->pcb.init_ssi_src &&
		    pcminfo->routeinfo->pcb.init_src) {
			/* start dma */
			scu_audma_start(SHDMA_SLAVE_PCM_MEM_SRC0, ss);

			if (first_flag) {
				/* start ssi */
				pcminfo->routeinfo->pcb.init_ssi_src();

				/* start src */
				pcminfo->routeinfo->pcb.init_src(
					ss->runtime->rate);
			}
		}

		/* ssi via src/dvc */
		if (pcminfo->routeinfo->pcb.init_ssi_dvc &&
		    pcminfo->routeinfo->pcb.init_src &&
		    pcminfo->routeinfo->pcb.init_dvc) {
			/* start dma */
			scu_audma_start(SHDMA_SLAVE_PCM_MEM_SRC0, ss);

			if (first_flag) {
				/* start ssi */
				pcminfo->routeinfo->pcb.init_ssi_dvc();

				/* start dvc */
				pcminfo->routeinfo->pcb.init_dvc();

				/* start src */
				pcminfo->routeinfo->pcb.init_src(
					ss->runtime->rate);
			}
		}
	} else { /* capture */
		/* ssi */
		if (pcminfo->routeinfo->ccb.init_ssi) {
			/* start dma */
			scu_audma_start(SHDMA_SLAVE_PCM_SSI1_MEM, ss);

			if (first_flag) {
				/* start ssi */
				pcminfo->routeinfo->ccb.init_ssi();
			}
		}

		/* ssi via src */
		if (pcminfo->routeinfo->ccb.init_ssi_src &&
		    pcminfo->routeinfo->ccb.init_src) {
			/* start dma */
			scu_audma_start(SHDMA_SLAVE_PCM_SRC1_MEM, ss);

			if (first_flag) {
				/* start ssi */
				pcminfo->routeinfo->ccb.init_ssi_src();

				/* start src */
				pcminfo->routeinfo->ccb.init_src(
					ss->runtime->rate);
			}
		}

		/* ssi via src/dvc */
		if (pcminfo->routeinfo->ccb.init_ssi_dvc &&
		    pcminfo->routeinfo->ccb.init_src_dvc &&
		    pcminfo->routeinfo->ccb.init_dvc) {
			/* start dma */
			scu_audma_start(SHDMA_SLAVE_PCM_CMD1_MEM, ss);

			if (first_flag) {
				/* start ssi */
				pcminfo->routeinfo->ccb.init_ssi_dvc();

				/* start dvc */
				pcminfo->routeinfo->ccb.init_dvc();

				/* start src */
				pcminfo->routeinfo->ccb.init_src_dvc(
					ss->runtime->rate);
			}
		}
	}

	FNC_EXIT
	return;
}

static void scu_pcm_stop(struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;
	int dir = ss->stream == SNDRV_PCM_STREAM_CAPTURE;

	FNC_ENTRY
	if (!dir) { /* playback */
		/* ssi */
		if (pcminfo->routeinfo->pcb.deinit_ssi) {
			DBG_MSG("post:ssi\n");
			/* stop ssi */
			pcminfo->routeinfo->pcb.deinit_ssi();
			/* stop dma */
			scu_audma_stop(SHDMA_SLAVE_PCM_MEM_SSI0, ss);
		}

		/* ssi via src */
		if (pcminfo->routeinfo->pcb.deinit_ssi_src &&
		    pcminfo->routeinfo->pcb.deinit_src) {
			DBG_MSG("post:src->ssi\n");
			/* stop src */
			pcminfo->routeinfo->pcb.deinit_src();
			/* stop ssi */
			pcminfo->routeinfo->pcb.deinit_ssi_src();
			/* stop dma */
			scu_audma_stop(SHDMA_SLAVE_PCM_MEM_SRC0, ss);
		}

		/* ssi via src/dvc */
		if (pcminfo->routeinfo->pcb.deinit_ssi_dvc &&
		    pcminfo->routeinfo->pcb.deinit_src &&
		    pcminfo->routeinfo->pcb.deinit_dvc) {
			/* stop src */
			pcminfo->routeinfo->pcb.deinit_src();
			/* stop dvc */
			pcminfo->routeinfo->pcb.deinit_dvc();
			/* stop ssi */
			pcminfo->routeinfo->pcb.deinit_ssi_dvc();
			/* stop dma */
			scu_audma_stop(SHDMA_SLAVE_PCM_MEM_SRC0, ss);
		}
	} else { /* capture */
		/* ssi */
		if (pcminfo->routeinfo->ccb.deinit_ssi) {
			DBG_MSG("post:ssi\n");
			/* stop ssi */
			pcminfo->routeinfo->ccb.deinit_ssi();
			/* stop dma */
			scu_audma_stop(SHDMA_SLAVE_PCM_SSI1_MEM, ss);
		}

		/* ssi via src */
		if (pcminfo->routeinfo->ccb.deinit_ssi_src &&
		    pcminfo->routeinfo->ccb.deinit_src) {
			DBG_MSG("post:src->ssi\n");
			/* stop src */
			pcminfo->routeinfo->ccb.deinit_src();
			/* stop ssi */
			pcminfo->routeinfo->ccb.deinit_ssi_src();
			/* stop dma */
			scu_audma_stop(SHDMA_SLAVE_PCM_SRC1_MEM, ss);
		}

		/* ssi via src/dvc */
		if (pcminfo->routeinfo->ccb.deinit_ssi_dvc &&
		    pcminfo->routeinfo->ccb.deinit_src_dvc &&
		    pcminfo->routeinfo->ccb.deinit_dvc) {
			/* start src */
			pcminfo->routeinfo->ccb.deinit_src_dvc();
			/* start dvc */
			pcminfo->routeinfo->ccb.deinit_dvc();
			/* start ssi */
			pcminfo->routeinfo->ccb.deinit_ssi_dvc();
			/* start dma */
			scu_audma_stop(SHDMA_SLAVE_PCM_CMD1_MEM, ss);
		}
	}

	FNC_EXIT
	return;
}

static void scu_dma_do_work(struct work_struct *work)
{
	struct scu_pcm_info *pcminfo =
			container_of(work, struct scu_pcm_info, work);
	struct snd_pcm_substream *ss = pcminfo->ss;

	FNC_ENTRY
	/* start pcm process */
	scu_pcm_start(ss, pcminfo->flag_first);
	if (pcminfo->flag_first == 1)
		pcminfo->flag_first = 0;

	FNC_EXIT
	return;
}

static int scu_audio_start(struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;
	int ret = 0;

	FNC_ENTRY
	/* dma channel request */
	ret = scu_dmae_request(ss);
	if (ret < 0) {
		pr_info("scu_dmae_request faild\n");
		FNC_EXIT
		return ret;
	}

	/* DMA control */
	pcminfo->flag_start = 1;
	/* PCM 1st process */
	pcminfo->flag_first = 1;

	schedule_work(&pcminfo->work);

	FNC_EXIT
	return ret;
}

static int scu_audio_stop(struct snd_pcm_substream *ss)
{
	int ret = 0;
	struct scu_pcm_info *pcminfo = ss->runtime->private_data;

	FNC_ENTRY
	/* stop dma */
	pcminfo->flag_start = 0;

	/* stop pcm process */
	scu_pcm_stop(ss);

	/* dma channel release */
	ret = scu_dmae_release(ss);

	FNC_EXIT
	return ret;
}

static struct scu_pcm_info *scu_pcm_new_stream(struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo;
	int i;

	FNC_ENTRY
	/* allocate scu_pcm_info structure */
	pcminfo = kzalloc(sizeof(struct scu_pcm_info), GFP_KERNEL);
	if (!pcminfo)
		return pcminfo;

	/* initialize rcar_pcm_info structure */
	pcminfo->period      = 0;
	pcminfo->tran_period = 0;
	pcminfo->routeinfo   = scu_get_route_info();
	pcminfo->ss          = ss;
	for (i = 0; i < SHDMA_SLAVE_PCM_MAX; i++)
		pcminfo->de_chan[i] = NULL;

	spin_lock_init(&pcminfo->pcm_lock);

	INIT_WORK(&pcminfo->work, scu_dma_do_work);
	FNC_EXIT
	return pcminfo;
}

static void scu_pcm_free_stream(struct snd_pcm_runtime *runtime)
{
	struct scu_pcm_info *pcminfo = runtime->private_data;

	FNC_ENTRY

	/* post process */
	cancel_work_sync(&pcminfo->work);
	kfree(runtime->private_data);	/* free pcminfo structure */

	FNC_EXIT
	return;
}

static int scu_pcm_open(struct snd_pcm_substream *ss)
{
	struct scu_pcm_info *pcminfo;
	int dir = ss->stream == SNDRV_PCM_STREAM_CAPTURE;
	int ret = 0;

	FNC_ENTRY
	pcminfo = scu_pcm_new_stream(ss);
	if (pcminfo == NULL)
		return -ENOMEM;

	ret = scu_check_route(dir, pcminfo->routeinfo);
	if (ret < 0)
		return ret;

	ss->runtime->private_data = pcminfo;
	ss->runtime->private_free = scu_pcm_free_stream;

	FNC_EXIT
	return 0;
}

static int scu_pcm_close(struct snd_pcm_substream *ss)
{
	FNC_ENTRY
	FNC_EXIT
	return 0;
}

static int scu_pcm_hw_params(struct snd_pcm_substream *ss,
			     struct snd_pcm_hw_params *hw_params)
{
	struct device *dev = ss->pcm->card->dev;
	int ret;

	FNC_ENTRY
	ret = snd_pcm_lib_malloc_pages(ss, params_buffer_bytes(hw_params));
	if (ret < 0)
		dev_err(dev, "snd_pcm_lib_malloc_pages() failed\n");

	FNC_EXIT
	return ret;
}

static int scu_pcm_hw_free(struct snd_pcm_substream *ss)
{
	struct device *dev = ss->pcm->card->dev;
	int ret;

	FNC_ENTRY
	ret = snd_pcm_lib_free_pages(ss);
	if (ret < 0)
		dev_err(dev, "snd_pcm_lib_free_pages() failed\n");

	FNC_EXIT
	return ret;
}

static int scu_pcm_prepare(struct snd_pcm_substream *ss)
{
	FNC_ENTRY
	FNC_EXIT
	return 0;
}

static int scu_pcm_trigger(struct snd_pcm_substream *ss, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct scu_pcm_info *pcminfo = runtime->private_data;

	spin_lock(&pcminfo->pcm_lock);

	FNC_ENTRY
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		ret = scu_audio_start(ss);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		ret = scu_audio_stop(ss);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&pcminfo->pcm_lock);

	FNC_EXIT
	return ret;
}

static snd_pcm_uframes_t scu_pcm_pointer_dma(struct snd_pcm_substream *ss)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct scu_pcm_info *pcminfo = runtime->private_data;
	snd_pcm_uframes_t position = 0;

	position = runtime->period_size *
			(pcminfo->tran_period & (SCU_PERIODS_MAX - 1));

	DBG_MSG("\tposition = %d\n", (u32)position);

	return position;
}

static int scu_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_card *card = rtd->card->snd_card;

	FNC_ENTRY
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &dma_mask;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = snd_pcm_lib_preallocate_pages_for_all(
		rtd->pcm,
		SNDRV_DMA_TYPE_DEV,
		rtd->card->snd_card->dev,
		SCU_BUFFER_BYTES_MAX, SCU_BUFFER_BYTES_MAX);

	FNC_EXIT
	return ret;
}

static void scu_pcm_free(struct snd_pcm *pcm)
{
	FNC_ENTRY

	/* free dma buffer */
	snd_pcm_lib_preallocate_free_for_all(pcm);

	FNC_EXIT
}

static struct snd_pcm_ops scu_pcm_ops = {
	.open		= scu_pcm_open,
	.close		= scu_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= scu_pcm_hw_params,
	.hw_free	= scu_pcm_hw_free,
	.prepare	= scu_pcm_prepare,
	.trigger	= scu_pcm_trigger,
	.pointer	= scu_pcm_pointer_dma,
};

struct snd_soc_platform_driver scu_platform = {
	.ops		= &scu_pcm_ops,
	.pcm_new	= scu_pcm_new,
	.pcm_free	= scu_pcm_free,
};
EXPORT_SYMBOL_GPL(scu_platform);

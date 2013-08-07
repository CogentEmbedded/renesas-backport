/*
 * drivers/dma/sh/audma-pp.h
 *     This file is header file for Audio-DMAC-pp peripheral.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This file is based on the drivers/dma/sh/shdma.h
 *
 * Renesas SuperH DMA Engine support
 *
 * Copyright (C) 2009 Nobuhiro Iwamatsu <iwamatsu.nobuhiro@renesas.com>
 * Copyright (C) 2009 Renesas Solutions, Inc. All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef __DMA_AUDMAPP_H
#define __DMA_AUDMAPP_H

#include <linux/sh_audma-pp.h>
#include <linux/shdmapp-base.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/list.h>

#define SH_AUDMAPP_MAX_CHANNELS 29

struct device;

struct sh_audmapp_chan {
	struct shdma_chan shdma_chan;
	const struct sh_audmapp_slave_config *config;
	u32 __iomem *base;
	char dev_id[16];		/* unique name per DMAC of channel */
};

struct sh_audmapp_device {
	struct shdma_dev shdma_dev;
	struct sh_audmapp_chan *chan[SH_AUDMAPP_MAX_CHANNELS];
	struct sh_audmapp_pdata *pdata;
	struct list_head node;
	u32 __iomem *chan_reg;
};

struct sh_audmapp_regs {
	u32 sar;	/* SAR  / source address */
	u32 dar;	/* DAR  / destination address */
	u32 chcr;	/* CHCR / channel control */
};

struct sh_audmapp_desc {
	struct sh_audmapp_regs hw;
	struct shdma_desc shdma_desc;
};

#define to_sh_chan(chan) container_of(chan, struct sh_audmapp_chan, shdma_chan)
#define to_sh_desc(lh) container_of(lh, struct sh_desc, node)
#define tx_to_sh_desc(tx) container_of(tx, struct sh_desc, async_tx)
#define to_sh_dev(chan) container_of(chan->shdma_chan.dma_chan.device,\
				struct sh_audmapp_device, shdma_dev.dma_dev)

#endif	/* __DMA_AUDMAPP_H */

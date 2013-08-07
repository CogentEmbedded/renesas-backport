/*
 * drivers/dma/sh/shdma-desc.h
 *     This file is header file for SYS-DMAC/Audio-DMAC peripheral.
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
#ifndef __DMA_SHDMADESC_H
#define __DMA_SHDMADESC_H

#include <linux/sh_dma-desc.h>
#include <linux/shdma-base.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/list.h>

#define SHDESC_DMAE_MAX_CHANNELS 15	/* Max channel of SYS-DMAC */
#define SHDESC_DMAE_TCR_MAX 0x00FFFFFF	/* 16MB */

struct device;

struct sh_dmadesc_chan {
	struct shdma_chan shdma_chan;
	const struct sh_dmadesc_slave_config *config;
	int xmit_shift;			/* log_2(bytes_per_xfer) */
	u32 __iomem *base;
	char dev_id[16];		/* unique name per DMAC of channel */
	int pm_error;
	u32 __iomem *descmem_start;
	u32 __iomem *descmem_end;
	u32 *descmem_ptr;
};

struct sh_dmadesc_device {
	struct shdma_dev shdma_dev;
	struct sh_dmadesc_chan *chan[SHDESC_DMAE_MAX_CHANNELS];
	struct sh_dmadesc_pdata *pdata;
	struct list_head node;
	u32 __iomem *chan_reg;
	u16 __iomem *dmars;
	unsigned int chcr_offset;
	u32 chcr_ie_bit;
};

struct sh_dmadesc_regs {
	u32 sar; /* SAR / source address */
	u32 dar; /* DAR / destination address */
	u32 tcr; /* TCR / transfer count */
};

struct sh_dmadesc_desc {
	struct sh_dmadesc_regs hw;
	struct shdma_desc shdma_desc;
};

#define to_sh_chan(chan) container_of(chan, struct sh_dmadesc_chan, shdma_chan)
#define to_sh_desc(lh) container_of(lh, struct sh_desc, node)
#define tx_to_sh_desc(tx) container_of(tx, struct sh_desc, async_tx)
#define to_sh_dev(chan) container_of(chan->shdma_chan.dma_chan.device,\
				struct sh_dmadesc_device, shdma_dev.dma_dev)

/* DMA register */
/*  channel offset  */
#define SAR	0x00
#define DAR	0x04
#define TCR	0x08
#define CHCR	0x0C
#define CHCRB	0x1C
#define DPBASE	0x50
/*  base offset  */
#define DMAOR	0x60
#define DMACLR	0x80

#define TEND	0x18 /* USB-DMAC */

/* DMA descriptor memory */
#define DESCMEM_BASE	0xa000
#define DESCMEM_SIZE	0x800
#define DESC_STEP_SIZE	0x10

/* DMAOR definitions */
#define DMAOR_AE	0x00000004
#define DMAOR_DME	0x00000001

/* CHCR definitions */
#define DPM_MSK	0x30000000
#define RPT_SRC	0x08000000
#define RPT_DST	0x04000000
#define RPT_TC	0x02000000
#define DPB	0x00400000
#define DSE	0x00080000
#define DSIE	0x00040000
#define DM_INC	0x00004000
#define DM_DEC	0x00008000
#define DM_FIX	0x0000c000
#define SM_INC	0x00001000
#define SM_DEC	0x00002000
#define SM_FIX	0x00003000
#define RS_IN	0x00000200
#define RS_OUT	0x00000300
#define TS_BLK	0x00000040
#define TM_BUR	0x00000020
#define CHCR_DE	0x00000001
#define CHCR_TE	0x00000002
#define CHCR_IE	0x00000004

#endif	/* __DMA_SHDMADESC_H */

/*
 * include/linux/sh_dma-desc.h
 *     This file is header file for SYS-DMAC/Audio-DMAC peripheral.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This file is based on the include/linux/sh_dma.h
 *
 * Header for the new SH dmaengine driver
 *
 * Copyright (C) 2010 Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef SH_DMA_DESC_H
#define SH_DMA_DESC_H

#include <linux/dmaengine.h>
#include <linux/list.h>
#include <linux/shdma-base.h>
#include <linux/types.h>

struct device;

/* Used by slave DMA clients to request DMA to/from a specific peripheral */
struct sh_dmadesc_slave {
	struct shdma_slave	shdma_slave;	/* Set by the platform */
};

/*
 * Supplied by platforms to specify, how a DMA channel has to be configured for
 * a certain peripheral
 */
struct sh_dmadesc_slave_config {
	int		slave_id;
	dma_addr_t	addr;
	u32		chcr;
	char		mid_rid;
	char		desc_mode:2;
	u16		desc_offset;
	u16		desc_stepnum;
};

struct sh_dmadesc_channel {
	unsigned int	offset;
	unsigned int	dmars;
	unsigned int	dmars_bit;
	unsigned int	chclr_offset;
};

struct sh_dmadesc_pdata {
	const struct sh_dmadesc_slave_config *slave;
	int slave_num;
	const struct sh_dmadesc_channel *channel;
	int channel_num;
	unsigned int ts_low_shift;
	unsigned int ts_low_mask;
	unsigned int ts_high_shift;
	unsigned int ts_high_mask;
	const unsigned int *ts_shift;
	int ts_shift_num;
	u16 dmaor_init;
	unsigned int chcr_offset;
	u32 chcr_ie_bit;

	unsigned int dmaor_is_32bit:1;
	unsigned int needs_tend_set:1;
	unsigned int no_dmars:1;
	unsigned int chclr_present:1;
	unsigned int slave_only:1;

	bool (*dma_filter)(struct platform_device *pdev);
	struct clk *(*clk_get)(struct platform_device *pdev);
};

#endif	/* SH_DMA_DESC_H */

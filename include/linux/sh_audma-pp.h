/*
 * include/linux/sh_audma-pp.h
 *     This file is header file for Audio-DMAC-pp peripheral.
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
#ifndef SH_AUDMAPP_H
#define SH_AUDMAPP_H

#include <linux/dmaengine.h>
#include <linux/list.h>
#include <linux/shdma-base.h>
#include <linux/types.h>

struct device;

/* Used by slave DMA clients to request DMA to/from a specific peripheral */
struct sh_audmapp_slave {
	struct shdma_slave	shdma_slave;	/* Set by the platform */
};

struct sh_audmapp_slave_config {
	int		slave_id;
	dma_addr_t	sar;
	dma_addr_t	dar;
	u32		chcr;
};

struct sh_audmapp_channel {
	unsigned int	offset;
};

struct sh_audmapp_pdata {
	const struct sh_audmapp_slave_config *slave;
	int slave_num;
	const struct sh_audmapp_channel *channel;
	int channel_num;
};

/* DMA register */
#define PDMASAR		0x00
#define PDMADAR		0x04
#define PDMACHCR	0x0c

/* PDMACHCR definitions */
#define PDMACHCR_DE		(1<<0)

#endif /* SH_AUDMAPP_H */

/*
 * include/linux/shdmapp-base.h
 *     This file is header file for DMA Engine driver base library of
 *     DMAC that is transferred between peripheral and peripheral.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This file is based on the include/linux/shdma-base.h
 *
 * Dmaengine driver base library for DMA controllers, found on SH-based SoCs
 *
 * extracted from shdma.c and headers
 *
 * Copyright (C) 2011-2012 Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 * Copyright (C) 2009 Nobuhiro Iwamatsu <iwamatsu.nobuhiro@renesas.com>
 * Copyright (C) 2009 Renesas Solutions, Inc. All rights reserved.
 * Copyright (C) 2007 Freescale Semiconductor, Inc. All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */

#ifndef SHDMAPP_BASE_H
#define SHDMAPP_BASE_H

#include <linux/shdma-base.h>

void shdmapp_chan_probe(struct shdma_dev *sdev,
			   struct shdma_chan *schan, int id);
void shdmapp_chan_remove(struct shdma_chan *schan);
int shdmapp_init(struct device *dev, struct shdma_dev *sdev, int chan_num);
void shdmapp_cleanup(struct shdma_dev *sdev);

#endif /* SHDMAPP_BASE_H */

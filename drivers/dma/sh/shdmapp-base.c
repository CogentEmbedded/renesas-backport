/*
 * drivers/dma/sh/shdmapp-base.c
 *     This file is DMA Engine driver base library for DMAC that is
 *     transferred between peripheral and peripheral.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This file is based on the drivers/dma/sh/shdma.c
 *
 * Dmaengine driver base library for DMA controllers, found on SH-based SoCs
 *
 * extracted from shdma.c
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

#include <linux/delay.h>
#include <linux/shdma-base.h>
#include <linux/dmaengine.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "../dmaengine.h"

#define to_shdma_chan(c) container_of(c, struct shdma_chan, dma_chan)
#define to_shdma_dev(d) container_of(d, struct shdma_dev, dma_dev)

/*
 * For slave DMA we assume, that there is a finite number of DMA slaves in the
 * system, and that each such slave can only use a finite number of channels.
 * We use slave channel IDs to make sure, that no such slave channel ID is
 * allocated more than once.
 */
static unsigned int slave_num = 256;

/* A bitmask with slave_num bits */
static unsigned long *shdma_slave_used;

static int shdmapp_setup_slave(struct shdma_chan *schan, int slave_id)
{
	struct shdma_dev *sdev = to_shdma_dev(schan->dma_chan.device);
	const struct shdma_ops *ops = sdev->ops;
	int ret;

	if (slave_id < 0 || slave_id >= slave_num)
		return -EINVAL;

	if (test_and_set_bit(slave_id, shdma_slave_used))
		return -EBUSY;

	ret = ops->set_slave(schan, slave_id, false);
	if (ret < 0) {
		clear_bit(slave_id, shdma_slave_used);
		return ret;
	}

	schan->slave_id = slave_id;

	return 0;
}

static int shdmapp_alloc_chan_resources(struct dma_chan *chan)
{
	struct shdma_chan *schan = to_shdma_chan(chan);
	struct shdma_slave *slave = chan->private;
	int ret;

	/*
	 * This relies on the guarantee from dmaengine that alloc_chan_resources
	 * never runs concurrently with itself or free_chan_resources.
	 */
	if (slave) {
		/* Legacy mode: .private is set in filter */
		ret = shdmapp_setup_slave(schan, slave->slave_id);
		if (ret < 0)
			goto esetslave;
	} else {
		schan->slave_id = -EINVAL;
	}

	return 0;

esetslave:
	clear_bit(slave->slave_id, shdma_slave_used);
	chan->private = NULL;
	return ret;
}

static void shdmapp_free_chan_resources(struct dma_chan *chan)
{
	struct shdma_chan *schan = to_shdma_chan(chan);
	struct shdma_dev *sdev = to_shdma_dev(chan->device);
	const struct shdma_ops *ops = sdev->ops;
	LIST_HEAD(list);

	/* Protect against ISR */
	spin_lock_irq(&schan->chan_lock);
	ops->halt_channel(schan);
	spin_unlock_irq(&schan->chan_lock);

	if (schan->slave_id >= 0) {
		/* The caller is holding dma_list_mutex */
		clear_bit(schan->slave_id, shdma_slave_used);
		chan->private = NULL;
	}

	spin_lock_irq(&schan->chan_lock);

	list_splice_init(&schan->ld_free, &list);
	schan->desc_num = 0;

	spin_unlock_irq(&schan->chan_lock);

	kfree(schan->desc);
}

static struct dma_async_tx_descriptor *shdmapp_prep_slave_sg(
	struct dma_chan *chan, struct scatterlist *sgl, unsigned int sg_len,
	enum dma_transfer_direction direction, unsigned long flags,
	void *context)
{
	return NULL;
}

static int shdmapp_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
			  unsigned long arg)
{
	return 0;
}

static void shdmapp_issue_pending(struct dma_chan *chan)
{
	return;
}

static enum dma_status shdmapp_tx_status(struct dma_chan *chan,
			dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	return DMA_IN_PROGRESS;
}

void shdmapp_chan_probe(struct shdma_dev *sdev,
			   struct shdma_chan *schan, int id)
{
	schan->pm_state = SHDMA_PM_ESTABLISHED;

	/* reference struct dma_device */
	schan->dma_chan.device = &sdev->dma_dev;
	dma_cookie_init(&schan->dma_chan);

	schan->dev = sdev->dma_dev.dev;
	schan->id = id;

	if (!schan->max_xfer_len)
		schan->max_xfer_len = PAGE_SIZE;

	spin_lock_init(&schan->chan_lock);

	/* Init descripter manage list */
	INIT_LIST_HEAD(&schan->ld_queue);
	INIT_LIST_HEAD(&schan->ld_free);

	/* Add the channel to DMA device channel list */
	list_add_tail(&schan->dma_chan.device_node,
			&sdev->dma_dev.channels);
	sdev->schan[sdev->dma_dev.chancnt++] = schan;
}
EXPORT_SYMBOL(shdmapp_chan_probe);

void shdmapp_chan_remove(struct shdma_chan *schan)
{
	list_del(&schan->dma_chan.device_node);
}
EXPORT_SYMBOL(shdmapp_chan_remove);

int shdmapp_init(struct device *dev, struct shdma_dev *sdev,
		    int chan_num)
{
	struct dma_device *dma_dev = &sdev->dma_dev;

	/*
	 * Require all call-backs for now, they can trivially be made optional
	 * later as required
	 */
	if (!sdev->ops || !sdev->ops->set_slave || !sdev->ops->halt_channel)
		return -EINVAL;

	sdev->schan = kcalloc(chan_num, sizeof(*sdev->schan), GFP_KERNEL);
	if (!sdev->schan)
		return -ENOMEM;

	INIT_LIST_HEAD(&dma_dev->channels);

	/* Common and MEMCPY operations */
	dma_dev->device_alloc_chan_resources = shdmapp_alloc_chan_resources;
	dma_dev->device_free_chan_resources = shdmapp_free_chan_resources;
	dma_dev->device_tx_status = shdmapp_tx_status;
	dma_dev->device_issue_pending = shdmapp_issue_pending;

	/* Compulsory for DMA_SLAVE fields */
	dma_dev->device_prep_slave_sg = shdmapp_prep_slave_sg;
	dma_dev->device_control = shdmapp_control;

	dma_dev->dev = dev;

	return 0;
}
EXPORT_SYMBOL(shdmapp_init);

void shdmapp_cleanup(struct shdma_dev *sdev)
{
	kfree(sdev->schan);
}
EXPORT_SYMBOL(shdmapp_cleanup);

static int __init shdmapp_enter(void)
{
	shdma_slave_used = kzalloc(DIV_ROUND_UP(slave_num, BITS_PER_LONG) *
				    sizeof(long), GFP_KERNEL);
	if (!shdma_slave_used)
		return -ENOMEM;
	return 0;
}
module_init(shdmapp_enter);

static void __exit shdmapp_exit(void)
{
	kfree(shdma_slave_used);
}
module_exit(shdmapp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SH-DMA driver base library");
MODULE_AUTHOR("Shunsuke Kataoka <shunsuke.kataoka.df@renesas.com>");

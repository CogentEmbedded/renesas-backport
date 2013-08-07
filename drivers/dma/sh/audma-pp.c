/*
 * drivers/dma/sh/audma-pp.c
 *     This file is DMA Engine driver for Audio-DMAC-pp peripheral.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This file is based on the drivers/dma/sh/shdma.c
 *
 * Renesas SuperH DMA Engine support
 *
 * base is drivers/dma/flsdma.c
 *
 * Copyright (C) 2011-2012 Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 * Copyright (C) 2009 Nobuhiro Iwamatsu <iwamatsu.nobuhiro@renesas.com>
 * Copyright (C) 2009 Renesas Solutions, Inc. All rights reserved.
 * Copyright (C) 2007 Freescale Semiconductor, Inc. All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * - DMA of SuperH does not have Hardware DMA chain mode.
 * - MAX DMA size is 16MB.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dmaengine.h>
#include <linux/platform_device.h>
#include <linux/sh_audma-pp.h>
#include <linux/kdebug.h>
#include <linux/spinlock.h>
#include <linux/rculist.h>

#include "../dmaengine.h"
#include "audma-pp.h"

#define SH_DMAE_DRV_NAME "sh-audmapp-engine"

/* Default MEMCPY transfer size = 2^2 = 4 bytes */
#define LOG2_DEFAULT_XFER_SIZE	2
#define SH_DMA_SLAVE_NUMBER 256
#define SH_DMA_TCR_MAX (16 * 1024 * 1024 - 1)

/*
 * Used for write-side mutual exclusion for the global device list,
 * read-side synchronization by way of RCU, and per-controller data.
 */
static DEFINE_SPINLOCK(sh_audmapp_lock);
static LIST_HEAD(sh_audmapp_devices);

static void sh_audmapp_writel(struct sh_audmapp_chan *sh_dc, u32 data, u32 reg)
{
	__raw_writel(data, sh_dc->base + reg / sizeof(u32));
}

static u32 sh_audmapp_readl(struct sh_audmapp_chan *sh_dc, u32 reg)
{
	return __raw_readl(sh_dc->base + reg / sizeof(u32));
}

static void dmae_start(struct sh_audmapp_chan *sh_chan)
{
	u32 chcr = sh_audmapp_readl(sh_chan, PDMACHCR);

	sh_audmapp_writel(sh_chan, (chcr | PDMACHCR_DE), PDMACHCR);
}

static void dmae_halt(struct sh_audmapp_chan *sh_chan)
{
	u32 chcr = sh_audmapp_readl(sh_chan, PDMACHCR);

	sh_audmapp_writel(sh_chan, (chcr & ~PDMACHCR_DE), PDMACHCR);
}

static const struct sh_audmapp_slave_config *dmae_find_slave(
	struct sh_audmapp_chan *sh_chan, int slave_id)
{
	struct sh_audmapp_device *shdev = to_sh_dev(sh_chan);
	struct sh_audmapp_pdata *pdata = shdev->pdata;
	const struct sh_audmapp_slave_config *cfg;
	int i;

	if (slave_id >= SH_DMA_SLAVE_NUMBER)
		return NULL;

	for (i = 0, cfg = pdata->slave; i < pdata->slave_num; i++, cfg++)
		if (cfg->slave_id == slave_id)
			return cfg;

	return NULL;
}

static int sh_audmapp_set_slave(struct shdma_chan *schan,
			     int slave_id, bool try)
{
	struct sh_audmapp_chan *sh_chan =
		container_of(schan, struct sh_audmapp_chan, shdma_chan);
	const struct sh_audmapp_slave_config *cfg =
		dmae_find_slave(sh_chan, slave_id);
	if (!cfg)
		return -ENODEV;

	if (!try)
		sh_chan->config = cfg;

	sh_audmapp_writel(sh_chan, cfg->sar, PDMASAR);
	sh_audmapp_writel(sh_chan, cfg->dar, PDMADAR);
	sh_audmapp_writel(sh_chan, cfg->chcr, PDMACHCR);
	dmae_start(sh_chan);

	return 0;
}

static void sh_audmapp_halt(struct shdma_chan *schan)
{
	struct sh_audmapp_chan *sh_chan = container_of(schan,
					struct sh_audmapp_chan, shdma_chan);
	dmae_halt(sh_chan);
}

static int __devinit sh_audmapp_chan_probe(struct sh_audmapp_device *shdev,
				int id, int irq, unsigned long flags)
{
	const struct sh_audmapp_channel *chan_pdata =
						&shdev->pdata->channel[id];
	struct shdma_dev *sdev = &shdev->shdma_dev;
	struct platform_device *pdev = to_platform_device(sdev->dma_dev.dev);
	struct sh_audmapp_chan *sh_chan;
	struct shdma_chan *schan;

	sh_chan = kzalloc(sizeof(struct sh_audmapp_chan), GFP_KERNEL);
	if (!sh_chan) {
		dev_err(sdev->dma_dev.dev,
			"No free memory for allocating dma channels!\n");
		return -ENOMEM;
	}

	schan = &sh_chan->shdma_chan;
	schan->max_xfer_len = SH_DMA_TCR_MAX + 1;

	shdmapp_chan_probe(sdev, schan, id);

	sh_chan->base = shdev->chan_reg + chan_pdata->offset / sizeof(u32);

	/* set up channel irq */
	if (pdev->id >= 0)
		snprintf(sh_chan->dev_id, sizeof(sh_chan->dev_id),
			 "sh-dmae%d.%d", pdev->id, id);
	else
		snprintf(sh_chan->dev_id, sizeof(sh_chan->dev_id),
			 "sh-dma%d", id);

	shdev->chan[id] = sh_chan;
	return 0;
}

static void sh_audmapp_chan_remove(struct sh_audmapp_device *shdev)
{
	struct dma_device *dma_dev = &shdev->shdma_dev.dma_dev;
	struct shdma_chan *schan;
	int i;

	shdma_for_each_chan(schan, &shdev->shdma_dev, i) {
		struct sh_audmapp_chan *sh_chan = container_of(schan,
					struct sh_audmapp_chan, shdma_chan);
		BUG_ON(!schan);
		shdmapp_chan_remove(schan);
		kfree(sh_chan);
	}
	dma_dev->chancnt = 0;
}

static void sh_audmapp_shutdown(struct platform_device *pdev)
{
	return;
}

static const struct shdma_ops sh_audmapp_shdma_ops = {
	.halt_channel = sh_audmapp_halt,
	.set_slave = sh_audmapp_set_slave,
};

static int __devinit sh_audmapp_probe(struct platform_device *pdev)
{
	struct sh_audmapp_pdata *pdata = pdev->dev.platform_data;
	unsigned long chan_flag[SH_AUDMAPP_MAX_CHANNELS] = {};
	int chan_irq[SH_AUDMAPP_MAX_CHANNELS];
	int err, i;
	struct sh_audmapp_device *shdev;
	struct dma_device *dma_dev;
	struct resource *chan;

	/* get platform data */
	if (!pdata || !pdata->channel_num)
		return -ENODEV;

	chan = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!request_mem_region(chan->start, resource_size(chan), pdev->name)) {
		dev_err(&pdev->dev, "DMAC register region already claimed\n");
		return -EBUSY;
	}

	err = -ENOMEM;
	shdev = kzalloc(sizeof(struct sh_audmapp_device), GFP_KERNEL);
	if (!shdev) {
		dev_err(&pdev->dev, "Not enough memory\n");
		goto ealloc;
	}

	dma_dev = &shdev->shdma_dev.dma_dev;

	shdev->chan_reg = ioremap(chan->start, resource_size(chan));
	if (!shdev->chan_reg)
		goto emapchan;

	if (pdata->slave && pdata->slave_num)
		dma_cap_set(DMA_SLAVE, dma_dev->cap_mask);

	/* Default transfer size of 32 bytes requires 32-byte alignment */
	dma_dev->copy_align = LOG2_DEFAULT_XFER_SIZE;

	shdev->shdma_dev.ops = &sh_audmapp_shdma_ops;
	shdev->shdma_dev.desc_size = sizeof(struct sh_audmapp_desc);

	err = shdmapp_init(&pdev->dev, &shdev->shdma_dev,
			      pdata->channel_num);
	if (err < 0)
		goto eshdma;

	/* platform data */
	shdev->pdata = pdev->dev.platform_data;

	platform_set_drvdata(pdev, shdev);

	spin_lock_irq(&sh_audmapp_lock);
	list_add_tail_rcu(&shdev->node, &sh_audmapp_devices);
	spin_unlock_irq(&sh_audmapp_lock);

	/* Create DMA Channel */
	for (i = 0; i < SH_AUDMAPP_MAX_CHANNELS; i++) {
		err = sh_audmapp_chan_probe(shdev, i, chan_irq[i],
								chan_flag[i]);
		if (err)
			goto chan_probe_err;
	}

	err = dma_async_device_register(&shdev->shdma_dev.dma_dev);
	if (err < 0)
		goto chan_probe_err;

	return err;

chan_probe_err:
	sh_audmapp_chan_remove(shdev);

	spin_lock_irq(&sh_audmapp_lock);
	list_del_rcu(&shdev->node);
	spin_unlock_irq(&sh_audmapp_lock);

	platform_set_drvdata(pdev, NULL);
	shdmapp_cleanup(&shdev->shdma_dev);
eshdma:
	iounmap(shdev->chan_reg);
	synchronize_rcu();
emapchan:
	kfree(shdev);
ealloc:
	release_mem_region(chan->start, resource_size(chan));

	return err;
}

static int __devexit sh_audmapp_remove(struct platform_device *pdev)
{
	struct sh_audmapp_device *shdev = platform_get_drvdata(pdev);
	struct dma_device *dma_dev = &shdev->shdma_dev.dma_dev;
	struct resource *res;
	int errirq = platform_get_irq(pdev, 0);

	dma_async_device_unregister(dma_dev);

	if (errirq > 0)
		free_irq(errirq, shdev);

	spin_lock_irq(&sh_audmapp_lock);
	list_del_rcu(&shdev->node);
	spin_unlock_irq(&sh_audmapp_lock);

	sh_audmapp_chan_remove(shdev);
	shdmapp_cleanup(&shdev->shdma_dev);

	iounmap(shdev->chan_reg);

	platform_set_drvdata(pdev, NULL);

	synchronize_rcu();
	kfree(shdev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res)
		release_mem_region(res->start, resource_size(res));

	return 0;
}

static struct platform_driver aupp_dmae_driver = {
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= SH_DMAE_DRV_NAME,
	},
	.remove		= __devexit_p(sh_audmapp_remove),
	.shutdown	= sh_audmapp_shutdown,
};

static int __init sh_audmapp_init(void)
{
	return platform_driver_probe(&aupp_dmae_driver, sh_audmapp_probe);
}
module_init(sh_audmapp_init);

static void __exit sh_audmapp_exit(void)
{
	platform_driver_unregister(&aupp_dmae_driver);
}
module_exit(sh_audmapp_exit);

MODULE_AUTHOR("Shunsuke Kataoka <shunsuke.kataoka.df@renesas.com>");
MODULE_DESCRIPTION("Renesas SH DMA Engine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" SH_DMAE_DRV_NAME);

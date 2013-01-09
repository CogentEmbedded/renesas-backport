/*
 * rcar_du_drv.c  --  R-Car Display Unit DRM driver
 *
 * Copyright (C) 2013 Renesas Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "rcar_du_crtc.h"
#include "rcar_du_drv.h"
#include "rcar_du_kms.h"

/* -----------------------------------------------------------------------------
 * DRM operations
 */

static int rcar_du_unload(struct drm_device *dev)
{
	drm_kms_helper_poll_fini(dev);
	drm_mode_config_cleanup(dev);
	drm_vblank_cleanup(dev);
	drm_irq_uninstall(dev);

	dev->dev_private = NULL;

	return 0;
}

static int rcar_du_load(struct drm_device *dev, unsigned long flags)
{
	struct platform_device *pdev = dev->platformdev;
	struct rcar_du_platform_data *pdata = pdev->dev.platform_data;
	struct rcar_du_device *rcdu;
	struct resource *ioarea;
	struct resource *mem;
	int ret;

	if (pdata == NULL) {
		dev_err(dev->dev, "no platform data\n");
		return -ENODEV;
	}

	rcdu = devm_kzalloc(&pdev->dev, sizeof(*rcdu), GFP_KERNEL);
	if (rcdu == NULL) {
		dev_err(dev->dev, "failed to allocate private data\n");
		return -ENOMEM;
	}

	rcdu->dev = &pdev->dev;
	rcdu->pdata = pdata;
	rcdu->ddev = dev;
	dev->dev_private = rcdu;

	/* I/O resources and clocks */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory resource\n");
		ret = -EINVAL;
		goto done;
	}

	ioarea = devm_request_mem_region(&pdev->dev, mem->start,
					 resource_size(mem), pdev->name);
	if (ioarea == NULL) {
		dev_err(&pdev->dev, "failed to request memory region\n");
		ret = -EBUSY;
		goto done;
	}

	rcdu->mmio = devm_ioremap_nocache(&pdev->dev, ioarea->start,
					  resource_size(ioarea));
	if (rcdu->mmio == NULL) {
		dev_err(&pdev->dev, "failed to remap memory resource\n");
		ret = -ENOMEM;
		goto done;
	}

	rcdu->clock = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(rcdu->clock)) {
		dev_err(&pdev->dev, "failed to get clock\n");
		ret = -ENOENT;
		goto done;
	}

	/* DRM/KMS objects */
	ret = rcar_du_modeset_init(rcdu);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to initialize DRM/KMS\n");
		goto done;
	}

	/* IRQ and vblank handling */
	ret = drm_vblank_init(dev, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to initialize vblank\n");
		goto done;
	}

	ret = drm_irq_install(dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to install IRQ handler\n");
		goto done;
	}

	platform_set_drvdata(pdev, rcdu);

done:
	if (ret)
		rcar_du_unload(dev);

	return ret;
}

static void rcar_du_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct rcar_du_device *rcdu = dev->dev_private;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(rcdu->crtc); ++i)
		rcar_du_crtc_cancel_page_flip(&rcdu->crtc[i], file);
}

static irqreturn_t rcar_du_irq(int irq, void *arg)
{
	struct drm_device *dev = arg;
	struct rcar_du_device *rcdu = dev->dev_private;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(rcdu->crtc); ++i)
		rcar_du_crtc_irq(&rcdu->crtc[i]);

	return IRQ_HANDLED;
}

static int rcar_du_enable_vblank(struct drm_device *dev, int crtc)
{
	struct rcar_du_device *rcdu = dev->dev_private;

	rcar_du_crtc_enable_vblank(&rcdu->crtc[crtc], true);

	return 0;
}

static void rcar_du_disable_vblank(struct drm_device *dev, int crtc)
{
	struct rcar_du_device *rcdu = dev->dev_private;

	rcar_du_crtc_enable_vblank(&rcdu->crtc[crtc], false);
}

static const struct file_operations rcar_du_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
	.poll		= drm_poll,
	.read		= drm_read,
	.fasync		= drm_fasync,
	.llseek		= no_llseek,
	.mmap		= drm_gem_cma_mmap,
};

static struct drm_driver rcar_du_driver = {
	.driver_features	= DRIVER_HAVE_IRQ | DRIVER_GEM | DRIVER_MODESET,
	.load			= rcar_du_load,
	.unload			= rcar_du_unload,
	.preclose		= rcar_du_preclose,
	.irq_handler		= rcar_du_irq,
	.get_vblank_counter	= drm_vblank_count,
	.enable_vblank		= rcar_du_enable_vblank,
	.disable_vblank		= rcar_du_disable_vblank,
	.gem_free_object	= drm_gem_cma_free_object,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
	.dumb_create		= drm_gem_cma_dumb_create,
	.dumb_map_offset	= drm_gem_cma_dumb_map_offset,
	.dumb_destroy		= drm_gem_cma_dumb_destroy,
	.fops			= &rcar_du_fops,
	.name			= "rcar-du",
	.desc			= "Renesas R-Car Display Unit",
	.date			= "20130110",
	.major			= 1,
	.minor			= 0,
};

/* -----------------------------------------------------------------------------
 * Power management
 */

#if CONFIG_PM_SLEEP
static int rcar_du_pm_suspend(struct device *dev)
{
	struct rcar_du_device *rcdu = dev_get_drvdata(dev);

	drm_kms_helper_poll_disable(rcdu->ddev);
	/* TODO Suspend the CRTC */

	return 0;
}

static int rcar_du_pm_resume(struct device *dev)
{
	struct rcar_du_device *rcdu = dev_get_drvdata(dev);

	/* TODO Resume the CRTC */

	drm_kms_helper_poll_enable(rcdu->ddev);
	return 0;
}
#endif

static const struct dev_pm_ops rcar_du_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rcar_du_pm_suspend, rcar_du_pm_resume)
};

/* -----------------------------------------------------------------------------
 * Platform driver
 */

static int rcar_du_probe(struct platform_device *pdev)
{
	return drm_platform_init(&rcar_du_driver, pdev);
}

static int rcar_du_remove(struct platform_device *pdev)
{
	drm_platform_exit(&rcar_du_driver, pdev);

	return 0;
}

static struct platform_driver rcar_du_platform_driver = {
	.probe		= rcar_du_probe,
	.remove		= rcar_du_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "rcar-du",
		.pm	= &rcar_du_pm_ops,
	},
};

module_platform_driver(rcar_du_platform_driver);

MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_DESCRIPTION("Renesas R-Car Display Unit DRM Driver");
MODULE_LICENSE("GPL");

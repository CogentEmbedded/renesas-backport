/*
 * rcar_du_plane.c  --  R-Car Display Unit Planes
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

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "rcar_du_drv.h"
#include "rcar_du_kms.h"
#include "rcar_du_plane.h"
#include "rcar_du_regs.h"

static u32 rcar_du_plane_read(struct rcar_du_device *rcdu,
			      unsigned int index, u32 reg)
{
	return rcar_du_read(rcdu, index * PLANE_OFF + reg);
}

static void rcar_du_plane_write(struct rcar_du_device *rcdu,
				unsigned int index, u32 reg, u32 data)
{
	rcar_du_write(rcdu, index * PLANE_OFF + reg, data);
}

int rcar_du_plane_reserve(struct rcar_du_plane *plane)
{
	struct rcar_du_device *rcdu = plane->dev;
	unsigned int i;
	int ret = -EBUSY;

	mutex_lock(&rcdu->planes.lock);

	for (i = 0; i < ARRAY_SIZE(rcdu->planes.planes); ++i) {
		if (!(rcdu->planes.free & (1 << i)))
			continue;

		if (plane->format->planes == 1 ||
		    rcdu->planes.free & (1 << ((i + 1) % 8)))
			break;
	}

	if (i == ARRAY_SIZE(rcdu->planes.planes))
		goto done;

	rcdu->planes.free &= ~(1 << i);
	if (plane->format->planes == 2)
		rcdu->planes.free &= ~(1 << ((i + 1) % 8));

	plane->hwindex = i;

	ret = 0;

done:
	mutex_unlock(&rcdu->planes.lock);
	return ret;
}

void rcar_du_plane_release(struct rcar_du_plane *plane)
{
	struct rcar_du_device *rcdu = plane->dev;

	if (plane->hwindex == -1)
		return;

	mutex_lock(&rcdu->planes.lock);
	rcdu->planes.free |= 1 << plane->hwindex;
	if (plane->format->planes == 2)
		rcdu->planes.free |= 1 << ((plane->hwindex + 1) % 8);
	mutex_unlock(&rcdu->planes.lock);

	plane->hwindex = -1;
}

void rcar_du_plane_update_base(struct rcar_du_plane *plane)
{
	struct rcar_du_device *rcdu = plane->dev;
	unsigned int index = plane->hwindex;

	rcar_du_plane_write(rcdu, index, PnSPXR, plane->src_x);
	rcar_du_plane_write(rcdu, index, PnSPYR, plane->src_y);
	rcar_du_plane_write(rcdu, index, PnDSA0R, plane->dma[0]);

	if (plane->format->planes == 2) {
		index = (index + 1) % 8;

		rcar_du_plane_write(rcdu, index, PnSPXR, plane->src_x);
		rcar_du_plane_write(rcdu, index, PnSPYR, plane->src_y);
		rcar_du_plane_write(rcdu, index, PnDSA0R, plane->dma[1]);
	}
}

void rcar_du_plane_compute_base(struct rcar_du_plane *plane,
				struct drm_framebuffer *fb)
{
	struct drm_gem_cma_object *gem;

	gem = drm_fb_cma_get_gem_obj(fb, 0);
	plane->dma[0] = gem->paddr + fb->offsets[0];

	if (plane->format->planes == 2) {
		gem = drm_fb_cma_get_gem_obj(fb, 1);
		plane->dma[1] = gem->paddr + fb->offsets[1];
	}
}

static void __rcar_du_plane_setup(struct rcar_du_plane *plane,
				  unsigned int index)
{
	struct rcar_du_device *rcdu = plane->dev;
	u32 ddcr2 = PnDDCR2_CODE;
	u32 ddcr4;
	u32 pnmr;
	u32 mwr;

	/* Data format
	 *
	 * The data format is selected by the DDDF field in PnMR and the EDF
	 * field in DDCR4.
	 */
	ddcr4 = rcar_du_plane_read(rcdu, index, PnDDCR4);
	ddcr4 &= ~PnDDCR4_EDF_MASK;
	ddcr4 |= plane->format->edf | PnDDCR4_CODE;

	/* Disable color-keying for now. */
	pnmr = PnMR_SPIM_TP_OFF | PnMR_BM_MD | plane->format->pnmr;

	/* For packed YUV formats we need to select the U/V order. */
	if (plane->format->fourcc == DRM_FORMAT_YUYV)
		pnmr |= PnMR_YCDF_YUYV;

	if (plane->format->planes == 2) {
		if (plane->hwindex != index) {
			if (plane->format->fourcc == DRM_FORMAT_NV12 ||
			    plane->format->fourcc == DRM_FORMAT_NV21)
				ddcr2 |= PnDDCR2_Y420;

			if (plane->format->fourcc == DRM_FORMAT_NV21)
				ddcr2 |= PnDDCR2_NV21;

			ddcr2 |= PnDDCR2_DIVU;
		} else {
			ddcr2 |= PnDDCR2_DIVY;
		}
	}

	rcar_du_plane_write(rcdu, index, PnMR, pnmr);
	rcar_du_plane_write(rcdu, index, PnDDCR2, ddcr2);
	rcar_du_plane_write(rcdu, index, PnDDCR4, ddcr4);

	/* The PnALPHAR register controls alpha-blending in 16bpp formats
	 * (ARGB1555). Set the alpha value to 0, and enable alpha-blending when
	 * the A bit is 0. This maps A=0 to alpha=0 and A=1 to alpha=255.
	 */
	rcar_du_plane_write(rcdu, index, PnALPHAR, PnALPHAR_ABIT_0);

	/* Memory pitch (expressed in pixels) */
	if (plane->format->planes == 2)
		mwr = plane->pitch;
	else
		mwr = plane->pitch * 8 / plane->format->bpp;

	rcar_du_plane_write(rcdu, index, PnMWR, mwr);

	/* Destination position and size */
	rcar_du_plane_write(rcdu, index, PnDSXR, plane->width);
	rcar_du_plane_write(rcdu, index, PnDSYR, plane->height);
	rcar_du_plane_write(rcdu, index, PnDPXR, plane->dst_x);
	rcar_du_plane_write(rcdu, index, PnDPYR, plane->dst_y);

	/* Wrap-around and blinking, disabled */
	rcar_du_plane_write(rcdu, index, PnWASPR, 0);
	rcar_du_plane_write(rcdu, index, PnWAMWR, 4095);
	rcar_du_plane_write(rcdu, index, PnBTR, 0);
	rcar_du_plane_write(rcdu, index, PnMLR, 0);
}

void rcar_du_plane_setup(struct rcar_du_plane *plane)
{
	__rcar_du_plane_setup(plane, plane->hwindex);
	if (plane->format->planes == 2)
		__rcar_du_plane_setup(plane, (plane->hwindex + 1) % 8);

	rcar_du_plane_update_base(plane);
}

static int
rcar_du_plane_update(struct drm_plane *plane, struct drm_crtc *crtc,
		       struct drm_framebuffer *fb, int crtc_x, int crtc_y,
		       unsigned int crtc_w, unsigned int crtc_h,
		       uint32_t src_x, uint32_t src_y,
		       uint32_t src_w, uint32_t src_h)
{
	struct rcar_du_plane *rplane = to_rcar_plane(plane);
	struct rcar_du_device *rcdu = plane->dev->dev_private;
	const struct rcar_du_format_info *format;
	unsigned int nplanes;
	int ret;

	format = rcar_du_format_info(fb->pixel_format);
	if (format == NULL) {
		dev_dbg(rcdu->dev, "%s: unsupported format %08x\n", __func__,
			fb->pixel_format);
		return -EINVAL;
	}

	if (src_w >> 16 != crtc_w || src_h >> 16 != crtc_h) {
		dev_dbg(rcdu->dev, "%s: scaling not supported\n", __func__);
		return -EINVAL;
	}

	nplanes = rplane->format ? rplane->format->planes : 0;

	rplane->crtc = crtc;
	rplane->format = format;
	rplane->pitch = fb->pitches[0];

	rplane->src_x = src_x >> 16;
	rplane->src_y = src_y >> 16;
	rplane->dst_x = crtc_x;
	rplane->dst_y = crtc_y;
	rplane->width = crtc_w;
	rplane->height = crtc_h;

	/* Reallocate hardware planes if the number of required planes has
	 * changed.
	 */
	if (format->planes != nplanes) {
		rcar_du_plane_release(rplane);
		ret = rcar_du_plane_reserve(rplane);
		if (ret < 0)
			return ret;
	}

	rcar_du_plane_compute_base(rplane, fb);
	rcar_du_plane_setup(rplane);

	mutex_lock(&rcdu->planes.lock);
	rplane->enabled = true;
	rcar_du_crtc_update_planes(rplane->crtc);
	mutex_unlock(&rcdu->planes.lock);

	return 0;
}

static int rcar_du_plane_disable(struct drm_plane *plane)
{
	struct rcar_du_device *rcdu = plane->dev->dev_private;
	struct rcar_du_plane *rplane = to_rcar_plane(plane);

	mutex_lock(&rcdu->planes.lock);
	rplane->enabled = false;
	rcar_du_crtc_update_planes(rplane->crtc);
	mutex_unlock(&rcdu->planes.lock);

	rcar_du_plane_release(rplane);

	rplane->crtc = NULL;
	rplane->format = NULL;

	return 0;
}

static int rcar_du_plane_set_property(struct drm_plane *plane,
				      struct drm_property *property,
				      uint64_t value)
{
	struct rcar_du_device *rcdu = plane->dev->dev_private;
	struct rcar_du_plane *rplane = to_rcar_plane(plane);

	if (property != rcdu->planes.zpos)
		return -EINVAL;

	mutex_lock(&rcdu->planes.lock);
	if (rplane->zpos == value)
		goto done;

	rplane->zpos = value;
	if (!rplane->enabled)
		goto done;

	rcar_du_crtc_update_planes(rplane->crtc);

done:
	mutex_unlock(&rcdu->planes.lock);
	return 0;
}

static const struct drm_plane_funcs rcar_du_plane_funcs = {
	.update_plane = rcar_du_plane_update,
	.disable_plane = rcar_du_plane_disable,
	.set_property = rcar_du_plane_set_property,
	.destroy = drm_plane_cleanup,
};

static const uint32_t formats[] = {
	DRM_FORMAT_RGB565,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV16,
};

int rcar_du_plane_init(struct rcar_du_device *rcdu)
{
	unsigned int i;
	int ret;

	mutex_init(&rcdu->planes.lock);
	rcdu->planes.free = 0xff;

	rcdu->planes.zpos =
		drm_property_create_range(rcdu->ddev, 0, "zpos",
					  ARRAY_SIZE(rcdu->crtc),
					  ARRAY_SIZE(rcdu->planes.planes) - 1);
	if (rcdu->planes.zpos == NULL)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(rcdu->planes.planes); ++i) {
		struct rcar_du_plane *plane = &rcdu->planes.planes[i];

		plane->dev = rcdu;
		plane->hwindex = -1;
		plane->zpos = 1;

		/* Reserve one plane per CRTC */
		if (i < ARRAY_SIZE(rcdu->crtc)) {
			plane->zpos = 0;
			continue;
		}

		ret = drm_plane_init(rcdu->ddev, &plane->plane, 1,
				     &rcar_du_plane_funcs, formats,
				     ARRAY_SIZE(formats), false);
		if (ret < 0)
			return ret;

		drm_object_attach_property(&plane->plane.base,
					   rcdu->planes.zpos, 1);

	}

	return 0;
}

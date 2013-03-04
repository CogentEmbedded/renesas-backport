/*
 * rcar_du_crtc.c  --  R-Car Display Unit CRTCs
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
#include <linux/mutex.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "rcar_du_crtc.h"
#include "rcar_du_drv.h"
#include "rcar_du_kms.h"
#include "rcar_du_lvds.h"
#include "rcar_du_plane.h"
#include "rcar_du_regs.h"
#include "rcar_du_vga.h"

#define to_rcar_crtc(c)	container_of(c, struct rcar_du_crtc, crtc)

static u32 rcar_du_crtc_read(struct rcar_du_crtc *rcrtc, u32 reg)
{
	struct rcar_du_device *rcdu = rcrtc->crtc.dev->dev_private;

	return rcar_du_read(rcdu, rcrtc->mmio_offset + reg);
}

static void rcar_du_crtc_write(struct rcar_du_crtc *rcrtc, u32 reg, u32 data)
{
	struct rcar_du_device *rcdu = rcrtc->crtc.dev->dev_private;

	rcar_du_write(rcdu, rcrtc->mmio_offset + reg, data);
}

static void rcar_du_crtc_clr(struct rcar_du_crtc *rcrtc, u32 reg, u32 clr)
{
	struct rcar_du_device *rcdu = rcrtc->crtc.dev->dev_private;

	rcar_du_write(rcdu, rcrtc->mmio_offset + reg,
		      rcar_du_read(rcdu, rcrtc->mmio_offset + reg) & ~clr);
}

static void rcar_du_crtc_set(struct rcar_du_crtc *rcrtc, u32 reg, u32 set)
{
	struct rcar_du_device *rcdu = rcrtc->crtc.dev->dev_private;

	rcar_du_write(rcdu, rcrtc->mmio_offset + reg,
		      rcar_du_read(rcdu, rcrtc->mmio_offset + reg) | set);
}

static void rcar_du_crtc_set_display_timing(struct rcar_du_crtc *rcrtc)
{
	struct drm_crtc *crtc = &rcrtc->crtc;
	struct rcar_du_device *rcdu = crtc->dev->dev_private;
	const struct drm_display_mode *mode = &crtc->mode;
	unsigned long clk;
	u32 value;
	u32 div;

	/* Dot clock */
	clk = clk_get_rate(rcdu->clock);
	div = DIV_ROUND_CLOSEST(clk, mode->clock * 1000);
	div = clamp(div, 1U, 64U) - 1;

	rcar_du_write(rcdu, rcrtc->index ? ESCR2 : ESCR,
		      ESCR_DCLKSEL_CLKS | div);
	rcar_du_write(rcdu, rcrtc->index ? OTAR2 : OTAR, 0);

	/* Signal polarities */
	value = ((mode->flags & DRM_MODE_FLAG_PVSYNC) ? 0 : DSMR_VSL)
	      | ((mode->flags & DRM_MODE_FLAG_PHSYNC) ? 0 : DSMR_HSL)
	      | DSMR_DIPM_DE;
	rcar_du_crtc_write(rcrtc, DSMR, value);

	/* Display timings */
	rcar_du_crtc_write(rcrtc, HDSR, mode->htotal - mode->hsync_start - 19);
	rcar_du_crtc_write(rcrtc, HDER, mode->htotal - mode->hsync_start +
					mode->hdisplay - 19);
	rcar_du_crtc_write(rcrtc, HSWR, mode->hsync_end -
					mode->hsync_start - 1);
	rcar_du_crtc_write(rcrtc, HCR,  mode->htotal - 1);

	rcar_du_crtc_write(rcrtc, VDSR, mode->vtotal - mode->vsync_end - 2);
	rcar_du_crtc_write(rcrtc, VDER, mode->vtotal - mode->vsync_end +
					mode->vdisplay - 2);
	rcar_du_crtc_write(rcrtc, VSPR, mode->vtotal - mode->vsync_end +
					mode->vsync_start - 1);
	rcar_du_crtc_write(rcrtc, VCR,  mode->vtotal - 1);

	rcar_du_crtc_write(rcrtc, DESR,  mode->htotal - mode->hsync_start);
	rcar_du_crtc_write(rcrtc, DEWR,  mode->hdisplay);
}

static void rcar_du_crtc_start_stop(struct rcar_du_crtc *rcrtc, bool start)
{
	u32 value;

	value = rcar_du_crtc_read(rcrtc, DSYSR)
	      & ~(DSYSR_DRES | DSYSR_DEN | DSYSR_TVM_MASK);

	if (start)
		rcar_du_crtc_write(rcrtc, DSYSR, value | DSYSR_DEN);
	else
		rcar_du_crtc_write(rcrtc, DSYSR, value | DSYSR_DRES);
}

void rcar_du_crtc_update_planes(struct drm_crtc *crtc)
{
	struct rcar_du_device *rcdu = crtc->dev->dev_private;
	struct rcar_du_crtc *rcrtc = to_rcar_crtc(crtc);
	struct rcar_du_plane *planes[ARRAY_SIZE(rcdu->planes.planes)];
	unsigned int num_planes = 0;
	unsigned int prio = 0;
	unsigned int i;
	u32 dspr = 0;

	for (i = 0; i < ARRAY_SIZE(rcdu->planes.planes); ++i) {
		struct rcar_du_plane *plane = &rcdu->planes.planes[i];
		unsigned int j;

		if (plane->crtc != &rcrtc->crtc || !plane->enabled)
			continue;

		/* Insert the plane in the sorted planes array. */
		for (j = num_planes++; j > 0; --j) {
			if (planes[j-1]->zpos <= plane->zpos)
				break;
			planes[j] = planes[j-1];
		}

		planes[j] = plane;
		prio += plane->format->planes * 4;
	}

	for (i = 0; i < num_planes; ++i) {
		struct rcar_du_plane *plane = planes[i];
		unsigned int index = plane->hwindex;

		prio -= 4;
		dspr |= (index + 1) << prio;

		if (plane->format->planes == 2) {
			index = (index + 1) % 8;

			prio -= 4;
			dspr |= (index + 1) << prio;
		}
	}

	rcar_du_crtc_write(rcrtc, DS1PR, dspr);
}

/*
 * rcar_du_crtc_start - Configure and start the LCDC
 * @rcrtc: the SH Mobile CRTC
 *
 * Configure and start the LCDC device. External devices (clocks, MERAM, panels,
 * ...) are not touched by this function.
 */
static void rcar_du_crtc_start(struct rcar_du_crtc *rcrtc)
{
	struct drm_crtc *crtc = &rcrtc->crtc;
	struct rcar_du_device *rcdu = crtc->dev->dev_private;
	struct drm_plane *plane;

	if (rcrtc->started)
		return;

	if (WARN_ON(rcrtc->plane->format == NULL))
		return;

	/* Enable clocks before accessing the hardware. */
	clk_enable(rcdu->clock);

	/* Enable extended features */
	rcar_du_write(rcdu, DEFR, DEFR_CODE | DEFR_DEFE);
	rcar_du_write(rcdu, DEFR2, DEFR2_CODE | DEFR2_DEFE2G);
	rcar_du_write(rcdu, DEFR3, DEFR3_CODE | DEFR3_DEFE3);
	rcar_du_write(rcdu, DEFR4, DEFR4_CODE);

	/* Set display off and background to black */
	rcar_du_crtc_write(rcrtc, DOOR, DOOR_RGB(0, 0, 0));
	rcar_du_crtc_write(rcrtc, BPOR, BPOR_RGB(0, 0, 0));

	/* Configure output routing: enable both superposition processors */
	rcar_du_write(rcdu, DORCR, DORCR_DPRS);

	rcar_du_crtc_set_display_timing(rcrtc);
	rcar_du_plane_setup(rcrtc->plane);

	mutex_lock(&rcdu->planes.lock);
	rcrtc->plane->enabled = true;
	rcar_du_crtc_update_planes(crtc);
	mutex_unlock(&rcdu->planes.lock);

	/* Setup planes. */
	list_for_each_entry(plane, &rcdu->ddev->mode_config.plane_list, head) {
		struct rcar_du_plane *rplane = to_rcar_plane(plane);

		if (plane->crtc != crtc)
			continue;

		rcar_du_plane_setup(rplane);
	}

	rcar_du_crtc_start_stop(rcrtc, true);

	rcrtc->started = true;
}

static void rcar_du_crtc_stop(struct rcar_du_crtc *rcrtc)
{
	struct drm_crtc *crtc = &rcrtc->crtc;
	struct rcar_du_device *rcdu = crtc->dev->dev_private;

	if (!rcrtc->started)
		return;

	mutex_lock(&rcdu->planes.lock);
	rcrtc->plane->enabled = false;
	rcar_du_crtc_update_planes(crtc);
	mutex_unlock(&rcdu->planes.lock);

	rcar_du_crtc_start_stop(rcrtc, false);

	clk_disable(rcdu->clock);

	rcrtc->started = false;
}

void rcar_du_crtc_suspend(struct rcar_du_crtc *rcrtc)
{
	rcar_du_crtc_stop(rcrtc);
}

void rcar_du_crtc_resume(struct rcar_du_crtc *rcrtc)
{
	if (rcrtc->dpms != DRM_MODE_DPMS_ON)
		return;

	rcar_du_crtc_start(rcrtc);
}

static void rcar_du_crtc_update_base(struct rcar_du_crtc *rcrtc)
{
	struct drm_crtc *crtc = &rcrtc->crtc;

	rcar_du_plane_compute_base(rcrtc->plane, crtc->fb);
	rcar_du_plane_update_base(rcrtc->plane);
}

static void rcar_du_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct rcar_du_crtc *rcrtc = to_rcar_crtc(crtc);

	if (rcrtc->dpms == mode)
		return;

	if (mode == DRM_MODE_DPMS_ON)
		rcar_du_crtc_start(rcrtc);
	else
		rcar_du_crtc_stop(rcrtc);

	rcrtc->dpms = mode;
}

static bool rcar_du_crtc_mode_fixup(struct drm_crtc *crtc,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	/* TODO Fixup modes */
	return true;
}

static void rcar_du_crtc_mode_prepare(struct drm_crtc *crtc)
{
	struct rcar_du_crtc *rcrtc = to_rcar_crtc(crtc);

	rcar_du_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
	rcar_du_plane_release(rcrtc->plane);
}

static int rcar_du_crtc_mode_set(struct drm_crtc *crtc,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode,
				 int x, int y,
				 struct drm_framebuffer *old_fb)
{
	struct rcar_du_device *rcdu = crtc->dev->dev_private;
	struct rcar_du_crtc *rcrtc = to_rcar_crtc(crtc);
	const struct rcar_du_format_info *format;
	int ret;

	format = rcar_du_format_info(crtc->fb->pixel_format);
	if (format == NULL) {
		dev_dbg(rcdu->dev, "mode_set: unsupported format %08x\n",
			crtc->fb->pixel_format);
		return -EINVAL;
	}

	ret = rcar_du_plane_reserve(rcrtc->plane, format);
	if (ret < 0)
		return ret;

	rcrtc->plane->format = format;
	rcrtc->plane->pitch = crtc->fb->pitches[0];

	rcrtc->plane->src_x = x;
	rcrtc->plane->src_y = y;
	rcrtc->plane->width = crtc->fb->width;
	rcrtc->plane->height = crtc->fb->height;

	rcar_du_plane_compute_base(rcrtc->plane, crtc->fb);

	return 0;
}

static void rcar_du_crtc_mode_commit(struct drm_crtc *crtc)
{
	rcar_du_crtc_dpms(crtc, DRM_MODE_DPMS_ON);
}

static int rcar_du_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
				      struct drm_framebuffer *old_fb)
{
	rcar_du_crtc_update_base(to_rcar_crtc(crtc));

	return 0;
}

static void rcar_du_crtc_disable(struct drm_crtc *crtc)
{
	struct rcar_du_crtc *rcrtc = to_rcar_crtc(crtc);

	rcar_du_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
	rcar_du_plane_release(rcrtc->plane);
}

static const struct drm_crtc_helper_funcs crtc_helper_funcs = {
	.dpms = rcar_du_crtc_dpms,
	.mode_fixup = rcar_du_crtc_mode_fixup,
	.prepare = rcar_du_crtc_mode_prepare,
	.commit = rcar_du_crtc_mode_commit,
	.mode_set = rcar_du_crtc_mode_set,
	.mode_set_base = rcar_du_crtc_mode_set_base,
	.disable = rcar_du_crtc_disable,
};

void rcar_du_crtc_cancel_page_flip(struct rcar_du_crtc *rcrtc,
				   struct drm_file *file)
{
	struct drm_pending_vblank_event *event;
	struct drm_device *dev = rcrtc->crtc.dev;
	unsigned long flags;

	/* Destroy the pending vertical blanking event associated with the
	 * pending page flip, if any, and disable vertical blanking interrupts.
	 */
	spin_lock_irqsave(&dev->event_lock, flags);
	event = rcrtc->event;
	if (event && event->base.file_priv == file) {
		rcrtc->event = NULL;
		event->base.destroy(&event->base);
		drm_vblank_put(dev, 0);
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static void rcar_du_crtc_finish_page_flip(struct rcar_du_crtc *rcrtc)
{
	struct drm_pending_vblank_event *event;
	struct drm_device *dev = rcrtc->crtc.dev;
	struct timeval vblanktime;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	event = rcrtc->event;
	rcrtc->event = NULL;
	spin_unlock_irqrestore(&dev->event_lock, flags);

	if (event == NULL)
		return;

	event->event.sequence = drm_vblank_count_and_time(dev, 0, &vblanktime);
	event->event.tv_sec = vblanktime.tv_sec;
	event->event.tv_usec = vblanktime.tv_usec;

	spin_lock_irqsave(&dev->event_lock, flags);
	list_add_tail(&event->base.link, &event->base.file_priv->event_list);
	wake_up_interruptible(&event->base.file_priv->event_wait);
	spin_unlock_irqrestore(&dev->event_lock, flags);

	drm_vblank_put(dev, 0);
}

static int rcar_du_crtc_page_flip(struct drm_crtc *crtc,
				  struct drm_framebuffer *fb,
				  struct drm_pending_vblank_event *event)
{
	struct rcar_du_crtc *rcrtc = to_rcar_crtc(crtc);
	struct drm_device *dev = rcrtc->crtc.dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	if (rcrtc->event != NULL) {
		spin_unlock_irqrestore(&dev->event_lock, flags);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);

	crtc->fb = fb;
	rcar_du_crtc_update_base(rcrtc);

	if (event) {
		event->pipe = 0;
		drm_vblank_get(dev, 0);
		spin_lock_irqsave(&dev->event_lock, flags);
		rcrtc->event = event;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	return 0;
}

static const struct drm_crtc_funcs crtc_funcs = {
	.destroy = drm_crtc_cleanup,
	.set_config = drm_crtc_helper_set_config,
	.page_flip = rcar_du_crtc_page_flip,
};

int rcar_du_crtc_create(struct rcar_du_device *rcdu, unsigned int index)
{
	const struct rcar_du_encoder_data *pdata = &rcdu->pdata->encoders[index];
	struct rcar_du_crtc *rcrtc = &rcdu->crtc[index];
	struct drm_crtc *crtc = &rcrtc->crtc;
	int ret;

	if (pdata->encoder == RCAR_DU_ENCODER_UNUSED)
		return 0;

	rcrtc->mmio_offset = index ? DISP2_REG_OFFSET : 0;
	rcrtc->index = index;
	rcrtc->dpms = DRM_MODE_DPMS_OFF;
	rcrtc->plane = &rcdu->planes.planes[index];

	rcrtc->plane->crtc = crtc;

	ret = drm_crtc_init(rcdu->ddev, crtc, &crtc_funcs);
	if (ret < 0)
		return ret;

	drm_crtc_helper_add(crtc, &crtc_helper_funcs);

	switch (pdata->encoder) {
	case RCAR_DU_ENCODER_VGA:
		rcar_du_vga_init(rcdu, &pdata->vga, index);
		break;

	case RCAR_DU_ENCODER_LVDS:
		rcar_du_lvds_init(rcdu, &pdata->lvds, index);
		break;

	default:
		break;
	}

	return 0;
}

void rcar_du_crtc_enable_vblank(struct rcar_du_crtc *rcrtc, bool enable)
{
	if (enable) {
		rcar_du_crtc_write(rcrtc, DSRCR, DSRCR_VBCL);
		rcar_du_crtc_set(rcrtc, DIER, DIER_VBE);
	} else {
		rcar_du_crtc_clr(rcrtc, DIER, DIER_VBE);
	}
}

void rcar_du_crtc_irq(struct rcar_du_crtc *rcrtc)
{
	u32 status;

	status = rcar_du_crtc_read(rcrtc, DSSR);
	rcar_du_crtc_write(rcrtc, DSRCR, status & DSRCR_MASK);

	if (status & DSSR_VBK) {
		drm_handle_vblank(rcrtc->crtc.dev, rcrtc->index);
		rcar_du_crtc_finish_page_flip(rcrtc);
	}
}

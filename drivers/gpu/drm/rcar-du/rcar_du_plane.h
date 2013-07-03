/*
 * rcar_du_plane.h  --  R-Car Display Unit Planes
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

#ifndef __RCAR_DU_PLANE_H__
#define __RCAR_DU_PLANE_H__

#include <drm/drmP.h>
#include <drm/drm_crtc.h>

struct drm_framebuffer;
struct rcar_du_device;

struct rcar_du_plane {
	struct drm_plane plane;

	struct rcar_du_device *dev;
	struct drm_crtc *crtc;

	bool enabled;

	int hwindex;		/* 0-based, -1 means unused */
	unsigned int alpha;

	const struct rcar_du_format_info *format;

	unsigned long dma[2];
	unsigned int pitch;

	unsigned int width;
	unsigned int height;

	unsigned int src_x;
	unsigned int src_y;
	unsigned int dst_x;
	unsigned int dst_y;
};

#define to_rcar_plane(p)	container_of(p, struct rcar_du_plane, plane)

int rcar_du_plane_init(struct rcar_du_device *rcdu);
void rcar_du_plane_setup(struct rcar_du_plane *plane);
void rcar_du_plane_update_base(struct rcar_du_plane *plane);
void rcar_du_plane_compute_base(struct rcar_du_plane *plane,
				struct drm_framebuffer *fb);
int rcar_du_plane_reserve(struct rcar_du_plane *plane);
void rcar_du_plane_release(struct rcar_du_plane *plane);

#endif /* __RCAR_DU_PLANE_H__ */

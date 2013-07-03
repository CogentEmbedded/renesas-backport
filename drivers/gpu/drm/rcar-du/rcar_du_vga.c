/*
 * rcar_du_vga.c  --  R-Car Display Unit VGA DAC and Connector
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

#include "rcar_du_drv.h"
#include "rcar_du_kms.h"
#include "rcar_du_vga.h"

struct rcar_du_vga_encoder {
	struct drm_encoder encoder;
};

struct rcar_du_vga_connector {
	struct drm_connector connector;
	struct drm_encoder *encoder;
};

/* -----------------------------------------------------------------------------
 * Connector
 */

#define to_rcar_vga_connector(c) \
	container_of(c, struct rcar_du_vga_connector, connector)

static int rcar_du_vga_connector_get_modes(struct drm_connector *connector)
{
	return drm_add_modes_noedid(connector, 1280, 768);
}

static int rcar_du_vga_connector_mode_valid(struct drm_connector *connector,
					    struct drm_display_mode *mode)
{
	return MODE_OK;
}

static struct drm_encoder *
rcar_du_vga_connector_best_encoder(struct drm_connector *connector)
{
	struct rcar_du_vga_connector *vgacon = to_rcar_vga_connector(connector);

	return vgacon->encoder;
}

static const struct drm_connector_helper_funcs connector_helper_funcs = {
	.get_modes = rcar_du_vga_connector_get_modes,
	.mode_valid = rcar_du_vga_connector_mode_valid,
	.best_encoder = rcar_du_vga_connector_best_encoder,
};

static void rcar_du_vga_connector_destroy(struct drm_connector *connector)
{
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
}

static enum drm_connector_status
rcar_du_vga_connector_detect(struct drm_connector *connector, bool force)
{
	return connector_status_unknown;
}

static const struct drm_connector_funcs connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = rcar_du_vga_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = rcar_du_vga_connector_destroy,
};

static int rcar_du_vga_connector_init(struct rcar_du_device *rcdu,
				      struct drm_encoder *encoder)
{
	struct rcar_du_vga_connector *vgacon;
	struct drm_connector *connector;
	int ret;

	vgacon = devm_kzalloc(rcdu->dev, sizeof(*vgacon), GFP_KERNEL);
	if (vgacon == NULL)
		return -ENOMEM;

	connector = &vgacon->connector;
	connector->display_info.width_mm = 0;
	connector->display_info.height_mm = 0;

	ret = drm_connector_init(rcdu->ddev, connector, &connector_funcs,
				 DRM_MODE_CONNECTOR_VGA);
	if (ret < 0)
		return ret;

	drm_connector_helper_add(connector, &connector_helper_funcs);
	ret = drm_sysfs_connector_add(connector);
	if (ret < 0)
		return ret;

	drm_helper_connector_dpms(connector, DRM_MODE_DPMS_OFF);
	drm_object_property_set_value(&connector->base,
		rcdu->ddev->mode_config.dpms_property, DRM_MODE_DPMS_OFF);

	ret = drm_mode_connector_attach_encoder(connector, encoder);
	if (ret < 0)
		return ret;

	connector->encoder = encoder;
	vgacon->encoder = encoder;

	return 0;
}

/* -----------------------------------------------------------------------------
 * Encoder
 */

#define to_rcar_vga_encoder(e) \
	container_of(e, struct rcar_du_vga_encoder, encoder)

static void rcar_du_vga_encoder_dpms(struct drm_encoder *encoder, int mode)
{
}

static bool rcar_du_vga_encoder_mode_fixup(struct drm_encoder *encoder,
					   const struct drm_display_mode *mode,
					   struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void rcar_du_vga_encoder_mode_prepare(struct drm_encoder *encoder)
{
}

static void rcar_du_vga_encoder_mode_set(struct drm_encoder *encoder,
					 struct drm_display_mode *mode,
					 struct drm_display_mode *adjusted_mode)
{
}

static void rcar_du_vga_encoder_mode_commit(struct drm_encoder *encoder)
{
}

static const struct drm_encoder_helper_funcs encoder_helper_funcs = {
	.dpms = rcar_du_vga_encoder_dpms,
	.mode_fixup = rcar_du_vga_encoder_mode_fixup,
	.prepare = rcar_du_vga_encoder_mode_prepare,
	.commit = rcar_du_vga_encoder_mode_commit,
	.mode_set = rcar_du_vga_encoder_mode_set,
};

static void rcar_du_vga_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs encoder_funcs = {
	.destroy = rcar_du_vga_encoder_destroy,
};

int rcar_du_vga_init(struct rcar_du_device *rcdu,
		     const struct rcar_du_encoder_vga_data *data,
		     unsigned int crtc)
{
	struct rcar_du_vga_encoder *vgaenc;
	struct drm_encoder *encoder;
	int ret;

	vgaenc = devm_kzalloc(rcdu->dev, sizeof(*vgaenc), GFP_KERNEL);
	if (vgaenc == NULL)
		return -ENOMEM;

	encoder = &vgaenc->encoder;
	encoder->possible_crtcs = 1 << crtc;

	ret = drm_encoder_init(rcdu->ddev, encoder, &encoder_funcs,
			       DRM_MODE_ENCODER_DAC);
	if (ret < 0)
		return ret;

	drm_encoder_helper_add(encoder, &encoder_helper_funcs);

	return rcar_du_vga_connector_init(rcdu, encoder);
}

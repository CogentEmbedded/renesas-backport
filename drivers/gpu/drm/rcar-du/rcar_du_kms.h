/*
 * rcar_du_kms.h  --  R-Car Display Unit Mode Setting
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

#ifndef __RCAR_DU_KMS_H__
#define __RCAR_DU_KMS_H__

#include <linux/types.h>

struct rcar_du_device;

struct rcar_du_format_info {
	u32 fourcc;
	unsigned int bpp;
	unsigned int planes;
	unsigned int dddf;
	unsigned int edf;
};

const struct rcar_du_format_info *rcar_du_format_info(u32 fourcc);

int rcar_du_modeset_init(struct rcar_du_device *rcdu);

#endif /* __RCAR_DU_KMS_H__ */

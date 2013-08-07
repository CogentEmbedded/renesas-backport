/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_HDMI_H_
#define __LINUX_HDMI_H_

#include <linux/types.h>

enum hdmi_infoframe_type {
	HDMI_INFOFRAME_TYPE_AVI,
};

enum hdmi_colorspace {
	HDMI_COLORSPACE_RGB,
	HDMI_COLORSPACE_YUV422,
	HDMI_COLORSPACE_YUV444,
};

enum hdmi_scan_mode {
	HDMI_SCAN_MODE_NONE,
	HDMI_SCAN_MODE_OVERSCAN,
	HDMI_SCAN_MODE_UNDERSCAN,
};

enum hdmi_colorimetry {
	HDMI_COLORIMETRY_NONE,
	HDMI_COLORIMETRY_ITU_601,
	HDMI_COLORIMETRY_ITU_709,
	HDMI_COLORIMETRY_EXTENDED,
};

enum hdmi_picture_aspect {
	HDMI_PICTURE_ASPECT_NONE,
	HDMI_PICTURE_ASPECT_4_3,
	HDMI_PICTURE_ASPECT_16_9,
};

enum hdmi_active_aspect {
	HDMI_ACTIVE_ASPECT_16_9_TOP = 2,
	HDMI_ACTIVE_ASPECT_14_9_TOP = 3,
	HDMI_ACTIVE_ASPECT_16_9_CENTER = 4,
	HDMI_ACTIVE_ASPECT_PICTURE = 8,
	HDMI_ACTIVE_ASPECT_4_3 = 9,
	HDMI_ACTIVE_ASPECT_16_9 = 10,
	HDMI_ACTIVE_ASPECT_14_9 = 11,
	HDMI_ACTIVE_ASPECT_4_3_SP_14_9 = 13,
	HDMI_ACTIVE_ASPECT_16_9_SP_14_9 = 14,
	HDMI_ACTIVE_ASPECT_16_9_SP_4_3 = 15,
};

enum hdmi_extended_colorimetry {
	HDMI_EXTENDED_COLORIMETRY_XV_YCC_601,
	HDMI_EXTENDED_COLORIMETRY_XV_YCC_709,
	HDMI_EXTENDED_COLORIMETRY_S_YCC_601,
	HDMI_EXTENDED_COLORIMETRY_ADOBE_YCC_601,
	HDMI_EXTENDED_COLORIMETRY_ADOBE_RGB,
};

enum hdmi_quantization_range {
	HDMI_QUANTIZATION_RANGE_DEFAULT,
	HDMI_QUANTIZATION_RANGE_LIMITED,
	HDMI_QUANTIZATION_RANGE_FULL,
};

/* non-uniform picture scaling */
enum hdmi_nups {
	HDMI_NUPS_UNKNOWN,
	HDMI_NUPS_HORIZONTAL,
	HDMI_NUPS_VERTICAL,
	HDMI_NUPS_BOTH,
};

enum hdmi_ycc_quantization_range {
	HDMI_YCC_QUANTIZATION_RANGE_LIMITED,
	HDMI_YCC_QUANTIZATION_RANGE_FULL,
};

enum hdmi_content_type {
	HDMI_CONTENT_TYPE_NONE,
	HDMI_CONTENT_TYPE_PHOTO,
	HDMI_CONTENT_TYPE_CINEMA,
	HDMI_CONTENT_TYPE_GAME,
};

struct hdmi_avi_infoframe {
	enum hdmi_infoframe_type type;
	unsigned char version;
	unsigned char length;
	enum hdmi_colorspace colorspace;
	bool active_info_valid;
	bool horizontal_bar_valid;
	bool vertical_bar_valid;
	enum hdmi_scan_mode scan_mode;
	enum hdmi_colorimetry colorimetry;
	enum hdmi_picture_aspect picture_aspect;
	enum hdmi_active_aspect active_aspect;
	bool itc;
	enum hdmi_extended_colorimetry extended_colorimetry;
	enum hdmi_quantization_range quantization_range;
	enum hdmi_nups nups;
	unsigned char video_code;
	enum hdmi_ycc_quantization_range ycc_quantization_range;
	enum hdmi_content_type content_type;
	unsigned char pixel_repeat;
	unsigned short top_bar;
	unsigned short bottom_bar;
	unsigned short left_bar;
	unsigned short right_bar;
};

ssize_t hdmi_avi_infoframe_pack(struct hdmi_avi_infoframe *frame, void *buffer,
				size_t size);

#endif /* _DRM_HDMI_H */

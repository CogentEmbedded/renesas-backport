/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/hdmi.h>

/**
 * hdmi_avi_infoframe_pack() - write HDMI AVI infoframe to binary buffer
 * @frame: HDMI AVI infoframe
 * @buffer: destination buffer
 * @size: size of buffer
 *
 * Packs the information contained in the @frame structure into a binary
 * representation that can be written into the corresponding controller
 * registers. Also computes the checksum as required by section 5.3.5 of the
 * HDMI 1.4 specification.
 */
ssize_t hdmi_avi_infoframe_pack(struct hdmi_avi_infoframe *frame, void *buffer,
				size_t size)
{
	u8 *ptr = buffer;
	unsigned int i;
	u8 csum = 0;

	if (frame->length > 0x1f)
		return -EINVAL;

	ptr[0] = frame->type;
	ptr[1] = frame->version;
	ptr[2] = frame->length;
	ptr[3] = 0; /* checksum */
	ptr[4] = ((frame->colorspace & 0x3) << 5) | (frame->scan_mode & 0x3);

	if (frame->active_info_valid)
		ptr[4] |= BIT(4);

	if (frame->horizontal_bar_valid)
		ptr[4] |= BIT(3);

	if (frame->vertical_bar_valid)
		ptr[4] |= BIT(2);

	ptr[5] = ((frame->colorimetry & 0x3) << 6) |
		 ((frame->picture_aspect & 0x3) << 4) |
		 (frame->active_aspect & 0xf);

	ptr[6] = ((frame->extended_colorimetry & 0x7) << 4) |
		 ((frame->quantization_range & 0x3) << 2) |
		 (frame->nups & 0x3);

	if (frame->itc)
		ptr[6] |= BIT(7);

	ptr[7] = frame->video_code & 0x7f;

	ptr[8] = ((frame->ycc_quantization_range & 0x3) << 6) |
		 ((frame->content_type & 0x3) << 4) |
		 (frame->pixel_repeat & 0xf);

	ptr[9] = frame->top_bar & 0xff;
	ptr[10] = (frame->top_bar >> 8) & 0xff;
	ptr[11] = frame->bottom_bar & 0xff;
	ptr[12] = (frame->bottom_bar >> 8) & 0xff;
	ptr[13] = frame->left_bar & 0xff;
	ptr[14] = (frame->left_bar >> 8) & 0xff;
	ptr[15] = frame->right_bar & 0xff;
	ptr[16] = (frame->right_bar >> 8) & 0xff;

	/* compute checksum */
	for (i = 0; i < 4 + frame->length; i++)
		csum += ptr[i];

	ptr[3] = 256 - csum;

	return 4 + frame->length;
}
EXPORT_SYMBOL_GPL(hdmi_avi_infoframe_pack);

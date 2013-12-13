/*
 * Copyright (c) 2006-2009 Red Hat Inc.
 * Copyright (c) 2006-2008 Intel Corporation
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 *
 * DRM framebuffer helper functions
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/sysrq.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/module.h>
#include "drmP.h"
#include "drm_crtc.h"
#include "drm_fb_helper.h"
#include "drm_crtc_helper.h"

MODULE_AUTHOR("David Airlie, Jesse Barnes");
MODULE_DESCRIPTION("DRM KMS helper");
MODULE_LICENSE("GPL and additional rights");

static LIST_HEAD(kernel_fb_helper_list);

/**
 * DOC: fbdev helpers
 *
 * The fb helper functions are useful to provide an fbdev on top of a drm kernel
 * mode setting driver. They can be used mostly independantely from the crtc
 * helper functions used by many drivers to implement the kernel mode setting
 * interfaces.
 */

/* simple single crtc case helper function */
int drm_fb_helper_single_add_all_connectors(struct drm_fb_helper *fb_helper)
{
	struct drm_device *dev = fb_helper->dev;
	struct drm_connector *connector;
	int i;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		struct drm_fb_helper_connector *fb_helper_connector;

		fb_helper_connector = kzalloc(sizeof(struct drm_fb_helper_connector), GFP_KERNEL);
		if (!fb_helper_connector)
			goto fail;

		fb_helper_connector->connector = connector;
		fb_helper->connector_info[fb_helper->connector_count++] = fb_helper_connector;
	}
	return 0;
fail:
	for (i = 0; i < fb_helper->connector_count; i++) {
		kfree(fb_helper->connector_info[i]);
		fb_helper->connector_info[i] = NULL;
	}
	fb_helper->connector_count = 0;
	return -ENOMEM;
}
EXPORT_SYMBOL(drm_fb_helper_single_add_all_connectors);

static int drm_fb_helper_parse_command_line(struct drm_fb_helper *fb_helper)
{
	struct drm_fb_helper_connector *fb_helper_conn;
	int i;

	for (i = 0; i < fb_helper->connector_count; i++) {
		struct drm_cmdline_mode *mode;
		struct drm_connector *connector;
		char *option = NULL;

		fb_helper_conn = fb_helper->connector_info[i];
		connector = fb_helper_conn->connector;
		mode = &fb_helper_conn->cmdline_mode;

		/* do something on return - turn off connector maybe */
		if (fb_get_options(drm_get_connector_name(connector), &option))
			continue;

		if (drm_mode_parse_command_line_for_connector(option,
							      connector,
							      mode)) {
			if (mode->force) {
				const char *s;
				switch (mode->force) {
				case DRM_FORCE_OFF:
					s = "OFF";
					break;
				case DRM_FORCE_ON_DIGITAL:
					s = "ON - dig";
					break;
				default:
				case DRM_FORCE_ON:
					s = "ON";
					break;
				}

				DRM_INFO("forcing %s connector %s\n",
					 drm_get_connector_name(connector), s);
				connector->force = mode->force;
			}

			DRM_DEBUG_KMS("cmdline mode for connector %s %dx%d@%dHz%s%s%s\n",
				      drm_get_connector_name(connector),
				      mode->xres, mode->yres,
				      mode->refresh_specified ? mode->refresh : 60,
				      mode->rb ? " reduced blanking" : "",
				      mode->margins ? " with margins" : "",
				      mode->interlace ?  " interlaced" : "");
		}

	}
	return 0;
}

static void drm_fb_helper_save_lut_atomic(struct drm_crtc *crtc, struct drm_fb_helper *helper)
{
	uint16_t *r_base, *g_base, *b_base;
	int i;

	r_base = crtc->gamma_store;
	g_base = r_base + crtc->gamma_size;
	b_base = g_base + crtc->gamma_size;

	for (i = 0; i < crtc->gamma_size; i++)
		helper->funcs->gamma_get(crtc, &r_base[i], &g_base[i], &b_base[i], i);
}

static void drm_fb_helper_restore_lut_atomic(struct drm_crtc *crtc)
{
	uint16_t *r_base, *g_base, *b_base;

	if (crtc->funcs->gamma_set == NULL)
		return;

	r_base = crtc->gamma_store;
	g_base = r_base + crtc->gamma_size;
	b_base = g_base + crtc->gamma_size;

	crtc->funcs->gamma_set(crtc, r_base, g_base, b_base, 0, crtc->gamma_size);
}

int drm_fb_helper_debug_enter(struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct drm_crtc_helper_funcs *funcs;
	int i;

	if (list_empty(&kernel_fb_helper_list))
		return false;

	list_for_each_entry(helper, &kernel_fb_helper_list, kernel_fb_list) {
		for (i = 0; i < helper->crtc_count; i++) {
			struct drm_mode_set *mode_set =
				&helper->crtc_info[i].mode_set;

			if (!mode_set->crtc->enabled)
				continue;

			funcs =	mode_set->crtc->helper_private;
			drm_fb_helper_save_lut_atomic(mode_set->crtc, helper);
			funcs->mode_set_base_atomic(mode_set->crtc,
						    mode_set->fb,
						    mode_set->x,
						    mode_set->y,
						    ENTER_ATOMIC_MODE_SET);
		}
	}

	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_debug_enter);

/* Find the real fb for a given fb helper CRTC */
static struct drm_framebuffer *drm_mode_config_fb(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_crtc *c;

	list_for_each_entry(c, &dev->mode_config.crtc_list, head) {
		if (crtc->base.id == c->base.id)
			return c->fb;
	}

	return NULL;
}

int drm_fb_helper_debug_leave(struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct drm_crtc *crtc;
	struct drm_crtc_helper_funcs *funcs;
	struct drm_framebuffer *fb;
	int i;

	for (i = 0; i < helper->crtc_count; i++) {
		struct drm_mode_set *mode_set = &helper->crtc_info[i].mode_set;
		crtc = mode_set->crtc;
		funcs = crtc->helper_private;
		fb = drm_mode_config_fb(crtc);

		if (!crtc->enabled)
			continue;

		if (!fb) {
			DRM_ERROR("no fb to restore??\n");
			continue;
		}

		drm_fb_helper_restore_lut_atomic(mode_set->crtc);
		funcs->mode_set_base_atomic(mode_set->crtc, fb, crtc->x,
					    crtc->y, LEAVE_ATOMIC_MODE_SET);
	}

	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_debug_leave);

bool drm_fb_helper_restore_fbdev_mode(struct drm_fb_helper *fb_helper)
{
	bool error = false;
	int i, ret;
	for (i = 0; i < fb_helper->crtc_count; i++) {
		struct drm_mode_set *mode_set = &fb_helper->crtc_info[i].mode_set;
		ret = drm_mode_set_config_internal(mode_set);
		if (ret)
			error = true;
	}
	return error;
}
EXPORT_SYMBOL(drm_fb_helper_restore_fbdev_mode);

static bool drm_fb_helper_force_kernel_mode(void)
{
	bool ret, error = false;
	struct drm_fb_helper *helper;

	if (list_empty(&kernel_fb_helper_list))
		return false;

	list_for_each_entry(helper, &kernel_fb_helper_list, kernel_fb_list) {
		if (helper->dev->switch_power_state == DRM_SWITCH_POWER_OFF)
			continue;

		ret = drm_fb_helper_restore_fbdev_mode(helper);
		if (ret)
			error = true;
	}
	return error;
}

int drm_fb_helper_panic(struct notifier_block *n, unsigned long ununsed,
			void *panic_str)
{
	/*
	 * It's a waste of time and effort to switch back to text console
	 * if the kernel should reboot before panic messages can be seen.
	 */
	if (panic_timeout < 0)
		return 0;

	pr_err("panic occurred, switching back to text console\n");
	return drm_fb_helper_force_kernel_mode();
}
EXPORT_SYMBOL(drm_fb_helper_panic);

static struct notifier_block paniced = {
	.notifier_call = drm_fb_helper_panic,
};

/**
 * drm_fb_helper_restore - restore the framebuffer console (kernel) config
 *
 * Restore's the kernel's fbcon mode, used for lastclose & panic paths.
 */
void drm_fb_helper_restore(void)
{
	bool ret;
	ret = drm_fb_helper_force_kernel_mode();
	if (ret == true)
		DRM_ERROR("Failed to restore crtc configuration\n");
}
EXPORT_SYMBOL(drm_fb_helper_restore);

static bool drm_fb_helper_is_bound(struct drm_fb_helper *fb_helper)
{
	struct drm_device *dev = fb_helper->dev;
	struct drm_crtc *crtc;
	int bound = 0, crtcs_bound = 0;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc->fb)
			crtcs_bound++;
		if (crtc->fb == fb_helper->fb)
			bound++;
	}

	if (bound < crtcs_bound)
		return false;
	return true;
}

#ifdef CONFIG_MAGIC_SYSRQ
static void drm_fb_helper_restore_work_fn(struct work_struct *ignored)
{
	drm_fb_helper_restore();
}
static DECLARE_WORK(drm_fb_helper_restore_work, drm_fb_helper_restore_work_fn);

static void drm_fb_helper_sysrq(int dummy1)
{
	schedule_work(&drm_fb_helper_restore_work);
}

static struct sysrq_key_op sysrq_drm_fb_helper_restore_op = {
	.handler = drm_fb_helper_sysrq,
	.help_msg = "force-fb(V)",
	.action_msg = "Restore framebuffer console",
};
#else
static struct sysrq_key_op sysrq_drm_fb_helper_restore_op = { };
#endif

static void drm_fb_helper_dpms(struct fb_info *info, int dpms_mode)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	int i, j;

	/*
	 * For each CRTC in this fb, turn the connectors on/off.
	 */
	drm_modeset_lock_all(dev);
#if !defined(CONFIG_DRM_FBDEV_CRTC)
	if (!drm_fb_helper_is_bound(fb_helper)) {
		drm_modeset_unlock_all(dev);
		return;
	}
#endif

	for (i = 0; i < fb_helper->crtc_count; i++) {
		crtc = fb_helper->crtc_info[i].mode_set.crtc;

		if (!crtc->enabled)
			continue;

		/* Walk the connectors & encoders on this fb turning them on/off */
		for (j = 0; j < fb_helper->connector_count; j++) {
			connector = fb_helper->connector_info[j]->connector;
			connector->funcs->dpms(connector, dpms_mode);
			drm_object_property_set_value(&connector->base,
				dev->mode_config.dpms_property, dpms_mode);
		}
	}
	drm_modeset_unlock_all(dev);
}

int drm_fb_helper_blank(int blank, struct fb_info *info)
{
	switch (blank) {
	/* Display: On; HSync: On, VSync: On */
	case FB_BLANK_UNBLANK:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_ON);
		break;
	/* Display: Off; HSync: On, VSync: On */
	case FB_BLANK_NORMAL:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_STANDBY);
		break;
	/* Display: Off; HSync: Off, VSync: On */
	case FB_BLANK_HSYNC_SUSPEND:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_STANDBY);
		break;
	/* Display: Off; HSync: On, VSync: Off */
	case FB_BLANK_VSYNC_SUSPEND:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_SUSPEND);
		break;
	/* Display: Off; HSync: Off, VSync: Off */
	case FB_BLANK_POWERDOWN:
		drm_fb_helper_dpms(info, DRM_MODE_DPMS_OFF);
		break;
	}
	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_blank);

static void drm_fb_helper_crtc_free(struct drm_fb_helper *helper)
{
	int i;

	for (i = 0; i < helper->connector_count; i++)
		kfree(helper->connector_info[i]);
	kfree(helper->connector_info);
	for (i = 0; i < helper->crtc_count; i++) {
		kfree(helper->crtc_info[i].mode_set.connectors);
		if (helper->crtc_info[i].mode_set.mode)
			drm_mode_destroy(helper->dev, helper->crtc_info[i].mode_set.mode);
	}
	kfree(helper->crtc_info);
}

int drm_fb_helper_init(struct drm_device *dev,
		       struct drm_fb_helper *fb_helper,
		       int crtc_count, int max_conn_count)
{
	struct drm_crtc *crtc;
	int i;

	fb_helper->dev = dev;

	INIT_LIST_HEAD(&fb_helper->kernel_fb_list);

	fb_helper->crtc_info = kcalloc(crtc_count, sizeof(struct drm_fb_helper_crtc), GFP_KERNEL);
	if (!fb_helper->crtc_info)
		return -ENOMEM;

	fb_helper->crtc_count = crtc_count;
	fb_helper->connector_info = kcalloc(dev->mode_config.num_connector, sizeof(struct drm_fb_helper_connector *), GFP_KERNEL);
	if (!fb_helper->connector_info) {
		kfree(fb_helper->crtc_info);
		return -ENOMEM;
	}
	fb_helper->connector_count = 0;

	for (i = 0; i < crtc_count; i++) {
		fb_helper->crtc_info[i].mode_set.connectors =
			kcalloc(max_conn_count,
				sizeof(struct drm_connector *),
				GFP_KERNEL);

		if (!fb_helper->crtc_info[i].mode_set.connectors)
			goto out_free;
		fb_helper->crtc_info[i].mode_set.num_connectors = 0;
	}

	i = 0;
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		fb_helper->crtc_info[i].mode_set.crtc = crtc;
		i++;
	}

	return 0;
out_free:
	drm_fb_helper_crtc_free(fb_helper);
	return -ENOMEM;
}
EXPORT_SYMBOL(drm_fb_helper_init);

void drm_fb_helper_fini(struct drm_fb_helper *fb_helper)
{
	if (!list_empty(&fb_helper->kernel_fb_list)) {
		list_del(&fb_helper->kernel_fb_list);
		if (list_empty(&kernel_fb_helper_list)) {
			pr_info("drm: unregistered panic notifier\n");
			atomic_notifier_chain_unregister(&panic_notifier_list,
							 &paniced);
			unregister_sysrq_key('v', &sysrq_drm_fb_helper_restore_op);
		}
	}

	drm_fb_helper_crtc_free(fb_helper);

}
EXPORT_SYMBOL(drm_fb_helper_fini);

static int setcolreg(struct drm_crtc *crtc, u16 red, u16 green,
		     u16 blue, u16 regno, struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_framebuffer *fb = fb_helper->fb;
	int pindex;

	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 *palette;
		u32 value;
		/* place color in psuedopalette */
		if (regno > 16)
			return -EINVAL;
		palette = (u32 *)info->pseudo_palette;
		red >>= (16 - info->var.red.length);
		green >>= (16 - info->var.green.length);
		blue >>= (16 - info->var.blue.length);
		value = (red << info->var.red.offset) |
			(green << info->var.green.offset) |
			(blue << info->var.blue.offset);
		if (info->var.transp.length > 0) {
			u32 mask = (1 << info->var.transp.length) - 1;
			mask <<= info->var.transp.offset;
			value |= mask;
		}
		palette[regno] = value;
		return 0;
	}

	pindex = regno;

	if (fb->bits_per_pixel == 16) {
		pindex = regno << 3;

		if (fb->depth == 16 && regno > 63)
			return -EINVAL;
		if (fb->depth == 15 && regno > 31)
			return -EINVAL;

		if (fb->depth == 16) {
			u16 r, g, b;
			int i;
			if (regno < 32) {
				for (i = 0; i < 8; i++)
					fb_helper->funcs->gamma_set(crtc, red,
						green, blue, pindex + i);
			}

			fb_helper->funcs->gamma_get(crtc, &r,
						    &g, &b,
						    pindex >> 1);

			for (i = 0; i < 4; i++)
				fb_helper->funcs->gamma_set(crtc, r,
							    green, b,
							    (pindex >> 1) + i);
		}
	}

	if (fb->depth != 16)
		fb_helper->funcs->gamma_set(crtc, red, green, blue, pindex);
	return 0;
}

int drm_fb_helper_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_crtc_helper_funcs *crtc_funcs;
	u16 *red, *green, *blue, *transp;
	struct drm_crtc *crtc;
	int i, j, rc = 0;
	int start;

	for (i = 0; i < fb_helper->crtc_count; i++) {
		crtc = fb_helper->crtc_info[i].mode_set.crtc;
		crtc_funcs = crtc->helper_private;

		red = cmap->red;
		green = cmap->green;
		blue = cmap->blue;
		transp = cmap->transp;
		start = cmap->start;

		for (j = 0; j < cmap->len; j++) {
			u16 hred, hgreen, hblue, htransp = 0xffff;

			hred = *red++;
			hgreen = *green++;
			hblue = *blue++;

			if (transp)
				htransp = *transp++;

			rc = setcolreg(crtc, hred, hgreen, hblue, start++, info);
			if (rc)
				return rc;
		}
		crtc_funcs->load_lut(crtc);
	}
	return rc;
}
EXPORT_SYMBOL(drm_fb_helper_setcmap);

int drm_fb_helper_check_var(struct fb_var_screeninfo *var,
			    struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_framebuffer *fb = fb_helper->fb;
	int depth;

#if defined(CONFIG_DRM_FBDEV_CRTC)
	if (in_dbg_master())
		return -EINVAL;
#else
	if (var->pixclock != 0 || in_dbg_master())
		return -EINVAL;
#endif

#if !defined(CONFIG_DRM_FBDEV_CRTC)
	/* Need to resize the fb object !!! */
	if (var->bits_per_pixel > fb->bits_per_pixel ||
	    var->xres > fb->width || var->yres > fb->height ||
	    var->xres_virtual > fb->width || var->yres_virtual > fb->height) {
		DRM_DEBUG("fb userspace requested width/height/bpp is greater than current fb "
			  "request %dx%d-%d (virtual %dx%d) > %dx%d-%d\n",
			  var->xres, var->yres, var->bits_per_pixel,
			  var->xres_virtual, var->yres_virtual,
			  fb->width, fb->height, fb->bits_per_pixel);
		return -EINVAL;
	}
#endif

	switch (var->bits_per_pixel) {
	case 16:
		depth = (var->green.length == 5) ? 15 : 16;
		break;
	case 32:
		depth = (var->transp.length > 0) ? 32 : 24;
		break;
	default:
		depth = var->bits_per_pixel;
		break;
	}

	switch (depth) {
	case 8:
		var->red.offset = 0;
		var->green.offset = 0;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 15:
		var->red.offset = 10;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.length = 1;
		var->transp.offset = 15;
		break;
	case 16:
		var->red.offset = 11;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 24:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 32:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 8;
		var->transp.offset = 24;
		break;
	default:
		return -EINVAL;
	}
#if defined(CONFIG_DRM_FBDEV_CRTC)
	fb->depth = depth;
#endif
	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_check_var);

/* this will let fbcon do the mode init */
int drm_fb_helper_set_par(struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	struct fb_var_screeninfo *var = &info->var;
	struct drm_crtc *crtc;
	int ret;
	int i;
#if defined(CONFIG_DRM_FBDEV_CRTC)
	struct drm_display_mode *disp_set_mode;
	struct drm_display_mode *ref_disp_mode = NULL;
	struct drm_connector *disp_conn;
	struct drm_framebuffer *fb;
	unsigned int bytes_per_pixel;
	unsigned int match_flag;
#endif

#if !defined(CONFIG_DRM_FBDEV_CRTC)
	if (var->pixclock != 0) {
		DRM_ERROR("PIXEL CLOCK SET\n");
		return -EINVAL;
	}
#endif

	drm_modeset_lock_all(dev);
	for (i = 0; i < fb_helper->crtc_count; i++) {
		crtc = fb_helper->crtc_info[i].mode_set.crtc;
		ret = drm_mode_set_config_internal(&fb_helper->crtc_info[i].mode_set);
		if (ret) {
			drm_modeset_unlock_all(dev);
			return ret;
		}
	}
#if defined(CONFIG_DRM_FBDEV_CRTC)
	disp_set_mode =
		fb_helper->crtc_info[CONFIG_DRM_FBDEV_CRTC_NUM].mode_set.mode;
	disp_conn =
		fb_helper->connector_info[CONFIG_DRM_FBDEV_CRTC_NUM]->connector;
	fb = fb_helper->crtc_info[CONFIG_DRM_FBDEV_CRTC_NUM].mode_set.fb;

	if (disp_set_mode && disp_conn && fb &&
		 ((info->flags & FBINFO_MISC_USEREVENT)
			 == FBINFO_MISC_USEREVENT)) {

		match_flag = 0;
		list_for_each_entry(ref_disp_mode, &disp_conn->modes, head) {
			if (((var->vmode & FB_VMODE_MASK) == true) &&
			    !(ref_disp_mode->flags & DRM_MODE_FLAG_INTERLACE))
				continue;
			if ((var->xres == ref_disp_mode->hdisplay)
			    && (var->yres == ref_disp_mode->vdisplay)) {
				match_flag = 1;
				break;
			}
		}
		if (match_flag == 0) {
			drm_modeset_unlock_all(dev);
			return -EINVAL;
		}

		strcpy(disp_set_mode->name, ref_disp_mode->name);
		disp_set_mode->type = ref_disp_mode->type;
		disp_set_mode->clock = ref_disp_mode->clock;
		disp_set_mode->hdisplay = ref_disp_mode->hdisplay;
		disp_set_mode->hsync_start = ref_disp_mode->hsync_start;
		disp_set_mode->hsync_end = ref_disp_mode->hsync_end;
		disp_set_mode->htotal = ref_disp_mode->htotal;
		disp_set_mode->hskew = ref_disp_mode->hskew;
		disp_set_mode->vdisplay = ref_disp_mode->vdisplay;
		disp_set_mode->vsync_start = ref_disp_mode->vsync_start;
		disp_set_mode->vsync_end = ref_disp_mode->vsync_end;
		disp_set_mode->vtotal = ref_disp_mode->vtotal;
		disp_set_mode->vscan = ref_disp_mode->vscan;
		disp_set_mode->flags = ref_disp_mode->flags;
		disp_set_mode->base.type = DRM_MODE_OBJECT_MODE;

		bytes_per_pixel = DIV_ROUND_UP(var->bits_per_pixel, 8);
		fb->width = var->xres_virtual;
		fb->height = var->yres_virtual;
		fb->bits_per_pixel = var->bits_per_pixel;
		fb->pitches[0] = var->xres * bytes_per_pixel;
		fb->pixel_format = drm_mode_legacy_fb_format(
					fb->bits_per_pixel, fb->depth);
		drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
		drm_fb_helper_fill_var(info, fb_helper,
					 var->xres, var->yres);
		disp_set_mode->private_flags = true;
		ret = drm_mode_set_config_internal(&fb_helper->
				crtc_info[CONFIG_DRM_FBDEV_CRTC_NUM].mode_set);
		if (ret) {
			drm_modeset_unlock_all(dev);
			return ret;
		}
	}
#endif
	drm_modeset_unlock_all(dev);

	if (fb_helper->delayed_hotplug) {
		fb_helper->delayed_hotplug = false;
		drm_fb_helper_hotplug_event(fb_helper);
	}
	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_set_par);

int drm_fb_helper_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	struct drm_mode_set *modeset;
	struct drm_crtc *crtc;
	int ret = 0;
	int i;

	drm_modeset_lock_all(dev);
#if !defined(CONFIG_DRM_FBDEV_CRTC)
	if (!drm_fb_helper_is_bound(fb_helper)) {
		drm_modeset_unlock_all(dev);
		return -EBUSY;
	}
#endif

	for (i = 0; i < fb_helper->crtc_count; i++) {
		crtc = fb_helper->crtc_info[i].mode_set.crtc;

		modeset = &fb_helper->crtc_info[i].mode_set;

#if defined(CONFIG_DRM_FBDEV_CRTC)
		if (crtc->base.id != crtc->flip_id) {
			modeset->x = var->xoffset;
			modeset->y = var->yoffset;

			if (modeset->num_connectors) {
				ret = drm_mode_set_config_internal(modeset);
				if (!ret) {
					info->var.xoffset = var->xoffset;
					info->var.yoffset = var->yoffset;
				}
			}
		}
#else
		modeset->x = var->xoffset;
		modeset->y = var->yoffset;

		if (modeset->num_connectors) {
			ret = drm_mode_set_config_internal(modeset);
			if (!ret) {
				info->var.xoffset = var->xoffset;
				info->var.yoffset = var->yoffset;
			}
		}
#endif
	}
	drm_modeset_unlock_all(dev);
	return ret;
}
EXPORT_SYMBOL(drm_fb_helper_pan_display);

static int drm_fb_helper_wait_for_vsync(struct drm_fb_helper *fb_helper)
{
	struct drm_device *dev = fb_helper->dev;
	union drm_wait_vblank vblwait;

	/* Waiting for VSYNC on multiple CRTCs doesn't make sense. Use the first
	 * CRTC only.
	 */
	vblwait.request.type = _DRM_VBLANK_RELATIVE;
	vblwait.request.sequence = 1;

	return drm_wait_vblank(dev, &vblwait, NULL);
}

int drm_fb_helper_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct drm_fb_helper *fb_helper = info->par;

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		return drm_fb_helper_wait_for_vsync(fb_helper);

	default:
		return -ENOIOCTLCMD;
	}
}
EXPORT_SYMBOL(drm_fb_helper_ioctl);

int drm_fb_helper_single_fb_probe(struct drm_fb_helper *fb_helper,
				  int preferred_bpp)
{
	int new_fb = 0;
	int crtc_count = 0;
	int i;
	struct fb_info *info;
	struct drm_fb_helper_surface_size sizes;
	int gamma_size = 0;
#if defined(CONFIG_DRM_RCAR_DU)
	int des_hdisplay, des_vdisplay = 0;
#endif

	memset(&sizes, 0, sizeof(struct drm_fb_helper_surface_size));
#if defined(CONFIG_DRM_RCAR_DU)
	sizes.surface_depth = 32;
#else
	sizes.surface_depth = 24;
#endif
	sizes.surface_bpp = 32;
	sizes.fb_width = (unsigned)-1;
	sizes.fb_height = (unsigned)-1;

#if defined(CONFIG_DRM_RCAR_DU)
	des_hdisplay = (unsigned)-1;
	des_vdisplay = (unsigned)-1;
#endif
	/* if driver picks 8 or 16 by default use that
	   for both depth/bpp */
	if (preferred_bpp != sizes.surface_bpp)
		sizes.surface_depth = sizes.surface_bpp = preferred_bpp;

	/* first up get a count of crtcs now in use and new min/maxes width/heights */
	for (i = 0; i < fb_helper->connector_count; i++) {
		struct drm_fb_helper_connector *fb_helper_conn = fb_helper->connector_info[i];
		struct drm_cmdline_mode *cmdline_mode;

		cmdline_mode = &fb_helper_conn->cmdline_mode;

		if (cmdline_mode->bpp_specified) {
			switch (cmdline_mode->bpp) {
			case 8:
				sizes.surface_depth = sizes.surface_bpp = 8;
				break;
			case 15:
				sizes.surface_depth = 15;
				sizes.surface_bpp = 16;
				break;
			case 16:
				sizes.surface_depth = sizes.surface_bpp = 16;
				break;
			case 24:
				sizes.surface_depth = sizes.surface_bpp = 24;
				break;
			case 32:
				sizes.surface_depth = 24;
				sizes.surface_bpp = 32;
				break;
			}
			break;
		}
	}

	crtc_count = 0;
	for (i = 0; i < fb_helper->crtc_count; i++) {
		struct drm_display_mode *desired_mode;
		desired_mode = fb_helper->crtc_info[i].desired_mode;

#if defined(CONFIG_DRM_FBDEV_CRTC)
		if ((desired_mode) && (i == CONFIG_DRM_FBDEV_CRTC_NUM)) {
#else
		if (desired_mode) {
#endif
			if (gamma_size == 0)
				gamma_size = fb_helper->crtc_info[i].mode_set.crtc->gamma_size;
			if (desired_mode->hdisplay < sizes.fb_width)
				sizes.fb_width = desired_mode->hdisplay;
			if (desired_mode->vdisplay < sizes.fb_height)
				sizes.fb_height = desired_mode->vdisplay;
			if (desired_mode->hdisplay > sizes.surface_width)
				sizes.surface_width = desired_mode->hdisplay;
			if (desired_mode->vdisplay > sizes.surface_height)
				sizes.surface_height = desired_mode->vdisplay;
			crtc_count++;
#if defined(CONFIG_DRM_RCAR_DU)
			des_hdisplay = desired_mode->hdisplay;
			des_vdisplay = desired_mode->vdisplay;
#endif
		}
	}

#if defined(CONFIG_DRM_RCAR_DU)
	if (crtc_count == 0) {
		sizes.fb_width = sizes.surface_width = 1920;
		sizes.fb_height = sizes.surface_height = 1080;
		crtc_count++;
	}
#endif
	if (crtc_count == 0 || sizes.fb_width == -1 || sizes.fb_height == -1) {
		/* hmm everyone went away - assume VGA cable just fell out
		   and will come back later. */
		DRM_INFO("Cannot find any crtc or sizes - going 1024x768\n");
		sizes.fb_width = sizes.surface_width = 1024;
		sizes.fb_height = sizes.surface_height = 768;
	}
#if defined(CONFIG_DRM_RCAR_DU)
	if (fb_helper->fbdev) {
		if (fb_helper->fbdev->var.xres != des_hdisplay) {
			sizes.fb_width = sizes.surface_width = des_hdisplay;
			fb_helper->fb->width = sizes.fb_width;
			fb_helper->fb->pitches[0]
				 = des_hdisplay * sizes.surface_bpp / 8;
		}
		if (fb_helper->fbdev->var.yres != des_vdisplay) {
			sizes.fb_height = sizes.surface_height = des_vdisplay;
			fb_helper->fb->height = sizes.fb_height;
		}
	}
#endif
	/* push down into drivers */
	new_fb = (*fb_helper->funcs->fb_probe)(fb_helper, &sizes);
	if (new_fb < 0)
		return new_fb;

	info = fb_helper->fbdev;

	/* set the fb pointer */
	for (i = 0; i < fb_helper->crtc_count; i++)
		fb_helper->crtc_info[i].mode_set.fb = fb_helper->fb;

	if (new_fb) {
#if !defined(CONFIG_DRM_FBDEV_CRTC)
		info->var.pixclock = 0;
#endif
		if (register_framebuffer(info) < 0)
			return -EINVAL;

		dev_info(fb_helper->dev->dev, "fb%d: %s frame buffer device\n",
				info->node, info->fix.id);

	} else {
		drm_fb_helper_set_par(info);
	}

	/* Switch back to kernel console on panic */
	/* multi card linked list maybe */
	if (list_empty(&kernel_fb_helper_list)) {
		dev_info(fb_helper->dev->dev, "registered panic notifier\n");
		atomic_notifier_chain_register(&panic_notifier_list,
					       &paniced);
		register_sysrq_key('v', &sysrq_drm_fb_helper_restore_op);
	}
	if (new_fb)
		list_add(&fb_helper->kernel_fb_list, &kernel_fb_helper_list);

#if defined(CONFIG_DRM_FBDEV_CRTC)
	drm_fb_helper_fill_fix(info,
			 fb_helper->fb->pitches[0], fb_helper->fb->depth);
	drm_fb_helper_fill_var(info,
			 fb_helper, sizes.fb_width, sizes.fb_height);
#endif
	return 0;
}
EXPORT_SYMBOL(drm_fb_helper_single_fb_probe);

void drm_fb_helper_fill_fix(struct fb_info *info, uint32_t pitch,
			    uint32_t depth)
{
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = depth == 8 ? FB_VISUAL_PSEUDOCOLOR :
		FB_VISUAL_TRUECOLOR;
	info->fix.mmio_start = 0;
	info->fix.mmio_len = 0;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 1; /* doing it in hw */
	info->fix.ypanstep = 1; /* doing it in hw */
	info->fix.ywrapstep = 1;
	info->fix.accel = FB_ACCEL_NONE;
	info->fix.type_aux = 0;

	info->fix.line_length = pitch;
	return;
}
EXPORT_SYMBOL(drm_fb_helper_fill_fix);

void drm_fb_helper_fill_var(struct fb_info *info, struct drm_fb_helper *fb_helper,
			    uint32_t fb_width, uint32_t fb_height)
{
	struct drm_framebuffer *fb = fb_helper->fb;
#if defined(CONFIG_DRM_FBDEV_CRTC)
	struct drm_display_mode *drm_mode;
#endif
	info->pseudo_palette = fb_helper->pseudo_palette;
	info->var.xres_virtual = fb->width;
	info->var.yres_virtual = fb->height;
	info->var.bits_per_pixel = fb->bits_per_pixel;
	info->var.accel_flags = FB_ACCELF_TEXT;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.activate = FB_ACTIVATE_NOW;
	info->var.height = -1;
	info->var.width = -1;

#if defined(CONFIG_DRM_FBDEV_CRTC)
	drm_mode =
		 fb_helper->crtc_info[CONFIG_DRM_FBDEV_CRTC_NUM].mode_set.mode;
	if (drm_mode) {
		info->var.left_margin = drm_mode->htotal - drm_mode->hsync_end;
		info->var.right_margin =
			 drm_mode->hsync_start - drm_mode->hdisplay;
		info->var.upper_margin = drm_mode->vtotal - drm_mode->vsync_end;
		info->var.lower_margin =
			 drm_mode->vsync_start - drm_mode->vdisplay;
		info->var.hsync_len =
			 drm_mode->hsync_end - drm_mode->hsync_start;
		info->var.vsync_len =
			 drm_mode->vsync_end - drm_mode->vsync_start;
		info->var.pixclock = 1000000000 / drm_mode->clock;
		if (drm_mode->flags & DRM_MODE_FLAG_INTERLACE)
			info->var.vmode = FB_VMODE_INTERLACED;
		else
			info->var.vmode = FB_VMODE_NONINTERLACED;
	}
#endif
	switch (fb->depth) {
	case 8:
		info->var.red.offset = 0;
		info->var.green.offset = 0;
		info->var.blue.offset = 0;
		info->var.red.length = 8; /* 8bit DAC */
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 0;
		info->var.transp.length = 0;
		break;
	case 15:
		info->var.red.offset = 10;
		info->var.green.offset = 5;
		info->var.blue.offset = 0;
		info->var.red.length = 5;
		info->var.green.length = 5;
		info->var.blue.length = 5;
		info->var.transp.offset = 15;
		info->var.transp.length = 1;
		break;
	case 16:
		info->var.red.offset = 11;
		info->var.green.offset = 5;
		info->var.blue.offset = 0;
		info->var.red.length = 5;
		info->var.green.length = 6;
		info->var.blue.length = 5;
		info->var.transp.offset = 0;
		break;
	case 24:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = 8;
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 0;
		info->var.transp.length = 0;
		break;
	case 32:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = 8;
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.transp.offset = 24;
		info->var.transp.length = 8;
		break;
	default:
		break;
	}

	info->var.xres = fb_width;
	info->var.yres = fb_height;
}
EXPORT_SYMBOL(drm_fb_helper_fill_var);

static int drm_fb_helper_probe_connector_modes(struct drm_fb_helper *fb_helper,
					       uint32_t maxX,
					       uint32_t maxY)
{
	struct drm_connector *connector;
	int count = 0;
	int i;
	struct drm_display_mode *cur_mode;
	struct drm_cmdline_mode *cmdline_mode;
	bool match_flag = false;

	for (i = 0; i < fb_helper->connector_count; i++) {
		connector = fb_helper->connector_info[i]->connector;
		cmdline_mode = &fb_helper->connector_info[i]->cmdline_mode;
		count += connector->funcs->fill_modes(connector, maxX, maxY);
		if (cmdline_mode->specified) {
			list_for_each_entry(cur_mode, &connector->modes, head) {
				if ((cur_mode->hdisplay != cmdline_mode->xres)
					|| (cur_mode->vdisplay
					!= cmdline_mode->yres))
					continue;
				if (cmdline_mode->interlace) {
					if (!(cur_mode->flags
						 & DRM_MODE_FLAG_INTERLACE))
						continue;
				} else {
					if (cur_mode->flags
						 & DRM_MODE_FLAG_INTERLACE)
						continue;
				}
				match_flag = true;
				break;
			}
			if (!match_flag) {
				printk(KERN_ERR
				 "Error! parse setting(%dx%d),laced:%d\n",
				cmdline_mode->xres, cmdline_mode->yres,
				cmdline_mode->interlace);
				return -EINVAL;
			}
		}
	}

	return count;
}

static struct drm_display_mode *drm_has_preferred_mode(struct drm_fb_helper_connector *fb_connector, int width, int height)
{
	struct drm_display_mode *mode;

	list_for_each_entry(mode, &fb_connector->connector->modes, head) {
		if (drm_mode_width(mode) > width ||
		    drm_mode_height(mode) > height)
			continue;
		if (mode->type & DRM_MODE_TYPE_PREFERRED)
			return mode;
	}
	return NULL;
}

static bool drm_has_cmdline_mode(struct drm_fb_helper_connector *fb_connector)
{
	struct drm_cmdline_mode *cmdline_mode;
	cmdline_mode = &fb_connector->cmdline_mode;
	return cmdline_mode->specified;
}

static struct drm_display_mode *drm_pick_cmdline_mode(struct drm_fb_helper_connector *fb_helper_conn,
						      int width, int height)
{
	struct drm_cmdline_mode *cmdline_mode;
	struct drm_display_mode *mode = NULL;

	cmdline_mode = &fb_helper_conn->cmdline_mode;
	if (cmdline_mode->specified == false)
		return mode;

	/* attempt to find a matching mode in the list of modes
	 *  we have gotten so far, if not add a CVT mode that conforms
	 */
	if (cmdline_mode->rb || cmdline_mode->margins)
		goto create_mode;

	list_for_each_entry(mode, &fb_helper_conn->connector->modes, head) {
		/* check width/height */
		if (mode->hdisplay != cmdline_mode->xres ||
		    mode->vdisplay != cmdline_mode->yres)
			continue;

		if (cmdline_mode->refresh_specified) {
			if (mode->vrefresh != cmdline_mode->refresh)
				continue;
		}

		if (cmdline_mode->interlace) {
			if (!(mode->flags & DRM_MODE_FLAG_INTERLACE))
				continue;
		}
		return mode;
	}

create_mode:
	mode = drm_mode_create_from_cmdline_mode(fb_helper_conn->connector->dev,
						 cmdline_mode);
	list_add(&mode->head, &fb_helper_conn->connector->modes);
	return mode;
}

static bool drm_connector_enabled(struct drm_connector *connector, bool strict)
{
	bool enable;

	if (strict)
		enable = connector->status == connector_status_connected;
	else
		enable = connector->status != connector_status_disconnected;

	return enable;
}

static void drm_enable_connectors(struct drm_fb_helper *fb_helper,
				  bool *enabled)
{
	bool any_enabled = false;
	struct drm_connector *connector;
	int i = 0;

	for (i = 0; i < fb_helper->connector_count; i++) {
		connector = fb_helper->connector_info[i]->connector;
		enabled[i] = drm_connector_enabled(connector, true);
		DRM_DEBUG_KMS("connector %d enabled? %s\n", connector->base.id,
			  enabled[i] ? "yes" : "no");
		any_enabled |= enabled[i];
	}

	if (any_enabled)
		return;

	for (i = 0; i < fb_helper->connector_count; i++) {
		connector = fb_helper->connector_info[i]->connector;
		enabled[i] = drm_connector_enabled(connector, false);
	}
}

static bool drm_target_cloned(struct drm_fb_helper *fb_helper,
			      struct drm_display_mode **modes,
			      bool *enabled, int width, int height)
{
	int count, i, j;
	bool can_clone = false;
	struct drm_fb_helper_connector *fb_helper_conn;
	struct drm_display_mode *dmt_mode, *mode;

	/* only contemplate cloning in the single crtc case */
	if (fb_helper->crtc_count > 1)
		return false;

	count = 0;
	for (i = 0; i < fb_helper->connector_count; i++) {
		if (enabled[i])
			count++;
	}

	/* only contemplate cloning if more than one connector is enabled */
	if (count <= 1)
		return false;

	/* check the command line or if nothing common pick 1024x768 */
	can_clone = true;
	for (i = 0; i < fb_helper->connector_count; i++) {
		if (!enabled[i])
			continue;
		fb_helper_conn = fb_helper->connector_info[i];
		modes[i] = drm_pick_cmdline_mode(fb_helper_conn, width, height);
		if (!modes[i]) {
			can_clone = false;
			break;
		}
		for (j = 0; j < i; j++) {
			if (!enabled[j])
				continue;
			if (!drm_mode_equal(modes[j], modes[i]))
				can_clone = false;
		}
	}

	if (can_clone) {
		DRM_DEBUG_KMS("can clone using command line\n");
		return true;
	}

	/* try and find a 1024x768 mode on each connector */
	can_clone = true;
	dmt_mode = drm_mode_find_dmt(fb_helper->dev, 1024, 768, 60, false);

	for (i = 0; i < fb_helper->connector_count; i++) {

		if (!enabled[i])
			continue;

		fb_helper_conn = fb_helper->connector_info[i];
		list_for_each_entry(mode, &fb_helper_conn->connector->modes, head) {
			if (drm_mode_equal(mode, dmt_mode))
				modes[i] = mode;
		}
		if (!modes[i])
			can_clone = false;
	}

	if (can_clone) {
		DRM_DEBUG_KMS("can clone using 1024x768\n");
		return true;
	}
	DRM_INFO("kms: can't enable cloning when we probably wanted to.\n");
	return false;
}

static bool drm_target_preferred(struct drm_fb_helper *fb_helper,
				 struct drm_display_mode **modes,
				 bool *enabled, int width, int height)
{
	struct drm_fb_helper_connector *fb_helper_conn;
	int i;

	for (i = 0; i < fb_helper->connector_count; i++) {
		fb_helper_conn = fb_helper->connector_info[i];

		if (enabled[i] == false)
			continue;

		DRM_DEBUG_KMS("looking for cmdline mode on connector %d\n",
			      fb_helper_conn->connector->base.id);

		/* got for command line mode first */
		modes[i] = drm_pick_cmdline_mode(fb_helper_conn, width, height);
		if (!modes[i]) {
			DRM_DEBUG_KMS("looking for preferred mode on connector %d\n",
				      fb_helper_conn->connector->base.id);
			modes[i] = drm_has_preferred_mode(fb_helper_conn, width, height);
		}
		/* No preferred modes, pick one off the list */
		if (!modes[i] && !list_empty(&fb_helper_conn->connector->modes)) {
			list_for_each_entry(modes[i], &fb_helper_conn->connector->modes, head)
				break;
		}
		DRM_DEBUG_KMS("found mode %s\n", modes[i] ? modes[i]->name :
			  "none");
	}
	return true;
}

static int drm_pick_crtcs(struct drm_fb_helper *fb_helper,
			  struct drm_fb_helper_crtc **best_crtcs,
			  struct drm_display_mode **modes,
			  int n, int width, int height)
{
	int c, o;
	struct drm_device *dev = fb_helper->dev;
	struct drm_connector *connector;
	struct drm_connector_helper_funcs *connector_funcs;
	struct drm_encoder *encoder;
	struct drm_fb_helper_crtc *best_crtc;
	int my_score, best_score, score;
	struct drm_fb_helper_crtc **crtcs, *crtc;
	struct drm_fb_helper_connector *fb_helper_conn;

	if (n == fb_helper->connector_count)
		return 0;

	fb_helper_conn = fb_helper->connector_info[n];
	connector = fb_helper_conn->connector;

	best_crtcs[n] = NULL;
	best_crtc = NULL;
	best_score = drm_pick_crtcs(fb_helper, best_crtcs, modes, n+1, width, height);
	if (modes[n] == NULL)
		return best_score;

	crtcs = kzalloc(dev->mode_config.num_connector *
			sizeof(struct drm_fb_helper_crtc *), GFP_KERNEL);
	if (!crtcs)
		return best_score;

	my_score = 1;
	if (connector->status == connector_status_connected)
		my_score++;
	if (drm_has_cmdline_mode(fb_helper_conn))
		my_score++;
	if (drm_has_preferred_mode(fb_helper_conn, width, height))
		my_score++;

	connector_funcs = connector->helper_private;
	encoder = connector_funcs->best_encoder(connector);
	if (!encoder)
		goto out;

	/* select a crtc for this connector and then attempt to configure
	   remaining connectors */
	for (c = 0; c < fb_helper->crtc_count; c++) {
		crtc = &fb_helper->crtc_info[c];

		if ((encoder->possible_crtcs & (1 << c)) == 0)
			continue;

		for (o = 0; o < n; o++)
			if (best_crtcs[o] == crtc)
				break;

		if (o < n) {
			/* ignore cloning unless only a single crtc */
			if (fb_helper->crtc_count > 1)
				continue;

			if (!drm_mode_equal(modes[o], modes[n]))
				continue;
		}

		crtcs[n] = crtc;
		memcpy(crtcs, best_crtcs, n * sizeof(struct drm_fb_helper_crtc *));
		score = my_score + drm_pick_crtcs(fb_helper, crtcs, modes, n + 1,
						  width, height);
		if (score > best_score) {
			best_crtc = crtc;
			best_score = score;
			memcpy(best_crtcs, crtcs,
			       dev->mode_config.num_connector *
			       sizeof(struct drm_fb_helper_crtc *));
		}
	}
out:
	kfree(crtcs);
	return best_score;
}

static void drm_setup_crtcs(struct drm_fb_helper *fb_helper)
{
	struct drm_device *dev = fb_helper->dev;
	struct drm_fb_helper_crtc **crtcs;
	struct drm_display_mode **modes;
	struct drm_mode_set *modeset;
	bool *enabled;
	int width, height;
	int i, ret;

	DRM_DEBUG_KMS("\n");

	width = dev->mode_config.max_width;
	height = dev->mode_config.max_height;

	crtcs = kcalloc(dev->mode_config.num_connector,
			sizeof(struct drm_fb_helper_crtc *), GFP_KERNEL);
	modes = kcalloc(dev->mode_config.num_connector,
			sizeof(struct drm_display_mode *), GFP_KERNEL);
	enabled = kcalloc(dev->mode_config.num_connector,
			  sizeof(bool), GFP_KERNEL);
	if (!crtcs || !modes || !enabled) {
		DRM_ERROR("Memory allocation failed\n");
		goto out;
	}


	drm_enable_connectors(fb_helper, enabled);

	ret = drm_target_cloned(fb_helper, modes, enabled, width, height);
	if (!ret) {
		ret = drm_target_preferred(fb_helper, modes, enabled, width, height);
		if (!ret)
			DRM_ERROR("Unable to find initial modes\n");
	}

	DRM_DEBUG_KMS("picking CRTCs for %dx%d config\n", width, height);

	drm_pick_crtcs(fb_helper, crtcs, modes, 0, width, height);

	/* need to set the modesets up here for use later */
	/* fill out the connector<->crtc mappings into the modesets */
	for (i = 0; i < fb_helper->crtc_count; i++) {
		modeset = &fb_helper->crtc_info[i].mode_set;
		modeset->num_connectors = 0;
	}

	for (i = 0; i < fb_helper->connector_count; i++) {
		struct drm_display_mode *mode = modes[i];
		struct drm_fb_helper_crtc *fb_crtc = crtcs[i];
		modeset = &fb_crtc->mode_set;

		if (mode && fb_crtc) {
			DRM_DEBUG_KMS("desired mode %s set on crtc %d\n",
				      mode->name, fb_crtc->mode_set.crtc->base.id);
			fb_crtc->desired_mode = mode;
			if (modeset->mode)
				drm_mode_destroy(dev, modeset->mode);
			modeset->mode = drm_mode_duplicate(dev,
							   fb_crtc->desired_mode);
			modeset->connectors[modeset->num_connectors++] = fb_helper->connector_info[i]->connector;
		}
	}

out:
	kfree(crtcs);
	kfree(modes);
	kfree(enabled);
}

/**
 * drm_helper_initial_config - setup a sane initial connector configuration
 * @fb_helper: fb_helper device struct
 * @bpp_sel: bpp value to use for the framebuffer configuration
 *
 * LOCKING:
 * Called at init time by the driver to set up the @fb_helper initial
 * configuration, must take the mode config lock.
 *
 * Scans the CRTCs and connectors and tries to put together an initial setup.
 * At the moment, this is a cloned configuration across all heads with
 * a new framebuffer object as the backing store.
 *
 * RETURNS:
 * Zero if everything went ok, nonzero otherwise.
 */
bool drm_fb_helper_initial_config(struct drm_fb_helper *fb_helper, int bpp_sel)
{
	struct drm_device *dev = fb_helper->dev;
	int count = 0;

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(fb_helper->dev);

	drm_fb_helper_parse_command_line(fb_helper);

	count = drm_fb_helper_probe_connector_modes(fb_helper,
						    dev->mode_config.max_width,
						    dev->mode_config.max_height);
	if (count < 0)
		return -EINVAL;
	/*
	 * we shouldn't end up with no modes here.
	 */
	if (count == 0)
		dev_info(fb_helper->dev->dev, "No connectors reported connected with modes\n");

	drm_setup_crtcs(fb_helper);

	return drm_fb_helper_single_fb_probe(fb_helper, bpp_sel);
}
EXPORT_SYMBOL(drm_fb_helper_initial_config);

/**
 * drm_fb_helper_hotplug_event - respond to a hotplug notification by
 *                               probing all the outputs attached to the fb
 * @fb_helper: the drm_fb_helper
 *
 * LOCKING:
 * Called at runtime, must take mode config lock.
 *
 * Scan the connectors attached to the fb_helper and try to put together a
 * setup after *notification of a change in output configuration.
 *
 * RETURNS:
 * 0 on success and a non-zero error code otherwise.
 */
int drm_fb_helper_hotplug_event(struct drm_fb_helper *fb_helper)
{
	struct drm_device *dev = fb_helper->dev;
	int count = 0;
	u32 max_width, max_height, bpp_sel;

	if (!fb_helper->fb)
		return 0;

	drm_modeset_lock_all(dev);
	if (!drm_fb_helper_is_bound(fb_helper)) {
		fb_helper->delayed_hotplug = true;
		drm_modeset_unlock_all(dev);
		return 0;
	}
	DRM_DEBUG_KMS("\n");

	max_width = fb_helper->fb->width;
	max_height = fb_helper->fb->height;
	bpp_sel = fb_helper->fb->bits_per_pixel;

	count = drm_fb_helper_probe_connector_modes(fb_helper, max_width,
						    max_height);
	drm_setup_crtcs(fb_helper);
	drm_modeset_unlock_all(dev);

	return drm_fb_helper_single_fb_probe(fb_helper, bpp_sel);
}
EXPORT_SYMBOL(drm_fb_helper_hotplug_event);

/* The Kconfig DRM_KMS_HELPER selects FRAMEBUFFER_CONSOLE (if !EXPERT)
 * but the module doesn't depend on any fb console symbols.  At least
 * attempt to load fbcon to avoid leaving the system without a usable console.
 */
#if defined(CONFIG_FRAMEBUFFER_CONSOLE_MODULE) && !defined(CONFIG_EXPERT)
static int __init drm_fb_helper_modinit(void)
{
	const char *name = "fbcon";
	struct module *fbcon;

	mutex_lock(&module_mutex);
	fbcon = find_module(name);
	mutex_unlock(&module_mutex);

	if (!fbcon)
		request_module_nowait(name);
	return 0;
}

module_init(drm_fb_helper_modinit);
#endif

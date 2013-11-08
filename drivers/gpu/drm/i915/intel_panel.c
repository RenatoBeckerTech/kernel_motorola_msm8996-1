/*
 * Copyright © 2006-2010 Intel Corporation
 * Copyright (c) 2006 Dave Airlie <airlied@linux.ie>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 *      Chris Wilson <chris@chris-wilson.co.uk>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/moduleparam.h>
#include "intel_drv.h"

#define PCI_LBPC 0xf4 /* legacy/combination backlight modes */

void
intel_fixed_panel_mode(const struct drm_display_mode *fixed_mode,
		       struct drm_display_mode *adjusted_mode)
{
	drm_mode_copy(adjusted_mode, fixed_mode);

	drm_mode_set_crtcinfo(adjusted_mode, 0);
}

/* adjusted_mode has been preset to be the panel's fixed mode */
void
intel_pch_panel_fitting(struct intel_crtc *intel_crtc,
			struct intel_crtc_config *pipe_config,
			int fitting_mode)
{
	struct drm_display_mode *adjusted_mode;
	int x, y, width, height;

	adjusted_mode = &pipe_config->adjusted_mode;

	x = y = width = height = 0;

	/* Native modes don't need fitting */
	if (adjusted_mode->hdisplay == pipe_config->pipe_src_w &&
	    adjusted_mode->vdisplay == pipe_config->pipe_src_h)
		goto done;

	switch (fitting_mode) {
	case DRM_MODE_SCALE_CENTER:
		width = pipe_config->pipe_src_w;
		height = pipe_config->pipe_src_h;
		x = (adjusted_mode->hdisplay - width + 1)/2;
		y = (adjusted_mode->vdisplay - height + 1)/2;
		break;

	case DRM_MODE_SCALE_ASPECT:
		/* Scale but preserve the aspect ratio */
		{
			u32 scaled_width = adjusted_mode->hdisplay
				* pipe_config->pipe_src_h;
			u32 scaled_height = pipe_config->pipe_src_w
				* adjusted_mode->vdisplay;
			if (scaled_width > scaled_height) { /* pillar */
				width = scaled_height / pipe_config->pipe_src_h;
				if (width & 1)
					width++;
				x = (adjusted_mode->hdisplay - width + 1) / 2;
				y = 0;
				height = adjusted_mode->vdisplay;
			} else if (scaled_width < scaled_height) { /* letter */
				height = scaled_width / pipe_config->pipe_src_w;
				if (height & 1)
				    height++;
				y = (adjusted_mode->vdisplay - height + 1) / 2;
				x = 0;
				width = adjusted_mode->hdisplay;
			} else {
				x = y = 0;
				width = adjusted_mode->hdisplay;
				height = adjusted_mode->vdisplay;
			}
		}
		break;

	case DRM_MODE_SCALE_FULLSCREEN:
		x = y = 0;
		width = adjusted_mode->hdisplay;
		height = adjusted_mode->vdisplay;
		break;

	default:
		WARN(1, "bad panel fit mode: %d\n", fitting_mode);
		return;
	}

done:
	pipe_config->pch_pfit.pos = (x << 16) | y;
	pipe_config->pch_pfit.size = (width << 16) | height;
	pipe_config->pch_pfit.enabled = pipe_config->pch_pfit.size != 0;
}

static void
centre_horizontally(struct drm_display_mode *mode,
		    int width)
{
	u32 border, sync_pos, blank_width, sync_width;

	/* keep the hsync and hblank widths constant */
	sync_width = mode->crtc_hsync_end - mode->crtc_hsync_start;
	blank_width = mode->crtc_hblank_end - mode->crtc_hblank_start;
	sync_pos = (blank_width - sync_width + 1) / 2;

	border = (mode->hdisplay - width + 1) / 2;
	border += border & 1; /* make the border even */

	mode->crtc_hdisplay = width;
	mode->crtc_hblank_start = width + border;
	mode->crtc_hblank_end = mode->crtc_hblank_start + blank_width;

	mode->crtc_hsync_start = mode->crtc_hblank_start + sync_pos;
	mode->crtc_hsync_end = mode->crtc_hsync_start + sync_width;
}

static void
centre_vertically(struct drm_display_mode *mode,
		  int height)
{
	u32 border, sync_pos, blank_width, sync_width;

	/* keep the vsync and vblank widths constant */
	sync_width = mode->crtc_vsync_end - mode->crtc_vsync_start;
	blank_width = mode->crtc_vblank_end - mode->crtc_vblank_start;
	sync_pos = (blank_width - sync_width + 1) / 2;

	border = (mode->vdisplay - height + 1) / 2;

	mode->crtc_vdisplay = height;
	mode->crtc_vblank_start = height + border;
	mode->crtc_vblank_end = mode->crtc_vblank_start + blank_width;

	mode->crtc_vsync_start = mode->crtc_vblank_start + sync_pos;
	mode->crtc_vsync_end = mode->crtc_vsync_start + sync_width;
}

static inline u32 panel_fitter_scaling(u32 source, u32 target)
{
	/*
	 * Floating point operation is not supported. So the FACTOR
	 * is defined, which can avoid the floating point computation
	 * when calculating the panel ratio.
	 */
#define ACCURACY 12
#define FACTOR (1 << ACCURACY)
	u32 ratio = source * FACTOR / target;
	return (FACTOR * ratio + FACTOR/2) / FACTOR;
}

static void i965_scale_aspect(struct intel_crtc_config *pipe_config,
			      u32 *pfit_control)
{
	struct drm_display_mode *adjusted_mode = &pipe_config->adjusted_mode;
	u32 scaled_width = adjusted_mode->hdisplay *
		pipe_config->pipe_src_h;
	u32 scaled_height = pipe_config->pipe_src_w *
		adjusted_mode->vdisplay;

	/* 965+ is easy, it does everything in hw */
	if (scaled_width > scaled_height)
		*pfit_control |= PFIT_ENABLE |
			PFIT_SCALING_PILLAR;
	else if (scaled_width < scaled_height)
		*pfit_control |= PFIT_ENABLE |
			PFIT_SCALING_LETTER;
	else if (adjusted_mode->hdisplay != pipe_config->pipe_src_w)
		*pfit_control |= PFIT_ENABLE | PFIT_SCALING_AUTO;
}

static void i9xx_scale_aspect(struct intel_crtc_config *pipe_config,
			      u32 *pfit_control, u32 *pfit_pgm_ratios,
			      u32 *border)
{
	struct drm_display_mode *adjusted_mode = &pipe_config->adjusted_mode;
	u32 scaled_width = adjusted_mode->hdisplay *
		pipe_config->pipe_src_h;
	u32 scaled_height = pipe_config->pipe_src_w *
		adjusted_mode->vdisplay;
	u32 bits;

	/*
	 * For earlier chips we have to calculate the scaling
	 * ratio by hand and program it into the
	 * PFIT_PGM_RATIO register
	 */
	if (scaled_width > scaled_height) { /* pillar */
		centre_horizontally(adjusted_mode,
				    scaled_height /
				    pipe_config->pipe_src_h);

		*border = LVDS_BORDER_ENABLE;
		if (pipe_config->pipe_src_h != adjusted_mode->vdisplay) {
			bits = panel_fitter_scaling(pipe_config->pipe_src_h,
						    adjusted_mode->vdisplay);

			*pfit_pgm_ratios |= (bits << PFIT_HORIZ_SCALE_SHIFT |
					     bits << PFIT_VERT_SCALE_SHIFT);
			*pfit_control |= (PFIT_ENABLE |
					  VERT_INTERP_BILINEAR |
					  HORIZ_INTERP_BILINEAR);
		}
	} else if (scaled_width < scaled_height) { /* letter */
		centre_vertically(adjusted_mode,
				  scaled_width /
				  pipe_config->pipe_src_w);

		*border = LVDS_BORDER_ENABLE;
		if (pipe_config->pipe_src_w != adjusted_mode->hdisplay) {
			bits = panel_fitter_scaling(pipe_config->pipe_src_w,
						    adjusted_mode->hdisplay);

			*pfit_pgm_ratios |= (bits << PFIT_HORIZ_SCALE_SHIFT |
					     bits << PFIT_VERT_SCALE_SHIFT);
			*pfit_control |= (PFIT_ENABLE |
					  VERT_INTERP_BILINEAR |
					  HORIZ_INTERP_BILINEAR);
		}
	} else {
		/* Aspects match, Let hw scale both directions */
		*pfit_control |= (PFIT_ENABLE |
				  VERT_AUTO_SCALE | HORIZ_AUTO_SCALE |
				  VERT_INTERP_BILINEAR |
				  HORIZ_INTERP_BILINEAR);
	}
}

void intel_gmch_panel_fitting(struct intel_crtc *intel_crtc,
			      struct intel_crtc_config *pipe_config,
			      int fitting_mode)
{
	struct drm_device *dev = intel_crtc->base.dev;
	u32 pfit_control = 0, pfit_pgm_ratios = 0, border = 0;
	struct drm_display_mode *adjusted_mode;

	adjusted_mode = &pipe_config->adjusted_mode;

	/* Native modes don't need fitting */
	if (adjusted_mode->hdisplay == pipe_config->pipe_src_w &&
	    adjusted_mode->vdisplay == pipe_config->pipe_src_h)
		goto out;

	switch (fitting_mode) {
	case DRM_MODE_SCALE_CENTER:
		/*
		 * For centered modes, we have to calculate border widths &
		 * heights and modify the values programmed into the CRTC.
		 */
		centre_horizontally(adjusted_mode, pipe_config->pipe_src_w);
		centre_vertically(adjusted_mode, pipe_config->pipe_src_h);
		border = LVDS_BORDER_ENABLE;
		break;
	case DRM_MODE_SCALE_ASPECT:
		/* Scale but preserve the aspect ratio */
		if (INTEL_INFO(dev)->gen >= 4)
			i965_scale_aspect(pipe_config, &pfit_control);
		else
			i9xx_scale_aspect(pipe_config, &pfit_control,
					  &pfit_pgm_ratios, &border);
		break;
	case DRM_MODE_SCALE_FULLSCREEN:
		/*
		 * Full scaling, even if it changes the aspect ratio.
		 * Fortunately this is all done for us in hw.
		 */
		if (pipe_config->pipe_src_h != adjusted_mode->vdisplay ||
		    pipe_config->pipe_src_w != adjusted_mode->hdisplay) {
			pfit_control |= PFIT_ENABLE;
			if (INTEL_INFO(dev)->gen >= 4)
				pfit_control |= PFIT_SCALING_AUTO;
			else
				pfit_control |= (VERT_AUTO_SCALE |
						 VERT_INTERP_BILINEAR |
						 HORIZ_AUTO_SCALE |
						 HORIZ_INTERP_BILINEAR);
		}
		break;
	default:
		WARN(1, "bad panel fit mode: %d\n", fitting_mode);
		return;
	}

	/* 965+ wants fuzzy fitting */
	/* FIXME: handle multiple panels by failing gracefully */
	if (INTEL_INFO(dev)->gen >= 4)
		pfit_control |= ((intel_crtc->pipe << PFIT_PIPE_SHIFT) |
				 PFIT_FILTER_FUZZY);

out:
	if ((pfit_control & PFIT_ENABLE) == 0) {
		pfit_control = 0;
		pfit_pgm_ratios = 0;
	}

	/* Make sure pre-965 set dither correctly for 18bpp panels. */
	if (INTEL_INFO(dev)->gen < 4 && pipe_config->pipe_bpp == 18)
		pfit_control |= PANEL_8TO6_DITHER_ENABLE;

	pipe_config->gmch_pfit.control = pfit_control;
	pipe_config->gmch_pfit.pgm_ratios = pfit_pgm_ratios;
	pipe_config->gmch_pfit.lvds_border_bits = border;
}

static int is_backlight_combination_mode(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (IS_GEN4(dev))
		return I915_READ(BLC_PWM_CTL2) & BLM_COMBINATION_MODE;

	if (IS_GEN2(dev))
		return I915_READ(BLC_PWM_CTL) & BLM_LEGACY_MODE;

	return 0;
}

static u32 pch_get_max_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;

	val = I915_READ(BLC_PWM_PCH_CTL2);
	if (dev_priv->regfile.saveBLC_PWM_CTL2 == 0) {
		dev_priv->regfile.saveBLC_PWM_CTL2 = val;
	} else if (val == 0) {
		val = dev_priv->regfile.saveBLC_PWM_CTL2;
		I915_WRITE(BLC_PWM_PCH_CTL2, val);
	}

	val >>= 16;

	return val;
}

static u32 i9xx_get_max_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;

	val = I915_READ(BLC_PWM_CTL);
	if (dev_priv->regfile.saveBLC_PWM_CTL == 0) {
		dev_priv->regfile.saveBLC_PWM_CTL = val;
	} else if (val == 0) {
		val = dev_priv->regfile.saveBLC_PWM_CTL;
		I915_WRITE(BLC_PWM_CTL, val);
	}

	val >>= 17;

	if (is_backlight_combination_mode(dev))
		val *= 0xff;

	return val;
}

static u32 i965_get_max_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;

	val = I915_READ(BLC_PWM_CTL);
	if (dev_priv->regfile.saveBLC_PWM_CTL == 0) {
		dev_priv->regfile.saveBLC_PWM_CTL = val;
		dev_priv->regfile.saveBLC_PWM_CTL2 = I915_READ(BLC_PWM_CTL2);
	} else if (val == 0) {
		val = dev_priv->regfile.saveBLC_PWM_CTL;
		I915_WRITE(BLC_PWM_CTL, val);
		I915_WRITE(BLC_PWM_CTL2, dev_priv->regfile.saveBLC_PWM_CTL2);
	}

	val >>= 16;

	if (is_backlight_combination_mode(dev))
		val *= 0xff;

	return val;
}

static u32 _vlv_get_max_backlight(struct drm_device *dev, enum pipe pipe)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;

	val = I915_READ(VLV_BLC_PWM_CTL(pipe));
	if (dev_priv->regfile.saveBLC_PWM_CTL == 0) {
		dev_priv->regfile.saveBLC_PWM_CTL = val;
		dev_priv->regfile.saveBLC_PWM_CTL2 =
			I915_READ(VLV_BLC_PWM_CTL2(pipe));
	} else if (val == 0) {
		val = dev_priv->regfile.saveBLC_PWM_CTL;
		I915_WRITE(VLV_BLC_PWM_CTL(pipe), val);
		I915_WRITE(VLV_BLC_PWM_CTL2(pipe),
			   dev_priv->regfile.saveBLC_PWM_CTL2);
	}

	if (!val)
		val = 0x0f42ffff;

	val >>= 16;

	return val;
}

static u32 vlv_get_max_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	enum pipe pipe = intel_get_pipe_from_connector(connector);

	return _vlv_get_max_backlight(dev, pipe);
}

static u32 intel_panel_get_max_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 max;

	WARN_ON_SMP(!spin_is_locked(&dev_priv->backlight_lock));

	max = dev_priv->display.get_max_backlight(connector);

	DRM_DEBUG_DRIVER("max backlight PWM = %d\n", max);

	return max;
}

static int i915_panel_invert_brightness;
MODULE_PARM_DESC(invert_brightness, "Invert backlight brightness "
	"(-1 force normal, 0 machine defaults, 1 force inversion), please "
	"report PCI device ID, subsystem vendor and subsystem device ID "
	"to dri-devel@lists.freedesktop.org, if your machine needs it. "
	"It will then be included in an upcoming module version.");
module_param_named(invert_brightness, i915_panel_invert_brightness, int, 0600);
static u32 intel_panel_compute_brightness(struct intel_connector *connector,
					  u32 val)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;

	WARN_ON(panel->backlight.max == 0);

	if (i915_panel_invert_brightness < 0)
		return val;

	if (i915_panel_invert_brightness > 0 ||
	    dev_priv->quirks & QUIRK_INVERT_BRIGHTNESS) {
		return panel->backlight.max - val;
	}

	return val;
}

static u32 pch_get_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	return I915_READ(BLC_PWM_CPU_CTL) & BACKLIGHT_DUTY_CYCLE_MASK;
}

static u32 i9xx_get_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	u32 val;

	val = I915_READ(BLC_PWM_CTL) & BACKLIGHT_DUTY_CYCLE_MASK;
	if (INTEL_INFO(dev)->gen < 4)
		val >>= 1;

	if (panel->backlight.combination_mode) {
		u8 lbpc;

		pci_read_config_byte(dev->pdev, PCI_LBPC, &lbpc);
		val *= lbpc;
	}

	return val;
}

static u32 _vlv_get_backlight(struct drm_device *dev, enum pipe pipe)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	return I915_READ(VLV_BLC_PWM_CTL(pipe)) & BACKLIGHT_DUTY_CYCLE_MASK;
}

static u32 vlv_get_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	enum pipe pipe = intel_get_pipe_from_connector(connector);

	return _vlv_get_backlight(dev, pipe);
}

static u32 intel_panel_get_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&dev_priv->backlight_lock, flags);

	val = dev_priv->display.get_backlight(connector);
	val = intel_panel_compute_brightness(connector, val);

	spin_unlock_irqrestore(&dev_priv->backlight_lock, flags);

	DRM_DEBUG_DRIVER("get backlight PWM = %d\n", val);
	return val;
}

static void pch_set_backlight(struct intel_connector *connector, u32 level)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 tmp;

	tmp = I915_READ(BLC_PWM_CPU_CTL) & ~BACKLIGHT_DUTY_CYCLE_MASK;
	I915_WRITE(BLC_PWM_CPU_CTL, tmp | level);
}

static void i9xx_set_backlight(struct intel_connector *connector, u32 level)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	u32 tmp, mask;

	WARN_ON(panel->backlight.max == 0);

	if (panel->backlight.combination_mode) {
		u8 lbpc;

		lbpc = level * 0xfe / panel->backlight.max + 1;
		level /= lbpc;
		pci_write_config_byte(dev->pdev, PCI_LBPC, lbpc);
	}

	if (IS_GEN4(dev)) {
		mask = BACKLIGHT_DUTY_CYCLE_MASK;
	} else {
		level <<= 1;
		mask = BACKLIGHT_DUTY_CYCLE_MASK_PNV;
	}

	tmp = I915_READ(BLC_PWM_CTL) & ~mask;
	I915_WRITE(BLC_PWM_CTL, tmp | level);
}

static void vlv_set_backlight(struct intel_connector *connector, u32 level)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	enum pipe pipe = intel_get_pipe_from_connector(connector);
	u32 tmp;

	tmp = I915_READ(VLV_BLC_PWM_CTL(pipe)) & ~BACKLIGHT_DUTY_CYCLE_MASK;
	I915_WRITE(VLV_BLC_PWM_CTL(pipe), tmp | level);
}

static void
intel_panel_actually_set_backlight(struct intel_connector *connector, u32 level)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	DRM_DEBUG_DRIVER("set backlight PWM = %d\n", level);

	level = intel_panel_compute_brightness(connector, level);
	dev_priv->display.set_backlight(connector, level);
}

/* set backlight brightness to level in range [0..max] */
void intel_panel_set_backlight(struct intel_connector *connector, u32 level,
			       u32 max)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	enum pipe pipe = intel_get_pipe_from_connector(connector);
	u32 freq;
	unsigned long flags;

	if (pipe == INVALID_PIPE)
		return;

	spin_lock_irqsave(&dev_priv->backlight_lock, flags);

	WARN_ON(panel->backlight.max == 0);

	/* scale to hardware max, but be careful to not overflow */
	freq = panel->backlight.max;
	if (freq < max)
		level = level * freq / max;
	else
		level = freq / max * level;

	panel->backlight.level = level;
	if (panel->backlight.device)
		panel->backlight.device->props.brightness = level;

	if (panel->backlight.enabled)
		intel_panel_actually_set_backlight(connector, level);

	spin_unlock_irqrestore(&dev_priv->backlight_lock, flags);
}

static void pch_disable_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 tmp;

	intel_panel_actually_set_backlight(connector, 0);

	tmp = I915_READ(BLC_PWM_CPU_CTL2);
	I915_WRITE(BLC_PWM_CPU_CTL2, tmp & ~BLM_PWM_ENABLE);

	tmp = I915_READ(BLC_PWM_PCH_CTL1);
	I915_WRITE(BLC_PWM_PCH_CTL1, tmp & ~BLM_PCH_PWM_ENABLE);
}

static void i9xx_disable_backlight(struct intel_connector *connector)
{
	intel_panel_actually_set_backlight(connector, 0);
}

static void i965_disable_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 tmp;

	intel_panel_actually_set_backlight(connector, 0);

	tmp = I915_READ(BLC_PWM_CTL2);
	I915_WRITE(BLC_PWM_CTL2, tmp & ~BLM_PWM_ENABLE);
}

static void vlv_disable_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	enum pipe pipe = intel_get_pipe_from_connector(connector);
	u32 tmp;

	intel_panel_actually_set_backlight(connector, 0);

	tmp = I915_READ(VLV_BLC_PWM_CTL2(pipe));
	I915_WRITE(VLV_BLC_PWM_CTL2(pipe), tmp & ~BLM_PWM_ENABLE);
}

void intel_panel_disable_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	enum pipe pipe = intel_get_pipe_from_connector(connector);
	unsigned long flags;

	if (pipe == INVALID_PIPE)
		return;

	/*
	 * Do not disable backlight on the vgaswitcheroo path. When switching
	 * away from i915, the other client may depend on i915 to handle the
	 * backlight. This will leave the backlight on unnecessarily when
	 * another client is not activated.
	 */
	if (dev->switch_power_state == DRM_SWITCH_POWER_CHANGING) {
		DRM_DEBUG_DRIVER("Skipping backlight disable on vga switch\n");
		return;
	}

	spin_lock_irqsave(&dev_priv->backlight_lock, flags);

	panel->backlight.enabled = false;
	dev_priv->display.disable_backlight(connector);

	spin_unlock_irqrestore(&dev_priv->backlight_lock, flags);
}

static void pch_enable_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	enum pipe pipe = intel_get_pipe_from_connector(connector);
	enum transcoder cpu_transcoder =
		intel_pipe_to_cpu_transcoder(dev_priv, pipe);
	u32 tmp;

	tmp = I915_READ(BLC_PWM_CPU_CTL2);

	/* Note that this can also get called through dpms changes. And
	 * we don't track the backlight dpms state, hence check whether
	 * we have to do anything first. */
	if (tmp & BLM_PWM_ENABLE)
		return;

	if (INTEL_INFO(dev)->num_pipes == 3)
		tmp &= ~BLM_PIPE_SELECT_IVB;
	else
		tmp &= ~BLM_PIPE_SELECT;

	if (cpu_transcoder == TRANSCODER_EDP)
		tmp |= BLM_TRANSCODER_EDP;
	else
		tmp |= BLM_PIPE(cpu_transcoder);
	tmp &= ~BLM_PWM_ENABLE;

	I915_WRITE(BLC_PWM_CPU_CTL2, tmp);
	POSTING_READ(BLC_PWM_CPU_CTL2);
	I915_WRITE(BLC_PWM_CPU_CTL2, tmp | BLM_PWM_ENABLE);

	if (!(dev_priv->quirks & QUIRK_NO_PCH_PWM_ENABLE)) {
		tmp = I915_READ(BLC_PWM_PCH_CTL1);
		tmp |= BLM_PCH_PWM_ENABLE;
		tmp &= ~BLM_PCH_OVERRIDE_ENABLE;
		I915_WRITE(BLC_PWM_PCH_CTL1, tmp);
	}

	/*
	 * Call below after setting BLC_PWM_CPU_CTL2 and BLC_PWM_PCH_CTL1.
	 * BLC_PWM_CPU_CTL may be cleared to zero automatically when these
	 * registers are set.
	 */
	intel_panel_actually_set_backlight(connector, panel->backlight.level);
}

static void i9xx_enable_backlight(struct intel_connector *connector)
{
	struct intel_panel *panel = &connector->panel;

	intel_panel_actually_set_backlight(connector, panel->backlight.level);
}

static void i965_enable_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	enum pipe pipe = intel_get_pipe_from_connector(connector);
	u32 tmp;

	tmp = I915_READ(BLC_PWM_CTL2);

	/* Note that this can also get called through dpms changes. And
	 * we don't track the backlight dpms state, hence check whether
	 * we have to do anything first. */
	if (tmp & BLM_PWM_ENABLE)
		return;

	tmp &= ~BLM_PIPE_SELECT;
	tmp |= BLM_PIPE(pipe);
	tmp &= ~BLM_PWM_ENABLE;

	I915_WRITE(BLC_PWM_CTL2, tmp);
	POSTING_READ(BLC_PWM_CTL2);
	I915_WRITE(BLC_PWM_CTL2, tmp | BLM_PWM_ENABLE);

	intel_panel_actually_set_backlight(connector, panel->backlight.level);
}

static void vlv_enable_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	enum pipe pipe = intel_get_pipe_from_connector(connector);
	u32 tmp;

	tmp = I915_READ(VLV_BLC_PWM_CTL2(pipe));

	/* Note that this can also get called through dpms changes. And
	 * we don't track the backlight dpms state, hence check whether
	 * we have to do anything first. */
	if (tmp & BLM_PWM_ENABLE)
		return;

	tmp &= ~BLM_PWM_ENABLE;

	I915_WRITE(VLV_BLC_PWM_CTL2(pipe), tmp);
	POSTING_READ(VLV_BLC_PWM_CTL2(pipe));
	I915_WRITE(VLV_BLC_PWM_CTL2(pipe), tmp | BLM_PWM_ENABLE);

	intel_panel_actually_set_backlight(connector, panel->backlight.level);
}

void intel_panel_enable_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	enum pipe pipe = intel_get_pipe_from_connector(connector);
	unsigned long flags;

	if (pipe == INVALID_PIPE)
		return;

	DRM_DEBUG_KMS("pipe %c\n", pipe_name(pipe));

	spin_lock_irqsave(&dev_priv->backlight_lock, flags);

	/* XXX: transitional, call to make sure freq is set */
	intel_panel_get_max_backlight(connector);

	WARN_ON(panel->backlight.max == 0);

	if (panel->backlight.level == 0) {
		panel->backlight.level = panel->backlight.max;
		if (panel->backlight.device)
			panel->backlight.device->props.brightness =
				panel->backlight.level;
	}

	dev_priv->display.enable_backlight(connector);
	panel->backlight.enabled = true;

	spin_unlock_irqrestore(&dev_priv->backlight_lock, flags);
}

enum drm_connector_status
intel_panel_detect(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* Assume that the BIOS does not lie through the OpRegion... */
	if (!i915_panel_ignore_lid && dev_priv->opregion.lid_state) {
		return ioread32(dev_priv->opregion.lid_state) & 0x1 ?
			connector_status_connected :
			connector_status_disconnected;
	}

	switch (i915_panel_ignore_lid) {
	case -2:
		return connector_status_connected;
	case -1:
		return connector_status_disconnected;
	default:
		return connector_status_unknown;
	}
}

#if IS_ENABLED(CONFIG_BACKLIGHT_CLASS_DEVICE)
static int intel_backlight_device_update_status(struct backlight_device *bd)
{
	struct intel_connector *connector = bl_get_data(bd);
	struct drm_device *dev = connector->base.dev;

	mutex_lock(&dev->mode_config.mutex);
	DRM_DEBUG_KMS("updating intel_backlight, brightness=%d/%d\n",
		      bd->props.brightness, bd->props.max_brightness);
	intel_panel_set_backlight(connector, bd->props.brightness,
				  bd->props.max_brightness);
	mutex_unlock(&dev->mode_config.mutex);
	return 0;
}

static int intel_backlight_device_get_brightness(struct backlight_device *bd)
{
	struct intel_connector *connector = bl_get_data(bd);
	struct drm_device *dev = connector->base.dev;
	int ret;

	mutex_lock(&dev->mode_config.mutex);
	ret = intel_panel_get_backlight(connector);
	mutex_unlock(&dev->mode_config.mutex);

	return ret;
}

static const struct backlight_ops intel_backlight_device_ops = {
	.update_status = intel_backlight_device_update_status,
	.get_brightness = intel_backlight_device_get_brightness,
};

static int intel_backlight_device_register(struct intel_connector *connector)
{
	struct intel_panel *panel = &connector->panel;
	struct backlight_properties props;

	if (WARN_ON(panel->backlight.device))
		return -ENODEV;

	BUG_ON(panel->backlight.max == 0);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.brightness = panel->backlight.level;
	props.max_brightness = panel->backlight.max;

	/*
	 * Note: using the same name independent of the connector prevents
	 * registration of multiple backlight devices in the driver.
	 */
	panel->backlight.device =
		backlight_device_register("intel_backlight",
					  connector->base.kdev,
					  connector,
					  &intel_backlight_device_ops, &props);

	if (IS_ERR(panel->backlight.device)) {
		DRM_ERROR("Failed to register backlight: %ld\n",
			  PTR_ERR(panel->backlight.device));
		panel->backlight.device = NULL;
		return -ENODEV;
	}
	return 0;
}

static void intel_backlight_device_unregister(struct intel_connector *connector)
{
	struct intel_panel *panel = &connector->panel;

	if (panel->backlight.device) {
		backlight_device_unregister(panel->backlight.device);
		panel->backlight.device = NULL;
	}
}
#else /* CONFIG_BACKLIGHT_CLASS_DEVICE */
static int intel_backlight_device_register(struct intel_connector *connector)
{
	return 0;
}
static void intel_backlight_device_unregister(struct intel_connector *connector)
{
}
#endif /* CONFIG_BACKLIGHT_CLASS_DEVICE */

/*
 * Note: The setup hooks can't assume pipe is set!
 *
 * XXX: Query mode clock or hardware clock and program PWM modulation frequency
 * appropriately when it's 0. Use VBT and/or sane defaults.
 */
static int pch_setup_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	u32 cpu_ctl2, pch_ctl1, pch_ctl2, val;

	pch_ctl1 = I915_READ(BLC_PWM_PCH_CTL1);
	panel->backlight.active_low_pwm = pch_ctl1 & BLM_PCH_POLARITY;

	pch_ctl2 = I915_READ(BLC_PWM_PCH_CTL2);
	panel->backlight.max = pch_ctl2 >> 16;
	if (!panel->backlight.max)
		return -ENODEV;

	val = pch_get_backlight(connector);
	panel->backlight.level = intel_panel_compute_brightness(connector, val);

	cpu_ctl2 = I915_READ(BLC_PWM_CPU_CTL2);
	panel->backlight.enabled = (cpu_ctl2 & BLM_PWM_ENABLE) &&
		(pch_ctl1 & BLM_PCH_PWM_ENABLE) && panel->backlight.level != 0;

	return 0;
}

static int i9xx_setup_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	u32 ctl, val;

	ctl = I915_READ(BLC_PWM_CTL);

	if (IS_GEN2(dev))
		panel->backlight.combination_mode = ctl & BLM_LEGACY_MODE;

	if (IS_PINEVIEW(dev))
		panel->backlight.active_low_pwm = ctl & BLM_POLARITY_PNV;

	panel->backlight.max = ctl >> 17;
	if (panel->backlight.combination_mode)
		panel->backlight.max *= 0xff;

	if (!panel->backlight.max)
		return -ENODEV;

	val = i9xx_get_backlight(connector);
	panel->backlight.level = intel_panel_compute_brightness(connector, val);

	panel->backlight.enabled = panel->backlight.level != 0;

	return 0;
}

static int i965_setup_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	u32 ctl, ctl2, val;

	ctl2 = I915_READ(BLC_PWM_CTL2);
	panel->backlight.combination_mode = ctl2 & BLM_COMBINATION_MODE;
	panel->backlight.active_low_pwm = ctl2 & BLM_POLARITY_I965;

	ctl = I915_READ(BLC_PWM_CTL);
	panel->backlight.max = ctl >> 16;
	if (panel->backlight.combination_mode)
		panel->backlight.max *= 0xff;

	if (!panel->backlight.max)
		return -ENODEV;

	val = i9xx_get_backlight(connector);
	panel->backlight.level = intel_panel_compute_brightness(connector, val);

	panel->backlight.enabled = (ctl2 & BLM_PWM_ENABLE) &&
		panel->backlight.level != 0;

	return 0;
}

static int vlv_setup_backlight(struct intel_connector *connector)
{
	struct drm_device *dev = connector->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_panel *panel = &connector->panel;
	enum pipe pipe;
	u32 ctl, ctl2, val;

	for_each_pipe(pipe) {
		u32 cur_val = I915_READ(VLV_BLC_PWM_CTL(pipe));

		/* Skip if the modulation freq is already set */
		if (cur_val & ~BACKLIGHT_DUTY_CYCLE_MASK)
			continue;

		cur_val &= BACKLIGHT_DUTY_CYCLE_MASK;
		I915_WRITE(VLV_BLC_PWM_CTL(pipe), (0xf42 << 16) |
			   cur_val);
	}

	ctl2 = I915_READ(VLV_BLC_PWM_CTL2(PIPE_A));
	panel->backlight.active_low_pwm = ctl2 & BLM_POLARITY_I965;

	ctl = I915_READ(VLV_BLC_PWM_CTL(PIPE_A));
	panel->backlight.max = ctl >> 16;
	if (!panel->backlight.max)
		return -ENODEV;

	val = _vlv_get_backlight(dev, PIPE_A);
	panel->backlight.level = intel_panel_compute_brightness(connector, val);

	panel->backlight.enabled = (ctl2 & BLM_PWM_ENABLE) &&
		panel->backlight.level != 0;

	return 0;
}

int intel_panel_setup_backlight(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_connector *intel_connector = to_intel_connector(connector);
	struct intel_panel *panel = &intel_connector->panel;
	unsigned long flags;
	int ret;

	/* set level and max in panel struct */
	spin_lock_irqsave(&dev_priv->backlight_lock, flags);
	ret = dev_priv->display.setup_backlight(intel_connector);
	spin_unlock_irqrestore(&dev_priv->backlight_lock, flags);

	if (ret) {
		DRM_DEBUG_KMS("failed to setup backlight for connector %s\n",
			      drm_get_connector_name(connector));
		return ret;
	}

	intel_backlight_device_register(intel_connector);

	panel->backlight.present = true;

	DRM_DEBUG_KMS("backlight initialized, %s, brightness %u/%u, "
		      "sysfs interface %sregistered\n",
		      panel->backlight.enabled ? "enabled" : "disabled",
		      panel->backlight.level, panel->backlight.max,
		      panel->backlight.device ? "" : "not ");

	return 0;
}

void intel_panel_destroy_backlight(struct drm_connector *connector)
{
	struct intel_connector *intel_connector = to_intel_connector(connector);
	struct intel_panel *panel = &intel_connector->panel;

	panel->backlight.present = false;
	intel_backlight_device_unregister(intel_connector);
}

/* Set up chip specific backlight functions */
void intel_panel_init_backlight_funcs(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (HAS_PCH_SPLIT(dev)) {
		dev_priv->display.setup_backlight = pch_setup_backlight;
		dev_priv->display.enable_backlight = pch_enable_backlight;
		dev_priv->display.disable_backlight = pch_disable_backlight;
		dev_priv->display.set_backlight = pch_set_backlight;
		dev_priv->display.get_backlight = pch_get_backlight;
		dev_priv->display.get_max_backlight = pch_get_max_backlight;
	} else if (IS_VALLEYVIEW(dev)) {
		dev_priv->display.setup_backlight = vlv_setup_backlight;
		dev_priv->display.enable_backlight = vlv_enable_backlight;
		dev_priv->display.disable_backlight = vlv_disable_backlight;
		dev_priv->display.set_backlight = vlv_set_backlight;
		dev_priv->display.get_backlight = vlv_get_backlight;
		dev_priv->display.get_max_backlight = vlv_get_max_backlight;
	} else if (IS_GEN4(dev)) {
		dev_priv->display.setup_backlight = i965_setup_backlight;
		dev_priv->display.enable_backlight = i965_enable_backlight;
		dev_priv->display.disable_backlight = i965_disable_backlight;
		dev_priv->display.set_backlight = i9xx_set_backlight;
		dev_priv->display.get_backlight = i9xx_get_backlight;
		dev_priv->display.get_max_backlight = i965_get_max_backlight;
	} else {
		dev_priv->display.setup_backlight = i9xx_setup_backlight;
		dev_priv->display.enable_backlight = i9xx_enable_backlight;
		dev_priv->display.disable_backlight = i9xx_disable_backlight;
		dev_priv->display.set_backlight = i9xx_set_backlight;
		dev_priv->display.get_backlight = i9xx_get_backlight;
		dev_priv->display.get_max_backlight = i9xx_get_max_backlight;
	}
}

int intel_panel_init(struct intel_panel *panel,
		     struct drm_display_mode *fixed_mode)
{
	panel->fixed_mode = fixed_mode;

	return 0;
}

void intel_panel_fini(struct intel_panel *panel)
{
	struct intel_connector *intel_connector =
		container_of(panel, struct intel_connector, panel);

	if (panel->fixed_mode)
		drm_mode_destroy(intel_connector->base.dev, panel->fixed_mode);
}

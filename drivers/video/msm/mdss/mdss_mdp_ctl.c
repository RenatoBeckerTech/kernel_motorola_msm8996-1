/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"

static DEFINE_MUTEX(mdss_mdp_ctl_lock);
static struct mdss_mdp_ctl mdss_mdp_ctl_list[MDSS_MDP_MAX_CTL];
static struct mdss_mdp_mixer mdss_mdp_mixer_list[MDSS_MDP_MAX_LAYERMIXER];

static struct mdss_mdp_ctl *mdss_mdp_ctl_alloc(void)
{
	struct mdss_mdp_ctl *ctl = NULL;
	int cnum;

	mutex_lock(&mdss_mdp_ctl_lock);
	for (cnum = 0; cnum < MDSS_MDP_MAX_CTL; cnum++) {
		if (mdss_mdp_ctl_list[cnum].ref_cnt == 0) {
			ctl = &mdss_mdp_ctl_list[cnum];
			ctl->num = cnum;
			ctl->ref_cnt++;
			mutex_init(&ctl->lock);

			pr_debug("alloc ctl_num=%d\n", ctl->num);
			break;
		}
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return ctl;
}

static int mdss_mdp_ctl_free(struct mdss_mdp_ctl *ctl)
{
	if (!ctl)
		return -ENODEV;

	pr_debug("free ctl_num=%d ref_cnt=%d\n", ctl->num, ctl->ref_cnt);

	if (!ctl->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	mutex_lock(&mdss_mdp_ctl_lock);
	if (--ctl->ref_cnt == 0)
		memset(ctl, 0, sizeof(*ctl));
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

static struct mdss_mdp_mixer *mdss_mdp_mixer_alloc(u32 type)
{
	struct mdss_mdp_mixer *mixer = NULL;
	int mnum;

	mutex_lock(&mdss_mdp_ctl_lock);
	for (mnum = 0; mnum < MDSS_MDP_MAX_LAYERMIXER; mnum++) {
		if (type == mdss_res->mixer_type_map[mnum] &&
		    mdss_mdp_mixer_list[mnum].ref_cnt == 0) {
			mixer = &mdss_mdp_mixer_list[mnum];
			mixer->num = mnum;
			mixer->ref_cnt++;
			mixer->params_changed++;
			mixer->type = type;

			pr_debug("mixer_num=%d\n", mixer->num);
			break;
		}
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return mixer;
}

static int mdss_mdp_mixer_free(struct mdss_mdp_mixer *mixer)
{
	if (!mixer)
		return -ENODEV;

	pr_debug("free mixer_num=%d ref_cnt=%d\n", mixer->num, mixer->ref_cnt);

	if (!mixer->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	mutex_lock(&mdss_mdp_ctl_lock);
	if (--mixer->ref_cnt == 0)
		memset(mixer, 0, sizeof(*mixer));
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

static int mdss_mdp_ctl_init(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl;
	u32 width, height;

	if (!mfd)
		return -ENODEV;

	width = mfd->fbi->var.xres;
	height = mfd->fbi->var.yres;

	if (width > (2 * MAX_MIXER_WIDTH)) {
		pr_err("unsupported resolution\n");
		return -EINVAL;
	}

	ctl = mdss_mdp_ctl_alloc();

	if (!ctl) {
		pr_err("unable to allocate ctl\n");
		return -ENOMEM;
	}

	ctl->mfd = mfd;
	ctl->width = width;
	ctl->height = height;
	ctl->dst_format = mfd->panel_info.out_format;

	ctl->mixer_left = mdss_mdp_mixer_alloc(MDSS_MDP_MIXER_TYPE_INTF);
	if (!ctl->mixer_left) {
		pr_err("unable to allocate layer mixer\n");
		mdss_mdp_ctl_free(ctl);
		return -ENOMEM;
	}

	ctl->mixer_left->width = MIN(width, MAX_MIXER_WIDTH);
	ctl->mixer_left->height = height;
	ctl->mixer_left->ctl = ctl;

	width -= ctl->mixer_left->width;

	if (width) {
		ctl->mixer_right =
		mdss_mdp_mixer_alloc(MDSS_MDP_MIXER_TYPE_INTF);
		if (!ctl->mixer_right) {
			pr_err("unable to allocate layer mixer\n");
			mdss_mdp_mixer_free(ctl->mixer_left);
			mdss_mdp_ctl_free(ctl);
			return -ENOMEM;
		}
		ctl->mixer_right->width = width;
		ctl->mixer_right->height = height;
		ctl->mixer_right->ctl = ctl;
	}

	switch (mfd->panel_info.type) {
	case WRITEBACK_PANEL:
		ctl->intf_num = MDSS_MDP_NO_INTF;
		ctl->opmode = MDSS_MDP_CTL_OP_WFD_MODE;
		ctl->start_fnc = mdss_mdp_writeback_start;
		break;
	default:
		pr_err("unsupported panel type (%d)\n", mfd->panel_info.type);
		mdss_mdp_ctl_free(ctl);
		return -EINVAL;

	}

	ctl->opmode |= (ctl->intf_num << 4);

	if (ctl->mixer_right) {
		ctl->opmode |= MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
			       MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT;
	}

	mfd->ctl = ctl;

	return 0;
}

static int mdss_mdp_ctl_destroy(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl;
	if (!mfd || !mfd->ctl)
		return -ENODEV;

	ctl = mfd->ctl;
	mfd->ctl = NULL;

	if (ctl->mixer_left)
		mdss_mdp_mixer_free(ctl->mixer_left);
	if (ctl->mixer_right)
		mdss_mdp_mixer_free(ctl->mixer_right);
	mdss_mdp_ctl_free(ctl);

	return 0;
}

int mdss_mdp_ctl_on(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;
	u32 outsize, temp, off;
	int ret = 0;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected\n");
		return -ENODEV;
	}

	if (!mfd->ctl) {
		if (mdss_mdp_ctl_init(mfd)) {
			pr_err("unable to initialize ctl\n");
			return -ENODEV;
		}
	}
	ctl = mfd->ctl;

	mutex_lock(&ctl->lock);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	if (ctl->start_fnc)
		ret = ctl->start_fnc(ctl);
	else
		pr_warn("no start function for ctl=%d type=%d\n", ctl->num,
				mfd->panel_info.type);

	if (ret) {
		pr_err("unable to start intf\n");
		goto start_fail;
	}

	pr_debug("ctl_num=%d\n", ctl->num);

	mixer = ctl->mixer_left;
	mixer->params_changed++;

	temp = MDSS_MDP_REG_READ(MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << (ctl->intf_num * 8));
	MDSS_MDP_REG_WRITE(MDSS_MDP_REG_DISP_INTF_SEL, temp);

	outsize = (mixer->height << 16) | mixer->width;
	off = MDSS_MDP_REG_LM_OFFSET(mixer->num);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OUT_SIZE, outsize);

	if (ctl->mixer_right) {
		mixer = ctl->mixer_right;
		mixer->params_changed++;
		outsize = (mixer->height << 16) | mixer->width;
		off = MDSS_MDP_REG_LM_OFFSET(mixer->num);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OUT_SIZE, outsize);
		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_PACK_3D, 0);
	}

	ret = pdata->on(pdata);

start_fail:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_ctl_off(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_mdp_ctl *ctl;
	int ret = 0;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (!mfd->ctl) {
		pr_err("ctl not initialized\n");
		return -ENODEV;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected\n");
		return -ENODEV;
	}

	ctl = mfd->ctl;

	pr_debug("ctl_num=%d\n", mfd->ctl->num);

	mutex_lock(&ctl->lock);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	if (ctl->stop_fnc)
		ret = ctl->stop_fnc(ctl);
	else
		pr_warn("no stop func for ctl=%d\n", ctl->num);

	if (ret)
		pr_warn("error powering off intf ctl=%d\n", ctl->num);

	ret = pdata->off(pdata);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	ctl->play_cnt = 0;
	mutex_unlock(&ctl->lock);

	mdss_mdp_pipe_release_all(mfd);

	if (!mfd->ref_cnt)
		mdss_mdp_ctl_destroy(mfd);


	return ret;
}

static int mdss_mdp_mixer_setup(struct mdss_mdp_ctl *ctl,
				struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *pipe, *bgpipe = NULL;
	u32 off, blend_op, blend_stage;
	u32 mixercfg = 0, blend_color_out = 0, bgalpha = 0;
	int stage;

	if (!mixer)
		return -ENODEV;

	pr_debug("setup mixer=%d\n", mixer->num);

	for (stage = MDSS_MDP_STAGE_BASE; stage < MDSS_MDP_MAX_STAGE; stage++) {
		pipe = mixer->stage_pipe[stage];
		if (pipe == NULL) {
			if (stage == MDSS_MDP_STAGE_BASE)
				mixercfg |= MDSS_MDP_LM_BORDER_COLOR;
			continue;
		}

		if (stage != pipe->mixer_stage) {
			mixer->stage_pipe[stage] = NULL;
			continue;
		}
		mixercfg |= stage << (3 * pipe->num);

		if (stage == MDSS_MDP_STAGE_BASE) {
			bgpipe = pipe;
			if (pipe->src_fmt->alpha_enable)
				bgalpha = 1;
			continue;
		}

		blend_stage = stage - MDSS_MDP_STAGE_0;
		off = MDSS_MDP_REG_LM_OFFSET(mixer->num) +
		      MDSS_MDP_REG_LM_BLEND_OFFSET(blend_stage);

		if (pipe->is_fg) {
			bgalpha = 0;
			if (bgpipe) {
				mixercfg &= ~(0x7 << (3 * bgpipe->num));
				mixercfg |= MDSS_MDP_LM_BORDER_COLOR;
			}
			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
			/* keep fg alpha */
			blend_color_out |= 1 << (blend_stage + 1);

			pr_debug("pnum=%d stg=%d alpha=IS_FG\n", pipe->num,
					stage);
		} else if (pipe->src_fmt->alpha_enable) {
			bgalpha = 0;
			blend_op = (MDSS_MDP_BLEND_BG_ALPHA_FG_PIXEL |
				    MDSS_MDP_BLEND_BG_INV_ALPHA);
			/* keep fg alpha */
			blend_color_out |= 1 << (blend_stage + 1);

			pr_debug("pnum=%d stg=%d alpha=FG PIXEL\n", pipe->num,
					stage);
		} else if (bgalpha) {
			blend_op = (MDSS_MDP_BLEND_BG_ALPHA_BG_PIXEL |
				    MDSS_MDP_BLEND_FG_ALPHA_BG_PIXEL |
				    MDSS_MDP_BLEND_FG_INV_ALPHA);
			/* keep bg alpha */
			pr_debug("pnum=%d stg=%d alpha=BG_PIXEL\n", pipe->num,
					stage);
		} else {
			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
			pr_debug("pnum=%d stg=%d alpha=CONST\n", pipe->num,
					stage);
		}

		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OP_MODE, blend_op);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_BLEND_FG_ALPHA,
				   pipe->alpha);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_BLEND_BG_ALPHA,
				   0xFF - pipe->alpha);
	}

	if (mixer->cursor_enabled)
		mixercfg |= MDSS_MDP_LM_CURSOR_OUT;

	pr_debug("mixer=%d mixer_cfg=%x\n", mixer->num, mixercfg);

	ctl->flush_bits |= BIT(6) << mixer->num;	/* LAYER_MIXER */

	off = MDSS_MDP_REG_LM_OFFSET(mixer->num);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OP_MODE, blend_color_out);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(mixer->num), mixercfg);

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux)
{
	struct mdss_mdp_mixer *mixer = NULL;
	if (!ctl)
		return NULL;

	switch (mux) {
	case MDSS_MDP_MIXER_MUX_DEFAULT:
	case MDSS_MDP_MIXER_MUX_LEFT:
		mixer = ctl->mixer_left;
		break;
	case MDSS_MDP_MIXER_MUX_RIGHT:
		mixer = ctl->mixer_right;
		break;
	}

	return mixer;
}

struct mdss_mdp_pipe *mdss_mdp_mixer_stage_pipe(struct mdss_mdp_ctl *ctl,
						int mux, int stage)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_mdp_mixer *mixer;
	if (!ctl)
		return NULL;

	if (mutex_lock_interruptible(&ctl->lock))
		return NULL;

	mixer = mdss_mdp_mixer_get(ctl, mux);
	if (mixer)
		pipe = mixer->stage_pipe[stage];
	mutex_unlock(&ctl->lock);

	return pipe;
}

int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe, int params_changed)
{
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;

	if (!pipe)
		return -EINVAL;
	mixer = pipe->mixer;
	if (!mixer)
		return -EINVAL;
	ctl = mixer->ctl;
	if (!ctl)
		return -EINVAL;

	if (pipe->mixer_stage >= MDSS_MDP_MAX_STAGE) {
		pr_err("invalid mixer stage\n");
		return -EINVAL;
	}

	pr_debug("pnum=%x mixer=%d stage=%d\n", pipe->num, mixer->num,
			pipe->mixer_stage);

	if (mutex_lock_interruptible(&ctl->lock))
		return -EINTR;

	if (params_changed) {
		mixer->params_changed++;
		mixer->stage_pipe[pipe->mixer_stage] = pipe;
	}

	if (pipe->type == MDSS_MDP_PIPE_TYPE_DMA)
		ctl->flush_bits |= BIT(pipe->num) << 5;
	else /* RGB/VIG pipe */
		ctl->flush_bits |= BIT(pipe->num);

	mutex_unlock(&ctl->lock);

	return 0;
}

int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;

	if (!pipe)
		return -EINVAL;
	mixer = pipe->mixer;
	if (!mixer)
		return -EINVAL;
	ctl = mixer->ctl;
	if (!ctl)
		return -EINVAL;

	pr_debug("unstage pnum=%d stage=%d mixer=%d\n", pipe->num,
			pipe->mixer_stage, mixer->num);

	if (mutex_lock_interruptible(&ctl->lock))
		return -EINTR;

	mixer->params_changed++;
	mixer->stage_pipe[pipe->mixer_stage] = NULL;

	mutex_unlock(&ctl->lock);

	return 0;
}

static int mdss_mdp_mixer_update(struct mdss_mdp_mixer *mixer)
{
	mixer->params_changed = 0;

	/* skip mixer setup for rotator */
	if (!mixer->rotator_mode)
		mdss_mdp_mixer_setup(mixer->ctl, mixer);

	return 0;
}

int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg)
{
	int mixer1_changed, mixer2_changed;
	int ret = 0;

	if (!ctl) {
		pr_err("display function not set\n");
		return -ENODEV;
	}

	pr_debug("commit ctl=%d\n", ctl->num);

	if (mutex_lock_interruptible(&ctl->lock))
		return -EINTR;

	mixer1_changed = (ctl->mixer_left && ctl->mixer_left->params_changed);
	mixer2_changed = (ctl->mixer_right && ctl->mixer_right->params_changed);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	if (mixer1_changed || mixer2_changed) {
		if (ctl->prepare_fnc)
			ret = ctl->prepare_fnc(ctl, arg);
		if (ret) {
			pr_err("error preparing display\n");
			goto done;
		}

		if (mixer1_changed)
			mdss_mdp_mixer_update(ctl->mixer_left);
		if (mixer2_changed)
			mdss_mdp_mixer_update(ctl->mixer_right);

		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, ctl->opmode);
		ctl->flush_bits |= BIT(17);	/* CTL */
	}

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, ctl->flush_bits);
	wmb();
	ctl->flush_bits = 0;

	if (ctl->display_fnc)
		ret = ctl->display_fnc(ctl, arg); /* kickoff */
	if (ret)
		pr_warn("error displaying frame\n");

	ctl->play_cnt++;

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_unlock(&ctl->lock);

	return ret;
}

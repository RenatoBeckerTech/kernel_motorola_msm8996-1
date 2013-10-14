/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>

#include "drm.h"
#include "gem.h"

#define GR2D_NUM_REGS 0x4d

struct gr2d {
	struct tegra_drm_client client;
	struct host1x_channel *channel;
	struct clk *clk;

	DECLARE_BITMAP(addr_regs, GR2D_NUM_REGS);
};

static inline struct gr2d *to_gr2d(struct tegra_drm_client *client)
{
	return container_of(client, struct gr2d, client);
}

static int gr2d_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct tegra_drm *tegra = dev_get_drvdata(client->parent);
	struct gr2d *gr2d = to_gr2d(drm);

	gr2d->channel = host1x_channel_request(client->dev);
	if (!gr2d->channel)
		return -ENOMEM;

	client->syncpts[0] = host1x_syncpt_request(client->dev, false);
	if (!client->syncpts[0]) {
		host1x_channel_free(gr2d->channel);
		return -ENOMEM;
	}

	return tegra_drm_register_client(tegra, drm);
}

static int gr2d_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct tegra_drm *tegra = dev_get_drvdata(client->parent);
	struct gr2d *gr2d = to_gr2d(drm);
	int err;

	err = tegra_drm_unregister_client(tegra, drm);
	if (err < 0)
		return err;

	host1x_syncpt_free(client->syncpts[0]);
	host1x_channel_free(gr2d->channel);

	return 0;
}

static const struct host1x_client_ops gr2d_client_ops = {
	.init = gr2d_init,
	.exit = gr2d_exit,
};

static int gr2d_open_channel(struct tegra_drm_client *client,
			     struct tegra_drm_context *context)
{
	struct gr2d *gr2d = to_gr2d(client);

	context->channel = host1x_channel_get(gr2d->channel);
	if (!context->channel)
		return -ENOMEM;

	return 0;
}

static void gr2d_close_channel(struct tegra_drm_context *context)
{
	host1x_channel_put(context->channel);
}

static struct host1x_bo *host1x_bo_lookup(struct drm_device *drm,
					  struct drm_file *file,
					  u32 handle)
{
	struct drm_gem_object *gem;
	struct tegra_bo *bo;

	gem = drm_gem_object_lookup(drm, file, handle);
	if (!gem)
		return NULL;

	mutex_lock(&drm->struct_mutex);
	drm_gem_object_unreference(gem);
	mutex_unlock(&drm->struct_mutex);

	bo = to_tegra_bo(gem);
	return &bo->base;
}

static int gr2d_is_addr_reg(struct device *dev, u32 class, u32 offset)
{
	struct gr2d *gr2d = dev_get_drvdata(dev);

	switch (class) {
	case HOST1X_CLASS_HOST1X:
		if (offset == 0x2b)
			return 1;

		break;

	case HOST1X_CLASS_GR2D:
	case HOST1X_CLASS_GR2D_SB:
		if (offset >= GR2D_NUM_REGS)
			break;

		if (test_bit(offset, gr2d->addr_regs))
			return 1;

		break;
	}

	return 0;
}

static int gr2d_submit(struct tegra_drm_context *context,
		       struct drm_tegra_submit *args, struct drm_device *drm,
		       struct drm_file *file)
{
	unsigned int num_cmdbufs = args->num_cmdbufs;
	unsigned int num_relocs = args->num_relocs;
	unsigned int num_waitchks = args->num_waitchks;
	struct drm_tegra_cmdbuf __user *cmdbufs =
		(void * __user)(uintptr_t)args->cmdbufs;
	struct drm_tegra_reloc __user *relocs =
		(void * __user)(uintptr_t)args->relocs;
	struct drm_tegra_waitchk __user *waitchks =
		(void * __user)(uintptr_t)args->waitchks;
	struct drm_tegra_syncpt syncpt;
	struct host1x_job *job;
	int err;

	/* We don't yet support other than one syncpt_incr struct per submit */
	if (args->num_syncpts != 1)
		return -EINVAL;

	job = host1x_job_alloc(context->channel, args->num_cmdbufs,
			       args->num_relocs, args->num_waitchks);
	if (!job)
		return -ENOMEM;

	job->num_relocs = args->num_relocs;
	job->num_waitchk = args->num_waitchks;
	job->client = (u32)args->context;
	job->class = context->client->base.class;
	job->serialize = true;

	while (num_cmdbufs) {
		struct drm_tegra_cmdbuf cmdbuf;
		struct host1x_bo *bo;

		err = copy_from_user(&cmdbuf, cmdbufs, sizeof(cmdbuf));
		if (err)
			goto fail;

		bo = host1x_bo_lookup(drm, file, cmdbuf.handle);
		if (!bo) {
			err = -ENOENT;
			goto fail;
		}

		host1x_job_add_gather(job, bo, cmdbuf.words, cmdbuf.offset);
		num_cmdbufs--;
		cmdbufs++;
	}

	err = copy_from_user(job->relocarray, relocs,
			     sizeof(*relocs) * num_relocs);
	if (err)
		goto fail;

	while (num_relocs--) {
		struct host1x_reloc *reloc = &job->relocarray[num_relocs];
		struct host1x_bo *cmdbuf, *target;

		cmdbuf = host1x_bo_lookup(drm, file, (u32)reloc->cmdbuf);
		target = host1x_bo_lookup(drm, file, (u32)reloc->target);

		reloc->cmdbuf = cmdbuf;
		reloc->target = target;

		if (!reloc->target || !reloc->cmdbuf) {
			err = -ENOENT;
			goto fail;
		}
	}

	err = copy_from_user(job->waitchk, waitchks,
			     sizeof(*waitchks) * num_waitchks);
	if (err)
		goto fail;

	err = copy_from_user(&syncpt, (void * __user)(uintptr_t)args->syncpts,
			     sizeof(syncpt));
	if (err)
		goto fail;

	job->syncpt_id = syncpt.id;
	job->syncpt_incrs = syncpt.incrs;
	job->timeout = 10000;
	job->is_addr_reg = gr2d_is_addr_reg;

	if (args->timeout && args->timeout < 10000)
		job->timeout = args->timeout;

	err = host1x_job_pin(job, context->client->base.dev);
	if (err)
		goto fail;

	err = host1x_job_submit(job);
	if (err)
		goto fail_submit;

	args->fence = job->syncpt_end;

	host1x_job_put(job);
	return 0;

fail_submit:
	host1x_job_unpin(job);
fail:
	host1x_job_put(job);
	return err;
}

static const struct tegra_drm_client_ops gr2d_ops = {
	.open_channel = gr2d_open_channel,
	.close_channel = gr2d_close_channel,
	.submit = gr2d_submit,
};

static const struct of_device_id gr2d_match[] = {
	{ .compatible = "nvidia,tegra30-gr2d" },
	{ .compatible = "nvidia,tegra20-gr2d" },
	{ },
};

static const u32 gr2d_addr_regs[] = {
	0x1a, 0x1b, 0x26, 0x2b, 0x2c, 0x2d, 0x31, 0x32,
	0x48, 0x49, 0x4a, 0x4b, 0x4c
};

static int gr2d_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct gr2d *gr2d;
	unsigned int i;
	int err;

	gr2d = devm_kzalloc(dev, sizeof(*gr2d), GFP_KERNEL);
	if (!gr2d)
		return -ENOMEM;

	syncpts = devm_kzalloc(dev, sizeof(*syncpts), GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	gr2d->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(gr2d->clk)) {
		dev_err(dev, "cannot get clock\n");
		return PTR_ERR(gr2d->clk);
	}

	err = clk_prepare_enable(gr2d->clk);
	if (err) {
		dev_err(dev, "cannot turn on clock\n");
		return err;
	}

	INIT_LIST_HEAD(&gr2d->client.base.list);
	gr2d->client.base.ops = &gr2d_client_ops;
	gr2d->client.base.dev = dev;
	gr2d->client.base.class = HOST1X_CLASS_GR2D;
	gr2d->client.base.syncpts = syncpts;
	gr2d->client.base.num_syncpts = 1;

	INIT_LIST_HEAD(&gr2d->client.list);
	gr2d->client.ops = &gr2d_ops;

	err = host1x_client_register(&gr2d->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		return err;
	}

	/* initialize address register map */
	for (i = 0; i < ARRAY_SIZE(gr2d_addr_regs); i++)
		set_bit(gr2d_addr_regs[i], gr2d->addr_regs);

	platform_set_drvdata(pdev, gr2d);

	return 0;
}

static int gr2d_remove(struct platform_device *pdev)
{
	struct gr2d *gr2d = platform_get_drvdata(pdev);
	int err;

	err = host1x_client_unregister(&gr2d->client.base);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);
		return err;
	}

	clk_disable_unprepare(gr2d->clk);

	return 0;
}

struct platform_driver tegra_gr2d_driver = {
	.driver = {
		.name = "tegra-gr2d",
		.of_match_table = gr2d_match,
	},
	.probe = gr2d_probe,
	.remove = gr2d_remove,
};

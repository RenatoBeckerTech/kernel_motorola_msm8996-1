/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/bitmap.h>
#include <linux/of.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>

#include "coresight-priv.h"

#define tpdm_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define tpdm_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define TPDM_LOCK(drvdata)						\
do {									\
	mb();								\
	tpdm_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define TPDM_UNLOCK(drvdata)						\
do {									\
	tpdm_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

/* GPR Registers */
#define TPDM_GPR_CR(n)		(0x0 + (n * 4))

/* BC Subunit Registers */
#define TPDM_BC_CR		(0x280)
#define TPDM_BC_SATROLL		(0x284)
#define TPDM_BC_CNTENSET	(0x288)
#define TPDM_BC_CNTENCLR	(0x28C)
#define TPDM_BC_INTENSET	(0x290)
#define TPDM_BC_INTENCLR	(0x294)
#define TPDM_BC_TRIG_LO(n)	(0x298 + (n * 4))
#define TPDM_BC_TRIG_HI(n)	(0x318 + (n * 4))
#define TPDM_BC_GANG		(0x398)
#define TPDM_BC_OVERFLOW(n)	(0x39C + (n * 4))
#define TPDM_BC_OVSR		(0x3C0)
#define TPDM_BC_SELR		(0x3C4)
#define TPDM_BC_CNTR_LO		(0x3C8)
#define TPDM_BC_CNTR_HI		(0x3CC)
#define TPDM_BC_SHADOW_LO(n)	(0x3D0 + (n * 4))
#define TPDM_BC_SHADOW_HI(n)	(0x450 + (n * 4))
#define TPDM_BC_SWINC		(0x4D0)

/* TC Subunit Registers */
#define TPDM_TC_CR		(0x500)
#define TPDM_TC_CNTENSET	(0x504)
#define TPDM_TC_CNTENCLR	(0x508)
#define TPDM_TC_INTENSET	(0x50C)
#define TPDM_TC_INTENCLR	(0x510)
#define TPDM_TC_TRIG_SEL(n)	(0x514 + (n * 4))
#define TPDM_TC_TRIG_LO(n)	(0x534 + (n * 4))
#define TPDM_TC_TRIG_HI(n)	(0x554 + (n * 4))
#define TPDM_TC_OVSR_GP		(0x580)
#define TPDM_TC_OVSR_IMPL	(0x584)
#define TPDM_TC_SELR		(0x588)
#define TPDM_TC_CNTR_LO		(0x58C)
#define TPDM_TC_CNTR_HI		(0x590)
#define TPDM_TC_SHADOW_LO(n)	(0x594 + (n * 4))
#define TPDM_TC_SHADOW_HI(n)	(0x644 + (n * 4))
#define TPDM_TC_SWINC		(0x700)

/* DSB Subunit Registers */
#define TPDM_DSB_CR		(0x780)
#define TPDM_DSB_TIER		(0x784)
#define TPDM_DSB_TPR(n)		(0x788 + (n * 4))
#define TPDM_DSB_TPMR(n)	(0x7A8 + (n * 4))
#define TPDM_DSB_XPR(n)		(0x7C8 + (n * 4))
#define TPDM_DSB_XPMR(n)	(0x7E8 + (n * 4))
#define TPDM_DSB_EDCR(n)	(0x808 + (n * 4))
#define TPDM_DSB_EDCMR(n)	(0x848 + (n * 4))
#define TPDM_DSB_CA_SELECT(n)	(0x86c + (n * 4))

/* CMB Subunit Registers */
#define TPDM_CMB_CR		(0xA00)
#define TPDM_CMB_TIER		(0xA04)
#define TPDM_CMB_TPR(n)		(0xA08 + (n * 4))
#define TPDM_CMB_TPMR(n)	(0xA10 + (n * 4))
#define TPDM_CMB_XPR(n)		(0xA18 + (n * 4))
#define TPDM_CMB_XPMR(n)	(0xA20 + (n * 4))

/* TPDM Specific Registers */
#define TPDM_ITATBCNTRL		(0xEF0)
#define TPDM_CLK_CTRL		(0x220)

#define TPDM_DATASETS		32
#define TPDM_BC_MAX_COUNTERS	32
#define TPDM_BC_MAX_OVERFLOW	6
#define TPDM_TC_MAX_COUNTERS	44
#define TPDM_TC_MAX_TRIG	8
#define TPDM_DSB_MAX_PATT	8
#define TPDM_DSB_MAX_SELECT	8
#define TPDM_DSB_MAX_EDCR	16
#define TPDM_DSB_MAX_LINES	256
#define TPDM_CMB_PATT_CMP	2

/* DSB programming modes */
#define TPDM_DSB_MODE_CYCACC(val)	BMVAL(val, 0, 2)
#define TPDM_DSB_MODE_PERF		BIT(3)
#define TPDM_DSB_MODE_HPBYTESEL(val)	BMVAL(val, 4, 8)
#define TPDM_MODE_ALL			(0xFFFFFFF)

#define NUM_OF_BITS		32
#define TPDM_GPR_REGS_MAX	160

enum tpdm_dataset {
	TPDM_DS_IMPLDEF,
	TPDM_DS_DSB,
	TPDM_DS_CMB,
	TPDM_DS_TC,
	TPDM_DS_BC,
	TPDM_DS_GPR,
};

enum tpdm_mode {
	TPDM_MODE_ATB,
	TPDM_MODE_APB,
};

enum tpdm_support_type {
	TPDM_SUPPORT_TYPE_FULL,
	TPDM_SUPPORT_TYPE_PARTIAL,
	TPDM_SUPPORT_TYPE_NO,
};

enum tpdm_cmb_mode {
	TPDM_CMB_MODE_CONTINUOUS,
	TPDM_CMB_MODE_TRACE_ON_CHANGE,
};

enum tpdm_cmb_patt_bits {
	TPDM_CMB_LSB,
	TPDM_CMB_MSB,
};

#ifdef CONFIG_CORESIGHT_TPDM_DEFAULT_ENABLE
static int boot_enable = 1;
#else
static int boot_enable;
#endif

module_param_named(
	boot_enable, boot_enable, int, S_IRUGO
);

struct gpr_dataset {
	DECLARE_BITMAP(gpr_dirty, TPDM_GPR_REGS_MAX);
	uint32_t		gp_regs[TPDM_GPR_REGS_MAX];
};

struct bc_dataset {
	enum tpdm_mode		capture_mode;
	enum tpdm_mode		retrieval_mode;
	uint32_t		sat_mode;
	uint32_t		enable_counters;
	uint32_t		clear_counters;
	uint32_t		enable_irq;
	uint32_t		clear_irq;
	uint32_t		trig_val_lo[TPDM_BC_MAX_COUNTERS];
	uint32_t		trig_val_hi[TPDM_BC_MAX_COUNTERS];
	uint32_t		enable_ganging;
	uint32_t		overflow_val[TPDM_BC_MAX_OVERFLOW];
};

struct tc_dataset {
	enum tpdm_mode		capture_mode;
	enum tpdm_mode		retrieval_mode;
	bool			sat_mode;
	uint32_t		enable_counters;
	uint32_t		clear_counters;
	uint32_t		enable_irq;
	uint32_t		clear_irq;
	uint32_t		trig_sel[TPDM_TC_MAX_TRIG];
	uint32_t		trig_val_lo[TPDM_TC_MAX_TRIG];
	uint32_t		trig_val_hi[TPDM_TC_MAX_TRIG];
};

struct dsb_dataset {
	uint32_t		mode;
	uint32_t		edge_ctrl[TPDM_DSB_MAX_EDCR];
	uint32_t		edge_ctrl_mask[TPDM_DSB_MAX_EDCR / 2];
	uint32_t		patt_val[TPDM_DSB_MAX_PATT];
	uint32_t		patt_mask[TPDM_DSB_MAX_PATT];
	bool			patt_ts;
	uint32_t		trig_patt_val[TPDM_DSB_MAX_PATT];
	uint32_t		trig_patt_mask[TPDM_DSB_MAX_PATT];
	bool			trig_ts;
	uint32_t		select_val[TPDM_DSB_MAX_SELECT];
};

struct cmb_dataset {
	enum tpdm_cmb_mode	mode;
	uint32_t		patt_val[TPDM_CMB_PATT_CMP];
	uint32_t		patt_mask[TPDM_CMB_PATT_CMP];
	bool			patt_ts;
	uint32_t		trig_patt_val[TPDM_CMB_PATT_CMP];
	uint32_t		trig_patt_mask[TPDM_CMB_PATT_CMP];
	bool			trig_ts;
};

struct tpdm_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct clk		*clk;
	struct mutex		lock;
	bool			enable;
	bool			clk_enable;
	DECLARE_BITMAP(datasets, TPDM_DATASETS);
	DECLARE_BITMAP(enable_ds, TPDM_DATASETS);
	enum tpdm_support_type	tc_trig_type;
	enum tpdm_support_type	bc_trig_type;
	enum tpdm_support_type	bc_gang_type;
	uint32_t		bc_counters_avail;
	uint32_t		tc_counters_avail;
	struct gpr_dataset	*gpr;
	struct bc_dataset	*bc;
	struct tc_dataset	*tc;
	struct dsb_dataset	*dsb;
	struct cmb_dataset	*cmb;
};

static void __tpdm_enable_gpr(struct tpdm_drvdata *drvdata)
{
	int i;

	for (i = 0; i < TPDM_GPR_REGS_MAX; i++) {
		if (!test_bit(i, drvdata->gpr->gpr_dirty))
			continue;
		tpdm_writel(drvdata, drvdata->gpr->gp_regs[i], TPDM_GPR_CR(i));
	}
}

static void __tpdm_enable_bc(struct tpdm_drvdata *drvdata)
{
	int i;
	uint32_t val;

	if (drvdata->bc->sat_mode)
		tpdm_writel(drvdata, drvdata->bc->sat_mode,
			    TPDM_BC_SATROLL);
	else
		tpdm_writel(drvdata, 0x0, TPDM_BC_SATROLL);

	if (drvdata->bc->enable_counters) {
		tpdm_writel(drvdata, 0xFFFFFFFF, TPDM_BC_CNTENCLR);
		tpdm_writel(drvdata, drvdata->bc->enable_counters,
			    TPDM_BC_CNTENSET);
	}
	if (drvdata->bc->clear_counters)
		tpdm_writel(drvdata, drvdata->bc->clear_counters,
			    TPDM_BC_CNTENCLR);

	if (drvdata->bc->enable_irq) {
		tpdm_writel(drvdata, 0xFFFFFFFF, TPDM_BC_INTENCLR);
		tpdm_writel(drvdata, drvdata->bc->enable_irq,
			    TPDM_BC_INTENSET);
	}
	if (drvdata->bc->clear_irq)
		tpdm_writel(drvdata, drvdata->bc->clear_irq,
			    TPDM_BC_INTENCLR);

	if (drvdata->bc_trig_type == TPDM_SUPPORT_TYPE_FULL) {
		for (i = 0; i < drvdata->bc_counters_avail; i++) {
			tpdm_writel(drvdata, drvdata->bc->trig_val_lo[i],
				    TPDM_BC_TRIG_LO(i));
			tpdm_writel(drvdata, drvdata->bc->trig_val_hi[i],
				    TPDM_BC_TRIG_HI(i));
		}
	} else if (drvdata->bc_trig_type == TPDM_SUPPORT_TYPE_PARTIAL) {
		tpdm_writel(drvdata, drvdata->bc->trig_val_lo[0],
			    TPDM_BC_TRIG_LO(0));
		tpdm_writel(drvdata, drvdata->bc->trig_val_hi[0],
			    TPDM_BC_TRIG_HI(0));
	}

	if (drvdata->bc->enable_ganging)
		tpdm_writel(drvdata, drvdata->bc->enable_ganging, TPDM_BC_GANG);

	for (i = 0; i < TPDM_BC_MAX_OVERFLOW; i++)
		tpdm_writel(drvdata, drvdata->bc->overflow_val[i],
			    TPDM_BC_OVERFLOW(i));

	val = tpdm_readl(drvdata, TPDM_BC_CR);
	if (drvdata->bc->retrieval_mode == TPDM_MODE_APB)
		val = val | BIT(2);
	else
		val = val & ~BIT(2);
	tpdm_writel(drvdata, val, TPDM_BC_CR);

	val = tpdm_readl(drvdata, TPDM_BC_CR);
	/* Set the enable bit */
	val = val | BIT(0);
	tpdm_writel(drvdata, val, TPDM_BC_CR);
}

static void __tpdm_enable_tc(struct tpdm_drvdata *drvdata)
{
	int i;
	uint32_t val;

	if (drvdata->tc->enable_counters) {
		tpdm_writel(drvdata, 0xF, TPDM_TC_CNTENCLR);
		tpdm_writel(drvdata, drvdata->tc->enable_counters,
			    TPDM_TC_CNTENSET);
	}
	if (drvdata->tc->clear_counters)
		tpdm_writel(drvdata, drvdata->tc->clear_counters,
			    TPDM_TC_CNTENCLR);

	if (drvdata->tc->enable_irq) {
		tpdm_writel(drvdata, 0xF, TPDM_TC_INTENCLR);
		tpdm_writel(drvdata, drvdata->tc->enable_irq,
			    TPDM_TC_INTENSET);
	}
	if (drvdata->tc->clear_irq)
		tpdm_writel(drvdata, drvdata->tc->clear_irq,
			    TPDM_TC_INTENCLR);

	if (drvdata->tc_trig_type == TPDM_SUPPORT_TYPE_FULL) {
		for (i = 0; i < TPDM_TC_MAX_TRIG; i++) {
			tpdm_writel(drvdata, drvdata->tc->trig_sel[i],
				    TPDM_TC_TRIG_SEL(i));
			tpdm_writel(drvdata, drvdata->tc->trig_val_lo[i],
				    TPDM_TC_TRIG_LO(i));
			tpdm_writel(drvdata, drvdata->tc->trig_val_hi[i],
				    TPDM_TC_TRIG_HI(i));
		}
	} else if (drvdata->tc_trig_type == TPDM_SUPPORT_TYPE_PARTIAL) {
		tpdm_writel(drvdata, drvdata->tc->trig_sel[0],
			    TPDM_TC_TRIG_SEL(0));
		tpdm_writel(drvdata, drvdata->tc->trig_val_lo[0],
			    TPDM_TC_TRIG_LO(0));
		tpdm_writel(drvdata, drvdata->tc->trig_val_hi[0],
			    TPDM_TC_TRIG_HI(0));
	}

	val = tpdm_readl(drvdata, TPDM_TC_CR);
	if (drvdata->tc->sat_mode)
		val = val | BIT(4);
	else
		val = val & ~BIT(4);
	if (drvdata->tc->retrieval_mode == TPDM_MODE_APB)
		val = val | BIT(2);
	else
		val = val & ~BIT(2);
	tpdm_writel(drvdata, val, TPDM_TC_CR);

	val = tpdm_readl(drvdata, TPDM_TC_CR);
	/* Set the enable bit */
	val = val | BIT(0);
	tpdm_writel(drvdata, val, TPDM_TC_CR);
}

static void __tpdm_enable_dsb(struct tpdm_drvdata *drvdata)
{
	uint32_t val, mode, i;

	for (i = 0; i < TPDM_DSB_MAX_EDCR; i++)
		tpdm_writel(drvdata, drvdata->dsb->edge_ctrl[i],
			    TPDM_DSB_EDCR(i));
	for (i = 0; i < TPDM_DSB_MAX_EDCR / 2; i++)
		tpdm_writel(drvdata, drvdata->dsb->edge_ctrl_mask[i],
			    TPDM_DSB_EDCMR(i));

	for (i = 0; i < TPDM_DSB_MAX_PATT; i++) {
		tpdm_writel(drvdata, drvdata->dsb->patt_val[i],
			    TPDM_DSB_TPR(i));
		tpdm_writel(drvdata, drvdata->dsb->patt_mask[i],
			    TPDM_DSB_TPMR(i));
	}

	for (i = 0; i < TPDM_DSB_MAX_PATT; i++) {
		tpdm_writel(drvdata, drvdata->dsb->trig_patt_val[i],
			    TPDM_DSB_XPR(i));
		tpdm_writel(drvdata, drvdata->dsb->trig_patt_mask[i],
			    TPDM_DSB_XPMR(i));
	}

	for (i = 0; i < TPDM_DSB_MAX_SELECT; i++)
		tpdm_writel(drvdata, drvdata->dsb->select_val[i],
			    TPDM_DSB_CA_SELECT(i));

	val = tpdm_readl(drvdata, TPDM_DSB_TIER);
	if (drvdata->dsb->patt_ts == true)
		val = val | BIT(0);
	else
		val = val & ~BIT(0);
	if (drvdata->dsb->trig_ts == true)
		val = val | BIT(1);
	else
		val = val & ~BIT(1);
	tpdm_writel(drvdata, val, TPDM_DSB_TIER);

	val = tpdm_readl(drvdata, TPDM_DSB_CR);
	/* Set the cycle accurate mode */
	mode = TPDM_DSB_MODE_CYCACC(drvdata->dsb->mode);
	val = val & ~(0x7 << 9);
	val = val | (mode << 9);
	/* Set the byte lane for high-performance mode */
	mode = TPDM_DSB_MODE_HPBYTESEL(drvdata->dsb->mode);
	val = val & ~(0x1F << 2);
	val = val | (mode << 2);
	/* Set the performance mode */
	if (drvdata->dsb->mode & TPDM_DSB_MODE_PERF)
		val = val | BIT(1);
	else
		val = val & ~BIT(1);
	tpdm_writel(drvdata, val, TPDM_DSB_CR);

	val = tpdm_readl(drvdata, TPDM_DSB_CR);
	/* Set the enable bit */
	val = val | BIT(0);
	tpdm_writel(drvdata, val, TPDM_DSB_CR);
}

static void __tpdm_enable_cmb(struct tpdm_drvdata *drvdata)
{
	uint32_t val;

	tpdm_writel(drvdata, drvdata->cmb->patt_val[TPDM_CMB_LSB],
		    TPDM_CMB_TPR(TPDM_CMB_LSB));
	tpdm_writel(drvdata, drvdata->cmb->patt_mask[TPDM_CMB_LSB],
		    TPDM_CMB_TPMR(TPDM_CMB_LSB));
	tpdm_writel(drvdata, drvdata->cmb->patt_val[TPDM_CMB_MSB],
		    TPDM_CMB_TPR(TPDM_CMB_MSB));
	tpdm_writel(drvdata, drvdata->cmb->patt_mask[TPDM_CMB_MSB],
		    TPDM_CMB_TPMR(TPDM_CMB_MSB));

	tpdm_writel(drvdata, drvdata->cmb->trig_patt_val[TPDM_CMB_LSB],
		    TPDM_CMB_XPR(TPDM_CMB_LSB));
	tpdm_writel(drvdata, drvdata->cmb->trig_patt_mask[TPDM_CMB_LSB],
		    TPDM_CMB_XPMR(TPDM_CMB_LSB));
	tpdm_writel(drvdata, drvdata->cmb->trig_patt_val[TPDM_CMB_MSB],
		    TPDM_CMB_XPR(TPDM_CMB_MSB));
	tpdm_writel(drvdata, drvdata->cmb->trig_patt_mask[TPDM_CMB_MSB],
		    TPDM_CMB_XPMR(TPDM_CMB_MSB));

	val = tpdm_readl(drvdata, TPDM_CMB_TIER);
	if (drvdata->cmb->patt_ts == true)
		val = val | BIT(0);
	else
		val = val & ~BIT(0);
	if (drvdata->cmb->trig_ts == true)
		val = val | BIT(1);
	else
		val = val & ~BIT(1);
	tpdm_writel(drvdata, val, TPDM_CMB_TIER);

	val = tpdm_readl(drvdata, TPDM_CMB_CR);
	/* Set the flow control bit */
	val = val & ~BIT(2);
	if (drvdata->cmb->mode == TPDM_CMB_MODE_CONTINUOUS)
		val = val & ~BIT(1);
	else
		val = val | BIT(1);
	tpdm_writel(drvdata, val, TPDM_CMB_CR);
	/* Set the enable bit */
	val = val | BIT(0);
	tpdm_writel(drvdata, val, TPDM_CMB_CR);
}

static void __tpdm_enable(struct tpdm_drvdata *drvdata)
{
	TPDM_UNLOCK(drvdata);

	if (drvdata->clk_enable)
		tpdm_writel(drvdata, 0x1, TPDM_CLK_CTRL);

	if (test_bit(TPDM_DS_GPR, drvdata->enable_ds))
		__tpdm_enable_gpr(drvdata);

	if (test_bit(TPDM_DS_BC, drvdata->enable_ds))
		__tpdm_enable_bc(drvdata);

	if (test_bit(TPDM_DS_TC, drvdata->enable_ds))
		__tpdm_enable_tc(drvdata);

	if (test_bit(TPDM_DS_DSB, drvdata->enable_ds))
		__tpdm_enable_dsb(drvdata);

	if (test_bit(TPDM_DS_CMB, drvdata->enable_ds))
		__tpdm_enable_cmb(drvdata);

	TPDM_LOCK(drvdata);
}

static int tpdm_enable(struct coresight_device *csdev)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	mutex_lock(&drvdata->lock);
	__tpdm_enable(drvdata);
	drvdata->enable = true;
	mutex_unlock(&drvdata->lock);

	dev_info(drvdata->dev, "TPDM tracing enabled\n");
	return 0;
}

static void __tpdm_disable_bc(struct tpdm_drvdata *drvdata)
{
	uint32_t config;

	config = tpdm_readl(drvdata, TPDM_BC_CR);
	config = config & ~BIT(0);
	tpdm_writel(drvdata, config, TPDM_BC_CR);
}

static void __tpdm_disable_tc(struct tpdm_drvdata *drvdata)
{
	uint32_t config;

	config = tpdm_readl(drvdata, TPDM_TC_CR);
	config = config & ~BIT(0);
	tpdm_writel(drvdata, config, TPDM_TC_CR);
}

static void __tpdm_disable_dsb(struct tpdm_drvdata *drvdata)
{
	uint32_t config;

	config = tpdm_readl(drvdata, TPDM_DSB_CR);
	config = config & ~BIT(0);
	tpdm_writel(drvdata, config, TPDM_DSB_CR);
}

static void __tpdm_disable_cmb(struct tpdm_drvdata *drvdata)
{
	uint32_t config;

	config = tpdm_readl(drvdata, TPDM_CMB_CR);
	config = config & ~BIT(0);
	tpdm_writel(drvdata, config, TPDM_CMB_CR);
}

static void __tpdm_disable(struct tpdm_drvdata *drvdata)
{
	TPDM_UNLOCK(drvdata);

	if (test_bit(TPDM_DS_BC, drvdata->enable_ds))
		__tpdm_disable_bc(drvdata);

	if (test_bit(TPDM_DS_TC, drvdata->enable_ds))
		__tpdm_disable_tc(drvdata);

	if (test_bit(TPDM_DS_DSB, drvdata->enable_ds))
		__tpdm_disable_dsb(drvdata);

	if (test_bit(TPDM_DS_CMB, drvdata->enable_ds))
		__tpdm_disable_cmb(drvdata);

	if (drvdata->clk_enable)
		tpdm_writel(drvdata, 0x0, TPDM_CLK_CTRL);

	TPDM_LOCK(drvdata);
}

static void tpdm_disable(struct coresight_device *csdev)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	mutex_lock(&drvdata->lock);
	__tpdm_disable(drvdata);
	drvdata->enable = false;
	mutex_unlock(&drvdata->lock);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "TPDM tracing disabled\n");
}

static const struct coresight_ops_source tpdm_source_ops = {
	.enable		= tpdm_enable,
	.disable	= tpdm_disable,
};

static const struct coresight_ops tpdm_cs_ops = {
	.source_ops	= &tpdm_source_ops,
};

static ssize_t tpdm_show_available_datasets(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;

	if (test_bit(TPDM_DS_IMPLDEF, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s",
				  "IMPLDEF");

	if (test_bit(TPDM_DS_DSB, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "DSB");

	if (test_bit(TPDM_DS_CMB, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "CMB");

	if (test_bit(TPDM_DS_TC, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "TC");

	if (test_bit(TPDM_DS_BC, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "BC");

	if (test_bit(TPDM_DS_GPR, drvdata->datasets))
		size += scnprintf(buf + size, PAGE_SIZE - size, "%-8s", "GPR");

	size += scnprintf(buf + size, PAGE_SIZE - size, "\n");
	return size;
}
static DEVICE_ATTR(available_datasets, S_IRUGO, tpdm_show_available_datasets,
		   NULL);

static ssize_t tpdm_show_enable_datasets(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size;

	size = bitmap_scnprintf(buf, PAGE_SIZE, drvdata->enable_ds,
				TPDM_DATASETS);

	if (PAGE_SIZE - size < 2)
		size = -EINVAL;
	else
		size += scnprintf(buf + size, 2, "\n");
	return size;
}

static ssize_t tpdm_store_enable_datasets(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int i;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	mutex_lock(&drvdata->lock);
	if (drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	for (i = 0; i < TPDM_DATASETS; i++) {
		if (test_bit(i, drvdata->datasets) && (val & BIT(i)))
			__set_bit(i, drvdata->enable_ds);
		else
			__clear_bit(i, drvdata->enable_ds);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(enable_datasets, S_IRUGO | S_IWUSR,
		   tpdm_show_enable_datasets, tpdm_store_enable_datasets);

static ssize_t tpdm_show_gp_regs(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_GPR, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_GPR_REGS_MAX; i++) {
		if (!test_bit(i, drvdata->gpr->gpr_dirty))
			continue;
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->gpr->gp_regs[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_gp_regs(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_GPR, drvdata->datasets) ||
	    index >= TPDM_GPR_REGS_MAX)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->gpr->gp_regs[index] = val;
	__set_bit(index, drvdata->gpr->gpr_dirty);
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(gp_regs, S_IRUGO | S_IWUSR, tpdm_show_gp_regs,
		   tpdm_store_gp_regs);

static ssize_t tpdm_show_bc_capture_mode(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 drvdata->bc->capture_mode == TPDM_MODE_ATB ?
			 "ATB" : "APB");
}

static ssize_t tpdm_store_bc_capture_mode(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[20] = "";
	uint32_t val;

	if (size >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (!strcmp(str, "ATB")) {
		drvdata->bc->capture_mode = TPDM_MODE_ATB;
	} else if (!strcmp(str, "APB") &&
		   drvdata->bc->retrieval_mode == TPDM_MODE_APB) {

		TPDM_UNLOCK(drvdata);
		val = tpdm_readl(drvdata, TPDM_BC_CR);
		val = val | BIT(3);
		tpdm_writel(drvdata, val, TPDM_BC_CR);
		TPDM_LOCK(drvdata);

		drvdata->bc->capture_mode = TPDM_MODE_APB;
	} else {
		mutex_unlock(&drvdata->lock);
		return -EINVAL;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_capture_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_capture_mode, tpdm_store_bc_capture_mode);

static ssize_t tpdm_show_bc_retrieval_mode(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 drvdata->bc->retrieval_mode == TPDM_MODE_ATB ?
			 "ATB" : "APB");
}

static ssize_t tpdm_store_bc_retrieval_mode(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[20] = "";

	if (size >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (!strcmp(str, "ATB")) {
		drvdata->bc->retrieval_mode = TPDM_MODE_ATB;
	} else if (!strcmp(str, "APB")) {
		drvdata->bc->retrieval_mode = TPDM_MODE_APB;
	} else {
		mutex_unlock(&drvdata->lock);
		return -EINVAL;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_retrieval_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_retrieval_mode, tpdm_store_bc_retrieval_mode);

static ssize_t tpdm_store_bc_reset_counters(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		val = tpdm_readl(drvdata, TPDM_BC_CR);
		val = val | BIT(1);
		tpdm_writel(drvdata, val, TPDM_BC_CR);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_reset_counters, S_IRUGO | S_IWUSR, NULL,
		   tpdm_store_bc_reset_counters);

static ssize_t tpdm_show_bc_sat_mode(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->bc->sat_mode);
}

static ssize_t tpdm_store_bc_sat_mode(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->sat_mode = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_sat_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_sat_mode, tpdm_store_bc_sat_mode);

static ssize_t tpdm_show_bc_enable_counters(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->bc->enable_counters);
}

static ssize_t tpdm_store_bc_enable_counters(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf,
					     size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->enable_counters = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_enable_counters, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_enable_counters, tpdm_store_bc_enable_counters);

static ssize_t tpdm_show_bc_clear_counters(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->bc->clear_counters);
}

static ssize_t tpdm_store_bc_clear_counters(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->clear_counters = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_clear_counters, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_clear_counters, tpdm_store_bc_clear_counters);

static ssize_t tpdm_show_bc_enable_irq(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->bc->enable_irq);
}

static ssize_t tpdm_store_bc_enable_irq(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->enable_irq = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_enable_irq, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_enable_irq, tpdm_store_bc_enable_irq);

static ssize_t tpdm_show_bc_clear_irq(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->bc->clear_irq);
}

static ssize_t tpdm_store_bc_clear_irq(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->clear_irq = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_clear_irq, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_clear_irq, tpdm_store_bc_clear_irq);

static ssize_t tpdm_show_bc_trig_val_lo(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_BC_MAX_COUNTERS; i++)
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->bc->trig_val_lo[i]);
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_bc_trig_val_lo(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets) ||
	    index >= drvdata->bc_counters_avail ||
	    drvdata->bc_trig_type == TPDM_SUPPORT_TYPE_NO ||
	    (drvdata->bc_trig_type == TPDM_SUPPORT_TYPE_PARTIAL && index > 0))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->trig_val_lo[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_trig_val_lo, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_trig_val_lo, tpdm_store_bc_trig_val_lo);

static ssize_t tpdm_show_bc_trig_val_hi(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_BC_MAX_COUNTERS; i++)
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->bc->trig_val_hi[i]);
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_bc_trig_val_hi(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets) ||
	    index >= drvdata->bc_counters_avail ||
	    drvdata->bc_trig_type == TPDM_SUPPORT_TYPE_NO ||
	    (drvdata->bc_trig_type == TPDM_SUPPORT_TYPE_PARTIAL && index > 0))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->trig_val_hi[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_trig_val_hi, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_trig_val_hi, tpdm_store_bc_trig_val_hi);

static ssize_t tpdm_show_bc_enable_ganging(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->bc->enable_ganging);
}

static ssize_t tpdm_store_bc_enable_ganging(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->enable_ganging = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_enable_ganging, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_enable_ganging, tpdm_store_bc_enable_ganging);

static ssize_t tpdm_show_bc_overflow_val(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_BC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_BC_MAX_OVERFLOW; i++)
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->bc->overflow_val[i]);
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_bc_overflow_val(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->datasets) ||
	    index >= TPDM_BC_MAX_OVERFLOW)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->bc->overflow_val[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_overflow_val, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_overflow_val, tpdm_store_bc_overflow_val);

static ssize_t tpdm_show_bc_ovsr(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_BC_OVSR);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_bc_ovsr(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		tpdm_writel(drvdata, val, TPDM_BC_OVSR);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_ovsr, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_ovsr, tpdm_store_bc_ovsr);

static ssize_t tpdm_show_bc_counter_sel(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_BC_SELR);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_bc_counter_sel(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable || val >= drvdata->bc_counters_avail) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	tpdm_writel(drvdata, val, TPDM_BC_SELR);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_counter_sel, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_counter_sel, tpdm_store_bc_counter_sel);

static ssize_t tpdm_show_bc_count_val_lo(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_BC_CNTR_LO);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_bc_count_val_lo(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val, select;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		select = tpdm_readl(drvdata, TPDM_BC_SELR);

		/* Check if selected counter is disabled */
		if (BVAL(tpdm_readl(drvdata, TPDM_BC_CNTENSET), select)) {
			mutex_unlock(&drvdata->lock);
			return -EPERM;
		}

		tpdm_writel(drvdata, val, TPDM_BC_CNTR_LO);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_count_val_lo, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_count_val_lo, tpdm_store_bc_count_val_lo);

static ssize_t tpdm_show_bc_count_val_hi(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_BC_CNTR_HI);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_bc_count_val_hi(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val, select;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		select = tpdm_readl(drvdata, TPDM_BC_SELR);

		/* Check if selected counter is disabled */
		if (BVAL(tpdm_readl(drvdata, TPDM_BC_CNTENSET), select)) {
			mutex_unlock(&drvdata->lock);
			return -EPERM;
		}

		tpdm_writel(drvdata, val, TPDM_BC_CNTR_HI);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_count_val_hi, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_count_val_hi, tpdm_store_bc_count_val_hi);

static ssize_t tpdm_show_bc_shadow_val_lo(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	for (i = 0; i < drvdata->bc_counters_avail; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  tpdm_readl(drvdata, TPDM_BC_SHADOW_LO(i)));
	}
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_shadow_val_lo, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_shadow_val_lo, NULL);

static ssize_t tpdm_show_bc_shadow_val_hi(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	for (i = 0; i < drvdata->bc_counters_avail; i++)
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  tpdm_readl(drvdata, TPDM_BC_SHADOW_HI(i)));
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_shadow_val_hi, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_shadow_val_hi, NULL);

static ssize_t tpdm_show_bc_sw_inc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_BC_SWINC);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_bc_sw_inc(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_BC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		tpdm_writel(drvdata, val, TPDM_BC_SWINC);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(bc_sw_inc, S_IRUGO | S_IWUSR,
		   tpdm_show_bc_sw_inc, tpdm_store_bc_sw_inc);

static ssize_t tpdm_show_tc_capture_mode(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 drvdata->tc->capture_mode == TPDM_MODE_ATB ?
			 "ATB" : "APB");
}

static ssize_t tpdm_store_tc_capture_mode(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[20] = "";
	uint32_t val;

	if (size >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (!strcmp(str, "ATB")) {
		drvdata->tc->capture_mode = TPDM_MODE_ATB;
	} else if (!strcmp(str, "APB") &&
		   drvdata->tc->retrieval_mode == TPDM_MODE_APB) {

		TPDM_UNLOCK(drvdata);
		val = tpdm_readl(drvdata, TPDM_TC_CR);
		val = val | BIT(3);
		tpdm_writel(drvdata, val, TPDM_TC_CR);
		TPDM_LOCK(drvdata);

		drvdata->tc->capture_mode = TPDM_MODE_APB;
	} else {
		mutex_unlock(&drvdata->lock);
		return -EINVAL;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_capture_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_capture_mode, tpdm_store_tc_capture_mode);

static ssize_t tpdm_show_tc_retrieval_mode(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 drvdata->tc->retrieval_mode == TPDM_MODE_ATB ?
			 "ATB" : "APB");
}

static ssize_t tpdm_store_tc_retrieval_mode(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[20] = "";

	if (size >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (!strcmp(str, "ATB")) {
		drvdata->tc->retrieval_mode = TPDM_MODE_ATB;
	} else if (!strcmp(str, "APB")) {
		drvdata->tc->retrieval_mode = TPDM_MODE_APB;
	} else {
		mutex_unlock(&drvdata->lock);
		return -EINVAL;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_retrieval_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_retrieval_mode, tpdm_store_tc_retrieval_mode);

static ssize_t tpdm_store_tc_reset_counters(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		val = tpdm_readl(drvdata, TPDM_TC_CR);
		val = val | BIT(1);
		tpdm_writel(drvdata, val, TPDM_TC_CR);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_reset_counters, S_IRUGO | S_IWUSR, NULL,
		   tpdm_store_tc_reset_counters);

static ssize_t tpdm_show_tc_sat_mode(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->tc->sat_mode);
}

static ssize_t tpdm_store_tc_sat_mode(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->tc->sat_mode = true;
	else
		drvdata->tc->sat_mode = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_sat_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_sat_mode, tpdm_store_tc_sat_mode);

static ssize_t tpdm_show_tc_enable_counters(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->tc->enable_counters);
}

static ssize_t tpdm_store_tc_enable_counters(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf,
					     size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;
	if (val >> drvdata->tc_counters_avail)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->tc->enable_counters = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_enable_counters, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_enable_counters, tpdm_store_tc_enable_counters);

static ssize_t tpdm_show_tc_clear_counters(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->tc->clear_counters);
}

static ssize_t tpdm_store_tc_clear_counters(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;
	if (val >> drvdata->tc_counters_avail)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->tc->clear_counters = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_clear_counters, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_clear_counters, tpdm_store_tc_clear_counters);

static ssize_t tpdm_show_tc_enable_irq(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->tc->enable_irq);
}

static ssize_t tpdm_store_tc_enable_irq(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->tc->enable_irq = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_enable_irq, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_enable_irq, tpdm_store_tc_enable_irq);

static ssize_t tpdm_show_tc_clear_irq(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->tc->clear_irq);
}

static ssize_t tpdm_store_tc_clear_irq(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->tc->clear_irq = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_clear_irq, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_clear_irq, tpdm_store_tc_clear_irq);

static ssize_t tpdm_show_tc_trig_sel(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_TC_MAX_TRIG; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->tc->trig_sel[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_tc_trig_sel(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets) ||
	    index >= TPDM_TC_MAX_TRIG ||
	    drvdata->tc_trig_type == TPDM_SUPPORT_TYPE_NO ||
	    (drvdata->tc_trig_type == TPDM_SUPPORT_TYPE_PARTIAL && index > 0))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->tc->trig_sel[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_trig_sel, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_trig_sel, tpdm_store_tc_trig_sel);

static ssize_t tpdm_show_tc_trig_val_lo(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_TC_MAX_TRIG; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->tc->trig_val_lo[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_tc_trig_val_lo(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets) ||
	    index >= TPDM_TC_MAX_TRIG ||
	    drvdata->tc_trig_type == TPDM_SUPPORT_TYPE_NO ||
	    (drvdata->tc_trig_type == TPDM_SUPPORT_TYPE_PARTIAL && index > 0))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->tc->trig_val_lo[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_trig_val_lo, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_trig_val_lo, tpdm_store_tc_trig_val_lo);

static ssize_t tpdm_show_tc_trig_val_hi(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_TC_MAX_TRIG; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->tc->trig_val_hi[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_tc_trig_val_hi(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->datasets) ||
	    index >= TPDM_TC_MAX_TRIG ||
	    drvdata->tc_trig_type == TPDM_SUPPORT_TYPE_NO ||
	    (drvdata->tc_trig_type == TPDM_SUPPORT_TYPE_PARTIAL && index > 0))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->tc->trig_val_hi[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_trig_val_hi, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_trig_val_hi, tpdm_store_tc_trig_val_hi);

static ssize_t tpdm_show_tc_ovsr_gp(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_TC, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_TC_OVSR_GP);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_tc_ovsr_gp(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		tpdm_writel(drvdata, val, TPDM_TC_OVSR_GP);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_ovsr_gp, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_ovsr_gp, tpdm_store_tc_ovsr_gp);

static ssize_t tpdm_show_tc_ovsr_impl(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_TC_OVSR_IMPL);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_tc_ovsr_impl(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		tpdm_writel(drvdata, val, TPDM_TC_OVSR_IMPL);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_ovsr_impl, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_ovsr_impl, tpdm_store_tc_ovsr_impl);

static ssize_t tpdm_show_tc_counter_sel(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_TC_SELR);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_tc_counter_sel(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	tpdm_writel(drvdata, val, TPDM_TC_SELR);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_counter_sel, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_counter_sel, tpdm_store_tc_counter_sel);

static ssize_t tpdm_show_tc_count_val_lo(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_TC_CNTR_LO);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_tc_count_val_lo(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val, select;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		select = tpdm_readl(drvdata, TPDM_TC_SELR);
		select = (select >> 11) & 0x3;

		/* Check if selected counter is disabled */
		if (BVAL(tpdm_readl(drvdata, TPDM_TC_CNTENSET), select)) {
			mutex_unlock(&drvdata->lock);
			return -EPERM;
		}

		tpdm_writel(drvdata, val, TPDM_TC_CNTR_LO);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_count_val_lo, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_count_val_lo, tpdm_store_tc_count_val_lo);

static ssize_t tpdm_show_tc_count_val_hi(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_TC_CNTR_HI);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_tc_count_val_hi(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val, select;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		select = tpdm_readl(drvdata, TPDM_TC_SELR);
		select = (select >> 11) & 0x3;

		/* Check if selected counter is disabled */
		if (BVAL(tpdm_readl(drvdata, TPDM_TC_CNTENSET), select)) {
			mutex_unlock(&drvdata->lock);
			return -EPERM;
		}

		tpdm_writel(drvdata, val, TPDM_TC_CNTR_HI);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_count_val_hi, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_count_val_hi, tpdm_store_tc_count_val_hi);

static ssize_t tpdm_show_tc_shadow_val_lo(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	for (i = 0; i < TPDM_TC_MAX_COUNTERS; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  tpdm_readl(drvdata, TPDM_TC_SHADOW_LO(i)));
	}
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_shadow_val_lo, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_shadow_val_lo, NULL);

static ssize_t tpdm_show_tc_shadow_val_hi(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	for (i = 0; i < TPDM_TC_MAX_COUNTERS; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  tpdm_readl(drvdata, TPDM_TC_SHADOW_HI(i)));
	}
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_shadow_val_hi, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_shadow_val_hi, NULL);

static ssize_t tpdm_show_tc_sw_inc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	TPDM_UNLOCK(drvdata);
	val = tpdm_readl(drvdata, TPDM_TC_SWINC);
	TPDM_LOCK(drvdata);
	mutex_unlock(&drvdata->lock);
	return scnprintf(buf, PAGE_SIZE, "%lx\n", val);
}

static ssize_t tpdm_store_tc_sw_inc(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_TC, drvdata->enable_ds))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!drvdata->enable) {
		mutex_unlock(&drvdata->lock);
		return -EPERM;
	}

	if (val) {
		TPDM_UNLOCK(drvdata);
		tpdm_writel(drvdata, val, TPDM_TC_SWINC);
		TPDM_LOCK(drvdata);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(tc_sw_inc, S_IRUGO | S_IWUSR,
		   tpdm_show_tc_sw_inc, tpdm_store_tc_sw_inc);

static ssize_t tpdm_show_dsb_mode(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%lx\n",
			 (unsigned long)drvdata->dsb->mode);
}

static ssize_t tpdm_store_dsb_mode(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->dsb->mode = val & TPDM_MODE_ALL;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_mode, tpdm_store_dsb_mode);

static ssize_t tpdm_show_dsb_edge_ctrl(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i;

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_DSB_MAX_EDCR; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index:0x%x Val:0x%x\n", i,
				  drvdata->dsb->edge_ctrl[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_dsb_edge_ctrl(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long start, end, edge_ctrl;
	uint32_t val;
	int i, bit, reg;

	if (sscanf(buf, "%lx %lx %lx", &start, &end, &edge_ctrl) != 3)
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets) ||
	    (start >= TPDM_DSB_MAX_LINES) || (end >= TPDM_DSB_MAX_LINES) ||
	    edge_ctrl > 0x2)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = start; i <= end; i++) {
		reg = i / (NUM_OF_BITS / 2);
		bit = i % (NUM_OF_BITS / 2);
		bit = bit * 2;

		val = drvdata->dsb->edge_ctrl[reg];
		val = val | (edge_ctrl << bit);
		drvdata->dsb->edge_ctrl[reg] = val;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_edge_ctrl, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_edge_ctrl, tpdm_store_dsb_edge_ctrl);

static ssize_t tpdm_show_dsb_edge_ctrl_mask(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i;

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_DSB_MAX_EDCR / 2; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index:0x%x Val:0x%x\n", i,
				  drvdata->dsb->edge_ctrl_mask[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_dsb_edge_ctrl_mask(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf,
					     size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long start, end, val;
	uint32_t set;
	int i, bit, reg;

	if (sscanf(buf, "%lx %lx %lx", &start, &end, &val) != 3)
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets) ||
	    (start >= TPDM_DSB_MAX_LINES) || (end >= TPDM_DSB_MAX_LINES))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = start; i <= end; i++) {
		reg = i / NUM_OF_BITS;
		bit = (i % NUM_OF_BITS);

		set = drvdata->dsb->edge_ctrl_mask[reg];
		if (val)
			set = set | BIT(bit);
		else
			set = set & ~BIT(bit);
		drvdata->dsb->edge_ctrl_mask[reg] = set;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_edge_ctrl_mask, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_edge_ctrl_mask, tpdm_store_dsb_edge_ctrl_mask);

static ssize_t tpdm_show_dsb_patt_val(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_DSB_MAX_PATT; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->dsb->patt_val[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_dsb_patt_val(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets) ||
	    index >= TPDM_DSB_MAX_PATT)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->dsb->patt_val[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_patt_val, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_patt_val, tpdm_store_dsb_patt_val);

static ssize_t tpdm_show_dsb_patt_mask(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_DSB_MAX_PATT; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->dsb->patt_mask[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_dsb_patt_mask(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets) ||
	    index >= TPDM_DSB_MAX_PATT)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->dsb->patt_mask[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_patt_mask, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_patt_mask, tpdm_store_dsb_patt_mask);

static ssize_t tpdm_show_dsb_patt_ts(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->dsb->patt_ts);
}

static ssize_t tpdm_store_dsb_patt_ts(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->dsb->patt_ts = true;
	else
		drvdata->dsb->patt_ts = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_patt_ts, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_patt_ts, tpdm_store_dsb_patt_ts);

static ssize_t tpdm_show_dsb_trig_patt_val(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_DSB_MAX_PATT; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->dsb->trig_patt_val[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_dsb_trig_patt_val(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets) ||
	    index >= TPDM_DSB_MAX_PATT)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->dsb->trig_patt_val[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_trig_patt_val, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_trig_patt_val, tpdm_store_dsb_trig_patt_val);

static ssize_t tpdm_show_dsb_trig_patt_mask(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i = 0;

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_DSB_MAX_PATT; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index: 0x%x Value: 0x%x\n", i,
				  drvdata->dsb->trig_patt_mask[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_dsb_trig_patt_mask(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf,
					     size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long index, val;

	if (sscanf(buf, "%lx %lx", &index, &val) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets) ||
	    index >= TPDM_DSB_MAX_PATT)
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->dsb->trig_patt_mask[index] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_trig_patt_mask, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_trig_patt_mask, tpdm_store_dsb_trig_patt_mask);

static ssize_t tpdm_show_dsb_trig_ts(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->dsb->trig_ts);
}

static ssize_t tpdm_store_dsb_trig_ts(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->dsb->trig_ts = true;
	else
		drvdata->dsb->trig_ts = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_trig_ts, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_trig_ts, tpdm_store_dsb_trig_ts);

static ssize_t tpdm_show_dsb_select_val(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	int i;

	if (!test_bit(TPDM_DS_DSB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = 0; i < TPDM_DSB_MAX_SELECT; i++) {
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  "Index:0x%x Val:0x%x\n", i,
				  drvdata->dsb->select_val[i]);
	}
	mutex_unlock(&drvdata->lock);
	return size;
}

static ssize_t tpdm_store_dsb_select_val(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long start, end;
	uint32_t val;
	int i, bit, reg;

	if (sscanf(buf, "%lx %lx", &start, &end) != 2)
		return -EINVAL;
	if (!test_bit(TPDM_DS_DSB, drvdata->datasets) ||
	    (start >= TPDM_DSB_MAX_LINES) || (end >= TPDM_DSB_MAX_LINES))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	for (i = start; i <= end; i++) {
		reg = i / NUM_OF_BITS;
		bit = (i % NUM_OF_BITS);

		val = drvdata->dsb->select_val[reg];
		val = val | BIT(bit);
		drvdata->dsb->select_val[reg] = val;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(dsb_select_val, S_IRUGO | S_IWUSR,
		   tpdm_show_dsb_select_val, tpdm_store_dsb_select_val);

static ssize_t tpdm_show_cmb_available_modes(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", "continuous trace_on_change");
}
static DEVICE_ATTR(cmb_available_modes, S_IRUGO, tpdm_show_cmb_available_modes,
		   NULL);

static ssize_t tpdm_show_cmb_mode(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 drvdata->cmb->mode == TPDM_CMB_MODE_CONTINUOUS ?
			 "continuous" : "trace_on_change");
}

static ssize_t tpdm_store_cmb_mode(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[20] = "";

	if (strlen(buf) >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (!strcmp(str, "continuous")) {
		drvdata->cmb->mode = TPDM_CMB_MODE_CONTINUOUS;
	} else if (!strcmp(str, "trace_on_change")) {
		drvdata->cmb->mode = TPDM_CMB_MODE_TRACE_ON_CHANGE;
	} else {
		mutex_unlock(&drvdata->lock);
		return -EINVAL;
	}
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_mode, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_mode, tpdm_store_cmb_mode);

static ssize_t tpdm_show_cmb_patt_val_lsb(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->patt_val[TPDM_CMB_LSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_patt_val_lsb(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->patt_val[TPDM_CMB_LSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_val_lsb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_val_lsb,
		   tpdm_store_cmb_patt_val_lsb);

static ssize_t tpdm_show_cmb_patt_mask_lsb(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->patt_mask[TPDM_CMB_LSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_patt_mask_lsb(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->patt_mask[TPDM_CMB_LSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_mask_lsb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_mask_lsb, tpdm_store_cmb_patt_mask_lsb);

static ssize_t tpdm_show_cmb_patt_val_msb(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->patt_val[TPDM_CMB_MSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_patt_val_msb(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->patt_val[TPDM_CMB_MSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_val_msb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_val_msb,
		   tpdm_store_cmb_patt_val_msb);

static ssize_t tpdm_show_cmb_patt_mask_msb(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->patt_mask[TPDM_CMB_MSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_patt_mask_msb(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->patt_mask[TPDM_CMB_MSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_mask_msb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_mask_msb, tpdm_store_cmb_patt_mask_msb);

static ssize_t tpdm_show_cmb_patt_ts(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->cmb->patt_ts);
}

static ssize_t tpdm_store_cmb_patt_ts(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->cmb->patt_ts = true;
	else
		drvdata->cmb->patt_ts = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_patt_ts, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_patt_ts, tpdm_store_cmb_patt_ts);

static ssize_t tpdm_show_cmb_trig_patt_val_lsb(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->trig_patt_val[TPDM_CMB_LSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_trig_patt_val_lsb(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->trig_patt_val[TPDM_CMB_LSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_patt_val_lsb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_patt_val_lsb,
		   tpdm_store_cmb_trig_patt_val_lsb);

static ssize_t tpdm_show_cmb_trig_patt_mask_lsb(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->trig_patt_mask[TPDM_CMB_LSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_trig_patt_mask_lsb(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->trig_patt_mask[TPDM_CMB_LSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_patt_mask_lsb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_patt_mask_lsb,
		   tpdm_store_cmb_trig_patt_mask_lsb);

static ssize_t tpdm_show_cmb_trig_patt_val_msb(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->trig_patt_val[TPDM_CMB_MSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_trig_patt_val_msb(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->trig_patt_val[TPDM_CMB_MSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_patt_val_msb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_patt_val_msb,
		   tpdm_store_cmb_trig_patt_val_msb);

static ssize_t tpdm_show_cmb_trig_patt_mask_msb(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	val = drvdata->cmb->trig_patt_mask[TPDM_CMB_MSB];

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t tpdm_store_cmb_trig_patt_mask_msb(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	drvdata->cmb->trig_patt_mask[TPDM_CMB_MSB] = val;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_patt_mask_msb, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_patt_mask_msb,
		   tpdm_store_cmb_trig_patt_mask_msb);

static ssize_t tpdm_show_cmb_trig_ts(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 (unsigned)drvdata->cmb->trig_ts);
}

static ssize_t tpdm_store_cmb_trig_ts(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t size)
{
	struct tpdm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;
	if (!test_bit(TPDM_DS_CMB, drvdata->datasets))
		return -EPERM;

	mutex_lock(&drvdata->lock);
	if (val)
		drvdata->cmb->trig_ts = true;
	else
		drvdata->cmb->trig_ts = false;
	mutex_unlock(&drvdata->lock);
	return size;
}
static DEVICE_ATTR(cmb_trig_ts, S_IRUGO | S_IWUSR,
		   tpdm_show_cmb_trig_ts, tpdm_store_cmb_trig_ts);

static struct attribute *tpdm_bc_attrs[] = {
	&dev_attr_bc_capture_mode.attr,
	&dev_attr_bc_retrieval_mode.attr,
	&dev_attr_bc_reset_counters.attr,
	&dev_attr_bc_sat_mode.attr,
	&dev_attr_bc_enable_counters.attr,
	&dev_attr_bc_clear_counters.attr,
	&dev_attr_bc_enable_irq.attr,
	&dev_attr_bc_clear_irq.attr,
	&dev_attr_bc_trig_val_lo.attr,
	&dev_attr_bc_trig_val_hi.attr,
	&dev_attr_bc_enable_ganging.attr,
	&dev_attr_bc_overflow_val.attr,
	&dev_attr_bc_ovsr.attr,
	&dev_attr_bc_counter_sel.attr,
	&dev_attr_bc_count_val_lo.attr,
	&dev_attr_bc_count_val_hi.attr,
	&dev_attr_bc_shadow_val_lo.attr,
	&dev_attr_bc_shadow_val_hi.attr,
	&dev_attr_bc_sw_inc.attr,
	NULL,
};

static struct attribute *tpdm_tc_attrs[] = {
	&dev_attr_tc_capture_mode.attr,
	&dev_attr_tc_retrieval_mode.attr,
	&dev_attr_tc_reset_counters.attr,
	&dev_attr_tc_sat_mode.attr,
	&dev_attr_tc_enable_counters.attr,
	&dev_attr_tc_clear_counters.attr,
	&dev_attr_tc_enable_irq.attr,
	&dev_attr_tc_clear_irq.attr,
	&dev_attr_tc_trig_sel.attr,
	&dev_attr_tc_trig_val_lo.attr,
	&dev_attr_tc_trig_val_hi.attr,
	&dev_attr_tc_ovsr_gp.attr,
	&dev_attr_tc_ovsr_impl.attr,
	&dev_attr_tc_counter_sel.attr,
	&dev_attr_tc_count_val_lo.attr,
	&dev_attr_tc_count_val_hi.attr,
	&dev_attr_tc_shadow_val_lo.attr,
	&dev_attr_tc_shadow_val_hi.attr,
	&dev_attr_tc_sw_inc.attr,
	NULL,
};

static struct attribute *tpdm_dsb_attrs[] = {
	&dev_attr_dsb_mode.attr,
	&dev_attr_dsb_edge_ctrl.attr,
	&dev_attr_dsb_edge_ctrl_mask.attr,
	&dev_attr_dsb_patt_val.attr,
	&dev_attr_dsb_patt_mask.attr,
	&dev_attr_dsb_patt_ts.attr,
	&dev_attr_dsb_trig_patt_val.attr,
	&dev_attr_dsb_trig_patt_mask.attr,
	&dev_attr_dsb_trig_ts.attr,
	&dev_attr_dsb_select_val.attr,
	NULL,
};

static struct attribute *tpdm_cmb_attrs[] = {
	&dev_attr_cmb_available_modes.attr,
	&dev_attr_cmb_mode.attr,
	&dev_attr_cmb_patt_val_lsb.attr,
	&dev_attr_cmb_patt_mask_lsb.attr,
	&dev_attr_cmb_patt_val_msb.attr,
	&dev_attr_cmb_patt_mask_msb.attr,
	&dev_attr_cmb_patt_ts.attr,
	&dev_attr_cmb_trig_patt_val_lsb.attr,
	&dev_attr_cmb_trig_patt_mask_lsb.attr,
	&dev_attr_cmb_trig_patt_val_msb.attr,
	&dev_attr_cmb_trig_patt_mask_msb.attr,
	&dev_attr_cmb_trig_ts.attr,
	NULL,
};

static struct attribute_group tpdm_bc_attr_grp = {
	.attrs = tpdm_bc_attrs,
};

static struct attribute_group tpdm_tc_attr_grp = {
	.attrs = tpdm_tc_attrs,
};

static struct attribute_group tpdm_dsb_attr_grp = {
	.attrs = tpdm_dsb_attrs,
};

static struct attribute_group tpdm_cmb_attr_grp = {
	.attrs = tpdm_cmb_attrs,
};

static struct attribute *tpdm_attrs[] = {
	&dev_attr_available_datasets.attr,
	&dev_attr_enable_datasets.attr,
	&dev_attr_gp_regs.attr,
	NULL,
};

static struct attribute_group tpdm_attr_grp = {
	.attrs = tpdm_attrs,
};
static const struct attribute_group *tpdm_attr_grps[] = {
	&tpdm_attr_grp,
	&tpdm_bc_attr_grp,
	&tpdm_tc_attr_grp,
	&tpdm_dsb_attr_grp,
	&tpdm_cmb_attr_grp,
	NULL,
};

static int tpdm_datasets_alloc(struct tpdm_drvdata *drvdata)
{
	if (test_bit(TPDM_DS_GPR, drvdata->datasets)) {
		drvdata->gpr = devm_kzalloc(drvdata->dev, sizeof(*drvdata->gpr),
					    GFP_KERNEL);
		if (!drvdata->gpr)
			return -ENOMEM;
	}
	if (test_bit(TPDM_DS_BC, drvdata->datasets)) {
		drvdata->bc = devm_kzalloc(drvdata->dev, sizeof(*drvdata->bc),
					   GFP_KERNEL);
		if (!drvdata->bc)
			return -ENOMEM;
	}
	if (test_bit(TPDM_DS_TC, drvdata->datasets)) {
		drvdata->tc = devm_kzalloc(drvdata->dev, sizeof(*drvdata->tc),
					   GFP_KERNEL);
		if (!drvdata->tc)
			return -ENOMEM;
	}
	if (test_bit(TPDM_DS_DSB, drvdata->datasets)) {
		drvdata->dsb = devm_kzalloc(drvdata->dev, sizeof(*drvdata->dsb),
					    GFP_KERNEL);
		if (!drvdata->dsb)
			return -ENOMEM;
	}
	if (test_bit(TPDM_DS_CMB, drvdata->datasets)) {
		drvdata->cmb = devm_kzalloc(drvdata->dev, sizeof(*drvdata->cmb),
					    GFP_KERNEL);
		if (!drvdata->cmb)
			return -ENOMEM;
	}
	return 0;
}

static void tpdm_init_default_data(struct tpdm_drvdata *drvdata)
{
	if (test_bit(TPDM_DS_BC, drvdata->datasets))
		drvdata->bc->retrieval_mode = TPDM_MODE_ATB;

	if (test_bit(TPDM_DS_TC, drvdata->datasets))
		drvdata->tc->retrieval_mode = TPDM_MODE_ATB;

	if (test_bit(TPDM_DS_DSB, drvdata->datasets))
		drvdata->dsb->trig_ts = true;

	if (test_bit(TPDM_DS_CMB, drvdata->datasets))
		drvdata->cmb->trig_ts = true;
}

static int tpdm_probe(struct platform_device *pdev)
{
	int ret, i;
	uint32_t pidr, devid;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct tpdm_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;

	if (coresight_fuse_access_disabled())
		return -EPERM;

	pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tpdm-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	drvdata->clk_enable = of_property_read_bool(pdev->dev.of_node,
						    "qcom,clk-enable");

	mutex_init(&drvdata->lock);

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	pidr = tpdm_readl(drvdata, CORESIGHT_PERIPHIDR0);
	for (i = 0; i < TPDM_DATASETS; i++) {
		if (pidr & BIT(i)) {
			__set_bit(i, drvdata->datasets);
			__set_bit(i, drvdata->enable_ds);
		}
	}

	ret = tpdm_datasets_alloc(drvdata);
	if (ret)
		return ret;

	tpdm_init_default_data(drvdata);

	devid = tpdm_readl(drvdata, CORESIGHT_DEVID);
	drvdata->tc_trig_type = BMVAL(devid, 27, 28);
	drvdata->bc_trig_type = BMVAL(devid, 25, 26);
	drvdata->bc_gang_type = BMVAL(devid, 23, 24);
	drvdata->bc_counters_avail = BMVAL(devid, 6, 10) + 1;
	drvdata->tc_counters_avail = BMVAL(devid, 4, 5) + 1;

	clk_disable_unprepare(drvdata->clk);

	ret = tpdm_datasets_alloc(drvdata);
	if (ret)
		return ret;

	tpdm_init_default_data(drvdata);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_PROC;
	desc->ops = &tpdm_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = tpdm_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_dbg(drvdata->dev, "TPDM initialized\n");

	if (boot_enable)
		coresight_enable(drvdata->csdev);

	return 0;
}

static int tpdm_remove(struct platform_device *pdev)
{
	struct tpdm_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id tpdm_match[] = {
	{.compatible = "qcom,coresight-tpdm"},
	{}
};

static struct platform_driver tpdm_driver = {
	.probe          = tpdm_probe,
	.remove         = tpdm_remove,
	.driver         = {
		.name   = "coresight-tpdm",
		.owner	= THIS_MODULE,
		.of_match_table = tpdm_match,
	},
};

static int __init tpdm_init(void)
{
	return platform_driver_register(&tpdm_driver);
}
module_init(tpdm_init);

static void __exit tpdm_exit(void)
{
	platform_driver_unregister(&tpdm_driver);
}
module_exit(tpdm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Trace, Profiling & Diagnostic Monitor driver");

/*
 * linux/arch/arm/mach-omap2/irq.c
 *
 * Interrupt handler for OMAP2 boards.
 *
 * Copyright (C) 2005 Nokia Corporation
 * Author: Paul Mundt <paul.mundt@nokia.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <asm/exception.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "irqchip.h"

/* Define these here for now until we drop all board-files */
#define OMAP24XX_IC_BASE	0x480fe000
#define OMAP34XX_IC_BASE	0x48200000

/* selected INTC register offsets */

#define INTC_REVISION		0x0000
#define INTC_SYSCONFIG		0x0010
#define INTC_SYSSTATUS		0x0014
#define INTC_SIR		0x0040
#define INTC_CONTROL		0x0048
#define INTC_PROTECTION		0x004C
#define INTC_IDLE		0x0050
#define INTC_THRESHOLD		0x0068
#define INTC_MIR0		0x0084
#define INTC_MIR_CLEAR0		0x0088
#define INTC_MIR_SET0		0x008c
#define INTC_PENDING_IRQ0	0x0098
#define INTC_PENDING_IRQ1	0x00b8
#define INTC_PENDING_IRQ2	0x00d8
#define INTC_PENDING_IRQ3	0x00f8
#define INTC_ILR0		0x0100

#define ACTIVEIRQ_MASK		0x7f	/* omap2/3 active interrupt bits */
#define INTCPS_NR_ILR_REGS	128
#define INTCPS_NR_MIR_REGS	3

/*
 * OMAP2 has a number of different interrupt controllers, each interrupt
 * controller is identified as its own "bank". Register definitions are
 * fairly consistent for each bank, but not all registers are implemented
 * for each bank.. when in doubt, consult the TRM.
 */

/* Structure to save interrupt controller context */
struct omap_intc_regs {
	u32 sysconfig;
	u32 protection;
	u32 idle;
	u32 threshold;
	u32 ilr[INTCPS_NR_ILR_REGS];
	u32 mir[INTCPS_NR_MIR_REGS];
};
static struct omap_intc_regs intc_context;

static struct irq_domain *domain;
static void __iomem *omap_irq_base;
static int omap_nr_pending = 3;
static int omap_nr_irqs = 96;

/* INTC bank register get/set */
static void intc_writel(u32 reg, u32 val)
{
	writel_relaxed(val, omap_irq_base + reg);
}

static u32 intc_readl(u32 reg)
{
	return readl_relaxed(omap_irq_base + reg);
}

void omap_intc_save_context(void)
{
	int i;

	intc_context.sysconfig =
		intc_readl(INTC_SYSCONFIG);
	intc_context.protection =
		intc_readl(INTC_PROTECTION);
	intc_context.idle =
		intc_readl(INTC_IDLE);
	intc_context.threshold =
		intc_readl(INTC_THRESHOLD);

	for (i = 0; i < omap_nr_irqs; i++)
		intc_context.ilr[i] =
			intc_readl((INTC_ILR0 + 0x4 * i));
	for (i = 0; i < INTCPS_NR_MIR_REGS; i++)
		intc_context.mir[i] =
			intc_readl(INTC_MIR0 + (0x20 * i));
}

void omap_intc_restore_context(void)
{
	int i;

	intc_writel(INTC_SYSCONFIG, intc_context.sysconfig);
	intc_writel(INTC_PROTECTION, intc_context.protection);
	intc_writel(INTC_IDLE, intc_context.idle);
	intc_writel(INTC_THRESHOLD, intc_context.threshold);

	for (i = 0; i < omap_nr_irqs; i++)
		intc_writel(INTC_ILR0 + 0x4 * i,
				intc_context.ilr[i]);

	for (i = 0; i < INTCPS_NR_MIR_REGS; i++)
		intc_writel(INTC_MIR0 + 0x20 * i,
			intc_context.mir[i]);
	/* MIRs are saved and restore with other PRCM registers */
}

void omap3_intc_prepare_idle(void)
{
	/*
	 * Disable autoidle as it can stall interrupt controller,
	 * cf. errata ID i540 for 3430 (all revisions up to 3.1.x)
	 */
	intc_writel(INTC_SYSCONFIG, 0);
}

void omap3_intc_resume_idle(void)
{
	/* Re-enable autoidle */
	intc_writel(INTC_SYSCONFIG, 1);
}

/* XXX: FIQ and additional INTC support (only MPU at the moment) */
static void omap_ack_irq(struct irq_data *d)
{
	intc_writel(INTC_CONTROL, 0x1);
}

static void omap_mask_ack_irq(struct irq_data *d)
{
	irq_gc_mask_disable_reg(d);
	omap_ack_irq(d);
}

static void __init omap_irq_soft_reset(void)
{
	unsigned long tmp;

	tmp = intc_readl(INTC_REVISION) & 0xff;

	pr_info("IRQ: Found an INTC at 0x%p (revision %ld.%ld) with %d interrupts\n",
		omap_irq_base, tmp >> 4, tmp & 0xf, omap_nr_irqs);

	tmp = intc_readl(INTC_SYSCONFIG);
	tmp |= 1 << 1;	/* soft reset */
	intc_writel(INTC_SYSCONFIG, tmp);

	while (!(intc_readl(INTC_SYSSTATUS) & 0x1))
		/* Wait for reset to complete */;

	/* Enable autoidle */
	intc_writel(INTC_SYSCONFIG, 1 << 0);
}

int omap_irq_pending(void)
{
	int i;

	for (i = 0; i < omap_nr_pending; i++)
		if (intc_readl(INTC_PENDING_IRQ0 + (0x20 * i)))
			return 1;
	return 0;
}

void omap3_intc_suspend(void)
{
	/* A pending interrupt would prevent OMAP from entering suspend */
	omap_ack_irq(NULL);
}

static int __init omap_alloc_gc_of(struct irq_domain *d, void __iomem *base)
{
	int ret;
	int i;

	ret = irq_alloc_domain_generic_chips(d, 32, 1, "INTC",
			handle_level_irq, IRQ_NOREQUEST | IRQ_NOPROBE,
			IRQ_LEVEL, 0);
	if (ret) {
		pr_warn("Failed to allocate irq chips\n");
		return ret;
	}

	for (i = 0; i < omap_nr_pending; i++) {
		struct irq_chip_generic *gc;
		struct irq_chip_type *ct;

		gc = irq_get_domain_generic_chip(d, 32 * i);
		gc->reg_base = base;
		ct = gc->chip_types;

		ct->type = IRQ_TYPE_LEVEL_MASK;
		ct->handler = handle_level_irq;

		ct->chip.irq_ack = omap_mask_ack_irq;
		ct->chip.irq_mask = irq_gc_mask_disable_reg;
		ct->chip.irq_unmask = irq_gc_unmask_enable_reg;

		ct->chip.flags |= IRQCHIP_SKIP_SET_WAKE;

		ct->regs.enable = INTC_MIR_CLEAR0 + 32 * i;
		ct->regs.disable = INTC_MIR_SET0 + 32 * i;
	}

	return 0;
}

static void __init omap_alloc_gc_legacy(void __iomem *base,
		unsigned int irq_start, unsigned int num)
{
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;

	gc = irq_alloc_generic_chip("INTC", 1, irq_start, base,
			handle_level_irq);
	ct = gc->chip_types;
	ct->chip.irq_ack = omap_mask_ack_irq;
	ct->chip.irq_mask = irq_gc_mask_disable_reg;
	ct->chip.irq_unmask = irq_gc_unmask_enable_reg;
	ct->chip.flags |= IRQCHIP_SKIP_SET_WAKE;

	ct->regs.enable = INTC_MIR_CLEAR0;
	ct->regs.disable = INTC_MIR_SET0;
	irq_setup_generic_chip(gc, IRQ_MSK(num), IRQ_GC_INIT_MASK_CACHE,
			IRQ_NOREQUEST | IRQ_NOPROBE, 0);
}

static int __init omap_init_irq_of(struct device_node *node)
{
	int ret;

	omap_irq_base = of_iomap(node, 0);
	if (WARN_ON(!omap_irq_base))
		return -ENOMEM;

	domain = irq_domain_add_linear(node, omap_nr_irqs,
			&irq_generic_chip_ops, NULL);

	omap_irq_soft_reset();

	ret = omap_alloc_gc_of(domain, omap_irq_base);
	if (ret < 0)
		irq_domain_remove(domain);

	return ret;
}

static int __init omap_init_irq_legacy(u32 base)
{
	int j, irq_base;

	omap_irq_base = ioremap(base, SZ_4K);
	if (WARN_ON(!omap_irq_base))
		return -ENOMEM;

	irq_base = irq_alloc_descs(-1, 0, omap_nr_irqs, 0);
	if (irq_base < 0) {
		pr_warn("Couldn't allocate IRQ numbers\n");
		irq_base = 0;
	}

	domain = irq_domain_add_legacy(NULL, omap_nr_irqs, irq_base, 0,
			&irq_domain_simple_ops, NULL);

	omap_irq_soft_reset();

	for (j = 0; j < omap_nr_irqs; j += 32)
		omap_alloc_gc_legacy(omap_irq_base + j, j + irq_base, 32);

	return 0;
}

static int __init omap_init_irq(u32 base, struct device_node *node)
{
	if (node)
		return omap_init_irq_of(node);
	else
		return omap_init_irq_legacy(base);
}

static asmlinkage void __exception_irq_entry
omap_intc_handle_irq(struct pt_regs *regs)
{
	u32 irqnr = 0;
	int handled_irq = 0;
	int i;

	do {
		for (i = 0; i < omap_nr_pending; i++) {
			irqnr = intc_readl(INTC_PENDING_IRQ0 + (0x20 * i));
			if (irqnr)
				goto out;
		}

out:
		if (!irqnr)
			break;

		irqnr = intc_readl(INTC_SIR);
		irqnr &= ACTIVEIRQ_MASK;

		if (irqnr) {
			irqnr = irq_find_mapping(domain, irqnr);
			handle_IRQ(irqnr, regs);
			handled_irq = 1;
		}
	} while (irqnr);

	/* If an irq is masked or deasserted while active, we will
	 * keep ending up here with no irq handled. So remove it from
	 * the INTC with an ack.*/
	if (!handled_irq)
		omap_ack_irq(NULL);
}

void __init omap2_init_irq(void)
{
	omap_nr_irqs = 96;
	omap_nr_pending = 3;
	omap_init_irq(OMAP24XX_IC_BASE, NULL);
	set_handle_irq(omap_intc_handle_irq);
}

void __init omap3_init_irq(void)
{
	omap_nr_irqs = 96;
	omap_nr_pending = 3;
	omap_init_irq(OMAP34XX_IC_BASE, NULL);
	set_handle_irq(omap_intc_handle_irq);
}

void __init ti81xx_init_irq(void)
{
	omap_nr_irqs = 96;
	omap_nr_pending = 4;
	omap_init_irq(OMAP34XX_IC_BASE, NULL);
	set_handle_irq(omap_intc_handle_irq);
}

static int __init intc_of_init(struct device_node *node,
			     struct device_node *parent)
{
	struct resource res;
	int ret;

	omap_nr_pending = 3;
	omap_nr_irqs = 96;

	if (WARN_ON(!node))
		return -ENODEV;

	if (of_address_to_resource(node, 0, &res)) {
		WARN(1, "unable to get intc registers\n");
		return -EINVAL;
	}

	if (of_device_is_compatible(node, "ti,am33xx-intc")) {
		omap_nr_irqs = 128;
		omap_nr_pending = 4;
	}

	ret = omap_init_irq(-1, of_node_get(node));
	if (ret < 0)
		return ret;

	set_handle_irq(omap_intc_handle_irq);

	return 0;
}

IRQCHIP_DECLARE(omap2_intc, "ti,omap2-intc", intc_of_init);
IRQCHIP_DECLARE(omap3_intc, "ti,omap3-intc", intc_of_init);
IRQCHIP_DECLARE(am33xx_intc, "ti,am33xx-intc", intc_of_init);

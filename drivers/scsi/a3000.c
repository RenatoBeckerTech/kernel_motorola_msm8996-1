#include <linux/types.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/amigaints.h>
#include <asm/amigahw.h>

#include "scsi.h"
#include "wd33c93.h"
#include "a3000.h"


static irqreturn_t a3000_intr(int irq, void *data)
{
	struct Scsi_Host *instance = data;
	struct a3000_scsiregs *regs = (struct a3000_scsiregs *)(instance->base);
	unsigned int status = regs->ISTR;
	unsigned long flags;

	if (!(status & ISTR_INT_P))
		return IRQ_NONE;
	if (status & ISTR_INTS) {
		spin_lock_irqsave(instance->host_lock, flags);
		wd33c93_intr(instance);
		spin_unlock_irqrestore(instance->host_lock, flags);
		return IRQ_HANDLED;
	}
	pr_warning("Non-serviced A3000 SCSI-interrupt? ISTR = %02x\n", status);
	return IRQ_NONE;
}

static int dma_setup(struct scsi_cmnd *cmd, int dir_in)
{
	struct Scsi_Host *instance = cmd->device->host;
	struct WD33C93_hostdata *hdata = shost_priv(instance);
	struct a3000_scsiregs *regs = (struct a3000_scsiregs *)(instance->base);
	unsigned short cntr = CNTR_PDMD | CNTR_INTEN;
	unsigned long addr = virt_to_bus(cmd->SCp.ptr);

	/*
	 * if the physical address has the wrong alignment, or if
	 * physical address is bad, or if it is a write and at the
	 * end of a physical memory chunk, then allocate a bounce
	 * buffer
	 */
	if (addr & A3000_XFER_MASK) {
		hdata->dma_bounce_len = (cmd->SCp.this_residual + 511) & ~0x1ff;
		hdata->dma_bounce_buffer = kmalloc(hdata->dma_bounce_len,
						   GFP_KERNEL);

		/* can't allocate memory; use PIO */
		if (!hdata->dma_bounce_buffer) {
			hdata->dma_bounce_len = 0;
			return 1;
		}

		if (!dir_in) {
			/* copy to bounce buffer for a write */
			memcpy(hdata->dma_bounce_buffer, cmd->SCp.ptr,
			       cmd->SCp.this_residual);
		}

		addr = virt_to_bus(hdata->dma_bounce_buffer);
	}

	/* setup dma direction */
	if (!dir_in)
		cntr |= CNTR_DDIR;

	/* remember direction */
	hdata->dma_dir = dir_in;

	regs->CNTR = cntr;

	/* setup DMA *physical* address */
	regs->ACR = addr;

	if (dir_in) {
		/* invalidate any cache */
		cache_clear(addr, cmd->SCp.this_residual);
	} else {
		/* push any dirty cache */
		cache_push(addr, cmd->SCp.this_residual);
	}

	/* start DMA */
	mb();			/* make sure setup is completed */
	regs->ST_DMA = 1;
	mb();			/* make sure DMA has started before next IO */

	/* return success */
	return 0;
}

static void dma_stop(struct Scsi_Host *instance, struct scsi_cmnd *SCpnt,
		     int status)
{
	struct WD33C93_hostdata *hdata = shost_priv(instance);
	struct a3000_scsiregs *regs = (struct a3000_scsiregs *)(instance->base);

	/* disable SCSI interrupts */
	unsigned short cntr = CNTR_PDMD;

	if (!hdata->dma_dir)
		cntr |= CNTR_DDIR;

	regs->CNTR = cntr;
	mb();			/* make sure CNTR is updated before next IO */

	/* flush if we were reading */
	if (hdata->dma_dir) {
		regs->FLUSH = 1;
		mb();		/* don't allow prefetch */
		while (!(regs->ISTR & ISTR_FE_FLG))
			barrier();
		mb();		/* no IO until FLUSH is done */
	}

	/* clear a possible interrupt */
	/* I think that this CINT is only necessary if you are
	 * using the terminal count features.   HM 7 Mar 1994
	 */
	regs->CINT = 1;

	/* stop DMA */
	regs->SP_DMA = 1;
	mb();			/* make sure DMA is stopped before next IO */

	/* restore the CONTROL bits (minus the direction flag) */
	regs->CNTR = CNTR_PDMD | CNTR_INTEN;
	mb();			/* make sure CNTR is updated before next IO */

	/* copy from a bounce buffer, if necessary */
	if (status && hdata->dma_bounce_buffer) {
		if (SCpnt) {
			if (hdata->dma_dir && SCpnt)
				memcpy(SCpnt->SCp.ptr,
				       hdata->dma_bounce_buffer,
				       SCpnt->SCp.this_residual);
			kfree(hdata->dma_bounce_buffer);
			hdata->dma_bounce_buffer = NULL;
			hdata->dma_bounce_len = 0;
		} else {
			kfree(hdata->dma_bounce_buffer);
			hdata->dma_bounce_buffer = NULL;
			hdata->dma_bounce_len = 0;
		}
	}
}

static int a3000_bus_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *instance = cmd->device->host;

	/* FIXME perform bus-specific reset */

	/* FIXME 2: kill this entire function, which should
	   cause mid-layer to call wd33c93_host_reset anyway? */

	spin_lock_irq(instance->host_lock);
	wd33c93_host_reset(cmd);
	spin_unlock_irq(instance->host_lock);

	return SUCCESS;
}

static struct scsi_host_template amiga_a3000_scsi_template = {
	.module			= THIS_MODULE,
	.name			= "Amiga 3000 built-in SCSI",
	.proc_info		= wd33c93_proc_info,
	.proc_name		= "A3000",
	.queuecommand		= wd33c93_queuecommand,
	.eh_abort_handler	= wd33c93_abort,
	.eh_bus_reset_handler	= a3000_bus_reset,
	.eh_host_reset_handler	= wd33c93_host_reset,
	.can_queue		= CAN_QUEUE,
	.this_id		= 7,
	.sg_tablesize		= SG_ALL,
	.cmd_per_lun		= CMD_PER_LUN,
	.use_clustering		= ENABLE_CLUSTERING
};

static int __init amiga_a3000_scsi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct Scsi_Host *instance;
	int error;
	struct a3000_scsiregs *regs;
	wd33c93_regs wdregs;
	struct WD33C93_hostdata *hdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	if (!request_mem_region(res->start, resource_size(res), "wd33c93"))
		return -EBUSY;

	instance = scsi_host_alloc(&amiga_a3000_scsi_template,
				   sizeof(struct WD33C93_hostdata));
	if (!instance) {
		error = -ENOMEM;
		goto fail_alloc;
	}

	instance->base = ZTWO_VADDR(res->start);
	instance->irq = IRQ_AMIGA_PORTS;

	regs = (struct a3000_scsiregs *)(instance->base);
	regs->DAWR = DAWR_A3000;

	wdregs.SASR = &regs->SASR;
	wdregs.SCMD = &regs->SCMD;

	hdata = shost_priv(instance);
	hdata->no_sync = 0xff;
	hdata->fast = 0;
	hdata->dma_mode = CTRL_DMA;

	wd33c93_init(instance, wdregs, dma_setup, dma_stop, WD33C93_FS_12_15);
	error = request_irq(IRQ_AMIGA_PORTS, a3000_intr, IRQF_SHARED,
			    "A3000 SCSI", instance);
	if (error)
		goto fail_irq;

	regs->CNTR = CNTR_PDMD | CNTR_INTEN;

	error = scsi_add_host(instance, NULL);
	if (error)
		goto fail_host;

	platform_set_drvdata(pdev, instance);

	scsi_scan_host(instance);
	return 0;

fail_host:
	free_irq(IRQ_AMIGA_PORTS, instance);
fail_irq:
	scsi_host_put(instance);
fail_alloc:
	release_mem_region(res->start, resource_size(res));
	return error;
}

static int __exit amiga_a3000_scsi_remove(struct platform_device *pdev)
{
	struct Scsi_Host *instance = platform_get_drvdata(pdev);
	struct a3000_scsiregs *regs = (struct a3000_scsiregs *)(instance->base);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	regs->CNTR = 0;
	scsi_remove_host(instance);
	free_irq(IRQ_AMIGA_PORTS, instance);
	scsi_host_put(instance);
	release_mem_region(res->start, resource_size(res));
	return 0;
}

static struct platform_driver amiga_a3000_scsi_driver = {
	.remove = __exit_p(amiga_a3000_scsi_remove),
	.driver   = {
		.name	= "amiga-a3000-scsi",
		.owner	= THIS_MODULE,
	},
};

static int __init amiga_a3000_scsi_init(void)
{
	return platform_driver_probe(&amiga_a3000_scsi_driver,
				     amiga_a3000_scsi_probe);
}
module_init(amiga_a3000_scsi_init);

static void __exit amiga_a3000_scsi_exit(void)
{
	platform_driver_unregister(&amiga_a3000_scsi_driver);
}
module_exit(amiga_a3000_scsi_exit);

MODULE_DESCRIPTION("Amiga 3000 built-in SCSI");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:amiga-a3000-scsi");

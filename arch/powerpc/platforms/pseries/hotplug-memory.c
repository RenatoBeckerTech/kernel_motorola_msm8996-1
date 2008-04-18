/*
 * pseries Memory Hotplug infrastructure.
 *
 * Copyright (C) 2008 Badari Pulavarty, IBM Corporation
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/of.h>
#include <asm/firmware.h>
#include <asm/machdep.h>
#include <asm/pSeries_reconfig.h>

static int pseries_remove_memory(struct device_node *np)
{
	const char *type;
	const unsigned int *my_index;
	const unsigned int *regs;
	u64 start_pfn, start;
	struct zone *zone;
	int ret = -EINVAL;

	/*
	 * Check to see if we are actually removing memory
	 */
	type = of_get_property(np, "device_type", NULL);
	if (type == NULL || strcmp(type, "memory") != 0)
		return 0;

	/*
	 * Find the memory index and size of the removing section
	 */
	my_index = of_get_property(np, "ibm,my-drc-index", NULL);
	if (!my_index)
		return ret;

	regs = of_get_property(np, "reg", NULL);
	if (!regs)
		return ret;

	start_pfn = section_nr_to_pfn(*my_index & 0xffff);
	zone = page_zone(pfn_to_page(start_pfn));

	/*
	 * Remove section mappings and sysfs entries for the
	 * section of the memory we are removing.
	 *
	 * NOTE: Ideally, this should be done in generic code like
	 * remove_memory(). But remove_memory() gets called by writing
	 * to sysfs "state" file and we can't remove sysfs entries
	 * while writing to it. So we have to defer it to here.
	 */
	ret = __remove_pages(zone, start_pfn, regs[3] >> PAGE_SHIFT);
	if (ret)
		return ret;

	/*
	 * Remove htab bolted mappings for this section of memory
	 */
	start = (unsigned long)__va(start_pfn << PAGE_SHIFT);
	ret = remove_section_mapping(start, start + regs[3]);
	return ret;
}

static int pseries_memory_notifier(struct notifier_block *nb,
				unsigned long action, void *node)
{
	int err = NOTIFY_OK;

	switch (action) {
	case PSERIES_RECONFIG_ADD:
		break;
	case PSERIES_RECONFIG_REMOVE:
		if (pseries_remove_memory(node))
			err = NOTIFY_BAD;
		break;
	default:
		err = NOTIFY_DONE;
		break;
	}
	return err;
}

static struct notifier_block pseries_mem_nb = {
	.notifier_call = pseries_memory_notifier,
};

static int __init pseries_memory_hotplug_init(void)
{
	if (firmware_has_feature(FW_FEATURE_LPAR))
		pSeries_reconfig_notifier_register(&pseries_mem_nb);

	return 0;
}
machine_device_initcall(pseries, pseries_memory_hotplug_init);

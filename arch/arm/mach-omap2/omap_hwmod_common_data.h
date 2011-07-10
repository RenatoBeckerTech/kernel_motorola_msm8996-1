/*
 * omap_hwmod_common_data.h - OMAP hwmod common macros and declarations
 *
 * Copyright (C) 2010-2011 Nokia Corporation
 * Paul Walmsley
 *
 * Copyright (C) 2010-2011 Texas Instruments, Inc.
 * Benoît Cousson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ARCH_ARM_MACH_OMAP2_OMAP_HWMOD_COMMON_DATA_H
#define __ARCH_ARM_MACH_OMAP2_OMAP_HWMOD_COMMON_DATA_H

#include <plat/omap_hwmod.h>

/* Common address space across OMAP2xxx */
extern struct omap_hwmod_addr_space omap2xxx_uart1_addr_space[];
extern struct omap_hwmod_addr_space omap2xxx_uart2_addr_space[];
extern struct omap_hwmod_addr_space omap2xxx_uart3_addr_space[];
extern struct omap_hwmod_addr_space omap2xxx_timer2_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_timer3_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_timer4_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_timer5_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_timer6_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_timer7_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_timer8_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_timer9_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_timer12_addrs[];
extern struct omap_hwmod_addr_space omap2xxx_mcbsp2_addrs[];

/* Common address space across OMAP2xxx/3xxx */
extern struct omap_hwmod_addr_space omap2_i2c1_addr_space[];
extern struct omap_hwmod_addr_space omap2_i2c2_addr_space[];
extern struct omap_hwmod_addr_space omap2_dss_addrs[];
extern struct omap_hwmod_addr_space omap2_dss_dispc_addrs[];
extern struct omap_hwmod_addr_space omap2_dss_rfbi_addrs[];
extern struct omap_hwmod_addr_space omap2_dss_venc_addrs[];
extern struct omap_hwmod_addr_space omap2_timer10_addrs[];
extern struct omap_hwmod_addr_space omap2_timer11_addrs[];
extern struct omap_hwmod_addr_space omap2430_mmc1_addr_space[];
extern struct omap_hwmod_addr_space omap2430_mmc2_addr_space[];
extern struct omap_hwmod_addr_space omap2_mcspi1_addr_space[];
extern struct omap_hwmod_addr_space omap2_mcspi2_addr_space[];
extern struct omap_hwmod_addr_space omap2430_mcspi3_addr_space[];
extern struct omap_hwmod_addr_space omap2_dma_system_addrs[];
extern struct omap_hwmod_addr_space omap2_mailbox_addrs[];
extern struct omap_hwmod_addr_space omap2_mcbsp1_addrs[];

/* OMAP hwmod classes - forward declarations */
extern struct omap_hwmod_class l3_hwmod_class;
extern struct omap_hwmod_class l4_hwmod_class;
extern struct omap_hwmod_class mpu_hwmod_class;
extern struct omap_hwmod_class iva_hwmod_class;


#endif

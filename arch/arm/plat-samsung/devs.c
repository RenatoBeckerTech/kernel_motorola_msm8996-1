/* linux/arch/arm/plat-samsung/devs.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Base SAMSUNG platform device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/gfp.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/onenand.h>
#include <linux/mtd/partitions.h>
#include <linux/mmc/host.h>
#include <linux/ioport.h>

#include <asm/irq.h>
#include <asm/pmu.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <mach/map.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/adc.h>
#include <plat/ata.h>
#include <plat/ehci.h>
#include <plat/fb.h>
#include <plat/fb-s3c2410.h>
#include <plat/hwmon.h>
#include <plat/iic.h>
#include <plat/keypad.h>
#include <plat/mci.h>
#include <plat/nand.h>
#include <plat/sdhci.h>
#include <plat/ts.h>
#include <plat/udc.h>
#include <plat/usb-control.h>
#include <plat/usb-phy.h>
#include <plat/regs-iic.h>
#include <plat/regs-serial.h>
#include <plat/regs-spi.h>

static u64 samsung_device_dma_mask = DMA_BIT_MASK(32);

/* AC97 */
#ifdef CONFIG_CPU_S3C2440
static struct resource s3c_ac97_resource[] = {
	[0] = {
		.start	= S3C2440_PA_AC97,
		.end	= S3C2440_PA_AC97 + S3C2440_SZ_AC97 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S3C244x_AC97,
		.end	= IRQ_S3C244x_AC97,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.name	= "PCM out",
		.start	= DMACH_PCM_OUT,
		.end	= DMACH_PCM_OUT,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.name	= "PCM in",
		.start	= DMACH_PCM_IN,
		.end	= DMACH_PCM_IN,
		.flags	= IORESOURCE_DMA,
	},
	[4] = {
		.name	= "Mic in",
		.start	= DMACH_MIC_IN,
		.end	= DMACH_MIC_IN,
		.flags	= IORESOURCE_DMA,
	},
};

struct platform_device s3c_device_ac97 = {
	.name		= "samsung-ac97",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_ac97_resource),
	.resource	= s3c_ac97_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif /* CONFIG_CPU_S3C2440 */

/* ADC */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_adc_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_ADC,
		.end	= S3C24XX_PA_ADC + S3C24XX_SZ_ADC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_TC,
		.end	= IRQ_TC,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_ADC,
		.end	= IRQ_ADC,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_adc = {
	.name		= "s3c24xx-adc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_adc_resource),
	.resource	= s3c_adc_resource,
};
#endif /* CONFIG_PLAT_S3C24XX */

#if defined(CONFIG_SAMSUNG_DEV_ADC)
static struct resource s3c_adc_resource[] = {
	[0] = {
		.start	= SAMSUNG_PA_ADC,
		.end	= SAMSUNG_PA_ADC + SZ_256 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_TC,
		.end	= IRQ_TC,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_ADC,
		.end	= IRQ_ADC,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_adc = {
	.name		= "samsung-adc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_adc_resource),
	.resource	= s3c_adc_resource,
};
#endif /* CONFIG_SAMSUNG_DEV_ADC */

/* Camif Controller */

#ifdef CONFIG_CPU_S3C2440
static struct resource s3c_camif_resource[] = {
	[0] = {
		.start	= S3C2440_PA_CAMIF,
		.end	= S3C2440_PA_CAMIF + S3C2440_SZ_CAMIF - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_CAM,
		.end	= IRQ_CAM,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_camif = {
	.name		= "s3c2440-camif",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_camif_resource),
	.resource	= s3c_camif_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif /* CONFIG_CPU_S3C2440 */

/* ASOC DMA */

struct platform_device samsung_asoc_dma = {
	.name		= "samsung-audio",
	.id		= -1,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

struct platform_device samsung_asoc_idma = {
	.name		= "samsung-idma",
	.id		= -1,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

/* FB */

#ifdef CONFIG_S3C_DEV_FB
static struct resource s3c_fb_resource[] = {
	[0] = {
		.start	= S3C_PA_FB,
		.end	= S3C_PA_FB + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_LCD_VSYNC,
		.end	= IRQ_LCD_VSYNC,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_LCD_FIFO,
		.end	= IRQ_LCD_FIFO,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= IRQ_LCD_SYSTEM,
		.end	= IRQ_LCD_SYSTEM,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_fb = {
	.name		= "s3c-fb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_fb_resource),
	.resource	= s3c_fb_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

void __init s3c_fb_set_platdata(struct s3c_fb_platdata *pd)
{
	s3c_set_platdata(pd, sizeof(struct s3c_fb_platdata),
			 &s3c_device_fb);
}
#endif /* CONFIG_S3C_DEV_FB */

/* FIMC */

#ifdef CONFIG_S5P_DEV_FIMC0
static struct resource s5p_fimc0_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC0,
		.end	= S5P_PA_FIMC0 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC0,
		.end	= IRQ_FIMC0,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_fimc0 = {
	.name		= "s5p-fimc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s5p_fimc0_resource),
	.resource	= s5p_fimc0_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};
#endif /* CONFIG_S5P_DEV_FIMC0 */

#ifdef CONFIG_S5P_DEV_FIMC1
static struct resource s5p_fimc1_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC1,
		.end	= S5P_PA_FIMC1 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC1,
		.end	= IRQ_FIMC1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_fimc1 = {
	.name		= "s5p-fimc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s5p_fimc1_resource),
	.resource	= s5p_fimc1_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};
#endif /* CONFIG_S5P_DEV_FIMC1 */

#ifdef CONFIG_S5P_DEV_FIMC2
static struct resource s5p_fimc2_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC2,
		.end	= S5P_PA_FIMC2 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC2,
		.end	= IRQ_FIMC2,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_fimc2 = {
	.name		= "s5p-fimc",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(s5p_fimc2_resource),
	.resource	= s5p_fimc2_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};
#endif /* CONFIG_S5P_DEV_FIMC2 */

#ifdef CONFIG_S5P_DEV_FIMC3
static struct resource s5p_fimc3_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC3,
		.end	= S5P_PA_FIMC3 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC3,
		.end	= IRQ_FIMC3,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_fimc3 = {
	.name		= "s5p-fimc",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(s5p_fimc3_resource),
	.resource	= s5p_fimc3_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};
#endif /* CONFIG_S5P_DEV_FIMC3 */

/* FIMD0 */

#ifdef CONFIG_S5P_DEV_FIMD0
static struct resource s5p_fimd0_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMD0,
		.end	= S5P_PA_FIMD0 + SZ_32K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMD0_VSYNC,
		.end	= IRQ_FIMD0_VSYNC,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_FIMD0_FIFO,
		.end	= IRQ_FIMD0_FIFO,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= IRQ_FIMD0_SYSTEM,
		.end	= IRQ_FIMD0_SYSTEM,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_fimd0 = {
	.name		= "s5p-fb",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s5p_fimd0_resource),
	.resource	= s5p_fimd0_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

void __init s5p_fimd0_set_platdata(struct s3c_fb_platdata *pd)
{
	s3c_set_platdata(pd, sizeof(struct s3c_fb_platdata),
			 &s5p_device_fimd0);
}
#endif /* CONFIG_S5P_DEV_FIMD0 */

/* HWMON */

#ifdef CONFIG_S3C_DEV_HWMON
struct platform_device s3c_device_hwmon = {
	.name		= "s3c-hwmon",
	.id		= -1,
	.dev.parent	= &s3c_device_adc.dev,
};

void __init s3c_hwmon_set_platdata(struct s3c_hwmon_pdata *pd)
{
	s3c_set_platdata(pd, sizeof(struct s3c_hwmon_pdata),
			 &s3c_device_hwmon);
}
#endif /* CONFIG_S3C_DEV_HWMON */

/* HSMMC */

#define S3C_SZ_HSMMC	0x1000

#ifdef CONFIG_S3C_DEV_HSMMC
static struct resource s3c_hsmmc_resource[] = {
	[0] = {
		.start	= S3C_PA_HSMMC0,
		.end	= S3C_PA_HSMMC0 + S3C_SZ_HSMMC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_HSMMC0,
		.end	= IRQ_HSMMC0,
		.flags	= IORESOURCE_IRQ,
	}
};

struct s3c_sdhci_platdata s3c_hsmmc0_def_platdata = {
	.max_width	= 4,
	.host_caps	= (MMC_CAP_4_BIT_DATA |
			   MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED),
	.clk_type	= S3C_SDHCI_CLK_DIV_INTERNAL,
};

struct platform_device s3c_device_hsmmc0 = {
	.name		= "s3c-sdhci",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s3c_hsmmc_resource),
	.resource	= s3c_hsmmc_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &s3c_hsmmc0_def_platdata,
	},
};

void s3c_sdhci0_set_platdata(struct s3c_sdhci_platdata *pd)
{
	s3c_sdhci_set_platdata(pd, &s3c_hsmmc0_def_platdata);
}
#endif /* CONFIG_S3C_DEV_HSMMC */

#ifdef CONFIG_S3C_DEV_HSMMC1
static struct resource s3c_hsmmc1_resource[] = {
	[0] = {
		.start	= S3C_PA_HSMMC1,
		.end	= S3C_PA_HSMMC1 + S3C_SZ_HSMMC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_HSMMC1,
		.end	= IRQ_HSMMC1,
		.flags	= IORESOURCE_IRQ,
	}
};

struct s3c_sdhci_platdata s3c_hsmmc1_def_platdata = {
	.max_width	= 4,
	.host_caps	= (MMC_CAP_4_BIT_DATA |
			   MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED),
	.clk_type	= S3C_SDHCI_CLK_DIV_INTERNAL,
};

struct platform_device s3c_device_hsmmc1 = {
	.name		= "s3c-sdhci",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s3c_hsmmc1_resource),
	.resource	= s3c_hsmmc1_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &s3c_hsmmc1_def_platdata,
	},
};

void s3c_sdhci1_set_platdata(struct s3c_sdhci_platdata *pd)
{
	s3c_sdhci_set_platdata(pd, &s3c_hsmmc1_def_platdata);
}
#endif /* CONFIG_S3C_DEV_HSMMC1 */

/* HSMMC2 */

#ifdef CONFIG_S3C_DEV_HSMMC2
static struct resource s3c_hsmmc2_resource[] = {
	[0] = {
		.start	= S3C_PA_HSMMC2,
		.end	= S3C_PA_HSMMC2 + S3C_SZ_HSMMC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_HSMMC2,
		.end	= IRQ_HSMMC2,
		.flags	= IORESOURCE_IRQ,
	}
};

struct s3c_sdhci_platdata s3c_hsmmc2_def_platdata = {
	.max_width	= 4,
	.host_caps	= (MMC_CAP_4_BIT_DATA |
			   MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED),
	.clk_type	= S3C_SDHCI_CLK_DIV_INTERNAL,
};

struct platform_device s3c_device_hsmmc2 = {
	.name		= "s3c-sdhci",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(s3c_hsmmc2_resource),
	.resource	= s3c_hsmmc2_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &s3c_hsmmc2_def_platdata,
	},
};

void s3c_sdhci2_set_platdata(struct s3c_sdhci_platdata *pd)
{
	s3c_sdhci_set_platdata(pd, &s3c_hsmmc2_def_platdata);
}
#endif /* CONFIG_S3C_DEV_HSMMC2 */

#ifdef CONFIG_S3C_DEV_HSMMC3
static struct resource s3c_hsmmc3_resource[] = {
	[0] = {
		.start	= S3C_PA_HSMMC3,
		.end	= S3C_PA_HSMMC3 + S3C_SZ_HSMMC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_HSMMC3,
		.end	= IRQ_HSMMC3,
		.flags	= IORESOURCE_IRQ,
	}
};

struct s3c_sdhci_platdata s3c_hsmmc3_def_platdata = {
	.max_width	= 4,
	.host_caps	= (MMC_CAP_4_BIT_DATA |
			   MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED),
	.clk_type	= S3C_SDHCI_CLK_DIV_INTERNAL,
};

struct platform_device s3c_device_hsmmc3 = {
	.name		= "s3c-sdhci",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(s3c_hsmmc3_resource),
	.resource	= s3c_hsmmc3_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &s3c_hsmmc3_def_platdata,
	},
};

void s3c_sdhci3_set_platdata(struct s3c_sdhci_platdata *pd)
{
	s3c_sdhci_set_platdata(pd, &s3c_hsmmc3_def_platdata);
}
#endif /* CONFIG_S3C_DEV_HSMMC3 */

/* I2C */

static struct resource s3c_i2c0_resource[] = {
	[0] = {
		.start	= S3C_PA_IIC,
		.end	= S3C_PA_IIC + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC,
		.end	= IRQ_IIC,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_i2c0 = {
	.name		= "s3c2410-i2c",
#ifdef CONFIG_S3C_DEV_I2C1
	.id		= 0,
#else
	.id		= -1,
#endif
	.num_resources	= ARRAY_SIZE(s3c_i2c0_resource),
	.resource	= s3c_i2c0_resource,
};

struct s3c2410_platform_i2c default_i2c_data __initdata = {
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 100*1000,
	.sda_delay	= 100,
};

void __init s3c_i2c0_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd)
		pd = &default_i2c_data;

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s3c_device_i2c0);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = s3c_i2c0_cfg_gpio;
}

#ifdef CONFIG_S3C_DEV_I2C1
static struct resource s3c_i2c1_resource[] = {
	[0] = {
		.start	= S3C_PA_IIC1,
		.end	= S3C_PA_IIC1 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC1,
		.end	= IRQ_IIC1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_i2c1 = {
	.name		= "s3c2410-i2c",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s3c_i2c1_resource),
	.resource	= s3c_i2c1_resource,
};

void __init s3c_i2c1_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd) {
		pd = &default_i2c_data;
		pd->bus_num = 1;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s3c_device_i2c1);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = s3c_i2c1_cfg_gpio;
}
#endif /* CONFIG_S3C_DEV_I2C1 */

#ifdef CONFIG_S3C_DEV_I2C2
static struct resource s3c_i2c2_resource[] = {
	[0] = {
		.start	= S3C_PA_IIC2,
		.end	= S3C_PA_IIC2 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC2,
		.end	= IRQ_IIC2,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_i2c2 = {
	.name		= "s3c2410-i2c",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(s3c_i2c2_resource),
	.resource	= s3c_i2c2_resource,
};

void __init s3c_i2c2_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd) {
		pd = &default_i2c_data;
		pd->bus_num = 2;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s3c_device_i2c2);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = s3c_i2c2_cfg_gpio;
}
#endif /* CONFIG_S3C_DEV_I2C2 */

#ifdef CONFIG_S3C_DEV_I2C3
static struct resource s3c_i2c3_resource[] = {
	[0] = {
		.start	= S3C_PA_IIC3,
		.end	= S3C_PA_IIC3 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC3,
		.end	= IRQ_IIC3,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_i2c3 = {
	.name		= "s3c2440-i2c",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(s3c_i2c3_resource),
	.resource	= s3c_i2c3_resource,
};

void __init s3c_i2c3_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd) {
		pd = &default_i2c_data;
		pd->bus_num = 3;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s3c_device_i2c3);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = s3c_i2c3_cfg_gpio;
}
#endif /*CONFIG_S3C_DEV_I2C3 */

#ifdef CONFIG_S3C_DEV_I2C4
static struct resource s3c_i2c4_resource[] = {
	[0] = {
		.start	= S3C_PA_IIC4,
		.end	= S3C_PA_IIC4 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC4,
		.end	= IRQ_IIC4,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_i2c4 = {
	.name		= "s3c2440-i2c",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(s3c_i2c4_resource),
	.resource	= s3c_i2c4_resource,
};

void __init s3c_i2c4_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd) {
		pd = &default_i2c_data;
		pd->bus_num = 4;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s3c_device_i2c4);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = s3c_i2c4_cfg_gpio;
}
#endif /*CONFIG_S3C_DEV_I2C4 */

#ifdef CONFIG_S3C_DEV_I2C5
static struct resource s3c_i2c5_resource[] = {
	[0] = {
		.start	= S3C_PA_IIC5,
		.end	= S3C_PA_IIC5 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC5,
		.end	= IRQ_IIC5,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_i2c5 = {
	.name		= "s3c2440-i2c",
	.id		= 5,
	.num_resources	= ARRAY_SIZE(s3c_i2c5_resource),
	.resource	= s3c_i2c5_resource,
};

void __init s3c_i2c5_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd) {
		pd = &default_i2c_data;
		pd->bus_num = 5;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s3c_device_i2c5);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = s3c_i2c5_cfg_gpio;
}
#endif /*CONFIG_S3C_DEV_I2C5 */

#ifdef CONFIG_S3C_DEV_I2C6
static struct resource s3c_i2c6_resource[] = {
	[0] = {
		.start	= S3C_PA_IIC6,
		.end	= S3C_PA_IIC6 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC6,
		.end	= IRQ_IIC6,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_i2c6 = {
	.name		= "s3c2440-i2c",
	.id		= 6,
	.num_resources	= ARRAY_SIZE(s3c_i2c6_resource),
	.resource	= s3c_i2c6_resource,
};

void __init s3c_i2c6_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd) {
		pd = &default_i2c_data;
		pd->bus_num = 6;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s3c_device_i2c6);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = s3c_i2c6_cfg_gpio;
}
#endif /* CONFIG_S3C_DEV_I2C6 */

#ifdef CONFIG_S3C_DEV_I2C7
static struct resource s3c_i2c7_resource[] = {
	[0] = {
		.start	= S3C_PA_IIC7,
		.end	= S3C_PA_IIC7 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC7,
		.end	= IRQ_IIC7,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_i2c7 = {
	.name		= "s3c2440-i2c",
	.id		= 7,
	.num_resources	= ARRAY_SIZE(s3c_i2c7_resource),
	.resource	= s3c_i2c7_resource,
};

void __init s3c_i2c7_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd) {
		pd = &default_i2c_data;
		pd->bus_num = 7;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s3c_device_i2c7);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = s3c_i2c7_cfg_gpio;
}
#endif /* CONFIG_S3C_DEV_I2C7 */

/* I2C HDMIPHY */

#ifdef CONFIG_S5P_DEV_I2C_HDMIPHY
static struct resource s5p_i2c_resource[] = {
	[0] = {
		.start	= S5P_PA_IIC_HDMIPHY,
		.end	= S5P_PA_IIC_HDMIPHY + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_IIC_HDMIPHY,
		.end	= IRQ_IIC_HDMIPHY,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_i2c_hdmiphy = {
	.name		= "s3c2440-hdmiphy-i2c",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_i2c_resource),
	.resource	= s5p_i2c_resource,
};

void __init s5p_i2c_hdmiphy_set_platdata(struct s3c2410_platform_i2c *pd)
{
	struct s3c2410_platform_i2c *npd;

	if (!pd) {
		pd = &default_i2c_data;

		if (soc_is_exynos4210())
			pd->bus_num = 8;
		else if (soc_is_s5pv210())
			pd->bus_num = 3;
		else
			pd->bus_num = 0;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s3c2410_platform_i2c),
			       &s5p_device_i2c_hdmiphy);
}
#endif /* CONFIG_S5P_DEV_I2C_HDMIPHY */

/* I2S */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_iis_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_IIS,
		.end	= S3C24XX_PA_IIS + S3C24XX_SZ_IIS - 1,
		.flags	= IORESOURCE_MEM,
	}
};

struct platform_device s3c_device_iis = {
	.name		= "s3c24xx-iis",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_iis_resource),
	.resource	= s3c_iis_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif /* CONFIG_PLAT_S3C24XX */

#ifdef CONFIG_CPU_S3C2440
struct platform_device s3c2412_device_iis = {
	.name		= "s3c2412-iis",
	.id		= -1,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif /* CONFIG_CPU_S3C2440 */

/* IDE CFCON */

#ifdef CONFIG_SAMSUNG_DEV_IDE
static struct resource s3c_cfcon_resource[] = {
	[0] = {
		.start	= SAMSUNG_PA_CFCON,
		.end	= SAMSUNG_PA_CFCON + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_CFCON,
		.end	= IRQ_CFCON,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_cfcon = {
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s3c_cfcon_resource),
	.resource	= s3c_cfcon_resource,
};

void s3c_ide_set_platdata(struct s3c_ide_platdata *pdata)
{
	s3c_set_platdata(pdata, sizeof(struct s3c_ide_platdata),
			 &s3c_device_cfcon);
}
#endif /* CONFIG_SAMSUNG_DEV_IDE */

/* KEYPAD */

#ifdef CONFIG_SAMSUNG_DEV_KEYPAD
static struct resource samsung_keypad_resources[] = {
	[0] = {
		.start	= SAMSUNG_PA_KEYPAD,
		.end	= SAMSUNG_PA_KEYPAD + 0x20 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_KEYPAD,
		.end	= IRQ_KEYPAD,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device samsung_device_keypad = {
	.name		= "samsung-keypad",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(samsung_keypad_resources),
	.resource	= samsung_keypad_resources,
};

void __init samsung_keypad_set_platdata(struct samsung_keypad_platdata *pd)
{
	struct samsung_keypad_platdata *npd;

	npd = s3c_set_platdata(pd, sizeof(struct samsung_keypad_platdata),
			&samsung_device_keypad);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = samsung_keypad_cfg_gpio;
}
#endif /* CONFIG_SAMSUNG_DEV_KEYPAD */

/* LCD Controller */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_lcd_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_LCD,
		.end	= S3C24XX_PA_LCD + S3C24XX_SZ_LCD - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_LCD,
		.end	= IRQ_LCD,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_lcd = {
	.name		= "s3c2410-lcd",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_lcd_resource),
	.resource	= s3c_lcd_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

void __init s3c24xx_fb_set_platdata(struct s3c2410fb_mach_info *pd)
{
	struct s3c2410fb_mach_info *npd;

	npd = s3c_set_platdata(pd, sizeof(*npd), &s3c_device_lcd);
	if (npd) {
		npd->displays = kmemdup(pd->displays,
			sizeof(struct s3c2410fb_display) * npd->num_displays,
			GFP_KERNEL);
		if (!npd->displays)
			printk(KERN_ERR "no memory for LCD display data\n");
	} else {
		printk(KERN_ERR "no memory for LCD platform data\n");
	}
}
#endif /* CONFIG_PLAT_S3C24XX */

/* MFC */

#ifdef CONFIG_S5P_DEV_MFC
static struct resource s5p_mfc_resource[] = {
	[0] = {
		.start	= S5P_PA_MFC,
		.end	= S5P_PA_MFC + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_MFC,
		.end	= IRQ_MFC,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s5p_device_mfc = {
	.name		= "s5p-mfc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_mfc_resource),
	.resource	= s5p_mfc_resource,
};

/*
 * MFC hardware has 2 memory interfaces which are modelled as two separate
 * platform devices to let dma-mapping distinguish between them.
 *
 * MFC parent device (s5p_device_mfc) must be registered before memory
 * interface specific devices (s5p_device_mfc_l and s5p_device_mfc_r).
 */

struct platform_device s5p_device_mfc_l = {
	.name		= "s5p-mfc-l",
	.id		= -1,
	.dev		= {
		.parent			= &s5p_device_mfc.dev,
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

struct platform_device s5p_device_mfc_r = {
	.name		= "s5p-mfc-r",
	.id		= -1,
	.dev		= {
		.parent			= &s5p_device_mfc.dev,
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};
#endif /* CONFIG_S5P_DEV_MFC */

/* MIPI CSIS */

#ifdef CONFIG_S5P_DEV_CSIS0
static struct resource s5p_mipi_csis0_resource[] = {
	[0] = {
		.start	= S5P_PA_MIPI_CSIS0,
		.end	= S5P_PA_MIPI_CSIS0 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_MIPI_CSIS0,
		.end	= IRQ_MIPI_CSIS0,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s5p_device_mipi_csis0 = {
	.name		= "s5p-mipi-csis",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s5p_mipi_csis0_resource),
	.resource	= s5p_mipi_csis0_resource,
};
#endif /* CONFIG_S5P_DEV_CSIS0 */

#ifdef CONFIG_S5P_DEV_CSIS1
static struct resource s5p_mipi_csis1_resource[] = {
	[0] = {
		.start	= S5P_PA_MIPI_CSIS1,
		.end	= S5P_PA_MIPI_CSIS1 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_MIPI_CSIS1,
		.end	= IRQ_MIPI_CSIS1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_mipi_csis1 = {
	.name		= "s5p-mipi-csis",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s5p_mipi_csis1_resource),
	.resource	= s5p_mipi_csis1_resource,
};
#endif

/* NAND */

#ifdef CONFIG_S3C_DEV_NAND
static struct resource s3c_nand_resource[] = {
	[0] = {
		.start	= S3C_PA_NAND,
		.end	= S3C_PA_NAND + SZ_1M,
		.flags	= IORESOURCE_MEM,
	}
};

struct platform_device s3c_device_nand = {
	.name		= "s3c2410-nand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_nand_resource),
	.resource	= s3c_nand_resource,
};

/*
 * s3c_nand_copy_set() - copy nand set data
 * @set: The new structure, directly copied from the old.
 *
 * Copy all the fields from the NAND set field from what is probably __initdata
 * to new kernel memory. The code returns 0 if the copy happened correctly or
 * an error code for the calling function to display.
 *
 * Note, we currently do not try and look to see if we've already copied the
 * data in a previous set.
 */
static int __init s3c_nand_copy_set(struct s3c2410_nand_set *set)
{
	void *ptr;
	int size;

	size = sizeof(struct mtd_partition) * set->nr_partitions;
	if (size) {
		ptr = kmemdup(set->partitions, size, GFP_KERNEL);
		set->partitions = ptr;

		if (!ptr)
			return -ENOMEM;
	}

	if (set->nr_map && set->nr_chips) {
		size = sizeof(int) * set->nr_chips;
		ptr = kmemdup(set->nr_map, size, GFP_KERNEL);
		set->nr_map = ptr;

		if (!ptr)
			return -ENOMEM;
	}

	if (set->ecc_layout) {
		ptr = kmemdup(set->ecc_layout,
			      sizeof(struct nand_ecclayout), GFP_KERNEL);
		set->ecc_layout = ptr;

		if (!ptr)
			return -ENOMEM;
	}

	return 0;
}

void __init s3c_nand_set_platdata(struct s3c2410_platform_nand *nand)
{
	struct s3c2410_platform_nand *npd;
	int size;
	int ret;

	/* note, if we get a failure in allocation, we simply drop out of the
	 * function. If there is so little memory available at initialisation
	 * time then there is little chance the system is going to run.
	 */

	npd = s3c_set_platdata(nand, sizeof(struct s3c2410_platform_nand),
				&s3c_device_nand);
	if (!npd)
		return;

	/* now see if we need to copy any of the nand set data */

	size = sizeof(struct s3c2410_nand_set) * npd->nr_sets;
	if (size) {
		struct s3c2410_nand_set *from = npd->sets;
		struct s3c2410_nand_set *to;
		int i;

		to = kmemdup(from, size, GFP_KERNEL);
		npd->sets = to;	/* set, even if we failed */

		if (!to) {
			printk(KERN_ERR "%s: no memory for sets\n", __func__);
			return;
		}

		for (i = 0; i < npd->nr_sets; i++) {
			ret = s3c_nand_copy_set(to);
			if (ret) {
				printk(KERN_ERR "%s: failed to copy set %d\n",
				__func__, i);
				return;
			}
			to++;
		}
	}
}
#endif /* CONFIG_S3C_DEV_NAND */

/* ONENAND */

#ifdef CONFIG_S3C_DEV_ONENAND
static struct resource s3c_onenand_resources[] = {
	[0] = {
		.start	= S3C_PA_ONENAND,
		.end	= S3C_PA_ONENAND + 0x400 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= S3C_PA_ONENAND_BUF,
		.end	= S3C_PA_ONENAND_BUF + S3C_SZ_ONENAND_BUF - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_ONENAND,
		.end	= IRQ_ONENAND,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_onenand = {
	.name		= "samsung-onenand",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s3c_onenand_resources),
	.resource	= s3c_onenand_resources,
};
#endif /* CONFIG_S3C_DEV_ONENAND */

#ifdef CONFIG_S3C64XX_DEV_ONENAND1
static struct resource s3c64xx_onenand1_resources[] = {
	[0] = {
		.start	= S3C64XX_PA_ONENAND1,
		.end	= S3C64XX_PA_ONENAND1 + 0x400 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= S3C64XX_PA_ONENAND1_BUF,
		.end	= S3C64XX_PA_ONENAND1_BUF + S3C64XX_SZ_ONENAND1_BUF - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_ONENAND1,
		.end	= IRQ_ONENAND1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c64xx_device_onenand1 = {
	.name		= "samsung-onenand",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s3c64xx_onenand1_resources),
	.resource	= s3c64xx_onenand1_resources,
};

void s3c64xx_onenand1_set_platdata(struct onenand_platform_data *pdata)
{
	s3c_set_platdata(pdata, sizeof(struct onenand_platform_data),
			 &s3c64xx_device_onenand1);
}
#endif /* CONFIG_S3C64XX_DEV_ONENAND1 */

#ifdef CONFIG_S5P_DEV_ONENAND
static struct resource s5p_onenand_resources[] = {
	[0] = {
		.start	= S5P_PA_ONENAND,
		.end	= S5P_PA_ONENAND + SZ_128K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= S5P_PA_ONENAND_DMA,
		.end	= S5P_PA_ONENAND_DMA + SZ_8K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_ONENAND_AUDI,
		.end	= IRQ_ONENAND_AUDI,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_onenand = {
	.name		= "s5pc110-onenand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_onenand_resources),
	.resource	= s5p_onenand_resources,
};
#endif /* CONFIG_S5P_DEV_ONENAND */

/* PMU */

#ifdef CONFIG_PLAT_S5P
static struct resource s5p_pmu_resource = {
	.start	= IRQ_PMU,
	.end	= IRQ_PMU,
	.flags	= IORESOURCE_IRQ,
};

struct platform_device s5p_device_pmu = {
	.name		= "arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
	.num_resources	= 1,
	.resource	= &s5p_pmu_resource,
};

static int __init s5p_pmu_init(void)
{
	platform_device_register(&s5p_device_pmu);
	return 0;
}
arch_initcall(s5p_pmu_init);
#endif /* CONFIG_PLAT_S5P */

/* PWM Timer */

#ifdef CONFIG_SAMSUNG_DEV_PWM

#define TIMER_RESOURCE_SIZE (1)

#define TIMER_RESOURCE(_tmr, _irq)			\
	(struct resource [TIMER_RESOURCE_SIZE]) {	\
		[0] = {					\
			.start	= _irq,			\
			.end	= _irq,			\
			.flags	= IORESOURCE_IRQ	\
		}					\
	}

#define DEFINE_S3C_TIMER(_tmr_no, _irq)			\
	.name		= "s3c24xx-pwm",		\
	.id		= _tmr_no,			\
	.num_resources	= TIMER_RESOURCE_SIZE,		\
	.resource	= TIMER_RESOURCE(_tmr_no, _irq),	\

/*
 * since we already have an static mapping for the timer,
 * we do not bother setting any IO resource for the base.
 */

struct platform_device s3c_device_timer[] = {
	[0] = { DEFINE_S3C_TIMER(0, IRQ_TIMER0) },
	[1] = { DEFINE_S3C_TIMER(1, IRQ_TIMER1) },
	[2] = { DEFINE_S3C_TIMER(2, IRQ_TIMER2) },
	[3] = { DEFINE_S3C_TIMER(3, IRQ_TIMER3) },
	[4] = { DEFINE_S3C_TIMER(4, IRQ_TIMER4) },
};
#endif /* CONFIG_SAMSUNG_DEV_PWM */

/* RTC */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_rtc_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_RTC,
		.end	= S3C24XX_PA_RTC + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_RTC,
		.end	= IRQ_RTC,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_TICK,
		.end	= IRQ_TICK,
		.flags	= IORESOURCE_IRQ
	}
};

struct platform_device s3c_device_rtc = {
	.name		= "s3c2410-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_rtc_resource),
	.resource	= s3c_rtc_resource,
};
#endif /* CONFIG_PLAT_S3C24XX */

#ifdef CONFIG_S3C_DEV_RTC
static struct resource s3c_rtc_resource[] = {
	[0] = {
		.start	= S3C_PA_RTC,
		.end	= S3C_PA_RTC + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_RTC_ALARM,
		.end	= IRQ_RTC_ALARM,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_RTC_TIC,
		.end	= IRQ_RTC_TIC,
		.flags	= IORESOURCE_IRQ
	}
};

struct platform_device s3c_device_rtc = {
	.name		= "s3c64xx-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_rtc_resource),
	.resource	= s3c_rtc_resource,
};
#endif /* CONFIG_S3C_DEV_RTC */

/* SDI */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_sdi_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_SDI,
		.end	= S3C24XX_PA_SDI + S3C24XX_SZ_SDI - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_SDI,
		.end	= IRQ_SDI,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_sdi = {
	.name		= "s3c2410-sdi",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_sdi_resource),
	.resource	= s3c_sdi_resource,
};

void __init s3c24xx_mci_set_platdata(struct s3c24xx_mci_pdata *pdata)
{
	s3c_set_platdata(pdata, sizeof(struct s3c24xx_mci_pdata),
			 &s3c_device_sdi);
}
#endif /* CONFIG_PLAT_S3C24XX */

/* SPI */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_spi0_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_SPI,
		.end	= S3C24XX_PA_SPI + 0x1f,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_SPI0,
		.end	= IRQ_SPI0,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_spi0 = {
	.name		= "s3c2410-spi",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s3c_spi0_resource),
	.resource	= s3c_spi0_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

static struct resource s3c_spi1_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_SPI + S3C2410_SPI1,
		.end	= S3C24XX_PA_SPI + S3C2410_SPI1 + 0x1f,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_SPI1,
		.end	= IRQ_SPI1,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_spi1 = {
	.name		= "s3c2410-spi",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s3c_spi1_resource),
	.resource	= s3c_spi1_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif /* CONFIG_PLAT_S3C24XX */

/* Touchscreen */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_ts_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_ADC,
		.end	= S3C24XX_PA_ADC + S3C24XX_SZ_ADC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_TC,
		.end	= IRQ_TC,
		.flags	= IORESOURCE_IRQ,
	},

};

struct platform_device s3c_device_ts = {
	.name		= "s3c2410-ts",
	.id		= -1,
	.dev.parent	= &s3c_device_adc.dev,
	.num_resources	= ARRAY_SIZE(s3c_ts_resource),
	.resource	= s3c_ts_resource,
};

void __init s3c24xx_ts_set_platdata(struct s3c2410_ts_mach_info *hard_s3c2410ts_info)
{
	s3c_set_platdata(hard_s3c2410ts_info,
			 sizeof(struct s3c2410_ts_mach_info), &s3c_device_ts);
}
#endif /* CONFIG_PLAT_S3C24XX */

#ifdef CONFIG_SAMSUNG_DEV_TS
static struct resource s3c_ts_resource[] = {
	[0] = {
		.start	= SAMSUNG_PA_ADC,
		.end	= SAMSUNG_PA_ADC + SZ_256 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_TC,
		.end	= IRQ_TC,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct s3c2410_ts_mach_info default_ts_data __initdata = {
	.delay			= 10000,
	.presc			= 49,
	.oversampling_shift	= 2,
};

struct platform_device s3c_device_ts = {
	.name		= "s3c64xx-ts",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_ts_resource),
	.resource	= s3c_ts_resource,
};

void __init s3c24xx_ts_set_platdata(struct s3c2410_ts_mach_info *pd)
{
	if (!pd)
		pd = &default_ts_data;

	s3c_set_platdata(pd, sizeof(struct s3c2410_ts_mach_info),
			 &s3c_device_ts);
}
#endif /* CONFIG_SAMSUNG_DEV_TS */

/* TV */

#ifdef CONFIG_S5P_DEV_TV

static struct resource s5p_hdmi_resources[] = {
	[0] = {
		.start	= S5P_PA_HDMI,
		.end	= S5P_PA_HDMI + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_HDMI,
		.end	= IRQ_HDMI,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_hdmi = {
	.name		= "s5p-hdmi",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_hdmi_resources),
	.resource	= s5p_hdmi_resources,
};

static struct resource s5p_sdo_resources[] = {
	[0] = {
		.start	= S5P_PA_SDO,
		.end	= S5P_PA_SDO + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_SDO,
		.end	= IRQ_SDO,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s5p_device_sdo = {
	.name		= "s5p-sdo",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_sdo_resources),
	.resource	= s5p_sdo_resources,
};

static struct resource s5p_mixer_resources[] = {
	[0] = {
		.start	= S5P_PA_MIXER,
		.end	= S5P_PA_MIXER + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
		.name	= "mxr"
	},
	[1] = {
		.start	= S5P_PA_VP,
		.end	= S5P_PA_VP + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
		.name	= "vp"
	},
	[2] = {
		.start	= IRQ_MIXER,
		.end	= IRQ_MIXER,
		.flags	= IORESOURCE_IRQ,
		.name	= "irq"
	}
};

struct platform_device s5p_device_mixer = {
	.name		= "s5p-mixer",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_mixer_resources),
	.resource	= s5p_mixer_resources,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif /* CONFIG_S5P_DEV_TV */

/* USB */

#ifdef CONFIG_S3C_DEV_USB_HOST
static struct resource s3c_usb_resource[] = {
	[0] = {
		.start	= S3C_PA_USBHOST,
		.end	= S3C_PA_USBHOST + 0x100 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_USBH,
		.end	= IRQ_USBH,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_ohci = {
	.name		= "s3c2410-ohci",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_usb_resource),
	.resource	= s3c_usb_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

/*
 * s3c_ohci_set_platdata - initialise OHCI device platform data
 * @info: The platform data.
 *
 * This call copies the @info passed in and sets the device .platform_data
 * field to that copy. The @info is copied so that the original can be marked
 * __initdata.
 */

void __init s3c_ohci_set_platdata(struct s3c2410_hcd_info *info)
{
	s3c_set_platdata(info, sizeof(struct s3c2410_hcd_info),
			 &s3c_device_ohci);
}
#endif /* CONFIG_S3C_DEV_USB_HOST */

/* USB Device (Gadget) */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_usbgadget_resource[] = {
	[0] = {
		.start	= S3C24XX_PA_USBDEV,
		.end	= S3C24XX_PA_USBDEV + S3C24XX_SZ_USBDEV - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_USBD,
		.end	= IRQ_USBD,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_usbgadget = {
	.name		= "s3c2410-usbgadget",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_usbgadget_resource),
	.resource	= s3c_usbgadget_resource,
};

void __init s3c24xx_udc_set_platdata(struct s3c2410_udc_mach_info *pd)
{
	s3c_set_platdata(pd, sizeof(*pd), &s3c_device_usbgadget);
}
#endif /* CONFIG_PLAT_S3C24XX */

/* USB EHCI Host Controller */

#ifdef CONFIG_S5P_DEV_USB_EHCI
static struct resource s5p_ehci_resource[] = {
	[0] = {
		.start	= S5P_PA_EHCI,
		.end	= S5P_PA_EHCI + SZ_256 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_USB_HOST,
		.end	= IRQ_USB_HOST,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s5p_device_ehci = {
	.name		= "s5p-ehci",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_ehci_resource),
	.resource	= s5p_ehci_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

void __init s5p_ehci_set_platdata(struct s5p_ehci_platdata *pd)
{
	struct s5p_ehci_platdata *npd;

	npd = s3c_set_platdata(pd, sizeof(struct s5p_ehci_platdata),
			&s5p_device_ehci);

	if (!npd->phy_init)
		npd->phy_init = s5p_usb_phy_init;
	if (!npd->phy_exit)
		npd->phy_exit = s5p_usb_phy_exit;
}
#endif /* CONFIG_S5P_DEV_USB_EHCI */

/* USB HSOTG */

#ifdef CONFIG_S3C_DEV_USB_HSOTG
static struct resource s3c_usb_hsotg_resources[] = {
	[0] = {
		.start	= S3C_PA_USB_HSOTG,
		.end	= S3C_PA_USB_HSOTG + 0x10000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_OTG,
		.end	= IRQ_OTG,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_usb_hsotg = {
	.name		= "s3c-hsotg",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_usb_hsotg_resources),
	.resource	= s3c_usb_hsotg_resources,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};
#endif /* CONFIG_S3C_DEV_USB_HSOTG */

/* USB High Spped 2.0 Device (Gadget) */

#ifdef CONFIG_PLAT_S3C24XX
static struct resource s3c_hsudc_resource[] = {
	[0] = {
		.start	= S3C2416_PA_HSUDC,
		.end	= S3C2416_PA_HSUDC + S3C2416_SZ_HSUDC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_USBD,
		.end	= IRQ_USBD,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_usb_hsudc = {
	.name		= "s3c-hsudc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_hsudc_resource),
	.resource	= s3c_hsudc_resource,
	.dev		= {
		.dma_mask		= &samsung_device_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

void __init s3c24xx_hsudc_set_platdata(struct s3c24xx_hsudc_platdata *pd)
{
	s3c_set_platdata(pd, sizeof(*pd), &s3c_device_usb_hsudc);
}
#endif /* CONFIG_PLAT_S3C24XX */

/* WDT */

#ifdef CONFIG_S3C_DEV_WDT
static struct resource s3c_wdt_resource[] = {
	[0] = {
		.start	= S3C_PA_WDT,
		.end	= S3C_PA_WDT + SZ_1K,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_WDT,
		.end	= IRQ_WDT,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_wdt = {
	.name		= "s3c2410-wdt",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_wdt_resource),
	.resource	= s3c_wdt_resource,
};
#endif /* CONFIG_S3C_DEV_WDT */

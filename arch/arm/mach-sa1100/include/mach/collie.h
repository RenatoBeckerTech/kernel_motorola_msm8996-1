/*
 * arch/arm/mach-sa1100/include/mach/collie.h
 *
 * This file contains the hardware specific definitions for Assabet
 * Only include this file from SA1100-specific files.
 *
 * ChangeLog:
 *   04-06-2001 Lineo Japan, Inc.
 *   04-16-2001 SHARP Corporation
 *   07-07-2002 Chris Larson <clarson@digi.com>
 *
 */
#ifndef __ASM_ARCH_COLLIE_H
#define __ASM_ARCH_COLLIE_H


#define COLLIE_SCOOP_GPIO_BASE	(GPIO_MAX + 1)
#define COLLIE_GPIO_CHARGE_ON	(COLLIE_SCOOP_GPIO_BASE + 0)
#define COLLIE_SCP_DIAG_BOOT1	SCOOP_GPCR_PA12
#define COLLIE_SCP_DIAG_BOOT2	SCOOP_GPCR_PA13
#define COLLIE_SCP_MUTE_L	SCOOP_GPCR_PA14
#define COLLIE_SCP_MUTE_R	SCOOP_GPCR_PA15
#define COLLIE_SCP_5VON		SCOOP_GPCR_PA16
#define COLLIE_SCP_AMP_ON	SCOOP_GPCR_PA17
#define COLLIE_GPIO_VPEN	(COLLIE_SCOOP_GPIO_BASE + 7)
#define COLLIE_SCP_LB_VOL_CHG	SCOOP_GPCR_PA19

#define COLLIE_SCOOP_IO_DIR	(COLLIE_SCP_MUTE_L | COLLIE_SCP_MUTE_R | \
				COLLIE_SCP_5VON | COLLIE_SCP_AMP_ON | \
				COLLIE_SCP_LB_VOL_CHG)
#define COLLIE_SCOOP_IO_OUT	(COLLIE_SCP_MUTE_L | COLLIE_SCP_MUTE_R)

/* GPIOs for gpiolib  */

#define COLLIE_GPIO_ON_KEY		(0)
#define COLLIE_GPIO_AC_IN		(1)
#define COLLIE_GPIO_SDIO_INT		(11)
#define COLLIE_GPIO_CF_IRQ		(14)
#define COLLIE_GPIO_nREMOCON_INT	(15)
#define COLLIE_GPIO_UCB1x00_RESET	(16)
#define COLLIE_GPIO_nMIC_ON		(17)
#define COLLIE_GPIO_nREMOCON_ON		(18)
#define COLLIE_GPIO_CO			(20)
#define COLLIE_GPIO_MCP_CLK		(21)
#define COLLIE_GPIO_CF_CD		(22)
#define COLLIE_GPIO_UCB1x00_IRQ		(23)
#define COLLIE_GPIO_WAKEUP		(24)
#define COLLIE_GPIO_GA_INT		(25)
#define COLLIE_GPIO_MAIN_BAT_LOW	(26)

/* GPIO definitions for direct register access */

#define _COLLIE_GPIO_ON_KEY		GPIO_GPIO(0)
#define _COLLIE_GPIO_AC_IN		GPIO_GPIO(1)
#define _COLLIE_GPIO_nREMOCON_INT	GPIO_GPIO(15)
#define _COLLIE_GPIO_UCB1x00_RESET	GPIO_GPIO(16)
#define _COLLIE_GPIO_nMIC_ON		GPIO_GPIO(17)
#define _COLLIE_GPIO_nREMOCON_ON	GPIO_GPIO(18)
#define _COLLIE_GPIO_CO			GPIO_GPIO(20)
#define _COLLIE_GPIO_WAKEUP		GPIO_GPIO(24)
/* Interrupts */

#define COLLIE_IRQ_GPIO_ON_KEY		IRQ_GPIO0
#define COLLIE_IRQ_GPIO_AC_IN		IRQ_GPIO1
#define COLLIE_IRQ_GPIO_SDIO_IRQ	IRQ_GPIO11
#define COLLIE_IRQ_GPIO_CF_IRQ		IRQ_GPIO14
#define COLLIE_IRQ_GPIO_nREMOCON_INT	IRQ_GPIO15
#define COLLIE_IRQ_GPIO_CO		IRQ_GPIO20
#define COLLIE_IRQ_GPIO_CF_CD		IRQ_GPIO22
#define COLLIE_IRQ_GPIO_UCB1x00_IRQ	IRQ_GPIO23
#define COLLIE_IRQ_GPIO_WAKEUP		IRQ_GPIO24
#define COLLIE_IRQ_GPIO_GA_INT		IRQ_GPIO25
#define COLLIE_IRQ_GPIO_MAIN_BAT_LOW	IRQ_GPIO26

#define COLLIE_LCM_IRQ_GPIO_RTS		IRQ_LOCOMO_GPIO0
#define COLLIE_LCM_IRQ_GPIO_CTS		IRQ_LOCOMO_GPIO1
#define COLLIE_LCM_IRQ_GPIO_DSR		IRQ_LOCOMO_GPIO2
#define COLLIE_LCM_IRQ_GPIO_DTR		IRQ_LOCOMO_GPIO3
#define COLLIE_LCM_IRQ_GPIO_nSD_DETECT	IRQ_LOCOMO_GPIO13
#define COLLIE_LCM_IRQ_GPIO_nSD_WP	IRQ_LOCOMO_GPIO14

/* GPIO's on the TC35143AF (Toshiba Analog Frontend) */
#define COLLIE_TC35143_GPIO_BASE	(GPIO_MAX + 13)
#define COLLIE_TC35143_GPIO_VERSION0    UCB_IO_0
#define COLLIE_TC35143_GPIO_TBL_CHK     UCB_IO_1
#define COLLIE_TC35143_GPIO_VPEN_ON     UCB_IO_2
#define COLLIE_TC35143_GPIO_IR_ON       UCB_IO_3
#define COLLIE_TC35143_GPIO_AMP_ON      UCB_IO_4
#define COLLIE_TC35143_GPIO_VERSION1    UCB_IO_5
#define COLLIE_TC35143_GPIO_FS8KLPF     UCB_IO_5
#define COLLIE_TC35143_GPIO_BUZZER_BIAS UCB_IO_6
#define COLLIE_GPIO_MBAT_ON     	(COLLIE_TC35143_GPIO_BASE + 7)
#define COLLIE_GPIO_BBAT_ON     	(COLLIE_TC35143_GPIO_BASE + 8)
#define COLLIE_GPIO_TMP_ON      	(COLLIE_TC35143_GPIO_BASE + 9)
#define COLLIE_TC35143_GPIO_IN		(UCB_IO_0 | UCB_IO_2 | UCB_IO_5)
#define COLLIE_TC35143_GPIO_OUT		(UCB_IO_1 | UCB_IO_3 | UCB_IO_4 \
						| UCB_IO_6)

#endif

/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>

#include <plat/pincfg.h>

#include <mach/hardware.h>

#include "pins-db8500.h"

static pin_cfg_t mop500_pins[] = {
	/* SSP0 */
	GPIO143_SSP0_CLK,
	GPIO144_SSP0_FRM,
	GPIO145_SSP0_RXD,
	GPIO146_SSP0_TXD,

	/* I2C */
	GPIO147_I2C0_SCL,
	GPIO148_I2C0_SDA,
	GPIO16_I2C1_SCL,
	GPIO17_I2C1_SDA,
	GPIO10_I2C2_SDA,
	GPIO11_I2C2_SCL,
	GPIO229_I2C3_SDA,
	GPIO230_I2C3_SCL,

	/* SKE keypad */
	GPIO153_KP_I7,
	GPIO154_KP_I6,
	GPIO155_KP_I5,
	GPIO156_KP_I4,
	GPIO157_KP_O7,
	GPIO158_KP_O6,
	GPIO159_KP_O5,
	GPIO160_KP_O4,
	GPIO161_KP_I3,
	GPIO162_KP_I2,
	GPIO163_KP_I1,
	GPIO164_KP_I0,
	GPIO165_KP_O3,
	GPIO166_KP_O2,
	GPIO167_KP_O1,
	GPIO168_KP_O0,

	/* GPIO_EXP_INT */
	GPIO217_GPIO,

	/* STMPE1601 IRQ */
	GPIO218_GPIO    | PIN_INPUT_PULLUP,

	/* touch screen */
	GPIO84_GPIO     | PIN_INPUT_PULLUP,

	/* USB OTG */
	GPIO256_USB_NXT		| PIN_PULL_DOWN,
	GPIO257_USB_STP		| PIN_PULL_UP,
	GPIO258_USB_XCLK	| PIN_PULL_DOWN,
	GPIO259_USB_DIR		| PIN_PULL_DOWN,
	GPIO260_USB_DAT7	| PIN_PULL_DOWN,
	GPIO261_USB_DAT6	| PIN_PULL_DOWN,
	GPIO262_USB_DAT5	| PIN_PULL_DOWN,
	GPIO263_USB_DAT4	| PIN_PULL_DOWN,
	GPIO264_USB_DAT3	| PIN_PULL_DOWN,
	GPIO265_USB_DAT2	| PIN_PULL_DOWN,
	GPIO266_USB_DAT1	| PIN_PULL_DOWN,
	GPIO267_USB_DAT0	| PIN_PULL_DOWN,
};

void __init mop500_pins_init(void)
{
	nmk_config_pins(mop500_pins,
				ARRAY_SIZE(mop500_pins));
}

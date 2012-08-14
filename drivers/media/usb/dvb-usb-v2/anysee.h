/*
 * DVB USB Linux driver for Anysee E30 DVB-C & DVB-T USB2.0 receiver
 *
 * Copyright (C) 2007 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * TODO:
 * - add smart card reader support for Conditional Access (CA)
 *
 * Card reader in Anysee is nothing more than ISO 7816 card reader.
 * There is no hardware CAM in any Anysee device sold.
 * In my understanding it should be implemented by making own module
 * for ISO 7816 card reader, like dvb_ca_en50221 is implemented. This
 * module registers serial interface that can be used to communicate
 * with any ISO 7816 smart card.
 *
 * Any help according to implement serial smart card reader support
 * is highly welcome!
 */

#ifndef _DVB_USB_ANYSEE_H_
#define _DVB_USB_ANYSEE_H_

#define DVB_USB_LOG_PREFIX "anysee"
#include "dvb_usb.h"
#include "dvb_ca_en50221.h"

#ifdef CONFIG_DVB_USB_DEBUG
#define dprintk(var, level, args...) \
	do { if ((var & level)) printk(args); } while (0)
#define debug_dump(b, l, func) {\
	int loop_; \
	for (loop_ = 0; loop_ < l; loop_++) \
		func("%02x ", b[loop_]); \
	func("\n");\
}
#define DVB_USB_DEBUG_STATUS
#else
#define dprintk(args...)
#define debug_dump(b, l, func)
#define DVB_USB_DEBUG_STATUS " (debugging is not enabled)"
#endif

#define deb_info(args...) dprintk(dvb_usb_anysee_debug, 0x01, args)
#define deb_xfer(args...) dprintk(dvb_usb_anysee_debug, 0x02, args)
#define deb_rc(args...)   dprintk(dvb_usb_anysee_debug, 0x04, args)
#define deb_reg(args...)  dprintk(dvb_usb_anysee_debug, 0x08, args)
#define deb_i2c(args...)  dprintk(dvb_usb_anysee_debug, 0x10, args)
#define deb_fw(args...)   dprintk(dvb_usb_anysee_debug, 0x20, args)

#undef err
#define err(format, arg...)  printk(KERN_ERR     DVB_USB_LOG_PREFIX ": " format "\n" , ## arg)
#undef info
#define info(format, arg...) printk(KERN_INFO    DVB_USB_LOG_PREFIX ": " format "\n" , ## arg)
#undef warn
#define warn(format, arg...) printk(KERN_WARNING DVB_USB_LOG_PREFIX ": " format "\n" , ## arg)

enum cmd {
	CMD_I2C_READ            = 0x33,
	CMD_I2C_WRITE           = 0x31,
	CMD_REG_READ            = 0xb0,
	CMD_REG_WRITE           = 0xb1,
	CMD_STREAMING_CTRL      = 0x12,
	CMD_LED_AND_IR_CTRL     = 0x16,
	CMD_GET_IR_CODE         = 0x41,
	CMD_GET_HW_INFO         = 0x19,
	CMD_SMARTCARD           = 0x34,
	CMD_CI                  = 0x37,
};

struct anysee_state {
	u8 hw; /* PCB ID */
	u8 seq;
	u8 fe_id:1; /* frondend ID */
	u8 has_ci:1;
	struct dvb_ca_en50221 ci;
	unsigned long ci_cam_ready; /* jiffies */
};

#define ANYSEE_HW_507T    2 /* E30 */
#define ANYSEE_HW_507CD   6 /* E30 Plus */
#define ANYSEE_HW_507DC  10 /* E30 C Plus */
#define ANYSEE_HW_507SI  11 /* E30 S2 Plus */
#define ANYSEE_HW_507FA  15 /* E30 Combo Plus / E30 C Plus */
#define ANYSEE_HW_508TC  18 /* E7 TC */
#define ANYSEE_HW_508S2  19 /* E7 S2 */
#define ANYSEE_HW_508T2C 20 /* E7 T2C */
#define ANYSEE_HW_508PTC 21 /* E7 PTC Plus */
#define ANYSEE_HW_508PS2 22 /* E7 PS2 Plus */

#define REG_IOA       0x80 /* Port A (bit addressable) */
#define REG_IOB       0x90 /* Port B (bit addressable) */
#define REG_IOC       0xa0 /* Port C (bit addressable) */
#define REG_IOD       0xb0 /* Port D (bit addressable) */
#define REG_IOE       0xb1 /* Port E (NOT bit addressable) */
#define REG_OEA       0xb2 /* Port A Output Enable */
#define REG_OEB       0xb3 /* Port B Output Enable */
#define REG_OEC       0xb4 /* Port C Output Enable */
#define REG_OED       0xb5 /* Port D Output Enable */
#define REG_OEE       0xb6 /* Port E Output Enable */

#endif

/***************************************************************************
 * USB API description (reverse engineered)
 ***************************************************************************

Transaction flow:
=================
BULK[00001] >>> REQUEST PACKET 64 bytes
BULK[00081] <<< REPLY PACKET #1 64 bytes (PREVIOUS TRANSACTION REPLY)
BULK[00081] <<< REPLY PACKET #2 64 bytes (CURRENT TRANSACTION REPLY)

General reply packet(s) are always used if not own reply defined.

============================================================================
| 00-63 | GENERAL REPLY PACKET #1 (PREVIOUS REPLY)
============================================================================
|    00 | reply data (if any) from previous transaction
|       | Just same reply packet as returned during previous transaction.
|       | Needed only if reply is missed in previous transaction.
|       | Just skip normally.
----------------------------------------------------------------------------
| 01-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | GENERAL REPLY PACKET #2 (CURRENT REPLY)
============================================================================
|    00 | reply data (if any)
----------------------------------------------------------------------------
| 01-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | I2C WRITE REQUEST PACKET
============================================================================
|    00 | 0x31 I2C write command
----------------------------------------------------------------------------
|    01 | i2c address
----------------------------------------------------------------------------
|    02 | data length
|       | 0x02 (for typical I2C reg / val pair)
----------------------------------------------------------------------------
|    03 | 0x01
----------------------------------------------------------------------------
| 04-   | data
----------------------------------------------------------------------------
|   -59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | I2C READ REQUEST PACKET
============================================================================
|    00 | 0x33 I2C read command
----------------------------------------------------------------------------
|    01 | i2c address + 1
----------------------------------------------------------------------------
|    02 | register
----------------------------------------------------------------------------
|    03 | 0x00
----------------------------------------------------------------------------
|    04 | 0x00
----------------------------------------------------------------------------
|    05 | data length
----------------------------------------------------------------------------
| 06-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | USB CONTROLLER REGISTER WRITE REQUEST PACKET
============================================================================
|    00 | 0xb1 register write command
----------------------------------------------------------------------------
| 01-02 | register
----------------------------------------------------------------------------
|    03 | 0x01
----------------------------------------------------------------------------
|    04 | value
----------------------------------------------------------------------------
| 05-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | USB CONTROLLER REGISTER READ REQUEST PACKET
============================================================================
|    00 | 0xb0 register read command
----------------------------------------------------------------------------
| 01-02 | register
----------------------------------------------------------------------------
|    03 | 0x01
----------------------------------------------------------------------------
| 04-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | LED CONTROL REQUEST PACKET
============================================================================
|    00 | 0x16 LED and IR control command
----------------------------------------------------------------------------
|    01 | 0x01 (LED)
----------------------------------------------------------------------------
|    03 | 0x00 blink
|       | 0x01 lights continuously
----------------------------------------------------------------------------
|    04 | blink interval
|       | 0x00 fastest (looks like LED lights continuously)
|       | 0xff slowest
----------------------------------------------------------------------------
| 05-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | IR CONTROL REQUEST PACKET
============================================================================
|    00 | 0x16 LED and IR control command
----------------------------------------------------------------------------
|    01 | 0x02 (IR)
----------------------------------------------------------------------------
|    03 | 0x00 IR disabled
|       | 0x01 IR enabled
----------------------------------------------------------------------------
| 04-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | STREAMING CONTROL REQUEST PACKET
============================================================================
|    00 | 0x12 streaming control command
----------------------------------------------------------------------------
|    01 | 0x00 streaming disabled
|       | 0x01 streaming enabled
----------------------------------------------------------------------------
|    02 | 0x00
----------------------------------------------------------------------------
| 03-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | REMOTE CONTROL REQUEST PACKET
============================================================================
|    00 | 0x41 remote control command
----------------------------------------------------------------------------
| 01-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | REMOTE CONTROL REPLY PACKET
============================================================================
|    00 | 0x00 code not received
|       | 0x01 code received
----------------------------------------------------------------------------
|    01 | remote control code
----------------------------------------------------------------------------
| 02-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | GET HARDWARE INFO REQUEST PACKET
============================================================================
|    00 | 0x19 get hardware info command
----------------------------------------------------------------------------
| 01-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | GET HARDWARE INFO REPLY PACKET
============================================================================
|    00 | hardware id
----------------------------------------------------------------------------
| 01-02 | firmware version
----------------------------------------------------------------------------
| 03-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

============================================================================
| 00-63 | SMART CARD READER PACKET
============================================================================
|    00 | 0x34 smart card reader command
----------------------------------------------------------------------------
|    xx |
----------------------------------------------------------------------------
| xx-59 | don't care
----------------------------------------------------------------------------
|    60 | packet sequence number
----------------------------------------------------------------------------
| 61-63 | don't care
----------------------------------------------------------------------------

*/

/*
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *   http://www.agere.com
 *
 * SOFTWARE LICENSE
 *
 * This software is provided subject to the following terms and conditions,
 * which you should read carefully before using the software.  Using this
 * software indicates your acceptance of these terms and conditions.  If you do
 * not agree with these terms and conditions, do not use the software.
 *
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *
 * Redistribution and use in source or binary forms, with or without
 * modifications, are permitted provided that the following conditions are met:
 *
 * . Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following Disclaimer as comments in the code as
 *    well as in the documentation and/or other materials provided with the
 *    distribution.
 *
 * . Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following Disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * . Neither the name of Agere Systems Inc. nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Disclaimer
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
 * USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
 * RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#define DRIVER_NAME "et131x"
#define DRIVER_VERSION "v2.0"

/* EEPROM registers */

/* LBCIF Register Groups (addressed via 32-bit offsets) */
#define LBCIF_DWORD0_GROUP       0xAC
#define LBCIF_DWORD1_GROUP       0xB0

/* LBCIF Registers (addressed via 8-bit offsets) */
#define LBCIF_ADDRESS_REGISTER   0xAC
#define LBCIF_DATA_REGISTER      0xB0
#define LBCIF_CONTROL_REGISTER   0xB1
#define LBCIF_STATUS_REGISTER    0xB2

/* LBCIF Control Register Bits */
#define LBCIF_CONTROL_SEQUENTIAL_READ   0x01
#define LBCIF_CONTROL_PAGE_WRITE        0x02
#define LBCIF_CONTROL_EEPROM_RELOAD     0x08
#define LBCIF_CONTROL_TWO_BYTE_ADDR     0x20
#define LBCIF_CONTROL_I2C_WRITE         0x40
#define LBCIF_CONTROL_LBCIF_ENABLE      0x80

/* LBCIF Status Register Bits */
#define LBCIF_STATUS_PHY_QUEUE_AVAIL    0x01
#define LBCIF_STATUS_I2C_IDLE           0x02
#define LBCIF_STATUS_ACK_ERROR          0x04
#define LBCIF_STATUS_GENERAL_ERROR      0x08
#define LBCIF_STATUS_CHECKSUM_ERROR     0x40
#define LBCIF_STATUS_EEPROM_PRESENT     0x80


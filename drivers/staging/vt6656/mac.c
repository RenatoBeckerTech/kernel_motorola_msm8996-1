/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *
 * File: mac.c
 *
 * Purpose:  MAC routines
 *
 * Author: Tevin Chen
 *
 * Date: May 21, 1996
 *
 * Functions:
 *
 * Revision History:
 */

#include "tmacro.h"
#include "tether.h"
#include "desc.h"
#include "mac.h"
#include "80211hdr.h"
#include "control.h"

//static int          msglevel                =MSG_LEVEL_DEBUG;
static int          msglevel                =MSG_LEVEL_INFO;

/*
 * Description:
 *      Write MAC Multicast Address Mask
 *
 * Parameters:
 *  In:
 *	mc_filter (mac filter)
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void MACvWriteMultiAddr(struct vnt_private *priv, u64 mc_filter)
{
	__le64 le_mc = cpu_to_le64(mc_filter);

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE, MAC_REG_MAR0,
		MESSAGE_REQUEST_MACREG, sizeof(le_mc), (u8 *)&le_mc);
}

/*
 * Description:
 *      Shut Down MAC
 *
 * Parameters:
 *  In:
 *  Out:
 *      none
 *
 *
 */
void MACbShutdown(struct vnt_private *priv)
{
	CONTROLnsRequestOut(priv, MESSAGE_TYPE_MACSHUTDOWN, 0, 0, 0, NULL);
}

void MACvSetBBType(struct vnt_private *priv, u8 type)
{
	u8 data[2];

	data[0] = type;
	data[1] = EnCFG_BBType_MASK;

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE_MASK, MAC_REG_ENCFG0,
		MESSAGE_REQUEST_MACREG,	ARRAY_SIZE(data), data);
}

/*
 * Description:
 *      Disable the Key Entry by MISCFIFO
 *
 * Parameters:
 *  In:
 *      dwIoBase        - Base Address for MAC
 *
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void MACvDisableKeyEntry(struct vnt_private *priv, u8 entry_idx)
{
	CONTROLnsRequestOut(priv, MESSAGE_TYPE_CLRKEYENTRY, 0, 0,
		sizeof(entry_idx), &entry_idx);
}

/*
 * Description:
 *      Set the Key by MISCFIFO
 *
 * Parameters:
 *  In:
 *      dwIoBase        - Base Address for MAC
 *
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void MACvSetKeyEntry(struct vnt_private *pDevice, u16 wKeyCtl, u32 uEntryIdx,
	u32 uKeyIdx, u8 *pbyAddr, u8 *key)
{
	struct vnt_mac_set_key set_key;
	u16 wOffset;

	if (pDevice->byLocalID <= MAC_REVISION_A1)
		if (pDevice->vnt_mgmt.byCSSPK == KEY_CTL_CCMP)
			return;

	wOffset = MISCFIFO_KEYETRY0;
	wOffset += (uEntryIdx * MISCFIFO_KEYENTRYSIZE);

	set_key.u.write.key_ctl = cpu_to_le16(wKeyCtl);
	memcpy(set_key.u.write.addr, pbyAddr, ETH_ALEN);

	/* swap over swap[0] and swap[1] to get correct write order */
	swap(set_key.u.swap[0], set_key.u.swap[1]);

	memcpy(set_key.key, key, WLAN_KEY_LEN_CCMP);

	DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO
		"offset %d key ctl %d set key %24ph\n",
				wOffset, wKeyCtl, (u8 *)&set_key);

	CONTROLnsRequestOut(pDevice, MESSAGE_TYPE_SETKEY, wOffset,
		(u16)uKeyIdx, sizeof(struct vnt_mac_set_key), (u8 *)&set_key);
}

void MACvRegBitsOff(struct vnt_private *priv, u8 reg_ofs, u8 bits)
{
	u8 data[2];

	data[0] = 0;
	data[1] = bits;

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE_MASK,
		reg_ofs, MESSAGE_REQUEST_MACREG, ARRAY_SIZE(data), data);
}

void MACvRegBitsOn(struct vnt_private *priv, u8 reg_ofs, u8 bits)
{
	u8 data[2];

	data[0] = bits;
	data[1] = bits;

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE_MASK,
		reg_ofs, MESSAGE_REQUEST_MACREG, ARRAY_SIZE(data), data);
}

void MACvWriteWord(struct vnt_private *priv, u8 reg_ofs, u16 word)
{
	u8 data[2];

	data[0] = (u8)(word & 0xff);
	data[1] = (u8)(word >> 8);

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE,
		reg_ofs, MESSAGE_REQUEST_MACREG, ARRAY_SIZE(data), data);
}

void MACvWriteBSSIDAddress(struct vnt_private *priv, u8 *addr)
{
	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE, MAC_REG_BSSID0,
		MESSAGE_REQUEST_MACREG, ETH_ALEN, addr);
}

void MACvEnableProtectMD(struct vnt_private *priv)
{
	u8 data[2];

	data[0] = EnCFG_ProtectMd;
	data[1] = EnCFG_ProtectMd;

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE_MASK,
		MAC_REG_ENCFG0, MESSAGE_REQUEST_MACREG, ARRAY_SIZE(data), data);
}

void MACvDisableProtectMD(struct vnt_private *priv)
{
	u8 data[2];

	data[0] = 0;
	data[1] = EnCFG_ProtectMd;

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE_MASK,
		MAC_REG_ENCFG0, MESSAGE_REQUEST_MACREG, ARRAY_SIZE(data), data);
}

void MACvEnableBarkerPreambleMd(struct vnt_private *priv)
{
	u8 data[2];

	data[0] = EnCFG_BarkerPream;
	data[1] = EnCFG_BarkerPream;

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE_MASK,
		MAC_REG_ENCFG2, MESSAGE_REQUEST_MACREG, ARRAY_SIZE(data), data);
}

void MACvDisableBarkerPreambleMd(struct vnt_private *priv)
{
	u8 data[2];

	data[0] = 0;
	data[1] = EnCFG_BarkerPream;

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE_MASK,
		MAC_REG_ENCFG2, MESSAGE_REQUEST_MACREG, ARRAY_SIZE(data), data);
}

void MACvWriteBeaconInterval(struct vnt_private *priv, u16 interval)
{
	u8 data[2];

	data[0] = (u8)(interval & 0xff);
	data[1] = (u8)(interval >> 8);

	CONTROLnsRequestOut(priv, MESSAGE_TYPE_WRITE,
		MAC_REG_BI, MESSAGE_REQUEST_MACREG, ARRAY_SIZE(data), data);
}

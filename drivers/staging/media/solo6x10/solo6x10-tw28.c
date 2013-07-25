/*
 * Copyright (C) 2010-2013 Bluecherry, LLC <http://www.bluecherrydvr.com>
 *
 * Original author:
 * Ben Collins <bcollins@ubuntu.com>
 *
 * Additional work by:
 * John Brooks <john.brooks@bluecherry.net>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/delay.h>

#include "solo6x10.h"
#include "solo6x10-tw28.h"

#define DEFAULT_HDELAY_NTSC		(32 - 8)
#define DEFAULT_HACTIVE_NTSC		(720 + 16)
#define DEFAULT_VDELAY_NTSC		(7 - 2)
#define DEFAULT_VACTIVE_NTSC		(240 + 4)

#define DEFAULT_HDELAY_PAL		(32 + 4)
#define DEFAULT_HACTIVE_PAL		(864-DEFAULT_HDELAY_PAL)
#define DEFAULT_VDELAY_PAL		(6)
#define DEFAULT_VACTIVE_PAL		(312-DEFAULT_VDELAY_PAL)


static const u8 tbl_tw2864_ntsc_template[] = {
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x02, /* 0x00 */
	0x12, 0xf5, 0x0c, 0xd0, 0x00, 0x00, 0x00, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x02, /* 0x10 */
	0x12, 0xf5, 0x0c, 0xd0, 0x00, 0x00, 0x00, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x02, /* 0x20 */
	0x12, 0xf5, 0x0c, 0xd0, 0x00, 0x00, 0x00, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x02, /* 0x30 */
	0x12, 0xf5, 0x0c, 0xd0, 0x00, 0x00, 0x00, 0x7f,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x40 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA3, 0x00,
	0x00, 0x02, 0x00, 0xcc, 0x00, 0x80, 0x44, 0x50, /* 0x80 */
	0x22, 0x01, 0xd8, 0xbc, 0xb8, 0x44, 0x38, 0x00,
	0x00, 0x78, 0x72, 0x3e, 0x14, 0xa5, 0xe4, 0x05, /* 0x90 */
	0x00, 0x28, 0x44, 0x44, 0xa0, 0x88, 0x5a, 0x01,
	0x08, 0x08, 0x08, 0x08, 0x1a, 0x1a, 0x1a, 0x1a, /* 0xa0 */
	0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0x44,
	0x44, 0x0a, 0x00, 0xff, 0xef, 0xef, 0xef, 0xef, /* 0xb0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0 */
	0x00, 0x00, 0x55, 0x00, 0xb1, 0xe4, 0x40, 0x00,
	0x77, 0x77, 0x01, 0x13, 0x57, 0x9b, 0xdf, 0x20, /* 0xd0 */
	0x64, 0xa8, 0xec, 0xc1, 0x0f, 0x11, 0x11, 0x81,
	0x00, 0xe0, 0xbb, 0xbb, 0x00, 0x11, 0x00, 0x00, /* 0xe0 */
	0x11, 0x00, 0x00, 0x11, 0x00, 0x00, 0x11, 0x00,
	0x83, 0xb5, 0x09, 0x78, 0x85, 0x00, 0x01, 0x20, /* 0xf0 */
	0x64, 0x11, 0x40, 0xaf, 0xff, 0x00, 0x00, 0x00,
};

static const u8 tbl_tw2864_pal_template[] = {
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x12, /* 0x00 */
	0x18, 0xf5, 0x0c, 0xd0, 0x00, 0x00, 0x01, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x12, /* 0x10 */
	0x18, 0xf5, 0x0c, 0xd0, 0x00, 0x00, 0x01, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x12, /* 0x20 */
	0x18, 0xf5, 0x0c, 0xd0, 0x00, 0x00, 0x01, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x12, /* 0x30 */
	0x18, 0xf5, 0x0c, 0xd0, 0x00, 0x00, 0x01, 0x7f,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x40 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA3, 0x00,
	0x00, 0x02, 0x00, 0xcc, 0x00, 0x80, 0x44, 0x50, /* 0x80 */
	0x22, 0x01, 0xd8, 0xbc, 0xb8, 0x44, 0x38, 0x00,
	0x00, 0x78, 0x72, 0x3e, 0x14, 0xa5, 0xe4, 0x05, /* 0x90 */
	0x00, 0x28, 0x44, 0x44, 0xa0, 0x90, 0x5a, 0x01,
	0x0a, 0x0a, 0x0a, 0x0a, 0x1a, 0x1a, 0x1a, 0x1a, /* 0xa0 */
	0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0x44,
	0x44, 0x0a, 0x00, 0xff, 0xef, 0xef, 0xef, 0xef, /* 0xb0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0 */
	0x00, 0x00, 0x55, 0x00, 0xb1, 0xe4, 0x40, 0x00,
	0x77, 0x77, 0x01, 0x13, 0x57, 0x9b, 0xdf, 0x20, /* 0xd0 */
	0x64, 0xa8, 0xec, 0xc1, 0x0f, 0x11, 0x11, 0x81,
	0x00, 0xe0, 0xbb, 0xbb, 0x00, 0x11, 0x00, 0x00, /* 0xe0 */
	0x11, 0x00, 0x00, 0x11, 0x00, 0x00, 0x11, 0x00,
	0x83, 0xb5, 0x09, 0x00, 0xa0, 0x00, 0x01, 0x20, /* 0xf0 */
	0x64, 0x11, 0x40, 0xaf, 0xff, 0x00, 0x00, 0x00,
};

static const u8 tbl_tw2865_ntsc_template[] = {
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x02, /* 0x00 */
	0x12, 0xff, 0x09, 0xd0, 0x00, 0x00, 0x00, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x02, /* 0x10 */
	0x12, 0xff, 0x09, 0xd0, 0x00, 0x00, 0x00, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x02, /* 0x20 */
	0x12, 0xff, 0x09, 0xd0, 0x00, 0x00, 0x00, 0x7f,
	0x00, 0xf0, 0x70, 0x48, 0x80, 0x80, 0x00, 0x02, /* 0x30 */
	0x12, 0xff, 0x09, 0xd0, 0x00, 0x00, 0x00, 0x7f,
	0x00, 0x00, 0x90, 0x68, 0x00, 0x38, 0x80, 0x80, /* 0x40 */
	0x80, 0x80, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x45, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x43,
	0x08, 0x00, 0x00, 0x01, 0xf1, 0x03, 0xEF, 0x03, /* 0x70 */
	0xE9, 0x03, 0xD9, 0x15, 0x15, 0xE4, 0xA3, 0x80,
	0x00, 0x02, 0x00, 0xCC, 0x00, 0x80, 0x44, 0x50, /* 0x80 */
	0x22, 0x01, 0xD8, 0xBC, 0xB8, 0x44, 0x38, 0x00,
	0x00, 0x78, 0x44, 0x3D, 0x14, 0xA5, 0xE0, 0x05, /* 0x90 */
	0x00, 0x28, 0x44, 0x44, 0xA0, 0x90, 0x52, 0x13,
	0x08, 0x08, 0x08, 0x08, 0x1A, 0x1A, 0x1B, 0x1A, /* 0xa0 */
	0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x44,
	0x44, 0x4A, 0x00, 0xFF, 0xEF, 0xEF, 0xEF, 0xEF, /* 0xb0 */
	0xFF, 0xE7, 0xE9, 0xE9, 0xEB, 0xFF, 0xD6, 0xD8,
	0xD8, 0xD7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0 */
	0x00, 0x00, 0x55, 0x00, 0xE4, 0x39, 0x00, 0x80,
	0x77, 0x77, 0x03, 0x20, 0x57, 0x9b, 0xdf, 0x31, /* 0xd0 */
	0x64, 0xa8, 0xec, 0xd1, 0x0f, 0x11, 0x11, 0x81,
	0x10, 0xC0, 0xAA, 0xAA, 0x00, 0x11, 0x00, 0x00, /* 0xe0 */
	0x11, 0x00, 0x00, 0x11, 0x00, 0x00, 0x11, 0x00,
	0x83, 0xB5, 0x09, 0x78, 0x85, 0x00, 0x01, 0x20, /* 0xf0 */
	0x64, 0x51, 0x40, 0xaf, 0xFF, 0xF0, 0x00, 0xC0,
};

static const u8 tbl_tw2865_pal_template[] = {
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x12, /* 0x00 */
	0x11, 0xff, 0x01, 0xc3, 0x00, 0x00, 0x01, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x12, /* 0x10 */
	0x11, 0xff, 0x01, 0xc3, 0x00, 0x00, 0x01, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x12, /* 0x20 */
	0x11, 0xff, 0x01, 0xc3, 0x00, 0x00, 0x01, 0x7f,
	0x00, 0xf0, 0x70, 0x30, 0x80, 0x80, 0x00, 0x12, /* 0x30 */
	0x11, 0xff, 0x01, 0xc3, 0x00, 0x00, 0x01, 0x7f,
	0x00, 0x94, 0x90, 0x48, 0x00, 0x38, 0x7F, 0x80, /* 0x40 */
	0x80, 0x80, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x50 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x45, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x60 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x43,
	0x08, 0x00, 0x00, 0x01, 0xf1, 0x03, 0xEF, 0x03, /* 0x70 */
	0xEA, 0x03, 0xD9, 0x15, 0x15, 0xE4, 0xA3, 0x80,
	0x00, 0x02, 0x00, 0xCC, 0x00, 0x80, 0x44, 0x50, /* 0x80 */
	0x22, 0x01, 0xD8, 0xBC, 0xB8, 0x44, 0x38, 0x00,
	0x00, 0x78, 0x44, 0x3D, 0x14, 0xA5, 0xE0, 0x05, /* 0x90 */
	0x00, 0x28, 0x44, 0x44, 0xA0, 0x90, 0x52, 0x13,
	0x08, 0x08, 0x08, 0x08, 0x1A, 0x1A, 0x1A, 0x1A, /* 0xa0 */
	0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x44,
	0x44, 0x4A, 0x00, 0xFF, 0xEF, 0xEF, 0xEF, 0xEF, /* 0xb0 */
	0xFF, 0xE7, 0xE9, 0xE9, 0xE9, 0xFF, 0xD7, 0xD8,
	0xD9, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc0 */
	0x00, 0x00, 0x55, 0x00, 0xE4, 0x39, 0x00, 0x80,
	0x77, 0x77, 0x03, 0x20, 0x57, 0x9b, 0xdf, 0x31, /* 0xd0 */
	0x64, 0xa8, 0xec, 0xd1, 0x0f, 0x11, 0x11, 0x81,
	0x10, 0xC0, 0xAA, 0xAA, 0x00, 0x11, 0x00, 0x00, /* 0xe0 */
	0x11, 0x00, 0x00, 0x11, 0x00, 0x00, 0x11, 0x00,
	0x83, 0xB5, 0x09, 0x00, 0xA0, 0x00, 0x01, 0x20, /* 0xf0 */
	0x64, 0x51, 0x40, 0xaf, 0xFF, 0xF0, 0x00, 0xC0,
};

#define is_tw286x(__solo, __id) (!(__solo->tw2815 & (1 << __id)))

static u8 tw_readbyte(struct solo_dev *solo_dev, int chip_id, u8 tw6x_off,
		      u8 tw_off)
{
	if (is_tw286x(solo_dev, chip_id))
		return solo_i2c_readbyte(solo_dev, SOLO_I2C_TW,
					 TW_CHIP_OFFSET_ADDR(chip_id),
					 tw6x_off);
	else
		return solo_i2c_readbyte(solo_dev, SOLO_I2C_TW,
					 TW_CHIP_OFFSET_ADDR(chip_id),
					 tw_off);
}

static void tw_writebyte(struct solo_dev *solo_dev, int chip_id,
			 u8 tw6x_off, u8 tw_off, u8 val)
{
	if (is_tw286x(solo_dev, chip_id))
		solo_i2c_writebyte(solo_dev, SOLO_I2C_TW,
				   TW_CHIP_OFFSET_ADDR(chip_id),
				   tw6x_off, val);
	else
		solo_i2c_writebyte(solo_dev, SOLO_I2C_TW,
				   TW_CHIP_OFFSET_ADDR(chip_id),
				   tw_off, val);
}

static void tw_write_and_verify(struct solo_dev *solo_dev, u8 addr, u8 off,
				u8 val)
{
	int i;

	for (i = 0; i < 5; i++) {
		u8 rval = solo_i2c_readbyte(solo_dev, SOLO_I2C_TW, addr, off);
		if (rval == val)
			return;

		solo_i2c_writebyte(solo_dev, SOLO_I2C_TW, addr, off, val);
		msleep_interruptible(1);
	}

/*	printk("solo6x10/tw28: Error writing register: %02x->%02x [%02x]\n", */
/*		addr, off, val); */
}

static int tw2865_setup(struct solo_dev *solo_dev, u8 dev_addr)
{
	u8 tbl_tw2865_common[256];
	int i;

	if (solo_dev->video_type == SOLO_VO_FMT_TYPE_PAL)
		memcpy(tbl_tw2865_common, tbl_tw2865_pal_template,
		       sizeof(tbl_tw2865_common));
	else
		memcpy(tbl_tw2865_common, tbl_tw2865_ntsc_template,
		       sizeof(tbl_tw2865_common));

	/* ALINK Mode */
	if (solo_dev->nr_chans == 4) {
		tbl_tw2865_common[0xd2] = 0x01;
		tbl_tw2865_common[0xcf] = 0x00;
	} else if (solo_dev->nr_chans == 8) {
		tbl_tw2865_common[0xd2] = 0x02;
		if (dev_addr == TW_CHIP_OFFSET_ADDR(1))
			tbl_tw2865_common[0xcf] = 0x80;
	} else if (solo_dev->nr_chans == 16) {
		tbl_tw2865_common[0xd2] = 0x03;
		if (dev_addr == TW_CHIP_OFFSET_ADDR(1))
			tbl_tw2865_common[0xcf] = 0x83;
		else if (dev_addr == TW_CHIP_OFFSET_ADDR(2))
			tbl_tw2865_common[0xcf] = 0x83;
		else if (dev_addr == TW_CHIP_OFFSET_ADDR(3))
			tbl_tw2865_common[0xcf] = 0x80;
	}

	for (i = 0; i < 0xff; i++) {
		/* Skip read only registers */
		switch (i) {
		case 0xb8 ... 0xc1:
		case 0xc4 ... 0xc7:
		case 0xfd:
			continue;
		}
		switch (i & ~0x30) {
		case 0x00:
		case 0x0c ... 0x0d:
			continue;
		}

		tw_write_and_verify(solo_dev, dev_addr, i,
				    tbl_tw2865_common[i]);
	}

	return 0;
}

static int tw2864_setup(struct solo_dev *solo_dev, u8 dev_addr)
{
	u8 tbl_tw2864_common[256];
	int i;

	if (solo_dev->video_type == SOLO_VO_FMT_TYPE_PAL)
		memcpy(tbl_tw2864_common, tbl_tw2864_pal_template,
		       sizeof(tbl_tw2864_common));
	else
		memcpy(tbl_tw2864_common, tbl_tw2864_ntsc_template,
		       sizeof(tbl_tw2864_common));

	if (solo_dev->tw2865 == 0) {
		/* IRQ Mode */
		if (solo_dev->nr_chans == 4) {
			tbl_tw2864_common[0xd2] = 0x01;
			tbl_tw2864_common[0xcf] = 0x00;
		} else if (solo_dev->nr_chans == 8) {
			tbl_tw2864_common[0xd2] = 0x02;
			if (dev_addr == TW_CHIP_OFFSET_ADDR(0))
				tbl_tw2864_common[0xcf] = 0x43;
			else if (dev_addr == TW_CHIP_OFFSET_ADDR(1))
				tbl_tw2864_common[0xcf] = 0x40;
		} else if (solo_dev->nr_chans == 16) {
			tbl_tw2864_common[0xd2] = 0x03;
			if (dev_addr == TW_CHIP_OFFSET_ADDR(0))
				tbl_tw2864_common[0xcf] = 0x43;
			else if (dev_addr == TW_CHIP_OFFSET_ADDR(1))
				tbl_tw2864_common[0xcf] = 0x43;
			else if (dev_addr == TW_CHIP_OFFSET_ADDR(2))
				tbl_tw2864_common[0xcf] = 0x43;
			else if (dev_addr == TW_CHIP_OFFSET_ADDR(3))
				tbl_tw2864_common[0xcf] = 0x40;
		}
	} else {
		/* ALINK Mode. Assumes that the first tw28xx is a
		 * 2865 and these are in cascade. */
		for (i = 0; i <= 4; i++)
			tbl_tw2864_common[0x08 | i << 4] = 0x12;

		if (solo_dev->nr_chans == 8) {
			tbl_tw2864_common[0xd2] = 0x02;
			if (dev_addr == TW_CHIP_OFFSET_ADDR(1))
				tbl_tw2864_common[0xcf] = 0x80;
		} else if (solo_dev->nr_chans == 16) {
			tbl_tw2864_common[0xd2] = 0x03;
			if (dev_addr == TW_CHIP_OFFSET_ADDR(1))
				tbl_tw2864_common[0xcf] = 0x83;
			else if (dev_addr == TW_CHIP_OFFSET_ADDR(2))
				tbl_tw2864_common[0xcf] = 0x83;
			else if (dev_addr == TW_CHIP_OFFSET_ADDR(3))
				tbl_tw2864_common[0xcf] = 0x80;
		}
	}

	for (i = 0; i < 0xff; i++) {
		/* Skip read only registers */
		switch (i) {
		case 0xb8 ... 0xc1:
		case 0xfd:
			continue;
		}
		switch (i & ~0x30) {
		case 0x00:
		case 0x0c:
		case 0x0d:
			continue;
		}

		tw_write_and_verify(solo_dev, dev_addr, i,
				    tbl_tw2864_common[i]);
	}

	return 0;
}

static int tw2815_setup(struct solo_dev *solo_dev, u8 dev_addr)
{
	u8 tbl_ntsc_tw2815_common[] = {
		0x00, 0xc8, 0x20, 0xd0, 0x06, 0xf0, 0x08, 0x80,
		0x80, 0x80, 0x80, 0x02, 0x06, 0x00, 0x11,
	};

	u8 tbl_pal_tw2815_common[] = {
		0x00, 0x88, 0x20, 0xd0, 0x05, 0x20, 0x28, 0x80,
		0x80, 0x80, 0x80, 0x82, 0x06, 0x00, 0x11,
	};

	u8 tbl_tw2815_sfr[] = {
		0x00, 0x00, 0x00, 0xc0, 0x45, 0xa0, 0xd0, 0x2f, /* 0x00 */
		0x64, 0x80, 0x80, 0x82, 0x82, 0x00, 0x00, 0x00,
		0x00, 0x0f, 0x05, 0x00, 0x00, 0x80, 0x06, 0x00, /* 0x10 */
		0x00, 0x00, 0x00, 0xff, 0x8f, 0x00, 0x00, 0x00,
		0x88, 0x88, 0xc0, 0x00, 0x20, 0x64, 0xa8, 0xec, /* 0x20 */
		0x31, 0x75, 0xb9, 0xfd, 0x00, 0x00, 0x88, 0x88,
		0x88, 0x11, 0x00, 0x88, 0x88, 0x00,		/* 0x30 */
	};
	u8 *tbl_tw2815_common;
	int i;
	int ch;

	tbl_ntsc_tw2815_common[0x06] = 0;

	/* Horizontal Delay Control */
	tbl_ntsc_tw2815_common[0x02] = DEFAULT_HDELAY_NTSC & 0xff;
	tbl_ntsc_tw2815_common[0x06] |= 0x03 & (DEFAULT_HDELAY_NTSC >> 8);

	/* Horizontal Active Control */
	tbl_ntsc_tw2815_common[0x03] = DEFAULT_HACTIVE_NTSC & 0xff;
	tbl_ntsc_tw2815_common[0x06] |=
		((0x03 & (DEFAULT_HACTIVE_NTSC >> 8)) << 2);

	/* Vertical Delay Control */
	tbl_ntsc_tw2815_common[0x04] = DEFAULT_VDELAY_NTSC & 0xff;
	tbl_ntsc_tw2815_common[0x06] |=
		((0x01 & (DEFAULT_VDELAY_NTSC >> 8)) << 4);

	/* Vertical Active Control */
	tbl_ntsc_tw2815_common[0x05] = DEFAULT_VACTIVE_NTSC & 0xff;
	tbl_ntsc_tw2815_common[0x06] |=
		((0x01 & (DEFAULT_VACTIVE_NTSC >> 8)) << 5);

	tbl_pal_tw2815_common[0x06] = 0;

	/* Horizontal Delay Control */
	tbl_pal_tw2815_common[0x02] = DEFAULT_HDELAY_PAL & 0xff;
	tbl_pal_tw2815_common[0x06] |= 0x03 & (DEFAULT_HDELAY_PAL >> 8);

	/* Horizontal Active Control */
	tbl_pal_tw2815_common[0x03] = DEFAULT_HACTIVE_PAL & 0xff;
	tbl_pal_tw2815_common[0x06] |=
		((0x03 & (DEFAULT_HACTIVE_PAL >> 8)) << 2);

	/* Vertical Delay Control */
	tbl_pal_tw2815_common[0x04] = DEFAULT_VDELAY_PAL & 0xff;
	tbl_pal_tw2815_common[0x06] |=
		((0x01 & (DEFAULT_VDELAY_PAL >> 8)) << 4);

	/* Vertical Active Control */
	tbl_pal_tw2815_common[0x05] = DEFAULT_VACTIVE_PAL & 0xff;
	tbl_pal_tw2815_common[0x06] |=
		((0x01 & (DEFAULT_VACTIVE_PAL >> 8)) << 5);

	tbl_tw2815_common =
	    (solo_dev->video_type == SOLO_VO_FMT_TYPE_NTSC) ?
	     tbl_ntsc_tw2815_common : tbl_pal_tw2815_common;

	/* Dual ITU-R BT.656 format */
	tbl_tw2815_common[0x0d] |= 0x04;

	/* Audio configuration */
	tbl_tw2815_sfr[0x62 - 0x40] &= ~(3 << 6);

	if (solo_dev->nr_chans == 4) {
		tbl_tw2815_sfr[0x63 - 0x40] |= 1;
		tbl_tw2815_sfr[0x62 - 0x40] |= 3 << 6;
	} else if (solo_dev->nr_chans == 8) {
		tbl_tw2815_sfr[0x63 - 0x40] |= 2;
		if (dev_addr == TW_CHIP_OFFSET_ADDR(0))
			tbl_tw2815_sfr[0x62 - 0x40] |= 1 << 6;
		else if (dev_addr == TW_CHIP_OFFSET_ADDR(1))
			tbl_tw2815_sfr[0x62 - 0x40] |= 2 << 6;
	} else if (solo_dev->nr_chans == 16) {
		tbl_tw2815_sfr[0x63 - 0x40] |= 3;
		if (dev_addr == TW_CHIP_OFFSET_ADDR(0))
			tbl_tw2815_sfr[0x62 - 0x40] |= 1 << 6;
		else if (dev_addr == TW_CHIP_OFFSET_ADDR(1))
			tbl_tw2815_sfr[0x62 - 0x40] |= 0 << 6;
		else if (dev_addr == TW_CHIP_OFFSET_ADDR(2))
			tbl_tw2815_sfr[0x62 - 0x40] |= 0 << 6;
		else if (dev_addr == TW_CHIP_OFFSET_ADDR(3))
			tbl_tw2815_sfr[0x62 - 0x40] |= 2 << 6;
	}

	/* Output mode of R_ADATM pin (0 mixing, 1 record) */
	/* tbl_tw2815_sfr[0x63 - 0x40] |= 0 << 2; */

	/* 8KHz, used to be 16KHz, but changed for remote client compat */
	tbl_tw2815_sfr[0x62 - 0x40] |= 0 << 2;
	tbl_tw2815_sfr[0x6c - 0x40] |= 0 << 2;

	/* Playback of right channel */
	tbl_tw2815_sfr[0x6c - 0x40] |= 1 << 5;

	/* Reserved value (XXX ??) */
	tbl_tw2815_sfr[0x5c - 0x40] |= 1 << 5;

	/* Analog output gain and mix ratio playback on full */
	tbl_tw2815_sfr[0x70 - 0x40] |= 0xff;
	/* Select playback audio and mute all except */
	tbl_tw2815_sfr[0x71 - 0x40] |= 0x10;
	tbl_tw2815_sfr[0x6d - 0x40] |= 0x0f;

	/* End of audio configuration */

	for (ch = 0; ch < 4; ch++) {
		tbl_tw2815_common[0x0d] &= ~3;
		switch (ch) {
		case 0:
			tbl_tw2815_common[0x0d] |= 0x21;
			break;
		case 1:
			tbl_tw2815_common[0x0d] |= 0x20;
			break;
		case 2:
			tbl_tw2815_common[0x0d] |= 0x23;
			break;
		case 3:
			tbl_tw2815_common[0x0d] |= 0x22;
			break;
		}

		for (i = 0; i < 0x0f; i++) {
			if (i == 0x00)
				continue;	/* read-only */
			solo_i2c_writebyte(solo_dev, SOLO_I2C_TW,
					   dev_addr, (ch * 0x10) + i,
					   tbl_tw2815_common[i]);
		}
	}

	for (i = 0x40; i < 0x76; i++) {
		/* Skip read-only and nop registers */
		if (i == 0x40 || i == 0x59 || i == 0x5a ||
		    i == 0x5d || i == 0x5e || i == 0x5f)
			continue;

		solo_i2c_writebyte(solo_dev, SOLO_I2C_TW, dev_addr, i,
				       tbl_tw2815_sfr[i - 0x40]);
	}

	return 0;
}

#define FIRST_ACTIVE_LINE	0x0008
#define LAST_ACTIVE_LINE	0x0102

static void saa712x_write_regs(struct solo_dev *dev, const uint8_t *vals,
		int start, int n)
{
	for (;start < n; start++, vals++) {
		/* Skip read-only registers */
		switch (start) {
		/* case 0x00 ... 0x25: */
		case 0x2e ... 0x37:
		case 0x60:
		case 0x7d:
			continue;
		}
		solo_i2c_writebyte(dev, SOLO_I2C_SAA, 0x46, start, *vals);
	}
}

#define SAA712x_reg7c (0x80 | ((LAST_ACTIVE_LINE & 0x100) >> 2) \
		| ((FIRST_ACTIVE_LINE & 0x100) >> 4))

static void saa712x_setup(struct solo_dev *dev)
{
	const int reg_start = 0x26;
	const uint8_t saa7128_regs_ntsc[] = {
	/* :0x26 */
		0x0d, 0x00,
	/* :0x28 */
		0x59, 0x1d, 0x75, 0x3f, 0x06, 0x3f,
	/* :0x2e XXX: read-only */
		0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* :0x38 */
		0x1a, 0x1a, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* :0x40 */
		0x00, 0x00, 0x00, 0x68, 0x10, 0x97, 0x4c, 0x18,
		0x9b, 0x93, 0x9f, 0xff, 0x7c, 0x34, 0x3f, 0x3f,
	/* :0x50 */
		0x3f, 0x83, 0x83, 0x80, 0x0d, 0x0f, 0xc3, 0x06,
		0x02, 0x80, 0x71, 0x77, 0xa7, 0x67, 0x66, 0x2e,
	/* :0x60 */
		0x7b, 0x11, 0x4f, 0x1f, 0x7c, 0xf0, 0x21, 0x77,
		0x41, 0x88, 0x41, 0x52, 0xed, 0x10, 0x10, 0x00,
	/* :0x70 */
		0x41, 0xc3, 0x00, 0x3e, 0xb8, 0x02, 0x00, 0x00,
		0x00, 0x00, FIRST_ACTIVE_LINE, LAST_ACTIVE_LINE & 0xff,
		SAA712x_reg7c, 0x00, 0xff, 0xff,
	}, saa7128_regs_pal[] = {
	/* :0x26 */
		0x0d, 0x00,
	/* :0x28 */
		0xe1, 0x1d, 0x75, 0x3f, 0x06, 0x3f,
	/* :0x2e XXX: read-only */
		0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* :0x38 */
		0x1a, 0x1a, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* :0x40 */
		0x00, 0x00, 0x00, 0x68, 0x10, 0x97, 0x4c, 0x18,
		0x9b, 0x93, 0x9f, 0xff, 0x7c, 0x34, 0x3f, 0x3f,
	/* :0x50 */
		0x3f, 0x83, 0x83, 0x80, 0x0d, 0x0f, 0xc3, 0x06,
		0x02, 0x80, 0x0f, 0x77, 0xa7, 0x67, 0x66, 0x2e,
	/* :0x60 */
		0x7b, 0x02, 0x35, 0xcb, 0x8a, 0x09, 0x2a, 0x77,
		0x41, 0x88, 0x41, 0x52, 0xf1, 0x10, 0x20, 0x00,
	/* :0x70 */
		0x41, 0xc3, 0x00, 0x3e, 0xb8, 0x02, 0x00, 0x00,
		0x00, 0x00, 0x12, 0x30,
		SAA712x_reg7c | 0x40, 0x00, 0xff, 0xff,
	};

	if (dev->video_type == SOLO_VO_FMT_TYPE_PAL)
		saa712x_write_regs(dev, saa7128_regs_pal, reg_start,
				sizeof(saa7128_regs_pal));
	else
		saa712x_write_regs(dev, saa7128_regs_ntsc, reg_start,
				sizeof(saa7128_regs_ntsc));
}

int solo_tw28_init(struct solo_dev *solo_dev)
{
	int i;
	u8 value;

	solo_dev->tw28_cnt = 0;

	/* Detect techwell chip type(s) */
	for (i = 0; i < solo_dev->nr_chans / 4; i++) {
		value = solo_i2c_readbyte(solo_dev, SOLO_I2C_TW,
					  TW_CHIP_OFFSET_ADDR(i), 0xFF);

		switch (value >> 3) {
		case 0x18:
			solo_dev->tw2865 |= 1 << i;
			solo_dev->tw28_cnt++;
			break;
		case 0x0c:
			solo_dev->tw2864 |= 1 << i;
			solo_dev->tw28_cnt++;
			break;
		default:
			value = solo_i2c_readbyte(solo_dev, SOLO_I2C_TW,
						  TW_CHIP_OFFSET_ADDR(i),
						  0x59);
			if ((value >> 3) == 0x04) {
				solo_dev->tw2815 |= 1 << i;
				solo_dev->tw28_cnt++;
			}
		}
	}

	if (solo_dev->tw28_cnt != (solo_dev->nr_chans >> 2)) {
		dev_err(&solo_dev->pdev->dev,
			"Could not initialize any techwell chips\n");
		return -EINVAL;
	}

	saa712x_setup(solo_dev);

	for (i = 0; i < solo_dev->tw28_cnt; i++) {
		if ((solo_dev->tw2865 & (1 << i)))
			tw2865_setup(solo_dev, TW_CHIP_OFFSET_ADDR(i));
		else if ((solo_dev->tw2864 & (1 << i)))
			tw2864_setup(solo_dev, TW_CHIP_OFFSET_ADDR(i));
		else
			tw2815_setup(solo_dev, TW_CHIP_OFFSET_ADDR(i));
	}

	return 0;
}

/*
 * We accessed the video status signal in the Techwell chip through
 * iic/i2c because the video status reported by register REG_VI_STATUS1
 * (address 0x012C) of the SOLO6010 chip doesn't give the correct video
 * status signal values.
 */
int tw28_get_video_status(struct solo_dev *solo_dev, u8 ch)
{
	u8 val, chip_num;

	/* Get the right chip and on-chip channel */
	chip_num = ch / 4;
	ch %= 4;

	val = tw_readbyte(solo_dev, chip_num, TW286x_AV_STAT_ADDR,
			  TW_AV_STAT_ADDR) & 0x0f;

	return val & (1 << ch) ? 1 : 0;
}

#if 0
/* Status of audio from up to 4 techwell chips are combined into 1 variable.
 * See techwell datasheet for details. */
u16 tw28_get_audio_status(struct solo_dev *solo_dev)
{
	u8 val;
	u16 status = 0;
	int i;

	for (i = 0; i < solo_dev->tw28_cnt; i++) {
		val = (tw_readbyte(solo_dev, i, TW286x_AV_STAT_ADDR,
				   TW_AV_STAT_ADDR) & 0xf0) >> 4;
		status |= val << (i * 4);
	}

	return status;
}
#endif

bool tw28_has_sharpness(struct solo_dev *solo_dev, u8 ch)
{
	return is_tw286x(solo_dev, ch / 4);
}

int tw28_set_ctrl_val(struct solo_dev *solo_dev, u32 ctrl, u8 ch,
		      s32 val)
{
	char sval;
	u8 chip_num;

	/* Get the right chip and on-chip channel */
	chip_num = ch / 4;
	ch %= 4;

	if (val > 255 || val < 0)
		return -ERANGE;

	switch (ctrl) {
	case V4L2_CID_SHARPNESS:
		/* Only 286x has sharpness */
		if (is_tw286x(solo_dev, chip_num)) {
			u8 v = solo_i2c_readbyte(solo_dev, SOLO_I2C_TW,
						 TW_CHIP_OFFSET_ADDR(chip_num),
						 TW286x_SHARPNESS(chip_num));
			v &= 0xf0;
			v |= val;
			solo_i2c_writebyte(solo_dev, SOLO_I2C_TW,
					   TW_CHIP_OFFSET_ADDR(chip_num),
					   TW286x_SHARPNESS(chip_num), v);
		} else {
			return -EINVAL;
		}
		break;

	case V4L2_CID_HUE:
		if (is_tw286x(solo_dev, chip_num))
			sval = val - 128;
		else
			sval = (char)val;
		tw_writebyte(solo_dev, chip_num, TW286x_HUE_ADDR(ch),
			     TW_HUE_ADDR(ch), sval);

		break;

	case V4L2_CID_SATURATION:
		/* 286x chips have a U and V component for saturation */
		if (is_tw286x(solo_dev, chip_num)) {
			solo_i2c_writebyte(solo_dev, SOLO_I2C_TW,
					   TW_CHIP_OFFSET_ADDR(chip_num),
					   TW286x_SATURATIONU_ADDR(ch), val);
		}
		tw_writebyte(solo_dev, chip_num, TW286x_SATURATIONV_ADDR(ch),
			     TW_SATURATION_ADDR(ch), val);

		break;

	case V4L2_CID_CONTRAST:
		tw_writebyte(solo_dev, chip_num, TW286x_CONTRAST_ADDR(ch),
			     TW_CONTRAST_ADDR(ch), val);
		break;

	case V4L2_CID_BRIGHTNESS:
		if (is_tw286x(solo_dev, chip_num))
			sval = val - 128;
		else
			sval = (char)val;
		tw_writebyte(solo_dev, chip_num, TW286x_BRIGHTNESS_ADDR(ch),
			     TW_BRIGHTNESS_ADDR(ch), sval);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int tw28_get_ctrl_val(struct solo_dev *solo_dev, u32 ctrl, u8 ch,
		      s32 *val)
{
	u8 rval, chip_num;

	/* Get the right chip and on-chip channel */
	chip_num = ch / 4;
	ch %= 4;

	switch (ctrl) {
	case V4L2_CID_SHARPNESS:
		/* Only 286x has sharpness */
		if (is_tw286x(solo_dev, chip_num)) {
			rval = solo_i2c_readbyte(solo_dev, SOLO_I2C_TW,
						 TW_CHIP_OFFSET_ADDR(chip_num),
						 TW286x_SHARPNESS(chip_num));
			*val = rval & 0x0f;
		} else
			*val = 0;
		break;
	case V4L2_CID_HUE:
		rval = tw_readbyte(solo_dev, chip_num, TW286x_HUE_ADDR(ch),
				   TW_HUE_ADDR(ch));
		if (is_tw286x(solo_dev, chip_num))
			*val = (s32)((char)rval) + 128;
		else
			*val = rval;
		break;
	case V4L2_CID_SATURATION:
		*val = tw_readbyte(solo_dev, chip_num,
				   TW286x_SATURATIONU_ADDR(ch),
				   TW_SATURATION_ADDR(ch));
		break;
	case V4L2_CID_CONTRAST:
		*val = tw_readbyte(solo_dev, chip_num,
				   TW286x_CONTRAST_ADDR(ch),
				   TW_CONTRAST_ADDR(ch));
		break;
	case V4L2_CID_BRIGHTNESS:
		rval = tw_readbyte(solo_dev, chip_num,
				   TW286x_BRIGHTNESS_ADDR(ch),
				   TW_BRIGHTNESS_ADDR(ch));
		if (is_tw286x(solo_dev, chip_num))
			*val = (s32)((char)rval) + 128;
		else
			*val = rval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#if 0
/*
 * For audio output volume, the output channel is only 1. In this case we
 * don't need to offset TW_CHIP_OFFSET_ADDR. The TW_CHIP_OFFSET_ADDR used
 * is the base address of the techwell chip.
 */
void tw2815_Set_AudioOutVol(struct solo_dev *solo_dev, unsigned int u_val)
{
	unsigned int val;
	unsigned int chip_num;

	chip_num = (solo_dev->nr_chans - 1) / 4;

	val = tw_readbyte(solo_dev, chip_num, TW286x_AUDIO_OUTPUT_VOL_ADDR,
			  TW_AUDIO_OUTPUT_VOL_ADDR);

	u_val = (val & 0x0f) | (u_val << 4);

	tw_writebyte(solo_dev, chip_num, TW286x_AUDIO_OUTPUT_VOL_ADDR,
		     TW_AUDIO_OUTPUT_VOL_ADDR, u_val);
}
#endif

u8 tw28_get_audio_gain(struct solo_dev *solo_dev, u8 ch)
{
	u8 val;
	u8 chip_num;

	/* Get the right chip and on-chip channel */
	chip_num = ch / 4;
	ch %= 4;

	val = tw_readbyte(solo_dev, chip_num,
			  TW286x_AUDIO_INPUT_GAIN_ADDR(ch),
			  TW_AUDIO_INPUT_GAIN_ADDR(ch));

	return (ch % 2) ? (val >> 4) : (val & 0x0f);
}

void tw28_set_audio_gain(struct solo_dev *solo_dev, u8 ch, u8 val)
{
	u8 old_val;
	u8 chip_num;

	/* Get the right chip and on-chip channel */
	chip_num = ch / 4;
	ch %= 4;

	old_val = tw_readbyte(solo_dev, chip_num,
			      TW286x_AUDIO_INPUT_GAIN_ADDR(ch),
			      TW_AUDIO_INPUT_GAIN_ADDR(ch));

	val = (old_val & ((ch % 2) ? 0x0f : 0xf0)) |
		((ch % 2) ? (val << 4) : val);

	tw_writebyte(solo_dev, chip_num, TW286x_AUDIO_INPUT_GAIN_ADDR(ch),
		     TW_AUDIO_INPUT_GAIN_ADDR(ch), val);
}

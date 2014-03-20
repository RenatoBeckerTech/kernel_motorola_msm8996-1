/*
 * st_spi_fsm.c	- ST Fast Sequence Mode (FSM) Serial Flash Controller
 *
 * Author: Angus Clark <angus.clark@st.com>
 *
 * Copyright (C) 2010-2014 STicroelectronics Limited
 *
 * JEDEC probe based on drivers/mtd/devices/m25p80.c
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/mtd/mtd.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>

#include "serial_flash_cmds.h"

/*
 * FSM SPI Controller Registers
 */
#define SPI_CLOCKDIV			0x0010
#define SPI_MODESELECT			0x0018
#define SPI_CONFIGDATA			0x0020
#define SPI_STA_MODE_CHANGE		0x0028
#define SPI_FAST_SEQ_TRANSFER_SIZE	0x0100
#define SPI_FAST_SEQ_ADD1		0x0104
#define SPI_FAST_SEQ_ADD2		0x0108
#define SPI_FAST_SEQ_ADD_CFG		0x010c
#define SPI_FAST_SEQ_OPC1		0x0110
#define SPI_FAST_SEQ_OPC2		0x0114
#define SPI_FAST_SEQ_OPC3		0x0118
#define SPI_FAST_SEQ_OPC4		0x011c
#define SPI_FAST_SEQ_OPC5		0x0120
#define SPI_MODE_BITS			0x0124
#define SPI_DUMMY_BITS			0x0128
#define SPI_FAST_SEQ_FLASH_STA_DATA	0x012c
#define SPI_FAST_SEQ_1			0x0130
#define SPI_FAST_SEQ_2			0x0134
#define SPI_FAST_SEQ_3			0x0138
#define SPI_FAST_SEQ_4			0x013c
#define SPI_FAST_SEQ_CFG		0x0140
#define SPI_FAST_SEQ_STA		0x0144
#define SPI_QUAD_BOOT_SEQ_INIT_1	0x0148
#define SPI_QUAD_BOOT_SEQ_INIT_2	0x014c
#define SPI_QUAD_BOOT_READ_SEQ_1	0x0150
#define SPI_QUAD_BOOT_READ_SEQ_2	0x0154
#define SPI_PROGRAM_ERASE_TIME		0x0158
#define SPI_MULT_PAGE_REPEAT_SEQ_1	0x015c
#define SPI_MULT_PAGE_REPEAT_SEQ_2	0x0160
#define SPI_STATUS_WR_TIME_REG		0x0164
#define SPI_FAST_SEQ_DATA_REG		0x0300

/*
 * Register: SPI_MODESELECT
 */
#define SPI_MODESELECT_CONTIG		0x01
#define SPI_MODESELECT_FASTREAD		0x02
#define SPI_MODESELECT_DUALIO		0x04
#define SPI_MODESELECT_FSM		0x08
#define SPI_MODESELECT_QUADBOOT		0x10

/*
 * Register: SPI_CONFIGDATA
 */
#define SPI_CFG_DEVICE_ST		0x1
#define SPI_CFG_DEVICE_ATMEL		0x4
#define SPI_CFG_MIN_CS_HIGH(x)		(((x) & 0xfff) << 4)
#define SPI_CFG_CS_SETUPHOLD(x)		(((x) & 0xff) << 16)
#define SPI_CFG_DATA_HOLD(x)		(((x) & 0xff) << 24)

#define SPI_CFG_DEFAULT_MIN_CS_HIGH    SPI_CFG_MIN_CS_HIGH(0x0AA)
#define SPI_CFG_DEFAULT_CS_SETUPHOLD   SPI_CFG_CS_SETUPHOLD(0xA0)
#define SPI_CFG_DEFAULT_DATA_HOLD      SPI_CFG_DATA_HOLD(0x00)

/*
 * Register: SPI_FAST_SEQ_TRANSFER_SIZE
 */
#define TRANSFER_SIZE(x)		((x) * 8)

/*
 * Register: SPI_FAST_SEQ_ADD_CFG
 */
#define ADR_CFG_CYCLES_ADD1(x)		((x) << 0)
#define ADR_CFG_PADS_1_ADD1		(0x0 << 6)
#define ADR_CFG_PADS_2_ADD1		(0x1 << 6)
#define ADR_CFG_PADS_4_ADD1		(0x3 << 6)
#define ADR_CFG_CSDEASSERT_ADD1		(1   << 8)
#define ADR_CFG_CYCLES_ADD2(x)		((x) << (0+16))
#define ADR_CFG_PADS_1_ADD2		(0x0 << (6+16))
#define ADR_CFG_PADS_2_ADD2		(0x1 << (6+16))
#define ADR_CFG_PADS_4_ADD2		(0x3 << (6+16))
#define ADR_CFG_CSDEASSERT_ADD2		(1   << (8+16))

/*
 * Register: SPI_FAST_SEQ_n
 */
#define SEQ_OPC_OPCODE(x)		((x) << 0)
#define SEQ_OPC_CYCLES(x)		((x) << 8)
#define SEQ_OPC_PADS_1			(0x0 << 14)
#define SEQ_OPC_PADS_2			(0x1 << 14)
#define SEQ_OPC_PADS_4			(0x3 << 14)
#define SEQ_OPC_CSDEASSERT		(1   << 16)

/*
 * Register: SPI_FAST_SEQ_CFG
 */
#define SEQ_CFG_STARTSEQ		(1 << 0)
#define SEQ_CFG_SWRESET			(1 << 5)
#define SEQ_CFG_CSDEASSERT		(1 << 6)
#define SEQ_CFG_READNOTWRITE		(1 << 7)
#define SEQ_CFG_ERASE			(1 << 8)
#define SEQ_CFG_PADS_1			(0x0 << 16)
#define SEQ_CFG_PADS_2			(0x1 << 16)
#define SEQ_CFG_PADS_4			(0x3 << 16)

/*
 * Register: SPI_MODE_BITS
 */
#define MODE_DATA(x)			(x & 0xff)
#define MODE_CYCLES(x)			((x & 0x3f) << 16)
#define MODE_PADS_1			(0x0 << 22)
#define MODE_PADS_2			(0x1 << 22)
#define MODE_PADS_4			(0x3 << 22)
#define DUMMY_CSDEASSERT		(1   << 24)

/*
 * Register: SPI_DUMMY_BITS
 */
#define DUMMY_CYCLES(x)			((x & 0x3f) << 16)
#define DUMMY_PADS_1			(0x0 << 22)
#define DUMMY_PADS_2			(0x1 << 22)
#define DUMMY_PADS_4			(0x3 << 22)
#define DUMMY_CSDEASSERT		(1   << 24)

/*
 * Register: SPI_FAST_SEQ_FLASH_STA_DATA
 */
#define STA_DATA_BYTE1(x)		((x & 0xff) << 0)
#define STA_DATA_BYTE2(x)		((x & 0xff) << 8)
#define STA_PADS_1			(0x0 << 16)
#define STA_PADS_2			(0x1 << 16)
#define STA_PADS_4			(0x3 << 16)
#define STA_CSDEASSERT			(0x1 << 20)
#define STA_RDNOTWR			(0x1 << 21)

/*
 * FSM SPI Instruction Opcodes
 */
#define STFSM_OPC_CMD			0x1
#define STFSM_OPC_ADD			0x2
#define STFSM_OPC_STA			0x3
#define STFSM_OPC_MODE			0x4
#define STFSM_OPC_DUMMY		0x5
#define STFSM_OPC_DATA			0x6
#define STFSM_OPC_WAIT			0x7
#define STFSM_OPC_JUMP			0x8
#define STFSM_OPC_GOTO			0x9
#define STFSM_OPC_STOP			0xF

/*
 * FSM SPI Instructions (== opcode + operand).
 */
#define STFSM_INSTR(cmd, op)		((cmd) | ((op) << 4))

#define STFSM_INST_CMD1			STFSM_INSTR(STFSM_OPC_CMD,	1)
#define STFSM_INST_CMD2			STFSM_INSTR(STFSM_OPC_CMD,	2)
#define STFSM_INST_CMD3			STFSM_INSTR(STFSM_OPC_CMD,	3)
#define STFSM_INST_CMD4			STFSM_INSTR(STFSM_OPC_CMD,	4)
#define STFSM_INST_CMD5			STFSM_INSTR(STFSM_OPC_CMD,	5)
#define STFSM_INST_ADD1			STFSM_INSTR(STFSM_OPC_ADD,	1)
#define STFSM_INST_ADD2			STFSM_INSTR(STFSM_OPC_ADD,	2)

#define STFSM_INST_DATA_WRITE		STFSM_INSTR(STFSM_OPC_DATA,	1)
#define STFSM_INST_DATA_READ		STFSM_INSTR(STFSM_OPC_DATA,	2)

#define STFSM_INST_STA_RD1		STFSM_INSTR(STFSM_OPC_STA,	0x1)
#define STFSM_INST_STA_WR1		STFSM_INSTR(STFSM_OPC_STA,	0x1)
#define STFSM_INST_STA_RD2		STFSM_INSTR(STFSM_OPC_STA,	0x2)
#define STFSM_INST_STA_WR1_2		STFSM_INSTR(STFSM_OPC_STA,	0x3)

#define STFSM_INST_MODE			STFSM_INSTR(STFSM_OPC_MODE,	0)
#define STFSM_INST_DUMMY		STFSM_INSTR(STFSM_OPC_DUMMY,	0)
#define STFSM_INST_WAIT			STFSM_INSTR(STFSM_OPC_WAIT,	0)
#define STFSM_INST_STOP			STFSM_INSTR(STFSM_OPC_STOP,	0)

#define STFSM_DEFAULT_EMI_FREQ 100000000UL                        /* 100 MHz */
#define STFSM_DEFAULT_WR_TIME  (STFSM_DEFAULT_EMI_FREQ * (15/1000)) /* 15ms */

#define STFSM_FLASH_SAFE_FREQ  10000000UL                         /* 10 MHz */

#define STFSM_MAX_WAIT_SEQ_MS  1000     /* FSM execution time */

struct stfsm {
	struct device		*dev;
	void __iomem		*base;
	struct resource		*region;
	struct mtd_info		mtd;
	struct mutex		lock;
	struct flash_info       *info;

	uint32_t                fifo_dir_delay;
	bool                    booted_from_spi;
	bool                    reset_signal;
	bool                    reset_por;
};

struct stfsm_seq {
	uint32_t data_size;
	uint32_t addr1;
	uint32_t addr2;
	uint32_t addr_cfg;
	uint32_t seq_opc[5];
	uint32_t mode;
	uint32_t dummy;
	uint32_t status;
	uint8_t  seq[16];
	uint32_t seq_cfg;
} __packed __aligned(4);

/* Parameters to configure a READ or WRITE FSM sequence */
struct seq_rw_config {
	uint32_t        flags;          /* flags to support config */
	uint8_t         cmd;            /* FLASH command */
	int             write;          /* Write Sequence */
	uint8_t         addr_pads;      /* No. of addr pads (MODE & DUMMY) */
	uint8_t         data_pads;      /* No. of data pads */
	uint8_t         mode_data;      /* MODE data */
	uint8_t         mode_cycles;    /* No. of MODE cycles */
	uint8_t         dummy_cycles;   /* No. of DUMMY cycles */
};

/* SPI Flash Device Table */
struct flash_info {
	char            *name;
	/*
	 * JEDEC id zero means "no ID" (most older chips); otherwise it has
	 * a high byte of zero plus three data bytes: the manufacturer id,
	 * then a two byte device id.
	 */
	u32             jedec_id;
	u16             ext_id;
	/*
	 * The size listed here is what works with FLASH_CMD_SE, which isn't
	 * necessarily called a "sector" by the vendor.
	 */
	unsigned        sector_size;
	u16             n_sectors;
	u32             flags;
	/*
	 * Note, where FAST_READ is supported, freq_max specifies the
	 * FAST_READ frequency, not the READ frequency.
	 */
	u32             max_freq;
	int             (*config)(struct stfsm *);
};

static struct flash_info flash_types[] = {
	/*
	 * ST Microelectronics/Numonyx --
	 * (newer production versions may have feature updates
	 * (eg faster operating frequency)
	 */
#define M25P_FLAG (FLASH_FLAG_READ_WRITE | FLASH_FLAG_READ_FAST)
	{ "m25p40",  0x202013, 0,  64 * 1024,   8, M25P_FLAG, 25, NULL },
	{ "m25p80",  0x202014, 0,  64 * 1024,  16, M25P_FLAG, 25, NULL },
	{ "m25p16",  0x202015, 0,  64 * 1024,  32, M25P_FLAG, 25, NULL },
	{ "m25p32",  0x202016, 0,  64 * 1024,  64, M25P_FLAG, 50, NULL },
	{ "m25p64",  0x202017, 0,  64 * 1024, 128, M25P_FLAG, 50, NULL },
	{ "m25p128", 0x202018, 0, 256 * 1024,  64, M25P_FLAG, 50, NULL },

#define M25PX_FLAG (FLASH_FLAG_READ_WRITE      |	\
		    FLASH_FLAG_READ_FAST        |	\
		    FLASH_FLAG_READ_1_1_2       |	\
		    FLASH_FLAG_WRITE_1_1_2)
	{ "m25px32", 0x207116, 0,  64 * 1024,  64, M25PX_FLAG, 75, NULL },
	{ "m25px64", 0x207117, 0,  64 * 1024, 128, M25PX_FLAG, 75, NULL },

#define MX25_FLAG (FLASH_FLAG_READ_WRITE       |	\
		   FLASH_FLAG_READ_FAST         |	\
		   FLASH_FLAG_READ_1_1_2        |	\
		   FLASH_FLAG_READ_1_2_2        |	\
		   FLASH_FLAG_READ_1_1_4        |	\
		   FLASH_FLAG_READ_1_4_4        |	\
		   FLASH_FLAG_SE_4K             |	\
		   FLASH_FLAG_SE_32K)
	{ "mx25l25635e", 0xc22019, 0, 64*1024, 512,
	  (MX25_FLAG | FLASH_FLAG_32BIT_ADDR | FLASH_FLAG_RESET), 70, NULL }

#define N25Q_FLAG (FLASH_FLAG_READ_WRITE       |	\
		   FLASH_FLAG_READ_FAST         |	\
		   FLASH_FLAG_READ_1_1_2        |	\
		   FLASH_FLAG_READ_1_2_2        |	\
		   FLASH_FLAG_READ_1_1_4        |	\
		   FLASH_FLAG_READ_1_4_4        |	\
		   FLASH_FLAG_WRITE_1_1_2       |	\
		   FLASH_FLAG_WRITE_1_2_2       |	\
		   FLASH_FLAG_WRITE_1_1_4       |	\
		   FLASH_FLAG_WRITE_1_4_4)
	{ "n25q128", 0x20ba18, 0, 64 * 1024,  256, N25Q_FLAG, 108, NULL },
	{ "n25q256", 0x20ba19, 0, 64 * 1024,  512,
	  N25Q_FLAG | FLASH_FLAG_32BIT_ADDR, 108, NULL },

	/*
	 * Spansion S25FLxxxP
	 *     - 256KiB and 64KiB sector variants (identified by ext. JEDEC)
	 */
#define S25FLXXXP_FLAG (FLASH_FLAG_READ_WRITE  |	\
			FLASH_FLAG_READ_1_1_2   |	\
			FLASH_FLAG_READ_1_2_2   |	\
			FLASH_FLAG_READ_1_1_4   |	\
			FLASH_FLAG_READ_1_4_4   |	\
			FLASH_FLAG_WRITE_1_1_4  |	\
			FLASH_FLAG_READ_FAST)
	{ "s25fl129p0", 0x012018, 0x4d00, 256 * 1024,  64, S25FLXXXP_FLAG, 80,
	  NULL },
	{ "s25fl129p1", 0x012018, 0x4d01,  64 * 1024, 256, S25FLXXXP_FLAG, 80,
	  NULL },

	/*
	 * Spansion S25FLxxxS
	 *     - 256KiB and 64KiB sector variants (identified by ext. JEDEC)
	 *     - RESET# signal supported by die but not bristled out on all
	 *       package types.  The package type is a function of board design,
	 *       so this information is captured in the board's flags.
	 *     - Supports 'DYB' sector protection. Depending on variant, sectors
	 *       may default to locked state on power-on.
	 */
#define S25FLXXXS_FLAG (S25FLXXXP_FLAG         |	\
			FLASH_FLAG_RESET        |	\
			FLASH_FLAG_DYB_LOCKING)
	{ "s25fl128s0", 0x012018, 0x0300,  256 * 1024, 64, S25FLXXXS_FLAG, 80,
	  NULL },
	{ "s25fl128s1", 0x012018, 0x0301,  64 * 1024, 256, S25FLXXXS_FLAG, 80,
	  NULL },
	{ "s25fl256s0", 0x010219, 0x4d00, 256 * 1024, 128,
	  S25FLXXXS_FLAG | FLASH_FLAG_32BIT_ADDR, 80, NULL },
	{ "s25fl256s1", 0x010219, 0x4d01,  64 * 1024, 512,
	  S25FLXXXS_FLAG | FLASH_FLAG_32BIT_ADDR, 80, NULL },

	/* Winbond -- w25x "blocks" are 64K, "sectors" are 4KiB */
#define W25X_FLAG (FLASH_FLAG_READ_WRITE       |	\
		   FLASH_FLAG_READ_FAST         |	\
		   FLASH_FLAG_READ_1_1_2        |	\
		   FLASH_FLAG_WRITE_1_1_2)
	{ "w25x40",  0xef3013, 0,  64 * 1024,   8, W25X_FLAG, 75, NULL },
	{ "w25x80",  0xef3014, 0,  64 * 1024,  16, W25X_FLAG, 75, NULL },
	{ "w25x16",  0xef3015, 0,  64 * 1024,  32, W25X_FLAG, 75, NULL },
	{ "w25x32",  0xef3016, 0,  64 * 1024,  64, W25X_FLAG, 75, NULL },
	{ "w25x64",  0xef3017, 0,  64 * 1024, 128, W25X_FLAG, 75, NULL },

	/* Winbond -- w25q "blocks" are 64K, "sectors" are 4KiB */
#define W25Q_FLAG (FLASH_FLAG_READ_WRITE       |	\
		   FLASH_FLAG_READ_FAST         |	\
		   FLASH_FLAG_READ_1_1_2        |	\
		   FLASH_FLAG_READ_1_2_2        |	\
		   FLASH_FLAG_READ_1_1_4        |	\
		   FLASH_FLAG_READ_1_4_4        |	\
		   FLASH_FLAG_WRITE_1_1_4)
	{ "w25q80",  0xef4014, 0,  64 * 1024,  16, W25Q_FLAG, 80, NULL },
	{ "w25q16",  0xef4015, 0,  64 * 1024,  32, W25Q_FLAG, 80, NULL },
	{ "w25q32",  0xef4016, 0,  64 * 1024,  64, W25Q_FLAG, 80, NULL },
	{ "w25q64",  0xef4017, 0,  64 * 1024, 128, W25Q_FLAG, 80, NULL },

	/* Sentinel */
	{ NULL, 0x000000, 0, 0, 0, 0, 0, NULL },
};

static struct stfsm_seq stfsm_seq_read_jedec = {
	.data_size = TRANSFER_SIZE(8),
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_RDID)),
	.seq = {
		STFSM_INST_CMD1,
		STFSM_INST_DATA_READ,
		STFSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct stfsm_seq stfsm_seq_erase_sector = {
	/* 'addr_cfg' configured during initialisation */
	.seq_opc = {
		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_WREN) | SEQ_OPC_CSDEASSERT),

		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_SE)),
	},
	.seq = {
		STFSM_INST_CMD1,
		STFSM_INST_CMD2,
		STFSM_INST_ADD1,
		STFSM_INST_ADD2,
		STFSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static int stfsm_n25q_en_32bit_addr_seq(struct stfsm_seq *seq)
{
	seq->seq_opc[0] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(FLASH_CMD_EN4B_ADDR));
	seq->seq_opc[1] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(FLASH_CMD_WREN) |
			   SEQ_OPC_CSDEASSERT);

	seq->seq[0] = STFSM_INST_CMD2;
	seq->seq[1] = STFSM_INST_CMD1;
	seq->seq[2] = STFSM_INST_WAIT;
	seq->seq[3] = STFSM_INST_STOP;

	seq->seq_cfg = (SEQ_CFG_PADS_1 |
			SEQ_CFG_ERASE |
			SEQ_CFG_READNOTWRITE |
			SEQ_CFG_CSDEASSERT |
			SEQ_CFG_STARTSEQ);

	return 0;
}

static inline int stfsm_is_idle(struct stfsm *fsm)
{
	return readl(fsm->base + SPI_FAST_SEQ_STA) & 0x10;
}

static inline uint32_t stfsm_fifo_available(struct stfsm *fsm)
{
	return (readl(fsm->base + SPI_FAST_SEQ_STA) >> 5) & 0x7f;
}

static void stfsm_clear_fifo(struct stfsm *fsm)
{
	uint32_t avail;

	for (;;) {
		avail = stfsm_fifo_available(fsm);
		if (!avail)
			break;

		while (avail) {
			readl(fsm->base + SPI_FAST_SEQ_DATA_REG);
			avail--;
		}
	}
}

static inline void stfsm_load_seq(struct stfsm *fsm,
				  const struct stfsm_seq *seq)
{
	void __iomem *dst = fsm->base + SPI_FAST_SEQ_TRANSFER_SIZE;
	const uint32_t *src = (const uint32_t *)seq;
	int words = sizeof(*seq) / sizeof(*src);

	BUG_ON(!stfsm_is_idle(fsm));

	while (words--) {
		writel(*src, dst);
		src++;
		dst += 4;
	}
}

static void stfsm_wait_seq(struct stfsm *fsm)
{
	unsigned long deadline;
	int timeout = 0;

	deadline = jiffies + msecs_to_jiffies(STFSM_MAX_WAIT_SEQ_MS);

	while (!timeout) {
		if (time_after_eq(jiffies, deadline))
			timeout = 1;

		if (stfsm_is_idle(fsm))
			return;

		cond_resched();
	}

	dev_err(fsm->dev, "timeout on sequence completion\n");
}

static void stfsm_read_fifo(struct stfsm *fsm, uint32_t *buf,
			    const uint32_t size)
{
	uint32_t remaining = size >> 2;
	uint32_t avail;
	uint32_t words;

	dev_dbg(fsm->dev, "Reading %d bytes from FIFO\n", size);

	BUG_ON((((uint32_t)buf) & 0x3) || (size & 0x3));

	while (remaining) {
		for (;;) {
			avail = stfsm_fifo_available(fsm);
			if (avail)
				break;
			udelay(1);
		}
		words = min(avail, remaining);
		remaining -= words;

		readsl(fsm->base + SPI_FAST_SEQ_DATA_REG, buf, words);
		buf += words;
	}
}

/*
 * SoC reset on 'boot-from-spi' systems
 *
 * Certain modes of operation cause the Flash device to enter a particular state
 * for a period of time (e.g. 'Erase Sector', 'Quad Enable', and 'Enter 32-bit
 * Addr' commands).  On boot-from-spi systems, it is important to consider what
 * happens if a warm reset occurs during this period.  The SPIBoot controller
 * assumes that Flash device is in its default reset state, 24-bit address mode,
 * and ready to accept commands.  This can be achieved using some form of
 * on-board logic/controller to force a device POR in response to a SoC-level
 * reset or by making use of the device reset signal if available (limited
 * number of devices only).
 *
 * Failure to take such precautions can cause problems following a warm reset.
 * For some operations (e.g. ERASE), there is little that can be done.  For
 * other modes of operation (e.g. 32-bit addressing), options are often
 * available that can help minimise the window in which a reset could cause a
 * problem.
 *
 */
static bool stfsm_can_handle_soc_reset(struct stfsm *fsm)
{
	/* Reset signal is available on the board and supported by the device */
	if (fsm->reset_signal && fsm->info->flags & FLASH_FLAG_RESET)
		return true;

	/* Board-level logic forces a power-on-reset */
	if (fsm->reset_por)
		return true;

	/* Reset is not properly handled and may result in failure to reboot */
	return false;
}

/* Configure 'addr_cfg' according to addressing mode */
static void stfsm_prepare_erasesec_seq(struct stfsm *fsm,
				       struct stfsm_seq *seq)
{
	int addr1_cycles = fsm->info->flags & FLASH_FLAG_32BIT_ADDR ? 16 : 8;

	seq->addr_cfg = (ADR_CFG_CYCLES_ADD1(addr1_cycles) |
			 ADR_CFG_PADS_1_ADD1 |
			 ADR_CFG_CYCLES_ADD2(16) |
			 ADR_CFG_PADS_1_ADD2 |
			 ADR_CFG_CSDEASSERT_ADD2);
}

/* Search for preferred configuration based on available flags */
static struct seq_rw_config *
stfsm_search_seq_rw_configs(struct stfsm *fsm,
			    struct seq_rw_config cfgs[])
{
	struct seq_rw_config *config;
	int flags = fsm->info->flags;

	for (config = cfgs; config->cmd != 0; config++)
		if ((config->flags & flags) == config->flags)
			return config;

	return NULL;
}

/* Prepare a READ/WRITE sequence according to configuration parameters */
static void stfsm_prepare_rw_seq(struct stfsm *fsm,
				 struct stfsm_seq *seq,
				 struct seq_rw_config *cfg)
{
	int addr1_cycles, addr2_cycles;
	int i = 0;

	memset(seq, 0, sizeof(*seq));

	/* Add READ/WRITE OPC  */
	seq->seq_opc[i++] = (SEQ_OPC_PADS_1 |
			     SEQ_OPC_CYCLES(8) |
			     SEQ_OPC_OPCODE(cfg->cmd));

	/* Add WREN OPC for a WRITE sequence */
	if (cfg->write)
		seq->seq_opc[i++] = (SEQ_OPC_PADS_1 |
				     SEQ_OPC_CYCLES(8) |
				     SEQ_OPC_OPCODE(FLASH_CMD_WREN) |
				     SEQ_OPC_CSDEASSERT);

	/* Address configuration (24 or 32-bit addresses) */
	addr1_cycles  = (fsm->info->flags & FLASH_FLAG_32BIT_ADDR) ? 16 : 8;
	addr1_cycles /= cfg->addr_pads;
	addr2_cycles  = 16 / cfg->addr_pads;
	seq->addr_cfg = ((addr1_cycles & 0x3f) << 0 |	/* ADD1 cycles */
			 (cfg->addr_pads - 1) << 6 |	/* ADD1 pads */
			 (addr2_cycles & 0x3f) << 16 |	/* ADD2 cycles */
			 ((cfg->addr_pads - 1) << 22));	/* ADD2 pads */

	/* Data/Sequence configuration */
	seq->seq_cfg = ((cfg->data_pads - 1) << 16 |
			SEQ_CFG_STARTSEQ |
			SEQ_CFG_CSDEASSERT);
	if (!cfg->write)
		seq->seq_cfg |= SEQ_CFG_READNOTWRITE;

	/* Mode configuration (no. of pads taken from addr cfg) */
	seq->mode = ((cfg->mode_data & 0xff) << 0 |	/* data */
		     (cfg->mode_cycles & 0x3f) << 16 |	/* cycles */
		     (cfg->addr_pads - 1) << 22);	/* pads */

	/* Dummy configuration (no. of pads taken from addr cfg) */
	seq->dummy = ((cfg->dummy_cycles & 0x3f) << 16 |	/* cycles */
		      (cfg->addr_pads - 1) << 22);		/* pads */


	/* Instruction sequence */
	i = 0;
	if (cfg->write)
		seq->seq[i++] = STFSM_INST_CMD2;

	seq->seq[i++] = STFSM_INST_CMD1;

	seq->seq[i++] = STFSM_INST_ADD1;
	seq->seq[i++] = STFSM_INST_ADD2;

	if (cfg->mode_cycles)
		seq->seq[i++] = STFSM_INST_MODE;

	if (cfg->dummy_cycles)
		seq->seq[i++] = STFSM_INST_DUMMY;

	seq->seq[i++] =
		cfg->write ? STFSM_INST_DATA_WRITE : STFSM_INST_DATA_READ;
	seq->seq[i++] = STFSM_INST_STOP;
}

static int stfsm_search_prepare_rw_seq(struct stfsm *fsm,
				       struct stfsm_seq *seq,
				       struct seq_rw_config *cfgs)
{
	struct seq_rw_config *config;

	config = stfsm_search_seq_rw_configs(fsm, cfgs);
	if (!config) {
		dev_err(fsm->dev, "failed to find suitable config\n");
		return -EINVAL;
	}

	stfsm_prepare_rw_seq(fsm, seq, config);

	return 0;
}

static void stfsm_read_jedec(struct stfsm *fsm, uint8_t *const jedec)
{
	const struct stfsm_seq *seq = &stfsm_seq_read_jedec;
	uint32_t tmp[2];

	stfsm_load_seq(fsm, seq);

	stfsm_read_fifo(fsm, tmp, 8);

	memcpy(jedec, tmp, 5);

	stfsm_wait_seq(fsm);
}

static struct flash_info *stfsm_jedec_probe(struct stfsm *fsm)
{
	struct flash_info	*info;
	u16                     ext_jedec;
	u32			jedec;
	u8			id[5];

	stfsm_read_jedec(fsm, id);

	jedec     = id[0] << 16 | id[1] << 8 | id[2];
	/*
	 * JEDEC also defines an optional "extended device information"
	 * string for after vendor-specific data, after the three bytes
	 * we use here. Supporting some chips might require using it.
	 */
	ext_jedec = id[3] << 8  | id[4];

	dev_dbg(fsm->dev, "JEDEC =  0x%08x [%02x %02x %02x %02x %02x]\n",
		jedec, id[0], id[1], id[2], id[3], id[4]);

	for (info = flash_types; info->name; info++) {
		if (info->jedec_id == jedec) {
			if (info->ext_id && info->ext_id != ext_jedec)
				continue;
			return info;
		}
	}
	dev_err(fsm->dev, "Unrecognized JEDEC id %06x\n", jedec);

	return NULL;
}

static int stfsm_set_mode(struct stfsm *fsm, uint32_t mode)
{
	int ret, timeout = 10;

	/* Wait for controller to accept mode change */
	while (--timeout) {
		ret = readl(fsm->base + SPI_STA_MODE_CHANGE);
		if (ret & 0x1)
			break;
		udelay(1);
	}

	if (!timeout)
		return -EBUSY;

	writel(mode, fsm->base + SPI_MODESELECT);

	return 0;
}

static void stfsm_set_freq(struct stfsm *fsm, uint32_t spi_freq)
{
	uint32_t emi_freq;
	uint32_t clk_div;

	/* TODO: Make this dynamic */
	emi_freq = STFSM_DEFAULT_EMI_FREQ;

	/*
	 * Calculate clk_div - values between 2 and 128
	 * Multiple of 2, rounded up
	 */
	clk_div = 2 * DIV_ROUND_UP(emi_freq, 2 * spi_freq);
	if (clk_div < 2)
		clk_div = 2;
	else if (clk_div > 128)
		clk_div = 128;

	/*
	 * Determine a suitable delay for the IP to complete a change of
	 * direction of the FIFO. The required delay is related to the clock
	 * divider used. The following heuristics are based on empirical tests,
	 * using a 100MHz EMI clock.
	 */
	if (clk_div <= 4)
		fsm->fifo_dir_delay = 0;
	else if (clk_div <= 10)
		fsm->fifo_dir_delay = 1;
	else
		fsm->fifo_dir_delay = DIV_ROUND_UP(clk_div, 10);

	dev_dbg(fsm->dev, "emi_clk = %uHZ, spi_freq = %uHZ, clk_div = %u\n",
		emi_freq, spi_freq, clk_div);

	writel(clk_div, fsm->base + SPI_CLOCKDIV);
}

static int stfsm_init(struct stfsm *fsm)
{
	int ret;

	/* Perform a soft reset of the FSM controller */
	writel(SEQ_CFG_SWRESET, fsm->base + SPI_FAST_SEQ_CFG);
	udelay(1);
	writel(0, fsm->base + SPI_FAST_SEQ_CFG);

	/* Set clock to 'safe' frequency initially */
	stfsm_set_freq(fsm, STFSM_FLASH_SAFE_FREQ);

	/* Switch to FSM */
	ret = stfsm_set_mode(fsm, SPI_MODESELECT_FSM);
	if (ret)
		return ret;

	/* Set timing parameters */
	writel(SPI_CFG_DEVICE_ST            |
	       SPI_CFG_DEFAULT_MIN_CS_HIGH  |
	       SPI_CFG_DEFAULT_CS_SETUPHOLD |
	       SPI_CFG_DEFAULT_DATA_HOLD,
	       fsm->base + SPI_CONFIGDATA);
	writel(STFSM_DEFAULT_WR_TIME, fsm->base + SPI_STATUS_WR_TIME_REG);

	/* Clear FIFO, just in case */
	stfsm_clear_fifo(fsm);

	return 0;
}

static void stfsm_fetch_platform_configs(struct platform_device *pdev)
{
	struct stfsm *fsm = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;
	struct regmap *regmap;
	uint32_t boot_device_reg;
	uint32_t boot_device_spi;
	uint32_t boot_device;     /* Value we read from *boot_device_reg */
	int ret;

	/* Booting from SPI NOR Flash is the default */
	fsm->booted_from_spi = true;

	regmap = syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(regmap))
		goto boot_device_fail;

	fsm->reset_signal = of_property_read_bool(np, "st,reset-signal");

	fsm->reset_por = of_property_read_bool(np, "st,reset-por");

	/* Where in the syscon the boot device information lives */
	ret = of_property_read_u32(np, "st,boot-device-reg", &boot_device_reg);
	if (ret)
		goto boot_device_fail;

	/* Boot device value when booted from SPI NOR */
	ret = of_property_read_u32(np, "st,boot-device-spi", &boot_device_spi);
	if (ret)
		goto boot_device_fail;

	ret = regmap_read(regmap, boot_device_reg, &boot_device);
	if (ret)
		goto boot_device_fail;

	if (boot_device != boot_device_spi)
		fsm->booted_from_spi = false;

	return;

boot_device_fail:
	dev_warn(&pdev->dev,
		 "failed to fetch boot device, assuming boot from SPI\n");
}

static int stfsm_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct flash_info *info;
	struct resource *res;
	struct stfsm *fsm;
	int ret;

	if (!np) {
		dev_err(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}

	fsm = devm_kzalloc(&pdev->dev, sizeof(*fsm), GFP_KERNEL);
	if (!fsm)
		return -ENOMEM;

	fsm->dev = &pdev->dev;

	platform_set_drvdata(pdev, fsm);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Resource not found\n");
		return -ENODEV;
	}

	fsm->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(fsm->base)) {
		dev_err(&pdev->dev,
			"Failed to reserve memory region %pR\n", res);
		return PTR_ERR(fsm->base);
	}

	mutex_init(&fsm->lock);

	ret = stfsm_init(fsm);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialise FSM Controller\n");
		return ret;
	}

	stfsm_fetch_platform_configs(pdev);

	/* Detect SPI FLASH device */
	info = stfsm_jedec_probe(fsm);
	if (!info)
		return -ENODEV;
	fsm->info = info;

	/* Use device size to determine address width */
	if (info->sector_size * info->n_sectors > 0x1000000)
		info->flags |= FLASH_FLAG_32BIT_ADDR;

	fsm->mtd.dev.parent	= &pdev->dev;
	fsm->mtd.type		= MTD_NORFLASH;
	fsm->mtd.writesize	= 4;
	fsm->mtd.writebufsize	= fsm->mtd.writesize;
	fsm->mtd.flags		= MTD_CAP_NORFLASH;
	fsm->mtd.size		= info->sector_size * info->n_sectors;
	fsm->mtd.erasesize	= info->sector_size;

	dev_err(&pdev->dev,
		"Found serial flash device: %s\n"
		" size = %llx (%lldMiB) erasesize = 0x%08x (%uKiB)\n",
		info->name,
		(long long)fsm->mtd.size, (long long)(fsm->mtd.size >> 20),
		fsm->mtd.erasesize, (fsm->mtd.erasesize >> 10));

	return mtd_device_parse_register(&fsm->mtd, NULL, NULL, NULL, 0);
}

static int stfsm_remove(struct platform_device *pdev)
{
	struct stfsm *fsm = platform_get_drvdata(pdev);
	int err;

	err = mtd_device_unregister(&fsm->mtd);
	if (err)
		return err;

	return 0;
}

static struct of_device_id stfsm_match[] = {
	{ .compatible = "st,spi-fsm", },
	{},
};
MODULE_DEVICE_TABLE(of, stfsm_match);

static struct platform_driver stfsm_driver = {
	.probe		= stfsm_probe,
	.remove		= stfsm_remove,
	.driver		= {
		.name	= "st-spi-fsm",
		.owner	= THIS_MODULE,
		.of_match_table = stfsm_match,
	},
};
module_platform_driver(stfsm_driver);

MODULE_AUTHOR("Angus Clark <angus.clark@st.com>");
MODULE_DESCRIPTION("ST SPI FSM driver");
MODULE_LICENSE("GPL");

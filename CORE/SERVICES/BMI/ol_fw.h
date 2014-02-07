/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef _OL_FW_H_
#define _OL_FW_H_

#ifdef QCA_WIFI_FTM
#include "vos_types.h"
#endif

#define AR9888_REV1_VERSION          0x4000002c
#define AR9888_REV2_VERSION          0x4100016c
#define QCA_VERSION                  0x4100270f
#define AR6320_REV1_VERSION	     0x5000000
#define AR6320_REV1_1_VERSION	     0x5000001
#define AR6320_REV1_3_VERSION	     0x5000003
#define QCA_FIRMWARE_FILE            "athwlan.bin"
#define QCA_UTF_FIRMWARE_FILE        "utf.bin"
#define QCA_BOARD_DATA_FILE          "fakeboar.bin"
#define QCA_OTP_FILE                 "otp.bin"
#define AR61X4_SINGLE_FILE           "qca61x4.bin"

/* Configuration for statistics pushed by firmware */
#define PDEV_DEFAULT_STATS_UPDATE_PERIOD    500
#define VDEV_DEFAULT_STATS_UPDATE_PERIOD    500
#define PEER_DEFAULT_STATS_UPDATE_PERIOD    500

#if defined(QCA_WIFI_2_0) && !defined(QCA_WIFI_ISOC)
#define REGISTER_LOCATION       0x00000000
#define REGISTER_SIZE           0x00000800

#define DRAM_LOCATION           0x00400000
#define DRAM_SIZE               0x00070000

#define IRAM_LOCATION           0x00980000
#define IRAM_SIZE               0x00038000

#define AXI_LOCATION            0x000a0000
#define AXI_SIZE                0x00018000

#define CE_OFFSET               0x00000400
#define CE_USEFUL_SIZE          0x00000058

#define TOTAL_DUMP_SIZE         0x00200000
#define PCIE_READ_LIMIT         0x00005000

int ol_target_coredump(void *instance, void* memoryBlock,
                        u_int32_t blockLength);
int ol_diag_read(struct ol_softc *scn, u_int8_t* buffer,
                 u_int32_t pos, size_t count);
void schedule_ramdump_work(struct ol_softc *scn);
#endif
int ol_download_firmware(struct ol_softc *scn);
int ol_configure_target(struct ol_softc *scn);
void ol_target_failure(void *instance, A_STATUS status);
u_int8_t ol_get_number_of_peers_supported(struct ol_softc *scn);
#endif /* _OL_FW_H_ */

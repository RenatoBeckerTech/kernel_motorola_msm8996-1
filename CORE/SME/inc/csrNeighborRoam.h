/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file csrNeighborRoam.h

    Exports and types for the neighbor roaming algorithm which is sepcifically
    designed for Android.
========================================================================== */
#ifndef CSR_NEIGHBOR_ROAM_H
#define CSR_NEIGHBOR_ROAM_H

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
#include "sme_Api.h"

#define ROAM_AP_AGE_LIMIT_MS                     10000

/* Enumeration of various states in neighbor roam algorithm */
typedef enum
{
    eCSR_NEIGHBOR_ROAM_STATE_CLOSED,
    eCSR_NEIGHBOR_ROAM_STATE_INIT,
    eCSR_NEIGHBOR_ROAM_STATE_CONNECTED,
    eCSR_NEIGHBOR_ROAM_STATE_CFG_CHAN_LIST_SCAN,
    eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING,
#ifdef WLAN_FEATURE_VOWIFI_11R
    eCSR_NEIGHBOR_ROAM_STATE_REPORT_QUERY,
    eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN,
    eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING,
    eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE,
#endif /* WLAN_FEATURE_VOWIFI_11R */
    eNEIGHBOR_STATE_MAX
} eCsrNeighborRoamState;

/* Parameters that are obtained from CFG */
typedef struct sCsrNeighborRoamCfgParams
{
    tANI_U8         maxNeighborRetries;
    tANI_U32        neighborScanPeriod;
    tCsrChannelInfo channelInfo;
    tANI_U8         neighborLookupThreshold;
    tANI_U8         neighborReassocThreshold;
    tANI_U32        minChannelScanTime;
    tANI_U32        maxChannelScanTime;
    tANI_U16        neighborResultsRefreshPeriod;
    tANI_U16        emptyScanRefreshPeriod;
    tANI_U8         nOpportunisticThresholdDiff;
    tANI_U8         nRoamRescanRssiDiff;
    tANI_U8         nRoamBmissFirstBcnt;
    tANI_U8         nRoamBmissFinalBcnt;
    tANI_U8         nRoamBeaconRssiWeight;
    tANI_U8         delay_before_vdev_stop;
    uint32_t        hi_rssi_scan_max_count;
    uint32_t        hi_rssi_scan_rssi_delta;
    uint32_t        hi_rssi_scan_delay;
    int32_t        hi_rssi_scan_rssi_ub;
} tCsrNeighborRoamCfgParams, *tpCsrNeighborRoamCfgParams;

#define CSR_NEIGHBOR_ROAM_INVALID_CHANNEL_INDEX    255
typedef struct sCsrNeighborRoamChannelInfo
{
    tANI_BOOLEAN    IAPPNeighborListReceived; // Flag to mark reception of IAPP Neighbor list
    tANI_BOOLEAN    chanListScanInProgress;
    tANI_U8         currentChanIndex;       //Current channel index that is being scanned
    tCsrChannelInfo currentChannelListInfo; //Max number of channels in channel list and the list of channels
} tCsrNeighborRoamChannelInfo, *tpCsrNeighborRoamChannelInfo;

typedef struct sCsrNeighborRoamBSSInfo
{
    tListElem           List;
    tANI_U8             apPreferenceVal;
//    tCsrScanResultInfo  *scanResultInfo;
    tpSirBssDescription pBssDescription;
} tCsrNeighborRoamBSSInfo, *tpCsrNeighborRoamBSSInfo;

#ifdef WLAN_FEATURE_VOWIFI_11R
#define CSR_NEIGHBOR_ROAM_REPORT_QUERY_TIMEOUT  1000    //in milliseconds
#define CSR_NEIGHBOR_ROAM_PREAUTH_RSP_WAIT_MULTIPLIER   10     //in milliseconds
#define MAX_NUM_PREAUTH_FAIL_LIST_ADDRESS       10 //Max number of MAC addresses with which the pre-auth was failed
#define MAX_BSS_IN_NEIGHBOR_RPT                 15
#define CSR_NEIGHBOR_ROAM_MAX_NUM_PREAUTH_RETRIES 3

/* Black listed APs. List of MAC Addresses with which the Preauthentication was failed. */
typedef struct sCsrPreauthFailListInfo
{
    tANI_U8     numMACAddress;
    tSirMacAddr macAddress[MAX_NUM_PREAUTH_FAIL_LIST_ADDRESS];
} tCsrPreauthFailListInfo, *tpCsrPreauthFailListInfo;

typedef struct sCsrNeighborReportBssInfo
{
    tANI_U8 channelNum;
    tANI_U8 neighborScore;
    tSirMacAddr neighborBssId;
} tCsrNeighborReportBssInfo, *tpCsrNeighborReportBssInfo;

typedef struct sCsr11rAssocNeighborInfo
{
    tANI_BOOLEAN                preauthRspPending;
    tANI_BOOLEAN                neighborRptPending;
    tANI_U8                     currentNeighborRptRetryNum;
    tCsrPreauthFailListInfo     preAuthFailList;
    tANI_U32                    neighborReportTimeout;
    tANI_U32                    PEPreauthRespTimeout;
    tANI_U8                     numPreAuthRetries;
    tDblLinkList                preAuthDoneList;    /* Linked list which consists or preauthenticated nodes */
    tANI_U8                     numBssFromNeighborReport;
    tCsrNeighborReportBssInfo   neighboReportBssInfo[MAX_BSS_IN_NEIGHBOR_RPT];  //Contains info needed during REPORT_SCAN State
} tCsr11rAssocNeighborInfo, *tpCsr11rAssocNeighborInfo;
#endif /* WLAN_FEATURE_VOWIFI_11R */

/*
 * Set lookup UP threshold 5 dB higher than the configured
 * lookup DOWN threshold to minimize thrashing between
 * DOWN and UP events.
 */
#define NEIGHBOR_ROAM_LOOKUP_UP_THRESHOLD \
    (pNeighborRoamInfo->cfgParams.neighborLookupThreshold-5)
#ifdef FEATURE_WLAN_LFR
typedef enum
{
    eFirstEmptyScan=1,
    eSecondEmptyScan,
    eThirdEmptyScan,
    eFourthEmptyScan,
    eFifthEmptyScan,
    eMaxEmptyScan=eFifthEmptyScan,
} eNeighborRoamEmptyScanCount;

typedef enum
{
    DEFAULT_SCAN=0,
    SPLIT_SCAN_OCCUPIED_LIST=1,
} eNeighborRoamScanMode;
#endif

/* Complete control information for neighbor roam algorithm */
typedef struct sCsrNeighborRoamControlInfo
{
    eCsrNeighborRoamState       neighborRoamState;
    eCsrNeighborRoamState       prevNeighborRoamState;
    tCsrNeighborRoamCfgParams   cfgParams;
    tCsrBssid                   currAPbssid; // current assoc AP
    tANI_U8                     currAPoperationChannel; // current assoc AP
    vos_timer_t                 neighborScanTimer;
    vos_timer_t                 neighborResultsRefreshTimer;
    vos_timer_t                 emptyScanRefreshTimer;
    tCsrTimerInfo               neighborScanTimerInfo;
    tCsrNeighborRoamChannelInfo roamChannelInfo;
    tANI_U8                     currentNeighborLookupThreshold;
    tANI_U8                     currentOpportunisticThresholdDiff;
    tANI_U8                     currentRoamRescanRssiDiff;
    tANI_BOOLEAN                scanRspPending;
    tANI_TIMESTAMP              scanRequestTimeStamp;
    tDblLinkList                roamableAPList;    // List of current FT candidates
    tCsrRoamProfile             csrNeighborRoamProfile;
#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_BOOLEAN                is11rAssoc;
    tCsr11rAssocNeighborInfo    FTRoamInfo;
#endif /* WLAN_FEATURE_VOWIFI_11R */
#ifdef FEATURE_WLAN_ESE
    tANI_BOOLEAN                isESEAssoc;
    tANI_BOOLEAN                isVOAdmitted;
    tANI_U32                    MinQBssLoadRequired;
#endif
#ifdef FEATURE_WLAN_LFR
    tANI_U8                     uEmptyScanCount; /* Consecutive number of times scan
                                                    yielded no results. */
    tCsrRoamConnectedProfile    prevConnProfile; /* Previous connected profile. If the
                                                    new profile does not match previous
                                                    we re-initialize occupied channel list */
    tANI_S8                     lookupDOWNRssi;
    tANI_U8                     uScanMode;
    tANI_U8                     uOsRequestedHandoff; /* upper layer requested
                                                        a reassoc */
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    tCsrHandoffRequest          handoffReqInfo; /* handoff related info came
                                                   with upper layer's req for
                                                   reassoc */
#endif
#endif
    tSmeFastRoamTrigger         cfgRoamEn;
    tSirMacAddr                 cfgRoambssId;
    tANI_U8                     currentRoamBmissFirstBcnt;
    tANI_U8                     currentRoamBmissFinalBcnt;
    tANI_U8                     currentRoamBeaconRssiWeight;
    tANI_U8                     lastSentCmd;
} tCsrNeighborRoamControlInfo, *tpCsrNeighborRoamControlInfo;

/* All the necessary Function declarations are here */
eHalStatus csrNeighborRoamIndicateConnect(tpAniSirGlobal pMac,
                                         tANI_U8 sessionId, VOS_STATUS status);
eHalStatus csrNeighborRoamIndicateDisconnect(tpAniSirGlobal pMac,
                                         tANI_U8 sessionId);
tANI_BOOLEAN csrNeighborRoamIsHandoffInProgress(tpAniSirGlobal pMac,
                                                tANI_U8 sessionId);
void csrNeighborRoamRequestHandoff(tpAniSirGlobal pMac, tANI_U8 sessionId);
eHalStatus csrNeighborRoamInit(tpAniSirGlobal pMac, tANI_U8 sessionId);
void csrNeighborRoamClose(tpAniSirGlobal pMac, tANI_U8 sessionId);
void csrNeighborRoamPurgePreauthFailedList(tpAniSirGlobal pMac);
VOS_STATUS csrNeighborRoamTransitToCFGChanScan(tpAniSirGlobal pMac,
                                               tANI_U8 sessionId);
VOS_STATUS csrNeighborRoamTransitionToPreauthDone(tpAniSirGlobal pMac);
eHalStatus csrNeighborRoamPrepareScanProfileFilter(tpAniSirGlobal pMac,
                                              tCsrScanResultFilter *pScanFilter,
                                              tANI_U8 sessionId);
bool csrNeighborRoamGetHandoffAPInfo(tpAniSirGlobal pMac,
                                     tpCsrNeighborRoamBSSInfo pHandoffNode,
                                     tANI_U8 sessionId);
eHalStatus csrNeighborRoamPreauthRspHandler(tpAniSirGlobal pMac,
                                            tANI_U8 sessionId,
                                            tSirRetStatus limStatus);
#ifdef WLAN_FEATURE_VOWIFI_11R
tANI_BOOLEAN csrNeighborRoamIs11rAssoc(tpAniSirGlobal pMac,
                                       tANI_U8 sessionId);
#endif
VOS_STATUS csrNeighborRoamCreateChanListFromNeighborReport(tpAniSirGlobal pMac,
                                                           tANI_U8 sessionId);
void csrNeighborRoamTranistionPreauthDoneToDisconnected(tpAniSirGlobal pMac,
                                                        tANI_U8 sessionId);
tANI_BOOLEAN csrNeighborRoamStatePreauthDone(tpAniSirGlobal pMac,
                                             tANI_U8 sessionId);
tANI_BOOLEAN csrNeighborRoamScanRspPending(tHalHandle hHal,
                                           tANI_U8 sessionId);
tANI_BOOLEAN csrNeighborMiddleOfRoaming(tHalHandle hHal, tANI_U8 sessionId);
VOS_STATUS csrNeighborRoamSetLookupRssiThreshold(tpAniSirGlobal pMac,
                                    tANI_U8 sessionId,
                                    v_U8_t neighborLookupRssiThreshold);
VOS_STATUS
csrNeighborRoamSetOpportunisticScanThresholdDiff(tpAniSirGlobal pMac,
                                        tANI_U8 sessionId,
                                        v_U8_t nOpportunisticThresholdDiff);
VOS_STATUS
csrNeighborRoamSetRoamRescanRssiDiff(tpAniSirGlobal pMac,
                                     tANI_U8 sessionId,
                                     v_U8_t nRoamRescanRssiDiff);
VOS_STATUS
csrNeighborRoamSetRoamBmissFirstBcnt(tpAniSirGlobal pMac,
                                     tANI_U8 sessionId,
                                     v_U8_t nRoamBmissFirstBcnt);
VOS_STATUS
csrNeighborRoamSetRoamBmissFinalBcnt(tpAniSirGlobal pMac,
                                     tANI_U8 sessionId,
                                     v_U8_t nRoamBmissFinalBcnt);
VOS_STATUS
csrNeighborRoamSetRoamBeaconRssiWeight(tpAniSirGlobal pMac,
                                     tANI_U8 sessionId,
                                     v_U8_t nRoamBeaconRssiWeight);
VOS_STATUS csrNeighborRoamUpdateFastRoamingEnabled(tpAniSirGlobal pMac,
                                                 tANI_U8 sessionId,
                                               const v_BOOL_t fastRoamEnabled);
VOS_STATUS csrNeighborRoamUpdateEseModeEnabled(tpAniSirGlobal pMac,
                                               tANI_U8 sessionId,
                                               const v_BOOL_t eseMode);
VOS_STATUS csrNeighborRoamChannelsFilterByCurrentBand(
                      tpAniSirGlobal pMac,
                      tANI_U8   sessionId,
                      tANI_U8*  pInputChannelList,
                      tANI_U8   inputNumOfChannels,
                      tANI_U8*  pOutputChannelList,
                      tANI_U8*  pMergedOutputNumOfChannels
                      );
VOS_STATUS csrNeighborRoamReassocIndCallback(v_PVOID_t pAdapter,
                                             v_U8_t trafficStatus,
                                             v_PVOID_t pUserCtxt,
                                             v_S7_t   avgRssi);
VOS_STATUS csrNeighborRoamMergeChannelLists(tpAniSirGlobal pMac,
                                    tANI_U8  *pInputChannelList,
                                    tANI_U8  inputNumOfChannels,
                                    tANI_U8  *pOutputChannelList,
                                    tANI_U8  outputNumOfChannels,
                                    tANI_U8  *pMergedOutputNumOfChannels);
void csr_roam_reset_roam_params(tpAniSirGlobal mac_ptr);
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#define ROAM_SCAN_OFFLOAD_START                     1
#define ROAM_SCAN_OFFLOAD_STOP                      2
#define ROAM_SCAN_OFFLOAD_RESTART                   3
#define ROAM_SCAN_OFFLOAD_UPDATE_CFG                4
#define ROAM_SCAN_OFFLOAD_ABORT_SCAN                5

#define REASON_CONNECT                              1
#define REASON_CHANNEL_LIST_CHANGED                 2
#define REASON_LOOKUP_THRESH_CHANGED                3
#define REASON_DISCONNECTED                         4
#define REASON_RSSI_DIFF_CHANGED                    5
#define REASON_ESE_INI_CFG_CHANGED                  6
#define REASON_NEIGHBOR_SCAN_REFRESH_PERIOD_CHANGED 7
#define REASON_VALID_CHANNEL_LIST_CHANGED           8
#define REASON_FLUSH_CHANNEL_LIST                   9
#define REASON_EMPTY_SCAN_REF_PERIOD_CHANGED        10
#define REASON_PREAUTH_FAILED_FOR_ALL               11
#define REASON_NO_CAND_FOUND_OR_NOT_ROAMING_NOW     12
#define REASON_NPROBES_CHANGED                      13
#define REASON_HOME_AWAY_TIME_CHANGED               14
#define REASON_OS_REQUESTED_ROAMING_NOW             15
#define REASON_SCAN_CH_TIME_CHANGED                 16
#define REASON_SCAN_HOME_TIME_CHANGED               17
#define REASON_OPPORTUNISTIC_THRESH_DIFF_CHANGED    18
#define REASON_ROAM_RESCAN_RSSI_DIFF_CHANGED        19
#define REASON_ROAM_BMISS_FIRST_BCNT_CHANGED        20
#define REASON_ROAM_BMISS_FINAL_BCNT_CHANGED        21
#define REASON_ROAM_BEACON_RSSI_WEIGHT_CHANGED      22
#define REASON_ROAM_DFS_SCAN_MODE_CHANGED           23
#define REASON_ROAM_ABORT_ROAM_SCAN                 24
#define REASON_ROAM_EXT_SCAN_PARAMS_CHANGED         25
#define REASON_ROAM_SET_SSID_ALLOWED                26
#define REASON_ROAM_SET_FAVORED_BSSID               27
#define REASON_ROAM_GOOD_RSSI_CHANGED               28
#define REASON_ROAM_SET_BLACKLIST_BSSID             29
#define REASON_ROAM_SCAN_HI_RSSI_MAXCOUNT_CHANGED   30
#define REASON_ROAM_SCAN_HI_RSSI_DELTA_CHANGED      31
#define REASON_ROAM_SCAN_HI_RSSI_DELAY_CHANGED      32
#define REASON_ROAM_SCAN_HI_RSSI_UB_CHANGED         33
#define REASON_ROAM_SCAN_STA_ROAM_POLICY_CHANGED    34

eHalStatus csrRoamOffloadScan(tpAniSirGlobal pMac, tANI_U8 sessionId,
                              tANI_U8 command, tANI_U8 reason);
eHalStatus csrNeighborRoamCandidateFoundIndHdlr(tpAniSirGlobal pMac,
                                                void* pMsg);
eHalStatus csrNeighborRoamHandoffReqHdlr(tpAniSirGlobal pMac, void* pMsg);
eHalStatus csrNeighborRoamProceedWithHandoffReq(tpAniSirGlobal pMac,
                                                tANI_U8 sessionId);
eHalStatus csrNeighborRoamSssidScanDone(tpAniSirGlobal pMac,
                                        tANI_U8 sessionId,
                                        eHalStatus status);
eHalStatus csrNeighborRoamStartLfrScan(tpAniSirGlobal pMac, tANI_U8 sessionId);
#endif

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
VOS_STATUS csrSetCCKMIe(tpAniSirGlobal pMac, const tANI_U8 sessionId,
                            const tANI_U8 *pCckmIe,
                            const tANI_U8 ccKmIeLen);
VOS_STATUS csrRoamReadTSF(tpAniSirGlobal pMac, tANI_U8 *pTimestamp,
                          const tANI_U8 sessionId);
#endif /*FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
eHalStatus csrRoamOffloadSendSynchCnf(tpAniSirGlobal pMac, tANI_U8 sessionId);
eHalStatus
csrNeighborRoamOffloadUpdatePreauthList(tpAniSirGlobal pMac,
        tpSirRoamOffloadSynchInd pSmeRoamOffloadSynchInd,
                   tANI_U8 sessionId);
#endif
#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */

#endif /* CSR_NEIGHBOR_ROAM_H */

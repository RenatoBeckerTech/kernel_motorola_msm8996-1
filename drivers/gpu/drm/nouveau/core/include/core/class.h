#ifndef __NOUVEAU_CLASS_H__
#define __NOUVEAU_CLASS_H__

#include <nvif/class.h>

/* Perfmon counter class
 *
 * XXXX: NV_PERFCTR
 */
#define NV_PERFCTR_CLASS                                             0x0000ffff
#define NV_PERFCTR_QUERY                                             0x00000000
#define NV_PERFCTR_SAMPLE                                            0x00000001
#define NV_PERFCTR_READ                                              0x00000002

struct nv_perfctr_class {
	u16 logic_op;
	struct {
		char __user *name; /*XXX: use cfu when exposed to userspace */
		u32 size;
	} signal[4];
};

struct nv_perfctr_query {
	u32 iter;
	u32 size;
	char __user *name; /*XXX: use ctu when exposed to userspace */
};

struct nv_perfctr_sample {
};

struct nv_perfctr_read {
	u32 ctr;
	u32 clk;
};

/* Device control class
 *
 * XXXX: NV_CONTROL
 */
#define NV_CONTROL_CLASS                                             0x0000fffe

#define NV_CONTROL_PSTATE_INFO                                       0x00000000
#define NV_CONTROL_PSTATE_INFO_USTATE_DISABLE                              (-1)
#define NV_CONTROL_PSTATE_INFO_USTATE_PERFMON                              (-2)
#define NV_CONTROL_PSTATE_INFO_PSTATE_UNKNOWN                              (-1)
#define NV_CONTROL_PSTATE_INFO_PSTATE_PERFMON                              (-2)
#define NV_CONTROL_PSTATE_ATTR                                       0x00000001
#define NV_CONTROL_PSTATE_ATTR_STATE_CURRENT                               (-1)
#define NV_CONTROL_PSTATE_USER                                       0x00000002
#define NV_CONTROL_PSTATE_USER_STATE_UNKNOWN                               (-1)
#define NV_CONTROL_PSTATE_USER_STATE_PERFMON                               (-2)

struct nv_control_pstate_info {
	u32 count; /* out: number of power states */
	s32 ustate_ac; /* out: target pstate index */
	s32 ustate_dc; /* out: target pstate index */
	s32 pwrsrc; /* out: current power source */
	u32 pstate; /* out: current pstate index */
};

struct nv_control_pstate_attr {
	s32 state; /*  in: index of pstate to query
		    * out: pstate identifier
		    */
	u32 index; /*  in: index of attribute to query
		    * out: index of next attribute, or 0 if no more
		    */
	char name[32];
	char unit[16];
	u32 min;
	u32 max;
};

struct nv_control_pstate_user {
	s32 ustate; /*  in: pstate identifier */
	s32 pwrsrc; /*  in: target power source */
};

/* DMA FIFO channel classes
 *
 * 006b: NV03_CHANNEL_DMA
 * 006e: NV10_CHANNEL_DMA
 * 176e: NV17_CHANNEL_DMA
 * 406e: NV40_CHANNEL_DMA
 * 506e: NV50_CHANNEL_DMA
 * 826e: NV84_CHANNEL_DMA
 */
#define NV03_CHANNEL_DMA_CLASS                                       0x0000006b
#define NV10_CHANNEL_DMA_CLASS                                       0x0000006e
#define NV17_CHANNEL_DMA_CLASS                                       0x0000176e
#define NV40_CHANNEL_DMA_CLASS                                       0x0000406e
#define NV50_CHANNEL_DMA_CLASS                                       0x0000506e
#define NV84_CHANNEL_DMA_CLASS                                       0x0000826e

struct nv03_channel_dma_class {
	u32 pushbuf;
	u32 pad0;
	u64 offset;
};

/* Indirect FIFO channel classes
 *
 * 506f: NV50_CHANNEL_IND
 * 826f: NV84_CHANNEL_IND
 * 906f: NVC0_CHANNEL_IND
 * a06f: NVE0_CHANNEL_IND
 */

#define NV50_CHANNEL_IND_CLASS                                       0x0000506f
#define NV84_CHANNEL_IND_CLASS                                       0x0000826f
#define NVC0_CHANNEL_IND_CLASS                                       0x0000906f
#define NVE0_CHANNEL_IND_CLASS                                       0x0000a06f

struct nv50_channel_ind_class {
	u32 pushbuf;
	u32 ilength;
	u64 ioffset;
};

#define NVE0_CHANNEL_IND_ENGINE_GR                                   0x00000001
#define NVE0_CHANNEL_IND_ENGINE_VP                                   0x00000002
#define NVE0_CHANNEL_IND_ENGINE_PPP                                  0x00000004
#define NVE0_CHANNEL_IND_ENGINE_BSP                                  0x00000008
#define NVE0_CHANNEL_IND_ENGINE_CE0                                  0x00000010
#define NVE0_CHANNEL_IND_ENGINE_CE1                                  0x00000020
#define NVE0_CHANNEL_IND_ENGINE_ENC                                  0x00000040

struct nve0_channel_ind_class {
	u32 pushbuf;
	u32 ilength;
	u64 ioffset;
	u32 engine;
};

/* 0046: NV04_DISP
 */

#define NV04_DISP_CLASS                                              0x00000046

#define NV04_DISP_MTHD                                               0x00000000
#define NV04_DISP_MTHD_HEAD                                          0x00000001

#define NV04_DISP_SCANOUTPOS                                         0x00000000

struct nv04_display_class {
};

struct nv04_display_scanoutpos {
	s64 time[2];
	u32 vblanks;
	u32 vblanke;
	u32 vtotal;
	u32 vline;
	u32 hblanks;
	u32 hblanke;
	u32 htotal;
	u32 hline;
};

/* 5070: NV50_DISP
 * 8270: NV84_DISP
 * 8370: NVA0_DISP
 * 8870: NV94_DISP
 * 8570: NVA3_DISP
 * 9070: NVD0_DISP
 * 9170: NVE0_DISP
 * 9270: NVF0_DISP
 * 9470: GM107_DISP
 */

#define NV50_DISP_CLASS                                              0x00005070
#define NV84_DISP_CLASS                                              0x00008270
#define NVA0_DISP_CLASS                                              0x00008370
#define NV94_DISP_CLASS                                              0x00008870
#define NVA3_DISP_CLASS                                              0x00008570
#define NVD0_DISP_CLASS                                              0x00009070
#define NVE0_DISP_CLASS                                              0x00009170
#define NVF0_DISP_CLASS                                              0x00009270
#define GM107_DISP_CLASS                                             0x00009470

#define NV50_DISP_MTHD                                               0x00000000
#define NV50_DISP_MTHD_HEAD                                          0x00000003

#define NV50_DISP_SCANOUTPOS                                         0x00000000

#define NV50_DISP_SOR_MTHD                                           0x00010000
#define NV50_DISP_SOR_MTHD_TYPE                                      0x0000f000
#define NV50_DISP_SOR_MTHD_HEAD                                      0x00000018
#define NV50_DISP_SOR_MTHD_LINK                                      0x00000004
#define NV50_DISP_SOR_MTHD_OR                                        0x00000003

#define NV50_DISP_SOR_PWR                                            0x00010000
#define NV50_DISP_SOR_PWR_STATE                                      0x00000001
#define NV50_DISP_SOR_PWR_STATE_ON                                   0x00000001
#define NV50_DISP_SOR_PWR_STATE_OFF                                  0x00000000
#define NVA3_DISP_SOR_HDA_ELD                                        0x00010100
#define NV84_DISP_SOR_HDMI_PWR                                       0x00012000
#define NV84_DISP_SOR_HDMI_PWR_STATE                                 0x40000000
#define NV84_DISP_SOR_HDMI_PWR_STATE_OFF                             0x00000000
#define NV84_DISP_SOR_HDMI_PWR_STATE_ON                              0x40000000
#define NV84_DISP_SOR_HDMI_PWR_MAX_AC_PACKET                         0x001f0000
#define NV84_DISP_SOR_HDMI_PWR_REKEY                                 0x0000007f
#define NV50_DISP_SOR_LVDS_SCRIPT                                    0x00013000
#define NV50_DISP_SOR_LVDS_SCRIPT_ID                                 0x0000ffff
#define NV94_DISP_SOR_DP_PWR                                         0x00016000
#define NV94_DISP_SOR_DP_PWR_STATE                                   0x00000001
#define NV94_DISP_SOR_DP_PWR_STATE_OFF                               0x00000000
#define NV94_DISP_SOR_DP_PWR_STATE_ON                                0x00000001

#define NV50_DISP_DAC_MTHD                                           0x00020000
#define NV50_DISP_DAC_MTHD_TYPE                                      0x0000f000
#define NV50_DISP_DAC_MTHD_OR                                        0x00000003

#define NV50_DISP_DAC_PWR                                            0x00020000
#define NV50_DISP_DAC_PWR_HSYNC                                      0x00000001
#define NV50_DISP_DAC_PWR_HSYNC_ON                                   0x00000000
#define NV50_DISP_DAC_PWR_HSYNC_LO                                   0x00000001
#define NV50_DISP_DAC_PWR_VSYNC                                      0x00000004
#define NV50_DISP_DAC_PWR_VSYNC_ON                                   0x00000000
#define NV50_DISP_DAC_PWR_VSYNC_LO                                   0x00000004
#define NV50_DISP_DAC_PWR_DATA                                       0x00000010
#define NV50_DISP_DAC_PWR_DATA_ON                                    0x00000000
#define NV50_DISP_DAC_PWR_DATA_LO                                    0x00000010
#define NV50_DISP_DAC_PWR_STATE                                      0x00000040
#define NV50_DISP_DAC_PWR_STATE_ON                                   0x00000000
#define NV50_DISP_DAC_PWR_STATE_OFF                                  0x00000040
#define NV50_DISP_DAC_LOAD                                           0x00020100
#define NV50_DISP_DAC_LOAD_VALUE                                     0x00000007

#define NV50_DISP_PIOR_MTHD                                          0x00030000
#define NV50_DISP_PIOR_MTHD_TYPE                                     0x0000f000
#define NV50_DISP_PIOR_MTHD_OR                                       0x00000003

#define NV50_DISP_PIOR_PWR                                           0x00030000
#define NV50_DISP_PIOR_PWR_STATE                                     0x00000001
#define NV50_DISP_PIOR_PWR_STATE_ON                                  0x00000001
#define NV50_DISP_PIOR_PWR_STATE_OFF                                 0x00000000
#define NV50_DISP_PIOR_TMDS_PWR                                      0x00032000
#define NV50_DISP_PIOR_TMDS_PWR_STATE                                0x00000001
#define NV50_DISP_PIOR_TMDS_PWR_STATE_ON                             0x00000001
#define NV50_DISP_PIOR_TMDS_PWR_STATE_OFF                            0x00000000
#define NV50_DISP_PIOR_DP_PWR                                        0x00036000
#define NV50_DISP_PIOR_DP_PWR_STATE                                  0x00000001
#define NV50_DISP_PIOR_DP_PWR_STATE_ON                               0x00000001
#define NV50_DISP_PIOR_DP_PWR_STATE_OFF                              0x00000000

struct nv50_display_class {
};

/* 507a: NV50_DISP_CURS
 * 827a: NV84_DISP_CURS
 * 837a: NVA0_DISP_CURS
 * 887a: NV94_DISP_CURS
 * 857a: NVA3_DISP_CURS
 * 907a: NVD0_DISP_CURS
 * 917a: NVE0_DISP_CURS
 * 927a: NVF0_DISP_CURS
 * 947a: GM107_DISP_CURS
 */

#define NV50_DISP_CURS_CLASS                                         0x0000507a
#define NV84_DISP_CURS_CLASS                                         0x0000827a
#define NVA0_DISP_CURS_CLASS                                         0x0000837a
#define NV94_DISP_CURS_CLASS                                         0x0000887a
#define NVA3_DISP_CURS_CLASS                                         0x0000857a
#define NVD0_DISP_CURS_CLASS                                         0x0000907a
#define NVE0_DISP_CURS_CLASS                                         0x0000917a
#define NVF0_DISP_CURS_CLASS                                         0x0000927a
#define GM107_DISP_CURS_CLASS                                        0x0000947a

struct nv50_display_curs_class {
	u32 head;
};

/* 507b: NV50_DISP_OIMM
 * 827b: NV84_DISP_OIMM
 * 837b: NVA0_DISP_OIMM
 * 887b: NV94_DISP_OIMM
 * 857b: NVA3_DISP_OIMM
 * 907b: NVD0_DISP_OIMM
 * 917b: NVE0_DISP_OIMM
 * 927b: NVE0_DISP_OIMM
 * 947b: GM107_DISP_OIMM
 */

#define NV50_DISP_OIMM_CLASS                                         0x0000507b
#define NV84_DISP_OIMM_CLASS                                         0x0000827b
#define NVA0_DISP_OIMM_CLASS                                         0x0000837b
#define NV94_DISP_OIMM_CLASS                                         0x0000887b
#define NVA3_DISP_OIMM_CLASS                                         0x0000857b
#define NVD0_DISP_OIMM_CLASS                                         0x0000907b
#define NVE0_DISP_OIMM_CLASS                                         0x0000917b
#define NVF0_DISP_OIMM_CLASS                                         0x0000927b
#define GM107_DISP_OIMM_CLASS                                        0x0000947b

struct nv50_display_oimm_class {
	u32 head;
};

/* 507c: NV50_DISP_SYNC
 * 827c: NV84_DISP_SYNC
 * 837c: NVA0_DISP_SYNC
 * 887c: NV94_DISP_SYNC
 * 857c: NVA3_DISP_SYNC
 * 907c: NVD0_DISP_SYNC
 * 917c: NVE0_DISP_SYNC
 * 927c: NVF0_DISP_SYNC
 * 947c: GM107_DISP_SYNC
 */

#define NV50_DISP_SYNC_CLASS                                         0x0000507c
#define NV84_DISP_SYNC_CLASS                                         0x0000827c
#define NVA0_DISP_SYNC_CLASS                                         0x0000837c
#define NV94_DISP_SYNC_CLASS                                         0x0000887c
#define NVA3_DISP_SYNC_CLASS                                         0x0000857c
#define NVD0_DISP_SYNC_CLASS                                         0x0000907c
#define NVE0_DISP_SYNC_CLASS                                         0x0000917c
#define NVF0_DISP_SYNC_CLASS                                         0x0000927c
#define GM107_DISP_SYNC_CLASS                                        0x0000947c

struct nv50_display_sync_class {
	u32 pushbuf;
	u32 head;
};

/* 507d: NV50_DISP_MAST
 * 827d: NV84_DISP_MAST
 * 837d: NVA0_DISP_MAST
 * 887d: NV94_DISP_MAST
 * 857d: NVA3_DISP_MAST
 * 907d: NVD0_DISP_MAST
 * 917d: NVE0_DISP_MAST
 * 927d: NVF0_DISP_MAST
 * 947d: GM107_DISP_MAST
 */

#define NV50_DISP_MAST_CLASS                                         0x0000507d
#define NV84_DISP_MAST_CLASS                                         0x0000827d
#define NVA0_DISP_MAST_CLASS                                         0x0000837d
#define NV94_DISP_MAST_CLASS                                         0x0000887d
#define NVA3_DISP_MAST_CLASS                                         0x0000857d
#define NVD0_DISP_MAST_CLASS                                         0x0000907d
#define NVE0_DISP_MAST_CLASS                                         0x0000917d
#define NVF0_DISP_MAST_CLASS                                         0x0000927d
#define GM107_DISP_MAST_CLASS                                        0x0000947d

struct nv50_display_mast_class {
	u32 pushbuf;
};

/* 507e: NV50_DISP_OVLY
 * 827e: NV84_DISP_OVLY
 * 837e: NVA0_DISP_OVLY
 * 887e: NV94_DISP_OVLY
 * 857e: NVA3_DISP_OVLY
 * 907e: NVD0_DISP_OVLY
 * 917e: NVE0_DISP_OVLY
 * 927e: NVF0_DISP_OVLY
 * 947e: GM107_DISP_OVLY
 */

#define NV50_DISP_OVLY_CLASS                                         0x0000507e
#define NV84_DISP_OVLY_CLASS                                         0x0000827e
#define NVA0_DISP_OVLY_CLASS                                         0x0000837e
#define NV94_DISP_OVLY_CLASS                                         0x0000887e
#define NVA3_DISP_OVLY_CLASS                                         0x0000857e
#define NVD0_DISP_OVLY_CLASS                                         0x0000907e
#define NVE0_DISP_OVLY_CLASS                                         0x0000917e
#define NVF0_DISP_OVLY_CLASS                                         0x0000927e
#define GM107_DISP_OVLY_CLASS                                        0x0000947e

struct nv50_display_ovly_class {
	u32 pushbuf;
	u32 head;
};

#endif

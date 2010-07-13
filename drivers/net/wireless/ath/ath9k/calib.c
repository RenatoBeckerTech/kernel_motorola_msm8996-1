/*
 * Copyright (c) 2008-2009 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "hw.h"
#include "hw-ops.h"

/* Common calibration code */

/* We can tune this as we go by monitoring really low values */
#define ATH9K_NF_TOO_LOW	-60

/* AR5416 may return very high value (like -31 dBm), in those cases the nf
 * is incorrect and we should use the static NF value. Later we can try to
 * find out why they are reporting these values */

static bool ath9k_hw_nf_in_range(struct ath_hw *ah, s16 nf)
{
	if (nf > ATH9K_NF_TOO_LOW) {
		ath_print(ath9k_hw_common(ah), ATH_DBG_CALIBRATE,
			  "noise floor value detected (%d) is "
			  "lower than what we think is a "
			  "reasonable value (%d)\n",
			  nf, ATH9K_NF_TOO_LOW);
		return false;
	}
	return true;
}

static int16_t ath9k_hw_get_nf_hist_mid(int16_t *nfCalBuffer)
{
	int16_t nfval;
	int16_t sort[ATH9K_NF_CAL_HIST_MAX];
	int i, j;

	for (i = 0; i < ATH9K_NF_CAL_HIST_MAX; i++)
		sort[i] = nfCalBuffer[i];

	for (i = 0; i < ATH9K_NF_CAL_HIST_MAX - 1; i++) {
		for (j = 1; j < ATH9K_NF_CAL_HIST_MAX - i; j++) {
			if (sort[j] > sort[j - 1]) {
				nfval = sort[j];
				sort[j] = sort[j - 1];
				sort[j - 1] = nfval;
			}
		}
	}
	nfval = sort[(ATH9K_NF_CAL_HIST_MAX - 1) >> 1];

	return nfval;
}

static void ath9k_hw_update_nfcal_hist_buffer(struct ath9k_nfcal_hist *h,
					      int16_t *nfarray)
{
	int i;

	for (i = 0; i < NUM_NF_READINGS; i++) {
		h[i].nfCalBuffer[h[i].currIndex] = nfarray[i];

		if (++h[i].currIndex >= ATH9K_NF_CAL_HIST_MAX)
			h[i].currIndex = 0;

		if (h[i].invalidNFcount > 0) {
			h[i].invalidNFcount--;
			h[i].privNF = nfarray[i];
		} else {
			h[i].privNF =
				ath9k_hw_get_nf_hist_mid(h[i].nfCalBuffer);
		}
	}
}

static bool ath9k_hw_get_nf_thresh(struct ath_hw *ah,
				   enum ieee80211_band band,
				   int16_t *nft)
{
	switch (band) {
	case IEEE80211_BAND_5GHZ:
		*nft = (int8_t)ah->eep_ops->get_eeprom(ah, EEP_NFTHRESH_5);
		break;
	case IEEE80211_BAND_2GHZ:
		*nft = (int8_t)ah->eep_ops->get_eeprom(ah, EEP_NFTHRESH_2);
		break;
	default:
		BUG_ON(1);
		return false;
	}

	return true;
}

void ath9k_hw_reset_calibration(struct ath_hw *ah,
				struct ath9k_cal_list *currCal)
{
	int i;

	ath9k_hw_setup_calibration(ah, currCal);

	currCal->calState = CAL_RUNNING;

	for (i = 0; i < AR5416_MAX_CHAINS; i++) {
		ah->meas0.sign[i] = 0;
		ah->meas1.sign[i] = 0;
		ah->meas2.sign[i] = 0;
		ah->meas3.sign[i] = 0;
	}

	ah->cal_samples = 0;
}

/* This is done for the currently configured channel */
bool ath9k_hw_reset_calvalid(struct ath_hw *ah)
{
	struct ath_common *common = ath9k_hw_common(ah);
	struct ieee80211_conf *conf = &common->hw->conf;
	struct ath9k_cal_list *currCal = ah->cal_list_curr;

	if (!ah->curchan)
		return true;

	if (!AR_SREV_9100(ah) && !AR_SREV_9160_10_OR_LATER(ah))
		return true;

	if (currCal == NULL)
		return true;

	if (currCal->calState != CAL_DONE) {
		ath_print(common, ATH_DBG_CALIBRATE,
			  "Calibration state incorrect, %d\n",
			  currCal->calState);
		return true;
	}

	if (!ath9k_hw_iscal_supported(ah, currCal->calData->calType))
		return true;

	ath_print(common, ATH_DBG_CALIBRATE,
		  "Resetting Cal %d state for channel %u\n",
		  currCal->calData->calType, conf->channel->center_freq);

	ah->curchan->CalValid &= ~currCal->calData->calType;
	currCal->calState = CAL_WAITING;

	return false;
}
EXPORT_SYMBOL(ath9k_hw_reset_calvalid);

void ath9k_hw_start_nfcal(struct ath_hw *ah)
{
	REG_SET_BIT(ah, AR_PHY_AGC_CONTROL,
		    AR_PHY_AGC_CONTROL_ENABLE_NF);
	REG_SET_BIT(ah, AR_PHY_AGC_CONTROL,
		    AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
	REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
}

void ath9k_hw_loadnf(struct ath_hw *ah, struct ath9k_channel *chan)
{
	struct ath9k_nfcal_hist *h;
	unsigned i, j;
	int32_t val;
	u8 chainmask;
	struct ath_common *common = ath9k_hw_common(ah);

	if (AR_SREV_9300_20_OR_LATER(ah))
		chainmask = 0x3F;
	else if (AR_SREV_9285(ah) || AR_SREV_9271(ah))
		chainmask = 0x9;
	else if (AR_SREV_9280(ah) || AR_SREV_9287(ah)) {
		if ((ah->rxchainmask & 0x2) || (ah->rxchainmask & 0x4))
			chainmask = 0x1B;
		else
			chainmask = 0x09;
	} else {
		if (ah->rxchainmask & 0x4)
			chainmask = 0x3F;
		else if (ah->rxchainmask & 0x2)
			chainmask = 0x1B;
		else
			chainmask = 0x09;
	}
	h = ah->nfCalHist;

	for (i = 0; i < NUM_NF_READINGS; i++) {
		if (chainmask & (1 << i)) {
			val = REG_READ(ah, ah->nf_regs[i]);
			val &= 0xFFFFFE00;
			val |= (((u32) (h[i].privNF) << 1) & 0x1ff);
			REG_WRITE(ah, ah->nf_regs[i], val);
		}
	}

	/*
	 * Load software filtered NF value into baseband internal minCCApwr
	 * variable.
	 */
	REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL,
		    AR_PHY_AGC_CONTROL_ENABLE_NF);
	REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL,
		    AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
	REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);

	/*
	 * Wait for load to complete, should be fast, a few 10s of us.
	 * The max delay was changed from an original 250us to 10000us
	 * since 250us often results in NF load timeout and causes deaf
	 * condition during stress testing 12/12/2009
	 */
	for (j = 0; j < 1000; j++) {
		if ((REG_READ(ah, AR_PHY_AGC_CONTROL) &
		     AR_PHY_AGC_CONTROL_NF) == 0)
			break;
		udelay(10);
	}

	/*
	 * We timed out waiting for the noisefloor to load, probably due to an
	 * in-progress rx. Simply return here and allow the load plenty of time
	 * to complete before the next calibration interval.  We need to avoid
	 * trying to load -50 (which happens below) while the previous load is
	 * still in progress as this can cause rx deafness. Instead by returning
	 * here, the baseband nf cal will just be capped by our present
	 * noisefloor until the next calibration timer.
	 */
	if (j == 1000) {
		ath_print(common, ATH_DBG_ANY, "Timeout while waiting for nf "
			  "to load: AR_PHY_AGC_CONTROL=0x%x\n",
			  REG_READ(ah, AR_PHY_AGC_CONTROL));
		return;
	}

	/*
	 * Restore maxCCAPower register parameter again so that we're not capped
	 * by the median we just loaded.  This will be initial (and max) value
	 * of next noise floor calibration the baseband does.
	 */
	ENABLE_REGWRITE_BUFFER(ah);
	for (i = 0; i < NUM_NF_READINGS; i++) {
		if (chainmask & (1 << i)) {
			val = REG_READ(ah, ah->nf_regs[i]);
			val &= 0xFFFFFE00;
			val |= (((u32) (-50) << 1) & 0x1ff);
			REG_WRITE(ah, ah->nf_regs[i], val);
		}
	}
	REGWRITE_BUFFER_FLUSH(ah);
	DISABLE_REGWRITE_BUFFER(ah);
}


static void ath9k_hw_nf_sanitize(struct ath_hw *ah, s16 *nf)
{
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath_nf_limits *limit;
	int i;

	if (IS_CHAN_2GHZ(ah->curchan))
		limit = &ah->nf_2g;
	else
		limit = &ah->nf_5g;

	for (i = 0; i < NUM_NF_READINGS; i++) {
		if (!nf[i])
			continue;

		ath_print(common, ATH_DBG_CALIBRATE,
			  "NF calibrated [%s] [chain %d] is %d\n",
			  (i > 3 ? "ext" : "ctl"), i % 3, nf[i]);

		if (nf[i] > limit->max) {
			ath_print(common, ATH_DBG_CALIBRATE,
				  "NF[%d] (%d) > MAX (%d), correcting to MAX",
				  i, nf[i], limit->max);
			nf[i] = limit->max;
		} else if (nf[i] < limit->min) {
			ath_print(common, ATH_DBG_CALIBRATE,
				  "NF[%d] (%d) < MIN (%d), correcting to NOM",
				  i, nf[i], limit->min);
			nf[i] = limit->nominal;
		}
	}
}

int16_t ath9k_hw_getnf(struct ath_hw *ah,
		       struct ath9k_channel *chan)
{
	struct ath_common *common = ath9k_hw_common(ah);
	int16_t nf, nfThresh;
	int16_t nfarray[NUM_NF_READINGS] = { 0 };
	struct ath9k_nfcal_hist *h;
	struct ieee80211_channel *c = chan->chan;

	chan->channelFlags &= (~CHANNEL_CW_INT);
	if (REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) {
		ath_print(common, ATH_DBG_CALIBRATE,
			  "NF did not complete in calibration window\n");
		nf = 0;
		chan->rawNoiseFloor = nf;
		return chan->rawNoiseFloor;
	} else {
		ath9k_hw_do_getnf(ah, nfarray);
		ath9k_hw_nf_sanitize(ah, nfarray);
		nf = nfarray[0];
		if (ath9k_hw_get_nf_thresh(ah, c->band, &nfThresh)
		    && nf > nfThresh) {
			ath_print(common, ATH_DBG_CALIBRATE,
				  "noise floor failed detected; "
				  "detected %d, threshold %d\n",
				  nf, nfThresh);
			chan->channelFlags |= CHANNEL_CW_INT;
		}
	}

	h = ah->nfCalHist;

	ath9k_hw_update_nfcal_hist_buffer(h, nfarray);
	chan->rawNoiseFloor = h[0].privNF;

	return chan->rawNoiseFloor;
}

void ath9k_init_nfcal_hist_buffer(struct ath_hw *ah)
{
	struct ath_nf_limits *limit;
	int i, j;

	if (!ah->curchan || IS_CHAN_2GHZ(ah->curchan))
		limit = &ah->nf_2g;
	else
		limit = &ah->nf_5g;

	for (i = 0; i < NUM_NF_READINGS; i++) {
		ah->nfCalHist[i].currIndex = 0;
		ah->nfCalHist[i].privNF = limit->nominal;
		ah->nfCalHist[i].invalidNFcount =
			AR_PHY_CCA_FILTERWINDOW_LENGTH;
		for (j = 0; j < ATH9K_NF_CAL_HIST_MAX; j++) {
			ah->nfCalHist[i].nfCalBuffer[j] = limit->nominal;
		}
	}
}

s16 ath9k_hw_getchan_noise(struct ath_hw *ah, struct ath9k_channel *chan)
{
	s16 nf;

	if (chan->rawNoiseFloor == 0)
		nf = -96;
	else
		nf = chan->rawNoiseFloor;

	if (!ath9k_hw_nf_in_range(ah, nf))
		nf = ATH_DEFAULT_NOISE_FLOOR;

	return nf;
}
EXPORT_SYMBOL(ath9k_hw_getchan_noise);

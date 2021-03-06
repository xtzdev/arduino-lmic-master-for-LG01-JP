/*
* Copyright (c) 2014-2016 IBM Corporation.
* Copyright (c) 2017 MCCI Corporation.
* All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  * Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*  * Neither the name of the <organization> nor the
*    names of its contributors may be used to endorse or promote products
*    derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define LMIC_DR_LEGACY 0

#include "lmic_bandplan.h"

#if CFG_LMIC_EU_like

void  LMIC_enableSubBand(u1_t band) {
}

void  LMIC_disableSubBand(u1_t band) {
}

void LMIC_disableChannel(u1_t channel) {
        LMIC.channelFreq[channel] = 0;
        LMIC.channelDrMap[channel] = 0;
        LMIC.channelMap &= ~(1 << channel);
}

// this is a no-op provided for compatibilty
void LMIC_enableChannel(u1_t channel) {
}

u1_t LMICeulike_mapChannels(u1_t chpage, u2_t chmap) {
        // Bad page, disable all channel, enable non-existent
        if (chpage != 0 || chmap == 0 || (chmap & ~LMIC.channelMap) != 0)
                return 0;  // illegal input
        for (u1_t chnl = 0; chnl<MAX_CHANNELS; chnl++) {
                if ((chmap & (1 << chnl)) != 0 && LMIC.channelFreq[chnl] == 0)
                        chmap &= ~(1 << chnl); // ignore - channel is not defined
        }
        LMIC.channelMap = chmap;
        return 1;
}

#if !defined(DISABLE_JOIN)
void LMICeulike_initJoinLoop(uint8_t nDefaultChannels, s1_t adrTxPow) {

#if defined(FOR_LG01_GW)
	LMIC.txChnl = 0;
#else
	#if CFG_TxContinuousMode
        LMIC.txChnl = 0
	#else
        LMIC.txChnl = os_getRndU1() % nDefaultChannels;
	#endif

#endif
        LMIC.adrTxPow = adrTxPow;
        // TODO(tmm@mcci.com) don't use EU directly, use a table. That
        // will allow support for EU-style bandplans with similar code.
        LMICcore_setDrJoin(DRCHG_SET, LMICbandplan_getInitialDrJoin());
        LMICbandplan_initDefaultChannels(/* put into join mode */ 1);
        ASSERT((LMIC.opmode & OP_NEXTCHNL) == 0);
        LMIC.txend = os_getTime() + LMICcore_rndDelay(8);
}
#endif // DISABLE_JOIN

void LMICeulike_updateTx(ostime_t txbeg) {
        u4_t freq = LMIC.channelFreq[LMIC.txChnl];
        // Update global/band specific duty cycle stats
        ostime_t airtime = calcAirTime(LMIC.rps, LMIC.dataLen);
        // Update channel/global duty cycle stats
        xref2band_t band = &LMIC.bands[freq & 0x3];
        LMIC.freq = freq & ~(u4_t)3;
        LMIC.txpow = band->txpow;
        band->avail = txbeg + airtime * band->txcap;
        if (LMIC.globalDutyRate != 0)
                LMIC.globalDutyAvail = txbeg + (airtime << LMIC.globalDutyRate);
}

#if !defined(DISABLE_JOIN)
ostime_t LMICeulike_nextJoinState(uint8_t nDefaultChannels) {
        u1_t failed = 0;

        // Try each default channel with same DR
        // If all fail try next lower datarate
		
		#if defined(FOR_LG01_GW)
		//	Don't change SF for LG01
		#else
        	if (++LMIC.txChnl == /* NUM_DEFAULT_CHANNELS */ nDefaultChannels)
                LMIC.txChnl = 0;
        	if ((++LMIC.txCnt % nDefaultChannels) == 0) {
                // Lower DR every nth try (having all default channels with same DR)
                if (LMIC.datarate == LORAWAN_DR0)
                        failed = 1; // we have tried all DR - signal EV_JOIN_FAILED
                else
                        LMICcore_setDrJoin(DRCHG_NOJACC, decDR((dr_t)LMIC.datarate));
        	}
		#endif
        // Clear NEXTCHNL because join state engine controls channel hopping
        LMIC.opmode &= ~OP_NEXTCHNL;
        // Move txend to randomize synchronized concurrent joins.
        // Duty cycle is based on txend.
        ostime_t const time = LMICbandplan_nextJoinTime(os_getTime());
        LMIC.txend = time +
                (isTESTMODE()
                        // Avoid collision with JOIN ACCEPT @ SF12 being sent by GW (but we missed it)
                        ? DNW2_SAFETY_ZONE
                        // Otherwise: randomize join (street lamp case):
                        // SF12:255, SF11:127, .., SF7:8secs
                        : DNW2_SAFETY_ZONE + LMICcore_rndDelay(255 >> LMIC.datarate));
        // 1 - triggers EV_JOIN_FAILED event
        return failed;
}
#endif // !DISABLE_JOIN

#endif // CFG_LMIC_EU_like

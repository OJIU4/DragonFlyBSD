/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD: src/sys/dev/ath/ath_rate/onoe/onoe.h,v 1.2 2004/12/31 22:41:45 sam Exp $
 * $DragonFly: src/sys/netproto/802_11/wlan_ratectl/onoe/Attic/ieee80211_ratectl_onoe.h,v 1.1 2006/09/01 15:12:12 sephe Exp $
 */

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _IEEE80211_RATECTL_ONOE_H
#define _IEEE80211_RATECTL_ONOE_H

struct onoe_softc {
	struct ieee80211com	*ic;
	struct callout		timer;		/* periodic timer */

	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_oid;
	int			interval;	/* rate ctl interval (ms) */
	int			raise;		/* add credit threshold */
	int			raise_threshold;/* rate ctl raise threshold */
	int			debug;
};

struct onoe_data {
	u_int		od_tx_ok;	/* tx ok pkt */
	u_int		od_tx_err;	/* tx !ok pkt */
	u_int		od_tx_retr;	/* tx retry count */
	int		od_tx_upper;	/* tx upper rate req cnt */
};

#endif /* !_IEEE80211_RATECTL_ONOE_H */

/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _osl_h_
#define _osl_h_

/* Drivers use PKTFREESETCB to register a callback function
   when a packet is freed by OSL */
typedef void (*pktfree_cb_fn_t) (void *ctx, void *pkt, unsigned int status);

struct osl_pubinfo {
	uint pktalloced;	/* Number of allocated packet buffers */
	bool mmbus;		/* Bus supports memory-mapped registers */
	pktfree_cb_fn_t tx_fn;	/* Callback function for PKTFREE */
	void *tx_ctx;		/* Context to the callback function */
#if defined(BCMSDIO) && !defined(BRCM_FULLMAC)
	osl_rreg_fn_t rreg_fn;	/* Read Register function */
	osl_wreg_fn_t wreg_fn;	/* Write Register function */
	void *reg_ctx;		/* Context to the reg callback functions */
#endif
};

/* osl handle type forward declaration */
struct osl_info {
	struct osl_pubinfo pub;
	uint magic;
	void *pdev;
	uint bustype;
};

typedef struct osl_dmainfo osldma_t;


#ifdef BCMSDIO
/* Drivers use REGOPSSET() to register register read/write funcitons */
typedef unsigned int (*osl_rreg_fn_t) (void *ctx, void *reg, unsigned int size);
typedef void (*osl_wreg_fn_t) (void *ctx, void *reg, unsigned int val,
			       unsigned int size);
#endif

#include <linux_osl.h>

/* --------------------------------------------------------------------------
** Register manipulation macros.
*/

#define	SET_REG(osh, r, mask, val)	W_REG((osh), (r), ((R_REG((osh), r) & ~(mask)) | (val)))

#ifndef AND_REG
#define AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#endif				/* !AND_REG */

#ifndef OR_REG
#define OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))
#endif				/* !OR_REG */

#if !defined(OSL_SYSUPTIME)
#define OSL_SYSUPTIME() (0)
#define OSL_SYSUPTIME_SUPPORT false
#else
#define OSL_SYSUPTIME_SUPPORT true
#endif				/* OSL_SYSUPTIME */

#endif				/* _osl_h_ */

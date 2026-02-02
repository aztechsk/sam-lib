/*
 * rstc.c
 *
 * Copyright (c) 2024 Jan Rusnak <jan@rusnak.sk>
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <gentyp.h>
#include "sysconf.h"
#include "board.h"
#include <mmio.h>
#include "criterr.h"
#include "msgconf.h"
#include "rstc.h"

#define MR_KEY (RSTC_MR_KEY_Msk & (0xA5 << RSTC_MR_KEY_Pos))
#define CR_KEY (RSTC_CR_KEY_Msk & (0xA5 << RSTC_CR_KEY_Pos))

#if TERMOUT == 1
static const char *const rst_type_arr[] = {"PWR", "BKP", "WTD", "SW", "USR"};
#endif

static unsigned int sr;

/**
 * init_rstc
 */
void init_rstc(void)
{
	sr = RSTC->RSTC_SR;
#if RSTC_USER_RESET_ENABLED == 1
	RSTC->RSTC_MR = MR_KEY | RSTC_MR_ERSTL(RSTC_EXT_RESET_LENGTH) |
		        RSTC_MR_URSTEN;
#else
	RSTC->RSTC_MR = MR_KEY | RSTC_MR_ERSTL(RSTC_EXT_RESET_LENGTH);
#endif
}

/**
 * rst_cause
 */
int rst_cause(void)
{
	return ((sr & RSTC_SR_RSTTYP_Msk) >> RSTC_SR_RSTTYP_Pos);
}

/**
 * soft_rst
 */
void soft_rst(void)
{
	while (RSTC->RSTC_SR & RSTC_SR_SRCMP) {
		;
	}
	RSTC->RSTC_CR = CR_KEY |
	                RSTC_CR_EXTRST |
			RSTC_CR_PERRST |
	                RSTC_CR_PROCRST;
}

#if TERMOUT == 1
/**
 * rst_cause_str
 */
const char *rst_cause_str(void)
{
	int i = (sr & RSTC_SR_RSTTYP_Msk) >> RSTC_SR_RSTTYP_Pos;

	if (i < 5) {
		return (rst_type_arr[i]);
	} else {
		return ("err");
	}
}

/**
 * log_rst_cause
 */
void log_rst_cause(void)
{
	msg(INF, "rstc.c: %s reset\n", rst_cause_str());
}
#endif

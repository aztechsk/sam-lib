/*
 * supc.c
 *
 * Copyright (c) 2020 Jan Rusnak <jan@rusnak.sk>
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
#include "supc.h"

static unsigned int sr;

/**
 * init_supc
 */
void init_supc(void)
{
	sr = SUPC->SUPC_SR;
}

/**
 * enable_bod_rst
 */
void enable_bod_rst(void)
{
	unsigned int mr;

	mr = SUPC->SUPC_MR;
	mr &= ~SUPC_MR_BODDIS;
	mr |= 0xA5 << 24 | SUPC_MR_BODRSTEN;
	SUPC->SUPC_MR = mr;
}

/**
 * disable_bod_rst
 */
void disable_bod_rst(void)
{
	unsigned int mr;

	mr = SUPC->SUPC_MR;
	mr &= ~SUPC_MR_BODRSTEN;
	mr |= 0xA5 << 24 | SUPC_MR_BODDIS;
	SUPC->SUPC_MR = mr;
}

/**
 * bod_rst_stat
 */
boolean_t bod_rst_stat(void)
{
	if (sr & SUPC_SR_BODRSTS_PRESENT) {
		return (TRUE);
	} else {
		return (FALSE);
	}
}

/**
 * disable_emb_vreg
 */
void disable_emb_vreg(void)
{
	SUPC->SUPC_MR = 0xA5 << 24 | (SUPC->SUPC_MR & ~SUPC_MR_ONREG);
}

/**
 * stop_vreg_rst_core
 */
void stop_vreg_rst_core(void)
{
	SUPC->SUPC_CR = 0xA5 << 24 | SUPC_CR_VROFF_STOP_VREG;
	for (;;);
}

/**
 * enable_sup_mon
 */
void enable_sup_mon(enum supc_sm_smpl smpl, enum supc_sm_th th)
{
	SUPC->SUPC_SMMR = smpl << SUPC_SMMR_SMSMPL_Pos | th << SUPC_SMMR_SMTH_Pos;
}

/**
 * enable_sup_mon_rst
 */
void enable_sup_mon_rst(void)
{
	SUPC->SUPC_SMMR |= SUPC_SMMR_SMRSTEN;
}

/**
 * disable_sup_mon_rst
 */
void disable_sup_mon_rst(void)
{
	SUPC->SUPC_SMMR &= ~SUPC_SMMR_SMRSTEN;
}

/**
 * enable_32k_xtal_osc
 */
void enable_32k_xtal_osc(void)
{
	SUPC->SUPC_MR = 0xA5 << 24 | (SUPC->SUPC_MR & ~SUPC_MR_OSCBYPASS);
	SUPC->SUPC_CR = 0xA5 << 24 | SUPC_CR_XTALSEL;
        while (!(SUPC->SUPC_SR & SUPC_SR_OSCSEL_CRYST)) {
		;
	}
}

#if TERMOUT == 1
/**
 * log_supc_cfg
 */
void log_supc_cfg(void)
{
	unsigned int mr;

	mr = SUPC->SUPC_MR;
	msg(INF, "supc.c: cfg> OSCBYPASS=%d ONREG=%d BODDIS=%d BODRSTEN=%d SMSMPL=%d\n",
	    (mr & SUPC_MR_OSCBYPASS) ? 1 : 0,
	    (mr & SUPC_MR_ONREG) ? 1 : 0,
            (mr & SUPC_MR_BODDIS) ? 1 : 0,
	    (mr & SUPC_MR_BODRSTEN) ? 1 : 0,
	    (SUPC->SUPC_SMMR & SUPC_SMMR_SMSMPL_Msk) >> SUPC_SMMR_SMSMPL_Pos);
}

/**
 * log_supc_rst_stat
 */
void log_supc_rst_stat(void)
{
	msg(INF, "supc.c: SMRSTS=%d BODRSTS=%d\n",
	    (sr & SUPC_SR_SMRSTS_PRESENT) ? 1 : 0,
	    (sr & SUPC_SR_BODRSTS_PRESENT) ? 1 : 0);
}
#endif

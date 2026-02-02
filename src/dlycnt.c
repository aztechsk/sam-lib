/*
 * dlycnt.c
 *
 * Copyright (c) 2025 Jan Rusnak <jan@rusnak.sk>
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
#include "tc.h"
#include "criterr.h"
#include "msgconf.h"
#include "clocks.h"
#include "pmc.h"
#include "dlycnt.h"

#if DLYCNT_US == DLYCNT_US_DWT

static void delay_cycles(uint32_t cycles);

/**
 * init_dlycnt
 */
void init_dlycnt(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * delay_us
 */
void delay_us(uint32_t dly)
{
	if (!dly) {
		return;
	}
	uint64_t ticks = ((uint64_t) dly * SystemCoreClock + 999999) / 1000000;
	while (ticks) {
		uint32_t chunk = (ticks > 0x7FFFFFFF) ? 0x7FFFFFFF : ticks;
		delay_cycles(chunk);
		ticks -= chunk;
	}
}

/**
 * log_dlycnt
 */
void log_dlycnt(void)
{
	msg(INF, "dlycnt.c: cfg> DWT\n");
}

/**
 * delay_cycles
 */
static void delay_cycles(uint32_t cycles)
{
	uint32_t start = DWT->CYCCNT;

	while ((uint32_t) (DWT->CYCCNT - start) < cycles) {
		__NOP();
	}
}

#elif DLYCNT_US == DLYCNT_US_TC

#if (F_MCK % 1000000) != 0
#error "F_MCK is not a multiple of MHz"
#endif

static uint32_t clk_hz;
static uint32_t ticks_per_us;

static inline void delay_ticks_16(uint16_t ticks);
static inline uint16_t tc_cv(void);
static inline uint32_t tcc_divisor(uint32_t tcc);

/**
 * init_dlycnt
 */
void init_dlycnt(void)
{
	enable_periph_clk(DLYCNT_US_TID);
	DLYCNT_US_TDV->TC_CHANNEL[tc_chnl(DLYCNT_US_TID)].TC_CCR = TC_CCR_CLKDIS;
	(void) DLYCNT_US_TDV->TC_CHANNEL[tc_chnl(DLYCNT_US_TID)].TC_SR;
	DLYCNT_US_TDV->TC_CHANNEL[tc_chnl(DLYCNT_US_TID)].TC_IDR = 0xFFFFFFFF;
	DLYCNT_US_TDV->TC_CHANNEL[tc_chnl(DLYCNT_US_TID)].TC_CMR = DLYCNT_US_TCCLKS;
	DLYCNT_US_TDV->TC_CHANNEL[tc_chnl(DLYCNT_US_TID)].TC_CCR = TC_CCR_CLKEN | TC_CCR_SWTRG;
	clk_hz = F_MCK / tcc_divisor(DLYCNT_US_TCCLKS);
	ticks_per_us = (clk_hz + 1000000 - 1) / 1000000;
}

/**
 * delay_us
 */
void delay_us(uint32_t dly)
{
	if (!dly) {
		return;
	}
	uint64_t total = (uint64_t) dly * ticks_per_us;
	uint16_t chunk = 0x7FFF;
	while (total) {
		uint16_t step = (total >= chunk) ? chunk : (uint16_t) total;
		delay_ticks_16(step);
		total -= step;
	}
}

/**
 * log_dlycnt
 */
void log_dlycnt(void)
{
	float resol_us = 1e6F / clk_hz;
	float overf_ms = (65536 / (float) clk_hz) * 1e3F;
	msg(INF, "dlycnt.c: cfg> TC ov=%.3f ms res=%.3f us/tick\n", overf_ms, resol_us);
}

/**
 * delay_ticks_16
 */
static inline void delay_ticks_16(uint16_t ticks)
{
	uint16_t start = tc_cv();
	while ((uint16_t) (tc_cv() - start) < ticks) {
		__NOP();
	}
}

/**
 * tc_cv
 */
static inline uint16_t tc_cv(void)
{
	return (DLYCNT_US_TDV->TC_CHANNEL[tc_chnl(DLYCNT_US_TID)].TC_CV & 0xFFFF);
}

/**
 * tcc_divisor
 */
static inline uint32_t tcc_divisor(uint32_t tcc)
{
	switch (tcc & TC_CMR_TCCLKS_Msk) {
	case TC_CMR_TCCLKS_TIMER_CLOCK1: // MCK/2
		return (2);
	case TC_CMR_TCCLKS_TIMER_CLOCK2: // MCK/8
		return (8);
	case TC_CMR_TCCLKS_TIMER_CLOCK3: // MCK/32
		return (32);
	case TC_CMR_TCCLKS_TIMER_CLOCK4: // MCK/128
		return (128);
	default:
		crit_err_exit(BAD_PARAMETER);
		return (0);
	}
}

#else

/**
 * init_dlycnt
 */
void init_dlycnt(void)
{
	crit_err_exit(UNEXP_PROG_STATE);
}
#endif

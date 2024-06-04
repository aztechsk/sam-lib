/*
 * shift164.c
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
#include "atom.h"
#include "tc.h"
#include "pio.h"
#include "pmc.h"
#include "shift164.h"

#if SHIFT164 == 1

enum state {
	SIG_CP_LOW,
	SIG_CP_HIGH,
#if SHIFT164_OUT_LATCH == 1
	SIG_OL_LOW
#endif
};

static shift164 act_dev;
static SemaphoreHandle_t sig;
static unsigned int data;
static enum state state;
static int bit;

static BaseType_t tc_hndlr(void);

/**
 * init_shift164
 */
void init_shift164(shift164 dev)
{
	if (sig == NULL) {
		if (NULL == (sig = xSemaphoreCreateBinary())) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
	conf_io_pin(dev->cp_pin, dev->cp_cont, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
	conf_io_pin(dev->sd_pin, dev->sd_cont, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
#if SHIFT164_OUT_LATCH == 1
	conf_io_pin(dev->ol_pin, dev->ol_cont, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
#endif
}

/**
 * write_shift164
 */
void write_shift164(shift164 dev, unsigned int r)
{
	act_dev = dev;
	data = r;
        NVIC_DisableIRQ(SHIFT164_TID);
        enable_periph_clk(SHIFT164_TID);
	SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_IDR = ~0;
        NVIC_ClearPendingIRQ(SHIFT164_TID);
	SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_CMR = TC_CMR_CPCTRG |
	                                                         TC_CMR_TCCLKS_TIMER_CLOCK4;
	SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_RC = F_MCK / 128 / 1000 - 1;
        SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_IER = TC_IER_CPCS;
	set_tc_intr_clbk(SHIFT164_TID, tc_hndlr);
        NVIC_SetPriority(SHIFT164_TID, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(SHIFT164_TID);
	if (data & 1) {
		set_pin_lev(act_dev->sd_pin, act_dev->sd_cont, HIGH);
	} else {
		set_pin_lev(act_dev->sd_pin, act_dev->sd_cont, LOW);
	}
	data >>= 1;
        bit = 0;
	state = SIG_CP_HIGH;
        barrier();
        SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN;
	xSemaphoreTake(sig, portMAX_DELAY);
        NVIC_DisableIRQ(SHIFT164_TID);
        disable_periph_clk(SHIFT164_TID);
}

/**
 * tc_hndlr
 */
static BaseType_t tc_hndlr(void)
{
	volatile unsigned int sr;
        BaseType_t tsk_wkn = pdFALSE;

	sr = SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_SR;
	switch (state) {
	case SIG_CP_LOW :
		set_pin_lev(act_dev->cp_pin, act_dev->cp_cont, LOW);
		if (bit < act_dev->size) {
			if (data & 1) {
				set_pin_lev(act_dev->sd_pin, act_dev->sd_cont, HIGH);
			} else {
				set_pin_lev(act_dev->sd_pin, act_dev->sd_cont, LOW);
			}
			data >>= 1;
                        state = SIG_CP_HIGH;
		} else {
#if SHIFT164_OUT_LATCH == 1
			set_pin_lev(act_dev->ol_pin, act_dev->ol_cont, HIGH);
			state = SIG_OL_LOW;
#else
			SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_CCR = TC_CCR_CLKDIS;
			SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_IDR = TC_IDR_CPCS;
			xSemaphoreGiveFromISR(sig, &tsk_wkn);
#endif
		}
		break;
	case SIG_CP_HIGH :
		set_pin_lev(act_dev->cp_pin, act_dev->cp_cont, HIGH);
		bit++;
		state = SIG_CP_LOW;
		break;
#if SHIFT164_OUT_LATCH == 1
	case SIG_OL_LOW :
		set_pin_lev(act_dev->ol_pin, act_dev->ol_cont, LOW);
		SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_CCR = TC_CCR_CLKDIS;
		SHIFT164_TDV->TC_CHANNEL[tc_chnl(SHIFT164_TID)].TC_IDR = TC_IDR_CPCS;
		xSemaphoreGiveFromISR(sig, &tsk_wkn);
		break;
#endif
	}
	return (tsk_wkn);
}
#endif

/*
 * shift165.c
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
#include "atom.h"
#include "tc.h"
#include "pio.h"
#include "pmc.h"
#include "shift165.h"

#if SHIFT165 == 1

enum state {
	LOAD_REG,
	READ_BIT7,
	SIG_CP_LOW,
	SIG_CP_HIGH
};

static shift165 act_dev;
static QueueHandle_t que;
static enum state state;

static BaseType_t tc_hndlr(void);

/**
 * init_shift165
 */
void init_shift165(shift165 dev)
{
	if (que == NULL) {
		que = xQueueCreate(1, sizeof(unsigned int));
		if (que == NULL) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
	conf_io_pin(dev->pl_pin, dev->pl_cont, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_HIGH, PIO_END_OF_FEAT);
	conf_io_pin(dev->cp_pin, dev->cp_cont, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
#if SHIFT165_DRIVE_CE == 1
	conf_io_pin(dev->ce_pin, dev->ce_cont, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
#endif
	conf_io_pin(dev->q_pin, dev->q_cont, PIO_INPUT, PIO_PULL_UP_OFF, PIO_END_OF_FEAT);
}

/**
 * read_shift165
 */
unsigned int read_shift165(shift165 dev)
{
	unsigned int r;

	act_dev = dev;
	NVIC_DisableIRQ(SHIFT165_TID);
        enable_periph_clk(SHIFT165_TID);
	SHIFT165_TDV->TC_CHANNEL[tc_chnl(SHIFT165_TID)].TC_IDR = ~0;
        NVIC_ClearPendingIRQ(SHIFT165_TID);
	SHIFT165_TDV->TC_CHANNEL[tc_chnl(SHIFT165_TID)].TC_CMR = TC_CMR_CPCTRG |
	                                                         TC_CMR_TCCLKS_TIMER_CLOCK4;
	SHIFT165_TDV->TC_CHANNEL[tc_chnl(SHIFT165_TID)].TC_RC = F_MCK / 128 / 1000 - 1;
        SHIFT165_TDV->TC_CHANNEL[tc_chnl(SHIFT165_TID)].TC_IER = TC_IER_CPCS;
	set_tc_intr_clbk(SHIFT165_TID, tc_hndlr);
        NVIC_SetPriority(SHIFT165_TID, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(SHIFT165_TID);
        set_pin_lev(act_dev->pl_pin, act_dev->pl_cont, LOW);
	state = LOAD_REG;
        barrier();
        SHIFT165_TDV->TC_CHANNEL[tc_chnl(SHIFT165_TID)].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN;
        xQueueReceive(que, &r, portMAX_DELAY);
        NVIC_DisableIRQ(SHIFT165_TID);
        disable_periph_clk(SHIFT165_TID);
	return (r);
}

/**
 * tc_hndlr
 */
static BaseType_t tc_hndlr(void)
{
	volatile unsigned int sr;
        BaseType_t tsk_wkn = pdFALSE;
	static unsigned int reg;
	static int bit;

	sr = SHIFT165_TDV->TC_CHANNEL[tc_chnl(SHIFT165_TID)].TC_SR;
	switch (state) {
	case LOAD_REG    :
		set_pin_lev(act_dev->pl_pin, act_dev->pl_cont, HIGH);
                state = READ_BIT7;
		break;
	case READ_BIT7   :
		bit = 1;
		if (get_pin_lev(act_dev->q_pin, act_dev->q_cont)) {
			reg = 1;
		} else {
			reg = 0;
		}
		set_pin_lev(act_dev->cp_pin, act_dev->cp_cont, HIGH);
                state = SIG_CP_LOW;
		break;
	case SIG_CP_LOW  :
		bit++;
		reg <<= 1;
		if (get_pin_lev(act_dev->q_pin, act_dev->q_cont)) {
			reg |= 1;
		}
		set_pin_lev(act_dev->cp_pin, act_dev->cp_cont, LOW);
		if (bit < act_dev->size) {
			state = SIG_CP_HIGH;
		} else {
			SHIFT165_TDV->TC_CHANNEL[tc_chnl(SHIFT165_TID)].TC_CCR = TC_CCR_CLKDIS;
			SHIFT165_TDV->TC_CHANNEL[tc_chnl(SHIFT165_TID)].TC_IDR = TC_IDR_CPCS;
			xQueueSendFromISR(que, &reg, &tsk_wkn);
		}
		break;
	case SIG_CP_HIGH :
		set_pin_lev(act_dev->cp_pin, act_dev->cp_cont, HIGH);
                state = SIG_CP_LOW;
		break;
	}
	return (tsk_wkn);
}
#endif

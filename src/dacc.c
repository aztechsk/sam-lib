/*
 * dacc.c
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
#include "pmc.h"
#include "dacc.h"

#if DACC_FREE_RUN == 1

/**
 * init_dacc
 */
void init_dacc(dacc dev)
{
        NVIC_DisableIRQ(ID_DACC);
        enable_periph_clk(ID_DACC);
	DACC->DACC_CR = DACC_CR_SWRST;
	DACC->DACC_IDR = ~0;
#if DACC_FREE_RUN == 1
#if SAM3S_SERIES
	DACC->DACC_MR = (dev->mr & ~(DACC_MR_MAXS | DACC_MR_SLEEP | DACC_MR_WORD | DACC_MR_TRGEN)) |
	                DACC_MR_TAG;
	DACC->DACC_ACR = dev->acr;
#elif SAM4S_SERIES
	DACC->DACC_MR = (dev->mr & ~(DACC_MR_MAXS | DACC_MR_WORD | DACC_MR_TRGEN)) | DACC_MR_ONE |
	                DACC_MR_TAG;
	DACC->DACC_ACR = dev->acr;
#else
	DACC->DACC_MR = dev->mr & ~(DACC_MR_WORD | DACC_MR_TRGEN);
#endif
#endif
}

/**
 * enable_dacc_chnl
 */
void enable_dacc_chnl(unsigned int cd)
{
	taskENTER_CRITICAL();
#if SAM3S_SERIES || SAM4S_SERIES
	if (cd & DACC_CHNL_1) {
		DACC->DACC_CHER |= DACC_CHER_CH1;
	} else {
		DACC->DACC_CHER |= DACC_CHER_CH0;
	}
        write_dacc_fifo(cd);
#else
	DACC->DACC_MR |= DACC_MR_DACEN;
	write_dacc_fifo(cd);
#endif
	taskEXIT_CRITICAL();
}

/**
 * write_dacc_fifo
 */
void write_dacc_fifo(unsigned int cd)
{
#if SAM3S_SERIES || SAM4S_SERIES
	if (cd & DACC_CHNL_1) {
		cd &= 0xFFF;
		cd |= 1 << 12;
	} else {
		cd &= 0xFFF;
	}
	while (!(DACC->DACC_ISR & DACC_ISR_TXRDY)) {
		;
	}
	DACC->DACC_CDR = cd;
#else
	while (!(DACC->DACC_ISR & DACC_ISR_TXRDY)) {
		;
	}
	DACC->DACC_CDR = cd & 0x3FF;
#endif
}
#endif

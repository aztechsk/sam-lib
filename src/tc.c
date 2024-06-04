/*
 * tc.c
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
#include "tc.h"

#if TMC_TC0 == 1 || TMC_TC1 == 1 || TMC_TC2 == 1 ||\
    TMC_TC3 == 1 || TMC_TC4 == 1 || TMC_TC5 == 1

#if TMC_TC0 == 1 && defined(ID_TC0)
static BaseType_t (*h0)(void);
#endif
#if TMC_TC1 == 1 && defined(ID_TC1)
static BaseType_t (*h1)(void);
#endif
#if TMC_TC2 == 1 && defined(ID_TC2)
static BaseType_t (*h2)(void);
#endif
#if TMC_TC3 == 1 && defined(ID_TC3)
static BaseType_t (*h3)(void);
#endif
#if TMC_TC4 == 1 && defined(ID_TC4)
static BaseType_t (*h4)(void);
#endif
#if TMC_TC5 == 1 && defined(ID_TC5)
static BaseType_t (*h5)(void);
#endif

/**
 * set_tc_intr_clbk
 */
void set_tc_intr_clbk(int chnl_id, BaseType_t (*clbk)(void))
{
	switch (chnl_id) {
#if TMC_TC0 == 1 && defined(ID_TC0)
	case ID_TC0 :
		h0 = clbk;
		break;
#endif
#if TMC_TC1 == 1 && defined(ID_TC1)
	case ID_TC1 :
		h1 = clbk;
		break;
#endif
#if TMC_TC2 == 1 && defined(ID_TC2)
        case ID_TC2 :
		h2 = clbk;
		break;
#endif
#if TMC_TC3 == 1 && defined(ID_TC3)
        case ID_TC3 :
		h3 = clbk;
		break;
#endif
#if TMC_TC4 == 1 && defined(ID_TC4)
        case ID_TC4 :
		h4 = clbk;
		break;
#endif
#if TMC_TC5 == 1 && defined(ID_TC5)
        case ID_TC5 :
		h5 = clbk;
		break;
#endif
	default :
		crit_err_exit(BAD_PARAMETER);
		break;
	}
}

/**
 * tc_chnl
 */
int tc_chnl(int chnl_id)
{
	switch (chnl_id) {
#if TMC_TC0 == 1 && defined(ID_TC0)
	case ID_TC0 :
		return (0);
#endif
#if TMC_TC3 == 1 && defined(ID_TC3)
	case ID_TC3 :
		return (0);
#endif
#if TMC_TC1 == 1 && defined(ID_TC1)
        case ID_TC1 :
		return (1);
#endif
#if TMC_TC4 == 1 && defined(ID_TC4)
        case ID_TC4 :
		return (1);
#endif
#if TMC_TC2 == 1 && defined(ID_TC2)
        case ID_TC2 :
		return (2);
#endif
#if TMC_TC5 == 1 && defined(ID_TC5)
        case ID_TC5 :
		return (2);
#endif
	default :
		crit_err_exit(BAD_PARAMETER);
                return (0);
	}
}

#if TMC_TC0 == 1 && defined(ID_TC0)
/**
 * TC0_Handler
 */
void TC0_Handler(void)
{
	if (pdTRUE == (*h0)()) {
		portEND_SWITCHING_ISR(pdTRUE);
	}
}
#endif

#if TMC_TC1 == 1 && defined(ID_TC1)
/**
 * TC1_Handler
 */
void TC1_Handler(void)
{
	if (pdTRUE == (*h1)()) {
		portEND_SWITCHING_ISR(pdTRUE);
	}
}
#endif

#if TMC_TC2 == 1 && defined(ID_TC2)
/**
 * TC2_Handler
 */
void TC2_Handler(void)
{
	if (pdTRUE == (*h2)()) {
		portEND_SWITCHING_ISR(pdTRUE);
	}
}
#endif

#if TMC_TC3 == 1 && defined(ID_TC3)
/**
 * TC3_Handler
 */
void TC3_Handler(void)
{
	if (pdTRUE == (*h3)()) {
		portEND_SWITCHING_ISR(pdTRUE);
	}
}
#endif

#if TMC_TC4 == 1 && defined(ID_TC4)
/**
 * TC4_Handler
 */
void TC4_Handler(void)
{
	if (pdTRUE == (*h4)()) {
		portEND_SWITCHING_ISR(pdTRUE);
	}
}
#endif

#if TMC_TC5 == 1 && defined(ID_TC5)
/**
 * TC5_Handler
 */
void TC5_Handler(void)
{
	if (pdTRUE == (*h5)()) {
		portEND_SWITCHING_ISR(pdTRUE);
	}
}
#endif

#endif

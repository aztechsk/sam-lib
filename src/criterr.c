/*
 * criterr.c
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
#include "msgconf.h"
#include "pio.h"
#include "wd.h"
#include "tc.h"
#include "pmc.h"
#include "criterr.h"
#include <libarm.h>

#if CRITERR == 1

#define CRITERR_INTR_FREQ 20

static volatile int intr_cnt;
static boolean_t led1_en;

#if TERMOUT == 1
static const char err0[] = "UNEXP_PROG_STATE";
static const char err1[] = "TASK_STACK_OVERFLOW";
static const char err2[] = "MALLOC_ERROR";
static const char err3[] = "BAD_PARAMETER";
static const char err4[] = "APPLICATION_ERROR_1";
static const char err5[] = "APPLICATION_ERROR_2";
static const char err6[] = "APPLICATION_ERROR_3";
static const char err7[] = "HARDWARE_ERROR";
static const char *const errors[] = {err0, err1, err2, err3, err4, err5, err6, err7};
#endif

#if TERMOUT == 1
static char *fname(char *file);
#endif
static void crit_err(enum crit_err err);
static void show_err(enum crit_err err);
static void init_tc_50ms(void);
static BaseType_t tc_hndlr(void);

/**
 * crit_err_exit_fn
 */
void crit_err_exit_fn(enum crit_err err, char *file, int line)
{
#if TERMOUT == 1
	char *p;
#endif
	if (err == TASK_STACK_OVERFLOW) {
		crit_err(TASK_STACK_OVERFLOW);
	}
	switch (xTaskGetSchedulerState()) {
	case taskSCHEDULER_RUNNING :
#if TERMOUT == 1
		vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
		msg(INF, "%s: crit_err_exit(%s) on line %d\n", fname(file),
		    errors[err], line);
		disable_tout();
		vTaskPrioritySet(tout_tsk_hndl(), configMAX_PRIORITIES - 1);
		while (pdTRUE == xQueuePeek(tout_mque(), &p, 0)) {
                        taskYIELD();
                }
		vTaskDelay(250 / portTICK_PERIOD_MS);
#endif
		/* FALLTHRU */
	case taskSCHEDULER_NOT_STARTED :
		/* FALLTHRU */
	case taskSCHEDULER_SUSPENDED :
		/* FALLTHRU */
	default :
		crit_err(err);
		break;
	}
}

#if TERMOUT == 1
/**
 * fname
 */
static char *fname(char *file)
{
	int i = 0, p = 0;

	while (*(file + i) != '\0') {
		if (*(file + i) == '/' || *(file + i) == '\\') {
			p = i;
		}
		i++;
	}
	return ((p) ? file + ++p : file);
}
#endif

/**
 * crit_err
 */
static void crit_err(enum crit_err err)
{
	__enable_irq();
	vPortRaiseBASEPRI();
	init_tc_50ms();
#if defined(LEDUI_ANODE_ON_IO_PIN)
#if LEDUI_ANODE_ON_IO_PIN == 1
	set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, HIGH);
        set_pin_lev(LEDUI2_IO_PIN, LEDUI2_IO_CONT, HIGH);
        set_pin_lev(LEDUI3_IO_PIN, LEDUI3_IO_CONT, HIGH);
        set_pin_lev(LEDUI4_IO_PIN, LEDUI4_IO_CONT, HIGH);
#else
	set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, LOW);
        set_pin_lev(LEDUI2_IO_PIN, LEDUI2_IO_CONT, LOW);
        set_pin_lev(LEDUI3_IO_PIN, LEDUI3_IO_CONT, LOW);
        set_pin_lev(LEDUI4_IO_PIN, LEDUI4_IO_CONT, LOW);
#endif
#endif
        intr_cnt = CRITERR_INTR_FREQ * 3;
        while (intr_cnt)
                ;
#if defined(LEDUI_ANODE_ON_IO_PIN)
#if LEDUI_ANODE_ON_IO_PIN == 1
	set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, LOW);
        set_pin_lev(LEDUI2_IO_PIN, LEDUI2_IO_CONT, LOW);
        set_pin_lev(LEDUI3_IO_PIN, LEDUI3_IO_CONT, LOW);
        set_pin_lev(LEDUI4_IO_PIN, LEDUI4_IO_CONT, LOW);
#else
	set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, HIGH);
        set_pin_lev(LEDUI2_IO_PIN, LEDUI2_IO_CONT, HIGH);
        set_pin_lev(LEDUI3_IO_PIN, LEDUI3_IO_CONT, HIGH);
        set_pin_lev(LEDUI4_IO_PIN, LEDUI4_IO_CONT, HIGH);
#endif
#endif
        intr_cnt = CRITERR_INTR_FREQ * 2;
        while (intr_cnt)
                ;
	show_err(err);
	led1_en = TRUE;
        while (TRUE)
                ;
}

/**
 * show_err
 */
static void show_err(enum crit_err err)
{
        int i;

        for (i = 0; i < 3; i++) {
                switch (i) {
                case 0 :
                        if (err & 1) {
#if defined(LEDUI_ANODE_ON_IO_PIN)
#if LEDUI_ANODE_ON_IO_PIN == 1
				set_pin_lev(LEDUI4_IO_PIN, LEDUI4_IO_CONT, HIGH);
#else
				set_pin_lev(LEDUI4_IO_PIN, LEDUI4_IO_CONT, LOW);
#endif
#endif
                        }
                        break;
                case 1 :
                        if (err & 1) {
#if defined(LEDUI_ANODE_ON_IO_PIN)
#if LEDUI_ANODE_ON_IO_PIN == 1
				set_pin_lev(LEDUI3_IO_PIN, LEDUI3_IO_CONT, HIGH);
#else
				set_pin_lev(LEDUI3_IO_PIN, LEDUI3_IO_CONT, LOW);
#endif
#endif
                        }
                        break;
                case 2 :
                        if (err & 1) {
#if defined(LEDUI_ANODE_ON_IO_PIN)
#if LEDUI_ANODE_ON_IO_PIN == 1
				set_pin_lev(LEDUI2_IO_PIN, LEDUI2_IO_CONT, HIGH);
#else
				set_pin_lev(LEDUI2_IO_PIN, LEDUI2_IO_CONT, LOW);
#endif
#endif
                        }
                        break;
                }
                err >>= 1;
        }
}

/**
 * init_tc_50ms
 */
static void init_tc_50ms(void)
{
	NVIC_DisableIRQ(CRITERR_TID);
        enable_periph_clk_nocs(CRITERR_TID);
	CRITERR_TDV->TC_CHANNEL[tc_chnl(CRITERR_TID)].TC_IDR = ~0;
	CRITERR_TDV->TC_QIDR = ~0;
        NVIC_ClearPendingIRQ(CRITERR_TID);
	CRITERR_TDV->TC_BMR = 0;
	CRITERR_TDV->TC_CHANNEL[tc_chnl(CRITERR_TID)].TC_CMR = TC_CMR_CPCTRG |
	                                                       TC_CMR_TCCLKS_TIMER_CLOCK4;
	CRITERR_TDV->TC_CHANNEL[tc_chnl(CRITERR_TID)].TC_RC = F_MCK / 128 / 20 - 1;
        CRITERR_TDV->TC_CHANNEL[tc_chnl(CRITERR_TID)].TC_IER = TC_IER_CPCS;
	set_tc_intr_clbk(CRITERR_TID, tc_hndlr);
        NVIC_SetPriority(CRITERR_TID, 0);
	NVIC_EnableIRQ(CRITERR_TID);
        CRITERR_TDV->TC_CHANNEL[tc_chnl(CRITERR_TID)].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN;
}

/**
 * tc_hndlr
 */
static BaseType_t tc_hndlr(void)
{
	static boolean_t ld_st;
        static int i = CRITERR_WD_RST;
	volatile unsigned int dm;

	if (intr_cnt) {
		intr_cnt--;
	}
	if (i) {
		i--;
	} else {
		i = CRITERR_WD_RST;
                wd_rst();
	}
	if (led1_en) {
		if (ld_st) {
#if defined(LEDUI_ANODE_ON_IO_PIN)
#if LEDUI_ANODE_ON_IO_PIN == 1
			set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, LOW);
#else
			set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, HIGH);
#endif
#elif defined(CRITERR_LED_PIN) && defined(CRITERR_LED_CONT)
			set_pin_lev(CRITERR_LED_PIN, CRITERR_LED_CONT, LOW);
#endif
			ld_st = FALSE;
		} else {
#if defined(LEDUI_ANODE_ON_IO_PIN)
#if LEDUI_ANODE_ON_IO_PIN == 1
			set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, HIGH);
#else
			set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, LOW);
#endif
#elif defined(CRITERR_LED_PIN) && defined(CRITERR_LED_CONT)
			set_pin_lev(CRITERR_LED_PIN, CRITERR_LED_CONT, HIGH);
#endif
			ld_st = TRUE;
		}
	}
	dm = CRITERR_TDV->TC_CHANNEL[tc_chnl(CRITERR_TID)].TC_SR;
	return (pdFALSE);
}
#else

/**
 * crit_err_exit
 */
void crit_err_exit(enum crit_err err)
{
	taskDISABLE_INTERRUPTS();
	for (;;) {
		;
	}
}
#endif

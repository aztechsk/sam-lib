/*
 * sleep.c
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
#include "msgconf.h"
#include "pio.h"
#include "pmc.h"
#include "supc.h"
#include "tools.h"
#include "sleep.h"

#if SLEEP_FEAT == 1

static void (*first[SLEEP_FIRST_ARY_SIZE])(enum sleep_cmd, ...);
static void (*second[SLEEP_SECOND_ARY_SIZE])(enum sleep_cmd, ...);
static void (*last[SLEEP_LAST_ARY_SIZE])(enum sleep_cmd, ...);
static TaskHandle_t tsk_hndl;
static enum sleep_mode sleep_mode;
static volatile boolean_t idle_sleep;
static unsigned long long pclk_bslp;
static void (*clocks)(boolean_t);
static void (*sleepclbk)(boolean_t);

static void tsk(void *p);

/**
 * init_sleep
 */
void init_sleep(void (*set_clocks)(boolean_t), void (*sleep_clbk)(boolean_t))
{
	clocks = set_clocks;
	sleep_clbk = sleep_clbk;
	if (pdPASS != xTaskCreate(tsk, "SLEEP", SLEEP_TASK_STACK_SIZE, NULL,
				  TASK_PRIO_LOW, &tsk_hndl)) {
		crit_err_exit(MALLOC_ERROR);
	}
}

/**
 * reg_sleep_clbk
 */
void reg_sleep_clbk(void (*clbk)(enum sleep_cmd, ...), enum sleep_prio prio)
{
	void (**ary)(enum sleep_cmd, ...) = NULL;
	int sz = 0;

	switch (prio) {
	case SLEEP_PRIO_SUSP_FIRST :
		ary = first;
		sz = SLEEP_FIRST_ARY_SIZE;
		break;
        case SLEEP_PRIO_SUSP_SECOND :
		ary = second;
		sz = SLEEP_SECOND_ARY_SIZE;
		break;
        case SLEEP_PRIO_SUSP_LAST :
		ary = last;
		sz = SLEEP_LAST_ARY_SIZE;
		break;
	}
	taskENTER_CRITICAL();
	for (int i = 0; i < sz; i++) {
		if (!ary[i]) {
			ary[i] = clbk;
                        taskEXIT_CRITICAL();
			return;
		} else {
			if (ary[i] == clbk) {
				taskEXIT_CRITICAL();
				return;
			}
		}
	}
        taskEXIT_CRITICAL();
	crit_err_exit(UNEXP_PROG_STATE);
}

/**
 * start_sleep
 */
void start_sleep(enum sleep_mode mode)
{
	sleep_mode = mode;
	barrier();
	vTaskResume(tsk_hndl);
}

/**
 * enable_idle_sleep
 */
void enable_idle_sleep(void)
{
	set_pmc_lpm(PMC_LPM_IDLE);
	SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
	idle_sleep = TRUE;
}

/**
 * disable_idle_sleep
 */
void disable_idle_sleep(void)
{
	idle_sleep = FALSE;
}

/**
 * vApplicationIdleHook
 */
void vApplicationIdleHook(void)
{
	if (idle_sleep) {
		__DSB();
		__WFI();
	}
}

/**
 * tsk
 */
static void tsk(void *p)
{
	int max_f = -1, max_s = -1, max_l = -1;

	for (;;) {
		vTaskSuspend(NULL);
		msg(INF, "sleep.c: init sleep (%d)\n", sleep_mode);
		for (int i = 0; i < SLEEP_FIRST_ARY_SIZE; i++) {
			if (first[i]) {
				max_f = i;
				first[i](SLEEP_CMD_SUSP, sleep_mode);
				continue;
			}
			break;
		}
		for (int i = 0; i < SLEEP_SECOND_ARY_SIZE; i++) {
			if (second[i]) {
				max_s = i;
				second[i](SLEEP_CMD_SUSP, sleep_mode);
				continue;
			}
			break;
		}
		for (int i = 0; i < SLEEP_LAST_ARY_SIZE; i++) {
			if (last[i]) {
				max_l = i;
				last[i](SLEEP_CMD_SUSP, sleep_mode);
				continue;
			}
			break;
		}
#if PIOA_INTR == 1
		NVIC_DisableIRQ(ID_PIOA);
#endif
#if PIOB_INTR == 1
		NVIC_DisableIRQ(ID_PIOB);
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
		NVIC_DisableIRQ(ID_PIOC);
#endif
#if PIOA_CLOCK == 1
		disable_pio_clk(PIOA);
#endif
#if PIOB_CLOCK == 1
		disable_pio_clk(PIOB);
#endif
#if defined(ID_PIOC) && PIOC_CLOCK == 1
		disable_pio_clk(PIOC);
#endif
		pclk_bslp = get_act_periph_clks();
		__disable_irq();
		SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
		SCB->ICSR |= SCB_ICSR_PENDSTCLR_Pos;
		if (sleepclbk) {
			sleepclbk(SLEEP);
		}
		clocks(SLEEP);
#if SAM3_SERIES
		if (sleep_mode == SLEEP_MODE_BACKUP) {
			set_pmc_lpm(PMC_LPM_IDLE);
			SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
		} else {
			set_pmc_lpm(PMC_LPM_WAIT);
			SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
		}
		__DSB();
		__WFE();
		for (int i = 0; i < 500; i++) {
			__NOP();
		}
		wait_fast_rc_osc_enabled();
#elif SAM4_SERIES
#if SLEEP_NOT_USE_WFE == 1
		if (sleep_mode == SLEEP_MODE_BACKUP) {
			SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
			stop_vreg_rst_core();
		} else {
			set_pmc_flash_lpm(SLEEP_FLASH_LP_MODE);
			enter_pmc_wait_lpm();
		}
#else
		if (sleep_mode == SLEEP_MODE_BACKUP) {
			set_pmc_lpm(PMC_LPM_IDLE);
			SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
		} else {
			set_pmc_flash_lpm(SLEEP_FLASH_LP_MODE);
			set_pmc_lpm(PMC_LPM_WAIT);
			SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
		}
		__DSB();
		__WFE();
#endif
		for (int i = 0; i < 500; i++) {
			__NOP();
		}
		wait_fast_rc_osc_enabled();
		set_pmc_flash_lpm(PMC_FLASH_LPM_IDLE);
#else
 #error "SAM_SERIES definition error"
#endif
		set_pmc_lpm(PMC_LPM_IDLE);
		SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
		clocks(WAKE);
		if (sleepclbk) {
			sleepclbk(WAKE);
		}
		SysTick->VAL = 0;
		SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
		__enable_irq();
#if PIOA_CLOCK == 1
		enable_pio_clk(PIOA);
#endif
#if PIOB_CLOCK == 1
		enable_pio_clk(PIOB);
#endif
#if defined(ID_PIOC) && PIOC_CLOCK == 1
		enable_pio_clk(PIOC);
#endif
#if PIOA_INTR == 1
		clear_pio_isr(PIOA);
		NVIC_ClearPendingIRQ(ID_PIOA);
		NVIC_EnableIRQ(ID_PIOA);
#endif
#if PIOB_INTR == 1
		clear_pio_isr(PIOB);
		NVIC_ClearPendingIRQ(ID_PIOB);
		NVIC_EnableIRQ(ID_PIOB);
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
		clear_pio_isr(PIOC);
		NVIC_ClearPendingIRQ(ID_PIOC);
		NVIC_EnableIRQ(ID_PIOC);
#endif
		for (int i = max_l; i >= 0; i--) {
			last[i](SLEEP_CMD_WAKE);
		}
		for (int i = max_s; i >= 0; i--) {
			second[i](SLEEP_CMD_WAKE);
		}
		for (int i = max_f; i >= 0; i--) {
			first[i](SLEEP_CMD_WAKE);
		}
                msg(INF, "sleep.c: waked\n");
#if SLEEP_LOG_STATE == 1
		msg(INF, "-----------------------\n");
#endif
		if (pclk_bslp) {
			char s[48];
			prn_bv_str(s, pclk_bslp >> 32, 32);
			msg(INF, "sleep.c: pclk_bslp_w1: %s\n", s);
			prn_bv_str(s, pclk_bslp, 32);
			msg(INF, "sleep.c: pclk_bslp_w0: %s\n", s);
		}
	}
}
#endif

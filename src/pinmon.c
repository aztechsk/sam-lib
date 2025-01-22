/*
 * pinmon.c
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
#include "pio.h"
#include "pinmon.h"

#if PINMON == 1

struct evnt {
	Pio *cont;
	unsigned int pins;
};

static QueueHandle_t evnt_que, lev_que;
static pinmon pinmons;
static TaskHandle_t evnt_hndl, tmb_hndl;
static int evnt_que_full, lev_que_full, pin_lev_err;
static volatile boolean_t start;

static void tmb_tsk(void *p);
static void evnt_tsk(void *p);
static boolean_t check_intr_evnt(struct evnt *evnt);
static BaseType_t pio_clbk(unsigned int isr, Pio *cont);
#if PIOA_INTR == 1
static BaseType_t pioa_clbk(unsigned int isr);
#endif
#if PIOB_INTR == 1
static BaseType_t piob_clbk(unsigned int isr);
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
static BaseType_t pioc_clbk(unsigned int isr);
#endif

/**
 * init_pinmon
 */
void init_pinmon(void)
{
	evnt_que = xQueueCreate(PINMON_EVNT_QUE_SZ, sizeof(struct evnt));
	if (evnt_que == NULL) {
		crit_err_exit(MALLOC_ERROR);
	}
	lev_que = xQueueCreate(PINMON_PIN_LEV_QUE_SZ, sizeof(unsigned int));
	if (lev_que == NULL) {
		crit_err_exit(MALLOC_ERROR);
	}
        if (pdPASS != xTaskCreate(evnt_tsk, "PMONEVNT", PINMON_EVNT_TASK_STACK_SIZE, NULL,
                                  PINMON_EVNT_TASK_PRIO, &evnt_hndl)) {
                crit_err_exit(MALLOC_ERROR);
        }
        if (pdPASS != xTaskCreate(tmb_tsk, "PMONTMB", PINMON_TMB_TASK_STACK_SIZE, NULL,
                                  PINMON_TMB_TASK_PRIO, &tmb_hndl)) {
                crit_err_exit(MALLOC_ERROR);
        }
}

/**
 * start_pinmon
 */
void start_pinmon(void)
{
	start = TRUE;
}

/**
 * get_pinmon_que
 */
QueueHandle_t get_pinmon_que(void)
{
	if (lev_que == NULL) {
		crit_err_exit(UNEXP_PROG_STATE);
	}
	return (lev_que);
}

/**
 * add_pinmon
 */
void add_pinmon(pinmon pm)
{
#if PIOA_INTR == 1
	if (pm->cont == PIOA) {
		if (!add_pio_intr_clbk(PIOA, pioa_clbk)) {
			crit_err_exit(BAD_PARAMETER);
		}
                goto conf;
	}
#endif
#if PIOB_INTR == 1
	if (pm->cont == PIOB) {
		if (!add_pio_intr_clbk(PIOB, piob_clbk)) {
			crit_err_exit(BAD_PARAMETER);
		}
               	goto conf;
	}
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
	if (pm->cont == PIOC) {
		if (!add_pio_intr_clbk(PIOC, pioc_clbk)) {
			crit_err_exit(BAD_PARAMETER);
		}
                goto conf;
	}
#endif
	crit_err_exit(BAD_PARAMETER);
conf:
	if (pm->pull_res) {
		if (pm->active_lev == LOW) {
			conf_io_pin(pm->pin, pm->cont, PIO_INPUT, PIO_PULL_UP_ON, PIO_DEBOUNCE_FILTER_ON,
				    PIO_LOW_LEVEL_INTR_CFG, PIO_END_OF_FEAT);
		} else {
			conf_io_pin(pm->pin, pm->cont, PIO_INPUT, PIO_PULL_DOWN_ON, PIO_DEBOUNCE_FILTER_ON,
				    PIO_HIGH_LEVEL_INTR_CFG, PIO_END_OF_FEAT);
		}
	} else {
		if (pm->active_lev == LOW) {
			conf_io_pin(pm->pin, pm->cont, PIO_INPUT, PIO_PULL_UP_OFF, PIO_PULL_DOWN_OFF,
				    PIO_DEBOUNCE_FILTER_ON, PIO_LOW_LEVEL_INTR_CFG, PIO_END_OF_FEAT);
		} else {
			conf_io_pin(pm->pin, pm->cont, PIO_INPUT, PIO_PULL_UP_OFF, PIO_PULL_DOWN_OFF,
				    PIO_DEBOUNCE_FILTER_ON, PIO_HIGH_LEVEL_INTR_CFG, PIO_END_OF_FEAT);
		}
	}
	pm->lev = get_pin_lev(pm->pin, pm->cont);
	pm->cnt = 0;
	taskENTER_CRITICAL();
	if (pinmons) {
		pinmon p = pinmons;
		while (p->next) {
			p = p->next;
		}
		p->next = pm;
	} else {
		pinmons = pm;
	}
	if (pm->lev != pm->active_lev) {
		enable_pin_intr(pm->pin, pm->cont);
	}
	taskEXIT_CRITICAL();
}

/**
 * tmb_tsk
 */
static void tmb_tsk(void *p)
{
	static struct evnt evnt;
	static TickType_t lwt;

	lwt = xTaskGetTickCount();
	for (;;) {
		vTaskDelayUntil(&lwt, PINMON_TIME_BASE_MS / portTICK_PERIOD_MS);
		if (errQUEUE_FULL == xQueueSend(evnt_que, &evnt, 0)) {
			taskENTER_CRITICAL();
			evnt_que_full++;
			taskEXIT_CRITICAL();
		}
	}
}

/**
 * evnt_tsk
 */
static void evnt_tsk(void *p)
{
	static struct evnt evnt;
	static boolean_t upd;
	static pinmon pm;

	for (;;) {
		xQueueReceive(evnt_que, &evnt, portMAX_DELAY);
		if (!pinmons) {
			continue;
		}
		upd = FALSE;
		if (evnt.cont == NULL) {
			pm = pinmons;
			do {
				if (pm->lev == pm->active_lev) {
					if (get_pin_lev(pm->pin, pm->cont) == pm->active_lev) {
						pm->cnt = 0;
					} else {
						pm->cnt++;
					}
					if (pm->cnt == PINMON_DEACT_TMB_CNT) {
						pm->lev = !pm->lev;
						enable_pin_intr(pm->pin, pm->cont);
						upd = TRUE;
					}
				}
			} while ((pm = pm->next));
		} else {
			if (check_intr_evnt(&evnt)) {
				upd = TRUE;
			}
		}
		if (!start) {
			continue;
		} else {
			static boolean_t b;
			if (!b) {
				b = TRUE;
				upd = TRUE;
			}
		}
		if (upd) {
			unsigned int lev = 0, bit = 1;
			pm = pinmons;
			do {
				if (pm->lev) {
					lev |= bit;
				}
				bit <<= 1;
			} while ((pm = pm->next));
			if (errQUEUE_FULL == xQueueSend(lev_que, &lev, 0)) {
				lev_que_full++;
			}
		}
	}
}

/**
 * check_intr_evnt
 */
static boolean_t check_intr_evnt(struct evnt *evnt)
{
	pinmon pm;
	boolean_t upd = FALSE;

	pm = pinmons;
	do {
		if (pm->cont == evnt->cont && pm->pin & evnt->pins) {
			if (get_pin_lev(pm->pin, pm->cont) == pm->active_lev) {
				pm->cnt = 0;
				pm->lev = pm->active_lev;
				upd = TRUE;
			} else {
				enable_pin_intr(pm->pin, pm->cont);
				pin_lev_err++;
			}
		}
	} while ((pm = pm->next));
	return (upd);
}

/**
 * pio_clbk
 */
static BaseType_t pio_clbk(unsigned int isr, Pio *cont)
{
	BaseType_t tsk_wkn = pdFALSE;
	struct evnt evnt = {.cont = NULL};
	pinmon pm;

	if (!pinmons) {
		return (tsk_wkn);
	}
	pm = pinmons;
	do {
		if (pm->cont != cont) {
			continue;
		}
		if (pm->pin & isr && pm->pin & cont->PIO_IMR) {
			if (get_pin_lev(pm->pin, cont) == pm->active_lev) {
				evnt.cont = cont;
				evnt.pins |= pm->pin;
				disable_pin_intr(pm->pin, cont);
			}
		}
	} while ((pm = pm->next));
	if (evnt.cont) {
		BaseType_t wkn = pdFALSE;
		if (errQUEUE_FULL == xQueueSendFromISR(evnt_que, &evnt, &wkn)) {
			evnt_que_full++;
		}
		if (wkn) {
			tsk_wkn = pdTRUE;
		}
	}
	return (tsk_wkn);
}

#if PIOA_INTR == 1
/**
 * pioa_clbk
 */
static BaseType_t pioa_clbk(unsigned int isr)
{
	return (pio_clbk(isr, PIOA));
}
#endif

#if PIOB_INTR == 1
/**
 * piob_clbk
 */
static BaseType_t piob_clbk(unsigned int isr)
{
	return (pio_clbk(isr, PIOB));
}
#endif

#if defined(ID_PIOC) && PIOC_INTR == 1
/**
 * pioc_clbk
 */
static BaseType_t pioc_clbk(unsigned int isr)
{
	return (pio_clbk(isr, PIOC));
}
#endif

#if TERMOUT == 1
/**
 * log_pinmon_stats
 */
void log_pinmon_stats(void)
{
	msg(INF, "pinmon.c: evnt_que_full=%d, lev_que_full=%d pin_lev_err=%d\n", evnt_que_full, lev_que_full, pin_lev_err);
}
#endif

#endif

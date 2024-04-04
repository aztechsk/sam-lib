/*
 * btn.c
 *
 * Copyright (c) 2023 Jan Rusnak <jan@rusnak.sk>
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
#include "btn.h"
#include <string.h>

#if BTN == 1

#ifndef BTN_SLEEP
 #define BTN_SLEEP 0
#endif

#if BTN_SLEEP == 1
#include "sleep.h"
#endif

struct intr {
	Pio *cont;
	unsigned int isr;
	unsigned int pin_lev;
        TickType_t tm;
};

static TaskHandle_t tsk_hndl;
static const char *const tsk_nm = "BTN";
static struct btn_dsc *btn_list;
static QueueHandle_t intr_que;
static int intr_que_full_err;

static void btn_tsk(void *p);
static boolean_t btn_release(btn b, struct intr *intr);
static void conf_btn_pin(btn b);
#if PIOA_INTR == 1
static BaseType_t pioa_clbk(unsigned int isr);
#endif
#if PIOB_INTR == 1
static BaseType_t piob_clbk(unsigned int isr);
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
static BaseType_t pioc_clbk(unsigned int isr);
#endif
#if BTN_SLEEP == 1
static void sleep_clbk(enum sleep_cmd cmd, ...);
#endif

/**
 * init_btn
 */
void init_btn(void)
{
	intr_que = xQueueCreate(BTN_INTR_QUE_SIZE, sizeof(struct intr));
	if (intr_que == NULL) {
		crit_err_exit(MALLOC_ERROR);
	}
	if (pdPASS != xTaskCreate(btn_tsk, tsk_nm, BTN_TASK_STACK_SIZE, NULL,
                                  BTN_TASK_PRIO, &tsk_hndl)) {
                crit_err_exit(MALLOC_ERROR);
        }
#if BTN_SLEEP == 1
	reg_sleep_clbk(sleep_clbk, SLEEP_PRIO_SUSP_FIRST);
#endif
}

/**
 * add_btn_dev
 */
void add_btn_dev(btn dev)
{
	dev->evnt_que = xQueueCreate(dev->evnt_que_size, sizeof(struct btn_evnt));
	if (dev->evnt_que == NULL) {
		crit_err_exit(MALLOC_ERROR);
	}
#if configUSE_QUEUE_SETS == 1
	if (dev->qset) {
		if (pdFAIL == xQueueAddToSet(dev->evnt_que, dev->qset)) {
			crit_err_exit(UNEXP_PROG_STATE);
		}
	}
#endif
#if PIOA_INTR == 1
	if (dev->cont == PIOA) {
		if (!add_pio_intr_clbk(PIOA, pioa_clbk)) {
			crit_err_exit(BAD_PARAMETER);
		}
                goto conf;
	}
#endif
#if PIOB_INTR == 1
	if (dev->cont == PIOB) {
		if (!add_pio_intr_clbk(PIOB, piob_clbk)) {
			crit_err_exit(BAD_PARAMETER);
		}
               	goto conf;
	}
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
	if (dev->cont == PIOC) {
		if (!add_pio_intr_clbk(PIOC, pioc_clbk)) {
			crit_err_exit(BAD_PARAMETER);
		}
                goto conf;
	}
#endif
	crit_err_exit(BAD_PARAMETER);
conf:
	taskENTER_CRITICAL();
	if (btn_list) {
		btn b = btn_list;
		while (b->next) {
			b = b->next;
		}
		b->next = dev;
	} else {
		btn_list = dev;
	}
	taskEXIT_CRITICAL();
	conf_btn_pin(dev);
}

/**
 * btn_tsk
 */
static void btn_tsk(void *p)
{
	static struct intr intr;
        static struct btn_evnt evnt;
	static btn b;

	while (TRUE) {
		xQueueReceive(intr_que, &intr, portMAX_DELAY);
		b = btn_list;
#if BTN_SLEEP == 1
		if (intr.cont == NULL) {
			while (b) {
				if (b->active_lev == LOW) {
					conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_DISABLE_INTR, PIO_INPUT_FILTER_OFF,
						    PIO_END_OF_FEAT);
				} else {
					conf_io_pin(b->pin, b->cont, PIO_OUTPUT, PIO_DISABLE_INTR, PIO_INPUT_FILTER_OFF,
					            PIO_DRIVE_LOW, PIO_PULL_DOWN_OFF, PIO_END_OF_FEAT);
				}
				b = b->next;
			}
#if SLEEP_LOG_STATE == 1
			msg(INF, "btn.c: %s suspended\n", tsk_nm);
#endif
			vTaskSuspend(NULL);
#if SLEEP_LOG_STATE == 1
			msg(INF, "btn.c: %s resumed\n", tsk_nm);
#endif
			while (pdTRUE == xQueueReceive(intr_que, &intr, 0));
			b = btn_list;
			while (b) {
				b->tm_pres = 0;
				conf_btn_pin(b);
				b = b->next;
			}
			continue;
		}
#endif
		while (b) {
			if (b->cont != intr.cont || !(b->pin & intr.isr)) {
				b = b->next;
				continue;
			}
			if (b->mode == BTN_REPORT_MODE) {
				if (btn_release(b, &intr)) {
					if (!b->tm_pres) {
						b = b->next;
						continue;
					}
					evnt.type = BTN_PRESSED_DOWN;
					evnt.time = intr.tm - b->tm_pres;
					if (errQUEUE_FULL == xQueueSend(b->evnt_que, &evnt, 0)) {
						b->evnt_que_full_err++;
					}
				} else {
					b->tm_pres = intr.tm;
				}
			} else {
				if (btn_release(b, &intr)) {
					evnt.type = BTN_RELEASE;
				} else {
					evnt.type = BTN_PRESS;
				}
				evnt.time = intr.tm;
				if (errQUEUE_FULL == xQueueSend(b->evnt_que, &evnt, 0)) {
					b->evnt_que_full_err++;
				}
			}
                        b = b->next;
		}
	}
}

/**
 * btn_release
 */
static boolean_t btn_release(btn b, struct intr *intr)
{
	if (b->pin & intr->pin_lev) {
		return ((b->active_lev == LOW) ? TRUE : FALSE);
	} else {
		return ((b->active_lev == LOW) ? FALSE : TRUE);
	}
}

/**
 * conf_btn_pin
 */
static void conf_btn_pin(btn b)
{
	if (b->active_lev == LOW) {
		conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_PULL_UP_ON, PIO_DEBOUNCE_FILTER_ON,
			    PIO_ANY_EDGE_INTR, PIO_END_OF_FEAT);
	} else {
		conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_PULL_DOWN_ON, PIO_DEBOUNCE_FILTER_ON,
			    PIO_ANY_EDGE_INTR, PIO_END_OF_FEAT);
	}
}

#if PIOA_INTR == 1
/**
 * pioa_clbk
 */
static BaseType_t pioa_clbk(unsigned int isr)
{
	static struct intr intr;
        BaseType_t tsk_wkn = pdFALSE;

	intr.cont = PIOA;
	intr.isr = isr;
	intr.pin_lev = PIOA->PIO_PDSR;
	intr.tm = xTaskGetTickCountFromISR();
	if (errQUEUE_FULL == xQueueSendFromISR(intr_que, &intr, &tsk_wkn)) {
		intr_que_full_err++;
	}
	return (tsk_wkn);
}
#endif

#if PIOB_INTR == 1
/**
 * piob_clbk
 */
static BaseType_t piob_clbk(unsigned int isr)
{
	static struct intr intr;
	BaseType_t tsk_wkn = pdFALSE;

	intr.cont = PIOB;
	intr.isr = isr;
	intr.pin_lev = PIOB->PIO_PDSR;
	intr.tm = xTaskGetTickCountFromISR();
	if (errQUEUE_FULL == xQueueSendFromISR(intr_que, &intr, &tsk_wkn)) {
		intr_que_full_err++;
	}
	return (tsk_wkn);
}
#endif

#if defined(ID_PIOC) && PIOC_INTR == 1
/**
 * pioc_clbk
 */
static BaseType_t pioc_clbk(unsigned int isr)
{
	static struct intr intr;
        BaseType_t tsk_wkn = pdFALSE;

	intr.cont = PIOC;
	intr.isr = isr;
	intr.pin_lev = PIOC->PIO_PDSR;
	intr.tm = xTaskGetTickCountFromISR();
	if (errQUEUE_FULL == xQueueSendFromISR(intr_que, &intr, &tsk_wkn)) {
		intr_que_full_err++;
	}
	return (tsk_wkn);
}
#endif

#if BTN_SLEEP == 1
/**
 * sleep_clbk
 */
static void sleep_clbk(enum sleep_cmd cmd, ...)
{
	struct intr intr;

	if (cmd == SLEEP_CMD_SUSP) {
		memset(&intr, 0, sizeof(struct intr));
		xQueueSend(intr_que, &intr, portMAX_DELAY);
		while (eSuspended != eTaskGetState(tsk_hndl)) {
			taskYIELD();
		}
	} else {
		vTaskResume(tsk_hndl);
	}
}
#endif

#if TERMOUT == 1
/**
 * log_btn_stats
 */
void log_btn_stats(btn dev)
{
	msg(INF, "btn.c: evnt_que_full_err=%d intr_que_full_err=%d\n", dev->evnt_que_full_err,
	    intr_que_full_err);
}
#endif

#endif

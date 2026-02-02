/*
 * btn1.c
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
#include "btn1.h"

#if BTN1 == 1

#ifndef BTN1_SLEEP
 #define BTN1_SLEEP 0
#endif

#if BTN1_SLEEP == 1
#include "sleep.h"
#endif

struct isr_msg {
	TickType_t tm;
	boolean_t intr_sig;
};

static btn1 btn_list;

static void btn_tsk(void *p_btn);
static void conf_btn_pin(btn1 b);
#if PIOA_INTR == 1
static BaseType_t pioa_clbk(unsigned int isr);
#endif
#if PIOB_INTR == 1
static BaseType_t piob_clbk(unsigned int isr);
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
static BaseType_t pioc_clbk(unsigned int isr);
#endif
#if BTN1_SLEEP == 1
static void sleep_clbk(enum sleep_cmd cmd, ...);
#endif

/**
 * add_btn1_dev
 */
void add_btn1_dev(btn1 dev)
{
	dev->intr_que = xQueueCreate(1, sizeof(struct isr_msg));
	if (dev->intr_que == NULL) {
		crit_err_exit(MALLOC_ERROR);
	}
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
		btn1 b = btn_list;
		while (b->next) {
			b = b->next;
		}
		b->next = dev;
	} else {
		btn_list = dev;
	}
	taskEXIT_CRITICAL();
	if (pdPASS != xTaskCreate(btn_tsk, dev->tsk_nm, BTN1_TASK_STACK_SIZE, dev, BTN1_TASK_PRIO, &dev->tsk_hndl)) {
                crit_err_exit(MALLOC_ERROR);
        }
	conf_btn_pin(dev);
#if BTN1_SLEEP == 1
	reg_sleep_clbk(sleep_clbk, SLEEP_PRIO_SUSP_FIRST);
#endif
}

/**
 * btn_tsk
 */
static void btn_tsk(void *p_btn)
{
	btn1 b = p_btn;
        struct btn_evnt evnt;
        struct isr_msg isr_msg;
	int cnt;

	while (TRUE) {
lab1:
		xQueueReceive(b->intr_que, &isr_msg, portMAX_DELAY);
#if BTN1_SLEEP == 1
		if (isr_msg.intr_sig) {
			conf_io_pin(b->pin, b->cont, PIO_OUTPUT, PIO_DISABLE_INTR, PIO_INPUT_FILTER_OFF,
			            PIO_DRIVE_LOW, PIO_PULL_UP_OFF, PIO_PULL_DOWN_OFF , PIO_END_OF_FEAT);
#if SLEEP_LOG_STATE == 1
                        msg(INF, "btn1.c: %s suspended\n", b->tsk_nm);
#endif
                        vTaskSuspend(NULL);
#if SLEEP_LOG_STATE == 1
			msg(INF, "btn1.c: %s resumed\n", b->tsk_nm);
#endif
			while (pdTRUE == xQueueReceive(b->intr_que, &isr_msg, 0));
			conf_btn_pin(b);
			b->slp = FALSE;
			continue;
		}
#endif
		if (b->mode == BTN_EVENT_MODE) {
			evnt.type = BTN_PRESS;
			evnt.time = isr_msg.tm;
			if (errQUEUE_FULL == xQueueSend(b->evnt_que, &evnt, 0)) {
				b->evnt_que_full_err++;
			}
		}
		cnt = 0;
		while (TRUE) {
			vTaskDelay(BTN1_CHECK_DELAY / portTICK_PERIOD_MS);
			if (b->slp) {
				goto lab1;
			}
			if (b->active_lev == LOW) {
				if (b->cont->PIO_PDSR & b->pin) {
					cnt++;
				} else {
					cnt = 0;
					continue;
				}
			} else {
				if (!(b->cont->PIO_PDSR & b->pin)) {
					cnt++;
				} else {
					cnt = 0;
					continue;
				}
			}
			if (cnt == BTN1_CHECK_DELAY_CNT) {
				if (b->mode == BTN_EVENT_MODE) {
					evnt.type = BTN_RELEASE;
					evnt.time = xTaskGetTickCount();
				} else {
					evnt.type = BTN_PRESSED_DOWN;
                                        evnt.time = xTaskGetTickCount() - isr_msg.tm;

				}
				if (errQUEUE_FULL == xQueueSend(b->evnt_que, &evnt, 0)) {
					b->evnt_que_full_err++;
				}
				break;
			}
		}
		b->cont->PIO_IER = b->pin;
	}
}

/**
 * conf_btn_pin
 */
static void conf_btn_pin(btn1 b)
{
#if BTN1_CONF_PULL_RES == 1
	if (b->pull_res) {
		if (b->active_lev == LOW) {
			conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_PULL_UP_ON, PIO_DEBOUNCE_FILTER_ON,
				    PIO_LOW_LEVEL_INTR, PIO_END_OF_FEAT);
		} else {
			conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_PULL_DOWN_ON, PIO_DEBOUNCE_FILTER_ON,
				    PIO_HIGH_LEVEL_INTR, PIO_END_OF_FEAT);
		}
	} else {
		if (b->active_lev == LOW) {
			conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_PULL_UP_OFF, PIO_PULL_DOWN_OFF, PIO_DEBOUNCE_FILTER_ON,
				    PIO_LOW_LEVEL_INTR, PIO_END_OF_FEAT);
		} else {
			conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_PULL_UP_OFF, PIO_PULL_DOWN_OFF, PIO_DEBOUNCE_FILTER_ON,
				    PIO_HIGH_LEVEL_INTR, PIO_END_OF_FEAT);
		}
	}
#else
	if (b->active_lev == LOW) {
		conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_PULL_UP_ON, PIO_DEBOUNCE_FILTER_ON,
			    PIO_LOW_LEVEL_INTR, PIO_END_OF_FEAT);
	} else {
		conf_io_pin(b->pin, b->cont, PIO_INPUT, PIO_PULL_DOWN_ON, PIO_DEBOUNCE_FILTER_ON,
			    PIO_HIGH_LEVEL_INTR, PIO_END_OF_FEAT);
	}
#endif
}

#if PIOA_INTR == 1
/**
 * pioa_clbk
 */
static BaseType_t pioa_clbk(unsigned int isr)
{
        BaseType_t tsk_wkn = pdFALSE;
	struct isr_msg isr_msg;
	btn1 b = btn_list;

	while (b != NULL) {
		if (b->cont == PIOA && b->pin & isr && b->pin & PIOA->PIO_IMR) {
			if (b->active_lev == LOW) {
				if (PIOA->PIO_PDSR & b->pin) {
					b = b->next;
					continue;
				}
			} else {
				if (!(PIOA->PIO_PDSR & b->pin)) {
					b = b->next;
					continue;
				}
			}
			PIOA->PIO_IDR = b->pin;
			isr_msg.tm = xTaskGetTickCountFromISR();
			isr_msg.intr_sig = FALSE;
			BaseType_t wkn = pdFALSE;
			xQueueSendFromISR(b->intr_que, &isr_msg, &wkn);
			if (wkn) {
				tsk_wkn = pdTRUE;
			}
		}
		b = b->next;
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
	BaseType_t tsk_wkn = pdFALSE;
	struct isr_msg isr_msg;
	btn1 b = btn_list;

	while (b != NULL) {
		if (b->cont == PIOB && b->pin & isr && b->pin & PIOB->PIO_IMR) {
			if (b->active_lev == LOW) {
				if (PIOB->PIO_PDSR & b->pin) {
					b = b->next;
					continue;
				}
			} else {
				if (!(PIOB->PIO_PDSR & b->pin)) {
					b = b->next;
					continue;
				}
			}
			PIOB->PIO_IDR = b->pin;
			isr_msg.tm = xTaskGetTickCountFromISR();
			isr_msg.intr_sig = FALSE;
			BaseType_t wkn = pdFALSE;
			xQueueSendFromISR(b->intr_que, &isr_msg, &wkn);
			if (wkn) {
				tsk_wkn = pdTRUE;
			}
		}
		b = b->next;
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
        BaseType_t tsk_wkn = pdFALSE;
	struct isr_msg isr_msg;
	btn1 b = btn_list;

	while (b != NULL) {
		if (b->cont == PIOC && b->pin & isr && b->pin & PIOC->PIO_IMR) {
			if (b->active_lev == LOW) {
				if (PIOC->PIO_PDSR & b->pin) {
					b = b->next;
					continue;
				}
			} else {
				if (!(PIOC->PIO_PDSR & b->pin)) {
					b = b->next;
					continue;
				}
			}
			PIOC->PIO_IDR = b->pin;
			isr_msg.tm = xTaskGetTickCountFromISR();
			isr_msg.intr_sig = FALSE;
			BaseType_t wkn = pdFALSE;
			xQueueSendFromISR(b->intr_que, &isr_msg, &wkn);
			if (wkn) {
				tsk_wkn = pdTRUE;
			}
		}
		b = b->next;
	}
	return (tsk_wkn);
}
#endif

#if BTN1_SLEEP == 1
/**
 * sleep_clbk
 */
static void sleep_clbk(enum sleep_cmd cmd, ...)
{
	btn1 b;

	if (btn_list) {
		b = btn_list;
	} else {
		return;
	}
	if (cmd == SLEEP_CMD_SUSP) {
		do {
			struct isr_msg isr_msg;
                        isr_msg.tm = 0;
			isr_msg.intr_sig = TRUE;
                        b->cont->PIO_IDR = b->pin;
			b->slp = TRUE;
			xQueueSend(b->intr_que, &isr_msg, portMAX_DELAY);
			while (eSuspended != eTaskGetState(b->tsk_hndl)) {
				taskYIELD();
			}
		} while ((b = b->next));
	} else {
		do {
			vTaskResume(b->tsk_hndl);
		} while ((b = b->next));
	}
}
#endif

#if TERMOUT == 1
/**
 * log_btn1_stats
 */
void log_btn1_stats(btn1 dev)
{
	msg(INF, "btn1.c: %s evnt_que_full_err=%d\n", dev->tsk_nm, dev->evnt_que_full_err);
}
#endif

#endif

/*
 * led.c
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
#include "pio.h"
#include "tc.h"
#include "pmc.h"
#include "led.h"
#include <stdarg.h>

#if LED == 1

static TaskHandle_t tsk_hndl;
static const char *const tsk_nm = "LED";
static led led_list;

static void led_tsk(void *p);
static BaseType_t tc_hndlr(void);
static void set_led_on(led ld);
static void set_led_off(led ld);

/**
 * init_led
 */
void init_led(void)
{
	NVIC_DisableIRQ(LED_TID);
        enable_periph_clk(LED_TID);
	LED_TDV->TC_CHANNEL[tc_chnl(LED_TID)].TC_IDR = ~0;
        NVIC_ClearPendingIRQ(LED_TID);
	LED_TDV->TC_CHANNEL[tc_chnl(LED_TID)].TC_CMR = TC_CMR_WAVE |
	                                               TC_CMR_WAVSEL_UP_RC |
						       TC_CMR_CPCSTOP |
						       TC_CMR_TCCLKS_TIMER_CLOCK4;
	LED_TDV->TC_CHANNEL[tc_chnl(LED_TID)].TC_RC = LED_ON_TIME * (F_MCK / 128 / 1000) - 1;
        LED_TDV->TC_CHANNEL[tc_chnl(LED_TID)].TC_IER = TC_IER_CPCS;
	set_tc_intr_clbk(LED_TID, tc_hndlr);
        NVIC_SetPriority(LED_TID, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(LED_TID);
        LED_TDV->TC_CHANNEL[tc_chnl(LED_TID)].TC_CCR = TC_CCR_CLKEN;
        if (pdPASS != xTaskCreate(led_tsk, tsk_nm, LED_TASK_STACK_SIZE, NULL,
                                  LED_TASK_PRIO, &tsk_hndl)) {
                crit_err_exit(MALLOC_ERROR);
        }
}

/**
 * add_led_dev
 */
void add_led_dev(led dev)
{
	led l;

	if (dev->anode_on_pin) {
		conf_io_pin(dev->pin, dev->cont, PIO_OUTPUT, PIO_PULL_UP_OFF, PIO_DRIVE_LOW,
	                    PIO_END_OF_FEAT);
	} else {
		conf_io_pin(dev->pin, dev->cont, PIO_OUTPUT, PIO_PULL_UP_OFF, PIO_DRIVE_HIGH,
	                    PIO_END_OF_FEAT);
	}
	taskENTER_CRITICAL();
	if (led_list) {
		l = led_list;
		while (l->next) {
			l = l->next;
		}
		l->next = dev;
	} else {
		led_list = dev;
	}
        dev->state = dev->state_chng = LED_STATE_OFF;
	dev->off = FALSE;
        dev->next = NULL;
	taskEXIT_CRITICAL();
}

/**
 * set_led_dev_state
 */
void set_led_dev_state(led dev, enum led_state state, ...)
{
	va_list ap;

	switch (state) {
	case LED_STATE_ON :
		/* FALLTHRU */
	case LED_STATE_OFF :
		/* FALLTHRU */
        case LED_STATE_FLASH :
		dev->state_chng = state;
		break;
        case LED_STATE_BLINK :
		va_start(ap, state);
                taskENTER_CRITICAL();
		dev->state_chng = state;
		dev->delay_chng = va_arg(ap, int);;
		taskEXIT_CRITICAL();
                va_end(ap);
                break;
	default :
		crit_err_exit(BAD_PARAMETER);
		break;
	}
}

/**
 * led_tsk
 */
static void led_tsk(void *p)
{
        static TickType_t xLastWakeTime, freq;
	static led ld;
	static boolean_t swtrg;

        freq = LED_BASE_FREQ / portTICK_PERIOD_MS;
        xLastWakeTime = xTaskGetTickCount();
        while (TRUE) {
                vTaskDelayUntil(&xLastWakeTime, freq);
		if (!led_list) {
			continue;
		}
		ld = led_list;
		swtrg = FALSE;
		do {
			taskENTER_CRITICAL();
			if (ld->state != ld->state_chng) {
				ld->state = ld->state_chng;
				if (ld->state == LED_STATE_FLASH) {
					ld->state_chng = LED_STATE_OFF;
				}
                                taskEXIT_CRITICAL();
				if (ld->state == LED_STATE_ON) {
					set_led_on(ld);
				} else if (ld->state == LED_STATE_OFF) {
					set_led_off(ld);
				} else if (ld->state == LED_STATE_FLASH) {
					set_led_on(ld);
                                        ld->off = TRUE;
                                        swtrg = TRUE;
                                        ld->state = LED_STATE_OFF;
				} else if (ld->state == LED_STATE_BLINK) {
					set_led_on(ld);
                                        ld->off = TRUE;
                                        swtrg = TRUE;
                                        ld->dly_cnt = ld->delay = ld->delay_chng;
				}
			} else {
				taskEXIT_CRITICAL();
				if (ld->state == LED_STATE_BLINK) {
					if (ld->delay != ld->delay_chng) {
						ld->dly_cnt = ld->delay = ld->delay_chng;
						set_led_on(ld);
                                                ld->off = TRUE;
                                                swtrg = TRUE;
					} else {
						if (ld->dly_cnt) {
							ld->dly_cnt--;
						} else {
							set_led_on(ld);
							ld->off = TRUE;
                                                        swtrg = TRUE;
							ld->dly_cnt = ld->delay;
						}
					}
				}
			}
		} while ((ld = ld->next));
		if (swtrg) {
			LED_TDV->TC_CHANNEL[tc_chnl(LED_TID)].TC_CCR = TC_CCR_SWTRG;
		}
        }
}

/**
 * tc_hndlr
 */
static BaseType_t tc_hndlr(void)
{
	led ld;
        volatile unsigned int dm;

	ld = led_list;
	do {
		if (ld->off) {
			ld->off = FALSE;
			set_led_off(ld);
		}
	} while ((ld = ld->next));
	dm = LED_TDV->TC_CHANNEL[tc_chnl(LED_TID)].TC_SR;
	return (pdFALSE);
}

/**
 * set_led_on
 */
static void set_led_on(led ld)
{
	if (ld->anode_on_pin) {
                set_pin_lev(ld->pin, ld->cont, HIGH);
	} else {
                set_pin_lev(ld->pin, ld->cont, LOW);
	}
}

/**
 * set_led_off
 */
static void set_led_off(led ld)
{
	if (ld->anode_on_pin) {
                set_pin_lev(ld->pin, ld->cont, LOW);
	} else {
                set_pin_lev(ld->pin, ld->cont, HIGH);
	}
}
#endif

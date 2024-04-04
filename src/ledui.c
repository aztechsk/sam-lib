/*
 * ledui.c
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
#include "ledui.h"
#include <stdarg.h>

#if LEDUI == 1

#ifndef LEDUI_SLEEP
 #define LEDUI_SLEEP 0
#endif

#if LEDUI_SLEEP == 1
#include "sleep.h"
#endif

static struct ledui_led_state ld1, ld2, ld3, ld4;
ledui_led LEDUI1 = &ld1, LEDUI2 = &ld2, LEDUI3 = &ld3, LEDUI4 = &ld4;
static volatile boolean_t ini;

static TaskHandle_t tsk_hndl;
static SemaphoreHandle_t mtx;
static const char *const tsk_nm = "LEDUI";
#if LEDUI_SLEEP == 1
static volatile boolean_t sleep_req;
#endif

static void set_ld_stat(ledui_led ins, enum ledui_led_cmnd cmnd, enum ledui_blink_start lb_is);
static void set_ld_init_stat(ledui_led ins, enum ledui_blink_start lb_is);
static void sw_ld(ledui_led ins);
static void led_tsk(void *p);
#if LEDUI_SLEEP == 1
static void sleep_clbk(enum sleep_cmd cmd, ...);
#endif

/**
 * init_ledui_pins
 */
void init_ledui_pins(void)
{
#if LEDUI_ANODE_ON_IO_PIN == 1
        conf_io_pin(LEDUI1_IO_PIN, LEDUI1_IO_CONT, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
#else
	conf_io_pin(LEDUI1_IO_PIN, LEDUI1_IO_CONT, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_HIGH, PIO_END_OF_FEAT);
#endif
#if LEDUI_ANODE_ON_IO_PIN == 1
        conf_io_pin(LEDUI2_IO_PIN, LEDUI2_IO_CONT, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
#else
        conf_io_pin(LEDUI2_IO_PIN, LEDUI2_IO_CONT, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_HIGH, PIO_END_OF_FEAT);
#endif
#if LEDUI_ANODE_ON_IO_PIN == 1
        conf_io_pin(LEDUI3_IO_PIN, LEDUI3_IO_CONT, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
#else
        conf_io_pin(LEDUI3_IO_PIN, LEDUI3_IO_CONT, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_HIGH, PIO_END_OF_FEAT);
#endif
#if LEDUI_ANODE_ON_IO_PIN == 1
	conf_io_pin(LEDUI4_IO_PIN, LEDUI4_IO_CONT, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_LOW, PIO_END_OF_FEAT);
#else
        conf_io_pin(LEDUI4_IO_PIN, LEDUI4_IO_CONT, PIO_OUTPUT,
	            PIO_PULL_UP_OFF, PIO_DRIVE_HIGH, PIO_END_OF_FEAT);
#endif
}

/**
 * init_ledui
 */
void init_ledui(void)
{
        ld1.next = &ld2;
        ld2.next = &ld3;
        ld3.next = &ld4;
        ld4.next = &ld1;
        ld1.prev = &ld4;
        ld2.prev = &ld1;
        ld3.prev = &ld2;
        ld4.prev = &ld3;
        ld1.state = LEDUI_LED_OFF;
        ld2.state = LEDUI_LED_OFF;
        ld3.state = LEDUI_LED_OFF;
        ld4.state = LEDUI_LED_OFF;
        ld1.pos = LEDUI_BLED1;
        ld2.pos = LEDUI_BLED2;
        ld3.pos = LEDUI_BLED3;
        ld4.pos = LEDUI_BLED4;
        ld1.on_off = OFF;
        ld2.on_off = OFF;
        ld3.on_off = OFF;
        ld4.on_off = OFF;
        ld1.freq = 0;
        ld2.freq = 0;
        ld3.freq = 0;
        ld4.freq = 0;
        ld1.cnt = 0;
        ld2.cnt = 0;
        ld3.cnt = 0;
        ld4.cnt = 0;
        mtx = xSemaphoreCreateMutex();
        if (mtx == NULL) {
                crit_err_exit(MALLOC_ERROR);
        }
        if (pdPASS != xTaskCreate(led_tsk, tsk_nm, LEDUI_TASK_STACK_SIZE,
				  NULL, LEDUI_TASK_PRIO, &tsk_hndl)) {
                crit_err_exit(MALLOC_ERROR);
        }
#if LEDUI_SLEEP == 1
	reg_sleep_clbk(sleep_clbk, SLEEP_PRIO_SUSP_SECOND);
#endif
	ini = TRUE;
}

/**
 * set_ledui_led_state
 */
void set_ledui_led_state(ledui_led ins, enum ledui_led_cmnd cmnd, ...)
{
	enum ledui_blink_start lb_is;
        va_list ap;

        if (!ini) {
                crit_err_exit(UNEXP_PROG_STATE);
        }
	switch (cmnd) {
	case LEDUI_LED_BLINK_SLOW_STDF   :
		/* FALLTHRU */
	case LEDUI_LED_BLINK_NORMAL_STDF :
		/* FALLTHRU */
	case LEDUI_LED_BLINK_FAST_STDF   :
		cmnd -= 5;
		va_start(ap, cmnd);
		lb_is = va_arg(ap, int);
		va_end(ap);
		break;
	default                          :
		lb_is = LEDUI_BLINK_START_NSET;
		break;
	}
	switch (lb_is) {
	case LEDUI_BLINK_START_ON   :
		/* FALLTHRU */
	case LEDUI_BLINK_START_OFF  :
		/* FALLTHRU */
	case LEDUI_BLINK_START_NSET :
		break;
	default                     :
		crit_err_exit(BAD_PARAMETER);
		break;
	}
        xSemaphoreTake(mtx, portMAX_DELAY);
        switch (cmnd) {
        case LEDUI_LED_ON           :
                set_ld_stat(ins, LEDUI_LED_ON, lb_is);
                break;
        case LEDUI_LED_OFF          :
                set_ld_stat(ins, LEDUI_LED_OFF, lb_is);
                break;
        case LEDUI_LED_BLINK_FAST   :
                set_ld_stat(ins, LEDUI_LED_BLINK_FAST, lb_is);
                break;
        case LEDUI_LED_BLINK_NORMAL :
                set_ld_stat(ins, LEDUI_LED_BLINK_NORMAL, lb_is);
                break;
        case LEDUI_LED_BLINK_SLOW   :
                set_ld_stat(ins, LEDUI_LED_BLINK_SLOW, lb_is);
                break;
        case LEDUI_LED_NEXT_STATE   :
                if (++ins->state == LEDUI_LED_NEXT_STATE) {
                        ins->state = LEDUI_LED_OFF;
                }
                set_ld_stat(ins, ins->state, lb_is);
                break;
        case LEDUI_LED_PREV_STATE   :
                if (ins->state == LEDUI_LED_OFF) {
                        ins->state = LEDUI_LED_BLINK_FAST;
                } else {
                        ins->state--;
                }
                set_ld_stat(ins, ins->state, lb_is);
                break;
        default                     :
                crit_err_exit(BAD_PARAMETER);
                break;
        }
        xSemaphoreGive(mtx);
}

/**
 * set_ledui_all_leds_state
 */
void set_ledui_all_leds_state(enum ledui_led_cmnd cmnd)
{
        if (!ini) {
                crit_err_exit(UNEXP_PROG_STATE);
        }
        xSemaphoreTake(mtx, portMAX_DELAY);
        switch (cmnd) {
        case LEDUI_LED_ON           :
                set_ld_stat(&ld1, LEDUI_LED_ON, LEDUI_BLINK_START_OFF);
		set_ld_stat(&ld2, LEDUI_LED_ON, LEDUI_BLINK_START_OFF);
		set_ld_stat(&ld3, LEDUI_LED_ON, LEDUI_BLINK_START_OFF);
		set_ld_stat(&ld4, LEDUI_LED_ON, LEDUI_BLINK_START_OFF);
                break;
        case LEDUI_LED_OFF          :
                set_ld_stat(&ld1, LEDUI_LED_OFF, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld2, LEDUI_LED_OFF, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld3, LEDUI_LED_OFF, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld4, LEDUI_LED_OFF, LEDUI_BLINK_START_OFF);
                break;
        case LEDUI_LED_BLINK_FAST   :
                set_ld_stat(&ld1, LEDUI_LED_BLINK_FAST, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld2, LEDUI_LED_BLINK_FAST, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld3, LEDUI_LED_BLINK_FAST, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld4, LEDUI_LED_BLINK_FAST, LEDUI_BLINK_START_OFF);
                break;
        case LEDUI_LED_BLINK_NORMAL :
                set_ld_stat(&ld1, LEDUI_LED_BLINK_NORMAL, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld2, LEDUI_LED_BLINK_NORMAL, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld3, LEDUI_LED_BLINK_NORMAL, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld4, LEDUI_LED_BLINK_NORMAL, LEDUI_BLINK_START_OFF);
                break;
        case LEDUI_LED_BLINK_SLOW   :
                set_ld_stat(&ld1, LEDUI_LED_BLINK_SLOW, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld2, LEDUI_LED_BLINK_SLOW, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld3, LEDUI_LED_BLINK_SLOW, LEDUI_BLINK_START_OFF);
                set_ld_stat(&ld4, LEDUI_LED_BLINK_SLOW, LEDUI_BLINK_START_OFF);
                break;
        default                     :
                xSemaphoreGive(mtx);
                crit_err_exit(BAD_PARAMETER);
                break;
        }
        xSemaphoreGive(mtx);
}

/**
 * set_ld_stat
 */
static void set_ld_stat(ledui_led ins, enum ledui_led_cmnd cmnd, enum ledui_blink_start lb_is)
{
        switch (cmnd) {
        case LEDUI_LED_ON           :
                ledui_led_hw_on(ins->pos);
                ins->state = LEDUI_LED_ON;
                ins->on_off = ON;
                ins->freq = 0;
                ins->cnt = 0;
                break;
        case LEDUI_LED_OFF          :
                ledui_led_hw_off(ins->pos);
                ins->state = LEDUI_LED_OFF;
                ins->on_off = OFF;
                ins->freq = 0;
                ins->cnt = 0;
                break;
        case LEDUI_LED_BLINK_FAST   :
		set_ld_init_stat(ins, lb_is);
                ins->state = LEDUI_LED_BLINK_FAST;
                ins->freq = LEDUI_BLINK_FAST_SWITCH;
                ins->cnt = LEDUI_BLINK_FAST_SWITCH;
                break;
        case LEDUI_LED_BLINK_NORMAL :
		set_ld_init_stat(ins, lb_is);
                ins->state = LEDUI_LED_BLINK_NORMAL;
                ins->freq = LEDUI_BLINK_NORMAL_SWITCH;
                ins->cnt = LEDUI_BLINK_NORMAL_SWITCH;
                break;
        case LEDUI_LED_BLINK_SLOW   :
		set_ld_init_stat(ins, lb_is);
                ins->state = LEDUI_LED_BLINK_SLOW;
                ins->freq = LEDUI_BLINK_SLOW_SWITCH;
                ins->cnt = LEDUI_BLINK_SLOW_SWITCH;
                break;
	default                     :
		break;
        }
}

/**
 * set_ld_init_stat
 */
static void set_ld_init_stat(ledui_led ins, enum ledui_blink_start lb_is)
{
	switch (lb_is) {
	case LEDUI_BLINK_START_ON  :
		ledui_led_hw_on(ins->pos);
		ins->on_off = ON;
		break;
	case LEDUI_BLINK_START_OFF :
		ledui_led_hw_off(ins->pos);
                ins->on_off = OFF;
		break;
	default                    :
		sw_ld(ins);
		break;
	}
}

/**
 * sw_ld
 */
static void sw_ld(ledui_led ins)
{
        if (ins->on_off == ON) {
                ledui_led_hw_off(ins->pos);
                ins->on_off = OFF;
        } else {
                ledui_led_hw_on(ins->pos);
                ins->on_off = ON;
        }
}

/**
 * led_tsk
 */
static void led_tsk(void *p)
{
        static TickType_t xLastWakeTime, freq;

        freq = LEDUI_BASE_SWITCH_FREQ / portTICK_PERIOD_MS;
        xLastWakeTime = xTaskGetTickCount();
        while (TRUE) {
                vTaskDelayUntil(&xLastWakeTime, freq);
#if LEDUI_SLEEP == 1
		if (sleep_req) {
			boolean_t l1, l2, l3, l4;
			l1 = get_pin_out(LEDUI1_IO_PIN, LEDUI1_IO_CONT);
			l2 = get_pin_out(LEDUI2_IO_PIN, LEDUI2_IO_CONT);
			l3 = get_pin_out(LEDUI3_IO_PIN, LEDUI3_IO_CONT);
                        l4 = get_pin_out(LEDUI4_IO_PIN, LEDUI4_IO_CONT);
			ledui_led_hw_off(LEDUI_BLED1);
			ledui_led_hw_off(LEDUI_BLED2);
			ledui_led_hw_off(LEDUI_BLED3);
			ledui_led_hw_off(LEDUI_BLED4);
#if SLEEP_LOG_STATE == 1
                        msg(INF, "ledui.c: %s suspended\n", tsk_nm);
#endif
                        vTaskSuspend(NULL);
#if SLEEP_LOG_STATE == 1
			msg(INF, "ledui.c: %s resumed\n", tsk_nm);
#endif
#if LEDUI_ANODE_ON_IO_PIN == 1
			if (l1) {
#else
			if (!l1) {
#endif
				ledui_led_hw_on(LEDUI_BLED1);
			}
#if LEDUI_ANODE_ON_IO_PIN == 1
			if (l2) {
#else
			if (!l2) {
#endif
				ledui_led_hw_on(LEDUI_BLED2);
			}
#if LEDUI_ANODE_ON_IO_PIN == 1
			if (l3) {
#else
			if (!l3) {
#endif
				ledui_led_hw_on(LEDUI_BLED3);
			}
#if LEDUI_ANODE_ON_IO_PIN == 1
			if (l4) {
#else
			if (!l4) {
#endif
				ledui_led_hw_on(LEDUI_BLED4);
			}
                        xLastWakeTime = xTaskGetTickCount();
			sleep_req = FALSE;
			continue;
		}
#endif
                xSemaphoreTake(mtx, portMAX_DELAY);
                if (ld1.freq) {
                        if (!--ld1.cnt) {
                                sw_ld(&ld1);
                                ld1.cnt = ld1.freq;
                        }
                }
                if (ld2.freq) {
                        if (!--ld2.cnt) {
                                sw_ld(&ld2);
                                ld2.cnt = ld2.freq;
                        }
                }
                if (ld3.freq) {
                        if (!--ld3.cnt) {
                                sw_ld(&ld3);
                                ld3.cnt = ld3.freq;
                        }
                }
                if (ld4.freq) {
                        if (!--ld4.cnt) {
                                sw_ld(&ld4);
                                ld4.cnt = ld4.freq;
                        }
                }
                xSemaphoreGive(mtx);
        }
}

/**
 * ledui_led_hw_[on|off]
 */
#if LEDUI_ANODE_ON_IO_PIN == 1
void ledui_led_hw_on(enum ledui_board_leds pos)
#else
void ledui_led_hw_off(enum ledui_board_leds pos)
#endif
{
	switch (pos) {
	case LEDUI_BLED1 :
		set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, HIGH);
		break;
	case LEDUI_BLED2 :
		set_pin_lev(LEDUI2_IO_PIN, LEDUI2_IO_CONT, HIGH);
		break;
	case LEDUI_BLED3 :
		set_pin_lev(LEDUI3_IO_PIN, LEDUI3_IO_CONT, HIGH);
		break;
	case LEDUI_BLED4 :
		set_pin_lev(LEDUI4_IO_PIN, LEDUI4_IO_CONT, HIGH);
		break;
	}
}

/**
 * ledui_led_hw_[off|on]
 */
#if LEDUI_ANODE_ON_IO_PIN == 1
void ledui_led_hw_off(enum ledui_board_leds pos)
#else
void ledui_led_hw_on(enum ledui_board_leds pos)
#endif
{
	switch (pos) {
	case LEDUI_BLED1 :
		set_pin_lev(LEDUI1_IO_PIN, LEDUI1_IO_CONT, LOW);
		break;
	case LEDUI_BLED2 :
		set_pin_lev(LEDUI2_IO_PIN, LEDUI2_IO_CONT, LOW);
		break;
	case LEDUI_BLED3 :
		set_pin_lev(LEDUI3_IO_PIN, LEDUI3_IO_CONT, LOW);
		break;
	case LEDUI_BLED4 :
		set_pin_lev(LEDUI4_IO_PIN, LEDUI4_IO_CONT, LOW);
		break;
	}
}
#endif

#if LEDUI_SLEEP == 1
/**
 * sleep_clbk
 */
static void sleep_clbk(enum sleep_cmd cmd, ...)
{
	if (cmd == SLEEP_CMD_SUSP) {
		sleep_req = TRUE;
		while (eSuspended != eTaskGetState(tsk_hndl)) {
			taskYIELD();
		}
	} else {
		vTaskResume(tsk_hndl);
		while (sleep_req) {
			taskYIELD();
		}
	}
}
#endif

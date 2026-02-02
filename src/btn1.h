/*
 * btn1.h
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

#ifndef BTN1_H
#define BTN1_H

#ifndef BTN1
 #define BTN1 0
#endif

#if BTN1 == 1

#ifndef BTN1_CONF_PULL_RES
  #define BTN1_CONF_PULL_RES 0
#endif

typedef struct btn1_dsc *btn1;

enum btn_mode {
	BTN_REPORT_MODE,
	BTN_EVENT_MODE
};

struct btn1_dsc {
        unsigned int pin; // <SetIt>
	Pio *cont; // <SetIt>
	enum btn_mode mode; // <SetIt>
	boolean_t active_lev; // <SetIt>
#if BTN1_CONF_PULL_RES == 1
	boolean_t pull_res; // <SetIt>
#endif
        int evnt_que_size; // <SetIt>
#if configUSE_QUEUE_SETS == 1
	QueueSetHandle_t qset; // <SetIt>
#endif
	const char *tsk_nm; // <SetIt>
        QueueHandle_t evnt_que;
	QueueHandle_t intr_que;
	int evnt_que_full_err;
	TaskHandle_t tsk_hndl;
	volatile boolean_t slp;
        struct btn1_dsc *next;
};

enum btn_evnt_type {
	BTN_PRESSED_DOWN,
	BTN_PRESS,
	BTN_RELEASE
};

struct btn_evnt {
	enum btn_evnt_type type;
	int time;
};

/**
 * add_btn1_dev
 *
 * Register new button device.
 *
 * @dev: Button instance.
 */
void add_btn1_dev(btn1 dev);

#if TERMOUT == 1
/**
 * log_btn1_stats
 */
void log_btn1_stats(btn1 dev);
#endif

#endif

#endif

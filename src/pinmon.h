/*
 * pinmon.h
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

#ifndef PINMON_H
#define PINMON_H

#ifndef PINMON
 #define PINMON 0
#endif

#if PINMON == 1

typedef struct pinmon_dsc *pinmon;

struct pinmon_dsc {
	unsigned int pin; // <SetIt>
	Pio *cont; // <SetIt>
	boolean_t active_lev; // <SetIt>
	boolean_t pull_res; // <SetIt>
	boolean_t lev;
	int cnt;
	pinmon next;
};

/**
 * init_pinmon
 */
void init_pinmon(void);

/**
 * start_pinmon
 */
void start_pinmon(void);

/**
 * get_pinmon_que
 */
QueueHandle_t get_pinmon_que(void);

/**
 * add_pinmon
 *
 * Adds io pin for monitoring.
 *
 * @pm: pin instance.
 */
void add_pinmon(pinmon pm);

#if TERMOUT == 1
/**
 * log_pinmon_stats
 */
void log_pinmon_stats(void);
#endif

#endif

#endif

/*
 * shift165.h
 *
 * Copyright (c) 2020 Jan Rusnak <jan@rusnak.sk>
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

#ifndef SHIFT165_H
#define SHIFT165_H

#ifndef SHIFT165
 #define SHIFT165 0
#endif

#if SHIFT165 == 1

typedef struct shift165_dsc *shift165;

struct shift165_dsc {
	int size; // <SetIt>
	unsigned int pl_pin; // <SetIt>
	Pio *pl_cont; // <SetIt>
#if SHIFT165_DRIVE_CE == 1
	unsigned int ce_pin; // <SetIt>
	Pio *ce_cont; // <SetIt>
#endif
        unsigned int cp_pin; // <SetIt>
	Pio *cp_cont; // <SetIt>
        unsigned int q_pin; // <SetIt>
	Pio *q_cont; // <SetIt>
};

/**
 * init_shift165
 */
void init_shift165(shift165 dev);

/**
 * read_shift165
 */
unsigned int read_shift165(shift165 dev);
#endif

#endif

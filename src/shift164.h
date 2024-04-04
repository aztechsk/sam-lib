/*
 * shift164.h
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

#ifndef SHIFT164_H
#define SHIFT164_H

#ifndef SHIFT164
 #define SHIFT164 0
#endif

#if SHIFT164 == 1

typedef struct shift164_dsc *shift164;

struct shift164_dsc {
	int size; // <SetIt>
	unsigned int cp_pin; // <SetIt>
	Pio *cp_cont; // <SetIt>
	unsigned int sd_pin; // <SetIt>
	Pio *sd_cont; // <SetIt>
#if SHIFT164_OUT_LATCH == 1
	unsigned int ol_pin; // <SetIt>
	Pio *ol_cont; // <SetIt>
#endif
};

/**
 * init_shift164
 */
void init_shift164(shift164 dev);

/**
 * write_shift164
 */
void write_shift164(shift164 dev, unsigned int r);
#endif

#endif

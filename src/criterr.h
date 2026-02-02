/*
 * criterr.h
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

#ifndef CRITERR_H
#define CRITERR_H

enum crit_err {               // L123
	UNEXP_PROG_STATE,     // B---
	TASK_STACK_OVERFLOW,  // B--x
	MALLOC_ERROR,         // B-x-
	BAD_PARAMETER,        // B-xx
	APPLICATION_ERROR_1,  // Bx--
	APPLICATION_ERROR_2,  // Bx-x
	APPLICATION_ERROR_3,  // Bxx-
	HARDWARE_ERROR        // Bxxx
};

#ifndef CRITERR
 #define CRITERR 0
#endif

#if CRITERR == 1
/**
 * crit_err_exit
 */
#define crit_err_exit(err) crit_err_exit_fn(err, __FILE__, __LINE__)
void crit_err_exit_fn(enum crit_err err, char *file, int line);
#else
void crit_err_exit(enum crit_err err);
#endif

#endif

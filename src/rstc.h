/*
 * rstc.h
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

#ifndef RSTC_H
#define RSTC_H

enum rst_type {
	POWERUP_RST,
	BACKUP_RST,
	WATCHDOG_RST,
	SOFTWARE_RST,
	USER_RST
};

/**
 * init_rstc
 */
void init_rstc(void);

/**
 * rst_cause
 */
int rst_cause(void);

/**
 * soft_rst
 */
void soft_rst(void);

#if TERMOUT == 1
/**
 * rst_cause_str
 */
const char *rst_cause_str(void);

/**
 * log_rst_cause
 */
void log_rst_cause(void);
#endif

#endif

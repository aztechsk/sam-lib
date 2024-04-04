/*
 * dacc.h
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

#ifndef DACC_H
#define DACC_H

#ifndef DACC_FREE_RUN
 #define DACC_FREE_RUN 0
#endif

#if DACC_FREE_RUN == 1

typedef struct dacc_dev *dacc;

#if SAM3S_SERIES || SAM4S_SERIES
 #define DACC_CHNL_0 (0 << 16)
 #define DACC_CHNL_1 (1 << 16)
#elif SAM3N_SERIES || SAM4N_SERIES
 #define DACC_CHNL_0 (0 << 16)
#else
 #error "SAM_SERIES definition error"
#endif

struct dacc_dev {
	unsigned int mr; // <SetIt>
#if SAM3S_SERIES || SAM4S_SERIES
	unsigned int acr; // <SetIt>
#endif
};

/**
 * init_dacc
 *
 * Configure DACC to requested mode.
 *
 * @dev: DACC device.
 */
void init_dacc(dacc dev);

/**
 * enable_dacc_chnl
 *
 * Enable DACC channel with initial level setting.
 *
 * @cd: Initial conversion data. For SAMN data is bits[9:0]. For SAMS data is
 *      bits[11:0] and channel selection is bit[16].
 */
void enable_dacc_chnl(unsigned int cd);

/**
 * write_dacc_fifo
 *
 * Write data to DACC fifo.
 *
 * @cd: Conversion data. For SAMN data is bits[9:0]. For SAMS data is
 *      bits[11:0] and channel selection is bit[16].
 */
void write_dacc_fifo(unsigned int cd);
#endif

#endif

/*
 * wd.h
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

#ifndef WD_H
#define WD_H

#define WD_EXP_50MS (F_SLCK / 128 / 20)
#define WD_EXP_100MS (WD_EXP_50MS * 2)
#define WD_EXP_150MS (WD_EXP_50MS * 3)
#define WD_EXP_200MS (WD_EXP_50MS * 4)
#define WD_EXP_250MS (WD_EXP_50MS * 5)
#define WD_EXP_300MS (WD_EXP_50MS * 6)
#define WD_EXP_350MS (WD_EXP_50MS * 7)
#define WD_EXP_400MS (WD_EXP_50MS * 8)
#define WD_EXP_450MS (WD_EXP_50MS * 9)
#define WD_EXP_500MS (WD_EXP_50MS * 10)
#define WD_EXP_550MS (WD_EXP_50MS * 11)
#define WD_EXP_600MS (WD_EXP_50MS * 12)
#define WD_EXP_650MS (WD_EXP_50MS * 13)
#define WD_EXP_700MS (WD_EXP_50MS * 14)
#define WD_EXP_750MS (WD_EXP_50MS * 15)
#define WD_EXP_1S (WD_EXP_50MS * 20)
#define WD_EXP_1S_50MS (WD_EXP_50MS * 21)
#define WD_EXP_3S (WD_EXP_50MS * 60)
#define WD_EXP_3S_50MS (WD_EXP_50MS * 61)
#define WD_EXP_5S (WD_EXP_50MS * 100)
#define WD_EXP_5S_50MS (WD_EXP_50MS * 101)
#define WD_EXP_10S (WD_EXP_50MS * 200)

/**
 * init_wd
 */
void init_wd(void);

/**
 * disable_wd
 */
void disable_wd(void);

/**
 * wd_rst
 */
void wd_rst(void);

#endif

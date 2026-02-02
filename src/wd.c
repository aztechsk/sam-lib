/*
 * wd.c
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

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <gentyp.h>
#include "sysconf.h"
#include "board.h"
#include <mmio.h>
#include "criterr.h"
#include "wd.h"

/**
 * init_wd
 */
void init_wd(void)
{
	unsigned int mr;

	mr = WDT_MR_WDDBGHLT | WDT_MR_WDD(WD_EXPIRE_WDD) | WDT_MR_WDRSTEN |
	     WDT_MR_WDV(WD_EXPIRE_WDV);
#if WD_IDLE_HALT == 1
	mr |= WDT_MR_WDIDLEHLT;
#endif
	WDT->WDT_MR = mr;
}

/**
 * disable_wd
 */
void disable_wd(void)
{
	WDT->WDT_MR |=  WDT_MR_WDDIS;
}

/**
 * wd_rst
 */
void wd_rst(void)
{
	WDT->WDT_CR = 0xA5 << 24  | WDT_CR_WDRSTT;
}

/*
 * hwerr.c
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
#include "tools.h"
#include "hwerr.h"

#if TERMOUT == 1

static const struct txt_item hwerr[] = {
	{ENOERR, "ENOERR"},
        {EDMA,   "EDMA"},
        {ETMO,   "ETMO"},
        {EHW,    "EHW"},
	{EADDR,  "EADDR"},
	{ERCV,   "ERCV"},
	{ESND,   "ESND"},
	{EFMT,   "EFMT"},
	{EBFOV,  "EBFOV"},
	{ENRDY,  "ENRDY"},
	{EERASE, "EERASE"},
	{EDATA,  "EDATA"},
        {EINTR,  "EINTR"},
        {ENACK,  "ENACK"},
	{0, NULL}
};

/**
 * hwerr_str
 */
const char *hwerr_str(int err)
{
	return (find_txt_item(-err, hwerr, "HWERR_NDEF"));
}
#endif

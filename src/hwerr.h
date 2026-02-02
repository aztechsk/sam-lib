/*
 * hwerr.h
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

#ifndef HWERR_H
#define HWERR_H

enum hwerr {
        ENOERR, // No Error.
        EDMA,	// DMA error.
        ETMO,   // TiMeOut.
        EHW,    // HardWare in unexpected state.
	EADDR,  // Bad data ADDRess or data size.
	ERCV,   // Data ReCeiVe (general error).
	ESND,	// Data SeND (general error).
	EFMT,	// Format error.
	EBFOV,	// BuFfer OVerflow.
	ENRDY,  // Not ReaDY.
	EERASE, // Erase error.
	EDATA,  // Data error.
        EINTR,  // Operation interrupted.
	ENACK   // Operation negative aknowledgement.
};

#if TERMOUT == 1
/**
 * hwerr_str
 */
const char *hwerr_str(int err);
#endif

#endif

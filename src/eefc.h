/*
 * eefc.h
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

#ifndef EEFC_H
#define EEFC_H

#ifndef EEFC_FLASH_CMD
 #define EEFC_FLASH_CMD 0
#endif

/**
 * init_flash
 */
void init_flash(Efc *efc, unsigned int clk);

#if EEFC_FLASH_CMD == 1
enum eefc_flash_error {
	EEFC_FLASH_LOCK_ERROR = 0x01,
	EEFC_FLASH_CMD_ERROR  = 0x02,
	EEFC_FLASH_MEM_ERROR  = 0x04,
        EEFC_FLASH_DATA_ERROR = 0x08
};

/**
 * write_flash_page
 *
 * Write FLASH_PAGE_SIZE bytes from data buffer to internal flash memory page.
 *
 * @efc: EFC instance.
 * @p_adr: Page address in memory space.
 * @d_buf: Data buffer (may be unaligned).
 *
 * Returns: 0 - success; EEFC_FLASH_LOCK_ERROR | EEFC_FLASH_CMD_ERROR |
 *          EEFC_FLASH_MEM_ERROR | EEFC_FLASH_DATA_ERROR - error bitmap.
 */
unsigned int write_flash_page(Efc *efc, void *p_adr, uint8_t *d_buf);
#endif

#if TERMOUT == 1
/**
 * log_efc_cfg
 */
void log_efc_cfg(Efc *efc);
#endif

#endif

/*
 * eefc.c
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
#include "msgconf.h"
#include "eefc.h"

#define RAMFUNC __attribute__ ((section(".fast"))) __attribute__ ((noinline))

enum eefc_cmd {
	EEFC_CMD_GETD  = 0x00,
        EEFC_CMD_WP    = 0x01,
        EEFC_CMD_WPL   = 0x02,
        EEFC_CMD_EWP   = 0x03,
        EEFC_CMD_EWPL  = 0x04,
        EEFC_CMD_EA    = 0x05,
        EEFC_CMD_SLB   = 0x08,
        EEFC_CMD_CLB   = 0x09,
        EEFC_CMD_GLB   = 0x0A,
        EEFC_CMD_SGPB  = 0x0B,
        EEFC_CMD_CGPB  = 0x0C,
        EEFC_CMD_GGPB  = 0x0D,
        EEFC_CMD_STUI  = 0x0E,
        EEFC_CMD_SPUI  = 0x0F,
        EEFC_CMD_GCALB = 0x10
};

static void write_fmr(Efc *efc, unsigned int val) RAMFUNC;
#if EEFC_FLASH_CMD == 1
static unsigned int cmd(Efc *efc, enum eefc_cmd cmd, unsigned int arg) RAMFUNC;
#endif

/**
 * init_flash
 */
void init_flash(Efc *efc, unsigned int clk)
{
	if (clk <= CHIP_FREQ_FWS_0) {
		write_fmr(efc, EEFC_FMR_FWS(0));
	} else if (clk <= CHIP_FREQ_FWS_1) {
		write_fmr(efc, EEFC_FMR_FWS(1));
	} else if (clk <= CHIP_FREQ_FWS_2) {
		write_fmr(efc, EEFC_FMR_FWS(2));
#ifdef CHIP_FREQ_FWS_3
	} else if (clk <= CHIP_FREQ_FWS_3) {
		write_fmr(efc, EEFC_FMR_FWS(3));
#endif
#ifdef CHIP_FREQ_FWS_4
	} else if (clk <= CHIP_FREQ_FWS_4) {
		write_fmr(efc, EEFC_FMR_FWS(4));
#endif
#ifdef CHIP_FREQ_FWS_5
	} else if (clk <= CHIP_FREQ_FWS_5) {
		write_fmr(efc, EEFC_FMR_FWS(5));
#endif
	} else {
		for (;;);
	}
}

#if EEFC_FLASH_CMD == 1
/**
 * write_flash_page
 */
unsigned int write_flash_page(Efc *efc, void *p_adr, uint8_t *d_buf)
{
	uint32_t w;
	unsigned int err, ret = 0;
	int pg, fws;

	if ((uint32_t) p_adr % IFLASH_PAGE_SIZE) {
		crit_err_exit(BAD_PARAMETER);
	}
	fws = (efc->EEFC_FMR & EEFC_FMR_FWS_Msk) >> EEFC_FMR_FWS_Pos;
	write_fmr(efc, (efc->EEFC_FMR & ~EEFC_FMR_FWS_Msk) | EEFC_FMR_FWS(CHIP_FLASH_WRITE_WAIT_STATE));
	for (int i = 0, j = 0; i < (int) IFLASH_PAGE_SIZE / 4; i++) {
		w = d_buf[j++];
		w |= d_buf[j++] << 8;
		w |= d_buf[j++] << 16;
		w |= d_buf[j++] << 24;
		*((uint32_t *) p_adr + i) = w;
	}
	pg = ((uint8_t *) p_adr - (uint8_t *) IFLASH_ADDR) / IFLASH_PAGE_SIZE;
	if ((err = cmd(efc, EEFC_CMD_EWP, pg)) != 0) {
		if (err & EEFC_FSR_FLOCKE) {
			ret |= EEFC_FLASH_LOCK_ERROR;
		}
		if (err & EEFC_FSR_FCMDE) {
			ret |= EEFC_FLASH_CMD_ERROR;
		}
#if SAM4N_SERIES || SAM4S_SERIES
		if (err & EEFC_FSR_FLERR) {
			ret |= EEFC_FLASH_MEM_ERROR;
		}
#endif
		goto fn_exit;
	}
	for (int i = 0; i < (int) IFLASH_PAGE_SIZE; i++) {
		if (*((uint8_t *) p_adr + i) != d_buf[i]) {
			ret |= EEFC_FLASH_DATA_ERROR;
			break;
		}
	}
fn_exit:
	write_fmr(efc, (efc->EEFC_FMR & ~EEFC_FMR_FWS_Msk) | EEFC_FMR_FWS(fws));
	return (ret);
}

/**
 * cmd
 */
static unsigned int cmd(Efc *efc, enum eefc_cmd cmd, unsigned int arg)
{
	unsigned int st;

	__DSB();
	efc->EEFC_FCR = 0x5A << 24 | EEFC_FCR_FARG(arg) | cmd;
	while (!((st = efc->EEFC_FSR) & EEFC_FSR_FRDY)) {
		;
	}
#if SAM3N_SERIES || SAM3S_SERIES
	return (st & (EEFC_FSR_FLOCKE | EEFC_FSR_FCMDE));
#elif SAM4N_SERIES || SAM4S_SERIES
	return (st & (EEFC_FSR_FLERR | EEFC_FSR_FLOCKE | EEFC_FSR_FCMDE));
#else
 #error "SAM_SERIES definition error"
#endif
}
#endif

/**
 * write_fmr
 */
static void write_fmr(Efc *efc, unsigned int val)
{
	__DSB();
	efc->EEFC_FMR = val;
}

#if TERMOUT == 1
/**
 * log_efc_cfg
 */
void log_efc_cfg(Efc *efc)
{
	unsigned int mr;
	int n;

#ifdef EFC1
	n = (efc == EFC1) ? 1 : 0;
#else
	n = 0;
#endif
	mr = efc->EEFC_FMR;
#if SAM3N_SERIES || SAM3S_SERIES
	msg(INF, "eefc.c: cfg%s> FAM=%d SCOD=%d FWS=%d\n",
	    (n == 1) ? "2" : "",
	    (mr & EEFC_FMR_FAM) ? 1 : 0,
	    (mr & EEFC_FMR_SCOD) ? 1 : 0,
	    (mr & EEFC_FMR_FWS_Msk) >> EEFC_FMR_FWS_Pos);
#elif SAM4N_SERIES || SAM4S_SERIES
	msg(INF, "eefc.c: cfg%s> CLOE=%d FAM=%d SCOD=%d FWS=%d\n",
	    (n == 1) ? "2" : "",
	    (mr & EEFC_FMR_CLOE) ? 1 : 0,
	    (mr & EEFC_FMR_FAM) ? 1 : 0,
	    (mr & EEFC_FMR_SCOD) ? 1 : 0,
	    (mr & EEFC_FMR_FWS_Msk) >> EEFC_FMR_FWS_Pos);
#else
 #error "SAM_SERIES definition error"
#endif
}
#endif

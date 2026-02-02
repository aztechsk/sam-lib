/*
 * hsmci_sd.c
 *
 * Copyright (c) 2025 Jan Rusnak <jan@rusnak.sk>
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
#include <stdint.h>
#include <stddef.h>
#include "sysconf.h"
#include "board.h"
#include <mmio.h>
#include "criterr.h"
#include "atom.h"
#include "msgconf.h"
#include "hwerr.h"
#include "pmc.h"
#include "hsmci_cmd.h"
#include "hsmci_sd.h"

#if HSMCI_SD == 1

_Static_assert((HSMCI_SD_DLINE_NUM == 1) || (HSMCI_SD_DLINE_NUM == 4),
               "HSMCI_SD_DLINE_NUM must be 1 or 4");

#define WAIT_INTR_MS 10000
#define R1B_BUSY_TMO_MS 10000
#define SEND_CLOCK_TMO_MS 30
#define HSMCI_BLOCK_SIZE 512
#define HSMCI_400K_CLOCK 400000

static QueueHandle_t sr_que;
static unsigned int r1b_busy_tmo = R1B_BUSY_TMO_MS;

static unsigned int stat_spurious_int_cnt;
static unsigned int stat_sr_unre_cnt;
static unsigned int stat_sr_ovre_cnt;
static unsigned int stat_isr_que_full_cnt;
static unsigned int stat_sr_cstoe_cnt;
static unsigned int stat_sr_dtoe_cnt;
static unsigned int stat_sr_dcrce_cnt;
static unsigned int stat_sr_rtoe_cnt;
static unsigned int stat_sr_rende_cnt;
static unsigned int stat_sr_rcrce_cnt;
static unsigned int stat_sr_rdire_cnt;
static unsigned int stat_sr_rinde_cnt;
static unsigned int stat_busy_cnt;
static unsigned int stat_intr_tmo_cnt;
static unsigned int stat_no_xfr_done_cnt;
static unsigned int stat_rx_dma_err_cnt;
static unsigned int stat_tx_dma_err_cnt;
static unsigned int stat_rd_err_cnt;
static unsigned int stat_wr_err_cnt;
static unsigned int stat_wr_n_cmdrdy_cnt;
static unsigned int stat_wr_n_notbusy_cnt;
static unsigned int stat_wr_n_blke_cnt;
static unsigned int stat_rx_blk_cnt;
static unsigned int stat_tx_blk_cnt;

static void reset_hsmci(void);
static void sr_err_cnt(unsigned int sr);

/**
 * init_hsmci
 */
void init_hsmci(void)
{
	if (sr_que == NULL) {
		sr_que = xQueueCreate(1, sizeof(unsigned int));
		if (sr_que == NULL) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
	NVIC_DisableIRQ(HSMCI_IRQn);
	enable_periph_clk(ID_HSMCI);
	HSMCI->HSMCI_CR = HSMCI_CR_SWRST;
	HSMCI->HSMCI_IDR = ~0;
	HSMCI->HSMCI_SR;
	NVIC_ClearPendingIRQ(HSMCI_IRQn);
	NVIC_SetPriority(HSMCI_IRQn, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(HSMCI_IRQn);
	HSMCI->HSMCI_DTOR = HSMCI_DTOR_DTOMUL_1048576 | HSMCI_DTOR_DTOCYC(2);
	HSMCI->HSMCI_CSTOR = HSMCI_CSTOR_CSTOMUL_1048576 | HSMCI_CSTOR_CSTOCYC(2);
	HSMCI->HSMCI_CFG = HSMCI_CFG_FERRCTRL | HSMCI_CFG_FIFOMODE;
	HSMCI->HSMCI_MR = HSMCI_MR_PWSDIV(0x7);
	hsmci_set_clock(HSMCI_400K_CLOCK, NULL, FALSE);
	HSMCI->HSMCI_SDCR = HSMCI_SDCR_SDCBUS_1 | HSMCI_SDCR_SDCSEL_SLOTA;
	HSMCI->HSMCI_CR = HSMCI_CR_PWSEN | HSMCI_CR_MCIEN;
}

/**
 * hsmci_soft_reset
 */
void hsmci_soft_reset(void)
{
	reset_hsmci();
}

/**
 * hsmci_set_clock
 */
void hsmci_set_clock(unsigned int clock_hz, unsigned int *clock_hz_set, boolean_t overclk)
{
	unsigned int clkdiv;
	unsigned int max_div;
	unsigned int actual_clock;

	if (!clock_hz) {
		crit_err_exit(BAD_PARAMETER);
	}
	max_div = HSMCI_MR_CLKDIV_Msk >> HSMCI_MR_CLKDIV_Pos;
	if (clock_hz >= ((F_MCK + 1) / 2)) {
		clkdiv = 0;
	} else {
		unsigned int denom = 2 * clock_hz;
		unsigned int n;
		if (overclk) {
			n = (F_MCK + (denom / 2)) / denom;
		} else {
			n = (F_MCK + denom - 1) / denom;
		}
		if (n == 0) {
			n = 1;
		}
		clkdiv = n - 1;
		if (clkdiv > max_div) {
			clkdiv = max_div;
		}
	}
	actual_clock = F_MCK / (2 * (clkdiv + 1));
	if (clock_hz_set) {
		*clock_hz_set = actual_clock;
	}
	HSMCI->HSMCI_MR &= ~HSMCI_MR_CLKDIV_Msk;
	HSMCI->HSMCI_MR |= HSMCI_MR_CLKDIV(clkdiv);
}

/**
 * hsmci_set_bus_width
 */
void hsmci_set_bus_width(enum hsmci_bus_width bw)
{
	unsigned int busw;

	busw = HSMCI->HSMCI_SDCR;
	busw &= ~HSMCI_SDCR_SDCBUS_Msk;
	switch (bw) {
	case HSMCI_BUS_WIDTH_1 :
		busw |= HSMCI_SDCR_SDCBUS_1;
		break;
	case HSMCI_BUS_WIDTH_4 :
#if HSMCI_SD_DLINE_NUM == 1
		crit_err_exit(BAD_PARAMETER);
#else
		busw |= HSMCI_SDCR_SDCBUS_4;
#endif
		break;
	default :
		crit_err_exit(BAD_PARAMETER);
		break;
	}
	HSMCI->HSMCI_SDCR = busw;
}

/**
 * hsmci_enable_hspeed
 */
void hsmci_enable_hspeed(void)
{
	HSMCI->HSMCI_CFG |= HSMCI_CFG_HSMODE;
}

/**
 * hsmci_disable_hspeed
 */
void hsmci_disable_hspeed(void)
{
	HSMCI->HSMCI_CFG &= ~HSMCI_CFG_HSMODE;
}

/**
 * hsmci_send_clock
 */
int hsmci_send_clock(void)
{
	unsigned int sr;

	HSMCI->HSMCI_MR &= ~(HSMCI_MR_PDCMODE | HSMCI_MR_WRPROOF | HSMCI_MR_RDPROOF);
	HSMCI->HSMCI_ARGR = 0;
	HSMCI->HSMCI_CMDR = HSMCI_CMDR_RSPTYP_NORESP | HSMCI_CMDR_SPCMD_INIT | HSMCI_CMDR_OPDCMD_OPENDRAIN;
	TickType_t t0 = xTaskGetTickCount();
	for (;;) {
		sr = HSMCI->HSMCI_SR;
		if (sr & HSMCI_SR_CMDRDY) {
			break;
		}
		if ((xTaskGetTickCount() - t0) > ms_to_os_ticks(SEND_CLOCK_TMO_MS)) {
			reset_hsmci();
			return (-EHW);
		}
		taskYIELD();
	}
	return (0);
}

/**
 * hsmci_set_next_r1b_busy_tmo_ms
 */
void hsmci_set_next_r1b_busy_tmo_ms(unsigned int tmo_ms)
{
	r1b_busy_tmo = tmo_ms;
}

/**
 * hsmci_send_cmd
 */
int hsmci_send_cmd(unsigned int cmd, unsigned int arg, hsmci_resp_t *resp)
{
	unsigned int cmdr;
	unsigned int sr;

	HSMCI->HSMCI_MR &= ~(HSMCI_MR_PDCMODE | HSMCI_MR_WRPROOF | HSMCI_MR_RDPROOF);
	unsigned int cmd_idx = SDMMC_CMD_GET_INDEX(cmd);
	cmdr = HSMCI_CMDR_SPCMD_STD | HSMCI_CMDR_CMDNB(cmd_idx);
	if (cmd & SDMMC_RESP_PRESENT) {
		cmdr |= HSMCI_CMDR_MAXLAT;
		if (cmd & SDMMC_RESP_136) {
			cmdr |= HSMCI_CMDR_RSPTYP_136_BIT;
		} else if (cmd & SDMMC_RESP_BUSY) {
			cmdr |= HSMCI_CMDR_RSPTYP_R1B;
		} else {
			cmdr |= HSMCI_CMDR_RSPTYP_48_BIT;
		}
	}
	if (cmd & SDMMC_CMD_OPENDRAIN) {
		cmdr |= HSMCI_CMDR_OPDCMD_OPENDRAIN;
	}
	HSMCI->HSMCI_ARGR = arg;
	taskENTER_CRITICAL();
	HSMCI->HSMCI_CMDR = cmdr;
	if (cmd & SDMMC_RESP_CRC) {
		HSMCI->HSMCI_IER = HSMCI_IER_CSTOE | HSMCI_IER_RTOE | HSMCI_IER_RENDE | HSMCI_IER_RCRCE | HSMCI_IER_RDIRE | HSMCI_IER_RINDE |
				   HSMCI_IER_CMDRDY;
	} else {
		HSMCI->HSMCI_IER = HSMCI_IER_CSTOE | HSMCI_IER_RTOE | HSMCI_IER_RENDE | HSMCI_IER_RDIRE | HSMCI_IER_RINDE | HSMCI_IER_CMDRDY;
	}
	taskEXIT_CRITICAL();
	if (pdFALSE == xQueueReceive(sr_que, &sr, ms_to_os_ticks(WAIT_INTR_MS))) {
		reset_hsmci();
		xQueueReceive(sr_que, &sr, 0);
		stat_intr_tmo_cnt++;
		return (-EHW);
	}
	if (!(cmd & SDMMC_RESP_CRC)) {
		sr &= ~HSMCI_SR_RCRCE;
	}
	if (sr & (HSMCI_SR_CSTOE | HSMCI_SR_RTOE | HSMCI_SR_RENDE | HSMCI_SR_RCRCE | HSMCI_SR_RDIRE | HSMCI_SR_RINDE)) {
		reset_hsmci();
		sr_err_cnt(sr);
		return (-EHW);
	}
	if (!(sr & HSMCI_SR_CMDRDY)) {
		reset_hsmci();
		return (-EHW);
	}
	if (cmd & SDMMC_RESP_BUSY) {
		HSMCI->HSMCI_IER = HSMCI_IER_NOTBUSY;
		if (pdFALSE == xQueueReceive(sr_que, &sr, ms_to_os_ticks(r1b_busy_tmo))) {
			reset_hsmci();
			xQueueReceive(sr_que, &sr, 0);
			stat_intr_tmo_cnt++;
			return (-EHW);
		}
		if (!(sr & HSMCI_SR_NOTBUSY)) {
			reset_hsmci();
			stat_busy_cnt++;
			r1b_busy_tmo = R1B_BUSY_TMO_MS;
			return (-EHW);
		}
		r1b_busy_tmo = R1B_BUSY_TMO_MS;
	}
	if (resp && (cmd & SDMMC_RESP_PRESENT)) {
		if (cmd & SDMMC_RESP_136) {
			uint8_t *rp = (uint8_t *) &resp->r2;
			uint32_t r_32;
			for (int i = 0; i < 4; i++) {
				// HSMCI_RSPR incremented internally by read OP.
				r_32 = HSMCI->HSMCI_RSPR[0];
				*rp = (r_32 >> 24) & 0xFF;
				rp++;
				*rp = (r_32 >> 16) & 0xFF;
				rp++;
				*rp = (r_32 >>  8) & 0xFF;
				rp++;
				*rp = (r_32 >>  0) & 0xFF;
				rp++;
			}
		} else {
			resp->r1 = HSMCI->HSMCI_RSPR[0];
		}
	}
	return (0);
}

/**
 * hsmci_send_data_cmd
 */
int hsmci_send_data_cmd(unsigned int cmd, unsigned int arg, void *buf, size_t len, hsmci_resp_t *resp)
{
	unsigned int cmdr;
	unsigned int sr;
	unsigned int nb_words;
	unsigned int cmd_idx;

	if (len == 0 || (len > HSMCI_BLOCK_SIZE) || (len & 3u)) {
		crit_err_exit(BAD_PARAMETER);
	}
	if (cmd & SDMMC_CMD_MULTI_BLOCK) {
		crit_err_exit(BAD_PARAMETER);
	}
	nb_words = len / 4;
	HSMCI->HSMCI_MR |= HSMCI_MR_PDCMODE | HSMCI_MR_RDPROOF | HSMCI_MR_WRPROOF;
	cmd_idx = SDMMC_CMD_GET_INDEX(cmd);
	cmdr = HSMCI_CMDR_SPCMD_STD | HSMCI_CMDR_CMDNB(cmd_idx);
	if (cmd & SDMMC_RESP_PRESENT) {
		cmdr |= HSMCI_CMDR_MAXLAT;
		if (cmd & SDMMC_RESP_136) {
			cmdr |= HSMCI_CMDR_RSPTYP_136_BIT;
		} else if (cmd & SDMMC_RESP_BUSY) {
			crit_err_exit(BAD_PARAMETER);
		} else {
			cmdr |= HSMCI_CMDR_RSPTYP_48_BIT;
		}
	}
	if (cmd & SDMMC_CMD_OPENDRAIN) {
		cmdr |= HSMCI_CMDR_OPDCMD_OPENDRAIN;
	}
	cmdr |= HSMCI_CMDR_TRDIR_READ | HSMCI_CMDR_TRCMD_START_DATA | HSMCI_CMDR_TRTYP_SINGLE;
	if (cmd & SDMMC_CMD_WRITE) {
		crit_err_exit(BAD_PARAMETER);
	}
	HSMCI->HSMCI_BLKR = HSMCI_BLKR_BLKLEN(len) | HSMCI_BLKR_BCNT(1);
	HSMCI->HSMCI_RPR  = (uint32_t) buf;
	HSMCI->HSMCI_RCR  = nb_words;
	HSMCI->HSMCI_RNCR = 0;
	HSMCI->HSMCI_ARGR = arg;
	taskENTER_CRITICAL();
	HSMCI->HSMCI_PTCR = HSMCI_PTCR_RXTEN;
	HSMCI->HSMCI_CMDR = cmdr;
	HSMCI->HSMCI_IER = HSMCI_IER_UNRE | HSMCI_IER_OVRE | HSMCI_IER_XFRDONE | HSMCI_IER_CSTOE |
			   HSMCI_IER_DTOE | HSMCI_IER_DCRCE | HSMCI_IER_RTOE | HSMCI_IER_RENDE |
			   HSMCI_IER_RCRCE | HSMCI_IER_RDIRE | HSMCI_IER_RINDE;
	taskEXIT_CRITICAL();
	if (pdFALSE == xQueueReceive(sr_que, &sr, ms_to_os_ticks(WAIT_INTR_MS))) {
		reset_hsmci();
		xQueueReceive(sr_que, &sr, 0);
		stat_intr_tmo_cnt++;
		return (-EHW);
	}
	HSMCI->HSMCI_PTCR = HSMCI_PTCR_RXTDIS | HSMCI_PTCR_TXTDIS;
	if (sr & (HSMCI_SR_UNRE | HSMCI_SR_OVRE | HSMCI_SR_CSTOE | HSMCI_SR_DTOE | HSMCI_SR_DCRCE |
	    HSMCI_SR_RTOE | HSMCI_SR_RENDE | HSMCI_SR_RCRCE | HSMCI_SR_RDIRE | HSMCI_SR_RINDE)) {
		reset_hsmci();
		sr_err_cnt(sr);
		return (-EHW);
	}
	if (HSMCI->HSMCI_RCR) {
		reset_hsmci();
		stat_rx_dma_err_cnt++;
		return (-EHW);
	}
	if (!(sr & HSMCI_SR_XFRDONE)) {
		reset_hsmci();
		stat_no_xfr_done_cnt++;
		return (-EHW);
	}
	HSMCI->HSMCI_MR &= ~(HSMCI_MR_PDCMODE | HSMCI_MR_WRPROOF | HSMCI_MR_RDPROOF);
	if (resp && (cmd & SDMMC_RESP_PRESENT)) {
		if (cmd & SDMMC_RESP_136) {
			uint8_t *rp = (uint8_t *) &resp->r2;
			uint32_t r_32;
			for (int i = 0; i < 4; i++) {
				r_32 = HSMCI->HSMCI_RSPR[0];
				*rp++ = (r_32 >> 24) & 0xFF;
				*rp++ = (r_32 >> 16) & 0xFF;
				*rp++ = (r_32 >>  8) & 0xFF;
				*rp++ = (r_32 >>  0) & 0xFF;
			}
		} else {
			resp->r1 = HSMCI->HSMCI_RSPR[0];
		}
	}
	return (0);
}

/**
 * hsmci_read_blocks
 */
int hsmci_read_blocks(size_t lba, int block_cnt, void *buf)
{
	unsigned int cmdr;
	unsigned int sr;
	unsigned int nb_data;
	int ret = 0;

	if (block_cnt < 1) {
		if (block_cnt == 0) {
			return (0);
		} else {
			crit_err_exit(BAD_PARAMETER);
		}
	}
	nb_data = block_cnt * HSMCI_BLOCK_SIZE;
	if (block_cnt == 1) {
		cmdr = HSMCI_CMDR_TRTYP_SINGLE | HSMCI_CMDR_CMDNB(SDMMC_CMD_GET_INDEX(SDMMC_CMD17_READ_SINGLE_BLOCK));
	} else {
		cmdr = HSMCI_CMDR_TRTYP_MULTIPLE | HSMCI_CMDR_CMDNB(SDMMC_CMD_GET_INDEX(SDMMC_CMD18_READ_MULTIPLE_BLOCK));
	}
	cmdr |=  HSMCI_CMDR_TRDIR_READ | HSMCI_CMDR_TRCMD_START_DATA | HSMCI_CMDR_MAXLAT | HSMCI_CMDR_RSPTYP_48_BIT;
	HSMCI->HSMCI_MR |= HSMCI_MR_PDCMODE | HSMCI_MR_RDPROOF | HSMCI_MR_WRPROOF;
	HSMCI->HSMCI_BLKR = HSMCI_BLKR_BLKLEN(HSMCI_BLOCK_SIZE) | HSMCI_BLKR_BCNT(block_cnt);
	HSMCI->HSMCI_RPR = (uint32_t) buf;
	HSMCI->HSMCI_RCR = nb_data / 4;
	HSMCI->HSMCI_RNCR = 0;
	HSMCI->HSMCI_ARGR = lba;
	taskENTER_CRITICAL();
	HSMCI->HSMCI_PTCR = HSMCI_PTCR_RXTEN;
	HSMCI->HSMCI_CMDR = cmdr;
	HSMCI->HSMCI_IER = HSMCI_IER_UNRE | HSMCI_IER_OVRE | HSMCI_IER_CSTOE | HSMCI_IER_DTOE |
	                   HSMCI_IER_DCRCE | HSMCI_IER_RTOE | HSMCI_IER_RENDE | HSMCI_IER_RCRCE |
			   HSMCI_IER_RDIRE | HSMCI_IER_RINDE | HSMCI_IER_XFRDONE;
	taskEXIT_CRITICAL();
	if (pdFALSE == xQueueReceive(sr_que, &sr, ms_to_os_ticks(WAIT_INTR_MS))) {
		reset_hsmci();
		xQueueReceive(sr_que, &sr, 0);
		stat_intr_tmo_cnt++;
		return (-EHW);
	}
	HSMCI->HSMCI_PTCR = HSMCI_PTCR_RXTDIS | HSMCI_PTCR_TXTDIS;
	if (sr & (HSMCI_SR_UNRE | HSMCI_SR_OVRE | HSMCI_SR_CSTOE | HSMCI_SR_DTOE | HSMCI_SR_DCRCE |
	    HSMCI_SR_RTOE | HSMCI_SR_RENDE | HSMCI_SR_RCRCE | HSMCI_SR_RDIRE | HSMCI_SR_RINDE)) {
		reset_hsmci();
		sr_err_cnt(sr);
		return (-EHW);
	}
	if (HSMCI->HSMCI_RCR) {
		reset_hsmci();
		stat_rx_dma_err_cnt++;
		return (-EHW);
	}
	if (!(sr & HSMCI_SR_XFRDONE)) {
		reset_hsmci();
		stat_no_xfr_done_cnt++;
		return (-EHW);
	}
	HSMCI->HSMCI_MR &= ~(HSMCI_MR_PDCMODE | HSMCI_MR_WRPROOF | HSMCI_MR_RDPROOF);
	hsmci_resp_t resp;
	resp.r1 = HSMCI->HSMCI_RSPR[0];
	if (resp.r1 & (CARD_STATUS_ERR_RD_WR | CARD_STATUS_COM_CRC_ERROR)) {
		stat_rd_err_cnt++;
		return (-EHW);
	}
	if (block_cnt > 1) {
		if ((ret = hsmci_send_cmd(SDMMC_CMD12_STOP_TRANSMISSION, 0, &resp))) {
			return (ret);
		}
		if (resp.r1 & (CARD_STATUS_ERR_RD_WR | CARD_STATUS_COM_CRC_ERROR)) {
			stat_rd_err_cnt++;
			return (-EHW);
		}
	}
	stat_rx_blk_cnt += block_cnt;
	return (ret);
}

/**
 * hsmci_write_blocks
 */
int hsmci_write_blocks(size_t lba, int block_cnt, const void *buf)
{
	unsigned int cmdr;
	unsigned int sr;
	unsigned int nb_data;
	hsmci_resp_t resp;
	unsigned int ier;
	int ret = 0;

	if (block_cnt < 1) {
		if (block_cnt == 0) {
			return (0);
		} else {
			crit_err_exit(BAD_PARAMETER);
		}
	}
	nb_data = block_cnt * HSMCI_BLOCK_SIZE;
	if (block_cnt == 1) {
		cmdr = HSMCI_CMDR_TRTYP_SINGLE | HSMCI_CMDR_CMDNB(SDMMC_CMD_GET_INDEX(SDMMC_CMD24_WRITE_BLOCK));
	} else {
		cmdr = HSMCI_CMDR_TRTYP_MULTIPLE | HSMCI_CMDR_CMDNB(SDMMC_CMD_GET_INDEX(SDMMC_CMD25_WRITE_MULTIPLE_BLOCK));
	}
	cmdr |= HSMCI_CMDR_TRCMD_START_DATA | HSMCI_CMDR_MAXLAT | HSMCI_CMDR_RSPTYP_48_BIT;
	HSMCI->HSMCI_MR |= HSMCI_MR_PDCMODE | HSMCI_MR_RDPROOF | HSMCI_MR_WRPROOF;
	HSMCI->HSMCI_BLKR = HSMCI_BLKR_BLKLEN(HSMCI_BLOCK_SIZE) | HSMCI_BLKR_BCNT(block_cnt);
	HSMCI->HSMCI_TPR  = (uint32_t) buf;
	HSMCI->HSMCI_TCR  = nb_data / 4;
	HSMCI->HSMCI_TNCR = 0;
	HSMCI->HSMCI_ARGR = lba;
	taskENTER_CRITICAL();
	HSMCI->HSMCI_CMDR = cmdr;
	HSMCI->HSMCI_IER = HSMCI_IER_CSTOE | HSMCI_IER_RTOE | HSMCI_IER_RENDE | HSMCI_IER_RCRCE | HSMCI_IER_RDIRE | HSMCI_IER_RINDE |
			   HSMCI_IER_CMDRDY;
	taskEXIT_CRITICAL();
	if (pdFALSE == xQueueReceive(sr_que, &sr, ms_to_os_ticks(WAIT_INTR_MS))) {
		reset_hsmci();
		xQueueReceive(sr_que, &sr, 0);
		stat_intr_tmo_cnt++;
		return (-EHW);
	}
	if (sr & (HSMCI_SR_CSTOE | HSMCI_SR_RTOE | HSMCI_SR_RENDE | HSMCI_SR_RCRCE | HSMCI_SR_RDIRE | HSMCI_SR_RINDE)) {
		reset_hsmci();
		sr_err_cnt(sr);
		return (-EHW);
	}
	if (!(sr & HSMCI_SR_CMDRDY)) {
		reset_hsmci();
		stat_wr_n_cmdrdy_cnt++;
		return (-EHW);
	}
	resp.r1 = HSMCI->HSMCI_RSPR[0];
	if (resp.r1 & (CARD_STATUS_ERR_RD_WR | CARD_STATUS_COM_CRC_ERROR)) {
		stat_wr_err_cnt++;
		return (-EHW);
	}
	ier = HSMCI_IER_UNRE | HSMCI_IER_OVRE | HSMCI_IER_CSTOE | HSMCI_IER_DTOE | HSMCI_IER_DCRCE | HSMCI_IER_RTOE |
	      HSMCI_IER_RENDE | HSMCI_IER_RCRCE | HSMCI_IER_RDIRE | HSMCI_IER_RINDE;
	if (block_cnt == 1) {
		ier |= HSMCI_IER_NOTBUSY;
	} else {
		ier |= HSMCI_IER_BLKE;
	}
	taskENTER_CRITICAL();
	HSMCI->HSMCI_PTCR = HSMCI_PTCR_TXTEN;
	HSMCI->HSMCI_IER = ier;
	taskEXIT_CRITICAL();
	if (pdFALSE == xQueueReceive(sr_que, &sr, ms_to_os_ticks(WAIT_INTR_MS))) {
		reset_hsmci();
		xQueueReceive(sr_que, &sr, 0);
		stat_intr_tmo_cnt++;
		return (-EHW);
	}
	HSMCI->HSMCI_PTCR = HSMCI_PTCR_RXTDIS | HSMCI_PTCR_TXTDIS;
	if (sr & (HSMCI_SR_UNRE | HSMCI_SR_OVRE | HSMCI_SR_CSTOE | HSMCI_SR_DTOE | HSMCI_SR_DCRCE |
	    HSMCI_SR_RTOE | HSMCI_SR_RENDE | HSMCI_SR_RCRCE | HSMCI_SR_RDIRE | HSMCI_SR_RINDE)) {
		reset_hsmci();
		sr_err_cnt(sr);
		return (-EHW);
	}
	if (block_cnt == 1) {
		if (!(sr & HSMCI_SR_NOTBUSY)) {
			reset_hsmci();
			stat_wr_n_notbusy_cnt++;
			return (-EHW);
		}
	} else {
		if (!(sr & HSMCI_SR_BLKE)) {
			reset_hsmci();
			stat_wr_n_blke_cnt++;
			return (-EHW);
		}
	}
	if (HSMCI->HSMCI_TCR) {
		reset_hsmci();
		stat_tx_dma_err_cnt++;
		return (-EHW);
	}
	HSMCI->HSMCI_MR &= ~(HSMCI_MR_PDCMODE | HSMCI_MR_WRPROOF | HSMCI_MR_RDPROOF);
	if (block_cnt > 1) {
		if ((ret = hsmci_send_cmd(SDMMC_CMD12_STOP_TRANSMISSION, 0, &resp))) {
			return (ret);
		}
		if (resp.r1 & (CARD_STATUS_ERR_RD_WR | CARD_STATUS_COM_CRC_ERROR)) {
			stat_wr_err_cnt++;
			return (-EHW);
		}
	}
	stat_tx_blk_cnt += block_cnt;
	return (ret);
}

/**
 * reset_hsmci
 */
static void reset_hsmci(void)
{
	HSMCI->HSMCI_IDR = ~0;
	HSMCI->HSMCI_PTCR = HSMCI_PTCR_RXTDIS | HSMCI_PTCR_TXTDIS;
	unsigned int dtor = HSMCI->HSMCI_DTOR;
	unsigned int cstor = HSMCI->HSMCI_CSTOR;
	unsigned int cfg = HSMCI->HSMCI_CFG;
	unsigned int mr = HSMCI->HSMCI_MR;
	mr &= ~(HSMCI_MR_PDCMODE | HSMCI_MR_WRPROOF | HSMCI_MR_RDPROOF);
	unsigned int sdcr = HSMCI->HSMCI_SDCR;
	HSMCI->HSMCI_CR = HSMCI_CR_SWRST;
	HSMCI->HSMCI_IDR = ~0;
	NVIC_ClearPendingIRQ(HSMCI_IRQn);
	HSMCI->HSMCI_SR;
	HSMCI->HSMCI_DTOR = dtor;
	HSMCI->HSMCI_CSTOR = cstor;
	HSMCI->HSMCI_CFG = cfg;
	HSMCI->HSMCI_MR = mr;
	HSMCI->HSMCI_SDCR = sdcr;
	HSMCI->HSMCI_PTCR = HSMCI_PTCR_RXTDIS | HSMCI_PTCR_TXTDIS;
	HSMCI->HSMCI_CR = HSMCI_CR_PWSEN | HSMCI_CR_MCIEN;
}

/**
 * HSMCI_Handler
 */
void HSMCI_Handler(void)
{
	unsigned int sr;
        BaseType_t tsk_wkn = pdFALSE;

	sr = HSMCI->HSMCI_SR;
	if (sr & HSMCI->HSMCI_IMR) {
		if (errQUEUE_FULL == xQueueSendFromISR(sr_que, &sr, &tsk_wkn)) {
			stat_isr_que_full_cnt++;
		}
		HSMCI->HSMCI_IDR = ~0;
	} else  {
		stat_spurious_int_cnt++;

	}
	portEND_SWITCHING_ISR(tsk_wkn);
}

#if TERMOUT == 1
/**
 * log_hsmci_stats
 */
void log_hsmci_stats(void)
{
	msg(INF, "hsmci_sd: stat_rx_blk_cnt=%u stat_tx_blk_cnt=%u\n", stat_rx_blk_cnt, stat_tx_blk_cnt);
	if (stat_spurious_int_cnt) {
		msg(INF, "hsmci_sd: stat_spurious_int_cnt=%u\n", stat_spurious_int_cnt);
	}
	if (stat_sr_unre_cnt) {
		msg(INF, "hsmci_sd: stat_sr_unre_cnt=%u\n", stat_sr_unre_cnt);
	}
	if (stat_sr_ovre_cnt) {
		msg(INF, "hsmci_sd: stat_sr_ovre_cnt=%u\n", stat_sr_ovre_cnt);
	}
	if (stat_isr_que_full_cnt) {
		msg(INF, "hsmci_sd: stat_isr_que_full_cnt=%u\n", stat_isr_que_full_cnt);
	}
	if (stat_sr_cstoe_cnt) {
		msg(INF, "hsmci_sd: stat_sr_cstoe_cnt=%u\n", stat_sr_cstoe_cnt);
	}
	if (stat_sr_dtoe_cnt) {
		msg(INF, "hsmci_sd: stat_sr_dtoe_cnt=%u\n", stat_sr_dtoe_cnt);
	}
	if (stat_sr_dcrce_cnt) {
		msg(INF, "hsmci_sd: stat_sr_dcrce_cnt=%u\n", stat_sr_dcrce_cnt);
	}
	if (stat_sr_rtoe_cnt) {
		msg(INF, "hsmci_sd: stat_sr_rtoe_cnt=%u\n", stat_sr_rtoe_cnt);
	}
	if (stat_sr_rende_cnt) {
		msg(INF, "hsmci_sd: stat_sr_rende_cnt=%u\n", stat_sr_rende_cnt);
	}
	if (stat_sr_rcrce_cnt) {
		msg(INF, "hsmci_sd: stat_sr_rcrce_cnt=%u\n", stat_sr_rcrce_cnt);
	}
	if (stat_sr_rdire_cnt) {
		msg(INF, "hsmci_sd: stat_sr_rdire_cnt=%u\n", stat_sr_rdire_cnt);
	}
	if (stat_sr_rinde_cnt) {
		msg(INF, "hsmci_sd: stat_sr_rinde_cnt=%u\n", stat_sr_rinde_cnt);
	}
	if (stat_busy_cnt) {
		msg(INF, "hsmci_sd: stat_busy_cnt=%u\n", stat_busy_cnt);
	}
	if (stat_intr_tmo_cnt) {
		msg(INF, "hsmci_sd: stat_intr_tmo_cnt=%u\n", stat_intr_tmo_cnt);
	}
	if (stat_no_xfr_done_cnt) {
		msg(INF, "hsmci_sd: stat_no_xfr_done_cnt=%u\n", stat_no_xfr_done_cnt);
	}
	if (stat_rx_dma_err_cnt) {
		msg(INF, "hsmci_sd: stat_rx_dma_err_cnt=%u\n", stat_rx_dma_err_cnt);
	}
	if (stat_tx_dma_err_cnt) {
		msg(INF, "hsmci_sd: stat_tx_dma_err_cnt=%u\n", stat_tx_dma_err_cnt);
	}
	if (stat_rd_err_cnt) {
		msg(INF, "hsmci_sd: stat_rd_err_cnt=%u\n", stat_rd_err_cnt);
	}
	if (stat_wr_err_cnt) {
		msg(INF, "hsmci_sd: stat_wr_err_cnt=%u\n", stat_wr_err_cnt);
	}
	if (stat_wr_n_cmdrdy_cnt) {
		msg(INF, "hsmci_sd: stat_wr_n_cmdrdy_cnt=%u\n", stat_wr_n_cmdrdy_cnt);
	}
	if (stat_wr_n_notbusy_cnt) {
		msg(INF, "hsmci_sd: stat_wr_n_notbusy_cnt=%u\n", stat_wr_n_notbusy_cnt);
	}
	if (stat_wr_n_blke_cnt) {
		msg(INF, "hsmci_sd: stat_wr_n_blke_cnt=%u\n", stat_wr_n_blke_cnt);
	}
}
#endif

/**
 * sr_err_cnt
 */
static void sr_err_cnt(unsigned int sr)
{
	if (sr & HSMCI_SR_UNRE) {
		stat_sr_unre_cnt++;
	}
	if (sr & HSMCI_SR_OVRE) {
		stat_sr_ovre_cnt++;
	}
	if (sr & HSMCI_SR_CSTOE) {
		stat_sr_cstoe_cnt++;
	}
	if (sr & HSMCI_SR_DTOE) {
		stat_sr_dtoe_cnt++;
	}
	if (sr & HSMCI_SR_DCRCE) {
		stat_sr_dcrce_cnt++;
	}
	if (sr & HSMCI_SR_RTOE) {
		stat_sr_rtoe_cnt++;
	}
	if (sr & HSMCI_SR_RENDE) {
		stat_sr_rende_cnt++;
	}
	if (sr & HSMCI_SR_RCRCE) {
		stat_sr_rcrce_cnt++;
	}
	if (sr & HSMCI_SR_RDIRE) {
		stat_sr_rdire_cnt++;
	}
	if (sr & HSMCI_SR_RINDE) {
		stat_sr_rinde_cnt++;
	}
}
#endif

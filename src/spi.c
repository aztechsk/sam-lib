/*
 * spi.c
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
#include "atom.h"
#include "msgconf.h"
#include "hwerr.h"
#include "pmc.h"
#include "pio.h"
#include "spi.h"
#include <string.h>

#if SPIM == 1

#define WAIT_PDC_INTR (1000 / portTICK_PERIOD_MS)
#define HW_RESP_TMOUT 1000000

enum spi_pcs {
	SPI_PCS0,
	SPI_PCS1,
        SPI_PCS2 = 3,
        SPI_PCS3 = 7
};

#ifdef ID_SPI
static spim smi;
#endif
#ifdef ID_SPI0
static spim smi0;
#endif
#ifdef ID_SPI1
static spim smi1;
#endif

static boolean_t trans_poll(spim spi, void *buf, int size);
static unsigned int csr_reg(spim_csel csel);
static enum spi_pcs pcs_fld(enum spim_csel_num csn);
static BaseType_t spi_hndlr(spim spi);

/**
 * init_spim
 */
void init_spim(spim spi)
{
	NVIC_DisableIRQ(spi->id);
#if defined(ID_SPI)
	if (spi->id == ID_SPI) {
		spi->mmio = SPI;
                smi = spi;
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
#elif defined(ID_SPI0) && !defined(ID_SPI1)
	if (spi->id == ID_SPI0) {
		spi->mmio = SPI0;
                smi0 = spi;
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
#elif defined(ID_SPI0) && defined(ID_SPI1)
	if (spi->id == ID_SPI0) {
		spi->mmio = SPI0;
                smi0 = spi;
	} else if (spi->id == ID_SPI1) {
		spi->mmio = SPI1;
                smi1 = spi;
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
#else
 #error "ID_SPI not defined"
#endif
	memset(&spi->stats, 0, sizeof(struct spim_stats));
	if (spi->sig == NULL) {
		if (NULL == (spi->sig = xSemaphoreCreateBinary())) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
        enable_periph_clk(spi->id);
        ((Spi *) spi->mmio)->SPI_CR = SPI_CR_SWRST;
	((Spi *) spi->mmio)->SPI_CR = SPI_CR_SPIDIS;
	((Spi *) spi->mmio)->SPI_PTCR = SPI_PTCR_RXTDIS | SPI_PTCR_TXTDIS;
	((Spi *) spi->mmio)->SPI_IDR = ~0;
	NVIC_ClearPendingIRQ(spi->id);
	((Spi *) spi->mmio)->SPI_MR = SPI_MR_MODFDIS | SPI_MR_MSTR;
	NVIC_SetPriority(spi->id, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(spi->id);
	disable_periph_clk(spi->id);
}

/**
 * spi_trans
 */
int spi_trans(spim spi, spim_csel csel, void *buf0, int size0, void *buf1, int size1,
              boolean_t dma)
{
	int ret = 0;
	unsigned int ui, sr;

	if (!(size0 || size1)) {
		return (0);
	}
	if (spi->mtx != NULL && !csel->csel_ext) {
		xSemaphoreTake(spi->mtx, portMAX_DELAY);
	}
#if SPIM_CSEL_LINE_ERR == 1
	if (!csel->csel_ext && !(((Pio *) csel->csel_cont)->PIO_PDSR & csel->csel_pin)) {
		spi->stats.csel_err = 1;
		if (spi->mtx != NULL) {
			xSemaphoreGive(spi->mtx);
		}
		return (-EHW);
	}
#endif
	spi->act_csel = csel;
	enable_periph_clk(spi->id);
	ui = ((Spi *) spi->mmio)->SPI_MR;
	ui &= ~SPI_MR_PCS_Msk;
	if (ui != (SPI_MR_MODFDIS | SPI_MR_MSTR)) {
		spi->stats.mr_cfg_err = 1;
		ret = -EHW;
		goto err_exit;
	}
	ui |= SPI_MR_PCS(pcs_fld(csel->csn));
	((Spi *) spi->mmio)->SPI_MR = ui;
	if (csel->ini) {
		((Spi *) spi->mmio)->SPI_CSR[csel->csn] = csel->csr = csr_reg(csel);
		csel->ini = FALSE;
	} else {
		((Spi *) spi->mmio)->SPI_CSR[csel->csn] = csel->csr;
	}
	((Spi *) spi->mmio)->SPI_CR = SPI_CR_SPIEN;
	sr = ((Spi *) spi->mmio)->SPI_SR;
	sr &= SPI_SR_TDRE | SPI_SR_TXEMPTY;
	if (sr != (SPI_SR_TDRE | SPI_SR_TXEMPTY)) {
		spi->stats.tx_start_err = 1;
		ret = -EHW;
		goto err_exit;
	}
	if ((csel->dma = dma) == DMA_ON) {
		((Spi *) spi->mmio)->SPI_RPR = (unsigned int) buf0;
		((Spi *) spi->mmio)->SPI_RCR = size0;
		((Spi *) spi->mmio)->SPI_TPR = (unsigned int) buf0;
		((Spi *) spi->mmio)->SPI_TCR = size0;
		((Spi *) spi->mmio)->SPI_RNPR = (unsigned int) buf1;
		((Spi *) spi->mmio)->SPI_RNCR = size1;
		((Spi *) spi->mmio)->SPI_TNPR = (unsigned int) buf1;
		((Spi *) spi->mmio)->SPI_TNCR = size1;
                barrier();
                ((Spi *) spi->mmio)->SPI_IER = SPI_IER_RXBUFF;
		((Spi *) spi->mmio)->SPI_PTCR = SPI_PTCR_RXTEN | SPI_PTCR_TXTEN;
                if (pdFALSE == xSemaphoreTake(spi->sig, WAIT_PDC_INTR)) {
			((Spi *) spi->mmio)->SPI_IDR = ~0;
                        xSemaphoreTake(spi->sig, 0);
                        spi->stats.dma_err = 1;
			ret = -EDMA;
			goto err_exit;
		}
		if (((Spi *) spi->mmio)->SPI_RPR != ((Spi *) spi->mmio)->SPI_TPR ||
		    ((Spi *) spi->mmio)->SPI_RNPR != ((Spi *) spi->mmio)->SPI_TNPR ||
		    ((Spi *) spi->mmio)->SPI_RCR || ((Spi *) spi->mmio)->SPI_TCR ||
		    ((Spi *) spi->mmio)->SPI_RNCR || ((Spi *) spi->mmio)->SPI_TNCR) {
			spi->stats.dma_err = 1;
			ret = -EDMA;
			goto err_exit;
		}
	} else if (csel->no_dma_intr == TRUE) {
		csel->buf0 = buf0;
		csel->size0 = size0;
		csel->buf1 = buf1;
                csel->size1 = size1;
		if (size0 > 0) {
			csel->bufn = 0;
			if (csel->bits == SPIM_8_BIT_TRANS) {
				((Spi *) spi->mmio)->SPI_TDR = *((uint8_t *) csel->buf0);
                                csel->buf0 = (uint8_t *) csel->buf0 + 1;
			} else {
				((Spi *) spi->mmio)->SPI_TDR = *((uint16_t *) csel->buf0);
                                csel->buf0 = (uint16_t *) csel->buf0 + 1;
			}
                        csel->size0--;
		} else {
			csel->bufn = 1;
			if (csel->bits == SPIM_8_BIT_TRANS) {
				((Spi *) spi->mmio)->SPI_TDR = *((uint8_t *) csel->buf1);
                                csel->buf1 = (uint8_t *) csel->buf1 + 1;
			} else {
				((Spi *) spi->mmio)->SPI_TDR = *((uint16_t *) csel->buf1);
                                csel->buf1 = (uint16_t *) csel->buf1 + 1;
			}
                        csel->size1--;
		}
                barrier();
                ((Spi *) spi->mmio)->SPI_IER = SPI_IER_RDRF;
                if (pdFALSE == xSemaphoreTake(spi->sig, portMAX_DELAY) ||
		    csel->size0 || csel->size1) {
			((Spi *) spi->mmio)->SPI_IDR = ~0;
                        spi->stats.rdrf_err = 1;
			ret = -EHW;
			goto err_exit;
		}
	} else {
		if (size0 > 0) {
			if (!trans_poll(spi, buf0, size0)) {
				ret = -EHW;
				goto err_exit;
			}
		}
                if (size1 > 0) {
			if (!trans_poll(spi, buf1, size1)) {
				ret = -EHW;
				goto err_exit;
			}
		}
	}
	if (!(((Spi *) spi->mmio)->SPI_SR & SPI_SR_TXEMPTY)) {
		spi->stats.tx_end_err = 1;
		ret = -EHW;
		goto err_exit;
	}
	spi->stats.trans += size0 + size1;
        csel->stats_trans += size0 + size1;
err_exit:
	((Spi *) spi->mmio)->SPI_CR = SPI_CR_SPIDIS;
	disable_periph_clk(spi->id);
#if SPIM_CSEL_LINE_ERR == 1
	if (!csel->csel_ext && !ret && !(((Pio *) csel->csel_cont)->PIO_PDSR & csel->csel_pin)) {
		spi->stats.csel_err = 1;
		ret = -EHW;
	}
#endif
	if (spi->mtx != NULL && !csel->csel_ext) {
		xSemaphoreGive(spi->mtx);
	}
	return (ret);
}

/**
 * trans_poll
 */
static boolean_t trans_poll(spim spi, void *buf, int size)
{
	int cnt;

	for (int i = 0; i < size; i++) {
		if (spi->act_csel->bits == SPIM_8_BIT_TRANS) {
			((Spi *) spi->mmio)->SPI_TDR = *((uint8_t *) buf + i);
		} else {
			((Spi *) spi->mmio)->SPI_TDR = *((uint16_t *) buf + i);
		}
		for (cnt = 0; cnt < HW_RESP_TMOUT; cnt++) {
			if (((Spi *) spi->mmio)->SPI_SR & SPI_SR_RDRF) {
				break;
			}
		}
		if (cnt == HW_RESP_TMOUT) {
			spi->stats.poll_err = 1;
			return (FALSE);
		}
		if (spi->act_csel->bits == SPIM_8_BIT_TRANS) {
			*((uint8_t *) buf + i) = ((Spi *) spi->mmio)->SPI_RDR;
		} else {
			*((uint16_t *) buf + i) = ((Spi *) spi->mmio)->SPI_RDR;
		}
	}
	return (TRUE);
}

/**
 * csr_reg
 */
static unsigned int csr_reg(spim_csel csel)
{
	unsigned int ui;

	ui = (~csel->mode & 1) << 1 | (csel->mode & 2) >> 1;
	ui |= csel->bits << SPI_CSR_BITS_Pos |
	      ((csel->csrise) ? SPI_CSR_CSNAAT : SPI_CSR_CSAAT);
	ui |= SPI_CSR_DLYBCT(csel->dlybct) | SPI_CSR_DLYBS(csel->dlybs) |
	      SPI_CSR_SCBR(csel->scbr);
	return (ui);
}

/**
 * pcs_fld
 */
static enum spi_pcs pcs_fld(enum spim_csel_num csn)
{
	if (csn == SPIM_CSEL0) {
		return (SPI_PCS0);
	} else if (csn == SPIM_CSEL1) {
		return (SPI_PCS1);
	} else if (csn == SPIM_CSEL2) {
		return (SPI_PCS2);
	} else if (csn == SPIM_CSEL3) {
		return (SPI_PCS3);
	} else {
		crit_err_exit(BAD_PARAMETER);
		return (0);
	}
}

/**
 * spi_hndlr
 */
static BaseType_t spi_hndlr(spim spi)
{
	unsigned int sr;
        BaseType_t tsk_wkn = pdFALSE;
        int *p_sz;
	void **p_bf;

	sr = ((Spi *) spi->mmio)->SPI_SR;
        sr &= ((Spi *) spi->mmio)->SPI_IMR;
        spi->stats.intr++;
	if (sr & SPI_SR_RDRF && spi->act_csel->dma == DMA_OFF) {
		if (spi->act_csel->bufn == 1) {
			p_sz = &spi->act_csel->size1;
			p_bf = &spi->act_csel->buf1;
		} else {
			p_sz = &spi->act_csel->size0;
			p_bf = &spi->act_csel->buf0;
		}
		if (spi->act_csel->bits == SPIM_8_BIT_TRANS) {
			*((uint8_t *) *p_bf - 1) = ((Spi *) spi->mmio)->SPI_RDR;
		} else {
			*((uint16_t *) *p_bf - 1) = ((Spi *) spi->mmio)->SPI_RDR;
		}
		if (*p_sz == 0) {
			if (spi->act_csel->bufn == 1) {
				((Spi *) spi->mmio)->SPI_IDR = SPI_IDR_RDRF;
                                xSemaphoreGiveFromISR(spi->sig, &tsk_wkn);
                                return (tsk_wkn);
			} else {
				if (spi->act_csel->size1 == 0) {
					((Spi *) spi->mmio)->SPI_IDR = SPI_IDR_RDRF;
                                        xSemaphoreGiveFromISR(spi->sig, &tsk_wkn);
                                        return (tsk_wkn);
				} else {
					spi->act_csel->bufn = 1;
					p_sz = &spi->act_csel->size1;
					p_bf = &spi->act_csel->buf1;
				}
			}
		}
		if (spi->act_csel->bits == SPIM_8_BIT_TRANS) {
			((Spi *) spi->mmio)->SPI_TDR = *((uint8_t *) *p_bf);
			*p_bf = (uint8_t *) *p_bf + 1;
		} else {
			((Spi *) spi->mmio)->SPI_TDR = *((uint16_t *) *p_bf);
			*p_bf = (uint16_t *) *p_bf + 1;
		}
		--*p_sz;
	} else if (sr & SPI_SR_RXBUFF && spi->act_csel->dma == DMA_ON) {
		((Spi *) spi->mmio)->SPI_PTCR = SPI_PTCR_RXTDIS | SPI_PTCR_TXTDIS;
		((Spi *) spi->mmio)->SPI_IDR = SPI_IDR_RXBUFF;
                xSemaphoreGiveFromISR(spi->sig, &tsk_wkn);
	} else {
		spi->stats.intr_err = 1;
                ((Spi *) spi->mmio)->SPI_IDR = ~0;
	}
	return (tsk_wkn);
}

#ifdef ID_SPI
/**
 * SPI_Handler
 */
void SPI_Handler(void)
{
	portEND_SWITCHING_ISR(spi_hndlr(smi));
}
#endif

#ifdef ID_SPI0
/**
 * SPI0_Handler
 */
void SPI0_Handler(void)
{
	portEND_SWITCHING_ISR(spi_hndlr(smi0));
}
#endif

#ifdef ID_SPI1
/**
 * SPI1_Handler
 */
void SPI1_Handler(void)
{
	portEND_SWITCHING_ISR(spi_hndlr(smi1));
}
#endif

#if TERMOUT == 1
/**
 * log_spim_stats
 */
void log_spim_stats(spim spi)
{
	UBaseType_t pr;

	pr = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
        msg(INF, "spim.c: errors=");
	if (spi->stats.tx_start_err) {
		msg(INF, "tx_start_err ");
	}
	if (spi->stats.tx_end_err) {
		msg(INF, "tx_end_err ");
	}
	if (spi->stats.mr_cfg_err) {
		msg(INF, "mr_cfg_err ");
	}
	if (spi->stats.dma_err) {
		msg(INF, "dma_err ");
	}
	if (spi->stats.rdrf_err) {
		msg(INF, "rdrf_err ");
	}
	if (spi->stats.intr_err) {
		msg(INF, "intr_err ");
	}
	if (spi->stats.poll_err) {
		msg(INF, "poll_err ");
	}
#if SPIM_CSEL_LINE_ERR == 1
	if (spi->stats.csel_err) {
		msg(INF, "csel_err ");
	}
#endif
	msg(INF, "\n");
        msg(INF, "spim.c: trans=%u\n", spi->stats.trans);
	msg(INF, "spim.c: intr=%u\n", spi->stats.intr);
	vTaskPrioritySet(NULL, pr);
}
#endif

#endif

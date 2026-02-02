/*
 * spi.c
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

#if SPIBUS == 1

#define WAIT_PDC_INTR (1000 / portTICK_PERIOD_MS)
#define HW_RESP_TMOUT 1000000

enum spi_pcs {
	SPI_PCS0,
	SPI_PCS1,
        SPI_PCS2 = 3,
        SPI_PCS3 = 7
};

#ifdef ID_SPI
static spibus smi;
#endif
#ifdef ID_SPI0
static spibus smi0;
#endif
#ifdef ID_SPI1
static spibus smi1;
#endif

static boolean_t trans_poll(spibus bus, void *buf, int size);
static unsigned int csr_reg(spi_csel csel);
static enum spi_pcs pcs_fld(enum spi_csel_num csn);
static BaseType_t spi_hndlr(spibus bus);

/**
 * init_spi
 */
void init_spi(spibus bus)
{
	NVIC_DisableIRQ(bus->id);
#if defined(ID_SPI)
	if (bus->id == ID_SPI) {
		bus->mmio = SPI;
		bus->nm = "SPI";
                smi = bus;
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
#elif defined(ID_SPI0) && !defined(ID_SPI1)
	if (bus->id == ID_SPI0) {
		bus->mmio = SPI0;
		bus->nm = "SPI0";
                smi0 = bus;
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
#elif defined(ID_SPI0) && defined(ID_SPI1)
	if (bus->id == ID_SPI0) {
		bus->mmio = SPI0;
		bus->nm = "SPI0";
                smi0 = bus;
	} else if (bus->id == ID_SPI1) {
		bus->mmio = SPI1;
		bus->nm = "SPI1";
                smi1 = bus;
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
#else
 #error "ID_SPI not defined"
#endif
	memset(&bus->stats, 0, sizeof(struct spi_stats));
	if (bus->sig == NULL) {
		if (NULL == (bus->sig = xSemaphoreCreateBinary())) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
        enable_periph_clk(bus->id);
        ((Spi *) bus->mmio)->SPI_CR = SPI_CR_SWRST;
	((Spi *) bus->mmio)->SPI_CR = SPI_CR_SPIDIS;
	((Spi *) bus->mmio)->SPI_PTCR = SPI_PTCR_RXTDIS | SPI_PTCR_TXTDIS;
	((Spi *) bus->mmio)->SPI_IDR = ~0;
	NVIC_ClearPendingIRQ(bus->id);
	((Spi *) bus->mmio)->SPI_MR = SPI_MR_DLYBCS(bus->dlybcs) | SPI_MR_MODFDIS | SPI_MR_MSTR;
	NVIC_SetPriority(bus->id, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(bus->id);
	disable_periph_clk(bus->id);
}

/**
 * spi_trans
 */
int spi_trans(spibus bus, spi_csel csel, void *buf0, int size0, void *buf1, int size1,
              boolean_t dma)
{
	int ret = 0;
	unsigned int ui, sr;

	if (size0 <= 0) {
		crit_err_exit(BAD_PARAMETER);
	}
	if (bus->mtx != NULL && !csel->csel_ext) {
		xSemaphoreTake(bus->mtx, portMAX_DELAY);
	}
#if SPI_CSEL_LINE_ERR == 1
	if (!csel->csel_ext && !(((Pio *) csel->csel_cont)->PIO_PDSR & csel->csel_pin)) {
		bus->stats.csel_err = 1;
		if (bus->mtx != NULL) {
			xSemaphoreGive(bus->mtx);
		}
		return (-EHW);
	}
#endif
	bus->act_csel = csel;
	enable_periph_clk(bus->id);
	ui = ((Spi *) bus->mmio)->SPI_MR;
	ui &= ~SPI_MR_PCS_Msk;
	if ((ui & (SPI_MR_MODFDIS | SPI_MR_MSTR)) != (SPI_MR_MODFDIS | SPI_MR_MSTR)) {
		bus->stats.mr_cfg_err = 1;
		ret = -EHW;
		goto err_exit;
	}
	ui |= SPI_MR_PCS(pcs_fld(csel->csn));
	((Spi *) bus->mmio)->SPI_MR = ui;
	if (csel->ini) {
		((Spi *) bus->mmio)->SPI_CSR[csel->csn] = csel->csr = csr_reg(csel);
		csel->ini = FALSE;
	} else {
		((Spi *) bus->mmio)->SPI_CSR[csel->csn] = csel->csr;
	}
	((Spi *) bus->mmio)->SPI_CR = SPI_CR_SPIEN;
	sr = ((Spi *) bus->mmio)->SPI_SR;
	sr &= SPI_SR_TDRE | SPI_SR_TXEMPTY;
	if (sr != (SPI_SR_TDRE | SPI_SR_TXEMPTY)) {
		bus->stats.tx_start_err = 1;
		ret = -EHW;
		goto err_exit;
	}
	if ((csel->dma = dma) == DMA_ON) {
		((Spi *) bus->mmio)->SPI_RPR = (unsigned int) buf0;
		((Spi *) bus->mmio)->SPI_RCR = size0;
		((Spi *) bus->mmio)->SPI_TPR = (unsigned int) buf0;
		((Spi *) bus->mmio)->SPI_TCR = size0;
		((Spi *) bus->mmio)->SPI_RNPR = (unsigned int) buf1;
		((Spi *) bus->mmio)->SPI_RNCR = size1;
		((Spi *) bus->mmio)->SPI_TNPR = (unsigned int) buf1;
		((Spi *) bus->mmio)->SPI_TNCR = size1;
                barrier();
                ((Spi *) bus->mmio)->SPI_IER = SPI_IER_RXBUFF;
		((Spi *) bus->mmio)->SPI_PTCR = SPI_PTCR_RXTEN | SPI_PTCR_TXTEN;
                if (pdFALSE == xSemaphoreTake(bus->sig, WAIT_PDC_INTR)) {
			((Spi *) bus->mmio)->SPI_IDR = ~0;
                        xSemaphoreTake(bus->sig, 0);
                        bus->stats.dma_err = 1;
			ret = -EDMA;
			goto err_exit;
		}
		if (((Spi *) bus->mmio)->SPI_RPR != ((Spi *) bus->mmio)->SPI_TPR ||
		    ((Spi *) bus->mmio)->SPI_RNPR != ((Spi *) bus->mmio)->SPI_TNPR ||
		    ((Spi *) bus->mmio)->SPI_RCR || ((Spi *) bus->mmio)->SPI_TCR ||
		    ((Spi *) bus->mmio)->SPI_RNCR || ((Spi *) bus->mmio)->SPI_TNCR) {
			bus->stats.dma_err = 1;
			ret = -EDMA;
			goto err_exit;
		}
	} else if (csel->no_dma_intr == TRUE) {
		csel->buf0 = buf0;
		csel->size0 = size0;
		csel->buf1 = buf1;
                csel->size1 = size1;
		csel->bufn = 0;
		if (csel->bits == SPI_8_BIT_TRANS) {
			((Spi *) bus->mmio)->SPI_TDR = *((uint8_t *) csel->buf0);
			csel->buf0 = (uint8_t *) csel->buf0 + 1;
		} else {
			((Spi *) bus->mmio)->SPI_TDR = *((uint16_t *) csel->buf0);
			csel->buf0 = (uint16_t *) csel->buf0 + 1;
		}
		csel->size0--;
                barrier();
                ((Spi *) bus->mmio)->SPI_IER = SPI_IER_RDRF;
                if (pdFALSE == xSemaphoreTake(bus->sig, portMAX_DELAY) ||
		    csel->size0 || csel->size1) {
			((Spi *) bus->mmio)->SPI_IDR = ~0;
                        bus->stats.rdrf_err = 1;
			ret = -EHW;
			goto err_exit;
		}
	} else {
		if (!trans_poll(bus, buf0, size0)) {
			ret = -EHW;
			goto err_exit;
		}
                if (size1 > 0) {
			if (!trans_poll(bus, buf1, size1)) {
				ret = -EHW;
				goto err_exit;
			}
		}
	}
	int cnt;
	for (cnt = 0; cnt < HW_RESP_TMOUT; cnt++) {
		if (((Spi *) bus->mmio)->SPI_SR & SPI_SR_TXEMPTY) {
			break;
		}
	}
	if (cnt == HW_RESP_TMOUT) {
		bus->stats.tx_end_err = 1;
		ret = -EHW;
		goto err_exit;
	}
	bus->stats.trans += size0 + size1;
        csel->stats_trans += size0 + size1;
err_exit:
	((Spi *) bus->mmio)->SPI_CR = SPI_CR_SPIDIS;
	disable_periph_clk(bus->id);
#if SPI_CSEL_LINE_ERR == 1
	if (!csel->csel_ext && !ret && !(((Pio *) csel->csel_cont)->PIO_PDSR & csel->csel_pin)) {
		bus->stats.csel_err = 1;
		ret = -EHW;
	}
#endif
	if (bus->mtx != NULL && !csel->csel_ext) {
		xSemaphoreGive(bus->mtx);
	}
	return (ret);
}

/**
 * trans_poll
 */
static boolean_t trans_poll(spibus bus, void *buf, int size)
{
	int cnt;

	for (int i = 0; i < size; i++) {
		if (bus->act_csel->bits == SPI_8_BIT_TRANS) {
			((Spi *) bus->mmio)->SPI_TDR = *((uint8_t *) buf + i);
		} else {
			((Spi *) bus->mmio)->SPI_TDR = *((uint16_t *) buf + i);
		}
		for (cnt = 0; cnt < HW_RESP_TMOUT; cnt++) {
			if (((Spi *) bus->mmio)->SPI_SR & SPI_SR_RDRF) {
				break;
			}
		}
		if (cnt == HW_RESP_TMOUT) {
			bus->stats.poll_err = 1;
			return (FALSE);
		}
		if (bus->act_csel->bits == SPI_8_BIT_TRANS) {
			*((uint8_t *) buf + i) = ((Spi *) bus->mmio)->SPI_RDR;
		} else {
			*((uint16_t *) buf + i) = ((Spi *) bus->mmio)->SPI_RDR;
		}
	}
	return (TRUE);
}

/**
 * csr_reg
 */
static unsigned int csr_reg(spi_csel csel)
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
static enum spi_pcs pcs_fld(enum spi_csel_num csn)
{
	if (csn == SPI_CSEL0) {
		return (SPI_PCS0);
	} else if (csn == SPI_CSEL1) {
		return (SPI_PCS1);
	} else if (csn == SPI_CSEL2) {
		return (SPI_PCS2);
	} else if (csn == SPI_CSEL3) {
		return (SPI_PCS3);
	} else {
		crit_err_exit(BAD_PARAMETER);
		return (0);
	}
}

/**
 * spi_hndlr
 */
static BaseType_t spi_hndlr(spibus bus)
{
	unsigned int sr;
        BaseType_t tsk_wkn = pdFALSE;
        int *p_sz;
	void **p_bf;

	sr = ((Spi *) bus->mmio)->SPI_SR;
        sr &= ((Spi *) bus->mmio)->SPI_IMR;
        bus->stats.intr++;
	if (sr & SPI_SR_RDRF && bus->act_csel->dma == DMA_OFF) {
		if (bus->act_csel->bufn == 1) {
			p_sz = &bus->act_csel->size1;
			p_bf = &bus->act_csel->buf1;
		} else {
			p_sz = &bus->act_csel->size0;
			p_bf = &bus->act_csel->buf0;
		}
		if (bus->act_csel->bits == SPI_8_BIT_TRANS) {
			*((uint8_t *) *p_bf - 1) = ((Spi *) bus->mmio)->SPI_RDR;
		} else {
			*((uint16_t *) *p_bf - 1) = ((Spi *) bus->mmio)->SPI_RDR;
		}
		if (*p_sz == 0) {
			if (bus->act_csel->bufn == 1) {
				((Spi *) bus->mmio)->SPI_IDR = SPI_IDR_RDRF;
                                xSemaphoreGiveFromISR(bus->sig, &tsk_wkn);
                                return (tsk_wkn);
			} else {
				if (bus->act_csel->size1 == 0) {
					((Spi *) bus->mmio)->SPI_IDR = SPI_IDR_RDRF;
                                        xSemaphoreGiveFromISR(bus->sig, &tsk_wkn);
                                        return (tsk_wkn);
				} else {
					bus->act_csel->bufn = 1;
					p_sz = &bus->act_csel->size1;
					p_bf = &bus->act_csel->buf1;
				}
			}
		}
		if (bus->act_csel->bits == SPI_8_BIT_TRANS) {
			((Spi *) bus->mmio)->SPI_TDR = *((uint8_t *) *p_bf);
			*p_bf = (uint8_t *) *p_bf + 1;
		} else {
			((Spi *) bus->mmio)->SPI_TDR = *((uint16_t *) *p_bf);
			*p_bf = (uint16_t *) *p_bf + 1;
		}
		--*p_sz;
	} else if (sr & SPI_SR_RXBUFF && bus->act_csel->dma == DMA_ON) {
		((Spi *) bus->mmio)->SPI_PTCR = SPI_PTCR_RXTDIS | SPI_PTCR_TXTDIS;
		((Spi *) bus->mmio)->SPI_IDR = SPI_IDR_RXBUFF;
                xSemaphoreGiveFromISR(bus->sig, &tsk_wkn);
	} else {
		bus->stats.intr_err = 1;
                ((Spi *) bus->mmio)->SPI_IDR = ~0;
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

/**
 * get_spi_by_per_id
 */
spibus get_spi_by_per_id(int per_id)
{
#ifdef ID_SPI
	if (per_id == ID_SPI && smi) {
		return (smi);
	}
#endif
#ifdef ID_SPI0
	if (per_id == ID_SPI0 && smi0) {
		return (smi0);
	}
#endif
#ifdef ID_SPI1
	if (per_id == ID_SPI1 && smi1) {
		return (smi1);
	}
#endif
	crit_err_exit(BAD_PARAMETER);
	return (NULL);
}

/**
 * get_spi_by_dev_id
 */
spibus get_spi_by_dev_id(int dev_id)
{
#ifdef ID_SPI
	if (dev_id == 0 && smi) {
		return (smi);
	}
	crit_err_exit(BAD_PARAMETER);
	return (NULL);
#else
	switch (dev_id) {
#ifdef ID_SPI0
	case 0 :
		if (smi0) {
			return (smi0);
		}
		break;
#endif
#ifdef ID_SPI1
	case 1 :
		if (smi1) {
			return (smi1);
		}
		break;
#endif
	default:
		break;
	}
#endif
	crit_err_exit(BAD_PARAMETER);
	return (NULL);
}

#if TERMOUT == 1
/**
 * log_spi_stats
 */
void log_spi_stats(spibus bus)
{
	UBaseType_t pr;

	pr = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
        msg(INF, "spi.c: bus=%s\n", bus->nm);
        msg(INF, "spi.c: errors=");
	if (bus->stats.tx_start_err) {
		msg(INF, "tx_start_err ");
	}
	if (bus->stats.tx_end_err) {
		msg(INF, "tx_end_err ");
	}
	if (bus->stats.mr_cfg_err) {
		msg(INF, "mr_cfg_err ");
	}
	if (bus->stats.dma_err) {
		msg(INF, "dma_err ");
	}
	if (bus->stats.rdrf_err) {
		msg(INF, "rdrf_err ");
	}
	if (bus->stats.intr_err) {
		msg(INF, "intr_err ");
	}
	if (bus->stats.poll_err) {
		msg(INF, "poll_err ");
	}
#if SPI_CSEL_LINE_ERR == 1
	if (bus->stats.csel_err) {
		msg(INF, "csel_err ");
	}
#endif
	msg(INF, "\n");
        msg(INF, "spi.c: trans=%u\n", bus->stats.trans);
	msg(INF, "spi.c: intr=%u\n", bus->stats.intr);
	vTaskPrioritySet(NULL, pr);
}
#endif

#endif

/*
 * usart.c
 *
 * Copyright (c) 2023 Jan Rusnak <jan@rusnak.sk>
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
#include "fmalloc.h"
#include "hwerr.h"
#include "pmc.h"
#include "usart.h"
#include <string.h>

#define WAIT_PDC_INTR (1000 / portTICK_PERIOD_MS)

#if USART_YIT == 1
enum {
	YIT_WAIT_CA,
	YIT_WAIT_SZ_LSB,
	YIT_WAIT_SZ_MSB,
	YIT_RCV_DATA,
	YIT_RCV_SUM
};
#endif

#if USART_RX_CHAR == 1 || USART_HDLC == 1 || USART_ADR_HDLC == 1 || USART_ADR_CHAR == 1 ||\
    USART_YIT == 1
static usart u0;
#ifdef ID_USART1
static usart u1;
#endif
#ifdef ID_USART2
static usart u2;
#endif
#endif

#if USART_RX_CHAR == 1 || USART_ADR_CHAR == 1
static BaseType_t rx_char_hndlr(usart dev);
#endif
#if USART_HDLC == 1
static BaseType_t hdlc_hndlr(usart dev);
#endif
#if USART_ADR_HDLC == 1
static BaseType_t adr_hdlc_hndlr(usart dev);
#endif
#if USART_YIT == 1
static BaseType_t yit_hndlr(usart dev);
#endif

#if USART_RX_CHAR == 1 || USART_HDLC == 1 || USART_ADR_HDLC == 1 || USART_ADR_CHAR == 1 ||\
    USART_YIT == 1
/**
 * init_usart
 */
void init_usart(usart dev, enum usart_mode m)
{
	NVIC_DisableIRQ(dev->id);
	if (dev->id == ID_USART0) {
		u0 = dev;
                u0->mmio = USART0;
		u0->dma = TRUE;
#ifdef ID_USART1
	} else if (dev->id == ID_USART1) {
		u1 = dev;
                u1->mmio = USART1;
#ifdef PDC_USART1
		u1->dma = TRUE;
#else
		u1->dma = FALSE;
#endif
#endif
#ifdef ID_USART2
	} else if (dev->id == ID_USART2) {
		u2 = dev;
                u2->mmio = USART2;
#ifdef PDC_USART2
		u2->dma = TRUE;
#else
		u2->dma = FALSE;
#endif
#endif
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
	switch (dev->mode = m) {
#if USART_RX_CHAR == 1 || USART_ADR_CHAR == 1
	case USART_RX_CHAR_MODE  :
	case USART_ADR_CHAR_MODE :
		if (dev->rx_que == NULL) {
			dev->rx_que = xQueueCreate(dev->rx_que_sz, sizeof(uint16_t));
			if (dev->rx_que == NULL) {
				crit_err_exit(MALLOC_ERROR);
			}
		} else {
			crit_err_exit(UNEXP_PROG_STATE);
		}
		dev->hndlr = rx_char_hndlr;
		break;
#endif
#if USART_HDLC == 1 || USART_ADR_HDLC == 1
	case USART_HDLC_MODE     :
	case USART_ADR_HDLC_MODE :
		if (NULL == (dev->hdlc_mesg.pld = pvPortMalloc(dev->hdlc_bf_sz))) {
			crit_err_exit(MALLOC_ERROR);
		}
		if (dev->mode == USART_HDLC_MODE) {
#if USART_HDLC == 1
			dev->hndlr = hdlc_hndlr;
#endif
		} else {
#if USART_ADR_HDLC == 1
			dev->hndlr = adr_hdlc_hndlr;
#endif
		}
		break;
#endif
#if USART_YIT == 1
	case USART_YIT_MODE      :
		if (dev->rx_que == NULL) {
			dev->rx_que = xQueueCreate(USART_YIT_CMD_ARY_SIZE, sizeof(struct yit_cmd *));
			if (dev->rx_que == NULL) {
				crit_err_exit(MALLOC_ERROR);
			}
		} else {
			crit_err_exit(UNEXP_PROG_STATE);
		}
                dev->hndlr = yit_hndlr;
                dev->rcv_st = YIT_WAIT_CA;
		break;
#endif
	default                  :
		crit_err_exit(BAD_PARAMETER);
		break;
	}
	if (dev->sig == NULL) {
		if (NULL == (dev->sig = xSemaphoreCreateBinary())) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
	if (dev->conf_pins) {
		dev->conf_pins(ON);
	}
	enable_periph_clk(dev->id);
        dev->mmio->US_IDR = ~0U;
	dev->mmio->US_CR = US_CR_RSTSTA | US_CR_RSTTX | US_CR_RSTRX;
	NVIC_ClearPendingIRQ(dev->id);
	dev->mmio->US_MR = dev->mr;
        dev->mmio->US_BRGR = F_MCK / 16 / dev->bdr;
	dev->mmio->US_RTOR = 0;
	dev->mmio->US_TTGR = 0;
        dev->mmio->US_PTCR = US_PTCR_TXTDIS;
        dev->mmio->US_TCR = 0;
	dev->mmio->US_TNCR = 0;
	dev->mmio->US_PTCR = US_PTCR_RXTDIS;
        dev->mmio->US_RCR = 0;
	dev->mmio->US_RNCR = 0;
	if (dev->mr & US_MR_USART_MODE_RS485) {
		dev->mmio->US_CR = US_CR_TXEN;
		dev->mmio->US_CR = US_CR_TXDIS;
	}
        NVIC_SetPriority(dev->id, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(dev->id);
}
#endif

#if USART_RX_CHAR == 1
/**
 * enable_usart
 */
void enable_usart(void *dev)
{
	uint16_t d;

	while (pdTRUE == xQueueReceive(((usart) dev)->rx_que, &d, 0));
	if (((usart) dev)->conf_pins) {
		((usart) dev)->conf_pins(ON);
	}
	enable_periph_clk(((usart) dev)->id);
	((usart) dev)->mmio->US_CR = US_CR_RSTSTA | US_CR_RSTTX | US_CR_RSTRX;
	NVIC_ClearPendingIRQ(((usart) dev)->id);
	NVIC_EnableIRQ(((usart) dev)->id);
}
#endif

#if USART_RX_CHAR == 1
/**
 * disable_usart
 */
void disable_usart(void *dev)
{
	NVIC_DisableIRQ(((usart) dev)->id);
	((usart) dev)->mmio->US_IDR = ~0U;
	((usart) dev)->mmio->US_CR = US_CR_RSTTX | US_CR_RSTRX;
	((usart) dev)->mmio->US_CR = US_CR_TXDIS | US_CR_RXDIS;
	if (((usart) dev)->conf_pins) {
		((usart) dev)->conf_pins(OFF);
	}
	disable_periph_clk(((usart) dev)->id);
}
#endif

#if USART_RX_CHAR == 1 || USART_HDLC == 1 || USART_YIT == 1
/**
 * usart_tx_buff
 */
int usart_tx_buff(void *dev, void *p_buf, int size)
{
        if (size < 1) {
                return (0);
        }
	if (((usart) dev)->dma) {
		((usart) dev)->mmio->US_TCR = size;
		((usart) dev)->mmio->US_TPR = (unsigned int) p_buf;
                ((usart) dev)->mmio->US_CR = US_CR_TXEN;
		((usart) dev)->mmio->US_IER = US_IER_ENDTX;
		((usart) dev)->mmio->US_PTCR = US_PTCR_TXTEN;
		if (pdFALSE == xSemaphoreTake(((usart) dev)->sig, WAIT_PDC_INTR) ||
		    ((usart) dev)->mmio->US_TCR != 0) {
			((usart) dev)->mmio->US_IDR = US_IDR_ENDTX;
			((usart) dev)->mmio->US_PTCR = US_PTCR_TXTDIS;
			((usart) dev)->mmio->US_TCR = 0;
			((usart) dev)->mmio->US_CR = US_CR_RSTTX;
			if (((usart) dev)->mr & US_MR_USART_MODE_RS485) {
				((usart) dev)->mmio->US_CR = US_CR_TXEN;
			}
                        ((usart) dev)->mmio->US_CR = US_CR_TXDIS;
			xSemaphoreTake(((usart) dev)->sig, 0);
			return (-EDMA);
		}
		while (!(((usart) dev)->mmio->US_CSR & US_CSR_TXEMPTY)) {
			;
		}
		((usart) dev)->mmio->US_PTCR = US_PTCR_TXTDIS;
	} else {
		((usart) dev)->mmio->US_CR = US_CR_TXEN;
		for (int i = 0; i < size; i++) {
			while (!(((usart) dev)->mmio->US_CSR & US_CSR_TXRDY)) {
				;
			}
			if (((usart) dev)->mmio->US_MR & US_MR_MODE9) {
				((usart) dev)->mmio->US_THR = *((uint16_t *) p_buf + i);
			} else {
				((usart) dev)->mmio->US_THR = *((uint8_t *) p_buf + i);
			}
		}
		while (!(((usart) dev)->mmio->US_CSR & US_CSR_TXEMPTY)) {
			;
		}
	}
        ((usart) dev)->mmio->US_CR = US_CR_TXDIS;
        return (0);
}
#endif

#if USART_RX_CHAR == 1
/**
 * usart_rx_char
 */
int usart_rx_char(void *dev, void *p_char, TickType_t tmo)
{
	uint16_t d;

	if (!(((usart) dev)->mmio->US_IMR & US_IMR_RXRDY)) {
		((usart) dev)->mmio->US_IER = US_IER_RXRDY;
		((usart) dev)->mmio->US_CR = US_CR_RXEN;
	}
	if (pdFALSE == xQueueReceive(((usart) dev)->rx_que, &d, tmo)) {
		if (((usart) dev)->mmio->US_MR & US_MR_MODE9) {
			*((uint16_t *) p_char) = '\0';
		} else {
			*((uint8_t *) p_char) = '\0';
		}
		return (-ETMO);
	}
	if (((usart) dev)->mmio->US_MR & US_MR_MODE9) {
		*((uint16_t *) p_char) = d & 0x01FF;
	} else {
		*((uint8_t *) p_char) = d;
	}
	if (d & 0x1000) {
		return (-EINTR);
	}
	if (d >> 8 & (US_CSR_OVRE | US_CSR_FRAME | US_CSR_PARE)) {
		return (-ERCV);
	} else {
		return (0);
	}
}
#endif

#if USART_RX_CHAR == 1
/**
 * usart_intr_rx
 */
boolean_t usart_intr_rx(void *dev)
{
	uint16_t d = 0x1000;

	if (pdTRUE == xQueueSend(((usart) dev)->rx_que, &d, 0)) {
		return (TRUE);
	} else {
		return (FALSE);
	}
}
#endif

#if USART_RX_CHAR == 1 || USART_ADR_CHAR == 1
/**
 * rx_char_hndlr
 */
static BaseType_t rx_char_hndlr(usart dev)
{
        BaseType_t tsk_wkn = pdFALSE;
	unsigned int sr;
        uint16_t d;

	sr = dev->mmio->US_CSR;
	if (sr & US_CSR_RXRDY && dev->mmio->US_IMR & US_IMR_RXRDY) {
		d = dev->mmio->US_RHR & 0x01FF;
		if (sr & (US_CSR_OVRE | US_CSR_FRAME | US_CSR_PARE)) {
			d |= (sr & 0xE0) << 8;
			dev->mmio->US_CR = US_CR_RSTSTA;
		}
		xQueueSendFromISR(dev->rx_que, &d, &tsk_wkn);
	} else if (sr & US_CSR_ENDTX && dev->mmio->US_IMR & US_IMR_ENDTX) {
        	dev->mmio->US_IDR = US_IDR_ENDTX;
                xSemaphoreGiveFromISR(dev->sig, &tsk_wkn);
	}
	return (tsk_wkn);
}
#endif

#if USART_HDLC == 1 || USART_ADR_HDLC == 1
enum {
	HDLC_RCV_WAIT_ADDR,
        HDLC_RCV_FLAG_1,
        HDLC_RCV_DATA,
        HDLC_RCV_ESC
};
#endif

#if USART_HDLC == 1
/**
 * usart_tx_hdlc_mesg
 */
int usart_tx_hdlc_mesg(usart dev, uint8_t *pld, int size)
{
	int sz = 1;

	if (size < 1) {
		return (0);
	}
	*dev->hdlc_mesg.pld = dev->HDLC_FLAG;
        for (int i = 0; i < size; i++) {
		if (*(pld + i) == dev->HDLC_FLAG || *(pld + i) == dev->HDLC_ESC) {
			if (sz + 2 < dev->hdlc_bf_sz) {
				*(dev->hdlc_mesg.pld + sz++) = dev->HDLC_ESC;
				*(dev->hdlc_mesg.pld + sz++) = *(pld + i) ^ dev->HDLC_MOD;
			} else {
				return (-EBFOV);
			}
		} else {
			if (sz + 1 < dev->hdlc_bf_sz) {
				*(dev->hdlc_mesg.pld + sz++) = *(pld + i);
			} else {
				return (-EBFOV);
			}
		}
	}
	*(dev->hdlc_mesg.pld + sz++) = dev->HDLC_FLAG;
	return (usart_tx_buff(dev, dev->hdlc_mesg.pld, sz));
}

/**
 * usart_rx_hdlc_mesg
 */
struct hdlc_mesg *usart_rx_hdlc_mesg(usart dev, TickType_t tmo)
{
	dev->rcv_st = HDLC_RCV_FLAG_1;
        dev->mmio->US_CR = US_CR_RSTRX;
        barrier();
	dev->mmio->US_IER = US_IER_RXRDY;
	dev->mmio->US_CR = US_CR_RXEN;
	if (pdFALSE == xSemaphoreTake(dev->sig, tmo)) {
		dev->mmio->US_IDR = US_IDR_RXRDY;
                dev->mmio->US_CR = US_CR_RXDIS;
                xSemaphoreTake(dev->sig, 0);
                return (NULL);
	} else {
		return (&dev->hdlc_mesg);
	}
}

/**
 * hdlc_hndlr
 */
static BaseType_t hdlc_hndlr(usart dev)
{
	BaseType_t tsk_wkn = pdFALSE;
	unsigned int sr;
        uint8_t d;

	sr = dev->mmio->US_CSR;
	if (sr & US_CSR_RXRDY && dev->mmio->US_IMR & US_IMR_RXRDY) {
		d = dev->mmio->US_RHR;
		if (sr & US_CSR_OVRE) {
			dev->mmio->US_CR = US_CR_RSTSTA;
                        dev->hdlc_stats.ovr_lerr++;
                        dev->rcv_st = HDLC_RCV_FLAG_1;
			return (pdFALSE);
		} else if (sr & US_CSR_FRAME) {
			dev->mmio->US_CR = US_CR_RSTSTA;
			dev->hdlc_stats.fra_lerr++;
                        dev->rcv_st = HDLC_RCV_FLAG_1;
                        return (pdFALSE);
		} else if (sr & US_CSR_PARE) {
			dev->mmio->US_CR = US_CR_RSTSTA;
			dev->hdlc_stats.par_lerr++;
                        dev->rcv_st = HDLC_RCV_FLAG_1;
                        return (pdFALSE);
		}
                switch (dev->rcv_st) {
		case HDLC_RCV_FLAG_1    :
			if (d == dev->HDLC_FLAG) {
				dev->rcv_st = HDLC_RCV_DATA;
                                dev->hdlc_mesg.sz = 0;
			} else {
				dev->hdlc_stats.no_f1_perr++;
			}
			break;
		case HDLC_RCV_DATA      :
			if (d == dev->HDLC_FLAG) {
				if (dev->hdlc_mesg.sz != 0) {
					dev->mmio->US_IDR = US_IDR_RXRDY;
					dev->mmio->US_CR = US_CR_RXDIS;
					xSemaphoreGiveFromISR(dev->sig, &tsk_wkn);
				} else {
					dev->hdlc_stats.syn_f1_perr++;
				}
			} else if (d == dev->HDLC_ESC) {
				dev->rcv_st = HDLC_RCV_ESC;
			} else {
				if (dev->hdlc_mesg.sz < dev->hdlc_bf_sz) {
					*(dev->hdlc_mesg.pld + dev->hdlc_mesg.sz++) = d;
				} else {
					dev->hdlc_stats.bf_ov_perr++;
                                        dev->rcv_st = HDLC_RCV_FLAG_1;
				}
			}
			break;
		case HDLC_RCV_ESC       :
			if (dev->hdlc_mesg.sz < dev->hdlc_bf_sz) {
				uint8_t n = d ^ dev->HDLC_MOD;
				if (n == dev->HDLC_FLAG || n == dev->HDLC_ESC) {
					*(dev->hdlc_mesg.pld + dev->hdlc_mesg.sz++) = n;
					dev->rcv_st = HDLC_RCV_DATA;
				} else {
					dev->hdlc_stats.es_sq_perr++;
					dev->rcv_st = HDLC_RCV_FLAG_1;
				}
			} else {
				dev->hdlc_stats.bf_ov_perr++;
				dev->rcv_st = HDLC_RCV_FLAG_1;
			}
			break;
		}
	} else if (sr & US_CSR_ENDTX && dev->mmio->US_IMR & US_IMR_ENDTX) {
        	dev->mmio->US_IDR = US_IDR_ENDTX;
                xSemaphoreGiveFromISR(dev->sig, &tsk_wkn);
	}
	return (tsk_wkn);
}
#endif

#if USART_ADR_HDLC == 1
/**
 * usart_tx_adr_hdlc_mesg
 */
int usart_tx_adr_hdlc_mesg(usart dev, uint8_t *pld, int size, uint8_t adr)
{
        int sz = 2;

	if (size < 0) {
		return (0);
	}
	*dev->hdlc_mesg.pld = adr;
	*(dev->hdlc_mesg.pld + 1) = dev->HDLC_FLAG;
	for (int i = 0; i < size; i++) {
		if (*(pld + i) == dev->HDLC_ESC || *(pld + i) == dev->HDLC_FLAG) {
			if (sz + 2 < dev->hdlc_bf_sz) {
				*(dev->hdlc_mesg.pld + sz++) = dev->HDLC_ESC;
#if USART_ADR_HDLC_OFFS_ESC_SEQ == 1
                                *(dev->hdlc_mesg.pld + sz++) = *(pld + i) - dev->HDLC_MOD;
#else
				*(dev->hdlc_mesg.pld + sz++) = *(pld + i) ^ dev->HDLC_MOD;
#endif
			} else {
				return (-EBFOV);
			}
		} else {
			if (sz + 1 < dev->hdlc_bf_sz) {
				*(dev->hdlc_mesg.pld + sz++) = *(pld + i);
			} else {
				return (-EBFOV);
			}
		}
	}
	*(dev->hdlc_mesg.pld + sz++) = dev->HDLC_FLAG;
	if (dev->dma) {
		dev->mmio->US_TCR = sz;
		dev->mmio->US_TPR = (unsigned int) dev->hdlc_mesg.pld;
                dev->mmio->US_CR = US_CR_TXEN;
		dev->mmio->US_CR = US_CR_SENDA;
		dev->mmio->US_IER = US_IER_ENDTX;
		dev->mmio->US_PTCR = US_PTCR_TXTEN;
		if (pdFALSE == xSemaphoreTake(dev->sig, WAIT_PDC_INTR) ||
		    dev->mmio->US_TCR != 0) {
			dev->mmio->US_IDR = US_IDR_ENDTX;
			dev->mmio->US_PTCR = US_PTCR_TXTDIS;
			dev->mmio->US_TCR = 0;
			dev->mmio->US_CR = US_CR_RSTTX;
                        if (dev->mr & US_MR_USART_MODE_RS485) {
				dev->mmio->US_CR = US_CR_TXEN;
			}
			dev->mmio->US_CR = US_CR_TXDIS;
			xSemaphoreTake(dev->sig, 0);
			return (-EDMA);
		}
		while (!(dev->mmio->US_CSR & US_CSR_TXEMPTY)) {
			;
		}
		dev->mmio->US_PTCR = US_PTCR_TXTDIS;
	} else {
		dev->mmio->US_CR = US_CR_TXEN;
		dev->mmio->US_CR = US_CR_SENDA;
		for (int i = 0; i < sz; i++) {
			while (!(dev->mmio->US_CSR & US_CSR_TXRDY)) {
				;
			}
			dev->mmio->US_THR = *(dev->hdlc_mesg.pld + i);
		}
		while (!(dev->mmio->US_CSR & US_CSR_TXEMPTY)) {
			;
		}
	}
        dev->mmio->US_CR = US_CR_TXDIS;
        return (0);
}

/**
 * usart_rx_adr_hdlc_mesg
 */
struct hdlc_mesg *usart_rx_adr_hdlc_mesg(usart dev, TickType_t tmo)
{
#if USART_ADR_HDLC_EXT_STATS == 1
	memset(dev->hdlc_mesg.pld, 0xCC, dev->hdlc_bf_sz);
#endif
	dev->rcv_st = HDLC_RCV_WAIT_ADDR;
        dev->mmio->US_CR = US_CR_RSTRX;
        barrier();
	dev->mmio->US_IER = US_IER_RXRDY;
	dev->mmio->US_CR = US_CR_RXEN;
	if (pdFALSE == xSemaphoreTake(dev->sig, tmo)) {
		dev->mmio->US_IDR = US_IDR_RXRDY;
                dev->mmio->US_CR = US_CR_RXDIS;
                xSemaphoreTake(dev->sig, 0);
                return (NULL);
	} else {
		return (&dev->hdlc_mesg);
	}
}

/**
 * adr_hdlc_hndlr
 */
static BaseType_t adr_hdlc_hndlr(usart dev)
{
	BaseType_t tsk_wkn = pdFALSE;
	unsigned int sr;
        uint8_t d;

	sr = dev->mmio->US_CSR;
	if (sr & US_CSR_RXRDY && dev->mmio->US_IMR & US_IMR_RXRDY) {
#if USART_ADR_HDLC_EXT_STATS == 1
		dev->adr_hdlc_ext_stats.rx_byte_cnt++;
#endif
		d = dev->mmio->US_RHR;
                if (sr & US_CSR_OVRE) {
			dev->mmio->US_CR = US_CR_RSTSTA;
                        dev->hdlc_stats.ovr_lerr++;
                        dev->rcv_st = HDLC_RCV_WAIT_ADDR;
                        return (pdFALSE);
		} else if (sr & US_CSR_FRAME) {
			dev->mmio->US_CR = US_CR_RSTSTA;
			dev->hdlc_stats.fra_lerr++;
#if USART_ADR_HDLC_EXT_STATS == 1
			dev->adr_hdlc_ext_stats.fra_lerr_hdlc[dev->rcv_st]++;
#endif
                        dev->rcv_st = HDLC_RCV_WAIT_ADDR;
                        return (pdFALSE);
		} else if (sr & US_CSR_PARE) {
			dev->mmio->US_CR = US_CR_RSTSTA;
			if (dev->rcv_st == HDLC_RCV_WAIT_ADDR) {
				if (d > USART_ADR_HDLC_MAX_ADR && d != dev->bcst_addr) {
#if USART_ADR_HDLC_EXT_STATS == 1
					dev->adr_hdlc_ext_stats.max_adr_ovr_perr++;
#endif
					return (pdFALSE);
				}
				if (d == dev->addr || d == dev->bcst_addr || dev->addr > 255) {
					dev->hdlc_mesg.adr = d;
					dev->hdlc_mesg.sz = 0;
					dev->rcv_st = HDLC_RCV_FLAG_1;
				}
			} else {
#if USART_ADR_HDLC_EXT_STATS == 1
				if (!dev->adr_hdlc_ext_stats.was_perr) {
					dev->adr_hdlc_ext_stats.was_perr = TRUE;
					dev->adr_hdlc_ext_stats.perr_adr = d;
					dev->adr_hdlc_ext_stats.perr_sz = dev->hdlc_mesg.sz;
                                        dev->adr_hdlc_ext_stats.perr_dump[0] = dev->hdlc_mesg.adr;
					for (int i = 0; i < USART_ADR_HDLC_PERR_DUMP_SIZE - 1; i++) {
						dev->adr_hdlc_ext_stats.perr_dump[i + 1] = *(dev->hdlc_mesg.pld + i);
					}
				}
				dev->adr_hdlc_ext_stats.unxp_adr_perr++;
#endif
                                dev->rcv_st = HDLC_RCV_WAIT_ADDR;
			}
			return (pdFALSE);
		}
		switch (dev->rcv_st) {
		case HDLC_RCV_WAIT_ADDR :
			break;
		case HDLC_RCV_FLAG_1    :
			if (d == dev->HDLC_FLAG) {
				dev->rcv_st = HDLC_RCV_DATA;
			} else {
				dev->hdlc_stats.no_f1_perr++;
				dev->rcv_st = HDLC_RCV_WAIT_ADDR;
			}
			break;
		case HDLC_RCV_DATA      :
			if (d == dev->HDLC_FLAG) {
				dev->mmio->US_IDR = US_IDR_RXRDY;
				dev->mmio->US_CR = US_CR_RXDIS;
				xSemaphoreGiveFromISR(dev->sig, &tsk_wkn);
			} else if (d == dev->HDLC_ESC) {
				dev->rcv_st = HDLC_RCV_ESC;
			} else {
				if (dev->hdlc_mesg.sz < dev->hdlc_bf_sz) {
					*(dev->hdlc_mesg.pld + dev->hdlc_mesg.sz++) = d;
				} else {
					dev->hdlc_stats.bf_ov_perr++;
                                        dev->rcv_st = HDLC_RCV_WAIT_ADDR;
				}
			}
			break;
		case HDLC_RCV_ESC       :
			if (dev->hdlc_mesg.sz < dev->hdlc_bf_sz) {
#if USART_ADR_HDLC_OFFS_ESC_SEQ == 1
				uint8_t n = d + dev->HDLC_MOD;
#else
				uint8_t n = d ^ dev->HDLC_MOD;
#endif
				if (n == dev->HDLC_FLAG || n == dev->HDLC_ESC) {
					*(dev->hdlc_mesg.pld + dev->hdlc_mesg.sz++) = n;
                                        dev->rcv_st = HDLC_RCV_DATA;
				} else {
					dev->hdlc_stats.es_sq_perr++;
					dev->rcv_st = HDLC_RCV_WAIT_ADDR;
				}
			} else {
				dev->hdlc_stats.bf_ov_perr++;
				dev->rcv_st = HDLC_RCV_WAIT_ADDR;
			}
			break;
		}
	} else if (sr & US_CSR_ENDTX && dev->mmio->US_IMR & US_IMR_ENDTX) {
        	dev->mmio->US_IDR = US_IDR_ENDTX;
                xSemaphoreGiveFromISR(dev->sig, &tsk_wkn);
	}
	return (tsk_wkn);
}
#endif

#if USART_ADR_CHAR == 1
/**
 * usart_tx_adr_buff
 */
int usart_tx_adr_buff(usart dev, void *p_buf, int size)
{
        if (size < 1) {
                return (0);
        }
        if (dev->dma) {
		dev->mmio->US_TCR = size;
		dev->mmio->US_TPR = (unsigned int) p_buf;
                dev->mmio->US_CR = US_CR_TXEN;
		dev->mmio->US_CR = US_CR_SENDA;
		dev->mmio->US_IER = US_IER_ENDTX;
		dev->mmio->US_PTCR = US_PTCR_TXTEN;
		if (pdFALSE == xSemaphoreTake(dev->sig, WAIT_PDC_INTR) ||
		    dev->mmio->US_TCR != 0) {
			dev->mmio->US_IDR = US_IDR_ENDTX;
			dev->mmio->US_PTCR = US_PTCR_TXTDIS;
			dev->mmio->US_TCR = 0;
			dev->mmio->US_CR = US_CR_RSTTX;
			if (dev->mr & US_MR_USART_MODE_RS485) {
				dev->mmio->US_CR = US_CR_TXEN;
			}
			dev->mmio->US_CR = US_CR_TXDIS;
			xSemaphoreTake(dev->sig, 0);
			return (-EDMA);
		}
		while (!(dev->mmio->US_CSR & US_CSR_TXEMPTY)) {
			;
		}
		dev->mmio->US_PTCR = US_PTCR_TXTDIS;
	} else {
		dev->mmio->US_CR = US_CR_TXEN;
		dev->mmio->US_CR = US_CR_SENDA;
		for (int i = 0; i < size; i++) {
			while (!(dev->mmio->US_CSR & US_CSR_TXRDY)) {
				;
			}
			if (dev->mmio->US_MR & US_MR_MODE9) {
				dev->mmio->US_THR = *((uint16_t *) p_buf + i);
			} else {
				dev->mmio->US_THR = *((uint8_t *) p_buf + i);
			}
		}
		while (!(dev->mmio->US_CSR & US_CSR_TXEMPTY)) {
			;
		}
	}
        dev->mmio->US_CR = US_CR_TXDIS;
        return (0);
}

/**
 * usart_rx_adr_char
 */
int usart_rx_adr_char(usart dev, void *p_char, boolean_t *p_adr, TickType_t tmo)
{
	uint16_t d;

	*p_adr = FALSE;
	if (!(dev->mmio->US_IMR & US_IMR_RXRDY)) {
		dev->mmio->US_IER = US_IER_RXRDY;
		dev->mmio->US_CR = US_CR_RXEN;
	}
	if (pdFALSE == xQueueReceive(dev->rx_que, &d, tmo)) {
		if (dev->mmio->US_MR & US_MR_MODE9) {
			*((uint16_t *) p_char) = '\0';
		} else {
			*((uint8_t *) p_char) = '\0';
		}
		return (-ETMO);
	}
	if (((usart) dev)->mmio->US_MR & US_MR_MODE9) {
		*((uint16_t *) p_char) = d & 0x01FF;
	} else {
		*((uint8_t *) p_char) = d;
	}
	d >>= 8;
	if (d & (US_CSR_OVRE | US_CSR_FRAME)) {
		return (-ERCV);
	} else {
		if (d & US_CSR_PARE) {
			*p_adr = TRUE;
		}
		return (0);
	}
}
#endif

#if USART_YIT == 1
/**
 * usart_rcv_yit_cmd
 */
struct yit_cmd *usart_rcv_yit_cmd(void *dev, TickType_t tmo)
{
	struct yit_cmd *cmd;

	if (!(((usart) dev)->mmio->US_IMR & US_IMR_RXRDY)) {
		((usart) dev)->mmio->US_IER = US_IER_RXRDY;
		((usart) dev)->mmio->US_CR = US_CR_RXEN;
	}
	if (pdTRUE == xQueueReceive(((usart) dev)->rx_que, &cmd, tmo)) {
		return (cmd);
	} else {
		return (NULL);
	}
}

/**
 * usart_rst_yit_drv
 */
void usart_rst_yit_drv(void *dev)
{
	struct yit_cmd *cmd;

	((usart) dev)->mmio->US_IDR = US_IDR_RXRDY;
        ((usart) dev)->mmio->US_CR = US_CR_RSTRX;
        ((usart) dev)->mmio->US_CR = US_CR_RXDIS;
	while (pdTRUE == xQueueReceive(((usart) dev)->rx_que, &cmd, 0)) {
		;
	}
        for (int i = 0; i < USART_YIT_CMD_ARY_SIZE; i++) {
		(((usart) dev)->usart_yit.cmd + i)->valid = FALSE;
	}
	((usart) dev)->rcv_st = YIT_WAIT_CA;
        barrier();
	((usart) dev)->mmio->US_IER = US_IER_RXRDY;
	((usart) dev)->mmio->US_CR = US_CR_RXEN;
}

/**
 * usart_free_yit_cmd_num
 */
int usart_free_yit_cmd_num(void *dev)
{
	int n = 0;

        for (int i = 0; i < USART_YIT_CMD_ARY_SIZE; i++) {
		if ((((usart) dev)->usart_yit.cmd + i)->valid == FALSE) {
			n++;
		}
	}
	return (n);
}

/**
 * yit_hndlr
 */
static BaseType_t yit_hndlr(usart dev)
{
	BaseType_t tsk_wkn = pdFALSE;
	unsigned int sr;
        struct yit_cmd *cmd;
        uint8_t d;
        int i;

	sr = dev->mmio->US_CSR;
	if (sr & US_CSR_RXRDY && dev->mmio->US_IMR & US_IMR_RXRDY) {
		d = dev->mmio->US_RHR;
		cmd = dev->usart_yit.cmd + dev->usart_yit.cmd_idx;
                if (cmd->valid) {
			for (i = 0; i < USART_YIT_CMD_ARY_SIZE; i++) {
				if ((dev->usart_yit.cmd + i)->valid) {
					continue;
				} else {
					cmd = dev->usart_yit.cmd + i;
					dev->usart_yit.cmd_idx = i;
					break;
				}
			}
			if (i == USART_YIT_CMD_ARY_SIZE) {
#if USART_YIT_DRIVER_STATS == 1
				dev->usart_yit.buf_err++;
#endif
				return (pdFALSE);
			}
			dev->rcv_st = YIT_WAIT_CA;
		}
		if (sr & (US_CSR_OVRE | US_CSR_FRAME | US_CSR_PARE)) {
			dev->mmio->US_CR = US_CR_RSTSTA;
#if USART_YIT_DRIVER_STATS == 1
			dev->usart_yit.ser_err++;
#endif
			dev->rcv_st = YIT_WAIT_CA;
			return (pdFALSE);
		}
		switch (dev->rcv_st) {
		case YIT_WAIT_CA     :
#if USART_YIT_DRIVER_STATS == 1
			if (d == YIT_MSG_FLAG) {
				dev->rcv_st = YIT_WAIT_SZ_LSB;
			} else {
				dev->usart_yit.syn_err++;
			}
#else
			if (d == YIT_MSG_FLAG) {
				dev->rcv_st = YIT_WAIT_SZ_LSB;
			}
#endif
			break;
		case YIT_WAIT_SZ_LSB :
			dev->rcv_st = YIT_WAIT_SZ_MSB;
			cmd->size = d;
			dev->usart_yit.sum = d;
			break;
		case YIT_WAIT_SZ_MSB :
			cmd->size |= d << 8;
			if (cmd->size <= USART_YIT_RCV_BUF_SIZE) {
				dev->rcv_st = YIT_RCV_DATA;
				dev->usart_yit.buf_idx = 0;
				dev->usart_yit.cmd_sz = cmd->size;
				dev->usart_yit.sum += d;
			} else {
				dev->rcv_st = YIT_WAIT_CA;
#if USART_YIT_DRIVER_STATS == 1
				dev->usart_yit.cmd_err++;
#endif
			}
			break;
		case YIT_RCV_DATA    :
			if (!--dev->usart_yit.cmd_sz) {
				dev->rcv_st = YIT_RCV_SUM;
			}
			*(cmd->buf + dev->usart_yit.buf_idx++) = d;
			dev->usart_yit.sum += d;
			break;
		case YIT_RCV_SUM     :
			if (dev->usart_yit.sum == d) {
				cmd->valid = TRUE;
				xQueueSendFromISR(dev->rx_que, &cmd, &tsk_wkn);
#if USART_YIT_DRIVER_STATS == 1
				if (++dev->usart_yit.rx_cmd_cnt == 1) {
					dev->usart_yit.syn_err = 0;
				}
#endif
			} else {
#if USART_YIT_DRIVER_STATS == 1
				dev->usart_yit.sum_err++;
#endif
			}
			dev->rcv_st = YIT_WAIT_CA;
			break;
		}
	} else if (sr & US_CSR_ENDTX && dev->mmio->US_IMR & US_IMR_ENDTX) {
        	dev->mmio->US_IDR = US_IDR_ENDTX;
                xSemaphoreGiveFromISR(dev->sig, &tsk_wkn);
	}
	return (tsk_wkn);
}
#endif

#if USART_RX_CHAR == 1 || USART_HDLC == 1 || USART_ADR_HDLC == 1 || USART_ADR_CHAR == 1 ||\
    USART_YIT == 1
/**
 * USART0_Handler
 */
void USART0_Handler(void)
{
	portEND_SWITCHING_ISR((*u0->hndlr)(u0));
}

#ifdef ID_USART1
/**
 * USART1_Handler
 */
void USART1_Handler(void)
{
	portEND_SWITCHING_ISR((*u1->hndlr)(u1));
}
#endif

#ifdef ID_USART2
/**
 * USART2_Handler
 */
void USART2_Handler(void)
{
	portEND_SWITCHING_ISR((*u2->hndlr)(u2));
}
#endif

/**
 * usart_get_dev
 */
usart usart_get_dev(int id)
{
	if (id == ID_USART0) {
		return (u0);
#ifdef ID_USART1
	} else if (id == ID_USART1) {
		return (u1);
#endif
#ifdef ID_USART2
	} else if (id == ID_USART2) {
		return (u2);
#endif
	} else {
		crit_err_exit(BAD_PARAMETER);
		return (NULL);
	}
}
#endif

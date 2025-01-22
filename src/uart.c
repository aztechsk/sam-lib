/*
 * uart.c
 *
 * Copyright (c) 2024 Jan Rusnak <jan@rusnak.sk>
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
#include "uart.h"

#define WAIT_PDC_INTR (1000 / portTICK_PERIOD_MS)

#if UART_RX_BYTE == 1 || UART_HDLC == 1
static uart u0;
#ifdef ID_UART1
static uart u1;
#endif
#ifdef ID_UART2
static uart u2;
#endif
#ifdef ID_UART3
static uart u3;
#endif
#endif

#if UART_RX_BYTE == 1
static BaseType_t rx_byte_hndlr(uart dev);
#endif
#if UART_HDLC == 1
static BaseType_t hdlc_hndlr(uart dev);
#endif

#if UART_RX_BYTE == 1 || UART_HDLC == 1
/**
 * init_uart
 */
void init_uart(uart dev, enum uart_rx_mode m)
{
	NVIC_DisableIRQ(dev->id);
	if (dev->id == ID_UART0) {
		u0 = dev;
		u0->mmio = UART0;
                u0->dma = TRUE;
#ifdef ID_UART1
	} else if (dev->id == ID_UART1) {
		u1 = dev;
		u1->mmio = UART1;
#ifdef PDC_UART1
                u1->dma = TRUE;
#else
                u1->dma = FALSE;
#endif
#endif
#ifdef ID_UART2
	} else if (dev->id == ID_UART2) {
		u2 = dev;
		u2->mmio = UART2;
#ifdef PDC_UART2
                u2->dma = TRUE;
#else
                u2->dma = FALSE;
#endif
#endif
#ifdef ID_UART3
	} else if (dev->id == ID_UART3) {
		u3 = dev;
		u3->mmio = UART3;
#ifdef PDC_UART3
                u3->dma = TRUE;
#else
                u3->dma = FALSE;
#endif
#endif
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
	switch (dev->rx_mode = m) {
#if UART_RX_BYTE == 1
	case UART_RX_BYTE_MODE :
		if (dev->rx_que == NULL) {
			dev->rx_que = xQueueCreate(dev->rx_que_sz, sizeof(uint16_t));
			if (dev->rx_que == NULL) {
				crit_err_exit(MALLOC_ERROR);
			}
		} else {
			crit_err_exit(UNEXP_PROG_STATE);
		}
		dev->hndlr = rx_byte_hndlr;
		break;
#endif
#if UART_HDLC == 1
	case UART_HDLC_MODE :
		if (NULL == (dev->hdlc_mesg.pld = pvPortMalloc(dev->hdlc_bf_sz))) {
			crit_err_exit(MALLOC_ERROR);
		}
		dev->hndlr = hdlc_hndlr;
		break;
#endif
	default :
		crit_err_exit(BAD_PARAMETER);
		break;
	}
	if (dev->tx_sig == NULL) {
		if (NULL == (dev->tx_sig = xSemaphoreCreateBinary())) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
#if UART_HDLC == 1
	if (dev->rx_sig == NULL) {
		if (NULL == (dev->rx_sig = xSemaphoreCreateBinary())) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
#endif
	enable_periph_clk(dev->id);
        dev->mmio->UART_IDR = ~0;
	dev->mmio->UART_CR = UART_CR_RSTRX | UART_CR_RSTTX | UART_CR_RSTSTA;
	NVIC_ClearPendingIRQ(dev->id);
	dev->mmio->UART_BRGR = F_MCK / 16 / dev->bdr;
	dev->mmio->UART_MR = dev->mr;
        dev->mmio->UART_PTCR = UART_PTCR_TXTDIS;
        dev->mmio->UART_TCR = 0;
	dev->mmio->UART_TNCR = 0;
	dev->mmio->UART_PTCR = UART_PTCR_RXTDIS;
        dev->mmio->UART_RCR = 0;
	dev->mmio->UART_RNCR = 0;
        NVIC_SetPriority(dev->id, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(dev->id);
}
#endif

#if UART_RX_BYTE == 1 || UART_HDLC == 1
/**
 * uart_tx_buff
 */
int uart_tx_buff(void *dev, void *p_buf, int size)
{
        if (size < 1) {
                return (0);
        }
	if (((uart) dev)->dma) {
		((uart) dev)->mmio->UART_TCR = size;
		((uart) dev)->mmio->UART_TPR = (unsigned int) p_buf;
		((uart) dev)->mmio->UART_IER = UART_IER_ENDTX;
		((uart) dev)->mmio->UART_CR = UART_CR_TXEN;
		((uart) dev)->mmio->UART_PTCR = UART_PTCR_TXTEN;
		if (pdFALSE == xSemaphoreTake(((uart) dev)->tx_sig, WAIT_PDC_INTR) ||
		    ((uart) dev)->mmio->UART_TCR != 0) {
			((uart) dev)->mmio->UART_IDR = UART_IDR_ENDTX;
			((uart) dev)->mmio->UART_PTCR = UART_PTCR_TXTDIS;
			((uart) dev)->mmio->UART_TCR = 0;
			((uart) dev)->mmio->UART_CR = UART_CR_RSTTX;
			xSemaphoreTake(((uart) dev)->tx_sig, 0);
			return (-EDMA);
		}
		while (!(((uart) dev)->mmio->UART_SR & UART_SR_TXEMPTY)) {
			;
		}
		((uart) dev)->mmio->UART_PTCR = UART_PTCR_TXTDIS;
	} else {
		((uart) dev)->mmio->UART_CR = UART_CR_TXEN;
		for (int i = 0; i < size; i++) {
			while (!(((uart) dev)->mmio->UART_SR & UART_SR_TXRDY)) {
				;
			}
			((uart) dev)->mmio->UART_THR = *((uint8_t *) p_buf + i);
		}
		while (!(((uart) dev)->mmio->UART_SR & UART_SR_TXEMPTY)) {
			;
		}
	}
	((uart) dev)->mmio->UART_CR = UART_CR_TXDIS;
        return (0);
}
#endif

#if UART_RX_BYTE == 1
/**
 * uart_rx_byte
 */
int uart_rx_byte(void *dev, void *p_byte, TickType_t tmo)
{
	uint16_t d;

	if (!(((uart) dev)->mmio->UART_IMR & UART_IMR_RXRDY)) {
		((uart) dev)->mmio->UART_IER = UART_IER_RXRDY;
		((uart) dev)->mmio->UART_CR = UART_CR_RXEN;
	}
	if (pdFALSE == xQueueReceive(((uart) dev)->rx_que, &d, tmo)) {
		*((uint8_t *) p_byte) = '\0';
		return (-ETMO);
	}
	*((uint8_t *) p_byte) = d;
	if (d & 0x1000) {
		return (-EINTR);
	}
	if (d >> 8 & (UART_SR_OVRE | UART_SR_FRAME | UART_SR_PARE)) {
		return (-ERCV);
	} else {
		return (0);
	}
}

#if UART_RX_BYTE == 1
/**
 * uart_intr_rx
 */
boolean_t uart_intr_rx(void *dev)
{
	uint16_t d = 0x1000;

	if (pdTRUE == xQueueSend(((uart) dev)->rx_que, &d, 0)) {
		return (TRUE);
	} else {
		return (FALSE);
	}
}
#endif

/**
 * rx_byte_hndlr
 */
static BaseType_t rx_byte_hndlr(uart dev)
{
        BaseType_t tsk_wkn = pdFALSE;
	unsigned int sr;
        uint16_t d;

	sr = dev->mmio->UART_SR;
	if (sr & UART_SR_RXRDY && dev->mmio->UART_IMR & UART_IMR_RXRDY) {
		d = dev->mmio->UART_RHR & 0xFF;
		if (sr & (UART_SR_OVRE | UART_SR_FRAME | UART_SR_PARE)) {
			d |= (sr & 0xE0) << 8;
			dev->mmio->UART_CR = UART_CR_RSTSTA;
		}
		xQueueSendFromISR(dev->rx_que, &d, &tsk_wkn);
	} else if (sr & UART_SR_ENDTX && dev->mmio->UART_IMR & UART_IMR_ENDTX) {
        	dev->mmio->UART_IDR = UART_IDR_ENDTX;
                xSemaphoreGiveFromISR(dev->tx_sig, &tsk_wkn);
	}
	return (tsk_wkn);
}
#endif

#if UART_HDLC == 1
enum {
        HDLC_RCV_FLAG_1,
        HDLC_RCV_DATA,
        HDLC_RCV_ESC
};

/**
 * uart_tx_hdlc_mesg
 */
int uart_tx_hdlc_mesg(uart dev, uint8_t *pld, int size)
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
	return (uart_tx_buff(dev, dev->hdlc_mesg.pld, sz));
}

/**
 * uart_rx_hdlc_mesg
 */
struct hdlc_mesg *uart_rx_hdlc_mesg(uart dev, TickType_t tmo)
{
	dev->rcv_st = HDLC_RCV_FLAG_1;
        dev->mmio->UART_CR = UART_CR_RSTRX;
        barrier();
	dev->mmio->UART_IER = UART_IER_RXRDY;
	dev->mmio->UART_CR = UART_CR_RXEN;
	if (pdFALSE == xSemaphoreTake(dev->rx_sig, tmo)) {
		dev->mmio->UART_IDR = UART_IDR_RXRDY;
                dev->mmio->UART_CR = UART_CR_RXDIS;
                xSemaphoreTake(dev->rx_sig, 0);
                return (NULL);
	} else {
		return (&dev->hdlc_mesg);
	}
}

/**
 * hdlc_hndlr
 */
static BaseType_t hdlc_hndlr(uart dev)
{
	BaseType_t tsk_wkn = pdFALSE;
	unsigned int sr;
        uint8_t d;

	sr = dev->mmio->UART_SR;
	if (sr & UART_SR_RXRDY && dev->mmio->UART_IMR & UART_IMR_RXRDY) {
		d = dev->mmio->UART_RHR;
		if (sr & UART_SR_OVRE) {
			dev->mmio->UART_CR = UART_CR_RSTSTA;
                        dev->hdlc_stats.ovr_lerr++;
                        dev->rcv_st = HDLC_RCV_FLAG_1;
			return (pdFALSE);
		} else if (sr & UART_SR_FRAME) {
			dev->mmio->UART_CR = UART_CR_RSTSTA;
			dev->hdlc_stats.fra_lerr++;
                        dev->rcv_st = HDLC_RCV_FLAG_1;
                        return (pdFALSE);
		} else if (sr & UART_SR_PARE) {
			dev->mmio->UART_CR = UART_CR_RSTSTA;
			dev->hdlc_stats.par_lerr++;
                        dev->rcv_st = HDLC_RCV_FLAG_1;
                        return (pdFALSE);
		}
                switch (dev->rcv_st) {
		case HDLC_RCV_FLAG_1 :
			if (d == dev->HDLC_FLAG) {
				dev->rcv_st = HDLC_RCV_DATA;
                                dev->hdlc_mesg.sz = 0;
			} else {
				dev->hdlc_stats.no_f1_perr++;
			}
			break;
		case HDLC_RCV_DATA :
			if (d == dev->HDLC_FLAG) {
				if (dev->hdlc_mesg.sz != 0) {
					dev->mmio->UART_IDR = UART_IDR_RXRDY;
					dev->mmio->UART_CR = UART_CR_RXDIS;
					xSemaphoreGiveFromISR(dev->rx_sig, &tsk_wkn);
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
		case HDLC_RCV_ESC :
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
	} else if (sr & UART_SR_ENDTX && dev->mmio->UART_IMR & UART_IMR_ENDTX) {
        	dev->mmio->UART_IDR = UART_IDR_ENDTX;
                xSemaphoreGiveFromISR(dev->tx_sig, &tsk_wkn);
	}
	return (tsk_wkn);
}
#endif

#if UART_RX_BYTE == 1 || UART_HDLC == 1
/**
 * UART0_Handler
 */
void UART0_Handler(void)
{
	portEND_SWITCHING_ISR((*u0->hndlr)(u0));
}

#ifdef ID_UART1
/**
 * UART1_Handler
 */
void UART1_Handler(void)
{
	portEND_SWITCHING_ISR((*u1->hndlr)(u1));
}
#endif

#ifdef ID_UART2
/**
 * UART2_Handler
 */
void UART2_Handler(void)
{
	portEND_SWITCHING_ISR((*u2->hndlr)(u2));
}
#endif

#ifdef ID_UART3
/**
 * UART3_Handler
 */
void UART3_Handler(void)
{
	portEND_SWITCHING_ISR((*u3->hndlr)(u3));
}
#endif

/**
 * uart_get_dev
 */
uart uart_get_dev(int id)
{
	if (id == ID_UART0) {
		return (u0);
#ifdef ID_UART1
	} else if (id == ID_UART1) {
		return (u1);
#endif
#ifdef ID_UART2
	} else if (id == ID_UART2) {
		return (u2);
#endif
#ifdef ID_UART3
	} else if (id == ID_UART3) {
		return (u3);
#endif
	} else {
		crit_err_exit(BAD_PARAMETER);
		return (NULL);
	}
}
#endif

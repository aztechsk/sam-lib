/*
 * i2c.c
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
#include "hwerr.h"
#include "pmc.h"
#include "i2c.h"
#include <stdarg.h>

#if I2CM == 1

#define WAIT_INTR (200 / portTICK_PERIOD_MS)
#define LOW_LEV_TM_LIMIT 384000
#define FAST_MODE_SPEED  400000

static i2c i2c0;
static i2c i2c1;
#ifdef ID_TWI2
static i2c i2c2;
#endif

static void set_cwgr(i2c dev);
static BaseType_t i2cm_read_hndlr(i2c dev);
static BaseType_t i2cm_dma_read_hndlr(i2c dev);
static BaseType_t i2cm_write_hndlr(i2c dev);
static BaseType_t i2cm_dma_write_hndlr(i2c dev);

/**
 * init_i2c
 */
void init_i2c(i2c dev)
{
	NVIC_DisableIRQ(dev->id);
        if (dev->id == ID_TWI0) {
		i2c0 = dev;
		dev->mmio = TWI0;
#ifdef PDC_TWI0
		dev->dma = TRUE;
#endif
	} else if (dev->id == ID_TWI1) {
		i2c1 = dev;
                dev->mmio = TWI1;
#ifdef PDC_TWI1
		dev->dma = TRUE;
#endif
#ifdef ID_TWI2
	} else if (dev->id == ID_TWI2) {
		i2c2 = dev;
                dev->mmio = TWI2;
#ifdef PDC_TWI2
		dev->dma = TRUE;
#endif
#endif
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
	if (dev->sig_que == NULL) {
		dev->sig_que = xQueueCreate(1, sizeof(int8_t));
		if (dev->sig_que == NULL) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
        enable_periph_clk(dev->id);
        dev->mmio->TWI_CR = TWI_CR_SWRST;
	dev->mmio->TWI_CR = TWI_CR_SVDIS;
        dev->mmio->TWI_CR = TWI_CR_MSDIS;
        dev->mmio->TWI_IDR = ~0;
	dev->mmio->TWI_SR;
	if (dev->clk_hz != 0) {
		set_cwgr(dev);
	}
	dev->ini = 0;
	NVIC_ClearPendingIRQ(dev->id);
	NVIC_SetPriority(dev->id, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(dev->id);
        disable_periph_clk(dev->id);
}

/**
 * set_cwgr
 */
static void set_cwgr(i2c dev)
{
	int ckdiv = 0;

	if (dev->clk_hz > FAST_MODE_SPEED) {
		crit_err_exit(BAD_PARAMETER);
	}
        if (dev->clk_hz > LOW_LEV_TM_LIMIT) { // Tlow >= 1.3us in I2C fast mode.
		int chdiv = F_MCK / ((dev->clk_hz + (dev->clk_hz - LOW_LEV_TM_LIMIT)) * 2) - 4;
                int cldiv = F_MCK / (LOW_LEV_TM_LIMIT * 2) - 4;
		while (cldiv > 255) {
			chdiv /= 2;
			cldiv /= 2;
			if (++ckdiv > 7) {
				crit_err_exit(BAD_PARAMETER);
			}
		}
		dev->mmio->TWI_CWGR = TWI_CWGR_CKDIV(ckdiv) | TWI_CWGR_CHDIV(chdiv) | TWI_CWGR_CLDIV(cldiv);
	} else {
		int div = F_MCK / (dev->clk_hz * 2) - 4;
		while (div > 255) {
			div /= 2;
			if (++ckdiv > 7) {
				crit_err_exit(BAD_PARAMETER);
			}
		}
                dev->mmio->TWI_CWGR = TWI_CWGR_CKDIV(ckdiv) | TWI_CWGR_CHDIV(div) | TWI_CWGR_CLDIV(div);
	}
}

/**
 * i2cm_read
 */
int i2cm_read(i2c dev, enum i2cm_mode mode, int adr, uint8_t *p_buf, int size, boolean_t dma, ...)
{
	va_list ap;
	int iadr = 0;
	int8_t msg;

	if (mode != I2CM_MODE_7BIT_ADR && mode != I2CM_MODE_10BIT_ADR) {
		va_start(ap, dma);
		iadr = va_arg(ap, int);
		va_end(ap);
		if (mode == I2CM_MODE_7BIT_ADR_IADR1 || mode == I2CM_MODE_10BIT_ADR_IADR1) {
			if (iadr & 0xFFFFFF00) {
				return (-EADDR);
			}
		} else if (mode == I2CM_MODE_7BIT_ADR_IADR2 || mode == I2CM_MODE_10BIT_ADR_IADR2) {
			if (iadr & 0xFFFF0000) {
				return (-EADDR);
			}
		}
	}
	if (size < 1) {
		crit_err_exit(BAD_PARAMETER);
	}
	if (dev->mtx != NULL) {
		xSemaphoreTake(dev->mtx, portMAX_DELAY);
	}
        dev->ovre = FALSE;
        enable_periph_clk(dev->id);
	if (dev->ini == 0 || dev->ini == 2) {
		dev->ini = 1;
		dev->mmio->TWI_CR = TWI_CR_MSDIS;
		dev->mmio->TWI_CR = TWI_CR_SVDIS;
		dev->mmio->TWI_CR = TWI_CR_MSEN;
	}
	if (mode < I2CM_MODE_10BIT_ADR) {
		dev->mmio->TWI_MMR = TWI_MMR_DADR(adr) | TWI_MMR_MREAD | mode << 8;
		if (mode > I2CM_MODE_7BIT_ADR) {
			dev->mmio->TWI_IADR = iadr;
		}
	} else {
		int a = 0x78;
		a |= adr >> 8;
                dev->mmio->TWI_MMR = TWI_MMR_DADR(a) | TWI_MMR_MREAD | (mode - 3) << 8;
		if (mode == I2CM_MODE_10BIT_ADR) {
			iadr = adr & 0xFF;
		} else if (mode == I2CM_MODE_10BIT_ADR_IADR1) {
			iadr |= (adr & 0xFF) << 8;
		} else {
			iadr |= (adr & 0xFF) << 16;
		}
		dev->mmio->TWI_IADR = iadr;
	}
	dev->mmio->TWI_SR;
	if (dev->dma && dma && size >= 4) {
		dev->hndlr = i2cm_dma_read_hndlr;
		dev->cnt = 2;
		dev->buf = p_buf + size - 2;
		dev->mmio->TWI_RPR = (unsigned int) p_buf;
                dev->mmio->TWI_RCR = size - 2;
                dev->mmio->TWI_RNCR = 0;
                dev->mmio->TWI_PTCR = TWI_PTCR_RXTEN;
                dev->mmio->TWI_CR = TWI_CR_START;
                barrier();
		dev->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_ENDRX;
	} else {
		dev->hndlr = i2cm_read_hndlr;
		dev->cnt = size;
                dev->buf = p_buf;
		if (size == 1) {
			dev->mmio->TWI_CR = TWI_CR_STOP | TWI_CR_START;
		} else {
			dev->mmio->TWI_CR = TWI_CR_START;
		}
		barrier();
                dev->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_RXRDY;
	}
        if (pdFALSE == xQueueReceive(dev->sig_que, &msg, WAIT_INTR)) {
		dev->mmio->TWI_IDR = ~0;
		dev->mmio->TWI_CR = TWI_CR_SWRST;
		dev->mmio->TWI_CR = TWI_CR_MSDIS;
		dev->mmio->TWI_CR = TWI_CR_SVDIS;
		dev->mmio->TWI_SR;
		set_cwgr(dev);
		xQueueReceive(dev->sig_que, &msg, 0);
		dev->ini = 0;
		msg = EHW;
	}
        disable_periph_clk(dev->id);
	if (dev->mtx != NULL) {
		xSemaphoreGive(dev->mtx);
	}
	return (-msg);
}

/**
 * i2cm_read_hndlr
 */
static BaseType_t i2cm_read_hndlr(i2c dev)
{
	BaseType_t tsk_wkn = pdFALSE;
        int8_t msg;

	unsigned int sr = dev->mmio->TWI_SR;
	if (sr & TWI_SR_NACK) {
		dev->mmio->TWI_IDR = ~0;
		msg = ENACK;
                xQueueSendFromISR(dev->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
	if (sr & TWI_SR_OVRE) {
		dev->ovre = TRUE;
	}
        if (sr & TWI_SR_TXCOMP && dev->mmio->TWI_IMR & TWI_IMR_TXCOMP) {
		dev->mmio->TWI_IDR = ~0;
		msg = (dev->ovre == FALSE) ? 0 : EDATA;
		xQueueSendFromISR(dev->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
	if (sr & TWI_SR_RXRDY) {
                if (--dev->cnt < 2) {
			if (dev->cnt == 1) {
				dev->mmio->TWI_CR = TWI_CR_STOP;
			} else {
				dev->mmio->TWI_IER = TWI_IER_TXCOMP;
				dev->mmio->TWI_IDR = TWI_IDR_NACK | TWI_IDR_RXRDY;
			}
		}
		*dev->buf++ = dev->mmio->TWI_RHR;
	}
        return (tsk_wkn);
}

/**
 * i2cm_dma_read_hndlr
 */
static BaseType_t i2cm_dma_read_hndlr(i2c dev)
{
	BaseType_t tsk_wkn = pdFALSE;
        int8_t msg;

	unsigned int sr = dev->mmio->TWI_SR;
	dev->mmio->TWI_PTCR = TWI_PTCR_RXTDIS;
	if (sr & TWI_SR_NACK) {
		dev->mmio->TWI_IDR = ~0;
		msg = ENACK;
                xQueueSendFromISR(dev->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
        if (sr & TWI_SR_ENDRX) {
        	dev->hndlr = i2cm_read_hndlr;
		dev->mmio->TWI_IDR = TWI_IDR_ENDRX;
		dev->mmio->TWI_IER = TWI_IER_RXRDY;
	}
        return (tsk_wkn);
}

/**
 * i2cm_write
 */
int i2cm_write(i2c dev, enum i2cm_mode mode, int adr, uint8_t *p_buf, int size, boolean_t dma, ...)
{
	va_list ap;
	int iadr = 0;
	int8_t msg;

	if (mode != I2CM_MODE_7BIT_ADR && mode != I2CM_MODE_10BIT_ADR) {
		va_start(ap, dma);
		iadr = va_arg(ap, int);
		va_end(ap);
		if (mode == I2CM_MODE_7BIT_ADR_IADR1 || mode == I2CM_MODE_10BIT_ADR_IADR1) {
			if (iadr & 0xFFFFFF00) {
				return (-EADDR);
			}
		} else if (mode == I2CM_MODE_7BIT_ADR_IADR2 || mode == I2CM_MODE_10BIT_ADR_IADR2) {
			if (iadr & 0xFFFF0000) {
				return (-EADDR);
			}
		}
	}
	if (size < 1) {
		crit_err_exit(BAD_PARAMETER);
	}
	if (dev->mtx != NULL) {
		xSemaphoreTake(dev->mtx, portMAX_DELAY);
	}
        enable_periph_clk(dev->id);
	if (dev->ini == 0 || dev->ini == 2) {
		dev->ini = 1;
		dev->mmio->TWI_CR = TWI_CR_MSDIS;
		dev->mmio->TWI_CR = TWI_CR_SVDIS;
		dev->mmio->TWI_CR = TWI_CR_MSEN;
	}
	if (mode < I2CM_MODE_10BIT_ADR) {
		dev->mmio->TWI_MMR = TWI_MMR_DADR(adr) | mode << 8;
		if (mode > I2CM_MODE_7BIT_ADR) {
			dev->mmio->TWI_IADR = iadr;
		}
	} else {
		int a = 0x78;
		a |= adr >> 8;
                dev->mmio->TWI_MMR = TWI_MMR_DADR(a) | (mode - 3) << 8;
		if (mode == I2CM_MODE_10BIT_ADR) {
			iadr = adr & 0xFF;
		} else if (mode == I2CM_MODE_10BIT_ADR_IADR1) {
			iadr |= (adr & 0xFF) << 8;
		} else {
			iadr |= (adr & 0xFF) << 16;
		}
		dev->mmio->TWI_IADR = iadr;
	}
        taskENTER_CRITICAL();
	dev->mmio->TWI_SR;
	if (dev->dma && dma && size >= 3) {
		dev->hndlr = i2cm_dma_write_hndlr;
		dev->cnt = 1;
		dev->buf = p_buf + size - 1;
		dev->mmio->TWI_TPR = (unsigned int) p_buf;
                dev->mmio->TWI_TCR = size - 1;
                dev->mmio->TWI_TNCR = 0;
                dev->mmio->TWI_PTCR = TWI_PTCR_TXTEN;
		barrier();
		dev->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_ENDTX;
	} else {
		dev->hndlr = i2cm_write_hndlr;
		dev->mmio->TWI_THR = *p_buf;
		if (size == 1) {
			dev->cnt = 0;
			dev->mmio->TWI_CR = TWI_CR_STOP;
			barrier();
			dev->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_TXCOMP;
		} else {
			dev->cnt = size - 1;
			dev->buf = p_buf + 1;
			barrier();
			dev->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_TXRDY;
		}
	}
        taskEXIT_CRITICAL();
        if (pdFALSE == xQueueReceive(dev->sig_que, &msg, WAIT_INTR)) {
		dev->mmio->TWI_IDR = ~0;
                dev->mmio->TWI_CR = TWI_CR_SWRST;
		dev->mmio->TWI_CR = TWI_CR_MSDIS;
		dev->mmio->TWI_CR = TWI_CR_SVDIS;
		dev->mmio->TWI_SR;
		set_cwgr(dev);
		xQueueReceive(dev->sig_que, &msg, 0);
		dev->ini = 0;
		msg = EHW;
	}
        disable_periph_clk(dev->id);
	if (dev->mtx != NULL) {
		xSemaphoreGive(dev->mtx);
	}
	return (-msg);
}

/**
 * i2cm_write_hndlr
 */
static BaseType_t i2cm_write_hndlr(i2c dev)
{
	BaseType_t tsk_wkn = pdFALSE;
        int8_t msg;

	unsigned int sr = dev->mmio->TWI_SR;
	if (sr & TWI_SR_NACK) {
		dev->mmio->TWI_IDR = ~0;
		msg = ENACK;
                xQueueSendFromISR(dev->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
	if (sr & TWI_SR_TXCOMP) {
		dev->mmio->TWI_IDR = ~0;
		msg = 0;
		xQueueSendFromISR(dev->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
	if (sr & TWI_SR_TXRDY) {
		dev->mmio->TWI_THR = *dev->buf++;
		if (--dev->cnt == 0) {
			dev->mmio->TWI_CR = TWI_CR_STOP;
			dev->mmio->TWI_IER = TWI_IER_TXCOMP;
			dev->mmio->TWI_IDR = TWI_IDR_TXRDY;
		}
	}
        return (tsk_wkn);
}

/**
 * i2cm_dma_write_hndlr
 */
static BaseType_t i2cm_dma_write_hndlr(i2c dev)
{
	BaseType_t tsk_wkn = pdFALSE;
        int8_t msg;

	unsigned int sr = dev->mmio->TWI_SR;
	dev->mmio->TWI_PTCR = TWI_PTCR_TXTDIS;
	if (sr & TWI_SR_NACK) {
		dev->mmio->TWI_IDR = ~0;
		msg = ENACK;
                xQueueSendFromISR(dev->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
        if (sr & TWI_SR_ENDTX) {
        	dev->hndlr = i2cm_write_hndlr;
		dev->mmio->TWI_IDR = TWI_IDR_ENDTX;
		dev->mmio->TWI_IER = TWI_IER_TXRDY;
	}
        return (tsk_wkn);
}

/**
 * TWI0_Handler
 */
void TWI0_Handler(void)
{
	portEND_SWITCHING_ISR((*i2c0->hndlr)(i2c0));
}

/**
 * TWI1_Handler
 */
void TWI1_Handler(void)
{
	portEND_SWITCHING_ISR((*i2c1->hndlr)(i2c1));
}

#ifdef ID_TWI2
/**
 * TWI2_Handler
 */
void TWI2_Handler(void)
{
	portEND_SWITCHING_ISR((*i2c2->hndlr)(i2c2));
}
#endif

#endif

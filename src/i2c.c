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
#include <string.h>
#include <stdarg.h>
#include "sysconf.h"
#include "board.h"
#include <mmio.h>
#include "msgconf.h"
#include "criterr.h"
#include "atom.h"
#include "hwerr.h"
#include "pmc.h"
#include "i2c.h"

#if I2CBUS == 1

#define WAIT_INTR_MS 1000
#define LOW_LEV_TM_LIMIT 384000
#define FAST_MODE_SPEED  400000
#define CWGR_CEIL 1

static i2cbus i2c0;
#ifdef ID_TWI1
static i2cbus i2c1;
#endif
#ifdef ID_TWI2
static i2cbus i2c2;
#endif

static void set_cwgr(i2cbus bus);
static BaseType_t i2c_read_hndlr(i2cbus bus);
static BaseType_t i2c_dma_read_hndlr(i2cbus bus);
static BaseType_t i2c_write_hndlr(i2cbus bus);
static BaseType_t i2c_dma_write_hndlr(i2cbus bus);
static BaseType_t i2c_empty_hndlr(i2cbus bus);
static IRQn_Type busid2irqn(int per_id);
static inline uint32_t udiv_ceil(uint32_t a, uint32_t b);

/**
 * init_i2c
 */
void init_i2c(i2cbus bus)
{
	NVIC_DisableIRQ(busid2irqn(bus->id));
	memset(&bus->stats, 0, sizeof(bus->stats));
	bus->dma = FALSE;
        if (bus->id == ID_TWI0) {
		i2c0 = bus;
		bus->mmio = TWI0;
#ifdef PDC_TWI0
		bus->dma = TRUE;
#endif
		bus->nm = "TWI0";
#ifdef ID_TWI1
	} else if (bus->id == ID_TWI1) {
		i2c1 = bus;
		bus->mmio = TWI1;
#ifdef PDC_TWI1
		bus->dma = TRUE;
#endif
		bus->nm = "TWI1";
#endif
#ifdef ID_TWI2
	} else if (bus->id == ID_TWI2) {
		i2c2 = bus;
		bus->mmio = TWI2;
#ifdef PDC_TWI2
		bus->dma = TRUE;
#endif
		bus->nm = "TWI2";
#endif
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
	if (bus->sig_que == NULL) {
		bus->sig_que = xQueueCreate(1, sizeof(int8_t));
		if (bus->sig_que == NULL) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
        enable_periph_clk(bus->id);
        bus->mmio->TWI_CR = TWI_CR_SWRST;
	bus->mmio->TWI_CR = TWI_CR_SVDIS;
        bus->mmio->TWI_CR = TWI_CR_MSDIS;
        bus->mmio->TWI_IDR = ~0;
	bus->mmio->TWI_SR;
	if (!(bus->clk_hz > 0)) {
		crit_err_exit(BAD_PARAMETER);
	}
	set_cwgr(bus);
	bus->ini = 0;
	NVIC_ClearPendingIRQ(busid2irqn(bus->id));
	NVIC_SetPriority(busid2irqn(bus->id), configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	bus->hndlr = i2c_empty_hndlr;
	NVIC_EnableIRQ(busid2irqn(bus->id));
        disable_periph_clk(bus->id);
}

#if CWGR_CEIL == 1
/**
 * set_cwgr
 */
static void set_cwgr(i2cbus bus)
{
	uint32_t ckdiv = 0;
	uint32_t clk = bus->clk_hz;
	uint32_t mck = F_MCK;

	if (clk == 0 || clk > FAST_MODE_SPEED) {
		crit_err_exit(BAD_PARAMETER);
	}
	for (ckdiv = 0; ckdiv <= 7; ckdiv++) {
		uint32_t scale = 1 << ckdiv;
		uint32_t t_total = udiv_ceil(mck, clk);
		if (t_total <= 8) {
			t_total = 9;
		}
		uint32_t sum_needed = udiv_ceil(t_total - 8, scale);
		uint32_t cldiv, chdiv;
		if (clk > LOW_LEV_TM_LIMIT) {
			uint32_t low_counts = udiv_ceil(mck, 2 * LOW_LEV_TM_LIMIT);
			uint32_t cl_min = 0;
			if (low_counts > 4) {
				cl_min = udiv_ceil(low_counts - 4, scale);
			}
			cldiv = cl_min;
			chdiv = (sum_needed > cldiv) ? (sum_needed - cldiv) : 0;
		} else {
			uint32_t div = udiv_ceil(t_total - 8, 2 * scale);
			cldiv = div;
			chdiv = div;
		}
		if (cldiv <= 255 && chdiv <= 255) {
			bus->mmio->TWI_CWGR = TWI_CWGR_CKDIV(ckdiv) | TWI_CWGR_CHDIV(chdiv) | TWI_CWGR_CLDIV(cldiv);
			bus->cwgr_reg = bus->mmio->TWI_CWGR;
			return;
		}
	}
	crit_err_exit(BAD_PARAMETER);
}
#else
/**
 * set_cwgr
 */
static void set_cwgr(i2cbus bus)
{
	int ckdiv = 0;

	if (bus->clk_hz > FAST_MODE_SPEED) {
		crit_err_exit(BAD_PARAMETER);
	}
        if (bus->clk_hz > LOW_LEV_TM_LIMIT) { // Tlow >= 1.3us in I2C fast mode.
		int chdiv = F_MCK / ((bus->clk_hz + (bus->clk_hz - LOW_LEV_TM_LIMIT)) * 2) - 4;
                int cldiv = F_MCK / (LOW_LEV_TM_LIMIT * 2) - 4;
		while (cldiv > 255) {
			chdiv /= 2;
			cldiv /= 2;
			if (++ckdiv > 7) {
				crit_err_exit(BAD_PARAMETER);
			}
		}
		bus->mmio->TWI_CWGR = TWI_CWGR_CKDIV(ckdiv) | TWI_CWGR_CHDIV(chdiv) | TWI_CWGR_CLDIV(cldiv);
	} else {
		int div = F_MCK / (bus->clk_hz * 2) - 4;
		while (div > 255) {
			div /= 2;
			if (++ckdiv > 7) {
				crit_err_exit(BAD_PARAMETER);
			}
		}
		bus->mmio->TWI_CWGR = TWI_CWGR_CKDIV(ckdiv) | TWI_CWGR_CHDIV(div) | TWI_CWGR_CLDIV(div);
	}
	bus->cwgr_reg = bus->mmio->TWI_CWGR;
}
#endif

/**
 * i2c_read
 */
int i2c_read(i2cbus bus, enum i2c_mode mode, int adr, uint8_t *p_buf, int size, boolean_t dma, ...)
{
	va_list ap;
	int iadr = 0;
	int8_t msg;

	if (mode != I2C_MODE_7BIT_ADR && mode != I2C_MODE_10BIT_ADR) {
		va_start(ap, dma);
		iadr = va_arg(ap, int);
		va_end(ap);
		if (mode == I2C_MODE_7BIT_ADR_IADR1 || mode == I2C_MODE_10BIT_ADR_IADR1) {
			if (iadr & 0xFFFFFF00) {
				return (-EADDR);
			}
		} else if (mode == I2C_MODE_7BIT_ADR_IADR2 || mode == I2C_MODE_10BIT_ADR_IADR2) {
			if (iadr & 0xFFFF0000) {
				return (-EADDR);
			}
		} else {
			if (iadr & 0xFF000000) {
				return (-EADDR);
			}
		}
	}
	if (size < 1) {
		crit_err_exit(BAD_PARAMETER);
	}
	if (bus->mtx != NULL) {
		xSemaphoreTake(bus->mtx, portMAX_DELAY);
	}
        bus->ovre = FALSE;
        enable_periph_clk(bus->id);
	if (bus->ini == 0 || bus->ini == 2) {
		bus->ini = 1;
		bus->mmio->TWI_CR = TWI_CR_MSDIS;
		bus->mmio->TWI_CR = TWI_CR_SVDIS;
		bus->mmio->TWI_CR = TWI_CR_MSEN;
	}
	if (mode < I2C_MODE_10BIT_ADR) {
		bus->mmio->TWI_MMR = TWI_MMR_DADR(adr) | TWI_MMR_MREAD | mode << 8;
		if (mode > I2C_MODE_7BIT_ADR) {
			bus->mmio->TWI_IADR = iadr;
		}
	} else {
		int a = 0x78;
		a |= adr >> 8;
                bus->mmio->TWI_MMR = TWI_MMR_DADR(a) | TWI_MMR_MREAD | (mode - 3) << 8;
		if (mode == I2C_MODE_10BIT_ADR) {
			iadr = adr & 0xFF;
		} else if (mode == I2C_MODE_10BIT_ADR_IADR1) {
			iadr |= (adr & 0xFF) << 8;
		} else {
			iadr |= (adr & 0xFF) << 16;
		}
		bus->mmio->TWI_IADR = iadr;
	}
	bus->mmio->TWI_SR;
	if (bus->dma && dma && size >= 4) {
		bus->hndlr = i2c_dma_read_hndlr;
		bus->cnt = 2;
		bus->buf = p_buf + size - 2;
		bus->mmio->TWI_RPR = (unsigned int) p_buf;
                bus->mmio->TWI_RCR = size - 2;
                bus->mmio->TWI_RNCR = 0;
                bus->mmio->TWI_PTCR = TWI_PTCR_RXTEN;
                bus->mmio->TWI_CR = TWI_CR_START;
                barrier();
		bus->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_ENDRX;
	} else {
		bus->hndlr = i2c_read_hndlr;
		bus->cnt = size;
                bus->buf = p_buf;
		if (size == 1) {
			bus->mmio->TWI_CR = TWI_CR_STOP | TWI_CR_START;
		} else {
			bus->mmio->TWI_CR = TWI_CR_START;
		}
		barrier();
                bus->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_RXRDY;
	}
        if (pdFALSE == xQueueReceive(bus->sig_que, &msg, ms_to_os_ticks(WAIT_INTR_MS))) {
		bus->mmio->TWI_IDR = ~0;
		bus->mmio->TWI_CR = TWI_CR_SWRST;
		bus->mmio->TWI_CR = TWI_CR_MSDIS;
		bus->mmio->TWI_CR = TWI_CR_SVDIS;
		bus->mmio->TWI_SR;
		set_cwgr(bus);
		xQueueReceive(bus->sig_que, &msg, 0);
		bus->ini = 0;
		bus->stats.intr_tmo_err_cnt++;
		msg = EHW;
	}
	if (msg == 0) {
		bus->stats.rx_bytes_cnt += size;
	}
        disable_periph_clk(bus->id);
	if (bus->mtx != NULL) {
		xSemaphoreGive(bus->mtx);
	}
	return (-msg);
}

/**
 * i2c_read_hndlr
 */
static BaseType_t i2c_read_hndlr(i2cbus bus)
{
	BaseType_t tsk_wkn = pdFALSE;
        int8_t msg;

	unsigned int sr = bus->mmio->TWI_SR;
	if (sr & TWI_SR_NACK) {
		bus->mmio->TWI_IDR = ~0;
		msg = ENACK;
                xQueueSendFromISR(bus->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
	if (sr & TWI_SR_OVRE) {
		bus->ovre = TRUE;
	}
        if (sr & TWI_SR_TXCOMP && bus->mmio->TWI_IMR & TWI_IMR_TXCOMP) {
		bus->mmio->TWI_IDR = ~0;
		msg = (bus->ovre == FALSE) ? 0 : EDATA;
		xQueueSendFromISR(bus->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
	if (sr & TWI_SR_RXRDY) {
                if (--bus->cnt < 2) {
			if (bus->cnt == 1) {
				bus->mmio->TWI_CR = TWI_CR_STOP;
			} else {
				bus->mmio->TWI_IER = TWI_IER_TXCOMP;
				bus->mmio->TWI_IDR = TWI_IDR_NACK | TWI_IDR_RXRDY;
			}
		}
		*bus->buf++ = bus->mmio->TWI_RHR;
	}
        return (tsk_wkn);
}

/**
 * i2c_dma_read_hndlr
 */
static BaseType_t i2c_dma_read_hndlr(i2cbus bus)
{
	BaseType_t tsk_wkn = pdFALSE;
        int8_t msg;

	unsigned int sr = bus->mmio->TWI_SR;
	bus->mmio->TWI_PTCR = TWI_PTCR_RXTDIS;
	if (sr & TWI_SR_NACK) {
		bus->mmio->TWI_IDR = ~0;
		msg = ENACK;
                xQueueSendFromISR(bus->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
        if (sr & TWI_SR_ENDRX) {
        	bus->hndlr = i2c_read_hndlr;
		bus->mmio->TWI_IDR = TWI_IDR_ENDRX;
		bus->mmio->TWI_IER = TWI_IER_RXRDY;
	}
        return (tsk_wkn);
}

/**
 * i2c_write
 */
int i2c_write(i2cbus bus, enum i2c_mode mode, int adr, uint8_t *p_buf, int size, boolean_t dma, ...)
{
	va_list ap;
	int iadr = 0;
	int8_t msg;

	if (mode != I2C_MODE_7BIT_ADR && mode != I2C_MODE_10BIT_ADR) {
		va_start(ap, dma);
		iadr = va_arg(ap, int);
		va_end(ap);
		if (mode == I2C_MODE_7BIT_ADR_IADR1 || mode == I2C_MODE_10BIT_ADR_IADR1) {
			if (iadr & 0xFFFFFF00) {
				return (-EADDR);
			}
		} else if (mode == I2C_MODE_7BIT_ADR_IADR2 || mode == I2C_MODE_10BIT_ADR_IADR2) {
			if (iadr & 0xFFFF0000) {
				return (-EADDR);
			}
		} else {
			if (iadr & 0xFF000000) {
				return (-EADDR);
			}
		}
	}
	if (size < 1) {
		crit_err_exit(BAD_PARAMETER);
	}
	if (bus->mtx != NULL) {
		xSemaphoreTake(bus->mtx, portMAX_DELAY);
	}
        enable_periph_clk(bus->id);
	if (bus->ini == 0 || bus->ini == 2) {
		bus->ini = 1;
		bus->mmio->TWI_CR = TWI_CR_MSDIS;
		bus->mmio->TWI_CR = TWI_CR_SVDIS;
		bus->mmio->TWI_CR = TWI_CR_MSEN;
	}
	if (mode < I2C_MODE_10BIT_ADR) {
		bus->mmio->TWI_MMR = TWI_MMR_DADR(adr) | mode << 8;
		if (mode > I2C_MODE_7BIT_ADR) {
			bus->mmio->TWI_IADR = iadr;
		}
	} else {
		int a = 0x78;
		a |= adr >> 8;
                bus->mmio->TWI_MMR = TWI_MMR_DADR(a) | (mode - 3) << 8;
		if (mode == I2C_MODE_10BIT_ADR) {
			iadr = adr & 0xFF;
		} else if (mode == I2C_MODE_10BIT_ADR_IADR1) {
			iadr |= (adr & 0xFF) << 8;
		} else {
			iadr |= (adr & 0xFF) << 16;
		}
		bus->mmio->TWI_IADR = iadr;
	}
        taskENTER_CRITICAL();
	bus->mmio->TWI_SR;
	if (bus->dma && dma && size >= 3) {
		bus->hndlr = i2c_dma_write_hndlr;
		bus->cnt = 1;
		bus->buf = p_buf + size - 1;
		bus->mmio->TWI_TPR = (unsigned int) p_buf;
                bus->mmio->TWI_TCR = size - 1;
                bus->mmio->TWI_TNCR = 0;
                bus->mmio->TWI_PTCR = TWI_PTCR_TXTEN;
		barrier();
		bus->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_ENDTX;
	} else {
		bus->hndlr = i2c_write_hndlr;
		bus->mmio->TWI_THR = *p_buf;
		if (size == 1) {
			bus->cnt = 0;
			bus->mmio->TWI_CR = TWI_CR_STOP;
			barrier();
			bus->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_TXCOMP;
		} else {
			bus->cnt = size - 1;
			bus->buf = p_buf + 1;
			barrier();
			bus->mmio->TWI_IER = TWI_IER_NACK | TWI_IER_TXRDY;
		}
	}
        taskEXIT_CRITICAL();
        if (pdFALSE == xQueueReceive(bus->sig_que, &msg, ms_to_os_ticks(WAIT_INTR_MS))) {
		bus->mmio->TWI_IDR = ~0;
                bus->mmio->TWI_CR = TWI_CR_SWRST;
		bus->mmio->TWI_CR = TWI_CR_MSDIS;
		bus->mmio->TWI_CR = TWI_CR_SVDIS;
		bus->mmio->TWI_SR;
		set_cwgr(bus);
		xQueueReceive(bus->sig_que, &msg, 0);
		bus->ini = 0;
		bus->stats.intr_tmo_err_cnt++;
		msg = EHW;
	}
	if (msg == 0) {
		bus->stats.tx_bytes_cnt += size;
	}
        disable_periph_clk(bus->id);
	if (bus->mtx != NULL) {
		xSemaphoreGive(bus->mtx);
	}
	return (-msg);
}

/**
 * get_i2cbus_by_per_id
 */
i2cbus get_i2cbus_by_per_id(int per_id)
{
	switch (per_id) {
	case ID_TWI0 :
		if (!i2c0) {
			crit_err_exit(BAD_PARAMETER);
		}
		return (i2c0);
#ifdef ID_TWI1
	case ID_TWI1 :
		if (!i2c1) {
			crit_err_exit(BAD_PARAMETER);
		}
		return (i2c1);
#endif
#ifdef ID_TWI2
	case ID_TWI2 :
		if (!i2c2) {
			crit_err_exit(BAD_PARAMETER);
		}
		return (i2c2);
#endif
	default :
		crit_err_exit(BAD_PARAMETER);
	}
	return (NULL);
}

/**
 * i2c_write_hndlr
 */
static BaseType_t i2c_write_hndlr(i2cbus bus)
{
	BaseType_t tsk_wkn = pdFALSE;
        int8_t msg;

	unsigned int sr = bus->mmio->TWI_SR;
	if (sr & TWI_SR_NACK) {
		bus->mmio->TWI_IDR = ~0;
		msg = ENACK;
                xQueueSendFromISR(bus->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
	if (sr & TWI_SR_TXCOMP) {
		bus->mmio->TWI_IDR = ~0;
		msg = 0;
		xQueueSendFromISR(bus->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
	if (sr & TWI_SR_TXRDY) {
		bus->mmio->TWI_THR = *bus->buf++;
		if (--bus->cnt == 0) {
			bus->mmio->TWI_CR = TWI_CR_STOP;
			bus->mmio->TWI_IER = TWI_IER_TXCOMP;
			bus->mmio->TWI_IDR = TWI_IDR_TXRDY;
		}
	}
        return (tsk_wkn);
}

/**
 * i2c_dma_write_hndlr
 */
static BaseType_t i2c_dma_write_hndlr(i2cbus bus)
{
	BaseType_t tsk_wkn = pdFALSE;
        int8_t msg;

	unsigned int sr = bus->mmio->TWI_SR;
	bus->mmio->TWI_PTCR = TWI_PTCR_TXTDIS;
	if (sr & TWI_SR_NACK) {
		bus->mmio->TWI_IDR = ~0;
		msg = ENACK;
                xQueueSendFromISR(bus->sig_que, &msg, &tsk_wkn);
                return (tsk_wkn);
	}
        if (sr & TWI_SR_ENDTX) {
        	bus->hndlr = i2c_write_hndlr;
		bus->mmio->TWI_IDR = TWI_IDR_ENDTX;
		bus->mmio->TWI_IER = TWI_IER_TXRDY;
	}
        return (tsk_wkn);
}

/**
 * i2c_empty_hndlr
 */
static BaseType_t i2c_empty_hndlr(i2cbus bus)
{
	bus->mmio->TWI_IDR = ~0;
	bus->mmio->TWI_SR;
        return (pdFALSE);
}

/**
 * busid2irqn
 */
static IRQn_Type busid2irqn(int per_id)
{
	switch (per_id) {
	case ID_TWI0 :
		return (TWI0_IRQn);
#ifdef ID_TWI1
	case ID_TWI1 :
		return (TWI1_IRQn);
#endif
#ifdef ID_TWI2
	case ID_TWI2 :
		return (TWI2_IRQn);
#endif
	default :
		crit_err_exit(BAD_PARAMETER);
	}
	return (0);
}

/**
 * udiv_ceil
 */
static inline uint32_t udiv_ceil(uint32_t a, uint32_t b)
{
    return (a + b - 1) / b;
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

#if TERMOUT == 1
/**
 * log_i2c_stats
 */
void log_i2c_stats(i2cbus bus)
{
	UBaseType_t pr;

	pr = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
        msg(INF, "i2c.c: bus=%s cnt: rx_bytes=%u tx_bytes=%u\n", bus->nm, bus->stats.rx_bytes_cnt, bus->stats.tx_bytes_cnt);
        msg(INF, "i2c.c: bus=%s cnt: intr_tmo_err=%u\n", bus->nm, bus->stats.intr_tmo_err_cnt);
	vTaskPrioritySet(NULL, pr);
}

/**
 * log_i2c_waveform
 */
void log_i2c_waveform(i2cbus bus)
{
	uint32_t cwgr = bus->cwgr_reg;
	uint32_t cldiv = (cwgr >> 0) & 0xFF;
	uint32_t chdiv = (cwgr >> 8) & 0xFF;
	uint32_t ckdiv = (cwgr >> 16) & 0x7;
	uint32_t mck = F_MCK;
	uint32_t scale = 1 << ckdiv;
	uint32_t tlow_cycles = cldiv * scale + 4;
	uint32_t thigh_cycles = chdiv * scale + 4;
	uint32_t period_cycles = tlow_cycles + thigh_cycles;
	if (mck == 0 || period_cycles == 0) {
		msg(INF, "i2c.c: bus=%s CWGR=0x%08lx (invalid: mck=%lu, period_cycles=%lu)\n",
		    bus->nm, (unsigned long) cwgr, (unsigned long) mck, (unsigned long) period_cycles);
		return;
	}
	uint64_t freq_hz_x1000 = ((uint64_t) mck * 1000 + (uint64_t) period_cycles / 2) / (uint64_t) period_cycles;
	uint32_t freq_hz_i = freq_hz_x1000 / 1000;
	uint32_t freq_hz_f = freq_hz_x1000 % 1000;
	uint64_t tlow_us_x1000  = ((uint64_t) tlow_cycles  * 1000000000 + (uint64_t) mck / 2) / (uint64_t) mck;
	uint64_t thigh_us_x1000 = ((uint64_t) thigh_cycles * 1000000000 + (uint64_t) mck / 2) / (uint64_t) mck;
	uint64_t t_us_x1000 = tlow_us_x1000 + thigh_us_x1000;
	msg(INF, "i2c.c: bus=%s CWGR=0x%08lx (CLDIV=%lu CHDIV=%lu CKDIV=%lu)\n",
	    bus->nm, (unsigned long) cwgr, (unsigned long) cldiv, (unsigned long) chdiv, (unsigned long) ckdiv);
	msg(INF, "i2c.c: bus=%s SCL=%lu.%03lu Hz req=%d Hz\n",
	    bus->nm, (unsigned long) freq_hz_i, (unsigned long) freq_hz_f, bus->clk_hz);
	msg(INF, "i2c.c: bus=%s tLOW=%lu.%03lu us tHIGH=%lu.%03lu us T=%lu.%03lu us\n",
	    bus->nm, (unsigned long) (tlow_us_x1000 / 1000),  (unsigned long) (tlow_us_x1000 % 1000),
	    (unsigned long) (thigh_us_x1000 / 1000), (unsigned long) (thigh_us_x1000 % 1000),
	    (unsigned long) (t_us_x1000 / 1000), (unsigned long) (t_us_x1000 % 1000));
}
#endif

#endif

/*
 * spi.h
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

#ifndef SPI_H
#define SPI_H

#ifndef SPIM
 #define SPIM 0
#endif

#if SPIM == 1

enum spim_csel_num {
	SPIM_CSEL0,
        SPIM_CSEL1,
        SPIM_CSEL2,
        SPIM_CSEL3
};

enum spim_bits {
	SPIM_8_BIT_TRANS,
	SPIM_9_BIT_TRANS,
        SPIM_10_BIT_TRANS,
	SPIM_11_BIT_TRANS,
	SPIM_12_BIT_TRANS,
	SPIM_13_BIT_TRANS,
	SPIM_14_BIT_TRANS,
	SPIM_15_BIT_TRANS,
	SPIM_16_BIT_TRANS
};

struct spim_stats {
	int tx_start_err : 1;
        int tx_end_err : 1;
        int mr_cfg_err : 1;
	int dma_err : 1;
        int rdrf_err : 1;
        int intr_err : 1;
	int poll_err : 1;
#if SPIM_CSEL_LINE_ERR == 1
	int csel_err : 1;
#endif
        unsigned int trans;
        unsigned int intr;
};

typedef struct spim_dsc *spim;
typedef struct spim_csel_dcs *spim_csel;

struct spim_dsc {
	int id;  // <SetIt>
        SemaphoreHandle_t mtx; // <SetIt> Mutex or NULL.
	int dlybcs; // <SetIt> Delay between chipselects (spim_dlybcs()).
	void *mmio;
        SemaphoreHandle_t sig;
        spim_csel act_csel;
	struct spim_stats stats;
};

#define spim_dlybcs_ns(dly) (((dly) * (F_MCK / 1000000)) / 1000)
#define spim_dlybcs_us(dly) ((dly) * (F_MCK / 1000000))

#define spim_dlybct_ns(dly) (((dly) * (F_MCK / 1000000)) / 32000)
#define spim_dlybct_us(dly) (((dly) * (F_MCK / 1000000)) / 32)

#define spim_dlybs_ns(dly) (((dly) * (F_MCK / 1000000)) / 1000)
#define spim_dlybs_us(dly) ((dly) * (F_MCK / 1000000))

#define spim_scbr_mhz(clk) (F_MCK / ((clk) * 1000000))
#define spim_scbr_hz(clk) (F_MCK / (clk))

struct spim_csel_dcs {
	// <SetIt> TRUE - initialize driver or CSR setting changed.
	boolean_t ini;
        // <SetIt> SPI mode (0 - 3).
	int mode;
        // <SetIt> Chip select line number (enum spim_csel_num).
	int csn;
        // <SetIt> Delay between consecutive transfers (spim_dlybct()).
	int dlybct;
        // <SetIt> Delay before serial clock start (spim_dlybs()).
	int dlybs;
        // <SetIt> Serial clock baud rate (spim_scbr()).
        int scbr;
        // <SetIt> Bits per transfer (enum spim_bits).
	int bits;
        // <SetIt> Chip select line rises between consecutive transfers.
	boolean_t csrise;
        // <SetIt> TRUE - interrupt, FALSE - polling in non DMA mode.
	boolean_t no_dma_intr;
	// <SetIt> TRUE - chip select line is driven externally (SPI mutex not used).
	boolean_t csel_ext;
#if SPIM_CSEL_LINE_ERR == 1
	// <SetIt> Chip select io line.
	unsigned int csel_pin;
        // <SetIt> Chip select io controller.
        void *csel_cont;
#endif
	int bufn;
	boolean_t dma;
        void *buf0;
	int size0;
        void *buf1;
	int size1;
        unsigned int stats_trans;
	unsigned int csr;
};

/**
 * init_spim
 *
 * Configure SPI master instance.
 *
 * @spi: SPI master instance.
 */
void init_spim(spim spi);

/**
 * spi_trans
 *
 * Transfer two buffers via SPI. Buffer is sequence of bytes (transfer
 * unit is 8 bits long), or sequence of uint16_t integers (transfer unit
 * is 9-16 bits long). Caller task is blocked during data transfer, except
 * if polling is used in non DMA mode.
 *
 * @spi: SPI master instance.
 * @csel: Pointer to SPI chip select descriptor.
 * @buf0: Pointer to buffer0 (generic pointer to buffer).
 * @size0: Count of byte or uint16_t units in buffer0 to send.
 * @buf1: Pointer to buffer1 (generic pointer to buffer).
 * @size1: Count of byte or uint16_t units in buffer1 to send.
 * @dma: DMA_ON or DMA_OFF (enum boolean_t).
 *
 * Returns: 0 - success; -EHW - hardware error; -EDMA - dma error.
 */
int spi_trans(spim spi, spim_csel csel, void *buf0, int size0, void *buf1, int size1,
              boolean_t dma);

#if TERMOUT == 1
/**
 * log_spim_stats
 *
 * @spi: Pointer to SPI master instance.
 */
void log_spim_stats(spim spi);
#endif

#endif

#endif

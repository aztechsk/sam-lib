/*
 * spi.h
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

/**
 * @file spi.h
 *
 * @brief SPI master driver for Atmel/Microchip SAM SPI peripheral.
 *
 * This driver targets the SAM SPI peripheral in master mode and is written as an
 * embedded component with emphasis on efficiency. The API is synchronous
 * (blocking): the calling task is blocked until the SPI transaction completes.
 *
 * Key characteristics:
 * - Master mode only. Fixed peripheral select.
 * - Two-segment transaction model: buf0 + buf1 (current + next). In DMA mode this
 *   maps directly to the SPI PDC double-buffer registers.
 * - Transfer width 8..16 bits is supported via enum spi_bits. For 8-bit transfers
 *   buffers are sequences of uint8_t; for 9..16-bit transfers buffers are sequences
 *   of uint16_t.
 * - Optional PDC (SPI PDC) usage when dma == DMA_ON; otherwise the driver uses either
 *   interrupt-driven pumping or polling, selected by spi_csel_dcs.no_dma_intr.
 *
 * IMPORTANT behavior:
 * - Full duplex and in-place buffering: transmitted elements are overwritten by received
 *   elements in the same buffer location. If you must preserve TX data, keep a copy or
 *   transmit from a temporary buffer.
 *
 * Recommendations for correct and robust use:
 * - Always set spi_dsc members marked as "Set by caller" before calling init_spi().
 * - For each slave, configure a dedicated spi_csel_dcs. Whenever you change any of its
 *   public configuration fields, set csel->ini = TRUE so that spi_trans() recomputes and
 *   programs SPI_CSR[csn].
 * - Provide a valid SCBR value (1..255). SCBR == 0 is invalid on SAM SPI.
 * - Ensure size0 > 0. size1 may be 0.
 * - For read operations, fill the corresponding TX buffer with dummy values (commonly
 *   0xFF or 0x00) of the required length, and read back the overwritten buffer content.
 * - If a bus mutex is used (spi_dsc.mtx != NULL), it is taken only when csel->csel_ext
 *   is FALSE. If csel_ext is TRUE, the application is responsible for mutual exclusion
 *   and chip select timing (chip select is assumed to be driven externally).
 *
 * Build-time notes:
 * - The content of this header is enabled only when SPIBUS == 1.
 * - If SPI_CSEL_LINE_ERR == 1, optional chip select GPIO line checks are enabled and
 *   require spi_csel_dcs.csel_pin/csel_cont to be set when csel_ext is FALSE.
 *
 * Timing helpers:
 * - spi_dlybcs_*(), spi_dlybs_*(), spi_dlybct_*() macros compute register field values
 *   from time units using F_MCK (master clock frequency, in Hz).
 * - spi_scbr_*() macros compute SCBR from requested SCK rate; *_ceil variants round up
 *   to ensure SCK does not exceed the requested frequency.
 */

#ifndef SPI_H
#define SPI_H

#ifndef SPIBUS
 #define SPIBUS 0
#endif

#ifndef SPI_HAL_IMPL
 #define SPI_HAL_IMPL 0
#endif

#if SPIBUS == 1

/**
 * @enum spi_csel_num.
 *
 * @brief SPI chip select number (maps to SPI_CSR[0..3] and NPCS lines).
 */
enum spi_csel_num {
	SPI_CSEL0,
	SPI_CSEL1,
	SPI_CSEL2,
	SPI_CSEL3
};

/**
 * @enum spi_bits.
 *
 * @brief Bits per transfer selection for the SAM SPI CSR BITS field (8..16 bits).
 */
enum spi_bits {
	SPI_8_BIT_TRANS,
	SPI_9_BIT_TRANS,
	SPI_10_BIT_TRANS,
	SPI_11_BIT_TRANS,
	SPI_12_BIT_TRANS,
	SPI_13_BIT_TRANS,
	SPI_14_BIT_TRANS,
	SPI_15_BIT_TRANS,
	SPI_16_BIT_TRANS
};

/**
 * @struct spi_stats.
 *
 * @brief Runtime counters and sticky error flags collected by the SPI driver.
 */
struct spi_stats {
	int tx_start_err : 1;
	int tx_end_err : 1;
	int mr_cfg_err : 1;
	int dma_err : 1;
	int rdrf_err : 1;
	int intr_err : 1;
	int poll_err : 1;
#if SPI_CSEL_LINE_ERR == 1
	int csel_err : 1;
#endif
	unsigned int trans;
	unsigned int intr;
};

typedef struct spi_dsc *spibus;
typedef struct spi_csel_dcs *spi_csel;

/**
 * @struct spi_dsc.
 *
 * @brief SPI master instance descriptor (one instance per SPI peripheral).
 *
 * The application owns the storage of this structure. Only members documented as
 * "Set by caller" are considered public configuration inputs. All other members are
 * maintained by the driver at runtime.
 */
struct spi_dsc {
	int id;			/**< Set by caller: Peripheral ID (e.g., ID_SPI / ID_SPI0 / ID_SPI1). */
	SemaphoreHandle_t mtx;	/**< Set by caller: Optional mutex protecting the SPI bus, or NULL. */
	int dlybcs;		/**< Set by caller: SPI_MR.DLYBCS field value (use spi_dlybcs_ns/us helpers). */
	const char *nm;
	void *mmio;
	SemaphoreHandle_t sig;
	spi_csel act_csel;
	struct spi_stats stats;
};

/**
 * @name Timing helper macros (register field helpers)
 * @{
 */
/** Compute SPI_MR.DLYBCS value from nanoseconds using F_MCK. */
#define spi_dlybcs_ns(dly) (((dly) * (F_MCK / 1000000)) / 1000)
/** Compute SPI_MR.DLYBCS value from microseconds using F_MCK. */
#define spi_dlybcs_us(dly) ((dly) * (F_MCK / 1000000))

/** Compute SPI_CSR.DLYBCT value from nanoseconds using F_MCK. */
#define spi_dlybct_ns(dly) (((dly) * (F_MCK / 1000000)) / 32000)
/** Compute SPI_CSR.DLYBCT value from microseconds using F_MCK. */
#define spi_dlybct_us(dly) (((dly) * (F_MCK / 1000000)) / 32)

/** Compute SPI_CSR.DLYBS value from nanoseconds using F_MCK. */
#define spi_dlybs_ns(dly) (((dly) * (F_MCK / 1000000)) / 1000)
/** Compute SPI_CSR.DLYBS value from microseconds using F_MCK. */
#define spi_dlybs_us(dly) ((dly) * (F_MCK / 1000000))

/** Compute SCBR from requested SCK frequency in MHz (integer division).
 *  Note: this rounds SCBR down, therefore the resulting SCK may be higher than requested. */
#define spi_scbr_mhz(clk) (F_MCK / ((clk) * 1000000U))
/** Compute SCBR from requested SCK frequency in Hz (integer division).
 *  Note: this rounds SCBR down, therefore the resulting SCK may be higher than requested. */
#define spi_scbr_hz(clk) (F_MCK / (clk))
/** Compute SCBR from requested SCK frequency in MHz (ceil division).
 *  The resulting SCK will be less than or equal to the requested frequency. */
#define spi_scbr_mhz_ceil(clk) (((F_MCK) + ((clk) * 1000000U) - 1) / ((clk) * 1000000U))
/** Compute SCBR from requested SCK frequency in Hz (ceil division).
 *  The resulting SCK will be less than or equal to the requested frequency. */
#define spi_scbr_hz_ceil(clk) (((F_MCK) + (clk) - 1) / (clk))
/** @} */

/**
 * @struct spi_csel_dcs.
 *
 * @brief Per-slave (chip select) configuration and runtime state.
 *
 * The application typically creates one instance per SPI slave device. Only members
 * documented as "Set by caller" are public configuration inputs. The driver uses
 * internal members to track transfer progress, selected mode (DMA/non-DMA), and
 * cached CSR value.
 */
struct spi_csel_dcs {
	/**< Set by caller: TRUE to (re)initialize CSR from current settings; cleared by driver. */
	boolean_t ini;
        /**< Set by caller: SPI mode 0..3. */
	int mode;
        /**< Set by caller: Chip select number (enum spi_csel_num). */
	int csn;
        /**< Set by caller: SPI_CSR.DLYBCT field value (use spi_dlybct_ns/us helpers). */
	int dlybct;
        /**< Set by caller: SPI_CSR.DLYBS field value (use spi_dlybs_ns/us helpers). */
	int dlybs;
        /**< Set by caller: SPI_CSR.SCBR field value (use spi_scbr_* helpers; valid range 1..255). */
	int scbr;
        /**< Set by caller: Bits per transfer (enum spi_bits). */
	int bits;
        /**< Set by caller: TRUE -> CS rises between consecutive transfers (CSNAAT); FALSE -> keep asserted (CSAAT). */
	boolean_t csrise;
        /**< Set by caller: Non-DMA mode selection: TRUE -> interrupt-driven, FALSE -> polling. */
	boolean_t no_dma_intr;
	/**< Set by caller: TRUE -> chip select driven externally (SPI mutex not used). */
	boolean_t csel_ext;
#if SPI_CSEL_LINE_ERR == 1
	/**< Set by caller: Chip select GPIO pin mask (PIO controller specific). */
	unsigned int csel_pin;
        /**< Set by caller: Chip select GPIO controller base address (PIO instance). */
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
 * @brief Configure a SAM SPI instance for master mode operation.
 *
 * This function binds @p bus to the selected SPI instance (SPI/SPI0/SPI1), creates
 * an internal binary semaphore used for transfer completion signaling, configures
 * SPI_MR (master mode, MODFDIS, DLYBCS), and enables the corresponding SPI IRQ in NVIC.
 *
 * The peripheral clock is enabled only during initialization; spi_trans() enables and
 * disables the peripheral clock around each transaction.
 *
 * Preconditions:
 * - bus->id, bus->mtx, and bus->dlybcs must be set by the caller.
 * - bus->sig must be NULL (it is created by the driver).
 *
 * @param bus SPI master instance descriptor.
 */
void init_spi(spibus bus);

/**
 * @brief Transfer up to two buffer segments over SPI (buf0 + buf1).
 *
 * The transaction is full-duplex and in-place: transmitted elements are overwritten by
 * the received elements in the same buffer positions.
 *
 * Buffer format depends on transfer width:
 * - If csel->bits == SPI_8_BIT_TRANS: buffers are sequences of uint8_t and sizes are
 *   counts of bytes.
 * - If csel->bits is SPI_9_BIT_TRANS .. SPI_16_BIT_TRANS: buffers are sequences of
 *   uint16_t and sizes are counts of 16-bit transfer units.
 *
 * Mode selection:
 * - If @p dma == DMA_ON, the SPI PDC is used (double-buffer: current + next).
 * - If @p dma == DMA_OFF and csel->no_dma_intr == TRUE, the transfer is interrupt-driven.
 * - If @p dma == DMA_OFF and csel->no_dma_intr == FALSE, the transfer uses polling.
 *
 * Concurrency:
 * - If bus->mtx != NULL and csel->csel_ext == FALSE, the mutex is taken for the duration
 *   of the transaction.
 * - If csel->csel_ext == TRUE, no mutex is taken; the application must ensure exclusive
 *   access and handle chip select externally.
 *
 * Side effects:
 * - If csel->ini is TRUE, spi_trans() recomputes and programs SPI_CSR[csn], caches it
 *   in csel->csr, and clears csel->ini to FALSE.
 *
 * @param bus  SPI master instance descriptor.
 * @param csel Pointer to chip select descriptor (per-slave configuration).
 * @param buf0 Pointer to first buffer segment (TX, overwritten by RX).
 * @param size0 Number of transfer units in buf0; must be > 0.
 * @param buf1 Pointer to second buffer segment (TX, overwritten by RX); may be NULL if size1 == 0.
 * @param size1 Number of transfer units in buf1; may be 0.
 * @param dma  DMA_ON or DMA_OFF (boolean_t).
 *
 * @return 0 on success; -EHW on hardware/transfer error; -EDMA on PDC timeout or PDC consistency error.
 */
int spi_trans(spibus bus, spi_csel csel, void *buf0, int size0, void *buf1, int size1, boolean_t dma);

/**
 * @brief Lookup SPI master device descriptor by peripheral ID.
 *
 * The descriptor is available after init_spi() was called for the given peripheral.
 *
 * @param per_id Peripheral ID (e.g., ID_SPI / ID_SPI0 / ID_SPI1).
 * @return SPI master instance descriptor.
 */
spibus get_spi_by_per_id(int per_id);

/**
 * @brief Lookup SPI master device descriptor by device ID.
 *
 * Device ID is a logical index used by the platform configuration (e.g., 0 -> SPI0, 1 -> SPI1).
 * The descriptor is available after init_spi() was called for the given device.
 *
 * @param dev_id Device ID number.
 * @return SPI master instance descriptor.
 */
spibus get_spi_by_dev_id(int dev_id);

#if TERMOUT == 1
/**
 * @brief Log SPI driver statistics for a given bus (terminal output).
 *
 * @param bus SPI master instance descriptor.
 */
void log_spi_stats(spibus bus);
#endif

#endif

#endif

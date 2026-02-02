/*
 * hsmci_sd.h
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
 * @file hsmci_sd.h
 * @brief Low-level HSMCI SD-card driver for SAM (HSMCI SD mode).
 *
 * This header exposes a minimal, blocking interface for SD-card access
 * over the SAM HSMCI peripheral. It is intended to be used by a higher
 * level SD card stack (card identification, partitioning, filesystem).
 *
 * Design assumptions and limitations:
 *  - SD only: MMC and SDIO cards are not supported.
 *  - Controller: SAM HSMCI, slot A, 1 or 4 bit data bus selected at
 *    compile time via HSMCI_SD_DLINE_NUM.
 *  - Block size is fixed to 512 bytes; all transfers are in 512-byte
 *    units. SDSC (byte-addressed) cards are not supported by this driver.
 *  - Addressing in the API uses LBA (unit: 512-byte block) as for
 *    SDHC/SDXC devices.
 *  - Single-instance, non-reentrant driver: only one command or data
 *    transfer may be active at any time. Callers running in multiple
 *    tasks must provide external serialization (e.g. a mutex).
 *  - All functions are strictly blocking and wait until the command or
 *    data transfer completes or an error/timeout occurs.
 *  - Data buffers must reside in memory accessible by the HSMCI PDC/DMA
 *    engine and are expected to be 32-bit aligned for optimal access.
 *  - The implementation relies on the 16-bit PDC transfer counters; very
 *    large multi-block transfers should keep block_cnt within the range
 *    that fits into the underlying HSMCI/PDC counters.
 *
 * Error handling:
 *  - Most functions return 0 on success and -EHW on hardware/transfer
 *    error. Some configuration errors result in crit_err_exit(), since
 *    this is an embedded, low-overhead driver and the caller is assumed
 *    to pass valid parameters.
 */

#ifndef HSMCI_SD_H
#define HSMCI_SD_H

#ifndef HSMCI_SD
 #define HSMCI_SD 0
#endif

#if HSMCI_SD == 1

enum hsmci_bus_width {
	HSMCI_BUS_WIDTH_1,
	HSMCI_BUS_WIDTH_4
};

/**
 * @brief HSMCI command response container.
 *
 * Short (48-bit) responses (R1/R1b/R3/R6/R7) are exposed via @ref r1,
 * which contains the 32-bit value read from HSMCI_RSPR[0] as provided
 * by the controller.
 * Long (136-bit) responses (R2 – CID/CSD) are stored in @ref r2 and
 * shall be interpreted as a raw 16-byte big-endian buffer
 * ((uint8_t *)&resp->r2[0]). The order of bytes corresponds to the
 * wire order defined in the SD specification (MSB first); it does
 * not match the native 32-bit word order of the hardware registers.
 *
 * Access the fields of R2 responses as bytes rather than words and do
 * not rely on host endianness when using @ref r2 as an array of
 * uint32_t.
 */
typedef union {
	uint32_t r1;
	uint32_t r2[4];
} hsmci_resp_t;

/**
 * @brief Initialize HSMCI peripheral and set base parameters.
 *
 * This function:
 *  - enables peripheral clock for HSMCI,
 *  - configures HSMCI for SD mode with 1-bit data bus,
 *  - sets safe initialization clock (around 400 kHz),
 *  - allocates and initializes internal driver structures (queue, ISR),
 *  - performs software reset of HSMCI,
 *  - initializes internal driver state and interrupt controller.
 *
 * It is expected to be called once at startup, before any other hsmci_*
 * function. It is not reentrant.
 *
 * Returns: nothing; on unrecoverable configuration error calls crit_err_exit().
 */
void init_hsmci(void);

/**
 * @brief Perform software reset of HSMCI preserving configuration.
 *
 * Can be called by higher layers (e.g. SD-card logic) to recover the
 * interface after a fault. Saves and restores DTOR, CSTOR, CFG, MR and SDCR,
 * re-enables the controller and power-save mode.
 *
 * Equivalent to power-cycling the HSMCI block.
 */
void hsmci_soft_reset(void);

/**
 * @brief Set HSMCI card clock frequency.
 *
 * Caller typically uses low frequency (about 400 kHz) during card
 * identification and switches to higher transfer frequency (for example
 * 25 MHz or 50 MHz) after the card is in transfer state and capabilities
 * are known.
 *
 * The effective SD card clock is derived from the master clock F_MCK
 * using the HSMCI divider:
 *
 *   f_card = F_MCK / (2 * (CLKDIV + 1))
 *
 * Therefore the resulting frequency is quantized and may differ from
 * the requested value @p clock_hz. The selection policy is controlled
 * by the param overclk:
 *  - FALSE: the driver selects the highest available frequency that does
 *           not exceed @p clock_hz;
 *  - TRUE:  the driver selects the closest available frequency, which may
 *           be slightly higher than @p clock_hz.
 *
 * @param clock_hz  Requested card clock frequency in Hz.
 * @param clock_hz_set Actual clock frequency configured in Hz (return).
 * @param overclk Overclock policy.
 */
void hsmci_set_clock(unsigned int clock_hz, unsigned int *clock_hz_set,
		     boolean_t overclk);

/**
 * @brief Set active data bus width (1-bit or 4-bit).
 *
 * @param bw  Bus width: HSMCI_BUS_WIDTH_1 or HSMCI_BUS_WIDTH_4.
 *
 * Switches the HSMCI data-bus configuration between 1 and 4 bit modes.
 * This should only be invoked after a successful ACMD6 (SET_BUS_WIDTH)
 * command when communicating with SD cards. The function validates
 * the requested width against hardware capabilities (compile-time constant
 * @ref HSMCI_SD_DLINE_NUM) and performs a runtime check;
 * invalid requests trigger crit_err_exit().
 */
void hsmci_set_bus_width(enum hsmci_bus_width bw);

/**
 * @brief Enable high-speed timing mode.
 *
 * Configures the HSMCI to use high-speed timing for SD transfers
 * (typically for card clock frequencies above 25 MHz). The caller must
 * ensure that the card itself has been switched to high-speed mode (CMD6).
 */
void hsmci_enable_hspeed(void);

/**
 * @brief Disable high-speed timing mode.
 */
void hsmci_disable_hspeed(void);

/**
 * @brief Send initial 74 clock cycles.
 *
 * It is required after card insertion and before starting the card
 * initialization sequence.
 *
 * Returns: 0 - success; -EHW on error.
 */
int hsmci_send_clock(void);

/**
  * @brief Set R1b busy timeout for the next hsmci_*() call with busy semantic.
  *
  * Intended for long-busy operations (e.g. SD ERASE / CMD38).
  *
  * @param tmo_ms Timeout in milliseconds.
  */
void hsmci_set_next_r1b_busy_tmo_ms(unsigned int tmo_ms);

/**
 * @brief Send a command to the card and receive the response.
 *
 * Command definition encodes command index and response type
 * (see hsmci_cmd.h). Busy handling for R1b commands is done
 * inside this function.
 *
 * @param cmd  Command definition.
 * @param arg  Command argument (command specific).
 * @param resp Pointer to response container or NULL if caller does not care.
 *        For short (48-bit) responses, resp->r1 is filled with the 32-bit
 *        value from HSMCI_RSPR[0]. For long (136-bit) responses, resp->r2
 *        contains the 16 raw response bytes in big-endian order
 *        (see @ref hsmci_resp_t).
 *
 * Returns: 0 - success; -EHW on error.
 */
int hsmci_send_cmd(unsigned int cmd, unsigned int arg, hsmci_resp_t *resp);

/**
 * @brief Send a command with single-block data read transfer.
 *
 * This function sends an SD card command that transfers a single data block
 * using the HSMCI PDC engine.
 *
 * Data length must be non-zero, a multiple of 4 bytes and must not exceed
 * 512 bytes (maximum HSMCI block size).
 * Caller must ensure that @p buf points to RAM accessible by the HSMCI
 * PDC engine and is suitably aligned (typically 32-bit aligned) for the
 * target MCU; no runtime checks are performed.
 * The transfer is always a single-block read; multi-block transfers or
 * write transfers are not supported by this helper and must use the
 * dedicated block read/write API.
 *
 * The function is blocking.
 *
 * @param cmd  Command definition.
 * @param arg  Command argument (command specific).
 * @param buf  Pointer to data buffer.
 * @param len  Data length in bytes.
 * @param resp Pointer to response container or NULL if caller does not care.
 *        For short (48-bit) responses, resp->r1 is filled with the 32-bit
 *        value from HSMCI_RSPR[0]. For long (136-bit) responses, resp->r2
 *        contains the 16 raw response bytes in big-endian order
 *        (see @ref hsmci_resp_t).
 *
 * Returns: 0 - success; -EHW on error.
 */
int hsmci_send_data_cmd(unsigned int cmd, unsigned int arg, void *buf, size_t len,
                        hsmci_resp_t *resp);

/**
 * @brief Read one or more 512-byte blocks from the card.
 *
 * This function is blocking.
 *
 * Driver chooses appropriate SD command:
 *  - for block_cnt == 1 it uses single-block read (CMD17),
 *  - for block_cnt > 1 it uses multi-block read (CMD18) with STOP.
 *
 * Address argument is the logical block address (LBA, unit: 512-byte block)
 * as used by SDHC/SDXC cards. SDSC (byte-addressed) cards are not supported
 * by this low-level driver.
 * The implementation relies on the 16-bit HSMCI/PDC transfer counters;
 * the caller should choose @p block_cnt such that the resulting transfer
 * length fits into the underlying counters.
 *
 * @param lba       Logical block address (LBA, 512 B units).
 * @param block_cnt Number of 512 B blocks to read.
 * @param buf       Pointer to destination buffer. Buffer size must be at least
 *                  block_cnt * 512 bytes and must reside in RAM accessible
 *                  by the HSMCI PDC engine (typically 32-bit aligned).
 *
 * Returns: 0 - success; -EHW on error.
 */
int hsmci_read_blocks(size_t lba, int block_cnt, void *buf);

/**
 * @brief Write one or more 512-byte blocks to the card.
 *
 * This function is blocking.
 *
 * Driver chooses appropriate SD command:
 *  - for block_cnt == 1 it uses single-block write (CMD24),
 *  - for block_cnt > 1 it uses multi-block write (CMD25) with STOP.
 *
 * Address argument is the logical block address (LBA, unit: 512-byte block)
 * as used by SDHC/SDXC cards. SDSC (byte-addressed) cards are not supported
 * by this low-level driver.
 * The implementation relies on the 16-bit HSMCI/PDC transfer counters;
 * the caller should choose @p block_cnt such that the resulting transfer
 * length fits into the underlying counters.
 *
 * @param lba       Logical block address (LBA, 512 B units).
 * @param block_cnt Number of 512 B blocks to write.
 * @param buf       Pointer to source buffer. Buffer size must be at least
 *                  block_cnt * 512 bytes and must reside in RAM accessible
 *                  by the HSMCI PDC engine (typically 32-bit aligned).
 *
 * Returns: 0 - success; -EHW on error.
 */
int hsmci_write_blocks(size_t lba, int block_cnt, const void *buf);

#if TERMOUT == 1
/**
 * @brief Logs HSMCI driver statistics.
 *
 * Print internal HSMCI driver statistics (error flags, transfer counters).
 */
void log_hsmci_stats(void);
#endif

#endif

#endif

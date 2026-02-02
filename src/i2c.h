/*
 * i2c.h
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

/**
 * @file i2c.h
 *
 * @brief I2C (TWI) master driver API for Microchip/Atmel SAM3/SAM4 MCUs.
 *
 * This module implements a blocking I2C master transaction API on top of the
 * SAM TWI peripheral. A call to i2c_read()/i2c_write() blocks the calling task
 * until the transaction completes (success, NACK, data error, or timeout).
 *
 * Key properties:
 * - Supports 7-bit and 10-bit slave addressing.
 * - Supports optional internal address (IADR) modes (1-3 bytes for 7-bit, 0-2
 *   bytes for 10-bit, as supported by the SAM TWI hardware).
 * - Optional use of PDC/DMA (when available for the particular TWI instance).
 * - Optional mutual exclusion via user-provided FreeRTOS mutex.
 *
 * @note The driver intentionally does not validate the slave address @p adr on
 *       every call (caller responsibility). It does validate the *internal*
 *       address size in IADR modes and returns -EADDR on mismatch.
 */

#ifndef I2C_H
#define I2C_H

#ifndef I2CBUS
 #define I2CBUS 0
#endif

#if I2CBUS == 1

/**
 * @enum i2c_mode
 *
 * @brief Addressing / internal address mode for i2c_read() and i2c_write().
 *
 * This selects:
 * - Slave address width (7-bit vs 10-bit).
 * - Whether an internal address is used (IADR modes).
 * - Internal address size (1-3 bytes depending on mode).
 *
 * For IADR modes, the caller must provide one additional `int` argument
 * (var-args) holding the internal address value. The driver checks that the
 * provided value fits into the configured number of bytes.
 */
enum i2c_mode {
	/** 7-bit slave address, no internal address (IADRSZ=0). */
	I2C_MODE_7BIT_ADR,
	/** 7-bit slave address + 1-byte internal address (IADRSZ=1). Extra arg: iadr (0..0xFF). */
	I2C_MODE_7BIT_ADR_IADR1,
	/** 7-bit slave address + 2-byte internal address (IADRSZ=2). Extra arg: iadr (0..0xFFFF). */
	I2C_MODE_7BIT_ADR_IADR2,
	/** 7-bit slave address + 3-byte internal address (IADRSZ=3). Extra arg: iadr (0..0xFFFFFF). */
	I2C_MODE_7BIT_ADR_IADR3,
	/** 10-bit slave address, no additional internal address. */
	I2C_MODE_10BIT_ADR,
	/** 10-bit slave address + 1-byte internal address. Extra arg: iadr (0..0xFF). */
	I2C_MODE_10BIT_ADR_IADR1,
	/** 10-bit slave address + 2-byte internal address. Extra arg: iadr (0..0xFFFF). */
	I2C_MODE_10BIT_ADR_IADR2
};

/**
 * @typedef i2cbus
 *
 * @brief Handle to an I2C/TWI bus instance.
 */
typedef struct i2c_dsc *i2cbus;

/**
 * @struct i2c_stats
 *
 * @brief Cumulative driver statistics for one bus instance.
 */
struct i2c_stats {
	unsigned int rx_bytes_cnt;
	unsigned int tx_bytes_cnt;
	unsigned int intr_tmo_err_cnt;
};

/**
 * @struct i2c_dsc
 *
 * @brief I2C/TWI bus descriptor (driver instance state).
 *
 * The structure is public to allow static allocation and integration with the
 * rest of the system, but several fields are internal driver state.
 *
 * Required initialization by the caller (before init_i2c()):
 * - id.
 * - clk_hz.
 * - mtx (optional).
 */
struct i2c_dsc {
	/** Peripheral ID for this TWI instance (e.g., ID_TWI0 / ID_TWI1 / ID_TWI2). */
	int id; /**< Must be set by the caller before init_i2c(). */
	/** Requested SCL clock frequency in Hz (master mode). Must be > 0. */
	int clk_hz; /**< Must be set by the caller before init_i2c(). */
	/** Optional mutual exclusion primitive for locking bus. */
        SemaphoreHandle_t mtx; /**< Must be set by the caller before init_i2c(). May be NULL. */
	const char *nm;
	Twi *mmio;
        BaseType_t (*hndlr)(i2cbus);
        boolean_t dma;
	int cnt;
        uint8_t *buf;
	boolean_t ovre;
        QueueHandle_t sig_que;
	int ini;
	struct i2c_stats stats;
	unsigned int cwgr_reg;
};

/**
 * @brief Initialize one I2C/TWI bus instance.
 *
 * This function:
 * - Maps the descriptor to the correct TWI peripheral (mmio).
 * - Creates internal signaling resources (queue).
 * - Performs a peripheral reset and configures the clock waveform generator.
 * - Sets up NVIC interrupt routing and priority.
 *
 * @param[in,out] bus Bus descriptor pointer.
 *
 * @warning This function is expected to be called exactly once per bus
 *          descriptor during system initialization. Re-initializing the same
 *          descriptor without reset is not supported by design.
 *
 * @warning Must be called from task context (uses FreeRTOS primitives).
 */
void init_i2c(i2cbus bus);

/**
 * @brief Read bytes from an I2C slave device.
 *
 * Performs a blocking I2C master read transaction. The calling task is blocked
 * until the transaction completes.
 *
 * If @p mode is an internal-address mode (IADR1/IADR2/IADR3), the caller must
 * provide one additional var-arg of type `int` holding the internal address.
 *
 * @param[in,out] bus  Bus instance.
 * @param[in]     mode Addressing mode (7-bit/10-bit + optional internal address).
 * @param[in]     adr  Slave address (7-bit or 10-bit depending on @p mode).
 * @param[out]    p_buf Destination buffer for received bytes.
 * @param[in]     size Number of bytes to read (must be >= 1).
 * @param[in]     dma  Request DMA/PDC usage (DMA_ON or DMA_OFF / TRUE/FALSE depending on project).
 * @param[in]     ...  Optional internal address value (only for IADR modes).
 *
 * @return 0 on success.
 * @return -ENACK if the slave responded with NACK.
 * @return -EDATA on data error (e.g., receiver overrun detected).
 * @return -EADDR if the provided internal address does not fit the selected IADR size.
 * @return -EHW on unexpected hardware state / driver timeout recovery.
 *
 * @note The driver does not validate the slave address on every call; the caller
 *       must supply a correct @p adr for the selected @p mode.
 *
 * @note DMA/PDC may be used only if supported by the bus instance and if the
 *       transfer size is large enough (implementation-defined threshold).
 */
int i2c_read(i2cbus bus, enum i2c_mode mode, int adr, uint8_t *p_buf, int size, boolean_t dma, ...);

/**
 * @brief Write bytes to an I2C slave device.
 *
 * Performs a blocking I2C master write transaction. The calling task is blocked
 * until the transaction completes.
 *
 * If @p mode is an internal-address mode (IADR1/IADR2/IADR3), the caller must
 * provide one additional var-arg of type `int` holding the internal address.
 *
 * @param[in,out] bus  Bus instance.
 * @param[in]     mode Addressing mode (7-bit/10-bit + optional internal address).
 * @param[in]     adr  Slave address (7-bit or 10-bit depending on @p mode).
 * @param[in]     p_buf Source buffer of bytes to send.
 * @param[in]     size Number of bytes to write (must be >= 1).
 * @param[in]     dma  Request DMA/PDC usage (DMA_ON or DMA_OFF / TRUE/FALSE depending on project).
 * @param[in]     ...  Optional internal address value (only for IADR modes).
 *
 * @return 0 on success.
 * @return -ENACK if the slave responded with NACK.
 * @return -EADDR if the provided internal address does not fit the selected IADR size.
 * @return -EHW on unexpected hardware state / driver timeout recovery.
 *
 * @note DMA/PDC may be used only if supported by the bus instance and if the
 *       transfer size is large enough (implementation-defined threshold).
 */
int i2c_write(i2cbus bus, enum i2c_mode mode, int adr, uint8_t *p_buf, int size, boolean_t dma, ...);

/**
 * @brief Obtain the bus descriptor by peripheral ID.
 *
 * Convenience lookup for systems that want to map from a TWI peripheral ID
 * (e.g., ID_TWI0) to the corresponding i2cbus descriptor pointer.
 *
 * @param[in] per_id Peripheral ID.
 * @return Bus descriptor pointer previously registered via init_i2c().
 *
 * @warning If the bus for @p per_id was not initialized/registered, the
 *          implementation may terminate the system (project-specific).
 */
i2cbus get_i2cbus_by_per_id(int per_id);

#if TERMOUT == 1
/**
 * @brief Log cumulative I2C statistics for a bus.
 *
 * @param[in] bus Bus instance.
 */
void log_i2c_stats(i2cbus bus);

/**
 * @brief Log computed I2C waveform timing (SCL frequency, tLOW, tHIGH).
 *
 * Uses bus->cwgr_reg and the system peripheral clock (F_MCK in the implementation)
 * to compute the *effective* SCL frequency and timing parameters implied by
 * the CWGR register:
 * - tLOW  = ((CLDIV * 2^CKDIV) + 4) * t_peripheral_clock.
 * - tHIGH = ((CHDIV * 2^CKDIV) + 4) * t_peripheral_clock.
 *
 * @param[in] bus Bus instance.
 */
void log_i2c_waveform(i2cbus bus);
#endif

#endif

#endif

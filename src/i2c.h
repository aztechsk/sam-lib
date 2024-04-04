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

#ifndef I2C_H
#define I2C_H

#ifndef I2CM
 #define I2CM 0
#endif

#if I2CM == 1

enum i2cm_mode {
	I2CM_MODE_7BIT_ADR,
	I2CM_MODE_7BIT_ADR_IADR1,
        I2CM_MODE_7BIT_ADR_IADR2,
	I2CM_MODE_7BIT_ADR_IADR3,
	I2CM_MODE_10BIT_ADR,
	I2CM_MODE_10BIT_ADR_IADR1,
	I2CM_MODE_10BIT_ADR_IADR2
};

typedef struct i2c_dsc *i2c;

struct i2c_dsc {
	int id; // <SetIt>
	int clk_hz; // <SetIt> 0 - slave only.
        SemaphoreHandle_t mtx; // <SetIt> Mutex or NULL.
	Twi *mmio;
        BaseType_t (*hndlr)(i2c);
        boolean_t dma;
	int cnt;
        uint8_t *buf;
	boolean_t ovre;
        QueueHandle_t sig_que;
	int ini;
};

/**
 * init_i2c
 */
void init_i2c(i2c dev);

/**
 * i2cm_read
 *
 * Read bytes from i2c slave device.
 * Caller task is blocked until operation terminated.
 *
 * @dev: I2C instance.
 * @i2cm_mode: I2C address mode.
 * @adr: Slave address.
 * @p_buf: Pointer to memory for store received bytes.
 * @size: Bytes count.
 * @dma: DMA_ON or DMA_OFF (enum boolean_t).
 * @va1: Slave internal address for IADR modes.
 *
 * Returns: 0 - success; -ENACK - slave NACK; -EDATA - read data error; -EADDR - bad address;
 *          -EHW - hardware in unexpected state.
 */
int i2cm_read(i2c dev, enum i2cm_mode mode, int adr, uint8_t *p_buf, int size, boolean_t dma, ...);

/**
 * i2cm_write
 *
 * Write bytes to i2c slave device.
 * Caller task is blocked until operation terminated.
 *
 * @dev: I2C instance.
 * @i2cm_mode: I2C address mode.
 * @adr: Slave address.
 * @p_buf: Pointer to bytes to send.
 * @size: Bytes count.
 * @dma: DMA_ON or DMA_OFF (enum boolean_t).
 * @va1: Slave internal address for IADR modes.
 *
 * Returns: 0 - success; -ENACK - slave NACK; -EADDR - bad address; -EHW - hardware in unexpected state.
 */
int i2cm_write(i2c dev, enum i2cm_mode mode, int adr, uint8_t *p_buf, int size, boolean_t dma, ...);
#endif

#endif

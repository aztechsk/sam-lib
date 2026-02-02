/*
 * spi_hal_impl.c
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
#include "fmalloc.h"
#include "msgconf.h"
#include "hwerr.h"
#include "spi.h"
#include "spi_hal.h"
#include <string.h>

#if SPI_HAL_IMPL == 1

struct spi_dev {
	struct spi_csel_dcs csel;
	spibus bus;
};

static inline int spi_map_bits(enum spi_hal_bits_trans b);

/**
 * spi_hal_dev_init
 */
void spi_hal_dev_init(struct spi_hal_dev *dev)
{
	if (dev->opaque) {
		crit_err_exit(BAD_PARAMETER);
	}
	if (NULL == (dev->opaque = pvPortMalloc(sizeof(struct spi_dev)))) {
		crit_err_exit(MALLOC_ERROR);
	}
	memset(dev->opaque, 0, sizeof(struct spi_dev));
	((struct spi_dev *) dev->opaque)->bus = get_spi_by_dev_id(dev->cfg.spi_bus_id);
	((struct spi_dev *) dev->opaque)->csel.mode = dev->cfg.mode;
	((struct spi_dev *) dev->opaque)->csel.bits = spi_map_bits(dev->cfg.bits_trans);
	((struct spi_dev *) dev->opaque)->csel.dlybct = spi_dlybct_ns(dev->cfg.dly_bct_ns);
	((struct spi_dev *) dev->opaque)->csel.dlybs = spi_dlybs_ns(dev->cfg.dly_bcs_ns);
	if (dev->cfg.cs_rise) {
		((struct spi_dev *) dev->opaque)->csel.csrise = TRUE;
	} else {
		((struct spi_dev *) dev->opaque)->csel.csrise = FALSE;
	}
	((struct spi_dev *) dev->opaque)->csel.scbr = spi_scbr_hz_ceil(dev->cfg.sck_hz);
	((struct spi_dev *) dev->opaque)->csel.ini = TRUE;
	switch (dev->cfg.csel_num) {
	case SPI_HAL_CSEL0 :
		((struct spi_dev *) dev->opaque)->csel.csn = SPI_CSEL0;
		break;
        case SPI_HAL_CSEL1 :
		((struct spi_dev *) dev->opaque)->csel.csn = SPI_CSEL1;
		break;
        case SPI_HAL_CSEL2 :
		((struct spi_dev *) dev->opaque)->csel.csn = SPI_CSEL2;
		break;
        case SPI_HAL_CSEL3 :
		((struct spi_dev *) dev->opaque)->csel.csn = SPI_CSEL3;
		break;
	default :
		crit_err_exit(BAD_PARAMETER);
	}
#if SPI_CSEL_LINE_ERR == 1
	((struct spi_dev *) dev->opaque)->csel.csel_pin = dev->cfg.csel_pin;
	((struct spi_dev *) dev->opaque)->csel.csel_cont = dev->cfg.csel_cont;
#endif
}

/**
 * spi_hal_dev_cfg
 */
void spi_hal_dev_cfg(struct spi_hal_dev *dev)
{
	if (!dev->opaque) {
		crit_err_exit(BAD_PARAMETER);
	}
	((struct spi_dev *) dev->opaque)->csel.bits = spi_map_bits(dev->cfg.bits_trans);
	((struct spi_dev *) dev->opaque)->csel.dlybct = spi_dlybct_ns(dev->cfg.dly_bct_ns);
	((struct spi_dev *) dev->opaque)->csel.dlybs = spi_dlybs_ns(dev->cfg.dly_bcs_ns);
	if (dev->cfg.cs_rise) {
		((struct spi_dev *) dev->opaque)->csel.csrise = TRUE;
	} else {
		((struct spi_dev *) dev->opaque)->csel.csrise = FALSE;
	}
	((struct spi_dev *) dev->opaque)->csel.scbr = spi_scbr_hz_ceil(dev->cfg.sck_hz);
	((struct spi_dev *) dev->opaque)->csel.ini = TRUE;
}

/**
 * spi_hal_xfer
 */
int spi_hal_xfer(struct spi_hal_dev *dev, enum spi_hal_xfer_type xfer_type, void *buf0, int size0, void *buf1, int size1)
{
	boolean_t dma = FALSE;

	switch (xfer_type) {
	case SPI_HAL_POLL :
		((struct spi_dev *) dev->opaque)->csel.no_dma_intr = FALSE;
		break;
	case SPI_HAL_INTR :
		((struct spi_dev *) dev->opaque)->csel.no_dma_intr = TRUE;
		break;
	case SPI_HAL_DMA  :
		dma = TRUE;
		break;
	default :
		crit_err_exit(BAD_PARAMETER);
	}
	return (spi_trans(((struct spi_dev *) dev->opaque)->bus, &((struct spi_dev *) dev->opaque)->csel,
			  buf0, size0, buf1, size1, dma));
}

/**
 * spi_map_bits
 */
static inline int spi_map_bits(enum spi_hal_bits_trans b)
{
	switch (b) {
	case SPI_HAL_8_BIT_TRANS :
		return (SPI_8_BIT_TRANS);
	case SPI_HAL_9_BIT_TRANS :
		return (SPI_9_BIT_TRANS);
	case SPI_HAL_10_BIT_TRANS :
		return (SPI_10_BIT_TRANS);
	case SPI_HAL_11_BIT_TRANS :
		return (SPI_11_BIT_TRANS);
	case SPI_HAL_12_BIT_TRANS :
		return (SPI_12_BIT_TRANS);
	case SPI_HAL_13_BIT_TRANS :
		return (SPI_13_BIT_TRANS);
	case SPI_HAL_14_BIT_TRANS :
		return (SPI_14_BIT_TRANS);
	case SPI_HAL_15_BIT_TRANS :
		return (SPI_15_BIT_TRANS);
	case SPI_HAL_16_BIT_TRANS :
		return (SPI_16_BIT_TRANS);
	default:
		crit_err_exit(BAD_PARAMETER);
		return (SPI_8_BIT_TRANS);
	}
}
#endif

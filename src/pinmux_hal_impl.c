/*
 * pinmux_hal_impl.c
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
#include "msgconf.h"
#include "pio.h"
#include "pinmux_hal.h"

#if PINMUX_HAL_IMPL == 1

static inline void check_single_bit(uint32_t mask);

/**
 * pinmux_hal_set_func
 */
void pinmux_hal_set_func(void *ctrl, uint32_t pin_mask, enum pinmux_hal_func func)
{
	enum pio_func fn = PIO_INPUT;

	check_single_bit(pin_mask);
	switch (func) {
	case PINMUX_HAL_FUNC_GPIO_IN :
		fn = PIO_INPUT;
		break;
	case PINMUX_HAL_FUNC_GPIO_OUT :
		fn = PIO_OUTPUT;
		break;
	case PINMUX_HAL_AF0 :
		fn = PIO_PERIPH_A;
		break;
	case PINMUX_HAL_AF1 :
		fn = PIO_PERIPH_B;
		break;
	case PINMUX_HAL_AF2 :
		fn = PIO_PERIPH_C;
		break;
	case PINMUX_HAL_AF3 :
		fn = PIO_PERIPH_D;
		break;
	default :
		crit_err_exit(BAD_PARAMETER);
	}
	conf_io_pin(pin_mask, ctrl, fn, PIO_END_OF_FEAT);
}

/**
 * pinmux_hal_get_func
 */
enum pinmux_hal_func pinmux_hal_get_func(void *ctrl, uint32_t pin_mask)
{
	enum pinmux_hal_func func = 0;

	check_single_bit(pin_mask);
	enum pio_func fn = get_pio_periph_abcd(pin_mask, ctrl);
	switch (fn) {
	case PIO_INPUT :
		func = PINMUX_HAL_FUNC_GPIO_IN;
		break;
	case PIO_OUTPUT :
		func = PINMUX_HAL_FUNC_GPIO_OUT;
		break;
	case PIO_PERIPH_A :
		func = PINMUX_HAL_AF0;
		break;
	case PIO_PERIPH_B :
		func = PINMUX_HAL_AF1;
		break;
	case PIO_PERIPH_C :
		func = PINMUX_HAL_AF2;
		break;
	case PIO_PERIPH_D :
		func = PINMUX_HAL_AF3;
		break;
	}
	return (func);
}

/**
 * pinmux_hal_set_drive
 */
void pinmux_hal_set_drive(void *ctrl, uint32_t pin_mask, enum pinmux_hal_drive drive)
{
	/* Not implemented */
}

/**
 * pinmux_hal_set_slew
 */
void pinmux_hal_set_slew(void *ctrl, uint32_t pin_mask, enum pinmux_hal_slew slew)
{
	/* Not implemented */
}

/**
 * check_single_bit
 */
static inline void check_single_bit(uint32_t mask)
{
	if (!mask || (mask & (mask - 1))) {
		crit_err_exit(BAD_PARAMETER);
	}
}
#endif

/*
 * gpio_hal_impl.c
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
#include "gpio_hal.h"

#if GPIO_HAL_IMPL == 1

static inline void check_single_bit(uint32_t mask);

/**
 * gpio_hal_set_dir
 */
void gpio_hal_set_dir(void *ctrl, uint32_t pin_mask, enum gpio_hal_dir dir)
{
	check_single_bit(pin_mask);
	conf_io_pin(pin_mask, ctrl, (dir == GPIO_HAL_DIR_INPUT) ? PIO_INPUT : PIO_OUTPUT, PIO_END_OF_FEAT);
}

/**
 * gpio_hal_set_pull
 */
void gpio_hal_set_pull(void *ctrl, uint32_t pin_mask, enum gpio_hal_pull pull)
{
	check_single_bit(pin_mask);
	enum pio_func fn = get_pio_periph_abcd(pin_mask, ctrl);
	if (pull == GPIO_HAL_PULL_UP) {
		conf_io_pin(pin_mask, ctrl, fn, PIO_PULL_UP_ON, PIO_END_OF_FEAT);
	} else if (pull == GPIO_HAL_PULL_DOWN) {
		conf_io_pin(pin_mask, ctrl, fn, PIO_PULL_DOWN_ON, PIO_END_OF_FEAT);
	} else {
		conf_io_pin(pin_mask, ctrl, fn, PIO_PULL_UP_OFF, PIO_PULL_DOWN_OFF, PIO_END_OF_FEAT);
	}
}

/**
 * gpio_hal_set_drive
 */
void gpio_hal_set_drive(void *ctrl, uint32_t pin_mask, enum gpio_hal_drive drive)
{
	check_single_bit(pin_mask);
	enum pio_func fn = get_pio_periph_abcd(pin_mask, ctrl);
	conf_io_pin(pin_mask, ctrl, fn,
		    (drive == GPIO_HAL_DRIVE_OPEN_DRAIN) ? PIO_MULTI_DRIVE_ON : PIO_MULTI_DRIVE_OFF,
		    PIO_END_OF_FEAT);
}

/**
 * gpio_hal_set_schmitt
 */
void gpio_hal_set_schmitt(void *ctrl, uint32_t pin_mask, boolean_t enable)
{
	check_single_bit(pin_mask);
	enum pio_func fn = get_pio_periph_abcd(pin_mask, ctrl);
	conf_io_pin(pin_mask, ctrl, fn, (enable) ? PIO_SCHMITT_ON : PIO_SCHMITT_OFF, PIO_END_OF_FEAT);
}

/**
 * gpio_hal_set_filter
 */
void gpio_hal_set_filter(void *ctrl, uint32_t pin_mask, enum gpio_hal_filter filter)
{
	check_single_bit(pin_mask);
	enum pio_func fn = get_pio_periph_abcd(pin_mask, ctrl);
	if (filter == GPIO_HAL_FILTER_GLITCH) {
		conf_io_pin(pin_mask, ctrl, fn, PIO_GLITCH_FILTER_ON, PIO_END_OF_FEAT);
	} else if (filter == GPIO_HAL_FILTER_DEBOUNCE) {
		conf_io_pin(pin_mask, ctrl, fn, PIO_DEBOUNCE_FILTER_ON, PIO_END_OF_FEAT);
	} else {
		conf_io_pin(pin_mask, ctrl, fn, PIO_INPUT_FILTER_OFF, PIO_END_OF_FEAT);
	}
}

/**
 * gpio_hal_set_level
 */
void gpio_hal_set_level(void *ctrl, uint32_t pin_mask, enum gpio_hal_level level)
{
	set_pin_lev(pin_mask, ctrl, (level) ? TRUE : FALSE);
}

/**
 * gpio_hal_get_input
 */
enum gpio_hal_level gpio_hal_get_input(void *ctrl, uint32_t pin_mask)
{
	return ((get_pin_lev(pin_mask, ctrl)) ? GPIO_HAL_HIGH : GPIO_HAL_LOW);
}

/**
 * gpio_hal_get_output
 */
enum gpio_hal_level gpio_hal_get_output(void *ctrl, uint32_t pin_mask)
{
	return ((get_pin_out(pin_mask, ctrl)) ? GPIO_HAL_HIGH : GPIO_HAL_LOW);
}

/**
 * gpio_hal_intr_config
 */
void gpio_hal_intr_config(void *ctrl, uint32_t pin_mask, enum gpio_hal_intr trig)
{
	int cfg = 0;

	check_single_bit(pin_mask);
	enum pio_func fn = get_pio_periph_abcd(pin_mask, ctrl);
	switch (trig) {
	case GPIO_HAL_INTR_DISABLED :
		cfg = PIO_DISABLE_INTR;
		break;
	case GPIO_HAL_INTR_RISING :
		cfg = PIO_RISING_EDGE_INTR_CFG;
		break;
	case GPIO_HAL_INTR_FALLING :
		cfg = PIO_FALLING_EDGE_INTR_CFG;
		break;
	case GPIO_HAL_INTR_BOTH :
		cfg = PIO_ANY_EDGE_INTR_CFG;
		break;
	case GPIO_HAL_INTR_LEVEL_HIGH :
		cfg = PIO_HIGH_LEVEL_INTR_CFG;
		break;
	case GPIO_HAL_INTR_LEVEL_LOW :
		cfg = PIO_LOW_LEVEL_INTR_CFG;
		break;
	default :
		crit_err_exit(BAD_PARAMETER);
	}
	conf_io_pin(pin_mask, ctrl, fn, cfg, PIO_END_OF_FEAT);
}

/**
 * gpio_hal_intr_enable
 */
void gpio_hal_intr_enable(void *ctrl, uint32_t pin_mask)
{
	enable_pin_intr(pin_mask, ctrl);
}

/**
 * gpio_hal_is_intr_enabled
 */
boolean_t gpio_hal_is_intr_enabled(void *ctrl, uint32_t pin_mask)
{
	return ((((Pio *) ctrl)->PIO_IMR & pin_mask) ? TRUE : FALSE);
}

/**
 * gpio_hal_intr_disable
 */
void gpio_hal_intr_disable(void *ctrl, uint32_t pin_mask)
{
	disable_pin_intr(pin_mask, ctrl);
}

/**
 * gpio_hal_intr_clear
 */
void gpio_hal_intr_clear(void *ctrl)
{
	clear_pio_isr(ctrl);
}

/**
 * gpio_hal_isr_register
 */
void gpio_hal_isr_register(void *ctrl, gpio_hal_isr_clbk_t cb)
{
	if (!add_pio_intr_clbk(ctrl, cb)) {
		crit_err_exit(BAD_PARAMETER);
	}
}

/**
 * gpio_hal_isr_registered
 */
boolean_t gpio_hal_isr_registered(void *ctrl, gpio_hal_isr_clbk_t cb)
{
	return (test_pio_intr_clbk(ctrl, cb));
}

/**
 * gpio_hal_debounce_set_us
 */
void gpio_hal_debounce_set_us(void *ctrl, int us)
{
	set_io_dbnc_tm_us(ctrl, us);
}

/**
 * gpio_hal_debounce_set_ms
 */
void gpio_hal_debounce_set_ms(void *ctrl, int ms)
{
	set_io_dbnc_tm_ms(ctrl, ms);
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

/**
 * gpio_hal_get_ctrl
 */
void *gpio_hal_get_ctrl(int ctrl_id)
{
	switch (ctrl_id) {
	case 0 :
		return (PIOA);
	case 1 :
		return (PIOB);
#if defined(ID_PIOC)
	case 2 :
		return (PIOC);
#endif
	default :
		crit_err_exit(BAD_PARAMETER);
		return (NULL);
	}
}
#endif

/*
 * pio.h
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

#ifndef PIO_H
#define PIO_H

#ifndef GPIO_HAL_IMPL
 #define GPIO_HAL_IMPL 0
#endif

#ifndef PINMUX_HAL_IMPL
 #define PINMUX_HAL_IMPL 0
#endif

enum pio_func {
	PIO_OUTPUT,
	PIO_INPUT,
	PIO_PERIPH_A,
	PIO_PERIPH_B,
        PIO_PERIPH_C,
        PIO_PERIPH_D
};

enum pio_feat {
	PIO_PULL_UP_ON,
        PIO_PULL_UP_OFF,
	PIO_PULL_DOWN_ON,
        PIO_PULL_DOWN_OFF,
	PIO_MULTI_DRIVE_ON,
	PIO_MULTI_DRIVE_OFF,
        PIO_GLITCH_FILTER_ON,
	PIO_DEBOUNCE_FILTER_ON,
	PIO_INPUT_FILTER_OFF,
	PIO_DRIVE_LOW,
	PIO_DRIVE_HIGH,
	PIO_ANY_EDGE_INTR,
	PIO_RISING_EDGE_INTR,
	PIO_FALLING_EDGE_INTR,
	PIO_HIGH_LEVEL_INTR,
	PIO_LOW_LEVEL_INTR,
	PIO_ANY_EDGE_INTR_CFG,
	PIO_RISING_EDGE_INTR_CFG,
	PIO_FALLING_EDGE_INTR_CFG,
	PIO_HIGH_LEVEL_INTR_CFG,
	PIO_LOW_LEVEL_INTR_CFG,
        PIO_DISABLE_INTR,
        PIO_SCHMITT_ON,
        PIO_SCHMITT_OFF,
        PIO_END_OF_FEAT
};

/**
 * conf_io_pin
 *
 * Configure IO pin.
 *
 * @pin: Pin position bit.
 * @cont: PIO controller instance.
 * @func: Pin function definition (enum pio_func).
 * @va: List of additional pin features terminated with PIO_END_OF_FEAT
 *   (enum pio_feat).
 */
void conf_io_pin(unsigned int pin, Pio *cont, enum pio_func func, ...);

/**
 * get_pin_lev
 *
 * Get pin input logical level.
 *
 * @pin: Pin position bit.
 * @cont: PIO controller instance.
 *
 * Returns: HIGH; LOW.
 */
inline boolean_t get_pin_lev(unsigned int pin, Pio *cont)
{
	return ((cont->PIO_PDSR & pin) ? HIGH : LOW);
}

/**
 * set_pin_lev
 *
 * Set pin output logical level.
 *
 * @pin: Pin position bit.
 * @cont: PIO controller instance.
 * @lev: Pin level.
 */
inline void set_pin_lev(unsigned int pin, Pio *cont, boolean_t lev)
{
	if (lev) {
		cont->PIO_SODR = pin;
	} else {
		cont->PIO_CODR = pin;
	}
}

/**
 * get_pin_out
 *
 * Get pin output logical level.
 *
 * @pin: Pin position bit.
 * @cont: PIO controller instance.
 *
 * Returns: HIGH; LOW.
 */
inline boolean_t get_pin_out(unsigned int pin, Pio *cont)
{
	return ((cont->PIO_ODSR & pin) ? HIGH : LOW);
}

/**
 * set_io_dbnc_tm_us
 *
 * Set DIV field of PIO_SCDR register.
 *
 * @cont: PIO controller instance.
 * @utm: Time of debouncing clock tick in us (min 62 us, max 999 us).
 */
void set_io_dbnc_tm_us(Pio *cont, int utm);

/**
 * set_io_dbnc_tm_ms
 *
 * Set DIV field of PIO_SCDR register.
 *
 * @cont: PIO controller instance.
 * @mtm: Time of debouncing clock tick in ms.
 */
void set_io_dbnc_tm_ms(Pio *cont, int mtm);

#if PIOA_INTR == 1 || PIOB_INTR == 1 || PIOC_INTR == 1
/**
 * add_pio_intr_clbk
 *
 * Register PIO interrupt callback function.
 *
 * @cont: PIO controller instance.
 * @clbk: Pointer to callback function.
 */
boolean_t add_pio_intr_clbk(Pio *cont, BaseType_t (*clbk)(unsigned int));

/**
 * test_pio_intr_clbk
 *
 * Test if interrupt callback function is registered.
 *
 * @cont: PIO controller instance.
 * @clbk: Pointer to callback function.
 */
boolean_t test_pio_intr_clbk(Pio *cont, BaseType_t (*clbk)(unsigned int));
#endif

/**
 * enable_pio_clk
 *
 * Enable PIO clock.
 *
 * @cont: PIO controller instance.
 */
void enable_pio_clk(Pio *cont);

/**
 * disable_pio_clk
 *
 * Disable PIO clock.
 *
 * @cont: PIO controller instance.
 */
void disable_pio_clk(Pio *cont);

/**
 * clear_pio_isr
 *
 * Clear PIO pending interrupts.
 *
 * @cont: PIO controller instance.
 */
void clear_pio_isr(Pio *cont);

/**
 * enable_pin_intr
 *
 * Enable pin interrupt.
 *
 * @pin: Pin position bit.
 * @cont: PIO controller instance.
 */
inline void enable_pin_intr(unsigned int pin, Pio *cont)
{
	cont->PIO_IER = pin;
}

/**
 * disable_pin_intr
 *
 * Disable pin interrupt.
 *
 * @pin: Pin position bit.
 * @cont: PIO controller instance.
 */
inline void disable_pin_intr(unsigned int pin, Pio *cont)
{
	cont->PIO_IDR = pin;
}

/**
 * is_pin_intr_enabled
 *
 * Is pin interrupt enabled?
 *
 * @pin: Pin position bit.
 * @cont: PIO controller instance.
 */
inline boolean_t is_pin_intr_enabled(unsigned int pin, Pio *cont)
{
	return ((cont->PIO_IMR & pin) ? TRUE : FALSE);
}

/**
 * get_pio_periph_abcd
 *
 * Get pin peripheral assignment (enum pio_func).
 *
 * @pin: Pin position bit.
 * @cont: PIO controller instance.
 *
 * Returns: PIO_PERIPH_A; PIO_PERIPH_B; PIO_PERIPH_C; PIO_PERIPH_D.
 */
enum pio_func get_pio_periph_abcd(unsigned int pin, Pio *cont);

#endif

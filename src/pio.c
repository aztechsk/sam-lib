/*
 * pio.c
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

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <gentyp.h>
#include "sysconf.h"
#include "board.h"
#include <mmio.h>
#include "criterr.h"
#include "pmc.h"
#include "pio.h"
#include <stdarg.h>

extern inline boolean_t get_pin_lev(unsigned int pin, Pio *cont);
extern inline void set_pin_lev(unsigned int pin, Pio *cont, boolean_t lev);
extern inline boolean_t get_pin_out(unsigned int pin, Pio *cont);
extern inline void enable_pin_intr(unsigned int pin, Pio *cont);
extern inline void disable_pin_intr(unsigned int pin, Pio *cont);
extern inline boolean_t is_pin_intr_enabled(unsigned int pin, Pio *cont);

/**
 * conf_io_pin
 */
void conf_io_pin(unsigned int pin, Pio *cont, enum pio_func func, ...)
{
	va_list ap;
        enum pio_feat feat;

	if (!pin || (pin & (pin - 1))) {
		crit_err_exit(BAD_PARAMETER);
	}
	va_start(ap, func);
	while ((feat = va_arg(ap, int)) != PIO_END_OF_FEAT) {
		switch (feat) {
		case PIO_PULL_UP_ON :
			cont->PIO_PPDDR = pin;
			cont->PIO_PUER = pin;
			break;
		case PIO_PULL_UP_OFF :
			cont->PIO_PUDR = pin;
			break;
		case PIO_PULL_DOWN_ON :
			cont->PIO_PUDR = pin;
                        cont->PIO_PPDER = pin;
			break;
		case PIO_PULL_DOWN_OFF :
			cont->PIO_PPDDR = pin;
			break;
		case PIO_MULTI_DRIVE_ON :
			cont->PIO_MDER = pin;
			break;
		case PIO_MULTI_DRIVE_OFF :
			cont->PIO_MDDR = pin;
			break;
		case PIO_GLITCH_FILTER_ON :
			cont->PIO_IFSCDR = pin;
			cont->PIO_IFER = pin;
			break;
		case PIO_DEBOUNCE_FILTER_ON :
			cont->PIO_IFSCER = pin;
			cont->PIO_IFER = pin;
			break;
		case PIO_INPUT_FILTER_OFF :
			cont->PIO_IFDR = pin;
			break;
		case PIO_DRIVE_LOW :
			cont->PIO_CODR = pin;
			break;
		case PIO_DRIVE_HIGH :
			cont->PIO_SODR = pin;
			break;
		case PIO_ANY_EDGE_INTR :
			cont->PIO_AIMDR = pin;
			cont->PIO_IER = pin;
			break;
		case PIO_RISING_EDGE_INTR :
			cont->PIO_AIMER = pin;
                        cont->PIO_ESR = pin;
                        cont->PIO_REHLSR = pin;
			cont->PIO_IER = pin;
			break;
		case PIO_FALLING_EDGE_INTR :
			cont->PIO_AIMER = pin;
                        cont->PIO_ESR = pin;
                        cont->PIO_FELLSR = pin;
			cont->PIO_IER = pin;
			break;
		case PIO_HIGH_LEVEL_INTR :
			cont->PIO_AIMER = pin;
                        cont->PIO_LSR = pin;
                        cont->PIO_REHLSR = pin;
			cont->PIO_IER = pin;
			break;
		case PIO_LOW_LEVEL_INTR :
			cont->PIO_AIMER = pin;
                        cont->PIO_LSR = pin;
                        cont->PIO_FELLSR = pin;
			cont->PIO_IER = pin;
			break;
		case PIO_ANY_EDGE_INTR_CFG :
			cont->PIO_AIMDR = pin;
			break;
		case PIO_RISING_EDGE_INTR_CFG :
			cont->PIO_AIMER = pin;
                        cont->PIO_ESR = pin;
                        cont->PIO_REHLSR = pin;
			break;
		case PIO_FALLING_EDGE_INTR_CFG :
			cont->PIO_AIMER = pin;
                        cont->PIO_ESR = pin;
                        cont->PIO_FELLSR = pin;
			break;
		case PIO_HIGH_LEVEL_INTR_CFG :
			cont->PIO_AIMER = pin;
                        cont->PIO_LSR = pin;
                        cont->PIO_REHLSR = pin;
			break;
		case PIO_LOW_LEVEL_INTR_CFG :
			cont->PIO_AIMER = pin;
                        cont->PIO_LSR = pin;
                        cont->PIO_FELLSR = pin;
			break;
		case PIO_SCHMITT_ON :
			taskENTER_CRITICAL();
			cont->PIO_SCHMITT &= ~pin;
			taskEXIT_CRITICAL();
			break;
		case PIO_SCHMITT_OFF :
			taskENTER_CRITICAL();
			cont->PIO_SCHMITT |= pin;
                        taskEXIT_CRITICAL();
			break;
		case PIO_DISABLE_INTR :
			cont->PIO_IDR = pin;
			break;
		default :
			crit_err_exit(BAD_PARAMETER);
			break;
		}
	}
	va_end(ap);
	if (func == PIO_OUTPUT) {
		cont->PIO_OER = pin;
		cont->PIO_PER = pin;
	} else if (func == PIO_INPUT) {
		cont->PIO_ODR = pin;
		cont->PIO_PER = pin;
	} else {
		switch (func) {
		case PIO_PERIPH_A :
			taskENTER_CRITICAL();
                	cont->PIO_ABCDSR[1] &= ~pin;
			cont->PIO_ABCDSR[0] &= ~pin;
                        taskEXIT_CRITICAL();
			break;
		case PIO_PERIPH_B :
			taskENTER_CRITICAL();
                	cont->PIO_ABCDSR[1] &= ~pin;
			cont->PIO_ABCDSR[0] |= pin;
                        taskEXIT_CRITICAL();
			break;
		case PIO_PERIPH_C :
			taskENTER_CRITICAL();
                	cont->PIO_ABCDSR[1] |= pin;
			cont->PIO_ABCDSR[0] &= ~pin;
                        taskEXIT_CRITICAL();
			break;
		case PIO_PERIPH_D :
			taskENTER_CRITICAL();
                	cont->PIO_ABCDSR[1] |= pin;
			cont->PIO_ABCDSR[0] |= pin;
                        taskEXIT_CRITICAL();
			break;
		default :
			crit_err_exit(BAD_PARAMETER);
			break;
		}
		cont->PIO_PDR = pin;
	}
}

/**
 * set_io_dbnc_tm_us
 */
void set_io_dbnc_tm_us(Pio *cont, int utm)
{
	if (utm < 61) {
		crit_err_exit(BAD_PARAMETER);
	}
	int div = (((uint64_t) utm * F_SLCK) + 1000000 - 1) / (2 * 1000000);
	if(div) {
		div--;
	}
	if (div > 0x3FFF) {
		div = 0x3FFF;
	}
	cont->PIO_SCDR = PIO_SCDR_DIV(div);
}

/**
 * set_io_dbnc_tm_ms
 */
void set_io_dbnc_tm_ms(Pio *cont, int mtm)
{
	int div;

	uint64_t num = mtm * (uint64_t) F_SLCK;
	uint64_t div_u64 = (num + (2000 - 1)) / 2000;
	if (div_u64 == 0) {
		div = 0;
	} else if (div_u64 - 1 > 0x3FFF) {
	        div = 0x3FFF;
	} else {
	        div = div_u64 - 1;
	}
	cont->PIO_SCDR = PIO_SCDR_DIV(div);
}

#if PIOA_INTR == 1
static BaseType_t (*pioa_clbk_arr[PIOA_INTR_CLBK_ARRAY_SIZE])(unsigned int);
static boolean_t pioa_ini;

/**
 * PIOA_Handler
 */
void PIOA_Handler(void)
{
	BaseType_t tsk_wkn = pdFALSE;
	unsigned int pio_isr;

	pio_isr = PIOA->PIO_ISR;
	for (int i = 0; i < PIOA_INTR_CLBK_ARRAY_SIZE; i++) {
		if (!pioa_clbk_arr[i]) {
			break;
		} else {
			if (pdTRUE == (*pioa_clbk_arr[i])(pio_isr)) {
				tsk_wkn = pdTRUE;
			}
		}
	}
	portEND_SWITCHING_ISR(tsk_wkn);
}
#endif

#if PIOB_INTR == 1
static BaseType_t (*piob_clbk_arr[PIOB_INTR_CLBK_ARRAY_SIZE])(unsigned int);
static boolean_t piob_ini;

/**
 * PIOB_Handler
 */
void PIOB_Handler(void)
{
	BaseType_t tsk_wkn = pdFALSE;
	unsigned int pio_isr;

	pio_isr = PIOB->PIO_ISR;
	for (int i = 0; i < PIOB_INTR_CLBK_ARRAY_SIZE; i++) {
		if (!piob_clbk_arr[i]) {
			break;
		} else {
			if (pdTRUE == (*piob_clbk_arr[i])(pio_isr)) {
				tsk_wkn = pdTRUE;
			}
		}
	}
	portEND_SWITCHING_ISR(tsk_wkn);
}
#endif

#if defined(ID_PIOC) && PIOC_INTR == 1
static BaseType_t (*pioc_clbk_arr[PIOC_INTR_CLBK_ARRAY_SIZE])(unsigned int);
static boolean_t pioc_ini;

/**
 * PIOC_Handler
 */
void PIOC_Handler(void)
{
	BaseType_t tsk_wkn = pdFALSE;
	unsigned int pio_isr;

	pio_isr = PIOC->PIO_ISR;
	for (int i = 0; i < PIOC_INTR_CLBK_ARRAY_SIZE; i++) {
		if (!pioc_clbk_arr[i]) {
			break;
		} else {
			if (pdTRUE == (*pioc_clbk_arr[i])(pio_isr)) {
				tsk_wkn = pdTRUE;
			}
		}
	}
	portEND_SWITCHING_ISR(tsk_wkn);
}
#endif

#if PIOA_INTR == 1 || PIOB_INTR == 1 || PIOC_INTR == 1
/**
 * add_pio_intr_clbk
 */
boolean_t add_pio_intr_clbk(Pio *cont, BaseType_t (*clbk)(unsigned int))
{
#if PIOA_INTR == 1
	if (cont == PIOA) {
		taskENTER_CRITICAL();
		for (int i = 0; i < PIOA_INTR_CLBK_ARRAY_SIZE; i++) {
			if (!pioa_clbk_arr[i]) {
				pioa_clbk_arr[i] = clbk;
				if (!pioa_ini) {
					pioa_ini = TRUE;
					PIOA->PIO_ISR;
                                        NVIC_ClearPendingIRQ(PIOA_IRQn);
                                        NVIC_SetPriority(PIOA_IRQn, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
                                        NVIC_EnableIRQ(PIOA_IRQn);
				}
                                taskEXIT_CRITICAL();
				return (TRUE);
			} else {
				if (pioa_clbk_arr[i] == clbk) {
					taskEXIT_CRITICAL();
					return (TRUE);
				}
			}
		}
		taskEXIT_CRITICAL();
                return (FALSE);
	}
#endif
#if PIOB_INTR == 1
	if (cont == PIOB) {
		taskENTER_CRITICAL();
		for (int i = 0; i < PIOB_INTR_CLBK_ARRAY_SIZE; i++) {
			if (!piob_clbk_arr[i]) {
				piob_clbk_arr[i] = clbk;
				if (!piob_ini) {
					piob_ini = TRUE;
					PIOB->PIO_ISR;
                                        NVIC_ClearPendingIRQ(PIOB_IRQn);
                                        NVIC_SetPriority(PIOB_IRQn, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
                                        NVIC_EnableIRQ(PIOB_IRQn);
				}
                                taskEXIT_CRITICAL();
				return (TRUE);
			} else {
				if (piob_clbk_arr[i] == clbk) {
					taskEXIT_CRITICAL();
					return (TRUE);
				}
			}
		}
                taskEXIT_CRITICAL();
                return (FALSE);
	}
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
	if (cont == PIOC) {
		taskENTER_CRITICAL();
		for (int i = 0; i < PIOC_INTR_CLBK_ARRAY_SIZE; i++) {
			if (!pioc_clbk_arr[i]) {
				pioc_clbk_arr[i] = clbk;
				if (!pioc_ini) {
					pioc_ini = TRUE;
                                        PIOC->PIO_ISR;
                                        NVIC_ClearPendingIRQ(PIOC_IRQn);
                                        NVIC_SetPriority(PIOC_IRQn, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
                                        NVIC_EnableIRQ(PIOC_IRQn);
				}
                                taskEXIT_CRITICAL();
				return (TRUE);
			} else {
				if (pioc_clbk_arr[i] == clbk) {
					taskEXIT_CRITICAL();
					return (TRUE);
				}
			}
		}
                taskEXIT_CRITICAL();
                return (FALSE);
	}
#endif
	crit_err_exit(BAD_PARAMETER);
	return (FALSE);
}

/**
 * test_pio_intr_clbk
 */
boolean_t test_pio_intr_clbk(Pio *cont, BaseType_t (*clbk)(unsigned int))
{
#if PIOA_INTR == 1
	if (cont == PIOA) {
		taskENTER_CRITICAL();
		for (int i = 0; i < PIOA_INTR_CLBK_ARRAY_SIZE; i++) {
			if (pioa_clbk_arr[i] == clbk) {
                                taskEXIT_CRITICAL();
				return (TRUE);
			}
		}
		taskEXIT_CRITICAL();
                return (FALSE);
	}
#endif
#if PIOB_INTR == 1
	if (cont == PIOB) {
		taskENTER_CRITICAL();
		for (int i = 0; i < PIOB_INTR_CLBK_ARRAY_SIZE; i++) {
			if (piob_clbk_arr[i] == clbk) {
                                taskEXIT_CRITICAL();
				return (TRUE);
			}
		}
                taskEXIT_CRITICAL();
                return (FALSE);
	}
#endif
#if defined(ID_PIOC) && PIOC_INTR == 1
	if (cont == PIOC) {
		taskENTER_CRITICAL();
		for (int i = 0; i < PIOC_INTR_CLBK_ARRAY_SIZE; i++) {
			if (pioc_clbk_arr[i] == clbk) {
                                taskEXIT_CRITICAL();
				return (TRUE);
			}
		}
                taskEXIT_CRITICAL();
                return (FALSE);
	}
#endif
	crit_err_exit(BAD_PARAMETER);
	return (FALSE);
}
#endif

/**
 * enable_pio_clk
 */
void enable_pio_clk(Pio *cont)
{
	if (cont == PIOA) {
		enable_periph_clk(ID_PIOA);
		return;
	}
	if (cont == PIOB) {
		enable_periph_clk(ID_PIOB);
		return;
	}
#if defined(ID_PIOC)
	if (cont == PIOC) {
		enable_periph_clk(ID_PIOC);
		return;
	}
#endif
	crit_err_exit(BAD_PARAMETER);
}

/**
 * disable_pio_clk
 */
void disable_pio_clk(Pio *cont)
{
	if (cont == PIOA) {
		disable_periph_clk(ID_PIOA);
		return;
	}
	if (cont == PIOB) {
		disable_periph_clk(ID_PIOB);
		return;
	}
#if defined(ID_PIOC)
	if (cont == PIOC) {
		disable_periph_clk(ID_PIOC);
		return;
	}
#endif
	crit_err_exit(BAD_PARAMETER);
}

/**
 * clear_pio_isr
 */
void clear_pio_isr(Pio *cont)
{
	if (cont == PIOA) {
		PIOA->PIO_ISR;
		return;
	}
	if (cont == PIOB) {
		PIOB->PIO_ISR;
		return;
	}
#if defined(ID_PIOC)
	if (cont == PIOC) {
		PIOC->PIO_ISR;
		return;
	}
#endif
	crit_err_exit(BAD_PARAMETER);
}

/**
 * get_pio_periph_abcd
 */
enum pio_func get_pio_periph_abcd(unsigned int pin, Pio *cont)
{
	if (!pin || (pin & (pin - 1))) {
		crit_err_exit(BAD_PARAMETER);
	}
	taskENTER_CRITICAL();
	if (cont->PIO_PSR & pin) {
		if (cont->PIO_OSR & pin) {
			taskEXIT_CRITICAL();
			return (PIO_OUTPUT);
		} else {
			taskEXIT_CRITICAL();
			return (PIO_INPUT);
		}
	} else {
		if (cont->PIO_ABCDSR[1] & pin) {
			if (cont->PIO_ABCDSR[0] & pin) {
				taskEXIT_CRITICAL();
				return (PIO_PERIPH_D);
			} else {
				taskEXIT_CRITICAL();
				return (PIO_PERIPH_C);
			}
		} else {
			if (cont->PIO_ABCDSR[0] & pin) {
				taskEXIT_CRITICAL();
				return (PIO_PERIPH_B);
			} else {
				taskEXIT_CRITICAL();
				return (PIO_PERIPH_A);
			}
		}
	}
}

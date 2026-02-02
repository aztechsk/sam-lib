/*
 * sleep.h
 *
 * Copyright (c) 2023 Jan Rusnak <jan@rusnak.sk>
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

#ifndef SLEEP_H
#define SLEEP_H

#ifndef SLEEP_FEAT
 #define SLEEP_FEAT 0
#endif

#if SLEEP_FEAT == 1

enum sleep_cmd {
	SLEEP_CMD_SUSP,
	SLEEP_CMD_WAKE
};

enum sleep_prio {
	SLEEP_PRIO_SUSP_FIRST,
	SLEEP_PRIO_SUSP_SECOND,
	SLEEP_PRIO_SUSP_LAST
};

enum sleep_mode {
	SLEEP_MODE_BACKUP,
        SLEEP_MODE_WAIT
};

/**
 * init_sleep
 *
 * Initializes SLEEP module.
 *
 * @set_clocks: Callback function for suspend/resume clocks.
 *   Callback's parameters: SLEEP - prepare clocks for sleep; WAKE - return clocks to normal.
 * @sleep_clbk: Before sleep / after wake callback function. May be NULL.
 *   Callback's parameters: SLEEP; WAKE.
 */
void init_sleep(void (*set_clocks)(boolean_t), void (*sleep_clbk)(boolean_t));

/**
 * reg_sleep_clbk
 *
 * Registers sleep callback function for software modules.
 *
 * Callback function is called before entering sleep mode with 'SLEEP_CMD_SUSP'
 * parameter, and after waking up from sleep mode with 'SLEEP_CMD_WAKE' parameter.
 * Call order for suspend is defined by parameter 'prio'. Call order in 'prio' group
 * is defined by reg_sleep_clbk() calling order. Wakeup call order is reverted.
 * In case of first parameter of callback function is 'SLEEP_CMD_SUSP', then second
 * parameter will be sleep mode type (enum sleep_mode).
 *
 * @clbk: Pointer to callback function.
 * @prio: Callback priority (enum sleep_prio).
 */
void reg_sleep_clbk(void (*clbk)(enum sleep_cmd, ...), enum sleep_prio prio);

/**
 * start_sleep
 *
 * Starts sleep mode.
 *
 * @mode: Sleep mode type (enum sleep_mode).
 */
void start_sleep(enum sleep_mode mode);

/**
 * enable_idle_sleep
 *
 * Enables FreeRTOS idle sleep feature.
 */
void enable_idle_sleep(void);

/**
 * disable_idle_sleep
 *
 * Disabled FreeRTOS idle sleep feature.
 */
void disable_idle_sleep(void);

/**
 * vApplicationIdleHook
 *
 * FreeRTOS idle hook prototype.
 */
void vApplicationIdleHook(void);
#endif

#endif

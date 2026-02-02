/*
 * dlycnt.h
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

/*
 * dlycnt.h - us busy-wait based on a free-running counter (DWT or TC)
 *
 * @file dlycnt.h
 * @brief Microsecond delay routines implemented as pure busy-wait loops.
 * @defgroup dlycnt Delay counter (us busy-wait)
 * @{
 *
 * Provides a unified API for two backends:
 *  - DWT/CYCCNT (Cortex-M) - high-resolution core cycle counter.
 *  - TC (Timer/Counter) - 16-bit free-running hardware timer, wrap-safe.
 *
 * @note DWT mode: CYCCNT can be affected externally by the debug port.
 *       When JTAG/SWD is disconnected, the DWT block may freeze and
 *       the busy-wait will effectively hang until debug hardware is
 *       re-attached. Use the TC backend if operation independent of
 *       the debugger is required.
 *
 * @note TC mode: for us precision, choose TCCLKS so that the resulting
 *       f_TC is an integer number of MHz (e.g., MCK/32 = 2 MHz at
 *       SystemCoreClock = 64 MHz).
 *
 * @par Thread safety
 * Functions are reentrant; the implementation uses pure busy-wait and
 * no shared state.
 *
 * @par Accuracy
 * Busy-waits are not guaranteed real-time minima under preemption.
 * Interrupt service routines can extend actual delay time. us->tick
 * conversion uses "ceil" mapping to ensure the delay is never shorter
 * than requested.
 */

#ifndef DLYCNT_H
#define DLYCNT_H

#ifndef DLYCNT_US
 #define DLYCNT_US 0
#endif

/**
 * @brief Initialize the delay backend (DWT or TC).
 *
 *  DWT: enables DEMCR.TRCENA and DWT_CTRL.CYCCNTENA, resets CYCCNT.
 *  TC: enables peripheral clock, sets up capture free-run mode, starts CV.
 *
 * @pre Core clock (SystemCoreClock / MCK) must be configured.
 * @warning In DWT mode, CYCCNT may freeze when JTAG/SWD is detached.
 */
void init_dlycnt(void);

/**
 * @brief Busy-wait delay in microseconds.
 *
 * @param dly  Delay time in us. A value of 0 returns immediately.
 *
 * Implementation details:
 * - DWT: computes cycle count as ceil(us * SystemCoreClock / 1e6).
 * - TC: uses 16-bit wrap-safe delta, chunked to 0x7FFF ticks.
 *
 * @note Interrupt latency may lengthen the real elapsed time.
 */
void delay_us(uint32_t dly);

/**
 * @brief Optional diagnostic output (clock, resolution, overflow).
 *
 * @note Availability depends on project logging facilities.
 */
void log_dlycnt(void);

/** @} */
#endif

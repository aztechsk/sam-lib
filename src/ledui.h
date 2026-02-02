/*
 * ledui.h
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

#ifndef LEDUI_H
#define LEDUI_H

#ifndef LEDUI
 #define LEDUI 0
#endif

#if LEDUI == 1

// Led control commands.
enum ledui_led_cmnd {
        LEDUI_LED_OFF,
        LEDUI_LED_ON,
        LEDUI_LED_BLINK_SLOW,
        LEDUI_LED_BLINK_NORMAL,
        LEDUI_LED_BLINK_FAST,
        LEDUI_LED_NEXT_STATE,
        LEDUI_LED_PREV_STATE,
        LEDUI_LED_BLINK_SLOW_STDF,
        LEDUI_LED_BLINK_NORMAL_STDF,
        LEDUI_LED_BLINK_FAST_STDF
};

// Initial led state after LEDUI_LED_BLINK_*_STDF command.
enum ledui_blink_start {
	LEDUI_BLINK_START_ON,
	LEDUI_BLINK_START_OFF,
        LEDUI_BLINK_START_NSET
};

// Led position.
enum ledui_board_leds {
        LEDUI_BLED1,
        LEDUI_BLED2,
        LEDUI_BLED3,
        LEDUI_BLED4
};

// Led state descriptor.
struct ledui_led_state {
        struct ledui_led_state *prev;
        struct ledui_led_state *next;
        enum ledui_led_cmnd state;
        enum ledui_board_leds pos;
        boolean_t on_off;
        int freq;
        int cnt;
};

typedef struct ledui_led_state *ledui_led;

// Led instances.
extern ledui_led LEDUI1, LEDUI2, LEDUI3, LEDUI4;

/**
 * init_ledui_pins
 *
 * Initialize LEDUI pins.
 */
void init_ledui_pins(void);

/**
 * init_ledui
 *
 * Initialize LEDUI control task.
 */
void init_ledui(void);

/**
 * set_ledui_led_state
 *
 * Change state of LEDUI led.
 *
 * @ins: Led instance.
 * @cmnd: New state = LEDUI_LED_OFF, LEDUI_LED_ON, LEDUI_LED_BLINK_SLOW,
 *   LEDUI_LED_BLINK_SLOW_STDF, LEDUI_LED_BLINK_NORMAL, LEDUI_LED_BLINK_NORMAL_STDF,
 *   LEDUI_LED_BLINK_FAST, LEDUI_LED_BLINK_FAST_STDF, LEDUI_LED_NEXT_STATE,
 *   LEDUI_LED_PREV_STATE (enum ledui_led_cmnd).
 * @va1: Blink start = LEDUI_BLINK_START_ON, LEDUI_BLINK_START_OFF, LEDUI_BLINK_START_NSET
 *   (enum ledui_blink_start). Parameter must be used for commands LEDUI_LED_BLINK_SLOW_STDF,
 *   LEDUI_LED_BLINK_NORMAL_STDF, LEDUI_LED_BLINK_FAST_STDF.
 */
void set_ledui_led_state(ledui_led ins, enum ledui_led_cmnd cmnd, ...);

/**
 * set_ledui_all_leds_state
 *
 * Change state of all LEDUI leds.
 *
 * @cmnd: New state = LEDUI_LED_OFF, LEDUI_LED_ON, LEDUI_LED_BLINK_SLOW, LEDUI_LED_BLINK_NORMAL,
 *   LEDUI_LED_BLINK_FAST (enum ledui_led_cmnd).
 */
void set_ledui_all_leds_state(enum ledui_led_cmnd cmnd);

/**
 * ledui_led_hw_on
 *
 * Switch led on (low level command).
 *
 * @pos: Led position on board (enum ledui_board_leds).
 */
void ledui_led_hw_on(enum ledui_board_leds pos);

/**
 * ledui_led_hw_off
 *
 * Switch led off (low level command).
 *
 * @pos: Led position on board (enum ledui_board_leds).
 */
void ledui_led_hw_off(enum ledui_board_leds pos);
#endif

#endif

/*
 * led.h
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

#ifndef LED_H
#define LED_H

#ifndef LED
 #define LED 0
#endif

#if LED == 1

enum led_state {
        LED_STATE_ON,
	LED_STATE_OFF,
        LED_STATE_FLASH,
        LED_STATE_BLINK
};

typedef struct led_dsc *led;

struct led_dsc {
        unsigned int pin; // <SetIt>
	Pio *cont; // <SetIt>
	boolean_t anode_on_pin; // <SetIt>
	enum led_state state;
        enum led_state state_chng;
	int delay;
        int delay_chng;
	int dly_cnt;
	boolean_t off;
        led next;
};

/**
 * init_led
 *
 * Initialize led control task.
 */
void init_led(void);

/**
 * add_led_dev
 *
 * Register new led device.
 *
 * @dev: LED instance.
 */
void add_led_dev(led dev);

/**
 * set_led_dev_state
 *
 * @dev: LED instance.
 * @state: new state = LED_STATE_OFF, LED_STATE_ON, LED_STATE_FLASH,
 *   LED_STATE_BLINK (enum led_state).
 * @va1: Parameter must be used for state LED_STATE_BLINK and determines delay
 *   between flashes.
 */
void set_led_dev_state(led dev, enum led_state state, ...);
#endif

#endif

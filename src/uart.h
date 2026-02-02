/*
 * uart.h
 *
 * Copyright (c) 2021 Jan Rusnak <jan@rusnak.sk>
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

#ifndef UART_H
#define UART_H

#ifndef UART_HDLC
 #define UART_HDLC 0
#endif
#ifndef UART_RX_BYTE
 #define UART_RX_BYTE 0
#endif

#if UART_HDLC == 1
#ifndef STRUCT_HDLC_MESG_STATS
#define STRUCT_HDLC_MESG_STATS
struct hdlc_mesg {
	int sz;
        int adr;
	uint8_t *pld;
};

struct hdlc_stats {
	int ovr_lerr;
	int fra_lerr;
        int par_lerr;
        int no_f1_perr;
        int bf_ov_perr;
        int es_sq_perr;
	int syn_f1_perr;
};
#endif
#endif

#if UART_RX_BYTE == 1 || UART_HDLC == 1
enum uart_rx_mode {
	UART_RX_BYTE_MODE,
        UART_HDLC_MODE
};

typedef struct uart_dsc *uart;

struct uart_dsc {
	int id; // <SetIt>
        Uart *mmio;
	int bdr; // <SetIt>
	unsigned int mr; // <SetIt>
        SemaphoreHandle_t tx_sig;
#if UART_HDLC == 1
	SemaphoreHandle_t rx_sig;
#endif
#if UART_RX_BYTE == 1
	int rx_que_sz; // <SetIt>
	QueueHandle_t rx_que;
#endif
        enum uart_rx_mode rx_mode;
	BaseType_t (*hndlr)(uart u);
#if UART_HDLC == 1
	int hdlc_bf_sz; // <SetIt>
	int HDLC_FLAG; // <SetIt>
	int HDLC_ESC; // <SetIt>
	int HDLC_MOD; // <SetIt>
	struct hdlc_mesg hdlc_mesg;
	struct hdlc_stats hdlc_stats;
        int rcv_st;
#endif
        boolean_t dma;
};

/**
 * init_uart
 *
 * Configure UART instance to requested mode.
 *
 * @dev: UART instance.
 * @m: UART receive mode (enum uart_rx_mode).
 */
void init_uart(uart dev, enum uart_rx_mode m);

/**
 * uart_tx_buff
 *
 * Transmit data buffer via UART instance.
 * Caller task is blocked during sending data. Call to this
 * function should be synchronized externally.
 *
 * @dev: UART instance.
 * @p_buf: Pointer to data buffer.
 * @size: Number of bytes to send.
 *
 * Returns: 0 - success; -EDMA - dma error.
 */
int uart_tx_buff(void *dev, void *p_buf, int size);
#endif

#if UART_RX_BYTE == 1
/**
 * uart_rx_byte
 *
 * Receive byte via UART instance.
 * Caller task is blocked until byte is not received.
 *
 * @dev: UART instance.
 * @p_byte: Pointer to memory for store received byte.
 * @tmo: Timeout in tick periods.
 *
 * Returns: 0 - success; -ERCV - serial line error; -ETMO - no byte received
 *          in tmo time; -EINTR - receiver interrupted.
 */
int uart_rx_byte(void *dev, void *p_byte, TickType_t tmo);
#endif

#if UART_RX_BYTE == 1
/**
 * uart_intr_rx
 *
 * Send INTR event to receiver.
 *
 * @dev: UART instance.
 *
 * Returns: TRUE - event sent successfully; FALSE - rx queue full.
 */
boolean_t uart_intr_rx(void *dev);
#endif

#if UART_HDLC == 1
/**
 * uart_tx_hdlc_mesg
 *
 * Create HDLC message from payload data and transmit it via UART instance.
 * Caller task is blocked during sending message.
 *
 * @dev: UART instance.
 * @pld: Pointer to payload data.
 * @size: Size of payload data.
 *
 * Returns: 0 - success; -EBFOV - construction of HDLC message failed;
 *          -EDMA - dma error.
 */
int uart_tx_hdlc_mesg(uart dev, uint8_t *pld, int size);

/**
 * uart_rx_hdlc_mesg
 *
 * Receive HDLC formated raw message via UART instance.
 * Caller task is blocked until message is not received or timeout is expired.
 *
 * @dev: UART instance.
 * @tmo: Timeout in tick periods.
 *
 * Returns: struct hdlc_mesg * - message successfully received;
 *          NULL - timeout.
 */
struct hdlc_mesg *uart_rx_hdlc_mesg(uart dev, TickType_t tmo);
#endif

#if UART_RX_BYTE == 1 || UART_HDLC == 1
/**
 * uart_get_dev
 *
 * @id: UART instance id.
 *
 * Returns: UART device.
 */
uart uart_get_dev(int id);
#endif

#endif

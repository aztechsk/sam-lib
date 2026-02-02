/*
 * usart.h
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

#ifndef USART_H
#define USART_H

#ifndef USART_RX_CHAR
 #define USART_RX_CHAR 0
#endif
#ifndef USART_HDLC
 #define USART_HDLC 0
#endif
#ifndef USART_ADR_HDLC
 #define USART_ADR_HDLC 0
#endif
#ifndef USART_ADR_CHAR
 #define USART_ADR_CHAR 0
#endif
#ifndef USART_YIT
 #define USART_YIT 0
#endif

#if USART_HDLC == 1 || USART_ADR_HDLC == 1
#ifndef STRUCT_HDLC_MESG_STATS
#define STRUCT_HDLC_MESG_STATS
struct hdlc_mesg {
	int sz;
	int adr;
	uint8_t *pld;
};

struct hdlc_stats {
	int ovr_lerr;      // HDLC ADR_HDLC
	int fra_lerr;      // HDLC ADR_HDLC
        int par_lerr;      // HDLC
        int no_f1_perr;    // HDLC ADR_HDLC
        int bf_ov_perr;    // HDLC ADR_HDLC
        int es_sq_perr;    // HDLC ADR_HDLC
	int syn_f1_perr;   // HDLC
};
#endif
#endif

#if USART_ADR_HDLC == 1
struct adr_hdlc_ext_stats {
	int unxp_adr_perr;     //  ADR_HDLC
        int max_adr_ovr_perr;  //  ADR_HDLC
	int fra_lerr_hdlc[4];  //  ADR_HDLC
	boolean_t was_perr;    //  ADR_HDLC
	int perr_sz;           //  ADR_HDLC
	int perr_adr;          //  ADR_HDLC
	uint8_t perr_dump[USART_ADR_HDLC_PERR_DUMP_SIZE]; // ADR_HDLC
	int rx_byte_cnt;       //  ADR_HDLC
};
#endif

#if USART_YIT == 1
#include "yit_cmd.h"
#define YIT_MSG_FLAG 0xCA

struct usart_yit {
	int cmd_idx;
	int buf_idx;
	uint8_t sum;
	int cmd_sz;
#if USART_YIT_DRIVER_STATS == 1
	int sum_err;
	int ser_err;
	int cmd_err;
	int buf_err;
	int syn_err;
	int rx_cmd_cnt;
#endif
	struct yit_cmd cmd[USART_YIT_CMD_ARY_SIZE];
};
#endif

#if USART_RX_CHAR == 1 || USART_HDLC == 1 || USART_ADR_HDLC == 1 || USART_ADR_CHAR == 1 ||\
    USART_YIT == 1
enum usart_mode {
	USART_RX_CHAR_MODE,
	USART_HDLC_MODE,
        USART_ADR_HDLC_MODE,
	USART_ADR_CHAR_MODE,
	USART_YIT_MODE
};

typedef struct usart_dsc *usart;

struct usart_dsc {
	int id; // <SetIt>
	void (*conf_pins)(boolean_t); // <SetIt>
        Usart *mmio;
        BaseType_t (*hndlr)(usart u);
#if USART_HDLC == 1 || USART_ADR_HDLC == 1
        SemaphoreHandle_t sig_rx;
#endif
	SemaphoreHandle_t sig_tx;
	int bdr; // <SetIt>
	unsigned int mr; // <SetIt>
        enum usart_mode mode;
#if USART_RX_CHAR == 1 || USART_ADR_CHAR == 1
	int rx_que_sz; // <SetIt>
#endif
#if USART_RX_CHAR == 1 || USART_ADR_CHAR == 1 || USART_YIT == 1
	QueueHandle_t rx_que;
#endif
#if USART_HDLC == 1 || USART_ADR_HDLC == 1
	int hdlc_bf_sz; // <SetIt>
	int HDLC_FLAG; // <SetIt>
	int HDLC_ESC; // <SetIt>
	int HDLC_MOD; // <SetIt>
	struct hdlc_mesg hdlc_mesg;
	struct hdlc_stats hdlc_stats;
#endif
#if USART_ADR_HDLC == 1
	int addr; // <SetIt> 256 - promiscuous mode.
	int bcst_addr; // <SetIt> 256 - broadcast unused.
#if USART_ADR_HDLC_EXT_STATS == 1
	struct adr_hdlc_ext_stats adr_hdlc_ext_stats;
#endif
#endif
#if USART_YIT == 1
	struct usart_yit usart_yit;
#endif
#if USART_HDLC == 1 || USART_ADR_HDLC == 1 || USART_YIT == 1
        int rcv_st;
#endif
	boolean_t dma;
};
#endif

#if USART_RX_CHAR == 1 || USART_HDLC == 1 || USART_ADR_HDLC == 1 || USART_ADR_CHAR == 1 ||\
    USART_YIT == 1
/**
 * init_usart
 *
 * Configure USART instance to requested mode.
 *
 * @dev: USART instance.
 * @m: USART mode (enum usart_mode).
 */
void init_usart(usart dev, enum usart_mode m);
#endif

#if USART_RX_CHAR == 1
/**
 * enable_usart
 *
 * Enable USART (revert disable_usart() function effects).
 *
 * @dev: USART instance.
 */
void enable_usart(void *dev);
#endif

#if USART_RX_CHAR == 1
/**
 * disable_usart
 *
 * Disable USART (switch USART block off).
 *
 * @dev: USART instance.
 */
void disable_usart(void *dev);
#endif

#if USART_RX_CHAR == 1 || USART_HDLC == 1 || USART_YIT == 1
/**
 * usart_tx_buff
 *
 * Transmit data buffer via USART instance.
 * Caller task is blocked during sending data.
 *
 * @dev: USART instance.
 * @p_buf: Pointer to data buffer (bytes or half-words (MODE9)).
 * @size: Number of units to send.
 *
 * Returns: 0 - success; -EDMA - dma error.
 */
int usart_tx_buff(void *dev, void *p_buf, int size);
#endif

#if USART_RX_CHAR == 1
/**
 * usart_rx_char
 *
 * Receive 5-9 bit char via USART instance.
 * Caller task is blocked until char is not received, or timeout is expired.
 *
 * @dev: USART instance.
 * @p_char: Pointer to byte or half-word memory for store received char.
 * @tmo: Timeout in tick periods.
 *
 * Returns: 0 - success; -ERCV - serial line error; -ETMO - no byte received
 *          in tmo time; -EINTR - receiver interrupted.
 */
int usart_rx_char(void *dev, void *p_char, TickType_t tmo);
#endif

#if USART_RX_CHAR == 1
/**
 * usart_intr_rx
 *
 * Send INTR event to receiver.
 *
 * @dev: USART instance.
 *
 * Returns: TRUE - event sent successfully; FALSE - rx queue full.
 */
boolean_t usart_intr_rx(void *dev);
#endif

#if USART_HDLC == 1
/**
 * usart_tx_hdlc_mesg
 *
 * Create HDLC message from payload data and transmit it via USART instance.
 * Caller task is blocked during sending message.
 *
 * @dev: USART instance.
 * @pld: Pointer to payload data.
 * @size: Size of payload data.
 *
 * Returns: 0 - success; -EBFOV - construction of HDLC message failed;
 *          -EDMA - dma error.
 */
int usart_tx_hdlc_mesg(usart dev, uint8_t *pld, int size);

/**
 * usart_rx_hdlc_mesg
 *
 * Receive HDLC formated raw message via USART instance.
 * Caller task is blocked until message is not received or timeout is expired.
 *
 * @dev: USART instance.
 * @tmo: Timeout in tick periods.
 *
 * Returns: struct hdlc_mesg * - message successfully received;
 *          NULL - timeout.
 */
struct hdlc_mesg *usart_rx_hdlc_mesg(usart dev, TickType_t tmo);
#endif

#if USART_ADR_HDLC == 1
/**
 * usart_tx_adr_hdlc_mesg
 *
 * Create HDLC message from payload data and transmit it via USART instance.
 * Caller task is blocked during sending message.
 *
 * @dev: USART instance.
 * @pld: Pointer to payload data.
 * @size: Size of payload data.
 * @adr: Recipient address.
 *
 * Returns: 0 - success; -EBFOV - construction of HDLC message failed;
 *          -EDMA - dma error.
 */
int usart_tx_adr_hdlc_mesg(usart dev, uint8_t *pld, int size, uint8_t adr);

/**
 * usart_rx_adr_hdlc_mesg
 *
 * Receive HDLC formated raw message via USART instance.
 * Caller task is blocked until message is not received or timeout is
 * expired.
 *
 * @dev: USART instance.
 * @tmo: Timeout in tick periods.
 *
 * Returns: struct hdlc_mesg * - message successfully received;
 *          NULL - timeout.
 */
struct hdlc_mesg *usart_rx_adr_hdlc_mesg(usart dev, TickType_t tmo);
#endif

#if USART_ADR_CHAR == 1
/**
 * usart_tx_adr_buff
 *
 * Transmit data buffer via USART instance (first character is marked
 * as address). Caller task is blocked during sending data.
 *
 * @dev: USART instance.
 * @p_buf: Pointer to data buffer (bytes or half-words (MODE9)).
 * @size: Number of units to send.
 *
 * Returns: 0 - success; -EDMA - dma error.
 */
int usart_tx_adr_buff(usart dev, void *p_buf, int size);

/**
 * usart_rx_adr_char
 *
 * Receive 5-9 bit char via USART instance.
 * Caller task is blocked until char is not received.
 *
 * @dev: USART instance.
 * @p_char: Pointer to byte or half-word memory for store received char.
 * @p_adr: Function set *p_adr to TRUE if char marked as address is received.
 * @tmo: Timeout in tick periods.
 *
 * Returns: 0 - success; -ERCV - serial line error;
 *          -ETMO - no byte received in tmo time.
 */
int usart_rx_adr_char(usart dev, void *p_char, boolean_t *p_adr, TickType_t tmo);
#endif

#if USART_YIT == 1
/**
 * usart_rcv_yit_cmd
 *
 * Receive Yitran cmd message via USART instance.
 * Caller task is blocked until message is not received or timeout is
 * expired.
 *
 * @dev: USART instance.
 * @tmo: Timeout in tick periods.
 *
 * Returns: struct yit_cmd * - message successfully received;
 *          NULL - timeout.
 */
struct yit_cmd *usart_rcv_yit_cmd(void *dev, TickType_t tmo);

/**
 * usart_rst_yit_drv
 *
 * Reset Yitran driver state.
 *
 * @dev: USART instance.
 */
void usart_rst_yit_drv(void *dev);

/**
 * usart_free_yit_cmd_num
 *
 * Return number of free yit_cmd structures.
 *
 * @dev: USART instance.
 */
int usart_free_yit_cmd_num(void *dev);
#endif

#if USART_RX_CHAR == 1 || USART_HDLC == 1 || USART_ADR_HDLC == 1  || USART_ADR_CHAR == 1 ||\
    USART_YIT == 1
/**
 * usart_get_dev
 *
 * @id: USART instance id.
 *
 * Returns: USART device.
 */
usart usart_get_dev(int id);
#endif

#endif

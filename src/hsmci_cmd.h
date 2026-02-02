/*
 * hsmci_cmd.h
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

#ifndef HSMCI_CMD_H
#define HSMCI_CMD_H

/**
 * \name Macros for command definition
 *
 * Commands types:
 * - broadcast commands (bc), no response
 * - broadcast commands with response (bcr) (Note: No open drain on SD card)
 * - addressed (point-to-point) commands (ac), no data transfer on DAT lines
 * - addressed (point-to-point) data transfer commands (adtc), data transfer
 *   on DAT lines
 *
 ***************************************
 * Responses types:
 *
 * R1, R3, R4 & R5 use a 48 bits response protected by a 7bit CRC checksum
 * - R1 receiv data not specified
 * - R3 receiv OCR
 * - R4, R5 RCA management (MMC only)
 * - R6, R7 RCA management (SD only)
 *
 * R1b assert the BUSY signal and respond with R1.
 * If the busy signal is asserted, it is done two clock cycles (Nsr time)
 * after the end bit of the command. The DAT0 line is driven low.
 * DAT1-DAT7 lines are driven by the card though their values are not relevant.
 *
 * R2 use a 136 bits response protected by a 7bit CRC checksum.
 * The content is CID or CSD.
 *
 * R6 (Published RCA) return RCA.
 * R7 (Card interface condition) return RCA null.
 */

// Flags used to define a SD/MMC/SDIO command
#define SDMMC_CMD_GET_INDEX(cmd) (cmd & 0x3F)
// Have response (MCI only)
#define SDMMC_RESP_PRESENT      (1lu << 8)
// 136 bit response (MCI only)
#define SDMMC_RESP_136          (1lu << 11)
// Expect valid crc (MCI only)
#define SDMMC_RESP_CRC          (1lu << 12)
// Card may send busy
#define SDMMC_RESP_BUSY         (1lu << 13)
// Open drain for a braodcast command (bc)
// or to enter in inactive state (MCI only)
#define SDMMC_CMD_OPENDRAIN     (1lu << 14)
// To signal a data write operation
#define SDMMC_CMD_WRITE         (1lu << 15)
// To signal a SDIO tranfer in multi byte mode
#define SDMMC_CMD_SDIO_BYTE     (1lu << 16)
// To signal a SDIO tranfer in block mode
#define SDMMC_CMD_SDIO_BLOCK    (1lu << 17)
// To signal a data transfer in stream mode
#define SDMMC_CMD_STREAM        (1lu << 18)
// To signal a data transfer in single block mode
#define SDMMC_CMD_SINGLE_BLOCK  (1lu << 19)
// To signal a data transfer in multi block mode
#define SDMMC_CMD_MULTI_BLOCK   (1lu << 20)

// Set of flags to define a reponse type
#define SDMMC_CMD_NO_RESP (0)
#define SDMMC_CMD_R1      (SDMMC_RESP_PRESENT | SDMMC_RESP_CRC)
#define SDMMC_CMD_R1B     (SDMMC_RESP_PRESENT | SDMMC_RESP_CRC | SDMMC_RESP_BUSY)
#define SDMMC_CMD_R2      (SDMMC_RESP_PRESENT | SDMMC_RESP_136 | SDMMC_RESP_CRC)
#define SDMMC_CMD_R3      (SDMMC_RESP_PRESENT)
#define SDMMC_CMD_R4      (SDMMC_RESP_PRESENT)
#define SDMMC_CMD_R5      (SDMMC_RESP_PRESENT | SDMMC_RESP_CRC)
#define SDMMC_CMD_R6      (SDMMC_RESP_PRESENT | SDMMC_RESP_CRC)
#define SDMMC_CMD_R7      (SDMMC_RESP_PRESENT | SDMMC_RESP_CRC)

/*
 * --- Basic commands and read-stream command (class 0 and class 1) ---
 */

/** Cmd0(bc): Reset all cards to idle state */
#define SDMMC_MCI_CMD0_GO_IDLE_STATE     (0 | SDMMC_CMD_NO_RESP | SDMMC_CMD_OPENDRAIN)
/** Cmd2(bcr, R2): Ask the card to send its CID number (stuff but arg 0 used) */
#define SDMMC_CMD2_ALL_SEND_CID          (2 | SDMMC_CMD_R2 | SDMMC_CMD_OPENDRAIN)
/** Cmd3(bcr, R6): Ask the card to publish a new relative address (RCA) */
#define SD_CMD3_SEND_RELATIVE_ADDR       (3 | SDMMC_CMD_R6 | SDMMC_CMD_OPENDRAIN)
/** Cmd4(bc): Program the DSR of all cards (MCI only) */
#define SDMMC_CMD4_SET_DSR               (4 | SDMMC_CMD_NO_RESP)
/** Cmd7(ac, R1/R1b): Select/Deselect card */
#define SDMMC_CMD7_SELECT_CARD_CMD       (7 | SDMMC_CMD_R1B)
#define SDMMC_CMD7_DESELECT_CARD_CMD     (7 | SDMMC_CMD_R1)
/** Cmd8(bcr, R7) : Send SD Memory Card interface condition */
#define SD_CMD8_SEND_IF_COND             (8 | SDMMC_CMD_R7 | SDMMC_CMD_OPENDRAIN)
/** Cmd9 MCI (ac, R2): Addressed card sends its card-specific data (CSD) */
#define SDMMC_MCI_CMD9_SEND_CSD          (9 | SDMMC_CMD_R2)
/** Cmd10(ac, R2): Addressed card sends its card identification (CID) */
#define SDMMC_CMD10_SEND_CID             (10 | SDMMC_CMD_R2)
/** Cmd11 MCI (ac, R1): Voltage switching */
#define SD_CMD11_VOLTAGE_SWITCH          (11 | SDMMC_CMD_R1)
/** Cmd12(ac, R1b): Force the card to stop transmission */
#define SDMMC_CMD12_STOP_TRANSMISSION    (12 | SDMMC_CMD_R1B)
/** Cmd13(ac, R1): Addressed card sends its status register. */
#define SDMMC_MCI_CMD13_SEND_STATUS      (13 | SDMMC_CMD_R1)
/** Cmd15(ac): Send an addressed card into the Inactive State. */
// Note: It is a ac cmd, but it must be send like bc cmd to open drain
#define SDMMC_CMD15_GO_INACTIVE_STATE    (15 | SDMMC_CMD_NO_RESP | SDMMC_CMD_OPENDRAIN)

/*
 * --- Block-oriented read commands (class 2) ---
 */

/** Cmd16(ac, R1): Set the block length (in bytes) */
#define SDMMC_CMD16_SET_BLOCKLEN         (16 | SDMMC_CMD_R1)
/** Cmd17(adtc, R1): Read single block */
#define SDMMC_CMD17_READ_SINGLE_BLOCK    (17 | SDMMC_CMD_R1 | SDMMC_CMD_SINGLE_BLOCK)
/** Cmd18(adtc, R1): Read multiple block */
#define SDMMC_CMD18_READ_MULTIPLE_BLOCK  (18 | SDMMC_CMD_R1 | SDMMC_CMD_MULTI_BLOCK)

/*
 * --- Block-oriented write commands (class 4) ---
 */

/** Cmd24(adtc, R1): Write block */
#define SDMMC_CMD24_WRITE_BLOCK          (24 | SDMMC_CMD_R1 | SDMMC_CMD_WRITE | SDMMC_CMD_SINGLE_BLOCK)
/** Cmd25(adtc, R1): Write multiple block */
#define SDMMC_CMD25_WRITE_MULTIPLE_BLOCK (25 | SDMMC_CMD_R1 | SDMMC_CMD_WRITE | SDMMC_CMD_MULTI_BLOCK)
/** Cmd27(adtc, R1): Programming of the programmable bits of the CSD. */
#define SDMMC_CMD27_PROGRAM_CSD          (27 | SDMMC_CMD_R1)

/*
 * --- Erase commands  (class 5) ---
 */

/** Cmd32(ac, R1): */
#define SD_CMD32_ERASE_WR_BLK_START      (32 | SDMMC_CMD_R1)
/** Cmd33(ac, R1): */
#define SD_CMD33_ERASE_WR_BLK_END        (33 | SDMMC_CMD_R1)
/** Cmd38(ac, R1B): */
#define SDMMC_CMD38_ERASE                (38 | SDMMC_CMD_R1B)

/*
 * --- Block Oriented Write Protection Commands (class 6) ---
 */

/** Cmd28(ac, R1b): Set write protection */
#define SDMMC_CMD28_SET_WRITE_PROT       (28 | SDMMC_CMD_R1B)
/** Cmd29(ac, R1b): Clr write protection */
#define SDMMC_CMD29_CLR_WRITE_PROT       (29 | SDMMC_CMD_R1B)
/** Cmd30(adtc, R1b): Send write protection */
#define SDMMC_CMD30_SEND_WRITE_PROT      (30 | SDMMC_CMD_R1)

/*
 * --- Lock Card (class 7) ---
 */

/** Cmd42(adtc, R1): Used to set/reset the password or lock/unlock the card. */
#define SDMMC_CMD42_LOCK_UNLOCK          (42 | SDMMC_CMD_R1)

/*
 * --- Application-specific commands (class 8) ---
 */

/**
 * Cmd55(ac, R1): Indicate to the card that the next command is an application
 * specific command rather than a standard command.
 */
#define SDMMC_CMD55_APP_CMD              (55 | SDMMC_CMD_R1)
/**
 * Cmd 56(adtc, R1): Used either to transfer a data block to the card or to get
 * a data block from the card for general purpose/application specific commands.
 */
#define SDMMC_CMD56_GEN_CMD              (56 | SDMMC_CMD_R1)
/**
 * Cmd6(adtc, R1) : Check switchable function (mode 0)
 * and switch card function (mode 1).
 */
#define SD_CMD6_SWITCH_FUNC              (6 | SDMMC_CMD_R1 | SDMMC_CMD_SINGLE_BLOCK)
/** ACMD6(ac, R1): Define the data bus width */
#define SD_ACMD6_SET_BUS_WIDTH           (6 | SDMMC_CMD_R1)
/** ACMD13(adtc, R1): Send the SD Status. */
#define SD_ACMD13_SD_STATUS              (13 | SDMMC_CMD_R1)
/**
 * ACMD22(adtc, R1): Send the number of the written (with-out errors) write
 * blocks.
 */
#define SD_ACMD22_SEND_NUM_WR_BLOCKS     (22 | SDMMC_CMD_R1)
/**
 * ACMD23(ac, R1): Set the number of write blocks to be pre-erased before
 * writing
 */
#define SD_ACMD23_SET_WR_BLK_ERASE_COUNT (23 | SDMMC_CMD_R1)
/**
 * ACMD41(bcr, R3): Send host capacity support information (HCS) and asks the
 * accessed card to send its operating condition register (OCR) content
 * in the response
 */
#define SD_MCI_ACMD41_SD_SEND_OP_COND    (41 | SDMMC_CMD_R3 | SDMMC_CMD_OPENDRAIN)
/**
 * ACMD42(ac, R1): Connect[1]/Disconnect[0] the 50 KOhm pull-up resistor on
 * CD/DAT3 (pin 1) of the card.
 */
#define SD_ACMD42_SET_CLR_CARD_DETECT    (42 | SDMMC_CMD_R1)
/** ACMD51(adtc, R1): Read the SD Configuration Register (SCR). */
#define SD_ACMD51_SEND_SCR               (51 | SDMMC_CMD_R1 | SDMMC_CMD_SINGLE_BLOCK)

// Macros for command argument definition

// SD CMD6 argument structure
// CMD6 arg[ 3: 0] function group 1, access mode
#define SD_CMD6_GRP1_HIGH_SPEED     (0x1lu << 0)
#define SD_CMD6_GRP1_DEFAULT        (0x0lu << 0)
// CMD6 arg[ 7: 4] function group 2, command system
#define SD_CMD6_GRP2_NO_INFLUENCE   (0xFlu << 4)
#define SD_CMD6_GRP2_DEFAULT        (0x0lu << 4)
// CMD6 arg[11: 8] function group 3, 0xF or 0x0
#define SD_CMD6_GRP3_NO_INFLUENCE   (0xFlu << 8)
#define SD_CMD6_GRP3_DEFAULT        (0x0lu << 8)
// CMD6 arg[15:12] function group 4, 0xF or 0x0
#define SD_CMD6_GRP4_NO_INFLUENCE   (0xFlu << 12)
#define SD_CMD6_GRP4_DEFAULT        (0x0lu << 12)
// CMD6 arg[19:16] function group 5, 0xF or 0x0
#define SD_CMD6_GRP5_NO_INFLUENCE   (0xFlu << 16)
#define SD_CMD6_GRP5_DEFAULT        (0x0lu << 16)
// CMD6 arg[23:20] function group 6, 0xF or 0x0
#define SD_CMD6_GRP6_NO_INFLUENCE   (0xFlu << 20)
#define SD_CMD6_GRP6_DEFAULT        (0x0lu << 20)
// CMD6 arg[30:24] reserved 0
// CMD6 arg[31   ] Mode, 0: Check, 1: Switch
#define SD_CMD6_MODE_CHECK          (0lu << 31)
#define SD_CMD6_MODE_SWITCH         (1lu << 31)

// SD CMD8 argument structure
#define SD_CMD8_PATTERN       0xAA
#define SD_CMD8_MASK_PATTERN  0xFF
#define SD_CMD8_HIGH_VOLTAGE  0x100
#define SD_CMD8_MASK_VOLTAGE  0xF00

// SD ACMD41 arguments
#define SD_ACMD41_HCS   (1lu << 30) //< (SD) Host Capacity Support

// CSD, OCR, SCR, Switch status, extend CSD definitions

/**
 * Macro function to extract a bits field from a large SD MMC register
 * Used by : CSD, SCR, Switch status
 */
static inline uint32_t SDMMC_UNSTUFF_BITS(uint8_t *reg, uint16_t reg_size, uint16_t pos, uint8_t size)
{
	uint32_t value;
	uint32_t mask;

	value = reg[((reg_size - pos + 7) / 8) - 1] >> (pos % 8);
	if (((pos % 8) + size) > 8) {
		value |= (uint32_t) reg[((reg_size - pos + 7) / 8) - 2] << (8 - (pos % 8));
	}
	if (((pos % 8) + size) > 16) {
		value |= (uint32_t) reg[((reg_size - pos + 7) / 8) - 3] << (16 - (pos % 8));
	}
	if (size >= 32) {
		mask = 0xFFFFFFFFu;
	} else {
		mask = ((uint32_t) 1U << size) - 1U;
	}
	value &= mask;
	return value;
}

// CSD Fields
#define CSD_REG_BIT_SIZE            128 //< 128 bits
#define CSD_REG_BSIZE               (CSD_REG_BIT_SIZE / 8) //< 16 bytes
#define CSD_STRUCTURE(csd, pos, size) SDMMC_UNSTUFF_BITS(csd, CSD_REG_BIT_SIZE, pos, size)
#define CSD_STRUCTURE_VERSION(csd)  CSD_STRUCTURE(csd, 126, 2)
#define SD_CSD_VER_1_0              0
#define SD_CSD_VER_2_0              1
#define CSD_TRAN_SPEED(csd)         CSD_STRUCTURE(csd, 96, 8)
#define SD_CSD_1_0_C_SIZE(csd)      CSD_STRUCTURE(csd, 62, 12)
#define SD_CSD_1_0_C_SIZE_MULT(csd) CSD_STRUCTURE(csd, 47, 3)
#define SD_CSD_1_0_READ_BL_LEN(csd) CSD_STRUCTURE(csd, 80, 4)
#define SD_CSD_2_0_C_SIZE(csd)      CSD_STRUCTURE(csd, 48, 22)

// OCR Register Fields
#define OCR_REG_BSIZE          (32 / 8)    /**< 32 bits, 4 bytes */
#define OCR_VDD_170_195        (1lu << 7)
#define OCR_VDD_20_21          (1lu << 8)
#define OCR_VDD_21_22          (1lu << 9)
#define OCR_VDD_22_23          (1lu << 10)
#define OCR_VDD_23_24          (1lu << 11)
#define OCR_VDD_24_25          (1lu << 12)
#define OCR_VDD_25_26          (1lu << 13)
#define OCR_VDD_26_27          (1lu << 14)
#define OCR_VDD_27_28          (1lu << 15)
#define OCR_VDD_28_29          (1lu << 16)
#define OCR_VDD_29_30          (1lu << 17)
#define OCR_VDD_30_31          (1lu << 18)
#define OCR_VDD_31_32          (1lu << 19)
#define OCR_VDD_32_33          (1lu << 20)
#define OCR_VDD_33_34          (1lu << 21)
#define OCR_VDD_34_35          (1lu << 22)
#define OCR_VDD_35_36          (1lu << 23)
#define OCR_CCS                (1lu << 30) /**< (SD) Card Capacity Status */
#define OCR_POWER_UP_BUSY      (1lu << 31) /**< Card power up status bit */

// SD SCR Register Fields
#define SD_SCR_REG_BIT_SIZE    64 //< 64 bits
#define SD_SCR_REG_BSIZE       (SD_SCR_REG_BIT_SIZE / 8) //< 8 bytes
#define SD_SCR_STRUCTURE(scr, pos, size) SDMMC_UNSTUFF_BITS(scr, SD_SCR_REG_BIT_SIZE, pos, size)
#define SD_SCR_SCR_STRUCTURE(scr)            SD_SCR_STRUCTURE(scr, 60, 4)
#define SD_SCR_SCR_STRUCTURE_1_0             0
#define SD_SCR_SD_SPEC(scr)                  SD_SCR_STRUCTURE(scr, 56, 4)
#define SD_SCR_SD_SPEC_1_0_01                0
#define SD_SCR_SD_SPEC_1_10                  1
#define SD_SCR_SD_SPEC_2_00                  2
#define SD_SCR_DATA_STATUS_AFTER_ERASE(scr)  SD_SCR_STRUCTURE(scr, 55, 1)
#define SD_SCR_SD_SECURITY(scr)              SD_SCR_STRUCTURE(scr, 52, 3)
#define SD_SCR_SD_SECURITY_NO                0
#define SD_SCR_SD_SECURITY_NOTUSED           1
#define SD_SCR_SD_SECURITY_1_01              2
#define SD_SCR_SD_SECURITY_2_00              3
#define SD_SCR_SD_SECURITY_3_00              4
#define SD_SCR_SD_BUS_WIDTHS(scr)            SD_SCR_STRUCTURE(scr, 48, 4)
#define SD_SCR_SD_BUS_WIDTH_1BITS            (1lu << 0)
#define SD_SCR_SD_BUS_WIDTH_4BITS            (1lu << 2)
#define SD_SCR_SD_SPEC3(scr)                 SD_SCR_STRUCTURE(scr, 47, 1)
#define SD_SCR_SD_SPEC_3_00                  1
#define SD_SCR_SD_EX_SECURITY(scr)           SD_SCR_STRUCTURE(scr, 43, 4)
#define SD_SCR_SD_CMD_SUPPORT(scr)           SD_SCR_STRUCTURE(scr, 32, 2)

// SD Switch Status Fields
#define SD_SW_STATUS_BIT_SIZE   512 //< 512 bits
#define SD_SW_STATUS_BSIZE      (SD_SW_STATUS_BIT_SIZE / 8) //< 64 bytes
#define SD_SW_STATUS_STRUCTURE(sd_sw_status, pos, size) \
		SDMMC_UNSTUFF_BITS(sd_sw_status, SD_SW_STATUS_BIT_SIZE, pos, size)
#define SD_SW_STATUS_MAX_CURRENT_CONSUMPTION(status) \
		SD_SW_STATUS_STRUCTURE(status, 496, 16)
#define SD_SW_STATUS_FUN_GRP6_INFO(status) \
		SD_SW_STATUS_STRUCTURE(status, 480, 16)
#define SD_SW_STATUS_FUN_GRP5_INFO(status) \
		SD_SW_STATUS_STRUCTURE(status, 464, 16)
#define SD_SW_STATUS_FUN_GRP4_INFO(status) \
		SD_SW_STATUS_STRUCTURE(status, 448, 16)
#define SD_SW_STATUS_FUN_GRP3_INFO(status) \
		SD_SW_STATUS_STRUCTURE(status, 432, 16)
#define SD_SW_STATUS_FUN_GRP2_INFO(status) \
		SD_SW_STATUS_STRUCTURE(status, 416, 16)
#define SD_SW_STATUS_FUN_GRP1_INFO(status) \
		SD_SW_STATUS_STRUCTURE(status, 400, 16)
#define SD_SW_STATUS_FUN_GRP6_RC(status) \
		SD_SW_STATUS_STRUCTURE(status, 396, 4)
#define SD_SW_STATUS_FUN_GRP5_RC(status) \
		SD_SW_STATUS_STRUCTURE(status, 392, 4)
#define SD_SW_STATUS_FUN_GRP4_RC(status) \
		SD_SW_STATUS_STRUCTURE(status, 388, 4)
#define SD_SW_STATUS_FUN_GRP3_RC(status) \
		SD_SW_STATUS_STRUCTURE(status, 384, 4)
#define SD_SW_STATUS_FUN_GRP2_RC(status) \
		SD_SW_STATUS_STRUCTURE(status, 380, 4)
#define SD_SW_STATUS_FUN_GRP1_RC(status) \
		SD_SW_STATUS_STRUCTURE(status, 376, 4)
#define SD_SW_STATUS_FUN_GRP_RC_ERROR   0xFU
#define SD_SW_STATUS_DATA_STRUCT_VER(status) \
		SD_SW_STATUS_STRUCTURE(status, 368, 8)
#define SD_SW_STATUS_FUN_GRP6_BUSY(status) \
		SD_SW_STATUS_STRUCTURE(status, 352, 16)
#define SD_SW_STATUS_FUN_GRP5_BUSY(status) \
		SD_SW_STATUS_STRUCTURE(status, 336, 16)
#define SD_SW_STATUS_FUN_GRP4_BUSY(status) \
		SD_SW_STATUS_STRUCTURE(status, 320, 16)
#define SD_SW_STATUS_FUN_GRP3_BUSY(status) \
		SD_SW_STATUS_STRUCTURE(status, 304, 16)
#define SD_SW_STATUS_FUN_GRP2_BUSY(status) \
		SD_SW_STATUS_STRUCTURE(status, 288, 16)
#define SD_SW_STATUS_FUN_GRP1_BUSY(status) \
		SD_SW_STATUS_STRUCTURE(status, 272, 16)

// Card Status Fields
#define CARD_STATUS_APP_CMD           (1lu << 5)
#define CARD_STATUS_SWITCH_ERROR      (1lu << 7)
#define CARD_STATUS_READY_FOR_DATA    (1lu << 8)
#define CARD_STATUS_STATE_IDLE        (0lu << 9)
#define CARD_STATUS_STATE_READY       (1lu << 9)
#define CARD_STATUS_STATE_IDENT       (2lu << 9)
#define CARD_STATUS_STATE_STBY        (3lu << 9)
#define CARD_STATUS_STATE_TRAN        (4lu << 9)
#define CARD_STATUS_STATE_DATA        (5lu << 9)
#define CARD_STATUS_STATE_RCV         (6lu << 9)
#define CARD_STATUS_STATE_PRG         (7lu << 9)
#define CARD_STATUS_STATE_DIS         (8lu << 9)
#define CARD_STATUS_STATE             (0xFlu << 9)
#define CARD_STATUS_ERASE_RESET       (1lu << 13)
#define CARD_STATUS_WP_ERASE_SKIP     (1lu << 15)
#define CARD_STATUS_CIDCSD_OVERWRITE  (1lu << 16)
#define CARD_STATUS_OVERRUN           (1lu << 17)
#define CARD_STATUS_UNERRUN           (1lu << 18)
#define CARD_STATUS_ERROR             (1lu << 19)
#define CARD_STATUS_CC_ERROR          (1lu << 20)
#define CARD_STATUS_CARD_ECC_FAILED   (1lu << 21)
#define CARD_STATUS_ILLEGAL_COMMAND   (1lu << 22)
#define CARD_STATUS_COM_CRC_ERROR     (1lu << 23)
#define CARD_STATUS_UNLOCK_FAILED     (1lu << 24)
#define CARD_STATUS_CARD_IS_LOCKED    (1lu << 25)
#define CARD_STATUS_WP_VIOLATION      (1lu << 26)
#define CARD_STATUS_ERASE_PARAM       (1lu << 27)
#define CARD_STATUS_ERASE_SEQ_ERROR   (1lu << 28)
#define CARD_STATUS_BLOCK_LEN_ERROR   (1lu << 29)
#define CARD_STATUS_ADDRESS_MISALIGN  (1lu << 30)
#define CARD_STATUS_ADDR_OUT_OF_RANGE (1lu << 31)

#define CARD_STATUS_ERR_RD_WR (CARD_STATUS_ADDR_OUT_OF_RANGE \
		| CARD_STATUS_ADDRESS_MISALIGN \
		| CARD_STATUS_BLOCK_LEN_ERROR \
		| CARD_STATUS_WP_VIOLATION \
		| CARD_STATUS_ILLEGAL_COMMAND \
		| CARD_STATUS_CC_ERROR \
		| CARD_STATUS_ERROR \
		| CARD_STATUS_CARD_ECC_FAILED \
		| CARD_STATUS_CARD_IS_LOCKED)

// SD Status Field
#define SD_STATUS_BSIZE    (512 / 8)  /**< 512 bits, 64bytes */

// R6 (Published RCA) helper macros
// R6 response is returned in resp.r1:
//   [31:16] RCA
//   [15:0]  Status field (subset of card status)
#define SD_R6_STATUS_BIT_SIZE   16
#define SD_R6_GET_RCA(r6)       ((uint16_t) ((r6) >> 16))
#define SD_R6_GET_STATUS(r6)    ((uint16_t) ((r6) & 0xFFFFU))

// R6 status field bits used during CMD3 (SEND_RELATIVE_ADDR)
#define SD_R6_STATUS_COM_CRC_ERROR    (1 << 2)
#define SD_R6_STATUS_ERROR            (1 << 13)
#define SD_R6_STATUS_ILLEGAL_COMMAND  (1 << 14)
#define SD_R6_STATUS_AKE_SEQ_ERROR    (1 << 15)

#define SD_R6_STATUS_ERR_MASK ( \
		SD_R6_STATUS_AKE_SEQ_ERROR | \
		SD_R6_STATUS_ILLEGAL_COMMAND | \
		SD_R6_STATUS_ERROR | \
		SD_R6_STATUS_COM_CRC_ERROR)

#endif

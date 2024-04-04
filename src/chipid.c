/*
 * chipid.c
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
#include "tools.h"
#include "chipid.h"

#if CHIP_ID == 1

#if TERMOUT == 1
static const char *const na = "n/a";

enum dev {
	// SAM3N
	ATSAM3N4C   = 0x2954096,
        ATSAM3N2C   = 0x2959076,
        ATSAM3N1C   = 0x2958056,
	ATSAM3N4B   = 0x2944096,
        ATSAM3N2B   = 0x2949076,
        ATSAM3N1B   = 0x2948056,
        ATSAM3N4A   = 0x2934096,
        ATSAM3N2A   = 0x2939076,
        ATSAM3N1A   = 0x2938056,
        ATSAM3N0C   = 0x2958036,
        ATSAM3N0B   = 0x2948036,
        ATSAM3N0A   = 0x2938036,
        ATSAM3N00B  = 0x2945026,
        ATSAM3N00A  = 0x2935026,
	// SAM3S
        ATSAM3S4A   = 0x2880096,
        ATSAM3S2A   = 0x288A076,
        ATSAM3S1A   = 0x2889056,
        ATSAM3S4B   = 0x2890096,
        ATSAM3S2B   = 0x289A076,
        ATSAM3S1B   = 0x2899056,
        ATSAM3S4C   = 0x28A0096,
        ATSAM3S2C   = 0x28AA076,
        ATSAM3S1C   = 0x28A9056,
	ATSAM3S8A   = 0x288B0A6,
        ATSAM3S8B   = 0x289B0A6,
        ATSAM3S8C   = 0x28AB0A6,
        ATSAM3SD8A  = 0x298B0A6,
        ATSAM3SD8B  = 0x299B0A6,
        ATSAM3SD8C  = 0x29AB0A6,
	// SAM4N
        ATSAM4N16B  = 0x29460CE,
        ATSAM4N16C  = 0x29560CE,
        ATSAM4N8A   = 0x293B0AE,
        ATSAM4N8B   = 0x294B0AE,
        ATSAM4N8C   = 0x295B0AE,
	// SAM4S
	ATSAM4SD32C = 0x29A70EE,
        ATSAM4SD32B = 0x29970EE,
        ATSAM4SD16C = 0x29A70CE,
        ATSAM4SD16B = 0x29970CE,
	ATSAM4SA16C = 0x28A70CE,
        ATSAM4SA16B = 0x28970CE,
        ATSAM4S16B  = 0x289C0CE,
        ATSAM4S16C  = 0x28AC0CE,
        ATSAM4S8B   = 0x289C0AE,
        ATSAM4S8C   = 0x28AC0AE,
        ATSAM4S4C   = 0x28AB09E,
        ATSAM4S4B   = 0x289B09E,
        ATSAM4S4A   = 0x288B09E,
        ATSAM4S2C   = 0x28AB07E,
        ATSAM4S2B   = 0x289B07E,
        ATSAM4S2A   = 0x288B07E
};

static const struct txt_item dev_dsc_ary[] = {
	// SAM3N
	{ATSAM3N4C, "3N4C"},
        {ATSAM3N2C, "3N2C"},
        {ATSAM3N1C, "3N1C"},
	{ATSAM3N4B, "3N4B"},
        {ATSAM3N2B, "3N2B"},
        {ATSAM3N1B, "3N1B"},
        {ATSAM3N4A, "3N4A"},
        {ATSAM3N2A, "3N2A"},
        {ATSAM3N1A, "3N1A"},
        {ATSAM3N0C, "3N0C"},
        {ATSAM3N0B, "3N0B"},
        {ATSAM3N0A, "3N0A"},
        {ATSAM3N00B, "3N00B"},
        {ATSAM3N00A, "3N00A"},
	// SAM3S
        {ATSAM3S4A, "3S4A"},
        {ATSAM3S2A, "3S2A"},
        {ATSAM3S1A, "3S1A"},
        {ATSAM3S4B, "3S4B"},
        {ATSAM3S2B, "3S2B"},
        {ATSAM3S1B, "3S1B"},
        {ATSAM3S4C, "3S4C"},
        {ATSAM3S2C, "3S2C"},
        {ATSAM3S1C, "3S1C"},
	{ATSAM3S8A, "3S8A"},
        {ATSAM3S8B, "3S8B"},
        {ATSAM3S8C, "3S8C"},
        {ATSAM3SD8A, "3SD8A"},
        {ATSAM3SD8B, "3SD8B"},
        {ATSAM3SD8C, "3SD8C"},
	// SAM4N
        {ATSAM4N16B, "4N16B"},
        {ATSAM4N16C, "4N16C"},
        {ATSAM4N8A, "4N8A"},
        {ATSAM4N8B, "4N8B"},
        {ATSAM4N8C, "4N8C"},
	// SAM4S
	{ATSAM4SD32C, "4SD32C"},
        {ATSAM4SD32B, "4SD32B"},
        {ATSAM4SD16C, "4SD16C"},
        {ATSAM4SD16B, "4SD16B"},
	{ATSAM4SA16C, "4SA16C"},
        {ATSAM4SA16B, "4SA16B"},
        {ATSAM4S16B, "4S16B"},
        {ATSAM4S16C, "4S16C"},
        {ATSAM4S8B, "4S8B"},
        {ATSAM4S8C, "4S8C"},
        {ATSAM4S4C, "4S4C"},
        {ATSAM4S4B, "4S4B"},
        {ATSAM4S4A, "4S4A"},
        {ATSAM4S2C, "4S2C"},
        {ATSAM4S2B, "4S2B"},
        {ATSAM4S2A, "4S2A"},
        {0, NULL}
};

enum arch_cm3 {
	ARCH3N_48PIN  = 0x93,
        ARCH3N_64PIN  = 0x94,
        ARCH3N_100PIN = 0x95,
	ARCH3S_48PIN  = 0x88,
        ARCH3S_64PIN  = 0x89,
        ARCH3S_100PIN = 0x8A
};

static const struct txt_item arch_cm3_dsc_ary[] = {
	{ARCH3N_48PIN, "3N_48PIN"},
        {ARCH3N_64PIN, "3N_64PIN"},
        {ARCH3N_100PIN, "3N_100PIN"},
	{ARCH3S_48PIN, "3S_48PIN"},
        {ARCH3S_64PIN, "3S_64PIN"},
        {ARCH3S_100PIN, "3S_100PIN"},
	{0,  NULL}
};

enum arch_cm4 {
	ARCH4N_48PIN  = 0x93,
        ARCH4N_64PIN  = 0x94,
        ARCH4N_100PIN = 0x95,
	ARCH4S_48PIN  = 0x88,
        ARCH4S_64PIN  = 0x89,
        ARCH4S_100PIN = 0x8A
};

static const struct txt_item arch_cm4_dsc_ary[] = {
	{ARCH4N_48PIN, "4N_48PIN"},
        {ARCH4N_64PIN, "4N_64PIN"},
        {ARCH4N_100PIN, "4N_100PIN"},
	{ARCH4S_48PIN, "4S_48PIN"},
        {ARCH4S_64PIN, "4S_64PIN"},
        {ARCH4S_100PIN, "4S_100PIN"},
	{0, NULL}
};

enum cpu {
	EPROC_CM3 = 0x03,
	EPROC_CM4 = 0x07
};

static const struct txt_item cpu_dsc_ary[] = {
	{EPROC_CM3, "CM3"},
        {EPROC_CM4, "CM4"},
	{0, NULL}
};

enum sram_size {
	SRAMSIZ_48K  = 0x00,
	SRAMSIZ_192K = 0x01,
	SRAMSIZ_2K   = 0x02,
	SRAMSIZ_6K   = 0x03,
	SRAMSIZ_24K  = 0x04,
	SRAMSIZ_4K   = 0x05,
	SRAMSIZ_80K  = 0x06,
	SRAMSIZ_160K = 0x07,
	SRAMSIZ_8K   = 0x08,
	SRAMSIZ_16K  = 0x09,
	SRAMSIZ_32K  = 0x0A,
	SRAMSIZ_64K  = 0x0B,
	SRAMSIZ_128K = 0x0C,
	SRAMSIZ_256K = 0x0D,
	SRAMSIZ_96K  = 0x0E,
	SRAMSIZ_512K = 0x0F
};

static const struct txt_item sram_size_dsc_ary[] = {
	{SRAMSIZ_48K, "48"},
	{SRAMSIZ_192K, "192"},
	{SRAMSIZ_2K, "2"},
	{SRAMSIZ_6K, "6"},
	{SRAMSIZ_24K, "24"},
	{SRAMSIZ_4K, "4"},
	{SRAMSIZ_80K, "80"},
	{SRAMSIZ_160K, "160"},
	{SRAMSIZ_8K, "8"},
	{SRAMSIZ_16K, "16"},
	{SRAMSIZ_32K, "32"},
	{SRAMSIZ_64K, "64"},
	{SRAMSIZ_128K, "128"},
	{SRAMSIZ_256K, "256"},
	{SRAMSIZ_96K, "96"},
	{SRAMSIZ_512K, "512"},
	{0, NULL}
};

enum flash_size {
	NVPSIZ_0K    = 0x00,
	NVPSIZ_8K    = 0x01,
	NVPSIZ_16K   = 0x02,
	NVPSIZ_32K   = 0x03,
	NVPSIZ_64K   = 0x05,
	NVPSIZ_128K  = 0x07,
	NVPSIZ_256K  = 0x09,
	NVPSIZ_512K  = 0x0A,
	NVPSIZ_1024K = 0x0C,
	NVPSIZ_2048K = 0x0E
};

static const struct txt_item flash_size_dsc_ary[] = {
	{NVPSIZ_0K, "0"},
	{NVPSIZ_8K, "8"},
	{NVPSIZ_16K, "16"},
	{NVPSIZ_32K, "32"},
	{NVPSIZ_64K, "64"},
	{NVPSIZ_128K, "128"},
	{NVPSIZ_256K, "256"},
	{NVPSIZ_512K, "512"},
	{NVPSIZ_1024K, "1024"},
	{NVPSIZ_2048K, "2048"},
	{0, NULL}
};

static const char *dev(void);
static char *rev(void);
static const char *arch(void);
static const char *cpu(void);
static const char *sram(void);
static const char *flash(void);
#endif

/**
 * get_chipid
 */
unsigned int get_chipid(void)
{
	return (CHIPID->CHIPID_CIDR);
}

/**
 * get_chipid_ext
 */
unsigned int get_chipid_ext(void)
{
	return (CHIPID->CHIPID_EXID);
}

#if TERMOUT == 1
/**
 * dev
 */
static const char *dev(void)
{
	const char *p;

	p = find_txt_item(CHIPID->CHIPID_CIDR >> 4, dev_dsc_ary, na);
	return (p);
}

/**
 * rev
 */
static char *rev(void)
{
	static char rev;

	rev = (CHIPID->CHIPID_CIDR & CHIPID_CIDR_VERSION_Msk) + 65;
	return (&rev);
}

/**
 * arch
 */
static const char *arch(void)
{
	const char *p = na;

	if ((CHIPID->CHIPID_CIDR & CHIPID_CIDR_EPROC_Msk) >> CHIPID_CIDR_EPROC_Pos == EPROC_CM3) {
		p = find_txt_item((CHIPID->CHIPID_CIDR & CHIPID_CIDR_ARCH_Msk) >> CHIPID_CIDR_ARCH_Pos,
				  arch_cm3_dsc_ary, na);
	} else if ((CHIPID->CHIPID_CIDR & CHIPID_CIDR_EPROC_Msk) >> CHIPID_CIDR_EPROC_Pos == EPROC_CM4) {
		p = find_txt_item((CHIPID->CHIPID_CIDR & CHIPID_CIDR_ARCH_Msk) >> CHIPID_CIDR_ARCH_Pos,
				  arch_cm4_dsc_ary, na);
	}
        return (p);
}

/**
 * cpu
 */
static const char *cpu(void)
{
	const char *p;

	p = find_txt_item((CHIPID->CHIPID_CIDR & CHIPID_CIDR_EPROC_Msk) >> CHIPID_CIDR_EPROC_Pos,
	                  cpu_dsc_ary, na);
	return (p);
}

/**
 * sram
 */
static const char *sram(void)
{
	const char *p;

	p = find_txt_item((CHIPID->CHIPID_CIDR & CHIPID_CIDR_SRAMSIZ_Msk) >> CHIPID_CIDR_SRAMSIZ_Pos,
	                  sram_size_dsc_ary, na);
	return (p);
}

/**
 * flash
 */
static const char *flash(void)
{
	const char *p = na;

	if ((CHIPID->CHIPID_CIDR & CHIPID_CIDR_NVPTYP_Msk) == CHIPID_CIDR_NVPTYP_FLASH) {
		p = find_txt_item((CHIPID->CHIPID_CIDR & CHIPID_CIDR_NVPSIZ_Msk) >> CHIPID_CIDR_NVPSIZ_Pos,
				  flash_size_dsc_ary, na);
	}
	return (p);
}

/**
 * log_chipid
 */
void log_chipid(void)
{
	msg(INF, "chipid.c: Dev=ATSAM%s Rev=%s Arch=%s Cpu=%s Sram=%sK Flash=%sK\n",
	    dev(), rev(), arch(), cpu(), sram(), flash());
}
#endif

#endif

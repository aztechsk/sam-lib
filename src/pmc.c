/*
 * pmc.c
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

#define PMC_FSMR_LPM_XXX (0x1u << 20)

extern inline void wait_fast_rc_osc_enabled(void);
#if SAM4_SERIES
extern inline void enter_pmc_wait_lpm(void);
#endif

unsigned int SystemCoreClock = CHIP_FREQ_MAINCK_RC_4MHZ;

/**
 * enable_periph_clk
 */
void enable_periph_clk(int id)
{
#if SAM3S_SERIES || SAM4S_SERIES
	taskENTER_CRITICAL();
	if (id < 32) {
		if (!(PMC->PMC_PCSR0 & (1 << id))) {
			PMC->PMC_PCER0 = 1 << id;
		}
	} else {
		id -= 32;
		if (!(PMC->PMC_PCSR1 & (1 << id))) {
			PMC->PMC_PCER1 = 1 << id;
		}
	}
        taskEXIT_CRITICAL();
#elif SAM3N_SERIES || SAM4N_SERIES
	taskENTER_CRITICAL();
	if (!(PMC->PMC_PCSR0 & (1 << id))) {
		PMC->PMC_PCER0 = 1 << id;
	}
        taskEXIT_CRITICAL();
#else
 #error "SAM_SERIES definition error"
#endif
}

/**
 * disable_periph_clk
 */
void disable_periph_clk(int id)
{
#if SAM3S_SERIES || SAM4S_SERIES
	taskENTER_CRITICAL();
	if (id < 32) {
		if (PMC->PMC_PCSR0 & (1 << id)) {
			PMC->PMC_PCDR0 = 1 << id;
		}
	} else {
		id -= 32;
		if (PMC->PMC_PCSR1 & (1 << id)) {
			PMC->PMC_PCDR1 = 1 << id;
		}
	}
        taskEXIT_CRITICAL();
#elif SAM3N_SERIES || SAM4N_SERIES
	taskENTER_CRITICAL();
	if (PMC->PMC_PCSR0 & (1 << id)) {
		PMC->PMC_PCDR0 = 1 << id;
	}
        taskEXIT_CRITICAL();
#else
 #error "SAM_SERIES definition error"
#endif
}

/**
 * enable_periph_clk_nocs
 */
void enable_periph_clk_nocs(int id)
{
#if SAM3S_SERIES || SAM4S_SERIES
	if (id < 32) {
		if (!(PMC->PMC_PCSR0 & (1 << id))) {
			PMC->PMC_PCER0 = 1 << id;
		}
	} else {
		id -= 32;
		if (!(PMC->PMC_PCSR1 & (1 << id))) {
			PMC->PMC_PCER1 = 1 << id;
		}
	}
#elif SAM3N_SERIES || SAM4N_SERIES
	if (!(PMC->PMC_PCSR0 & (1 << id))) {
		PMC->PMC_PCER0 = 1 << id;
	}
#else
 #error "SAM_SERIES definition error"
#endif
}

/**
 * disable_periph_clk_nocs
 */
void disable_periph_clk_nocs(int id)
{
#if SAM3S_SERIES || SAM4S_SERIES
	if (id < 32) {
		if (PMC->PMC_PCSR0 & (1 << id)) {
			PMC->PMC_PCDR0 = 1 << id;
		}
	} else {
		id -= 32;
		if (PMC->PMC_PCSR1 & (1 << id)) {
			PMC->PMC_PCDR1 = 1 << id;
		}
	}
#elif SAM3N_SERIES || SAM4N_SERIES
	if (PMC->PMC_PCSR0 & (1 << id)) {
		PMC->PMC_PCDR0 = 1 << id;
	}
#else
 #error "SAM_SERIES definition error"
#endif
}

/**
 * get_act_periph_clks
 */
unsigned long long get_act_periph_clks(void)
{
#if SAM3S_SERIES || SAM4S_SERIES
	unsigned long long tmp;

	taskENTER_CRITICAL();
	tmp = PMC->PMC_PCSR1;
	tmp <<= 32;
	tmp |= PMC->PMC_PCSR0;
	taskEXIT_CRITICAL();
	return (tmp);
#elif SAM3N_SERIES || SAM4N_SERIES
	return (PMC->PMC_PCSR0);
#else
 #error "SAM_SERIES definition error"
#endif
}

/**
 * select_mast_clk_src
 */
void select_mast_clk_src(enum mast_clk_src mck_src, enum mast_clk_presc mck_presc)
{
	switch (mck_src) {
	case MCK_SRC_SLOW_CLK :
		/* FALLTHRU */
	case MCK_SRC_MAIN_CLK :
		PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk) | mck_src;
		while (!(PMC->PMC_SR & PMC_SR_MCKRDY)) {
			;
		}
		PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_PRES_Msk) |
		                mck_presc << PMC_MCKR_PRES_Pos;
		break;
	case MCK_SRC_PLLA_CLK :
#if SAM3S_SERIES || SAM4S_SERIES
		/* FALLTHRU */
	case MCK_SRC_PLLB_CLK :
#endif
		PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_PRES_Msk) |
		                mck_presc << PMC_MCKR_PRES_Pos;
		while (!(PMC->PMC_SR & PMC_SR_MCKRDY)) {
			;
		}
		PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk) | mck_src;
		break;
	}
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY)) {
		;
	}
}

/**
 * enable_fast_rc_osc
 */
void enable_fast_rc_osc(void)
{
        PMC->CKGR_MOR |= 0x37 << 16 | CKGR_MOR_MOSCRCEN;
	while (!(PMC->PMC_SR & PMC_SR_MOSCRCS)) {
		;
	}
}

/**
 * disable_fast_rc_osc
 */
void disable_fast_rc_osc(void)
{
	PMC->CKGR_MOR = 0x37 << 16 | (PMC->CKGR_MOR & ~CKGR_MOR_MOSCRCEN);
	while (PMC->PMC_SR & PMC_SR_MOSCRCS) {
		;
	}
}

/**
 * set_fast_rc_osc_freq
 */
void set_fast_rc_osc_freq(enum fast_rc_osc_freq freq)
{
	PMC->CKGR_MOR = 0x37 << 16 | (PMC->CKGR_MOR & ~CKGR_MOR_MOSCRCF_Msk) |
			freq << CKGR_MOR_MOSCRCF_Pos;
	while (!(PMC->PMC_SR & PMC_SR_MOSCRCS)) {
		;
	}
}

/**
 * enable_main_xtal_osc
 */
void enable_main_xtal_osc(int st_up_tm)
{
	PMC->CKGR_MOR = 0x37 << 16 | (PMC->CKGR_MOR & ~CKGR_MOR_MOSCXTBY & ~CKGR_MOR_MOSCXTST_Msk) |
			CKGR_MOR_MOSCXTST(st_up_tm) | CKGR_MOR_MOSCXTEN;
	while (!(PMC->PMC_SR & PMC_SR_MOSCXTS)) {
		;
	}
}

/**
 * disable_main_xtal_osc
 */
void disable_main_xtal_osc(void)
{
        PMC->CKGR_MOR = 0x37 << 16 | (PMC->CKGR_MOR & ~CKGR_MOR_MOSCXTBY & ~CKGR_MOR_MOSCXTEN);
	while (PMC->PMC_SR & PMC_SR_MOSCXTS) {
		;
	}
}

/**
 * select_main_clk_src
 */
void select_main_clk_src(enum main_clk_src clk_src)
{
	if (clk_src == MAIN_CLK_SRC_FAST_RC_OSC) {
		PMC->CKGR_MOR = 0x37 << 16 | (PMC->CKGR_MOR & ~CKGR_MOR_MOSCSEL);
	} else {
		PMC->CKGR_MOR |=  0x37 << 16 | CKGR_MOR_MOSCSEL;
	}
	while (!(PMC->PMC_SR & PMC_SR_MOSCSELS)) {
		;
	}
}

/**
 * set_pll_freq
 */
void set_pll_freq(enum pll_unit unit, int mul, int div, boolean_t div2, int lock_tm)
{
	switch (unit) {
	case PLL_UNIT_A :
		if (div2) {
			if (!(PMC->PMC_MCKR & PMC_MCKR_PLLADIV2)) {
				PMC->PMC_MCKR |= PMC_MCKR_PLLADIV2;
			}
		} else {
			if (PMC->PMC_MCKR & PMC_MCKR_PLLADIV2) {
				PMC->PMC_MCKR &= ~PMC_MCKR_PLLADIV2;
			}
		}
		PMC->CKGR_PLLAR = CKGR_PLLAR_ONE | CKGR_PLLAR_MULA(mul) |
		                  CKGR_PLLAR_PLLACOUNT(lock_tm) |
                                  CKGR_PLLAR_DIVA(div);
		if (mul) {
			while (!(PMC->PMC_SR & PMC_SR_LOCKA)) {
				;
			}
		}
		break;
#if SAM3S_SERIES || SAM4S_SERIES
	case PLL_UNIT_B :
		if (div2) {
			if (!(PMC->PMC_MCKR & PMC_MCKR_PLLBDIV2)) {
				PMC->PMC_MCKR |= PMC_MCKR_PLLBDIV2;
			}
		} else {
			if (PMC->PMC_MCKR & PMC_MCKR_PLLBDIV2) {
				PMC->PMC_MCKR &= ~PMC_MCKR_PLLBDIV2;
			}
		}
		PMC->CKGR_PLLBR = CKGR_PLLBR_MULB(mul) |
		                  CKGR_PLLBR_PLLBCOUNT(lock_tm) |
                                  CKGR_PLLBR_DIVB(div);
		if (mul) {
			while (!(PMC->PMC_SR & PMC_SR_LOCKB)) {
				;
			}
		}
		break;
#endif
	}
}

#if SAM3S_SERIES || SAM4S_SERIES
/**
 * enable_udp_48mhz_clk
 */
void enable_udp_48mhz_clk(enum pll_unit unit, int mul, int div, boolean_t div2,
                          int lock_tm, int usbdiv)
{
        set_pll_freq(unit, mul, div, div2, lock_tm);
	if (unit == PLL_UNIT_A) {
		PMC->PMC_USB = PMC_USB_USBDIV(usbdiv);
	} else {
		PMC->PMC_USB = PMC_USB_USBDIV(usbdiv) | PMC_USB_USBS;
	}
        PMC->PMC_SCER = PMC_SCER_UDP;
}

/**
 * disable_udp_48mhz_clk
 */
void disable_udp_48mhz_clk(enum pll_unit unit)
{
        PMC->PMC_SCDR = PMC_SCDR_UDP;
	set_pll_freq(unit, 0, 0, FALSE, 0);
}
#endif

/**
 * enable_pmc_frst
 */
void enable_pmc_frst(enum pmc_frst_src src, boolean_t pol)
{
	PMC->PMC_FSMR |= 1 << src;
	if (src < PMC_FRST_RTT) {
		if (pol) {
			PMC->PMC_FSPR |= 1 << src;
		} else {
			PMC->PMC_FSPR &= ~(1 << src);
		}
	}
}

/**
 * disable_pmc_frst
 */
void disable_pmc_frst(enum pmc_frst_src src)
{
	PMC->PMC_FSMR &= ~(1 << src);
	if (src < PMC_FRST_RTT) {
		PMC->PMC_FSPR &= ~(1 << src);
	}
}

/**
 * set_pmc_lpm
 */
void set_pmc_lpm(enum pmc_lpm m)
{
	if (m == PMC_LPM_WAIT) {
		PMC->PMC_FSMR |= PMC_FSMR_LPM_XXX;
	} else {
		PMC->PMC_FSMR &= ~PMC_FSMR_LPM_XXX;
	}
}

#if SAM4_SERIES
/**
 * set_pmc_flash_lpm
 */
void set_pmc_flash_lpm(enum pmc_flash_lpm m)
{
	unsigned int tmp;

	tmp = PMC->PMC_FSMR;
	tmp &= ~PMC_FSMR_FLPM_Msk;
	PMC->PMC_FSMR = tmp | (m << PMC_FSMR_FLPM_Pos);
}
#endif

#if PMC_UPDATE_SYS_CORE_CLK == 1
/**
 * update_sys_core_clk
 */
void update_sys_core_clk(void)
{
	switch (PMC->PMC_MCKR & PMC_MCKR_CSS_Msk) {
	case PMC_MCKR_CSS_SLOW_CLK :	/* Slow clock */
		if (SUPC->SUPC_SR & SUPC_SR_OSCSEL) {
			SystemCoreClock = CHIP_FREQ_XTAL_32K;
		} else {
			SystemCoreClock = CHIP_FREQ_SLCK_RC;
		}
		break;
	case PMC_MCKR_CSS_MAIN_CLK :	/* Main clock */
		if (PMC->CKGR_MOR & CKGR_MOR_MOSCSEL) {
			SystemCoreClock = F_XTAL;
		} else {
			SystemCoreClock = CHIP_FREQ_MAINCK_RC_4MHZ;
			switch (PMC->CKGR_MOR & CKGR_MOR_MOSCRCF_Msk) {
			case CKGR_MOR_MOSCRCF_4_MHz :
				break;
			case CKGR_MOR_MOSCRCF_8_MHz :
				SystemCoreClock *= 2;
				break;
			case CKGR_MOR_MOSCRCF_12_MHz :
				SystemCoreClock *= 3;
				break;
			}
		}
		break;
	case PMC_MCKR_CSS_PLLA_CLK :	/* PLLA clock */
#if SAM3S_SERIES || SAM4S_SERIES
		/* FALLTHRU */
	case PMC_MCKR_CSS_PLLB_CLK :	/* PLLB clock */
#endif
		if (PMC->CKGR_MOR & CKGR_MOR_MOSCSEL) {
			SystemCoreClock = F_XTAL;
		} else {
			SystemCoreClock = CHIP_FREQ_MAINCK_RC_4MHZ;
			switch (PMC->CKGR_MOR & CKGR_MOR_MOSCRCF_Msk) {
			case CKGR_MOR_MOSCRCF_4_MHz :
				break;
			case CKGR_MOR_MOSCRCF_8_MHz :
				SystemCoreClock *= 2;
				break;
			case CKGR_MOR_MOSCRCF_12_MHz :
				SystemCoreClock *= 3;
				break;
			}
		}
#if SAM3S_SERIES || SAM4S_SERIES
		if ((PMC->PMC_MCKR & PMC_MCKR_CSS_Msk) == PMC_MCKR_CSS_PLLA_CLK) {
			SystemCoreClock *= (((PMC->CKGR_PLLAR & CKGR_PLLAR_MULA_Msk) >>
				           CKGR_PLLAR_MULA_Pos) + 1);
			SystemCoreClock /= (((PMC->CKGR_PLLAR & CKGR_PLLAR_DIVA_Msk) >>
				           CKGR_PLLAR_DIVA_Pos));
			if (PMC->PMC_MCKR & PMC_MCKR_PLLADIV2) {
				SystemCoreClock /= 2;
			}
		} else {
			SystemCoreClock *= (((PMC->CKGR_PLLBR & CKGR_PLLBR_MULB_Msk) >>
				           CKGR_PLLBR_MULB_Pos) + 1);
			SystemCoreClock /= (((PMC->CKGR_PLLBR & CKGR_PLLBR_DIVB_Msk) >>
				           CKGR_PLLBR_DIVB_Pos));
			if (PMC->PMC_MCKR & PMC_MCKR_PLLBDIV2) {
				SystemCoreClock /= 2;
			}
		}
#else
		SystemCoreClock *= (((PMC->CKGR_PLLAR & CKGR_PLLAR_MULA_Msk) >>
				   CKGR_PLLAR_MULA_Pos) + 1);
		SystemCoreClock /= (((PMC->CKGR_PLLAR & CKGR_PLLAR_DIVA_Msk) >>
			           CKGR_PLLAR_DIVA_Pos));
		if (PMC->PMC_MCKR & PMC_MCKR_PLLADIV2) {
			SystemCoreClock /= 2;
		}
#endif
		break;
	}
	if ((PMC->PMC_MCKR & PMC_MCKR_PRES_Msk) == PMC_MCKR_PRES_CLK_3) {
		SystemCoreClock /= 3;
	} else {
		SystemCoreClock >>= ((PMC->PMC_MCKR & PMC_MCKR_PRES_Msk) >> PMC_MCKR_PRES_Pos);
	}
}
#endif

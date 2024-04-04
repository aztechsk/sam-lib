/*
 * pmc.h
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

#ifndef PMC_H
#define PMC_H

extern unsigned int SystemCoreClock;

enum mast_clk_src {
	MCK_SRC_SLOW_CLK,
	MCK_SRC_MAIN_CLK,
	MCK_SRC_PLLA_CLK,
#if SAM3S_SERIES || SAM4S_SERIES
        MCK_SRC_PLLB_CLK
#endif
};

enum mast_clk_presc {
	MCK_PRESC_CLK_1,
        MCK_PRESC_CLK_2,
        MCK_PRESC_CLK_4,
        MCK_PRESC_CLK_8,
        MCK_PRESC_CLK_16,
        MCK_PRESC_CLK_32,
        MCK_PRESC_CLK_64,
        MCK_PRESC_CLK_3
};

enum fast_rc_osc_freq {
	FAST_RC_OSC_FREQ_4_MHZ,
	FAST_RC_OSC_FREQ_8_MHZ,
	FAST_RC_OSC_FREQ_12_MHZ
};

enum main_clk_src {
	MAIN_CLK_SRC_FAST_RC_OSC,
        MAIN_CLK_SRC_MAIN_XTAL_OSC
};

enum pll_unit {
	PLL_UNIT_A,
#if SAM3S_SERIES || SAM4S_SERIES
        PLL_UNIT_B
#endif
};

enum pmc_frst_src {
	PMC_FRST_P0,
	PMC_FRST_P1,
	PMC_FRST_P2,
	PMC_FRST_P3,
	PMC_FRST_P4,
	PMC_FRST_P5,
	PMC_FRST_P6,
	PMC_FRST_P7,
	PMC_FRST_P8,
	PMC_FRST_P9,
	PMC_FRST_P10,
	PMC_FRST_P11,
	PMC_FRST_P12,
	PMC_FRST_P13,
	PMC_FRST_P14,
	PMC_FRST_P15,
	PMC_FRST_RTT,
	PMC_FRST_RTC,
#if SAM3S_SERIES || SAM4S_SERIES
	PMC_FRST_USB
#endif
};

enum pmc_lpm {
	PMC_LPM_IDLE,
	PMC_LPM_WAIT
};

#if SAM4_SERIES
enum pmc_flash_lpm {
	PMC_FLASH_LPM_STANDBY,
	PMC_FLASH_LPM_DEEP_PWRDOWN,
	PMC_FLASH_LPM_IDLE
};
#endif

/**
 * enable_periph_clk
 */
void enable_periph_clk(int id);

/**
 * disable_periph_clk
 */
void disable_periph_clk(int id);

/**
 * enable_periph_clk_nocs
 *
 * Thread unsafe.
 */
void enable_periph_clk_nocs(int id);

/**
 * disable_periph_clk_nocs
 *
 * Thread unsafe.
 */
void disable_periph_clk_nocs(int id);

/**
 * get_act_periph_clks
 *
 * Returns: Active peripheral clocks bitmap.
 */
unsigned long long get_act_periph_clks(void);

/**
 * select_mast_clk_src
 *
 * Select master clock source.
 *
 * @mck_src: Master clock source (enum mast_clk_src).
 * @mck_presc: Master clock prescaler (enum mast_clk_presc).
 */
void select_mast_clk_src(enum mast_clk_src mck_src, enum mast_clk_presc mck_presc);

/**
 * enable_fast_rc_osc
 */
void enable_fast_rc_osc(void);

/**
 * disable_fast_rc_osc
 */
void disable_fast_rc_osc(void);

/**
 * wait_fast_rc_osc_enabled
 *
 * Function returns after fastrc oscilator enable bit is 1.
 */
inline void wait_fast_rc_osc_enabled(void)
{
	while (!(PMC->CKGR_MOR & CKGR_MOR_MOSCRCEN));
}

/**
 * set_fast_rc_osc_freq
 *
 * Set fast rc oscillator frequency.
 *
 * @freq: Fast rc oscillator frequency (enum fast_rc_osc_freq).
 */
void set_fast_rc_osc_freq(enum fast_rc_osc_freq freq);

/**
 * enable_main_xtal_osc
 *
 * @st_up_tm: Startup time st_up_tm * 8 SLCK cycles.
 */
void enable_main_xtal_osc(int st_up_tm);

/**
 * disable_main_xtal_osc
 */
void disable_main_xtal_osc(void);

/**
 * select_main_clk_src
 *
 * @clk_src: Main clock source (enum main_clk_src).
 */
void select_main_clk_src(enum main_clk_src clk_src);

/**
 * set_pll_freq
 *
 * PLL_OUT_FREQ = ((MUL + 1) / DIV) / 2. If div2 is TRUE.
 * PLL_OUT_FREQ = (MUL + 1) / DIV.
 *
 * @unit: PLL unit (enum pll_unit).
 * @mul: PLL input frequency multiplier (0 disable PLL).
 * @div: PLL input frequency divider.
 * @div2: TRUE enable PLL block internal divider by 2.
 * @lock_tm: PLL locks after lock_tm * 8 SLCK cycles.
 */
void set_pll_freq(enum pll_unit unit, int mul, int div, boolean_t div2, int lock_tm);

#if SAM3S_SERIES || SAM4S_SERIES
/**
 * enable_udp_48mhz_clk
 *
 * F_USB = ((MUL + 1) / DIV) / (USBDIV + 1).
 *
 * @unit: PLL unit (enum pll_unit).
 * @mul: PLL multiplier.
 * @div: PLL divider.
 * @div2: TRUE enable PLL block internal divider by 2.
 * @lock_tm: PLL locks after lock_tm * 8 SLCK cycles.
 * @usbdiv: PLL output clock divider.
 */
void enable_udp_48mhz_clk(enum pll_unit unit, int mul, int div, boolean_t div2,
                          int lock_tm, int usbdiv);

/**
 * disable_udp_48mhz_clk
 *
 * @unit: PLL unit (enum pll_unit).
 */
void disable_udp_48mhz_clk(enum pll_unit unit);
#endif

/**
 * enable_pmc_frst
 *
 * Enables fast restart (wake) signal to PMC.
 *
 * @src: Fast restart signal source (enum pmc_frst_src).
 * @pol: Active signal polarity (for wake pins only).
 */
void enable_pmc_frst(enum pmc_frst_src src, boolean_t pol);

/**
 * disable_pmc_frst
 *
 * Disables fast restart (wake) signal to PMC.
 *
 * @src: Fast restart signal source (enum pmc_frst_src).
 */
void disable_pmc_frst(enum pmc_frst_src src);

/**
 * set_pmc_lpm
 *
 * Set low power mode (WAIT mode or IDLE (SLEEP) mode).
 *
 * @m: Low power mode (enum pmc_lpm).
 */
void set_pmc_lpm(enum pmc_lpm m);

#if SAM4_SERIES
/**
 * set_pmc_flash_lpm
 *
 * Set FLASH low power mode.
 *
 * @m: Low power mode (enum pmc_flash_lpm).
 */
void set_pmc_flash_lpm(enum pmc_flash_lpm m);
#endif

#if SAM4_SERIES
/**
 * enter_pmc_wait_lpm
 *
 * Enter wait low power mode with WAITMODE bit.
 *
 */
inline void enter_pmc_wait_lpm(void)
{
	PMC->CKGR_MOR |=  0x37 << 16 | CKGR_MOR_WAITMODE;
	while (!(PMC->PMC_SR & PMC_SR_MCKRDY));
}
#endif

#if PMC_UPDATE_SYS_CORE_CLK == 1
/**
 * update_sys_core_clk
 */
void update_sys_core_clk(void);
#endif

#endif

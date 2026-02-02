/*
 * adc.h
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

#ifndef ADC_H
#define ADC_H

#ifndef ADC_SW_TRG_1CH
 #define ADC_SW_TRG_1CH 0
#endif
#ifndef ADC_SW_TRG_1CH_N
 #define ADC_SW_TRG_1CH_N 0
#endif
#ifndef ADC_SW_TRG_XCH
 #define ADC_SW_TRG_XCH 0
#endif

#if ADC_SW_TRG_1CH == 1 || ADC_SW_TRG_1CH_N == 1 || ADC_SW_TRG_XCH == 1

enum adc_chn {
	ADC_CH0,
	ADC_CH1,
	ADC_CH2,
	ADC_CH3,
	ADC_CH4,
	ADC_CH5,
	ADC_CH6,
	ADC_CH7,
	ADC_CH8,
	ADC_CH9,
	ADC_CH10,
	ADC_CH11,
	ADC_CH12,
	ADC_CH13,
	ADC_CH14,
#if SAM3N_SERIES
	ADC_CH15
#endif
#if SAM3S_SERIES || SAM4S_SERIES
	ADC_CHTEMP
#endif
#if SAM4N_SERIES
	ADC_CH15,
	ADC_CHTEMP
#endif
};

#if SAM3S_SERIES || SAM4S_SERIES
struct adc_acr_cfg {
	boolean_t temp_sensor;
	int ibctl;
};
#endif

#if SAM4N_SERIES
enum adc_ref_vol_src {
	ADC_REF_VOL_EXTERNAL,
	ADC_REF_VOL_STUCK_AT_MIN,
	ADC_REF_VOL_VDDANA,
	ADC_REF_VOL_IRVS
};

struct adc_acr_cfg {
	enum adc_ref_vol_src ref_vol_src;
	int irvs;
};

enum adc_temp_cmp_mod {
	ADC_TEMP_CMP_MOD_LOW,
        ADC_TEMP_CMP_MOD_HIGH,
        ADC_TEMP_CMP_MOD_IN,
        ADC_TEMP_CMP_MOD_OUT
};

struct adc_chtemp_cfg {
	boolean_t temp_sensor;
	enum adc_temp_cmp_mod cmp_mod;
	unsigned int tcwr;
};
#endif

typedef struct adc_dev *adc;

struct adc_dev {
	unsigned int mr; // <SetIt>
	unsigned int emr; // <SetIt>
	unsigned int seqr1; // <SetIt>
	unsigned int seqr2; // <SetIt>
	unsigned int cwr; // <SetIt>
#if SAM3S_SERIES || SAM4S_SERIES
	unsigned int cgr; // <SetIt>
        unsigned int cor; // <SetIt>
#endif
#if SAM3S_SERIES || SAM4S_SERIES || SAM4N_SERIES
	struct adc_acr_cfg acr_cfg; // <SetIt>
#endif
#if SAM4N_SERIES
	struct adc_chtemp_cfg chtemp_cfg; // <SetIt>
#endif
#if ADC_SW_TRG_1CH == 1
	int chn; // <SetIt>
#endif
#if ADC_SW_TRG_XCH == 1
	unsigned int chnls_bmp; // <SetIt>
#endif
#if ADC_SW_TRG_1CH_N == 1
	SemaphoreHandle_t mtx; // <SetIt>
#endif
};

/**
 * init_adc
 *
 * Configure ADC to requested mode.
 *
 * @dev: ADC device.
 */
void init_adc(adc dev);

/**
 * calibrate_adc
 *
 * Calibrate ADC.
 */
void calibrate_adc(void);
#endif

#if ADC_SW_TRG_1CH == 1
/**
 * read_adc_chnl
 *
 * Return result of ADC conversion in one channel mode.
 *
 * Returns: ADC conversion result.
 */
int read_adc_chnl(void);
#endif

#if ADC_SW_TRG_1CH_N == 1
/**
 * read_adc_chnl_n
 *
 * Start conversion of one channel and return result.
 *
 * @chn: ADC channel number (enum adc_chn).
 *
 * Returns: ADC conversion result; -EHW - ADC error.
 */
int read_adc_chnl_n(enum adc_chn chn);
#endif

#if ADC_SW_TRG_XCH == 1
/**
 * start_adc_conv
 *
 * Start conversion and return after conversion is done.
 *
 * Returns: 0 - success; -EHW - ADC error.
 */
int start_adc_conv(void);

/**
 * read_adc_chnl
 *
 * Return result of ADC conversion of channel 'chn'.
 *
 * @chn: ADC channel number (enum adc_chn).
 *
 * Returns: ADC conversion result.
 */
int read_adc_chnl(enum adc_chn chn);
#endif

#if SAM4N_SERIES
/**
 * read_adc_chtemp
 *
 * Read ADC_CHTEMP channel value.
 *
 * Returns: ADC_CHTEMP conversion result; -ENRDY - ADC config error.
 */
int read_adc_chtemp(void);
#endif

#endif

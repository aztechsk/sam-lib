/*
 * adc.c
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
#include "hwerr.h"
#include "pmc.h"
#include "adc.h"

#if ADC_SW_TRG_1CH == 1 || ADC_SW_TRG_1CH_N == 1 || ADC_SW_TRG_XCH == 1

#define WAIT_ADC_EOC (1000 / portTICK_PERIOD_MS)

#if SAM3N_SERIES || SAM3S_SERIES || SAM4S_SERIES
 #define ADC_CHNL_NUM 16
#elif SAM4N_SERIES
 #define ADC_CHNL_NUM 17
#else
 #error "SAM_SERIES definition error"
#endif

static adc ac;
#if ADC_SW_TRG_1CH_N == 1 || ADC_SW_TRG_XCH == 1
static SemaphoreHandle_t sig;
#endif

/**
 * init_adc
 */
void init_adc(adc dev)
{
	ac = dev;
        NVIC_DisableIRQ(ID_ADC);
        enable_periph_clk(ID_ADC);
        ADC->ADC_CR = ADC_CR_SWRST;
        ADC->ADC_CHDR = ~0;
	ADC->ADC_IDR = ~0;
	ADC->ADC_MR = dev->mr;
	ADC->ADC_EMR = dev->emr;
	if (dev->mr & ADC_MR_USEQ) {
		ADC->ADC_SEQR1 = dev->seqr1;
                ADC->ADC_SEQR2 = dev->seqr2;
	}
	ADC->ADC_CWR = dev->cwr;
#if SAM3S_SERIES || SAM4S_SERIES
	ADC->ADC_CGR = dev->cgr;
        ADC->ADC_COR = dev->cor;
	if (dev->acr_cfg.temp_sensor) {
		ADC->ADC_ACR |= ADC_ACR_TSON;
	} else {
		ADC->ADC_ACR &= ~ADC_ACR_TSON;
	}
	ADC->ADC_ACR = (ADC->ADC_ACR & ADC_ACR_IBCTL_Msk) | ADC_ACR_IBCTL(dev->acr_cfg.ibctl);
#endif
#if SAM4N_SERIES
	if (dev->acr_cfg.ref_vol_src == ADC_REF_VOL_EXTERNAL) {
		ADC->ADC_ACR &= ~ADC_ACR_ONREF_EN;
	} else if (dev->acr_cfg.ref_vol_src == ADC_REF_VOL_STUCK_AT_MIN) {
		ADC->ADC_ACR |= ADC_ACR_ONREF_EN;
		ADC->ADC_ACR &= ~(ADC_ACR_IRVCE_EN | ADC_ACR_FORCEREF_EN);
	} else if (dev->acr_cfg.ref_vol_src == ADC_REF_VOL_VDDANA) {
		ADC->ADC_ACR |= ADC_ACR_ONREF_EN | ADC_ACR_FORCEREF_EN;
	} else if (dev->acr_cfg.ref_vol_src == ADC_REF_VOL_IRVS) {
		ADC->ADC_ACR &= ~ADC_ACR_FORCEREF_EN;
		ADC->ADC_ACR = ADC_ACR_ONREF_EN |
			       (ADC->ADC_ACR & ADC_ACR_IRVS_Msk) | ADC_ACR_IRVS(dev->acr_cfg.irvs) |
                               ADC_ACR_IRVCE_EN;
	} else {
		crit_err_exit(BAD_PARAMETER);
	}
	if (dev->chtemp_cfg.temp_sensor) {
		ADC->ADC_TEMPMR = dev->chtemp_cfg.cmp_mod << ADC_TEMPMR_TEMPCMPMOD_Pos | ADC_TEMPMR_TEMPON;
		ADC->ADC_TEMPCWR = dev->chtemp_cfg.tcwr;
	}
#endif
#if ADC_SW_TRG_1CH == 1
	ADC->ADC_CHER = 1 << dev->chn;
#endif
#if ADC_SW_TRG_XCH == 1 || ADC_SW_TRG_1CH_N == 1
	if (sig == NULL) {
		if (NULL == (sig = xSemaphoreCreateBinary())) {
			crit_err_exit(MALLOC_ERROR);
		}
	} else {
		crit_err_exit(UNEXP_PROG_STATE);
	}
	NVIC_ClearPendingIRQ(ID_ADC);
	NVIC_SetPriority(ID_ADC, configLIBRARY_MAX_API_CALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(ID_ADC);
#endif
#if ADC_SW_TRG_XCH == 1
	ADC->ADC_CHER = dev->chnls_bmp;
#endif
#if ADC_SW_TRG_1CH_N == 1
	ADC->ADC_EMR |= ADC_EMR_TAG;
#endif
}

/**
 * calibrate_adc
 */
void calibrate_adc(void)
{
	unsigned int chsr;

	chsr = ADC->ADC_CHSR;
#if SAM3N_SERIES || SAM3S_SERIES || SAM4S_SERIES
	ADC->ADC_CHER = 0xFFFF;
#else
	ADC->ADC_CHER = 0x1FFFF;
#endif
	ADC->ADC_CR = 1 << 3;
	while (!(ADC->ADC_ISR & 1 << 23)) {
		;
	}
#if SAM3N_SERIES || SAM3S_SERIES || SAM4S_SERIES
	ADC->ADC_CHDR = 0xFFFF;
#else
	ADC->ADC_CHDR = 0x1FFFF;
#endif
	ADC->ADC_CHER = chsr;
}
#endif

#if ADC_SW_TRG_1CH == 1
/**
 * read_adc_chnl
 */
int read_adc_chnl(void)
{
	unsigned int lcdr;

	lcdr = ADC->ADC_LCDR;
	ADC->ADC_CR = ADC_CR_START;
	while (!(ADC->ADC_ISR & ADC_ISR_DRDY)) {
		;
	}
	return (ADC_LCDR_LDATA_Msk & ADC->ADC_LCDR);
}
#endif

#if ADC_SW_TRG_1CH_N == 1
/**
 * read_adc_chnl_n
 */
int read_adc_chnl_n(enum adc_chn chn)
{
	unsigned int lcdr;
	int ret;

	if (ac->mtx != NULL) {
		xSemaphoreTake(ac->mtx, portMAX_DELAY);
	}
        ADC->ADC_CHER = 1 << chn;
        lcdr = ADC->ADC_LCDR;
	ADC->ADC_IER = ADC_IER_DRDY;
	ADC->ADC_CR = ADC_CR_START;
        if (pdFALSE == xSemaphoreTake(sig, WAIT_ADC_EOC)) {
		ADC->ADC_IDR = ~0;
                ADC->ADC_CHDR = 1 << chn;
                xSemaphoreTake(sig, 0);
		ret = -EHW;
		goto exit;
	}
	lcdr = ADC->ADC_LCDR;
        ADC->ADC_CHDR = 1 << chn;
	if ((lcdr & ADC_LCDR_CHNB_Msk) >> ADC_LCDR_CHNB_Pos == chn) {
		ret = ADC_LCDR_LDATA_Msk & lcdr;
	} else {
		ret = -EHW;
	}
exit:
	if (ac->mtx != NULL) {
		xSemaphoreGive(ac->mtx);
	}
	return (ret);
}

/**
 * ADC_Handler
 */
void ADC_Handler(void)
{
	BaseType_t tsk_wkn = pdFALSE;

	if (ADC->ADC_ISR & ADC_ISR_DRDY) {
		ADC->ADC_IDR = ADC_IDR_DRDY;
		xSemaphoreGiveFromISR(sig, &tsk_wkn);
	}
        portEND_SWITCHING_ISR(tsk_wkn);
}
#endif

#if ADC_SW_TRG_XCH == 1
static volatile unsigned int pchnls;

/**
 * start_adc_conv
 */
int start_adc_conv(void)
{
	int idx = 1;
	unsigned int cdr;

	for (int i = 0; i < ADC_CHNL_NUM; i++) {
		if (ac->chnls_bmp & idx) {
			cdr = ADC->ADC_CDR[i];
		}
		idx <<= 1;
	}
        pchnls = ac->chnls_bmp;
	ADC->ADC_IER = ac->chnls_bmp;
	ADC->ADC_CR = ADC_CR_START;
        if (pdFALSE == xSemaphoreTake(sig, WAIT_ADC_EOC)) {
		ADC->ADC_IDR = ~0;
                xSemaphoreTake(sig, 0);
		return (-EHW);
	}
	return 0;
}

/**
 * read_adc_chnl
 */
int read_adc_chnl(enum adc_chn chn)
{
	return (ADC_CDR_DATA_Msk & ADC->ADC_CDR[chn]);
}

/**
 * ADC_Handler
 */
void ADC_Handler(void)
{
	BaseType_t tsk_wkn = pdFALSE;
	unsigned int isr;

	isr = ADC->ADC_ISR;
	ADC->ADC_IDR = isr;
	pchnls &= ~isr;
	if (!pchnls) {
		xSemaphoreGiveFromISR(sig, &tsk_wkn);
	}
        portEND_SWITCHING_ISR(tsk_wkn);
}
#endif

#if SAM4N_SERIES && (ADC_SW_TRG_1CH == 1 || ADC_SW_TRG_1CH_N == 1 || ADC_SW_TRG_XCH == 1)
/**
 * read_adc_chtemp
 */
int read_adc_chtemp(void)
{
	if (ADC->ADC_TEMPMR & ADC_TEMPMR_TEMPON) {
		return (ADC_CDR_DATA_Msk & ADC->ADC_CDR[ADC_CHTEMP]);
	} else {
		return (-ENRDY);
	}
}
#endif

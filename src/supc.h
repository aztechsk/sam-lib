/*
 * supc.h
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

#ifndef SUPC_H
#define SUPC_H

/**
 * init_supc
 */
void init_supc(void);

/**
 * enable_bod_rst
 */
void enable_bod_rst(void);

/**
 * disable_bod_rst
 */
void disable_bod_rst(void);

/**
 * bod_rst_stat
 */
boolean_t bod_rst_stat(void);

/**
 * disable_emb_vreg
 */
void disable_emb_vreg(void);

/**
 * stop_vreg_rst_core
 */
void stop_vreg_rst_core(void);

enum supc_sm_smpl {
	SUPC_SMSMPL_DISABLE,
        SUPC_SMSMPL_CSM,
	SUPC_SMSMPL_32SLCK,
        SUPC_SMSMPL_256SLCK,
        SUPC_SMSMPL_2048SLCK
};

enum supc_sm_th {
	SUPC_SM_TH_1_9V,
        SUPC_SM_TH_2_0V,
	SUPC_SM_TH_2_1V,
	SUPC_SM_TH_2_2V,
	SUPC_SM_TH_2_3V,
	SUPC_SM_TH_2_4V,
	SUPC_SM_TH_2_5V,
	SUPC_SM_TH_2_6V,
	SUPC_SM_TH_2_7V,
	SUPC_SM_TH_2_8V,
	SUPC_SM_TH_2_9V,
	SUPC_SM_TH_3_0V,
	SUPC_SM_TH_3_1V,
	SUPC_SM_TH_3_2V,
	SUPC_SM_TH_3_3V,
	SUPC_SM_TH_3_4V
};

/**
 * enable_sup_mon
 *
 * Enable VDDIO suply monitor.
 *
 * @smpl: Suply monitor sampling mode (enum supc_sm_smpl).
 * @th: Supply monitor threshold voltage (enum supc_sm_th).
 */
void enable_sup_mon(enum supc_sm_smpl smpl, enum supc_sm_th th);

/**
 * enable_sup_mon_rst
 */
void enable_sup_mon_rst(void);

/**
 * disable_sup_mon_rst
 */
void disable_sup_mon_rst(void);

/**
 * enable_32k_xtal_osc
 */
void enable_32k_xtal_osc(void);

#if TERMOUT == 1
/**
 * log_supc_cfg
 */
void log_supc_cfg(void);

/**
 * log_supc_rst_stat
 */
void log_supc_rst_stat(void);
#endif

#endif

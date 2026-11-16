/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TEE_SMC_XFER_H
#define TEE_SMC_XFER_H

struct smc_param;

struct invoke_tee_response {
	unsigned long a0;
	unsigned long a1;
	unsigned long a2;
	unsigned long a3;
};

typedef int (* invoke_tee_fn) (uint32_t fid,
		unsigned long a0, unsigned long a1, unsigned long a2,
		struct invoke_tee_response *resp);

struct tee_task_cpu_affinity;

void run_tee_task(invoke_tee_fn invoke_tee,
		struct invoke_tee_response *req_resp,
		struct tee_task_cpu_affinity *aff);

int tee_init_task(void);
void tee_exit_task(void);

#endif

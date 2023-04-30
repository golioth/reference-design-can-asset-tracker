/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_GNSS_H__
#define __APP_GNSS_H__

#include <zephyr/kernel.h>

#include "lib/minmea/minmea.h"

struct rmc_msg {
	int64_t timestamp;
	struct minmea_sentence_rmc frame;
};

extern struct k_msgq rmc_msgq;

int gnss_init(void);

#endif /* __APP_GNSS_H__ */

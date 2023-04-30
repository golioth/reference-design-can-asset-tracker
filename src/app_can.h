/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_CAN_H__
#define __APP_CAN_H__

#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>

struct can_msg {
	int64_t timestamp;
	struct can_frame frame;
};

extern struct k_msgq can_msgq;

int can_listener_init(struct golioth_client *listener_client);

#endif /* __APP_CAN_H__ */

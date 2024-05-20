/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Handle remote procedure calls received from Golioth, returning a status code
 * indicating the success or failure of the call.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/remote-procedure-call
 */

#ifndef __APP_RPC_H__
#define __APP_RPC_H__

#include <golioth/client.h>

void app_rpc_register(struct golioth_client *client);

#endif /* __APP_RPC_H__ */

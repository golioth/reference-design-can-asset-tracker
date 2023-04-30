/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_stack.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>
#include <qcbor/qcbor_encode.h>
#include <net/golioth/system_client.h>

#include "app_can.h"

#define CAN_LISTENER_THREAD_STACK_SIZE 2048
#define CAN_LISTENER_THREAD_PRIORITY 2
#define COUNTER_MSG_ID 0x244

static struct golioth_client *client;
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
struct k_thread can_listener_thread_data;

LOG_MODULE_REGISTER(app_can, LOG_LEVEL_DBG);
K_THREAD_STACK_DEFINE(can_listener_thread_stack, CAN_LISTENER_THREAD_STACK_SIZE);
CAN_MSGQ_DEFINE(can_frame_msgq, 4);
K_MSGQ_DEFINE(can_msgq, sizeof(struct can_msg), 64, 4);

/* Listens for CAN frames matching the filter and puts them in can_frame_msgq */
void can_listener_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	int err, filter_id;
	struct can_frame frame;
	struct can_msg msg;
	const struct can_filter filter = {
		.flags = CAN_FILTER_DATA | CAN_FILTER_IDE,
		.id = COUNTER_MSG_ID,
		.mask = CAN_EXT_ID_MASK
	};

	/* Automatically put frames matching the filter into can_frame_msgq */
	filter_id = can_add_rx_filter_msgq(can_dev, &can_frame_msgq, &filter);
	if (filter_id == -ENOSPC) {
		LOG_ERR("No free CAN filters [%d]", filter_id);
		return;
	} else if (filter_id == -ENOTSUP) {
		LOG_ERR("CAN filter type not supported [%d]", filter_id);
		return;
	} else if (filter_id != 0) {
		LOG_ERR("Error adding a message queue for the given filter [%d]", filter_id);
		return;
	}
	LOG_DBG("CAN bus receive filter id: %d", filter_id);

	while (1) {
		k_msgq_get(&can_frame_msgq, &frame, K_FOREVER);

		/*
		 * LOG_HEXDUMP_INF(frame.data, frame.dlc, "CAN frame data:");
		 *
		 * uint16_t kmh = sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data[3])) / 100;
		 * uint16_t mph = (uint16_t)(kmh * 0.6213751);
		 * LOG_DBG("MPH: %u", mph);
		 */

		msg.timestamp = k_uptime_get();
		msg.frame = frame;

		err = k_msgq_put(&can_msgq, &msg, K_NO_WAIT);
		if (err) {
			LOG_ERR("Unable to add CAN message to queue: %d", err);
		}
	}
}

int can_listener_init(struct golioth_client *listener_client)
{
	int ret;
	k_tid_t rx_tid;

	LOG_DBG("Initializing CAN listener");

	client = listener_client;

	if (!device_is_ready(can_dev)) {
		LOG_ERR("CAN device %s not ready", can_dev->name);
		return -ENODEV;
	}

	/* Start the CAN controller */
	ret = can_start(can_dev);
	if (ret == -EALREADY) {
		LOG_ERR("CAN controller already started [%d]", ret);
		return ret;
	} else if (ret != 0) {
		LOG_ERR("Error starting CAN controller [%d]", ret);
		return ret;
	}

	/* Spawn a thread to listen for CAN frames and put them in can_frame_msgq */
	rx_tid = k_thread_create(&can_listener_thread_data, can_listener_thread_stack,
				 K_THREAD_STACK_SIZEOF(can_listener_thread_stack),
				 can_listener_thread, NULL, NULL, NULL,
				 CAN_LISTENER_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!rx_tid) {
		LOG_ERR("Error spawning CAN receive thread");
	}

	return 0;
}

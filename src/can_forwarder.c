/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <qcbor/qcbor_encode.h>
#include <net/golioth/system_client.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_stack.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>

#include "can_forwarder.h"

#define CAN_FORWARDER_THREAD_STACK_SIZE 2048
#define CAN_FORWARDER_THREAD_PRIORITY 2
#define COUNTER_MSG_ID 0x123

LOG_MODULE_REGISTER(can_forwarder, LOG_LEVEL_DBG);
K_THREAD_STACK_DEFINE(can_forwarder_thread_stack, CAN_FORWARDER_THREAD_STACK_SIZE);
CAN_MSGQ_DEFINE(can_frame_msgq, 2);

static struct golioth_client *client;
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
struct k_thread can_forwarder_thread_data;

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

void can_forwarder_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	const struct can_filter filter = {
		.flags = CAN_FILTER_DATA | CAN_FILTER_IDE,
		.id = COUNTER_MSG_ID,
		.mask = CAN_EXT_ID_MASK
	};
	struct can_frame frame;
	int ret;

	ret = can_add_rx_filter_msgq(can_dev, &can_frame_msgq, &filter);
	if (ret == -ENOSPC) {
		LOG_ERR("No free CAN filters [%d]", ret);
		return;
	} else if (ret == -ENOTSUP) {
		LOG_ERR("CAN filter type not supported [%d]", ret);
		return;
	} else if (ret != 0) {
		LOG_ERR("Error adding a message queue for the given filter [%d]", ret);
		return;
	}
	LOG_DBG("CAN bus receive filter id: %d", ret);

	while (1) {
		k_msgq_get(&can_frame_msgq, &frame, K_FOREVER);

		LOG_DBG("CAN frame: ID=%d, DLC=%d, IDE=%d, RTR=%d",
			frame.id, frame.dlc, (int)(frame.flags & CAN_FRAME_IDE),
			(int)(frame.flags & CAN_FRAME_RTR));
		LOG_HEXDUMP_INF(frame.data, sizeof(frame.data),
			"CAN frame data:");

		uint8_t cbor_buf[256];
		size_t size = 0;
		UsefulBufC can_frame_buf = {
			.ptr = frame.data,
			.len = frame.dlc
		};
		UsefulBuf ubuf = {
			.ptr = cbor_buf,
			.len = sizeof(cbor_buf)
		};
		QCBOREncodeContext ec;

		QCBOREncode_Init(&ec, ubuf);
		QCBOREncode_OpenMap(&ec);
		QCBOREncode_AddUInt64ToMap(&ec, "id", frame.id);
		QCBOREncode_AddUInt64ToMap(&ec, "dlc", frame.dlc);
		QCBOREncode_AddBoolToMap(&ec, "ide",
			frame.flags & CAN_FRAME_IDE);
		QCBOREncode_AddBoolToMap(&ec, "rtr",
			frame.flags & CAN_FRAME_RTR);
		QCBOREncode_AddBytesToMap(&ec, "data", can_frame_buf);
		QCBOREncode_CloseMap(&ec);
		QCBOREncode_FinishGetSize(&ec, &size);
		if (size < 0) {
			LOG_ERR("Failed to encode CAN data [%d]", size);
		}
		LOG_DBG("Serialized CAN frame as %d bytes of CBOR encoded data", size);

		LOG_DBG("Sending CAN frame to Golioth LightDB Stream");
		ret = golioth_stream_push_cb(client, "can",
			GOLIOTH_CONTENT_FORMAT_APP_CBOR,
			cbor_buf, size,
			async_error_handler, NULL);
		if (ret != 0) {
			LOG_ERR("Failed to send CAN data to Golioth: %d", ret);
		}
	}
}

int can_forwarder_init(struct golioth_client *forwarder_client)
{
	int ret;
	k_tid_t rx_tid;

	LOG_DBG("Initializing CAN forwarder");

	client = forwarder_client;

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

	/* Spawn a thread to forward CAN frames */
	rx_tid = k_thread_create(&can_forwarder_thread_data, can_forwarder_thread_stack,
				 K_THREAD_STACK_SIZEOF(can_forwarder_thread_stack),
				 can_forwarder_thread, NULL, NULL, NULL,
				 CAN_FORWARDER_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!rx_tid) {
		LOG_ERR("Error spawning CAN receive thread");
	}

	return 0;
}

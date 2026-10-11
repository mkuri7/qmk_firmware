/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <stdint.h>
#include <string.h>
#include "raw_hid.h"
#include "usb_descriptor.h"

#define KATANA_REPORT_SIZE RAW_EPSIZE
#define KATANA_PROTOCOL_VERSION 0x01

#define KATANA_CAP_COMMAND_INPUT (1 << 0)
#define KATANA_CAP_HOST_STATE (1 << 1)
#define KATANA_CAP_FEEDBACK_EVENT (1 << 2)
#define KATANA_CAP_HEARTBEAT (1 << 3)
#define KATANA_CAPABILITIES (KATANA_CAP_COMMAND_INPUT | KATANA_CAP_HOST_STATE | KATANA_CAP_FEEDBACK_EVENT | KATANA_CAP_HEARTBEAT)

#define KATANA_MAGIC_0 0x4B /* K */
#define KATANA_MAGIC_1 0x54 /* T */
#define KATANA_MAGIC_2 0x4E /* N */
#define KATANA_MAGIC_3 0x41 /* A */

enum katana_message_type {
    KATANA_MSG_HELLO = 0x01,
    KATANA_MSG_HELLO_ACK = 0x02,
    KATANA_MSG_COMMAND = 0x03,
    KATANA_MSG_ACK = 0x04,
    KATANA_MSG_NACK = 0x05,
    KATANA_MSG_HOST_STATE = 0x06,
    KATANA_MSG_FEEDBACK_EVENT = 0x07,
    KATANA_MSG_HEARTBEAT = 0x08,
};

enum katana_host_state_flag {
    KATANA_STATE_READY = 1 << 0,
    KATANA_STATE_ARMED = 1 << 1,
    KATANA_STATE_ON_POSITION = 1 << 2,
};

enum katana_feedback_event {
    KATANA_FEEDBACK_COMMAND_ACCEPTED = 0x01,
    KATANA_FEEDBACK_COMMAND_REJECTED = 0x02,
};

enum katana_command_id {
    KATANA_CMD_NONE = 0x00,
    KATANA_CMD_ARM_TOGGLE = 0x01,
    KATANA_CMD_OPEN_LONG = 0x02,
    KATANA_CMD_OPEN_SHORT = 0x03,
    KATANA_CMD_CANCEL_ALL = 0x04,
    KATANA_CMD_MOVE_TO_BE = 0x05,
    KATANA_CMD_EXIT_9 = 0x06,
    KATANA_CMD_EXIT_6 = 0x07,
    KATANA_CMD_EXIT_3 = 0x08,
    KATANA_CMD_EXIT_1 = 0x09,
    KATANA_CMD_FLATTEN = 0x0A,
    KATANA_CMD_REVERSE = 0x0B,
};

enum katana_packet_offset {
    KATANA_OFFSET_MAGIC = 0,
    KATANA_OFFSET_VERSION = 4,
    KATANA_OFFSET_MESSAGE_TYPE = 5,
    KATANA_OFFSET_COMMAND_ID = 6,
    KATANA_OFFSET_SEQUENCE = 7,
    KATANA_OFFSET_PAYLOAD = 8,
};

enum katana_hello_ack_payload_offset {
    KATANA_HELLO_ACK_FW_MAJOR = 0,
    KATANA_HELLO_ACK_FW_MINOR = 1,
    KATANA_HELLO_ACK_FW_PATCH = 2,
    KATANA_HELLO_ACK_DEVICE_TYPE = 3,
    KATANA_HELLO_ACK_CAPABILITIES = 4,
};

static inline bool katana_packet_has_magic(const uint8_t *data, uint8_t length) {
    return length == KATANA_REPORT_SIZE && data[0] == KATANA_MAGIC_0 && data[1] == KATANA_MAGIC_1 && data[2] == KATANA_MAGIC_2 && data[3] == KATANA_MAGIC_3;
}

static inline void katana_packet_init(uint8_t *data, uint8_t message_type, uint8_t command_id, uint8_t sequence) {
    memset(data, 0, KATANA_REPORT_SIZE);
    data[0] = KATANA_MAGIC_0;
    data[1] = KATANA_MAGIC_1;
    data[2] = KATANA_MAGIC_2;
    data[3] = KATANA_MAGIC_3;
    data[KATANA_OFFSET_VERSION] = KATANA_PROTOCOL_VERSION;
    data[KATANA_OFFSET_MESSAGE_TYPE] = message_type;
    data[KATANA_OFFSET_COMMAND_ID] = command_id;
    data[KATANA_OFFSET_SEQUENCE] = sequence;
}

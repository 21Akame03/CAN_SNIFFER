#include "Oracle.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define ORACLE_JSON_TYPE "can"

size_t Oracle_FormatCANFrame(const oracle_can_frame_t *frame, char *buffer, size_t buffer_len) {
    if (!frame || !buffer || buffer_len == 0) {
        return 0;
    }

    const twai_message_t *msg = &frame->message;
    const uint8_t dlc = (msg->data_length_code <= 8) ? msg->data_length_code : 8;

    char data_hex[(8 * 2) + 1] = {0};
    for (uint8_t i = 0; i < dlc; ++i) {
        snprintf(&data_hex[i * 2], sizeof(data_hex) - (i * 2), "%02X", msg->data[i]);
    }

    int written = snprintf(
        buffer,
        buffer_len,
        "{\"type\":\"%s\",\"ts_us\":%" PRIu64 ",\"id\":%u,\"ext\":%s,\"rtr\":%s,\"dlc\":%u,\"data\":\"%s\"}\n",
        ORACLE_JSON_TYPE,
        frame->timestamp_us,
        msg->identifier,
        msg->extd ? "true" : "false",
        msg->rtr ? "true" : "false",
        dlc,
        data_hex);

    if (written < 0) {
        return 0;
    }

    return (size_t)written;
}

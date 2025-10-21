#include "Oracle.h"
#include "main.h"

#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <string.h>

#define ORACLE_QUEUE_LENGTH 64
#define ORACLE_LOG_TAG "[ORACLE_JTAG]"

static QueueHandle_t s_frame_queue;
static StaticQueue_t s_frame_queue_struct;
static uint8_t s_frame_queue_storage[ORACLE_QUEUE_LENGTH * sizeof(oracle_can_frame_t)];

static uint32_t s_dropped_frames;

static inline void ensure_usb_jtag_ready(void) {
    static bool s_installed = false;
    if (!s_installed) {
        usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        esp_err_t err = usb_serial_jtag_driver_install(&cfg);
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            s_installed = true;
        } else {
            ESP_LOGW(ORACLE_LOG_TAG, "usb_serial_jtag init failed: %d", (int)err);
        }
    }
}

static inline void usb_write_bytes(const char *data, size_t length) {
    if (!data || length == 0) {
        return;
    }
    ensure_usb_jtag_ready();
    usb_serial_jtag_write_bytes((const uint8_t *)data, length, 0);
}

void Oracle_Setup(void) {
    ensure_usb_jtag_ready();

    s_frame_queue = xQueueCreateStatic(
        ORACLE_QUEUE_LENGTH,
        sizeof(oracle_can_frame_t),
        s_frame_queue_storage,
        &s_frame_queue_struct);

    if (s_frame_queue == NULL) {
        ESP_LOGE(ORACLE_LOG_TAG, "Failed to allocate CAN frame queue");
        flags.ORACLE_INIT_ERROR = true;
        return;
    }

    s_dropped_frames = 0;
}

bool Oracle_QueueFrame(const twai_message_t *msg, uint64_t timestamp_us) {
    if (!msg || !s_frame_queue) {
        return false;
    }

    oracle_can_frame_t frame = {
        .message = *msg,
        .timestamp_us = timestamp_us,
    };

    BaseType_t status = xQueueSend(s_frame_queue, &frame, 0);
    if (status != pdTRUE) {
        s_dropped_frames++;
        static uint32_t last_warn_ms;
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (now_ms - last_warn_ms >= 1000) {
            ESP_LOGW(ORACLE_LOG_TAG, "Dropping CAN frames: total=%" PRIu32, s_dropped_frames);
            last_warn_ms = now_ms;
        }
        return false;
    }

    return true;
}

void Oracle_to_laptop(void *args) {
    (void)args;

    oracle_can_frame_t frame;
    char json_buffer[160];

    for (;;) {
        if (xQueueReceive(s_frame_queue, &frame, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        size_t len = Oracle_FormatCANFrame(&frame, json_buffer, sizeof(json_buffer));
        if (len == 0) {
            continue;
        }

        if (len > sizeof(json_buffer)) {
            len = sizeof(json_buffer);
        }

        usb_write_bytes(json_buffer, len);
    }
}

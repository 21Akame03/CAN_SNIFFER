#include "can_handler.h"
#include "Oracle.h"
#include "main.h"

#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "[CAN]";

#define SN65HVD230_TX_GPIO GPIO_NUM_6
#define SN65HVD230_RX_GPIO GPIO_NUM_7
#define SN65HVD230_STANDBY_GPIO GPIO_NUM_5

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_general_config_t g_config = {
    .mode = TWAI_MODE_LISTEN_ONLY,
    .tx_io = SN65HVD230_TX_GPIO,
    .rx_io = SN65HVD230_RX_GPIO,
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 1,
    .rx_queue_len = 32,
    .alerts_enabled = TWAI_ALERT_RX_DATA | TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_ERR_PASS |
                     TWAI_ALERT_BUS_ERROR | TWAI_ALERT_ARB_LOST,
    .clkout_divider = 0,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
};

void CAN_Setup(void) {
    gpio_config_t standby_cfg = {
        .pin_bit_mask = 1ULL << SN65HVD230_STANDBY_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&standby_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure SN65HVD230 standby pin");
        flags.CAN_INTT_ERROR = true;
        return;
    }

    // Drive RS pin low to keep the transceiver in high-speed mode.
    gpio_set_level(SN65HVD230_STANDBY_GPIO, 0);

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver");
        flags.CAN_INTT_ERROR = true;
        return;
    }

    if (twai_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver");
        flags.CAN_INTT_ERROR = true;
        return;
    }

    ESP_LOGI(TAG, "TWAI started in listen-only mode (500 kbit/s)");
}

void twai_alert_task(void *args) {
    (void)args;

    uint32_t alerts = 0;
    twai_status_info_t status;

    for (;;) {
        if (twai_read_alerts(&alerts, portMAX_DELAY) != ESP_OK) {
            continue;
        }

        if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
            ESP_LOGW(TAG, "RX queue full; consider increasing ORACLE_QUEUE_LENGTH");
        }

        if (alerts & TWAI_ALERT_BUS_ERROR) {
            twai_get_status_info(&status);
            ESP_LOGW(TAG, "Bus error detected (state=%d, rx_err=%u)",
                     (int)status.state, (unsigned)status.rx_error_counter);
        }

        if (alerts & TWAI_ALERT_ARB_LOST) {
            ESP_LOGW(TAG, "Arbitration lost (should not occur in listen-only mode)");
        }

        if (alerts & TWAI_ALERT_ERR_PASS) {
            twai_get_status_info(&status);
            ESP_LOGW(TAG, "Error-passive (rx_err=%u)", (unsigned)status.rx_error_counter);
        }
    }
}

void CAN_RX_task(void *args) {
    (void)args;

    twai_message_t msg;

    for (;;) {
        esp_err_t res = twai_receive(&msg, portMAX_DELAY);
        if (res != ESP_OK) {
            if (res != ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "twai_receive failed: %d", (int)res);
            }
            continue;
        }

        uint64_t timestamp = esp_timer_get_time();
        if (!Oracle_QueueFrame(&msg, timestamp)) {
            ESP_LOGV(TAG, "Dropped CAN frame (queue full)");
        }
    }
}

#include "can_handler.h"
#include "Oracle.h"
#include "main.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdio.h>

static const char *TAG = "[CAN]";

#define SN65HVD230_TX_GPIO GPIO_NUM_9
#define SN65HVD230_RX_GPIO GPIO_NUM_46
#define SN65HVD230_STANDBY_GPIO GPIO_NUM_5

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_general_config_t g_config = {
    .mode = TWAI_MODE_LISTEN_ONLY,
    .tx_io = SN65HVD230_TX_GPIO,
    .rx_io = SN65HVD230_RX_GPIO,
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 64,
    .rx_queue_len = 4096,
    .alerts_enabled = TWAI_ALERT_RX_DATA | TWAI_ALERT_RX_QUEUE_FULL |
                      TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR |
                      TWAI_ALERT_ARB_LOST | TWAI_ALERT_BUS_OFF |
                      TWAI_ALERT_BUS_RECOVERED,
    .clkout_divider = 0,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
};

static volatile bool s_bus_recovery_in_progress;

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

  s_bus_recovery_in_progress = false;

  ESP_LOGI(TAG, "TWAI started in listen-only mode (250 kbit/s)");
}

void twai_alert_task(void *args) {
  (void)args;

  uint32_t alerts = 0;
  twai_status_info_t status;

  for (;;) {
    if (twai_read_alerts(&alerts, portMAX_DELAY) != ESP_OK) {
      continue;
    }

#if CONFIG_CAN_SNIFFER_DEBUG_TWAI_ALERT_DETAILS
    bool need_status_snapshot = false;
    ESP_LOGD(TAG, "TWAI alert mask: 0x%08" PRIx32, alerts);
#endif

    bool status_valid = false;

    if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
      ESP_LOGW(TAG, "RX queue full; consider increasing ORACLE_QUEUE_LENGTH");
#if CONFIG_CAN_SNIFFER_DEBUG_TWAI_ALERT_DETAILS
      need_status_snapshot = true;
#endif
    }

    if (alerts & TWAI_ALERT_BUS_ERROR) {
      if (!status_valid && twai_get_status_info(&status) == ESP_OK) {
        status_valid = true;
      }
      if (status_valid) {
        ESP_LOGW(TAG, "Bus error detected (state=%d, rx_err=%u)",
                 (int)status.state, (unsigned)status.rx_error_counter);
      } else {
        ESP_LOGW(TAG, "Bus error detected (status unavailable)");
      }
    }

    if (alerts & TWAI_ALERT_ARB_LOST) {
      ESP_LOGW(TAG, "Arbitration lost (should not occur in listen-only mode)");
    }

    if (alerts & TWAI_ALERT_ERR_PASS) {
      if (!status_valid && twai_get_status_info(&status) == ESP_OK) {
        status_valid = true;
      }
      if (status_valid) {
        ESP_LOGW(TAG, "Error-passive (rx_err=%u)",
                 (unsigned)status.rx_error_counter);
      } else {
        ESP_LOGW(TAG, "Error-passive (status unavailable)");
      }
    }

    if (alerts & TWAI_ALERT_BUS_OFF) {
      if (!status_valid && twai_get_status_info(&status) == ESP_OK) {
        status_valid = true;
      }
      if (status_valid) {
        ESP_LOGW(TAG, "Bus-off detected (state=%d rx_err=%u)",
                 (int)status.state, (unsigned)status.rx_error_counter);
      } else {
        ESP_LOGW(TAG, "Bus-off detected (status unavailable)");
      }

      if (!s_bus_recovery_in_progress) {
        esp_err_t err = twai_initiate_recovery();
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "Failed to initiate bus recovery (err=%d)", (int)err);
        } else {
          s_bus_recovery_in_progress = true;
          ESP_LOGI(TAG, "Bus recovery initiated");
        }
      }
    }

    if (alerts & TWAI_ALERT_BUS_RECOVERED) {
      if (s_bus_recovery_in_progress) {
        esp_err_t err = twai_start();
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "Failed to restart TWAI after bus recovery (err=%d)",
                   (int)err);
        } else {
          s_bus_recovery_in_progress = false;
          ESP_LOGI(TAG, "TWAI restarted after bus recovery");
        }
      } else {
        ESP_LOGD(TAG, "Received BUS_RECOVERED alert while not recovering");
      }
    }

#if CONFIG_CAN_SNIFFER_DEBUG_TWAI_ALERT_DETAILS
    if (!need_status_snapshot &&
        (alerts & (TWAI_ALERT_BUS_ERROR | TWAI_ALERT_ERR_PASS))) {
      need_status_snapshot = true;
    }

    if (need_status_snapshot) {
      if (!status_valid && twai_get_status_info(&status) == ESP_OK) {
        status_valid = true;
      }
      if (status_valid) {
        ESP_LOGI(TAG,
                 "Status state=%d rx_err=%u tx_err=%u msgs_rx=%u msgs_tx=%u "
                 "tx_failed=%u rx_missed=%u arb_lost=%u bus_err=%u",
                 (int)status.state, (unsigned)status.rx_error_counter,
                 (unsigned)status.tx_error_counter, (unsigned)status.msgs_to_rx,
                 (unsigned)status.msgs_to_tx, (unsigned)status.tx_failed_count,
                 (unsigned)status.rx_missed_count,
                 (unsigned)status.arb_lost_count,
                 (unsigned)status.bus_error_count);
      } else {
        ESP_LOGD(TAG, "Failed to fetch TWAI status snapshot");
      }
    }
#endif
  }
}

void CAN_RX_task(void *args) {
  (void)args;

  twai_message_t msg;
  static bool waiting_for_recovery_logged = false;

  for (;;) {
    esp_err_t res = twai_receive(&msg, portMAX_DELAY);
    if (res != ESP_OK) {
      if (res == ESP_ERR_TIMEOUT) {
        continue;
      }

      if (res == ESP_ERR_INVALID_STATE) {
        if (!waiting_for_recovery_logged) {
          ESP_LOGW(TAG,
                   "TWAI driver not ready (recovering=%s); waiting for bus",
                   s_bus_recovery_in_progress ? "true" : "false");
          waiting_for_recovery_logged = true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
      } else {
        ESP_LOGW(TAG, "twai_receive failed: %d", (int)res);
      }
      continue;
    }

    waiting_for_recovery_logged = false;

    char data_hex[(TWAI_FRAME_MAX_DLC * 3)] = {0};
    if (!msg.rtr && msg.data_length_code > 0) {
      size_t out = 0;
      for (int i = 0; i < msg.data_length_code && i < TWAI_FRAME_MAX_DLC; ++i) {
        out +=
            snprintf(&data_hex[out], sizeof(data_hex) - out, "%02X%s",
                     msg.data[i], (i + 1 == msg.data_length_code) ? "" : " ");
        if (out >= sizeof(data_hex)) {
          break;
        }
      }
    }

    // if (msg.rtr) {
    //   ESP_LOGI(TAG, "RX %s RTR id=0x%08" PRIX32 " dlc=%d",
    //            msg.extd ? "EXT" : "STD", msg.identifier,
    //            msg.data_length_code);
    // } else {
    //   ESP_LOGI(TAG, "RX %s id=0x%08" PRIX32 " dlc=%d data=%s",
    //            msg.extd ? "EXT" : "STD", msg.identifier,
    //            msg.data_length_code, msg.data_length_code ? data_hex :
    //            "<empty>");
    // }
    //
    uint64_t timestamp = esp_timer_get_time();
    if (!Oracle_QueueFrame(&msg, timestamp)) {
      ESP_LOGV(TAG, "Dropped CAN frame (queue full)");
    }
  }
}

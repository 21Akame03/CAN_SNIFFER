/*

 @@@@@@  @@@@@@@   @@@@@@   @@@@@@@ @@@      @@@@@@@@
@@!  @@@ @@!  @@@ @@!  @@@ !@@      @@!      @@!
@!@  !@! @!@!!@!  @!@!@!@! !@!      @!!      @!!!:!
!!:  !!! !!: :!!  !!:  !!! :!!      !!:      !!:
 : :. :   :   : :  :   : :  :: :: : : ::.: : : :: :::

┏━┓   ┏━╸   ╺┳╸┏━┓┏━┓╻ ╻╻ ╻
┃┣┛   ┣╸ ╺━╸ ┃ ┣┳┛┣━┫┏╋┛┏╋┛
┗━╸   ┗━╸    ╹ ╹┗╸╹ ╹╹ ╹╹ ╹

Made by Ubdhot Ashitosh
 *** *   ***   * **   *
*           *       *
      * *    *  *  * *
 *             *
     *   *      *    *
             *     *
* *     * *
   *
            *       *

      *    *     *    *
 */

#include "Oracle.h"
#include "sdkconfig.h"

#if CONFIG_CAN_SNIFFER_LOG_LEVEL_VERBOSE
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#elif CONFIG_CAN_SNIFFER_LOG_LEVEL_DEBUG
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#elif CONFIG_CAN_SNIFFER_LOG_LEVEL_WARN
#define LOG_LOCAL_LEVEL ESP_LOG_WARN
#elif CONFIG_CAN_SNIFFER_LOG_LEVEL_ERROR
#define LOG_LOCAL_LEVEL ESP_LOG_ERROR
#else
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#endif

#include "ENV_variables.h"
#include "can_handler.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "main.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define LED_GPIO 2
static const char *TAG = "Setup";

// Overall System Start Status
bool SystemInitialised;

// Flags to check functionality of System and avoid Stackoverflow due to too
// many errors
System_health_Flags flags;

static const char *log_level_to_str(esp_log_level_t level) {
  switch (level) {
  case ESP_LOG_ERROR:
    return "ERROR";
  case ESP_LOG_WARN:
    return "WARN";
  case ESP_LOG_INFO:
    return "INFO";
  case ESP_LOG_DEBUG:
    return "DEBUG";
  case ESP_LOG_VERBOSE:
    return "VERBOSE";
  default:
    return "UNKNOWN";
  }
}

static esp_log_level_t get_configured_log_level(void) {
#if CONFIG_CAN_SNIFFER_LOG_LEVEL_VERBOSE
  return ESP_LOG_VERBOSE;
#elif CONFIG_CAN_SNIFFER_LOG_LEVEL_DEBUG
  return ESP_LOG_DEBUG;
#elif CONFIG_CAN_SNIFFER_LOG_LEVEL_WARN
  return ESP_LOG_WARN;
#elif CONFIG_CAN_SNIFFER_LOG_LEVEL_ERROR
  return ESP_LOG_ERROR;
#else
  return ESP_LOG_INFO;
#endif
}

static void configure_logging(void) {
  const esp_log_level_t level = get_configured_log_level();

#if CONFIG_CAN_SNIFFER_OVERRIDE_GLOBAL_LOG_LEVEL
  esp_log_level_set("*", level);
#else
  esp_log_level_set(TAG, level);
  esp_log_level_set("[CAN]", level);
  esp_log_level_set("[ORACLE_JTAG]", level);
#endif

  ESP_LOGI(TAG, "Log level configured: %s", log_level_to_str(level));
}

#if CONFIG_CAN_SNIFFER_DEBUG_STARTUP_SUMMARY
static void log_module_status(const char *module, bool enabled, bool flag) {
  if (!enabled) {
    ESP_LOGI(TAG, "%s disabled by configuration", module);
    return;
  }

  if (flag) {
    ESP_LOGE(TAG, "%s initialisation failed", module);
  } else {
    ESP_LOGI(TAG, "%s initialised successfully", module);
  }
}
#endif

static void log_health_summary(void) {
#if CONFIG_CAN_SNIFFER_DEBUG_STARTUP_SUMMARY
  if (!SystemHasError(&flags)) {
    ESP_LOGI(TAG, "System health summary: all subsystems OK");
    return;
  }

  ESP_LOGW(TAG, "System health summary: issues detected");

  struct {
    bool flag;
    const char *name;
  } report[] = {
      {flags.CAN_INTT_ERROR, "CAN interface"},
      {flags.UART_INIT_ERROR, "UART"},
      {flags.WIFI_INIT_ERROR, "WiFi"},
      {flags.ALBERT_INIT_ERROR, "Albert"},
      {flags.DIAG_INIT_ERROR, "Diagnostics"},
      {flags.RTOS_INIT_ERROR, "RTOS"},
      {flags.Charging_INIT_ERROR, "Charging"},
      {flags.ROBIN_INIT_ERROR, "Robin"},
      {flags.ORACLE_INIT_ERROR, "Oracle"},
  };

  for (size_t i = 0; i < sizeof(report) / sizeof(report[0]); ++i) {
    if (report[i].flag) {
      ESP_LOGE(TAG, " - %s reported an error", report[i].name);
    }
  }
#endif
}

/*
 * Startup Procedure
 *   CAN Setup()
 * - Configure Network
 * - Configure Semaphore for CAN Transmission
 *
 *
 *   WIFI Setup()
 * 1. Setup Wifi Connection
 * 2. Setup DHCP
 * 3. Setup Static Connection
 * 4. Start Wifi AP
 *
 */

static inline bool SystemHasError(const System_health_Flags *f) {
  return f->bits != 0; // any flag set
}

bool initialise_setups(void) {
  // Default to initialised; flip to false only if we detect a fault
  SystemInitialised = true;
  memset(&flags, 0, sizeof(flags));

#if CONFIG_CAN_SNIFFER_DEBUG_STARTUP_SUMMARY
  ESP_LOGI(TAG, "Initialising subsystems (CAN: %s, Oracle: %s)",
           CAN_on ? "enabled" : "disabled", Oracle_on ? "enabled" : "disabled");
#endif

  if (CAN_on) {
    CAN_Setup();
  }

  if (Oracle_on) {
    Oracle_Setup();
  }

  // Charging_protocol ();

#if CONFIG_CAN_SNIFFER_DEBUG_STARTUP_SUMMARY
  log_module_status("CAN", CAN_on, flags.CAN_INTT_ERROR);
  log_module_status("Oracle", Oracle_on, flags.ORACLE_INIT_ERROR);
#endif

  return !SystemHasError(&flags);
}

void Start_schedule(void) {

  // Conditional activation of Modules based on Error reports
  if (CAN_on && !flags.CAN_INTT_ERROR) {
    TaskHandle_t rx_handle = NULL;
    TaskHandle_t alert_handle = NULL;

    BaseType_t rx_status = xTaskCreatePinnedToCore(
        CAN_RX_task, "can_rx", 4096, NULL, 6, &rx_handle, 1);
    if (rx_status != pdPASS) {
      flags.RTOS_INIT_ERROR = true;
      ESP_LOGE(TAG, "Failed to start CAN_RX_task (err=%ld)", (long)rx_status);
    }
#if CONFIG_CAN_SNIFFER_DEBUG_TASK_EVENTS
    else {
      ESP_LOGI(TAG, "CAN_RX_task started (handle=%p, core=%d, priority=%d)",
               (void *)rx_handle, 1, 6);
    }
#endif

    BaseType_t alert_status = xTaskCreatePinnedToCore(
        twai_alert_task, "twai_alert_task", 4096, NULL, 5, &alert_handle,
        tskNO_AFFINITY);
    if (alert_status != pdPASS) {
      flags.RTOS_INIT_ERROR = true;
      ESP_LOGE(TAG, "Failed to start twai_alert_task (err=%ld)",
               (long)alert_status);
    }
#if CONFIG_CAN_SNIFFER_DEBUG_TASK_EVENTS
    else {
      ESP_LOGI(TAG,
               "twai_alert_task started (handle=%p, requested_affinity=%d, priority=%d)",
               (void *)alert_handle, (int)tskNO_AFFINITY, 5);
    }
#endif
  } else if (CAN_on) {
    ESP_LOGW(TAG, "CAN Tasks will not start!");
  }

  if (Oracle_on && !flags.ORACLE_INIT_ERROR) {
    TaskHandle_t oracle_handle = NULL;
    BaseType_t oracle_status =
        xTaskCreate(Oracle_to_laptop, "Oracle", 4096, NULL, 4, &oracle_handle);
    if (oracle_status != pdPASS) {
      flags.RTOS_INIT_ERROR = true;
      ESP_LOGE(TAG, "Failed to start Oracle task (err=%ld)",
               (long)oracle_status);
    }
#if CONFIG_CAN_SNIFFER_DEBUG_TASK_EVENTS
    else {
      ESP_LOGI(TAG, "Oracle task started (handle=%p, priority=%d)",
               (void *)oracle_handle, 4);
    }
#endif
  } else if (Oracle_on) {
    ESP_LOGW(TAG, "Oracle Task will not start!");
  }
}

void app_main(void) {

  // Configure the pin as output
  gpio_config_t io_conf = {.pin_bit_mask = 1ULL << LED_GPIO,
                           .mode = GPIO_MODE_OUTPUT,
                           .pull_up_en = 0,
                           .pull_down_en = 0,
                           .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&io_conf);

  // Set pin high to turn on LED

  configure_logging();

  ESP_LOGI(TAG, "heap before timers: %" PRIu32, esp_get_free_heap_size());

  if (initialise_setups()) {
    ESP_LOGI(TAG, "[+] Startup Process Completed\n");
  } else {
    ESP_LOGW(TAG, "[@] Startup Process Completed with Errors\n");
    gpio_set_level(LED_GPIO, 1);
  }

  log_health_summary();

  Start_schedule();

  uxTaskGetStackHighWaterMark(NULL);
}

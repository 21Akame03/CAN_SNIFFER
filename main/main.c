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
#define LOG_LOCAL_LEVEL ESP_LOG_WARN

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

  if (CAN_on) {
    CAN_Setup();
  }

  if (Oracle_on) {
    Oracle_Setup();
  }

  // Charging_protocol ();

  return !SystemHasError(&flags);
}

void Start_schedule(void) {

  // Conditional activation of Modules based on Error reports
  if (CAN_on && !flags.CAN_INTT_ERROR) {
    xTaskCreatePinnedToCore(CAN_RX_task, "can_rx", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(twai_alert_task, "twai_alert_task", 4096, NULL, 5,
                            NULL, tskNO_AFFINITY);
  } else if (CAN_on) {
    ESP_LOGW(TAG, "CAN Tasks will not start!");
  }

  if (Oracle_on && !flags.ORACLE_INIT_ERROR) {
    xTaskCreate(Oracle_to_laptop, "Oracle", 4096, NULL, 4, NULL);
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

  ESP_LOGI(TAG, "heap before timers: %" PRIu32, esp_get_free_heap_size());

  if (initialise_setups()) {
    ESP_LOGI(TAG, "[+] Startup Process Completed\n");
  } else {
    ESP_LOGW(TAG, "[@] Startup Process Completed with Errors\n");
    gpio_set_level(LED_GPIO, 1);
  }

  Start_schedule();

  uxTaskGetStackHighWaterMark(NULL);
}

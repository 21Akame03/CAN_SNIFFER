#include "diagnostic_handler.h"
// #include "esp_log.h"
// #include "driver/usb_serial_jtag.h"
// #include "esp_err.h"
// #include <string.h>
// #include <stdlib.h>
// #include "ENV_variables.h"
// #include "Oracle.h"
// #include "Robin_handler.h"
//
// static const char *TAG = "[ORACLE]";
// // hard cap per message
// #define RX_LINE_MAX 512
// // usb fifo read granularity
// #define READ_CHUNK 64
//
// SemaphoreHandle_t Oracle_semaphore;
// TimerHandle_t Oracle_timer;
//
// /*--------------------------------------------*/
// // Definitions
// void Oracle_timer_callback (TimerHandle_t Oracle_timer);
// /*--------------------------------------------*/
//
// static inline void
// ensure_usb_jtag_ready (void)
// {
//     static bool s_installed = false;
//     if (!s_installed)
// 	{
// 	    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT ();
// 	    esp_err_t err = usb_serial_jtag_driver_install (&cfg);
// 	    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE)
// 		{
// 		    s_installed = true;
// 		}
// 	    else
// 		{
// 		    ESP_LOGW (TAG, "usb_serial_jtag init failed: %d", (int)err);
// 		}
// 	}
// }
//
// static inline int
// usb_write_str (const char *s)
// {
//     if (!s)
// 	return 0;
//     ensure_usb_jtag_ready ();
//     size_t len = strlen (s);
//     if (len == 0)
// 	return 0;
//     return usb_serial_jtag_write_bytes ((const uint8_t *)s, len, 0);
// }
//
// void
// Oracle_Setup (void)
// {
//     ESP_LOGI (TAG, "Initializing Oracle USB Serial JTAG transport");
//     ensure_usb_jtag_ready ();
// }
//
// void
// Oracle_timer_callback (TimerHandle_t Oracle_timer)
// {
//     // Software timer callback
//     if (xSemaphoreGive (Oracle_semaphore) != pdPASS)
// 	{
// 	    ESP_LOGE (TAG, "Failed to give Semaphore to Oracle");
// 	}
//     else
// 	{
// 	    ESP_LOGD (TAG, "Semaphore given Successfully");
// 	}
// }
//
// // void
// // Oracle_to_laptop (void *args)
// // {
// //     (void)args;
// //     // Send diagnostic JSON
// //     char *diag_json = Oracle_SerializeDiagnostic();
// //     if (diag_json) {
// //         usb_write_str(diag_json);
// //         usb_write_str("\n");
// //         free(diag_json);
// //     }
// //
// //     // Send cell data JSON
// //     char *cells_json = Oracle_SerializeCellData();
// //     if (cells_json) {
// //         usb_write_str(cells_json);
// //         usb_write_str("\n");
// //         free(cells_json);
// //     }
// // }

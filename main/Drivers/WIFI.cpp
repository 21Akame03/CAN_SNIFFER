// #include "OTA.h"
// #include "esp_err.h"
// #include "esp_event.h"
// #include "esp_log.h"
// #include "esp_netif.h"
// #include "esp_ota_ops.h"
// #include "esp_wifi.h"
// #include "lwip/ip4_addr.h"
// #include "nvs_flash.h"
// #include <string>
// #include <memory>
// #include "Oracle/Oracle.h"
//
// namespace {
//     const char* const TAG = "WIFI";
//     esp_netif_t* ap_netif = nullptr;
//     httpd_handle_t server = nullptr;
//
//     class WiFiManager {
//     private:
//         static esp_err_t wifi_init_nvs() {
//             esp_err_t ret = nvs_flash_init();
//             if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//                 ESP_LOGI(TAG, "NVS partition erased and will be initialized");
//                 ESP_ERROR_CHECK(nvs_flash_erase());
//                 ret = nvs_flash_init();
//             }
//             return ret;
//         }
//
//         static void wifi_cleanup() {
//             if (server) {
//                 httpd_stop(server);
//                 server = nullptr;
//             }
//
//             if (ap_netif) {
//                 esp_netif_dhcps_stop(ap_netif);
//                 esp_netif_destroy(ap_netif);
//                 ap_netif = nullptr;
//             }
//
//             esp_wifi_stop();
//             esp_wifi_deinit();
//             esp_event_loop_delete_default();
//             esp_netif_deinit();
//             nvs_flash_deinit();
//         }
//
//         static esp_err_t reset_handler(httpd_req_t* req) {
//             ESP_LOGI(TAG, "Received system reset request");
//
//             const std::string response = "{\"status\":\"restarting\"}";
//             httpd_resp_send(req, response.c_str(), response.length());
//
//             vTaskDelay(pdMS_TO_TICKS(100));
//             esp_restart();
//
//             return ESP_OK;
//         }
//
//     public:
//         static esp_err_t setup() {
//             esp_err_t ret = ESP_OK;
//
//             // 1. Initialize NVS
//             ret = wifi_init_nvs();
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
//                 return ret;
//             }
//
//             // 2. Initialize the TCP/IP stack
//             ret = esp_netif_init();
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to initialize TCP/IP stack: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             // 3. Create default event loop
//             ret = esp_event_loop_create_default();
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             // 4. Create default Wi-Fi AP netif
//             ap_netif = esp_netif_create_default_wifi_ap();
//             if (!ap_netif) {
//                 ESP_LOGE(TAG, "Failed to create default AP netif");
//                 wifi_cleanup();
//                 return ESP_FAIL;
//             }
//
//             // 5. Initialize Wi-Fi with default config
//             wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//             ret = esp_wifi_init(&cfg);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to initialize Wi-Fi: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             // 6. Configure and Start Wi-Fi in AP mode
//             wifi_config_t wifi_config = {};
//             const std::string ssid = "BATMAN";
//             const std::string password = "IamBATmanExxe25";
//
//             std::copy(ssid.begin(), ssid.end(), wifi_config.ap.ssid);
//             wifi_config.ap.ssid_len = ssid.length();
//             std::copy(password.begin(), password.end(), wifi_config.ap.password);
//             wifi_config.ap.max_connection = 5;
//             wifi_config.ap.authmode = WIFI_AUTH_OPEN;
//
//             ret = esp_wifi_set_mode(WIFI_MODE_AP);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to set Wi-Fi mode: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to set Wi-Fi config: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             // 7-9. Configure IP and DHCP
//             ret = esp_netif_dhcps_stop(ap_netif);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to stop DHCP server: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             esp_netif_ip_info_t ip_info = {};
//             IP4_ADDR(&ip_info.ip, 192, 168, 5, 1);
//             IP4_ADDR(&ip_info.gw, 192, 168, 5, 1);
//             IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
//
//             ret = esp_netif_set_ip_info(ap_netif, &ip_info);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to set IP info: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             ret = esp_netif_dhcps_start(ap_netif);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to start DHCP server: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//             ESP_LOGI(TAG, "Static AP IP set to: " IPSTR, IP2STR(&ip_info.ip));
//
//             // 10. Start Wi-Fi
//             ret = esp_wifi_start();
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             ESP_LOGI(TAG, "Wi-Fi AP started.");
//
//             // 11. Start HTTP server
//             httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//             ret = httpd_start(&server, &config);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             // 12. Register endpoints
//             const httpd_uri_t ota_uri = {
//                 .uri = "/update",
//                 .method = HTTP_POST,
//                 .handler = Ota_updater,
//                 .user_ctx = nullptr
//             };
//
//             ret = httpd_register_uri_handler(server, &ota_uri);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to register OTA URI handler: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             const httpd_uri_t reset_uri = {
//                 .uri = "/reset",
//                 .method = HTTP_POST,
//                 .handler = reset_handler,
//                 .user_ctx = nullptr
//             };
//
//             ret = httpd_register_uri_handler(server, &reset_uri);
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to register reset URI handler: %s", esp_err_to_name(ret));
//                 wifi_cleanup();
//                 return ret;
//             }
//
//             // Register Oracle endpoints (diagnostic and cell data)
//             Oracle_Setup(server);
//
//             return ESP_OK;
//         }
//
//         static void cleanup() {
//             wifi_cleanup();
//         }
//
//         static void reset() {
//             ESP_LOGI(TAG, "Resetting WiFi and HTTP server...");
//
//             wifi_cleanup();
//
//             esp_err_t ret = setup();
//             if (ret != ESP_OK) {
//                 ESP_LOGE(TAG, "Failed to reset WiFi: %s", esp_err_to_name(ret));
//             } else {
//                 ESP_LOGI(TAG, "WiFi reset successful");
//             }
//         }
//     };
// }
//
// // Export C interface for compatibility
// extern "C" {
//     esp_err_t WIFI_Setup(void) {
//         return WiFiManager::setup();
//     }
//
//     void WIFI_Cleanup(void) {
//         WiFiManager::cleanup();
//     }
//
//     void WIFI_Reset(void) {
//         WiFiManager::reset();
//     }
// }

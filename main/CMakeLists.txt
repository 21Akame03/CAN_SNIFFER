# idf_component_register(SRCS "bq79616_esp32.c" "WIFI.cpp" "flash_storage.c" "Batman_esp.c" "can_handler.c" "diagnostic_handler.c" "spi_handler.c" "Robin" "OTA" "WIFI.c" "Albert" "Diag"
#                     INCLUDE_DIRS "include")

idf_component_register(
    SRCS
        "main.c"
        "Drivers/can_handler.c"
        "Oracle/Oracle_parsing.c"
        "Oracle/Oracle_usb_jtag.c"
    INCLUDE_DIRS
        "."
        "include"
        "Oracle"
    REQUIRES
        driver
        esp_timer
)

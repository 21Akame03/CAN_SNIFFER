#ifndef BATMAN_ESP_H
#define BATMAN_ESP_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <stdint.h>

// Shows which section is working and not working
// Used for internal debugging and logging

typedef union
{
    struct
    {
	bool CAN_INTT_ERROR : 1;
	bool UART_INIT_ERROR : 1;
	bool WIFI_INIT_ERROR : 1;
	bool ALBERT_INIT_ERROR : 1;
	bool DIAG_INIT_ERROR : 1;
	bool RTOS_INIT_ERROR : 1;
	bool Charging_INIT_ERROR : 1;
	bool ROBIN_INIT_ERROR : 1;
	bool ORACLE_INIT_ERROR : 1;
    };

    uint32_t bits; // all flags placed together
} System_health_Flags;

extern System_health_Flags flags;

#endif // BATMAN_ESP_H

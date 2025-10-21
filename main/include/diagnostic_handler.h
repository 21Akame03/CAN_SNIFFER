#ifndef DIAGH_H
#define DIAGH_H

#include "main.h"
#include "freertos/idf_additions.h"
#include <stdint.h>

typedef struct
{
    uint32_t Overall_voltage : 13;	 // 0-12 Bits
    uint32_t Highest_temp_recorded : 14; // 13-26 Bits
    uint32_t Curr_value : 13;		 // 27-39 Bits
    uint32_t SOC : 8;			 // 40-47 Bits

    // Error Flags (matching DBC bit positions)
    uint32_t lost_comm : 1;	       // Bit 48  Flag 0
    uint32_t Voltage_sensor_loss : 1;  // Bit 49  Flag 1
    uint32_t Battery_overvoltage : 1;  // Bit 50  Flag 2
    uint32_t Charging_on : 1;	       // Bit 51  Flag 3
    uint32_t temp_sensor_loss : 1;     // Bit 52  Flag 4
    uint32_t Battery_undervoltage : 1; // Bit 53  Flag 5
    uint32_t curr_sensor_loss : 1;     // Bit 54  Flag 6
    uint32_t Over_templimit : 1;       // Bit 55  Flag 7
    uint32_t System_health : 1;	       // Bit 56  Flag 8
    uint32_t Cell_overvoltage : 1;     // Bit 57  Flag 9
    uint32_t Cell_undervoltage : 1;    // Bit 58  Flag 10

    // Reserved bits to complete the 64-bit structure
    uint32_t Reserved : 6; // Bits 56-63
} Diag_message_t;

// Union to access both as Struct and byte array
typedef union
{
    Diag_message_t fields;
    uint8_t bytes[10];
} diag_frame_t;

// Diagnostic Flags and values
// TODO: remove SOH from whole firmware
typedef struct __attribute__ ((aligned (4)))
{
    uint16_t overall_voltage;
    uint16_t current;
    uint16_t hightemp;
    uint8_t soc;
    uint8_t soh;

    uint8_t flags[11];
} Diagnostic_Container_t;

extern Diagnostic_Container_t diag __attribute__ ((aligned (4)));

/*-----------------------------------------------------------------------------*/
void Diag_Setup (void);
void Diagnostic_check (void *argumensts);
void SOC (void);
float calculate_OCV (void);
uint8_t get_soc_from_voltage (float voltage_per_cell);
void Check_COMM_status ();
void Check_connections_and_limits (Diagnostic_Container_t *diag);
void Check_limits (uint8_t *flags);
void Fault_management (uint8_t *Flags);
uint16_t calculate_TS_Voltage ();

extern SemaphoreHandle_t diagnostic_semaphore;
extern TimerHandle_t diagnostic_timer;

#endif

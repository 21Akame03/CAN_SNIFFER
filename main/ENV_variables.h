#ifndef ENV_H
#define ENV_H

#include "driver/twai.h"
#include "esp_twai.h"

// Flip this single switch: true = fake BMS data over USB, false = normal CAN.
#define DEMO_MODE false

// Module switches — derived from DEMO_MODE so you only change one place.
#define Oracle_on true
#define CAN_on (!DEMO_MODE)

// Set to true when testing a lone node with no other ACK-capable nodes on bus.
// The ESP32 will run in TWAI_MODE_NORMAL and ACK every received frame,
// preventing the remote node from going BUS OFF due to missing ACKs.
// Set back to false for production sniffing (listen-only, invisible on bus).
#define CAN_ACK_MODE true


#endif

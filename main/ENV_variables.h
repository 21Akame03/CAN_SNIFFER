#ifndef ENV_H
#define ENV_H

#include "driver/twai.h"
#include "esp_twai.h"

// Module switches
#define Oracle_on true
#define CAN_on true

#define CAN_BAUD TWAI_TIMING_CONFIG_500KBITS()

#endif

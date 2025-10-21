#ifndef ROBIN_TYPES_H
#define ROBIN_TYPES_H

#include "ENV_variables.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint16_t individual_voltages[TOTALBOARDS - 1][10];
    uint16_t individual_temperatures[TOTALBOARDS - 1][10];
    bool individual_cell_balancing_on[TOTALBOARDS - 1][10];
    uint16_t overall_voltage;
    uint16_t max_current;
    uint16_t max_cell_voltage;
    float lowest_cell_voltage;
    float max_temperature;
    float current;
    uint16_t SOC;
} Robin_container_t;

extern Robin_container_t *robin;

#endif

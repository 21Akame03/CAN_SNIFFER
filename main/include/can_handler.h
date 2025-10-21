#ifndef CANH_H
#define CANH_H

#include "freertos/idf_additions.h"
#include <stdint.h>

#define CAN_QUEUE_SIZE 20
#define CAN_MAX_DATA_LENGTH 8

/*-------------------------------------------*/
// Definitions
void CAN_Setup(void);
void CAN_RX_task(void *args);
void twai_alert_task(void *args);

#endif // MY_FUNCTIONS_H

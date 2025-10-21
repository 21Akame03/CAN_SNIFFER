#ifndef ORACLE_H
#define ORACLE_H

#include "freertos/idf_additions.h"
#include "driver/twai.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    twai_message_t message;
    uint64_t timestamp_us;
} oracle_can_frame_t;

void Oracle_Setup(void);
void Oracle_to_laptop(void *args);
bool Oracle_QueueFrame(const twai_message_t *msg, uint64_t timestamp_us);
size_t Oracle_FormatCANFrame(const oracle_can_frame_t *frame, char *buffer, size_t buffer_len);

#ifdef __cplusplus
}
#endif

#endif // ORACLE_H

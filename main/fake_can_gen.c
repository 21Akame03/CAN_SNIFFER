/*
 * Fake CAN frame generator — software injection into the Oracle queue.
 *
 * Produces frames that match the BATMAN.dbc signal layout so that the
 * JSON arriving on the laptop over USB-JTAG looks identical to what a
 * real BMS would send on the CAN bus.
 *
 * Message map (from BATMAN.dbc):
 *   0x100  individual_temperature  — 140 cells, mux 1-28, 5 per frame
 *   0x202  individual_voltages     — 140 cells, mux 1-28, 5 per frame
 *   0x199  Diagnostic_Code         — pack voltage, temp, current, SOC, flags
 */

#include "fake_can_gen.h"
#include "Oracle.h"

#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

/* DBC message IDs */
#define MSG_ID_TEMPERATURE 0x100 /* 256 */
#define MSG_ID_VOLTAGE     0x202 /* 514 */
#define MSG_ID_DIAGNOSTIC  0x199 /* 409 */

/* How fast to cycle (ms).  Full 140-cell sweep = MUX_COUNT * this value. */
#define FAKE_CAN_CYCLE_MS  50
#define MUX_COUNT           28
#define SIGNALS_PER_MUX     5

/* ---------- helpers ---------------------------------------------------- */

/*
 * Pack an unsigned integer into a CAN data byte array (little-endian / Intel
 * byte order, matching the @1 specifier in the DBC).
 */
static void pack_le(uint8_t *data, uint32_t raw,
                    uint8_t start_bit, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        uint8_t bit = start_bit + i;
        if (raw & (1u << i)) {
            data[bit / 8] |= (1u << (bit % 8));
        }
    }
}

/* Return a random float in  [base - jitter, base + jitter]. */
static float rand_jitter(float base, float jitter) {
    float norm = (float)esp_random() / (float)UINT32_MAX; /* 0 … 1 */
    return base + jitter * (2.0f * norm - 1.0f);
}

/* ---------- frame builders --------------------------------------------- */

/*
 * individual_temperature (0x100)
 *   Mux:          bits 0-7   (8 bit, unsigned)
 *   cell_temp_N:  10 bit each at bit offsets 8, 18, 28, 38, 48
 *                 factor 0.1, offset 0, unit °C
 */
static void build_temperature_frame(twai_message_t *msg, uint8_t mux,
                                    const float temps[5]) {
    memset(msg, 0, sizeof(*msg));
    msg->identifier        = MSG_ID_TEMPERATURE;
    msg->data_length_code  = 8;

    pack_le(msg->data, mux, 0, 8);

    static const uint8_t pos[5] = {8, 18, 28, 38, 48};
    for (int i = 0; i < 5; i++) {
        uint16_t raw = (uint16_t)(temps[i] / 0.1f);
        if (raw > 1023) raw = 1023;
        pack_le(msg->data, raw, pos[i], 10);
    }
}

/*
 * individual_voltages (0x202)
 *   Mux:            bits 0-7   (8 bit, unsigned)
 *   cell_voltage_N: 10 bit each at bit offsets 8, 18, 28, 38, 48
 *                   factor 0.01, offset 0.001, unit V
 */
static void build_voltage_frame(twai_message_t *msg, uint8_t mux,
                                const float volts[5]) {
    memset(msg, 0, sizeof(*msg));
    msg->identifier        = MSG_ID_VOLTAGE;
    msg->data_length_code  = 8;

    pack_le(msg->data, mux, 0, 8);

    static const uint8_t pos[5] = {8, 18, 28, 38, 48};
    for (int i = 0; i < 5; i++) {
        float raw_f = (volts[i] - 0.001f) / 0.01f;
        uint16_t raw = (raw_f > 0.0f) ? (uint16_t)raw_f : 0;
        if (raw > 1023) raw = 1023;
        pack_le(msg->data, raw, pos[i], 10);
    }
}

/*
 * Diagnostic_Code (0x199)
 *   Overall_voltage       : bit  0, 13 bits, factor 0.1,  unsigned  (V)
 *   Highest_temp_recorded : bit 13, 14 bits, factor 0.01, unsigned  (°C)
 *   Curr_value            : bit 27, 16 bits, factor 0.1,  signed    (A)
 *   SOC                   : bit 43,  8 bits, factor 1,    signed
 *   Flags                 : bits 51-61 (1-bit each, all cleared = healthy)
 */
static void build_diagnostic_frame(twai_message_t *msg,
                                   float pack_voltage,
                                   float highest_temp,
                                   float current,
                                   int8_t soc) {
    memset(msg, 0, sizeof(*msg));
    msg->identifier        = MSG_ID_DIAGNOSTIC;
    msg->data_length_code  = 8;

    uint16_t v_raw = (uint16_t)(pack_voltage / 0.1f);
    if (v_raw > 8191) v_raw = 8191;
    pack_le(msg->data, v_raw, 0, 13);

    uint16_t t_raw = (uint16_t)(highest_temp / 0.01f);
    if (t_raw > 16383) t_raw = 16383;
    pack_le(msg->data, t_raw, 13, 14);

    int16_t i_raw = (int16_t)(current / 0.1f);
    pack_le(msg->data, (uint32_t)(uint16_t)i_raw, 27, 16);

    pack_le(msg->data, (uint32_t)(uint8_t)soc, 43, 8);

    /* All fault flags stay zero — healthy system. */
}

/* ---------- RTOS task -------------------------------------------------- */

void FakeCAN_task(void *args) {
    (void)args;

    uint8_t mux = 1;

    for (;;) {
        uint64_t ts = esp_timer_get_time();
        twai_message_t msg;

        /* --- temperature frame for this mux slot --- */
        float temps[SIGNALS_PER_MUX];
        for (int i = 0; i < SIGNALS_PER_MUX; i++) {
            temps[i] = rand_jitter(28.0f, 4.0f); /* ~24-32 °C */
        }
        build_temperature_frame(&msg, mux, temps);
        Oracle_QueueFrame(&msg, ts);

        /* --- voltage frame for this mux slot --- */
        float volts[SIGNALS_PER_MUX];
        for (int i = 0; i < SIGNALS_PER_MUX; i++) {
            volts[i] = rand_jitter(3.65f, 0.15f); /* ~3.50-3.80 V */
        }
        build_voltage_frame(&msg, mux, volts);
        Oracle_QueueFrame(&msg, ts + 1);

        /* --- diagnostic frame (sent every cycle) --- */
        build_diagnostic_frame(&msg,
                               rand_jitter(504.0f, 5.0f),  /* pack V  */
                               rand_jitter(32.0f, 2.0f),   /* hi temp */
                               rand_jitter(15.0f, 3.0f),   /* current */
                               75);                         /* SOC %   */
        Oracle_QueueFrame(&msg, ts + 2);

        /* advance mux: 1 → 28 → 1 … */
        if (++mux > MUX_COUNT) mux = 1;

        vTaskDelay(pdMS_TO_TICKS(FAKE_CAN_CYCLE_MS));
    }
}

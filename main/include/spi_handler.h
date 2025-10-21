#ifndef SPIH_H
#define SPIH_H

#include "driver/spi_master.h"
#include <stddef.h>
#include <stdint.h>

void ISOSPI_Setup (void);

// SPI commands (Normal function)
esp_err_t isoSPI_tx_rx (const spi_device_handle_t dev, const uint8_t *tx_data, size_t tx_len, uint8_t *rx_data,
			size_t rx_len);

uint16_t Compute_Command_PEC (const uint8_t *data, size_t data_length);
uint16_t Compute_Data_PEC (const uint8_t *data, size_t data_length);
uint16_t pec15_calc (const uint8_t *data, uint8_t len);

extern spi_device_handle_t spi_cs1, spi_cs2;
extern const uint16_t crc15Table[256];
#endif

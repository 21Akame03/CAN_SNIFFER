#ifndef UARTH_H
#define UARTH_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "stdio.h"

bool bq79600_wake_tone ();
extern const uint16_t TI_crc16_table[256];

void UART_Setup ();
esp_err_t bq_uart_tx_rx (const uint8_t *tx_data, size_t tx_len, uint8_t *rx_data, int rx_len);
uint16_t CRC16_IBM (uint8_t *pBuf, int sendLen);
int bq_WriteReg (uint8_t id, uint16_t addr, uint64_t data, uint8_t len, uint8_t write_type);
int bq_ReadReg (uint8_t bID, uint16_t wAddr, uint8_t *pData, uint8_t bLen, uint32_t dwTimeOut, uint8_t bWriteType);
void Bq79600_clrFLT ();

// Error
typedef struct
{
    bool CONF_MON_ERR;
    bool FACTLDERR;
    bool FACT_CRC;
    bool VALIDATE_DET;
    bool LFO;
    bool SHUTDOWN_REC;
    bool DRST;
    bool CTL;
    bool CTS;
    bool TSHUT;
    bool INH;
} BQ79600_err_t;
extern BQ79600_err_t BQ79600_errors_present;

/* Constants -----------------------------------------------------------------*/
// Timing constants (in milliseconds)
#define BQ79600_WAKE_PING_TIME_MS 4    // 2.75ms for WAKE ping low time
#define BQ79600_WAKE_SETUP_TIME_MS 4   // 3.5ms wait after WAKE ping
#define BQ79600_WAKE_TONE_TIME_MS 1.6  // 1.6ms for WAKE tone duration
#define BQ79600_ACTIVE_MODE_TIME_MS 10 // 10ms to enter ACTIVE mode

// Command types
#define CMD_SINGLE_DEV_READ 0x80   // Read from a single device
#define CMD_SINGLE_DEV_WRITE 0x90  // Write to a single device
#define CMD_STACK_READ 0xA0	   // Read from a stack of devices (multiple in series)
#define CMD_STACK_WRITE 0xB0	   // Write to a stack of devices (multiple in series)
#define CMD_BROADCAST_READ 0xC0	   // Read from all devices simultaneously (broadcast)
#define CMD_BROADCAST_WRITE 0xD0   // Write to all devices simultaneously (broadcast)
#define CMD_BROADCAST_WRITE_R 0xE0 // Broadcast write in reverse direction (from top to bottom of stack)

// Register addresses
#define REG_DIR0_ADDR 0x0306 // Direction register 0 (controls communication direction)
#define REG_DIR1_ADDR 0x0307 // Direction register 1 (controls communication direction)
#define REG_COMM_CTRL 0x0308 // Communication control register
#define REG_CONTROL1 0x0309  // Control register 1 (general device control)
#define REG_CONTROL2 0x030A  // Control register 2 (general device control)
#define REG_DEV_CONF1 0x2001 // Device configuration register 1

// Constants
#define FRMWRT_SGL_R 0x00     // Frame write: single device READ
#define FRMWRT_SGL_W 0x10     // Frame write: single device WRITE
#define FRMWRT_STK_R 0x20     // Frame write: stack READ
#define FRMWRT_STK_W 0x30     // Frame write: stack WRITE
#define FRMWRT_ALL_R 0x40     // Frame write: broadcast READ
#define FRMWRT_ALL_W 0x50     // Frame write: broadcast WRITE
#define FRMWRT_REV_ALL_W 0x60 // Frame write: broadcast WRITE in reverse direction

// BQ79600-Q1 REGISTER DEFINES
// Bridge_DIAG_CTRL: Diagnostic control register, used to configure and trigger diagnostic tests on the bridge device
#define Bridge_DIAG_CTRL 0x2000
// Bridge_DEV_CONF1: Device configuration register 1, sets up main device configuration options
#define Bridge_DEV_CONF1 0X2001
// Bridge_DEV_CONF2: Device configuration register 2, additional device configuration settings
#define Bridge_DEV_CONF2 0X2002
// Bridge_TX_HOLD_OFF: Transmission hold-off timer, controls delay before transmitting data
#define Bridge_TX_HOLD_OFF 0X2003
// Bridge_SLP_TIMEOUT: Sleep timeout register, sets the time before the device enters sleep mode
#define Bridge_SLP_TIMEOUT 0X2004
// Bridge_COMM_TIMEOUT: Communication timeout register, sets the maximum allowed time for communication inactivity
#define Bridge_COMM_TIMEOUT 0X2005
// Bridge_SPI_FIFO_UNLOCK: SPI FIFO unlock register, used to unlock the SPI FIFO for data transfer
#define Bridge_SPI_FIFO_UNLOCK 0X2010
// Bridge_FAULT_MSK: Fault mask register, enables or disables reporting of specific faults
#define Bridge_FAULT_MSK 0X2020
// Bridge_FAULT_RST: Fault reset register, used to clear fault conditions
#define Bridge_FAULT_RST 0X2030
// Bridge_FAULT_SUMMARY: Summary of all fault conditions detected by the device
#define Bridge_FAULT_SUMMARY 0X2100
// Bridge_FAULT_REG: General fault register, provides details on specific faults
#define Bridge_FAULT_REG 0X2101
// Bridge_FAULT_SYS: System fault register, indicates system-level faults
#define Bridge_FAULT_SYS 0X2102
// Bridge_FAULT_PWR: Power fault register, indicates faults related to power supply
#define Bridge_FAULT_PWR 0X2103
// Bridge_FAULT_COMM1: Communication fault register 1, details on communication errors (part 1)
#define Bridge_FAULT_COMM1 0X2104
// Bridge_FAULT_COMM2: Communication fault register 2, details on communication errors (part 2)
#define Bridge_FAULT_COMM2 0X2105
// Bridge_DEV_DIAG_STAT: Device diagnostic status register, shows results of diagnostic tests
#define Bridge_DEV_DIAG_STAT 0X2110
// Bridge_PARTID: Device part identification register, contains the part number
#define Bridge_PARTID 0X2120
// Bridge_DIE_ID1-Bridge_DIE_ID9: Unique die identification registers, provide unique serial numbers for the device
#define Bridge_DIE_ID1 0X2121
#define Bridge_DIE_ID2 0X2122
#define Bridge_DIE_ID3 0X2123
#define Bridge_DIE_ID4 0X2124
#define Bridge_DIE_ID5 0X2125
#define Bridge_DIE_ID6 0X2126
#define Bridge_DIE_ID7 0X2127
#define Bridge_DIE_ID8 0X2128
#define Bridge_DIE_ID9 0X2129
// Bridge_DEBUG_CTRL_UNLOCK: Debug control unlock register, used to enable debug features
#define Bridge_DEBUG_CTRL_UNLOCK 0X2200
// Bridge_DEBUG_COMM_CTRL: Debug communication control register, configures debug communication settings
#define Bridge_DEBUG_COMM_CTRL 0X2201
// Bridge_DEBUG_COMM_STAT: Debug communication status register, shows status of debug communication
#define Bridge_DEBUG_COMM_STAT 0X2300
// Bridge_DEBUG_SPI_PHY: Debug SPI physical layer register, provides SPI physical layer debug info
#define Bridge_DEBUG_SPI_PHY 0X2301
// Bridge_DEBUG_SPI_FRAME: Debug SPI frame register, provides SPI frame debug info
#define Bridge_DEBUG_SPI_FRAME 0X2302
// Bridge_DEBUG_UART_FRAME: Debug UART frame register, provides UART frame debug info
#define Bridge_DEBUG_UART_FRAME 0X2303
// Bridge_DEBUG_COMH_PHY: Debug COMH (high) physical layer register, debug info for high-side communication
#define Bridge_DEBUG_COMH_PHY 0X2304
// Bridge_DEBUG_COMH_FRAME: Debug COMH (high) frame register, debug info for high-side communication frames
#define Bridge_DEBUG_COMH_FRAME 0X2305
// Bridge_DEBUG_COML_PHY: Debug COML (low) physical layer register, debug info for low-side communication
#define Bridge_DEBUG_COML_PHY 0X2306
// Bridge_DEBUG_COML_FRAME: Debug COML (low) frame register, debug info for low-side communication frames
#define Bridge_DEBUG_COML_FRAME 0X2307

#endif

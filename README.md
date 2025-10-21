# CAN Sniffer

ESP-IDF firmware that turns an ESP32-S3 into a passive CAN bus sniffer using an SN65HVD230 transceiver. Incoming frames are streamed to a host UI over the on-board USB Serial JTAG interface as newline-delimited JSON.

## Hardware
- ESP32-S3 module or board with native USB
- SN65HVD230 CAN transceiver
- CAN bus with proper 120 Ω termination

### Default pin mapping
- ESP32 `GPIO6` → SN65HVD230 `TXD`
- ESP32 `GPIO7` → SN65HVD230 `RXD`
- ESP32 `GPIO5` → SN65HVD230 `RS` (driven low for normal mode)
- 3.3 V and GND to the transceiver supply pins
- CANH / CANL from the transceiver to the target bus

Update the pin definitions in `main/Drivers/can_handler.c` if your hardware differs.

## Build & Flash
1. Install ESP-IDF (v5 or newer recommended) and set the target to `esp32s3`.
2. From the repository root run:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

## Runtime behaviour
- Firmware enters listen-only mode at 500 kbit/s to avoid disturbing the bus.
- Every received frame is timestamped (`ts_us`) and queued for the Oracle task.
- Frames are emitted over USB Serial JTAG as one JSON object per line:
  ```json
  {"type":"can","ts_us":123456789,"id":512,"ext":false,"rtr":false,"dlc":8,"data":"1122334455667788"}
  ```
- `id` is presented in decimal. `data` is a hex string without separators.

## Integrating with a GUI
- Open the ESP32 USB Serial JTAG port at 115200 baud (flow control off).
- Parse the newline-delimited JSON stream and feed frames into your visualization.
- Frames may be dropped if the GUI cannot keep up; a warning is emitted every second while drops occur.

## Tuning
- Adjust `ORACLE_QUEUE_LENGTH` in `main/Oracle/Oracle_usb_jtag.c` to match the throughput required by your GUI.
- Change the TWAI timing configuration in `main/Drivers/can_handler.c` if you need a different bit rate.
- Use the standby pin to place the SN65HVD230 into silent mode if you want to disconnect from the bus dynamically.

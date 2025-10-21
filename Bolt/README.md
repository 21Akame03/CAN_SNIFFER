# Bolt – CAN Monitor Dashboard

A NiceGUI-based dashboard for inspecting the real-time CAN traffic produced by the ESP-IDF project under `../main`. The app mirrors the structure of the existing `Oracle` UI but is tailored to visualising raw CAN frames in a compact table and basic statistics chart.

## Features

- Serial/JTAG connection management with one-click refresh and connect/disconnect
- Live table of the most recent CAN frames (newest first)
- Quick filtering by identifier, payload bytes, or free-text matches
- Bar chart highlighting the most frequently observed identifiers
- Scrollable monitor log that captures unknown lines or connection status messages

## Requirements

- Python 3.10+
- `pip install -r requirements.txt` (NiceGUI + pyserial)

## Usage

```bash
cd Bolt
python3 src/main.py
```

The app starts on `http://127.0.0.1:8075/` by default. Select the ESP32 USB Serial/JTAG port, adjust the baud rate if needed (defaults to 921600), and click **Connect**. Incoming JSON lines formatted as

```json
{"type": "can", "ts_us": 123456, "id": 321, "dlc": 8, "data": "0102030405060708"}
```

are decoded into the “CAN Frames” table. Any non-CAN messages fall back to the *Monitor Log* for troubleshooting.

## Notes

- The dashboard keeps the latest 500 frames and 400 log entries in memory.
- Filtering is case-insensitive and matches against the hex/decimal identifier, payload string, flags, or raw line.
- The script reuses the serial helper from `Oracle` for consistency.


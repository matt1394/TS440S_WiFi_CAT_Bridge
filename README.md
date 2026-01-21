# TS-440S WiFi CAT Bridge

ESP-01S WiFi CAT bridge for the Kenwood TS-440S transceiver. Provides wireless CAT control via TCP for use with rigctl, WSJT-X, and other amateur radio software.

## Features

- **TCP Bridge** - Port 7373 for rigctl/WSJT-X/flrig compatibility
- **Web Interface** - Browser-based frequency and mode control
- **OTA Updates** - Update firmware over WiFi
- **WiFiManager** - Easy WiFi configuration via captive portal
- **mDNS** - Access via `ts440-bridge.local`

## Hardware

- ESP-01S (1MB flash)
- MAX3232 level shifter
- Kenwood TS-440S with IF-232C interface

### Wiring

```
ESP-01S TX  ->  MAX3232 TTL RX  ->  IF-232C RX
ESP-01S RX  ->  MAX3232 TTL TX  ->  IF-232C TX
```

## Installation

1. Install the Arduino IDE with ESP8266 board support
2. Install required libraries:
   - WiFiManager
   - ESP8266WiFi (included with board package)
   - ESP8266WebServer (included with board package)
   - ESP8266mDNS (included with board package)
   - ArduinoOTA (included with board package)
3. Open `TS440S_WiFi_CAT_Bridge.ino`
4. Select board: Generic ESP8266 Module
5. Upload to ESP-01S

## Configuration

Default settings in the sketch:

| Setting | Value |
|---------|-------|
| TCP Port | 7373 |
| Serial Baud | 4800 |
| Hostname | ts440-bridge |

## Usage

### First-Time WiFi Setup

1. Power on the ESP-01S
2. Connect to the `TS440-Setup` WiFi network
3. Configure your WiFi credentials in the captive portal
4. The device will restart and connect to your network

### Web Interface

Navigate to `http://ts440-bridge.local` or the device's IP address to access the web interface for manual frequency and mode control.

### WSJT-X / rigctl

Configure your software to use:
- **Rig**: Kenwood TS-440S (or Hamlib rigctl)
- **Connection**: Network
- **Address**: `ts440-bridge.local` or IP address
- **Port**: 7373

### Supported CAT Commands

- `FA` - Frequency (read/write)
- `MD` - Mode (read/write)

All other Kenwood CAT commands are passed through transparently via the TCP bridge.

## License

MIT License

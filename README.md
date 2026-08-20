# MatterClimate

[![Build](https://github.com/ferreira-igor/MatterClimate/actions/workflows/compile-sketch.yml/badge.svg)](https://github.com/ferreira-igor/MatterClimate/actions/workflows/compile-sketch.yml)
![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino-green)
![Matter](https://img.shields.io/badge/Matter-Compatible-brightgreen)

This project transforms an ESP32-based board into a Matter-compatible temperature and humidity sensor using an SHT30 sensor. It provides seamless integration with smart home ecosystems like Alexa, Google Home, and Apple Home through the Matter protocol.

Designed for smart home enthusiasts and developers, this device automatically detects and commissions itself into your Matter network, providing real-time temperature and humidity readings without complex configuration.

The system features automatic commissioning method selection (BLE or WiFi), persistent Matter configuration, robust error recovery with automatic reboot, and LED status indication for easy setup.

## Features

- **Matter Protocol Support**: Compatible with Matter-certified smart home hubs (Alexa, Google Home, Apple Home)
- **Environmental Sensing**: Accurate temperature and humidity readings with SHT30 sensor
- **Automatic Commissioning**: Smart method selection based on hardware capabilities
  - **BLE Commissioning** (ESP32-C3): Standard Matter commissioning via BLE - WiFi credentials provided by the commissioner app
  - **WiFi Commissioning** (Fallback): Captive portal for manual WiFi configuration when BLE is unavailable
- **Smart Reporting**: Configurable thresholds (0.3°C temperature, 2.0% humidity) to minimize network traffic
- **Automatic Recovery**: Detects and handles Matter disconnections, reboots after 60 seconds of lost connectivity
- **LED Status Indication**: Clear visual feedback - LED ON means ready for commissioning, LED OFF means commissioned
- **Automatic Reconnection**: WiFi watchdog for reliable connectivity
- **Error Handling**: Graceful failure recovery with automatic restarts

## Hardware

### Required Components

- ESP32 Development Board (Tested on WeAct Studio ESP32C3, compatible with most ESP32 boards)
- SHT30 Temperature and Humidity Sensor (I2C address 0x45)
- USB Cable for programming and power
- Connecting wires (4-pin I2C connection)

### Supported Boards

The code is tested on the WeAct Studio ESP32C3 but should work on any ESP32-based board with the following:
- Built-in LED (configurable via LED_BUILTIN)
- I2C pins (configurable via pin_i2c_scl and pin_i2c_sda)
- BLE support (for automatic commissioning) or fallback to WiFi commissioning

### Sensor Compatibility

The SHT30 sensor is recommended and tested. Other I2C temperature/humidity sensors may work with minor code modifications:
- SHT31 (compatible)
- SHT35 (compatible)
- AHT10/AHT20 (would require code changes)

## Wiring

The project requires minimal wiring - just the I2C connection for the sensor:

| Component | ESP32 Pin | Description |
|-----------|-----------|-------------|
| SHT30 VCC | 3.3V | Power supply |
| SHT30 GND | GND | Ground |
| SHT30 SCL | GPIO 0 | I2C Clock (WeAct Studio ESP32C3) |
| SHT30 SDA | GPIO 1 | I2C Data (WeAct Studio ESP32C3) |
| Built-in LED | LED_BUILTIN | Status indicator (active LOW) |

**Note**: For other ESP32 boards, adjust the I2C pins in the code:
- pin_i2c_scl: Change to your SCL pin (default: 0)
- pin_i2c_sda: Change to your SDA pin (default: 1)

## Flashing

### Method 1: Pre-compiled Binary

1. Download the latest binary from the Releases page

2. Install esptool:
   ```bash
   pipx install esptool
   ```

3. Using esptool:
   ```bash
   esptool --port /dev/ttyUSB0 erase-flash
   esptool --port /dev/ttyUSB0 write-flash 0x0 MatterClimate.ino.merged.bin
   ```

   Replace /dev/ttyUSB0 with your actual serial port.

### Method 2: Using Arduino IDE

1. **Install ESP32 Core**:
   - Open Arduino IDE
   - Go to File > Preferences
   - Add https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json to Additional Boards Manager URLs
   - Go to Tools > Board > Boards Manager
   - Search for ESP32 and install esp32 by Espressif Systems (v3.3.11 or later)

2. **Install Required Libraries**:
   - Open Sketch > Include Library > Manage Libraries
   - Install the following libraries:
     - Adafruit SHT31 by Adafruit (v2.2.2)
     - Matter by Arduino (v1.2.5 or later)
     - WiFiManager by tzapu (v2.0.17) - only required when BLE commissioning is disabled

3. **Configure and Upload**:
   - Open MatterClimate.ino in Arduino IDE
   - Select your ESP32 board: Tools > Board > ESP32 Arduino > [Your Board Model]
   - Select the correct port: Tools > Port > [Your USB Port]
   - Select "Erase All Flash Before Sketch Upload: Enabled"
   - Select "Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)"
   - Click Upload (arrow icon) to compile and flash

4. **Monitor Serial Output**:
   - Open Tools > Serial Monitor
   - Set baud rate to 115200
   - Observe startup logs and sensor readings

## Startup

### First Boot and Commissioning

The commissioning method depends on your ESP32 board and the CONFIG_ENABLE_CHIPOBLE flag:

#### Method 1: BLE Commissioning (Recommended - ESP32-C3)

1. **Power the Device**: Connect the ESP32 via USB or power supply
2. **LED Behavior**:
   - LED ON (LOW): Device is NOT commissioned - ready for setup
   - LED OFF (HIGH): Device IS commissioned - operating normally
3. **Commission via Smart Home App**:
   - Open your Matter-compatible app (Alexa, Google Home, Apple Home)
   - Add a new device and scan the QR code displayed in the serial monitor
   - Or enter the manual pairing code shown in the serial output
   - The app will provide WiFi credentials via BLE
   - Wait for commissioning to complete
   - The LED will turn OFF (HIGH) once commissioned

#### Method 2: WiFi Commissioning (Fallback - no BLE)

1. **Power the Device**: Connect the ESP32 via USB or power supply
2. **Connect to Captive Portal**:
   - The device creates a WiFi access point (usually named MatterClimate or similar)
   - Connect your phone or computer to this network
3. **Configure WiFi**:
   - Open a web browser and navigate to the captive portal (usually 192.168.4.1)
   - Enter your WiFi credentials
   - Device will connect to your WiFi network
4. **Commission via Matter**:
   - Use your Matter-compatible app to add the device
   - Scan the QR code or enter the manual pairing code shown in the serial monitor
   - The device will be commissioned to your Matter hub

### Commissioning Information

After startup, the serial monitor will display:
- Manual pairing code (numeric code for manual entry)
- QR code URL (for scanning with smart home apps)

Use this information to commission the device to your Matter hub.

### LED Status Indicator

| LED State | Pin State | Meaning |
|-----------|-----------|---------|
| ON (LOW) | 0 | Device NOT commissioned - ready for setup |
| OFF (HIGH) | 1 | Device IS commissioned - operating normally |

## Configuration

### Sensor Reporting Thresholds

The device uses smart reporting to minimize network traffic:

- **Temperature**: Reports when changes exceed 0.3°C
- **Humidity**: Reports when changes exceed 2.0%

These thresholds can be adjusted in the code:
- temperature_diff: Adjust as needed (default: 0.3f)
- humidity_diff: Adjust as needed (default: 2.0f)

### Timing Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| Sensors Read Interval | 1 second | How often sensor is read |
| Matter Check Interval | 15 seconds | How often connection is verified |
| Matter Timeout | 3 minutes | Max disconnection time before reboot |
| WiFi Manager Timeout | 3 minutes | Captive portal timeout (WiFi method) |

## Notes

### Important Considerations

- **Matter Compatibility**: Ensure your smart home hub supports Matter (most modern hubs do)
- **Network Requirements**: Device must be on the same network as your Matter hub
- **BLE Commissioning**: Requires BLE support on the ESP32 (ESP32-C3 has built-in BLE)
- **Power Requirements**: ESP32 boards typically require 5V via USB or 3.3V from a regulated power supply
- **Sensor Placement**: For accurate readings, place the sensor away from heat sources and in open air

### Performance and Limitations

- **Reading Frequency**: Sensor is read every 1 second - sufficient for most environmental monitoring applications
- **Reporting Frequency**: Reports are sent only when thresholds are exceeded, reducing network traffic
- **Connection Recovery**: Automatic reboot after 60 seconds of disconnection ensures reliability
- **Memory Usage**: Matter library requires significant flash - use appropriate partition scheme

### Security Considerations

- **Matter Security**: Uses Matter's built-in security and encryption
- **WiFi Credentials**: Stored in ESP32's non-volatile storage
- **BLE Commissioning**: Secure pairing using Matter's standard commissioning process
- **WiFi Manager**: Only active during setup, not during normal operation

### Troubleshooting

| Issue | Solution |
|-------|----------|
| Sensor not initializing | Check wiring; verify I2C pins; ensure sensor is powered |
| LED stays ON continuously | Device is not commissioned - complete commissioning process |
| Commissioning fails | Verify Matter hub supports the device; check network connectivity |
| No sensor readings | Check I2C address (0x45); verify SHT30 library installed |
| Device doesn't appear in Matter app | Ensure device is on the same network; reboot device and try again |
| Frequent disconnections | Check WiFi signal strength; adjust device placement |
| Serial output shows no QR code | Ensure partition scheme has enough space for Matter library |

# MatterClimate

Matter-compatible temperature and humidity sensor based on an **ESP32-C3** and an **SHT30** sensor.

The project exposes temperature and relative humidity through standard Matter endpoints, allowing the device to be integrated into Matter-compatible smart-home platforms such as **Amazon Alexa, Google Home, and Apple Home**.

The firmware is designed around the ESP32 Arduino ecosystem and includes automatic Matter commissioning, sensor-change filtering, connection monitoring, and automatic recovery from prolonged Matter disconnections.

## Features

- 🌡️ Temperature measurement using SHT30
- 💧 Relative humidity measurement using SHT30
- 🏠 Matter integration
- 📱 Compatible with Matter commissioners such as Alexa, Google Home, and Apple Home
- 🔵 BLE-based Matter commissioning on BLE-capable configurations
- 📶 Wi-FiManager captive-portal fallback when Matter BLE commissioning is disabled
- 📉 Configurable reporting thresholds to avoid unnecessary Matter updates
- 🔄 Automatic recovery after prolonged Matter disconnection
- 💡 Built-in LED status indication
- 🕐 Non-blocking periodic execution using `millis()`
- 🧪 Serial diagnostics at 115200 baud
- 📦 Two Matter sensor endpoints:
  - Temperature Sensor
  - Humidity Sensor

---

## Hardware

The firmware was developed and tested with:

| Component | Specification |
|---|---|
| Microcontroller | WeAct Studio ESP32-C3 |
| Sensor | SHT30 |
| SHT30 I²C address | `0x45` |
| I²C SDA | GPIO `1` |
| I²C SCL | GPIO `0` |
| Status LED | `LED_BUILTIN` |
| LED logic | Active LOW |

### SHT30 wiring

For the tested WeAct Studio ESP32-C3 configuration:

```text
SHT30          ESP32-C3
-----------------------
VCC     ------ 3.3V
GND     ------ GND
SDA     ------ GPIO 1
SCL     ------ GPIO 0
```

The sensor is initialized at I²C address:

```cpp
constexpr uint8_t sht30_addr = 0x45;
```

If your SHT30 uses another address, change this value accordingly.

> **Note:** GPIO 0 and GPIO 1 are specific to the tested hardware configuration. If you use another ESP32 board, verify its recommended I²C pins before changing the firmware.

---

## Software requirements

The project was developed using:

- Arduino IDE `2.3.10`
- ESP32 Arduino Core `3.3.11`

### Required libraries

Install the following libraries before compiling:

| Library | Version referenced | Purpose |
|---|---:|---|
| Matter | ESP32 Arduino Core | Matter protocol |
| Adafruit SHT31 | `2.2.2` | SHT30 temperature/humidity sensor |
| WiFiManager | `2.0.17` | Wi-Fi captive portal fallback |

The WiFiManager library is only required when BLE Matter commissioning is disabled.

### Library repositories

- Adafruit SHT31: https://github.com/adafruit/Adafruit_SHT31
- WiFiManager: https://github.com/tzapu/WiFiManager

---

## Matter commissioning

One of the main characteristics of this project is that the Wi-Fi commissioning method is selected at compile time.

### BLE Matter commissioning

When:

```cpp
CONFIG_ENABLE_CHIPOBLE
```

is enabled, the ESP32-C3 uses the standard Matter commissioning flow.

The user does not manually enter the Wi-Fi credentials into the firmware.

Instead:

1. Power on the MatterClimate device.
2. Add a new Matter device from the smart-home application.
3. Scan the Matter QR code or enter the manual pairing code.
4. The Matter commissioner provides the Wi-Fi credentials to the device.
5. The device joins the Wi-Fi network.
6. The Matter node becomes available to the smart-home ecosystem.

This is the preferred commissioning method for the ESP32-C3.

### Wi-FiManager fallback

When BLE Matter commissioning is disabled, the firmware includes WiFiManager and creates a configuration portal.

The relevant code is compiled only when:

```cpp
#if !CONFIG_ENABLE_CHIPOBLE
```

In this mode:

1. The ESP32 starts WiFiManager.
2. The user connects to the temporary configuration network.
3. Wi-Fi credentials are entered through the captive portal.
4. The ESP32 connects to the configured network.
5. Matter initialization continues.

The configuration portal has a timeout of:

```text
180 seconds
```

and a Wi-Fi connection timeout of:

```text
30 seconds
```

If Wi-Fi cannot be configured successfully, the device restarts.

---

## Matter endpoints

The firmware exposes two Matter endpoints:

```cpp
MatterTemperatureSensor temperatureSensor;
MatterHumiditySensor humiditySensor;
```

They are initialized independently:

```cpp
temperatureSensor.begin();
humiditySensor.begin();
```

This allows the smart-home ecosystem to expose temperature and humidity as separate measurements while both values originate from the same physical SHT30 sensor.

---

## Sensor reading

The SHT30 is read every:

```text
1 second
```

using:

```cpp
sht31.readTemperature();
sht31.readHumidity();
```

The readings are stored in:

```cpp
float current_temperature;
float current_humidity;
```

The firmware checks for invalid `NaN` readings before accepting the values.

If a reading fails, the previous valid value is retained and an error is printed to the Serial Monitor.

Example:

```text
Temperature: 27.45°C
Humidity: 58.32%
```

---

## Matter update strategy

The firmware deliberately does **not** update Matter every time the SHT30 is read.

Instead, it compares the current measurement with the last value reported to Matter.

### Temperature

Temperature is reported only when the absolute difference is at least:

```text
0.3 °C
```

Configured by:

```cpp
const float temperature_diff = 0.3f;
```

### Humidity

Humidity is reported only when the absolute difference is at least:

```text
2.0 %
```

Configured by:

```cpp
const float humidity_diff = 2.0f;
```

The comparison is performed with:

```cpp
fabsf(current_temperature - reported_temperature)
```

and:

```cpp
fabsf(current_humidity - reported_humidity)
```

### Why use thresholds?

Environmental sensors naturally fluctuate slightly even when the room conditions are effectively unchanged.

Without a threshold, the device could continuously send small changes to Matter.

The threshold strategy reduces unnecessary Matter updates and network traffic while still providing meaningful environmental changes.

---

## Important behavior during startup

The initial values are:

```cpp
float reported_temperature = 0.0f;
float reported_humidity = 0.0f;
```

Therefore, after the first valid sensor reading, the difference from zero will normally exceed the configured thresholds and the initial values will be reported to Matter.

This is useful because the Matter endpoints receive an initial meaningful measurement without requiring a separate initialization flag.

---

## Matter connection monitoring

The firmware checks Matter status every:

```text
15 seconds
```

using:

```cpp
const uint32_t matter_check_interval = 15000;
```

Two Matter states are evaluated:

```cpp
Matter.isDeviceCommissioned()
Matter.isDeviceConnected()
```

The firmware distinguishes between:

- **Not commissioned** — device still needs to be added to a Matter network.
- **Commissioned and connected** — normal operation.
- **Commissioned but disconnected** — communication with the Matter network has been lost.

---

## Automatic Matter recovery

If the device is commissioned but remains disconnected, the firmware increments:

```cpp
matter_disconnect_counter
```

The timeout is:

```text
60 seconds
```

with status checks every:

```text
15 seconds
```

After the configured timeout is reached, the firmware:

1. Calls `Matter.decommission()`
2. Resets the disconnection counter
3. Waits 1 second
4. Restarts the ESP32

This intentionally removes the Matter commissioning state and starts the device again so it can be commissioned from a clean state.

### Why decommission?

This recovery strategy is intended for situations where the device becomes stuck in a commissioned-but-disconnected state and normal reconnection does not recover it.

> **Important:** Because the firmware calls `Matter.decommission()`, the device may need to be commissioned again after this recovery procedure.

---

## LED status

The built-in LED is configured as **active LOW**.

### LED states

| State | LED |
|---|---|
| Initialization | ON |
| Not commissioned | ON |
| Commissioned | OFF |

The logic is:

```cpp
LOW  = LED ON
HIGH = LED OFF
```

The LED therefore provides a simple indication that the device still needs Matter commissioning.

---

## Firmware flow

The firmware follows this general sequence:

```text
                    Power On
                       │
                       ▼
              Hardware initialization
                       │
                       ▼
                Initialize I²C
                       │
                       ▼
                Initialize SHT30
                       │
                       ▼
          Select Wi-Fi commissioning
              ┌────────┴────────┐
              │                 │
          BLE Matter        WiFiManager
          commissioning       fallback
              │                 │
              └────────┬────────┘
                       │
                       ▼
              Initialize Matter
                       │
                       ▼
          ┌─────────────────────────┐
          │       Main loop         │
          └────────────┬────────────┘
                       │
          ┌────────────┴─────────────┐
          │                          │
          ▼                          ▼
   Every 1 second              Every 15 seconds
          │                          │
          ▼                          ▼
   Read SHT30                  Check Matter
          │                          │
          ▼                          ▼
 Compare thresholds          Update LED/status
          │                          │
          ▼                          ▼
 Update Matter             Recovery if needed
```

---

## Main timing parameters

The main timing values are defined near the beginning of the source code:

| Parameter | Value | Function |
|---|---:|---|
| `sensors_read_interval` | `1000 ms` | Sensor reading interval |
| `matter_check_interval` | `15000 ms` | Matter status check |
| `matter_timeout` | `60000 ms` | Maximum Matter disconnection |
| `temperature_diff` | `0.3 °C` | Temperature reporting threshold |
| `humidity_diff` | `2.0 %` | Humidity reporting threshold |

These values can be changed according to the desired responsiveness and network traffic.

---

## Installation

### 1. Install Arduino IDE

Install Arduino IDE 2.x and configure the ESP32 board package.

### 2. Install ESP32 support

Install the ESP32 Arduino Core and select the appropriate ESP32-C3 board.

For the tested hardware, use the appropriate **WeAct Studio ESP32-C3** board configuration available in your installed ESP32 core.

### 3. Install libraries

Install:

- Matter support from the ESP32 Arduino Core
- Adafruit SHT31
- WiFiManager if using the WiFiManager commissioning fallback

### 4. Connect the SHT30

Connect:

```text
SDA → GPIO 1
SCL → GPIO 0
3.3V → 3.3V
GND → GND
```

### 5. Compile and upload

Open:

```text
MatterClimate.ino
```

Compile and upload the firmware.

### 6. Open Serial Monitor

Use:

```text
115200 baud
```

The Serial Monitor displays sensor readings and Matter commissioning information.

---

## First commissioning

If the device is not commissioned, the Serial Monitor prints information similar to:

```text
Matter Node is not commissioned yet.
Initiate the device discovery in your Matter environment.
Commission it to your Matter hub with the manual pairing code or QR code
Manual pairing code: XXXXXXXX
QR code URL: ...
```

Use the displayed pairing information to add the device to your Matter ecosystem.

Depending on the commissioning method and platform, the Matter controller may use the QR code, manual pairing code, or its normal Matter onboarding flow.

---

## Smart-home integration

Once commissioned, the device exposes:

```text
MatterClimate
├── Temperature
└── Humidity
```

The exact presentation depends on the Matter controller and smart-home platform.

The device is intended for integration with ecosystems supporting Matter, including:

- Amazon Alexa
- Google Home
- Apple Home
- Other Matter-compatible controllers

---

## Error handling

### SHT30 initialization failure

If the SHT30 cannot be initialized:

```text
Error initializing SHT30 sensor. Check your wiring!
```

is printed to Serial.

The firmware currently **continues execution** after this error.

If the sensor remains unavailable, subsequent readings will fail.

### Invalid sensor reading

If the SHT30 returns `NaN`:

```text
Error reading temperature!
```

or:

```text
Error reading humidity!
```

is printed.

The corresponding current value is not overwritten.

### Wi-FiManager connection failure

In WiFiManager mode, if Wi-Fi configuration fails:

```text
Could not connect to WiFi! Rebooting...
```

The ESP32 waits briefly and restarts.

### Matter updates are event-like

Although the sensor is physically sampled once per second, Matter is updated only when the configured threshold is reached.

This provides a useful balance between:

- sensor responsiveness;
- network traffic;
- Matter update frequency;
- small environmental fluctuations.

---

## Known considerations and limitations

- The firmware is primarily targeted at the **WeAct Studio ESP32-C3**.
- GPIO 0 and GPIO 1 are hardware-specific choices.
- The SHT30 address is configured as `0x45`.
- The built-in LED is assumed to be active LOW.
- Sensor readings are sampled every second, but Matter updates are threshold-based.
- Temperature updates require a change of at least `0.3 °C`.
- Humidity updates require a change of at least `2.0 %`.
- A prolonged Matter disconnection causes automatic decommissioning and reboot.
- Automatic decommissioning means the device may need to be commissioned again.
- If the SHT30 fails to initialize, the firmware currently continues running rather than entering a sensor-error state.
- WiFiManager is only compiled when `CONFIG_ENABLE_CHIPOBLE` is disabled.
- Matter commissioning behavior depends on the ESP32 core configuration and the Matter controller being used.

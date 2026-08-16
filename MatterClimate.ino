/**
 * @file MatterClimate.ino
 * @brief Matter-compatible Temperature and Humidity Sensor
 * 
 * This firmware implements a Matter-compatible environmental sensor using
 * an ESP32 with an SHT30 temperature/humidity sensor. It provides:
 * - Matter protocol support for smart home integration
 * - Temperature and humidity sensing with SHT30
 * - Automatic commissioning method selection (BLE or WiFi)
 * - Automatic reconnection and error recovery
 * - LED status indication (active LOW)
 * 
 * @author Igor Ferreira
 * @version 1.0
 * @date 2026
 * 
 * @hardware WeAct Studio ESP32C3 (tested)
 * @sensor Adafruit SHT30 (I2C address 0x45)
 * @toolchain Arduino IDE 2.3.10
 * @core ESP32 Arduino Core 3.3.11
 * 
 * @note Commissioning Method Selection:
 *       - ESP32-C3: Uses BLE commissioning (CONFIG_ENABLE_CHIPOBLE enabled)
 *         WiFi credentials are provided by the Matter commissioner (Alexa,
 *         Google Home, Apple Home) during commissioning via BLE
 * 
 *       - ESP32 (without BLE) or when CONFIG_ENABLE_CHIPOBLE is disabled:
 *         Uses WiFi commissioning via WiFiManager captive portal
 *         User manually configures WiFi credentials through a web interface
 * 
 *       The code automatically adapts to the available commissioning method
 *       based on the CONFIG_ENABLE_CHIPOBLE compilation flag.
 * 
 * @note LED Behavior (Active LOW - typical for ESP32 boards):
 *       - LED ON (LOW): Device is NOT commissioned - ready for setup
 *       - LED OFF (HIGH): Device IS commissioned - operating normally
 * 
 * @note The SHT30 sensor communicates via I2C on pins 0 (SCL) and 1 (SDA)
 *       as defined for the WeAct Studio ESP32C3 board. Adjust pins for
 *       other ESP32 boards if needed.
 */

#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_SHT31.h>  // https://github.com/adafruit/Adafruit_SHT31/releases/tag/v2.2.2

#include <Matter.h>

//=============================================================================
// Conditional WiFi Configuration
//=============================================================================

/**
 * @brief WiFiManager is included only when BLE commissioning is disabled
 * 
 * The commissioning method is determined at compile time:
 * 
 * 1. BLE Commissioning (CONFIG_ENABLE_CHIPOBLE = 1):
 *    - Used by ESP32-C3 and other boards with BLE support
 *    - WiFi credentials are provided by the Matter commissioner app
 *      (Alexa, Google Home, Apple Home) via BLE
 *    - WiFiManager is NOT used
 *    - This is the recommended method for most ESP32 boards
 * 
 * 2. WiFi Commissioning (CONFIG_ENABLE_CHIPOBLE = 0):
 *    - Used as fallback for boards without BLE or for debugging
 *    - ESP32 creates a captive portal WiFi network
 *    - User connects and configures WiFi via web interface
 *    - WiFiManager handles the configuration process
 * 
 * The conditional compilation ensures the code works on both BLE-capable
 * and non-BLE boards without modification.
 */
#if !CONFIG_ENABLE_CHIPOBLE
#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager/releases/tag/v2.0.17

/** @brief WiFiManager instance for captive portal (BLE commissioning disabled) */
WiFiManager wm;
#endif

//=============================================================================
// Hardware Configuration Constants
//=============================================================================

/** @brief I2C SCL pin (GPIO 0 - adjust for other ESP32 boards) */
constexpr uint8_t pin_i2c_scl = 0;

/** @brief I2C SDA pin (GPIO 1 - adjust for other ESP32 boards) */
constexpr uint8_t pin_i2c_sda = 1;

/**
 * @brief Built-in LED pin for status indication (Active LOW)
 * 
 * The LED is connected between VCC and the GPIO pin (common on ESP32 boards).
 * - LOW = LED ON
 * - HIGH = LED OFF
 */
constexpr uint8_t pin_led = LED_BUILTIN;

//=============================================================================
// Sensor Data Variables
//=============================================================================

/** @brief Current temperature reading in Celsius */
float current_temperature = 0.0f;

/** @brief Last temperature value reported to Matter */
float reported_temperature = 0.0f;

/**
 * @brief Temperature change threshold for reporting
 * 
 * Only report temperature changes greater than this value (0.3°C)
 * to avoid excessive Matter updates.
 */
const float temperature_diff = 0.3f;

/** @brief Current humidity reading in percentage */
float current_humidity = 0.0f;

/** @brief Last humidity value reported to Matter */
float reported_humidity = 0.0f;

/**
 * @brief Humidity change threshold for reporting
 * 
 * Only report humidity changes greater than this value (2.0%)
 * to avoid excessive Matter updates.
 */
const float humidity_diff = 2.0f;

//=============================================================================
// Timing Constants
//=============================================================================

/** @brief Interval between sensor readings (1 second) */
const uint32_t sensors_read_interval = 1000;

/** @brief Interval for Matter connection checks (15 seconds) */
const uint32_t matter_check_interval = 15000;

/** @brief Maximum time without Matter connection before reboot (60 seconds) */
const uint32_t matter_timeout = 60000;

/**
 * @brief Counter for consecutive Matter disconnections
 * 
 * Incremented when device is commissioned but not connected.
 * Reset to 0 when connection is restored.
 */
uint8_t matter_disconnect_counter = 0;

/** @brief Timestamp of last sensor reading */
uint32_t last_sensors_read = 0;

/** @brief Timestamp of last Matter connection check */
uint32_t last_matter_check = 0;

//=============================================================================
// Sensor Initialization
//=============================================================================

/** @brief I2C address for SHT30 sensor (0x45) */
constexpr uint8_t sht30_addr = 0x45;

/** @brief SHT30 sensor instance */
Adafruit_SHT31 sht31 = Adafruit_SHT31();

//=============================================================================
// Matter Endpoints
//=============================================================================

/**
 * @brief Matter Temperature Sensor endpoint
 * 
 * Represents the temperature sensor as a Matter device.
 * Provides standard Matter temperature measurement cluster.
 */
MatterTemperatureSensor temperatureSensor;

/**
 * @brief Matter Humidity Sensor endpoint
 * 
 * Represents the humidity sensor as a Matter device.
 * Provides standard Matter humidity measurement cluster.
 */
MatterHumiditySensor humiditySensor;

//=============================================================================
// Sensor Reading Functions
//=============================================================================

/**
 * @brief Read data from SHT30 sensor
 * 
 * Reads temperature and humidity from the SHT30 sensor.
 * Updates global current_temperature and current_humidity variables.
 * Validates readings (checks for NaN) and logs errors if readings fail.
 * 
 * @note This function is called every 1 second (sensors_read_interval)
 * 
 * @warning SHT30 may return NaN if sensor is disconnected or I2C fails.
 */
void readSensors() {

  // Read temperature (Celsius)
  float t = sht31.readTemperature();

  if (!isnan(t)) {
    current_temperature = t;
    Serial.print("Temperature: ");
    Serial.print(current_temperature);
    Serial.println("°C");
  } else {
    Serial.println("Error reading temperature!");
  }

  // Read humidity (percentage)
  float h = sht31.readHumidity();

  if (!isnan(h)) {
    current_humidity = h;
    Serial.print("Humidity: ");
    Serial.print(current_humidity);
    Serial.println("%");
  } else {
    Serial.println("Error reading humidity!");
  }
}

//=============================================================================
// Matter Update Functions
//=============================================================================

/**
 * @brief Update Matter endpoints with new sensor data
 * 
 * Checks if the device is commissioned and connected to a Matter hub.
 * Compares current readings with last reported values and updates
 * Matter endpoints only when changes exceed configured thresholds.
 * 
 * @note Temperature threshold: 0.3°C
 * @note Humidity threshold: 2.0%
 * 
 * This prevents unnecessary network traffic and Matter updates
 * when sensor values fluctuate minimally.
 */
void updateMatter() {

  // Only update if device is commissioned
  if (Matter.isDeviceCommissioned()) {

    // Update temperature if change exceeds threshold
    if (fabsf(current_temperature - reported_temperature) >= temperature_diff) {
      temperatureSensor.setTemperature(current_temperature);
      reported_temperature = current_temperature;
    }

    // Update humidity if change exceeds threshold
    if (fabsf(current_humidity - reported_humidity) >= humidity_diff) {
      humiditySensor.setHumidity(current_humidity);
      reported_humidity = current_humidity;
    }
  }
}

//=============================================================================
// Matter Connection Management
//=============================================================================

/**
 * @brief Check Matter connection status and handle disconnections
 * 
 * Monitors the Matter connection state and implements a recovery strategy:
 * 1. Updates LED status based on commissioning state (Active LOW)
 * 2. Tracks disconnection duration
 * 3. Automatically decommissions and reboots after 60 seconds of disconnection
 * 
 * LED Behavior (Active LOW - LED ON when pin is LOW):
 * - LED ON (LOW):  Device is NOT commissioned - ready for commissioning
 * - LED OFF (HIGH): Device IS commissioned - operating normally
 * 
 * This provides clear visual feedback:
 * - Blinking/ON LED indicates device needs to be set up
 * - OFF LED indicates device is already in your smart home
 * 
 * Recovery Strategy:
 * - Counts consecutive check cycles where device is commissioned but disconnected
 * - If disconnection persists for matter_timeout (60s), decommission and reboot
 * - This forces the device to be re-added to the Matter network
 * 
 * @note Check interval is defined by matter_check_interval (15 seconds)
 * @note matter_disconnect_counter increments each check cycle when disconnected
 * @note Counter resets to 0 when connection is restored
 */
void checkMatter() {
  
  // Update LED based on commission status (Active LOW)
  // LED ON (LOW)  = Not commissioned - ready for setup
  // LED OFF (HIGH) = Commissioned - operating normally
  if (!Matter.isDeviceCommissioned()) {
    digitalWrite(pin_led, LOW);   // LED ON - waiting for commissioning
  } else {
    digitalWrite(pin_led, HIGH);  // LED OFF - commissioned and running
  }

  // Detect disconnection scenario:
  // Device is commissioned (known to hub) but not connected (communication lost)
  if (Matter.isDeviceCommissioned() != Matter.isDeviceConnected()) {
    matter_disconnect_counter++;
    Serial.println("Device disconnected!");
    
    // If disconnected for longer than matter_timeout
    if (matter_disconnect_counter >= (matter_timeout / matter_check_interval)) {
      Serial.println("Device disconnected for 1 minute - Decommissioning and rebooting...");
      
      // Decommission the device (removes it from the Matter network)
      Matter.decommission();
      matter_disconnect_counter = 0;
      
      // Allow time for decommissioning to complete
      delay(1000);
      
      // Reboot to start fresh
      ESP.restart();
    }
  } else {
    // Connection restored or still connected - reset counter
    matter_disconnect_counter = 0;
  }
}

//=============================================================================
// Setup Function
//=============================================================================

/**
 * @brief Arduino setup function
 * 
 * Initializes the system in the following order:
 * 1. Hardware initialization (pins, LED, serial, I2C)
 * 2. SHT30 sensor initialization
 * 3. WiFi configuration (method depends on CONFIG_ENABLE_CHIPOBLE)
 * 4. Matter endpoint initialization (temperature and humidity)
 * 5. Matter protocol initialization
 * 6. Display commissioning information (pairing code, QR code)
 * 
 * WiFi Configuration Methods:
 * 
 * A) BLE Commissioning (CONFIG_ENABLE_CHIPOBLE = 1):
 *    - Used by ESP32-C3 and BLE-capable boards
 *    - WiFi credentials are NOT defined in code
 *    - The Matter commissioner (Alexa, Google Home, Apple Home) app
 *      provides WiFi credentials during commissioning via BLE
 *    - No user interaction required for WiFi setup
 *    - This is the standard Matter commissioning flow
 * 
 * B) WiFi Commissioning (CONFIG_ENABLE_CHIPOBLE = 0):
 *    - Used by boards without BLE or for debugging
 *    - ESP32 creates a captive portal WiFi network
 *    - User connects to the ESP32's AP and configures WiFi via web
 *    - WiFiManager handles the configuration process
 *    - Useful for testing or when BLE is not available
 * 
 * The conditional compilation allows the same code to work on
 * different ESP32 boards without modification.
 * 
 * LED Behavior (Active LOW - LED ON when pin is LOW):
 * - LED ON (LOW) during initialization
 * - After setup, LED state is managed by checkMatter():
 *   * LED ON (LOW)  = Not commissioned - ready for setup
 *   * LED OFF (HIGH) = Commissioned - operating normally
 * 
 * @note I2C pins are configured for WeAct Studio ESP32C3 (pins 0 and 1)
 *       Adjust for other ESP32 boards if needed
 * 
 * @warning If SHT30 initialization fails, an error message is printed but setup continues
 * @warning If WiFi commissioning fails (WiFiManager mode), the device reboots
 */
void setup() {

  //-----------------------------------------------------------------------
  // 1. Hardware Initialization
  //-----------------------------------------------------------------------
  
  pinMode(pin_led, OUTPUT);

  // LED ON during initialization (Active LOW)
  digitalWrite(pin_led, LOW);

  Serial.begin(115200);

  // Allow time for serial to initialize
  delay(1000);

  //-----------------------------------------------------------------------
  // 2. SHT30 Sensor Initialization
  //-----------------------------------------------------------------------
  
  Wire.begin(pin_i2c_sda, pin_i2c_scl);

  if (!sht31.begin(sht30_addr)) {
    Serial.println("Error initializing SHT30 sensor. Check your wiring!");
  }

  //-----------------------------------------------------------------------
  // 3. WiFi Configuration (Conditional based on commissioning method)
  //-----------------------------------------------------------------------
  
  /**
   * WiFi configuration method is selected at compile time:
   * 
   * - If CONFIG_ENABLE_CHIPOBLE is defined (default for ESP32-C3):
   *   BLE commissioning is used. WiFi credentials are provided by the
   *   Matter commissioner app during commissioning. No manual setup needed.
   * 
   * - If CONFIG_ENABLE_CHIPOBLE is NOT defined (fallback):
   *   WiFiManager captive portal is used. User manually configures WiFi
   *   credentials through a web interface. Useful for boards without BLE
   *   or for debugging purposes.
   * 
   * This design ensures compatibility with both BLE-capable and non-BLE
   * ESP32 boards without code changes.
   */
#if !CONFIG_ENABLE_CHIPOBLE
  // WiFi Commissioning Mode (Captive Portal)
  std::vector<const char *> menu = { "wifi", "restart", "exit" };
  wm.setMenu(menu);
  wm.setConfigPortalTimeout(180);  // 3 minutes timeout
  wm.setConnectTimeout(30);        // 30 seconds connection timeout

  if (!wm.autoConnect()) {
    Serial.println("Could not connect to WiFi! Rebooting...");
    delay(1000);
    ESP.restart();
  }
#endif

  //-----------------------------------------------------------------------
  // 4. Matter Endpoint Initialization
  //-----------------------------------------------------------------------
  
  if (!temperatureSensor.begin()) {
    Serial.println("Error initializing temperature endpoint!");
  }

  if (!humiditySensor.begin()) {
    Serial.println("Error initializing humidity endpoint!");
  }

  //-----------------------------------------------------------------------
  // 5. Matter Protocol Initialization
  //-----------------------------------------------------------------------
  
  Matter.begin();

  //-----------------------------------------------------------------------
  // 6. Commissioning Information
  //-----------------------------------------------------------------------
  
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.print("Manual pairing code: ");
    Serial.println(Matter.getManualPairingCode().c_str());
    Serial.print("QR code URL: ");
    Serial.println(Matter.getOnboardingQRCodeUrl().c_str());
  }
}

//=============================================================================
// Loop Function
//=============================================================================

/**
 * @brief Arduino main loop
 * 
 * Manages periodic tasks using non-blocking timers:
 * 1. Sensor Reading (every 1 second):
 *    - Reads temperature and humidity from SHT30
 *    - Updates Matter endpoints if changes exceed thresholds
 * 
 * 2. Matter Connection Check (every 15 seconds):
 *    - Monitors commission and connection status
 *    - Handles disconnection recovery
 *    - Updates LED status (Active LOW)
 * 
 * @note Uses millis() for timing instead of delay() to keep loop non-blocking
 * @note Both tasks run independently at their configured intervals
 */
void loop() {

  uint32_t current_time = millis();

  // Task 1: Read sensors every second
  if (current_time - last_sensors_read >= sensors_read_interval) {
    last_sensors_read = current_time;
    readSensors();
    updateMatter();
  }

  // Task 2: Check Matter connection every 15 seconds
  if (current_time - last_matter_check >= matter_check_interval) {
    last_matter_check = current_time;
    checkMatter();
  }
}
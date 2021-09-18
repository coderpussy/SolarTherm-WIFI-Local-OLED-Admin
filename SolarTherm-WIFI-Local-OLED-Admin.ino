/*----------------------------------------------------------------------------------------------------
  Project Name : SolarTherm WIFI Local Admin aka. Solar Powered WiFi Weather Station V2
  Features: temperature, humidity, pressure, altitude and battery status
  Authors: Keith Hungerford, Debasish Dutta
  Website : www.opengreenenergy.com
  
  Main microcontroller (ESP8266) and BME280 both sleep between measurements
  CODE: https://github.com/coderpussy/SolarTherm-WIFI-Local-Admin
  INSTRUCTIONS & HARDWARE: https://www.instructables.com/id/Solar-Powered-WiFi-Weather-Station-V20/

  CREDITS:
  
  Inspiration, code fragments and document parts are taken from:
  https://github.com/3KUdelta/Solar_WiFi_Weather_Station
  https://github.com/balassy/solar-wifi-weather-station
  https://github.com/esp8266/Arduino/blob/master/doc/ota_updates/readme.rst
  
  Needed libraries:
  <Adafruit_Sensor.h>    --> Adafruit unified sensor
  <Adafruit_BME280.h>    --> Adafrout BME280 sensor
  <ESP8266WiFi.h>        --> To connect to the WiFi network.
  <WiFiManager.h>        --> To manage network configuration and connection.
  <ESP8266HTTPClient.h>  --> To send HTTP requests.
  <ESP8266httpUpdate.h>  --> To get new binary update

  <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
  
  Needed OLED Display SSD1306 library from https://github.com/ThingPulse/esp8266-oled-ssd1306
  
  <SSD1306Wire.h> // legacy include: `#include "SSD1306.h"`
  <OLEDDisplayUi.h>
  <SansSerif_bold_24.h> // generated weather text font created by http://oleddisplay.squix.ch/
  <images.h> // generated weather frame icons (icons exported as xbm file)

  CREDITS for Adafruit libraries:
  
  This is a library for the BME280 humidity, temperature & pressure sensor
  Designed specifically to work with the Adafruit BME280 Breakout
  ----> http://www.adafruit.com/products/2650
  These sensors use I2C or SPI to communicate, 2 or 4 pins are required
  to interface. The device's I2C address is either 0x76 or 0x77.
  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!
  Written by Limor Fried & Kevin Townsend for Adafruit Industries.
  BSD license, all text above must be included in any redistribution

////  Features :  //////////////////////////////////////////////////////////////////////////////////////////////////////
                                                                                                                         
// 1. Connect to Wi-Fi, and upload the data to either local network storage or webspace
// 2. Monitoring Weather parameters like Temperature, Pressure, Humidity.
// 3. Over The Air device update possible with ESP8266 http Update functionality
// 4. Extra Ports to add more Weather Sensors like UV Index, Light and Rain Guage etc.
// 5. Remote Battery Status Monitoring
// 6. Using Deep Sleep mode to reduce the energy consumed

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************
 * IMPORTANT:                                      *
 * All settings located in config.h !              *
 **************************************************/

// Platform libraries.
#include <Arduino.h>           // To add IntelliSense for platform constants.
#include <ESP8266WiFi.h>       // To connect to the WiFi network.

// Third-party libraries.
#include <WiFiManager.h>  // To manage network configuration and connection.

// My classes.
#include "bme280.h"              // To read the temperature, pressure and humidity sensor.

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
// Include the UI lib
#include "OLEDDisplayUi.h"
// Include custom oled images
#include "SansSerif_bold_24.h"
#include "images.h"

#include "last-update.h" // To get last device update from local network database.
#include "localstorage-client.h" // To send measured data to a local network database.
#include "magicmirror-client.h"  // To manage the communication with the MagicMirror.
#include "ota-updater.h"         // To manage over-the-air updates trough web server script.

#include "config.h"  // To store configuration and secrets (create for each device separately e.g. config-living.h).

WiFiManager wifiManager;
BME280 bme;

LastUpdateClient lastUpdate;

// Initialize the OLED display using Wire library
SSD1306Wire display(0x3c, D2, D1);
OLEDDisplayUi ui ( &display );

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, APP_CONFIG);
  //display->drawHorizontalLine(0, 12, 128);
}

void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  BME280::Measurement m = bme.getMeasuredData();
  display->drawXbm(x, y + 6, 60, 60, thermometer_bits);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(SansSerif_bold_24);
  display->drawString(116 + x, 21 + y, String(int(round(m.temperature))) + "°C");
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  BME280::Measurement m = bme.getMeasuredData();
  display->drawXbm(x, y + 6, 60, 60, humidity_bits);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(SansSerif_bold_24);
  display->drawString(116 + x, 21 + y, String(int(round(m.humidity))) + "%");
}

void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  float voltage = readBatteryVoltage();
  //float voltage = (voltage*10) / 10.;
  String volt = String(voltage);
  display->drawXbm(x, y + 6, 60, 60, battery_bits);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(SansSerif_bold_24);
  display->drawString(116 + x, 21 + y, volt.substring(0,3) + "V");
}

// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = { drawFrame1, drawFrame2, drawFrame3 };

// how many frames are there?
int frameCount = 3;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;

LocalStorageClient localStorage;
MagicMirrorClient magicMirror;
OTAUpdater updater;

void setup() {
  initSerial();
  Serial.printf("Application version: %s\n", APP_VERSION);

  initTemperatureSensor();
    
  initNetwork();
  initLastUpdateClient();
 
  int lastUpdateTime = lastUpdate.getLastUpdate().toInt();
  Serial.printf("Last Update Time (secs): %d\n", lastUpdateTime);

  initLocalStorageClient();
  initMagicMirrorClient();
  initUpdater();
  Serial.println("Setup completed.");
  if (lastUpdateTime == 0) {
    Serial.println("Transfer first Measurement");
    measureAndUpdateTargets();
  } else if (lastUpdateTime < (UPDATE_INTERVAL_SECONDS - 180)) {
    showMeasurementOnDisplay();
    
    int timeleft = UPDATE_INTERVAL_SECONDS - lastUpdateTime;

    if (timeleft < 0) {
      measureAndUpdateTargets();
      timeleft = UPDATE_INTERVAL_SECONDS;
    }
    
    Serial.printf("Time left (secs): %d\n", timeleft);
    Serial.printf("Going to Deep-Sleep for %d seconds...\n", timeleft);
    
    ESP.deepSleep(timeleft * 1e6); // Microseconds.
  } else {
    Serial.println("Transfer new Measurement");
    measureAndUpdateTargets();
  }

  goToDeepSleep();
}

void loop() {
  // No code here, because all logic is running in the setup phase and then the device goes to deep sleep.
  // After waking up the setup phase is executed again.
}

void initSerial() {
  Serial.begin(115200); // 115200 is the speed used to print boot messages.
  Serial.setDebugOutput(false); // Show debug output
  Serial.println();
  Serial.println("Initializing serial connection DONE.");
}

void initNetwork() {
  Serial.printf("Initializing connection to the network with MAC address %s using WiFiManager (SSID: %s)...\n", WiFi.macAddress().c_str(), WIFI_AP_SSID);
  wifiManager.setAPCallback([&](WiFiManager *mgr) {});
  wifiManager.setSaveConfigCallback([&]() {});

  // Configuration portal timeout and automatic restart is required because sometimes the device does not find the
  // network credentials after booting up, but after a restart it can connect to the network without any issue.
  // This ensures that the device retries connecting to the network without waiting for user interaction.
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_PORTAL_TIMEOUT_SECONDS);

  if (!wifiManager.autoConnect(WIFI_AP_SSID, WIFI_AP_PASSWORD)) {
    Serial.println("Failed to connect to the network and the WiFi configuration portal hit inactivity timeout. Restarting the device in 3 seconds and trying again...");
    delay(3000);

    // The restart() function triggers a more clean reboot than reset(), so this one is the preferred.
    // Read more: https://www.pieterverhees.nl/sparklesagarbage/esp8266/130-difference-between-esp-reset-and-esp-restart
    ESP.restart();
  }

  Serial.printf("DONE. IP address: %s, MAC address: %s\n", WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());
}

void initLastUpdateClient() {
  Serial.printf("Initializing LastUpdate client to host %s...", LOCAL_STORAGE_HOST);
  lastUpdate.setHostUrl(LOCAL_STORAGE_HOST);
  lastUpdate.setDeviceName(LOCAL_STORAGE_HOSTNAME);
  Serial.println("DONE.");
}

void initLocalStorageClient() {
  Serial.printf("Initializing LocalStorage client to host %s...", LOCAL_STORAGE_HOST);
  localStorage.setHostUrl(LOCAL_STORAGE_HOST);
  localStorage.setApiKey(LOCAL_STORAGE_API_KEY);
  localStorage.setDeviceName(LOCAL_STORAGE_HOSTNAME);
  Serial.println("DONE.");
}

void initMagicMirrorClient() {
  Serial.printf("Initializing MagicMirror client to host %s...", MAGIC_MIRROR_HOST);
  magicMirror.setHostUrl(MAGIC_MIRROR_HOST);
  magicMirror.setSensorId(MAGIC_MIRROR_SENSORID);
  Serial.println("DONE.");
}

void initTemperatureSensor() {
  Serial.print("Initializing the temperature sensor...");
  bme.init();
  Serial.println("DONE.");
}

void initUpdater() {
  Serial.print("Initializing Over-The-Air Updater...");
  updater.initialize(OTA_UPDATE_HOST, OTA_UPDATE_PORT, OTA_UPDATE_SCRIPT_PATH, OTA_UPDATE_VERSION);
  Serial.println("DONE.");
}

void StartNotification() {
  // Send start notification
  String resetReason = ESP.getResetReason();
  Serial.println("Reset reason: " + resetReason);
}

void measureAndUpdateTargets() {
  Serial.println("Measuring...");
  
  Serial.print("Reading sensor data...");
  BME280::Measurement m = bme.getMeasuredData();
  Serial.println(" DONE.");

  Serial.printf("Temperature: %.1f °C\n", m.temperature);
  Serial.printf("Humidity: %.1f %%\n", m.humidity);
  Serial.printf("Pressure: %.1f hPa\n", m.pressure);
  Serial.printf("Altitude: %.1f m\n", m.altitude);

  Serial.print("Reading battery voltage... ");
  float voltage = readBatteryVoltage();
  Serial.print(voltage);
  Serial.println("V DONE.");

  Serial.println("Sending data to LocalStorage...");
  localStorage.sendTemperature(m.temperature, m.humidity, m.pressure, m.altitude, voltage);
  Serial.println("DONE.");
  
  Serial.println("Sending data to MagicMirror...");
  magicMirror.sendTemperature(m.temperature, m.humidity, voltage);
  Serial.println("DONE.");

  Serial.println("Measuring: DONE.");
}

void showMeasurementOnDisplay() {
  // The ESP is capable of rendering 60fps in 80Mhz mode
  // but that won't give you much time for anything else
  // run it in 160Mhz mode or just set it to 30 fps
  ui.setTargetFPS(60);

  // Customize the active and inactive symbol
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  ui.setOverlays(overlays, overlaysCount);

  ui.setTimePerFrame(3000);
  ui.setTimePerTransition(300);
  
  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();
  display.setContrast(255);

  long myTimer = 0;
  long myTimeout = 12600;

  Serial.println("Display: ON");
  
  while (1) {
    ui.update();
    if (millis() > myTimeout + myTimer){
      break;
    }
  }

  display.clear();
  display.end();
  display.displayOff();

  Serial.println("Display: OFF");
}

float readBatteryVoltage() {
  // Voltage divider R1 = 220k+100k+220k =540k and R2=100k
  float calib_factor = 5.28; // change this value to calibrate the battery voltage
  unsigned long raw = analogRead(A0);
  float volt = raw * calib_factor/1024; 
  return volt;
}

void goToDeepSleep() {
  delay(50);
  
  Serial.printf("Going to Deep-Sleep for %.0f seconds...\n", UPDATE_INTERVAL_SECONDS);
  //pinMode(D0, WAKEUP_PULLUP);
  ESP.deepSleep(UPDATE_INTERVAL_SECONDS * 1e6); // Microseconds.
}

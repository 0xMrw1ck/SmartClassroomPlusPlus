// Keep every .h file beside this sketch. LocalProvisioning.h stays private.
// Install ArduinoJson 7.4.2, Adafruit SSD1306 and Adafruit GFX.
// Board ESP32-S3-USB-OTG; Minimal SPIFFS (1.9MB APP with OTA).
#include "ClassroomFirmware.h"
void setup() {
  firmwareSetup();
}
void loop() {
  firmwareLoop();
}

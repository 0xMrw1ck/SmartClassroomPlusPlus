// Keep ClassroomFirmware.h and DashboardAssets.h in this same folder.
// Install Adafruit SSD1306 + Adafruit GFX (and their dependencies).
// Select ESP32S3 Dev Module and your previously working USB/flash settings.
#include "ClassroomFirmware.h"
void setup() {
  firmwareSetup();
}
void loop() {
  firmwareLoop();
}

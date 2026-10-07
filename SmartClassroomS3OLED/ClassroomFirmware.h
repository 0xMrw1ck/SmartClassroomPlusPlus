#pragma once
#include <Arduino.h>
#include <cstring>
#include <cstdlib>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AppSettings.h"
#include "GithubOTA.h"
#include "CloudBridge.h"
#include "DashboardAssets.h"
#include "ButtonInput.h"
#include "ButtonInputChecks.h"

// ESP32-S3 DevKit: radar wiring preserved from your sketch.
#define HOME_WIFI_SSID settings.ssid
#define HOME_WIFI_PASSWORD settings.wifiPassword
#define AP_WIFI_SSID settings.apName
#define AP_WIFI_PASSWORD settings.apPassword
#define ROOM_NAME settings.room
constexpr bool REQUIRE_WEB_LOGIN = true;  // Set false to remove local webpage sign-in.
#define OLED_SDA settings.pins[4]
#define OLED_SCL settings.pins[5]
#define BUTTON_PIN settings.pins[8]  // Normally open momentary button: GPIO3 to GND.
extern bool otaBootPending;
bool standby = false;
ButtonInput button;
bool oledReinitializeRequested = false;
uint8_t oledView = 0;  // Live status, electrical telemetry, network.
constexpr int OLED_WIDTH = 128, OLED_HEIGHT = 64;
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1, 100000UL, 100000UL);
bool oledReady = false;
bool oledBusStarted = false;
uint8_t oledFailures = 0, oledI2CError = 0;
uint8_t oledAddress = 0;
uint32_t lastOLED = 0, lastOLEDProbe = 0;
uint32_t oledFrames = 0;
#define WEB_USER settings.webUser
#define WEB_PASSWORD settings.webPassword
#define RADAR_RX settings.pins[0]
#define RADAR_TX settings.pins[1]
// PZEM TX -> ESP32 RX GPIO4; PZEM RX <- ESP32 TX GPIO5 (through level translation).
#define METER_RX settings.pins[2]
#define METER_TX settings.pins[3]
#define RELAY_PINS (settings.pins + 6)
#define RELAY_ACTIVE_LOW settings.activeLow
#define VACANCY_MS settings.vacancy  // lights 30 s; fan 15 s after last detection
#define RADAR_TIMEOUT_MS settings.radarTimeout
// Rectangular classroom zone, millimetres. Adjust after mounting/testing.
#define X_MIN settings.xMin
#define X_MAX settings.xMax
#define Y_MIN settings.yMin
#define Y_MAX settings.yMax
#define TIMEZONE_SECONDS (settings.timezoneMinutes * 60)  // Philippines UTC+8
// PZEM-004T V3.0 only: default general address, ONE meter on this UART.
constexpr uint8_t METER_ADDRESS = 0xF8;
HardwareSerial RadarSerial(2), meter(1);
WebServer server(80);
Preferences prefs;
struct RadarTarget {
  bool active = false;
  int16_t x = 0, y = 0, speed = 0;
  uint16_t resolution = 0, distance = 0;
  bool inZone = false;
};
RadarTarget targets[3];
// 0 = AUTO, 1 = forced ON, 2 = forced OFF; manual modes last until AUTO/reboot.
uint8_t modes[2] = { 0, 0 };
bool relays[2] = { false, false };
const char *names[2] = { "Lights", "Fan" };
bool radarSeen = false, radarWasOnline = false;
uint32_t lastFrame = 0, lastPresence = 0;
bool presenceSeen = false;
int previousCount = -1;
const uint8_t FRAME_HEADER[4] = { 0xAA, 0xFF, 3, 0 };
const uint8_t FRAME_TAIL[2] = { 0x55, 0xCC };
const size_t FRAME_LENGTH = 30;
uint8_t frameBuffer[FRAME_LENGTH];
size_t frameIndex = 0;
float voltage = NAN, current = NAN, power = NAN, frequency = NAN, pf = NAN;
uint32_t meterWh = 0, lastMeterOK = 0, lastPoll = 0, requestAt = 0;
bool meterSeen = false, meterPending = false, meterWasOnline = false;
uint8_t meterBuffer[32];
size_t meterLength = 0;
bool storageOK = false, dirty = false;
uint32_t lastSave = 0, lastSummary = 0, lastReconnect = 0;
bool wifiWasConnected = false;
struct Day {
  uint32_t date;
  uint64_t wh;
};
struct Ledger {
  uint32_t magic;
  bool baseline;
  uint32_t previousWh;
  int64_t previousTime;
  uint64_t unallocatedWh;
  Day days[31];
};
Ledger ledger{};
constexpr uint32_t LEDGER_MAGIC = 0x53434C32;
String events[40];
unsigned eventNext = 0, eventCount = 0;

int64_t clockNow() {
  time_t t = time(nullptr);
  return t >= 1704067200 ? (int64_t)t : 0;
}
uint32_t dateAt(int64_t epoch) {
  if (!epoch) return 0;
  time_t local = (time_t)(epoch + TIMEZONE_SECONDS);
  struct tm t;
  gmtime_r(&local, &t);
  return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}
String dateText(uint32_t d) {
  char b[16];
  snprintf(b, sizeof(b), "%04u-%02u-%02u", d / 10000, (d / 100) % 100, d % 100);
  return String(b);
}
String timestamp() {
  int64_t epoch = clockNow();
  if (!epoch) return String("uptime ") + String(millis() / 1000) + "s";
  time_t local = epoch + TIMEZONE_SECONDS;
  struct tm t;
  gmtime_r(&local, &t);
  char b[24];
  strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &t);
  return String(b);
}
void logEvent(String message) {
  String line = timestamp() + " | " + message;
  Serial.println(line);
  events[eventNext] = line;
  eventNext = (eventNext + 1) % 40;
  if (eventCount < 40) eventCount++;
}
bool radarOnline() {
  return radarSeen && (uint32_t)(millis() - lastFrame) < RADAR_TIMEOUT_MS;
}
bool meterOnline() {
  return meterSeen && (uint32_t)(millis() - lastMeterOK) < max(8000UL, settings.meterPoll * 3UL);
}
int targetCount() {
  if (!radarOnline()) return 0;
  int n = 0;
  for (auto &t : targets)
    if (t.inZone) n++;
  return n;
}
void setRelay(int ch, bool on, const char *reason) {
  // Reassert the commanded GPIO even when the cached state already matches.
  digitalWrite(RELAY_PINS[ch], RELAY_ACTIVE_LOW ? !on : on);
  if (relays[ch] == on) return;
  relays[ch] = on;
  Serial.printf("Relay %s GPIO%d -> %s (active-%s)\n", names[ch], RELAY_PINS[ch],
    digitalRead(RELAY_PINS[ch]) == HIGH ? "HIGH" : "LOW", RELAY_ACTIVE_LOW ? "low" : "high");
  logEvent(String(names[ch]) + " -> " + (on ? "ON" : "OFF") + " (" + reason + ")");
}
void controlRelays() {
  if (standby || otaBootPending) {
    for (int ch = 0; ch < 2; ++ch) setRelay(ch, false, "standby");
    return;
  }
  uint32_t now = millis();
  bool online = radarOnline();
  int count = targetCount();
  if (online != radarWasOnline) {
    radarWasOnline = online;
    logEvent(online ? "Radar online" : "Radar unavailable: AUTO holds existing outputs");
  }
  if (online && count != previousCount) {
    previousCount = count;
    logEvent("Targets in classroom zone: " + String(count));
  }
  if (count > 0) {
    lastPresence = now;
    presenceSeen = true;
  }
  for (int ch = 0; ch < 2; ch++) {
    if (modes[ch] == 1) setRelay(ch, true, "manual ON");
    else if (modes[ch] == 2) setRelay(ch, false, "manual OFF");
    else if (online) {
      if (count > 0) setRelay(ch, true, "radar presence");
      else if (!presenceSeen || (uint32_t)(now - lastPresence) >= VACANCY_MS[ch]) setRelay(ch, false, "vacancy timeout");
    }
  }
}
int16_t decodeSignedValue(
  uint8_t lowByte,
  uint8_t highByte) {
  uint16_t value =
    (uint16_t)lowByte | ((uint16_t)highByte << 8);

  if ((value & 0x8000) != 0) {
    return (int16_t)(value & 0x7FFF);
  }

  return -(int16_t)(value & 0x7FFF);
}

uint16_t decodeUnsignedValue(
  uint8_t lowByte,
  uint8_t highByte) {
  return (uint16_t)lowByte | ((uint16_t)highByte << 8);
}

uint16_t calculateDistance(
  int16_t x,
  int16_t y) {
  double xSquared = (double)x * x;
  double ySquared = (double)y * y;

  return (uint16_t)sqrt(
    (float)(xSquared + ySquared));
}

int getActiveTargetCount() {
  int count = 0;

  for (int i = 0; i < 3; i++) {
    if (targets[i].active) {
      count++;
    }
  }

  return count;
}

void initializeTargets() {
  for (int i = 0; i < 3; i++) {
    targets[i].active = false;
    targets[i].x = 0;
    targets[i].y = 0;
    targets[i].speed = 0;
    targets[i].resolution = 0;
    targets[i].distance = 0;
  }
}

// =====================================================
// Serial Monitor
// =====================================================

void printRadarData() {
  Serial.println();
  Serial.println("========== LD2450 RADAR ==========");

  Serial.print("Active targets: ");
  Serial.println(getActiveTargetCount());

  for (int i = 0; i < 3; i++) {
    Serial.print("Target ");
    Serial.print(i + 1);
    Serial.print(": ");

    if (!radarOnline() || !targets[i].active) {
      Serial.println("Not detected");
      continue;
    }

    Serial.print("X=");
    Serial.print(targets[i].x);
    Serial.print(" mm, Y=");
    Serial.print(targets[i].y);
    Serial.print(" mm, Distance=");
    Serial.print(targets[i].distance);
    Serial.print(" mm, Speed=");
    Serial.print(targets[i].speed);
    Serial.print(" cm/s, Resolution=");
    Serial.print(targets[i].resolution);
    Serial.println(" mm");
  }

  Serial.println("==================================");
}

// =====================================================
// LD2450 parser
// =====================================================

bool hasValidHeader() {
  for (int i = 0; i < 4; i++) {
    if (frameBuffer[i] != FRAME_HEADER[i]) {
      return false;
    }
  }

  return true;
}

bool hasValidTail() {
  return frameBuffer[28] == FRAME_TAIL[0] && frameBuffer[29] == FRAME_TAIL[1];
}

void processRadarFrame() {
  if (!hasValidHeader() || !hasValidTail()) {
    return;
  }

  for (int targetIndex = 0; targetIndex < 3; targetIndex++) {
    int offset = 4 + targetIndex * 8;

    int16_t x = decodeSignedValue(
      frameBuffer[offset],
      frameBuffer[offset + 1]);

    int16_t y = decodeSignedValue(
      frameBuffer[offset + 2],
      frameBuffer[offset + 3]);

    int16_t speed = decodeSignedValue(
      frameBuffer[offset + 4],
      frameBuffer[offset + 5]);

    uint16_t resolution = decodeUnsignedValue(
      frameBuffer[offset + 6],
      frameBuffer[offset + 7]);

    bool active =
      x != 0 || y != 0 || speed != 0 || resolution != 0;

    targets[targetIndex].active = active;
    targets[targetIndex].x = x;
    targets[targetIndex].y = y;
    targets[targetIndex].speed = speed;
    targets[targetIndex].resolution = resolution;
    targets[targetIndex].distance = calculateDistance(x, y);
    targets[targetIndex].inZone = active && x >= X_MIN && x <= X_MAX && y >= Y_MIN && y <= Y_MAX;
  }

  lastFrame = millis();
  radarSeen = true;
}

void readRadarData() {
  unsigned budget = 2048;
  while (RadarSerial.available() && budget--) {
    uint8_t byte = RadarSerial.read();
    if (frameIndex == FRAME_LENGTH) {
      memmove(frameBuffer, frameBuffer + 1, FRAME_LENGTH - 1);
      frameIndex = FRAME_LENGTH - 1;
    }
    frameBuffer[frameIndex++] = byte;
    if (frameIndex == FRAME_LENGTH && hasValidHeader() && hasValidTail()) {
      processRadarFrame();
      frameIndex = 0;
    }
  }
}
void addDay(uint32_t date, uint32_t wh) {
  int slot = -1;
  uint32_t oldest = 0xFFFFFFFF;
  for (int i = 0; i < 31; i++) {
    if (ledger.days[i].date == date) {
      ledger.days[i].wh += wh;
      return;
    }
    if (ledger.days[i].date < oldest) {
      oldest = ledger.days[i].date;
      slot = i;
    }
  }
  ledger.days[slot] = { date, wh };
}
uint64_t dayWh(uint32_t date) {
  for (auto &d : ledger.days)
    if (d.date == date) return d.wh;
  return 0;
}
void saveLedger() {
  if (!storageOK || !dirty) return;
  if (prefs.putBytes("ledger", &ledger, sizeof(ledger)) == sizeof(ledger)) {
    dirty = false;
    lastSave = millis();
    logEvent("Energy checkpoint saved");
  } else {
    lastSave = millis();
    logEvent("ERROR: energy checkpoint failed");
  }
}
void recordEnergy(uint32_t wh) {
  int64_t now = clockNow();
  uint32_t today = dateAt(now);
  if (!ledger.baseline) {
    ledger.baseline = true;
    ledger.previousWh = wh;
    ledger.previousTime = now;
    dirty = true;
    logEvent("PZEM baseline established; existing meter total excluded");
    saveLedger();
    return;
  }
  if (wh < ledger.previousWh) {
    logEvent("WARNING: meter energy decreased/reset; new baseline, missing energy unknown");
  } else {
    uint32_t delta = wh - ledger.previousWh;
    // Never invent daily attribution across outages, clock changes or unsynced intervals.
    if (today && ledger.previousTime && now >= ledger.previousTime && dateAt(ledger.previousTime) == today) addDay(today, delta);
    else if (today && ledger.previousTime && now >= ledger.previousTime && now - ledger.previousTime <= 15) {
      // Midnight polling interval goes to ending day; at most one polling interval error.
      addDay(today, delta);
    } else {
      ledger.unallocatedWh += delta;
      if (delta) logEvent("Energy spanning unknown/multiple dates -> unallocated: " + String(delta) + " Wh");
    }
  }
  ledger.previousWh = wh;
  ledger.previousTime = now;
  dirty = true;
  if ((uint32_t)(millis() - lastSave) >= settings.checkpoint) saveLedger();
}
uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= *p++;
    for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}
uint16_t meterReg(int n) {
  return ((uint16_t)meterBuffer[3 + n * 2] << 8) | meterBuffer[4 + n * 2];
}
uint32_t meter32(int n) {
  return meterReg(n) | ((uint32_t)meterReg(n + 1) << 16);
}
void pollMeter() {
  uint32_t now = millis();
  if (!meterPending && (uint32_t)(now - lastPoll) >= settings.meterPoll) {
    unsigned drainBudget = 256;
    while (meter.available() && drainBudget--) meter.read();
    uint8_t request[8] = { METER_ADDRESS, 0x04, 0, 0, 0, 10, 0, 0 };
    uint16_t crc = crc16(request, 6);
    request[6] = crc & 255;
    request[7] = crc >> 8;
    meter.write(request, 8);
    meterLength = 0;
    meterPending = true;
    requestAt = lastPoll = now;
  }
  unsigned receiveBudget = 64;
  while (meterPending && meter.available() && receiveBudget--) {
    uint8_t b = meter.read();
    if (meterLength == 0 && b != METER_ADDRESS) continue;
    if (meterLength >= sizeof(meterBuffer)) {
      meterPending = false;
      break;
    }
    meterBuffer[meterLength++] = b;
    size_t needed = 0;
    if (meterLength >= 2 && meterBuffer[1] == 0x84) needed = 5;
    else if (meterLength >= 3 && meterBuffer[1] == 4 && meterBuffer[2] == 20) needed = 25;
    else if (meterLength >= 3) {
      meterPending = false;
      logEvent("PZEM malformed reply");
      break;
    }
    if (needed && meterLength == needed) {
      meterPending = false;
      uint16_t received = meterBuffer[needed - 2] | ((uint16_t)meterBuffer[needed - 1] << 8);
      if (crc16(meterBuffer, needed - 2) != received) {
        logEvent("PZEM CRC error");
        break;
      }
      if (needed == 5) {
        logEvent("PZEM Modbus exception " + String(meterBuffer[2]));
        break;
      }
      voltage = meterReg(0) / 10.0f;
      current = meter32(1) / 1000.0f;
      power = meter32(3) / 10.0f;
      meterWh = meter32(5);
      frequency = meterReg(7) / 10.0f;
      pf = meterReg(8) / 100.0f;
      meterSeen = true;
      lastMeterOK = now;
      recordEnergy(meterWh);
    }
  }
  if (meterPending && (uint32_t)(now - requestAt) > 500) {
    meterPending = false;
    logEvent("PZEM read timeout");
  }
  bool online = meterOnline();
  if (online != meterWasOnline) {
    meterWasOnline = online;
    logEvent(online ? "PZEM online" : "PZEM offline: readings stale");
  }
}

const char *modeLabel(uint8_t mode) {
  return mode == 0 ? "AUTO" : mode == 1 ? "ON"
                                        : "OFF";
}
uint32_t vacancyRemaining(int ch) {
  uint32_t elapsed = millis() - lastPresence;
  return presenceSeen && elapsed < VACANCY_MS[ch] ? (VACANCY_MS[ch] - elapsed + 999) / 1000 : 0;
}
void oledText(int y, String text, uint8_t size = 1) {
  if (size == 2 && text.length() > 10) size = 1;
  oled.setTextSize(size);
  oled.setCursor(0, y);
  // Fixed-width built-in font: 21 columns at size 1, 10 at size 2.
  oled.print(text.substring(0, size == 1 ? 21 : 10));
}
bool oledResponds(uint8_t address) {
  Wire.beginTransmission(address);
  oledI2CError = Wire.endTransmission();
  return oledI2CError == 0;
}
void restoreButtonAuto(const char *reason) {
  standby = false;
  oledView = 0;
  oledReinitializeRequested = true;
  modes[0] = modes[1] = 0;
  // Resume based on fresh detection, without inheriting a pre-standby hold timer.
  presenceSeen = false;
  lastPresence = millis();
  logEvent(reason);
  controlRelays();
}
uint32_t buttonStandbySeconds() {
  return standby ? 0 : button.countdown(millis());
}
void updateButton() {
  uint32_t now = millis();
  ButtonInput::Action action = button.update(digitalRead(BUTTON_PIN) == LOW, now, standby);
  if (button.edge) {
    Serial.printf("Button GPIO%d: %s\n", BUTTON_PIN, button.pressed ? "PRESSED (LOW)" : "RELEASED (HIGH)");
    lastOLED = now - 1000;
  }
  static uint32_t lastCountdown = 0;
  uint32_t countdown = buttonStandbySeconds();
  if (countdown != lastCountdown) {
    lastCountdown = countdown;
    if (countdown) Serial.printf("Button hold: %lus until standby (GPIO%d must stay LOW)\n",
                                (unsigned long)countdown, BUTTON_PIN);
  }
  switch (action) {
    case ButtonInput::RESUME:
      restoreButtonAuto("Physical button: resumed AUTO");
      break;
    case ButtonInput::STANDBY:
      standby = true;
      modes[0] = modes[1] = 2;
      controlRelays();
      logEvent("Physical button: entered STANDBY; both loads commanded OFF");
      lastOLED = now - 1000;
      saveLedger();
      break;
    case ButtonInput::AUTO:
      restoreButtonAuto("Physical button: both channels set to AUTO");
      break;
    case ButtonInput::NEXT_VIEW:
      oledView = (oledView + 1) % 3;
      lastOLED = now - 1000;
      logEvent("Physical button: OLED view -> " + String(oledView + 1));
      break;
    case ButtonInput::CANCEL_HOLD:
      Serial.println("Button: hold cancelled; no page change");
      break;
    default: break;
  }
}
// Check every command/data transfer; an address ACK alone cannot verify a frame.
bool sendOLEDFrame() {
  const uint8_t commands[] = {0x00, SSD1306_MEMORYMODE, 0x00,
    SSD1306_COLUMNADDR, 0, OLED_WIDTH - 1,
    SSD1306_PAGEADDR, 0, OLED_HEIGHT / 8 - 1, SSD1306_DISPLAYON};
  Wire.beginTransmission(oledAddress);
  Wire.write(commands, sizeof(commands));
  oledI2CError = Wire.endTransmission();
  if (oledI2CError) return false;
  const uint8_t *buffer = oled.getBuffer();
  constexpr size_t frameBytes = OLED_WIDTH * OLED_HEIGHT / 8;
  for (size_t offset = 0; offset < frameBytes; offset += 16) {
    Wire.beginTransmission(oledAddress);
    Wire.write((uint8_t)0x40);
    Wire.write(buffer + offset, 16);
    oledI2CError = Wire.endTransmission();
    if (oledI2CError) return false;
  }
  ++oledFrames;
  return true;
}
// Keep all live information visible on one 128x64 screen.
bool renderOLED() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);
  bool validTime = clockNow() != 0;
  oledText(0, validTime ? timestamp() : String("TIME NOT SET"));
  oled.drawFastHLine(0, 10, OLED_WIDTH, SSD1306_WHITE);
  if (standby) {
    oledText(17, "STANDBY", 2);
    oledText(35, "L:OFF F:OFF (command)");
    oledText(45, meterOnline() ? String("Measured: ") + String(power, 1) + " W" : String("PZEM OFFLINE"));
    oledText(56, "Press: resume AUTO");
    return sendOLEDFrame();
  }
  if (buttonStandbySeconds()) {
    oledText(17, "Standby in " + String(buttonStandbySeconds()) + "s");
    oledText(35, "Release to cancel");
    oledText(53, "Hold "+String(settings.buttonHold/1000.0,1)+"s: OFF");
    return sendOLEDFrame();
  }
  if (oledView == 1) {
    bool live = meterOnline() && isfinite(power);
    oledText(14, live ? String(power, 1) + " W" : String("W: --"), 2);
    oledText(34, live ? String(voltage, 1) + "V " + String(current, 3) + "A" : String("PZEM OFFLINE"));
    oledText(45, live ? String(frequency, 1) + "Hz PF " + String(pf, 2) : String("Readings unavailable"));
    oledText(56, "ELECTRICITY  2/3");
    return sendOLEDFrame();
  }
  if (oledView == 2) {
    oledText(13, WiFi.status() == WL_CONNECTED ? String("LAN:") + WiFi.localIP().toString() : String("LAN: disconnected"));
    oledText(24, String("AP:") + WiFi.softAPIP().toString());
    oledText(35, "Home Wi-Fi MAC:");
    oledText(46, WiFi.macAddress());
    oledText(56, "NETWORK  3/3");
    return sendOLEDFrame();
  }
  if (!radarOnline()) {
    oledText(13, "Room: RADAR OFFLINE");
  } else {
    int count = targetCount();
    oledText(13, count ? String("Room: OCCUPIED (") + String(count) + ")"
                       : String("Room: NO DETECTION"));
  }
  bool liveMeter = meterOnline() && isfinite(power);
  oledText(26, liveMeter ? String(power, 1) + " W" : String("W: --"), 2);
  oledText(45, String("L:") + (relays[0] ? "ON" : "OFF") + "  F:" + (relays[1] ? "ON" : "OFF"));
  oledText(56, !validTime  ? String("Set time on dashboard")
               : liveMeter ? String("PZEM LIVE  SMART ROOM")
                           : String("PZEM OFFLINE"));
  return sendOLEDFrame();
}
void initializeOLED() {
  lastOLEDProbe = millis();
  // Restart the dedicated OLED bus on recovery, rather than reusing a stuck bus.
  if (oledBusStarted) Wire.end();
  oledBusStarted = Wire.begin(OLED_SDA, OLED_SCL);
  if (!oledBusStarted) {
    oledReady = false;
    logEvent("OLED I2C bus start failed: SDA=8 SCL=9");
    return;
  }
  Wire.setClock(100000);
  Wire.setTimeOut(100);
  oledAddress = oledResponds(0x3C) ? 0x3C : oledResponds(0x3D) ? 0x3D
                                                               : 0;
  if (!oledAddress) {
    oledReady = false;
    logEvent("OLED unavailable: no I2C ACK at 0x3C/0x3D; error=" + String(oledI2CError) + "; retry in 5s");
    return;
  }
  oledReady = oled.begin(SSD1306_SWITCHCAPVCC, oledAddress, true, false);
  if (!oledReady) {
    logEvent("OLED initialization/allocation failed; automation continues");
    return;
  }
  oledReady = renderOLED();
  if (!oledReady) {
    logEvent("OLED not responding after initialization; error=" + String(oledI2CError));
    return;
  }
  oledFailures = 0;
  lastOLED = millis();
  logEvent("OLED initialized at 0x" + String(oledAddress, HEX) + " SDA=8 SCL=9 (SSD1306)");
}
void updateOLED() {
  uint32_t now = millis();
  if (oledReinitializeRequested) {
    oledReinitializeRequested = false;
    initializeOLED();
    return;
  }
  if (!oledReady) {
    if ((uint32_t)(now - lastOLEDProbe) >= 5000) initializeOLED();
    return;
  }
  if ((uint32_t)(now - lastOLED) < settings.oledRefresh) return;
  lastOLED = now;
  bool responding = renderOLED();
  if (responding) {
    oledFailures = 0;
    return;
  }
  if (++oledFailures < 3) return;  // Ignore isolated errors; retry next refresh.
  oledReady = false;
  lastOLEDProbe = millis();
  logEvent("OLED offline after 3 failed checks; I2C error=" + String(oledI2CError) + "; bus recovery in 5s");
}

String quote(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return "\"" + s + "\"";
}
String number(float f, int digits = 2) {
  return isfinite(f) ? String(f, digits) : String("null");
}
bool authorized() {
  if (!REQUIRE_WEB_LOGIN) return true;
  if (server.authenticate(WEB_USER, WEB_PASSWORD)) return true;
  server.requestAuthentication();
  return false;
}
void sendJSON(int code, const String &s) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", s);
}
String radarJSON() {
  int64_t epoch = clockNow();
  uint32_t today = dateAt(epoch);
  String s = "{\"time\":" + quote(timestamp()) + ",\"timeValid\":" + (epoch ? "true" : "false");
  s += ",\"standby\":" + String(standby ? "true" : "false") + ",\"deviceStatus\":" + quote(standby ? "STANDBY" : "RUNNING");
  s += ",\"buttonCountdown\":" + String(buttonStandbySeconds()) + ",\"buttonPin\":" + String(BUTTON_PIN) + ",\"oledView\":" + String(oledView);
  s += ",\"buttonDown\":" + String(digitalRead(BUTTON_PIN) == LOW ? "true" : "false");
  s += ",\"storageOK\":" + String(storageOK ? "true" : "false") + ",\"radarOnline\":" + (radarOnline() ? "true" : "false") + ",\"personCount\":" + String(targetCount());
  s += ",\"meterOnline\":" + String(meterOnline() ? "true" : "false") + ",\"voltage\":" + number(voltage) + ",\"current\":" + number(current, 3) + ",\"power\":" + number(power, 1);
  s += ",\"frequency\":" + number(frequency, 1) + ",\"pf\":" + number(pf) + ",\"totalKWh\":" + (meterSeen ? String(meterWh / 1000.0, 3) : String("null"));
  s += ",\"todayKWh\":" + (today ? String(dayWh(today) / 1000.0, 3) : String("null")) + ",\"unallocatedKWh\":" + String(ledger.unallocatedWh / 1000.0, 3);
  s += ",\"room\":" + quote(ROOM_NAME) + ",\"oledOnline\":" + String(oledReady ? "true" : "false") + ",\"uptimeSec\":" + String(millis() / 1000);
  s += ",\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",\"localIP\":" + quote(WiFi.localIP().toString()) + ",\"apIP\":" + quote(WiFi.softAPIP().toString());
  s += ",\"stationMAC\":" + quote(WiFi.macAddress()) + ",\"hotspotMAC\":" + quote(WiFi.softAPmacAddress());
  s += ",\"delaysSec\":[" + String(VACANCY_MS[0] / 1000) + "," + String(VACANCY_MS[1] / 1000) + "]";
  s += ",\"zone\":{\"xMin\":" + String(X_MIN) + ",\"xMax\":" + String(X_MAX) + ",\"yMin\":" + String(Y_MIN) + ",\"yMax\":" + String(Y_MAX) + "}";
  s += ",\"channels\":[";
  for (int i = 0; i < 2; i++) {
    if (i) s += ",";
    uint32_t elapsed = millis() - lastPresence;
    uint32_t remaining = !standby && presenceSeen && elapsed < VACANCY_MS[i] ? (VACANCY_MS[i] - elapsed + 999) / 1000 : 0;
    s += "{\"on\":" + String(relays[i] ? "true" : "false") + ",\"mode\":" + String(modes[i]) + ",\"remaining\":" + String(remaining) + "}";
  }
  s += "],\"targets\":[";
  for (int i = 0; i < 3; i++) {
    if (i) s += ",";
    auto &t = targets[i];
    s += "{\"distance\":" + String(t.distance) + ",\"resolution\":" + String(t.resolution) + ",\"x\":" + String(t.x) + ",\"y\":" + String(t.y) + ",\"speed\":" + String(t.speed) + ",\"active\":" + String(radarOnline() && t.active ? "true" : "false") + ",\"inZone\":" + String(radarOnline() && t.inZone ? "true" : "false") + "}";
  }
  s += "],\"days\":[";
  bool first = true;
  for (auto &d : ledger.days)
    if (d.date) {
      if (!first) s += ",";
      first = false;
      s += "{\"date\":" + quote(dateText(d.date)) + ",\"kWh\":" + String(d.wh / 1000.0, 3) + "}";
    }
  s += "],\"events\":[";
  for (unsigned i = 0; i < eventCount; i++) {
    if (i) s += ",";
    s += quote(events[(eventNext + 40 - 1 - i) % 40]);
  }
  s += "]}";
  return s;
}
void handleRadarApi() { if(authorized()) sendJSON(200,radarJSON()); }
bool cloudPublishNow=false;
void publishCloudSnapshot() {
  if(!cloudConfig.enabled || otaBootPending || (!cloudPublishNow && millis()-cloudLastQueued<cloudConfig.intervalSeconds*1000UL))return;
  cloudPublishNow=false;
  cloudLastQueued=millis();
  JsonDocument d; deserializeJson(d,radarJSON());
  d["remoteControl"]=true;
  d.remove("localIP");d.remove("apIP");d.remove("stationMAC");d.remove("hotspotMAC");d.remove("events");d.remove("targets");
  String payload;serializeJson(d,payload);queueCloudTelemetry(payload);
}
extern String otaStatus;
extern bool otaRequested;
uint32_t restartAt = 0;
void startWebServer() {
  server.on("/api/cloud",HTTP_GET,[](){
    if(!authorized())return;CloudState state=cloudSnapshot();JsonDocument d;
    d["enabled"]=cloudConfig.enabled;d["intervalSeconds"]=cloudConfig.intervalSeconds;d["apiKey"]=cloudConfig.apiKey;d["databaseURL"]=cloudConfig.databaseURL;
    d["deviceUid"]=state.uid;d["status"]=state.status;d["lastSuccessAgeSec"]=nullptr;
    if(state.lastSuccess)d["lastSuccessAgeSec"]=(millis()-state.lastSuccess)/1000;
    String out;serializeJson(d,out);sendJSON(200,out);
  });
  server.on("/api/cloud",HTTP_POST,[](){
    if(!authorized())return;String body=server.arg("plain");JsonDocument d;
    if(body.length()>2048 || deserializeJson(d,body) || !d["enabled"].is<bool>() || !d["intervalSeconds"].is<uint32_t>() || !d["apiKey"].is<const char*>() || !d["databaseURL"].is<const char*>()) {sendJSON(400,"{\"error\":\"Invalid cloud settings\"}");return;}
    CloudConfig c=cloudConfig;String key=d["apiKey"].as<String>(),url=d["databaseURL"].as<String>();
    if(key.length()>128 || url.length()>192){sendJSON(400,"{\"error\":\"Cloud configuration too long\"}");return;}
    c.enabled=d["enabled"];c.intervalSeconds=d["intervalSeconds"];strlcpy(c.apiKey,key.c_str(),sizeof(c.apiKey));strlcpy(c.databaseURL,url.c_str(),sizeof(c.databaseURL));
    String error=validateCloudConfig(c);if(error.length()){sendJSON(400,"{\"error\":"+quote(error)+"}");return;}
    if(!saveCloudConfig(c)){sendJSON(500,"{\"error\":\"Cloud settings save failed\"}");return;}
    standby=true;controlRelays();saveLedger();sendJSON(200,"{\"ok\":true}");restartAt=millis()+1000;
  });
  server.on("/api/ota",HTTP_GET,[](){ if(!authorized())return; sendJSON(200,"{\"version\":"+quote(FIRMWARE_VERSION)+",\"status\":"+quote(otaStatus)+"}"); });
  server.on("/api/ota",HTTP_POST,[](){ if(!authorized())return; otaRequested=true; sendJSON(202,"{\"ok\":true}"); });
  server.on("/api/settings", HTTP_GET, []() {
    if (!authorized()) return;
    JsonDocument d; settingsJSON(d); String out; serializeJson(d,out); sendJSON(200,out);
  });
  server.on("/api/settings", HTTP_POST, []() {
    if (!authorized()) return;
    String body=server.arg("plain");
    if (body.length()>4096) { sendJSON(413,"{\"error\":\"Settings too large\"}"); return; }
    JsonDocument d; if (deserializeJson(d,body)) { sendJSON(400,"{\"error\":\"Invalid JSON\"}"); return; }
    AppSettings candidate=settings; String error=parseSettings(d,candidate);
    if (error.length()) { sendJSON(400,"{\"error\":"+quote(error)+"}"); return; }
    if (!writeSettings(candidate)) { sendJSON(500,"{\"error\":\"Settings could not be saved\"}"); return; }
    standby=true; controlRelays(); saveLedger();
    sendJSON(200,"{\"ok\":true,\"restartRequired\":true}"); restartAt=millis()+1000;
  });
  server.on("/app.css", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/css", DASHBOARD_CSS);
  });
  server.on("/app.js", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "application/javascript", DASHBOARD_JS);
  });
  server.on("/manifest.webmanifest", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "application/manifest+json", DASHBOARD_MANIFEST);
  });
  server.on("/sw.js", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Service-Worker-Allowed", "/");
    server.send_P(200, "application/javascript", DASHBOARD_SW);
  });
  server.on("/icon-192.png", HTTP_GET, []() {
    server.send_P(200, "image/png", reinterpret_cast<const char *>(DASHBOARD_ICON_192), sizeof(DASHBOARD_ICON_192));
  });
  server.on("/icon-512.png", HTTP_GET, []() {
    server.send_P(200, "image/png", reinterpret_cast<const char *>(DASHBOARD_ICON_512), sizeof(DASHBOARD_ICON_512));
  });
  server.on("/", HTTP_GET, []() {
    if (!authorized()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
  });
  server.on("/api/data", HTTP_GET, handleRadarApi);
  server.on("/api/resume", HTTP_POST, []() {
    if (!authorized()) return;
    if (standby) {
      button.begin(digitalRead(BUTTON_PIN) == LOW, millis());
      restoreButtonAuto("Website: resumed AUTO");
    }
    sendJSON(200, "{\"ok\":true}");
  });
  server.on("/api/control", HTTP_POST, []() {
    if (!authorized()) return;
    if (standby) {
      sendJSON(409, "{\"error\":\"Device is in standby. Press the physical button to resume AUTO.\"}");
      return;
    }
    String c = server.arg("ch"), m = server.arg("mode");
    if ((c != "0" && c != "1") || (m != "0" && m != "1" && m != "2")) {
      sendJSON(400, "{\"error\":\"Invalid channel or mode\"}");
      return;
    }
    int ch = c.toInt();
    modes[ch] = m.toInt();
    logEvent(String(names[ch]) + " mode -> " + (modes[ch] == 0 ? "AUTO" : modes[ch] == 1 ? "ON"
                                                                                         : "OFF"));
    controlRelays();
    sendJSON(200, "{\"ok\":true}");
  });
  server.on("/api/time", HTTP_POST, []() {
    if (!authorized()) return;
    String value = server.arg("epoch");
    bool valid = value.length() >= 10 && value.length() <= 11;
    for (unsigned i = 0; i < value.length(); i++)
      if (value[i] < '0' || value[i] > '9') valid = false;
    int64_t epoch = strtoll(value.c_str(), nullptr, 10);
    if (!valid || epoch < 1704067200LL || epoch > 4102444800LL) {
      sendJSON(400, "{\"error\":\"Invalid epoch\"}");
      return;
    }
    timeval tv{};
    tv.tv_sec = epoch;
    settimeofday(&tv, nullptr);
    logEvent("Clock set from browser");
    sendJSON(200, "{\"ok\":true}");
  });
  server.on("/api/save", HTTP_POST, []() {
    if (!authorized()) return;
    saveLedger();
    sendJSON(storageOK && !dirty ? 200 : 500, storageOK && !dirty ? "{\"ok\":true}" : "{\"error\":\"Save failed\"}");
  });
  server.on("/energy.csv", HTTP_GET, []() {
    if (!authorized()) return;
    String csv = "date,energy_kWh\n";
    for (auto &d : ledger.days)
      if (d.date) csv += dateText(d.date) + "," + String(d.wh / 1000.0, 3) + "\n";
    csv += "UNALLOCATED," + String(ledger.unallocatedWh / 1000.0, 3) + "\n";
    server.sendHeader("Content-Disposition", "attachment; filename=classroom-energy.csv");
    server.send(200, "text/csv", csv);
  });
  server.on("/events.txt", HTTP_GET, []() {
    if (!authorized()) return;
    String s;
    for (unsigned i = 0; i < eventCount; i++) s += events[(eventNext + 40 - eventCount + i) % 40] + "\n";
    server.send(200, "text/plain", s);
  });
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
}
void connectToHomeWiFi() {
  Serial.println("Connecting to home Wi-Fi (nonblocking)...");
  WiFi.begin(HOME_WIFI_SSID, HOME_WIFI_PASSWORD);
}
void startAccessPoint() {
  if (WiFi.softAP(AP_WIFI_SSID, AP_WIFI_PASSWORD)) logEvent("Hotspot: " + String(AP_WIFI_SSID) + " http://" + WiFi.softAPIP().toString());
  else logEvent("ERROR: hotspot failed");
}

void firmwareSetup() {
  loadSettings();
  loadCloudConfig();
  button.holdMs = settings.buttonHold;
  // Latch OFF before enabling outputs. Hardware must also hold relay inputs OFF during reset.
  for (int i = 0; i < 2; i++) {
    digitalWrite(RELAY_PINS[i], RELAY_ACTIVE_LOW ? HIGH : LOW);
    pinMode(RELAY_PINS[i], OUTPUT);
  }
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  button.begin(digitalRead(BUTTON_PIN) == LOW, millis());
  Serial.printf("Button GPIO%d to GND; hold %lu ms for standby\n", BUTTON_PIN,(unsigned long)settings.buttonHold);
  delay(1000);
  Serial.println("SmartClassroom++ firmware " FIRMWARE_VERSION);
  initializeTargets();
  RadarSerial.setRxBufferSize(2048);
  RadarSerial.begin(256000, SERIAL_8N1, RADAR_RX, RADAR_TX);
  meter.begin(9600, SERIAL_8N1, METER_RX, METER_TX);
  Serial.printf("PZEM UART1: RX=GPIO%d TX=GPIO%d baud=9600 address=0x%02X\n", METER_RX, METER_TX, METER_ADDRESS);
  storageOK = prefs.begin("classroom", false);
  if (storageOK && prefs.getBytesLength("ledger") == sizeof(ledger)) prefs.getBytes("ledger", &ledger, sizeof(ledger));
  if (ledger.magic != LEDGER_MAGIC) {
    memset(&ledger, 0, sizeof(ledger));
    ledger.magic = LEDGER_MAGIC;
  }
  logEvent("Boot: outputs OFF; both channels AUTO; serial 115200 baud");
  if (!storageOK) logEvent("ERROR: persistent energy storage unavailable");
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  startAccessPoint();
  Serial.println("Home Wi-Fi MAC (router allowlist): " + WiFi.macAddress());
  Serial.println("Hotspot MAC: " + WiFi.softAPmacAddress());
  connectToHomeWiFi();
  configTime(0, 0, "pool.ntp.org", "time.google.com");  // Epoch stays UTC; display/date code adds UTC+8.
  startWebServer();
  logEvent("Web server ready; AUTO holds outputs if radar unavailable");
  initializeOLED();
}
void firmwareLoop() {
  if (restartAt && (int32_t)(millis()-restartAt)>=0) ESP.restart();
  if (!otaWaitStarted) otaWaitStarted=millis();
  bool startupReady=WiFi.status()==WL_CONNECTED && time(nullptr)>=1704067200;
  if (otaBootPending && (!settings.autoUpdate || startupReady || millis()-otaWaitStarted>=20000)) {
    if(settings.autoUpdate) checkGithubOTA(); else otaStatus="Automatic updates disabled";
    otaBootPending=false; esp_ota_mark_app_valid_cancel_rollback();
  }
  if (otaRequested) { otaRequested=false; standby=true; controlRelays(); saveLedger(); checkGithubOTA(); }
  updateButton();
  if(cloudCommands && cloudAcks) {
    CloudCommand command;
    if(xQueueReceive(cloudCommands,&command,0)==pdTRUE) {
      CloudAck ack={};ack.command=command;
      if(int64_t(time(nullptr))*1000>command.expiresAt)strlcpy(ack.status,"expired",sizeof(ack.status));
      else if(standby || otaBootPending || restartAt)strlcpy(ack.status,"blocked_standby",sizeof(ack.status));
      else {
        modes[command.channel]=command.mode;
        logEvent(String("Online: ")+names[command.channel]+" mode -> "+(command.mode==0?"AUTO":command.mode==1?"ON":"OFF"));
        controlRelays();cloudPublishNow=true;
        strlcpy(ack.status,"applied",sizeof(ack.status));
      }
      xQueueSend(cloudAcks,&ack,0);
    }
  }
  readRadarData();
  if(!otaBootPending) controlRelays();
  pollMeter();
  updateOLED();
  server.handleClient();
  publishCloudSnapshot();
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifiWasConnected) {
    wifiWasConnected = connected;
    logEvent(connected ? "Wi-Fi connected: http://" + WiFi.localIP().toString() : "Wi-Fi disconnected; hotspot still available");
  }
  if (!connected && (uint32_t)(millis() - lastReconnect) >= 30000) {
    lastReconnect = millis();
    WiFi.reconnect();
  }
  if ((uint32_t)(millis() - lastSummary) >= 5000) {
    lastSummary = millis();
    Serial.printf("Button GPIO%d=%s debounced=%s device=%s view=%u\n", BUTTON_PIN,
      digitalRead(BUTTON_PIN) == LOW ? "LOW" : "HIGH", button.pressed ? "pressed" : "released",
      standby ? "STANDBY" : "RUNNING", oledView + 1);
    Serial.printf("OLED frames=%lu lastAttemptAge=%lums I2Cerror=%u heap=%lu\n",
      (unsigned long)oledFrames, (unsigned long)(millis() - lastOLED),
      oledI2CError, (unsigned long)ESP.getFreeHeap());
    Serial.printf("[%s] radar=%s zoneTargets=%d Lights=%s(mode=%u) Fan=%s(mode=%u) PZEM=%s OLED=%s V=%.1f A=%.3f W=%.1f Hz=%.1f PF=%.2f meter=%.3f kWh today=%.3f kWh unallocated=%.3f kWh\n", timestamp().c_str(), radarOnline() ? "OK" : "FAULT", targetCount(), relays[0] ? "ON" : "OFF", modes[0], relays[1] ? "ON" : "OFF", modes[1], meterOnline() ? "OK" : "STALE", oledReady ? "OK" : "FAULT", meterOnline() ? voltage : NAN, meterOnline() ? current : NAN, meterOnline() ? power : NAN, meterOnline() ? frequency : NAN, meterOnline() ? pf : NAN, meterWh / 1000.0, dayWh(dateAt(clockNow())) / 1000.0, ledger.unallocatedWh / 1000.0);
    for (int i = 0; i < 3; i++)
      if (radarOnline() && targets[i].active) Serial.printf("  T%d x=%d y=%d mm speed=%d cm/s zone=%s\n", i + 1, targets[i].x, targets[i].y, targets[i].speed, targets[i].inZone ? "YES" : "NO");
  }
}

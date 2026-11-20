#pragma once
// 1-minute energy log with hourly and daily summaries (firmware 1.3.0).
// Included from ClassroomFirmware.h after its globals (settings, relays, modes, server, clock helpers).
// Storage lives in the existing 128 KB "spiffs" partition as LittleFS. The legacy NVS ledger,
// settings and cloud identity are not touched, so an OTA update keeps all earlier records.
#include <LittleFS.h>

bool authorized();

struct __attribute__((packed)) MinuteRec {
  uint32_t t;            // UTC epoch of the minute start; 0 = empty slot
  uint16_t v10;          // volts x10
  uint8_t pf100, samples;
  uint32_t a1000, w10, wp10;  // amps x1000, average/peak watts x10
  uint16_t hz10;
  uint8_t flags, reserved;
  uint32_t mwh;          // energy in this minute, milli-watt-hours
  uint32_t meterWh;      // PZEM lifetime register at the last sample
};
struct __attribute__((packed)) HourRec {
  uint32_t t, mwh, sumW10, peakW10, minW10, sumV10;  // t = local hour start (UTC epoch)
  uint16_t sumPf100, minutes, occ, lights, fan, reserved;
};
struct __attribute__((packed)) DayRec {
  uint32_t day, mwh, sumW10, peakW10, peakT, sumV10, sumPf100;  // day = local days since 1970
  uint16_t minutes, occ, lights, fan;
};
static_assert(sizeof(MinuteRec) == 32, "MinuteRec layout");
static_assert(sizeof(HourRec) == 36, "HourRec layout");
static_assert(sizeof(DayRec) == 36, "DayRec layout");

enum : uint8_t { REC_LIGHTS = 1, REC_FAN = 2, REC_OCC = 4, REC_METER = 8, REC_MANUAL = 16, REC_STANDBY = 32 };
constexpr uint32_t MINUTE_SLOTS = 1440, HOUR_SLOTS = 168, DAY_SLOTS = 31, PENDING_MAX = 16;
constexpr uint32_t ENERGY_FLUSH_MS = 600000;  // batch flash writes: at most 10 min lost on power cut
constexpr const char *MINUTE_FILE = "/minutes.bin", *HOUR_FILE = "/hours.bin", *DAY_FILE = "/days.bin";

struct MinuteAcc {
  uint32_t t = 0;
  double v = 0, a = 0, w = 0, pf = 0, hz = 0, mwh = 0;
  float peak = 0;
  uint16_t n = 0, ticks = 0, lights = 0, fan = 0, occ = 0, manual = 0, stby = 0;
  uint32_t meterWh = 0;
};
MinuteAcc minuteAcc;
MinuteRec pendingMinutes[PENDING_MAX];
uint8_t pendingCount = 0;
HourRec curHour{};
DayRec curDay{};
bool hourDirty = false, dayDirty = false, energyFSOK = false;
uint32_t lastEnergySample = 0, lastEnergyTick = 0, lastEnergyFlush = 0, lastFinalMinute = 0, savedAck = 0;
float energyRate = 0;  // cost per kWh, 0 = not set
Preferences energyPrefs;

// ---------- time helpers (local time = UTC + settings.timezoneMinutes) ----------
inline int32_t tzSeconds() { return TIMEZONE_SECONDS; }
inline uint32_t localDayNo(uint32_t t) { return (uint32_t)(((int64_t)t + tzSeconds()) / 86400); }
inline uint32_t localHourStart(uint32_t t) { return (uint32_t)((((int64_t)t + tzSeconds()) / 3600) * 3600 - tzSeconds()); }
inline void localTm(uint32_t t, struct tm &out) { time_t l = (time_t)((int64_t)t + tzSeconds()); gmtime_r(&l, &out); }
inline String fmtLocal(uint32_t t, const char *fmt) { struct tm tm; localTm(t, tm); char b[32]; strftime(b, sizeof(b), fmt, &tm); return String(b); }
inline String dayNoText(uint32_t day) { time_t d = (time_t)day * 86400; struct tm tm; gmtime_r(&d, &tm); char b[12]; strftime(b, sizeof(b), "%Y-%m-%d", &tm); return String(b); }
inline String tzSuffix() {
  int32_t m = settings.timezoneMinutes; char b[8];
  snprintf(b, sizeof(b), "%c%02ld:%02ld", m < 0 ? '-' : '+', (long)(abs(m) / 60), (long)(abs(m) % 60));
  return String(b);
}
inline String fx(double v, int digits) { return String(v, digits); }
inline String csvText(const char *s) {
  String v(s);
  if (v.indexOf(',') < 0 && v.indexOf('"') < 0 && v.indexOf('\n') < 0) return v;
  v.replace("\"", "\"\"");
  return "\"" + v + "\"";
}
inline const char *recMode(uint8_t f) { return (f & REC_STANDBY) ? "STANDBY" : (f & REC_MANUAL) ? "MANUAL" : "AUTO"; }

// ---------- flash storage ----------
inline bool ensureLogFile(const char *path, size_t bytes) {
  File f = LittleFS.open(path, "r");
  size_t size = f ? f.size() : 0;
  if (f) f.close();
  if (size == bytes) return true;
  f = LittleFS.open(path, "w");
  if (!f) return false;
  uint8_t zero[256] = {};
  for (size_t left = bytes; left;) {
    size_t n = min(left, sizeof(zero));
    if (f.write(zero, n) != n) { f.close(); return false; }
    left -= n;
  }
  f.close();
  return true;
}
inline bool readSlot(const char *path, uint32_t slot, void *rec, size_t size) {
  memset(rec, 0, size);
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  bool ok = f.seek(slot * size) && f.read((uint8_t *)rec, size) == size;
  f.close();
  if (!ok) memset(rec, 0, size);
  return ok;
}
inline bool writeSlot(File &f, uint32_t slot, const void *rec, size_t size) {
  return f.seek(slot * size) && f.write((const uint8_t *)rec, size) == size;
}
inline uint32_t hourSlot(uint32_t t) { return (t / 3600) % HOUR_SLOTS; }
inline uint32_t minuteSlot(uint32_t t) { return (t / 60) % MINUTE_SLOTS; }

void flushEnergyLog() {
  if (!energyFSOK) return;
  bool ok = true;
  if (pendingCount) {
    File f = LittleFS.open(MINUTE_FILE, "r+");
    ok = f;
    for (uint8_t i = 0; f && i < pendingCount; ++i) ok = writeSlot(f, minuteSlot(pendingMinutes[i].t), &pendingMinutes[i], sizeof(MinuteRec)) && ok;
    if (f) f.close();
    if (ok) pendingCount = 0;
  }
  if (hourDirty && curHour.t) {
    File f = LittleFS.open(HOUR_FILE, "r+");
    if (f && writeSlot(f, hourSlot(curHour.t), &curHour, sizeof(curHour))) hourDirty = false; else ok = false;
    if (f) f.close();
  }
  if (dayDirty && curDay.day) {
    File f = LittleFS.open(DAY_FILE, "r+");
    if (f && writeSlot(f, curDay.day % DAY_SLOTS, &curDay, sizeof(curDay))) dayDirty = false; else ok = false;
    if (f) f.close();
  }
  if (cloudHistoryAcked != savedAck) { energyPrefs.putUInt("ack", cloudHistoryAcked); savedAck = cloudHistoryAcked; }
  lastEnergyFlush = millis();
  if (!ok) logEvent("ERROR: energy log flash write failed");
}

void energyLogBegin() {
  if (!energyPrefs.begin("classenergy", false)) logEvent("ERROR: energy settings storage unavailable");
  energyRate = energyPrefs.getFloat("rate", 0);
  if (!isfinite(energyRate) || energyRate < 0 || energyRate > 1000) energyRate = 0;
  cloudHistoryAcked = savedAck = energyPrefs.getUInt("ack", 0);
  energyFSOK = LittleFS.begin(true, "/littlefs", 4, "spiffs") &&
               ensureLogFile(MINUTE_FILE, MINUTE_SLOTS * sizeof(MinuteRec)) &&
               ensureLogFile(HOUR_FILE, HOUR_SLOTS * sizeof(HourRec)) &&
               ensureLogFile(DAY_FILE, DAY_SLOTS * sizeof(DayRec));
  logEvent(energyFSOK ? "Energy log ready (1-minute, hourly, daily)" : "ERROR: energy log storage unavailable; CSV history disabled");
}

// ---------- aggregation ----------
void energySample(float v, float a, float w, float hz, float pf, uint32_t meterWhNow) {
  uint32_t now = millis();
  uint32_t dt = lastEnergySample ? now - lastEnergySample : 0;
  lastEnergySample = now;
  if (!minuteAcc.t || !isfinite(w)) return;
  // A long gap means the meter was offline: energy in that gap is unknown, count one interval only.
  if (dt > settings.meterPoll * 3) dt = settings.meterPoll;
  minuteAcc.v += v; minuteAcc.a += a; minuteAcc.w += w; minuteAcc.pf += pf; minuteAcc.hz += hz;
  minuteAcc.mwh += w * dt / 3600.0;
  if (w > minuteAcc.peak || !minuteAcc.n) minuteAcc.peak = w;
  minuteAcc.meterWh = meterWhNow;
  ++minuteAcc.n;
}

void loadHour(uint32_t hs) {
  readSlot(HOUR_FILE, hourSlot(hs), &curHour, sizeof(curHour));
  if (curHour.t != hs) { memset(&curHour, 0, sizeof(curHour)); curHour.t = hs; curHour.minW10 = UINT32_MAX; }
}
void loadDay(uint32_t day) {
  readSlot(DAY_FILE, day % DAY_SLOTS, &curDay, sizeof(curDay));
  if (curDay.day != day) { memset(&curDay, 0, sizeof(curDay)); curDay.day = day; }
}

void addSummaries(const MinuteRec &r) {
  uint32_t hs = localHourStart(r.t), day = localDayNo(r.t);
  if (curHour.t != hs) { if (curHour.t && hourDirty) flushEnergyLog(); loadHour(hs); }
  if (curDay.day != day) { if (curDay.day && dayDirty) flushEnergyLog(); loadDay(day); }
  bool meterOK = r.flags & REC_METER;
  if (meterOK) {
    curHour.mwh += r.mwh; curHour.sumW10 += r.w10; curHour.sumV10 += r.v10; curHour.sumPf100 += r.pf100; ++curHour.minutes;
    curHour.peakW10 = max(curHour.peakW10, r.wp10); curHour.minW10 = min(curHour.minW10, r.w10);
    curDay.mwh += r.mwh; curDay.sumW10 += r.w10; curDay.sumV10 += r.v10; curDay.sumPf100 += r.pf100; ++curDay.minutes;
    if (r.wp10 > curDay.peakW10) { curDay.peakW10 = r.wp10; curDay.peakT = r.t; }
  }
  if (r.flags & REC_OCC) { ++curHour.occ; ++curDay.occ; }
  if (r.flags & REC_LIGHTS) { ++curHour.lights; ++curDay.lights; }
  if (r.flags & REC_FAN) { ++curHour.fan; ++curDay.fan; }
  hourDirty = dayDirty = true;
}

void finalizeMinute() {
  const MinuteAcc &m = minuteAcc;
  if (!m.ticks && !m.n) return;
  MinuteRec r{};
  r.t = m.t;
  if (m.ticks) {
    if (m.lights * 2 >= m.ticks && m.lights) r.flags |= REC_LIGHTS;
    if (m.fan * 2 >= m.ticks && m.fan) r.flags |= REC_FAN;
  } else {
    if (relays[0]) r.flags |= REC_LIGHTS;
    if (relays[1]) r.flags |= REC_FAN;
  }
  if (m.occ) r.flags |= REC_OCC;
  if (m.manual) r.flags |= REC_MANUAL;
  if (m.stby) r.flags |= REC_STANDBY;
  if (m.n) {
    r.flags |= REC_METER;
    r.v10 = lround(m.v / m.n * 10); r.a1000 = lround(m.a / m.n * 1000);
    r.w10 = lround(m.w / m.n * 10); r.wp10 = lround(m.peak * 10);
    r.pf100 = lround(m.pf / m.n * 100); r.hz10 = lround(m.hz / m.n * 10);
    r.mwh = lround(m.mwh); r.meterWh = m.meterWh; r.samples = min<uint16_t>(m.n, 255);
  }
  if (pendingCount >= PENDING_MAX) flushEnergyLog();
  if (pendingCount < PENDING_MAX) pendingMinutes[pendingCount++] = r;
  addSummaries(r);
  lastFinalMinute = r.t;
}

void queueHistoryBatch();

void energyLogLoop() {
  int64_t epoch = clockNow();
  if (!epoch) { minuteAcc = MinuteAcc(); return; }
  uint32_t minute = (uint32_t)(epoch / 60 * 60);
  if (!minuteAcc.t) minuteAcc.t = minute;
  else if (minute != minuteAcc.t) {
    if (minute > minuteAcc.t && minute - minuteAcc.t <= 120) finalizeMinute();
    else logEvent("Clock changed: partial minute discarded from energy log");
    minuteAcc = MinuteAcc();
    minuteAcc.t = minute;
  }
  uint32_t now = millis();
  if (now - lastEnergyTick >= 2000) {
    lastEnergyTick = now;
    ++minuteAcc.ticks;
    if (relays[0]) ++minuteAcc.lights;
    if (relays[1]) ++minuteAcc.fan;
    if (targetCount() > 0) ++minuteAcc.occ;
    if (standby) ++minuteAcc.stby;
    else if (modes[0] || modes[1]) ++minuteAcc.manual;
  }
  if ((pendingCount || hourDirty || dayDirty) && now - lastEnergyFlush >= ENERGY_FLUSH_MS) flushEnergyLog();
  queueHistoryBatch();
}

// Looks in the unflushed buffer first, then flash.
// Pass an already-open minute file when scanning many rows (one open instead of one per row).
bool findMinute(uint32_t t, MinuteRec &r, File *file = nullptr) {
  for (uint8_t i = 0; i < pendingCount; ++i) if (pendingMinutes[i].t == t) { r = pendingMinutes[i]; return true; }
  if (!energyFSOK) return false;
  if (file && *file) return file->seek(minuteSlot(t) * sizeof(r)) && file->read((uint8_t *)&r, sizeof(r)) == sizeof(r) && r.t == t;
  return readSlot(MINUTE_FILE, minuteSlot(t), &r, sizeof(r)) && r.t == t;
}
bool findHour(uint32_t hs, HourRec &h) {
  if (curHour.t == hs) { h = curHour; return true; }
  return energyFSOK && readSlot(HOUR_FILE, hourSlot(hs), &h, sizeof(h)) && h.t == hs;
}
bool findDay(uint32_t day, DayRec &d) {
  if (curDay.day == day) { d = curDay; return true; }
  return energyFSOK && readSlot(DAY_FILE, day % DAY_SLOTS, &d, sizeof(d)) && d.day == day;
}

// ---------- shared row formatting ----------
inline double avgW(uint32_t sum10, uint16_t n) { return n ? sum10 / 10.0 / n : 0; }

// ---------- cloud history (main task builds, cloud worker sends) ----------
void addCloudMinute(JsonDocument &doc, const MinuteRec &r) {
  JsonObject o = doc[String("minutes/") + fmtLocal(r.t, "%Y-%m-%d/%H%M")].to<JsonObject>();
  o["t"] = r.t; o["z"] = settings.timezoneMinutes;
  o["l"] = (r.flags & REC_LIGHTS) ? 1 : 0; o["f"] = (r.flags & REC_FAN) ? 1 : 0;
  o["o"] = (r.flags & REC_OCC) ? 1 : 0; o["m"] = recMode(r.flags); o["ok"] = (r.flags & REC_METER) ? 1 : 0;
  if (!(r.flags & REC_METER)) return;
  o["V"] = serialized(fx(r.v10 / 10.0, 1)); o["A"] = serialized(fx(r.a1000 / 1000.0, 3));
  o["W"] = serialized(fx(r.w10 / 10.0, 1)); o["Wp"] = serialized(fx(r.wp10 / 10.0, 1));
  o["pf"] = serialized(fx(r.pf100 / 100.0, 2)); o["Hz"] = serialized(fx(r.hz10 / 10.0, 1));
  o["Wh"] = serialized(fx(r.mwh / 1000.0, 3)); o["mWh"] = r.meterWh; o["n"] = r.samples;
}
void addCloudHour(JsonDocument &doc, const HourRec &h) {
  JsonObject o = doc[String("hours/") + fmtLocal(h.t, "%Y-%m-%d/%H")].to<JsonObject>();
  o["t"] = h.t; o["kWh"] = serialized(fx(h.mwh / 1e6, 3)); o["n"] = h.minutes;
  o["occ"] = h.occ; o["lights"] = h.lights; o["fan"] = h.fan;
  if (!h.minutes) return;
  o["avgW"] = serialized(fx(avgW(h.sumW10, h.minutes), 1)); o["peakW"] = serialized(fx(h.peakW10 / 10.0, 1));
  o["minW"] = serialized(fx(h.minW10 / 10.0, 1)); o["avgV"] = serialized(fx(h.sumV10 / 10.0 / h.minutes, 1));
  o["avgPf"] = serialized(fx(h.sumPf100 / 100.0 / h.minutes, 2));
}
void addCloudDay(JsonDocument &doc, const DayRec &d) {
  JsonObject o = doc[String("days/") + dayNoText(d.day)].to<JsonObject>();
  double kwh = d.mwh / 1e6;
  o["kWh"] = serialized(fx(kwh, 3)); o["n"] = d.minutes; o["z"] = settings.timezoneMinutes;
  o["occH"] = serialized(fx(d.occ / 60.0, 1)); o["lightsH"] = serialized(fx(d.lights / 60.0, 1));
  o["fanH"] = serialized(fx(d.fan / 60.0, 1));
  if (energyRate > 0) { o["rate"] = serialized(fx(energyRate, 2)); o["cost"] = serialized(fx(kwh * energyRate, 2)); }
  if (!d.minutes) return;
  o["avgW"] = serialized(fx(avgW(d.sumW10, d.minutes), 1)); o["peakW"] = serialized(fx(d.peakW10 / 10.0, 1));
  o["peakT"] = fmtLocal(d.peakT, "%H:%M");
}

void queueHistoryBatch() {
  static uint32_t lastBuild = 0;
  if (!energyFSOK || !cloudConfig.enabled || !cloudHistoryQueue || cloudHistoryInFlight || !lastFinalMinute) return;
  if (cloudHistoryRetryAt) {  // back-off after a failed upload (cleared once passed; safe across millis() wrap)
    if ((int32_t)(millis() - cloudHistoryRetryAt) < 0) return;
    cloudHistoryRetryAt = 0;
  }
  if (millis() - lastBuild < 5000) return;
  lastBuild = millis();
  // Cloud keeps rows the device still has: never older than the 24 h flash buffer.
  uint32_t oldest = lastFinalMinute >= 86400 - 60 ? lastFinalMinute - (86400 - 60) : 60;
  uint32_t from = max(cloudHistoryAcked + 60, oldest);
  if (from > lastFinalMinute) return;
  JsonDocument doc;
  uint32_t rows = 0, scanned = 0, upTo = from - 60, lastHour = 0, lastDay = 0;
  File minutes = LittleFS.open(MINUTE_FILE, "r");
  for (uint32_t t = from; t <= lastFinalMinute && rows < 30 && scanned < 240; t += 60, ++scanned) {
    upTo = t;
    MinuteRec r;
    if (!findMinute(t, r, &minutes)) continue;
    addCloudMinute(doc, r);
    ++rows;
    uint32_t hs = localHourStart(t), day = localDayNo(t);
    HourRec h; DayRec d;
    if (hs != lastHour && findHour(hs, h)) { addCloudHour(doc, h); lastHour = hs; }
    if (day != lastDay && findDay(day, d)) { addCloudDay(doc, d); lastDay = day; }
  }
  if (minutes) minutes.close();
  if (!rows) { cloudHistoryAcked = upTo; return; }  // only gaps in this window
  // Keep 31 days of 1-minute rows online; hourly and daily summaries are kept.
  doc[String("minutes/") + dayNoText(localDayNo(lastFinalMinute) - 32)] = nullptr;
  HistoryBatch *batch = new HistoryBatch();
  serializeJson(doc, batch->body);
  batch->upTo = upTo;
  cloudHistoryInFlight = true;
  if (xQueueSend(cloudHistoryQueue, &batch, 0) != pdTRUE) { delete batch; cloudHistoryInFlight = false; }
}

// ---------- local CSV downloads ----------
void csvStart(const char *filename, const char *header) {
  flushEnergyLog();
  server.sendHeader("Content-Disposition", String("attachment; filename=") + filename);
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent(header);
}
void csvChunk(String &buffer, bool final = false) {
  if (buffer.length() && (final || buffer.length() > 1800)) { server.sendContent(buffer); buffer = ""; }
  if (final) server.sendContent("");
}
bool energyCsvReady() {
  if (!authorized()) return false;
  if (!energyFSOK) { server.send(503, "text/plain", "Energy log storage unavailable"); return false; }
  if (!clockNow()) { server.send(503, "text/plain", "Device clock not set: set the time first"); return false; }
  return true;
}
void handleMinuteCsv() {
  if (!energyCsvReady()) return;
  csvStart("classroom-energy-1min.csv", "timestamp,device_id,voltage_V,current_A,power_avg_W,power_peak_W,pf,frequency_Hz,energy_Wh,meter_total_Wh,lights,fan,mode,occupied,samples,meter_ok\n");
  uint32_t end = (uint32_t)(clockNow() / 60 * 60), start = end - 86400;
  String room = csvText(settings.room), suffix = tzSuffix(), out;
  File minutes = LittleFS.open(MINUTE_FILE, "r");
  for (uint32_t t = start + 60; t < end; t += 60) {
    MinuteRec r;
    if (!findMinute(t, r, &minutes)) continue;
    bool m = r.flags & REC_METER;
    out += fmtLocal(r.t, "%Y-%m-%dT%H:%M:%S") + suffix + "," + room + ",";
    if (m) out += fx(r.v10 / 10.0, 1) + "," + fx(r.a1000 / 1000.0, 3) + "," + fx(r.w10 / 10.0, 1) + "," + fx(r.wp10 / 10.0, 1) + "," +
                  fx(r.pf100 / 100.0, 2) + "," + fx(r.hz10 / 10.0, 1) + "," + fx(r.mwh / 1000.0, 3) + "," + String(r.meterWh) + ",";
    else out += ",,,,,,,,";
    out += String((r.flags & REC_LIGHTS) ? "ON" : "OFF") + "," + ((r.flags & REC_FAN) ? "ON" : "OFF") + "," + recMode(r.flags) + "," +
           ((r.flags & REC_OCC) ? "1" : "0") + "," + String(r.samples) + "," + (m ? "1" : "0") + "\n";
    csvChunk(out);
  }
  if (minutes) minutes.close();
  csvChunk(out, true);
}
void handleHourlyCsv() {
  if (!energyCsvReady()) return;
  csvStart("classroom-energy-hourly.csv", "hour_start,energy_kWh,avg_W,peak_W,min_W,avg_V,avg_pf,occupied_min,lights_on_min,fan_on_min,minutes_recorded\n");
  uint32_t now = (uint32_t)clockNow(), last = localHourStart(now);
  String out;
  for (int32_t i = HOUR_SLOTS - 1; i >= 0; --i) {
    uint32_t hs = localHourStart(last - (uint32_t)i * 3600);
    HourRec h;
    if (!findHour(hs, h)) continue;
    out += fmtLocal(h.t, "%Y-%m-%dT%H:%M") + "," + fx(h.mwh / 1e6, 3) + ",";
    if (h.minutes) out += fx(avgW(h.sumW10, h.minutes), 1) + "," + fx(h.peakW10 / 10.0, 1) + "," + fx(h.minW10 / 10.0, 1) + "," +
                          fx(h.sumV10 / 10.0 / h.minutes, 1) + "," + fx(h.sumPf100 / 100.0 / h.minutes, 2) + ",";
    else out += ",,,,,";
    out += String(h.occ) + "," + String(h.lights) + "," + String(h.fan) + "," + String(h.minutes) + "\n";
    csvChunk(out);
  }
  csvChunk(out, true);
}
void handleDailyCsv() {
  if (!energyCsvReady()) return;
  csvStart("classroom-energy-daily.csv", "date,energy_kWh,avg_W,peak_W,peak_time,occupied_h,lights_on_h,fan_on_h,minutes_recorded,est_cost\n");
  uint32_t today = localDayNo((uint32_t)clockNow());
  String out;
  for (int32_t i = DAY_SLOTS - 1; i >= 0; --i) {
    DayRec d;
    if (!findDay(today - i, d)) continue;
    double kwh = d.mwh / 1e6;
    out += dayNoText(d.day) + "," + fx(kwh, 3) + ",";
    if (d.minutes) out += fx(avgW(d.sumW10, d.minutes), 1) + "," + fx(d.peakW10 / 10.0, 1) + "," + fmtLocal(d.peakT, "%H:%M") + ",";
    else out += ",,,";
    out += fx(d.occ / 60.0, 1) + "," + fx(d.lights / 60.0, 1) + "," + fx(d.fan / 60.0, 1) + "," + String(d.minutes) + "," +
           (energyRate > 0 ? fx(kwh * energyRate, 2) : String("")) + "\n";
    csvChunk(out);
  }
  csvChunk(out, true);
}

void registerEnergyRoutes() {
  server.on("/minute.csv", HTTP_GET, handleMinuteCsv);
  server.on("/hourly.csv", HTTP_GET, handleHourlyCsv);
  server.on("/daily.csv", HTTP_GET, handleDailyCsv);
  server.on("/api/energy", HTTP_GET, []() {
    if (!authorized()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", String("{\"rate\":") + fx(energyRate, 2) + ",\"logOK\":" + (energyFSOK ? "true" : "false") + "}");
  });
  server.on("/api/energy", HTTP_POST, []() {
    if (!authorized()) return;
    JsonDocument d;
    String body = server.arg("plain");
    if (body.length() > 256 || deserializeJson(d, body) || !d["rate"].is<float>()) { server.send(400, "application/json", "{\"error\":\"Rate must be a number\"}"); return; }
    float rate = d["rate"];
    if (!isfinite(rate) || rate < 0 || rate > 1000) { server.send(400, "application/json", "{\"error\":\"Rate must be 0..1000 per kWh\"}"); return; }
    if (energyPrefs.putFloat("rate", rate) != sizeof(float)) { server.send(500, "application/json", "{\"error\":\"Rate could not be saved\"}"); return; }
    energyRate = rate;
    dayDirty = curDay.day != 0;  // next cloud batch carries the new cost
    logEvent("Electricity rate set to " + fx(rate, 2) + " per kWh");
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

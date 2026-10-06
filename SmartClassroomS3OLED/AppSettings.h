#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cstring>

struct AppSettings {
  uint32_t schema = 1;
  uint32_t vacancy[2] = {30000, 15000};
  uint32_t radarTimeout = 3000, oledRefresh = 1000, meterPoll = 2000;
  uint32_t checkpoint = 300000, buttonHold = 5000;
  int32_t xMin = -4000, xMax = 4000, yMin = 0, yMax = 6000;
  int32_t timezoneMinutes = 480;
  int pins[9] = {16,17,4,5,8,9,10,11,3};
  bool activeLow = true, autoUpdate = true;
  char room[49] = "Room 101";
  char ssid[33] = "", wifiPassword[65] = "";
  char apName[33] = "ESP32-LD2450", apPassword[65] = "ESP32Radar123";
  char webUser[33] = "admin", webPassword[65] = "ChangeThisPassword";
  char repository[97] = "0xMrw1ck/SmartClassroomPlusPlus";
};
AppSettings settings;
bool settingsStorageOK = false;

// This file is local-only and excluded from public source/release builds.
#if !defined(PUBLIC_RELEASE) && __has_include("LocalProvisioning.h")
#include "LocalProvisioning.h"
#else
inline void provisionLocalDefaults(AppSettings &) {}
#endif

inline bool validRepository(const char *s) {
  unsigned slashes = 0;
  if (!s[0] || s[0]=='/' || s[strlen(s)-1]=='/') return false;
  for (; *s; ++s) {
    if (*s=='/') ++slashes;
    else if (!isalnum((unsigned char)*s) && *s!='-' && *s!='_' && *s!='.') return false;
  }
  return slashes==1;
}
inline String validateSettings(const AppSettings &s) {
  for (auto ms : s.vacancy) if (ms < 1000 || ms > 86400000) return "Vacancy delay must be 1..86400 seconds";
  if (s.radarTimeout<500 || s.radarTimeout>30000) return "Radar timeout must be 500..30000 ms";
  if (s.oledRefresh<250 || s.oledRefresh>5000) return "OLED refresh must be 250..5000 ms";
  if (s.meterPoll<1000 || s.meterPoll>10000) return "Meter poll must be 1000..10000 ms";
  if (s.checkpoint<60000 || s.checkpoint>3600000) return "Checkpoint must be 60..3600 seconds";
  if (s.buttonHold<2000 || s.buttonHold>15000) return "Button hold must be 2000..15000 ms";
  if (s.xMin < -6000 || s.xMax>6000 || s.xMin>=s.xMax || s.yMin<0 || s.yMax>6000 || s.yMin>=s.yMax) return "Invalid radar zone (millimetres)";
  if (s.timezoneMinutes < -720 || s.timezoneMinutes > 840) return "Timezone must be -720..840 minutes";
  if (!s.room[0] || !s.apName[0] || !s.webUser[0] || !s.webPassword[0]) return "Room, hotspot and login cannot be empty";
  if (strlen(s.apPassword)<8 || strlen(s.apPassword)>63) return "Hotspot password must be 8..63 characters";
  if (s.wifiPassword[0] && (strlen(s.wifiPassword)<8 || strlen(s.wifiPassword)>63)) return "Wi-Fi password must be blank (open network) or 8..63 characters";
  if (!validRepository(s.repository)) return "Repository must be owner/name";
  for (int i=0;i<9;++i) {
    int p=s.pins[i];
    bool allowed=(p>=1 && p<=18) || p==21 || (p>=38 && p<=44) || p==47 || p==48;
    if (!allowed) return "GPIO must be an exposed, usable S3 pin (USB/memory/reserved pins excluded)";
    for (int j=0;j<i;++j) if (p==s.pins[j]) return "GPIO assignments must be unique";
  }
  return "";
}
inline bool writeSettings(const AppSettings &s) {
  Preferences store;
  if (!store.begin("classconfig",false)) return false;
  bool ok=store.putBytes("settings",&s,sizeof(s))==sizeof(s);
  store.end();
  return ok;
}
inline void loadSettings() {
  provisionLocalDefaults(settings);
  Preferences store;
  if (!store.begin("classconfig",false)) return;
  if (store.getBytesLength("settings")==sizeof(settings)) {
    AppSettings loaded;
    store.getBytes("settings",&loaded,sizeof(loaded));
    // Ensure stored strings are terminated before any validation/string use.
    loaded.room[48]=0; loaded.ssid[32]=0; loaded.wifiPassword[64]=0;
    loaded.apName[32]=0; loaded.apPassword[64]=0; loaded.webUser[32]=0;
    loaded.webPassword[64]=0; loaded.repository[96]=0;
    if (loaded.schema==1 && validateSettings(loaded).isEmpty()) {
      settings=loaded; settingsStorageOK=true;
    }
  } else {
    settingsStorageOK=store.putBytes("settings",&settings,sizeof(settings))==sizeof(settings);
  }
  store.end();
}
inline void settingsJSON(JsonDocument &d) {
  d["room"]=settings.room; d["lightsSeconds"]=settings.vacancy[0]/1000;
  d["fanSeconds"]=settings.vacancy[1]/1000; d["radarTimeoutMs"]=settings.radarTimeout;
  d["oledRefreshMs"]=settings.oledRefresh; d["meterPollMs"]=settings.meterPoll;
  d["checkpointSeconds"]=settings.checkpoint/1000; d["buttonHoldMs"]=settings.buttonHold;
  d["xMin"]=settings.xMin; d["xMax"]=settings.xMax; d["yMin"]=settings.yMin; d["yMax"]=settings.yMax;
  d["timezoneMinutes"]=settings.timezoneMinutes; d["ssid"]=settings.ssid;
  d["apName"]=settings.apName; d["webUser"]=settings.webUser;
  d["repository"]=settings.repository; d["autoUpdate"]=settings.autoUpdate;
  d["activeLow"]=settings.activeLow; d["saved"]=settingsStorageOK;
  JsonArray pins=d["pins"].to<JsonArray>(); for (int p:settings.pins) pins.add(p);
}
// Reject malformed/missing fields instead of silently interpreting them as zero.
inline String parseSettings(JsonDocument &d, AppSettings &s) {
  struct Number { const char *name; int32_t min,max; uint32_t multiplier; void *target; bool signedValue; };
  Number values[] = {
    {"lightsSeconds",1,86400,1000,&s.vacancy[0],false},{"fanSeconds",1,86400,1000,&s.vacancy[1],false},
    {"radarTimeoutMs",500,30000,1,&s.radarTimeout,false},{"oledRefreshMs",250,5000,1,&s.oledRefresh,false},
    {"meterPollMs",1000,10000,1,&s.meterPoll,false},{"checkpointSeconds",60,3600,1000,&s.checkpoint,false},
    {"buttonHoldMs",2000,15000,1,&s.buttonHold,false},{"xMin",-6000,5999,1,&s.xMin,true},
    {"xMax",-5999,6000,1,&s.xMax,true},{"yMin",0,5999,1,&s.yMin,true},
    {"yMax",1,6000,1,&s.yMax,true},{"timezoneMinutes",-720,840,1,&s.timezoneMinutes,true}
  };
  for (auto &n:values) {
    if (!d[n.name].is<int32_t>()) return String("Integer required: ")+n.name;
    int32_t v=d[n.name]; if(v<n.min || v>n.max) return String("Out of range: ")+n.name;
    if(n.signedValue) *static_cast<int32_t*>(n.target)=v;
    else *static_cast<uint32_t*>(n.target)=(uint32_t)v*n.multiplier;
  }
  struct Text { const char *name; char *target; size_t capacity; bool secret; };
  Text texts[]={{"room",s.room,sizeof(s.room),false},{"ssid",s.ssid,sizeof(s.ssid),false},
    {"wifiPassword",s.wifiPassword,sizeof(s.wifiPassword),true},{"apName",s.apName,sizeof(s.apName),false},
    {"apPassword",s.apPassword,sizeof(s.apPassword),true},{"webUser",s.webUser,sizeof(s.webUser),false},
    {"webPassword",s.webPassword,sizeof(s.webPassword),true},{"repository",s.repository,sizeof(s.repository),false}};
  for(auto &t:texts) {
    if(t.secret && d[t.name].isNull()) continue;
    if(!d[t.name].is<const char*>()) return String("Text required: ")+t.name;
    const char *v=d[t.name]; if(strlen(v)>=t.capacity) return String("Too long: ")+t.name;
    if(t.secret && !v[0]) continue; // Blank password means keep existing.
    strcpy(t.target,v);
  }
  if(d["clearWifiPassword"]==true) s.wifiPassword[0]=0;
  if(!d["activeLow"].is<bool>() || !d["autoUpdate"].is<bool>()) return "Boolean settings required";
  s.activeLow=d["activeLow"]; s.autoUpdate=d["autoUpdate"];
  if(!d["pins"].is<JsonArray>() || d["pins"].size()!=9) return "Nine GPIO values required";
  for(int i=0;i<9;++i) { if(!d["pins"][i].is<int>()) return "Integer GPIO required"; s.pins[i]=d["pins"][i]; }
  return validateSettings(s);
}

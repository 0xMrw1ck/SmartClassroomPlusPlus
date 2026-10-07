#pragma once
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "OtaTrust.h"
#define FIRMWARE_VERSION "1.2.0"
#define FIRMWARE_BOARD "esp32s3usbotg-4mb"
String otaStatus="Waiting for startup check";
bool otaRequested=false, otaBootPending=true;
uint32_t otaWaitStarted=0;
inline bool newerVersion(const String &v) {
  unsigned a,b,c,x,y,z; char extra;
  if (sscanf(v.c_str(),"%u.%u.%u%c",&a,&b,&c,&extra)!=3) return false;
  if (sscanf(FIRMWARE_VERSION,"%u.%u.%u",&x,&y,&z)!=3) return false;
  return a>x || (a==x && (b>y || (b==y && c>z)));
}
inline void configureOTAHTTP(HTTPClient &h) {
  h.setConnectTimeout(5000); h.setTimeout(8000);
  h.useHTTP10(true); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  h.setRedirectLimit(5);
}
// Called only while both outputs are OFF. No insecure TLS fallback.
inline void checkGithubOTA() {
  if (WiFi.status()!=WL_CONNECTED || time(nullptr)<1704067200) { otaStatus="Skipped: internet/clock unavailable"; return; }
  otaStatus="Checking GitHub";
  NetworkClientSecure client; client.setCACert(OTA_ROOTS); client.setHandshakeTimeout(8);
  HTTPClient h; configureOTAHTTP(h);
  String base="https://github.com/"+String(settings.repository)+"/releases/";
  if (!h.begin(client,base+"latest/download/manifest.json")) { otaStatus="Manifest connection failed"; return; }
  int code=h.GET();
  if (code!=200 || h.getSize()<0 || h.getSize()>4096) { otaStatus=code==404?"No published release yet":"Manifest download failed"; h.end(); return; }
  String body=h.getString(); h.end(); JsonDocument d;
  if (body.length()>4096 || deserializeJson(d,body)) { otaStatus="Invalid manifest"; return; }
  String version=d["version"]|"",board=d["board"]|"",url=d["url"]|"",hash=d["sha256"]|"";
  if (!newerVersion(version)) { otaStatus="Current version (no update)"; return; }
  const esp_partition_t *slot=esp_ota_get_next_update_partition(nullptr);
  size_t size=d["size"]|0;
  bool hashOK=hash.length()==64;
  for (char ch:hash) if (!isxdigit(static_cast<unsigned char>(ch))) hashOK=false;
  if (board!=FIRMWARE_BOARD || !url.startsWith(base+"download/") || !url.endsWith("/firmware.bin") || !hashOK || !slot || !size || size>slot->size) { otaStatus="Rejected incompatible release"; return; }
  configureOTAHTTP(h);
  if (!h.begin(client,url) || h.GET()!=200 || h.getSize()!=static_cast<int>(size)) { otaStatus="Firmware download failed"; h.end(); return; }
  if (!Update.begin(size) || !Update.setSHA256(hash.c_str())) { Update.abort(); otaStatus="Update initialization failed"; h.end(); return; }
  otaStatus="Installing "+version;
  uint8_t buffer[1024]; size_t received=0; uint32_t lastByte=millis();
  auto *stream=h.getStreamPtr();
  while (received<size && (h.connected() || stream->available())) {
    int n=stream->available();
    if (n>0) {
      size_t count=stream->readBytes(buffer,min(sizeof(buffer),min(static_cast<size_t>(n),size-received)));
      if (!count || Update.write(buffer,count)!=count) break;
      received+=count; lastByte=millis();
    } else { if (millis()-lastByte>8000) break; delay(1); }
  }
  h.end();
  if (received!=size || !Update.end()) { Update.abort(); otaStatus="Update failed; existing firmware retained"; return; }
  otaStatus="Update installed; restarting"; Serial.println(otaStatus); delay(200); ESP.restart();
}

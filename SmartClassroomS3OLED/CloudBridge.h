#pragma once
#include <ArduinoJson.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "CloudTrust.h"
#include "CloudResponse.h"

struct CloudConfig {
  uint32_t schema=1, intervalSeconds=30;
  bool enabled=false;
  char apiKey[129]="", databaseURL[193]="";
};
CloudConfig cloudConfig;
QueueHandle_t cloudQueue=nullptr;
portMUX_TYPE cloudLock=portMUX_INITIALIZER_UNLOCKED;
struct CloudState { char status[96]="Disabled", uid[129]=""; uint32_t lastSuccess=0; };
CloudState cloudState;
uint32_t cloudLastQueued=0;

inline bool validCloudURL(const String &url) {
  if (!url.startsWith("https://") || url.length()>192) return false;
  String host=url.substring(8);
  if (!host.endsWith(".firebaseio.com") && !host.endsWith(".firebasedatabase.app")) return false;
  for (char c:host) if (!(isalnum(static_cast<unsigned char>(c)) || c=='-' || c=='.')) return false;
  return true;
}
inline String validateCloudConfig(const CloudConfig &c) {
  if (c.intervalSeconds<15 || c.intervalSeconds>300) return "Cloud interval must be 15..300 seconds";
  if (c.enabled && (!validCloudURL(c.databaseURL) || strlen(c.apiKey)<30)) return "Enter the Firebase API key and HTTPS database URL (without a trailing slash)";
  return "";
}
inline void cloudSetStatus(const String &status) {
  portENTER_CRITICAL(&cloudLock);
  strlcpy(cloudState.status,status.c_str(),sizeof(cloudState.status));
  portEXIT_CRITICAL(&cloudLock);
}
inline CloudState cloudSnapshot() {
  CloudState s; portENTER_CRITICAL(&cloudLock); s=cloudState; portEXIT_CRITICAL(&cloudLock); return s;
}
inline bool saveCloudConfig(const CloudConfig &c) {
  Preferences p; if(!p.begin("classcloudcfg",false))return false;
  bool ok=p.putBytes("config",&c,sizeof(c))==sizeof(c);p.end();return ok;
}
inline void loadCloudConfig() {
  Preferences p;
  if(p.begin("classcloudcfg",true)) {
    if(p.getBytesLength("config")==sizeof(cloudConfig)) {
      CloudConfig c;p.getBytes("config",&c,sizeof(c));c.apiKey[128]=0;c.databaseURL[192]=0;
      if(c.schema==1 && validateCloudConfig(c).isEmpty())cloudConfig=c;
    } p.end();
  }
  cloudSetStatus(cloudConfig.enabled?"Waiting for internet":"Disabled");
}
inline int cloudHTTP(const String &url,const char *method,const String &body,String &response,const char *type="application/json") {
  response="";
  NetworkClientSecure client;client.setCACert(CLOUD_ROOTS);client.setHandshakeTimeout(5);
  HTTPClient h;h.setConnectTimeout(4000);h.setTimeout(5000);h.useHTTP10(true);
  if(!h.begin(client,url))return -1;
  if(strcmp(method,"GET")!=0)h.addHeader("Content-Type",type);
  int code=strcmp(method,"GET")==0?h.GET():h.sendRequest(method,body);
  int length=h.getSize();
  if(code>0 && code!=204) {
    if(length>static_cast<int>(CloudResponse::limit)) { h.end(); return -1001; }
    CloudResponse sink;
    int received=h.writeToStream(&sink);
    if(sink.overflow) { h.end(); return -1001; }
    if(sink.allocationFailed) { h.end(); return -1002; }
    if(received<0) { h.end(); return received; }
    response=sink.body;
  }
  h.end();return code;
}
inline String cloudFormEscape(const String &s) {
  String out;const char *hex="0123456789ABCDEF";
  for(unsigned char c:s) { if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);else {out+='%';out+=hex[c>>4];out+=hex[c&15];} }return out;
}
// Network and token refresh run on their own task. Never touch OLED, relays or UARTs here.
inline void cloudWorker(void *) {
  String token,refresh,uid,registeredViewer;
  uint32_t tokenAt=0;
  Preferences identity;
  if(identity.begin("classcloudid",false)) {
    if(identity.getString("key")==cloudConfig.apiKey) {refresh=identity.getString("refresh");uid=identity.getString("uid");}
  }
  if(uid.length()) {portENTER_CRITICAL(&cloudLock);strlcpy(cloudState.uid,uid.c_str(),sizeof(cloudState.uid));portEXIT_CRITICAL(&cloudLock);}
  for(;;) {
    String *packet=nullptr;
    if(xQueueReceive(cloudQueue,&packet,portMAX_DELAY)!=pdTRUE || !packet)continue;
    String payload=*packet;delete packet;
    if(WiFi.status()!=WL_CONNECTED || time(nullptr)<1704067200) {cloudSetStatus("Waiting for Wi-Fi and clock");continue;}
    String response;
    if(token.isEmpty() || millis()-tokenAt>3300000UL) {
      int code;
      if(refresh.isEmpty()) {
        code=cloudHTTP("https://identitytoolkit.googleapis.com/v1/accounts:signUp?key="+String(cloudConfig.apiKey),"POST","{\"returnSecureToken\":true}",response);
      } else {
        code=cloudHTTP("https://securetoken.googleapis.com/v1/token?key="+String(cloudConfig.apiKey),"POST","grant_type=refresh_token&refresh_token="+cloudFormEscape(refresh),response,"application/x-www-form-urlencoded");
      }
      JsonDocument auth;
      if(code!=200) {cloudSetStatus("Firebase sign-in HTTP/transport error ("+String(code)+")");continue;}
      DeserializationError authError=deserializeJson(auth,response);
      if(authError) {
        cloudSetStatus("Firebase response invalid: "+String(authError.c_str()));
        Serial.printf("Cloud auth JSON error: %s; HTTP=%d; bytes=%u\n",authError.c_str(),code,(unsigned)response.length());
        continue;
      }
      String nextToken=auth["idToken"].is<const char*>() ? auth["idToken"].as<String>() : String(auth["id_token"] | "");
      String nextRefresh=auth["refreshToken"].is<const char*>() ? auth["refreshToken"].as<String>() : String(auth["refresh_token"] | "");
      String nextUID=auth["localId"].is<const char*>() ? auth["localId"].as<String>() : String(auth["user_id"] | "");
      if(nextToken.isEmpty() || nextRefresh.isEmpty() || nextUID.isEmpty()) {cloudSetStatus("Incomplete Firebase sign-in response");continue;}
      // Store refresh token before publishing: the device identity must survive reboot/OTA.
      if(!identity.isKey("key") || refresh!=nextRefresh || uid!=nextUID) {
        if(identity.putString("refresh",nextRefresh)!=nextRefresh.length() || identity.putString("uid",nextUID)!=nextUID.length() || identity.putString("key",cloudConfig.apiKey)!=strlen(cloudConfig.apiKey)) {cloudSetStatus("Cannot save cloud device identity");continue;}
      }
      token=nextToken;refresh=nextRefresh;uid=nextUID;tokenAt=millis();
      portENTER_CRITICAL(&cloudLock);strlcpy(cloudState.uid,uid.c_str(),sizeof(cloudState.uid));portEXIT_CRITICAL(&cloudLock);
      Serial.println("Cloud device ID: "+uid);
    }
    // Only devices enrolled by the project owner can publish. Anonymous identity alone grants no data access.
    int code=cloudHTTP(String(cloudConfig.databaseURL)+"/registry/"+uid+".json?auth="+token,"GET","",response);
    JsonDocument enrollment;
    if(code==401)token="";
    if(code!=200 || deserializeJson(enrollment,response) || !enrollment.is<const char*>()) {cloudSetStatus("Pair device ID on the online dashboard");continue;}
    registeredViewer=enrollment.as<String>();
    JsonDocument data;
    if(deserializeJson(data,payload)) {cloudSetStatus("Telemetry serialization failed");continue;}
    JsonDocument envelope;envelope["viewerUid"]=registeredViewer;envelope["updatedAt"][".sv"]="timestamp";
    envelope["telemetry"]=data.as<JsonObject>();String output;serializeJson(envelope,output);
    response="";
    code=cloudHTTP(String(cloudConfig.databaseURL)+"/devices/"+uid+".json?auth="+token+"&print=silent","PUT",output,response);
    if(code==200 || code==204) {
      portENTER_CRITICAL(&cloudLock);cloudState.lastSuccess=millis();portEXIT_CRITICAL(&cloudLock);cloudSetStatus("Publishing live data");
    } else {if(code==401)token="";cloudSetStatus("Cloud publish failed ("+String(code)+")");}
  }
}
inline void queueCloudTelemetry(const String &json) {
  if(!cloudConfig.enabled)return;
  if(!cloudQueue) {
    cloudQueue=xQueueCreate(1,sizeof(String*));
    if(!cloudQueue || xTaskCreate(cloudWorker,"classroom-cloud",10240,nullptr,1,nullptr)!=pdPASS) {
      if(cloudQueue)vQueueDelete(cloudQueue);cloudQueue=nullptr;cloudSetStatus("Cloud worker could not start");return;
    }
  }
  // One pending snapshot only. Slow internet never creates an accumulating backlog.
  String *previous=nullptr;
  if(xQueueReceive(cloudQueue,&previous,0)==pdTRUE)delete previous;
  String *packet=new String(json);
  if(xQueueSend(cloudQueue,&packet,0)!=pdTRUE)delete packet;
}

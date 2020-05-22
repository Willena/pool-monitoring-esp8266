#include "app.h"
#include "config.h"
#include "utils.h"
#include "webserver.h"


#ifndef STASSID
#define STASSID "freebox"
#define STAPSK  "220519972201200017022004AA"
#endif

//#include <GDBStub.h>
#include <TZ.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <coredecls.h>                  // settimeofday_cb()
#include <Schedule.h>
#include <PolledTimeout.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <sntp.h>                       // sntp_servermode_dhcp()

#define MYTZ TZ_Europe_Paris
#define RTC_UTC_TEST 1510592825 

// for testing purpose:
extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);

static bool isTimeSet = false;
static bool hasOTAStarted = false;
Webserver *httpServer;
App *app;
DynamicJsonDocument config(3072); //3k bytes for the JsonDocument

#define PTM(w) \
  Serial.print(" " #w "="); \
  Serial.print(tm->tm_##w);

tm* get_localtime(){
  time_t now = time(nullptr);
  return localtime(&now);
}

void printTm(const char* what, const tm* tm) {
  Serial.print(what);
  PTM(isdst); PTM(yday); PTM(wday);
  PTM(year);  PTM(mon);  PTM(mday);
  PTM(hour);  PTM(min);  PTM(sec);
}

void showTime() {
  timeval tv;
  timespec tp;
  time_t now;
  uint32_t now_ms, now_us;
  
  gettimeofday(&tv, nullptr);
  clock_gettime(0, &tp);
  now = time(nullptr);
  now_ms = millis();
  now_us = micros();

  Serial.println();
  printTm("localtime:", localtime(&now));
  Serial.println();
  printTm("gmtime:   ", gmtime(&now));
  Serial.println();

  // time from boot
  Serial.print("clock:     ");
  Serial.print((uint32_t)tp.tv_sec);
  Serial.print("s / ");
  Serial.print((uint32_t)tp.tv_nsec);
  Serial.println("ns");

  // time from boot
  Serial.print("millis:    ");
  Serial.println(now_ms);
  Serial.print("micros:    ");
  Serial.println(now_us);

  // EPOCH+tz+dst
  Serial.print("gtod:      ");
  Serial.print((uint32_t)tv.tv_sec);
  Serial.print("s / ");
  Serial.print((uint32_t)tv.tv_usec);
  Serial.println("us");

  // EPOCH+tz+dst
  Serial.print("time:      ");
  Serial.println((uint32_t)now);

  // timezone and demo in the future
  Serial.printf("timezone:  %s\n", getenv("TZ") ? : "(none)");

  // human readable
  Serial.print("ctime:     ");
  Serial.print(ctime(&now));
  Serial.println();
}

void wifi_connect(){
  // start network
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK); 
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void ntp_config(){
  time_t rtc = RTC_UTC_TEST;
  timeval tv = { rtc, 0 };
  settimeofday(&tv, nullptr);
  settimeofday_cb([](){
    isTimeSet = true;
    showTime();
    Serial.println("NTP Callback : Time updated !");
  });
  configTime(MYTZ, "pool.ntp.org");

  Serial.println("Wait for time");
  
  while (!isTimeSet){
    delay(500);
    Serial.print(".");
  }
  Serial.println();
}

void setUpOTA(){
    // No authentication by default
  ArduinoOTA.onStart([]() {
    hasOTAStarted = true;
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    hasOTAStarted = false;
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
    hasOTAStarted = false;
  });
  ArduinoOTA.begin();
}


void setup() {

  Serial.begin(115200);
  //gdbstub_init();
  
  // put your setup code here, to run once:
  wifi_connect();
  Serial.println("Wifi OK !");
  ntp_config();
  Serial.println("Time OK !");
  LittleFS.begin();

  if (!MDNS.begin("pool")) {             // Start the mDNS responder for esp8266.local
  Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");

  setUpOTA();
  
  //Init temperature 
  app = new App();
  httpServer = new Webserver(app, 80);
  httpServer->begin();
  
}

void loop() {
    MDNS.update();
    ArduinoOTA.handle();
    
    if (!hasOTAStarted){
      httpServer->handleClient();
      app->update();
    }
}

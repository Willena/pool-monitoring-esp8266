
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <flash_hal.h>
#include <FS.h>
#include "StreamString.h"

#include <functional>
#include <LittleFS.h>
#include "app.h"
#include "mini_prom_client.h"
#include "fileConstants.h"
#include <EspSaveCrash.h>

#define TEXT_PLAIN "text/plain"
#define FS_INIT_ERROR "FS INIT ERROR"
#define FILE_NOT_FOUND "FileNotFound"
#define fsName "LittleFS"


class Webserver : public ESP8266WebServer {
  public:
    Webserver( App* app_ptr, EspSaveCrash* ch, int port = 80) : ESP8266WebServer(port) {
      this->app = app_ptr;
      this->crashHandler = ch;
      //this->getServer().setServerKeyAndCert_P(rsakey, sizeof(rsakey), x509, sizeof(x509));
      fsOK = LittleFS.begin();
      Serial.println(fsOK ? F("Filesystem initialized.") : F("Filesystem init failed!"));

      on("/", HTTP_GET, std::bind(&Webserver::handleGetIndex, this));
      
      //Initialize routes
      on("/up", HTTP_GET, std::bind(&Webserver::handleGetUp, this));
      
      // FSBrowser Routes
      on("/status", HTTP_GET, std::bind(&Webserver::handleStatus, this));
      on("/list", HTTP_GET, std::bind(&Webserver::handleFileList, this));
      on("/edit", HTTP_GET, std::bind(&Webserver::handleGetEdit, this));
      on("/edit",  HTTP_PUT, std::bind(&Webserver::handleFileCreate, this));
      on("/edit",  HTTP_DELETE, std::bind(&Webserver::handleFileDelete, this));
      on("/edit",  HTTP_POST, std::bind(&Webserver::replyOK,this), std::bind(&Webserver::handleFileUpload, this));
      on("/api/prometheus", HTTP_GET, std::bind(&Webserver::handleGetStats, this));
      on("/state", HTTP_GET, std::bind(&Webserver::handleGetState, this));

      //on("/api/update", HTTP_GET, std::bind(&Webserver::handleGetUpdate, this)); //May be useless or for improved compatibility
      on("/api/update", HTTP_POST, std::bind(&Webserver::handlePostUpdate, this), std::bind(&Webserver::handlePostUpdateFile, this));

      on("/api/manual", HTTP_PUT, std::bind(&Webserver::handlePutManual, this));
      on("/api/manual", HTTP_DELETE, std::bind(&Webserver::handleDeleteManual, this));

      on("/api/status", HTTP_GET, std::bind(&Webserver::handleAPIGetStatus, this));
      on("/api/reboot", HTTP_POST, std::bind(&Webserver::handleAPIPostReboot, this));
      on("/api/help", HTTP_GET, std::bind(&Webserver::handleAPIGetHelp, this));

      on("/api/crash", HTTP_GET, std::bind(&Webserver::handleAPIGetCrash, this)); //Get crash report
      on("/api/crash", HTTP_DELETE, std::bind(&Webserver::handleAPIPutCrash, this)); // Clear crash report


      // on("/api/config/",



      //Default handler
      onNotFound(std::bind(&Webserver::routeNotFound, this));

      
    }
  private:

    String unsupportedFiles = String();
    File uploadFile;
    bool fsOK = false;
    App * app;
    String _updaterError;
    EspSaveCrash * crashHandler;


    void _setUpdaterError()
    {
      Update.printError(Serial);
      StreamString str;
      Update.printError(str);
      _updaterError = str.c_str();
    }


    //Routes
    void handleGetUp() {
//      digitalWrite(led, 1);
      char temp[400];
      int sec = millis() / 1000;
      int min = sec / 60;
      int hr = min / 60;

      snprintf(temp, 400,
       "<html>\
          <head>\
            <meta http-equiv='refresh' content='5'/>\
            <title>ESP8266 Demo</title>\
            <style>\
              body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
            </style>\
          </head>\
          <body>\
            <h1>Hello from ESP8266!</h1>\
            <p>Uptime: %02d:%02d:%02d</p>\
            <img src=\"/test.svg\" />\
          </body>\
        </html>",
         hr, min % 60, sec % 60
         );
      this->send(200, "text/html", temp);
//      digitalWrite(led, 0);
    }
      
    void routeNotFound(){
      if (!fsOK) {
          return replyServerError(FPSTR(FS_INIT_ERROR));
        }
      
        String uri = ESP8266WebServer::urlDecode(this->uri()); // required to read paths with blanks
      
        if (handleFileRead(uri)) {
          return;
        }
      
        // Dump debug data
        String message;
        message.reserve(100);
        message = F("Error: File not found\n\nURI: ");
        message += uri;
        message += F("\nMethod: ");
        message += (this->method() == HTTP_GET) ? "GET" : "POST";
        message += F("\nArguments: ");
        message += this->args();
        message += '\n';
        for (uint8_t i = 0; i < this->args(); i++) {
          message += F(" NAME:");
          message += this->argName(i);
          message += F("\n VALUE:");
          message += this->arg(i);
          message += '\n';
        }
        message += "path=";
        message += this->arg("path");
        message += '\n';
        Serial.print(message);
      
        return replyNotFound(message);
    }

    void replyOK() {
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->send(200, FPSTR(TEXT_PLAIN), "");
    }
    
    void replyOKWithMsg(String msg) {
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->send(200, FPSTR(TEXT_PLAIN), msg);
    }

    void replyOKWithJson(String json){
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->send(200, "application/json", json);
    }
    
    void replyNotFound(String msg) {
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->send(404, FPSTR(TEXT_PLAIN), msg);
    }
    
    void replyBadRequest(String msg) {
      Serial.println(msg);
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->send(400, FPSTR(TEXT_PLAIN), msg + "\r\n");
    }
    
    void replyServerError(String msg) {
      Serial.println(msg);
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->send(500, FPSTR(TEXT_PLAIN), msg + "\r\n");
    }

    void handleStatus() {
      Serial.println("handleStatus");
      FSInfo fs_info;
      String json;
      json.reserve(128);
    
      json = "{\"type\":\"";
      json += fsName;
      json += "\", \"isOk\":";
      if (fsOK) {
        LittleFS.info(fs_info);
        json += F("\"true\", \"totalBytes\":\"");
        json += fs_info.totalBytes;
        json += F("\", \"usedBytes\":\"");
        json += fs_info.usedBytes;
        json += "\"";
      } else {
        json += "\"false\"";
      }
      json += F(",\"unsupportedFiles\":\"");
      json += unsupportedFiles;
      json += "\"}";
    
      replyOKWithJson(json);
    }

    void handleFileList() {
      if (!fsOK) {
        return replyServerError(FPSTR(FS_INIT_ERROR));
      }
    
      if (!this->hasArg("dir")) {
        return replyBadRequest(F("DIR ARG MISSING"));
      }
    
      String path = this->arg("dir");
      if (path != "/" && !LittleFS.exists(path)) {
        return replyBadRequest("BAD PATH");
      }
    
      Serial.println(String("handleFileList: ") + path);
      Dir dir = LittleFS.openDir(path);
      path.clear();    
    
      // use HTTP/1.1 Chunked response to avoid building a huge temporary string
      if (!this->chunkedResponseModeStart(200, "application/json")) {
        this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
        this->send(505, F("text/html"), F("HTTP1.1 required"));
        return;
      }
    
      // use the same string for every line
      String output;
      output.reserve(64);
      while (dir.next()) {
        if (output.length()) {
          // send string from previous iteration
          // as an HTTP chunk
          this->sendContent(output);
          output = ',';
        } else {
          output = '[';
        }
    
        output += "{\"type\":\"";
        if (dir.isDirectory()) {
          output += "dir";
        } else {
          output += F("file\",\"size\":\"");
          output += dir.fileSize();
        }
    
        output += F("\",\"name\":\"");
        // Always return names without leading "/"
        if (dir.fileName()[0] == '/') {
          output += &(dir.fileName()[1]);
        } else {
          output += dir.fileName();
        }
    
        output += "\"}";
      }
    
      // send last string
      output += "]";
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->sendContent(output);
      this->chunkedResponseFinalize();
    }

    bool handleFileRead(String path) {
      Serial.println(String("handleFileRead: ") + path);
      if (!fsOK) {
        replyServerError(FPSTR(FS_INIT_ERROR));
        return true;
      }
    
      if (path.endsWith("/")) {
        path += "index.htm";
      }

      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      String contentType;
      if (this->hasArg("download")) {
        contentType = F("application/octet-stream");
      } else {
        contentType = mime::getContentType(path);
      }
    
      if (!LittleFS.exists(path)) {
        // File not found, try gzip version
        path = path + ".gz";
      }
      if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        if (this->streamFile(file, contentType) != file.size()) {
          Serial.println("Sent less data than expected!");
        }
        file.close();
        return true;
      }
    
      return false;
    }

    String lastExistingParent(String path) {
      while (!path.isEmpty() && !LittleFS.exists(path)) {
        if (path.lastIndexOf('/') > 0) {
          path = path.substring(0, path.lastIndexOf('/'));
        } else {
          path = String();  // No slash => the top folder does not exist
        }
      }
      Serial.println(String("Last existing parent: ") + path);
      return path;
    }
    
  void handleFileCreate() {
    if (!fsOK) {
      return replyServerError(FPSTR(FS_INIT_ERROR));
    }
  
    String path = this->arg("path");
    if (path.isEmpty()) {
      return replyBadRequest(F("PATH ARG MISSING"));
    }
  
    if (path == "/") {
      return replyBadRequest("BAD PATH");
    }
    if (LittleFS.exists(path)) {
      return replyBadRequest(F("PATH FILE EXISTS"));
    }
  
    String src = this->arg("src");
    if (src.isEmpty()) {
      // No source specified: creation
      Serial.println(String("handleFileCreate: ") + path);
      if (path.endsWith("/")) {
        // Create a folder
        path.remove(path.length() - 1);
        if (!LittleFS.mkdir(path)) {
          return replyServerError(F("MKDIR FAILED"));
        }
      } else {
        // Create a file
        File file = LittleFS.open(path, "w");
        if (file) {
          file.write((const char *)0);
          file.close();
        } else {
          return replyServerError(F("CREATE FAILED"));
        }
      }
      if (path.lastIndexOf('/') > -1) {
        path = path.substring(0, path.lastIndexOf('/'));
      }
      replyOKWithMsg(path);
    } else {
      // Source specified: rename
      if (src == "/") {
        return replyBadRequest("BAD SRC");
      }
      if (!LittleFS.exists(src)) {
        return replyBadRequest(F("SRC FILE NOT FOUND"));
      }
  
      Serial.println(String("handleFileCreate: ") + path + " from " + src);
  
      if (path.endsWith("/")) {
        path.remove(path.length() - 1);
      }
      if (src.endsWith("/")) {
        src.remove(src.length() - 1);
      }
      if (!LittleFS.rename(src, path)) {
        return replyServerError(F("RENAME FAILED"));
      }
      replyOKWithMsg(lastExistingParent(src));
    }
  }
  
  void deleteRecursive(String path) {
    File file = LittleFS.open(path, "r");
    bool isDir = file.isDirectory();
    file.close();
  
    // If it's a plain file, delete it
    if (!isDir) {
      LittleFS.remove(path);
      return;
    }
  
    // Otherwise delete its contents first
    Dir dir = LittleFS.openDir(path);
  
    while (dir.next()) {
      deleteRecursive(path + '/' + dir.fileName());
    }
  
    // Then delete the folder itself
    LittleFS.rmdir(path);
  }

  void handleFileDelete() {
    if (!fsOK) {
      return replyServerError(FPSTR(FS_INIT_ERROR));
    }
  
    String path = this->arg(0);
    if (path.isEmpty() || path == "/") {
      return replyBadRequest("BAD PATH");
    }
  
    Serial.println(String("handleFileDelete: ") + path);
    if (!LittleFS.exists(path)) {
      return replyNotFound(FPSTR(FILE_NOT_FOUND));
    }
    deleteRecursive(path);
  
    replyOKWithMsg(lastExistingParent(path));
  }
  
  void handleFileUpload() {
    if (!fsOK) {
      return replyServerError(FPSTR(FS_INIT_ERROR));
    }
    if (this->uri() != "/edit") {
      return;
    }
    HTTPUpload& upload = this->upload();
    if (upload.status == UPLOAD_FILE_START) {
      String filename = upload.filename;
      // Make sure paths always start with "/"
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }
      Serial.println(String("handleFileUpload Name: ") + filename);
      uploadFile = LittleFS.open(filename, "w");
      if (!uploadFile) {
        return replyServerError(F("CREATE FAILED"));
      }
      Serial.println(String("Upload: START, filename: ") + filename);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (uploadFile) {
        size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
        if (bytesWritten != upload.currentSize) {
          return replyServerError(F("WRITE FAILED"));
        }
      }
      Serial.println(String("Upload: WRITE, Bytes: ") + upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (uploadFile) {
        uploadFile.close();
      }
      Serial.println(String("Upload: END, Size: ") + upload.totalSize);
    }
  }

  void handleGetEdit() {
    if (handleFileRead(F("/edit/index.htm"))) {
      return;
    }
  
    #ifdef INCLUDE_FALLBACK_INDEX_HTM
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->sendHeader(F("Content-Encoding"), "gzip");
      this->send(200, "text/html", index_htm_gz, index_htm_gz_len);
    #else
      replyNotFound(FPSTR(FILE_NOT_FOUND));
    #endif
  
  }

  void handleGetIndex() {
    if (handleFileRead(F("/index.html"))) {
      return;
    }
  
    #ifdef INCLUDE_FALLBACK_INDEX_HTM
      this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
      this->sendHeader(F("Content-Encoding"), "gzip");
      this->send(200, "text/html", index_htm_gz, index_htm_gz_len);
    #else
      replyNotFound(FPSTR(FILE_NOT_FOUND));
    #endif
  
  }
  
  void handleGetStats(){
      String message;

      MiniPromClient client;

      client.put("pool_temperature", "unit=\"C\"", String(this->app->getStatus()->rtlTemp));
      client.put("pool_pump_status", String(this->app->getStatus()->isPumpActivated?1:0));
      client.put("pool_is_manual", String(this->app->getStatus()->isManual?1:0));
      client.put("pool_ph_level", String(this->app->getStatus()->pHLevel));
      client.put("pool_ph_raw", String(this->app->getStatus()->pHRaw));
      client.put("pool_OrpClBr_level", String(this->app->getStatus()->ORP_CL_BR));
      client.put("pool_OrpClBr_raw", String(this->app->getStatus()->ORPRaw));
      client.put("pool_manual_remaining_time", String(this->app->getRemainingManualTime()));
      client.put("pool_ambiant_temperature", String(this->app->getStatus()->ambiantTemp));
      client.put("pool_water_level", String(this->app->getStatus()->waterLevel));

      //Add more metrics in the future

      replyOKWithMsg(client.getMessage());
  };

  void handleGetState(){
    String message;
    time_t now = time(NULL);

    message += "Current Time is : " + String(ctime(&now)) + "\n" ;
    message += "Temperature " + String(this->app->getStatus()->currentTemp) + "\n";
    message += "Time table :\n";
    int i=1;
    for (TableObject o : this->app->getStatus()->timetable) {
      message += String(i) + ". " + o.on + " - " + o.off + "\n";
      i++;
    }

    replyOKWithMsg(message);
  };

  void handlePutManual(){
    StaticJsonDocument<200> jsonbuffer;

    Serial.println("Body of manual put");
    Serial.println(this->arg("plain"));
    
    deserializeJson(jsonbuffer, this->arg("plain"));
    JsonObject obj = jsonbuffer.as<JsonObject>();
    
    bool on = true;
    if (obj.containsKey("on")){
      on = obj["on"];
    }
    
    
    if (obj.containsKey("duration")){
      unsigned int duration = obj["duration"];
      this->app->enableManualPump(duration, on);
    }
    else{
      this->app->enableManualPump(on);
    }

    replyOK();
  };

  void handleDeleteManual(){
    this->app->disableManualPump();
    replyOK();
  };

//  void handleGetUpdate(){
//    //No authentication...
//    this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
//    this->send_P(200, PSTR("text/html"), serverIndex);
//  }
//  
  void handlePostUpdateFile(){
      HTTPUpload& upload = this->upload();

      if(upload.status == UPLOAD_FILE_START){
        _updaterError.clear();
        
        Serial.setDebugOutput(true);

        WiFiUDP::stopAll();
        
        Serial.printf("Update: %s\n", upload.filename.c_str());
        
        if (upload.name == "filesystem") {
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          close_all_fs();
          if (!Update.begin(fsSize, U_FS)){//start with max available size
            Update.printError(Serial);
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
          }
        }
      } else if(upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        Serial.printf(".");
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          _setUpdaterError();
        }
      } else if(upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        if(Update.end(true)){ //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          _setUpdaterError();
        }
        Serial.setDebugOutput(false);
      } else if(upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
        Serial.println("Update was aborted");
      }
      delay(0);
  }

  void handlePostUpdate(){
    if (Update.hasError()) {
       this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
       this->send(200, F("text/html"), String(F("Update error: ")) + _updaterError);
      } else {
        this->client().setNoDelay(true);
        this->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
        this->send_P(200, PSTR("text/html"), successResponse);
        delay(100);
        this->client().stop();
        
        ESP.restart();
      }
  }

  void handleAPIGetCrash(){
    char crashbuf[2048];
    this->crashHandler->crashToBuffer(crashbuf);
    replyOKWithMsg(String(crashbuf));
  }

  void handleAPIPutCrash(){
    this->crashHandler->clear();
    replyOK();
  }

  void handleAPIGetStatus(){
    Serial.printf("Heap is %d ", ESP.getFreeHeap());
    DynamicJsonDocument jsonbuffer(1024);
    
    String jsonMessage;
    State* state = this->app->getStatus();
    
    jsonbuffer["isManual"] = state->isManual;
    jsonbuffer["remainingManualTime"] = state->isManual? this->app->getRemainingManualTime() : 0;
    jsonbuffer["currentTimestamp"] = time(NULL);
    jsonbuffer["lastTableUpdate"] = state->lastTableUpdate;
    jsonbuffer["temperature"] = state->currentTemp;
    jsonbuffer["rtlTemperature"] = state->rtlTemp;
    jsonbuffer["isPumpActivated"] = state->isPumpActivated;
    jsonbuffer["phLevel"] = state->pHLevel;
    jsonbuffer["phRaw"] = state->pHRaw;
    jsonbuffer["orpRaw"] = state->ORPRaw;
    jsonbuffer["OrpClBrLevel"] = state->ORP_CL_BR;
    jsonbuffer["ambiantTemperature"] = state->ambiantTemp;
    jsonbuffer["waterLevel"] = state->waterLevel;
    jsonbuffer["version"] = POOL_FW_VERSION;
    jsonbuffer["uptime"] = millis() / 1000;
    
    JsonArray timetableArray = jsonbuffer.createNestedArray("currentTimetable");
    for (TableObject o : state->timetable) {
      JsonObject arrayElement = timetableArray.createNestedObject();
      arrayElement["on"] = o.on;
      arrayElement["off"] = o.off;    
    }

    JsonObject seasonObject = jsonbuffer.createNestedObject("currentSeason");
    JsonArray tableArray = seasonObject.createNestedArray("table");
    for (TableObject o : this->app->getSeason()->table) {
      JsonObject arrayElement = tableArray.createNestedObject();
      arrayElement["on"] = o.on;
      arrayElement["off"] = o.off;    
    }

    JsonArray monthsArray = seasonObject.createNestedArray("months");
    for (unsigned int i : this->app->getSeason()->months) {
      monthsArray.add(i);
    }
        
    seasonObject["name"] = this->app->getSeason()->name;

    serializeJson(jsonbuffer, jsonMessage);
    replyOKWithJson( jsonMessage);
  }

  void handleAPIGetHelp(){
     replyOKWithJson("{}");
  }

  void handleAPIPostReboot(){
    replyOK();
    delay(200);
    //Ask a reboot.
    ESP.restart();
  }
};


#endif

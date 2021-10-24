
#ifndef CONFIG_H
#define CONFIG_H


#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

#define POOL_FW_VERSION __DATE__
#define GPIO_RELAY 4 //D2
#define GPIO_DS18B20 5 //D1
#define PASSWORD_OTA "password"
#define GPIO_PRESSURE A0
#define ADC_MAX_STEPS 1023
#define PWR_VLT 3.33
#define PRESSURE_MIN 0
#define PRESSURE_MAX 100
#define PSI_BAR_UNIT 0.0689

// uint8_t pin5[8] = {0x28, 0x2F, 0xD7, 0x79, 0xA2, 0x01, 0x03, 0x27}; Address of DS1820B


#define FILENAME_LEN 64


class ConfigurationFactory {
    public: 
        static bool loadConfig(String filename, DynamicJsonDocument * doc){
            if (!LittleFS.exists(filename)){
                Serial.println("LoadConfig : Failed file does not exists");
                Serial.println("'"+filename+"'");
                return false;
            }
              
            File file = LittleFS.open(filename, "r");
            deserializeJson(*doc, file);
            file.close();
            return true;
        };

        static void writeConfig(String filename, JsonDocument &doc ){
            File file = LittleFS.open(filename, "w+");
            serializeJson(doc, file);
            file.close();
        };
        
        static String getDefault(){
            char fileName[FILENAME_LEN];
            File configFile = LittleFS.open("/config/default.txt", "r");
            int n = configFile.readBytes(fileName, FILENAME_LEN);
            configFile.close();
            fileName[n] = '\0';
            return String(fileName);
        };
        
        static void setDefault(String filename){
            File configFile = LittleFS.open("/config/default.txt", "w+");
            configFile.print(filename);
            configFile.close();
        };
};

#endif

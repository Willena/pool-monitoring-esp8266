#ifndef TIMETABLE_MGT
#define TIMETABLE_MGT

#include "utils.h"
#include "config.h"
#include "timer.h"
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include <string>
#include <time.h>   
#include <OneWire.h>
#include <DallasTemperature.h>


typedef struct {
  char on[6]; // 00:00 => 6char 
  char off[6];
} TableObject;

typedef struct {
    float minT;
    float maxT;
    unsigned int splits;
    unsigned long duration;
    std::vector<TableObject> table;
} TemperatureObject;

typedef struct {
    char name[20];
    std::vector<unsigned int> months;
    std::vector<TableObject> table;
} SeasonObject;

typedef struct {
  float currentTemp;
  float pHLevel;
  float ORP_CL_BR;
  bool isPumpActivated;
  std::vector<TableObject> timetable;
  bool isManual;
} State;

class App{

    private:
        OneWire *oneWire;
        DallasTemperature *sensors;
        State state;
        Timer *pumpUpdateTimer;
        Timer *timeTableUpdate;
        DynamicJsonDocument * doc;
        TemperatureObject currentTemperatureSlot;
        SeasonObject currentSeasonSlot;
        std::vector<TemperatureObject> temperatureTable;
        std::vector<SeasonObject> seasonTable;
        bool initialized = false;
        Timer *watchDogTimer;
        Timer *manualActivationTimer;
        

        void readTemperatureAndSeasonsTable(JsonObject &root){
            Serial.println("Reading Temperatures from Json");
            JsonArray array = root["timetable"];
            
            Serial.print("Size of JsonArray");
            Serial.println(array.size());

            for (JsonObject kv : array) {
                Serial.println("1. Element of TimeTable");
                
                TemperatureObject temperatureObject;
                temperatureObject.minT = kv["minT"];
                temperatureObject.maxT = kv["maxT"];
                temperatureObject.splits = kv["splits"];
                temperatureObject.duration = kv["duration"];
                
                Serial.print(temperatureObject.minT);
                Serial.print(" - ");
                Serial.println(temperatureObject.maxT);
                
                JsonArray tableArray = kv["table"];
                
                temperatureObject.table.clear();
                for (JsonObject vv : tableArray){
                  
                  TableObject tableObject;
                  strlcpy(tableObject.on, vv["on"], sizeof(tableObject.on));
                  strlcpy(tableObject.off, vv["off"], sizeof(tableObject.off));  
                  
                  temperatureObject.table.push_back(tableObject);
                }

                Serial.print("Done adding Temperature slot : ");
                Serial.println(temperatureTable.size());
                
                temperatureTable.push_back(temperatureObject);

            }

            Serial.println("Done Reading Temperatures from Json");

            Serial.println("Reading Seasons from Json");
            JsonArray objects = root["whitehours"];
            
            for (JsonObject kv : objects) {
  
                SeasonObject seasonObject;
                strlcpy( seasonObject.name, kv["name"], sizeof(seasonObject.name));
                seasonObject.months.clear();

                JsonArray months = kv["months"];

                for(unsigned int v : months) {
                  seasonObject.months.push_back(v);
                }
                 

                if (kv["table"].is<JsonArray>()) //In case table becom an array in the config
                {
                  JsonArray tableArray = kv["table"];
                  Serial.println("Adding elements");
                  for (JsonObject vv : tableArray){
                                       
                    TableObject tableObject;
                    strlcpy(tableObject.on, vv["on"], sizeof(tableObject.on));
                    strlcpy(tableObject.off, vv["off"], sizeof(tableObject.off));                    
                    seasonObject.table.push_back(tableObject);
                  }
                }
                else
                {
                  //Not a table .. It is an object
                  JsonObject vv = kv["table"];

                  TableObject tableObject;
                  strlcpy(tableObject.on, vv["on"], sizeof(tableObject.on));
                  strlcpy(tableObject.off, vv["off"], sizeof(tableObject.off));  
                  
                  seasonObject.table.push_back(tableObject);
                }
               
                Serial.print("Done Converting seasonJson : ");
                Serial.println(seasonObject.table.size());

                seasonTable.push_back(seasonObject);
                
            }

            Serial.println("Done Reading Seasons from Json");


        };
        
    public:
        App(){

            this->doc = new DynamicJsonDocument(3072);
            if(!ConfigurationFactory::loadConfig(ConfigurationFactory::getDefault(), this->doc)){
              Serial.println("Could not read Configuration file");
              return;
            }
            else{
              Serial.println("Read back the configuration loaded");
              serializeJsonPretty(*(this->doc), Serial);
            }


            JsonObject root = this->doc->as<JsonObject>();
            this->readTemperatureAndSeasonsTable(root);
            
            pinMode(GPIO_RELAY, OUTPUT);
            digitalWrite(GPIO_RELAY, LOW);

            this->oneWire = new OneWire(GPIO_DS18B20);
            this->sensors = new DallasTemperature(oneWire);
            this->sensors->begin();
            
            //Start timers
            this->pumpUpdateTimer =new Timer(Timer::getIntervalFromUnit(1, UNIT_MIN), LOOP_UNTIL_STOP);
            this->pumpUpdateTimer->start();
            
            this->timeTableUpdate = new Timer(Timer::getIntervalFromUnit(1, UNIT_H), LOOP_UNTIL_STOP);
            this->timeTableUpdate->start();

            //initialize Manual timmers
            this->manualActivationTimer = new Timer(Timer::getIntervalFromUnit(10, UNIT_D), SINGLE_SHOT);
            this->watchDogTimer = new Timer(Timer::getIntervalFromUnit(10, UNIT_D), SINGLE_SHOT);


            
            this->initialized = true;
        };

        State* getStatus(){
          return &(this->state);
        }
        
        bool getCurrentTemperatureSlot(){
            Serial.print("Getting temperature slot for value ");
            Serial.println(this->state.currentTemp);
            
            for (TemperatureObject v : temperatureTable) {
                if (this->state.currentTemp >= v.minT && this->state.currentTemp < v.maxT){
                    Serial.print("Found ! ");
                    Serial.print(v.minT);
                    Serial.print(" - ");
                    Serial.println(v.maxT);
                    this->currentTemperatureSlot = v;
                    return true;
                }
            }
            return false;

        };

        bool getCurrentSeason(){
            unsigned int month = get_localtime()->tm_mon;
            Serial.print("Finding season for current month ");
            Serial.println(month);
            for (SeasonObject kv : seasonTable) {
                if (has_value(kv.months, month)){
                    Serial.print("Found ");
                    Serial.println(kv.name);
                    this->currentSeasonSlot = kv;
                    return true;
                }
            }
            return false;
        };

        unsigned long computeAvailableSeasonTime(TableObject &t){
            Serial.println("Computing available time between ");
            Serial.print(t.on);
            Serial.print(" - ");
            Serial.println(t.off);
            
            unsigned long onTime = timeToSecFromString(t.on);
            unsigned long offTime = timeToSecFromString(t.off);
            return offTime - onTime;
        };

        void printTimeTable(){
            Serial.println("Printing current Time table");
            unsigned int i = 1;
            for ( auto const &kv : this->state.timetable) {
                char string[30];
                sprintf(string, "%d. %s-%s", i, kv.on , kv.off);
                Serial.println(string);
                i++;
            }
        };

        void generateTable(){
            Serial.println("Generating a new Time table !");
            Serial.print("TT_Gen Size table season ");
            Serial.println(this->currentSeasonSlot.table.size());
            Serial.println(this->currentTemperatureSlot.duration);
            Serial.println(this->currentTemperatureSlot.splits);
            
            this->state.timetable.clear();        
            if ((this->currentTemperatureSlot.duration == 0 || this->currentTemperatureSlot.splits == 0) && !this->currentTemperatureSlot.table.empty()){
                Serial.println("TT_Gen: Using Table");
                this->state.timetable.insert(this->state.timetable.end(), this->currentTemperatureSlot.table.begin(),this->currentTemperatureSlot.table.end());
                return;
            }

            Serial.println("TT_Gen Gen table ");
            if (this->currentTemperatureSlot.duration >= DAY_H * HOUR_MIN * MIN_S){
                Serial.println("TT_Gen: Above 24H slot..");
                this->currentTemperatureSlot.duration = DAY_H * HOUR_MIN * MIN_S;
            }

            Serial.println("TT_Gen 24h checked ");
            bool is24h = false;

            
            
            unsigned long availableSeconds = computeAvailableSeasonTime(this->currentSeasonSlot.table.at(0));
            Serial.print("TT_Gen: Have");
            Serial.println(availableSeconds);
            
            if (this->currentTemperatureSlot.duration >= availableSeconds) {
                Serial.println("Too much ours to place.");
                Serial.println("Switching to 24h band");
                availableSeconds = DAY_H * HOUR_MIN * MIN_S;
                is24h = true;
            }

            Serial.println("TT_Gen: Compute ");
            unsigned long splitedAvailableTime = availableSeconds / currentTemperatureSlot.splits;
            unsigned long splitedAvailableTimeCenter = splitedAvailableTime / 2;

            unsigned long splitsSlotDuration = currentTemperatureSlot.duration / currentTemperatureSlot.splits;
            unsigned long slotHalfDuration = splitsSlotDuration / 2;
            
            unsigned long startShift = 0;
            if (!is24h) {
                startShift = timeToSecFromString(this->currentSeasonSlot.table.at(0).on);
                Serial.print("TT_Gen: Start1 ");
                Serial.println(startShift);
            }

            Serial.println("TT_Gen: Building with iterations ");
            this->state.timetable.clear();
            for (unsigned int i = 0; i< this->currentTemperatureSlot.splits; i++){
                unsigned long startTime = i*splitedAvailableTime + startShift + splitedAvailableTimeCenter - slotHalfDuration;
                unsigned long endTime = startTime + splitsSlotDuration;

                TableObject o;
                
                secToTimeString(startTime, o.on);
                secToTimeString(endTime, o.off );

                this->state.timetable.push_back(o);
    
            }

        };

        void getTemp(){
            Serial.print("Requesting temperatures...");
            this->sensors->requestTemperatures(); // Send the command to get temperatures
            Serial.println("DONE");
            float tempC = this->sensors->getTempCByIndex(0);

            // Check if reading was successful
            if(tempC != DEVICE_DISCONNECTED_C) 
            {
                Serial.print("Temperature for the device 1 (index 0) is: ");
                Serial.println(tempC);
                this->state.currentTemp = tempC;

                Serial.print("Set ");
                Serial.println(this->state.currentTemp);
            } 
            else
            {
                Serial.println("Error: Could not read temperature data");
            }
        };

        void onTimeTableUpdateFired(){
            Serial.println("Updating timetable...");
            this->getTemp();
            if (!getCurrentTemperatureSlot()){
              Serial.println("Could not find temperature slot");
              return;
            }
            
            if (!getCurrentSeason()){
              Serial.println("Could not find a season");
              return;
            }
            
            this->generateTable();
            this->printTimeTable();
            this->onCheckPumpForUpdate();
            Serial.println("Table fully updated ! ");
        };

        bool isInTimeTable(unsigned int hour, unsigned int minutes){
            unsigned long currentSec =  MIN_S * minutes + hour * HOUR_MIN * MIN_S;

            for (TableObject o : this->state.timetable) {
                unsigned long onSec = timeToSecFromString(o.on);
                unsigned long offSec = timeToSecFromString(o.off);
                
                if (currentSec >= onSec && currentSec <= offSec){
                    Serial.print("In ");
                    Serial.print(o.on);
                    Serial.print(" - ");
                    Serial.print(o.off);
                    Serial.print(" Time  ");
                    Serial.print(hour);
                    Serial.print(" - ");
                    Serial.print(minutes);
                    Serial.print(" - ");
                    Serial.println(currentSec);
                    return true;
                }
            }
            return false;
        };

        void setPumpOn(){
            if (this->state.isPumpActivated)
              return;
            this->state.isPumpActivated = true;
            Serial.println("Switching pump on");
            digitalWrite(GPIO_RELAY, HIGH);   
        };

        void setPumpOff(){
            if (!this->state.isPumpActivated)
              return;

            this->state.isPumpActivated = false;
            Serial.println("Switching pump off");
            digitalWrite(GPIO_RELAY, LOW);
        };

        void onCheckPumpForUpdate(){
            Serial.println("Checking pump status....");
            tm *completeTime = get_localtime();
            time_t now = time(NULL);
            Serial.print("Current Time is : ");
            Serial.println(ctime(&now));

            if (this->isInTimeTable(completeTime->tm_hour, completeTime->tm_min)){
                setPumpOn();
            }
            else{
                setPumpOff();
            }
            
            Serial.println("Pump checked !");
        };


        void enableManualPump(unsigned long duration_s, bool on){
            Serial.println("Enabling watchdog manual");

            this->manualActivationTimer->pause();
            this->manualActivationTimer->setInterval(Timer::getIntervalFromUnit(duration_s, UNIT_S));
            this->manualActivationTimer->start(true);
            Serial.println("Updated timer manual");
            
            enableManualPump(on);

        }

        void enableManualPump(bool on){

            Serial.println("Enabling watchdog manual");

            this->watchDogTimer->pause();
            this->watchDogTimer->setInterval(Timer::getIntervalFromUnit(10, UNIT_D));
            this->watchDogTimer->start(true);
            
            this->state.isManual = true;
            Serial.println("Watchdog created");
            
            if (on)
              setPumpOn();
            else
              setPumpOff();
        }

        unsigned long getRemainingManualTime(){
          if (!this->state.isManual)
            return 0;

          unsigned long watchDogRemaining = this->watchDogTimer->remainingTime();
          unsigned long manualActivationTimerRemain = this->manualActivationTimer->remainingTime();
          

          return this->manualActivationTimer->paused() ? watchDogRemaining : ((watchDogRemaining > manualActivationTimerRemain)? manualActivationTimerRemain : watchDogRemaining);
          
        }

        void disableManualPump(){
         if(!this->state.isManual)
         {
          Serial.println("not in manual mode");
         }
         Serial.println("Disabling Manual mode");
          
         this->watchDogTimer->pause();
         this->manualActivationTimer->pause();
          

         Serial.println("Changing pump status");
         setPumpOff();
          
         this->state.isManual = false;
         onTimeTableUpdateFired();
        }

        SeasonObject  * getSeason(){
          return &(this->currentSeasonSlot);
        }

        void update(){
            if (!this->initialized)
              return;

            unsigned long time_sec = time(NULL);
            
            if (this->state.isManual){

              //If watchDog wakes up or manual is expired !
              if (watchDogTimer->update(time_sec) || manualActivationTimer->update(time_sec)){
                disableManualPump();
              }
              
            }else{
              if (pumpUpdateTimer->update(time_sec))
                onCheckPumpForUpdate();
            
              if (timeTableUpdate->update(time_sec))
                onTimeTableUpdateFired();  
            }
              
            
        }

};

#endif

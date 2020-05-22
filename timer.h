#ifndef TIMER_H
#define TIMER_H

#define SINGLE_SHOT 1
#define LOOP_UNTIL_STOP 2

#define UNIT_MS 0
#define UNIT_S 1
#define UNIT_MIN 2
#define UNIT_H 3
#define UNIT_D 4

#define DAY_H 24 //Number of hour in a day
#define MIN_S 60 //Number of second in a minute
#define HOUR_MIN 60 //Number of minites in an hour

#define S_MS 1000
#define MIN_MS (MIN_S * S_MS)
#define HOUR_MS (HOUR_MIN * MIN_MS)
#define DAY_MS (DAY_H * HOUR_MS)

#include <functional>
#include <time.h>

class Timer {
    public:
        Timer(unsigned long interval, unsigned int type, std::function<void()> function){
            //Using callback and pooling
            this->callbackFunction = function;
            this->hasCallback = true;
            setInterval(interval);
            this->type = type;

        };

        Timer(unsigned long interval, unsigned int type){
            // Pooling only
            this->interval = interval;
            this->type = type;
            Serial.print("Timer set for ");
            Serial.println(interval);
        };

        bool update(){
            unsigned long time_sec = time(NULL);
            return this->update(time_sec);
        };

        bool update(unsigned long &time_sec){
          if (started){
                // TODO : Replace with time function because millis can do a complete loop... 
                if (time_sec - previousCall >= interval){
                  
                    if (this->hasCallback){
                      callbackFunction();
                    }
                    
                    previousCall = time_sec;
                    if (type == SINGLE_SHOT)
                        pause();
                        
                    return true;
                }
            }

            return false;

        }

        void start(){
            started = true;
        };

        void start(bool reset){
           if (reset)
              this->previousCall = time(NULL);

           start();
        }

        void pause(){
            this->started = false;
        };

        unsigned long remainingTime(){
          if (this->started){
            unsigned long time_sec = time(NULL);
            return interval - (time_sec - previousCall);
          }

          return 0;
        };

        bool paused(){
          return !this->started;
        }

        void setInterval(unsigned long interv){
           this->interval = interv;
           Serial.printf("Timer set for %d", interval);            
        }

        static unsigned long getIntervalFromUnit(float amout, int unit){
            unsigned long ms = 0;
            long tmp = 0;
            switch(unit){
                case UNIT_D:
                    tmp = ((long) amout);
                    ms = ms + tmp * DAY_H * HOUR_MIN * MIN_S;
                    amout = (amout - tmp) * DAY_H;              
                case UNIT_H:
                    tmp = ((long) amout);
                    ms = ms + tmp * HOUR_MIN * MIN_S;
                    amout = (amout - tmp) * HOUR_MIN;
                case UNIT_MIN:
                    tmp = ((long) amout);
                    ms = ms + tmp * MIN_S;
                    amout = (amout - tmp) * MIN_S; 
                case UNIT_S:
                    tmp = ((long) amout);
                    ms = ms + tmp;
                default:
                    return ms;
            };

            return ms;
        };

    private:
        unsigned int type = LOOP_UNTIL_STOP;
        unsigned long interval = 0;
        unsigned long previousCall = 0;
        bool started = false;
        bool hasCallback = false;
        std::function<void(void)> callbackFunction;
};

#endif

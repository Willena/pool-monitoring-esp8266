#ifndef FIXED_TIMER_H
#define FIXED_TIMER_H

#include "utils.h"
#include "timer.h"
#include <functional>
#include <time.h>

#define DAY_H 24 //Number of hour in a day
#define MIN_S 60 //Number of second in a minute
#define HOUR_MIN 60 //Number of minites in an hour
#define DAY_S (DAY_H * HOUR_MIN * MIN_S)

class FixedTimeTimer : public Timer  {
    public:
        FixedTimeTimer(unsigned int startAt,unsigned int type, std::function<void()> function ) : Timer(0, type, function){
          this->startAt = startAt;
        };

        FixedTimeTimer(unsigned int startAt, unsigned int type) : Timer(0, type){
          this->startAt = startAt;
        };


        void start(){
            this->setInterval(this->computeTime());
            Timer::start();
        };

        void pause(){
          Timer::pause();
        }

   
        bool update(unsigned long &time_sec){
          bool res = Timer::update(time_sec);
          if (res){
            this->setInterval(this->computeTime());
          }
          
          return res;
        }
    private:
        unsigned long computeTime(){
            tm* timeObj = get_localtime();
            unsigned long nextTriggerSec = 0;
            unsigned long currentTimeSec = timeToSec(timeObj->tm_hour, timeObj->tm_min);
            if (startAt < currentTimeSec ) {
              //for example it is 14h and should start at 7h
              nextTriggerSec = (DAY_S - currentTimeSec) + startAt;
            }
            else{
              //it is 7h should start at 14h
              nextTriggerSec = startAt - currentTimeSec;
            }
            return nextTriggerSec;
        }
        unsigned int startAt = 0; //Second in the day to start at
};

#endif

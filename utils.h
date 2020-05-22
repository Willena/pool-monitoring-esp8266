#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <string.h>
#include "timer.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <vector>

tm* get_localtime(); // Defined in main ! 

unsigned long timeToSecFromString(const char * time){
  Serial.print("Time to str input : ");
  Serial.println(time);
  
  unsigned long result = 0;
  char * timeCopy = strdup(time);
  char * part = strtok(timeCopy, ":");
  result += atoi(part) * HOUR_MIN * MIN_S;
  part = strtok(NULL, ":");
  result += atoi(part) * MIN_S;

  free(timeCopy);
  return result;
}

bool formatFs(){
  LittleFS.begin();
  LittleFS.format();
}

void secToTimeString(unsigned long seconds, char * dest){

  if (seconds >= HOUR_MIN * MIN_S * DAY_H)
    seconds = (HOUR_MIN * MIN_S * DAY_H) - 60;

  unsigned int hours = seconds/(HOUR_MIN * MIN_S);
  unsigned int minutes = (seconds - (hours * HOUR_MIN * MIN_S ))/MIN_S;

  Serial.print("Seconds ");
  Serial.print(seconds);
  Serial.print(" Hour : ");
  Serial.print(hours);
  Serial.print(" Min : ");
  Serial.println(minutes);
  
  int s = sprintf(dest, "%d:%2d", hours, minutes);
  dest[s]= '\0';
  Serial.print("Read back the string ");
  Serial.println(dest);
  
}

unsigned long timeToSec(unsigned int hours, unsigned int minutes){
  unsigned long result = (hours * HOUR_MIN * MIN_S) + (minutes * MIN_S);
  return result;
}

bool has_value(std::vector<unsigned int> array, unsigned int month){
  for(unsigned int v: array) {
    if (v == month){
      return true;
    }
  }
  return false;
}

#endif

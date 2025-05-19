#include "ServoFlag.h"




ServoFlag::ServoFlag(int pin, int calibrationOffset)
{   
    _pin = pin;
    _calibrationOffset = calibrationOffset;     
}


void ServoFlag::init()
{
    
    Serial.printf("Init servo moving to %d \n", 0+_calibrationOffset);
    delay(1000);

    _servo1.attach(_pin);
    _servo1.write(0+_calibrationOffset);
}

void ServoFlag::moveTo(int degrees) 
{
    Serial.printf("Moving to %d\n", degrees);
    delay(1000);

    for(int posDegrees = 0+_calibrationOffset; posDegrees <= degrees+_calibrationOffset; posDegrees++) {
        _servo1.write(posDegrees);
        delay(10);
      }
}

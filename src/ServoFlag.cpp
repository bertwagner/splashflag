#include "ServoFlag.h"
#include <ESP32Servo.h>

Servo servo1; 

ServoFlag::ServoFlag(int pin, int calibrationOffset)
{   
    _calibrationOffset = calibrationOffset;

    servo1.attach(pin);
    servo1.write(0+_calibrationOffset);
         
}

void ServoFlag::moveTo(int degrees) 
{
    for(int posDegrees = 0+_calibrationOffset; posDegrees <= degrees+_calibrationOffset; posDegrees++) {
        servo1.write(posDegrees);
        Serial.println(posDegrees);
        delay(10);
      }
}

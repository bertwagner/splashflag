#include "ServoFlag.h"


ServoFlag::ServoFlag(int pin)
{   
    _pin = pin;
}


void ServoFlag::init()
{
    _servo1.attach(_pin);
    moveTo(0);
}

void ServoFlag::moveTo(int degrees) 
{
    Serial.printf("Moving to %d\n", degrees);
    _servo1.write(degrees);
}

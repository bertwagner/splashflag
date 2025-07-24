#include "ServoFlag.h"


ServoFlag::ServoFlag(int pin)
{   
    _pin = pin;
}


void ServoFlag::init()
{
    _servo1.attach(_pin);
    moveTo(0);
    _servo_initialized = true;
}

void ServoFlag::moveTo(int degrees) 
{
    if (_servo_initialized && degrees == _currentPositionDegrees) {
        return;
    } else {
        _currentPositionDegrees = degrees;
        _servo1.write(degrees);
    }
    
}

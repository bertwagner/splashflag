#include <Arduino.h>
#include <ESP32Servo.h>

class ServoFlag {
    public:
        ServoFlag(int pin);

        void init();

        void moveTo(int degrees);

    private:
        Servo _servo1; 
        int _pin;
        int _currentPositionDegrees = 0;
};

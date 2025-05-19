#include <Arduino.h>
#include <ESP32Servo.h>

class ServoFlag {
    public:
        ServoFlag(int pin, int calibrationOffset);

        void init();

        void moveTo(int degrees);

    private:
        Servo _servo1; 
        int _pin;
        int _calibrationOffset;
};

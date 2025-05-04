#pragma once

#include <Arduino.h>

class ServoFlag {
    public:
        ServoFlag(int pin, int calibrationOffset);

        void moveTo(int degrees);

    private:
        int _calibrationOffset;
};


#pragma once

#include <LiquidCrystal_I2C.h>

class Lcd {
    public:
        Lcd(int address, int columns, int rows);

        void write(const char *message);

    private:
        LiquidCrystal_I2C _lcd;

};
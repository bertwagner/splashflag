#pragma once

#include <LiquidCrystal_I2C.h>
#include <vector>
#include <Arduino.h>

class Lcd {
    public:
        Lcd(int address, int columns, int rows);

        void write(const char *message);

        void printString(char *text);

    private:
        LiquidCrystal_I2C _lcd;
        std::vector<std::vector<char>> _parseMessage(const char *message);
        

};
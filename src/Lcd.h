#ifndef LCD_H
#define LCD_H

#pragma once

#include <LiquidCrystal_I2C.h>
#include <iostream>

class Lcd {
    public:
        Lcd(int address, int columns, int rows);

        void write(std::string message);

    private:
        LiquidCrystal_I2C _lcd;

};
#endif
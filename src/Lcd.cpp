#include "Lcd.h"
#include <LiquidCrystal_I2C.h>
#include <iostream>

#include <Arduino.h>
Lcd::Lcd(int address, int columns, int rows) : _lcd(address, columns, rows) //constructor initialization list
{
    _lcd.init();
    _lcd.clear();         
    _lcd.backlight();   

}

void Lcd::write(String message) 
{
    _lcd.setCursor(0,0);
    _lcd.print(message); //TODO: Fix these data types? should i use std::string or char*?
}
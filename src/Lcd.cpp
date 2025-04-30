#include "Lcd.h"
#include <LiquidCrystal_I2C.h>
#include <iostream>

#include <Arduino.h>
Lcd::Lcd(int address, int columns, int rows) : _lcd(address, columns, rows) //TODO: I don't understand this
{
    _lcd.init();
    _lcd.clear();         
    _lcd.backlight();   

}

void Lcd::write(std::string message) 
{
    // Print a message on both lines of the LCD.
    _lcd.setCursor(0,0);   //Set cursor to character 2 on line 0
    _lcd.print(message.c_str()); //TODO: Fix these data types? should i use std::string or char*?
}
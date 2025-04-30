#include "Lcd.h"
#include <LiquidCrystal_I2C.h>

Lcd::Lcd(int address, int columns, int rows) : _lcd(address, columns, rows) //constructor initialization list
{
    _lcd.init();
    _lcd.clear();         
    _lcd.backlight();   

}

void Lcd::write(String message) 
{
    _lcd.setCursor(0,0);
    _lcd.print(message);
}
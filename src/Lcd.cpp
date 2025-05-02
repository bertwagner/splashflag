#include "Lcd.h"



Lcd::Lcd(int address, int columns, int rows) : _lcd(address, columns, rows) //constructor initialization list
{
    _lcd.init();
    _lcd.clear();         
    _lcd.backlight();   

}

void Lcd::write(const char *message) 
{
    _lcd.clear(); 
    _lcd.setCursor(0,0);
    _lcd.print(message);
}

std::vector<std::vector<char>> Lcd::_parseMessage(const char *message)
{
    std::vector<std::vector<char>> screens;

    std::vector<char> screen;

    screen.insert(0, "Hello");
    screen.insert(1, "Bert");

    return screens;
}

const int lcdCols = 16; //Number of columns in my lcd
const int lcdRows = 2;//Number of rows in my lcd
const int delayTime = 1000; //Value in milliseconds to delay the scroll

char text[] = "This is example text that should need more than one line to print. Here's more text."; //The actual text

void Lcd::printString(char *text) {
    char temp[lcdCols + 1];
    int iterations;
    int len = strlen(text);
  
    if (len <= lcdCols) {  //Prints the string if its length is
        _lcd.clear(); 
        _lcd.setCursor(0,0);
        _lcd.print(text);
      return;
    }

    iterations = len / lcdCols + 1;
    for (int counter = 0; counter < iterations; counter++) {
      strncpy(temp, &text[counter * lcdCols], lcdCols);
      temp[lcdCols] = '\0';
      _lcd.clear(); 
      _lcd.setCursor(0,0);
      _lcd.print(temp);
      delay(delayTime);
    }
    Serial.println();
  }
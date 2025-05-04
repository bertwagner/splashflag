#include "Lcd.h"

void setup() {
  // Instantiate the LCD
  Lcd lcd(0x27, 16, 2);

  // lcd.write("Another message");
  lcd.write("This is example text that should need more than one line to print. Here's more text. Visit https://bertwagner.com/testurl for more help.");
}

void loop() {


}
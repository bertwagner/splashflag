#include "Lcd.h"

void setup() {
  // Instantiate the LCD
  Lcd lcd(0x27, 16, 2);

  lcd.write("Hello, World!32");
}

void loop() {


}
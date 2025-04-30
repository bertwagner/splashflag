#include "Lcd.h"

void setup() {
  // Instantiate the LCD
  Lcd lcd(0x27, 16, 2);

  lcd.write("0123456789ABCDEF1123456789ABCDEF");

  delay(1000);

  lcd.write("Another message");
}

void loop() {


}
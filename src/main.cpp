#include "Lcd.h"

void setup() {
  Lcd lcd(0x27, 16, 2);

  lcd.write("Welcome to SplashFlag!");
}

void loop() {


}
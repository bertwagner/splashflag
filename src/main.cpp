#include "Lcd.h"
#include "ServoFlag.h"



static const int servoPin = 9;
ServoFlag servoFlag(9,5);

void setup() {
  Serial.begin(115200);

  // TODO: Move outside setup?
  Lcd lcd(0x27, 16, 2);
  lcd.write("Welcome to SplashFlag!");
  servoFlag.init();
 
}

void loop() {
  // servoFlag.moveTo(90);
  // delay(3000);
  // servoFlag.moveTo(0);
  // delay(3000);
}
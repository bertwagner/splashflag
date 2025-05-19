#include "Lcd.h"
#include "ServoFlag.h"


Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.write("Welcome to SplashFlag!");
  
  servoFlag.init();

  delay(2000);
  servoFlag.moveTo(120);

  delay(2000);
  servoFlag.moveTo(90);
 
}

void loop() {
  // servoFlag.moveTo(90);
  // delay(3000);
  // servoFlag.moveTo(0);
  // delay(3000);
}
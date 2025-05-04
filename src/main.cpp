#include "Lcd.h"



static const int servoPin = 9;



void setup() {


  Lcd lcd(0x27, 16, 2);

  lcd.write("Welcome to SplashFlag!");

  

  servo1.attach(servoPin);
  servo1.write(0);
  delay(2000);

  int servoCalibrationOffset=5;

  for(int posDegrees = 0+servoCalibrationOffset; posDegrees <= 90+servoCalibrationOffset; posDegrees++) {
    servo1.write(posDegrees);
    Serial.println(posDegrees);
    delay(20);
  }
  delay(1000);

  servo1.write(0+servoCalibrationOffset);
}

void loop() {
  

}
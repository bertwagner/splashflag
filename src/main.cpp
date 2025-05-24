#include "Lcd.h"
#include "ServoFlag.h"
#include "CaptivePortal.h"

Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);
CaptivePortal portal;

void setup() {
	Serial.begin(115200);
	// Wait for the Serial object to become available.
	while (!Serial)
		;

	// lcd.init();
	// lcd.write("Welcome to SplashFlag!");

	// servoFlag.init();

	// delay(2000);
	// servoFlag.moveTo(120);

	// delay(2000);
	// servoFlag.moveTo(90);

	
	

	portal.init();
 
}

void loop() {
  // servoFlag.moveTo(90);
  // delay(3000);
  // servoFlag.moveTo(0);
  // delay(3000);

	portal.processNextDNSRequest();
}





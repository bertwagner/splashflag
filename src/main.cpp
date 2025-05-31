#include "Lcd.h"
#include "ServoFlag.h"
#include "CaptivePortal.h"

#include "CredentialManager.h"

Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);
CaptivePortal portal;

CredentialManager credentialManager;

int resetButtonState = 0;

void setup() {
	Serial.begin(115200);
	// Wait for the Serial object to become available.
	while (!Serial)
		;

	delay(3000);
	Serial.printf("Getting ready...\n");
	delay(3000);


	//portal.init();


	auto [ssid,password] = credentialManager.retrieveCredentials();
	Serial.printf("Creds in main thread: %s, %s\n", ssid, password);
	delay(3000);
	credentialManager.saveCredentials("SSID5","Password5");
	delay(3000);


	// lcd.init();
	// lcd.write("Welcome to SplashFlag!");

	// servoFlag.init();

	// delay(2000);
	// servoFlag.moveTo(120);

	// delay(2000);
	// servoFlag.moveTo(90);

	
	
	

	

	// Reset button
	//pinMode(5, INPUT);
}

void loop() {
  // servoFlag.moveTo(90);
  // delay(3000);
  // servoFlag.moveTo(0);
  // delay(3000);

	//portal.processNextDNSRequest();

	// resetButtonState = digitalRead(5);
	// Serial.printf("Reset button state: %d", resetButtonState);

	delay(1000);
}





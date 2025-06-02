#include "Lcd.h"
#include "ServoFlag.h"
#include "CaptivePortal.h"
#include "CredentialManager.h"
#include "esp_wifi.h"
#include "WiFi.h"


Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);
CredentialManager credentialManager;
CaptivePortal portal(credentialManager, lcd);

int resetButtonState = 0;

void factoryReset() {
	Serial.printf("FACTORY RESET");
	credentialManager.saveCredentials("","");
}


void setup() {
	Serial.begin(115200);
	// Wait for the Serial object to become available.
	while (!Serial)
		;

	// Disconnect from APs if previously connected
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);

	

	delay(3000);
	Serial.printf("Getting ready...\n");
	delay(3000);
	//factoryReset();

	lcd.init();
	lcd.write("Welcome to SplashFlag!");

	
	// Check if credentials exist
	auto [ssid,password] = credentialManager.retrieveCredentials();

	char ssidarr[strlen(ssid)+1];
	strcpy(ssidarr, ssid);
	Serial.printf("ssidarr: %s\n", ssidarr);
	
	char passwordarr[strlen(password)+1];
	strcpy(passwordarr, password);

	if (ssid == nullptr || ssid[0] == '\0') {
		Serial.printf("Blank SSID\n");
		portal.init();
		lcd.write("Please connect to SplashFlag Wifi AP with your phone and visit http://4.3.2.1");
	} else {		
		// Attempt to connect to WiFi using saved credentials
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssidarr, passwordarr);
		Serial.print("Connecting to WiFi ..");
		int wifiConnectTries = 0;
		const int maxTries = 20; // Try for ~20 seconds
		while (WiFi.status() != WL_CONNECTED && wifiConnectTries < maxTries) {
			Serial.print('.');
			delay(1000);
			wifiConnectTries++;
		}
		if (WiFi.status() == WL_CONNECTED) {
			Serial.println("\nWiFi connected!");
			Serial.print("IP address: ");
			Serial.println(WiFi.localIP());
		} else {
			Serial.println("\nFailed to connect to WiFi. Resetting credentials.");
			// TODO: If fail to connect to wifi, reset creds and have user connect to AP again.
			//credentialManager.saveCredentials("", "");
			//portal.init();
			//lcd.write("Please connect to SplashFlag Wifi AP with your phone and visit http://4.3.2.1");
		}
	}

	// If Wifi credentials exist connect.
	// If unsuccessful or no credits, start captive portal.


	
	


	
	// Serial.printf("Creds in main thread: %s, %s\n", ssid, password);
	// delay(3000);
	// credentialManager.saveCredentials("SSID5","Password5");
	// delay(3000);


	

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

	portal.processNextDNSRequest();

	// resetButtonState = digitalRead(5);
	// Serial.printf("Reset button state: %d", resetButtonState);

	//delay(1000);
}




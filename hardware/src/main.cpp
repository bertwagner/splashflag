#include "SplashFlagController.h"

Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);
CredentialManager credentialManager;
CaptivePortal portal(credentialManager, lcd);
SplashFlagController controller(lcd, servoFlag, credentialManager, portal);


void setup() {
	Serial.begin(115200);

	// Reset button
	pinMode(4, INPUT);
	
	//Disconnect from APs if previously connected
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);

	// initialize all components
	servoFlag.init();
	lcd.init();
	controller.init();
	controller.setDisplayMessage("Welcome to SplashFlag!");
	
	//Check if wifi credentials exist
	auto [ssid,password] = credentialManager.retrieveCredentials();
	char ssidarr[strlen(ssid)+1];
	strcpy(ssidarr, ssid);
	
	char passwordarr[strlen(password)+1];
	strcpy(passwordarr, password);

	if (ssid == nullptr || strlen(ssidarr) == 0 || ssid[0] == '\0') {
		//Serial.printf("Blank SSID\n");
		portal.init();
		controller.setDisplayMessage("Please connect to SplashFlag Wifi AP with your phone and visit http://4.3.2.1");
	} else {		
		// Attempt to connect to WiFi using saved credentials
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssidarr, passwordarr);

		int wifiConnectTries = 0;
		const int maxTries = 20; // Try for ~20 seconds
		while (WiFi.status() != WL_CONNECTED && wifiConnectTries < maxTries) {
			Serial.print('.');
			delay(1000);
			wifiConnectTries++;
			controller.handleResetButton();
		}
		if (WiFi.status() == WL_CONNECTED) {
			Serial.println("\nWiFi connected!");
			Serial.print("IP address: ");
			Serial.println(WiFi.localIP());

			controller.setDisplayMessageWithDuration("SplashFlag wifi connected! The screen will now go blank until a pool announcement is made.", 3, true);
			servoFlag.init();
		} else {
			Serial.println("\nFailed to connect to WiFi.");
			controller.setDisplayMessage("Wifi connection failed. Check your wifi connection. If problem persists, hold factory reset button for 10 seconds and re-enter Wifi password.");
			
			// Continuously handle reset button after WiFi failure
			while (true) {
				controller.handleResetButton();
				delay(50); // Small delay to prevent excessive CPU usage
			}
		}
	}
}

void loop() {
	controller.update();
}




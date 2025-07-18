#include "secrets.h"
#include "Lcd.h"
#include "ServoFlag.h"
#include "CaptivePortal.h"
#include "CredentialManager.h"
#include "esp_wifi.h"
#include "WiFi.h"
#include <WebSocketsClient.h>  // include before MQTTPubSubClient.h
#include <MQTTPubSubClient.h>
#include <ArduinoJson.h>

Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);
CredentialManager credentialManager;
CaptivePortal portal(credentialManager, lcd);

int resetButtonState = 0;
bool mqttInitialized = false;
unsigned long flagUpSecondsEndTime = 0;
const char* message;

WebSocketsClient client;
MQTTPubSub::PubSubClient<512> mqtt;

void connect() {
connect_to_host:
    Serial.println("connecting to host...");
    client.disconnect();
    client.begin(MQTT_BROKER_URL, 80, "/", "mqtt"); 
	// can use port 443 and .beginSSL(), but need to set up root certs on arduino
    client.setReconnectInterval(2000);

    Serial.print("connecting to mqtt broker...");
    while (!mqtt.connect("arduino", MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.print(".");
        lcd.write("Connecting to SplashFlag...");
       
        if (!client.isConnected()) {
            Serial.println("WebSocketsClient disconnected");
            goto connect_to_host;
        }
    }
    Serial.println(" connected!");
	lcd.write("Connected!");
	lcd.turnOff();
}

void factoryReset() {
	Serial.printf("FACTORY RESET");
	lcd.write("RESETTING TO FACTORY SETTINGS");
	credentialManager.saveCredentials("","");
}

void setup() {
	Serial.begin(115200);
	// Wait for the Serial object to become available.
	while (!Serial)
		;

	// Reset button
	pinMode(4, INPUT);
	
	//Disconnect from APs if previously connected
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);


	servoFlag.init();
	lcd.init();
	lcd.write("Welcome to SplashFlag!");

	
	//Check if credentials exist
	auto [ssid,password] = credentialManager.retrieveCredentials();
	char ssidarr[strlen(ssid)+1];
	strcpy(ssidarr, ssid);
	
	char passwordarr[strlen(password)+1];
	strcpy(passwordarr, password);

	if (ssid == nullptr || strlen(ssidarr) == 0 || ssid[0] == '\0') {
		//Serial.printf("Blank SSID\n");
		portal.init();
		lcd.write("Please connect to SplashFlag Wifi AP with your phone and visit http://4.3.2.1");
	} else {		
		// Attempt to connect to WiFi using saved credentials
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssidarr, passwordarr);
		//Serial.print("Connecting to WiFi...");
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

			lcd.write("SplashFlag wifi connected! The screen will now go blank until a pool announcement is made.");
			lcd.turnOff();
			servoFlag.init();
		} else {
			Serial.println("\nFailed to connect to WiFi. Resetting credentials.");
			lcd.write("Wifi connection failed. Check your wifi connection. If problem persists, hold factory reset button for 10 seconds and re-enter Wifi password.");
			esp_restart();
		}
	}

	
}

void loop() {
	if (WiFi.status() != WL_CONNECTED) {
		portal.processNextDNSRequest();
		Serial.println("Processing next DNS request in captive portal mode.");

		return;
	} 

	if (!mqttInitialized) {
		// Initialize the MQTT client
		mqtt.disconnect();
		mqtt.begin(client);

		// connect to wifi, host and mqtt broker
		connect();

		mqtt.subscribe("splashflag/all", [](const String& payload, const size_t size) {
			// Max message length is ~490 characters
			//Serial.print("splashflag/all received: ");
			//Serial.println(payload);
			JsonDocument doc;
			DeserializationError error = deserializeJson(doc, payload);
			if (error) {
				Serial.print("deserializeJson() failed: ");
				Serial.println(error.c_str());
				return;
			}

			// Extract values from the JSON document
			message = doc["message"];
			const char* current_time = doc["current_time"];
			const char* expiration_time = doc["expiration_time"];

			// Helper function to parse "YYYY-MM-DD HH:MM:SS" to time_t
			auto parseDateTime = [](const char* datetime) -> time_t {
				struct tm tm;
				memset(&tm, 0, sizeof(tm));
				sscanf(datetime, "%d-%d-%d %d:%d:%d",
					&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
					&tm.tm_hour, &tm.tm_min, &tm.tm_sec);
				tm.tm_year -= 1900;
				tm.tm_mon -= 1;
				return mktime(&tm);
			};

			time_t currentTime = parseDateTime(current_time);
			time_t expirationTime = parseDateTime(expiration_time);
			unsigned long flagUpDurationSeconds = expirationTime - currentTime;
			flagUpSecondsEndTime = (millis() / 1000) + flagUpDurationSeconds;
		});
		mqttInitialized = true;
		Serial.println("MQTT client initialized.");

	}
	if (!mqtt.isConnected()) {
		//connect();
		mqttInitialized = false;
	} else {
		mqtt.update();  // should be called
	}

	


	


  

	// TODO: Refactor everything
	// TODO: Keep message displayed for duration of flag up

	if ((millis()/1000) < flagUpSecondsEndTime){
		servoFlag.moveTo(90);
		lcd.write(message);
	} else {
		servoFlag.moveTo(0);
		flagUpSecondsEndTime = 0; 
		lcd.turnOff();
	}
	


	// Reset if reset button has been held for 10 seconds
	resetButtonState = digitalRead(4);
	//Serial.printf("Reset button state: %d\n", resetButtonState);
	static unsigned long resetButtonPressedTime = 0;
	static bool resetInProgress = false;

	if (resetButtonState == HIGH) {
		if (!resetInProgress) {
			resetButtonPressedTime = millis();
			resetInProgress = true;
		} else if (millis() - resetButtonPressedTime >= 10000) {
			factoryReset();
			servoFlag.moveTo(0);
			esp_restart();
			Serial.printf("RESETTING\n");
		}
	} else {
		resetInProgress = false;
	}
	delay(500);
	// Serial.printf("Reset button held for: %d\n", millis() - resetButtonPressedTime);

}




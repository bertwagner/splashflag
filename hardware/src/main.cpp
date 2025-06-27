#include "secrets.h"
#include "Lcd.h"
#include "ServoFlag.h"
#include "CaptivePortal.h"
#include "CredentialManager.h"
#include "esp_wifi.h"
#include "WiFi.h"
#include <WebSocketsClient.h>  // include before MQTTPubSubClient.h
#include <MQTTPubSubClient.h>

Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);
CredentialManager credentialManager;
CaptivePortal portal(credentialManager, lcd);

int resetButtonState = 0;
int wifiConnected = 0;

WebSocketsClient client;
MQTTPubSubClient mqtt;

void connect() {
connect_to_host:
    Serial.println("connecting to host...");
    client.disconnect();
    client.begin(MQTT_BROKER_URL, 80, "/", "mqtt");  // "mqtt" is required
	// can use port 443 and .beginSSL(), but need to set up root certs on arduino
    client.setReconnectInterval(2000);

    Serial.print("connecting to mqtt broker...");
    while (!mqtt.connect("arduino", MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.print(".");
        delay(1000);
       
        if (!client.isConnected()) {
            Serial.println("WebSocketsClient disconnected");
            goto connect_to_host;
        }
    }
    Serial.println(" connected!");
}






void factoryReset() {
	Serial.printf("FACTORY RESET");
	credentialManager.saveCredentials("","");
}


void setup() {
	Serial.begin(115200);
	// Wait for the Serial object to become available.
	while (!Serial)
		;

	// Reset button
	pinMode(5, INPUT);

	// Disconnect from APs if previously connected
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);
	wifiConnected = 0;
	

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
	
	char passwordarr[strlen(password)+1];
	strcpy(passwordarr, password);

	if (ssid == nullptr || strlen(ssidarr) == 0 || ssid[0] == '\0') {
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
			wifiConnected = 1;
			lcd.write("SplashFlag wifi connected! The screen will now go blank until a pool announcement is made.");
			lcd.turnOff();
			servoFlag.init();
		} else {
			Serial.println("\nFailed to connect to WiFi. Resetting credentials.");
			lcd.write("Wifi connection failed. Check your wifi connection. If problem persists, hold factory reset button for 10 seconds and re-enter Wifi password.");
			//factoryReset();
			esp_restart();
		}
	}






	
	// initialize mqtt client
    mqtt.begin(client);

    // connect to wifi, host and mqtt broker
    connect();

    // subscribe callback which is called when every packet has come
    // mqtt.subscribe([](const String& topic, const String& payload, const size_t size) {
    //     Serial.println("mqtt received: " + topic + " - " + payload);
    // });

    // subscribe topic and callback which is called when /hello has come
    mqtt.subscribe("splashflag/all", [](const String& payload, const size_t size) {
        Serial.print("splashflag/all received: ");
        Serial.println(payload);
    });
	

	
}

void loop() {
	if (wifiConnected == 0) {
		portal.processNextDNSRequest();
	} else {
		// Monitor for pool announcements.
		servoFlag.moveTo(90);
		delay(3000);
		servoFlag.moveTo(0);
		delay(3000);
	}


	// TODO: Get servo working again
	// TODO: Get mqtt sub working and into a class
	// TODO: Move this mqtt all into the else above

	mqtt.update();  // should be called

    if (!mqtt.isConnected()) {
        connect();
    }

    // publish message
    // static uint32_t prev_ms = millis();
    // if (millis() > prev_ms + 1000) {
    //     prev_ms = millis();
    //     mqtt.publish("splashflag/all", "world");
    // }

	








  

	
	// Reset if reset button has been held for 10 seconds
	resetButtonState = digitalRead(5);
	//Serial.printf("Reset button state: %d\n", resetButtonState);
	static unsigned long resetButtonPressedTime = 0;
	static bool resetInProgress = false;

	if (resetButtonState == HIGH) {
		if (!resetInProgress) {
			resetButtonPressedTime = millis();
			resetInProgress = true;
		} else if (millis() - resetButtonPressedTime >= 10000) {
			factoryReset();
			lcd.write("RESETTING TO FACTORY SETTINGS");
			servoFlag.moveTo(0);
			esp_restart();
			Serial.printf("RESETTING\n");
		}
	} else {
		resetInProgress = false;
	}
	delay(500);
	//Serial.printf("Reset button held for: %d\n", millis() - resetButtonPressedTime);

}




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
#include <base64.h>
#include <pthread.h>

Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);
CredentialManager credentialManager;
CaptivePortal portal(credentialManager, lcd);

int resetButtonState = 0;
bool mqttInitialized = false;

unsigned long flagUpSecondsEndTime = 0;
char message[512];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


static unsigned long resetButtonPressedTime = 0;
static bool buttonWasPressed = false;

WebSocketsClient client;
MQTTPubSub::PubSubClient<512> mqtt;

bool shouldStopDisplaying() {
    pthread_mutex_lock(&mutex);
    bool stop = (flagUpSecondsEndTime == 0) || ((millis()/1000) >= flagUpSecondsEndTime);
    pthread_mutex_unlock(&mutex);
    return stop;
}

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
	// TODO: Only show message if doesn't connect 10 times?
	//lcd.write("Connected!");
	//lcd.turnOff();
}

void factoryReset() {
	Serial.printf("FACTORY RESET");
	lcd.write("RESETTING TO FACTORY SETTINGS");
	credentialManager.saveCredentials("","");
}

void* display_thread(void* arg) {
    char local_message[512] = "";
    int screen_count = 0;
    LCDScreen screens[50];
    int current_screen = 0;
    unsigned long last_screen_change = 0;
    bool displaying = false;
    bool screen_drawn = false;  // Track if current screen is already drawn
    
    while (true) {
        // Check if we should stop displaying
        if (shouldStopDisplaying()) {
            if (displaying) {
                pthread_mutex_lock(&mutex);
                servoFlag.moveTo(0);
                pthread_mutex_unlock(&mutex);
                lcd.turnOff();
                displaying = false;
                screen_drawn = false;
            }
        } else {
            // We should be displaying
            pthread_mutex_lock(&mutex);
            // Check if we got a new message or we're not currently displaying
            if (!displaying || strcmp(local_message, message) != 0) {
                strcpy(local_message, message);
                pthread_mutex_unlock(&mutex);
                
                // Format message for LCD
                lcd.formatForLcd(local_message, &screen_count, &screens);
                current_screen = 0;
                last_screen_change = millis();
                displaying = true;
                screen_drawn = false;  // Need to redraw new message
                
                pthread_mutex_lock(&mutex);
                servoFlag.moveTo(90);
                pthread_mutex_unlock(&mutex);
            } else {
                pthread_mutex_unlock(&mutex);
            }
            
            if (displaying && screen_count > 0) {
                // Only draw screen if we haven't drawn it yet
                if (!screen_drawn) {
                    lcd.displayScreen(screens[current_screen]);
                    screen_drawn = true;
                }
                
                // Check if it's time to move to next screen
                if (millis() - last_screen_change >= SCROLL_DELAY) {
                    current_screen++;
                    if (current_screen >= screen_count) {
                        current_screen = 0; // Loop back to first screen
                    }
                    last_screen_change = millis();
                    screen_drawn = false;  // Need to draw the new screen
                }
            }
        }
        
        delay(10); // Much faster checking, but only redraw when needed
    }
    return NULL;
}

void handleMqttMessage(const char* topic, const String& payload, const size_t size) {
    // Max message length is ~490 characters
	//Serial.print("splashflag/all received: ");
	Serial.println(payload);
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload);
	if (error) {
		Serial.print("deserializeJson() failed: ");
		Serial.println(error.c_str());
		return;
	}

	// Extract values from the JSON document
	pthread_mutex_lock(&mutex);
	if (strcmp(topic, "debug") == 0) {
		snprintf(message, sizeof(message), "DEBUG: %s", doc["message"].as<const char*>());
	} else {
		strncpy(message, doc["message"], sizeof(message) - 1);
	}
	message[sizeof(message) - 1] = '\0';

	const char* current_time = doc["current_time"];
	const char* expiration_time = doc["expiration_time"];

	// Helper function to parse "YYYY-MM-DD HH:MM:SS" to time_t
	auto parseDateTime = [](const char* datetime) -> time_t {
		struct tm tm;
		memset(&tm, 0, sizeof(tm));
		sscanf(datetime, "%d-%d-%dT%d:%d:%d",
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
	//Serial.printf("Flag up Seconds Duration: %d\n", flagUpDurationSeconds);
	//Serial.printf("Flag up Seconds EndTime: %d\n", flagUpSecondsEndTime);
	pthread_mutex_unlock(&mutex);
}

void setup() {
	Serial.begin(115200);
	// Wait for the Serial object to become available.
	// while (!Serial)
	// 	;

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
			Serial.println("\nFailed to connect to WiFi.");
			lcd.write("Wifi connection failed. Check your wifi connection. If problem persists, hold factory reset button for 10 seconds and re-enter Wifi password.");
			esp_restart();
		}
	}

	
	pthread_t reader_tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8192); // Increase from default ~2KB to 8KB
	pthread_create(&reader_tid, &attr, display_thread, NULL);
	pthread_attr_destroy(&attr);

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
			handleMqttMessage("all", payload, size);
		});
		mqtt.subscribe("splashflag/debug", [](const String& payload, const size_t size) {
			handleMqttMessage("debug", payload, size);
		});

		mqttInitialized = true;
		Serial.println("MQTT client initialized.");
	}

	if (!mqtt.isConnected()) {
		mqttInitialized = false;
	} else {
		mqtt.update();  // should be called
	}


	// Clear message if reset button pressed quickly
	// Reset device if reset button has been held for 10 seconds
	
	resetButtonState = digitalRead(4);

	if (resetButtonState == HIGH && !buttonWasPressed) {
		// Button just pressed
		resetButtonPressedTime = millis();
		buttonWasPressed = true;
	}

	if (resetButtonState == HIGH && buttonWasPressed) {
		// Button is still pressed
		unsigned long heldTime = millis() - resetButtonPressedTime;
		if (heldTime >= 10000) {
			// Button has been held for 10 seconds, factory reset
			factoryReset();
			pthread_mutex_lock(&mutex);
			servoFlag.moveTo(0);
			pthread_mutex_unlock(&mutex);
			esp_restart();
			Serial.printf("RESETTING\n");
		} else if (heldTime > 100) {
			// Button pressed for less than 10 seconds, reset the status
			pthread_mutex_lock(&mutex);
			flagUpSecondsEndTime = 0;
			pthread_mutex_unlock(&mutex);
			//Serial.printf("Reset button pressed for less than 10 seconds but more than 2 seconds, resetting flag up status.\n");
		}
		
	}

	if (resetButtonState == LOW) {
		buttonWasPressed = false;
	}
	
}




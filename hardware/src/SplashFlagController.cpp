#include "SplashFlagController.h"

unsigned long SplashFlagController::resetButtonPressedTime = 0;
bool SplashFlagController::buttonWasPressed = false;

SplashFlagController::SplashFlagController(Lcd& lcd, ServoFlag& servoFlag, CredentialManager& credentialManager, CaptivePortal& portal)
    : lcd(lcd), servoFlag(servoFlag), credentialManager(credentialManager), portal(portal),
      resetButtonState(0), mqttInitialized(false), flagUpSecondsEndTime(0) {
    mutex = PTHREAD_MUTEX_INITIALIZER;
    memset(message, 0, sizeof(message));
}

SplashFlagController::~SplashFlagController() {
    pthread_mutex_destroy(&mutex);
}

void SplashFlagController::init() {
    pthread_t reader_tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8192);
    pthread_create(&reader_tid, &attr, display_thread, this);
    pthread_attr_destroy(&attr);
}

void SplashFlagController::update() {
    if (WiFi.status() != WL_CONNECTED) {
        portal.processNextDNSRequest();
        Serial.println("Processing next DNS request in captive portal mode.");
        return;
    } 

    if (!mqttInitialized) {
        mqtt.disconnect();
        mqtt.begin(client);

        connect();

        mqtt.subscribe("splashflag/all", [this](const String& payload, const size_t size) {
            handleMqttMessage("all", payload, size);
        });
        mqtt.subscribe("splashflag/debug", [this](const String& payload, const size_t size) {
            handleMqttMessage("debug", payload, size);
        });

        mqttInitialized = true;
        Serial.println("MQTT client initialized.");
    }

    if (!mqtt.isConnected()) {
        mqttInitialized = false;
    } else {
        mqtt.update();
    }

    handleResetButton();
}

bool SplashFlagController::shouldStopDisplaying() {
    pthread_mutex_lock(&mutex);
    bool stop = (flagUpSecondsEndTime == 0) || ((millis()/1000) >= flagUpSecondsEndTime);
    pthread_mutex_unlock(&mutex);
    return stop;
}

void SplashFlagController::connect() {
connect_to_host:
    Serial.println("connecting to host...");
    client.disconnect();

    client.begin(MQTT_BROKER_URL, 80, "/", "mqtt");
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
}

void SplashFlagController::factoryReset() {
    Serial.printf("FACTORY RESET");
    lcd.write("RESETTING TO FACTORY SETTINGS");
    credentialManager.saveCredentials("","");
}

void* SplashFlagController::display_thread(void* arg) {
    SplashFlagController* controller = static_cast<SplashFlagController*>(arg);
    char local_message[512] = "";
    int screen_count = 0;
    LCDScreen screens[50];
    int current_screen = 0;
    unsigned long last_screen_change = 0;
    bool displaying = false;
    bool screen_drawn = false;
    
    while (true) {
        if (controller->shouldStopDisplaying()) {
            if (displaying) {
                pthread_mutex_lock(&controller->mutex);
                controller->servoFlag.moveTo(0);
                pthread_mutex_unlock(&controller->mutex);
                controller->lcd.turnOff();
                displaying = false;
                screen_drawn = false;
            }
        } else {
            pthread_mutex_lock(&controller->mutex);
            if (!displaying || strcmp(local_message, controller->message) != 0) {
                strcpy(local_message, controller->message);
                pthread_mutex_unlock(&controller->mutex);
                
                controller->lcd.formatForLcd(local_message, &screen_count, &screens);
                current_screen = 0;
                last_screen_change = millis();
                displaying = true;
                screen_drawn = false;
                
                pthread_mutex_lock(&controller->mutex);
                controller->servoFlag.moveTo(90);
                pthread_mutex_unlock(&controller->mutex);
            } else {
                pthread_mutex_unlock(&controller->mutex);
            }
            
            if (displaying && screen_count > 0) {
                if (!screen_drawn) {
                    controller->lcd.displayScreen(screens[current_screen]);
                    screen_drawn = true;
                }
                
                if (millis() - last_screen_change >= SCROLL_DELAY) {
                    current_screen++;
                    if (current_screen >= screen_count) {
                        current_screen = 0;
                    }
                    last_screen_change = millis();
                    screen_drawn = false;
                }
            }
        }
        
        delay(10);
    }
    return NULL;
}

void SplashFlagController::handleMqttMessage(const char* topic, const String& payload, const size_t size) {
    Serial.println(payload);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    pthread_mutex_lock(&mutex);
    if (strcmp(topic, "debug") == 0) {
        snprintf(message, sizeof(message), "DEBUG: %s", doc["message"].as<const char*>());
    } else {
        strncpy(message, doc["message"], sizeof(message) - 1);
    }
    message[sizeof(message) - 1] = '\0';

    const char* current_time = doc["current_time"];
    const char* expiration_time = doc["expiration_time"];

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
    pthread_mutex_unlock(&mutex);
}

void SplashFlagController::handleResetButton() {
    resetButtonState = digitalRead(4);
    
    if (resetButtonState == HIGH && !buttonWasPressed) {
        resetButtonPressedTime = millis();
        buttonWasPressed = true;
    }

    if (resetButtonState == HIGH && buttonWasPressed) {
        unsigned long heldTime = millis() - resetButtonPressedTime;
        if (heldTime >= 10000) {
            factoryReset();
            pthread_mutex_lock(&mutex);
            servoFlag.moveTo(0);
            pthread_mutex_unlock(&mutex);
            esp_restart();
        } else if (heldTime > 100) {
            pthread_mutex_lock(&mutex);
            flagUpSecondsEndTime = 0;
            pthread_mutex_unlock(&mutex);
        }
    }

    if (resetButtonState == LOW) {
        buttonWasPressed = false;
    }
}
#include "SplashFlagController.h"

unsigned long SplashFlagController::resetButtonPressedTime = 0;
bool SplashFlagController::buttonWasPressed = false;

SplashFlagController::SplashFlagController(Lcd& lcd, ServoFlag& servoFlag, CredentialManager& credentialManager, CaptivePortal& portal)
    : lcd(lcd), servoFlag(servoFlag), credentialManager(credentialManager), portal(portal),
      resetButtonState(0), mqttInitialized(false), flagUpSecondsEndTime(0), forceStop(false) {
    mutex = PTHREAD_MUTEX_INITIALIZER;
    queueMutex = PTHREAD_MUTEX_INITIALIZER;
    memset(message, 0, sizeof(message));
}

SplashFlagController::~SplashFlagController() {
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&queueMutex);
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
    bool stop = (flagUpSecondsEndTime > 0) && ((millis()/1000) >= flagUpSecondsEndTime);
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
        setDisplayMessage("Connecting to SplashFlag...");
       
        if (!client.isConnected()) {
            Serial.println("WebSocketsClient disconnected");
            goto connect_to_host;
        }
    }
    Serial.println(" connected!");
}

void SplashFlagController::factoryReset() {
    Serial.printf("FACTORY RESET");
    setDisplayMessage("RESETTING TO FACTORY SETTINGS");
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
    unsigned long message_start_time = 0;
    bool current_message_indefinite = false;
    unsigned long current_message_duration = 0;
    bool all_screens_shown = false;
    bool current_message_stop_after_loop = false;
    bool current_message_is_from_mqtt = false;
    
    while (true) {
        // Check if current message should stop displaying
        bool should_stop = false;
        bool has_queued_messages = false;
        
        // Check if there are more messages waiting
        pthread_mutex_lock(&controller->queueMutex);
        has_queued_messages = !controller->messageQueue.empty();
        pthread_mutex_unlock(&controller->queueMutex);
        
        if (displaying) {
            // Check for force stop (from reset button)
            pthread_mutex_lock(&controller->mutex);
            bool force_stop_requested = controller->forceStop;
            pthread_mutex_unlock(&controller->mutex);
            
            if (force_stop_requested) {
                // Force stop has highest priority - stop immediately
                should_stop = true;
            } else if (current_message_indefinite) {
                // Indefinite messages need minimum time to show all screens, then yield to queue
                unsigned long elapsed_time = millis() - message_start_time;
                unsigned long min_display_time = screen_count * SCROLL_DELAY + 1000; // All screens + 1 second buffer
                
                should_stop = has_queued_messages && (elapsed_time >= min_display_time);
            } else {
                // Timed messages stop when duration expires OR (all screens shown AND stopAfterOneLoop is true)
                bool duration_expired = (millis() - message_start_time) >= (current_message_duration * 1000);
                bool should_stop_after_loop = current_message_stop_after_loop && all_screens_shown;
                should_stop = duration_expired || should_stop_after_loop;
            }
        }
        
        // Check for new message in queue or if current message should stop
        bool need_new_message = !displaying || should_stop;
        if (need_new_message) {
            pthread_mutex_lock(&controller->queueMutex);
            if (!controller->messageQueue.empty()) {
                QueuedMessage qMsg = controller->messageQueue.front();
                controller->messageQueue.pop();
                pthread_mutex_unlock(&controller->queueMutex);
                
                // Clear display before transitioning to prevent flashing
                if (displaying) {
                    controller->lcd.turnOff();
                    delay(100); // Brief pause for clean transition
                }
                
                strcpy(local_message, qMsg.message);
                current_message_indefinite = qMsg.isIndefinite;
                current_message_duration = qMsg.durationSeconds;
                current_message_stop_after_loop = qMsg.stopAfterOneLoop;
                current_message_is_from_mqtt = qMsg.isFromMqtt;
                message_start_time = millis();
                
                controller->lcd.formatForLcd(local_message, &screen_count, &screens);
                current_screen = 0;
                last_screen_change = millis();
                displaying = true;
                screen_drawn = false;
                all_screens_shown = (screen_count <= 1); // Single screen messages are immediately "all shown"
                
                pthread_mutex_lock(&controller->mutex);
                controller->forceStop = false; // Reset force stop flag for new message
                // Only raise servo flag for MQTT messages
                if (current_message_is_from_mqtt) {
                    controller->servoFlag.moveTo(90);
                }
                pthread_mutex_unlock(&controller->mutex);
            } else {
                pthread_mutex_unlock(&controller->queueMutex);
                if (displaying && should_stop) {
                    // No more messages and current one expired, or force stop requested
                    pthread_mutex_lock(&controller->mutex);
                    // Only lower servo flag if the current message was from MQTT
                    if (current_message_is_from_mqtt) {
                        controller->servoFlag.moveTo(0);
                    }
                    controller->forceStop = false; // Reset force stop flag
                    pthread_mutex_unlock(&controller->mutex);
                    controller->lcd.turnOff();
                    displaying = false;
                    screen_drawn = false;
                }
            }
        }
        
        // Display current message screens
        if (displaying && screen_count > 0) {
            // Check if force stop is requested before any screen operations
            pthread_mutex_lock(&controller->mutex);
            bool force_stop_check = controller->forceStop;
            pthread_mutex_unlock(&controller->mutex);
            
            if (!force_stop_check) {
                if (!screen_drawn) {
                    controller->lcd.displayScreen(screens[current_screen]);
                    screen_drawn = true;
                }
                
                if (millis() - last_screen_change >= SCROLL_DELAY) {
                    current_screen++;
                    if (current_screen >= screen_count) {
                        // We've shown all screens once
                        if (!current_message_indefinite) {
                            // For timed messages, mark as all screens shown
                            all_screens_shown = true;
                            // Only reset to 0 if we should continue looping (not stopAfterOneLoop)
                            if (!current_message_stop_after_loop) {
                                current_screen = 0;
                            }
                        } else {
                            // For indefinite messages, loop back to screen 0
                            current_screen = 0;
                        }
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

    char mqttMessage[512];
    if (strcmp(topic, "debug") == 0) {
        snprintf(mqttMessage, sizeof(mqttMessage), "DEBUG: %s", doc["message"].as<const char*>());
    } else {
        strncpy(mqttMessage, doc["message"], sizeof(mqttMessage) - 1);
    }
    mqttMessage[sizeof(mqttMessage) - 1] = '\0';

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
    
    // Use the queue system for MQTT messages - create message with MQTT flag set
    QueuedMessage qMsg;
    strncpy(qMsg.message, mqttMessage, sizeof(qMsg.message) - 1);
    qMsg.message[sizeof(qMsg.message) - 1] = '\0';
    
    // Calculate how many screens this message will need
    int screen_count = 0;
    LCDScreen temp_screens[50];
    lcd.formatForLcd(qMsg.message, &screen_count, &temp_screens);
    
    // Ensure duration is long enough to show all screens
    unsigned long minDurationSeconds = (screen_count * SCROLL_DELAY) / 1000 + 1; // +1 for safety
    if (flagUpDurationSeconds < minDurationSeconds) {
        flagUpDurationSeconds = minDurationSeconds;
    }
    
    qMsg.durationSeconds = flagUpDurationSeconds;
    qMsg.isIndefinite = false;
    qMsg.stopAfterOneLoop = false;
    qMsg.isFromMqtt = true; // This is from MQTT, so flag should be raised
    
    pthread_mutex_lock(&queueMutex);
    messageQueue.push(qMsg);
    pthread_mutex_unlock(&queueMutex);
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
            clearDisplay();
        }
    }

    if (resetButtonState == LOW) {
        buttonWasPressed = false;
    }
}

void SplashFlagController::setDisplayMessage(const char* msg) {
    QueuedMessage qMsg;
    strncpy(qMsg.message, msg, sizeof(qMsg.message) - 1);
    qMsg.message[sizeof(qMsg.message) - 1] = '\0';
    qMsg.durationSeconds = 0;
    qMsg.isIndefinite = true;
    qMsg.stopAfterOneLoop = false;
    qMsg.isFromMqtt = false; // Regular display messages are not from MQTT
    
    pthread_mutex_lock(&queueMutex);
    messageQueue.push(qMsg);
    pthread_mutex_unlock(&queueMutex);
}

void SplashFlagController::setDisplayMessageWithDuration(const char* msg, unsigned long durationSeconds) {
    setDisplayMessageWithDuration(msg, durationSeconds, false);
}

void SplashFlagController::setDisplayMessageWithDuration(const char* msg, unsigned long durationSeconds, bool stopAfterOneLoop) {
    QueuedMessage qMsg;
    strncpy(qMsg.message, msg, sizeof(qMsg.message) - 1);
    qMsg.message[sizeof(qMsg.message) - 1] = '\0';
    
    // Calculate how many screens this message will need
    int screen_count = 0;
    LCDScreen temp_screens[50];
    lcd.formatForLcd(qMsg.message, &screen_count, &temp_screens);
    
    // Ensure duration is long enough to show all screens
    // Each screen needs SCROLL_DELAY milliseconds, so total time = screen_count * SCROLL_DELAY
    unsigned long minDurationSeconds = (screen_count * SCROLL_DELAY) / 1000 + 1; // +1 for safety
    if (durationSeconds < minDurationSeconds) {
        durationSeconds = minDurationSeconds;
    }
    
    qMsg.durationSeconds = durationSeconds;
    qMsg.isIndefinite = false;
    qMsg.stopAfterOneLoop = stopAfterOneLoop;
    qMsg.isFromMqtt = false; // Regular display messages are not from MQTT
    
    pthread_mutex_lock(&queueMutex);
    messageQueue.push(qMsg);
    pthread_mutex_unlock(&queueMutex);
}

void SplashFlagController::clearDisplay() {
    pthread_mutex_lock(&queueMutex);
    // Clear the entire queue
    while (!messageQueue.empty()) {
        messageQueue.pop();
    }
    pthread_mutex_unlock(&queueMutex);
    
    // Force stop the current message immediately
    pthread_mutex_lock(&mutex);
    forceStop = true;
    pthread_mutex_unlock(&mutex);
}
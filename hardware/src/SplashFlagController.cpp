#include "SplashFlagController.h"

unsigned long SplashFlagController::resetButtonPressedTime = 0;
bool SplashFlagController::buttonWasPressed = false;

SplashFlagController::SplashFlagController(Lcd& lcd, ServoFlag& servoFlag, CredentialManager& credentialManager, CaptivePortal& portal)
    : lcd(lcd), servoFlag(servoFlag), credentialManager(credentialManager), portal(portal),
      resetButtonState(0), mqttInitialized(false), flagUpSecondsEndTime(0), forceStop(false),
      lastFirmwareCheckTime(0), firmwareUpdateAvailable(false), latestFirmwareVersion(""), firmwareDownloadUrl("") {
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
        //Serial.println("Processing next DNS request in captive portal mode.");
        handleResetButton(); // Handle reset button even when WiFi is disconnected
        return;
    } 

    if (!mqttInitialized) {
        mqtt.disconnect();
        mqtt.begin(client);

        connect();

        mqtt.subscribe("splashflag/all", [this](const String& payload, const size_t size) {
            handleMqttMessage("all", payload, size);
        });
        
        // Only subscribe to debug messages on authorized devices
        if (isDebugDevice()) {
            mqtt.subscribe("splashflag/debug", [this](const String& payload, const size_t size) {
                handleMqttMessage("debug", payload, size);
            });
            Serial.println("Debug subscription enabled for this device");
        }

        mqttInitialized = true;
        Serial.println("MQTT client initialized.");
    }

    if (!mqtt.isConnected()) {
        mqttInitialized = false;
    } else {
        mqtt.update();
    }

    // Check for firmware updates daily
    if (shouldCheckForUpdate()) {
        checkForFirmwareUpdate();
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
    
    // Clear any existing MQTT messages from the queue before adding the new one
    std::queue<QueuedMessage> tempQueue;
    while (!messageQueue.empty()) {
        QueuedMessage existingMsg = messageQueue.front();
        messageQueue.pop();
        // Keep non-MQTT messages in the queue
        if (!existingMsg.isFromMqtt) {
            tempQueue.push(existingMsg);
        }
    }
    
    // Restore non-MQTT messages to the main queue
    while (!tempQueue.empty()) {
        messageQueue.push(tempQueue.front());
        tempQueue.pop();
    }
    
    // Add the new MQTT message
    messageQueue.push(qMsg);
    pthread_mutex_unlock(&queueMutex);
    
    // Force stop current message if it's from MQTT to immediately show the new one
    pthread_mutex_lock(&mutex);
    forceStop = true;
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

bool SplashFlagController::shouldCheckForUpdate() {
    // Check once per day (86400000 milliseconds = 24 hours)
    const unsigned long UPDATE_CHECK_INTERVAL = 60000UL; //86400000UL;
    
    unsigned long currentTime = millis();
    
    // Handle millis() overflow (happens every ~49 days)
    if (currentTime < lastFirmwareCheckTime) {
        lastFirmwareCheckTime = 0;
    }
    
    return (currentTime - lastFirmwareCheckTime) >= UPDATE_CHECK_INTERVAL;
}

void SplashFlagController::checkForFirmwareUpdate() {
    Serial.println("Checking for firmware updates from GitHub...");
    
    HTTPClient http;
    String url = "https://" + String(GITHUB_API_URL) + String(GITHUB_RELEASES_PATH);
    
    Serial.println("Connecting to: " + url);
    
    // Configure SSL for GitHub - use WiFiClientSecure for better SSL handling
    WiFiClientSecure *client = new WiFiClientSecure;
    client->setInsecure(); // Skip SSL certificate verification
    
    http.begin(*client, url);
    http.setTimeout(15000); // 15 second timeout for GitHub API
    http.addHeader("User-Agent", "SplashFlag-Device/1.0"); // GitHub requires User-Agent
    http.addHeader("Authorization", "token " + String(GITHUB_TOKEN)); // Private repo authentication
    http.addHeader("Accept", "application/vnd.github.v3+json"); // GitHub API version
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Follow any redirects
    http.setReuse(false);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("GitHub API response received");
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            String serverTagName = doc["tag_name"].as<String>();
            String serverVersion = serverTagName;
            String currentVersion = String(FIRMWARE_VERSION);
            
            Serial.println("GitHub tag_name: " + serverTagName);
            
            // Remove 'v' prefix if present for version comparison (e.g., "v1.0.1" -> "1.0.1")
            if (serverVersion.startsWith("v")) {
                serverVersion = serverVersion.substring(1);
            }
            
            Serial.println("Current version: " + currentVersion);
            Serial.println("Latest GitHub release: " + serverVersion);
            
            if (serverVersion != currentVersion && serverVersion.length() > 0) {
                firmwareUpdateAvailable = true;
                latestFirmwareVersion = serverVersion;
                
                // Find the firmware binary asset in the release
                JsonArray assets = doc["assets"];
                String downloadUrl = "";
                
                for (JsonVariant asset : assets) {
                    String assetName = asset["name"].as<String>();
                    if (assetName == "firmware.bin") {
                        downloadUrl = asset["browser_download_url"].as<String>();
                        Serial.println("Found firmware asset: " + assetName);
                        Serial.println("Asset download URL: " + downloadUrl);
                        break;
                    }
                }
                
                if (downloadUrl.length() > 0) {
                    Serial.println("Firmware update available: " + serverVersion);
                    setDisplayMessage(("Firmware update available: v" + serverVersion + ". Device will update automatically.").c_str());
                    
                    // Store both version and download URL
                    latestFirmwareVersion = serverVersion;
                    firmwareDownloadUrl = downloadUrl;
                    
                    // Automatically download and install the update
                    if (downloadAndInstallFirmware()) {
                        Serial.println("Firmware update completed. Restarting...");
                        ESP.restart();
                    } else {
                        Serial.println("Firmware update failed.");
                        setDisplayMessageWithDuration("Firmware update failed. Will retry tomorrow.", 10, true);
                    }
                } else {
                    Serial.println("No firmware binary found in GitHub release");
                    setDisplayMessageWithDuration("Firmware update found but no binary available.", 8, true);
                }
            } else {
                Serial.println("Firmware is up to date.");
            }
        } else {
            Serial.println("Failed to parse GitHub API response: " + String(error.c_str()));
        }
    } else {
        Serial.println("GitHub API request failed with HTTP code: " + String(httpCode));
        if (httpCode == 403) {
            Serial.println("GitHub API rate limit may be exceeded");
        } else if (httpCode == -1) {
            Serial.println("Connection failed - check WiFi, DNS, or SSL certificate issues");
            Serial.println("Attempted URL: " + url);
        } else if (httpCode == -5) {
            Serial.println("Connection timeout - check internet connection and GitHub availability");
            Serial.println("Attempted URL: " + url);
        } else if (httpCode == 404) {
            Serial.println("Repository not found - check repo name and token permissions");
        } else if (httpCode == 401) {
            Serial.println("Authentication failed - check GitHub token");
        }
    }
    
    http.end();
    delete client; // Clean up WiFiClientSecure
    lastFirmwareCheckTime = millis();
}

bool SplashFlagController::downloadAndInstallFirmware() {
    if (!firmwareUpdateAvailable || firmwareDownloadUrl.length() == 0) {
        return false;
    }
    
    Serial.println("Starting firmware download from GitHub...");
    Serial.println("Download URL: " + firmwareDownloadUrl);
    setDisplayMessage("Downloading firmware update...");
    
    HTTPClient http;
    String url = firmwareDownloadUrl; // This now contains the GitHub download URL
    
    // Configure SSL for GitHub download
    WiFiClientSecure *client = new WiFiClientSecure;
    client->setInsecure(); // Skip SSL certificate verification
    
    http.begin(*client, url);
    http.setTimeout(60000); // 60 second timeout for GitHub download
    http.addHeader("User-Agent", "SplashFlag-Device/1.0"); // GitHub requires User-Agent
    http.addHeader("Authorization", "token " + String(GITHUB_TOKEN)); // Private repo authentication
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Follow GitHub redirects
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.println("Firmware download failed with HTTP code: " + String(httpCode));
        Serial.println("Failed URL: " + url);
        if (httpCode == 404) {
            Serial.println("Asset not found - check if firmware.bin exists in the release");
        } else if (httpCode == 401) {
            Serial.println("Authentication failed - check GitHub token permissions");
        }
        http.end();
        delete client;
        return false;
    }
    
    int contentLength = http.getSize();
    
    if (contentLength <= 0) {
        Serial.println("Invalid firmware file size");
        http.end();
        return false;
    }
    
    Serial.println("Firmware size: " + String(contentLength) + " bytes");
    
    if (!Update.begin(contentLength)) {
        Serial.println("Not enough space for firmware update");
        http.end();
        return false;
    }
    
    setDisplayMessage("Installing firmware update...");
    
    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buffer[128];
    
    while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        if (available) {
            int bytesToRead = min(available, sizeof(buffer));
            int bytesRead = stream->readBytes(buffer, bytesToRead);
            
            if (Update.write(buffer, bytesRead) != bytesRead) {
                Serial.println("Firmware write failed");
                Update.abort();
                http.end();
                return false;
            }
            
            written += bytesRead;
            
            // Show progress
            int progress = (written * 100) / contentLength;
            if (progress % 10 == 0) {
                Serial.println("Firmware install progress: " + String(progress) + "%");
            }
        }
        delay(1);
    }
    
    if (!Update.end(true)) {
        Serial.println("Firmware update failed: " + String(Update.getError()));
        http.end();
        return false;
    }
    
    if (!Update.isFinished()) {
        Serial.println("Firmware update incomplete");
        http.end();
        return false;
    }
    
    Serial.println("Firmware update completed successfully");
    setDisplayMessage("Firmware update completed. Restarting...");
    
    http.end();
    delete client; // Clean up WiFiClientSecure
    return true;
}

bool SplashFlagController::isDebugDevice() {
    String mac = WiFi.macAddress();
    String macSuffix = mac.substring(mac.length() - 8); // Get last 8 chars (includes colons)
    macSuffix.replace(":", ""); // Remove colons to get last 6 hex chars
    macSuffix.toUpperCase(); // Ensure uppercase for comparison
    
    Serial.println("Device MAC suffix: " + macSuffix);
    
    // Check against authorized debug device MACs
    if (macSuffix == String(DEBUG_DEVICE_MAC_1)) {
        return true;
    }
    
    return false;
}
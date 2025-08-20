#include "SplashFlagController.h"

unsigned long SplashFlagController::_resetButtonPressedTime = 0;
bool SplashFlagController::_buttonWasPressed = false;

SplashFlagController::SplashFlagController(Lcd& lcd, ServoFlag& servoFlag, CredentialManager& credentialManager, CaptivePortal& portal)
    : _lcd(lcd), _servoFlag(servoFlag), _credentialManager(credentialManager), _portal(portal),
      _resetButtonState(0), _mqttInitialized(false), _flagUpSecondsEndTime(0), _forceStop(false),
      _lastFirmwareCheckTime(0), _firmwareUpdateAvailable(false), _latestFirmwareVersion(""), _firmwareDownloadUrl("") {
    _mutex = PTHREAD_MUTEX_INITIALIZER;
    _queueMutex = PTHREAD_MUTEX_INITIALIZER;
    memset(_message, 0, sizeof(_message));
}

SplashFlagController::~SplashFlagController() {
    pthread_mutex_destroy(&_mutex);
    pthread_mutex_destroy(&_queueMutex);
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
        _portal.processNextDNSRequest();
        handleResetButton();
        return;
    } 

    if (!_mqttInitialized) {
        _mqtt.disconnect();
        _mqtt.begin(_client);

        connect();

        _mqtt.subscribe("splashflag/all", [this](const String& payload, const size_t size) {
            handleMqttMessage("all", payload, size);
        });
        
        if (isDebugDevice()) {
            _mqtt.subscribe("splashflag/debug", [this](const String& payload, const size_t size) {
                handleMqttMessage("debug", payload, size);
            });
            Serial.println("Debug subscription enabled for this device");
        }

        _mqttInitialized = true;
        Serial.println("MQTT client initialized.");
    }

    if (!_mqtt.isConnected()) {
        _mqttInitialized = false;
    } else {
        _mqtt.update();
    }

    if (shouldCheckForUpdate()) {
        checkForFirmwareUpdate();
    }

    handleResetButton();
}

bool SplashFlagController::shouldStopDisplaying() {
    pthread_mutex_lock(&_mutex);
    bool stop = (_flagUpSecondsEndTime > 0) && ((millis()/1000) >= _flagUpSecondsEndTime);
    pthread_mutex_unlock(&_mutex);
    return stop;
}

void SplashFlagController::connect() {
connect_to_host:
    Serial.println("connecting to host...");
    _client.disconnect();

    _client.begin(MQTT_BROKER_URL, 80, "/", "mqtt");
    _client.setReconnectInterval(2000);

    Serial.print("connecting to mqtt broker...");
    while (!_mqtt.connect("arduino", MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.print(".");
        setDisplayMessage("Connecting to SplashFlag...");
       
        if (!_client.isConnected()) {
            Serial.println("WebSocketsClient disconnected");
            goto connect_to_host;
        }
    }
    Serial.println(" connected!");
}

void SplashFlagController::factoryReset() {
    Serial.printf("FACTORY RESET");
    setDisplayMessage("RESETTING TO FACTORY SETTINGS");
    _credentialManager.saveCredentials("","");
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
        bool should_stop = false;
        bool has_queued_messages = false;
        
        pthread_mutex_lock(&controller->_queueMutex);
        has_queued_messages = !controller->_messageQueue.empty();
        pthread_mutex_unlock(&controller->_queueMutex);
        
        if (displaying) {
            pthread_mutex_lock(&controller->_mutex);
            bool force_stop_requested = controller->_forceStop;
            pthread_mutex_unlock(&controller->_mutex);
            
            if (force_stop_requested) {
                if (current_message_is_from_mqtt) {
                    should_stop = true;
                }
            } else if (current_message_indefinite) {
                unsigned long elapsed_time = millis() - message_start_time;
                unsigned long min_display_time = screen_count * SCROLL_DELAY + 1000;
                
                should_stop = has_queued_messages && (elapsed_time >= min_display_time);
            } else {
                bool duration_expired = (millis() - message_start_time) >= (current_message_duration * 1000);
                bool should_stop_after_loop = current_message_stop_after_loop && all_screens_shown;
                should_stop = duration_expired || should_stop_after_loop;
            }
        }
        
        bool need_new_message = !displaying || should_stop;
        if (need_new_message) {
            pthread_mutex_lock(&controller->_queueMutex);
            if (!controller->_messageQueue.empty()) {
                QueuedMessage qMsg = controller->_messageQueue.front();
                controller->_messageQueue.pop();
                pthread_mutex_unlock(&controller->_queueMutex);
                
                if (displaying) {
                    controller->_lcd.turnOff();
                    delay(100);
                }
                
                strcpy(local_message, qMsg.message);
                current_message_indefinite = qMsg.isIndefinite;
                current_message_duration = qMsg.durationSeconds;
                current_message_stop_after_loop = qMsg.stopAfterOneLoop;
                current_message_is_from_mqtt = qMsg.isFromMqtt;
                message_start_time = millis();
                
                controller->_lcd.formatForLcd(local_message, &screen_count, &screens);
                current_screen = 0;
                last_screen_change = millis();
                displaying = true;
                screen_drawn = false;
                all_screens_shown = (screen_count <= 1);
                
                pthread_mutex_lock(&controller->_mutex);
                controller->_forceStop = false;
                if (current_message_is_from_mqtt) {
                    controller->_servoFlag.moveTo(90);
                }
                pthread_mutex_unlock(&controller->_mutex);
            } else {
                pthread_mutex_unlock(&controller->_queueMutex);
                if (displaying && should_stop) {
                    pthread_mutex_lock(&controller->_mutex);
                    if (current_message_is_from_mqtt) {
                        controller->_servoFlag.moveTo(0);
                    }
                    controller->_forceStop = false;
                    pthread_mutex_unlock(&controller->_mutex);
                    controller->_lcd.turnOff();
                    displaying = false;
                    screen_drawn = false;
                }
            }
        }
        
        if (displaying && screen_count > 0) {
            pthread_mutex_lock(&controller->_mutex);
            bool force_stop_check = controller->_forceStop;
            pthread_mutex_unlock(&controller->_mutex);
            
            if (!force_stop_check) {
                if (!screen_drawn) {
                    controller->_lcd.displayScreen(screens[current_screen]);
                    screen_drawn = true;
                }
                
                if (millis() - last_screen_change >= SCROLL_DELAY) {
                    current_screen++;
                    if (current_screen >= screen_count) {
                        if (!current_message_indefinite) {
                            all_screens_shown = true;
                            if (!current_message_stop_after_loop) {
                                current_screen = 0;
                            } else {
                                current_screen = screen_count - 1;
                            }
                        } else {
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
    if (size > 480) {
        Serial.printf("MQTT message too long (%zu bytes), maximum is 480. Message ignored.\n", size);
        return;
    }
    
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
    
    QueuedMessage qMsg;
    strncpy(qMsg.message, mqttMessage, sizeof(qMsg.message) - 1);
    qMsg.message[sizeof(qMsg.message) - 1] = '\0';
    
    int screen_count = 0;
    LCDScreen temp_screens[50];
    _lcd.formatForLcd(qMsg.message, &screen_count, &temp_screens);
    
    unsigned long minDurationSeconds = (screen_count * SCROLL_DELAY) / 1000 + 1;
    if (flagUpDurationSeconds < minDurationSeconds) {
        flagUpDurationSeconds = minDurationSeconds;
    }
    
    qMsg.durationSeconds = flagUpDurationSeconds;
    qMsg.isIndefinite = false;
    qMsg.stopAfterOneLoop = false;
    qMsg.isFromMqtt = true;
    
    pthread_mutex_lock(&_queueMutex);
    
    std::queue<QueuedMessage> tempQueue;
    while (!_messageQueue.empty()) {
        QueuedMessage existingMsg = _messageQueue.front();
        _messageQueue.pop();
        if (!existingMsg.isFromMqtt) {
            tempQueue.push(existingMsg);
        }
    }
    
    while (!tempQueue.empty()) {
        _messageQueue.push(tempQueue.front());
        tempQueue.pop();
    }
    
    _messageQueue.push(qMsg);
    pthread_mutex_unlock(&_queueMutex);
    
    pthread_mutex_lock(&_mutex);
    _forceStop = true;
    pthread_mutex_unlock(&_mutex);
}

void SplashFlagController::handleResetButton() {
    _resetButtonState = digitalRead(4);
    
    if (_resetButtonState == HIGH && !_buttonWasPressed) {
        _resetButtonPressedTime = millis();
        _buttonWasPressed = true;
    }

    if (_resetButtonState == HIGH && _buttonWasPressed) {
        unsigned long heldTime = millis() - _resetButtonPressedTime;
        if (heldTime >= 10000) {
            factoryReset();
            pthread_mutex_lock(&_mutex);
            _servoFlag.moveTo(0);
            pthread_mutex_unlock(&_mutex);
            esp_restart();
        } else if (heldTime > 100) {
            clearMqttMessages();
        }
    }

    if (_resetButtonState == LOW) {
        _buttonWasPressed = false;
    }
}

void SplashFlagController::setDisplayMessage(const char* msg) {
    QueuedMessage qMsg;
    strncpy(qMsg.message, msg, sizeof(qMsg.message) - 1);
    qMsg.message[sizeof(qMsg.message) - 1] = '\0';
    qMsg.durationSeconds = 0;
    qMsg.isIndefinite = true;
    qMsg.stopAfterOneLoop = true;
    qMsg.isFromMqtt = false;
    
    pthread_mutex_lock(&_queueMutex);
    _messageQueue.push(qMsg);
    pthread_mutex_unlock(&_queueMutex);
}

void SplashFlagController::setDisplayMessageWithDuration(const char* msg, unsigned long durationSeconds) {
    setDisplayMessageWithDuration(msg, durationSeconds, false);
}

void SplashFlagController::setDisplayMessageWithDuration(const char* msg, unsigned long durationSeconds, bool stopAfterOneLoop) {
    QueuedMessage qMsg;
    strncpy(qMsg.message, msg, sizeof(qMsg.message) - 1);
    qMsg.message[sizeof(qMsg.message) - 1] = '\0';
    
    int screen_count = 0;
    LCDScreen temp_screens[50];
    _lcd.formatForLcd(qMsg.message, &screen_count, &temp_screens);
    
    unsigned long minDurationSeconds = (screen_count * SCROLL_DELAY) / 1000 + 1;
    if (durationSeconds < minDurationSeconds) {
        durationSeconds = minDurationSeconds;
    }
    
    qMsg.durationSeconds = durationSeconds;
    qMsg.isIndefinite = false;
    qMsg.stopAfterOneLoop = stopAfterOneLoop;
    qMsg.isFromMqtt = false;
    
    pthread_mutex_lock(&_queueMutex);
    _messageQueue.push(qMsg);
    pthread_mutex_unlock(&_queueMutex);
}

void SplashFlagController::clearDisplay() {
    pthread_mutex_lock(&_queueMutex);
    while (!_messageQueue.empty()) {
        _messageQueue.pop();
    }
    pthread_mutex_unlock(&_queueMutex);
    
    pthread_mutex_lock(&_mutex);
    _forceStop = true;
    pthread_mutex_unlock(&_mutex);
}

void SplashFlagController::clearMqttMessages() {
    pthread_mutex_lock(&_queueMutex);
    
    std::queue<QueuedMessage> tempQueue;
    while (!_messageQueue.empty()) {
        QueuedMessage existingMsg = _messageQueue.front();
        _messageQueue.pop();
        if (!existingMsg.isFromMqtt) {
            tempQueue.push(existingMsg);
        }
    }
    
    while (!tempQueue.empty()) {
        _messageQueue.push(tempQueue.front());
        tempQueue.pop();
    }
    
    pthread_mutex_unlock(&_queueMutex);
    
    pthread_mutex_lock(&_mutex);
    _forceStop = true;
    pthread_mutex_unlock(&_mutex);
}

bool SplashFlagController::shouldCheckForUpdate() {
    const unsigned long UPDATE_CHECK_INTERVAL = 86400000UL;
    
    unsigned long currentTime = millis();
    
    if (currentTime < _lastFirmwareCheckTime) {
        _lastFirmwareCheckTime = 0;
    }
    
    return (currentTime - _lastFirmwareCheckTime) >= UPDATE_CHECK_INTERVAL;
}

void SplashFlagController::checkForFirmwareUpdate() {
    Serial.println("Checking for firmware updates from GitHub...");
    
    HTTPClient http;
    String url = "https://" + String(GITHUB_API_URL) + String(GITHUB_RELEASES_PATH);
    
    Serial.println("Connecting to: " + url);
    
    WiFiClientSecure *client = new WiFiClientSecure;
    client->setInsecure();
    
    http.begin(*client, url);
    http.setTimeout(15000);
    http.addHeader("User-Agent", "SplashFlag-Device/1.0");
    http.addHeader("Authorization", "token " + String(GITHUB_TOKEN));
    http.addHeader("Accept", "application/vnd.github.v3+json");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
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
            
            if (serverVersion.startsWith("v")) {
                serverVersion = serverVersion.substring(1);
            }
            
            Serial.println("Current version: " + currentVersion);
            Serial.println("Latest GitHub release: " + serverVersion);
            
            if (serverVersion != currentVersion && serverVersion.length() > 0) {
                _firmwareUpdateAvailable = true;
                _latestFirmwareVersion = serverVersion;
                
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
                    
                    _latestFirmwareVersion = serverVersion;
                    _firmwareDownloadUrl = downloadUrl;
                    
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
    delete client;
    _lastFirmwareCheckTime = millis();
}

bool SplashFlagController::downloadAndInstallFirmware() {
    if (!_firmwareUpdateAvailable || _firmwareDownloadUrl.length() == 0) {
        return false;
    }
    
    Serial.println("Starting firmware download from GitHub...");
    Serial.println("Download URL: " + _firmwareDownloadUrl);
    setDisplayMessage("Downloading firmware update...");
    
    HTTPClient http;
    String url = _firmwareDownloadUrl;
    
    WiFiClientSecure *client = new WiFiClientSecure;
    client->setInsecure();
    
    http.begin(*client, url);
    http.setTimeout(60000);
    http.addHeader("User-Agent", "SplashFlag-Device/1.0");
    http.addHeader("Authorization", "token " + String(GITHUB_TOKEN));
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
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
    delete client;
    return true;
}

bool SplashFlagController::isDebugDevice() {
    String mac = WiFi.macAddress();
    String macSuffix = mac.substring(mac.length() - 8);
    macSuffix.replace(":", "");
    macSuffix.toUpperCase();
    
    Serial.println("Device MAC suffix: " + macSuffix);
    
    if (macSuffix == String(DEBUG_DEVICE_MAC_1)) {
        return true;
    }
    
    return false;
}
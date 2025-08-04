#pragma once

#include "secrets.h"
#include "Lcd.h"
#include "ServoFlag.h"
#include "CaptivePortal.h"
#include "CredentialManager.h"
#include "esp_wifi.h"
#include "WiFi.h"
#include <WebSocketsClient.h>
#include <MQTTPubSubClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <pthread.h>
#include <queue>
#include <HTTPClient.h>
#include <Update.h>
#include <time.h>

struct QueuedMessage {
    char message[512];
    unsigned long durationSeconds;
    bool isIndefinite;
    bool stopAfterOneLoop;
    bool isFromMqtt;
};

class SplashFlagController {
private:
    Lcd& lcd;
    ServoFlag& servoFlag;
    CredentialManager& credentialManager;
    CaptivePortal& portal;
    
    int resetButtonState;
    bool mqttInitialized;
    unsigned long flagUpSecondsEndTime;
    char message[512];
    pthread_mutex_t mutex;
    
    std::queue<QueuedMessage> messageQueue;
    pthread_mutex_t queueMutex;
    
    bool forceStop;
    
    // Firmware update variables
    unsigned long lastFirmwareCheckTime;
    bool firmwareUpdateAvailable;
    String latestFirmwareVersion;
    
    static unsigned long resetButtonPressedTime;
    static bool buttonWasPressed;
    
    WebSocketsClient client;
    MQTTPubSub::PubSubClient<512> mqtt;

public:
    SplashFlagController(Lcd& lcd, ServoFlag& servoFlag, CredentialManager& credentialManager, CaptivePortal& portal);
    ~SplashFlagController();
    
    void init();
    void update();
    
    bool shouldStopDisplaying();
    void connect();
    void factoryReset();
    static void* display_thread(void* arg);
    void handleMqttMessage(const char* topic, const String& payload, const size_t size);
    void handleResetButton();
    void setDisplayMessage(const char* msg);
    void setDisplayMessageWithDuration(const char* msg, unsigned long durationSeconds);
    void setDisplayMessageWithDuration(const char* msg, unsigned long durationSeconds, bool stopAfterOneLoop);
    void clearDisplay();
    
    // Firmware update methods
    void checkForFirmwareUpdate();
    bool downloadAndInstallFirmware();
    bool shouldCheckForUpdate();
    
    // Debug helper
    bool isDebugDevice();
    
    bool getMqttInitialized() const { return mqttInitialized; }
    void setMqttInitialized(bool value) { mqttInitialized = value; }
    MQTTPubSub::PubSubClient<512>& getMqtt() { return mqtt; }
};


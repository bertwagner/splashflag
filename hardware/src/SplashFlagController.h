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
#include <WiFiClientSecure.h>
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
        void clearMqttMessages();
        
        void checkForFirmwareUpdate();
        bool downloadAndInstallFirmware();
        bool shouldCheckForUpdate();
        
        bool isDebugDevice();
        
        bool getMqttInitialized() const { return _mqttInitialized; }
        void setMqttInitialized(bool value) { _mqttInitialized = value; }
        MQTTPubSub::PubSubClient<1024>& getMqtt() { return _mqtt; }

    private:
        Lcd& _lcd;
        ServoFlag& _servoFlag;
        CredentialManager& _credentialManager;
        CaptivePortal& _portal;
        
        int _resetButtonState;
        bool _mqttInitialized;
        unsigned long _flagUpSecondsEndTime;
        char _message[512];
        pthread_mutex_t _mutex;
        
        std::queue<QueuedMessage> _messageQueue;
        pthread_mutex_t _queueMutex;
        
        bool _forceStop;
        
        unsigned long _lastFirmwareCheckTime;
        bool _firmwareUpdateAvailable;
        String _latestFirmwareVersion;
        String _firmwareDownloadUrl;
        
        static unsigned long _resetButtonPressedTime;
        static bool _buttonWasPressed;
        
        WebSocketsClient _client;
        MQTTPubSub::PubSubClient<1024> _mqtt;
};
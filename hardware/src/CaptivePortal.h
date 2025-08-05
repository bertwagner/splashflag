#pragma once
#include <Arduino.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include "CredentialManager.h"
#include "Lcd.h"

class CaptivePortal {
    public:
        CaptivePortal(CredentialManager& credentialManager, Lcd& lcd);

        void init();
        void processNextDNSRequest();

    private:
        CredentialManager& _credentialManager;
        Lcd& _lcd;

        void _setUpDNSServer(DNSServer& dnsServer, const IPAddress& localIP);
        void _startSoftAccessPoint(const char* ssid, const char* password, const IPAddress& localIP, const IPAddress& gatewayIP);
        void _setUpWebserver(AsyncWebServer& server, const IPAddress& localIP);
};

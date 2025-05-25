#include <Arduino.h>

// Captive Portal
#include <AsyncTCP.h>  //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>	//https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>			//Used for mpdu_rx_disable android workaround

class CaptivePortal {
    public:
        CaptivePortal();

        void init();
        void processNextDNSRequest();

    private:
        void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP);
        void startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP);
        void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP);
};

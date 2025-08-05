#include "CaptivePortal.h"

const char* ssid = "SplashFlag";
const char* password = NULL;

#define MAX_CLIENTS 4
#define WIFI_CHANNEL 6

const IPAddress localIP(4, 3, 2, 1);
const IPAddress gatewayIP(4, 3, 2, 1);
const IPAddress subnetMask(255, 255, 255, 0);

const String localIPURL = "http://4.3.2.1";

const char index_html[] PROGMEM = R"=====(
  <!DOCTYPE html> <html>
    <head>
      <title>ESP32 Captive Portal</title>
      <style>
        body {background-color:#8cd5ff;}
        h1 {color: white;}
        h2 {color: white;}
      </style>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <h1>SplashFlag WiFi setup</h1>
      <p>This SplashFlag device needs to connect to your home's WiFi so it can receive statuses for when the Wagner's are swimming.</p>

    <form action="/save" method="POST">
        <label for="ssid">WiFi SSID:</label><br>
        <input type="text" id="ssid" name="ssid" required><br><br>
        <label for="password">WiFi Password:</label><br>
        <input type="password" id="password" name="password"><br><br>
        <input type="submit" value="Connect">
    </form>
    </body>
  </html>
)=====";

const char save_html[] PROGMEM = R"=====(
  <!DOCTYPE html> <html>
    <head>
      <title>ESP32 Captive Portal</title>
      <style>
        body {background-color:#8cd5ff;}
        h1 {color: white;}
        h2 {color: white;}
      </style>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <h1>Thanks!</h1>
      <p>Saving your credentials...</p>
      <p>If the SplashFlag successfully connects to your Wifi, the device will restart and display a success message.</p>
      <p>If you are returned to the credentials screen, please try your credentials again.</p>

    </body>
  </html>
)=====";

DNSServer dnsServer;
AsyncWebServer server(80);

CaptivePortal::CaptivePortal(CredentialManager& credentialManager, Lcd& lcd) : _credentialManager(credentialManager), _lcd(lcd)
{
}

void CaptivePortal::init()
{
    Serial.println("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER");
    Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

    _startSoftAccessPoint(ssid, password, localIP, gatewayIP);
    _setUpDNSServer(dnsServer, localIP);
    _setUpWebserver(server, localIP);
    server.begin();

    Serial.print("\n");
    Serial.print("Startup Time:");
    Serial.println(millis());
    Serial.print("\n");
}

void CaptivePortal::_setUpDNSServer(DNSServer& dnsServer, const IPAddress& localIP) {
#define DNS_INTERVAL 30

    dnsServer.setTTL(3600);
    dnsServer.start(53, "*", localIP);
}

void CaptivePortal::_startSoftAccessPoint(const char* ssid, const char* password, const IPAddress& localIP, const IPAddress& gatewayIP) {
#define MAX_CLIENTS 4
#define WIFI_CHANNEL 6

    WiFi.mode(WIFI_MODE_AP);

    const IPAddress subnetMask(255, 255, 255, 0);

    WiFi.softAPConfig(localIP, gatewayIP, subnetMask);
    WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);

    esp_wifi_stop();
    esp_wifi_deinit();
    wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
    my_config.ampdu_rx_enable = false;
    esp_wifi_init(&my_config);
    esp_wifi_start();
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void CaptivePortal::_setUpWebserver(AsyncWebServer& server, const IPAddress& localIP) {
    server.on("/connecttest.txt", [](AsyncWebServerRequest* request) { request->redirect("http://logout.net"); });
    server.on("/wpad.dat", [](AsyncWebServerRequest* request) { request->send(404); });

    server.on("/generate_204", [](AsyncWebServerRequest* request) { request->redirect(localIPURL); });
    server.on("/redirect", [](AsyncWebServerRequest* request) { request->redirect(localIPURL); });
    server.on("/hotspot-detect.html", [](AsyncWebServerRequest* request) { request->redirect(localIPURL); });
    server.on("/canonical.html", [](AsyncWebServerRequest* request) { request->redirect(localIPURL); });
    server.on("/success.txt", [](AsyncWebServerRequest* request) { request->send(200); });
    server.on("/ncsi.txt", [](AsyncWebServerRequest* request) { request->redirect(localIPURL); });

    server.on("/favicon.ico", [](AsyncWebServerRequest* request) { request->send(404); });

    server.on("/", HTTP_ANY, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "text/html", index_html);
        response->addHeader("Cache-Control", "public,max-age=31536000");
        request->send(response);
        Serial.println("Served index page");
    });

    server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
        char input_ssid[50] = {0};
        char input_password[50] = {0};

        if (request->hasParam("ssid", true)) {
            const char* value = request->getParam("ssid", true)->value().c_str();
            strncpy(input_ssid, value, sizeof(input_ssid) - 1);
            input_ssid[sizeof(input_ssid) - 1] = '\0';
            Serial.printf("SSID: %s\n", input_ssid);
        }

        if (request->hasParam("password", true)) {
            const char* value = request->getParam("password", true)->value().c_str();
            strncpy(input_password, value, sizeof(input_password) - 1);
            input_password[sizeof(input_password) - 1] = '\0';
        }

        AsyncWebServerResponse* response = request->beginResponse(200, "text/html", save_html);
        response->addHeader("Refresh", "5; url=/");
        request->send(response);
        Serial.println("Served save wifi page");

        _credentialManager.saveCredentials(input_ssid, input_password);
        _lcd.write("Restarting SplashFlag...");
        delay(2000);
        esp_restart();
    });

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->redirect(localIPURL);
        Serial.print("onnotfound ");
        Serial.print(request->host());
        Serial.print(" ");
        Serial.print(request->url());
        Serial.print(" sent redirect to " + localIPURL + "\n");
    });
}

void CaptivePortal::processNextDNSRequest()
{
    dnsServer.processNextRequest();
    delay(DNS_INTERVAL);
}